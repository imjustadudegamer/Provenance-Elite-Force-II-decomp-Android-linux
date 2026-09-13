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
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
#ifndef SMP_HWTEX_H
#define SMP_HWTEX_H

typedef enum {
	SMP_TEXC_NONE = 0,
	SMP_TEXC_ETC2_RGBA8,	
	SMP_TEXC_ASTC_4x4		
} smpTexTranscode_t;

#define SMP_DXT1 1
#define SMP_DXT3 3
#define SMP_DXT5 5

#define SMP_TEXC_BLOCK_BYTES 16

int SMP_SelectTexTranscode(int vkCoreMajor, int vkCoreMinor,
							int hasS3tc, int hasEtc2Ext, int hasArbEs3,
							int hasAstcLdr, int compressAllowed);

void SMP_InitCodecTables(void);

void SMP_DXTDecodeBlock(int dxt, const unsigned char *block, unsigned char rgba[64]);

void SMP_TranscodeBlock(int dxt, const unsigned char *src, int target, unsigned char *dst);
void SMP_TranscodeRGBA(const unsigned char rgba[64], int target, unsigned char *dst);

typedef struct {
	const unsigned char *src;	
	unsigned char		*dst;	
	int					 dxt;
	int					 target;
	int					 bw;	
} smpTranscodeRows_t;

void SMP_TranscodeRows(void *vctx, int begin, int end);

void SMP_MipMap(unsigned char *in, int width, int height);

/* The number of levels the software upload's mip loop emits for a base of
 * width x height: level 0 plus one per halving until both reach 1. */
int SMP_MipLevelCount(int width, int height);

int SMP_TranscodeChainBytes(int upW, int upH, int levels);

typedef void (*smpTranscodeRunner_t)(void *ud, smpTranscodeRows_t *ctx, int bh, int bw);

int SMP_TranscodeChain(const unsigned char *data, int srcDxt, int width, int height,
						int iMipmapsAvailable, int base,
						int upW, int upH, int levels, int target,
						unsigned char *out,
						void *(*alloc)(int), void (*dealloc)(void *),
						smpTranscodeRunner_t runRows, void *runUd);

#endif 
