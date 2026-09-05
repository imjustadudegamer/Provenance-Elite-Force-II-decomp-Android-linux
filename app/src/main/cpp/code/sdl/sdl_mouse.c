/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

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

#ifdef USE_INTERNAL_SDL_HEADERS
#    include "SDL.h"
#else
#    include <SDL.h>
#endif

#include "../qcommon/qcommon.h"

static SDL_Cursor *cursor = NULL;
static SDL_Surface *cursor_surface = NULL;

void IN_GetMousePosition(int *x, int *y) {
    SDL_GetMouseState(x, y);
}

void IN_FreeCursor(void) {
    if (cursor) {
        SDL_FreeCursor(cursor);
    }
    if (cursor_surface) {
        SDL_FreeSurface(cursor_surface);
    }
}

qboolean IN_IsCursorActive(void)
{
    return SDL_GetRelativeMouseMode() == SDL_TRUE;
}

/* Touch and gamepad input reach the UI on Android, but neither is a pointer, so
   neither may raise one.  Only a real mouse event sets this, and it stays set
   once one has arrived. */
#ifndef __ANDROID__
static qboolean in_realmouse = qfalse;
#endif

void IN_NoteRealMouse(void)
{
#ifndef __ANDROID__
    in_realmouse = qtrue;
#endif
}

qboolean IN_HasRealMouse(void)
{
#ifdef __ANDROID__
    /* Android ships no pointer: no event and no state may raise one, so the
       answer is a constant and every cursor consumer is covered by it. */
    return qfalse;
#else
    return qtrue;
#endif
}
