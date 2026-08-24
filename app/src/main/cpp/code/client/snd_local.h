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

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_public.h"

#if defined(NO_MODERN_DMA) && NO_MODERN_DMA

#define	PAINTBUFFER_SIZE		4096					

#define SND_CHUNK_SIZE			1024					
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		
#define SND_CHUNK_SIZE_BYTE		(SND_CHUNK_SIZE*2)		


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int			left;	 
	int			right;
} portable_samplepair_t;

typedef struct adpcm_state {
    short	sample;		
    char	index;		
} adpcm_state_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
    int						size;
	adpcm_state_t			adpcm;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	qboolean		defaultSound;			
	qboolean		inMemory;				
	qboolean		soundCompressed;		
	int				soundCompressionMethod;	
	int 			soundLength;
	int			soundChannels;
	char 			soundName[MAX_QPATH];
	int				lastTimeUsed;
	struct sfx_s	*next;
} sfx_t;

typedef struct {
	int			channels;
	int			samples;				
	int			fullsamples;			
	int			submission_chunk;		
	int			samplebits;
	int			isfloat;
	int			speed;
	byte		*buffer;
} dma_t;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

#define MAX_DOPPLER_SCALE 50.0f 

#define THIRD_PERSON_THRESHOLD_SQ (48.0f*48.0f)

typedef struct loopSound_s {
	vec3_t		origin;
	vec3_t		velocity;
	sfx_t		*sfx;
	int			mergeFrame;
	qboolean	active;
	qboolean	kill;
	qboolean	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	int			framenum;
} loopSound_t;

typedef struct
{
	int			allocTime;
	int			startSample;	
	int			entnum;			
	int			entchannel;		
	int			leftvol;		
	int			rightvol;		
	int			master_vol;		
	float		dopplerScale;
	float		oldDopplerScale;
	vec3_t		origin;			
	qboolean	fixed_origin;	
	sfx_t		*thesfx;		
	qboolean	doppler;
	qboolean	fullVolume;
} channel_t;


#define	WAV_FORMAT_PCM		1


typedef struct {
	int			format;
	int			rate;
	int			width;
	int			channels;
	int			samples;
	int			dataofs;		
} wavinfo_t;

typedef struct
{
	void (*Shutdown)(void);
	void (*StartSound)( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
	void (*StartLocalSound)( sfxHandle_t sfx, int channelNum );
	void (*StartBackgroundTrack)( const char *intro, const char *loop );
	void (*StopBackgroundTrack)( void );
	void (*RawSamples)(int stream, int samples, int rate, int width, int channels, const byte *data, float volume, int entityNum);
	void (*StopAllSounds)( void );
	void (*ClearLoopingSounds)( qboolean killall );
	void (*AddLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*StopLoopingSound)(int entityNum );
	void (*Respatialize)( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
	void (*UpdateEntityPosition)( int entityNum, const vec3_t origin );
	void (*Update)( void );
	void (*DisableSounds)( void );
	void (*BeginRegistration)( void );
	sfxHandle_t (*RegisterSound)( const char *sample, qboolean compressed );
	void (*ClearSoundBuffer)( void );
	void (*SoundInfo)( void );
	void (*SoundList)( void );
#ifdef USE_VOIP
#endif
} soundInterface_t;



qboolean SNDDMA_Init(void);

int		SNDDMA_GetDMAPos(void);

void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

#ifdef USE_VOIP
#endif



#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];
extern	channel_t   loop_channels[MAX_CHANNELS];
extern	int		numLoopChannels;

extern	int		s_paintedtime;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

#define	MAX_RAW_SAMPLES	16384
#define MAX_RAW_STREAMS (MAX_CLIENTS * 2 + 1)
extern	portable_samplepair_t s_rawsamples[MAX_RAW_STREAMS][MAX_RAW_SAMPLES];
extern	int		s_rawend[MAX_RAW_STREAMS];

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
extern cvar_t *s_muted;
extern cvar_t *s_doppler;

extern cvar_t *s_testsound;

qboolean S_LoadSound( sfx_t *sfx );

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc( void );
void		SND_setup( void );
void		SND_shutdown(void);

void S_PaintChannels(int endtime);

void S_memoryLoad(sfx_t *sfx);

void S_Spatialize(channel_t *ch);

int  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);


#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

qboolean S_FreeOldestSound( void );

#define	NXStream byte

void encodeWavelet(sfx_t *sfx, short *packets);
void decodeWavelet( sndBuffer *stream, short *packets);

void encodeMuLaw( sfx_t *sfx, short *packets);
extern short mulawToShort[256];

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern int	   sfxScratchIndex;

qboolean S_Base_Init( soundInterface_t *si );

typedef enum
{
	SRCPRI_AMBIENT = 0,	
	SRCPRI_ENTITY,			
	SRCPRI_ONESHOT,			
	SRCPRI_LOCAL,				
	SRCPRI_STREAM				
} alSrcPriority_t;

typedef int srcHandle_t;

qboolean S_AL_Init( soundInterface_t *si );


#include "new/snd_local_new.h"

#ifdef __cplusplus
}
#endif

#else
#  include "snd_local_new.h"
#endif
