/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

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


#pragma once

#include "g_local.h"
#include "entity.h"
#include "camera.h"
#include "navigate.h"

extern Event EV_PlayerStart_SetThread;

class PlayerStart : public Entity
{
public:
    CLASS_PROTOTYPE(PlayerStart);  

private:
    str thread;

public:


    void Archive(Archiver& arc) override;

    void SetAngle(Event *ev);
    void SetThread(Event *ev);
    str  getThread(void);
};

inline void PlayerStart::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveString(&thread);
}

class TestPlayerStart : public PlayerStart
{
public:
    CLASS_PROTOTYPE(TestPlayerStart);  
};

class PlayerDeathmatchStart : public PlayerStart
{
private:
    void SetType(Event *ev);

public:
    str _type;

    PlayerDeathmatchStart() {}

    CLASS_PROTOTYPE(PlayerDeathmatchStart);
};

class PlayerIntermission : public Camera
{
public:
    CLASS_PROTOTYPE(PlayerIntermission);
    PlayerIntermission();
};
