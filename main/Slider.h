//
// Created by Melanie on 31/01/2025.
//

#ifndef LIGHTSWITCH_SLIDER_H
#define LIGHTSWITCH_SLIDER_H

#include "OpenHabLvglObject.h"
#include "nlohmann/json.hpp"

class Slider : public OpenHabLvglObject
{
public:
    Slider(OpenHabObject *parent, const json &data);
    ~Slider() override;

    void handleEvent(string target, json& evt) override;

private:
    string m_stateString;
    int m_value;
    lv_obj_t *m_slider;
    lv_obj_t *m_iconBg;
    lv_obj_t *m_percentage;

    static void handleSlider(lv_event_t *evt);
    void realHandleSlider(lv_event_t *evt);
    string valueString();
    void updateState() override;
};


#endif //LIGHTSWITCH_SLIDER_H
