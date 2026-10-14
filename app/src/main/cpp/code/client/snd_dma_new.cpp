/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "snd_local.h"
#include "snd_openal_new.h"
#include "snd_mixer.h"
#include "client.h"
#include "cl_ui.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

// the success and the "not initializing." arms, exactly where retail emits it.
#define S_SOUND_SEPARATOR "------------------------------------\n"

typedef struct {
    vec3_t origin;
    vec3_t velocity;
    sfx_t *sfx;
    int    mergeFrame;
} loopSound_t;

qboolean s_bLastInitSound  = qfalse;
qboolean s_bSoundStarted   = qfalse;
qboolean s_bSoundPaused    = qfalse;
qboolean s_bTryUnpause     = qfalse;
int      s_iListenerNumber = 0;
float    s_fAmbientVolume  = 1.f;

int number_of_sfx_infos = 0;

cvar_t *s_volume;
cvar_t *s_loadas8bit;
cvar_t *s_khz;
cvar_t *s_show;
cvar_t *s_musicVolume;
cvar_t *s_dialogVolume;
cvar_t *s_sfxVolume;
cvar_t *s_combatVolume;

sfx_info_t sfx_infos[MAX_SFX_INFOS];

sfx_t s_knownSfx[MAX_SFX];
int   s_numSfx;

s_entity_t s_entity[MAX_GENTITIES];

static int      s_registrationSequence;
static qboolean s_inRegistration;

int         numLoopSounds;
loopSound_t loopSounds[MAX_LOOP_SOUNDS];

void S_SoundInfo_f()
{
    S_PrintInfo();
}

void S_SoundDump_f()
{
    S_DumpInfo();
}

void S_Init(qboolean full_startup)
{
    cvar_t *cv;

    SMP_LOAD_BEGIN( sndinit );

    Com_Printf("\n------- sound initialization -------\n");

    s_volume         = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE);
    s_musicVolume    = Cvar_Get("s_musicvolume", "0.60", CVAR_ARCHIVE);
    /* 0.96 == s_sfxvolume x the loudest paired effect (0.60 x 1.6), and that holds
       under either gain law; 0.96 x 1.0 cannot clip.  See S_EF2_GainLaw. */
    s_dialogVolume   = Cvar_Get("s_dialogvolume", "0.96", CVAR_ARCHIVE);
    s_sfxVolume      = Cvar_Get("s_sfxvolume", "0.60", CVAR_ARCHIVE);
    s_combatVolume   = Cvar_Get("s_combatvolume", "0.60", CVAR_ARCHIVE);
    s_khz            = Cvar_Get("s_khz", "44", CVAR_ARCHIVE | CVAR_SOUND_LATCH);
    s_loadas8bit     = Cvar_Get("s_loadas8bit", "0", CVAR_ARCHIVE | CVAR_LATCH);
    s_show           = Cvar_Get("s_show", "0", CVAR_CHEAT);

    /* the mixer's cvars are registered here so they exist before the driver
       opens its context: `s_limiter' is read at context creation, and a QUIET
       boot with the mixer off must still find every knob. */
    SndMix_Init();

    s_bLastInitSound = false;

    cv = Cvar_Get("s_initsound", "1", 0);

    if (cv->integer) {
        Cmd_AddCommand("play", S_Play);
        Cmd_AddCommand("soundtrack", S_SoundTrack_f);
        Cmd_AddCommand("soundlist", S_SoundList);
        Cmd_AddCommand("soundinfo", S_SoundInfo_f);
        Cmd_AddCommand("sounddump", S_SoundDump_f);

        if (S_Driver_Init()) {
            s_bLastInitSound = true;
            s_bSoundStarted  = true;

            if (full_startup) {
                s_numSfx = 0;
                S_StopAllSounds(true);
            }

            S_FindName("***DEFAULT***", -1);
            S_PrintInfo();
        }

        Com_Printf(S_SOUND_SEPARATOR);
    } else {
        Com_Printf("not initializing.\n");
        Com_Printf(S_SOUND_SEPARATOR);
        s_bSoundStarted = 0;
    }
    SMP_LOAD_END( sndinit );
}

void S_Shutdown(qboolean full_shutdown)
{
    int    i;
    sfx_t *sfx;

    if (!s_bSoundStarted) {
        return;
    }

    S_Driver_Shutdown();
    s_bSoundStarted = false;

    Cmd_RemoveCommand("play");
    Cmd_RemoveCommand("soundtrack");
    Cmd_RemoveCommand("music");
    Cmd_RemoveCommand("stopsound");
    Cmd_RemoveCommand("soundlist");
    Cmd_RemoveCommand("soundinfo");
    Cmd_RemoveCommand("sounddump");

    if (full_shutdown) {
        for (i = 0; i < s_numSfx; i++) {
            sfx = &s_knownSfx[i];

            if (sfx->name[0]) {
                if (sfx->data) {
                    Z_Free(sfx->data);
                }

                *sfx = {};
            }
        }

        s_numSfx = 0;
    }

}

qboolean S_NameExists(const char *name)
{
    int i;

    if (strlen(name) >= MAX_RES_NAME) {
        Com_DPrintf("Sound name too long: %s", name);
        return qfalse;
    }

    for (i = 0; i < s_numSfx; i++) {
        if (!strcmp(s_knownSfx[i].name, name)) {
            return qtrue;
        }
    }

    return qfalse;
}

sfx_t *S_FindName(const char *name, int sequenceNumber)
{
    int    i;
    sfx_t *sfx;

    if (!name) {
        Com_WDPrintf("S_FindName: NULL\n");
        return NULL;
    }

    if (!name[0]) {
        Com_WDPrintf("S_FindName: empty name\n");
        return NULL;
    }

    if (strlen(name) >= MAX_RES_NAME) {
        Com_WDPrintf("Sound name too long: %s", name);
        return NULL;
    }

    for (i = 0; i < s_numSfx; i++) {
        sfx = &s_knownSfx[i];

        if (!strcmp(sfx->name, name)) {
            if (sfx->registration_sequence != -1) {
                sfx->registration_sequence = sequenceNumber;
            }

            return sfx;
        }
    }

    sfx = &s_knownSfx[0];

    for (i = 0; i < s_numSfx; i++) {
        sfx = &s_knownSfx[i];
        if (!sfx->name[0]) {
            break;
        }
    }

    if (i == s_numSfx) {
        if (i == MAX_SFX) {
            Com_Error(ERR_FATAL, "S_FindName: out of sfx_t");
        }

        sfx = &s_knownSfx[s_numSfx];
        s_numSfx++;
    }

    *sfx = {};
    Q_strncpyz(sfx->name, name, sizeof(sfx->name));
    sfx->registration_sequence = sequenceNumber;

    return sfx;
}

void S_BeginRegistration()
{
    SMP_LOAD_BEGIN( sndinit );
    s_inRegistration = true;

    s_registrationSequence++;
    s_knownSfx[0].registration_sequence = s_registrationSequence;

    S_StopAllSounds(true);
    SMP_LOAD_END( sndinit );
}

void S_DefaultSound(sfx_t *sfx)
{
    sfx->data = NULL;
    sfx->iFlags |= SFX_FLAG_DEFAULT_SOUND;
}

qboolean S_IsSoundRegistered(const char *name)
{
    char szCacheName[MAX_QPATH];
    int  i;

    if (!s_bSoundStarted) {
        return qfalse;
    }

    for (i = 0; name[i] && i < MAX_QPATH; i++) {
        szCacheName[i] = tolower(name[i]);
    }
    szCacheName[i] = 0;

    if (i >= MAX_QPATH) {
        Com_Printf("Sound name exceeds MAX_QPATH\n");
        return qfalse;
    }

    return S_NameExists(szCacheName);
}

sfxHandle_t S_RegisterSound(const char *name)
{
    return S_RegisterSoundExt(name, qfalse, qfalse);
}

sfxHandle_t S_RegisterSoundExt(const char *name, int streamed, qboolean force_load)
{
    char   szCacheName[MAX_QPATH];
    sfx_t *sfx;
    int    i;

    if (!s_bSoundStarted) {
        return 0;
    }

    if (!name) {
        return 0;
    }

    if (strlen(name) >= MAX_QPATH) {
        Com_WPrintf("Sound name exceeds MAX_QPATH\n");
        return 0;
    }

    Com_LocalizeFilePath(name, szCacheName);

    sfx = S_FindName(szCacheName, s_registrationSequence);
    if (!sfx) {
        return 0;
    }

    if (!sfx->data) {
        if (!S_LoadSound(sfx->name, sfx, streamed, force_load)) {
            Com_WDPrintf("Couldn't load sound: %s\n", sfx->name);
            sfx->iFlags |= SFX_FLAG_DEFAULT_SOUND;
            return 0;
        }

        sfx->iFlags &= ~SFX_FLAG_DEFAULT_SOUND;
        sfx->sfx_info_index = 0;

        for (i = 0; i < number_of_sfx_infos; i++) {
            if (!Q_stricmp(sfx->name, sfx_infos[i].name)) {
                sfx->sfx_info_index = i;
                break;
            }
        }

        load_lip_info(szCacheName);
    }

    return sfx - s_knownSfx;
}

float S_GetSoundTime(sfxHandle_t handle)
{
    return s_knownSfx[handle].time_length;
}

void S_EndRegistration()
{
    int    i;
    sfx_t *sfx;

    SMP_LOAD_BEGIN( sndinit );

    s_inRegistration = qfalse;

    for (i = 0; i < s_numSfx; i++) {
        sfx = &s_knownSfx[i];

        if (!sfx->name[0]) {
            continue;
        }

        if (sfx->registration_sequence && sfx->registration_sequence != s_registrationSequence) {
            if (sfx->data) {
                Z_Free(sfx->data);
            }

            *sfx = {};
        }
    }
    SMP_LOAD_END( sndinit );
}

void S_UpdateEntity(int entityNum, const vec3_t origin, const vec3_t vel, qboolean use_listener)
{
    if (entityNum > MAX_GENTITIES) {
        Com_Error(ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum);
    }

    VectorCopy(origin, s_entity[entityNum].position);
    VectorCopy(vel, s_entity[entityNum].velocity);
    s_entity[entityNum].use_listener = use_listener;
    s_entity[entityNum].time         = cl.serverTime;
}

void S_SetGlobalAmbientVolumeLevel(float volume)
{
    s_fAmbientVolume = volume;
}

void S_StartSound(
    const vec3_t origin,
    int          entnum,
    int          entchannel,
    sfxHandle_t  sfxHandle,
    float        volume,
    float        min_dist,
    float        pitch
)
{
    S_StartSoundExt(origin, entnum, entchannel, sfxHandle, volume, min_dist, pitch, -1.0f, qfalse);
}

void S_StartSoundExt(
    const vec3_t origin,
    int          entnum,
    int          entchannel,
    sfxHandle_t  sfxHandle,
    float        volume,
    float        min_dist,
    float        pitch,
    float        maxDist,
    int          streamed
)
{
    if (!s_bSoundStarted) {
        return;
    }

    if (sfxHandle < 0 || sfxHandle >= s_numSfx) {
        Com_Error(ERR_DROP, "S_MILES_StartSound: handle %i out of range", sfxHandle);
    }

    if (!sfxHandle) {
        return;
    }

    S_Driver_StartSound(origin, entnum, entchannel, sfxHandle, volume, min_dist, pitch, maxDist, streamed);
}

void S_StartLocalSound(const char *sound_name, qboolean force_load)
{
    sfxHandle_t sfxHandle;
    const char *name;

    if (!s_bSoundStarted) {
        return;
    }

    if (!sound_name) {
        return;
    }

    // both applied playback parms retail never had and dereferenced the still-NULL
    name = Alias_FindRandom(sound_name);
    if (!name) {
        name = sound_name;
    }

    sfxHandle = S_RegisterSoundExt(name, qfalse, force_load);

    S_StartSoundExt(NULL, s_iListenerNumber, CHAN_MENU, sfxHandle, -1.0, -1.0, 1.0, -1.0, qfalse);
}

void S_StartLocalSoundChannel(const char *sound_name, qboolean force_load, soundChannel_t channel)
{
    sfxHandle_t sfxHandle;
    const char *name;

    if (!s_bSoundStarted) {
        return;
    }

    if (!sound_name) {
        return;
    }

    name = Alias_FindRandom(sound_name);
    if (!name) {
        name = sound_name;
    }

    sfxHandle = S_RegisterSoundExt(name, qfalse, force_load);

    /* r54: the UI sound census.  handle 0 is a silent load failure (retail
       warns through Com_WDPrintf, which a quiet boot never prints) and a
       non-CHAN_MENU channel arms instead of starting, so this row plus
       [SND:menuflush] is the whole chain a --diag boot needs. */
    PROBE_EMIT_N("[SND:", "uisound", 64,
                 "name='%s' chan=%d handle=%d %s\n",
                 name ? name : "(null)", (int)channel, (int)sfxHandle,
                 sfxHandle ? "ok" : "LOAD-FAILED");

    S_StartSoundExt(NULL, s_iListenerNumber, channel, sfxHandle, -1.0, -1.0, 1.0, -1.0, qfalse);
}

void S_StopAllSounds(qboolean stop_music)
{
    if (!s_bSoundStarted) {
        return;
    }

    Com_Printf("------- S_StopAllSounds (%s) -------\n", stop_music ? "stop music" : "don't stop music");

    S_Driver_StopAllSounds(stop_music);

    Com_Printf("------- S_StopAllSounds Complete-------\n");
}

void S_ClearLoopingSounds()
{
    if (!s_bSoundStarted) {
        return;
    }

    S_Driver_ClearLoopingSounds();
}

void S_AddLoopingSound(
    const vec3_t origin,
    const vec3_t velocity,
    sfxHandle_t  sfxHandle,
    float        volume,
    float        min_dist,
    int          entityNum,
    int          unused7,
    int          unused8
)
{
    if (!s_bSoundStarted) {
        return;
    }

    if (sfxHandle < 0 || sfxHandle >= s_numSfx) {
        Com_Error(ERR_DROP, "S_AddLoopingSound: handle %i out of range", sfxHandle);
    }

    if (!sfxHandle) {
        return;
    }

    S_Driver_AddLoopingSound(origin, velocity, sfxHandle, volume, min_dist, entityNum, unused7, unused8);
}

void S_Respatialize(int entityNum, const vec3_t head, vec3_t axis[3])
{
    if (!s_bSoundStarted) {
        return;
    }

    S_Driver_Respatialize(entityNum, head, axis);
}

void S_Update()
{
    if (!s_bSoundStarted) {
        return;
    }

    S_Driver_Update();
}

void S_StopSound(int entnum, int channel)
{
    if (!s_bSoundStarted) {
        return;
    }

    S_Driver_StopSound(entnum, channel);
}

void S_SetReverb(int reverb_type, float reverb_level)
{
    if (!s_bSoundStarted) {
        return;
    }

    S_Driver_SetReverb(reverb_type, reverb_level);
}

void S_Play()
{
    int  i;
    char name[MAX_QPATH];

    if (Cmd_Argc() < 2) {
        return;
    }

    for (i = 1; i < Cmd_Argc(); i++) {
        if (strrchr(Cmd_Argv(i), '.')) {
            Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
        } else {
            Com_sprintf(name, sizeof(name), "%s.wav", Cmd_Argv(1));
        }

        S_StartLocalSound(name, qfalse);
    }
}

void S_SoundTrack_f(void)
{
    if (Cmd_Argc() != 2) {
        return;
    }

    MUSIC_NewSoundtrack(Cmd_Argv(1));
}

void S_SoundList()
{
    int    i;
    sfx_t *sfx;
    int    size;
    int    total = 0;

    for (i = 0; i < s_numSfx; i++) {
        sfx = &s_knownSfx[i];

        if (sfx->registration_sequence) {
            size = sfx->length * sfx->width;
            total += size;
            Com_Printf("(%2db) %6i : %s\n", sfx->width * 8, size, sfx->name);
        }
    }

    Com_Printf("Total resident: %i\n", total);
}

/* Deliberately empty.  Not a retail entry point (absent from the 5,241-function
   Mac engine corpus): it is Q3-era FS bookkeeping (files.cpp) from when the mixer
   owned a device buffer a file op could swap under.  Every sample here is already
   in an AL buffer and no source reads a file handle, so there is nothing to flush. */
void S_ClearSoundBuffer()
{
}

const char* S_GetMusicFilename()
{
    return S_Driver_GetMusicFilename();
}

int S_GetMusicLoopCount()
{
    return S_Driver_GetMusicLoopCount();
}

unsigned int S_GetMusicOffset()
{
    return S_Driver_GetMusicOffset();
}
