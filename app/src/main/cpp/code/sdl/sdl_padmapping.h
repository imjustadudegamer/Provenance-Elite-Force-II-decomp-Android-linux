/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
Quake III Arena source code; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __SDL_PADMAPPING_H__
#define __SDL_PADMAPPING_H__

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL_gamecontroller.h"
#else
#	include <SDL_gamecontroller.h>
#endif

#include <stdio.h>
#include <string.h>

/* SDL's Android driver numbers the joystick buttons with the controller-button enum itself (SDL_sysjoystick.c keycode_to_SDL), so a missing element takes its enum index; b20+ are the API-12 BUTTON_1..16 keycodes. */
typedef struct
{
	const char *elem;
	int         index;
} sdlPadElem_t;

static const sdlPadElem_t sdl_padButtons[] = {
	{ "a",             SDL_CONTROLLER_BUTTON_A },
	{ "b",             SDL_CONTROLLER_BUTTON_B },
	{ "x",             SDL_CONTROLLER_BUTTON_X },
	{ "y",             SDL_CONTROLLER_BUTTON_Y },
	{ "back",          SDL_CONTROLLER_BUTTON_BACK },
	{ "guide",         SDL_CONTROLLER_BUTTON_GUIDE },
	{ "start",         SDL_CONTROLLER_BUTTON_START },
	{ "leftstick",     SDL_CONTROLLER_BUTTON_LEFTSTICK },
	{ "rightstick",    SDL_CONTROLLER_BUTTON_RIGHTSTICK },
	{ "leftshoulder",  SDL_CONTROLLER_BUTTON_LEFTSHOULDER },
	{ "rightshoulder", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER },
	{ "dpup",          SDL_CONTROLLER_BUTTON_DPAD_UP },
	{ "dpdown",        SDL_CONTROLLER_BUTTON_DPAD_DOWN },
	{ "dpleft",        SDL_CONTROLLER_BUTTON_DPAD_LEFT },
	{ "dpright",       SDL_CONTROLLER_BUTTON_DPAD_RIGHT }
};

static const sdlPadElem_t sdl_padAxes[] = {
	{ "leftx",        SDL_CONTROLLER_AXIS_LEFTX },
	{ "lefty",        SDL_CONTROLLER_AXIS_LEFTY },
	{ "rightx",       SDL_CONTROLLER_AXIS_RIGHTX },
	{ "righty",       SDL_CONTROLLER_AXIS_RIGHTY },
	{ "lefttrigger",  SDL_CONTROLLER_AXIS_TRIGGERLEFT },
	{ "righttrigger", SDL_CONTROLLER_AXIS_TRIGGERRIGHT }
};

static int sdl_padHasElem( const char *map, const char *elem )
{
	char token[16];

	snprintf( token, sizeof( token ), ",%s:", elem );
	return strstr( map, token ) != NULL;
}

static int sdl_padIndexUsed( const char *map, int index )
{
	char token[12];

	snprintf( token, sizeof( token ), ":b%d,", index );
	return strstr( map, token ) != NULL;
}

/* Appends every standard element the mapping is missing; 1 completed, 0 nothing missing, -1 does not fit. */
static int sdl_padMappingComplete( const char *map, char *out, size_t outsize )
{
	char   add[320];
	size_t len = 0, maplen, at, i;
	char  *plat;

	if ( !map || !out || outsize < 2 )
		return -1;

	add[0] = '\0';

	for ( i = 0; i < sizeof( sdl_padButtons ) / sizeof( sdl_padButtons[0] ); i++ )
	{
		char elem[24];
		int  n;

		if ( sdl_padHasElem( map, sdl_padButtons[i].elem ) || sdl_padIndexUsed( map, sdl_padButtons[i].index ) )
			continue;

		n = snprintf( elem, sizeof( elem ), ",%s:b%d", sdl_padButtons[i].elem, sdl_padButtons[i].index );
		if ( n <= 0 || (size_t)n >= sizeof( elem ) || len + (size_t)n >= sizeof( add ) )
			return -1;

		memcpy( add + len, elem, (size_t)n + 1 );
		len += (size_t)n;
	}

	for ( i = 0; i < sizeof( sdl_padAxes ) / sizeof( sdl_padAxes[0] ); i++ )
	{
		char elem[24];
		int  n;

		if ( sdl_padHasElem( map, sdl_padAxes[i].elem ) )
			continue;

		n = snprintf( elem, sizeof( elem ), ",%s:a%d", sdl_padAxes[i].elem, sdl_padAxes[i].index );
		if ( n <= 0 || (size_t)n >= sizeof( elem ) || len + (size_t)n >= sizeof( add ) )
			return -1;

		memcpy( add + len, elem, (size_t)n + 1 );
		len += (size_t)n;
	}

	if ( !len )
		return 0;

	maplen = strlen( map );
	if ( maplen + 1 > outsize || maplen + len + 1 > outsize )
		return -1;

	memcpy( out, map, maplen + 1 );

	/* New elements go before the platform field's own leading comma, which stays last. */
	plat = strstr( out, "platform:" );
	at = plat ? (size_t)( plat - out ) : maplen;

	if ( at > 0 && out[at - 1] == ',' )
		at--;

	memmove( out + at + len, out + at, maplen - at + 1 );
	memcpy( out + at, add, len );

	return 1;
}

#endif
