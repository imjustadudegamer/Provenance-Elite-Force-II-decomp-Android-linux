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
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
#include "tr_local.h"
#include "tr_vbo.h"
#include "tr_program.h"



typedef struct {
	int		i2;
	int		facing;
} edgeDef_t;

#define	MAX_EDGE_DEFS	32

static	edgeDef_t	edgeDefs[SHADER_MAX_VERTEXES][MAX_EDGE_DEFS];
static	int			numEdgeDefs[SHADER_MAX_VERTEXES];
static	int			facing[SHADER_MAX_INDEXES/3];
/* the silhouette quads the last R_RenderShadowEdges call emitted: an empty
   volume and a volume with nowhere to land read the same from outside */
static	int			s_edgeQuads;
/* the facing cross-product area of the first quad a volume emitted: a zero
   lightDir duplicates every vertex, and this is the number that says so */
static	float		s_firstQuadArea;

void R_ShutdownShadows( void ) {
	Com_Memset( numEdgeDefs, 0, sizeof( numEdgeDefs ) );
	Com_Memset( facing, 0, sizeof( facing ) );
}

void R_AddEdgeDef( int i1, int i2, int localFacing ) {
	int		c;

	c = numEdgeDefs[ i1 ];
	if ( c == MAX_EDGE_DEFS ) {
		return;		
	}
	edgeDefs[ i1 ][ c ].i2 = i2;
	edgeDefs[ i1 ][ c ].facing = localFacing;

	numEdgeDefs[ i1 ]++;
}

void R_RenderShadowEdges( void ) {
	int		i;

	VK_BeginShadow();

#if 0
	int		numTris;

	numTris = tess.numIndexes / 3;

	for ( i = 0 ; i < numTris ; i++ ) {
		int		i1, i2, i3;

		if ( !facing[i] ) {
			continue;
		}

		i1 = tess.indexes[ i*3 + 0 ];
		i2 = tess.indexes[ i*3 + 1 ];
		i3 = tess.indexes[ i*3 + 2 ];

		RB_ImmBegin( VK_TRIANGLE_STRIP );
		RB_ImmVertex3fv( tess.xyz[ i1 ] );
		RB_ImmVertex3fv( tess.xyz[ i1 + tess.numVertexes ] );
		RB_ImmVertex3fv( tess.xyz[ i2 ] );
		RB_ImmVertex3fv( tess.xyz[ i2 + tess.numVertexes ] );
		RB_ImmVertex3fv( tess.xyz[ i3 ] );
		RB_ImmVertex3fv( tess.xyz[ i3 + tess.numVertexes ] );
		RB_ImmVertex3fv( tess.xyz[ i1 ] );
		RB_ImmVertex3fv( tess.xyz[ i1 + tess.numVertexes ] );
		RB_ImmEnd();
	}
#else
	int		c, c2;
	int		j, k;
	int		i2;
	int		hit[2];

	/* ONE run for the whole volume.  RB_ImmBegin restarts the run's vertex
	   window, so a begin per edge left only the last silhouette quad in the
	   draw; a triangle list keeps the independent quads independent without
	   one draw call per edge. */
	RB_ImmBegin( VK_TRIANGLES );

	for ( i = 0 ; i < tess.numVertexes ; i++ ) {
		c = numEdgeDefs[ i ];
		for ( j = 0 ; j < c ; j++ ) {
			if ( !edgeDefs[ i ][ j ].facing ) {
				continue;
			}

			hit[0] = 0;
			hit[1] = 0;

			i2 = edgeDefs[ i ][ j ].i2;
			c2 = numEdgeDefs[ i2 ];
			for ( k = 0 ; k < c2 ; k++ ) {
				if ( edgeDefs[ i2 ][ k ].i2 == i ) {
					hit[ edgeDefs[ i2 ][ k ].facing ]++;
				}
			}

			if ( hit[ 1 ] == 0 ) {
				BE_Bump( BE_FAM_DS, BE_DSC_edges, 1 );
				if ( !s_edgeQuads ) {
					vec3_t	d1, d2, n;
					float	area;

					VectorSubtract( tess.xyz[ i + tess.numVertexes ], tess.xyz[ i ], d1 );
					VectorSubtract( tess.xyz[ i2 ], tess.xyz[ i ], d2 );
					CrossProduct( d1, d2, n );
					area = VectorLength( n );
					s_firstQuadArea = area;
				}
				/* the strip's own two triangles, written as a list so all edges
				   share the one run: (i, i+N, i2) and (i2, i+N, i2+N).  A strip
				   alternates its winding, so the second triangle takes the first
				   two vertices swapped; writing it in ascending order instead
				   turns one half of every quad against the other. */
				RB_ImmVertex3fv( tess.xyz[ i ] );
				RB_ImmVertex3fv( tess.xyz[ i + tess.numVertexes ] );
				RB_ImmVertex3fv( tess.xyz[ i2 ] );
				RB_ImmVertex3fv( tess.xyz[ i2 ] );
				RB_ImmVertex3fv( tess.xyz[ i + tess.numVertexes ] );
				RB_ImmVertex3fv( tess.xyz[ i2 + tess.numVertexes ] );
				s_edgeQuads++;
			}
		}
	}
#endif

	RB_ImmFlush();
}


void RB_ShadowTessEnd( void ) {
	int		i;
	int		numTris;
	int		shadowBuildT0;
	int		numFacingTris = 0;
	vec3_t	lightDir;

	if ( tess.numVertexes >= SHADER_MAX_VERTEXES / 2 ) {
		return;
	}

	if ( vkConfig.stencilBits < 4 ) {
		return;
	}

	VectorCopy( backEnd.currentEntity->lightDir, lightDir );

	/* R_Sphere_SetupEntity builds this direction from a SUM of six weighted
	   basis vectors, so a room lit from opposite sides cancels it; the
	   extrusion would then duplicate every vertex and the stencil would stay
	   zero while the volume still reaches the draw list.  A degraded direction
	   for the shadow only: the entity's strongest sphere light, else retail's
	   own entity-lighting fallback axis.  A direction that is present is used
	   verbatim, exactly as retail does. */
	if ( VectorLength( lightDir ) == 0.0f ) {
		float	best = 0.0f;
		vec3_t	delta;

		for ( i = 0; i < tr.numSLights; i++ ) {
			float	w;

			VectorSubtract( tr.sLights[i].origin, backEnd.currentEntity->e.origin, delta );
			w = Q_max( Q_max( tr.sLights[i].color[0], tr.sLights[i].color[1] ),
					   tr.sLights[i].color[2] );
			if ( w > best && VectorNormalize( delta ) > 0.0f ) {
				best = w;
				VectorCopy( delta, lightDir );
			}
		}
		if ( best == 0.0f ) {
			VectorSet( lightDir, 0.0f, 0.0f, 1.0f );
		}
	}

	for ( i = 0 ; i < tess.numVertexes ; i++ ) {
		VectorMA( tess.xyz[i], -512, lightDir, tess.xyz[i+tess.numVertexes] );
	}

	Com_Memset( numEdgeDefs, 0, 4 * tess.numVertexes );

	numTris = tess.numIndexes / 3;
	shadowBuildT0 = BE_On() ? BE_Microseconds() : 0;
	for ( i = 0 ; i < numTris ; i++ ) {
		int		i1, i2, i3;
		vec3_t	d1, d2, normal;
		float	*v1, *v2, *v3;
		float	d;

		i1 = tess.indexes[ i*3 + 0 ];
		i2 = tess.indexes[ i*3 + 1 ];
		i3 = tess.indexes[ i*3 + 2 ];

		v1 = tess.xyz[ i1 ];
		v2 = tess.xyz[ i2 ];
		v3 = tess.xyz[ i3 ];

		VectorSubtract( v2, v1, d1 );
		VectorSubtract( v3, v1, d2 );
		CrossProduct( d1, d2, normal );

		d = DotProduct( normal, lightDir );
		if ( d > 0 ) {
			facing[ i ] = 1;
			numFacingTris++;
		} else {
			facing[ i ] = 0;
		}

		R_AddEdgeDef( i1, i2, facing[ i ] );
		R_AddEdgeDef( i2, i3, facing[ i ] );
		R_AddEdgeDef( i3, i1, facing[ i ] );
	}

	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;	/* no world, no shadow volume, nothing may arm the stencil */
	}

	VK_BindUnit0( tr.whiteImage );
	vkEnable( VK_CULL_FACE );
	vkState.faceCulling = -1;
	VK_State( VKS_SRCBLEND_ONE | VKS_DSTBLEND_ZERO );
	RB_ImmColor3f( 0.2f, 0.2f, 0.2f );

	vkColorMask( VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE );

	VK_StencilState( qtrue, VK_ALWAYS, 1, 255 );

	s_edgeQuads = 0;
	if ( backEnd.viewParms.isMirror ) {
		vkCullFace( VK_FRONT );
		vkStencilOp( VK_KEEP, VK_KEEP, VK_INCR );

		R_RenderShadowEdges();

		vkCullFace( VK_BACK );
		vkStencilOp( VK_KEEP, VK_KEEP, VK_DECR );

		R_RenderShadowEdges();
	} else {
		vkCullFace( VK_BACK );
		vkStencilOp( VK_KEEP, VK_KEEP, VK_INCR );

		R_RenderShadowEdges();

		vkCullFace( VK_FRONT );
		vkStencilOp( VK_KEEP, VK_KEEP, VK_DECR );

		R_RenderShadowEdges();
	}


	vkColorMask( VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE );
	/* The volume's arm is state only this pass wants: the reference's GL_Cull
	   re-issues the face at the next state block, and its stencil test is a
	   context call the next block overwrites, so both fall back here rather
	   than leak into the draws that follow the shadow batch. */
	vkEnable( VK_CULL_FACE );
	vkState.faceCulling = -1;
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );

	if ( R_ProbeMarkOn() ) {
		static int	s_rows;

		s_rows++;
		if ( s_rows <= 16 || ( s_rows % 600 ) == 0 ) {
			/* the face each of the two passes culls, resolved through the renderer's
			   own mapping: a volume that counts the wrong side reads as an empty
			   plane with the geometry intact, and only these two numbers say so */
			int	first  = backEnd.viewParms.isMirror ? VK_FRONT : VK_BACK;
			int	second = backEnd.viewParms.isMirror ? VK_BACK : VK_FRONT;

			VK_Guard_Row( "VK:shadow",
						  "site=tess ent=%d lightdir=%.3f,%.3f,%.3f verts=%d idx=%d facing=%d edges=%d "
						  "quarter=%.3f stencil=%d vcull=%d,%d f=%d",
						  ( backEnd.currentEntity && backEnd.currentEntity->e.entityNumber >= 0 )
							? backEnd.currentEntity->e.entityNumber : -1,
						  lightDir[0], lightDir[1], lightDir[2],
						  tess.numVertexes, tess.numIndexes, numFacingTris, s_edgeQuads,
						  s_firstQuadArea, 1,
						  (int)VK_Draw_CullModeFor( first, CT_TWO_SIDED ),
						  (int)VK_Draw_CullModeFor( second, CT_TWO_SIDED ),
						  RB_FrameCount() );
		}
	}

	if ( BE_On() ) {
		BE_Sub( BE_FAM_DS, BE_DSZ_sub_shadow, BE_Microseconds() - shadowBuildT0 );
	}
}

void RB_ShadowFinish( void ) {
	int		ringBefore, ringAfter;

	if ( r_shadows->integer != 2 ) {
		return;
	}
	if ( vkConfig.stencilBits < 4 ) {
		return;
	}
	/* No shadow volume wrote the plane here, so arming left a stale comparator. */
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}
	VK_StencilState( qtrue, VK_NOTEQUAL, 0, 255 );

	RB_ClipPlane_Disable();
	vkDisable (VK_CULL_FACE);
	vkState.faceCulling = -1;

	VK_BindUnit0( tr.whiteImage );

	RB_LoadIdentityModelView();

	RB_ImmColor3f( 0.6f, 0.6f, 0.6f );
	VK_State( VKS_DEPTHMASK_TRUE | VKS_SRCBLEND_DST_COLOR | VKS_DSTBLEND_ZERO );

	VK_BeginWire();



	ringBefore = RB_ImmStreamVerts();
	RB_ImmBegin( VK_TRIANGLE_FAN );
	RB_ImmVertex3f( -100, 100, -10 );
	RB_ImmVertex3f( 100, 100, -10 );
	RB_ImmVertex3f( 100, -100, -10 );
	RB_ImmVertex3f( -100, -100, -10 );
	RB_ImmEnd();
	RB_ImmFlush();
	ringAfter = RB_ImmStreamVerts();

	/* the overlay is the last draw of the pass and the only one that paints
	   the volume: the vertices it committed, the topology it took and the state
	   its key carries are what tell "never drawn" from "drawn over an empty
	   stencil" on a boot where nothing visibly darkened */
	if ( R_ProbeMarkOn() ) {
		static int	s_rows;

		s_rows++;
		if ( s_rows <= 16 || ( s_rows % 600 ) == 0 ) {
			qboolean	stTest;
			int			stFunc, stRef, stFail, stZFail, stPass, stCull;
			unsigned	stMask, stColor;

			VK_Shade_ShadowState( &stTest, &stFunc, &stRef, &stMask, &stFail,
								  &stZFail, &stPass, &stCull, &stColor );
			VK_Guard_Row( "VK:shadow",
						  "site=finish verts=%d topo=%d stencil=%d func=%d ref=%d "
						  "ops=%d,%d,%d cull=%d cmask=0x%x f=%d",
						  ringAfter - ringBefore, VK_IMM_TRIANGLE_FAN,
						  stTest ? 1 : 0, stFunc, stRef, stFail, stZFail, stPass,
						  stCull, stColor, RB_FrameCount() );
		}
	}

	RB_ImmColor4f( 1, 1, 1, 1 );
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );
	/* the overlay drew cull-free; the next state block owns the face again */
	vkEnable( VK_CULL_FACE );
	vkState.faceCulling = -1;
}
