/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <stdio.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

void CON_Shutdown( void )
{
}

void CON_Init( void )
{
}

char *CON_Input( void )
{
	return NULL;
}

void CON_Print( const char *msg )
{
	/* stderr is the diag capture's sink (stdout.txt/stderr.txt); the mirror to
	   logcat is the routine channel and stays silent unless the operator armed
	   it.  Crash and fatal rows reach logcat through Sys_AndroidLog directly,
	   not through here. */
#ifdef __ANDROID__
	if( Sys_AndroidDiagOn() )
	{
		fputs( msg, stderr );
		Sys_AndroidLog( ANDROID_LOG_INFO, msg );
	}
#else
	fputs( msg, stderr );
#endif
}
