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
#include "cl_ui.h"

byte *data_p;
byte *iff_end;
byte *last_chunk;
byte *iff_data;
int   iff_chunk_len;

qboolean S_LoadMP3(const char *fileName, sfx_t *sfx);

short int GetLittleShort()
{
    union {
        short value;
        byte  bytes[2];
    } val;

#ifdef Q3_LITTLE_ENDIAN
    val.bytes[0] = data_p[0];
    val.bytes[1] = data_p[1];
#else
    val.bytes[0] = data_p[1];
    val.bytes[1] = data_p[0];
#endif

    data_p += sizeof(short);
    return val.value;
}

int GetLittleLong()
{
    union {
        int  value;
        byte bytes[4];
    } val;

#ifdef Q3_LITTLE_ENDIAN
    val.bytes[0] = data_p[0];
    val.bytes[1] = data_p[1];
    val.bytes[2] = data_p[2];
    val.bytes[3] = data_p[3];
#else
    val.bytes[0] = data_p[3];
    val.bytes[1] = data_p[2];
    val.bytes[2] = data_p[1];
    val.bytes[3] = data_p[0];
#endif

    data_p += sizeof(int);
    return val.value;
}

void SetLittleShort(int i)
{
    union {
        short value;
        byte  bytes[2];
    } val;

    val.value = i;

#ifdef Q3_LITTLE_ENDIAN
    data_p[0] = val.bytes[0];
    data_p[1] = val.bytes[1];
#else
    data_p[0] = val.bytes[1];
    data_p[1] = val.bytes[0];
#endif

    data_p += sizeof(short);
}

void SetLittleLong(int i)
{
    union {
        int  value;
        byte bytes[4];
    } val;

    val.value = i;

#ifdef Q3_LITTLE_ENDIAN
    data_p[0] = val.bytes[0];
    data_p[1] = val.bytes[1];
    data_p[2] = val.bytes[2];
    data_p[3] = val.bytes[3];
#else
    data_p[0] = val.bytes[3];
    data_p[1] = val.bytes[2];
    data_p[2] = val.bytes[1];
    data_p[3] = val.bytes[0];
#endif

    data_p += sizeof(int);
}

void FindNextChunk(const char *name)
{
    int value;

    while (1) {
        data_p = last_chunk;

        if (last_chunk >= (byte *)iff_end) {
            break;
        }

        data_p = last_chunk + 4;
        value  = GetLittleLong();

        iff_chunk_len = value;
        if (value < 0) {
            break;
        }

        data_p -= 8;
        value++;
        value &= ~1;

        last_chunk = data_p + value + 8;
        if (!strncmp((const char *)data_p, name, 4u)) {
            return;
        }
    }

    data_p = NULL;
}

void FindChunk(const char *name)
{
    last_chunk = iff_data;
    FindNextChunk(name);
}

wavinfo_t GetWavinfo(const char *name, byte *wav, int wavlength)
{
    wavinfo_t info;
    int       samples;

    memset(&info, 0, sizeof(wavinfo_t));

    if (!wav) {
        return info;
    }

    iff_data = wav;
    iff_end  = &wav[wavlength];
    FindChunk("RIFF");

    if (!data_p || strncmp((const char *)data_p + 8, "WAVE", 4u)) {
        Com_WPrintf("Missing RIFF/WAVE chunks\n");
        return info;
    }

    iff_data = data_p + 12;
    FindChunk("fmt ");
    if (!data_p) {
        Com_WPrintf("Missing fmt chunk\n");
        return info;
    }

    data_p += 8;

    // MOHAA's format-17 (IMA ADPCM) branch does not exist in retail EF2: a
    // bail-out.  Also read off the disassembly: retail never touches
    info.format = GetLittleShort();
    if (info.format == 1) {
        info.channels = GetLittleShort();
        info.rate     = GetLittleLong();
        data_p += 4;

        GetLittleShort();
        info.width = GetLittleShort() / 8;

        FindChunk("data");
        if (!data_p) {
            Com_WPrintf("Missing data chunk\n");
            return info;
        }

        data_p += 4;
        samples = GetLittleLong() / info.width;

        if (!info.samples) {
            info.samples = samples;
        } else if (samples < info.samples) {
            Com_Error(ERR_DROP, "Sound %s has a bad loop length", name);
        }

        info.dataofs = data_p - wav;
    } else {
        Com_WPrintf("Microsoft PCM format only\n");
        return info;
    }

    return info;
}

qboolean DownSampleWav(wavinfo_t *info, byte *wav, int wavlength, int newkhz, byte **newdata)
{
    int   newdatasize;
    byte *datap;
    int   i;
    int   ii;
    int   error;
    int   width;
    int   oldsamples;
    int   oldrate;

    newdatasize = 0;
    datap       = &wav[info->dataofs];

    if (info->channels > 1) {
        Com_DPrintf("Could not downsample WAV file. Stereo WAVs not supported!\n");
        return 0;
    }

    if (info->format != 1 || !info->dataofs) {
        Com_DPrintf("Could not downsample WAV file. Not PCM format!\n");
        return 0;
    }

    if (info->rate <= newkhz) {
        return 0;
    }

    error = 0;
    width = info->width;
    for (i = 0; i < info->samples; i++) {
        error += newkhz;
        while (error > info->rate) {
            error -= info->rate;
            newdatasize += width;
        }
    }

    oldsamples    = info->samples;
    oldrate       = info->rate;
    info->samples = newdatasize / width;
    info->rate    = newkhz;
    newdatasize += info->dataofs;

    *newdata = (byte *)Z_TagMalloc(newdatasize, TAG_SOUND);
    memcpy(*newdata, wav, info->dataofs);

    iff_data = *newdata;
    iff_end  = *newdata + newdatasize;
    FindChunk("RIFF");

    if (!data_p || strncmp((const char *)data_p + 8, "WAVE", 4u)) {
        Com_DPrintf("Missing RIFF/WAVE chunks\n");
        return 0;
    }

    iff_data = data_p + 12;
    FindChunk("fmt ");
    if (!data_p) {
        Com_DPrintf("Missing fmt chunk\n");
        return 0;
    }

    data_p += 12;
    SetLittleShort((int)info->rate);
    data_p += 8;

    FindChunk("data");
    if (!data_p) {
        Com_DPrintf("Missing data chunk\n");
        return 0;
    }

    data_p += 4;
    SetLittleLong((int)(info->samples * info->width));

    error = 0;
    for (i = 0; i < oldsamples; i++) {
        error += newkhz;
        while (error > oldrate) {
            error -= oldrate;
            for (ii = 0; ii < width; ii++) {
                data_p[ii] = datap[ii];
            }

            data_p += width;
        }

        datap += width;
    }

    assert(iff_end - data_p == 0);

    return newdatasize;
}

int DownSampleWav_MILES(wavinfo_t *info, byte *wav, int wavlength, int newkhz, byte **newdata)
{
    return DownSampleWav(info, wav, wavlength, newkhz, newdata);
}

qboolean S_LoadSound(const char *fileName, sfx_t *sfx, int streamed, qboolean force_load)
{
    int          size;
    fileHandle_t file_handle;
    char         tempName[MAX_RES_NAME + 1];
    int          realKhz;

    sfx->buffer = 0;

    if (fileName[0] == '*') {
        return qfalse;
    }

    if (strstr(fileName, ".mp3")) {
        sfx->iFlags |= SFX_FLAG_STREAMED;
        return S_LoadMP3(fileName, sfx);
    }

    size = FS_FOpenFileRead(fileName, &file_handle, qfalse, qtrue);
    if (size <= 0) {
        if (file_handle) {
            FS_FCloseFile(file_handle);
        }
        return qfalse;
    }

    sfx->data = (byte *)Z_TagMalloc(size, TAG_SOUND);

    FS_Read(sfx->data, size, file_handle);
    FS_FCloseFile(file_handle);
    sfx->info = GetWavinfo(fileName, sfx->data, size);

    if (sfx->info.channels != 1 && !streamed) {
        Com_WPrintf("%s is a stereo wav file\n", fileName);
        Z_Free(sfx->data);
        sfx->data = NULL;
        return qfalse;
    }

    if (!sfx->info.dataofs) {
        sfx->iFlags |= SFX_FLAG_NO_OFFSET;
    }

    switch (s_khz->integer) {
    case 11:
        realKhz = 11025;
        break;
    case 22:
    default:
        realKhz = 22050;
        break;
    case 44:
        realKhz = 44100;
        break;
    }

    if (!(sfx->iFlags & SFX_FLAG_STREAMED) && realKhz < sfx->info.rate) {
        byte *newdata;
        int   newdatasize;

        newdata = NULL;
        if (sfx->iFlags & SFX_FLAG_NO_OFFSET) {
            newdatasize = DownSampleWav_MILES(&sfx->info, sfx->data, size, realKhz, &newdata);
        } else {
            newdatasize = DownSampleWav(&sfx->info, sfx->data, size, realKhz, &newdata);
        }

        if (newdatasize) {
            Z_Free(sfx->data);
            sfx->data = newdata;
        }
    }

    sfx->length      = sfx->info.samples;
    sfx->width       = sfx->info.width;
    sfx->time_length = sfx->info.samples / (float)sfx->info.rate * 1000.f;

    Com_sprintf(tempName, sizeof(tempName), "k%s", fileName);
    UI_LoadResource(tempName);

    if (strstr(fileName, "sound/null.wav")) {
        sfx->iFlags |= SFX_FLAG_NULL;
        return qtrue;
    }

    return qtrue;
}

qboolean S_LoadMP3(const char *fileName, sfx_t *sfx)
{
    int          length;
    fileHandle_t file_handle;

    length = FS_FOpenFileRead(fileName, &file_handle, 0, 1);
    if (length <= 0) {
        if (file_handle) {
            FS_FCloseFile(file_handle);
        }
        return qfalse;
    }

    memset(&sfx->info, 0, sizeof(sfx->info));
    sfx->data   = (byte *)Z_TagMalloc(length, TAG_SOUND);
    sfx->length = length;
    sfx->width  = 1;
    FS_Read(sfx->data, length, file_handle);
    FS_FCloseFile(file_handle);

    sfx->iFlags |= SFX_FLAG_MP3;

    return qtrue;
}
