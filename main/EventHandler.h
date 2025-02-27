//
// Created by Melanie on 23/02/2025.
//

#ifndef LIGHTSWITCH_EVENTHANDLER_H
#define LIGHTSWITCH_EVENTHANDLER_H

#include "WiFiEventHandler.h"

class EventHandler : public WiFiEventHandler
{
    esp_err_t staGotIp(ip_event_got_ip_t *info) override;
    esp_err_t staDisconnected(wifi_event_sta_disconnected_t *info) override;
    esp_err_t apStart() override;
    esp_err_t apStop() override;
};


#endif //LIGHTSWITCH_EVENTHANDLER_H
