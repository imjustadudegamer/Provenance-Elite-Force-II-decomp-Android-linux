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

#ifdef _WIN32
#    include <windows.h>
#endif

clientGameImport_t        cgi;
static clientGameExport_t cge;

cg_t          cg;
cgs_t         cgs;
int           cg_protocol;
centity_t     cg_entities[MAX_GENTITIES];

cvar_t *cg_fsDebug;

cvar_t *CG_FsDebug(void)
{
    if (!cg_fsDebug) {
        cg_fsDebug = cgi.Cvar_Get("fs_debug", "0", 0);
    }

    return cg_fsDebug;
}

cvar_t *cg_viewsize;
cvar_t *cg_viewposx;
cvar_t *cg_viewposy;
cvar_t *cg_viewsizewidth;
cvar_t *cg_viewsizeheight;
cvar_t *cg_android_aspect;
cvar_t *zoomfov;
cvar_t *cg_relativeyaw;
cvar_t *cg_marks;
cvar_t *cg_markTime;
cvar_t *cg_animSpeed;
cvar_t *cg_debugAnim;
cvar_t *cg_debugAnimWatch;
cvar_t *cg_errorDecay;
cvar_t *cg_nopredict;
cvar_t *cg_showmiss;
cvar_t *cg_stats;
cvar_t *cg_hidetempmodels;
cvar_t *cg_synchronousClients;
cvar_t *cg_stereoSeparation;
cvar_t *cg_lagometer;
cvar_t *paused;
cvar_t *cg_added_objective;
cvar_t *r_lerpmodels;
cvar_t *cg_3rd_person;
cvar_t *cg_camerahorizontaloffset;
cvar_t *cg_cameraheight;
cvar_t *cg_cameradist;
cvar_t *cg_cameraverticaldisplacement;
cvar_t *cg_camerascale;
cvar_t *cg_traceinfo;
cvar_t *cg_debugFootsteps;
cvar_t *cg_shadows;
cvar_t *developer;
cvar_t *cg_crosshair;
cvar_t *cg_crosshairtexture;
cvar_t *r_fadeDistMod;
cvar_t *cg_viewBob;
cvar_t *cg_viewStrafeTilt;
cvar_t *cg_viewBobMaxHorz;
cvar_t *cg_viewBobMaxVert;
cvar_t *cg_viewBobMaxRoll;
cvar_t *cg_viewBobTime;
cvar_t *cg_viewWeaponBob;
cvar_t *cg_viewKick;
cvar_t *cg_targetedPlayerName;
cvar_t *cg_spectatingPlayerName;
cvar_t *cg_debug;

void CG_RegisterCvars(void)
{
    cvar_t *temp;

    cg_viewsize                   = cgi.Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
    cg_viewposx                   = cgi.Cvar_Get("viewposx", "0", CVAR_ARCHIVE);
    cg_viewposy                   = cgi.Cvar_Get("viewposy", "0", CVAR_ARCHIVE);
    cg_viewsizewidth              = cgi.Cvar_Get("viewsizewidth", "-1", CVAR_ARCHIVE);
    cg_viewsizeheight             = cgi.Cvar_Get("viewsizeheight", "-1", CVAR_ARCHIVE);
    // aspect-aspect3d: port-only cvar (retail has no Android renderer); the client
    cg_android_aspect             = cgi.Cvar_Get("android_aspect", "0", CVAR_ARCHIVE);
    zoomfov                       = cgi.Cvar_Get("zoomfov", "90", 0);
    cg_relativeyaw                = cgi.Cvar_Get("relativeyaw", "0", 0);
    cg_marks                      = cgi.Cvar_Get("cg_marks", "1", CVAR_ARCHIVE);
    cg_markTime                   = cgi.Cvar_Get("cg_markTime", "10", CVAR_ARCHIVE);
    cg_animSpeed                  = cgi.Cvar_Get("cg_animspeed", "1", CVAR_CHEAT);
    cg_debugAnim                  = cgi.Cvar_Get("cg_debuganim", "0", CVAR_CHEAT);
    cg_debugAnimWatch             = cgi.Cvar_Get("cg_debuganimwatch", "0", CVAR_CHEAT);
    cg_errorDecay                 = cgi.Cvar_Get("cg_errordecay", "100", 0);
    cg_nopredict                  = cgi.Cvar_Get("cg_nopredict", "0", 0);
    cg_showmiss                   = cgi.Cvar_Get("cg_showmiss", "0", 0);
    cg_stats                      = cgi.Cvar_Get("cg_stats", "0", 0);
    cg_hidetempmodels             = cgi.Cvar_Get("cg_hidetempmodels", "0", 0);
    cg_synchronousClients         = cgi.Cvar_Get("g_syncronousClients", "0", 0);
    cg_stereoSeparation           = cgi.Cvar_Get("cg_stereosep", "0.4", CVAR_ARCHIVE);
    cg_lagometer                  = cgi.Cvar_Get("cg_lagometer", "0", 0);
    paused                        = cgi.Cvar_Get("paused", "0", 0);
    cg_added_objective            = cgi.Cvar_Get("added_objective", "0", 0);
    r_lerpmodels                  = cgi.Cvar_Get("r_lerpmodels", "1", 0);
    cg_3rd_person                 = cgi.Cvar_Get("cg_3rd_person", "0", 0);
    cg_camerahorizontaloffset     = cgi.Cvar_Get("cg_camerahorizontaloffset", "0", CVAR_ARCHIVE);
    cg_cameraheight               = cgi.Cvar_Get("cg_cameraheight", "18", CVAR_ARCHIVE);
    cg_cameradist                 = cgi.Cvar_Get("cg_cameradist", "120", CVAR_ARCHIVE);
    cg_cameraverticaldisplacement = cgi.Cvar_Get("cg_cameraverticaldisplacement", "-2", CVAR_ARCHIVE);
    cg_camerascale                = cgi.Cvar_Get("cg_camerascale", "1.0", CVAR_ARCHIVE);
    cg_traceinfo                  = cgi.Cvar_Get("cg_traceinfo", "0", CVAR_ARCHIVE);
    cg_debugFootsteps             = cgi.Cvar_Get("cg_debugfootsteps", "0", CVAR_CHEAT);
    cg_shadows                    = cgi.Cvar_Get("cg_shadows", "1", CVAR_ARCHIVE);
    developer                     = cgi.Cvar_Get("developer", "0", CVAR_CHEAT);
    cg_crosshair                  = cgi.Cvar_Get("cg_crosshair", "1", CVAR_ARCHIVE);
    cg_crosshairtexture           = cgi.Cvar_Get("cg_crosshairTexture", "textures/sprites/crosshair/crosshair_2", CVAR_ARCHIVE);
    r_fadeDistMod                 = cgi.Cvar_Get("r_fadeDistMod", "1", CVAR_ARCHIVE);
    cg_viewBob                    = cgi.Cvar_Get("cg_viewBob", "1", CVAR_ARCHIVE);
    cg_viewStrafeTilt             = cgi.Cvar_Get("cg_viewStrafeTilt", "1", CVAR_ARCHIVE);
    cg_viewBobMaxHorz             = cgi.Cvar_Get("cg_viewBobMaxHorz", "2", 0);
    cg_viewBobMaxVert             = cgi.Cvar_Get("cg_viewBobMaxVert", "3", 0);
    cg_viewBobMaxRoll             = cgi.Cvar_Get("cg_viewBobMaxRoll", ".2", 0);
    cg_viewBobTime                = cgi.Cvar_Get("cg_viewBobTime", "2", 0);
    cg_viewWeaponBob              = cgi.Cvar_Get("cg_viewWeaponBob", "1.0", CVAR_ARCHIVE);
    cg_viewKick                   = cgi.Cvar_Get("cg_viewKick", "1", CVAR_ARCHIVE);
    cg_targetedPlayerName         = cgi.Cvar_Get("cg_targetedPlayerName", "", 0);
    cg_spectatingPlayerName       = cgi.Cvar_Get("cg_spectatingPlayerName", "", 0);
    cg_debug                      = cgi.Cvar_Get("cg_debug", "0", 0);

    temp            = cgi.Cvar_Get("sv_running", "0", 0);
    cgs.localServer = temp->integer;
}
int qsort_compare_strings(const void *s1, const void *s2)
{
    return Q_stricmp(*(const char **)s1, *(const char **)s2);
}

void CG_RegisterSounds(void)
{
    int i;

    for (i = 0; i < 10; i++) {
        if (!CG_Command_ProcessFile(va("global/global%d.scr", i), qtrue, -1)) {
            break;
        }
    }
}

void CG_ProcessConfigString(int num)
{
    const char *str;
    size_t      index;

    str = CG_ConfigString(num);

    if (num >= CS_MODELS && num < CS_MODELS + MAX_MODELS) {
        index = num - CS_MODELS;

        if (str && str[0]) {
            qhandle_t hModel = cgi.R_RegisterModel(str);

            cgs.model_draw[index] = hModel;

            if (strstr(str, ".tik")) {
                int tiki = cgi.R_Model_GetHandle(hModel);

                cgs.model_tiki[index] = tiki;
                if (tiki >= 0) {
                    CG_ProcessInitCommands(tiki, NULL);
                }
            }
        }
    }

    {
        // stood here was MOHAA residue fed by a server-side publisher retail does not have.
        switch (num) {
        case CS_SOUNDTRACK:
            cgi.MUSIC_NewSoundtrack(str);
            return;
        case CS_FOGINFO:
            CG_ParseFogInfo(str);
            return;
        case CS_SKYINFO:
            sscanf(str, "%f %d", &cg.sky_alpha, &cg.sky_portal);
            return;
        case CS_SERVERINFO:
            CG_ParseServerinfo();
            return;
        case CS_LEVEL_START_TIME:
            cgs.levelStartTime = atoi(str);
            return;
        }

        if (num >= CS_SOUNDS && num < CS_SOUNDS + MAX_SOUNDS) {
            if (str && *str && *str != '*') {
                cgs.sound_precache[num - CS_SOUNDS] = cgi.S_RegisterSound(str);
            }
        } else if (num >= CS_LIGHTSTYLES && num < CS_LIGHTSTYLES + MAX_LIGHTSTYLES) {
            CG_SetLightStyle(num - CS_LIGHTSTYLES, str);
        } else if (num >= CS_PLAYERS && num < CS_PLAYERS + MAX_CLIENTS) {
            const char *value;

            index = num - CS_PLAYERS;

            value = Info_ValueForKey(str, "name");
            if (!value) {
                value = "UnnamedSoldier";
            }
            Q_strncpyz(cg.clientinfo[index].name, value, sizeof(cg.clientinfo[index].name));

            value = Info_ValueForKey(str, "team");
            if (value) {
                cg.clientinfo[index].team = atoi(value);
            } else {
                cg.clientinfo[index].team = TEAM_NONE;
            }
        }
    }
}


void CG_PrepRefresh(void)
{
    int i;

    memset(&cg.refdef, 0, sizeof(cg.refdef));

    cgi.R_LoadWorldMap(cgs.mapname);

    // misattribution of this instruction: retail never -1-fills inlineDrawModel
    memset(cgs.model_tiki, -1, sizeof(cgs.model_tiki));

    cgs.numInlineModels = cgi.CM_NumInlineModels();

    for (i = 1; i < cgs.numInlineModels; i++) {
        char   name[10];
        vec3_t mins, maxs;
        int    j;

        Com_sprintf(name, sizeof(name), "*%i", i);
        cgs.inlineDrawModel[i] = cgi.R_RegisterModel(name);
        cgi.R_ModelBounds(cgs.inlineDrawModel[i], mins, maxs);

        for (j = 0; j < 3; j++) {
            cgs.inlineModelMidpoints[i][j] = mins[j] + 0.5 * (maxs[j] - mins[j]);
        }
    }
    cgi.ProcessLoadingScreen("$$LoadingMedia$$");  

    cgs.media.shadowMarkShader         = cgi.R_RegisterShader("markShadow");
    cgs.media.footShadowMarkShader     = cgs.media.shadowMarkShader;
    cgs.media.wakeMarkShader           = cgi.R_RegisterShader("ripple.spr");
    cgs.media.identificationShader     = cgi.R_RegisterShader("hud/identification");
    cgs.media.lagometerShader          = cgi.R_RegisterShaderNoMip("sysimg/2d/blank"); 
    cgs.media.pausedShader             = cgi.R_RegisterShaderNoMip("hud/paused");
    cgs.media.disconnectedShader       = cgi.R_RegisterShaderNoMip("sysimg/2d/disconnected");
    cgs.media.backTileShader           = cgi.R_RegisterShader("sysimg/2d/backtile"); 
    cgs.media.crosshairShader          = cgi.R_RegisterShaderNoMip(cg_crosshairtexture->string);
    cgs.media.useCrosshairShader       = cgi.R_RegisterShaderNoMip("textures/sprites/crosshair/use1");
    cgs.media.tricorderCrosshairShader = cgi.R_RegisterShaderNoMip("textures/sprites/crosshair/use2");
    cgs.media.damageFrontShader        = cgi.R_RegisterShaderNoMip("textures/sprites/damageFront");
    cgs.media.damageBackShader         = cgi.R_RegisterShaderNoMip("textures/sprites/damageBack");
    cgs.media.damageLeftShader         = cgi.R_RegisterShaderNoMip("textures/sprites/damageLeft");
    cgs.media.damageRightShader        = cgi.R_RegisterShaderNoMip("textures/sprites/damageRight");
    cgs.media.levelExitShader          = 0;
    // own cg_public.h note: retail's refexport[51] exists but CL_InitCGameDLL never
    cgs.media.hudDrawFont   = NULL;
    cgs.media.attackerFont  = cgi.R_LoadFont("facfont-20");
    cgs.media.objectiveFont = cgi.R_LoadFont("facfont-20"); 

    for (i = CS_SYSTEMINFO + 1; i < MAX_CONFIGSTRINGS; i++) {
        CG_ProcessConfigString(i);
        cgi.ProcessLoadingScreen("$$LoadingMedia$$");  
    }

    // layer.  (Retail passes the BARE map name here -- cgs+0xC568 -- not the .bsp path.)
    {
        const char *environmentName = NULL;
        const char *levelName       = NULL;
        const char *sublevelName    = NULL;

        cgi.GetLevelDefs(cgs.rawMapName, &environmentName, &levelName, &sublevelName);
        cgi.loadLevelStrings(environmentName);
    }

    cgi.Dialog_ResetConsole("DialogConsole");
    cgi.Cvar_Set("added_objective", "0");
    cgi.Cmd_Stuff("ui_enableinventory\n");
}


const char *CG_ConfigString(int index)
{
    if (index < 0 || index >= MAX_CONFIGSTRINGS) {
        cgi.Error(ERR_DROP, "CG_ConfigString: bad index: %i", index);
    }
    return cgs.gameState.stringData + cgs.gameState.stringOffsets[index];
}


void CG_GetRendererConfig(void)
{
    int dx, dy, dw, dh;

    cgi.GetGlconfig(&cgs.glconfig);

    CG_AndroidDisplayRect(cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, &dx, &dy, &dw, &dh);
    cgs.screenXScale  = (float)dw / 640.0;
    cgs.screenYScale  = (float)dh / 480.0;

    cgi.UI_GetHighResolutionScale(cgs.uiHiResScale);
}

void CG_Precache(void)
{
    char        filename[520];
    const char *environmentName = NULL;
    const char *levelName       = NULL;
    const char *sublevelName    = NULL;
    int         i;

    cgi.GetLevelDefs(cgs.mapname, &environmentName, &levelName, &sublevelName);

    for (i = 0; i < 10; i++) {
        if (i) {
            Com_sprintf(filename, sizeof(filename), "precache/client/global%d.txt", i);
        } else {
            Q_strncpyz(filename, "precache/client/global.txt", sizeof(filename));
        }

        if (!CG_Command_ProcessFile(filename, qtrue, -1)) {
            break;
        }
    }

    if (environmentName && strlen(environmentName)) {
        for (i = 0; i < 10; i++) {
            if (i) {
                Com_sprintf(filename, sizeof(filename), "precache/client/%s%d.txt", environmentName, i);
            } else {
                Com_sprintf(filename, sizeof(filename), "precache/client/%s.txt", environmentName);
            }

            if (!CG_Command_ProcessFile(filename, qtrue, -1)) {
                break;
            }
        }
    }

    if (levelName && strlen(levelName)) {
        for (i = 0; i < 10; i++) {
            if (i) {
                Com_sprintf(filename, sizeof(filename), "precache/client/%s%d.txt", levelName, i);
            } else {
                Com_sprintf(filename, sizeof(filename), "precache/client/%s.txt", levelName);
            }

            if (!CG_Command_ProcessFile(filename, qtrue, -1)) {
                break;
            }
        }
    }

    if (sublevelName && strlen(sublevelName) && Q_stricmp(sublevelName, levelName)) {
        for (i = 0; i < 10; i++) {
            if (i) {
                Com_sprintf(filename, sizeof(filename), "precache/client/%s%d.txt", sublevelName, i);
            } else {
                Com_sprintf(filename, sizeof(filename), "precache/client/%s.txt", sublevelName);
            }

            if (!CG_Command_ProcessFile(filename, qtrue, -1)) {
                break;
            }
        }
    }
}

void CG_GameStateReceived(void)
{
    const char *s;
    int checksum;

    memset(&cg, 0, sizeof(cg));
    cg.cachedViewMode = -1;  
    memset(cg_entities, 0, sizeof(cg_entities));

    CG_ClearLightStyles();

    CG_GetRendererConfig();

    cgi.GetGameState(&cgs.gameState);

    cgi.ProcessLoadingScreen("$$LoadingClientGame$$");  

    s = CG_ConfigString(CS_GAME_VERSION);
    if (strcmp(s, GAME_VERSION)) {
        cgi.Error(ERR_DROP, "Client/Server game mismatch: %s/%s", GAME_VERSION, s);
    }

    s                  = CG_ConfigString(CS_LEVEL_START_TIME);
    cgs.levelStartTime = atoi(s);

    CG_ParseServerinfo();

    cgi.CM_LoadMap(cgs.mapname, &checksum);

    CG_InitMarks();

    CG_RegisterSounds();

    CG_PrepRefresh();

    CG_Precache();  
}

void CG_ServerRestarted(void)
{
    const char *s;

    s                  = CG_ConfigString(CS_LEVEL_START_TIME);
    cgs.levelStartTime = atoi(s);

    CG_ParseServerinfo();

    cg.thisFrameTeleport    = qtrue;
    cg.viewInterpTime       = -1;
    cg.bIntermissionDisplay = qfalse;

    CG_RestartCommandManager(cg.oldFrameTime - cg.time);
    CG_RestartBeams(cg.oldFrameTime - cg.time);
    CG_InitMarks();
    CG_ClearSwipes();
    CG_ResetTempModels();
    CG_InitializeWeather();
}

/*
 * cginit is one SMP_LOAD zone worth ~3.3 s on the device and CG_Init's own body is
 * a few registrations, so the time is inside one of them.  These legs say which.
 * cgi.Milliseconds is the module's own clock, available before anything is set up.
 */
#define CGINIT_LEG_COUNT 6

static int  s_cginitLegs[CGINIT_LEG_COUNT];
static int  s_cginitLegT0[CGINIT_LEG_COUNT];

#define CGINIT_LEG_BEGIN( leg ) \
    do { s_cginitLegT0[ leg ] = cgi.Milliseconds(); } while ( 0 )
#define CGINIT_LEG_END( leg ) \
    do { s_cginitLegs[ leg ] += cgi.Milliseconds() - s_cginitLegT0[ leg ]; } while ( 0 )

void CG_Init(clientGameImport_t *imported, int serverMessageNum, int serverCommandSequence)
{
    int leg;
    int cginitT0;

    cgi = *imported;

    // does not exist in retail cgame.  Leaving the gates preserves that.
    cg_protocol = cgi.Cvar_Get("com_protocol", "", 0)->integer;

    memset(&cg, 0, sizeof(cg));
    memset(&cgs, 0, sizeof(cgs));
    cg.farplane_fog = qtrue;

    cgs.processedSnapshotNum  = serverMessageNum;
    cgs.serverCommandSequence = serverCommandSequence;

    for ( leg = 0; leg < CGINIT_LEG_COUNT; leg++ ) {
        s_cginitLegs[ leg ] = 0;
    }
    cginitT0 = cgi.Milliseconds();

    CGINIT_LEG_BEGIN( 0 );
    CG_RegisterCvars();
    CGINIT_LEG_END( 0 );

    CGINIT_LEG_BEGIN( 1 );
    L_InitEvents();

    Swap_Init();
    CGINIT_LEG_END( 1 );

    CGINIT_LEG_BEGIN( 2 );
    CG_InitializeCommandManager();

    CG_InitializeWeather();
    CGINIT_LEG_END( 2 );

    CGINIT_LEG_BEGIN( 3 );
    CG_GameStateReceived();
    CGINIT_LEG_END( 3 );

    CGINIT_LEG_BEGIN( 4 );
    CG_InitConsoleCommands();
    CGINIT_LEG_END( 4 );

    CGINIT_LEG_BEGIN( 5 );
    CG_InitFootprintMap();
    CGINIT_LEG_END( 5 );

    cg.cachedViewMode = -1;

    LOADHANG_PROBE( "cginit",
                    "total=%d cvars=%d events=%d cmds=%d gamestate=%d console=%d footprint=%d\n",
                    cgi.Milliseconds() - cginitT0,
                    s_cginitLegs[ 0 ], s_cginitLegs[ 1 ], s_cginitLegs[ 2 ],
                    s_cginitLegs[ 3 ], s_cginitLegs[ 4 ], s_cginitLegs[ 5 ] );
}

void CG_Shutdown(void)
{
    L_ShutdownEvents();
    // MOHAA's UI_HideMenu("StatsScreen") tail is not retail and is gone.
    CG_ClearFootprintMap();
}

clientGameExport_t *GetCGameAPI(void)
{
    cge.CG_Init            = CG_Init;
    cge.CG_Shutdown        = CG_Shutdown;
    cge.CG_DrawActiveFrame = CG_DrawActiveFrame;
    cge.CG_ConsoleCommand  = CG_ConsoleCommand;
    cge.CG_GetRendererConfig = CG_GetRendererConfig;
    cge.CG_Draw2D            = CG_Draw2D;
    cge.CG_GetInterpState    = CG_GetInterpState;
    cge.CG_GetViewOrigin     = CG_GetViewOrigin;
    cge.CG_GetViewAngles     = CG_GetViewAngles;
    cge.CG_GetThirdPerson    = CG_GetThirdPerson;
    cge.CG_ProcessSnapshots  = CG_ProcessSnapshots;
    cge.CG_UpdateClient      = CG_UpdateClient;

    return &cge;
}

void CG_DrawActive(stereoFrame_t stereoView)
{
    float  separation;
    vec3_t baseOrg;

    switch (stereoView) {
    case STEREO_CENTER:
        separation = 0;
        break;
    case STEREO_LEFT:
        separation = -cg_stereoSeparation->value / 2;
        break;
    case STEREO_RIGHT:
        separation = cg_stereoSeparation->value / 2;
        break;
    default:
        separation = 0;
        cgi.Error(ERR_DROP, "CG_DrawActive: Undefined stereoView");
    }

    CG_TileClear();

    VectorCopy(cg.refdef.vieworg, baseOrg);
    if (separation != 0) {
        VectorMA(cg.refdef.vieworg, -separation, cg.refdef.viewaxis[1], cg.refdef.vieworg);
    }

    cgi.R_RenderScene(&cg.refdef);

    if (separation != 0) {
        VectorCopy(baseOrg, cg.refdef.vieworg);
    }
}

#ifndef CGAME_HARD_LINKED

void Com_Error(int level, const char *error, ...)
{
    va_list argptr;
    char    text[1024];

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    cgi.Error(level, "%s", text);
    Q_UNREACHABLE();
}

void Com_Printf(const char *msg, ...)
{
    va_list argptr;
    char    text[1024];

    va_start(argptr, msg);
    Q_vsnprintf(text, sizeof(text), msg, argptr);
    va_end(argptr);

    cgi.Printf("%s", text);
}

#endif

void CG_ParseFogInfo(const char *str)
{

    cg.farplane_bias            = 0.001f;

    sscanf(
        str,
        "%d %d %f %f %f %f",
        &cg.farplane_cull,
        &cg.farplane_fog,
        &cg.farplane_distance,
        &cg.farplane_color[0],
        &cg.farplane_color[1],
        &cg.farplane_color[2]
    );
}
