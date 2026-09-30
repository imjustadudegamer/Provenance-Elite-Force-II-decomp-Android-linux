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
Copyright (C) 2024 the OpenMoHAA team

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
#include "../corepp/str.h"

static byte  cntColor[4];
static float cntSt[2];


void RB_StreamBegin(shader_t *shader)
{
    RB_BeginSurface(shader, 0);
}

void RB_StreamEnd(void)
{
    int i;

    if (tess.numVertexes <= 2) {
        RB_EndSurface();
        return;
    }

    tess.indexes[0] = 0;
    tess.indexes[1] = 1;
    tess.indexes[2] = 2;

    for (i = 0; i < tess.numVertexes - 2; i++) {
        tess.indexes[i * 3 + 0] = (i & 1) + i;
        tess.indexes[i * 3 + 1] = i - ((i & 1) - 1);
        tess.indexes[i * 3 + 2] = i + 2;
        tess.numIndexes += 3;
    }

    RB_EndSurface();
}

void RB_StreamBeginDrawSurf(void)
{
    backEnd.dsStreamVert = tess.numVertexes;
}

void RB_StreamEndDrawSurf(void)
{
    int numverts;
    int i;

    if (tess.numVertexes - backEnd.dsStreamVert <= 2) {
        tess.numVertexes = backEnd.dsStreamVert;
        return;
    }

    numverts = tess.numVertexes - backEnd.dsStreamVert - 2;
    for (i = 0; i < numverts; i++) {
        tess.indexes[i + tess.numIndexes]     = (i & 1) + i + backEnd.dsStreamVert;
        tess.indexes[i + tess.numIndexes + 1] = i + backEnd.dsStreamVert - ((i & 1) - 1);
        tess.indexes[i + tess.numIndexes + 2] = i + backEnd.dsStreamVert + 2;
        tess.numIndexes += 3;
    }
}

static void addTriangle(void)
{
    tess.texCoords[tess.numVertexes][0][0] = cntSt[0];
    tess.texCoords[tess.numVertexes][0][1] = cntSt[1];
    tess.vertexColors[tess.numVertexes][0] = cntColor[0];
    tess.vertexColors[tess.numVertexes][1] = cntColor[1];
    tess.vertexColors[tess.numVertexes][2] = cntColor[2];
    tess.vertexColors[tess.numVertexes][3] = cntColor[3];
    tess.vertexColorValid                  = qtrue;
    tess.numVertexes++;
}

/* THE 2D placement, in one place.  Retail keeps glViewport and glOrtho constant
   for the whole 2D pass and carries each widget's placement in its screen-space
   vertex stream (retail-menu-solomatch-final.trace calls 10103-10124: constant
   glViewport(0,0,640,480) + glOrtho(0,640,480,0), per-widget glScissor), so a
   port vertex is written in the widget's own coordinates and placed here.
   Every 2D writer -- the surface batch, the immediate stream and the font/glyph
   runs -- calls this at the LAST step, after any glyph or HUD scale: the
   placement is an offset, not a quantity the widget's own scale applies to.
   `backEnd.in2D` keeps the 3D pass untouched. */
void RB_2DPlaceVertex(float *xyz)
{
    if ( backEnd.in2D ) {
        xyz[0] += vk_2dOrigin[0];
        xyz[1] += vk_2dOrigin[1];
    }
}

void RB_2DPlaceRange(int first, int count)
{
    int i;

    for ( i = 0; i < count; i++ ) {
        RB_2DPlaceVertex(tess.xyz[first + i]);
    }
}

void RB_Vertex3fv(vec3_t v)
{
    int ndx = tess.numVertexes;

    VectorCopy(v, tess.xyz[ndx]);
    RB_2DPlaceVertex(tess.xyz[ndx]);
    addTriangle();
}

void RB_Vertex3f(vec_t x, vec_t y, vec_t z)
{
    int ndx = tess.numVertexes;

    tess.xyz[ndx][0] = x;
    tess.xyz[ndx][1] = y;
    tess.xyz[ndx][2] = z;
    RB_2DPlaceVertex(tess.xyz[ndx]);
    addTriangle();
}

void RB_Vertex2f(vec_t x, vec_t y)
{
    RB_Vertex3f(x, y, 0);
}

void RB_Color4f(vec_t r, vec_t g, vec_t b, vec_t a)
{
    cntColor[0] = r * tr.identityLightByte;
    cntColor[1] = g * tr.identityLightByte;
    cntColor[2] = b * tr.identityLightByte;
    cntColor[3] = a * 255.0;
}

void RB_Color3f(vec_t r, vec_t g, vec_t b)
{
    RB_Color4f(r, g, b, 1.0);
}

void RB_Color3fv(vec3_t col)
{
    RB_Color3f(col[0], col[1], col[2]);
}

void RB_Color4bv(unsigned char *colors)
{
    cntColor[0] = colors[0];
    cntColor[1] = colors[1];
    cntColor[2] = colors[2];
    cntColor[3] = colors[3];
}

void RB_Texcoord2f(float s, float t)
{
    cntSt[0] = s;
    cntSt[1] = t;
}

void RB_Texcoord2fv(vec2_t st)
{
    cntSt[0] = st[0];
    cntSt[1] = st[1];
}

int RE_GetShaderWidth(qhandle_t hShader)
{
    shader_t *shader;

    if (hShader) {
        shader = R_GetShaderByHandle(hShader);
    } else {
        shader = tr.defaultShader;
    }

    return shader->unfoggedStages[0]->bundle[0].image[0]->uploadWidth;
}

int RE_GetShaderHeight(qhandle_t hShader)
{
    shader_t *shader;

    if (hShader) {
        shader = R_GetShaderByHandle(hShader);
    } else {
        shader = tr.defaultShader;
    }

    return shader->unfoggedStages[0]->bundle[0].image[0]->uploadHeight;
}

const char *RE_GetShaderName(qhandle_t hShader)
{
    shader_t *shader;

    if (hShader) {
        shader = R_GetShaderByHandle(hShader);
    } else {
        shader = tr.defaultShader;
    }

    return shader->name;
}
