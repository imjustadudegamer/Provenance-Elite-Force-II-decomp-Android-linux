//----------------------------------------------------------------------------- 
//
//  $Logfile:: /Code/DLLs/game/class.cpp                                      $
// $Revision:: 17                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 9:43a                                                 $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Base class that all classes that are used in conjunction with the game should
// be based off of.  Class gives run-time type information about any class
// derived from it.  This is really handy when you have a pointer to an object
// that you need to know if it supports certain behaviour.
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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>


#if defined( GAME_DLL )

#include "g_local.h"
#include "listener.h"  

#elif defined ( CGAME_DLL )

#include "cg_local.h"
#include "listener.h"
#include <qcommon/qcommon.h>

#else

#include "listener.h"
#include <qcommon/qcommon.h>

#endif

#include "class.h"
#include "Linklist.h"
#include <qcommon/output.h>

int totalmemallocated = 0;
int numclassesallocated = 0;

static ClassDef *classlist = NULL;


ClassDef::ClassDef()
{
	this->classname		= NULL;
	this->classID			= NULL;
	this->superclass		= NULL;
	this->responses		= NULL;
	this->numEvents		= 0;
	this->responseLookup = NULL;
	this->newInstance		= NULL;
	this->classSize		= 0;
	this->super				= NULL;
	this->prev				= this;
	this->next				= this;
}

ClassDef::ClassDef( const char *classname_shadow1, const char *classID_shadow1, const char *superclass_shadow1, ResponseDef<Class> *responses_shadow1,
		void *( *newInstance_shadow1 )( void ), int classSize_shadow1 )
{
	ClassDef *node;
	
	if ( classlist == NULL )
	{
		classlist = new ClassDef;
	}
	
	this->classname			= classname_shadow1;
	this->classID				= classID_shadow1;
	this->superclass			= superclass_shadow1;
	this->responses			= responses_shadow1;
	this->numEvents			= 0;
	this->responseLookup		= NULL;
	this->newInstance			= newInstance_shadow1;
	this->classSize			= classSize_shadow1;
	this->super					= getClass( superclass_shadow1 );
	
	if ( !classID_shadow1 )
	{
		this->classID = "";
	}
	
	for( node = classlist->next; node != classlist; node = node->next )
	{
		if ( ( node->super == NULL ) && ( !Q_stricmp( node->superclass, this->classname ) ) &&
			( Q_stricmp( node->classname, "Class" ) ) )
		{
			node->super	= this;
		}
	}
	
	LL_Add( classlist, this, prev, next );
}

void ClassDef::Shutdown( void )
{
	if ( responseLookup )
	{
		delete[] responseLookup;
		responseLookup = NULL;
	}
}

ClassDef::~ClassDef()
{
	ClassDef *node;
	
	if ( classlist != this )
	{
		LL_Remove( this, prev, next );
		
		for( node = classlist->next; node != classlist; node = node->next )
		{
			if ( node->super == this )
			{
				node->super	= NULL;
			}
		}
	}
	else
	{
		assert( this->next == this->prev );
	}
	
	if ( responseLookup )
	{
		delete[] responseLookup;
		responseLookup = NULL;
	}
}

void ClassDef::BuildResponseList( void )
{
	ClassDef		*c;
	ResponseDef<Class> *r;
	int			ev;
	int			i;
	qboolean		*set;
	int			num;
	
	if ( responseLookup )
	{
		delete[] responseLookup;
		responseLookup = NULL;
	}
	
	num = Event::NumEventCommands();
	responseLookup = ( Response ** )new char[ sizeof( Response * ) * num ];
	memset( responseLookup, 0, sizeof( Response * ) * num );
	
	set = new qboolean[ num ];
	memset( set, 0, sizeof( qboolean ) * num );
	
	this->numEvents = num;
	
	for( c = this; c != NULL; c = c->super )
	{
		r = c->responses;
		if ( r )
		{
			for( i = 0; r[ i ].event != NULL; i++ )
			{
				ev = ( int )*r[ i ].event;
				if ( !set[ ev ] )
				{
					set[ ev ] = true;
					if ( r[ i ].response )
					{
						responseLookup[ ev ] = &r[ i ].response;
					}
					else
					{
						responseLookup[ ev ] = NULL;
					}
				}
			}
		}
	}
	
	delete[] set;
}

void BuildEventResponses( void )
{
	ClassDef *c;
	int amount;
	int numclasses;
	
	amount = 0;
	numclasses = 0;
	for( c = classlist->next; c != classlist; c = c->next )
	{
		c->BuildResponseList();
		
		amount += c->numEvents * sizeof( Response * );
		numclasses++;
	}
	
	CLASS_DPrintf( "\n------------------\nEvent system initialized: "
		"%d classes %d events %d total memory in response list\n\n", numclasses, Event::NumEventCommands(), amount );
}

ClassDef *getClassForID( const char *name )
{
	ClassDef *c;
	
	for( c = classlist->next; c != classlist; c = c->next )
	{
		if ( c->classID && !Q_stricmp( c->classID, name ) )
		{
			return c;
		}
	}
	
	return NULL;
}

ClassDef *getClass( const char *name )
{
	ClassDef *c;
	
	for( c = classlist->next; c != classlist; c = c->next )
	{
		if ( !Q_stricmp( c->classname, name ) )
		{
			return c;
		}
	}
	
	return NULL;
}

ClassDef *getClassList( void )
{
	return classlist;
}

void listAllClasses( void )
{
	ClassDef *c;
	
	for( c = classlist->next; c != classlist; c = c->next )
	{
		CLASS_DPrintf( "%s\n", c->classname );
	}
}

void listInheritanceOrder( const char *classname )
{
	ClassDef *cls;
	ClassDef *c;
	
	cls = getClass( classname );
	if ( !cls )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", classname );
		return;
	}
	for( c = cls; c != NULL; c = c->super )
	{
		CLASS_DPrintf( "%s\n", c->classname );
	}
}

qboolean checkInheritance( const ClassDef *superclass, const ClassDef *subclass )
{
	const ClassDef *c;
	
	for( c = subclass; c != NULL; c = c->super )
	{
		if ( c == superclass )
		{
			return true;
		}
	}
	return false;
}

qboolean checkInheritance( ClassDef *superclass, const char *subclass )
{
	ClassDef *c;
	
	c = getClass( subclass );
	if ( c == NULL )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", subclass );
		return false;
	}
	return checkInheritance( superclass, c );
}

qboolean checkInheritance( const char *superclass, const char *subclass )
{
	ClassDef *c1;
	ClassDef *c2;
	
	c1 = getClass( superclass );
	c2 = getClass( subclass );
	if ( c1 == NULL )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", superclass );
		return false;
	}
	if ( c2 == NULL )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", subclass );
		return false;
	}
	return checkInheritance( c1, c2 );
}

Q_PRINTF_FUNC(2, 3)
void CLASS_Print( FILE *class_file, const char *fmt, ... )
{
	va_list	argptr;
	char		text[ 1024 ];
	
	va_start( argptr, fmt );
	vsprintf( text, fmt, argptr );
	va_end( argptr );
	
	if ( class_file )
		fprintf( class_file, "%s", text );
	else
		CLASS_DPrintf( "%s", text );
}

CLASS_DECLARATION( NULL, Class, NULL )
{
	{ NULL, NULL }
};

#ifdef NDEBUG

void * Class::operator new( size_t s ) throw()
{
	int *p;

	if ( s == 0 )
		return 0;

	s += sizeof( void * );
#if defined( GAME_DLL )
	p = ( int * )gi.Malloc( s );
#elif defined( CGAME_DLL )
	p = ( int * )cgi.Malloc( s );
#else
	p = ( int * )Z_TagMalloc( s, TAG_CLIENT );
#endif
	*p = s;
	totalmemallocated += s;
	numclassesallocated++;
	return p + 2;
}

void Class::operator delete( void *ptr ) throw()
{
	int *p;

	p = ( ( int * )ptr ) - 2;
	totalmemallocated -= *p;
	numclassesallocated--;
#if defined( GAME_DLL )
	gi.Free( p );
#elif defined( CGAME_DLL )
	cgi.Free( p );
#else
	Z_Free( p );
#endif
}

#else

#ifdef MEMORY_LEAK_TEST
int classindex = 0;
#endif

	void * Class::operator new( size_t s ) throw()
{
	int *p;
	
	s += sizeof( int ) * 3;
#if defined( GAME_DLL )
	p = ( int * )gi.Malloc( s );
#elif defined( CGAME_DLL )
	p = ( int * )cgi.Malloc( s );
#else
	p = ( int * )Z_TagMalloc( s, TAG_CLIENT );
#endif
	memset( p, 0xaa, s );
#ifdef MEMORY_LEAK_TEST
	p[ 0 ] = classindex++;
#else
	p[ 0 ] = 0x12348765;
#endif
	*( int * )( ((byte *)p) + s - sizeof( int ) ) = 0x56784321;
	p[ 1 ] = s;
	totalmemallocated += s;
	numclassesallocated++;
	return p + 2;
}

void Class::operator delete( void *ptr ) throw()
{
	int *p;
	
	p = ( ( int * )ptr ) - 2;
#ifndef MEMORY_LEAK_TEST
	assert( p[ 0 ] == 0x12348765 );
#endif
	assert( *( int * )( ((byte *)p) + p[ 1 ] - sizeof( int ) ) == 0x56784321 );
	
	totalmemallocated -= p[ 1 ];
	numclassesallocated--;
#if defined( GAME_DLL )
	gi.Free( p );
#elif defined( CGAME_DLL )
	cgi.Free( p );
#else
	Z_Free( p );
#endif
}

#endif

void DisplayMemoryUsage( void )
{
	CLASS_Printf( "Classes %-5d Class memory used: %d\n", numclassesallocated, totalmemallocated );
}

Class::Class()
{
	SafePtrList = NULL;
}

Class::~Class()
{
	ClearSafePointers();
}

#ifdef GAME_DLL

void Class::Archive( Archiver &arc )
{
}

#endif

void Class::ClearSafePointers( void )
{
	while( SafePtrList != NULL )
	{
		SafePtrList->Clear();
	}
}


void Class::warning( const char *function, const char *fmt, ... )
{
	va_list	argptr;
	char		text[ 1024 ];
	
	va_start( argptr, fmt );
	vsprintf( text, fmt, argptr );
	va_end( argptr );
	
	if ( getClassID() )
	{
		CLASS_WDPrintf( "%s::%s : %s\n", getClassID(), function, text );
	}
	else
	{
		CLASS_WDPrintf( "%s::%s : %s\n", getClassname(), function, text );
	}
}

void Class::error( const char *function, const char *fmt, ... )
{
	va_list	argptr;
	char		text[ 1024 ];
	
	va_start( argptr, fmt );
	vsprintf( text, fmt, argptr );
	va_end( argptr );
	
	if ( getClassID() )
	{
		CLASS_Error( ERR_DROP, "%s::%s : %s\n", getClassID(), function, text );
	}
	else
	{
		CLASS_Error( ERR_DROP, "%s::%s : %s\n", getClassname(), function, text );
	}
}

qboolean Class::inheritsFrom( const char *name ) const
{
	ClassDef *c;
	
	c = getClass( name );
	if ( c == NULL )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", name );
		return false;
	}
	return checkInheritance( c, classinfo() );
}

qboolean Class::isInheritedBy( const char *name	) const
{
	ClassDef *c;
	
	c = getClass( name );
	if ( c == NULL )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", name );
		return false;
	}
	return checkInheritance( classinfo(), c );
}

const char *Class::getClassname( void )
{
	ClassDef *cls;
	
	cls = classinfo();
	return cls->classname;
}

const char *Class::getClassID( void )
{
	ClassDef *cls;
	
	cls = classinfo();
	return cls->classID;
}

const char *Class::getSuperclass( void )
{
	ClassDef *cls;
	
	cls = classinfo();
	return cls->superclass;
}

void *Class::newInstance( void )
{
	ClassDef *cls;
	
	cls = classinfo();
	return cls->newInstance();
}

#define MAX_INHERITANCE 64
void ClassEvents( const char *classname, qboolean print_to_disk )
{
	ClassDef		*c;
	ResponseDef<Class> *r;
	int			ev;
	int			i, j;
	qboolean    *set;
	int			num, orderNum;
	Event       **events;
	byte        *order;
	FILE        *class_file;
	str         classNames[ MAX_INHERITANCE ];
	str         class_filename;
	
	c = getClass( classname );
	if ( !c )
	{
		CLASS_WDPrintf( "Unknown class: %s\n", classname );
		return;
	}
	
	class_file = NULL;
	
	if ( print_to_disk )
	{
		class_filename = str ( classname ) + ".txt";
		class_file = fopen( class_filename.c_str(), "w" );
		if ( class_file == NULL )
			return;
	}
	
	num = Event::NumEventCommands();
	
	set = new qboolean[ num ];
	memset( set, 0, sizeof( qboolean ) * num );
	
	events = new Event *[ num ];
	memset( events, 0, sizeof( Event * ) * num );
	
	order = new byte[ num ];
	memset( order, 0, sizeof( byte ) * num );
	
	orderNum = 0;
	for( ; c != NULL; c = c->super )
	{
		if ( orderNum < MAX_INHERITANCE )
		{
			classNames[ orderNum ] = c->classname;
		}
		r = c->responses;
		if ( r )
		{
			for( i = 0; r[ i ].event != NULL; i++ )
			{
				ev = ( int )*r[ i ].event;
				if ( !set[ ev ] )
				{
					set[ ev ] = true;
					
					if ( r[ i ].response )
					{
						events[ ev ] = r[ i ].event;
						order[ ev ] = orderNum;
					}
				}
			}
		}
		orderNum++;
	}
	
	CLASS_Print( class_file, "********************************************************\n" );
	CLASS_Print( class_file, "********************************************************\n" );
	CLASS_Print( class_file, "* All Events For Class: %s\n", classname );
	CLASS_Print( class_file, "********************************************************\n" );
	CLASS_Print( class_file, "********************************************************\n\n" );
	
	for( j = orderNum - 1; j >= 0; j-- )
	{
		CLASS_Print( class_file, "\n********************************************************\n" );
		CLASS_Print( class_file, "* Class: %s\n", classNames[ j ].c_str() );
		CLASS_Print( class_file, "********************************************************\n\n" );
		for( i = 1; i < num; i++ )
		{
			int index;
			
			index = Event::MapSortedEventIndex( i );
			if ( events[ index ] && ( order[ index ] == j ) )
            {
				Event::PrintEventDocumentation( events[ index ], class_file );
            }
		}
	}
	
	if ( class_file != NULL )
	{
		CLASS_DPrintf( "Printed class info to file %s\n", class_filename.c_str() );
		fclose( class_file );
	}
	
	delete[] events;
	delete[] order;
	delete[] set;
}

static int dump_numclasses;
static int dump_numevents;
void DumpClass( FILE * class_file, const char * className, int typeFlag )
{
	ClassDef		*c;
	ResponseDef<Class> *r;
	int			ev;
	int			i;
	int			num;
	Event       **events;
	
	c = getClass( className );
	if ( !c )
	{
		return;
	}
	
	num = Event::NumEventCommands();
	
	events = new Event *[ num ];
	memset( events, 0, sizeof( Event * ) * num );
	
	r = c->responses;
	if ( r )
	{
		for( i = 0; r[ i ].event != NULL; i++ )
		{
			ev = ( int )*r[ i ].event;
			if ( r[ i ].response )
			{
				events[ ev ] = r[ i ].event;
			}
		}
	}
	
	CLASS_Print( class_file, "\n");
	if ( c->classID[ 0 ] )
	{
		CLASS_Print( class_file, "<h2> <a name=\"%s\">%s (<i>%s</i>)</a>", c->classname, c->classname, c->classID );
	}
	else
	{
		CLASS_Print( class_file, "<h2> <a name=\"%s\">%s</a>", c->classname, c->classname );
	}
	
	for( c = c->super; c != NULL; c = c->super )
	{
		CLASS_Print( class_file, " -> <a href=\"#%s\">%s</a>", c->classname, c->classname );
	}
	CLASS_Print( class_file, "</h2>\n");
	
	dump_numclasses++;
	
	CLASS_Print( class_file, "<BLOCKQUOTE>\n");
	for( i = 1; i < num; i++ )
	{
		int index;
		
		index = Event::MapSortedEventIndex( i );
		if ( events[ index ] )
		{
			Event::PrintEventDocumentation( events[ index ], class_file, true, typeFlag );
			dump_numevents++;
		}
	}
	CLASS_Print( class_file, "</BLOCKQUOTE>\n");
	delete[] events;
}

void DumpAllClasses( int typeFlag, int outputType, const char *filename )
{
	str defaultname;
#if defined( GAME_DLL )
	defaultname = "g_allclasses";
#elif defined( CGAME_DLL )
	defaultname = "cg_allclasses";
#else
	defaultname = "cl_allclasses";
#endif
	
	if ( outputType == OUTPUT_HTML || !outputType )
	{
		HTMLDocFileOutput *hdout = new HTMLDocFileOutput();
		if ( filename )
			hdout->Write(filename, classlist, typeFlag);
		else
			hdout->Write(defaultname, classlist, typeFlag);
		delete hdout;
	}
	
	if ( outputType == OUTPUT_CMD || !outputType )
	{
		ToolDocFileOutput *dout = new ToolDocFileOutput();
		if ( filename )
			dout->Write(filename, classlist, typeFlag);
		else
			dout->Write(defaultname, classlist, typeFlag);
		delete dout;
	}

}


void ShutdownClasses( void )
{
	ClassDef *c;
	
	for( c = classlist->next; c != classlist; c = c->next )
	{
		c->Shutdown();
	}
}
