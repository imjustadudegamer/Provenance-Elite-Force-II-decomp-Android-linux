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


#include "nature.h"




Event EV_PuffDaddy_Idle
( 
	"idle",
	EV_DEFAULT,
	NULL,
	NULL,
	"Animates the puff daddy."
);

Event EV_Emitter_EmitterName (	"emitter",
	EV_DEFAULT,
	"s",
	"name",
	"Emitter to use"
);








CLASS_DECLARATION(Entity, Emitter, "func_emitter") {
    {&EV_Emitter_EmitterName, &Emitter::EmitterName},
    {NULL,                    NULL                 }
};

Emitter::Emitter()
{
    edict->s.eType = ET_EMITTER;
}

void Emitter::setEmitter(str name)
{
    emitterName      = name;
    edict->s.tag_num = gi.imageindex(emitterName);
}

void Emitter::EmitterName(Event *ev)
{
    setEmitter(ev->GetString(1));
}


CLASS_DECLARATION(Entity, Rain, "func_rain") {
    {NULL, NULL}
};

Rain::Rain()
{
    if (LoadingSavegame) {
        return;
    }
    setSolidType(SOLID_NOT);
    edict->s.eType = ET_RAIN;
    setRainName("defaultrain");
}

void Rain::setRainName(str name)
{
    rainName         = name;
    edict->s.tag_num = gi.imageindex(rainName);
}



CLASS_DECLARATION(Entity, PuffDaddy, "plant_puffdaddy") {
    {&EV_Touch,          &PuffDaddy::Touch},
    {&EV_PuffDaddy_Idle, &PuffDaddy::Idle },
    {NULL,               NULL             }
};

void PuffDaddy::Idle(Event *ev)
{
    animate->RandomAnimate("idle");
}

void PuffDaddy::Touch(Event *ev)
{
    Entity *other;

    other = ev->GetEntity(1);

    if (!other->inheritsFrom("Sentient")) {
        return;
    }

    animate->RandomAnimate("touch", EV_PuffDaddy_Idle);

    SurfaceCommand("puffdaddy", "+nodraw");
    setSolidType(SOLID_NOT);
}

PuffDaddy::PuffDaddy()
{
    animate = new Animate(this);
    if (LoadingSavegame) {
        return;
    }
    setSolidType(SOLID_TRIGGER);
    edict->s.eType = ET_MODELANIM;
    setModel("plant_puffdaddy.tik");
    PostEvent(EV_Show, 0);
}
