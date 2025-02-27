//
// Created by Melanie on 27/01/2025.
//

#include "Frame.h"
#include "lvgl_port.h"

Frame::Frame(OpenHabObject *parent, const json &data) : OpenHabLvglObject(parent, data, nullptr)
{
    if (lvgl_port_lock(-1))
    {
        m_lvglObject = lv_obj_create(m_lvglParent);

        lv_obj_clear_flag(m_lvglObject, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(m_lvglObject, LV_OBJ_FLAG_PRESS_LOCK);

        lv_obj_set_style_pad_all(m_lvglObject, 0, 0);
        lv_obj_set_style_pad_gap(m_lvglObject, 0, 0);

        lv_obj_set_style_radius(m_lvglObject, 0, 0);
        lv_obj_set_style_outline_width(m_lvglObject, 0, 0);
        lv_obj_set_style_border_width(m_lvglObject, 0, 0);

        lv_obj_set_style_bg_color(m_lvglObject, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_opa(m_lvglObject, LV_OPA_100, 0);

        m_flexDescriptor.applyFlexStyles(m_lvglObject);

//        lv_obj_set_style_bg_color(m_lvglObject, lv_color_make(255, 0, 0), 0);
        lvgl_port_unlock();

        if (m_strippedLabel == "GP")
            m_isGroupFlyout = true;
    }
}

Frame::~Frame()
{

}
