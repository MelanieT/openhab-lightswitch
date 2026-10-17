//
// Created by Melanie on 20/02/2025.
//

#include <esp_wifi_types_generic.h>
#include <esp_wifi.h>
#include "SetupApi.h"
#include "UrlMapper.h"
#include "nlohmann/json.hpp"
#include "vector"
#include "main.h"
#include "WiFi.h"
#include "OpenHab.h"

using namespace nlohmann;
using namespace std;


void SetupApi::init()
{
    // Wifi setup and /deviceData are the framework's
    UrlMapper::AddMapping("POST", "/getSitemaps", SetupApi::getSitemaps);
    UrlMapper::AddMapping("POST", "/setOpenhabData", SetupApi::setOpenhabData);
}

void SetupApi::getSitemaps(const ApiHttpRequest &req, ApiHttpResponse &resp)
{
    vector<Sitemap> sitemaps;

    try {
        auto jsonData = json::parse(req.body());
        if (jsonData.contains("url"))
        {
            string url = jsonData["url"].get<string>();
            if (!url.empty())
            {
                openhab.setBaseUrl(url);
                int err = openhab.loadSitemapsList([&sitemaps](const vector<Sitemap> &maps) {
                    printf("Sitemaps received, count %d\r\n", maps.size());
                    sitemaps.insert(sitemaps.end(), maps.begin(), maps.end());
                });
            }
        }
    }
    catch (exception& e) {
        printf("Error deserializing json %s\r\n", e.what());
    }

    vector<string> names;
    for (const auto& s : sitemaps)
        names.push_back(s.name);

    json ret = {
        {"sitemaps", names},
    };

    resp.setBody(ret.dump());
}

void SetupApi::setOpenhabData(const ApiHttpRequest &req, ApiHttpResponse &resp)
{
    try
    {
        auto jsonData = json::parse(req.body());
        if (jsonData.contains("url") && jsonData.contains("sitemap"))
        {
            string url = jsonData["url"].get<string>();
            string sitemap = jsonData["sitemap"].get<string>();
            if (jsonData.contains("apiKey"))
            {
                string apiKey = jsonData["apiKey"].get<string>();
                appMain.saveApiKey(apiKey);
            }

            appMain.saveOpenhabData(url, sitemap);
        }
    }
    catch (...) {}
}
