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

/* ---------------- the pad ---------------- */

/* pad is draw-only: sdl_input.c takes the move finger by the left-half rule, never a rect */
#define TC_PAD_CX   0.135f
#define TC_PAD_CY   0.72f
#define TC_PAD_R    0.105f

/* one constant, so a lit arm and a queued direction cannot disagree */
#define TC_PAD_DEADZONE 0.035f

/* ---------------- aspect ---------------- */

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

/* x radius that makes the cap a true semicircle: pillhh / aspect */
/* draw-side only: the hit rect already contains the cap */
static float Q_UNUSED_VAR TC_BarCapR( float hh, float aspect ) {
	return TC_PillHalfH( hh, aspect ) / aspect;
}

/* touch-target floor: 48dp ~= 9mm (m3.material.io).  Reference 6.5in 1080x2400 -> hw 0.0664, hh 0.0299; every bar below clears both.  Advisory. */
#define TC_MIN_HW 0.070f
#define TC_MIN_HH 0.030f

/* comm head panel's rect lives in the retail pak; keep out of x<0.36, y<0.27 */
#define TC_RESERVED_X 0.360f
#define TC_RESERVED_Y 0.270f

/* canonical LCARS stops: thelcars.com/colors.php + trekcolors lcars_colors(); hex inline.  Master's eight are kept byte-for-byte. */
#define LCARS_ORANGE       1.00f, 0.60f, 0.00f     /* atomic-tangerine #FF9900 */
#define LCARS_GOLD         1.00f, 0.667f, 0.00f    /* near harvestgold #FFAA00 */
#define LCARS_SUNFLOWER    1.00f, 0.80f, 0.60f     /* tanoi            #FFCC99 */
#define LCARS_ICE          0.60f, 0.80f, 1.00f     /* anakiwa          #99CCFF */
#define LCARS_BUTTERSCOTCH 1.00f, 0.60f, 0.40f     /* orange-peel      #FF9966 */
#define LCARS_VIOLET       0.80f, 0.60f, 1.00f     /* near lilac       #CC99FF */
#define LCARS_MARS         1.00f, 0.133f, 0.00f    /* mars             #FF2200 */
#define LCARS_HOPBUSH      0.80f, 0.40f, 0.60f     /* hopbush          #CC6699 */

#define LCARS_BLUE         LCARS_ICE
#define LCARS_SALMON       LCARS_BUTTERSCOTCH
#define LCARS_LILAC        LCARS_VIOLET
#define LCARS_RED          LCARS_MARS

/* added for the LCARS block work */
#define LCARS_TANGERINE    1.00f, 0.533f, 0.20f    /* tangerine        #FF8833 */

typedef struct {
	float cx, cy, hw, hh;  /* centre and half-extents, cx/hw x-normalised, cy/hh y-normalised */
	int   key;
	const char *label;
	float r, g, b;
	qboolean tap;          /* qtrue: send the key down and straight back up (a click) */
	qboolean aim;          /* qtrue: this bar also drags the view while held */
	qboolean capLeft;      /* qtrue: LCARS cap on the bar's left, flat cut on its right */
} tcActionButton_t;

/* decorative spine: not a table row, so no key and no hit rect */
#define TC_SPINE_CX 0.026f
#define TC_SPINE_CY 0.392f
#define TC_SPINE_HW 0.020f
#define TC_SPINE_HH 0.102f

typedef struct {
	qboolean controllerConnected;
	qboolean moveActive;
	float    padCx, padCy;
	float    thumbX, thumbY;
	int      actionPressed;    /* bit per tcActionButtons row, set while its finger is down */
	qboolean actionFingerHeld; /* any pill finger is down */
	int      lastTouchMs;      /* last touch of any kind, for the idle fade */
	int      overlayStartMs;
	float    deviceW, deviceH; /* the panel's real pixel size, for the aspect factor */
} touchControlState_t;

/* a pill's key field is a KEY, not a command: sdl_input.c queues it as an SE_KEY and the binding executor runs whatever that key's bind carries, so each
   row rides the keyboard's own bind and must follow it when the pad rows move.  TRICORDER rides '6' = warpinv 6, retail's own weapon-select row for the inventory group. */

/* rows 0..6 = right cluster, 7..9 = left block (TC_NUM_RIGHT_ACTION) */
static const tcActionButton_t tcActionButtons[] = {
	/* bottom-right attack row: USE butts FIRE, the caps face outward */
	{ 0.870f, 0.770f, 0.082f, 0.044f, K_MOUSE1,             "FIRE",     LCARS_MARS,      qfalse, qtrue,  qfalse },
	{ 0.700f, 0.770f, 0.072f, 0.032f, K_MOUSE3,             "USE",      LCARS_GOLD,      qfalse, qfalse, qtrue  },
	/* bottom-right movement row */
	{ 0.800f, 0.900f, 0.072f, 0.032f, K_SPACE,              "JUMP",     LCARS_ICE,       qfalse, qfalse, qfalse },
	{ 0.640f, 0.900f, 0.072f, 0.032f, K_PAD0_B,             "CROUCH",   LCARS_VIOLET,    qfalse, qfalse, qtrue  },
	/* right-edge LCARS column: caps left, so the flat cut lands on the screen edge */
	{ 0.928f, 0.375f, 0.072f, 0.030f, K_PAD0_LEFTSHOULDER,  "PREV",     LCARS_BUTTERSCOTCH, qtrue, qfalse, qtrue  },
	{ 0.928f, 0.447f, 0.072f, 0.030f, K_PAD0_RIGHTSHOULDER, "NEXT",     LCARS_SUNFLOWER, qtrue,  qfalse, qtrue  },
	{ 0.928f, 0.550f, 0.072f, 0.034f, K_MOUSE2,             "ALT",      LCARS_ORANGE,    qfalse, qtrue,  qtrue  },
	/* left LCARS block: caps face right, away from the spine */
	{ 0.130f, 0.320f, 0.070f, 0.030f, '6',                  "TRICORDER",LCARS_ICE,       qtrue,  qfalse, qfalse },
	{ 0.130f, 0.392f, 0.070f, 0.030f, 'n',                  "NIGHT",    LCARS_ICE,       qtrue,  qfalse, qfalse },
	{ 0.130f, 0.464f, 0.074f, 0.030f, K_TAB,                "MISSION",  LCARS_HOPBUSH,   qfalse, qfalse, qfalse },
};
#define TC_NUM_ACTION ( (int)( sizeof( tcActionButtons ) / sizeof( tcActionButtons[0] ) ) )
#define TC_NUM_RIGHT_ACTION 7

extern touchControlState_t touchControl;

#endif /* __ANDROID__ */
#endif /* TOUCH_CONTROLS_H */
