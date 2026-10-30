/*
===========================================================================
Copyright (C) 2023-2024 the OpenMoHAA team

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
#include "tr_vbo.h"

#define MAX_FLARES 20

static void R_DrawEmitterLensflares(void);
static void R_AddDlightFlares(void);
static void R_AddSpherelFlares(void);
static void R_AddEntityFlares(void);

class flare_s
{
public:
    float     size;
    float     where;
    shader_t *shader;
    float     alphascale;
};

class lens_flare
{
public:
    static int max_flares;

    float dot_min;
    float fullscale;

    int     num_flares;
    flare_s flares[MAX_FLARES];

    shader_t *fullscreen;
    int       fullfade;
    qboolean  blend_active;

    vec3_t trace_v;
    vec3_t v;

    vec3_t color;
    float  alpha;

    vec2_t screen;
    float  dot;

    int   lasttime;
    float lastbright;

    bool inportalsky;

    qboolean initted;

public:
    lens_flare()
    {
        color[0] = 1.0;
        color[1] = 1.0;
        color[2] = 1.0;
        alpha    = 1.0;
        initted  = false;
    }

    bool         CheckRange();
    bool         CheckRay();
    bool         ScreenCalc();
    virtual void Init(const char *which);
    void         ScreenBlend();
    void         InPortalSky();
    void         NotInPortalSky();
    void         SetVect(const float *vect);

    void SetColor(const float *c) { VectorCopy(c, color); }

    void SetAlpha(float a) { alpha = a; }
};

class sun_flare_class : public lens_flare
{
public:
    bool SunCheckRay();
    void SunScreenCalc();
    void SunTry();
};

sun_flare_class   sunFlare;
int               lens_flare::max_flares;

void lens_flare::SetVect(const float *vect)
{
    if (inportalsky) {
        vec3_t offset;
        vec3_t rot_offset;

        /* frame-carry: the walk reads the record's refdef */
        VectorSubtract(vect, backEnd.refdef.sky_origin, offset);
        VectorRotate(offset, backEnd.refdef.sky_axis, rot_offset);
        VectorAdd(backEnd.refdef.vieworg, rot_offset, v);

        VectorNormalize(offset);
        VectorMA(backEnd.refdef.vieworg, 16384, offset, trace_v);
    } else {
        VectorCopy(vect, v);
        VectorCopy(vect, trace_v);
    }
}

void lens_flare::InPortalSky()
{
    inportalsky = true;
}

void lens_flare::NotInPortalSky()
{
    inportalsky = false;
}

bool lens_flare::CheckRange()
{
    vec3_t diff;

    VectorSubtract(v, backEnd.viewParms.ori.origin, diff);
    VectorNormalizeFast(diff);
    dot = DotProduct(backEnd.viewParms.ori.axis[0], diff);

    return dot > dot_min;
}

bool lens_flare::CheckRay()
{
    trace_t trace;

    ri.CM_BoxTrace(&trace, backEnd.viewParms.ori.origin, trace_v, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);
    if (inportalsky) {
        return (trace.surfaceFlags & 4) != 0;
    }

    return trace.fraction == 1.0;
}

bool lens_flare::ScreenCalc()
{
    vec4_t eye;
    vec4_t clip;
    vec4_t point;
    bool   retval;
    int    i, j;

    retval = true;

    VectorCopy(v, point);
    point[3] = 1.0;

    VectorClear4(eye);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            eye[i] += point[j] * backEnd.ori.modelMatrix[i + j * 4];
        }
    }

    VectorClear4(clip);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            clip[i] += eye[j] * backEnd.viewParms.projectionMatrix[i + j * 4];
        }
    }

    for (i = 0; i < 3; i++) {
        if (clip[i] >= clip[3] || clip[i] <= -clip[3]) {
            retval = false;
        }
    }

    for (i = 0; i < 2; i++) {
        screen[i] = clip[i] / clip[3];
    }

    return retval;
}

void lens_flare::ScreenBlend()
{
    float fadeAlpha;
    int   timediff;

    if (!fullscreen || !blend_active) {
        return;
    }

    timediff = backEnd.refdef.time - lasttime;
    if (timediff > fullfade) {
        blend_active = false;
        return;
    }

    fadeAlpha = (float)(fullfade - timediff) / fullfade * lastbright;
    if (fadeAlpha <= 0.0001) {
        blend_active = false;
        return;
    }

    RB_Color4f(1.0, 1.0, 1.0, fadeAlpha);

    RB_StreamBegin(fullscreen);
    RB_Vertex2f(-1.0, -1.0);
    RB_Vertex2f(1.0, -1.0);
    RB_Vertex2f(-1.0, 1.0);
    RB_Vertex2f(1.0, 1.0);

    RB_StreamEnd();
}

class tmpRiFile
{
private:
    bool  file_exists;
    void *buf;

public:
    tmpRiFile()
    {
        file_exists = false;
        buf         = NULL;
    }

    ~tmpRiFile() { close(); }

    int open(const char *which, void **buffer)
    {
        close();

        if (ri.FS_ReadFile(which, buffer) != -1) {
            file_exists = true;
            buf         = *buffer;
        }

        return file_exists;
    }

    bool exists() const { return file_exists; }

    void close()
    {
        if (file_exists) {
            ri.FS_FreeFile(buf);
            file_exists = false;
        }
    }
};

void lens_flare::Init(const char *which)
{
    char       *file;
    const char *token;
    tmpRiFile   theFile;

    initted = false;
    theFile.open("global/lensflare1.txt", (void **)&file);

    if (!theFile.exists()) {
        ri.Printf(PRINT_WARNING, "WARNING: Could not open lens flare info file!\n");
        return;
    }

    while ((token = COM_ParseExt(&file, qtrue))) {
        if (!file) {
            ri.Printf(PRINT_WARNING, "WARNING: could not find begin for section '%s' in lensflares\n", which);
            return;
        }

        if (!Q_stricmp(token, "begin")) {
            token = COM_ParseExt(&file, qfalse);
            if (!Q_stricmp(token, which)) {
                break;
            }
        }
    }

    dot_min      = 0.8f;
    fullscale    = 0.7f;
    num_flares   = 0;
    fullscreen   = NULL;
    initted      = true;
    fullfade     = 0;
    blend_active = false;

    while ((token = COM_ParseExt(&file, qtrue))) {
        if (!file) {
            ri.Printf(PRINT_WARNING, "WARNING: could not find end for section '%s' in lensflares\n", which);
            break;
        }

        if (!Q_stricmp(token, "dot_min")) {
            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: invalid dot_min in lensflares\n");
                continue;
            }
            dot_min = atof(token);
        } else if (!Q_stricmp(token, "flare")) {
            assert(num_flares < MAX_FLARES);

            flares[num_flares].alphascale = 1.0;

            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: no flare arg 1 for flare '%d' in lensflares\n", num_flares);
                continue;
            }
            flares[num_flares].size = atof(token);

            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: no flare arg 2 for flare '%d' in lensflares\n", num_flares);
                continue;
            }
            flares[num_flares].where = atof(token);

            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: no flare arg 3 for flare '%d' in lensflares\n", num_flares);
                continue;
            }
            flares[num_flares].shader = R_FindShader(token, -1, qfalse, qfalse, qfalse, qfalse);

            token = COM_ParseExt(&file, qfalse);
            if (token[0]) {
                flares[num_flares].alphascale = atof(token);
            }

            num_flares++;
        } else if (!Q_stricmp(token, "fullscale")) {
            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: no arg for fullscale, assuming default (in lensflares)\n");
                continue;
            }
            fullscale = atof(token);
        } else if (!Q_stricmp(token, "fullscreen")) {
            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: no arg for fullsreen in lensflares\n");
                continue;
            }
            fullscreen = R_FindShader(token, -1, qfalse, qfalse, qfalse, qfalse);
        } else if (!Q_stricmp(token, "fullfade")) {
            token = COM_ParseExt(&file, qfalse);
            if (!token[0]) {
                ri.Printf(PRINT_WARNING, "WARNING: no arg for fullfade in lensflares\n");
                continue;
            }
            fullfade = atof(token);
        } else if (!Q_stricmp(token, "end")) {
            break;
        }
    }

    if (!num_flares) {
        ri.Printf(PRINT_WARNING, "WARNING: no lensflares defined!\n");
    }
    lasttime = backEnd.refdef.time - fullfade;
}

static void R_DrawSunFlare()
{
    if (!s_sun.exists) {
        return;
    }
    if (!sunFlare.initted) {
        if (!s_sun.szFlareName[0]) {
            return;
        }
        if (Q_stricmp(s_sun.szFlareName, "none")) {
            sunFlare.Init(s_sun.szFlareName);
        }
    }

    if (!sunFlare.initted) {
        s_sun.szFlareName[0] = 0;
        return;
    }

    if (!sunFlare.num_flares) {
        return;
    }

    VectorMA(backEnd.viewParms.ori.origin, 16384, s_sun.flaredirection, sunFlare.trace_v);
    VectorMA(backEnd.viewParms.ori.origin, 128, s_sun.flaredirection, sunFlare.v);

    sunFlare.SunTry();
}

static void R_DrawSunFlareBlend()
{
    if (!s_sun.exists) {
        return;
    }
    if (!sunFlare.initted) {
        return;
    }

    sunFlare.ScreenBlend();
}

void R_DrawLensFlares()
{
    int modelViewMark, projectionMark;

    /* retail RB_RenderFlares @0x0004A6E0 gates the pass on tr.world too */
    if (!tr.world) {
        return;
    }

    /* retail's whole-pass gate (RB_RenderFlares @0x0004A6E0), not an arm-local one */
    if (!r_flares->integer) {
        return;
    }

    /* the walk runs on the render thread: retail's RB_RenderFlares resets the
       orientation it is about to transform with, and that orientation is the
       carried frame's, never the front end's live copy */
    backEnd.ori = backEnd.viewParms.world;

    RB_ImmFlush();
    modelViewMark = RB_ModelViewDepth();
    projectionMark = RB_ProjectionDepth();
    RB_PushModelView();
    RB_LoadIdentityModelView();
    RB_PushProjection();
    RB_LoadIdentityProjection();
    RB_OrthoProjection(-1.0, 1.0, -1.0, 1.0, -99999.0, 99999.0);

    tess.no_global_fog = true;

    R_DrawSunFlare();
    R_AddDlightFlares();
    R_AddSpherelFlares();
    R_AddEntityFlares();

    R_DrawSunFlareBlend();

    R_DrawEmitterLensflares();

    tess.no_global_fog = qfalse;

    RB_ImmFlush();
    RB_PopProjectionTo( projectionMark );
    RB_PopModelViewTo( modelViewMark );
}

void R_InitLensFlare()
{
    sunFlare.initted = false;

    R_LoadLensflares();
    R_RegisterLensflareShaders();
    R_ClearLensflareStates();
}

bool sun_flare_class::SunCheckRay()
{
    mnode_t *pViewLeaf;
    trace_t  trace;

    pViewLeaf = R_PointInLeaf(backEnd.refdef.vieworg);

    if (pViewLeaf->area != -1) {
        ri.CM_BoxTrace(
            &trace, backEnd.viewParms.ori.origin, trace_v, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse
        );

        if (trace.surfaceFlags & SURF_SKY) {
            return true;
        }
    }

    return false;
}

void sun_flare_class::SunScreenCalc()
{
    vec4_t eye;
    vec4_t clip;
    vec4_t point;
    int    i, j;

    VectorCopy(v, point);
    point[3] = 1.0;

    VectorClear4(eye);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            eye[i] += point[j] * backEnd.ori.modelMatrix[i + j * 4];
        }
    }

    VectorClear4(clip);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            clip[i] += eye[j] * backEnd.viewParms.projectionMatrix[i + j * 4];
        }
    }

    for (i = 0; i < 2; i++) {
        screen[i] = clip[i] / clip[3];
    }
}

void sun_flare_class::SunTry()
{
    vec2_t   diff;
    vec2_t   point;
    float    fadeAlpha;
    int      i;
    qboolean bDrawingFade = qfalse;

    if (!CheckRange()) {
        return;
    }

    if (SunCheckRay()) {
        fadeAlpha = (dot - dot_min) / (1.0 - dot_min);
    } else {
        int timediff;

        if (!fullscreen) {
            return;
        }
        if (!blend_active) {
            return;
        }

        timediff = backEnd.refdef.time - lasttime;
        fadeAlpha    = (float)(fullfade - timediff) / fullfade * lastbright;

        if (fadeAlpha <= 0.0001) {
            return;
        }

        bDrawingFade = qtrue;
    }

    SunScreenCalc();
    VectorScale2D(screen, -2.0, diff);

    for (i = 0; i < num_flares; i++) {
        flare_s *flare = &flares[i];

        VectorMA2D(screen, flare->where, diff, point);

        RB_Color4f(color[0], color[1], color[2], fadeAlpha * this->alpha * flare->alphascale);
        RB_StreamBegin(flare->shader);
        RB_Texcoord2f(0.0, 0.0);

        RB_Vertex2f(point[0] - flare->size, point[1] + flare->size);
        RB_Texcoord2f(1.0, 0.0);

        RB_Vertex2f(point[0] + flare->size, point[1] + flare->size);
        RB_Texcoord2f(0.0, 1.0);

        RB_Vertex2f(point[0] - flare->size, point[1] - flare->size);
        RB_Texcoord2f(1.0, 1.0);

        RB_Vertex2f(point[0] + flare->size, point[1] - flare->size);
        RB_StreamEnd();
    }

    if (!bDrawingFade) {
        lasttime     = backEnd.refdef.time;
        blend_active = true;
        lastbright   = Square(fadeAlpha) * Square(fadeAlpha) * fullscale;
    }
}


#define MAX_LENSFLARE_FLARES 20

typedef struct lensflareFlare_s {
    float     size;        
    float     where;       
    shader_t *shader;      
    char      shadername[64];  
    float     alphascale;  
} lensflareFlare_t;

typedef struct lensflareInfo_s {
    char             name[32];                         
    int              num;                              
    lensflareFlare_t flares[MAX_LENSFLARE_FLARES];     
    float            coronaSize;                       
    shader_t        *coronaShader;                     
    char             coronaShaderName[64];             
    float            coronaAlpha;                      
    float            dot_min;                          
    int              numFlares;                        
    float            fadetime;                         
    float            fullscale;                        
    shader_t        *fullscreenShader;                 
    char             fullscreenName[64];               
    float            fullfade;                         
    qboolean         useMaxDist;                       

    struct lensflareInfo_s *next;                      
} lensflareInfo_t;

#define MAX_LENSFLARE_STATES 128

typedef struct lensflareState_s {
    struct lensflareState_s *next;
    qboolean                 inUse;

    void      *key;               
    int        frameSceneNum;     
    qboolean   inPortalSky;       
    int        viewFrameCount;    

    int        addTime;           
    int        fadeEndTime;       
    qboolean   visible;           
    float      alpha;             
    qboolean   clipped;           
    int        nextVisTime;       

    vec2_t     screen;            
    vec3_t     origin;            
    vec3_t     color;             
    float      scale;             
    shader_t  *replacementShader; 

    lensflareInfo_t *info;        
} lensflareState_t;

static lensflareInfo_t  *lf_infos;
static int               lf_numInfos;

static lensflareState_t  lf_states[MAX_LENSFLARE_STATES];
static lensflareState_t *lf_activeStates;
static lensflareState_t *lf_freeStates;

static lensflareInfo_t *R_LensflareInfoForName(const char *name)
{
    lensflareInfo_t *info;

    for (info = lf_infos; info; info = info->next) {
        if (!Q_stricmp(name, info->name)) {
            return info;
        }
    }

    return NULL;
}

static lensflareInfo_t *R_LensflareInfoForNum(int num)
{
    lensflareInfo_t *info;

    if (num < 0) {
        return NULL;
    }

    for (info = lf_infos; info; info = info->next) {
        if (info->num == num) {
            return info;
        }
    }

    return NULL;
}

int R_LensflareNameToNum(const char *name)
{
    lensflareInfo_t *info;

    if (!name || !name[0]) {
        return -1;
    }

    info = R_LensflareInfoForName(name);
    if (!info) {
        return -1;
    }

    return info->num;
}

static void R_ParseLensflareSection(const char **data, lensflareInfo_t *info)
{
    const char *token;

    info->dot_min          = 0.8f;
    info->numFlares        = 0;
    info->fullscale        = 0.7f;
    info->fullscreenShader = NULL;
    info->fullfade         = 0.0f;
    info->coronaShader     = NULL;
    info->fullscreenName[0] = 0;
    info->coronaShaderName[0] = 0;
    info->useMaxDist       = qtrue;

    while (1) {
        token = COM_ParseExt((char **)data, qtrue);
        if (!token || !token[0]) {
            ri.Printf(PRINT_WARNING, "No end statement found for %s\n", info->name);
            return;
        }

        if (!Q_stricmp(token, "dot_min")) {
            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                info->dot_min = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for dot_min in lensflare %s\n", info->name);
            }
            if (info->dot_min < 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for dot_min in lensflare %s\n", info->name);
                info->dot_min = 0.0f;
            }
        } else if (!Q_stricmp(token, "flare")) {
            lensflareFlare_t *flare;
            int               index;

            index = info->numFlares;
            if (index >= MAX_LENSFLARE_FLARES) {
                index = MAX_LENSFLARE_FLARES - 1;
            }
            flare = &info->flares[index];

            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                flare->size = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for flare size in lensflare %s\n", info->name);
            }
            if (flare->size <= 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for flare size in lensflare %s\n", info->name);
                flare->size = 0.1f;
            }

            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                flare->where = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for flare place in lensflare %s\n", info->name);
            }
            if (flare->where < 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for flare where in lensflare %s\n", info->name);
                flare->where = 0.0f;
            }

            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                Q_strncpyz(flare->shadername, token, sizeof(flare->shadername));
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for flare shader in lensflare %s\n", info->name);
                flare->shadername[0] = 0;
            }

            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                flare->alphascale = atof(token);
            } else {
                flare->alphascale = 1.0f;
            }

            if (info->numFlares < MAX_LENSFLARE_FLARES) {
                info->numFlares++;
            }
        } else if (!Q_stricmp(token, "corona")) {
            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                info->coronaSize = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for corona size in lensflare %s\n", info->name);
            }
            if (info->coronaSize <= 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for corona size in lensflare %s\n", info->name);
                info->coronaSize = 0.1f;
            }

            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                Q_strncpyz(info->coronaShaderName, token, sizeof(info->coronaShaderName));
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for corona shader in lensflare %s\n", info->name);
            }

            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                info->coronaAlpha = atof(token);
            } else {
                info->coronaAlpha = 1.0f;
            }
        } else if (!Q_stricmp(token, "fullscale")) {
            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                info->fullscale = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for fullscale in lensflare %s\n", info->name);
            }
            if (info->fullscale <= 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for fullscale in lensflare %s\n", info->name);
                info->fullscale = 0.7f;
            }
        } else if (!Q_stricmp(token, "noMaxDist")) {
            info->useMaxDist = qfalse;
        } else if (!Q_stricmp(token, "fullscreen")) {
            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                Q_strncpyz(info->fullscreenName, token, sizeof(info->fullscreenName));
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for fullscreen in lensflare %s\n", info->name);
            }
        } else if (!Q_stricmp(token, "fullfade")) {
            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                info->fullfade = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for fullfade in lensflare %s\n", info->name);
            }
            if (info->fullfade < 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for fullfade in lensflare %s\n", info->name);
                info->fullfade = 0.0f;
            }
        } else if (!Q_stricmp(token, "fadetime")) {
            token = COM_ParseExt((char **)data, qfalse);
            if (token && token[0]) {
                info->fadetime = atof(token);
            } else {
                ri.Printf(PRINT_WARNING, "No value specified for fadetime in lensflare %s\n", info->name);
            }
            if (info->fadetime < 0.0f) {
                ri.Printf(PRINT_WARNING, "Bad value specified for fadetime in lensflare %s\n", info->name);
                info->fadetime = 0.0f;
            }
        } else if (!Q_stricmp(token, "end")) {
            return;
        }
    }
}

void R_LoadLensflares(void)
{
    int i;

    ri.Printf(PRINT_DEVELOPER, "Loading lensflares\n");

    for (i = 1; i < 10; i++) {
        char        filename[MAX_QPATH];
        char       *buffer;
        const char *data;
        const char *token;

        Com_sprintf(filename, sizeof(filename), "global/lensflare%d.txt", i);

        if (ri.FS_ReadFileEx(filename, (void **)&buffer, qtrue) <= 0 || !buffer) {
            continue;
        }

        data = buffer;

        while (1) {
            lensflareInfo_t *info;
            qboolean         isNew;

            token = COM_ParseExt((char **)&data, qtrue);
            if (!token || !token[0]) {
                break;
            }

            if (Q_stricmp(token, "begin")) {
                continue;
            }

            token = COM_ParseExt((char **)&data, qfalse);
            if (!token || !token[0]) {
                break;
            }

            info  = R_LensflareInfoForName(token);
            isNew = qfalse;
            if (!info) {
                info = (lensflareInfo_t *)ri.Malloc(sizeof(*info));
                Com_Memset(info, 0, sizeof(*info));
                info->num = lf_numInfos++;
                isNew     = qtrue;
            }
            Q_strncpyz(info->name, token, sizeof(info->name));

            R_ParseLensflareSection(&data, info);

            if (isNew) {
                info->next = lf_infos;
                lf_infos   = info;
            }
        }

        ri.FS_FreeFile(buffer);
    }
}

static shader_t *R_RegisterLensflareShader(const char *name, const char *defname)
{
    shader_t *sh;

    if (!name[0]) {
        return NULL;
    }

    sh = R_FindShader(name, -1, qfalse, qfalse, qfalse, qfalse);
    if (sh == tr.defaultShader) {
        ri.Printf(PRINT_WARNING, "Bad shader %s in lensflare %s\n", name, defname);
    }

    return sh;
}

void R_RegisterLensflareShaders(void)
{
    lensflareInfo_t *info;
    int              i;

    ri.Printf(PRINT_DEVELOPER, "Registering lensflare shaders\n");

    for (info = lf_infos; info; info = info->next) {
        info->fullscreenShader = R_RegisterLensflareShader(info->fullscreenName, info->name);
        info->coronaShader     = R_RegisterLensflareShader(info->coronaShaderName, info->name);

        for (i = 0; i < info->numFlares; i++) {
            info->flares[i].shader = R_RegisterLensflareShader(info->flares[i].shadername, info->name);
        }
    }
}

void R_ShutdownLensflares(void)
{
    lensflareInfo_t *info, *next;

    for (info = lf_infos; info; info = next) {
        next = info->next;
        ri.Free(info);
    }

    lf_infos = NULL;
    /* the next load re-keys from zero, so a stale lensflareNum cached in cgame
       cannot survive into a table that numbers its entries from here */
    lf_numInfos = 0;
}

void R_ClearLensflareStates(void)
{
    int i;

    Com_Memset(lf_states, 0, sizeof(lf_states));

    lf_activeStates = NULL;
    lf_freeStates   = NULL;

    for (i = 0; i < MAX_LENSFLARE_STATES; i++) {
        lf_states[i].next = lf_freeStates;
        lf_freeStates     = &lf_states[i];
    }
}

static lensflareState_t *R_LensflareStateFor(void *key, qboolean clipped)
{
    lensflareState_t *f;
    int               sceneNum    = backEnd.viewParms.frameSceneNum;
    qboolean          inPortalSky = backEnd.viewParms.isPortalSky;

    for (f = lf_activeStates; f; f = f->next) {
        if (f->key == key && f->frameSceneNum == sceneNum && f->inPortalSky == inPortalSky) {
            return f;
        }
    }

    if (clipped) {
        return NULL;
    }

    f = lf_freeStates;
    if (!f) {
        return NULL;
    }

    lf_freeStates   = f->next;
    f->next         = lf_activeStates;
    lf_activeStates = f;

    f->inUse          = qtrue;
    f->key            = key;
    f->frameSceneNum  = sceneNum;
    f->inPortalSky    = inPortalSky;
    f->viewFrameCount = -1;
    f->addTime        = backEnd.refdef.time;
    f->fadeEndTime    = 0;
    f->visible        = qfalse;
    f->alpha          = 0.0f;
    f->nextVisTime    = 0;

    return f;
}

static qboolean R_LensflareScreenCalc(const vec3_t org, vec2_t screen)
{
    vec4_t   eye;
    vec4_t   clip;
    vec4_t   point;
    qboolean inside = qtrue;
    int      i, j;

    VectorCopy(org, point);
    point[3] = 1.0;

    VectorClear4(eye);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            eye[i] += point[j] * backEnd.ori.modelMatrix[i + j * 4];
        }
    }

    VectorClear4(clip);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            clip[i] += eye[j] * backEnd.viewParms.projectionMatrix[i + j * 4];
        }
    }

    for (i = 0; i < 3; i++) {
        if (clip[i] >= clip[3] || clip[i] <= -clip[3]) {
            inside = qfalse;
        }
    }

    if (clip[3] == 0.0f) {
        return qfalse;
    }

    for (i = 0; i < 2; i++) {
        screen[i] = clip[i] / clip[3];
    }

    return inside;
}

static void R_LensflareTestVisibility(lensflareState_t *f)
{
    lensflareInfo_t *info = f->info;
    qboolean         visible;
    float            maxAlpha;
    float            fadeRate;
    float            a;
    vec3_t           diff;
    float            d;

    if (f->viewFrameCount != backEnd.viewParms.frameCount || f->clipped) {
        visible = qfalse;
    } else if (r_flares->integer <= 1) {
        /* retail's Simple tier runs no occlusion test; Precise is the trace below */
        visible = qtrue;
    } else if (backEnd.refdef.time < f->nextVisTime) {
        visible = f->visible;
    } else {
        trace_t trace;
        float   need;

        ri.CM_BoxTrace(
            &trace, backEnd.viewParms.ori.origin, f->origin, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse
        );

        VectorSubtract(f->origin, backEnd.viewParms.ori.origin, diff);
        d = VectorLength(diff);

        if (d >= 24.0f) {
            need = (d - 24.0f) / d;
        } else {
            need = 1.0f;
        }

        f->nextVisTime = backEnd.refdef.time + 50;
        visible        = (qboolean)(trace.fraction > need);
    }

    maxAlpha = 1.0f;
    if (info->useMaxDist && r_flaresMaxDist->value > 0.0f) {
        float start = 0.8f * r_flaresMaxDist->value;

        VectorSubtract(backEnd.viewParms.ori.origin, f->origin, diff);
        d = VectorLength(diff);

        if (d > start) {
            maxAlpha = 1.0f - (d - start) / (r_flaresMaxDist->value - start);
        }
    }

    fadeRate = info->fadetime * 1000.0f - 10.0f;

    if (visible) {
        if (!f->visible) {
            f->visible     = qtrue;
            f->fadeEndTime = (int)(fadeRate * (1.0f - f->alpha)) + backEnd.refdef.time;
        }

        if (info->fadetime == 0.0f || f->fadeEndTime <= backEnd.refdef.time) {
            a = maxAlpha;
        } else {
            a = maxAlpha - ((f->fadeEndTime - backEnd.refdef.time) / 1000.0f) / info->fadetime;
        }
    } else {
        if (f->visible) {
            f->visible     = qfalse;
            f->fadeEndTime = (int)(fadeRate * f->alpha) + backEnd.refdef.time;
        }

        if (info->fadetime == 0.0f || f->fadeEndTime <= backEnd.refdef.time) {
            a = 0.0f;
        } else {
            a = ((f->fadeEndTime - backEnd.refdef.time) / 1000.0f) / info->fadetime;
        }
    }

    if (a < 0.0f) {
        a = 0.0f;
    }
    if (a > maxAlpha) {
        a = maxAlpha;
    }

    f->alpha = a;
}

static shader_t *R_LensflareShaderFor(const lensflareState_t *f, shader_t *defaultTo)
{
    if (!f->replacementShader || f->replacementShader == tr.defaultShader) {
        return defaultTo;
    }

    return f->replacementShader;
}

static void R_LensflareDrawCorona(const lensflareState_t *f)
{
    lensflareInfo_t *info = f->info;
    shader_t        *sh;
    vec3_t           dir;
    float            size;

    VectorSubtract(f->origin, backEnd.viewParms.ori.origin, dir);
    VectorNormalizeFast(dir);

    if (DotProduct(backEnd.viewParms.ori.axis[0], dir) <= 0.0f) {
        return;
    }

    size = info->coronaSize * f->scale;

    sh = R_LensflareShaderFor(f, info->coronaShader);
    if (!sh) {
        return;
    }

    RB_Color4f(
        f->color[0] * f->alpha, f->color[1] * f->alpha, f->color[2] * f->alpha, info->coronaAlpha * f->alpha
    );

    RB_StreamBegin(sh);
    RB_Texcoord2f(0.0, 0.0);
    RB_Vertex2f(f->screen[0] - size, f->screen[1] + size);
    RB_Texcoord2f(1.0, 0.0);
    RB_Vertex2f(f->screen[0] + size, f->screen[1] + size);
    RB_Texcoord2f(0.0, 1.0);
    RB_Vertex2f(f->screen[0] - size, f->screen[1] - size);
    RB_Texcoord2f(1.0, 1.0);
    RB_Vertex2f(f->screen[0] + size, f->screen[1] - size);
    RB_StreamEnd();
}

static void R_LensflareDrawScreenBlend(const lensflareState_t *f)
{
    lensflareInfo_t *info = f->info;
    float            elapsed;
    float            duration;
    float            a;

    if (!info->fullscreenShader) {
        return;
    }

    elapsed  = (float)(backEnd.refdef.time - f->addTime);
    duration = 1000.0f * info->fullfade;

    if (elapsed > duration) {
        return;
    }

    a = ((duration - elapsed) / duration) * (f->alpha * info->fullscale);
    if (a <= 0.0001f) {
        return;
    }

    RB_Color4f(f->color[0], f->color[1], f->color[2], a);

    RB_StreamBegin(info->fullscreenShader);
    RB_Vertex2f(-1.0, -1.0);
    RB_Vertex2f(1.0, -1.0);
    RB_Vertex2f(-1.0, 1.0);
    RB_Vertex2f(1.0, 1.0);
    RB_StreamEnd();
}

static void R_LensflareDraw(lensflareState_t *f)
{
    lensflareInfo_t *info = f->info;
    vec3_t           dir;
    vec2_t           diff;
    float            dot;
    float            brightness;
    int              i;

    if (info->coronaShader) {
        R_LensflareDrawCorona(f);
    }

    if (info->fullscreenShader) {
        R_LensflareDrawScreenBlend(f);
    }

    VectorSubtract(f->origin, backEnd.viewParms.ori.origin, dir);
    VectorNormalizeFast(dir);

    dot = DotProduct(backEnd.viewParms.ori.axis[0], dir);
    if (dot < info->dot_min) {
        return;
    }

    f->addTime = backEnd.refdef.time;

    brightness = (dot - info->dot_min) / (1.0f - info->dot_min);
    VectorScale2D(f->screen, -2.0f, diff);

    for (i = 0; i < info->numFlares; i++) {
        lensflareFlare_t *flare = &info->flares[i];
        shader_t         *sh;
        vec2_t            point;
        float             half;

        sh = R_LensflareShaderFor(f, flare->shader);
        if (!sh) {
            continue;
        }

        VectorMA2D(f->screen, flare->where, diff, point);
        half = flare->size * f->scale;

        RB_Color4f(
            f->color[0] * f->alpha,
            f->color[1] * f->alpha,
            f->color[2] * f->alpha,
            f->alpha * brightness * flare->alphascale
        );

        RB_StreamBegin(sh);
        RB_Texcoord2f(0.0, 0.0);
        RB_Vertex2f(point[0] - half, point[1] + half);
        RB_Texcoord2f(1.0, 0.0);
        RB_Vertex2f(point[0] + half, point[1] + half);
        RB_Texcoord2f(0.0, 1.0);
        RB_Vertex2f(point[0] - half, point[1] - half);
        RB_Texcoord2f(1.0, 1.0);
        RB_Vertex2f(point[0] + half, point[1] - half);
        RB_StreamEnd();
    }
}

/* One flare record into the shared Lensflare state; every arm below feeds the
   same records retail's RB_AddFlare @0x00049740 does. */
static void R_LensflareFeed(const lensflare_t *lf)
{
    lensflareInfo_t  *info;
    lensflareState_t *f;
    vec2_t            screen;
    qboolean          inside;

    info = R_LensflareInfoForNum(lf->num);
    if (!info) {
        return;
    }

    inside = R_LensflareScreenCalc(lf->origin, screen);

    f = R_LensflareStateFor(lf->key, (qboolean)!inside);
    if (!f) {
        return;
    }

    if (f->viewFrameCount != backEnd.viewParms.frameCount - 1
        && f->viewFrameCount != backEnd.viewParms.frameCount) {
        f->visible = qfalse;
    }
    f->viewFrameCount = backEnd.viewParms.frameCount;

    f->info              = info;
    f->clipped           = (qboolean)!inside;
    f->scale             = lf->scale;
    f->replacementShader = lf->replacementShader;
    VectorCopy(lf->origin, f->origin);
    VectorCopy(lf->color, f->color);
    f->screen[0] = screen[0];
    f->screen[1] = screen[1];
}

static void R_LensflarePush(lensflare_t *out, void *key, const vec3_t org, const vec3_t color, int num)
{
    out->key              = key;
    VectorCopy(org, out->origin);
    VectorCopy(color, out->color);
    out->num              = num;
    out->scale            = 1.0f;
    out->replacementShader = NULL;
    R_LensflareFeed(out);
}

/* RB_AddEntityFlares @0x00049B00: bit 0x100 names "entity", 0x8 "viewentity" */
static void R_AddEntityFlares(void)
{
    lensflare_t lf;
    vec3_t      rgb;
    int         entityNum = R_LensflareNameToNum("entity");
    int         viewNum   = R_LensflareNameToNum("viewentity");
    int         i;

    for (i = 0; i < backEnd.refdef.num_entities; i++) {
        const refEntity_t *re = &backEnd.refdef.entities[i].e;

        if ((backEnd.viewParms.isPortalSky && !(re->renderfx & RF_SKYENTITY))
            || (!backEnd.viewParms.isPortalSky && (re->renderfx & RF_SKYENTITY))) {
            continue;
        }

        if (re->renderfx & RF_LENSFLARE) {
            if (entityNum < 0) {
                continue;
            }
            rgb[0] = re->shaderRGBA[0] / 255.0;
            rgb[1] = re->shaderRGBA[1] / 255.0;
            rgb[2] = re->shaderRGBA[2] / 255.0;
            R_LensflarePush(&lf, (void *)re, re->origin, rgb, entityNum);
        } else if (re->renderfx & RF_VIEWLENSFLARE) {
            if (viewNum < 0) {
                continue;
            }
            rgb[0] = re->shaderRGBA[0] / 255.0;
            rgb[1] = re->shaderRGBA[1] / 255.0;
            rgb[2] = re->shaderRGBA[2] / 255.0;
            R_LensflarePush(&lf, (void *)re, re->origin, rgb, viewNum);
        }
    }
}

/* RB_RenderFlares @0x0004A6E0: type 1 names "dlight", type 2 "viewdlight", and
   no other type draws a flare */
static void R_AddDlightFlares(void)
{
    lensflare_t lf;
    int         dlightNum = R_LensflareNameToNum("dlight");
    int         viewNum   = R_LensflareNameToNum("viewdlight");
    int         i;

    for (i = 0; i < backEnd.refdef.num_dlights; i++) {
        dlight_t *dl = &backEnd.refdef.dlights[i];

        if (dl->type == dlighttype_t::lensflare) {
            if (dlightNum >= 0) {
                R_LensflarePush(&lf, dl, dl->origin, dl->color, dlightNum);
            }
        } else if (dl->type == dlighttype_t::viewlensflare) {
            if (viewNum >= 0) {
                R_LensflarePush(&lf, dl, dl->origin, dl->color, viewNum);
            }
        }
    }
}

/* RB_AddLightFlares @0x00049DF0: the map's own entity lights carry the flare
   bit, "corona" for every type but the sun; the per-record mark keeps a light
   listed under several nodes to one flare per view */
/* retail RB_AddLightFlares @0x00049DF0 bumps its own counter once per call, so the
   portal-sky view and the main view each get a pass over the same record */
static int s_spherelFlareSerial;

static void R_SpherelFlarePush(lensflare_t *out, spherel_t *s, int coronaNum, int sunNum)
{
    int num;

    if (!s->lensflare || s->flare_frame == s_spherelFlareSerial) {
        return;
    }
    s->flare_frame = s_spherelFlareSerial;

    num = (s->type == SPHEREL_SUN) ? sunNum : coronaNum;
    if (num >= 0) {
        R_LensflarePush(out, s, s->origin, s->color, num);
    }
}

static void R_AddSpherelFlares(void)
{
    lensflare_t lf;
    int         coronaNum = R_LensflareNameToNum("corona");
    int         sunNum    = R_LensflareNameToNum("sun");
    int         i, j;

    if (!tr.world) {
        return;
    }

    s_spherelFlareSerial++;

    if (tr.sphereLightVis) {
        /* retail's arm walks every node the view marked and flares that node's
           own list, so a light rides the nodes ENTLIGHTSVIS gave it; the -1 arm
           is the sky pass, whose marks are one serial older by the main view */
        for (i = 0; i < tr.world->numnodes; i++) {
            mnode_t *node = &tr.world->nodes[i];

            if (!node->numlights) {
                continue;
            }
            if (node->visframe != tr.visCount && !(tr.skyRendered && node->visframe == tr.visCount - 1)) {
                continue;
            }
            for (j = 0; j < node->numlights; j++) {
                R_SpherelFlarePush(&lf, node->lights[j], coronaNum, sunNum);
            }
        }
        return;
    }

    for (i = 0; i < tr.numSLights; i++) {
        R_SpherelFlarePush(&lf, &tr.sLights[i], coronaNum, sunNum);
    }
}

static void R_DrawEmitterLensflares(void)
{
    lensflareState_t **prev;
    lensflareState_t  *f;
    qboolean           draw;
    int                i;

    for (i = 0; i < backEnd.refdef.num_lensflares; i++) {
        const lensflare_t *lf = &backEnd.refdef.lensflares[i];
        lensflareInfo_t   *info;
        vec2_t             screen;
        qboolean           inside;

        info = R_LensflareInfoForNum(lf->num);
        if (!info) {
            continue;
        }

        inside = R_LensflareScreenCalc(lf->origin, screen);

        f = R_LensflareStateFor(lf->key, (qboolean)!inside);
        if (!f) {
            continue;
        }

        if (f->viewFrameCount != backEnd.viewParms.frameCount - 1
            && f->viewFrameCount != backEnd.viewParms.frameCount) {
            f->visible = qfalse;
        }
        f->viewFrameCount = backEnd.viewParms.frameCount;

        f->info              = info;
        f->clipped           = (qboolean)!inside;
        f->scale             = lf->scale;
        f->replacementShader = lf->replacementShader;
        VectorCopy(lf->origin, f->origin);
        VectorCopy(lf->color, f->color);
        f->screen[0] = screen[0];
        f->screen[1] = screen[1];
    }

    draw = qfalse;
    prev = &lf_activeStates;
    while ((f = *prev) != NULL) {
        if (f->viewFrameCount < backEnd.viewParms.frameCount - 1 && f->alpha == 0.0f) {
            *prev         = f->next;
            f->inUse      = qfalse;
            f->next       = lf_freeStates;
            lf_freeStates = f;
            continue;
        }

        if (f->info && f->frameSceneNum == backEnd.viewParms.frameSceneNum
            && f->inPortalSky == backEnd.viewParms.isPortalSky) {
            R_LensflareTestVisibility(f);

            if (f->alpha > 0.0f) {
                draw = qtrue;
            }
        }

        prev = &f->next;
    }

    if (!draw) {
        return;
    }

    for (f = lf_activeStates; f; f = f->next) {
        if (f->info && f->frameSceneNum == backEnd.viewParms.frameSceneNum
            && f->inPortalSky == backEnd.viewParms.isPortalSky && f->alpha > 0.0f) {
            R_LensflareDraw(f);
        }
    }
}
