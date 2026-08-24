

#include "animate.h"
#include "body.h"
#include "g_phys.h"

CLASS_DECLARATION(Entity, Body, NULL) {
    {&EV_Damage, &Body::Damage},
    {NULL,       NULL         }
};

// entirely off r.contents and never reads this flag -- which is exactly retail's
Body::Body()
{
    Event *newEvent;

    animate = new Animate(this);
    takedamage       = DAMAGE_YES;
    edict->s.eType   = ET_MODELANIM;
    health           = 10;
    edict->svflags |= SVF_DEADMONSTER;  
    edict->clipmask  = MASK_DEADSOLID;

    setSolidType(SOLID_NOT);
    setMoveType(MOVETYPE_NONE);

    newEvent = new Event(EV_DisplayEffect);
    newEvent->AddString("TransportOut");
    newEvent->AddString("Multiplayer");
    PostEvent(newEvent, 0.0f);

    PostEvent(EV_Remove, 5.0f);
}

void Body::Damage(Event *ev) {}




const char *Entity::Target()
	{
	return target.c_str();
	}

qboolean Entity::Targeted()
	{
	if ( !targetname.length() )
		return false;

	return true;
	}

const char *Entity::TargetName()
	{
	return targetname.c_str();
	}
