//
// Created by Melanie on 30/01/2025.
//

#ifndef LIGHTSWITCH_FLYOUT_H
#define LIGHTSWITCH_FLYOUT_H

#include "OpenHabLvglObject.h"
#include "OpenHabObject.h"
#include "nlohmann/json.hpp"

using namespace nlohmann;
using namespace std;

class Flyout : public OpenHabLvglObject
{
public:
    Flyout(OpenHabObject *parent, const json& data);
    ~Flyout() override;

    void receiveIcon(IconInfo *info) override;

private:
    static void handleClick(lv_event_t *e);
    void realHandleClick(lv_event_t *e);
    static void handleBack(lv_event_t  *e);
    void realHandleBack(lv_event_t  *e);
    void updateState() override;

    lv_obj_t *m_flyout = nullptr;
    lv_obj_t *m_flyoutParent = nullptr;
    lv_obj_t *m_backButton = nullptr;
    lv_obj_t *m_backBackground = nullptr;
    lv_obj_t *m_backIcon = nullptr;
    lv_img_dsc_t m_backDescriptor;

    shared_ptr<OpenHabObject> m_rootObject;
};


#endif //LIGHTSWITCH_FLYOUT_H
