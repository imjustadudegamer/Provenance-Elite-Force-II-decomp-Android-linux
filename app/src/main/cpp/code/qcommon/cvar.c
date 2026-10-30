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

#include "q_shared.h"
#include "qcommon.h"

#define PROBE_GET(name, def, flags) Cvar_FindVar(name)
#define PROBE_PRINT                 Com_Printf
#include "qa_probe.h"

cvar_t		*cvar_vars = NULL;
cvar_t		*cvar_cheats;
int			cvar_modifiedFlags;

#define	MAX_CVARS	2048
cvar_t		cvar_indexes[MAX_CVARS];
int			cvar_numIndexes;

/* MAX_CVARS below caps the live population at 2048, so a 256-bucket table
   held about eight per chain and Cvar_FindVar paid a Q_stricmp per entry on
   every lookup -- and the UI resolves cvars per widget per frame.  result-
   identical: same-named cvars hash to one bucket and keep insertion order
   within it, so the first match cannot change. */
#define FILE_HASH_SIZE		4096
static	cvar_t	*hashTable[FILE_HASH_SIZE];

qboolean cvar_global_force = qfalse;
qboolean cvar_inConfig = qfalse;
/* same right for the port's own defaults and the per-boot override slot: both are config-grade,
   so their rows outrank a pak `setd' and a later code registration */
qboolean cvar_inSeed = qfalse;

static void Cvar_FlagsCheck(int flags);

static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

static qboolean Cvar_ValidateString( const char *s ) {
	if ( !s ) {
		return qfalse;
	}
	if ( strchr( s, '\\' ) ) {
		return qfalse;
	}
	if ( strchr( s, '\"' ) ) {
		return qfalse;
	}
	if ( strchr( s, ';' ) ) {
		return qfalse;
	}
	return qtrue;
}

cvar_t *Cvar_FindVar( const char *var_name ) {
	cvar_t	*var;
	long hash;

	hash = generateHashValue(var_name);
	
	for (var=hashTable[hash] ; var ; var=var->hashNext) {
		if (!Q_stricmp(var_name, var->name)) {
			return var;
		}
	}

	return NULL;
}

float Cvar_VariableValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->value;
}


int Cvar_VariableIntegerValue( const char *var_name ) {
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->integer;
}


char *Cvar_VariableString( const char *var_name ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var) {
		*buffer = 0;
	}
	else {
		Q_strncpyz( buffer, var->string, bufsize );
	}
}

int Cvar_Flags(const char *var_name)
{
	cvar_t *var;
	
	if(!(var = Cvar_FindVar(var_name)))
		return CVAR_NONEXISTENT;
	else
	{
		if(var->modified)
			return var->flags | CVAR_MODIFIED;
		else
			return var->flags;
	}
}

void Cvar_CommandCompletion(void (*callback)(const char *s))
{
	cvar_t		*cvar;
	
	for(cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if(cvar->name)
			callback(cvar->name);
	}
}

static const char *Cvar_Validate( cvar_t *var,
    const char *value, qboolean warn )
{
	static char s[ MAX_CVAR_VALUE_STRING ];
	float valuef;
	qboolean changed = qfalse;

	if( !var->validate )
		return value;

	if( !value )
		return value;

	if( Q_isanumber( value ) )
	{
		valuef = atof( value );

		if( var->integral )
		{
			if( !Q_isintegral( valuef ) )
			{
				if( warn )
					Com_Printf( "WARNING: cvar '%s' must be integral", var->name );

				valuef = (int)valuef;
				changed = qtrue;
			}
		}
	}
	else
	{
		if( warn )
			Com_Printf( "WARNING: cvar '%s' must be numeric", var->name );

		valuef = atof( var->resetString );
		changed = qtrue;
	}

	if( valuef < var->min )
	{
		if( warn )
		{
			if( changed )
				Com_Printf( " and is" );
			else
				Com_Printf( "WARNING: cvar '%s'", var->name );

			if( Q_isintegral( var->min ) )
				Com_Printf( " out of range (min %d)", (int)var->min );
			else
				Com_Printf( " out of range (min %f)", var->min );
		}

		valuef = var->min;
		changed = qtrue;
	}
	else if( valuef > var->max )
	{
		if( warn )
		{
			if( changed )
				Com_Printf( " and is" );
			else
				Com_Printf( "WARNING: cvar '%s'", var->name );

			if( Q_isintegral( var->max ) )
				Com_Printf( " out of range (max %d)", (int)var->max );
			else
				Com_Printf( " out of range (max %f)", var->max );
		}

		valuef = var->max;
		changed = qtrue;
	}

	if( changed )
	{
		if( Q_isintegral( valuef ) )
		{
			Com_sprintf( s, sizeof( s ), "%d", (int)valuef );

			if( warn )
				Com_Printf( ", setting to %d\n", (int)valuef );
		}
		else
		{
			Com_sprintf( s, sizeof( s ), "%f", valuef );

			if( warn )
				Com_Printf( ", setting to %f\n", valuef );
		}

		return s;
	}
	else
		return value;
}


const char *Cvar_CompleteVariable( const char *partial )
{
	cvar_t *cvar;
	size_t len;

	len = strlen( partial );
	if( !len ) {
		return NULL;
	}

	for( cvar = cvar_vars; cvar != NULL; cvar = cvar->next )
	{
		if( !Q_stricmp( partial, cvar->name ) ) {
			return cvar->name;
		}
	}

	for( cvar = cvar_vars; cvar != NULL; cvar = cvar->next )
	{
		if( !Q_stricmpn( partial, cvar->name, len ) ) {
			return cvar->name;
		}
	}

	return NULL;
}

const char *Cvar_CompleteVariableByNumber( const char *partial, int number )
{
	cvar_t *cvar;
	size_t len;

	len = strlen( partial );
	if( !len ) {
		return NULL;
	}

	for( cvar = cvar_vars; cvar != NULL; cvar = cvar->next )
	{
		if( !Q_stricmpn( partial, cvar->name, len ) )
		{
			if( !number ) {
				return cvar->name;
			}
			number--;
		}
	}

	return NULL;
}

void Cvar_SetDefault( cvar_t *var, const char *var_value )
{
	if( strcmp( var->resetString, var_value ) ) {
		var->flags |= CVAR_USER_CREATED;
		Z_Free( var->resetString );
		var->resetString = CopyString( var_value );
	}

	/* a setd that lands on the default the cvar already carries still authored
	   that default: the renderer's own registration has to see the marker or a
	   fresh install keeps the pak's value and never opens on the code one */
	var->defaultRecorded = qtrue;
}

cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags ) {
	cvar_t	*var;
	long	hash;
	int	index;

	if ( !var_name || ! var_value ) {
		Com_Error( ERR_FATAL, "Cvar_Get: NULL parameter" );
	}

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0		
	if ( !Cvar_ValidateString( var_value ) ) {
		Com_Printf("invalid cvar value string: %s\n", var_value );
		var_value = "BADVALUE";
	}
#endif

	Cvar_FlagsCheck(flags);

	var = Cvar_FindVar (var_name);
	
	if(var)
	{
		var_value = Cvar_Validate(var, var_value, qfalse);

		if(var->flags & CVAR_VM_CREATED)
		{
			if(!(flags & CVAR_VM_CREATED))
				var->flags &= ~CVAR_VM_CREATED;
		}
		else if (!(var->flags & CVAR_USER_CREATED))
		{
			if(flags & CVAR_VM_CREATED)
				flags &= ~CVAR_VM_CREATED;
		}

		if ( ((flags & CVAR_SERVER_CREATED)
		      || ((var->flags & CVAR_USER_CREATED) && !(flags & CVAR_USER_CREATED)))
		     && var_value[0] && !var->defaultRecorded )
		{
			var->flags &= ~CVAR_USER_CREATED;
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );

			cvar_modifiedFlags |= var->flags | flags;
		}

		var->flags |= flags;

		if ( !var->resetString[0] ) {
			Z_Free( var->resetString );
			var->resetString = CopyString( var_value );
		}
		if ( var->latchedString ) {
			char *s;

			s = var->latchedString;
			var->latchedString = NULL;	
			Cvar_Set2( var_name, s, qtrue );
			Z_Free( s );
		}

		return var;
	}


	for(index = 0; index < MAX_CVARS; index++)
	{
		if(!cvar_indexes[index].name)
			break;
	}

	if(index >= MAX_CVARS)
	{
		if(!com_errorEntered)
			Com_Error(ERR_FATAL, "Error: Too many cvars, cannot create a new one!");

		return NULL;
	}
	
	var = &cvar_indexes[index];
	
	if(index >= cvar_numIndexes)
		cvar_numIndexes = index + 1;
		
	var->name = CopyString (var_name);
	var->string = CopyString (var_value);
	var->modified = qtrue;
	var->modificationCount = 1;
	var->value = atof (var->string);
	var->integer = atoi(var->string);
	var->resetString = CopyString( var_value );
	var->validate = qfalse;
	var->description = NULL;
	var->defaultRecorded = qfalse;	 

	var->next = cvar_vars;
	if(cvar_vars)
		cvar_vars->prev = var;

	var->prev = NULL;
	cvar_vars = var;

	var->flags = flags;
	cvar_modifiedFlags |= var->flags;

	hash = generateHashValue(var_name);
	var->hashIndex = hash;

	var->hashNext = hashTable[hash];
	if(hashTable[hash])
		hashTable[hash]->hashPrev = var;

	var->hashPrev = NULL;
	hashTable[hash] = var;

	return var;
}

void Cvar_Print( cvar_t *v ) {
	Com_Printf ("\"%s\" is:\"%s" S_COLOR_WHITE "\"",
			v->name, v->string );

	if ( !( v->flags & CVAR_ROM ) ) {
		if ( !Q_stricmp( v->string, v->resetString ) ) {
			Com_Printf (", the default" );
		} else {
			Com_Printf (" default:\"%s" S_COLOR_WHITE "\"",
					v->resetString );
		}
	}

	Com_Printf ("\n");

	if ( v->latchedString ) {
		Com_Printf( "latched: \"%s\"\n", v->latchedString );
	}

	if ( v->description ) {
		Com_Printf( "%s\n", v->description );
	}
}

cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force ) {
	cvar_t	*var;

	if( developer && developer->integer == 2 ) {
		Com_DPrintf( "Cvar_Set2: %s %s\n", var_name, value );
	}

	if ( !Cvar_ValidateString( var_name ) ) {
		Com_Printf("invalid cvar name string: %s\n", var_name );
		var_name = "BADNAME";
	}

#if 0	
	if ( value && !Cvar_ValidateString( value ) ) {
		Com_Printf("invalid cvar value string: %s\n", value );
		var_value = "BADVALUE";
	}
#endif

	var = Cvar_FindVar (var_name);
	if (!var) {
		if ( !value ) {
			return NULL;
		}
		if ( !force && !cvar_global_force ) {
			return Cvar_Get( var_name, value, CVAR_USER_CREATED );
		} else {
			return Cvar_Get (var_name, value, 0);
		}
	}

	/* a value out of config.cfg is the player's own, not a default: a pak `setd' must
	   not outrank it and neither may a later code registration */
	if ( cvar_inConfig || cvar_inSeed ) {
		var->defaultRecorded = qfalse;
	}

	if (!value ) {
		value = var->resetString;
	}

	/* the dlight hunt: the one cvar's writes, named at the choke point */
	if ( PROBE_On() && !strcmp( var_name, "r_dynamiclight" ) ) {
		PROBE_EMIT( "[UIVK:", "dlight",
			    "site=cvar_set2 old='%s' new='%s' force=%i from=%p",
			    var->string, value, force, __builtin_return_address(0) );
	}

	value = Cvar_Validate(var, value, qtrue);

	if((var->flags & CVAR_LATCH) && var->latchedString)
	{
		if(!strcmp(value, var->string))
		{
			Z_Free(var->latchedString);
			var->latchedString = NULL;
			return var;
		}

		if(!strcmp(value, var->latchedString))
			return var;
	}
	else if(!strcmp(value, var->string))
		return var;

	cvar_modifiedFlags |= var->flags;

	if (!force)
	{
		if (var->flags & CVAR_ROM)
		{
			Com_Printf ("%s is read only.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_INIT)
		{
			Com_Printf ("%s is write protected.\n", var_name);
			return var;
		}

		if ((var->flags & CVAR_CHEAT) && !cvar_cheats->integer)
		{
			Com_Printf ("%s is cheat protected.\n", var_name);
			return var;
		}
		
		if (var->flags & CVAR_LATCH)
		{
			if (var->latchedString)
			{
				if (strcmp(value, var->latchedString) == 0)
					return var;
				Z_Free (var->latchedString);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			Com_Printf ("%s will be changed upon restarting.\n", var_name);
			var->latchedString = CopyString(value);
			var->modified = qtrue;
			var->modificationCount++;
			return var;
		}
	}
	else
	{
		if (var->latchedString)
		{
			Z_Free (var->latchedString);
			var->latchedString = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		

	var->modified = qtrue;
	var->modificationCount++;
	
	Z_Free (var->string);	
	
	var->string = CopyString(value);
	var->value = atof (var->string);
	var->integer = atoi (var->string);

	return var;
}

void Cvar_Set( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qtrue);
}

void Cvar_SetSafe( const char *var_name, const char *value )
{
	int flags = Cvar_Flags( var_name );

	if((flags != CVAR_NONEXISTENT) && (flags & CVAR_PROTECTED))
	{
		if( value )
			Com_Error( ERR_DROP, "Restricted source tried to set "
				"\"%s\" to \"%s\"", var_name, value );
		else
			Com_Error( ERR_DROP, "Restricted source tried to "
				"modify \"%s\"", var_name );
		return;
	}
	Cvar_Set( var_name, value );
}

void Cvar_SetLatched( const char *var_name, const char *value) {
	Cvar_Set2 (var_name, value, qfalse);
}

void Cvar_SetValue( const char *var_name, float value) {
	char	val[32];

	if ( value == (int)value ) {
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	} else {
		Com_sprintf (val, sizeof(val), "%f",value);
	}
	Cvar_Set (var_name, val);
}

void Cvar_Reset( const char *var_name ) {
	Cvar_Set2( var_name, NULL, qfalse );
}

void Cvar_ForceReset(const char *var_name)
{
	Cvar_Set2(var_name, NULL, qtrue);
}

void Cvar_SetCheatState(void)
{
	cvar_t	*var;

	for(var = cvar_vars; var ; var = var->next)
	{
		if(var->flags & CVAR_CHEAT)
		{
			if (var->latchedString)
			{
				Z_Free(var->latchedString);
				var->latchedString = NULL;
			}
			if (strcmp(var->resetString,var->string))
				Cvar_Set(var->name, var->resetString);
		}
	}
}

qboolean Cvar_Command( void ) {
	cvar_t	*v;

	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v) {
		return qfalse;
	}

	if ( Cmd_Argc() == 1 ) {
		Cvar_Print( v );
		return qtrue;
	}

	Cvar_Set2 (v->name, Cmd_Args(), qfalse);
	return qtrue;
}


void Cvar_Toggle_f( void ) {
	int		i, c = Cmd_Argc();
	char		*curval;

	if(c < 2) {
		Com_Printf("usage: toggle <variable> [value1, value2, ...]\n");
		return;
	}

	if(c == 2) {
		Cvar_Set2(Cmd_Argv(1), va("%d", 
			!Cvar_VariableValue(Cmd_Argv(1))), 
			qfalse);
		return;
	}

	if(c == 3) {
		Com_Printf("toggle: nothing to toggle to\n");
		return;
	}

	curval = Cvar_VariableString(Cmd_Argv(1));

	for(i = 2; i + 1 < c; i++) {
		if(strcmp(curval, Cmd_Argv(i)) == 0) {
			Cvar_Set2(Cmd_Argv(1), Cmd_Argv(i + 1), qfalse);
			return;
		}
	}

	Cvar_Set2(Cmd_Argv(1), Cmd_Argv(2), qfalse);
}

void Cvar_Set_f( void ) {
	int		argc, last, i, len, total;
	char	buffer[264];

	argc = Cmd_Argc();
	if ( argc < 3 ) {
		Com_Printf ("usage: set <variable> <value>\n");
		return;
	}

	last = argc - 1;
	buffer[0] = '\0';
	total = 0;
	for ( i = 2; i < argc; i++ ) {
		len = strlen( Cmd_Argv( i ) + 1 );
		if ( total + len >= 254 ) {
			break;
		}
		strcat( buffer, Cmd_Argv( i ) );
		if ( i != last ) {
			strcat( buffer, " " );
		}
		total += len;
	}

	Cvar_Set2( Cmd_Argv( 1 ), buffer, qfalse );
}

void Cvar_SetU_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: setu <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_USERINFO;
}

void Cvar_SetH_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: seth <variable> <value>\n");
		return;
	}

	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v || atof( v->string ) < atof( Cmd_Argv( 2 ) ) ) {
		Cvar_Set_f();
	}
}

void Cvar_SetL_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: setl <variable> <value>\n");
		return;
	}

	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v || atof( Cmd_Argv( 2 ) ) < atof( v->string ) ) {
		Cvar_Set_f();
	}
}

void Cvar_SetS_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: sets <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_SERVERINFO;
}

void Cvar_SetD_f( void ) {
	cvar_t	*var;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: setd <variable> <value>\n");
		return;
	}

	Cvar_Set_f();

	var = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( var ) {
		Cvar_SetDefault( var, Cmd_Argv( 2 ) );
	}
}

void Cvar_Reset_f( void ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ) );
}

void Cvar_Add_f( void ) {
	int c;
	float current;
	float amount;
	char val[32];

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: add <variable> <amount>\n");
		return;
	}

	amount = atof(Cmd_Argv(2));
	current = Cvar_VariableValue(Cmd_Argv(1)) + amount;
	c = (int)current;
	if (current == c) {
		Com_sprintf(val, 32, "%i", c);
	} else {
		Com_sprintf(val, 32, "%f", current);
	}

	Cvar_Set2(Cmd_Argv(1), val, qfalse);
}

void Cvar_Subtract_f( void ) {
	int c;
	float current;
	float amount;
	char val[32];

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: subtract <variable> <amount>\n");
		return;
	}

	amount = atof(Cmd_Argv(2));
	current = Cvar_VariableValue(Cmd_Argv(1)) - amount;
	c = (int)current;
	if (current == c) {
		Com_sprintf(val, 32, "%i", c);
	} else {
		Com_sprintf(val, 32, "%f", current);
	}

	Cvar_Set2(Cmd_Argv(1), val, qfalse);
}

void Cvar_Scale_f( void ) {
	int c;
	float current;
	float amount;
	char val[32];

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: scale <variable> <amount>\n");
		return;
	}

	amount = atof(Cmd_Argv(2));
	current = Cvar_VariableValue(Cmd_Argv(1)) * amount;
	c = (int)current;
	if (current == c) {
		Com_sprintf(val, 32, "%i", c);
	} else {
		Com_sprintf(val, 32, "%f", current);
	}

	Cvar_Set2(Cmd_Argv(1), val, qfalse);
}

void Cvar_Append_f( void ) {
	char buffer[1024];

	if ( Cmd_Argc() < 3 ) {
		Com_Printf ("usage: append <variable> <string>\n");
		return;
	}

	Com_sprintf(buffer, sizeof(buffer), "%s %s", Cvar_VariableString(Cmd_Argv(1)), Cmd_Argv(2));
	Cvar_Set2(Cmd_Argv(1), buffer, qfalse);
}

void Cvar_SetA_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: seta <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	/* a row out of config.cfg cannot manufacture an archive right: only the
	   registration owns that, so a stale row dies with its own next write */
	if ( !cvar_inConfig ) {
		v->flags |= CVAR_ARCHIVE;
	}
}

/* `set' plus CVAR_NOARCHIVE: the value holds for this process only and is not
   folded into the archive at shutdown. */
void Cvar_SetX_f( void ) {
	cvar_t	*v;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf ("usage: setx <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( !v ) {
		return;
	}
	v->flags |= CVAR_NOARCHIVE;
}

void Cvar_WriteVariables(fileHandle_t f)
{
	cvar_t	*var;
	char	buffer[1024];

	for (var = cvar_vars; var; var = var->next)
	{
		if(!var->name || Q_stricmp( var->name, "cl_cdkey" ) == 0)
			continue;

		/* a `setx' value belongs to this boot, not to the player's config */
		if( var->flags & CVAR_NOARCHIVE )
			continue;

		if( var->flags & CVAR_ARCHIVE ) {
			if ( var->latchedString ) {
				if( strlen( var->name ) + strlen( var->latchedString ) + 10 > sizeof( buffer ) ) {
					Com_Printf( S_COLOR_YELLOW "WARNING: value of variable "
							"\"%s\" too long to write to file\n", var->name );
					continue;
				}
				Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->latchedString);
			} else {
				if( strlen( var->name ) + strlen( var->string ) + 10 > sizeof( buffer ) ) {
					Com_Printf( S_COLOR_YELLOW "WARNING: value of variable "
							"\"%s\" too long to write to file\n", var->name );
					continue;
				}
				Com_sprintf (buffer, sizeof(buffer), "seta %s \"%s\"\n", var->name, var->string);
			}
			FS_Write( buffer, strlen( buffer ), f );
		}
	}
}

void Cvar_List_f( void ) {
	cvar_t	*var;
	int		i;
	char	*match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	i = 0;
	for (var = cvar_vars ; var ; var = var->next, i++)
	{
		if(!var->name || (match && !Com_Filter(match, var->name, qfalse)))
			continue;

		if (var->flags & CVAR_SERVERINFO) {
			Com_Printf("S");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_SYSTEMINFO) {
			Com_Printf("s");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USERINFO) {
			Com_Printf("U");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ROM) {
			Com_Printf("R");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_INIT) {
			Com_Printf("I");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_ARCHIVE) {
			Com_Printf("A");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_LATCH) {
			Com_Printf("L");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_CHEAT) {
			Com_Printf("C");
		} else {
			Com_Printf(" ");
		}
		if (var->flags & CVAR_USER_CREATED) {
			Com_Printf("?");
		} else {
			Com_Printf(" ");
		}

		Com_Printf (" %s \"%s\"\n", var->name, var->string);
	}

	Com_Printf ("\n%i total cvars\n", i);
	Com_Printf ("%i cvar indexes\n", cvar_numIndexes);
}


cvar_t *Cvar_Unset(cvar_t *cv)
{
	cvar_t *next = cv->next;

	cvar_modifiedFlags |= cv->flags;

	if(cv->name)
		Z_Free(cv->name);
	if(cv->string)
		Z_Free(cv->string);
	if(cv->latchedString)
		Z_Free(cv->latchedString);
	if(cv->resetString)
		Z_Free(cv->resetString);
	if(cv->description)
		Z_Free(cv->description);

	if(cv->prev)
		cv->prev->next = cv->next;
	else
		cvar_vars = cv->next;
	if(cv->next)
		cv->next->prev = cv->prev;

	if(cv->hashPrev)
		cv->hashPrev->hashNext = cv->hashNext;
	else
		hashTable[cv->hashIndex] = cv->hashNext;
	if(cv->hashNext)
		cv->hashNext->hashPrev = cv->hashPrev;

	Com_Memset(cv, '\0', sizeof(*cv));
	
	return next;
}


void Cvar_Restart(qboolean unsetVM)
{
	cvar_t	*curvar;

	curvar = cvar_vars;

	while(curvar)
	{
		if((curvar->flags & CVAR_USER_CREATED) ||
			(unsetVM && (curvar->flags & CVAR_VM_CREATED)))
		{
			curvar = Cvar_Unset(curvar);
			continue;
		}
		
		if(!(curvar->flags & (CVAR_ROM | CVAR_INIT | CVAR_NORESTART)))
		{
			Cvar_Set2(curvar->name, curvar->resetString, qfalse);
		}
		
		curvar = curvar->next;
	}
}


void Cvar_Restart_f(void)
{
	Cvar_Restart(qfalse);
}

char *Cvar_InfoString(int bit)
{
	static char	info[MAX_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->name && (var->flags & bit) && (var->flags & CVAR_ROM))
			Info_SetValueForKey(info, var->name, var->string);
	}

	for(var = cvar_vars; var; var = var->next)
	{
		if (var->name && (var->flags & bit) && !(var->flags & CVAR_ROM))
			Info_SetValueForKey (info, var->name, var->string);
	}

	return info;
}

char *Cvar_InfoString_Big(int bit)
{
	static char	info[BIG_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->name && (var->flags & bit) && (var->flags & CVAR_ROM))
			Info_SetValueForKey(info, var->name, var->string);
	}

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->name && (var->flags & bit) && !(var->flags & CVAR_ROM))
			Info_SetValueForKey_Big (info, var->name, var->string);
	}
	return info;
}



void Cvar_InfoStringBuffer( int bit, char* buff, int buffsize ) {
	Q_strncpyz(buff,Cvar_InfoString(bit),buffsize);
}

void Cvar_CheckRange( cvar_t *var, float min, float max, qboolean integral )
{
	var->validate = qtrue;
	var->min = min;
	var->max = max;
	var->integral = integral;

	Cvar_Set( var->name, var->string );
}

void Cvar_SetDescription( cvar_t *var, const char *var_description )
{
	if( var_description && var_description[0] != '\0' )
	{
		if( var->description != NULL )
		{
			Z_Free( var->description );
		}
		var->description = CopyString( var_description );
	}
}

void Cvar_Register(vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags)
{
	cvar_t	*cv;

	if ((flags & (CVAR_ARCHIVE | CVAR_ROM)) == (CVAR_ARCHIVE | CVAR_ROM)) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: Unsetting CVAR_ROM from cvar '%s', "
			"since it is also CVAR_ARCHIVE\n", varName );
		flags &= ~CVAR_ROM;
	}

	if ( flags & CVAR_USER_CREATED ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to set CVAR_USER_CREATED on cvar '%s'\n", varName );
		flags &= ~CVAR_USER_CREATED;
	}
	if ( flags & CVAR_SERVER_CREATED ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to set CVAR_SERVER_CREATED on cvar '%s'\n", varName );
		flags &= ~CVAR_SERVER_CREATED;
	}
	if ( flags & CVAR_PROTECTED ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to set CVAR_PROTECTED on cvar '%s'\n", varName );
		flags &= ~CVAR_PROTECTED;
	}
	if ( flags & CVAR_MODIFIED ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to set CVAR_MODIFIED on cvar '%s'\n", varName );
		flags &= ~CVAR_MODIFIED;
	}
	if ( flags & CVAR_NONEXISTENT ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to set CVAR_NONEXISTENT on cvar '%s'\n", varName );
		flags &= ~CVAR_NONEXISTENT;
	}

	cv = Cvar_FindVar(varName);

	if ( cv && ( cv->flags & CVAR_PROTECTED ) ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: VM tried to register protected cvar '%s' with value '%s'%s\n",
			varName, defaultValue, ( flags & ~cv->flags ) != 0 ? " and new flags" : "" );
	} else {
		cv = Cvar_Get(varName, defaultValue, flags | CVAR_VM_CREATED);
	}

	if (!vmCvar)
		return;

	vmCvar->handle = cv - cvar_indexes;
	vmCvar->modificationCount = -1;
	Cvar_Update( vmCvar );
}


void	Cvar_Update( vmCvar_t *vmCvar ) {
	cvar_t	*cv = NULL;
	assert(vmCvar);

	if ( (unsigned)vmCvar->handle >= cvar_numIndexes ) {
		Com_Error( ERR_DROP, "Cvar_Update: handle out of range" );
	}

	cv = cvar_indexes + vmCvar->handle;

	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	if ( !cv->string ) {
		return;		
	}
	vmCvar->modificationCount = cv->modificationCount;
	if ( strlen(cv->string)+1 > MAX_CVAR_VALUE_STRING ) 
	  Com_Error( ERR_DROP, "Cvar_Update: src %s length %u exceeds MAX_CVAR_VALUE_STRING",
		     cv->string, 
		     (unsigned int) strlen(cv->string));
	Q_strncpyz( vmCvar->string, cv->string,  MAX_CVAR_VALUE_STRING ); 

	vmCvar->value = cv->value;
	vmCvar->integer = cv->integer;
}


void Cvar_Init (void)
{
	Com_Memset(cvar_indexes, '\0', sizeof(cvar_indexes));
	Com_Memset(hashTable, '\0', sizeof(hashTable));

	cvar_cheats = Cvar_Get("cheats", "0", CVAR_LATCH | CVAR_SYSTEMINFO );

	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("sets", Cvar_SetS_f);
	Cmd_AddCommand ("setu", Cvar_SetU_f);
	Cmd_AddCommand ("seta", Cvar_SetA_f);
	Cmd_AddCommand ("setx", Cvar_SetX_f);
	Cmd_AddCommand ("seth", Cvar_SetH_f);
	Cmd_AddCommand ("setl", Cvar_SetL_f);
	Cmd_AddCommand ("reset", Cvar_Reset_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("cvar_restart", Cvar_Restart_f);
	Cmd_AddCommand ("add", Cvar_Add_f);
	Cmd_AddCommand ("subtract", Cvar_Subtract_f);
	Cmd_AddCommand ("scale", Cvar_Scale_f);
	Cmd_AddCommand ("append", Cvar_Append_f);
}

#ifndef STANDALONE
#include "../server/server.h"
#endif

static void Cvar_FlagsCheck(int flags) {
    if ((flags & CVAR_ARCHIVE) && (flags & CVAR_SAVEGAME)) {
        Com_Error(ERR_FATAL, "Cvar_Get: cvar flagged with both CVAR_ARCHIVE and CVAR_SAVEGAME");
    }
}

cvar_t *Cvar_Next( cvar_t *var ) {
	if( var ) {
		return var->next;
	} else {
		return cvar_vars;
	}
}

void Cvar_SaveGameRestart_f( void ) {
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;
	while( 1 ) {
		var = *prev;
		if( !var ) {
			break;
		}

		if( !( var->flags & CVAR_SAVEGAME ) ) {
			prev = &var->next;
			continue;
		}

		if( var->flags & ( CVAR_ROM | CVAR_INIT | CVAR_NORESTART ) ) {
			prev = &var->next;
			continue;
		}

		Cvar_Set( var->name, var->resetString );

		prev = &var->next;
	}

#ifndef STANDALONE
	SV_ClearSvsTimeFixups();
#endif
}

float Cvar_VariableFloatValue(const char* var_name)
{
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return var->value;
}
