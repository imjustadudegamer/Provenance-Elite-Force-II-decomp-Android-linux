//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/str.cpp                                   $
// $Revision:: 9                                                              $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 4:08p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Simple, DLL portable string class
//
// WARNING: This file is shared between game, cgame and possibly the user interface.
// It is instanced in each one of these directories because of the way that SourceSafe works.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "str.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include "../qcommon/q_shared.h"  

#ifdef _WIN32
#pragma warning(disable : 4244)     
#pragma warning(disable : 4710)     
#endif

#if defined( GAME_DLL )

#include "g_local.h"
#define STR_Error gi.Error

#elif defined ( CGAME_DLL )

#include "cg_local.h"
#define STR_Error cgi.Error

#else

#define STR_Error Com_Error

#endif

static const int STR_ALLOC_GRAN = 20;

char *str::tolower( char *s1 )
{
	char *s;
	
	s = s1;
	while( *s )
	{
		*s = ::tolower( *s );
		s++;
	   }
	
	return s1;
}

char *str::toupper( char *s1 )
{
	char *s;
	
	s = s1;
	while( *s )
	{
		*s = ::toupper( *s );
		s++;
	   }
	
	return s1;
}

int str::icmpn( const char *s1, const char *s2, int n )
{
	int c1;
	int c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;
		
		if ( !n-- )
		{
			return 0;
		}
		
		if ( c1 != c2 )
		{
			if ( c1 >= 'a' && c1 <= 'z' )
            {
				c1 -= ( 'a' - 'A' );
			}
			
			if ( c2 >= 'a' && c2 <= 'z' )
            {
				c2 -= ( 'a' - 'A' );
			}
			
			if ( c1 < c2 )
            {
				return -1;
			}
			else if ( c1 > c2 )
            {
				return 1;
            }
		}
	   }
	while( c1 );
	
	return 0;
}

int str::icmp( const char *s1, const char *s2 )
{
	int c1;
	int c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;
		
		if ( c1 != c2 )
		{
			if ( c1 >= 'a' && c1 <= 'z' )
            {
				c1 -= ( 'a' - 'A' );
			}
			
			if ( c2 >= 'a' && c2 <= 'z' )
            {
				c2 -= ( 'a' - 'A' );
			}
			
			if ( c1 < c2 )
            {
				return -1;
			}
			else if ( c1 > c2 )
            {
				return 1;
            }
		}
	   }
	while( c1 );
	
	return 0;
}

int str::cmpn( const char *s1, const char *s2, int n )
{
	int c1;
	int c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;
		
		if ( !n-- )
		{
			return 0;
		}
		
		if ( c1 < c2 )
		{
			return -1;
		}
		else if ( c1 > c2 )
		{
			return 1;
		}
	   }
	while( c1 );
	
	return 0;
}

int str::cmp( const char *s1, const char *s2 )
{
	int c1;
	int c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;
		
		if ( c1 < c2 )
		{
			return -1;
		}
		else if ( c1 > c2 )
		{
			return 1;
		}
	   }
	while( c1 );
	
	return 0;
}

bool str::isNumeric( const char *str )
{
	int len;
	int i;
	bool dot;
	
	if ( *str == '-' )
	{
		str++;
	}
	
	dot = false;
	len = strlen( str );
	for( i = 0; i < len; i++ )
	{
		if ( !isdigit( str[ i ] ) )
		{
			if ( ( str[ i ] == '.' ) && !dot )
			{
				dot = true;
				continue;
			}
			return false;
		}
	}
	
	return true;
}

str operator+( const str& a, const float b )
{
	char text[ 20 ];
	
	str result( a );
	
	sprintf( text, "%f", b );
	result.append( text );
	
	return result;
}

str operator+( const str& a, const int b )
{
	char text[ 20 ];
	
	str result( a );
	
	sprintf( text, "%d", b );
	result.append( text );
	
	return result;
}

str operator+( const str& a, const unsigned b )
{
	char text[ 20 ];
	
	str result( a );
	
	sprintf( text, "%u", b );
	result.append( text );
	
	return result;
}

str& str::operator+=( const float a )
{
	char text[ 20 ];
	
	sprintf( text, "%f", a );
	append( text );
	
	return *this;
}

str& str::operator+=( const int a )
{
	char text[ 20 ];
	
	sprintf( text, "%d", a );
	append( text );
	
	return *this;
}

str& str::operator+=( const unsigned a )
{
	char text[ 20 ];
	
	sprintf( text, "%u", a );
	append( text );
	
	return *this;
}

void str::CapLength( int newlen )
{
	assert ( data );
	
	if ( length() <= newlen )
		return;
	
	data[newlen] = 0;
	len = newlen;
}

void str::EnsureAlloced( int amount, bool keepold )
{
	if ( !data )
		data = buffer;
	
	char *newbuffer;
	bool wasalloced = ( alloced != 0 );
	int old_size;
	
	
	if ( amount <= alloced )
		return;
	
	old_size = alloced;
	
	assert ( amount );
	
	
	int newsize, mod;
	
	mod = amount % STR_ALLOC_GRAN;
	
	if ( !mod )
		newsize = amount;
	else
		newsize = amount + STR_ALLOC_GRAN - mod;
	
	alloced = newsize;
	
	
	newbuffer = new char[ alloced ];
	
	if ( !newbuffer )
		STR_Error( ERR_FATAL, "Str: failed on allocation of %i bytes", alloced );
	
	if ( wasalloced && keepold )
	{
		strncpy ( newbuffer, data, old_size );
	}
	
	
	if ( data != buffer )
	{
		delete [] data;
	}
	
	data = newbuffer;
}

void str::BackSlashesToSlashes( void )
{
	int i;
	
	for ( i = 0 ; i < len ; i++ )
	{
		if ( data[i] == '\\' )
			data[i] = '/';
	}
}

void str::snprintf( char *dst, int size, const char *fmt, ... )
{
	char buffer[0x10000];
	[[maybe_unused]] int		len;
	va_list		argptr;
	
	va_start (argptr,fmt);
	len = vsprintf (buffer,fmt,argptr);
	va_end (argptr);
	
	assert ( len < size );
	
	strncpy (dst, buffer, size-1);
}

#ifdef _WIN32
#pragma warning(disable : 4189)		
#endif

void TestStringClass( void )
{
	[[maybe_unused]] char	ch;							
	str	*t;							
	str	a;								
	str	b;								
	str	c( "test" );				
	str	d( c );						
	str	e( reinterpret_cast<const char *>(NULL) );
	[[maybe_unused]] int	i;								
	
	i = a.length();					
	i = c.length();					
	
	[[maybe_unused]] const char *s1 = a.c_str();	
	[[maybe_unused]] const char *s2 = c.c_str();	
	
	t = new str();						
	delete t;							
	
	b = "test";							
	t = new str( "test" );			
	delete t;							
	
	a = c;								
	a = NULL;							
	a = c + d;							
	a = c + "wow";						
	a = c + reinterpret_cast<const char *>(NULL);
	a = "this" + d;					
	a = reinterpret_cast<const char *>(NULL) + d;
	a += c;								
	a += "wow";							
	a += reinterpret_cast<const char *>(NULL);
	
	a = "test";							
	ch = a[ 0 ];						
	ch = a[ -1 ];						
	ch = a[ 1000 ];					
	ch = a[ 0 ];						
	ch = a[ 1 ];						
	ch = a[ 2 ];						
	ch = a[ 3 ];						
	ch = a[ 4 ];						
	ch = a[ 5 ];						
	
	a[ 1 ] = 'b';						
	a[ -1 ] = 'b';						
	a[ 0 ] = '0';						
	a[ 1 ] = '1';						
	a[ 2 ] = '2';						
	a[ 3 ] = '3';						
	a[ 4 ] = '4';						
	a[ 5 ] = '5';						
	a[ 7 ] = '7';						
	
	a = "test";							
	b = "no";							
	
	i = ( a == b );					
	i = ( a == c );					
	
	i = ( a == "blow" );				
	i = ( a == "test" );				
	i = ( a == NULL );				
	
	i = ( "test" == b );				
	i = ( "test" == a );				
	i = ( NULL == a );				
	
	i = ( a != b );					
	i = ( a != c );					
	
	i = ( a != "blow" );				
	i = ( a != "test" );				
	i = ( a != NULL );				
	
	i = ( "test" != b );				
	i = ( "test" != a );				
	i = ( NULL != a );				
	
	a = "test";                   
	b = a;                        
	
	a = "not";                   
	
	a = b;                        
	
	a += b;                       
	
	a = b;
	
	a[1] = '1';                   
	
	a = "";								
	
	for( i = 0 ; i < ( STRING_PREALLOC_SIZE + 5 ) ; i++ )
	{
		a += "a";
	}
	
	t = new str( "testtesttesttesttesttesttesttest" );			
	delete t;							
}

#ifdef _WIN32
#pragma warning(default : 4189)		
#pragma warning(disable : 4514)     
#endif
