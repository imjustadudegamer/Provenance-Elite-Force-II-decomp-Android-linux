//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/lexer.cpp                                      $
// $Revision:: 7                                                              $
//     $Date:: 10/08/02 7:35a                                                 $
//
// Copyright (C) 1999 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
//
// DESCRIPTION:
// 
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "g_local.h"
#include "lexer.h"
#include "entity.h"
#include "worldspawn.h"   

static const char *pr_punctuation[] =
{
	"+=", "-=",  "*=",  "/=", "&=", "|=", "++", "--",
	"&&", "||",  "<=",  ">=", "==", "!=", ";",  ",", "!",
	"*",  "/",   "(",   ")",  "-",  "+",  "=",  "[", "]",
	"{",  "}",   "...", ".",  "<",  ">" , "#" , "&", "|",
	NULL
};



int		type_size[6]      = { 1, 1, 1, 3, 1, 1 };




Lexer::Lexer( Program &prg )  
	: program( prg )
{
	source_line = 0;
	pr_file_p = NULL;
	pr_line_start = NULL;
	pr_bracelevel = 0;
	pr_immediate_type = NULL;
}

void Lexer::NewLine( void )
{
	bool m;
	
	if ( *pr_file_p == '\n' )
	   {
		pr_file_p++;
		m = true;
	   }
	else
	{
		m = false;
	}
	
	source_line++;
	pr_line_start = pr_file_p;
	
	if ( m )
	{
		pr_file_p--;
	}
}

void Lexer::LexString( void )
{
	int   c;
	int	len;
	
	len = 0;
	pr_file_p++;
	do
	{
		c = *pr_file_p++;
		if ( !c )
		{
			ParseError( "EOF inside quote" );
		}
		
		if ( c == '\n' )
		{
			ParseError( "newline inside quote" );
		}
		
		if ( c=='\\' )
		{
			c = *pr_file_p++;
			if ( !c )
			{
				ParseError( "EOF inside quote" );
			}
			
			if ( c == 'n' )
			{
				c = '\n';
			}
			else if ( c == '"' )
			{
				c = '"';
			}
			else
			{
				ParseError( "Unknown escape char" );
			}
		}
		else if ( c=='\"' )
		{
			pr_token[ len ] = 0;
			pr_token_type = tt_immediate;
			pr_immediate_type = &type_string;
			strcpy( pr_immediate_string, pr_token );
			
			return;
		}
		
		pr_token[ len ] = c;
		len++;
	}
	while( 1 );
}

float Lexer::LexNumber( void )
{
	int c;
	int len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[ len ] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	}
	while( ( ( c >= '0' ) && ( c <= '9' ) ) || ( c == '.' ) );
	   
	pr_token[ len ] = 0;
	   
	return ( float )atof( pr_token );
}

void Lexer::LexVector( void )
{
	int i;
	
	pr_file_p++;
	pr_token_type = tt_immediate;
	pr_immediate_type = &type_vector;
	for( i = 0; i < 3; i++ )
	{
		pr_immediate.vector[ i ] = LexNumber();
		LexWhitespace();
	}
	
	if ( *pr_file_p != '\'' )
	{
		ParseError( "Bad vector" );
	}
	
	pr_file_p++;
}

void Lexer::LexEntity( void )
{
	int c;
	int len;
	TargetList *list;
	
	pr_file_p++;
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[ len ] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	}
	while( ( ( c >= 'a' ) && ( c <= 'z' ) ) || ( ( c >= 'A' ) && ( c <= 'Z' ) ) || ( c == '_' ) || ( ( c >= '0' ) && ( c <= '9' ) ) );
	   
	pr_token[ len ] = 0;
	pr_token_type = tt_immediate;
	pr_immediate_type = &type_entity;
	   
	list= world ? world->GetTargetList( str( pr_token ), true ) : NULL;
	if ( list )
	{
		pr_immediate.entity = -list->index;
	}
	else
	{
		pr_immediate.entity = 0;
	}
}

void Lexer::LexName( void )
{
	int c;
	int len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[ len ] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	}
	while( ( ( c >= 'a' ) && ( c <= 'z' ) ) || ( ( c >= 'A' ) && ( c <= 'Z' ) ) || ( c == '_' ) || ( ( c >= '0' ) && ( c <= '9' ) ) );
	   
	pr_token[ len ] = 0;
	pr_token_type = tt_name;
}

void Lexer::LexPunctuation( void )
{
	int	len;
	const char	**ptr;
	const char  *p;
	
	pr_token_type = tt_punct;
	
	for( ptr = pr_punctuation; *ptr != NULL; ptr++ )
	{
		p = *ptr;
		len = strlen( p );
		if ( !strncmp( p, pr_file_p, len ) )
		{
			strcpy( pr_token, p );
			if ( p[ 0 ] == '{' )
            {
				pr_bracelevel++;
            }
			else if ( p[ 0 ] == '}' )
            {
				pr_bracelevel--;
            }
			
			pr_file_p += len;
			return;
		}
	}
	
	ParseError( "Unknown punctuation" );
}

void Lexer::LexWhitespace( void )
{
	int c;
	
	while( 1 )
	{
		while( ( c = *pr_file_p ) <= ' ' )
		{
			if ( c=='\n' )
            {
				NewLine();
            }
			
			if ( c == 0 )
            {
				return;
            }
			
			pr_file_p++;
		}
		
		if ( ( c == '/' ) && ( pr_file_p[ 1 ] == '/' ) )
		{
			while( *pr_file_p && ( *pr_file_p != '\n' ) )
            {
				pr_file_p++;
            }
			
			NewLine();
			pr_file_p++;
			
			continue;
		}
		
		if ( ( c == '/' ) && ( pr_file_p[ 1 ] == '*' ) )
		{
			do
			{
				pr_file_p++;
				if ( pr_file_p[ 0 ]=='\n' )
				{
					NewLine();
				}
				
				if ( pr_file_p[ 1 ] == 0 )
				{
					return;
				}
			} 
			while( ( pr_file_p[ -1 ] != '*' ) || ( pr_file_p[ 0 ] != '/' ) );
			
			pr_file_p++;
			continue;
		}
		
		break;
	}
}

void Lexer::LexPreprocessor( void )
{
	int c;
	int len;
	
	len = 0;
	c = *pr_file_p;
	do
	{
		pr_token[ len ] = c;
		len++;
		pr_file_p++;
		c = *pr_file_p;
	}
	while( ( ( c >= 'a' ) && ( c <= 'z' ) ) || ( ( c >= 'A' ) && ( c <= 'Z' ) ) || ( c == '_' ) || ( ( c >= '0' ) && ( c <= '9' ) ) );

	pr_token[ len ] = 0;
	pr_token_type = tt_preprocessor;
}


void Lexer::Lex( void )
{
	int c;
	
	pr_token[ 0 ] = 0;
	
	if ( !pr_file_p )
	   {
		pr_token_type = tt_eof;
		return;
	   }
	
	LexWhitespace();
	
	c = *pr_file_p;
	if ( !c )
	   {
		pr_token_type = tt_eof;
		return;
	   }
	
	if ( c == '\"' )
	   {
		LexString ();
		return;
	   }
	
	if ( c == '\'' )
	   {
		LexVector ();
		return;
	   }
	
	if ( ( ( c >= '0' ) && ( c <= '9' ) ) || ( ( c == '-' ) && ( pr_file_p[ 1 ] >= '0' ) && ( pr_file_p[ 1 ] <= '9' ) ) || 
		( ( c == '.' ) && ( pr_file_p[ 1 ] >= '0' ) && ( pr_file_p[ 1 ] <= '9' ) ) )
	   {
		pr_token_type = tt_immediate;
		pr_immediate_type = &type_float;
		pr_immediate._float = LexNumber();
		return;
   	}
	
	if ( c == '$' )
	   {
		LexEntity();
		return;
	   }
	
	if ( c == '#' )
	{
		LexPreprocessor();
		return;
	}
	
	if ( ( ( c >= 'a' ) && ( c <= 'z' ) ) || ( ( c >= 'A' ) && ( c <= 'Z' ) ) || ( c == '_' ) )
	   {
		LexName();
		return;
	   }
	
	LexPunctuation();
}


void Lexer::ParseError( const char *error, ... )
{
	va_list	argptr;
	char		string[ 1024 ];
	
	va_start( argptr, error );
	vsprintf( string, error, argptr );
	va_end( argptr );
	
	gi.Printf( "%s(%i) : %s\n", program.s_file.c_str(), source_line, string );
	
	throw "Error";
}

void Lexer::Expect( const char *string )
{
	if ( strcmp( string, pr_token ) )
	{
		ParseError( "expected %s, found %s",string, pr_token );
	}
	
	Lex();
}

bool Lexer::Check( const char *string )
{
	if ( strcmp( string, pr_token ) )
	{
		return false;
	}
	
	Lex();
	
	return true;
}

char *Lexer::ParseName( void )
{
	static char	ident[ MAX_NAME ];
	
	if ( pr_token_type != tt_name )
	{
		ParseError( "not a name" );
	}
	
	if ( strlen( pr_token ) >= ( MAX_NAME - 1 ) )
	{
		ParseError( "name too long" );
	}
	
	strcpy( ident, pr_token );
	Lex();
	
	return ident;
}

void Lexer::SkipOutOfFunction( void )
{
	do
	{
		if ( !pr_bracelevel )
		{
			return;
		}
		   
		Lex();
	}
	while( pr_token[ 0 ] ); 
}

void Lexer::SkipToSemicolon( void )
{
	do
	{
		if ( Check( ";" ) )
		{
			return;
		}
		   
		Lex();
	}
	while( pr_token[ 0 ] ); 
}

type_t *Lexer::CheckType( void )
{
	type_t *type;
	
	if ( !strcmp( pr_token, "float" ) )
	{
		type = &type_float;
	}
	else if ( !strcmp( pr_token, "vector" ) )
	{
		type = &type_vector;
	}
	else if ( !strcmp( pr_token, "entity" ) )
	{
		type = &type_entity;
	}
	else if ( !strcmp( pr_token, "string" ) )
	{
		type = &type_string;
	}
	else if ( !strcmp( pr_token, "void" ) )
	{
		type = &type_void;
	}
	else
	{
		type = NULL;
	}
	
	return type;
}

type_t *Lexer::ParseType( void )
{
	type_t *type;
	
	type = CheckType();
	if ( !type )
	{
		ParseError( "\"%s\" is not a type", pr_token );
	}
	
	Lex();
	
	return type;
}
