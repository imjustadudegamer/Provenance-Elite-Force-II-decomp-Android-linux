/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tr_local.h"
#include "gles3_g1_api.h"

#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

vec4_t r_colorWhite = { 1.0, 1.0, 1.0, 1.0 };

/* Front-end copy of backEnd.color2D, so an entry point reads the colour it
 * recorded without touching render-thread state. */
static byte r_color2D[4];

/* Recorded window/scissor coalescing.  A duplicate is skipped only while both
 * caches are valid; R_Invalidate2DWindowCache() drops them from every recorded
 * command that can change the viewport or the scissor box. */
static qboolean r_window2DValid;
static int      r_window2D[4];
static float    r_window2DOrtho[6];
static qboolean r_scissor2DValid;
static int      r_scissor2D[4];

void R_Invalidate2DWindowCache( void ) {
	r_window2DValid = qfalse;
	r_scissor2DValid = qfalse;
}

/* A 3D view reopens depth, so the next overlay draw has to re-issue the phase's
   own state block; a window cached across it would let that draw skip it. */
void RB_2D_InvalidateWindow( void ) {
	r_window2DValid  = qfalse;
	r_scissor2DValid = qfalse;
}

/* ---- [2D:stall] per-call accounting of the 2D entry points ---------------- A menu frame's cost
   is charged to whichever widget calls in here, and the widget probe only sees the widget's total.
   This splits that total per entry point and separates the flush prefix -- which, with a
   single-threaded back end, executes the recorded frame inline -- from the immediate body, so one
   device run names the blocking call instead of inferring it from widget self-times. Every read is
   behind the master probe gate. */
typedef enum {
	D2P_flush = 0, D2P_setcolor, D2P_stretchpic, D2P_stretchpic2, D2P_tilepic,
	D2P_tilepicoffset, D2P_trianglepic, D2P_bgsub, D2P_bgpixels, D2P_box,
	D2P_lineloop, D2P_setwindow, D2P_scissor, D2P_shadertime, D2P_drawstring,
	D2P_SLOTS
} d2phase_t;

static const char *s_d2Name[D2P_SLOTS] = {
	"flush", "setcolor", "stretchpic", "stretchpic2", "tilepic",
	"tilepicoffset", "trianglepic", "bgsub", "bgpixels", "box",
	"lineloop", "setwindow", "scissor", "shadertime", "drawstring"
};

typedef struct { int calls; int usec; int max; } d2row_t;
static d2row_t	s_d2[D2P_SLOTS];
static int		s_d2WinStart;
static int		s_d2WinFrame;
static cvar_t  *s_d2Cvar;
static int		s_d2Stall;
static int		s_d2StallShown;

/* Surfaces the batch actually issued, so the row shows the collapse: calls is
   what the UI asked for, draws is what reached the driver. */
static int		s_d2Batches;

/* Batch coverage. Every vertex the 2D path is asked for is counted here and counted again where it
   is actually handed to the driver; the two must agree or something dropped a quad at a batch
   boundary, which is exactly the bug class that cost the menu its backgrounds. */
static int		s_d2VertsAsked;
static int		s_d2VertsDrawn;
static int		s_d2Drops;

static qboolean D2_On( void ) {
	if ( !s_d2Cvar ) {
		s_d2Cvar = ri.Cvar_Get( "d2_stallStats", "1", 0 );
	}
	return ( PROBE_On() && s_d2Cvar && s_d2Cvar->integer ) ? qtrue : qfalse;
}

static int D2_T0( void ) {
	return D2_On() ? BE_Microseconds() : 0;
}

static void D2_End( int slot, int t0 ) {
	d2row_t	*r;
	int		usec;

	if ( !t0 ) {
		return;
	}

	usec = BE_Microseconds() - t0;
	r    = &s_d2[slot];
	r->calls++;
	r->usec += usec;
	if ( usec > r->max ) {
		r->max = usec;
	}

	/* A single call that outcosts a whole frame is the disease; name it before
	   the roll drowns it in means.  First 8, then one in 64. */
	if ( usec >= 2000 ) {
		s_d2Stall++;
		if ( s_d2StallShown < 8 || ( s_d2Stall % 64 ) == 0 ) {
			s_d2StallShown++;
			ri.Printf( PRINT_ALL, "[2D:stall] call=%s usec=%d calls=%d flushMax=%d f=%d\n",
					   s_d2Name[slot], usec, r->calls, s_d2[D2P_flush].max,
					   RB_FrameCount() );
		}
	}
}

/* The prefix every immediate 2D call shares.  Under a single-threaded back end
   this runs the recorded frame, so its cost belongs to whichever widget called
   first, which is exactly what the widget self-times were charging.  Returns 0
   on the recorded path so the caller's D2_End is a no-op there. */
static int D2_Prepare( void ) {
	if ( glConfig.smpActive ) {
		return 0;
	} else {
		int	f0 = D2_T0();
		int	t0;

		/* Draw the batch only when a recorded scene is actually waiting, so a menu model the UI submits
		   mid-walk still lands *behind* the quads drawn before it. With nothing pending there is nothing
		   to order against, and flushing would break the batch on every call. */
		if ( R_PendingRenderCommands() ) {
			RB_2D_Flush();
		}

		R_SyncRenderThread();
		R_IssuePendingRenderCommands();
		D2_End( D2P_flush, f0 );

		t0 = D2_T0();
		return t0;
	}
}

static void D2_Roll( void ) {
	int		now, win, i, sum, sumCalls;
	int		frames;

	if ( !D2_On() ) {
		return;
	}

	now = BE_Microseconds();
	if ( !s_d2WinStart ) {
		s_d2WinStart = now;
		s_d2WinFrame = RB_FrameCount();
		return;
	}

	win = ( now - s_d2WinStart ) / 1000;
	if ( win < 1000 ) {
		return;
	}
	frames = RB_FrameCount() - s_d2WinFrame;
	if ( frames < 1 ) {
		frames = 1;
	}

	sum = sumCalls = 0;
	for ( i = 0; i < D2P_SLOTS; i++ ) {
		sum += s_d2[i].usec;
		sumCalls += s_d2[i].calls;
	}

	ri.Printf( PRINT_ALL, "[2D:sum] window=%dms frames=%d calls=%d draws=%d vasked=%d vdrawn=%d"
			   " drop=%d sum=%dus flush=%dus body=%dus max=",
			   win, frames, sumCalls, s_d2Batches, s_d2VertsAsked, s_d2VertsDrawn,
			   s_d2Drops, sum, s_d2[D2P_flush].usec, sum - s_d2[D2P_flush].usec );
	for ( i = 0; i < D2P_SLOTS; i++ ) {
		ri.Printf( PRINT_ALL, "%s=%d/%d%s", s_d2Name[i], s_d2[i].usec, s_d2[i].max,
				   ( i + 1 < D2P_SLOTS ) ? " " : "\n" );
	}

	memset( s_d2, 0, sizeof( s_d2 ) );
	s_d2Batches    = 0;
	s_d2VertsAsked = 0;
	s_d2VertsDrawn = 0;
	s_d2Drops      = 0;
	s_d2WinStart = now;
	s_d2WinFrame = RB_FrameCount();
	s_d2StallShown = 0;
}

/* ---- the 2D bodies: run directly when the back end is single-threaded, and
 * replayed on the render thread when the same call was recorded ---- */

/* ---- the 2D batch --------------------------------------------------------- A UI pass issued
   400-650 of these calls a frame and every one was a complete surface of its own: program and
   state setup, a buffer re-specification and a draw. That per-call price is the menu's whole frame
   -- 70-115 us typical, single calls spiking into milliseconds -- so quads now append to one open
   surface and the surface is ended only when the shader, the primitive kind or the view changes.
   Draw order survives because every change flushes first; colour never flushes, because it is
   baked per vertex. */

static shader_t *s_2dBatchShader;
static qboolean  s_2dBatchOpen;
static int       s_2dBatchVerts;
static qboolean  s_2dTriOpen;
static int       s_2dTriBlend;
static int       s_2dTriVerts;
static int       s_2dTriBase;

/* A run is closed well before RB_ImmBegin would flush the stream under us, so
   the coverage count stays exact. */
#define RB_2D_TRI_BOX_MAX 256

void RB_2D_Flush( void ) {
	if ( s_2dBatchOpen ) {
		int	verts;

		/* tess.numVertexes is what is actually about to be handed over; the counter is what the UI asked
		   for. A gap means something reset the tess under the open batch and those quads are gone. */
		verts = tess.numVertexes;
		if ( verts != s_2dBatchVerts ) {
			int			lost = s_2dBatchVerts - verts;
			static int	s_rows;

			s_d2Drops += lost;
			s_rows++;
			if ( D2_On() && ( s_rows <= 8 || ( s_rows % 32 ) == 0 ) ) {
				ri.Printf( PRINT_ALL, "[2D:drop] tess lost=%d asked=%d got=%d f=%d\n",
						   lost, s_2dBatchVerts, verts, RB_FrameCount() );
			}
		}

		/* Cleared before the draw: RB_EndSurface can reach RB_BeginSurface, and
		   that now flushes too, so an still-open flag here would recurse. */
		s_2dBatchOpen   = qfalse;
		s_2dBatchShader = NULL;
		s_2dBatchVerts  = 0;
		s_d2VertsDrawn += verts;
		s_d2Batches++;

		RB_EndSurface();
		GLES3_GlErrAt( "2d_tess_flush" );
	}
	if ( s_2dTriOpen ) {
		int	got;

		/* EndDeferred only closes the record; RB_ImmFlush is what draws it. Deferring left the boxes
		   pending over a frame boundary, and let any tess draw interleave a different program between
		   the boxes' setup and their draw -- both of which are the missing-draw class. Tidy first, flush
		   second: the run was opened after the last tess flush, so the order is the order the calls
		   arrived in. */
		RB_ImmEndDeferred();
		s_2dTriOpen  = qfalse;
		s_2dTriBlend = -1;
		s_d2Batches++;

		got = RB_ImmStreamVerts() - s_2dTriBase;
		if ( got != s_2dTriVerts ) {
			static int	s_rows;

			s_d2Drops += s_2dTriVerts - got;
			s_rows++;
			if ( D2_On() && ( s_rows <= 8 || ( s_rows % 32 ) == 0 ) ) {
				ri.Printf( PRINT_ALL, "[2D:drop] box lost=%d asked=%d got=%d f=%d\n",
						   s_2dTriVerts - got, s_2dTriVerts, got, RB_FrameCount() );
			}
		}
		s_d2VertsDrawn += got;
		RB_ImmFlush();
		GLES3_GlErrAt( "2d_box_flush" );
	}
}

/* The open batch holds a shader_t * that a purge frees; draw it first.  The
   draw census stays: its keys own their strings, so no purge reaches them and
   dropping it here cost the menu-model draws their state matching. */
void RB_2D_OwnersPurged( void ) {
	RB_2D_Flush();
}

static void RB_2D_Batch( shader_t *shader ) {
	if ( s_2dBatchOpen && s_2dBatchShader == shader ) {
		return;
	}

	RB_2D_Flush();
	RB_BeginSurface( shader, 0 );
	s_2dBatchOpen   = qtrue;
	s_2dBatchShader = shader;
	s_2dBatchVerts  = 0;
}

/* Four vertices, six indexes: the same two triangles the strip form built,
   minus the per-quad surface.  The vertices go through RB_Vertex2f so the
   current colour and texcoord reach the store exactly as they did. */
static void RB_2D_BatchQuad( shader_t *shader, const float (*xy)[2], const float (*st)[2] ) {
	int b, i;

	RB_2D_Batch( shader );

	if ( tess.numVertexes + 4 >= SHADER_MAX_VERTEXES - 1 || tess.numIndexes + 6 >= SHADER_MAX_INDEXES - 1 ) {
		RB_2D_Flush();
		RB_BeginSurface( shader, 0 );
		s_2dBatchOpen   = qtrue;
		s_2dBatchShader = shader;
	}

	b = tess.numVertexes;

	for ( i = 0; i < 4; i++ ) {
		RB_Texcoord2f( st[i][0], st[i][1] );
		RB_Vertex2f( xy[i][0], xy[i][1] );
	}

	tess.indexes[tess.numIndexes + 0] = b + 0;
	tess.indexes[tess.numIndexes + 1] = b + 1;
	tess.indexes[tess.numIndexes + 2] = b + 2;
	tess.indexes[tess.numIndexes + 3] = b + 1;
	tess.indexes[tess.numIndexes + 4] = b + 3;
	tess.indexes[tess.numIndexes + 5] = b + 2;
	tess.numIndexes += 6;

	s_2dBatchVerts += 4;
	s_d2VertsAsked += 4;
}

void RB_2D_SetColor( const byte *rgba ) {
	backEnd.color2D[0] = rgba[0];
	backEnd.color2D[1] = rgba[1];
	backEnd.color2D[2] = rgba[2];
	backEnd.color2D[3] = rgba[3];
	qglColor4ubv( backEnd.color2D );
}

void RB_2D_StretchPic( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 ) {
	float xy[4][2], st[4][2];

	/* color2D is bytes: RB_Color4f would scale it a second time and the quad
	   would draw in (byte)(c*255 mod 256). */
	RB_Color4bv(backEnd.color2D);

	xy[0][0] = x;     xy[0][1] = y;     st[0][0] = s1; st[0][1] = t1;
	xy[1][0] = x + w; xy[1][1] = y;     st[1][0] = s2; st[1][1] = t1;
	xy[2][0] = x;     xy[2][1] = y + h; st[2][0] = s1; st[2][1] = t2;
	xy[3][0] = x + w; xy[3][1] = y + h; st[3][0] = s2; st[3][1] = t2;

	RB_2D_BatchQuad( shader, xy, st );
}

void RB_2D_StretchPic2( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy ) {
	float halfWidth, halfHeight;
	float scaledWidth1, scaledHeight1;
	float scaledWidth2, scaledHeight2;
	float xy[4][2], st[4][2];

	halfWidth = w * 0.5f;
	halfHeight = h * 0.5f;
	scaledWidth1 = halfWidth * sy;
	scaledHeight1 = halfHeight * sx;
	scaledWidth2 = halfWidth * sx;
	scaledHeight2 = halfHeight * sy;

	/* color2D is bytes: RB_Color4f would scale it a second time and the quad
	   would draw in (byte)(c*255 mod 256). */
	RB_Color4bv(backEnd.color2D);

	xy[0][0] = x + halfWidth - (scaledWidth2 + -scaledHeight2);
	xy[0][1] = y + halfWidth - scaledHeight1 - scaledWidth1;
	st[0][0] = s1; st[0][1] = t1;

	xy[1][0] = scaledWidth2 - -scaledHeight2 + x + halfWidth;
	xy[1][1] = scaledWidth1 - scaledHeight1 + y + halfWidth;
	st[1][0] = t2; st[1][1] = t1;

	xy[2][0] = x + halfWidth - (scaledWidth2 + scaledHeight2);
	xy[2][1] = scaledHeight1 - scaledWidth1 + y + halfWidth;
	st[2][0] = s1; st[2][1] = s2;

	xy[3][0] = scaledWidth2 - scaledHeight2 + x + halfWidth;
	xy[3][1] = scaledWidth1 + scaledHeight1 + y + halfWidth;
	st[3][0] = t2; st[3][1] = s2;

	RB_2D_BatchQuad( shader, xy, st );
}

void RB_2D_TilePic( shader_t *shader, float x, float y, float w, float h, float picw, float pich ) {
	float xy[4][2], st[4][2];

	/* color2D is bytes: RB_Color4f would scale it a second time and the quad
	   would draw in (byte)(c*255 mod 256). */
	RB_Color4bv(backEnd.color2D);

	xy[0][0] = x;     xy[0][1] = y;     st[0][0] = x / picw;         st[0][1] = y / pich;
	xy[1][0] = x + w; xy[1][1] = y;     st[1][0] = (x + w) / picw;   st[1][1] = y / pich;
	xy[2][0] = x;     xy[2][1] = y + h; st[2][0] = x / picw;         st[2][1] = (y + h) / pich;
	xy[3][0] = x + w; xy[3][1] = y + h; st[3][0] = (x + w) / picw;   st[3][1] = (y + h) / pich;

	RB_2D_BatchQuad( shader, xy, st );
}

void RB_2D_TilePicOffset( shader_t *shader, float x, float y, float w, float h, float picw, float pich, int offsetX, int offsetY ) {
	float xy[4][2], st[4][2];

	/* color2D is bytes: RB_Color4f would scale it a second time and the quad
	   would draw in (byte)(c*255 mod 256). */
	RB_Color4bv(backEnd.color2D);

	xy[0][0] = x + offsetX;     xy[0][1] = y + offsetY;     st[0][0] = x / picw;       st[0][1] = y / pich;
	xy[1][0] = x + offsetX + w; xy[1][1] = y + offsetY;     st[1][0] = (x + w) / picw; st[1][1] = y / pich;
	xy[2][0] = x + offsetX;     xy[2][1] = y + offsetY + h; st[2][0] = x / picw;       st[2][1] = (y + h) / pich;
	xy[3][0] = x + offsetX + w; xy[3][1] = y + offsetY + h; st[3][0] = (x + w) / picw; st[3][1] = (y + h) / pich;

	RB_2D_BatchQuad( shader, xy, st );
}

void RB_2D_TrianglePic( shader_t *shader, const vec2_t vPoints[3], const vec2_t vTexCoords[3] ) {
	int i;

	/* color2D is bytes: RB_Color4f would scale it a second time and the quad
	   would draw in (byte)(c*255 mod 256). */
	RB_Color4bv(backEnd.color2D);

	RB_2D_Flush();
	RB_BeginSurface(shader, 0);

	for (i = 0; i < 3; i++) {
		RB_Texcoord2f(vTexCoords[i][0], vTexCoords[i][1]);
		RB_Vertex2f(vPoints[i][0], vPoints[i][1]);
	}

	RB_StreamEnd();
}

void RB_2D_Box( float x, float y, float w, float h, qboolean blend ) {
	gles3Program_t	*p2d;
	int				 wantBlend = blend ? 1 : 0;

	/* Consecutive boxes share one run and its program setup; blend splits it. Colour is per vertex:
	   u_BaseColor is a uniform on the one 2D family program, shared with RE_DrawBackground_*. */
	if ( !s_2dTriOpen || s_2dTriBlend != wantBlend ) {
		RB_2D_Flush();

		if ( blend ) {
			GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA);
		} else {
			GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
		}

		GL_BindUnit0( tr.whiteImage );
		p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
		GLES3_SetBaseColor4f( p2d, 1.0f, 1.0f, 1.0f, 1.0f );

		RB_ImmBegin( GL_TRIANGLES );
		s_2dTriOpen  = qtrue;
		s_2dTriBlend = wantBlend;
		s_2dTriVerts = 0;
		s_2dTriBase  = RB_ImmStreamVerts();
	}

	RB_ImmColor4f( backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f,
				   backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f );

	RB_ImmVertex2f(x, y);
	RB_ImmVertex2f(x + w, y);
	RB_ImmVertex2f(x + w, y + h);
	RB_ImmVertex2f(x, y);
	RB_ImmVertex2f(x + w, y + h);
	RB_ImmVertex2f(x, y + h);

	s_2dTriVerts   += 6;
	s_d2VertsAsked += 6;

	if ( s_2dTriVerts >= RB_2D_TRI_BOX_MAX * 6 ) {
		RB_2D_Flush();
	}
}

static void RB_DrawStippledLine2D( float x0, float y0, float x1, float y1,
								   int factor, unsigned short mask ) {
	float	dx = x1 - x0, dy = y1 - y0;
	float	len = sqrt( dx * dx + dy * dy );
	float	covered = 0.0f, t0 = 0.0f;
	int		bit = 0;

	if ( mask == 0xffff || len <= 0.0f ) {
		RB_ImmBegin( GL_LINES );
		RB_ImmVertex2f( x0, y0 );
		RB_ImmVertex2f( x1, y1 );
		RB_ImmEnd();
		return;
	}
	if ( factor < 1 ) {
		factor = 1;
	}
	while ( covered < len ) {
		float	seg = ( float )factor;
		float	t1;

		if ( covered + seg > len ) {
			seg = len - covered;
		}
		t1 = ( covered + seg ) / len;
		if ( ( mask >> bit ) & 1 ) {
			RB_ImmBegin( GL_LINES );
			RB_ImmVertex2f( x0 + dx * t0, y0 + dy * t0 );
			RB_ImmVertex2f( x0 + dx * t1, y0 + dy * t1 );
			RB_ImmEnd();
		}
		covered += seg;
		t0 = t1;
		bit = ( bit + 1 ) & 15;
	}
}

void RB_2D_LineLoop( const vec2_t *points, int count, int stipple_factor, int stipple_mask ) {
	int		i;

	GLES3_BeginWire();
	RB_ImmColor4f( backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f,
				   backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f );

	if (stipple_factor) {
		for ( i = 0; i < count; i++ ) {
			int	j = ( i + 1 ) % count;

			RB_DrawStippledLine2D( ( float )( int )points[i][0], ( float )( int )points[i][1],
								   ( float )( int )points[j][0], ( float )( int )points[j][1],
								   stipple_factor, ( unsigned short )stipple_mask );
		}
	} else {
		RB_ImmBegin( GL_LINE_LOOP );
		for ( i = 0; i < count; i++ ) {
			RB_ImmVertex2f( ( float )( int )points[i][0], ( float )( int )points[i][1] );
		}
		RB_ImmEnd();
	}
}

/* The widget's placement in the 2D window.  Retail keeps glViewport/glOrtho
   constant for every widget and carries the placement in the vertices
   (retail-menu-solomatch-final.trace 10103-10124), so the twin does the same. */
float	gles3_2dOrigin[2] = { 0.0f, 0.0f };

void Set2DOrigin( float x, float y ) {
	if ( glConfig.smpActive ) {
		origin2DCommand_t *cmd = ( origin2DCommand_t * )R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_ORIGIN;
			cmd->x = x;
			cmd->y = y;
		}
		return;
	}

	gles3_2dOrigin[0] = x;
	gles3_2dOrigin[1] = y;
}

/* retail's overlay phase is one state block issued at its start, not a choice
   each draw makes (the traced frame: glDepthMask FALSE then glDisable(GL_DEPTH_TEST)
   right after the ortho, and the view restores both).  A drawer that appends quads
   without entering the phase inherits the view's depth and lands behind the wall
   in front of the camera -- the in-game counter that only shows over playback.
   The block it re-issues is the screen window: the last widget's own rect is the
   widget's state, and re-arming it here put a screen-level string inside that
   widget's viewport and scissor a few seconds in, once the in-game panels began
   naming their own. */
void RB_2D_EnterPhase( void ) {
	if ( GLES3_DepthGate2D() && r_window2DValid && r_scissor2DValid ) {
		return;
	}

	RB_SetGL2D();
}

void RB_2D_SetWindow( int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f ) {
	/* The same tuple the batch was opened under is already in force: keep the
	   batch open and issue nothing.  A 3D pass leaves the depth gate open and
	   the 2D pass runs with it shut, or its z=0 quads write half depth. */
	if ( GLES3_DepthGate2D() && r_window2DValid && r_scissor2DValid &&
		 r_window2D[0] == x && r_window2D[1] == y && r_window2D[2] == w && r_window2D[3] == h &&
		 r_window2DOrtho[0] == left && r_window2DOrtho[1] == right && r_window2DOrtho[2] == bottom &&
		 r_window2DOrtho[3] == top && r_window2DOrtho[4] == n && r_window2DOrtho[5] == f &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == w && r_scissor2D[3] == h ) {
		return;
	}

	RB_2D_Flush();

	gles3_2dOrigin[0] = 0.0f;
	gles3_2dOrigin[1] = 0.0f;

	qglViewport(x, y, w, h);
	qglScissor(x, y, w, h);
	SMP_VpProbe( "2d", "Set2DWindow/arg", x, y, w, h, 0, qtrue );
	RB_LoadIdentityProjection();
	RB_OrthoProjection(left, right, bottom, top, n, f);
	RB_LoadIdentityModelView();
	GLES3_ApplyStateBits(GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA);
	GL_Cull( CT_TWO_SIDED );
	RB_ClipPlane_Disable();
	GLES3_DepthGate2DTake();

	r_window2DValid = qtrue;
	r_window2D[0] = x; r_window2D[1] = y; r_window2D[2] = w; r_window2D[3] = h;
	r_window2DOrtho[0] = left; r_window2DOrtho[1] = right; r_window2DOrtho[2] = bottom;
	r_window2DOrtho[3] = top; r_window2DOrtho[4] = n; r_window2DOrtho[5] = f;
	r_scissor2DValid = qtrue;
	r_scissor2D[0] = x; r_scissor2D[1] = y; r_scissor2D[2] = w; r_scissor2D[3] = h;

	if (!backEnd.in2D)
	{
		/* The 3D view is still what is on screen here: the viewport has not been
		   moved and no 2D quad has been issued since it finished. */
		{
			GLint	vp[4];

			qglGetIntegerv( GL_VIEWPORT, vp );
			GLES3_PixelProbe( "view", vp[0] + vp[2] / 2 - 8, vp[1] + vp[3] / 2 - 8, 16, 16 );
		}

		backEnd.refdef.time = ri.Milliseconds();
		backEnd.in2D = qtrue;
		backEnd.refdef.floatTime = backEnd.refdef.time / 1000.0;
        backEnd.shaderStartTime = 0;
	}
}

void RB_2D_Scissor( int x, int y, int width, int height ) {
	if ( r_scissor2DValid &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == width && r_scissor2D[3] == height ) {
		return;
	}

	RB_2D_Flush();

	qglEnable(GL_SCISSOR_TEST);
	qglScissor(x, y, width, height);
	SMP_VpProbe( "scissor", "RE_Scissor/arg", x, y, width, height, 0, qfalse );

	r_scissor2DValid = qtrue;
	r_scissor2D[0] = x; r_scissor2D[1] = y; r_scissor2D[2] = width; r_scissor2D[3] = height;
}

void RB_2D_ShaderTime( float startTime ) {
	if (backEnd.in2D)
	{
		RB_2D_Flush();
		backEnd.shaderStartTime = startTime;
	}
}

/* ---- entry points: record under r_smp, run the body otherwise ---- */

/* A string's renderer cost, in the same roll as the quads. The load-game menu draws ~29 labels a
   frame and the UI probe charges the whole of it to UIFakkLabel; without this slot the roll cannot
   say how much of that is the renderer and how much is the widget's own measure-and-wrap work. */
void D2_StringBegin( int *t0 ) {
	*t0 = D2_T0();
}

void D2_StringEnd( int t0 ) {
	D2_End( D2P_drawstring, t0 );
}

void Draw_SetColor(const vec4_t rgba) {
	int	t0 = D2_T0();

	if (!rgba) {
		rgba = r_colorWhite;
	}

	D2_Roll();

	r_color2D[0] = (byte)(rgba[0] * tr.identityLightByte);
	r_color2D[1] = (byte)(rgba[1] * tr.identityLightByte);
	r_color2D[2] = (byte)(rgba[2] * tr.identityLightByte);
	r_color2D[3] = (byte)(rgba[3] * 255.0);

	if ( glConfig.smpActive ) {
		color2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_COLOR;
			Com_Memcpy( cmd->color, r_color2D, sizeof( r_color2D ) );
		}

		/* The font path and RE_StretchRaw draw immediately on this thread after parking the render
		   thread, and they read backEnd.color2D. Only while this thread owns the context may the
		   front-end copy reach the render-thread state. */
		if ( GLimp_GLMainOwnsContext() ) {
			backEnd.color2D[0] = r_color2D[0];
			backEnd.color2D[1] = r_color2D[1];
			backEnd.color2D[2] = r_color2D[2];
			backEnd.color2D[3] = r_color2D[3];
		}
		return;
	}

	/* Colour is per vertex in both stores, so a change splits neither. */
	RB_2D_SetColor( r_color2D );

	D2_End( D2P_setcolor, t0 );
}

void R_ChangeTextColor(int index) {
	vec4_t color;

	if (index < 0 || index > 7) {
		return;
	}

	color[0] = g_color_table[index][0];
	color[1] = g_color_table[index][1];
	color[2] = g_color_table[index][2];
	color[3] = ( glConfig.smpActive ? r_color2D[3] : backEnd.color2D[3] ) / 255.0;
	Draw_SetColor(color);
}

void Draw_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
	shader_t* shader;
	int		t0 = D2_Prepare();

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	if ( glConfig.smpActive ) {
		stretchPic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_STRETCH_PIC;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->s1 = s1; cmd->t1 = t1; cmd->s2 = s2; cmd->t2 = t2;
		}
		return;
	}

	RB_2D_StretchPic( shader, x, y, w, h, s1, t1, s2, t2 );

	D2_End( D2P_stretchpic, t0 );
}

void Draw_StretchPic2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t hShader) {
	shader_t* shader;
	int		t0 = D2_Prepare();

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	if ( glConfig.smpActive ) {
		stretchPic2Command_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_STRETCH_PIC2;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->s1 = s1; cmd->t1 = t1; cmd->s2 = s2; cmd->t2 = t2;
			cmd->sx = sx; cmd->sy = sy;
		}
		return;
	}

	RB_2D_StretchPic2( shader, x, y, w, h, s1, t1, s2, t2, sx, sy );

	D2_End( D2P_stretchpic2, t0 );
}

void Draw_TilePic(float x, float y, float w, float h, qhandle_t hShader) {
	shader_t* shader;
	float		picw, pich;
	int			t0 = D2_Prepare();

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	picw = shader->unfoggedStages[0]->bundle[0].image[0]->uploadWidth;
	pich = shader->unfoggedStages[0]->bundle[0].image[0]->uploadHeight;

	if ( glConfig.smpActive ) {
		tilePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TILE_PIC;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->picw = picw; cmd->pich = pich;
			cmd->offsetX = 0; cmd->offsetY = 0;
		}
		return;
	}

	RB_2D_TilePic( shader, x, y, w, h, picw, pich );

	D2_End( D2P_tilepic, t0 );
}

void Draw_TilePicOffset(float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY) {
	shader_t* shader;
	float		picw, pich;
	int			t0 = D2_Prepare();

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if (w <= 0) {
		w = shader->unfoggedStages[0]->bundle[0].image[0]->width;
		h = shader->unfoggedStages[0]->bundle[0].image[0]->height;
	}

	picw = shader->unfoggedStages[0]->bundle[0].image[0]->uploadWidth;
	pich = shader->unfoggedStages[0]->bundle[0].image[0]->uploadHeight;

	if ( glConfig.smpActive ) {
		tilePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TILE_PIC_OFFSET;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->picw = picw; cmd->pich = pich;
			cmd->offsetX = offsetX; cmd->offsetY = offsetY;
		}
		return;
	}

	RB_2D_TilePicOffset( shader, x, y, w, h, picw, pich, offsetX, offsetY );

	D2_End( D2P_tilepicoffset, t0 );
}

void Draw_TrianglePic(const vec2_t vPoints[3], const vec2_t vTexCoords[3], qhandle_t hShader) {
	shader_t* shader;
	int			t0 = D2_Prepare();

	if (hShader) {
		shader = R_GetShaderByHandle(hShader);
	}
	else {
		shader = tr.defaultShader;
	}

	if ( glConfig.smpActive ) {
		trianglePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TRIANGLE_PIC;
			cmd->shader = shader;
			Com_Memcpy( cmd->points, vPoints, sizeof( cmd->points ) );
			Com_Memcpy( cmd->texCoords, vTexCoords, sizeof( cmd->texCoords ) );
		}
		return;
	}

	RB_2D_TrianglePic( shader, vPoints, vTexCoords );

	D2_End( D2P_trianglepic, t0 );
}

void RE_DrawBackground_TexSubImage(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	GLenum	format = GL_RGB;
	int		w, h;

	w = glConfig.vidWidth;
	h = glConfig.vidHeight;

	R_SyncRenderThread();
	R_FlushRenderCommands();
	qglFinish();

	if (bgr) {
		int	n = cols * rows * 3;
		int	i;

		upload = (byte *)ri.Malloc(n);
		for (i = 0; i + 2 < n; i += 3) {
			upload[i + 0] = data[i + 2];
			upload[i + 1] = data[i + 1];
			upload[i + 2] = data[i + 0];
		}
	}

	GL_BindUnit0(tr.scratchImage);

	if (cols == tr.scratchImage->width && rows == tr.scratchImage->height && format == tr.scratchImage->internalFormat)
	{
		qglTexSubImage2D(3553, 0, 0, 0, cols, rows, format, 5121, upload);
	}
	else
	{
		tr.scratchImage->uploadWidth = cols;
		tr.scratchImage->uploadHeight = rows;
		tr.scratchImage->internalFormat = format;
		qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cols, rows, 0, format, 5121, upload);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 9729.0);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 9729.0);
	}

	if (bgr) {
		ri.Free( upload );
	}

	GL_Cull( CT_TWO_SIDED );
	GLES3_ApplyStateBits( GLS_DEPTHTEST_DISABLE );

	{
		gles3Program_t	*p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );

		/* State the multiplier rather than inherit whatever ran last. */
		GLES3_SetBaseColor4f( p2d, 1.0f, 1.0f, 1.0f, 1.0f );
		RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		RB_ImmBegin( GL_TRIANGLES );

		RB_ImmTexCoord2f(0.5 / (GLfloat)cols, ((GLfloat)rows - 0.5) / rows);
		RB_ImmVertex2f(0, 0);

		RB_ImmTexCoord2f(((GLfloat)cols - 0.5) / cols, ((GLfloat)rows - 0.5) / rows);
		RB_ImmVertex2f(w, 0);

		RB_ImmTexCoord2f(((GLfloat)cols - 0.5) / cols, 0.5 / (GLfloat)rows);
		RB_ImmVertex2f(w, h);

		RB_ImmTexCoord2f(0.5 / (GLfloat)cols, ((GLfloat)rows - 0.5) / rows);
		RB_ImmVertex2f(0, 0);

		RB_ImmTexCoord2f(((GLfloat)cols - 0.5) / cols, 0.5 / (GLfloat)rows);
		RB_ImmVertex2f(w, h);

		RB_ImmTexCoord2f(0.5 / (GLfloat)rows, 0.5 / (GLfloat)rows);
		RB_ImmVertex2f(0, h);

		RB_ImmEnd();
	}
}

void RE_DrawBackground_DrawPixels(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	float	zoomX, zoomY, qw, qh;
	gles3Program_t	*p2d;

	R_SyncRenderThread();
	R_FlushRenderCommands();

	if (bgr) {
		int	n = cols * rows * 3;
		int	i;

		upload = (byte *)ri.Malloc(n);
		for (i = 0; i + 2 < n; i += 3) {
			upload[i + 0] = data[i + 2];
			upload[i + 1] = data[i + 1];
			upload[i + 2] = data[i + 0];
		}
	}

	GL_BindUnit0( tr.scratchImage );
	tr.scratchImage->uploadWidth = cols;
	tr.scratchImage->uploadHeight = rows;
	tr.scratchImage->internalFormat = GL_RGB;
	qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, cols, rows, 0, GL_RGB, GL_UNSIGNED_BYTE, upload );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	if (bgr) {
		ri.Free( upload );
	}

	GLES3_ApplyStateBits(0);

	zoomX = (float)glConfig.vidWidth / (float)rows;
	zoomY = (float)glConfig.vidHeight / (float)cols;
	qw = (float)cols * zoomX;
	qh = (float)rows * zoomY;

	p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
	GLES3_SetBaseColor4f( p2d, 1.0f, 1.0f, 1.0f, 1.0f );

	RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	RB_ImmBegin( GL_TRIANGLES );
	RB_ImmTexCoord2f( 0, 0 ); RB_ImmVertex2f( 0, 0 );
	RB_ImmTexCoord2f( 1, 0 ); RB_ImmVertex2f( qw, 0 );
	RB_ImmTexCoord2f( 1, 1 ); RB_ImmVertex2f( qw, qh );
	RB_ImmTexCoord2f( 0, 0 ); RB_ImmVertex2f( 0, 0 );
	RB_ImmTexCoord2f( 1, 1 ); RB_ImmVertex2f( qw, qh );
	RB_ImmTexCoord2f( 0, 1 ); RB_ImmVertex2f( 0, qh );
	RB_ImmEnd();
}

void AddBox(float x, float y, float w, float h) {
	int		t0;

	if (!r_draw2d->integer) {
		return;
	}

	if ( !glConfig.smpActive ) {
		t0 = D2_Prepare();
		RB_2D_Box( x, y, w, h, qfalse );
		D2_End( D2P_box, t0 );
		return;
	}

	{
		box2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_BOX;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->blend = 0;
		}
	}
}

void DrawBox(float x, float y, float w, float h) {
	int		t0;

	if (!r_draw2d->integer) {
		return;
	}

	if ( !glConfig.smpActive ) {
		t0 = D2_Prepare();
		RB_2D_Box( x, y, w, h, qtrue );
		D2_End( D2P_box, t0 );
		return;
	}

	{
		box2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_BOX;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->blend = 1;
		}
	}
}

void DrawLineLoop(const vec2_t* points, int count, int stipple_factor, int stipple_mask) {
	int		t0;

	if (!r_draw2d->integer) {
		return;
	}

	if ( !glConfig.smpActive ) {
		t0 = D2_Prepare();
		RB_2D_LineLoop( points, count, stipple_factor, stipple_mask );
		D2_End( D2P_lineloop, t0 );
		return;
	}

	if ( count <= 0 ) {
		return;
	}

	{
		int bytes = PAD( (int)sizeof( lineLoop2DCommand_t ) + count * (int)sizeof( vec2_t ), sizeof( void * ) );
		lineLoop2DCommand_t *cmd = R_GetCommandBuffer( bytes );

		if ( cmd ) {
			cmd->commandId = RC_2D_LINE_LOOP;
			cmd->count = count;
			cmd->stipple_factor = stipple_factor;
			cmd->stipple_mask = stipple_mask;
			Com_Memcpy( (vec2_t *)( cmd + 1 ), points, count * sizeof( vec2_t ) );
		}
	}
}

void Set2DWindow(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f) {
	int		t0;

	if ( !glConfig.smpActive ) {
		t0 = D2_Prepare();
		RB_2D_SetWindow( x, y, w, h, left, right, bottom, top, n, f );
		D2_End( D2P_setwindow, t0 );
		return;
	}

	if ( r_window2DValid && r_scissor2DValid &&
		 r_window2D[0] == x && r_window2D[1] == y && r_window2D[2] == w && r_window2D[3] == h &&
		 r_window2DOrtho[0] == left && r_window2DOrtho[1] == right && r_window2DOrtho[2] == bottom &&
		 r_window2DOrtho[3] == top && r_window2DOrtho[4] == n && r_window2DOrtho[5] == f &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == w && r_scissor2D[3] == h ) {
		Set2DOrigin( 0.0f, 0.0f );
		return;
	}

	{
		setWindow2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SET_WINDOW;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->left = left; cmd->right = right; cmd->bottom = bottom; cmd->top = top;
			cmd->n = n; cmd->f = f;

			r_window2DValid = qtrue;
			r_window2D[0] = x; r_window2D[1] = y; r_window2D[2] = w; r_window2D[3] = h;
			r_window2DOrtho[0] = left; r_window2DOrtho[1] = right; r_window2DOrtho[2] = bottom;
			r_window2DOrtho[3] = top; r_window2DOrtho[4] = n; r_window2DOrtho[5] = f;
			r_scissor2DValid = qtrue;
			r_scissor2D[0] = x; r_scissor2D[1] = y; r_scissor2D[2] = w; r_scissor2D[3] = h;
		}
	}

	/* the window defines the space: the placement the last widget set is gone */
	Set2DOrigin( 0.0f, 0.0f );
}

void RE_Scissor(int x, int y, int width, int height) {
	int		t0;

	if ( !glConfig.smpActive ) {
		t0 = D2_T0();
		RB_2D_Scissor( x, y, width, height );
		D2_End( D2P_scissor, t0 );
		return;
	}

	if ( r_scissor2DValid &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == width && r_scissor2D[3] == height ) {
		return;
	}

	{
		scissor2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SCISSOR;
			cmd->x = x; cmd->y = y; cmd->width = width; cmd->height = height;

			r_scissor2DValid = qtrue;
			r_scissor2D[0] = x; r_scissor2D[1] = y; r_scissor2D[2] = width; r_scissor2D[3] = height;
		}
	}
}

void Set2DInitialShaderTime(float startTime)
{
	if ( !glConfig.smpActive ) {
		int		t0 = D2_T0();

		RB_2D_ShaderTime( startTime );
		D2_End( D2P_shadertime, t0 );
		return;
	}

	{
		shaderTime2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SHADER_TIME;
			cmd->startTime = startTime;
		}
	}
}
