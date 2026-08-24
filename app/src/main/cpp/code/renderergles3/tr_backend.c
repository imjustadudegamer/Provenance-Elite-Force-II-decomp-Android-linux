#include "tr_local.h"
#include "gles3_g1_api.h"

#define R120_PROBE_GET(name, def, flags) ri.Cvar_Get(name, def, flags)
#define R120_PROBE_PRINT(...)            ri.Printf(PRINT_ALL, __VA_ARGS__)
#include "../qcommon/qa_r120.h"

backEndData_t	*backEndData;			
backEndData_t	*backEndDataBuf[SMP_FRAMES];	
backEndState_t	backEnd;


static float	s_flipMatrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

void GL_SetFogColor(const vec4_t fColor) {
	glState.fFogColor[0] = fColor[0];
	glState.fFogColor[1] = fColor[1];
	glState.fFogColor[2] = fColor[2];
	glState.fFogColor[3] = fColor[3];

	// tr.viewParms.farplane_color at 0x1CF7C-0x1CF9C).  It never switches the
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
		qglBindTexture (GL_TEXTURE_2D, texnum);

#define R201_BIND_MAX   2048
#define R201_BIND_HASH  8192			
		{
			typedef struct {
				int				tmu;
				unsigned		texnum;
				const char		*name;
				unsigned		count;
			} r201Bind_t;
			static r201Bind_t	*s_r201Bind;
			static int			*s_r201BindHash;
			static int			 s_r201BindCount;
			static qboolean		 s_r201BindNotice;
			unsigned			 h;
			int					 slot;
			int					 idx;

			if ( !s_r201Bind ) {
				s_r201Bind = (r201Bind_t *)ri.Malloc( sizeof( r201Bind_t ) * R201_BIND_MAX );
				s_r201BindHash = (int *)ri.Malloc( sizeof( int ) * R201_BIND_HASH );
				Com_Memset( s_r201BindHash, 0, sizeof( int ) * R201_BIND_HASH );
			}

			h = (unsigned)( glState.currenttmu * 2654435761u ) ^ ( texnum * 2246822519u );
			slot = (int)( h & ( R201_BIND_HASH - 1 ) );
			for ( ;; ) {
				idx = s_r201BindHash[slot];
				if ( !idx ) {
					break;
				}
				idx--;
				if ( s_r201Bind[idx].tmu == glState.currenttmu
					 && s_r201Bind[idx].texnum == (unsigned)texnum ) {
					s_r201Bind[idx].count++;
					goto r201_bind_done;
				}
				slot = ( slot + 1 ) & ( R201_BIND_HASH - 1 );
			}

			if ( s_r201BindCount >= R201_BIND_MAX ) {
				if ( !s_r201BindNotice ) {
					s_r201BindNotice = qtrue;
					ri.Printf( PRINT_ALL,
						"[R200:bind] PAIR BUDGET reached (%d distinct (tmu,tex) pairs recorded); "
						"further distinct pairs are NOT recorded from here on\n", R201_BIND_MAX );
				}
			} else {
				idx = s_r201BindCount++;
				s_r201Bind[idx].tmu = glState.currenttmu;
				s_r201Bind[idx].texnum = (unsigned)texnum;
				s_r201Bind[idx].name = image ? image->imgName : "*null-image";
				s_r201Bind[idx].count = 1;
				s_r201BindHash[slot] = idx + 1;
				ri.Printf( PRINT_ALL, "[R200:bind] tmu=%d tex=%d name=%s\n",
						   glState.currenttmu, texnum, s_r201Bind[idx].name );
			}
r201_bind_done:;
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
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
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
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	if ( diff & GLS_POLYMODE_LINE )
	{
	}

	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
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
			// NOTHING -- it does not even disable the alpha test.  assert(0)
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



#define R201_VP_ROW_CAP		240
#define R201_VP_TUPLE_SLOTS	192
#define R201_VP_QUERY_BUDGET	8
#define R201_VP_HEARTBEAT	300

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
} r201VpTuple_t;

void R201_VpProbe( const char *src, const char *why, int x, int y, int w, int h, int clearBits, qboolean setsViewport ) {
	static r201VpTuple_t	s_tuples[R201_VP_TUPLE_SLOTS];
	static int				s_rows;
	static int				s_suppressed;
	static int				s_lastFrame = -1;
	static int				s_queriesThisFrame;
	static int				s_fullAnnounced;
	static cvar_t			*s_cvar;
	r201VpTuple_t			*t = NULL;
	int						i, slot = -1;
	int						sc[4] = { 0, 0, 0, 0 };
	int						vp[4] = { 0, 0, 0, 0 };
	int						dtest = 0;
	qboolean				heartbeat, needQuery, disagree;

	if ( !s_cvar ) {
		s_cvar = ri.Cvar_Get( "r201_vpProbe", "1", 0 );
	}
	if ( !s_cvar || !s_cvar->integer ) {
		return;
	}

	if ( RB_FrameCount() != s_lastFrame ) {
		s_lastFrame = RB_FrameCount();
		s_queriesThisFrame = 0;
	}
	heartbeat = ( s_lastFrame % R201_VP_HEARTBEAT ) == 0;

	for ( i = 0; i < R201_VP_TUPLE_SLOTS; i++ ) {
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
		for ( i = 0; i < R201_VP_TUPLE_SLOTS; i++ ) {
			if ( !s_tuples[i].src ) {
				slot = i;
				break;
			}
		}
	}
	if ( slot < 0 ) {
		if ( !s_fullAnnounced ) {
			s_fullAnnounced = 1;
			ri.Printf( PRINT_ALL, "[R201:vp] tuple table full (%d keys); further rects untracked\n",
					   R201_VP_TUPLE_SLOTS );
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
	if ( !heartbeat && s_queriesThisFrame >= R201_VP_QUERY_BUDGET ) {
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

	if ( s_rows >= R201_VP_ROW_CAP ) {
		if ( s_rows == R201_VP_ROW_CAP ) {
			s_rows++;
			ri.Printf( PRINT_ALL, "[R201:vp] capped at %d rows (%d queries rate-limited); no further lines\n",
					   R201_VP_ROW_CAP, s_suppressed );
		}
		return;
	}
	s_rows++;
	ri.Printf( PRINT_ALL,
		"[R201:vp] src=%s why=%s f=%d int=%d,%d,%dx%d drvSc=%d,%d,%dx%d drvVp=%d,%d,%dx%d scTest=%d %s clear=0x%x"
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

	R201_VpProbe( "view", "SetViewportAndScissor/viewParms",
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

	backEnd.in2D = qfalse;

	SetViewportAndScissor();


	GL_State( GLS_DEFAULT );
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_measureOverdraw->integer || r_shadows->integer == 2 )
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

	R201_VpProbe( "clear", "RB_BeginDrawingView/qglClear",
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
		static int s_r200ViewLogged;
		if ( s_r200ViewLogged < 200 ) {
			vec4_t		cp;
			qboolean	clipOn = RB_ClipPlane_Get( cp );
			ri.Printf( PRINT_ALL, "[R200:view] portal=%d clip=%d in2D=%d fovX=%.1f\n",
				backEnd.viewParms.isPortal ? 1 : 0, clipOn ? 1 : 0,
				backEnd.in2D ? 1 : 0, backEnd.viewParms.fovX );
			s_r200ViewLogged++;
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
				R204_ZONE( R204_FAM_DS, R204_DSZ_end, RB_EndSurface() );
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

		if (R120_On() && *drawSurf->surface == SF_POLY) {
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

			if (s_iProbeRows <= R1XX_PROBE_ROW_CAP &&
			    s_iThisFrame < 4 && (s_iFrameCounter % 30) == 1) {
				s_iThisFrame++;
				s_iProbeRows++;

				qglGetBooleanv( GL_DEPTH_TEST, &bTest );
				qglGetIntegerv( GL_DEPTH_FUNC, &iFunc );
				qglGetBooleanv( GL_DEPTH_WRITEMASK, &bMask );
				qglGetFloatv( GL_DEPTH_RANGE, fRange );

				R133_PROBE(
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
		R204_ZONE( R204_FAM_DS, R204_DSZ_end, RB_EndSurface() );
	}

	RB_LoadModelViewMatrix(  backEnd.viewParms.world.modelMatrix );
	if ( oldDepthRange ) {
		qglDepthRange (0, 1.0);
	}

	if (!(backEnd.refdef.rdflags & RDF_HUD)) {
		R204_ZONE( R204_FAM_DS, R204_DSZ_flare, R_DrawLensFlares() );
	}

	if (g_bInfoworldtris)
	{
		g_bInfoworldtris = 0;
		R_PrintInfoWorldtris();
	}

	R204_ZONE( R204_FAM_DS, R204_DSZ_shadow, RB_ShadowFinish() );

}

void RB_RenderSpriteSurfList(drawSurf_t* drawSurfs, int numDrawSurfs) {
	shader_t	*shader;
	shader_t	*oldShader;
	qboolean	depthRange;
	qboolean	oldDepthRange;
	int			i;
	int			r208SprT0;
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
		R204_Bump( R204_FAM_BE, R204_BEC_sprN, 1 );
		r208SprT0 = R204_On() ? R204_Microseconds() : 0;
		shader = tr.sortedShaders[spr->shaderNum];
		depthRange = (spr->renderfx & RF_DEPTHHACK) != 0;

        if ((shader != oldShader || (shader->noMerge & 1)) && !shader->entityMergable)
        {
			if (oldShader) {
				RB_EndSurface();
				R204_Bump( R204_FAM_BE, R204_BEC_sprBatch, 1 );
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
		if ( R204_On() ) {
			R204_Zone( R204_FAM_BE, R204_BEZ_sprwalk, R204_Microseconds() - r208SprT0 );
		}
	}

	if (oldShader) {
		RB_EndSurface();
		R204_Bump( R204_FAM_BE, R204_BEC_sprBatch, 1 );
	}

	r_spriteListOpen = 0;

    RB_LoadModelViewMatrix( backEnd.viewParms.world.modelMatrix);
	if (oldDepthRange) {
		qglDepthRange(0.0, 1.0);
	}
}



void	RB_SetGL2D (void) {
	Set2DWindow(0, 0, glConfig.vidWidth, glConfig.vidHeight, 0.0, glConfig.vidWidth, glConfig.vidHeight, 0.0, 0.0, 1.0);
}


static void R200_DrainGLError( void ) {
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
	R_IssuePendingRenderCommands();

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
		R200_DrainGLError();
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );

		{
			GLenum err = qglGetError();
			if ( err == GL_NO_ERROR ) {
				tr.scratchImage->width = tr.scratchImage->uploadWidth = cols;
				tr.scratchImage->height = tr.scratchImage->uploadHeight = rows;
			} else {
				static int s_r200CineErrLogged;
				if ( s_r200CineErrLogged < 10 ) {
					ri.Printf( PRINT_ALL, "[R200:glerr] cinematic glTexImage2D %dx%d rejected: 0x%04x\n",
							   cols, rows, err );
					s_r200CineErrLogged++;
				}
			}
		}
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	} else if (dirty) {
		R200_DrainGLError();
		qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);
		{
			GLenum err = qglGetError();
			if ( err != GL_NO_ERROR ) {
				static int s_r200CineSubErr;
				if ( s_r200CineSubErr < 10 ) {
					ri.Printf( PRINT_ALL, "[R200:glerr] cinematic glTexSubImage2D %dx%d rejected: 0x%04x\n",
							   cols, rows, err );
					s_r200CineSubErr++;
				}
			}
		}
	}

	{
		static int s_r200CineLogged;
		if ( s_r200CineLogged < 200 ) {
			const byte *d = (const byte *)data;
			ri.Printf( PRINT_ALL,
				"[R200:cine] stretch x=%d y=%d w=%d h=%d cols=%d rows=%d comp=%d scratch=%u(%dx%d) dirty=%d px0=%02x%02x%02x%02x identityLight=%.3f\n",
				x, y, w, h, cols, rows, components,
				tr.scratchImage ? tr.scratchImage->texnum : 0u,
				tr.scratchImage ? tr.scratchImage->width : 0,
				tr.scratchImage ? tr.scratchImage->height : 0,
				dirty ? 1 : 0,
				d ? d[0] : 0, d ? d[1] : 0, d ? d[2] : 0, d ? d[3] : 0,
				tr.identityLight );
			s_r200CineLogged++;
		}
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	// rectangle down (CIN_DrawCinematic, `cin_keepAspect`).  Retail never had to paint
	if ( x > 0 || y > 0 || x + w < glConfig.vidWidth || y + h < glConfig.vidHeight ) {
		gles3Program_t	*p2d;

		GLES3_ApplyStateBits( GLS_DEPTHTEST_DISABLE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA );

		GL_BindUnit0( tr.whiteImage );
		p2d = GLES3_BeginFamily( GLP_FAMILY_2D, 1 );
		GLES3_SetBaseColor4f( p2d, 0.0f, 0.0f, 0.0f, 1.0f );
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
		R200_DrainGLError();
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
			R200_DrainGLError();
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
			{
				GLenum err = qglGetError();
				if ( err != GL_NO_ERROR ) {
					static int s_r200UpCineSubErr;
					if ( s_r200UpCineSubErr < 10 ) {
						ri.Printf( PRINT_ALL, "[R200:glerr] cinematic upload glTexSubImage2D %dx%d rejected: 0x%04x\n",
								   cols, rows, err );
						s_r200UpCineSubErr++;
					}
				}
			}
		}
	}

	{
		static int s_r200UpCineLogged;
		if ( s_r200UpCineLogged < 100 ) {
			const byte *d = (const byte *)data;
			ri.Printf( PRINT_ALL,
				"[R200:cine] upload w=%d h=%d cols=%d rows=%d client=%d scratch=%u(%dx%d) dirty=%d px0=%02x%02x%02x%02x\n",
				w, h, cols, rows, client,
				scratch->texnum, scratch->width, scratch->height,
				dirty ? 1 : 0, d ? d[0] : 0, d ? d[1] : 0, d ? d[2] : 0, d ? d[3] : 0 );
			s_r200UpCineLogged++;
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

	R204_RootBegin( R204_FAM_DS );

	if ( tess.numIndexes ) {
		R204_ZONE( R204_FAM_DS, R204_DSZ_end, RB_EndSurface() );
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	R204_Bump( R204_FAM_BE, R204_BEC_surfs, cmd->numDrawSurfs );
	R204_Bump( R204_FAM_DS, R204_DSC_surfs, cmd->numDrawSurfs );

	RB_SetupFog();
	R204_ZONE( R204_FAM_DS, R204_DSZ_walk,
			   RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs ) );

	R204_RootEnd( R204_FAM_DS );

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

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	GLimp_EndFrame();

	backEnd.in2D = qfalse;

	return (const void *)(cmd + 1);
}

#define R204_WINDOW_MS		1000
#define R204_MAX_ROOTS		3600
#define R204_MAX_ROWS		400
#define R204_MAX_ZONES		8
#define R204_MAX_COUNTERS	8
#define R204_MAX_SUBS		4
#define R204_TOP_N			3
#define R204_NAME_LEN		48

typedef struct {
	char	name[R204_NAME_LEN];
	int		usec;
} r204Top_t;

typedef struct {
	int			sums[R204_MAX_ZONES];
	int			maxs[R204_MAX_ZONES];
	int			subSums[R204_MAX_SUBS];
	int			subMaxs[R204_MAX_SUBS];
	int			cntSum[R204_MAX_COUNTERS];
	int			cntMax[R204_MAX_COUNTERS];
	int			totalSum, totalMax;
	int			named;			
	int			roots;			
	int			rows;
	int			stopped;
	r204Top_t	top[R204_TOP_N];
} r204Fam_t;

static r204Fam_t	s_r204[R204_FAM_COUNT];
static int			s_r204RootT0[R204_FAM_COUNT];
static int			s_r204RootArmed[R204_FAM_COUNT];
static int			s_r204WindowStart;
static qboolean		s_r204WindowSet;

static const char *const s_r204FamName[R204_FAM_COUNT] = { "be", "ds", "prog" };

static const char *const s_r204ZoneName[R204_FAM_COUNT][R204_MAX_ZONES] = {
	{ "surfs", "sprites", "sprwalk", "pic", "buf", "swap", "color", "other" },
	{ "walk", "end", "flare", "shadow", "other", NULL, NULL, NULL },
	{ "compile", "link", "other", NULL, NULL, NULL, NULL, NULL }
};
static const int s_r204ZoneCount[R204_FAM_COUNT] = {
	R204_BE_ZONES, R204_DS_ZONES, R204_PROG_ZONES
};

static const char *const s_r204SubName[R204_MAX_SUBS] = {
	"substage", "subvbo", "subunif", "subshadow"
};

static const char *const s_r204CntName[R204_FAM_COUNT][R204_MAX_COUNTERS] = {
	{ "lists", "cmds", "surfN", "drawN", "empty", "sprN", "sprBatch", "sprDraw" },
	{ "surfsN", "batchN", "vertN", "stageN", "drawN", "unifs", "binds", "edgeN" },
	{ "made", NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};
static const int s_r204CntCount[R204_FAM_COUNT] = {
	R204_BE_COUNTERS, R204_DS_COUNTERS, R204_PROG_COUNTERS
};

typedef char r204_tables_agree_enums[
	( R204_FAM_COUNT == 3 && R204_BE_ZONES == 8 && R204_DS_ZONES == 5
	  && R204_PROG_ZONES == 3 && R204_BE_COUNTERS == 8 && R204_DS_COUNTERS == 8
	  && R204_PROG_COUNTERS == 1 && R204_DS_SUBS == 4 ) ? 1 : -1 ];

void R204_Zone( int family, int zone, int usec );

static void R204_FamAdd( r204Fam_t *f, int zone, int usec ) {
	f->named += usec;
	f->sums[zone] += usec;
	if ( usec > f->maxs[zone] ) {
		f->maxs[zone] = usec;
	}
}

static void R204_Reset( int family ) {
	r204Fam_t	*f = &s_r204[family];
	int			i;

	f->named = 0;
	f->roots = 0;
	f->totalSum = f->totalMax = 0;
	for ( i = 0; i < R204_MAX_ZONES; i++ ) {
		f->sums[i] = f->maxs[i] = 0;
	}
	for ( i = 0; i < R204_MAX_SUBS; i++ ) {
		f->subSums[i] = f->subMaxs[i] = 0;
	}
	for ( i = 0; i < R204_MAX_COUNTERS; i++ ) {
		f->cntSum[i] = f->cntMax[i] = 0;
	}
	for ( i = 0; i < R204_TOP_N; i++ ) {
		f->top[i].name[0] = '\0';
		f->top[i].usec = 0;
	}
}

static void R204_Print( int family, int windowMs ) {
	const r204Fam_t	*f = &s_r204[family];
	char			line[1024], part[64];
	int				i, roots, mean10, max10;

	roots = ( f->roots < 1 ) ? 1 : f->roots;

	line[0] = '\0';

	for ( i = 0; i < s_r204ZoneCount[family]; i++ ) {
		mean10 = (int)( ( (long long)f->sums[i] + (long long)roots * 50 )
						/ ( (long long)roots * 100 ) );
		max10 = f->maxs[i] / 100;
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
					 s_r204ZoneName[family][i],
					 mean10 / 10, mean10 % 10, max10 / 10, max10 % 10 );
		Q_strcat( line, sizeof( line ), part );
	}

	for ( i = 0; i < s_r204CntCount[family]; i++ ) {
		mean10 = (int)( ( (long long)f->cntSum[i] * 10 + roots / 2 ) / roots );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 s_r204CntName[family][i],
					 mean10 / 10, mean10 % 10, f->cntMax[i] );
		Q_strcat( line, sizeof( line ), part );
	}

	if ( family == R204_FAM_DS ) {
		for ( i = 0; i < R204_DS_SUBS; i++ ) {
			mean10 = (int)( ( (long long)f->subSums[i] + (long long)roots * 50 )
							/ ( (long long)roots * 100 ) );
			max10 = f->subMaxs[i] / 100;
			Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
						 s_r204SubName[i],
						 mean10 / 10, mean10 % 10, max10 / 10, max10 % 10 );
			Q_strcat( line, sizeof( line ), part );
		}
	}

	if ( family == R204_FAM_PROG ) {
		for ( i = 0; i < R204_TOP_N; i++ ) {
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

	ri.Printf( PRINT_ALL, "[R204:%s] roots=%d window=%dms%s\n",
			   s_r204FamName[family], f->roots, windowMs, line );
}

static void R204_MaybeRoll( void ) {
	int	now, window, i;

	if ( !s_r204WindowSet ) {
		s_r204WindowStart = R201_RLoadNow();
		s_r204WindowSet = qtrue;
		return;
	}

	now = R201_RLoadNow();
	window = now - s_r204WindowStart;
	if ( window < R204_WINDOW_MS && s_r204[R204_FAM_BE].roots < R204_MAX_ROOTS ) {
		return;
	}

	for ( i = 0; i < R204_FAM_COUNT; i++ ) {
		if ( s_r204[i].roots < 1 || s_r204[i].stopped ) {
			R204_Reset( i );
			continue;
		}
		if ( s_r204[i].rows < R204_MAX_ROWS ) {
			R204_Print( i, window );
			s_r204[i].rows++;
		} else {
			s_r204[i].stopped = 1;
			ri.Printf( PRINT_ALL,
					   "[R204:%s] capped at %d rows; no further lines\n",
					   s_r204FamName[i], R204_MAX_ROWS );
		}
		R204_Reset( i );
	}

	s_r204WindowStart = now;
}

void R204_RootBegin( int family ) {
	if ( family < 0 || family >= R204_FAM_COUNT ) {
		return;
	}

	s_r204RootArmed[family] = R204_Armed();
	s_r204RootT0[family] = s_r204RootArmed[family] ? R204_Microseconds() : 0;
	s_r204[family].named = 0;
}

void R204_RootEnd( int family ) {
	int		total, other;

	if ( family < 0 || family >= R204_FAM_COUNT ) {
		return;
	}

	if ( !s_r204RootArmed[family] ) {
		s_r204[family].named = 0;
		return;
	}
	s_r204RootArmed[family] = 0;

	total = R204_Microseconds() - s_r204RootT0[family];
	if ( total < 0 ) {
		total = 0;
	}

	other = total - s_r204[family].named;
	if ( other < 0 ) {
		other = 0;
	}
	R204_FamAdd( &s_r204[family], s_r204ZoneCount[family] - 1, other );
	s_r204[family].named = 0;

	s_r204[family].totalSum += total;
	if ( total > s_r204[family].totalMax ) {
		s_r204[family].totalMax = total;
	}
	s_r204[family].roots++;

	if ( family == R204_FAM_BE || !s_r204RootArmed[R204_FAM_BE] ) {
		R204_MaybeRoll();
	}
}

void R204_Zone( int family, int zone, int usec ) {
	if ( family < 0 || family >= R204_FAM_COUNT || !s_r204RootArmed[family] ) {
		return;
	}
	if ( zone < 0 || zone >= s_r204ZoneCount[family] ) {
		return;
	}
	if ( usec < 0 ) {
		usec = 0;
	}

	R204_FamAdd( &s_r204[family], zone, usec );
}

void R204_Sub( int family, int sub, int usec ) {
	r204Fam_t	*f;

	if ( family != R204_FAM_DS || !s_r204RootArmed[family] ) {
		return;
	}
	if ( sub < 0 || sub >= R204_DS_SUBS ) {
		return;
	}
	if ( usec < 0 ) {
		usec = 0;
	}

	f = &s_r204[family];
	f->subSums[sub] += usec;
	if ( usec > f->subMaxs[sub] ) {
		f->subMaxs[sub] = usec;
	}
}

void R204_Bump( int family, int counter, int n ) {
	r204Fam_t	*f;

	if ( family < 0 || family >= R204_FAM_COUNT || !s_r204RootArmed[family] ) {
		return;
	}
	if ( counter < 0 || counter >= s_r204CntCount[family] ) {
		return;
	}

	f = &s_r204[family];
	f->cntSum[counter] += n;
	if ( n > f->cntMax[counter] ) {
		f->cntMax[counter] = n;
	}
}

int R204_RootUsec( int family ) {
	if ( family < 0 || family >= R204_FAM_COUNT || !s_r204RootArmed[family] ) {
		return 0;
	}
	return R204_Microseconds() - s_r204RootT0[family];
}

void R204_Name( int family, const char *name, int usec ) {
	r204Fam_t	*f;
	int			i, slot;

	if ( family != R204_FAM_PROG || !s_r204RootArmed[family] || !name ) {
		return;
	}

	f = &s_r204[family];

	slot = -1;
	for ( i = 0; i < R204_TOP_N; i++ ) {
		if ( !f->top[i].name[0] || usec > f->top[i].usec ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		return;
	}
	for ( i = R204_TOP_N - 1; i > slot; i-- ) {
		f->top[i] = f->top[i - 1];
	}
	Q_strncpyz( f->top[slot].name, name, R204_NAME_LEN );
	f->top[slot].usec = usec;
}

void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;

	int		r204Cmds = 0;

	{
		static int r201_smpframe_rows = 0;

		if ( r201_smpframe_rows < 8 || ( r201_smpframe_rows % 600 ) == 0 ) {
			ri.Printf( PRINT_ALL,
					   "[R201:smpframe] back=%d front=%d arena=%d frameCount=%d\n",
					   RB_FrameSmpFrame(), tr.smpFrame, RB_FrameBoneArena(), RB_FrameCount() );
		}
		r201_smpframe_rows++;
	}

	t1 = ri.Milliseconds ();

	R204_RootBegin( R204_FAM_BE );

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		r204Cmds++;
		R204_Bump( R204_FAM_BE, R204_BEC_cmds, 1 );

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			R204_ZONE( R204_FAM_BE, R204_BEZ_color, data = RB_SetColor( data ) );
			break;
		case RC_STRETCH_PIC:
			R204_ZONE( R204_FAM_BE, R204_BEZ_pic, data = RB_StretchPic( data ) );
			break;
		case RC_DRAW_SURFS:
			R204_ZONE( R204_FAM_BE, R204_BEZ_surfs, data = RB_DrawSurfs( data ) );
			break;
		case RC_SPRITE_SURFS:
			R204_ZONE( R204_FAM_BE, R204_BEZ_sprites, data = RB_SpriteSurfs( data ) );
			break;
		case RC_DRAW_BUFFER:
			R204_ZONE( R204_FAM_BE, R204_BEZ_buf, data = RB_DrawBuffer( data ) );
			break;
		case RC_SWAP_BUFFERS:
			R204_ZONE( R204_FAM_BE, R204_BEZ_swap, data = RB_SwapBuffers( data ) );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;
		case RC_COLORMASK:
			R204_ZONE( R204_FAM_BE, R204_BEZ_color, data = RB_ColorMask(data) );
			break;
		case RC_CLEARDEPTH:
			R204_ZONE( R204_FAM_BE, R204_BEZ_color, data = RB_ClearDepth(data) );
			break;
		case RC_END_OF_LIST:
		default:
			t2 = ri.Milliseconds ();
			backEnd.pc.msec = t2 - t1;
			R204_Bump( R204_FAM_BE, R204_BEC_lists, 1 );
			if ( r204Cmds <= 1 ) {
				R204_Bump( R204_FAM_BE, R204_BEC_empty, 1 );
			}
			R204_RootEnd( R204_FAM_BE );
			return;
		}
	}

}
