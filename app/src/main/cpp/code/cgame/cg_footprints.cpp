

#include "cg_local.h"
#include "cg_footprints.h"
#include "cg_commands.h"

static Container<FootprintMap_s *> footprintMaps;

#define FP_KEYWORD_CLOUD          "Cloud"
#define FP_KEYWORD_LEFT           "FootprintLeft"
#define FP_KEYWORD_RIGHT          "FootprintRight"
#define FP_KEYWORD_BONENAME       "BoneName"
#define FP_KEYWORD_CLOSE_BRACE    "}"
#define FP_KEYWORD_OPEN_BRACE     "{"
#define FP_KEYWORD_FOOTPRINTS     "Footprints"
#define FP_FOOTPRINTS_FILE        "global/footprints.txt"

FootprintBoneShaderPair_s::FootprintBoneShaderPair_s()
    : radius(0.0f)
{
}

FootprintBoneShaderPair_s::~FootprintBoneShaderPair_s() {}

FootprintFX_s::FootprintFX_s() {}

FootprintFX_s::~FootprintFX_s()
{
    boneShaderPairs.ClearObjectList();
}

FootprintMap_s::~FootprintMap_s() {}

void ParseFootprintFXData(FootprintFX_s *fx, Script& script)
{
    str token = script.GetToken(qtrue);

    while (Q_stricmp(token.c_str(), FP_KEYWORD_CLOSE_BRACE)) {
        token = script.GetToken(qtrue);

        if (!Q_stricmp(token.c_str(), FP_KEYWORD_CLOUD)) {
            fx->cloud = script.GetToken(qfalse);
        }

        if (!Q_stricmp(token.c_str(), FP_KEYWORD_LEFT)) {
            fx->footprintLeft = script.GetToken(qfalse);
        }

        if (!Q_stricmp(token.c_str(), FP_KEYWORD_RIGHT)) {
            fx->footprintRight = script.GetToken(qfalse);
        }

        if (!Q_stricmp(token.c_str(), FP_KEYWORD_BONENAME)) {
            FootprintBoneShaderPair_s pair;

            pair.boneName = script.GetToken(qfalse);
            pair.shader   = script.GetToken(qfalse);
            pair.radius   = atof(script.GetToken(qfalse));

            fx->boneShaderPairs.AddObject(pair);
        }
    }
}

void ParseFootprintSurfaceData(FootprintMap_s *map, Script& script)
{
    str token = script.GetToken(qtrue);

    while (Q_stricmp(token.c_str(), FP_KEYWORD_CLOSE_BRACE)) {
        int surfaceType;
        int row;

        token = script.GetToken(qtrue);

        if (!Q_stricmp(token.c_str(), FP_KEYWORD_OPEN_BRACE)) {
            script.GetToken(qtrue);
        }

        surfaceType = cgi.SurfaceNameToType(token.c_str());

        switch (surfaceType) {
        case 0x200:     row = FOOTPRINT_SAND;         break;
        case 0x1000:    row = FOOTPRINT_SNOW;         break;
        case 0x10000:   row = FOOTPRINT_METAL_DUCT;   break;
        case 0x20000:   row = FOOTPRINT_METAL;        break;
        case 0x40000:   row = FOOTPRINT_ROCK;         break;
        case 0x80000:   row = FOOTPRINT_DIRT;         break;
        case 0x100000:  row = FOOTPRINT_GRILL;        break;
        case 0x200000:  row = FOOTPRINT_ORGANIC;      break;
        case 0x400000:  row = FOOTPRINT_SQUISHY;      break;
        case 0x2000000: row = FOOTPRINT_DEFAULT;      break;
        case 0x4000000: row = FOOTPRINT_METAL_HOLLOW; break;
        case 0x8000000: row = FOOTPRINT_CARPET;       break;
        default:        row = -1;                     break;
        }

        if (row >= 0) {
            ParseFootprintFXData(&map->fx[row], script);
        }
    }
}

FootprintMap_s *CreateNewFootprintMap(const str& name, Script& script)
{
    FootprintMap_s *map = new FootprintMap_s;

    map->name = name;
    footprintMaps.AddObject(map);

    return map;
}

extern "C" void CG_InitFootprintMap(void)
{
    Script script;

    script.LoadFile(FP_FOOTPRINTS_FILE);

    while (script.TokenAvailable(qtrue)) {
        const char *token = script.GetToken(qfalse);

        if (Q_stricmp(token, FP_KEYWORD_FOOTPRINTS)) {
            continue;
        }

        str name(script.GetToken(qfalse));

        ParseFootprintSurfaceData(CreateNewFootprintMap(name, script), script);
    }

    script.Close();
}

extern "C" void CG_ClearFootprintMap(void)
{
    int i;

    for (i = footprintMaps.NumObjects(); i > 0; i--) {
        FootprintMap_s *map = footprintMaps.ObjectAt(i);

        footprintMaps.RemoveObjectAt(i);

        if (map) {
            delete map;
        }
    }
}


surfaceinfo_t *CG_GetSurfaceInfo(centity_t *cent, const char *tagname)
{
    static surfaceinfo_t info;
    orientation_t        ori;
    Vector               forward, right, up;
    vec3_t               start, end;

    memset(&info, 0, sizeof(info));

    VectorSet(start, 0.0f, 0.0f, 0.0f);
    VectorCopy(start, end);

    if (tagname && tagname[0]) {
        Vector tagOrigin;

        commandManager.GetOrientation(str(tagname), &ori);
        AngleVectors(cent->currentState.angles, forward, right, up);

        start[0] = cent->currentState.origin[0];
        start[1] = cent->currentState.origin[1];
        start[2] = cent->currentState.origin[2];

        VectorCopy(ori.origin, tagOrigin);

        end[0] = start[0] + tagOrigin[0] * forward[0] + tagOrigin[1] * right[0] + tagOrigin[2] * up[0];
        end[1] = start[1] + tagOrigin[0] * forward[1] + tagOrigin[1] * right[1] + tagOrigin[2] * up[1];
        end[2] = start[2] + tagOrigin[0] * forward[2] + tagOrigin[1] * right[2] + tagOrigin[2] * up[2];
    } else {
        VectorCopy(cent->currentState.origin, start);
        VectorCopy(start, end);
    }

    end[2] -= 32.0f;

    if (cent->currentState.eType == 1) {
        CG_Trace(&info.trace, start, NULL, NULL, end, cent->currentState.number, 33628161, qtrue, qtrue, "playerfootstep");
    } else {
        CG_Trace(&info.trace, start, NULL, NULL, end, cent->currentState.number, 1, qtrue, qtrue, "monsterfootstep");
    }

    info.hit         = (info.trace.fraction != 1.0f) ? qtrue : qfalse;
    info.contents    = CG_PointContents(info.trace.endpos, -1);
    info.surfaceFlags = info.trace.surfaceFlags & 0xC7F1200;
    VectorCopy(info.trace.endpos, info.endpos);

    return &info;
}


#define WATER_CONTENTS_MASK 0x38
#define WATER_RAISE_HEIGHT  16.0f

#define WATERFX_MIN_DISTANCE (-1.0f)
#define WATERFX_PITCH        1.0f

void CG_PlayWaterFX(const trace_t *trace, float volume)
{
    vec3_t pos;

    pos[0] = trace->endpos[0];
    pos[1] = trace->endpos[1];
    pos[2] = trace->endpos[2] + WATER_RAISE_HEIGHT;

    if (CG_PointContents(pos, -1) & WATER_CONTENTS_MASK) {
        commandManager.PlaySound("snd_footstep_wade", NULL, CHAN_AUTO, volume, WATERFX_MIN_DISTANCE, WATERFX_PITCH);

        if (cg_debugFootsteps->integer) {
            cgi.DPrintf("Footstep: wade    volume: %.2f\n", volume);
        }
    } else {
        commandManager.PlaySound("snd_footstep_splash", NULL, CHAN_AUTO, volume, WATERFX_MIN_DISTANCE, WATERFX_PITCH);

        if (cg_debugFootsteps->integer) {
            cgi.DPrintf("Footstep: splash  volume: %.2f\n", volume);
        }
    }
}


void CG_Footstep(centity_t *cent, const char *tagname, const char *creaturetype)
{
    surfaceinfo_t info;

    if (cgs.gametype != 0 && (cgs.mp_flags & MP_FLAG_NO_FOOTSTEPS)) {
        return;
    }

    if (cg.snap && cg.snap->ps.clientNum == cent->currentState.number
        && (cg.snap->ps.pm_type == PM_SPECTATOR || cg.snap->ps.pm_type == PM_SPECTATOR_FOLLOW)) {
        return;
    }

    memcpy(&info, CG_GetSurfaceInfo(cent, tagname), sizeof(info));

    if (!info.hit) {
        return;
    }

    if (info.contents & WATER_CONTENTS_MASK) {
        CG_PlayWaterFX(&info.trace, 1.0f);
        return;
    }

    CG_PlaySurfaceFX(&info.trace, 0, info.surfaceFlags, tagname, creaturetype);
}

void CG_PlaySurfaceSound(centity_t *cent, const char *prefix, int channel, float volume, float minDistance)
{
    str              soundname;
    surfaceinfo_t   *info;
    const char      *material;

    if (prefix && !stricmp(prefix, "snd_footstep")) {
        if (cgs.gametype != 0 && (cgs.mp_flags & MP_FLAG_NO_FOOTSTEPS)) {
            return;
        }

        if (cg.snap && cg.snap->ps.clientNum == cent->currentState.number
            && (cg.snap->ps.pm_type == PM_SPECTATOR
                || cg.snap->ps.pm_type == PM_SPECTATOR_FOLLOW)) {
            return;
        }
    }

    info = CG_GetSurfaceInfo(cent, NULL);

    if (info->contents & WATER_CONTENTS_MASK) {
        if (prefix && !stricmp(prefix, "snd_footstep")) {
            CG_PlayWaterFX(&info->trace, volume);
            return;
        }

        material = "water";
    } else {
        material = cgi.SurfaceTypeToName(info->surfaceFlags);
    }

    soundname = prefix;
    soundname += "_";
    soundname += material ? material : "default";

    if (soundname.length()) {
        commandManager.PlaySound(soundname, cent->rawOrigin, channel, volume, minDistance, 1.0f);
    }
}

//   1. walk footprintMaps BACKWARDS for `creaturetype'.  Retail does not reset
// `r23` is `r1 + 0xDB0`, and there is NOT ONE store into `r1+0xDA0..0xDD0`

extern refEntity_t *current_entity;
extern int current_tiki;

void CG_PlaceCloudFX(const char *cloudName, const trace_t *trace)
{
    spawnthing_t st;
    refEntity_t  ent;
    str          cloud;

    // inheritance, which is not what retail does.
    memset(&ent, 0, sizeof(ent));
    ent.shaderRGBA[0] = 255;
    ent.shaderRGBA[1] = 255;
    ent.shaderRGBA[2] = 255;
    ent.shaderRGBA[3] = 255;

    refEntity_t *saved_entity = current_entity;
    int          saved_tiki   = current_tiki;
    current_entity            = &ent;

    commandManager.InitializeSpawnthing(&st);

    cloud = cloudName;
    st.m_modellist.ClearObjectList();
    st.m_modellist.AddObject(cloud);

    st.cgd.origin = Vector(trace->endpos) + Vector(0.0f, 0.0f, 5.0f);
    st.cgd.life   = 2000;
    st.cgd.flags2 |= T2_TEMPORARY_DECAL | T2_PARALLEL;
    VectorClear(st.cgd.angles);
    VectorClear(st.cgd.velocity);

    commandManager.SpawnTempModel(1, &st);

    current_entity = saved_entity;
    current_tiki   = saved_tiki;
}

void CG_PlaySurfaceFX(const trace_t  *trace,
                      int              unused,
                      unsigned int     surfaceFlags,
                      const char      *tagname,
                      const char      *creaturetype)
{
    FootprintMap_s *map = NULL;
    FootprintFX_s  *fx  = NULL;
    str             soundname;
    str             cloudShader;
    str             markShader;
    str             shader;
    float           radius  = 0.0f;
    float           colour  = 0.0f;
    int             i;
    int             k;

    for (i = footprintMaps.NumObjects(); i > 0; i--) {
        map = footprintMaps.ObjectAt(i);

        if (!Q_stricmp(map->name, creaturetype)) {
            break;
        }
    }

    if (!map) {
        return;
    }

    fx = NULL;

    if (surfaceFlags == 0x80000) {
        fx = &map->fx[0];
    } else if (surfaceFlags == 0x40000) {
        fx = &map->fx[1];
    } else if (surfaceFlags == 0x20000) {
        fx = &map->fx[2];
    } else if (surfaceFlags == 0x100000) {
        fx = &map->fx[3];
    } else if (surfaceFlags == 0x200000) {
        fx = &map->fx[4];
    } else if (surfaceFlags == 0x400000) {
        fx = &map->fx[5];
    } else if (surfaceFlags == 0x200) {
        fx = &map->fx[6];
    } else if (surfaceFlags == 0x1000) {
        fx = &map->fx[7];
    } else if (surfaceFlags == 0x10000) {
        fx = &map->fx[8];
    } else if (surfaceFlags == 0x4000000) {
        fx = &map->fx[9];
    } else if (surfaceFlags == 0x8000000) {
        fx = &map->fx[10];
    } else if (surfaceFlags == 0x2000000) {
        fx = &map->fx[11];
    }

    if (!fx) {
        return;
    }

    soundname = "snd_footstep_";

    const char *material = cgi.SurfaceTypeToName(surfaceFlags);

    if (material) {
        soundname += material;
    } else {
        soundname += "default";
    }

    commandManager.PlaySound(soundname, NULL);

    cloudShader = fx->cloud;

    for (k = fx->boneShaderPairs.NumObjects(); k > 0; k--) {
        FootprintBoneShaderPair_s *pair = &fx->boneShaderPairs.ObjectAt(k);

        if (!stricmp(pair->boneName.c_str(), tagname)) {
            shader = pair->shader;
            radius = pair->radius;
        }
    }

    if (cloudShader.length()) {
        CG_PlaceCloudFX(cloudShader.c_str(), trace);
    }

    if (!shader.length()) {
        return;
    }

    qhandle_t handle = cgi.R_RegisterShader(shader.c_str());

    // `r23` is `r1 + 0xDB0`, and NOT ONE store in the body writes into
    float roll = 0.0f;

    CG_ImpactMark(handle,
                  trace->endpos,
                  trace->plane.normal,
                  roll,
                  colour,
                  colour,
                  colour,
                  colour,
                  1,             
                  radius,        
                  qfalse,        
                  -1,            
                  qfalse);       
}
