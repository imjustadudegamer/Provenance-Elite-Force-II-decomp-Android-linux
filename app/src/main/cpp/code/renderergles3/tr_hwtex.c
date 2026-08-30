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
#include "tr_hwtex.h"

#include <string.h>

int SMP_SelectTexTranscode(int glesMajor, int glesMinor,
							int hasS3tc, int hasEtc2Ext, int hasArbEs3,
							int hasAstcLdr, int compressAllowed)
{
	int coreEtc2;

	(void)hasS3tc;

	if (!compressAllowed) {
		return SMP_TEXC_NONE;
	}

	coreEtc2 = (glesMajor > 3 || (glesMajor == 3 && glesMinor >= 0)) ? 1 : 0;

	if (hasAstcLdr) {
		return SMP_TEXC_ASTC_4x4;
	}
	if (coreEtc2 || hasEtc2Ext || hasArbEs3) {
		return SMP_TEXC_ETC2_RGBA8;
	}
	return SMP_TEXC_NONE;
}

static void SMP_DXTExpand565(unsigned short c, unsigned char *out)
{
	out[0] = (unsigned char)((((c >> 11) & 0x1F) * 255 + 15) / 31);
	out[1] = (unsigned char)((((c >> 5) & 0x3F) * 255 + 31) / 63);
	out[2] = (unsigned char)(((c & 0x1F) * 255 + 15) / 31);
	out[3] = 255;
}

static void SMP_DXTDecodeColorBlock(const unsigned char *b, unsigned char out[64], int dxt1)
{
	unsigned short c0 = (unsigned short)(b[0] | (b[1] << 8));
	unsigned short c1 = (unsigned short)(b[2] | (b[3] << 8));
	unsigned char  pal[4][4];
	unsigned int   bits;
	int            i, k;

	SMP_DXTExpand565(c0, pal[0]);
	SMP_DXTExpand565(c1, pal[1]);

	if (!dxt1 || c0 > c1) {
		for (k = 0; k < 3; k++) {
			pal[2][k] = (unsigned char)((2 * pal[0][k] + pal[1][k]) / 3);
			pal[3][k] = (unsigned char)((pal[0][k] + 2 * pal[1][k]) / 3);
		}
		pal[2][3] = 255;
		pal[3][3] = 255;
	} else {
		for (k = 0; k < 3; k++) {
			pal[2][k] = (unsigned char)((pal[0][k] + pal[1][k]) / 2);
			pal[3][k] = 0;
		}
		pal[2][3] = 255;
		pal[3][3] = 0; 
	}

	bits = (unsigned int)(b[4] | (b[5] << 8) | (b[6] << 16) | ((unsigned int)b[7] << 24));
	for (i = 0; i < 16; i++) {
		int idx = (bits >> (2 * i)) & 3;

		out[i * 4 + 0] = pal[idx][0];
		out[i * 4 + 1] = pal[idx][1];
		out[i * 4 + 2] = pal[idx][2];
		out[i * 4 + 3] = pal[idx][3];
	}
}

void SMP_DXTDecodeBlock(int dxt, const unsigned char *block, unsigned char rgba[64])
{
	const unsigned char *colorBlock = (dxt == SMP_DXT1) ? block : block + 8;
	unsigned char       alpha[16];
	int                 i;

	SMP_DXTDecodeColorBlock(colorBlock, rgba, (dxt == SMP_DXT1) ? 1 : 0);

	if (dxt == SMP_DXT3) {
		for (i = 0; i < 16; i++) {
			int a = (block[i >> 1] >> ((i & 1) * 4)) & 0xF;

			alpha[i] = (unsigned char)(a * 255 / 15);
		}
	} else if (dxt == SMP_DXT5) {
		unsigned char      ac[8];
		unsigned long long abits = 0;
		unsigned char      a0 = block[0];
		unsigned char      a1 = block[1];

		ac[0] = a0;
		ac[1] = a1;
		if (a0 > a1) {
			for (i = 1; i < 7; i++) {
				ac[i + 1] = (unsigned char)(((7 - i) * a0 + i * a1) / 7);
			}
		} else {
			for (i = 1; i < 5; i++) {
				ac[i + 1] = (unsigned char)(((5 - i) * a0 + i * a1) / 5);
			}
			ac[6] = 0;
			ac[7] = 255;
		}
		for (i = 0; i < 6; i++) {
			abits |= ((unsigned long long)block[2 + i]) << (8 * i);
		}
		for (i = 0; i < 16; i++) {
			alpha[i] = ac[(abits >> (3 * i)) & 7];
		}
	} else {
		for (i = 0; i < 16; i++) {
			alpha[i] = rgba[i * 4 + 3];
		}
	}

	for (i = 0; i < 16; i++) {
		rgba[i * 4 + 3] = alpha[i];
	}
}


static const int s_etc2Mod[8][4] = {
	{   2,    8,   -2,    -8 },
	{   5,   17,   -5,   -17 },
	{   9,   29,   -9,   -29 },
	{  13,   42,  -13,   -42 },
	{  18,   60,  -18,   -60 },
	{  24,   80,  -24,   -80 },
	{  33,  106,  -33,  -106 },
	{  47,  183,  -47,  -183 }
};

static const int s_eacMod[16][8] = {
	{  -3,  -6,  -9, -15,   2,   5,   8,  14 },
	{  -3,  -7, -10, -13,   2,   6,   9,  12 },
	{  -2,  -5,  -8, -13,   1,   4,   7,  12 },
	{  -2,  -4,  -6, -13,   1,   3,   5,  12 },
	{  -3,  -6,  -8, -12,   2,   5,   7,  11 },
	{  -3,  -7,  -9, -11,   2,   6,   8,  10 },
	{  -4,  -7,  -8, -11,   3,   6,   7,  10 },
	{  -3,  -5,  -8, -11,   2,   4,   7,  10 },
	{  -2,  -6,  -8, -10,   1,   5,   7,   9 },
	{  -2,  -5,  -8, -10,   1,   4,   7,   9 },
	{  -2,  -4,  -8, -10,   1,   3,   7,   9 },
	{  -2,  -5,  -7, -10,   1,   4,   6,   9 },
	{  -3,  -4,  -7, -10,   2,   3,   6,   9 },
	{  -1,  -2,  -3, -10,   0,   1,   2,   9 },
	{  -4,  -6,  -8,  -9,   3,   5,   7,   8 },
	{  -3,  -5,  -7,  -9,   2,   4,   6,   8 }
};

static int etc2Clamp255(int v)
{
	if (v < 0) return 0;
	if (v > 255) return 255;
	return v;
}

static int etc2Quant4(int v)
{
	int q = (v * 15 + 127) / 255;

	if (q < 0) q = 0;
	if (q > 15) q = 15;
	return q;
}

static int etc2Quant5(int v)
{
	int q = (v * 31 + 127) / 255;

	if (q < 0) q = 0;
	if (q > 31) q = 31;
	return q;
}

static int etc2Extend4(int v)
{
	return v * 16 + v; 
}

static int etc2Extend5(int v)
{
	return (v << 3) | (v >> 2); 
}

#define SMP_ETC2_S_RANGE 1531
static unsigned char s_etc2BestJ[8][SMP_ETC2_S_RANGE];
static signed char   s_etc2BestMod[8][SMP_ETC2_S_RANGE];
static int           s_etc2TablesReady = 0;

void SMP_InitCodecTables(void)
{
	int t, i;

	if (s_etc2TablesReady) {
		return;
	}
	for (t = 0; t < 8; t++) {
		for (i = 0; i < SMP_ETC2_S_RANGE; i++) {
			int S = i - 765, j, bj = 0, bc = 0x7fffffff;

			for (j = 0; j < 4; j++) {
				int m = s_etc2Mod[t][j];
				int c = 3 * m * m - 2 * m * S;

				if (c < bc) {
					bc = c;
					bj = j;
				}
			}
			s_etc2BestJ[t][i] = (unsigned char)bj;
			s_etc2BestMod[t][i] = (signed char)s_etc2Mod[t][bj];
		}
	}
	s_etc2TablesReady = 1;
}

static int etc2TableCost(const int px[8][3], const int base[3], int t,
						 int modOut[8], int idxOut[8])
{
	int p, err = 0;
	const unsigned char *jTab = s_etc2BestJ[t];
	const signed char   *mTab = s_etc2BestMod[t];

	for (p = 0; p < 8; p++) {
		int dr = px[p][0] - base[0];
		int dg = px[p][1] - base[1];
		int db = px[p][2] - base[2];
		int S = dr + dg + db;
		int si = S + 765;
		int m;

		if (si < 0) si = 0;
		if (si >= SMP_ETC2_S_RANGE) si = SMP_ETC2_S_RANGE - 1;
		m = mTab[si];
		if (modOut) modOut[p] = m;
		if (idxOut) idxOut[p] = jTab[si];
		err += (dr - m) * (dr - m) + (dg - m) * (dg - m) + (db - m) * (db - m);
	}
	return err;
}

typedef struct {
	int flip;
	int sub[16];
	int slot[16];
} etc2Map_t;

static void etc2BuildMap(int flip, etc2Map_t *m)
{
	int cnt[2];
	int l;

	m->flip = flip;
	cnt[0] = cnt[1] = 0;
	for (l = 0; l < 16; l++) {
		int x = l >> 2, y = l & 3;
		int s = flip ? (y >> 1) : (x >> 1);

		m->sub[l] = s;
		m->slot[l] = cnt[s]++;
	}
}

static void etc2GatherSubblock(const unsigned char rgba[64], const etc2Map_t *m,
							   int sub, int px[8][3])
{
	int l;

	for (l = 0; l < 16; l++) {
		int x = l >> 2, y = l & 3;
		int src;

		if (m->sub[l] != sub) {
			continue;
		}
		src = (y * 4 + x) * 4;
		px[m->slot[l]][0] = rgba[src + 0];
		px[m->slot[l]][1] = rgba[src + 1];
		px[m->slot[l]][2] = rgba[src + 2];
	}
}

static const int s_etc2Reach[8] = { 8, 17, 29, 42, 60, 80, 106, 183 };

static int etc2SubblockFixed(const int px[8][3], int differential, const int baseQ[3],
							 int *tableOut, int idxOut[8])
{
	int base[3], t, bestErr = 0x7fffffff, bestT = 0, k;
	int mn = 0x7fffffff, mx = -0x7fffffff, seed, lo, hi;

	for (k = 0; k < 3; k++) {
		base[k] = differential ? etc2Extend5(baseQ[k]) : etc2Extend4(baseQ[k]);
	}
	for (k = 0; k < 8; k++) {
		int S = (px[k][0] - base[0]) + (px[k][1] - base[1]) + (px[k][2] - base[2]);

		if (S < mn) mn = S;
		if (S > mx) mx = S;
	}
	{
		int need = (mn < 0 ? -mn : mn);
		int bd = 0x7fffffff;

		if (mx > need) need = mx;
		seed = 0;
		for (t = 0; t < 8; t++) {
			int d = s_etc2Reach[t] * 3 - need;

			if (d < 0) d = -d;
			if (d < bd) {
				bd = d;
				seed = t;
			}
		}
	}
	lo = seed - 2;
	hi = seed + 2;
	if (lo < 0) lo = 0;
	if (hi > 7) hi = 7;
	for (t = lo; t <= hi; t++) {
		int e = etc2TableCost(px, base, t, NULL, NULL);

		if (e < bestErr) {
			bestErr = e;
			bestT = t;
		}
	}
	etc2TableCost(px, base, bestT, NULL, idxOut);
	*tableOut = bestT;
	return bestErr;
}

static int etc2SubblockRefine4(const int px[8][3], int baseQ[3],
							   int *tableOut, int idxOut[8])
{
	int iter, k;
	int bestErr = 0x7fffffff;

	for (iter = 0; iter < 2; iter++) {
		int base[3], mods[8], newQ[3], same = 1;

		for (k = 0; k < 3; k++) {
			base[k] = etc2Extend4(baseQ[k]);
		}
		bestErr = etc2SubblockFixed(px, 0, baseQ, tableOut, idxOut);
		etc2TableCost(px, base, *tableOut, mods, NULL);
		for (k = 0; k < 3; k++) {
			int sum = 0, p;

			for (p = 0; p < 8; p++) {
				sum += px[p][k] - mods[p];
			}
			newQ[k] = etc2Quant4((sum + 4) / 8);
			if (newQ[k] != baseQ[k]) {
				same = 0;
			}
		}
		if (same) {
			break;
		}
		for (k = 0; k < 3; k++) {
			baseQ[k] = newQ[k];
		}
	}
	bestErr = etc2SubblockFixed(px, 0, baseQ, tableOut, idxOut);
	return bestErr;
}

static void etc2PutBits(unsigned long long *lo, unsigned long long *hi,
						int pos, unsigned long long val, int nbits)
{
	int i;

	for (i = 0; i < nbits; i++) {
		if (((val >> i) & 1u) != 0) {
			int b = pos + i;

			if (b < 64) {
				*lo |= 1ull << b;
			} else {
				*hi |= 1ull << (b - 64);
			}
		}
	}
}

static void smpPutBitsDown(unsigned long long *lo, unsigned long long *hi,
							int pos, unsigned long long val, int nbits)
{
	int i;

	for (i = 0; i < nbits; i++) {
		if (((val >> i) & 1u) != 0) {
			int b = pos - i;

			if (b < 0) {
				break;
			}
			if (b < 64) {
				*lo |= 1ull << b;
			} else {
				*hi |= 1ull << (b - 64);
			}
		}
	}
}

static void etc2PackColorBlock(const etc2Map_t *m, int differential,
							   const int base5[3], const int delta5[3],
							   const int base4a[3], const int base4b[3],
							   int table1, int table2,
							   const int idx1[8], const int idx2[8],
							   unsigned char *out)
{
	unsigned long long hi = 0, lo = 0;
	int                l;

	for (l = 0; l < 16; l++) {
		int s = m->sub[l], slot = m->slot[l];
		int bit = (s == 0) ? idx1[slot] : idx2[slot];

		if ((bit & 1) != 0) {
			lo |= 1u << l;
		}
		if ((bit & 2) != 0) {
			lo |= 1u << (l + 16);
		}
	}

	if (differential) {
		hi |= (unsigned int)(base5[0] & 0x1F) << 27;
		hi |= (unsigned int)(delta5[0] & 0x07) << 24;
		hi |= (unsigned int)(base5[1] & 0x1F) << 19;
		hi |= (unsigned int)(delta5[1] & 0x07) << 16;
		hi |= (unsigned int)(base5[2] & 0x1F) << 11;
		hi |= (unsigned int)(delta5[2] & 0x07) << 8;
		hi |= (unsigned int)(table1 & 0x07) << 5;
		hi |= (unsigned int)(table2 & 0x07) << 2;
		hi |= 1u << 1; 
		hi |= (unsigned int)(m->flip & 1);
	} else {
		hi |= (unsigned int)(base4a[0] & 0x0F) << 28;
		hi |= (unsigned int)(base4b[0] & 0x0F) << 24;
		hi |= (unsigned int)(base4a[1] & 0x0F) << 20;
		hi |= (unsigned int)(base4b[1] & 0x0F) << 16;
		hi |= (unsigned int)(base4a[2] & 0x0F) << 12;
		hi |= (unsigned int)(base4b[2] & 0x0F) << 8;
		hi |= (unsigned int)(table1 & 0x07) << 5;
		hi |= (unsigned int)(table2 & 0x07) << 2;
		hi |= (unsigned int)(m->flip & 1);
	}

	out[0] = (unsigned char)((hi >> 24) & 0xFF);
	out[1] = (unsigned char)((hi >> 16) & 0xFF);
	out[2] = (unsigned char)((hi >> 8) & 0xFF);
	out[3] = (unsigned char)(hi & 0xFF);
	out[4] = (unsigned char)((lo >> 24) & 0xFF);
	out[5] = (unsigned char)((lo >> 16) & 0xFF);
	out[6] = (unsigned char)((lo >> 8) & 0xFF);
	out[7] = (unsigned char)(lo & 0xFF);
}

static int etc2EncodeColor(const unsigned char rgba[64], unsigned char out[8])
{
	int flip, bestErr = 0x7fffffff;
	int bestDiff = 0;
	etc2Map_t map, bestMap;
	int bestBase5[3] = { 0, 0, 0 }, bestDelta5[3] = { 0, 0, 0 };
	int bestBase4a[3] = { 0, 0, 0 }, bestBase4b[3] = { 0, 0, 0 };
	int bestT1 = 0, bestT2 = 0;
	int bestIdx1[8], bestIdx2[8];

	memset(bestIdx1, 0, sizeof(bestIdx1));
	memset(bestIdx2, 0, sizeof(bestIdx2));
	memset(&bestMap, 0, sizeof(bestMap));

	for (flip = 0; flip < 2; flip++) {
		int pxA[8][3], pxB[8][3];
		int k;

		etc2BuildMap(flip, &map);
		etc2GatherSubblock(rgba, &map, 0, pxA);
		etc2GatherSubblock(rgba, &map, 1, pxB);

		{
			int baseA[3], baseB[3], t1, t2, idx1[8], idx2[8], err;

			for (k = 0; k < 3; k++) {
				int sA = 0, sB = 0, p;

				for (p = 0; p < 8; p++) {
					sA += pxA[p][k];
					sB += pxB[p][k];
				}
				baseA[k] = etc2Quant4((sA + 4) / 8);
				baseB[k] = etc2Quant4((sB + 4) / 8);
			}
			err = etc2SubblockRefine4(pxA, baseA, &t1, idx1);
			err += etc2SubblockRefine4(pxB, baseB, &t2, idx2);

			if (err < bestErr) {
				bestErr = err;
				bestMap = map;
				bestDiff = 0;
				memcpy(bestBase4a, baseA, sizeof(baseA));
				memcpy(bestBase4b, baseB, sizeof(baseB));
				bestT1 = t1;
				bestT2 = t2;
				memcpy(bestIdx1, idx1, sizeof(idx1));
				memcpy(bestIdx2, idx2, sizeof(idx2));
			}
		}

		{
			int base5[3], delta[3], t1, t2, idx1[8], idx2[8], err = 0;
			int iter;

			for (k = 0; k < 3; k++) {
				int sA = 0, sB = 0, p, qa, qb, d;

				for (p = 0; p < 8; p++) {
					sA += pxA[p][k];
					sB += pxB[p][k];
				}
				qa = etc2Quant5((sA + 4) / 8);
				qb = etc2Quant5((sB + 4) / 8);
				d = qb - qa;
				if (d > 3) d = 3;
				if (d < -4) d = -4;
				if (qa + d < 0) qa = -d;
				if (qa + d > 31) qa = 31 - d;
				base5[k] = qa;
				delta[k] = d;
			}

			t1 = t2 = 0;
			for (iter = 0; iter < 2; iter++) {
				int b2[3], modsA[8], modsB[8], eA, eB, same = 1;
				int bA8[3], bB8[3];

				for (k = 0; k < 3; k++) {
					b2[k] = base5[k] + delta[k];
					bA8[k] = etc2Extend5(base5[k]);
					bB8[k] = etc2Extend5(b2[k]);
				}
				eA = etc2SubblockFixed(pxA, 1, base5, &t1, idx1);
				eB = etc2SubblockFixed(pxB, 1, b2, &t2, idx2);
				err = eA + eB;

				etc2TableCost(pxA, bA8, t1, modsA, NULL);
				etc2TableCost(pxB, bB8, t2, modsB, NULL);
				for (k = 0; k < 3; k++) {
					int sA = 0, sB = 0, p, n1, n2, nd;

					for (p = 0; p < 8; p++) {
						sA += pxA[p][k] - modsA[p];
						sB += pxB[p][k] - modsB[p];
					}
					n1 = etc2Quant5((sA + 4) / 8);
					n2 = etc2Quant5((sB + 4) / 8);
					nd = n2 - n1;
					if (nd > 3) nd = 3;
					if (nd < -4) nd = -4;
					if (n1 + nd < 0) n1 = -nd;
					if (n1 + nd > 31) n1 = 31 - nd;
					if (n1 != base5[k] || nd != delta[k]) {
						same = 0;
					}
					base5[k] = n1;
					delta[k] = nd;
				}
				if (same) {
					break;
				}
			}

			if (err < bestErr) {
				int b2[3];

				for (k = 0; k < 3; k++) {
					b2[k] = base5[k] + delta[k];
				}
				(void)b2;
				bestErr = err;
				bestMap = map;
				bestDiff = 1;
				memcpy(bestBase5, base5, sizeof(base5));
				memcpy(bestDelta5, delta, sizeof(delta));
				bestT1 = t1;
				bestT2 = t2;
				memcpy(bestIdx1, idx1, sizeof(idx1));
				memcpy(bestIdx2, idx2, sizeof(idx2));
			}
		}
	}

	etc2PackColorBlock(&bestMap, bestDiff, bestBase5, bestDelta5,
					   bestBase4a, bestBase4b, bestT1, bestT2,
					   bestIdx1, bestIdx2, out);
	return bestErr;
}

static void etc2EncodeAlpha(const unsigned char rgba[64], unsigned char out[8])
{
	int a[16], i, t;
	int lo = 255, hi = 0;
	int bestErr = 0x7fffffff, bestT = 0, bestM = 1, bestBase = 255;
	int bestIdx[16];

	for (i = 0; i < 16; i++) {
		int x = i >> 2, y = i & 3; 

		a[i] = rgba[(y * 4 + x) * 4 + 3];
		if (a[i] < lo) lo = a[i];
		if (a[i] > hi) hi = a[i];
	}
	for (i = 0; i < 16; i++) {
		bestIdx[i] = 0;
	}

	for (t = 0; t < 16; t++) {
		int maxM = 0, minM = 0, span, m, dm;

		for (i = 0; i < 8; i++) {
			if (s_eacMod[t][i] > maxM) maxM = s_eacMod[t][i];
			if (s_eacMod[t][i] < minM) minM = s_eacMod[t][i];
		}
		span = maxM - minM;
		if (span <= 0) {
			continue;
		}
		m = ((hi - lo) * 2 + span) / (2 * span);

		for (dm = -1; dm <= 1; dm++) {
			int mm = m + dm;
			int base, err = 0, idx[16];

			if (mm < 1) mm = 1;
			if (mm > 15) mm = 15;

			base = (lo + hi) / 2 - (mm * (maxM + minM)) / 2;
			base = etc2Clamp255(base);

			for (i = 0; i < 16; i++) {
				int j, bestJ = 0, bestC = 0x7fffffff;

				for (j = 0; j < 8; j++) {
					int c = base + mm * s_eacMod[t][j] - a[i];

					c = c * c;
					if (c < bestC) {
						bestC = c;
						bestJ = j;
					}
				}
				idx[i] = bestJ;
				err += bestC;
			}

			if (err < bestErr) {
				bestErr = err;
				bestT = t;
				bestM = mm;
				bestBase = base;
				memcpy(bestIdx, idx, sizeof(idx));
			}
		}
	}

	for (t = 0; t < 2; t++) {
		int sum = 0, nb, err = 0, idx[16];

		for (i = 0; i < 16; i++) {
			sum += a[i] - bestM * s_eacMod[bestT][bestIdx[i]];
		}
		nb = etc2Clamp255((sum + 8) / 16);
		if (nb == bestBase) {
			break;
		}
		for (i = 0; i < 16; i++) {
			int j, bestJ = 0, bestC = 0x7fffffff;

			for (j = 0; j < 8; j++) {
				int c = nb + bestM * s_eacMod[bestT][j] - a[i];

				c = c * c;
				if (c < bestC) {
					bestC = c;
					bestJ = j;
				}
			}
			idx[i] = bestJ;
			err += bestC;
		}
		if (err <= bestErr) {
			bestErr = err;
			bestBase = nb;
			memcpy(bestIdx, idx, sizeof(idx));
		} else {
			break;
		}
	}

	{
		unsigned long long w = 0;

		w |= (unsigned long long)(bestBase & 0xFF) << 56;
		w |= (unsigned long long)(bestM & 0x0F) << 52;
		w |= (unsigned long long)(bestT & 0x0F) << 48;
		for (i = 0; i < 16; i++) {
			w |= (unsigned long long)(bestIdx[i] & 0x07) << (45 - 3 * i);
		}
		for (i = 0; i < 8; i++) {
			out[i] = (unsigned char)((w >> (56 - 8 * i)) & 0xFF);
		}
	}
}

#define SMP_ASTC_MODE_2BIT 0x42u	

static const int s_astcLvl2[4] = { 0, 21, 43, 64 };

static void astcApply(const int e0[4], const int e1[4], const int wVal[16], unsigned char out[64])
{
	int i, k;

	for (i = 0; i < 16; i++) {
		for (k = 0; k < 4; k++) {
			int c0 = (e0[k] << 8) | e0[k];
			int c1 = (e1[k] << 8) | e1[k];
			int c = (c0 * (64 - wVal[i]) + c1 * wVal[i] + 32) / 64;

			out[i * 4 + k] = (unsigned char)(c >> 8);
		}
	}
}

static int astcError(const unsigned char a[64], const unsigned char b[64])
{
	int i, e = 0;

	for (i = 0; i < 64; i++) {
		int d = (int)a[i] - (int)b[i];

		e += d * d;
	}
	return e;
}

static int astcRound255(double v)
{
	if (v < 0) return 0;
	if (v > 255) return 255;
	return (int)(v + 0.5);
}

static int astcEncodeCem12(const unsigned char rgba[64], unsigned char out[16])
{
	double e0[4], e1[4];
	int    wIdx[16], wVal[16], e0i[4], e1i[4], i, k, iter;
	unsigned char tmp[64];
	int    err;

	for (k = 0; k < 4; k++) {
		int mn = 255, mx = 0;

		for (i = 0; i < 16; i++) {
			int v = rgba[i * 4 + k];

			if (v < mn) mn = v;
			if (v > mx) mx = v;
		}
		e0[k] = mn;
		e1[k] = mx;
	}

	for (iter = 0; iter < 2; iter++) {
		double dir[4], dd = 0, s1 = 0, s2 = 0, s3 = 0, s4[4], s5[4], ne0[4], ne1[4];

		for (k = 0; k < 4; k++) {
			dir[k] = e1[k] - e0[k];
			dd += dir[k] * dir[k];
			s4[k] = 0;
			s5[k] = 0;
		}
		if (dd < 1.0) {
			dd = 1.0;
		}

		for (i = 0; i < 16; i++) {
			double proj = 0, t, it, ot;
			int    bestJ = 0;
			double bestD = 1e30;

			for (k = 0; k < 4; k++) {
				proj += (rgba[i * 4 + k] - e0[k]) * dir[k];
			}
			t = proj / dd;
			if (t < 0) t = 0;
			if (t > 1) t = 1;
			for (k = 0; k < 4; k++) {
				double lvl = s_astcLvl2[k] / 64.0;
				double d = t - lvl;

				if (d < 0) d = -d;
				if (d < bestD) {
					bestD = d;
					bestJ = k;
				}
			}
			wIdx[i] = bestJ;
			it = s_astcLvl2[bestJ] / 64.0;
			ot = 1.0 - it;
			s1 += ot * ot;
			s2 += ot * it;
			s3 += it * it;
			for (k = 0; k < 4; k++) {
				s4[k] += ot * rgba[i * 4 + k];
				s5[k] += it * rgba[i * 4 + k];
			}
		}

		{
			double det = s1 * s3 - s2 * s2;

			if (det < 1e-6) {
				break;
			}
			for (k = 0; k < 4; k++) {
				ne0[k] = astcRound255((s4[k] * s3 - s5[k] * s2) / det);
				ne1[k] = astcRound255((s1 * s5[k] - s2 * s4[k]) / det);
			}
		}
		if (ne0[0] == e0[0] && ne0[1] == e0[1] && ne0[2] == e0[2] && ne0[3] == e0[3] &&
			ne1[0] == e1[0] && ne1[1] == e1[1] && ne1[2] == e1[2] && ne1[3] == e1[3]) {
			break;
		}
		for (k = 0; k < 4; k++) {
			e0[k] = ne0[k];
			e1[k] = ne1[k];
		}
	}

	if ((e1[0] + e1[1] + e1[2]) < (e0[0] + e0[1] + e0[2])) {
		double tmpd;

		for (k = 0; k < 4; k++) {
			tmpd = e0[k];
			e0[k] = e1[k];
			e1[k] = tmpd;
		}
		for (i = 0; i < 16; i++) {
			wIdx[i] = 3 - wIdx[i];
		}
	}

	{
		double dir[4], dd = 0;

		for (k = 0; k < 4; k++) {
			dir[k] = e1[k] - e0[k];
			dd += dir[k] * dir[k];
		}
		if (dd < 1.0) {
			dd = 1.0;
		}
		for (i = 0; i < 16; i++) {
			double proj = 0, t;
			int    bestJ = 0;
			double bestD = 1e30;

			for (k = 0; k < 4; k++) {
				proj += (rgba[i * 4 + k] - e0[k]) * dir[k];
			}
			t = proj / dd;
			if (t < 0) t = 0;
			if (t > 1) t = 1;
			for (k = 0; k < 4; k++) {
				double lvl = s_astcLvl2[k] / 64.0;
				double d = t - lvl;

				if (d < 0) d = -d;
				if (d < bestD) {
					bestD = d;
					bestJ = k;
				}
			}
			wIdx[i] = bestJ;
		}
	}

	for (k = 0; k < 4; k++) {
		e0i[k] = astcRound255(e0[k]);
		e1i[k] = astcRound255(e1[k]);
	}
	for (i = 0; i < 16; i++) {
		wVal[i] = s_astcLvl2[wIdx[i]];
	}

	astcApply(e0i, e1i, wVal, tmp);
	err = astcError(rgba, tmp);

	{
		unsigned long long lo = 0, hi = 0;
		int                vals[8];

		etc2PutBits(&lo, &hi, 0, (unsigned long long)SMP_ASTC_MODE_2BIT | (12ull << 13), 17);
		vals[0] = e0i[0]; vals[1] = e1i[0];
		vals[2] = e0i[1]; vals[3] = e1i[1];
		vals[4] = e0i[2]; vals[5] = e1i[2];
		vals[6] = e0i[3]; vals[7] = e1i[3];
		for (i = 0; i < 8; i++) {
			etc2PutBits(&lo, &hi, 17 + 8 * i, (unsigned long long)(vals[i] & 0xFF), 8);
		}
		for (i = 0; i < 16; i++) {
			smpPutBitsDown(&lo, &hi, 127 - 2 * i, (unsigned long long)(wIdx[i] & 3), 2);
		}
		for (i = 0; i < 8; i++) {
			out[i] = (unsigned char)((lo >> (8 * i)) & 0xFF);
			out[8 + i] = (unsigned char)((hi >> (8 * i)) & 0xFF);
		}
	}
	return err;
}

static void astcEncode(const unsigned char rgba[64], unsigned char out[16])
{
	(void)astcEncodeCem12(rgba, out);
}

void SMP_TranscodeRGBA(const unsigned char rgba[64], int target, unsigned char *dst)
{
	if (target == SMP_TEXC_ASTC_4x4) {
		astcEncode(rgba, dst);
	} else if (target == SMP_TEXC_ETC2_RGBA8) {
		etc2EncodeAlpha(rgba, dst);
		etc2EncodeColor(rgba, dst + 8);
	} else {
		memset(dst, 0, SMP_TEXC_BLOCK_BYTES);
	}
}

void SMP_TranscodeBlock(int dxt, const unsigned char *src, int target, unsigned char *dst)
{
	unsigned char rgba[64];

	SMP_DXTDecodeBlock(dxt, src, rgba);
	SMP_TranscodeRGBA(rgba, target, dst);
}

void SMP_TranscodeRows(void *vctx, int begin, int end)
{
	const smpTranscodeRows_t *c = (const smpTranscodeRows_t *)vctx;
	int                        by;

	for (by = begin; by < end; by++) {
		int bx;

		for (bx = 0; bx < c->bw; bx++) {
			int i = by * c->bw + bx;

			SMP_TranscodeBlock(c->dxt, c->src + i * (c->dxt == SMP_DXT1 ? 8 : 16),
								c->target, c->dst + i * SMP_TEXC_BLOCK_BYTES);
		}
	}
}


int SMP_MipLevelCount(int width, int height)
{
	int levels = 1;

	if (width < 1) width = 1;
	if (height < 1) height = 1;

	while (width > 1 || height > 1) {
		width >>= 1;
		height >>= 1;
		if (width < 1) width = 1;
		if (height < 1) height = 1;
		levels++;
	}
	return levels;
}


void SMP_MipMap(unsigned char *in, int width, int height)
{
	int   i, j;
	unsigned char *out;
	int   row;

	if (width == 1 && height == 1) {
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if (width == 0 || height == 0) {
		width += height;	
		for (i = 0; i < width; i++, out += 4, in += 8) {
			out[0] = (unsigned char)((in[0] + in[4]) >> 1);
			out[1] = (unsigned char)((in[1] + in[5]) >> 1);
			out[2] = (unsigned char)((in[2] + in[6]) >> 1);
			out[3] = (unsigned char)((in[3] + in[7]) >> 1);
		}
		return;
	}

	for (i = 0; i < height; i++, in += row) {
		for (j = 0; j < width; j++, out += 4, in += 8) {
			out[0] = (unsigned char)((in[0] + in[4] + in[row + 0] + in[row + 4]) >> 2);
			out[1] = (unsigned char)((in[1] + in[5] + in[row + 1] + in[row + 5]) >> 2);
			out[2] = (unsigned char)((in[2] + in[6] + in[row + 2] + in[row + 6]) >> 2);
			out[3] = (unsigned char)((in[3] + in[7] + in[row + 3] + in[row + 7]) >> 2);
		}
	}
}

static void SMP_DecodeLevelToRGBA(const unsigned char *src, int dxt, int w, int h,
								   unsigned char *rgba)
{
	int bw = (w + 3) / 4;
	int bh = (h + 3) / 4;
	int blockBytes = (dxt == SMP_DXT1) ? 8 : 16;
	int bx, by, x, y, k;

	for (by = 0; by < bh; by++) {
		for (bx = 0; bx < bw; bx++) {
			unsigned char blockRGBA[64];

			SMP_DXTDecodeBlock(dxt, src + (by * bw + bx) * blockBytes, blockRGBA);
			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					int tx = bx * 4 + x;
					int ty = by * 4 + y;

					if (tx >= w || ty >= h) {
						continue;
					}
					for (k = 0; k < 4; k++) {
						rgba[((size_t)ty * w + tx) * 4 + k] = blockRGBA[(y * 4 + x) * 4 + k];
					}
				}
			}
		}
	}
}

static void SMP_EncodeLevelToBlocks(const unsigned char *rgba, int w, int h, int target,
									 unsigned char *out)
{
	int bw = (w + 3) / 4;
	int bh = (h + 3) / 4;
	int bx, by, x, y, k;

	for (by = 0; by < bh; by++) {
		for (bx = 0; bx < bw; bx++) {
			unsigned char blockRGBA[64];

			for (y = 0; y < 4; y++) {
				for (x = 0; x < 4; x++) {
					int tx = bx * 4 + x;
					int ty = by * 4 + y;

					if (tx >= w) tx = w - 1;
					if (ty >= h) ty = h - 1;
					for (k = 0; k < 4; k++) {
						blockRGBA[(y * 4 + x) * 4 + k] = rgba[((size_t)ty * w + tx) * 4 + k];
					}
				}
			}
			SMP_TranscodeRGBA(blockRGBA, target, out + (by * bw + bx) * SMP_TEXC_BLOCK_BYTES);
		}
	}
}

int SMP_TranscodeChainBytes(int upW, int upH, int levels)
{
	int w = upW, h = upH, lvl, total = 0;

	if (levels < 1 || upW < 1 || upH < 1) {
		return 0;
	}
	for (lvl = 0; lvl < levels; lvl++) {
		total += ((w + 3) / 4) * ((h + 3) / 4) * SMP_TEXC_BLOCK_BYTES;
		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
	}
	return total;
}

int SMP_TranscodeChain(const unsigned char *data, int srcDxt, int width, int height,
						int iMipmapsAvailable, int base,
						int upW, int upH, int levels, int target,
						unsigned char *out,
						void *(*alloc)(int), void (*dealloc)(void *),
						smpTranscodeRunner_t runRows, void *runUd)
{
	int				blockBytes = (srcDxt == SMP_DXT1) ? 8 : 16;
	int				storedLevels, needRGBA, srcOffset = 0;
	int				w, h, pw, ph, lvl, off = 0, i, tw, th;
	unsigned char	*rgba = NULL;
	smpTranscodeRows_t ctx;

	if (!data || !out || levels < 1 || upW < 1 || upH < 1) {
		return 0;
	}

	storedLevels = iMipmapsAvailable - base;
	if (storedLevels < 0) storedLevels = 0;
	if (storedLevels > levels) storedLevels = levels;
	needRGBA = (levels > storedLevels) ? 1 : 0;

	tw = width;
	th = height;
	for (i = 0; i < base; i++) {
		srcOffset += ((tw + 3) / 4) * ((th + 3) / 4) * blockBytes;
		if (tw > 1) tw >>= 1;
		if (th > 1) th >>= 1;
	}

	if (needRGBA) {
		if (!alloc) {
			return 0;
		}
		rgba = (unsigned char *)alloc((int)((size_t)upW * upH * 4));
		if (!rgba) {
			return 0;
		}
	}

	ctx.dxt = srcDxt;
	ctx.target = target;

	w = upW;
	h = upH;
	pw = upW;
	ph = upH;
	for (lvl = 0; lvl < levels; lvl++) {
		int bw = (w + 3) / 4;
		int bh = (h + 3) / 4;
		int size = bw * bh * SMP_TEXC_BLOCK_BYTES;

		ctx.dst = out + off;
		if (lvl < storedLevels) {
			ctx.src = data + srcOffset;
			ctx.bw = bw;
			if (runRows) {
				runRows(runUd, &ctx, bh, bw);
			} else {
				SMP_TranscodeRows(&ctx, 0, bh);
			}

			if (needRGBA && lvl == storedLevels - 1) {
				SMP_DecodeLevelToRGBA(data + srcOffset, srcDxt, w, h, rgba);
			}
			srcOffset += bw * bh * blockBytes;
		} else {
			SMP_MipMap(rgba, pw, ph);
			SMP_EncodeLevelToBlocks(rgba, w, h, target, out + off);
		}
		pw = w;
		ph = h;
		off += size;

		if (w > 1) w >>= 1;
		if (h > 1) h >>= 1;
	}

	if (rgba && dealloc) {
		dealloc(rgba);
	}
	return off;
}
