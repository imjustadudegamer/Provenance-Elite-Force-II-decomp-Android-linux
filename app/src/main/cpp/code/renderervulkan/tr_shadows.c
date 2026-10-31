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

/* The frame's own volume census, per view: a volume can only arm the plane of
   the view whose walk tessellated it, so the readback row has to name which
   view's volumes it counted. */
#define VK_SHADOW_VIEWS	8

typedef struct {
	int	view;			/* the tr.viewCount the volumes were tessellated in */
	int	volumes;
	int	quads;
} vkShadowView_t;

static	vkShadowView_t	s_viewCensus[VK_SHADOW_VIEWS];
static	int			s_viewCensusCount;
static	int			s_lastShadowView = -1;
static	int			s_frameQuads;
static	int			s_frameVolumes;
static	int			s_frameOverlayVerts;
static	int			s_counterFrame = -1;

static void VK_Shadow_NoteFrame( void )
{
	if ( RB_FrameCount() != s_counterFrame ) {
		s_counterFrame = RB_FrameCount();
		s_frameQuads = 0;
		s_frameVolumes = 0;
		s_frameOverlayVerts = 0;
		s_viewCensusCount = 0;
		s_lastShadowView = -1;
	}
}

/* one slot per view, in the order the views first drew a volume */
static vkShadowView_t *VK_Shadow_ViewSlot( void )
{
	int i;

	for ( i = 0; i < s_viewCensusCount; i++ ) {
		if ( s_viewCensus[i].view == tr.viewCount ) {
			return &s_viewCensus[i];
		}
	}
	if ( s_viewCensusCount >= VK_SHADOW_VIEWS ) {
		return &s_viewCensus[VK_SHADOW_VIEWS - 1];
	}

	s_viewCensus[s_viewCensusCount].view = tr.viewCount;
	s_viewCensus[s_viewCensusCount].volumes = 0;
	s_viewCensus[s_viewCensusCount].quads = 0;

	return &s_viewCensus[s_viewCensusCount++];
}

static void VK_Shadow_NoteVolume( int quads )
{
	vkShadowView_t *slot;

	VK_Shadow_NoteFrame();
	slot = VK_Shadow_ViewSlot();
	if ( slot ) {
		slot->volumes++;
		slot->quads += quads;
		s_lastShadowView = slot->view;
	}
	s_frameVolumes++;
	s_frameQuads += quads;
}

void VK_Shadow_FrameCounters( int *volumes, int *quads, int *overlayVerts )
{
	if ( volumes ) {
		*volumes = s_frameVolumes;
	}
	if ( quads ) {
		*quads = s_frameQuads;
	}
	if ( overlayVerts ) {
		*overlayVerts = s_frameOverlayVerts;
	}
}

/* the per-view split as text: "v101=2/2500,v103=1/1200" pairs a plane's texels
   with the view whose walk could have put them there */
const char *VK_Shadow_ViewCensusText( void )
{
	static char	s_text[VK_SHADOW_VIEWS * 32];
	int			i, len = 0;

	s_text[0] = 0;
	for ( i = 0; i < s_viewCensusCount; i++ ) {
		int n = Com_sprintf( s_text + len, sizeof( s_text ) - len, "%sv%d=%d/%d",
							 i ? "," : "", s_viewCensus[i].view,
							 s_viewCensus[i].volumes, s_viewCensus[i].quads );

		if ( n < 0 || (size_t)n >= sizeof( s_text ) - len ) {
			break;
		}
		len += n;
	}

	return s_text[0] ? s_text : "-";
}

int VK_Shadow_LastView( void )
{
	return s_lastShadowView;
}

void R_ShutdownShadows( void ) {
	Com_Memset( numEdgeDefs, 0, sizeof( numEdgeDefs ) );
	Com_Memset( facing, 0, sizeof( facing ) );
}

/* whether the walk handed this entity the view alone instead of its own transform:
   the one state in which a model-space volume has no space to live in */
static qboolean VK_Shadow_OriIsView( void )
{
	const float	*a = backEnd.ori.modelMatrix;
	const float	*b = backEnd.viewParms.world.modelMatrix;
	int			i;

	for ( i = 0; i < 16; i++ ) {
		if ( a[i] != b[i] ) {
			return qfalse;
		}
	}

	return qtrue;
}

/* the row's slot: the index the sort word named, so a slot outside the frame's
   entity list reads as what it is */
int VK_Shadow_EntitySlot( void )
{
	if ( !backEnd.currentEntity || backEnd.refdef.num_entities <= 0
		 || backEnd.currentEntity < backEnd.refdef.entities
		 || backEnd.currentEntity >= backEnd.refdef.entities + backEnd.refdef.num_entities ) {
		return -1;
	}

	return (int)( backEnd.currentEntity - backEnd.refdef.entities );
}void R_AddEdgeDef( int i1, int i2, int localFacing ) {
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
	int		runsBefore = 0, vertsBefore = 0;
	qboolean	armTest = qfalse;
	int			armFunc = 0, armRef = 0, armFail = 0, armZFail = 0, armPass = 0, armCull = 0;
	unsigned	armMask = 0, armColor = 0;
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

	/* The volume's vertices are the caster's own model space, so it only says anything under that caster's model transform.
	   R_RotateForEntity hands back the view alone for a slot whose reType is not RT_MODEL (tr_main.c:262-266), and a volume extruded
	   under it wraps the caster's model space around the eye and arms the whole viewport.  No shadow draw surf is registered for such a
	   slot, so this pair is the draw surf and the entity disagreeing, and it refuses the volume rather than painting the frame. */
	if ( !backEnd.currentEntity || backEnd.currentEntity->e.reType != RT_MODEL || VK_Shadow_OriIsView() ) {
#ifndef PROBE_BUILD_SILENT
		if ( R_ProbeMarkOn() ) {
			static int	s_rows;

			s_rows++;
			if ( s_rows <= 16 || ( s_rows % 600 ) == 0 ) {
				VK_Guard_Row( "VK:shadow",
							  "site=refuse reType=%d slot=%d nent=%d ent=%d vw=%.1f,%.1f,%.1f f=%d",
							  backEnd.currentEntity ? (int)backEnd.currentEntity->e.reType : -1,
							  VK_Shadow_EntitySlot(),
							  backEnd.refdef.num_entities,
							  backEnd.currentEntity ? (int)backEnd.currentEntity->e.entityNumber : -1,
							  backEnd.viewParms.world.modelMatrix[12],
							  backEnd.viewParms.world.modelMatrix[13],
							  backEnd.viewParms.world.modelMatrix[14],
							  RB_FrameCount() );
			}
		}
#endif
		return;
	}

	VK_BindUnit0( tr.whiteImage );
	vkEnable( VK_CULL_FACE );
	vkState.faceCulling = -1;
	VK_State( VKS_SRCBLEND_ONE | VKS_DSTBLEND_ZERO );
	RB_ImmColor3f( 0.2f, 0.2f, 0.2f );

#ifndef PROBE_BUILD_SILENT
	if ( r_shadowVolume && r_shadowVolume->integer ) {
		/* the picture the numbers cannot give: the volume with colour writes on, no cull and no stencil test, so a wrong extrusion,
		   a wrong matrix or a degenerate quad shows as the shape it is instead of as a count in a row */
		VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );
		vkColorMask( VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE );
		RB_ImmColor3f( 0.0f, 1.0f, 1.0f );
		vkDisable( VK_CULL_FACE );
		vkState.faceCulling = -1;

		s_edgeQuads = 0;
		R_RenderShadowEdges();

		VK_Shadow_NoteVolume( s_edgeQuads );

		vkEnable( VK_CULL_FACE );
		vkState.faceCulling = -1;
		RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		return;
	}
#endif

	vkColorMask( VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE );

	VK_StencilState( qtrue, VK_ALWAYS, 1, 255 );

	s_edgeQuads = 0;
	VK_Imm_ShadowCommits( &runsBefore, &vertsBefore );
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


	/* the arm as the passes left it, sampled before the teardown below: this is
	   the pair the row prints, and the reason the old row printed a literal */
	VK_Shade_ShadowState( &armTest, &armFunc, &armRef, &armMask, &armFail,
						  &armZFail, &armPass, &armCull, &armColor );

	vkColorMask( VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE );
	/* The volume's arm is state only this pass wants: the reference's GL_Cull
	   re-issues the face at the next state block, and its stencil test is a
	   context call the next block overwrites, so both fall back here rather
	   than leak into the draws that follow the shadow batch. */
	vkEnable( VK_CULL_FACE );
	vkState.faceCulling = -1;
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );

	VK_Shadow_NoteVolume( s_edgeQuads );

	if ( R_ProbeMarkOn() ) {
		static int	s_rows;

		s_rows++;
		if ( s_rows <= 16 || ( s_rows % 600 ) == 0 ) {
			/* the arm this call left on the volumes, read back rather than restated:
			   the stencil fields and both passes' built pipeline keys are the only
			   witnesses of "counted the face retail keeps" */
			vkPipelineKey_t	kInc, kDec;
			char		incText[160], decText[160];
			float		rN, rF;
			int			commitRuns, commitVerts;
			const float	*pushM = RB_ImmPushedMatrix();
			const float	*v0 = tess.xyz[0];
			const float	*ori = backEnd.ori.modelMatrix;
			const float	*eorg = backEnd.currentEntity ? backEnd.currentEntity->e.origin : vec3_origin;
			const float	*vorg = backEnd.viewParms.ori.origin;
			float	stageM[16];
			float	wpush, wstage;
			float	eye;

			/* w of the volume's first vertex through the matrix the run pushed and
			   through the one the caster's own surfaces were tessellated with: two
			   numbers that have to agree, or the volume sits in another space */
			VK_Mat4Mul( stageM, RB_GetProjectionMatrix(), RB_GetModelViewMatrix() );
			wpush  = pushM[3] * v0[0] + pushM[7] * v0[1] + pushM[11] * v0[2] + pushM[15];
			wstage = stageM[3] * v0[0] + stageM[7] * v0[1] + stageM[11] * v0[2] + stageM[15];
			eye = Distance( eorg, vorg );

			VK_Draw_DepthRangeLast( &rN, &rF );
			/* VK_Imm_KeyGet(0) is the run opened last, i.e. the DECR pass */
			incText[0] = decText[0] = 0;
			if ( VK_Imm_KeyGet( 1, &kInc ) ) {
				VK_Draw_KeyText( &kInc, incText, sizeof( incText ) );
			}
			if ( VK_Imm_KeyGet( 0, &kDec ) ) {
				VK_Draw_KeyText( &kDec, decText, sizeof( decText ) );
			}
			{
				int	runsAfter = 0, vertsAfter = 0;

				VK_Imm_ShadowCommits( &runsAfter, &vertsAfter );
				commitRuns  = runsAfter - runsBefore;
				commitVerts = vertsAfter - vertsBefore;
			}

			/* pv/sky/vc/vp name the view this volume belongs to, and eye its caster's
			   distance from the camera: a plane armed over the whole viewport is a
			   caster at the eye only if this row's eye says so */
			VK_Guard_Row( "VK:shadow",
						  "site=tess ent=%d lightdir=%.3f,%.3f,%.3f verts=%d idx=%d facing=%d edges=%d "
						  "quarter=%.3f stencil=%d func=%d ref=%d mask=%u ops=%d,%d,%d cull=%d "
						  "range=%.3f..%.3f commit=%d,%d inc[%s] dec[%s] "
						  "mv=%.1f,%.1f,%.1f wpush=%.1f wstage=%.1f "
						  "pv=%d sky=%d vc=%d vp=%d,%d,%d,%d vorg=%.1f,%.1f,%.1f eorg=%.1f,%.1f,%.1f eye=%.1f "
						  "reType=%d slot=%d nent=%d f=%d",
						  ( backEnd.currentEntity && backEnd.currentEntity->e.entityNumber >= 0 )
							? backEnd.currentEntity->e.entityNumber : -1,
						  lightDir[0], lightDir[1], lightDir[2],
						  tess.numVertexes, tess.numIndexes, numFacingTris, s_edgeQuads,
						  s_firstQuadArea, armTest ? 1 : 0,
						  armFunc, armRef, (unsigned)armMask,
						  armFail, armZFail, armPass, armCull,
						  rN, rF, commitRuns, commitVerts, incText, decText,
						  ori[12], ori[13], ori[14], wpush, wstage,
						  backEnd.viewParms.isPortal ? 1 : 0,
						  backEnd.viewParms.isPortalSky ? 1 : 0,
						  tr.viewCount,
						  backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
						  backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight,
						  vorg[0], vorg[1], vorg[2],
						  eorg[0], eorg[1], eorg[2], eye,
						  (int)backEnd.currentEntity->e.reType, VK_Shadow_EntitySlot(),
						  backEnd.refdef.num_entities, RB_FrameCount() );
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
#ifndef PROBE_BUILD_SILENT
	if ( r_shadowVolume && r_shadowVolume->integer ) {
		return;	/* the visualisation run is the picture; this overlay would multiply it down */
	}
#endif
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

	VK_Shadow_NoteFrame();
	s_frameOverlayVerts += ringAfter - ringBefore;

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
			const float	*m = RB_ImmPushedMatrix();
			float		clip[4];
			int			r, c, inside;
			float		ndc[3];

			VK_Shade_ShadowState( &stTest, &stFunc, &stRef, &stMask, &stFail,
								  &stZFail, &stPass, &stCull, &stColor );

			/* the quad's centre through the matrix the run pushed: a run whose own centre lands outside the clip volume paints
			   nothing, which reads exactly like an empty stencil plane */
			{
				const float	centre[4] = { 0.0f, 0.0f, -10.0f, 1.0f };

				for ( r = 0; r < 4; r++ ) {
					clip[r] = 0.0f;
					for ( c = 0; c < 4; c++ ) {
						clip[r] += m[c * 4 + r] * centre[c];
					}
				}
			}
			for ( r = 0; r < 3; r++ ) {
				ndc[r] = ( clip[3] != 0.0f ) ? clip[r] / clip[3] : 0.0f;
			}
			inside = ( clip[3] > 0.0f && ndc[0] >= -1.0f && ndc[0] <= 1.0f
					   && ndc[1] >= -1.0f && ndc[1] <= 1.0f
					   && ndc[2] >= 0.0f && ndc[2] <= 1.0f ) ? 1 : 0;

			VK_Guard_Row( "VK:shadow",
						  "site=finish verts=%d topo=%d stencil=%d func=%d ref=%d "
						  "ops=%d,%d,%d cull=%d cmask=0x%x clip=%.3f,%.3f,%.3f in=%d f=%d",
						  ringAfter - ringBefore, VK_IMM_TRIANGLE_FAN,
						  stTest ? 1 : 0, stFunc, stRef, stFail, stZFail, stPass,
						  stCull, stColor, ndc[0], ndc[1], ndc[2], inside,
						  RB_FrameCount() );
		}
	}

	RB_ImmColor4f( 1, 1, 1, 1 );
	VK_StencilState( qfalse, VK_ALWAYS, 0, 255 );
	/* the overlay drew cull-free; the next state block owns the face again */
	vkEnable( VK_CULL_FACE );
	vkState.faceCulling = -1;
}
