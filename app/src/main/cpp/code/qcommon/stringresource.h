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
#ifndef STRINGRESOURCE_H
#define STRINGRESOURCE_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void        initializeStringResource(void);
void        uninitializeStringResource(void);
void        loadGlobalStrings(void);
void        clearAllStrings(void);
void        loadLevelStrings(const char *levelName);
const char *getStringResource(const char *stringId);
qboolean    isStringResource(const char *text);
void        convertStringResourceIds(const char *text, char *out, int size);

#ifdef __cplusplus
}
#endif

#endif 
