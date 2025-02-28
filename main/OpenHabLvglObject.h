//
// Created by Melanie on 27/01/2025.
//

#ifndef LIGHTSWITCH_OPENHABLVGLOBJECT_H
#define LIGHTSWITCH_OPENHABLVGLOBJECT_H

#include <regex>
#include <list>
#include "OpenHabObject.h"
#include "lvgl.h"
#include "IconLoader.h"

struct FlexDescriptor
{
    map<string, lv_flex_flow_t> flows = {
        {"r", LV_FLEX_FLOW_ROW},
        {"c", LV_FLEX_FLOW_COLUMN},
        {"rw", LV_FLEX_FLOW_ROW_WRAP},
        {"cw", LV_FLEX_FLOW_COLUMN_WRAP},
        {"rr", LV_FLEX_FLOW_ROW_REVERSE},
        {"cr", LV_FLEX_FLOW_COLUMN_REVERSE},
        {"rwr", LV_FLEX_FLOW_ROW_WRAP_REVERSE},
        {"cwr", LV_FLEX_FLOW_COLUMN_WRAP_REVERSE},
    };

    map<string, lv_flex_align_t> aligns = {
        {"s", LV_FLEX_ALIGN_START},
        {"e", LV_FLEX_ALIGN_START},
        {"c", LV_FLEX_ALIGN_START},
        {"se", LV_FLEX_ALIGN_START},
        {"sa", LV_FLEX_ALIGN_START},
        {"sb", LV_FLEX_ALIGN_START},
    };

    lv_color_t parseColor(string hex)
    {
        if (hex.length() == 6)
        {
            uint8_t r = strtoul(hex.substr(0, 2).c_str(), nullptr, 16);
            uint8_t g = strtoul(hex.substr(2, 2).c_str(), nullptr, 16);
            uint8_t b = strtoul(hex.substr(4, 2).c_str(), nullptr, 16);

            return lv_color_make(r, g, b);
        }
        printf("Color value %s invalid\r\n", hex.c_str());
        return lv_color_make(0, 0, 0);
    }

    FlexDescriptor() = default;

    FlexDescriptor(const string& title)
    {
        printf("FlexDescriptor parsing %s\r\n", title.c_str());

        label = title;
        bool foundDescriptor = false;

        size_t e;
        auto b = title.find('{');
        if (b != -1)
        {
            e = title.find('}', b);
            if (e != -1)
            {
                foundDescriptor = true;
                label = title.substr(0, b);
            }
        }

        if (!foundDescriptor)
        {
            haveBgColor = true;
            bgColorOff = lv_color_make(0x27, 0x89, 0xd1);
            bgColorOn = lv_color_make(0xe6, 0x7e, 0x22);

            return;
        }

        string desc = regex_replace(title.substr(b, e - b + 1), regex("'"), "\"");

        try
        {
            auto fd = json::parse(desc);

            x = fd.value("x", 25);
            y = fd.value("y", 25);

            if (fd.contains("g"))
                grow = fd.value("g", 1);

            color1 = parseColor(fd.value("c1", "2789d1"));
            color2 = parseColor(fd.value("c2", "e67e22"));
            color3 = parseColor(fd.value("c3", "808080"));
            color4 = parseColor(fd.value("c4", "404040"));

            if (fd.contains("bgoff"))
            {
                bgColorOn = parseColor(fd.value("bgon", "e67e22"));
                bgColorOff = parseColor(fd.value("bgoff", "0000ff"));
                haveBgColor = true;
            }
            if (fd.contains("icoff"))
            {
                imageLineColorOn = parseColor(fd.value("icon", "ffffff"));
                imageLineColorOff = parseColor(fd.value("icoff", "ffffff"));
                haveImageLineColor = true;
            }
            if (fd.contains("f"))
                flex = true;

            auto flowSpec = fd.value("f", "rw");
            if (flows.contains(flowSpec))
                flow = flows[flowSpec];
            else
                printf("Flow spec invalid\r\n");

            auto alignSpec = fd.value("m", "s");
            if (aligns.contains(alignSpec))
                main = aligns[alignSpec];
            else
                printf("Main spec invalid\r\n");

            alignSpec = fd.value("c", "c");
            if (aligns.contains(alignSpec))
                cross = aligns[alignSpec];
            else
                printf("Cross spec invalid\r\n");

            alignSpec = fd.value("t", "c");
            if (aligns.contains(alignSpec))
                track = aligns[alignSpec];
            else
                printf("Track spec invalid\r\n");


        }
        catch (...)
        {
            return;
        }
    }
    bool flex = false;
    lv_flex_align_t main;
    lv_flex_align_t cross;
    lv_flex_align_t track;
    lv_flex_flow_t flow;
    int x = 25;
    int y = 25;
    string label;
    int grow = 0;
    lv_color_t bgColorOn = lv_color_make(255, 0, 0);
    lv_color_t bgColorOff = lv_color_make(255, 0, 0);
    bool haveBgColor = false;
    lv_color_t imageLineColorOn = lv_color_make(255, 255, 255);
    lv_color_t imageLineColorOff = lv_color_make(255, 255, 255);
    bool haveImageLineColor = false;
    lv_color_t color1;
    lv_color_t color2;
    lv_color_t color3;
    lv_color_t color4;

    void applyFlexStyles(lv_obj_t *o)
    {
        lv_obj_set_size(o, lv_pct(x), lv_pct(y));
        if (grow)
            lv_obj_set_flex_grow(o, grow);
        if (haveBgColor)
            lv_obj_set_style_bg_color(o, bgColorOff, 0);
        if (!flex)
        {
            lv_obj_set_layout(o, 0);
            return;
        }
        printf("Applying flex styles to %s\r\n", label.c_str());

        lv_obj_set_layout(o, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(o, flow);
        lv_obj_set_flex_align(o, main, cross, track);
    }
};

class OpenHabLvglObject : public OpenHabObject, public IconReceiver
{
public:
    OpenHabLvglObject(OpenHabObject *parent, const json& data, void *userData);
    ~OpenHabLvglObject() override;

    inline OpenHabLvglObject *parent() { return (OpenHabLvglObject *)OpenHabObject::parent();};
    inline lv_obj_t *lvglObject() { return m_lvglObject; };

    void receiveIcon(IconInfo *info) override;

protected:

    lv_obj_t *m_lvglObject = nullptr;
    lv_obj_t *m_lvglParent = nullptr;
    lv_obj_t *m_icon = nullptr;
    lv_coord_t m_iconSize = 128;
    lv_obj_t *m_label = nullptr;
    string m_strippedLabel;
    FlexDescriptor m_flexDescriptor;
    string m_currentIconName;
    lv_img_dsc_t m_imageDescriptor;
    bool m_imageHasAlpha;
    string m_stateString;

private:
    virtual void updateState() {};

};


#endif //LIGHTSWITCH_OPENHABLVGLOBJECT_H
