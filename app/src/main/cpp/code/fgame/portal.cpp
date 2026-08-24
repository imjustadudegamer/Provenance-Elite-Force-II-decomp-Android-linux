

#include "portal.h"
#include "game.h"




Event EV_Portal_Roll
(
	"roll",
	EV_SCRIPTONLY,
	"f",
	"roll",
	"Sets the portal camera's roll."
);

Event EV_Portal_LocateCamera (	"locatecamera",
	EV_CODEONLY,
	NULL,
	NULL,
	"Locates the camera position."
);









CLASS_DECLARATION(Entity, PortalSurface, "portal_surface") {
    {&EV_Portal_LocateCamera, &PortalSurface::LocateCamera},
    {NULL,                    NULL                        }
};

void PortalSurface::LocateCamera(Event *ev)
{
    Entity *owner;
    Entity *cameraTarget;
    Vector  dir;

    owner = (Entity *)G_FindTarget(NULL, Target());

    if (!owner) {
        VectorCopy(edict->s.origin, edict->s.origin2);
        return;
    }

    edict->s.skinNum = owner->edict->s.skinNum;

    VectorCopy(owner->origin, edict->s.origin2);

    cameraTarget = (Entity *)G_FindTarget(NULL, owner->Target());

    if (cameraTarget) {
        dir = cameraTarget->origin - owner->origin;
        dir.normalize();
        setAngles(dir.toAngles());
    } else {
        setAngles(owner->angles);
        dir = owner->orientation[0];
    }
}

PortalSurface::PortalSurface()
{
    if (LoadingSavegame) {
        return;
    }

    VectorClear(edict->mins);
    VectorClear(edict->maxs);

    gi.linkentity(edict);

    edict->svflags = SVF_PORTAL | SVF_SENDPVS;
    edict->s.eType   = ET_PORTAL;

    PostEvent(EV_Portal_LocateCamera, EV_POSTSPAWN);
}



CLASS_DECLARATION(Entity, PortalCamera, "portal_camera") {
    {&EV_Portal_Roll, &PortalCamera::Roll},
    {NULL,            NULL               }
};

void PortalCamera::Roll(Event *ev)
{
    float roll = ev->GetFloat(1);

    edict->s.skinNum = roll / 360.0f * 256;
}

PortalCamera::PortalCamera()
{
    if (LoadingSavegame) {
        return;
    }

    VectorClear(edict->mins);
    VectorClear(edict->maxs);

    edict->s.skinNum = 0;

    gi.linkentity(edict);
}
