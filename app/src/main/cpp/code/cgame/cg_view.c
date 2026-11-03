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

#define PROBE_GET(probe_n, probe_d, probe_f) cgi.Cvar_Get(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       cgi.Printf
#include "../qcommon/qa_probe.h"

#include "be_cgprobe.h"


void CG_AndroidDisplayRect( int surfaceW, int surfaceH, int *outX, int *outY, int *outW, int *outH )
{
    if ( outX ) *outX = 0;
    if ( outY ) *outY = 0;
    if ( outW ) *outW = surfaceW;
    if ( outH ) *outH = surfaceH;

#ifndef __ANDROID__
    /* same rule as SCR_AndroidDisplayRect: the handheld's letterbox, ignored on desktop */
    return;
#else
    int   mode   = cg_android_aspect ? cg_android_aspect->integer : 0;
    float target = 0.0f;

    if ( mode == 1 ) {
        target = 16.0f / 9.0f;
    } else if ( mode == 2 ) {
        target = 4.0f / 3.0f;
    } else {
        return;
    }

    if ( surfaceW <= 0 || surfaceH <= 0 ) {
        return;
    }

    if ( (float)surfaceW / (float)surfaceH > target ) {
        int w = (int)( (float)surfaceH * target + 0.5f );

        if ( w <= 0 ) w = 1;
        if ( outW ) *outW = w;
        if ( outX ) *outX = ( surfaceW - w ) / 2;
    } else {
        int h = (int)( (float)surfaceW / target + 0.5f );

        if ( h <= 0 ) h = 1;
        if ( outH ) *outH = h;
        if ( outY ) *outY = ( surfaceH - h ) / 2;
    }
#endif
}

static void CG_CalcVrect(void)
{
    int size;
    int dx, dy, dw, dh;

    CG_AndroidDisplayRect( cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, &dx, &dy, &dw, &dh );

    if ( cg_android_aspect && cg_android_aspect->integer != 0 ) {
        cg.refdef.x      = dx;
        cg.refdef.y      = dy;
        cg.refdef.width  = dw;
        cg.refdef.height = dh;
    } else {
        if (cg.snap->ps.pm_flags & PMF_INTERMISSION) {
            size = 100;
        } else {
            if (cg_viewsize->integer < 30) {
                cgi.Cvar_Set("viewsize", "30");
                size = 30;
            } else if (cg_viewsize->integer > 100) {
                cgi.Cvar_Set("viewsize", "100");
                size = 100;
            } else {
                size = cg_viewsize->integer;
            }
        }
        if (cg_viewsizeheight->integer <= -1 || cg_viewsizewidth->integer <= -1) {
            cg.refdef.width = cgs.glconfig.vidWidth * size / 100;
            cg.refdef.width &= ~1;

            cg.refdef.height = cgs.glconfig.vidHeight * size / 100;
            cg.refdef.height &= ~1;

            cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width) / 2;
            cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height) / 2;
        } else {
            cg.refdef.width = cg_viewsizewidth->integer & ~1;
            cg.refdef.height = cg_viewsizeheight->integer & ~1;

            cg.refdef.x = cg_viewposx->integer;
            cg.refdef.y = cg_viewposy->integer;
        }
    }

    cgs.screenXScale  = (float)dw / 640.0;
    cgs.screenYScale  = (float)dh / 480.0;
}


#define CAMERA_MINIMUM_DISTANCE 40

static int CG_OffsetThirdPersonView(void)
{
    vec3_t        forward;
    vec3_t        original_camera_position;
    vec3_t        new_vieworg;
    trace_t       trace;
    vec3_t        min, max;
    float        *look_offset;
    float        *target_angles;
    float        *target_position;
    vec3_t        delta;
    vec3_t        original_angles;
    qboolean      lookactive, resetview;
    static vec3_t saved_look_offset;
    vec3_t        camera_offset;

    target_angles   = cg.refdefViewAngles;
    target_position = cg.refdef.vieworg;

    if (cg.snap->ps.stats[STAT_HEALTH] <= 0 && cg.snap->ps.pm_type != PM_SPECTATOR_FOLLOW) {
        target_angles[PITCH] = 45.0f;
        target_angles[YAW]   = (float)cg.snap->ps.stats[STAT_DEAD_YAW];
        target_angles[ROLL]  = 0.0f;
    }

    if (cg.predicted_player_state.in_vehicle) {
        target_position[2] = cg.predicted_player_state.vehicleoffset[2];
    }

    if (cg.predicted_player_state.camera_flags & CF_CAMERA_ANGLES_ABSOLUTE) {
        VectorClear(target_angles);
    }

    if (cg.predicted_player_state.camera_flags & CF_CAMERA_ANGLES_IGNORE_YAW) {
        target_angles[YAW] = 0;
    }

    if (cg.predicted_player_state.camera_flags & CF_CAMERA_ANGLES_IGNORE_PITCH) {
        target_angles[PITCH] = 0;
    }

    VectorSubtract(target_angles, cg.predicted_player_state.camera_offset, target_angles);

    look_offset = cgi.get_camera_offset(&lookactive, &resetview);

    if ((!resetview) && ((cg.predicted_player_state.camera_flags & CF_CAMERA_ANGLES_ALLOWOFFSET) || (lookactive))) {
        VectorSubtract(look_offset, saved_look_offset, camera_offset);
        VectorAdd(target_angles, camera_offset, target_angles);
        if (target_angles[PITCH] > 90) {
            target_angles[PITCH] = 90;
        } else if (target_angles[PITCH] < -90) {
            target_angles[PITCH] = -90;
        }
    } else {
        VectorCopy(look_offset, saved_look_offset);
    }

    target_angles[YAW]   = AngleNormalize360(target_angles[YAW]);
    target_angles[PITCH] = AngleNormalize180(target_angles[PITCH]);


    target_position[2] += cg_cameraheight->value;

    VectorCopy(target_position, original_camera_position);


    AngleVectors(target_angles, forward, NULL, NULL);

    VectorMA(target_position, -cg_cameradist->value, forward, new_vieworg);

    new_vieworg[2] += cg_cameraverticaldisplacement->value;


    min[0] = -5;
    min[1] = -5;
    min[2] = -5;

    max[0] = 5;
    max[1] = 5;
    max[2] = 5;

    CG_Trace(&trace, cg.playerHeadPos, min, max, new_vieworg, 0, MASK_CAMERASOLID, qfalse, qtrue, "ThirdPersonTrace 1");

    VectorCopy(trace.endpos, target_position);

    VectorSubtract(target_position, cg.playerHeadPos, delta);
    if (delta[2] < CAMERA_MINIMUM_DISTANCE) {
        delta[2] = 0;
    }
    if (VectorLength(delta) < CAMERA_MINIMUM_DISTANCE) {
        VectorNormalize(delta);
        {
            VectorCopy(target_angles, original_angles);
            while (target_angles[PITCH] < 90) {
                target_angles[PITCH] += 2;

                AngleVectors(target_angles, forward, NULL, NULL);

                VectorMA(original_camera_position, -cg_cameradist->value, forward, new_vieworg);

                new_vieworg[2] += cg_cameraverticaldisplacement->value;

                CG_Trace(
                    &trace,
                    cg.playerHeadPos,
                    min,
                    max,
                    new_vieworg,
                    0,
                    MASK_CAMERASOLID,
                    qfalse,
                    qtrue,
                    "ThirdPersonTrace 3"
                );

                VectorCopy(trace.endpos, target_position);

                VectorSubtract(target_position, cg.playerHeadPos, delta);
                if (delta[2] < 0) {
                    delta[2] = 0;
                }
                if (VectorLength(delta) >= CAMERA_MINIMUM_DISTANCE) {
                    target_angles[PITCH] = (0.25f * target_angles[PITCH]) + (0.75f * original_angles[PITCH]);
                    break;
                }
            }
            if (target_angles[PITCH] > 90) {
                target_angles[PITCH] = original_angles[PITCH];
            }
        }
    }

    return 0;
}

#define WAVE_AMPLITUDE 1
#define WAVE_FREQUENCY 0.4

static int CG_CalcFov(void)
{
    float x;
    float phase;
    float v;
    int   contents;
    float fov_x, fov_y;
    int   inwater;
    float fov_ratio;

    fov_ratio = (float)cg.refdef.width / (float)cg.refdef.height * (3.0 / 4.0);
    if (fov_ratio == 1) {
        fov_x = cg.camera_fov;
    } else {
        fov_x = RAD2DEG(atan(tan(DEG2RAD(cg.camera_fov / 2.0)) * fov_ratio)) * 2.0;
    }

    x = cg.refdef.width / tan(fov_x / 360 * M_PI);
    fov_y = atan2(cg.refdef.height, x);
    fov_y = fov_y * 360 / M_PI;

    contents = CG_PointContents(cg.refdef.vieworg, -1);
    if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
        v     = WAVE_AMPLITUDE * sin(phase);
        fov_x += v;
        fov_y -= v;
        inwater = qtrue;
    } else {
        inwater = qfalse;
    }

    cg.refdef.fov_x    = fov_x;
    cg.refdef.fov_y    = fov_y;
    cg.fRefFovXCos     = cos(fov_x / 114.0f);
    cg.fRefFovXSin     = sin(fov_x / 114.0f);
    cg.fRefFovYCos     = cos(fov_y / 114.0f);
    cg.fRefFovYSin     = sin(fov_y / 114.0f);
    cg.zoomSensitivity = cg.refdef.fov_y / 75.0;
    return inwater;
}

void CG_SetupFog(void) {
    cg.refdef.farplane_distance = cg.farplane_distance;
    cg.refdef.farplane_bias = cg.farplane_bias;
    cg.refdef.farplane_color[0] = cg.farplane_color[0];
    cg.refdef.farplane_color[1] = cg.farplane_color[1];
    cg.refdef.farplane_color[2] = cg.farplane_color[2];
    cg.refdef.farplane_cull = cg.farplane_cull;
    cg.refdef.farplane_fog = cg.farplane_fog;

    cg.refdef.sky_portal = cg.sky_portal;
    cg.refdef.sky_alpha  = cg.sky_alpha;
    VectorCopy(cg.sky_origin, cg.refdef.sky_origin);
    VectorCopy(cg.sky_axis[0], cg.refdef.sky_axis[0]);
    VectorCopy(cg.sky_axis[1], cg.refdef.sky_axis[1]);
    VectorCopy(cg.sky_axis[2], cg.refdef.sky_axis[2]);
}

static int   ef2_bobActive     = 0; 
static float ef2_bobStartPhase = 0.f; 
float ef2_bobfracsin           = 0.f;  

int CG_OffsetFirstPersonView(void)
{
    playerState_t *ps      = &cg.predicted_player_state;
    float         *vieworg = cg.refdef.vieworg;
    float          speed   = sqrt(ps->velocity[0] * ps->velocity[0] + ps->velocity[1] * ps->velocity[1]);
    float          phase;
    vec3_t         fwd, right, up;


    if (speed > 100.0f || ef2_bobActive) {
        if (cg_viewBobTime->value > 0.0f) {
            int q   = (int)((float)cg.time / cg_viewBobTime->value);
            int deg = q % 360;
            phase   = (float)((double)((float)deg * 0.0055555557f) * 3.141592653589793);
        } else {
            phase = 0.0f;
        }

        if (!ef2_bobActive) {
            ef2_bobActive     = 1;
            ef2_bobStartPhase = phase;
        }

        if (speed > 100.0f) {
            ef2_bobfracsin = sin(phase - ef2_bobStartPhase);
        } else {
            ef2_bobfracsin = ef2_bobfracsin * 0.75f;
        }

        if (cg_viewBob->integer == 0) {
            ef2_bobfracsin = 0.0f;
        }

        if (fabs(ef2_bobfracsin) < 0.05f) {
            ef2_bobActive  = 0;
            ef2_bobfracsin = 0.0f;
        }
    } else {
        ef2_bobfracsin = 0.0f;
    }

    if (cg_viewStrafeTilt->integer) {
        vec3_t yawAng;
        float  strafeVel;
        VectorSet(yawAng, 0.0f, cg.refdefViewAngles[YAW], 0.0f);
        AngleVectorsLeft(yawAng, fwd, right, NULL);
        strafeVel = DotProduct(ps->velocity, right);
        if (strafeVel > -13.0f && strafeVel < 13.0f) {
            strafeVel = 0.0f;
        }
        cg.refdefViewAngles[2] += strafeVel * -0.0080000004f;
    }

    {
        int delta = cg.time - cg.duckTime;

        if (delta < 100) {
            vec3_t  duckEnd;
            trace_t duckTrace;

            VectorCopy(vieworg, duckEnd);
            duckEnd[2] -= cg.duckChange * (float)(100 - delta) / 100.0f;

            CG_Trace(
                &duckTrace,
                vieworg,
                vec3_origin,
                vec3_origin,
                duckEnd,
                ps->clientNum,
                CONTENTS_SOLID | CONTENTS_SETCLIP | CONTENTS_WEAPONCLIP | CONTENTS_SHOOTABLE_ONLY | CONTENTS_ORIGIN
                    | CONTENTS_BODY | CONTENTS_CORPSE,
                qfalse,
                qtrue,
                "CG_OffsetFirstPersonView_duck"
            );

            VectorCopy(duckTrace.endpos, vieworg);
        }
    }

    {
        int delta = cg.time - cg.landTime;

        if (delta < 150) {
            vieworg[2] += cg.landChange * ((float)delta * 0.0066666668f);
        } else if (delta < 450) {
            vieworg[2] += cg.landChange * (1.0f - (float)(delta - 150) * 0.0033333334f);
        }
    }

    AngleVectorsLeft(cg.refdefViewAngles, fwd, right, up);
    VectorMA(vieworg, ef2_bobfracsin * cg_viewBobMaxHorz->value, right, vieworg);
    VectorMA(vieworg, fabs(ef2_bobfracsin * cg_viewBobMaxVert->value), up, vieworg);
    cg.refdefViewAngles[2] += ef2_bobfracsin * cg_viewBobMaxRoll->value;

    if (cg.predicted_player_state.leanDelta != 0.0f) {
        vec3_t leanLeft;

        cg.refdefViewAngles[ROLL] -= cg.predicted_player_state.leanDelta * 0.5f;

        AngleVectorsLeft(cg.refdefViewAngles, NULL, leanLeft, NULL);
        VectorMA(vieworg, cg.predicted_player_state.leanDelta, leanLeft, vieworg);
    }


    // death camera is CG_OffsetThirdPersonView's, Windows @0x30049031).  Not carried over.

    return 1;
}

void CG_InterpolateView(int personView)
{
    float   deltaTime;
    float   blend;
    vec3_t  targetOrigin;
    vec3_t  targetAngles;
    vec3_t  angleDelta;
    trace_t trace;

    (void)personView;

    if (cg.viewInterpTime != -1
        && (cg.predicted_player_state.camera_flags & CF_CAMERA_CUT_BIT)
            == (cg.viewInterpCameraFlags & CF_CAMERA_CUT_BIT)) {
        deltaTime = (float)(cg.time - cg.viewInterpTime) / 1000.0f;

        if (paused->integer) {
            blend = cg_camerascale->value;
        } else {
            blend = 30.0f * (deltaTime * cg_camerascale->value);
            if (blend > 1.0f) {
                blend = 1.0f;
            }
            if (blend < 0.0f) {
                blend = 0.0f;
            }
        }
    } else {
        deltaTime = 0.0f;
        blend     = 1.0f;
    }

    cg.viewInterpCameraFlags = cg.predicted_player_state.camera_flags;
    cg.viewInterpTime        = cg.time;

    if (!(cg.predicted_player_state.pm_flags & PMF_CAMERA_VIEW)) {
        if (cg.viewInterpCamera) {
            cg.viewInterpBlendLeft = cg.predicted_player_state.camera_time;
            cg.viewInterpCamera    = 0;
        }

        VectorCopy(cg.refdef.vieworg, targetOrigin);
        VectorCopy(cg.refdefViewAngles, targetAngles);
    } else {
        if (!cg.viewInterpCamera) {
            cg.viewInterpBlendLeft = cg.predicted_player_state.camera_time;
            cg.viewInterpCamera    = 1;
        }

        VectorCopy(cg.camera_origin, targetOrigin);
        VectorCopy(cg.camera_angles, targetAngles);
    }

    if (cg.viewInterpBlendLeft != 0.0f) {
        float remaining;
        float frac;

        remaining = cg.viewInterpBlendLeft - deltaTime;
        frac      = cg.viewInterpBlendLeft / cg.snap->ps.camera_time;

        cg.viewInterpBlendLeft = remaining;
        if (remaining < 0.0f) {
            cg.viewInterpBlendLeft = 0.0f;
        }
        if (frac > 1.0f) {
            frac = 1.0f;
        }

        blend = 1.0f - frac;
    }

    if (cg.viewInterpBlendLeft == 0.0f && (cg.predicted_player_state.pm_flags & PMF_CAMERA_VIEW)) {
        VectorCopy(cg.camera_origin, cg.refdef.vieworg);
        VectorCopy(cg.camera_angles, cg.refdefViewAngles);
    } else {
        AnglesSubtract(targetAngles, cg.currentViewAngles, angleDelta);

        cg.refdef.vieworg[0] = (targetOrigin[0] - cg.currentViewPos[0]) * blend + cg.currentViewPos[0];
        cg.refdef.vieworg[1] = (targetOrigin[1] - cg.currentViewPos[1]) * blend + cg.currentViewPos[1];
        cg.refdef.vieworg[2] = (targetOrigin[2] - cg.currentViewPos[2]) * blend + cg.currentViewPos[2];

        cg.refdefViewAngles[0] = angleDelta[0] * blend + cg.currentViewAngles[0];
        cg.refdefViewAngles[1] = angleDelta[1] * blend + cg.currentViewAngles[1];
        cg.refdefViewAngles[2] = angleDelta[2] * blend + cg.currentViewAngles[2];

        if (!VectorCompare(targetOrigin, cg.refdef.vieworg)) {
            CG_Trace(
                &trace,
                targetOrigin,
                vec3_origin,
                vec3_origin,
                cg.refdef.vieworg,
                cg.snap->ps.clientNum,
                CONTENTS_SOLID | CONTENTS_SETCLIP | CONTENTS_WEAPONCLIP | CONTENTS_SHOOTABLE_ONLY | CONTENTS_ORIGIN
                    | CONTENTS_BODY | CONTENTS_CORPSE,
                qfalse,
                qtrue,
                "CG_InterpolateView"
            );

            if (trace.fraction < 1.0f) {
                VectorCopy(trace.endpos, cg.refdef.vieworg);
            }
        }

        cg.refdefViewAngles[0] = AngleMod(cg.refdefViewAngles[0]);
        cg.refdefViewAngles[1] = AngleMod(cg.refdefViewAngles[1]);
        cg.refdefViewAngles[2] = AngleMod(cg.refdefViewAngles[2]);
    }

    if (cg.viewInterpBlendLeft == 0.0f) {
        VectorCopy(cg.refdef.vieworg, cg.currentViewPos);
        VectorCopy(cg.refdefViewAngles, cg.currentViewAngles);
    }
}

static int CG_CalcViewValues(void)
{
    playerState_t *ps;
    float          SoundAngles[3];
    float          relativeyaw;
    qboolean       personView;

    memset(&cg.refdef, 0, sizeof(cg.refdef));

    CG_CalcVrect();

    if (zoomfov->value != cg.snap->ps.fov) {
        cgi.Cvar_SetValue("zoomfov", cg.snap->ps.fov);
    }

    relativeyaw = AngleNormalize360(cg.snap->ps.viewangles[YAW]) / 360.0f;
    if (cg_relativeyaw->value != relativeyaw) {
        cgi.Cvar_SetValue("relativeyaw", relativeyaw);
    }

    CG_SetupFog();

    ps = &cg.predicted_player_state;

    VectorCopy(ps->origin, cg.refdef.vieworg);
    VectorCopy(ps->viewangles, cg.refdefViewAngles);



    if (cg_errorDecay->value > 0) {
        int   t;
        float f;

        t = cg.time - cg.predictedErrorTime;
        f = (cg_errorDecay->value - t) / cg_errorDecay->value;
        if (f > 0 && f < 1) {
            VectorMA(cg.refdef.vieworg, f, cg.predictedError, cg.refdef.vieworg);
        } else {
            cg.predictedErrorTime = 0;
        }
    }

    cg.refdef.vieworg[2] += cg.predicted_player_state.viewheight;

    VectorCopy(cg.refdef.vieworg, cg.playerHeadPos);

    VectorCopy(cg.playerHeadPos, cg.SoundOrg);

    // negation was dropped for SDL/OpenAL was a port decision, not retail.
    VectorCopy(cg.refdefViewAngles, SoundAngles);
    SoundAngles[YAW] = -SoundAngles[YAW];
    AnglesToAxis(SoundAngles, cg.SoundAxis);

    cg.renderingThirdPerson = (cg_3rd_person->integer || ps->pm_type == PM_SPECTATOR_FOLLOW
                               || cg.snap->ps.stats[STAT_HEALTH] <= 0
                               || ps->pm_type == PM_SECRET_MOVE_MODE || ps->pm_type == PM_3RD_PERSON)
                                 ? qtrue
                                 : qfalse;

    if (cg.renderingThirdPerson) {
        personView = CG_OffsetThirdPersonView();
    } else {
        personView = CG_OffsetFirstPersonView();
    }

    CG_InterpolateView(personView);

    if (cg.viewInterpCamera) {
        VectorCopy(cg.refdef.vieworg, cg.SoundOrg);
        VectorCopy(cg.refdefViewAngles, SoundAngles);
        SoundAngles[YAW] = -SoundAngles[YAW];
        AnglesToAxis(SoundAngles, cg.SoundAxis);
    }

    if (cg_viewKick->integer) {
        int i;

        for (i = 0; i < 3; i++) {
            cg.refdefViewAngles[i] -=
                cg.snapDamageAngles[i]
                + (ps->damage_angles[i] - cg.snapDamageAngles[i]) * cg.frameInterpolation;
        }
    }

    AnglesToAxis(cg.refdefViewAngles, cg.refdef.viewaxis);

    if (cg.hyperspace) {
        cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
    }

    return CG_CalcFov();
}


void CG_GetViewOrigin(float *x, float *y, float *z)
{
    *x = cg.refdef.vieworg[0];
    *y = cg.refdef.vieworg[1];
    *z = cg.refdef.vieworg[2];
}

void CG_GetViewAngles(float *x, float *y, float *z)
{
    *x = cg.currentViewAngles[0];
    *y = cg.currentViewAngles[1];
    *z = cg.currentViewAngles[2];
}

int CG_GetThirdPerson(void)
{
    return cg.renderingThirdPerson;
}

void CG_GetInterpState(int entNum, vec3_t *origin, vec3_t *angles)
{
    const centity_t *cent = &cg_entities[entNum];

    (*origin)[0] = cent->lerpOrigin[0];
    (*origin)[1] = cent->lerpOrigin[1];
    (*origin)[2] = cent->lerpOrigin[2];

    (*angles)[0] = cent->lerpAngles[0];
    (*angles)[1] = cent->lerpAngles[1];
    (*angles)[2] = cent->lerpAngles[2];
}

qboolean CG_FrustumCullSphere(const vec3_t vPos, float fRadius) {
    vec3_t delta;
    float fDotFwd, fDotSide, fDotUp;

    VectorSubtract(vPos, cg.refdef.vieworg, delta);

    fDotFwd = DotProduct(delta, cg.refdef.viewaxis[0]);
    if (-fRadius >= fDotFwd) {
        return qtrue;
    }

    if (cg.refdef.farplane_distance && cg.refdef.farplane_distance + fRadius <= fDotFwd) {
        return qtrue;
    }

    fDotSide = DotProduct(delta, cg.refdef.viewaxis[1]);
    if (fDotSide < 1.f) {
        fDotSide = -fDotSide;
    }

    if (fDotSide * cg.fRefFovXCos - fDotFwd * cg.fRefFovXSin >= fRadius) {
        return qtrue;
    }

    fDotUp = DotProduct(delta, cg.refdef.viewaxis[2]);
    if (fDotUp < 0.f) {
        fDotUp = -fDotUp;
    }

    if (fDotUp * cg.fRefFovYCos - fDotFwd * cg.fRefFovYSin >= fRadius) {
        return qtrue;
    }

    return qfalse;
}

qboolean CG_SimpleDistanceCull(const vec3_t origin, float maxDist) {
    vec3_t delta;

    VectorSubtract(origin, cg.refdef.vieworg, delta);

    return VectorLengthSquared(delta) > maxDist;
}


static int cg_targetNameTime = 0;

static void CG_AddTargetName(void)
{
    vec3_t      forward, end;
    trace_t     tr;
    const char *name;

    if (!cg.snap) {
        return;
    }

    AngleVectors(cg.refdefViewAngles, forward, NULL, NULL);
    VectorMA(cg.refdef.vieworg, 96.0f, forward, end);

    CG_Trace(&tr, cg.refdef.vieworg, NULL, NULL, end, ENTITYNUM_NONE, MASK_SHOT, qfalse, qtrue, "CG_AddTargetName");

    /* retail CG_AddTargetName @0x00044AB0: no hit means an empty name, which
       empties the cvar once 500ms have passed since the last hit */
    name = "";

    if (tr.entityNum < MAX_CLIENTS && tr.entityNum != cg.snap->ps.clientNum) {
        name = CG_ConfigString(CS_PLAYERS + tr.entityNum);

        if (strlen(name) > 5) {
            name += 5;
        }

        cg_targetNameTime = cg.time;
    }

    if (strcmp(name, cg_targetedPlayerName->string)) {
        if (name[0] || cg_targetNameTime + 500 < cg.time) {
            cgi.Cvar_Set("cg_targetedPlayerName", name);
        }
    }
}

// CG_DrawActiveFrame @0x00044D10 never touches the clock: it ignores its first
void CG_UpdateClient(int serverTime)
{
    cg.time = serverTime;

    cgi.S_ClearLoopingSounds();

    CG_ProcessSnapshots(serverTime);

    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        return;
    }

    cg.clientFrame++;
}

// So retail's body does NOT set cg.time, does NOT call CG_ProcessSnapshots and does
// NOT bump cg.clientFrame.  Those three live in CG_UpdateClient @0x000385E0
// the UI -- does not run.  The level script's `showmenu prologue` (retail sends it at
// registration.  The split is therefore retail's, with no port-only replacement.
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback)
{
    (void)serverTime;

    int be_cgFrame = BE_CgFrameOpen();

    cg.demoPlayback = demoPlayback;

    {
        int be_z = BE_CG_T0();

        cgi.S_ClearLoopingSounds();

        cgi.R_ClearScene();

        BE_CG_ZONE_END(BE_CGZ_clear, be_z);
    }

    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        CG_DrawActive(stereoView);
        BE_CgFrameEnd(be_cgFrame);
        return;
    }

    if (cg_crosshairtexture->modified) {
        cgs.media.crosshairShader = cgi.R_RegisterShaderNoMip(cg_crosshairtexture->string);
        cg_crosshairtexture->modified = qfalse;
    }

    if (cg.nextSnap && r_lerpmodels->integer) {
        int delta;

        delta = (cg.nextSnap->serverTime - cg.snap->serverTime);
        if (delta == 0) {
            cg.frameInterpolation = 0;
        } else {
            cg.frameInterpolation = (float)(cg.time - cg.snap->serverTime) / delta;
        }
    } else {
        cg.frameInterpolation = 0; 
    }

    {
        int be_z = BE_CG_T0();

        CG_PredictPlayerState();

        BE_CG_ZONE_END(BE_CGZ_predict, be_z);
    }

    {
        int be_z = BE_CG_T0();

        CG_CalcViewValues();

        BE_CG_ZONE_END(BE_CGZ_calcview, be_z);
    }

    {
        int be_z = BE_CG_T0();

        if (!cg.hyperspace) {
            CG_AddPacketEntities(); 
            CG_AddMarks();
        }

        BE_CG_COUNT(BE_CGC_snapents, cg.snap->numEntities);

        BE_CG_ZONE_END(BE_CGZ_ents, be_z);
    }

    cg.refdef.time = cg.time;
    if (cg.nextSnap) {
        int k;

        for (k = 0; k < (int)sizeof(cg.refdef.areamask) / 4; k++) {
            ((int *)cg.refdef.areamask)[k] =
                ((int *)cg.snap->areamask)[k] & ((int *)cg.nextSnap->areamask)[k];
        }
    } else {
        memcpy(cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));
    }

    {
        int be_z = BE_CG_T0();

        cgi.S_Respatialize(cg.snap->ps.clientNum, cg.SoundOrg, cg.SoundAxis);

        BE_CG_ZONE_END(BE_CGZ_audio, be_z);
    }

    if (stereoView != STEREO_RIGHT) {
        cg.frametime = cg.time - cg.oldFrameTime;
        if (cg.frametime < 0) {
            cg.frametime = 0;
        }
        cg.oldFrameTime = cg.time;

        CG_AddLagometerFrameInfo();
    }

    {
        int be_z = BE_CG_T0();

        CG_UpdateTestEmitter();

        if (!cg_hidetempmodels->integer) {
            CG_AddTempModels();
        }

        CG_AddBeams();

        CG_Weather();

        CG_UpdateSwipes();

        BE_CG_ZONE_END(BE_CGZ_fx, be_z);
    }

    {
        int be_z = BE_CG_T0();

    {
        unsigned int viewMode = cg.snap->ps.viewMode;
        int          i;

        if (cg.cachedViewMode == -1) {
            for (i = 0; i < cgi.GetNumViewModes(); i++) {
                const char *hudName = NULL;

                if (cgi.GetViewModeHud(1u << i, &hudName)) {
                    cgi.Cmd_Execute(EXEC_NOW, va("ui_removehud %s\n", hudName));
                }
            }

            cg.cachedViewMode = 0;
        }

        if (viewMode != 0 && viewMode != (unsigned)cg.cachedViewMode) {
            const char *hudName = NULL;

            if (cgi.GetViewModeHud(viewMode, &hudName)) {
                PROBE("vm:add", "viewMode=%u '%s' -> ui_addhud", viewMode, hudName);
                cgi.Cmd_Execute(EXEC_NOW, va("ui_addhud %s\n", hudName));
            } else {
                PROBE("vm:add", "viewMode=%u GetViewModeHud=FALSE (table n=%d)",
                           viewMode, cgi.GetNumViewModes());
            }
        }

        if (cg.cachedViewMode != 0 && viewMode != (unsigned)cg.cachedViewMode) {
            const char *hudName = NULL;

            if (cgi.GetViewModeHud(cg.cachedViewMode, &hudName)) {
                PROBE("vm:rem", "viewMode=%u cached=%d '%s' -> ui_removehud",
                           viewMode, cg.cachedViewMode, hudName);
                cgi.Cmd_Execute(EXEC_NOW, va("ui_removehud %s\n", hudName));
            }
        }

        cg.cachedViewMode = (int)viewMode;

        if (cgi.GetViewModeUseFullbright(viewMode)) {
            const char *hudName = NULL;

            cg.refdef.rdflags |= RDF_FULLBRIGHT;

            /* The shipped `global/viewmodes.txt' gives exactly one mode `useFullBright 1' --
               `nightvision' -- and that mode's own hud is `nightvisionhud', so the mode is named
               by the hud the table gives it rather than by the generic fullbright predicate. */
            if (cgi.GetViewModeHud(viewMode, &hudName) && hudName
                && !Q_stricmp(hudName, "nightvisionhud")) {
                cg.refdef.rdflags |= RDF_NIGHTVISION;
            } else {
                cg.refdef.rdflags &= ~RDF_NIGHTVISION;
            }
        } else {
            cg.refdef.rdflags &= ~RDF_FULLBRIGHT;
            cg.refdef.rdflags &= ~RDF_NIGHTVISION;
        }
    }

    CG_AddTargetName();

        BE_CG_ZONE_END(BE_CGZ_viewmode, be_z);
    }

    {
        int be_z = BE_CG_T0();

    CG_DrawActive(stereoView);

    if (cg_stats->integer) {
        cgi.Printf("cg.clientFrame:%i\n", cg.clientFrame);
    }

        BE_CG_ZONE_END(BE_CGZ_draw, be_z);
    }

    BE_CgFrameEnd(be_cgFrame);
}
