/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
Copyright (C) 2005-2006 Joerg Dietrich <dietrich_joerg@gmx.de>
Copyright (C) 2006 Thilo Schulz <arny@ats.s.bawue.de>

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

#if USE_CODEC_MAD

#include "client.h"
#include "snd_codec.h"

#include "mad.h"

#define MP3_SAMPLE_WIDTH		2
#define MP3_PCMSAMPLES_PERSLICE		32

#define MP3_DATA_BUFSIZ			128*1024

#define MP3_DITHERING

snd_codec_t mp3_codec =
{
    "mp3",
    S_MP3_CodecLoad,
    S_MP3_CodecOpenStream,
    S_MP3_CodecReadStream,
    S_MP3_CodecCloseStream,
    NULL
};

struct snd_codec_mp3_info
{
    byte encbuf[MP3_DATA_BUFSIZ];	
    struct mad_stream madstream;	
    struct mad_frame madframe;	
    struct mad_synth madsynth;

    byte* pcmbuf;			
    int buflen;			
    int pcmbufsize;			

    byte* dest;			
    int destlen;			
    int destsize;			
};



int S_MP3_ReadData(snd_stream_t* stream, struct mad_stream* madstream, byte* encbuf, unsigned long encbufsize)
{
    unsigned long retval;
    unsigned long leftover;

    if (!stream)
        return -1;

    leftover = madstream->bufend - madstream->next_frame;
    if (leftover > 0)
        memmove(encbuf, madstream->this_frame, leftover);



    retval = FS_Read(&encbuf[leftover], encbufsize - leftover, stream->file);

    if (retval <= 0)
    {
        return 0;
    }

    mad_stream_buffer(madstream, encbuf, retval + leftover);

    return retval;
}



int S_MP3_Scanfile(snd_stream_t* stream)
{
    struct mad_stream madstream;
    struct mad_header madheader;
    int retval;
    int samplecount;
    byte encbuf[MP3_DATA_BUFSIZ];

    if (!stream)
        return -1;

    mad_stream_init(&madstream);
    mad_header_init(&madheader);

    while (1)
    {
        retval = S_MP3_ReadData(stream, &madstream, encbuf, sizeof(encbuf));
        if (retval < 0)
            return -1;
        else if (retval == 0)
            break;

        while (1)
        {
            if ((retval = mad_header_decode(&madheader, &madstream)) < 0)
            {
                if (madstream.error == MAD_ERROR_BUFLEN)
                {
                    break;
                }

                if (!MAD_RECOVERABLE(madstream.error))
                {
                    return retval;
                }

                mad_stream_skip(&madstream, madstream.skiplen);
                continue;
            }


            if (madheader.layer != MAD_LAYER_III)
            {
                return -1;
            }

            if (!stream->info.samples)
            {

                stream->info.rate = madheader.samplerate;
                stream->info.width = MP3_SAMPLE_WIDTH;
                stream->info.channels = MAD_NCHANNELS(&madheader);
                stream->info.samples = 0;
                stream->info.size = 0;				
                stream->info.dataofs = 0;
            }
            else
            {

                if (stream->info.rate != madheader.samplerate ||
                    stream->info.channels != MAD_NCHANNELS(&madheader))
                    return -1;
            }

            samplecount = MAD_NSBSAMPLES(&madheader) * MP3_PCMSAMPLES_PERSLICE;
            stream->info.samples += samplecount;
            stream->info.size += samplecount * stream->info.channels * stream->info.width;
        }
    }

    return 0;
}


#ifdef MP3_DITHERING



#define MP3_DITH_N 624
#define MP3_DITH_M 397
#define MATRIX_A 0x9908b0df   
#define UPPER_MASK 0x80000000 
#define LOWER_MASK 0x7fffffff 

#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

static unsigned long mt[MP3_DITH_N]; 
static int mti = MP3_DITH_N + 1; 

void sgenrand(unsigned long seed)
{
    mt[0] = seed & 0xffffffff;
    for (mti = 1; mti < MP3_DITH_N; mti++)
        mt[mti] = (69069 * mt[mti - 1]) & 0xffffffff;
}

unsigned long genrand(void)
{
    unsigned long y;
    static unsigned long mag01[2] = { 0x0, MATRIX_A };

    if (mti >= MP3_DITH_N) { 
        int kk;

        if (mti == MP3_DITH_N + 1)   
            sgenrand(4357); 

        for (kk = 0; kk < MP3_DITH_N - MP3_DITH_M; kk++) {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + MP3_DITH_M] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        for (; kk < MP3_DITH_N - 1; kk++) {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + (MP3_DITH_M - MP3_DITH_N)] ^ (y >> 1) ^ mag01[y & 0x1];
        }
        y = (mt[MP3_DITH_N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
        mt[MP3_DITH_N - 1] = mt[MP3_DITH_M - 1] ^ (y >> 1) ^ mag01[y & 0x1];

        mti = 0;
    }

    y = mt[mti++];
    y ^= TEMPERING_SHIFT_U(y);
    y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
    y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
    y ^= TEMPERING_SHIFT_L(y);

    return y;
}

long triangular_dither_noise(int nbits) {

    long v = (genrand() / 2 - genrand() / 2); 
    long P = 1 << (32 - nbits); 
    v /= P;

    return v;
}

#endif 




signed int S_MP3_Scale(mad_fixed_t sample)
{
    int n_bits_to_loose = MAD_F_FRACBITS + 1 - 16;
#ifdef MP3_DITHERING
    int dither;
#endif

    sample += (1L << (n_bits_to_loose - 1));

#ifdef MP3_DITHERING
    dither = triangular_dither_noise(n_bits_to_loose + 1);
    sample += dither;
#endif

    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> n_bits_to_loose;
}




int S_MP3_PCMCopy(byte* buf, struct mad_pcm* pcm, int bufofs,
    int sampleofs, int bytecount, int* wrotecnt)
{
    int written = 0;
    signed int sample;
    int framesize = pcm->channels * MP3_SAMPLE_WIDTH;

    while (written < bytecount && sampleofs < pcm->length)
    {
        sample = S_MP3_Scale(pcm->samples[0][sampleofs]);

#ifdef Q3_BIG_ENDIAN
        buf[bufofs++] = (sample >> 8) & 0xff;
        buf[bufofs++] = sample & 0xff;
#else
        buf[bufofs++] = sample & 0xff;
        buf[bufofs++] = (sample >> 8) & 0xff;
#endif

        if (pcm->channels == 2)
        {
            sample = S_MP3_Scale(pcm->samples[1][sampleofs]);

#ifdef Q3_BIG_ENDIAN
            buf[bufofs++] = (sample >> 8) & 0xff;
            buf[bufofs++] = sample & 0xff;
#else
            buf[bufofs++] = sample & 0xff;
            buf[bufofs++] = (sample >> 8) & 0xff;
#endif
        }

        sampleofs++;
        written += framesize;
    }

    if (wrotecnt)
        *wrotecnt = written;

    return sampleofs;
}



int S_MP3_Decode(snd_stream_t* stream)
{
    struct snd_codec_mp3_info* mp3info;
    struct mad_stream* madstream;
    struct mad_frame* madframe;
    struct mad_synth* madsynth;
    struct mad_pcm* pcm;
    int cursize;
    int samplecount;
    int needcount;
    int wrote;
    int retval;

    if (!stream)
        return -1;

    mp3info = stream->ptr;
    madstream = &mp3info->madstream;
    madframe = &mp3info->madframe;

    if (mad_frame_decode(madframe, madstream))
    {
        if (madstream->error == MAD_ERROR_BUFLEN)
        {
            retval = S_MP3_ReadData(stream, madstream, mp3info->encbuf, sizeof(mp3info->encbuf));

            if (retval > 0)
                retval = S_MP3_Decode(stream);
        }
        else if (MAD_RECOVERABLE(madstream->error))
        {
            mad_stream_skip(madstream, madstream->skiplen);
            return S_MP3_Decode(stream);
        }
        else
            retval = -1;

        return retval;
    }

    if (madframe->header.layer != MAD_LAYER_III)
        return -1;

    madsynth = &mp3info->madsynth;
    mad_synth_frame(madsynth, madframe);

    pcm = &madsynth->pcm;


    if (stream->info.rate != pcm->samplerate ||
        stream->info.channels != pcm->channels)
    {
        return -1;
    }
    cursize = pcm->length * pcm->channels * stream->info.width;
    needcount = mp3info->destsize - mp3info->destlen;

    samplecount = S_MP3_PCMCopy(mp3info->dest, pcm,
        mp3info->destlen, 0, needcount, &wrote);
    mp3info->destlen += wrote;

    if (samplecount < pcm->length)
    {
        samplecount = S_MP3_PCMCopy(mp3info->pcmbuf, pcm,
            mp3info->buflen,
            samplecount,
            mp3info->pcmbufsize - mp3info->buflen,
            &wrote);
        mp3info->buflen += wrote;


        if (samplecount < pcm->length)
        {
            int   newsize = (cursize > mp3info->buflen) ? cursize : mp3info->buflen;
            byte* newbuf = Z_Malloc(newsize);

            if (mp3info->pcmbuf)
            {
                memcpy(newbuf, mp3info->pcmbuf, mp3info->buflen);
                Z_Free(mp3info->pcmbuf);
            }

            mp3info->pcmbuf = newbuf;
            mp3info->pcmbufsize = newsize;

            samplecount = S_MP3_PCMCopy(mp3info->pcmbuf, pcm,
                mp3info->buflen,
                samplecount,
                mp3info->pcmbufsize - mp3info->buflen,
                &wrote);
            mp3info->buflen += wrote;
        }

        retval = 0;
    }
    else if (mp3info->destlen >= mp3info->destsize)
        retval = 0;
    else
        retval = 1;

    return retval;
}



snd_stream_t* S_MP3_CodecOpenStream(const char* filename)
{
    snd_stream_t* stream;
    struct snd_codec_mp3_info* mp3info;

    stream = S_CodecUtilOpen(filename, &mp3_codec);
    if (!stream || stream->length <= 0)
        return NULL;

    if (S_MP3_Scanfile(stream) < 0)
    {
        S_CodecUtilClose(&stream);
        return NULL;
    }

    // "WARNING: Failed to open sound <path>!", MUSIC_PlaySong as "Could not start music
    // FS_SEEK_CUR only, and retail itself never runs this codec (its mp3 path is Miles).
    {
        fileHandle_t rewindFile = 0;
        long         rewindLen;

        FS_FCloseFile(stream->file);

        rewindLen = FS_FOpenFileRead(filename, &rewindFile, qtrue, qtrue);
        if (!rewindFile || rewindLen <= 0)
        {
            Z_Free(stream);
            return NULL;
        }

        stream->file   = rewindFile;
        stream->length = rewindLen;
    }

    mp3info = Z_Malloc(sizeof(*mp3info));
    if (!mp3info)
    {
        S_CodecUtilClose(&stream);
        return NULL;
    }

    stream->ptr = mp3info;

    mad_stream_init(&mp3info->madstream);
    mad_frame_init(&mp3info->madframe);
    mad_synth_init(&mp3info->madsynth);

    if (S_MP3_ReadData(stream, &mp3info->madstream, mp3info->encbuf, sizeof(mp3info->encbuf)) <= 0)
    {
        S_MP3_CodecCloseStream(stream);
        return NULL;
    }

    return stream;
}


void S_MP3_CodecCloseStream(snd_stream_t* stream)
{
    struct snd_codec_mp3_info* mp3info;

    if (!stream)
        return;


    if (stream->ptr)
    {
        mp3info = stream->ptr;

        if (mp3info->pcmbuf)
            Z_Free(mp3info->pcmbuf);

        mad_synth_finish(&mp3info->madsynth);
        mad_frame_finish(&mp3info->madframe);
        mad_stream_finish(&mp3info->madstream);

        Z_Free(stream->ptr);
    }

    S_CodecUtilClose(&stream);
}

int S_MP3_CodecReadStream(snd_stream_t* stream, int bytes, void* buffer)
{
    struct snd_codec_mp3_info* mp3info;
    int retval;

    if (!stream)
        return -1;

    mp3info = stream->ptr;

    bytes -= fmod(bytes, (stream->info.channels * stream->info.width));

    if (mp3info->buflen)
    {
        if (bytes < mp3info->buflen)
        {
            memcpy(buffer, mp3info->pcmbuf, bytes);

            mp3info->buflen -= bytes;
            memmove(mp3info->pcmbuf, &mp3info->pcmbuf[bytes], mp3info->buflen);
            return bytes;
        }
        else
        {
            memcpy(buffer, mp3info->pcmbuf, mp3info->buflen);
            mp3info->destlen = mp3info->buflen;
            mp3info->buflen = 0;
        }
    }
    else
        mp3info->destlen = 0;

    mp3info->dest = buffer;
    mp3info->destsize = bytes;

    do
    {
        retval = S_MP3_Decode(stream);
    } while (retval > 0);

    if (retval < 0)
        return 0;

    return mp3info->destlen;
}

void* S_MP3_CodecLoad(const char* filename, snd_info_t* info)
{
    snd_stream_t* stream;
    byte* pcmbuffer;

    if (!filename)
        return NULL;

    stream = S_MP3_CodecOpenStream(filename);

    if (!stream)
        return NULL;

    info->rate = stream->info.rate;
    info->width = stream->info.width;
    info->channels = stream->info.channels;
    info->samples = stream->info.samples;
    info->dataofs = stream->info.dataofs;

    pcmbuffer = Z_Malloc(stream->info.size);
    if (!pcmbuffer)
    {
        S_MP3_CodecCloseStream(stream);
        return NULL;
    }

    info->size = S_MP3_CodecReadStream(stream, stream->info.size, pcmbuffer);

    if (info->size <= 0)
    {
        Z_Free(pcmbuffer);
        pcmbuffer = NULL;
    }

    S_MP3_CodecCloseStream(stream);

    return pcmbuffer;
}

#endif
