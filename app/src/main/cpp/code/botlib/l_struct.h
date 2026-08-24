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



#define MAX_STRINGFIELD				80
#define FT_CHAR						1			
#define FT_INT							2			
#define FT_FLOAT						3			
#define FT_STRING						4			
#define FT_STRUCT						6			
#define FT_TYPE						0x00FF	
#define FT_ARRAY						0x0100	
#define FT_BOUNDED					0x0200	
#define FT_UNSIGNED					0x0400

typedef struct fielddef_s
{
	char *name;										
	int offset;										
	int type;										
	int maxarray;									
	float floatmin, floatmax;					
	struct structdef_s *substruct;			
} fielddef_t;

typedef struct structdef_s
{
	int size;
	fielddef_t *fields;
} structdef_t;

int ReadStructure(source_t *source, structdef_t *def, char *structure);
int WriteStructure(FILE *fp, structdef_t *def, char *structure);
int WriteIndent(FILE *fp, int indent);
int WriteFloat(FILE *fp, float value);


