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
#ifndef GLES3_GLES3_FIRSTLIGHT_H
#define GLES3_GLES3_FIRSTLIGHT_H

#include "qgl_gles3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLES3_FL_CLEAR 0x1
#define GLES3_FL_QUAD  0x2
#define GLES3_FL_FACE  0x4

void GLES3_FirstLight_SetLogger(void (*fn)(const char *line));

unsigned GLES3_FirstLight_ClearAndQuad(void);

unsigned GLES3_FirstLight_DrawFace(const float *xyz, const float *st, int numVerts,
								  unsigned tex);

unsigned GLES3_FirstLight_UploadRGBA8(int w, int h, const unsigned char *pixels);
void GLES3_FirstLight_DeleteTexture(unsigned tex);

void GLES3_FirstLight_Shutdown(void);

int GLES3_FirstLight_Ready(void);

#ifdef __cplusplus
}
#endif

#endif 
