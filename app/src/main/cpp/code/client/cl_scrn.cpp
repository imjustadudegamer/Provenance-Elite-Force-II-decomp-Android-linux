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

#include "client.h"
#include "cl_ui.h"
#include "cl_cin.h"

#include "../uilib/limit_uiprobe.h"

qboolean	scr_initialized;		
stereoFrame_t	s_scr_stereoFrame;

cvar_t		*cl_timegraph;
cvar_t		*cl_debuggraph;
cvar_t		*cl_graphheight;
cvar_t		*cl_graphscale;
cvar_t		*cl_graphshift;
cvar_t		*cl_drawFPS;		

void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


void SCR_AndroidDisplayRect( int surfaceW, int surfaceH, int *outX, int *outY, int *outW, int *outH )
{
	int   mode   = android_aspect ? android_aspect->integer : 0;
	float target = 0.0f;

	if ( outX ) *outX = 0;
	if ( outY ) *outY = 0;
	if ( outW ) *outW = surfaceW;
	if ( outH ) *outH = surfaceH;

	if ( mode == 1 ) {
		target = 16.0f / 9.0f;
	} else if ( mode == 2 ) {
		target = 4.0f / 3.0f;
	} else {
		return;    
	}

	if ( surfaceW <= 0 || surfaceH <= 0 ) {
		return;
	}

	if ( (float)surfaceW / (float)surfaceH > target ) {
		int w = (int)( (float)surfaceH * target + 0.5f );

		if ( w <= 0 ) w = 1;
		if ( outW ) *outW = w;
		if ( outX ) *outX = ( surfaceW - w ) / 2;
	} else {
		int h = (int)( (float)surfaceW / target + 0.5f );

		if ( h <= 0 ) h = 1;
		if ( outH ) *outH = h;
		if ( outY ) *outY = ( surfaceH - h ) / 2;
	}
}

void SCR_DrawAndroidBars( void ) {
	int dx, dy, dw, dh;
	int right, bottom;

	SCR_AndroidDisplayRect( cls.glconfig.vidWidth, cls.glconfig.vidHeight, &dx, &dy, &dw, &dh );
	if ( dx == 0 && dy == 0 && dw == cls.glconfig.vidWidth && dh == cls.glconfig.vidHeight ) {
		return;
	}

	right  = dx + dw;
	bottom = dy + dh;

	int limit_subT0 = LIMIT_SubT0();

	re.Set2DWindow(
		0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight,
		0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0,
		-1, 1 );

	LIMIT_SubPushEnd( limit_subT0 );

	re.SetColor( g_color_table[0] );   
	if ( dy > 0 ) {
		re.DrawBox( 0, 0, (float)cls.glconfig.vidWidth, (float)dy );
		re.DrawBox( 0, (float)bottom, (float)cls.glconfig.vidWidth, (float)( cls.glconfig.vidHeight - bottom ) );
	}
	if ( dx > 0 ) {
		re.DrawBox( 0, (float)dy, (float)dx, (float)dh );
		re.DrawBox( (float)right, (float)dy, (float)( cls.glconfig.vidWidth - right ), (float)dh );
	}
	re.SetColor( NULL );
}

void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	float	xscale;
	float	yscale;
	int	dx, dy, dw, dh;

	SCR_AndroidDisplayRect( cls.glconfig.vidWidth, cls.glconfig.vidHeight, &dx, &dy, &dw, &dh );

	xscale = (float)dw / 640.0f;
	yscale = (float)dh / 480.0f;
	if ( x ) {
		*x = *x * xscale + (float)dx;
	}
	if ( y ) {
		*y = *y * yscale + (float)dy;
	}
	if ( w ) {
		*w *= xscale;
	}
	if ( h ) {
		*h *= yscale;
	}
}

void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}







static	int			current;
static	float		values[1024];

static void SCR_DrawStringFont( fontheader_t *font, float x, float y, float scale,
	const float *color, const char *s )
{
	float vs[2];

	if ( !font || !s || !*s ) {
		return;
	}
	vs[0] = vs[1] = scale;
	re.SetColor( color );
	re.DrawString( font, s, x / scale, y / scale, -1, vs );
	re.SetColor( NULL );
}

#define	FPS_FRAMES	4
static void SCR_DrawFPS( void ) {
	static int	previousTimes[FPS_FRAMES];
	static int	index;
	static int	previous;
	static fontheader_t *fpsFont;
	char		line[160];
	int			t, frameTime, i, total, framesPerSecond;
	float		W, H, scale, w, x, y;
	float		white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	if ( !cl_drawFPS || cl_drawFPS->integer <= 0 ) {
		return;
	}

	t = Com_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if ( index <= FPS_FRAMES ) {
		return;		
	}

	total = 0;
	for ( i = 0; i < FPS_FRAMES; i++ ) {
		total += previousTimes[i];
	}
	if ( total < 1 ) {
		total = 1;
	}
	framesPerSecond = 1000 * FPS_FRAMES / total;

	if ( !fpsFont ) {
		fpsFont = re.LoadFont( "tahoma-10" );
		if ( !fpsFont ) {
			return;
		}
	}

	W = uid.vidWidth;
	H = uid.vidHeight;
	if ( W <= 0.0f || H <= 0.0f ) {
		return;
	}

	scale = H / 480.0f;		
	if ( scale < 1.0f ) scale = 1.0f;
	if ( scale > 6.0f ) scale = 6.0f;

	Com_sprintf( line, sizeof( line ), "%ifps", framesPerSecond );
	w = re.GetFontStringWidth( fpsFont, line ) * scale;
	x = W - w - 4.0f * scale;
	y = 4.0f * scale;
	SCR_DrawStringFont( fpsFont, x, y, scale, white, line );

	if ( cl_drawFPS->integer >= 2 ) {
		int	frames = 0, mean10[SMP_BUDGET_ZONES];

		y += re.GetFontHeight( fpsFont ) * scale + 2.0f * scale;

		if ( SMP_BudgetLastWindow( &frames, mean10 ) ) {
			Com_sprintf( line, sizeof( line ),
				"srv=%d.%d cli=%d.%d ren=%d.%d/%d.%d aud=%d.%d swp=%d.%d spin=%d.%d tot=%d.%d n=%d",
				mean10[0] / 10, mean10[0] % 10, mean10[1] / 10, mean10[1] % 10,
				mean10[2] / 10, mean10[2] % 10, mean10[3] / 10, mean10[3] % 10,
				mean10[4] / 10, mean10[4] % 10, mean10[7] / 10, mean10[7] % 10,
				mean10[8] / 10, mean10[8] % 10, mean10[10] / 10, mean10[10] % 10,
				frames );
		} else {
			Q_strncpyz( line, "(frame budget: no window closed yet)", sizeof( line ) );
		}
		w = re.GetFontStringWidth( fpsFont, line ) * scale;
		SCR_DrawStringFont( fpsFont, W - w - 4.0f * scale, y, scale, white, line );
	}
}

void SCR_DebugGraph (float value)
{
	values[current] = value;
	current = (current + 1) % ARRAY_LEN(values);
}

void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;

	w = uid.vidWidth;
	x = 0;
	y = uid.vidHeight;
	re.SetColor( g_color_table[0] );
	re.DrawStretchPic(x, y - cl_graphheight->integer, 
		w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (ARRAY_LEN(values)+current-1-(a % ARRAY_LEN(values))) % ARRAY_LEN(values);
		v = values[i];
		v = v * cl_graphscale->integer + cl_graphshift->integer;
		
		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}


void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);

	cl_drawFPS = Cvar_Get ("cg_drawFPS", "1", CVAR_ARCHIVE);

#ifdef __ANDROID__
#endif

	scr_initialized = qtrue;
}



#ifdef __ANDROID__
#include "touch_controls.h"

static qhandle_t tc_circleShader;
static qhandle_t tc_ringShader;
static qhandle_t tc_pillShader;

static fontheader_t *tc_labelFont;

static void CL_DrawTouchText( float ncx, float ncy, const char *label,
	float maxW, float maxH, const float *color )
{
	float W = cls.glconfig.vidWidth, H = cls.glconfig.vidHeight;
	float rawH, rawW, s, lx, ly;
	float vs[2];

	if ( !tc_labelFont || !label || !*label ) return;

	rawH = re.GetFontHeight( tc_labelFont );
	rawW = re.GetFontStringWidth( tc_labelFont, label );
	if ( rawH <= 0.0f ) return;

	s = maxH / rawH;
	if ( rawW > 0.0f && rawW * s > maxW ) s = maxW / rawW;
	if ( s <= 0.0f ) return;

	lx = ncx * W - rawW * s * 0.5f;
	ly = ncy * H - rawH * s * 0.5f;
	vs[0] = vs[1] = s;

	re.SetColor( color );
	re.DrawString( tc_labelFont, label, lx / s, ly / s, -1, vs );
	re.SetColor( NULL );
}

[[maybe_unused]] static void CL_DrawTouchLabel( float ncx, float ncy, const char *label, float *color )
{
	float H = cls.glconfig.vidHeight;
	CL_DrawTouchText( ncx, ncy, label, cls.glconfig.vidWidth, H * 0.028f, color );
}

static void CL_DrawTouchShader( float ncx, float ncy, float nr, qhandle_t shader,
	float cr, float cg, float cb, float alpha )
{
	float W = cls.glconfig.vidWidth, H = cls.glconfig.vidHeight;
	float da = ( touchControl.deviceW > 0 && touchControl.deviceH > 0 )
		? touchControl.deviceW / touchControl.deviceH
		: ( H > 0 ? W / H : 2.0f );
	float hw = nr * W;            
	float hh = nr * H * da;       
	float col[4];
	col[0] = cr; col[1] = cg; col[2] = cb; col[3] = alpha;
	re.SetColor( col );
	re.DrawStretchPic( ncx * W - hw, ncy * H - hh, 2.0f * hw, 2.0f * hh, 0, 0, 1, 1, shader );
	re.SetColor( NULL );
}

static void CL_DrawTouchPill( float ncx, float ncy, float nhw, float nhh, qboolean pressed,
	const char *label, float cr, float cg, float cb, float opacity )
{
	float W = cls.glconfig.vidWidth, H = cls.glconfig.vidHeight;
	float black[4] = { 0, 0, 0, 1 };
	float col[4];
	if ( pressed ) {
		col[0] = 0.80f; col[1] = 0.40f; col[2] = 0.60f; col[3] = 1.0f * opacity; 
	} else {
		col[0] = cr; col[1] = cg; col[2] = cb; col[3] = 0.82f * opacity;
	}
	re.SetColor( col );
	re.DrawStretchPic( ( ncx - nhw ) * W, ( ncy - nhh ) * H, 2.0f * nhw * W, 2.0f * nhh * H,
		0, 0, 1, 1, tc_pillShader );
	re.SetColor( NULL );
	if ( label && *label ) {
		float pillW  = 2.0f * nhw * W;
		float pillH  = 2.0f * nhh * H;
		float cap    = nhh * H;                       
		float avail  = pillW - 2.0f * cap;            
		if ( avail < pillW * 0.5f ) avail = pillW * 0.5f;  
		black[3] = opacity;
		CL_DrawTouchText( ncx, ncy, label, avail, pillH * 0.72f, black );
	}
}

void CL_TouchRegisterShaders( void )
{
	// uivk-android: the names MUST carry the real extension.  Retail's R_LoadImage
	tc_circleShader = re.RegisterShaderNoMip( "gfx/touch/circle.tga" );
	tc_ringShader   = re.RegisterShaderNoMip( "gfx/touch/ring.tga" );
	tc_pillShader   = re.RegisterShaderNoMip( "gfx/touch/pill.tga" );

	if ( !tc_circleShader || !tc_ringShader || !tc_pillShader ) {
		Com_Printf( S_COLOR_RED "WARNING: touch overlay images missing "
			"(circle=%d ring=%d pill=%d) -- using the white fallback. "
			"Is zpak-android.pk3 present in base/?\n",
			tc_circleShader, tc_ringShader, tc_pillShader );
	}
	if ( !tc_circleShader ) tc_circleShader = cls.whiteShader;
	if ( !tc_ringShader )   tc_ringShader   = cls.whiteShader;
	if ( !tc_pillShader )   tc_pillShader   = cls.whiteShader;

	tc_labelFont = re.LoadFont( "tahoma-10" );
	if ( !tc_labelFont ) {
		Com_Printf( S_COLOR_RED "WARNING: touch overlay label font 'tahoma-10' "
			"failed to load -- pill labels will be blank.\n" );
	}
}

void CL_DrawTouchControls( void ) {
	float padCx, padCy;
	int i;
	float scale, opacity;

	if ( !tc_pillShader )
		return;


	if ( !touchControl.overlayStartMs )
		touchControl.overlayStartMs = Sys_Milliseconds();

	{
		int idleRef = ( touchControl.lastTouchMs > touchControl.overlayStartMs )
			? touchControl.lastTouchMs : touchControl.overlayStartMs;
		if ( !touchControl.moveActive && !touchControl.actionFingerHeld
			&& Sys_Milliseconds() - idleRef > 4000 )
			return;
	}

	re.Set2DWindow(
		0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight,
		0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0,
		-1, 1 );

	{
		float s = Cvar_VariableValue( "cl_touchScale" );
		float o = Cvar_VariableValue( "cl_touchOpacity" );
		if ( s < 0.6f ) s = 0.6f;
		if ( s > 2.0f ) s = 2.0f;
		if ( o < 0.05f ) o = 0.05f;
		if ( o > 1.0f ) o = 1.0f;
		scale = s;
		opacity = o;
	}

	if ( touchControl.moveActive ) {
		padCx = touchControl.padCx; padCy = touchControl.padCy;
	} else {
		padCx = TC_PAD_CX; padCy = TC_PAD_CY;
	}
	CL_DrawTouchShader( padCx, padCy, TC_PAD_R * scale, tc_ringShader, LCARS_ORANGE,
		0.45f * opacity * ( touchControl.moveActive ? 1.55f : 1.0f ) );
	CL_DrawTouchShader(
		touchControl.moveActive ? touchControl.thumbX : padCx,
		touchControl.moveActive ? touchControl.thumbY : padCy,
		TC_PAD_R * 0.45f * scale, tc_circleShader, LCARS_GOLD,
		( touchControl.moveActive ? 1.0f : 0.7f ) * opacity );

	for ( i = 0; i < TC_NUM_ACTION; i++ ) {
		const tcActionButton_t *ab = &tcActionButtons[i];
		qboolean pressed = ( touchControl.actionPressed & ( 1 << i ) ) != 0;
		CL_DrawTouchPill( ab->cx, ab->cy, ab->hw * scale, ab->hh * scale, pressed, ab->label,
			ab->r, ab->g, ab->b, opacity );
	}
}

qboolean CL_TouchControlsVisible( void ) {
	if ( Key_GetCatcher( ) & ( KEYCATCH_CONSOLE | KEYCATCH_UI ) ) return qfalse;
	if ( UI_MenuActive() ) return qfalse;
	if ( UI_IsHudActive() ) return qfalse;
	if ( cls.no_menus ) return qfalse;          
	if ( cls.state != CA_ACTIVE ) return qfalse;
	if ( cinematicRunning() ) return qfalse;
	if ( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) return qfalse;
	return qtrue;
}

#endif 

void SCR_DrawScreenField( void ) {
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			re.Set2DWindow(
				0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight,
				0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0,
				-1, 1 );
			re.SetColor( g_color_table[0] );
			re.DrawBox( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight );
			re.SetColor( NULL );
		}
	}

	if ( cinematicRunning() ) {
		SCR_DrawCinematic();
		return;
	}

	// CA_CONNECTING/CA_CHALLENGING/CA_CONNECTED into UI_DrawConnect is not retail:
	switch( cls.state ) {
	case CA_UNINITIALIZED:
		Com_Error( ERR_FATAL, "SCR_DrawScreenField: cls.state == CA_UNINITIALIZED" );   
		break;
	case CA_LOADING:
	case CA_PRIMED:
	case CA_ACTIVE:
		{
			SMP_LOAD_BEGIN( cgame );
			CL_CGameRendering(s_scr_stereoFrame);
			SMP_LOAD_END( cgame );
		}
		LIMIT_SubArm();
		{
			int limit_subT0 = LIMIT_SubT0();

			SCR_DrawAndroidBars();

			LIMIT_SubZoneEnd( LIMIT_SUBZ_abars, limit_subT0 );
		}
		break;
	case CA_CINEMATIC:
		SCR_DrawCinematic();
		break;
	default:
		break;
	}
}

void UpdateStereoSide( stereoFrame_t s ) {
	s_scr_stereoFrame = s;
	re.BeginFrame( s );
	if (cls.state == CA_CINEMATIC || GetBackgroundMode() == 2 || cinematicRunning()) {
		SCR_DrawScreenField();
	} else {
		SMP_LOAD_BEGIN( ui );
		UI_Update();
		SMP_LOAD_END( ui );
	}

	/* On every frame, and BEFORE both overlays. `SCR_DrawScreenField' can be skipped (cls.state ==
	   CA_DISCONNECTED, or a frame with no View3D widget), so the bands would otherwise keep an
	   undefined back-buffer; and the touch pad and pills are laid out in FULL-SURFACE coordinates
	   (sdl_input.c maps a touch to vidWidth/vidHeight), so a band painted after them cuts off
	   whatever falls in it -- at 4:3 the pad sits entirely inside the left band. The bands are still
	   after the pass that can write the back-buffer, and the overlays only add their own quads, so
	   the bands keep covering the rest of it. A no-op when the rect is the surface (android_aspect
	   0), so mode 0 is bit-identical. */
	SCR_DrawAndroidBars();

	/* The debug counter goes UNDER the touch overlay: it is drawn from uid.vidWidth/
	   vidHeight (inside the display rect) and must not paint over the pad or a pill,
	   so the overlay stays on top and complete. */
	SCR_DrawFPS();

#ifdef __ANDROID__
	if ( CL_TouchControlsVisible() ) {
		CL_DrawTouchControls();
	}
#endif
}

void SCR_SimpleUpdateScreen( void ) {
	// landed-fixes: DO NOT TOUCH GL WHILE THE APP IS BACKGROUNDED.
	if ( Cvar_VariableIntegerValue( "com_minimized" ) ) {
		return;
	}

	if( cls.glconfig.stereoEnabled ) {
		UpdateStereoSide( STEREO_LEFT );
		UpdateStereoSide( STEREO_RIGHT );
	}
	else {
		UpdateStereoSide( STEREO_CENTER );
	}

	if( com_speeds->integer || Cvar_VariableIntegerValue( "smp_frameStats" ) ) {
		re.EndFrame( &time_frontend, &time_backend );
	}
	else {
		re.EndFrame( NULL, NULL );
	}
}

static qboolean scr_update_recursive;

void SCR_ResetUpdateGuard( void ) {
	scr_update_recursive = qfalse;
}

void SCR_UpdateScreen( void ) {
	if ( !scr_initialized ) {
		return;				
	}

	if (scr_update_recursive) {
		return;
	}

	scr_update_recursive = qtrue;

	SMP_LOAD_BEGIN( scr );

	CL_StartHunkUsers(qfalse);
	SCR_SimpleUpdateScreen();

	/* The save shot must come from a frame the pause menu did not composite
	   over, so the UI stays suppressed until the renderer reports the shot was
	   taken; a frame cap bounds the wait so the menu can never be lost. */
	if ( cls.no_menus ) {
		if ( re.ScreenshotPending && !re.ScreenshotPending() ) {
			cls.no_menus = qfalse;
			cls.saveShotFrames = 0;
		} else if ( ++cls.saveShotFrames > 8 ) {
			cls.no_menus = qfalse;
			cls.saveShotFrames = 0;
		}
	}

	if( fps->integer && cls.state == CA_ACTIVE ) {
		static int belowlastplaytime = 0;
		static qboolean belowframerate = qfalse;

		if( currentfps > 19.8 ) {
			belowframerate = qfalse;
		} else if( !belowframerate ) {
			if( currentfps < 18.0 ) {
				belowlastplaytime = 10000;
				belowframerate = qtrue;
			}
		}

		if( belowframerate && cls.realtime > belowlastplaytime ) {
			belowlastplaytime = cls.realtime + 4000;
		}
	}

	scr_update_recursive = qfalse;

	SMP_LOAD_END( scr );
}

int cl_loadingScreenStage = 0;

void CL_ProcessLoadingScreen( const char *loadingStatus ) {
	static cvar_t	*cl_loadingScreenDelay = NULL;
	static int		lastScreenUpdate = 0;
	cvar_t			*loadingstatus;

	if ( !loadingStatus ) {
		return;
	}

	if ( com_dedicated && com_dedicated->integer ) {
		return;
	}

	if ( !cl_loadingScreenDelay ) {
		cl_loadingScreenDelay = Cvar_Get( "cl_loadingScreenDelay", "100", CVAR_ARCHIVE );
	}
	loadingstatus = Cvar_Get( "loadingstatus", "", 0 );

	if ( !strcmp( loadingstatus->string, loadingStatus )
		&& Sys_Milliseconds() < lastScreenUpdate + cl_loadingScreenDelay->integer ) {
		return;
	}

	if ( loadingStatus[0] ) {
		if ( !Q_stricmp( loadingStatus, "$$LoadingMap$$" ) ) {
			cl_loadingScreenStage = 2;
		} else if ( !Q_stricmp( loadingStatus, "$$SpawningEntities$$" ) ) {
			cl_loadingScreenStage = 3;
		} else if ( !Q_stricmp( loadingStatus, "$$RunningFirstGameFrames$$" ) ) {
			cl_loadingScreenStage = 4;
		} else if ( !Q_stricmp( loadingStatus, "$$LoadingClientGame$$" ) ) {
			cl_loadingScreenStage = 5;
		}

		UI_UpdateLoadingStatus( cl_loadingScreenStage );

		Cvar_Set( "loadingstatus", loadingStatus );
	}

	if ( cls.state < CA_ACTIVE ) {
		SCR_UpdateScreen();
		lastScreenUpdate = Sys_Milliseconds();
	}
}

