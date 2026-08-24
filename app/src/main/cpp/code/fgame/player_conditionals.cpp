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


#include "player.h"
#include "weaputils.h"


#define SLOPE_45_MIN         0.7071f
#define SLOPE_45_MAX         0.831f
#define SLOPE_22_MIN         SLOPE_45_MAX
#define SLOPE_22_MAX         0.95f

#define MIN_Z                -999999
#define PUSH_OBJECT_DISTANCE 16.0f

#define ARMS_NAME            "Bip01 Spine2"

static Vector min_box_8x8(-4, -4, -4);
static Vector max_box_8x8(4, 4, 4);
static Vector min4x4(-4, -4, 0);
static Vector max4x4x0(4, 4, 0);
static Vector max4x4x8(4, 4, 8);














































































































































CLASS_DECLARATION(Class, Conditional, NULL) {
    {NULL, NULL}
};
