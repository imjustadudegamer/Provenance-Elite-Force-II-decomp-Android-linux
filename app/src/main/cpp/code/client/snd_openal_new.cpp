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
#include "../server/server.h"
#include "snd_codec.h"
#include "../qcommon/cm_public.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

typedef struct {
    const char *funcname;
    void      **funcptr;
    bool        required;
} extensions_table_t;

#define MAX_MUSIC_SONGS 16

static int   s_iNextLoopingWarning = 0;
static int   s_iReverbType         = 0;
static float s_fReverbLevel        = 0;
static bool  s_bReverbChanged      = false;
static bool  s_bFading             = false;
static float s_fFadeVolume         = 1.f;
static float s_fVelocityScale      = 1.0f / (52.49f * 500.0f * 52.49f);

static constexpr float s_fVolumeGain = 1.f; 

cvar_t *s_milesdriver;
cvar_t *s_openaldevice;
cvar_t *s_reverb;
cvar_t *s_show_cpu;
cvar_t *s_show_num_active_sounds;
cvar_t *s_show_sounds;
cvar_t *s_show_loopsounds;
cvar_t *s_speaker_type;
cvar_t *s_checkObstructions; 
cvar_t *s_openaldriver;
cvar_t *s_alAvailableDevices;

/* r54: retail arms a non-CHAN_MENU start (S_MILES_StartSound @0x001F1700, slot[1]=1,
   no AIL_start) and starts it only in S_MILES_Respatialize @0x001F2CC0, which the
   port reaches only from cgame (cg_view.c:861) -- so a UI click, which retail plays
   on CHAN_MENU2 (UIButtonBase::Released @0x00050710) as a .wav, armed and never
   started at the main menu.  0 restores that. */
cvar_t *s_menuFlush;

static const float s_reverbTable[26] = {
    0.5f,   0.25f,        0.417f, 0.653f,      0.208f,      0.5f,   0.403f, 0.5f,   0.5f,
    0.361f, 0.5f,         0.153f, 0.361f,      0.44400001f, 0.25f,  0.111f, 0.111f, 0.19400001f,
    1.0f,   0.097000003f, 0.208f, 0.65200001f, 1.0f,        0.875f, 0.139f, 0.486f,
};

static vec3_t       vec_zero                   = {0, 0, 0};
int                 s_iNumMilesAudioProviders  = 0;
bool                s_bProvidersEmunerated     = false;
static bool         al_initialized             = false;
static bool         s_bReverbWanted            = false;
static bool         s_bReverbCapable           = true;
static float        al_current_volume          = 0;
static unsigned int al_frequency               = 22050;
static ALCcontext  *al_context_id              = NULL;
static ALCdevice   *al_device                  = NULL;
static ALsizei      al_default_resampler_index = 0;
static ALsizei      al_resampler_index         = 0;

static int s_iNumChannels3D = MAX_SOUNDSYSTEM_CHANNELS_3D;
static int S_OPENAL_MaxSupportedSamples();

static ALboolean (*_alutLoadMP3_LOKI)(unsigned int buffer, const byte *data, int length) Q_UNUSED_VAR;

static qboolean music_active            = qfalse;
int             music_current_mood      = 0;
int             music_fallback_mood     = 0;
float           old_music_volume        = 1.f;
float           music_volume            = 1.f;
float           new_music_volume        = 1.f;
float           music_volume_fade_time  = 0;
long int        music_volume_start_time = 0;
int             music_volume_direction  = 0;
int             music_volume_changed    = 0;
int             music_loaded            = 0;
int             music_numsongs          = 0;
int             music_currentsong       = 0;

static qboolean enumeration_ext     = qfalse;
static qboolean enumeration_all_ext = qfalse;

static qboolean ima4_ext         = qfalse;
static qboolean soft_block_align = qfalse;

song_t            music_songs[MAX_MUSIC_SONGS];
openal_internal_t openal;
static float      s_fFadeStartTime;
static float      s_fFadeStopTime;
static char       current_soundtrack[128];

/* The pre-r52 duck state is gone: `s_bRetailDuckDriver' was initialised false
   and never written again, so `s_bDucking' could never become true and
   `s_fDuckVolume' never moved off 1.0 -- the whole block in S_OPENAL_Update was
   unreachable.  Ducking now lives in snd_mixer.cpp behind `s_duckEnable'. */

static void S_OPENAL_PlayMP3();
static void S_OPENAL_StopMP3();
static int
S_OPENAL_SpatializeStereoSound(const vec3_t listener_origin, const vec3_t listener_left, const vec3_t origin);
static void   S_OPENAL_reverb(int iChannel, int iReverbType, float fReverbLevel);
static bool   S_OPENAL_LoadMP3_Codec(const char *_path, sfx_t *pSfx, byte **ppPcm, snd_info_t *pInfo);
static ALuint S_OPENAL_Format(float width, int channels);

#define alDieIfError() __alDieIfError(__FILE__, __LINE__)

#if defined(_WIN64)
#    define ALDRIVER_DEFAULT "OpenAL64.dll"
#elif defined(_WIN32)
#    define ALDRIVER_DEFAULT "OpenAL32.dll"
#elif defined(__OpenBSD__)
#    define ALDRIVER_DEFAULT "libopenal.so"
#elif defined(__ANDROID__)
#    define ALDRIVER_DEFAULT "libopenal.so"
#else
#    define ALDRIVER_DEFAULT "libopenal.so.1"
#endif

#ifdef AL_SOFT_source_resampler
LPALGETSTRINGISOFT qalGetStringiSOFT;
#endif

static void __alDieIfError(const char *file, int line)
{
    ALint alErr = qalGetError();
    if (alErr) {
        if (s_show_sounds->integer) {
            Com_DPrintf("OpenAL error, %s, line %i: [%s].\n", file, line, qalGetString(alErr));
        }
    }
}

static void S_OPENAL_NukeSource(ALuint *srcptr)
{
    ALuint source;

    source = *srcptr;
    if (!*srcptr) {
        return;
    }

    if (!qalIsSource(*srcptr)) {
        return;
    }

    alDieIfError();
    qalSourceStop(source);

    alDieIfError();
    qalSourcei(source, AL_BUFFER, 0);

    alDieIfError();
    qalDeleteSources(1, srcptr);

    alDieIfError();
    *srcptr = 0;
}

static void S_OPENAL_NukeBuffer(ALuint *bufptr)
{
    if (!*bufptr) {
        return;
    }

    if (!qalIsBuffer(*bufptr)) {
        return;
    }

    alDieIfError();
    qalDeleteBuffers(1, bufptr);

    alDieIfError();
    *bufptr = 0;
}

static void S_OPENAL_NukeChannel(openal_channel *channel)
{
    if (!channel) {
        return;
    }

    S_OPENAL_NukeSource(&channel->source);
    S_OPENAL_NukeBuffer(&channel->buffer);

    if (channel->bufferdata) {
        delete[] channel->bufferdata;
    }
}

static void S_OPENAL_NukeContext()
{
    int i;

    Com_Printf("OpenAL: Destroying channels...\n");

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS; ++i) {
        S_OPENAL_NukeChannel(openal.channel[i]);
    }

    Com_Printf("OpenAL: Channels destroyed successfully.\n");

    for (i = 0; i < s_numSfx; i++) {
        S_OPENAL_NukeBuffer(&s_knownSfx[i].buffer);
    }

    S_OPENAL_NukeBuffer(&openal.movieSFX.buffer);

    if (al_context_id) {
        Com_Printf("OpenAL: Destroying context...\n");

        qalcSuspendContext(al_context_id);
        qalcMakeContextCurrent(NULL);
        qalcDestroyContext(al_context_id);
        al_context_id = NULL;

        Com_Printf("OpenAL: Context destroyed successfully.\n");
    }

    if (al_device) {
        Com_Printf("OpenAL: Closing device...\n");

        qalcCloseDevice(al_device);
        al_device = NULL;

        Com_Printf("OpenAL: Device closed successfully.\n");
    }
}

static bool S_OPENAL_InitContext()
{
    const char *dev;
    int         attrlist[12];

    Com_DPrintf("OpenAL: Context initialization\n");

    dev = NULL;
    if (s_openaldevice) {
        dev = s_openaldevice->string;
    }

    if (dev && !*dev) {
        dev = NULL;
    }

    enumeration_all_ext = qalcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT");
    enumeration_ext     = qalcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");

    if (enumeration_ext || enumeration_all_ext) {
        char        devicenames[16384] = "";
        const char *devicelist;
#ifdef _WIN32
        const char *defaultdevice;
#endif
        int curlen;

        if (enumeration_all_ext) {
            devicelist = qalcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
#ifdef _WIN32
            defaultdevice = qalcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
#endif
        } else {
            devicelist = qalcGetString(NULL, ALC_DEVICE_SPECIFIER);
#ifdef _WIN32
            defaultdevice = qalcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
#endif
            enumeration_ext = qtrue;
        }

#ifdef _WIN32
        Com_DPrintf("OpenAL: Default playback device: \"%s\"\n", defaultdevice);
#endif

#ifdef _WIN32
        if (!dev && defaultdevice && !strcmp(defaultdevice, "Generic Hardware")) {
            dev = "Generic Software";
        }
#endif


        if (devicelist) {
            while ((curlen = strlen(devicelist))) {
                Q_strcat(devicenames, sizeof(devicenames), devicelist);
                Q_strcat(devicenames, sizeof(devicenames), "\n");

                devicelist += curlen + 1;
            }
        }

        Com_Printf("OpenAL: List of available devices:\n%s\n", devicenames);

        s_alAvailableDevices = Cvar_Get("s_alAvailableDevices", devicenames, CVAR_ROM | CVAR_NORESTART);
    }

    Com_Printf("OpenAL: Opening device \"%s\"...\n", dev ? dev : "{default}");

    al_device = qalcOpenDevice(dev);
    if (!al_device && dev) {
        Com_Printf("Failed to open OpenAL device '%s', trying default.\n", dev);
        al_device = qalcOpenDevice(NULL);
    }

    if (!al_device) {
        Com_Printf("OpenAL: Could not open device\n");
        S_OPENAL_NukeContext();
        return false;
    }

    Com_Printf("OpenAL: Device opened successfully.\n");
    switch (s_khz->integer) {
    case 11:
        al_frequency = 11025;
        break;
    default:
    case 22:
        al_frequency = 22050;
        break;
    case 44:
        al_frequency = 44100;
        break;
    }

    attrlist[0] = ALC_FREQUENCY;
    attrlist[1] = al_frequency;
    attrlist[2] = ALC_SYNC;
    attrlist[3] = qfalse;

    s_speaker_type->modified = false;

#ifdef ALC_SOFT_output_mode
    attrlist[4] = ALC_OUTPUT_MODE_SOFT;
    attrlist[6] = ALC_HRTF_SOFT;
    attrlist[7] = ALC_FALSE;

    switch (s_speaker_type->integer) {
    default:
    case 0:
        attrlist[5] = ALC_STEREO_SOFT;
        break;
    case 1:
        attrlist[5] = ALC_STEREO_HRTF_SOFT;
        attrlist[7] = ALC_DONT_CARE_SOFT;
        break;
    case 2:
        attrlist[5] = ALC_SURROUND_5_1_SOFT;
        break;
    case 3:
        attrlist[5] = ALC_QUAD_SOFT;
        break;
    case 4:
        attrlist[5] = ALC_SURROUND_5_1_SOFT;
        break;
    case 5:
        attrlist[5] = ALC_SURROUND_7_1_SOFT;
        break;
    }
#else
#    pragma message("OpenAL: ALC_OUTPUT_MODE_SOFT unavailable")

    Com_Printf(
        "OpenAL: ALC_OUTPUT_MODE_SOFT is unavailable. The speaker type will be ignored, fallback to normal stereo.\n"
    );
    attrlist[4] = 0;
    attrlist[5] = 0;
#endif

#ifdef ALC_SOFT_output_limiter
    /* `s_limiter 1' turns on the device brickwall the old code asked for and
       then refused (Ratio=inf, AutoPostGain, AutoDeclip, 1 ms look-ahead;
       alc/alc.cpp CreateDeviceLimiter).  Its ceiling is the output format's full
       scale less the dither headroom, not a tuned -1 dBTP: that would need
       master pre-gain, which moves the balance.  This stops clipping; the mixer
       does the shaping. */
    attrlist[8]  = ALC_OUTPUT_LIMITER_SOFT;
    attrlist[9]  = (SndMix_LimiterWanted() && SndMix_Enabled()) ? ALC_TRUE : ALC_FALSE;
#endif
    attrlist[10] = 0;
    attrlist[11] = 0;

    Com_Printf("OpenAL: Creating AL context...\n");
    al_context_id = qalcCreateContext(al_device, attrlist);
    if (!al_context_id) {
        Com_Printf("OpenAL: Could not create context\n");
        S_OPENAL_NukeContext();
        return false;
    }

    Com_Printf("OpenAL: Context created successfully.\n");

    qalcMakeContextCurrent(al_context_id);
    alDieIfError();

    Com_Printf("AL_VENDOR: %s\n", qalGetString(AL_VENDOR));
    alDieIfError();

    Com_Printf("AL_VERSION: %s\n", qalGetString(AL_VERSION));
    alDieIfError();

    Com_Printf("AL_RENDERER: %s\n", qalGetString(AL_RENDERER));
    alDieIfError();

    Com_Printf("AL_EXTENSIONS: %s\n", qalGetString(AL_EXTENSIONS));
    alDieIfError();

    qalDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    alDieIfError();

    qalDopplerFactor(0.0f);
    alDieIfError();

    qalcProcessContext(al_context_id);
    alDieIfError();

    return true;
}

static bool S_OPENAL_InitExtensions()
{
    extensions_table_t extensions_table[] = {
#ifdef AL_SOFT_source_resampler
        extensions_table_t {
                            "alGetStringiSOFT", (void **)&qalGetStringiSOFT,
                            false, },
#endif
        extensions_table_t {NULL, NULL, false}
    };

    extensions_table_t *i;

    for (i = extensions_table; i->funcname; ++i) {
        *i->funcptr = NULL;
    }

    for (i = extensions_table; i->funcname; ++i) {
        Com_Printf("AL extension: Looking up %ssymbol \"%s\"...", i->required ? "required " : "", i->funcname);

        *i->funcptr = qalGetProcAddress(i->funcname);
        if (!*i->funcptr) {
            Com_Printf("...not found! %d [%s]\n", qalGetError(), qalGetString(qalGetError()));
            if (i->required) {
                S_OPENAL_NukeContext();
                return false;
            }
            continue;
        }

        Com_Printf("...found.\n");
    }

    ima4_ext         = qalIsExtensionPresent("AL_EXT_IMA4");
    soft_block_align = qalIsExtensionPresent("AL_SOFT_block_alignment");

    qalGetError();
    return true;
}

static bool S_OPENAL_InitChannel(int idx, openal_channel *chan)
{
    openal.channel[idx] = chan;
    VectorClear(chan->vOrigin);
    chan->fObstruction   = 1.0; 
    chan->fDistGain      = 1.0; 
    chan->fVolume         = 1.0;
    chan->fNewPitchMult   = 0.0;
    chan->fMinDist        = 0.0;
    chan->fMaxDist        = 0.0;
    chan->pSfx            = 0;
    chan->bPendingStart   = qfalse;
    chan->iEntNum         = 0;
    chan->iEntChannel     = 0;
    chan->iBaseRate       = 0;
    chan->iStartTime      = 0;
    chan->iTime           = 0;
    chan->iEndTime        = 0;
    chan->iFlags          = 0;
    chan->iPausedOffset   = 0;
    chan->source          = 0;
    chan->buffer          = 0;
    chan->bufferdata      = 0;
    chan->fading          = FADE_NONE;
    chan->fade_time       = 0;
    chan->fade_start_time = 0;
    chan->song_number     = 0;
    chan->song_in_use     = qfalse;
    chan->song_looping    = qfalse;

    qalGenSources(1, &chan->source);
    alDieIfError();
    qalSourcei(chan->source, AL_SOURCE_RELATIVE, true);
    alDieIfError();

#ifdef AL_SOFT_source_resampler
    qalSourcei(chan->source, AL_SOURCE_RESAMPLER_SOFT, al_resampler_index);
    alDieIfError();
#endif

    return true;
}

static int S_OPENAL_MaxSupportedSamples()
{
    ALint iMono = 0;

    if (al_device) {
        qalcGetIntegerv(al_device, ALC_MONO_SOURCES, 1, &iMono);
    }

    if (iMono <= 0) {
        iMono = MAX_SOUNDSYSTEM_CHANNELS_3D;
    }

    return iMono;
}

qboolean S_OPENAL_Init()
{
    int i;

    if (al_initialized) {
        Com_DPrintf("S_OPENAL_Init(): Called when sound is already initialized!\n");
        return qtrue;
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS; i++) {
        openal.channel[i] = NULL;
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
        openal.loop_sounds[i] = {};
    }

    s_milesdriver            = Cvar_Get("s_milesdriver", "auto", CVAR_SOUND_LATCH | CVAR_ARCHIVE);
    s_openaldevice           = Cvar_Get("s_openaldevice", "", CVAR_SOUND_LATCH);
    s_reverb                 = Cvar_Get("s_reverb", "0", CVAR_SOUND_LATCH | CVAR_ARCHIVE);
    s_show_cpu               = Cvar_Get("s_show_cpu", "0", 0);
    s_show_num_active_sounds = Cvar_Get("s_show_num_active_sounds", "0", 0);
    s_show_sounds            = Cvar_Get("s_show_sounds", "0", 0);
    s_show_loopsounds        = Cvar_Get("s_show_loopsounds", "0", 0);
    s_speaker_type           = Cvar_Get("s_speaker_type", "0", CVAR_ARCHIVE);
    s_checkObstructions      = Cvar_Get("s_checkObstructions", "1", CVAR_ARCHIVE);
    s_menuFlush              = Cvar_Get("s_menuFlush", "1", 0);
    s_openaldriver = Cvar_Get("s_openaldriver", ALDRIVER_DEFAULT, CVAR_LATCH | CVAR_PROTECTED);

    if (!QAL_Init(s_openaldriver->string)) {
        Com_Printf("Failed to load library: \"%s\".\n", s_openaldriver->string);
        if (!Q_stricmp(s_openaldriver->string, ALDRIVER_DEFAULT) || !QAL_Init(ALDRIVER_DEFAULT)) {
            return qfalse;
        }
    }

    if (!Cvar_Get("s_initsound", "1", 0)->integer) {
        Com_Printf("OpenAL: s_initsound set to zero...disabling audio.\n");
        return true;
    }

    if (!S_OPENAL_InitContext()) {
        Com_Printf("OpenAL: initialization failed. No audio will play.\n");
        return false;
    }

    if (!S_OPENAL_InitExtensions()) {
        Com_Printf("OpenAL: A required extension is missing. No audio will play.\n");
        return false;
    }

    s_bReverbWanted = (s_reverb->integer == 1);
    s_reverb->modified = false;

    al_current_volume = 1.0f;
    qalListenerf(AL_GAIN, al_current_volume);
    alDieIfError();

#ifdef AL_SOFT_source_resampler
    if (qalGetStringiSOFT) {
        size_t numResamplers;
        size_t resamplerIdx;

        al_default_resampler_index = qalGetInteger(AL_DEFAULT_RESAMPLER_SOFT);
        alDieIfError();
        al_resampler_index = al_default_resampler_index;
        numResamplers      = qalGetInteger(AL_NUM_RESAMPLERS_SOFT);
        alDieIfError();

        for (resamplerIdx = 0; resamplerIdx < numResamplers; resamplerIdx++) {
            const ALchar *resamplerName = qalGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, resamplerIdx);
            if (Q_stristr(resamplerName, "spline")) {
                Com_Printf("OpenAL: Using %s as the resampler.\n", resamplerName);
                al_resampler_index = resamplerIdx;
                break;
            }
        }
    }
#endif

    {
        int iProviderMax = S_OPENAL_MaxSupportedSamples();

        s_iNumChannels3D = iProviderMax;
        if (s_iNumChannels3D > MAX_SOUNDSYSTEM_CHANNELS_3D) {
            s_iNumChannels3D = MAX_SOUNDSYSTEM_CHANNELS_3D;
        }

        if (iProviderMax < 32) {
            s_iNumChannels3D = 32;
        }
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_3D; i++) {
        if (!S_OPENAL_InitChannel(i, &openal.chan_3D[i])) {
            return false;
        }
    }
    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
        if (!S_OPENAL_InitChannel(i + MAX_SOUNDSYSTEM_CHANNELS_3D, &openal.chan_2D[i])) {
            return false;
        }
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D_STREAM; i++) {
        if (!S_OPENAL_InitChannel(
                i + MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D, &openal.chan_2D_stream[i]
            )) {
            return false;
        }
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        if (!S_OPENAL_InitChannel(
                i + MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D + MAX_SOUNDSYSTEM_CHANNELS_2D_STREAM,
                &openal.chan_song[i]
            )) {
            return false;
        }
    }

    if (!S_OPENAL_InitChannel(SOUNDSYSTEM_CHANNEL_MP3_ID, &openal.chan_mp3)) {
        return false;
    }

    if (!S_OPENAL_InitChannel(SOUNDSYSTEM_CHANNEL_TRIGGER_MUSIC_ID, &openal.chan_trig_music)) {
        return false;
    }

    if (!S_OPENAL_InitChannel(SOUNDSYSTEM_CHANNEL_MOVIE_ID, &openal.chan_movie)) {
        return false;
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        openal.chan_song[i].set_no_virtualization();
    }
    openal.chan_mp3.set_no_virtualization();
    openal.chan_trig_music.set_no_virtualization();
    openal.chan_movie.set_no_virtualization();

    SndMix_BindPool(&openal);

    Cmd_AddCommand("playmp3", S_OPENAL_PlayMP3);
    Cmd_AddCommand("stopmp3", S_OPENAL_StopMP3);
    Cmd_AddCommand("loadsoundtrack", S_loadsoundtrack);
    Cmd_AddCommand("playsong", S_PlaySong);

    S_OPENAL_ClearLoopingSounds();
    load_sfx_info();
    s_bProvidersEmunerated = true;
    al_initialized         = true;

    S_CodecInit();

    return true;
}

void S_OPENAL_Shutdown()
{
    if (!al_initialized) {
        Com_DPrintf("S_OPENAL_Shutdown(): Called when sound is NOT initialized!\n");
        return;
    }

    S_OPENAL_StopAllSounds(true);

    Cmd_RemoveCommand("playmp3");
    Cmd_RemoveCommand("stopmp3");
    Cmd_RemoveCommand("loadsoundtrack");
    Cmd_RemoveCommand("playsong");

    S_OPENAL_NukeContext();

    s_bProvidersEmunerated = false;
    al_initialized         = false;

    QAL_Shutdown();
}

void S_FadeSound(float fTime)
{
    if (fTime > 0) {
        s_bFading        = true;
        s_fFadeStartTime = cls.realtime;
        s_fFadeVolume    = 1;
        s_fFadeStopTime  = cls.realtime + fTime;
    } else {
        s_fFadeVolume = 1;
        s_bFading     = false;
        music_volume_changed = true;
    }
}

float S_GetBaseVolume()
{
    return s_volume->value;
}

/* The one place the mixer's gains land, in the pre-mixer order: master x
   category x fade x what the mixer adds.  With `s_duckEnable 0' and
   `s_loudnessAnchor 0' SndMix_BusGain is exactly 1.0, so this builds
   `((s_volume x category) x fade) x 1.0' -- today's product, bit for bit.
   CHAN_MENU still returns the master alone, with no fade and no category. */
float S_EF2_ChannelVolume(int iEntChannel)
{
    int   bus;
    float fVolume;

    if (iEntChannel == CHAN_MENU || iEntChannel == CHAN_MENU2) {
        return s_volume->value;
    }

    bus = SndMix_BusForChannel(iEntChannel, qfalse);

    fVolume = s_volume->value * SndMix_CategoryGain(bus);
    fVolume *= s_fFadeVolume;
    fVolume *= SndMix_BusGain(bus);

    return fVolume;
}

/* The S_AddLoopingSound pool rides the same product on the AMBIENT bus: the
   s_sfxvolume trim it had as CHAN_AUTO, and the SFX duck depth. */
float S_EF2_LoopChannelVolume()
{
    const int bus = SNDBUS_AMBIENT;
    float     fVolume;

    fVolume = s_volume->value * SndMix_CategoryGain(bus);
    fVolume *= s_fFadeVolume;
    fVolume *= SndMix_BusGain(bus);

    return fVolume;
}

qboolean S_NeedFullRestart()
{
    return Cvar_Get("s_initsound", "1", 0)->integer != s_bLastInitSound;
}

void S_PrintInfo()
{
    const char *dev;

    Com_Printf("----- Sound Info -----\n");

    if (s_bSoundStarted) {
        dev = NULL;
        if (s_openaldevice) {
            dev = s_openaldevice->string;
        }
        if (!dev || !*dev) {
            dev = "{default}";
        }
        Com_Printf("device - %s\n", dev);
        if (s_reverb->integer) {
            Com_Printf("reverb - ON\n");
        } else {
            Com_Printf("reverb - OFF\n");
        }
        Com_Printf("samplebits - 16\n");
        Com_Printf("speed - %d\n", al_frequency);
        if (s_loadas8bit->integer) {
            Com_Printf("Can NOT force all sounds to 8 bit in OpenAL, I think.\n");
        }
    } else {
        Com_Printf("sound system not started\n");
    }

    Com_Printf("----------------------\n");
}

static void S_DumpStatus(const char *pszChanName, int iChanNum, openal_channel *channel)
{
    sfx_t *sfx;
    ALint  status;

    sfx = channel->pSfx;

    qalGetSourceiv(channel->source, AL_SOURCE_STATE, &status);
    alDieIfError();

    if (status == AL_PLAYING || status == AL_PAUSED) {
        const char *pszMode = status == AL_PLAYING ? "playing" : "paused";

        if (sfx) {
            if (sfx != (sfx_t *)-16 && sfx->name[0]) {
                Com_Printf("%s channel %i - %s sfx %s\n", pszChanName, iChanNum, pszMode, sfx->name);
            } else {
                Com_Printf("%s channel %i - %s with nameless sfx\n", pszChanName, iChanNum, pszMode);
            }
        } else {
            Com_Printf("%s channel %i - %s with NULL sfx\n", pszChanName, iChanNum, pszMode);
        }
    }
}

void S_DumpInfo()
{
    int i;

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_3D; i++) {
        S_DumpStatus("3D", i, openal.channel[i]);
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
        S_DumpStatus("2D", i, openal.channel[MAX_SOUNDSYSTEM_CHANNELS_3D + i]);
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D_STREAM; i++) {
        S_DumpStatus("2D stream", i, openal.channel[MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D + i]);
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        S_DumpStatus(
            "Song",
            i,
            openal.channel
                [MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D + MAX_SOUNDSYSTEM_CHANNELS_2D_STREAM + i]
        );
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_MISC_CHANNELS; i++) {
        S_DumpStatus("Misc", i, openal.channel[SOUNDSYSTEM_CHANNEL_MP3_ID + i]);
    }
}

static bool S_OPENAL_LoadMP3(const char *_path, openal_channel *chan)
{
    char       path[MAX_QPATH];
    snd_info_t info;
    ALuint     format;

    chan->stop();

    qalSourcei(chan->source, AL_BUFFER, 0);
    alDieIfError();

    Q_strncpyz(path, _path, sizeof(path));
    path[MAX_QPATH - 1] = 0;

    FS_CorrectCase(path);

    chan->bufferdata = (ALubyte *)S_CodecLoad(path, &info);
    if (!chan->bufferdata) {
        return false;
    }

    format = S_OPENAL_Format(info.width, info.channels);

    qalGenBuffers(1, &chan->buffer);
    alDieIfError();

    if (info.size == 0) {
        byte dummyData[2] = {0};

        qalBufferData(chan->buffer, AL_FORMAT_MONO16, (void *)dummyData, 2, 22050);
    } else {
        qalBufferData(chan->buffer, format, chan->bufferdata, info.size, info.rate);
    }

    alDieIfError();

    Hunk_FreeTempMemory(chan->bufferdata);
    chan->bufferdata = NULL;

    qalSourcei(chan->source, AL_BUFFER, chan->buffer);
    alDieIfError();

    chan->set_no_3d();

    return true;
}

static void S_OPENAL_PlayMP3()
{
    const char *path;

    if (Cmd_Argc() != 2) {
        Com_Printf("playmp3 <mp3 file>\n");
        return;
    }

    path = Cmd_Argv(1);
    if (!S_OPENAL_LoadMP3(path, &openal.chan_mp3)) {
        Com_Printf("Failed to play mp3 - %s\n", path);
        return;
    }

    openal.chan_mp3.play();
    Com_Printf("Playing mp3 - %s\n", path);
}

static void S_OPENAL_StopMP3()
{
    S_OPENAL_NukeChannel(&openal.chan_mp3);
}

void MUSIC_Pause()
{
    int i;

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        openal.chan_song[i].pause();
    }
}

void MUSIC_Unpause()
{
    int i;

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        if (openal.chan_song[i].is_paused()) {
            openal.chan_song[i].play();
        }
    }
}

void S_PauseSound()
{
    int i;

    if (!s_bSoundStarted) {
        return;
    }

    s_bSoundPaused = true;

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (!pChannel) {
            continue;
        }

        if (pChannel->iEntChannel == CHAN_MENU) {
            continue;
        }

        if (!pChannel->is_playing()) {
            continue;
        }

        pChannel->pause();
    }

    if (openal.chan_mp3.is_playing()) {
        openal.chan_mp3.pause();
    }

    MUSIC_Pause();
    S_TriggeredMusic_Pause();
}

void S_UnpauseSound()
{
    int i;

    if (!s_bSoundStarted) {
        return;
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (!pChannel) {
            continue;
        }

        if (pChannel->is_paused() || (pChannel->iFlags & CHANNEL_FLAG_PLAY_DEFERRED)) {
            pChannel->iFlags &= ~CHANNEL_FLAG_PLAY_DEFERRED;
            pChannel->play();
        }
    }

    if (openal.chan_mp3.is_paused()) {
        openal.chan_mp3.play();
    }

    MUSIC_Unpause();
    S_TriggeredMusic_Unpause();

    s_bSoundPaused = false;
}

static qboolean S_OPENAL_ShouldPlay(sfx_t *pSfx)
{
    int iMaxNumberPlaying;
    int iNumberPlaying;
    int i;

    // is NEVER played.  Reproduced.
    iMaxNumberPlaying = sfx_infos[pSfx->sfx_info_index].max_number_playing;
    if (iMaxNumberPlaying < 0) {
        return qtrue;
    }

    iNumberPlaying = 0;

    /* Retail counts too: S_MILES_ShouldPlay @0x001F4250 walks the 3D pool,
       increments on `strcmp(name) && (status==4 || pending)' and returns
       `count < max_number_playing'.  The note that stood here said the opposite;
       the decompile refutes it.  Phase 3's steal policy is what would change it. */
    for (i = 0; i < s_iNumChannels3D; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (!pChannel || !pChannel->pSfx) {
            continue;
        }

        if (!strcmp(pChannel->pSfx->name, pSfx->name)
            && (pChannel->is_playing() || pChannel->bPendingStart)) {
            iNumberPlaying++;
        }
    }

    return iNumberPlaying < iMaxNumberPlaying;
}

static qboolean S_OPENAL_ShouldStart(const vec3_t vOrigin, float fMinDist, float fMaxDist)
{
    vec3_t vDir;
    vec3_t vListenerOrigin;
    vec3_t alvec;
    float  fStartDistance;

    if (!al_initialized) {
        return false;
    }

    qalGetListenerfv(AL_POSITION, alvec);

    VectorCopy(alvec, vListenerOrigin);
    VectorSubtract(vOrigin, vListenerOrigin, vDir);

    fStartDistance = fMinDist * 16.0f;
    if (fStartDistance > fMaxDist) {
        fStartDistance = fMaxDist;
    }

    return Square(fStartDistance) > VectorLengthSquared(vDir);
}

static float S_OPENAL_Priority(const sfx_t *pSfx, int iEntChannel, qboolean bCountSelf)
{
    float fPriority;
    int   iNumPlaying;
    int   i;

    if (iEntChannel == CHAN_DIALOG || iEntChannel == CHAN_DIALOG_SECONDARY || iEntChannel == CHAN_CINEMATIC) {
        fPriority = 1.0f;
    } else if (iEntChannel == CHAN_WEAPON || iEntChannel == CHAN_WEAPONIDLE) {
        fPriority = 0.75f;
    } else {
        fPriority = 0.5f;
    }

    iNumPlaying = bCountSelf ? 1 : 0;

    for (i = 0; i < s_iNumChannels3D; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (!pChannel) {
            continue;
        }

        if (pChannel->pSfx == pSfx && pChannel->is_playing()) {
            iNumPlaying++;
        }
    }

    if (fPriority > 0.5f || iNumPlaying <= 1) {
        if (fPriority <= 0.75f && iNumPlaying > 2) {
            fPriority /= (float)(iNumPlaying - 1);
        }
    } else {
        fPriority /= (float)iNumPlaying;
    }

    return fPriority;
}

static int S_OPENAL_PickChannelBase(sfx_t *pSfx, int iEntNum, int iEntChannel, int iFirstChannel, int iLastChannel)
{
    int             i;
    int             iBestChannel;
    float           fNewPriority;
    float           fBestPriority;
    float           fChannelPriority;
    openal_channel *pChannel;

    if (iEntChannel < 0) {
        Com_Error(ERR_DROP, "S_OPENAL_PickChannel: entchannel<0");
    }

    fNewPriority  = S_OPENAL_Priority(pSfx, iEntChannel, qtrue);
    fBestPriority = 1.0f;
    iBestChannel  = -1;

    for (i = iFirstChannel; i <= iLastChannel; i++) {
        pChannel = openal.channel[i];
        if (!pChannel) {
            continue;
        }

        if (iEntChannel && pChannel->iEntNum == iEntNum && pChannel->iEntChannel == iEntChannel) {
            pChannel->end_sample();
            fBestPriority = 0.0f;
            iBestChannel  = i;
            break;
        }

        if (pChannel->is_free()) {
            fBestPriority = 0.0f;
            iBestChannel  = i;
        }

        fChannelPriority = S_OPENAL_Priority(pChannel->pSfx, pChannel->iEntChannel, qfalse);

        if ((iBestChannel < 0 || fChannelPriority < fBestPriority) && pChannel->pSfx
            && strcmp(pChannel->pSfx->name, pSfx->name)) {
            fBestPriority = fChannelPriority;
            iBestChannel  = i;
        }
    }

    if (fBestPriority < fNewPriority) {
        return iBestChannel;
    }

    if (iBestChannel > 0 && fBestPriority >= 0.5f) {
        Com_DPrintf("Could not find a free channel for a sound with priority %f\n", fBestPriority);
    }

    return -1;
}

static int S_OPENAL_PickChannel3D(sfx_t *pSfx, int iEntNum, int iEntChannel)
{
    return S_OPENAL_PickChannelBase(pSfx, iEntNum, iEntChannel, 0, s_iNumChannels3D - 1);
}

static void SND_SndRow(
    const char *stage, const char *name, int handle, int iEntNum, int iEntChannel, float fVol, float fMin, float fMax,
    float fPitch, float gain, int iPick, int iStolen
);

static void S_OPENAL_Start2DSound(
    const vec3_t vOrigin,
    int          iEntNum,
    int          iEntChannel,
    sfx_t       *pSfx,
    float        fVolume,
    float        fMinDistance,
    float        fPitch,
    float        fMaxDistance
)
{
    int             i;
    int             iRealEntNum;
    int             iFreeChannel;
    int             iOldEntChannel;
    openal_channel *pChannel;
    sfx_info_t     *pSfxInfo;
    float           fRecordVolume;
    float           fRealVolume;

    (void)vOrigin;       
    (void)fMaxDistance; 

    iRealEntNum = iEntNum;
    pSfxInfo    = &sfx_infos[pSfx->sfx_info_index];

    // and it had a 32-handle streamed pool that retail does not have at all (retail's 2D
    if (iEntChannel == CHAN_MENU) {
        for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
            openal_channel *pMenu = &openal.chan_2D[i];

            if (pMenu->iEntChannel == CHAN_MENU && pMenu->is_playing()) {
                pMenu->stop();
            }
        }
    }

    iFreeChannel = -1;
    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
        openal_channel *pSlot = &openal.chan_2D[i];

        if (!pSlot->is_playing() && !pSlot->bPendingStart) {
            iFreeChannel = MAX_SOUNDSYSTEM_CHANNELS_3D + i;
            break;
        }
    }

    if (iFreeChannel < 0 && SndMix_Enabled()) {
        /* Dialog never drops: steal the oldest slot, never a menu one -- the
           CHAN_MENU block above keeps at most one menu sample live, so a victim
           always exists.  Retail drops here (S_MILES_Start2DSound @0x001F1470
           warns and returns); that is the divergence this removes. */
        int iVictim = -1;
        int iOldest = 0;

        for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
            openal_channel *pSlot = &openal.chan_2D[i];

            if (pSlot->iEntChannel == CHAN_MENU) {
                continue;
            }
            if (!pSlot->is_playing() && !pSlot->bPendingStart) {
                continue;
            }
            if (iVictim < 0 || pSlot->iStartTime < iOldest) {
                iVictim = i;
                iOldest = pSlot->iStartTime;
            }
        }

        if (iVictim >= 0) {
            iFreeChannel = MAX_SOUNDSYSTEM_CHANNELS_3D + iVictim;
            SndMix_NoteDialogSteal();
        }
    }

    if (iFreeChannel < 0) {
        Com_DPrintf("Could not find a free dialog channel\n");
        SndMix_NoteDialogDrop();
        return;
    }

    pChannel = openal.channel[iFreeChannel];

    /* The category gain reads the slot's PREVIOUS channel, not the one about to
       be stored -- retail does the same (S_MILES_Start2DSound @0x001F1470 reads
       slot[42], the old channel, into S_GetChanVolume before storing the new
       one).  One frame per recycled slot, and retail's: do not "fix" it. */
    iOldEntChannel = pChannel->iEntChannel;
    pChannel->force_free();

    if (iOldEntChannel == CHAN_MENU) {
        pChannel->fVolume = fRecordVolume = pSfxInfo->volume;
        fRealVolume = S_GetBaseVolume() * fRecordVolume;
    } else if (fVolume >= 0.0f) {
        pChannel->fVolume = fRecordVolume = fVolume;
        fRealVolume = S_EF2_ChannelVolume(iOldEntChannel) * fVolume;
    } else {
        pChannel->fVolume = fRecordVolume = pSfxInfo->volume;
        fRealVolume = S_EF2_ChannelVolume(iOldEntChannel) * fRecordVolume;
    }

    if (pSfxInfo->localsound) {
        pChannel->iFlags |= CHANNEL_FLAG_LOCAL_LISTENER;
    } else {
        pChannel->iFlags &= ~CHANNEL_FLAG_LOCAL_LISTENER;
    }
    pChannel->iFlags &= ~CHANNEL_FLAG_PAUSED;

    if (fMinDistance < 0.0) {
        fMinDistance = pSfxInfo->mindist;
    }
    pChannel->fMinDist = fMinDistance;
    pChannel->fMaxDist = 0.0;

    pChannel->fNewPitchMult = 0.0f; // retail's 2D path never sets a playback rate
    pChannel->pSfx          = pSfx;
    pChannel->iEntNum       = iRealEntNum;
    pChannel->iEntChannel   = iEntChannel;
    pChannel->iFlags &= ~CHANNEL_FLAG_NO_ENTITY;
    pChannel->iTime         = 0;
    VectorClear(pChannel->vOrigin);

    pChannel->stop();
    pChannel->set_no_3d();
    if (!pChannel->set_sfx(pSfx)) {
        Com_DPrintf("Could not setup dialog file!\n");
        return;
    }
    pChannel->iBaseRate = pChannel->sample_playback_rate();
    pChannel->set_sample_pan(64); 
    pChannel->set_gain_3d(fRealVolume);

    SND_SndRow(
        "start2d", pSfx->name, (int)( pSfx - s_knownSfx ), iEntNum, iEntChannel, fVolume, fMinDistance, 0.0f, fPitch,
        powf(fRealVolume, 5.0f / 3.0f), iFreeChannel, 0
    );

    pChannel->iStartTime    = cl.serverTime;
    pChannel->iEndTime      = 0;
    pChannel->bPendingStart = qtrue;

    if (iEntChannel == CHAN_MENU) {
        pChannel->bPendingStart = qfalse;
        pChannel->play();
    }

    if (s_show_sounds->integer > 0) {
        Com_DPrintf("%d - %s (vol %f)\n", iFreeChannel, pSfx->name, fRecordVolume);
    }
}

static void S_EF2_ArmSampleLoop(openal_channel *pChannel, sfx_t *pSfx, const sfx_info_t *pSfxInfo)
{
    if (pSfxInfo->loop_start == -1 && pSfxInfo->loop_end == -1) {
        pChannel->set_sample_loop_count(1);
        return;
    }

    if (pSfxInfo->loop_start == -1 || pSfxInfo->loop_end == -1) {
        Com_DPrintf("invalid loop block - %d to %d\n", pSfxInfo->loop_start, pSfxInfo->loop_end);
        pChannel->set_sample_loop_count(1);
        return;
    }

    {
        int width = (pSfx && pSfx->width > 0) ? pSfx->width : 1;

        pChannel->set_sample_loop_block(pSfxInfo->loop_start / width, pSfxInfo->loop_end / width);
    }
    pChannel->set_sample_loop_count(0);

    pChannel->iFlags |= CHANNEL_FLAG_LOOPING;
    if (s_show_sounds->integer) {
        Com_DPrintf("loopblock - %d to %d\n", pSfxInfo->loop_start, pSfxInfo->loop_end);
    }
}

static void SND_SndRow(
    const char *stage, const char *name, int handle, int iEntNum, int iEntChannel, float fVol, float fMin, float fMax,
    float fPitch, float gain, int iPick, int iStolen
)
{
    static cvar_t *snd = NULL;
    static int     rows;
    static int     capped;

    if (!snd) {
        snd = Cvar_Get("qa_snd", "0", 0);
    }

    if (!snd->integer) {
        return;
    }

    rows++;

    if (rows == 97 && !capped) {
        capped = 1;
        Com_Printf("[SND:snd] row budget 96 spent -- one in 16 from here; `qa_snd 0' silences\n");
    }

    /* The gate samples the world; a weapon's own fire row is the one event this
       census exists to see, so it is never one of the sampled-away rows. */
    if (rows > 96 && (rows % 16) != 0
        && !(name && (strstr(name, "_fire") || strstr(name, "/fire") || strstr(name, "firestop")))) {
        return;
    }

    ALfloat fRef = 0.0f, fMaxd = 0.0f, fRoll = 0.0f;
    ALint   fRel = 0;

    if (iPick >= 0) {
        qalGetSourcef(openal.channel[iPick]->source, AL_REFERENCE_DISTANCE, &fRef);
        qalGetSourcef(openal.channel[iPick]->source, AL_MAX_DISTANCE, &fMaxd);
        qalGetSourcef(openal.channel[iPick]->source, AL_ROLLOFF_FACTOR, &fRoll);
        qalGetSourcei(openal.channel[iPick]->source, AL_SOURCE_RELATIVE, &fRel);
    }

    Com_Printf(
        "[SND:snd] stage=%s name='%s' handle=%d ent=%d echan=%d vol=%.3f min=%.2f max=%.2f pitch=%.3f "
        "bus=%.4f gain=%.4f ref=%.2f maxd=%.2f roll=%.1f rel=%d pick=%d stolen=%d\n",
        stage, name, handle, iEntNum, iEntChannel, fVol, fMin, fMax, fPitch,
        S_EF2_ChannelVolume(iEntChannel), gain, fRef, fMaxd, fRoll, fRel, iPick, iStolen
    );
}

void S_OPENAL_StartSound(
    const vec3_t vOrigin,
    int          iEntNum,
    int          iEntChannel,
    sfxHandle_t  sfxHandle,
    float        fVolume,
    float        fMinDist,
    float        fPitch,
    float        fMaxDist,
    qboolean     bStreamed
)
{
    int             iChannel;
    int             iStolen;
    openal_channel *pChannel;
    sfx_info_t     *pSfxInfo;
    sfx_t          *pSfx;

    pSfx = &s_knownSfx[sfxHandle];
    if (bStreamed) {
        pSfx->iFlags |= SFX_FLAG_STREAMED;
    }

    if (!S_OPENAL_ShouldPlay(pSfx)) {
        Com_DPrintf("OpenAL: ^~^~^ Not playing sound '%s'\n", pSfx->name);
        SND_SndRow("reject=maxnumber", pSfx->name, sfxHandle, iEntNum, iEntChannel, fVolume, fMinDist, fMaxDist, fPitch, 0.0f, -1, 0);
        return;
    }

    // .wav (menu click, `play x.wav`) belongs in the 64-handle 3D pool in retail, not the
    if (strstr(pSfx->name, ".mp3")) {
        S_OPENAL_Start2DSound(vOrigin, iEntNum, iEntChannel, pSfx, fVolume, fMinDist, fPitch, fMaxDist);
        return;
    }

    pSfxInfo = &sfx_infos[pSfx->sfx_info_index];

    iChannel = S_OPENAL_PickChannel3D(pSfx, iEntNum, iEntChannel);
    if (iChannel < 0) {
        Com_DPrintf(
            "OpenAL: Couldn't play %s sound '%s' for entity %i on channel %s\n",
            (pSfx->iFlags & SFX_FLAG_STREAMED) ? "3Dstreamed" : "3D",
            pSfx->name,
            iEntNum,
            S_ChannelNumToName(iEntChannel)
        );
        SND_SndRow("reject=nochannel", pSfx->name, sfxHandle, iEntNum, iEntChannel, fVolume, fMinDist, fMaxDist, fPitch, 0.0f, -1, 0);
        return;
    }

    iStolen = !openal.chan_3D[iChannel].is_free();

    pChannel = &openal.chan_3D[iChannel];

    pChannel->stop();
    pChannel->iFlags &= ~(CHANNEL_FLAG_LOOPING | CHANNEL_FLAG_PAUSED | CHANNEL_FLAG_LOCAL_LISTENER);
    pChannel->fNewPitchMult = fPitch;
    pChannel->iEntChannel   = iEntChannel;
    if (!pChannel->set_sfx(pSfx)) {
        Com_DPrintf("Set sample error - %s for %s\n", pSfx->name, pSfx->name);
        SND_SndRow("reject=setfx", pSfx->name, sfxHandle, iEntNum, iEntChannel, fVolume, fMinDist, fMaxDist, fPitch, 0.0f, iChannel, iStolen);
        return;
    }

    if (fMinDist <= 0.0) {
        fMinDist = pSfxInfo->mindist;
        if (fMinDist <= 0.0) {
            fMinDist = 160.0;
        }
    }
    if (fMaxDist <= 0.0) {
        fMaxDist = fMinDist * 64.0;
    }
    pChannel->fMinDist = fMinDist;
    pChannel->fMaxDist = fMaxDist;

    if (fVolume < 0.0) {
        fVolume = pSfxInfo->volume;
    }

    pChannel->fVolume = fVolume;

    pChannel->set_3d();
    pChannel->fObstruction = 1.0f; 
    pChannel->fDistGain    = 1.0f; 
    pChannel->set_gain_3d(S_EF2_ChannelVolume(iEntChannel) * pChannel->fVolume);

    SND_SndRow(
        "start", pSfx->name, sfxHandle, iEntNum, iEntChannel, fVolume, fMinDist, fMaxDist, fPitch,
        powf(S_EF2_ChannelVolume(iEntChannel) * pChannel->fVolume, 5.0f / 3.0f), iChannel, iStolen
    );

    if (s_show_sounds->integer > 0) {
        Com_DPrintf("%d - %s (vol %f, mindist %f)\n", iChannel, pSfx->name, pChannel->fVolume, fMinDist);
    }

    //     `iEntChannel == CHAN_LOCAL`.  That predicate is MOHAA's, not retail's -- retail
    if (pSfxInfo->localsound) {
        pChannel->iFlags |= CHANNEL_FLAG_LOCAL_LISTENER;
    } else {
        pChannel->iFlags &= ~CHANNEL_FLAG_LOCAL_LISTENER;
    }

    pChannel->set_velocity(0, 0, 0);
    if (iEntNum == ENTITYNUM_NONE || iEntNum == -1) {
        if (vOrigin) {
            VectorCopy(vOrigin, pChannel->vOrigin);
        } else {
            VectorClear(pChannel->vOrigin);
        }

        pChannel->set_position(pChannel->vOrigin[0], pChannel->vOrigin[1], pChannel->vOrigin[2]);
        pChannel->iFlags |= CHANNEL_FLAG_NO_ENTITY;
        pChannel->iEntNum = 0;
    } else {
        pChannel->iFlags &= ~CHANNEL_FLAG_NO_ENTITY;
        pChannel->iEntNum = iEntNum;
        if (vOrigin) {
            VectorCopy(vOrigin, pChannel->vOrigin);

            pChannel->iTime = cl.serverTime - 1;
            if (pChannel->iTime < 0) {
                pChannel->iTime = 0;
            }

            if (s_entity[iEntNum].time < pChannel->iTime) {
                pChannel->iFlags |= CHANNEL_FLAG_MISSING_ENT;
            }
        } else {
            VectorClear(pChannel->vOrigin);
            pChannel->iTime = 0;
        }
        pChannel->set_position(pChannel->vOrigin[0], pChannel->vOrigin[1], pChannel->vOrigin[2]);
    }

    // into the loop-block arm where retail warns and does not loop, and it set the +52 flag
    S_EF2_ArmSampleLoop(pChannel, pSfx, pSfxInfo);

    pChannel->bPendingStart = qtrue;
    pChannel->iStartTime    = cl.serverTime;
    pChannel->iEndTime      = 0;

    if (iEntChannel == CHAN_MENU) {
        pChannel->bPendingStart = qfalse;
        pChannel->iBaseRate     = pSfx->info.rate;
        if (pChannel->fNewPitchMult > 0.0f) {
            pChannel->set_sample_playback_rate(pChannel->iBaseRate * pChannel->fNewPitchMult);
            pChannel->fNewPitchMult = 0;
        }

        pChannel->play();
        pChannel->iEndTime = cl.serverTime + (int)pChannel->pSfx->time_length + 250;
    }
}

void S_OPENAL_AddLoopingSound(
    const vec3_t vOrigin,
    const vec3_t vVelocity,
    sfxHandle_t  sfxHandle,
    float        fVolume,
    float        fMinDist,
    int          iEntNum,
    int          iUnused7,
    int          iUnused8
)
{
    int                  i;
    int                  iFreeLoopSound;
    sfx_t               *pSfx;
    openal_loop_sound_t *pLoopSound;

    iFreeLoopSound = -1;
    pSfx           = &s_knownSfx[sfxHandle];
    if (!pSfx || !pSfx->length) {
        return;
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
        pLoopSound = &openal.loop_sounds[i];
        if (pLoopSound->pSfx == pSfx && !pLoopSound->bInUse) {
            iFreeLoopSound = i;
            break;
        }
    }

    if (iFreeLoopSound < 0) {
        for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
            pLoopSound = &openal.loop_sounds[i];
            if (!pLoopSound->pSfx && !pLoopSound->bInUse) {
                iFreeLoopSound       = i;
                pLoopSound->bPlaying = false;
                break;
            }
        }
    }

    if (iFreeLoopSound < 0) {
        if (cls.realtime >= s_iNextLoopingWarning) {
            Com_DPrintf("Too many looping sounds\n");
            s_iNextLoopingWarning = cls.realtime + 250;
        }
        return;
    }

    pLoopSound = &openal.loop_sounds[iFreeLoopSound];
    VectorCopy(vOrigin, pLoopSound->vOrigin);
    VectorCopy(vVelocity, pLoopSound->vVelocity);
    pLoopSound->pSfx        = pSfx;
    pLoopSound->bInUse      = true;
    pLoopSound->iStartTime  = cls.realtime;
    pLoopSound->fBaseVolume = fVolume;
    pLoopSound->fMinDist    = fMinDist;
    pLoopSound->fMaxDist    = 0;
    pLoopSound->fPitch      = 0;
    pLoopSound->iFlags      = 0;
    pLoopSound->iEntNum     = iEntNum;
    pLoopSound->bUseListener = s_entity[iEntNum].use_listener ? true : false;
    /* Retail combines a loop's emitters when the caller passes no velocity:
       S_MILES_AddLoopingSound @0x001F1EB0 compares each velocity component against the pool's zero
       word (`v28 = *(float *)(LODWORD(v10) + 24)', then `v26[16] = v28 == a2[0] && v28 == a2[1] &&
       v28 == a2[2]'), so combine is TRUE when velocity IS zero. The predicate here was inverted,
       and every cgame loop call passes vec3_origin, so the combine arm in S_OPENAL_AddLoopSounds
       never ran and neither the summed volume nor the weighted-centroid origin was ever computed. */
    pLoopSound->bCombine     = ( VectorCompare( vVelocity, vec_zero ) != 0 ) ? true : false;

    if (pLoopSound->bCombine) {
        for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
            if (openal.loop_sounds[i].pSfx == pSfx && openal.loop_sounds[i].bInUse) {
                pLoopSound->iStartTime = openal.loop_sounds[i].iStartTime;
                if (openal.loop_sounds[i].bPlaying) {
                    pLoopSound->bPlaying = true;
                    pLoopSound->iChannel = openal.loop_sounds[i].iChannel;
                }
            }
        }
    }
}

void S_OPENAL_StopLoopingSound(openal_loop_sound_t *pLoopSound)
{
    bool            bMayStop;
    int             i;

    if (!pLoopSound->bPlaying) {
        return;
    }
    bMayStop = true;

    if (pLoopSound->bCombine) {
        for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
            if (openal.loop_sounds[i].pSfx == pLoopSound->pSfx && openal.loop_sounds[i].bInUse) {
                bMayStop = false;
                break;
            }
        }
    }

    if (bMayStop) {
        openal_channel *pChannel = openal.channel[pLoopSound->iChannel];

        if (pChannel && pChannel->pSfx == pLoopSound->pSfx) {
            if (s_show_sounds->integer != 0 || s_show_loopsounds->integer != 0) {
                Com_DPrintf("%d - stopped loop - %s\n", pLoopSound->iChannel, pLoopSound->pSfx->name);
            }
            pChannel->force_free();
        }
    }
    pLoopSound->pSfx     = NULL;
    pLoopSound->bPlaying = false;

    if (pLoopSound->bCombine) {
        for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
            if (openal.loop_sounds[i].pSfx == pLoopSound->pSfx) {
                openal.loop_sounds[i].bPlaying = false;
                openal.loop_sounds[i].pSfx     = NULL;
            }
        }
    }
}

void S_OPENAL_ClearLoopingSounds()
{
    int i;

    for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
        openal.loop_sounds[i].bInUse = false;
    }
}

void S_OPENAL_StopLoopingSounds()
{
    int i;

    for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
        openal.loop_sounds[i].bInUse   = false;
        openal.loop_sounds[i].bPlaying = false;
        openal.loop_sounds[i].pSfx     = NULL;
    }
}

void S_OPENAL_StopSound(int iEntNum, int iEntChannel)
{
    int i;

    for (i = 0; i < s_iNumChannels3D; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (pChannel && !pChannel->is_free() && pChannel->iEntNum == iEntNum && pChannel->iEntChannel == iEntChannel) {
            pChannel->end_sample();
        }
    }

    for (i = MAX_SOUNDSYSTEM_CHANNELS_3D; i < MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (pChannel && !pChannel->is_free() && pChannel->iEntNum == iEntNum && pChannel->iEntChannel == iEntChannel) {
            pChannel->end_sample();
        }
    }
}

void S_OPENAL_StopAllSounds(qboolean bStopMusic)
{
    int i;

    if (!s_bSoundStarted) {
        return;
    }

    S_OPENAL_StopLoopingSounds();

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        openal_channel *pChannel = openal.channel[i];
        if (pChannel) {
            pChannel->force_free();
        }
    }

    if (bStopMusic) {
        MUSIC_FreeAllSongs();
        S_TriggeredMusic_Stop();
    }
}

static int S_OPENAL_Start3DLoopSound(
    openal_loop_sound_t *pLoopSound,
    float                fVolumeToPlay,
    float                fMinDistance,
    float                fMaxDistance,
    const vec3_t         vLoopOrigin,
    const vec3_t         vListenerOrigin
)
{
    int             iChannel;
    int             iSoundOffset;
    float           fLoopPitch;
    openal_channel *pChan3D;

    if (!pLoopSound->pSfx->data || !pLoopSound->pSfx->length) {
        return -1;
    }

    iChannel = S_OPENAL_PickChannel3D(pLoopSound->pSfx, 0, CHAN_AUTO);
    if (iChannel < 0) {
        Com_DPrintf("OpenAL: Could not find a free channel\n");
        return iChannel;
    }

    pChan3D = &openal.chan_3D[iChannel];
    pChan3D->force_free();
    pChan3D->iEntNum     = 0;
    pChan3D->iEntChannel = 0;
    pChan3D->fMinDist    = fMinDistance;
    pChan3D->fMaxDist    = fMaxDistance;
    pChan3D->set_3d();

    if (!pChan3D->set_sfx(pLoopSound->pSfx)) {
        Com_DPrintf("OpenAL: Set sample error - %s\n", pLoopSound->pSfx->name);
        return -1;
    }

    pChan3D->set_position(vLoopOrigin[0], vLoopOrigin[1], vLoopOrigin[2]);
    VectorScale(pLoopSound->vVelocity, s_fVelocityScale, pLoopSound->vVelocity);
    pChan3D->set_velocity(pLoopSound->vVelocity[0], pLoopSound->vVelocity[1], pLoopSound->vVelocity[2]);
    pChan3D->pSfx = pLoopSound->pSfx;
    pChan3D->iFlags |= CHANNEL_FLAG_PAUSED | CHANNEL_FLAG_NO_ENTITY | CHANNEL_FLAG_LOOP_SOUND;
    pChan3D->iBaseRate = pChan3D->sample_playback_rate();
    pChan3D->iEndTime  = 0; // retail's +60 is never armed on a loop channel

    iSoundOffset = (int)((int)pLoopSound->pSfx->info.width * pLoopSound->pSfx->info.rate
                         * (float)(cls.realtime - pLoopSound->iStartTime) / 1000.0)
                 % pLoopSound->pSfx->length;
    pChan3D->set_sample_offset(iSoundOffset);
    pChan3D->set_sample_loop_count(0);
    pChan3D->fVolume = fVolumeToPlay;
    pChan3D->fDistGain = 1.0f; 
    pChan3D->fObstruction = 1.0f;
    pChan3D->set_gain_3d(fVolumeToPlay);
    pChan3D->play();

    /* A loop has no sfxHandle of its own -- the caller passes the registered sfx_t -- so `handle'
       is the loop's index in the known-sfx table, the same table the handles on the start rows
       index. `pitch' is the rate the channel is actually playing at over its base rate; the loop's
       own pitch multiplier is applied later, in the resample pass, and would print as the
       never-written zero. Both fields were call-site constants here, which is why every loop row
       read handle=0 pitch=0.000. */
    fLoopPitch = ( pChan3D->iBaseRate > 0 )
                     ? (float)pChan3D->sample_playback_rate() / (float)pChan3D->iBaseRate : 1.0f;

    SND_SndRow(
        "startloop", pLoopSound->pSfx->name, (int)( pLoopSound->pSfx - s_knownSfx ), 0, CHAN_AUTO,
        fVolumeToPlay, fMinDistance, fMaxDistance, fLoopPitch,
        powf(fVolumeToPlay, 5.0f / 3.0f), iChannel, 0
    );

    S_OPENAL_reverb(iChannel, s_iReverbType, s_fReverbLevel);

    return iChannel;
}

static bool S_OPENAL_UpdateLoopSound(
    openal_loop_sound_t *pLoopSound,
    float                fVolumeToPlay,
    float                fMinDistance,
    float                fMaxDistance,
    const vec3_t         vListenerOrigin,
    const vec3_t         vTempAxis,
    const vec3_t         vLoopOrigin
)
{
    openal_channel *pChannel;

    (void)fMinDistance;
    (void)fMaxDistance;
    (void)vListenerOrigin;
    (void)vTempAxis;

    pChannel = openal.channel[pLoopSound->iChannel];
    if (!pChannel) {
        return false;
    }

    if (pChannel->pSfx != pLoopSound->pSfx) {
        pLoopSound->bPlaying = 0;
        return false;
    }

    pChannel->iStartTime = cl.serverTime;

    pChannel->set_position(vLoopOrigin[0], vLoopOrigin[1], vLoopOrigin[2]);
    pChannel->fVolume = fVolumeToPlay;
    pChannel->set_gain_3d(fVolumeToPlay);

    if (s_bReverbChanged) {
        S_OPENAL_reverb(pLoopSound->iChannel, s_iReverbType, s_fReverbLevel);
    }

    return true;
}

void S_OPENAL_AddLoopSounds(const vec3_t vTempAxis)
{
    int                  i, j;
    float                fDistance;
    int                  iChannel;
    vec3_t               vListenerOrigin;
    vec3_t               vLoopOrigin;
    openal_loop_sound_t *pLoopSound;
    float                fTotalVolume;
    float                fVolumeToPlay;
    float                fPlayVolume;
    float                fMinDistance, fMaxDistance;
    float                fVolume;
    float                fMaxVolume, fMaxFactor;
    sfx_info_t          *pSfxInfo;
    openal_channel      *pChannel;
    bool                 bAlreadyAdded[MAX_SOUNDSYSTEM_LOOP_SOUNDS] = {false};
    vec3_t               alvec;

    qalGetListenerfv(AL_POSITION, alvec);
    VectorCopy(alvec, vListenerOrigin);

    for (i = 0; i < MAX_SOUNDSYSTEM_LOOP_SOUNDS; i++) {
        vec3_t vDir;

        if (bAlreadyAdded[i]) {
            continue;
        }

        pLoopSound = &openal.loop_sounds[i];
        if (!pLoopSound->pSfx) {
            continue;
        }
        pChannel = openal.channel[pLoopSound->iChannel];

        pSfxInfo = &sfx_infos[pLoopSound->pSfx->sfx_info_index];

        fMinDistance = pLoopSound->fMinDist;
        if (fMinDistance < 0) {
            fMinDistance = pSfxInfo->mindist;
            if (fMinDistance <= 0) {
                fMinDistance = 160;
            }
        }

        fMaxDistance = pLoopSound->fMaxDist;
        if (fMaxDistance <= 0) {
            fMaxDistance = fMinDistance * 64;
        }

        fVolume = pLoopSound->fBaseVolume;
        if (fVolume < 0) {
            fVolume = pSfxInfo->volume;
            if (fVolume <= 0) {
                fVolume = 1;
            }
        }
        fVolume = fVolume * s_fAmbientVolume;

        fTotalVolume = 0.0;
        fMaxVolume   = 0.0;

        if (pLoopSound->bPlaying) {
            pChannel->fNewPitchMult = pLoopSound->fPitch;
        }

        if (pLoopSound->bCombine) {
            for (j = 0; j < MAX_LOOP_SOUNDS; j++) {
                openal_loop_sound_t *pLoopSound2 = &openal.loop_sounds[j];

                if (pLoopSound2->pSfx == pLoopSound->pSfx) {
                    VectorSubtract(pLoopSound2->vOrigin, vListenerOrigin, vDir);
                    VectorCopy(vDir, pLoopSound2->vRelativeOrigin);
                    fDistance = VectorLength(pLoopSound2->vRelativeOrigin);

                    if (fDistance <= fMinDistance) {
                        fVolumeToPlay = fVolume;
                    } else if (fDistance >= fMaxDistance) {
                        fVolumeToPlay = 0;
                    } else {
                        fVolumeToPlay = fMinDistance * fVolume / fDistance;
                    }

                    if (fMaxVolume < fVolumeToPlay) {
                        fMaxVolume = fVolumeToPlay;
                    }
                    fTotalVolume += fVolumeToPlay;
                    /* Retail weights the centroid with each member's own contribution (`*(float
                       *)(v41 + 80) / v26' in S_MILES_AddLoopSounds @0x001F2310); without this
                       store the weight below is a value no arm ever wrote. */
                    pLoopSound2->fVolume = fVolumeToPlay;
                    bAlreadyAdded[j] = true;
                }
            }
        } else {
            VectorSubtract(pLoopSound->vOrigin, vListenerOrigin, vDir);
            VectorCopy(vDir, pLoopSound->vRelativeOrigin);
            fDistance = VectorLength(pLoopSound->vRelativeOrigin);

            if (fDistance <= fMinDistance) {
                fTotalVolume = fVolume;
            } else if (fDistance >= fMaxDistance) {
                fTotalVolume = 0;
            } else {
                fTotalVolume = fMinDistance * fVolume / fDistance;
            }
            pLoopSound->fVolume = fTotalVolume;
            fMaxVolume          = fTotalVolume;
        }

        fMaxFactor = sfx_infos[pLoopSound->pSfx->sfx_info_index].max_factor;
        if (fMaxFactor >= 1 && fMaxVolume * fMaxFactor < fTotalVolume) {
            fTotalVolume = fMaxVolume * fMaxFactor;
        }

        if (fTotalVolume <= 0 && !(pLoopSound->iFlags & LOOPSOUND_FLAG_NO_PAN)) {
            if (pLoopSound->bPlaying) {
                if (s_show_sounds->integer != 0 || s_show_loopsounds->integer != 0) {
                    Com_DPrintf("%d - stopped loop - %s\n", pLoopSound->iChannel, pLoopSound->pSfx->name);
                }

                pChannel->stop();
                pLoopSound->bPlaying = false;
                if (pLoopSound->bCombine) {
                    for (j = 0; j < MAX_LOOP_SOUNDS; j++) {
                        openal_loop_sound_t *pLoopSound2 = &openal.loop_sounds[j];

                        if (pLoopSound2->pSfx == pLoopSound->pSfx) {
                            pLoopSound2->bPlaying = false;
                        }
                    }
                }
            }

            continue;
        }

        VectorClear(vLoopOrigin);

        if (pLoopSound->bCombine) {
            for (j = 0; j < MAX_LOOP_SOUNDS; j++) {
                openal_loop_sound_t *pLoopSound2 = &openal.loop_sounds[j];

                if (pLoopSound2->pSfx == pLoopSound->pSfx) {
                    VectorNormalize(pLoopSound2->vRelativeOrigin);

                    VectorScale(
                        pLoopSound2->vRelativeOrigin, pLoopSound2->fVolume / fTotalVolume, pLoopSound2->vRelativeOrigin
                    );
                    VectorAdd(pLoopSound2->vRelativeOrigin, vLoopOrigin, vLoopOrigin);
                }
            }

            VectorNormalize(vLoopOrigin);
            VectorMA(vListenerOrigin, fMinDistance * 0.5f, vLoopOrigin, vLoopOrigin);

            fPlayVolume = (fTotalVolume > 1.0f) ? 1.0f : fTotalVolume;
        } else {
            VectorCopy(pLoopSound->vOrigin, vLoopOrigin);

            fPlayVolume = 1.0f;
        }

        if (pLoopSound->bPlaying) {
            if (s_show_sounds->integer != 0 || s_show_loopsounds->integer != 0) {
                Com_DPrintf("%d - still playing loop - %s\n", pLoopSound->iChannel, pLoopSound->pSfx->name);
            }

            S_OPENAL_UpdateLoopSound(
                pLoopSound,
                S_EF2_LoopChannelVolume() * fPlayVolume,
                fMinDistance,
                fMaxDistance,
                vListenerOrigin,
                vTempAxis,
                vLoopOrigin
            );

            continue;
        }

        /* One source per playing loop: a second slot for the same sound joins the
           channel already playing it, so a re-start finds its previous copy
           instead of adding a second source at the same level. */
        for (j = 0; j < MAX_SOUNDSYSTEM_LOOP_SOUNDS; j++) {
            openal_loop_sound_t *pLoopSound2 = &openal.loop_sounds[j];

            if (pLoopSound2 != pLoopSound && pLoopSound2->pSfx == pLoopSound->pSfx && pLoopSound2->bPlaying) {
                pLoopSound->bPlaying = true;
                pLoopSound->iChannel = pLoopSound2->iChannel;
                break;
            }
        }
        if (j < MAX_SOUNDSYSTEM_LOOP_SOUNDS) {
            continue;
        }

        if (s_show_sounds->integer != 0 || s_show_loopsounds->integer != 0) {
            Com_DPrintf("%d - started loop - %s\n", pLoopSound->iChannel, pLoopSound->pSfx->name);
        }

        iChannel = S_OPENAL_Start3DLoopSound(
            pLoopSound,
            S_EF2_LoopChannelVolume() * fPlayVolume,
            fMinDistance,
            fMaxDistance,
            vLoopOrigin,
            vListenerOrigin
        );

        if (iChannel < 0) {
            continue;
        }

        pLoopSound->bPlaying = true;
        pLoopSound->iChannel = iChannel;

        if (pLoopSound->bCombine) {
            for (j = 0; j < MAX_LOOP_SOUNDS; j++) {
                openal_loop_sound_t *pLoopSound2 = &openal.loop_sounds[j];

                if (pLoopSound2->pSfx == pLoopSound->pSfx) {
                    pLoopSound2->bPlaying = true;
                    pLoopSound2->iChannel = iChannel;
                }
            }
        }
    }
}

void S_OPENAL_Respatialize(int iEntNum, const vec3_t vHeadPos, const vec3_t vAxis[3])
{
    int             i;
    vec3_t          vOrigin;
    vec3_t          vVelocity;
    vec3_t          vEntOrigin;
    vec3_t          vEntVelocity;
    vec3_t          vDir;
    vec3_t          vListenerOrigin;
    int             iPan;
    vec3_t          vTempAxis;
    float           fVolume;
    float           fDist;
    openal_channel *pChannel;
    vec3_t          alvec {0};
    vec3_t          alorientation[2];

    if (cls.no_menus) {
        return;
    }

    s_iListenerNumber = iEntNum;

    // OpenAL apply a pitch shift retail never does.
    alvec[0] = 0.0f;
    alvec[1] = 0.0f;
    alvec[2] = 0.0f;
    qalListenerfv(AL_VELOCITY, alvec);
    alDieIfError();

    VectorCopy(vHeadPos, alvec);
    VectorCopy(alvec, vListenerOrigin);
    qalListenerfv(AL_POSITION, alvec);
    alDieIfError();

    alorientation[0][0] = vAxis[0][0];
    alorientation[0][1] = vAxis[1][0];
    alorientation[0][2] = vAxis[2][0];
    alorientation[1][0] = vAxis[0][2];
    alorientation[1][1] = vAxis[1][2];
    alorientation[1][2] = vAxis[2][2];
    qalListenerfv(AL_ORIENTATION, (const ALfloat *)alorientation);
    alDieIfError();

    vTempAxis[0] = vAxis[0][1];
    vTempAxis[1] = vAxis[1][1];
    vTempAxis[2] = vAxis[2][1];

    fVolume = 1;
    iPan    = 64;

    for (i = 0; i < s_iNumChannels3D; i++) {
        pChannel = openal.channel[i];

        if (!pChannel) {
            continue;
        }

        if (!(pChannel->iFlags & (CHANNEL_FLAG_LOOP_SOUND | CHANNEL_FLAG_LOOPING)) && pChannel->iEndTime > 0
            && pChannel->iEndTime < cl.serverTime && pChannel->is_playing()) {
            pChannel->end_sample();
            continue;
        }

        if (!(pChannel->is_playing() || pChannel->bPendingStart)) {
            continue;
        }

        if (pChannel->iFlags & CHANNEL_FLAG_PAUSED) {
            continue;
        }

        if (pChannel->iFlags & CHANNEL_FLAG_LOOP_SOUND) {
            continue;
        }

        if (pChannel->iFlags & CHANNEL_FLAG_NO_ENTITY) {
            VectorCopy(pChannel->vOrigin, vOrigin);

            if (pChannel->iFlags & CHANNEL_FLAG_LOCAL_LISTENER) {
                VectorCopy(vListenerOrigin, vOrigin);
            }
            pChannel->set_position(vOrigin[0], vOrigin[1], vOrigin[2]);
        } else if (pChannel->iFlags & CHANNEL_FLAG_LOCAL_LISTENER) {
            // N13 tail (uivk-sound3d): retail does NOT put a localsound 3D sample ON the
            vOrigin[0] = vListenerOrigin[0] + 16.0f * vAxis[0][0];
            vOrigin[1] = vListenerOrigin[1] + 16.0f * vAxis[1][0];
            vOrigin[2] = vListenerOrigin[2] + 16.0f * vAxis[2][0];
            pChannel->set_position(vOrigin[0], vOrigin[1], vOrigin[2]);
        } else {
            if (s_entity[pChannel->iEntNum].time <= pChannel->iTime) {
                VectorCopy(pChannel->vOrigin, vOrigin);

                if (pChannel->iFlags & CHANNEL_FLAG_LOOPING) {
                    pChannel->end_sample();
                    continue;
                }
            } else {
                pChannel->iFlags &= ~CHANNEL_FLAG_MISSING_ENT;

                VectorCopy(s_entity[pChannel->iEntNum].position, vEntOrigin);
                VectorCopy(vEntOrigin, vOrigin);
                VectorCopy(vOrigin, pChannel->vOrigin);
                pChannel->iTime = s_entity[pChannel->iEntNum].time;
            }

            if (s_entity[pChannel->iEntNum].use_listener) {
                VectorCopy(vListenerOrigin, vOrigin);
            }

            if (pChannel->iEntNum == s_iListenerNumber) {
                if (vListenerOrigin[0] == vOrigin[0] && vListenerOrigin[2] == vOrigin[2]) {
                    float fDelta = vListenerOrigin[1] - vOrigin[1];

                    if (fDelta > 89.9f && fDelta < 90.09f) {
                        VectorCopy(vListenerOrigin, vOrigin);
                    }
                }
            }

            pChannel->set_position(vOrigin[0], vOrigin[1], vOrigin[2]);
            VectorCopy(s_entity[pChannel->iEntNum].velocity, vEntVelocity);
            VectorCopy(vEntVelocity, vVelocity);
            VectorScale(vVelocity, s_fVelocityScale, vVelocity);
            pChannel->set_velocity(vVelocity[0], vVelocity[1], vVelocity[2]);
        }

        // EF2 runs.)  EF2 never sets the preference, so retail plays with it ON and a 3D
        if (pChannel->fMaxDist > 0.0f) {
            VectorSubtract(vOrigin, vListenerOrigin, vDir);
            fDist = VectorLength(vDir);
            float fDistGain = (fDist > pChannel->fMaxDist) ? 0.0f : 1.0f;
            if (pChannel->fDistGain != fDistGain) {
                pChannel->fDistGain = fDistGain;
                pChannel->set_gain_3d(S_EF2_ChannelVolume(pChannel->iEntChannel) * pChannel->fVolume);
            }
        }

        if (pChannel->iEntNum != s_iListenerNumber) {
            float fObstruction = 1.0f;
            if (s_checkObstructions->integer) {
                trace_t obTrace;
                CM_BoxTrace(&obTrace, vListenerOrigin, vOrigin, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse);
                if (obTrace.fraction < 1.0f && !obTrace.startsolid) {
                    fObstruction = 0.5f;
                }
            }
            if (pChannel->fObstruction != fObstruction) {
                pChannel->fObstruction = fObstruction;
                pChannel->set_gain_3d(S_EF2_ChannelVolume(pChannel->iEntChannel) * pChannel->fVolume);
            }
        }

        if (pChannel->bPendingStart) {
            if (!S_OPENAL_ShouldStart(pChannel->vOrigin, pChannel->fMinDist, pChannel->fMaxDist)) {
                pChannel->end_sample();
                continue;
            }

            pChannel->bPendingStart = qfalse;
            pChannel->iBaseRate     = pChannel->pSfx->info.rate;
            if (pChannel->fNewPitchMult > 0.0f) {
                pChannel->set_sample_playback_rate(pChannel->iBaseRate * pChannel->fNewPitchMult);
                pChannel->fNewPitchMult = 0;
            }

            pChannel->play();
            pChannel->iStartTime = cl.serverTime;
            pChannel->iEndTime   = cl.serverTime + (int)pChannel->pSfx->time_length + 250;
        }

        if (s_bReverbChanged) {
            S_OPENAL_reverb(i, s_iReverbType, s_fReverbLevel);
        }
    }

    for (i = MAX_SOUNDSYSTEM_CHANNELS_3D; i < MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
        pChannel = openal.channel[i];

        if (!pChannel) {
            continue;
        }

        if (pChannel->iFlags & CHANNEL_FLAG_PAUSED) {
            continue;
        }

        if (!(pChannel->is_playing() || pChannel->bPendingStart)) {
            continue;
        }

        if (pChannel->iEntNum >= 0 && pChannel->iEntNum < MAX_GENTITIES) {
            VectorCopy(s_entity[pChannel->iEntNum].position, vOrigin);
        } else {
            VectorCopy(pChannel->vOrigin, vOrigin);
        }

        iPan = 64;
        if (!(pChannel->iFlags & CHANNEL_FLAG_LOCAL_LISTENER) && pChannel->fMinDist < 10000.0f) {
            iPan = S_OPENAL_SpatializeStereoSound(vListenerOrigin, vTempAxis, vOrigin);
        }

        fVolume = pChannel->fVolume * S_EF2_ChannelVolume(pChannel->iEntChannel);
        if (!(pChannel->iFlags & CHANNEL_FLAG_LOCAL_LISTENER)) {
            VectorSubtract(vOrigin, vListenerOrigin, vDir);
            fDist = VectorLength(vDir);
            if (fDist > pChannel->fMinDist) {
                fVolume = pChannel->fMinDist * fVolume / fDist;
            }
        }

        pChannel->set_gain_3d(fVolume);
        pChannel->set_sample_pan(iPan);

        if (pChannel->bPendingStart) {
            pChannel->bPendingStart = qfalse;
            pChannel->play();
            pChannel->iStartTime = cl.serverTime;
        }

        if (s_bReverbChanged) {
            S_OPENAL_reverb(i, s_iReverbType, s_fReverbLevel);
        }
    }

    S_OPENAL_AddLoopSounds(vTempAxis);
    s_bReverbChanged = false;
}

static int S_OPENAL_SpatializeStereoSound(const vec3_t listener_origin, const vec3_t listener_left, const vec3_t origin)
{
    vec3_t source_vec;
    float  pan;

    VectorSubtract(origin, listener_origin, source_vec);
    VectorNormalize(source_vec);

    const float fSeparation = 0.5f;

    pan = fSeparation + (1.f - fSeparation) * -DotProduct(listener_left, source_vec);

    if (pan < 0) {
        pan = 0;
    }

    return ceilf(pan * 127);
}

static void S_OPENAL_reverb(int iChannel, int iReverbType, float fReverbLevel)
{
    // the 32-clamp at 0x001EFDD0 sits in the second-provider branch that never runs.
    if (iChannel >= s_iNumChannels3D) {
        return;
    }

    if (iReverbType < 0 || iReverbType > 25) {
        return;
    }

    if (!s_bReverbWanted || !s_bReverbCapable) {
        fReverbLevel = 0.0f;
    }

    const float fEffectVolume = fReverbLevel * s_reverbTable[iReverbType];

    (void)fEffectVolume;
}

void S_OPENAL_SetReverb(int iType, float fLevel)
{
    s_fReverbLevel = fLevel;
    s_iReverbType  = iType;

    if (!s_bReverbWanted) {
        return;
    }

    if (!s_bReverbCapable) {
        return;
    }

    s_bReverbChanged = true;
}

/* r54: the flush half of S_OPENAL_Respatialize, lifted into the per-frame pass:
   the UI is the one place Respatialize never runs, because its only caller is
   cgame and the main menu has no cgame.  In game it finds nothing -- Respatialize
   already ran this frame, inside SCR_UpdateScreen, before CL_Frame's S_Update. */
static void S_OPENAL_FlushPendingStarts()
{
    static int s_iTotal;
    openal_channel *pChannel;
    int             i;

    if (!s_menuFlush || !s_menuFlush->integer) {
        return;
    }

    /* CA_ACTIVE means cgame ran Respatialize this frame and owns every armed
       start, including the ones it culled with ShouldStart -- those stay dead.
       Only the UI, which has no cgame, needs this pass. */
    if (cls.state == CA_ACTIVE) {
        return;
    }

    for (i = 0; i < s_iNumChannels3D; i++) {
        pChannel = openal.channel[i];

        if (!pChannel || !pChannel->bPendingStart) {
            continue;
        }

        if (pChannel->iFlags & CHANNEL_FLAG_PAUSED) {
            continue;
        }

        if (!S_OPENAL_ShouldStart(pChannel->vOrigin, pChannel->fMinDist, pChannel->fMaxDist)) {
            continue;
        }

        pChannel->bPendingStart = qfalse;
        pChannel->iBaseRate     = pChannel->pSfx->info.rate;
        if (pChannel->fNewPitchMult > 0.0f) {
            pChannel->set_sample_playback_rate(pChannel->iBaseRate * pChannel->fNewPitchMult);
            pChannel->fNewPitchMult = 0;
        }

        pChannel->play();
        pChannel->iStartTime = cl.serverTime;
        pChannel->iEndTime   = cl.serverTime + (int)pChannel->pSfx->time_length + 250;
        s_iTotal++;

        PROBE_EMIT_N("[SND:", "menuflush", 64,
                     "pool=3d slot=%d chan=%d name='%s' total=%d\n",
                     i, pChannel->iEntChannel, pChannel->pSfx ? pChannel->pSfx->name : "", s_iTotal);
    }

    for (i = MAX_SOUNDSYSTEM_CHANNELS_3D; i < MAX_SOUNDSYSTEM_CHANNELS_3D + MAX_SOUNDSYSTEM_CHANNELS_2D; i++) {
        pChannel = openal.channel[i];

        if (!pChannel || !pChannel->bPendingStart) {
            continue;
        }

        if (pChannel->iFlags & CHANNEL_FLAG_PAUSED) {
            continue;
        }

        pChannel->bPendingStart = qfalse;
        pChannel->play();
        pChannel->iStartTime = cl.serverTime;
        s_iTotal++;

        PROBE_EMIT_N("[SND:", "menuflush", 64,
                     "pool=2d slot=%d chan=%d name='%s' total=%d\n",
                     i, pChannel->iEntChannel, pChannel->pSfx ? pChannel->pSfx->name : "", s_iTotal);
    }
}

void S_OPENAL_Update()
{
    int             i;
    openal_channel *pChannel;

    bool bLevelsChanged = false;


    if (paused->integer && !s_bSoundPaused) {
        S_PauseSound();
    } else if (!paused->integer && s_bSoundPaused) {
        S_UnpauseSound();
    }

    if (s_bFading) {
        s_fFadeVolume = 1.f - (cls.realtime - s_fFadeStartTime) / (s_fFadeStopTime - s_fFadeStartTime);
        if (s_fFadeVolume < 0) {
            s_fFadeVolume = 0;
        }
        music_volume_changed = true;
        bLevelsChanged       = true;
    }

    /* the mixer advances its envelope here and latches the same two dirty
       flags the unreachable pre-r52 ramp used: `bLevelsChanged' re-gains the 3D
       pool and `music_volume_changed' re-gains the song pool, so a duck that
       moves this frame is audible this frame */
    {
        qboolean bMixLevels = qfalse;
        qboolean bMixMusic  = qfalse;

        SndMix_FrameAdvance(cls.realtime, s_fFadeVolume, &bMixLevels, &bMixMusic);

        if (bMixLevels) {
            bLevelsChanged = true;
        }
        if (bMixMusic) {
            music_volume_changed = true;
        }
    }

    if (s_volume->modified) {
        if (s_volume->value > 1) {
            Cvar_Set("s_volume", "1.0");
        } else if (s_volume->value < 0) {
            Cvar_Set("s_volume", "0.0");
        }

        music_volume_changed = true;
        bLevelsChanged       = true;
        s_volume->modified   = 0;
        al_current_volume = 1.0f;
        qalListenerf(AL_GAIN, al_current_volume);
        alDieIfError();
    }

    if (bLevelsChanged) {
        for (i = 0; i < s_iNumChannels3D; i++) {
            pChannel = openal.channel[i];

            if (!pChannel) {
                continue;
            }

            if (pChannel->iFlags & CHANNEL_FLAG_LOOP_SOUND) {
                continue;
            }

            if (!(pChannel->is_playing() || pChannel->bPendingStart)) {
                continue;
            }

            pChannel->set_gain_3d(S_EF2_ChannelVolume(pChannel->iEntChannel) * pChannel->fVolume);
        }
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        pChannel = openal.channel[i];
        if (!pChannel) {
            continue;
        }

        if (!pChannel->is_playing()) {
            continue;
        }

        if (pChannel->fNewPitchMult <= 0) {
            continue;
        }

        pChannel->set_sample_playback_rate(pChannel->iBaseRate * pChannel->fNewPitchMult);
        pChannel->fNewPitchMult = 0;
    }

    if (s_speaker_type->modified) {
        Cbuf_AddText("snd_restart\n");
        s_speaker_type->modified = false;
    }

    if (s_reverb->modified) {
        s_reverb->modified = false;

        s_bReverbWanted = (s_reverb->integer == 1);

        if (s_bReverbWanted) {
            S_OPENAL_SetReverb(s_iReverbType, s_fReverbLevel);
        } else {
            S_OPENAL_SetReverb(0, 0);
        }
    }

    if (s_show_num_active_sounds->integer == 1) {
        int num = 0;

        for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
            pChannel = openal.channel[i];
            if (pChannel && pChannel->is_playing()) {
                ++num;
            }
        }
        Com_DPrintf("Number of active sounds = %d\n", num);
    }

    /* r54: the per-frame flush the UI was missing (see the comment at
       S_OPENAL_FlushPendingStarts).  In game it finds nothing: Respatialize
       already ran inside this frame's SCR_UpdateScreen. */
    S_OPENAL_FlushPendingStarts();

    Music_Update();

    for (i = 0; i < MAX_LOOP_SOUNDS; i++) {
        if (!openal.loop_sounds[i].bInUse) {
            S_OPENAL_StopLoopingSound(&openal.loop_sounds[i]);
        }
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_CHANNELS; i++) {
        openal.channel[i]->update();
    }
}

const char *S_OPENAL_GetMusicFilename()
{
    if (!al_initialized) {
        return "";
    }

    if (!openal.chan_trig_music.is_playing() && !openal.chan_trig_music.is_paused()) {
        return "";
    }

    return openal.tm_filename;
}

int S_OPENAL_GetMusicLoopCount()
{
    return openal.tm_loopcount;
}

unsigned int S_OPENAL_GetMusicOffset()
{
    return openal.chan_trig_music.sample_offset();
}

qboolean S_IsSoundPlaying(int channel_number, const char *sfxName)
{
    openal_channel *pChannel = openal.channel[channel_number];
    if (!pChannel) {
        return false;
    }

    if (s_bSoundPaused) {
        return false;
    }

    if (!pChannel->is_playing()) {
        return false;
    }

    if (!pChannel->pSfx) {
        return false;
    }

    if (strcmp(sfxName, pChannel->pSfx->name)) {
        return false;
    }

    return true;
}

static void S_StoreBase(channelbasesavegame_t *pBase, openal_channel *pChannel)
{
    if (!pChannel) {
        return;
    }

    if (pChannel->iEntChannel == CHAN_MENU || pChannel->is_free()) {
        pBase->bPlaying      = false;
        pBase->iOffset       = 0;
        pBase->iLoopCount    = 0;
        pBase->sfx.szName[0] = 0;
        pBase->sfx.iFlags    = 0;
        pBase->fNewPitchMult = 1.f;
        pBase->iBaseRate     = pChannel->iBaseRate;
        pBase->iStatus       = 0;
    } else {
        pBase->bPlaying   = true;
        pBase->iOffset    = pChannel->sample_offset();
        pBase->iLoopCount = pChannel->sample_loop_count();
        memcpy(pBase->sfx.szName, pChannel->pSfx->name, sizeof(pBase->sfx.szName));
        pBase->sfx.iFlags    = pChannel->pSfx->iFlags;
        pBase->iBaseRate     = pChannel->iBaseRate;
        pBase->iStatus       = 0;
        pBase->fNewPitchMult = (float)pBase->iBaseRate / (float)pChannel->sample_playback_rate();
    }

    pBase->iStartTime                = pChannel->iStartTime - cl.serverTime;
    pBase->iEndTime                  = pChannel->iEndTime - cl.serverTime;
    pBase->iEntChannel               = pChannel->iEntChannel;
    pBase->iEntNum                   = pChannel->iEntNum;
    pBase->iFlags                    = pChannel->iFlags;
    pBase->fMaxDist                  = pChannel->fMaxDist;
    pBase->fMinDist                  = pChannel->fMinDist;
    pBase->iNextCheckObstructionTime = 0;
    VectorCopy(pChannel->vOrigin, pBase->vOrigin);
    pBase->iTime   = pChannel->iTime - cl.serverTime;
    pBase->fVolume = pChannel->fVolume;
}

static void
S_StartSoundFromBase(channelbasesavegame_t *pBase, openal_channel *pChannel, sfx_t *pSfx, bool bStartUnpaused)
{
    if (!pChannel->set_sfx(pSfx)) {
        Com_DPrintf("OpenAL: Set sample error - %s\n", pSfx->name);
        pChannel->iFlags &= ~CHANNEL_FLAG_PLAY_DEFERRED;
        return;
    }

    pChannel->iBaseRate = pChannel->sample_playback_rate();

    S_EF2_ArmSampleLoop(pChannel, pSfx, &sfx_infos[pSfx->sfx_info_index]);

    pChannel->set_gain_3d(S_EF2_ChannelVolume(pChannel->iEntChannel) * pChannel->fVolume);
    pChannel->set_sample_offset(pBase->iOffset);
    pChannel->set_sample_playback_rate(pChannel->iBaseRate * pBase->fNewPitchMult);

    if (bStartUnpaused) {
        pChannel->resume_sample();
    } else {
        pChannel->iFlags |= CHANNEL_FLAG_PLAY_DEFERRED;
    }

    if (pChannel->iEntNum != ENTITYNUM_NONE) {
        s_entity[pChannel->iEntNum].time = pChannel->iTime;
        VectorCopy(pChannel->vOrigin, s_entity[pChannel->iEntNum].position);
    }
}

static void S_LoadBase(channelbasesavegame_t *pBase, openal_channel *pChannel, bool bStartUnpaused)
{
    sfxHandle_t handle;

    if (!pChannel) {
        return;
    }

    if (!pBase->bPlaying) {
        return;
    }

    if (strstr(pBase->sfx.szName, "null.wav")) {
        return;
    }

    if (pBase->iEntNum != ENTITYNUM_NONE && (pBase->iEntNum < 0 || pBase->iEntNum >= MAX_GENTITIES)) {
        Com_DPrintf("S_LoadBase: dropping channel with out-of-range entity number %d\n", pBase->iEntNum);
        return;
    }

    handle = S_RegisterSoundExt(pBase->sfx.szName, (pBase->sfx.iFlags & SFX_FLAG_STREAMED), false);

    pChannel->iBaseRate     = pBase->iBaseRate;
    pChannel->iStartTime    = pBase->iStartTime;
    pChannel->iEndTime      = pBase->iEndTime;
    pChannel->iEntChannel   = pBase->iEntChannel;
    pChannel->iEntNum       = pBase->iEntNum;
    pChannel->iFlags        = pBase->iFlags;
    pChannel->fMaxDist      = pBase->fMaxDist;
    pChannel->fMinDist      = pBase->fMinDist;
    pChannel->fNewPitchMult = pBase->fNewPitchMult;
    VectorCopy(pBase->vOrigin, pChannel->vOrigin);
    pChannel->iTime   = pBase->iTime;
    pChannel->fVolume = pBase->fVolume;
    pChannel->pSfx    = &s_knownSfx[handle];

    S_StartSoundFromBase(pBase, pChannel, &s_knownSfx[handle], bStartUnpaused);
}

void S_SaveData(soundsystemsavegame_t *pSave)
{
    int  i;
    bool bSoundWasUnpaused;

    bSoundWasUnpaused = !s_bSoundPaused;
    if (!s_bSoundPaused) {
        S_PauseSound();
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        S_StoreBase(&pSave->Channels[i], openal.channel[i]);
    }

    if (bSoundWasUnpaused) {
        S_UnpauseSound();
    }
}

void S_ReLoad(soundsystemsavegame_t *pSave)
{
    int  i;
    bool bSoundWasUnpaused;

    bSoundWasUnpaused = !s_bSoundPaused;
    if (!s_bSoundPaused) {
        S_PauseSound();
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        S_LoadBase(&pSave->Channels[i], openal.channel[i], bSoundWasUnpaused);
    }

    if (bSoundWasUnpaused) {
        S_UnpauseSound();
    }
}

static void S_InitBase(channelbasesavegame_t *pBase)
{
    if (!pBase->bPlaying) {
        return;
    }

    if (strstr(pBase->sfx.szName, "null.wav")) {
        return;
    }

    SV_AddSvsTimeFixup(&pBase->iStartTime);
    SV_AddSvsTimeFixup(&pBase->iEndTime);
    SV_AddSvsTimeFixup(&pBase->iTime);
}

void S_LoadData(soundsystemsavegame_t *pSave)
{
    int i;

    for (i = 0; i < MAX_SOUNDSYSTEM_POSITION_CHANNELS; i++) {
        S_InitBase(&pSave->Channels[i]);
    }
}

void openal_channel::set_velocity(float v0, float v1, float v2)
{
    qalSource3f(source, AL_VELOCITY, v0, v1, v2);
    alDieIfError();
}

void openal_channel::set_position(float v0, float v1, float v2)
{
    qalSource3f(source, AL_POSITION, v0, v1, v2);
    alDieIfError();
}

void openal_channel::set_gain(float gain)
{
    qalSourcef(source, AL_GAIN, gain * fObstruction * fDistGain);
    alDieIfError();
}

void openal_channel::set_gain_3d(float gain)
{
    set_gain(powf(gain, 5.0f / 3.0f));
}

void openal_channel::set_no_3d()
{
    qalSource3f(source, AL_POSITION, 0, 0, 0);
    alDieIfError();
    qalSource3f(source, AL_VELOCITY, 0, 0, 0);
    alDieIfError();
    qalSourcei(source, AL_SOURCE_RELATIVE, true);
    alDieIfError();
    qalSourcei(source, AL_LOOPING, false);
    alDieIfError();
    qalSourcei(source, AL_ROLLOFF_FACTOR, 0);
    alDieIfError();
    qalSourcef(source, AL_GAIN, S_GetBaseVolume());
    alDieIfError();
}

void openal_channel::set_3d()
{
    qalSourcei(source, AL_SOURCE_RELATIVE, false);
    alDieIfError();
    qalSourcei(source, AL_LOOPING, false);
    alDieIfError();
    qalSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
    alDieIfError();
    qalSourcei(source, AL_DISTANCE_MODEL, AL_INVERSE_DISTANCE_CLAMPED);
    alDieIfError();
    if (fMinDist > 0) {
        qalSourcef(source, AL_REFERENCE_DISTANCE, fMinDist);
        alDieIfError();
    }
    if (fMaxDist > 0) {
        qalSourcef(source, AL_MAX_DISTANCE, fMaxDist);
        alDieIfError();
    }
}

void openal_channel::play()
{
    qalSourcePlay(source);
    alDieIfError();
}

void openal_channel::set_no_virtualization()
{
#if AL_SOFT_direct_channels_remix
    qalSourcei(source, AL_DIRECT_CHANNELS_SOFT, AL_REMIX_UNMATCHED_SOFT);
    alDieIfError();
#elif AL_SOFT_direct_channels
    qalSourcei(source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
    alDieIfError();
#endif
}

void openal_channel::set_virtualization()
{
#if AL_SOFT_direct_channels_remix || AL_SOFT_direct_channels
    qalSourcei(source, AL_DIRECT_CHANNELS_SOFT, AL_FALSE);
    alDieIfError();
#endif
}

void openal_channel::pause()
{
    qalSourcePause(source);
    alDieIfError();
}

void openal_channel::stop()
{
    qalSourceStop(source);
    alDieIfError();
}

bool openal_channel::is_free()
{
    if (bPendingStart) {
        return false;
    }

    ALint state = sample_status();

    return state == AL_INITIAL || state == AL_STOPPED;
}

bool openal_channel::is_paused()
{
    ALint state = sample_status();

    return state == AL_PAUSED;
}

bool openal_channel::is_playing()
{
    ALint state = sample_status();

    return state == AL_PLAYING;
}

void openal_channel::force_free()
{
    stop();
    bPendingStart = qfalse;
    iFlags        = 0;
}

bool openal_channel::set_sfx(sfx_t *pNewSfx)
{
    ALint      freq = 0;
    byte      *pAnalyze = NULL;
    snd_info_t analyzeInfo;
    int        iAnalyzeWidth = 0;

    assert(pNewSfx->length);

    this->pSfx = pNewSfx;
    if (!pNewSfx->buffer || !qalIsBuffer(pNewSfx->buffer)) {
        pNewSfx->al_loop_start = -1;
        pNewSfx->al_loop_end   = -1;

        if (pNewSfx->iFlags & SFX_FLAG_MP3) {
            qalGenBuffers(1, &pNewSfx->buffer);
            alDieIfError();

            if (!S_OPENAL_LoadMP3_Codec(pNewSfx->name, pNewSfx, &pAnalyze, &analyzeInfo)) {
                qalDeleteBuffers(1, &pNewSfx->buffer);
                alDieIfError();

                Com_DPrintf("OpenAL: Failed to load MP3.\n");
                return false;
            }

            alDieIfError();
            iAnalyzeWidth = (int)analyzeInfo.width;
        } else {
            ALenum fmt = S_OPENAL_Format(pNewSfx->info.width, pNewSfx->info.channels);

            if (!fmt) {
                Com_Printf(
                    "OpenAL: Bad Wave file (%d channels, %d bits) [%s].\n",
                    pNewSfx->info.channels,
                    (int)(pNewSfx->info.width * 8),
                    pNewSfx->name
                );
                return false;
            }

            qalGenBuffers(1, &pNewSfx->buffer);
            alDieIfError();

            qalBufferData(
                pNewSfx->buffer,
                fmt,
                &pNewSfx->data[pNewSfx->info.dataofs],
                pNewSfx->info.samples * pNewSfx->info.width * pNewSfx->info.channels,
                pNewSfx->info.rate
            );
            alDieIfError();

            /* resident PCM: analysed in place, never freed here */
            pAnalyze        = &pNewSfx->data[pNewSfx->info.dataofs];
            analyzeInfo.rate     = pNewSfx->info.rate;
            analyzeInfo.width    = (float)pNewSfx->info.width;
            analyzeInfo.channels = pNewSfx->info.channels;
            analyzeInfo.samples  = pNewSfx->info.samples;
            iAnalyzeWidth        = pNewSfx->info.width;
        }
    }

    qalSourceStop(source);
    alDieIfError();

    qalSourcei(source, AL_BUFFER, pNewSfx->buffer);
    alDieIfError();

    qalSourcef(source, AL_PITCH, 1.0);
    alDieIfError();

    qalGetBufferi(pNewSfx->buffer, AL_FREQUENCY, &freq);
    alDieIfError();

    iBaseRate = freq;

    /* the bus is already known: every caller stores iEntChannel before this */
    if (pAnalyze) {
        SndMix_RegisterPCM(
            pNewSfx, pAnalyze, analyzeInfo.samples, analyzeInfo.rate, iAnalyzeWidth, analyzeInfo.channels,
            SndMix_BusForChannel(iEntChannel, (iFlags & CHANNEL_FLAG_LOOP_SOUND) ? qtrue : qfalse));

        if (pNewSfx->iFlags & SFX_FLAG_MP3) {
            Hunk_FreeTempMemory(pAnalyze);
        }
    }

    return true;
}

void openal_channel::start_sample()
{
    play();
}

void openal_channel::stop_sample()
{
    pause();
}

void openal_channel::resume_sample()
{
    play();
}

void openal_channel::end_sample()
{
    stop();
    bPendingStart = qfalse;
}

void openal_channel::set_sample_pan(S32 pan)
{
    float panning = (pan - 64) / 63.f;

    if (panning < -1.f) {
        panning = -1.f;
    } else if (panning > 1.f) {
        panning = 1.f;
    }

    const ALfloat sourcePosition[3] = {panning, 0, sqrtf(1.f - Square(panning))};

    qalSourcef(source, AL_ROLLOFF_FACTOR, 0);
    qalSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    qalSourcefv(source, AL_POSITION, sourcePosition);
}

void openal_channel::set_sample_playback_rate(S32 rate)
{
    qalSourcef(source, AL_PITCH, rate / (float)iBaseRate);
    alDieIfError();
}

S32 openal_channel::sample_playback_rate()
{
    float pitch = 1;

    qalGetSourcef(source, AL_PITCH, &pitch);
    alDieIfError();

    return buffer_frequency() * pitch;
}

U32 openal_channel::buffer_frequency() const
{
    ALint freq = 0;

    if (buffer) {
        qalGetBufferi(buffer, AL_FREQUENCY, &freq);
        alDieIfError();
    } else if (pSfx) {
        qalGetBufferi(pSfx->buffer, AL_FREQUENCY, &freq);
        alDieIfError();
    } else {
        freq = 22050;
    }

    return freq;
}

S32 openal_channel::sample_volume()
{
    ALfloat gain = 0;

    qalGetSourcef(source, AL_GAIN, &gain);
    alDieIfError();

    return (gain / (S_GetBaseVolume() * s_fFadeVolume)) * 100;
}

U32 openal_channel::sample_offset()
{
    ALint offset = 0;

    qalGetSourcei(source, AL_SAMPLE_OFFSET, &offset);
    alDieIfError();

    return offset;
}

U32 openal_channel::sample_ms_offset()
{
    if (!is_playing()) {
        return 0;
    }

    S32 rate = sample_playback_rate();
    if (!rate) {
        return 0;
    }

    return sample_offset() * 1000ull / rate;
}

U32 openal_channel::sample_loop_count()
{
    ALuint queued;
    ALuint processed;
    S32    left;

    qalGetSourceiv(source, AL_BUFFERS_QUEUED, (ALint *)&queued);
    alDieIfError();
    qalGetSourcei(source, AL_BUFFERS_PROCESSED, (ALint *)&processed);
    alDieIfError();

    left = queued + ~processed;
    if (left < 0) {
        left = 0;
    }
    return left;
}

void openal_channel::set_sample_offset(U32 offset)
{
    qalSourcei(source, AL_SAMPLE_OFFSET, offset);
    alDieIfError();
}

void openal_channel::set_sample_ms_offset(U32 offset)
{
    set_sample_offset(offset * sample_playback_rate() / 1000);
}

void openal_channel::set_sample_loop_count(S32 count)
{
    ALuint processed = 0;

    stop();

    qalGetSourceiv(source, AL_BUFFERS_PROCESSED, (ALint *)&processed);
    alDieIfError();

    for (ALuint i = 0; i < processed; i++) {
        ALuint bufName;

        qalSourceUnqueueBuffers(source, 1, &bufName);
        alDieIfError();
    }

    if (buffer) {
        for (S32 i = 0; i < count; i++) {
            qalSourceQueueBuffers(source, 1, &buffer);
            alDieIfError();
        }
    }

    if (count == 0) {
        qalSourcei(source, AL_LOOPING, true);
        alDieIfError();
    } else {
        qalSourcei(source, AL_LOOPING, false);
        alDieIfError();
    }
}

void openal_channel::set_sample_loop_block(S32 start_offset, S32 end_offset)
{
    ALint points[2];

    if (!qalBufferiv || !pSfx || !pSfx->buffer || !qalIsBuffer(pSfx->buffer)) {
        return;
    }
    if (start_offset < 0) {
        return;
    }
    if (end_offset < 0) {
        end_offset = pSfx->length;
    }
    if (end_offset > pSfx->length) {
        end_offset = pSfx->length;      
    }
    if (start_offset >= end_offset) {
        return;                          
    }
    if (pSfx->al_loop_start == start_offset && pSfx->al_loop_end == end_offset) {
        return;                          
    }

    qalSourceStop(source);
    alDieIfError();
    qalSourcei(source, AL_BUFFER, 0);
    alDieIfError();

    points[0] = start_offset;
    points[1] = end_offset;
    qalBufferiv(pSfx->buffer, AL_LOOP_POINTS_SOFT, points);
    alDieIfError(); 

    qalSourcei(source, AL_BUFFER, pSfx->buffer);
    alDieIfError();

    pSfx->al_loop_start = start_offset;
    pSfx->al_loop_end   = end_offset;
}

U32 openal_channel::sample_status()
{
    ALint retval;

    qalGetSourcei(source, AL_SOURCE_STATE, &retval);
    alDieIfError();

    return retval;
}

qboolean MUSIC_LoadSoundtrackFile(const char *filename)
{
    song_t *psong = NULL;
    // register's "retail token area larger" reading was about com_token, not args.  retail's
    char    args[MAX_MUSIC_SONGS][MAX_RES_NAME];
    int     numargs;
    char    com_token[1024];
    char    alias[128];
    char    file[128];
    char    load_path[128];
    char   *buffer;
    char    path[MAX_RES_NAME];
    int     i;
    byte   *data;

    if (strrchr(filename, '.')) {
        Com_sprintf(path, sizeof(path), "%s", filename);
    } else {
        Com_sprintf(path, sizeof(path), "%s.mus", filename);
    }

    FS_ReadFile(path, (void **)&data);
    if (!data) {
        Com_WDPrintf("Couldn't load %s\n", path);
        return false;
    }
    Com_DPrintf("SOUNDTRACK: Loading %s\n", path);

    MUSIC_StopAllSongs();

    for (i = 0; i < MAX_MUSIC_SONGS; i++) {
        music_songs[i] = song_t{};
    }
    music_numsongs = 0;

    buffer       = (char *)data;
    load_path[0] = 0;

    while (1) {
        Q_strncpyz(com_token, COM_GetToken(&buffer, true), sizeof(com_token));
        if (!com_token[0]) {
            break;
        }

        if (strlen(com_token) >= MAX_RES_NAME) {
            Com_WPrintf("MUSIC_LoadSoundtrackFile: argument too long, truncating in %s\n", path);
            com_token[MAX_RES_NAME - 1] = 0;
        }

        numargs = 1;
        Q_strncpyz(args[0], com_token, sizeof(args[0]));

        while (1) {
            Q_strncpyz(com_token, COM_GetToken(&buffer, false), sizeof(com_token));
            if (!com_token[0]) {
                break;
            }

            if (strlen(com_token) >= MAX_RES_NAME) {
                Com_WPrintf("MUSIC_LoadSoundtrackFile: argument too long, truncating in %s\n", path);
                com_token[MAX_RES_NAME - 1] = 0;
            }

            Q_strncpyz(args[numargs], com_token, sizeof(args[numargs]));
            numargs++;
        }

        if (!Q_stricmp(args[0], "path")) {
            Q_strncpyz(load_path, args[1], sizeof(load_path));
            // unconditionally, so retail's index-(-1) read on an empty path is not performed
            Q_strcat(load_path, sizeof(load_path), "/");
        } else if (args[0][0] == '!') {
            for (i = 0; i < music_numsongs; i++) {
                psong = &music_songs[i];
                if (!Q_stricmp(psong->alias, &args[0][1])) {
                    break;
                }
            }

            if (i == music_numsongs) {
                Com_WPrintf("MUSIC_LoadSoundtrackFile: song %s not found, command skipped in %s.\n", &args[0][1], path);
                continue;
            }

            if (!Q_stricmp(args[1], "volume")) {
                psong->volume = atof(args[2]);
            } else if (!Q_stricmp(args[1], "fadetime")) {
                psong->fadetime = atof(args[2]);
            } else if (!Q_stricmp(args[1], "loop")) {
                psong->flags |= 1;
            } else if (!Q_stricmp(args[1], "restart")) {
                psong->flags |= 2;
            } else if (!Q_stricmp(args[1], "interrupt")) {
                psong->fadetime = 0;
                psong->flags |= 4 | 2;
            } else {
                Com_WPrintf(
                    "MUSIC_LoadSoundtrackFile: unknown command %s for song %s in %s.\n", args[1], &args[0][1], path
                );
            }
        } else {
            if (numargs > 1) {
                Q_strncpyz(alias, args[0], sizeof(alias));
                Q_strncpyz(file, load_path, sizeof(file));
                Q_strcat(file, sizeof(file), args[1]);
            } else {
                int len;

                Q_strncpyz(file, load_path, sizeof(file));
                Q_strcat(file, sizeof(file), args[1]);

                len = (int)strlen(args[0]);
                strncpy(alias, args[0], len - 4);
                alias[len - 4] = 0;
            }

            if (music_numsongs >= MAX_MUSIC_SONGS) {
                Com_WPrintf("MUSIC_LoadSoundtrackFile: Too many songs in %s, skipping %s.\n", path, alias);
                continue;
            }

            psong = &music_songs[music_numsongs];
            Q_strncpyz(psong->alias, alias, sizeof(psong->alias));
            Q_strncpyz(psong->path, file, sizeof(psong->path));
            music_songs[music_numsongs].fadetime    = 1.0;
            music_songs[music_numsongs].volume      = 1.0;
            music_songs[music_numsongs].flags       = 0;
            music_songs[music_numsongs].current_pos = 0;
            music_songs[music_numsongs].mood_num    = MusicMood_NameToNum(alias);
            music_numsongs++;
        }
    }

    if (!music_numsongs) {
        Com_WPrintf("MUSIC_LoadSoundtrackFile: could not load %s, no songs defined.\n", path);
        FS_FreeFile(data);
        return false;
    }

    music_currentsong = -1;
    FS_FreeFile(data);

    if (music_current_mood == mood_none) {
        MUSIC_UpdateMood(mood_normal, mood_normal);
    }

    return true;
}

qboolean MUSIC_SongValid(const char *mood)
{
    return MUSIC_FindSong(mood) != -1;
}

qboolean MUSIC_Loaded()
{
    return music_loaded;
}

void Music_Update()
{
    int currentsong;

    if (!s_bSoundStarted) {
        return;
    }

    if (!music_active) {
        return;
    }

    if (s_bSoundPaused) {
        return;
    }

    MUSIC_CheckForStoppedSongs();

    if (!MUSIC_Loaded() && music_active && strlen(current_soundtrack)) {
        MUSIC_LoadSoundtrackFile(current_soundtrack);
        music_loaded = true;
    }

    if (music_currentsong >= 0) {
        currentsong = music_songs[music_currentsong].mood_num;
    } else {
        currentsong = -1;
    }

    if (music_current_mood == mood_none) {
        if (MUSIC_Playing()) {
            MUSIC_StopAllSongs();
        }
    } else if (music_current_mood != currentsong) {
        const char *mood = MusicMood_NumToName(music_current_mood);
        if (MUSIC_SongValid(mood) && MUSIC_Loaded() && strlen(current_soundtrack)) {
            Com_DebugPrintf("Playing %s.\n", mood);
            MUSIC_PlaySong(mood);
        }
    }

    if (new_music_volume != music_volume) {
        if (music_volume_fade_time > 0) {
            if (music_volume_direction == 0) {
                music_volume = (cls.realtime - music_volume_start_time) * (new_music_volume - old_music_volume)
                                 / (music_volume_fade_time * 1000.f)
                             + old_music_volume;

                if (music_volume > new_music_volume) {
                    music_volume         = new_music_volume;
                    music_volume_changed = 1;
                } else {
                    music_volume_changed = 1;
                }
            } else if (music_volume_direction == 1) {
                music_volume = 1.0
                             - (cls.realtime - music_volume_start_time) * (old_music_volume - new_music_volume)
                                   / (music_volume_fade_time * 1000.f);

                if (music_volume >= new_music_volume) {
                    music_volume_changed = true;
                } else {
                    music_volume         = new_music_volume;
                    music_volume_changed = true;
                }
            } else {
                music_volume_changed = true;
            }
        } else {
            music_volume         = new_music_volume;
            music_volume_changed = true;
        }
    }

    MUSIC_UpdateMusicVolumes();
}

void MUSIC_SongEnded()
{
    Com_DPrintf(
        "MUSIC: Song ended, changing from [ %s ] to [ %s ]\n",
        MusicMood_NumToName(music_current_mood),
        MusicMood_NumToName(music_fallback_mood)
    );
    music_current_mood = music_fallback_mood;
}

void MUSIC_NewSoundtrack(const char *name)
{
    if (!s_bSoundStarted) {
        return;
    }

    if (!Q_stricmp(name, current_soundtrack)) {
        return;
    }

    Q_strncpyz(current_soundtrack, name, sizeof(current_soundtrack));

    if (!Q_stricmp(current_soundtrack, "none") || !Q_stricmp(current_soundtrack, "")) {
        music_active = qfalse;
        if (MUSIC_Playing()) {
            MUSIC_StopAllSongs();
        }
    } else {
        music_active = qtrue;
        MUSIC_LoadSoundtrackFile(name);
        music_loaded = qtrue;
    }
}

void MUSIC_UpdateMood(int current, int fallback)
{
    static int last_current_mood  = -1;
    static int last_fallback_mood = -1;
    static int current_mood       = -1;
    static int fallback_mood      = -1;
    qboolean   was_action;

    if (current == current_mood && fallback == fallback_mood) {
        return;
    }

    was_action          = current == last_current_mood && fallback == last_fallback_mood && current_mood == mood_action;
    last_current_mood   = current_mood;
    last_fallback_mood  = fallback_mood;
    current_mood        = current;
    music_current_mood  = current;
    fallback_mood       = fallback;
    music_fallback_mood = fallback;

    // mood play for a soundtrack retail deliberately suppresses.

    Com_DPrintf(
        "MUSIC: changing from [ %s | %s ] to [ %s | %s ]\n",
        MusicMood_NumToName(last_current_mood),
        MusicMood_NumToName(last_fallback_mood),
        MusicMood_NumToName(current_mood),
        MusicMood_NumToName(fallback)
    );

    if (was_action) {
        int songnum = MUSIC_FindSong(MusicMood_NumToName(current_mood));

        if (songnum != -1 && (music_songs[songnum].flags & 4)) {
            Com_DPrintf(
                "MUSIC: restoring music from action state, skipping [ %s ] for [ %s ]\n",
                MusicMood_NumToName(current_mood),
                MusicMood_NumToName(fallback_mood)
            );
            music_current_mood = music_fallback_mood;
        }
    }
}

void MUSIC_UpdateDucking(int ducking)
{
    /* the plumbing was live and its consumer was not: this used to gate a
       multiply by `s_fDuckVolume', which never moved off 1.0.  It now gates the
       music bus's own depth in the mixer. */
    SndMix_SetMusicDucking(ducking);
}

void MUSIC_UpdateVolume(float volume, float fade_time)
{
    if (new_music_volume == volume && music_volume_fade_time == fade_time) {
        return;
    }

    old_music_volume        = music_volume;
    new_music_volume        = volume;
    music_volume_fade_time  = fade_time;
    music_volume_start_time = cls.realtime;

    if (volume > music_volume) {
        music_volume_direction = 0;
    } else if (volume < music_volume) {
        music_volume_direction = 1;
    }
}

void MUSIC_StopAllSongs()
{
    for (int i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        MUSIC_StopChannel(i);
    }

    music_currentsong = -1;
}

void MUSIC_FreeAllSongs()
{
    MUSIC_StopAllSongs();
    MUSIC_UpdateMood(mood_none, mood_none);
    current_soundtrack[0] = 0;
    music_loaded          = false;

}

qboolean MUSIC_Playing()
{
    return MUSIC_CurrentSongChannel() != -1;
}

int MUSIC_FindSong(const char *name)
{
    int i;

    for (i = 0; i < music_numsongs; i++) {
        if (!Q_stricmp(music_songs[i].alias, name)) {
            return i;
        }
    }

    return -1;
}

void S_loadsoundtrack()
{
    if (Cmd_Argc() != 2) {
        Com_Printf("loadsoundtrack <sound track file>\n");
        return;
    }

    music_loaded = MUSIC_LoadSoundtrackFile(Cmd_Argv(1)) ? true : music_loaded;
}

const char *S_CurrentSoundtrack()
{
    return current_soundtrack;
}

void S_PlaySong()
{
    if (Cmd_Argc() != 2) {
        Com_Printf("playsong <song alias>\n");
        return;
    }

    MUSIC_PlaySong(Cmd_Argv(1));
}

int MUSIC_CurrentSongChannel()
{
    int channel_number = -1;
    int ch_idx         = 0;

    for (ch_idx = 0; ch_idx < MAX_SOUNDSYSTEM_SONGS; ch_idx++) {
        if (openal.chan_song[ch_idx].is_playing() && openal.chan_song[ch_idx].song_number == music_currentsong) {
            channel_number = ch_idx;
        }
    }

    return channel_number;
}

void MUSIC_StopChannel(int channel_number)
{
    openal_channel *channel = &openal.chan_song[channel_number];

    if (channel->is_playing()) {
        if (music_songs[channel->song_number].flags & 2) {
            music_songs[channel->song_number].current_pos = 0;
        } else {
            music_songs[channel->song_number].current_pos = channel->sample_ms_offset();
        }

        channel->song_in_use = qfalse;
    }

    channel->stop();
}

qboolean MUSIC_PlaySong(const char *alias)
{
    int                          channel_number;
    song_t                      *song;
    int                          songnum;
    int                          channel_to_play_on;
    int                          fading_song;
    openal_channel_two_d_stream *song_channel;
    int                          rate;

    fading_song = 0;
    songnum     = MUSIC_FindSong(alias);

    if (songnum == -1) {
        return true;
    }

    song = &music_songs[songnum];

    if (!S_RegisterSound(song->path)) {
        Com_DPrintf("Could not start music file '%s'!", song->path);
        return false;
    }

    if (MUSIC_Playing() && songnum == music_currentsong) {
        return true;
    }

    channel_number    = MUSIC_CurrentSongChannel();
    music_currentsong = songnum;

    if (channel_number != -1) {
        if (song->flags & 4) {
            MUSIC_StopChannel(channel_number);
        } else {
            song_channel                  = &openal.chan_song[channel_number];
            song_channel->fading          = FADE_OUT;
            song_channel->fade_time       = song->fadetime;
            song_channel->fade_start_time = cls.realtime;
            fading_song                   = true;
        }
    }
    channel_to_play_on = (fading_song != 0) && (channel_number == 0);
    song_channel       = &openal.chan_song[channel_to_play_on];

    if (song_channel->is_playing() || song_channel->is_paused()) {
        MUSIC_StopChannel(channel_to_play_on);
    }

    if (!song_channel->queue_stream(song->path)) {
        Com_DPrintf("Could not start music file '%s'!", song->path);
        return false;
    }

    rate = song_channel->sample_playback_rate();

    song_channel->song_number = songnum;
    song_channel->song_in_use = qtrue;
    if (song->current_pos) {
        song_channel->set_sample_ms_offset(song->current_pos);
    } else {
        song_channel->set_sample_offset(rate * 0.063f);
    }

    if (song->flags & 1) {
        song_channel->set_sample_loop_count(0);
        song_channel->set_sample_loop_block(rate * 0.063f, -1);
        song_channel->song_looping = qtrue;
    } else {
        song_channel->set_sample_loop_count(1);
        song_channel->song_looping = qfalse;
    }

    if (fading_song) {
        song_channel->fading    = FADE_IN;
        song_channel->fade_time = song->fadetime;
        song_channel->set_gain(0.0);
        song_channel->fade_start_time = cls.realtime;
    } else {
        song_channel->fading = FADE_NONE;
        song_channel->set_gain_3d(S_EF2_ChannelVolume(CHAN_MUSIC) * song->volume);
    }

    song_channel->play();

    return true;
}

void MUSIC_UpdateMusicVolumes()
{
    int   i;
    float new_volume, max_volume;

    if (s_musicVolume->modified) {
        s_musicVolume->modified = false;
        music_volume_changed    = true;
    }

    if (music_volume_changed) {
        music_volume_changed = false;

        for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
            if (!openal.chan_song[i].is_playing() && !openal.chan_song[i].is_paused()) {
                continue;
            }

            if (openal.chan_song[i].fading != FADE_NONE) {
                continue;
            }

            openal.chan_song[i].set_gain_3d(
                S_EF2_ChannelVolume(CHAN_MUSIC) * music_songs[openal.chan_song[i].song_number].volume * music_volume
            );
        }

        // Added in OPM, kept: chan_trig_music is not part of retail's two-entry song pool
        if (openal.chan_trig_music.is_playing() || openal.chan_trig_music.is_paused()) {
            openal.chan_trig_music.set_gain_3d(
                (S_GetBaseVolume() * s_fFadeVolume) * (openal.chan_trig_music.fVolume * s_musicVolume->value) * s_fVolumeGain
            );
        }
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        if (!openal.chan_song[i].is_playing() && !openal.chan_song[i].is_paused()) {
            continue;
        }

        switch (openal.chan_song[i].fading) {
        case fade_t::FADE_IN:
            max_volume = S_EF2_ChannelVolume(CHAN_MUSIC) * music_songs[openal.chan_song[i].song_number].volume;
            new_volume = (unsigned int)(cls.realtime - openal.chan_song[i].fade_start_time)
                       / (openal.chan_song[i].fade_time * 1000.f) * max_volume;

            if (new_volume > max_volume) {
                openal.chan_song[i].set_gain_3d(max_volume * music_volume);
                openal.chan_song[i].fading = FADE_NONE;
            } else {
                openal.chan_song[i].set_gain_3d(new_volume * music_volume);
            }
            break;
        case fade_t::FADE_OUT:
            max_volume = S_EF2_ChannelVolume(CHAN_MUSIC) * music_songs[openal.chan_song[i].song_number].volume;
            new_volume = max_volume
                       - (unsigned int)(cls.realtime - openal.chan_song[i].fade_start_time)
                             / (openal.chan_song[i].fade_time * 1000.f) * max_volume;

            if (new_volume > 0) {
                openal.chan_song[i].set_gain_3d(new_volume * music_volume);
            } else {
                MUSIC_StopChannel(i);
            }
            break;
        default:
            break;
        }
    }
}

void MUSIC_CheckForStoppedSongs()
{
    int i;

    // is a genuine retail out-of-bounds read, but it is not reproducible: it lands on
    if (s_bSoundPaused) {
        return;
    }

    for (i = 0; i < MAX_SOUNDSYSTEM_SONGS; i++) {
        openal_channel_two_d_stream *song_channel = &openal.chan_song[i];
        unsigned int                 lengthMs, posMs;
        int                          songnum;
        float                        fadetime;

        if (!song_channel->song_in_use) {
            continue;
        }

        if (song_channel->song_looping) {
            continue;
        }

        lengthMs = song_channel->sample_ms_length();
        posMs    = song_channel->sample_ms_offset();

        songnum  = MUSIC_FindSong(MusicMood_NumToName(music_fallback_mood));
        fadetime = (songnum != -1) ? music_songs[songnum].fadetime : 0.f;

        if ((float)posMs + 1000.f * fadetime >= (float)lengthMs || !song_channel->is_playing()) {
            song_channel->song_in_use = qfalse;
            MUSIC_SongEnded();
        }
    }
}

void S_TriggeredMusic_SetupHandle(const char *pszName, int iLoopCount, int iOffset, qboolean autostart)
{
    char       *pszFilename;
    const char *pszRealName;
    float       fVolume = 1.0;

    if (!s_bSoundStarted) {
        return;
    }

    if (openal.chan_trig_music.is_playing() || openal.chan_trig_music.is_paused()) {
        Com_DPrintf("Didn't start new triggered music because some was already playing\n");
        return;
    }

    openal.chan_trig_music.stop();
    Q_strncpyz(openal.tm_filename, pszName, sizeof(openal.tm_filename));

    openal.tm_loopcount = iLoopCount;
    openal.chan_trig_music.set_sample_loop_count(iLoopCount);
    pszRealName = Alias_FindRandom(pszName);

    if (!pszRealName) {
        pszRealName = pszName;
    }

    pszFilename = FS_BuildOSPath(Cvar_VariableString("fs_basepath"), FS_Gamedir(), pszRealName);
    (void)pszFilename;

    if (!openal.chan_trig_music.queue_stream(pszRealName)) {
        S_OPENAL_InitChannel(SOUNDSYSTEM_CHANNEL_TRIGGER_MUSIC_ID, &openal.chan_trig_music);
        Com_DPrintf("Could not start triggered music '%s'\n", pszName);
        return;
    }

    openal.chan_trig_music.fVolume = fVolume;
    openal.chan_trig_music.set_gain_3d((S_GetBaseVolume() * s_fFadeVolume) * (fVolume * s_musicVolume->value) * s_fVolumeGain);
    openal.chan_trig_music.set_sample_loop_count(iLoopCount);
    openal.chan_trig_music.set_sample_offset(iOffset);

    openal.chan_trig_music.play();

    if (!autostart) {
        openal.chan_trig_music.pause();
    }
}

void S_TriggeredMusic_Start()
{
    if (Cmd_Argc() != 2) {
        Com_Printf("tmstart <sound file>\n");
        return;
    }

    S_TriggeredMusic_SetupHandle(Cmd_Argv(1), 1, 0, true);
}

void S_TriggeredMusic_StartLoop()
{
    if (Cmd_Argc() != 2) {
        Com_Printf("tmstartloop <sound file>\n");
        return;
    }

    S_TriggeredMusic_SetupHandle(Cmd_Argv(1), 0, 0, true);
}

void S_TriggeredMusic_Stop()
{
    if (!s_bSoundStarted) {
        return;
    }

    openal.chan_trig_music.stop();
    openal.tm_filename[0] = 0;
    openal.tm_loopcount   = 0;
}

void S_TriggeredMusic_Volume()
{
    if (!s_bSoundStarted) {
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("tmvolume <0-1>\n\n");
        return;
    }

    openal.chan_trig_music.fVolume = atof(Cmd_Argv(1));
    music_volume_changed           = true;
}

void S_TriggeredMusic_Pause()
{
    if (!s_bSoundStarted) {
        return;
    }

    if (openal.chan_trig_music.is_playing()) {
        openal.chan_trig_music.pause();
    }
}

void S_TriggeredMusic_Unpause()
{
    if (!s_bSoundStarted) {
        return;
    }

    if (openal.chan_trig_music.is_paused()) {
        openal.chan_trig_music.play();
    }

    openal.chan_trig_music.set_gain_3d(
        (S_GetBaseVolume() * s_fFadeVolume) * (openal.chan_trig_music.fVolume * s_musicVolume->value) * s_fVolumeGain
    );
}

void S_TriggeredMusic_PlayIntroMusic()
{
    void *buf;
    if (FS_ReadFileEx("sound/music/mus_MainTheme.mp3", &buf, qtrue) < 0) {
        return;
    }
    FS_FreeFile(buf);
    S_TriggeredMusic_SetupHandle("sound/music/mus_MainTheme.mp3", 0, 0, true);
}

void S_StopMovieAudio()
{
    openal.chan_movie.stop();
}

static ALuint s_rawSource = 0;

static int      s_rawRate          = 22050; 
static int      s_rawBytesPerFrame = 4;     
static unsigned s_rawSamplesFed    = 0;     
static unsigned s_rawSamplesPlayed = 0;     
static unsigned s_rawClockMs       = 0;     
static int      s_rawUnderruns     = 0;     

static void S_RawResetClock(void)
{
    s_rawSamplesFed    = 0;
    s_rawSamplesPlayed = 0;
    s_rawClockMs       = 0;
    s_rawUnderruns     = 0;
}

void S_StopRawSamples(void)
{
    ALint queued = 0;

    S_RawResetClock();

    if (!s_rawSource) {
        return;
    }

    qalSourceStop(s_rawSource);
    qalGetSourcei(s_rawSource, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0) {
        ALuint b = 0;
        qalSourceUnqueueBuffers(s_rawSource, 1, &b);
        qalDeleteBuffers(1, &b);
    }
    qalDeleteSources(1, &s_rawSource);
    s_rawSource = 0;
}

void S_RawSamples(
    int stream, int samples, int rate, int width, int channels, const byte *data, float volume, int entnum
)
{
    ALint  processed = 0, state = 0;
    ALuint buffer    = 0;
    ALuint format;

    if (!al_initialized || !s_bSoundStarted || samples <= 0 || !data) {
        return;
    }

    if (!s_rawSource) {
        qalGenSources(1, &s_rawSource);
        if (!s_rawSource) {
            return;
        }
        qalSourcei(s_rawSource, AL_SOURCE_RELATIVE, AL_TRUE);
        qalSource3f(s_rawSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
        S_RawResetClock();
    }

    s_rawRate          = rate;
    s_rawBytesPerFrame = width * channels;

    qalGetSourcei(s_rawSource, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint b    = 0;
        ALint  size = 0;
        qalSourceUnqueueBuffers(s_rawSource, 1, &b);
        if (s_rawBytesPerFrame > 0) {
            qalGetBufferi(b, AL_SIZE, &size);
            if (size > 0) {
                s_rawSamplesPlayed += (unsigned)(size / s_rawBytesPerFrame);
            }
        }
        qalDeleteBuffers(1, &b);
    }

    {
        ALint queuedNow = 0;
        qalGetSourcei(s_rawSource, AL_BUFFERS_QUEUED, &queuedNow);
        if (queuedNow == 0 && s_rawSamplesFed > s_rawSamplesPlayed) {
            s_rawUnderruns++;
            if (s_rawUnderruns <= 20) {
                Com_Printf("[GLES3:snd] raw stream underrun #%d (fed=%u played=%u rate=%d)\n",
                           s_rawUnderruns, s_rawSamplesFed, s_rawSamplesPlayed, s_rawRate);
            }
        }
    }

    format = S_OPENAL_Format((float)width, channels);
    qalGenBuffers(1, &buffer);
    qalBufferData(buffer, format, (void *)data, samples * width * channels, rate);
    qalSourceQueueBuffers(s_rawSource, 1, &buffer);
    s_rawSamplesFed += (unsigned)samples;

    qalSourcef(s_rawSource, AL_GAIN, powf(S_EF2_ChannelVolume(CHAN_CINEMATIC) * volume, 5.0f / 3.0f));

    qalGetSourcei(s_rawSource, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        qalSourcePlay(s_rawSource);
    }
}

void S_SetupMovieAudio(const char *pszMovieName)
{
    char filename[MAX_QPATH];
    char base[MAX_QPATH];

    S_StopMovieAudio();

    COM_StripExtension(pszMovieName, base, sizeof(base));

    Com_sprintf(filename, sizeof(filename), "%s.mp3", base);
    if (FS_ReadFileEx(filename, NULL, qtrue) <= 0) {
        Com_sprintf(filename, sizeof(filename), "%s.wav", base);
        if (FS_ReadFileEx(filename, NULL, qtrue) <= 0) {
            Com_DPrintf("No separate audio track for movie %s (using embedded)\n", pszMovieName);
            return;
        }
    }

    if (!openal.chan_movie.queue_stream(filename)) {
        return;
    }

    openal.chan_movie.set_gain_3d(S_EF2_ChannelVolume(CHAN_CINEMATIC));
    openal.chan_movie.play();

    Com_DPrintf("Movie Audio setup: %s\n", filename);
}

int S_CurrentMoviePosition()
{
    if (s_rawSource && s_rawSamplesFed > 0 && s_rawRate > 0) {
        unsigned ms = (unsigned)(((unsigned long long)s_rawSamplesPlayed * 1000ull) /
                                 (unsigned long long)s_rawRate);

        if (ms > s_rawClockMs) {
            s_rawClockMs = ms;
        }
        return (int)s_rawClockMs;
    }

    return openal.chan_movie.sample_ms_offset();
}

static ALuint S_OPENAL_Format(float width, int channels)
{
    ALuint format = AL_FORMAT_MONO16;

    if (width == 1) {
        if (channels == 1) {
            format = AL_FORMAT_MONO8;
        } else if (channels == 2) {
            format = AL_FORMAT_STEREO8;
        }
    } else if (width == 2) {
        if (channels == 1) {
            format = AL_FORMAT_MONO16;
        } else if (channels == 2) {
            format = AL_FORMAT_STEREO16;
        }
    } else if (width == 0.5) {
        if (ima4_ext && soft_block_align) {
            if (channels == 1) {
                format = AL_FORMAT_MONO_IMA4;
            } else if (channels == 2) {
                format = AL_FORMAT_STEREO_IMA4;
            }
        } else {
            format = 0;
        }
    }

    return format;
}

static bool S_OPENAL_LoadMP3_Codec(const char *_path, sfx_t *pSfx, byte **ppPcm, snd_info_t *pInfo)
{
    void      *data;
    snd_info_t info;
    ALuint     format;

    data = S_CodecLoad(_path, &info);
    if (!data) {
        return false;
    }

    format = S_OPENAL_Format(info.width, info.channels);

    qalGenBuffers(1, &pSfx->buffer);
    alDieIfError();

    if (info.size == 0) {
        byte dummyData[2] = {0};

        qalBufferData(pSfx->buffer, AL_FORMAT_MONO16, (void *)dummyData, 2, 22050);
    } else {
        qalBufferData(pSfx->buffer, format, data, info.size, info.rate);
    }

    alDieIfError();

    /* the decoded PCM is handed out and freed by the caller, which registers it
       with the mixer before letting it go */
    if (ppPcm && pInfo) {
        *ppPcm  = (byte *)data;
        *pInfo  = info;
    } else {
        Hunk_FreeTempMemory(data);
    }

    return true;
}

void openal_channel::update() {}

void openal_channel_two_d_stream::stop()
{
    openal_channel::stop();

    clear_stream();
}

bool openal_channel_two_d_stream::set_sfx(sfx_t *pNewSfx)
{
    snd_stream_t *stream;
    unsigned int  bytesToRead, bytesRead;
    char          rawData[MAX_BUFFER_SAMPLES * 2 * 2];

    stop();

    sampleLoopCount = 1;

    this->pSfx = pNewSfx;
    Q_strncpyz(this->fileName, pNewSfx->name, sizeof(this->fileName));

    streamHandle = S_CodecLoad(pNewSfx->name, NULL);
    if (!streamHandle) {
        Com_DPrintf("OpenAL: Failed to load sound file.\n");
        return false;
    }

    stream              = (snd_stream_t *)streamHandle;
    pNewSfx->info.channels = stream->info.channels;
    pNewSfx->info.dataofs  = stream->info.dataofs;
    pNewSfx->info.rate     = stream->info.rate;
    pNewSfx->info.samples  = stream->info.samples;
    pNewSfx->info.width    = stream->info.width;

    pNewSfx->info.format = S_OPENAL_Format(stream->info.width, stream->info.channels);
    if (!pNewSfx->info.format) {
        Com_DPrintf(
            "OpenAL: Bad Wave file (%d channels, %d bits) [%s].\n",
            pNewSfx->info.channels,
            (int)(pNewSfx->info.width * 8.f),
            pNewSfx->name
        );

        S_CodecCloseStream(stream);
        return false;
    }

    qalSourceStop(source);
    alDieIfError();

    qalGenBuffers(MAX_STREAM_BUFFERS, buffers);
    alDieIfError();

    bytesToRead      = Q_min(MAX_BUFFER_SAMPLES * stream->info.width * stream->info.channels, sizeof(rawData));
    bytesRead        = S_CodecReadStream(stream, bytesToRead, rawData);
    streamNextOffset = bytesRead;
    if (!bytesRead) {
        S_CodecCloseStream(stream);
        return true;
    }

#if AL_SOFT_block_alignment
    if (stream->info.dataalign > 1 && soft_block_align) {
        qalBufferi(buffers[currentBuf], AL_UNPACK_BLOCK_ALIGNMENT_SOFT, stream->info.dataalign);
        alDieIfError();
    }
#endif

    qalBufferData(buffers[currentBuf], pNewSfx->info.format, rawData, bytesRead, stream->info.rate);
    alDieIfError();

    qalSourceQueueBuffers(source, 1, &buffers[currentBuf]);
    alDieIfError();

    iBaseRate  = stream->info.rate;
    currentBuf = (currentBuf + 1) % MAX_STREAM_BUFFERS;
    streaming  = true;
    sampleLengthMs =
        stream->info.rate ? (unsigned int)((int64_t)stream->info.samples * 1000 / stream->info.rate) : 0;

    return true;
}

void openal_channel_two_d_stream::set_sample_loop_count(S32 count)
{
    sampleLoopCount = count;
    sampleLooped    = 0;
}

openal_channel_two_d_stream::openal_channel_two_d_stream()
{
    int i;

    streamHandle = NULL;
    for (i = 0; i < MAX_STREAM_BUFFERS; i++) {
        buffers[i] = 0;
    }
    currentBuf       = 0;
    sampleLoopCount  = 0;
    sampleLooped     = 0;
    streamNextOffset = 0;
    sampleLengthMs   = 0;
    streaming        = false;
}

openal_channel_two_d_stream::~openal_channel_two_d_stream()
{
    clear_stream();
}

void openal_channel_two_d_stream::update()
{
    snd_stream_t *stream = (snd_stream_t *)streamHandle;
    unsigned int  bytesToRead, bytesRead;
    ALuint        format;
    ALint         numProcessedBuffers = 0, numQueuedBuffers = 0;
    ALint         state = sample_status();
    char rawData[MAX_BUFFER_SAMPLES * 2 * 2];

    if (!streaming || is_paused() || state == AL_INITIAL) {
        return;
    }

    qalGetSourcei(source, AL_BUFFERS_PROCESSED, &numProcessedBuffers);
    alDieIfError();

    qalGetSourcei(source, AL_BUFFERS_QUEUED, &numQueuedBuffers);
    alDieIfError();

    if (numProcessedBuffers >= Q_max(numQueuedBuffers / 2, 1)) {
        while (numProcessedBuffers-- > 0) {
            ALuint tmpBuffer;
            qalSourceUnqueueBuffers(source, 1, &tmpBuffer);
        }
    }

    if (numQueuedBuffers >= MAX_STREAM_BUFFERS) {
        return;
    }

    if (sampleLoopCount && sampleLooped >= sampleLoopCount) {
        if (!is_playing()) {
            clear_stream();
        }
        return;
    }

    bytesToRead = Q_min(MAX_BUFFER_SAMPLES * stream->info.width * stream->info.channels, sizeof(rawData));

    format = S_OPENAL_Format(stream->info.width, stream->info.channels);

    bytesRead = S_CodecReadStream(stream, bytesToRead, rawData);
    streamNextOffset += bytesRead;
    if (!bytesRead) {
        S_CodecCloseStream(stream);

        sampleLooped++;
        if (sampleLoopCount && sampleLooped >= sampleLoopCount) {
            streamHandle = NULL;
            return;
        }

        streamHandle = S_CodecLoad(this->fileName, NULL);
        if (!streamHandle) {
            clear_stream();
            return;
        }

        stream = (snd_stream_t *)streamHandle;
        format = S_OPENAL_Format(stream->info.width, stream->info.channels);

        bytesRead        = S_CodecReadStream(stream, bytesToRead, rawData);
        streamNextOffset = bytesRead;
        if (!bytesRead) {
            S_CodecCloseStream(stream);
            streamHandle = NULL;
            return;
        }
    }

#if AL_SOFT_block_alignment
    if (stream->info.dataalign > 1 && soft_block_align) {
        qalBufferi(buffers[currentBuf], AL_UNPACK_BLOCK_ALIGNMENT_SOFT, stream->info.dataalign);
        alDieIfError();
    }
#endif

    qalBufferData(buffers[currentBuf], format, rawData, bytesRead, stream->info.rate);
    alDieIfError();

    qalSourceQueueBuffers(source, 1, &buffers[currentBuf]);
    alDieIfError();

    if (!is_playing()) {
        play();
    }

    currentBuf = (currentBuf + 1) % MAX_STREAM_BUFFERS;
}

U32 openal_channel_two_d_stream::sample_offset()
{
    snd_stream_t *stream              = (snd_stream_t *)streamHandle;
    ALint         playerByteOffset    = 0;
    unsigned int  totalQueueLength    = 0;
    unsigned int  bitsPerSample       = 0;
    unsigned int  offset              = 0;

    if (!streaming) {
        return 0;
    }

    qalGetSourcei(source, AL_BYTE_OFFSET, &playerByteOffset);
    alDieIfError();

    totalQueueLength = getQueueLength();
    bitsPerSample    = getBitsPerSample();

    assert(playerByteOffset < totalQueueLength);
    assert(streamNextOffset >= totalQueueLength || stream);

    if (streamNextOffset >= totalQueueLength) {
        offset = streamNextOffset - totalQueueLength + playerByteOffset;
    } else if (stream) {
        if (streamNextOffset + playerByteOffset <= totalQueueLength) {
            if ((unsigned int)stream->info.size > totalQueueLength) {
                offset = stream->info.size + streamNextOffset - totalQueueLength + playerByteOffset;
            } else {
                offset = (streamNextOffset - totalQueueLength + playerByteOffset) % stream->info.size;
            }
        } else {
            offset = streamNextOffset + playerByteOffset - totalQueueLength;
        }

        assert(offset < stream->info.size);
    } else {
        offset = totalQueueLength - streamNextOffset + playerByteOffset;
    }

    if (!bitsPerSample) {
        return 0;
    }

    return offset * 8 / bitsPerSample;
}

U32 openal_channel_two_d_stream::buffer_frequency() const
{
    unsigned int bufferId;
    ALint        freq = 0;

    bufferId = (currentBuf - 1) % MAX_STREAM_BUFFERS;
    qalGetBufferi(buffers[bufferId], AL_FREQUENCY, &freq);
    alDieIfError();

    return freq;
}

void openal_channel_two_d_stream::set_sample_offset(U32 offset)
{
    snd_stream_t *stream;
    unsigned int  bytesToRead, bytesRead;
    unsigned int  byteOffset;
    unsigned int  bitsPerSample;
    unsigned int  streamPosition;
    ALuint        format;
    ALint         numQueuedBuffers;
    bool          bWasPlaying;
    char          rawData[MAX_BUFFER_SAMPLES * 2 * 2];

    if (!streaming) {
        return;
    }

    stream         = (snd_stream_t *)streamHandle;
    streamPosition = getCurrentStreamPosition();
    bitsPerSample  = getBitsPerSample();
    byteOffset     = offset * bitsPerSample / 8;

    if (byteOffset >= streamPosition && byteOffset < streamNextOffset) {
        qalSourcei(source, AL_BYTE_OFFSET, byteOffset - streamPosition);
        alDieIfError();

        return;
    }

    bWasPlaying = is_playing();

    if (stream) {
        if (byteOffset < streamPosition) {
            S_CodecCloseStream(stream);
            streamHandle = NULL;
        } else if (byteOffset >= (unsigned int)stream->info.size) {
            byteOffset %= stream->info.size;
            S_CodecCloseStream(stream);
            streamHandle = NULL;
        }
    }

    if (!streamHandle) {
        streamHandle = S_CodecLoad(this->fileName, NULL);
        if (!streamHandle) {
            clear_stream();
            return;
        }

        stream           = (snd_stream_t *)streamHandle;
        streamNextOffset = 0;
    }

    while (streamNextOffset <= byteOffset) {
        streamPosition = streamNextOffset;

        bytesToRead = Q_min(MAX_BUFFER_SAMPLES * stream->info.width * stream->info.channels, sizeof(rawData));
        bytesRead   = S_CodecReadStream(stream, bytesToRead, rawData);
        if (!bytesRead) {
            clear_stream();
            return;
        }

        streamNextOffset += bytesRead;
    }

    qalSourceStop(source);
    alDieIfError();

    numQueuedBuffers = 0;
    qalGetSourcei(source, AL_BUFFERS_QUEUED, &numQueuedBuffers);
    alDieIfError();

    while (numQueuedBuffers--) {
        ALuint b;
        qalSourceUnqueueBuffers(source, 1, &b);
        alDieIfError();
    }

    format = S_OPENAL_Format(stream->info.width, stream->info.channels);

    qalBufferData(buffers[currentBuf], format, rawData, bytesRead, stream->info.rate);
    alDieIfError();

    qalSourceQueueBuffers(source, 1, &buffers[currentBuf]);
    alDieIfError();
    currentBuf = (currentBuf + 1) % MAX_STREAM_BUFFERS;

    qalSourcei(source, AL_BYTE_OFFSET, byteOffset - streamPosition);
    alDieIfError();

    if (bWasPlaying) {
        play();
    }
}

bool openal_channel_two_d_stream::queue_stream(const char *pszFileName)
{
    snd_stream_t *stream;
    unsigned int  bytesToRead, bytesRead;
    ALuint        format = 0;
    char          rawData[MAX_BUFFER_SAMPLES * 2 * 2];

    Q_strncpyz(this->fileName, pszFileName, sizeof(this->fileName));
    clear_stream();

    sampleLoopCount = 1;

    streamHandle = S_CodecLoad(this->fileName, NULL);
    if (!streamHandle) {
        return false;
    }
    stream = (snd_stream_t *)streamHandle;

    iStartTime = cl.serverTime;
    iEndTime   = (int)(cl.serverTime + (stream->info.samples / (float)stream->info.rate * 1000.f));
    sampleLengthMs =
        stream->info.rate ? (unsigned int)((int64_t)stream->info.samples * 1000 / stream->info.rate) : 0;

    format = S_OPENAL_Format(stream->info.width, stream->info.channels);
    if (!format) {
        Com_Printf(
            "OpenAL: Bad Wave file (%d channels, %d bits) [%s].\n",
            stream->info.channels,
            (int)(stream->info.width * 8.f),
            pszFileName
        );

        S_CodecCloseStream(stream);
        return false;
    }

    qalGenBuffers(MAX_STREAM_BUFFERS, buffers);
    alDieIfError();

    bytesToRead = Q_min(MAX_BUFFER_SAMPLES * stream->info.width * stream->info.channels, sizeof(rawData));
    bytesRead   = S_CodecReadStream(stream, bytesToRead, rawData);
    if (!bytesRead) {
        return true;
    }

    streamNextOffset = bytesRead;

    qalBufferData(buffers[currentBuf], format, rawData, bytesRead, stream->info.rate);
    alDieIfError();

    qalSourceQueueBuffers(source, 1, &buffers[currentBuf]);
    alDieIfError();

    currentBuf = (currentBuf + 1) % MAX_STREAM_BUFFERS;

    streaming = true;

    return true;
}

U32 openal_channel_two_d_stream::sample_ms_length() const
{
    return sampleLengthMs;
}

void openal_channel_two_d_stream::clear_stream()
{
    if (!streaming) {
        return;
    }

    qalSourceStop(source);
    qalSourcei(source, AL_BUFFER, 0);
    qalDeleteBuffers(MAX_STREAM_BUFFERS, buffers);

    if (streamHandle) {
        S_CodecCloseStream((snd_stream_t *)streamHandle);
    }

    fileName[0]  = 0;
    streamHandle = NULL;

    currentBuf       = 0;
    streamNextOffset = 0;
    sampleLengthMs   = 0;
    streaming        = false;
    sampleLooped     = 0;
}

unsigned int openal_channel_two_d_stream::getQueueLength() const
{
    ALint        numQueuedBuffers = 0;
    unsigned int totalQueueLength = 0;
    unsigned int bufferId;
    unsigned int i;

    qalGetSourcei(source, AL_BUFFERS_QUEUED, &numQueuedBuffers);
    alDieIfError();

    for (i = 0; i < (unsigned int)numQueuedBuffers; i++) {
        ALint bufferSize = 0;

        bufferId = (currentBuf - i - 1) % MAX_STREAM_BUFFERS;
        qalGetBufferi(buffers[bufferId], AL_SIZE, &bufferSize);

        totalQueueLength += bufferSize;
    }

    return totalQueueLength;
}

unsigned int openal_channel_two_d_stream::getCurrentStreamPosition() const
{
    unsigned int queueLength = getQueueLength();

    if (streamNextOffset >= queueLength) {
        return streamNextOffset - queueLength;
    } else if (streamHandle) {
        snd_stream_t *stream = (snd_stream_t *)streamHandle;
        return (stream->info.size + streamNextOffset - queueLength) % stream->info.size;
    } else {
        return queueLength - streamNextOffset;
    }
}

unsigned int openal_channel_two_d_stream::getBitsPerSample() const
{
    unsigned int bufferId;
    ALint        bits = 0, channels = 0;

    bufferId = (currentBuf - 1) % MAX_STREAM_BUFFERS;
    qalGetBufferi(buffers[bufferId], AL_BITS, &bits);
    alDieIfError();
    qalGetBufferi(buffers[bufferId], AL_CHANNELS, &channels);
    alDieIfError();

    return bits * channels;
}
