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

#include "snd_local.h"
#include "snd_mixer.h"
#include "snd_openal_new.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT(...)            Com_Printf(__VA_ARGS__)
#include "../qcommon/qa_probe.h"

#define SNDMIX_PI 3.14159265358979f

/* ------------------------------------------------------------------ cvars */

static cvar_t *s_duckEnable;
static cvar_t *s_duckAttack;
static cvar_t *s_duckHold;
static cvar_t *s_duckRelease;
static cvar_t *s_duckThreshold;
static cvar_t *s_duckDepthMusic;
static cvar_t *s_duckDepthSfx;
static cvar_t *s_duckDepthUi;
static cvar_t *s_limiter;
static cvar_t *s_loudnessAnchor;
static cvar_t *s_dialogTargetLufs;
static cvar_t *s_sfxTargetLufs;
static cvar_t *s_musicTargetLufs;
static cvar_t *qa_sndmix;

qboolean SndMix_Enabled(void)
{
    return (s_duckEnable && s_duckEnable->integer) ? qtrue : qfalse;
}

qboolean SndMix_LimiterWanted(void)
{
    return (s_limiter && s_limiter->integer) ? qtrue : qfalse;
}

/* ----------------------------------------------------------- envelope pool */

/* 10 ms blocks of K-shaped RMS, one row per sound that ever started on a protected bus, analysed once in set_sfx while the PCM is in hand.  The pool is
   bounded: a sound longer than the block cap keeps its first blocks, and past the entry cap later sounds fall back to the voice-level detector. */
#define SNDMIX_ENV_BLOCK_MS    10
#define SNDMIX_ENV_MAX_BLOCKS  600
#define SNDMIX_ENV_MAX_ENTRIES 128

typedef struct {
    sfx_t *pSfx;
    char   szName[64];
    float *pfBlock;
    int    iBlocks;
    float  fBlockSamples;
} sndmix_env_t;

static sndmix_env_t s_env[SNDMIX_ENV_MAX_ENTRIES];
static int          s_numEnv;

/* per-bus loudness census, filled by the same pass */
#define SNDMIX_CENSUS_MAX 128
static float    s_census[SNDBUS_COUNT][SNDMIX_CENSUS_MAX];
static int      s_numCensus[SNDBUS_COUNT];
static qboolean s_bCensusDirty = qfalse;
static float    s_fAnchor[SNDBUS_COUNT];

static struct openal_internal_t *s_pPool = NULL;

/* the retail plumbing MUSIC_UpdateDucking carries in from the snapshot
   (`ps.allowMusicDucking', true unless a script turns it off): the music bus
   ducks only while the game asks for it */
static int s_iMusicDucking = 0;

/* ------------------------------------------------------ detector + envelope */

static float    s_fDuckAmount = 0.0f;  // 0 = open, 1 = fully ducked
static qboolean s_bEngaged    = qfalse;
static int      s_iHoldMsLeft = 0;
static float    s_fDetector   = 0.0f;
static float    s_fFadeVolume = 1.0f;
static int      s_iLastMs     = -1;

static int   s_iDialogDrops;
static int   s_iDialogSteals;
static int   s_iPendingVoices;
static int   s_iSummaryMs;
static int   s_iBusCount[SNDBUS_COUNT];
static float s_fBusSquare[SNDBUS_COUNT];

static float DbToLinear(float fDb)
{
    return powf(10.0f, fDb / 20.0f);
}

/* Depth of a bus while the detector is fully engaged, in dB of attenuation.
   0 means the bus is never ducked, which is what DIALOG, MOVIE and (by
   default) UI carry. */
static float DuckDepthDb(int bus)
{
    switch (bus) {
    case SNDBUS_MUSIC:   return s_iMusicDucking ? s_duckDepthMusic->value : 0.0f;
    case SNDBUS_AMBIENT:
    case SNDBUS_SFX:     return s_duckDepthSfx->value;
    case SNDBUS_UI:      return s_duckDepthUi->value;
    default:             return 0.0f;
    }
}

/* ------------------------------------------------------- K-shape (approx) */

/* a two-stage approximation of the BS.1770 pre-filter -- a 1st-order high-pass near 40 Hz, then a +4 dB shelf above 1.5 kHz.  Not a certified loudness
   meter: the duck threshold and the anchor are ear-tuned cvars and the anchor is clamped to +-6 dB. */
typedef struct {
    float fB0, fB1, fB2, fA1, fA2;
    float fX1, fX2, fY1, fY2;
} sndmix_biquad_t;

static void Biquad_Reset(sndmix_biquad_t *pF)
{
    pF->fX1 = pF->fX2 = pF->fY1 = pF->fY2 = 0.0f;
}

static void Biquad_HighPass(sndmix_biquad_t *pF, float fRate, float fFreq)
{
    const float w0 = 2.0f * SNDMIX_PI * fFreq / fRate;
    const float cs = cosf(w0);
    const float alpha = sinf(w0) / 1.41421f;
    const float a0 = 1.0f + alpha;

    pF->fB0 = ((1.0f + cs) / 2.0f) / a0;
    pF->fB1 = (-(1.0f + cs)) / a0;
    pF->fB2 = ((1.0f + cs) / 2.0f) / a0;
    pF->fA1 = (-2.0f * cs) / a0;
    pF->fA2 = (1.0f - alpha) / a0;
    Biquad_Reset(pF);
}

static void Biquad_HighShelf(sndmix_biquad_t *pF, float fRate, float fFreq, float fGainDb)
{
    const float a = powf(10.0f, fGainDb / 40.0f);
    const float w0 = 2.0f * SNDMIX_PI * fFreq / fRate;
    const float cs = cosf(w0);
    const float sn = sinf(w0);
    const float sq = 2.0f * sqrtf(a) * sn;
    const float a0 = (a + 1.0f) - (a - 1.0f) * cs + sq;

    pF->fB0 = (a * ((a + 1.0f) + (a - 1.0f) * cs + sq)) / a0;
    pF->fB1 = (-2.0f * a * ((a - 1.0f) + (a + 1.0f) * cs)) / a0;
    pF->fB2 = (a * ((a + 1.0f) + (a - 1.0f) * cs - sq)) / a0;
    pF->fA1 = (2.0f * ((a - 1.0f) - (a + 1.0f) * cs)) / a0;
    pF->fA2 = ((a + 1.0f) - (a - 1.0f) * cs - sq) / a0;
    Biquad_Reset(pF);
}

static float Biquad_Step(sndmix_biquad_t *pF, float x)
{
    const float y = pF->fB0 * x + pF->fB1 * pF->fX1 + pF->fB2 * pF->fX2 - pF->fA1 * pF->fY1 - pF->fA2 * pF->fY2;

    pF->fX2 = pF->fX1;
    pF->fX1 = x;
    pF->fY2 = pF->fY1;
    pF->fY1 = y;
    return y;
}

/* ------------------------------------------------------------ env lookups */

static sndmix_env_t *Env_Find(const sfx_t *pSfx)
{
    int i;

    for (i = 0; i < s_numEnv; i++) {
        if (s_env[i].pSfx == pSfx && !strcmp(s_env[i].szName, pSfx->name)) {
            return &s_env[i];
        }
    }

    return NULL;
}

/* Speech activity of one dialog voice at its current playhead.  The blocks are
   normalised by the sound's own loudest block at registration, so the detector
   does not move with the player's volume sliders.  1.0 when no envelope is
   known, which keeps the duck engaged for the whole voice: the fallback can
   only over-duck. */
static float Env_NormAt(const sndmix_env_t *pEnv, unsigned int iOffset)
{
    int iBlock;

    if (!pEnv || !pEnv->iBlocks || !pEnv->pfBlock) {
        return 1.0f;
    }

    if (pEnv->fBlockSamples < 1.0f) {
        return 1.0f;
    }

    iBlock = (int)((float)iOffset / pEnv->fBlockSamples);
    if (iBlock >= pEnv->iBlocks) {
        iBlock = pEnv->iBlocks - 1;
    }

    return pEnv->pfBlock[iBlock];
}

void SndMix_RegisterPCM(sfx_t *pSfx, const byte *pPcm, int samples, int rate, int width, int channels, int bus)
{
    sndmix_biquad_t hp, shelf;
    sndmix_env_t   *pEnv;
    float           fPeak;
    float           fSumSq;
    float           fIntSq;
    long            lCount;
    long            lTotal;
    int             iBlock;
    int             iBlockSamples;
    int             iStride;
    int             i;

    if (!SndMix_Enabled() || !pSfx || !pPcm || samples <= 0 || rate <= 0 || width <= 0 || channels <= 0) {
        return;
    }
    if (bus < 0 || bus >= SNDBUS_COUNT) {
        return;
    }

    pEnv = Env_Find(pSfx);
    if (!pEnv) {
        if (s_numEnv >= SNDMIX_ENV_MAX_ENTRIES) {
            return;
        }
        pEnv = &s_env[s_numEnv++];
        memset(pEnv, 0, sizeof(*pEnv));
        pEnv->pSfx = pSfx;
        Q_strncpyz(pEnv->szName, pSfx->name, sizeof(pEnv->szName));
    }

    if (!pEnv->pfBlock) {
        pEnv->pfBlock = (float *)Z_TagMalloc(SNDMIX_ENV_MAX_BLOCKS * sizeof(float), TAG_SOUND);
        if (!pEnv->pfBlock) {
            return;
        }
    }

    iStride       = width * channels;
    iBlockSamples = (int)((float)rate * (SNDMIX_ENV_BLOCK_MS / 1000.0f));
    if (iBlockSamples < 1) {
        iBlockSamples = 1;
    }

    Biquad_HighPass(&hp, (float)rate, 40.0f);
    Biquad_HighShelf(&shelf, (float)rate, 1500.0f, 4.0f);

    fPeak   = 1e-6f;
    iBlock  = 0;
    lCount  = 0;
    lTotal  = 0;
    fSumSq  = 0.0f;
    fIntSq  = 0.0f;
    for (i = 0; i + iStride <= samples; i += iStride) {
        float fShape;
        float fS;

        if (width == 2) {
            fS = (float)(short)((unsigned)pPcm[i] | ((unsigned)pPcm[i + 1] << 8)) / 32768.0f;
        } else {
            fS = ((float)pPcm[i] - 128.0f) / 128.0f;
        }

        fShape = Biquad_Step(&shelf, Biquad_Step(&hp, fS));
        fSumSq += fShape * fShape;
        lCount++;

        if (lCount >= iBlockSamples) {
            const float fRms = sqrtf(fSumSq / (float)lCount);

            if (iBlock < SNDMIX_ENV_MAX_BLOCKS) {
                pEnv->pfBlock[iBlock] = fRms;
                iBlock++;
            }
            if (fRms > fPeak) {
                fPeak = fRms;
            }
            fIntSq += fSumSq;
            lTotal += lCount;
            lCount = 0;
            fSumSq = 0.0f;
        }
    }

    if (iBlock <= 0) {
        iBlock = 1;
        pEnv->pfBlock[0] = 0.0f;
    }

    pEnv->fBlockSamples = (float)iBlockSamples;

    /* Normalise in place: the detector reads activity, not level. */
    for (i = 0; i < iBlock; i++) {
        pEnv->pfBlock[i] /= fPeak;
    }

    /* the integrated value the loudness census ranks sounds by */
    if (lTotal > 0 && s_numCensus[bus] < SNDMIX_CENSUS_MAX) {
        s_census[bus][s_numCensus[bus]++] = sqrtf(fIntSq / (float)lTotal) / fPeak;
    }

    s_bCensusDirty = qtrue;
}

/* --------------------------------------------------------------- bus utils */

int SndMix_BusForChannel(int iEntChannel, qboolean bLoopSound)
{
    if (bLoopSound) {
        return SNDBUS_AMBIENT;
    }

    switch (iEntChannel) {
    case CHAN_MENU:
    case CHAN_MENU2:            return SNDBUS_UI;
    case CHAN_MUSIC:            return SNDBUS_MUSIC;
    case CHAN_DIALOG:
    case CHAN_DIALOG_SECONDARY: return SNDBUS_DIALOG;
    case CHAN_CINEMATIC:        return SNDBUS_MOVIE;
    default:                    return SNDBUS_SFX;
    }
}

/* The cvar trim of a bus: exactly the multipliers S_EF2_ChannelVolume applied
   before the mixer existed.  COMBAT reads the SFX trim -- the two cvars are
   aliased in FrameAdvance, so nothing a config says is silently dropped. */
float SndMix_CategoryGain(int bus)
{
    switch (bus) {
    case SNDBUS_MUSIC:          return s_musicVolume->value;
    case SNDBUS_DIALOG:
    case SNDBUS_MOVIE:          return s_dialogVolume->value;
    case SNDBUS_AMBIENT:
    case SNDBUS_SFX:            return s_sfxVolume->value;
    default:                    return 1.0f;
    }
}

/* ---------------------------------------------------------- loudness anchor */

static float Anchor_Median(int bus)
{
    float a[SNDMIX_CENSUS_MAX];
    int   i;
    int   j;

    if (s_numCensus[bus] <= 0) {
        return 0.0f;
    }

    memcpy(a, s_census[bus], s_numCensus[bus] * sizeof(float));
    for (i = 1; i < s_numCensus[bus]; i++) {
        const float fKey = a[i];
        for (j = i - 1; j >= 0 && a[j] > fKey; j--) {
            a[j + 1] = a[j];
        }
        a[j + 1] = fKey;
    }

    return a[s_numCensus[bus] / 2];
}

static void Anchor_Recompute(void)
{
    int bus;

    for (bus = 0; bus < SNDBUS_COUNT; bus++) {
        float fTargetDb;
        float fMedianDb;
        float fDeltaDb;

        s_fAnchor[bus] = 1.0f;
        if (s_numCensus[bus] < 4) {
            continue;
        }

        switch (bus) {
        case SNDBUS_MUSIC: fTargetDb = s_musicTargetLufs->value; break;
        case SNDBUS_DIALOG:
        case SNDBUS_MOVIE: fTargetDb = s_dialogTargetLufs->value; break;
        default:           fTargetDb = s_sfxTargetLufs->value; break;
        }

        fMedianDb = 20.0f * log10f(Anchor_Median(bus) + 1e-9f);
        fDeltaDb  = fTargetDb - fMedianDb;
        if (fDeltaDb > 6.0f) {
            fDeltaDb = 6.0f;
        } else if (fDeltaDb < -6.0f) {
            fDeltaDb = -6.0f;
        }
        s_fAnchor[bus] = DbToLinear(fDeltaDb);
    }

    s_bCensusDirty = qfalse;
}

float SndMix_BusGain(int bus)
{
    float fGain = 1.0f;

    if (s_loudnessAnchor->integer) {
        if (s_bCensusDirty) {
            Anchor_Recompute();
        }
        fGain *= s_fAnchor[bus];
    }

    if (s_duckEnable->integer) {
        fGain *= 1.0f - s_fDuckAmount * (1.0f - DbToLinear(-DuckDepthDb(bus)));
    }

    return fGain;
}

/* ------------------------------------------------------------------ frames */

/* `s_combatvolume' is an alias of the SFX trim: either name moves both, so the
   combat fold costs no knob and no archived value. */
static void Alias_CombatVolume(void)
{
    if (s_combatVolume->modified) {
        s_combatVolume->modified = qfalse;
        Cvar_SetValue("s_sfxvolume", s_combatVolume->value);
        s_sfxVolume->modified = qfalse;
    } else if (s_sfxVolume->modified) {
        s_sfxVolume->modified = qfalse;
        Cvar_SetValue("s_combatvolume", s_sfxVolume->value);
        s_combatVolume->modified = qfalse;
    }
}

void SndMix_FrameAdvance(int iNowMs, float fFadeVolume, qboolean *pbLevelsChanged, qboolean *pbMusicVolumeChanged)
{
    int   i;
    int   iDeltaMs;
    float fDetector = 0.0f;
    float fPrev;
    float fAmount;
    float fAttackMs;
    float fReleaseMs;

    Alias_CombatVolume();

    if (s_iLastMs < 0) {
        s_iLastMs = iNowMs;
    }
    iDeltaMs = iNowMs - s_iLastMs;
    s_iLastMs = iNowMs;
    if (iDeltaMs < 0) {
        iDeltaMs = 0;
    } else if (iDeltaMs > 250) {
        iDeltaMs = 250;
    }

    if (!s_duckEnable->integer) {
        if (s_fDuckAmount != 0.0f) {
            s_fDuckAmount = 0.0f;
            s_bEngaged    = qfalse;
            s_iHoldMsLeft = 0;
            *pbLevelsChanged = qtrue;
        }
        return;
    }

    s_fFadeVolume = fFadeVolume;

    memset(s_iBusCount, 0, sizeof(s_iBusCount));
    memset(s_fBusSquare, 0, sizeof(s_fBusSquare));
    s_iPendingVoices = 0;

    /* --- the detector reads the 2D dialog pool only: the movie track is
           protected and is not a competitor of itself. */
    if (s_pPool) {
        for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
            openal_channel *pChannel = s_pPool->channel[i];
            int             bus;
            float           fGain;

            if (!pChannel) {
                continue;
            }
            if (!(pChannel->is_playing() || pChannel->bPendingStart)) {
                continue;
            }
            if (pChannel->iFlags & CHANNEL_FLAG_PAUSED) {
                continue;
            }

            /* r54: armed but never started is the silent-slot signature -- a
               0.5 s UI click cannot be alive at two 1 Hz samples, so a voice
               that shows up here without playing is the flush that never came. */
            if (pChannel->bPendingStart && !pChannel->is_playing()) {
                s_iPendingVoices++;
            }

            bus = SndMix_BusForChannel(
                pChannel->iEntChannel,
                (pChannel->iFlags & CHANNEL_FLAG_LOOP_SOUND) ? qtrue : qfalse);
            s_iBusCount[bus]++;

            /* the gain the voice actually carries: the same product
               S_EF2_ChannelVolume builds, times the per-voice trims, raised to
               the port's perceptual curve */
            fGain = pChannel->fVolume * s_fFadeVolume * SndMix_CategoryGain(bus) * SndMix_BusGain(bus);
            fGain = powf(fGain * pChannel->fObstruction * pChannel->fDistGain, 5.0f / 3.0f);
            s_fBusSquare[bus] += fGain * fGain;

            if (bus == SNDBUS_DIALOG && pChannel->pSfx && pChannel->is_playing()) {
                fDetector += Env_NormAt(Env_Find(pChannel->pSfx), pChannel->sample_offset()) * pChannel->fVolume;
            }
        }

        /* the song pool, the triggered music and the movie stream sit outside
           MAX_SOUNDSYSTEM_POSITION_CHANNELS but do carry a bus */
        for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
            if (s_pPool->chan_song[i].is_playing() || s_pPool->chan_song[i].bPendingStart) {
                s_iBusCount[SNDBUS_MUSIC]++;
            }
        }
        if (s_pPool->chan_trig_music.is_playing() || s_pPool->chan_trig_music.bPendingStart) {
            s_iBusCount[SNDBUS_MUSIC]++;
        }
        if (s_pPool->chan_movie.is_playing() || s_pPool->chan_movie.bPendingStart) {
            s_iBusCount[SNDBUS_MOVIE]++;
        }
    }

    /* --- the envelope */
    if (fDetector >= s_duckThreshold->value) {
        s_bEngaged    = qtrue;
        s_iHoldMsLeft = (int)s_duckHold->value;
    } else {
        s_iHoldMsLeft -= iDeltaMs;
        if (s_iHoldMsLeft <= 0) {
            s_iHoldMsLeft = 0;
            s_bEngaged = qfalse;
        }
    }

    fAttackMs  = s_duckAttack->value;
    fReleaseMs = s_duckRelease->value;
    if (fAttackMs < 1.0f) {
        fAttackMs = 1.0f;
    }
    if (fReleaseMs < 1.0f) {
        fReleaseMs = 1.0f;
    }

    fPrev   = s_fDuckAmount;
    fAmount = s_fDuckAmount;
    if (s_bEngaged) {
        /* the dip is reached in `s_duckAttack': a 12 ms ramp lands inside the
           ~20 ms device period, so the duck is audible at the next buffer
           without stepping the whole mix in one frame */
        fAmount += (float)iDeltaMs / fAttackMs;
        if (fAmount > 1.0f) {
            fAmount = 1.0f;
        }
    } else if (fAmount > 0.0f) {
        /* exponential in the *attenuation*, so the mix breathes back instead of
           stepping: -60 dB of the remaining dip per `s_duckRelease' */
        fAmount *= powf(0.001f, (float)iDeltaMs / fReleaseMs);
        if (fAmount < 1e-4f) {
            fAmount = 0.0f;
        }
    }
    s_fDuckAmount = fAmount;
    s_fDetector   = fDetector;

    if (fAmount != fPrev) {
        *pbLevelsChanged      = qtrue;
        *pbMusicVolumeChanged = qtrue;
    }

    s_iSummaryMs += iDeltaMs;
    if (s_iSummaryMs >= 1000) {
        s_iSummaryMs = 0;
        SndMix_SummaryRow();
    }
}

void SndMix_NoteDialogDrop(void)
{
    s_iDialogDrops++;
}

void SndMix_NoteDialogSteal(void)
{
    s_iDialogSteals++;
}

/* ------------------------------------------------------------- instruments */

void SndMix_SummaryRow(void)
{
    static int s_iRows;

    if (!PROBE_On() || !qa_sndmix->integer) {
        return;
    }

    if (s_iRows < PROBE_ROW_CAP) {
        s_iRows++;
        PROBE_PRINT(
            "[SND:mix] bus(dlg=%d mov=%d mus=%d amb=%d sfx=%d ui=%d)"
            " det=%.3f thr=%.3f duck=%.1f%% rmsdB(dlg=%.1f sfx=%.1f mus=%.1f)"
            " drops=%d steal=%d pend=%d anchor=%s limiter=%s\n",
            s_iBusCount[SNDBUS_DIALOG], s_iBusCount[SNDBUS_MOVIE], s_iBusCount[SNDBUS_MUSIC],
            s_iBusCount[SNDBUS_AMBIENT], s_iBusCount[SNDBUS_SFX], s_iBusCount[SNDBUS_UI],
            s_fDetector, s_duckThreshold->value, s_fDuckAmount * 100.0f,
            10.0f * log10f(s_fBusSquare[SNDBUS_DIALOG] + 1e-9f),
            10.0f * log10f(s_fBusSquare[SNDBUS_SFX] + 1e-9f),
            10.0f * log10f(s_fBusSquare[SNDBUS_MUSIC] + 1e-9f),
            s_iDialogDrops, s_iDialogSteals, s_iPendingVoices,
            s_loudnessAnchor->integer ? "on" : "off",
            s_limiter->integer ? "on" : "off");
    } else if (s_iRows == PROBE_ROW_CAP) {
        s_iRows++;
        PROBE_PRINT("[SND:mix] capped at %d rows; `qa_sndmix 0' silences\n", PROBE_ROW_CAP);
    }
}

/* -------------------------------------------------------------------- init */

void SndMix_Init(void)
{
    s_duckEnable       = Cvar_Get("s_duckEnable", "1", CVAR_ARCHIVE);
    s_duckAttack       = Cvar_Get("s_duckAttack", "12", CVAR_ARCHIVE);
    s_duckHold         = Cvar_Get("s_duckHold", "120", CVAR_ARCHIVE);
    s_duckRelease      = Cvar_Get("s_duckRelease", "350", CVAR_ARCHIVE);
    s_duckThreshold    = Cvar_Get("s_duckThreshold", "0.25", CVAR_ARCHIVE);
    s_duckDepthMusic   = Cvar_Get("s_duckDepthMusic", "6.0", CVAR_ARCHIVE);
    s_duckDepthSfx     = Cvar_Get("s_duckDepthSfx", "3.0", CVAR_ARCHIVE);
    s_duckDepthUi      = Cvar_Get("s_duckDepthUi", "0", CVAR_ARCHIVE);
    s_limiter          = Cvar_Get("s_limiter", "1", CVAR_ARCHIVE);
    s_loudnessAnchor   = Cvar_Get("s_loudnessAnchor", "0", CVAR_ARCHIVE);
    s_dialogTargetLufs = Cvar_Get("s_dialogTargetLufs", "-23", CVAR_ARCHIVE);
    s_sfxTargetLufs    = Cvar_Get("s_sfxTargetLufs", "-18", CVAR_ARCHIVE);
    s_musicTargetLufs  = Cvar_Get("s_musicTargetLufs", "-20", CVAR_ARCHIVE);
    qa_sndmix          = Cvar_Get("qa_sndmix", "1", 0);

    memset(s_env, 0, sizeof(s_env));
    s_numEnv      = 0;
    s_fDuckAmount = 0.0f;
    s_bEngaged    = qfalse;
    s_iHoldMsLeft = 0;
    s_iLastMs     = -1;

    memset(s_numCensus, 0, sizeof(s_numCensus));
    s_bCensusDirty = qfalse;
    s_iMusicDucking = 0;
    s_iDialogDrops = 0;
    s_iDialogSteals = 0;
    s_iPendingVoices = 0;

    Alias_CombatVolume();
}

void SndMix_BindPool(struct openal_internal_t *pPool)
{
    s_pPool = pPool;
}

void SndMix_SetMusicDucking(int iDucking)
{
    s_iMusicDucking = iDucking;
}
