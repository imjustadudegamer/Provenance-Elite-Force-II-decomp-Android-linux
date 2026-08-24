
#ifndef _SND_CODEC_H_
#define _SND_CODEC_H_

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_info_s
{
	int rate;
	float width;
	int channels;
	int samples;
	int size;
	int dataofs;
	int dataalign;
} snd_info_t;

typedef struct snd_codec_s snd_codec_t;

typedef struct snd_stream_s
{
	snd_codec_t *codec;
	fileHandle_t file;
	snd_info_t info;
	int length;
	int pos;
	void *ptr;
} snd_stream_t;

typedef void *(*CODEC_LOAD)(const char *filename, snd_info_t *info);
typedef snd_stream_t *(*CODEC_OPEN)(const char *filename);
typedef int (*CODEC_READ)(snd_stream_t *stream, int bytes, void *buffer);
typedef void (*CODEC_CLOSE)(snd_stream_t *stream);

struct snd_codec_s
{
	char *ext;
	CODEC_LOAD load;
	CODEC_OPEN open;
	CODEC_READ read;
	CODEC_CLOSE close;
	snd_codec_t *next;
};

void S_CodecInit( void );
void S_CodecShutdown( void );
void S_CodecRegister(snd_codec_t *codec);
void *S_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_CodecOpenStream(const char *filename);
void S_CodecCloseStream(snd_stream_t *stream);
int S_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

snd_stream_t *S_CodecUtilOpen(const char *filename, snd_codec_t *codec);
void S_CodecUtilClose(snd_stream_t **stream);

extern snd_codec_t wav_codec;
void *S_WAV_CodecLoad(const char *filename, snd_info_t *info);
snd_stream_t *S_WAV_CodecOpenStream(const char *filename);
void S_WAV_CodecCloseStream(snd_stream_t *stream);
int S_WAV_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer);

#ifdef USE_CODEC_VORBIS
#endif 

#ifdef USE_CODEC_MAD
extern snd_codec_t mp3_codec;
void* S_MP3_CodecLoad(const char* filename, snd_info_t * info);
snd_stream_t * S_MP3_CodecOpenStream(const char* filename);
void S_MP3_CodecCloseStream(snd_stream_t * stream);
int S_MP3_CodecReadStream(snd_stream_t * stream, int bytes, void* buffer);
#endif 

#ifdef USE_CODEC_OPUS
#endif 

#ifdef __cplusplus
}
#endif

#endif 
