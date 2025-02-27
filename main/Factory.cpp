//
// Created by Melanie on 27/01/2025.
//

#include "Factory.h"
#include "Frame.h"
#include "Switch.h"
#include "Flyout.h"
#include "Slider.h"
#include "ColorWheel.h"
#include "TextDisplay.h"

OpenHabObject *Factory::createObject(OpenHabObject *parent, const json &data, Type type, void *userData)
{
    switch (type)
    {
    case Root:
        printf("Creating root\r\n");
        return new OpenHabLvglObject(parent, data, userData);
    case Frame:
        printf("Creating frame\r\n");
        return new class Frame(parent, data);
    case Switch:
        printf("Creating switch\r\n");
        return new class Switch(parent, data);
    case Text:
        {
            auto parentFrame = dynamic_cast<class Frame *>(parent);
            if (parentFrame != nullptr && parentFrame->isGroupFlyout())
            {
                printf("Creating flyout\r\n");
                return new Flyout(parent, data);
            }
            return new TextDisplay(parent, data);
        }
        break;
    case Slider:
        printf("Creating slider\r\n");
        return new class Slider(parent, data);
    case Colorpicker:
        printf("Creating colorwheel\r\n");
        return new ColorWheel(parent, data);
    }
    return new OpenHabLvglObject(parent, data, userData);
}
