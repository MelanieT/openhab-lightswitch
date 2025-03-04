//
// Created by Melanie on 23/02/2025.
//

#ifndef LIGHTSWITCH_MAIN_H
#define LIGHTSWITCH_MAIN_H

#include "lvgl.h"
#include "esp_http_server.h"

#if __cplusplus
class OpenHab;
class Factory;
class WiFi;

extern OpenHab openhab;
extern Factory factory;
extern WiFi wifi;

#include <string>
#include <vector>

class Main
{
public:
    Main() = default;

    void run();
    void enterApMode();
    void tryConnectWifi(std::string ssid, std::string password);
    void saveOpenhabData(std::string url, std::string sitemap);
    void saveApiKey(std::string apiKey);
    void loadOpenHab();
    void apStarted();
    void apStopped();
    void spiffsUpdate(const char *from);

    inline std::string hostname() { return m_hostname; };
    inline bool apMode() { return m_apMode; };
    inline bool stationConnected() { return m_stationConnected; };
    inline std::string ssid() { return m_ssid; };
    inline std::string openHabUrl() { return m_openHabUrl; };
    inline std::string sitemap() { return m_sitemap; };

private:
    bool m_apMode = false;
    lv_obj_t *m_apRoot = nullptr;
    lv_obj_t *m_apLabel1;
    lv_obj_t *m_apLabel2;
    std::string m_hostname;
    std::string m_ssid;
    std::string m_password;
    std::string m_openHabUrl;
    std::string m_sitemap;
    std::string m_apiKey;
    httpd_handle_t m_webserver;
    bool m_testConnection = false;
    bool m_stationConnected = false;

    void setupApScreen();
    void setupConnectingScreen();
    std::string generateHostname(const std::string &hostname_base);
    esp_err_t mountStorage(const char *basePath);
    esp_err_t unmountStorage();
    void processCommand(std::vector<std::string> args);
};

extern Main appMain;

#endif
#endif //LIGHTSWITCH_MAIN_H
