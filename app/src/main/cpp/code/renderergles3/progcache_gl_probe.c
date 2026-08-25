#define _POSIX_C_SOURCE 200809L

#include "tr_program_cache.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define GL_PROBE_MAX_PROGRAMS 32
#define ATTRIB_LOCS 5

static const char	*g_cacheDir = NULL;
static const char	*g_phase = "cold";
static int			g_n = 8;

static int	g_ioOpens, g_ioWrites, g_driverCalls, g_clockReads;

static int GL_GetError( void ) { g_driverCalls++; return (int)glGetError(); }
static unsigned GL_CreateProgram( void ) { g_driverCalls++; return (unsigned)glCreateProgram(); }
static void GL_DeleteProgram( unsigned p ) { g_driverCalls++; glDeleteProgram( (GLuint)p ); }
static void GL_GetProgramiv( unsigned p, unsigned pname, int *out ) {
	g_driverCalls++;
	glGetProgramiv( (GLuint)p, (GLenum)pname, out );
}
static void GL_GetProgramBinary( unsigned p, int bufSize, int *length,
								 unsigned *format, void *binary ) {
	GLsizei	got = 0;
	GLenum	fmt = 0;

	g_driverCalls++;
	glGetProgramBinary( (GLuint)p, (GLsizei)bufSize, &got, &fmt, binary );
	if ( length ) { *length = (int)got; }
	if ( format ) { *format = (unsigned)fmt; }
}
static void GL_ProgramBinary( unsigned p, unsigned format, const void *binary, int length ) {
	g_driverCalls++;
	glProgramBinary( (GLuint)p, (GLenum)format, binary, (GLsizei)length );
}
static void GL_ProgramParameteri( unsigned p, unsigned pname, int value ) {
	g_driverCalls++;
	glProgramParameteri( (GLuint)p, (GLenum)pname, (GLint)value );
}
static void GL_GetActiveAttrib( unsigned p, unsigned index, int bufSize,
								int *length, int *size, unsigned *type, char *name ) {
	GLsizei	len = 0;
	GLint	sz = 0;
	GLenum	ty = 0;

	g_driverCalls++;
	glGetActiveAttrib( (GLuint)p, (GLuint)index, (GLsizei)bufSize, &len, &sz, &ty, (GLchar *)name );
	if ( length ) { *length = (int)len; }
	if ( size ) { *size = (int)sz; }
	if ( type ) { *type = (unsigned)ty; }
}
static void GL_GetActiveUniform( unsigned p, unsigned index, int bufSize,
								 int *length, int *size, unsigned *type, char *name ) {
	GLsizei	len = 0;
	GLint	sz = 0;
	GLenum	ty = 0;

	g_driverCalls++;
	glGetActiveUniform( (GLuint)p, (GLuint)index, (GLsizei)bufSize, &len, &sz, &ty, (GLchar *)name );
	if ( length ) { *length = (int)len; }
	if ( size ) { *size = (int)sz; }
	if ( type ) { *type = (unsigned)ty; }
}
static int GL_GetAttribLocation( unsigned p, const char *name ) {
	g_driverCalls++;
	return (int)glGetAttribLocation( (GLuint)p, (const GLchar *)name );
}
static int GL_GetUniformLocation( unsigned p, const char *name ) {
	g_driverCalls++;
	return (int)glGetUniformLocation( (GLuint)p, (const GLchar *)name );
}
static int GL_GetBinaryFormatCount( void ) {
	GLint	n = 0;

	g_driverCalls++;
	glGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &n );
	return (int)n;
}
static int GL_GetBinaryFormats( unsigned *out, int max ) {
	GLint	n = 0, i;
	GLint	tmp[PROGCACHE_PROGCACHE_MAX_FORMATS];

	g_driverCalls++;
	if ( max <= 0 || max > PROGCACHE_PROGCACHE_MAX_FORMATS ) { return 0; }
	glGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &n );
	if ( n <= 0 || n > max ) { return 0; }
	glGetIntegerv( GL_PROGRAM_BINARY_FORMATS, tmp );
	for ( i = 0; i < n; i++ ) { out[i] = (unsigned)tmp[i]; }
	return (int)n;
}

static const progcacheProgCacheGl_t s_gl = {
	GL_GetError, GL_CreateProgram, GL_DeleteProgram, GL_GetProgramiv,
	GL_GetProgramBinary, GL_ProgramBinary, GL_ProgramParameteri,
	GL_GetActiveAttrib, GL_GetActiveUniform, GL_GetAttribLocation,
	GL_GetUniformLocation, GL_GetBinaryFormatCount, GL_GetBinaryFormats
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
	if ( !f ) { return -1; }
	if ( fseek( f, 0, SEEK_END ) != 0 ) { fclose( f ); return -1; }
	len = ftell( f );
	if ( len < 0 || fseek( f, 0, SEEK_SET ) != 0 ) { fclose( f ); return -1; }
	if ( fh ) { *fh = f; }
	return len;
}
static int HostRead( void *ud, void *fh, void *buf, int len ) {
	(void)ud;
	return (int)fread( buf, 1, (size_t)len, (FILE *)fh );
}
static void HostClose( void *ud, void *fh ) { (void)ud; fclose( (FILE *)fh ); }

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
	HostPath( path, sizeof( path ), qpath );
	HostMakeDirs( path );
	f = fopen( path, "wb" );
	if ( !f ) { return 0; }
	if ( fwrite( buf, 1, (size_t)len, f ) != (size_t)len ) { fclose( f ); return 0; }
	return fclose( f ) == 0;
}

static int HostMicroseconds( void ) {
	struct timespec	ts;

	g_clockReads++;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec * 1000000 + ts.tv_nsec / 1000 ) & 0x7fffffff );
}

static const progcacheProgCacheIO_t s_io = {
	NULL, HostOpen, HostRead, HostClose, HostWrite, HostMicroseconds
};

static int EGL_Open( void ) {
	EGLDisplay	d;
	EGLConfig	c;
	EGLContext	ctx;
	EGLSurface	surf;
	EGLint		major = 0, minor = 0, cfgs = 0;
	EGLint		ca[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
						EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE };
	EGLint		pa[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
	EGLint		ca3[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE };
	PFNEGLGETPLATFORMDISPLAYEXTPROC	getpd;

	getpd = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress( "eglGetPlatformDisplayEXT" );
	d = getpd ? getpd( EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL )
			  : eglGetDisplay( EGL_DEFAULT_DISPLAY );
	if ( d == EGL_NO_DISPLAY || !eglInitialize( d, &major, &minor ) ) {
		return 0;
	}
	if ( !eglChooseConfig( d, ca, &c, 1, &cfgs ) || cfgs < 1 ) {
		return 0;
	}
	ctx = eglCreateContext( d, c, EGL_NO_CONTEXT, ca3 );
	if ( ctx == EGL_NO_CONTEXT ) {
		return 0;
	}
	surf = eglCreatePbufferSurface( d, c, pa );
	if ( surf == EGL_NO_SURFACE ) {
		return 0;
	}
	if ( !eglMakeCurrent( d, surf, surf, ctx ) ) {
		return 0;
	}
	return 1;
}

static const char *VS_VAR[4] = {
	"uniform vec4 u_Var1;\n", "uniform vec4 u_Var2;\n",
	"uniform vec4 u_Var3;\n", "uniform vec4 u_Var4;\n"
};
static const char *VS_USE[4] = {
	"+ u_Var1.x", "+ u_Var2.x", "+ u_Var3.x", "+ u_Var4.x"
};

#define FS_TEMPLATE \
	"precision mediump float;\n" \
	"in vec4 v_Color;\n" \
	"in vec2 v_TexCoord0;\n" \
	"uniform sampler2D u_Tex0;\n" \
	"uniform int u_TexEnv0;\n" \
	"out vec4 fragColor;\n" \
	"void main() {\n" \
	"  vec4 t = texture(u_Tex0, v_TexCoord0);\n" \
	"  fragColor = (u_TexEnv0 == 0) ? (v_Color * t) : v_Color;\n" \
	"}\n"

typedef struct {
	char		name[32];
	unsigned	defines;
	char		vs[2048];
	char		fs[1024];
	unsigned	vsHash;
	unsigned	fsHash;
} glProgSpec_t;

static void SpecBuild( int i, glProgSpec_t *s ) {
	const char	*var = VS_VAR[i % 4];
	const char	*use = VS_USE[i % 4];

	snprintf( s->name, sizeof( s->name ), "glprog%02d", i );
	s->defines = 0x02000000u | (unsigned)i;

	snprintf( s->vs, sizeof( s->vs ),
			  "#version 300 es\n#define PROGCACHE_ID %d\nuniform vec4 u_Var0;\n%s"
			  "in vec4 a_Position;\nin vec3 a_Normal;\nin vec4 a_Color;\n"
			  "in vec2 a_TexCoord0;\nin vec2 a_TexCoord1;\n"
			  "uniform mat4 u_MVP;\nuniform vec4 u_BaseColor;\n"
			  "out vec4 v_Color;\nout vec2 v_TexCoord0;\n"
			  "void main() {\n"
			  "  v_Color = a_Color * u_BaseColor + u_Var0 * (a_Normal.x + a_TexCoord1.y) %s;\n"
			  "  v_TexCoord0 = a_TexCoord0;\n"
			  "  gl_Position = u_MVP * a_Position;\n}\n",
			  i, var, use );
	snprintf( s->fs, sizeof( s->fs ), "#version 300 es\n#define PROGCACHE_ID %d\n%s", i, FS_TEMPLATE );

	s->vsHash = PROGCACHE_ProgCache_Hash( s->vs, (int)strlen( s->vs ) );
	s->fsHash = PROGCACHE_ProgCache_Hash( s->fs, (int)strlen( s->fs ) );
}

static unsigned CompileShader( GLenum type, const char *src, const char *label ) {
	GLuint	sh = glCreateShader( type );
	GLint	ok = 0;
	char	log[2048];

	glShaderSource( sh, 1, &src, NULL );
	glCompileShader( sh );
	glGetShaderiv( sh, GL_COMPILE_STATUS, &ok );
	if ( !ok ) {
		log[0] = '\0';
		glGetShaderInfoLog( sh, sizeof( log ) - 1, NULL, log );
		fprintf( stderr, "compile failed (%s): %s\n", label, log );
		return 0;
	}
	return (unsigned)sh;
}

static int NowUsec( void ) {
	struct timespec	ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec * 1000000 + ts.tv_nsec / 1000 ) & 0x7fffffff );
}

static unsigned CompileProgram( const glProgSpec_t *s ) {
	unsigned	vs, fs, program;
	GLint		ok = 0;
	char		log[2048];
	int			t0, t1, buildUsec;

	t0 = NowUsec();
	vs = CompileShader( GL_VERTEX_SHADER, s->vs, s->name );
	fs = CompileShader( GL_FRAGMENT_SHADER, s->fs, s->name );
	if ( !vs || !fs ) {
		return 0;
	}
	program = (unsigned)glCreateProgram();
	glAttachShader( (GLuint)program, (GLuint)vs );
	glAttachShader( (GLuint)program, (GLuint)fs );
	glBindAttribLocation( (GLuint)program, 0, "a_Position" );
	glBindAttribLocation( (GLuint)program, 1, "a_Normal" );
	glBindAttribLocation( (GLuint)program, 2, "a_Color" );
	glBindAttribLocation( (GLuint)program, 3, "a_TexCoord0" );
	glBindAttribLocation( (GLuint)program, 4, "a_TexCoord1" );
	PROGCACHE_ProgCache_HintLink( program );
	glLinkProgram( (GLuint)program );
	glGetProgramiv( (GLuint)program, GL_LINK_STATUS, &ok );
	if ( !ok ) {
		log[0] = '\0';
		glGetProgramInfoLog( (GLuint)program, sizeof( log ) - 1, NULL, log );
		fprintf( stderr, "link failed (%s): %s\n", s->name, log );
		return 0;
	}
	t1 = NowUsec();
	buildUsec = t1 - t0;
	if ( buildUsec < 0 ) { buildUsec = 0; }

	glDetachShader( (GLuint)program, (GLuint)vs );
	glDetachShader( (GLuint)program, (GLuint)fs );
	glDeleteShader( (GLuint)vs );
	glDeleteShader( (GLuint)fs );

	PROGCACHE_ProgCache_NoteCompiled( s->name, s->defines, s->vsHash, s->fsHash, program, (unsigned)buildUsec );
	return program;
}

int main( int argc, char **argv ) {
	progcacheProgCacheKey_t		key;
	progcacheProgCacheStats_t	st;
	glProgSpec_t			specs[GL_PROBE_MAX_PROGRAMS];
	int						i, restored = 0, compiled = 0, diverged = 0;
	int						warm, wrote;

	for ( i = 1; i < argc; i++ ) {
		if ( strncmp( argv[i], "--cache=", 8 ) == 0 ) { g_cacheDir = argv[i] + 8; }
		else if ( strncmp( argv[i], "--phase=", 8 ) == 0 ) { g_phase = argv[i] + 8; }
		else if ( strncmp( argv[i], "--n=", 4 ) == 0 ) { g_n = atoi( argv[i] + 4 ); }
		else { fprintf( stderr, "unknown argument: %s\n", argv[i] ); return 2; }
	}
	if ( !g_cacheDir || g_n < 1 || g_n > GL_PROBE_MAX_PROGRAMS ) {
		fprintf( stderr, "usage: --cache=DIR --phase=cold|warm [--n=N]\n" );
		return 2;
	}
	warm = ( strcmp( g_phase, "warm" ) == 0 );

	if ( !EGL_Open() ) {
		printf( "[PROGCACHE:pc-gl] SKIP no ES3 context on this host\n" );
		return 77;
	}

	PROGCACHE_ProgCache_Init( &s_io, &s_gl, 1 );
	memset( &key, 0, sizeof( key ) );
	key.glVersion = (const char *)glGetString( GL_VERSION );
	key.glRenderer = (const char *)glGetString( GL_RENDERER );
	key.glVendor = (const char *)glGetString( GL_VENDOR );
	key.glesMajor = 3;
	key.glesMinor = 0;
	{
		unsigned	h = PROGCACHE_ProgCache_HashSeed();

		for ( i = 0; i < g_n; i++ ) {
			SpecBuild( i, &specs[i] );
			h = PROGCACHE_ProgCache_FoldStr( h, specs[i].vs );
			h = PROGCACHE_ProgCache_FoldStr( h, specs[i].fs );
		}
		key.sourceHash = h;
		key.definesHash = 0x207u;
	}
	PROGCACHE_ProgCache_Load( &key );

	if ( !PROGCACHE_ProgCache_On() ) {
		printf( "[PROGCACHE:pc-gl] SKIP the driver reports no program-binary format\n" );
		return 77;
	}

	for ( i = 0; i < g_n; i++ ) {
		unsigned	program = 0, savedUsec = 0, restoreUsec = 0;

		if ( warm ) {
			if ( PROGCACHE_ProgCache_TryRestore( specs[i].name, specs[i].defines,
											specs[i].vsHash, specs[i].fsHash,
											&program, &savedUsec, &restoreUsec ) ) {
				GLint	link = 0;
				int		pos;

				restored++;
				glGetProgramiv( (GLuint)program, GL_LINK_STATUS, &link );
				pos = glGetAttribLocation( (GLuint)program, "a_Position" );
				if ( !link || pos != 0 ) {
					diverged++;
					printf( "[PROGCACHE:pc-gl] program i=%d result=restored link=%d a_Position=%d DIVERGED\n",
							i, (int)link, pos );
				} else {
					printf( "[PROGCACHE:pc-gl] program i=%d name=%s result=restored saved=%uus restore=%uus\n",
							i, specs[i].name, savedUsec, restoreUsec );
				}
				continue;
			}
		}
		compiled++;
		program = CompileProgram( &specs[i] );
		if ( !program ) {
			printf( "[PROGCACHE:pc-gl] program i=%d result=compile-FAILED\n", i );
			diverged++;
			continue;
		}
		printf( "[PROGCACHE:pc-gl] program i=%d name=%s result=compiled\n", i, specs[i].name );
	}

	wrote = PROGCACHE_ProgCache_Save();
	PROGCACHE_ProgCache_Stats( &st );
	printf( "[PROGCACHE:pc-gl] summary phase=%s state=%s hits=%u miss=%u bad=%u "
			"savedUsec=%u coldUsec=%u restoreUsec=%u restored=%d compiled=%d diverged=%d "
			"wrote=%d bytes=%u ioOpen=%d ioWrite=%d driver=%d clock=%d glerr=%u\n",
			g_phase, ( st.state == 3 ) ? "loaded" : ( st.state == 2 ? "nofile" : "other" ),
			st.hits, st.miss, st.bad, st.savedUsec, st.coldUsec, st.restoreUsec,
			restored, compiled, diverged, wrote, st.fileBytes,
			g_ioOpens, g_ioWrites, g_driverCalls, g_clockReads, st.glerr );

	PROGCACHE_ProgCache_Shutdown();
	return diverged ? 1 : 0;
}
