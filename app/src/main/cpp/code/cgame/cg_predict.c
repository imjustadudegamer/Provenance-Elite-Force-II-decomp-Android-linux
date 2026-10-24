/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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

#include "cg_local.h"

#define PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_probe.h"

static pmove_t cg_pmove;

/* the server owns this one; the client prediction must agree with it */
static cvar_t *cg_easyJump;
static int     cg_easyJumpLast = -1;

static int        cg_numSolidEntities;
static centity_t *cg_solidEntities[MAX_ENTITIES_IN_SNAPSHOT];
static int        cg_numTriggerEntities;

/* the temp box must carry CONTENTS_BODY: the trace mask never asks for less */
#define CG_CLIP_BOX_CONTENTS CONTENTS_BODY

static qboolean CG_SegmentCrossesBox(
    const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs
)
{
    vec3_t dir;
    float  tmin = 0.0f;
    float  tmax = 1.0f;
    int    i;

    for (i = 0; i < 3; i++) {
        dir[i] = end[i] - start[i];

        if (dir[i] > -1e-8f && dir[i] < 1e-8f) {
            if (start[i] < mins[i] || start[i] > maxs[i]) {
                return qfalse;
            }
        } else {
            float inv = 1.0f / dir[i];
            float t1 = (mins[i] - start[i]) * inv;
            float t2 = (maxs[i] - start[i]) * inv;

            if (t1 > t2) {
                float t = t1;
                t1 = t2;
                t2 = t;
            }

            if (t1 > tmin) {
                tmin = t1;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax) {
                return qfalse;
            }
        }
    }

    return qtrue;
}

void CG_BuildSolidList(void)
{
    int            i;
    centity_t     *cent;
    snapshot_t    *snap;
    entityState_t *ent;

    cg_numSolidEntities   = 0;
    cg_numTriggerEntities = 0;

    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        snap = cg.nextSnap;
    } else {
        snap = cg.snap;
    }

    for (i = 0; i < snap->numEntities; i++) {
        cent = &cg_entities[snap->entities[i].number];
        ent  = &cent->currentState;

        if (ent->eType == ET_ITEM || ent->eType == ET_PUSH_TRIGGER || ent->eType == ET_TELEPORT_TRIGGER) {
            continue;
        }

        if (cent->nextState.solid) {
            cg_solidEntities[cg_numSolidEntities] = cent;
            cg_numSolidEntities++;
            continue;
        }
    }
}


void CG_ClipMoveToEntities(
    const vec3_t start,
    const vec3_t mins,
    const vec3_t maxs,
    const vec3_t end,
    int          skipNumber,
    int          mask,
    trace_t     *tr,
    qboolean     cylinder
)
{
    int            i;
    int            boxContents;
    trace_t        trace;
    entityState_t *ent;
    clipHandle_t   cmodel;
    vec3_t         bmins, bmaxs;
    vec3_t         origin, angles;
    centity_t     *cent;

    for (i = 0; i < cg_numSolidEntities; i++) {
        cent = cg_solidEntities[i];
        ent  = &cent->currentState;

        if (ent->number == skipNumber) {
            continue;
        }

        if (ent->solid == SOLID_BMODEL) {
            cmodel = cgi.CM_InlineModel(ent->modelindex);
            if (!cmodel) {
                continue;
            }
        } else {
            IntegerToBoundingBox(ent->solid, bmins, bmaxs);
            boxContents = CG_CLIP_BOX_CONTENTS;
            cmodel      = cgi.CM_TempBoxModel(bmins, bmaxs, boxContents);

            /* Anomaly only: the caller asked for body hits and this temp box
               cannot answer them, so every actor standing on the segment is
               invisible to the trace and the impact or beam lands behind it.
               With CG_CLIP_BOX_CONTENTS == CONTENTS_BODY the second test is
               never true, so the row stays silent on a correct build. */
            if (PROBE_On() && (mask & CONTENTS_BODY) && !(boxContents & CONTENTS_BODY)
                && CG_SegmentCrossesBox(start, end, bmins, bmaxs)) {
                static int clip_xmask_n;

                if (PROBE_Throttle(&clip_xmask_n, 4, 200)) {
                    PROBE(
                        "clip:xmask",
                        "ent=%d etype=%d solid=0x%x box=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) "
                        "boxcontents=0x%x mask=0x%x crossedbox=1 -> entity pass blind for this "
                        "mask, the shot art lands behind the actor\n",
                        ent->number,
                        ent->eType,
                        ent->solid,
                        bmins[0], bmins[1], bmins[2],
                        bmaxs[0], bmaxs[1], bmaxs[2],
                        boxContents,
                        mask
                    );
                }
            }
        }

        VectorCopy(cent->lerpOrigin, origin);
        if (ent->solid == SOLID_BMODEL) {
            VectorCopy(cent->lerpAngles, angles);
        } else {
            VectorCopy(vec3_origin, angles);
        }

        cgi.CM_TransformedBoxTrace(&trace, start, end, mins, maxs, cmodel, mask, origin, angles, cylinder);

        if (trace.allsolid || trace.fraction < tr->fraction) {
            trace.entityNum = ent->number;
            *tr             = trace;
        } else if (trace.startsolid) {
            tr->startsolid = qtrue;
        }
    }
}

void CG_ShowTrace(trace_t *trace, int passent, const char *reason)
{
    char text[1024];

    assert(reason);
    assert(trace);

    Com_sprintf(
        text,
        sizeof(text),
        "%0.2f : Pass (%d) Frac %f Hit (%d): '%s'\n",
        (float)cg.time,
        passent,
        trace->fraction,
        trace->entityNum,
        reason ? reason : ""
    );

    if (cg_traceinfo->integer == 3) {
        cgi.DebugPrintf(text);
    } else {
        cgi.DPrintf(text);
    }
}

void CG_Trace(
    trace_t     *result,
    const vec3_t start,
    const vec3_t mins,
    const vec3_t maxs,
    const vec3_t end,
    int          skipNumber,
    int          mask,
    qboolean     cylinder,
    qboolean     cliptoentities,
    const char  *description
)
{
    trace_t t;

    cgi.CM_BoxTrace(&t, start, end, mins, maxs, 0, mask, cylinder);
    t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;


    if (t.startsolid) {
        t.entityNum = ENTITYNUM_WORLD;
    }

    if (cliptoentities) {
        CG_ClipMoveToEntities(start, mins, maxs, end, skipNumber, mask, &t, cylinder);
    }

    *result = t;

    if (cg_traceinfo->integer) {
        CG_ShowTrace(result, skipNumber, description);
    }
}

void CG_PlayerTrace(
    trace_t     *result,
    const vec3_t start,
    const vec3_t mins,
    const vec3_t maxs,
    const vec3_t end,
    int          skipNumber,
    int          mask,
    qboolean     cylinder
)
{
    CG_Trace(result, start, mins, maxs, end, skipNumber, mask, cylinder, qtrue, "PlayerTrace");
}

int CG_PointContents(const vec3_t point, int passEntityNum)
{
    int            i;
    entityState_t *ent;
    centity_t     *cent;
    clipHandle_t   cmodel;
    int            contents;

    contents = cgi.CM_PointContents(point, 0);

    for (i = 0; i < cg_numSolidEntities; i++) {
        cent = cg_solidEntities[i];

        ent = &cent->currentState;

        if (ent->number == passEntityNum) {
            continue;
        }

        if (ent->solid != SOLID_BMODEL) { 
            continue;
        }

        cmodel = cgi.CM_InlineModel(ent->modelindex);
        if (!cmodel) {
            continue;
        }

        contents |= cgi.CM_TransformedPointContents(point, cmodel, ent->origin, ent->angles);
    }

    return contents;
}

static void CG_InterpolatePlayerStateCamera(void)
{
    float       f;
    int         i;
    snapshot_t *prev, *next;

    prev = cg.snap;
    next = cg.nextSnap;

    VectorCopy(cg.predicted_player_state.camera_angles, cg.camera_angles);
    VectorCopy(cg.predicted_player_state.camera_origin, cg.camera_origin);

    // STAT_INZOOM.  zoomfov is not read anywhere in retail cgame: CG_CalcViewValues is its
    cg.camera_fov = cg.predicted_player_state.fov;

    if (cg.nextFrameCameraCut) {
        return;
    }

    if (!next || next->serverTime <= prev->serverTime) {
        return;
    }

    f = (float)(cg.time - prev->serverTime) / (next->serverTime - prev->serverTime);

    cg.camera_fov = prev->ps.fov + f * (next->ps.fov - prev->ps.fov);

    if (!(cg.snap->ps.pm_flags & PMF_CAMERA_VIEW)) {
        return;
    }

    if (cg.predicted_player_state.camera_flags & CF_CAMERA_ANGLES_TURRETMODE) {
        VectorCopy(cg.nextSnap->ps.camera_origin, cg.camera_origin);
        VectorCopy(cg.nextSnap->ps.camera_angles, cg.camera_angles);
        return;
    }

    for (i = 0; i < 3; i++) {
        cg.camera_origin[i] = prev->ps.camera_origin[i] + f * (next->ps.camera_origin[i] - prev->ps.camera_origin[i]);
        cg.camera_angles[i] = LerpAngle(prev->ps.camera_angles[i], next->ps.camera_angles[i], f);
    }
}

static void CG_InterpolatePlayerState(qboolean grabAngles)
{
    float          f;
    int            i;
    playerState_t *out;
    snapshot_t    *prev, *next;

    out  = &cg.predicted_player_state;
    prev = cg.snap;
    next = cg.nextSnap;

    *out = cg.snap->ps;

    CG_InterpolatePlayerStateCamera();

    if (grabAngles) {
        usercmd_t cmd;
        int       cmdNum;

        cmdNum = cgi.GetCurrentCmdNumber();
        cgi.GetUserCmd(cmdNum, &cmd);

        PM_UpdateViewAngles(out, &cmd);
    }

    if (cg.nextFrameTeleport) {
        return;
    }

    if (!next || next->serverTime <= prev->serverTime) {
        return;
    }

    f = cg.frameInterpolation;

    i = next->ps.bobCycle;
    if (i < prev->ps.bobCycle) {
        i += 256;
    }
    out->bobCycle = (int)(f * (i - prev->ps.bobCycle)) + prev->ps.bobCycle;

    for (i = 0; i < 3; i++) {
        out->origin[i] = prev->ps.origin[i] + f * (next->ps.origin[i] - prev->ps.origin[i]);
        if (!grabAngles) {
            out->viewangles[i] = LerpAngle(prev->ps.viewangles[i], next->ps.viewangles[i], f);
        }
        out->velocity[i] = prev->ps.velocity[i] + f * (next->ps.velocity[i] - prev->ps.velocity[i]);
    }
}


static void PM_PlayerTrace_Adapter(trace_t *results, const vec3_t start, const vec3_t mins,
                                   const vec3_t maxs, const vec3_t end, int skipNumber,
                                   int mask, qboolean cylinder)
{
    CG_PlayerTrace(results, start, mins, maxs, end, skipNumber, mask, cylinder);
}

void CG_PredictPlayerState(void)
{
    int           cmdNum, current;
    int           moverNum;
    playerState_t oldPlayerState;
    qboolean      moved;
    usercmd_t     latestCmd;

    cg.hyperspace = qfalse; 

    if (!cg.validPPS) {
        cg.validPPS               = qtrue;
        cg.predicted_player_state = cg.snap->ps;
    }

    if (cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_NO_PREDICTION) || (cg.snap->ps.pm_flags & PMF_FROZEN)
        || (cg.snap->ps.pm_type == PM_SPECTATOR_FOLLOW)) {
        CG_InterpolatePlayerState(qfalse);
        return;
    }

    if (cg_nopredict->integer || cg_synchronousClients->integer || (cg.snap->ps.pm_type == PM_NOCLIP)) {
        CG_InterpolatePlayerState(qtrue);
        return;
    }

    cg_pmove.ps            = &cg.predicted_player_state;
    cg_pmove.trace         = PM_PlayerTrace_Adapter;
    cg_pmove.pointcontents = CG_PointContents;

    if (cg_pmove.ps->pm_type == PM_DEAD) {
        cg_pmove.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
    } else {
        cg_pmove.tracemask = MASK_PLAYERSOLID;
    }

    cg_pmove.noFootsteps = (cgs.mp_flags & MP_FLAG_NO_FOOTSTEPS) > 0;

    if (!cg_easyJump) {
        cg_easyJump = cgi.Cvar_Get("cl_easyJump", "0", 0);
    }
    cg_pmove.easyJump = cg_easyJump->integer ? qtrue : qfalse;

    if (PROBE_On() && cg_easyJumpLast != cg_pmove.easyJump) {
        PROBE("easyjump", "cgame state easy=%d was=%d\n", (int)cg_pmove.easyJump, cg_easyJumpLast);
        cg_easyJumpLast = cg_pmove.easyJump;
    }



    oldPlayerState = cg.predicted_player_state;

    current = cgi.GetCurrentCmdNumber();

    cgi.GetUserCmd(current, &latestCmd);

    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        cg.predicted_player_state = cg.nextSnap->ps;
        cg.physicsTime            = cg.nextSnap->serverTime;
    } else {
        cg.predicted_player_state = cg.snap->ps;
        cg.physicsTime            = cg.snap->serverTime;
    }


    moved = qfalse;
    for (cmdNum = current - CMD_BACKUP + 1; cmdNum <= current; cmdNum++) {
        cgi.GetUserCmd(cmdNum, &cg_pmove.cmd);

        if (cg_pmove.cmd.serverTime <= cg.predicted_player_state.commandTime) {
            continue;
        }

        if (cg_pmove.cmd.serverTime > latestCmd.serverTime) {
            continue;
        }

        if (cg.predicted_player_state.commandTime == oldPlayerState.commandTime) {
            if (cg.thisFrameTeleport) {
                VectorClear(cg.predictedError);
                cg.thisFrameTeleport = qfalse;
                if (cg_showmiss->integer) {
                    cgi.Printf("PredictionTeleport\n");
                }
            } else {
                vec3_t adjusted;
                vec3_t delta;
                float  deltaLength;
                float  f;

                moverNum = cg.predicted_player_state.groundEntityNum;
                if (moverNum != ENTITYNUM_NONE && cg_entities[moverNum].currentState.eType != ET_MOVER) {
                    moverNum = ENTITYNUM_NONE;
                }

                CG_AdjustPositionForMover(
                    cg.predicted_player_state.origin, moverNum, cg.snap->serverTime,
                    cg.oldFrameTime, adjusted);

                VectorSubtract(oldPlayerState.origin, adjusted, delta);

                deltaLength = VectorLength(delta);
                if (deltaLength > 0.1f) {
                    if (cg_showmiss->integer) {
                        cgi.Printf("PredictionMiss(%f)\n", (double)deltaLength);
                    }

                    if (cg_errorDecay->value > 0) {
                        int t;

                        t = cg.time - cg.predictedErrorTime;
                        f = (cg_errorDecay->value - t) / cg_errorDecay->value;
                        if (f < 0.0f) {
                            f = 0.0f;
                        }
                        VectorScale(delta, f, cg.predictedError);
                    } else {
                        VectorCopy(delta, cg.predictedError);
                    }

                    cg.predictedErrorTime = cg.oldFrameTime;
                } else {
                    VectorClear(cg.predictedError);
                }
            }
        }

        if (cg.predicted_player_state.feetfalling && cg_pmove.waterlevel < 2) {
            cg_pmove.cmd.forwardmove = 0;
            cg_pmove.cmd.rightmove   = 0;
        }


        Pmove(&cg_pmove);

        if (cg_pmove.jumpResult == 1) {
            PROBE("jump", "cgame takeoff easy=%d duck=%d up=%d vz=%d\n", (int)cg_pmove.easyJump,
                  (cg.predicted_player_state.pm_flags & PMF_DUCKED) ? 1 : 0, (int)cg_pmove.cmd.upmove,
                  (int)cg.predicted_player_state.velocity[2]);
        } else if (cg_pmove.jumpResult == 2) {
            PROBE("jump", "cgame eaten easy=%d duck=1 up=%d vz=%d\n", (int)cg_pmove.easyJump,
                  (int)cg_pmove.cmd.upmove, (int)cg.predicted_player_state.velocity[2]);
        }

        moved = qtrue;

    }

    if (cg_showmiss->integer > 1) {
        cgi.Printf("[%i : %i] ", cg_pmove.cmd.serverTime, cg.time);
    }

    CG_InterpolatePlayerStateCamera();

    moverNum = cg.predicted_player_state.groundEntityNum;
    if (moverNum != ENTITYNUM_NONE && cg_entities[moverNum].currentState.eType != ET_MOVER) {
        moverNum = ENTITYNUM_NONE;
    }
    CG_AdjustPositionForMover(
        cg.predicted_player_state.origin, moverNum, cg.snap->serverTime, cg.time,
        cg.predicted_player_state.origin);

    if (!moved) {
        if (cg_showmiss->integer) {
            cgi.Printf("not moved\n");
        }
        return;
    }

    CG_TransitionPlayerState(&cg.predicted_player_state, &oldPlayerState);
}

void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops)
{
    if (cg_pmove.landed) {
        cg.landChange = cg_pmove.landedVelocity * 0.025f;

        if (cg.landChange < -32.0f) {
            cg.landChange = -32.0f;
        }

        cg.landTime = cg.time;
    }

    if (ps->viewheight != ops->viewheight) {
        cg.duckChange = (float)(ps->viewheight - ops->viewheight);
        cg.duckTime   = cg.time;
    }
}
