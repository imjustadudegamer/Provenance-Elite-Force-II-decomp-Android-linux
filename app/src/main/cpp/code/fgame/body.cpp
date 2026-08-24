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
