

#pragma once

#include "g_local.h"
#include "entity.h"

class PortalSurface : public Entity
{
public:
    CLASS_PROTOTYPE(PortalSurface);
    PortalSurface();

    void LocateCamera(Event *ev);
};

class PortalCamera : public Entity
{
public:
    CLASS_PROTOTYPE(PortalCamera);
    PortalCamera();
    void Roll(Event *ev);
};
