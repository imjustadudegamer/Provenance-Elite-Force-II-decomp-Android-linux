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
 * app/src/main/cpp/tools/build_vk_spirv.py and emitted as the VK_UBO_OFF_* constants this file
 * packs at, so the CPU side and the shader cannot drift.
 */

#include "tr_local.h"
#include "tr_program.h"
#include "tr_vbo.h"
#include "vk_shaders_embedded3d.h"

#include <string.h>
#include <math.h>

/* vk_texture.c's own record of the created image: the source chain the loaders
   report and the number of levels the image was actually created with are two
   different numbers, and only the second one the sampler can reach. */
int VK_Texture_MipLevels( int handle );
const char *VK_Texture_LayoutName( int handle );

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
	VK_UBO_OFF_U_FLATMODE,
};
/* the same positional index as vk_uboMap, but for each member's writable span
   in bytes.  These come out of the generated header next to the offsets, so
   the packer's clamp cannot drift from the compiler's own layout. */
static const int vk_uboSize[VK_U_COUNT] = {
	VK_UBO_SIZE_U_MVP,
	0,                     /* u_ModelView: not in the block */
	VK_UBO_SIZE_U_MODELMATRIX,
	VK_UBO_SIZE_U_VIEWORIGIN,
	VK_UBO_SIZE_U_VIEWAXIS0,
	VK_UBO_SIZE_U_LOCALVIEWORIGIN,
	VK_UBO_SIZE_U_ENTITYORIGIN,
	VK_UBO_SIZE_U_ENTITYAXIS,
	VK_UBO_SIZE_U_ENTITYCOLOR,
	VK_UBO_SIZE_U_BASECOLOR,
	VK_UBO_SIZE_U_VERTCOLOR,
	VK_UBO_SIZE_U_IDENTITYLIGHT,
	VK_UBO_SIZE_U_IDENTITYLIGHTBYTE,
	VK_UBO_SIZE_U_ENTITYFADE,
	VK_UBO_SIZE_U_PORTALRANGE,
	VK_UBO_SIZE_U_SPECORIGIN,
	VK_UBO_SIZE_U_SUNDIRECTION,
	VK_UBO_SIZE_U_SKYALPHA,
	VK_UBO_SIZE_U_SKYSCROLL,
	VK_UBO_SIZE_U_SKYALPHAMODE,
	VK_UBO_SIZE_U_ENTITYTEXCOORD,
	VK_UBO_SIZE_U_COLORGEN,
	VK_UBO_SIZE_U_ALPHAGEN,
	VK_UBO_SIZE_U_ALPHAMIN,
	VK_UBO_SIZE_U_ALPHAMAX,
	VK_UBO_SIZE_U_ALPHACONST,
	VK_UBO_SIZE_U_ALPHACONSTMIN,
	VK_UBO_SIZE_U_WAVERGB,
	VK_UBO_SIZE_U_WAVEALPHA,
	VK_UBO_SIZE_U_FREQRGB,
	VK_UBO_SIZE_U_FREQALPHA,
	VK_UBO_SIZE_U_TCGEN0,
	VK_UBO_SIZE_U_TCGEN1,
	VK_UBO_SIZE_U_TCGEN0VEC0,
	VK_UBO_SIZE_U_TCGEN0VEC1,
	VK_UBO_SIZE_U_TCGEN1VEC0,
	VK_UBO_SIZE_U_TCGEN1VEC1,
	VK_UBO_SIZE_U_NUMTEXMOD0,
	VK_UBO_SIZE_U_NUMTEXMOD1,
	VK_UBO_SIZE_U_TEXMOD0,
	VK_UBO_SIZE_U_TEXMOD1,
	VK_UBO_SIZE_U_ALPHATEST,
	VK_UBO_SIZE_U_FORCEALPHA,
	VK_UBO_SIZE_U_TEXENV0,
	VK_UBO_SIZE_U_TEXENV1,
	0,                     /* u_Tex0: not in the block */
	0,                     /* u_Tex1: not in the block */
	VK_UBO_SIZE_U_TEX1COLOR,
	0,                     /* u_WaveTable: not in the block */
	VK_UBO_SIZE_U_CLIPPLANE,
	VK_UBO_SIZE_U_CLIPPLANEENABLE,
	VK_UBO_SIZE_U_SHADERTIME,
	VK_UBO_SIZE_U_ACFF,
	VK_UBO_SIZE_U_FOGNUM,
	VK_UBO_SIZE_U_FOGTCSCALE,
	VK_UBO_SIZE_U_FOGSCONSTANT,
	VK_UBO_SIZE_U_FOGSVECTOR,
	VK_UBO_SIZE_U_FOGTCONSTANT,
	VK_UBO_SIZE_U_FOGTVECTOR,
	VK_UBO_SIZE_U_FOGVIEWOUTSIDE,
	VK_UBO_SIZE_U_FOGCOLOR,
	VK_UBO_SIZE_U_FOGDENSITYARM,
	VK_UBO_SIZE_U_FOGOOLEN,
	VK_UBO_SIZE_U_FOGPLANECONST,
	VK_UBO_SIZE_U_FOGDVIEW,
	VK_UBO_SIZE_U_FOGDENSITYTVECTOR,
	VK_UBO_SIZE_U_FOGMODE,
	VK_UBO_SIZE_U_FOGSTART,
	VK_UBO_SIZE_U_FOGEND,
	VK_UBO_SIZE_U_FOGFIXCOLOR,
	VK_UBO_SIZE_U_FLATMODE,
};

/* the clamp is only safe while each span is the compiler's own.  A span smaller
   than the member truncates it -- that is how u_BaseColor published (r,0,0,0)
   and every rgbGen const stage multiplied its texel by (1,0,0) -- and one larger
   than the gap to the next member would run over it.  Both are build failures
   here rather than device symptoms to chase. */
typedef char vk_ubo_span_vec4[( VK_UBO_SIZE_U_BASECOLOR == 16
							   && VK_UBO_SIZE_U_ENTITYCOLOR == 16
							   && VK_UBO_SIZE_U_VERTCOLOR == 16
							   && VK_UBO_SIZE_U_WAVERGB == 16
							   && VK_UBO_SIZE_U_WAVEALPHA == 16
							   && VK_UBO_SIZE_U_CLIPPLANE == 16
							   && VK_UBO_SIZE_U_FOGCOLOR == 16
							   && VK_UBO_SIZE_U_FOGFIXCOLOR == 16
							   && VK_UBO_SIZE_U_TEX1COLOR == 16
							   && VK_UBO_SIZE_U_ENTITYTEXCOORD == 16 ) ? 1 : -1];
typedef char vk_ubo_span_vec3[( VK_UBO_SIZE_U_VIEWORIGIN >= 12
							   && VK_UBO_SIZE_U_SUNDIRECTION >= 12
							   && VK_UBO_SIZE_U_FOGSVECTOR >= 12
							   && VK_UBO_SIZE_U_FOGTVECTOR >= 12
							   && VK_UBO_SIZE_U_FOGDENSITYTVECTOR >= 12
							   && VK_UBO_SIZE_U_LOCALVIEWORIGIN >= 12
							   && VK_UBO_SIZE_U_SPECORIGIN >= 12 ) ? 1 : -1];
typedef char vk_ubo_span_array[( VK_UBO_SIZE_U_TEXMOD0 == TR_MAX_TEXMODS * 4 * 4 * sizeof( float )
								&& VK_UBO_SIZE_U_TEXMOD1 == TR_MAX_TEXMODS * 4 * 4 * sizeof( float ) ) ? 1 : -1];

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
	   rows put the near plane on -1, outside Vulkan's clip volume. */
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

/* the value-keyed caches are indexed, not walked: each index stores entry+1 (0 = empty), the insert probes from
   the lookup's start, and the index is twice its table, so a probe never runs off the end. */
#define VK_PROG_HASH 512
static int				s_programIndex[VK_PROG_HASH];

static unsigned VK_ProgramHash( unsigned defines )
{
	return ( defines * 2654435761u ) & ( VK_PROG_HASH - 1u );
}

/* the module table is 682 entries and the permutation path asks one (family, defines, stage) triple per stage request; this
   open-addressed index, keyed (family, defines-minus-the-family-byte, isFragment), answers in a probe or two. */
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

/* the lookup key is (family, permutation bits): a program's defines carry the family in the high byte while the offline
   table keeps it in its own field.  Strip the family byte here or every non-STAGE family asks a permutation the table never names. */
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
		int total = ( int )( sizeof( familyKeys ) / sizeof( familyKeys[0] ) );

		for ( i = 0; i < total; i++ ) {
			VkShaderModule v, f;

			VK_Program_Modules( familyKeys[i].family, familyKeys[i].bit, &v, &f );
		}
		/* the summary rides the probe; a family that did not resolve is the
		   defect the census exists for and names itself whatever the probe says */
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
		/* every fixed-size table answers exhaustion the same way: name it through the guard and return the failure the draw path
		   already refuses -- VK_NULL_HANDLE from the pipeline, the set and both rings -- so the surface drops and refusals counts it. */
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

/* The gens whose u_ColorGen branch reads a_Color: the tess vertex array is
   their operand, not a uniform.  VK_StageNeedsCPUColor deliberately excludes
   them -- their svars bodies would re-apply identityLight the vertex stage
   already applies -- so the pack has to be told separately to carry the array
   instead of substituting white. */
qboolean VK_StageReadsVertexColor( const shaderStage_t *pStage )
{
	if ( !pStage )
	{
		return qfalse;
	}
	switch ( pStage->rgbGen )
	{
	case CGEN_EXACT_VERTEX:
	case CGEN_VERTEX:
	case CGEN_ONE_MINUS_VERTEX:
		return qtrue;
	default:
		break;
	}
	/* the sprite feed retires a gen-entity stage to the vertex generators, and
	   RB_DrawSprite has already stamped the batch's own rgba into the array, so
	   the guard that keeps a static model from reading its neighbour's colours
	   does not apply here */
	if ( r_spriteListOpen
		 && ( pStage->rgbGen == CGEN_ENTITY || pStage->rgbGen == CGEN_ONE_MINUS_ENTITY ) )
	{
		return qtrue;
	}
	return qfalse;
}

/* the alpha-only vertex gens: AGEN_VERTEX/ONE_MINUS_VERTEX read a_Color.a on their own
   (vk_stage.vert u_AlphaGen 4/5), so the array has to reach the pack even when the stage's
   rgbGen is not a vertex gen.  A decal stage carrying `alphaGen vertex` alone -- EF2's whole
   goosplat/splat family -- drew with a_Color unbound, so its alpha came off the ring as
   garbage and the quad painted opaque. */
qboolean VK_StageReadsVertexAlpha( const shaderStage_t *pStage )
{
	if ( !pStage )
	{
		return qfalse;
	}
	return pStage->alphaGen == AGEN_VERTEX || pStage->alphaGen == AGEN_ONE_MINUS_VERTEX;
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
	/* the slot handle each view was resolved from.  A level load frees world
	   images -- R_FreeImage destroys the view and zeroes the slot -- and the
	   latch then names a view that is no longer the slot's own, so the handle
	   is what a bind re-validates against. */
	int				texNum[2];
	/* the image_t each unit's view came from: the census compares what a stage
	   asked for with what was bound, and a view handle alone cannot say which
	   art it is */
	image_t			*texImage[2];
	int				currentTmu;
	qboolean		scissor;
	VkRect2D		scissorRect;
	qboolean		depthTest;
	qboolean		depthWrite;
	qboolean		colorMask[4];
	qboolean		mirrored;
	qboolean		stencil;
	/* the shadow volume's own state: the stencil test/ops it counts with, the
	   cull face it flips between volumes, and the colour it never writes */
	qboolean		stencilTest;
	int				stencilFunc;
	int				stencilRef;
	unsigned		stencilMask;
	int				stencilOpFail;
	int				stencilOpZFail;
	int				stencilOpPass;
	int				cullFaceOverride;	/* 0 none, else VK_FRONT / VK_BACK */
	qboolean		waveBound;
	VkImageView		waveView;
	int				alphaTest;
} vkShadeState_t;

/* an unarmed colour mask writes every channel: the shadow volumes are the only
   draws that clear it, and they put it back themselves */
static vkShadeState_t	s_shade = { .colorMask = { qtrue, qtrue, qtrue, qtrue } };
int						r_spriteListOpen = 0;
/* the sprite count the walk was handed, not the live front-end counter: the walk
   runs as a recorded command, so tr.refdef.num_sprites at draw time is a later
   scene's number and made the batch arithmetic read wrong */
int						r_spriteWalkSurfs = 0;

/* the alpha-test selector is a property of the STAGE (the state word's atest bits): derived at every use, never latched from the
   last state block that ran, so an iterator publishing its own state word cannot leave a GT0 surface with the test switched off. */
static int s_alphaTest;

static int VK_AlphaTestForState( unsigned long stateBits )
{
	switch ( stateBits & VKS_ATEST_BITS ) {
	case VKS_ATEST_GT_0:	return 1;
	case VKS_ATEST_LT_80:	return 2;
	case VKS_ATEST_GE_80:	return 4;
	default:				return 0;
	}
}

void VK_SetFogColor( const vec4_t fColor )
{
	byte rgba[4];

	rgba[0] = (byte)( fColor[0] * 255.0f );
	rgba[1] = (byte)( fColor[1] * 255.0f );
	rgba[2] = (byte)( fColor[2] * 255.0f );
	rgba[3] = 255;
	backEnd.viewParms.fogColorInt = ( rgba[0] ) | ( rgba[1] << 8 ) | ( rgba[2] << 16 ) | ( 255 << 24 );

	/* The GLES3 twin's GL_SetFogColor publishes the same colour into glState.fFogColor;
	   this is the record RB_BeginDrawingView clears a world view's colour plane with.
	   Vulkan kept only the packed int above, so vkState.fFogColor stayed at its zero
	   initializer and a farplane-fog mission cleared the sky/horizon to BLACK instead
	   of the level script's fog colour -- the fog "gone" on Drull while the GLES3 twin
	   still draws it.  Keep both records written here, where the one view's colour is
	   published. */
	vkState.fFogColor[0] = fColor[0];
	vkState.fFogColor[1] = fColor[1];
	vkState.fFogColor[2] = fColor[2];
	vkState.fFogColor[3] = fColor[3];
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
	s_alphaTest = VK_AlphaTestForState( (unsigned long)stateBits );
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

/* the immediate run is one pipeline, so a shadow-state change has to end the run
   the way a topology change does: every setter bumps this generation */
static unsigned s_shadowStateGen;

unsigned VK_Shade_ShadowGen( void )
{
	return s_shadowStateGen;
}

void VK_StencilState( qboolean test, int func, int ref, unsigned mask )
{
	s_shadowStateGen++;
	s_shade.stencilTest = test;
	s_shade.stencilFunc = func;
	s_shade.stencilRef  = ref;
	s_shade.stencilMask = mask;
}

/* the shadow volume pass reads this back when it builds its pipeline key: the
   stencil arm is pipeline state in Vulkan, not a context call */
void VK_Shade_ShadowState( qboolean *test, int *func, int *ref, unsigned *mask,
						   int *fail, int *zfail, int *zpass, int *cullFace,
						   unsigned *colorMask )
{
	unsigned bits = 0;

	if ( s_shade.colorMask[0] ) bits |= VK_COLOR_COMPONENT_R_BIT;
	if ( s_shade.colorMask[1] ) bits |= VK_COLOR_COMPONENT_G_BIT;
	if ( s_shade.colorMask[2] ) bits |= VK_COLOR_COMPONENT_B_BIT;
	if ( s_shade.colorMask[3] ) bits |= VK_COLOR_COMPONENT_A_BIT;

	*test     = s_shade.stencilTest;
	*func     = s_shade.stencilFunc;
	*ref      = s_shade.stencilRef;
	*mask     = s_shade.stencilMask;
	*fail     = s_shade.stencilOpFail;
	*zfail    = s_shade.stencilOpZFail;
	*zpass    = s_shade.stencilOpPass;
	*cullFace = s_shade.cullFaceOverride;
	*colorMask = bits;
}

/* the 2D window's state block ran last: this gate holds depth test off for the UI whatever a UI shader's own state bits say and
   opens on the next 3D draw, or the viewmodel draws over the health bar and the pause menu loses surfaces. */
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

/* each wait names itself in and out with its thread and the caller's business, so the last row before a stop is the wait that never returned. */

VkResult VK_Sync_WaitForFences( VkFence fence, VkBool32 waitAll, const char *site, const char *why )
{
	VkResult result = qvk.WaitForFences( vk.device, 1, &fence, waitAll, UINT64_MAX );

	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_FENCE, why ? why : site, result );
	}
	return result;
}

VkResult VK_Sync_DeviceWaitIdle( const char *site, const char *why )
{
	VkResult result = qvk.DeviceWaitIdle( vk.device );

	if ( result != VK_SUCCESS ) {
		VK_Guard_Hit( VK_GSITE_DEVICE, why ? why : site, result );
	}
	return result;
}

/* ---- textures ------------------------------------------------------------ */

void VK_SelectTexture( int tmu )
{
	s_shade.currentTmu = tmu;
}

/* the drawn colour of a stage's texture, read back off the device once per image at first bind: what the sampler holds is the one
   thing no offline reading answers. */
/* the whole texture table, not half of it: MAX_DRAWIMAGES is 8192 and a row that
   stopped at 4096 silently answered for no image past the halfway mark */
#define VK_TEXEL_MAX VK_MAX_TEXTURES

/* the walk a bind rides in: the row's only cheap way to name its caller's family */

static void VK_Shade_BindImage( image_t *image, int tmu )
{
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;

	if ( !image || image->texnum < 0 ) {
		s_shade.texView[tmu] = VK_NULL_HANDLE;
		s_shade.texSampler[tmu] = VK_NULL_HANDLE;
		s_shade.texImage[tmu] = image;
		s_shade.texBound[tmu] = 0;
		return;
	}
	/* texnum is unsigned: 0 is "no texture" at every other reader of it.  A
	   stage that binds it draws its surface with NO image -- the sampler reads
	   the unit's previous view, or nothing at all, which is the surface that
	   looks like it never loaded.  Gated like every other row: the diag boot is
	   what arms it. */
	if ( image->texnum == 0 ) {
		s_shade.texView[tmu] = VK_NULL_HANDLE;
		s_shade.texSampler[tmu] = VK_NULL_HANDLE;
		s_shade.texImage[tmu] = image;
		s_shade.texBound[tmu] = 0;
		return;
	}
	view = VK_Texture_View( image->texnum );
	if ( view == VK_NULL_HANDLE ) {
		/* the slot is empty: binding would leave the previous unit's view armed
		   for a stage that never binds this one, and that view may be the one a
		   level load just destroyed */
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "bind: texture has no view", VK_INCOMPLETE );
		s_shade.texView[tmu] = VK_NULL_HANDLE;
		s_shade.texSampler[tmu] = VK_NULL_HANDLE;
		s_shade.texBound[tmu] = 0;
		return;
	}
	/* wrapClampModeX/Y hold the legacy wrap enum: REPEAT is 10497, not 0 */
	VK_Texture_Sampler( ( image->wrapClampModeX != VK_TWRAP_REPEAT
						  || image->wrapClampModeY != VK_TWRAP_REPEAT ), qfalse, qtrue, &sampler );
	s_shade.texView[tmu] = view;
	s_shade.texSampler[tmu] = sampler;
	s_shade.texNum[tmu] = image->texnum;
	s_shade.texImage[tmu] = image;
	s_shade.texBound[tmu] = 1;

}

/* the latch names slots a registration recycles, and the walk is parked here */
void VK_Shade_RegistrationReset( void )
{
	int tmu;

	for ( tmu = 0; tmu < 2; tmu++ ) {
		s_shade.texBound[tmu]  = 0;
		s_shade.texView[tmu]   = VK_NULL_HANDLE;
		s_shade.texSampler[tmu] = VK_NULL_HANDLE;
		s_shade.texNum[tmu]    = -1;
		s_shade.texImage[tmu]  = NULL;
	}
	s_shade.waveBound = qfalse;
	s_shade.waveView  = VK_NULL_HANDLE;

	/* A registration is about to retire every world image, and both set caches
	   outlive one.  The generation is the flush: it is bumped here whatever the
	   frees below report, so no set written before this point can be bound
	   without being rewritten first -- the invariant is stated, not inferred
	   from which images happened to be freed. */
	VK_Draw_TextureGenBump();
}

/* a latched unit may still write into a descriptor: a level load destroys world images out from under the latch, so it is re-validated
   against the slot's own view before vkUpdateDescriptorSets (a NULL view or a reused view disarms); reads no image_t field. */
static VkImageView VK_Shade_LiveView( int tmu )
{
	VkImageView live;

	if ( !s_shade.texBound[tmu] ) {
		return VK_NULL_HANDLE;
	}
	live = VK_Texture_View( s_shade.texNum[tmu] );
	if ( live == VK_NULL_HANDLE || live != s_shade.texView[tmu] ) {
		s_shade.texBound[tmu] = 0;
		s_shade.texView[tmu] = VK_NULL_HANDLE;
		s_shade.texSampler[tmu] = VK_NULL_HANDLE;
		return VK_NULL_HANDLE;
	}
	return live;
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
	/* R_BindAnimatedImage @0x00034190: the clock is FUNCTABLE_SIZE * (speed * time) shifted down by FUNCTABLE_SIZE2, negatives
	   clamped to 0 then wrapped (BUNDLE_ANIMATE_ONCE clamps to the last frame).  The scale is the wave-table size, not 60: 60 fast-forwarded every animMap 60x. */
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
	/* the view origin in the space a_Position and tess.xyz live in, which is what
	   retail's own RB_Calc* arms read (tr_shade_calc.c) and what the shader's
	   viewer-vector arms need on entity geometry -- the world pass passes both
	   names the same value */
	VK_SetUniform3fv( p, VK_U_VIEWORIGIN, backEnd.ori.viewOrigin );
	/* frame-carry: read the record's refdef, not the live one */
	VK_SetUniform3fv( p, VK_U_VIEWAXIS0, backEnd.refdef.viewaxis[0] );
	VK_SetUniform3f( p, VK_U_LOCALVIEWORIGIN,
						backEnd.refdef.vieworg[0], backEnd.refdef.vieworg[1], backEnd.refdef.vieworg[2] );

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
	if ( off + bytes > s_uboHighWater ) {
		s_uboHighWater = off + bytes;
	}
	s_uboDirty = qtrue;
}

/* the flat-world diagnostic: 0 off, 1 red, 2 the batch's own vertex colour.
   World batches only, so entities and the UI stay reference. */
/* the gate's four inputs, sampled at the last stage draw that asked: a boot
   whose worldflat row is silent has to name which input refused it */
int VK_WorldFlatMode( void )
{
	int mode;

	if ( !r_worldFlat || r_worldFlat->integer <= 0 || backEnd.in2D
		 || backEnd.currentEntity != &tr.worldEntity ) {
		return 0;
	}
	mode = r_worldFlat->integer;
	return ( mode > 2 ) ? 2 : mode;
}

void VK_SetupStageUniforms( vkProgram_t *p, const shaderStage_t *pStage, const shader_t *shader )
{
	float	baseColor[4];
	qboolean	cpuColor;
	qboolean	cpuTexCoords;
	int		b;
	int		flat;

	VK_SetupViewUniforms( p );

	if ( !pStage )
	{
		return;
	}

	cpuColor = VK_StageNeedsCPUColor( pStage );
	cpuTexCoords = VK_StageNeedsCPUTexCoords( pStage );

	/* 2D carries Draw_SetColor in a_Color, so u_BaseColor stays 1 there for the gens that read the vertex.  Forcing 1 on the two
	   gens that read it anyway made every constant-colour 2D stage draw white: the retail viewmodes tint through rgbGen const. */
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

	/* a const colour cannot change between binds, so a move names a wrong feed; the vent's purple flash reads here */

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

	/* The stage's own selector, not the last state block's: retail's GL_State is
	   called with the stage's stateBits for every stage, including the
	   lightmapped-multitexture pass, whose iterator publishes a state word of its
	   own.  Taking the latch there switched alphaFunc GT0 off for exactly the
	   alpha-cut world art that asks for it. */
	VK_SetUniform1i( p, VK_U_ALPHATEST,
					 VK_AlphaTestForState( (unsigned long)pStage->stateBits ) );
	VK_SetUniform4f( p, VK_U_TEX1COLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	/* published by every stage draw so a non-world draw can never inherit a
	   world draw's mode through a shared ring slice */
	flat = VK_WorldFlatMode();
	VK_SetUniform1i( p, VK_U_FLATMODE, flat );

	VK_SetUniform1i( p, VK_U_ACFF, cpuColor ? 0 : (int)pStage->acff );
}

void VK_SetupFogUniforms( vkProgram_t *p, int fogNum, qboolean fogDensityArm )
{
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
		/* the walk's own frame copy -- RB_SetupFog writes the colour here and every
		   other field this function reads comes from backEnd.viewParms */
		unsigned colorInt = (unsigned)backEnd.viewParms.fogColorInt;
		const byte *byteColor = ( const byte * )&colorInt;

		/* the shader does floor(u_FogColor * 255) for both fog colour gens, so the
		   bytes have to arrive divided by 255 -- a raw u32 copy reinterpreted the
		   first byte as a float and left the other three at whatever the block held */
		VK_SetUniform4f( p, VK_U_FOGCOLOR, byteColor[0] / 255.0f, byteColor[1] / 255.0f,
						 byteColor[2] / 255.0f, byteColor[3] / 255.0f );
		VK_SetUniform1f( p, VK_U_FOGOOLEN, backEnd.viewParms.fog.oolen );
		VK_SetUniform1i( p, VK_U_FOGDENSITYARM, fogDensityArm ? 1 : 0 );

		return;
		return;
	}

	{
		fog_t	*fog = &tr.world->fogs[fogNum];
		float	tcScale;
		float	sVector[3], tVector[3];
		float	sConstant, tConstant, viewOutside;
		vec3_t	delta;
		unsigned colorInt = fog->colorInt;
		const byte *byteColor = ( const byte * )&colorInt;

		VK_SetUniform4f( p, VK_U_FOGCOLOR, byteColor[0] / 255.0f, byteColor[1] / 255.0f,
						 byteColor[2] / 255.0f, byteColor[3] / 255.0f );

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
		VK_SetUniform1f( p, VK_U_FOGOOLEN, backEnd.viewParms.fog.oolen );
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
		VK_IteratorTouched( "wire overlay draw" );
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
/* vkStreamVertex_t and VK_STREAM_STRIDE are published by tr_vbo.h: the static arena holds exactly the bytes VK_Stream_Pack writes here */
/* The batch-relative vertex index the draw reads at footprint position i.  Off
   the arena arm the caller's array is already batch-relative; on it, the block
   holds arena-relative values, so each maps back through the item that owns its
   position -- and through that item's OWN block.  A batch's items are not
   adjacent in the arena (the compose is what makes them one draw: devrun=0 on
   every shipped boot says no run ever reached MIN_IBO_RUN), so one block stepped
   by position decoded another item's indexes and priced a correct batch as
   carrying out-of-range vertices. */

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


/* ---- the static arena's draw arm ----------------------------------------- */
/* a batch whose every surface is already arena-resident draws its index blocks directly instead of re-packing the tess stream; the
   lightmapped-multitexture iterator is the only caller, and item runs of MIN_IBO_RUN or more draw where they lie. */
static qboolean RB_VBO_StaticDrawBatch( const shaderCommands_t *input,
										const float *texcoord0, const float *texcoord1,
										int tcStrideFloats, const unsigned char *colors,
										const float *normals, const void *stage )
{
	vkStaticItemView_t	items[VK_STATIC_BATCH_MAX];
	int					numItems;
	VkCommandBuffer		cmd;

	/* Not this arm's batch: the streaming path owns it. */
	if ( !RB_VboStaticArenaBatch() ) {
		return qfalse;
	}

	/* From here the batch's tess arrays were never written, so a fallback to
	   the streaming pack would read another batch's bytes.  An inconsistency is
	   named and the draw dropped: a missing draw is diagnosable, garbage is not. */
	if ( input != &tess || stage != NULL ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, "static batch drawn off the tess command", VK_INCOMPLETE );
		return qtrue;
	}
	if ( colors != (const unsigned char *)tess.constantColor255 ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, "static batch colour source is not the constant", VK_INCOMPLETE );
		return qtrue;
	}
	if ( tcStrideFloats != 4
		 || texcoord0 != (const float *)input->texCoords[0][0]
		 || texcoord1 != (const float *)input->texCoords[0][0] + 2
		 || normals != input->normal[0] ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, "static batch texcoord source is not the tess store", VK_INCOMPLETE );
		return qtrue;
	}
	if ( !input->shader || input->shader->needsNormal
		 || input->currentStageIteratorFunc != RB_StageIteratorLightmappedMultitextureUnfogged ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, "static batch shader is not the lightmapped signature", VK_INCOMPLETE );
		return qtrue;
	}

	numItems = RB_VboStaticBatchItems( items, VK_STATIC_BATCH_MAX );
	if ( numItems <= 0 ) {
		VK_Guard_Hit( VK_GSITE_QUIRK, "static batch items do not tile the batch", VK_INCOMPLETE );
		return qtrue;
	}

	/* the census of the bytes this draw is about to read: a region that is not
	   the surface's own names itself here, before the pixels are argued about */

	/* THE ARENA IS A PACK CACHE, NOT A SECOND DRAW PATH.  Whatever the two paths
	   could disagree about -- which buffer is bound, what firstIndex means, how
	   many draws a batch becomes, whether the bytes came from a device copy --
	   is removed by making the arena produce the STREAMING path's own input: its
	   cached vertices are copied into the ring the stream would have packed into,
	   its arena-absolute indexes are rebased to batch-relative, and the draw is
	   the stream's own VK_Draw_BindRingOffset plus one DrawIndexed over the whole
	   batch.  A cached surface therefore draws through the identical command
	   sequence a streamed one does, so the two cannot differ in pixels at all. */
	{
		const byte	*stageBytes = (const byte *)RB_VboStaticStageFloats();
		vkIndex_t	*idx;
		byte		*dst;
		int			base = 0, indexBase = 0, tessSlots, k, o;

		if ( !stageBytes ) {
			VK_Guard_Hit( VK_GSITE_QUIRK, "static batch has no staging arena", VK_INCOMPLETE );
			return qtrue;
		}

		tessSlots = ( input->numVertexes * VK_STREAM_STRIDE + VK_TESS_VERTEX_BYTES - 1 )
					/ VK_TESS_VERTEX_BYTES;
		idx = VK_VertexRing_ReserveIndexes( input->numIndexes, &indexBase );
		if ( !idx ) {
			VK_Guard_Hit( VK_GSITE_ALLOC, "tess index ring exhausted", VK_INCOMPLETE );
			return qtrue;
		}
		if ( !VK_VertexRing_ReserveBytes( tessSlots * VK_TESS_VERTEX_BYTES, &base ) ) {
			VK_VertexRing_RollBack( 0, input->numIndexes );
			VK_Guard_Hit( VK_GSITE_ALLOC, "tess ring exhausted", VK_INCOMPLETE );
			return qtrue;
		}

		dst = ( byte * )VK_VertexRing_Mapped() + base;
		o = 0;
		for ( k = 0; k < numItems; k++ ) {
			const vkIndex_t	*block = RB_VboStaticIndexBlock( items[k].idxFirst );
			int				j;

			if ( !block ) {
				VK_Guard_Hit( VK_GSITE_QUIRK, "static batch item has no index block", VK_INCOMPLETE );
				return qtrue;
			}
			memcpy( dst + ( size_t )items[k].vertStart * VK_STREAM_STRIDE,
					stageBytes + ( size_t )items[k].first * VK_STREAM_STRIDE,
					( size_t )items[k].numVerts * VK_STREAM_STRIDE );
			/* the block is arena-absolute; the ring draw is batch-relative */
			for ( j = 0; j < items[k].numIdx; j++ ) {
				idx[o + j] = block[j] - ( vkIndex_t )items[k].first
							 + ( vkIndex_t )items[k].vertStart;
			}
			o += items[k].numIdx;
		}
		RB_VboStaticNoteCopied( o );

		cmd = VK_Shade_CommandBuffer();
		if ( !VK_Shade_Bind( cmd, input->shader ) ) {
			/* nothing is bound: recording the draw anyway faults the driver, and
			   VK_Shade_Bind's rows name the reason */
			VK_VertexRing_Flushed();
			return qtrue;
		}
		VK_Draw_BindRingOffset( cmd, base );
		VK_Draw_DrawIndexed( cmd, input->numIndexes, indexBase, 0 );
		VK_VertexRing_Flushed();
	}

	return qtrue;
}

/* the pack as a JobPool range: every element reads and writes only its own index, so which worker runs which chunk cannot move a byte */
/* measured break-even: docs/controls/threadroot shows a surface this small costs more to hand out than to run inline */
#define VK_PACK_CHUNK	2048

typedef struct {
	vkStreamVertex_t		*verts;
	const shaderCommands_t	*input;
	const float				*tc0;
	const float				*tc1;
	int						 tcStride;
	const unsigned char		*colors;
	const float				*normals;
} vkPackCtx_t;

static void VK_Stream_PackRange( void *vctx, int begin, int end )
{
	const vkPackCtx_t	*c = (const vkPackCtx_t *)vctx;
	int					 i;

	for ( i = begin; i < end; i++ ) {
		VK_Stream_Pack( &c->verts[i], c->input, i, c->tc0, c->tc1, c->tcStride,
						c->colors, c->normals );
	}
}

/* the draw: pack into the ring slice the batch reserves, one indexed draw */
void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const vkIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals,
							const void *stage )
{
	if ( !input || input->numVertexes <= 0 || numIndexes <= 0 ) {
		return;
	}

	/* the static arena's arm: nothing is packed and no tess array is read */
	if ( RB_VBO_StaticDrawBatch( input, texcoord0, texcoord1, tcStrideFloats,
								 colors, normals, stage ) ) {
		return;
	}

	RB_VBO_DrawTessStream( input, indexes, numIndexes, texcoord0, texcoord1,
						   tcStrideFloats, colors, normals, stage );
}

/* The streaming half alone.  A draw whose texcoords and colours the walk
   computed itself -- the dlight term -- carries no arena bytes, so the arena
   arm's own signature cannot own it: retail draws the term over the surface's
   triangles every frame, and this arm never consults the batch's residency. */
void RB_VBO_DrawTessStream( const shaderCommands_t *input,
							const vkIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals,
							const void *stage )
{
	vkStreamVertex_t	*verts;
	vkIndex_t			*idx;
	int					base = 0, indexBase = 0;
	int					tessSlots;
	VkCommandBuffer		cmd;

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
		if ( !VK_Shade_Bind( cmd, input->shader ) ) {
			VK_VertexRing_Flushed();
			return;
		}
		VK_Draw_BindRingOffset( cmd, base );
		VK_Draw_DrawIndexed( cmd, numIndexes, indexBase, 0 );
		VK_VertexRing_Flushed();
		return;
	}

	/* Both halves are reserved before a byte is packed: a surface whose index
	   half does not fit is skipped whole, and reserving the vertex half first
	   left its packed bytes stranded in the ring on that path.  tessSlots is the
	   reservation expressed in ring slots, so the rollback hands back exactly
	   what ReserveBytes took (a multiple of the slot size rounds to itself). */
	tessSlots = ( input->numVertexes * VK_STREAM_STRIDE + VK_TESS_VERTEX_BYTES - 1 )
				/ VK_TESS_VERTEX_BYTES;
	if ( indexes ) {
		idx = VK_VertexRing_ReserveIndexes( numIndexes, &indexBase );
		if ( !idx ) {
			VK_Guard_Hit( VK_GSITE_ALLOC, "tess index ring exhausted", VK_INCOMPLETE );
			return;
		}
	}
	if ( !VK_VertexRing_ReserveBytes( tessSlots * VK_TESS_VERTEX_BYTES, &base ) ) {
		if ( indexes ) {
			VK_VertexRing_RollBack( 0, numIndexes );
		}
		VK_Guard_Hit( VK_GSITE_ALLOC, "tess ring exhausted", VK_INCOMPLETE );
		return;
	}
	verts = ( vkStreamVertex_t * )( ( byte * )VK_VertexRing_Mapped() + base );
	{
		/* data-parallel by construction, and the JobPool contract fits it element for element: the range is final before the kick,
		   vertex i is a pure function of i and no two chunks write the same byte, so the parallel bytes are the sequential bytes.
		   Below VK_PACK_CHUNK vertices the pool runs it inline. */
		vkPackCtx_t		packCtx;

		packCtx.verts      = verts;
		packCtx.input      = input;
		packCtx.tc0        = texcoord0;
		packCtx.tc1        = texcoord1;
		packCtx.tcStride   = tcStrideFloats;
		packCtx.colors     = colors;
		packCtx.normals    = normals;

		JobPool_ParallelRange( input->numVertexes, VK_PACK_CHUNK, VK_Stream_PackRange, &packCtx );
	}
	if ( indexes ) {
		memcpy( idx, indexes, numIndexes * sizeof( vkIndex_t ) );
	}

	/* A recorded surface's packed bytes are exactly the arena's, so the bake
	   copies them out of the ring it just filled and the next frame draws from
	   the device-local copy instead.  ONLY the pack that is the batch's own
	   invariant geometry may feed it: a term whose texcoords and colours the
	   walk computed itself -- the dlight projection, which runs after the world
	   draw inside the same batch -- packs the same vertex count out of the same
	   surface, so a bake there would copy the term's texcoords and colours into
	   the world's region and the arena would draw them for the rest of the
	   session.  The signature below is the one the lightmapped iterator packs. */
	if ( tcStrideFloats == 4
		 && texcoord0 == input->texCoords[0][0]
		 && texcoord1 == input->texCoords[0][0] + 2
		 && colors == (const unsigned char *)input->constantColor255
		 && normals == input->normal[0] ) {
		RB_VboStaticBakeFrame( base );
		/* the same gate, the same audit: the ring bytes this draw reads are
		   compared with the surfaces the batch claims, so a pack that is not the
		   surface's own is named on the frame it is drawn, not argued about on
		   the frame the arena serves it from */
	}

	if ( stage && colors == NULL
		 && texcoord0 == input->texCoords[0][0]
		 && texcoord1 == input->texCoords[0][0] + 2
		 && tcStrideFloats == 4 ) {
		VK_VertexRing_StreamStore( input->numVertexes, numIndexes,
								   texcoord0, texcoord1, tcStrideFloats,
								   colors, normals, base, indexBase );
	}

	cmd = VK_Shade_CommandBuffer();
	if ( !VK_Shade_Bind( cmd, input->shader ) ) {
			/* nothing is bound: recording the draw anyway faults the driver, and VK_Shade_Bind's rows name the reason */
		VK_VertexRing_Flushed();
		return;
	}

	VK_Draw_BindRingOffset( cmd, base );
	VK_Draw_DrawIndexed( cmd, numIndexes, indexBase, 0 );
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

/* ---- the immediate stream, pipeline and descriptor set ------------------ */
/* one layout: the uniform block at 0, the bundles and wave table at 1..3.  VK_SHADE_SETS lives in vk_local.h beside the pool budget
   that carries it; 4096 is sized off the boot that exhausted 1024 with 143,383 refusals (1,887 draw-surfs, 244 textures), and the fill row below names the truth. */

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

/* the pipeline table is the set table's shape: keyed on what a draw asks (state bits, family, defines, cull, polygon offset, topology),
   boot-persistent and refusing the draw on exhaustion; the measured peak was 252 shade draws a frame, so 4096 is headroom, not a proven need. */
#define VK_SHADE_PIPELINES 4096

static vkShadePipeline_t	vk_shadePipelines[VK_SHADE_PIPELINES];
static int					vk_numShadePipelines;

/* both cache tables are looked up by binding, not by index, and both were searched by walking every filled entry, so bind cost grew
   with the boot (a 1792-entry chain of seven pointer compares per bind).  Each index stores entry+1 (0 = none), the insert probes the
   lookup's own window, and the index is twice its table, so a key that was inserted is always found and no probe runs off the end. */
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
/* A heavy frame (the Drull effect set) can ask for more stages than any fixed
   ring holds.  Exhaustion used to drop every draw after the first refused one
   for the rest of the frame -- the whole 3D view for as long as the effect ran.
   The ring now grows from the previous frame's demand, so one busy frame costs
   that frame's tail and the next frame is sized for the load. */
#define VK_SHADE_UBO_SLICES_MAX		16384
#define VK_SHADE_UBO_ALIGN_MAX		256
/* every draw writes the uniforms its own permutation reads; the floor keeps the block's head alive */
#define VK_UBO_HIGHWATER_FLOOR		256
static int						vk_shadeUboSliceBytes = VK_UBO_SIZE_PAD;
static int						vk_shadeUboNext[VK_FRAMES_IN_FLIGHT];
static int						vk_shadeUboSlices[VK_FRAMES_IN_FLIGHT];
static int						vk_shadeUboDemand[VK_FRAMES_IN_FLIGHT];
static VkDeviceSize				vk_shadeUboLast[VK_FRAMES_IN_FLIGHT];
static qboolean					vk_shadeUboLastValid[VK_FRAMES_IN_FLIGHT];
/* The block as it was last published to this slot's ring: only the bytes
   decide whether a draw may share the previous slice. */
static unsigned char			vk_shadeUboShadow[VK_FRAMES_IN_FLIGHT][VK_UBO_SIZE_PAD];
/* the ring's mapping came out host-cached: the descriptors-ready row names it */
static qboolean					uboSawCached;

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
	/* The texture generation the set was written in.  A registration retires
	   every world image, and this table outlives one, so a hit is only a hit if
	   it was written after the last create/destroy -- otherwise the set names a
	   view whose image is gone and the draw samples freed memory: black. */
	unsigned				gen;
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
	uboSawCached = qfalse;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = ( VkDeviceSize )vk_shadeUboSliceBytes * VK_SHADE_UBO_SLICES;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	for ( i = 0; i < VK_FRAMES_IN_FLIGHT; i++ ) {
		int memType;

		result = qvk.CreateBuffer( vk.device, &bufferInfo, NULL, &vk_shadeUbo[i] );
		if ( !VK_Guard_Result( VK_GSITE_ALLOC, result, "vkCreateBuffer(ubo)" ) ) {
			return qfalse;
		}
		qvk.GetBufferMemoryRequirements( vk.device, vk_shadeUbo[i], &reqs );
		memset( &allocInfo, 0, sizeof( allocInfo ) );
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		/* the ring is written per draw, so prefer the host-cached type and fall back to plain host-visible (HOST_COHERENT either way
		   keeps the device read correct).  FindType answers an int and -1 means no match: the field is unsigned, so storing -1 first made
		   both guards vacuous and read memoryTypes[0xFFFFFFFF] -- the Pixel 8 bring-up SEGV.  VK_Memory_AllocFor runs this shape. */
		memType = VK_Memory_FindType( reqs.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
				| VK_MEMORY_PROPERTY_HOST_CACHED_BIT );
		if ( memType < 0 ) {
			memType = VK_Memory_FindType( reqs.memoryTypeBits,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		}
		if ( memType < 0 ) {
			VK_Guard_Hit( VK_GSITE_ALLOC, "no host-visible memory type for the shade ubo ring",
						  VK_ERROR_OUT_OF_DEVICE_MEMORY );
			return qfalse;
		}
		if ( !i ) {
			uboSawCached = ( ( vk.memProps.memoryTypes[memType].propertyFlags
							   & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ) != 0 );
		}
		allocInfo.memoryTypeIndex = ( uint32_t )memType;
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
		vk_shadeUboSlices[i] = VK_SHADE_UBO_SLICES;
		vk_shadeUboDemand[i] = 0;
	}

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
	memset( vk_shadeUboLastValid, 0, sizeof( vk_shadeUboLastValid ) );
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

/* the one buffer binding a cached set carries; a ring grow moves it to the new
   buffer, so every set the grown slot owns has to be rewritten before it binds */
static void VK_Shade_UboRewrites( int slot )
{
	VkDescriptorBufferInfo		buffer;
	VkWriteDescriptorSet		write;
	int							at;

	memset( &buffer, 0, sizeof( buffer ) );
	buffer.buffer = vk_shadeUbo[slot];
	buffer.offset = 0;
	buffer.range  = VK_UBO_SIZE_PAD;

	memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	write.pBufferInfo = &buffer;

	for ( at = 0; at < vk_numShadeSets; at++ ) {
		if ( !vk_shadeSetKeys[at].written || vk_shadeSetKeys[at].slot != slot ) {
			continue;
		}
		write.dstSet = vk_shadeSets[at];
		qvk.UpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
	}
}

/* one ring per frame in flight, sized by the last frame that filled it.  The
   slot's own commands have completed before its frame opens, so the old buffer
   is retired only after every reader of it is done. */
static qboolean VK_Shade_UboGrow( int slot )
{
	VkBufferCreateInfo		bufferInfo;
	VkMemoryRequirements	reqs;
	VkMemoryAllocateInfo	allocInfo;
	VkBuffer				buffer = VK_NULL_HANDLE;
	VkDeviceMemory			memory = VK_NULL_HANDLE;
	void					*mapped = NULL;
	VkResult				result;
	int						memType;
	int						slices;

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT || !vk_shadeUboMapped[slot] ) {
		return qfalse;
	}
	slices = vk_shadeUboSlices[slot] * 2;
	if ( slices > VK_SHADE_UBO_SLICES_MAX ) {
		slices = VK_SHADE_UBO_SLICES_MAX;
	}
	if ( slices <= vk_shadeUboSlices[slot] ) {
		return qfalse;
	}

	if ( qvk.DeviceWaitIdle ) {
		qvk.DeviceWaitIdle( vk.device );
	}

	memset( &bufferInfo, 0, sizeof( bufferInfo ) );
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = ( VkDeviceSize )vk_shadeUboSliceBytes * slices;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	result = qvk.CreateBuffer( vk.device, &bufferInfo, NULL, &buffer );
	if ( result != VK_SUCCESS ) {
		return qfalse;
	}
	qvk.GetBufferMemoryRequirements( vk.device, buffer, &reqs );

	memType = VK_Memory_FindType( reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			| VK_MEMORY_PROPERTY_HOST_CACHED_BIT );
	if ( memType < 0 ) {
		memType = VK_Memory_FindType( reqs.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	}
	if ( memType < 0 ) {
		qvk.DestroyBuffer( vk.device, buffer, NULL );
		return qfalse;
	}

	memset( &allocInfo, 0, sizeof( allocInfo ) );
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = reqs.size;
	allocInfo.memoryTypeIndex = ( uint32_t )memType;
	result = qvk.AllocateMemory( vk.device, &allocInfo, NULL, &memory );
	if ( result != VK_SUCCESS ) {
		qvk.DestroyBuffer( vk.device, buffer, NULL );
		return qfalse;
	}
	result = qvk.BindBufferMemory( vk.device, buffer, memory, 0 );
	if ( result != VK_SUCCESS ) {
		qvk.FreeMemory( vk.device, memory, NULL );
		qvk.DestroyBuffer( vk.device, buffer, NULL );
		return qfalse;
	}
	result = qvk.MapMemory( vk.device, memory, 0, bufferInfo.size, 0, &mapped );
	if ( result != VK_SUCCESS ) {
		qvk.FreeMemory( vk.device, memory, NULL );
		qvk.DestroyBuffer( vk.device, buffer, NULL );
		return qfalse;
	}

	/* the new ring is complete; only now retire the old one */
	qvk.UnmapMemory( vk.device, vk_shadeUboMemory[slot] );
	qvk.FreeMemory( vk.device, vk_shadeUboMemory[slot], NULL );
	qvk.DestroyBuffer( vk.device, vk_shadeUbo[slot], NULL );

	vk_shadeUbo[slot]       = buffer;
	vk_shadeUboMemory[slot] = memory;
	vk_shadeUboMapped[slot] = ( unsigned char * )mapped;
	vk_shadeUboSlices[slot] = slices;
	VK_Shade_UboRewrites( slot );

	return qtrue;
}

/* copy the staging block into the next ring slice; -1 when the ring is out.  A draw whose staging block is byte-identical to the
   previous draw's slice shares it, which is also what lets the bind omit a repeated set: the test is the bytes, not the dirty flag. */
static VkDeviceSize VK_Shade_WriteUBO( void )
{
	int			slot = VK_Shade_CommandBufferSlot();
	int			slice;
	VkDeviceSize offset = 0;

	if ( slot < 0 || slot >= VK_FRAMES_IN_FLIGHT || !vk_shadeUboMapped[slot] ) {
		return ( VkDeviceSize )-1;
	}

	/* the descriptor range is the whole slice, so zero past this draw's writes:
	   an unset uniform must not read another draw's block */
	if ( s_uboHighWater < VK_UBO_SIZE_PAD ) {
		memset( s_ubo + s_uboHighWater, 0, (size_t)( VK_UBO_SIZE_PAD - s_uboHighWater ) );
	}

	if ( vk_shadeUboLastValid[slot]
		 && ( !s_uboDirty
			  || memcmp( s_ubo, vk_shadeUboShadow[slot], (size_t)VK_UBO_SIZE_PAD ) == 0 ) ) {
		return vk_shadeUboLast[slot];
	}

	slice = vk_shadeUboNext[slot];
	if ( slice >= vk_shadeUboSlices[slot] ) {
		static int s_rows;

		if ( s_rows < 8 || ( s_rows % 600 ) == 0 ) {
			VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "uniform ring exhausted for this frame", VK_INCOMPLETE );
		}
		s_rows++;
		return ( VkDeviceSize )-1;
	}
	vk_shadeUboNext[slot] = slice + 1;
	if ( vk_shadeUboNext[slot] > vk_shadeUboDemand[slot] ) {
		vk_shadeUboDemand[slot] = vk_shadeUboNext[slot];
	}

	offset = ( VkDeviceSize )slice * vk_shadeUboSliceBytes;
	memcpy( vk_shadeUboMapped[slot] + offset, s_ubo, VK_UBO_SIZE_PAD );
	memcpy( vk_shadeUboShadow[slot], s_ubo, VK_UBO_SIZE_PAD );
	s_uboDirty = qfalse;
	s_uboHighWater = VK_UBO_HIGHWATER_FLOOR;
	vk_shadeUboLast[slot] = offset;
	vk_shadeUboLastValid[slot] = qtrue;

	return offset;
}

/* the cull mode the shader's cullType and the mirror flag ask for.  FrontFace is
   CLOCKWISE because the window transform negates clip y, so a clip-space triangle
   the engine wound counter-clockwise arrives clockwise here; with that declared,
   CT_FRONT_SIDED keeps its GL meaning (`cull back`) and CT_BACK_SIDED `cull front`. */
static VkCullModeFlagBits VK_Shade_CullMode( cullType_t cull, qboolean mirrored )
{
	switch ( cull ) {
	case CT_TWO_SIDED:
		return VK_CULL_MODE_NONE;
	case CT_BACK_SIDED:
		return mirrored ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
	case CT_FRONT_SIDED:
	default:
		return mirrored ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
	}
}

/* the nibble arrives as its ordinal (VKS_DSTBLEND_x is VKS_SRCBLEND_x << 4), so the two families need their own switches; one table
   over both resolves dst=GL_SRC_COLOR to its DST_COLOR twin.  Retail GL_State @0x0df0 feeds 0x30->0x300 and 0x40->0x301 to glBlendFunc. */
static VkBlendFactor VK_Shade_SrcBlendFactor( unsigned nibble )
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
		return VK_BLEND_FACTOR_ONE;
	}
}

/* the cases are the VKS_DSTBLEND_* constants (the call site shifts the stateBits down by 4).  Written unshifted, as first landed, every
   case missed and every dst factor defaulted to ZERO, so every blended stage REPLACE'd the destination: the black cone, hull, vent gas and halos. */
static VkBlendFactor VK_Shade_DstBlendFactor( unsigned nibble )
{
	switch ( nibble ) {
	case VKS_DSTBLEND_ZERO >> 4:					return VK_BLEND_FACTOR_ZERO;
	case VKS_DSTBLEND_ONE >> 4:						return VK_BLEND_FACTOR_ONE;
	case VKS_DSTBLEND_SRC_COLOR >> 4:				return VK_BLEND_FACTOR_SRC_COLOR;
	case VKS_DSTBLEND_ONE_MINUS_SRC_COLOR >> 4:		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case VKS_DSTBLEND_SRC_ALPHA >> 4:				return VK_BLEND_FACTOR_SRC_ALPHA;
	case VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA >> 4:		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case VKS_DSTBLEND_DST_ALPHA >> 4:				return VK_BLEND_FACTOR_DST_ALPHA;
	case VKS_DSTBLEND_ONE_MINUS_DST_ALPHA >> 4:		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	default:
		return VK_BLEND_FACTOR_ZERO;
	}
}

static void VK_Shade_BuildNote( const char *shader, unsigned stateBits, int family,
								unsigned defines, long long usec );


static qboolean	s_shadeDeferred;

static void VK_Shade_BuildNote( const char *shader, unsigned stateBits, int family,
								unsigned defines, long long usec );
static qboolean VK_Shade_WarmOn( void );
static qboolean VK_Shade_DeferRegister( const char *shader, unsigned stateBits, int family,
										unsigned defines, cullType_t cull, qboolean polygonOffset,
										int topology );

VkPipeline VK_Shade_Pipeline( const char *shader, unsigned stateBits, int family,
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
	long long buildT0;
	unsigned srcNibble, dstNibble;
	uint32_t attribCount;
	uintptr_t pkey[6];
	unsigned phash;
	int newIndex;
	int probe;

	s_shadeDeferred = qfalse;

	/* every 3D tess draw binds through VK_Shade_Bind, so this is where a pipeline
	   that can write depth is noted: the pass's own clear no longer stands for the
	   views after it once one of these has drawn (RB_BeginDrawingView, depth bit) */
	if ( !( stateBits & VKS_DEPTHTEST_DISABLE ) && ( stateBits & VKS_DEPTHMASK_TRUE ) ) {
		vk.depthWritten = qtrue;
	}

	/* the cache key names the modules, so it keys on the permutation bits alone */
	defines &= ~VKP_FAMILY_MASK;

	pkey[0] = stateBits;      pkey[1] = (uintptr_t)family;
	pkey[2] = defines;        pkey[3] = (uintptr_t)cull;
	pkey[4] = polygonOffset;  pkey[5] = (uintptr_t)topology;
	phash = VK_Shade_HashWords( pkey, 6 ) & VK_SHADE_HASH_MASK;

	/* the probe comes first and a hit returns before the program lookup: the key carries the family and the permutation bits, the module
	   lookup's whole input, so a built pipeline proves the modules.  The census is unchanged: a hit still counts a request. */
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

	if ( !VK_Program_Modules( family, defines, &vert, &frag ) ) {
		/* the mask names the permutation the offline build has to add, and the
		   shader names where the content asked for it: a next gap is one boot
		   from diagnosed instead of one census from inferred */
		return VK_NULL_HANDLE;
	}
	if ( vk_numShadePipelines >= VK_SHADE_PIPELINES ) {
		VK_Guard_Hit( VK_GSITE_DESCRIPTOR, "shade pipeline table full", VK_INCOMPLETE );
		return VK_NULL_HANDLE;
	}

	/* A pass is recording, so a build here parks the walk thread and the front end behind it: the key is parked for the next walk's slice and this
	   surface skips one frame.  Warm off or a full park table means today's build, which draws the same surface one frame earlier. */
	if ( vk.passOpen && VK_Shade_WarmOn()
		 && VK_Shade_DeferRegister( shader, stateBits, family, defines, cull, polygonOffset, topology ) ) {
		s_shadeDeferred = qtrue;
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
	/* the clip y negation reverses the winding (VK_Mat4_ClipToWindow) */
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
	/* blending arms when either nibble carries an ordinal -- retail arms on
	   (stateBits & 0xFF).  The compare must not read the blend constants: the
	   dst ordinal arrives shifted, so any constant compare is always true and
	   the flag never fell.  With both clear the factors resolve to ONE/ZERO,
	   the off state's own arithmetic, so shipped pixels did not move. */
	attachment.blendEnable = ( srcNibble || dstNibble ) ? VK_TRUE : VK_FALSE;
	attachment.srcColorBlendFactor = VK_Shade_SrcBlendFactor( srcNibble );
	attachment.dstColorBlendFactor = VK_Shade_DstBlendFactor( dstNibble );
	attachment.colorBlendOp = VK_BLEND_OP_ADD;
	attachment.srcAlphaBlendFactor = VK_Shade_SrcBlendFactor( srcNibble );
	attachment.dstAlphaBlendFactor = VK_Shade_DstBlendFactor( dstNibble );
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

	buildT0 = BE_Microseconds64();
	VK_PCache_ConsultBegin();
	result = qvk.CreateGraphicsPipelines( vk.device, vk.pipelineCache, 1, &info, NULL, &slot->pipeline );
	VK_PCache_ConsultEnd();
	if ( !VK_Guard_Result( VK_GSITE_DESCRIPTOR, result, "vkCreateGraphicsPipelines(shade)" ) ) {
		return VK_NULL_HANDLE;
	}
	VK_Shade_BuildNote( shader, stateBits, family, defines, BE_Microseconds64() - buildT0 );
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
	return slot->pipeline;
}

/* the descriptor set for the (bundle0, bundle1, wave) triple, cached on those views */
static qboolean VK_Shade_DefaultImage( VkImageView *view, VkSampler *sampler )
{
	static VkImageView s_view;
	static VkSampler s_sampler;

	if ( s_view != VK_NULL_HANDLE && s_sampler != VK_NULL_HANDLE ) {
		/* the cache is only as live as the slot it came from: the texture
		   shutdown destroys every view including this one, so a cached handle is
		   re-checked against the slot before it may reach a descriptor */
		if ( tr.whiteImage && VK_Texture_View( (int)tr.whiteImage->texnum ) == s_view ) {
			*view = s_view;
			*sampler = s_sampler;
			return qtrue;
		}
		s_view = VK_NULL_HANDLE;
		s_sampler = VK_NULL_HANDLE;
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

/* Repair a cached set in place.  Only the three sampler bindings move; binding 0
   is the frame slot's own uniform buffer, which no registration touches, and an
   in-place update costs no descriptor allocation -- the pool is sized for the
   table, so re-allocating every set at every registration would spend it. */
static qboolean VK_Shade_RewriteImages( int at )
{
	VkDescriptorImageInfo	images[3];
	VkWriteDescriptorSet	writes[3];
	VkImageView				defView;
	VkSampler				defSampler;
	vkShadeSetKey_t			*key = &vk_shadeSetKeys[at];
	int						i;

	if ( !VK_Shade_DefaultImage( &defView, &defSampler ) ) {
		return qfalse;
	}

	memset( images, 0, sizeof( images ) );
	memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 3; i++ ) {
		images[i].imageView = key->view[i] ? key->view[i] : defView;
		images[i].sampler = key->view[i] ? key->sampler[i] : defSampler;
		images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk_shadeSets[at];
		writes[i].dstBinding = i + 1;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &images[i];
	}
	qvk.UpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	key->gen = VK_Draw_TextureGen();
	return qtrue;
}

/* What the last bind actually wrote: the witness row reads this rather than the
   latch, because the latch is what the stage asked for and the set is what the
   draw samples. */
static VkDescriptorSet	s_lastSet;
static VkImageView		s_lastSetView[3];
static int				s_lastSetIndex = -1;
static qboolean			s_lastSetRewrote;

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
			/* The key matching is not proof the set is still about live images:
			   the same (view, sampler) tuple survives a registration only if the
			   driver handed the identical handle back, and every image behind it
			   was destroyed.  A set written in an older texture generation is
			   rewritten before it is bound, never handed back untouched. */
			qboolean	rebound = qfalse;

			if ( ( !r_texgen3d || r_texgen3d->integer )
				 && vk_shadeSetKeys[at].gen != VK_Draw_TextureGen() ) {
				rebound = VK_Shade_RewriteImages( at );
			}
			s_lastSet = vk_shadeSets[at];
			s_lastSetView[0] = key.view[0];
			s_lastSetView[1] = key.view[1];
			s_lastSetView[2] = key.view[2];
			s_lastSetIndex = at;
			s_lastSetRewrote = rebound;
			return vk_shadeSets[at];
		}
	}
	if ( vk_numShadeSets >= VK_SHADE_SETS ) {
		/* exhaustion refuses the draw rather than aliasing a live set: slot 0 is already bound inside the command buffer being recorded,
		   and storing the new key over vk_shadeSetKeys[0] makes the original miss forever.  VK_NULL_HANDLE becomes the same
		   s_frameShadeDrops++ refusal -- the surface drops, the frame stays correct, and the guard row names the reason. */
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
	vk_shadeSetKeys[found].gen = VK_Draw_TextureGen();
	for ( probe = 0; probe < VK_SHADE_HASH_SIZE; probe++ ) {
		int *at = &vk_shadeSetHash[( shash + probe ) & VK_SHADE_HASH_MASK];

		if ( !*at ) {
			*at = found + 1;
			break;
		}
	}
	s_lastSet = set;
	s_lastSetView[0] = v0;
	s_lastSetView[1] = v1;
	s_lastSetView[2] = wave;
	s_lastSetIndex = found;
	s_lastSetRewrote = qtrue;
	return set;
}

/* ---- the black-batch witness --------------------------------------------- */

/* A black batch is one of three things and no row in the port told them apart:
   the set names a view no live image owns, the view's own bytes are not what the
   image's bytes are, or the draw multiplies the sample by a zero vertex colour.
   This row reads the texel back from the exact view the set was written with, at
   the coordinate the batch's first vertex names, and prints it beside the image's
   centre texel and the colour stream: one armed boot names the answer. */

/* The frame's draw census: retail's trace answers "is the 3D draw list alive" with a count per frame. */
static int s_frameShadeDrops;   /* binds VK_Shade_Bind refused */

/* the shader clock, one row per animated shader per second: tess.shaderTime is what the stage's tcMod/gen advances on, floatTime is
   refdef.time*0.001 and start is the shaderStartTime the ANIMATE_ONCE arm subtracts.  ~1.0 per refdef second is right, ~1000 is a unit
   mix: the model and tempmodel paths write seconds while the sprite path writes a negated millisecond value. */

/* the frame the census is mid-way through: the harness reads it beside the readback */

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

/* The census the boot answers: which permutation keys were requested and
   whether each resolved -- a miss is visible only if a row carries it, and the
   row names the surface that asked, so the offline census that has to gain the
   mask is read off the boot rather than inferred from the family byte. */

/* a refused bind loses the batch's geometry -- pixels vanish without a hole in the frame -- and the refusals row only prints on a
   batch-count change, so this names the refusing table at the refusal, rate limited. */

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

	/* Every 3D, sky and 2D-batch draw binds its pipeline here, before the draw
	   is recorded, so this is where the pass colour clear is paid when the draw
	   that follows can read the plane (VK_DeferredClear_Flush).  A refused bind
	   still pays it: the clear is what the pass would have written anyway. */
	VK_DeferredClear_Flush( cmd );

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
		v0 = VK_Shade_LiveView( 0 );
	}
	if ( s_shade.texBound[1] ) {
		v1 = VK_Shade_LiveView( 1 );
	}
	/* a unit the stage's own bundles never name must not serve the previous stage's art: the latch survives the stage boundary and
	   LiveView only disarms a dead or retargeted slot, so a live foreign view rode every following single-bundle stage's descriptor.
	   Retail's single-texture stage never reads TMU1; a program without VKP_LIGHTMAP declares no u_Tex1 at all. */
	if ( !( s_shade.prog->defines & VKP_LIGHTMAP ) ) {
		v1 = VK_NULL_HANDLE;
	}

	/* The bind-side census of the world pass: a lightmapped program whose base
	   or lightmap unit resolved to no image draws its surface with nothing to
	   sample, which is the surface that looks like its texture never loaded.
	   Names the shader and, per unit, the image the walk asked for and the
	   texture it held; the diag gate is what arms it. */
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
	VK_Draw_BindPipeline( cmd, pipeline );
	VK_Draw_BindSet( cmd, vk_shadeLayout, set, uboOffset, 1 );
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
		/* back to the shader's own cull: the volume pass names its face per draw */
		s_shadowStateGen++;
		s_shade.cullFaceOverride = 0;
	}
}

void vkDisable( int cap )
{
	if ( cap == VK_POLYGON_OFFSET_FILL ) {
		s_polygonOffset = qfalse;
		s_shade.polygonOffset = qfalse;
	} else if ( cap == VK_CULL_FACE ) {
		/* GL with culling off: the finish overlay draws cull-free */
		s_shadowStateGen++;
		s_shade.cullFaceOverride = -1;
	}
}

void vkCullFace( int mode )
{
	/* the shadow passes flip the cull between their two volumes */
	s_shadowStateGen++;
	s_shade.cullFaceOverride = mode;
}

void vkStencilOp( int fail, int zfail, int zpass )
{
	s_shadowStateGen++;
	s_shade.stencilOpFail  = fail;
	s_shade.stencilOpZFail = zfail;
	s_shade.stencilOpPass  = zpass;
}

void vkColorMask( qboolean r, qboolean g, qboolean b, qboolean a )
{
	s_shadowStateGen++;
	s_shade.colorMask[0] = r;
	s_shade.colorMask[1] = g;
	s_shade.colorMask[2] = b;
	s_shade.colorMask[3] = a;

	/* The mask is a pipeline-key field, so a mask left off blacks out every
	   later key that reads it; the row names the transition and who set it. */
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
	   last clamped.  This backend has to re-issue it, and the client's view rect can
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

/* a draw whose clipped footprint covers a frame is the wedge shape the raw vertex box never sees (corners outside the volume still
   cross the frame), so the triangles are clipped first and the survivor's screen area is measured; the row also carries the smallest
   box this shader drew, since a wall the camera stands beside fills the frame too. */
typedef struct {
	const shader_t	*shader;
	int				 frame;
	float			 minArea;
} vkFootprintKey_t;

#define VK_FOOTPRINT_KEYS 64


/* The position of batch-local vertex vi: the tess arrays for a streamed batch,
   the static arena's staging copy for an arena batch whose generator never ran. */

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


void VK_GlErrAt( const char *what )
{
	(void)what;
}

void VK_CensusShutdownAll( void ) { }

/* one attempt, whatever the outcome: with only a `>= 0' guard a FAILED creation left the handle at -1 and every later bind re-ran the
   whole LUT fill plus vkCreateImage + upload + publish.  Now the attempt happens once and says what it did. */
static qboolean	s_waveTableTried;

void VK_CreateWaveTable( void )
{
	static float	lut[FUNCTABLE_SIZE * 5];
	int				i;
	int				handle = -1;

	if ( s_waveTableHandle >= 0 || s_waveTableTried ) {
		return;
	}
	if ( !qvk.CreateImage || !qvk.MapMemory ) {
		return;
	}
	s_waveTableTried = qtrue;

	for ( i = 0; i < FUNCTABLE_SIZE; i++ ) {
		lut[0 * FUNCTABLE_SIZE + i] = tr.sinTable[i];
		lut[1 * FUNCTABLE_SIZE + i] = tr.squareTable[i];
		lut[2 * FUNCTABLE_SIZE + i] = tr.triangleTable[i];
		lut[3 * FUNCTABLE_SIZE + i] = tr.sawToothTable[i];
		lut[4 * FUNCTABLE_SIZE + i] = tr.inverseSawToothTable[i];
	}

	if ( !VK_Texture_Create( FUNCTABLE_SIZE, 5, 1, VK_FORMAT_R32_SFLOAT, &handle ) ) {
		VK_Guard_Row( "VK:wave", "status=create-failed w=%d h=5 fmt=R32_SFLOAT bytes=%d",
					  (int)FUNCTABLE_SIZE, (int)sizeof( lut ) );
		return;
	}
	if ( !VK_Texture_Upload( handle, 0, FUNCTABLE_SIZE, 5, lut, sizeof( lut ) ) ) {
		VK_Texture_Destroy( handle );
		VK_Guard_Row( "VK:wave", "status=upload-failed handle=%d", handle );
		return;
	}
	if ( !VK_Texture_Publish( handle ) ) {
		VK_Texture_Destroy( handle );
		VK_Guard_Row( "VK:wave", "status=publish-failed handle=%d", handle );
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
	/* a destroyed table is a new attempt's business, not a remembered failure */
	s_waveTableTried = qfalse;
}

int VK_WaveTable( void )
{
	return s_waveTableHandle;
}


vkProgram_t *VK_FamilyProgram( unsigned family )  /* the draw's family, by name */
{
	return VK_GetProgram( family );
}

/* the program a stage needs: the shade tree's defines, the stage's own texenv and the caller's.  The texenv flags select vk_stage.frag's
   second-bundle arm; leaving them out compiled every two-bundle pass into MODULATE, so a merged ADD pass multiplied instead of adding. */
vkProgram_t *VK_StageProgram( const shaderStage_t *pStage, const shader_t *shader, unsigned extraDefines )
{
	unsigned d = VK_StageDefines( pStage, shader );

	return VK_GetProgram( d | VK_TexEnvForStage( pStage, shader ) | extraDefines );
}

/* ---- the world stage-set census ------------------------------------------ */

/* the loss this backend hunts is silent by construction -- a stage never submitted, or submitted with a foreign texture, still draws -- so the census records the
   EXPECTED set beside the SUBMITTED one and the difference is a row. */

/* WORLD_SET_CENSUS_BEGIN */

/* a bundle's image set is image[0] plus the animMap chain: only animMap advances numImageAnimations, so a single-image bundle carries 0, and a membership test
   bounded by it called the stage's own image foreign on every world and menu stage.  One name can also resolve to two image_t (picmip or wrap); the art is still the stage's own. */

/* Whether the stage expects anything on a unit at all: VK_DrawStage binds a
   unit only when its bundle holds an image, and VK_TexEnvForStage selects the
   second-sampler arm under the same condition.  A unit the stage never binds
   must not be judged -- s_shade.texImage[] still holds the previous stage's
   image, which is what the `unit=1 wanted=''` rows named. */

/* the census's sampling cadence, in frames: a batch that disagrees always emits, an agreeing one emits while the table fills and then
   once every VK_WORLDSET_SAMPLE_FRAMES frames.  The batch-counted cadence this replaced fired once per 600 batches and bounded nothing. */
#define VK_WORLDSET_SAMPLE_FRAMES	120
#define VK_WORLDSET_FIRST_ROWS		24

/* WORLD_SET_CENSUS_END */

/* The entity the batch belongs to, as far as it is knowable when the batch
   opens.  backEnd.currentEntity is updated by the draw-surf walk AFTER
   RB_BeginSurface opens the batch (tr_backend.c:1495 against :1517), so a
   batch that begins on an entity change carries the previous surface's
   entity: the class is recorded and the census is not gated on it. */

/* the geometry the arena arm actually submitted for this batch */
void RB_VboStaticLastRun( int *modeOut, int *numIdxOut );

/* One draw surf charged to the batch now open: an indexed draw can cover many
   same-shader surfs, so the draw count alone does not say whether the walk's
   surfaces reached the stream. */

/* the two fast iterators draw their one pass themselves instead of through VK_DrawStage, so the census must be told the pass was
   submitted; an iterator that calls both for one pass counts two, which is what the 296 `expected=1 drawn=2' rows named. */

/* The engine's own images a stage may legitimately bind in place of the one it
   resolved: the white substitutions RDF_FULLBRIGHT and r_lightmap ask for, a
   video map's scratch, and the lightmap arrays a BSP's shaders point into. */

/* One unit's bind, named if it is not the one the stage asked for.  A unit the
   stage does not bind is not witnessed: VK_DrawStage binds unit 1 only when
   bundle[1] holds an image, and VK_TexEnvForStage selects the second-sampler
   arm under the same condition, so a stale s_shade.texImage[1] left by an
   earlier bind is not a substitution and must not be named as one. */

/* the dlight term's bind witness: the one pass that binds outside VK_DrawStage, so the worldset census never names what it sampled.
   Retail rebinds its dlight texture before every dlight draw (ProjectDlightTexture @0x34F60), so what convicts is a latch holding
   non-dlight art, a slot another image_t took, or a view the slot no longer owns. */

/* The expected pass list: what the iterator in force would take, by its own
   rules.  The detail names each stage, so a census that disagrees names which
   stage it lost rather than only how many. */

/* The batch is closed: expected against drawn, plus the residency of every
   image either set names.  A disagreement always emits; an agreeing batch
   rides a bounded frame cadence. */

/* The colour stream the pack reads, bounded per channel; mark-gated.  The same
   row names the batch's own feed: srccolor is the base-lighting work buffer the
   group feed binds, dv is the file's own bytes, feed is the handler the batch is
   in -- an arena batch drew the arena's copy, not this array. */

/* A sprite walk batch is one draw for many sprites, so a gen-entity stage cannot
   read a per-sprite record; retail's walk installs tr.worldEntity there, which is
   the magenta.  The sprite's own rgba is already per-vertex in the tess colour
   array, so the vertex generators are the feed that gives each sprite its own
   colour without giving up the batch. */
static int VK_Shade_SpriteFeedRgb( int gen )
{
	if ( !r_spriteListOpen ) {
		return gen;
	}
	if ( gen == CGEN_ENTITY ) {
		return CGEN_VERTEX;
	}
	if ( gen == CGEN_ONE_MINUS_ENTITY ) {
		return CGEN_ONE_MINUS_VERTEX;
	}
	return gen;
}

static int VK_Shade_SpriteFeedAlpha( int gen )
{
	if ( !r_spriteListOpen ) {
		return gen;
	}
	if ( gen == AGEN_ENTITY ) {
		return AGEN_VERTEX;
	}
	if ( gen == AGEN_ONE_MINUS_ENTITY ) {
		return AGEN_ONE_MINUS_VERTEX;
	}
	return gen;
}

/* the entity colour a gen-entity stage is fed under the WORLD entity: one row per shader, budgeted, master gate only */

/* Which walk reached the world entity with a gen-entity stage: retail only ever
   draws such a stage there through the sprite list, so the route is the verdict.
   One row per shader, same caps as the row above, master gate only. */

/* the sprite walk's bind witness: worldset covers world batches and VK:stagebind samples on a cadence, so the sprite list was the one
   draw path that never named its art, which is the light cones flashing between two beamflare shafts.  A sprite's colour is per-vertex,
   so the row reports the whole batch's rgba range and fires on first sight, any bind anomaly, or a range move. */

/* ---- the square over a billboard: what the sprite's own draw carries -------- */

/* shader.c drops a blended sprite's fog pass (numFog/numGlobalFogPasses--), so what is left is the fixed-fog arm, and this pair decides it: the arm replaces rgb with
   the fog colour and keeps the stage's alpha, so ONE/ONE or DST_COLOR/ONE fills the quad's rectangle and SRC_ALPHA does not. */

/* The vulkan twin of the GLES3 decal-family selector.  Same four names, because
   both renderers walk the same shipped shader corpus. */

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

	/* The vulkan twin of the GLES3 [SMP:decal] row: the acff selector that was
	   just published (cpuColor collapses it to 0, which is what u_Acff holds),
	   the gens the shader asked for, and the fog volume the fog stage samples. */

	if ( r_spriteListOpen
		 && ( pStage->rgbGen == CGEN_ENTITY || pStage->rgbGen == CGEN_ONE_MINUS_ENTITY
			  || pStage->alphaGen == AGEN_ENTITY || pStage->alphaGen == AGEN_ONE_MINUS_ENTITY ) )
	{
		VK_SetUniform1i( prog, VK_U_COLORGEN,
						 VK_Shade_SpriteFeedRgb( ( int )pStage->rgbGen ) );
		VK_SetUniform1i( prog, VK_U_ALPHAGEN,
						 VK_Shade_SpriteFeedAlpha( ( int )pStage->alphaGen ) );
	}

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
	else if ( prog && prog->linked && ( prog->defines & VKP_LIGHTMAP ) )
	{
		/* retail leaves TMU1 bound but the stage's env for that unit is GL_DISABLE, so
		   the unit is never sampled; a descriptor has no disable, so the neutral bind
		   is the same effect -- otherwise the pass samples the last two-texture draw */
		VK_SelectTexture( 1 );
		VK_Bind( tr.whiteImage );
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
	/* a CPU-colour stage takes the svars array (in 2D every tess quad carries Draw_SetColor in vertexColors); the three vertex gens instead read a_Color raw, the
	   shader doing retail's own transform -- but only when the surface wrote the array: tess.vertexColorValid is raised solely by the handlers that fill it, and a static
	   model fills nothing, so carrying its array drew the previous surface's colour, which under the retail dst factors multiplies to black.  2D keeps the array unconditionally. */
	colors = cpuColor ? (const unsigned char *)input->svars.colors
					  : ( backEnd.in2D
						  || ( VK_StageReadsVertexColor( pStage ) && input->vertexColorValid )
						  || ( VK_StageReadsVertexAlpha( pStage ) && input->vertexColorValid ) )
						? (const unsigned char *)input->vertexColors
						: NULL;

	(void)stateBits;


	/* the draw-time witness: everything above names what the stage asked for, this
	   row names what the set the draw binds actually samples */

	RB_VBO_DrawTessArrays( input, input->indexes, input->numIndexes,
						   tc0, tc1, tcStride, colors, input->normal[0], pStage );
}

/* ---- the census seam the reference renderer carries ---------------------- */

/* the world path's answer to VK:2ddraw: one row per stage pass naming the shader, the stage and the image each bundle resolves to, so a
   world surface sampling a slot another texture took names itself the way a menu tile does. */

/* ---- what a model batch's own pixels actually came out as ---------------- */

/* The stage rows name the texture, the vertex colour and the blend; none of them
   names the framebuffer.  This projects each model batch's vertices with the very
   matrix the batch draws through and, on a frame whose presented image is in hand,
   reports the pixels inside that box -- the one witness that separates "drawn dark"
   from "not drawn at all", and the same reading for every actor in the frame so the
   comparison costs one boot. */
#define VK_SRFPIX_MAX 16

typedef struct {
	int			frame;
	int			ent;
	const char	*shader;
	float		box[4];			/* ndc x0,y0,x1,y1 */
	int			verts;
	int			behind;			/* vertices the batch projected behind the eye */
	unsigned	colour;			/* the first vertex colour the pack handed the stage */
} vkSrfPix_t;


/* one row per DISTINCT pass signature: shader, stage, stateBits, the blend factors the builder resolves, the colour array the pack got
   and whether the surface wrote one; first occurrence only, so a boot prints a bounded set.  Blend factors are the VkBlendFactor enums
   (0=ZERO 1=ONE 2=SRC_COLOR 3=ONE_MINUS_SRC_COLOR 4=DST_COLOR 5=ONE_MINUS_DST_COLOR 6=SRC_ALPHA 7=ONE_MINUS_SRC_ALPHA 8=DST_ALPHA
   9=ONE_MINUS_DST_ALPHA 10=SRC_ALPHA_SATURATE); col: 0=white(NULL) 1=svars 2=vertex array. */

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
/* The walk frame the copy belongs to, so a verdict can be checked against the
   frame it claims to describe rather than assumed to describe it. */
static int				 s_swapFrame = -1;

qboolean VK_ModelPixelAtSwap( int x, int y, byte out[4] )
{
	int ox, oy;

	if ( !out || !s_swapValid || x < 0 || y < 0 ) {
		return qfalse;
	}
	if ( x >= s_swapW || y >= s_swapH ) {
		return qfalse;
	}

	/* The copy preserves image row order and the framebuffer origin is the upper
	   left (vkspec.txt:196524-196527, :197120-197124), and the 2D ortho puts
	   client y = 0 on that same row, so the client coordinate indexes it as is. */
	ox = x;
	oy = y;

	out[0] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 0];
	out[1] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 1];
	out[2] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 2];
	out[3] = s_swapPixels[( oy * s_swapW + ox ) * 4 + 3];
	return qtrue;
}

/* The walk frame the copy belongs to, -1 when there is no copy: a readback
   verdict names a frame, so the row carries which frame it actually sampled. */
int VK_ModelSwapFrame( void )
{
	return s_swapValid ? s_swapFrame : -1;
}

/* the box's own pixels out of the presented copy: clear is the 3D clear colour
   (0,0,0,0), so a batch whose box is mostly clear never reached the screen */

/* ---- the presented frame's own map --------------------------------------- */

/* nine fixed samples cannot describe a shape -- they name nine pixels and the wedge between them is inferred.  This reads the same host
   copy the sampler reads and answers what the screen's coarse grid looks like and which single colour covers the biggest box. */

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

/* a rendered frame carries far more than VK_FLAT_BUCKETS distinct colours, so the table fills within the first few thousand samples and
   a probe run to capacity would cost sampled-pixels x 4096 (47 ms per 800x600 readback).  The probe is therefore bounded, and insert and
   lookup use the SAME bound, so every key that was inserted stays findable and a miss costs the bound. */
#define VK_FLAT_CAP	32

/* The census is an instrument, so its price must not scale with the resolution
   it is measuring: the scan is given a fixed sample budget and the step is
   derived from the frame. */
#define VK_PIX_BUDGET	32768


/* the frame's hue census, over the same host copy the flat boxes read: which primitive channel dominates each pixel and where those
   pixels are -- the radar cone is a green texel under rgbGen const white, so a red-clustered box names the same defect with no coordinate
   guessed in advance.  Position-independent on purpose. */
typedef struct {
	unsigned long long	n;
	int					minx, miny, maxx, maxy;
	unsigned long long	sum[3];
} vkHueRun_t;


/* One row per grid line: alpha in the high nibble of each cell and luminance in
   the low, so a translucent overlay and the scene behind it read off the same
   line.  Emitted only when the grid changes and only every few sampled frames,
   so a moving camera costs five lines a second and a still screen costs one. */

/* Called from the present seam.  Reads the presented image back once and
   samples a fixed grid, so one boot says whether the dome's stage draws are in
   the composited frame or only ever in a target the 2D pass then covers. */
/* The UI names each 3D widget rect it is about to render.  The nine fixed
   samples cannot answer the CONTROLS question -- the frame is dominated by the
   0.35,0.67,1.00 wash of the full-screen RenderModel, and whether the stardome
   ever reaches the framebuffer is exactly "what colour is inside the stardome's
   own rect at present".  Deduped, so a per-frame caller keeps a stable list. */

void VK_SetGhostImage( const char *name, image_t *image )
{
	(void)name; (void)image;
}

/* defined by the mirrored 3D front end */

/* ---- the 2D order transcript -------------------------------------------- */

/* a widget's depth is the position it draws at, so the ordered shader names are the z-order answer, which the rate-limited VK:2ddraw row
   cannot carry (past the fortieth draw the sampling throws the sequence away).  NewFrame emits the frame's names in order, deduped down the run. */
#define VK_2DORDER_MAX	64
void VK_Shade_NewFrame( void )
{
	int slot;

	/* the front end wrote this frame's uniforms; the first draw uploads them into a fresh ring */
	s_uboDirty = qtrue;

	slot = RB_FrameSmpFrame();
	if ( slot >= 0 && slot < VK_FRAMES_IN_FLIGHT ) {
		/* size this frame's ring from the last frame that filled it: one heavy
		   frame costs its own tail, and the next frame no longer refuses every
		   draw behind the first exhausted slice */
		if ( vk_shadeUboDemand[slot] * 4 >= vk_shadeUboSlices[slot] * 3 ) {
			VK_Shade_UboGrow( slot );
		}
		vk_shadeUboDemand[slot] = 0;
		vk_shadeUboNext[slot] = 0;
		vk_shadeUboLastValid[slot] = qfalse;
	}
}

/* ---- the pipeline warm pass ---------------------------------------------- */
/* A missing pipeline is built inside the draw that asks for it, on the walk
   thread.  This pass asks first, at registration, so the build lands in the
   load instead of the fight. */

#define VK_WARM_GRACE_FRAMES	60
#define VK_WARM_LATE_MIN_USEC	2000

static cvar_t *r_vkShadeWarm;
static cvar_t *r_vkShadeWarmBudget;

static int		s_warmCursor;
static int		s_warmArmFrame = -1;
static qboolean	s_warmInPass;
static int		s_warmBuilt;
static int		s_warmSkipped;
static int		s_warmNewPipelines;

/* A key the walk missed while a pass was recording.  Only the walk thread
   writes it and only the walk thread drains it, at the top of the next walk
   before the pass opens. */
#define VK_SHADE_DEFER_MAX	128
#define VK_SHADE_DEFER_NAME	48

typedef struct {
	unsigned	stateBits;
	int			family;
	unsigned	defines;
	cullType_t	cull;
	qboolean	polygonOffset;
	int			topology;
	char		shader[VK_SHADE_DEFER_NAME];
} vkShadeDefer_t;

static vkShadeDefer_t	s_shadeDefer[VK_SHADE_DEFER_MAX];
static int				s_shadeDeferCount;

static qboolean VK_Shade_WarmOn( void )
{
	return ( r_vkShadeWarm && r_vkShadeWarm->integer ) ? qtrue : qfalse;
}

static qboolean VK_Shade_DeferRegister( const char *shader, unsigned stateBits, int family,
										unsigned defines, cullType_t cull, qboolean polygonOffset,
										int topology )
{
	vkShadeDefer_t *d;
	int i;

	for ( i = 0; i < s_shadeDeferCount; i++ ) {
		d = &s_shadeDefer[i];
		if ( d->stateBits == stateBits && d->family == family && d->defines == defines
			 && d->cull == cull && d->polygonOffset == polygonOffset && d->topology == topology ) {
			return qtrue;
		}
	}

	if ( s_shadeDeferCount >= VK_SHADE_DEFER_MAX ) {
		return qfalse;
	}

	d = &s_shadeDefer[s_shadeDeferCount++];
	d->stateBits = stateBits;
	d->family = family;
	d->defines = defines;
	d->cull = cull;
	d->polygonOffset = polygonOffset;
	d->topology = topology;
	Q_strncpyz( d->shader, shader ? shader : "(null)", sizeof( d->shader ) );

	return qtrue;
}

/* One slice a frame: each entry is a driver build, so the budget is checked
   after it rather than before, and a slow build still finishes the frame it
   started in instead of being abandoned mid-call. */
static void VK_Shade_DeferDrain( int budgetMsec )
{
	long long t0;
	int i;

	if ( !s_shadeDeferCount ) {
		return;
	}

	t0 = BE_Microseconds64();
	for ( i = 0; i < s_shadeDeferCount; i++ ) {
		vkShadeDefer_t *d = &s_shadeDefer[i];

		VK_Shade_Pipeline( d->shader, d->stateBits, d->family, d->defines,
						   d->cull, d->polygonOffset, d->topology );

		if ( BE_Microseconds64() - t0 >= ( long long )budgetMsec * 1000LL ) {
			i++;
			break;
		}
	}

	memmove( &s_shadeDefer[0], &s_shadeDefer[i],
			 ( s_shadeDeferCount - i ) * sizeof( s_shadeDefer[0] ) );
	s_shadeDeferCount -= i;
}

static void VK_Shade_BuildNote( const char *shader, unsigned stateBits, int family,
								unsigned defines, long long usec )
{ s_warmNewPipelines++; }

static qboolean VK_Shade_WarmKey( const char *name, unsigned stateBits, unsigned defines,
								  cullType_t cull, qboolean polygonOffset, int topology )
{
	VkShaderModule v, f;

	if ( !VK_Program_Modules( VK_Family_Of( defines ), defines & ~VKP_FAMILY_MASK, &v, &f ) ) {
		/* no such permutation in the offline table; the walk would miss too */
		s_warmSkipped++;
		return qfalse;
	}
	if ( VK_Shade_Pipeline( name, stateBits, VK_Family_Of( defines ), defines,
							cull, polygonOffset, topology ) != VK_NULL_HANDLE ) {
		s_warmBuilt++;
		return qtrue;
	}
	return qfalse;
}

static void VK_Shade_WarmShader2D( const shader_t *shader );

/* the state word the walk substitutes when the entity fades, minus the
   backEnd read: RF_ForceAlphaStateBits @tr_backend.c:2508 */
static unsigned VK_Shade_WarmFadeBits( unsigned stateBits )
{
	return ( stateBits & ~( VKS_SRCBLEND_BITS | VKS_DSTBLEND_BITS | VKS_DEPTHMASK_TRUE ) )
		   | VKS_SRCBLEND_SRC_ALPHA | VKS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
}

/* the tess pipeline a 2D stage draws with: RB_StagePass @tr_backend.c:988 */
static void VK_Shade_WarmShader2D( const shader_t *shader )
{
	int s;

	for ( s = 0; s < shader->numUnfoggedPasses && s < MAX_SHADER_STAGES; s++ ) {
		const shaderStage_t	*pStage = shader->unfoggedStages[s];
		vkPipelineKey_t		key;

		if ( !pStage || !pStage->active || !pStage->bundle[0].image[0] ) {
			continue;
		}

		memset( &key, 0, sizeof( key ) );
		key.topology = VK_IMM_TRIANGLES;
		key.stateBits = pStage->stateBits;
		key.cull = CT_TWO_SIDED;
		key.depthTest = ( pStage->stateBits & VKS_DEPTHTEST_DISABLE ) ? qfalse : qtrue;
		key.depthWrite = ( pStage->stateBits & VKS_DEPTHMASK_TRUE ) ? qtrue : qfalse;
		VK_Shade_ShadowState( &key.stencilTest, &key.stencilFunc, &key.stencilRef,
							  &key.stencilMask, &key.stencilOpFail, &key.stencilOpZFail,
							  &key.stencilOpPass, &key.cullFace, &key.colorMask );
		VK_Draw_Pipeline( &key );
	}
}

/* one stage list's pipeline words: the draw path and the warm pass have to ask
   for the same key, so this is the unfogged loop's body for any list */
static void VK_Shade_WarmStageList( const shader_t *shader, shaderStage_t *const *stages, int count )
{
	int s;

	for ( s = 0; s < count && s < MAX_SHADER_STAGES; s++ ) {
		const shaderStage_t	*pStage = stages[s];
		unsigned			defines;

		if ( !pStage || !pStage->active ) {
			continue;
		}

		defines = VK_StageDefines_Full( pStage, shader, 0, 0, 0, 0 )
				  | VK_TexEnvForStage( pStage, shader );

		VK_Shade_WarmKey( shader->name, pStage->stateBits, defines, shader->cullType,
						  shader->polygonOffset, VK_IMM_TRIANGLES );
		VK_Shade_WarmKey( shader->name, VK_Shade_WarmFadeBits( pStage->stateBits ), defines,
						  shader->cullType, shader->polygonOffset, VK_IMM_TRIANGLES );
	}
}

static void VK_Shade_WarmShader( const shader_t *shader )
{
	int s;

	if ( !shader || shader->defaultShader ) {
		return;
	}

	/* 2D shaders draw through the tess pipeline, not this one */
	if ( shader->lightmapIndex == LIGHTMAP_2D ) {
		VK_Shade_WarmShader2D( shader );
		return;
	}

	for ( s = 0; s < shader->numUnfoggedPasses && s < MAX_SHADER_STAGES; s++ ) {
		const shaderStage_t	*pStage = shader->unfoggedStages[s];
		unsigned			defines;

		if ( !pStage || !pStage->active ) {
			continue;
		}

		defines = VK_StageDefines_Full( pStage, shader, 0, 0, 0, 0 )
				  | VK_TexEnvForStage( pStage, shader );

		if ( shader->optimalStageIteratorFunc == RB_StageIteratorLightmappedMultitextureUnfogged ) {
			/* single pass, single state word, its own permutation */
			if ( s == 0 ) {
				VK_Shade_WarmKey( shader->name, VKS_DEPTHMASK_TRUE,
								  defines | VKP_LIGHTMAP_MT, shader->cullType,
								  shader->polygonOffset, VK_IMM_TRIANGLES );
			}
			continue;
		}

		VK_Shade_WarmKey( shader->name, pStage->stateBits, defines, shader->cullType,
						  shader->polygonOffset, VK_IMM_TRIANGLES );
		VK_Shade_WarmKey( shader->name, VK_Shade_WarmFadeBits( pStage->stateBits ), defines,
						  shader->cullType, shader->polygonOffset, VK_IMM_TRIANGLES );
	}

	/* RB_StageIteratorGeneric swaps in these lists when the surface has a fog
	   number (tr_shade.c:236-240).  They were never warmed, so the first fogged
	   surface of a map -- the Drull 3 effect/fog entry -- parked its pipeline
	   mid-pass and the next walk's drain compiled it synchronously: the 523 ms
	   frame.  Warming the same keys at registration moves that build into the
	   load. */
	VK_Shade_WarmStageList( shader, shader->fogStages, shader->numFogPasses );
	VK_Shade_WarmStageList( shader, shader->globalFogStages, shader->numGlobalFogPasses );
}

void VK_Shade_WarmReset( void )
{
	s_warmCursor = 1;
	s_warmBuilt = 0;
	s_warmSkipped = 0;
	s_warmArmFrame = -1;
}

/* the detector arms at the first world scene after a registration, so the
   loading screen's own builds are exempt and a mid-combat build is not */
void VK_Shade_WarmArm( void )
{
	if ( s_warmArmFrame < 0 ) {
		s_warmArmFrame = RB_FrameCount();
	}
}

static qboolean VK_Shade_WarmRoom( void )
{
	return vk.device != VK_NULL_HANDLE
		   && vk_numShadePipelines < VK_SHADE_PIPELINES - 256;
}

/* Returns 1 when every registered shader has been walked. */
int VK_Shade_WarmRegistration( int budgetMsec )
{
	long long t0;

	if ( !r_vkShadeWarm ) {
		r_vkShadeWarm = ri.Cvar_Get( "r_vkShadeWarm", "1", 0 );
		r_vkShadeWarmBudget = ri.Cvar_Get( "r_vkShadeWarmBudget", "1500", 0 );
	}
	if ( !r_vkShadeWarm->integer || !VK_Shade_WarmRoom() ) {
		return 1;
	}
	if ( budgetMsec <= 0 ) {
		budgetMsec = r_vkShadeWarmBudget->integer;
	}

	t0 = BE_Microseconds64();
	s_warmInPass = qtrue;

	for ( ; s_warmCursor < tr.numShaders; s_warmCursor++ ) {
		if ( ( s_warmCursor & 31 ) == 0
			 && ( BE_Microseconds64() - t0 >= ( long long )budgetMsec * 1000LL
				  || !VK_Shade_WarmRoom() ) ) {
			s_warmInPass = qfalse;
			return 0;
		}
		VK_Shade_WarmShader( tr.shaders[s_warmCursor] );
	}

	s_warmInPass = qfalse;
	return 1;
}

/* Leftover of a budget that ran out, drained on the walk thread. */
void VK_Shade_WarmFrameStep( int budgetMsec )
{
	long long t0;

	/* the parked mid-pass keys come first, before the pass this walk is about to open */
	VK_Shade_DeferDrain( budgetMsec );

	if ( s_warmCursor >= tr.numShaders || !VK_Shade_WarmRoom() ) {
		return;
	}
	if ( !r_vkShadeWarm || !r_vkShadeWarm->integer ) {
		s_warmCursor = tr.numShaders;
		return;
	}

	t0 = BE_Microseconds64();
	s_warmInPass = qtrue;
	for ( ; s_warmCursor < tr.numShaders; s_warmCursor++ ) {
		if ( ( s_warmCursor & 7 ) == 0
			 && ( BE_Microseconds64() - t0 >= ( long long )budgetMsec * 1000LL
				  || !VK_Shade_WarmRoom() ) ) {
			break;
		}
		VK_Shade_WarmShader( tr.shaders[s_warmCursor] );
	}
	s_warmInPass = qfalse;
}

/* pipelines created since the last call, so a save only runs when the blob grew */
int VK_Shade_WarmTakeNewPipelines( void )
{
	int n = s_warmNewPipelines;

	s_warmNewPipelines = 0;
	return n;
}

/* the tess family's builds report through the same detector */
void VK_Shade_WarmNoteBuild( const char *shader, unsigned stateBits, long long usec )
{
	VK_Shade_BuildNote( shader, stateBits, -1, 0, usec );
}
