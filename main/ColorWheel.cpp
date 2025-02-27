//
// Created by Melanie on 04/02/2025.
//

#include "ColorWheel.h"
#include "OpenHab.h"
#include "lvgl_port.h"

static vector<string> split (const string &s, char delim) {
    vector<string> result;
    stringstream ss (s);
    string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

ColorWheel::ColorWheel(OpenHabObject *parent, const json &data) : OpenHabLvglObject(parent, data, nullptr)
{
    OpenHab::registerEventHandler(this);

    if (lvgl_port_lock(-1))
    {
        m_lvglObject = lv_obj_create(m_lvglParent);
        lv_obj_set_style_radius(m_lvglObject, 0, 0);
        lv_obj_set_style_border_width(m_lvglObject, 4, 0);
        lv_obj_set_style_border_color(m_lvglObject, lv_color_make(0, 0, 0), 0);

        lv_obj_clear_flag(m_lvglObject, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(m_lvglObject, LV_OBJ_FLAG_PRESS_LOCK);

        m_flexDescriptor.applyFlexStyles(m_lvglObject);

        lv_obj_update_layout(m_lvglObject);

        auto w = lv_obj_get_width(m_lvglObject);
        auto h = lv_obj_get_height(m_lvglObject);
        auto s = min(w, h);

        m_colorWheel = lv_colorwheel_create(m_lvglObject, true);
        lv_colorwheel_set_mode_fixed(m_colorWheel, true);
        lv_obj_set_style_arc_width(m_colorWheel, 48, LV_PART_MAIN);
        lv_obj_set_style_pad_all(m_colorWheel, 20, LV_PART_KNOB);
        lv_obj_set_style_outline_width(m_colorWheel, 4, LV_PART_KNOB);
        lv_obj_set_style_outline_color(m_colorWheel, lv_color_make(0, 0, 0), LV_PART_KNOB);

        lv_obj_set_size(m_colorWheel, s - 60, s - 60);

        lv_obj_center(m_colorWheel);

        m_modeButton = lv_btn_create(m_colorWheel);
        lv_obj_set_size(m_modeButton, 128, 128);
        lv_obj_center(m_modeButton);
        lv_obj_set_style_radius(m_modeButton, 64, 0);
        lv_obj_set_style_bg_color(m_modeButton, m_flexDescriptor.color1, 0);
        lv_obj_add_event_cb(m_modeButton, handleMode, LV_EVENT_CLICKED, this);
        m_modeLabel = lv_label_create(m_modeButton);
        lv_obj_center(m_modeLabel);
        lv_label_set_text(m_modeLabel, "Hue");

        auto pieces = split(m_widgetData.item.state, ',');
        h = (short)strtoul(pieces[0].c_str(), nullptr, 10);
        s = (short)strtoul(pieces[1].c_str(), nullptr, 10);
        auto b = (short)strtoul(pieces[2].c_str(), nullptr, 10);

        lv_colorwheel_set_hsv(m_colorWheel, (lv_color_hsv_t){(uint16_t)h, (uint8_t)s, (uint8_t)b});

        lv_obj_add_event_cb(m_colorWheel, handleColor, LV_EVENT_VALUE_CHANGED, this);
        lvgl_port_unlock();
    }
}

ColorWheel::~ColorWheel()
{
    OpenHab::unregisterEventHandler(this);
}

void ColorWheel::handleEvent(std::string target, json &evt)
{
    if (m_widgetData.item.name == target)
    {
        auto type = evt.value("type", "");
        if (type == "ItemStateUpdatedEvent" || type == "GroupStateUpdatedEvent")
        {
            if (evt["payload"].contains("value"))
            {
                auto stateString = evt["payload"].value("value", "");

                auto pieces = split(stateString, ',');
                auto h = (uint16_t) strtoul(pieces[0].c_str(), nullptr, 10);
                auto s = (uint8_t) strtoul(pieces[1].c_str(), nullptr, 10);
                auto b = (uint8_t) strtoul(pieces[2].c_str(), nullptr, 10);

                if (lvgl_port_lock(-1))
                {
                    lv_colorwheel_set_hsv(m_colorWheel, (lv_color_hsv_t) {h, s, b});

                    lvgl_port_unlock();
                }
            }
        }
    }

}

void ColorWheel::handleMode(lv_event_t *e)
{
    auto *me = (ColorWheel *) e->user_data;

    me->m_modeIndex++;
    if (me->m_modeIndex >= me->m_modes.size())
        me->m_modeIndex = 0;

    if (lvgl_port_lock(-1))
    {
        auto it = me->m_modes.begin();
        advance(it, me->m_modeIndex);
        lv_label_set_text(me->m_modeLabel, it->first.c_str());
        lv_colorwheel_set_mode(me->m_colorWheel, it->second);

        lvgl_port_unlock();
    }
}

void ColorWheel::handleColor(lv_event_t *e)
{
    auto me = (ColorWheel *)e->user_data;

    auto color = lv_colorwheel_get_hsv(me->m_colorWheel);

    char buf[40];
    snprintf(buf, 40, "%d,%d,%d", color.h, color.s, color.v);
    string data(buf);
    std::vector<uint8_t> body(data.length());
    std::transform(data.begin(), data.end(), body.begin(), [](const char c){ return (uint8_t)c; });
    HttpRequests::postUrl(me->m_widgetData.item.link, body, nullptr);
}
