//
// Created by Melanie on 04/02/2025.
//

#ifndef LIGHTSWITCH_COLORWHEEL_H
#define LIGHTSWITCH_COLORWHEEL_H

#include "OpenHabLvglObject.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;
using namespace std;

class ColorWheel : public OpenHabLvglObject
{
public:
    ColorWheel(OpenHabObject *parent, const json& data);
    ~ColorWheel() override;

    void handleEvent(std::string target, nlohmann::json &evt) override;

private:
    ordered_map<string, int> m_modes = {
        {"Hue", LV_COLORWHEEL_MODE_HUE},
        {"Saturation", LV_COLORWHEEL_MODE_SATURATION},
        {"Brightness", LV_COLORWHEEL_MODE_VALUE}
    };
    int m_modeIndex = 0;

    static void handleMode(lv_event_t *e);
    static void handleColor(lv_event_t *e);

    lv_obj_t *m_colorWheel;
    lv_obj_t *m_modeButton;
    lv_obj_t *m_modeLabel;
};


#endif //LIGHTSWITCH_COLORWHEEL_H
