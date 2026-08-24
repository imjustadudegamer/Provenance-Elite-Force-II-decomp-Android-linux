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

//     adds code_base (0), NOT data_base (+0x63D50) -- so the pointer targets
// One retail store is NOT reproduced, because this port has no field for it: the

#include "cg_local.h"
#include "cg_commands.h"
#include "surfaceflags.h"


#define GROUND_DISTANCE        8
#define WATER_NO_SPLASH_HEIGHT 16



static spawnthing_t cg_splashSpawnthing;

void CG_Splash(centity_t *cent)
{
    vec3_t  start, end, diff;
    trace_t trace;
    int     contents;
    float   dist;
    float   time_required;
    float   fLife;

    spawnthing_t *m_ripple = &cg_splashSpawnthing;

    if (!cg_shadows->integer) {
        return;
    }

    if (cg.snap && cg.snap->ps.clientNum == cent->currentState.number
        && (cg.snap->ps.pm_type == PM_SPECTATOR || cg.snap->ps.pm_type == PM_SPECTATOR_FOLLOW)) {
        return;
    }

    VectorSubtract(cent->currentState.origin, cent->nextState.origin, diff);
    diff[2] = 0;
    dist    = VectorNormalize(diff);


    if (dist >= 1) {
        time_required = 100 - dist;
    } else {
        time_required = 200;
    }

    if (time_required < 10) {
        time_required = 10;
    }

    if (cent->splash_last_spawn_time + time_required > cg.time) {
        return;
    }


    cent->splash_last_spawn_time = cg.time;

    if (dist < 1) {
        if (cent->splash_still_count >= 0) {
            cent->splash_still_count++;

            if (cent->splash_still_count > 2) {
                cent->splash_still_count = 0;
            } else {
                return;
            }
        } else {
            return;
        }
    }

    VectorCopy(cent->lerpOrigin, end);

    contents = cgi.CM_PointContents(end, 0);
    if (!(contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))) {
        return;
    }

    VectorCopy(cent->lerpOrigin, start);
    start[2] += 72;

    contents = cgi.CM_PointContents(start, 0);
    if (contents & (CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        return;
    }

    cgi.CM_BoxTrace(
        &trace, start, end, vec3_origin, vec3_origin, 0, (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA), qfalse
    );

    if (trace.fraction == 1.0) {
        return;
    }


    commandManager.InitializeSpawnthing(m_ripple);

    m_ripple->m_modellist.ClearObjectList();
    m_ripple->m_modellist.AddObject("ripple.spr");

    m_ripple->cgd.origin = trace.endpos;

    m_ripple->cgd.scale = 0.33f + dist / 100.0f;
    if (m_ripple->cgd.scale > 1.0f) {
        m_ripple->cgd.scale = 1.0f;
    }

    m_ripple->cgd.scaleRate = 1.0f + dist / 250.0f;
    if (m_ripple->cgd.scaleRate > 1.4f) {
        m_ripple->cgd.scaleRate = 1.4f;
    }

    fLife = 2000.0f - 10.0f * dist;
    if (fLife < 1000.0f) {
        fLife = 1000.0f;
    }

    m_ripple->cgd.life    = (int)fLife;
    m_ripple->life_random = 0.0f;

    m_ripple->cgd.flags |= (T_FADE | T_ANGLES);  
    m_ripple->cgd.angles = Vector(90.0f, 0.0f, 0.0f);

    commandManager.SpawnTempModel(1, m_ripple);
}
