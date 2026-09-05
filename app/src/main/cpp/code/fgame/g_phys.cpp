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


#include "g_phys.h"
#include "animate.h"
#include "sentient.h"
#include "actor.h"
#include "player.h"
#include "vehicle.h"
#include "weaputils.h"
#include "mp_manager.hpp"
#include "debuglines.h"

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#include "../qcommon/qa_probe.h"


pushed_t  pushed[MAX_GENTITIES];
pushed_t *pushed_p;

Entity *obstacle;

Entity *G_FixEntityPosition(Entity *ent)
{
    int     mask;
    trace_t trace;
    Vector  start;

    mask = ent->edict->clipmask;
    if (!mask) {
        mask = MASK_SOLID;
    }

    start = ent->origin;
    start.z += 8.0f;
    if (ent->client) {
        trace = G_Trace(start, ent->mins, ent->maxs, ent->origin, ent, mask, true, "G_TestEntityPosition1");
    } else {
        trace = G_Trace(start, ent->mins, ent->maxs, ent->origin, ent, mask, false, "G_TestEntityPosition2");
    }

    if (trace.startsolid) {
        assert(trace.ent);
        assert(trace.ent->entity);
        return trace.ent->entity;
    }

    ent->setOrigin(trace.endpos);

    return NULL;
}

Entity *G_TestEntityPosition(Entity *ent)
{
    int     mask;
    trace_t trace;

    mask = ent->edict->clipmask;
    if (!mask) {
        mask = MASK_SOLID;
    }

    if (ent->client) {
        trace = G_Trace(ent->origin, ent->mins, ent->maxs, ent->origin, ent, mask, true, "G_TestEntityPosition1");
    } else {
        trace = G_Trace(ent->origin, ent->mins, ent->maxs, ent->origin, ent, mask, false, "G_TestEntityPosition2");
    }

    if (trace.startsolid) {
        assert(trace.ent);
        assert(trace.ent->entity);
        return trace.ent->entity;
    }

    return NULL;
}

#define UIVK_KB_FRAMES 40

static int uivk_kb_frame[MAX_GENTITIES];
static int uivk_kb_left[MAX_GENTITIES];

void UIVK_KB_Arm(int entnum)
{
    if (entnum < 0 || entnum >= MAX_GENTITIES) {
        return;
    }

    if (uivk_kb_frame[entnum] > level.framenum) {
        uivk_kb_left[entnum] = 0;
    }

    uivk_kb_frame[entnum] = level.framenum;
    uivk_kb_left[entnum]  = UIVK_KB_FRAMES;
}

void G_CheckVelocity(Entity *ent)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (ent->velocity[i] > sv_maxvelocity->value) {
            ent->velocity[i] = sv_maxvelocity->value;
        } else if (ent->velocity[i] < -sv_maxvelocity->value) {
            ent->velocity[i] = -sv_maxvelocity->value;
        }
    }
}

void G_Impact(Entity *e1, const trace_t *trace)
{
    gentity_t *e2;
    Event     *ev;

    e2 = trace->ent;

    level.impact_trace = *trace;

    if (e1->edict->solid != SOLID_NOT) {
        ev = new Event(EV_Touch);
        ev->AddEntity(e2->entity);
        e1->ProcessEvent(ev);
    }

    if (e2->entity && (e2->solid != SOLID_NOT) && (!(e2->contents & CONTENTS_SHOOTABLE_ONLY)) && (e2->entity != world)) {
        ev = new Event(EV_Touch);
        ev->AddEntity(e1);
        e2->entity->ProcessEvent(ev);
    }

    memset(&level.impact_trace, 0, sizeof(level.impact_trace));
}

#define STOP_EPSILON 0.1

int G_ClipVelocity(const Vector& in, const Vector& normal, Vector& out, float overbounce)
{
    int   i;
    int   blocked;
    float backoff;

    blocked = 0;

    if (normal[2] > 0) {
        blocked |= 1;
    }
    if (!normal[2]) {
        blocked |= 2;
    }

    backoff = (in * normal) * overbounce;

    out = in - normal * backoff;
    for (i = 0; i < 3; i++) {
        if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON) {
            out[i] = 0;
        }
    }

    return blocked;
}

#define MAX_CLIP_PLANES 5

int G_FlyMove(Entity *ent, const Vector &basevel, float time, int mask)
{
    Entity    *hit;
    gentity_t *edict;
    int        bumpcount, numbumps;
    Vector     dir;
    float      d;
    int        numplanes;
    vec3_t     planes[MAX_CLIP_PLANES];
    Vector     primal_velocity, original_velocity, new_velocity;
    int        i, j;
    trace_t    trace;
    Vector     end;
    float      time_left;
    int        blocked;

    edict = ent->edict;

    numbumps = 4;

    blocked           = 0;
    original_velocity = ent->velocity;
    primal_velocity   = ent->velocity;
    numplanes         = 0;

#if 1
    time_left = time;
#else
    time_left = 1.0; 

    v    = ent->total_delta;
    v[1] = -v[1]; 
    MatrixTransformVector(v, ent->orientation, move);
    move += ent->velocity * time;
    ent->total_delta = vec_zero;
#endif

    ent->groundentity = NULL;
    for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
#if 1
        end = ent->origin + time_left * (ent->velocity + basevel);
#else
        end = ent->origin + time_left * move;
#endif

        trace = G_Trace(ent->origin, ent->mins, ent->maxs, end, ent, mask, false, "G_FlyMove");


        if (trace.fraction > 0) {
            ent->setOrigin(trace.endpos);
            original_velocity = ent->velocity;
            numplanes         = 0;
        }

        if (trace.fraction == 1) {
            break;
        }

        assert(trace.ent);
        hit = trace.ent->entity;

        if (trace.plane.normal[2] > 0.7) {
            blocked |= 1;
            if (hit->getSolidType() == SOLID_BSP) {
                ent->groundentity   = hit->edict;
                ent->groundplane    = trace.plane;
                ent->groundcontents = trace.contents;
            }
        }

        if (!trace.plane.normal[2]) {
            blocked |= 2;
        }

        G_Impact(ent, &trace);
        if (!edict->inuse) {
            break; 
        }

        time_left -= time_left * trace.fraction;

        if (numplanes >= MAX_CLIP_PLANES) {
            ent->velocity = vec_zero;
            return 3;
        }

        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

        for (i = 0; i < numplanes; i++) {
            G_ClipVelocity(original_velocity, Vector(planes[i]), new_velocity, 1.01f);
            for (j = 0; j < numplanes; j++) {
                if (j != i) {
                    if ((new_velocity * planes[j]) < 0) {
                        break;
                    }
                }
            }

            if (j == numplanes) {
                break;
            }
        }

        if (i != numplanes) {
            ent->velocity = new_velocity;
        } else {
            if (numplanes != 2) {
                ent->velocity = vec_zero;
                return 7;
            }
            CrossProduct(planes[0], planes[1], dir);
            d             = dir * ent->velocity;
            ent->velocity = dir * d;
        }

        if ((ent->velocity * primal_velocity) <= 0) {
            ent->velocity = vec_zero;
            return blocked;
        }
    }

    return blocked;
}

void G_AddGravity(Entity *ent)
{
    float grav;

    if (ent->waterlevel > 2) {
        grav = ent->gravity * 60 * level.frametime;
    } else {
        grav = ent->gravity * sv_currentGravity->value * level.frametime;
    }

    if (ent->isSubclassOf(Vehicle)) {
        ent->velocity[2] -= grav + 500.0f;
    } else {
        ent->velocity[2] -= grav;
    }
}


trace_t G_PushEntity(Entity *ent, const Vector &push)
{
    trace_t    trace;
    Vector     start;
    Vector     end;
    int        mask;
    gentity_t *edict;

    start = ent->origin;
    end   = start + push;

retry:
    if (ent->edict->clipmask) {
        mask = ent->edict->clipmask;
    } else {
        mask = MASK_SOLID;
    }

    if (ent->usesFullTrace() && (!multiplayerManager.inMultiplayer() || multiplayerManager.fullCollision())) {
        trace = G_FullTrace(start, ent->mins, ent->maxs, end, ent, mask, false, "G_PushEntity");
    } else {
        trace = G_Trace(start, ent->mins, ent->maxs, end, ent, mask, false, "G_PushEntity");
    }

    edict = ent->edict;

    ent->setOrigin(trace.endpos);

    if (trace.fraction != 1.0f || (trace.startsolid && trace.ent && trace.ent->entity != world)) {
        G_Impact(ent, &trace);

        if ((!trace.ent || !trace.ent->inuse) && edict->inuse) {
            ent->setOrigin(start);
            goto retry;
        }
    }

    if (edict && (edict != ent->edict)) {
        if (ent->flags & FL_TOUCH_TRIGGERS) {
            G_TouchTriggers(ent);
        }
    }

    return trace;
}

trace_t G_SlideEntity(Entity *ent, const Vector &push)
{
    trace_t trace;
    Vector  start;
    Vector  end;
    int     mask;

    start = ent->origin;
    end   = start + push;

    if (ent->edict->clipmask) {
        mask = ent->edict->clipmask;
    } else {
        mask = MASK_SOLID;
    }

    trace = G_Trace(start, ent->mins, ent->maxs, end, ent, mask, false, "G_SlideEntity");

    ent->setOrigin(trace.endpos);

    return trace;
}


qboolean G_Push(Entity *pusher, const Vector &pushermove, const Vector &pusheramove)
{
    Entity    *check, *block;
    gentity_t *edict;
    Vector     move, amove;
    Vector     mins, maxs;
    Vector     save;
    pushed_t  *p;
    Vector     org, org2, move2;
    Vector     norm;
    float      mat[3][3];
    pushed_t  *pusher_p;
    float      radius;
    int        i, num;
    int        touch[MAX_GENTITIES];
    gentity_t *next;

    pusher_p               = pushed_p;
    pushed_p->ent          = pusher;
    pushed_p->_localOrigin = pusher->GetLocalOrigin();
    pushed_p->origin       = pusher->origin;
    pushed_p->localangles  = pusher->localangles;
    pushed_p->angles       = pusher->angles;

    if (pusher->client) {
        pushed_p->deltayaw = pusher->client->ps.delta_angles[YAW];
    }

    pushed_p++;

    if (pushed_p >= &pushed[MAX_GENTITIES]) {
        gi.Error(ERR_FATAL, "Pushed too many entities.");
    }

    pusher->addAngles(pusheramove);
    pusher->addOrigin(pushermove);

    if (pusher->edict->solid == SOLID_NOT) {
        return true;
    }

    move  = pusher->origin - pusher_p->origin;
    amove = pusher->angles - pusher_p->angles;

    AnglesToAxis(amove, mat);

    mins = pusher->absmin;
    maxs = pusher->absmax;


    num = gi.AreaEntities(mins, maxs, touch, MAX_GENTITIES, qfalse);


    for (edict = active_edicts.next; edict != &active_edicts; edict = next) {
        assert(edict);
        assert(edict->inuse);
        assert(edict->entity);

        next  = edict->next;
        check = edict->entity;

        if (check->groundentity == pusher->edict) {

            for (i = 0; i < num; i++) {
                if (touch[i] == check->edict - g_entities) {
                    break;
                }
            }

            if (i == num) {
                touch[num] = check->edict - g_entities;
                num++;
            }
        }
    }

    for (i = 0; i < num; i++) {
        edict = &g_entities[touch[i]];
        assert(edict);
        assert(edict->inuse);
        assert(edict->entity);
        check = edict->entity;

        if ((check->movetype == MOVETYPE_PUSH) || (check->movetype == MOVETYPE_STOP)
            || (check->movetype == MOVETYPE_NONE) || (check->movetype == MOVETYPE_NOCLIP)) {
            continue;
        }

        if ((check->edict->contents == CONTENTS_SHOOTABLE_ONLY) || (check->edict->contents == CONTENTS_CORPSE)) {
            continue;
        }

        if (check->groundentity != pusher->edict) {
            if ((check->edict->solid == SOLID_TRIGGER) || (check->edict->solid == SOLID_NOT)) {
                continue;
            }

            if ((check->absmin[0] >= maxs[0]) || (check->absmin[1] >= maxs[1]) || (check->absmin[2] >= maxs[2])
                || (check->absmax[0] <= mins[0]) || (check->absmax[1] <= mins[1]) || (check->absmax[2] <= mins[2])) {
                continue;
            }

            if (!G_TestEntityPosition(check)) {
                continue;
            }
        }

        if ((pusher->movetype == MOVETYPE_PUSH) || (check->groundentity == pusher->edict)) {
            pushed_p->_localOrigin = check->GetLocalOrigin();
            pushed_p->localangles  = check->localangles;
            pushed_p->ent    = check;
            pushed_p->origin = check->origin;
            pushed_p->angles = check->angles;
            pushed_p++;

            if (pushed_p >= &pushed[MAX_GENTITIES]) {
                gi.Error(ERR_FATAL, "Pushed too many entities.");
            }

            save = check->GetLocalOrigin();

            move2 = move;

            if (check->client) {
                check->client->ps.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
            }

            if (check->size.x > check->size.z) {
                radius = check->size.z * 0.5f;
            } else {
                radius = check->size.x * 0.5f;
            }

            org = check->origin - pusher->origin;
            org.z += radius;

            MatrixTransformVector(org, mat, org2);

            move2 += org2 - org;

            check->addOrigin(check->getParentVector(move2));

            if (check->groundentity != pusher->edict) {
                check->groundentity = NULL;
            }

            block = G_TestEntityPosition(check);
            if (block) {
                block = G_FixEntityPosition(check);
            }
            if (!block) {
                check->link();

                continue;
            }


            check->setOrigin(save);
            block = G_TestEntityPosition(check);
            if (!block) {
                pushed_p--;
                continue;
            }
        }

        if (check->edict->solid == SOLID_NOT) {
            continue;
        }

        obstacle = check;

        for (p = pushed_p - 1; p >= pushed; p--) {
            p->ent->angles = p->angles;
            p->ent->origin = p->origin;

            p->ent->localangles = p->localangles;
            p->ent->SetLocalOrigin(p->_localOrigin);

            if (p->ent->client) {
                p->ent->client->ps.delta_angles[YAW] = (int)p->deltayaw;
            }
        }

        for (p = pushed; p < pushed_p; p++) {
            p->ent->setAngles();
            p->ent->setOrigin();
        }

        return false;
    }

    for (p = pushed_p - 1; p >= pushed; p--) {
        if (p->ent->flags & FL_TOUCH_TRIGGERS) {
            G_TouchTriggers(p->ent);
        }
    }

    return true;
}

qboolean G_PushMove(Entity *ent, const Vector &move, const Vector &amove)
{
    Entity *part;
    Vector  m, a;
    Event  *ev;

    m = move;
    a = amove;

    pushed_p = pushed;

    part = ent;

    while (part) {
        if (!G_Push(part, m, a)) {
            ev = new Event(EV_Blocked);
            ev->AddEntity(obstacle);
            part->ProcessEvent(ev);
            return false;
        }

        m = vec_zero;
        a = vec_zero;

        if (part->bind_info) {
            part = part->bind_info->teamchain;
        } else {
            part = NULL;
        }
    }

    return true;
}

void G_Physics_Pusher(Entity *ent)
{
    Vector  move, amove;
    Entity *part, *mv;
    Event  *ev;

    if (ent->flags & FL_TEAMSLAVE) {
        return;
    }

    part = ent;

    while (part) {
        if (part->velocity != vec_zero || part->avelocity != vec_zero) {
            break;
        }

        if (part->bind_info) {
            part = part->bind_info->teamchain;
        } else {
            part = NULL;
        }
    }

    pushed_p = pushed;
    while (part) {
        move  = part->velocity * level.frametime;
        amove = part->avelocity * level.frametime;

        if (!G_Push(part, move, amove)) {
            break;
        }

        if (part->bind_info) {
            part = part->bind_info->teamchain;
        } else {
            part = NULL;
        }
    }

    if (part) {
        mv = ent;

        while (mv) {
            mv->PostponeEvent(EV_MoveDone, FRAMETIME);

            if (mv->bind_info) {
                mv = mv->bind_info->teamchain;
            } else {
                mv = NULL;
            }
        }

        ev = new Event(EV_Blocked);
        ev->AddEntity(obstacle);
        part->ProcessEvent(ev);
    }

}


void G_Physics_Noclip(Entity *ent)
{
    ent->angles += ent->avelocity * level.frametime;
    ent->origin += ent->velocity * level.frametime;
    ent->link();
}


void G_Physics_Toss(Entity *ent)
{
    trace_t    trace;
    Vector     move;
    float      backoff;
    Entity    *slave;
    qboolean   wasinwater;
    qboolean   isinwater;
    Vector     origin2;
    Vector     basevel;
    gentity_t *edict;
    qboolean   onconveyor;

    if (ent->flags & FL_TEAMSLAVE) {
        return;
    }

    if (ent->velocity[2] > 0) {
        ent->groundentity = NULL;
    }

    if (ent->groundentity && !ent->groundentity->inuse) {
        ent->groundentity = NULL;
    }

    // NOTE: basevel is never assigned, here or in retail (SDK g_phys.cpp:1092) — conveyor
    onconveyor = (basevel != vec_zero);

    // SDK g_phys.cpp:1116 — retail does NOT require the groundentity to be MOVETYPE_NONE.
    if (ent->groundentity && !onconveyor && (ent->movetype != MOVETYPE_VEHICLE)) {
        if (ent->avelocity.length()) {
            ent->setAngles(ent->angles + ent->avelocity * level.frametime);
        }
        ent->velocity = vec_zero;
        return;
    }

    origin2 = ent->origin;


    if (!onconveyor && (ent->movetype != MOVETYPE_FLY) && (ent->movetype != MOVETYPE_FLYMISSILE)) {
        if (!(ent->flags & FL_FLY)) {
            G_AddGravity(ent);
        }
    }

    ent->setAngles(ent->angles + (ent->avelocity * level.frametime));

    move = (ent->velocity + basevel) * level.frametime;

    edict = ent->edict;
    if (ent->movetype == MOVETYPE_VEHICLE) {
        int mask;

        if (ent->edict->clipmask) {
            mask = ent->edict->clipmask;
        } else {
            mask = MASK_MONSTERSOLID;
        }
        G_FlyMove(ent, basevel, FRAMETIME, mask);
        if (ent->flags & FL_TOUCH_TRIGGERS) {
            G_TouchTriggers(ent);
        }
        return;
    } else {
        trace = G_PushEntity(ent, move);
    }

    if ((trace.fraction == 0.0f) && (ent->movetype == MOVETYPE_SLIDE)) {
        Vector slide;

        slide[0] = move[0] * 0.7f;
        slide[1] = move[1] * 0.7f;
        slide[2] = 0;

        G_PushEntity(ent, slide);
    }

    if (!edict->inuse) {
        return;
    }

    if (trace.fraction < 1.0f) {
        if ((ent->movetype == MOVETYPE_BOUNCE) || (ent->movetype == MOVETYPE_GIB)) {
            backoff = 1.5f;
        } else {
            backoff = 1;
        }

        if (ent->movetype != MOVETYPE_NONE) {
            G_ClipVelocity(ent->velocity, Vector(trace.plane.normal), ent->velocity, backoff);
        }

        if (trace.plane.normal[2] > 0.7f) {
            if (((ent->velocity.length() < 10.0f)
                 || ((ent->movetype != MOVETYPE_BOUNCE) && (ent->movetype != MOVETYPE_GIB)))
                && (ent->movetype != MOVETYPE_SLIDE)) {
                ent->groundentity   = trace.ent;
                ent->groundplane    = trace.plane;
                ent->groundcontents = trace.contents;
                ent->velocity       = vec_zero;
                ent->avelocity      = vec_zero;
                ent->ProcessEvent(EV_Stop);
            } else if (ent->movetype == MOVETYPE_GIB) {
                ent->avelocity = vec_zero;
            }
        }
    }

    if ((move[2] == 0) && onconveyor) {
        ent->CheckGround();
    }

    wasinwater     = (ent->watertype & MASK_WATER);
    ent->watertype = gi.pointcontents(ent->origin, 0);
    isinwater      = ent->watertype & MASK_WATER;

    if (isinwater) {
        ent->waterlevel = 1;
    } else {
        ent->waterlevel = 0;
    }

    if ((edict->spawntime < (level.time - FRAMETIME)) && (ent->mass > 0)) {
        if (!wasinwater && isinwater) {
            ent->Sound("impact_watersplash", CHAN_BODY, DEFAULT_VOL, DEFAULT_MIN_DIST, &origin2);
        } else if (wasinwater && !isinwater) {
            ent->Sound("impact_leavewater", CHAN_BODY, DEFAULT_VOL, DEFAULT_MIN_DIST, &origin2);
        }
    }

    if (ent->bind_info) {
        for (slave = ent->bind_info->teamchain; slave; slave = slave->bind_info->teamchain) {
            slave->setOrigin(slave->GetLocalOrigin());  
            slave->setAngles(slave->localangles);
        }
    }

    if (ent->flags & FL_TOUCH_TRIGGERS) {
        G_TouchTriggers(ent);
    }

}


void G_AddRotationalFriction(Entity *ent)
{
    int   n;
    float adjustment;

    ent->angles += level.frametime * ent->avelocity;
    adjustment = level.frametime * sv_stopspeed->value * sv_friction->value;
    for (n = 0; n < 3; n++) {
        if (ent->avelocity[n] > 0) {
            ent->avelocity[n] -= adjustment;
            if (ent->avelocity[n] < 0) {
                ent->avelocity[n] = 0;
            }
        } else {
            ent->avelocity[n] += adjustment;
            if (ent->avelocity[n] > 0) {
                ent->avelocity[n] = 0;
            }
        }
    }
}


void G_CheckWater(Entity *ent)
{
    if (ent->isSubclassOf(Actor)) {
        ((Actor *)ent)->movementSubsystem->CheckWater();
    } else {
        ent->watertype = gi.pointcontents(ent->origin, 0);
        if (ent->watertype & MASK_WATER) {
            ent->waterlevel = 1;
        } else {
            ent->waterlevel = 0;
        }
    }
}


void G_Physics_Step(Entity *ent)
{
    qboolean  wasonground;
    qboolean  hitsound = false;
    Vector    vel;
    float     speed, newspeed, control;
    float     friction;
    int       mask;
    Vector    basevel;

    {
        if (ent->entnum >= 0 && ent->entnum < MAX_GENTITIES) {
            if (uivk_kb_frame[ent->entnum] > level.framenum) {
                uivk_kb_left[ent->entnum] = 0;
            }
            uivk_kb_frame[ent->entnum] = level.framenum;

            if (uivk_kb_left[ent->entnum] > 0 && getenv("EF2_KNOCKBACK") && PROBE_On()) {
                int uivk_kb_ge = -1;

                if (ent->groundentity) {
                    uivk_kb_ge = ent->groundentity - g_entities;
                }

                gi.Printf("[UIVK:kbflight] "
                          "ent=%d f=%d ge=%d vel=(%.1f,%.1f,%.1f) spd=%.1f "
                          "org=(%.1f,%.1f,%.1f) mv=%d df=%d mb=%d svtime=%d\n",
                          ent->entnum,
                          UIVK_KB_FRAMES - uivk_kb_left[ent->entnum] + 1,
                          uivk_kb_ge,
                          ent->velocity[0], ent->velocity[1], ent->velocity[2],
                          ent->velocity.length(),
                          ent->origin[0], ent->origin[1], ent->origin[2],
                          (int)ent->movetype,
                          (int)ent->deadflag,
                          (int)M_CheckBottom(ent),
                          level.inttime);

                uivk_kb_left[ent->entnum]--;
            }
        }
    }


    if (!ent->groundentity) {
        ent->CheckGround();
    }

    if (ent->groundentity) {
        wasonground = true;
        ent->velocity[2] = 0.0;
    } else {
        wasonground = false;
    }

    G_CheckVelocity(ent);

    if (ent->avelocity != vec_zero) {
        G_AddRotationalFriction(ent);
    }

    if (!wasonground) {
        if (!(ent->flags & FL_FLY)) {
            if (!((ent->flags & FL_SWIM) && (ent->waterlevel > 2))) {
                if (ent->velocity[2] < (sv_currentGravity->value * ent->gravity * -0.1f)) {
                    hitsound = true;
                }

                G_AddGravity(ent);
            }
        }
    }

    if ((ent->flags & FL_FLY) && (ent->velocity.z != 0)) {
        speed    = fabs(ent->velocity.z);
        control  = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        friction = sv_friction->value / 3;
        newspeed = speed - (level.frametime * control * friction);
        if (newspeed < 0) {
            newspeed = 0;
        }
        newspeed /= speed;
        ent->velocity.z *= newspeed;
    }

    if ((ent->flags & FL_SWIM) && (ent->velocity.z != 0)) {
        speed    = fabs(ent->velocity.z);
        control  = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
        newspeed = speed - (level.frametime * control * sv_waterfriction->value * ent->waterlevel);
        if (newspeed < 0) {
            newspeed = 0;
        }
        newspeed /= speed;
        ent->velocity.z *= newspeed;
    }

    if (ent->velocity != vec_zero) {
        if ((wasonground) || (ent->flags & (FL_SWIM | FL_FLY))) {
            if (!(ent->health <= 0.0 && !M_CheckBottom(ent))) {
                vel   = ent->velocity;
                vel.z = 0;
                speed = vel.length();
                if (speed) {
                    friction = sv_friction->value;

                    control  = speed < sv_stopspeed->value ? sv_stopspeed->value : speed;
                    newspeed = speed - level.frametime * control * friction;

                    if (newspeed < 0) {
                        newspeed = 0;
                    }

                    newspeed /= speed;

                    ent->velocity.x *= newspeed;
                    ent->velocity.y *= newspeed;
                }
            }
        }
    }

    if ((basevel != vec_zero) || (ent->velocity != vec_zero) || (ent->total_delta != vec_zero)) {
        if (ent->edict->svflags & SVF_MONSTER) {
            mask = ent->edict->clipmask;
        } else {
            mask = MASK_SOLID;
        }

        G_FlyMove(ent, basevel, level.frametime, mask);

        ent->link();

        G_CheckWater(ent);
        if (ent->flags & FL_TOUCH_TRIGGERS) {
            G_TouchTriggers(ent);
        }

        if (ent->groundentity && !wasonground && hitsound) {
            ent->Sound("impact_softland", CHAN_BODY, 0.5f);
        }
    }
}


Trajectory::Trajectory( const Vector &launchPoint, const Vector &targetPoint, const Angle launchAngle, const float gravity ):
	_launchPoint( launchPoint ),
	_targetPoint( targetPoint ),
	_launchAngle( launchAngle ),
	_travelTime( -1.0f ),
	_initialVelocity( Vector::Identity() ),
	_gravity( gravity )
{
	assert( gravity <= 0.0f );
	const Vector direction( targetPoint - launchPoint );
	const float horizontalDistance = direction.lengthXY();
	const float verticalDistance = -direction.z;
	const float cosineOfLaunchAngle = cos( DEG2RAD( _launchAngle ) );
	const float sineOfLaunchAngle = sin( DEG2RAD( _launchAngle ) );

	const float travelTimeSquared = ( ( 2.0f * horizontalDistance * sineOfLaunchAngle) / ( -gravity * cosineOfLaunchAngle) ) +
										( 2.0f * verticalDistance / -gravity);

	if ( travelTimeSquared > 0.0f )
	{
		_travelTime = sqrt( travelTimeSquared );
		_initialVelocity = direction;
		_initialVelocity.z = 0.0f;
		_initialVelocity.normalize();
		_initialVelocity *= cosineOfLaunchAngle;
		_initialVelocity.z = sineOfLaunchAngle;
		_initialVelocity.normalize();

		const float initialSpeed = horizontalDistance / ( cosineOfLaunchAngle * _travelTime );
		_initialVelocity *= initialSpeed;
	}
	else
	{
		gi.WPrintf( "Failed Trajectory: LaunchPoint<%f,%f,%f>, Target Point<%f,%f,%f>, LaunchAngle<%f>\n",
			launchPoint.x, launchPoint.y, launchPoint.z,
			targetPoint.x, targetPoint.y, targetPoint.z,
			launchAngle.GetValue()
			);
	}
}

Trajectory::Trajectory( const Vector &launchPoint, const Vector &targetPoint, const float initialSpeed, const float gravity, const bool useHighTrajectory ):
	_launchPoint( launchPoint ),
	_targetPoint( targetPoint ),
	_travelTime( -1.0f ),
	_initialVelocity( Vector::Identity() ),
	_gravity( gravity )
{
	const Vector direction( targetPoint - launchPoint );
	
	Vector directionXY( direction );
	directionXY.z = 0.0f;
	directionXY.normalize();

	const double X = direction.lengthXY();
	const double Y = direction.z;
	const double R = initialSpeed;
	assert( gravity <= 0.0f );
	const double a = gravity; 

	const double X2 = X * X;
	const double Y2 = Y * Y;
	const double R2 = R * R;
	const double a2 = a * a;

	const double R4 = R2 * R2;
	const double Ya = Y * a;

	const double denominator = 2.0 * ( 1.0 + (Y2 / X2) );

	const bool test = (2*R2*Ya - X2*a2 > R2);
	const double radical = R4 + 2*R2*Ya - X2*a2;

	const double numerator1 = R2 + Ya + sqrt( radical );
	const double numerator2 = R2 + Ya - sqrt( radical );

	const double Vx2_1 = numerator1 / denominator;
	const double Vx2_2 = numerator2 / denominator;

	const double Vx2 = useHighTrajectory ? Vx2_2 : Vx2_1; 

	const double Vx = sqrt( Vx2 );
	double Vy = sqrt( R2 - Vx2 );
	if ( test && ! useHighTrajectory )
	{
		Vy *= -1.0f;
	}
	const double launchRadians = atan2( Vy, Vx );
	const double launchQuakePitchDegrees = -RAD2DEG( launchRadians );

	const double travelTime = X / Vx;
	if( travelTime < 0.0 )
		return;

	_launchAngle = launchQuakePitchDegrees;
	_initialVelocity = (directionXY * Vx) + Vector( 0.0f, 0.0f, Vy );
	_initialVelocity.normalize();
	_initialVelocity *= initialSpeed;
	_travelTime = travelTime;
}



static int g_enttrash_movetype[MAX_GENTITIES];
static int g_enttrash_frame[MAX_GENTITIES];
static unsigned g_enttrash_scale[MAX_GENTITIES];

static qboolean G_EntTrashProbeOn( void ) {
	static cvar_t *enttrash_probe;

	if ( !enttrash_probe ) {
		enttrash_probe = gi.cvar( "qa_probe", "1", 0 );
	}
	return enttrash_probe->integer ? qtrue : qfalse;
}

static void G_EntTrash_Record( Entity *ent, int movetype )
{
	gentity_t *edict = ent->edict;
	int        entnum = ent->entnum;
	int        previous;

	if ( entnum < 0 || entnum >= MAX_GENTITIES ) {
		return;
	}

	if ( g_enttrash_frame[ entnum ] > level.framenum ) {
		g_enttrash_movetype[ entnum ] = -2;
	}

	{
		static cvar_t *enttrash_probe;

		if ( !enttrash_probe ) {
			enttrash_probe = gi.cvar( "qa_probe", "1", 0 );
		}
		if ( !enttrash_probe->integer ) {
			return;
		}
	}

	previous = g_enttrash_movetype[ entnum ];
	g_enttrash_movetype[ entnum ] = movetype;
	g_enttrash_frame[ entnum ] = level.framenum;

	if ( G_EntTrashProbeOn() ) {
	gi.Printf( "[UIVK:enttrash] movetype ent=%d class='%s' old=%d new=0x%08x(%d)"
			   " size=%08x,%08x,%08x mass=0x%08x gravity=0x%08x"
			   " inuse=%d linked=%d freetime=%.3f spawntime=%.3f"
			   " frame=%d ltime=%.3f\n",
			   entnum,
			   ent->getClassname() ? ent->getClassname() : "?",
			   previous,
			   (unsigned)movetype, movetype,
			   *(unsigned *)&ent->size.x, *(unsigned *)&ent->size.y,
			   *(unsigned *)&ent->size.z,
			   *(unsigned *)&ent->mass, *(unsigned *)&ent->gravity,
			   edict ? (int)edict->inuse : -1,
			   edict ? (int)edict->linked : -1,
			   edict ? edict->freetime : -1.0f,
			   edict ? edict->spawntime : -1.0f,
			   level.framenum,
			   level.time );
	}
}

static void G_EntTrash_Scale( Entity *ent )
{
	gentity_t *edict = ent->edict;
	unsigned   bits;
	unsigned   expo;
	int        entnum;

	if ( !edict ) {
		return;
	}

	entnum = ent->entnum;
	if ( entnum < 0 || entnum >= MAX_GENTITIES ) {
		return;
	}

	bits = *(unsigned *)&edict->s.scale;
	expo = ( bits >> 23 ) & 0xFF;

	if ( expo != 0 && expo < 0x90 ) {
		g_enttrash_scale[ entnum ] = bits;
		return;
	}

	if ( g_enttrash_scale[ entnum ] == bits ) {
		return; 
	}

	g_enttrash_scale[ entnum ] = bits;

	if ( G_EntTrashProbeOn() ) {
	gi.Printf( "[UIVK:enttrash] scale ent=%d class='%s' scale=0x%08x alpha=0x%08x"
			   " renderfx=0x%08x solid=%d modelindex=%d parent=%d"
			   " frame=%d ltime=%.3f\n",
			   entnum,
			   ent->getClassname() ? ent->getClassname() : "?",
			   bits,
			   *(unsigned *)&edict->s.alpha,
			   edict->s.renderfx,
			   edict->s.solid,
			   edict->s.modelindex,
			   edict->s.parent,
			   level.framenum,
			   level.time );
	}
}


static void     *g_enttrash_ph_last;
static int       g_enttrash_ph_lastframe;
static qboolean  g_enttrash_ph_valid;
static int       g_enttrash_mt_last[MAX_GENTITIES];
static int       g_enttrash_mt_frame[MAX_GENTITIES];
static qboolean  g_enttrash_mt_valid[MAX_GENTITIES];

void G_EntTrash_Pointer(const char *where, Entity *player, void *pheur)
{
	int entnum;
	int flip;

	if (!UIVK_EntTrashOn()) {
		return;
	}

	entnum = player ? player->entnum : -1;

	if (g_enttrash_ph_lastframe > level.framenum) {
		g_enttrash_ph_valid = qfalse;
		g_enttrash_ph_last  = NULL;
	}

	flip = (g_enttrash_ph_valid && pheur != g_enttrash_ph_last) ? 1 : 0;

	g_enttrash_ph_last      = pheur;
	g_enttrash_ph_lastframe = level.framenum;
	g_enttrash_ph_valid     = qtrue;

	gi.Printf("[UIVK:enttrash] %s ent=%d class='%s' p_heuristics=%p movetype=%d%s"
			  " frame=%d ltime=%.3f\n",
			  where,
			  entnum,
			  (player && player->getClassname()) ? player->getClassname() : "?",
			  pheur,
			  player ? player->movetype : -1,
			  flip ? " FLIP" : "",
			  level.framenum,
			  level.time);
}

void G_EntTrash_KilledEntry(Entity *victim, Entity *attacker, float damage)
{
	if (!UIVK_EntTrashOn()) {
		return;
	}

	gi.Printf("[UIVK:enttrash] Killed ent=%d class='%s' attacker=%p aent=%d"
			  " damage=%.1f frame=%d ltime=%.3f\n",
			  victim ? victim->entnum : -1,
			  (victim && victim->getClassname()) ? victim->getClassname() : "?",
			  (void *)attacker,
			  attacker ? attacker->entnum : -1,
			  damage,
			  level.framenum,
			  level.time);
}

void G_EntTrash_Tick(void)
{
	gentity_t *ed;
	Entity    *ent;
	Player    *player;
	int        i;
	int        previous;

	if (!UIVK_EntTrashOn()) {
		return;
	}

	if ((level.framenum % 60) == 0) {
		player = GetPlayer(0);
		G_EntTrash_Pointer("tick", player ? (Entity *)player : NULL, player ? (void *)player->p_heuristics : NULL);
	}

	if ((level.framenum % 300) != 0) {
		return;
	}

	for (i = 0; i < globals.num_entities; i++) {
		ed = &g_entities[i];

		if (!ed->inuse || !ed->entity) {
			continue;
		}

		ent = ed->entity;

		if (i >= MAX_GENTITIES) {
			continue;
		}

		if (g_enttrash_mt_frame[i] > level.framenum) {
			g_enttrash_mt_valid[i] = qfalse;
			g_enttrash_mt_last[i]  = -2;
		}

		previous = g_enttrash_mt_last[i];
		g_enttrash_mt_last[i]  = ent->movetype;
		g_enttrash_mt_frame[i] = level.framenum;
		g_enttrash_mt_valid[i] = qtrue;

		gi.Printf("[UIVK:enttrash] sweep ent=%d class='%s' movetype=%d old=%d%s"
				  " health=%.1f deadflag=%d frame=%d ltime=%.3f\n",
				  i,
				  ent->getClassname() ? ent->getClassname() : "?",
				  ent->movetype,
				  previous,
				  (g_enttrash_mt_valid[i] && previous != ent->movetype) ? " CHANGED" : "",
				  ent->health,
				  ent->deadflag,
				  level.framenum,
				  level.time);
	}
}

void G_RunEntity(Entity *ent)
{
    gentity_t *edict;

    edict = ent->edict;

    if (!edict->inuse) {
        return;
    }

    if (getenv("EF2_DEADWEDGE") && gi.cvar("qa_probe", "1", 0)->integer) {
        static int  uivk_nf_reported[MAX_GENTITIES];
        static vec3_t uivk_nf_lastorg[MAX_GENTITIES];
        static vec3_t uivk_nf_lastvel[MAX_GENTITIES];
        int         uivk_nf_e = edict - g_entities;

        if (uivk_nf_e >= 0 && uivk_nf_e < MAX_GENTITIES) {
            int uivk_nf_bad = 0;

            for (int uivk_nf_i = 0; uivk_nf_i < 3; uivk_nf_i++) {
                if (!(ent->origin[uivk_nf_i] == ent->origin[uivk_nf_i])
                    || (ent->origin[uivk_nf_i] > 1e12f) || (ent->origin[uivk_nf_i] < -1e12f)) {
                    uivk_nf_bad |= 1;
                }
                if (!(ent->velocity[uivk_nf_i] == ent->velocity[uivk_nf_i])
                    || (ent->velocity[uivk_nf_i] > 1e12f) || (ent->velocity[uivk_nf_i] < -1e12f)) {
                    uivk_nf_bad |= 2;
                }
            }

            if (uivk_nf_bad) {
                if (!uivk_nf_reported[uivk_nf_e]) {
                    uivk_nf_reported[uivk_nf_e] = 1;
                    gi.Printf("[UIVK:nanfirst] ent=%d cls='%s' m='%s' bad=%d svtime=%d "
                              "mv=%d ge=%d fl=%d scale=%.6g mass=%d "
                              "org=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) "
                              "lastorg=(%.1f,%.1f,%.1f) lastvel=(%.1f,%.1f,%.1f) "
                              "td=(%.2f,%.2f,%.2f) df=%d\n",
                              uivk_nf_e,
                              ent->getClassname(),
                              ent->model.c_str(),
                              uivk_nf_bad,
                              level.inttime,
                              (int)ent->movetype,
                              ent->groundentity ? (int)(ent->groundentity - g_entities) : -1,
                              (int)ent->flags,
                              edict->s.scale,
                              ent->mass,
                              ent->origin[0], ent->origin[1], ent->origin[2],
                              ent->velocity[0], ent->velocity[1], ent->velocity[2],
                              uivk_nf_lastorg[uivk_nf_e][0],
                              uivk_nf_lastorg[uivk_nf_e][1],
                              uivk_nf_lastorg[uivk_nf_e][2],
                              uivk_nf_lastvel[uivk_nf_e][0],
                              uivk_nf_lastvel[uivk_nf_e][1],
                              uivk_nf_lastvel[uivk_nf_e][2],
                              ent->total_delta[0], ent->total_delta[1], ent->total_delta[2],
                              (int)ent->deadflag);
                }
            } else {
                for (int uivk_nf_i = 0; uivk_nf_i < 3; uivk_nf_i++) {
                    uivk_nf_lastorg[uivk_nf_e][uivk_nf_i] = ent->origin[uivk_nf_i];
                    uivk_nf_lastvel[uivk_nf_e][uivk_nf_i] = ent->velocity[uivk_nf_i];
                }
            }
        }
    }

    if (ent->isThinkOn()) {
        ent->Think();
    }


    if ((edict->s.parent == ENTITYNUM_NONE) && !(ent->flags & FL_IMMOBILE) && !(ent->flags & FL_PARTIAL_IMMOBILE)) {
        switch ((int)ent->movetype) {
        case MOVETYPE_PUSH:
        case MOVETYPE_STOP:
            G_Physics_Pusher(ent);
            break;
        case MOVETYPE_NONE:
        case MOVETYPE_STATIONARY:
        case MOVETYPE_WALK:
            break;
        case MOVETYPE_NOCLIP:
            G_Physics_Noclip(ent);
            break;
        case MOVETYPE_STEP:
            G_Physics_Step(ent);
            break;
        case MOVETYPE_TOSS:
        case MOVETYPE_BOUNCE:
        case MOVETYPE_GIB:
        case MOVETYPE_FLY:
        case MOVETYPE_FLYMISSILE:
        case MOVETYPE_SLIDE:
        case MOVETYPE_VEHICLE:
            G_Physics_Toss(ent);
            break;
        default:
            if (UIVK_EntTrashOn()) {
                G_EntTrash_Record(ent, (int)ent->movetype);
            }
            gi.Error(ERR_DROP, "G_Physics: bad movetype %i", (int)ent->movetype);
        }
    }

    if (UIVK_EntTrashOn()) {
        G_EntTrash_Scale(ent);
    }

    if (ent->flags & FL_POSTTHINK) {
        ent->Postthink();
    }
}
