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


#include "g_local.h"
#include "entity.h"
#include "g_utils.h"
#include "mover.h"
#include "level.h"
#include "g_phys.h"

CLASS_DECLARATION(Listener, Mover, "mover") {
    {&EV_MoveDone, &Mover::MoveDone},
    {NULL,         NULL            }
};

Mover::Mover()
{
    assert(0);

    self      = NULL;
    endevent  = NULL;
    moveflags = 0;
}

Mover::Mover(Entity *ent)
{
    self = ent;

    self->edict->s.pos.trType = TR_LERP;

    endevent  = NULL;
    finaldest = ent->origin;
    moveflags = 0;
}

Mover::~Mover() {}

void Mover::MoveDone(Event *ev)
{
    Event *event;
    Vector move;
    Vector amove;

    if (moveflags & MOVE_ANGLES) {
        self->avelocity = vec_zero;
        amove           = angledest - self->localangles;
    } else {
        amove = vec_zero;
    }

    if (moveflags & MOVE_ORIGIN) {
        self->velocity = vec_zero;
        move           = finaldest - self->GetLocalOrigin();
    } else {
        move = vec_zero;
    }

    if (!G_PushMove(self, move, amove)) {
        PostEvent(EV_MoveDone, level.fixedframetime);
        return;
    }

    if (moveflags & MOVE_ORIGIN) {
        self->setOrigin(finaldest);  
    }

    if (moveflags & MOVE_ANGLES) {
        self->localangles = angledest;

        if ((self->localangles.x >= 360.0f) || (self->localangles.x < 0.0f)) {
            self->localangles.x -= (float)(((int)self->localangles.x / 360) * 360);
        }
        if ((self->localangles.y >= 360.0f) || (self->localangles.y < 0.0f)) {
            self->localangles.y -= (float)(((int)self->localangles.y / 360) * 360);
        }
        if ((self->localangles.z >= 360.0f) || (self->localangles.z < 0.0f)) {
            self->localangles.z -= (float)(((int)self->localangles.z / 360) * 360);
        }
    }

    event    = endevent;
    endevent = NULL;

    // Retail's invariant is that endevent is always set before EV_MoveDone can be posted
    // (MoveTo and LinearInterpolate both assign it first, on every path), so retail never
    // retail's invariant holds, which is every other case.
    if (event) {
        self->ProcessEvent(event);
    }
}

void Mover::MoveTo(const Vector& tdest, const Vector& angdest, float tspeed, Event& event)
{
    Vector vdestdelta;
    Vector angdestdelta;
    float  len;
    float  traveltime;

    assert(tspeed >= 0.0f);

    if (!tspeed) {
        error("MoveTo", "No speed is defined!");
    }

    if (tspeed < 0.0f) {
        error("MoveTo", "Speed is negative!");
    }

    CancelEventsOfType(EV_MoveDone);

    moveflags = 0;

    if (endevent) {
        delete endevent;
    }
    endevent = new Event(event);

    finaldest = tdest;
    angledest = angdest;

    if (finaldest != self->GetLocalOrigin()) {
        moveflags |= MOVE_ORIGIN;
    }
    if (angledest != self->localangles) {
        moveflags |= MOVE_ANGLES;
    }

    if (!moveflags) {
        self->velocity  = vec_zero;
        self->avelocity = vec_zero;

        PostEvent(EV_MoveDone, level.fixedframetime);
        return;
    }

    vdestdelta   = tdest - self->GetLocalOrigin();
    angdestdelta = angdest - self->localangles;

    if (tdest == self->GetLocalOrigin()) {
        len = angdestdelta.length();
    } else {
        len = vdestdelta.length();
    }

    traveltime = len / tspeed;

    if (traveltime < level.fixedframetime) {
        traveltime   = level.fixedframetime;
        vdestdelta   = vec_zero;
        angdestdelta = vec_zero;
    }

    if (moveflags & MOVE_ORIGIN) {
        self->velocity = vdestdelta * (1.0f / traveltime);
    }

    if (moveflags & MOVE_ANGLES) {
        self->avelocity = angdestdelta * (1.0f / traveltime);
    }

    PostEvent(EV_MoveDone, traveltime);
}

void Mover::LinearInterpolate(const Vector& tdest, const Vector& angdest, float time, Event& event)
{
    Vector vdestdelta;
    Vector angdestdelta;
    float  t;

    if (endevent) {
        delete endevent;
    }
    endevent  = new Event(event);
    finaldest = tdest;
    angledest = angdest;

    CancelEventsOfType(EV_MoveDone);

    if (time < level.fixedframetime) {
        time = level.fixedframetime;
    }

    moveflags = 0;
    t         = 1.0f / time;
    if (finaldest != self->GetLocalOrigin()) {
        vdestdelta     = tdest - self->GetLocalOrigin();
        self->velocity = vdestdelta * t;
        moveflags |= MOVE_ORIGIN;
    }

    if (angledest != self->localangles) {
        angdestdelta    = angdest - self->localangles;
        self->avelocity = angdestdelta * t;
        moveflags |= MOVE_ANGLES;
    }

    PostEvent(EV_MoveDone, time);
}

void Mover::SetEndEvent(const int endEvent)
{
    if (endevent) {
        delete endevent;
    }

    endevent = new Event(endEvent);
}
