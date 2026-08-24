

#include "glb_local.h"
#include "simpleentity.h"
#include "worldspawn.h"
#include "level.h"
#include "../script/scriptexception.h"


void Entity::SetTargetName(const char *text)
{
    if (targetname.length() && world) {
        world->RemoveTargetEntity(targetname, this);
    }

    if (text) {
        if (text[0] == '$') {
            text++;
        }
        targetname = text;
    } else {
        targetname = "";
    }

    if (targetname.length() && world) {
        if ((this != world) || (targetname == str("world"))) {
            world->AddTargetEntity(targetname, this);
        } else {
            error("SetTargetName", "World was re-targeted with targetname %s\n", targetname.c_str());
        }
    }
}








extern Event EV_Path_SetTargetname;

extern Event EV_Path_SetOriginEvent;

extern Event EV_FuncBeam_SetTarget;

extern Event EV_Path_SetAngles;

extern Event EV_SetAngle;




void Entity::GetAngles(Event *ev)
{
    ev->ReturnVector(angles);
}

void Entity::SetOrigin(Event *ev)
{
    setOrigin(ev->GetVector(1));
}

void Entity::GetOrigin(Event *ev)
{
    ev->ReturnVector(origin);
}


void Entity::SetTarget(const char *text)
{
    if (text) {
        target = text;
    } else {
        target = "";
    }
}

void Entity::SetTarget(Event *ev)
{
    SetTarget(ev->GetString(1));
}




void Entity::SetAngleEvent(Event *ev)
{
    float angle = ev->GetFloat(1);
    Vector dir = G_GetMovedir(angle);
    setAngles(dir.toAngles());
}




