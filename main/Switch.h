//
// Created by Melanie on 27/01/2025.
//

#ifndef LIGHTSWITCH_SWITCH_H
#define LIGHTSWITCH_SWITCH_H

#include "OpenHabLvglObject.h"
#include "OpenHabObject.h"
#include "IconLoader.h"

using namespace nlohmann;

class Switch : public OpenHabLvglObject
{
public:
    Switch(OpenHabObject *parent, const json& data);
    ~Switch() override;

    void handleEvent(string target, json& evt) override;

private:
    void updateState() override;
    static void handleClick(lv_event_t *e);
    void realHandleClick(lv_event_t *e);

    string m_fullIconName;
    bool m_state = false;
};


#endif //LIGHTSWITCH_SWITCH_H
