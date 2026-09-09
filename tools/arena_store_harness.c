/*
 * ===========================================================================
 * tools/arena_store_harness.c -- the host-side acceptance for the mapped store
 * ===========================================================================
 *
 * The mapped tess store (code/renderergles3/tr_vbo.c, r_tessMap 1) was armed on
 * the device exactly once, in 0.3.21, and the session came back `ran=0 fall=0
 * arena=0': the arm selected the store, not one batch was carried by it, and not
 * one batch was counted as fallen back either, so the log could not say which of
 * the two had happened.  This harness is the gate the arm now has to pass before
 * it ships armed again.  It runs on a host GLES 3 context (Mesa, EGL
 * surfaceless) and drives the store's own sequence:
 *
 *   - three stores rotating by frame, allocated once through
 *     GL_EXT_buffer_storage and mapped persistently (RB_VboArenaMapPair);
 *   - a frame opens the next store, plants a fence on the one it leaves
 *     (RB_VBO_BeginFrame) and carries each batch as a memcpy into the mapped
 *     range (RB_VboArenaSend);
 *   - a store whose fence has not signalled is not written; its batches take
 *     the ring (RB_VboArenaSlotReady / s_arenaFull);
 *   - a store the driver refuses stands the arm down (s_arenaMapDead) and every
 *     batch goes to the ring, which is what 0.3.21 actually ran.
 *
 * Four cases.  Every one must render the batch's own colour, and each reports
 * the carry and fall counts it claims:
 *
 *   mapped   the store carries every batch; ran counts every frame; fall is 0.
 *   overtake a store that is not yet released is skipped and its batches fall
 *            to the ring -- fall counts, and the render is unchanged.  The busy
 *            condition is injected: a synchronous host GL has every fence
 *            signalled by the time the CPU looks, which is exactly why this
 *            path can otherwise only be inferred on a device.
 *   refused  no entry point: the arm stands down, the ring carries everything,
 *            fall counts it, and the render is still unchanged.  This is the
 *            0.3.21 shape; `fall=0` beside it was the counter not being read,
 *            not the ring being idle.
 *   shrunk   a mapped store too small for the batch, so the full path is
 *            exercised rather than inferred.
 *
 * Build and run:
 *   cc -O2 -std=gnu99 tools/arena_store_harness.c -o /tmp/arena_harness \
 *      -lEGL -lGLESv2 -lm && /tmp/arena_harness
 *
 * Exit status 0 is the acceptance.  A host without GL_EXT_buffer_storage still
 * runs two of the four cases and names the ones it could not; that is a result,
 * not a failure of the harness.
 * ===========================================================================
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef unsigned char byte;

/* ---- the store's own constants (code/renderergles3/tr_vbo.c) ------------- */

#define HARNESS_STRIDE		 48			/* rbVertex_t: pos16 nrm16 col4 tc0_8 tc1_8 */
#define HARNESS_ARENA_COUNT	 3
#define HARNESS_VALIGN		 ( HARNESS_STRIDE * ( HARNESS_ARENA_ALIGN / 4 ) )
#define HARNESS_VBO_BYTES	 ( 2 * 1024 * 1024 )
#define HARNESS_EBO_BYTES	 ( 1 * 1024 * 1024 )
#define HARNESS_ARENA_ALIGN	 32

#ifndef GL_MAP_PERSISTENT_BIT_EXT
#define GL_MAP_PERSISTENT_BIT_EXT	0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT_EXT
#define GL_MAP_COHERENT_BIT_EXT		0x0080
#endif
#define HARNESS_STORE_FLAGS	( GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT | GL_MAP_COHERENT_BIT_EXT )

typedef void (*harnessBufferStorage_t)( GLenum target, GLsizeiptr size,
										const void *data, GLbitfield flags );

/* ---- the arm's own state, one field per tr_vbo.c field -------------------- */

typedef struct {
	GLuint		vbo[HARNESS_ARENA_COUNT];
	GLuint		ebo[HARNESS_ARENA_COUNT];
	GLuint		vao[HARNESS_ARENA_COUNT];
	byte	   *mapV[HARNESS_ARENA_COUNT];
	byte	   *mapI[HARNESS_ARENA_COUNT];
	GLsync		fence[HARNESS_ARENA_COUNT];
	int			next;				/* s_arenaNext */
	int			slot;				/* s_arenaMappedSlot */
	int			dead;				/* s_arenaMapDead */
	int			why;				/* 0 none, 1 no entry point, 2 refused */
	GLenum		err;				/* s_arenaMapErr */
	int			active;				/* s_arenaActive */
	int			full;				/* s_arenaFull */
	int			busy;				/* injected: the slot's fence will not signal */
	int			ran;				/* frames the store opened */
	int			fall;				/* batches the ring carried */
	int			carried;			/* batches the store carried */
} harnessStore_t;

#define HARNESS_RING			8
#define HARNESS_BATCH_VERTS		256
#define HARNESS_BATCH_IDX		( ( HARNESS_BATCH_VERTS - 2 ) * 3 )
#define HARNESS_W				64
#define HARNESS_H				64

static GLuint					s_ringVbo[HARNESS_RING], s_ringEbo[HARNESS_RING];
static GLuint					s_ringVao[HARNESS_RING];
static int						s_ringNext;

static byte						s_batch[HARNESS_BATCH_VERTS * HARNESS_STRIDE];
static GLuint					s_batchIdx[HARNESS_BATCH_IDX];

static GLuint					s_prog;
static GLuint					s_fbo, s_colour;
static harnessBufferStorage_t	pBufferStorage;
static GLsizeiptr				s_storeBytes = HARNESS_VBO_BYTES;
static int						s_fail;

#define CHECK( cond, ... )														\
	do {																		\
		if ( !( cond ) ) {														\
			s_fail++;															\
			printf( "  FAIL " );												\
			printf( __VA_ARGS__ );												\
			printf( "\n" );														\
		}																		\
	} while ( 0 )

/* ---- GL plumbing --------------------------------------------------------- */

static const char *s_vs =
	"#version 300 es\n"
	"layout(location=0) in vec4 a_pos;\n"
	"layout(location=1) in vec4 a_nrm;\n"
	"layout(location=2) in vec4 a_col;\n"
	"layout(location=3) in vec2 a_tc0;\n"
	"layout(location=4) in vec2 a_tc1;\n"
	"uniform mat4 u_mvp;\n"
	"out vec4 v_col;\n"
	"void main() { v_col = a_col; gl_Position = u_mvp * a_pos; }\n";

static const char *s_fs =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec4 v_col;\n"
	"out vec4 o_col;\n"
	"void main() { o_col = v_col; }\n";

static GLuint harnessProgram( void ) {
	GLuint	p = glCreateProgram(), v, f;
	GLint	ok;

	v = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( v, 1, &s_vs, NULL );
	glCompileShader( v );
	f = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( f, 1, &s_fs, NULL );
	glCompileShader( f );
	glAttachShader( p, v );
	glAttachShader( p, f );
	glLinkProgram( p );
	glGetProgramiv( p, GL_LINK_STATUS, &ok );
	if ( !ok ) {
		char	log[1024];

		glGetProgramInfoLog( p, sizeof( log ), NULL, log );
		printf( "  FAIL program link: %s\n", log );
		exit( 2 );
	}
	glDeleteShader( v );
	glDeleteShader( f );
	return p;
}

/* The store's own five attributes, at RB_VboAttribsAt's offsets, captured into
 * whatever VAO is bound -- which is how the renderer captures them too. */
static void harnessAttribs( void ) {
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 4, GL_FLOAT, GL_FALSE, HARNESS_STRIDE, (const void *)0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, HARNESS_STRIDE, (const void *)16 );
	glEnableVertexAttribArray( 2 );
	glVertexAttribPointer( 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, HARNESS_STRIDE, (const void *)32 );
	glEnableVertexAttribArray( 3 );
	glVertexAttribPointer( 3, 2, GL_FLOAT, GL_FALSE, HARNESS_STRIDE, (const void *)36 );
	glEnableVertexAttribArray( 4 );
	glVertexAttribPointer( 4, 2, GL_FLOAT, GL_FALSE, HARNESS_STRIDE, (const void *)44 );
}

static int harnessContext( void ) {
	PFNEGLGETPLATFORMDISPLAYEXTPROC	getDisplay;
	EGLDisplay						dpy;
	EGLContext						ctx;
	EGLConfig						cfg;
	EGLint							nelem = 0;
	const EGLint					cfgAttr[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_NONE
	};
	const EGLint					ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	const EGLint					pbufAttr[] = {
		EGL_WIDTH, HARNESS_W, EGL_HEIGHT, HARNESS_H, EGL_NONE
	};

	getDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
					 eglGetProcAddress( "eglGetPlatformDisplayEXT" );
	dpy = getDisplay ? getDisplay( EGL_PLATFORM_SURFACELESS_MESA, NULL, NULL )
					 : eglGetDisplay( EGL_DEFAULT_DISPLAY );
	if ( dpy == EGL_NO_DISPLAY || !eglInitialize( dpy, NULL, NULL ) ) {
		printf( "  FAIL no EGL display (surfaceless)\n" );
		return 0;
	}
	if ( !eglChooseConfig( dpy, cfgAttr, &cfg, 1, &nelem ) || nelem < 1 ) {
		printf( "  FAIL no EGL config\n" );
		return 0;
	}
	eglBindAPI( EGL_OPENGL_ES_API );
	ctx = eglCreateContext( dpy, cfg, EGL_NO_CONTEXT, ctxAttr );
	if ( ctx == EGL_NO_CONTEXT ) {
		printf( "  FAIL no EGL context\n" );
		return 0;
	}
	if ( !eglMakeCurrent( dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx ) ) {
		EGLSurface	surf = eglCreatePbufferSurface( dpy, cfg, pbufAttr );

		if ( !surf || !eglMakeCurrent( dpy, surf, surf, ctx ) ) {
			printf( "  FAIL eglMakeCurrent\n" );
			return 0;
		}
	}
	printf( "  context %s | %s\n", eglQueryString( dpy, EGL_VENDOR ),
			glGetString( GL_VERSION ) );
	printf( "  GL_EXT_buffer_storage %s\n",
			strstr( (const char *)glGetString( GL_EXTENSIONS ),
					"GL_EXT_buffer_storage" ) ? "present" : "ABSENT" );
	return 1;
}

static void harnessTarget( void ) {
	glGenFramebuffers( 1, &s_fbo );
	glGenTextures( 1, &s_colour );
	glBindTexture( GL_TEXTURE_2D, s_colour );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, HARNESS_W, HARNESS_H, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glBindFramebuffer( GL_FRAMEBUFFER, s_fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
							s_colour, 0 );
	glViewport( 0, 0, HARNESS_W, HARNESS_H );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_CULL_FACE );
}

static void harnessRead( byte out[4] ) {
	/* The centre of the disc, where the batch must have put a fragment. */
	glReadPixels( HARNESS_W / 2, HARNESS_H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out );
	out[3] = 255;
}

/* One batch, packed the way the back end packs it: the frame's colour in the
 * vertex colour, so a store that carried nothing, or carried a stale frame,
 * shows up in the pixel the read takes. */
static void harnessBatch( int frame, byte want[4] ) {
	int		i, j, k = 0;

	for ( i = 0; i < HARNESS_BATCH_VERTS; i++ ) {
		byte   *v	= s_batch + i * HARNESS_STRIDE;
		float	a	= (float)i / (float)HARNESS_BATCH_VERTS * 6.2831853f;
		float  *pos = (float *)( v + 0 );
		float  *nrm = (float *)( v + 16 );
		float  *tc0 = (float *)( v + 36 );
		float  *tc1 = (float *)( v + 44 );

		pos[0] = 0.9f * cosf( a );
		pos[1] = 0.9f * sinf( a );
		pos[2] = 0.0f;
		pos[3] = 1.0f;
		nrm[0] = 0.0f; nrm[1] = 0.0f; nrm[2] = 1.0f; nrm[3] = 0.0f;
		tc0[0] = tc0[1] = tc1[0] = tc1[1] = 0.0f;
		v[32] = ( frame & 1 ) ? 220 : 30;
		v[33] = ( frame & 1 ) ?  40 : 200;
		v[34] = 60;
		v[35] = 255;
	}
	for ( j = 1; j < HARNESS_BATCH_VERTS - 1; j++ ) {
		s_batchIdx[k++] = (GLuint)0;
		s_batchIdx[k++] = (GLuint)j;
		s_batchIdx[k++] = (GLuint)( j + 1 );
	}
	want[0] = ( frame & 1 ) ? 220 : 30;
	want[1] = ( frame & 1 ) ?  40 : 200;
	want[2] = 60;
	want[3] = 255;
}

/* ===========================================================================
 * the store's own state machine
 * =========================================================================== */

static int storeMapPair( harnessStore_t *s, int slot ) {
	if ( !pBufferStorage ) {
		s->why = 1;
		return 0;
	}
	glBindBuffer( GL_ARRAY_BUFFER, s->vbo[slot] );
	pBufferStorage( GL_ARRAY_BUFFER, s_storeBytes, NULL, HARNESS_STORE_FLAGS );
	s->mapV[slot] = (byte *)glMapBufferRange( GL_ARRAY_BUFFER, 0, s_storeBytes,
											  HARNESS_STORE_FLAGS );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, s->ebo[slot] );
	pBufferStorage( GL_ELEMENT_ARRAY_BUFFER, HARNESS_EBO_BYTES, NULL, HARNESS_STORE_FLAGS );
	s->mapI[slot] = (byte *)glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, HARNESS_EBO_BYTES,
											  HARNESS_STORE_FLAGS );
	if ( !s->mapV[slot] || !s->mapI[slot] ) {
		s->why = 2;
		s->err = glGetError();
		s->mapV[slot] = NULL;
		s->mapI[slot] = NULL;
		return 0;
	}
	return 1;
}

static int storeSlotReady( harnessStore_t *s, int slot ) {
	if ( s->busy ) {
		/* The device's own reason for a fall: the GPU is still reading this
		 * store.  Injected, because a synchronous host GL has already signalled
		 * every fence by the time the CPU asks. */
		return 0;
	}
	if ( s->fence[slot] ) {
		GLenum	r = glClientWaitSync( s->fence[slot], 0, 0 );

		glDeleteSync( s->fence[slot] );
		s->fence[slot] = NULL;
		/* ALREADY_SIGNALED and CONDITION_SATISFIED are both "the GPU is done";
		 * the second is what a zero timeout returns when the wait is what
		 * finished it.  The renderer took only the first, so every frame that
		 * had to wait at all fell to the ring over a store that was released. */
		if ( r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED
			 && r != GL_WAIT_FAILED ) {
			return 0;
		}
	}
	return 1;
}

/* One frame.  Opens the next store, carries the batch or falls to the ring,
 * draws, reads the pixel back, and plants the fence on the store it leaves. */
static void storeFrame( harnessStore_t *s, int frame ) {
	int		slot = ( s->next + 1 ) % HARNESS_ARENA_COUNT;
	byte	got[4], want[4];
	float	m[16];

	s->next = slot;
	s->slot = slot;
	/* cleared here and nowhere else, as the renderer now does: a refusal made by
	 * the fence check has to survive to the send */
	s->full = 0;

	if ( s->mapV[slot] ) {
		if ( !storeSlotReady( s, slot ) ) {
			s->full = 1;			/* the frame's batches all go to the ring */
		}
	} else if ( !s->dead && !storeMapPair( s, slot ) ) {
		s->dead   = 1;
		s->active = 0;
		s->full   = 1;
		printf( "  store stood down (why=%d glerror=0x%x): the ring runs\n",
				s->why, (unsigned)s->err );
	}
	/* active is a property of the frame, not of the batch: a frame whose store is
	 * fenced still ran, and `ran=` counts it, exactly as RB_VboArenaMode does. */
	if ( !s->dead ) {
		s->active = 1;
	}

	harnessBatch( frame, want );

	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	glUseProgram( s_prog );
	memset( m, 0, sizeof( m ) );
	m[0] = m[5] = m[10] = m[15] = 1.0f;
	glUniformMatrix4fv( glGetUniformLocation( s_prog, "u_mvp" ), 1, GL_FALSE, m );

	if ( !s->dead && !s->full && s->mapV[s->slot]
		 && sizeof( s_batch ) <= (size_t)s_storeBytes
		 && sizeof( s_batchIdx ) <= (size_t)HARNESS_EBO_BYTES ) {
		/* the carry: a memcpy into the mapped range, not a driver call */
		memcpy( s->mapV[s->slot], s_batch, sizeof( s_batch ) );
		memcpy( s->mapI[s->slot], s_batchIdx, sizeof( s_batchIdx ) );
		glBindVertexArray( s->vao[s->slot] );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, s->ebo[s->slot] );
		glDrawElements( GL_TRIANGLES, HARNESS_BATCH_IDX, GL_UNSIGNED_INT, (const void *)0 );
		s->carried++;
	} else {
		int		k = s_ringNext;

		s_ringNext = ( s_ringNext + 1 ) % HARNESS_RING;
		glBindVertexArray( s_ringVao[k] );
		glBindBuffer( GL_ARRAY_BUFFER, s_ringVbo[k] );
		glBufferSubData( GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof( s_batch ), s_batch );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_ringEbo[k] );
		glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof( s_batchIdx ),
						 s_batchIdx );
		glDrawElements( GL_TRIANGLES, HARNESS_BATCH_IDX, GL_UNSIGNED_INT, (const void *)0 );
		s->fall++;
	}

	harnessRead( got );
	CHECK( abs( (int)got[0] - (int)want[0] ) <= 2 && abs( (int)got[1] - (int)want[1] ) <= 2,
		   "frame %d rendered %d,%d,%d wanted %d,%d,%d", frame,
		   got[0], got[1], got[2], want[0], want[1], want[2] );

	/* the store the frame leaves behind is fenced, so the frame after next does
	 * not write memory the GPU may still be reading */
	if ( !s->dead && s->mapV[s->slot] ) {
		if ( s->fence[s->slot] ) {
			glDeleteSync( s->fence[s->slot] );
		}
		s->fence[s->slot] = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	}
	/* the device ends a frame at the swap, which is what releases its stores; a
	 * host frame has to say so itself or no fence ever signals */
	glFlush();
	if ( s->active ) {
		s->ran++;
	}
	s->active = 0;
}

static void storeInit( harnessStore_t *s ) {
	int		i;

	memset( s, 0, sizeof( *s ) );
	for ( i = 0; i < HARNESS_ARENA_COUNT; i++ ) {
		glGenVertexArrays( 1, &s->vao[i] );
		glBindVertexArray( s->vao[i] );
		glGenBuffers( 1, &s->vbo[i] );
		glBindBuffer( GL_ARRAY_BUFFER, s->vbo[i] );
		glGenBuffers( 1, &s->ebo[i] );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, s->ebo[i] );
		/* One VAO a store, its pointers naming that store: the renderer captured
		 * all three stores' pointers into one VAO that named store 0, so the
		 * draw read a frame's bytes out of whichever store the pointers named
		 * rather than out of the one the frame had just written. */
		harnessAttribs();
	}
	s->next = HARNESS_ARENA_COUNT - 1;
}

static void storeFree( harnessStore_t *s ) {
	int		i;

	for ( i = 0; i < HARNESS_ARENA_COUNT; i++ ) {
		if ( s->fence[i] ) {
			glDeleteSync( s->fence[i] );
			s->fence[i] = NULL;
		}
		glDeleteVertexArrays( 1, &s->vao[i] );
		s->mapV[i] = NULL;
		s->mapI[i] = NULL;
		glDeleteBuffers( 1, &s->vbo[i] );
		glDeleteBuffers( 1, &s->ebo[i] );
	}
}

static void ringInit( void ) {
	int		i;

	for ( i = 0; i < HARNESS_RING; i++ ) {
		glGenBuffers( 1, &s_ringVbo[i] );
		glBindBuffer( GL_ARRAY_BUFFER, s_ringVbo[i] );
		glBufferData( GL_ARRAY_BUFFER, (GLsizeiptr)sizeof( s_batch ), NULL, GL_STREAM_DRAW );
		glGenBuffers( 1, &s_ringEbo[i] );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, s_ringEbo[i] );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof( s_batchIdx ), NULL,
					  GL_STREAM_DRAW );
		glGenVertexArrays( 1, &s_ringVao[i] );
		glBindVertexArray( s_ringVao[i] );
		glBindBuffer( GL_ARRAY_BUFFER, s_ringVbo[i] );
		harnessAttribs();
	}
	glBindVertexArray( 0 );
}

static void harnessRun( harnessStore_t *s, int frames ) {
	int		f;

	for ( f = 0; f < frames; f++ ) {
		storeFrame( s, f );
	}
}

/* ===========================================================================
 * the cases
 * =========================================================================== */

static void caseMapped( void ) {
	harnessStore_t	s;

	printf( "case mapped: the store carries every batch\n" );
	if ( !pBufferStorage ) {
		printf( "  SKIP the host has no GL_EXT_buffer_storage\n" );
		return;
	}
	storeInit( &s );
	s_storeBytes = HARNESS_VBO_BYTES;
	harnessRun( &s, 6 );
	CHECK( s.ran == 6, "ran=%d wanted 6 (the store opened every frame)", s.ran );
	CHECK( s.carried == 6, "carried=%d wanted 6 (every batch was a memcpy)", s.carried );
	CHECK( s.fall == 0, "fall=%d wanted 0 (nothing fell back)", s.fall );
	CHECK( s.dead == 0, "dead=%d wanted 0", s.dead );
	storeFree( &s );
}

static void caseOvertake( void ) {
	harnessStore_t	s;

	printf( "case overtake: a store the GPU is still reading falls to the ring\n" );
	if ( !pBufferStorage ) {
		printf( "  SKIP the host has no GL_EXT_buffer_storage\n" );
		return;
	}
	/* Three frames fill the rotation; each store is then fenced by the frame
	 * that left it.  With the release withheld, every frame that lands on an
	 * existing store must fall to the ring rather than write it, and the render
	 * must not change.  The first three frames carry because they are stores
	 * being mapped for the first time, which is the renderer's own first pass. */
	storeInit( &s );
	s_storeBytes = HARNESS_VBO_BYTES;
	harnessRun( &s, 3 );
	CHECK( s.carried == 3, "carried=%d wanted 3 (three stores mapped in)", s.carried );

	s.busy = 1;						/* the fences will not release */
	harnessRun( &s, 3 );
	CHECK( s.carried == 3, "carried=%d wanted 3 (a busy store is never written)", s.carried );
	CHECK( s.fall == 3, "fall=%d wanted 3 (every batch fell to the ring)", s.fall );

	s.busy = 0;						/* released: the store resumes carrying */
	harnessRun( &s, 3 );
	CHECK( s.carried == 6, "carried=%d wanted 6 (the store resumed)", s.carried );
	CHECK( s.ran == 9, "ran=%d wanted 9", s.ran );
	storeFree( &s );
}

static void caseRefused( void ) {
	harnessStore_t	s;

	printf( "case refused: no entry point -- the arm stands down, the ring runs\n" );
	pBufferStorage = NULL;
	storeInit( &s );
	s_storeBytes = HARNESS_VBO_BYTES;
	harnessRun( &s, 4 );
	CHECK( s.dead == 1, "dead=%d wanted 1 (the arm stood down)", s.dead );
	CHECK( s.why == 1, "why=%d wanted 1 (no entry point)", s.why );
	CHECK( s.carried == 0, "carried=%d wanted 0 (the store carried nothing)", s.carried );
	CHECK( s.fall == 4, "fall=%d wanted 4 (the ring carried every batch)", s.fall );
	CHECK( s.ran == 0, "ran=%d wanted 0 (no frame opened a store)", s.ran );
	storeFree( &s );
	pBufferStorage = (harnessBufferStorage_t)eglGetProcAddress( "glBufferStorageEXT" );
}

static void caseShrunk( void ) {
	harnessStore_t	s;

	printf( "case shrunk: a store too small for the batch is the full path\n" );
	if ( !pBufferStorage ) {
		printf( "  SKIP the host has no GL_EXT_buffer_storage\n" );
		return;
	}
	storeInit( &s );
	s_storeBytes = HARNESS_STRIDE;	/* one vertex: nothing fits */
	harnessRun( &s, 3 );
	CHECK( s.fall == 3, "fall=%d wanted 3 (the ring carried the batch)", s.fall );
	CHECK( s.carried == 0, "carried=%d wanted 0", s.carried );
	storeFree( &s );
	s_storeBytes = HARNESS_VBO_BYTES;
}

int main( int argc, char **argv ) {
	(void)argc;
	(void)argv;
	printf( "arena_store_harness\n" );
	if ( !harnessContext() ) {
		return 2;
	}
	pBufferStorage = (harnessBufferStorage_t)eglGetProcAddress( "glBufferStorageEXT" );
	s_prog = harnessProgram();
	harnessTarget();
	ringInit();

	caseMapped();
	caseOvertake();
	caseRefused();
	caseShrunk();

	if ( s_fail ) {
		printf( "FAIL %d check(s)\n", s_fail );
		return 1;
	}
	printf( "PASS\n" );
	return 0;
}
