/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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


#include "player.h"
#include "weaputils.h"

Q_UNUSED_VAR static Entity *FindClosestEntityInRadius(Vector origin, Vector forward, float fov, float maxdist)
{
    float    dist, dot;
    float    fovdot = cos(fov * 0.5 * M_PI / 180.0);
    Entity  *ent;
    Entity  *bestent  = NULL;
    int      bestdist = 999999;
    qboolean valid_entity;

    ent = findradius(NULL, origin, maxdist);

    while (ent) {
        valid_entity = false;

        if (ent->flags & FL_AUTOAIM) {
            valid_entity = true;
        }

        if (valid_entity) {
            Vector delta = (ent->centroid) - origin;

            dist = delta.length();

            if (dist < bestdist) {
                delta.normalize();

                dot = DotProduct(forward, delta);

                if (dot > fovdot) {
                    trace_t trace;
                    trace = G_Trace(
                        origin, vec_zero, vec_zero, ent->centroid, NULL, MASK_OPAQUE, false, "FindClosestEntityInRadius"
                    );

                    if ((trace.ent && trace.entityNum == ent->entnum) || (trace.fraction == 1)) {
                        bestent  = ent;
                        bestdist = dist;
                    }
                }
            }
        }
        ent = findradius(ent, origin, maxdist);
    }
    return bestent;
}



