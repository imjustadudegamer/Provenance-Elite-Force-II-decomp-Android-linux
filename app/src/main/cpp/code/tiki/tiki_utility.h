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


#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    const char *TIKI_Name(dtiki_t *pmdl);
    int         TIKI_NumAnims(dtiki_t *pmdl);
    int         TIKI_NumSurfaces(dtiki_t *pmdl);
    int         TIKI_NumTags(dtiki_t *pmdl);
    void        TIKI_CalculateBounds(dtiki_t *pmdl, float scale, vec3_t mins, vec3_t maxs);
    float       TIKI_GetFadeDistMod(dtiki_t *pmdl);
    qboolean    TIKI_Alias_Add(dtiki_t *pmdl, const char *alias, const char *name, const char *parameters);
    const char *TIKI_Alias_FindRandom(dtiki_t *pmdl, const char *alias);
    void        TIKI_Alias_Dump(dtiki_t *pmdl);
    void        TIKI_Alias_Clear(dtiki_t *pmdl);
    const char *TIKI_Alias_FindDialog(dtiki_t *pmdl, const char *alias, int random, int entity_number);
    const char *TIKI_Alias_FindSpecificAnim(dtiki_t *pmdl, const char *name);
    qboolean    TIKI_Alias_CheckLoopAnim(dtiki_t *pmdl, const char *name);
    float       TIKI_GlobalRadius(dtiki_t *pmdl);

#ifdef __cplusplus
}
#endif
