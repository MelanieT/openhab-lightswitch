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
#include "SetupApi.h"
#include "AppFramework.h"

extern "C" {
#include "spiffs-ota.h"
}

shared_ptr<OpenHabObject> uiRoot;

static const char *TAG = "lightswitch";

OpenHab openhab;
Factory factory;
Ota ota;
WiFi wifi;
AppFramework framework(&wifi);
Main appMain;
Console console;
static bool consoleStarted = false;

void Main::processCommand(vector<string> args) // Do not move, do not pass by ref, we WANT a copy
{
    if (args.empty())
        return;

    if (args[0] == "update")
    {
        framework.stopWebServer();
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

    SetupApi::init();

    // The framework keeps the wifi credentials in "wifi". Up to 2.4 they were in "ls": move them
    // over once, and keep them there too, an older version still needs them.
    NVS wifiNvs("wifi");
    if (wifiNvs.get("ssid", &m_ssid))
    {
        if (!nvs.get("ssid", &m_ssid) && !nvs.get("password", &m_password))
        {
            wifiNvs.set("ssid", m_ssid);
            wifiNvs.set("password", m_password);
            wifiNvs.commit();
            printf("Wifi credentials moved to the framework\r\n");
        }
        else
        {
            m_ssid.clear();
            m_password.clear();
        }
    }
    else if (wifiNvs.get("password", &m_password))
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

    m_hostname = framework.hostname();
    framework.setHandler(this);
    framework.setWebAppIsSpa(true);
    framework.setMdnsInstance("Lightswitch");
    // For the openHAB setup page
    framework.setDeviceDataHandler([this](auto &reply) {
        reply["base"] = m_openHabUrl;
        reply["sitemap"] = m_sitemap;
    });

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

    // Joins the network, or opens the setup access point: without credentials, after
    // failing to join, or asked for on the boot screen
    if (m_enterAPMode)
        framework.startSetup();
    framework.init();
    esp_wifi_set_ps(WIFI_PS_NONE); // Mains powered, modem sleep only adds latency

    if (!m_ssid.empty() && !m_password.empty() && !m_enterAPMode)
    {
        if (m_updateMode)
            setupInfoScreen("Update", "Connecting to", m_ssid.c_str());

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

    while (true)
    {
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
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
        lv_label_set_text(m_apLabel2, m_hostname.c_str()); // The access point
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
        setupApScreen();
        if (lvgl_port_lock(-1))
        {
            lv_label_set_text(m_apLabel1, "Connect to wifi to setup at");
            lv_label_set_text(m_apLabel2, ip.c_str());

            lvgl_port_unlock();
        }

        return;
    }

    if (!consoleStarted)
    {
        console.init(Console::ConsoleType::TelnetConsole, [this](auto args) {this->processCommand(args);},
                     [] { console_printf("%s %s\r\n", TAG, VERSION); });
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

// Framework events, from the wifi event task like the old handler

// The setup access point is up: show how to reach it
void Main::apActive()
{
    m_apMode = true;
    setupApScreen();
}

void Main::apStopped()
{
    m_apMode = false;
}

void Main::staActive()
{
    loadOpenHab(framework.ipAddress());
}

// From the web server task, while the setup page tests new credentials
void Main::setupConnectStarted()
{
    setupConnectingScreen();
}

void Main::setupConnectFinished(bool connected)
{
    if (!connected)
    {
        setupApScreen();
        return;
    }

    // A new network: openHAB is set up again, over the network at the address shown
    NVS nvs("ls");
    nvs.set("base", "");
    nvs.set("sitemap", "");
    nvs.commit();
    m_openHabUrl.clear();
    m_sitemap.clear();

    setupApScreen();
    if (lvgl_port_lock(-1))
    {
        lv_label_set_text(m_apLabel1, "Connect to wifi to setup at");
        lv_label_set_text(m_apLabel2, framework.ipAddress().c_str());

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
    framework.stopWebServer();
    openhab.disconnectEventChannel();

    spiffsUpdate();
    firmwareUpdate();
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
        console_printf("%s: %d%% (%d of %d bytes)\r\n", what, step * 10, done, total);
    else
        console_printf("%s: %d bytes\r\n", what, done);
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
