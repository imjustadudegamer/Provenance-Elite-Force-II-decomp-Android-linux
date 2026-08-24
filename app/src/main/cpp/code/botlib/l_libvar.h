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


typedef struct libvar_s
{
	char		*name;
	char		*string;
	int		flags;
	qboolean	modified;	
	float		value;
	struct	libvar_s *next;
} libvar_t;

void LibVarDeAllocAll(void);
libvar_t *LibVarGet(const char *var_name);
char *LibVarGetString(const char *var_name);
float LibVarGetValue(const char *var_name);
libvar_t *LibVar(const char *var_name, const char *value);
float LibVarValue(const char *var_name, const char *value);
char *LibVarString(const char *var_name, const char *value);
void LibVarSet(const char *var_name, const char *value);
qboolean LibVarChanged(const char *var_name);
void LibVarSetNotModified(const char *var_name);

