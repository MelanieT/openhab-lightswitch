//
// Created by Melanie on 11/02/2025.
//

#ifndef LIGHTSWITCH_TEXTDISPLAY_H
#define LIGHTSWITCH_TEXTDISPLAY_H


#include "OpenHabLvglObject.h"

class TextDisplay : public OpenHabLvglObject
{
public:
    TextDisplay(OpenHabObject *parent, const json& data);
    ~TextDisplay() override;

    void handleEvent(string target, json& evt) override;

private:
    void updateState() override;

    lv_obj_t *m_unitLabel;
    lv_obj_t *m_valueLabel;
    string m_text;
};


#endif //LIGHTSWITCH_TEXTDISPLAY_H
