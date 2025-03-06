//
// Created by Melanie on 11/02/2025.
//

#include "TextDisplay.h"
#include "OpenHab.h"
#include "lvgl_port.h"
#include "main.h"

static vector<string> split (const string &s, char delim) {
    vector<string> result;
    stringstream ss (s);
    string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

TextDisplay::TextDisplay(OpenHabObject *parent, const json& data)
    : OpenHabLvglObject(parent, data, nullptr)
{
    OpenHab::registerEventHandler(this);

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

        m_label = lv_label_create(m_lvglObject);
        lv_obj_align(m_label, LV_ALIGN_BOTTOM_MID, 0, (short)-(m_iconSize / 10));
        lv_label_set_text(m_label, m_strippedLabel.c_str());

        if (m_widgetData.item.type == ItemType::StringItem && !m_widgetData.icon.empty())
        {
            IconLoader::registerReceiver(this);

            m_isIconLabel = true;
            m_icon = lv_img_create(m_lvglObject);
            lv_obj_set_size(m_icon, m_iconSize, m_iconSize);
            lv_obj_center(m_icon);

            IconLoader::fetchIcon(m_widgetData.icon, m_iconSize);
        }
        else
        {
            m_valueLabel = lv_label_create(m_lvglObject);
            lv_obj_align(m_valueLabel, LV_ALIGN_CENTER, 0, -8);
            lv_obj_set_style_text_font(m_valueLabel, &lv_font_montserrat_40, 0);

            m_unitLabel = lv_label_create(m_lvglObject);
            lv_label_set_text(m_unitLabel, m_widgetData.item.unitSymbol.c_str());
            lv_obj_align_to(m_unitLabel, m_valueLabel, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, -6);
            lv_obj_set_style_text_font(m_unitLabel, &lv_font_montserrat_14, 0);
        }

        lv_obj_add_event_cb(m_lvglObject, handleClick, LV_EVENT_CLICKED, this);

        m_text = m_widgetData.item.state;
        
        TextDisplay::updateState();

        lvgl_port_unlock();
    }
}

TextDisplay::~TextDisplay()
{
    OpenHab::unregisterEventHandler(this);
    if (m_isIconLabel)
        IconLoader::unregisterReceiver(this);
}

void TextDisplay::handleEvent(string target, json &evt)
{
    if (m_widgetData.item.name == target)
    {
        auto type = evt.value("type", "");
        if (type == "ItemStateUpdatedEvent" || type == "GroupStateUpdatedEvent")
        {
            if (evt["payload"].contains("value"))
                m_text = evt["payload"].value("value", "");
        }

        if (lvgl_port_lock(-1))
        {
            updateState();

            lvgl_port_unlock();
        }
    }
}

void TextDisplay::updateState()
{
    auto value = m_text;
    replace(value, m_widgetData.item.unitSymbol, "");
    value.erase(value.find_last_not_of(" \n\r\t")+1);

    if (m_flexDescriptor.haveBgColor)
    {
        if (m_state)
            lv_obj_set_style_bg_color(m_lvglObject, m_flexDescriptor.bgColorOn, 0);
        else
            lv_obj_set_style_bg_color(m_lvglObject, m_flexDescriptor.bgColorOff, 0);
    }

    if (m_isIconLabel)
    {
        m_stateString = m_text;
        IconLoader::fetchIcon(m_widgetData.icon + "-" + m_stateString, m_iconSize);

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

        if (m_widgetData.item.type == ItemType::StringItem && (m_text == "ON" || m_text == "OFF"))
        {
            m_state = m_text == "ON";
        }
    }
    else
    {
        if (m_widgetData.item.type == ItemType::NumberItem && !m_widgetData.item.stateDescription.pattern.empty())
        {
            auto num = strtod(value.c_str(), nullptr);
            char buffer[38];
            snprintf(buffer, 38, m_widgetData.item.stateDescription.pattern.c_str(), num);
            lv_label_set_text(m_valueLabel, buffer);
        }
        else
        {
            lv_label_set_text(m_valueLabel, m_text.c_str());
        }

        lv_obj_align_to(m_unitLabel, m_valueLabel, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, -6);
    }
}

void TextDisplay::handleClick(lv_event_t *e)
{
    ((TextDisplay *)e->user_data)->realHandleClick(e);
}

void TextDisplay::realHandleClick(lv_event_t *e)
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
    else if (!m_widgetData.item.link.empty())
    {
        std::string onOff = m_text;
        std::vector<uint8_t> body(onOff.length());
        std::transform(onOff.begin(), onOff.end(), body.begin(), [](const char c) { return (uint8_t) c; });
        HttpRequests::postUrl(m_widgetData.item.link, body, nullptr);
    }
}
