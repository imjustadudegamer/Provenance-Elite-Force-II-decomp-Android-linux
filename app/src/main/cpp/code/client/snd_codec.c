/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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

#include "client.h"
#include "snd_codec.h"

static snd_codec_t *codecs;

static void *S_CodecGetSound(const char *filename, snd_info_t *info)
{
	snd_codec_t *codec;
	snd_codec_t *orgCodec = NULL;
	qboolean	orgNameFailed = qfalse;
	char		localName[ MAX_QPATH ];
	const char	*ext;
	char		altName[ MAX_QPATH ];
	void		*rtn = NULL;

	Q_strncpyz(localName, filename, MAX_QPATH);

	ext = COM_GetExtension(localName);

	if( *ext )
	{
		for( codec = codecs; codec; codec = codec->next )
		{
			if( !Q_stricmp( ext, codec->ext ) )
			{
				if( info )
					rtn = codec->load(localName, info);
				else
					rtn = codec->open(localName);
				break;
			}
		}

		if( codec )
		{
			if( !rtn )
			{
				orgNameFailed = qtrue;
				orgCodec = codec;
				COM_StripExtension( filename, localName, MAX_QPATH );
			}
			else
			{
				return rtn;
			}
		}
	}

	for( codec = codecs; codec; codec = codec->next )
	{
		if( codec == orgCodec )
			continue;

		Com_sprintf( altName, sizeof (altName), "%s.%s", localName, codec->ext );

		if( info )
			rtn = codec->load(altName, info);
		else
			rtn = codec->open(altName);

		if( rtn )
		{
			if( orgNameFailed )
			{
				Com_DPrintf(S_COLOR_YELLOW "WARNING: %s not present, using %s instead\n",
						filename, altName );
			}

			return rtn;
		}
	}

	Com_Printf(S_COLOR_YELLOW "WARNING: Failed to %s sound %s!\n", info ? "load" : "open", filename);

	return NULL;
}

void S_CodecInit(void)
{
	codecs = NULL;

#ifdef USE_CODEC_OPUS
#endif

#ifdef USE_CODEC_VORBIS
#endif

#ifdef USE_CODEC_MAD
	S_CodecRegister(&mp3_codec);
#endif

	S_CodecRegister(&wav_codec);
}

void S_CodecShutdown(void)
{
	codecs = NULL;
}

void S_CodecRegister(snd_codec_t *codec)
{
	codec->next = codecs;
	codecs = codec;
}

void *S_CodecLoad(const char *filename, snd_info_t *info)
{
	return S_CodecGetSound(filename, info);
}

snd_stream_t *S_CodecOpenStream(const char *filename)
{
	return S_CodecGetSound(filename, NULL);
}

void S_CodecCloseStream(snd_stream_t *stream)
{
	stream->codec->close(stream);
}

int S_CodecReadStream(snd_stream_t *stream, int bytes, void *buffer)
{
	return stream->codec->read(stream, bytes, buffer);
}


snd_stream_t *S_CodecUtilOpen(const char *filename, snd_codec_t *codec)
{
	snd_stream_t *stream;
	fileHandle_t hnd;
	int length;

	length = FS_FOpenFileRead(filename, &hnd, qtrue, qtrue);
	if(!hnd)
	{
		Com_DPrintf("Can't read sound file %s\n", filename);
		return NULL;
	}

	stream = Z_Malloc(sizeof(snd_stream_t));
	if(!stream)
	{
		FS_FCloseFile(hnd);
		return NULL;
	}

	stream->codec = codec;
	stream->file = hnd;
	stream->length = length;
	return stream;
}

void S_CodecUtilClose(snd_stream_t **stream)
{
	FS_FCloseFile((*stream)->file);
	Z_Free(*stream);
	*stream = NULL;
}
