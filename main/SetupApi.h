//
// Created by Melanie on 20/02/2025.
//

#ifndef LIGHTSWITCH_FACTORY_SETUPAPI_H
#define LIGHTSWITCH_FACTORY_SETUPAPI_H


#include "ApiHttpResponse.h"
#include "ApiHttpRequest.h"
#include "WiFi.h"

class SetupApi
{
public:
    static void init();

private:
    static std::vector<WiFiAPRecord> m_apRecords;
    static bool m_scanned;
    static bool m_scanning;

    static void scan(const ApiHttpRequest& req, ApiHttpResponse& resp);
    static void connect(const ApiHttpRequest& req, ApiHttpResponse& resp);
    static void deviceData(const ApiHttpRequest& req, ApiHttpResponse& resp);
    static void getSitemaps(const ApiHttpRequest& req, ApiHttpResponse& resp);
    static void setOpenhabData(const ApiHttpRequest& req, ApiHttpResponse& resp);
};


#endif //LIGHTSWITCH_FACTORY_SETUPAPI_H
