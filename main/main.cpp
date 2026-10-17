/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_spiffs.h>

#include <utility>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_port.h"
#include "OpenHab.h"
#include "Factory.h"
#include "IconLoader.h"
#include "back.h"
#include "lcd.h"
#include "CppConsole.h"
#include "WiFi.h"
#include "ota.h"
#include "main.h"
#include "CPPNVS.h"
#include "EventHandler.h"
#include "SimpleWebServer.h"
#include "SetupApi.h"

extern "C" {
#include "captdns.h"
#include "spiffs-ota.h"
}

shared_ptr<OpenHabObject> uiRoot;

static const char *TAG = "lightswitch";

OpenHab openhab;
Factory factory;
Ota ota;
WiFi wifi;
MDNS *mdns;
Main appMain;
Console console;
static bool consoleStarted = false;

// console_printf needs os_sprintf, which base_console keeps to itself
static void consolePrintf(const char *format, ...)
{
    char data[256];
    va_list args;
    va_start(args, format);
    vsnprintf(data, sizeof(data), format, args);
    va_end(args);
    console_send(data);
}

esp_netif_t *ap_netif; // For the DNS

void Main::processCommand(vector<string> args) // Do not move, do not pass by ref, we WANT a copy
{
    if (args.empty())
        return;

    if (args[0] == "update")
    {
        SimpleWebServer::stop_webserver(m_webserver);
        openhab.disconnectEventChannel();

        spiffsUpdate();
        firmwareUpdate();
    }

    if (args[0] == "spiffsupdate")
    {
        spiffsUpdate();
        esp_restart();
    }

    if (args[0] == "reboot")
    {
        esp_restart();
    }
}

extern "C" {
void app_main()
{
    appMain.run();
}
}

void Main::run()
{
    NVS nvs("ls");

    mountStorage("/data");

    SetupApi::init();

    if (nvs.get("ssid", &m_ssid))
    {
        m_ssid.clear();
    }
    if (nvs.get("password", &m_password))
    {
        m_password.clear();
    }
    if (nvs.get("base", &m_openHabUrl))
    {
        m_openHabUrl.clear();
    }
    if (nvs.get("sitemap", &m_sitemap))
    {
        m_sitemap.clear();
    }
    if (nvs.get("apiKey", &m_apiKey))
    {
        m_apiKey.clear();
    }

    printf("ssid %s base %s sitemap %s api key %s\r\n", m_ssid.c_str(), m_openHabUrl.c_str(), m_sitemap.c_str(), m_apiKey.c_str());

    initLcd();

    m_hostname = generateHostname(TAG);
    wifi.setStationHostname(m_hostname.c_str());
    // wifi.startAP(generateHostname(TAG), "", WIFI_AUTH_OPEN);
    wifi.setWifiEventHandler(new EventHandler());

    if (lvgl_port_lock(-1))
    {
        lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(lv_scr_act(), LV_OBJ_FLAG_PRESS_LOCK);

        lv_disp_t *dispp = lv_disp_get_default();
        lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE),
                                                  lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
        lv_disp_set_theme(dispp, theme);
        lv_obj_set_style_bg_color(lv_scr_act(), LV_COLOR_MAKE(0, 0, 0), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_100, 0);

        lvgl_port_unlock();
    }

    setupHwScreen();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    closeHwScreen();

    if (m_updateMode && (m_ssid.empty() || m_password.empty()))
    {
        m_updateMode = false;
        setupInfoScreen("Update", "Not possible,", "no network configured");
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        closeInfoScreen();
    }

    if (!m_ssid.empty() && !m_password.empty() && !m_enterAPMode)
    {
        wifi.connectSTA(m_ssid, m_password, false);
        m_stationStarted = true;
        esp_wifi_set_ps(WIFI_PS_NONE); // Mains powered, modem sleep only adds latency

        if (m_updateMode)
            setupInfoScreen("Update", "Connecting to", m_ssid.c_str());

        mdns = new MDNS();
        mdns->setHostname(m_hostname.c_str());
        mdns->setInstance("Lightswitch");
        mdns->serviceAdd(nullptr, "_http", "_tcp", 80);

        if (!m_openHabUrl.empty() && !m_sitemap.empty())
        {
            if (!m_updateMode)
                setupConnectingScreen();

            openhab.setBaseUrl(m_openHabUrl);
            openhab.setApiKey(m_apiKey); // This must be called before the first request, else it will be ignored
            ESP_LOGI(TAG, "Open hab url: %s", m_openHabUrl.c_str());
            ESP_LOGI(TAG, "Open hab token: %s", m_apiKey.c_str());

            string url = m_openHabUrl;
            if (m_openHabUrl.starts_with("http://"))
                url = m_openHabUrl.substr(7);
            else if (m_openHabUrl.starts_with("https://"))
                url = m_openHabUrl.substr(8);
            int found = url.find_first_of(":/");
            if (found != string::npos)
            {
                url = url.substr(0, found);
            }
            IconLoader::init("http://" + url + ":5050/?image=");
            IconLoader::addIconFromData("back", (uint8_t *) back, 64, 64);
        }
    }
    else
    {
        enterApMode();
    }

    while (true)
    {
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

void Main::enterApMode(bool retryStation)
{
    if (m_apMode)
        return;

    printf("Entering AP mode\r\n");

    m_apMode = true;

    // if (m_webserver)
    //     SimpleWebServer::stop_webserver(m_webserver);
    // m_webserver = SimpleWebServer::start_webserver("/data");

    // With the station running, wifi.stop() + startAP() re-ran the wifi init, which
    // creates the default netifs a second time and asserts. Just drop the station.
    if (m_stationStarted)
        wifi.disconnectSTA();
    else
        wifi.stop();
    wifi.startAP(generateHostname(TAG), "", WIFI_AUTH_OPEN);

    setupApScreen();

    ap_netif = wifi.getAccessPointIf();

    // Entered because the network wasn't found, e.g. the router was still booting
    // after a power cut. Keep looking for it, or the switch stays offline for good.
    if (retryStation)
        xTaskCreate(Main::apRetryTask, "ap_retry", 4096, this, 1, nullptr);
}

void Main::setupApScreen()
{
    uiRoot.reset();

    if (lvgl_port_lock(-1))
    {
        if (m_apRoot != nullptr)
            lv_obj_del_async(m_apRoot);
        m_progressBar = nullptr;

        m_apRoot = lv_obj_create(lv_scr_act());
        lv_obj_set_size(m_apRoot, LVGL_PORT_H_RES, LVGL_PORT_V_RES);
        lv_obj_center(m_apRoot);

        lv_obj_t *welcome = lv_obj_create(m_apRoot);
        lv_obj_set_style_bg_color(welcome, lv_color_make(0xe6, 0x7e, 0x22), 0);
        lv_obj_set_size(welcome, lv_pct(75), lv_pct(50));
        lv_obj_center(welcome);

        lv_obj_t *welcomeLabel = lv_label_create(welcome);
        lv_obj_set_style_text_color(welcomeLabel, lv_color_make(255, 255, 255), 0);
        lv_label_set_text(welcomeLabel, "Welcome!");
        lv_obj_set_style_text_font(welcomeLabel, &lv_font_montserrat_34, 0);
        lv_obj_align(welcomeLabel, LV_ALIGN_TOP_MID, 0, 40);

        m_apLabel1 = lv_label_create(welcome);
        lv_label_set_text(m_apLabel1, "Connect Wifi to");
        lv_obj_set_style_text_font(m_apLabel1, &lv_font_montserrat_18, 0);
        lv_obj_align(m_apLabel1, LV_ALIGN_TOP_MID, 0, 110);

        m_apLabel2 = lv_label_create(welcome);
        lv_label_set_text(m_apLabel2, m_hostname.c_str());
        lv_obj_set_style_text_font(m_apLabel2, &lv_font_montserrat_24, 0);
        lv_obj_align(m_apLabel2, LV_ALIGN_TOP_MID, 0, 140);

        // Release the mutex
        lvgl_port_unlock();
    }
}

void Main::setupInfoScreen(const char *title, const char *action, const char *data)
{
    uiRoot.reset();

    closeInfoScreen();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if (lvgl_port_lock(-1))
    {
        m_apRoot = lv_obj_create(lv_scr_act());
        lv_obj_set_size(m_apRoot, LVGL_PORT_H_RES, LVGL_PORT_V_RES);
        lv_obj_center(m_apRoot);

        lv_obj_t *welcome = lv_obj_create(m_apRoot);
        lv_obj_set_style_bg_color(welcome, lv_color_make(0xe6, 0x7e, 0x22), 0);
        lv_obj_set_size(welcome, lv_pct(75), lv_pct(50));
        lv_obj_center(welcome);

        lv_obj_t *welcomeLabel = lv_label_create(welcome);
        lv_obj_set_style_text_color(welcomeLabel, lv_color_make(255, 255, 255), 0);
        lv_label_set_text(welcomeLabel, title);
        lv_obj_set_style_text_font(welcomeLabel, &lv_font_montserrat_34, 0);
        lv_obj_align(welcomeLabel, LV_ALIGN_TOP_MID, 0, 40);

        m_apLabel1 = lv_label_create(welcome);
        lv_label_set_text(m_apLabel1, action);
        lv_obj_set_style_text_font(m_apLabel1, &lv_font_montserrat_18, 0);
        lv_obj_align(m_apLabel1, LV_ALIGN_TOP_MID, 0, 110);

        m_apLabel2 = lv_label_create(welcome);
        lv_label_set_text(m_apLabel2, data);
        lv_obj_set_style_text_font(m_apLabel2, &lv_font_montserrat_24, 0);
        lv_obj_align(m_apLabel2, LV_ALIGN_TOP_MID, 0, 140);

        // Release the mutex
        lvgl_port_unlock();
    }
}

void Main::closeInfoScreen()
{
    if (lvgl_port_lock(-1))
    {
        if (m_apRoot != nullptr)
            lv_obj_del_async(m_apRoot);
        m_apRoot = nullptr;
        m_progressBar = nullptr;

        lvgl_port_unlock();
    }
}

void Main::loadOpenHab(const std::string &ip)
{
    if (m_testConnection)
        return;

    m_stationConnected = true;

    vTaskDelay(1000 / portTICK_PERIOD_MS); // Let wifi stabilize

    if (m_updateMode)
    {
        m_updateMode = false;
        // Own task, not this event handler: the download posts http client events
        // to the default event loop, which is the task running this
        xTaskCreate(Main::updateTask, "update", 8192, this, 5, nullptr);
        return;
    }

    if (m_openHabUrl.empty() || m_sitemap.empty())
    {
        // No AP here: setup continues over the network at the IP shown below.
        // The AP is only for AP mode (chosen at boot, or no network found).
        m_webserver = SimpleWebServer::start_webserver("/data");

        setupApScreen();
        if (lvgl_port_lock(-1))
        {
            lv_label_set_text(m_apLabel1, "Connect to wifi to setup at");
            lv_label_set_text(m_apLabel2, wifi.getStaIp().c_str());

            lvgl_port_unlock();
        }

        return;
    }

    if (m_webserver)
        SimpleWebServer::stop_webserver(m_webserver);
    m_webserver = SimpleWebServer::start_webserver("/data");

    if (!consoleStarted)
    {
        console.init(Console::ConsoleType::TelnetConsole, [this](auto args) {this->processCommand(args);},
                     [] { consolePrintf("%s %s\r\n", TAG, VERSION); });
        consoleStarted = true;
    }

    if (m_setupMode)
    {
        setupInfoScreen("Setup mode", "Connect to", ip.c_str());
        return;
    }

    closeInfoScreen();

    if (m_loadRetryRunning)
        return; // The background retry does the loading

    openhab.setBaseUrl(m_openHabUrl);
    openhab.disconnectEventChannel();

    if (!loadSitemapUi())
    {
        // openHAB not reachable yet, e.g. still booting after a power cut. Without a
        // retry the switch stayed on "Connecting" with no updates until rebooted.
        printf("Loading sitemap %s failed, retrying in the background\r\n", m_sitemap.c_str());
        m_loadRetryRunning = true;
        // 16k like the system event task this normally runs on, the sitemap parse is deep
        if (xTaskCreate(Main::loadRetryTask, "load_retry", 16384, this, 1, nullptr) != pdPASS)
            m_loadRetryRunning = false;
    }
}

bool Main::loadSitemapUi()
{
    bool loaded = false;
    auto sitemap = m_sitemap;
    openhab.loadSitemapsList([sitemap, &loaded](vector<Sitemap> s){
        auto it = std::find_if(s.begin(), s.end(),[sitemap](auto e){ return e.name == sitemap;});
        if (it != s.end())
        {
            openhab.loadSitemap((*it).homepage, &factory, [&loaded](auto root){
//                        printf("Sitemap load done\r\n");

                if (!root)
                    return;
                loaded = true;

                uiRoot = root;
                openhab.connectEventChannel();
            }, nullptr);
        }
    });
    return loaded;
}

#define LOAD_RETRY_INTERVAL_MS (10 * 1000)

void Main::loadRetryTask(void *arg)
{
    auto main = static_cast<Main *>(arg);

    while (!main->m_apMode)
    {
        vTaskDelay(LOAD_RETRY_INTERVAL_MS / portTICK_PERIOD_MS);

        if (main->m_apMode)
            break;
        if (main->loadSitemapUi())
        {
            printf("Sitemap %s loaded\r\n", main->m_sitemap.c_str());
            break;
        }
    }

    main->m_loadRetryRunning = false;
    vTaskDelete(nullptr);
}

string Main::generateHostname(const string& hostname_base)
{
    uint8_t chipid[6];
    esp_read_mac(chipid, ESP_MAC_WIFI_STA);
    static char hostname[32];
    snprintf(hostname, 32, "%s_%02x%02x%02x", hostname_base.c_str(), chipid[3], chipid[4], chipid[5]);

    return {hostname};
}

void Main::setupConnectingScreen()
{
    if (lvgl_port_lock(-1))
    {
        if (m_apRoot)
            lv_obj_del_async(m_apRoot);
        m_progressBar = nullptr;

        m_apRoot = lv_obj_create(lv_scr_act());
        lv_obj_set_size(m_apRoot, LVGL_PORT_H_RES, LVGL_PORT_V_RES);
        lv_obj_center(m_apRoot);

        lv_obj_t *welcome = lv_obj_create(m_apRoot);
        lv_obj_set_style_bg_color(welcome, lv_color_make(0xe6, 0x7e, 0x22), 0);
        lv_obj_set_size(welcome, lv_pct(75), lv_pct(35));
        lv_obj_center(welcome);

        lv_obj_t *welcomeLabel = lv_label_create(welcome);
        lv_obj_set_style_text_color(welcomeLabel, lv_color_make(255, 255, 255), 0);
        lv_label_set_text(welcomeLabel, "Connecting");
        lv_obj_set_style_text_font(welcomeLabel, &lv_font_montserrat_34, 0);
        lv_obj_center(welcomeLabel);

        // Release the mutex
        lvgl_port_unlock();
    }
}

void Main::setupHwScreen()
{
    if (lvgl_port_lock(-1))
    {
        m_hwScreen = lv_obj_create(lv_scr_act());
        lv_obj_set_size(m_hwScreen, LVGL_PORT_H_RES, LVGL_PORT_V_RES);
        lv_obj_center(m_hwScreen);

        lv_obj_t *versionLabel = lv_label_create(m_hwScreen);
        lv_obj_set_style_text_color(versionLabel, lv_color_make(255, 255, 255), 0);
        lv_label_set_text(versionLabel, (string("Version: ") + VERSION).c_str());
        lv_obj_set_style_text_font(versionLabel, &lv_font_montserrat_20, 0);
        lv_obj_set_pos(versionLabel, 0, 0);
        
        lv_obj_t *nameLabel = lv_label_create(m_hwScreen);
        lv_obj_set_style_text_color(nameLabel, lv_color_make(255, 255, 255), 0);
        lv_label_set_text(nameLabel, ("Hostname: " + m_hostname).c_str());
        lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_20, 0);
        lv_obj_set_pos(nameLabel, 0, 24);

        lv_obj_t *apButton = lv_btn_create(m_hwScreen);
        lv_obj_set_pos(apButton, 0, 150);
        lv_obj_set_size(apButton, 140, 55);

        lv_obj_t *apButtonLabel = lv_label_create(apButton);
        lv_label_set_text(apButtonLabel, "Start AP");
        lv_obj_set_style_text_color(apButtonLabel, lv_color_make(255, 255, 255), 0);
        lv_obj_set_style_text_font(apButtonLabel, &lv_font_montserrat_24, 0);
        lv_obj_center(apButtonLabel);
        lv_obj_add_event_cb(apButton, [](lv_event_t *e) {
            auto main = static_cast<Main *>(e->user_data);
            main->m_enterAPMode = true;
        }, LV_EVENT_CLICKED, this);

        lv_obj_t *keepApButton = lv_btn_create(m_hwScreen);
        lv_obj_set_pos(keepApButton, 160, 150);
        lv_obj_set_size(keepApButton, 140, 55);

        lv_obj_t *keepApButtonLabel = lv_label_create(keepApButton);
        lv_label_set_text(keepApButtonLabel, "Setup");
        lv_obj_set_style_text_color(keepApButtonLabel, lv_color_make(255, 255, 255), 0);
        lv_obj_set_style_text_font(keepApButtonLabel, &lv_font_montserrat_24, 0);
        lv_obj_center(keepApButtonLabel);
        lv_obj_add_event_cb(keepApButton, [](lv_event_t *e) {
            auto main = static_cast<Main *>(e->user_data);
            main->m_setupMode = true;
        }, LV_EVENT_CLICKED, this);

        lv_obj_t *updateButton = lv_btn_create(m_hwScreen);
        lv_obj_set_pos(updateButton, 0, 220);
        lv_obj_set_size(updateButton, 140, 55);

        lv_obj_t *updateButtonLabel = lv_label_create(updateButton);
        lv_label_set_text(updateButtonLabel, "Update");
        lv_obj_set_style_text_color(updateButtonLabel, lv_color_make(255, 255, 255), 0);
        lv_obj_set_style_text_font(updateButtonLabel, &lv_font_montserrat_24, 0);
        lv_obj_center(updateButtonLabel);
        lv_obj_add_event_cb(updateButton, [](lv_event_t *e) {
            auto main = static_cast<Main *>(e->user_data);
            main->m_updateMode = true;
        }, LV_EVENT_CLICKED, this);
        
        lvgl_port_unlock();
    }
}

void Main::closeHwScreen()
{
    if (lvgl_port_lock(-1))
    {
        lv_obj_del(m_hwScreen);

        lvgl_port_unlock();
    }
}

void Main::apStarted()
{
    captdnsStop();
    captdnsInit();
    if (m_webserver)
    {
        SimpleWebServer::stop_webserver(m_webserver);
    }
    m_webserver = SimpleWebServer::start_webserver("/data");

}

void Main::apStopped()
{
    // Connected to the network, the web server is still needed. When stopping it,
    // forget the handle, or a later stop would use the freed one.
    if (!m_stationConnected && m_webserver)
    {
        SimpleWebServer::stop_webserver(m_webserver);
        m_webserver = nullptr;
    }
    captdnsStop();
}

esp_err_t Main::unmountStorage()
{
    return esp_vfs_spiffs_unregister("spiffs");
}

esp_err_t Main::mountStorage(const char *basePath)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = basePath,
        .partition_label = "spiffs",
        .max_files = 5,   // This sets the maximum number of files that can be open at the same time
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("spiffs", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}

void Main::tryConnectWifi(std::string ssid, std::string password)
{
//    esp_wifi_disconnect();
//    esp_wifi_stop();

    if (m_testConnection)
        return;

    m_testConnection = true;

    setupConnectingScreen();
    printf("Attempting to connect\r\n");
    // Keeps the AP running while connecting, the setup page is using it
    auto ret = wifi.connectSTA(ssid, password, true);
    printf("Connect done, result %d\r\n", ret);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    m_testConnection = false;

    if (ret)
    {
        wifi.disconnectSTA(); // Back to AP only
        setupApScreen();
        return;
    }
    m_stationConnected = true;

    // Setup continues over the network. Only now, so apStopped() keeps the web server.
    wifi.stopAP();

    NVS nvs("ls");

    nvs.set("ssid", ssid);
    nvs.set("password", password);
    nvs.set("base", "");
    nvs.set("sitemap", "");

    nvs.commit();

    setupApScreen();
    if (lvgl_port_lock(-1))
    {
        lv_label_set_text(m_apLabel1, "Connect to wifi to setup at");
        lv_label_set_text(m_apLabel2, wifi.getStaIp().c_str());

        lvgl_port_unlock();
    }
}

void Main::saveOpenhabData(std::string url, std::string sitemap)
{
    NVS nvs("ls");

    nvs.set("base", std::move(url));
    nvs.set("sitemap", std::move(sitemap));

    nvs.commit();

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    esp_restart();
}

void Main::saveApiKey(std::string apiKey)
{
    NVS nvs("ls");

    nvs.set("apiKey", std::move(apiKey));

    nvs.commit();
}

void Main::update()
{
    SimpleWebServer::stop_webserver(m_webserver);
    openhab.disconnectEventChannel();

    spiffsUpdate();
    firmwareUpdate();
}

#define AP_RETRY_INTERVAL_MS (60 * 1000)

void Main::apRetryTask(void *arg)
{
    auto main = static_cast<Main *>(arg);

    while (main->m_apMode)
    {
        vTaskDelay(AP_RETRY_INTERVAL_MS / portTICK_PERIOD_MS);

        // Leave anyone using the setup page alone, a scan takes the AP off channel
        wifi_sta_list_t clients;
        if (main->m_testConnection || (esp_wifi_ap_get_sta_list(&clients) == ESP_OK && clients.num > 0))
            continue;

        for (auto &ap : wifi.scan())
        {
            if (ap.getSSID() == main->m_ssid)
            {
                printf("Network %s is back, restarting\r\n", main->m_ssid.c_str());
                esp_restart(); // A clean normal boot
            }
        }
    }

    vTaskDelete(nullptr);
}

void Main::updateTask(void *arg)
{
    static_cast<Main *>(arg)->update();
    vTaskDelete(nullptr);
}

// Update progress on the telnet console, in steps of 10%
static void printProgress(const char *what, int done, int total)
{
    static const char *lastWhat = nullptr;
    static int lastStep = -1;

    if (!consoleStarted)
        return; // Nothing to print to before the console exists

    if (what != lastWhat)
    {
        lastWhat = what;
        lastStep = -1;
    }

    int step = total > 0 ? done * 10 / total : done / (256 * 1024);
    if (step == lastStep)
        return;
    lastStep = step;

    if (total > 0)
        consolePrintf("%s: %d%% (%d of %d bytes)\r\n", what, step * 10, done, total);
    else
        consolePrintf("%s: %d bytes\r\n", what, done);
}

static void printSpiffsProgress(int done, int total)
{
    printProgress("Web interface", done, total);
    appMain.showProgress(done, total);
}

// A slim bar along the bottom edge of the box on screen, edge to edge, no label
void Main::addProgressBar()
{
    if (lvgl_port_lock(-1))
    {
        lv_obj_t *box = m_apRoot ? lv_obj_get_child(m_apRoot, 0) : nullptr;
        if (box && !m_progressBar)
        {
            lv_obj_update_layout(box);
            // The bar sits in the padding at the bottom: don't let that scroll, keep it in the rounded corners
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_clip_corner(box, true, 0);

            lv_coord_t border = lv_obj_get_style_border_width(box, LV_PART_MAIN);
            m_progressBar = lv_bar_create(box);
            lv_obj_set_size(m_progressBar, lv_obj_get_width(box) - 2 * border, 4);
            lv_obj_align(m_progressBar, LV_ALIGN_BOTTOM_LEFT, -lv_obj_get_style_pad_left(box, LV_PART_MAIN),
                         lv_obj_get_style_pad_bottom(box, LV_PART_MAIN));
            lv_obj_set_style_radius(m_progressBar, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(m_progressBar, 0, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(m_progressBar, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(m_progressBar, LV_OPA_30, LV_PART_MAIN);
            lv_obj_set_style_bg_color(m_progressBar, lv_color_white(), LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(m_progressBar, LV_OPA_COVER, LV_PART_INDICATOR);
            lv_bar_set_range(m_progressBar, 0, 100);
        }
        if (m_progressBar)
            lv_bar_set_value(m_progressBar, 0, LV_ANIM_OFF);
        m_progressPercent = 0;

        lvgl_port_unlock();
    }
}

// From the update task: only take the UI lock when the bar actually moves
void Main::showProgress(int done, int total)
{
    if (total <= 0)
        return;

    int percent = (int) ((int64_t) done * 100 / total);
    if (percent == m_progressPercent)
        return;
    m_progressPercent = percent;

    if (lvgl_port_lock(-1))
    {
        if (m_progressBar)
            lv_bar_set_value(m_progressBar, percent, LV_ANIM_OFF);

        lvgl_port_unlock();
    }
}

void Main::spiffsUpdate()
{
    setupInfoScreen("Updating", "Downloading update for", "Web Interface"); // Todo
    addProgressBar();

    ota_spiffs_set_progress_callback(printSpiffsProgress);
    ota_spiffs("spiffs"); // From CONFIG_SPIFFS_OTA_URI
}

void Main::firmwareUpdate()
{
    if (m_apRoot == nullptr)
    {
        setupInfoScreen("Updating", "Downloading update for", "Firmware");
    }
    else
    {
        if (lvgl_port_lock(-1))
        {
            lv_label_set_text(m_apLabel2, "Firmware");

            lvgl_port_unlock();
        }
    }

    addProgressBar(); // Back to empty for the firmware

    vTaskDelay(100 / portTICK_PERIOD_MS);

    ota.setProgressCallback([](int done, int total) {
        printProgress("Firmware", done, total);
        appMain.showProgress(done, total);
    });
    ota.update(); // From CONFIG_OTA_SERVER_URI

    setupInfoScreen("Error", "Update has failed", "");

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    closeInfoScreen();

    vTaskDelay(500 / portTICK_PERIOD_MS);

    esp_restart();
}
