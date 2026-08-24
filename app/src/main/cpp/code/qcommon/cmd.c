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

#define	MAX_CMD_BUFFER  128*1024
#define	MAX_CMD_LINE	8192 
#define MAX_ALIAS_NAME	32
#define MAX_ALIAS_COUNT	16

typedef struct {
	byte	*data;
	size_t	maxsize;
	size_t	cursize;
} cmd_t;

typedef struct cmdalias_s {
	struct cmdalias_s *next;
	char name[ MAX_ALIAS_NAME ];
	const char *value;
} cmdalias_t;

int			cmd_wait;
cmd_t		cmd_text;
byte		cmd_text_buf[MAX_CMD_BUFFER];
static		cmdalias_t *cmd_alias;
int			alias_count;



void Cmd_Wait_f( void ) {
	if ( Cmd_Argc() == 2 ) {
		cmd_wait = atoi( Cmd_Argv( 1 ) );
		if ( cmd_wait < 0 )
			cmd_wait = 1; 
	} else {
		cmd_wait = 1;
	}
}



void Cbuf_Init (void)
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = MAX_CMD_BUFFER;
	cmd_text.cursize = 0;
}

void Cbuf_AddText( const char *text ) {
	size_t l;
	
	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	Com_Memcpy(&cmd_text.data[cmd_text.cursize], text, l);
	cmd_text.cursize += l;
}


void Cbuf_InsertText( const char *text ) {
	size_t	len;
	intptr_t	i;

	len = strlen( text ) + 1;
	if ( len + cmd_text.cursize > cmd_text.maxsize ) {
		Com_Printf( "Cbuf_InsertText overflowed\n" );
		return;
	}

	for ( i = cmd_text.cursize - 1 ; i >= 0 ; i-- ) {
		cmd_text.data[ i + len ] = cmd_text.data[ i ];
	}

	Com_Memcpy( cmd_text.data, text, len - 1 );

	cmd_text.data[ len - 1 ] = '\n';

	cmd_text.cursize += len;
}


void Cbuf_ExecuteText (int exec_when, const char *text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		if (text && strlen(text) > 0) {
			Cmd_ExecuteString (text);
		} else {
			Cbuf_Execute(0);
		}
		break;
	case EXEC_INSERT:
		Cbuf_InsertText (text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText (text);
		break;
	default:
		Com_Error (ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

void Cbuf_Execute (int msec)
{
	int		i;
	char	*text;
	char	line[MAX_CMD_LINE];
	int		quotes;

	alias_count = 0;
	
	qboolean in_star_comment = qfalse;
	qboolean in_slash_comment = qfalse;
	while (cmd_text.cursize)
	{
		if ( cmd_wait )	{
			if( cmd_wait == 1 ) {
				cmd_wait = 0;
				return;
			}
			cmd_wait -= msec;
			if ( cmd_wait > 0 )
				return;
			cmd_wait = 0;
		}

		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;

			if ( !(quotes&1)) {
				if (i < cmd_text.cursize - 1) {
					if (! in_star_comment && text[i] == '/' && text[i+1] == '/')
						in_slash_comment = qtrue;
					else if (! in_slash_comment && text[i] == '/' && text[i+1] == '*')
						in_star_comment = qtrue;
					else if (in_star_comment && text[i] == '*' && text[i+1] == '/') {
						in_star_comment = qfalse;
						i++;
						break;
					}
				}
				if (! in_slash_comment && ! in_star_comment && text[i] == ';')
					break;
			}
			if (! in_star_comment && (text[i] == '\n' || text[i] == '\r')) {
				in_slash_comment = qfalse;
				break;
			}
		}

		if( i >= (MAX_CMD_LINE - 1)) {
			i = MAX_CMD_LINE - 1;
		}
				
		Com_Memcpy (line, text, i);
		line[i] = 0;
		

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove (text, text+i, cmd_text.cursize);
		}


		Cmd_ExecuteString (line);		
	}
}




void Cmd_Exec_f( void ) {
	union {
		char	*c;
		void	*v;
	} f;
	char	filename[MAX_QPATH];

	if (Cmd_Argc () != 2) {
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
	FS_ReadFile( filename, &f.v);
	if (!f.c) {
		Com_Printf ("couldn't exec %s\n", filename);
		return;
	}
	Com_Printf ("execing %s\n", filename);

	Cbuf_InsertText (f.c);

	FS_FreeFile (f.v);
}


void Cmd_Vstr_f( void ) {
	char	*v;

	if (Cmd_Argc () != 2) {
		Com_Printf ("vstr <variablename> : execute a variable command\n");
		return;
	}

	v = Cvar_VariableString( Cmd_Argv( 1 ) );
	Cbuf_InsertText( va("%s\n", v ) );
}


void Cmd_Echo_f (void)
{
	Com_Printf ("%s\n", Cmd_Args());
}

void Cmd_Alias( const char *s, const char *cmd )
{
	cmdalias_t *a;

	if( strlen( s ) >= MAX_ALIAS_NAME ) {
		Com_WPrintf( "Alias name is too long\n" );
		return;
	}

	for( a = cmd_alias; a != NULL; a = a->next )
	{
		if( !strcmp( a->name, s ) ) {
			Z_Free( ( void * )a->value );
			break;
		}
	}

	if( !a )
	{
		a = Z_TagMalloc( sizeof( cmdalias_t ), TAG_STRINGS_AND_COMMANDS );
		a->next = cmd_alias;
		cmd_alias = a;
	}

	strcpy( a->name, s );
	a->value = CopyString( cmd );

	cvar_modifiedFlags |= CVAR_ARCHIVE;
}

void Cmd_Alias_f( void )
{
	cmdalias_t *a;
	int i, c;
	const char *s;
	char cmd[ 1024 ];

	if( Cmd_Argc() == 1 ) {
		Com_Printf( "Current alias commands:\n" );
		for( a = cmd_alias; a != NULL; a = a->next ) {
			Com_Printf( "%s : %s\n", a->name, a->value );
		}
		return;
	}

	s = Cmd_Argv( 1 );

	if( strlen( s ) >= MAX_ALIAS_NAME ) {
		Com_WPrintf( "Alias name is too long\n" );
		return;
	}

	for( a = cmd_alias; a != NULL; a = a->next )
	{
		if( !strcmp( a->name, s ) ) {
			Z_Free( ( void * )a->value );
			break;
		}
	}

	if( !a )
	{
		a = Z_TagMalloc( sizeof( cmdalias_t ), TAG_STRINGS_AND_COMMANDS );
		a->next = cmd_alias;
		cmd_alias = a;
	}

	strcpy( a->name, s );

	cmd[ 0 ] = 0;

	c = Cmd_Argc();

	for( i = 2; i < c; i++ )
	{
		s = Cmd_Argv( i );
		strcat( cmd, s );
		if( i != c - 1 ) {
			strcat( cmd, " " );
		}
	}

	strcat( cmd, "\n" );
	a->value = CopyString( cmd );

	cvar_modifiedFlags |= CVAR_ARCHIVE;
}

void Cmd_WriteAliases( fileHandle_t f )
{
	char tempbuf[ 1024 ];
	char buffer[ 1024 ];
	cmdalias_t *a;

	for( a = cmd_alias; a != NULL; a = a->next )
	{
		strcpy( tempbuf, a->value );

		tempbuf[ strlen( tempbuf ) - 1 ] = 0;
		Com_sprintf( buffer, sizeof( buffer ), "alias %s \"%s\"\n", a->name, tempbuf );

		FS_Printf( f, "%s", buffer );
	}
}


typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	const char				*name;
	xcommand_t				function;
} cmd_function_t;


static	int			cmd_argc;
static	char		*cmd_argv[MAX_STRING_TOKENS];		
static	char		cmd_tokenized[BIG_INFO_STRING+MAX_STRING_TOKENS];	
static	char		cmd_cmd[BIG_INFO_STRING]; 

static	cmd_function_t	*cmd_functions;		

int		Cmd_Argc( void ) {
	return cmd_argc;
}

char	*Cmd_Argv( int arg ) {
	if ( (unsigned)arg >= cmd_argc ) {
		return "";
	}
	return cmd_argv[arg];	
}

void	Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength ) {
	Q_strncpyz( buffer, Cmd_Argv( arg ), bufferLength );
}


char	*Cmd_Args( void ) {
	static	char		cmd_args[MAX_STRING_CHARS];
	int		i;

	cmd_args[0] = 0;
	for ( i = 1 ; i < cmd_argc ; i++ ) {
		strcat( cmd_args, cmd_argv[i] );
		// Retail Cmd_Args @0x000C6FA0 compares against cmd_argc, not cmd_argc-1
		if ( i != cmd_argc ) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}

char *Cmd_ArgsFrom( int arg ) {
	static	char		cmd_args[BIG_INFO_STRING];
	int		i;

	cmd_args[0] = 0;
	if (arg < 0)
		arg = 0;
	for ( i = arg ; i < cmd_argc ; i++ ) {
		strcat( cmd_args, cmd_argv[i] );
		if ( i != cmd_argc-1 ) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}

void	Cmd_ArgsBuffer( char *buffer, int bufferLength ) {
	Q_strncpyz( buffer, Cmd_Args(), bufferLength );
}

char *Cmd_Cmd(void)
{
	return cmd_cmd;
}


void Cmd_Args_Sanitize(void)
{
	int i;

	for(i = 1; i < cmd_argc; i++)
	{
		char *c = cmd_argv[i];
		
		if(strlen(c) > MAX_CVAR_VALUE_STRING - 1)
			c[MAX_CVAR_VALUE_STRING - 1] = '\0';
		
		while ((c = strpbrk(c, "\n\r"))) {
			*c = ' ';
			++c;
		}
	}
}

static void Cmd_TokenizeString2( const char *text_in, qboolean ignoreQuotes ) {
	const char	*text;
	char	*textOut;

#ifdef TKN_DBG
  Com_DPrintf("Cmd_TokenizeString: %s\n", text_in);
#endif

	cmd_argc = 0;

	if ( !text_in ) {
		return;
	}
	
	Q_strncpyz( cmd_cmd, text_in, sizeof(cmd_cmd) );

	text = text_in;
	textOut = cmd_tokenized;

	while ( 1 ) {
		if ( cmd_argc == MAX_STRING_TOKENS ) {
			return;			
		}

		while ( 1 ) {
			while ( *text && ( unsigned char )*text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				return;			
			}

			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		
				}
				text += 2;
			} else {
				break;			
			}
		}

		if ( !ignoreQuotes && *text == '"' ) {
			cmd_argv[cmd_argc] = textOut;
			cmd_argc++;
			text++;
			while ( *text && *text != '"' ) {
				*textOut++ = *text++;
			}
			*textOut++ = 0;
			if ( !*text ) {
				return;		
			}
			text++;
			continue;
		}

		cmd_argv[cmd_argc] = textOut;
		cmd_argc++;

		while ( ( unsigned char )*text > ' ' ) {
			if ( !ignoreQuotes && text[0] == '"' ) {
				break;
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			if ( text[0] == '/' && text[1] =='*' ) {
				break;
			}

			*textOut++ = *text++;
		}

		*textOut++ = 0;

		if ( !*text ) {
			return;		
		}
	}
	
}

void Cmd_TokenizeString( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qfalse );
}

void Cmd_TokenizeStringIgnoreQuotes( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qtrue );
}

cmd_function_t *Cmd_FindCommand( const char *cmd_name )
{
	cmd_function_t *cmd;
	for( cmd = cmd_functions; cmd; cmd = cmd->next )
		if( !Q_stricmp( cmd_name, cmd->name ) )
			return cmd;
	return NULL;
}

void	Cmd_AddCommand( const char *cmd_name, xcommand_t function ) {
	cmd_function_t	*cmd;
	
	if( Cmd_FindCommand( cmd_name ) )
	{
		if( function != NULL )
			Com_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
		return;
	}

	cmd = Z_TagMalloc( sizeof( cmd_function_t ), TAG_STRINGS_AND_COMMANDS );
	cmd->name = CopyString( cmd_name );
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

void	Cmd_RemoveCommand( const char *cmd_name ) {
	cmd_function_t	*cmd, **back;

	back = &cmd_functions;
	while( 1 ) {
		cmd = *back;
		if ( !cmd ) {
			return;
		}
		if ( !strcmp( cmd_name, cmd->name ) ) {
			*back = cmd->next;
			if (cmd->name) {
				Z_Free( ( void * )cmd->name );
			}
			Z_Free (cmd);
			return;
		}
		back = &cmd->next;
	}
}

void Cmd_RemoveCommandSafe( const char *cmd_name )
{
	cmd_function_t *cmd = Cmd_FindCommand( cmd_name );

	if( !cmd )
		return;
	if( cmd->function )
	{
		Com_Error( ERR_DROP, "Restricted source tried to remove "
			"system command \"%s\"", cmd_name );
		return;
	}

	Cmd_RemoveCommand( cmd_name );
}

void	Cmd_CommandCompletion( void(*callback)(const char *s) ) {
	cmd_function_t	*cmd;
	
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next) {
		callback( cmd->name );
	}
}


const char *Cmd_CompleteCommand( const char *partial )
{
	size_t len;
	cmd_function_t *cmd;
	cmdalias_t *a;

	len = strlen( partial );
	if( !len ) {
		return NULL;
	}

	for( cmd = cmd_functions; cmd != NULL; cmd = cmd->next )
	{
		if( !Q_stricmp( partial, cmd->name ) ) {
			return cmd->name;
		}
	}

	for( cmd = cmd_functions; cmd != NULL; cmd = cmd->next )
	{
		if( !Q_stricmpn( partial, cmd->name, len ) ) {
			return cmd->name;
		}
	}

	for( a = cmd_alias; a != NULL; a = a->next )
	{
		if( !strncmp( partial, a->name, len ) ) {
			return a->name;
		}
	}

	return NULL;
}

const char *Cmd_CompleteCommandByNumber( const char *partial, int number )
{
	size_t len;
	cmd_function_t *cmd;
	cmdalias_t *a;

	len = strlen( partial );
	if( !len ) {
		return NULL;
	}

	for( cmd = cmd_functions; cmd != NULL; cmd = cmd->next )
	{
		if( !strncmp( partial, cmd->name, len ) )
		{
			if( !number ) {
				return cmd->name;
			}
			number--;
		}
	}

	for( a = cmd_alias; a != NULL; a = a->next )
	{
		if( !strncmp( partial, a->name, len ) )
		{
			if( !number ) {
				return a->name;
			}
			number--;
		}
	}

	return NULL;
}

void	Cmd_ExecuteString( const char *text ) {	
	cmd_function_t	*cmd, **prev;
	cmdalias_t		*a;

	Cmd_TokenizeString( text );		
	if ( !Cmd_Argc() ) {
		return;		
	}

	for ( prev = &cmd_functions ; *prev ; prev = &cmd->next ) {
		cmd = *prev;
		if ( !Q_stricmp( cmd_argv[0],cmd->name ) ) {
			*prev = cmd->next;
			cmd->next = cmd_functions;
			cmd_functions = cmd;

			if ( !cmd->function ) {
				break;
			} else {
				cmd->function ();
			}
			return;
		}
	}

	for( a = cmd_alias; a != NULL; a = a->next )
	{
		if( !Q_stricmp( cmd_argv[ 0 ], a->name ) )
		{
			alias_count++;
			if( alias_count >= MAX_ALIAS_COUNT ) {
				Com_Printf( "ALIAS_LOOP_COUNT\n" );
			} else {
				Cbuf_InsertText( a->value );
			}
			return;
		}
	}
	
	if ( Cvar_Command() ) {
		return;
	}

	if ( com_cl_running && com_cl_running->integer && CL_GameCommand() ) {
		return;
	}

	if ( com_sv_running && com_sv_running->integer && SV_GameCommand() ) {
		return;
	}

	CL_ForwardCommandToServer ( text );
}

void Cmd_List_f (void)
{
	cmd_function_t	*cmd;
	int				i;
	char			*match;

	if ( Cmd_Argc() > 1 ) {
		match = Cmd_Argv( 1 );
	} else {
		match = NULL;
	}

	i = 0;
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next) {
		if (match && !Com_Filter(match, cmd->name, qfalse)) continue;

		Com_Printf ("%s\n", cmd->name);
		i++;
	}
	Com_Printf ("%i commands\n", i);
}


void Cmd_Init (void) {
	Cmd_AddCommand ("cmdlist",Cmd_List_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("vstr",Cmd_Vstr_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand( "alias", Cmd_Alias_f );
}

