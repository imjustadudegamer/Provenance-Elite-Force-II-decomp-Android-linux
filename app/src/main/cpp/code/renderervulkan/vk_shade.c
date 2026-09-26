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
 * vk_shade.c -- the 3D shade layer: the state machine, the uniform block and
 * the stage draw.
 *
 * The reference renderer splits this across tr_backend.c (the state words),
 * tr_vbo.c (the stream and the matrix stack) and tr_program.c (the programs
 * and their uniforms); here it is one file, because every one of those calls
 * ends as a pipeline key, a uniform write or a recorded draw.
 *
 *   VK_State / VK_Cull / VK_ApplyStateBits  ->  the pending pipeline key
 *   VK_SetUniform*                          ->  the std140 block's staging
 *   VK_Setup{View,Stage,Fog}Uniforms        ->  the values the reference renderer loads
 *   VK_DrawStage / RB_VBO_DrawTessArrays    ->  pack, bind, draw
 *
 * The uniform block's layout is read back out of the compiled SPIR-V by
 * tools/build_vk_spirv.py and emitted as the VK_UBO_OFF_* constants this file
 * packs at, so the CPU side and the shader cannot drift.
 */

#include "tr_local.h"
#include "tr_program.h"
#include "tr_vbo.h"
#include "vk_shaders_embedded3d.h"

#include <string.h>
#include <math.h>

static const short vk_uboMap[VK_U_COUNT] = {
	VK_UBO_OFF_U_MVP,
	-1,                    /* u_ModelView: not in the block */
	VK_UBO_OFF_U_MODELMATRIX,
	VK_UBO_OFF_U_VIEWORIGIN,
	VK_UBO_OFF_U_VIEWAXIS0,
	VK_UBO_OFF_U_LOCALVIEWORIGIN,
	VK_UBO_OFF_U_ENTITYORIGIN,
	VK_UBO_OFF_U_ENTITYAXIS,
	VK_UBO_OFF_U_ENTITYCOLOR,
	VK_UBO_OFF_U_BASECOLOR,
	VK_UBO_OFF_U_VERTCOLOR,
	VK_UBO_OFF_U_IDENTITYLIGHT,
	VK_UBO_OFF_U_IDENTITYLIGHTBYTE,
	VK_UBO_OFF_U_ENTITYFADE,
	VK_UBO_OFF_U_PORTALRANGE,
	VK_UBO_OFF_U_SPECORIGIN,
	VK_UBO_OFF_U_SUNDIRECTION,
	VK_UBO_OFF_U_SKYALPHA,
	VK_UBO_OFF_U_SKYSCROLL,
	VK_UBO_OFF_U_SKYALPHAMODE,
	VK_UBO_OFF_U_ENTITYTEXCOORD,
	VK_UBO_OFF_U_COLORGEN,
	VK_UBO_OFF_U_ALPHAGEN,
	VK_UBO_OFF_U_ALPHAMIN,
	VK_UBO_OFF_U_ALPHAMAX,
	VK_UBO_OFF_U_ALPHACONST,
	VK_UBO_OFF_U_ALPHACONSTMIN,
	VK_UBO_OFF_U_WAVERGB,
	VK_UBO_OFF_U_WAVEALPHA,
	VK_UBO_OFF_U_FREQRGB,
	VK_UBO_OFF_U_FREQALPHA,
	VK_UBO_OFF_U_TCGEN0,
	VK_UBO_OFF_U_TCGEN1,
	VK_UBO_OFF_U_TCGEN0VEC0,
	VK_UBO_OFF_U_TCGEN0VEC1,
	VK_UBO_OFF_U_TCGEN1VEC0,
	VK_UBO_OFF_U_TCGEN1VEC1,
	VK_UBO_OFF_U_NUMTEXMOD0,
	VK_UBO_OFF_U_NUMTEXMOD1,
	VK_UBO_OFF_U_TEXMOD0,
	VK_UBO_OFF_U_TEXMOD1,
	VK_UBO_OFF_U_ALPHATEST,
	VK_UBO_OFF_U_FORCEALPHA,
	VK_UBO_OFF_U_TEXENV0,
	VK_UBO_OFF_U_TEXENV1,
	-1,                    /* u_Tex0: not in the block */
	-1,                    /* u_Tex1: not in the block */
	VK_UBO_OFF_U_TEX1COLOR,
	-1,                    /* u_WaveTable: not in the block */
	VK_UBO_OFF_U_CLIPPLANE,
	VK_UBO_OFF_U_CLIPPLANEENABLE,
	VK_UBO_OFF_U_SHADERTIME,
	VK_UBO_OFF_U_ACFF,
	VK_UBO_OFF_U_FOGNUM,
	VK_UBO_OFF_U_FOGTCSCALE,
	VK_UBO_OFF_U_FOGSCONSTANT,
	VK_UBO_OFF_U_FOGSVECTOR,
	VK_UBO_OFF_U_FOGTCONSTANT,
	VK_UBO_OFF_U_FOGTVECTOR,
	VK_UBO_OFF_U_FOGVIEWOUTSIDE,
	VK_UBO_OFF_U_FOGCOLOR,
	VK_UBO_OFF_U_FOGDENSITYARM,
	VK_UBO_OFF_U_FOGOOLEN,
	VK_UBO_OFF_U_FOGPLANECONST,
	VK_UBO_OFF_U_FOGDVIEW,
	VK_UBO_OFF_U_FOGDENSITYTVECTOR,
	VK_UBO_OFF_U_FOGMODE,
	VK_UBO_OFF_U_FOGSTART,
	VK_UBO_OFF_U_FOGEND,
	VK_UBO_OFF_U_FOGFIXCOLOR,
};
static const int vk_uboSize[VK_U_COUNT] = {
	64, 0, 64, 4, 4, 4, 4, 48, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 64, 64, 4, 4, 4, 4, 0, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

#define VK_UBO_SIZE_PAD	2048		/* the std140 block, rounded up for the bind range */

#define MAT4_IDENTITY(v) do { \
	(v)[0] = 1.0f; (v)[1] = 0.0f; (v)[2] = 0.0f; (v)[3] = 0.0f; \
	(v)[4] = 0.0f; (v)[5] = 1.0f; (v)[6] = 0.0f; (v)[7] = 0.0f; \
	(v)[8] = 0.0f; (v)[9] = 0.0f; (v)[10] = 1.0f; (v)[11] = 0.0f; \
	(v)[12] = 0.0f; (v)[13] = 0.0f; (v)[14] = 0.0f; (v)[15] = 1.0f; } while (0)

qboolean VK_Shade_Bind( VkCommandBuffer cmd, const shader_t *shader );
VkCommandBuffer VK_Shade_CommandBuffer( void );
void VK_UploadClipPlane( vkProgram_t *p );
float EvalWaveForm( const waveForm_t *wf );

/* ---- the matrix stack (the reference renderer keeps it in tr_vbo.c) ------ */

#define VK_MATRIX_DEPTH 8

static float	s_modelView[VK_MATRIX_DEPTH][16];
static float	s_projection[VK_MATRIX_DEPTH][16];
static int		s_modelViewDepth;
static int		s_projectionDepth;
static qboolean s_modelViewDirty;
static qboolean s_projectionDirty;

static vec4_t	s_clipPlane;
static qboolean	s_clipPlaneValid;

static void VK_Matrix_Identity( float *m )
{
	MAT4_IDENTITY( m );
}

static void VK_Matrix_Multiply( float *out, const float *a, const float *b )
{
	int i, j, k;

	for ( i = 0; i < 4; i++ ) {
		for ( j = 0; j < 4; j++ ) {
			float sum = 0.0f;
			for ( k = 0; k < 4; k++ ) {
				sum += a[k * 4 + j] * b[i * 4 + k];
			}
			out[i * 4 + j] = sum;
		}
	}
}

/* The window seam: engine matrices leave +y up in clip space, a Vulkan
   framebuffer has y down, so clip y negates here and row 1 is 1, 5, 9, 13. */
void VK_Mat4_ClipToWindow( float *m )
{
	m[1] = -m[1];
	m[5] = -m[5];
	m[9] = -m[9];
	m[13] = -m[13];
}

int VK_Shade_ModelViewDepth( void )
{
	return s_modelViewDepth;
}

int VK_Shade_ProjectionDepth( void )
{
	return s_projectionDepth;
}

const float *RB_GetModelViewMatrix( void )
{
	return s_modelView[s_modelViewDepth];
}

const float *RB_GetProjectionMatrix( void )
{
	return s_projection[s_projectionDepth];
}

void RB_LoadModelViewMatrix( const float *matrix )
{
	memcpy( s_modelView[s_modelViewDepth], matrix, 16 * sizeof( float ) );
	s_modelViewDirty = qtrue;
}

void RB_LoadProjectionMatrix( const float *matrix )
{
	memcpy( s_projection[s_projectionDepth], matrix, 16 * sizeof( float ) );
	s_projectionDirty = qtrue;
}

void RB_LoadIdentityModelView( void )
{
	VK_Matrix_Identity( s_modelView[s_modelViewDepth] );
	s_modelViewDirty = qtrue;
}

void RB_LoadIdentityProjection( void )
{
	VK_Matrix_Identity( s_projection[s_projectionDepth] );
	s_projectionDirty = qtrue;
}

void RB_PushModelView( void )
{
	if ( s_modelViewDepth < VK_MATRIX_DEPTH - 1 ) {
		memcpy( s_modelView[s_modelViewDepth + 1], s_modelView[s_modelViewDepth], 16 * sizeof( float ) );
		s_modelViewDepth++;
	}
}

void RB_PopModelView( void )
{
	if ( s_modelViewDepth > 0 ) {
		s_modelViewDepth--;
	}
	s_modelViewDirty = qtrue;
}

void RB_PushProjection( void )
{
	if ( s_projectionDepth < VK_MATRIX_DEPTH - 1 ) {
		memcpy( s_projection[s_projectionDepth + 1], s_projection[s_projectionDepth], 16 * sizeof( float ) );
		s_projectionDepth++;
	}
}

void RB_PopProjection( void )
{
	if ( s_projectionDepth > 0 ) {
		s_projectionDepth--;
	}
	s_projectionDirty = qtrue;
}

void RB_PopModelViewTo( int depth )
{
	if ( depth < 0 ) {
		depth = 0;
	}
	if ( depth > VK_MATRIX_DEPTH - 1 ) {
		depth = VK_MATRIX_DEPTH - 1;
	}
	s_modelViewDepth = depth;
	s_modelViewDirty = qtrue;
}

void RB_PopProjectionTo( int depth )
{
	if ( depth < 0 ) {
		depth = 0;
	}
	if ( depth > VK_MATRIX_DEPTH - 1 ) {
		depth = VK_MATRIX_DEPTH - 1;
	}
	s_projectionDepth = depth;
	s_projectionDirty = qtrue;
}

void RB_TranslateModelView( float x, float y, float z )
{
	float	*t = s_modelView[s_modelViewDepth];

	/* column-major translation: post-multiply */
	t[12] += t[0] * x + t[4] * y + t[8] * z;
	t[13] += t[1] * x + t[5] * y + t[9] * z;
	t[14] += t[2] * x + t[6] * y + t[10] * z;
	s_modelViewDirty = qtrue;
}

void RB_OrthoProjection( float left, float right, float bottom, float top, float zNear, float zFar )
{
	float	*p = s_projection[s_projectionDepth];

	memset( p, 0, 16 * sizeof( float ) );
	p[0] = 2.0f / ( right - left );
	p[5] = 2.0f / ( top - bottom );
	/* the same zero-to-one z rows Set2DWindow's ortho already carries
	   (vk_draw.c: -1/fn and 0.5 - (f+n)/2fn, which is -n/fn): the OpenGL ortho
	   rows put the near plane on -1, outside Vulkan's clip volume.  This is the
	   ortho the sun flare's (-99999, 99999) window builds, and its own quad sat
	   on the volume's boundary face under the old rows. */
	p[10] = -1.0f / ( zFar - zNear );
	p[12] = -( right + left ) / ( right - left );
	p[13] = -( top + bottom ) / ( top - bottom );
	p[14] = 0.5f - ( zFar + zNear ) / ( 2.0f * ( zFar - zNear ) );
	p[15] = 1.0f;
	s_projectionDirty = qtrue;
}

/* the portal clip plane, discarded on in the fragment stage (no user planes) */
void RB_ClipPlane_Set( const vec4_t plane )
{
	Vector4Copy( plane, s_clipPlane );
	s_clipPlaneValid = qtrue;
}

void RB_ClipPlane_Disable( void )
{
	s_clipPlaneValid = qfalse;
}

qboolean RB_ClipPlane_Get( vec4_t out )
{
	if ( !s_clipPlaneValid ) {
		return qfalse;
	}
	Vector4Copy( s_clipPlane, out );
	return qtrue;
}

/* ---- the uniform block -------------------------------------------------- */

/* the staging block, at the SPIR-V-reflected offsets; one block serves every program */
static unsigned char	s_ubo[VK_UBO_SIZE_PAD];
static qboolean			s_uboDirty;
static int				s_uboHighWater = VK_UBO_SIZE_PAD;

#define VK_UNBO(p) ( ( void )( p ) )

static int VK_Ubo_Offset( vkUniform_t u )
{
	if ( (int)u < 0 || (int)u >= VK_U_COUNT ) {
		return -1;
	}
	return vk_uboMap[u];
}

static int VK_Ubo_Size( vkUniform_t u )
{
	if ( (int)u < 0 || (int)u >= VK_U_COUNT ) {
		return 0;
	}
	return vk_uboSize[u];
}

static void VK_Ubo_Set( vkUniform_t u, const void *data, int bytes )
{
	int off = VK_Ubo_Offset( u );

	if ( off < 0 ) {
		return;
	}
	if ( bytes > VK_Ubo_Size( u ) ) {
		bytes = VK_Ubo_Size( u );
	}
	memcpy( s_ubo + off, data, bytes );
	if ( off + bytes > s_uboHighWater ) {
		s_uboHighWater = off + bytes;
	}
	s_uboDirty = qtrue;
}

void VK_SetUniform1i( vkProgram_t *p, vkUniform_t u, int v )
{
	VK_UNBO(p);
	VK_Ubo_Set( u, &v, sizeof( v ) );
}

void VK_SetUniform1f( vkProgram_t *p, vkUniform_t u, float v )
{
	VK_UNBO(p);
	VK_Ubo_Set( u, &v, sizeof( v ) );
}

void VK_SetUniform2f( vkProgram_t *p, vkUniform_t u, float x, float y )
{
	float v[2];

	VK_UNBO(p);
	v[0] = x; v[1] = y;
	VK_Ubo_Set( u, v, sizeof( v ) );
}

void VK_SetUniform3f( vkProgram_t *p, vkUniform_t u, float x, float y, float z )
{
	float v[3];

	VK_UNBO(p);
	v[0] = x; v[1] = y; v[2] = z;
	VK_Ubo_Set( u, v, sizeof( v ) );
}

void VK_SetUniform4f( vkProgram_t *p, vkUniform_t u, float x, float y, float z, float w )
{
	float v[4];

	VK_UNBO(p);
	v[0] = x; v[1] = y; v[2] = z; v[3] = w;
	VK_Ubo_Set( u, v, sizeof( v ) );
}

void VK_SetUniformMat4( vkProgram_t *p, vkUniform_t u, const float *m )
{
	VK_UNBO(p);
	VK_Ubo_Set( u, m, 16 * sizeof( float ) );
}

void VK_SetUniform3fv( vkProgram_t *p, vkUniform_t u, const float *v )
{
	VK_UNBO(p);
	VK_Ubo_Set( u, v, 3 * sizeof( float ) );
}

void VK_SetUniform4fv( vkProgram_t *p, vkUniform_t u, const float *v )
{
	VK_UNBO(p);
	VK_Ubo_Set( u, v, 4 * sizeof( float ) );
}

void VK_SetUniformMatrix3fv( vkProgram_t *p, vkUniform_t u, const float *v )
{
	/* std140 lays a mat3 out as three columns padded to 16 bytes */
	float m[12];

	VK_UNBO(p);
	m[0] = v[0]; m[1] = v[1]; m[2] = v[2]; m[3] = 0.0f;
	m[4] = v[3]; m[5] = v[4]; m[6] = v[5]; m[7] = 0.0f;
	m[8] = v[6]; m[9] = v[7]; m[10] = v[8]; m[11] = 0.0f;
	VK_Ubo_Set( u, m, sizeof( m ) );
}

void VK_Mat4Mul( float *out, const float *a, const float *b )  /* the view setup's multiply */
{
	VK_Matrix_Multiply( out, a, b );
}
	/* the uniform's byte offset in the std140 block; -1 the block lacks it */


/* ---- the programs -------------------------------------------------------- */

/* one row of the offline table: defines bitmask, two module handles */
#define VK_PROG_MAX 256

typedef struct {
	vkShaderModuleDef_t	def;
	VkShaderModule		module;
} vkProgModule_t;

static vkProgModule_t	vk_progModules[VK_SHADER_MODULE_COUNT];
static int				vk_numProgModules;

static vkProgram_t	vk_programs[VK_PROG_MAX];
static int				vk_numPrograms;

/* Every boot-persistent cache in this file is looked up by value and was
   searched by walking the entries it had filled, so the per-draw cost grew with
   the boot instead of staying flat.  VK_GetProgram is one of them: it is asked
   once per stage of every surface, and its table is 256 long.  Each index below
   stores the entry index plus one, so 0 is "empty", and the insert uses the same
   probe the lookup starts at, so an inserted key is always found.  The index is
   at least twice its table, so a full table still leaves an empty slot. */
#define VK_PROG_HASH 512
static int				s_programIndex[VK_PROG_HASH];

static unsigned VK_ProgramHash( unsigned defines )
{
	return ( defines * 2654435761u ) & ( VK_PROG_HASH - 1u );
}

/* The module table is 682 entries and the permutation path asks for one
   (family, defines, stage) triple per stage request -- 367 a frame in the
   15:34 boot.  Both the old finder and the module loop that followed it walked
   the whole table on every request, and the loop had no early exit, so a cache
   hit on an already-built pipeline still paid about 2700 iterations.  One open
   addressed index, built once from the same table, answers in a probe or two.
   The def's key is (family, defines-without-the-family-byte, isFragment), which
   is exactly what the callers pass. */
#define VK_MODULE_HASH 2048
static int				s_moduleIndex[VK_MODULE_HASH];	/* stored as slot + 1, 0 = empty */

static unsigned VK_ModuleHash( int family, unsigned defines, int isFragment )
{
	unsigned h = (unsigned)family * 2654435761u;

	h ^= ( defines & 0xFFFFu ) * 40503u;
	h ^= (unsigned)isFragment * 2246822519u;

	return h & ( VK_MODULE_HASH - 1u );
}

static void VK_ModuleIndexBuild( void )
{
	int i;

	memset( s_moduleIndex, 0, sizeof( s_moduleIndex ) );

	for ( i = 0; i < VK_SHADER_MODULE_COUNT; i++ ) {
		const vkShaderModuleDef_t *d = &vk_progModules[i].def;
		unsigned h = VK_ModuleHash( d->family, d->defines, d->isFragment );

		while ( s_moduleIndex[h] ) {
			h = ( h + 1 ) & ( VK_MODULE_HASH - 1u );
		}
		s_moduleIndex[h] = i + 1;
	}
}

static int VK_ModuleSlot( int family, unsigned defines, int isFragment )
{
	unsigned h = VK_ModuleHash( family, defines, isFragment );
	unsigned start = h;

	while ( s_moduleIndex[h] ) {
		const vkShaderModuleDef_t *d = &vk_progModules[s_moduleIndex[h] - 1].def;

		if ( d->family == family && d->defines == defines && d->isFragment == isFragment ) {
			return s_moduleIndex[h] - 1;
		}

		h = ( h + 1 ) & ( VK_MODULE_HASH - 1u );
		if ( h == start ) {
			break;
		}
	}

	return -1;
}

static const vkShaderModuleDef_t *VK_Program_Find( int family, unsigned defines, int isFragment )
{
	int slot = VK_ModuleSlot( family, defines, isFragment );

	return ( slot >= 0 ) ? &vk_progModules[slot].def : NULL;
}

/* The lookup key is (family, permutation bits).  A program's defines bitmask
   carries its family in the high byte, so the caller's key still holds those
   bits; the offline table stores the family in its own field and keeps only the
   permutation bits in defines.  Strip the family byte here, or every non-STAGE
   family asks for a permutation the table never names -- STAGE hides the bug
   because VKP_FAMILY_STAGE is zero. */
static qboolean VK_Program_Modules( int family, unsigned defines, VkShaderModule *vert, VkShaderModule *frag )
{
	int v;
	int f;

	defines &= ~VKP_FAMILY_MASK;
	v = VK_ModuleSlot( family, defines, 0 );
	f = VK_ModuleSlot( family, defines, 1 );

	if ( v < 0 || f < 0 ) {
		return qfalse;
	}

	*vert = vk_progModules[v].module;
	*frag = vk_progModules[f].module;

	return qtrue;
}

qboolean VK_Shade_Init( void )
{
	VkShaderModuleCreateInfo info;
	VkResult result;
	int i;

	memset( vk_progModules, 0, sizeof( vk_progModules ) );
	for ( i = 0; i < VK_SHADER_MODULE_COUNT; i++ ) {
		memset( &info, 0, sizeof( info ) );
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = vkShaderModules[i].words * sizeof( unsigned int );
		info.pCode = vkShaderModules[i].spv;
		result = qvk.CreateShaderModule( vk.device, &info, NULL, &vk_progModules[i].module );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkCreateShaderModule(stage)" ) ) {
			return qfalse;
		}
		vk_progModules[i].def = vkShaderModules[i];
	}
	vk_numProgModules = VK_SHADER_MODULE_COUNT;

	/* the index is built from the table that was just filled, before any request */
	VK_ModuleIndexBuild();

	/* the miss census: every family the draw side can name must resolve through
	   the same key VK_Shade_Pipeline uses, so a gap here is the whole class */
	{
		static const struct {
			unsigned	bit;
			int			family;
		} familyKeys[] = {
			{ VKP_FAMILY_STAGE,	VK_SHADER_FAM_STAGE },
			{ VKP_FAMILY_2D,	VK_SHADER_FAM_2D },
			{ VKP_FAMILY_DLIGHT,	VK_SHADER_FAM_DLIGHT },
			{ VKP_FAMILY_SKY,	VK_SHADER_FAM_SKY },
			{ VKP_FAMILY_SPRITE,	VK_SHADER_FAM_SPRITE },
			{ VKP_FAMILY_SUNFLARE,	VK_SHADER_FAM_SUNFLARE },
			{ VKP_FAMILY_SHADOW,	VK_SHADER_FAM_SHADOW },
			{ VKP_FAMILY_WIRE,	VK_SHADER_FAM_WIRE },
		};
		int resolved = 0;
		int total = ( int )( sizeof( familyKeys ) / sizeof( familyKeys[0] ) );

		for ( i = 0; i < total; i++ ) {
			VkShaderModule v, f;

			if ( VK_Program_Modules( familyKeys[i].family, familyKeys[i].bit, &v, &f ) ) {
				resolved++;
			}
		}
		VK_Guard_Row( "VK:shade", "modules=%d permutations=%d families=%d resolved=%d",
					  VK_SHADER_MODULE_COUNT, VK_SHADER_MODULE_COUNT / 2, total, resolved );
	}
	return qtrue;
}

void VK_Shade_Shutdown( void )
{
	int i;

	for ( i = 0; i < vk_numProgModules; i++ ) {
		if ( vk_progModules[i].module != VK_NULL_HANDLE ) {
			qvk.DestroyShaderModule( vk.device, vk_progModules[i].module, NULL );
			vk_progModules[i].module = VK_NULL_HANDLE;
		}
	}
	vk_numProgModules = 0;
	vk_numPrograms = 0;
	memset( s_programIndex, 0, sizeof( s_programIndex ) );
}

/* the family a defines bitmask carries; the reference renderer's high byte */
static int VK_Family_Of( unsigned defines )
{
	switch ( defines & VKP_FAMILY_MASK ) {
	case VKP_FAMILY_2D:		return VK_SHADER_FAM_2D;
	case VKP_FAMILY_DLIGHT:	return VK_SHADER_FAM_DLIGHT;
	case VKP_FAMILY_SKY:	return VK_SHADER_FAM_SKY;
	case VKP_FAMILY_SPRITE:	return VK_SHADER_FAM_SPRITE;
	case VKP_FAMILY_SUNFLARE: return VK_SHADER_FAM_SUNFLARE;
	case VKP_FAMILY_SHADOW:	return VK_SHADER_FAM_SHADOW;
	case VKP_FAMILY_WIRE:	return VK_SHADER_FAM_WIRE;
	default:				return VK_SHADER_FAM_STAGE;
	}
}

vkProgram_t *VK_GetProgram( unsigned defines )
{
	vkProgram_t *p;
	unsigned	h = VK_ProgramHash( defines );
	int			i;

	for ( i = 0; i < VK_PROG_HASH; i++ ) {
		int at = s_programIndex[( h + i ) & ( VK_PROG_HASH - 1u )];

		if ( !at ) {
			break;
		}
		if ( vk_programs[at - 1].defines == defines && vk_programs[at - 1].linked ) {
			return &vk_programs[at - 1];
		}
	}
	if ( vk_numPrograms >= VK_PROG_MAX ) {
		/* Every fixed-size table here answers exhaustion the same way: name it
		   through the guard and hand back a failure the draw path already
		   refuses -- VK_Shade_Pipeline returns VK_NULL_HANDLE, VK_Shade_Set
		   returns VK_NULL_HANDLE, the UBO ring and the vertex ring return their
		   own failure.  This table used to hand back vk_programs[0] instead,
		   which is a live program for a DIFFERENT defines mask: the caller then
		   packs the requested stage's uniforms into it and binds it, and the
		   surface renders with the wrong permutation and no row names why.
		   NULL is safe end to end -- the uniform setters only fill the s_ubo
		   staging block, and VK_Shade_Bind refuses on a NULL prog before it
		   takes a UBO slice -- so the surface is dropped and refusals counts
		   it, exactly like a pipeline miss. */
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "VK_GetProgram: program table full", VK_INCOMPLETE );
		return NULL;
	}
	p = &vk_programs[vk_numPrograms++];
	memset( p, 0, sizeof( *p ) );
	Q_strncpyz( p->name, "vk_program", sizeof( p->name ) );
	p->defines = defines;
	p->program = VK_Family_Of( defines );
	p->linked = qtrue;
	for ( i = 0; i < VK_PROG_HASH; i++ ) {
		int *at = &s_programIndex[( h + i ) & ( VK_PROG_HASH - 1u )];

		if ( !*at ) {
			*at = vk_numPrograms;
			break;
		}
	}
	return p;
}

void VK_ProgramsInit( void ) { }
void VK_ProgramsShutdown( void ) { }
void VK_PrebuildPrograms( void ) { }
void VK_CloseProgramRegistration( void ) { }
int VK_ProgramsCreatedAfterRegistration( void ) { return 0; }

unsigned VK_StageDefines_Full( const shaderStage_t *pStage, const shader_t *shader,
								  unsigned bundleCount, unsigned texEnv, unsigned fogStage,
								  unsigned extra )
{
	unsigned	d = 0;
	unsigned	rgbGen, alphaGen;
	unsigned	tc0, tc1;
	int			b;

	(void)shader;
	(void)texEnv;

	if ( !pStage )
	{
		return 0;
	}

	rgbGen = (unsigned)pStage->rgbGen;
	alphaGen = (unsigned)pStage->alphaGen;

	if ( bundleCount == 0 )
	{
		bundleCount = ( pStage->bundle[1].image[0] != NULL ) ? 2u : 1u;
	}
	if ( fogStage == 0 )
	{
		if ( rgbGen == (unsigned)CGEN_FOG )
		{
			fogStage = 1;
		}
		else if ( rgbGen == (unsigned)CGEN_FOG_BLEND )
		{
			fogStage = 2;
		}
	}
	tc0 = (unsigned)pStage->bundle[0].tcGen;
	tc1 = ( bundleCount == 2 ) ? (unsigned)pStage->bundle[1].tcGen : (unsigned)TCGEN_BAD;

	if ( rgbGen != (unsigned)CGEN_IDENTITY
		|| ( alphaGen != (unsigned)AGEN_SKIP && alphaGen != (unsigned)AGEN_IDENTITY ) )
	{
		d |= VKP_RGBAGEN;
	}
	if ( rgbGen == (unsigned)CGEN_DOT || rgbGen == (unsigned)CGEN_ONE_MINUS_DOT
		|| alphaGen == (unsigned)AGEN_DOT || alphaGen == (unsigned)AGEN_ONE_MINUS_DOT
		|| alphaGen == (unsigned)AGEN_DOT_VIEW || alphaGen == (unsigned)AGEN_ONE_MINUS_DOT_VIEW
		|| alphaGen == (unsigned)AGEN_LIGHTING_SPECULAR || alphaGen == (unsigned)AGEN_HEIGHT_FADE )
	{
		d |= VKP_NORMAL;
	}
	/* the tcGen arms reading the normal need GLP_NORMAL too, or one texel is sampled */
	if ( tc0 == (unsigned)TCGEN_ENVIRONMENT_MAPPED || tc0 == (unsigned)TCGEN_ENVIRONMENT_MAPPED2
		|| tc0 == (unsigned)TCGEN_ANGLE_BASED_ENVIRONMENT || tc0 == (unsigned)TCGEN_SUN_REFLECTION
		|| ( bundleCount == 2
			 && ( tc1 == (unsigned)TCGEN_ENVIRONMENT_MAPPED || tc1 == (unsigned)TCGEN_ENVIRONMENT_MAPPED2
				  || tc1 == (unsigned)TCGEN_ANGLE_BASED_ENVIRONMENT || tc1 == (unsigned)TCGEN_SUN_REFLECTION ) ) )
	{
		d |= VKP_NORMAL;
	}
	if ( shader && shader->needsNormal )
	{
		d |= VKP_NORMAL;
	}
	if ( rgbGen == (unsigned)CGEN_WAVEFORM || rgbGen == (unsigned)CGEN_MULTIPLY_BY_WAVEFORM
		|| alphaGen == (unsigned)AGEN_WAVEFORM )
	{
		d |= VKP_WAVE;
	}
	if ( alphaGen == (unsigned)AGEN_LIGHTING_SPECULAR )
	{
		d |= VKP_SPECULAR;
	}
	if ( alphaGen == (unsigned)AGEN_DIST_FADE || alphaGen == (unsigned)AGEN_ONE_MINUS_DIST_FADE
		|| alphaGen == (unsigned)AGEN_TIKI_DIST_FADE || alphaGen == (unsigned)AGEN_ONE_MINUS_TIKI_DIST_FADE
		|| alphaGen == (unsigned)AGEN_HEIGHT_FADE )
	{
		d |= VKP_HEIGHTFOG;
	}
	if ( tc0 != (unsigned)TCGEN_TEXTURE && bundleCount >= 1 )
	{
		d |= VKP_TCGEN0;
	}
	if ( bundleCount == 2 && tc1 != (unsigned)TCGEN_LIGHTMAP && tc1 != (unsigned)TCGEN_TEXTURE )
	{
		d |= VKP_TCGEN1;
	}
	if ( bundleCount == 2 )
	{
		d |= VKP_LIGHTMAP;
	}
	if ( rgbGen == (unsigned)CGEN_ENTITY || rgbGen == (unsigned)CGEN_ONE_MINUS_ENTITY
		|| rgbGen == (unsigned)CGEN_LIGHTING_SPHERICAL || rgbGen == (unsigned)CGEN_STATIC
		|| alphaGen == (unsigned)AGEN_ENTITY || alphaGen == (unsigned)AGEN_ONE_MINUS_ENTITY
		|| alphaGen == (unsigned)AGEN_FORCED_ALPHA )
	{
		d |= VKP_ENTITY_SPACE;
	}

	for ( b = 0; b < (int)bundleCount; b++ )
	{
		int tm;

		if ( !pStage->bundle[b].image[0] )
		{
			continue;
		}
		if ( pStage->bundle[b].numTexMods > 0 )
		{
			d |= ( b == 0 ) ? VKP_TCMOD0 : VKP_TCMOD1;
		}
		for ( tm = 0; tm < pStage->bundle[b].numTexMods; tm++ )
		{
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_TURBULENT:
			case TMOD_STRETCH:
			case TMOD_WAVETRANS:
			case TMOD_WAVETRANT:
			case TMOD_BULGETRANS:
				d |= VKP_WAVE;
				break;
			default:
				break;
			}
		}
	}

	if ( ( (unsigned)pStage->stateBits & VKS_ATEST_BITS ) != 0 )
	{
		d |= VKP_ALPHATEST;
	}
	if ( fogStage )
	{
		d |= VKP_FOG;
	}

	return d | extra;
}

unsigned VK_StageDefines( const shaderStage_t *pStage, const shader_t *shader )
{
	return VK_StageDefines_Full( pStage, shader, 0, 0, 0, 0 );
}

unsigned VK_TexEnvForStage( const shaderStage_t *pStage, const shader_t *shader )
{
	unsigned	d = 0;

	(void)shader;
	if ( !pStage )
	{
		return 0;
	}

	if ( pStage->bundle[1].image[0] )
	{
		if ( pStage->multitextureEnv == VK_TENV_ADD )
		{
			d |= VKP_TEXENV_ADD;
		}
		if ( pStage->stateBits & VKS_MULTITEXTURE_ENV )
		{
			d |= VKP_TEXENV_COMBINE4;
		}
		else if ( pStage->stateBits & VKS_MULTITEXTURE )
		{
			d |= VKP_TEXENV_INTERPOLATE;
		}
	}
	return d;
}

qboolean VK_StageNeedsCPUColor( const shaderStage_t *pStage )
{
	if ( !pStage )
	{
		return qfalse;
	}
	switch ( pStage->rgbGen )
	{
	case CGEN_LIGHTING_SPHERICAL:
	case CGEN_STATIC:
		return qtrue;
	default:
		break;
	}
	switch ( pStage->alphaGen )
	{
	case AGEN_NOISE:
	case AGEN_DIST_FADE:
	case AGEN_ONE_MINUS_DIST_FADE:
	case AGEN_TIKI_DIST_FADE:
	case AGEN_ONE_MINUS_TIKI_DIST_FADE:
		return qtrue;
	default:
		break;
	}
	return qfalse;
}

qboolean VK_StageNeedsCPUTexCoords( const shaderStage_t *pStage )
{
	int b, tm;

	if ( !pStage )
	{
		return qfalse;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
	{
		for ( tm = 0; tm < pStage->bundle[b].numTexMods; tm++ )
		{
			if ( pStage->bundle[b].texMods[tm].type == TMOD_MACRO )
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}


/* the wave texmods' function table: 2048 x 5 R32F, texel-fetched, set 0 binding 3; without it tcMod rotate reads sin = cos = 1 */
static int	s_waveTableHandle = -1;

/* ---- the state machine --------------------------------------------------- */

/* the reference applies its state words to the context; here they fold into the pending pipeline key */
typedef struct {
	unsigned		stateBits;
	cullType_t		cull;
	qboolean		polygonOffset;
	int				topology;
	vkProgram_t	*prog;
	VkImageView		texView[2];
	VkSampler		texSampler[2];
	VkSampler		waveSampler;
	int				texBound[2];
	int				currentTmu;
	qboolean		scissor;
	VkRect2D		scissorRect;
	qboolean		depthTest;
	qboolean		depthWrite;
	qboolean		colorMask[4];
	qboolean		mirrored;
	qboolean		stencil;
	qboolean		waveBound;
	VkImageView		waveView;
	int				alphaTest;
} vkShadeState_t;

static vkShadeState_t	s_shade;
int						r_spriteListOpen = 0;

/* the alpha-test latch: the state word's atest bits, for the fragment */
static int s_alphaTest;

void VK_SetFogColor( const vec4_t fColor )
{
	byte rgba[4];

	rgba[0] = (byte)( fColor[0] * 255.0f );
	rgba[1] = (byte)( fColor[1] * 255.0f );
	rgba[2] = (byte)( fColor[2] * 255.0f );
	rgba[3] = 255;
	backEnd.viewParms.fogColorInt = ( rgba[0] ) | ( rgba[1] << 8 ) | ( rgba[2] << 16 ) | ( 255 << 24 );
}

void VK_State( unsigned long stateBits )
{
	s_shade.stateBits = (unsigned long)stateBits;
	s_shade.depthWrite = ( stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;
	s_shade.depthTest = ( stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
	/* the reference's GLES3_DepthState: the 2D gate is open again the moment a
	   3D state block turns depth back on, so the next window has to shut it */
	if ( s_shade.depthTest ) {
		VK_DepthGate3DOpen();
	}
}

void VK_ApplyStateBits( unsigned long stateBits )
{
		/* the alpha test reaches the fragment stage through the stage uniform block; a push on the 2D walk's layout is invalid usage */
	VK_State( stateBits );
	switch ( (unsigned long)stateBits & VKS_ATEST_BITS ) {
	case VKS_ATEST_GT_0:	s_alphaTest = 1; break;
	case VKS_ATEST_LT_80:	s_alphaTest = 2; break;
	case VKS_ATEST_GE_80:	s_alphaTest = 4; break;
	default:				s_alphaTest = 0; break;
	}
}

unsigned long vk_Shade_StateBits( void )
{
	return s_shade.stateBits;
}

unsigned VK_CurrentAlphaTest( void )
{
	return s_alphaTest;
}

void VK_Cull( int cull )
{
	s_shade.cull = cull;
}

void VK_TexEnv( int env )
{
	/* folded into the program permutation; nothing to apply */
	(void)env;
}

void VK_DepthFuncApply( int func )
{
	/* a pipeline state word; the key carries the equal-depth arm */
	(void)func;
}

void VK_DepthState( qboolean test, qboolean write )
{
	s_shade.depthTest = test;
	s_shade.depthWrite = write;
}

void VK_DepthStateReset( void )
{
	VK_DepthState( qtrue, qtrue );
}

void VK_StencilState( qboolean test, int func, int ref, unsigned mask )
{
	/* stencil shadow volumes are a later milestone: no stencil attachment yet */
	(void)test; (void)func; (void)ref; (void)mask;
}

/* The 2D window's state block ran last.  The reference holds this gate shut so
   the UI draws with depth test off regardless of what a UI shader's own state
   bits say, and opens it the moment a 3D draw turns depth back on -- the UI
   was being depth-tested against the world, and the nearest geometry (the
   viewmodel) drew straight over the health bar while the pause menu lost
   surfaces to whatever the world had left behind. */
static qboolean	s_depthGate2D = qfalse;

qboolean VK_DepthGate2D( void )
{
	return s_depthGate2D;
}

void VK_DepthGate2DTake( void )
{
	s_depthGate2D = qtrue;
}

void VK_DepthGate3DOpen( void )
{
	s_depthGate2D = qfalse;
}

/* ---- the blocking driver waits ------------------------------------------- */

/* A hang stops making progress inside one of these, and a boot that stops here
   otherwise says nothing.  Each wait names itself on the way in and on the way
   out, with the thread and the caller's business, so the last row before a stop
   is the wait that never returned and the side of the front end that asked. */
static void VK_Sync_Row( const char *site, const char *why, qboolean leaving, long usec )
{
	VK_Guard_Row( "VK:sync", "site=%s why=%s phase=%s thr=%s id=%lu f=%d usec=%ld",
				  site, why, leaving ? "leave" : "enter",
				  VKimp_IsRenderThread() ? "render" : "main",
				  VKimp_ThreadID(), RB_FrameCount(), usec );
}

VkResult VK_Sync_WaitForFences( VkFence fence, VkBool32 waitAll, const char *site, const char *why )
{
	VkResult result;
	int		 t0;
	qboolean row = R_ProbeStatsOn();
	/* the walk's own frame fence: the device owns this wait, and the walk's
	   cost row has to be able to subtract it from its CPU time */
	qboolean walkFence = ( !strcmp( site, "frame-fence" ) && !strcmp( why, "frame" )
						   && VKimp_IsRenderThread() && VK_WalkStatOn() );

	if ( !row && !walkFence ) {
		return qvk.WaitForFences( vk.device, 1, &fence, waitAll, UINT64_MAX );
	}

	if ( row ) {
		VK_Sync_Row( site, why, qfalse, 0 );
	}
	t0 = BE_Microseconds();
	result = qvk.WaitForFences( vk.device, 1, &fence, waitAll, UINT64_MAX );
	if ( walkFence ) {
		VK_WalkStat_Add( VK_WP_FENCE, (long long)( BE_Microseconds() - t0 ) );
	}
	if ( row ) {
		VK_Sync_Row( site, why, qtrue, BE_Microseconds() - t0 );
	}
	return result;
}

VkResult VK_Sync_DeviceWaitIdle( const char *site, const char *why )
{
	VkResult result;
	int		 t0;

	if ( !R_ProbeStatsOn() ) {
		return qvk.DeviceWaitIdle( vk.device );
	}

	VK_Sync_Row( site, why, qfalse, 0 );
	t0 = BE_Microseconds();
	result = qvk.DeviceWaitIdle( vk.device );
	VK_Sync_Row( site, why, qtrue, BE_Microseconds() - t0 );
	return result;
}

/* ---- textures ------------------------------------------------------------ */

void VK_SelectTexture( int tmu )
{
	s_shade.currentTmu = tmu;
}

/* The drawn colour of a stage's texture, read back off the device once per
   image at the moment the walk first binds it.  The parse, the decode and the
   colour maths can all be verified on the CPU and the cone still come out
   wrong, because the one thing no offline reading answers is what the sampler
   actually holds: the radar fov source is a green DXT3, the port's decoder is
   correct, the stage is rgbGen const white with GL_SRC_ALPHA/GL_ONE, and the
   cone is red -- so the question is whether the device texture is red or the
   draw is.  Bound (not uploaded) because that is when the image is published
   and in SHADER_READ_ONLY, which is what the copy-source layout must be.  Its
   own gate, default off: one readback is a submit and a fence, so it must not
   ride the master probe gate into a performance boot.  Every image gets a row,
   so a miscoloured surface the operator names on any screen is answered by the
   same boot, not by a second instrument. */
#define VK_TEXEL_PROBE_MAX 4096
static byte s_texelProbed[VK_TEXEL_PROBE_MAX];

static void VK_Shade_TexelProbe( image_t *image )
{
	static cvar_t *cv;
	unsigned char  px[4];
	int            handle;
	const char     *name;

	if ( !cv ) {
		cv = ri.Cvar_Get( "r_texelProbe", "0", 0 );
	}
	if ( !cv || !cv->integer ) {
		return;
	}

	handle = (int)image->texnum;
	if ( handle < 0 || handle >= VK_TEXEL_PROBE_MAX || s_texelProbed[handle] ) {
		return;
	}
	s_texelProbed[handle] = 1;

	name = image->imgName[0] ? image->imgName : "(unnamed)";
	if ( !VK_Texture_ReadTexel( handle, image->uploadWidth / 2, image->uploadHeight / 2, px ) ) {
		VK_Guard_Row( "VK:texel", "name=%s w=%d h=%d read=fail", name,
					  image->uploadWidth, image->uploadHeight );
		return;
	}

	VK_Guard_Row( "VK:texel",
				  "name=%s w=%d h=%d upto=%dx%d mips=%d fmt=0x%x wrap=%d,%d texel_mid=%u,%u,%u,%u",
				  name, image->width, image->height, image->uploadWidth, image->uploadHeight,
				  image->numMipmaps, (unsigned)image->internalFormat,
				  (int)image->wrapClampModeX, (int)image->wrapClampModeY,
				  (unsigned)px[0], (unsigned)px[1], (unsigned)px[2], (unsigned)px[3] );
}

static void VK_Shade_BindImage( image_t *image, int tmu )
{
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;

	if ( !image || image->texnum < 0 ) {		s_shade.texView[tmu] = VK_NULL_HANDLE;
		s_shade.texSampler[tmu] = VK_NULL_HANDLE;
		s_shade.texBound[tmu] = 0;
		return;
	}
	view = VK_Texture_View( image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "bind: texture has no view", VK_INCOMPLETE );
		return;
	}
	/* wrapClampModeX/Y hold the legacy wrap enum: REPEAT is 10497, not 0 */
	VK_Texture_Sampler( ( image->wrapClampModeX != VK_TWRAP_REPEAT
						  || image->wrapClampModeY != VK_TWRAP_REPEAT ), qfalse, qtrue, &sampler );
	s_shade.texView[tmu] = view;
	s_shade.texSampler[tmu] = sampler;
	s_shade.texBound[tmu] = 1;

	VK_Shade_TexelProbe( image );
}

void VK_Bind( image_t *image )
{
	VK_Shade_BindImage( image, s_shade.currentTmu );
}

void VK_BindUnit0( image_t *image )
{
	s_shade.currentTmu = 0;
	VK_Shade_BindImage( image, 0 );
}

void VK_BindMultitexture( image_t *image, int tmu )
{
	s_shade.currentTmu = tmu;
	VK_Shade_BindImage( image, tmu );
}

void VK_BindAnimatedImage( const textureBundle_t *bundle, int unit )
{
	image_t *image;
	int index;

	if ( bundle->isVideoMap ) {
		VK_Shade_BindImage( tr.scratchImage, unit );
		return;
	}
	if ( bundle->numImageAnimations <= 1 ) {
		VK_Shade_BindImage( bundle->image[0], unit );
		return;
	}
	/* R_BindAnimatedImage @0x00034190, raw words: `lfs f1,0x78(tr)' (refdef
	   floatTime), `lfs f0,0xc(bundle)' (speed), `fmuls', `lfs f1,0(2048.0f)',
	   `fmuls', `fctiwz', `srawi. r4,r4,0xb' -- FUNCTABLE_SIZE * (speed * time),
	   shifted down by FUNCTABLE_SIZE2, negatives clamped to 0, then wrapped, or
	   clamped to the last frame under BUNDLE_ANIMATE_ONCE (0x34204..0x3421c).
	   The scale is the wave-table size, not 60: 60 fast-forwarded every animMap
	   by 60x while the clock that feeds it stayed correct, which is why the
	   shader-clock row read clean. */
	index = myftol( FUNCTABLE_SIZE * ( bundle->imageAnimationSpeed * backEnd.refdef.floatTime ) );
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 ) {
		index = 0;
	}
	if ( bundle->flags & BUNDLE_ANIMATE_ONCE ) {
		if ( index >= bundle->numImageAnimations ) {
			index = bundle->numImageAnimations - 1;
		}
	} else {
		index %= bundle->numImageAnimations;
	}
	image = bundle->image[index];
	VK_Shade_BindImage( image, unit );
}

void VK_SetBaseColor4f( vkProgram_t *p, float r, float g, float b, float a )
{
	VK_SetUniform4f( p, VK_U_BASECOLOR, r, g, b, a );
}
void VK_SetupViewUniforms( vkProgram_t *p )
{
	float	mvp[16];
	vec3_t	entityOrigin;
	vec3_t	entityAxis[3];
	float	entityColor[4];
	const float *mv = RB_GetModelViewMatrix();
	const float *proj = RB_GetProjectionMatrix();

	VK_Mat4Mul( mvp, proj, mv );
	VK_Mat4_ClipToWindow( mvp );

	VK_SetUniformMat4( p, VK_U_MVP, mvp );
	VK_SetUniformMat4( p, VK_U_MODELMATRIX, mv );
	VK_SetUniform3fv( p, VK_U_VIEWORIGIN, backEnd.viewParms.ori.origin );
	VK_SetUniform3fv( p, VK_U_VIEWAXIS0, tr.refdef.viewaxis[0] );
	VK_SetUniform3f( p, VK_U_LOCALVIEWORIGIN,
						tr.refdef.vieworg[0], tr.refdef.vieworg[1], tr.refdef.vieworg[2] );

	if ( backEnd.currentEntity )
	{
		VectorCopy( backEnd.currentEntity->e.origin, entityOrigin );
		VectorCopy( backEnd.currentEntity->e.axis[0], entityAxis[0] );
		VectorCopy( backEnd.currentEntity->e.axis[1], entityAxis[1] );
		VectorCopy( backEnd.currentEntity->e.axis[2], entityAxis[2] );
		entityColor[0] = backEnd.currentEntity->e.shaderRGBA[0] / 255.0f;
		entityColor[1] = backEnd.currentEntity->e.shaderRGBA[1] / 255.0f;
		entityColor[2] = backEnd.currentEntity->e.shaderRGBA[2] / 255.0f;
		entityColor[3] = backEnd.currentEntity->e.shaderRGBA[3] / 255.0f;
		VK_SetUniform4f( p, VK_U_ENTITYTEXCOORD,
							backEnd.currentEntity->e.shaderTexCoord[0],
							backEnd.currentEntity->e.shaderTexCoord[1], 0.0f, 0.0f );
	}
	else
	{
		VectorCopy( backEnd.viewParms.ori.origin, entityOrigin );
		VectorCopy( backEnd.viewParms.ori.axis[0], entityAxis[0] );
		VectorCopy( backEnd.viewParms.ori.axis[1], entityAxis[1] );
		VectorCopy( backEnd.viewParms.ori.axis[2], entityAxis[2] );
		entityColor[0] = entityColor[1] = entityColor[2] = entityColor[3] = 1.0f;
		VK_SetUniform4f( p, VK_U_ENTITYTEXCOORD, 0.0f, 0.0f, 0.0f, 0.0f );
	}

	VK_SetUniform3fv( p, VK_U_ENTITYORIGIN, entityOrigin );
	VK_SetUniformMatrix3fv( p, VK_U_ENTITYAXIS, entityAxis[0] );
	VK_SetUniform4fv( p, VK_U_ENTITYCOLOR, entityColor );
	VK_SetUniform1f( p, VK_U_IDENTITYLIGHT, tr.identityLight );
	VK_SetUniform1f( p, VK_U_IDENTITYLIGHTBYTE, (float)tr.identityLightByte );
	VK_SetUniform1f( p, VK_U_ENTITYFADE, tess.entityFade );
	VK_SetUniform1f( p, VK_U_SHADERTIME, tess.shaderTime );
	VK_SetUniform3fv( p, VK_U_SUNDIRECTION, tr.sunDirection );
	VK_SetUniform4f( p, VK_U_VERTCOLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	VK_UploadClipPlane( p );
}

void VK_SetUniform4fvN( vkProgram_t *p, vkUniform_t u, const float *v, int n )
{
	int off = VK_Ubo_Offset( u );
	int size = VK_Ubo_Size( u );
	int bytes;

	VK_UNBO(p);
	if ( off < 0 || n <= 0 ) {
		return;
	}
	bytes = n * 4 * (int)sizeof( float );
	if ( bytes > size ) {
		bytes = size;
	}
	memcpy( s_ubo + off, v, bytes );
	s_uboDirty = qtrue;
}

void VK_SetupStageUniforms( vkProgram_t *p, const shaderStage_t *pStage, const shader_t *shader )
{
	float	baseColor[4];
	qboolean	cpuColor;
	qboolean	cpuTexCoords;
	int		b;

	VK_SetupViewUniforms( p );

	if ( !pStage )
	{
		return;
	}

	cpuColor = VK_StageNeedsCPUColor( pStage );
	cpuTexCoords = VK_StageNeedsCPUTexCoords( pStage );

	/* 2D carries Draw_SetColor in a_Color, so u_BaseColor stays 1 there for the
	   gens that take their colour from the vertex.  Two gens read it anyway, and
	   forcing 1 on them made every constant-colour 2D stage draw white: the
	   retail viewmodes all tint through rgbGen const -- hud/structural 1 0.5 0,
	   nightvision 0.15 1 0.25, phaseshift 1 0 0.8 -- so the tricorder's alternate
	   viewmode came up with its CRT shader and no tint at all. */
	if ( backEnd.in2D && pStage->rgbGen != CGEN_CONSTANT
		 && pStage->rgbGen != CGEN_MULTIPLY_BY_WAVEFORM )
	{
		baseColor[0] = baseColor[1] = baseColor[2] = baseColor[3] = 1.0f;
	}
	else
	{
		if ( pStage->rgbGen == CGEN_GLOBAL_COLOR )
		{
			baseColor[0] = backEnd.color2D[0] / 255.0f;
			baseColor[1] = backEnd.color2D[1] / 255.0f;
			baseColor[2] = backEnd.color2D[2] / 255.0f;
		}
		else
		{
			baseColor[0] = pStage->colorConst[0] / 255.0f;
			baseColor[1] = pStage->colorConst[1] / 255.0f;
			baseColor[2] = pStage->colorConst[2] / 255.0f;
		}
		if ( pStage->alphaGen == AGEN_GLOBAL_ALPHA )
		{
			baseColor[3] = backEnd.color2D[3] / 255.0f;
		}
		else
		{
			baseColor[3] = pStage->colorConst[3] / 255.0f;
		}
	}
	VK_SetUniform4fv( p, VK_U_BASECOLOR, baseColor );

	/* the 2D quad carries Draw_SetColor per vertex; in 3D u_BaseColor carries the global colour */
	if ( backEnd.in2D && !cpuColor )
	{
		VK_SetUniform1i( p, VK_U_COLORGEN,
							pStage->rgbGen == CGEN_GLOBAL_COLOR ? (int)CGEN_EXACT_VERTEX
																: (int)pStage->rgbGen );
		VK_SetUniform1i( p, VK_U_ALPHAGEN,
							pStage->alphaGen == AGEN_GLOBAL_ALPHA ? (int)AGEN_SKIP
																  : (int)pStage->alphaGen );
	}
	else
	{
		VK_SetUniform1i( p, VK_U_COLORGEN, cpuColor ? (int)CGEN_EXACT_VERTEX : (int)pStage->rgbGen );
		VK_SetUniform1i( p, VK_U_ALPHAGEN, cpuColor ? (int)AGEN_SKIP : (int)pStage->alphaGen );
	}

	VK_SetUniform1f( p, VK_U_ALPHAMIN, pStage->alphaMin );
	VK_SetUniform1f( p, VK_U_ALPHAMAX, pStage->alphaMax );
	VK_SetUniform1f( p, VK_U_ALPHACONST, pStage->alphaConst / 255.0f );
	VK_SetUniform1f( p, VK_U_ALPHACONSTMIN, pStage->alphaConstMin / 255.0f );

	VK_SetUniform4f( p, VK_U_WAVERGB,
						(float)pStage->rgbWave.func, pStage->rgbWave.base,
						pStage->rgbWave.amplitude, pStage->rgbWave.phase );
	VK_SetUniform1f( p, VK_U_FREQRGB, pStage->rgbWave.frequency );
	VK_SetUniform4f( p, VK_U_WAVEALPHA,
						(float)pStage->alphaWave.func, pStage->alphaWave.base,
						pStage->alphaWave.amplitude, pStage->alphaWave.phase );
	VK_SetUniform1f( p, VK_U_FREQALPHA, pStage->alphaWave.frequency );

	VK_SetUniform1f( p, VK_U_PORTALRANGE, shader ? shader->portalRange : 0.0f );
	VK_SetUniform3fv( p, VK_U_SPECORIGIN, pStage->specOrigin );

	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
	{
		const textureBundle_t	*bundle = &pStage->bundle[b];
		float			packed[TR_MAX_TEXMODS * 4][4];
		int				tm;
		int				numMods = 0;

		if ( b == 0 )
		{
			VK_SetUniform1i( p, VK_U_TCGEN0,
								cpuTexCoords ? (int)TCGEN_TEXTURE : (int)bundle->tcGen );
			VK_SetUniform3fv( p, VK_U_TCGEN0VEC0, bundle->tcGenVectors[0] );
			VK_SetUniform3fv( p, VK_U_TCGEN0VEC1, bundle->tcGenVectors[1] );
		}
		else
		{
			VK_SetUniform1i( p, VK_U_TCGEN1,
								cpuTexCoords ? (int)TCGEN_TEXTURE : (int)bundle->tcGen );
			VK_SetUniform3fv( p, VK_U_TCGEN1VEC0, bundle->tcGenVectors[0] );
			VK_SetUniform3fv( p, VK_U_TCGEN1VEC1, bundle->tcGenVectors[1] );
		}

		Com_Memset( packed, 0, sizeof( packed ) );

		if ( !cpuTexCoords )
		{
			for ( tm = 0; tm < bundle->numTexMods && tm < TR_MAX_TEXMODS; tm++ )
			{
				texModInfo_t	*tmi = &bundle->texMods[tm];
				float			*p0 = packed[tm * 4 + 0];
				float			*p1 = packed[tm * 4 + 1];

				p0[0] = (float)tmi->type;
				switch ( tmi->type )
				{
				case TMOD_TRANSFORM:			 
					p0[1] = tmi->matrix[0][0];
					p0[2] = tmi->matrix[1][0];
					p0[3] = tmi->translate[0];
					p1[0] = tmi->matrix[0][1];
					p1[1] = tmi->matrix[1][1];
					p1[2] = tmi->translate[1];
					break;
				case TMOD_TURBULENT:			 
					p0[3] = tmi->wave.amplitude;
					p1[0] = tmi->wave.phase;
					p1[1] = tmi->wave.frequency;
					break;
				case TMOD_SCROLL:				 
					p0[1] = tmi->scroll[0];
					p0[2] = tmi->scroll[1];
					break;
				case TMOD_SCALE:				 
					p0[1] = tmi->scale[0];
					p0[2] = tmi->scale[1];
					break;
				case TMOD_STRETCH:				 
					p0[1] = (float)tmi->wave.func;
					p0[2] = tmi->wave.base;
					p0[3] = tmi->wave.amplitude;
					p1[0] = tmi->wave.phase;
					p1[1] = tmi->wave.frequency;
					break;
				case TMOD_ROTATE:				 
					if ( tmi->rotateSpeed == 1234567.0f )
					{
						p0[1] = backEnd.currentEntity
								? -(float)backEnd.currentEntity->e.tag_num : 0.0f;
					}
					else
					{
						p0[1] = tmi->rotateSpeed;
					}
					p0[2] = tmi->rotateCoef;
					p0[3] = tmi->rotateStart;
					break;
				case TMOD_OFFSET:				 
					if ( tmi->scroll[0] != 1234567.0f )
					{
						p0[1] = tmi->scroll[0];
						p0[2] = tmi->scroll[1];
					}
					else
					{
						p0[1] = backEnd.currentEntity ? backEnd.currentEntity->e.tag_num : 0.0f;
						p0[2] = backEnd.currentEntity ? backEnd.currentEntity->e.skin_num : 0.0f;
					}
					break;
				case TMOD_PARALLAX:				 
					p0[1] = tmi->rate[0];
					p0[2] = tmi->rate[1];
					break;
				case TMOD_WAVETRANS:			 
				case TMOD_WAVETRANT:			 
					p0[1] = (float)tmi->wave.func;
					p0[2] = tmi->wave.base;
					p0[3] = tmi->wave.amplitude;
					p1[0] = tmi->wave.phase;
					p1[1] = tmi->wave.frequency;
					break;
				case TMOD_BULGETRANS:			 
				case TMOD_MACRO:				
				case TMOD_NOISE:				
				case TMOD_NONE:
				default:
					break;
				}
				numMods = tm + 1;
			}
		}

		if ( b == 0 )
		{
			VK_SetUniform1i( p, VK_U_NUMTCMOD0, numMods );
			VK_SetUniform4fvN( p, VK_U_TCMOD0, packed[0], TR_MAX_TEXMODS * 4 );
		}
		else
		{
			VK_SetUniform1i( p, VK_U_NUMTCMOD1, numMods );
			VK_SetUniform4fvN( p, VK_U_TCMOD1, packed[0], TR_MAX_TEXMODS * 4 );
		}
	}

	VK_SetUniform1i( p, VK_U_TEXENV0, 1 );	
	VK_SetUniform1i( p, VK_U_TEXENV1, 1 );
	if ( pStage->bundle[1].image[0] )
	{
		unsigned env = VK_TexEnvForStage( pStage, shader );

		if ( env & VKP_TEXENV_ADD )
		{
			VK_SetUniform1i( p, VK_U_TEXENV1, 2 );		 
		}
		if ( env & VKP_TEXENV_INTERPOLATE )
		{
			VK_SetUniform1i( p, VK_U_TEXENV1, 4 );		 
		}
		if ( env & VKP_TEXENV_COMBINE4 )
		{
			VK_SetUniform1i( p, VK_U_TEXENV1, 5 );		 
		}
	}
	if ( r_lightmap->integer && pStage->bundle[1].isLightmap )
	{
		VK_SetUniform1i( p, VK_U_TEXENV1, 3 );
	}

	VK_SetUniform1i( p, VK_U_ALPHATEST, (int)VK_CurrentAlphaTest() );
	VK_SetUniform4f( p, VK_U_TEX1COLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	VK_SetUniform1i( p, VK_U_ACFF, cpuColor ? 0 : (int)pStage->acff );
}

void VK_SetupFogUniforms( vkProgram_t *p, int fogNum, qboolean fogDensityArm )
{
	float	color[4];
	float	fixColor[3];
	int		densityArm = 0;

	VK_SetUniform1i( p, VK_U_FOGNUM, fogNum );

	fixColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
	fixColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
	fixColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
	VK_SetUniform1f( p, VK_U_FOGSTART, backEnd.viewParms.farplane_bias );
	VK_SetUniform1f( p, VK_U_FOGEND, backEnd.viewParms.farplane_distance );
	VK_SetUniform4f( p, VK_U_FOGFIXCOLOR, fixColor[0], fixColor[1], fixColor[2], 1.0f );
	VK_SetUniform1i( p, VK_U_FOGMODE,
						( vkState.externalSetState & VKS_FOG ) ? 2 : 0 );

	if ( fogNum == 0 || !tr.world )
	{
		VK_SetUniform4f( p, VK_U_FOGCOLOR, 0.0f, 0.0f, 0.0f, 1.0f );
		VK_SetUniform1f( p, VK_U_FOGTCSCALE, 0.0f );
		VK_SetUniform1f( p, VK_U_FOGSCONSTANT, 0.0f );
		VK_SetUniform3f( p, VK_U_FOGSVECTOR, 0.0f, 0.0f, 0.0f );
		VK_SetUniform1f( p, VK_U_FOGTCONSTANT, 1.0f );
		VK_SetUniform3f( p, VK_U_FOGTVECTOR, 0.0f, 0.0f, 0.0f );
		VK_SetUniform1i( p, VK_U_FOGVIEWOUTSIDE, 0 );
		VK_SetUniform1f( p, VK_U_FOGOOLEN, 0.0f );
		VK_SetUniform1i( p, VK_U_FOGDENSITYARM, 0 );
		return;
	}

	if ( fogNum == 9999 )
	{
		unsigned colorInt = (unsigned)tr.viewParms.fogColorInt;

		Com_Memcpy( color, &colorInt, sizeof( colorInt ) );
		VK_SetUniform4fv( p, VK_U_FOGCOLOR, color );
		VK_SetUniform1f( p, VK_U_FOGOOLEN, tr.viewParms.fog.oolen );
		VK_SetUniform1i( p, VK_U_FOGDENSITYARM, fogDensityArm ? 1 : 0 );
		return;
	}

	{
		fog_t	*fog = &tr.world->fogs[fogNum];
		float	tcScale;
		float	sVector[3], tVector[3];
		float	sConstant, tConstant, viewOutside;
		vec3_t	delta;
		unsigned colorInt = fog->colorInt;

		Com_Memcpy( color, &colorInt, sizeof( colorInt ) );
		VK_SetUniform4fv( p, VK_U_FOGCOLOR, color );

		if ( fog->parms.wave.func )
		{
			float wave = EvalWaveForm( &fog->parms.wave );

			if ( wave < 1.0f )
			{
				wave = 1.0f;
			}
			fog->tcScale = 8.0f * ( 1.0f / wave );
		}
		tcScale = fog->tcScale;
		VK_SetUniform1f( p, VK_U_FOGTCSCALE, tcScale );

		VectorSubtract( backEnd.ori.origin, backEnd.viewParms.ori.origin, delta );
		sConstant = DotProduct( delta, backEnd.viewParms.ori.axis[0] ) * tcScale;
		sVector[0] = -backEnd.ori.modelMatrix[3] * tcScale;
		sVector[1] = -backEnd.ori.modelMatrix[7] * tcScale;
		sVector[2] = -backEnd.ori.modelMatrix[11] * tcScale;

		if ( fog->hasSurface )
		{
			tVector[0] = DotProduct( fog->surface, backEnd.ori.axis[0] );
			tVector[1] = DotProduct( fog->surface, backEnd.ori.axis[1] );
			tVector[2] = DotProduct( fog->surface, backEnd.ori.axis[2] );
			tConstant = -fog->surface[3]
				+ DotProduct( fog->surface, backEnd.ori.origin )
				+ DotProduct( backEnd.ori.viewOrigin, tVector );
		}
		else
		{
			tConstant = 1.0f;
			tVector[0] = tVector[1] = tVector[2] = 0.0f;
		}
		viewOutside = ( tConstant < 0.0f ) ? 1.0f : 0.0f;

		VK_SetUniform1f( p, VK_U_FOGSCONSTANT, sConstant );
		VK_SetUniform3fv( p, VK_U_FOGSVECTOR, sVector );
		VK_SetUniform1f( p, VK_U_FOGTCONSTANT, tConstant );
		VK_SetUniform3fv( p, VK_U_FOGTVECTOR, tVector );
		VK_SetUniform1i( p, VK_U_FOGVIEWOUTSIDE, (int)viewOutside );

		{
			float	tVector2[3], planeConst, dView;
			float	*viewOrigin = backEnd.ori.viewOrigin;

			tVector2[0] = DotProduct( fog->surface, backEnd.ori.axis[0] ) * tcScale;
			tVector2[1] = DotProduct( fog->surface, backEnd.ori.axis[1] ) * tcScale;
			tVector2[2] = DotProduct( fog->surface, backEnd.ori.axis[2] ) * tcScale;
			planeConst = ( fog->surface[3]
						   - DotProduct( fog->surface, backEnd.ori.origin ) ) * tcScale;
			dView = ( ( viewOrigin[0] * tVector2[0] + viewOrigin[1] * tVector2[1] )
					  + viewOrigin[2] * tVector2[2] ) - planeConst;

			VK_SetUniform3fv( p, VK_U_FOGDENSITYTVECTOR, tVector2 );
			VK_SetUniform1f( p, VK_U_FOGPLANECONST, planeConst );
			VK_SetUniform1f( p, VK_U_FOGDVIEW, dView );

			if ( fogDensityArm )
			{
				densityArm = ( dView >= 0.0f ) ? 2 : 3;
			}
		}
		VK_SetUniform1f( p, VK_U_FOGOOLEN, tr.viewParms.fog.oolen );
	}

	VK_SetUniform1i( p, VK_U_FOGDENSITYARM, densityArm );
}

/* ---- the wire stream ----------------------------------------------------- */

static struct {
	float	verts[SHADER_MAX_VERTEXES * 2][3];
	byte	colors[SHADER_MAX_VERTEXES * 2][4];
	int		count;
	int		mode;
} s_wire;

void VK_WireBegin( void )
{
	s_wire.count = 0;
}

void VK_WireVertex( const float *xyz, const byte *rgba )
{
	if ( s_wire.count >= SHADER_MAX_VERTEXES * 2 ) {
		return;
	}
	VectorCopy( xyz, s_wire.verts[s_wire.count] );
	if ( rgba ) {
		memcpy( s_wire.colors[s_wire.count], rgba, 4 );
	} else {
		s_wire.colors[s_wire.count][0] = 255;
		s_wire.colors[s_wire.count][1] = 255;
		s_wire.colors[s_wire.count][2] = 255;
		s_wire.colors[s_wire.count][3] = 255;
	}
	s_wire.count++;
}

void VK_WireEnd( int mode )
{
	(void)mode;
	/* the wire families are the debug overlays: the rows name the site */
	if ( s_wire.count ) {
		VK_M3_Touched( "wire overlay draw" );
	}
	s_wire.count = 0;
}

vkProgram_t *VK_BeginFamily( unsigned family, int texEnv )
{
	vkProgram_t *prog = VK_GetProgram( family );

	(void)texEnv;
	VK_BindProgram( prog );
	VK_SetupViewUniforms( prog );
	return prog;
}

void VK_Begin2D( void )
{
	VK_BeginFamily( VKP_FAMILY_2D, 1 );
	VK_SetBaseColor4f( VK_GetProgram( VKP_FAMILY_2D ),
						  backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f,
						  backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f );
}

void VK_BeginWire( void )
{
	VK_BeginFamily( VKP_FAMILY_WIRE, 1 );
	VK_WireBegin();
}

void VK_BeginSprite( void )
{
	VK_BeginFamily( VKP_FAMILY_SPRITE, 1 );
}

void VK_BeginSunflare( void )
{
	VK_BeginFamily( VKP_FAMILY_SUNFLARE, 1 );
}

void VK_BeginShadow( void )
{
	VK_BeginFamily( VKP_FAMILY_SHADOW, 1 );
}

/* ---- the tessellation stream --------------------------------------------- */
/* the vertex the SPIR-V reads: position, normal, colour, two texcoord pairs */
typedef struct {
	float	xyz[4];
	float	normal[4];
	byte	color[4];
	float	st[2][2];
} vkStreamVertex_t;

/* the census that names a draw whose own vertices cover half the frame; it is
   defined below the shade state it reports and called from both tess arms */
static void VK_Shade_FootprintRow( const shaderCommands_t *input, const void *stage,
								   const vkIndex_t *indexes, int numIndexes );

#define VK_STREAM_STRIDE ( (int)sizeof( vkStreamVertex_t ) )

static void VK_Stream_Pack( vkStreamVertex_t *out, const shaderCommands_t *input, int v,
							const float *tc0, const float *tc1, int tcStrideFloats,
							const unsigned char *colors, const float *normals )
{
	const float *xyz = input->xyz[v];

	out->xyz[0] = xyz[0];
	out->xyz[1] = xyz[1];
	out->xyz[2] = xyz[2];
	out->xyz[3] = 1.0f;
	if ( normals ) {
		out->normal[0] = normals[v * 4 + 0];
		out->normal[1] = normals[v * 4 + 1];
		out->normal[2] = normals[v * 4 + 2];
	} else {
		out->normal[0] = 0.0f;
		out->normal[1] = 0.0f;
		out->normal[2] = 1.0f;
	}
	out->normal[3] = 0.0f;
	if ( colors ) {
		memcpy( out->color, colors + v * 4, 4 );
	} else {
		out->color[0] = 255; out->color[1] = 255; out->color[2] = 255; out->color[3] = 255;
	}
	if ( tc0 ) {
		out->st[0][0] = tc0[v * tcStrideFloats + 0];
		out->st[0][1] = tc0[v * tcStrideFloats + 1];
	} else {
		out->st[0][0] = 0.0f; out->st[0][1] = 0.0f;
	}
	if ( tc1 ) {
		out->st[1][0] = tc1[v * tcStrideFloats + 0];
		out->st[1][1] = tc1[v * tcStrideFloats + 1];
	} else {
		out->st[1][0] = 0.0f; out->st[1][1] = 0.0f;
	}
}

void RB_VboStaticReset( void ) { }
void RB_VboStaticRecord( const void *surf, int vertStart, int numVerts, int idxStart, int numIdx )
{
	(void)surf; (void)vertStart; (void)numVerts; (void)idxStart; (void)numIdx;
}
void RB_VboStaticForget( void ) { }
void RB_VBO_DecalAttribProbe( int *enabled, int *size, int *stride, int *type,
							  int *normalized, const void **pointer )
{
	if ( enabled ) { *enabled = 0; }
	if ( size ) { *size = 0; }
	if ( stride ) { *stride = 0; }
	if ( type ) { *type = 0; }
	if ( normalized ) { *normalized = 0; }
	if ( pointer ) { *pointer = NULL; }
}

void RB_VBO_Init( void ) { }
void RB_VBO_Shutdown( void ) { }
void RB_VBO_BeginFrame( void ) { }

/* the draw: pack into the ring slice the batch reserves, one indexed draw */
void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const vkIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals,
							const void *stage )
{
	vkStreamVertex_t	*verts;
	vkIndex_t			*idx;
	int					base = 0, indexBase = 0;
	int					i;
	VkCommandBuffer		cmd;
	long long			walkT0;

	if ( !input || input->numVertexes <= 0 || numIndexes <= 0 ) {
		return;
	}

	cmd = VK_Shade_CommandBuffer();

	/* Retail locks a surface once and draws every stage against it (the traced
	   menu frame: five locks, thirteen draws).  Only the invariant sources can be
	   shared: texcoords straight out of the tess store and no CPU colour array,
  	   which is the case where no stage's recomputation reaches the pack. */
	if ( stage && colors == NULL
		 && texcoord0 == input->texCoords[0][0]
		 && texcoord1 == input->texCoords[0][0] + 2
		 && tcStrideFloats == 4
		 && VK_VertexRing_StreamMatch( input->numVertexes, numIndexes,
									   texcoord0, texcoord1, tcStrideFloats,
									   colors, normals, &base, &indexBase ) ) {
		walkT0 = VK_WALK_T0();
		if ( !VK_Shade_Bind( cmd, input->shader ) ) {
			VK_WALK_ADD( walkT0, VK_WP_BIND );
			VK_VertexRing_Flushed();
			return;
		}
		VK_WALK_ADD( walkT0, VK_WP_BIND );
		VK_Shade_FootprintRow( input, stage, indexes, numIndexes );
		walkT0 = VK_WALK_T0();
		VK_Draw_BindRingOffset( cmd, base );
		VK_Draw_DrawIndexed( cmd, numIndexes, indexBase, 0 );
		VK_WALK_ADD( walkT0, VK_WP_REC );
		VK_VertexRing_Flushed();
		return;
	}

	walkT0 = VK_WALK_T0();
	/* the shade vertex is wider than the 2D tess slot: reserve bytes */
	if ( !VK_VertexRing_ReserveBytes( input->numVertexes * VK_STREAM_STRIDE, &base ) ) {
		VK_WALK_ADD( walkT0, VK_WP_PACK );
		VK_Guard_Hit( VK_GSITE_ALLOC, "tess ring exhausted", VK_INCOMPLETE );
		return;
	}
	verts = ( vkStreamVertex_t * )( ( byte * )VK_VertexRing_Mapped() + base );
	for ( i = 0; i < input->numVertexes; i++ ) {
		VK_Stream_Pack( &verts[i], input, i, texcoord0, texcoord1, tcStrideFloats, colors, normals );
	}
	if ( indexes ) {
		idx = VK_VertexRing_ReserveIndexes( numIndexes, &indexBase );
		if ( !idx ) {
			VK_WALK_ADD( walkT0, VK_WP_PACK );
			VK_Guard_Hit( VK_GSITE_ALLOC, "tess index ring exhausted", VK_INCOMPLETE );
			return;
		}
		memcpy( idx, indexes, numIndexes * sizeof( vkIndex_t ) );
	}
	VK_WALK_ADD( walkT0, VK_WP_PACK );

	if ( stage && colors == NULL
		 && texcoord0 == input->texCoords[0][0]
		 && texcoord1 == input->texCoords[0][0] + 2
		 && tcStrideFloats == 4 ) {
		VK_VertexRing_StreamStore( input->numVertexes, numIndexes,
								   texcoord0, texcoord1, tcStrideFloats,
								   colors, normals, base, indexBase );
	}

	cmd = VK_Shade_CommandBuffer();
	walkT0 = VK_WALK_T0();
	if ( !VK_Shade_Bind( cmd, input->shader ) ) {
			/* nothing is bound: recording the draw anyway faults the driver, and VK_Shade_Bind's rows name the reason */
		VK_WALK_ADD( walkT0, VK_WP_BIND );
		VK_VertexRing_Flushed();
		return;
	}
	VK_WALK_ADD( walkT0, VK_WP_BIND );

	VK_Shade_FootprintRow( input, stage, indexes, numIndexes );
	walkT0 = VK_WALK_T0();
	VK_Draw_BindRingOffset( cmd, base );
	VK_Draw_DrawIndexed( cmd, numIndexes, indexBase, 0 );
	VK_WALK_ADD( walkT0, VK_WP_REC );
	VK_VertexRing_Flushed();
}

void RB_VBO_DrawTessIndexed( const shaderCommands_t *input, const vkIndex_t *indexes, int numIndexes )
{
	RB_VBO_DrawTessIndexed_Stage( input, indexes, numIndexes, NULL );
}

void RB_VBO_DrawTessIndexed_Stage( const shaderCommands_t *input, const vkIndex_t *indexes,
								   int numIndexes, const void *stage )
{
	RB_VBO_DrawTessArrays( input, indexes, numIndexes,
						   input->texCoords[0][0], input->texCoords[0][0] + 2, 4,
						   (const unsigned char *)input->vertexColors, input->normal[0], stage );
}

void RB_VBO_DrawTess( const shaderCommands_t *input )
{
	RB_VBO_DrawTessIndexed( input, input->indexes, input->numIndexes );
}

/* ---- the immediate stream, pipeline and descriptor set -------------------
   One layout: the uniform block at 0, the bundles and wave table at 1..3.

   VK_SHADE_SETS lives in vk_local.h next to the pool budget that carries it.
   The 1024 it replaced was a guess, and the first un-clipped world blew it:
   143,383 "shade descriptor set table exhausted" refusals in one boot.  The
   measurement that sizes it: a set is keyed on the frame slot and the three
   image/sampler pairs a stage binds, the table is boot-persistent because
   those keys are frame- and world-invariant while the views live, and a frame
   can add at most one new key per draw -- so the working set is bounded by
   2 * (draw-surfs in the busiest frame).  The same boot measured 1,887
   draw-surfs in its busiest frame, 244 distinct textures and 284 distinct
   (tex0,tex1) pairs in the sampled stage rows, and it exhausted 1024 -- so
   4096 is four times the failure and twice the measured per-frame ceiling,
   with [VK:shade] and the fill row below still naming the truth if a bigger
   world ever asks for more. */

typedef struct {
	unsigned	stateBits;
	int			family;
	unsigned	defines;
	cullType_t	cull;
	qboolean	polygonOffset;
	int			topology;
	VkPipeline	pipeline;
	qboolean	used;
} vkShadePipeline_t;

/* The pipeline table is the same shape as the set table: keyed on what a draw
   asks for, filled once per distinct answer, boot-persistent, and refusing the
   draw on exhaustion.  Its key is wider -- state bits, family, defines, cull,
   polygon offset, topology -- so its space is the product, not a single
   census: the live boot measured 14 distinct state-bit values and a peak of
   252 shade draws in one frame, and never reached this number, so 4096 is
   headroom over the measured frame working set rather than a proven need.
   VK_Shade_Descriptors_Init's row below names the fill, so a bigger world
   asks for the number it needs instead of a guard row asking for a guess. */
#define VK_SHADE_PIPELINES 4096

static vkShadePipeline_t	vk_shadePipelines[VK_SHADE_PIPELINES];
static int					vk_numShadePipelines;

/* Both cache tables below are looked up by binding, not by index, and both were
   searched by walking every entry they had filled.  The bind cost therefore
   grew with the boot: the operator's session filled the set table to 1792 of
   4096, which is a 1792-entry chain of seven pointer compares on every bind,
   every stage, every surface.  These indexes make a hit O(1).  Each stores the
   entry index plus one, so 0 is "no entry", and the insert uses the same window
   the lookup starts at, so a key that was inserted is always found.  The tables
   hold at most VK_SHADE_SETS/VK_SHADE_PIPELINES entries and the index is twice
   that, so there is always an empty slot and no probe can run off the end. */
#define VK_SHADE_HASH_BITS	13
#define VK_SHADE_HASH_SIZE	( 1 << VK_SHADE_HASH_BITS )
#define VK_SHADE_HASH_MASK	( VK_SHADE_HASH_SIZE - 1 )

static int vk_shadePipelineHash[VK_SHADE_HASH_SIZE];
static int vk_shadeSetHash[VK_SHADE_HASH_SIZE];

static unsigned VK_Shade_HashWords( const uintptr_t *w, int n )
{
	unsigned	h = 2166136261u;
	int			i;

	for ( i = 0; i < n; i++ ) {
		h = ( h ^ (unsigned)( w[i] & 0xffffffffu ) ) * 16777619u;
		h = ( h ^ (unsigned)( ( (uint64_t)w[i] >> 32 ) & 0xffffffffu ) ) * 16777619u;
	}
	return h;
}

static VkDescriptorSetLayout	vk_shadeSetLayout;
static VkPipelineLayout			vk_shadeLayout;
static VkBuffer					vk_shadeUbo[VK_FRAMES_IN_FLIGHT];
static VkDeviceMemory			vk_shadeUboMemory[VK_FRAMES_IN_FLIGHT];
static unsigned char			*vk_shadeUboMapped[VK_FRAMES_IN_FLIGHT];
static VkDescriptorSet			vk_shadeSets[VK_SHADE_SETS];
static int						vk_numShadeSets;

/* The uniform ring: a Vulkan uniform buffer is read at submit time, so each draw takes the next slice and binds it by dynamic offset. */
#define VK_SHADE_UBO_SLICES			4096
#define VK_SHADE_UBO_ALIGN_MAX		256
/* every draw writes the uniforms its own permutation reads; the floor keeps the block's head alive */
#define VK_UBO_HIGHWATER_FLOOR		256
static int						vk_shadeUboSliceBytes = VK_UBO_SIZE_PAD;
static int						vk_shadeUboNext[VK_FRAMES_IN_FLIGHT];

/* What a cached set was written for: it carries the frame slot's UBO too. */
typedef struct {
	int						slot;
	VkImageView				view[3];
	VkSampler				sampler[3];
	/* A set is only a cache hit once every binding its layout declares has been
	   written.  Without this flag a key of all zeros -- frame slot 0 and no
	   texture bound, which a stage with an empty bundle produces -- matches the
	   zeroed slot a failed allocation left behind, and the boot binds a set that
	   was never written: undefined sampler, undefined uniform block. */
	qboolean				written;
} vkShadeSetKey_t;

static vkShadeSetKey_t			vk_shadeSetKeys[VK_SHADE_SETS];
static int						VK_Shade_CommandBufferSlot( void );

qboolean VK_Shade_Descriptors_Init( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo layoutInfo;
	VkPipelineLayoutCreateInfo pipeInfo;
	VkDescriptorSetLayout		setLayouts[1];
	VkPushConstantRange push;
	VkBufferCreateInfo bufferInfo;
	VkMemoryRequirements reqs;
	VkMemoryAllocateInfo allocInfo;
	VkResult result;
	int i;

	memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	for ( i = 1; i < 4; i++ ) {
		bindings[i].binding = i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[i].descriptorCount = 1;
		/* the vertex stage samples too: both stages, or validation rejects */
		bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 4;
	layoutInfo.pBindings = bindings;
	result = qvk.CreateDescriptorSetLayout( vk.device, &layoutInfo, NULL, &vk_shadeSetLayout );
	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR, result, "vkCreateDescriptorSetLayout(stage)" ) ) {
		return qfalse;
	}

	memset( &push, 0, sizeof( push ) );
	push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	push.offset = 0;
	push.size = 16 * sizeof( float );

	memset( &pipeInfo, 0, sizeof( pipeInfo ) );
	pipeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeInfo.setLayoutCount = 1;
	pipeInfo.pSetLayouts = setLayouts;
	setLayouts[0] = vk_shadeSetLayout;
	pipeInfo.pushConstantRangeCount = 1;
	pipeInfo.pPushConstantRanges = &push;
	result = qvk.CreatePipelineLayout( vk.device, &pipeInfo, NULL, &vk_shadeLayout );
	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR, result, "vkCreatePipelineLayout(stage)" ) ) {
		return qfalse;
	}

		/* one uniform ring per frame in flight; the slice stride is the block rounded to the dynamic-offset alignment */
	vk_shadeUboSliceBytes = ( VK_UBO_SIZE_PAD + VK_SHADE_UBO_ALIGN_MAX - 1 ) & ~( VK_SHADE_UBO_ALIGN_MAX - 1 );
	if ( qvk.GetPhysicalDeviceProperties && vk.physicalDevice ) {
		int align;

		qvk.GetPhysicalDeviceProperties( vk.physicalDevice, &vk.deviceProps );
		align = (int)vk.deviceProps.limits.minUniformBufferOffsetAlignment;
		if ( align > 1 ) {
			vk_shadeUboSliceBytes = ( VK_UBO_SIZE_PAD + align - 1 ) & ~( align - 1 );
		}
	}

	memset( &bufferInfo, 0, sizeof( bufferInfo ) );
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = ( VkDeviceSize )vk_shadeUboSliceBytes * VK_SHADE_UBO_SLICES;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		result = qvk.CreateBuffer( vk.device, &bufferInfo, NULL, &vk_shadeUbo[i] );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkCreateBuffer(ubo)" ) ) {
			return qfalse;
		}
		qvk.GetBufferMemoryRequirements( vk.device, vk_shadeUbo[i], &reqs );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = VK_Memory_FindType( reqs.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		result = qvk.AllocateMemory( vk.device, &allocInfo, NULL, &vk_shadeUboMemory[i] );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkAllocateMemory(ubo)" ) ) {
			return qfalse;
		}
		result = qvk.BindBufferMemory( vk.device, vk_shadeUbo[i], vk_shadeUboMemory[i], 0 );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkBindBufferMemory(ubo)" ) ) {
			return qfalse;
		}
		result = qvk.MapMemory( vk.device, vk_shadeUboMemory[i], 0, bufferInfo.size, 0, (void **)&vk_shadeUboMapped[i] );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkMapMemory(ubo)" ) ) {
			return qfalse;
		}
		vk_shadeUboNext[i] = 0;
	}

	VK_Guard_Row( "VK:shade", "descriptors ready sets=%d ubo=%d slices=%d stride=%d",
				  VK_SHADE_SETS, VK_UBO_SIZE_PAD, VK_SHADE_UBO_SLICES, vk_shadeUboSliceBytes );
	return qtrue;
}

void VK_Shade_Descriptors_Shutdown( void )
{
	int i;

	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		if ( vk_shadeUboMapped[i] ) {
			qvk.UnmapMemory( vk.device, vk_shadeUboMemory[i] );
			vk_shadeUboMapped[i] = NULL;
		}
		if ( vk_shadeUboMemory[i] != VK_NULL_HANDLE ) {
			qvk.FreeMemory( vk.device, vk_shadeUboMemory[i], NULL );
			vk_shadeUboMemory[i] = VK_NULL_HANDLE;
		}
		if ( vk_shadeUbo[i] != VK_NULL_HANDLE ) {
			qvk.DestroyBuffer( vk.device, vk_shadeUbo[i], NULL );
			vk_shadeUbo[i] = VK_NULL_HANDLE;
		}
	}
	if ( vk_shadeLayout != VK_NULL_HANDLE ) {
		qvk.DestroyPipelineLayout( vk.device, vk_shadeLayout, NULL );
		vk_shadeLayout = VK_NULL_HANDLE;
	}
	if ( vk_shadeSetLayout != VK_NULL_HANDLE ) {
		qvk.DestroyDescriptorSetLayout( vk.device, vk_shadeSetLayout, NULL );
		vk_shadeSetLayout = VK_NULL_HANDLE;
	}
	/* the pipelines bind this layout, so they go first: leaving them live into vkDestroyDevice is the leak validation names */
	for ( i = 0; i < vk_numShadePipelines; i++ ) {
		if ( vk_shadePipelines[i].used && vk_shadePipelines[i].pipeline != VK_NULL_HANDLE ) {
			qvk.DestroyPipeline( vk.device, vk_shadePipelines[i].pipeline, NULL );
			vk_shadePipelines[i].pipeline = VK_NULL_HANDLE;
			vk_shadePipelines[i].used = qfalse;
		}
	}
	vk_numShadePipelines = 0;
	vk_numShadeSets = 0;
	/* The sets died with the pool above them.  Leaving the handles behind let a
	   re-init skip the allocate on a stale handle and bind a set from a pool
	   that no longer exists. */
	memset( vk_shadeSets, 0, sizeof( vk_shadeSets ) );
	memset( vk_shadeSetKeys, 0, sizeof( vk_shadeSetKeys ) );
	/* The indexes point into the tables above: leaving them behind would make a
	   re-init return a handle from the pool this call just destroyed. */
	memset( vk_shadePipelineHash, 0, sizeof( vk_shadePipelineHash ) );
	memset( vk_shadeSetHash, 0, sizeof( vk_shadeSetHash ) );
}

int VK_Shade_CommandBufferSlot( void )
{
	/* The slot the walk opened the pass on, which under r_smp 1 is the frame the
	   render thread is walking, not the frame the front end is already recording:
	   tr.smpFrame has flipped to the next frame by then. */
	return RB_FrameSmpFrame();
}

VkCommandBuffer VK_Shade_CommandBuffer( void )
{
	int slot = VK_Shade_CommandBufferSlot();

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT ) {
		slot = 0;
	}
	return vk.commandBuffers[slot];
}

/* copy the staging block into the next ring slice; -1 when the ring is out */
static VkDeviceSize VK_Shade_WriteUBO( void )
{
	int			slot = VK_Shade_CommandBufferSlot();
	int			slice;
	VkDeviceSize offset = 0;

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT || !vk_shadeUboMapped[slot] ) {
		return ( VkDeviceSize )-1;
	}

	slice = vk_shadeUboNext[slot];
	if ( slice >= VK_SHADE_UBO_SLICES ) {
		static int s_rows;

		if ( s_rows < 8 || ( s_rows % 600 ) == 0 ) {
			VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "uniform ring exhausted for this frame", VK_INCOMPLETE );
		}
		s_rows++;
		return ( VkDeviceSize )-1;
	}
	vk_shadeUboNext[slot] = slice + 1;

	offset = ( VkDeviceSize )slice * vk_shadeUboSliceBytes;
	memcpy( vk_shadeUboMapped[slot] + offset, s_ubo, s_uboHighWater );
	s_uboDirty = qfalse;
	s_uboHighWater = VK_UBO_HIGHWATER_FLOOR;

	return offset;
}

/* the cull mode the shader's cullType and the mirror flag ask for */
static VkCullModeFlagBits VK_Shade_CullMode( cullType_t cull, qboolean mirrored )
{
	switch ( cull ) {
	case CT_TWO_SIDED:
		return VK_CULL_MODE_NONE;
	case CT_BACK_SIDED:
		return mirrored ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
	case CT_FRONT_SIDED:
	default:
		return mirrored ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
	}
}

static VkBlendFactor VK_Shade_BlendFactor( unsigned nibble, qboolean src )
{
	switch ( nibble ) {
	case VKS_SRCBLEND_ZERO:					return VK_BLEND_FACTOR_ZERO;
	case VKS_SRCBLEND_ONE:					return VK_BLEND_FACTOR_ONE;
	case VKS_SRCBLEND_DST_COLOR:			return VK_BLEND_FACTOR_DST_COLOR;
	case VKS_SRCBLEND_ONE_MINUS_DST_COLOR:	return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case VKS_SRCBLEND_SRC_ALPHA:			return VK_BLEND_FACTOR_SRC_ALPHA;
	case VKS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case VKS_SRCBLEND_DST_ALPHA:			return VK_BLEND_FACTOR_DST_ALPHA;
	case VKS_SRCBLEND_ONE_MINUS_DST_ALPHA:	return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case VKS_SRCBLEND_ALPHA_SATURATE:		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	default:
		return src ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
	}
}

void VK_Shade_PermRow( const char *shader, int family, unsigned defines );

static VkPipeline VK_Shade_Pipeline( const char *shader, unsigned stateBits, int family,
									 unsigned defines, cullType_t cull,
									 qboolean polygonOffset, int topology )
{
	VkGraphicsPipelineCreateInfo info;
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attribs[5];
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo assembly;
	VkPipelineViewportStateCreateInfo viewport;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineDepthStencilStateCreateInfo depth;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineColorBlendAttachmentState attachment;
	VkDynamicState dynamicStates[2];
	VkPipelineDynamicStateCreateInfo dynamic;
	VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
	vkShadePipeline_t	*slot = NULL;
	VkResult result;
	unsigned srcNibble, dstNibble;
	uint32_t attribCount;
	uintptr_t pkey[6];
	unsigned phash;
	int newIndex;
	int probe;

	/* the cache key names the modules, so it keys on the permutation bits alone */
	defines &= ~VKP_FAMILY_MASK;

	if ( !VK_Program_Modules( family, defines, &vert, &frag ) ) {
		/* the mask names the permutation the offline build has to add, and the
		   shader names where the content asked for it: a next gap is one boot
		   from diagnosed instead of one census from inferred */
		VK_Shade_NotePerm( 1 );
		VK_Shade_PermRow( shader, family, defines );
		return VK_NULL_HANDLE;
	}
	VK_Shade_NotePerm( 0 );

	pkey[0] = stateBits;      pkey[1] = (uintptr_t)family;
	pkey[2] = defines;        pkey[3] = (uintptr_t)cull;
	pkey[4] = polygonOffset;  pkey[5] = (uintptr_t)topology;
	phash = VK_Shade_HashWords( pkey, 6 ) & VK_SHADE_HASH_MASK;

	for ( probe = 0; probe < VK_SHADE_HASH_SIZE; probe++ ) {
		int at = vk_shadePipelineHash[( phash + probe ) & VK_SHADE_HASH_MASK];

		if ( !at ) {
			break;
		}
		at--;
		if ( vk_shadePipelines[at].used && vk_shadePipelines[at].stateBits == stateBits
			 && vk_shadePipelines[at].family == family && vk_shadePipelines[at].defines == defines
			 && vk_shadePipelines[at].cull == cull
			 && vk_shadePipelines[at].polygonOffset == polygonOffset
			 && vk_shadePipelines[at].topology == topology ) {
			return vk_shadePipelines[at].pipeline;
		}
	}
	if ( vk_numShadePipelines >= VK_SHADE_PIPELINES ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "shade pipeline table full", VK_INCOMPLETE );
		return VK_NULL_HANDLE;
	}
	newIndex = vk_numShadePipelines;
	slot = &vk_shadePipelines[newIndex];

	memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.stride = VK_STREAM_STRIDE;
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	memset( attribs, 0, sizeof( attribs ) );
	attribs[0].location = ATTR_POSITION; attribs[0].binding = 0;
	attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT; attribs[0].offset = 0;
	attribs[1].location = ATTR_NORMAL; attribs[1].binding = 0;
	attribs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT; attribs[1].offset = 16;
	attribs[2].location = ATTR_COLOR; attribs[2].binding = 0;
	attribs[2].format = VK_FORMAT_R8G8B8A8_UNORM; attribs[2].offset = 32;
	attribs[3].location = ATTR_TEXCOORD0; attribs[3].binding = 0;
	attribs[3].format = VK_FORMAT_R32G32_SFLOAT; attribs[3].offset = 36;
	attribs[4].location = ATTR_TEXCOORD1; attribs[4].binding = 0;
	attribs[4].format = VK_FORMAT_R32G32_SFLOAT; attribs[4].offset = 44;

	/* Declare only the locations this permutation's vertex stage reads.  The
	   ring's stride is fixed, so the binding and the offsets are unchanged. */
	{
		const vkShaderModuleDef_t *vdef = VK_Program_Find( family, defines, 0 );
		unsigned int mask = vdef ? VK_Pipeline_VertexInputMask( vdef->spv, vdef->words )
								 : 0xFFFFFFFFu;

		attribCount = VK_Pipeline_TrimAttributes( attribs, 5, mask );
	}

	memset( &vertexInput, 0, sizeof( vertexInput ) );
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = attribCount;
	vertexInput.pVertexAttributeDescriptions = attribs;

	memset( &assembly, 0, sizeof( assembly ) );
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = ( topology == VK_IMM_TRIANGLE_STRIP ) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
						: ( topology == VK_IMM_LINE_LOOP || topology == VK_IMM_LINES )
						? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	memset( &viewport, 0, sizeof( viewport ) );
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	memset( &raster, 0, sizeof( raster ) );
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_Shade_CullMode( cull, qfalse );
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.lineWidth = 1.0f;
	raster.depthClampEnable = VK_FALSE;
	raster.depthBiasEnable = polygonOffset ? VK_TRUE : VK_FALSE;
	raster.depthBiasConstantFactor = r_offsetFactor ? r_offsetFactor->value : -1.0f;
	raster.depthBiasSlopeFactor = r_offsetUnits ? r_offsetUnits->value : -2.0f;

	memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	memset( &depth, 0, sizeof( depth ) );
	depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth.depthTestEnable = ( stateBits & VKS_DEPTHTEST_DISABLE ) ? VK_FALSE : VK_TRUE;
	depth.depthWriteEnable = ( stateBits & VKS_DEPTHMASK_TRUE ) ? VK_TRUE : VK_FALSE;
	depth.depthCompareOp = ( stateBits & VKS_DEPTHFUNC_EQUAL ) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
	depth.minDepthBounds = 0.0f;
	depth.maxDepthBounds = 1.0f;

	srcNibble = ( stateBits & VKS_SRCBLEND_BITS ) >> 0;
	dstNibble = ( stateBits & VKS_DSTBLEND_BITS ) >> 4;
	memset( &attachment, 0, sizeof( attachment ) );
	attachment.blendEnable = ( srcNibble != VKS_SRCBLEND_ONE || dstNibble != VKS_DSTBLEND_ZERO ) ? VK_TRUE : VK_FALSE;
	attachment.srcColorBlendFactor = VK_Shade_BlendFactor( srcNibble, qtrue );
	attachment.dstColorBlendFactor = VK_Shade_BlendFactor( dstNibble, qfalse );
	attachment.colorBlendOp = VK_BLEND_OP_ADD;
	attachment.srcAlphaBlendFactor = VK_Shade_BlendFactor( srcNibble, qtrue );
	attachment.dstAlphaBlendFactor = VK_Shade_BlendFactor( dstNibble, qfalse );
	attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
								| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &attachment;

	memset( &dynamic, 0, sizeof( dynamic ) );
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
	dynamic.dynamicStateCount = 2;
	dynamic.pDynamicStates = dynamicStates;

	memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vert;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = frag;
	stages[1].pName = "main";

	memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount = 2;
	info.pStages = stages;
	info.pVertexInputState = &vertexInput;
	info.pInputAssemblyState = &assembly;
	info.pViewportState = &viewport;
	info.pRasterizationState = &raster;
	info.pMultisampleState = &multisample;
	info.pDepthStencilState = &depth;
	info.pColorBlendState = &blend;
	info.pDynamicState = &dynamic;
	info.layout = vk_shadeLayout;
	info.renderPass = vk.renderPass;
	info.subpass = 0;

	result = qvk.CreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &info, NULL, &slot->pipeline );
	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR, result, "vkCreateGraphicsPipelines(shade)" ) ) {
		return VK_NULL_HANDLE;
	}
	slot->stateBits = stateBits;
	slot->family = family;
	slot->defines = defines;
	slot->cull = cull;
	slot->polygonOffset = polygonOffset;
	slot->topology = topology;
	slot->used = qtrue;
	for ( probe = 0; probe < VK_SHADE_HASH_SIZE; probe++ ) {
		int *at = &vk_shadePipelineHash[( phash + probe ) & VK_SHADE_HASH_MASK];

		if ( !*at ) {
			*at = newIndex + 1;
			break;
		}
	}
	vk_numShadePipelines++;
	if ( ( vk_numShadePipelines & 15 ) == 1 ) {
		VK_Guard_Row( "VK:shade", "pipelines=%d of %d sets=%d of %d",
					  vk_numShadePipelines, VK_SHADE_PIPELINES, vk_numShadeSets, VK_SHADE_SETS );
	}
	return slot->pipeline;
}

/* the descriptor set for the (bundle0, bundle1, wave) triple, cached on those views */
static qboolean VK_Shade_DefaultImage( VkImageView *view, VkSampler *sampler )
{
	static VkImageView s_view;
	static VkSampler s_sampler;

	if ( s_view != VK_NULL_HANDLE && s_sampler != VK_NULL_HANDLE ) {
		*view = s_view;
		*sampler = s_sampler;
		return qtrue;
	}

	if ( !tr.whiteImage ) {
		return qfalse;
	}

	s_view = VK_Texture_View( (int)tr.whiteImage->texnum );
	if ( s_view == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( !VK_Texture_Sampler( qtrue, qfalse, qtrue, &s_sampler ) ) {
		return qfalse;
	}

	*view = s_view;
	*sampler = s_sampler;
	return qtrue;
}

static VkDescriptorSet VK_Shade_Set( VkImageView v0, VkImageView v1, VkImageView wave,
									 VkSampler waveSampler )
{
	VkDescriptorImageInfo images[3];
	VkDescriptorBufferInfo buffer;
	VkWriteDescriptorSet writes[4];
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorSet set;
	VkImageView defView;
	VkSampler defSampler;
	VkResult result;
	vkShadeSetKey_t key;
	uintptr_t skey[7];
	unsigned shash;
	int i, found = -1, probe;

	memset( &key, 0, sizeof( key ) );
	key.slot = VK_Shade_CommandBufferSlot();
	key.view[0] = v0;      key.view[1] = v1;      key.view[2] = wave;
	key.sampler[0] = v0 ? s_shade.texSampler[0] : VK_NULL_HANDLE;
	key.sampler[1] = v1 ? s_shade.texSampler[1] : VK_NULL_HANDLE;
	key.sampler[2] = wave ? waveSampler : VK_NULL_HANDLE;

	skey[0] = (uintptr_t)key.slot;
	skey[1] = (uintptr_t)key.view[0];      skey[2] = (uintptr_t)key.view[1];
	skey[3] = (uintptr_t)key.view[2];
	skey[4] = (uintptr_t)key.sampler[0];   skey[5] = (uintptr_t)key.sampler[1];
	skey[6] = (uintptr_t)key.sampler[2];
	shash = VK_Shade_HashWords( skey, 7 ) & VK_SHADE_HASH_MASK;

	for ( probe = 0; probe < VK_SHADE_HASH_SIZE; probe++ ) {
		int at = vk_shadeSetHash[( shash + probe ) & VK_SHADE_HASH_MASK];

		if ( !at ) {
			break;
		}
		at--;
		if ( !vk_shadeSetKeys[at].written ) {
			continue;
		}
		if ( vk_shadeSetKeys[at].slot == key.slot
			 && vk_shadeSetKeys[at].view[0] == key.view[0] && vk_shadeSetKeys[at].view[1] == key.view[1]
			 && vk_shadeSetKeys[at].view[2] == key.view[2]
			 && vk_shadeSetKeys[at].sampler[0] == key.sampler[0]
			 && vk_shadeSetKeys[at].sampler[1] == key.sampler[1]
			 && vk_shadeSetKeys[at].sampler[2] == key.sampler[2] ) {
			return vk_shadeSets[at];
		}
	}
	if ( vk_numShadeSets >= VK_SHADE_SETS ) {
		/* Exhaustion refuses the draw rather than aliasing a live set.  Slot 0
		   is already bound inside the command buffer being recorded: rewriting
		   it here retargets every earlier draw that bound it, and storing the
		   new key over vk_shadeSetKeys[0] makes the original key miss forever,
		   so slot 0 then thrashes for the rest of the boot.  VK_NULL_HANDLE is
		   what VK_Shade_Pipeline, the UBO ring and the vertex ring all return
		   when a table runs out, and VK_Shade_Bind turns it into the same
		   s_frameShadeDrops++ refusal -- the surface drops, the frame stays
		   correct, and the guard row names the reason. */
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "shade descriptor set table exhausted", VK_INCOMPLETE );
		return VK_NULL_HANDLE;
	}
	/* The table slot is only taken once the set is complete.  Publishing the
	   handle before the writes left a slot a later all-zero key could hit, and
	   binding a set with an unwritten descriptor is undefined sampling and an
	   undefined uniform block -- the RenderDoc capture put four of those on
	   single draws. */
	if ( vk_shadeSets[vk_numShadeSets] != VK_NULL_HANDLE ) {
		set = vk_shadeSets[vk_numShadeSets];
	} else {
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vk.descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &vk_shadeSetLayout;
		result = qvk.AllocateDescriptorSets( vk.device, &allocInfo, &set );
		if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR, result, "vkAllocateDescriptorSets(shade)" ) ) {
			return VK_NULL_HANDLE;
		}
	}
	found = vk_numShadeSets;

	memset( &buffer, 0, sizeof( buffer ) );
	buffer.buffer = vk_shadeUbo[VK_Shade_CommandBufferSlot()];
	/* the offset moves per draw; the bind carries it */
	buffer.offset = 0;
	buffer.range = VK_UBO_SIZE_PAD;

	memset( &images, 0, sizeof( images ) );
			/* an unbound slot still has to be a legal binding: the white image */
	if ( !VK_Shade_DefaultImage( &defView, &defSampler ) ) {
		/* nothing was written and no slot was taken: the caller refuses the
		   draw and the next call starts clean */
		return VK_NULL_HANDLE;
	}

	images[0].imageView = v0 ? v0 : defView;
	images[0].sampler = v0 ? s_shade.texSampler[0] : defSampler;
	images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	images[1].imageView = v1 ? v1 : defView;
	images[1].sampler = v1 ? s_shade.texSampler[1] : defSampler;
	images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	images[2].imageView = wave ? wave : defView;
	images[2].sampler = wave ? waveSampler : defSampler;
	images[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 3; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = set;
		writes[i].dstBinding = i + 1;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &images[i];
	}
	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = set;
	writes[3].dstBinding = 0;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	writes[3].pBufferInfo = &buffer;
	qvk.UpdateDescriptorSets( vk.device, 4, writes, 0, NULL );

	vk_shadeSets[found] = set;
	vk_numShadeSets = found + 1;
	vk_shadeSetKeys[found] = key;
	vk_shadeSetKeys[found].written = qtrue;
	for ( probe = 0; probe < VK_SHADE_HASH_SIZE; probe++ ) {
		int *at = &vk_shadeSetHash[( shash + probe ) & VK_SHADE_HASH_MASK];

		if ( !*at ) {
			*at = found + 1;
			break;
		}
	}
	{
		/* The working set, measured rather than guessed: a set is created once
		   per distinct (frame slot, three views, three samplers) key and lives
		   for the boot, so this count is the number the table has to hold.  It
		   is what sized VK_SHADE_SETS and it is what a bigger world answers. */
		static int s_rows;

		s_rows++;
		if ( s_rows < 4 || ( vk_numShadeSets & 255 ) == 0 || vk_numShadeSets == VK_SHADE_SETS ) {
			VK_Guard_Row( "VK:shade", "sets=%d of %d", vk_numShadeSets, VK_SHADE_SETS );
		}
	}
	return set;
}

/* The frame's draw census: retail's trace answers "is the 3D draw list alive" with a count per frame. */
static int s_frameShadeDraws;   /* surface batches the walk completed */
static int s_frameShadeDrops;   /* binds VK_Shade_Bind refused */
static int s_frameImmDraws;     /* 2D batches */
static int s_frameSurfCmds;     /* RC_DRAW_SURFS commands the walk performed */
static int s_frameDrawSurfs;    /* draw surfs those commands carried */
static int s_framePermReq;      /* permutation lookups */
static int s_framePermMiss;     /* lookups with no SPIR-V for the key */
static int s_framePermMissRows; /* [VK:perm] rows this frame */

/* The shader clock, one row per animated shader per second: the fields that
   settle whether an animation runs at real seconds.  `tess.shaderTime` is the
   value the stage's tcMod/gen actually advances on; `floatTime` is
   refdef.time*0.001 and `start` is the entity/sprite/world shaderStartTime the
   ANIMATE_ONCE arm subtracts.  A stage that advances ~1.0 per second of
   refdef time is right whatever its unit; one that advances ~1000 is a unit
   mix, and the tree's own producers disagree -- the model and tempmodel paths
   write seconds (cg.time/1000.0) while the sprite path writes a negated
   millisecond value, which the Mac CG_ModelAnim @0x00034F00 then divides by a
   per-entity float.  The row names which of them the stage in front of the
   operator is using. */
void VK_Shade_NoteShaderTime( const char *kind )
{
	static const void	*s_seenShader[24];
	static int			s_seenSecond[24];
	static int			s_seenCount;
	static int			s_rows;
	const shader_t		*sh = tess.shader;
	const shaderStage_t	*st;
	int					second;
	int					i;

	if ( !sh ) {
		return;
	}

	second = (int)( backEnd.refdef.time / 1000 );

	for ( i = 0; i < s_seenCount; i++ ) {
		if ( s_seenShader[i] == (const void *)sh && s_seenSecond[i] == second ) {
			return;
		}
	}

	if ( s_seenCount < (int)( sizeof( s_seenShader ) / sizeof( s_seenShader[0] ) ) ) {
		s_seenShader[s_seenCount] = sh;
		s_seenSecond[s_seenCount] = second;
		s_seenCount++;
	} else {
		i                                = s_rows % (int)( sizeof( s_seenShader ) / sizeof( s_seenShader[0] ) );
		s_seenShader[i]                  = sh;
		s_seenSecond[i]                  = second;
	}

	st = tess.xstages[0];

	s_rows++;
	if ( s_rows > 600 ) {
		return;
	}

	VK_Guard_Row( "VK:shadertime",
				  "shader='%s' kind=%s animate_once=%d floatTime=%.3f start=%.3f "
				  "tessShaderTime=%.3f passes=%d tcMod0=%d tcMod1=%d rgbGen=%d alphaGen=%d refdefTime=%d",
				  sh->name,
				  kind,
				  ( sh->flags & BUNDLE_ANIMATE_ONCE ) ? 1 : 0,
				  (double)backEnd.refdef.floatTime,
				  (double)backEnd.shaderStartTime,
				  (double)tess.shaderTime,
				  sh->numUnfoggedPasses,
				  st ? st->bundle[0].numTexMods : 0,
				  st ? st->bundle[1].numTexMods : 0,
				  st ? (int)st->rgbGen : -1,
				  st ? (int)st->alphaGen : -1,
				  backEnd.refdef.time );
}

void VK_Shade_NoteDraw( int shade )
{
	if ( shade ) {
		s_frameShadeDraws++;
	} else {
		s_frameImmDraws++;
	}
}

/* the frame the census is mid-way through: the harness reads it beside the readback */
int VK_Shade_FrameBatches( void )
{
	return s_frameShadeDraws;
}

void VK_Shade_NoteSurfCmds( int surfs )
{
	s_frameSurfCmds++;
	s_frameDrawSurfs += surfs;
}

/* The frame's stage census, per shader: retail's traced menu frame draws the
   dome as five surfaces of 3, 3, 3, 2 and 2 stages, thirteen draws */
#define VK_STAGE_CENSUS_MAX	64
/* wide enough for every name the census can hold; VK_Shade_StageRows stops
   explicitly and says "+N more" rather than filling silently */
#define VK_STAGE_ROW_BYTES 4096

typedef struct {
	const char *name;
	int         stages;
} vkStageCensus_t;

static vkStageCensus_t s_frameStages[VK_STAGE_CENSUS_MAX];
static int             s_frameStageCount;
static int             s_frameStageDropped;

void VK_Shade_NoteStage( const shader_t *shader )
{
	int i;

	for ( i = 0; i < s_frameStageCount; i++ ) {
		if ( s_frameStages[i].name == shader->name ) {
			s_frameStages[i].stages++;
			return;
		}
	}
	if ( s_frameStageCount < ( int )( sizeof( s_frameStages ) / sizeof( s_frameStages[0] ) ) ) {
		s_frameStages[s_frameStageCount].name   = shader->name;
		s_frameStages[s_frameStageCount].stages = 1;
		s_frameStageCount++;
	} else {
		/* a census that silently stops taking names reads as a complete census:
		   the frame that drew 64 distinct shaders rowed 22 and said nothing */
		s_frameStageDropped++;
	}
}

/* One row per shader the frame drew stages for, only while the set holds still.
   An in-game frame draws far more than eight distinct shaders once the HUD and a
   menu join the world, and a census that holds eight names the world and drops
   the 2D draws a binding question is about.  The tail is counted, not printed. */
static void VK_Shade_StageRows( void )
{
	static char last[VK_STAGE_ROW_BYTES];
	char        now[VK_STAGE_ROW_BYTES];
	int         i, written, omitted = 0;

	if ( !s_frameStageCount ) {
		return;
	}

	/* the counts lead, so a row that has to stop short still names what it
	   omitted; the buffer used to fill under Q_strcat and stop mid-entry with no
	   marker at all */
	Q_strncpyz( now, va( "frame shaders=%d dropped=%d names=",
						 s_frameStageCount, s_frameStageDropped ),
				sizeof( now ) );
	written = (int)strlen( now );

	for ( i = 0; i < s_frameStageCount; i++ ) {
		char entry[VK_STAGE_ROW_BYTES / 4];
		int  len;

		Com_sprintf( entry, sizeof( entry ), "%s%s=%d", i ? " " : "",
					 s_frameStages[i].name ? s_frameStages[i].name : "?",
					 s_frameStages[i].stages );
		len = (int)strlen( entry );
		if ( written + len + 1 > (int)sizeof( now ) ) {
			omitted = s_frameStageCount - i;
			break;
		}
		memcpy( now + written, entry, (size_t)len + 1 );
		written += len;
	}

	if ( omitted ) {
		Q_strcat( now, sizeof( now ), va( " +%d more", omitted ) );
	}

	{
		static int rows;

		rows++;
		if ( rows <= 3 || Q_stricmp( now, last ) ) {
			VK_Guard_Row( "VK:stages", "%s", now );
			Q_strncpyz( last, now, sizeof( last ) );
		}
	}
}

void VK_Shade_NotePerm( int miss )
{
	s_framePermReq++;
	if ( miss ) {
		s_framePermMiss++;
	}
}

/* The census the boot answers: which permutation keys were requested and
   whether each resolved -- a miss is visible only if a row carries it, and the
   row names the surface that asked, so the offline census that has to gain the
   mask is read off the boot rather than inferred from the family byte. */
void VK_Shade_PermRow( const char *shader, int family, unsigned defines )
{
	if ( s_framePermMissRows < 256 || ( s_framePermMissRows % 1000 ) == 0 ) {
		VK_Guard_Row( "VK:perm", "shader='%s' key family=%d defines=0x%06x status=miss batches=%d",
					  shader ? shader : "(null)", family, defines, s_frameShadeDraws );
	}
	s_framePermMissRows++;
}

/* The bind carries a stage's program and textures and answers whether a pipeline is bound. */
qboolean VK_Shade_Bind( VkCommandBuffer cmd, const shader_t *shader )
{
	VkImageView v0 = VK_NULL_HANDLE, v1 = VK_NULL_HANDLE, wave = VK_NULL_HANDLE;
	VkSampler waveSampler = VK_NULL_HANDLE;
	VkPipeline pipeline;
	VkDescriptorSet set;
	uint32_t uboOffset = 0;
	int family;

	(void)shader;
	if ( !s_shade.prog ) {
		s_frameShadeDrops++;
		return qfalse;
	}
	family = VK_Family_Of( s_shade.prog->defines );
	pipeline = VK_Shade_Pipeline( shader ? shader->name : "(null)", s_shade.stateBits, family,
								  s_shade.prog->defines,
								  s_shade.cull, s_shade.polygonOffset, s_shade.topology );
	if ( pipeline == VK_NULL_HANDLE ) {
		s_frameShadeDrops++;
		return qfalse;
	}
	if ( s_shade.texBound[0] ) {
		v0 = s_shade.texView[0];
	}
	if ( s_shade.texBound[1] ) {
		v1 = s_shade.texView[1];
	}
	/* created with the first draw that reads it; the shaders fetch single texels, so the sampler is nearest */
	VK_CreateWaveTable();
	if ( s_waveTableHandle >= 0 ) {
		wave = VK_Texture_View( s_waveTableHandle );
		if ( wave != VK_NULL_HANDLE && !VK_Texture_Sampler( qtrue, qfalse, qfalse, &waveSampler ) ) {
			waveSampler = VK_NULL_HANDLE;
			wave = VK_NULL_HANDLE;
		}
	}
	set = VK_Shade_Set( v0, v1, wave, waveSampler );
	if ( set == VK_NULL_HANDLE ) {
		s_frameShadeDrops++;
		return qfalse;
	}
	{
		VkDeviceSize slice = VK_Shade_WriteUBO();

		if ( slice == ( VkDeviceSize )-1 ) {
			s_frameShadeDrops++;
			return qfalse;
		}
		uboOffset = ( uint32_t )slice;
	}
	qvk.CmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	qvk.CmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_shadeLayout, 0, 1, &set,
							   1, &uboOffset );
	return qtrue;
}


/* ---- the sub-stream shims the mirrored front end calls ------------------- */

static qboolean s_polygonOffset;
static qboolean s_stencilTest;

qboolean VK_Shade_PolygonOffset( void )
{
	return s_polygonOffset;
}

void VK_Shade_SetStencil( qboolean test, int func, int ref, unsigned int mask )
{
	s_stencilTest = test;
	VK_StencilState( test, func, ref, mask );
}

void vkEnable( int cap )
{
	if ( cap == VK_POLYGON_OFFSET_FILL ) {
		s_polygonOffset = qtrue;
		s_shade.polygonOffset = qtrue;
	} else if ( cap == VK_CULL_FACE ) {
		/* cull is a pipeline state word; VK_Cull carries it */
	}
}

void vkDisable( int cap )
{
	if ( cap == VK_POLYGON_OFFSET_FILL ) {
		s_polygonOffset = qfalse;
		s_shade.polygonOffset = qfalse;
	}
}

void vkCullFace( int mode )
{
	/* the shadow passes flip the cull between their two volumes */
	(void)mode;
}

void vkStencilOp( int fail, int zfail, int zpass )
{
	(void)fail; (void)zfail; (void)zpass;
}

void vkColorMask( qboolean r, qboolean g, qboolean b, qboolean a )
{
	s_shade.colorMask[0] = r;
	s_shade.colorMask[1] = g;
	s_shade.colorMask[2] = b;
	s_shade.colorMask[3] = a;
}

void vkLineWidth( float width )
{
	VkCommandBuffer cmd = VK_Shade_CommandBuffer();

	/* 1.0 is all a pipeline without the wideLines feature accepts; the 3D path draws no lines yet */
	if ( cmd != VK_NULL_HANDLE && width > 1.0f ) {
		static int s_wideRows;
		if ( s_wideRows < 4 ) {
			VK_Guard_Hit( VK_GSITE_QUIRK, "line width needs the wideLines feature", VK_INCOMPLETE );
			s_wideRows++;
		}
	}
}

/* ---- the legacy depth-range seam ------------------------------------- */

/* the rect the 3D view set, so vkDepthRange can re-issue it */
static int s_view3D[4];
static qboolean s_view3DValid;

void VK_Shade_Viewport3D( int x, int y, int w, int h )
{
	s_view3D[0] = x;
	s_view3D[1] = y;
	s_view3D[2] = w;
	s_view3D[3] = h;
	s_view3DValid = ( w > 0 && h > 0 ) ? qtrue : qfalse;
}

/* the depth range maps to the viewport's minDepth/maxDepth: the only form that reproduces the sky's (0,0) pin */
static void VK_Shade_DepthRange( float n, float f )
{
	int				x, y, w, h;
	VkCommandBuffer cmd = VK_Shade_CommandBuffer();

	if ( cmd == VK_NULL_HANDLE ) {
		return;
	}

	if ( s_view3DValid ) {
		x = s_view3D[0];
		y = VK_Draw_TopY( s_view3D[1], s_view3D[3] );
		w = s_view3D[2];
		h = s_view3D[3];
	} else {
		x = 0;
		y = 0;
		w = vkConfig.vidWidth;
		h = vkConfig.vidHeight;
	}

	/* GL's glDepthRange never touched x or y; the rect stays whatever glViewport
	   last clamped.  This lane has to re-issue it, and the client's view rect can
	   land outside the device's own bounds range, so it rides the same clamp the
	   3D view took. */
	VK_Draw_ViewportRect( cmd, x, y, w, h, n, f );
}

void vkDepthRange( float n, float f )
{
	VK_Shade_DepthRange( n, f );
}

void vkDepthRangef( float n, float f )
{
	VK_Shade_DepthRange( n, f );
}

void vkPolygonOffset( float factor, float units )
{
	VkCommandBuffer cmd = VK_Shade_CommandBuffer();

	if ( cmd != VK_NULL_HANDLE ) {
		qvk.CmdSetDepthBias( cmd, units, 0.0f, factor );
	}
}



void VK_BindProgram( vkProgram_t *p )
{
	s_shade.prog = p;
}

/* ---- the footprint census ------------------------------------------------ */

/* Clip one triangle to the volume the rasteriser actually clips against and
   return the survivors as NDC pairs.  Vulkan's z runs 0..w, not -w..w, and the
   w>=eps plane keeps the divide finite.  Returns the survivor count. */
static int VK_Shade_ClipTri( const float pj[16], const float *const v[3], float ndc[16][2] )
{
	/* a,b,c,d with inside = a*x+b*y+c*z+d*w - bias >= 0 */
	static const float planes[7][4] = {
		{ -1, 0, 0, 1 }, { 1, 0, 0, 1 },
		{ 0, -1, 0, 1 }, { 0, 1, 0, 1 },
		{ 0, 0, -1, 1 }, { 0, 0, 1, 0 },
		{ 0, 0, 0, 1 }
	};
	static const float bias[7] = { 0, 0, 0, 0, 0, 0, 1e-4f };
	float poly[16][4];
	float out[16][4];
	int   n = 3, p, i, j;

	for ( i = 0; i < 3; i++ ) {
		const float *xyz = v[i];

		poly[i][0] = pj[0] * xyz[0] + pj[4] * xyz[1] + pj[8]  * xyz[2] + pj[12];
		poly[i][1] = pj[1] * xyz[0] + pj[5] * xyz[1] + pj[9]  * xyz[2] + pj[13];
		poly[i][2] = pj[2] * xyz[0] + pj[6] * xyz[1] + pj[10] * xyz[2] + pj[14];
		poly[i][3] = pj[3] * xyz[0] + pj[7] * xyz[1] + pj[11] * xyz[2] + pj[15];
	}

	for ( p = 0; p < 7 && n; p++ ) {
		int m = 0;

		for ( i = 0; i < n; i++ ) {
			const float *a = poly[i];
			const float *b = poly[( i + 1 ) % n];
			float		 da = 0.0f, db = 0.0f, t;

			for ( j = 0; j < 4; j++ ) {
				da += planes[p][j] * a[j];
				db += planes[p][j] * b[j];
			}
			da -= bias[p];
			db -= bias[p];

			if ( da >= 0.0f ) {
				out[m][0] = a[0]; out[m][1] = a[1];
				out[m][2] = a[2]; out[m][3] = a[3];
				m++;
			}
			if ( ( da >= 0.0f ) != ( db >= 0.0f ) ) {
				t = da / ( da - db );
				for ( j = 0; j < 4; j++ ) {
					out[m][j] = a[j] + t * ( b[j] - a[j] );
				}
				m++;
			}
		}

		n = m;
		if ( n > 14 ) {
			n = 14;
		}
		memcpy( poly, out, (size_t)n * sizeof( poly[0] ) );
	}

	for ( i = 0; i < n; i++ ) {
		ndc[i][0] = poly[i][0] / poly[i][3];
		ndc[i][1] = poly[i][1] / poly[i][3];
	}

	return n;
}

/* A draw whose clipped footprint covers a frame is the shape a wedge makes, and
   the raw vertex box never saw it: a primitive whose corners all sit outside the
   volume still crosses the frame, and that is exactly the class the whole-vertex
   test missed.  So the triangles are clipped first and the survivor's screen
   area is what gets measured.  A wall the camera stands next to fills the frame
   too, so the row also carries the smallest box this shader drew this frame: a
   stage whose own footprint is an outlier against its siblings is the broken
   one, and a shader whose every draw is large is just close geometry. */
typedef struct {
	const shader_t	*shader;
	int				 frame;
	float			 minArea;
} vkFootprintKey_t;

#define VK_FOOTPRINT_KEYS 64

static vkFootprintKey_t	s_footprint[VK_FOOTPRINT_KEYS];

static void VK_Shade_FootprintRow( const shaderCommands_t *input, const void *stage,
								   const vkIndex_t *indexes, int numIndexes )
{
	const float	*mv, *pj;
	float		m[16];
	float		cminx, cminy, cmaxx, cmaxy, clipArea;
	float		bx0, by0, bx1, by1;
	float		*sibMin = NULL, sibling = 0.0f;
	qboolean	rawInside;
	int			i, v, tris, big, oob, oobMax, maxTri, key = -1;
	static int	s_rows;
	static int	s_frame = -1;
	static int	s_lastToken;

	if ( !R_ProbeStatsOn() || !input || !indexes || numIndexes < 3 || !input->numVertexes ) {
		return;
	}

	if ( s_frame != RB_FrameCount() ) {
		s_frame = RB_FrameCount();
		s_lastToken = 0;
		memset( s_footprint, 0, sizeof( s_footprint ) );
	}

	/* One census a batch.  The stage walk reaches this for every stage of a
	   batch, and every stage of a batch carries the same triangles, so the
	   per-triangle clip ran three to five times over the same geometry --
	   measured at 22500 clips a frame, 3.1 ms, against 6558 unique triangles
	   and 0.9 ms.  The batch token is the same RB_BeginSurface identity the
	   ring slice shares, so a repeat is the same batch and every number the row
	   reports is unchanged. */
	if ( s_lastToken && s_lastToken == VK_VertexRing_SliceToken() ) {
		return;
	}
	s_lastToken = VK_VertexRing_SliceToken();
	for ( i = 0; i < VK_FOOTPRINT_KEYS; i++ ) {
		if ( s_footprint[i].shader == input->shader ) {
			key = i;
			break;
		}
		if ( s_footprint[i].shader == NULL ) {
			s_footprint[i].shader = input->shader;
			s_footprint[i].frame = s_frame;
			s_footprint[i].minArea = 1e30f;
			key = i;
			break;
		}
	}
	if ( key >= 0 ) {
		if ( s_footprint[key].frame != s_frame ) {
			s_footprint[key].frame = s_frame;
			s_footprint[key].minArea = 1e30f;
		}
		sibMin = &s_footprint[key].minArea;
		sibling = *sibMin;
	}

	if ( backEnd.in2D && VK_Draw_CurrentOrtho() ) {
		/* the 2D window's own ortho is the push constant the draw carries */
		mv = NULL;
		pj = VK_Draw_CurrentOrtho();
	} else {
		mv = RB_GetModelViewMatrix();
		pj = RB_GetProjectionMatrix();
	}
	if ( !pj ) {
		return;
	}
	if ( mv ) {
		VK_Matrix_Multiply( m, pj, mv );
		pj = m;
	}

	cminx = cminy = 2.0f;
	cmaxx = cmaxy = -2.0f;
	bx0 = by0 = 2.0f;
	bx1 = by1 = -2.0f;
	big = 0;
	oob = 0;
	oobMax = -1;
	maxTri = -1;
	clipArea = 0.0f;
	tris = numIndexes / 3;

	for ( i = 0; i + 2 < numIndexes; i += 3 ) {
		const float	*vert[3];
		float		 ndc[16][2];
		int			 n, k, inside = 0;

		for ( v = 0; v < 3; v++ ) {
			int vi = indexes[i + v];

			if ( vi < 0 || vi >= input->numVertexes ) {
				/* an index past the vertices this surface wrote is the
				   uninitialised-region class on its own: the rasteriser reads
				   whatever the ring slice held before it, and that is one
				   primitive with arbitrary corners.  Name it whatever the box
				   looks like. */
				oob++;
				if ( vi > oobMax ) {
					oobMax = vi;
				}
				inside = -1;
				continue;
			}
			vert[v] = input->xyz[vi];
		}
		if ( inside == -1 ) {
			continue;
		}

		n = VK_Shade_ClipTri( pj, vert, ndc );

		/* the raw box is the corners as projected, before the clip, and it keeps
		   its own accumulator: a draw whose corners already sit on the unit cube
		   is pre-projected vertex data, and only the unclipped corners can say
		   so.  The clipped box is the polygon's own, and the two never mix. */
		for ( v = 0; v < 3; v++ ) {
			const float *xyz = vert[v];
			float		 cw = pj[3] * xyz[0] + pj[7] * xyz[1] + pj[11] * xyz[2] + pj[15];
			float		 nx, ny;

			if ( cw < 1e-6f && cw > -1e-6f ) {
				continue;
			}
			nx = ( pj[0] * xyz[0] + pj[4] * xyz[1] + pj[8]  * xyz[2] + pj[12] ) / cw;
			ny = ( pj[1] * xyz[0] + pj[5] * xyz[1] + pj[9]  * xyz[2] + pj[13] ) / cw;
			if ( nx < bx0 ) bx0 = nx;
			if ( ny < by0 ) by0 = ny;
			if ( nx > bx1 ) bx1 = nx;
			if ( ny > by1 ) by1 = ny;
		}

		if ( n >= 3 ) {
			/* NDC spans 2x2, so the frame is 4 units of area */
			float cx0 = 2.0f, cy0 = 2.0f, cx1 = -2.0f, cy1 = -2.0f;
			float area;

			for ( k = 0; k < n; k++ ) {
				if ( ndc[k][0] < cx0 ) cx0 = ndc[k][0];
				if ( ndc[k][1] < cy0 ) cy0 = ndc[k][1];
				if ( ndc[k][0] > cx1 ) cx1 = ndc[k][0];
				if ( ndc[k][1] > cy1 ) cy1 = ndc[k][1];
			}
			area = ( cx1 - cx0 ) * ( cy1 - cy0 );

			if ( area > clipArea ) {
				clipArea = area;
				maxTri = i / 3;
			}
			if ( cx0 < cminx ) cminx = cx0;
			if ( cy0 < cminy ) cminy = cy0;
			if ( cx1 > cmaxx ) cmaxx = cx1;
			if ( cy1 > cmaxy ) cmaxy = cy1;
			/* one primitive covering an eighth of the frame is no wall slice */
			if ( area >= 0.5f ) {
				big++;
			}
		}
	}

	/* the sibling floor is the smallest box this shader drew this frame, taken
	   after the walk so a lone draw reports its own area as its own floor */
	if ( sibMin && clipArea < *sibMin ) {
		*sibMin = clipArea;
		sibling = clipArea;
	}

	/* A wall the camera stands beside fills the frame after the clip and its raw
	   corners reach far past the volume, so area alone names ordinary geometry.
	   A primitive that was never in world space has its corners inside the volume
	   already -- but so does a wall seen from close by, and those cover up to
	   about 1.2 of the 4 units a frame holds.  Half a frame is past what a close
	   wall reaches and well under what a sheet across the frame covers. */
	rawInside = ( bx0 >= -1.0f && by0 >= -1.0f && bx1 <= 1.0f && by1 <= 1.0f );

	/* an index past the vertices, or a primitive already inside the volume whose
	   clipped area covers half the frame */
	if ( !oob && !( rawInside && clipArea >= 2.0f ) ) {
		return;
	}

	s_rows++;
	if ( s_rows > 64 && ( s_rows % 600 ) != 0 ) {
		return;
	}

	VK_Guard_Row( "VK:bigdraw",
				  "n=%d shader='%s' in2d=%d tris=%d verts=%d idx=%d clip=(%.3f,%.3f)-(%.3f,%.3f) "
				  "raw=(%.3f,%.3f)-(%.3f,%.3f) area=%.3f sib=%.3f tri=%d bigtris=%d oobidx=%d oobmax=%d "
				  "dw=%d sb=0x%x prog=0x%06x stage=%d ent=%d rt=%d model='%s'",
				  s_rows, input->shader ? input->shader->name : "(null)",
				  backEnd.in2D ? 1 : 0, tris, input->numVertexes, numIndexes,
				  cminx, cminy, cmaxx, cmaxy, bx0, by0, bx1, by1,
				  clipArea, sibling, maxTri, big, oob, oobMax,
				  s_shade.depthWrite ? 1 : 0,
				  (unsigned)s_shade.stateBits,
				  ( s_shade.prog && s_shade.prog->linked ) ? (unsigned)s_shade.prog->defines : 0u,
				  stage ? 1 : 0,
				  backEnd.currentEntityNum,
				  backEnd.currentEntity ? backEnd.currentEntity->e.reType : -1,
				  ( backEnd.currentEntity && backEnd.currentEntity->e.hModel )
					  ? RE_GetModelName( backEnd.currentEntity->e.hModel ) : "(none)" );
}

/* the portal clip plane, uploaded with the view uniforms */
void VK_UploadClipPlane( vkProgram_t *p )
{
	vec4_t plane;

	if ( RB_ClipPlane_Get( plane ) ) {
		VK_SetUniform4fv( p, VK_U_CLIPPLANE, plane );
		VK_SetUniform1i( p, VK_U_CLIPPLANEENABLE, 1 );
	} else {
		VK_SetUniform1i( p, VK_U_CLIPPLANEENABLE, 0 );
	}
}

void VK_RefreshClipPlane( void )
{
	VK_UploadClipPlane( s_shade.prog );
}

void VK_PixelProbe( const char *src, int x, int y, int w, int h )
{
	(void)src; (void)x; (void)y; (void)w; (void)h;
}

void VK_GlErrAt( const char *what )
{
	(void)what;
}

void VK_CensusShutdownAll( void ) { }

void VK_CreateWaveTable( void )
{
	static float	lut[FUNCTABLE_SIZE * 5];
	int				i;
	int				handle = -1;

	if ( s_waveTableHandle >= 0 ) {
		return;
	}
	if ( !qvk.CreateImage || !qvk.MapMemory ) {
		return;
	}

	for ( i = 0; i < FUNCTABLE_SIZE; i++ ) {
		lut[0 * FUNCTABLE_SIZE + i] = tr.sinTable[i];
		lut[1 * FUNCTABLE_SIZE + i] = tr.squareTable[i];
		lut[2 * FUNCTABLE_SIZE + i] = tr.triangleTable[i];
		lut[3 * FUNCTABLE_SIZE + i] = tr.sawToothTable[i];
		lut[4 * FUNCTABLE_SIZE + i] = tr.inverseSawToothTable[i];
	}

	if ( !VK_Texture_Create( FUNCTABLE_SIZE, 5, 1, VK_FORMAT_R32_SFLOAT, &handle ) ) {
		return;
	}
	if ( !VK_Texture_Upload( handle, 0, FUNCTABLE_SIZE, 5, lut, sizeof( lut ) ) ) {
		VK_Texture_Destroy( handle );
		return;
	}
	if ( !VK_Texture_Publish( handle ) ) {
		VK_Texture_Destroy( handle );
		return;
	}

	s_waveTableHandle = handle;
}

void VK_DestroyWaveTable( void )
{
	if ( s_waveTableHandle >= 0 ) {
		VK_Texture_Destroy( s_waveTableHandle );
		s_waveTableHandle = -1;
	}
}

int VK_WaveTable( void )
{
	return s_waveTableHandle;
}


vkProgram_t *VK_FamilyProgram( unsigned family )  /* the draw's family, by name */
{
	return VK_GetProgram( family );
}

/* the program a stage needs: the shade tree's defines, the stage's own texture
   environment, and the caller's.  The texenv flags select vk_stage.frag's
   second-bundle arm; leaving them out here compiled every two-bundle pass into
   the MODULATE branch, so a merged ADD pass -- the star dome's, and every
   world fx merge -- multiplied instead of adding.  GLES3_StageProgram ORs its
   GLES3_TexEnvForStage in the same way. */
vkProgram_t *VK_StageProgram( const shaderStage_t *pStage, const shader_t *shader, unsigned extraDefines )
{
	unsigned d = VK_StageDefines( pStage, shader );

	return VK_GetProgram( d | VK_TexEnvForStage( pStage, shader ) | extraDefines );
}

void VK_DrawStage( const shaderCommands_t *input, const shaderStage_t *pStage,
					  const shader_t *shader, unsigned stateBits, float forceAlpha,
					  qboolean hudColorClobber )
{
	vkProgram_t	*prog;
	const float		*tc0, *tc1;
	const unsigned char *colors;
	int				tcStride;
	qboolean		cpuColor = VK_StageNeedsCPUColor( pStage );
	qboolean		cpuTexCoords = VK_StageNeedsCPUTexCoords( pStage );

	prog = VK_StageProgram( pStage, shader, 0 );
	VK_BindProgram( prog );

	VK_SetupStageUniforms( prog, pStage, shader );
	VK_SetupFogUniforms( prog, tess.fogNum, pStage->fogDensityArm );
	VK_SetUniform1f( prog, VK_U_FORCEALPHA, forceAlpha );

	if ( hudColorClobber && ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D
		 && pStage->rgbGen != CGEN_LIGHTING_SPHERICAL
		 && pStage->rgbGen != CGEN_NOISE
		 && pStage->rgbGen != CGEN_SCOORD
		 && pStage->rgbGen != CGEN_TCOORD )
	{
		VK_SetUniform1i( prog, VK_U_COLORGEN, (int)CGEN_CONSTANT );
		VK_SetUniform1i( prog, VK_U_ALPHAGEN, (int)AGEN_IDENTITY );
		VK_SetBaseColor4f( prog, 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( pStage->bundle[0].vertexLightmap && r_vertexLight->integer && r_lightmap->integer )
	{
		VK_SelectTexture( 0 );
		VK_Bind( tr.whiteImage );
	}
	else if ( ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D && pStage->bundle[0].isLightmap )
	{
		VK_SelectTexture( 0 );
		VK_Bind( tr.whiteImage );
	}
	else
	{
		VK_BindAnimatedImage( &pStage->bundle[0], 0 );
	}

	if ( pStage->bundle[1].image[0] )
	{
		if ( ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D && pStage->bundle[1].isLightmap )
		{
			VK_SelectTexture( 1 );
			VK_Bind( tr.whiteImage );
		}
		else
		{
			VK_BindAnimatedImage( &pStage->bundle[1], 1 );
		}
	}

	if ( cpuTexCoords )
	{
		tc0 = input->svars.texcoords[0][0];
		tc1 = input->svars.texcoords[1][0];
		tcStride = 2;
	}
	else
	{
		tc0 = input->texCoords[0][0];
		tc1 = input->texCoords[0][0] + 2;
		tcStride = 4;
	}
	/* A CPU-colour stage takes the svars array.  In 2D every tess quad carries
	   Draw_SetColor in vertexColors -- the font run writes it explicitly and the
	   recorded 2D prims write it per vertex -- and VK_SetupStageUniforms pins
	   u_BaseColor to 1 there expecting exactly that, so the array has to reach
	   the pack: white here erased the pause menu's title and button labels while
	   the same subtree's textured art stayed right.  In 3D a constant-colour
	   stage never fills vertexColors, so the white pack is the one that is right. */
	colors = cpuColor ? (const unsigned char *)input->svars.colors
					  : ( backEnd.in2D ? (const unsigned char *)input->vertexColors : NULL );

	(void)stateBits;
	VK_StageBindCensus( shader, pStage, -1, backEnd.in2D ? qtrue : qfalse, 0, 0, 0, 0 );

	RB_VBO_DrawTessArrays( input, input->indexes, input->numIndexes,
						   tc0, tc1, tcStride, colors, input->normal[0], pStage );
}

/* ---- the census seam the reference renderer carries ---------------------- */

/* The world path's answer to VK:2ddraw: one row per stage pass that names the
   shader, the stage, and the image each bundle resolves to.  A world surface
   sampling a slot another texture has taken names itself here exactly the way a
   menu tile does, which is the row that closes the patchy-world question. */
void VK_StageBindCensus( const shader_t *shader, const shaderStage_t *pStage, int stageIndex,
							qboolean in2D, unsigned tex0, unsigned tex1,
							unsigned cur0, unsigned cur1 )
{
	static int s_rows;
	const char *n0, *n1;
	unsigned    t0, t1;

	(void)tex0; (void)tex1; (void)cur0; (void)cur1;

	if ( !shader || !pStage ) {
		return;
	}

	n0 = ( pStage->bundle[0].image[0] && pStage->bundle[0].image[0]->imgName[0] )
		 ? pStage->bundle[0].image[0]->imgName : "";
	t0 = pStage->bundle[0].image[0] ? (unsigned)pStage->bundle[0].image[0]->texnum : 0u;
	n1 = ( pStage->bundle[1].image[0] && pStage->bundle[1].image[0]->imgName[0] )
		 ? pStage->bundle[1].image[0]->imgName : "";
	t1 = pStage->bundle[1].image[0] ? (unsigned)pStage->bundle[1].image[0]->texnum : 0u;

	s_rows++;
	if ( s_rows <= 48 || ( s_rows % 257 ) == 0 ) {
		VK_Guard_Row( "VK:stagebind",
					  "n=%d d=%d shader='%s' stage=%d prog=0x%06x sb=0x%x mt=%d "
					  "b0='%s' tex0=%u b1='%s' tex1=%u rg=%d ag=%d const=%u,%u,%u,%u",
					  s_rows, in2D ? 1 : 0,
					  shader->name,
					  stageIndex,
					  ( s_shade.prog && s_shade.prog->linked ) ? (unsigned)s_shade.prog->defines : 0u,
					  (unsigned)pStage->stateBits,
					  (unsigned)pStage->multitextureEnv,
					  n0, t0, n1, t1,
					  (int)pStage->rgbGen, (int)pStage->alphaGen,
					  (unsigned)pStage->colorConst[0], (unsigned)pStage->colorConst[1],
					  (unsigned)pStage->colorConst[2], (unsigned)pStage->colorConst[3] );
	}
}

void VK_DrawCensusKey( const void *key, qboolean in2D )
{
	(void)key; (void)in2D;
}

/* ---- the presented-frame readback ---------------------------------------- */

/* One host copy of the swapchain, shared by every sampler in the frame: the
   dome question is "do its draws reach the framebuffer", and answering it needs
   the composited result, not a stage's own target. */
static unsigned char	*s_swapPixels;
static int				 s_swapW, s_swapH, s_swapValid;

qboolean VK_ModelPixelAtSwap( int x, int y, byte out[4] )
{
	int ox, oy;

	if ( !out || !s_swapValid || x < 0 || y < 0 ) {
		return qfalse;
	}
	if ( x >= s_swapW || y >= s_swapH ) {
		return qfalse;
	}

	/* the swapchain rows run bottom-up, the client's run top-down */
	ox = x;
	oy = s_swapH - 1 - y;

	out[0] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 0];
	out[1] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 1];
	out[2] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 2];
	out[3] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 3];
	return qtrue;
}

static void VK_Shade_FreeSwapPixels( void )
{
	if ( s_swapPixels ) {
		ri.Free( s_swapPixels );
		s_swapPixels = NULL;
	}
	s_swapW = s_swapH = s_swapValid = 0;
}

/* ---- the presented frame's own map --------------------------------------- */

/* Nine fixed samples cannot describe a shape: they name nine pixels and the
   wedge between them is inferred, which is how a defect survives two rounds.
   This reads the same host copy the sampler reads and answers the two
   questions a shape asks -- what does the screen's coarse grid look like, and
   which single colour covers the biggest box.  A flat translucent triangle
   over half the viewport names itself in the second row: one colour, one
   bounding box, and an extent a real scene never produces. */

#define VK_GRID_COLS	32
#define VK_GRID_ROWS	24

typedef struct {
	unsigned	rgba;
	int			count;
	int			minx, miny, maxx, maxy;
} vkFlatRun_t;

/* 640x480 downsampled by four is 160x120 -- 19200 samples, so a flat half
   screen is ~9600 hits and the histogram holds it with room for a real scene's
   noise.  Linear probe, no allocation, rebuilt every sampled frame. */
#define VK_FLAT_BUCKETS	4096

/* A rendered frame carries far more than VK_FLAT_BUCKETS distinct colours, so
   the table fills within the first few thousand samples.  Once it is full a
   linear probe that runs to the capacity is O(4096) per sample, and the census
   costs sampled-pixels x 4096: measured offline at 47 ms per 800x600 readback
   and 253 ms per 1920x1200 one, which is the walk's whole residual bucket.  The
   probe is therefore bounded, and insert and lookup use the SAME bound, so
   every key that was inserted stays findable and a miss costs the bound. */
#define VK_FLAT_PROBE	32

/* The census is an instrument, so its price must not scale with the resolution
   it is measuring: the scan is given a fixed sample budget and the step is
   derived from the frame. */
#define VK_PIX_BUDGET	32768

static int VK_Shade_CensusStep( int w, int h )
{
	int step = 1;

	while ( step < 64 && ( w / step ) * ( h / step ) > VK_PIX_BUDGET ) {
		step++;
	}
	return step;
}

static vkFlatRun_t	s_flat[VK_FLAT_BUCKETS];

static void VK_Shade_FlatReset( void )
{
	memset( s_flat, 0, sizeof( s_flat ) );
}

/* The frame's hue census, over the same host copy the flat boxes read.  A shape
   the nine fixed samples miss is answered by counting which primitive channel
   dominates each pixel and where those pixels are: the radar cone is a green
   texel under rgbGen const white, so the boot that opens the radar has to show
   its pixels green-dominant, and a red-clustered box names the same defect with
   no coordinate guessed in advance.  Position-independent on purpose, so it is
   a class instrument and not one surface's probe. */
typedef struct {
	unsigned long long	n;
	int					minx, miny, maxx, maxy;
	unsigned long long	sum[3];
} vkHueRun_t;

static vkHueRun_t	s_hue[3];

static void VK_Shade_HueReset( void )
{
	int i;

	memset( s_hue, 0, sizeof( s_hue ) );
	for ( i = 0; i < 3; i++ ) {
		s_hue[i].minx = s_hue[i].miny = 0x7fffffff;
		s_hue[i].maxx = s_hue[i].maxy = -1;
	}
}

static void VK_Shade_HueAdd( const unsigned char *p, int x, int y )
{
	int			 r = p[2], g = p[1], b = p[0];
	int			 hi, lo, which;
	vkHueRun_t	*run;

	hi = r > g ? r : g;
	hi = hi > b ? hi : b;
	lo = r < g ? r : g;
	lo = lo < b ? lo : b;

	/* a grey or a dim pixel is not a hue: 40 of separation and 64 of level keeps
	   the UI's greys, the black bars and the dark world out of the counts */
	if ( hi - lo < 40 || hi < 64 ) {
		return;
	}
	which = ( hi == r ) ? 0 : ( hi == g ) ? 1 : 2;
	run = &s_hue[which];
	run->n++;
	run->sum[0] += (unsigned)p[2];
	run->sum[1] += (unsigned)p[1];
	run->sum[2] += (unsigned)p[0];
	if ( x < run->minx ) run->minx = x;
	if ( y < run->miny ) run->miny = y;
	if ( x > run->maxx ) run->maxx = x;
	if ( y > run->maxy ) run->maxy = y;
}

static void VK_Shade_HueRow( int frame )
{
	static const char *name[3] = { "red", "green", "blue" };
	int i;

	for ( i = 0; i < 3; i++ ) {
		vkHueRun_t *r = &s_hue[i];

		if ( !r->n ) {
			continue;
		}
		VK_Guard_Row( "VK:hue",
					  "frame=%d %s n=%llu box=(%d,%d)-(%d,%d) mean=%u,%u,%u",
					  frame, name[i], r->n, r->minx, r->miny, r->maxx, r->maxy,
					  (unsigned)( r->sum[0] / r->n ), (unsigned)( r->sum[1] / r->n ),
					  (unsigned)( r->sum[2] / r->n ) );
	}
}

static void VK_Shade_FlatAdd( const unsigned char *p, int x, int y ){
	/* the swapchain is BGRA in memory; the row names r,g,b,a, so the key packs
	   the four bytes the same way the row prints them */
	unsigned	key = ( (unsigned)p[2] << 24 ) | ( (unsigned)p[1] << 16 )
					  | ( (unsigned)p[0] << 8 ) | (unsigned)p[3];
	unsigned	h = ( key * 2654435761u ) & ( VK_FLAT_BUCKETS - 1 );
	int			i;

	for ( i = 0; i < VK_FLAT_PROBE; i++ ) {
		vkFlatRun_t *r = &s_flat[( h + i ) & ( VK_FLAT_BUCKETS - 1 )];

		if ( r->count == 0 ) {
			r->rgba = key;
			r->count = 1;
			r->minx = r->maxx = x;
			r->miny = r->maxy = y;
			return;
		}
		if ( r->rgba == key ) {
			r->count++;
			if ( x < r->minx ) r->minx = x;
			if ( y < r->miny ) r->miny = y;
			if ( x > r->maxx ) r->maxx = x;
			if ( y > r->maxy ) r->maxy = y;
			return;
		}
	}
}

static void VK_Shade_FlatRows( int frame )
{
	vkFlatRun_t	*top[4] = { NULL, NULL, NULL, NULL };
	int			n = 0;
	int			i, k;

	for ( i = 0; i < VK_FLAT_BUCKETS; i++ ) {
		int at = -1;

		if ( s_flat[i].count < 64 ) {
			continue;
		}
		/* the slot this count belongs in, in a list kept descending by count */
		for ( k = 0; k < n; k++ ) {
			if ( s_flat[i].count > top[k]->count ) {
				at = k;
				break;
			}
		}
		if ( at < 0 ) {
			if ( n >= 4 ) {
				continue;
			}
			at = n;
		}
		if ( n < 4 ) {
			for ( k = n; k > at; k-- ) {
				top[k] = top[k - 1];
			}
			n++;
		} else {
			for ( k = 3; k > at; k-- ) {
				top[k] = top[k - 1];
			}
		}
		top[at] = &s_flat[i];
	}

	for ( i = 0; i < 4 && top[i]; i++ ) {
		vkFlatRun_t *r = top[i];

		VK_Guard_Row( "VK:frameflat",
					  "frame=%d rank=%d rgba=%u,%u,%u,%u count=%d box=(%d,%d)-(%d,%d) of %dx%d",
					  frame, i + 1,
					  ( r->rgba >> 24 ) & 0xff, ( r->rgba >> 16 ) & 0xff,
					  ( r->rgba >> 8 ) & 0xff, ( r->rgba >> 0 ) & 0xff,
					  r->count, r->minx, r->miny, r->maxx, r->maxy, s_swapW, s_swapH );
	}
}

/* One row per grid line: alpha in the high nibble of each cell and luminance in
   the low, so a translucent overlay and the scene behind it read off the same
   line.  Emitted only when the grid changes and only every few sampled frames,
   so a moving camera costs five lines a second and a still screen costs one. */
static void VK_Shade_GridRows( int frame )
{
	static unsigned	s_lastHash;
	static qboolean	s_haveHash;
	static int		s_ticks;
	int				cx, cy, gx, gy;
	unsigned		hash;
	char			line[VK_GRID_COLS * 2 + 1];

	cx = s_swapW / VK_GRID_COLS;
	cy = s_swapH / VK_GRID_ROWS;
	if ( cx < 1 || cy < 1 ) {
		return;
	}

	s_ticks++;
	if ( s_haveHash && ( s_ticks % 5 ) != 0 ) {
		return;
	}

	hash = 2166136261u;
	for ( gy = 0; gy < VK_GRID_ROWS; gy++ ) {
		for ( gx = 0; gx < VK_GRID_COLS; gx++ ) {
			unsigned	a = 0, l = 0;
			int			sx, sy, n = 0;
			int			x0 = gx * cx, y0 = gy * cy;
			int			x1 = x0 + cx, y1 = y0 + cy;
			int			sxs = ( cx + 7 ) / 8, sys = ( cy + 7 ) / 8;

			if ( sxs < 1 ) sxs = 1;
			if ( sys < 1 ) sys = 1;
			for ( sy = y0; sy < y1 && sy < s_swapH; sy += sys ) {
				for ( sx = x0; sx < x1 && sx < s_swapW; sx += sxs ) {
					const unsigned char *p = s_swapPixels
						+ ( (size_t)sy * s_swapW + sx ) * 4;

					a += p[3];
					l += ( p[0] + p[1] + p[2] ) / 3;
					n++;
				}
			}
			if ( !n ) {
				n = 1;
			}
			a = ( a / n ) >> 4;
			l = ( l / n ) >> 4;
			hash = ( hash ^ ( ( a << 4 ) | l ) ) * 16777619u;
		}
	}

	if ( s_haveHash && hash == s_lastHash ) {
		return;
	}
	s_lastHash = hash;
	s_haveHash = qtrue;

	VK_Guard_Row( "VK:framegrid", "frame=%d grid=%dx%d hash=0x%08x",
				  frame, VK_GRID_COLS, VK_GRID_ROWS, hash );
	for ( gy = 0; gy < VK_GRID_ROWS; gy++ ) {
		for ( gx = 0; gx < VK_GRID_COLS; gx++ ) {
			unsigned	a = 0, l = 0;
			int			sx, sy, n = 0;
			int			x0 = gx * cx, y0 = gy * cy;
			int			x1 = x0 + cx, y1 = y0 + cy;
			int			sxs = ( cx + 7 ) / 8, sys = ( cy + 7 ) / 8;

			if ( sxs < 1 ) sxs = 1;
			if ( sys < 1 ) sys = 1;
			for ( sy = y0; sy < y1 && sy < s_swapH; sy += sys ) {
				for ( sx = x0; sx < x1 && sx < s_swapW; sx += sxs ) {
					const unsigned char *p = s_swapPixels
						+ ( (size_t)sy * s_swapW + sx ) * 4;

					a += p[3];
					l += ( p[0] + p[1] + p[2] ) / 3;
					n++;
				}
			}
			if ( !n ) {
				n = 1;
			}
			snprintf( &line[gx * 2], 3, "%x%x", (int)( ( a / n ) >> 4 ), (int)( ( l / n ) >> 4 ) );
		}
		line[VK_GRID_COLS * 2] = 0;
		VK_Guard_Row( "VK:framegrid", "frame=%d row=%02d %s", frame, gy, line );
	}
}

/* Called from the present seam.  Reads the presented image back once and
   samples a fixed grid, so one boot says whether the dome's stage draws are in
   the composited frame or only ever in a target the 2D pass then covers. */
#define VK_3DRECT_MAX 4
static struct { int x, y, w, h; } s_ui3D[VK_3DRECT_MAX];
static int s_ui3DCount;

/* The UI names each 3D widget rect it is about to render.  The nine fixed
   samples cannot answer the CONTROLS question -- the frame is dominated by the
   0.35,0.67,1.00 wash of the full-screen RenderModel, and whether the stardome
   ever reaches the framebuffer is exactly "what colour is inside the stardome's
   own rect at present".  Deduped, so a per-frame caller keeps a stable list. */
void VK_Shade_Note3DRect( int x, int y, int w, int h )
{
	int i;

	if ( w <= 0 || h <= 0 ) {
		return;
	}
	for ( i = 0; i < s_ui3DCount; i++ ) {
		if ( s_ui3D[i].x == x && s_ui3D[i].y == y
			 && s_ui3D[i].w == w && s_ui3D[i].h == h ) {
			return;
		}
	}
	if ( s_ui3DCount < VK_3DRECT_MAX ) {
		s_ui3D[s_ui3DCount].x = x;
		s_ui3D[s_ui3DCount].y = y;
		s_ui3D[s_ui3DCount].w = w;
		s_ui3D[s_ui3DCount].h = h;
		s_ui3DCount++;
	} else {
		s_ui3D[i % VK_3DRECT_MAX].x = x;
		s_ui3D[i % VK_3DRECT_MAX].y = y;
		s_ui3D[i % VK_3DRECT_MAX].w = w;
		s_ui3D[i % VK_3DRECT_MAX].h = h;
	}
}

void VK_Shade_ReadbackFrame( uint32_t imageIndex )
{
	static int	s_frames;
	byte		px[4];
	int			i;
	int			x, y;
	/* A three-by-three grid: five points named the ship and the corners and
	   nothing between them, and the dome's own colour lives between them. */
	struct { float fx, fy; const char *tag; } spot[9] = {
		{ 0.5f,  0.5f,  "centre" },
		{ 0.25f, 0.25f, "q1" },
		{ 0.75f, 0.25f, "q2" },
		{ 0.25f, 0.75f, "q3" },
		{ 0.75f, 0.75f, "q4" },
		{ 0.5f,  0.25f, "n" },
		{ 0.5f,  0.75f, "s" },
		{ 0.25f, 0.5f,  "w" },
		{ 0.75f, 0.5f,  "e" },
	};

	s_frames++;
	if ( !R_ProbeStatsOn() || ( s_frames % 60 ) != 0 ) {
		return;
	}
	if ( vk.swapchain == VK_NULL_HANDLE || !vk.swapchainExtent.width || !vk.swapchainExtent.height ) {
		return;
	}

	if ( (int)vk.swapchainExtent.width != s_swapW || (int)vk.swapchainExtent.height != s_swapH ) {
		VK_Shade_FreeSwapPixels();
		s_swapW = (int)vk.swapchainExtent.width;
		s_swapH = (int)vk.swapchainExtent.height;
		s_swapPixels = ri.Malloc( (size_t)s_swapW * (size_t)s_swapH * 4 );
	}

	if ( !VK_Swapchain_ReadPixels( imageIndex, s_swapPixels, (size_t)s_swapW * (size_t)s_swapH * 4 ) ) {
		s_swapValid = 0;
		VK_Guard_Row( "VK:readback", "frame=%d w=%d h=%d status=failed", s_frames, s_swapW, s_swapH );
		return;
	}
	s_swapValid = 1;

	{
		/* the presented frame's own map: the coarse grid and the flat boxes.
		   The step comes from the fixed sample budget, so this pass costs the
		   same at 800x600 and at 1920x1200 instead of scaling with the pixels
		   whose defect it is measuring. */
		int step = VK_Shade_CensusStep( s_swapW, s_swapH );
		int fx, fy;

		VK_Shade_FlatReset();
		VK_Shade_HueReset();
		for ( fy = 0; fy < s_swapH; fy += step ) {
			for ( fx = 0; fx < s_swapW; fx += step ) {
				const unsigned char *p = s_swapPixels + ( (size_t)fy * s_swapW + fx ) * 4;

				VK_Shade_FlatAdd( p, fx, fy );
				VK_Shade_HueAdd( p, fx, fy );
			}
		}
		VK_Shade_FlatRows( s_frames );
		VK_Shade_HueRow( s_frames );
		VK_Shade_GridRows( s_frames );
	}

	for ( i = 0; i < 9; i++ ) {
		x = (int)( spot[i].fx * ( s_swapW - 1 ) );
		y = (int)( spot[i].fy * ( s_swapH - 1 ) );
		if ( VK_ModelPixelAtSwap( x, y, px ) ) {
			VK_Guard_Row( "VK:readback", "frame=%d w=%d h=%d at=%s x=%d y=%d rgba=%d,%d,%d,%d clear=%.3f,%.3f,%.3f",
						  s_frames, s_swapW, s_swapH, spot[i].tag, x, y,
						  px[0], px[1], px[2], px[3],
						  vk.clearColor[0], vk.clearColor[1], vk.clearColor[2] );
		}
	}

	/* what landed inside each UI 3D widget's own rect: the wash and the model
	   are told apart by the colour at the model's own centre */
	for ( i = 0; i < s_ui3DCount; i++ ) {
		x = s_ui3D[i].x + s_ui3D[i].w / 2;
		y = s_ui3D[i].y + s_ui3D[i].h / 2;
		if ( VK_ModelPixelAtSwap( x, y, px ) ) {
			VK_Guard_Row( "VK:readback", "frame=%d at=ui3d%d rect=%d,%d %dx%d x=%d y=%d rgba=%d,%d,%d,%d",
						  s_frames, i, s_ui3D[i].x, s_ui3D[i].y, s_ui3D[i].w, s_ui3D[i].h,
						  x, y, px[0], px[1], px[2], px[3] );
		}
	}
}

void VK_SetGhostImage( const char *name, image_t *image )
{
	(void)name; (void)image;
}

/* defined by the mirrored 3D front end */

/* ---- the 2D order transcript -------------------------------------------- */

/* A widget's depth is the position it draws at, so the ordered shader names are
   the z-order answer.  The rate-limited VK:2ddraw row cannot carry it: past the
   fortieth draw the sampling throws the sequence away, and a frame's menu is
   hundreds of draws.  This keeps the frame's names in order and NewFrame emits
   them, deduped down the run so a widget that paints once reads as one entry.
   Emitted when the sequence changes, so a still menu costs one row. */
#define VK_2DORDER_MAX	64
static char		s_2dOrder[VK_2DORDER_MAX][64];
static int		s_2dOrderCount;
static unsigned	s_2dOrderHash;
static qboolean	s_2dOrderHave;

void VK_2DOrder_Note( const char *shader )
{
	unsigned h;
	int		 i;

	if ( !R_ProbeStatsOn() ) {
		return;
	}
	if ( !shader || !shader[0] ) {
		return;
	}
	i = s_2dOrderCount - 1;
	if ( i >= 0 && !strcmp( s_2dOrder[i], shader ) ) {
		return;
	}

	h = s_2dOrderHash;
	for ( i = 0; shader[i]; i++ ) {
		h = ( h ^ (unsigned char)shader[i] ) * 16777619u;
	}
	s_2dOrderHash = h;

	if ( s_2dOrderCount < VK_2DORDER_MAX ) {
		Q_strncpyz( s_2dOrder[s_2dOrderCount], shader, sizeof( s_2dOrder[0] ) );
		s_2dOrderCount++;
	}
}

static void VK_2DOrder_FrameRow( void )
{
	static unsigned	s_lastHash;
	static int		s_frames;
	char			line[VK_2DORDER_MAX * 32];
	int				i, written;

	s_frames++;
	if ( !R_ProbeStatsOn() || !s_2dOrderCount ) {
		s_2dOrderCount = 0;
		s_2dOrderHash = 0;
		return;
	}
	if ( !s_2dOrderHave || s_2dOrderHash != s_lastHash || ( s_frames % 900 ) == 0 ) {
		line[0] = 0;
		written = 0;
		for ( i = 0; i < s_2dOrderCount; i++ ) {
			int len = Com_sprintf( line + written, sizeof( line ) - written, "%s%s",
								   i ? "|" : "", s_2dOrder[i] );

			if ( len < 0 || (size_t)len >= sizeof( line ) - written ) {
				break;
			}
			written += len;
		}
		VK_Guard_Row( "VK:2dorder", "frame=%d draws=%d %s", s_frames, s_2dOrderCount, line );
		s_lastHash = s_2dOrderHash;
		s_2dOrderHave = qtrue;
	}
	s_2dOrderCount = 0;
	s_2dOrderHash = 0;
}

void VK_Shade_NewFrame( void )
{
	int slot;

	VK_2DOrder_FrameRow();

	/* the frame before this one is complete: its census is the number the retail trace carries */
	if ( s_frameShadeDraws || s_frameShadeDrops ) {
		static int rows;
		static int lastBatches = -1;

		VK_Shade_StageRows();

		/* a count that moves between frames is the vanish signature, so a transition is always named */
		if ( rows < 8 || s_frameShadeDraws != lastBatches || ( rows % 600 ) == 0 ) {
			VK_Guard_Row( "VK:draw3d", "frame batches=%d refusals=%d imm=%d surf-cmds=%d draw-surfs=%d perm-req=%d perm-miss=%d",
			  s_frameShadeDraws, s_frameShadeDrops, s_frameImmDraws,
			  s_frameSurfCmds, s_frameDrawSurfs, s_framePermReq, s_framePermMiss );
		}
		lastBatches = s_frameShadeDraws;
		rows++;
	}
	s_frameShadeDraws    = 0;
	s_frameShadeDrops    = 0;
	s_frameImmDraws      = 0;
	s_frameSurfCmds      = 0;
	s_frameDrawSurfs     = 0;
	s_framePermReq       = 0;
	s_framePermMiss      = 0;
	s_framePermMissRows  = 0;
	s_frameStageCount    = 0;
	s_frameStageDropped  = 0;

	/* the front end wrote this frame's uniforms; the first draw uploads them into a fresh ring */
	s_uboDirty = qtrue;

	slot = RB_FrameSmpFrame();
	if ( slot >= 0 && slot < VK_FRAMES_IN_FLIGHT ) {
		vk_shadeUboNext[slot] = 0;
	}
}
