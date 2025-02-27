//
// Created by Melanie on 27/01/2025.
//

#ifndef LIGHTSWITCH_FRAME_H
#define LIGHTSWITCH_FRAME_H

#include "OpenHabLvglObject.h"

class Frame : public OpenHabLvglObject
{
public:
    Frame(OpenHabObject *parent, const json& data);
    ~Frame() override;

    inline bool isGroupFlyout() { return m_isGroupFlyout; };

private:
    bool m_isGroupFlyout = false;
    lv_obj_t *m_flyoutButton = nullptr;
};


#endif //LIGHTSWITCH_FRAME_H
