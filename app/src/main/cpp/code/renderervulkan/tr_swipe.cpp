/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
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

#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

#define MAX_SWIPE_POINTS 512
#define MAX_SWIPES       32

typedef struct {
    vec3_t points[2];
    float  time;
} rswipepoint_t;

typedef struct {
    surfaceType_t surftype;
    float         life;
    float         time;
    int           lastrendtime;
    qhandle_t     shader;
    int           numswipes;
    /* the smp slot this record was filled in; the walk's own copy of
       RB_FrameSmpFrame() has to name it, or the walk reads mid-refill geometry */
    int           smpframe;
    rswipepoint_t swipes[MAX_SWIPE_POINTS];
} rendswipe_t;

static int         lastswipeframe;
static int         numswipes;
/* One slot per smp frame: RE_SwipeBegin refills this list on the recorder while the
   render thread is still walking the previous frame, so a shared list is geometry
   the walk reads mid-refill. */
static rendswipe_t swipes[SMP_FRAMES][MAX_SWIPES];

static rendswipe_t *RE_SwipeSlot(void)
{
    return swipes[tr.smpFrame];
}

void RE_SwipeBegin(float thistime, float life, qhandle_t shader)
{
    rendswipe_t *swipe;
    rendswipe_t *slot = RE_SwipeSlot();

    if (tr.frameCount != lastswipeframe) {
        numswipes = 0;
    }

    if (numswipes >= MAX_SWIPES) {
        return;
    }

    swipe = &slot[numswipes++];

    swipe->life         = life;
    swipe->time         = thistime;
    swipe->surftype     = SF_SWIPE;
    swipe->numswipes    = 0;
    swipe->lastrendtime = 0;
    swipe->shader       = shader;
    swipe->smpframe     = tr.smpFrame;

    if (shader < 0 && life) {
        ri.Printf(PRINT_DEVELOPER, "RE_SwipeBegin: Invalid shader handle\n");
    }

    lastswipeframe = tr.frameCount;
}

void RE_SwipePoint(vec3_t point1, vec3_t point2, float time)
{
    rendswipe_t *swipe = &RE_SwipeSlot()[numswipes - 1];

    if (swipe->numswipes >= MAX_SWIPE_POINTS) {
        return;
    }

    VectorCopy(point1, swipe->swipes[swipe->numswipes].points[0]);
    VectorCopy(point2, swipe->swipes[swipe->numswipes].points[1]);
    swipe->swipes[swipe->numswipes].time = time;
    swipe->numswipes++;
}

void RE_SwipeEnd() {}

void R_AddSwipeSurfaces()
{
    shader_t *shader;
    int       at;

    if (!numswipes) {
        return;
    }

    tr.currentEntityNum = ENTITYNUM_WORLD;
    tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

    for (at = 0; at < numswipes; at++) {
        rendswipe_t *swipe     = &RE_SwipeSlot()[at];
        int          timedelta = 0;

        if (!swipe->numswipes) {
            continue;
        }

        if (swipe->lastrendtime) {
            timedelta = tr.refdef.time - swipe->lastrendtime;
        }
        swipe->lastrendtime = tr.refdef.time;

        if (timedelta) {
            swipe->time += timedelta;
        }

        shader = R_GetShaderByHandle(swipe->shader);
        R_AddDrawSurf(&swipe->surftype, shader, 0, 0);
    }
}

void RB_DrawSwipeSurface(surfaceType_t *pswipe)
{
    int          i;
    float        oolife;
    rendswipe_t *swipe = (rendswipe_t *)pswipe;

    /* The swipe list is the one surface payload the front end refills while the walk
       still holds it, so a record from another slot is the mid-refill geometry that
       draws as streaking lines.  Born gated, like every other instrument row. */
    if (PROBE_On() && vkConfig.smpActive && swipe->smpframe != RB_FrameSmpFrame()) {
        static int s_rows;

        if (s_rows < 8 || (s_rows % 256) == 0) {
            ri.Printf(PRINT_ALL,
                      "[VK:swipeslot] record slot %d walk slot %d points=%d f=%d\n",
                      swipe->smpframe, RB_FrameSmpFrame(), swipe->numswipes, RB_FrameCount());
        }
        s_rows++;
    }

    oolife = 1.0 / swipe->life;

    RB_CHECKOVERFLOW(swipe->numswipes * 2, swipe->numswipes * 6);

    RB_StreamBeginDrawSurf();

    for (i = 0; i < swipe->numswipes; i++) {
        rswipepoint_t *swipepoint = &swipe->swipes[i];
        float          alpha;
        float          f;

        f = 1.0 - (swipe->time - swipepoint->time) * oolife;
        if (f > 0 && swipe->time >= swipepoint->time) {
            alpha = f - Q_max(f - 1.0, 0);

            RB_Color4f(alpha, alpha, alpha, alpha);
            RB_Texcoord2f(i / (float)swipe->numswipes, 1.0);
            RB_Vertex3fv(swipepoint->points[0]);
            RB_Texcoord2f(i / (float)swipe->numswipes, 1.0);
            RB_Vertex3fv(swipepoint->points[1]);
        } else if (i == swipe->numswipes - 1) {
            swipe->numswipes = 0;
        }
    }

    RB_StreamEndDrawSurf();
}
