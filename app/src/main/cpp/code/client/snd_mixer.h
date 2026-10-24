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

#ifndef __SND_MIXER_H__
#define __SND_MIXER_H__

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

struct openal_internal_t;

/* the control plane of the mixer: OpenAL stays the output and the spatialiser, this module owns the
   policy -- bus assignment, the dialog sidechain, the loudness anchor and the per-bus instrumentation.  `s_duckEnable 0' makes every number a no-op, which is
   today's behaviour bit for bit. */

/* DIALOG and MOVIE are both protected (operator decision: cinematics carry lip-synced speech); COMBAT is folded into SFX and `s_combatvolume' is aliased
   onto the SFX trim; AMBIENT is the S_AddLoopingSound pool, which keeps the SFX trim and takes the SFX depth. */
typedef enum {
    SNDBUS_DIALOG = 0,
    SNDBUS_MOVIE,
    SNDBUS_MUSIC,
    SNDBUS_AMBIENT,
    SNDBUS_SFX,
    SNDBUS_UI,
    SNDBUS_COUNT
} sndbus_t;

void SndMix_Init(void);

/* The channel pool is created in S_OPENAL_Init; the mixer walks it once per
   frame for the detector and the summary row. */
void SndMix_BindPool(struct openal_internal_t *pPool);

int   SndMix_BusForChannel(int iEntChannel, qboolean bLoopSound);
float SndMix_CategoryGain(int bus);
/* everything the mixer contributes: the loudness anchor (1.0 unless
   `s_loudnessAnchor') times the duck envelope (1.0 unless `s_duckEnable') */
float SndMix_BusGain(int bus);
qboolean SndMix_Enabled(void);
qboolean SndMix_LimiterWanted(void);

/* Called once per frame from S_OPENAL_Update.  Sets the latches the caller
   already uses to re-gain its live channels. */
void SndMix_FrameAdvance(int iNowMs, float fFadeVolume, qboolean *pbLevelsChanged, qboolean *pbMusicVolumeChanged);

/* Envelope registration.  PCM is in hand only while a buffer is being
   uploaded, so the hook lives in openal_channel::set_sfx. */
void SndMix_RegisterPCM(
    sfx_t *pSfx, const byte *pPcm, int samples, int rate, int width, int channels, int bus);

/* Dialog preemption bookkeeping: the counter must read 0 under the mixer. */
void SndMix_NoteDialogDrop(void);
void SndMix_NoteDialogSteal(void);

/* MUSIC_UpdateDucking: the snapshot's `ps.allowMusicDucking' gates the music
   bus depth (it is true unless a script turns it off). */
void SndMix_SetMusicDucking(int iDucking);

void SndMix_SummaryRow(void);

#ifdef __cplusplus
}
#endif

#endif
