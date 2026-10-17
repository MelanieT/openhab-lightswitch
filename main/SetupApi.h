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
    static void getSitemaps(const ApiHttpRequest& req, ApiHttpResponse& resp);
    static void setOpenhabData(const ApiHttpRequest& req, ApiHttpResponse& resp);
};


#endif //LIGHTSWITCH_FACTORY_SETUPAPI_H
