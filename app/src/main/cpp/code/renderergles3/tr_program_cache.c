#include "tr_program_cache.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
	unsigned	magic;
	unsigned	version;
	unsigned	headerBytes;
	unsigned	keyVersion;
	unsigned	entryCount;
	unsigned	blobBytes;
	unsigned	blobHash;
	unsigned	sourceHash;
	unsigned	definesHash;
	unsigned	glesMajor;
	unsigned	glesMinor;
	unsigned	formatCount;
	unsigned	formatList[PROGCACHE_PROGCACHE_MAX_FORMATS];
	char		glVersion[PROGCACHE_PROGCACHE_MAX_FP];
	char		glRenderer[PROGCACHE_PROGCACHE_MAX_FP];
	char		glVendor[PROGCACHE_PROGCACHE_MAX_FP];
} progcacheHeader_t;

typedef struct {
	unsigned	magic;
	unsigned	entryBytes;		
	unsigned	defines;
	unsigned	nameHash;
	unsigned	vsHash;
	unsigned	fsHash;
	unsigned	ifaceHash;
	unsigned	ifaceHash2;		
	unsigned	format;
	unsigned	binaryBytes;
	unsigned	binaryHash;
	unsigned	buildUsec;
	char		name[PROGCACHE_PROGCACHE_MAX_NAME];
} progcacheEntryHdr_t;

typedef struct {
	char		name[PROGCACHE_PROGCACHE_MAX_NAME];
	unsigned	defines;
	unsigned	nameHash;
	unsigned	vsHash;
	unsigned	fsHash;
	unsigned	ifaceHash;
	unsigned	ifaceHash2;		
	unsigned	format;
	unsigned	binaryBytes;
	unsigned	binaryHash;
	unsigned	buildUsec;
	const unsigned char	*binary;	
	unsigned	program;			
	int			fromFile;
	int			dirty;				
} progcacheEntry_t;

static const progcacheProgCacheIO_t	*s_io;
static const progcacheProgCacheGl_t	*s_gl;
static int			s_mode;
static int			s_engaged;
static int			s_loadDone;
static int			s_writeDisabled;
static int			s_writeFailures;
static int			s_state;

static char			s_glVersion[PROGCACHE_PROGCACHE_MAX_FP];
static char			s_glRenderer[PROGCACHE_PROGCACHE_MAX_FP];
static char			s_glVendor[PROGCACHE_PROGCACHE_MAX_FP];
static unsigned		s_glesMajor, s_glesMinor;
static unsigned		s_sourceHash, s_definesHash;
static unsigned		s_formatCount;
static unsigned		s_formats[PROGCACHE_PROGCACHE_MAX_FORMATS];

static unsigned char	*s_blob;
static int			s_blobBytes;
static progcacheEntry_t	*s_entries;
static int			s_numEntries;
static int			s_tableCap;
static unsigned		s_dirty;
static progcacheProgCacheStats_t s_stats;

unsigned PROGCACHE_ProgCache_Hash( const void *data, int len ) {
	const unsigned char	*p = (const unsigned char *)data;
	unsigned			h = 2166136261u;
	int					i;

	if ( !p || len <= 0 ) {
		return 0;
	}
	for ( i = 0; i < len; i++ ) {
		h ^= (unsigned)p[i];
		h *= 16777619u;
	}
	return h;
}

unsigned PROGCACHE_ProgCache_HashStr( const char *s ) {
	if ( !s ) {
		return 0;
	}
	return PROGCACHE_ProgCache_Hash( s, (int)strlen( s ) );
}

unsigned PROGCACHE_ProgCache_HashSeed( void ) {
	return 2166136261u;
}

unsigned PROGCACHE_ProgCache_Fold( unsigned h, const void *data, int len ) {
	const unsigned char	*p = (const unsigned char *)data;
	int					i;

	if ( !p || len <= 0 ) {
		return h;
	}
	for ( i = 0; i < len; i++ ) {
		h ^= (unsigned)p[i];
		h *= 16777619u;
	}
	return h;
}

unsigned PROGCACHE_ProgCache_FoldStr( unsigned h, const char *s ) {
	unsigned n = s ? (unsigned)strlen( s ) : 0u;
	unsigned i;

	h ^= n;
	h *= 16777619u;
	for ( i = 0; i < n; i++ ) {
		h ^= (unsigned char)s[i];
		h *= 16777619u;
	}
	return h;
}

static unsigned PROGCACHE_Mix( unsigned h, unsigned v ) {
	h ^= v;
	h *= 16777619u;
	return h;
}

#define PROGCACHE_MIX2_SEED  0x9E3779B9u
#define PROGCACHE_MIX2_PRIME 2654435761u

static unsigned PROGCACHE_Mix2( unsigned h, unsigned v ) {
	h ^= v;
	h *= PROGCACHE_MIX2_PRIME;
	return h;
}

static unsigned PROGCACHE_Mix2Str( unsigned h, const char *s ) {
	unsigned n = s ? (unsigned)strlen( s ) : 0u;
	unsigned i;

	h = PROGCACHE_Mix2( h, n );
	for ( i = 0; i < n; i++ ) {
		h ^= (unsigned char)s[i];
		h *= PROGCACHE_MIX2_PRIME;
	}
	return h;
}

static unsigned PROGCACHE_MixStr( unsigned h, const char *s ) {
	return PROGCACHE_ProgCache_FoldStr( h, s );
}

static void PROGCACHE_CopyField( char *dst, int dstSize, const char *src ) {
	if ( dstSize <= 0 ) {
		return;
	}
	if ( !src ) {
		dst[0] = '\0';
		return;
	}
	strncpy( dst, src, (size_t)( dstSize - 1 ) );
	dst[dstSize - 1] = '\0';
}

static int PROGCACHE_Now( void ) {
	if ( s_io && s_io->Microseconds ) {
		return s_io->Microseconds();
	}
	return 0;
}

static int PROGCACHE_DrainErrors( void ) {
	int	err, n = 0;

	if ( !s_gl || !s_gl->GetError ) {
		return 0;
	}
	while ( ( err = s_gl->GetError() ) != 0 ) {
		n++;
		if ( n >= 16 ) {
			break;
		}
	}
	s_stats.glerr += (unsigned)n;
	return n;
}

static int PROGCACHE_FormatIsLive( unsigned format ) {
	unsigned i;

	for ( i = 0; i < s_formatCount && i < PROGCACHE_PROGCACHE_MAX_FORMATS; i++ ) {
		if ( s_formats[i] == format ) {
			return 1;
		}
	}
	return 0;
}

int PROGCACHE_ProgCache_On( void ) {
	return s_engaged;
}

int PROGCACHE_ProgCache_Now( void ) {
	return s_engaged ? PROGCACHE_Now() : 0;
}

#define PROGCACHE_IFACE_MAX_ITEMS  256
#define PROGCACHE_IFACE_NAME_BYTES 256

static int PROGCACHE_InterfaceHash( unsigned program, unsigned *out, unsigned *out2 ) {
	int			nAttrib = 0, nUnif = 0, i, len, size;
	unsigned	type, h, h2;
	char		name[PROGCACHE_IFACE_NAME_BYTES];

	if ( !s_gl || !s_gl->GetProgramiv || !s_gl->GetActiveAttrib
		 || !s_gl->GetActiveUniform || !s_gl->GetAttribLocation
		 || !s_gl->GetUniformLocation ) {
		return 0;
	}
	if ( out ) {
		*out = 0;
	}
	if ( out2 ) {
		*out2 = 0;
	}

	s_gl->GetProgramiv( program, PROGCACHE_GL_ACTIVE_ATTRIBUTES, &nAttrib );
	s_gl->GetProgramiv( program, PROGCACHE_GL_ACTIVE_UNIFORMS, &nUnif );
	if ( nAttrib < 0 || nUnif < 0
		 || nAttrib > PROGCACHE_IFACE_MAX_ITEMS || nUnif > PROGCACHE_IFACE_MAX_ITEMS ) {
		return 0;
	}

	h = 2166136261u;
	h2 = PROGCACHE_MIX2_SEED;
	h = PROGCACHE_Mix( h, (unsigned)nAttrib );
	h2 = PROGCACHE_Mix2( h2, (unsigned)nAttrib );

	for ( i = 0; i < nAttrib; i++ ) {
		name[0] = '\0';
		len = 0; size = 0; type = 0;
		s_gl->GetActiveAttrib( program, (unsigned)i, PROGCACHE_IFACE_NAME_BYTES,
							   &len, &size, &type, name );
		name[PROGCACHE_IFACE_NAME_BYTES - 1] = '\0';
		if ( len >= PROGCACHE_IFACE_NAME_BYTES - 1 ) {
			return 0;	
		}
		h = PROGCACHE_MixStr( h, name );
		h = PROGCACHE_Mix( h, (unsigned)size );
		h = PROGCACHE_Mix( h, type );
		h = PROGCACHE_Mix( h, (unsigned)s_gl->GetAttribLocation( program, name ) );
		h2 = PROGCACHE_Mix2Str( h2, name );
		h2 = PROGCACHE_Mix2( h2, (unsigned)size );
		h2 = PROGCACHE_Mix2( h2, type );
		h2 = PROGCACHE_Mix2( h2, (unsigned)s_gl->GetAttribLocation( program, name ) );
	}

	h = PROGCACHE_Mix( h, (unsigned)nUnif );
	h2 = PROGCACHE_Mix2( h2, (unsigned)nUnif );
	for ( i = 0; i < nUnif; i++ ) {
		name[0] = '\0';
		len = 0; size = 0; type = 0;
		s_gl->GetActiveUniform( program, (unsigned)i, PROGCACHE_IFACE_NAME_BYTES,
								&len, &size, &type, name );
		name[PROGCACHE_IFACE_NAME_BYTES - 1] = '\0';
		if ( len >= PROGCACHE_IFACE_NAME_BYTES - 1 ) {
			return 0;
		}
		h = PROGCACHE_MixStr( h, name );
		h = PROGCACHE_Mix( h, (unsigned)size );
		h = PROGCACHE_Mix( h, type );
		h = PROGCACHE_Mix( h, (unsigned)s_gl->GetUniformLocation( program, name ) );
		h2 = PROGCACHE_Mix2Str( h2, name );
		h2 = PROGCACHE_Mix2( h2, (unsigned)size );
		h2 = PROGCACHE_Mix2( h2, type );
		h2 = PROGCACHE_Mix2( h2, (unsigned)s_gl->GetUniformLocation( program, name ) );
	}

	if ( out ) {
		*out = h;
	}
	if ( out2 ) {
		*out2 = h2;
	}
	return 1;
}

static int PROGCACHE_AttribContract( unsigned program ) {
	static const char *const names[PROGCACHE_ATTR_COUNT] = {
		"a_Position", "a_Normal", "a_Color", "a_TexCoord0", "a_TexCoord1"
	};
	int i, loc;

	for ( i = 0; i < PROGCACHE_ATTR_COUNT; i++ ) {
		loc = s_gl->GetAttribLocation( program, names[i] );
		if ( loc >= 0 && loc != i ) {
			return 0;
		}
	}
	return 1;
}

static int PROGCACHE_ProbeCapability( void ) {
	int	n, got, i;

	if ( !s_gl || !s_gl->GetBinaryFormatCount || !s_gl->GetBinaryFormats ) {
		return 0;
	}
	PROGCACHE_DrainErrors();
	n = s_gl->GetBinaryFormatCount();
	PROGCACHE_DrainErrors();
	if ( n <= 0 || n > PROGCACHE_PROGCACHE_MAX_FORMATS ) {
		return 0;	
	}
	memset( s_formats, 0, sizeof( s_formats ) );
	got = s_gl->GetBinaryFormats( s_formats, PROGCACHE_PROGCACHE_MAX_FORMATS );
	PROGCACHE_DrainErrors();
	if ( got < n ) {
		return 0;
	}
	for ( i = n; i < PROGCACHE_PROGCACHE_MAX_FORMATS; i++ ) {
		s_formats[i] = 0;
	}
	s_formatCount = (unsigned)n;
	return 1;
}

static void PROGCACHE_FreeTable( void ) {
	if ( s_blob ) {
		free( s_blob );
		s_blob = NULL;
	}
	if ( s_entries ) {
		free( s_entries );
		s_entries = NULL;
	}
	s_blobBytes = 0;
	s_numEntries = 0;
	s_tableCap = 0;
}

static progcacheEntry_t *PROGCACHE_FindEntry( unsigned nameHash, unsigned defines,
									unsigned vsHash, unsigned fsHash ) {
	int i;

	for ( i = 0; i < s_numEntries; i++ ) {
		progcacheEntry_t *e = &s_entries[i];
		if ( e->defines == defines && e->nameHash == nameHash
			 && e->vsHash == vsHash && e->fsHash == fsHash ) {
			return e;
		}
	}
	return NULL;
}

static progcacheEntry_t *PROGCACHE_AddEntry( void ) {
	if ( s_numEntries >= PROGCACHE_PROGCACHE_MAX_ENTRIES ) {
		return NULL;
	}
	if ( s_numEntries >= s_tableCap ) {
		int			cap = s_tableCap ? s_tableCap * 2 : 64;
		progcacheEntry_t	*grown;

		if ( cap > PROGCACHE_PROGCACHE_MAX_ENTRIES ) {
			cap = PROGCACHE_PROGCACHE_MAX_ENTRIES;
		}
		grown = (progcacheEntry_t *)realloc( s_entries, (size_t)cap * sizeof( progcacheEntry_t ) );
		if ( !grown ) {
			return NULL;
		}
		s_entries = grown;
		s_tableCap = cap;
	}
	memset( &s_entries[s_numEntries], 0, sizeof( progcacheEntry_t ) );
	return &s_entries[s_numEntries++];
}

static int PROGCACHE_ParseBlob( const unsigned char *blob, int blobBytes, unsigned entryCount ) {
	int	off = 0;
	unsigned	i;

	for ( i = 0; i < entryCount; i++ ) {
		progcacheEntryHdr_t	h;
		progcacheEntry_t		*e;
		int				padded;
		unsigned		nameHash;

		if ( off < 0 || off + (int)sizeof( progcacheEntryHdr_t ) > blobBytes ) {
			break;
		}
		memcpy( &h, blob + off, sizeof( h ) );
		if ( h.magic != PROGCACHE_PROGCACHE_ENTRY_MAGIC ) {
			break;
		}
		padded = (int)h.binaryBytes;
		if ( padded <= 0 || padded > (int)PROGCACHE_PROGCACHE_MAX_ENTRY ) {
			break;
		}
		padded = ( padded + 3 ) & ~3;
		if ( off + (int)sizeof( progcacheEntryHdr_t ) + padded > blobBytes ) {
			break;
		}
		if ( h.entryBytes != (unsigned)( sizeof( progcacheEntryHdr_t ) - 2u * sizeof( unsigned ) + (unsigned)padded ) ) {
			break;
		}
		if ( h.binaryHash != PROGCACHE_ProgCache_Hash( blob + off + sizeof( progcacheEntryHdr_t ),
												  (int)h.binaryBytes ) ) {
			break;
		}
		nameHash = PROGCACHE_ProgCache_HashStr( h.name );
		if ( nameHash != h.nameHash ) {
			break;
		}
		if ( !PROGCACHE_FormatIsLive( h.format ) ) {
			break;
		}

		e = PROGCACHE_AddEntry();
		if ( !e ) {
			break;
		}
		PROGCACHE_CopyField( e->name, (int)sizeof( e->name ), h.name );
		e->defines = h.defines;
		e->nameHash = h.nameHash;
		e->vsHash = h.vsHash;
		e->fsHash = h.fsHash;
		e->ifaceHash = h.ifaceHash;
		e->ifaceHash2 = h.ifaceHash2;
		e->format = h.format;
		e->binaryBytes = h.binaryBytes;
		e->binaryHash = h.binaryHash;
		e->buildUsec = h.buildUsec;
		e->binary = blob + off + sizeof( progcacheEntryHdr_t );
		e->program = 0;
		e->fromFile = 1;
		e->dirty = 0;

		off += (int)sizeof( progcacheEntryHdr_t ) + padded;
	}
	return s_numEntries;
}

void PROGCACHE_ProgCache_Init( const progcacheProgCacheIO_t *io, const progcacheProgCacheGl_t *gl, int mode ) {
	PROGCACHE_FreeTable();
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_io = io;
	s_gl = gl;
	s_mode = ( mode > 0 ) ? 1 : 0;
	s_loadDone = 0;
	s_writeDisabled = 0;
	s_writeFailures = 0;
	s_dirty = 0;
	s_formatCount = 0;
	s_sourceHash = s_definesHash = 0;
	s_glesMajor = s_glesMinor = 0;
	s_glVersion[0] = s_glRenderer[0] = s_glVendor[0] = '\0';
	s_state = PROGCACHE_PC_OFF;

	if ( !s_mode ) {
		s_engaged = 0;
		return;
	}
	s_engaged = PROGCACHE_ProbeCapability();
	s_state = s_engaged ? PROGCACHE_PC_NOFILE : PROGCACHE_PC_UNSUPPORTED;
	s_stats.loadState = s_state;
}

void PROGCACHE_ProgCache_Load( const progcacheProgCacheKey_t *key ) {
	unsigned char	*file = NULL;
	long			len;
	void			*fh = NULL;
	int				got;
	progcacheHeader_t	hdr;

	if ( !s_engaged || s_loadDone ) {
		return;
	}
	s_loadDone = 1;
	s_stats.loadState = s_state;

	if ( key ) {
		PROGCACHE_CopyField( s_glVersion, (int)sizeof( s_glVersion ), key->glVersion );
		PROGCACHE_CopyField( s_glRenderer, (int)sizeof( s_glRenderer ), key->glRenderer );
		PROGCACHE_CopyField( s_glVendor, (int)sizeof( s_glVendor ), key->glVendor );
		s_glesMajor = key->glesMajor;
		s_glesMinor = key->glesMinor;
		s_sourceHash = key->sourceHash;
		s_definesHash = key->definesHash;
	}

	if ( !s_io || !s_io->Open || !s_io->Read || !s_io->Close ) {
		s_engaged = 0;
		s_state = PROGCACHE_PC_UNSUPPORTED;
		s_stats.loadState = s_state;
		return;
	}

	len = s_io->Open( s_io->ud, PROGCACHE_PROGCACHE_FILE, &fh );
	if ( len <= 0 || !fh ) {
		s_state = PROGCACHE_PC_NOFILE;
		s_stats.loadState = s_state;
		return;
	}
	if ( len < (long)sizeof( progcacheHeader_t ) || len > (long)( PROGCACHE_PROGCACHE_MAX_BYTES + sizeof( progcacheHeader_t ) ) ) {
		s_io->Close( s_io->ud, fh );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}

	file = (unsigned char *)malloc( (size_t)len );
	if ( !file ) {
		s_io->Close( s_io->ud, fh );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}
	got = s_io->Read( s_io->ud, fh, file, (int)len );
	s_io->Close( s_io->ud, fh );
	if ( got != (int)len ) {
		free( file );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}

	memcpy( &hdr, file, sizeof( hdr ) );

	if ( hdr.magic != PROGCACHE_PROGCACHE_MAGIC
		 || hdr.version != PROGCACHE_PROGCACHE_VERSION
		 || hdr.headerBytes != (unsigned)sizeof( progcacheHeader_t ) ) {
		free( file );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}
	if ( hdr.keyVersion != PROGCACHE_PROGCACHE_KEY_VERSION
		 || hdr.sourceHash != s_sourceHash
		 || hdr.definesHash != s_definesHash
		 || hdr.glesMajor != s_glesMajor
		 || hdr.glesMinor != s_glesMinor
		 || hdr.formatCount != s_formatCount
		 || hdr.glVersion[sizeof( hdr.glVersion ) - 1] != '\0'
		 || hdr.glRenderer[sizeof( hdr.glRenderer ) - 1] != '\0'
		 || hdr.glVendor[sizeof( hdr.glVendor ) - 1] != '\0'
		 || strcmp( hdr.glVersion, s_glVersion ) != 0
		 || strcmp( hdr.glRenderer, s_glRenderer ) != 0
		 || strcmp( hdr.glVendor, s_glVendor ) != 0 ) {
		free( file );
		s_state = PROGCACHE_PC_FOREIGN;
		s_stats.loadState = s_state;
		return;
	}
	{
		unsigned i;

		for ( i = 0; i < PROGCACHE_PROGCACHE_MAX_FORMATS; i++ ) {
			if ( hdr.formatList[i] != s_formats[i] ) {
				free( file );
				s_state = PROGCACHE_PC_FOREIGN;
				s_stats.loadState = s_state;
				return;
			}
		}
	}

	if ( hdr.entryCount > PROGCACHE_PROGCACHE_MAX_ENTRIES
		 || hdr.blobBytes == 0
		 || (long)sizeof( progcacheHeader_t ) + (long)hdr.blobBytes != len ) {
		free( file );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}
	if ( hdr.blobHash != PROGCACHE_ProgCache_Hash( file + sizeof( progcacheHeader_t ),
											 (int)hdr.blobBytes ) ) {
		free( file );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}

	s_blob = (unsigned char *)malloc( (size_t)hdr.blobBytes );
	if ( !s_blob ) {
		free( file );
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}
	memcpy( s_blob, file + sizeof( progcacheHeader_t ), (size_t)hdr.blobBytes );
	s_blobBytes = (int)hdr.blobBytes;
	s_stats.fileBytes = (unsigned)hdr.blobBytes;
	free( file );

	if ( PROGCACHE_ParseBlob( s_blob, s_blobBytes, hdr.entryCount ) != (int)hdr.entryCount ) {
		PROGCACHE_FreeTable();
		s_state = PROGCACHE_PC_CORRUPT;
		s_stats.loadState = s_state;
		return;
	}

	s_stats.entries = (unsigned)s_numEntries;
	s_state = PROGCACHE_PC_LOADED;
	s_stats.loadState = s_state;
	s_engaged = 1;
}

void PROGCACHE_ProgCache_HintLink( unsigned program ) {
	if ( !s_engaged || !program || !s_gl || !s_gl->ProgramParameteri ) {
		return;
	}
	s_gl->ProgramParameteri( program, PROGCACHE_GL_PROGRAM_BINARY_RETRIEVABLE_HINT, PROGCACHE_GL_TRUE );
}

int PROGCACHE_ProgCache_TryRestore( const char *name, unsigned defines,
							   unsigned vsHash, unsigned fsHash,
							   unsigned *programOut, unsigned *savedUsecOut,
							   unsigned *restoreUsecOut ) {
	progcacheEntry_t	*e;
	unsigned	nameHash, program;
	int			link = 0, t0, usec;
	unsigned	iface = 0, iface2 = 0;

	if ( programOut ) {
		*programOut = 0;
	}
	if ( savedUsecOut ) {
		*savedUsecOut = 0;
	}
	if ( restoreUsecOut ) {
		*restoreUsecOut = 0;
	}
	if ( !s_engaged || s_state != PROGCACHE_PC_LOADED || !name ) {
		return 0;
	}

	nameHash = PROGCACHE_ProgCache_HashStr( name );
	e = PROGCACHE_FindEntry( nameHash, defines, vsHash, fsHash );
	if ( !e || !e->binary || e->binaryBytes == 0 ) {
		return 0;	
	}

	t0 = PROGCACHE_Now();
	PROGCACHE_DrainErrors();

	program = s_gl->CreateProgram();
	if ( !program ) {
		PROGCACHE_DrainErrors();
		s_stats.bad++;
		return 0;
	}

	s_gl->ProgramBinary( program, e->format, e->binary, (int)e->binaryBytes );
	if ( PROGCACHE_DrainErrors() != 0 ) {
		s_gl->DeleteProgram( program );
		s_stats.bad++;
		return 0;
	}

	s_gl->GetProgramiv( program, PROGCACHE_GL_LINK_STATUS, &link );
	if ( !link ) {
		s_gl->DeleteProgram( program );
		s_stats.bad++;
		return 0;
	}

	if ( !PROGCACHE_InterfaceHash( program, &iface, &iface2 )
		 || iface != e->ifaceHash || iface2 != e->ifaceHash2 ) {
		s_gl->DeleteProgram( program );
		s_stats.bad++;
		return 0;
	}
	if ( !PROGCACHE_AttribContract( program ) ) {
		s_gl->DeleteProgram( program );
		s_stats.bad++;
		return 0;
	}

	usec = PROGCACHE_Now() - t0;
	if ( usec < 0 ) {
		usec = 0;
	}

	e->program = program;
	s_stats.hits++;
	s_stats.savedUsec += e->buildUsec;
	s_stats.restoreUsec += (unsigned)usec;

	if ( programOut ) {
		*programOut = program;
	}
	if ( savedUsecOut ) {
		*savedUsecOut = e->buildUsec;
	}
	if ( restoreUsecOut ) {
		*restoreUsecOut = (unsigned)usec;
	}
	return 1;
}

void PROGCACHE_ProgCache_NoteCompiled( const char *name, unsigned defines,
								  unsigned vsHash, unsigned fsHash,
								  unsigned program, unsigned buildUsec ) {
	progcacheEntry_t	*e;
	unsigned	nameHash;

	if ( !s_engaged || !name ) {
		return;
	}
	nameHash = PROGCACHE_ProgCache_HashStr( name );
	e = PROGCACHE_FindEntry( nameHash, defines, vsHash, fsHash );
	if ( !e ) {
		e = PROGCACHE_AddEntry();
		if ( !e ) {
			return;
		}
	}
	PROGCACHE_CopyField( e->name, (int)sizeof( e->name ), name );
	e->defines = defines;
	e->nameHash = nameHash;
	e->vsHash = vsHash;
	e->fsHash = fsHash;
	e->ifaceHash = 0;
	e->ifaceHash2 = 0;
	e->format = 0;
	e->binaryBytes = 0;
	e->binaryHash = 0;
	e->buildUsec = buildUsec;
	e->binary = NULL;
	e->program = program;
	e->fromFile = 0;
	if ( !e->dirty ) {
		e->dirty = 1;
		s_dirty++;
	}
	s_stats.miss++;
	s_stats.coldUsec += buildUsec;
	s_stats.entries = (unsigned)s_numEntries;
}

static int PROGCACHE_Append( unsigned char **buf, int *cap, int *used,
						const void *data, int len ) {
	if ( len <= 0 ) {
		return 1;
	}
	if ( *used + len > (int)PROGCACHE_PROGCACHE_MAX_BYTES ) {
		return 0;
	}
	if ( *used + len > *cap ) {
		int				ncap = *cap ? *cap : 65536;
		unsigned char	*grown;

		while ( ncap < *used + len ) {
			ncap *= 2;
		}
		grown = (unsigned char *)realloc( *buf, (size_t)ncap );
		if ( !grown ) {
			return 0;
		}
		*buf = grown;
		*cap = ncap;
	}
	memcpy( *buf + *used, data, (size_t)len );
	*used += len;
	return 1;
}

int PROGCACHE_ProgCache_Save( void ) {
	unsigned char	*blob = NULL;
	unsigned char	*image = NULL;
	int				cap = 0, used = 0;
	int				stored = 0, i, total;
	progcacheHeader_t	hdr;
	int				t0, usec;

	if ( !s_engaged || s_writeDisabled ) {
		return 0;
	}
	if ( s_dirty == 0 && s_state == PROGCACHE_PC_LOADED ) {
		return 0;
	}
	t0 = PROGCACHE_Now();

	for ( i = 0; i < s_numEntries; i++ ) {
		progcacheEntry_t		*e = &s_entries[i];
		progcacheEntryHdr_t	eh;
		const unsigned char	*binary = NULL;
		unsigned char	*fetched = NULL;
		unsigned		binaryBytes = 0, ifaceHash = 0, ifaceHash2 = 0, fmt = 0;
		int				padded;

		memset( &eh, 0, sizeof( eh ) );

		if ( e->dirty && e->program && s_gl ) {
			int	len = 0, got = 0;
			unsigned	format = 0;

			PROGCACHE_DrainErrors();
			s_gl->GetProgramiv( e->program, PROGCACHE_GL_PROGRAM_BINARY_LENGTH, &len );
			if ( PROGCACHE_DrainErrors() != 0 || len <= 0
				 || len > (int)PROGCACHE_PROGCACHE_MAX_ENTRY ) {
				continue;
			}
			fetched = (unsigned char *)malloc( (size_t)len );
			if ( !fetched ) {
				continue;
			}
			s_gl->GetProgramBinary( e->program, len, &got, &format, fetched );
			if ( PROGCACHE_DrainErrors() != 0 || got <= 0 || got > len
				 || !PROGCACHE_FormatIsLive( format ) ) {
				free( fetched );
				continue;
			}
			if ( !PROGCACHE_InterfaceHash( e->program, &ifaceHash, &ifaceHash2 ) ) {
				free( fetched );
				continue;
			}
			binary = fetched;
			binaryBytes = (unsigned)got;
			fmt = format;
		} else if ( !e->dirty && e->fromFile && e->binary && e->binaryBytes ) {
			binary = e->binary;
			binaryBytes = e->binaryBytes;
			ifaceHash = e->ifaceHash;
			ifaceHash2 = e->ifaceHash2;
			fmt = e->format;
		} else {
			continue;
		}

		if ( binaryBytes == 0 || binaryBytes > PROGCACHE_PROGCACHE_MAX_ENTRY ) {
			free( fetched );
			continue;
		}

		padded = (int)( ( binaryBytes + 3u ) & ~3u );

		eh.magic = PROGCACHE_PROGCACHE_ENTRY_MAGIC;
		eh.entryBytes = (unsigned)( sizeof( progcacheEntryHdr_t ) - sizeof( unsigned ) * 2 + padded );
		eh.defines = e->defines;
		eh.nameHash = e->nameHash;
		eh.vsHash = e->vsHash;
		eh.fsHash = e->fsHash;
		eh.ifaceHash = ifaceHash;
		eh.ifaceHash2 = ifaceHash2;
		eh.format = fmt;
		eh.binaryBytes = binaryBytes;
		eh.binaryHash = PROGCACHE_ProgCache_Hash( binary, (int)binaryBytes );
		eh.buildUsec = e->buildUsec;
		PROGCACHE_CopyField( eh.name, (int)sizeof( eh.name ), e->name );

		if ( !PROGCACHE_Append( &blob, &cap, &used, &eh, (int)sizeof( eh ) )
			 || !PROGCACHE_Append( &blob, &cap, &used, binary, (int)binaryBytes ) ) {
			free( fetched );
			free( blob );
			return 0;
		}
		{
			static const unsigned char zero[4] = { 0, 0, 0, 0 };
			if ( !PROGCACHE_Append( &blob, &cap, &used, zero, padded - (int)binaryBytes ) ) {
				free( fetched );
				free( blob );
				return 0;
			}
		}
		free( fetched );
		stored++;
	}

	if ( stored == 0 ) {
		free( blob );
		return 0;
	}

	memset( &hdr, 0, sizeof( hdr ) );
	hdr.magic = PROGCACHE_PROGCACHE_MAGIC;
	hdr.version = PROGCACHE_PROGCACHE_VERSION;
	hdr.headerBytes = (unsigned)sizeof( hdr );
	hdr.keyVersion = PROGCACHE_PROGCACHE_KEY_VERSION;
	hdr.entryCount = (unsigned)stored;
	hdr.blobBytes = (unsigned)used;
	hdr.blobHash = PROGCACHE_ProgCache_Hash( blob, used );
	hdr.sourceHash = s_sourceHash;
	hdr.definesHash = s_definesHash;
	hdr.glesMajor = s_glesMajor;
	hdr.glesMinor = s_glesMinor;
	hdr.formatCount = s_formatCount;
	for ( i = 0; i < PROGCACHE_PROGCACHE_MAX_FORMATS; i++ ) {
		hdr.formatList[i] = s_formats[i];
	}
	PROGCACHE_CopyField( hdr.glVersion, (int)sizeof( hdr.glVersion ), s_glVersion );
	PROGCACHE_CopyField( hdr.glRenderer, (int)sizeof( hdr.glRenderer ), s_glRenderer );
	PROGCACHE_CopyField( hdr.glVendor, (int)sizeof( hdr.glVendor ), s_glVendor );

	total = (int)sizeof( hdr ) + used;
	image = (unsigned char *)malloc( (size_t)total );
	if ( !image ) {
		free( blob );
		return 0;
	}
	memcpy( image, &hdr, sizeof( hdr ) );
	memcpy( image + sizeof( hdr ), blob, (size_t)used );
	free( blob );

	if ( !s_io || !s_io->Write
		 || !s_io->Write( s_io->ud, PROGCACHE_PROGCACHE_FILE, image, total ) ) {
		free( image );
		s_writeFailures++;
		if ( s_writeFailures >= 2 ) {
			s_writeDisabled = 1;
		}
		s_state = PROGCACHE_PC_WRITEFAIL;
		return 0;
	}

	PROGCACHE_FreeTable();
	s_entries = NULL;
	s_numEntries = 0;
	s_tableCap = 0;
	s_blob = (unsigned char *)malloc( (size_t)used );
	if ( s_blob ) {
		memcpy( s_blob, image + sizeof( hdr ), (size_t)used );
		s_blobBytes = used;
		if ( PROGCACHE_ParseBlob( s_blob, s_blobBytes, hdr.entryCount ) == 0 ) {
			PROGCACHE_FreeTable();
		}
	}
	free( image );

	s_dirty = 0;
	s_writeFailures = 0;
	s_stats.wroteBytes = (unsigned)total;
	s_stats.fileBytes = (unsigned)used;
	s_stats.entries = (unsigned)s_numEntries;
	s_state = PROGCACHE_PC_LOADED;

	usec = PROGCACHE_Now() - t0;
	if ( usec < 0 ) {
		usec = 0;
	}
	s_stats.saveUsec = (unsigned)usec;
	return total;
}

void PROGCACHE_ProgCache_Shutdown( void ) {
	PROGCACHE_FreeTable();
	s_engaged = 0;
	s_loadDone = 0;
	s_writeDisabled = 0;
	s_writeFailures = 0;
	s_dirty = 0;
	s_mode = 0;
	s_state = PROGCACHE_PC_OFF;
	memset( &s_stats, 0, sizeof( s_stats ) );
}

void PROGCACHE_ProgCache_Stats( progcacheProgCacheStats_t *out ) {
	if ( out ) {
		*out = s_stats;
		out->engaged = s_engaged;
		out->loaded = ( s_state == PROGCACHE_PC_LOADED );
		out->state = s_state;
		out->loadState = s_stats.loadState;
	}
}

static const char *PROGCACHE_StateName( int state ) {
	switch ( state ) {
	case PROGCACHE_PC_UNSUPPORTED:	return "unsupported";
	case PROGCACHE_PC_NOFILE:		return "nofile";
	case PROGCACHE_PC_LOADED:		return "loaded";
	case PROGCACHE_PC_FOREIGN:		return "foreign";
	case PROGCACHE_PC_CORRUPT:		return "corrupt";
	case PROGCACHE_PC_WRITEFAIL:		return "writefail";
	default:					return "off";
	}
}

void PROGCACHE_ProgCache_Format( progcacheProgCacheStats_t *stats, char *out, int outSize ) {
	progcacheProgCacheStats_t	s;

	if ( !out || outSize <= 0 ) {
		return;
	}
	if ( stats ) {
		s = *stats;
	} else {
		PROGCACHE_ProgCache_Stats( &s );
	}
	snprintf( out, (size_t)outSize,
			  "state=%s entries=%u hits=%u miss=%u bad=%u saved=%u.%01ums cold=%u.%01ums "
			  "restored=%u.%01ums save=%u.%01ums wrote=%uB bytes=%uB glerr=%u",
			  PROGCACHE_StateName( s.state ), s.entries, s.hits, s.miss, s.bad,
			  s.savedUsec / 1000u, ( s.savedUsec / 100u ) % 10u,
			  s.coldUsec / 1000u, ( s.coldUsec / 100u ) % 10u,
			  s.restoreUsec / 1000u, ( s.restoreUsec / 100u ) % 10u,
			  s.saveUsec / 1000u, ( s.saveUsec / 100u ) % 10u,
			  s.wroteBytes, s.fileBytes, s.glerr );
}

#ifndef PROGCACHE_PROGCACHE_STANDALONE
#include "tr_local.h"
#include "tr_program.h"

static long PROGCACHE_RI_Open( void *ud, const char *qpath, void **fh ) {
	fileHandle_t	h = 0;
	long			len;

	(void)ud;
	len = ri.FS_OpenFile( (char *)qpath, &h, qfalse, qtrue );
	if ( len < 0 || h == 0 ) {
		return -1;
	}
	if ( fh ) {
		*fh = (void *)(intptr_t)h;
	}
	return len;
}

static int PROGCACHE_RI_Read( void *ud, void *fh, void *buf, int len ) {
	(void)ud;
	return (int)ri.FS_Read( buf, (size_t)len, (fileHandle_t)(intptr_t)fh );
}

static void PROGCACHE_RI_Close( void *ud, void *fh ) {
	(void)ud;
	ri.FS_CloseFile( (fileHandle_t)(intptr_t)fh );
}

static int PROGCACHE_RI_Write( void *ud, const char *qpath, const void *buf, int len ) {
	fileHandle_t	h;

	(void)ud;
	h = ri.FS_OpenFileWrite( qpath );
	if ( h == 0 ) {
		return 0;
	}
	if ( (int)ri.FS_Write( buf, (size_t)len, h ) != len ) {
		ri.FS_CloseFile( h );
		return 0;
	}
	ri.FS_CloseFile( h );
	return 1;
}

static int PROGCACHE_RI_Microseconds( void ) {
	return BE_Microseconds();
}

static const progcacheProgCacheIO_t s_ri_io = {
	NULL, PROGCACHE_RI_Open, PROGCACHE_RI_Read, PROGCACHE_RI_Close, PROGCACHE_RI_Write, PROGCACHE_RI_Microseconds
};

static int PROGCACHE_GL_GetError( void ) {
	return (int)qglGetError();
}

static unsigned PROGCACHE_GL_CreateProgram( void ) {
	return (unsigned)qglCreateProgram();
}

static void PROGCACHE_GL_DeleteProgram( unsigned program ) {
	qglDeleteProgram( (GLuint)program );
}

static void PROGCACHE_GL_GetProgramiv( unsigned program, unsigned pname, int *params ) {
	qglGetProgramiv( (GLuint)program, (GLenum)pname, params );
}

static void PROGCACHE_GL_GetProgramBinary( unsigned program, int bufSize, int *length,
									  unsigned *format, void *binary ) {
	GLsizei	got = 0;
	GLenum	fmt = 0;

	qglGetProgramBinary( (GLuint)program, (GLsizei)bufSize, &got, &fmt, binary );
	if ( length ) {
		*length = (int)got;
	}
	if ( format ) {
		*format = (unsigned)fmt;
	}
}

static void PROGCACHE_GL_ProgramBinary( unsigned program, unsigned format,
								   const void *binary, int length ) {
	qglProgramBinary( (GLuint)program, (GLenum)format, binary, (GLsizei)length );
}

static void PROGCACHE_GL_ProgramParameteri( unsigned program, unsigned pname, int value ) {
	qglProgramParameteri( (GLuint)program, (GLenum)pname, (GLint)value );
}

static void PROGCACHE_GL_GetActiveAttrib( unsigned program, unsigned index, int bufSize,
									 int *length, int *size, unsigned *type, char *name ) {
	GLsizei	len = 0;
	GLint	sz = 0;
	GLenum	ty = 0;

	qglGetActiveAttrib( (GLuint)program, (GLuint)index, (GLsizei)bufSize,
						&len, &sz, &ty, (GLchar *)name );
	if ( length ) {
		*length = (int)len;
	}
	if ( size ) {
		*size = (int)sz;
	}
	if ( type ) {
		*type = (unsigned)ty;
	}
}

static void PROGCACHE_GL_GetActiveUniform( unsigned program, unsigned index, int bufSize,
									  int *length, int *size, unsigned *type, char *name ) {
	GLsizei	len = 0;
	GLint	sz = 0;
	GLenum	ty = 0;

	qglGetActiveUniform( (GLuint)program, (GLuint)index, (GLsizei)bufSize,
						 &len, &sz, &ty, (GLchar *)name );
	if ( length ) {
		*length = (int)len;
	}
	if ( size ) {
		*size = (int)sz;
	}
	if ( type ) {
		*type = (unsigned)ty;
	}
}

static int PROGCACHE_GL_GetAttribLocation( unsigned program, const char *name ) {
	return (int)qglGetAttribLocation( (GLuint)program, (const GLchar *)name );
}

static int PROGCACHE_GL_GetUniformLocation( unsigned program, const char *name ) {
	return (int)qglGetUniformLocation( (GLuint)program, (const GLchar *)name );
}

static int PROGCACHE_GL_GetBinaryFormatCount( void ) {
	GLint	n = 0;

	qglGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &n );
	return (int)n;
}

static int PROGCACHE_GL_GetBinaryFormats( unsigned *out, int max ) {
	GLint	n = 0, i;
	GLint	tmp[PROGCACHE_PROGCACHE_MAX_FORMATS];

	if ( max <= 0 || max > PROGCACHE_PROGCACHE_MAX_FORMATS ) {
		return 0;
	}
	qglGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &n );
	if ( n <= 0 || n > max ) {
		return 0;
	}
	qglGetIntegerv( GL_PROGRAM_BINARY_FORMATS, tmp );
	for ( i = 0; i < n; i++ ) {
		out[i] = (unsigned)tmp[i];
	}
	return (int)n;
}

static const progcacheProgCacheGl_t s_ri_gl = {
	PROGCACHE_GL_GetError,
	PROGCACHE_GL_CreateProgram,
	PROGCACHE_GL_DeleteProgram,
	PROGCACHE_GL_GetProgramiv,
	PROGCACHE_GL_GetProgramBinary,
	PROGCACHE_GL_ProgramBinary,
	PROGCACHE_GL_ProgramParameteri,
	PROGCACHE_GL_GetActiveAttrib,
	PROGCACHE_GL_GetActiveUniform,
	PROGCACHE_GL_GetAttribLocation,
	PROGCACHE_GL_GetUniformLocation,
	PROGCACHE_GL_GetBinaryFormatCount,
	PROGCACHE_GL_GetBinaryFormats
};

void PROGCACHE_ProgCache_InitEngine( int mode ) {
	PROGCACHE_ProgCache_Init( &s_ri_io, &s_ri_gl, mode );
}
#endif 
