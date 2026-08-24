
#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include "g_local.h"
#include "class.h"
#include "str.h"

#include "vector.h"

class Entity;

#define ARCHIVE_NULL_POINTER ( -654321 )
#define ARCHIVE_POINTER_VALID ( 0 )
#define ARCHIVE_POINTER_NULL ( ARCHIVE_NULL_POINTER )
#define ARCHIVE_POINTER_SELF_REFERENTIAL ( -123456 )

#define ARCHIVE_WRITE 0
#define ARCHIVE_READ  1

typedef uint32_t archsize_t;

typedef SafePtr<Entity> EntityPtr;

enum
	{
   pointer_fixup_normal,
   pointer_fixup_safe
	};

typedef struct
   {
   void **ptr;
   int  index;
   int  type;
   } pointer_fixup_t;

class FileRead : public Class
	{
	protected:
		str				filename;
      size_t         length;
      byte           *buffer;
      byte           *pos;

	public:
      CLASS_PROTOTYPE( FileRead );

							FileRead();
							~FileRead();
		void				Close( void );
      const char     *Filename( void );
      size_t         Length( void );
      size_t         Pos( void );
      qboolean       Seek( size_t newpos );
      qboolean       Open( const char *name );
      qboolean       Read( void *dest, size_t size );
	};

class Archiver : public Class
	{
   private:
      Container<Class *>         classpointerList;
      Container<pointer_fixup_t *> fixupList;

	protected:
		str				filename;
		qboolean			fileerror;
		fileHandle_t   file;
      FileRead       readfile;
		int				archivemode;
      int            numclassespos;
      qboolean       harderror;

		void				CheckRead( void );
		void				CheckType( int type );
		int            ReadType( void );
		archsize_t		ReadSize( void );
		void				CheckSize( int type, size_t size );
		void				ArchiveData( int type, void *data, size_t size );

		void				CheckWrite( void );
		void				WriteType( int type );
		void				WriteSize( archsize_t size );

	public:
      CLASS_PROTOTYPE( Archiver );

							Archiver();
							~Archiver();
		void				FileError( const char *fmt, ... ) Q_PRINTF_FUNC(2, 3);
		void				Close( void );

		size_t				Pos( void );

		qboolean       Read( const str &name, qboolean file_harderror = true );
		qboolean       Read( const char *name, qboolean file_harderror = true );
		Class				*ReadObject( void );

		qboolean       Create( const str &name, qboolean file_harderror = true );
		qboolean       Create( const char *name, qboolean file_harderror = true );

      qboolean       Loading( void );
      qboolean       Saving( void );
      qboolean       NoErrors( void );

		void  			ArchiveVector( Vector * vec );
		void  			ArchiveQuat( Quat * quat );
		void				ArchiveInteger( int * num );
		void  			ArchiveUnsigned( unsigned * unum);
		void				ArchiveByte( byte * num );
		void				ArchiveChar( char * ch );
		void				ArchiveShort( short * num );
		void           ArchiveUnsignedShort( unsigned short * num );
		void           ArchiveFloat( float * num );
		void			   ArchiveDouble( double * num );
		void  			ArchiveBoolean( qboolean * boolean );
		void  			ArchiveString( str * string );
		void           ArchiveObjectPointer( Class ** ptr );
		void           ArchiveSafePointer( SafePtrBase * ptr );
		void           ArchiveEvent( Event * ev );
		void           ArchiveEventPointer( Event ** ev );
		void  			ArchiveBool( bool * boolean );
		void  			ArchiveVec3( vec3_t vec );
		void  			ArchiveVec4( vec4_t vec );

		void				ArchiveRaw( void *data, size_t size );
		void				ArchiveObject( Class *obj );

	};

inline qboolean Archiver::Read
   (
   const str &name,
   qboolean file_harderror
   )

   {
   return Read( name.c_str(), file_harderror );
   }

inline qboolean Archiver::Create
   (
   const str &name,
   qboolean file_harderror
   )

   {
   return Create( name.c_str(), file_harderror );
   }



inline qboolean Archiver::NoErrors
   (
   void
   )
   {
	return ( !fileerror );
   }

template<>
inline void Container<str>::Archive
	(
   Archiver &arc
	)
	{
   int i, num;

   if ( arc.Loading() )
      {
      ClearObjectList();
      arc.ArchiveInteger( &num );
      Resize( num );
      }
   else
      {
      num = numobjects;
      arc.ArchiveInteger( &num );
      }
   for( i = 1; i <= num; i++ )
      {
      arc.ArchiveString( AddressOfObjectAt( i ) );
      }
	}

template<>
inline void Container<Vector>::Archive
	(
   Archiver &arc
	)
	{
   int i, num;

   if ( arc.Loading() )
      {
      ClearObjectList();
      arc.ArchiveInteger( &num );
      Resize( num );
      }
   else
      {
      num = numobjects;
      arc.ArchiveInteger( &num );
      }
   for( i = 1; i <= num; i++ )
      {
      arc.ArchiveVector( AddressOfObjectAt( i ) );
      }
	}

template<>
inline void Container<int>::Archive
	(
   Archiver &arc
	)
	{
   int i, num;

   if ( arc.Loading() )
      {
      ClearObjectList();
      arc.ArchiveInteger( &num );
      Resize( num );
      }
   else
      {
      num = numobjects;
      arc.ArchiveInteger( &num );
      }
   for( i = 1; i <= num; i++ )
      {
      arc.ArchiveInteger( AddressOfObjectAt( i ) );
      }
	}

template<>
inline void Container<float>::Archive
	(
   Archiver &arc
	)
	{
   int i, num;

   if ( arc.Loading() )
      {
      ClearObjectList();
      arc.ArchiveInteger( &num );
      Resize( num );
      }
   else
      {
      num = numobjects;
      arc.ArchiveInteger( &num );
      }
   for( i = 1; i <= num; i++ )
      {
      arc.ArchiveFloat( AddressOfObjectAt( i ) );
      }
	}




template<>
inline void Container<Class*>::Archive
(
	Archiver &arc
)
{
	int numObjects = numobjects ;
	arc.ArchiveInteger( &numObjects );

	if ( arc.Loading() )	
		Resize( numObjects );

	for ( int objectIdx = 1; objectIdx <= numObjects; ++objectIdx )
	{
		arc.ArchiveObjectPointer( AddressOfObjectAt( objectIdx ) );
		arc.ArchiveObject( ObjectAt( objectIdx ) );
	}
}



template<>
inline void Container<Class>::Archive
(
	Archiver &arc
)
{
	int numObjects = numobjects ;
	arc.ArchiveInteger( &numObjects );

	if ( arc.Loading() )	
		Resize( numObjects );

	for ( int objectIdx = 1; objectIdx <= numObjects; ++objectIdx )
	{
		arc.ArchiveObject( AddressOfObjectAt( objectIdx ) );
	}
}


template<>
inline void Container<Entity*>::Archive
(
	Archiver &arc
)
{
	int numObjects = numobjects ;
	arc.ArchiveInteger( &numObjects );

	if ( arc.Loading() )
		Resize( numObjects );

	for ( int objectIdx = 1; objectIdx <= numObjects; ++objectIdx )
	{
		Entity** entity = AddressOfObjectAt( objectIdx );
		arc.ArchiveObjectPointer( (Class**)entity );
	}
}

template<>
inline void Container<EntityPtr>::Archive
(
	Archiver &arc
)
{
	int i;
	int numEntries;
	EntityPtr eptr;
	EntityPtr *eptrptr;

	if ( arc.Saving() )
	{
		numEntries = NumObjects();
		arc.ArchiveInteger( &numEntries );
		for ( i = 1 ; i <= numEntries ; i++ )
		{
			eptr = ObjectAt( i );
			arc.ArchiveSafePointer( &eptr );			
		}
	}
	else
	{
		arc.ArchiveInteger( &numEntries );

		ClearObjectList();
		Resize( numEntries );

		for ( i = 1 ; i <= numEntries ; i++ )
		{
			AddObject( eptr );
			eptrptr = &ObjectAt( i );
			arc.ArchiveSafePointer( eptrptr );		
		}
	}
}



#define ArchiveEnum( thing, type )  \
   {                                \
   int tempInt;                     \
                                    \
   tempInt = ( int )( thing );      \
   arc.ArchiveInteger( &tempInt );  \
   ( thing ) = ( type )tempInt;     \
   }


#endif 
