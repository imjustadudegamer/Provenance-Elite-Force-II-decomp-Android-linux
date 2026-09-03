//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/archive.cpp                               $
// $Revision:: 12                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:35p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Class for archiving objects
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).
#include "glb_local.h"
#include "archive.h"
#include "level.h"
#include "entity.h"
#include <new>

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#include "../qcommon/qa_probe.h"
#include <time.h>

#define GRS_NOW() ( PROBE_On() ? ( long long )clock() * 1000000LL / CLOCKS_PER_SEC : 0 )

enum
{
	ARC_NULL, ARC_Vector, ARC_Vec3, ARC_Vec4, ARC_Integer, ARC_Unsigned, ARC_Byte, ARC_Char, ARC_Short, ARC_UnsignedShort,
	ARC_Float, ARC_Double, ARC_Boolean, ARC_String, ARC_Raw, ARC_Object, ARC_ObjectPointer,
	ARC_SafePointer, ARC_Event, ARC_EventPointer, ARC_Quat, ARC_Entity, ARC_Bool,
	ARC_NUMTYPES
};

static const char *typenames[] =
{
	"NULL", "vector", "vec3", "vec4", "int", "unsigned", "byte", "char", "short", "unsigned short",
	"float", "double", "qboolean", "string", "raw data", "object", "objectpointer",
	"safepointer", "event", "eventpointer", "quaternion", "entity", "bool"
};

#define ArchiveHeader   ( *( int * )"EF2" )
#define ArchiveVersion	2                       
#define ArchiveInfo     "EF2 Archive Version 2" 

CLASS_DECLARATION( Class, FileRead, NULL )
{
	{ NULL, NULL }
};

FileRead::FileRead()
{
	length = 0;
	buffer = NULL;
	pos = 0;
}

FileRead::~FileRead()
{
	Close();
}

void FileRead::Close( void )
{
	if ( buffer )
	{
		gi.Free( ( void * )buffer );
		buffer = NULL;
	}
	
	filename = "";
	length = 0;
	pos = 0;
}

const char *FileRead::Filename( void )
{
	return filename.c_str();
}

size_t FileRead::Length( void )
{
	return length;
}

size_t FileRead::Pos( void )
{
	return pos - buffer;
}

qboolean FileRead::Seek( size_t newpos )
{
	if ( !buffer )
	{
		return false;
	}
	
	if ( newpos > length )
	{
		return false;
	}
	
	pos = buffer + newpos;
	
	return true;
}

qboolean FileRead::Open( const char *name )
{
	byte  *tempbuf;
	assert( name );
	
	assert( !buffer );
	Close();
	
	if ( !name )
	{
		return false;
	}
	
	length = gi.FS_ReadFile( name, ( void ** )&tempbuf, true );
	if ( length == ( size_t )( -1 ) )
	{
		length = 0;
		pos    = 0;
		buffer = 0;
		return false;
	}
	buffer = ( byte * )gi.Malloc( length );
	memcpy( buffer, tempbuf, length );
	gi.FS_FreeFile( tempbuf );
	
	filename = name;
	pos = buffer;
	
	return true;
}

qboolean FileRead::Read( void *dest, size_t size )
{
	assert( dest );
	assert( buffer );
	assert( pos );
	
	if ( !dest )
	{
		return false;
	}
	
	if ( size == 0 )
	{
		return false;
	}
	if ( ( pos + size ) > ( buffer + length ) )
	{
		return false;
	}

	if ( ( pos + size ) > ( buffer + length ) )
	{
		return false;
	}

	memcpy( dest, pos, size );
	pos += size;
	
	return true;
}

CLASS_DECLARATION( Class, Archiver, NULL )
{
	{ NULL, NULL }
};

Archiver::Archiver()
{
	file = 0;
	fileerror = false;
	harderror = true;
	archivemode = ARCHIVE_READ;
	numclassespos = 0;
	assert( ( sizeof( typenames ) / sizeof( typenames[ 0 ] ) ) == ARC_NUMTYPES );
}

Archiver::~Archiver()
{
	if ( file )
	{
		Close();
	}
	
	readfile.Close();
}

void Archiver::FileError( const char *fmt, ... )
{
	va_list	argptr;
	char		text[ 1024 ];
	
	va_start( argptr, fmt );
	vsprintf( text, fmt, argptr );
	va_end( argptr );
	
	fileerror = true;
	Close();
	if ( archivemode == ARCHIVE_READ )
	{
		if ( harderror )
		{
			gi.Error( ERR_DROP, "Error while loading %s : %s\n", filename.c_str(), text );
		}
		else
		{
			gi.WPrintf( "Error while loading %s : %s\n", filename.c_str(), text );
		}
	}
	else
	{
		if ( harderror )
		{
			gi.Error( ERR_DROP, "Error while writing to %s : %s\n", filename.c_str(), text );
		}
		else
		{
			gi.WPrintf( "Error while writing to %s : %s\n", filename.c_str(), text );
		}
	}
}

void Archiver::Close( void )
{
	if ( file )
	{
		if ( archivemode == ARCHIVE_WRITE )
		{
			int numobjects;
			
			gi.FS_FSeek( file, numclassespos, FS_SEEK_SET );
			numclassespos = gi.FS_FTell( file );
			numobjects = classpointerList.NumObjects();
			ArchiveInteger( &numobjects );
		}
		
		gi.FS_FCloseFile( file );
		file = 0;
	}
	
	WIDGET_PROBE("archive", "close: enter (read mode)\n");
	readfile.Close();
	WIDGET_PROBE("archive", "close: readfile closed\n");

	if ( archivemode == ARCHIVE_READ )
	{
		int i, num;
		Class * classptr;
		pointer_fixup_t *fixup;

		num = fixupList.NumObjects();
		WIDGET_PROBE("archive", "close: %d fixups, %d classpointers\n", num,
		           classpointerList.NumObjects());
		for( i = 1; i <= num; i++ )
		{
			fixup = fixupList.ObjectAt( i );
			classptr = classpointerList.ObjectAt( fixup->index );

			assert( !( ( classptr == NULL ) && ( fixup->index != ARCHIVE_NULL_POINTER ) ) );

			if ( fixup->type == pointer_fixup_normal )
            {
				Class ** fixupptr;
				fixupptr = ( Class ** )fixup->ptr;
				*fixupptr = classptr;
            }
			else if ( fixup->type == pointer_fixup_safe )
            {
				SafePtrBase * fixupptr;
				fixupptr = ( SafePtrBase * )fixup->ptr;
				fixupptr->InitSafePtr( classptr );
            }
			delete fixup;

			if ( ( i % 4096 ) == 0 ) {
				WIDGET_PROBE("archive", "close: fixup %d of %d\n", i, num);
			}
		}
		WIDGET_PROBE("archive", "close: fixups resolved (%d)\n", num);

		{
			int             ei;
			int             ents = 0, bad = 0;
			int             evs = 0, badSrc = 0;
			int             firstBad = -1;
			Listener       *L;
			Event          *w;

			extern Event *EventQueue;

			for ( ei = 0; ei < globals.max_entities; ei++ ) {
				L = ( Listener * )G_GetEntity( ei );
				if ( !L ) {
					continue;
				}
				ents++;
				if ( SafePtrBase::WIDGETP1_HeadPrevNull( L ) ) {
					if ( firstBad < 0 ) {
						firstBad = ei;
					}
					bad++;
				}
			}
			for ( w = EventQueue->next; w != EventQueue; w = w->next ) {
				L = w->GetSourceObject();
				evs++;
				if ( L && SafePtrBase::WIDGETP1_HeadPrevNull( L ) ) {
					badSrc++;
				}
			}
			WIDGET_PROBE(
				"archive",
				"closeresweep: entities=%d badHead=%d firstBadEnt=%d "
				"queueEvents=%d badSrcHead=%d\n",
				ents, bad, firstBad, evs, badSrc );
		}

		fixupList.FreeObjectList();
		classpointerList.FreeObjectList();
	}
	WIDGET_PROBE("archive", "close: done\n");
}

size_t Archiver::Pos( void )
{
	return readfile.Pos();
}


qboolean Archiver::Read( const char *name, qboolean file_harderror )
{
	unsigned header;
	unsigned version;
	str		info;
	int      num;
	int      i;
	Class    *null;
	
	harderror = file_harderror;
	
	assert( name );
	if ( !name )
	{
		FileError( "NULL pointer for filename in Archiver::Read.\n" );
		return false;
	}
	
	fileerror = false;
	
	archivemode = ARCHIVE_READ;
	
	filename = name;
	
	if ( !readfile.Open( filename.c_str() ) )
	{
		if ( file_harderror )
		{
			FileError( "Couldn't open file." );
		}
		return false;
	}
	
	ArchiveUnsigned( &header );
	if ( header != ( unsigned int )ArchiveHeader )
	{
		readfile.Close();
		FileError( "Not a valid archive." );
		return false;
	}
	
	ArchiveUnsigned( &version );
	if ( version > ArchiveVersion )
	{
		readfile.Close();
		FileError( "Archive is from version %.2f.  Check http://www.ritual.com for an update.", ( double )version );
		return false;
	}
	
	if ( version < ArchiveVersion )
	{
		readfile.Close();
		FileError( "Archive is out of date." );
		return false;
	}
	
	ArchiveString( &info );
	gi.DPrintf( "%s\n", info.c_str() );
	
	ArchiveInteger( &num );
	classpointerList.Resize( num );
	null = NULL;
	for( i = 1; i <= num; i++ )
	{
		classpointerList.AddObject( null );
	}
	
	return true;
}

qboolean Archiver::Create( const char *name, qboolean file_harderror )
{
	unsigned header;
	unsigned version;
	str      info;
	int      numZero = 0;
	
	harderror = file_harderror;
	
	assert( name );
	if ( !name )
	{
		FileError( "NULL pointer for filename in Archiver::Create.\n" );
		return false;
	}
	
	fileerror = false;
	
	archivemode = ARCHIVE_WRITE;
	
	filename = name;
	
	file = gi.FS_FOpenFileWrite( filename.c_str() );
	if ( !file )
	{
		FileError( "Couldn't open file." );
		return false;
	}
	
	header = ArchiveHeader;
	ArchiveUnsigned( &header );
	version = ArchiveVersion;
	ArchiveUnsigned( &version );
	info = ArchiveInfo;
	ArchiveString( &info );
	
	numclassespos = gi.FS_FTell( file );
	ArchiveInteger( &numZero );
	
	return true;
}

inline void Archiver::CheckRead( void )
{
	assert( archivemode == ARCHIVE_READ );
	if ( !fileerror && ( archivemode != ARCHIVE_READ ) )
	{
		FileError( "File read during a write operation." );
	}
}

inline void Archiver::CheckWrite( void )
{
	assert( archivemode == ARCHIVE_WRITE );
	if ( !fileerror && ( archivemode != ARCHIVE_WRITE ) )
	{
		FileError( "File write during a read operation." );
	}
}

inline archsize_t Archiver::ReadSize( void )
{
	archsize_t s;

	s = 0;
	if ( !fileerror )
	{
		readfile.Read( &s, sizeof( s ) );
	}

	return s;
}

inline void Archiver::CheckSize( int type, size_t size )
{
	archsize_t s;

	if ( !fileerror )
	{
		s = ReadSize();

		if ( size != s )
		{
			FileError( "Invalid data size of %d on %s.", ( int )s, typenames[ type ] );
		}
	}
}

qboolean Archiver::Loading(void)
{
    return (archivemode == ARCHIVE_READ);
}

qboolean Archiver::Saving(void)
{
    return (archivemode == ARCHIVE_WRITE);
}

inline void Archiver::WriteSize( archsize_t size )
{
	gi.FS_Write( &size, sizeof( size ), file );
}

inline int Archiver::ReadType( void )
{
	int t;
	
	if ( !fileerror )
	{
		readfile.Read( &t, sizeof( t ) );
		
		return t;
	}
	
	return ARC_NULL;
}

inline void Archiver::WriteType( int type )
{
	gi.FS_Write( &type, sizeof( type ), file );
}

inline void Archiver::CheckType( int type )
{
	int t;
	
	assert( ( type >= 0 ) && ( type < ARC_NUMTYPES ) );
	
	if ( !fileerror )
	{
		t = ReadType();
		if ( t != type )
		{
			FileError( "Expecting %s", typenames[ type ] );
		}
	}
}



inline void Archiver::ArchiveData( int type, void *data, size_t size )
{
	if ( archivemode == ARCHIVE_READ )
	{
#ifndef NDEBUG
		CheckRead();
#endif
#ifdef ARCHIVE_USE_TYPES
		CheckType( type );
#endif
		
		if ( !fileerror && size )
		{
			readfile.Read( data, size );
		}
	}
	else
	{
#ifndef NDEBUG
		CheckWrite();
#endif
#ifdef ARCHIVE_USE_TYPES
		WriteType( type );
#endif
		
		if ( !fileerror && size )
		{
			gi.FS_Write( data, size, file );
		}
	}
}


#define ARCHIVE( func, type )								\
void Archiver::Archive##func								\
	(																\
	type * v														\
	)																\
																	\
	{																\
	ArchiveData( ARC_##func, v, sizeof( type ) );	\
	}

	ARCHIVE( Vector, Vector );
	ARCHIVE( Integer, int );
	ARCHIVE( Unsigned, unsigned );
	ARCHIVE( Byte, byte );
	ARCHIVE( Char, char );
	ARCHIVE( Short, short );
	ARCHIVE( UnsignedShort, unsigned short );
	ARCHIVE( Float, float );
	ARCHIVE( Double, double );
	ARCHIVE( Boolean, qboolean );
	ARCHIVE( Quat, Quat );
	ARCHIVE( Bool, bool );

void Archiver::ArchiveVec3( vec3_t vec )
{
	ArchiveData( ARC_Vec3, vec, sizeof( vec3_t ) );
}

void Archiver::ArchiveVec4( vec4_t vec )
{
	ArchiveData( ARC_Vec4, vec, sizeof( vec4_t ) );
}

void Archiver::ArchiveObjectPointer( Class ** ptr )
{
	int index = 0;
	
	if ( archivemode == ARCHIVE_READ )
	{
		pointer_fixup_t *fixup;
		ArchiveData( ARC_ObjectPointer, &index, sizeof( index ) );
		
		assert( ptr );
		if ( !ptr )
		{
			FileError( "NULL pointer in ArchiveObjectPointer." );
		}
		
		if ( index == ARCHIVE_NULL_POINTER )
		{
			*ptr = NULL;
		}
		else
		{
			*ptr = NULL;
			
			fixup = new pointer_fixup_t;
			fixup->ptr = ( void ** )ptr;
			fixup->index = index;
			fixup->type = pointer_fixup_normal;
			fixupList.AddObject( fixup );
		}
	}
	else
	{
		if ( *ptr )
		{
			index = classpointerList.AddUniqueObject( *ptr );
		}
		else
		{
			index = ARCHIVE_NULL_POINTER;
		}
		ArchiveData( ARC_ObjectPointer, &index, sizeof( index ) );
	}
}

void Archiver::ArchiveSafePointer( SafePtrBase * ptr )
{
	int index = 0;
	
	if ( archivemode == ARCHIVE_READ )
	{
		pointer_fixup_t *fixup;
		
		ArchiveData( ARC_SafePointer, &index, sizeof( index ) );
		
		assert( ptr );
		if ( !ptr )
		{
			FileError( "NULL pointer in ReadSafePointer." );
		}
		
		if ( index == ARCHIVE_NULL_POINTER )
		{
			ptr->InitSafePtr( NULL );
		}
		else
		{
			ptr->InitSafePtr( NULL );
			
			fixup = new pointer_fixup_t;
			fixup->ptr = ( void ** )ptr;
			fixup->index = index;
			fixup->type = pointer_fixup_safe;
			fixupList.AddObject( fixup );
		}
	}
	else
	{
		if ( ptr->Pointer() )
		{
			Class * obj;
			
			obj = ptr->Pointer();
			index = classpointerList.AddUniqueObject( obj );
		}
		else
		{
			index = ARCHIVE_NULL_POINTER;
		}
		ArchiveData( ARC_SafePointer, &index, sizeof( index ) );
	}
}

void Archiver::ArchiveEvent( Event * ev )
{
	if ( archivemode == ARCHIVE_READ )
	{
#ifndef NDEBUG
		CheckRead();
#endif
#ifdef ARCHIVE_USE_TYPES
		CheckType( ARC_Event );
#endif
		
		if ( !fileerror )
		{
			ev->Archive( *this );
		}
	}
	else
	{
#ifndef NDEBUG
		CheckWrite();
#endif
#ifdef ARCHIVE_USE_TYPES
		WriteType( ARC_Event );
#endif
		
		if ( ev == NULL )
		{
			NullEvent.Archive( *this );
		}
		else
		{
			ev->Archive( *this );
		}
	}
}

void Archiver::ArchiveEventPointer( Event ** ev )
{
	int index;
	
	if ( archivemode == ARCHIVE_READ )
	{
#ifndef NDEBUG
		CheckRead();
#endif
#ifdef ARCHIVE_USE_TYPES
		CheckType( ARC_EventPointer );
#endif
		ArchiveInteger( &index );
		
		if ( !fileerror )
		{
			if ( index == ARCHIVE_POINTER_VALID )
            {
				*ev = new Event;
				(*ev)->Archive( *this );
            }
			else
            {
				(*ev) = NULL;
            }
		}
	}
	else
	{
#ifndef NDEBUG
		CheckWrite();
#endif
		if ( *ev )
		{
			index = ARCHIVE_POINTER_VALID;
		}
		else
		{
			index = ARCHIVE_NULL_POINTER;
		}
		
#ifdef ARCHIVE_USE_TYPES
		WriteType( ARC_EventPointer );
#endif
		
		ArchiveInteger( &index );
		if ( *ev )
		{
			(*ev)->Archive( *this );
		}
	}
}

void Archiver::ArchiveRaw( void *data, size_t size )
{
	ArchiveData( ARC_Raw, data, size );
}

void Archiver::ArchiveString( str * string )
{
	if ( archivemode == ARCHIVE_READ )
	{
		archsize_t	s;
		char		*data;

#ifndef NDEBUG
		CheckRead();
#endif
#ifdef ARCHIVE_USE_TYPES
		CheckType( ARC_String );
#endif

		if ( !fileerror )
		{
			s = ReadSize();
			if ( !fileerror )
			{
				data = new ( std::nothrow ) char[ s + 1 ];
				if ( data )
				{
					if ( s )
					{
						readfile.Read( data, s );
					}
					data[ s ] = 0;

					*string = data;

					delete [] data;
				}
			}
		}
	}
	else
	{
#ifndef NDEBUG
		CheckWrite();
#endif
#ifdef ARCHIVE_USE_TYPES
		WriteType( ARC_String );
#endif
		WriteSize( string->length() );
		gi.FS_Write( string->c_str(), string->length(), file );
	}
}

Class * Archiver::ReadObject( void )
{
	ClassDef	*cls;
	Class		*obj;
	str		classname;
	long		objstart;
	long		endpos;
	int      index;
	size_t	size;
	qboolean isent;
	int      type;
	long long t0, t1;

	t0 = GRS_NOW();
	CheckRead();
	
	type = ReadType();
	if ( ( type != ARC_Object ) && ( type != ARC_Entity ) )
	{
		FileError( "Expecting %s or %s", typenames[ ARC_Object ], typenames[ ARC_Entity ] );
	}
	
	size = ReadSize();
	ArchiveString( &classname );

	t1 = GRS_NOW();
	if ( t0 ) {
		g_restoreSplitUs[ GRS_HEAD ] += t1 - t0;
	}
	t0 = t1;

	cls = getClass( classname.c_str() );
	if ( !cls )
	{
		FileError( "Invalid class %s.", classname.c_str() );
	}
	
	isent = checkInheritance( &Entity::ClassInfo, cls );
	if ( type == ARC_Entity )
	{
		if ( !isent )
		{
			FileError( "Non-Entity class object '%s' saved as an Entity based object.", classname.c_str() );
		}
		
		ArchiveInteger( &level.spawn_entnum );
		ArchiveInteger( &level.spawnflags );
	}
	else if ( isent )
	{
		FileError( "Entity class object '%s' saved as non-Entity based object.", classname.c_str() );
	}
	
	ArchiveInteger( &index );
	objstart = readfile.Pos();

	t1 = GRS_NOW();
	if ( t0 ) {
		g_restoreSplitUs[ GRS_CLASS ] += t1 - t0;
	}
	t0 = t1;

	WIDGET_PROBE("archive", "obj [%d] %s size=%u @0x%lx type=%s\n", index, classname.c_str(),
	           (unsigned)size, (unsigned long)objstart, (type == ARC_Entity) ? "Entity" : "Object");
	
	
	obj = ( Class * )cls->newInstance();

	t1 = GRS_NOW();
	if ( t0 ) {
		g_restoreSplitUs[ GRS_NEW ] += t1 - t0;
	}
	t0 = t1;

	if ( !obj )
	{
		FileError( "Failed to on new instance of class %s.", classname.c_str() );
	}
	else
	{
		obj->Archive( *this );
	}

	t1 = GRS_NOW();
	if ( t0 ) {
		g_restoreSplitUs[ GRS_ARCH ] += t1 - t0;
		g_restoreSplitN[ GRS_ARCH ]++;
	}
	t0 = t1;
	
	if ( !fileerror )
	{
		endpos = readfile.Pos();
		if ( ( endpos - objstart ) > ( long )size )
		{
			FileError( "Object read past end of object's data" );
		}
		else if ( ( endpos - objstart ) < ( long )size )
		{
			FileError( "Object didn't read entire data from file" );
		}
	}

	WIDGET_PROBE("archive", "obj done %s [%d] (%u bytes)\n", classname.c_str(), index,
	           (unsigned)size);
	
	classpointerList.AddObjectAt( index, obj );

	t1 = GRS_NOW();
	if ( t0 ) {
		g_restoreSplitUs[ GRS_ADD ] += t1 - t0;
	}
	
	return obj;
}

void Archiver::ArchiveObject( Class *obj )
{
	str		classname;
	int      index;
	size_t	size;
	qboolean isent;
	
	if ( archivemode == ARCHIVE_READ )
	{
		ClassDef	*cls;
		long		objstart;
		long		endpos;
		int      type;
		
		CheckRead();
		type = ReadType();
		if ( ( type != ARC_Object ) && ( type != ARC_Entity ) )
		{
			FileError( "Expecting %s or %s", typenames[ ARC_Object ], typenames[ ARC_Entity ] );
		}
		
		size = ReadSize();
		ArchiveString( &classname );
		
		cls = getClass( classname.c_str() );
		if ( !cls )
		{
			FileError( "Invalid class %s.", classname.c_str() );
		}
		
		if ( obj->classinfo() != cls )
		{
			FileError( "Archive has a '%s' object, but was expecting a '%s' object.", classname.c_str(), obj->getClassname() );
		}
		
		isent = obj->isSubclassOf( Entity );
		if ( type == ARC_Entity )
		{
			int entnum;
			if ( !isent )
            {
				FileError( "Non-Entity class object '%s' saved as an Entity based object.", classname.c_str() );
            }
			
			ArchiveInteger( &entnum );
			( ( Entity * )obj )->SetEntNum( entnum );
			ArchiveInteger( &level.spawnflags );
		}
		else if ( isent )
		{
			FileError( "Entity class object '%s' saved as non-Entity based object.", classname.c_str() );
		}
		
		ArchiveInteger( &index );
		objstart = readfile.Pos();
		
		obj->Archive( *this );
		
		if ( !fileerror )
		{
			endpos = readfile.Pos();
			if ( ( endpos - objstart ) > ( long )size )
			{
				FileError( "Object read past end of object's data" );
			}
			else if ( ( endpos - objstart ) < ( long )size )
			{
				FileError( "Object didn't read entire data from file" );
			}
		}
		
		classpointerList.AddObjectAt( index, obj );
	}
	else
	{
		long		sizepos;
		long		objstart = 0;
		long		endpos;
		
		assert( obj );
		if ( !obj )
		{
			FileError( "NULL object in WriteObject" );
		}
		
		isent = obj->isSubclassOf( Entity );
		
		CheckWrite();
		if ( isent )
		{
			WriteType( ARC_Entity );
		}
		else
		{
			WriteType( ARC_Object );
		}
		
		sizepos = gi.FS_FTell( file );
		size = 0;
		WriteSize( size );
		
		classname = obj->getClassname();
		ArchiveString( &classname );
		
		if ( isent )
		{
			ArchiveInteger( &( ( Entity * )obj )->entnum );
			ArchiveInteger( &( ( Entity * )obj )->spawnflags );
		}
		
		index = classpointerList.AddUniqueObject( obj );
		ArchiveInteger( &index );
		
		if ( !fileerror )
		{
			objstart = gi.FS_FTell( file );
			obj->Archive( *this );
		}
		
		if ( !fileerror )
		{
			endpos = gi.FS_FTell( file );
			size = endpos - objstart;
			gi.FS_FSeek( file, sizepos, FS_SEEK_SET );
			WriteSize( size );
			
			if ( !fileerror )
			{
				gi.FS_FSeek( file, endpos, FS_SEEK_SET );
			}
		}
	}
}
