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

typedef enum {
    selectsound,
    rejectsound,
    changesound
} invsound_t;

struct refEntity_s;

void CL_Draw3DModel(
    float             x,
    float             y,
    float             w,
    float             h,
    qhandle_t         model,
    vec3_t            origin,
    vec3_t            rotateoffset,
    vec3_t            offset,
    vec3_t            angle,
    const str        &anim,
    int               a11,
    struct refEntity_s *a12,
    float             fov_x,
    float             fov_y
);

qboolean UI_CloseInventory(void);

