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


#include "cg_local.h"
#include "../tiki/tiki_shared.h"

#define PROBE_GET(probe_n, probe_d, probe_f) cgi.Cvar_Get(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       cgi.Printf
#include "../qcommon/qa_probe.h"
#define WIDGET_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define WIDGET_PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_widget.h"



void CG_InterpolateAnimParms(centity_t *cent, refEntity_t *model)
{
    entityState_t *state = &cent->currentState;
    entityState_t *sNext = &cent->nextState;
    int            i;

    for (i = 0; i < NUM_MORPH_CONTROLLERS; i++) {
        model->morph_controllers[i].index = state->morph_controllers[i].index;

        if (state->morph_controllers[i].index == sNext->morph_controllers[i].index) {
            model->morph_controllers[i].percent =
                cg.frameInterpolation
                    * (sNext->morph_controllers[i].percent - state->morph_controllers[i].percent)
                + state->morph_controllers[i].percent;
        } else {
            model->morph_controllers[i].percent = sNext->morph_controllers[i].percent;
        }
    }
}


static void CG_SetModelAnimation(
    int          tiki,
    animstate_t *state,
    animstate_t *otherstate,
    int          anim,
    int          time,
    const vec3_t origin,
    int          entnum,
    qboolean     bCheckCrossblendTime,
    float        animationRate,
    qboolean     bNoDeltaDrive
)
{
    int      animnum;
    int      animflags;
    int      numframes;
    qboolean driven;

    animnum = anim & ANIM_MASK;

    numframes = cgi.Anim_NumFrames(tiki, animnum);
    if (!numframes) {
        cgi.DPrintf("Can't find animation %i\n", animnum);
        return;
    }

    animflags = cgi.Anim_Flags(tiki, animnum);
    driven    = ((animflags & TAF_DELTADRIVEN) || animationRate != 1.0f) ? qtrue : qfalse;

    state->time_per_frame = 1000.0f * cgi.TIKI_Frame_Time(tiki, animnum, 0) / animationRate;
    if (state->time_per_frame == 0.0f) {
        cgi.DPrintf("Animation %i has no time information\n", animnum);
        return;
    }


    if (state->base.anim & ANIM_BLEND) {
        state->base.oldanim  = state->base.anim;
        state->base.oldframe = state->base.frame;
    } else {
        state->base.oldanim  = otherstate->base.oldanim;
        state->base.oldframe = otherstate->base.oldframe;
    }

    // 0x3427C. A restart of the SAME animation index does not start a crossblend -- only a
    if (animnum != (state->base.anim & ANIM_MASK)) {
        if ((time - state->crossblend.starttime) >= state->crossblend_totaltime || !bCheckCrossblendTime
            || (animflags & TAF_NOTIMECHECK)) {
            if (state->base.anim & ANIM_BLEND) {
                state->crossblend = state->base;
            } else {
                state->crossblend = otherstate->base;
            }

            state->crossblend.starttime = time;
            state->crossblend_totaltime = cgi.Anim_CrossblendTime(tiki, animnum);
        }
    }

    if (driven != state->driven) {
        if (driven) {
            VectorCopy(origin, state->last_origin);
            state->last_driven_time  = cg.time;
            state->current_distance  = 0.0f;
            state->base.frame        = 0;
        } else {
            state->last_cmd_frame = 0;
        }
    } else if (driven && animnum != (state->base.anim & ANIM_MASK)) {
        state->base.frame = 0;
    }

    if (state->numframes == numframes && numframes < 3) {
        state->last_cmd_frame = 0;
    }

    state->driven    = driven;
    state->base.anim = anim;

    if (driven && !bNoDeltaDrive) {
        cgi.Anim_Delta(tiki, animnum, state->frame_delta);
        VectorScale(state->frame_delta, 1.0f / (float)numframes, state->frame_delta);
        state->frame_distance = VectorLength(state->frame_delta);
        VectorNormalize(state->frame_delta);

        if (state->frame_distance < 0.1f) {
            CHAN_PROBE(
                "anim:nodelta",
                "ent=%d anim=%d '%s' numframes=%d frame_distance=%.4f -> time-driven clamp\n",
                entnum,
                animnum,
                cgi.Anim_NameForNum(tiki, animnum),
                numframes,
                state->frame_distance
            );
            cgi.DPrintf(
                "Animation %s has no delta information, canceling delta.\n", cgi.Anim_NameForNum(tiki, animnum)
            );
            state->driven = qfalse;
        }
    } else {
        state->driven     = qfalse;
        state->base.frame = 0;
    }

    state->has_commands       = cgi.Anim_HasCommands(tiki, animnum);
    state->numframes          = numframes;
    state->base.starttime     = time;
    state->next_evaluate_time = (int)((float)time + state->time_per_frame);

    {
        static int chan_set;
        if (PROBE_Throttle(&chan_set, 200, 100)) {
            CHAN_PROBE(
                "anim:set",
                "ent=%d anim=%d(%d) '%s' numframes=%d rate=%.2f tpf=%.2f driven=%d "
                "frame_distance=%.4f bNoDeltaDrive=%d\n",
                entnum,
                anim,
                animnum,
                cgi.Anim_NameForNum(tiki, animnum),
                numframes,
                animationRate,
                state->time_per_frame,
                state->driven,
                state->frame_distance,
                bNoDeltaDrive
            );
        }
    }
}

static void CG_RunModelAnimation(
    int            tiki,
    animstate_t   *state,
    animstate_t   *otherstate,
    int            anim,
    int            time,
    const vec3_t   origin,
    const vec3_t  *axis,
    entityState_t *s,
    qboolean       bCheckCrossblendTime,
    qboolean       bNoDeltaDrive
)
{
    if (tiki < 0) {
        return;
    }

    if (cg_animSpeed->value == 0.0f) {
        state->base.framelerp = 0.0f;
        state->base.frame     = 0;
        state->base.oldframe  = 0;
        return;
    }

    if (anim != state->base.anim) {
        CG_SetModelAnimation(
            tiki,
            state,
            otherstate,
            anim,
            time,
            origin,
            s->number,
            bCheckCrossblendTime,
            s->animationRate,
            bNoDeltaDrive
        );
    }

    if (state->time_per_frame == 0.0f) {
        return;
    }

    if (state->driven) {
        vec3_t delta;
        vec3_t local;
        float  dist;

        VectorSubtract(origin, state->last_origin, delta);
        VectorCopy(origin, state->last_origin);
        state->last_driven_time = cg.time;

        local[0] = DotProduct(delta, axis[0]);
        local[1] = DotProduct(delta, axis[1]);
        local[2] = DotProduct(delta, axis[2]);

        dist = VectorNormalize(local);
        state->current_distance += dist * DotProduct(state->frame_delta, local);

        while (state->current_distance > state->frame_distance) {
            state->base.oldanim  = state->base.anim & ANIM_MASK;
            state->base.oldframe = state->base.frame;
            state->base.frame++;
            state->current_distance -= state->frame_distance;
            if (state->base.frame >= state->numframes) {
                state->base.frame -= state->numframes;
            }
        }

        while (state->current_distance < -state->frame_distance) {
            state->base.oldanim  = state->base.anim & ANIM_MASK;
            state->base.oldframe = state->base.frame;
            state->base.frame--;
            state->current_distance += state->frame_distance;
            if (state->base.frame < 0) {
                state->base.frame += state->numframes;
            }
        }

        if (state->current_distance < 0.0f) {
            state->base.framelerp = (state->current_distance + state->frame_distance) / state->frame_distance;
        } else {
            state->base.framelerp = (state->frame_distance - state->current_distance) / state->frame_distance;
        }
    } else {
        if (cg.time >= state->next_evaluate_time) {
            int framenum;

            state->next_evaluate_time = (int)((float)state->next_evaluate_time + state->time_per_frame);

            framenum = (int)((float)(cg.time - state->base.starttime) / state->time_per_frame);
            if (framenum < 0) {
                framenum = 0;
            }
            if (framenum >= state->numframes) {
                // 0x34828: CLAMP, not wrap.  A time-driven channel plays its animation
                framenum = state->numframes - 1;
            }

            state->base.oldframe = state->base.frame;
            state->base.oldanim  = state->base.anim & ANIM_MASK;
            state->base.frame    = framenum;
        }

        /* the too-fast report needed a clock to read: tpf is the anim's declared per-frame time, cgt the client clock, dt the real milliseconds since
           this state last emitted.  Keyed on the entity alone it aliased every channel and anim switch; `driven' names the movement-driven channel. */
        if (PROBE_On()) {
            static const void *s_clockState = NULL;
            static int  s_clockAnim = -1;
            static int  s_lastTime;
            static int  s_rows;
            static int  s_lastFrame;

            if (s_clockState != (const void *)state || s_clockAnim != (state->base.anim & ANIM_MASK)) {
                s_clockState = (const void *)state;
                s_clockAnim  = state->base.anim & ANIM_MASK;
                s_lastTime   = cg.time;
                s_lastFrame  = state->base.frame;
                s_rows       = 0;
            } else if (++s_rows % 120 == 0) {
                Com_Printf(
                    "[ANIM:clock] ent=%d anim=%d frame=%d oldframe=%d tpf=%.2f cgt=%d "
                    "dt=%d advanced=%d expect=%.2f driven=%d state=%p\n",
                    s->number, state->base.anim & ANIM_MASK, state->base.frame,
                    state->base.oldframe, state->time_per_frame, cg.time,
                    cg.time - s_lastTime, state->base.frame - s_lastFrame,
                    (float)(cg.time - s_lastTime) / state->time_per_frame,
                    state->driven ? 1 : 0, (const void *)state);
                s_lastTime  = cg.time;
                s_lastFrame = state->base.frame;
            }
        }

        // 0x34854. Recomputed every frame, not only on advance. Clamped below at 0 and
        state->base.framelerp =
            1.0f
            - ((float)(cg.time - state->base.starttime) - (float)state->base.frame * state->time_per_frame)
                  / state->time_per_frame;
        if (state->base.framelerp < 0.0f) {
            state->base.framelerp = 0.0f;
        }
    }

    // 0x348C0. Crossblend progress, for both clocks. -1 is the "no blend running" sentinel,
    if (state->crossblend_totaltime) {
        state->crossblend_lerp = (float)(cg.time - state->crossblend.starttime) / (float)state->crossblend_totaltime;
        if (state->crossblend_lerp < 0.0f) {
            state->crossblend_lerp = 0.0f;
        }
        if (state->crossblend_lerp > 1.0f) {
            state->crossblend_lerp = -1.0f;
        }
    } else {
        state->crossblend_lerp = -1.0f;
    }
}

void CG_ClearModelAnimation(
    int tiki, animstate_t *state, int anim, int time, const vec3_t origin, int entnum, float animationRate
)
{
    memset(&state->base, 0, sizeof(state->base));

    if (tiki >= 0) {
        CG_SetModelAnimation(
            tiki, state, state, anim, time, origin, entnum, qtrue , animationRate,
            qfalse 
        );

        state->base.oldanim  = state->base.anim;
        state->base.oldframe = state->base.frame;
    }

    state->last_cmd_time  = 0;
    state->last_cmd_frame = 0;
    state->last_cmd_anim  = 0;

    memset(&state->crossblend, 0, sizeof(state->crossblend));
    state->crossblend.starttime = time;
    state->crossblend_totaltime = 0;
    state->crossblend_lerp      = -1.0f;
}

static void CG_EF2ClearLegsChannel(refEntity_t *model)
{
    model->legsLive        = qfalse;
    model->crossblend_lerp = -1.0f;
}

static void CG_EF2ClearTorsoChannel(refEntity_t *model)
{
    model->torsoLive              = qfalse;
    model->torso_crossblend_lerp  = -1.0f;
}

static void CG_EF2WriteLegsChannel(refEntity_t *model, const animstate_t *state)
{
    model->anim     = state->base.anim & ANIM_MASK;
    model->oldanim  = state->base.oldanim & ANIM_MASK;
    model->oldframe = (short)state->base.oldframe;
    model->frame    = (short)state->base.frame;
    model->framelerp = state->base.framelerp;

    model->crossblend_lerp      = state->crossblend_lerp;
    model->crossblend_framelerp = state->crossblend.framelerp;
    model->crossblend_anim      = state->crossblend.anim & ANIM_MASK;
    model->crossblend_frame     = (short)state->crossblend.frame;
    model->crossblend_oldframe  = (short)state->crossblend.oldframe;

    model->legsLive = qtrue;
}

static void CG_EF2WriteLegsRetireChannel(refEntity_t *model, const animstate_t *state)
{
    model->anim      = model->torso_anim;
    model->oldanim   = model->torso_oldanim;
    model->oldframe  = model->torso_oldframe;
    model->frame     = model->torso_frame;
    model->framelerp = model->torso_framelerp;

    model->crossblend_lerp      = state->crossblend_lerp;
    model->crossblend_framelerp = state->crossblend.framelerp;
    model->crossblend_anim      = state->crossblend.anim & ANIM_MASK;
    model->crossblend_frame     = (short)state->crossblend.frame;
    model->crossblend_oldframe  = (short)state->crossblend.oldframe;

    model->legsLive = qtrue;
}

static void CG_EF2WriteTorsoChannel(refEntity_t *model, const animstate_t *state)
{
    model->torso_anim     = state->base.anim & ANIM_MASK;
    model->torso_oldanim  = state->base.oldanim & ANIM_MASK;
    model->torso_frame    = (short)state->base.frame;
    model->torso_oldframe = (short)state->base.oldframe;
    model->torso_framelerp = state->base.framelerp;

    model->torso_crossblend_lerp     = state->crossblend_lerp;
    model->torso_crossblend_framelerp = state->crossblend.framelerp;
    model->torso_crossblend_anim     = state->crossblend.anim & ANIM_MASK;
    model->torso_crossblend_frame    = (short)state->crossblend.frame;
    model->torso_crossblend_oldframe = (short)state->crossblend.oldframe;

    model->torsoLive = qtrue;
}

static qboolean CG_EF2RetireAnimChannel(int tiki, animstate_t *state, qboolean bRequireExpired)
{
    if (state->base.anim & ANIM_BLEND) {
        if (!bRequireExpired || (cg.time - state->crossblend.starttime) >= state->crossblend_totaltime) {
            state->crossblend.starttime = cg.snap->serverTime;
            state->crossblend_totaltime =
                cgi.Anim_CrossblendTime(tiki, state->crossblend.anim & ANIM_MASK);
            state->base.anim &= ~ANIM_BLEND;
        }
    }

    if ((cg.time - state->crossblend.starttime) > state->crossblend_totaltime) {
        return qfalse;
    }

    if (state->crossblend_totaltime) {
        state->crossblend_lerp = (float)(cg.time - state->crossblend.starttime) / (float)state->crossblend_totaltime;
        if (state->crossblend_lerp < 0.0f) {
            state->crossblend_lerp = 0.0f;
        }
        if (state->crossblend_lerp > 1.0f) {
            state->crossblend_lerp = -1.0f;
        }
    } else {
        state->crossblend_lerp = -1.0f;
    }

    return qtrue;
}

static qboolean CG_EF2ModelAnim(centity_t *cent, refEntity_t *model)
{
    entityState_t *s1              = &cent->currentState;
    qboolean       bDoLegs;
    qboolean       bDoTorso;
    int            tiki = model->tiki ? cgi.R_Model_GetHandle(model->hModel) : -1;
    int animTime = cg.snap->serverTime;

    bDoLegs  = (s1->anim & ANIM_BLEND) ? qtrue : qfalse;
    bDoTorso = (s1->torso_anim & ANIM_BLEND) ? qtrue : qfalse;

    CG_EF2ClearLegsChannel(model);
    CG_EF2ClearTorsoChannel(model);

    if (!bDoLegs) {
        bDoLegs = (bDoTorso == qfalse) ? qtrue : qfalse;
    }

    if (bDoLegs) {
        if (s1->frame & FRAME_EXPLICIT) {
            if (tiki >= 0 && s1->anim != cent->animstate[0].base.anim) {
                CG_SetModelAnimation(
                    tiki,
                    &cent->animstate[0],
                    &cent->animstate[1],
                    s1->anim,
                    animTime,
                    cent->lerpOrigin,
                    s1->number,
                    qtrue,
                    s1->animationRate,
                    qfalse
                );
            }

            model->anim      = s1->anim & ANIM_MASK;
            model->oldanim   = model->anim;
            model->frame     = (short)(s1->frame & FRAME_MASK);
            model->oldframe  = model->frame;
            model->framelerp = 0.0f;
            model->legsLive  = qtrue;
        } else {
            CG_RunModelAnimation(
                tiki,
                &cent->animstate[0],
                &cent->animstate[1],
                s1->anim,
                animTime,
                cent->lerpOrigin,
                (const vec3_t *)model->axis,
                s1,
                qtrue,
                qfalse
            );

            CG_EF2WriteLegsChannel(model, &cent->animstate[0]);
        }
    }

    if (bDoTorso) {
        if (s1->torso_frame & FRAME_EXPLICIT) {
            if (tiki >= 0 && s1->torso_anim != cent->animstate[1].base.anim) {
                CG_SetModelAnimation(
                    tiki,
                    &cent->animstate[1],
                    &cent->animstate[0],
                    s1->torso_anim,
                    animTime,
                    cent->lerpOrigin,
                    s1->number,
                    qfalse,
                    s1->animationRate,
                    qtrue
                );
            }

            model->torso_anim      = s1->torso_anim & ANIM_MASK;
            model->torso_oldanim   = model->torso_anim;
            model->torso_frame     = (short)(s1->torso_frame & FRAME_MASK);
            model->torso_oldframe  = model->torso_frame;
            model->torso_framelerp = 0.0f;
            model->torsoLive       = qtrue;
        } else {
            CG_RunModelAnimation(
                tiki,
                &cent->animstate[1],
                &cent->animstate[0],
                s1->torso_anim,
                animTime,
                cent->lerpOrigin,
                (const vec3_t *)model->axis,
                s1,
                qfalse,
                qtrue
            );

            CG_EF2WriteTorsoChannel(model, &cent->animstate[1]);
        }
    } else if (CG_EF2RetireAnimChannel(tiki, &cent->animstate[1], qfalse )) {
        CG_EF2WriteTorsoChannel(model, &cent->animstate[1]);
    }

    if (!bDoLegs) {
        if (CG_EF2RetireAnimChannel(tiki, &cent->animstate[0], qtrue )) {
            CG_EF2WriteLegsRetireChannel(model, &cent->animstate[0]);
        }
    }

    {
        static int chan_lastanim[256];
        static int chan_lasttorso[256];
        static int chan_chan[8];
        int        slot = s1->number & 255;

        if ((s1->anim != chan_lastanim[slot]) || (s1->torso_anim != chan_lasttorso[slot])) {
            chan_lastanim[slot]  = s1->anim;
            chan_lasttorso[slot] = s1->torso_anim;
            if (PROBE_Throttle(&chan_chan[s1->number & 7], 60, 25)) {
                CHAN_PROBE(
                    "anim:chan",
                    "ent=%d anim=%d(%d) torso=%d(%d) blendL=%d blendT=%d legsLive=%d torsoLive=%d "
                    "frame=%d oldframe=%d lerp=%.2f cblerp=%.2f tiki=%d\n",
                    s1->number,
                    s1->anim,
                    s1->anim & ANIM_MASK,
                    s1->torso_anim,
                    s1->torso_anim & ANIM_MASK,
                    (s1->anim & ANIM_BLEND) ? 1 : 0,
                    (s1->torso_anim & ANIM_BLEND) ? 1 : 0,
                    model->legsLive,
                    model->torsoLive,
                    model->frame,
                    model->oldframe,
                    model->framelerp,
                    model->crossblend_lerp,
                    tiki
                );
            }
        }
    }

    return qtrue;
}

#define SHADOW_DISTANCE 96

qboolean CG_EntityShadow(centity_t *cent, refEntity_t *model)
{
    float   alpha;
    float   fWidth;
    vec3_t  end;
    trace_t trace;

    model->shadowPlane = 0.0f;

    if (cg_shadows->integer == 0) {
        return qfalse;
    }

    if (model->renderfx & RF_SKYENTITY) {
        return qfalse;
    }


    VectorCopy(model->origin, end);
    end[2] -= SHADOW_DISTANCE;

    cgi.CM_BoxTrace(&trace, model->origin, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse);

    if (trace.fraction == 1.0) {
        return qfalse;
    }

    model->shadowPlane = trace.endpos[2] + 1.0f;

    if (trace.startsolid || trace.allsolid) {
        return qfalse;
    }

    if ((cg_shadows->integer == 2) && (model->renderfx & RF_SHADOW_PRECISE)) {
        return qtrue;
    }

    // f6 is never rewritten before the `bl CG_ImpactMark' at 0x340b4, and the callee's
    // retail's is 50-70 -- the "character shadows do not look like retail" size defect.
    fWidth = model->scale * cgi.R_ModelRadius(model->hModel);
    if (fWidth < 1) {
        return qfalse;
    }

    alpha = (1.0 - trace.fraction) * 0.65f;

    CG_ImpactMark(
        cgs.media.shadowMarkShader,
        trace.endpos,
        trace.plane.normal,
        cent->lerpAngles[YAW],
        alpha,
        alpha,
        alpha,
        1.0f,
        0,                 
        fWidth,             
        qtrue,             
        -1,                 
        qfalse              
    );

    return qtrue;
}


void CG_AnimationDebugMessage(int number, const char *fmt, ...)
{
    (void)number;
    (void)fmt;
}

void CG_AttachEntity(
    refEntity_t *entity, refEntity_t *parent, int tiki, int tagnum, qboolean use_angles, vec3_t attach_offset,
    vec3_t attach_angles_offset
)
{
    int i;
    orientation_t or ;
    vec3_t tempAxis[3];

    if (r_lerpmodels->integer) {
        cgi.TIKI_Tag_LerpOrientation(&or, tiki, parent, tagnum);
    } else {
        cgi.TIKI_Tag_OrientationEx(
            &or,
            tiki,
            parent->anim,
            parent->frame,
            tagnum & TAG_MASK,
            parent->scale,
            parent->bone_tag,
            parent->bone_quat,
            0,
            0,
            1.0f,
            (parent->anim & ANIM_BLEND) != 0,
            (parent->torso_anim & ANIM_BLEND) != 0,
            parent->torso_anim,
            parent->torso_frame,
            0,
            0,
            1.0f
        );
    }

    VectorCopy(parent->origin, entity->origin);

    for (i = 0; i < 3; i++) {
        VectorMA(entity->origin, or.origin[i], parent->axis[i], entity->origin);
    }

    // the origin at 0x34C48-0x34C78, so the two are NOT the same value whenever
    VectorCopy(entity->origin, entity->attachOrigin);

    MatrixMultiply(or.axis, parent->axis, tempAxis);
    {
        vec3_t vOffset;

        MatrixTransformVector(attach_offset, tempAxis, vOffset);
        VectorAdd(entity->origin, vOffset, entity->origin);
    }

    if (use_angles) {
        MatrixMultiply(entity->axis, parent->axis, tempAxis);
        MatrixMultiply(or.axis, tempAxis, entity->axis);
    } else {
        vec3_t angAxis[3];
        MatrixMultiply(or.axis, parent->axis, tempAxis);
        AnglesToAxis(attach_angles_offset, angAxis);
        MatrixMultiply(angAxis, tempAxis, entity->axis);
    }

    entity->scale *= parent->scale;

    entity->renderfx |= (parent->renderfx & ~(RF_FLAGS_NOT_INHERITED | RF_LIGHTING_ORIGIN));

    VectorCopy(parent->lightingOrigin, entity->lightingOrigin);
    entity->shadowPlane = parent->shadowPlane;
}

void CG_ServerModelLoaded(const char *name, qhandle_t handle)
{
}

void CG_ServerModelUnloaded(qhandle_t handle)
{
}

void CalcModelAlpha(refEntity_t *model, int tiki)
{
    float  fadeMod;
    float  fadeEnd, fadeStart;
    float  dist, scaled;
    vec3_t delta;

    fadeMod = 1.0f;
    if (tiki >= 0) {
        fadeMod = cgi.TIKI_GetFadeDistMod(tiki);
    }

    model->fade = 1.0f;

    if (model->renderfx & RF_SKYENTITY) {
        return;
    }

    fadeEnd = fadeMod * (cg.refdef.entity_fade_dist * r_fadeDistMod->value);
    if (fadeEnd <= 0.0f) {
        return;
    }

    fadeStart = 0.9f * fadeEnd;

    VectorSubtract(cg.refdef.vieworg, model->origin, delta);
    dist = VectorLength(delta);
    scaled = dist * tan(cg.refdef.fov_x * (M_PI / 360.0));

    if (scaled > fadeEnd) {
        model->fade = 0.0f;
    } else if (scaled >= fadeStart) {
        model->fade = 1.0f - (scaled - fadeStart) / (fadeEnd - fadeStart);
    }
}

void CG_ModelAnim(centity_t *cent, qboolean bDoShaderTime)
{
    int tiki;
    entityState_t *s1;
    refEntity_t    model;
    refEntity_t    sprite;
    int            i;
    int            modelindex;
    vec3_t         vMins, vMaxs, vTmp;
    vec3_t         vDelta;
    vec3_t         vAngles;
    char           szIconName[256];
    qboolean       bThirdPerson = qfalse;

    s1 = &cent->currentState;

    bThirdPerson |= cg_3rd_person->integer ? qtrue : qfalse;

    // CG_CalcViewModelMovement, does not exist in retail (no such symbol in
    modelindex = s1->modelindex;
    if ((s1->renderfx & RF_WEAPONMODEL) && s1->parent == cg.snap->ps.clientNum
        && !cg_3rd_person->integer && cg.snap->ps.pm_type != PM_SECRET_MOVE_MODE
        && cg.snap->ps.pm_type != PM_3RD_PERSON) {
        s1->renderfx |= RF_DEPTHHACK;
        modelindex = s1->viewmodelindex;
    } else if (s1->clientNum >= 0 && cgs.model_draw[modelindex] < 0) {
        modelindex = s1->viewmodelindex;
    }

    if ((cg.snap->ps.pm_flags & PMF_INTERMISSION) && s1->number == cg.snap->ps.clientNum && !bThirdPerson) {
        return;
    }

    memset(&model, 0, sizeof(model));

    if (s1->loopSound && (s1->parent == ENTITYNUM_NONE)) {
        cgi.S_AddLoopingSound(
            cent->lerpOrigin,
            vec3_origin,
            cgs.sound_precache[s1->loopSound],
            s1->loopSoundVolume,
            s1->loopSoundMinDist,
            s1->number,
            0,
            0
        );
    }

    if (cent->tikiLoopSound && (s1->parent == ENTITYNUM_NONE)) {
        cgi.S_AddLoopingSound(
            cent->lerpOrigin,
            vec3_origin,
            cent->tikiLoopSound,
            cent->tikiLoopSoundVolume,
            cent->tikiLoopSoundMinDist,
            s1->number,
            0,
            0
        );
    }

    if (s1->renderfx & RF_SKYORIGIN) {
        AnglesToAxis(cent->lerpAngles, cg.sky_axis);
        VectorCopy(cent->lerpOrigin, cg.sky_origin);
    }

    if (!modelindex) {
        return;
    }

    model.entityNumber = s1->number;

    VectorCopy(cent->lerpOrigin, model.origin);

    IntegerToBoundingBox(s1->solid, vMins, vMaxs);
    VectorAdd(vMins, vMaxs, vTmp);
    VectorMA(model.origin, 0.5, vTmp, model.lightingOrigin);
    VectorSubtract(vMins, vMaxs, vTmp);
    model.radius = VectorLength(vTmp) * 0.5;

    //     uninitialised client-side -- an uninitialised read that retail never performs.
    for (i = 0; i < NUM_BONE_CONTROLLERS; i++) {
        model.bone_tag[i] = s1->bone_tag[i];

        if (s1->bone_tag[i] >= 0) {
            model.boneTagGate = 1;
            if (cent->interpolate && cent->nextState.bone_tag[i] == s1->bone_tag[i]) {
                SlerpQuaternion(s1->bone_quat[i], cent->nextState.bone_quat[i], cg.frameInterpolation, model.bone_quat[i]);
            } else {
                model.bone_quat[i][0] = s1->bone_quat[i][0];
                model.bone_quat[i][1] = s1->bone_quat[i][1];
                model.bone_quat[i][2] = s1->bone_quat[i][2];
                model.bone_quat[i][3] = s1->bone_quat[i][3];
            }
        }
    }

    AnglesToAxis(cent->lerpAngles, model.axis);

    model.tag_num  = s1->tag_num;
    model.skin_num = s1->skinNum;

    if (bDoShaderTime) {
        model.shaderTime = cg.time / 1000.0;
    }

    if (cent->interpolate) {
        model.scale = s1->scale + cg.frameInterpolation * (cent->nextState.scale - s1->scale);
    } else {
        model.scale = s1->scale;
    }


    model.hModel = cgs.model_draw[modelindex];
    tiki         = cgi.R_Model_GetHandle(model.hModel);
    model.tiki   = CG_TikiFromHandle(tiki);

    if (!model.tiki) {
        if (model.hModel) {
            model.skinNum = s1->skinNum;
            model.renderfx |= s1->renderfx;

            for (i = 0; i < 3; i++) {
                model.shaderRGBA[i] = cent->color[i] * 255;
            }
            model.shaderRGBA[3] = s1->alpha * 255;

            model.fade = 1.0f; 
            cgi.R_AddRefEntityToScene(&model);
        }
        return;
    }

    model.skinNum = s1->skinNum;
    model.renderfx |= s1->renderfx;

    CG_InterpolateAnimParms(cent, &model);

    CG_EF2ModelAnim(cent, &model);

    if (cent->currentState.parent != ENTITYNUM_NONE) {
        refEntity_t *parent;

        parent = cgi.R_GetRenderEntity(s1->parent);
        if (!parent) {
            cgi.Com_WDPrintf("CG_ModelAnim: Could not find parent entity\n");

            {
                static int widget_orphanm;

                if (WIDGET_On() && WIDGET_Throttle(&widget_orphanm, 4, 32)) {
                    WIDGET_A1_PROBE(
                        "tag:orphan",
                        "MODEL child=%d etype=%d modelindex=%d parent=%d tagnum=%d cg.time=%d\n",
                        s1->number,
                        s1->eType,
                        s1->modelindex,
                        s1->parent,
                        s1->tag_num & TAG_MASK,
                        cg.time);
                }
            }
            return;
        }

        tiki = cgi.R_Model_GetHandle(parent->hModel);
        {
            static int widget_modelatt;

            if (WIDGET_On() && WIDGET_Throttle(&widget_modelatt, 8, 600)) {
                orientation_t widget_or;

                cgi.TIKI_Tag_LerpOrientation(&widget_or, tiki, parent, s1->tag_num & TAG_MASK);
                WIDGET_A1_PROBE(
                    "tag:modelattach",
                    "child=%d parent=%d tagnum=%d or=(%.2f,%.2f,%.2f) porg=(%.1f,%.1f,%.1f) "
                    "useang=%d off=(%.1f,%.1f,%.1f) ang=(%.1f,%.1f,%.1f) model=%d cg.time=%d\n",
                    s1->number,
                    s1->parent,
                    s1->tag_num & TAG_MASK,
                    widget_or.origin[0],
                    widget_or.origin[1],
                    widget_or.origin[2],
                    parent->origin[0],
                    parent->origin[1],
                    parent->origin[2],
                    s1->attach_use_angles,
                    s1->attach_offset[0],
                    s1->attach_offset[1],
                    s1->attach_offset[2],
                    s1->attach_angles_offset[0],
                    s1->attach_angles_offset[1],
                    s1->attach_angles_offset[2],
                    modelindex,
                    cg.time);
            }
        }
        CG_AttachEntity(&model, parent, tiki, s1->tag_num & TAG_MASK, s1->attach_use_angles, s1->attach_offset, s1->attach_angles_offset);

        if ((s1->renderfx & RF_WEAPONMODEL) && (parent->renderfx & RF_THIRD_PERSON)
            && !(cg.snap->ps.pm_flags & PMF_CAMERA_VIEW)) {
            model.renderfx &= ~RF_THIRD_PERSON;

            float dist;
            float fade;
            float bobAmt;
            float dip;

            dist = (float)(cg.time - cg.landTime);

            fade = 0.0f;
            if (dist < 150.0f) {
                fade = cg.landChange * (dist / 150.0f);
            } else if (dist < 450.0f) {
                fade = cg.landChange * (1.0f - (dist - 150.0f) / 300.0f);
            }

            bobAmt = 0.25f * ef2_bobfracsin * cg_viewWeaponBob->value;
            fade *= 0.25f;

            for (i = 0; i < 3; i++) {
                model.origin[i] = cg.refdef.vieworg[i] + cg.refdef.viewaxis[1][i] * bobAmt;
            }

            dip = (fade + fabs(bobAmt)) - 1.0f;
            for (i = 0; i < 3; i++) {
                model.origin[i] += cg.refdef.viewaxis[2][i] * dip;
            }

            model.scale = 1.0f;

            AxisCopy(cg.refdef.viewaxis, model.axis);

            {
                vec3_t          ang;
                const float    *crosshairOffset = cgi.get_crosshairoffset();

                AxisToAngles( model.axis, ang );
                ang[0] += ( 40.0f * crosshairOffset[1] ) / 300.0f;
                ang[1] += ( 40.0f * -crosshairOffset[0] ) / 200.0f;
                AnglesToAxis( ang, model.axis );
            }
        }

        if (s1->loopSound) {
            cgi.S_AddLoopingSound(
                model.origin,
                vec3_origin,
                cgs.sound_precache[s1->loopSound],
                s1->loopSoundVolume,
                s1->loopSoundMinDist,
                s1->number,
                0,
                0
            );
        }

        if (cent->tikiLoopSound) {
            cgi.S_AddLoopingSound(
                cent->lerpOrigin,
                vec3_origin,
                cent->tikiLoopSound,
                cent->tikiLoopSoundVolume,
                cent->tikiLoopSoundMinDist,
                s1->number,
                0,
                0
            );
        }

        // DELETED (MoHAA residue, not retail): the block that used to sit here cleared
    }

    for (i = 0; i < 3; i++) {
        model.shaderRGBA[i] = cent->color[i] * 255;
    }
    model.shaderRGBA[3] = s1->alpha * 255;

    memcpy(model.surfaces, s1->surfaces, MAX_MODEL_SURFACES);

    CalcModelAlpha(&model, cgi.R_Model_GetHandle(model.hModel));

    if (s1->number == cg.snap->ps.clientNum
        && !cg_3rd_person->integer
        && cg.snap->ps.stats[STAT_HEALTH] > 0
        && cg.snap->ps.pm_type != PM_SPECTATOR_FOLLOW
        && cg.snap->ps.pm_type != PM_SECRET_MOVE_MODE
        && cg.snap->ps.pm_type != PM_3RD_PERSON) {
        model.renderfx |= RF_THIRD_PERSON;
    }

    if ((s1->eFlags & EF_EMITTER_CUSTOM) && s1->customEmitter) {
        CG_TurnEmitterOn(CG_ConfigString(CS_IMAGES + cent->nextState.customEmitter), (entityState_t *)cent);
    }

    // @0x001A8450, verbatim in the SDK source) never zeroes
    if ((s1->eFlags & EF_EFFECT_CUSTOM) && s1->customShader) {
        model.customShader = cgi.R_RegisterShader(CG_ConfigString(CS_IMAGES + s1->customShader));
        model.renderfx |= RF_CUSTOMSHADERPASS;
    } else if (s1->parent != ENTITYNUM_NONE) {
        // A child inherits the parent's overlay, never its own (retail puts this in
        const refEntity_t *parentEnt = cgi.R_GetRenderEntity(s1->parent);

        if (parentEnt && (parentEnt->renderfx & RF_CUSTOMSHADERPASS)) {
            model.customShader = parentEnt->customShader;
            model.renderfx |= RF_CUSTOMSHADERPASS;
        }
    }

    if (s1->eFlags & EF_EFFECT_ELECTRIC) {
        model.customShader = cgi.R_RegisterShader("electriclines");
        model.renderfx |= RF_CUSTOMSHADERPASS;
    }

    if (s1->eFlags & EF_BEHAVIOR_FAILURE) {
        model.customShader = cgi.R_RegisterShader("failure");
        model.renderfx |= RF_CUSTOMSHADERPASS;
    }

    if (s1->affectingViewModes) {
        unsigned int viewModeMask = cg.snap->ps.viewMode & s1->affectingViewModes;

        if (viewModeMask) {
            vec3_t viewModeColor;

            if (cgi.GetViewModeEntityColor(viewModeMask, viewModeColor)) {
                model.shaderRGBA[0] = (byte)(viewModeColor[0] * 255.0f);
                model.shaderRGBA[1] = (byte)(viewModeColor[1] * 255.0f);
                model.shaderRGBA[2] = (byte)(viewModeColor[2] * 255.0f);
                model.renderfx |= RF_LIGHTOFFSET;
            }

            {
                const char *viewModeShader = NULL;

                if (cgi.GetViewModeEntityShader(viewModeMask, &viewModeShader)) {
                    model.customShader = cgi.R_RegisterShader(viewModeShader);
                    model.renderfx |= RF_CUSTOMSHADERPASS;
                }
            }

            {
                float viewModeAlpha = 1.0f;

                if (cgi.GetViewModeEntityAlpha(viewModeMask, &viewModeAlpha)) {
                    if (viewModeAlpha < 1.0f) {
                        model.renderfx |= RF_FORCE_ALPHA;
                    }
                    model.shaderRGBA[3] = (byte)(viewModeAlpha * 255.0f);
                }
            }
        }
    }


    if (s1->number == cg.snap->ps.clientNum && s1->parent == ENTITYNUM_NONE && !bThirdPerson) {
        for (i = 0; i < MAX_MODEL_SURFACES; i++) {
            model.surfaces[i] |= MDL_SURFACE_NODRAW;
        }
    }

    if (!(s1->renderfx & RF_DONTDRAW) && (model.renderfx & RF_SHADOW) && cg_shadows->integer
        && !CG_SimpleDistanceCull(model.origin, Square(1400))
        && !CG_FrustumCullSphere(model.origin, model.radius + 64)) {
        CG_EntityShadow(cent, &model);
    }

    cent->bFootOnGround_Left  = qtrue;
    cent->bFootOnGround_Right = qtrue;


    if (s1->number == cg.snap->ps.clientNum) {
        if ((!cg.bFPSModelLastFrame && !bThirdPerson) || (cg.bFPSModelLastFrame && bThirdPerson)) {
            cg.bFPSModelLastFrame = !bThirdPerson;
        }


        if (!bThirdPerson) {


            if (cg.pLastPlayerWorldModel < 0 || cg.pLastPlayerWorldModel != tiki) {
                cg.hPlayerFPSModelHandle = cgs.model_draw[modelindex];
                cg.pPlayerFPSModel       = tiki;
                cg.pLastPlayerWorldModel = tiki;
            }

            for (i = 0; i < MAX_MODEL_SURFACES; i++) {
                model.surfaces[i] |= MDL_SURFACE_NODRAW;
            }
        }
    }

    model.reType = RT_MODEL;

    if (s1->infoIcon > 0
        && s1->infoIcon != cg.snap->ps.stats[STAT_MP_OTHERTEAM_ICON]
        && !(s1->renderfx & RF_DONTDRAW)
        && !(model.renderfx & RF_THIRD_PERSON)) {
        memset(&sprite, 0, sizeof(sprite));

        VectorCopy(cent->lerpOrigin, sprite.origin);
        VectorCopy(cent->lerpOrigin, sprite.attachOrigin);

        IntegerToBoundingBox(s1->solid, vMins, vMaxs);

        sprite.origin[2] += 32.0f + vMaxs[2];
        sprite.attachOrigin[2] += 32.0f + vMaxs[2];

        strcpy(szIconName, CG_ConfigString(s1->infoIcon + CS_IMAGES));
        strcat(szIconName, ".spr");

        sprite.reType = RT_SPRITE;                         
        sprite.hModel = cgi.R_RegisterModel(szIconName);   

        sprite.shaderTime = -(float)cg.time;

        sprite.shaderRGBA[0] = 255;                        
        sprite.shaderRGBA[1] = 255;
        sprite.shaderRGBA[2] = 255;
        sprite.shaderRGBA[3] = 255;

        VectorSubtract(sprite.origin, cg.refdef.vieworg, vDelta);
        vectoangles(vDelta, vAngles);
        AnglesToAxis(vAngles, sprite.axis);

        sprite.scale = 1.0f;

        cgi.R_AddRefSpriteToScene(&sprite);                
    }

    if (!(s1->renderfx & RF_DONTDRAW)) {
        if (VectorCompare(model.origin, vec3_origin)) {
            VectorCopy(s1->origin, model.origin);
            AngleVectors(s1->angles, model.axis[0], model.axis[1], model.axis[2]);
        }


        cgi.R_AddRefEntityToScene(&model);
    }

    if (model.tiki) {
        int iAnim, iFrame;

        CG_UpdateEntityEmitters(cgi.R_Model_GetHandle(model.hModel), &model, cent);

        if (s1->frame & FRAME_EXPLICIT) {
            iAnim  = s1->anim & ANIM_MASK;
            iFrame = (short)(s1->frame & FRAME_MASK);
        } else {
            iAnim  = cent->animstate[0].base.anim & ANIM_MASK;
            iFrame = (short)cent->animstate[0].base.frame;
        }
        CG_ClientCommands(model.tiki ? cgi.R_Model_GetHandle(model.hModel) : -1, iFrame, iAnim, &cent->animstate[0], &model, cent);

        if (s1->torso_anim & ANIM_BLEND) {
            if (s1->torso_frame & FRAME_EXPLICIT) {
                iAnim  = s1->torso_anim & ANIM_MASK;
                iFrame = (short)(s1->torso_frame & FRAME_MASK);
            } else {
                iAnim  = cent->animstate[1].base.anim & ANIM_MASK;
                iFrame = (short)cent->animstate[1].base.frame;
            }
            CG_ClientCommands(model.tiki ? cgi.R_Model_GetHandle(model.hModel) : -1, iFrame, iAnim, &cent->animstate[1], &model, cent);
        }
    }
}

