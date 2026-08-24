/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


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




