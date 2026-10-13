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
#ifndef EF2_ANDROID_DEFAULTS_H
#define EF2_ANDROID_DEFAULTS_H


/* s_dialogvolume 0.96 is deliberate: retail's shared 0.60 leaves dialogue 6.804 dB under
   the tutorial's authored effect, and this seta is what carries the change to an install
   whose config.cfg already archived the old value.  See snd_dma_new.cpp S_Init.

   Touch/gamepad feel = the EF1 port's answer key (VoyagerNX_Android
   android_defaults.cfg).  Its own derivation line names the target: full stick
   deflection ~245 deg/s yaw, ~180 deg/s pitch.  EF2's stick look is the key-turn
   path (cl_input.cpp CL_KeyState * cl_yawspeed/cl_pitchspeed, degrees per
   second), so those two cvars ARE the EF1 numbers.  sensitivity 5, m_pitch
   0.022, joy_threshold 0.15, cl_touchLook 2.2 and in_joystick 1 already equal
   EF1; in_joystickUseAnalog stays 0 because this engine has no analog consumer
   (cl.joystickAxis is written and read nowhere, and there is no JoystickMove) --
   EF1's 1 would route the movement stick into that dead axis array and the
   player would not move.

   Max graphics (operator order): pin the quality cvars at their top setting on
   every install.  r_subdivisions 3 -> 2 is the one real raise -- tr_bsp.c feeds
   the value straight in as the patch chord tolerance, so smaller is finer, and
   the twin's own Cvar_CheckRange floors it at 2.  r_lodbias 1 IS the maximum
   here, not 0: tr_model.cpp/tr_surface.c return every vertex once the value
   clears 0.5, so 0 would ENABLE the distance collapse, and
   r_lodScaleDistance/r_lodscale never run behind that early return.  r_picmip
   0 / r_texturebits 0 / r_colorbits 32 keep native mip, texture and colour
   depth (16 forces a downgrade in tr_image.c).  cg_marks/cg_shadows/
   cg_effectdetail are already the authored maximum.

   Advanced video: the page's rows are pinned at their maximums by explicit seta
   rows, because several of them are not born at their page maximum in
   renderervulkan/tr_init.c and an install's own config.cfg can outrun a compiled
   default.  Directions verified in this lane:
     r_flares 2 is the page's top option and the lane only tests the value, so 1
       and 2 draw identically.
     r_dynamiclightmaps 1 is DISPLAY ONLY -- renderervulkan declares it extern
       (tr_local.h) and neither defines nor reads it; without a row the checkbox
       reads an unregistered cvar and shows unchecked.
     r_allowExtensions 1 opens the page's master gate: the checkbox and the three
       sub-rows it gates (r_arb_multisample, r_ext_nv_multisample_filter,
       r_anisotropicFiltering) read unregistered cvars and grey out while it sits
       at 0.  Nothing in renderervulkan reads any of the four, so the pins change
       no pixel; they make the page read max.
     r_light_simple 0 is the maximum: tr_sphere_shade.cpp gates the RF_EXTRALIGHT
       path on !r_light_simple, so 1 SWITCHES IT OFF; unchecked is the rich path.
     r_light_simple_dist 5000 is the page's top option and the extralight radius
       compare (fDist <= value), so larger = the extra light reaches farther.
     r_lodCurveError 250 is the page's High; cg_effectdetail 1.0 High,
     cg_marks 1 and cg_shadows 1 are archive-proof pins of the maximums -- the
       compiled defaults equal them but an install's archived config can carry
       lower values, and the launcher's overrides only protect rows it names.
     cg_shadows Stencil (2) renders nothing here (the depth picker prefers
       D32_SFLOAT, stencilBits 0), so Simple IS this lane's maximum.
     r_swapInterval is deliberately absent: operator order leaves it alone. */
#define EF2_ANDROID_DEFAULT_COMMANDS \
	"bind UPARROW \"+forward\"\n" \
	"bind w \"+forward\"\n" \
	"bind DOWNARROW \"+back\"\n" \
	"bind s \"+back\"\n" \
	"bind a \"+moveleft\"\n" \
	"bind LEFTARROW \"+moveleft\"\n" \
	"bind d \"+moveright\"\n" \
	"bind RIGHTARROW \"+moveright\"\n" \
	"bind SPACE \"+moveup\"\n" \
	"bind c \"+movedown\"\n" \
	"bind ALT \"+strafe\"\n" \
	"bind SHIFT \"+speed\"\n" \
	"bind q \"+left\"\n" \
	"bind e \"+right\"\n" \
	"bind DEL \"+lookdown\"\n" \
	"bind PGDN \"+lookup\"\n" \
	"bind END \"centerview\"\n" \
	"bind f \"+leanleft\"\n" \
	"bind g \"+leanright\"\n" \
	"bind 1 \"warpinv 1\"\n" \
	"bind 2 \"warpinv 2\"\n" \
	"bind 3 \"warpinv 3\"\n" \
	"bind 4 \"warpinv 4\"\n" \
	"bind 5 \"warpinv 5\"\n" \
	"bind 6 \"warpinv 6\"\n" \
	"bind = \"+holster\"\n" \
	"bind r \"+droprune\"\n" \
	"bind n \"nightvision\"\n" \
	"bind MWHEELUP \"previnv\"\n" \
	"bind MWHEELDOWN \"nextinv\"\n" \
	"bind MOUSE1 \"+attackleft\"\n" \
	"bind MOUSE2 \"+attackright\"\n" \
	"bind CTRL \"+attackright\"\n" \
	"bind v \"+reload\"\n" \
	"bind MOUSE3 \"+use\"\n" \
	"bind x \"+use\"\n" \
	"bind PAUSE \"pause\"\n" \
	"bind ESCAPE \"togglemenu\"\n" \
	"bind ` \"toggleconsole\"\n" \
	"bind ~ \"toggleconsole\"\n" \
	"bind TAB \"+objectives_score\"\n" \
	"bind F1 \"vote y\"\n" \
	"bind F2 \"vote n\"\n" \
	"bind F3 \"taunt 1\"\n" \
	"bind F4 \"taunt 2\"\n" \
	"bind F6 \"ui_quicksave\"\n" \
	"bind F9 \"ui_quickload\"\n" \
	"bind F11 \"screenshot\"\n" \
	"bind enter \"messagemode\"\n" \
	"bind ' \"teammessagemode\"\n" \
	"bind PAD0_LEFTSTICK_UP \"+forward\"\n" \
	"bind PAD0_LEFTSTICK_DOWN \"+back\"\n" \
	"bind PAD0_LEFTSTICK_LEFT \"+moveleft\"\n" \
	"bind PAD0_LEFTSTICK_RIGHT \"+moveright\"\n" \
	"bind PAD0_RIGHTSTICK_UP \"+lookup\"\n" \
	"bind PAD0_RIGHTSTICK_DOWN \"+lookdown\"\n" \
	"bind PAD0_RIGHTSTICK_LEFT \"+left\"\n" \
	"bind PAD0_RIGHTSTICK_RIGHT \"+right\"\n" \
	"bind PAD0_RIGHTTRIGGER \"+attackleft\"\n" \
	"bind PAD0_LEFTTRIGGER \"+attackright\"\n" \
	"bind PAD0_RIGHTSHOULDER \"nextinv\"\n" \
	"bind PAD0_LEFTSHOULDER \"previnv\"\n" \
	"bind PAD0_A \"+moveup\"\n" \
	"bind PAD0_B \"+movedown\"\n" \
	"bind PAD0_X \"+use\"\n" \
	"bind PAD0_Y \"+reload\"\n" \
	"bind PAD0_DPAD_UP \"warpinv 6\"\n" \
	"bind PAD0_DPAD_DOWN \"+objectives_score\"\n" \
	"bind PAD0_DPAD_LEFT \"+leanleft\"\n" \
	"bind PAD0_DPAD_RIGHT \"+leanright\"\n" \
	"bind PAD0_LEFTSTICK_CLICK \"+speed\"\n" \
	"bind PAD0_RIGHTSTICK_CLICK \"+aim\"\n" \
	"bind PAD0_BACK \"nightvision\"\n" \
	"bind PAD0_START \"togglemenu\"\n" \
	"seta ui_dialogdisplaymode 1\n" \
	"seta ui_radarviewmode 0\n" \
	"seta rate 25000\n" \
	"seta m_aim 0\n" \
	"seta sensitivity 5.0\n" \
	"seta m_invert_pitch 0\n" \
	"seta m_pitch 0.022\n" \
	"seta m_filter 0\n" \
	"seta mp_autoSwitchWeapons 1\n" \
	"seta ui_autoswitchhudweapon 1\n" \
	"seta cl_run 1\n" \
	"seta s_playTaunts 1\n" \
	"seta com_printwarnings 0\n" \
	"seta cg_viewbob 1\n" \
	"seta cg_marks 1\n" \
	"seta cg_shadows 1\n" \
	"seta cg_effectdetail 1\n" \
	"seta cg_crosshair 1\n" \
	"seta cg_viewkick 1\n" \
	"seta cg_crosshairtexture \"textures/sprites/crosshair/crosshair_2.tga\"\n" \
	"set mp_itemRespawnMultiplier 1.0\n" \
	"set mp_weaponRespawnMultiplier 1.0\n" \
	"set mp_powerupRespawnMultiplier 1.0\n" \
	"set mp_damageMultiplier 1.0\n" \
	"set mp_knockbackMultiplier 1.0\n" \
	"set mp_respawnInvincibilityTime 4\n" \
	"set mp_lowBandwidth 0\n" \
	"set mp_warmUpTime 10\n" \
	"set mp_minPlayers 0\n" \
	"set sv_maxclients 1\n" \
	"seta cl_renderer \"vulkan\"\n" \
	"seta r_mode -2\n" \
	"seta r_fullscreen 1\n" \
	"seta r_swapInterval 0\n" \
	"seta com_maxfps 60\n" \
	"seta in_joystick 1\n" \
	"seta in_joystickUseAnalog 0\n" \
	"seta joy_threshold 0.15\n" \
	"seta cl_yawspeed 245\n" \
	"seta cl_pitchspeed 180\n" \
	"seta cl_touchLook 2.2\n" \
	"seta cl_touchScale 1.0\n" \
	"seta cl_touchOpacity 1.0\n" \
	"seta cl_crouchToggle 1\n" \
	"seta s_musicvolume 0.5\n" \
	"seta s_dialogvolume 0.96\n" \
	"seta ui_console 0\n" \
	"seta ui_minicon 0\n" \
	"seta r_smp 1\n" \
	"seta r_jobs -1\n" \
	"seta cl_jobs -1\n" \
	"seta r_vboStatic 0\n" \
	"seta r_picmip 0\n" \
	"seta r_lodbias 1\n" \
	"seta r_subdivisions 2\n" \
	"seta r_texturebits 0\n" \
	"seta r_colorbits 32\n" \
	"seta r_flares 2\n" \
	"seta r_dynamiclightmaps 1\n" \
	"seta r_allowExtensions 1\n" \
	"seta r_arb_multisample 1\n" \
	"seta r_ext_nv_multisample_filter 1\n" \
	"seta r_anisotropicFiltering 1\n" \
	"seta r_lodCurveError 250\n" \
	"seta r_light_simple 0\n" \
	"seta r_light_simple_dist 5000\n"
#define EF2_ANDROID_DEFAULT_BIND_COUNT 74

#endif 

#define EF2_ANDROID_DEFAULT_SETA_COUNT 64
