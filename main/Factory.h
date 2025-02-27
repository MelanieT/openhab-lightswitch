//
// Created by Melanie on 27/01/2025.
//

#ifndef LIGHTSWITCH_FACTORY_H
#define LIGHTSWITCH_FACTORY_H

#include "OpenHabFactory.h"

class Factory : public OpenHabFactory
{
public:
    Factory() = default;
    ~Factory() override = default;

    OpenHabObject * createObject(OpenHabObject *parent, const nlohmann::json &data, Type type, void *userData) override;
};


#endif //LIGHTSWITCH_FACTORY_H
