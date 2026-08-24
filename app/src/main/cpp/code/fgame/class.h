//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/class.h                                        $
// $Revision:: 18                                                             $
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
class Class;
class Event;

#ifndef __CLASS_H__
#define __CLASS_H__

#if defined( GAME_DLL )
#include "g_local.h"
#include "Linklist.h"

#define CLASS_DPrintf gi.DPrintf
#define CLASS_Printf gi.Printf
#define CLASS_WDPrintf gi.WDPrintf
#define CLASS_WPrintf gi.WPrintf
#define CLASS_Error gi.Error

class Archiver;

#elif defined ( CGAME_DLL )
#include "../../DLLs/game/q_shared.h"
#include "Linklist.h"

#define CLASS_DPrintf cgi.DPrintf
#define CLASS_Printf cgi.Printf
#define CLASS_WDPrintf cgi.WDPrintf
#define CLASS_WPrintf cgi.WPrintf
#define CLASS_Error cgi.Error

#else

#include "../../DLLs/game/q_shared.h"
#include "Linklist.h"

#define CLASS_DPrintf Com_DPrintf
#define CLASS_Printf Com_Printf
#define CLASS_WDPrintf Com_WDPrintf
#define CLASS_WPrintf Com_WPrintf
#define CLASS_Error Com_Error
#endif

#define EVENT_TIKI_ONLY		1
#define EVENT_SCRIPT_ONLY	2
#define EVENT_ALL				3

#define OUTPUT_HTML	1
#define OUTPUT_CMD	2
#define OUTPUT_ALL	3

#define MAX_CLASSES 1024

typedef void ( Class::*Response )( Event *event );

template< class Type >
struct ResponseDef
	{
	Event		      *event;
   void           ( Type::*response )( Event *event );
	};


class ClassDef
	{
	public:
		const char	*classname;
		const char	*classID;
		const char	*superclass;
		void			*( *newInstance )( void );
		int			classSize;
		ResponseDef<Class> *responses;
		int			numEvents;
		Response		**responseLookup;
		ClassDef		*super;
		ClassDef		*next;
		ClassDef		*prev;

		ClassDef();
		~ClassDef();
		ClassDef( const char *classname, const char *classID, const char *superclass,
			ResponseDef<Class> *responses, void *( *newInstance )( void ), int classSize );
		void	BuildResponseList( void );
      void  Shutdown( void );
	};


class SafePtrBase;

class Class;



#define CLASS_DECLARATION( nameofsuperclass, nameofclass, classid )	\
	ClassDef nameofclass::ClassInfo												\
		(																					\
		#nameofclass, classid, #nameofsuperclass,								\
		( ResponseDef<Class> * )nameofclass::Responses,                \
      nameofclass::_newInstance,	sizeof( nameofclass )					\
		);																					\
	void *nameofclass::_newInstance( void )				               \
		{																					\
		return new nameofclass;														\
		}																					\
	ClassDef *nameofclass::classinfo( void ) const								   \
		{																					\
		return &( nameofclass::ClassInfo );										\
      }                                                              \
   ResponseDef<nameofclass> nameofclass::Responses[] =

#define CLASS_PROTOTYPE( nameofclass )											\
	public:																				\
	static	ClassDef			ClassInfo;											\
	static	void				*_newInstance( void );			            \
	virtual	ClassDef			*classinfo( void ) const override;	       \
	static	ResponseDef<nameofclass>	Responses[]

class Class
	{
   private:
		SafePtrBase	 *SafePtrList;
		friend class SafePtrBase;

   protected:
      void              ClearSafePointers( void );

	public:
		static	ClassDef			ClassInfo;
		static	void				*_newInstance( void );
		virtual	ClassDef			*classinfo( void ) const;
		static	ResponseDef<Class>	Responses[];
		void * operator	new( size_t ) throw();
		void operator		delete( void * ) throw();

								Class();
		virtual				~Class();
		void					warning( const char *function, const char *fmt, ... ) Q_PRINTF_FUNC(3, 4);
		void					error( const char *function, const char *fmt, ... ) Q_PRINTF_FUNC(3, 4);
		qboolean				inheritsFrom( ClassDef *c ) const;
		qboolean				inheritsFrom( const char *name ) const;
		qboolean				isInheritedBy( const char *name ) const;
		qboolean				isInheritedBy( ClassDef *c ) const;
		const char			*getClassname( void );
		const char			*getClassID( void );
		const char			*getSuperclass( void );
		void					*newInstance( void );

#ifdef GAME_DLL
		virtual void		Archive( Archiver &arc );
#endif
	};

void		BuildEventResponses( void );
ClassDef	*getClassForID( const char *name );
ClassDef	*getClass( const char *name );
ClassDef	*getClassList( void );
void		listAllClasses( void );
void		listInheritanceOrder( const char *classname );
qboolean	checkInheritance( const ClassDef * const superclass, const ClassDef * const subclass );
qboolean	checkInheritance( ClassDef * const superclass, const char * const subclass );  
qboolean	checkInheritance( const char * const superclass, const char * const subclass );  
void     DisplayMemoryUsage( void );
void     ClassEvents( const char *classname, qboolean dump = false );
void     DumpAllClasses( int typeFlag = EVENT_ALL, int outputType = OUTPUT_ALL, const char *filename = NULL);
void		DumpClass( FILE * class_file, const char * className, int typeFlag = 0);

inline qboolean Class::inheritsFrom
	(
	ClassDef *c
	) const

	{
	return checkInheritance( c, classinfo() );
	}

inline qboolean Class::isInheritedBy
	(
	ClassDef *c
	) const

	{
	return checkInheritance( classinfo(), c );
	}

#define isSubclassOf( classname )inheritsFrom( &classname::ClassInfo )
#define isSuperclassOf( classname )isInheritedBy( &classname::ClassInfo )
#include "../corepp/safeptr.h"

typedef SafePtr<Class> ClassPtr;

#ifdef GAME_DLL
#include "archive.h"
#endif

extern int totalmemallocated;

#ifndef GAME_DLL
extern "C"
   {
   void	        ShutdownClasses( void );
   }
#endif

#endif 
