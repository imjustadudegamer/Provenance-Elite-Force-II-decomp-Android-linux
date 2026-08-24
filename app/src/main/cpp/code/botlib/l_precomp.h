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


#ifndef PATH_SEPERATORSTR
	#if defined(WIN32)|defined(_WIN32)|defined(__NT__)|defined(__WINDOWS__)|defined(__WINDOWS_386__)
		#define PATHSEPERATOR_STR		"\\"
	#else
		#define PATHSEPERATOR_STR		"/"
	#endif
#endif
#ifndef PATH_SEPERATORCHAR
	#if defined(WIN32)|defined(_WIN32)|defined(__NT__)|defined(__WINDOWS__)|defined(__WINDOWS_386__)
		#define PATHSEPERATOR_CHAR		'\\'
	#else
		#define PATHSEPERATOR_CHAR		'/'
	#endif
#endif

#if defined(BSPC) && !defined(QDECL)
#define QDECL
#endif


#define DEFINE_FIXED			0x0001

#define BUILTIN_LINE			1
#define BUILTIN_FILE			2
#define BUILTIN_DATE			3
#define BUILTIN_TIME			4
#define BUILTIN_STDC			5

#define INDENT_IF				0x0001
#define INDENT_ELSE				0x0002
#define INDENT_ELIF				0x0004
#define INDENT_IFDEF			0x0008
#define INDENT_IFNDEF			0x0010

typedef struct define_s
{
	char *name;							
	int flags;							
	int builtin;						
	int numparms;						
	token_t *parms;						
	token_t *tokens;					
	struct define_s *next;				
	struct define_s *hashnext;			
} define_t;

typedef struct indent_s
{
	int type;								
	int skip;								
	script_t *script;						
	struct indent_s *next;					
} indent_t;

typedef struct source_s
{
	char filename[1024];					
	char includepath[1024];					
	punctuation_t *punctuations;			
	script_t *scriptstack;					
	token_t *tokens;						
	define_t *defines;						
	define_t **definehash;					
	indent_t *indentstack;					
	int skip;								
	token_t token;							
} source_t;


int PC_ReadToken(source_t *source, token_t *token);
int PC_ExpectTokenString(source_t *source, char *string);
int PC_ExpectTokenType(source_t *source, int type, int subtype, token_t *token);
int PC_ExpectAnyToken(source_t *source, token_t *token);
int PC_CheckTokenString(source_t *source, char *string);
int PC_CheckTokenType(source_t *source, int type, int subtype, token_t *token);
int PC_SkipUntilString(source_t *source, char *string);
void PC_UnreadLastToken(source_t *source);
void PC_UnreadToken(source_t *source, token_t *token);
int PC_ReadLine(source_t *source, token_t *token);
int PC_WhiteSpaceBeforeToken(token_t *token);
int PC_AddDefine(source_t *source, char *string);
int PC_AddGlobalDefine(char *string);
int PC_RemoveGlobalDefine(char *name);
void PC_RemoveAllGlobalDefines(void);
void PC_AddBuiltinDefines(source_t *source);
void PC_SetIncludePath(source_t *source, char *path);
void PC_SetPunctuations(source_t *source, punctuation_t *p);
void PC_SetBaseFolder(char *path);
source_t *LoadSourceFile(const char *filename);
source_t *LoadSourceMemory(char *ptr, int length, char *name);
void FreeSource(source_t *source);
void QDECL SourceError(source_t *source, char *str, ...) Q_PRINTF_FUNC(2, 3);
void QDECL SourceWarning(source_t *source, char *str, ...)  Q_PRINTF_FUNC(2, 3);

#ifdef BSPC
#ifndef __Q_SHARED_H
#define MAX_TOKENLENGTH		1024
typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
} pc_token_t;
#endif 
#endif 

int PC_LoadSourceHandle(const char *filename);
int PC_FreeSourceHandle(int handle);
int PC_ReadTokenHandle(int handle, pc_token_t *pc_token);
int PC_SourceFileAndLine(int handle, char *filename, int *line);
void PC_CheckOpenSourceHandles(void);
