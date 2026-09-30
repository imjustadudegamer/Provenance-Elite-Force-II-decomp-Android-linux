/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
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

#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

vec4_t r_colorWhite = { 1.0, 1.0, 1.0, 1.0 };

/* Front-end copy of backEnd.color2D, so an entry point reads the colour it
 * recorded without touching render-thread state. */
static byte r_color2D[4];

/* the reference renderer's tr_draw.c carries this note in full */
static qboolean r_window2DValid;
static int      r_window2D[4];
static float    r_window2DOrtho[6];
static qboolean r_scissor2DValid;
static int      r_scissor2D[4];

void R_Invalidate2DWindowCache( void ) {
	r_window2DValid = qfalse;
	r_scissor2DValid = qfalse;
}

/* the reference renderer's tr_draw.c carries this note in full */
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

/* Surfaces the batch issued: calls is what the UI asked, draws what reached the driver. */
static int		s_d2Batches;

/* the reference renderer's tr_draw.c carries this note in full */
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

	/* A single call that outcosts a frame is the disease: first 8, then 1 in 64. */
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

/* the reference renderer's tr_draw.c carries this note in full */
static int D2_Prepare( void ) {
	/* The twin's prefix flushed the recorded frame; there is no immediate mode
	   here, so the FLUSH collapses to nothing.  The timestamp must not: D2_End
	   drops a zero t0, so returning 0 here left every slot whose recorder goes
	   through D2_Prepare -- box, stretchpic, stretchpic2, tilepic, setwindow,
	   scissor, lineloop, trianglepic -- reading 0 in the [2D:sum] roll, which
	   reads as "the boxes never reach the 2D path" when the box path is exactly
	   what they are.  Only setcolor and drawstring, which take D2_T0 directly,
	   were ever counted. */
	return D2_T0();
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

static shader_t *s_2dBatchShader;
static qboolean  s_2dBatchOpen;
static int       s_2dBatchVerts;
static qboolean  s_2dTriOpen;
static int       s_2dTriBlend;
static int       s_2dTriVerts;
static int       s_2dTriBase;

#define RB_2D_TRI_BOX_MAX 256

/* ---- the 2D batch's spans ------------------------------------------------ */

/* A widget sets its own window (and often its own scissor) before it draws, so
   a menu's 2D run is a window change every few quads.  Ending the surface at
   each one re-did the stage pass for a handful of vertices: pipeline lookup,
   texture and descriptor bind, uniform ring slice, index copy, draw.  None of
   that is what changed -- only the ortho push constant and the viewport and
   scissor, which are dynamic state -- so the batch now keeps one span per state
   and the draw re-applies each span's copy before the span's own indexed draw.
   The quads, their order and their clip rects are unchanged; what goes away is
   the surface work per widget.

   Each span is (firstIndex, ortho, window rect, scissor rect).  Span i draws
   the indexes [first[i], first[i+1]) of the batch's index range, and the last
   span runs to the batch's end.  Under two spans the batch draws exactly as it
   did before: one draw, with the state already in the command buffer. */
#define RB_2D_SPAN_MAX	256

static float	s_2dSpanOrtho[RB_2D_SPAN_MAX][16];
static int		s_2dSpanWin[RB_2D_SPAN_MAX][4];
static int		s_2dSpanScn[RB_2D_SPAN_MAX][4];
static int		s_2dSpanFirst[RB_2D_SPAN_MAX];
static int		s_2dSpanCount;

void RB_2D_SpanReset( void ) {
	s_2dSpanCount = 0;
}

int RB_2D_SpanCount( void ) {
	return s_2dSpanCount;
}

/* A span's index range and the state its quads were authored under.  count is 0
   for a span that got no quads, which the draw skips. */
void RB_2D_SpanGet( int i, int *firstIndex, int *count, float *ortho, int *win, int *scn ) {
	int	end;

	if ( i < 0 || i >= s_2dSpanCount ) {
		*firstIndex = 0; *count = 0;
		return;
	}

	end = ( i + 1 < s_2dSpanCount ) ? s_2dSpanFirst[i + 1] : tess.numIndexes;
	*firstIndex = s_2dSpanFirst[i];
	*count = end - s_2dSpanFirst[i];
	memcpy( ortho, s_2dSpanOrtho[i], sizeof( s_2dSpanOrtho[i] ) );
	memcpy( win, s_2dSpanWin[i], sizeof( s_2dSpanWin[i] ) );
	memcpy( scn, s_2dSpanScn[i], sizeof( s_2dSpanScn[i] ) );
}

/* The walk is about to apply a new 2D state with the batch still open: the
   quads already queued were authored under the state in force, which closes as
   a span.  qfalse means the split cannot be represented -- no window has been
   set, so the state those quads carry cannot be named, or the table is full --
   and the caller flushes the batch instead, which is what every window change
   did before. */
qboolean RB_2D_SpanBreak( void ) {
	if ( !s_2dBatchOpen || tess.numIndexes == 0 ) {
		return qtrue;			/* nothing queued under the old state */
	}
	if ( s_2dSpanCount + 2 > RB_2D_SPAN_MAX ) {
		return qfalse;
	}
	if ( s_2dSpanCount == 0 ) {
		if ( !VK_Draw_2DStateGet( s_2dSpanOrtho[0], s_2dSpanWin[0], s_2dSpanScn[0] ) ) {
			return qfalse;
		}
		s_2dSpanFirst[0] = 0;
		s_2dSpanCount = 1;
	}
	return qtrue;
}

/* After the new state is applied: the quads that follow belong to it.  The
   state is always nameable here -- VK_Draw_Set2D and VK_Draw_SetScissor both
   set it -- and RB_2D_SpanBreak reserved the room, so this does not fail. */
qboolean RB_2D_SpanNote( void ) {
	float	ortho[16];
	int		win[4], scn[4];

	if ( !s_2dBatchOpen ) {
		return qtrue;
	}
	if ( s_2dSpanCount >= RB_2D_SPAN_MAX
		 || !VK_Draw_2DStateGet( ortho, win, scn ) ) {
		VK_Guard_Row( "VK:span", "site=note detail=\"no room or no state\" n=%d idx=%d",
					  s_2dSpanCount, tess.numIndexes );
		s_2dSpanCount = 0;
		return qfalse;
	}

	if ( s_2dSpanCount > 0 && s_2dSpanFirst[s_2dSpanCount - 1] == tess.numIndexes ) {
		s_2dSpanCount--;		/* that segment got no quads: this replaces it */
	}

	s_2dSpanFirst[s_2dSpanCount] = tess.numIndexes;
	memcpy( s_2dSpanOrtho[s_2dSpanCount], ortho, sizeof( ortho ) );
	memcpy( s_2dSpanWin[s_2dSpanCount], win, sizeof( win ) );
	memcpy( s_2dSpanScn[s_2dSpanCount], scn, sizeof( scn ) );
	s_2dSpanCount++;
	return qtrue;
}

/* The immediate run of boxes and lines: its vertices carry no span of their
   own, so a window change still closes it, as it always did. */
void RB_2D_FlushImmediate( void ) {
	if ( s_2dTriOpen ) {
		int	got;

/* the reference renderer's tr_draw.c carries this note in full */
		RB_ImmEndDeferred();
		s_2dTriOpen  = qfalse;
		s_2dTriBlend = -1;
		s_d2Batches++;

		got = RB_ImmStreamVerts() - s_2dTriBase;
		if ( got != s_2dTriVerts ) {
			static int	s_rows;

			s_d2Drops += s_2dTriVerts - got;
			s_rows++;
			if ( s_rows <= 8 || ( s_rows % 32 ) == 0 ) {
				ri.Printf( PRINT_ALL, "[2D:drop] box lost=%d asked=%d got=%d f=%d\n",
						   s_2dTriVerts - got, s_2dTriVerts, got, RB_FrameCount() );
			}
		}
		s_d2VertsDrawn += got;
		RB_ImmFlush();
		/* Same: the pipeline bind and the draw returned their own result. */
	}
}

void RB_2D_Flush( void ) {
	if ( s_2dBatchOpen ) {
		int	verts;

/* the reference renderer's tr_draw.c carries this note in full */
		verts = tess.numVertexes;
		if ( verts != s_2dBatchVerts ) {
			int			lost = s_2dBatchVerts - verts;
			static int	s_rows;

			s_d2Drops += lost;
			s_rows++;
			if ( s_rows <= 8 || ( s_rows % 32 ) == 0 ) {
				ri.Printf( PRINT_ALL, "[2D:drop] tess lost=%d asked=%d got=%d f=%d\n",
						   lost, s_2dBatchVerts, verts, RB_FrameCount() );
			}
		}

		/* Cleared before the draw: RB_EndSurface can reach RB_BeginSurface, which flushes too. */
		s_2dBatchOpen   = qfalse;
		s_2dBatchShader = NULL;
		s_2dBatchVerts  = 0;
		s_d2VertsDrawn += verts;
		s_d2Batches++;

		RB_EndSurface();
		/* The tess path reports its own VkResult at the call site; nothing polls here. */
		RB_2D_SpanReset();
	}
	RB_2D_FlushImmediate();
}

/* the reference renderer's tr_draw.c carries this note in full */
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

/* the reference renderer's tr_draw.c carries this note in full */
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

	/* Colour is per vertex in the ring; the next quad picks it up, nothing to push. */
}

void RB_2D_StretchPic( shader_t *shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 ) {
	float xy[4][2], st[4][2];

	/* color2D is bytes: RB_Color4f would scale it a second time. */
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

	/* color2D is bytes: RB_Color4f would scale it a second time. */
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

	/* color2D is bytes: RB_Color4f would scale it a second time. */
	RB_Color4bv(backEnd.color2D);

	xy[0][0] = x;     xy[0][1] = y;     st[0][0] = x / picw;         st[0][1] = y / pich;
	xy[1][0] = x + w; xy[1][1] = y;     st[1][0] = (x + w) / picw;   st[1][1] = y / pich;
	xy[2][0] = x;     xy[2][1] = y + h; st[2][0] = x / picw;         st[2][1] = (y + h) / pich;
	xy[3][0] = x + w; xy[3][1] = y + h; st[3][0] = (x + w) / picw;   st[3][1] = (y + h) / pich;

	RB_2D_BatchQuad( shader, xy, st );
}

void RB_2D_TilePicOffset( shader_t *shader, float x, float y, float w, float h, float picw, float pich, int offsetX, int offsetY ) {
	float xy[4][2], st[4][2];

	/* color2D is bytes: RB_Color4f would scale it a second time. */
	RB_Color4bv(backEnd.color2D);

	xy[0][0] = x + offsetX;     xy[0][1] = y + offsetY;     st[0][0] = x / picw;       st[0][1] = y / pich;
	xy[1][0] = x + offsetX + w; xy[1][1] = y + offsetY;     st[1][0] = (x + w) / picw; st[1][1] = y / pich;
	xy[2][0] = x + offsetX;     xy[2][1] = y + offsetY + h; st[2][0] = x / picw;       st[2][1] = (y + h) / pich;
	xy[3][0] = x + offsetX + w; xy[3][1] = y + offsetY + h; st[3][0] = (x + w) / picw; st[3][1] = (y + h) / pich;

	RB_2D_BatchQuad( shader, xy, st );
}

void RB_2D_TrianglePic( shader_t *shader, const vec2_t vPoints[3], const vec2_t vTexCoords[3] ) {
	int i;

	/* color2D is bytes: RB_Color4f would scale it a second time. */
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
	int wantBlend = blend ? 1 : 0;

	if ( !s_2dTriOpen || s_2dTriBlend != wantBlend ) { /* state is the call's */
		RB_2D_Flush();

		RB_ImmBeginWithImage( VK_IMM_TRIANGLES, tr.whiteImage,
			wantBlend ? ( VKS_DEPTHTEST_DISABLE | VKS_SRCBLEND_SRC_ALPHA
						  | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
					  : ( VKS_DEPTHTEST_DISABLE | VKS_SRCBLEND_ONE | VKS_DSTBLEND_ONE ) );
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
	RB_ImmCommit();

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
		RB_ImmBegin( VK_IMM_LINES );
		RB_ImmVertex2f( x0, y0 );
		RB_ImmVertex2f( x1, y1 );
		RB_ImmCommit();
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
			RB_ImmBegin( VK_IMM_LINES );
			RB_ImmVertex2f( x0 + dx * t0, y0 + dy * t0 );
			RB_ImmVertex2f( x0 + dx * t1, y0 + dy * t1 );
			RB_ImmCommit();
			RB_ImmEnd();
		}
		covered += seg;
		t0 = t1;
		bit = ( bit + 1 ) & 15;
	}
}

void RB_2D_LineLoop( const vec2_t *points, int count, int stipple_factor, int stipple_mask ) {
	int		i;

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
		RB_ImmBegin( VK_IMM_LINE_LOOP );
		for ( i = 0; i < count; i++ ) {
			RB_ImmVertex2f( ( float )( int )points[i][0], ( float )( int )points[i][1] );
		}
		RB_ImmCommit();
		RB_ImmEnd();
	}
}

/* RB_2D_SetWindow and RB_2D_Scissor used to live here.  They were the walk's
   names, but the walk reaches the window and the scissor through the
   RC_2D_SET_WINDOW and RC_2D_SCISSOR cases in tr_cmds.c, so nothing in this
   lane ever called them: they were dead.  What they did do was flush the 2D
   batch from whatever thread called them, which is the front end's, and that
   is the walk's tess store -- a latent cross-thread touch of the batch state.
   Set2DWindow and RE_Scissor below are the live recorders. */

void RB_2D_ShaderTime( float startTime ) {
	if (backEnd.in2D)
	{
		RB_2D_Flush();
		backEnd.shaderStartTime = startTime;
	}
}

/* the reference renderer's tr_draw.c carries this note in full */
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

	/* Colour is per vertex in the ring: the next quad the walk writes picks it up. */
	{
		color2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_COLOR;
			Com_Memcpy( cmd->color, r_color2D, sizeof( r_color2D ) );
		}
	}

	backEnd.color2D[0] = r_color2D[0];
	backEnd.color2D[1] = r_color2D[1];
	backEnd.color2D[2] = r_color2D[2];
	backEnd.color2D[3] = r_color2D[3];

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
	color[3] = backEnd.color2D[3] / 255.0;
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

	{
		stretchPic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_STRETCH_PIC;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->s1 = s1; cmd->t1 = t1; cmd->s2 = s2; cmd->t2 = t2;
		}
	}

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

	{
		stretchPic2Command_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_STRETCH_PIC2;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->s1 = s1; cmd->t1 = t1; cmd->s2 = s2; cmd->t2 = t2;
			cmd->sx = sx; cmd->sy = sy;
		}
	}

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

	{
		tilePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TILE_PIC;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->picw = picw; cmd->pich = pich;
			cmd->offsetX = 0; cmd->offsetY = 0;
		}
	}

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

	{
		tilePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TILE_PIC_OFFSET;
			cmd->shader = shader;
			cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
			cmd->picw = picw; cmd->pich = pich;
			cmd->offsetX = offsetX; cmd->offsetY = offsetY;
		}
	}

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

	{
		trianglePic2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_TRIANGLE_PIC;
			cmd->shader = shader;
			Com_Memcpy( cmd->points, vPoints, sizeof( cmd->points ) );
			Com_Memcpy( cmd->texCoords, vTexCoords, sizeof( cmd->texCoords ) );
		}
	}

	D2_End( D2P_trianglepic, t0 );
}

void RE_DrawBackground_TexSubImage(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	int		w, h;

	w = vkConfig.vidWidth;
	h = vkConfig.vidHeight;

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

	/* A Vulkan image cannot be re-specified in place: a new size is a new texture object. */
	R_UploadScratchImage( cols, rows, upload );

	if (bgr) {
		ri.Free( upload );
	}

	RB_ImmBeginWithImage( VK_IMM_TRIANGLES, tr.scratchImage, VKS_DEPTHTEST_DISABLE );

	/* State the multiplier rather than inherit whatever ran last. */
	RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	{
		RB_ImmTexCoord2f(0.5 / (float)cols, ((float)rows - 0.5) / rows);
		RB_ImmVertex2f(0, 0);

		RB_ImmTexCoord2f(((float)cols - 0.5) / cols, ((float)rows - 0.5) / rows);
		RB_ImmVertex2f(w, 0);

		RB_ImmTexCoord2f(((float)cols - 0.5) / cols, 0.5 / (float)rows);
		RB_ImmVertex2f(w, h);

		RB_ImmTexCoord2f(0.5 / (float)cols, ((float)rows - 0.5) / rows);
		RB_ImmVertex2f(0, 0);

		RB_ImmTexCoord2f(((float)cols - 0.5) / cols, 0.5 / (float)rows);
		RB_ImmVertex2f(w, h);

		RB_ImmTexCoord2f(0.5 / (float)rows, 0.5 / (float)rows);
		RB_ImmVertex2f(0, h);

		RB_ImmEnd();
	}
}

void RE_DrawBackground_DrawPixels(int cols, int rows, int bgr, byte* data) {
	byte	*upload = data;
	float	zoomX, zoomY, qw, qh;

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

	R_UploadScratchImage( cols, rows, upload );

	if (bgr) {
		ri.Free( upload );
	}

	zoomX = (float)vkConfig.vidWidth / (float)rows;
	zoomY = (float)vkConfig.vidHeight / (float)cols;
	qw = (float)cols * zoomX;
	qh = (float)rows * zoomY;

	RB_ImmBeginWithImage( VK_IMM_TRIANGLES, tr.scratchImage, 0 );	/* no state applied */
	RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	RB_ImmTexCoord2f( 0, 0 ); RB_ImmVertex2f( 0, 0 );
	RB_ImmTexCoord2f( 1, 0 ); RB_ImmVertex2f( qw, 0 );
	RB_ImmTexCoord2f( 1, 1 ); RB_ImmVertex2f( qw, qh );
	RB_ImmTexCoord2f( 0, 0 ); RB_ImmVertex2f( 0, 0 );
	RB_ImmTexCoord2f( 1, 1 ); RB_ImmVertex2f( qw, qh );
	RB_ImmTexCoord2f( 0, 1 ); RB_ImmVertex2f( 0, qh );
	RB_ImmCommit();
	RB_ImmEnd();
}

void R_UploadScratchRGBA( int cols, int rows, const byte *rgba )
{
	image_t	*image = tr.scratchImage;
	int		n = cols * rows;
	int		handle = -1;

	if ( !image ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "scratch image missing", VK_ERROR_INITIALIZATION_FAILED );
		return;
	}

	if ( image->texnum && image->uploadWidth == cols && image->uploadHeight == rows ) {
		VK_Texture_Upload( (int)image->texnum, 0, cols, rows, rgba, (size_t)n * 4u );
		VK_Texture_Publish( (int)image->texnum );
		return;
	}

	if ( image->texnum ) {
		VK_Texture_Destroy( (int)image->texnum );
		image->texnum = 0;
	}

	if ( !VK_Texture_Create( cols, rows, 1, VK_FORMAT_R8G8B8A8_UNORM, &handle ) ) {
		return;
	}

	image->texnum = (unsigned int)handle;
	image->uploadWidth = cols;
	image->uploadHeight = rows;
	image->width = cols;
	image->height = rows;
	image->internalFormat = VK_TF_RGB;
	image->numMipmaps = 0;

	VK_Texture_Upload( handle, 0, cols, rows, rgba, (size_t)n * 4u );
	VK_Texture_Publish( handle );
}

void R_UploadScratchImage( int cols, int rows, const byte *rgb )
{
	byte	*rgba;
	int		i;
	int		n = cols * rows;

	if ( !tr.scratchImage ) {
		VK_Guard_Hit( VK_GSITE_ALLOC, "scratch image missing", VK_ERROR_INITIALIZATION_FAILED );
		return;
	}

	rgba = ( byte * )ri.Malloc( n * 4 );
	for ( i = 0; i < n; i++ ) {
		rgba[i * 4 + 0] = rgb[i * 3 + 0];
		rgba[i * 4 + 1] = rgb[i * 3 + 1];
		rgba[i * 4 + 2] = rgb[i * 3 + 2];
		rgba[i * 4 + 3] = 255;
	}

	R_UploadScratchRGBA( cols, rows, rgba );

	ri.Free( rgba );
}

static void RB_StretchRaw2D( int x, int y, int w, int h, int cols, int rows )
{
	int	screenW = vkConfig.vidWidth;
	int	screenH = vkConfig.vidHeight;

	if ( x > 0 || y > 0 || x + w < screenW || y + h < screenH ) {
		RB_ImmBeginWithImage( VK_IMM_TRIANGLES, tr.whiteImage,
							  VKS_DEPTHTEST_DISABLE | VKS_SRCBLEND_SRC_ALPHA
							  | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
		/* The reference renderer multiplies a black u_BaseColor in: the bar is black. */
		RB_ImmColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
		RB_ImmTexCoord2f( 0.0f, 0.0f );	RB_ImmVertex2f( 0.0f, 0.0f );
		RB_ImmTexCoord2f( 0.0f, 0.0f );	RB_ImmVertex2f( (float)screenW, 0.0f );
		RB_ImmTexCoord2f( 0.0f, 0.0f );	RB_ImmVertex2f( (float)screenW, (float)screenH );
		RB_ImmTexCoord2f( 0.0f, 0.0f );	RB_ImmVertex2f( 0.0f, 0.0f );
		RB_ImmTexCoord2f( 0.0f, 0.0f );	RB_ImmVertex2f( (float)screenW, (float)screenH );
		RB_ImmTexCoord2f( 0.0f, 0.0f );	RB_ImmVertex2f( 0.0f, (float)screenH );
		RB_ImmEnd();
	}

	RB_ImmBeginWithImage( VK_IMM_TRIANGLES, tr.scratchImage,
						  VKS_DEPTHTEST_DISABLE | VKS_SRCBLEND_SRC_ALPHA
						  | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	RB_ImmColor4f( tr.identityLight, tr.identityLight, tr.identityLight, 1.0f );

	RB_ImmTexCoord2f( 0.5f / cols,               0.5f / rows );	RB_ImmVertex2f( (float)x,     (float)y );
	RB_ImmTexCoord2f( ( cols - 0.5f ) / cols,    0.5f / rows );	RB_ImmVertex2f( (float)x + w, (float)y );
	RB_ImmTexCoord2f( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );	RB_ImmVertex2f( (float)x + w, (float)y + h );
	RB_ImmTexCoord2f( 0.5f / cols,               0.5f / rows );	RB_ImmVertex2f( (float)x,     (float)y );
	RB_ImmTexCoord2f( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );	RB_ImmVertex2f( (float)x + w, (float)y + h );
	RB_ImmTexCoord2f( 0.5f / cols,            ( rows - 0.5f ) / rows );	RB_ImmVertex2f( (float)x,     (float)y + h );

	RB_ImmEnd();
}

void RB_StretchRawDraw( const stretchRaw2DCommand_t *cmd )
{
	RB_2D_Flush();
	RB_StretchRaw2D( cmd->x, cmd->y, cmd->w, cmd->h, cmd->cols, cmd->rows );
}

void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, int components, const byte *data,
					qboolean dirty )
{
	stretchRaw2DCommand_t *cmd;
	int i, j;

	if ( !tr.registered ) {
		return;
	}

	for ( i = 0; ( 1 << i ) < cols; i++ ) {
	}
	for ( j = 0; ( 1 << j ) < rows; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows ) {
		ri.Error( ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows );
	}

	/* The decoder's own buffer: the upload is a front-end op, the walk parked. */
	R_SyncRenderThread();
	R_FlushRenderCommands();

	R_UploadScratchRGBA( cols, rows, data );

	/* The reference renderer's RB_SetGL2D sits exactly here: a movie frame runs
	   in whatever window the last caller left, and its scissor clips the quad. */
	Set2DWindow( 0, 0, vkConfig.vidWidth, vkConfig.vidHeight,
				 0.0f, (float)vkConfig.vidWidth, (float)vkConfig.vidHeight, 0.0f,
				 0.0f, 1.0f );

	cmd = ( stretchRaw2DCommand_t * )R_GetCommandBuffer( sizeof( *cmd ) );
	if ( cmd ) {
		cmd->commandId = RC_2D_STRETCH_RAW;
		cmd->x = x; cmd->y = y; cmd->w = w; cmd->h = h;
		cmd->cols = cols; cmd->rows = rows;
	}
}

void RE_UploadCinematic( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty )
{
	(void)w;
	(void)h;
	(void)client;

	if ( !tr.registered ) {
		return;
	}

	R_SyncRenderThread();
	R_FlushRenderCommands();

	R_UploadScratchRGBA( cols, rows, data );
}

void AddBox(float x, float y, float w, float h) {
	if (!r_draw2d->integer) {
		return;
	}

	D2_Prepare();

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
	if (!r_draw2d->integer) {
		return;
	}

	D2_Prepare();

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

	t0 = D2_Prepare();

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

	D2_End( D2P_lineloop, t0 );
}

/* The widget's placement inside the window the pass sets: retail carries it in
   the vertices (glVertex2f in screen space), so this records an offset the walk's
   2D vertex writers add, and the ortho stays the one constant screen ortho. */
void Set2DOrigin(float x, float y) {
	origin2DCommand_t *cmd = ( origin2DCommand_t * )R_GetCommandBuffer( sizeof( *cmd ) );

	if ( cmd ) {
		cmd->commandId = RC_2D_ORIGIN;
		cmd->x = x;
		cmd->y = y;
	}
}

void Set2DWindow(int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f) {
	int		t0;

	t0 = D2_Prepare();

	if ( r_window2DValid && r_scissor2DValid &&
		 r_window2D[0] == x && r_window2D[1] == y && r_window2D[2] == w && r_window2D[3] == h &&
		 r_window2DOrtho[0] == left && r_window2DOrtho[1] == right && r_window2DOrtho[2] == bottom &&
		 r_window2DOrtho[3] == top && r_window2DOrtho[4] == n && r_window2DOrtho[5] == f &&
		 r_scissor2D[0] == x && r_scissor2D[1] == y && r_scissor2D[2] == w && r_scissor2D[3] == h ) {
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

	D2_End( D2P_setwindow, t0 );
}

void RE_Scissor(int x, int y, int width, int height) {
	D2_T0();

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
	{
		shaderTime2DCommand_t *cmd = R_GetCommandBuffer( sizeof( *cmd ) );

		if ( cmd ) {
			cmd->commandId = RC_2D_SHADER_TIME;
			cmd->startTime = startTime;
		}
	}
}
