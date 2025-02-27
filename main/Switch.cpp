//
// Created by Melanie on 27/01/2025.
//

#include "Switch.h"
#include "lvgl_port.h"
#include "OpenHab.h"

extern OpenHab openhab;

Switch::Switch(OpenHabObject *parent, const json &data) : OpenHabLvglObject(parent, data, nullptr)
{
    IconLoader::registerReceiver(this);
    OpenHab::registerEventHandler(this);

    m_stateString = m_widgetData.item.state;

    printf("State string '%s'\r\n", m_stateString.c_str());

    if (m_stateString == "ON")
    {
        m_state = true;
    }

    if (lvgl_port_lock(-1))
    {
        m_lvglObject = lv_btn_create(m_lvglParent);

        lv_obj_clear_flag(m_lvglObject, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(m_lvglObject, LV_OBJ_FLAG_PRESS_LOCK);

        lv_obj_set_style_radius(m_lvglObject, 0, 0);
        lv_obj_set_style_border_width(m_lvglObject, 4, 0);
        lv_obj_set_style_border_color(m_lvglObject, lv_color_make(0, 0, 0), 0);

        lv_obj_set_style_pad_all(m_lvglObject, 0, 0);

        m_flexDescriptor.applyFlexStyles(m_lvglObject);

        lv_obj_update_layout(m_lvglObject);

        if (lv_obj_get_height(m_lvglObject) < 128)
            m_iconSize = 64;
        if (lv_obj_get_height(m_lvglObject) < 64)
            m_iconSize = 32;

        m_icon = lv_img_create(m_lvglObject);
        lv_obj_set_size(m_icon, m_iconSize, m_iconSize);
        lv_obj_center(m_icon);
//        lv_img_set_zoom(m_icon, 512);

        m_label = lv_label_create(m_lvglObject);
        if (m_widgetData.icon == "none")
            lv_obj_center(m_label);
        else
            lv_obj_align(m_label, LV_ALIGN_BOTTOM_MID, 0, (short)-(m_iconSize / 10));
        lv_label_set_text(m_label, m_strippedLabel.c_str());

        lv_obj_add_event_cb(m_lvglObject, handleClick, LV_EVENT_CLICKED, this);

        lvgl_port_unlock();
    }

    Switch::updateState();

    IconLoader::fetchIcon(m_widgetData.icon, m_iconSize);
}

Switch::~Switch()
{
    OpenHab::unregisterEventHandler(this);
    IconLoader::unregisterReceiver(this);
}

void Switch::updateState()
{
    if (m_flexDescriptor.haveBgColor)
    {
        if (m_state)
            lv_obj_set_style_bg_color(m_lvglObject, m_flexDescriptor.bgColorOn, 0);
        else
            lv_obj_set_style_bg_color(m_lvglObject, m_flexDescriptor.bgColorOff, 0);
    }

    if (m_imageHasAlpha && m_flexDescriptor.haveImageLineColor)
    {
        if (m_state)
            lv_obj_set_style_img_recolor(m_icon, m_flexDescriptor.imageLineColorOn, 0);
        else
            lv_obj_set_style_img_recolor(m_icon, m_flexDescriptor.imageLineColorOff, 0);

        lv_obj_set_style_img_recolor_opa(m_icon, LV_OPA_100, 0);
    }
    else
    {
        lv_obj_set_style_img_recolor_opa(m_icon, LV_OPA_0, 0);
    }
}

void Switch::handleClick(lv_event_t *e)
{
    ((Switch *)e->user_data)->realHandleClick(e);
}

void Switch::realHandleClick(lv_event_t *e)
{
    if (m_widgetData.item.link.empty() && !m_widgetData.mappings.empty())
    {
//        printf("Mappings:\r\n");
//        for (auto& m : m_widgetData.mappings)
//        {
//            printf("  %s: %s\r\n", m.command.c_str(), m.label.c_str());
//        }
        vector<uint8_t> body;
        HttpRequests::postUrl(openhab.baseUrl() + m_widgetData.mappings[0].command, body, nullptr);
    }
    else
    {
        m_state = !m_state;

        updateState();

        std::string onOff = m_state ? "ON" : "OFF";
        std::vector<uint8_t> body(onOff.length());
        std::transform(onOff.begin(), onOff.end(), body.begin(), [](const char c) { return (uint8_t) c; });
        HttpRequests::postUrl(m_widgetData.item.link, body, nullptr);
    }
}

void Switch::handleEvent(string target, json& evt)
{
    if (m_widgetData.item.name == target)
    {
        auto type = evt.value("type", "");
        if (type == "ItemStateUpdatedEvent" || type == "GroupStateUpdatedEvent")
        {
            if (evt["payload"].contains("value"))
            {
                auto stateString = evt["payload"].value("value", "");
                if (m_stateString != stateString && !stateString.empty())
                {
                    m_stateString = stateString;
                    if (stateString == "ON")
                        m_state = true;
                    else
                        m_state = false;

                    while (!lvgl_port_lock(5))
                        vTaskDelay(5 / portTICK_PERIOD_MS);
                    lvgl_port_unlock();
                    updateState();
                }
            }
        }
    }
}
