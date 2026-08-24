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
#include "playerstart.h"


Event EV_PlayerStart_SetThread (    "thread",
    EV_SCRIPTONLY,
    "s",
    "thread",
    "Set the thread to execute when this player start is used");

CLASS_DECLARATION(Entity, PlayerStart, "info_player_start") {
    {&EV_SetAngle,              &PlayerStart::SetAngle },
    {&EV_PlayerStart_SetThread, &PlayerStart::SetThread},
    {NULL,                      NULL                    }
};


void PlayerStart::SetAngle(Event *ev)
{
    angles = Vector(0.0f, ev->GetFloat(1), 0.0f);
}

void PlayerStart::SetThread(Event *ev)
{
    thread = ev->GetString(1);
}

str PlayerStart::getThread(void)
{
    return thread;
}


CLASS_DECLARATION(PlayerStart, TestPlayerStart, "testplayerstart") {
    {NULL, NULL}
};


Event EV_PlayerDeathmatchStart_SetType (    "spawnpoint_type",
    EV_DEFAULT,
    "s",
    "spawnpointType",
    "Sets the named type of this spawnpoint");

CLASS_DECLARATION(PlayerStart, PlayerDeathmatchStart, "info_player_deathmatch") {
    {&EV_PlayerDeathmatchStart_SetType, &PlayerDeathmatchStart::SetType},
    {NULL,                              NULL                          }
};

void PlayerDeathmatchStart::SetType(Event *ev)
{
    _type = ev->GetString(1);
}



CLASS_DECLARATION(Camera, PlayerIntermission, "info_player_intermission") {
    {NULL, NULL}
};

PlayerIntermission::PlayerIntermission()

{
    currentstate.watch.watchPath = false;
}
