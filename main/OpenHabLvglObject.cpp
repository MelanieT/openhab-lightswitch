//
// Created by Melanie on 27/01/2025.
//

#include "OpenHabLvglObject.h"
#include "lvgl_port.h"
#include "Frame.h"


OpenHabLvglObject::OpenHabLvglObject(OpenHabObject *parent, const json &data, void *userData) : OpenHabObject(parent, data)
{
    printf("OpenHabLvglObject parent=%08lx\r\n", parent);

    m_lvglParent = parent ? this->parent()->m_lvglObject : (userData ? (lv_obj_t*)userData : lv_scr_act());
    printf("LvglParent=%08lx\r\n", m_lvglParent);

    lv_coord_t width = lv_obj_get_width(m_lvglParent);
    lv_coord_t height = lv_obj_get_height(m_lvglParent);

    m_flexDescriptor = FlexDescriptor((m_widgetData.type == Type::Root && m_widgetData.label.find_first_of('{') == string::npos) ? "{'f':'rw'}" : m_widgetData.label);

    m_strippedLabel = m_flexDescriptor.label;

    if (m_widgetData.type == Type::Root)
    {
        printf("Type is root, label is %s\r\n", m_widgetData.label.c_str());
        if (lvgl_port_lock(-1))
        {
            m_lvglObject = lv_obj_create(m_lvglParent);

            lv_obj_clear_flag(m_lvglObject, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(m_lvglObject, LV_OBJ_FLAG_PRESS_LOCK);

            lv_obj_set_pos(m_lvglObject, 0, 0);
            lv_obj_set_size(m_lvglObject, width, height);

            lv_obj_set_style_pad_all(m_lvglObject, 0, 0);
            lv_obj_set_style_pad_gap(m_lvglObject, 0, 0);

            lv_obj_set_style_radius(m_lvglObject, 0, 0);
            lv_obj_set_style_outline_width(m_lvglObject, 0, 0);
            lv_obj_set_style_border_width(m_lvglObject, 0, 0);

            lv_obj_set_style_bg_color(m_lvglObject, lv_color_make(0, 0, 0), 0);
            lv_obj_set_style_bg_opa(m_lvglObject, LV_OPA_100, 0);

            if (m_flexDescriptor.flex)
            {
                lv_obj_set_layout(m_lvglObject, LV_LAYOUT_FLEX);
                lv_obj_set_flex_flow(m_lvglObject, m_flexDescriptor.flow);
                lv_obj_set_flex_align(m_lvglObject, m_flexDescriptor.main, m_flexDescriptor.cross, m_flexDescriptor.track);
            }
//            lv_obj_set_style_bg_color(m_lvglObject, lv_color_make(0, 255, 0), 0);
            lvgl_port_unlock();
        }
    }
}

OpenHabLvglObject::~OpenHabLvglObject()
{
}

void OpenHabLvglObject::receiveIcon(IconInfo *info)
{
//    printf("Widget label %s received icon %s, looking for icon %s\r\n", m_strippedLabel.c_str(), info->name.c_str(), m_widgetData.icon.c_str());
    string specificIcon;
    if (!m_stateString.empty())
        specificIcon = m_widgetData.icon + "-" + m_stateString;

    if (info->name != m_widgetData.icon && (!specificIcon.empty() && info->name != specificIcon))
        return; // Not ours

    if (!m_currentIconName.empty() && m_currentIconName == specificIcon)
        return; // Already using the most specific icon

    if (m_currentIconName.empty() || (m_currentIconName != specificIcon && info->name == specificIcon) ||
        (m_currentIconName != specificIcon && m_currentIconName != m_widgetData.icon))
    {
        m_currentIconName = info->name;

        if (m_icon && info->width == m_iconSize && info->height == m_iconSize)
        {
            printf("Using icon %s (%dx%d) on widget %s\r\n", m_widgetData.icon.c_str(), info->width, info->height,
                   m_strippedLabel.c_str());

            memset(&m_imageDescriptor, 0, sizeof(m_imageDescriptor));
            m_imageDescriptor = {
                .header = {
                    .cf = info->hasAlpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR,
                    .w = (uint32_t) info->width,
                    .h = (uint32_t) info->height,
                },
                .data_size = info->pngdata.size(),
                .data = info->pngdata.data()
            };

            //        if (lvgl_port_lock(-1))
            //        {
            lv_img_cache_invalidate_src(&m_imageDescriptor);
            lv_img_set_src(m_icon, &m_imageDescriptor);
            m_imageHasAlpha = info->hasAlpha;

            //            lvgl_port_unlock();
            //        }

            updateState();
        }
    }
}

