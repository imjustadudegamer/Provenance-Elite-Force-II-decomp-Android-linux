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
#include "gles3_g1_api.h"

#define PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_probe.h"

backEndData_t	*backEndData;			
backEndData_t	*backEndDataBuf[SMP_FRAMES];	
backEndState_t	backEnd;


static float	s_flipMatrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

/* ---- [GLES3:pix] what the framebuffer actually holds ----------------------
 * A submitted draw that produces no pixels is invisible from the command list:
 * the menu 3D view submits 13 draws a frame and the operator sees neither the
 * dome nor the ship.  Two readbacks name the screen -- one the moment the 3D
 * view hands back to 2D, one at swap.  Mean RGB tells the panel art from the
 * clear colour from a textured model. */
void GLES3_PixelProbe( const char *src, int x, int y, int w, int h ) {
	static cvar_t		*cvar;
	static struct {
		const char	*name;
		int			rows;
	}					 seen[6];
	static int			 seenN;
	byte				*buf;
	int				 i, n, r, g, b, a, opaque, slot;

	if ( !cvar ) {
		cvar = PROBE_GET( "pix_probe", "1", 0 );
	}
	if ( !cvar->integer || !PROBE_On() || w < 1 || h < 1 ) {
		return;
	}

	for ( slot = 0; slot < seenN; slot++ ) {
		if ( !strcmp( seen[slot].name, src ) ) {
			break;
		}
	}
	if ( slot == seenN && seenN < (int)( sizeof( seen ) / sizeof( seen[0] ) ) ) {
		seen[seenN].name = src;
		seen[seenN].rows = 0;
		seenN++;
	}
	if ( slot == seenN ) {
		return;
	}

	seen[slot].rows++;
	if ( seen[slot].rows > 4 && ( seen[slot].rows % 120 ) != 0 ) {
		return;
	}

	n = w * h;
	buf = ri.Hunk_AllocateTempMemory( n * 4 );
	qglReadPixels( x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf );
	r = g = b = a = opaque = 0;
	for ( i = 0; i < n; i++ ) {
		r += buf[i * 4 + 0];
		g += buf[i * 4 + 1];
		b += buf[i * 4 + 2];
		a += buf[i * 4 + 3];
		if ( buf[i * 4 + 3] == 255 ) {
			opaque++;
		}
	}
	ri.Hunk_FreeTempMemory( buf );

	ri.Printf( PRINT_ALL,
			   "[GLES3:pix] src=%s rect=%d,%d %dx%d mean=%d,%d,%d a=%d opaque=%d/%d f=%d\n",
			   src, x, y, w, h, r / n, g / n, b / n, a / n, opaque, n, RB_FrameCount() );
}

void GL_SetFogColor(const vec4_t fColor) {
	glState.fFogColor[0] = fColor[0];
	glState.fFogColor[1] = fColor[1];
	glState.fFogColor[2] = fColor[2];
	glState.fFogColor[3] = fColor[3];
}

/* ---- [GLES3:texaudit] what the device holds for a bind -------------------
 * An incomplete texture fetches black ("whole missing textures"), and an
 * immutable store an upload tried to re-specify keeps the old contents (the
 * smear).  Read off the bound object, once per image, then every 256th bind. */
#define TEXAUDIT_HASH	2048
#define TEXAUDIT_MAX	512
#define TEXAUDIT_RECHECK	256

typedef struct {
	unsigned	texnum;
	int			rows;
} texaudit_t;

static texaudit_t	*s_texaudit;
static int			*s_texauditHash;
static int			 s_texauditCount;
static qboolean		 s_texauditCapped;

static void GLES3_TexAudit( image_t *image ) {
	texaudit_t	*e;
	GLuint		errBefore;
	GLint		minF = -1, magF = -1, immut = -1;
	GLint		wrapS = -1, wrapT = -1, mipLevels = -1;
	int			recheck, verdict = 0;
	unsigned	hash;
	int			slot, idx;

	if ( !R_ProbeStatsOn() || !image || !image->texnum ) {
		return;
	}

	if ( !s_texaudit ) {
		s_texaudit     = (texaudit_t *)ri.Malloc( sizeof( texaudit_t ) * TEXAUDIT_MAX );
		s_texauditHash = (int *)ri.Malloc( sizeof( int ) * TEXAUDIT_HASH );
		Com_Memset( s_texauditHash, 0, sizeof( int ) * TEXAUDIT_HASH );
	}

	hash = (unsigned)( image->texnum * 2654435761u );
	slot = (int)( hash & ( TEXAUDIT_HASH - 1 ) );
	for ( ;; ) {
		idx = s_texauditHash[slot];
		if ( !idx ) {
			break;
		}
		idx--;
		if ( s_texaudit[idx].texnum == (unsigned)image->texnum ) {
			e = &s_texaudit[idx];
			goto found;
		}
		slot = ( slot + 1 ) & ( TEXAUDIT_HASH - 1 );
	}

	if ( s_texauditCount >= TEXAUDIT_MAX ) {
		if ( !s_texauditCapped ) {
			s_texauditCapped = qtrue;
			ri.Printf( PRINT_ALL,
					   "[GLES3:texaudit] capped at %d images; re-checks continue, new images are not recorded\n",
					   TEXAUDIT_MAX );
		}
		return;
	}
	idx = s_texauditCount++;
	s_texaudit[idx].texnum = image->texnum;
	s_texaudit[idx].rows   = 0;
	s_texauditHash[slot]   = idx + 1;
	e = &s_texaudit[idx];

found:
	e->rows++;
	recheck = ( e->rows > 4 ) && ( ( e->rows % TEXAUDIT_RECHECK ) != 0 );
	if ( recheck ) {
		return;
	}

	errBefore = qglGetError();
	qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minF );
	qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magF );
	qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrapS );
	qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrapT );
	qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &immut );
	qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_LEVELS, &mipLevels );
	if ( qglGetError() != GL_NO_ERROR || errBefore != GL_NO_ERROR ) {
		verdict |= 0x40;
	}

	/* v01 no size; 04 mip filter over one level, incomplete, fetches black;
	 * 08 the ES default filter never set; 10 recorded wrap against the
	 * object's; 20 immutable a re-upload would refuse. */
	if ( image->uploadWidth <= 0 || image->uploadHeight <= 0 ) {
		verdict |= 0x01;
	}
	if ( minF >= 0x2700 && minF <= 0x2703 && image->numMipmaps <= 0 ) {
		verdict |= 0x04;
	}
	if ( minF == 0x2601 ) {
		verdict |= 0x08;
	}
	if ( ( image->wrapClampModeX == GL_REPEAT ) != ( wrapS == GL_REPEAT )
		 || ( image->wrapClampModeY == GL_REPEAT ) != ( wrapT == GL_REPEAT ) ) {
		verdict |= 0x10;
	}
	if ( immut == 1 && image->dynamicallyUpdated ) {
		verdict |= 0x20;
	}

	if ( !verdict && e->rows > 1 ) {
		return;		/* clean, and already reported */
	}

	ri.Printf( PRINT_ALL,
			   "[GLES3:texaudit] %s tex=%u declared=%dx%d"
			   " mips=%d minF=0x%04x magF=0x%04x wrap=%d,%d immut=%d lvls=%d"
			   " dyn=%d flags=0x%x v=%02x n=%d f=%d\n",
			   image->imgName, (unsigned)image->texnum,
			   image->uploadWidth, image->uploadHeight,
			   image->numMipmaps, (unsigned)minF, (unsigned)magF,
			   wrapS, wrapT, immut, mipLevels,
			   image->dynamicallyUpdated ? 1 : 0, (unsigned)image->flags,
			   verdict, e->rows, RB_FrameCount() );
}

void GL_Bind( image_t *image ) {
	int texnum;

	if ( !image ) {
		ri.Error(ERR_DROP, "GL_Bind: NULL image\n");
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
		image->UseCount++;
	}

	if ( r_nobind->integer && tr.dlightImage ) {
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if (image) {
			image->frameUsed = RB_FrameCount();
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		GLES3_TexAudit( image );
		qglBindTexture (GL_TEXTURE_2D, texnum);

#define SMP_BIND_MAX   2048
#define SMP_BIND_HASH  8192			
		if ( R_ProbeStatsOn() ) {
			typedef struct {
				int				tmu;
				unsigned		texnum;
				const char		*name;
				unsigned		count;
			} smpBind_t;
			static smpBind_t	*s_smpBind;
			static int			*s_smpBindHash;
			static int			 s_smpBindCount;
			static qboolean		 s_smpBindNotice;
			unsigned			 h;
			int					 slot;
			int					 idx;

			if ( !s_smpBind ) {
				s_smpBind = (smpBind_t *)ri.Malloc( sizeof( smpBind_t ) * SMP_BIND_MAX );
				s_smpBindHash = (int *)ri.Malloc( sizeof( int ) * SMP_BIND_HASH );
				Com_Memset( s_smpBindHash, 0, sizeof( int ) * SMP_BIND_HASH );
			}

			h = (unsigned)( glState.currenttmu * 2654435761u ) ^ ( texnum * 2246822519u );
			slot = (int)( h & ( SMP_BIND_HASH - 1 ) );
			for ( ;; ) {
				idx = s_smpBindHash[slot];
				if ( !idx ) {
					break;
				}
				idx--;
				if ( s_smpBind[idx].tmu == glState.currenttmu
					 && s_smpBind[idx].texnum == (unsigned)texnum ) {
					s_smpBind[idx].count++;
					goto smp_bind_done;
				}
				slot = ( slot + 1 ) & ( SMP_BIND_HASH - 1 );
			}

			if ( s_smpBindCount >= SMP_BIND_MAX ) {
				if ( !s_smpBindNotice ) {
					s_smpBindNotice = qtrue;
					ri.Printf( PRINT_ALL,
						"[GLES3:bind] PAIR BUDGET reached (%d distinct (tmu,tex) pairs recorded); "
						"further distinct pairs are NOT recorded from here on\n", SMP_BIND_MAX );
				}
			} else {
				idx = s_smpBindCount++;
				s_smpBind[idx].tmu = glState.currenttmu;
				s_smpBind[idx].texnum = (unsigned)texnum;
				s_smpBind[idx].name = image ? image->imgName : "*null-image";
				s_smpBind[idx].count = 1;
				s_smpBindHash[slot] = idx + 1;
				ri.Printf( PRINT_ALL, "[GLES3:bind] tmu=%d tex=%d name=%s\n",
						   glState.currenttmu, texnum, s_smpBind[idx].name );
			}
smp_bind_done:;
		}
	}
}

void GL_BindUnit0( image_t *image ) {
	if ( glState.currenttmu != 0 ) {
		GLES3_SelectTexture( 0 );
	}
	GL_Bind( image );
}

void GL_SelectTexture( int unit )
{
	GLES3_SelectTexture( unit );
}


void GL_BindMultitexture( image_t *image0, GLuint env0, image_t *image1, GLuint env1 ) {
	int		texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if ( r_nobind->integer && tr.dlightImage ) {		
		texnum0 = texnum1 = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[1] != texnum1 ) {
		GL_SelectTexture( 1 );
		image1->frameUsed = RB_FrameCount();	
		glState.currenttextures[1] = texnum1;
		qglBindTexture( GL_TEXTURE_2D, texnum1 );
	}
	if ( glState.currenttextures[0] != texnum0 ) {
		GL_SelectTexture( 0 );
		image0->frameUsed = RB_FrameCount();	
		glState.currenttextures[0] = texnum0;
		qglBindTexture( GL_TEXTURE_2D, texnum0 );
	}
}


void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	} 
	else 
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}

void GL_TexEnv( int env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;

	switch ( env )
	{
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
	case GL_COMBINE4_NV:
	case GL_COMBINE:
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

unsigned long RF_ForceAlphaStateBits( unsigned long stateBits )
{
	if ( backEnd.currentEntity && ( backEnd.currentEntity->e.renderfx & RF_FORCE_ALPHA )
		&& backEnd.currentEntity->e.shaderRGBA[3] < 255 )
	{
		return ( stateBits & ~( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS | GLS_DEPTHMASK_TRUE ) )
		       | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}

	return stateBits;
}

static qboolean	s_depthTest = qtrue;
static qboolean	s_depthWrite = qtrue;
static qboolean	s_depth2D = qfalse;	/* the 2D window's state block ran last */

qboolean GLES3_DepthGate2D( void )
{
	return s_depth2D;
}

void GLES3_DepthGate2DTake( void )
{
	s_depth2D = qtrue;
}

void GLES3_DepthState( qboolean test, qboolean write )
{
	if ( test != s_depthTest )
	{
		if ( test ) {
			qglEnable( GL_DEPTH_TEST );
		} else {
			qglDisable( GL_DEPTH_TEST );
		}
		s_depthTest = test;
	}
	if ( write != s_depthWrite )
	{
		qglDepthMask( write ? GL_TRUE : GL_FALSE );
		s_depthWrite = write;
	}
	if ( test ) {
		s_depth2D = qfalse;	/* the gate is open again: 2D has to shut it */
	}
}

void GLES3_DepthStateReset( void )
{
	s_depthTest  = qtrue;
	s_depthWrite = qtrue;
	s_depth2D    = qfalse;
}

static qboolean	s_stenTest = qfalse;	/* the stencil gate, same discipline */
static GLint	s_stenFunc = GL_ALWAYS;
static GLint	s_stenRef  = 0;
static GLuint	s_stenMask = 255;

void GLES3_StencilState( qboolean test, GLint func, GLint ref, GLuint mask )
{
	if ( test != s_stenTest )
	{
		if ( test ) {
			qglEnable( GL_STENCIL_TEST );
		} else {
			qglDisable( GL_STENCIL_TEST );
		}
		s_stenTest = test;
	}
	if ( func != s_stenFunc || ref != s_stenRef || mask != s_stenMask )
	{
		qglStencilFunc( func, ref, mask );
		s_stenFunc = func;
		s_stenRef  = ref;
		s_stenMask = mask;
	}
}

void GLES3_StencilStateReset( void )
{
	GLES3_StencilState( qfalse, GL_ALWAYS, 0, 255 );
}

void GLES3_DepthFuncApply( GLenum func )
{
	qglDepthFunc( ( r_depthFuncAlways && r_depthFuncAlways->integer ) ? GL_ALWAYS : func );
}

void GL_State( unsigned long stateBits )
{
	unsigned long diff;

	diff = glState.glStateBits ^ (glState.externalSetState | stateBits);

	if ( !diff )
	{
		return;
	}
	if ( ( diff & GLS_CUBEMAP ) && glConfig.cubeMapAvailable )
	{
		if ( stateBits & GLS_CUBEMAP )
		{
			qglEnable( 0x8513 ); 
			qglEnable( 0x0C60 ); 
			qglEnable( 0x0C61 ); 
			qglEnable( 0x0C62 ); 
		}
		else
		{
			qglDisable( 0x8513 ); 
			qglDisable( 0x0C60 ); 
			qglDisable( 0x0C61 ); 
			qglDisable( 0x0C62 ); 
		}
	}

	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			GLES3_DepthFuncApply( GL_EQUAL );
		}
		else
		{
			GLES3_DepthFuncApply( GL_LEQUAL );
		}
	}

	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		GLES3_DepthState( ( stateBits & GLS_DEPTHTEST_DISABLE ) ? qfalse : qtrue,
						 ( stateBits & GLS_DEPTHMASK_TRUE ) ? qtrue : qfalse );
	}

	if ( diff & GLS_POLYMODE_LINE )
	{
	}

	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		GLES3_DepthState( ( stateBits & GLS_DEPTHTEST_DISABLE ) ? qfalse : qtrue,
						 ( stateBits & GLS_DEPTHMASK_TRUE ) ? qtrue : qfalse );
	}

	if ((diff & GLS_MULTITEXTURE_ENV) || (diff & GLS_MULTITEXTURE))
	{
		if (stateBits & GLS_MULTITEXTURE_ENV)
		{
			GL_TexEnv(GL_COMBINE4_NV);
		}
		else if (stateBits & GLS_MULTITEXTURE)
		{
			glState.cntTexEnvExt = GLS_MULTITEXTURE;
			GL_TexEnv(GL_COMBINE);
		}
		else
		{
			GL_TexEnv(GL_MODULATE);
		}
	}

	if ( diff & GLS_ATEST_BITS )
	{
		switch (stateBits & GLS_ATEST_BITS)
		{
		case 0:
			break;
		case GLS_ATEST_GT_0:
			break;
		case GLS_ATEST_LT_80:
			break;
		case GLS_ATEST_GE_80:
			break;
		default:
			break;
		}
	}

	if (diff & GLS_CLAMP_EDGE)
	{
		float clampValue;

		if (haveClampToEdge)
		{
			if (stateBits & GLS_CLAMP_EDGE) {
				clampValue = GL_CLAMP_TO_EDGE;
			}
			else {
				clampValue = GL_REPEAT;
			}
		}
		else
		{
			if (stateBits & GLS_CLAMP_EDGE) {
				clampValue = GL_CLAMP;
			}
			else {
				clampValue = GL_REPEAT;
			}
		}

		if (qglActiveTexture)
		{
			int currenttmu = glState.currenttmu;

			GL_SelectTexture(0);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampValue);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampValue);

			GL_SelectTexture(1);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampValue);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampValue);

			GL_SelectTexture(currenttmu);
		}
		else
		{
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampValue);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampValue);
		}
	}
	
	if (diff & GLS_FOG)
	{
		if (glState.externalSetState & GLS_FOG)
		{
			qglEnable(GL_FOG);
		}
		else
		{
			qglDisable(GL_FOG);
		}
	}

	glState.glStateBits = stateBits | glState.externalSetState;
}



static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}



#define SMP_VP_ROW_CAP		240
#define SMP_VP_TUPLE_SLOTS	192
#define SMP_VP_QUERY_BUDGET	8
#define SMP_VP_HEARTBEAT	300

typedef struct {
	const char	*src;
	int			ix, iy, iw, ih;		
	int			clearBits;			
	unsigned	visits;
	qboolean	queried;			
	qboolean	lastDisagree;		
	int			dx, dy, dw, dh;		
	int			dvx, dvy, dvw, dvh;	
	int			dtest;				
} smpVpTuple_t;

void SMP_VpProbe( const char *src, const char *why, int x, int y, int w, int h, int clearBits, qboolean setsViewport ) {
	static smpVpTuple_t	s_tuples[SMP_VP_TUPLE_SLOTS];
	static int				s_rows;
	static int				s_suppressed;
	static int				s_lastFrame = -1;
	static int				s_queriesThisFrame;
	static int				s_fullAnnounced;
	static cvar_t			*s_cvar;
	smpVpTuple_t			*t = NULL;
	int						i, slot = -1;
	int						sc[4] = { 0, 0, 0, 0 };
	int						vp[4] = { 0, 0, 0, 0 };
	int						dtest = 0;
	qboolean				heartbeat, needQuery, disagree;

	if ( !s_cvar ) {
		s_cvar = ri.Cvar_Get( "smp_vpProbe", "1", 0 );
	}
	if ( !s_cvar || !s_cvar->integer || !R_ProbeStatsOn() ) {
		return;
	}

	if ( RB_FrameCount() != s_lastFrame ) {
		s_lastFrame = RB_FrameCount();
		s_queriesThisFrame = 0;
	}
	heartbeat = ( s_lastFrame % SMP_VP_HEARTBEAT ) == 0;

	for ( i = 0; i < SMP_VP_TUPLE_SLOTS; i++ ) {
		if ( !s_tuples[i].src ) {
			continue;
		}
		if ( s_tuples[i].src == src && s_tuples[i].ix == x && s_tuples[i].iy == y &&
			 s_tuples[i].iw == w && s_tuples[i].ih == h && s_tuples[i].clearBits == clearBits ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		for ( i = 0; i < SMP_VP_TUPLE_SLOTS; i++ ) {
			if ( !s_tuples[i].src ) {
				slot = i;
				break;
			}
		}
	}
	if ( slot < 0 ) {
		if ( !s_fullAnnounced ) {
			s_fullAnnounced = 1;
			ri.Printf( PRINT_ALL, "[SMP:vp] tuple table full (%d keys); further rects untracked\n",
					   SMP_VP_TUPLE_SLOTS );
		}
		return;
	}
	t = &s_tuples[slot];
	if ( !t->src ) {
		t->src = src;
		t->ix = x; t->iy = y; t->iw = w; t->ih = h;
		t->clearBits = clearBits;
	}
	t->visits++;

	needQuery = ( !t->queried ) || heartbeat || t->lastDisagree;
	if ( !needQuery ) {
		return;
	}
	if ( !heartbeat && s_queriesThisFrame >= SMP_VP_QUERY_BUDGET ) {
		s_suppressed++;
		return;
	}

	if ( !qglGetIntegerv ) {
		return;
	}

	s_queriesThisFrame++;
	qglGetIntegerv( GL_SCISSOR_BOX, sc );
	qglGetIntegerv( GL_VIEWPORT, vp );
	dtest = qglIsEnabled ? ( qglIsEnabled( GL_SCISSOR_TEST ) ? 1 : 0 ) : -1;

	disagree = ( sc[0] != x || sc[1] != y || sc[2] != w || sc[3] != h || dtest != 1 );
	if ( setsViewport && ( vp[0] != x || vp[1] != y || vp[2] != w || vp[3] != h ) ) {
		disagree = qtrue;
	}

	if ( t->queried && !disagree && !heartbeat ) {
		t->lastDisagree = disagree;
		return;
	}
	t->queried = qtrue;
	t->lastDisagree = disagree;
	t->dx = sc[0]; t->dy = sc[1]; t->dw = sc[2]; t->dh = sc[3];
	t->dvx = vp[0]; t->dvy = vp[1]; t->dvw = vp[2]; t->dvh = vp[3];
	t->dtest = dtest;

	if ( s_rows >= SMP_VP_ROW_CAP ) {
		if ( s_rows == SMP_VP_ROW_CAP ) {
			s_rows++;
			ri.Printf( PRINT_ALL, "[SMP:vp] capped at %d rows (%d queries rate-limited); no further lines\n",
					   SMP_VP_ROW_CAP, s_suppressed );
		}
		return;
	}
	s_rows++;
	ri.Printf( PRINT_ALL,
		"[SMP:vp] src=%s why=%s f=%d int=%d,%d,%dx%d drvSc=%d,%d,%dx%d drvVp=%d,%d,%dx%d scTest=%d %s clear=0x%x"
		" refdef=%d,%d,%dx%d vid=%dx%d in2D=%d portal=%d owm=%d far=%.1f sky=%d prt=%d fastsky=%d\n",
		src, why, RB_FrameCount(), x, y, w, h,
		sc[0], sc[1], sc[2], sc[3], vp[0], vp[1], vp[2], vp[3], dtest,
		disagree ? "MISMATCH" : "match", clearBits,
		tr.refdef.x, tr.refdef.y, tr.refdef.width, tr.refdef.height,
		glConfig.vidWidth, glConfig.vidHeight, backEnd.in2D ? 1 : 0,
		backEnd.viewParms.isPortal ? 1 : 0,
		( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ? 1 : 0,
		backEnd.viewParms.farplane_distance,
		tr.skyRendered ? 1 : 0, tr.portalRendered ? 1 : 0,
		r_fastsky ? r_fastsky->integer : -1 );
}

static void SetViewportAndScissor( void ) {
	RB_LoadProjectionMatrix( backEnd.viewParms.projectionMatrix );

	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );

	SMP_VpProbe( "view", "SetViewportAndScissor/viewParms",
		backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, qtrue );
}

void RB_BeginDrawingView (void) {
	int clearBits = 0;

	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	/* The 2D pass ends here: its pending work draws now, under the 2D window and
	   its shut depth gate.  Left to the first RB_BeginSurface it landed inside
	   the view, after the depth clear, and the menu's widget painted over the ship. */
	RB_2D_Flush();

	backEnd.in2D = qfalse;

	/* A 2D rect cached here survives the 3D pass, and the next 2D call would
	   then skip re-issuing a scissor that pass overwrote: drop the pair. */
	R_Invalidate2DWindowCache();

	SetViewportAndScissor();


	GLES3_DepthState( qtrue, qtrue );	/* the 3D pass opens its gate, as retail's does */
	GLES3_StencilState( qfalse, GL_ALWAYS, 0, 255 );	/* retail's traced menu issues no stencil call */
	GL_State( GLS_DEFAULT );
	clearBits = GL_DEPTH_BUFFER_BIT;

	/* Retail's menu frame clears DEPTH only; the UI's 3D view builds no volume. */
	if ( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL )
		 && ( r_measureOverdraw->integer || r_shadows->integer == 2 ) )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}

	if (!( backEnd.refdef.rdflags & RDF_NOWORLDMODEL))
	{
		if (!backEnd.viewParms.farplane_distance || tr.skyRendered || tr.portalRendered)
		{
			if (r_fastsky->integer)
			{
				clearBits |= GL_COLOR_BUFFER_BIT;
				qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			}
		}
		else
		{
			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor(glState.fFogColor[0], glState.fFogColor[1], glState.fFogColor[2], 0.0f);
		}
	}
	qglClear( clearBits );

	GLES3_DepthFuncApply( GL_LEQUAL );	/* pinned per view for the A/B cvar */

	SMP_VpProbe( "clear", "RB_BeginDrawingView/qglClear",
		backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, clearBits, qtrue );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		

	backEnd.skyRenderedThisView = qfalse;

	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.ori.origin) - plane[3];

		RB_LoadModelViewMatrix(  s_flipMatrix );
		{
			vec4_t gles3Plane;
			gles3Plane[0] = (float)plane2[0];
			gles3Plane[1] = (float)plane2[1];
			gles3Plane[2] = (float)plane2[2];
			gles3Plane[3] = (float)plane2[3];
			RB_ClipPlane_Set( gles3Plane );
		}
	} else {
		RB_ClipPlane_Disable();
	}

	{
		static int s_gles3ViewLogged;
		if ( R_ProbeStatsOn() && s_gles3ViewLogged < 200 ) {
			vec4_t		cp;
			qboolean	clipOn = RB_ClipPlane_Get( cp );
			ri.Printf( PRINT_ALL, "[GLES3:view] portal=%d clip=%d in2D=%d fovX=%.1f\n",
				backEnd.viewParms.isPortal ? 1 : 0, clipOn ? 1 : 0,
				backEnd.in2D ? 1 : 0, backEnd.viewParms.fovX );
			s_gles3ViewLogged++;
		}
	}
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
	float			originalTime;

	originalTime = backEnd.refdef.floatTime;
	(void)originalTime;   // retail restores refdef.floatTime from this; that path is not ported

	RB_BeginDrawingView ();

	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldDepthRange = qfalse;
	oldFogNum = -1;
	oldDlightMap = 0;
	oldSort = -1;
	oldbStaticModel = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;
	backEnd.currentSphere = &backEnd.sphere;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == (unsigned)oldSort && *drawSurf->surface != SF_SPRITE ) {
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
		oldSort = (int)drawSurf->sort;
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

		if (shader != oldShader || fogNum != oldFogNum || dlightMap != oldDlightMap
			|| (shader->noMerge & 1)
			|| ( entityNum != oldEntityNum && !shader->entityMergable )
			|| ( bStaticModel != oldbStaticModel && !shader->entityMergable )) {
			if (oldShader != NULL) {
				BE_ZONE( BE_FAM_DS, BE_DSZ_end, RB_EndSurface() );
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
			oldFogNum = fogNum;
			oldDlightMap = dlightMap;
		}

		if ( entityNum != oldEntityNum || bStaticModel != oldbStaticModel ) {
			depthRange = qfalse;

			backEnd.sphere.TessFunction = NULL;

			if (bStaticModel) {
				backEnd.shaderStartTime = 0.0;
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

			RB_LoadModelViewMatrix(  backEnd.ori.modelMatrix );

			if ( oldDepthRange != depthRange ) {
				if ( depthRange ) {
					qglDepthRange (0, 0.3);
				} else {
					qglDepthRange (0, 1.0);
				}
				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
			oldbStaticModel = bStaticModel;
		}


		if (bStaticModel)
		{
			if (!r_fullbright->integer) {
				RB_Static_BuildDLights();
			}

			if (!backEnd.currentStaticModel->bStaticLightingCalculated) {
				RB_Static_SetupModel();
			}
		}
		else if (backEnd.currentEntity->e.tiki && shader->needsLSpherical)
		{
			R_Sphere_SetupEntity();
		}

		if (*drawSurf->surface == SF_SPRITE) {
			backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;
		}

		if (PROBE_On() && *drawSurf->surface == SF_POLY) {
			static int  s_iFrameCounter = 0;
			static int  s_iThisFrame = 0;
			static int  s_iLastFrame = 0;
			static int  s_iProbeRows = 0;	
			int         iFunc;
			unsigned    uiSt0 = ( shader->unfoggedStages[0] ) ? shader->unfoggedStages[0]->stateBits : 0u;
			GLboolean   bTest, bMask;
			GLfloat     fRange[2];
			int         iIsNewFrame;

			iIsNewFrame = (RB_FrameCount() != s_iLastFrame);	
			if (iIsNewFrame) {
				s_iLastFrame = RB_FrameCount();
				s_iThisFrame = 0;
			}
			s_iFrameCounter++;

			if (s_iProbeRows <= PROBE_ROW_CAP &&
			    s_iThisFrame < 4 && (s_iFrameCounter % 30) == 1) {
				s_iThisFrame++;
				s_iProbeRows++;

				qglGetBooleanv( GL_DEPTH_TEST, &bTest );
				qglGetIntegerv( GL_DEPTH_FUNC, &iFunc );
				qglGetBooleanv( GL_DEPTH_WRITEMASK, &bMask );
				qglGetFloatv( GL_DEPTH_RANGE, fRange );

				UIVK_PROBE(
					"DOCC:poly",
					"n=%d sh='%s' sort=%u ent=%d st0=0x%08x dtx=%d eq=%d poff=%d depthtest=%s func=%s mask=%d depthrange=%s(old=%s) drange=%.3f..%.3f proj15=%.0f\n",
					i,
					shader->name,
					drawSurf->sort,
					entityNum,
					uiSt0,
					( uiSt0 & GLS_DEPTHTEST_DISABLE ) ? 1 : 0,
					( uiSt0 & GLS_DEPTHFUNC_EQUAL ) ? 1 : 0,
					shader->polygonOffset ? 1 : 0,
					bTest ? "on" : "OFF",
					iFunc == GL_EQUAL ? "EQUAL" : (iFunc == GL_LEQUAL ? "LEQUAL" : (iFunc == GL_ALWAYS ? "ALWAYS" : "other")),
					bMask ? 1 : 0,
					depthRange ? "0..0.3" : "0..1",
					oldDepthRange ? "0..0.3" : "0..1",
					fRange[0], fRange[1],
					backEnd.viewParms.projectionMatrix[15]
				);
			}
		}

		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	if (oldShader != NULL) {
		BE_ZONE( BE_FAM_DS, BE_DSZ_end, RB_EndSurface() );
	}

	RB_LoadModelViewMatrix(  backEnd.viewParms.world.modelMatrix );
	if ( oldDepthRange ) {
		qglDepthRange (0, 1.0);
	}

	if (!(backEnd.refdef.rdflags & RDF_FULLBRIGHT)) {
		BE_ZONE( BE_FAM_DS, BE_DSZ_flare, R_DrawLensFlares() );
	}

	if (g_bInfoworldtris)
	{
		g_bInfoworldtris = 0;
		R_PrintInfoWorldtris();
	}

	BE_ZONE( BE_FAM_DS, BE_DSZ_shadow, RB_ShadowFinish() );

}

void RB_RenderSpriteSurfList(drawSurf_t* drawSurfs, int numDrawSurfs) {
	shader_t	*shader;
	shader_t	*oldShader;
	qboolean	depthRange;
	qboolean	oldDepthRange;
	int			i;
	int			shadowSprT0;
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
        refSprite_t *spr = (refSprite_t*)drawSurf->surface;
        if (spr->shaderNum < 0 || spr->shaderNum >= tr.numShaders) continue;
		BE_Bump( BE_FAM_BE, BE_BEC_sprN, 1 );
		shadowSprT0 = BE_On() ? BE_Microseconds() : 0;
		shader = tr.sortedShaders[spr->shaderNum];
		depthRange = (spr->renderfx & RF_DEPTHHACK) != 0;

        if ((shader != oldShader || (shader->noMerge & 1)) && !shader->entityMergable)
        {
			if (oldShader) {
				RB_EndSurface();
				BE_Bump( BE_FAM_BE, BE_BEC_sprBatch, 1 );
			}

            RB_BeginSurface(shader, spr->fogNum);
            oldShader = shader;
        }

        RB_LoadModelViewMatrix( backEnd.ori.modelMatrix);

        if (oldDepthRange != depthRange)
        {
			if (depthRange) {
				qglDepthRange(0.0, 0.3);
			} else {
                qglDepthRange(0.0, 1.0);
			}

            oldDepthRange = depthRange;
        }

        backEnd.shaderStartTime = ((refSprite_t*)drawSurf->surface)->shaderTime;

        rb_surfaceTable[*drawSurf->surface](drawSurf->surface);
		if ( BE_On() ) {
			BE_Zone( BE_FAM_BE, BE_BEZ_sprwalk, BE_Microseconds() - shadowSprT0 );
		}
	}

	if (oldShader) {
		RB_EndSurface();
		BE_Bump( BE_FAM_BE, BE_BEC_sprBatch, 1 );
	}

	r_spriteListOpen = 0;

    RB_LoadModelViewMatrix( backEnd.viewParms.world.modelMatrix);
	if (oldDepthRange) {
		qglDepthRange(0.0, 1.0);
	}
}



void	RB_SetGL2D (void) {
	RB_2D_SetWindow(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0.0, glConfig.vidWidth, glConfig.vidHeight, 0.0, 0.0, 1.0);
}


static void GLES3_DrainGLError( void ) {
	int i;
	for ( i = 0; i < 8; i++ ) {
		if ( qglGetError() == GL_NO_ERROR ) {
			return;
		}
	}
}

void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, int components, const byte* data, qboolean dirty) {
	int			i, j;
	int			start, end;

	if ( !tr.registered ) {
		return;
	}
	R_FlushRenderCommands();

	qglFinish();

	start = end = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	for ( i = 0 ; ( 1 << i ) < cols ; i++ ) {
	}
	for ( j = 0 ; ( 1 << j ) < rows ; j++ ) {
	}
	if ( ( 1 << i ) != cols || ( 1 << j ) != rows) {
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_BindUnit0( tr.scratchImage );

	if ( cols != tr.scratchImage->width || rows != tr.scratchImage->height ) {
		GLES3_DrainGLError();
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );

		{
			GLenum err = qglGetError();
			if ( err == GL_NO_ERROR ) {
				tr.scratchImage->width = tr.scratchImage->uploadWidth = cols;
				tr.scratchImage->height = tr.scratchImage->uploadHeight = rows;
			} else {
				static int s_gles3CineErrLogged;
				if ( s_gles3CineErrLogged < 10 ) {
					ri.Printf( PRINT_ALL, "[GLES3:glerr] cinematic glTexImage2D %dx%d rejected: 0x%04x\n",
							   cols, rows, err );
					s_gles3CineErrLogged++;
				}
			}
		}
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	} else if (dirty) {
		GLES3_DrainGLError();
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		{
			GLenum err = qglGetError();
			if ( err != GL_NO_ERROR ) {
				static int s_gles3CineSubErr;
				if ( s_gles3CineSubErr < 10 ) {
					ri.Printf( PRINT_ALL, "[GLES3:glerr] cinematic glTexSubImage2D %dx%d rejected: 0x%04x\n",
							   cols, rows, err );
					s_gles3CineSubErr++;
				}
			}
		}
	}

	{
		static int s_gles3CineLogged;
		if ( R_ProbeStatsOn() && s_gles3CineLogged < 200 ) {
			const byte *d = (const byte *)data;
			ri.Printf( PRINT_ALL,
				"[GLES3:cine] stretch x=%d y=%d w=%d h=%d cols=%d rows=%d comp=%d scratch=%u(%dx%d) dirty=%d px0=%02x%02x%02x%02x identityLight=%.3f\n",
				x, y, w, h, cols, rows, components,
				tr.scratchImage ? tr.scratchImage->texnum : 0u,
				tr.scratchImage ? tr.scratchImage->width : 0,
				tr.scratchImage ? tr.scratchImage->height : 0,
				dirty ? 1 : 0,
				d ? d[0] : 0, d ? d[1] : 0, d ? d[2] : 0, d ? d[3] : 0,
				tr.identityLight );
			s_gles3CineLogged++;
		}
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	if ( x > 0 || y > 0 || x + w < glConfig.vidWidth || y + h < glConfig.vidHeight ) {
		gles3Program_t	*p2d;

		GLES3_ApplyStateBits( GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA );

		GL_BindUnit0( tr.whiteImage );
		p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
		GLES3_SetBaseColor4f( p2d, 0.0f, 0.0f, 0.0f, 1.0f );
		RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		RB_ImmBegin( GL_TRIANGLES );
		RB_ImmVertex2f( 0, 0 );
		RB_ImmVertex2f( glConfig.vidWidth, 0 );
		RB_ImmVertex2f( glConfig.vidWidth, glConfig.vidHeight );
		RB_ImmVertex2f( 0, 0 );
		RB_ImmVertex2f( glConfig.vidWidth, glConfig.vidHeight );
		RB_ImmVertex2f( 0, glConfig.vidHeight );
		RB_ImmEnd();

		GL_BindUnit0( tr.scratchImage );
	}

	{
		gles3Program_t	*p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );

		GLES3_SetBaseColor4f( p2d, tr.identityLight, tr.identityLight, tr.identityLight, 1.0f );
		/* The frame quad is the colour the uniform states: the stream's current
		   colour is a leftover from whatever 2D ran before the movie. */
		RB_ImmColor4f( tr.identityLight, tr.identityLight, tr.identityLight, 1.0f );
		RB_ImmBegin( GL_TRIANGLES );
		RB_ImmTexCoord2f ( 0.5f / cols,  0.5f / rows );
		RB_ImmVertex2f (x, y);
		RB_ImmTexCoord2f ( ( cols - 0.5f ) / cols ,  0.5f / rows );
		RB_ImmVertex2f (x+w, y);
		RB_ImmTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
		RB_ImmVertex2f (x+w, y+h);
		RB_ImmTexCoord2f ( 0.5f / cols,  0.5f / rows );
		RB_ImmVertex2f (x, y);
		RB_ImmTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
		RB_ImmVertex2f (x+w, y+h);
		RB_ImmTexCoord2f ( 0.5f / cols, ( rows - 0.5f ) / rows );
		RB_ImmVertex2f (x, y+h);
		RB_ImmEnd();
	}
}

void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
	image_t	*scratch = tr.scratchImage;	
	(void)client;

	GL_BindUnit0( scratch );

	if ( cols != scratch->width || rows != scratch->height ) {
		GLES3_DrainGLError();
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		if ( qglGetError() == GL_NO_ERROR ) {
			scratch->width = scratch->uploadWidth = cols;
			scratch->height = scratch->uploadHeight = rows;
		}
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	} else {
		if (dirty) {
			GLES3_DrainGLError();
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
			{
				GLenum err = qglGetError();
				if ( err != GL_NO_ERROR ) {
					static int s_gles3UpCineSubErr;
					if ( s_gles3UpCineSubErr < 10 ) {
						ri.Printf( PRINT_ALL, "[GLES3:glerr] cinematic upload glTexSubImage2D %dx%d rejected: 0x%04x\n",
								   cols, rows, err );
						s_gles3UpCineSubErr++;
					}
				}
			}
		}
	}

	{
		static int s_gles3UpCineLogged;
		if ( R_ProbeStatsOn() && s_gles3UpCineLogged < 100 ) {
			const byte *d = (const byte *)data;
			ri.Printf( PRINT_ALL,
				"[GLES3:cine] upload w=%d h=%d cols=%d rows=%d client=%d scratch=%u(%dx%d) dirty=%d px0=%02x%02x%02x%02x\n",
				w, h, cols, rows, client,
				scratch->texnum, scratch->width, scratch->height,
				dirty ? 1 : 0, d ? d[0] : 0, d ? d[1] : 0, d ? d[2] : 0, d ? d[3] : 0 );
			s_gles3UpCineLogged++;
		}
	}
}

const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.in2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	*(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}

/* Each 2D dispatch arm runs the body its entry point runs. */
static const void *RB_2DColor( const void *data ) {
	const color2DCommand_t *cmd = (const color2DCommand_t *)data;

	RB_2D_SetColor( cmd->color );
	return (const void *)(cmd + 1);
}

static const void *RB_2DStretchPic( const void *data ) {
	const stretchPic2DCommand_t *cmd = (const stretchPic2DCommand_t *)data;

	RB_2D_StretchPic( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2 );
	return (const void *)(cmd + 1);
}

static const void *RB_2DStretchPic2( const void *data ) {
	const stretchPic2Command_t *cmd = (const stretchPic2Command_t *)data;

	RB_2D_StretchPic2( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2, cmd->sx, cmd->sy );
	return (const void *)(cmd + 1);
}

static const void *RB_2DTilePic( const void *data ) {
	const tilePic2DCommand_t *cmd = (const tilePic2DCommand_t *)data;

	RB_2D_TilePic( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h, cmd->picw, cmd->pich );
	return (const void *)(cmd + 1);
}

static const void *RB_2DTilePicOffset( const void *data ) {
	const tilePic2DCommand_t *cmd = (const tilePic2DCommand_t *)data;

	RB_2D_TilePicOffset( cmd->shader, cmd->x, cmd->y, cmd->w, cmd->h, cmd->picw, cmd->pich, cmd->offsetX, cmd->offsetY );
	return (const void *)(cmd + 1);
}

static const void *RB_2DTrianglePic( const void *data ) {
	const trianglePic2DCommand_t *cmd = (const trianglePic2DCommand_t *)data;

	RB_2D_TrianglePic( cmd->shader, (const vec2_t *)cmd->points, (const vec2_t *)cmd->texCoords );
	return (const void *)(cmd + 1);
}

static const void *RB_2DBox( const void *data ) {
	const box2DCommand_t *cmd = (const box2DCommand_t *)data;

	RB_2D_Box( cmd->x, cmd->y, cmd->w, cmd->h, cmd->blend ? qtrue : qfalse );
	return (const void *)(cmd + 1);
}

static const void *RB_2DLineLoop( const void *data ) {
	const lineLoop2DCommand_t *cmd = (const lineLoop2DCommand_t *)data;
	const vec2_t *points = (const vec2_t *)(cmd + 1);

	RB_2D_LineLoop( points, cmd->count, cmd->stipple_factor, cmd->stipple_mask );
	return (const void *)((const byte *)points + cmd->count * (int)sizeof( vec2_t ));
}

static const void *RB_2DSetWindow( const void *data ) {
	const setWindow2DCommand_t *cmd = (const setWindow2DCommand_t *)data;

	RB_2D_SetWindow( cmd->x, cmd->y, cmd->w, cmd->h, cmd->left, cmd->right, cmd->bottom, cmd->top, cmd->n, cmd->f );
	return (const void *)(cmd + 1);
}

static const void *RB_2DScissor( const void *data ) {
	const scissor2DCommand_t *cmd = (const scissor2DCommand_t *)data;

	RB_2D_Scissor( cmd->x, cmd->y, cmd->width, cmd->height );
	return (const void *)(cmd + 1);
}

static const void *RB_2DShaderTime( const void *data ) {
	const shaderTime2DCommand_t *cmd = (const shaderTime2DCommand_t *)data;

	RB_2D_ShaderTime( cmd->startTime );
	return (const void *)(cmd + 1);
}

/* One recorded page-and-colour run of a string. */
static const void *RB_2DFontString( const void *data ) {
	const fontString2DCommand_t	*cmd = (const fontString2DCommand_t *)data;
	const fontGlyph_t			*g = (const fontGlyph_t *)(cmd + 1);
	int							i;

	if ( cmd->numGlyphs > 0 && cmd->shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}

		RB_BeginSurface( cmd->shader, 0 );

		for ( i = 0; i < cmd->numGlyphs; i++, g++ ) {
			float	x = g->x, y = g->y;

			RB_CHECKOVERFLOW( 4, 6 );

			tess.texCoords[tess.numVertexes][0][0] = g->pos[0];
			tess.texCoords[tess.numVertexes][0][1] = g->pos[1];
			tess.texCoords[tess.numVertexes + 1][0][0] = g->size[0] + g->pos[0];
			tess.texCoords[tess.numVertexes + 1][0][1] = g->pos[1];
			tess.texCoords[tess.numVertexes + 2][0][0] = g->pos[0];
			tess.texCoords[tess.numVertexes + 2][0][1] = g->size[1] + g->pos[1];
			tess.texCoords[tess.numVertexes + 3][0][0] = g->size[0] + g->pos[0];
			tess.texCoords[tess.numVertexes + 3][0][1] = g->size[1] + g->pos[1];

			tess.xyz[tess.numVertexes][0] = x;
			tess.xyz[tess.numVertexes][1] = y;
			tess.xyz[tess.numVertexes][2] = cmd->z;
			tess.xyz[tess.numVertexes + 1][0] = x + cmd->glyphScale * g->size[0] * 256.0;
			tess.xyz[tess.numVertexes + 1][1] = y;
			tess.xyz[tess.numVertexes + 1][2] = cmd->z;
			tess.xyz[tess.numVertexes + 2][0] = x;
			tess.xyz[tess.numVertexes + 2][1] = y + cmd->charHeight;
			tess.xyz[tess.numVertexes + 2][2] = cmd->z;
			tess.xyz[tess.numVertexes + 3][0] = x + cmd->glyphScale * g->size[0] * 256.0;
			tess.xyz[tess.numVertexes + 3][1] = y + cmd->charHeight;
			tess.xyz[tess.numVertexes + 3][2] = cmd->z;

			tess.indexes[tess.numIndexes] = tess.numVertexes;
			tess.indexes[tess.numIndexes + 1] = tess.numVertexes + 1;
			tess.indexes[tess.numIndexes + 2] = tess.numVertexes + 2;
			tess.indexes[tess.numIndexes + 3] = tess.numVertexes + 1;
			tess.indexes[tess.numIndexes + 4] = tess.numVertexes + 3;
			tess.indexes[tess.numIndexes + 5] = tess.numVertexes + 2;

			if ( cmd->scaled ) {
				tess.xyz[tess.numVertexes][0] *= cmd->widthScale;
				tess.xyz[tess.numVertexes][1] *= cmd->heightScale;
				tess.xyz[tess.numVertexes + 1][0] *= cmd->widthScale;
				tess.xyz[tess.numVertexes + 1][1] *= cmd->heightScale;
				tess.xyz[tess.numVertexes + 2][0] *= cmd->widthScale;
				tess.xyz[tess.numVertexes + 2][1] *= cmd->heightScale;
				tess.xyz[tess.numVertexes + 3][0] *= cmd->widthScale;
				tess.xyz[tess.numVertexes + 3][1] *= cmd->heightScale;
			}

			tess.numVertexes += 4;
			tess.numIndexes += 6;
		}

		RB_EndSurface();
	}

	return (const void *)( (const byte *)data + PAD( (int)sizeof( *cmd )
						   + cmd->numGlyphs * (int)sizeof( fontGlyph_t ), sizeof( void * ) ) );
}

void RB_SetupFog(void) {
	if (!backEnd.viewParms.farplane_distance) {
		glState.externalSetState &= ~GLS_FOG;
	} else if (r_farplane_nofog->integer) {
		glState.externalSetState &= ~GLS_FOG;
	} else if (!backEnd.viewParms.farplane_fog) {
		glState.externalSetState &= ~GLS_FOG;
	} else {
		vec4_t vFogColor;

        vFogColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
        vFogColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
        vFogColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
        vFogColor[3] = 1.0;
        GL_SetFogColor(vFogColor);

		glState.externalSetState |= GLS_FOG;
	}
}

const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	BE_RootBegin( BE_FAM_DS );

	if ( tess.numIndexes ) {
		BE_ZONE( BE_FAM_DS, BE_DSZ_end, RB_EndSurface() );
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	BE_Bump( BE_FAM_BE, BE_BEC_surfs, cmd->numDrawSurfs );
	BE_Bump( BE_FAM_DS, BE_DSC_surfs, cmd->numDrawSurfs );

	RB_SetupFog();
	BE_ZONE( BE_FAM_DS, BE_DSZ_walk,
			   RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs ) );

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
	
	RB_SetupFog();
    RB_RenderSpriteSurfList(cmd->drawSurfs, cmd->numDrawSurfs);

    return (const void*)(cmd + 1);
}


const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	glState.finishCalled = qfalse;

	qglDrawBuffer( cmd->buffer );

	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

void RB_ShowImages( qboolean quiet ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if ( !backEnd.in2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();

	{
		gles3Program_t	*p2d;

		GL_SelectTexture( 0 );
		p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
		GLES3_SetBaseColor4f( p2d, 1.0f, 1.0f, 1.0f, 1.0f );
	}

	for ( i=0 ; i<tr.numImages ; i++ ) {
		image = &tr.images[i];

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_BindUnit0( image );
		RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		RB_ImmBegin (GL_TRIANGLES);
		RB_ImmTexCoord2f( 0, 0 );
		RB_ImmVertex2f( x, y );
		RB_ImmTexCoord2f( 1, 0 );
		RB_ImmVertex2f( x + w, y );
		RB_ImmTexCoord2f( 1, 1 );
		RB_ImmVertex2f( x + w, y + h );
		RB_ImmTexCoord2f( 0, 0 );
		RB_ImmVertex2f( x, y );
		RB_ImmTexCoord2f( 1, 1 );
		RB_ImmVertex2f( x + w, y + h );
		RB_ImmTexCoord2f( 0, 1 );
		RB_ImmVertex2f( x, y + h );
		RB_ImmEnd();
	}

	qglFinish();

	end = ri.Milliseconds();

    if (!quiet) {
        ri.Printf(PRINT_ALL, "%i msec to draw all images\n", end - start);
    }
}

const void *RB_ColorMask(const void *data)
{
	const colorMaskCommand_t *cmd = data;
	
	qglColorMask(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
	
	return (const void *)(cmd + 1);
}

const void *RB_ClearDepth(const void *data)
{
	const clearDepthCommand_t *cmd = data;
	
	if(tess.numIndexes)
		RB_EndSurface();

	if (r_showImages->integer)
		RB_ShowImages(qtrue);

	qglClear(GL_DEPTH_BUFFER_BIT);
	
	return (const void *)(cmd + 1);
}


const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	if ( r_showImages->integer ) {
		RB_ShowImages(qfalse);
	}

	cmd = (const swapBuffersCommand_t *)data;

	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}


	if ( !glState.finishCalled ) {
		qglFinish();
	}

	GLES3_PixelProbe( "swap", glConfig.vidWidth / 2 - 8, glConfig.vidHeight / 2 - 8, 16, 16 );
	GLES3_PixelProbe( "swap-q", glConfig.vidWidth / 4 - 8, glConfig.vidHeight / 4 - 8, 16, 16 );
	GLES3_ModelPixelAtSwap();
	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	GLimp_EndFrame();

	backEnd.in2D = qfalse;

	return (const void *)(cmd + 1);
}

#define BE_WINDOW_MS		1000
#define BE_MAX_ROOTS		3600
#define BE_MAX_ROWS		400
#define BE_MAX_ZONES		8
#define BE_MAX_COUNTERS	9
#define BE_MAX_SUBS		7
#define BE_TOP_N			3
#define BE_NAME_LEN		48

typedef struct {
	char	name[BE_NAME_LEN];
	int		usec;
} beTop_t;

typedef struct {
	int			sums[BE_MAX_ZONES];
	int			maxs[BE_MAX_ZONES];
	int			subSums[BE_MAX_SUBS];
	int			subMaxs[BE_MAX_SUBS];
	int			cntSum[BE_MAX_COUNTERS];
	int			cntMax[BE_MAX_COUNTERS];
	int			totalSum, totalMax;
	int			named;			
	int			roots;			
	int			rows;
	int			stopped;
	beTop_t	top[BE_TOP_N];
} beFam_t;

static beFam_t	s_be[BE_FAM_COUNT];
static int			s_beRootT0[BE_FAM_COUNT];
static int			s_beRootArmed[BE_FAM_COUNT];
static int			s_beWindowStart;
static qboolean		s_beWindowSet;

/* The tag each family prints under; the back-end families keep [BE:...]. */
static const char *const s_beFamTag[BE_FAM_COUNT] = { "BE:be", "BE:ds", "BE:prog", "FE:scene" };

static const char *const s_beZoneName[BE_FAM_COUNT][BE_MAX_ZONES] = {
	{ "surfs", "sprites", "sprwalk", "pic", "buf", "swap", "color", "other" },
	{ "walk", "end", "flare", "shadow", "other", NULL, NULL, NULL },
	{ "compile", "link", "other", NULL, NULL, NULL, NULL, NULL },
	{ "view", "world", "ents", "poly", "sprite", "sort", "exec", "other" }
};
static const int s_beZoneCount[BE_FAM_COUNT] = {
	BE_BE_ZONES, BE_DS_ZONES, BE_PROG_ZONES, BE_FE_ZONES
};

static const char *const s_beSubName[BE_MAX_SUBS] = {
	"substage", "subvbo", "subunif", "subshadow",
	"subpack", "subupload", "subdraw"
};

static const char *const s_beCntName[BE_FAM_COUNT][BE_MAX_COUNTERS] = {
	{ "lists", "cmds", "surfN", "drawN", "empty", "sprN", "sprBatch", "sprDraw" },
	{ "surfsN", "batchN", "vertN", "stageN", "drawN", "unifs", "unifskip", "binds", "edgeN" },
	{ "made", NULL, NULL, NULL, NULL, NULL, NULL, NULL },
	{ "surfs", "ents", "views", "portals", NULL, NULL, NULL, NULL }
};
static const int s_beCntCount[BE_FAM_COUNT] = {
	BE_BE_COUNTERS, BE_DS_COUNTERS, BE_PROG_COUNTERS, BE_FE_COUNTERS
};

typedef char be_tables_agree_enums[
	( BE_FAM_COUNT == 4 && BE_BE_ZONES == 8 && BE_DS_ZONES == 5
	  && BE_PROG_ZONES == 3 && BE_FE_ZONES == 8 && BE_BE_COUNTERS == 8
	  && BE_DS_COUNTERS == 9 && BE_PROG_COUNTERS == 1 && BE_FE_COUNTERS == 4
	  && BE_DS_SUBS == 7 ) ? 1 : -1 ];

void BE_Zone( int family, int zone, int usec );

static void BE_FamAdd( beFam_t *f, int zone, int usec ) {
	f->named += usec;
	f->sums[zone] += usec;
	if ( usec > f->maxs[zone] ) {
		f->maxs[zone] = usec;
	}
}

static void BE_Reset( int family ) {
	beFam_t	*f = &s_be[family];
	int			i;

	f->named = 0;
	f->roots = 0;
	f->totalSum = f->totalMax = 0;
	for ( i = 0; i < BE_MAX_ZONES; i++ ) {
		f->sums[i] = f->maxs[i] = 0;
	}
	for ( i = 0; i < BE_MAX_SUBS; i++ ) {
		f->subSums[i] = f->subMaxs[i] = 0;
	}
	for ( i = 0; i < BE_MAX_COUNTERS; i++ ) {
		f->cntSum[i] = f->cntMax[i] = 0;
	}
	for ( i = 0; i < BE_TOP_N; i++ ) {
		f->top[i].name[0] = '\0';
		f->top[i].usec = 0;
	}
}

static void BE_Print( int family, int windowMs ) {
	const beFam_t	*f = &s_be[family];
	char			line[1024], part[64];
	int				i, roots, mean10, max10;

	roots = ( f->roots < 1 ) ? 1 : f->roots;

	line[0] = '\0';

	for ( i = 0; i < s_beZoneCount[family]; i++ ) {
		mean10 = (int)( ( (long long)f->sums[i] + (long long)roots * 50 )
						/ ( (long long)roots * 100 ) );
		max10 = f->maxs[i] / 100;
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
					 s_beZoneName[family][i],
					 mean10 / 10, mean10 % 10, max10 / 10, max10 % 10 );
		Q_strcat( line, sizeof( line ), part );
	}

	for ( i = 0; i < s_beCntCount[family]; i++ ) {
		mean10 = (int)( ( (long long)f->cntSum[i] * 10 + roots / 2 ) / roots );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 s_beCntName[family][i],
					 mean10 / 10, mean10 % 10, f->cntMax[i] );
		Q_strcat( line, sizeof( line ), part );
	}

	if ( family == BE_FAM_DS ) {
		for ( i = 0; i < BE_DS_SUBS; i++ ) {
			mean10 = (int)( ( (long long)f->subSums[i] + (long long)roots * 50 )
							/ ( (long long)roots * 100 ) );
			max10 = f->subMaxs[i] / 100;
			Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
						 s_beSubName[i],
						 mean10 / 10, mean10 % 10, max10 / 10, max10 % 10 );
			Q_strcat( line, sizeof( line ), part );
		}
	}

	if ( family == BE_FAM_PROG ) {
		for ( i = 0; i < BE_TOP_N; i++ ) {
			if ( !f->top[i].name[0] ) {
				break;
			}
			Com_sprintf( part, sizeof( part ), "%s'%s'/%d.%d",
						 i ? "," : " top=",
						 f->top[i].name,
						 ( f->top[i].usec / 100 ) / 10, ( f->top[i].usec / 100 ) % 10 );
			Q_strcat( line, sizeof( line ), part );
		}
	}

	mean10 = (int)( ( (long long)f->totalSum + (long long)roots * 50 )
					/ ( (long long)roots * 100 ) );
	max10 = f->totalMax / 100;
	Com_sprintf( part, sizeof( part ), " total=%d.%d/%d.%d",
				 mean10 / 10, mean10 % 10, max10 / 10, max10 % 10 );
	Q_strcat( line, sizeof( line ), part );

	ri.Printf( PRINT_ALL, "[%s] roots=%d window=%dms%s\n",
			   s_beFamTag[family], f->roots, windowMs, line );
}

static void BE_MaybeRoll( void ) {
	int	now, window, i;
	int	windowSurfs;

	if ( !s_beWindowSet ) {
		s_beWindowStart = SMP_RLoadNow();
		s_beWindowSet = qtrue;
		return;
	}

	now = SMP_RLoadNow();
	window = now - s_beWindowStart;
	if ( window < BE_WINDOW_MS && s_be[BE_FAM_BE].roots < BE_MAX_ROOTS ) {
		return;
	}

	windowSurfs = s_be[BE_FAM_BE].cntSum[BE_BEC_surfs];	/* read before the reset */

	for ( i = 0; i < BE_FAM_COUNT; i++ ) {
		if ( s_be[i].roots < 1 || s_be[i].stopped ) {
			BE_Reset( i );
			continue;
		}
		if ( s_be[i].rows < BE_MAX_ROWS ) {
			BE_Print( i, window );
			s_be[i].rows++;
		} else {
			s_be[i].stopped = 1;
			ri.Printf( PRINT_ALL,
					   "[%s] capped at %d rows; no further lines\n",
					   s_beFamTag[i], BE_MAX_ROWS );
		}
		BE_Reset( i );
	}

	/* The send row shares this roll; a capture joins it by the window value. */
	if ( BE_On() ) {
		beSendRow_t send;

		if ( RB_VboSendTake( &send ) ) {
			send.surfs = windowSurfs;
			ri.Printf( PRINT_ALL,
					   "[BE:send] window=%dms batches=%d runs=%d sum=%dus first=%dus mid=%dus"
					   " max=%dus gapMax=%dus verts=%d big=%d bigVerts=%d arena=%d fall=%d"
					   " w=%d loc=%d st=%d md=%d sq=%d"
					   " d2=%d d2sum=%dus"
					   " surfs=%d gw=%d gd=%d gi=%d gs=%d gr=%d rc=%d\n",
					   window, send.batches, send.runs, send.sum, send.first, send.mid,
					   send.max, send.gapMax, send.verts, send.big, send.bigVerts,
					   RB_VboArenaMode(), send.fall,
					   send.writes, send.local, send.statics, send.multi, send.seq,
					   send.d2Batches, send.d2Sum,
					   send.surfs, send.gateWorld, send.gateDlight, send.gateIter,
					   send.gateSig, send.gateRec, send.recs );
			ri.Printf( PRINT_ALL,
					   "[BE:locIdx] visits=%d vtx=%d idx=%d hash=%d at=%d"
					   " surf=0x%x was=0x%x now=0x%x\n",
					   send.locVisits, send.locVtx, send.locIdx, send.locHash,
					   send.locFirstAt, send.locFirst, send.locWas, send.locNow );
		}
	}

	s_beWindowStart = now;
}

void BE_RootBegin( int family ) {
	if ( family < 0 || family >= BE_FAM_COUNT ) {
		return;
	}

	s_beRootArmed[family] = BE_Armed();
	s_beRootT0[family] = s_beRootArmed[family] ? BE_Microseconds() : 0;
	s_be[family].named = 0;
}

void BE_RootEnd( int family ) {
	int		total, other;

	if ( family < 0 || family >= BE_FAM_COUNT ) {
		return;
	}

	if ( !s_beRootArmed[family] ) {
		s_be[family].named = 0;
		return;
	}
	s_beRootArmed[family] = 0;

	total = BE_Microseconds() - s_beRootT0[family];
	if ( total < 0 ) {
		total = 0;
	}

	other = total - s_be[family].named;
	if ( other < 0 ) {
		other = 0;
	}
	BE_FamAdd( &s_be[family], s_beZoneCount[family] - 1, other );
	s_be[family].named = 0;

	s_be[family].totalSum += total;
	if ( total > s_be[family].totalMax ) {
		s_be[family].totalMax = total;
	}
	s_be[family].roots++;

	if ( family == BE_FAM_BE || !s_beRootArmed[BE_FAM_BE] ) {
		BE_MaybeRoll();
	}
}

void BE_Zone( int family, int zone, int usec ) {
	if ( family < 0 || family >= BE_FAM_COUNT || !s_beRootArmed[family] ) {
		return;
	}
	if ( zone < 0 || zone >= s_beZoneCount[family] ) {
		return;
	}
	if ( usec < 0 ) {
		usec = 0;
	}

	BE_FamAdd( &s_be[family], zone, usec );
}

void BE_Sub( int family, int sub, int usec ) {
	beFam_t	*f;

	if ( family != BE_FAM_DS || !s_beRootArmed[family] ) {
		return;
	}
	if ( sub < 0 || sub >= BE_DS_SUBS ) {
		return;
	}
	if ( usec < 0 ) {
		usec = 0;
	}

	f = &s_be[family];
	f->subSums[sub] += usec;
	if ( usec > f->subMaxs[sub] ) {
		f->subMaxs[sub] = usec;
	}
}

void BE_Bump( int family, int counter, int n ) {
	beFam_t	*f;

	if ( family < 0 || family >= BE_FAM_COUNT || !s_beRootArmed[family] ) {
		return;
	}
	if ( counter < 0 || counter >= s_beCntCount[family] ) {
		return;
	}

	f = &s_be[family];
	f->cntSum[counter] += n;
	if ( n > f->cntMax[counter] ) {
		f->cntMax[counter] = n;
	}
}

int BE_RootUsec( int family ) {
	if ( family < 0 || family >= BE_FAM_COUNT || !s_beRootArmed[family] ) {
		return 0;
	}
	return BE_Microseconds() - s_beRootT0[family];
}

void BE_Name( int family, const char *name, int usec ) {
	beFam_t	*f;
	int			i, slot;

	if ( family != BE_FAM_PROG || !s_beRootArmed[family] || !name ) {
		return;
	}

	f = &s_be[family];

	slot = -1;
	for ( i = 0; i < BE_TOP_N; i++ ) {
		if ( !f->top[i].name[0] || usec > f->top[i].usec ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		return;
	}
	for ( i = BE_TOP_N - 1; i > slot; i-- ) {
		f->top[i] = f->top[i - 1];
	}
	Q_strncpyz( f->top[slot].name, name, BE_NAME_LEN );
	f->top[slot].usec = usec;
}

/* ---- [GLES3:glerr] naming the call that left the error flag set ----------
 * The flag is sticky, so a probe that samples it reports the first call that
 * looked, not the one that set it -- the 2026-10-07 capture's [GLES3:pixd]
 * convicted a draw that ran clean.  One glGetError per list while idle; once a
 * list ends with the flag set, the next few pay one per command. */
enum {
	RB_GLES3_ERR_IDLE = 0,
	RB_GLES3_ERR_SEEN,
	RB_GLES3_ERR_TRACING
};

static int		s_glerrArm;
static int		s_glerrLists;
static int		s_glerrFound;
static int		s_glerrRows;

static const char *RB_GlErrName( GLenum e ) {
	switch ( e ) {
	case GL_INVALID_ENUM:		return "INVALID_ENUM";
	case GL_INVALID_VALUE:		return "INVALID_VALUE";
	case GL_INVALID_OPERATION:	return "INVALID_OPERATION";
	case GL_OUT_OF_MEMORY:		return "OUT_OF_MEMORY";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "INVALID_FRAMEBUFFER_OP";
	default:					return "unknown";
	}
}

static const char *RB_GlErrCmdName( int id ) {
	switch ( id ) {
	case RC_SET_COLOR:			return "SET_COLOR";
	case RC_STRETCH_PIC:		return "STRETCH_PIC";
	case RC_DRAW_SURFS:			return "DRAW_SURFS";
	case RC_SPRITE_SURFS:		return "SPRITE_SURFS";
	case RC_CLEARDEPTH:			return "CLEARDEPTH";
	case RC_2D_COLOR:			return "2D_COLOR";
	case RC_2D_STRETCH_PIC:		return "2D_STRETCH_PIC";
	case RC_2D_STRETCH_PIC2:	return "2D_STRETCH_PIC2";
	case RC_2D_TILE_PIC:		return "2D_TILE_PIC";
	case RC_2D_TILE_PIC_OFFSET:	return "2D_TILE_PIC_OFFSET";
	case RC_2D_TRIANGLE_PIC:	return "2D_TRIANGLE_PIC";
	case RC_2D_LINE_LOOP:		return "2D_LINE_LOOP";
	case RC_2D_SET_WINDOW:		return "2D_SET_WINDOW";
	case RC_2D_SCISSOR:			return "2D_SCISSOR";
	case RC_2D_FONT_STRING:		return "2D_FONT_STRING";
	case RC_2D_SHADER_TIME:		return "2D_SHADER_TIME";
	default:					return "END_OF_LIST";
	}
}

static void RB_GlErrSample( const char *where, int cmdId ) {	/* one read */

	GLuint		e;

	if ( !R_ProbeStatsOn() || s_glerrArm == RB_GLES3_ERR_IDLE ) {
		return;
	}

	e = qglGetError();
	if ( e == GL_NO_ERROR ) {
		return;
	}

	if ( s_glerrArm == RB_GLES3_ERR_TRACING && !s_glerrFound ) {
		s_glerrFound = 1;
		s_glerrRows++;
		ri.Printf( PRINT_ALL,
				   "[GLES3:glerr] at=%s cmd=0x%x err=%d(0x%x) %s in2D=%d"
				   " prog=%u tex0=%u tmu=%d lists=%d f=%d\n",
				   where, cmdId, (int)e, (unsigned)e, RB_GlErrName( e ),
				   backEnd.in2D, glState.currentProgram,
				   (unsigned)glState.currenttextures[0], glState.currenttmu,
				   s_glerrLists, RB_FrameCount() );
	}
}

/* The list-end read arms the per-command trace on a dirty list. */
#define RB_GLES3_ERR_TRACE_LISTS	4

static void RB_GlErrListEnd( int cmdId, int beCmds ) {
	GLuint	e;

	if ( !R_ProbeStatsOn() ) {
		return;
	}

	if ( s_glerrArm == RB_GLES3_ERR_TRACING ) {
		s_glerrLists++;
		if ( s_glerrLists > RB_GLES3_ERR_TRACE_LISTS ) {
			s_glerrArm = RB_GLES3_ERR_IDLE;
		}
		return;
	}

	e = qglGetError();
	if ( e == GL_NO_ERROR ) {
		return;
	}

	s_glerrArm    = RB_GLES3_ERR_TRACING;
	s_glerrLists  = 1;
	s_glerrFound  = 0;
	s_glerrRows++;
	ri.Printf( PRINT_ALL,
			   "[GLES3:glerr] seen err=%d(0x%x) %s cmds=%d in2D=%d prog=%u"
			   " tex0=%u tmu=%d f=%d\n",
			   (int)e, (unsigned)e, RB_GlErrName( e ), beCmds, backEnd.in2D,
			   glState.currentProgram, (unsigned)glState.currenttextures[0],
			   glState.currenttmu, RB_FrameCount() );
}

void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;
	int		beCmds = 0;

	{
		static int smp_smpframe_rows = 0;

		if ( R_ProbeStatsOn() && ( smp_smpframe_rows < 8 || ( smp_smpframe_rows % 600 ) == 0 ) ) {
			ri.Printf( PRINT_ALL,
					   "[SMP:smpframe] back=%d front=%d arena=%d frameCount=%d\n",
					   RB_FrameSmpFrame(), tr.smpFrame, RB_FrameBoneArena(), RB_FrameCount() );
		}
		smp_smpframe_rows++;
	}

	t1 = ri.Milliseconds ();

	BE_RootBegin( BE_FAM_BE );

	/* One frame's tessellation stream goes into one store, opened here. */
	RB_VBO_BeginFrame();

	while ( 1 ) {
		const int	cmdId = *(const int *)data;

		data = PADP(data, sizeof(void *));

		beCmds++;
		BE_Bump( BE_FAM_BE, BE_BEC_cmds, 1 );

		switch ( cmdId ) {
		case RC_SET_COLOR:
			BE_ZONE( BE_FAM_BE, BE_BEZ_color, data = RB_SetColor( data ) );
			break;
		case RC_STRETCH_PIC:
			BE_ZONE( BE_FAM_BE, BE_BEZ_pic, data = RB_StretchPic( data ) );
			break;
		case RC_DRAW_SURFS:
			BE_ZONE( BE_FAM_BE, BE_BEZ_surfs, data = RB_DrawSurfs( data ) );
			break;
		case RC_SPRITE_SURFS:
			BE_ZONE( BE_FAM_BE, BE_BEZ_sprites, data = RB_SpriteSurfs( data ) );
			break;
		case RC_DRAW_BUFFER:
			BE_ZONE( BE_FAM_BE, BE_BEZ_buf, data = RB_DrawBuffer( data ) );
			break;
		case RC_SWAP_BUFFERS:
			BE_ZONE( BE_FAM_BE, BE_BEZ_swap, data = RB_SwapBuffers( data ) );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;
		case RC_COLORMASK:
			BE_ZONE( BE_FAM_BE, BE_BEZ_color, data = RB_ColorMask(data) );
			break;
		case RC_CLEARDEPTH:
			BE_ZONE( BE_FAM_BE, BE_BEZ_color, data = RB_ClearDepth(data) );
			break;
		case RC_2D_COLOR:
			BE_ZONE( BE_FAM_BE, BE_BEZ_color, data = RB_2DColor(data) );
			break;
		case RC_2D_STRETCH_PIC:
			BE_ZONE( BE_FAM_BE, BE_BEZ_pic, data = RB_2DStretchPic(data) );
			break;
		case RC_2D_STRETCH_PIC2:
			BE_ZONE( BE_FAM_BE, BE_BEZ_pic, data = RB_2DStretchPic2(data) );
			break;
		case RC_2D_TILE_PIC:
			BE_ZONE( BE_FAM_BE, BE_BEZ_pic, data = RB_2DTilePic(data) );
			break;
		case RC_2D_TILE_PIC_OFFSET:
			BE_ZONE( BE_FAM_BE, BE_BEZ_pic, data = RB_2DTilePicOffset(data) );
			break;
		case RC_2D_TRIANGLE_PIC:
			BE_ZONE( BE_FAM_BE, BE_BEZ_pic, data = RB_2DTrianglePic(data) );
			break;
		case RC_2D_BOX:
			BE_ZONE( BE_FAM_BE, BE_BEZ_color, data = RB_2DBox(data) );
			break;
		case RC_2D_LINE_LOOP:
			BE_ZONE( BE_FAM_BE, BE_BEZ_other, data = RB_2DLineLoop(data) );
			break;
		case RC_2D_SET_WINDOW:
			BE_ZONE( BE_FAM_BE, BE_BEZ_other, data = RB_2DSetWindow(data) );
			break;
		case RC_2D_SCISSOR:
			BE_ZONE( BE_FAM_BE, BE_BEZ_other, data = RB_2DScissor(data) );
			break;
		case RC_2D_FONT_STRING:
			BE_ZONE( BE_FAM_BE, BE_BEZ_other, data = RB_2DFontString(data) );
			break;
		case RC_2D_SHADER_TIME:
			BE_ZONE( BE_FAM_BE, BE_BEZ_other, data = RB_2DShaderTime(data) );
			break;
		case RC_END_OF_LIST:
		default:
			t2 = ri.Milliseconds ();
			backEnd.pc.msec = t2 - t1;
			BE_Bump( BE_FAM_BE, BE_BEC_lists, 1 );
			if ( beCmds <= 1 ) {
				BE_Bump( BE_FAM_BE, BE_BEC_empty, 1 );
			}
			BE_RootEnd( BE_FAM_BE );
			RB_GlErrListEnd( cmdId, beCmds );
			return;
		}
		RB_GlErrSample( RB_GlErrCmdName( cmdId ), cmdId );
	}

}
