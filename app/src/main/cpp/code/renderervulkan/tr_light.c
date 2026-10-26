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
Copyright (C) 2015-2024 the OpenMoHAA team

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

/* the row cap only; the census gate is R_ProbeMarkOn, not the master switch */
#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

void R_TransformDlights(int count, dlight_t *dl, orientationr_t *ori)
{
    int    i;
    vec3_t temp;

    for (i = 0; i < count; i++, dl++) {
        VectorSubtract(dl->origin, ori->origin, temp);
        dl->transformed[0] = DotProduct(temp, ori->axis[0]);
        dl->transformed[1] = DotProduct(temp, ori->axis[1]);
        dl->transformed[2] = DotProduct(temp, ori->axis[2]);
    }
}

void R_DlightBmodel(bmodel_t *bmodel)
{
    int         i, j;
    dlight_t   *dl;
    int         mask;
    msurface_t *surf;

    R_TransformDlights(tr.refdef.num_dlights, tr.refdef.dlights, &tr.ori);

    mask = 0;
    for (i = 0; i < tr.refdef.num_dlights; i++) {
        dl = &tr.refdef.dlights[i];

        for (j = 0; j < 3; j++) {
            if (dl->transformed[j] - bmodel->bounds[1][j] > dl->radius) {
                break;
            }
            if (bmodel->bounds[0][j] - dl->transformed[j] > dl->radius) {
                break;
            }
        }
        if (j < 3) {
            continue;
        }

        mask |= 1 << i;
    }

    tr.currentEntity->needDlights = (mask != 0);

    for (i = 0; i < bmodel->numSurfaces; i++) {
        surf = bmodel->firstSurface + i;

        if (*surf->data == SF_FACE) {
            ((srfSurfaceFace_t *)surf->data)->dlightBits[tr.smpFrame] = mask;
        } else if (*surf->data == SF_GRID) {
            ((srfGridMesh_t *)surf->data)->dlightBits[tr.smpFrame] = mask;
        } else if (*surf->data == SF_TRIANGLES) {
            ((srfTriangles_t *)surf->data)->dlightBits[tr.smpFrame] = mask;
        }
    }
}


void R_GetLightingForDecal(vec3_t vLight, const vec3_t vFacing, const vec3_t vOrigin)
{
    R_GetLightingAtPoint(vOrigin, vLight, tr.refdef.lightGroupIntensity);

    if (!tr.overbrightShift) {
        return;
    }

    VectorScale(vLight, tr.overbrightMult, vLight);

    if (vLight[0] > 255.0 || vLight[1] > 255.0 || vLight[2] > 255.0) {
        float scale = 255.0 / Q_max(vLight[0], Q_max(vLight[1], vLight[2]));
        VectorScale(vLight, scale, vLight);
    }
}

void R_GetLightingForSmoke(vec3_t vLight, const vec3_t vOrigin)
{
    int       i;
    dlight_t *dl;
    float     power;
    vec3_t    dir;
    float     d;

    R_GetLightingAtPoint(vOrigin, vLight, tr.refdef.lightGroupIntensity);

    for (i = 0; i < backEnd.refdef.num_dlights; i++) {
        dl = &backEnd.refdef.dlights[i];
        VectorSubtract(dl->origin, vOrigin, dir);
        d = VectorLengthSquared(dir);

        power = dl->radius * dl->radius;
        if (power >= d) {
            d = dl->radius * 7500.0 / d;
            VectorMA(vLight, d, dl->color, vLight);
        }
    }

    if (tr.overbrightShift) {
        vLight[0] = tr.overbrightMult * vLight[0];
        vLight[1] = tr.overbrightMult * vLight[1];
        vLight[2] = tr.overbrightMult * vLight[2];
    }

    if (vLight[0] > 255.0 || vLight[1] > 255.0 || vLight[2] > 255.0) {
        float scale = 255.0 / Q_max(vLight[0], Q_max(vLight[1], vLight[2]));
        VectorScale(vLight, scale, vLight);
    }

    vLight[0] /= 255.0;
    vLight[1] /= 255.0;
    vLight[2] /= 255.0;
}


void RB_SetupStaticModelLighting(trRefdef_t *refdef, cStaticModelUnpacked_t *ent, const vec3_t lightOrigin)
{
    int       i;
    dlight_t *dl;
    float     power;
    vec3_t    vLight;
    vec3_t    dir;
    float     d;

    if (ent->bStaticLightingCalculated) {
        return;
    }
    ent->bStaticLightingCalculated = qtrue;

    if (!(refdef->rdflags & RDF_NOWORLDMODEL)) {
        R_GetLightingAtPoint(lightOrigin, vLight, backEnd.refdef.lightGroupIntensity);
    } else {
        vLight[0] = vLight[1] = vLight[2] = tr.identityLight * 150.0;
    }

    for (i = 0; i < refdef->num_dlights; i++) {
        dl = &refdef->dlights[i];
        VectorSubtract(dl->origin, lightOrigin, dir);
        d = VectorLengthSquared(dir);

        power = dl->radius * dl->radius;
        if (power >= d) {
            d = dl->radius * 7500.0 / d;
            VectorMA(vLight, d, dl->color, vLight);
        }
    }

    if (tr.overbrightShift) {
        vLight[0] = tr.overbrightMult * vLight[0];
        vLight[1] = tr.overbrightMult * vLight[1];
        vLight[2] = tr.overbrightMult * vLight[2];
    }

    if (vLight[0] > 255.0 || vLight[1] > 255.0 || vLight[2] > 255.0) {
        float scale = 255.0 / Q_max(vLight[0], Q_max(vLight[1], vLight[2]));
        VectorScale(vLight, scale, vLight);
    }

    for (i = 0; i < 3; i++) {
        if (vLight[i] > tr.identityLightByte) {
            vLight[i] = tr.identityLightByte;
        }
    }

    ((byte *)&ent->iStaticLighting)[0] = myftol(vLight[0]);
    ((byte *)&ent->iStaticLighting)[1] = myftol(vLight[1]);
    ((byte *)&ent->iStaticLighting)[2] = myftol(vLight[2]);
    ((byte *)&ent->iStaticLighting)[3] = 0xff;
}


extern cvar_t *r_ambientScale;
extern cvar_t *r_directedScale;

static void Q_UNUSED_VAR LogLight(trRefEntity_t *ent)
{
    int max1, max2;

    if (!(ent->e.renderfx & RF_FIRST_PERSON)) {
        return;
    }

    max1 = ent->ambientLight[0];
    if (ent->ambientLight[1] > max1) {
        max1 = ent->ambientLight[1];
    } else if (ent->ambientLight[2] > max1) {
        max1 = ent->ambientLight[2];
    }

    max2 = ent->directedLight[0];
    if (ent->directedLight[1] > max2) {
        max2 = ent->directedLight[1];
    } else if (ent->directedLight[2] > max2) {
        max2 = ent->directedLight[2];
    }

    ri.Printf(PRINT_ALL, "amb:%i  dir:%i\n", max1, max2);
}





int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir) {
    return 0;
}

/* retail R_SetupEntityLighting: the record every entity colour reads.  A world
   with no base-lighting surfs (every shipped EF2 map) takes the lit fallback --
   ambient = directed = identityLight * 255 -- plus the in-range sphere lights. */
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) {
    float   identity = tr.identityLight * 255.0f;
    float   max;
    int     i;

    if ( ( refdef->rdflags & RDF_NOWORLDMODEL )
            || !tr.world->numBaseLightingSurfs
            || ( ent->e.renderfx & RF_FULLBRIGHT ) ) {
        VectorSet( ent->ambientLight, identity, identity, identity );
        VectorSet( ent->directedLight, identity, identity, identity );
        VectorSet( ent->lightDir, 0.0f, 0.0f, 1.0f );
    }

    for ( i = 0; i < tr.numSLights; i++ ) {
        const spherel_t *pl = &tr.sLights[ i ];
        vec3_t           delta;
        float            dist, falloff;

        VectorSubtract( pl->origin, ent->e.origin, delta );
        dist = VectorLength( delta );
        if ( dist <= pl->falloff_end_dist ) {
            falloff = tr.identityLight * pl->falloff_end_dist / ( dist * dist );
            ent->directedLight[0] += pl->color[0] * falloff;
            ent->directedLight[1] += pl->color[1] * falloff;
            ent->directedLight[2] += pl->color[2] * falloff;
        }
    }

    max = ent->ambientLight[0];
    if ( ent->ambientLight[1] > max ) max = ent->ambientLight[1];
    if ( ent->ambientLight[2] > max ) max = ent->ambientLight[2];
    if ( max > 255.0f ) {
        VectorScale( ent->ambientLight, 255.0f / max, ent->ambientLight );
        VectorScale( ent->directedLight, 255.0f / max, ent->directedLight );
    }

    ( (byte *)&ent->ambientLightInt )[0] = (byte)Q_min( ent->ambientLight[0], 255.0f );
    ( (byte *)&ent->ambientLightInt )[1] = (byte)Q_min( ent->ambientLight[1], 255.0f );
    ( (byte *)&ent->ambientLightInt )[2] = (byte)Q_min( ent->ambientLight[2], 255.0f );
    ( (byte *)&ent->ambientLightInt )[3] = 0xFF;
}

/* One row per frame whose dlight feed moved on a light that did not: a stationary light going black or changing hue is a wrong slot, not the level. */
void RB_DLightFeedCensus( void )
{
    typedef struct { vec3_t origin; vec3_t color; float radius; } dlightKey_t;
    static dlightKey_t  prev[MAX_DLIGHTS];
    static int          prevNum;
    static int          rows;
    int                 i, j;

    if ( !R_ProbeMarkOn() ) {
        return;
    }

    for ( i = 0; i < backEnd.refdef.num_dlights && i < MAX_DLIGHTS; i++ ) {
        dlight_t    *dl = &backEnd.refdef.dlights[i];
        const char  *arm = NULL;

        for ( j = 0; j < prevNum && j < MAX_DLIGHTS; j++ ) {
            vec3_t delta;

            VectorSubtract( dl->origin, prev[j].origin, delta );
            if ( VectorLength( delta ) > 4.0f ) {
                continue;   /* a moved light is a different read, not a colour change */
            }
            if ( dl->color[0] < 0.02f && dl->color[1] < 0.02f && dl->color[2] < 0.02f
                 && prev[j].radius > 1.0f && dl->radius > 1.0f ) {
                arm = "black";
            } else if ( fabs( dl->color[0] - prev[j].color[0] ) > 0.25f
                     || fabs( dl->color[1] - prev[j].color[1] ) > 0.25f
                     || fabs( dl->color[2] - prev[j].color[2] ) > 0.25f ) {
                arm = "hue";
            }
            if ( arm && rows < PROBE_ROW_CAP ) {
                rows++;
                VK_Guard_Row( "DOCC:dlight",
                              "arm=%s slot=%d org=(%.1f,%.1f,%.1f) r=%.1f "
                              "was=%d,%d,%d now=%d,%d,%d f=%d",
                              arm, i, dl->origin[0], dl->origin[1], dl->origin[2],
                              dl->radius,
                              (int)( prev[j].color[0] * 255.0f ),
                              (int)( prev[j].color[1] * 255.0f ),
                              (int)( prev[j].color[2] * 255.0f ),
                              (int)( dl->color[0] * 255.0f ),
                              (int)( dl->color[1] * 255.0f ),
                              (int)( dl->color[2] * 255.0f ),
                              RB_FrameCount() );
            }
            break;
        }
    }

    prevNum = backEnd.refdef.num_dlights;
    for ( i = 0; i < prevNum && i < MAX_DLIGHTS; i++ ) {
        VectorCopy( backEnd.refdef.dlights[i].origin, prev[i].origin );
        VectorCopy( backEnd.refdef.dlights[i].color, prev[i].color );
        prev[i].radius = backEnd.refdef.dlights[i].radius;
    }
}
