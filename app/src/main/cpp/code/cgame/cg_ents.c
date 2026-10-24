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
#include "cg_radar.h"

#define WIDGET_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define WIDGET_PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_widget.h"

#define PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_probe.h"
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif


void CG_SetEntitySoundPosition(centity_t *cent)
{
    vec3_t origin;

    if (cent->currentState.solid == SOLID_BMODEL) {
        float *v;
        vec3_t vel;

        v = cgs.inlineModelMidpoints[cent->currentState.modelindex];
        VectorAdd(cent->lerpOrigin, v, origin);

        vel[0] = 0.0;
        vel[1] = 0.0;
        vel[2] = 0.0;

        cgi.S_UpdateEntity(cent->currentState.number, origin, vel, qfalse);
    } else {
        if (cent && cg.snap && cent->currentState.parent == cg.snap->ps.clientNum) {
            vec3_t localOrigin;
            vec3_t velocity;

            localOrigin[0] = 0;
            localOrigin[1] = 0;
            localOrigin[2] = 0;

            velocity[0] = 0;
            velocity[1] = 0;
            velocity[2] = 0;

            cgi.S_UpdateEntity(cent->currentState.number, localOrigin, velocity, qtrue);
        } else {
            CG_GetOrigin(cent, origin);
            cgi.S_UpdateEntity(cent->currentState.number, origin, cent->currentState.pos.trDelta, qfalse);
        }
    }
}

void CG_EntityEffects(centity_t *cent)
{
    cent->color[0] = cent->client_color[0];
    cent->color[1] = cent->client_color[1];
    cent->color[2] = cent->client_color[2];
    cent->color[3] = cent->client_color[3];

    if (cent->currentState.constantLight != 0xffffff) {
        int      style;
        unsigned cl;
        float    i, r, g, b;

        cl    = cent->currentState.constantLight;
        style = (cl & 255);
        r     = (float)style / 255.0f;
        g     = (float)((cl >> 8) & 255) / 255.0f;
        b     = (float)((cl >> 16) & 255) / 255.0f;
        i     = ((cl >> 24) & 255) * CONSTANTLIGHT_RADIUS_SCALE;
        if (cent->currentState.renderfx & RF_LIGHTSTYLE_DLIGHT) {
            float color[4];

            CG_LightStyleColor(style, cg.time, color, qfalse);
            r = color[0];
            g = color[1];
            b = color[2];
            i *= color[3];
        }
        if (i) {
            int flags;

            flags = 0;
            if (cent->currentState.renderfx & RF_LENSFLARE) {
                flags |= lensflare;
            } else if (cent->currentState.renderfx & RF_VIEWLENSFLARE) {
                flags |= viewlensflare;
            }
            if (cent->currentState.renderfx & RF_ADDITIVE_DLIGHT) {
                flags |= additive;
            }
            if (cent->currentState.parent == ENTITYNUM_NONE) {
                cgi.R_AddLightToScene(cent->lerpOrigin, i, r, g, b, flags);
            } else {
                vec3_t origin;
                CG_GetOrigin(cent, origin);
                cgi.R_AddLightToScene(origin, i, r, g, b, flags);
            }
        }
        if (r < cent->color[0]) {
            cent->color[0] = r;
        }
        if (g < cent->color[1]) {
            cent->color[1] = g;
        }
        if (b < cent->color[2]) {
            cent->color[2] = b;
        }
    }
}

void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out)
{
    centity_t     *moverCent;
    entityState_t *moverState;
    vec3_t         oldOrigin, newOrigin;
    vec3_t         oldAngles, newAngles;
    float          f;
    int            i;

    if (moverNum == ENTITYNUM_NONE) {
        VectorCopy(in, out);
        return;
    }

    moverCent  = &cg_entities[moverNum];
    moverState = &moverCent->currentState;

    f = cg.frameInterpolation;
    if (cg.nextSnap) {
        float serverDelta = (float)(cg.nextSnap->serverTime - cg.snap->serverTime);

        if (cg.frameInterpolation != serverDelta) {
            f = (float)(toTime - fromTime) / serverDelta;
        }
    }

    if (moverState->pos.trType == TR_LERP) {
        if (moverCent->interpolate) {
            for (i = 0; i < 3; i++) {
                moverCent->lerpOrigin[i] =
                    f * (moverCent->nextState.origin[i] - moverState->origin[i]) + moverState->origin[i];
            }
        }

        for (i = 0; i < 3; i++) {
            out[i] = in[i] + (moverCent->lerpOrigin[i] - moverState->origin[i]);
        }
    } else if (moverState->eType == ET_MOVER) {
        BG_EvaluateTrajectory(&moverState->pos, fromTime, oldOrigin);
        BG_EvaluateTrajectory(&moverState->apos, fromTime, oldAngles);
        BG_EvaluateTrajectory(&moverState->pos, toTime, newOrigin);
        BG_EvaluateTrajectory(&moverState->apos, toTime, newAngles);

        for (i = 0; i < 3; i++) {
            out[i] = in[i] + (newOrigin[i] - oldOrigin[i]);
        }
    } else {
        VectorCopy(in, out);
    }
}

void CG_Item(centity_t *cent)
{
    entityState_t *s1;
    qhandle_t      shader;
    qhandle_t      handle;
    vec3_t         mins, maxs;
    vec3_t         centre, rot;
    float          radius;
    float          rate;
    float          halfSpan;
    float          amount;
    float          lift;
    float          orgX, orgY;
    float          arg;
    float          size1, size2;
    float          z1, z2;
    int            i;
    polyVert_t     verts[4];

    s1 = &cent->currentState;

    if (!s1->modelindex || s1->parent != ENTITYNUM_NONE || (s1->renderfx & RF_DONTDRAW)) {
        return;
    }

    shader = cent->itemRingShader;
    if (!shader) {
        shader = cgs.media.itemRingShaderDefault;
    }

    VectorCopy(cg.autoAngles, cent->lerpAngles);

    // ---- 0x2EFF0-0x2F004: the guard is on cgs.model_tiki[], not cgs.model_draw[] --
    if (cgs.model_tiki[s1->modelindex] == -1) {
        return;
    }

    handle = cgs.model_draw[s1->modelindex];        

    radius = cgi.R_ModelRadius(handle);
    cgi.R_ModelBounds(handle, mins, maxs);

    //   [EVENT:K2:halfspan] The fmuls ORDER is load-bearing, not cosmetic: 0x2F02C
    halfSpan = s1->scale * (0.5f * radius);

    amount = 0.2f * halfSpan;

    lift = cent->lerpOrigin[2] + halfSpan;
    cent->lerpOrigin[2] = lift;

    rate = 1.0e-5f * s1->number + 0.005f;
    cent->lerpOrigin[2] += amount * (cosf((cg.time + 1000.0f) * rate) + 1.0f);

    orgX = cent->lerpOrigin[0];
    orgY = cent->lerpOrigin[1];

    centre[0] = (mins[0] + maxs[0]) * (0.5f * s1->scale);
    centre[1] = (mins[1] + maxs[1]) * (0.5f * s1->scale);
    centre[2] = (mins[2] + maxs[2]) * (0.5f * s1->scale);
    MatrixTransformVector(centre, cg.autoAxis, rot);
    cent->lerpOrigin[0] -= rot[0];       
    cent->lerpOrigin[1] -= rot[1];       
    cent->lerpOrigin[2] -= rot[2];       

    if (!shader) {
        return;
    }

    arg = (cg.time + 1000.0f) * (rate * 0.5f);

    z1    = lift + halfSpan * cosf(arg);
    size1 = 1.5f * (halfSpan - fabsf(0.5f * halfSpan * cosf(arg)));

    verts[0].xyz[0] = orgX - size1; verts[0].xyz[1] = orgY - size1; verts[0].xyz[2] = z1;
    verts[1].xyz[0] = orgX - size1; verts[1].xyz[1] = orgY + size1; verts[1].xyz[2] = z1;
    verts[2].xyz[0] = orgX + size1; verts[2].xyz[1] = orgY + size1; verts[2].xyz[2] = z1;
    verts[3].xyz[0] = orgX + size1; verts[3].xyz[1] = orgY - size1; verts[3].xyz[2] = z1;
    verts[0].st[0] = 0.0f; verts[0].st[1] = 0.0f;   
    verts[1].st[0] = 0.0f; verts[1].st[1] = 1.0f;   
    verts[2].st[0] = 1.0f; verts[2].st[1] = 1.0f;
    verts[3].st[0] = 1.0f; verts[3].st[1] = 0.0f;
    for (i = 0; i < 4; i++) {
        verts[i].modulate[0] = 255;                  
        verts[i].modulate[1] = 255;
        verts[i].modulate[2] = 255;
        verts[i].modulate[3] = 255;
    }

    cgi.R_AddPolyToScene(shader, 4, verts, 0);

    z2    = lift + halfSpan * cosf(arg + (float)M_PI);
    size2 = 1.5f * (halfSpan - fabsf(0.5f * halfSpan * cosf(arg + (float)M_PI)));

    verts[0].xyz[0] = orgX - size2; verts[0].xyz[1] = orgY - size2; verts[0].xyz[2] = z2;
    verts[1].xyz[0] = orgX - size2; verts[1].xyz[1] = orgY + size2; verts[1].xyz[2] = z2;
    verts[2].xyz[0] = orgX + size2; verts[2].xyz[1] = orgY + size2; verts[2].xyz[2] = z2;
    verts[3].xyz[0] = orgX + size2; verts[3].xyz[1] = orgY - size2; verts[3].xyz[2] = z2;

    cgi.R_AddPolyToScene(shader, 4, verts, 0);

}

void CG_Sprite(centity_t *cent)
{
    refEntity_t    ent;
    entityState_t *s1;

    s1 = &cent->currentState;

    memset(&ent, 0, sizeof(ent));

    if (s1->parent == ENTITYNUM_NONE) {
        VectorCopy(cent->lerpOrigin, ent.origin);
        VectorCopy(cent->lerpOrigin, ent.attachOrigin);  
    } else {
        refEntity_t *parent = cgi.R_GetRenderEntity(s1->parent);

        if (!parent) {
            cgi.Com_WDPrintf("CG_Sprite: Could not find parent entity\n");

            {
                static int widget_orphan;

                if (WIDGET_On() && WIDGET_Throttle(&widget_orphan, 4, 32)) {
                    WIDGET_A1_PROBE(
                        "tag:orphan",
                        "child=%d etype=%d modelindex=%d parent=%d tagnum=%d use_angles=%d cg.time=%d\n",
                        cent->currentState.number,
                        cent->currentState.eType,
                        cent->currentState.modelindex,
                        s1->parent,
                        s1->tag_num & 0x3FF,
                        s1->attach_use_angles,
                        cg.time);
                }
            }
            return;
        }

        {
            static int widget_att;

            if (WIDGET_On() && WIDGET_Throttle(&widget_att, 4, 64)) {
                orientation_t widget_or;
                int           widget_lerp = r_lerpmodels->integer;

                if (widget_lerp) {
                    cgi.TIKI_Tag_LerpOrientation(&widget_or, cgi.R_Model_GetHandle(parent->hModel), parent,
                                                 s1->tag_num & 0x3FF);
                } else {
                    VectorClear(widget_or.origin);
                }

                WIDGET_A1_PROBE(
                    "tag:attach",
                    "child=%d parent=%d tagnum=%d lerp=%d or=(%.1f,%.1f,%.1f) "
                    "porg=(%.1f,%.1f,%.1f) corg=(%.1f,%.1f,%.1f) plegslive=%d ptorsolive=%d\n",
                    cent->currentState.number,
                    s1->parent,
                    s1->tag_num & 0x3FF,
                    widget_lerp,
                    widget_or.origin[0], widget_or.origin[1], widget_or.origin[2],
                    parent->origin[0], parent->origin[1], parent->origin[2],
                    cent->lerpOrigin[0], cent->lerpOrigin[1], cent->lerpOrigin[2],
                    parent->legsLive,
                    parent->torsoLive);
            }
        }

        CG_AttachEntity(&ent, parent, cgi.R_Model_GetHandle(parent->hModel),
                       s1->tag_num & 0x3FF, s1->attach_use_angles,
                       s1->attach_offset, s1->attach_angles_offset);
    }

    ent.hModel = cgs.model_draw[s1->modelindex];

    ent.shaderRGBA[0] = (int)(255.0f * cent->color[0]);
    ent.shaderRGBA[1] = (int)(255.0f * cent->color[1]);
    ent.shaderRGBA[2] = (int)(255.0f * cent->color[2]);

    if (s1->alpha < 1.0f) {
        ent.shaderRGBA[3] = (int)(255.0f * s1->alpha);
    } else {
        ent.shaderRGBA[3] = (int)(255.0f * cent->color[3]);
    }

    ent.renderfx = s1->renderfx;

    AnglesToAxis(cent->lerpAngles, ent.axis);

    ent.radius = s1->scale;

    if (!(s1->renderfx & RF_DONTDRAW)) {
        ent.fade = 1.0f; // not faded: retail computes the fade only in CG_ModelAnim
        cgi.R_AddRefEntityToScene(&ent);
    }
}

void CG_General(centity_t *cent)
{
    refEntity_t    ent;
    entityState_t *s1;
    int            i;
    vec3_t         vMins, vMaxs, vTmp;

    s1 = &cent->currentState;

    // Pitch/Flags are never loaded by retail.
    if (s1->loopSound) {
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

    if (s1->renderfx & RF_SKYORIGIN) {
        AnglesToAxis(cent->lerpAngles, cg.sky_axis);
        VectorCopy(cent->lerpOrigin, cg.sky_origin);
    }

    if (!s1->modelindex) {
        return;
    }

    // NOTE (borg): there is deliberately NO `if (s1->renderfx & RF_DONTDRAW) return;'
    memset(&ent, 0, sizeof(ent));


    VectorCopy(cent->lerpOrigin, ent.origin);

    IntegerToBoundingBox(s1->solid, vMins, vMaxs);
    VectorAdd(vMins, vMaxs, vTmp);
    VectorMA(ent.origin, 0.5f, vTmp, ent.lightingOrigin);
    VectorSubtract(vMins, vMaxs, vTmp);
    ent.radius = VectorLength(vTmp) * 0.5;

    ent.skinNum = s1->skinNum;

    ent.hModel = cgs.model_draw[s1->modelindex];

    memcpy(ent.surfaces, s1->surfaces, MAX_MODEL_SURFACES);

    for (i = 0; i < 3; i++) {
        ent.shaderRGBA[i] = cent->color[i] * 255;
    }

    if (s1->alpha < 1) {
        ent.shaderRGBA[3] = s1->alpha * 255;
    } else {
        ent.shaderRGBA[3] = cent->color[3] * 255;
    }

    AnglesToAxis(cent->lerpAngles, ent.axis);

    if (cent->interpolate) {
        ent.scale = s1->scale + cg.frameInterpolation * (cent->nextState.scale - cent->currentState.scale);
    } else {
        ent.scale = s1->scale;
    }
    ent.entityNumber = s1->number;

    ent.tag_num  = s1->tag_num;
    ent.skin_num = s1->skinNum;
    ent.renderfx |= s1->renderfx;

    ent.tiki         = CG_TikiFromHandle(cgi.R_Model_GetHandle(cgs.model_draw[s1->modelindex]));
    ent.actionWeight = 1.0;

    int tikiHandle = cgs.model_tiki[s1->modelindex];

    if (tikiHandle >= 0) {
        CG_UpdateEntityEmitters(tikiHandle, &ent, cent);
    }

    if ((s1->eFlags & EF_EMITTER_CUSTOM) && s1->customEmitter) {
        CG_TurnEmitterOn(CG_ConfigString(CS_IMAGES + cent->nextState.customEmitter), (entityState_t *)cent);
    }

    if ((s1->eFlags & EF_EFFECT_CUSTOM) && s1->customShader) {
        ent.customShader = cgi.R_RegisterShader(CG_ConfigString(CS_IMAGES + s1->customShader));
        ent.renderfx |= RF_CUSTOMSHADERPASS;
    } else if (s1->parent != ENTITYNUM_NONE) {
        const refEntity_t *parentEnt = cgi.R_GetRenderEntity(s1->parent);

        if (parentEnt && (parentEnt->renderfx & RF_CUSTOMSHADERPASS)) {
            ent.customShader = parentEnt->customShader;
            ent.renderfx |= RF_CUSTOMSHADERPASS;
        }
    }

    if (s1->eFlags & EF_EFFECT_ELECTRIC) {
        ent.customShader = cgi.R_RegisterShader("electriclines");
        ent.renderfx |= RF_CUSTOMSHADERPASS;
    }

    if (s1->eFlags & EF_BEHAVIOR_FAILURE) {
        ent.customShader = cgi.R_RegisterShader("failure");
        ent.renderfx |= RF_CUSTOMSHADERPASS;
    }

    CalcModelAlpha(&ent, tikiHandle);

    if (!(ent.renderfx & RF_DONTDRAW)) {
        cgi.R_AddRefEntityToScene(&ent);
    }
}

void CG_Speaker(centity_t *cent)
{
    if (!cent->currentState.clientNum) 
    {
        return; 
    }

    if (cg.time < cent->miscTime) {
        return;
    }


    cent->miscTime = cg.time + cent->currentState.clientNum * 100 * crandom();
}

void CG_Mover(centity_t *cent)
{
    refEntity_t    ent;
    entityState_t *s1;

    s1 = &cent->currentState;

    memset(&ent, 0, sizeof(ent));
    VectorCopy(cent->lerpOrigin, ent.origin);
    AnglesToAxis(cent->lerpAngles, ent.axis);

    ent.renderfx &= ~RF_SHADOW;

    ent.skinNum = (cg.time >> 6) & 1;

    if (s1->solid == SOLID_BMODEL) {
        ent.hModel = cgs.inlineDrawModel[s1->modelindex];
    } else {
        ent.hModel = cgs.model_draw[s1->modelindex];
    }

    ent.fade = 1.0f; // not faded: retail computes the fade only in CG_ModelAnim
    cgi.R_AddRefEntityToScene(&ent);
}

void CG_Beam(centity_t *cent)
{
    entityState_t *s1;
    vec3_t         vz = {0, 0, 0}, origin = {0, 0, 0};
    byte           modulate[4];
    int            i;

    s1 = &cent->currentState;

    for (i = 0; i < 4; i++) {
        modulate[i] = (byte)(cent->color[i] * 255.0f);
    }

    // The port copied s1->origin, which is not retail's start for either arm.
    if (s1->parent == ENTITYNUM_NONE) {
        VectorCopy(cent->lerpOrigin, origin);
    } else {
        centity_t *parent = &cg_entities[s1->parent];
        VectorAdd(cent->lerpOrigin, parent->lerpOrigin, origin);
    }

    CG_CreateBeam(
        origin,                                   
        vz,                                       
        s1->number,                               
        cgs.model_draw[s1->modelindex],           
        s1->alpha,                                
        s1->scale,                                
        s1->skinNum,                              
        0,                                        
        PKT_TO_BEAM_PARM(s1->surfaces[0]) * 1000, 
        qfalse,                                   
        s1->origin2,                              
        s1->bone_angles[0][0],                    
        s1->bone_angles[0][1],                    
        PKT_TO_BEAM_PARM(s1->surfaces[3]),        
        s1->surfaces[4],                          
        PKT_TO_BEAM_PARM(s1->surfaces[5]) * 1000, 
        CG_ConfigString(CS_IMAGES + s1->tag_num), 
        modulate,                                 
        s1->surfaces[6],                          
        PKT_TO_BEAM_PARM(s1->surfaces[7]),        
        PKT_TO_BEAM_PARM(s1->surfaces[8]),        
        PKT_TO_BEAM_PARM(s1->surfaces[9]),        
        s1->renderfx,
        "",
        1.0f,                                     
        qfalse,                                   
        0                                         
    );
}

/* qa_probe is desktop-only: the renderer's device property arms here too. */
static qboolean CG_DecalProbeOn( void )
{
    static int cached = -1;

    if ( cached < 0 ) {
        cached = PROBE_On() ? 1 : 0;
#ifdef __ANDROID__
        if ( !cached ) {
            char val[PROP_VALUE_MAX];

            if ( __system_property_get( "log.tag.ProvenanceEF2", val ) > 0
                 && val[0] != '\0' && Q_stricmp( val, "0" ) && Q_stricmp( val, "false" ) ) {
                cached = 1;
            }
        }
#endif
    }
    return cached ? qtrue : qfalse;
}

void CG_Decal(centity_t *cent)

{
    qhandle_t      shader;
    vec3_t         dir;
    entityState_t *s1;

    s1 = &cent->currentState;

    shader = cgi.R_RegisterShader(CG_ConfigString(CS_IMAGES + s1->tag_num));
    ByteToDir(s1->surfaces[0], dir);

    if ( CG_DecalProbeOn() ) {
        static int decal_rows;

        decal_rows++;
        if ( decal_rows <= 64 || ( decal_rows % 240 ) == 0 ) {
            UIVK_PROBE(
                "DOCC:decal",
                "ent=%d org=(%.1f,%.1f,%.1f) dirbyte=%d dir=(%.3f,%.3f,%.3f) scale=%f (0x%08x) roll=%.1f shader='%s'\n",
                cent->currentState.number,
                s1->origin[0], s1->origin[1], s1->origin[2],
                s1->surfaces[0],
                dir[0], dir[1], dir[2],
                s1->scale,
                *(unsigned int *)&s1->scale,
                s1->angles[2],
                CG_ConfigString(CS_IMAGES + s1->tag_num)
            );
        }
    }
    CG_ImpactMark(
        shader,
        s1->origin,
        dir,
        s1->angles[2],
        cent->color[0],
        cent->color[1],
        cent->color[2],
        cent->color[3],
        qtrue,             
        s1->scale,          
        qfalse,            
        -1,                
        qfalse             
    );
}

void CG_Portal(centity_t *cent) {}

void BG_EvaluateTrajectory(const trajectory_t *tr, int atTime, vec3_t result)
{
    float deltaTime;
    float t;

    // atTime = ...` -- is gone.  "cg_smoothClientsTime" is not one of retail cgame's 48 cvars
    switch (tr->trType) {
    case TR_LINEAR_STOP:
        if (atTime > tr->trTime + tr->trDuration) {
            atTime = tr->trTime + tr->trDuration;
        }
        deltaTime = (float)(atTime - tr->trTime) / 1000.0;
        if (deltaTime < 0.0f) {
            deltaTime = 0.0f;
        }
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        return;

    case TR_GRAVITY:
        deltaTime = (float)(atTime - tr->trTime) / 1000.0;
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        t = 0.5f * deltaTime;
        result[2] -= t * deltaTime;
        return;

    case TR_SINE:
        deltaTime = (float)(atTime - tr->trTime) / 1000.0;
        t = sin(deltaTime);
        VectorMA(tr->trBase, t, tr->trDelta, result);
        return;

    case TR_LINEAR:
        deltaTime = (float)(atTime - tr->trTime) / 1000.0;
        VectorMA(tr->trBase, deltaTime, tr->trDelta, result);
        return;

    case TR_STATIONARY:
    case TR_INTERPOLATE:
        VectorCopy(tr->trBase, result);
        return;

    default:
        Com_Error(ERR_DROP, "EvaluateTrajectory: unknown trType %i", tr->trType);
        break;
    }
}

void CG_CalcEntityLerpPositions(centity_t *cent)
{
    int   i;
    float f;

    f = cg.frameInterpolation;

    // eType never enters retail's decision.  The three-way split after it is on
    if (cg.snap->ps.pm_type < 2 && cent->currentState.number == cg.snap->ps.clientNum) {
        VectorCopy(cg.predicted_player_state.origin, cent->lerpOrigin);
        for (i = 0; i < 3; i++) {
            cent->lerpAngles[i] = LerpAngle(cent->currentState.angles[i], cent->nextState.angles[i], f);
        }

        return;
    }

    if (cent->currentState.pos.trType == TR_LERP) {
        float quat[4];
        float mat[3][3];

        if (!cent->interpolate) {
            VectorCopy(cent->currentState.angles, cent->lerpAngles);
            VectorCopy(cent->currentState.origin, cent->lerpOrigin);
            return;
        }

        if (!cg.nextSnap) {
            cgi.Error(ERR_DROP, "CG_CalcEntityLerpPositions: no next snapshot");
        }

        for (i = 0; i < 3; i++) {
            cent->lerpOrigin[i] =
                cent->currentState.origin[i] + f * (cent->nextState.origin[i] - cent->currentState.origin[i]);
        }

        if (!memcmp(cent->currentState.angles, cent->nextState.angles, sizeof(vec3_t))) {
            VectorCopy(cent->currentState.angles, cent->lerpAngles);
        } else {
            SlerpQuaternion(cent->currentState.quat, cent->nextState.quat, f, quat);
            QuatToMat(quat, mat);
            MatrixToEulerAngles(mat, cent->lerpAngles);
        }
    } else if (cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE) {
        vec3_t current, next;

        BG_EvaluateTrajectory(&cent->currentState.pos, cg.snap->serverTime, current);
        BG_EvaluateTrajectory(&cent->nextState.pos, cg.nextSnap->serverTime, next);

        cent->lerpOrigin[0] = current[0] + f * (next[0] - current[0]);
        cent->lerpOrigin[1] = current[1] + f * (next[1] - current[1]);
        cent->lerpOrigin[2] = current[2] + f * (next[2] - current[2]);

        BG_EvaluateTrajectory(&cent->currentState.apos, cg.snap->serverTime, current);
        BG_EvaluateTrajectory(&cent->nextState.apos, cg.nextSnap->serverTime, next);

        for (i = 0; i < 3; i++) {
            cent->lerpAngles[i] = LerpAngle(current[i], next[i], f);
        }
    } else {
        BG_EvaluateTrajectory(&cent->currentState.pos, cg.time, cent->lerpOrigin);
        BG_EvaluateTrajectory(&cent->currentState.apos, cg.time, cent->lerpAngles);
        CG_AdjustPositionForMover(
            cent->lerpOrigin, cent->currentState.groundEntityNum, cg.snap->serverTime, cg.time, cent->lerpOrigin);
    }

    {
        static int widget_k1_postype[ENTITYNUM_MAX_NORMAL];
        static int widget_k1_apostype[ENTITYNUM_MAX_NORMAL];
        static int widget_k1_nets[ENTITYNUM_MAX_NORMAL];
        static int widget_k1_angs[ENTITYNUM_MAX_NORMAL];
        static int widget_k1_ctr[8];
        int        slot = cent->currentState.number;
        int        nets, angs;

        if (slot >= 0 && slot < ENTITYNUM_MAX_NORMAL) {
            nets = (int)(cent->currentState.netangles[0] * 4.0f) ^ (int)(cent->currentState.netangles[1] * 4.0f)
                 ^ (int)(cent->currentState.netangles[2] * 4.0f);
            angs = (int)(cent->currentState.angles[0] * 4.0f) ^ (int)(cent->currentState.angles[1] * 4.0f)
                 ^ (int)(cent->currentState.angles[2] * 4.0f);

            if (cent->currentState.pos.trType != widget_k1_postype[slot]
                || cent->currentState.apos.trType != widget_k1_apostype[slot] || nets != widget_k1_nets[slot]
                || angs != widget_k1_angs[slot]) {
                widget_k1_postype[slot] = cent->currentState.pos.trType;
                widget_k1_apostype[slot] = cent->currentState.apos.trType;
                widget_k1_nets[slot]  = nets;
                widget_k1_angs[slot]  = angs;

                if (WIDGET_On() && WIDGET_Throttle(&widget_k1_ctr[cent->currentState.number & 7], 40, 128)) {
                    WIDGET_K1_PROBE(
                        "lerp",
                        "ent=%d etype=%d interp=%d pos{t=%d tm=%d dur=%d base=(%.1f,%.1f,%.1f) d=(%.1f,%.1f,%.1f)}"
                        " apos{t=%d tm=%d dur=%d base=(%.1f,%.1f,%.1f) d=(%.1f,%.1f,%.1f)}"
                        " angles=(%.1f,%.1f,%.1f) netang=(%.1f,%.1f,%.1f)"
                        " org=(%.1f,%.1f,%.1f) netorg=(%.1f,%.1f,%.1f)"
                        " -> lerped org=(%.1f,%.1f,%.1f) ang=(%.1f,%.1f,%.1f)\n",
                        cent->currentState.number,
                        cent->currentState.eType,
                        cent->interpolate ? 1 : 0,
                        cent->currentState.pos.trType,
                        cent->currentState.pos.trTime,
                        cent->currentState.pos.trDuration,
                        cent->currentState.pos.trBase[0],
                        cent->currentState.pos.trBase[1],
                        cent->currentState.pos.trBase[2],
                        cent->currentState.pos.trDelta[0],
                        cent->currentState.pos.trDelta[1],
                        cent->currentState.pos.trDelta[2],
                        cent->currentState.apos.trType,
                        cent->currentState.apos.trTime,
                        cent->currentState.apos.trDuration,
                        cent->currentState.apos.trBase[0],
                        cent->currentState.apos.trBase[1],
                        cent->currentState.apos.trBase[2],
                        cent->currentState.apos.trDelta[0],
                        cent->currentState.apos.trDelta[1],
                        cent->currentState.apos.trDelta[2],
                        cent->currentState.angles[0],
                        cent->currentState.angles[1],
                        cent->currentState.angles[2],
                        cent->currentState.netangles[0],
                        cent->currentState.netangles[1],
                        cent->currentState.netangles[2],
                        cent->currentState.origin[0],
                        cent->currentState.origin[1],
                        cent->currentState.origin[2],
                        cent->currentState.netorigin[0],
                        cent->currentState.netorigin[1],
                        cent->currentState.netorigin[2],
                        cent->lerpOrigin[0],
                        cent->lerpOrigin[1],
                        cent->lerpOrigin[2],
                        cent->lerpAngles[0],
                        cent->lerpAngles[1],
                        cent->lerpAngles[2]
                    );
                }
            }
        }
    }
}

void CG_AddCEntity(centity_t *cent)
{
    if (cent->currentState.eType >= ET_EVENTS) {
        return;
    }

    CG_CalcEntityLerpPositions(cent);

    CG_EntityEffects(cent);

    CG_SetEntitySoundPosition(cent);

    // 0x0002FCC8) -- and gates it on the player not being in a cinematic
    if (cg.snap && !cg.snap->ps.stats[STAT_CINEMATIC]) {
        CG_Target(cent);
    }

    switch (cent->currentState.eType) {
    default:
        cgi.Error(ERR_DROP, "Bad entity type: %i\n", cent->currentState.eType);
        break;
    case ET_MODELANIM:
        CG_Splash(cent);
        CG_ModelAnim(cent, qfalse);
        break;
    case ET_PLAYER:
        CG_Player(cent);
        CG_Splash(cent);
        CG_ModelAnim(cent, qfalse);
        break;
    case ET_ITEM:
        // CG_Item itself does NOT draw the model (its epilogue is at 0x2F374 and the
        CG_Item(cent);
        CG_ModelAnim(cent, qfalse);
        break;
    case ET_GENERAL:
        CG_General(cent);
        break;
    case ET_MOVER:
        CG_Mover(cent);
        break;
    case ET_BEAM:
        CG_Beam(cent);
        break;
    case ET_MULTIBEAM: 
        break;
    case ET_SPRITE:
        CG_Sprite(cent);
        break;
    case ET_PORTAL:
        CG_Portal(cent);
        break;
    case ET_RAIN:
        CG_Emitter(cent);
        break;
    case ET_DECAL:
        CG_Decal(cent);
        break;
    case ET_EMITTER:
        CG_Emitter(cent);
        break;
    case ET_ROPE:
        CG_Rope(cent);
        break;
    }
}

void CG_AddPacketEntities(void)
{
    int        num;
    centity_t *cent;

    cg.refdef.entity_fade_dist = atof(CG_ConfigString(CS_ENTITYFADEINFO));

    cg.autoAngles[0] = 0;
    cg.autoAngles[1] = (cg.time & 2047) * 360 / 2048.0;
    cg.autoAngles[2] = 0;

    cg.autoAnglesSlow[0] = 0;
    cg.autoAnglesSlow[1] = (cg.time & 4095) * 360 / 4096.0f;
    cg.autoAnglesSlow[2] = 0;

    cg.autoAnglesFast[0] = 0;
    cg.autoAnglesFast[1] = (cg.time & 1023) * 360 / 1024.0f;
    cg.autoAnglesFast[2] = 0;

    AnglesToAxis(cg.autoAngles, cg.autoAxis);
    AnglesToAxis(cg.autoAnglesSlow, cg.autoAxisSlow);
    AnglesToAxis(cg.autoAnglesFast, cg.autoAxisFast);

    CG_UpdateRadarMode();

    // retail CG_AddPacketEntities calls CG_TargetInitialize here, every frame
    CG_TargetInitialize();

    for (num = 0; num < cg.snap->numEntities; num++) {
        CG_AddCEntity(&cg_entities[cg.snap->entities[num].number]);
    }

    for (num = 0; num < cg.snap->numEntities; num++) {
        cent = &cg_entities[cg.snap->entities[num].number];
        if (cent->currentState.eType == ET_MULTIBEAM) {
            CG_MultiBeam(cent);
        }
    }
}

void CG_GetOrigin(centity_t *cent, vec3_t origin)
{
    if (cent->currentState.parent == ENTITYNUM_NONE) {
        VectorCopy(cent->lerpOrigin, origin);
    } else {
        int i;
        int handle;
        orientation_t or ;
        refEntity_t *parent;

        parent = cgi.R_GetRenderEntity(cent->currentState.parent);

        if (!parent) {
            cgi.Com_WDPrintf("CG_GetOrigin: Could not find parent entity\n");
            return;
        }

        handle = cgi.R_Model_GetHandle(parent->hModel);

        if (r_lerpmodels->integer) {
            cgi.TIKI_Tag_LerpOrientation(&or, handle, parent, cent->currentState.tag_num);
        } else {
            cgi.TIKI_Tag_OrientationEx(
                &or,
                handle,
                parent->anim,
                parent->frame,
                cent->currentState.tag_num & TAG_MASK,
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

        VectorCopy(parent->origin, origin);

        for (i = 0; i < 3; i++) {
            VectorMA(origin, or.origin[i], parent->axis[i], origin);
        }
    }
}
