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
	"seta joy_threshold 0.0\n" \
	"seta in_joystick 0\n" \
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
	"seta cl_renderer \"gles3\"\n" \
	"seta r_mode -2\n" \
	"seta r_fullscreen 1\n" \
	"seta r_swapInterval 1\n" \
	"seta com_maxfps 85\n" \
	"seta in_joystick 1\n" \
	"seta in_joystickUseAnalog 0\n" \
	"seta joy_threshold 0.15\n" \
	"seta cl_yawspeed 220\n" \
	"seta cl_pitchspeed 170\n" \
	"seta cl_touchLook 2.2\n" \
	"seta cl_touchScale 1.0\n" \
	"seta cl_touchOpacity 1.0\n" \
	"seta cl_crouchToggle 1\n" \
	"seta s_musicvolume 0.5\n" \
	"seta ui_console 0\n" \
	"seta ui_minicon 0\n"
#define EF2_ANDROID_DEFAULT_BIND_COUNT 74

#endif 

#define EF2_ANDROID_DEFAULT_SETA_COUNT 49
