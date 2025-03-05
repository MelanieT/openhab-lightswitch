//
// Created by Melanie on 23/02/2025.
//

#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_phy_init.h>
#include "EventHandler.h"
#include "main.h"
#include "OpenHab.h"
#include "Factory.h"
#include "WiFi.h"

extern shared_ptr<OpenHabObject> uiRoot;

esp_err_t EventHandler::staGotIp(ip_event_got_ip_t *info)
{
    printf("IP event handler\r\n");
    auto err = WiFiEventHandler::staGotIp(info);
    if (err == ESP_OK && info->esp_netif == wifi.getStationIf())
    {
        char ipAddrStr[30];
        inet_ntop(AF_INET, &info->ip_info, ipAddrStr, sizeof(ipAddrStr));
        printf("IP for station netif %s, changed %d\r\n", ipAddrStr, info->ip_changed);

        appMain.loadOpenHab();
    }
    return err;
}

esp_err_t EventHandler::staDisconnected(wifi_event_sta_disconnected_t *info)
{
    static int64_t last_disconnect = 0;
    static int disconnect_count = 0;

    if (last_disconnect == 0)
    {
        last_disconnect = esp_timer_get_time();
    }
    else
    {
        if (esp_timer_get_time() - last_disconnect > 3600 * 60 * 1000000)
        {
            printf("Restarting count\r\n");
            last_disconnect = 0;
            disconnect_count = 0;
        }
    }

    disconnect_count++;
    if (disconnect_count < 5)
    {
        esp_phy_erase_cal_data_in_nvs();
        esp_wifi_connect();
    }
    else
    {
        appMain.enterApMode();
    }

    return WiFiEventHandler::staDisconnected(info);
}

esp_err_t EventHandler::apStart()
{
    appMain.apStarted();
    return WiFiEventHandler::apStart();
}

esp_err_t EventHandler::apStop()
{
    appMain.apStopped();
    return WiFiEventHandler::apStop();
}
