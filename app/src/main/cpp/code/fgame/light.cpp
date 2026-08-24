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


#include "g_local.h"
#include "entity.h"
#include "trigger.h"
#include "light.h"
#include "scriptmaster.h"

extern Event EV_Light_SetLight;





extern Event EV_FuncBeam_Color;


Event EV_Light_SetColor
(
	"color",
	EV_DEFAULT,
	NULL,
	NULL,
	""
);

Event EV_Light_SetFalloff
(
	"falloff",
	EV_CODEONLY,
	NULL,
	NULL,
	""
);

Event EV_Light_SetRadius (    "falloff",
    EV_CODEONLY,
    NULL,
    NULL,
    "");


Event EV_Light_SpotDir (    "spot_dir",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_SpotRadiusByDistance (    "spot_radiusbydistance",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_NoEntityLight (    "no_entity_light",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_EntityTrace (    "entity_trace",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_SpotAngle (    "spot_angle",
    EV_CODEONLY,
    NULL,
    NULL,
    "");

Event EV_Light_SetLight (    "light",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the intensity of the light");

CLASS_DECLARATION(Entity, Light, "light") {
     {&EV_Light_SetLight, NULL},
     {NULL,                    NULL}
};

Light::Light()
{
     PostEvent(EV_Remove, 0);
}
