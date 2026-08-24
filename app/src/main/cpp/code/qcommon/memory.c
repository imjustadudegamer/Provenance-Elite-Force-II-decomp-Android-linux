

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "q_shared.h"
#include "qcommon.h"

#ifdef __linux__
#  include <unwind.h>
#  include <dlfcn.h>
#endif

#ifndef DEDICATED
#  include "../client/client.h"
#endif

#ifdef __linux__
typedef struct {
	void **cur;
	void **end;
} sysUnwindState_t;

static _Unwind_Reason_Code Sys_ZoneUnwindCb( struct _Unwind_Context *ctx, void *arg ) {
	sysUnwindState_t *st = (sysUnwindState_t *)arg;
	void *pc = (void *)_Unwind_GetIP( ctx );

	if ( pc ) {
		if ( st->cur == st->end ) {
			return _URC_END_OF_STACK;
		}
		*st->cur++ = pc;
	}
	return _URC_NO_REASON;
}
#endif

#define	ZONEID			0x7331
#define ZONEID_CONST	0xC057

void Z_CheckHeap(void);

typedef struct zonedebug_s {
	const char *label;
	const char *file;
	int line;
	int allocSize;
} zonedebug_t;

typedef struct memblock_s {
	size_t	size;			
	struct memblock_s		*next, *prev;
	int     id;				
#ifdef ZONE_DEBUG
	zonedebug_t d;
#endif
} memblock_t;

typedef struct memconstant_s {
	memblock_t b;
	unsigned char mem[ 2 ];
} memconstant_t;

memconstant_t emptystring = { {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {0, 0} };
memconstant_t numberstring[] =
{
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'0', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'1', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'2', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'3', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'4', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'5', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'6', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'7', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'8', 0} },
	{ {sizeof( memconstant_t ), NULL, NULL, ZONEID_CONST}, {'9', 0} },
};

static memblock_t mem_blocks[ TAG_NUM_TOTAL_TAGS ];

const char *Z_EmptyStringPointer( void )
{
	return ( const char * )emptystring.mem;
}

const char *Z_NumberStringPointer( int iNum )
{
	return ( const char * )numberstring[ iNum - '0' ].mem;
}

void Z_Free( void *ptr )
{
	memblock_t *block = ( memblock_t * )( ( byte * )ptr - sizeof( memblock_t ) );

	if( block->id == ZONEID_CONST ) {
		return;
	}

	if( block->id != ZONEID ) {
		Com_Printf("Z_Free bad pointer %p, id=0x%x (expected 0x%x):\n", ptr, block->id, ZONEID);
#ifdef __linux__
		{
			void *frames[32];
			sysUnwindState_t st;
			size_t i, n;

			st.cur = frames;
			st.end = frames + ARRAY_LEN( frames );
			_Unwind_Backtrace( Sys_ZoneUnwindCb, &st );
			n = (size_t)( st.cur - frames );

			for ( i = 0; i < n; i++ ) {
				Dl_info info;

				if ( dladdr( frames[i], &info ) && info.dli_sname ) {
					Com_Printf("  #%d %s(%s+0x%lx)\n", (int)i,
						info.dli_fname ? info.dli_fname : "?", info.dli_sname,
						(unsigned long)((const char*)frames[i] - (const char*)info.dli_saddr) );
				} else if ( dladdr( frames[i], &info ) && info.dli_fbase ) {
					Com_Printf("  #%d %s(+0x%lx)\n", (int)i,
						info.dli_fname ? info.dli_fname : "?",
						(unsigned long)((const char*)frames[i] - (const char*)info.dli_fbase) );
				} else {
					Com_Printf("  #%d %p\n", (int)i, frames[i] );
				}
			}
		}
#endif
		Com_Error( ERR_FATAL, "Z_Free: freed a pointer without ZONEID" );
	}

#ifndef _DEBUG
	if( *( int * )( ( byte * )block + block->size - sizeof( int ) ) != ZONEID ) {
		Com_Error( ERR_FATAL, "Z_Free: memory block wrote past end" );
	}
#endif

	block->next->prev = block->prev;
	block->prev->next = block->next;
	block->prev = block;
	block->next = block;

	free( block );
}

void Z_FreeTags( int tag )
{
	memblock_t *block;
	memblock_t *next;

	for( block = mem_blocks[ tag ].next; block != &mem_blocks[ tag ]; block = next )
	{
		next = block->next;
		Z_Free( ( ( byte * )block + sizeof( memblock_t ) ) );
	}

	mem_blocks[ tag ].prev = &mem_blocks[ tag ];
	mem_blocks[ tag ].next = &mem_blocks[ tag ];
}

#ifdef ZONE_DEBUG
void *Z_TagMallocDebug( int size, int tag, const char *label, const char *file, int line ) {
	int		allocSize;
#else
void *Z_TagMalloc( int size, int tag ) {
#endif
	memblock_t *block;

	if( size <= 0 )
	{
		Com_DPrintf( "Z_TagMalloc, Negative or zero size %d tag %i\n", size, tag );
		return NULL;
	}

	if( tag == TAG_FREE ) {
		Com_Error( ERR_FATAL, "Z_TagMalloc: tried to use a 0 tag" );
	}

#ifdef ZONE_DEBUG
	allocSize = size;
#endif

	size += sizeof( memblock_t );				
#ifndef _DEBUG
	size += sizeof( int );						
#endif
	size = PAD( size, sizeof( intptr_t ) );		

	block = ( memblock_t * )malloc( size );
	if( !block ) {
		Com_Error( ERR_FATAL, "Z_TagMalloc: failed on allocation of %i bytes", size );
		return NULL;
	}
	block->id = ZONEID;
	block->size = size;
	block->next = &mem_blocks[ tag ];
	block->prev = mem_blocks[ tag ].prev;
	block->prev->next = block;
	mem_blocks[ tag ].prev = block;

#ifdef ZONE_DEBUG
	block->d.label = label;
	block->d.file = file;
	block->d.line = line;
	block->d.allocSize = allocSize;
#endif

#ifndef _DEBUG
	*( int * )( ( byte * )block + block->size - sizeof( int ) ) = ZONEID;
#endif

	return ( void * )( ( byte * )block + sizeof( memblock_t ) );
}

void Z_CheckHeap( void )
{
#ifndef _DEBUG
	int k;
	memblock_t *block;

	for( k = 0; k < TAG_NUM_TOTAL_TAGS; k++ )
	{
		for( block = mem_blocks[ k ].next; block != &mem_blocks[ k ]; block = block->next )
		{
			if( *( int * )( ( byte * )block + block->size - 4 ) != ZONEID ) {
				Com_Error( ERR_FATAL, "Z_CheckHeap: memory block wrote past end" );
			}
		}
	}
#endif
}

void Z_TouchMemory( void )
{
	size_t i, j;
	int k;
	int sum;
	(void)sum; 
	int start, end;
	memblock_t *block;

	Z_CheckHeap();
	start = Sys_Milliseconds();

	sum = 0;

	for( k = 0; k < TAG_NUM_TOTAL_TAGS; k++ )
	{
		for( block = mem_blocks[ k ].next; block != &mem_blocks[ k ]; block = block->next )
		{
			j = block->size >> 2;
			for( i = 0; i < j; i += 64 ) {				
				sum += ( ( int * )block )[ i ];
			}
		}
	}

	end = Sys_Milliseconds();

	Com_Printf( "Z_TouchMemory: %i msec\n", end - start );
}

void Z_Meminfo_f( void )
{
	int k;
	size_t totalBlocks;
	size_t totalBytes;
	size_t numBlocks;
	size_t numBytes;
	memblock_t *block;

	totalBlocks = 0;
	totalBytes = 0;

	Com_Printf( "-------------------------------------------\n" );

	for( k = TAG_CONST + 1; k < TAG_NUM_TOTAL_TAGS; k++ )
	{
		numBlocks = 0;
		numBytes = 0;

		for( block = mem_blocks[ k ].next; block != &mem_blocks[ k ]; block = block->next )
		{
			numBlocks++;
			numBytes += block->size;

			if( Cmd_Argc() != 1 ) {
			}
		}

		Com_Printf( "%zu Kbytes in %zu blocks in ", numBytes / 1024, numBlocks );

		switch( k )
		{
		case TAG_BOTLIB:
			Com_Printf( "botlib memory pool.\n" );
			break;
		case TAG_RENDERER:
			Com_Printf( "renderer memory pool.\n" );
			break;
		case TAG_CGAME:
			Com_Printf( "cgame memory pool.\n" );
			break;
		case TAG_GAME:
			Com_Printf( "game memory pool.\n" );
			break;
		case TAG_CLIENT:
			Com_Printf( "client memory pool.\n" );
			break;
		case TAG_TIKI:
			Com_Printf( "tiki memory pool.\n" );
			break;
		case TAG_STRINGS_AND_COMMANDS:
			Com_Printf( "strings and commands memory pool.\n" );
			break;
		case TAG_SOUND:
			Com_Printf( "sound memory pool.\n" );
			break;
		case TAG_STATIC:
			Com_Printf( "static memory pool.\n" );
			break;
		case TAG_STATIC_RENDERER:
			Com_Printf( "static renderer memory pool.\n" );
			break;
		case TAG_TEMP:
			Com_Printf( "temporary memory pool.\n" );
			break;
		default:
			Com_Error( ERR_FATAL, "Z_MemInfo: unknown memory pool\n" );
			break;
		}

		totalBlocks += numBlocks;
		totalBytes += numBytes;
	}

	Com_Printf( "\n%.2f megabytes in %zu blocks in all memory pools\n", ( float )totalBytes / ( 1024.0f * 1024.0f ), totalBlocks );

#ifndef DEDICATED
	if (re.CountTextureMemory) {
		Com_Printf( "\n%.2f megabytes in texture memory\n", ( float )re.CountTextureMemory() / 1024.0f );
		Com_Printf( "\n%.1f megabytes in total allocations\n", ( float )re.CountTextureMemory() + totalBytes - 1 / 1024.0f );
	}
#endif
}

void Z_InitMemory( void ) {
	int k;

	memset( &mem_blocks, 0, sizeof( mem_blocks ) );

	for( k = 0; k < TAG_NUM_TOTAL_TAGS; k++ )
	{
		mem_blocks[ k ].prev = &mem_blocks[ k ];
		mem_blocks[ k ].next = &mem_blocks[ k ];
	}

	Cmd_AddCommand( "meminfo", Z_Meminfo_f );
}

void Z_Shutdown( void ) {
	int k;

	Z_CheckHeap();

	for( k = 0; k < TAG_NUM_TOTAL_TAGS; k++ ) {
		Z_FreeTags( k );
	}
}

#ifdef HUNK_DEBUG
void *Hunk_AllocDebug( int size, ha_pref preference, const char *label, const char *file, int line ) {
#else
void *Hunk_Alloc( int size, ha_pref preference ) {
#endif
	void *ptr;

	ptr = Z_TagMalloc( size, TAG_STATIC );
	if ( ptr && size > 0 ) {
		memset( ptr, 0, size );
	}

	return ptr;
}

void Hunk_Clear( void ) {
	Z_FreeTags( TAG_STATIC );
}

void *Hunk_AllocateTempMemory(int size ) {
	return Z_TagMalloc( size, TAG_TEMP );
}

void Hunk_FreeTempMemory( void *ptr ) {
	Z_Free( ptr );
}

void Hunk_ClearTempMemory( void ) {
	Z_FreeTags( TAG_TEMP );
}

void Hunk_SetMark( void ) {

}

void Com_TouchMemory( void ) {
	Z_TouchMemory();
}

void Com_InitHunkMemory( void ) {
}

#ifdef ZONE_DEBUG
void *Z_MallocDebug( int size, const char *label, const char *file, int line ) {
#else
void *Z_Malloc( int size ) {
#endif
	void	*buf;
	

#ifdef ZONE_DEBUG
	buf = Z_TagMallocDebug( size, TAG_CONST, label, file, line );
#else
	buf = Z_TagMalloc( size, TAG_CONST );
#endif
	if ( buf && size > 0 ) {
		Com_Memset( buf, 0, size );
	}

	return buf;
}
