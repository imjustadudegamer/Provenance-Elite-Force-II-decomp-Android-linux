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
	if ( outX ) *outX = 0;
	if ( outY ) *outY = 0;
	if ( outW ) *outW = surfaceW;
	if ( outH ) *outH = surfaceH;

#ifndef __ANDROID__
	/* the desktop window is the display: r_mode owns its shape, retail ships no aspect
	   row, and the archived handheld cvar must not letterbox a window it cannot resize */
	return;
#else
	/* the handheld's letterbox */
	int   mode   = android_aspect ? android_aspect->integer : 0;
	float target = 0.0f;

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
#endif
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
	float		W, H, scale, x, y;
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

	/* The counter draws after the UI pass, so the last widget's clip is still the
	   scissor in force and the glyphs land in a dead rectangle.  A screen-level
	   overlay arms its own window, as the bars and the touch overlay do. */
	re.Set2DWindow(
		uid.originX, uid.surfaceHeight - uid.originY - uid.vidHeight,
		uid.vidWidth, uid.vidHeight,
		0, (float)uid.vidWidth, (float)uid.vidHeight, 0,
		-1, 1 );

	Com_sprintf( line, sizeof( line ), "%ifps", framesPerSecond );
	x = 4.0f * scale;
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
		SCR_DrawStringFont( fpsFont, x, y, scale, white, line );
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
	/* a quit that unwound out of a draw leaves the hold set; a fresh boot is never inside one */
	SCR_ResetUpdateGuard();
}



#ifdef __ANDROID__
#include "touch_controls.h"

static qhandle_t tc_circleShader;
static qhandle_t tc_ringShader;
static qhandle_t tc_pillShader;
static qhandle_t tc_arrowShader[4];   /* up, right, down, left -- the mode-select cross */

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
	/* 0 is a legal index -> tr.shaders[0], not a white fill: refuse */
	if ( !shader )
		return;

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

/* same aspect the hit test resolves through IN_TouchAspect */
static float TC_DrawAspect( void )
{
	float W = cls.glconfig.vidWidth, H = cls.glconfig.vidHeight;
	if ( touchControl.deviceW > 0 && touchControl.deviceH > 0 )
		return touchControl.deviceW / touchControl.deviceH;
	return ( H > 0 ? W / H : 2.0f );
}

/* LCARS bar: semicircular cap one end, flat cut the other; the cap stays inside the hit rect */
/* pill.tga's vertical middle is opaque (ST 0.2383..0.7578), so it draws as a sharp-cornered solid rect */
#define TC_PILL_SOLID_S1 0.2383f
#define TC_PILL_SOLID_S2 0.7578f

static void CL_DrawTouchBar( float ncx, float ncy, float nhw, float nhh,
	qboolean capLeft, const char *label, qboolean pressed,
	float cr, float cg, float cb, float opacity )
{
	float W = cls.glconfig.vidWidth, H = cls.glconfig.vidHeight;
	float da     = TC_DrawAspect();
	float pillhh = TC_PillHalfH( nhh, da );
	float cap    = TC_BarCapR( nhh, da );
	/* body runs to the flat cut, so the bar spans exactly cx +/- nhw */
	float bodyW  = ( 2.0f * nhw - cap ) * W;
	float bodyX  = capLeft ? ( ncx - nhw + cap ) * W : ( ncx - nhw ) * W;
	float capX   = capLeft ? ( ncx - nhw ) * W : ( ncx + nhw - 2.0f * cap ) * W;
	float bodyY  = ( ncy - pillhh ) * H;
	float bodyH  = 2.0f * pillhh * H;
	float capW   = 2.0f * cap * W;
	if ( !tc_pillShader || !tc_circleShader )
		return;

	float black[4] = { 0, 0, 0, 1 };
	float col[4];

	/* a bar too short to hold a body and a cap degrades to a whole pill */
	if ( cap >= nhw ) { bodyW = 0.0f; capX = ( ncx - nhw ) * W; capW = 2.0f * nhw * W; }

	col[0] = cr; col[1] = cg; col[2] = cb;
	if ( pressed ) {
		/* hopbush, the live-finger flash this overlay has always used */
		col[0] = 0.80f; col[1] = 0.40f; col[2] = 0.60f; col[3] = 1.0f * opacity;
	} else {
		col[3] = 0.82f * opacity;
	}
	re.SetColor( col );
	if ( bodyW > 0.0f )
		re.DrawStretchPic( bodyX, bodyY, bodyW, bodyH,
			TC_PILL_SOLID_S1, 0.0f, TC_PILL_SOLID_S2, 1.0f, tc_pillShader );
	re.DrawStretchPic( capX, bodyY, capW, bodyH, 0, 0, 1, 1, tc_circleShader );
	re.SetColor( NULL );

	if ( label && *label ) {
		/* label clears the cap on both sides */
		float barW  = 2.0f * nhw * W;
		float avail = barW - 2.0f * capW;
		if ( avail < barW * 0.5f ) avail = barW * 0.5f;
		black[3] = opacity;
		CL_DrawTouchText( ncx, ncy, label, avail, bodyH * 0.72f, black );
	}
}

/* sized in x-normalised units on both axes, so the pad's four arms stay square */
static void CL_DrawTouchRect( float ncx, float ncy, float nw, float nh,
	float cr, float cg, float cb, float alpha )
{
	if ( !tc_pillShader )
		return;

	float W = cls.glconfig.vidWidth, H = cls.glconfig.vidHeight;
	float da = TC_DrawAspect();
	float col[4];
	col[0] = cr; col[1] = cg; col[2] = cb; col[3] = alpha;
	re.SetColor( col );
	re.DrawStretchPic( ( ncx - nw ) * W, ( ncy - nh / da ) * H,
		2.0f * nw * W, 2.0f * ( nh / da ) * H,
		TC_PILL_SOLID_S1, 0.0f, TC_PILL_SOLID_S2, 1.0f, tc_pillShader );
	re.SetColor( NULL );
}

/* LCARS mode-select cross: bezel, four arms to arrowheads, thumb dot.  Draw-only: the pad has no hit rect */
#define TC_PAD_ARM_W  0.024f    /* an arm's thickness, x-normalised */
#define TC_PAD_HUB_R  0.016f    /* the hub cap where the arms meet */
#define TC_PAD_TIP    0.66f     /* arms reach this far into TC_PAD_R */
#define TC_PAD_HEAD   0.034f    /* an arrowhead's x radius */

static void CL_DrawTouchModeSelect( float ncx, float ncy, float scale,
	float opacity )
{
	float R    = TC_PAD_R * scale;
	float dx   = touchControl.thumbX - ncx;
	float dy   = ( touchControl.thumbY - ncy ) / TC_DrawAspect();
	float tip  = R * TC_PAD_TIP;
	/* half-length: the bar runs from the hub's edge out to the tip */
	float arm  = ( tip - TC_PAD_HUB_R ) * 0.5f;
	float mid  = ( tip + TC_PAD_HUB_R ) * 0.5f;
	float dim  = ( touchControl.moveActive ? 0.60f : 0.42f ) * opacity;
	float d[4];
	static const float px[4] = {  0.0f, 1.0f, 0.0f, -1.0f };
	static const float py[4] = { -1.0f, 0.0f, 1.0f,  0.0f };
	int i;

	/* the bezel, orange over tangerine with the black channel between them */
	CL_DrawTouchShader( ncx, ncy, R, tc_ringShader, LCARS_ORANGE,
		( touchControl.moveActive ? 0.42f : 0.30f ) * opacity );
	CL_DrawTouchShader( ncx, ncy, R * 0.80f, tc_ringShader, LCARS_TANGERINE,
		( touchControl.moveActive ? 0.30f : 0.20f ) * opacity );

	/* the four arms, chunky as the show's own cross is, in hopbush */
	CL_DrawTouchRect( ncx, ncy - mid, TC_PAD_ARM_W, arm / TC_DrawAspect(),
		LCARS_HOPBUSH, dim );
	CL_DrawTouchRect( ncx, ncy + mid, TC_PAD_ARM_W, arm / TC_DrawAspect(),
		LCARS_HOPBUSH, dim );
	CL_DrawTouchRect( ncx - mid, ncy, arm, TC_PAD_ARM_W / TC_DrawAspect(),
		LCARS_HOPBUSH, dim );
	CL_DrawTouchRect( ncx + mid, ncy, arm, TC_PAD_ARM_W / TC_DrawAspect(),
		LCARS_HOPBUSH, dim );

	/* how far the thumb has pushed along each arm, in x-normalised pixels */
	/* an idle pad must not light an arm from a stale thumb */
	if ( !touchControl.moveActive ) { dx = 0.0f; dy = 0.0f; }

	d[0] = ( dy < 0.0f ) ? -dy : 0.0f;   /* up */
	d[1] = ( dx > 0.0f ) ?  dx : 0.0f;   /* right */
	d[2] = ( dy > 0.0f ) ?  dy : 0.0f;   /* down */
	d[3] = ( dx < 0.0f ) ? -dx : 0.0f;   /* left */

	for ( i = 0; i < 4; i++ ) {
		float t = ( d[i] - TC_PAD_DEADZONE ) / ( TC_PAD_R - TC_PAD_DEADZONE );
		float r, g, b, a;
		if ( t < 0.0f ) t = 0.0f;
		if ( t > 1.0f ) t = 1.0f;
		/* tanoi -> mars as the direction engages */
		r = 1.00f;
		g = 0.80f + ( 0.133f - 0.80f ) * t;
		b = 0.60f + ( 0.000f - 0.60f ) * t;
		a = ( 0.50f + 0.50f * t ) * opacity;
		CL_DrawTouchShader( ncx + px[i] * tip, ncy + py[i] * tip,
			TC_PAD_HEAD * scale, tc_arrowShader[i], r, g, b, a );
	}

	/* the hub cap where the arms meet */
	CL_DrawTouchShader( ncx, ncy, TC_PAD_HUB_R * scale, tc_circleShader,
		LCARS_VIOLET, ( touchControl.moveActive ? 0.75f : 0.55f ) * opacity );

	/* the thumb dot: the one part of the pad that still reads as a stick */
	CL_DrawTouchShader(
		touchControl.moveActive ? touchControl.thumbX : ncx,
		touchControl.moveActive ? touchControl.thumbY : ncy,
		TC_PAD_R * 0.20f * scale, tc_circleShader, LCARS_GOLD,
		( touchControl.moveActive ? 0.95f : 0.55f ) * opacity );
}

void CL_TouchRegisterShaders( void )
{
	// the names MUST carry the real extension.  Retail's R_LoadImage
	tc_circleShader = re.RegisterShaderNoMip( "gfx/touch/circle.tga" );
	tc_ringShader   = re.RegisterShaderNoMip( "gfx/touch/ring.tga" );
	tc_pillShader   = re.RegisterShaderNoMip( "gfx/touch/pill.tga" );
	tc_arrowShader[0] = re.RegisterShaderNoMip( "gfx/touch/arrow_up.tga" );
	tc_arrowShader[1] = re.RegisterShaderNoMip( "gfx/touch/arrow_right.tga" );
	tc_arrowShader[2] = re.RegisterShaderNoMip( "gfx/touch/arrow_down.tga" );
	tc_arrowShader[3] = re.RegisterShaderNoMip( "gfx/touch/arrow_left.tga" );

	if ( !tc_circleShader || !tc_ringShader || !tc_pillShader
		|| !tc_arrowShader[0] || !tc_arrowShader[1]
		|| !tc_arrowShader[2] || !tc_arrowShader[3] ) {
		Com_Printf( S_COLOR_RED "WARNING: touch overlay images missing "
			"(circle=%d ring=%d pill=%d up=%d right=%d down=%d left=%d) "
			"-- using the white fallback.  Is zpak-android.pk3 in base/?\n",
			tc_circleShader, tc_ringShader, tc_pillShader,
			tc_arrowShader[0], tc_arrowShader[1],
			tc_arrowShader[2], tc_arrowShader[3] );
	}
	/* no white fallback: cls.whiteShader is never assigned, so 0 would reach tr.shaders[0] */

	tc_labelFont = re.LoadFont( "tahoma-10" );
	if ( !tc_labelFont ) {
		Com_Printf( S_COLOR_RED "WARNING: touch overlay label font 'tahoma-10' "
			"failed to load -- bar labels will be blank.\n" );
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

	/* the pad follows the finger: floating origin */
	if ( touchControl.moveActive ) {
		padCx = touchControl.padCx; padCy = touchControl.padCy;
	} else {
		padCx = TC_PAD_CX; padCy = TC_PAD_CY;
	}
	CL_DrawTouchModeSelect( padCx, padCy, scale, opacity );

	/* the spine: drawn here, so it has no key and no hit rect */
	CL_DrawTouchBar( TC_SPINE_CX, TC_SPINE_CY, TC_SPINE_HW * scale,
		TC_SPINE_HH * scale, qfalse, NULL, qfalse, LCARS_HOPBUSH, opacity );

	for ( i = 0; i < TC_NUM_ACTION; i++ ) {
		const tcActionButton_t *ab = &tcActionButtons[i];
		qboolean pressed = ( touchControl.actionPressed & ( 1 << i ) ) != 0;
		CL_DrawTouchBar( ab->cx, ab->cy, ab->hw * scale, ab->hh * scale,
			ab->capLeft, ab->label, pressed, ab->r, ab->g, ab->b, opacity );
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

	/* on every frame, and BEFORE both overlays: SCR_DrawScreenField can be skipped, so the bands would keep an undefined back-buffer, and the touch
	   pad and pills are laid out in FULL-SURFACE coordinates, so a band painted after them cuts off whatever falls in it (at 4:3 the pad sits entirely
	   inside the left band).  A no-op when the rect is the surface (android_aspect 0), so mode 0 is bit-identical. */
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

/* the renderer has read the shot, or the hold has run out: the screen is free again */
static void SCR_SaveShotRelease( void ) {
	cls.no_menus = qfalse;
	cls.saveShotFrames = 0;
	cls.saveShotCmd[0] = 0;
	if ( cls.saveShotDrawModel >= 0.0f ) {
		cvar_t *drawviewmodel = Cvar_FindVar( "r_drawviewmodel" );

		if ( drawviewmodel ) {
			Cvar_SetValue( "r_drawviewmodel", cls.saveShotDrawModel );
		}
		cls.saveShotDrawModel = -1.0f;
	}
}

void SCR_ResetUpdateGuard( void ) {
	scr_update_recursive = qfalse;
}

/* a quit that lands inside the hold must not carry the suppression or a pending
   readback into the renderer shutdown: the shot is dropped, never taken half-way */
void SCR_SaveShotDrop( void ) {
	if ( cls.no_menus || cls.saveShotCmd[0] ) {
		SCR_SaveShotRelease();
	}
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

	/* The thumbnail is the world frame the player was looking at.  A save made from
	   a page holds the request until a frame the world is drawing again, so the page
	   keeps the screen for the whole write; the suppression and the clean viewmodel
	   arm here with it, never ahead of it. */
	if ( cls.saveShotCmd[0] && cls.state == CA_ACTIVE && !UI_MenuActive() ) {
		cls.no_menus = qtrue;

		if ( cls.saveShotDrawModel < 0.0f ) {
			cvar_t *drawviewmodel = Cvar_FindVar( "r_drawviewmodel" );

			if ( drawviewmodel ) {
				cls.saveShotDrawModel = drawviewmodel->value;
				Cvar_SetValue( "r_drawviewmodel", 0.0f );
			}
		}

		Cbuf_ExecuteText( EXEC_NOW, cls.saveShotCmd );
		cls.saveShotCmd[0] = 0;
	}

	SCR_SimpleUpdateScreen();

	/* The UI stays suppressed until the renderer reports the shot was taken.  A
	   world that never comes -- quit to menu while the request was held -- still
	   has to give the screen back, so the hold is bounded by frames actually
	   drawn in the world, not by the wall clock. */
	if ( cls.no_menus ) {
		if ( re.ScreenshotPending && !re.ScreenshotPending() ) {
			SCR_SaveShotRelease();
		} else if ( cls.state != CA_ACTIVE ) {
			cls.saveShotFrames = 0;
		} else if ( ++cls.saveShotFrames > 600 ) {
			SCR_SaveShotRelease();
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

