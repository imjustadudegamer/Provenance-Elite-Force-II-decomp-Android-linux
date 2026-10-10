/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction
#ifndef TOUCH_CONTROLS_H
#define TOUCH_CONTROLS_H

#ifdef __ANDROID__

#define TC_PAD_CX   0.135f
#define TC_PAD_CY   0.72f
#define TC_PAD_R    0.060f

/* The table's h values were authored at TC_DESIGN_ASPECT.  Past that aspect a
   pill's height follows its WIDTH, so the pill keeps one shape on any panel;
   the draw and the hit test must apply the same factor or the finger lands
   beside the pill it is drawn on.  Narrower panels clamp at the authored size
   so a target never shrinks under the finger. */
#define TC_DESIGN_ASPECT 2.2222f
static float TC_PillHalfH( float hh, float aspect ) {
	float f = aspect / TC_DESIGN_ASPECT;
	return hh * ( f < 1.0f ? 1.0f : f );
}

#define LCARS_ORANGE       1.00f, 0.60f, 0.00f    
#define LCARS_GOLD         1.00f, 0.667f, 0.00f  
#define LCARS_SUNFLOWER    1.00f, 0.80f, 0.60f   
#define LCARS_ICE          0.60f, 0.80f, 1.00f   
#define LCARS_BUTTERSCOTCH 1.00f, 0.60f, 0.40f   
#define LCARS_VIOLET       0.80f, 0.60f, 1.00f   
#define LCARS_MARS         1.00f, 0.133f, 0.00f  
#define LCARS_HOPBUSH      0.80f, 0.40f, 0.60f   
#define LCARS_BLUE         LCARS_ICE
#define LCARS_SALMON       LCARS_BUTTERSCOTCH
#define LCARS_LILAC        LCARS_VIOLET
#define LCARS_RED          LCARS_MARS

typedef struct {
	float cx, cy, hw, hh; 
	int   key;
	const char *label;
	float r, g, b;
	qboolean tap;         
	qboolean aim;         
} tcActionButton_t;

typedef struct {
	qboolean controllerConnected;
	qboolean moveActive;
	float    padCx, padCy;
	float    thumbX, thumbY;
	int      actionPressed;    
	qboolean actionFingerHeld; 
	int      lastTouchMs;      
	int      overlayStartMs;   
	float    deviceW, deviceH; 
} touchControlState_t;

static const tcActionButton_t tcActionButtons[] = {
	{ 0.845f, 0.775f, 0.062f, 0.050f, K_MOUSE1,             "FIRE",     LCARS_MARS,      qfalse, qtrue  },
	{ 0.945f, 0.640f, 0.042f, 0.034f, K_MOUSE2,             "ALT",      LCARS_ORANGE,    qfalse, qtrue  },
	{ 0.725f, 0.780f, 0.046f, 0.034f, K_MOUSE3,             "USE",      LCARS_GOLD,      qfalse, qfalse },
	{ 0.790f, 0.925f, 0.050f, 0.038f, K_SPACE,              "JUMP",     LCARS_ICE,       qfalse, qfalse },
	{ 0.665f, 0.925f, 0.054f, 0.034f, K_PAD0_B,             "CROUCH",   LCARS_VIOLET,    qfalse, qfalse },
	{ 0.950f, 0.520f, 0.050f, 0.032f, K_PAD0_RIGHTSHOULDER, "NEXT",     LCARS_SUNFLOWER, qtrue,  qfalse },
	{ 0.950f, 0.405f, 0.050f, 0.032f, K_PAD0_LEFTSHOULDER,  "PREV",     LCARS_BUTTERSCOTCH, qtrue, qfalse },
	{ 0.050f, 0.290f, 0.048f, 0.030f, K_PAD0_DPAD_UP,       "TRICORDER",LCARS_ICE,       qtrue,  qfalse },
	{ 0.050f, 0.180f, 0.048f, 0.030f, K_PAD0_BACK,          "VIEW",     LCARS_ICE,       qtrue,  qfalse },
	{ 0.115f, 0.470f, 0.062f, 0.034f, K_TAB,                "MISSION",  LCARS_HOPBUSH,   qfalse, qfalse },
};
#define TC_NUM_ACTION ( (int)( sizeof( tcActionButtons ) / sizeof( tcActionButtons[0] ) ) )
#define TC_NUM_RIGHT_ACTION 7

extern touchControlState_t touchControl;

#endif 
#endif 
