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
/*
 * tr_backend.c -- the render-thread half of the frame.
 *
 * The front end records a frame's surfaces into `tess` exactly as the reference
 * twin's front end does; this module's back end walks them onto the frame's
 * command buffer.  One surface is one stage pass: the state tuple becomes a
 * pipeline, the bundle a descriptor bind, the tess arrays a slice of the
 * vertex ring, the indexes an indexed draw.  Nothing here touches a driver
 * object -- the reference renderer's per-state calls all become fields of the
 * pipeline and the bind that already carry them.
 *
 * The 3D stage iterators (sky, lightmapped multitexture, vertex-lit, the
 * legacy fog pass) are the next milestone's surface; the 2D/HUD iterator below
 * is the walk the menu paints through.
 */

#include "tr_local.h"
#include "tr_program.h"
#include "tr_vbo.h"

/* qa_probe is the master instrument switch (qcommon/qa_probe.h): every census
   family below answers to it, so a default build emits none of this. */
#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

#include <string.h>
#include <math.h>

/* defined by the mirrored 3D front end */
backEndState_t		backEnd;
int					r_sequencenumber = 1;
glconfig_t			vkConfig;

/* The slot the walk records into, -1 when none: the frame layer writes it. */
int					vk_back_end_slot = -1;
static int			s_rbSlot = -1;

void RB_InitBackend( void )
{
	memset( &tess, 0, sizeof( tess ) );
	memset( &backEnd, 0, sizeof( backEnd ) );

	backEnd.color2D[0] = 255;
	backEnd.color2D[1] = 255;
	backEnd.color2D[2] = 255;
	backEnd.color2D[3] = 255;
}

/* the tessellation store lives in tr_shade.c, as in the reference renderer */


/* ---- the 2D coverage census ---------------------------------------------- *
 *
 * WHY.  The main menu's six LCARS background tiles are each a one-quad batch
 * with its own shader and its own clip, and the frame's transcript says all six
 * were issued; the operator's screen shows one of them.  "Issued" and
 * "rasterised" are different claims, and the gap between them is this census.
 *
 * The scissor test is the mechanism that produces the gap: a sample outside the
 * scissor rectangle "will have [its] coverage set to 0" (vkspec.txt:202812-202816,
 * section 35.2 "Scissor Test"), and the rect Vulkan is given is the clamped one
 * VK_Draw_SetScissor handed it (offset >= 0, VUID-vkCmdSetScissor-x-00595,
 * vkspec.txt:202864).  The rect a draw runs under is dynamic state that a batch
 * replays per span, so it is readable only here, beside the draw -- which is
 * where this census reads it, rather than recomputing it.
 *
 * For every span the frame draws it names the pipeline bound, the scissor rect
 * ACTUALLY in force (read back from the clamp), the ortho the push constant
 * carried, the ring slice and descriptor set the draw read, the blend/depth/
 * alpha-test tuple, the index range, the texture sampled, the quad's own screen
 * rect, and the pixel area of
 *     quad rect  INTERSECT  applied scissor  INTERSECT  render area.
 * px=0 is proof that the draw cannot rasterise a pixel.  Each row also carries
 * the three rects that say where the state came from: `authoredscn` is the span
 * table's copy (the state the quad's batch recorded it under), `appliedscn` is
 * what VK_Draw_2DStateApply re-issued for this draw, and `walkscn` is the walk's
 * live 2D mirror at the flush -- which under the defect that hid the LCARS tiles
 * held the PREVIOUS widget's clip.  The frame summary then names every shader
 * with its issued/covered counts, which is the whole-frame answer.
 * vk_2dCover 2 adds a readback cross-check over the presented frame's own host
 * copy: a span whose intersection is non-empty but whose readback holds only the
 * clear colour is the OTHER failure class (a rejected fragment, a blend that
 * resolves to nothing, a sampler that never landed), which the scissor cannot
 * explain.
 *
 * Gate: qa_probe AND vk_2dCover, default 0.  A default build emits none of it.
 * ------------------------------------------------------------------------- */

/* Declared here rather than in tr_local.h: the accessors live in vk_draw.c and
   tr_backend.c is their only consumer. */
void	 VK_Draw_LastScissorRect( int out[4] );
qboolean VK_Draw_LastScissorValid( void );
qboolean VK_Draw_2DAppliedGet( int *win, int *scn );
int		 VK_Draw_LastSetIndex( void );
int		 VK_Draw_LastAlphaTest( void );
qboolean VK_ModelPixelAtSwap( int x, int y, byte out[4] );

#define RB_2DCOV_SPAN_MAX	1024

typedef struct {
	char		shader[64];
	char		image[64];
	int			win[4], scn[4], clip[4];
	int			quad[4];
	long long	px;
	int			first, count;
	int			vertices;
	unsigned	stateBits;
	/* The colour question: the per-vertex colour the span actually carries,
	   the stage's gen resolution, and the art the draw samples.  "Black" is a
	   claim about these, so the census has to carry them, not just px. */
	byte		vcol[4];
	int			nvcol;
	int			rgbGen, alphaGen;
	byte		constColor[4];
	char		imgDesc[48];
} rb2dCov_t;

static rb2dCov_t	s_2dCov[RB_2DCOV_SPAN_MAX];
static int			s_2dCovCount;
static int			s_2dCovFrame = -1;
static cvar_t	   *s_2dCovCvar;
static byte			s_2dCovReadPx[4];

static qboolean RB_2DCoverOn( void )
{
	if ( !PROBE_On() ) {
		return qfalse;
	}
	if ( !s_2dCovCvar ) {
		s_2dCovCvar = ri.Cvar_Get( "vk_2dCover", "0", 0 );
	}
	return ( s_2dCovCvar && s_2dCovCvar->integer ) ? qtrue : qfalse;
}

static int RB_2DCoverLevel( void )
{
	if ( !RB_2DCoverOn() ) {
		return 0;
	}
	return s_2dCovCvar ? s_2dCovCvar->integer : 1;
}

/* The readback has to be the frame the verdict names, so it is taken every frame
   this frame left spans behind, and the copy carries the frame it belongs to. */
qboolean RB_2DCover_ReadbackArmed( void )
{
	return ( RB_2DCoverLevel() >= 2 && s_2dCovCount > 0 ) ? qtrue : qfalse;
}

int RB_2DCover_ReadbackFrame( void )
{
	return RB_FrameCount();
}

/* The quad's own screen rect over the vertex range the span's indexes name: a
   2D vertex already carries its placement (RB_2DPlaceVertex, tr_util.cpp) and
   the port's 2D space is Vulkan's, so no transform is needed. */
static qboolean RB_2DCoverQuadRect( int first, int count, int out[4] )
{
	float	x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
	int		i, n = 0;

	for ( i = 0; i < count; i++ ) {
		int	idx = (int)tess.indexes[first + i];

		if ( idx < 0 || idx >= tess.numVertexes ) {
			continue;
		}
		if ( !n ) {
			x0 = x1 = tess.xyz[idx][0];
			y0 = y1 = tess.xyz[idx][1];
		} else {
			if ( tess.xyz[idx][0] < x0 ) x0 = tess.xyz[idx][0];
			if ( tess.xyz[idx][0] > x1 ) x1 = tess.xyz[idx][0];
			if ( tess.xyz[idx][1] < y0 ) y0 = tess.xyz[idx][1];
			if ( tess.xyz[idx][1] > y1 ) y1 = tess.xyz[idx][1];
		}
		n++;
	}

	memset( out, 0, 4 * sizeof( int ) );
	if ( !n ) {
		return qfalse;
	}

	out[0] = (int)floor( x0 );
	out[1] = (int)floor( y0 );
	out[2] = (int)ceil( x1 ) + 1;
	out[3] = (int)ceil( y1 ) + 1;
	return qtrue;
}

/* (quad INTERSECT scissor INTERSECT render area), in pixels. */
static long long RB_2DCoverArea( const int quad[4], const int scn[4], int out[4] )
{
	int	x0 = quad[0] > scn[0] ? quad[0] : scn[0];
	int	y0 = quad[1] > scn[1] ? quad[1] : scn[1];
	int	x1 = quad[2] < scn[0] + scn[2] ? quad[2] : scn[0] + scn[2];
	int	y1 = quad[3] < scn[1] + scn[3] ? quad[3] : scn[1] + scn[3];

	if ( x1 < x0 ) x1 = x0;
	if ( y1 < y0 ) y1 = y0;

	if ( out ) {
		out[0] = x0; out[1] = y0;
		out[2] = x1 - x0; out[3] = y1 - y0;
	}
	if ( x1 <= x0 || y1 <= y0 ) {
		return 0;
	}
	return (long long)( x1 - x0 ) * (long long)( y1 - y0 );
}

/* The readback measures the element's own footprint, not its widget's clip: a
   text run's clip is mostly background, so a verdict keyed on it said nothing
   about the run.  Content is any sample that is not the clear colour; the run's
   own ink is counted at two fifths of its brightness, so a wrong colour names. */
#define RB_2DCOV_INK_FLOOR_NUM	2
#define RB_2DCOV_INK_FLOOR_DEN	5

static void RB_2DCoverReadback( rb2dCov_t *c, unsigned *distinct, unsigned *nbg,
								unsigned *nink, unsigned *ndark, unsigned *centre,
								unsigned mean[3], unsigned *samples, qboolean *ambiguous )
{
	static byte	seen[24][4];
	const float	*clear = vk.clearColor;
	byte		clr[4], ink[4];
	int			rect[4];
	unsigned	sum[3] = { 0, 0, 0 };
	unsigned	ns = 0, nb = 0, ni = 0, nd = 0, tot = 0;
	unsigned	nx, ny;
	int			sx, sy;

	*distinct = 0;
	*nbg      = 0;
	*nink     = 0;
	*ndark    = 0;
	*centre   = 0;
	*samples  = 0;
	mean[0] = mean[1] = mean[2] = 0;

	/* the element's own footprint: its geometry, clipped by the rect in force */
	if ( c->quad[2] > c->quad[0] && c->quad[3] > c->quad[1] ) {
		rect[0] = c->quad[0] > c->clip[0] ? c->quad[0] : c->clip[0];
		rect[1] = c->quad[1] > c->clip[1] ? c->quad[1] : c->clip[1];
		rect[2] = ( c->quad[2] < c->clip[0] + c->clip[2] ? c->quad[2] : c->clip[0] + c->clip[2] ) - rect[0];
		rect[3] = ( c->quad[3] < c->clip[1] + c->clip[3] ? c->quad[3] : c->clip[1] + c->clip[3] ) - rect[1];
	} else {
		memcpy( rect, c->clip, sizeof( rect ) );
	}
	if ( rect[2] <= 0 || rect[3] <= 0 ) {
		return;
	}

	clr[0] = (byte)( clear[0] * 255.0f );
	clr[1] = (byte)( clear[1] * 255.0f );
	clr[2] = (byte)( clear[2] * 255.0f );
	clr[3] = 255;

	memcpy( ink, c->vcol, sizeof( ink ) );

	/* A run whose colour the clear colour already satisfies is a run this
	   readback can never convict: the clear background is a valid answer. */
	*ambiguous = qtrue;
	for ( sx = 0; sx < 3; sx++ ) {
		if ( clr[sx] * RB_2DCOV_INK_FLOOR_DEN < ink[sx] * RB_2DCOV_INK_FLOOR_NUM ) {
			*ambiguous = qfalse;
			break;
		}
	}

	/* up to 16x8 samples, dense on a small rect: a 20px label box is read
	   pixel for pixel, a full-screen wash is read on a grid */
	nx = rect[2] < 16 ? rect[2] : 16;
	ny = rect[3] < 8 ? rect[3] : 8;
	for ( sy = 0; sy < ny; sy++ ) {
		int	y = rect[1] + ( rect[3] * ( 2 * sy + 1 ) ) / ( 2 * ny );

		for ( sx = 0; sx < nx; sx++ ) {
			int		x = rect[0] + ( rect[2] * ( 2 * sx + 1 ) ) / ( 2 * nx );
			byte	px[4];
			unsigned i;
			qboolean	isInk, isClr;

			if ( !VK_ModelPixelAtSwap( x, y, px ) ) {
				return;				/* no host copy for this frame */
			}
			tot++;

			isClr = ( px[0] == clr[0] && px[1] == clr[1] && px[2] == clr[2] );
			isInk = !ambiguous;
			for ( i = 0; isInk && i < 3; i++ ) {
				if ( px[i] * RB_2DCOV_INK_FLOOR_DEN < ink[i] * RB_2DCOV_INK_FLOOR_NUM ) {
					isInk = qfalse;
				}
			}
			if ( isInk ) {
				ni++;
			}
			if ( isClr ) {
				nb++;
			} else {
				sum[0] += px[0]; sum[1] += px[1]; sum[2] += px[2];
				/* opaque and colourless is a finding about what the footprint
				   holds, not about which draw put it there */
				if ( px[0] < 8 && px[1] < 8 && px[2] < 8 && px[3] > 200 ) {
					nd++;
				}
			}
			for ( i = 0; i < ns; i++ ) {
				if ( seen[i][0] == px[0] && seen[i][1] == px[1]
					 && seen[i][2] == px[2] && seen[i][3] == px[3] ) {
					break;
				}
			}
			if ( i == ns && ns < 24 ) {
				memcpy( seen[ns], px, 4 );
				ns++;
			}
		}
	}

	*distinct = ns;
	*nbg      = nb;
	*nink     = ni;
	*ndark    = nd;
	*samples  = tot;
	mean[0] = ( tot - nb ) ? sum[0] / ( tot - nb ) : 0;
	mean[1] = ( tot - nb ) ? sum[1] / ( tot - nb ) : 0;
	mean[2] = ( tot - nb ) ? sum[2] / ( tot - nb ) : 0;

	if ( VK_ModelPixelAtSwap( rect[0] + rect[2] / 2, rect[1] + rect[3] / 2, s_2dCovReadPx ) ) {
		*centre = ( (unsigned)s_2dCovReadPx[0] << 24 ) | ( (unsigned)s_2dCovReadPx[1] << 16 )
				  | ( (unsigned)s_2dCovReadPx[2] << 8 ) | (unsigned)s_2dCovReadPx[3];
	}
}

/* The colour the span actually carries.  A 2D batch packs one colour per
   vertex, so the span's own vertex range is the answer, and the count of
   distinct values says whether the run is uniform.  Offsets are taken over the
   span's indexes so a multi-quad span reports its whole range. */
static void RB_2DCoverVertexColour( int first, int count, byte out[4], int *distinct )
{
	int		i, n = 0, nd = 0;
	byte	seen[8][4];

	for ( i = 0; i < 4; i++ ) {
		out[i] = 0;
	}
	*distinct = 0;

	for ( i = 0; i < count; i++ ) {
		int	idx = (int)tess.indexes[first + i];
		int	j;

		if ( idx < 0 || idx >= tess.numVertexes ) {
			continue;
		}
		if ( !n ) {
			memcpy( out, tess.vertexColors[idx], 4 );
		}
		n++;
		for ( j = 0; j < nd; j++ ) {
			if ( seen[j][0] == tess.vertexColors[idx][0] && seen[j][1] == tess.vertexColors[idx][1]
				 && seen[j][2] == tess.vertexColors[idx][2] && seen[j][3] == tess.vertexColors[idx][3] ) {
				break;
			}
		}
		if ( j == nd && nd < 8 ) {
			memcpy( seen[nd], tess.vertexColors[idx], 4 );
			nd++;
		}
	}

	*distinct = nd;
}

/* One row per span: the state the draw ran under and the pixels it could reach. */
static void RB_2DCoverSpan( int level, const float *ortho, const int *win, const int *scn,
							int first, int count, int indexBase, int vertexBase,
							VkPipeline pipeline, const shaderStage_t *stage, const image_t *image )
{
	int			clip[4], quad[4], appliedScn[4], appliedWin[4];
	int			area[4];
	long long	px;
	qboolean	haveQuad;
	rb2dCov_t  *c = NULL;
	const char *shaderName = tess.shader ? tess.shader->name : "?";
	const char *imageName  = image ? image->imgName : "(none)";

	/* the rect actually in force: read back from the clamp, not recomputed */
	if ( VK_Draw_LastScissorValid() ) {
		VK_Draw_LastScissorRect( clip );
	} else {
		memset( clip, 0, sizeof( clip ) );
	}

	if ( !VK_Draw_2DAppliedGet( appliedWin, appliedScn ) ) {
		memcpy( appliedScn, scn, sizeof( appliedScn ) );
	}

	haveQuad = RB_2DCoverQuadRect( first, count, quad );
	px       = RB_2DCoverArea( quad, clip, area );

	if ( s_2dCovCount < RB_2DCOV_SPAN_MAX ) {
		c = &s_2dCov[s_2dCovCount++];
		memset( c, 0, sizeof( *c ) );
		Com_sprintf( c->shader, sizeof( c->shader ), "%s", shaderName );
		Com_sprintf( c->image, sizeof( c->image ), "%s", imageName );
		memcpy( c->win, win, sizeof( c->win ) );
		memcpy( c->scn, appliedScn, sizeof( c->scn ) );
		memcpy( c->clip, clip, sizeof( c->clip ) );
		memcpy( c->quad, quad, sizeof( c->quad ) );
		c->px        = px;
		c->first     = first;
		c->count     = count;
		c->vertices  = tess.numVertexes;
		c->stateBits = stage ? stage->stateBits : 0u;

		RB_2DCoverVertexColour( first, count, c->vcol, &c->nvcol );
		c->rgbGen   = stage ? (int)stage->rgbGen : 0;
		c->alphaGen = stage ? (int)stage->alphaGen : 0;
		if ( stage ) {
			c->constColor[0] = stage->colorConst[0];
			c->constColor[1] = stage->colorConst[1];
			c->constColor[2] = stage->colorConst[2];
			c->constColor[3] = stage->colorConst[3];
		}
		/* what the unit actually samples: the art's own size, format and levels,
		   so a quad bound to a placeholder or an alias is nameable here */
		if ( image ) {
			Com_sprintf( c->imgDesc, sizeof( c->imgDesc ), "%dx%d/0x%x/%dm",
						 image->width, image->height, (unsigned)image->internalFormat,
						 image->numMipmaps );
		} else {
			Com_sprintf( c->imgDesc, sizeof( c->imgDesc ), "(no image)" );
		}
	}

	if ( level >= 1 ) {
		VK_Guard_Row( "VK:2dcov",
					  "f=%d span first=%d count=%d shader='%s' img='%s' verts=%d "
					  "pipe=0x%llx set=%d slice=v%d/i%d "
					  "win=(%d,%d,%d,%d) authoredscn=(%d,%d,%d,%d) appliedscn=(%d,%d,%d,%d) "
					  "walkscn=(%d,%d,%d,%d) scissor=(%d,%d %dx%d) quad=(%d,%d)-(%d,%d) clip=(%d,%d %dx%d) "
					  "px=%lld ort=(%.4f,%.4f,%.4f,%.4f) sb=0x%x atest=%d blend=%d/%d depth=%d/%d "
					  "vcol=%d,%d,%d,%d nvcol=%d rgbGen=%d const=%d,%d,%d,%d alphaGen=%d imgdesc=%s "
					  "state=%s%s",
					  RB_FrameCount(), first, count, shaderName, imageName, tess.numVertexes,
					  (unsigned long long)(uintptr_t)pipeline, VK_Draw_LastSetIndex(),
					  vertexBase, indexBase,
					  win[0], win[1], win[2], win[3],
					  scn[0], scn[1], scn[2], scn[3],
					  appliedScn[0], appliedScn[1], appliedScn[2], appliedScn[3],
					  vk_2dScissor[0], vk_2dScissor[1], vk_2dScissor[2], vk_2dScissor[3],
					  clip[0], clip[1], clip[2], clip[3],
					  quad[0], quad[1], quad[2], quad[3],
					  area[0], area[1], area[2], area[3], px,
					  ortho[0], ortho[5], ortho[12], ortho[13],
					  stage ? stage->stateBits : 0u,
					  stage ? (int)( ( stage->stateBits & VKS_ATEST_BITS ) >> 28 ) : 0,
					  stage ? (int)( stage->stateBits & VKS_SRCBLEND_BITS ) : 0,
					  stage ? (int)( ( stage->stateBits & VKS_DSTBLEND_BITS ) >> 4 ) : 0,
					  stage && ( stage->stateBits & VKS_DEPTHTEST_DISABLE ) ? 0 : 1,
					  stage && ( stage->stateBits & VKS_DEPTHMASK_TRUE ) ? 1 : 0,
					  c ? c->vcol[0] : 0, c ? c->vcol[1] : 0, c ? c->vcol[2] : 0, c ? c->vcol[3] : 0,
					  c ? c->nvcol : 0,
					  stage ? (int)stage->rgbGen : 0,
					  stage ? stage->colorConst[0] : 0, stage ? stage->colorConst[1] : 0,
					  stage ? stage->colorConst[2] : 0, stage ? stage->colorConst[3] : 0,
					  stage ? (int)stage->alphaGen : 0,
					  c && c->imgDesc[0] ? c->imgDesc : "(none)",
					  px > 0 ? "RASTER" : "CLIP0",
					  haveQuad ? "" : " QUAD=EMPTY" );
	}
}

/* The frame's whole answer, emitted where the next frame opens: every span of
   the frame that ended is listed by shader with the pixels it could reach, so a
   shader that was issued and covered nothing is one line. */
static void RB_2DCoverFrame( void )
{
	int	level = RB_2DCoverLevel();

	if ( s_2dCovCount > 0 ) {
		int	i, j, raster = 0, clip0 = 0;

		for ( i = 0; i < s_2dCovCount; i++ ) {
			if ( s_2dCov[i].px > 0 ) {
				raster++;
			} else {
				clip0++;
			}
		}

		VK_Guard_Row( "VK:2dcov",
					  "frame=%d draws=%d raster=%d clip0=%d level=%d",
					  s_2dCovFrame, s_2dCovCount, raster, clip0, level );

		for ( i = 0; i < s_2dCovCount; i++ ) {
			int			issued = 0, covered = 0;
			long long	px = 0;

			if ( !s_2dCov[i].shader[0] ) {
				continue;
			}
			for ( j = 0; j < i; j++ ) {
				if ( s_2dCov[j].shader[0]
					 && strcmp( s_2dCov[j].shader, s_2dCov[i].shader ) == 0 ) {
					break;
				}
			}
			if ( j < i ) {
				continue;			/* already reported at its first span */
			}
			for ( j = i; j < s_2dCovCount; j++ ) {
				if ( strcmp( s_2dCov[j].shader, s_2dCov[i].shader ) == 0 ) {
					issued++;
					px += s_2dCov[j].px;
					if ( s_2dCov[j].px > 0 ) {
						covered++;
					}
				}
			}
			VK_Guard_Row( "VK:2dcov",
						  "frame=%d shader='%s' img='%s' issued=%d covered=%d px=%lld %s",
						  s_2dCovFrame, s_2dCov[i].shader, s_2dCov[i].image,
						  issued, covered, px,
						  covered ? "" : "VERDICT=ISSUED_NOT_RASTERISED" );
		}

		/* The readback cross-check: level 2, over the presented frame's own copy. */
		if ( level >= 2 ) {
			for ( i = 0; i < s_2dCovCount; i++ ) {
				unsigned distinct, nbg, nink, ndark, centre, mean[3], samples;
				qboolean	ambiguous;
				const char *verdict = "", *inkNote = "";

				RB_2DCoverReadback( &s_2dCov[i], &distinct, &nbg, &nink, &ndark,
									&centre, mean, &samples, &ambiguous );

				/* Content is any sample that is not the clear colour; the run's
				   own ink is reported so a wrong colour names.  Where the clear
				   colour would also satisfy the ink test, no verdict. */
				if ( s_2dCov[i].px > 0 ) {
					if ( samples > nbg ) {
						verdict = "";
					} else if ( !ambiguous ) {
						verdict = "VERDICT=NO_INK";
					} else {
						inkNote = "INK=AMBIGUOUS ";
					}
				}

				VK_Guard_Row( "VK:2dcov",
							  "frame=%d src=%d rb shader='%s' clip=(%d,%d %dx%d) quad=(%d,%d %dx%d) "
							  "samples=%u distinct=%u bg=%u ink=%u dark=%u "
							  "centre=0x%08x mean=%u,%u,%u vcol=%d,%d,%d,%d rgbGen=%d "
							  "alphaGen=%d imgdesc=%s %s%s",
							  s_2dCovFrame, VK_ModelSwapFrame(), s_2dCov[i].shader,
							  s_2dCov[i].clip[0], s_2dCov[i].clip[1],
							  s_2dCov[i].clip[2], s_2dCov[i].clip[3],
							  s_2dCov[i].quad[0], s_2dCov[i].quad[1],
							  s_2dCov[i].quad[2] - s_2dCov[i].quad[0],
							  s_2dCov[i].quad[3] - s_2dCov[i].quad[1],
							  samples, distinct, nbg, nink, ndark,
							  centre, mean[0], mean[1], mean[2],
							  s_2dCov[i].vcol[0], s_2dCov[i].vcol[1],
							  s_2dCov[i].vcol[2], s_2dCov[i].vcol[3],
							  s_2dCov[i].rgbGen, s_2dCov[i].alphaGen,
							  s_2dCov[i].imgDesc[0] ? s_2dCov[i].imgDesc : "(none)",
							  inkNote, verdict );
			}
		}
	}

	s_2dCovCount = 0;
	s_2dCovFrame = RB_FrameCount();
}

/* A 2D batch that crossed a window or scissor change draws once per span, each
   draw preceded by the state its quads were authored under.  Under two spans
   this is the single draw the batch always made: nothing about a run that sets
   one window changes.  The vertices are packed once for the whole batch, so a
   span only moves the first index and re-pushes the ortho and the two rects. */
static void RB_StageDrawSpan( VkCommandBuffer cmd, int indexBase, int vertexBase, int numIndexes,
							  VkPipeline pipeline, const shaderStage_t *stage, const image_t *image )
{
	int	spans = RB_2D_SpanCount();
	int	level = backEnd.in2D ? RB_2DCoverLevel() : 0;
	int	i;

	/* The draw reads vertexBase[0..numVertexes); if the ring there is not the
	   tess the row predicted, the quads land wherever the stale slice says. */
	if ( backEnd.in2D && PROBE_On() && tess.numVertexes > 0 ) {
		const vkTessVertex_t *v0 = ( const vkTessVertex_t * )VK_VertexRing_Mapped() + vertexBase;
		const vkTessVertex_t *vn = v0 + tess.numVertexes - 1;
		const float			*x0 = tess.xyz[0];
		const float			*xn = tess.xyz[tess.numVertexes - 1];

		if ( v0->xyz[0] != x0[0] || v0->xyz[1] != x0[1]
			 || vn->xyz[0] != xn[0] || vn->xyz[1] != xn[1] ) {
			static int s_rows;

			s_rows++;
			if ( s_rows <= 16 || ( s_rows % 256 ) == 0 ) {
				VK_Guard_Row( "VK:2dring",
							  "f=%d shader='%s' vbase=%d verts=%d "
							  "ring0=(%.1f,%.1f) tess0=(%.1f,%.1f) "
							  "ringN=(%.1f,%.1f) tessN=(%.1f,%.1f)",
							  RB_FrameCount(), tess.shader ? tess.shader->name : "?",
							  vertexBase, tess.numVertexes,
							  (double)v0->xyz[0], (double)v0->xyz[1],
							  (double)x0[0], (double)x0[1],
							  (double)vn->xyz[0], (double)vn->xyz[1],
							  (double)xn[0], (double)xn[1] );
			}
		}
	}

	if ( spans < 2 ) {
		/* One draw for the whole batch, under whatever state the command buffer
		   holds: for a 2D batch that is the arm which cannot say what its quads
		   were authored under (tr_draw.c RB_2D_SpanCount reports a single span as
		   two, so a one-state batch never lands here). */
		if ( level ) {
			int	clip[4], quad[4], area[4];
			long long px;

			if ( VK_Draw_LastScissorValid() ) {
				VK_Draw_LastScissorRect( clip );
			} else {
				memset( clip, 0, sizeof( clip ) );
			}
			/* No span state to apply: the draw runs under the command buffer's
			   scissor, which is the last rect VK_Draw_ScissorRect was given, and
			   that is what the coverage is measured against.  A one-shot writer
			   (the glyph run, the triangle fan) is drawn where it is written, so
			   this IS its authored state; a batch lands here only when its run
			   could not be named. */
			RB_2DCoverQuadRect( 0, numIndexes, quad );
			px = RB_2DCoverArea( quad, clip, area );

			{
			byte	vcol[4];
			char	imgDesc[48];
			int		nvcol = 0;

			imgDesc[0] = 0;
			if ( image ) {
				Com_sprintf( imgDesc, sizeof( imgDesc ), "%dx%d/0x%x/%dm",
							 image->width, image->height, (unsigned)image->internalFormat,
							 image->numMipmaps );
			}
			if ( backEnd.in2D ) {
				RB_2DCoverVertexColour( 0, numIndexes, vcol, &nvcol );
			}

			VK_Guard_Row( "VK:2dcov",
						  "f=%d span=WHOLE shader='%s' img='%s' spans=%d idx=[0,%d) verts=%d "
						  "slice=v%d/i%d scissor=(%d,%d %dx%d) quad=(%d,%d)-(%d,%d) clip=(%d,%d %dx%d) "
						  "px=%lld vcol=%d,%d,%d,%d nvcol=%d rgbGen=%d const=%d,%d,%d,%d "
						  "alphaGen=%d pubatest=%d imgdesc=%s state=CMDBUF%s",
						  RB_FrameCount(), tess.shader ? tess.shader->name : "?",
						  image ? image->imgName : "(none)", spans,
						  numIndexes, tess.numVertexes,
						  vertexBase, indexBase,
						  clip[0], clip[1], clip[2], clip[3],
						  quad[0], quad[1], quad[2], quad[3],
						  area[0], area[1], area[2], area[3], px,
						  vcol[0], vcol[1], vcol[2], vcol[3], nvcol,
						  stage ? (int)stage->rgbGen : 0,
						  stage ? stage->colorConst[0] : 0, stage ? stage->colorConst[1] : 0,
						  stage ? stage->colorConst[2] : 0, stage ? stage->colorConst[3] : 0,
						  stage ? (int)stage->alphaGen : 0,
						  VK_Draw_LastAlphaTest(),
						  imgDesc[0] ? imgDesc : "(none)",
						  backEnd.in2D ? "" : " (3D)" );

			if ( backEnd.in2D && s_2dCovCount < RB_2DCOV_SPAN_MAX ) {
				rb2dCov_t	*c = &s_2dCov[s_2dCovCount++];

				memset( c, 0, sizeof( *c ) );
				Com_sprintf( c->shader, sizeof( c->shader ), "%s",
							 tess.shader ? tess.shader->name : "?" );
				Com_sprintf( c->image, sizeof( c->image ), "%s",
							 image ? image->imgName : "(none)" );
				memcpy( c->clip, clip, sizeof( c->clip ) );
				memcpy( c->quad, quad, sizeof( c->quad ) );
				c->vertices = tess.numVertexes;
				c->count    = numIndexes;
				c->stateBits = stage ? stage->stateBits : 0u;
				c->px       = px;
				memcpy( c->vcol, vcol, sizeof( vcol ) );
				c->nvcol    = nvcol;
				c->rgbGen   = stage ? (int)stage->rgbGen : 0;
				c->alphaGen = stage ? (int)stage->alphaGen : 0;
				if ( stage ) {
					c->constColor[0] = stage->colorConst[0];
					c->constColor[1] = stage->colorConst[1];
					c->constColor[2] = stage->colorConst[2];
					c->constColor[3] = stage->colorConst[3];
				}
				if ( image ) {
					Com_sprintf( c->imgDesc, sizeof( c->imgDesc ), "%dx%d/0x%x/%dm",
								 image->width, image->height, (unsigned)image->internalFormat,
								 image->numMipmaps );
				} else {
					Com_sprintf( c->imgDesc, sizeof( c->imgDesc ), "(no image)" );
				}
			}
			}
		}
		VK_Draw_DrawIndexed( cmd, numIndexes, indexBase, vertexBase );
		VK_Shade_NoteDraw( qfalse );
		return;
	}

	for ( i = 0; i < spans; i++ ) {
		float	ortho[16];
		int		win[4], scn[4];
		int		first = 0, count = 0;

		RB_2D_SpanGet( i, &first, &count, ortho, win, scn );
		if ( count <= 0 ) {
			continue;
		}

		VK_Draw_2DStateApply( cmd, ortho, win, scn );

		if ( level ) {
			RB_2DCoverSpan( level, ortho, win, scn, first, count, indexBase, vertexBase,
							pipeline, stage, image );
		}

		VK_Draw_DrawIndexed( cmd, count, indexBase + first, vertexBase );
		VK_Shade_NoteDraw( qfalse );
		/* One entry per span: the order transcript keeps the granularity it had
		   when each window ended the surface. */
		VK_2DOrder_Note( tess.shader ? tess.shader->name : "?" );
	}
}


/* ---- the stage pass: one stage, one pipeline, one bind, one indexed draw;
 * the state tuple carries blend, depth, cull and alpha test. */

static qboolean RB_StagePass( shaderStage_t *stage, const color4ub_t *colors )
{
	vkPipelineKey_t key;
	VkPipeline pipeline;
	VkImageView view;
	VkSampler sampler;
	vkTessVertex_t *ringVerts;
	vkIndex_t *ringIndexes;
	image_t *image;
	int vertexBase = 0;
	int indexBase = 0;
	int i;
	qboolean clamp;
	qboolean mipmap;

	if ( !stage->active ) {
		return qtrue;
	}

	image = stage->bundle[0].image[0];
	if ( !image ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "stage without an image", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	view = VK_Texture_View( (int)image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "image without a Vulkan view", VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	/* The 2D binding row: the quad's shader, the view the slot resolves to, and
	   the rect the geometry carries.  A tile sampling the wrong art names itself
	   here as shader X with image Y.  Everything in the block exists for the row
	   and for the order transcript, which gates itself, so a quiet boot pays
	   neither. */
	if ( backEnd.in2D && PROBE_On() ) {
		static int s_rows;

		float x0 = tess.xyz[0][0], y0 = tess.xyz[0][1];
		float x1 = x0, y1 = y0;
		float gx0, gy0, gx1, gy1;
		int   v;

		for ( v = 1; v < tess.numVertexes; v++ ) {
			if ( tess.xyz[v][0] < x0 ) x0 = tess.xyz[v][0];
			if ( tess.xyz[v][0] > x1 ) x1 = tess.xyz[v][0];
			if ( tess.xyz[v][1] < y0 ) y0 = tess.xyz[v][1];
			if ( tess.xyz[v][1] > y1 ) y1 = tess.xyz[v][1];
		}

		/* The rect the quad actually lands in, in the SAME space the trace logs:
		   a vertex carries its widget's placement (RB_2DPlaceVertex), the window
		   maps its ortho onto its viewport, and retail's glVertex2f/glScissor are
		   bottom-left.  The ortho range is recovered from the pushed matrix
		   (m[0] = 2/(r-l), m[12] = -(r+l)/(r-l), m[5] = -2/(t-b),
		   m[13] = (t+b)/(t-b)), so the row is right for a pillarboxed or a
		   window-sized window too, and `srect` diffs call for call against the
		   trace's stream and against `win`/`scn` below. */
		{
			const float	*o = VK_Draw_CurrentOrtho();
			float		winX = (float)vk_2dWindow[0];
			float		winY = (float)vk_2dWindow[1];
			float		winW = (float)vk_2dWindow[2];
			float		winH = (float)vk_2dWindow[3];

			if ( o && o[0] != 0.0f && o[5] != 0.0f ) {
				float	wx = 2.0f / o[0];				/* right - left */
				float	lx = ( -o[12] * wx - wx ) * 0.5f;
				float	tb = -2.0f / o[5];				/* top - bottom */
				float	ty = tb * ( o[13] + 1.0f ) * 0.5f;

				gx0 = winX + ( x0 - lx ) / wx * winW;
				gx1 = winX + ( x1 - lx ) / wx * winW;
				gy0 = winY + winH + ( y1 - ty ) / tb * winH;
				gy1 = winY + winH + ( y0 - ty ) / tb * winH;
			} else {
				gx0 = winX + x0;
				gx1 = winX + x1;
				gy0 = winY + winH - y1;
				gy1 = winY + winH - y0;
			}
		}

		/* the order transcript: the rate-limited row above cannot answer a
		   z-order question, because after the fortieth draw the sampling
		   throws the sequence away.  A widget's depth is the position it draws
		   at, so the ordered names are the answer; this collects them and
		   VK_Shade_NewFrame emits the frame's list. */
		VK_2DOrder_Note( tess.shader ? tess.shader->name : "?" );

		s_rows++;
		if ( s_rows <= 40 || ( s_rows % 64 ) == 0 ) {
			VK_Guard_Row( "VK:2ddraw",
						  "n=%d shader='%s' img='%s' tex=%d verts=%d idx=%d "
						  "srect=(%.1f,%.1f)-(%.1f,%.1f) st=(%.3f,%.3f)-(%.3f,%.3f) "
						  "sb=0x%x col=%d,%d,%d,%d vert=%d,%d,%d,%d rgbGen=%d "
						  "const=%d,%d,%d,%d alphaGen=%d "
						  "win=(%d,%d,%d,%d) scn=(%d,%d,%d,%d) wrap=%d,%d clamp=%d",
						  s_rows,
						  tess.shader ? tess.shader->name : "?",
						  image->imgName,
						  (int)image->texnum,
						  tess.numVertexes, tess.numIndexes,
						  gx0, gy0, gx1, gy1,
						  tess.texCoords[0][0][0], tess.texCoords[0][0][1],
						  tess.texCoords[tess.numVertexes - 1][0][0],
						  tess.texCoords[tess.numVertexes - 1][0][1],
						  stage->stateBits,
						  backEnd.color2D[0], backEnd.color2D[1],
						  backEnd.color2D[2], backEnd.color2D[3],
						  tess.vertexColors[0][0], tess.vertexColors[0][1],
						  tess.vertexColors[0][2], tess.vertexColors[0][3],
						  (int)stage->rgbGen,
						  stage->colorConst[0], stage->colorConst[1],
						  stage->colorConst[2], stage->colorConst[3],
						  (int)stage->alphaGen,
						  vk_2dWindow[0], vk_2dWindow[1], vk_2dWindow[2], vk_2dWindow[3],
						  vk_2dScissor[0], vk_2dScissor[1], vk_2dScissor[2], vk_2dScissor[3],
						  (int)image->wrapClampModeX, (int)image->wrapClampModeY,
						  ( image->flags & IMGFLAG_CLAMPTOEDGE
							|| image->wrapClampModeX != VK_TWRAP_REPEAT
							|| image->wrapClampModeY != VK_TWRAP_REPEAT ) ? 1 : 0 );
		}
	}

	memset( &key, 0, sizeof( key ) );
	key.topology = VK_IMM_TRIANGLES;
	key.stateBits = stage->stateBits;
	/* The 2D window is two-sided, as the reference renderer's RB_2D_SetWindow is. */
	key.cull = ( backEnd.in2D || tess.shader->lightmapIndex == LIGHTMAP_2D )
			   ? CT_TWO_SIDED : tess.shader->cullType;
	/* The reference's RB_2D_SetWindow turns depth off for the whole UI pass,
	   whatever a UI shader's own state bits say. */
	key.depthTest = ( stage->stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	if ( backEnd.in2D && VK_DepthGate2D() ) {
		key.depthTest = qfalse;
	}
	key.depthWrite = ( stage->stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;

	/* A 2D batch that loses its two-sided or depth-off state culls or z-fails
	   every fragment and reads as text that never rasterised. */
	if ( backEnd.in2D && PROBE_On() && ( key.cull != CT_TWO_SIDED || key.depthTest ) ) {
		static int	s_rows;

		s_rows++;
		if ( s_rows <= 8 || ( s_rows % 256 ) == 0 ) {
			VK_Guard_Row( "VK:2dstate",
						  "cull=%d depth=%d in2d=%d shader='%s' pubatest=%d f=%d",
						  (int)key.cull, key.depthTest ? 1 : 0, backEnd.in2D,
						  tess.shader ? tess.shader->name : "?",
						  VK_Draw_LastAlphaTest(), RB_FrameCount() );
		}
	}

	pipeline = VK_Draw_Pipeline( &key );
	if ( pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	VK_Draw_BindPipeline( vk.commandBuffers[s_rbSlot], pipeline );
	VK_Draw_SetAlphaTest( vk.commandBuffers[s_rbSlot], stage->stateBits );

	/* wrapClampModeX/Y hold the legacy wrap enum: REPEAT is 10497, not 0, and the
	   loader sets them from `clampmap' -- the GLES3 twin's whole 2D pass samples
	   by exactly this mode.  Testing only IMGFLAG_CLAMPTOEDGE, a flag the font
	   atlas alone carries, left every 2D clampmap stage on REPEAT, so a
	   linear-sampled panel bled its opposite edge into the border and left the
	   seam between neighbours.  clampTime is not a wrap: it is a tcMod clock and
	   no shipped shader sets it. */
	clamp = ( image->flags & IMGFLAG_CLAMPTOEDGE )
			|| ( image->wrapClampModeX != VK_TWRAP_REPEAT )
			|| ( image->wrapClampModeY != VK_TWRAP_REPEAT ) ? qtrue : qfalse;
	mipmap = ( image->numMipmaps > 1 ) ? qtrue : qfalse;

	if ( !VK_Texture_Sampler( clamp, mipmap, qtrue, &sampler ) ) {
		return qfalse;
	}

	if ( !VK_Draw_BindTexture( vk.commandBuffers[s_rbSlot], view, sampler ) ) {
		return qfalse;
	}

	/* xyz, st and colour are the surface's own; which colour array that is
	   depends on the stage's gens -- RB_StageIterator2D resolved the ones the
	   tess pipeline cannot express into svars.colors and hands that array in,
	   the vertex-driven ones keep the quad's own vertexColors. */
	if ( stage && VK_VertexRing_StreamMatch( tess.numVertexes, tess.numIndexes,
											 tess.texCoords[0][0], tess.texCoords[0][0], 0,
											 colors, NULL,
											 &vertexBase, &indexBase ) ) {
		VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
		RB_StageDrawSpan( vk.commandBuffers[s_rbSlot], indexBase, vertexBase, tess.numIndexes,
						  pipeline, stage, image );
		return qtrue;
	}

	/* Both halves are reserved before a vertex is written: a surface whose index
	   half does not fit is skipped whole, and reserving the vertex half first
	   left its written vertices stranded in the ring on that path. */
	ringIndexes = VK_VertexRing_ReserveIndexes( tess.numIndexes, &indexBase );
	if ( !ringIndexes ) {
		return qfalse;
	}

	ringVerts = VK_VertexRing_Reserve( tess.numVertexes, &vertexBase );
	if ( !ringVerts ) {
		VK_VertexRing_RollBack( 0, tess.numIndexes );
		return qfalse;
	}

	for ( i = 0; i < tess.numVertexes; i++ ) {
		ringVerts[i].xyz[0] = tess.xyz[i][0];
		ringVerts[i].xyz[1] = tess.xyz[i][1];
		ringVerts[i].xyz[2] = tess.xyz[i][2];
		ringVerts[i].st[0] = tess.texCoords[i][0][0];
		ringVerts[i].st[1] = tess.texCoords[i][0][1];

		if ( colors == tess.vertexColors && !tess.vertexColorValid ) {
			ringVerts[i].color[0] = 254;
			ringVerts[i].color[1] = 254;
			ringVerts[i].color[2] = 254;
			ringVerts[i].color[3] = 254;
		} else {
			ringVerts[i].color[0] = colors[i][0];
			ringVerts[i].color[1] = colors[i][1];
			ringVerts[i].color[2] = colors[i][2];
			ringVerts[i].color[3] = colors[i][3];
		}
	}

	memcpy( ringIndexes, tess.indexes, (size_t)tess.numIndexes * sizeof( vkIndex_t ) );

	if ( stage ) {
		VK_VertexRing_StreamStore( tess.numVertexes, tess.numIndexes,
								   tess.texCoords[0][0], tess.texCoords[0][0], 0,
								   colors, NULL, vertexBase, indexBase );
	}

	/* The ring is bound per surface: a bind is cheaper than tracking which. */
	VK_Draw_BindRing( vk.commandBuffers[s_rbSlot] );
	RB_StageDrawSpan( vk.commandBuffers[s_rbSlot], indexBase, vertexBase, tess.numIndexes,
					  pipeline, stage, image );

	vk_ring_note( vertexBase + tess.numVertexes, indexBase + tess.numIndexes );

	return qtrue;
}

/* The generic iterator: one pass per active stage.  A 3D term is the 3D
   surface's work and is named by its guard. */



void rb_set_slot( int slot )
{
	s_rbSlot = slot;
}

/* The frame's back-end state is reset where the frame is walked, not where it
   is recorded.  RE_BeginFrame used to do this on the front end, which with
   r_smp 1 rewrote the ring, the shade census and the 2D window cache while the
   render thread was still walking the previous frame through them. */
void RB_FrameOpen( int slot )
{
	RB_2DCoverFrame();				/* the frame that ended, by shader */

	VK_VertexRing_SetFrame( slot );	/* the ring this frame fills */
	VK_VertexRing_ResetFrame();
	RB_VBO_BeginFrame();
	VK_Shade_NewFrame();
	RB_2D_SpanReset();
	s_rbSlot = -1;

	/* No 2D window/scissor invalidation here.  That cache belongs to the FRONT
	   END's recording stream, and this runs on the walk thread for a frame the
	   front end recorded earlier: dropping it from here raced the front end's
	   recording of the NEXT frame, which is what made a frame's first 2D window
	   be elided on some frames and recorded on others.  The front end drops it
	   at its own frame boundary (RE_EndFrame) and after every 3D command. */
}

qboolean RB_RecordSurfaces( VkCommandBuffer cmd )
{
	return qtrue;
}

void RB_FramePrologue( void )
{
}

/* ---- the 2D immediate stream: a run is one pipeline and one ring slice -- */

typedef struct {
	qboolean	open;
	int			topology;
	int			verts;
	int			base;
	qboolean	pending;
	image_t		*image;
	VkPipeline	pipeline;
} vkImmRun_t;

static vkImmRun_t	s_imm;

static float		s_immCurSt[2];	/* the stream's current st, stamped per vertex */
static byte			s_immCurColor[4] = { 255, 255, 255, 255 };

static vkTessVertex_t *RB_Imm_NewVertex( void )
{
	vkTessVertex_t	*v;

	v = VK_VertexRing_Reserve( 1, NULL );
	if ( !v ) {
		return NULL;
	}

	v->xyz[2]   = 0.0f;
	v->st[0]    = s_immCurSt[0];
	v->st[1]    = s_immCurSt[1];
	memcpy( v->color, s_immCurColor, 4 );

	s_imm.verts++;
	s_imm.pending = qtrue;

	return v;
}

int RB_ImmStreamVerts( void )
{
	return s_imm.open ? ( s_imm.base + s_imm.verts ) : VK_VertexRing_Used();
}

static qboolean RB_Imm_BindImage( image_t *image )
{
	VkImageView view;
	VkSampler sampler;

	if ( !image ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "immediate run without an image",
					  VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	view = VK_Texture_View( (int)image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "immediate run image without a view",
					  VK_ERROR_INITIALIZATION_FAILED );
		return qfalse;
	}

	if ( !VK_Texture_Sampler( qtrue, image->numMipmaps > 1, qfalse, &sampler ) ) {
		return qfalse;
	}

	return VK_Draw_BindTexture( vk.commandBuffers[s_rbSlot], view, sampler );
}

/* The run's pipeline state is the caller's, not a field of the run. */
void RB_ImmBeginWithImage( int topology, image_t *image, unsigned long stateBits )
{
	vkPipelineKey_t key;
	vkImmRun_t *run = &s_imm;

	if ( run->open && ( run->topology != topology || run->image != image ) ) {
		RB_ImmEnd();
	}

	memset( &key, 0, sizeof( key ) );
	key.topology = topology;
	key.stateBits = stateBits;
	key.cull = CT_TWO_SIDED;
	key.depthTest = ( stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	key.depthWrite = ( stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;

	/* The ortho is recorded on every run, at function scope: a run that trusted
	   the previous run's push drew with a stale ortho, and a 2D quad under a
	   stale ortho lands as a garbage sheet across the frame.  Which ortho it
	   carries depends on the space the run draws in -- inside 2D it is the
	   window's own, the same value the window setup pushed, and the 3D getters
	   do not hold it -- but the recording itself is unconditional. */
	struct { float ortho[16]; float alphaTest[4]; } push;	/* vk_draw.c's layout */
	const float *window, *proj, *view;
	int r, c, k;

	memset( &push, 0, sizeof( push ) );
	window = backEnd.in2D ? VK_Draw_CurrentOrtho() : NULL;
	if ( window ) {
		memcpy( push.ortho, window, sizeof( push.ortho ) );
	} else {
		proj = RB_GetProjectionMatrix();
		view = RB_GetModelViewMatrix();
		for ( r = 0; r < 4; r++ ) {
			for ( c = 0; c < 4; c++ ) {
				float sum = 0.0f;

				for ( k = 0; k < 4; k++ ) {
					sum += proj[r * 4 + k] * view[k * 4 + c];
				}
				push.ortho[c * 4 + r] = sum;
			}
		}
		/* the push holds the product transposed, keeping the flat offsets of
		   the negated row: 1, 5, 9, 13 */
		VK_Mat4_ClipToWindow( push.ortho );
	}
	push.alphaTest[0] = 0.0f;
	push.alphaTest[1] = 0.0f;

	qvk.CmdPushConstants( vk.commandBuffers[s_rbSlot], VK_Draw_Layout(),
						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						  0, sizeof( push ), &push );

	run->pipeline = VK_Draw_Pipeline( &key );
	if ( run->pipeline == VK_NULL_HANDLE ) {
		return;
	}

	VK_Draw_BindPipeline( vk.commandBuffers[s_rbSlot], run->pipeline );
	/* the run's own state, as the stage pass does: a leftover push would arm the
	   previous surface's test on this one */
	VK_Draw_SetAlphaTest( vk.commandBuffers[s_rbSlot], stateBits );

	if ( !RB_Imm_BindImage( image ) ) {
		return;
	}

	run->open = qtrue;
	run->topology = topology;
	run->image = image;
	run->verts = 0;
	run->base = VK_VertexRing_Used();
}

void RB_ImmBegin( int topology )
{
	RB_ImmBeginWithImage( topology, tr.whiteImage, vk_Shade_StateBits() );
}

void RB_ImmEndDeferred( void )
{
}

/* A run reaches the ring whole: the setters rewrite the slice, so an early
   commit would miss vertices.  A commit consumes exactly the slice it drew,
   so a second commit of the same run draws what came after the first, not the
   run again.  Re-issuing the run put every earlier quad of a run back on the
   screen at the last commit's position in the command buffer -- after any 3D
   batch that ran in between -- which is the menu fill over its own star dome
   and the flashing the operator reported. */
void RB_ImmCommit( void )
{
	vkImmRun_t *run = &s_imm;
	VkCommandBuffer cmd;

	if ( !run->open || run->verts == 0 ) {
		return;
	}

	cmd = vk.commandBuffers[s_rbSlot];
	if ( cmd == VK_NULL_HANDLE ) {
		return;
	}

	if ( run->topology == VK_IMM_LINE_LOOP ) {
		/* No line-loop topology in Vulkan: repeat the first vertex to close it. */
		vkTessVertex_t *first = VK_VertexRing_Tail( run->verts );
		vkTessVertex_t *copy;

		if ( first ) {
			copy = VK_VertexRing_Reserve( 1, NULL );
			if ( copy ) {
				*copy = *first;
				run->verts++;
			}
		}
	}

	VK_Draw_BindRing( cmd );

	{
		static int s_dbg;
		if ( PROBE_On() && s_dbg < 40 ) {
			VK_Guard_Row( "VK:dbg", "imm-commit topo=%d verts=%d base=%d in2d=%d",
						  run->topology, run->verts, run->base, backEnd.in2D );
			s_dbg++;
		}
	}
	VK_Draw_Draw( cmd, run->verts, run->base );

	backEnd.pc.c_vertexes += run->verts;
	backEnd.pc.c_surfaces++;

	run->base += run->verts;
	run->verts = 0;
	run->pending = qfalse;
}

void RB_ImmEnd( void )
{
	if ( s_imm.pending ) {
		RB_ImmCommit();
	}

	s_imm.open = qfalse;
	s_imm.verts = 0;
	s_imm.pending = qfalse;
	s_imm.image = NULL;
}

void RB_ImmFlush( void )
{
	RB_ImmEnd();
}

void RB_ImmVertex2f( float x, float y )
{
	vkTessVertex_t *v = RB_Imm_NewVertex();

	if ( !v ) {
		return;
	}

	v->xyz[0] = x;
	v->xyz[1] = y;
	RB_2DPlaceVertex( v->xyz );
}

void RB_ImmColor4f( float r, float g, float b, float a )
{
	/* the stream's current colour: the vertices after it carry it */
	s_immCurColor[0] = (byte)( r * 255.0f + 0.5f );
	s_immCurColor[1] = (byte)( g * 255.0f + 0.5f );
	s_immCurColor[2] = (byte)( b * 255.0f + 0.5f );
	s_immCurColor[3] = (byte)( a * 255.0f + 0.5f );
}

void RB_ImmTexCoord2f( float s, float t )
{
	s_immCurSt[0] = s;
	s_immCurSt[1] = t;
}

void RB_ShowImages( qboolean quiet )
{
	( void )quiet;
}

qboolean RB_SmpActive( void )
{
	return vk.smpActive;
}

/* ---- the immediate stream's remaining surface: the mirrored 3D front end
   calls the whole reference set, so the 2D stream carries the rest ---------- */

void RB_ImmVertex3f( float x, float y, float z )
{
	vkTessVertex_t *v = RB_Imm_NewVertex();

	if ( !v ) {
		return;
	}

	v->xyz[0] = x;
	v->xyz[1] = y;
	v->xyz[2] = z;
	RB_2DPlaceVertex( v->xyz );
}

void RB_ImmVertex3fv( const float *pos )
{
	RB_ImmVertex3f( pos[0], pos[1], pos[2] );
}

void RB_ImmColor3f( float r, float g, float b )
{
	RB_ImmColor4f( r, g, b, 1.0f );
}

void RB_ImmTexCoord2fv( const float *st )
{
	s_immCurSt[0] = st[0];
	s_immCurSt[1] = st[1];
}

/* ---- the timing instruments: the seam the mirrored front end calls ---- */

static int s_beRootArmed[BE_FAM_COUNT];

void BE_Bump( int family, int counter, int n )
{
	(void)family; (void)counter; (void)n;
}

void BE_RootBegin( int family )
{
	if ( family >= 0 && family < BE_FAM_COUNT ) {
		s_beRootArmed[family]++;
	}
}

void BE_RootEnd( int family )
{
	if ( family >= 0 && family < BE_FAM_COUNT && s_beRootArmed[family] > 0 ) {
		s_beRootArmed[family]--;
	}
}

void BE_Sub( int family, int sub, int usec )
{
	(void)family; (void)sub; (void)usec;
}

void BE_Zone( int family, int zone, int usec )
{
	(void)family; (void)zone; (void)usec;
}

void BE_Name( int family, const char *name, int usec )
{
	(void)family; (void)name; (void)usec;
}

int BE_RootUsec( int family )
{
	(void)family;
	return 0;
}

/* ---- the data pool, the state block and the recorders ---- */

backEndData_t	*backEndData;

backEndData_t	*backEndDataBuf[SMP_FRAMES];

vkstate_t	vkState;

int		max_polys;

int		max_polyverts;

void	R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	/* A 3D view that reached the command list with no surfaces renders nothing
	   but the pass clear, which is exactly "the whole 3D viewport flashed".  A
	   scene that carried entities but produced no surfaces is the anomaly worth
	   naming; an empty scene is a legitimate NOWORLDMODEL-free view. */
	if ( R_ProbeStatsOn() && numDrawSurfs == 0 && tr.refdef.num_entities > 0 ) {
		static int	emptyRows;

		if ( PROBE_Throttle( &emptyRows, 8, 600 ) ) {
			VK_Guard_Row( "VK:view", "empty 3D view ents=%d polys=%d sprites=%d f=%d",
						  tr.refdef.num_entities, tr.refdef.numPolys,
						  tr.refdef.num_sprites, RB_FrameCount() );
		}
	}

	R_Invalidate2DWindowCache();

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;
}

void	R_AddSpriteSurfCmd(drawSurf_t* drawSurfs, int numDrawSurfs) {
    drawSurfsCommand_t* cmd;

    R_Invalidate2DWindowCache();

    cmd = R_GetCommandBuffer(sizeof(*cmd));
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_SPRITE_SURFS;

    cmd->drawSurfs = drawSurfs;
    cmd->numDrawSurfs = numDrawSurfs;

    cmd->refdef = tr.refdef;
    cmd->viewParms = tr.viewParms;
}

/* ---- the 3D view and the draw-surf walk ---------------------------------- */

static void SetViewportAndScissor( void ) {
	RB_LoadProjectionMatrix( backEnd.viewParms.projectionMatrix );

	/* The view's own rect, through the same clamp the 2D window takes: a view
	   whose rect lands off the framebuffer is undefined rasterization in
	   Vulkan, where GL cut it for free. */
	{
		VkCommandBuffer drawCmd = VK_Shade_CommandBuffer();

		VK_Shade_Viewport3D( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
							 backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		VK_Draw_View3D( drawCmd, backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
						backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	}
}

static void RB_Hyperspace( void ) {
	float c;

	c = ( backEnd.refdef.time & 255 ) / 255.0f;

	/* hyperspace is the grey the next view's clear paints */
	vk.clearColor[0] = c;
	vk.clearColor[1] = c;
	vk.clearColor[2] = c;
	vk.clearColor[3] = 1.0f;

	backEnd.isHyperspace = qtrue;
}

void RB_BeginDrawingView (void) {

	RB_2D_Flush();

	backEnd.in2D = qfalse;

	/* A rect cached here would survive the 3D pass and skip re-issuing a
	   scissor that pass overwrote: drop the pair. */
	R_Invalidate2DWindowCache();

	SetViewportAndScissor();

	VK_DepthState( qtrue, qtrue );	/* the 3D pass opens its gate, as retail's does */
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );	/* retail's traced menu issues no stencil call */
	VK_State( VKS_DEFAULT );

	/* the colour is this view's choice, and rides on the pass's clear */
	if ( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		if ( !backEnd.viewParms.farplane_distance || tr.skyRendered || tr.portalRendered )
		{
			if ( r_fastsky->integer )
			{
				vk.clearColor[0] = 0.0f;
				vk.clearColor[1] = 0.0f;
				vk.clearColor[2] = 0.0f;
				vk.clearColor[3] = 1.0f;
			}
		}
		else
		{
			vk.clearColor[0] = vkState.fFogColor[0];
			vk.clearColor[1] = vkState.fFogColor[1];
			vk.clearColor[2] = vkState.fFogColor[2];
			vk.clearColor[3] = 0.0f;
		}
	}

	VK_DepthFuncApply( VK_LEQUAL );	/* pinned per view for the A/B cvar */

	/* Retail's traced menu frame clears DEPTH and nothing else: the view builds no volume, and colour
	   joins only for a world view's fastsky or fog. Colour on the UI's view erased the 2D painted
	   ahead of it; no depth left a later view testing against the first one's depth. */
	{
		VkClearAttachment attachments[2];
		VkClearRect rect;
		int count = 0;

		memset( attachments, 0, sizeof( attachments ) );
		attachments[count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[count].clearValue.depthStencil.depth = 1.0f;
		attachments[count].clearValue.depthStencil.stencil = 0;
		count++;

		if ( ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) == 0 )
		{
			qboolean colour = qfalse;

			if ( !backEnd.viewParms.farplane_distance || tr.skyRendered || tr.portalRendered )
			{
				if ( r_fastsky->integer )
				{
					colour = qtrue;
				}
			}
			else
			{
				colour = qtrue;
			}

			if ( colour ) {
				attachments[count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				attachments[count].clearValue.color.float32[0] = vk.clearColor[0];
				attachments[count].clearValue.color.float32[1] = vk.clearColor[1];
				attachments[count].clearValue.color.float32[2] = vk.clearColor[2];
				attachments[count].clearValue.color.float32[3] = vk.clearColor[3];
				count++;
			}
		}

		/* The clear rect is the view's own, flipped to the framebuffer's origin
		   and clipped to the render area: a rect past the pass is a violation. */
		{
			int rx = backEnd.viewParms.viewportX;
			int ry = vkConfig.vidHeight - backEnd.viewParms.viewportY
					 - backEnd.viewParms.viewportHeight;
			int rw = backEnd.viewParms.viewportWidth;
			int rh = backEnd.viewParms.viewportHeight;
			int ex = (int)vk.swapchainExtent.width;
			int ey = (int)vk.swapchainExtent.height;

			if ( rx < 0 ) { rw += rx; rx = 0; }
			if ( ry < 0 ) { rh += ry; ry = 0; }
			if ( rx + rw > ex ) { rw = ex - rx; }
			if ( ry + rh > ey ) { rh = ey - ry; }

			rect.rect.offset.x = rx;
			rect.rect.offset.y = ry;
			rect.rect.extent.width  = (uint32_t)( rw > 0 ? rw : 0 );
			rect.rect.extent.height = (uint32_t)( rh > 0 ? rh : 0 );
			rect.baseArrayLayer = 0;
			rect.layerCount = 1;
		}

		if ( rect.rect.extent.width && rect.rect.extent.height ) {
			qvk.CmdClearAttachments( VK_Shade_CommandBuffer(), count, attachments, 1, &rect );
		}
	}

	vkState.faceCulling = -1;

	backEnd.skyRenderedThisView = qfalse;

	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		float	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.ori.origin) - plane[3];

		RB_ClipPlane_Set( plane2 );
	} else {
		RB_ClipPlane_Disable();
	}

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}
}

/* One shade part, net of the binds its own flush issued (RB_BeginSurface and
   RB_EndSurface reach the stage iterator); returned so OTHER is the remainder. */
static long long VK_Shade_PartCharge( int part, long long t0, long long nested0 )
{
	long long usec;

	if ( t0 < 0 ) {
		return 0;
	}

	usec = BE_Microseconds64() - t0 - VK_WalkStat_NestedSince( nested0 );
	if ( usec < 0 ) {
		usec = 0;
	}

	VK_WalkStat_ShadeAdd( part, usec );

	return usec;
}

/* Close the shade window: charge the residual to OTHER so the parts sum to the
   window, then charge the window to the walk bucket. */
static void VK_Shade_WindowClose( long long shadeT0, long long nested0, long long parts )
{
	long long usec, other;

	if ( shadeT0 < 0 ) {
		return;
	}

	usec = BE_Microseconds64() - shadeT0 - VK_WalkStat_NestedSince( nested0 );
	other = usec - parts;

	VK_WalkStat_ShadeAdd( VK_WS_OTHER, other > 0 ? other : 0 );
	VK_WalkStat_Add( VK_WP_SHADE, usec );
}

void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				entityNum, oldEntityNum;
	int				dlightMap, oldDlightMap;
	int				fogNum, oldFogNum;
	qboolean		depthRange, oldDepthRange;
	qboolean		bStaticModel, oldbStaticModel;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;

	RB_BeginDrawingView ();

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	oldFogNum = -1;
	oldDlightMap = 0;
	oldSort = -1;
	oldbStaticModel = qfalse;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.currentSphere = &backEnd.sphere;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		/* a null surface pointer is the fault that died at
		   RB_RenderDrawSurfList+0xb4; the sprite walk already answers it */
		if ( drawSurf->surface == NULL ) {
			static int nullRows;

			if ( nullRows < 8 || ( nullRows % 500 ) == 0 ) {
				VK_Guard_Hit( VK_GSITE_QUIRK, va( "draw surf %d of %d carries no surface",
												  i, numDrawSurfs ), VK_INCOMPLETE );
			}
			nullRows++;
			continue;
		}
		/* the surface's first word picks the walker: a key past the table is a
		   call through garbage, a fault that names nothing */
		if ( *drawSurf->surface < 0 || *drawSurf->surface >= SF_NUM_SURFACE_TYPES ) {
			static int surfRows;

			if ( surfRows < 8 || ( surfRows % 500 ) == 0 ) {
				VK_Guard_Hit( VK_GSITE_QUIRK, va( "draw surf names surface type %d",
												  *drawSurf->surface ), VK_INCOMPLETE );
			}
			surfRows++;
			continue;
		}
		if ( drawSurf->sort == (unsigned)oldSort && *drawSurf->surface != SF_SPRITE ) {
			long long walkT0 = VK_WALK_T0();
			long long walkNest0 = VK_WALK_NEST0();

			/* the static arena's arm: a resident world surface advances the
			   counters its generator would have, without generating */
			VK_Shade_NoteBatchSurf();
			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			}
			VK_WALK_ADD_NET( walkT0, walkNest0, VK_WP_GEN );
			continue;
		}
		oldSort = (int)drawSurf->sort;

		/* The per-surface bookkeeping between the surface generators: the sort
		   decompose, the entity rotation and the shader-change flush.  Each
		   named part is charged net of the binds it flushes; OTHER is the
		   residual, so the parts sum to the window. */
		long long shadeT0 = VK_WALK_T0();
		long long shadeNested0 = VK_WalkStat_Nested();
		long long shadeParts = 0;
		long long partT0, partNested0;

		partT0 = VK_WALK_T0();
		partNested0 = VK_WalkStat_Nested();

		if (*drawSurf->surface != SF_SPRITE) {
			R_DecomposeSort(drawSurf->sort, &entityNum, &shader, &fogNum, &dlightMap);
			bStaticModel = qfalse;
		} else {
			shader = tr.sortedShaders[((refSprite_t*)drawSurf->surface)->shaderNum];
			entityNum = ENTITYNUM_WORLD;
			fogNum = ((refSprite_t*)drawSurf->surface)->fogNum;
			dlightMap = 0;
			bStaticModel = qfalse;
		}
		shadeParts += VK_Shade_PartCharge( VK_WS_DECOMPOSE, partT0, partNested0 );

		if ( shader == NULL ) {
			/* the guard named the key; the window still closes or its parts
			   would sit outside every shade total */
			VK_Shade_WindowClose( shadeT0, shadeNested0, shadeParts );
			continue;
		}

		if (shader != oldShader || fogNum != oldFogNum || dlightMap != oldDlightMap
			|| (shader->noMerge & 1)
			|| ( entityNum != oldEntityNum && !shader->entityMergable )
			|| ( bStaticModel != oldbStaticModel && !shader->entityMergable )) {
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			/* the srf sub-parts are charged inside the pair, so the window arms
			   only across it: the sprite and world walks reach the same two
			   functions and must not add to this window's split */
			VK_SrfPart_Arm();
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			VK_SrfPart_Disarm();
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlightMap = dlightMap;
			shadeParts += VK_Shade_PartCharge( VK_WS_SURFACE, partT0, partNested0 );
		}

		if ( entityNum != oldEntityNum || bStaticModel != oldbStaticModel ) {
			depthRange = qfalse;

			backEnd.sphere.TessFunction = NULL;

			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();

			if (bStaticModel) {
				backEnd.currentEntity = 0;
				backEnd.currentStaticModel = &backEnd.refdef.staticModels[entityNum];
				R_RotateForStaticModel(backEnd.currentStaticModel, &backEnd.viewParms, &backEnd.ori );
			}
			else {
				backEnd.currentStaticModel = NULL;
				if (entityNum != ENTITYNUM_WORLD) {
					backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
					backEnd.shaderStartTime = backEnd.currentEntity->e.shaderTime;

					R_RotateForEntity(backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori);

					if (backEnd.currentEntity->needDlights) {
						R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
					}

					if (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) {
						depthRange = qtrue;
					}
				}
				else {
					backEnd.currentEntity = &tr.worldEntity;
					backEnd.ori = backEnd.viewParms.world;
					backEnd.shaderStartTime = 0.0;
					R_TransformDlights(backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori);
				}
			}
			shadeParts += VK_Shade_PartCharge( VK_WS_ROTATE, partT0, partNested0 );

			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			RB_LoadModelViewMatrix(  backEnd.ori.modelMatrix );
			shadeParts += VK_Shade_PartCharge( VK_WS_MODELVIEW, partT0, partNested0 );

			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					vkDepthRange (0, 0.3);
				} else {
					vkDepthRange (0, 1.0);
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
			oldbStaticModel = bStaticModel;
		}

		if (bStaticModel)
		{
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			if (!r_fullbright->integer) {
				RB_Static_BuildDLights();
			}

			if (!backEnd.currentStaticModel->bStaticLightingCalculated) {
				RB_Static_SetupModel();
			}
			shadeParts += VK_Shade_PartCharge( VK_WS_STATICDLIGHTS, partT0, partNested0 );
		}
		else if (backEnd.currentEntity->e.tiki && shader->needsLSpherical)
		{
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			R_Sphere_SetupEntity();
			shadeParts += VK_Shade_PartCharge( VK_WS_SPHERE, partT0, partNested0 );
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		VK_Shade_WindowClose( shadeT0, shadeNested0, shadeParts );

		{
			long long walkT0 = VK_WALK_T0();
			long long walkNest0 = VK_WALK_NEST0();

			VK_Shade_NoteBatchSurf();
			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			}
			VK_WALK_ADD_NET( walkT0, walkNest0, VK_WP_GEN );
		}
	}

	if (oldShader != NULL) {
		RB_EndSurface();
	}

	RB_LoadModelViewMatrix(  backEnd.viewParms.world.modelMatrix );
	if ( oldDepthRange ) {
		vkDepthRange (0, 1.0);
	}

	if (!(backEnd.refdef.rdflags & RDF_FULLBRIGHT)) {
		R_DrawLensFlares();
	}

	if (g_bInfoworldtris)
	{
		g_bInfoworldtris = 0;
		R_PrintInfoWorldtris();
	}

	RB_ShadowFinish();

}

void RB_RenderSpriteSurfList(drawSurf_t* drawSurfs, int numDrawSurfs) {
	shader_t	*shader;
	shader_t	*oldShader;
	qboolean	depthRange;
	qboolean	oldDepthRange;
	int			i;
	drawSurf_t	*drawSurf;

    backEnd.currentEntity = &tr.worldEntity;
    backEnd.currentStaticModel = 0;

	r_spriteListOpen = 1;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.ori = backEnd.viewParms.world;

	oldShader = NULL;
    depthRange = qfalse;
    oldDepthRange = qfalse;

    for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++) {
        if (!drawSurf->surface) continue;
        {
			refSprite_t *spr = (refSprite_t*)drawSurf->surface;

			if (spr->shaderNum < 0 || spr->shaderNum >= tr.numShaders) continue;
			shader = tr.sortedShaders[spr->shaderNum];
			if ( shader == NULL ) {
				continue;	/* the guard named the key */
			}
			depthRange = (spr->renderfx & RF_DEPTHHACK) != 0;

			if ((shader != oldShader || (shader->noMerge & 1)) && !shader->entityMergable)
			{
				if (oldShader) {
					RB_EndSurface();
				}

				RB_BeginSurface(shader, spr->fogNum);
				oldShader = shader;
			}

			RB_LoadModelViewMatrix( backEnd.ori.modelMatrix);

			if (oldDepthRange != depthRange)
			{
				vkDepthRange(0.0, depthRange ? 0.3 : 1.0);
				oldDepthRange = depthRange;
			}

			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;

			rb_surfaceTable[*drawSurf->surface](drawSurf->surface);
		}
	}

	if (oldShader) {
		RB_EndSurface();
	}

	r_spriteListOpen = 0;

    RB_LoadModelViewMatrix( backEnd.viewParms.world.modelMatrix);
	if (oldDepthRange) {
		vkDepthRange(0.0, 1.0);
	}
}

void RB_SetupFog(void) {
	if (!backEnd.viewParms.farplane_distance) {
		vkState.externalSetState &= ~VKS_FOG;
	} else if (r_farplane_nofog->integer) {
		vkState.externalSetState &= ~VKS_FOG;
	} else if (!backEnd.viewParms.farplane_fog) {
		vkState.externalSetState &= ~VKS_FOG;
	} else {
		vec4_t vFogColor;

        vFogColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
        vFogColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
        vFogColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
        vFogColor[3] = 1.0;
        VK_SetFogColor(vFogColor);

		vkState.externalSetState |= VKS_FOG;
	}
}

const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	BE_RootBegin( BE_FAM_DS );

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	/* The frame-carry law: the walk reads the record's copy of the frame, not
	   the live tr.* the front end may be refilling. */
	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	/* front end ahead of the handed-over frame: nonzero means the live reads raced */
	if ( R_ProbeStatsOn() ) {
		static int carryRows;

		if ( cmd->refdef.num_dlights != tr.refdef.num_dlights
			|| cmd->refdef.sky_alpha != tr.refdef.sky_alpha
			|| cmd->refdef.rdflags != tr.refdef.rdflags ) {
			if ( PROBE_Throttle( &carryRows, 8, 600 ) ) {
				VK_Guard_Row( "VK:refdefcarry",
							  "front end ahead dlive=%d drec=%d skylive=%.3f skyrec=%.3f "
							  "rflive=%d rfrec=%d f=%d gen=%d",
							  tr.refdef.num_dlights, cmd->refdef.num_dlights,
							  tr.refdef.sky_alpha, cmd->refdef.sky_alpha,
							  tr.refdef.rdflags, cmd->refdef.rdflags, RB_FrameCount(),
							  r_sequencenumber );
			}
		}
	}

	backEnd.pc.c_surfaces += 0;

	VK_Shade_NoteSurfCmds( cmd->numDrawSurfs );
	RB_SetupFog();
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	BE_RootEnd( BE_FAM_DS );

	return (const void *)(cmd + 1);
}

const void* RB_SpriteSurfs(const void* data) {
    const drawSurfsCommand_t* cmd;

    if (tess.numIndexes) {
        RB_EndSurface();
    }

    cmd = (const drawSurfsCommand_t*)data;

    backEnd.refdef = cmd->refdef;
    backEnd.viewParms = cmd->viewParms;

	/* The sprite pass is a 3D view and in2D is the walk's flag: cleared here as
	   RC_DRAW_SURFS clears it in RB_BeginDrawingView.  A sprite list reached with
	   in2D still up takes RB_StageIterator2D at RB_BeginSurface. */
    backEnd.in2D = qfalse;

	RB_SetupFog();
    RB_RenderSpriteSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

    return (const void*)(cmd + 1);
}

int RB_ModelViewDepth( void )
{
	return VK_Shade_ModelViewDepth();
}

int RB_ProjectionDepth( void )
{
	return VK_Shade_ProjectionDepth();
}

/* the reference renderer's debug overlay; the row names the site */
void R_DrawDebugNumber( const vec3_t org, float number, float scale, float r, float g, float b, int precision )
{
	(void)org; (void)number; (void)scale; (void)r; (void)g; (void)b; (void)precision;
}


void VKimp_LogComment(char *comment)
{
	(void)comment;
}

unsigned long RF_ForceAlphaStateBits( unsigned long stateBits )
{
	if ( backEnd.currentEntity && ( backEnd.currentEntity->e.renderfx & RF_FORCE_ALPHA )
		&& backEnd.currentEntity->e.shaderRGBA[3] < 255 )
	{
		return ( stateBits & ~( VKS_SRCBLEND_BITS | VKS_DSTBLEND_BITS | VKS_DEPTHMASK_TRUE ) )
		       | VKS_SRCBLEND_SRC_ALPHA | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}

	return stateBits;
}

/* the reference renderer drives this through the font's renderer; the row names the site */
void R_DrawFloatingString( fontheader_t *font, const char *text, const vec3_t org,
						   const vec4_t color, float scale, int maxlen )
{
	(void)font; (void)text; (void)org; (void)color; (void)scale; (void)maxlen;
}

/* ---- the 2D walk, which the M2 fixtures prove: one pass, pipeline, bind -- */
void RB_StageIterator2D( void )
{
	int stage;

	/* This lane's 2D dispatch is RB_StageIterator2D, which never reaches
	   RB_StageIteratorGeneric, so the clock row is taken here as well: a
	   scrolling 2D shader (a wall panel's screen, a HUD effect) animates off
	   the same tess.shaderTime. */
	tess.shaderTime = backEnd.refdef.floatTime;
	VK_Shade_NoteShaderTime( "2d" );

	for ( stage = 0; stage < tess.numPasses; stage++ ) {
		shaderStage_t *pStage = tess.xstages[stage];
		const color4ub_t *colors;

		if ( !pStage || !pStage->active ) {
			continue;
		}

		/* A stage whose gens the tess pipeline cannot express is resolved into
		   the CPU colour array first; the vertex-driven ones keep the 2D quad's
		   own colour (see VK_StageResolve2DColor). */
		colors = VK_StageResolve2DColor( pStage ) ? tess.svars.colors : tess.vertexColors;

		/* the 2D walk was the one iterator that never reported into the frame's
		   stage census, so a menu-heavy frame under-counted its own stages */
		VK_Shade_NoteStage( tess.shader );
		RB_StagePass( pStage, colors );
	}
}
