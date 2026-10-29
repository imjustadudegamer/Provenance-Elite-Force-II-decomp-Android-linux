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

#define PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_probe.h"

void CG_AdjustFrom640(float *x, float *y, float *w, float *h)
{
#if 0
	if ( cgs.glconfig.vidWidth * 480 > cgs.glconfig.vidHeight * 640 ) {
		*x += 0.5 * ( cgs.glconfig.vidWidth - ( cgs.glconfig.vidHeight * 640 / 480 ) );
	}
#endif
    *x = *x * cgs.screenXScale;
    *y = *y * cgs.screenYScale;
    *w *= cgs.screenXScale;
    *h *= cgs.screenYScale;
}

void CG_TileClearBox(int x, int y, int w, int h, qhandle_t hShader)
{
    float s1, t1, s2, t2;

    s1 = x / 64.0;
    t1 = y / 64.0;
    s2 = (x + w) / 64.0;
    t2 = (y + h) / 64.0;
    cgi.R_DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

void CG_TileClear(void)
{
    int top, bottom, left, right;
    int w, h;

    w = cgs.glconfig.vidWidth;
    h = cgs.glconfig.vidHeight;

    if (cg.refdef.x == 0 && cg.refdef.y == 0 && cg.refdef.width == w && cg.refdef.height == h) {
        return; 
    }

    top    = cg.refdef.y;
    bottom = top + cg.refdef.height - 1;
    left   = cg.refdef.x;
    right  = left + cg.refdef.width - 1;

    CG_TileClearBox(0, 0, w, top, cgs.media.backTileShader);

    CG_TileClearBox(0, bottom, w, h - bottom, cgs.media.backTileShader);

    CG_TileClearBox(0, top, left, bottom - top + 1, cgs.media.backTileShader);

    CG_TileClearBox(right, top, w - right, bottom - top + 1, cgs.media.backTileShader);
}


#define LAG_SAMPLES 128

typedef struct {
    int frameSamples[LAG_SAMPLES];
    int frameCount;
    int snapshotFlags[LAG_SAMPLES];
    int snapshotSamples[LAG_SAMPLES];
    int snapshotCount;
} lagometer_t;

lagometer_t lagometer;

void CG_AddLagometerFrameInfo(void)
{
    int offset;

    offset                                                           = cg.time - cg.latestSnapshotTime;
    lagometer.frameSamples[lagometer.frameCount & (LAG_SAMPLES - 1)] = offset;
    lagometer.frameCount++;
}

void CG_AddLagometerSnapshotInfo(snapshot_t *snap)
{
    if (!snap) {
        lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = -1;
        lagometer.snapshotCount++;
        return;
    }

    lagometer.snapshotSamples[lagometer.snapshotCount & (LAG_SAMPLES - 1)] = snap->ping;
    lagometer.snapshotFlags[lagometer.snapshotCount & (LAG_SAMPLES - 1)]   = snap->snapFlags;
    lagometer.snapshotCount++;
}

void CG_DrawDisconnect(void)
{
    float     x, y;
    float     w, h;
    int       cmdNum;
    qhandle_t handle;
    usercmd_t cmd;

    if (!cgs.gametype) {
        return;
    }

    cmdNum = cgi.GetCurrentCmdNumber() - CMD_BACKUP + 1;
    cgi.GetUserCmd(cmdNum, &cmd);

    if (cg.snap && (cmd.serverTime <= cg.snap->ps.commandTime
                    || cmd.serverTime > cg.time
                    || (cg.snap->ps.pm_flags & PMF_INTERMISSION))) {
        return;
    }

    if ((cg.time >> 9) & 1) {
        return;
    }

    // TIKIANIM-C: retail's path, not MOHAA's.  "gfx/2d/net.tga" is the Q3/MOHAA phone-jack
    handle = cgi.R_RegisterShader("sysimg/2d/disconnected");
    x      = 544.0f * cgs.screenXScale;
    y      = 216.0f * cgs.screenYScale;
    w      = 48.0f * cgs.screenXScale;
    h      = 48.0f * cgs.screenYScale;

    cgi.R_DrawStretchPic(x, y, w, h, 0, 0, 1, 1, handle);
}

#define MAX_LAGOMETER_PING  900
#define MAX_LAGOMETER_RANGE 300

void CG_DrawLagometer(void)
{
    int   a, i;
    float v;
    float ax, ay, aw, ah, mid, range;
    int   color;
    float vscale;

    if (!cg_lagometer->integer) {
        CG_DrawDisconnect();
        return;
    }

    ax = 272.0;
    ay = 432.0;
    aw = 96.0;
    ah = 48.0;
    CG_AdjustFrom640(&ax, &ay, &aw, &ah);

    cgi.R_SetColor(NULL);
    cgi.R_DrawStretchPic(ax, ay, aw, ah, 0, 0, 1, 1, cgs.media.lagometerShader);

    color = -1;
    range = ah / 3;
    mid   = ay + range;

    vscale = range / MAX_LAGOMETER_RANGE;

    for (a = 0; a < aw; a++) {
        i = (lagometer.frameCount - 1 - a) & (LAG_SAMPLES - 1);
        v = lagometer.frameSamples[i];
        v *= vscale;
        if (v > 0) {
            if (color != 1) {
                color = 1;
                cgi.R_SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
            }
            if (v > range) {
                v = range;
            }
            cgi.R_DrawBox(ax + aw - a, mid - v, 1, v);
        } else if (v < 0) {
            if (color != 2) {
                color = 2;
                cgi.R_SetColor(g_color_table[ColorIndex(COLOR_BLUE)]);
            }
            v = -v;
            if (v > range) {
                v = range;
            }
            cgi.R_DrawBox(ax + aw - a, mid, 1, v);
        }
    }

    range  = ah / 2;
    vscale = range / MAX_LAGOMETER_PING;

    for (a = 0; a < aw; a++) {
        i = (lagometer.snapshotCount - 1 - a) & (LAG_SAMPLES - 1);
        v = lagometer.snapshotSamples[i];
        if (v > 0) {
            if (lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED) {
                if (color != 5) {
                    color = 5; 
                    cgi.R_SetColor(g_color_table[ColorIndex(COLOR_YELLOW)]);
                }
            } else {
                if (color != 3) {
                    color = 3;
                    cgi.R_SetColor(g_color_table[ColorIndex(COLOR_GREEN)]);
                }
            }
            v = v * vscale;
            if (v > range) {
                v = range;
            }
            cgi.R_DrawBox(ax + aw - a, ay + ah - v, 1, v);
        } else if (v < 0) {
            if (color != 4) {
                color = 4; 
                cgi.R_SetColor(g_color_table[ColorIndex(COLOR_RED)]);
            }
            cgi.R_DrawBox(ax + aw - a, ay + ah - range, 1, range);
        }
    }

    cgi.R_SetColor(NULL);

    CG_DrawDisconnect();
}


const char *CG_EntityInteractiveTypeName(const centity_t *cent, char *out, int outlen)
{
    const char *archetype;
    int         idx;

    out[0] = '\0';

    if (!cgi.GetGameplayStringProperty || !cgi.CL_ConfigString) {
        return out;
    }

    idx = cent->currentState.archeTypeIndex;
    if (idx <= 0 || idx >= MAX_ARCHETYPES) {
        return out;
    }

    archetype = cgi.CL_ConfigString(CG_CS_ARCHETYPE + idx);
    if (!archetype || !archetype[0]) {
        return out;
    }

    cgi.GetGameplayStringProperty(archetype, "InteractiveType", out, outlen);
    return out;
}

static const char *CG_TargetDescriptionProperty(int descLevel)
{
    switch (descLevel) {
    case 1:
        return "Description1";
    case 2:
    case 3:
        return "Description2";
    default:
        return NULL;
    }
}

const char *CG_CreateTargetString(const centity_t *cent, const char *prop, char *out, int outlen)
{
    const char *archetype;
    int         idx;

    out[0] = '\0';
    if (!cent || !prop || !cgi.GetGameplayStringProperty || !cgi.CL_ConfigString) {
        return out;
    }
    idx = cent->currentState.archeTypeIndex;
    if (idx <= 0 || idx >= MAX_ARCHETYPES) {
        return out;
    }
    archetype = cgi.CL_ConfigString(CG_CS_ARCHETYPE + idx);
    if (!archetype || !archetype[0]) {
        return out;
    }

    cgi.GetGameplayStringProperty(archetype, prop, out, outlen);
    if (!out[0]) {
        return out;
    }

    {
        char wrapped[256];

        if (cgi.isStringResource && !cgi.isStringResource(out)) {
            Com_sprintf(wrapped, sizeof(wrapped), "$$%s$$", out);
        } else {
            Q_strncpyz(wrapped, out, sizeof(wrapped));
        }

        if (cgi.getStringResource) {
            const char *resolved = cgi.getStringResource(wrapped);

            if (!resolved) {
                out[0] = '\0';
                return out;
            }
            Q_strncpyz(out, resolved, outlen);
        } else {
            Q_strncpyz(out, wrapped, outlen);
        }
    }

    return out;
}

void CG_InteractiveTypeColor(const char *itype, vec4_t out)
{
    out[0] = 1.0f;
    out[1] = 1.0f;
    out[2] = 1.0f;
    out[3] = 1.0f;

    if (!itype[0] || !cgi.HasGameplayProperty || !cgi.GetGameplayFloatProperty) {
        return;
    }

    if (!cgi.HasGameplayProperty(itype, "Red") && !cgi.HasGameplayProperty(itype, "Green")
        && !cgi.HasGameplayProperty(itype, "Blue")) {
        return;
    }

    out[0] = cgi.GetGameplayFloatProperty(itype, "Red");
    out[1] = cgi.GetGameplayFloatProperty(itype, "Green");
    out[2] = cgi.GetGameplayFloatProperty(itype, "Blue");
}

typedef struct {
    int    entnum;     
    float  xMax, yMax;  
    float  xMin, yMin;  
    int    draw;
    /* cg+0xA3DC4: set by CG_Target @0x00059000, cleared per frame by CG_TargetInitialize */
    int    identDisplay;
    int    descLevel;  
    int    timer2;     
    int    timer3;     
    int    lastReveal;
    vec4_t color;
    int    frame;       /* bumped by CG_TargetInitialize, once per frame */
    int    drawStamp;   /* frame CG_DisplayTargetIdentifier last ran */
    int    identStamp;  /* frame an EF_DISPLAY_INFO entity was last seen */
} cgIdentifier_t;

static cgIdentifier_t cg_ident = {-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {1, 1, 1, 0}, 0, 0, 0};

void CG_DrawCrosshair()
{
    float x, y;
    float width, height;

    if (!cg.snap) {
        return;
    }

    if (paused->integer) {
        return;
    }

    if ((cg.snap->ps.pm_flags & PMF_INTERMISSION) || (cg.snap->ps.informationStates & 1)) {
        return;
    }

    // 0x2E084..0x2E1B8, and it is driven by the SERVER's identification latch, not by
    //   0x2E0A0 lwz r0,0x3dc4(cg)          ; the EF_DISPLAY_INFO latch
    //   0x2E14C beq 0x2E15C                ; latch == 0 -> default shader
    vec4_t    xhairColor = {1.0f, 1.0f, 1.0f, 0.5f};
    qhandle_t shader = cgs.media.crosshairShader;
    float     dist = 0.0f;

    if (!cg_crosshair->integer) {
        return;
    }

    if (cg_ident.identDisplay && cgs.gametype == 0) {
        if (cg_ident.identStamp != cg_ident.frame) {
            PROBE_EMIT_N("[CG:", "target", PROBE_ROW_CAP,
                         "staleident f=%d ent=%d colour=%.0f,%.0f,%.0f no EF_DISPLAY_INFO entity"
                         " this frame\n",
                         cg_ident.frame, cg_ident.entnum, cg_ident.color[0] * 255.0f,
                         cg_ident.color[1] * 255.0f, cg_ident.color[2] * 255.0f);
        }
        xhairColor[0] = cg_ident.color[0];
        xhairColor[1] = cg_ident.color[1];
        xhairColor[2] = cg_ident.color[2];
    }

    if (cg_ident.entnum >= 0) {
        vec3_t delta;

        VectorSubtract(cg_entities[cg_ident.entnum].lerpOrigin, cg.refdef.vieworg, delta);
        dist = VectorLength(delta);
    }

    xhairColor[3] = 0.5f;

    if (cg_ident.identDisplay && dist <= 96.0f) {
        char        itypeBuf[64];
        const char *itypeName = CG_EntityInteractiveTypeName(
            &cg_entities[cg_ident.entnum], itypeBuf, sizeof(itypeBuf));

        if (!Q_stricmp(itypeName, "Usable")) {
            if (cgs.media.useCrosshairShader) {
                shader = cgs.media.useCrosshairShader;
            }
        } else if (!Q_stricmp(itypeName, "TricorderUsable")) {
            if (cgs.media.tricorderCrosshairShader) {
                shader = cgs.media.tricorderCrosshairShader;
            }
        }
    }

    if (shader) {
        const float *crosshairOffset = cgi.get_crosshairoffset();

        {
            int dw, dh;

            CG_AndroidDisplayRect(cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, NULL, NULL, &dw, &dh);

            width  = 32.0f * cgs.uiHiResScale[0];
            height = 32.0f * cgs.uiHiResScale[1];
            x      = ((float)dw - width) * 0.5f + crosshairOffset[0] * cgs.uiHiResScale[0];
            y      = ((float)dh - height) * 0.5f + crosshairOffset[1] * cgs.uiHiResScale[1];
        }

        cgi.R_SetColor(xhairColor);
        cgi.R_DrawStretchPic(x, y, width, height, 0, 0, 1, 1, shader);
    }
}

static void CG_ResetTargetIdentifier(void)
{
    if (cg_ident.entnum >= 0) {
        PROBE_CENSUS_EMIT("[CG:", "target", "clear f=%d ent=%d desc=%d\n",
                          cg_ident.frame, cg_ident.entnum, cg_ident.descLevel);
    }
    cg_ident.entnum    = -1;
    cg_ident.draw      = 0;
    cg_ident.descLevel = 0;
    cg_ident.timer2    = 0;
    cg_ident.timer3    = 0;
}

static void CG_CalculateBoundingBoxPoints(const centity_t *cent, vec3_t points[8])
{
    vec3_t   mins, maxs;
    dtiki_t *tiki;
    int      handle;
    int      i;

    mins[0] = mins[1] = mins[2] = 0.0f;
    maxs[0] = maxs[1] = maxs[2] = 0.0f;

    handle = cgi.R_Model_GetHandle(cgs.model_draw[cent->currentState.modelindex]);

    if (handle >= 0) {
        tiki = cgi.TIKI_GetTiki(handle);
        if (tiki) {
            VectorCopy(tiki->boundingVolumeMin, mins);
            VectorCopy(tiki->boundingVolumeMax, maxs);
        }
    }

    if (mins[0] == 0.0f && mins[1] == 0.0f && mins[2] == 0.0f && maxs[0] == 0.0f && maxs[1] == 0.0f && maxs[2] == 0.0f) {
        IntegerToBoundingBox(cent->currentState.solid, mins, maxs);
    }

    VectorAdd(mins, cent->lerpOrigin, mins);
    VectorAdd(maxs, cent->lerpOrigin, maxs);

    for (i = 0; i < 8; i++) {
        points[i][0] = (i & 1) ? maxs[0] : mins[0];
        points[i][1] = (i & 2) ? maxs[1] : mins[1];
        points[i][2] = (i & 4) ? maxs[2] : mins[2];
    }
}

// NOT rejected: retail takes fabs of the depth ratio, see the note in the loop.
static qboolean CG_WorldPointsToScreen(const vec3_t *points, float *xs, float *ys, int count)
{
    vec3_t forward, right, up, local;
    float  xf, xr, xu;
    float  tanHalfX, tanHalfY;
    int    i;

    AngleVectors(cg.refdefViewAngles, forward, right, up);

    tanHalfX = (float)tan(DEG2RAD(cg.refdef.fov_x) * 0.5f);
    tanHalfY = (float)tan(DEG2RAD(cg.refdef.fov_y) * 0.5f);

    if (tanHalfX <= 0.0f || tanHalfY <= 0.0f) {
        return qfalse;
    }

    for (i = 0; i < count; i++) {
        VectorSubtract(points[i], cg.refdef.vieworg, local);

        xf = DotProduct(local, forward);

        // not clamp to a near plane.  Raw words of CG_WorldPointsToScreen @0x0002E300:
        xr = DotProduct(local, right);
        xu = DotProduct(local, up);

        xf = fabsf(xf);

        xs[i] = 320.0f - xr * 320.0f / (xf * tanHalfX);
        ys[i] = 240.0f - xu * 240.0f / (xf * tanHalfY);
    }

    return qtrue;
}

static int CG_qsortCmpFloatLess(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;

    if (fa <= fb) {
        return fa < fb;
    }

    return -1;
}

static void CG_ClipIdentifierBoxToScreen(void)
{
    float xMargin = cg_ident.draw ? 507.0f : 635.0f;

    if (cg_ident.xMin < 5.0f) {
        cg_ident.xMin = 5.0f;
    }
    if (cg_ident.yMin < 5.0f) {
        cg_ident.yMin = 5.0f;
    }

    if (cg_ident.xMax > xMargin) {
        cg_ident.xMax = xMargin;
    }
    if (cg_ident.yMax > 475.0f) {
        cg_ident.yMax = 475.0f;
    }

}

static void CG_DisplayTargetIdentifier(const centity_t *cent, int a3)
{
    vec3_t points[8];
    float  xs[8], ys[8];
    vec3_t dir, fwd;

    CG_CalculateBoundingBoxPoints(cent, points);

    VectorSubtract(cent->lerpOrigin, cg.refdef.vieworg, dir);
    VectorNormalize(dir);

    {
        vec3_t yawOnly;

        yawOnly[0] = 0.0f;
        yawOnly[1] = cg.refdefViewAngles[1];
        yawOnly[2] = 0.0f;
        AngleVectors(yawOnly, fwd, NULL, NULL);
    }

    if (!CG_WorldPointsToScreen((const vec3_t *)points, xs, ys, 8)) {
        CG_ResetTargetIdentifier();
        return;
    }

    qsort(xs, 8, sizeof(float), CG_qsortCmpFloatLess);
    qsort(ys, 8, sizeof(float), CG_qsortCmpFloatLess);

    cg_ident.xMax = xs[0];
    cg_ident.yMax = ys[0];
    cg_ident.xMin = xs[7];
    cg_ident.yMin = ys[7];

    if (DotProduct(dir, fwd) < 0.3f) {
        CG_ResetTargetIdentifier();
        return;
    }

    if (cg_ident.xMax < 0.0f && cg_ident.xMin < 0.0f) {
        return;
    }
    if (cg_ident.xMax > 640.0f && cg_ident.xMin > 640.0f) {
        return;
    }

    // cg+0xA3DAC is NOT write-only: it is colour component [3] of cg+0xA3DA0, so it is
    if (a3) {
        float reveal = 1.0f - (float)(cg.time - cg_ident.lastReveal) / 250.0f;

        if (reveal < 0.1f) {
            CG_ResetTargetIdentifier();
            return;
        }
        cg_ident.color[3] = reveal;
    } else {
        cg_ident.lastReveal = cg.time;
        cg_ident.color[3]  = 1.0f;
    }

    //      is deliberate retail data: characters need the tricorder, world objects do
    {
        char        itypeBuf[64];
        const char *itype = CG_EntityInteractiveTypeName(cent, itypeBuf, sizeof(itypeBuf));
        bool        scannerActive;
        bool        alwaysShowReticle;

        if (!itype[0]) {
            return;
        }

        CG_ClipIdentifierBoxToScreen();

        scannerActive = cg.snap && (cg.snap->ps.pm_flags & PMF_SCANNER) != 0;

        // as 1.0, so they always show.  That asymmetry is deliberate retail data.
        alwaysShowReticle = !cgi.GetGameplayFloatProperty
                            || cgi.GetGameplayFloatProperty(itype, "AlwaysShowReticle") != 0.0f;

        if (scannerActive || alwaysShowReticle) {
            cg_ident.draw      = 1;
            cg_ident.drawStamp = cg_ident.frame;
        } else {
            cg_ident.draw = 0;
        }
    }
}

static void CG_CreateDisplayInfo(const centity_t *cent)
{
    int   eFlags = cent->currentState.eFlags;
    float distSq;

    if (cg_ident.entnum != cent->currentState.number) {
        CG_ResetTargetIdentifier();
        cg_ident.entnum = cent->currentState.number;
    }

    if (cg.snap) {
        distSq = DistanceSquared(cent->lerpOrigin, cg.snap->ps.origin);
        if (distSq > 1440000.0f || distSq <= 0.0f) {
            CG_ResetTargetIdentifier();
            return;
        }
    }

    if (eFlags & EF_DISPLAY_DESC1) {
        if ((unsigned)cg_ident.descLevel < 2u) {
            cg_ident.descLevel = 1;
            CG_DisplayTargetIdentifier(cent, 0);
        } else {
            CG_DisplayTargetIdentifier(cent, 1);
            cg_ident.color[3] = 1.0f;
        }
    } else if ((eFlags & EF_DISPLAY_DESC2) && (eFlags & EF_DISPLAY_DESC3)) {
        if (cg_ident.descLevel != 3) {
            cg_ident.timer3 = cg.time;
        }
        cg_ident.descLevel = 3;
        CG_DisplayTargetIdentifier(cent, 0);
    } else if (eFlags & EF_DISPLAY_DESC2) {
        if (cg_ident.descLevel != 2) {
            cg_ident.timer2 = cg.time;
        }
        cg_ident.descLevel = 2;
        CG_DisplayTargetIdentifier(cent, 0);
    } else {
        if (!cg_ident.descLevel) {
            return;
        }
        CG_DisplayTargetIdentifier(cent, 1);
    }
}

void CG_UpdateTargetIdentifier(const centity_t *cent)
{
    if (!cent) {
        return;
    }

    if (cent->currentState.eFlags & EF_DISPLAY_INFO) {
        if (!cg_ident.identDisplay || cg_ident.entnum != cent->currentState.number) {
            char        itypeBuf[64];
            const char *itype = CG_EntityInteractiveTypeName(cent, itypeBuf, sizeof(itypeBuf));

            PROBE_CENSUS_EMIT("[CG:", "target",
                              "acquire f=%d ent=%d etype=%d desc=0x%x itype='%s'\n",
                              cg_ident.frame, cent->currentState.number,
                              cent->currentState.eType, cent->currentState.eFlags & 0x1E00, itype);
        }
        CG_CreateDisplayInfo(cent);
        cg_ident.identDisplay = 1;
        cg_ident.identStamp   = cg_ident.frame;
    } else if (cg_ident.entnum == cent->currentState.number) {
        CG_CreateDisplayInfo(cent);
    }
}

void CG_GetIdentificationData(const centity_t *cent, vec4_t out)
{
    float reveal = out[3];

    if (!cent) {
        out[0] = out[1] = out[2] = 1.0f;
    } else {
        char        itypeBuf[64];
        const char *itype = CG_EntityInteractiveTypeName(cent, itypeBuf, sizeof(itypeBuf));

        CG_InteractiveTypeColor(itype, out);
    }

    out[3] = reveal;
}

static void CG_QueueTargetBlock(const char *source, int startTime, float *pos)
{
    char *lines[12];
    char  buf[256];
    int   numLines = 0;
    float reveal;
    int   i;
    char *p;

    if (!source || !source[0]) {
        return;
    }

    Q_strncpyz(buf, source, sizeof(buf));

    lines[numLines++] = buf;
    for (p = buf; *p && numLines < 12; p++) {
        if (*p == '\n') {
            *p = 0;
            if (p[1]) {
                lines[numLines++] = p + 1;
            }
        }
    }

    reveal = (float)(cg.time - startTime) / 600.0f;
    if (reveal > 1.0f) {
        reveal = 1.0f;
    }

    for (i = 0; i < numLines; i++) {
        int len   = (int)strlen(lines[i]);
        int count = (int)(reveal * (float)len);

        if (count < 0 || count >= len) {
            count = len;
        }
        lines[i][count] = '\0';

        if (cgi.UI_AddPrintString) {
            cgi.UI_AddPrintString(lines[i], pos, 640, cg_ident.color, "serpentinelight-10");
        }

        pos[1] += 19.0f * CG_TARGET_TEXT_SCALE;
    }
}

void CG_DrawTargetIdentifier(void)
{
    const centity_t *cent;

    if (!cg_ident.draw) {
        return;
    }

    if (cg_ident.drawStamp != cg_ident.frame) {
        PROBE_EMIT_N("[CG:", "target", PROBE_ROW_CAP,
                     "stalebracket f=%d ent=%d box=(%.0f,%.0f)-(%.0f,%.0f) no ident update this"
                     " frame\n",
                     cg_ident.frame, cg_ident.entnum, cg_ident.xMin, cg_ident.yMin, cg_ident.xMax,
                     cg_ident.yMax);
    }

    cent = &cg_entities[cg_ident.entnum];


    cgi.R_SetColor(cg_ident.color);
    cgi.R_DrawStretchPic(
        cg_ident.xMin * cgs.screenXScale,
        cg_ident.yMin * cgs.screenYScale,
        (cg_ident.xMax - cg_ident.xMin) * cgs.screenXScale,
        (cg_ident.yMax - cg_ident.yMin) * cgs.screenYScale,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        cgs.media.identificationShader
    );

    // `startTime` is the TIER TIMER of the block, not a per-entity latch: retail's
    {
        char   built[256];
        float  pos[2];
        const char *source;

        /* the brackets are retail's own and stay as they are: only the label's box
           grows, so the label keeps its seat to their right and below their top */
        pos[0] = cg_ident.xMax + 10.0f * CG_TARGET_TEXT_SCALE;
        pos[1] = cg_ident.yMin + 10.0f * CG_TARGET_TEXT_SCALE;

        source = CG_CreateTargetString(cent, CG_TargetDescriptionProperty(cg_ident.descLevel), built, sizeof(built));
        CG_QueueTargetBlock(source, cg_ident.timer2, pos);

        if (cg_ident.descLevel == 3) {
            pos[1] += 19.0f * CG_TARGET_TEXT_SCALE;
            source = CG_CreateTargetString(cent, "Description3", built, sizeof(built));
            CG_QueueTargetBlock(source, cg_ident.timer3, pos);
        }
    }

    cgi.R_SetColor(NULL);
}


// retail CG_TargetInitialize @0x00058F40: both latches die at frame start and are
// re-set during the entity walk, so neither survives its own frame.
void CG_TargetInitialize(void)
{
    cg_ident.frame++;
    cg_ident.draw         = 0;
    cg_ident.identDisplay = 0;
}

void CG_Draw2D(void)
{
    CG_DrawLagometer();
    CG_DrawDisconnect();

    if (cg.snap && !(cg.snap->ps.stats[STAT_CINEMATIC] & 1)) {
        if (cg_ident.entnum >= 0) {
            CG_GetIdentificationData(&cg_entities[cg_ident.entnum], cg_ident.color);
            CG_DrawTargetIdentifier();
        }

        if (cg_crosshair->integer) {
            CG_DrawCrosshair();
        }
    }
}
