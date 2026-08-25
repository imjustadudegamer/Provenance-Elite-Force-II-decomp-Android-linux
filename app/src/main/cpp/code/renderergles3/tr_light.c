
#include "tr_local.h"

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
    R_GetLightingAtPoint(vOrigin, vLight);

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

    R_GetLightingAtPoint(vOrigin, vLight);

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
        R_GetLightingAtPoint(lightOrigin, vLight);
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
