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


#include "tr_local.h"
#include "../corepp/tiki.h"
#include "../corepp/vector.h"
#include "../tiki/tiki_skan.h"

#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

#define LL(x) x = LittleLong(x)

qboolean   g_bInfoworldtris = qfalse;
static int entityNumIndexes[MAX_ENTITIES];
static int staticModelNumIndexes[4095];


static int R_CullSkelModel(
    dtiki_t      *tiki,
    refEntity_t  *e,
    skelFrame_t  *nf,
    skelFrame_t  *of,
    float         fScale,
    float        *vLocalOrg
);

static int R_CullTikiModel(
    dtiki_t      *tiki,
    refEntity_t  *e,
    tikiFrame_t  *nf,
    tikiFrame_t  *of,
    float         fScale,
    float        *vLocalOrg
);

static qboolean R_ValidateSkelAnimAndFrame(
    dtiki_t           *tiki,
    int               *animIdx,
    short int         *frameIdx,
    skelAnimHeader_t **hdrOut,
    skelFrame_t      **frameOut
);
static qboolean R_ValidateSkelAnimAndFrame(dtiki_t *tiki, int *animIdx, short int *frameIdx, qboolean allowTan);
static float    R_EntityChannelTime(dtiki_t *tiki, int anim, int frame, float framelerp);
static int      R_ResolveEntityPose(refEntity_t *e, frameInfo_t *frameInfo, short int *bodyParts);

#ifndef SKELANIMFRAME_SIZE
#define SKELANIMFRAME_SIZE(n) \
    (sizeof(skelAnimFrame_t) + ((n) > 0 ? (n) - 1 : 0) * sizeof(SkelMat4))
#endif

void R_CacheModel(model_t *mod);
void R_RegisterShaders(model_t *mod);

model_t *R_GetModelByHandle(qhandle_t hModel)
{
    model_t *mod;

    if (hModel < 1 || hModel >= tr.numModels) {
        return &tr.models[0];
    }

    mod = &tr.models[hModel];

    return mod;
}

dtiki_t *R_Model_GetHandle(qhandle_t handle)
{
    model_t *model = R_GetModelByHandle(handle);

    if (model->type == MOD_TIKI) {
        return model->d.tiki;
    }

    return NULL;
}

void R_CacheModel(model_t *mod)
{
    if (mod->r_sequence != -1) {
        mod->r_sequence = r_sequencenumber;
    }

    if (mod->type == MOD_TIKI && mod->d.tiki) {
        R_RegisterShaders(mod);
    }
}


void R_FreeModel(model_t *mod)
{
    // retail: model_t teardown never calls back into cgame.  CG_ProcessInitCommands
    // @0x031F2C/@0x032098 and CG_PlaceCloudFX @0x03A96C -- never from the renderer,
    memset(mod, 0, sizeof(model_t));
}

model_t *R_AllocModel(void)
{
    int i;

    for (i = 0; i < tr.numModels; i++) {
        if (!tr.models[i].name[0]) {
            break;
        }
    }

    if (i == tr.numModels) {
        if (i == MAX_MOD_KNOWN) {
            return NULL;
        }
        tr.numModels++;
    }

    tr.models[i].index = i;
    tr.models[i].r_sequence = r_sequencenumber;

    return &tr.models[i];
}

void R_FreeNonTikiModels(void)
{
    int i;

    for (i = 1; i < tr.numModels; i++) {
        if (!tr.models[i].name[0]) {
            continue;
        }
        if (tr.models[i].type == MOD_TIKI) {
            continue;
        }

        memset(&tr.models[i], 0, sizeof(model_t));
    }
}

void R_FreeModels(void)
{
    int i;

    for (i = 0; i < tr.numModels; i++) {
        if (!tr.models[i].name[0]) {
            continue;
        }
        if (tr.models[i].type == MOD_TIKI) {
            continue;
        }

        memset(&tr.models[i], 0, sizeof(model_t));
    }
}



void R_RegisterShaders(model_t *mod)
{
    dtiki_t        *tiki;
    int             i, j;
    dtikisurface_t *psurface;
    shader_t       *sh;

    tiki = mod->d.tiki;

    for (i = 0; i < tiki->num_surfaces; i++) {
        psurface = &tiki->surfaces[i];

        assert(psurface->numskins <= MAX_TIKI_SHADER);
        for (j = 0; j < psurface->numskins; j++) {
            if (psurface->shader[j][0]) {
                sh = R_FindShader(
                    psurface->shader[j],
                    LIGHTMAP_NONE,
                    !(psurface->flags & TIKI_SURF_NOMIPMAPS),
                    r_picmip->integer,
                    qtrue,
                    qtrue
                );
                psurface->hShader[j] = sh->index;
                if ( sh == tr.defaultShader ) {
                    SMP_ShaderDefaultProbe( "tiki-surface", psurface->shader[j], mod->name, i, j );
                }
            } else {
                psurface->hShader[j] = 0;
            }
        }
    }
}

void RE_UnregisterServerModel(qhandle_t hModel)
{
    if (hModel < 0 || hModel >= MAX_MOD_KNOWN) {
        return;
    }

    if (tr.models[hModel].serveronly) {
        R_FreeModel(&tr.models[hModel]);
    }
}

#define TIKI_FADE_MAX_INCLUDES 8
#define TIKI_FADE_MAX_DEPTH    2

static float R_TikiScanFadeDistMod(const char *name, int depth)
{
    char *buffer = NULL;
    char *p;
    long  len;
    float result = -1.0f;
    char  includes[TIKI_FADE_MAX_INCLUDES][MAX_QPATH];
    int   numIncludes = 0;
    int   i;

    len = ri.FS_ReadFileEx(name, (void **)&buffer, qtrue);
    if (len <= 0 || !buffer) {
        return -1.0f;
    }

    p = buffer;
    while (1) {
        const char *tok = COM_ParseExt(&p, qtrue);

        if (!tok[0]) {
            break;
        }
        if (!Q_stricmp(tok, "fade_dist_mod")) {
            result = atof(COM_ParseExt(&p, qfalse));
            break;
        }
        if (depth < TIKI_FADE_MAX_DEPTH && numIncludes < TIKI_FADE_MAX_INCLUDES
            && !Q_stricmp(tok, "$include")) {
            Q_strncpyz(includes[numIncludes++], COM_ParseExt(&p, qfalse), MAX_QPATH);
        }
    }

    ri.FS_FreeFile(buffer);

    for (i = 0; result < 0.0f && i < numIncludes; i++) {
        result = R_TikiScanFadeDistMod(includes[i], depth + 1);
    }

    return result;
}

static qhandle_t R_RegisterModelInternal(const char *name, qboolean use)
{
    model_t    *mod;
    qhandle_t   hModel;
    const char *ptr;

    if (!name || !*name) {
        ri.Printf(PRINT_ALL, "RE_RegisterModel: NULL name\n");
        return 0;
    }

    if (strlen(name) >= 128) {
        ri.Printf(PRINT_ALL, "Model name exceeds MAX_MODEL_NAME\n");
        return 0;
    }

    for (hModel = 1; hModel < tr.numModels; hModel++) {
        mod = &tr.models[hModel];
        if (!Q_stricmp(mod->name, name)) {
            if (tr.registrating && mod->r_sequence != r_sequencenumber) {
                R_CacheModel(mod);
            }
            if (mod->type == MOD_BAD) {
                return 0;
            }
            if (mod->type == MOD_TIKI) {
                dtiki_t *fresh;
                SMP_RLOAD_BEGIN( modelreg );
                fresh = ri.TIKI_RegisterTikiFlags(name, use);
                SMP_RLOAD_END( modelreg );
                if (!fresh) {
                    return 0;
                }
                if (fresh != mod->d.tiki) {
                    mod->d.tiki = fresh;
                    R_RegisterShaders(mod);
                }
            }
            return hModel;
        }
    }


    if ((mod = R_AllocModel()) == NULL) {
        ri.Printf(PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
        return 0;
    }

    Q_strncpyz(mod->name, name, sizeof(mod->name));

    R_IssuePendingRenderCommands();

    mod->serveronly = qtrue;

    ptr = strrchr(name, '.');

    if (ptr) {
        ptr++;

        if (!stricmp(ptr, "spr")) {
            mod->d.sprite = SPR_RegisterSprite(name);
            Q_strncpyz(mod->name, name, sizeof(mod->name));

            if (mod->d.sprite) {
                mod->type = MOD_SPRITE;
                return mod->index;
            }
        } else if (!stricmp(ptr, "tik")) {
            SMP_RLOAD_BEGIN( modelreg );
            mod->d.tiki = ri.TIKI_RegisterTikiFlags(name, use);
            SMP_RLOAD_END( modelreg );
            Q_strncpyz(mod->name, name, sizeof(mod->name));

            if (mod->d.tiki) {
                mod->type = MOD_TIKI;
                mod->fadeDistMod = R_TikiScanFadeDistMod(name, 0);
                if (mod->fadeDistMod < 0.0f) {
                    mod->fadeDistMod = 1.0f;
                }
                R_RegisterShaders(mod);

                return mod->index;
            }
        }
    }

    ri.Printf(PRINT_ERROR, "RE_RegisterModel: Registration failed for '%s'\n", name);
    mod->type = MOD_BAD;

    return 0;
}

qhandle_t RE_RegisterServerModel(const char *name)
{
    return R_RegisterModelInternal(name, qfalse);
}

qhandle_t RE_SpawnEffectModel(const char *szModel, vec3_t vPos, vec3_t *axis)
{
    refEntity_t new_entity;

    memset(&new_entity, 0, sizeof(refEntity_t));
    memset(&new_entity.shaderRGBA, 255, sizeof(byte) * 4);

    VectorCopy(vPos, new_entity.origin);
    new_entity.scale = 1.0;

    if (axis) {
        AxisCopy(axis, new_entity.axis);
    }

    new_entity.hModel = R_RegisterModelInternal(szModel, qtrue);

    if (new_entity.hModel) {
        tr.models[new_entity.hModel].serveronly = qfalse;
    }

    return new_entity.hModel;
}

qhandle_t RE_RegisterModel(const char *name)
{
    qhandle_t handle;

    handle = R_RegisterModelInternal(name, qtrue);

    if (handle) {
        tr.models[handle].serveronly = qfalse;
    }
    return handle;
}

qhandle_t RE_ForceRegisterModel(const char *name)
{
    int i;

    if (!name || !*name) {
        ri.Printf(PRINT_WARNING, "RE_RegisterModel: NULL name\n");
        return 0;
    }

    if (strlen(name) >= MAX_QPATH) {
        ri.Printf(PRINT_ALL, "Model name exceeds MAX_QPATH\n");
        return 0;
    }

    for (i = 1; i < tr.numModels; i++) {
        if (!strcmp(tr.models[i].name, name)) {
            R_CacheModel(&tr.models[i]);
            return tr.models[i].type == MOD_BAD ? 0 : i;
        }
    }

    return 0;
}


void TIKI_FlushAll(void)
{
    char     name[MAX_QPATH];
    model_t *m;
    int      i;

    for (i = 1; i < tr.numModels; i++) {
        m = &tr.models[i];

        if (!m->name[0]) {
            continue;
        }
        if (m->type != MOD_TIKI) {
            continue;
        }

        Q_strncpyz(name, m->name, sizeof(name));
        memset(m, 0, sizeof(*m));
        RE_RegisterModel(name);
    }
}

void R_ModelInit(void)
{
    model_t *mod;
    int      i;

    tr.numModels = 0;

    mod = R_AllocModel();
    Q_strncpyz(mod->name, "** BAD MODEL **", sizeof(mod->name));
    mod->type = MOD_BAD;

    for (i = 0; i < (int)ARRAY_LEN(tr.skel_index); i++) {
        tr.skel_index[i] = -1;
    }
}

void R_Modellist_f(void)
{
    int i;

    for (i = 1; i < tr.numModels; i++) {
        ri.Printf(PRINT_ALL, "%s\n", tr.models[i].name);
    }
}

float R_ModelRadius(qhandle_t handle)
{
    int      j;
    vec3_t   bounds[2];
    model_t *model;
    float    radius, maxRadius;
    vec3_t   tmpVec;
    float    w;

    model = R_GetModelByHandle(handle);

    switch (model->type) {
    case MOD_BRUSH:
        maxRadius = 0.0;

        VectorCopy(model->d.bmodel->bounds[0], bounds[0]);
        VectorCopy(model->d.bmodel->bounds[1], bounds[1]);

        for (j = 0; j < 8; j++) {
            tmpVec[0] = bounds[j & 1 ? 1 : 0][0];
            tmpVec[1] = bounds[j & 2 ? 1 : 0][1];
            tmpVec[2] = bounds[j & 4 ? 1 : 0][2];

            radius = VectorLength(tmpVec);

            if (maxRadius < radius) {
                maxRadius = radius;
            }
        }
        break;
    case MOD_TIKI:
        return ri.TIKI_GlobalRadius(model->d.tiki);
    case MOD_SPRITE:
        maxRadius = model->d.sprite->width * model->d.sprite->scale * 0.5;
        w         = model->d.sprite->height * model->d.sprite->scale * 0.5;

        if (maxRadius <= w) {
            maxRadius = w;
        }
        break;
    default:
        maxRadius = 0.0;
    }

    return maxRadius;
}

void R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
    model_t *model;

    model = R_GetModelByHandle(handle);

    switch (model->type) {
    default:
    case MOD_BAD:
        VectorClear(mins);
        VectorClear(maxs);
        break;
    case MOD_BRUSH:
        VectorCopy(model->d.bmodel->bounds[0], mins);
        VectorCopy(model->d.bmodel->bounds[1], maxs);
        break;
    case MOD_TIKI:
        ri.TIKI_CalculateBounds(model->d.tiki, 1.0, mins, maxs);
        break;
    case MOD_SPRITE:
        mins[0] = -model->d.sprite->width * model->d.sprite->scale * 0.5;
        mins[1] = -model->d.sprite->height * model->d.sprite->scale * 0.5;
        mins[2] = -0.0;
        maxs[0] = model->d.sprite->width * model->d.sprite->scale * 0.5;
        maxs[1] = model->d.sprite->height * model->d.sprite->scale * 0.5;
        maxs[2] = 0.0;
        break;
    }
}


#if 0
const char *GetModelPath( skelHeaderGame_t *skelmodel ) {
	int			i;
	int			num;
	skelcache_t	*cache;

	num = cache_numskel;

	for( i = 0; i < TIKI_MAX_SKELCACHE; i++ )
	{
		cache = &skelcache[ i ];

		if( cache->skel )
		{
			if( cache->skel == skelmodel ) {
				return cache->path;
			}

			num--;
			if( num < 0 ) {
				break;
			}
		}
	}

	return NULL;
}
#endif

float R_TikiCalcLodDist(const vec3_t org, const dtiki_t *tiki)
{
    vec3_t delta;
    float  dist, scaled;

    if (r_lodbias->value > 0.5f) {
        return 1.0f;
    }

    VectorSubtract(tr.viewParms.ori.origin, org, delta);
    dist = VectorLength(delta) * r_lodScaleDistance->value;

    if (dist <= 0.0f) {
        return tiki->lod_start_percent_verts;
    }

    scaled = (float)(dist * tan((double)M_PI * (double)tr.viewParms.fovX / (double)360.0));

    if (scaled < tiki->lod_start_distance) {
        return tiki->lod_start_percent_verts;
    }
    if (scaled >= tiki->lod_stop_distance) {
        return r_lodscale->value * tiki->lod_stop_percent_verts;
    }

    return r_lodscale->value
         * (tiki->lod_start_percent_verts
            + (scaled - tiki->lod_start_distance)
                  * (tiki->lod_stop_percent_verts - tiki->lod_start_percent_verts)
                  / (tiki->lod_stop_distance - tiki->lod_start_distance));
}


int GetLodCutoff(const skelSurfaceGame_t *surf, float lodpercentage)
{
    const int *collapse;
    int        cutoff;
    int        threshold;
    int        i;

    if (lodpercentage == 1.0f) {
        return surf->numVerts;
    }

    threshold = (int)(float)((double)surf->numModelVerts * (double)lodpercentage);

    collapse = surf->pCollapse;
    cutoff   = 0;
    for (i = 0; i < surf->numVerts; i++) {
        if ((int)((unsigned)collapse[i] >> 16) > threshold) {
            break;
        }
        cutoff++;
    }

    return cutoff;
}

orientation_t R_GetTagPositionAndOrientation(refEntity_t *ent, int tagnum)
{
    int           i;
    orientation_t tag_or, new_or;

    tag_or = RE_TIKI_Orientation(ent, tagnum);

    VectorCopy(ent->origin, new_or.origin);

    for (i = 0; i < 3; i++) {
        VectorMA(new_or.origin, tag_or.origin[i], ent->axis[i], new_or.origin);
    }

    MatrixMultiply(tag_or.axis, ent->axis, new_or.axis);
    return new_or;
}

surfaceType_t skelSurface = SF_TIKI_SKEL;

static qboolean R_ValidateSkelAnimAndFrame(
    dtiki_t           *tiki,
    int               *animIdx,
    short int         *frameIdx,
    skelAnimHeader_t **hdrOut,
    skelFrame_t      **frameOut
)
{
    skelAnimHeader_t *hdr;
    unsigned int      frame;

    if (!tiki || !animIdx || !frameIdx) {
        return qfalse;
    }

    if (hdrOut) {
        *hdrOut = NULL;
    }
    if (frameOut) {
        *frameOut = NULL;
    }

    if (*animIdx < 0 || *animIdx >= tiki->a->num_anims) {
        *animIdx  = 0;
        *frameIdx = 0;
        return qfalse;
    }

    hdr = ri.TIKI_Skan_FindByAnim(tiki, *animIdx);
    if (!hdr) {
        return qfalse;
    }

    frame = (unsigned int)(unsigned short)*frameIdx;
    if (frame >= (unsigned)hdr->numFrames || (*frameIdx & 0x8000) != 0) {
        *frameIdx = 0;
        return qfalse;
    }

    if (hdrOut) {
        *hdrOut = hdr;
    }
    if (frameOut) {
        *frameOut =
            (skelFrame_t *)((byte *)hdr + hdr->ofsFrames + *frameIdx * (16 * (hdr->numbones - 1) + 56));
    }

    return qtrue;
}

static qboolean R_ValidateSkelAnimAndFrame(dtiki_t *tiki, int *animIdx, short int *frameIdx, qboolean allowTan)
{
    unsigned int frame;

    if (!tiki || !tiki->a || !animIdx || !frameIdx) {
        return qfalse;
    }

    if (*animIdx < 0 || *animIdx >= tiki->a->num_anims) {
        *animIdx  = 0;
        *frameIdx = 0;
        return qfalse;
    }

    if (R_ValidateSkelAnimAndFrame(tiki, animIdx, frameIdx, NULL, NULL)) {
        return qtrue;
    }

    // No native SKAN image.  For a `.tan' animation retail does NOT stop here:
    if (!allowTan) {
        return qfalse;
    }

    {
        int          numTanFrames = 0;
        tikiFrame_t *tanFrames    = ri.TIKI_Anim_TanFrames(tiki, *animIdx, &numTanFrames);

        if (!tanFrames || numTanFrames <= 0) {
            return qfalse;
        }

        frame = (unsigned int)(unsigned short)*frameIdx;
        if (frame >= (unsigned)numTanFrames || (*frameIdx & 0x8000) != 0) {
            *frameIdx = 0;
            return qfalse;
        }
    }

    return qtrue;
}

static float R_EntityChannelTime(dtiki_t *tiki, int anim, int frame, float framelerp)
{
    float frametime;
    float pos;

    frametime = ri.TIKI_Anim_FrameTime(tiki, anim, 0);
    pos       = (float)frame - framelerp;

    if (pos < 0.0f) {
        int numframes = ri.TIKI_Anim_NumFrames(tiki, anim);

        pos = (numframes > 0) ? pos + (float)numframes : 0.0f;
    }

    return pos * frametime;
}

static int R_ResolveEntityPose(refEntity_t *e, frameInfo_t *frameInfo, short int *bodyParts)
{
    static const struct {
        int live;      
        int bodyPart;  
        int anim;      
        int oldanim;   
        int frame;     
        int oldframe;  
        int framelerp; 
        int cbLerp;    
        int cbAnim;    
        int cbFrame;   
    } channels[2] = {
        {offsetof(refEntity_t, legsLive),
         SKEL_BODYPART_LEGS,
         offsetof(refEntity_t, anim),
         offsetof(refEntity_t, oldanim),
         offsetof(refEntity_t, frame),
         offsetof(refEntity_t, oldframe),
         offsetof(refEntity_t, framelerp),
         offsetof(refEntity_t, crossblend_lerp),
         offsetof(refEntity_t, crossblend_anim),
         offsetof(refEntity_t, crossblend_frame)},
        {offsetof(refEntity_t, torsoLive),
         SKEL_BODYPART_TORSO,
         offsetof(refEntity_t, torso_anim),
         offsetof(refEntity_t, torso_oldanim),
         offsetof(refEntity_t, torso_frame),
         offsetof(refEntity_t, torso_oldframe),
         offsetof(refEntity_t, torso_framelerp),
         offsetof(refEntity_t, torso_crossblend_lerp),
         offsetof(refEntity_t, torso_crossblend_anim),
         offsetof(refEntity_t, torso_crossblend_frame)},
    };

    byte *base;
    int   n;
    int   c;

    memset(frameInfo, 0, MAX_FRAMEINFOS * sizeof(frameInfo[0]));
    memset(bodyParts, 0, MAX_FRAMEINFOS * sizeof(bodyParts[0]));

    if (!e || !e->tiki) {
        return 0;
    }

    base = (byte *)e;
    n    = 0;

    for (c = 0; c < 2; c++) {
        int         *anims = (int *)(base + channels[c].anim);
        int         anim;
        int         oldanim;
        short int   oldframe;
        float       cbLerp;
        int         cbAnim;
        frameInfo_t *slot;

        if (!*(int *)(base + channels[c].live)) {
            continue;
        }

        if (n >= MAX_FRAMEINFOS) {
            break;
        }

        anim = anims[0];

        if (!R_ValidateSkelAnimAndFrame(e->tiki, &anim, (short int *)(base + channels[c].frame), qfalse)) {
            return 0;
        }

        oldanim  = anims[1];
        oldframe = *(short int *)(base + channels[c].oldframe);
        R_ValidateSkelAnimAndFrame(e->tiki, &oldanim, &oldframe, qfalse);

        cbLerp = *(float *)(base + channels[c].cbLerp);

        slot         = &frameInfo[n];
        slot->index  = anim;
        slot->weight = (cbLerp >= 0.0f) ? cbLerp : 1.0f;
        slot->time   = R_EntityChannelTime(
            e->tiki, anim, *(short int *)(base + channels[c].frame),
            *(float *)(base + channels[c].framelerp)
        );
        bodyParts[n] = (short int)channels[c].bodyPart;
        n++;

        if (channels[c].bodyPart == SKEL_BODYPART_TORSO || cbLerp >= 0.0f) {
            R_ValidateSkelAnimAndFrame(
                e->tiki, (int *)(base + channels[c].cbAnim), (short int *)(base + channels[c].cbFrame),
                qfalse
            );
        }

        if (cbLerp < 0.0f || n >= MAX_FRAMEINFOS) {
            continue;
        }

        cbAnim = *(int *)(base + channels[c].cbAnim);
        if (!R_ValidateSkelAnimAndFrame(e->tiki, &cbAnim, (short int *)(base + channels[c].cbFrame), qfalse)) {
            continue;
        }

        slot         = &frameInfo[n];
        slot->index  = cbAnim;
        slot->weight = 1.0f - cbLerp;
        slot->time = R_EntityChannelTime(e->tiki, cbAnim, *(short int *)(base + channels[c].cbFrame), 0.0f);
        bodyParts[n] = (short int)channels[c].bodyPart;
        n++;
    }

    return n;
}

static void R_BoneControllerGate(const refEntity_t *e, int *boneTag, vec4_t *boneQuat)
{
    int n;

    for (n = 0; n < NUM_BONE_CONTROLLERS; n++) {
        boneTag[n] = -1;
        VectorClear4(boneQuat[n]);
    }

    if (e->boneTagGate) {
        for (n = 0; n < NUM_BONE_CONTROLLERS; n++) {
            if (e->bone_tag[n] < 0) {
                continue;
            }
            boneTag[n] = e->bone_tag[n];
            VectorCopy4(e->bone_quat[n], boneQuat[n]);
        }
    }
}

static void R_ShowTikiLodDebugNumbers(trRefEntity_t *ent, dtiki_t *tiki)
{
    vec3_t org;
    int    render_tris, total_tris;
    float  number, divisor, scale;
    int    precision;

    if (!r_showlod->integer) {
        return;
    }

    VectorCopy(ent->e.origin, org);
    org[2] += 100.0f;

    R_CountTikiLodTris(tiki, ent->lodpercentage[0], &render_tris, &total_tris);

    if (total_tris < 10) {
        divisor   = 10.0f;
        precision = 1;
    } else if (total_tris < 100) {
        divisor   = 100.0f;
        precision = 2;
    } else if (total_tris < 1000) {
        divisor   = 1000.0f;
        precision = 3;
    } else {
        divisor   = 10000.0f;
        precision = 4;
    }

    number = (float)render_tris + (float)total_tris / divisor;
    scale  = 1.0f / ent->lodpercentage[0];

    R_DrawDebugNumber(org, number, scale, 1.0f, 1.0f, 0.0f, precision);
}

void R_AddSkelSurfaces(trRefEntity_t *ent)
{
    dtiki_t         *tiki;
    qboolean         personalModel;
    float            tiki_scale;
    vec3_t           tiki_localorigin;
    vec3_t           tiki_entityorigin;
    skelBoneCache_t *outbones;
    int              boneTag[NUM_BONE_CONTROLLERS];
    vec4_t           boneQuat[NUM_BONE_CONTROLLERS];
    SkelVec3         centroid;
    int              boneCount;
    shader_t        *shader;
    dtikisurface_t  *dsurf;
    byte            *bsurf;
    skelSurfaceGame_t *surface;
    skelSurfaceGame_t *drawnSurface;
    Vector newDistance;
    int i;
    int mesh;
    int fogNum;
    int poseCount;
    qboolean fadeMain;
    qboolean fadeSecond;
    int lodMaxStep;
    const int boneArena = tr.smpFrame;

    tiki = ent->e.tiki;


    if (!ent->e.legsLive && !ent->e.torsoLive) {
        ri.Printf(PRINT_DEVELOPER, "R_AddSkelSurfaces: model '%s' with no torso or leg anim set\n", tiki->a->name);
        return;
    }

    // and retail R_AddSkelSurfaces never poses a skeletor at all -- it resolves the two
    {
        frameInfo_t frameInfo[MAX_FRAMEINFOS];
        short int   bodyParts[MAX_FRAMEINFOS];

        poseCount = R_ResolveEntityPose(&ent->e, frameInfo, bodyParts);
    }

    personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;

    outbones = &TIKI_Skel_Bones_Arena[boneArena][TIKI_Skel_Bones_Cursor[boneArena]];

    // skeleton and must apply retail's own pre-test.  (REG-003, folded from W29-REG1.)
    {
        skelHeaderGame_t *skel = ri.TIKI_GetSkel(tiki->skelIndex);

        if (!skel) {
            ri.Printf(
                PRINT_DEVELOPER, "R_AddSkelSurfaces: couldn't get skel model in '%s'\n", tiki->a->name
            );
            return;
        }

        if (TIKI_Skel_Bones_Cursor[boneArena] + skel->numBones > MAX_SKELBONES) {
            /* A bail here drops the surface silently: PRINT_DEVELOPER never
             * reaches a device log, and a dropped skel surface is the flat-blue /
             * missing-model end state.  Name it at the master gate, throttled. */
            static int skelbail_rows;
            if (PROBE_Throttle(&skelbail_rows, 4, 240)) {
                ri.Printf(
                    PRINT_ALL,
                    "[GLES3:skelbail] arena=%d cursor=%d bones=%d model='%s' f=%d\n",
                    boneArena, TIKI_Skel_Bones_Cursor[boneArena], skel->numBones,
                    tiki->a->name ? tiki->a->name : "?", RB_FrameCount()
                );
            }
            ri.Printf(
                PRINT_DEVELOPER, "R_AddSkelSurfaces: too many skeleton models visible on '%s'\n", tiki->a->name
            );
            return;
        }
    }

    // wrote; 1.0 means "not faded" and 0.0 means fully faded out, so retail stops dead for a
    if (ent->e.fade == 0.0f) {
        return;
    }

    if (poseCount == 0) {
        return;
    }

    tiki_scale = tiki->load_scale * ent->e.scale;
    VectorScale(tiki->load_origin, tiki_scale, tiki_localorigin);
    VectorAdd(ent->e.origin, tiki_localorigin, tiki_entityorigin);

    if (ent->e.legsLive) {
        skelAnimHeader_t *nfHdr;
        skelFrame_t      *nf;
        skelAnimHeader_t *ofHdr;
        skelFrame_t      *of;

        if (!R_ValidateSkelAnimAndFrame(tiki, &ent->e.anim, &ent->e.frame, &nfHdr, &nf)) {
            return;
        }

        R_ValidateSkelAnimAndFrame(tiki, &ent->e.oldanim, &ent->e.oldframe, &ofHdr, &of);

        if (R_CullSkelModel(tiki, &ent->e, nf, of, tiki_scale, tiki_localorigin) == CULL_OUT) {
            return;
        }

        ent->e.of = of;
        ent->e.nf = nf;

        if (ent->e.crossblend_lerp >= 0.0f) {
            R_ValidateSkelAnimAndFrame(tiki, &ent->e.crossblend_anim, &ent->e.crossblend_frame, qfalse);
        }
    }

    if (ent->e.torsoLive) {
        skelAnimHeader_t *tfHdr;
        skelFrame_t      *tf;
        skelFrame_t      *tof;

        if (!R_ValidateSkelAnimAndFrame(tiki, &ent->e.torso_anim, &ent->e.torso_frame, &tfHdr, &tf)) {
            return;
        }

        R_ValidateSkelAnimAndFrame(tiki, &ent->e.torso_oldanim, &ent->e.torso_oldframe, NULL, &tof);

        R_ValidateSkelAnimAndFrame(tiki, &ent->e.torso_crossblend_anim, &ent->e.torso_crossblend_frame, qfalse);

        if (!ent->e.legsLive) {
            if (R_CullSkelModel(tiki, &ent->e, tf, tof, tiki_scale, tiki_localorigin) == CULL_OUT) {
                return;
            }
            ent->e.of = tof;
            ent->e.nf = tf;
        }
    }


    R_BoneControllerGate(&ent->e, boneTag, boneQuat);

    if (tr.viewParms.isPortal) {
        ent->lodpercentage[1] = R_TikiCalcLodDist(tiki_entityorigin, tiki);
    } else {
        ent->lodpercentage[0] = R_TikiCalcLodDist(tiki_entityorigin, tiki);
    }

    {
        vec3_t fogOrigin;
        float  fogRadius = tiki_scale * (ent->e.nf ? ent->e.nf->radius : 0.0f);

        VectorAdd(ent->e.origin, tiki_localorigin, fogOrigin);
        fogNum = R_CalcFogNum(fogOrigin, fogRadius);
    }

    boneCount = ri.TIKI_SkelBonesFromRecords(
        tiki,
        ent->e.anim,
        ent->e.frame,
        ent->e.oldanim,
        ent->e.oldframe,
        r_lerpmodels->integer ? ent->e.framelerp : 0.0f,
        ent->e.crossblend_anim,
        ent->e.crossblend_frame,
        ent->e.crossblend_lerp,
        ent->e.torso_anim,
        ent->e.torso_frame,
        ent->e.torso_oldanim,
        ent->e.torso_oldframe,
        r_lerpmodels->integer ? ent->e.torso_framelerp : 0.0f,
        ent->e.torso_crossblend_anim,
        ent->e.torso_crossblend_frame,
        ent->e.torso_crossblend_lerp,
        ent->e.legsLive ? qtrue : qfalse,
        ent->e.torsoLive ? qtrue : qfalse,
        boneTag,
        boneQuat,
        tiki_localorigin,
        outbones,
        MAX_SKELBONES - TIKI_Skel_Bones_Cursor[boneArena],
        NULL,
        NULL,
        NULL
    );

    // no second solver -- when a live channel's record does not resolve, retail
    if (boneCount <= 0) {
        return;
    }

    ent->e.bonestart = TIKI_Skel_Bones_Cursor[boneArena];
    TIKI_Skel_Bones_Cursor[boneArena] += boneCount;

    // a port-only superset with no retail counterpart, and one that also had to be
    fadeMain   = qfalse;
    fadeSecond = qfalse;
    if (ent->e.fade < 1.0f) {
        fadeMain   = qtrue;
        fadeSecond = qtrue;
    }
    if (ent->e.shaderRGBA[3] != 255) {
        if (ent->e.renderfx & RF_FORCE_ALPHA) {
            fadeMain   = qtrue;
            fadeSecond = qtrue;
        }
        if (ent->e.renderfx & RF_FORCE_ALPHA_EFFECTS) {
            fadeSecond = qtrue;
        }
    }
    if (fadeMain || fadeSecond) {
        ent->e.fade *= (float)ent->e.shaderRGBA[3] / 255.0f;
    }

    dsurf = tiki->surfaces;
    bsurf = &ent->e.surfaces[0];
    lodMaxStep = 0;
    for (mesh = 0; mesh < tiki->numMeshes; mesh++) {
        skelHeaderGame_t *skelmodel = ri.TIKI_GetSkel(tiki->mesh[mesh]);

        if (!skelmodel) {
            ri.Printf(PRINT_DEVELOPER, "R_AddSkelSurfaces: couldn't get skel model in '%s'\n", tiki->a->name);
            return;
        }

        surface = skelmodel->pSurfaces;
        for (i = 0; i < skelmodel->numSurfaces; i++, dsurf++, bsurf++, surface = surface->pNext) {
            drawnSurface = surface;
            if (tiki->replacedSurfaces) {
                int dsurfIndex = (int)(dsurf - tiki->surfaces);

                if (dsurfIndex >= 0 && dsurfIndex < tiki->numReplacedSurfaces) {
                    dtikiReplacedSurface_t *rrow = &tiki->replacedSurfaces[dsurfIndex];

                    if (rrow->skelIndex != -1) {
                        skelHeaderGame_t  *rskel = ri.TIKI_GetSkel(rrow->skelIndex);
                        skelSurfaceGame_t *rsurf = rskel ? rskel->pSurfaces : NULL;
                        int                rstep;

                        for (rstep = 0; rsurf && rstep < rrow->surfaceIndex; rstep++) {
                            rsurf = rsurf->pNext;
                        }

                        if (rsurf) {
                            drawnSurface = rsurf;
                        }
                    }
                }
            }

            if (drawnSurface->pCollapse && drawnSurface->numVerts > 0) {
                int step = (int)((unsigned)drawnSurface->pCollapse[drawnSurface->numVerts - 1] >> 16);

                if (step > lodMaxStep) {
                    lodMaxStep = step;
                }
            }

            if ((*bsurf & 4) || (dsurf->flags & TIKI_SURF_NODRAW)) {
                continue;
            }

            shader                 = NULL;
            drawnSurface->ident = SF_TIKI_SKEL;

            if (!(ent->e.customShader) || (ent->e.renderfx & RF_CUSTOMSHADERPASS)) {
                int iShaderNum = ent->e.skinNum + (*bsurf & 3);

                if (iShaderNum >= dsurf->numskins) {
                    iShaderNum = 0;
                }
                shader = tr.shaders[dsurf->hShader[iShaderNum]];
            } else {
                shader = R_GetShaderByHandle(ent->e.customShader);
            }

            // The port's R_FindShader's 6th slot is `wrapy', not retail's a6, so the
            if (fadeMain) {
                shader = R_FindShaderForceAlpha(shader->name);
            }

            if (!personalModel && r_shadows->integer == 2 && fogNum == 0
                && !(ent->e.renderfx & RF_FIRST_PERSON) && (ent->e.renderfx & RF_SHADOW)
                && (ent->e.renderfx & RF_SHADOW_PRECISE) && shader->sort == (float)SS_OPAQUE) {
                R_AddDrawSurf((surfaceType_t *)drawnSurface, tr.shadowShader, 0, 0);
            }

            if (!personalModel) {
                if ((*bsurf & 0x40) && (dsurf->numskins > 1)) {
                    int iShaderNum = ent->e.skinNum + (*bsurf & 2);

                    R_AddDrawSurf((surfaceType_t *)drawnSurface, tr.shaders[dsurf->hShader[iShaderNum]], fogNum, 0);
                    R_AddDrawSurf((surfaceType_t *)drawnSurface, tr.shaders[dsurf->hShader[iShaderNum + 1]], fogNum, 0);
                } else {
                    R_AddDrawSurf((surfaceType_t *)drawnSurface, shader, fogNum, 0);
                }
            }

            if ((ent->e.customShader) && (ent->e.renderfx & RF_CUSTOMSHADERPASS)
                && !(dsurf->flags & 0x400)) {
                shader = R_GetShaderByHandle(ent->e.customShader);
                if (fadeSecond) {
                    shader = R_FindShaderForceAlpha(shader->name);
                }
                R_AddDrawSurf((surfaceType_t *)drawnSurface, shader, fogNum, 0);
            }
        }
    }

    for (mesh = 0; mesh < tiki->numMeshes; mesh++) {
        skelHeaderGame_t *sm = ri.TIKI_GetSkel(tiki->mesh[mesh]);
        skelSurfaceGame_t *s;

        if (!sm) {
            continue;
        }

        for (s = sm->pSurfaces, i = 0; i < sm->numSurfaces; s = s->pNext, i++) {
            s->numModelVerts = lodMaxStep;
        }
    }

    R_ShowTikiLodDebugNumbers(ent, tiki);

}

inline static void SkelVertGetNormal(skeletorVertex_t *vert, skelBoneCache_t *bone, vec3_t out)
{
    out[0] = vert->normal[0] * bone->matrix[0][0] + vert->normal[1] * bone->matrix[1][0]
           + vert->normal[2] * bone->matrix[2][0];

    out[1] = vert->normal[0] * bone->matrix[0][1] + vert->normal[1] * bone->matrix[1][1]
           + vert->normal[2] * bone->matrix[2][1];

    out[2] = vert->normal[0] * bone->matrix[0][2] + vert->normal[1] * bone->matrix[1][2]
           + vert->normal[2] * bone->matrix[2][2];
}

inline static void SkelMorphGetXyz(skeletorMorph_t *morph, int *morphcache, vec3_t out)
{
    VectorMA(out, *morphcache, morph->offset, out);
}

inline static void SkelWeightGetXyz(skelWeight_t *weight, skelBoneCache_t *bone, vec3_t out)
{
    out[0] += ((weight->offset[0] * bone->matrix[0][0] + weight->offset[1] * bone->matrix[1][0]
                + weight->offset[2] * bone->matrix[2][0])
               + bone->offset[0])
            * weight->boneWeight;

    out[1] += ((weight->offset[0] * bone->matrix[0][1] + weight->offset[1] * bone->matrix[1][1]
                + weight->offset[2] * bone->matrix[2][1])
               + bone->offset[1])
            * weight->boneWeight;

    out[2] += ((weight->offset[0] * bone->matrix[0][2] + weight->offset[1] * bone->matrix[1][2]
                + weight->offset[2] * bone->matrix[2][2])
               + bone->offset[2])
            * weight->boneWeight;
}

inline static void SkelWeightMorphGetXyz(skelWeight_t *weight, skelBoneCache_t *bone, vec3_t totalmorph, vec3_t out)
{
    vec3_t point;

    VectorAdd(totalmorph, weight->offset, point);

    out[0] += ((point[0] * bone->matrix[0][0] + point[1] * bone->matrix[1][0] + point[2] * bone->matrix[2][0])
               + bone->offset[0])
            * weight->boneWeight;

    out[1] += ((point[0] * bone->matrix[0][1] + point[1] * bone->matrix[1][1] + point[2] * bone->matrix[2][1])
               + bone->offset[1])
            * weight->boneWeight;

    out[2] += ((point[0] * bone->matrix[0][2] + point[1] * bone->matrix[1][2] + point[2] * bone->matrix[2][2])
               + bone->offset[2])
            * weight->boneWeight;
}

static void R_TanDecodeNormal(short packed, vec3_t out)
{
    static float s_sin[256];
    static float s_cos[256];
    static qboolean s_init = qfalse;
    unsigned int    nrm    = (unsigned int)(unsigned short)packed;
    int             lat, lng;

    if (!s_init) {
        int i;
        for (i = 0; i < 256; i++) {
            float a  = (float)i * (2.0f * M_PI / 255.0f);
            s_sin[i] = sin(a);
            s_cos[i] = cos(a);
        }
        s_init = qtrue;
    }

    lat = (nrm >> 8) & 0xFF;
    lng = nrm & 0xFF;

    out[0] = s_cos[lat] * s_sin[lng];
    out[1] = s_sin[lat] * s_sin[lng];
    out[2] = s_cos[lng];
}

static void R_TanGetFrames(
    const skelHeaderGame_t *skelmodel, const refEntity_t *e, int *beforeFrame, int *afterFrame, float *afterWeight
)
{
    const float timeTolerance = 0.001f;
    const float frameTime     = skelmodel->tanFrameTime;
    const int   numFrames     = skelmodel->tanNumFrames;
    float       after;
    float       t;
    int         f1, f2;

    *beforeFrame = 0;
    *afterFrame  = 0;
    *afterWeight = 0.0f;

    if (numFrames < 2 || frameTime <= 0.0f) {
        return;
    }

    {
        int   anim   = e->anim;
        short frame  = e->frame;

        if (e->legsLive && R_ValidateSkelAnimAndFrame(e->tiki, &anim, &frame, qtrue)) {
            t = R_EntityChannelTime(e->tiki, anim, frame, e->framelerp);
        } else {
            t = 0.0f;
        }
    }

    f1 = (int)(t / frameTime);
    if (f1 < 0) {
        f1 = 0;
    }
    f2    = f1 + 1;
    after = (t / frameTime) - (float)f1;

    if ((1.0f - after) * frameTime < timeTolerance || after * frameTime < timeTolerance) {
        if (after > 0.5f) {
            f1++;
        }
        if (f1 >= numFrames) {
            f1 = numFrames - 1;
        }
        *beforeFrame = f1;
        *afterFrame  = f1;
        *afterWeight = 0.0f;
    } else {
        if (f2 >= numFrames) {
            f2 = numFrames - 1;
            if (f1 >= numFrames) {
                *beforeFrame = numFrames - 1;
                *afterFrame  = numFrames - 1;
                *afterWeight = 0.0f;
                return;
            }
        }
        *beforeFrame = f1;
        *afterFrame  = f2;
        *afterWeight = after;
    }
}


#define RB_SKEL_BONE_INDEX(bi) (boneMap ? boneMap[(bi)] : (bi))

static qboolean RB_TikiVertCap(int render_count, const skelHeaderGame_t *skelmodel, const skelSurfaceGame_t *sf)
{
    if (render_count <= 1200) {
        return qfalse;
    }

    ri.Printf(
        PRINT_DEVELOPER,
        "Too many verts (%d) in model %s surface %s (max is %d)\n",
        render_count,
        skelmodel ? skelmodel->name : "",
        sf->name,
        1200
    );
    return qtrue;
}

void RB_SkelMesh(skelSurfaceGame_t *sf)
{
    unsigned int       baseIndex, baseVertex;
    unsigned int       render_count;
    unsigned int       indexes;
    float             *outXyz;
    vec4_t            *outNormal;
    skelIndex_t       *triangles;
    int               *collapse_map;
    skeletorVertex_t  *newVerts;
    skeletorMorph_t   *morph;
    skelWeight_t      *weight;
    int                vertNum;
    int                morphNum;
    int                weightNum;
    skelBoneCache_t   *bones;
    skelBoneCache_t   *bone;
    int               *morphs;
    int               *morphcache;
    float              scale;
    dtiki_t           *tiki;
    int                mesh;
    int                surf;
    int                surfaceBase; 
    int                i;
    int                lod_cutoff;
    int                emitted;
    skelHeaderGame_t  *skelmodel;
    skelSurfaceGame_t *psurface;
    qboolean           bFound;
    int                collapse[TIKI_MAX_VERTEXES];
    skelHeaderGame_t  *replacedSkel;
    int               *boneMap;
    int               *morphMap;


    tiki = backEnd.currentEntity->e.tiki;

    scale = tiki->load_scale * backEnd.currentEntity->e.scale;

    bFound      = qfalse;
    surfaceBase = 0;
    for (mesh = 0; mesh < tiki->numMeshes; mesh++) {
        skelmodel = ri.TIKI_GetSkel(tiki->mesh[mesh]);
        psurface  = skelmodel->pSurfaces;

        surfaceBase = 0;
        for (surf = 0; surf < skelmodel->numSurfaces; surf++) {
            if (psurface == sf) {
                bFound = qtrue;
                break;
            }
            surfaceBase += psurface->numVerts;
            psurface = psurface->pNext;
        }

        if (bFound) {
            break;
        }
    }

    if (!bFound && tiki->replacedSurfaces) {
        for (surf = 0; surf < tiki->numReplacedSurfaces && !bFound; surf++) {
            dtikiReplacedSurface_t *rrow = &tiki->replacedSurfaces[surf];
            skelHeaderGame_t       *rskel;

            if (rrow->skelIndex == -1) {
                continue;
            }

            rskel = ri.TIKI_GetSkel(rrow->skelIndex);
            if (!rskel) {
                continue;
            }

            psurface    = rskel->pSurfaces;
            surfaceBase = 0;
            for (i = 0; i < rskel->numSurfaces; i++) {
                if (psurface == sf) {
                    bFound = qtrue;
                    break;
                }
                surfaceBase += psurface->numVerts;
                psurface = psurface->pNext;
            }
        }
    }

    assert(bFound);

    boneMap     = NULL;
    morphMap    = NULL;
    replacedSkel = NULL;
    for (surf = 0; surf < tiki->numReplacedSurfaces; surf++) {
        dtikiReplacedSurface_t *row = &tiki->replacedSurfaces[surf];
        skelHeaderGame_t       *rs;
        skelSurfaceGame_t      *rsurf;
        int                     rsi;

        if (row->skelIndex == -1) {
            continue;
        }

        rs = ri.TIKI_GetSkel(row->skelIndex);
        if (!rs) {
            continue;
        }

        rsurf = rs->pSurfaces;
        for (rsi = 0; rsi < rs->numSurfaces; rsi++) {
            if (rsurf == sf) {
                break;
            }
            rsurf = rsurf->pNext;
        }

        if (rsi >= rs->numSurfaces) {
            continue;
        }

        replacedSkel = rs;
        if (row->boneMappingIndex != -1) {
            boneMap = ri.TIKI_GetBoneMapping(row->boneMappingIndex);
        }
        if (row->morphMappingIndex != -1) {
            morphMap = ri.TIKI_GetMorphMapping(row->morphMappingIndex);
        }
        break;
    }

    if (replacedSkel && replacedSkel != skelmodel) {
        skelmodel = replacedSkel;
    }


    lod_cutoff = 0;
    emitted    = 0;

    if (backEnd.viewParms.isPortal) {
        lod_cutoff = GetLodCutoff(sf, backEnd.currentEntity->lodpercentage[1]);
    } else {
        lod_cutoff = GetLodCutoff(sf, backEnd.currentEntity->lodpercentage[0]);
    }

    render_count = lod_cutoff;

    if (!render_count) {
        return;
    }

    if (RB_TikiVertCap((int)render_count, skelmodel, sf)) {
        return;
    }

    indexes = sf->numTriangles * 3;
    RB_CHECKOVERFLOW(render_count, indexes);

    collapse_map = sf->pCollapse;
    triangles    = sf->pTriangles;
    baseIndex    = tess.numIndexes;
    baseVertex   = tess.numVertexes;
    tess.numVertexes += render_count;

    outXyz    = tess.xyz[baseVertex];
    outNormal = &tess.normal[baseVertex];
    newVerts  = sf->pVerts;

    if ((int)render_count == sf->numVerts) {
        for (i = 0; i < (int)indexes; i++) {
            tess.indexes[baseIndex + i] = baseVertex + triangles[i];
        }

        entityNumIndexes[backEnd.currentEntity - backEnd.refdef.entities] += indexes;
        tess.numIndexes += indexes;
    } else {
        assert(sf->numVerts < TIKI_MAX_VERTEXES);

        for (i = 0; i < (int)render_count; i++) {
            collapse[i] = i;
        }

        for (i = render_count; i < sf->numVerts; i++) {
            collapse[i] = collapse[collapse_map[i] & 0xFFFF];
        }

        for (i = 0; i < (int)indexes; i += 3) {
            const int a = collapse[triangles[i + 0]];
            const int b = collapse[triangles[i + 1]];
            const int c = collapse[triangles[i + 2]];

            if (a == b || b == c || c == a) {
                continue;
            }

            tess.indexes[baseIndex + i]     = baseVertex + a;
            tess.indexes[baseIndex + i + 1] = baseVertex + b;
            tess.indexes[baseIndex + i + 2] = baseVertex + c;

            emitted += 3;
        }

        entityNumIndexes[backEnd.currentEntity - backEnd.refdef.entities] += emitted;
        tess.numIndexes += emitted;
    }

    bones = &TIKI_Skel_Bones_Arena[RB_FrameBoneArena()][backEnd.currentEntity->e.bonestart];

    morphs = &skeletorMorphCache[0];

    static vec3_t s_extMorph[TIKI_MAX_VERTEXES];
    qboolean      haveExtMorph = qfalse;

    if (skelmodel->pMorphData && skelmodel->pMorphData->pIndices) {
        skmMorphData_t *md = skelmodel->pMorphData;
        int             t, r, vLocal;

#define RB_SKEL_MAX_MORPH_TARGETS 128
        static int   skmTargetWeight[RB_SKEL_MAX_MORPH_TARGETS];   
        static float skmTargetPercent[RB_SKEL_MAX_MORPH_TARGETS];  
        if (md->numTargets > RB_SKEL_MAX_MORPH_TARGETS) {
            return;
        }
        for (t = 0; t < md->numTargets; t++) {
            skmTargetWeight[t] = 0;
        }
        for (t = 0; t < md->numTargets; t++) {
            skmTargetPercent[t] = (float)skmTargetWeight[t] * (1.0f / 100.0f);
        }

        for (t = 0; t < NUM_MORPH_CONTROLLERS; t++) {
            const morph_controller_t *c      = &backEnd.currentEntity->e.morph_controllers[t];
            int                       target = c->index;

            if (target == -1 || c->percent <= 0.001f) {
                continue;
            }
            if (morphMap) {
                target = morphMap[target];
            }
            if (target < 0 || target >= md->numTargets) {
                continue;
            }

            skmTargetPercent[target] = c->percent;
            haveExtMorph              = qtrue;
        }

#define SKM_MAX_WELD 8192
        static int      scatter[SKM_MAX_WELD];
        static qboolean scatterInit = qfalse;
        if (!scatterInit) {
            for (r = 0; r < SKM_MAX_WELD; r++) {
                scatter[r] = -1;
            }
            scatterInit = qtrue;
        }

        for (vLocal = 0; vLocal < (int)render_count; vLocal++) {
            VectorClear(s_extMorph[vLocal]);
        }

        for (t = 0; t < md->numTargets; t++) {
            float f = skmTargetPercent[t];

            if (f <= 0.0f) {
                continue;
            }

            for (r = 0; r < md->targets[t].numVerts; r++) {
                int wi = md->targets[t].verts[r].index;
                if (wi >= 0 && wi < SKM_MAX_WELD) {
                    scatter[wi] = r;
                }
            }

            for (vLocal = 0; vLocal < (int)render_count; vLocal++) {
                int globalV = surfaceBase + vLocal;
                int wi, rec;

                if (globalV < 0 || globalV >= md->numMorphVertices) {
                    continue;
                }
                wi = md->pIndices[globalV];
                if (wi < 0 || wi >= SKM_MAX_WELD) {
                    continue;
                }
                rec = scatter[wi];
                if (rec < 0) {
                    continue;
                }
                VectorMA(s_extMorph[vLocal], f, md->targets[t].verts[rec].delta, s_extMorph[vLocal]);
            }

            for (r = 0; r < md->targets[t].numVerts; r++) {
                int wi = md->targets[t].verts[r].index;
                if (wi >= 0 && wi < SKM_MAX_WELD) {
                    scatter[wi] = -1;
                }
            }
        }
#undef SKM_MAX_WELD
        for (t = 0; t < md->numTargets; t++) {
            if (skmTargetPercent[t] > 0.0f) {
                haveExtMorph = qtrue;
                break;
            }
        }
    }

    // not by the port's anim-frame flag: retail's controller-only path must reach the
    if (haveExtMorph) {
        for (vertNum = 0; vertNum < (int)render_count; vertNum++) {
            vec3_t normal;
            vec3_t out;
            vec3_t totalmorph;

            VectorClear(out);
            VectorClear(outXyz);
            VectorClear(totalmorph);

            weight = (skelWeight_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                      + sizeof(skeletorMorph_t) * newVerts->numMorphs);
            morph  = (skeletorMorph_t *)((byte *)newVerts + sizeof(skeletorVertex_t));

            for (morphNum = 0; morphNum < newVerts->numMorphs; morphNum++) {
                morphcache = &morphs[morph->morphIndex];

                if (*morphcache) {
                    SkelMorphGetXyz(morph, morphcache, totalmorph);
                }

                morph++;
            }

            if (haveExtMorph) {
                VectorAdd(totalmorph, s_extMorph[vertNum], totalmorph);
            }

            // ONE bone path, exactly as retail RB_SkelMesh @0x32560 has it.  Retail never
            bone = &bones[RB_SKEL_BONE_INDEX(weight->boneIndex)];
            SkelVertGetNormal(newVerts, bone, normal);

            for (weightNum = 0; weightNum < newVerts->numWeights; weightNum++) {
                bone = &bones[RB_SKEL_BONE_INDEX(weight->boneIndex)];

                if (!weightNum) {
                    SkelWeightMorphGetXyz(weight, bone, totalmorph, out);
                } else {
                    SkelWeightGetXyz(weight, bone, out);
                }

                weight++;
            }

            VectorCopy(normal, *outNormal);
            VectorScale(out, scale, outXyz);

            tess.texCoords[baseVertex + vertNum][0][0] = newVerts->texCoords[0];
            tess.texCoords[baseVertex + vertNum][0][1] = newVerts->texCoords[1];

            newVerts = (skeletorVertex_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                            + sizeof(skeletorMorph_t) * newVerts->numMorphs
                                            + sizeof(skelWeight_t) * newVerts->numWeights);
            outXyz += 4;
            outNormal++;
        }
    } else {
        for (vertNum = 0; vertNum < (int)render_count; vertNum++) {
            vec3_t normal;
            vec3_t out;

            VectorClear(out);
            VectorClear(outXyz);

            weight = (skelWeight_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                      + sizeof(skeletorMorph_t) * newVerts->numMorphs);

            bone = &bones[RB_SKEL_BONE_INDEX(weight->boneIndex)];
            SkelVertGetNormal(newVerts, bone, normal);

            for (weightNum = 0; weightNum < newVerts->numWeights; weightNum++) {
                bone = &bones[RB_SKEL_BONE_INDEX(weight->boneIndex)];

                SkelWeightGetXyz(weight, bone, out);

                weight++;
            }

            VectorCopy(normal, *outNormal);
            VectorScale(out, scale, outXyz);

            tess.texCoords[baseVertex + vertNum][0][0] = newVerts->texCoords[0];
            tess.texCoords[baseVertex + vertNum][0][1] = newVerts->texCoords[1];

            newVerts = (skeletorVertex_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                            + sizeof(skeletorMorph_t) * newVerts->numMorphs
                                            + sizeof(skelWeight_t) * newVerts->numWeights);
            outXyz += 4;
            outNormal++;
        }
    }


#if 0
	if( backEnd.currentEntity->e.staticModelIndex ) {
		mstaticModel_t *sm;
		color4ub_t *out;
		color3ub_t *in;
		int cdofs;
		skdSurface_t *sf2;

		sm = &tr.world->staticModels[ backEnd.currentEntity->e.staticModelIndex - 1 ];

		tess.useStaticModelVertexColors = qtrue;

		cdofs = 0;
		sf2 = tiki->surfs;
		while( sf2 != sf ) {
			cdofs += sf2->numVerts;
			sf2 = ( skdSurface_t* )( ( ( byte* )sf2 ) + sf2->ofsEnd );
		}

		in = &tr.world->smColors[ sm->firstVert + cdofs ];
		out = tess.vertexColors + baseVertex;
		for( i = 0; i < sf->numVerts; i++, in++, out++ ) {
#    if 1
			( *out )[ 0 ] = ( *in )[ 0 ];
			( *out )[ 1 ] = ( *in )[ 1 ];
			( *out )[ 2 ] = ( *in )[ 2 ];
			( *out )[ 3 ] = 255;
#    elif 0
			( ( int* )out ) = tr.identityLightByte;
#    else		
			( *out )[ 0 ] = 255;
			( *out )[ 1 ] = 0;
			( *out )[ 2 ] = 0;
			( *out )[ 3 ] = 255;
#    endif
		}
	} 
#endif

    tess.entityFade = backEnd.currentEntity->e.fade;
}

#undef RB_SKEL_BONE_INDEX

void RB_TikiMesh(skelSurfaceGame_t *sf)
{
    skelSurfaceGame_t *psurface;
    skelHeaderGame_t  *skelmodel;
    dtiki_t           *tiki;
    skelIndex_t       *triangles;
    int               *collapse_map;
    int                collapse[TIKI_MAX_VERTEXES];
    int                surf, i, emitted, indexes, render_count, lod_cutoff;
    int                baseIndex, baseVertex;
    skeletorVertex_t  *newVerts;
    float             *outXyz;
    vec4_t            *outNormal;
    float              scale;
    vec3_t             tiki_localorigin;
    qboolean           bFound;

    if (!backEnd.currentEntity || !backEnd.currentEntity->e.tiki) {
        return;
    }

    tiki  = backEnd.currentEntity->e.tiki;
    scale = tiki->load_scale * backEnd.currentEntity->e.scale;

    VectorScale(tiki->load_origin, scale, tiki_localorigin);

    skelmodel = NULL;
    {
        const int anim = backEnd.currentEntity->e.anim;

        if (tiki->a && tiki->a->tanMesh && anim >= 0 && anim < tiki->a->num_anims
            && tiki->a->tanMesh[anim] >= 0) {
            skelmodel = ri.TIKI_GetSkel(tiki->a->tanMesh[anim]);
        }
    }
    if (!skelmodel && tiki->numMeshes > 0) {
        skelmodel = ri.TIKI_GetSkel(tiki->mesh[0]);
    }
    if (!skelmodel) {
        return;
    }

    bFound    = qfalse;
    psurface = skelmodel->pSurfaces;
    for (surf = 0; surf < skelmodel->numSurfaces; surf++) {
        if (psurface == sf) {
            bFound = qtrue;
            break;
        }
        psurface = psurface->pNext;
    }

    if (!bFound) {
        return;
    }

    if (backEnd.viewParms.isPortal) {
        lod_cutoff = GetLodCutoff(sf, backEnd.currentEntity->lodpercentage[1]);
    } else {
        lod_cutoff = GetLodCutoff(sf, backEnd.currentEntity->lodpercentage[0]);
    }

    render_count = lod_cutoff;

    if (!render_count) {
        return;
    }
    if (RB_TikiVertCap(render_count, skelmodel, sf)) {
        return;
    }

    indexes = sf->numTriangles * 3;
    RB_CHECKOVERFLOW(render_count, indexes);

    collapse_map = sf->pCollapse;
    triangles    = sf->pTriangles;
    baseIndex    = tess.numIndexes;
    baseVertex   = tess.numVertexes;
    tess.numVertexes += render_count;

    if (render_count == sf->numVerts) {
        for (i = 0; i < indexes; i++) {
            tess.indexes[baseIndex + i] = baseVertex + triangles[i];
        }

        entityNumIndexes[backEnd.currentEntity - backEnd.refdef.entities] += indexes;
        tess.numIndexes += indexes;
    } else {
        emitted = 0;

        for (i = 0; i < render_count; i++) {
            collapse[i] = i;
        }
        for (i = render_count; i < sf->numVerts; i++) {
            collapse[i] = collapse[collapse_map[i] & 0xFFFF];
        }

        for (i = 0; i < indexes; i += 3) {
            const int a = collapse[triangles[i + 0]];
            const int b = collapse[triangles[i + 1]];
            const int c = collapse[triangles[i + 2]];

            if (a == b || b == c || c == a) {
                continue;
            }

            tess.indexes[baseIndex + i]     = baseVertex + a;
            tess.indexes[baseIndex + i + 1] = baseVertex + b;
            tess.indexes[baseIndex + i + 2] = baseVertex + c;

            emitted += 3;
        }

        entityNumIndexes[backEnd.currentEntity - backEnd.refdef.entities] += emitted;
        tess.numIndexes += emitted;
    }

    newVerts  = sf->pVerts;
    outXyz    = tess.xyz[baseVertex];
    outNormal = &tess.normal[baseVertex];

    // port-only `r_tanmorph' switch (0 retail hits) is gone -- but the site it stood on is
    // NOT retail-free: 0x31DC0-0x31DF4 read RETAIL's `r_lerpmodels' and choose the
    //   0x31DDC  cmplw r25, r26        ; else: the two surface records must differ
    //   0x31DE4  cmplw r28, r27        ;       and so must the two frame records
    if (skelmodel->pTanFrameXform && sf->pTanXyz) {
        int                   tanFrame, tanOldFrame;
        float                 backLerp, frontLerp;
        const float          *xfNew;
        const float          *xfOld;
        const tanXyzNormal_t *vNew;
        const tanXyzNormal_t *vOld;

        R_TanGetFrames(skelmodel, &backEnd.currentEntity->e, &tanOldFrame, &tanFrame, &frontLerp);

        if (!r_lerpmodels->integer) {
            frontLerp = 1.0f;
        }
        backLerp = 1.0f - frontLerp;

        xfNew = &skelmodel->pTanFrameXform[tanFrame * 6];
        xfOld = &skelmodel->pTanFrameXform[tanOldFrame * 6];
        vNew  = sf->pTanXyz + (size_t)tanFrame * sf->numVerts;
        vOld  = sf->pTanXyz + (size_t)tanOldFrame * sf->numVerts;

        for (i = 0; i < render_count; i++) {
            vec3_t normal;
            vec3_t out;

            if (tanFrame == tanOldFrame) {
                out[0] = (float)vNew[i].xyz[0] * xfNew[0] + xfNew[3];
                out[1] = (float)vNew[i].xyz[1] * xfNew[1] + xfNew[4];
                out[2] = (float)vNew[i].xyz[2] * xfNew[2] + xfNew[5];
                R_TanDecodeNormal(vNew[i].normal, normal);
            } else {
                vec3_t nNew, nOld;

                out[0] = frontLerp * ((float)vNew[i].xyz[0] * xfNew[0] + xfNew[3])
                       + backLerp * ((float)vOld[i].xyz[0] * xfOld[0] + xfOld[3]);
                out[1] = frontLerp * ((float)vNew[i].xyz[1] * xfNew[1] + xfNew[4])
                       + backLerp * ((float)vOld[i].xyz[1] * xfOld[1] + xfOld[4]);
                out[2] = frontLerp * ((float)vNew[i].xyz[2] * xfNew[2] + xfNew[5])
                       + backLerp * ((float)vOld[i].xyz[2] * xfOld[2] + xfOld[5]);

                R_TanDecodeNormal(vNew[i].normal, nNew);
                R_TanDecodeNormal(vOld[i].normal, nOld);
                normal[0] = frontLerp * nNew[0] + backLerp * nOld[0];
                normal[1] = frontLerp * nNew[1] + backLerp * nOld[1];
                normal[2] = frontLerp * nNew[2] + backLerp * nOld[2];
                VectorNormalize(normal);
            }

            VectorCopy(normal, *outNormal);
            VectorScale(out, scale, outXyz);
            VectorAdd(outXyz, tiki_localorigin, outXyz);

            tess.texCoords[baseVertex + i][0][0] = newVerts->texCoords[0];
            tess.texCoords[baseVertex + i][0][1] = newVerts->texCoords[1];

            newVerts = (skeletorVertex_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                            + sizeof(skeletorMorph_t) * newVerts->numMorphs
                                            + sizeof(skelWeight_t) * newVerts->numWeights);
            outXyz += 4;
            outNormal++;
        }
    } else {
        for (i = 0; i < render_count; i++) {
            skelWeight_t *weight = (skelWeight_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                                    + sizeof(skeletorMorph_t) * newVerts->numMorphs);
            vec3_t        normal;
            vec3_t        out;

            VectorScale(weight->offset, scale, out);
            VectorCopy(newVerts->normal, normal);

            VectorCopy(normal, *outNormal);
            VectorAdd(out, tiki_localorigin, outXyz);

            tess.texCoords[baseVertex + i][0][0] = newVerts->texCoords[0];
            tess.texCoords[baseVertex + i][0][1] = newVerts->texCoords[1];

            newVerts = (skeletorVertex_t *)((byte *)newVerts + sizeof(skeletorVertex_t)
                                            + sizeof(skeletorMorph_t) * newVerts->numMorphs
                                            + sizeof(skelWeight_t) * newVerts->numWeights);
            outXyz += 4;
            outNormal++;
        }
    }

    tess.entityFade = backEnd.currentEntity->e.fade;
}

void R_AddTikiSurfaces(trRefEntity_t *ent)
{
    dtiki_t          *tiki;
    skelHeaderGame_t *skelmodel;
    int               fogNum = 0;

    tiki = ent->e.tiki;

    if (!tiki) {
        return;
    }

    skelmodel = (tiki->numMeshes > 0) ? ri.TIKI_GetSkel(tiki->mesh[0]) : NULL;

    if (tiki->skelIndex >= 0) {
        R_AddSkelSurfaces(ent);
        return;
    }

    if (!skelmodel) {
        ri.Printf(PRINT_DEVELOPER, "R_AddTikiSurfaces: couldn't get tiki '%s'\n", tiki->a->name);
        return;
    }


    // 0x26B2C for the frame -- and this arm does NOT bail on its qfalse: the clamped
    {
        int   anim  = ent->e.anim;
        short frame = ent->e.frame;
        int   oldanim  = ent->e.oldanim;
        short oldframe = ent->e.oldframe;

        R_ValidateSkelAnimAndFrame(tiki, &anim, &frame, qtrue);
        R_ValidateSkelAnimAndFrame(tiki, &oldanim, &oldframe, qtrue);

        ent->e.anim     = anim;
        ent->e.frame    = frame;
        ent->e.oldanim  = oldanim;
        ent->e.oldframe = oldframe;
    }

    {
        float        tiki_scale = tiki->load_scale * ent->e.scale;
        vec3_t       tiki_localorigin;
        vec3_t       tiki_entityorigin;
        tikiFrame_t *tanNewFrame;
        tikiFrame_t *tanOldFrame;

        VectorScale(tiki->load_origin, tiki_scale, tiki_localorigin);

        VectorAdd(ent->e.origin, tiki_localorigin, tiki_entityorigin);

        {
            int          numNewFrames = 0;
            int          numOldFrames = 0;
            tikiFrame_t *newFrames    = ri.TIKI_Anim_TanFrames(tiki, ent->e.anim, &numNewFrames);
            tikiFrame_t *oldFrames;

            if (!newFrames || numNewFrames <= 0) {
                ri.Printf(
                    PRINT_DEVELOPER,
                    "R_AddTikiSurfaces: couldn't get anim %d in '%s'\n",
                    ent->e.anim,
                    tiki->a->name
                );
                return;
            }

            oldFrames = ri.TIKI_Anim_TanFrames(tiki, ent->e.oldanim, &numOldFrames);
            if (!oldFrames || numOldFrames <= 0) {
                ri.Printf(
                    PRINT_DEVELOPER,
                    "R_AddTikiSurfaces: couldn't get anim %d in '%s'\n",
                    ent->e.oldanim,
                    tiki->a->name
                );
                return;
            }

            tanNewFrame = &newFrames[ent->e.frame];
            tanOldFrame = &oldFrames[ent->e.oldframe];
        }

        if (ent->e.fade != 0.0f) {
            if (R_CullTikiModel(tiki, &ent->e, tanNewFrame, tanOldFrame, tiki_scale, tiki_localorigin) != CULL_OUT) {
                if (tr.viewParms.isPortal) {
                    ent->lodpercentage[1] = R_TikiCalcLodDist(tiki_entityorigin, tiki);
                } else {
                    ent->lodpercentage[0] = R_TikiCalcLodDist(tiki_entityorigin, tiki);
                }

                {
                    float fogRadius = tiki_scale * tanNewFrame->radius;

                    fogNum = R_CalcFogNum(tiki_entityorigin, fogRadius);
                }

                qboolean fadeMain   = qfalse;
                qboolean fadeSecond = qfalse;

                if (ent->e.fade < 1.0f) {
                    fadeMain   = qtrue;
                    fadeSecond = qtrue;
                }
                if (ent->e.shaderRGBA[3] != 255) {
                    if (ent->e.renderfx & RF_FORCE_ALPHA) {
                        fadeMain   = qtrue;
                        fadeSecond = qtrue;
                    }
                    if (ent->e.renderfx & RF_FORCE_ALPHA_EFFECTS) {
                        fadeSecond = qtrue;
                    }
                }
                if (fadeMain || fadeSecond) {
                    ent->e.fade *= (float)ent->e.shaderRGBA[3] / 255.0f;
                }

                {
                    dtikisurface_t    *dsurf = tiki->surfaces;
                    byte              *bsurf = &ent->e.surfaces[0];
                    qboolean           personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal;
                    int                i;
                    int                lodMaxStep = 0;
                    const int          animMesh = ((tiki->a && tiki->a->tanMesh && ent->e.anim >= 0
                                                    && ent->e.anim < tiki->a->num_anims)
                                                       ? tiki->a->tanMesh[ent->e.anim]
                                                       : -1);
                    skelHeaderGame_t  *sm = (animMesh >= 0) ? ri.TIKI_GetSkel(animMesh) : NULL;
                    skelSurfaceGame_t *surface;

                    if (!sm) {
                        ri.Printf(
                            PRINT_DEVELOPER,
                            "R_AddTikiSurfaces: couldn't get anim %d in '%s'\n",
                            ent->e.anim,
                            tiki->a->name
                        );
                        return;
                    }

                    surface = sm->pSurfaces;
                    for (i = 0; i < tiki->num_surfaces; i++, dsurf++, bsurf++, surface = surface->pNext) {
                        shader_t *shader;
                        int       iShaderNum;

                            if (surface->pCollapse && surface->numVerts > 0) {
                                int step = (int)((unsigned)surface->pCollapse[surface->numVerts - 1] >> 16);

                                if (step > lodMaxStep) {
                                    lodMaxStep = step;
                                }
                            }

                            if (*bsurf & 4) {
                                continue;
                            }

                            surface->ident = SF_TIKI_MESH;

                            if (!(ent->e.customShader) || (ent->e.renderfx & RF_CUSTOMSHADERPASS)) {
                                iShaderNum = ent->e.skinNum + (*bsurf & 3);

                                if (iShaderNum >= dsurf->numskins) {
                                    iShaderNum = 0;
                                }
                                shader = tr.shaders[dsurf->hShader[iShaderNum]];
                            } else {
                                shader = R_GetShaderByHandle(ent->e.customShader);
                            }

                            if (fadeMain) {
                                shader = R_FindShaderForceAlpha(shader->name);
                            }

                            if (!personalModel && r_shadows->integer == 2 && fogNum == 0
                                && !(ent->e.renderfx & RF_FIRST_PERSON) && (ent->e.renderfx & RF_SHADOW)
                                && (ent->e.renderfx & RF_SHADOW_PRECISE) && shader->sort == (float)SS_OPAQUE) {
                                R_AddDrawSurf((surfaceType_t *)surface, tr.shadowShader, 0, 0);
                            }

                            if (!personalModel) {
                                if ((*bsurf & 0x40) && (dsurf->numskins > 1)) {
                                    int iCross = ent->e.skinNum + (*bsurf & 2);

                                    R_AddDrawSurf((surfaceType_t *)surface, tr.shaders[dsurf->hShader[iCross]], fogNum, 0);
                                    R_AddDrawSurf((surfaceType_t *)surface, tr.shaders[dsurf->hShader[iCross + 1]], fogNum, 0);
                                } else {
                                    R_AddDrawSurf((surfaceType_t *)surface, shader, fogNum, 0);
                                }
                            }

                            if ((ent->e.customShader) && (ent->e.renderfx & RF_CUSTOMSHADERPASS)) {
                                shader = R_GetShaderByHandle(ent->e.customShader);
                                if (fadeSecond) {
                                    shader = R_FindShaderForceAlpha(shader->name);
                                }
                                R_AddDrawSurf((surfaceType_t *)surface, shader, fogNum, 0);
                            }
                        }

                    for (surface = sm->pSurfaces, i = 0; i < sm->numSurfaces; surface = surface->pNext, i++) {
                        surface->numModelVerts = lodMaxStep;
                    }
                }

                ent->e.of   = (skelFrame_t *)tanOldFrame;
                ent->e.nf   = (skelFrame_t *)tanNewFrame;
                ent->e.tiki = tiki;

                R_ShowTikiLodDebugNumbers(ent, tiki);
            }
        }
    }
}


void R_InfoWorldTris_f(void)
{
    int i;

    g_bInfoworldtris = qtrue;

    for (i = 0; i < (int)ARRAY_LEN(entityNumIndexes); i++) {
        entityNumIndexes[i] = 0;
    }
    for (i = 0; i < (int)ARRAY_LEN(staticModelNumIndexes); i++) {
        staticModelNumIndexes[i] = 0;
    }
}

void R_PrintInfoWorldtris(void)
{
    int               i;
    int               numTris;
    int               totalNumTris;
    dtiki_t          *tiki;
    skelHeaderGame_t *skelmodel;

    totalNumTris = 0;

    for (i = 0; i < (int)ARRAY_LEN(entityNumIndexes); i++) {
        numTris = entityNumIndexes[i] / 3;
        if (!numTris) {
            continue;
        }

        totalNumTris += numTris;
        tiki      = backEnd.refdef.entities[i].e.tiki;
        skelmodel = ri.TIKI_GetSkel(tiki->mesh[0]);
        ri.Printf(PRINT_ALL, "ent: %i, tris: %i, %s, version: %i\n", i, numTris, tiki->a->name, skelmodel->version);
    }

    ri.Printf(PRINT_ALL, "total entity tris: %i\n\n", totalNumTris);

    totalNumTris = 0;

    for (i = 0; i < (int)ARRAY_LEN(entityNumIndexes); i++) {
        numTris = staticModelNumIndexes[i] / 3;
        if (!numTris) {
            continue;
        }

        totalNumTris += numTris;
        tiki      = backEnd.refdef.staticModels[i].tiki;
        skelmodel = ri.TIKI_GetSkel(tiki->mesh[0]);
        ri.Printf(PRINT_ALL, "sm: %i, tris: %i, %s, version: %i\n", i, numTris, tiki->a->name, skelmodel->version);
    }

    ri.Printf(PRINT_ALL, "total static model tris: %i\n\n", totalNumTris);
}

void RE_SetFrameNumber(int frameNumber)
{
    tr.frame_skel_index = frameNumber;
}


// tr.skel_index[].  Port-only renderer state -- it has no retail counterpart, because
// retail never builds a pose it cannot resolve: its R_AddSkelSurfaces bails before it
static short s_framePoseCount[1024];

int R_UpdatePoseInternal(refEntity_t *model)
{
    frameInfo_t     frameInfo[MAX_FRAMEINFOS];
    short int       bodyParts[MAX_FRAMEINFOS];
    int             numFrames;

    if (model->entityNumber != ENTITYNUM_NONE) {
        if (tr.skel_index[model->entityNumber] == tr.frame_skel_index) {
            return s_framePoseCount[model->entityNumber];
        }
        tr.skel_index[model->entityNumber] = tr.frame_skel_index;
    }

    numFrames = R_ResolveEntityPose(model, frameInfo, bodyParts);

    if (numFrames > 0) {
        int    boneTag[NUM_BONE_CONTROLLERS];
        vec4_t boneQuat[NUM_BONE_CONTROLLERS];

        R_BoneControllerGate(model, boneTag, boneQuat);

        ri.TIKI_SetPoseChannelsInternal(
            ri.TIKI_GetSkeletor(model->tiki, model->entityNumber),
            frameInfo,
            bodyParts,
            boneTag,
            boneQuat,
            model->actionWeight
        );
    } else {
        // retail shape -- retail never builds a pose for such an entity, it just refuses to
    }

    if (model->entityNumber != ENTITYNUM_NONE) {
        s_framePoseCount[model->entityNumber] = numFrames;
    }

    return numFrames;
}

void RE_ForceUpdatePose(refEntity_t *model)
{
    frameInfo_t     frameInfo[MAX_FRAMEINFOS];
    short int       bodyParts[MAX_FRAMEINFOS];
    int             numFrames;

    if (model->entityNumber != ENTITYNUM_NONE) {
        tr.skel_index[model->entityNumber] = tr.frame_skel_index;
    }

    numFrames = R_ResolveEntityPose(model, frameInfo, bodyParts);

    if (numFrames > 0) {
        int    boneTag[NUM_BONE_CONTROLLERS];
        vec4_t boneQuat[NUM_BONE_CONTROLLERS];

        R_BoneControllerGate(model, boneTag, boneQuat);

        ri.TIKI_SetPoseChannelsInternal(
            ri.TIKI_GetSkeletor(model->tiki, model->entityNumber),
            frameInfo,
            bodyParts,
            boneTag,
            boneQuat,
            model->actionWeight
        );
    }

    if (model->entityNumber != ENTITYNUM_NONE) {
        s_framePoseCount[model->entityNumber] = numFrames;
    }
}

orientation_t RE_TIKI_Orientation(refEntity_t *model, int tagnum)
{
    R_UpdatePoseInternal(model);
    return ri.TIKI_OrientationInternal(model->tiki, model->entityNumber, tagnum, model->scale);
}

qboolean RE_TIKI_IsOnGround(refEntity_t *model, int tagnum, float threshold)
{
    R_UpdatePoseInternal(model);
    return ri.TIKI_IsOnGroundInternal(model->tiki, model->entityNumber, tagnum, threshold);
}

float R_GetRadius(refEntity_t *model)
{
    R_UpdatePoseInternal(model);
    return ri.GetRadiusInternal(model->tiki, model->entityNumber, model->scale);
}

void R_GetFrame(refEntity_t *model, struct skelAnimFrame_s *newFrame)
{
    R_UpdatePoseInternal(model);
    ri.GetFrameInternal(model->tiki, model->entityNumber, newFrame);
}

static int R_CullSkelModel(dtiki_t *tiki, refEntity_t *e, skelFrame_t *nf, skelFrame_t *of, float fScale, float *vLocalOrg)
{
    vec3_t bounds[2];
    vec3_t delta;
    int    i;
    int    cull;

    if (!e->nonNormalizedAxes) {
        int v13;

        v13 = R_CullLocalPointAndRadius(vLocalOrg, nf->radius * fScale);

        if (of != nf && of) {
            if (v13 == R_CullLocalPointAndRadius(vLocalOrg, of->radius * fScale)) {
                if (v13 == CULL_OUT) {
                    tr.pc.c_box_cull_md3_out++;
                    return CULL_OUT;
                }

                if (!v13) {
                    tr.pc.c_box_cull_md3_in++;
                    return CULL_IN;
                }

                tr.pc.c_box_cull_md3_clip++;
            }
        } else {
            if (v13 == CULL_CLIP) {
                tr.pc.c_box_cull_md3_clip++;
            } else if (v13 >= CULL_CLIP) {
                if (v13 < 3) {
                    tr.pc.c_box_cull_md3_out++;
                    return CULL_OUT;
                }
            } else if (v13 >= CULL_IN) {
                tr.pc.c_box_cull_md3_in++;
                return CULL_IN;
            }
        }
    }

    if (tr.currentEntity && (tr.currentEntity->e.renderfx & RF_FRAMELERP)) {
        delta[0] = e->origin[0] - e->attachOrigin[0];
        delta[1] = e->origin[1] - e->attachOrigin[1];
        delta[2] = e->origin[2] - e->attachOrigin[2];
    } else {
        VectorClear(delta);
    }

    for (i = 0; i < 3; i++) {
        if (of != nf && of) {
            float a;
            float b;

            a = nf->bounds[0][i] * fScale + vLocalOrg[i];
            b = of->bounds[0][i] * fScale + vLocalOrg[i];
            bounds[0][i] = (b < a) ? b : a;

            a = of->bounds[1][i] * fScale + vLocalOrg[i];
            b = nf->bounds[1][i] * fScale + vLocalOrg[i];
            bounds[1][i] = (a < b) ? a : b;
        } else {
            bounds[0][i] = nf->bounds[0][i] * fScale + vLocalOrg[i];
            bounds[1][i] = nf->bounds[1][i] * fScale + vLocalOrg[i];
        }

        if (delta[i] <= 0.0f) {
            bounds[0][i] += delta[i];
        } else {
            bounds[1][i] += delta[i];
        }
    }

    cull = R_CullLocalBox(bounds);



    switch (cull) {
    case CULL_IN:
        tr.pc.c_box_cull_md3_in++;
        return CULL_IN;
    case CULL_CLIP:
        tr.pc.c_box_cull_md3_clip++;
        return CULL_CLIP;
    case CULL_OUT:
    default:
        tr.pc.c_box_cull_md3_out++;
        return CULL_OUT;
    }
}

static int R_CullTikiModel(dtiki_t *tiki, refEntity_t *e, tikiFrame_t *nf, tikiFrame_t *of, float fScale, float *vLocalOrg)
{
    vec3_t bounds[2];
    vec3_t delta;
    int    i;
    int    cull;

    if (!e->nonNormalizedAxes) {
        int v13;

        v13 = R_CullLocalPointAndRadius(vLocalOrg, nf->radius * fScale);

        if (of == nf) {
            if (v13 == CULL_CLIP) {
                tr.pc.c_box_cull_md3_clip++;
            } else if (v13 >= CULL_CLIP) {
                if (v13 < 3) {
                    tr.pc.c_box_cull_md3_out++;
                    return CULL_OUT;
                }
            } else if (v13 >= CULL_IN) {
                tr.pc.c_box_cull_md3_in++;
                return CULL_IN;
            }
        } else {
            if (v13 == R_CullLocalPointAndRadius(vLocalOrg, of->radius * fScale)) {
                if (v13 == CULL_OUT) {
                    tr.pc.c_box_cull_md3_out++;
                    return CULL_OUT;
                }

                if (!v13) {
                    tr.pc.c_box_cull_md3_in++;
                    return CULL_IN;
                }

                tr.pc.c_box_cull_md3_clip++;
            }
        }
    }

    if (tr.currentEntity && (tr.currentEntity->e.renderfx & RF_FRAMELERP)) {
        delta[0] = e->origin[0] - e->attachOrigin[0];
        delta[1] = e->origin[1] - e->attachOrigin[1];
        delta[2] = e->origin[2] - e->attachOrigin[2];
    } else {
        VectorClear(delta);
    }

    for (i = 0; i < 3; i++) {
        if (of != nf) {
            float a;
            float b;

            a = of->bounds[0][i] * fScale + vLocalOrg[i];
            b = nf->bounds[0][i] * fScale + vLocalOrg[i];
            bounds[0][i] = (a < b) ? a : b;

            a = of->bounds[1][i] * fScale + vLocalOrg[i];
            b = nf->bounds[1][i] * fScale + vLocalOrg[i];
            bounds[1][i] = (a < b) ? a : b;
        } else {
            bounds[0][i] = nf->bounds[0][i] * fScale + vLocalOrg[i];
            bounds[1][i] = nf->bounds[1][i] * fScale + vLocalOrg[i];
        }

        if (delta[i] <= 0.0f) {
            bounds[0][i] += delta[i];
        } else {
            bounds[1][i] += delta[i];
        }
    }

    cull = R_CullLocalBox(bounds);

    switch (cull) {
    case CULL_IN:
        tr.pc.c_box_cull_md3_in++;
        return CULL_IN;
    case CULL_CLIP:
        tr.pc.c_box_cull_md3_clip++;
        return CULL_CLIP;
    case CULL_OUT:
    default:
        tr.pc.c_box_cull_md3_out++;
        return CULL_OUT;
    }
}

void R_CountTikiLodTris(dtiki_t *tiki, float lodpercentage, int *render_tris, int *total_tris)
{
    skelHeaderGame_t  *skelmodel;
    skelSurfaceGame_t *surface;
    const int         *collapse_map;
    const skelIndex_t *triangles;
    int                remap[TIKI_MAX_VERTEXES];
    int                cutoff;
    int                indexes;
    int                mesh, surf, i, k;

    *render_tris = 0;
    *total_tris  = 0;

    for (mesh = 0; mesh < tiki->numMeshes; mesh++) {
        skelmodel = ri.TIKI_GetSkel(tiki->mesh[mesh]);
        surface   = skelmodel->pSurfaces;

        for (surf = 0; surf < skelmodel->numSurfaces; surf++) {
            cutoff  = GetLodCutoff(surface, lodpercentage);
            indexes = surface->numTriangles * 3;

            *total_tris += surface->numTriangles;

            triangles    = surface->pTriangles;
            collapse_map = surface->pCollapse;

            for (i = 0; i < cutoff; i++) {
                remap[i] = i;
            }

            for (i = cutoff; i < surface->numVerts; i++) {
                remap[i] = remap[collapse_map[i] & 0xFFFF];
            }

            for (k = 0; k < indexes; k += 3) {
                const int a = remap[triangles[k + 0]];
                const int b = remap[triangles[k + 1]];
                const int c = remap[triangles[k + 2]];

                if (a != b && b != c && c != a) {
                    *render_tris += 1;
                }
            }

            surface = surface->pNext;
        }
    }
}

int R_LerpTag(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, float frac, const char *tagName)
{
    (void)tag;
    (void)handle;
    (void)startFrame;
    (void)endFrame;
    (void)frac;
    (void)tagName;
    return 0;
}