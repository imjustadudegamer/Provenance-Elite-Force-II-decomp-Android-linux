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


/* ---- the 2D coverage census ---------------------------------------------- */
/* "issued" and "rasterised" are different claims and the scissor is what makes the gap: the census reads the rect beside the draw and
   reports quad INTERSECT scissor INTERSECT render area (px=0 proves no raster) plus the three rects that say where the state came from.
   vk_2dCover 2 adds a readback cross-check over the presented frame.  Gate: qa_probe AND vk_2dCover, default 0. */

/* Declared here rather than in tr_local.h: the accessors live in vk_draw.c and tr_backend.c is their only consumer. */
void	 VK_Draw_LastScissorRect( int out[4] );
qboolean VK_Draw_LastScissorValid( void );
qboolean VK_Draw_2DAppliedGet( int *win, int *scn );
int		 VK_Draw_LastSetIndex( void );
int		 VK_Draw_LastAlphaTest( void );
qboolean VK_ModelPixelAtSwap( int x, int y, byte out[4] );

#define RB_2DCOV_SPAN_MAX	1024

/* Per-frame row budgets: a menu frame carries tens of spans (784k rows / 270 MB of one boot), so each detail family names its first rows and one row names the drop, while the frame summary stays uncapped as the family's answer */
#define RB_2DCOV_SPAN_ROWS		4
#define RB_2DCOV_SHADER_ROWS	8
#define RB_2DCOV_READBACK_ROWS	4

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
static int			s_2dCovSpanRows, s_2dCovShaderRows, s_2dCovReadbackRows;

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
		s_2dCovSpanRows++;
		if ( s_2dCovSpanRows <= RB_2DCOV_SPAN_ROWS ) {
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
			s_2dCovShaderRows++;
			if ( s_2dCovShaderRows > RB_2DCOV_SHADER_ROWS ) {
				continue;
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

				s_2dCovReadbackRows++;
				if ( s_2dCovReadbackRows > RB_2DCOV_READBACK_ROWS ) {
					continue;		/* each row is a host readback as well as a line */
				}

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

	/* the row budgets belong to the frame that just ended */
	if ( s_2dCovSpanRows > RB_2DCOV_SPAN_ROWS ) {
		VK_Guard_Row( "VK:2dcov", "frame=%d spans=%d spanrows=%d truncated=%d",
					  s_2dCovFrame, s_2dCovCount, s_2dCovSpanRows,
					  s_2dCovSpanRows - RB_2DCOV_SPAN_ROWS );
	}
	s_2dCovSpanRows    = 0;
	s_2dCovShaderRows  = 0;
	s_2dCovReadbackRows = 0;

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
			/* no span state to apply: the draw runs under the command buffer's scissor, the last rect VK_Draw_ScissorRect was given, which is
			   what the coverage is measured against -- a one-shot writer is drawn where it is written, so this IS its authored state. */
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

			s_2dCovSpanRows++;
			if ( s_2dCovSpanRows <= RB_2DCOV_SPAN_ROWS ) {
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
			}

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

	/* 0 is "no texture" here too: the first slot is a real image */
	if ( image->texnum == 0 ) {
		static int s_zeroRows;

		s_zeroRows++;
		if ( R_ProbeMarkOn() && ( s_zeroRows < 8 || ( s_zeroRows % 600 ) == 0 ) ) {
			VK_Guard_Row( "VK:bind", "handle=0 image='%s' pass=2d shader='%s' f=%d",
						  image->imgName[0] ? image->imgName : "(unnamed)",
						  tess.shader ? tess.shader->name : "?", RB_FrameCount() );
		}
		return qfalse;
	}

	/* the 2D half of the stamp witness: the bind the world census cannot see and the path every menu material and font takes, anomaly-only */
	if ( PROBE_On() ) {
		int now, frame;

		now = R_Bundle_StampFold( &stage->bundle[0], image, &frame );
		if ( now != stage->bundle[0].imgAnimStamp ) {
			static int s_staleRows;

			s_staleRows++;
			if ( s_staleRows <= 24 || ( s_staleRows % 256 ) == 0 ) {
				VK_Guard_Row( "VK:2dset",
							  "MISS why=stale-image shader='%s' f=%d "
							  "parsed=%d now=%d fr=%d img='%s' tex=%d",
							  tess.shader ? tess.shader->name : "?", RB_FrameCount(),
							  stage->bundle[0].imgAnimStamp, now, frame,
							  image->imgName[0] ? image->imgName : "(unnamed)",
							  (int)image->texnum );
			}
		}
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

		/* the rect the quad actually lands in, in the same space the trace logs: the ortho range is recovered from the pushed matrix
		   (m[0] = 2/(r-l), m[12] = -(r+l)/(r-l)), so the row is right for a pillarboxed or window-sized window too, and `srect' diffs
		   call for call against the trace's stream and against `win'/`scn' below. */
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
			int stg;

			for ( stg = 0; stg < MAX_SHADER_STAGES && tess.xstages[stg] != stage; stg++ ) {
			}

			VK_Guard_Row( "VK:2ddraw",
						  "n=%d shader='%s' stg=%d img='%s' tex=%d verts=%d idx=%d "
						  "srect=(%.1f,%.1f)-(%.1f,%.1f) st=(%.3f,%.3f)-(%.3f,%.3f) "
						  "sb=0x%x col=%d,%d,%d,%d vert=%d,%d,%d,%d rgbGen=%d "
						  "const=%d,%d,%d,%d alphaGen=%d "
						  "fb=%d nv=%d "
						  "win=(%d,%d,%d,%d) scn=(%d,%d,%d,%d) wrap=%d,%d clamp=%d",
						  s_rows,
						  tess.shader ? tess.shader->name : "?",
						  stg,
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
						  ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) ? 1 : 0,
						  ( backEnd.refdef.rdflags & RDF_NIGHTVISION ) ? 1 : 0,
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
	VK_Shade_ShadowState( &key.stencilTest, &key.stencilFunc, &key.stencilRef,
						  &key.stencilMask, &key.stencilOpFail, &key.stencilOpZFail,
						  &key.stencilOpPass, &key.cullFace, &key.colorMask );

	/* A 2D batch that loses its two-sided or depth-off state culls or z-fails
	   every fragment and reads as text that never rasterised. */
	if ( backEnd.in2D && PROBE_On() && ( key.cull != CT_TWO_SIDED || key.depthTest ) ) {
		static int	s_rows;

		s_rows++;
		if ( s_rows <= 8 || ( s_rows % 256 ) == 0 ) {
			VK_Guard_Row( "VK:2dstate",
						  "cull=%d cullface=%d depth=%d cmask=0x%x stencil=%d in2d=%d shader='%s' pubatest=%d f=%d",
						  (int)key.cull, key.cullFace, key.depthTest ? 1 : 0, key.colorMask,
						  key.stencilTest ? 1 : 0, backEnd.in2D,
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

	/* wrapClampModeX/Y hold the legacy wrap enum (REPEAT is 10497, not 0) and come from `clampmap', which the GLES3 twin's whole 2D pass samples by;
	   testing only IMGFLAG_CLAMPTOEDGE left every 2D clampmap stage on REPEAT, so a linear-sampled panel bled its opposite edge into the border.
	   clampTime is not a wrap: it is a tcMod clock and no shipped shader sets it. */
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

	/* no 2D window/scissor invalidation here: that cache belongs to the front end's recording stream and this runs on the walk thread for a
	   frame the front end recorded earlier -- dropping it here raced the front end's recording of the NEXT frame.  The front end drops it at
	   its own frame boundary (RE_EndFrame) and after every 3D command. */
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
	unsigned	shadowGen;	/* the stencil/cull/colour-mask state the run opened under */
	qboolean	shadow;		/* the run armed the stencil: a volume pass */
} vkImmRun_t;

static vkImmRun_t	s_imm;

/* The volume's own commits: `imm` in the frame row counts the 2D batches only,
   so without these two numbers "the volume reached the command buffer" had no
   witness at all. */
static int	s_immShadowRuns;
static int	s_immShadowVerts;

/* the key the last immediate runs were opened with: the volume row reads the
   pipeline state that was actually built, not the state word it meant */
static vkPipelineKey_t	s_immKey[2];
static unsigned			s_immKeySeq;

qboolean VK_Imm_KeyGet( int back, vkPipelineKey_t *out ) {
	if ( !out || s_immKeySeq <= (unsigned)back ) {
		return qfalse;
	}
	*out = s_immKey[ ( s_immKeySeq - 1 - (unsigned)back ) & 1u ];
	return qtrue;
}

void VK_Imm_ShadowCommits( int *runs, int *verts ) {
	if ( runs ) {
		*runs = s_immShadowRuns;
	}
	if ( verts ) {
		*verts = s_immShadowVerts;
	}
}

/* the matrix the open run pushed: a row that reports where the run's geometry
   lands has to read the transform the draw actually carries */
static float		s_immPush[16];

static float		s_immCurSt[2];	/* the stream's current st, stamped per vertex */
static byte			s_immCurColor[4] = { 255, 255, 255, 255 };

const float *RB_ImmPushedMatrix( void )
{
	return s_immPush;
}

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

	if ( run->open && ( run->topology != topology || run->image != image
						|| run->shadowGen != VK_Shade_ShadowGen() ) ) {
		RB_ImmEnd();
	}

	memset( &key, 0, sizeof( key ) );
	key.topology = topology;
	key.stateBits = stateBits;
	key.cull = CT_TWO_SIDED;
	key.depthTest = ( stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	key.depthWrite = ( stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;
	/* the shadow volume's arm: the stencil test, the ops it counts with, the
	   cull face it flips between its two volumes and the colour it never writes */
	VK_Shade_ShadowState( &key.stencilTest, &key.stencilFunc, &key.stencilRef,
						  &key.stencilMask, &key.stencilOpFail, &key.stencilOpZFail,
						  &key.stencilOpPass, &key.cullFace, &key.colorMask );

	/* the ortho is recorded on every run, at function scope: a run that trusted the previous run's push drew with a stale ortho, and a 2D quad
	   under a stale ortho lands as a garbage sheet across the frame.  Which ortho it carries depends on the space the run draws in; the recording itself is unconditional. */
	struct { float ortho[16]; float alphaTest[4]; } push;	/* vk_draw.c's layout */
	const float *window;

	memset( &push, 0, sizeof( push ) );
	window = backEnd.in2D ? VK_Draw_CurrentOrtho() : NULL;
	if ( window ) {
		memcpy( push.ortho, window, sizeof( push.ortho ) );
	} else {
		/* column-major proj*view, the same product the stage pass uploads as u_MVP: the shader reads this push as a mat4, so a transposed
		   product put every 3D immediate run -- the volume and this overlay's own quad -- in a space none of its vertices survive. */
		VK_Mat4Mul( push.ortho, RB_GetProjectionMatrix(), RB_GetModelViewMatrix() );
		/* the engine's matrices leave +y up in clip space; row 1 is flat 1, 5, 9, 13 */
		VK_Mat4_ClipToWindow( push.ortho );
	}
	memcpy( s_immPush, push.ortho, sizeof( s_immPush ) );
	push.alphaTest[0] = 0.0f;
	push.alphaTest[1] = 0.0f;

	qvk.CmdPushConstants( vk.commandBuffers[s_rbSlot], VK_Draw_Layout(),
						  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						  0, sizeof( push ), &push );

	run->pipeline = VK_Draw_Pipeline( &key );
	if ( run->pipeline == VK_NULL_HANDLE ) {
		return;
	}

	s_immKey[s_immKeySeq & 1u] = key;
	s_immKeySeq++;
	run->shadow = key.stencilTest;

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
	run->shadowGen = VK_Shade_ShadowGen();
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

/* a run reaches the ring whole and a commit consumes exactly the slice it drew, so a second commit draws what came after the first, not the
   run again -- re-issuing the run put every earlier quad back on screen after any 3D batch that ran in between. */
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

	if ( run->shadow ) {
		s_immShadowRuns++;
		s_immShadowVerts += run->verts;
	}

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

/* ---- the surf record's own list ------------------------------------------ */

/* The record has to own its surfaces.  cmd->drawSurfs used to be a raw pointer
   into tr.refdef.drawSurfs, which the front end rewrites for a later frame while
   the walk reads this record; a walk that meets the refilled array meets it in
   add order, and there a caster's <stencil shadow> surf comes before the surfaces
   its volume has to be depth-tested against.  Both volume walls then pass, the
   counters cancel, and the stencil plane reads empty with every other reading in
   the frame healthy. */
static drawSurfsCommand_t *R_AllocSurfCmd( int headBytes, int numSurfs ) {
	int					listBytes = ( numSurfs > 0 ) ? numSurfs * (int)sizeof( drawSurf_t ) : 0;
	int					bytes = headBytes + listBytes + (int)sizeof( void * );
	drawSurfsCommand_t	*cmd = (drawSurfsCommand_t *)R_GetCommandBuffer( bytes );
	int					pad = (int)sizeof( void * );

	/* The list advances a command by its own size, and this record keeps its
	   surfaces behind the struct: without the padded total the walk read the
	   slice as commands and every record after it -- the whole 2D screen -- was
	   lost.  The allocator pads its request to pointer size, so the total does
	   too. */
	if ( cmd ) {
		cmd->totalBytes = ( bytes + pad - 1 ) & ~( pad - 1 );
	}
	return cmd;
}

static void R_CarryDrawSurfs( drawSurfsCommand_t *cmd, drawSurf_t *surfs, int numSurfs ) {
	unsigned long	addr, align, pad;
	byte			*base;

	if ( numSurfs <= 0 || surfs == NULL ) {
		cmd->drawSurfs = NULL;
		return;
	}

	base  = (byte *)cmd + sizeof( *cmd );
	addr  = (unsigned long)base;
	align = (unsigned long)sizeof( void * );
	pad   = ( align - ( addr % align ) ) % align;

	cmd->drawSurfs = (drawSurf_t *)( base + pad );
	Com_Memcpy( cmd->drawSurfs, surfs, numSurfs * sizeof( drawSurf_t ) );
}

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

	cmd = R_AllocSurfCmd( sizeof( *cmd ), numDrawSurfs );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	if ( r_surfOwn && r_surfOwn->integer ) {
		R_CarryDrawSurfs( cmd, drawSurfs, numDrawSurfs );
	} else {
		/* the record names the live array again, which is only ever safe when the
		   front end cannot refill it before the walk: the switch exists so the two
		   behaviours can be compared on one boot */
		cmd->drawSurfs = drawSurfs;
	}
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	/* the record's own list is the one the walk reads, so the refdef points at it */
	cmd->refdef.drawSurfs = cmd->drawSurfs;
	cmd->viewParms = tr.viewParms;

	/* the record owns a copy of THIS view's slice while the refdef it carries
	   counts the whole frame: a UI shader that never reaches the walk is either
	   absent from the copy or added after it, and only these numbers say which */
	if ( R_ProbeMarkOn() ) {
		static int	s_addRows;

		s_addRows++;
		if ( s_addRows <= 24 || ( s_addRows % 300 ) == 0 ) {
			int	menu = 0, i;

			for ( i = 0; i < numDrawSurfs; i++ ) {
				const char *n = drawSurfs[i].shader ? drawSurfs[i].shader->name : "";

				if ( !Q_strncmp( n, "menu/", 5 ) || strstr( n, "lcars" ) ) {
					menu++;
				}
			}
			VK_Guard_Row( "VK:surfcmd",
						  "site=add view=%d copied=%d refdef=%d menuSurfs=%d portal=%d f=%d",
						  backEnd.viewParms.isPortal ? 1 : 0, numDrawSurfs,
						  tr.refdef.numDrawSurfs, menu,
						  backEnd.viewParms.isPortal ? 1 : 0, RB_FrameCount() );
		}
	}
}

void	R_AddSpriteSurfCmd(drawSurf_t* drawSurfs, int numDrawSurfs) {
    drawSurfsCommand_t* cmd;

    R_Invalidate2DWindowCache();

    cmd = R_AllocSurfCmd( sizeof( *cmd ), numDrawSurfs );
    if (!cmd) {
        return;
    }
    cmd->commandId = RC_SPRITE_SURFS;

    if ( r_surfOwn && r_surfOwn->integer ) {
        R_CarryDrawSurfs( cmd, drawSurfs, numDrawSurfs );
    } else {
        cmd->drawSurfs = drawSurfs;
    }
    cmd->numDrawSurfs = numDrawSurfs;

    cmd->refdef = tr.refdef;
    cmd->refdef.spriteSurfs = cmd->drawSurfs;
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

/* ---- the sub-view's own rect, named where the pixels are decided ---------- */

/* The sub-view is rasterised before the main view and is masked by it, so the
   panel keeps a sub-view pixel only if the sub-view painted that pixel.  This
   row carries the rect the sub-view was given, the scissor its draws actually
   ran under, the marker's screen rect, the depth range and the draws it issued:
   a zero-area intersection is the number that says the content never landed on
   the panel, and a non-empty one says the panel's pixels are there and something
   else is eating them. */
typedef struct {
	int			frame;
	qboolean	subSeen;
	int			vp[4];
	int			scn[4];
	float		range[2];
	int			draws;
	int			markerSeen;
	int			marker[4];
} vkPortalComp_t;

static vkPortalComp_t	s_portalComp;

/* the marker surface's own screen rect, in the caller view's window space; the
   answer is Vulkan's top-left space, so it compares with the scissor directly */
static qboolean RB_PortalMarkerRect( const drawSurf_t *drawSurf, int out[4] ) {
	const srfSurfaceFace_t	*face;
	vec4_t					eye, clip, ndc, win;
	float					x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
	float					h = (float)backEnd.viewParms.viewportHeight;
	int						i;

	if ( !drawSurf->surface || *drawSurf->surface != SF_FACE ) {
		return qfalse;
	}
	face = (const srfSurfaceFace_t *)drawSurf->surface;
	for ( i = 0; i < face->numPoints; i++ ) {
		R_TransformModelToClip( face->points[i], backEnd.ori.modelMatrix,
								backEnd.viewParms.projectionMatrix, eye, clip );
		if ( clip[3] <= 0.0f ) {
			continue;
		}
		R_TransformClipToWindow( clip, &backEnd.viewParms, ndc, win );
		/* the clip helper answers GL's bottom-left window; the scissor is Vulkan's
		   top-left one, and the view's own origin is the viewport's */
		win[1] = h - win[1];
		if ( win[0] < x0 ) x0 = win[0];
		if ( win[1] < y0 ) y0 = win[1];
		if ( win[0] > x1 ) x1 = win[0];
		if ( win[1] > y1 ) y1 = win[1];
	}
	if ( x1 <= x0 || y1 <= y0 ) {
		return qfalse;
	}
	out[0] = backEnd.viewParms.viewportX + (int)( x0 + 0.5f );
	out[1] = VK_Draw_TopY( backEnd.viewParms.viewportY, backEnd.viewParms.viewportHeight )
			 + (int)( y0 + 0.5f );
	out[2] = backEnd.viewParms.viewportX + (int)( x1 + 0.5f );
	out[3] = VK_Draw_TopY( backEnd.viewParms.viewportY, backEnd.viewParms.viewportHeight )
			 + (int)( y1 + 0.5f );
	return qtrue;
}

static void RB_PortalCompRow( void ) {
	int	ix0, iy0, ix1, iy1, iw = 0, ih = 0;
	static int rows;

	ix0 = s_portalComp.scn[0] > s_portalComp.marker[0] ? s_portalComp.scn[0] : s_portalComp.marker[0];
	iy0 = s_portalComp.scn[1] > s_portalComp.marker[1] ? s_portalComp.scn[1] : s_portalComp.marker[1];
	ix1 = s_portalComp.scn[0] + s_portalComp.scn[2];
	iy1 = s_portalComp.scn[1] + s_portalComp.scn[3];
	if ( s_portalComp.marker[2] < ix1 ) ix1 = s_portalComp.marker[2];
	if ( s_portalComp.marker[3] < iy1 ) iy1 = s_portalComp.marker[3];
	if ( ix1 > ix0 ) iw = ix1 - ix0;
	if ( iy1 > iy0 ) ih = iy1 - iy0;

	rows++;
	if ( rows > 8 && ( rows % 600 ) != 0 ) {
		return;
	}
	VK_Guard_Row( "VK:portalframe",
				  "f=%d sub vp=(%d,%d %dx%d) scn=(%d,%d %dx%d) range=%.3f..%.3f "
				  "draws=%d marker=(%d,%d)-(%d,%d) inter=%dx%d",
				  RB_FrameCount(), s_portalComp.vp[0], s_portalComp.vp[1],
				  s_portalComp.vp[2], s_portalComp.vp[3],
				  s_portalComp.scn[0], s_portalComp.scn[1],
				  s_portalComp.scn[2], s_portalComp.scn[3],
				  s_portalComp.range[0], s_portalComp.range[1], s_portalComp.draws,
				  s_portalComp.marker[0], s_portalComp.marker[1],
				  s_portalComp.marker[2], s_portalComp.marker[3], iw, ih );
}

void RB_BeginDrawingView (void) {

	RB_2D_Flush();

	backEnd.in2D = qfalse;

	/* No R_Invalidate2DWindowCache here: the cache is the recorder's, and the walk
	   writing it made a string re-open the screen window over its widget's own. */

	SetViewportAndScissor();

	/* the rect this view is given and the scissor its draws will run under, read
	   where the backend has just issued both */
	if ( R_ProbeMarkOn() && backEnd.viewParms.isPortal ) {
		s_portalComp.frame = RB_FrameCount();
		s_portalComp.subSeen = qtrue;
		s_portalComp.markerSeen = qfalse;
		s_portalComp.draws = 0;
		s_portalComp.vp[0] = backEnd.viewParms.viewportX;
		s_portalComp.vp[1] = backEnd.viewParms.viewportY;
		s_portalComp.vp[2] = backEnd.viewParms.viewportWidth;
		s_portalComp.vp[3] = backEnd.viewParms.viewportHeight;
		if ( VK_Draw_LastScissorValid() ) {
			VK_Draw_LastScissorRect( s_portalComp.scn );
		} else {
			s_portalComp.scn[0] = s_portalComp.scn[1] = 0;
			s_portalComp.scn[2] = s_portalComp.scn[3] = 0;
		}
		VK_Draw_DepthRangeLast( &s_portalComp.range[0], &s_portalComp.range[1] );
	}

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
		/* RB_BeginDrawingView @0x1490 clears 1280 = DEPTH_BUFFER_BIT|STENCIL_BUFFER_BIT when the cvar at off_227CA0 is
		   nonzero OR the one at off_227C9C reads 2, the same pointer RB_ShadowFinish @0x40BD0 gates the overlay on, i.e.
		   the precise shadow tier.  The first term is r_measureOverdraw's readback, which sums the plane every swap.
		   Without the stencil half every view in a frame shared one plane, so a sub-view's caster marked the main
		   view's screen. */
		if ( ( r_measureOverdraw->integer || r_shadows->integer == 2 ) && vkConfig.stencilBits >= 4 ) {
			attachments[count].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
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

	/* RB_BeginDrawingView @0x1490 gates the user plane on the dword 16 bytes before
	   portalPlane, which in this struct is isMirror -- not isPortal.  Arming it for a
	   non-mirror portal (every viewscreen row in the log reads mirror=0) hands the
	   fragment stage an eye-space plane that discards one half of the panel. */
	if ( backEnd.viewParms.isMirror ) {
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

/* the stage-0 image a draw will sample: a wrong bind names both the image and the slot's tenant */
static image_t *RB_Stage0Image( shader_t *shader )
{
	if ( !shader->unfoggedStages[0] ) {
		return NULL;
	}
	return shader->unfoggedStages[0]->bundle[0].image[0];
}

/* the sprite record's own shader, as its model still holds it at draw time */
static shader_t *RB_SpriteModelShader( const refSprite_t *spr )
{
	model_t *model;

	if ( !spr->hModel ) {
		return NULL;
	}
	model = R_GetModelByHandle( spr->hModel );
	if ( !model || !model->d.sprite || !model->d.sprite->shader ) {
		return NULL;
	}
	return model->d.sprite->shader;
}

/* a vertex-colour stage whose stream is pure magenta is the rollback arm, not an authored fade */
static void RB_ColorStreamProbe( shader_t *shader, const char *fam, int index )
{
	const byte		*c;
	int				n, k;
	int				mn[4] = { 255, 255, 255, 255 };
	int				mx[4] = { 0, 0, 0, 0 };
	static int		s_rows;

	if ( !R_ProbeMarkOn() ) {
		return;
	}
	if ( !shader->unfoggedStages[0] || shader->unfoggedStages[0]->rgbGen != CGEN_VERTEX ) {
		return;
	}
	if ( tess.numVertexes <= 0 || tess.numVertexes > SHADER_MAX_VERTEXES ) {
		return;
	}

	c = &tess.vertexColors[0][0];
	for ( n = 0; n < tess.numVertexes; n++ ) {
		for ( k = 0; k < 4; k++ ) {
			int	v = c[ ( n << 2 ) + k ];

			if ( v < mn[k] ) {
				mn[k] = v;
			}
			if ( v > mx[k] ) {
				mx[k] = v;
			}
		}
	}

	if ( mx[0] != 255 || mx[2] != 255 || mn[1] != 0 ) {
		return;
	}

	s_rows++;
	if ( s_rows <= 24 || ( s_rows % 600 ) == 0 ) {
		VK_Guard_Row( "DOCC:color",
					  "magenta-stream fam=%s '%s' i=%d nverts=%d r=%d..%d g=%d..%d b=%d..%d a=%d..%d f=%d",
					  fam, shader->name, index, tess.numVertexes,
					  mn[0], mx[0], mn[1], mx[1], mn[2], mx[2], mn[3], mx[3],
					  RB_FrameCount() );
	}
}

/* retail resolves the index and drops nothing: a stale record takes the live slot */
shader_t *RB_ResolveStaleShader( const char *walk, int gen, int index )
{
	static int	s_rows;

	if ( index < 0 || index >= tr.numShaders ) {
		return NULL;
	}

	if ( R_ProbeMarkOn() ) {
		s_rows++;
		if ( s_rows <= 16 || ( s_rows % 600 ) == 0 ) {
			VK_Guard_Row( "VK:staledraw",
						  "walk=%s gen=%d live=%d index=%d shader='%s' f=%d",
						  walk, gen, tr.shaderGen, index, tr.sortedShaders[ index ]->name,
						  RB_FrameCount() );
		}
	}

	return tr.sortedShaders[ index ];
}

/* names a draw whose carried index no longer matches its shader's live sortedIndex */
void RB_NoteIndexDesync( const char *walk, shader_t *shader, int carriedIndex )
{
	static int	s_rows;

	/* the row is an instrument: master gate */
	if ( !R_ProbeMarkOn() ) {
		return;
	}

	if ( !shader || shader->sortedIndex == carriedIndex ) {
		return;
	}

	s_rows++;
	if ( s_rows <= 32 || ( s_rows % 600 ) == 0 ) {
		VK_Guard_Row( "VK:xidx",
					  "walk=%s shader='%s' live=%d carried=%d delta=%d f=%d",
					  walk, shader->name, shader->sortedIndex, carriedIndex,
					  carriedIndex - shader->sortedIndex, RB_FrameCount() );
	}
}

/* ---- where in the frame the volume is drawn ------------------------------ */

/* A z-pass volume is only correct after its receivers, and "drawn after" is not
   something the sort word proves: it packs sortedIndex at add time.  The frame's
   own draw counter is the witness -- the first draws by name, the counter the
   first <stencil shadow> met, and whether any surface draw came after it. */
#define VK_ORDER_HEAD		40
#define VK_ORDER_NAME		48
#define VK_ORDER_LINE		2048

static char	s_orderHead[VK_ORDER_HEAD][VK_ORDER_NAME];
static int	s_orderHeadAt[VK_ORDER_HEAD];
static int	s_orderHeadCount;
static int	s_orderDraws;
static int	s_orderVolumes;
static int	s_orderVolumeFirst = -1;
static int	s_orderVolumeLast = -1;
static int	s_orderOtherAfter;
static int	s_orderRows;

static void RB_Order_Reset( void ) {
	s_orderHeadCount   = 0;
	s_orderDraws       = 0;
	s_orderVolumes     = 0;
	s_orderVolumeFirst = -1;
	s_orderVolumeLast  = -1;
	s_orderOtherAfter  = 0;
}

static void RB_Order_Note( shader_t *shader ) {
	int	draw;

	if ( !R_ProbeMarkOn() || !shader ) {
		return;
	}

	draw = s_orderDraws++;

	if ( s_orderHeadCount < VK_ORDER_HEAD ) {
		Q_strncpyz( s_orderHead[s_orderHeadCount], shader->name, VK_ORDER_NAME );
		s_orderHeadAt[s_orderHeadCount] = draw;
		s_orderHeadCount++;
	}

	if ( shader == tr.shadowShader ) {
		if ( s_orderVolumeFirst < 0 ) {
			s_orderVolumeFirst = draw;
		}
		s_orderVolumeLast = draw;
		s_orderVolumes++;
	} else if ( s_orderVolumeFirst >= 0 ) {
		s_orderOtherAfter++;
	}
}

static void RB_Order_FrameRow( void ) {
	char	head[VK_ORDER_LINE];
	int		i, written = 0;

	if ( !R_ProbeMarkOn() || s_orderVolumes == 0 ) {
		RB_Order_Reset();
		return;
	}

	s_orderRows++;
	if ( s_orderRows > 16 && ( s_orderRows % 600 ) != 0 ) {
		RB_Order_Reset();
		return;
	}

	head[0] = 0;
	for ( i = 0; i < s_orderHeadCount; i++ ) {
		int n = Com_sprintf( head + written, sizeof( head ) - written, "%s#%d '%s'",
							 i ? "," : "", s_orderHeadAt[i], s_orderHead[i] );

		/* the row's own body is 896 bytes: the head is a window, and the count
		   after it says how much of the frame it left out */
		if ( n < 0 || written + n > 520 ) {
			break;
		}
		written += n;
	}
	if ( i < s_orderHeadCount ) {
		Com_sprintf( head + written, sizeof( head ) - written, ",+%d",
					 s_orderHeadCount - i );
	}

	/* the verdict is the one number the item turns on: a surface draw after the
	   first volume means the volume counted against depth the world had not yet
	   written, and both its walls then pass and cancel */
	VK_Guard_Row( "VK:draworder",
				  "f=%d view=%d draws=%d volumes=%d vol_first=%d vol_last=%d other_after=%d "
				  "verdict=%s head=%s",
				  RB_FrameCount(), tr.viewCount, s_orderDraws, s_orderVolumes,
				  s_orderVolumeFirst, s_orderVolumeLast, s_orderOtherAfter,
				  s_orderOtherAfter ? "volume-before-world" : "volume-after-world",
				  head );

	RB_Order_Reset();
}

void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				entityNum, oldEntityNum;
	int				dlightMap, oldDlightMap;
	int				fogNum, oldFogNum;
	shader_t		*fogShader;
	qboolean		depthRange;
	qboolean		bStaticModel, oldbStaticModel;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
	int				sphereEntity;
	int				portalDraws = 0;

	RB_BeginDrawingView ();

	RB_Order_Reset();

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDlightMap = 0;
	oldSort = -1;
	oldbStaticModel = qfalse;
	sphereEntity = -1;
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
			} else {
				RB_NoteFeedSurf( drawSurf->surface, 3 );
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
			int carriedIndex;

			/* sort decomposes for entity/fog/dlight; the shader comes from the
			   pack-time pointer, not the mutable index. */
			R_DecomposeSort(drawSurf->sort, &entityNum, &fogShader, &fogNum, &dlightMap);
			carriedIndex = ( drawSurf->sort >> QSORT_SHADERNUM_SHIFT ) & QSORT_SHADERNUM_MASK;
			shader = ( drawSurf->shaderGen == tr.shaderGen )
				   ? drawSurf->shader
				   : RB_ResolveStaleShader( "drawsurf", drawSurf->shaderGen, carriedIndex );
			RB_NoteIndexDesync( "drawsurf", shader, carriedIndex );
			RB_Order_Note( shader );
			bStaticModel = qfalse;
		} else {
			refSprite_t *spr = (refSprite_t*)drawSurf->surface;

			shader = ( spr->shaderGen == tr.shaderGen )
				   ? spr->shader
				   : RB_ResolveStaleShader( "sprite", spr->shaderGen, spr->shaderNum );
			RB_NoteIndexDesync( "sprite", shader, spr->shaderNum );
			RB_Order_Note( shader );
			entityNum = ENTITYNUM_WORLD;
			fogNum = spr->fogNum;
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

		if ( R_ProbeMarkOn() ) {
			if ( backEnd.viewParms.isPortal ) {
				portalDraws++;
			} else if ( !s_portalComp.markerSeen && shader->name[0]
						&& !strcmp( shader->name, "textures/common/portal" ) ) {
				/* the marker's own quad is the panel: its screen rect is the rect
				   the sub-view's pixels have to land in */
				s_portalComp.markerSeen = RB_PortalMarkerRect( drawSurf, s_portalComp.marker );
			}
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

			/* the range is a recorded command, not retail's sticky GL state, so every transition re-issues it */
			vkDepthRange( 0, depthRange ? 0.3 : 1.0 );

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
		/* the setup depends on the entity and the view, not the shader: one pass
		   per entity instead of one per spherical draw surface */
		else if (backEnd.currentEntity->e.tiki && shader->needsLSpherical
			&& entityNum != sphereEntity)
		{
			partT0 = VK_WALK_T0();
			partNested0 = VK_WalkStat_Nested();
			R_Sphere_SetupEntity();
			sphereEntity = entityNum;
			shadeParts += VK_Shade_PartCharge( VK_WS_SPHERE, partT0, partNested0 );
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		/* every family's first draw of a shader names itself: the poly transcript samples the list head only */
		if ( R_ProbeMarkOn() ) {
			static char	szDrawn[192][64];
			static int	s_nDrawn;
			static shader_t	*s_pCensusShader;
			static qboolean	s_bCensusSeen;
			unsigned	sState = ( shader->unfoggedStages[0] )
								 ? shader->unfoggedStages[0]->stateBits : 0u;

			if ( s_pCensusShader != shader ) {
				int	iSeen;

				s_pCensusShader = shader;
				s_bCensusSeen   = qfalse;
				for ( iSeen = 0; iSeen < s_nDrawn; iSeen++ ) {
					if ( !strcmp( szDrawn[iSeen], shader->name ) ) {
						s_bCensusSeen = qtrue;
						break;
					}
				}
				if ( !s_bCensusSeen && s_nDrawn < 192 ) {
					Q_strncpyz( szDrawn[s_nDrawn++], shader->name, sizeof( szDrawn[0] ) );
				}
			}

			if ( !s_bCensusSeen ) {
				float		rN, rF;
				image_t		*pTex0 = RB_Stage0Image( shader );
				image_t		*pOwn  = pTex0 ? VK_Image_SlotTenant( (int)pTex0->texnum ) : NULL;

				VK_Draw_DepthRangeLast( &rN, &rF );
				VK_Guard_Row( "DOCC:draw",
							  "f=%d fam=%d '%s' st0=0x%08x ent=%d dhack=%d "
							  "dtest=%d dwrite=%d dequal=%d blend=%d/%d range=%.3f..%.3f "
							  "tex0='%s' h=%d tenant='%s'",
							  RB_FrameCount(), *drawSurf->surface, shader->name,
							  sState, entityNum,
							  ( !bStaticModel && entityNum != ENTITYNUM_WORLD
								&& ( backEnd.refdef.entities[entityNum].e.renderfx & RF_DEPTHHACK ) )
								? 1 : 0,
							  ( sState & VKS_DEPTHTEST_DISABLE ) ? 0 : 1,
							  ( sState & VKS_DEPTHMASK_TRUE ) ? 1 : 0,
							  ( sState & VKS_DEPTHFUNC_EQUAL ) ? 1 : 0,
							  (int)( sState & VKS_SRCBLEND_BITS ),
							  (int)( ( sState & VKS_DSTBLEND_BITS ) >> 4 ),
							  rN, rF,
							  ( pTex0 && pTex0->imgName[0] ) ? pTex0->imgName : "(none)",
							  ( pTex0 ) ? (int)pTex0->texnum : -1,
							  ( pOwn && pOwn->imgName[0] ) ? pOwn->imgName
										: ( pOwn ? "(unnamed)" : "-" ) );
			}
		}

		if ( R_ProbeMarkOn() && *drawSurf->surface == SF_POLY ) {
			/* the sort word carries 11 bits of shader and MAX_SHADERS is 16384, so a past-2048 index wears another shader's stages */
			static int	s_keyRows;

			if ( s_keyRows < 32 || ( s_keyRows % 600 ) == 0 ) {
				const srfPoly_t	*pPoly = (const srfPoly_t *)drawSurf->surface;
				shader_t		*pAsk  = R_GetShaderByHandle( pPoly->hShader );

				if ( pAsk != shader ) {
					s_keyRows++;
					VK_Guard_Row( "DOCC:polykey",
								  "f=%d i=%d ask='%s' hsh=%d sidx=%d got='%s' gidx=%d "
								  "sort=0x%08x numshaders=%d",
								  RB_FrameCount(), i,
								  ( pAsk && pAsk->name[0] ) ? pAsk->name : "?",
								  (int)pPoly->hShader,
								  pAsk ? pAsk->sortedIndex : -1,
								  ( shader && shader->name[0] ) ? shader->name : "?",
								  shader ? shader->sortedIndex : -1,
								  drawSurf->sort, tr.numShaders );
				}
			}
		}

		if ( R_ProbeMarkOn() && *drawSurf->surface == SF_POLY ) {
			static int	s_polyFrame;
			static int	s_polyLastFrame = -1;
			static int	s_polyRows;
			static int	s_polyCount;
			static int	s_polyWindow;
			static int	s_polyAnomalyRows;

			if ( RB_FrameCount() != s_polyLastFrame ) {
				s_polyLastFrame = RB_FrameCount();
				s_polyFrame = 0;
			}
			/* the budget refills so a late decal still reaches the transcript */
			if ( RB_FrameCount() - s_polyWindow >= 1000 ) {
				s_polyWindow = RB_FrameCount();
				s_polyRows   = 0;
			}
			s_polyCount++;
			/* a mark with no depth test, depth write, or zero alpha can never show */
			if ( PROBE_Throttle( &s_polyAnomalyRows, 4, 600 ) ) {
				const srfPoly_t	*pAnom = (const srfPoly_t *)drawSurf->surface;
				unsigned		sAnom = ( shader->unfoggedStages[0] )
										? shader->unfoggedStages[0]->stateBits : 0u;
				int				aMin = 255, aMax = 0, vi;
				float			rN, rF;

				VK_Draw_DepthRangeLast( &rN, &rF );

				for ( vi = 0; vi < pAnom->numVerts; vi++ ) {
					if ( pAnom->verts[vi].modulate[3] < aMin ) {
						aMin = pAnom->verts[vi].modulate[3];
					}
					if ( pAnom->verts[vi].modulate[3] > aMax ) {
						aMax = pAnom->verts[vi].modulate[3];
					}
				}
				/* a world poly under the viewmodel's 0.3 cannot be occluded, so the draw names its range */
				if ( ( sAnom & VKS_DEPTHTEST_DISABLE ) || ( sAnom & VKS_DEPTHMASK_TRUE )
					 || aMax == 0 || rF < 0.999f ) {
					VK_Guard_Row( "DOCC:pipeline",
								  "'%s' sb=0x%08x dtest=%d dwrite=%d dequal=%d "
								  "blend=1 src=%d dst=%d alpha=%d..%d nverts=%d "
								  "range=%.3f..%.3f f=%d",
								  shader->name, sAnom,
								  ( sAnom & VKS_DEPTHTEST_DISABLE ) ? 0 : 1,
								  ( sAnom & VKS_DEPTHMASK_TRUE ) ? 1 : 0,
								  ( sAnom & VKS_DEPTHFUNC_EQUAL ) ? 1 : 0,
								  (int)( sAnom & VKS_SRCBLEND_BITS ),
								  (int)( ( sAnom & VKS_DSTBLEND_BITS ) >> 4 ),
								  aMin, aMax, pAnom->numVerts,
								  rN, rF, RB_FrameCount() );
				}
			}
			if ( s_polyRows <= PROBE_ROW_CAP && s_polyFrame < 4 && ( s_polyCount % 30 ) == 1 ) {
				char		szBefore[192];
				char		szAfter[96];
				shader_t	*pNb;
				int			iNb, iEnt, iFog, iDl, iLen;
				float		rN, rF;

				VK_Draw_DepthRangeLast( &rN, &rF );	/* the range in force at the draw, not the walk's belief */

				s_polyFrame++;
				s_polyRows++;

				szBefore[0] = 0;
				iLen = 0;
				for ( iNb = i - 6; iNb < i; iNb++ ) {
					if ( iNb < 0 || iNb >= numDrawSurfs || drawSurfs[iNb].surface == NULL ) {
						continue;
					}
					R_DecomposeSort( drawSurfs[iNb].sort, &iEnt, &pNb, &iFog, &iDl );
					iLen += Com_sprintf( szBefore + iLen, sizeof( szBefore ) - iLen, "%s%s|",
										 iNb == i ? ">" : "", pNb->name );
					if ( iLen >= (int)sizeof( szBefore ) - 48 ) {
						break;
					}
				}

				szAfter[0] = 0;
				iLen = 0;
				for ( iNb = i + 1; iNb <= i + 3 && iNb < numDrawSurfs; iNb++ ) {
					if ( drawSurfs[iNb].surface == NULL ) {
						continue;
					}
					R_DecomposeSort( drawSurfs[iNb].sort, &iEnt, &pNb, &iFog, &iDl );
					iLen += Com_sprintf( szAfter + iLen, sizeof( szAfter ) - iLen, "|%s", pNb->name );
					if ( iLen >= (int)sizeof( szAfter ) - 24 ) {
						break;
					}
				}

				VK_Guard_Row( "DOCC:order",
							  "n=%d '%s' sort=%u st0=0x%08x ent=%d range=%.3f..%.3f before=%s after=%s",
							  i, shader->name, drawSurf->sort,
							  ( shader->unfoggedStages[0] ) ? shader->unfoggedStages[0]->stateBits : 0u,
							  entityNum, rN, rF, szBefore, szAfter );
			}
		}

		VK_Shade_WindowClose( shadeT0, shadeNested0, shadeParts );

		{
			long long walkT0 = VK_WALK_T0();
			long long walkNest0 = VK_WALK_NEST0();

			VK_Shade_NoteBatchSurf();
			if ( !RB_VboStaticTrySurface( drawSurf->surface, *drawSurf->surface ) ) {
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			} else {
				RB_NoteFeedSurf( drawSurf->surface, 3 );
			}
			VK_WALK_ADD_NET( walkT0, walkNest0, VK_WP_GEN );
		}

		if ( *drawSurf->surface == SF_POLY ) {
			RB_ColorStreamProbe( shader, "poly", i );
		}
	}

	if (oldShader != NULL) {
		RB_EndSurface();
	}

	/* the sub-view's own numbers, and the marker's rect once the main view has
	   walked it: the pair is the whole question this row exists to answer */
	if ( R_ProbeMarkOn() ) {
		if ( backEnd.viewParms.isPortal ) {
			s_portalComp.draws = portalDraws;
			VK_Draw_DepthRangeLast( &s_portalComp.range[0], &s_portalComp.range[1] );
		} else if ( s_portalComp.subSeen && s_portalComp.markerSeen
					&& s_portalComp.frame == RB_FrameCount() ) {
			RB_PortalCompRow();
			s_portalComp.subSeen = qfalse;
			s_portalComp.markerSeen = qfalse;
		}
	}

	RB_LoadModelViewMatrix(  backEnd.viewParms.world.modelMatrix );
	/* the walk hands the next list the full range whatever its own mirror said */
	vkDepthRange (0, 1.0);

	/* the volumes' place in this view's walk, named before the overlay paints it */
	RB_Order_FrameRow();

	/* retail R_RenderView @0x0001F190 draws the pass only for a flag-free world refdef */
	if (!backEnd.refdef.rdflags) {
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
	int			i;
	drawSurf_t	*drawSurf;

    backEnd.currentEntity = &tr.worldEntity;
    backEnd.currentStaticModel = 0;

	r_spriteListOpen = 1;
	r_spriteWalkSurfs = numDrawSurfs;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.ori = backEnd.viewParms.world;

	oldShader = NULL;
    vkDepthRange( 0.0, 1.0 );	/* no RB_BeginDrawingView here, so the walk anchors the range it inherits */

    for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++) {
        if (!drawSurf->surface) continue;
        {
			refSprite_t *spr = (refSprite_t*)drawSurf->surface;

			if (spr->shaderNum < 0 || spr->shaderNum >= tr.numShaders) continue;
			shader = ( spr->shaderGen == tr.shaderGen )
				   ? spr->shader
				   : RB_ResolveStaleShader( "sprite", spr->shaderGen, spr->shaderNum );
			if ( shader == NULL ) {
				continue;	/* the record names a slot outside the live table */
			}
			/* a record whose index no longer names its own shader's slot.  The
			   draw above already took the pointer, so this is pure witness: it
			   names the frames where resolving the index would have worn another
			   shader's stages. */
			if ( shader->sortedIndex != spr->shaderNum ) {
				RB_NoteIndexDesync( "sprite", shader, spr->shaderNum );
			}
			if ((shader != oldShader || (shader->noMerge & 1)) && !shader->entityMergable)
			{
				if (oldShader) {
					RB_EndSurface();
				}

				RB_BeginSurface(shader, spr->fogNum);
				oldShader = shader;
			}

			RB_LoadModelViewMatrix( backEnd.ori.modelMatrix);

			/* retail's sprite walk carries no range of its own: RB_RenderSpriteSurfList
			   never calls glDepthRange, so an inherited renderfx bit must not shrink it */
			if ( R_ProbeMarkOn() ) {
				static int	s_sprRows;
				static int	s_sprWindow;
				static int	s_sprFrame = -1;
				static shader_t *s_sprCensus;
				static int	s_sprAnom;
				float		rN, rF;
				unsigned	sState = ( shader->unfoggedStages[0] )
								 ? shader->unfoggedStages[0]->stateBits : 0u;

				VK_Draw_DepthRangeLast( &rN, &rF );
				if ( RB_FrameCount() != s_sprFrame || s_sprCensus != shader ) {
					s_sprFrame  = RB_FrameCount();
					s_sprCensus = shader;
					if ( RB_FrameCount() - s_sprWindow >= 1000 ) {
						s_sprWindow = RB_FrameCount();
						s_sprRows   = 0;
					}
					if ( s_sprRows < 24 ) {
					shader_t	*pOwn  = RB_SpriteModelShader( spr );
					image_t		*pTex0 = RB_Stage0Image( shader );
					image_t		*pTen  = pTex0 ? VK_Image_SlotTenant( (int)pTex0->texnum ) : NULL;

					s_sprRows++;
					VK_Guard_Row( "DOCC:sprite",
								  "f=%d '%s' i=%d rgba=%u,%u,%u,%u dtest=%d dwrite=%d "
								  "dfunc_eq=%d blend=%d/%d range=%.3f..%.3f rf=0x%x "
								  "num=%d own='%s' tex0='%s' h=%d tenant='%s'",
								  RB_FrameCount(), shader->name, i,
								  (unsigned)spr->shaderRGBA[0], (unsigned)spr->shaderRGBA[1],
								  (unsigned)spr->shaderRGBA[2], (unsigned)spr->shaderRGBA[3],
								  ( sState & VKS_DEPTHTEST_DISABLE ) ? 0 : 1,
								  ( sState & VKS_DEPTHMASK_TRUE ) ? 1 : 0,
								  ( sState & VKS_DEPTHFUNC_EQUAL ) ? 1 : 0,
								  (int)( sState & VKS_SRCBLEND_BITS ),
								  (int)( ( sState & VKS_DSTBLEND_BITS ) >> 4 ),
								  rN, rF, spr->renderfx,
								  spr->shaderNum,
								  ( pOwn && pOwn->name[0] ) ? pOwn->name : "-",
								  ( pTex0 && pTex0->imgName[0] ) ? pTex0->imgName : "(none)",
								  ( pTex0 ) ? (int)pTex0->texnum : -1,
								  ( pTen && pTen->imgName[0] ) ? pTen->imgName
										: ( pTen ? "(unnamed)" : "-" ) );
				}
				}
				if ( rN != 0.0f || rF != 1.0f ) {
					s_sprAnom++;
					if ( s_sprAnom <= 8 || ( s_sprAnom % 600 ) == 0 ) {
						VK_Guard_Row( "DOCC:sprite",
									  "range-arm f=%d '%s' i=%d range=%.3f..%.3f rf=0x%x",
									  RB_FrameCount(), shader->name, i, rN, rF, spr->renderfx );
					}
				}
			}

			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;

			rb_surfaceTable[*drawSurf->surface](drawSurf->surface);

			RB_ColorStreamProbe( shader, "sprite", i );
		}
	}

	if (oldShader) {
		RB_EndSurface();
	}

	r_spriteListOpen = 0;
	r_spriteWalkSurfs = 0;

    RB_LoadModelViewMatrix( backEnd.viewParms.world.modelMatrix);
	vkDepthRange(0.0, 1.0);
}

/* Retail only *enables* fog in RB_GLFogStageIteratorFunc @0x00034830, around one surface's own passes; R_SetupFrustum just sets the colour and the end distance.  Arming
   it for the whole view fogged every blended sprite stage, whose fog pass FinishShader drops, and filled the quad's rectangle. */
void RB_SetupFog(void) {
	vec4_t	vFogColor;

	vkState.externalSetState &= ~VKS_FOG;

	if (!backEnd.viewParms.farplane_distance
		|| r_farplane_nofog->integer
		|| !backEnd.viewParms.farplane_fog) {
		return;
	}

	vFogColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
	vFogColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
	vFogColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
	vFogColor[3] = 1.0;
	VK_SetFogColor(vFogColor);
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

	if ( R_ProbeMarkOn() ) {
		static int	s_walkRows;

		s_walkRows++;
		if ( s_walkRows <= 24 || ( s_walkRows % 300 ) == 0 ) {
			int	menu = 0, i;

			for ( i = 0; i < cmd->numDrawSurfs; i++ ) {
				const char *n = cmd->drawSurfs[i].shader ? cmd->drawSurfs[i].shader->name : "";

				if ( !Q_strncmp( n, "menu/", 5 ) || strstr( n, "lcars" ) ) {
					menu++;
				}
			}
			VK_Guard_Row( "VK:surfcmd",
						  "site=walk copied=%d menuSurfs=%d live=%d in2D=%d f=%d",
						  cmd->numDrawSurfs, menu, tr.refdef.numDrawSurfs,
						  backEnd.in2D ? 1 : 0, RB_FrameCount() );
		}
	}

	if ( R_ProbeMarkOn() && cmd->numDrawSurfs != tr.refdef.numDrawSurfs ) {
		static int	s_walkRows;

		s_walkRows++;
		if ( s_walkRows <= 8 || ( s_walkRows % 300 ) == 0 ) {
			VK_Guard_Row( "VK:surfcmd",
						  "site=walk copied=%d live=%d f=%d",
						  cmd->numDrawSurfs, tr.refdef.numDrawSurfs, RB_FrameCount() );
		}
	}

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
	RB_DLightFeedCensus();
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	BE_RootEnd( BE_FAM_DS );

	return (const void *)((const char *)cmd + cmd->totalBytes);
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

    return (const void*)((const char *)cmd + cmd->totalBytes);
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
	/* retail never touches the blend for RF_FORCE_ALPHA: the fade rides the
	   entity's own alpha in the colour (R_AddSkelSurfaces), so the shader's
	   blendFunc survives -- an additive flash stays additive.  Rewriting the
	   factors here turned every forced fade into an alpha-blend box. */
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

	/* This backend's 2D dispatch is RB_StageIterator2D, which never reaches
	   RB_StageIteratorGeneric, so the clock row is taken here as well: a
	   scrolling 2D shader (a wall panel's screen, a HUD effect) animates off
	   the same tess.shaderTime. */
	tess.shaderTime = backEnd.refdef.floatTime;
	VK_Shade_NoteShaderTime( "2d" );

	/* the quad's own ST, before any stage's chain lands in it */
	VK_Stage2DAuthorST();

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

		/* the stage's tcMods land in the vertex ST: the tess shader has no tcMod arm */
		VK_Stage2DResolveTexCoords( pStage );

		/* the 2D walk was the one iterator that never reported into the frame's
		   stage census, so a menu-heavy frame under-counted its own stages */
		VK_Shade_NoteStage( tess.shader );
		RB_StagePass( pStage, colors );
	}
}
