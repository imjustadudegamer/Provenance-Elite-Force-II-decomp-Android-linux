#define _POSIX_C_SOURCE 200809L

#include "tr_program_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FAKE_MAX_PROGRAMS 512
#define FAKE_MAX_UNIFORMS 8
#define FAKE_ATTRS        5
#define FAKE_BLOB_BYTES   256

static const char	*g_cacheDir = NULL;
static const char	*g_phase = "cold";
static int			g_n = 8;
static const char	*g_version = "OpenGL ES 3.2 build 1.2.3";
static const char	*g_renderer = "FakeGPU 9000";
static const char	*g_vendor = "FakeVendor Inc.";
static unsigned		g_sourceSalt = 0;
static unsigned		g_definesSalt = 0;
static int			g_ifaceDrift = 0;
static int			g_refuseBinary = 0;
static int			g_noFormats = 0;
static int			g_failWrite = 0;
static int			g_disable = 0;

static int	g_ioOpens;
static int	g_ioWrites;
static int	g_ioReads;
static int	g_driverQueries;	
static int	g_cacheDriver;		
static int	g_clockReads;
static unsigned g_fakeFormat = 0x1234u;

static const unsigned g_fakeFormats[1] = { 0x1234u };

typedef struct {
	int				live;
	unsigned		identity;		
	unsigned		ifaceKey;		
	unsigned		format;
	unsigned char	blob[FAKE_BLOB_BYTES];
	int				blobLen;
	int				linkStatus;
} fakeProg_t;

static fakeProg_t	g_progs[FAKE_MAX_PROGRAMS];
static int			g_errorQueue;

static unsigned FakeHash( const void *data, int len ) {
	return PROGCACHE_ProgCache_Hash( data, len );
}

static unsigned FakeIdentity( const char *name, unsigned defines,
							  unsigned vsHash, unsigned fsHash ) {
	unsigned	h = PROGCACHE_ProgCache_HashSeed();

	h = PROGCACHE_ProgCache_FoldStr( h, name );
	h = PROGCACHE_ProgCache_Fold( h, &defines, (int)sizeof( defines ) );
	h = PROGCACHE_ProgCache_Fold( h, &vsHash, (int)sizeof( vsHash ) );
	h = PROGCACHE_ProgCache_Fold( h, &fsHash, (int)sizeof( fsHash ) );
	return h;
}

static int FakeUniformCount( unsigned key ) {
	return 2 + (int)( key % 4u );
}

static const char *const g_attrNames[FAKE_ATTRS] = {
	"a_Position", "a_Normal", "a_Color", "a_TexCoord0", "a_TexCoord1"
};

static void FakeBuildBlob( fakeProg_t *p, unsigned identity ) {
	unsigned	checksum;
	int			i;

	p->blobLen = 32;
	p->blob[0] = 'F'; p->blob[1] = 'K'; p->blob[2] = 'B'; p->blob[3] = '1';
	memcpy( p->blob + 4, &identity, 4 );
	for ( i = 12; i < p->blobLen; i++ ) {
		p->blob[i] = (unsigned char)( identity * 31u + (unsigned)i );
	}
	checksum = FakeHash( p->blob + 8, p->blobLen - 8 );
	memcpy( p->blob + 8, &checksum, 4 );
}

static unsigned FakeBlobIdentity( const fakeProg_t *p ) {
	unsigned	identity = 0, checksum = 0, want;
	unsigned char	tmp[FAKE_BLOB_BYTES];

	if ( p->blobLen < 32 || memcmp( p->blob, "FKB1", 4 ) != 0 ) {
		return 0;
	}
	memcpy( &identity, p->blob + 4, 4 );
	memcpy( &checksum, p->blob + 8, 4 );
	memcpy( tmp, p->blob, (size_t)p->blobLen );
	memset( tmp + 8, 0, 4 );
	want = FakeHash( tmp + 8, p->blobLen - 8 );
	if ( want != checksum ) {
		return 0;
	}
	return identity;
}

static unsigned FakeCreateProgram( void ) {
	int	i;

	g_driverQueries++;
	for ( i = 0; i < FAKE_MAX_PROGRAMS; i++ ) {
		if ( !g_progs[i].live ) {
			memset( &g_progs[i], 0, sizeof( g_progs[i] ) );
			g_progs[i].live = 1;
			return (unsigned)( i + 1 );
		}
	}
	return 0;
}

static void FakeDeleteProgram( unsigned program ) {
	g_driverQueries++;
	if ( program >= 1 && program <= FAKE_MAX_PROGRAMS ) {
		g_progs[program - 1].live = 0;
	}
}

static fakeProg_t *FakeGet( unsigned program ) {
	if ( program < 1 || program > FAKE_MAX_PROGRAMS || !g_progs[program - 1].live ) {
		return NULL;
	}
	return &g_progs[program - 1];
}

static void FakeGetProgramiv( unsigned program, unsigned pname, int *params ) {
	fakeProg_t	*p;

	g_driverQueries++;
	if ( !params ) {
		return;
	}
	p = FakeGet( program );
	if ( !p ) {
		*params = 0;
		return;
	}
	switch ( pname ) {
	case PROGCACHE_GL_LINK_STATUS:
		*params = p->linkStatus ? 1 : 0;
		break;
	case PROGCACHE_GL_PROGRAM_BINARY_LENGTH:
		*params = p->linkStatus ? p->blobLen : 0;
		break;
	case PROGCACHE_GL_ACTIVE_ATTRIBUTES:
		*params = p->linkStatus ? FAKE_ATTRS : 0;
		break;
	case PROGCACHE_GL_ACTIVE_UNIFORMS:
		*params = p->linkStatus ? FakeUniformCount( p->ifaceKey ) : 0;
		break;
	default:
		*params = 0;
		break;
	}
}

static void FakeGetProgramBinary( unsigned program, int bufSize, int *length,
								  unsigned *format, void *binary ) {
	fakeProg_t	*p;

	g_driverQueries++;
	if ( length ) {
		*length = 0;
	}
	if ( format ) {
		*format = 0;
	}
	p = FakeGet( program );
	if ( !p || !binary || bufSize < p->blobLen || !p->linkStatus ) {
		g_errorQueue++;
		return;
	}
	memcpy( binary, p->blob, (size_t)p->blobLen );
	if ( length ) {
		*length = p->blobLen;
	}
	if ( format ) {
		*format = p->format;
	}
}

static fakeProg_t *FakeGet( unsigned program );

static void FakeProgramBinary( unsigned program, unsigned format,
							   const void *binary, int length ) {
	fakeProg_t		p;
	unsigned		identity;
	fakeProg_t		*live = FakeGet( program );

	g_driverQueries++;
	if ( !live ) {
		return;
	}
	p.live = 1;
	p.format = format;
	p.blobLen = length;
	memset( p.blob, 0, sizeof( p.blob ) );
	if ( length > 0 && length <= (int)sizeof( p.blob ) && binary ) {
		memcpy( p.blob, binary, (size_t)length );
	}

	if ( g_refuseBinary ) {
		g_errorQueue++;
		live->linkStatus = 0;
		return;
	}

	identity = ( length > 0 ) ? FakeBlobIdentity( &p ) : 0;
	if ( !identity ) {
		g_errorQueue++;
		live->linkStatus = 0;
		return;
	}
	live->linkStatus = 1;
	live->identity = identity;
	live->ifaceKey = g_ifaceDrift ? ( identity + 1u ) : identity;
}

static void FakeGetActiveAttrib( unsigned program, unsigned index, int bufSize,
								 int *length, int *size, unsigned *type, char *name ) {
	fakeProg_t	*p = FakeGet( program );

	g_driverQueries++;
	if ( length ) { *length = 0; }
	if ( size ) { *size = 0; }
	if ( type ) { *type = 0; }
	if ( !name || bufSize <= 0 ) {
		return;
	}
	name[0] = '\0';
	if ( !p || !p->linkStatus || index >= FAKE_ATTRS ) {
		return;
	}
	snprintf( name, (size_t)bufSize, "%s", g_attrNames[index] );
	if ( length ) { *length = (int)strlen( name ); }
	if ( size ) { *size = 1; }
	if ( type ) { *type = 0x8B50u; }	
}

static void FakeGetActiveUniform( unsigned program, unsigned index, int bufSize,
								  int *length, int *size, unsigned *type, char *name ) {
	fakeProg_t	*p = FakeGet( program );

	g_driverQueries++;
	if ( length ) { *length = 0; }
	if ( size ) { *size = 0; }
	if ( type ) { *type = 0; }
	if ( !name || bufSize <= 0 ) {
		return;
	}
	name[0] = '\0';
	if ( !p || !p->linkStatus || (int)index >= FakeUniformCount( p->ifaceKey ) ) {
		return;
	}
	snprintf( name, (size_t)bufSize, "u_K%08x_%u", p->ifaceKey, index );
	if ( length ) { *length = (int)strlen( name ); }
	if ( size ) { *size = 1; }
	if ( type ) { *type = 0x8B5Eu; }	
}

static int FakeGetAttribLocation( unsigned program, const char *name ) {
	fakeProg_t	*p = FakeGet( program );
	int			i;

	g_driverQueries++;
	if ( !p || !p->linkStatus || !name ) {
		return -1;
	}
	for ( i = 0; i < FAKE_ATTRS; i++ ) {
		if ( strcmp( name, g_attrNames[i] ) == 0 ) {
			return i;
		}
	}
	return -1;
}

static int FakeGetUniformLocation( unsigned program, const char *name ) {
	fakeProg_t	*p = FakeGet( program );
	char		want[64];
	int			i, n;

	g_driverQueries++;
	if ( !p || !p->linkStatus || !name ) {
		return -1;
	}
	n = FakeUniformCount( p->ifaceKey );
	for ( i = 0; i < n; i++ ) {
		snprintf( want, sizeof( want ), "u_K%08x_%d", p->ifaceKey, i );
		if ( strcmp( name, want ) == 0 ) {
			return 100 + i;
		}
	}
	return -1;
}

static int FakeGetError( void ) {
	g_driverQueries++;
	if ( g_errorQueue > 0 ) {
		g_errorQueue--;
		return 0x0502;	
	}
	return 0;
}

static int FakeGetBinaryFormatCount( void ) {
	g_driverQueries++;
	return g_noFormats ? 0 : 1;
}

static int FakeGetBinaryFormats( unsigned *out, int max ) {
	g_driverQueries++;
	if ( g_noFormats || max < 1 || !out ) {
		return 0;
	}
	out[0] = g_fakeFormats[0];
	return 1;
}

static void FakeProgramParameteri( unsigned program, unsigned pname, int value ) {
	g_driverQueries++;
	(void)program; (void)pname; (void)value;
}

static const progcacheProgCacheGl_t s_fakeGl = {
	FakeGetError,
	FakeCreateProgram,
	FakeDeleteProgram,
	FakeGetProgramiv,
	FakeGetProgramBinary,
	FakeProgramBinary,
	FakeProgramParameteri,
	FakeGetActiveAttrib,
	FakeGetActiveUniform,
	FakeGetAttribLocation,
	FakeGetUniformLocation,
	FakeGetBinaryFormatCount,
	FakeGetBinaryFormats
};

static void HostPath( char *out, int outSize, const char *qpath ) {
	snprintf( out, (size_t)outSize, "%s/%s", g_cacheDir, qpath );
}

static long HostOpen( void *ud, const char *qpath, void **fh ) {
	char	path[1024];
	FILE	*f;
	long	len;

	(void)ud;
	g_ioOpens++;
	HostPath( path, sizeof( path ), qpath );
	f = fopen( path, "rb" );
	if ( !f ) {
		return -1;
	}
	if ( fseek( f, 0, SEEK_END ) != 0 ) { fclose( f ); return -1; }
	len = ftell( f );
	if ( len < 0 || fseek( f, 0, SEEK_SET ) != 0 ) { fclose( f ); return -1; }
	if ( fh ) {
		*fh = f;
	}
	return len;
}

static int HostRead( void *ud, void *fh, void *buf, int len ) {
	(void)ud;
	g_ioReads++;
	return (int)fread( buf, 1, (size_t)len, (FILE *)fh );
}

static void HostClose( void *ud, void *fh ) {
	(void)ud;
	fclose( (FILE *)fh );
}

static void HostMakeDirs( const char *path ) {
	char	make[1024];
	size_t	i;

	snprintf( make, sizeof( make ), "%s", path );
	for ( i = 1; make[i]; i++ ) {
		if ( make[i] == '/' ) {
			make[i] = '\0';
			mkdir( make, 0775 );
			make[i] = '/';
		}
	}
}

static int HostWrite( void *ud, const char *qpath, const void *buf, int len ) {
	char	path[1024];
	FILE	*f;

	(void)ud;
	g_ioWrites++;
	if ( g_failWrite ) {
		return 0;
	}
	HostPath( path, sizeof( path ), qpath );
	HostMakeDirs( path );
	f = fopen( path, "wb" );
	if ( !f ) {
		return 0;
	}
	if ( fwrite( buf, 1, (size_t)len, f ) != (size_t)len ) {
		fclose( f );
		return 0;
	}
	return fclose( f ) == 0;
}

static int HostMicroseconds( void ) {
	struct timespec	ts;

	g_clockReads++;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec * 1000000 + ts.tv_nsec / 1000 ) & 0x7fffffff );
}

static const progcacheProgCacheIO_t s_hostIo = {
	NULL, HostOpen, HostRead, HostClose, HostWrite, HostMicroseconds
};

typedef struct {
	char		name[32];
	unsigned	defines;
	unsigned	vsHash;
	unsigned	fsHash;
} progSpec_t;

static void ProgramSpec( int i, progSpec_t *s ) {
	char	text[128];

	snprintf( s->name, sizeof( s->name ), "prog%02d", i );
	s->defines = 0x01000000u | ( (unsigned)i << 8 ) | (unsigned)i;

	snprintf( text, sizeof( text ), "void main(){ /* prog %d salt %u */ }",
			  i, g_sourceSalt );
	s->vsHash = FakeHash( text, (int)strlen( text ) );
	snprintf( text, sizeof( text ), "frag %d salt %u", i, g_sourceSalt );
	s->fsHash = FakeHash( text, (int)strlen( text ) );
}

static unsigned TheSourceHash( void ) {
	char	text[128];
	unsigned	h = PROGCACHE_ProgCache_HashSeed();

	snprintf( text, sizeof( text ), "corpus salt %u", g_sourceSalt );
	h = PROGCACHE_ProgCache_FoldStr( h, text );
	return h;
}

static unsigned TheDefinesHash( void ) {
	unsigned	h = PROGCACHE_ProgCache_HashSeed();
	unsigned	salt = g_definesSalt;

	h = PROGCACHE_ProgCache_Fold( h, &salt, (int)sizeof( salt ) );
	return h;
}

static unsigned CompileProgram( const progSpec_t *s, unsigned buildUsec ) {
	unsigned	program = FakeCreateProgram();
	fakeProg_t	*p = FakeGet( program );
	unsigned	identity;

	if ( !p ) {
		return 0;
	}
	identity = FakeIdentity( s->name, s->defines, s->vsHash, s->fsHash );
	p->linkStatus = 1;
	p->identity = identity;
	p->ifaceKey = identity;
	p->format = g_fakeFormat;
	FakeBuildBlob( p, identity );

	PROGCACHE_ProgCache_NoteCompiled( s->name, s->defines, s->vsHash, s->fsHash,
								 program, buildUsec );
	return program;
}

static const char *StateName( int state ) {
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
static int ParseArgs( int argc, char **argv ) {
	int	i;

	for ( i = 1; i < argc; i++ ) {
		const char	*a = argv[i];

		if ( strncmp( a, "--cache=", 8 ) == 0 ) { g_cacheDir = a + 8; }
		else if ( strncmp( a, "--phase=", 8 ) == 0 ) { g_phase = a + 8; }
		else if ( strncmp( a, "--n=", 4 ) == 0 ) { g_n = atoi( a + 4 ); }
		else if ( strncmp( a, "--version=", 10 ) == 0 ) { g_version = a + 10; }
		else if ( strncmp( a, "--renderer=", 11 ) == 0 ) { g_renderer = a + 11; }
		else if ( strncmp( a, "--vendor=", 9 ) == 0 ) { g_vendor = a + 9; }
		else if ( strncmp( a, "--source-salt=", 14 ) == 0 ) { g_sourceSalt = (unsigned)atoi( a + 14 ); }
		else if ( strncmp( a, "--defines-salt=", 15 ) == 0 ) { g_definesSalt = (unsigned)atoi( a + 15 ); }
		else if ( strncmp( a, "--iface-drift=", 14 ) == 0 ) { g_ifaceDrift = atoi( a + 14 ); }
		else if ( strncmp( a, "--refuse-binary=", 16 ) == 0 ) { g_refuseBinary = atoi( a + 16 ); }
		else if ( strncmp( a, "--no-formats=", 13 ) == 0 ) { g_noFormats = atoi( a + 13 ); }
		else if ( strncmp( a, "--fail-write=", 13 ) == 0 ) { g_failWrite = atoi( a + 13 ); }
		else if ( strncmp( a, "--disable=", 10 ) == 0 ) { g_disable = atoi( a + 10 ); }
		else {
			fprintf( stderr, "unknown argument: %s\n", a );
			return 0;
		}
	}
	if ( !g_cacheDir ) {
		fprintf( stderr, "--cache=DIR is required\n" );
		return 0;
	}
	if ( g_n < 1 || g_n > FAKE_MAX_PROGRAMS ) {
		fprintf( stderr, "--n out of range\n" );
		return 0;
	}
	return 1;
}

int main( int argc, char **argv ) {
	progcacheProgCacheKey_t		key;
	progcacheProgCacheStats_t	st;
	progSpec_t				specs[FAKE_MAX_PROGRAMS];
	int						i, restored = 0, compiled = 0, mismatches = 0;
	int						warm;
	int						wrote, mark;

	if ( !ParseArgs( argc, argv ) ) {
		return 2;
	}
	warm = ( strcmp( g_phase, "warm" ) == 0 );

	mark = g_driverQueries;
	PROGCACHE_ProgCache_Init( &s_hostIo, &s_fakeGl, g_disable ? 0 : 1 );

	memset( &key, 0, sizeof( key ) );
	key.glVersion = g_version;
	key.glRenderer = g_renderer;
	key.glVendor = g_vendor;
	key.glesMajor = 3;
	key.glesMinor = 2;
	key.sourceHash = TheSourceHash();
	key.definesHash = TheDefinesHash();

	if ( !g_disable ) {
		PROGCACHE_ProgCache_Load( &key );
	}
	g_cacheDriver += g_driverQueries - mark;

	for ( i = 0; i < g_n; i++ ) {
		unsigned	program = 0, savedUsec = 0, restoreUsec = 0;
		unsigned	wantIface;
		fakeProg_t	*p;
		int			ok = 0;

		ProgramSpec( i, &specs[i] );

		mark = g_driverQueries;
		if ( warm && PROGCACHE_ProgCache_On()
			 && PROGCACHE_ProgCache_TryRestore( specs[i].name, specs[i].defines,
										   specs[i].vsHash, specs[i].fsHash,
										   &program, &savedUsec, &restoreUsec ) ) {
			g_cacheDriver += g_driverQueries - mark;
			restored++;
			ok = 1;
			p = FakeGet( program );
			wantIface = 0;
			{
				unsigned identity = FakeIdentity( specs[i].name, specs[i].defines,
												  specs[i].vsHash, specs[i].fsHash );
				wantIface = identity;
			}
			if ( !p || p->ifaceKey != wantIface ) {
				mismatches++;
			}
			printf( "[PROGCACHE:pc-probe] program i=%d name=%s defines=0x%08x result=restored iface=%08x saved=%uus restore=%uus\n",
					i, specs[i].name, specs[i].defines,
					p ? p->ifaceKey : 0u, savedUsec, restoreUsec );
		} else {
			unsigned	identity;

			g_cacheDriver += g_driverQueries - mark;
			compiled++;
			program = CompileProgram( &specs[i], 1000u + (unsigned)i );
			identity = FakeIdentity( specs[i].name, specs[i].defines,
									 specs[i].vsHash, specs[i].fsHash );
			printf( "[PROGCACHE:pc-probe] program i=%d name=%s defines=0x%08x result=compiled iface=%08x\n",
					i, specs[i].name, specs[i].defines, identity );
		}
		(void)ok;
	}

	mark = g_driverQueries;
	wrote = PROGCACHE_ProgCache_Save();
	g_cacheDriver += g_driverQueries - mark;
	PROGCACHE_ProgCache_Stats( &st );

	printf( "[PROGCACHE:pc-probe] summary phase=%s state=%s stateId=%d engaged=%d loaded=%d "
			"entries=%u hits=%u miss=%u bad=%u savedUsec=%u restoreUsec=%u coldUsec=%u "
			"restored=%d compiled=%d mismatches=%d wrote=%d bytes=%u "
			"loadState=%s loadStateId=%d "
			"ioOpen=%d ioRead=%d ioWrite=%d driver=%d cacheDriver=%d clock=%d glerr=%u\n",
			g_phase, StateName( st.state ), st.state, st.engaged, st.loaded,
			st.entries, st.hits, st.miss, st.bad, st.savedUsec, st.restoreUsec, st.coldUsec,
			restored, compiled, mismatches, wrote, st.fileBytes,
			StateName( st.loadState ), st.loadState,
			g_ioOpens, g_ioReads, g_ioWrites, g_driverQueries, g_cacheDriver,
			g_clockReads, st.glerr );

	PROGCACHE_ProgCache_Shutdown();
	return mismatches ? 1 : 0;
}
