

#pragma once

#include "animate.h"
#include "archive.h"

class Body : public Entity
{
private:
    void Damage(Event *ev);

public:
    CLASS_PROTOTYPE(Body);
    Body();
};

