
// Recovered from the Mac PPC build, which is not stripped.  Addresses are
// by locating the cgame TOC base at 0x00008000 -- established, not guessed, by
//       // NOTE: `di` is dead.  It is passed by CG_Target and never read --
//   field) and NOT infoIcon (retail's MULTIPLAYER info icon: SDK player.cpp:8544 writes

#include "cg_local.h"
#include "cg_radar.h"

#define RADAR_WORLD_RANGE_SQ 1440000.0f             
#define RADAR_NORMALIZE      (1.0f / 1200.0f)       


static qboolean CG_RadarShowInRadar(const char *itype)
{
    if (!itype[0] || !cgi.HasGameplayProperty || !cgi.GetGameplayFloatProperty) {
        return qfalse;
    }

    if (!cgi.HasGameplayProperty(itype, "ShowInRadar")) {
        return qfalse;
    }

    return cgi.GetGameplayFloatProperty(itype, "ShowInRadar") != 0.0f ? qtrue : qfalse;
}

static qboolean CG_RadarShowInSimpleMode(const char *itype)
{
    if (!itype[0] || !cgi.GetGameplayFloatProperty) {
        return qfalse;
    }
    return cgi.GetGameplayFloatProperty(itype, "ShowInSimpleMode") != 0.0f ? qtrue : qfalse;
}


void CG_UpdateRadarMode(void)
{
    if (!cg.snap) {
        return;
    }


    cgi.Cvar_Set("ui_radarmode", (cg.snap->ps.pm_flags & PMF_RADAR_MODE) ? "1" : "0");
}

static const char *CG_GetStringResource(const char *desc)
{
    static char buf[256];

    if (!desc || !strlen(desc)) {
        return desc;
    }

    if (cgi.isStringResource && cgi.isStringResource(desc)) {
        Q_strncpyz(buf, desc, sizeof(buf));
    } else {
        Com_sprintf(buf, sizeof(buf), "$$%s$$", desc);
    }

    if (!cgi.getStringResource) {
        return buf;
    }

    return cgi.getStringResource(buf);
}

static void CG_AddEntityToRadar(centity_t *cent, int selected, int type)
{
    const playerState_t *ps;
    const char          *archetype;
    char                 desc[256];
    char                 itypeBuf[256];
    const char          *name;
    const char          *itype;
    vec3_t               pos;
    vec4_t               color;
    float                d2;

    ps = &cg.snap->ps;

    archetype = cgi.CL_ConfigString(CG_CS_ARCHETYPE + cent->currentState.archeTypeIndex);
    if ((!archetype || !archetype[0]) && type == 0) {
        return;
    }

    desc[0]   = '\0';
    itypeBuf[0] = '\0';

    if (cgi.GetGameplayStringProperty) {
        cgi.GetGameplayStringProperty(archetype, "RadarDescription", desc, sizeof(desc));
    }
    name = CG_GetStringResource(desc);

    if (cgi.GetGameplayStringProperty) {
        cgi.GetGameplayStringProperty(archetype, "InteractiveType", itypeBuf, sizeof(itypeBuf));
    }
    itype = itypeBuf;

    if (!itype[0]) {
        if (type == 0) {
            return;
        }
        if (type == 1) {
            Q_strncpyz(itypeBuf, "MissionObjective", sizeof(itypeBuf));
        }
    }

    if (type == 0) {
        if (!CG_RadarShowInRadar(itype)) {
            return;
        }
    }

    CG_GetIdentificationData(cent, color);
    color[3] = 1.0f;

    pos[0] = (cent->lerpOrigin[0] - ps->origin[0]) * RADAR_NORMALIZE;
    pos[1] = (cent->lerpOrigin[1] - ps->origin[1]) * RADAR_NORMALIZE;
    pos[2] = (cent->lerpOrigin[2] - ps->origin[2]) * RADAR_NORMALIZE;

    if (type == 1) {
        d2 = DistanceSquared(cent->lerpOrigin, ps->origin);
        cgi.Radar_AddMissionObjectiveBlip(
            pos, color, name, selected, (d2 <= RADAR_WORLD_RANGE_SQ && d2 > 0.0f)
        );
    } else {
        cgi.Radar_AddBlip(pos, color, name, selected, CG_RadarShowInSimpleMode(itype));
    }
}

void CG_Target(centity_t *cent)
{
    const playerState_t *ps;
    float                d2;
    int                  selected;

    if (!cent) {
        return;
    }
    if (!cg.snap) {
        return;
    }

    ps = &cg.snap->ps;

    CG_UpdateTargetIdentifier(cent);

    selected = (cent->currentState.eFlags & EF_DISPLAY_INFO) != 0;

    cgi.Radar_SetViewAngles(ps->viewangles);

    if (cent->currentState.missionObjective) {
        CG_AddEntityToRadar(cent, selected, 1);
        return;
    }

    d2 = DistanceSquared(cent->lerpOrigin, ps->origin);
    if (d2 <= RADAR_WORLD_RANGE_SQ && d2 > 0.0f) {
        CG_AddEntityToRadar(cent, selected, 0);
    }
}

//        sentinel and renders as the literal "1", in retail too);
