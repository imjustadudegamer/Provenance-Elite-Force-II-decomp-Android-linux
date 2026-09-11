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
 * vk_m3_surface.c -- the 3D stage iterators' reserved seats.
 *
 * The shader compiler hands every shader an `optimalStageIteratorFunc`, so the
 * data model names the 3D iterators before they draw anything.  These bodies
 * are the guard, not a silent stub: reaching one prints a [VK:guard] row that
 * names the site.  M3 replaces each body with its real walk; nothing in the
 * M2 boot path reaches them, because RE_RenderScene never builds a 3D scene
 * in this milestone and logs its own [VK:stub] row when called.
 */

#include "tr_local.h"

/* Named in the header: the back end reaches it for a shader that carries 3D
   terms the 2D walk does not evaluate. */
void VK_M3_Touched( const char *what )
{
	static int rows;

	if ( rows < 8 || ( rows % 600 ) == 0 ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, what, VK_INCOMPLETE );
	}
	rows++;
}

void RB_StageIteratorSky( void )
{
	VK_M3_Touched( "RB_StageIteratorSky (3D surface, M3)" );
}

void RB_StageIteratorVertexLitTextureUnfogged( void )
{
	VK_M3_Touched( "RB_StageIteratorVertexLitTextureUnfogged (3D surface, M3)" );
}

void RB_StageIteratorLightmappedMultitextureUnfogged( void )
{
	VK_M3_Touched( "RB_StageIteratorLightmappedMultitextureUnfogged (3D surface, M3)" );
}

void RB_GLFogStageIteratorFunc( void )
{
	VK_M3_Touched( "RB_GLFogStageIteratorFunc (3D fog pass, M3)" );
}

void RB_ShadowTessEnd( void )
{
	VK_M3_Touched( "RB_ShadowTessEnd (stencil shadows, M3)" );
}

void RB_ShadowFinish( void )
{
	VK_M3_Touched( "RB_ShadowFinish (stencil shadows, M3)" );
}

void RB_ProjectionShadowDeform( void )
{
	VK_M3_Touched( "RB_ProjectionShadowDeform (projection shadow, M3)" );
}

void RB_DeformTessGeometry( void )
{
	VK_M3_Touched( "RB_DeformTessGeometry (shader deforms, M3)" );
}

void ProjectDlightTexture( void )
{
	VK_M3_Touched( "ProjectDlightTexture (dynamic lights, M3)" );
}

void R_InitSkyTexCoords( float cloudLayerHeight )
{
	(void)cloudLayerHeight;
	VK_M3_Touched( "R_InitSkyTexCoords (sky, M3)" );
}

void R_BuildCloudData( shaderCommands_t *shader )
{
	(void)shader;
	VK_M3_Touched( "R_BuildCloudData (sky, M3)" );
}

/* ---- the module globals the data model names ----------------------------- */

qboolean haveClampToEdge = qtrue;
float	displayAspect = 4.0f / 3.0f;
int	smp_texTranscode = 0;
qboolean smp_hasAstcLdr = qfalse;
qboolean smp_hasAstcSliced3d = qfalse;
qboolean smp_hasEtc2Core = qfalse;

qboolean R_ProbeStatsOn( void )
{
	static cvar_t *probe;

	if ( !probe ) {
		probe = ri.Cvar_Get( "qa_probe", "1", 0 );
	}

	return ( probe && probe->integer ) ? qtrue : qfalse;
}

int BE_Microseconds( void )
{
	return ri.Milliseconds() * 1000;
}

qboolean BE_On( void )
{
	return R_ProbeStatsOn();
}


/* the ghost overlay surface: the image layer names it, M3 draws it */
void LoadGHOST( const char *name, byte **pic, int *width, int *height )
{
	VK_M3_Touched( "LoadGHOST (ghost overlay, M3)" );
	*pic = NULL;
	*width = 0;
	*height = 0;
}

void R_SetGhostImage( const char *name, image_t *image )
{
	VK_M3_Touched( "R_SetGhostImage (ghost overlay, M3)" );
	( void )name;
	( void )image;
}

void R_UpdateGhostTextures( void )
{
	VK_M3_Touched( "R_UpdateGhostTextures (ghost overlay, M3)" );
}

qboolean VKimp_GLMainOwnsContext( void )
{
	/* There is no context to own: only the render thread records, and the
	   front end reaches Vulkan only through R_SyncRenderThread, which parks it. */
	return qfalse;
}
