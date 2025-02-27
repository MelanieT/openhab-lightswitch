//
// Created by Melanie on 31/01/2025.
//

#include "Slider.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "OpenHab.h"

Slider::Slider(OpenHabObject *parent, const json &data) : OpenHabLvglObject(parent, data, nullptr)
{
    IconLoader::registerReceiver(this);
    OpenHab::registerEventHandler(this);

    m_value = (int) strtoul(m_widgetData.item.state.c_str(), nullptr, 10);
    m_stateString = valueString();

    if (lvgl_port_lock(-1))
    {
        m_lvglObject = lv_obj_create(m_lvglParent);
        lv_obj_set_style_radius(m_lvglObject, 0, 0);
        lv_obj_set_style_border_width(m_lvglObject, 4, 0);
        lv_obj_set_style_border_color(m_lvglObject, lv_color_make(0, 0, 0), 0);
        m_flexDescriptor.applyFlexStyles(m_lvglObject);

        m_slider = lv_slider_create(m_lvglObject);
        lv_slider_set_range(m_slider, 0, 100);
        lv_slider_set_value(m_slider, m_value, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(m_slider, LV_OPA_0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(m_slider, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_border_width(m_slider, 10, LV_PART_MAIN);
        lv_obj_set_style_border_opa(m_slider, LV_OPA_100, LV_PART_MAIN);
        lv_obj_set_style_border_color(m_slider, m_flexDescriptor.bgColorOff, LV_PART_MAIN);
        lv_obj_set_style_bg_color(m_slider, m_flexDescriptor.color1, LV_PART_MAIN);
        lv_obj_set_style_bg_color(m_slider, m_flexDescriptor.color2, LV_PART_KNOB);
        lv_obj_set_size(m_slider, 32, lv_pct(60));
        lv_obj_align(m_slider, LV_ALIGN_CENTER, 0, lv_pct(11));

        m_iconBg = lv_obj_create(m_lvglObject);
        lv_obj_set_size(m_iconBg, 64, 64);
        lv_obj_set_style_radius(m_iconBg, 32, 0);
        lv_obj_set_style_bg_color(m_iconBg, lv_color_make(255, 255, 255), 0);
        lv_obj_set_style_outline_width(m_iconBg, 0, 0);
        lv_obj_set_style_border_width(m_iconBg, 0, 0);
        lv_obj_align(m_iconBg, LV_ALIGN_TOP_MID, 0, 5);
        lv_obj_clear_flag(m_iconBg, LV_OBJ_FLAG_SCROLLABLE);


        m_icon = lv_img_create(m_iconBg);
        lv_obj_set_size(m_icon, 48, 48);
        lv_obj_center(m_icon);

        m_percentage = lv_label_create(m_lvglObject);
        lv_obj_align_to(m_percentage, m_slider, LV_ALIGN_OUT_TOP_MID, 0, -24);
        lv_label_set_text(m_percentage, (valueString() + "%").c_str());

        m_label = lv_label_create(m_lvglObject);
        lv_label_set_text(m_label, m_strippedLabel.c_str());
        lv_obj_align_to(m_label, m_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);

        lv_obj_add_event_cb(m_slider, handleSlider, LV_EVENT_VALUE_CHANGED, this);
        lv_obj_add_event_cb(m_slider, handleSlider, LV_EVENT_RELEASED, this);

        lvgl_port_unlock();
    }

    m_iconSize = 48;

    IconLoader::fetchIcon(m_widgetData.icon, 48);
}

Slider::~Slider()
{
    OpenHab::unregisterEventHandler(this);
    IconLoader::unregisterReceiver(this);
}

void Slider::handleSlider(lv_event_t *evt)
{
    ((Slider *)evt->user_data)->realHandleSlider(evt);
}

void Slider::realHandleSlider(lv_event_t *evt)
{
    m_value = lv_slider_get_value(m_slider);
    lv_label_set_text(m_percentage, (valueString() + "%").c_str());

    if (lv_slider_is_dragged(m_slider))
        return;

    auto value = valueString();
    std::vector<uint8_t> body(value.length());
    std::transform(value.begin(), value.end(), body.begin(), [](const char c){ return (uint8_t)c; });
    HttpRequests::postUrl(m_widgetData.item.link, body, nullptr);
}

string Slider::valueString()
{
    char buf[34];
    std::string value(itoa(m_value, buf, 10));

    return value;
}

void Slider::handleEvent(string target, json &evt)
{
    if (m_widgetData.item.name == target)
    {
        auto type = evt.value("type", "");
        if (type == "ItemStateUpdatedEvent" || type == "GroupStateUpdatedEvent")
        {
            if (evt["payload"].contains("value"))
            {
                auto state = evt["payload"].value("value", "0");
                if (m_stateString != state)
                {
                    char buf[34];
                    auto value = (int)strtoul(state.c_str(), nullptr, 10);

                    m_value = value;
                    m_stateString = valueString();

                    if (lvgl_port_lock(-1))
                    {
                        updateState();

                        lvgl_port_unlock();
                    }
                }
            }
        }
    }
}

void Slider::updateState()
{
    lv_label_set_text(m_percentage, (m_stateString + "%").c_str());
    lv_slider_set_value(m_slider, m_value, LV_ANIM_ON);
}
