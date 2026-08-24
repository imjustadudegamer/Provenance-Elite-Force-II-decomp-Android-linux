/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

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
#include "../corepp/listener.h"
#include "entity.h"
#include "trigger.h"
#include "g_utils.h"

#define MOVE_ANGLES 1
#define MOVE_ORIGIN 2

class Entity;
class Mover;

typedef SafePtr<Mover> MoverPtr;

class Mover : public Listener
{
private:
    Vector finaldest;
    Vector angledest;
    Event *endevent;
    int    moveflags;

    // SDK mover.h:39.  The owning entity.  NEVER archived -- Entity::Archive re-supplies
    Entity *self;

public:
    CLASS_PROTOTYPE(Mover);

    Mover();
    explicit Mover(Entity *ent);
    virtual ~Mover() override;

    void MoveDone(Event *ev);
    void MoveTo(const Vector& tdest, const Vector& angdest, float tspeed, Event& event);
    void LinearInterpolate(const Vector& tdest, const Vector& angdest, float time, Event& event);

    int  GetMoveFlags(void) const { return moveflags; }
    void SetMoveFlags(const int moveFlags) { moveflags = moveFlags; }
    void SetEndEvent(const int endEvent);
    void SetFinalDestination(const Vector& finalDestination) { finaldest = finalDestination; }

    void Archive(Archiver& arc) override;
};

inline void Mover::Archive(Archiver& arc)
{

    arc.ArchiveVector(&finaldest);
    arc.ArchiveVector(&angledest);
    arc.ArchiveEventPointer(&endevent);
    arc.ArchiveInteger(&moveflags);
}
