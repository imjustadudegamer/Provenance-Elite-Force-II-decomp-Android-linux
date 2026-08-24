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

#if defined(NO_MODERN_DMA) && NO_MODERN_DMA

#ifdef __cplusplus
extern "C" {
#endif

void S_Init( void );
void S_Shutdown( void );

void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
void S_StartLocalSound( sfxHandle_t sfx, int channelNum );

void S_StartBackgroundTrack( const char *intro, const char *loop );
void S_StopBackgroundTrack( void );

void S_RawSamples(int stream, int samples, int rate, int width, int channels,
				   const byte *data, float volume, int entityNum);

void S_StopAllSounds( void );

void S_ClearLoopingSounds( qboolean killall );
void S_StopLoopingSound(int entityNum );

void S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );

void S_UpdateEntityPosition( int entityNum, const vec3_t origin );

void S_Update( void );

void S_DisableSounds( void );

void S_BeginRegistration( void );

sfxHandle_t	S_RegisterSound( const char *sample, qboolean compressed );

void S_DisplayFreeMemory(void);

void S_ClearSoundBuffer( void );

void SNDDMA_Activate( void );

void S_UpdateBackgroundTrack( void );


#ifdef USE_VOIP
#endif


#ifdef __cplusplus
}
#endif

#include "new/snd_public_new.h"

#else

#ifdef __cplusplus
extern "C" {
#endif

void S_SoundInfo_f(void);
void S_SoundDump_f(void);
void S_Init(qboolean full_startup);
void S_Shutdown(qboolean full_shutdown);
qboolean S_NameExists(const char* name);
void S_BeginRegistration(void);
qboolean S_IsSoundRegistered(const char* name);
sfxHandle_t S_RegisterSound(const char* name);
sfxHandle_t S_RegisterSoundExt(const char* name, int streamed, qboolean force_load);
float S_GetSoundTime(sfxHandle_t handle);
void S_EndRegistration(void);
void S_UpdateEntity(int entityNum, const vec3_t origin, const vec3_t vel, qboolean use_listener);
void S_SetGlobalAmbientVolumeLevel(float volume);
void S_StartSound(const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfxHandle, float volume, float min_dist, float pitch);
void S_StartSoundExt(const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfxHandle, float volume, float min_dist, float pitch, float maxDist, int streamed);
void S_StartLocalSound(const char* sound_name, qboolean force_load);
void S_StartLocalSoundChannel(const char* sound_name, qboolean force_load, soundChannel_t channel);
void S_StopAllSounds(qboolean stop_music);
void S_ClearLoopingSounds(void);
void S_AddLoopingSound(
    const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle, float volume, float min_dist, int entityNum,
    int unused7, int unused8
);
void S_Respatialize(int entityNum, const vec3_t head, vec3_t axis[3]);
void S_Update(void);
void S_StopSound(int entnum, int channel);
void S_SetReverb(int reverb_type, float reverb_level);
void S_Play(void);
void S_SoundTrack_f(void);
void S_SoundList(void);

qboolean S_IsSoundPlaying(int channel_number, const char* sfxName);
void S_FadeSound(float fTime);

qboolean MUSIC_LoadSoundtrackFile(const char* filename);
qboolean MUSIC_SongValid(const char* mood);
qboolean MUSIC_Loaded(void);
void Music_Update(void);
void MUSIC_SongEnded(void);
void MUSIC_NewSoundtrack(const char* name);
void MUSIC_UpdateMood(int current, int fallback);
void MUSIC_UpdateVolume(float volume, float fade_time);
void MUSIC_UpdateDucking(int ducking);
void MUSIC_StopAllSongs(void);
void MUSIC_FreeAllSongs(void);
qboolean MUSIC_Playing(void);
int MUSIC_FindSong(const char* name);
void S_loadsoundtrack(void);
const char* S_CurrentSoundtrack(void);
void S_PlaySong(void);
int MUSIC_CurrentSongChannel(void);
void MUSIC_StopChannel(int channel_number);
qboolean MUSIC_PlaySong(const char* alias);
void MUSIC_UpdateMusicVolumes(void);
void MUSIC_CheckForStoppedSongs(void);
void S_TriggeredMusic_SetupHandle(const char* pszName, int iLoopCount, int iOffset, qboolean autostart);
void S_TriggeredMusic_Start(void);
void S_TriggeredMusic_StartLoop(void);
void S_TriggeredMusic_Stop(void);
void S_TriggeredMusic_Volume(void);
void S_TriggeredMusic_Pause(void);
void S_TriggeredMusic_Unpause(void);
void S_TriggeredMusic_PlayIntroMusic(void);
void S_StopMovieAudio(void);
void S_SetupMovieAudio(const char* pszMovieName);

void S_RawSamples(int stream, int samples, int rate, int width, int channels,
				  const byte* data, float volume, int entityNum);
void S_StopRawSamples(void);
int S_CurrentMoviePosition(void);

const char* S_GetMusicFilename(void);
int S_GetMusicLoopCount(void);
unsigned int S_GetMusicOffset(void);

#ifdef __cplusplus
}
#endif

#endif


#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_VOIP
#endif

#ifdef __cplusplus
}
#endif
