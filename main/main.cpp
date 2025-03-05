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
Main appMain;
Console *console = nullptr;
esp_netif_t *ap_netif; // For the DNS

void Main::processCommand(vector<string> args) // Do not move, do not pass by ref, we WANT a copy
{
    if (args.empty())
        return;

    if (args[0] == "update")
        firmwareUpdate("http://192.168.201.50/lightswitch/lightswitch/rom.img");

    if (args[0] == "spiffsupdate")
        appMain.spiffsUpdate("http://192.168.201.50/lightswitch/lightswitch/spiffs.bin");
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

    printf("ssid %s base %s sitemap %s\r\n", m_ssid.c_str(), m_openHabUrl.c_str(), m_sitemap.c_str());

    initLcd();

    m_hostname = generateHostname(TAG);
    wifi.setStationHostname(generateHostname(TAG));
//    wifi.startAP(generateHostname(TAG), "", WIFI_AUTH_OPEN);
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

    if (!m_ssid.empty() && !m_password.empty())
    {
        wifi.connectAP("7_IG6_3DF", "a34c9aa3d18282478a34511ab3", false, WIFI_MODE_APSTA);

        if (!m_openHabUrl.empty() && !m_sitemap.empty())
        {
            setupConnectingScreen();

            openhab.setBaseUrl(m_openHabUrl);
            openhab.setApiKey(m_apiKey); // This must be called before the first request, else it will be ignored
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

void Main::enterApMode()
{
    if (m_apMode)
        return;

    printf("Entering AP mode\r\n");

    m_apMode = true;

    esp_wifi_stop();
    wifi.startAP(generateHostname(TAG), "", WIFI_AUTH_OPEN);

    setupApScreen();

    ap_netif = wifi.getAccessPointIf();
}

void Main::setupApScreen()
{
    uiRoot.reset();

    if (lvgl_port_lock(-1))
    {
        if (m_apRoot != nullptr)
            lv_obj_del_async(m_apRoot);

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

    if (lvgl_port_lock(-1))
    {
        if (m_apRoot != nullptr)
            lv_obj_del_async(m_apRoot);

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

void Main::loadOpenHab()
{
    if (m_testConnection)
        return;

    m_stationConnected = true;

    if (m_openHabUrl.empty() || m_sitemap.empty())
    {
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

    if (!console)
        console = new Console(Console::ConsoleType::TelnetConsole, [this](auto args) {this->processCommand(args);});

    if (m_apRoot != nullptr)
        lv_obj_del_async(m_apRoot);
    m_apRoot = nullptr;

    auto sitemap = m_sitemap;
    openhab.setBaseUrl(m_openHabUrl);
    openhab.disconnectEventChannel();
    openhab.loadSitemapsList([sitemap](vector<Sitemap> s){
        auto it = std::find_if(s.begin(), s.end(),[sitemap](auto e){ return e.name == sitemap;});
        if (it != s.end())
        {
            openhab.loadSitemap((*it).homepage, &factory, [](auto root){
//                        printf("Sitemap load done\r\n");

                uiRoot = root;
                openhab.connectEventChannel();
            }, nullptr);
        }
    });

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

void Main::apStarted()
{
    captdnsInit();
    m_webserver = SimpleWebServer::start_webserver("/data");
}

void Main::apStopped()
{
    SimpleWebServer::stop_webserver(m_webserver);
    captdnsStop();
}

esp_err_t Main::unmountStorage()
{
    esp_vfs_spiffs_unregister("spiffs");
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
    ret = esp_spiffs_info(NULL, &total, &used);
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
    auto ret = wifi.connectAP(ssid, password, true, WIFI_MODE_APSTA);
    printf("Connect done, result %d\r\n", ret);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    m_testConnection = false;

    if (ret)
    {
        esp_wifi_disconnect();
        esp_wifi_set_mode(WIFI_MODE_AP);
        setupApScreen();
        return;
    }
    m_stationConnected = true;

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

void Main::spiffsUpdate(const char *from)
{
    uiRoot.reset();

    setupInfoScreen("Updating", "Downloading update for", "Web Interface"); // Todo

    SimpleWebServer::stop_webserver(m_webserver);
//    unmountStorage();

    ota_spiffs(from);

    lv_obj_del(m_apRoot);

    esp_restart();
}

void Main::firmwareUpdate(const char *from)
{
    uiRoot.reset();

    setupInfoScreen("Updating", "Downloading update for", "Firmware"); // Todo

    SimpleWebServer::stop_webserver(m_webserver);
//    unmountStorage();

    ota.update("http://192.168.201.50/lightswitch/lightswitch/rom.img");

    lv_obj_del(m_apRoot);

    esp_restart();
}
