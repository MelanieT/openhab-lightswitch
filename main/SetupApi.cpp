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

std::vector<WiFiAPRecord> SetupApi::m_apRecords = {};
bool SetupApi::m_scanned = false;
bool SetupApi::m_scanning = false;

void SetupApi::init()
{
    UrlMapper::AddMapping("POST", "/scan", SetupApi::scan);
    UrlMapper::AddMapping("POST", "/connect", SetupApi::connect);
    UrlMapper::AddMapping("POST", "/deviceData", SetupApi::deviceData);
    UrlMapper::AddMapping("POST", "/getSitemaps", SetupApi::getSitemaps);
    UrlMapper::AddMapping("POST", "/setOpenhabData", SetupApi::setOpenhabData);
}

void SetupApi::scan(const ApiHttpRequest &req, ApiHttpResponse &resp)
{
    m_scanned = true;
    m_apRecords = wifi.scan(); // Will block until done

    resp.AddHeader("Content-type", "application/json");

    vector<string> aplist;
    for (auto ap : m_apRecords)
    {
        aplist.push_back(ap.getSSID());
    }

    json reply = {
        {"apCount", m_apRecords.size()},
        {"aplist", aplist},
    };

    resp.setBody(reply.dump());
}

void SetupApi::connect(const ApiHttpRequest &req, ApiHttpResponse &resp)
{
    try {
        auto jsonData = json::parse(req.body());
        if (jsonData.contains("ssid") && jsonData.contains("password"))
        {
            string ssid = jsonData["ssid"].get<string>();
            string password = jsonData["password"].get<string>();
            appMain.tryConnectWifi(ssid, password);
        }
    }
    catch (...) {}
}

void SetupApi::deviceData(const ApiHttpRequest &req, ApiHttpResponse &resp)
{
    json reply = {
        {"connected", appMain.stationConnected()},
        {"ssid", appMain.ssid()},
        {"base", appMain.openHabUrl()},
        {"sitemap", appMain.sitemap()},
    };

    resp.AddHeader("Content-type", "application/json");

    resp.setBody(reply.dump());
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
