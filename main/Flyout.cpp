//
// Created by Melanie on 30/01/2025.
//

#include "Flyout.h"
#include "Frame.h"
#include "Switch.h"
#include "lvgl_port.h"
#include "OpenHab.h"
#include "Factory.h"

extern OpenHab openhab;

Flyout::Flyout(OpenHabObject *parent, const json &data) : OpenHabLvglObject(parent, data, nullptr)
{
    if (m_strippedLabel == "FL")
    {
        auto parentFrame = dynamic_cast<class Frame *>(parent);
        if (parentFrame == nullptr)
        {
            printf("Flyout parent is not a Frame\r\n");
            return;
        }

        auto siblings = parentFrame->children();
        if (siblings.size() != 1)
        {
            printf("Flyout must have exactly one siblingr\r\n");
            return;
        }

        auto *button = dynamic_cast<class Switch *>(siblings.front().get());
        if (button == nullptr)
        {
            printf("Sibling is not a switch\r\n");
            return;
        }

        IconLoader::registerReceiver(this);

        if (lvgl_port_lock(-1))
        {
            m_lvglObject = lv_btn_create(button->lvglObject());
            lv_obj_set_size(m_lvglObject, 48, 48);
            lv_obj_set_style_radius(m_lvglObject, 24, 0);
            lv_obj_align(m_lvglObject, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
            lv_obj_set_style_bg_color(m_lvglObject, m_flexDescriptor.bgColorOff, 0);
            m_icon = lv_img_create(m_lvglObject);
            lv_obj_set_size(m_icon, 32, 32);
            lv_obj_center(m_icon);

            lv_obj_set_style_img_recolor_opa(m_icon, 255, 0);
            lv_obj_set_style_img_recolor(m_icon, m_flexDescriptor.imageLineColorOff, 0);

            lv_obj_add_event_cb(m_lvglObject, handleClick, LV_EVENT_CLICKED, this);

            lvgl_port_unlock();
        }

        m_iconSize = 32;
        IconLoader::fetchIcon("sv-cog", 32);
    }
    else if (m_strippedLabel == "BTN")
    {
        IconLoader::registerReceiver(this);

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
//            auto w = lv_obj_get_width(m_lvglObject);
//            auto h = lv_obj_get_height(m_lvglObject);
//            lv_obj_set_pos(m_icon, w / 2 - 64, h / 2 - 64);
            lv_obj_center(m_icon);

//        lv_img_set_zoom(m_icon, 512);

            lv_obj_add_event_cb(m_lvglObject, handleClick, LV_EVENT_CLICKED, this);

            lvgl_port_unlock();
        }

        IconLoader::fetchIcon(m_widgetData.icon, m_iconSize);
    }
}

Flyout::~Flyout()
{
    IconLoader::unregisterReceiver(this);
}

void Flyout::handleClick(lv_event_t *e)
{
    ((Flyout *)e->user_data)->realHandleClick(e);
}

void Flyout::realHandleClick(lv_event_t *e)
{
    if (lvgl_port_lock(-1))
    {
        auto factory = make_shared<Factory>();

        m_flyout = lv_obj_create(lv_scr_act());
        lv_obj_set_style_bg_color(m_flyout, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_opa(m_flyout, LV_OPA_100, 0);
        lv_obj_set_style_border_width(m_flyout, 0, 0);
        lv_obj_set_style_outline_width(m_flyout, 0, 0);
        lv_obj_set_style_radius(m_flyout, 0, 0);
        lv_obj_set_size(m_flyout, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
        lv_obj_center(m_flyout);
        lv_obj_set_style_pad_all(m_flyout, 0, 0);
        lv_obj_set_style_pad_gap(m_flyout, 0, 0);


        m_flyoutParent = lv_obj_create(m_flyout);
        lv_obj_set_style_bg_color(m_flyoutParent, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_opa(m_flyoutParent, LV_OPA_100, 0);
        lv_obj_set_style_border_width(m_flyoutParent, 0, 0);
        lv_obj_set_style_outline_width(m_flyoutParent, 0, 0);
        lv_obj_set_style_radius(m_flyoutParent, 0, 0);
        lv_obj_set_size(m_flyoutParent, (lv_coord_t)(lv_obj_get_width(lv_scr_act()) - 48), lv_obj_get_height(lv_scr_act()));
        lv_obj_set_pos(m_flyoutParent, 48, 0);
        lv_obj_set_style_pad_all(m_flyoutParent, 0, 0);
        lv_obj_set_style_pad_gap(m_flyoutParent, 0, 0);
        lv_obj_update_layout(m_flyoutParent);

        m_backButton = lv_btn_create(m_flyout);

        lv_obj_clear_flag(m_backButton, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(m_backButton, LV_OBJ_FLAG_PRESS_LOCK);

        lv_obj_set_style_radius(m_backButton, 0, 0);
        lv_obj_set_style_border_width(m_backButton, 4, 0);
        lv_obj_set_style_border_color(m_backButton, lv_color_make(0, 0, 0), 0);

        lv_obj_set_style_bg_color(m_backButton, lv_color_make(31, 97, 141), 0);
        lv_obj_set_size(m_backButton, 48, lv_obj_get_height(lv_scr_act()));
        lv_obj_set_pos(m_backButton, 0, 0);

        lv_obj_add_event_cb(m_backButton, handleBack, LV_EVENT_CLICKED, this);

        m_backBackground = lv_obj_create(m_backButton);
        lv_obj_set_size(m_backBackground, 40, 40);
        lv_obj_center(m_backBackground);
        lv_obj_set_style_bg_color(m_backBackground, lv_color_make(255, 255, 255), 0);
        lv_obj_set_style_border_width(m_backBackground, 0, 0);
        lv_obj_set_style_outline_width(m_backBackground, 0, 0);
        lv_obj_set_style_radius(m_backBackground, 20, 0);
        lv_obj_clear_flag(m_backBackground, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(m_backBackground, LV_OBJ_FLAG_SCROLLABLE);

        m_backIcon = lv_img_create(m_backBackground);
        lv_obj_set_size(m_backIcon, 32, 32);
        lv_obj_center(m_backIcon);
        lv_obj_set_style_img_recolor(m_backIcon, lv_color_make(0, 0, 2505), 0);
        lv_obj_set_style_img_recolor_opa(m_backIcon, LV_OPA_100, 0);

        m_flexDescriptor.applyFlexStyles(m_lvglObject);

        openhab.loadSitemap(m_widgetData.linkedPage.link, factory.get(), [this](auto root) {
            m_rootObject = root;
        }, m_flyoutParent);
        lvgl_port_unlock();
    }

    IconLoader::fetchIcon("sv-arrow-left", 32);
}

void Flyout::receiveIcon(IconInfo *info)
{
    OpenHabLvglObject::receiveIcon(info);

    if (info->name == "sv-arrow-left" && m_backIcon)
    {
        m_backDescriptor.header = {
            .cf = info->hasAlpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR,
            .w = (uint32_t)info->width,
            .h = (uint32_t)info->height
        };
        m_backDescriptor.data_size = info->data.size();
        m_backDescriptor.data = info->pngdata.data();

        lv_img_cache_invalidate_src(&m_backDescriptor);
        lv_img_set_src(m_backIcon, &m_backDescriptor);
    }
}

void Flyout::handleBack(lv_event_t *e)
{
    ((Flyout *)e->user_data)->realHandleBack(e);
}

void Flyout::realHandleBack(lv_event_t *e)
{
    m_rootObject->deleteChildren();
    m_rootObject.reset();

    if (m_backIcon)
        lv_obj_del(m_backIcon);
    if (m_backBackground)
        lv_obj_del(m_backBackground);
    if (m_backButton)
        lv_obj_del(m_backButton);
    if (m_flyoutParent)
        lv_obj_del(m_flyoutParent);
    if (m_flyout)
        lv_obj_del(m_flyout);
    m_backIcon = nullptr;
    m_backBackground = nullptr;
    m_backButton = nullptr;
    m_flyoutParent = nullptr;
    m_flyout = nullptr;

    if (m_strippedLabel == "FL")
    {
        lv_obj_set_size(m_lvglObject, 48, 48);
        lv_obj_set_style_radius(m_lvglObject, 24, 0);
        lv_obj_align(m_lvglObject, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
        lv_obj_set_style_bg_color(m_lvglObject, m_flexDescriptor.bgColorOff, 0);
        lv_obj_set_style_pad_all(m_lvglObject, 8, 0);

        lv_obj_set_size(m_icon, 32, 32);
        lv_obj_center(m_icon);
        lv_obj_update_layout(m_icon);
    }
}

void Flyout::updateState()
{
    if (m_imageHasAlpha && m_flexDescriptor.haveImageLineColor)
    {
        lv_obj_set_style_img_recolor(m_icon, m_flexDescriptor.imageLineColorOff, 0);
        lv_obj_set_style_img_recolor_opa(m_icon, LV_OPA_100, 0);
    }
    else
    {
        lv_obj_set_style_img_recolor_opa(m_icon, LV_OPA_0, 0);
    }
}
