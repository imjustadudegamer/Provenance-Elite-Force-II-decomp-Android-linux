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
#include "tr_texcache.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

static void rc_put32( unsigned char *p, unsigned int v )
{
	p[0] = (unsigned char)(v & 0xFF);
	p[1] = (unsigned char)((v >> 8) & 0xFF);
	p[2] = (unsigned char)((v >> 16) & 0xFF);
	p[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned int rc_get32( const unsigned char *p )
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
		   ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

#define RC_FNV_OFFSET	1469598103934665603ULL	 
#define RC_FNV_PRIME	1099511628211ULL		 

static unsigned long long rc_fnv( unsigned long long h, const void *data, int bytes )
{
	const unsigned char *p = (const unsigned char *)data;
	int i;

	for ( i = 0; i < bytes; i++ ) {
		h ^= (unsigned long long)p[i];
		h *= RC_FNV_PRIME;
	}
	return h;
}

void SMP_TexCache_Hash64( const void *data, int bytes, unsigned int *lo, unsigned int *hi )
{
	unsigned long long h;

	if ( bytes < 0 || !data ) {
		bytes = 0;
	}
	h = rc_fnv( RC_FNV_OFFSET, data, bytes );
	*lo = (unsigned int)(h & 0xFFFFFFFFULL);
	*hi = (unsigned int)(h >> 32);
}


int SMP_TexCache_StoredBytes( int width, int height, int iMipmapsAvailable, int srcDxt )
{
	int blockBytes = ( srcDxt == 1  ) ? 8 : 16;
	int baseSize   = ( ( width + 3 ) / 4 ) * ( ( height + 3 ) / 4 ) * blockBytes;
	int lw = width, lh = height, lvl, total = 0;

	if ( iMipmapsAvailable < 1 ) {
		iMipmapsAvailable = 1;
	}
	for ( lvl = 0; lvl < iMipmapsAvailable; lvl++ ) {
		total += ( ( lw + 3 ) / 4 ) * ( ( lh + 3 ) / 4 ) * blockBytes;
		if ( lw > 1 ) lw >>= 1;
		if ( lh > 1 ) lh >>= 1;
	}
	return ( total < baseSize ) ? baseSize : total;
}

void SMP_TexCache_SourceHash( const unsigned char *dxt, int width, int height,
							   int iMipmapsAvailable, int srcDxt,
							   unsigned int *lo, unsigned int *hi, int *storedBytes )
{
	int bytes = SMP_TexCache_StoredBytes( width, height, iMipmapsAvailable, srcDxt );

	if ( storedBytes ) {
		*storedBytes = bytes;
	}
	SMP_TexCache_Hash64( dxt, bytes, lo, hi );
}

static int rc_level_bytes( int upW, int upH, int levels )
{
	int w = upW, h = upH, lvl, total = 0;

	if ( levels < 1 || upW < 1 || upH < 1 ) {
		return 0;
	}
	for ( lvl = 0; lvl < levels; lvl++ ) {
		total += ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 ) * 16;
		if ( w > 1 ) w >>= 1;
		if ( h > 1 ) h >>= 1;
	}
	return total;
}

static void rc_key_hash( const smpTexCacheKey_t *k, unsigned int *lo, unsigned int *hi )
{
	unsigned long long h = RC_FNV_OFFSET;
	unsigned char b[4];
	int nameLen = (int)strlen( k->name );

#define RC_H32(v) do { rc_put32( b, (unsigned int)(v) ); h = rc_fnv( h, b, 4 ); } while ( 0 )

	RC_H32( SMP_TEXCACHE_MAGIC );
	RC_H32( k->version );
	RC_H32( k->target );
	RC_H32( k->srcDxt );
	RC_H32( k->width );
	RC_H32( k->height );
	RC_H32( k->numMipmaps );
	RC_H32( k->picmip );
	RC_H32( k->maxTextureSize );
	RC_H32( k->base );
	RC_H32( k->upW );
	RC_H32( k->upH );
	RC_H32( k->levels );
	RC_H32( k->srcBytes );
	RC_H32( k->srcHashLo );
	RC_H32( k->srcHashHi );
	RC_H32( nameLen );
#undef RC_H32

	h = rc_fnv( h, k->name, nameLen );
	*lo = (unsigned int)(h & 0xFFFFFFFFULL);
	*hi = (unsigned int)(h >> 32);
}

void SMP_TexCache_FillKey( smpTexCacheKey_t *key, const char *name,
							int width, int height, int numMipmaps, int picmip,
							int maxTextureSize, int srcDxt, int target,
							int base, int upW, int upH, int levels,
							unsigned int srcHashLo, unsigned int srcHashHi,
							int srcBytes )
{
	int i, n;

	memset( key, 0, sizeof( *key ) );
	key->version        = SMP_TEXCACHE_ENC_VERSION;
	key->target         = (unsigned int)target;
	key->srcDxt         = (unsigned int)srcDxt;
	key->width          = width;
	key->height         = height;
	key->numMipmaps     = numMipmaps;
	key->picmip         = picmip;
	key->maxTextureSize = maxTextureSize;
	key->base           = base;
	key->upW            = upW;
	key->upH            = upH;
	key->levels         = levels;
	key->srcBytes       = (unsigned int)srcBytes;
	key->srcHashLo      = srcHashLo;
	key->srcHashHi      = srcHashHi;

	n = name ? (int)strlen( name ) : 0;
	if ( n > SMP_TEXCACHE_NAME_MAX - 1 ) {
		n = SMP_TEXCACHE_NAME_MAX - 1;
	}
	for ( i = 0; i < n; i++ ) {
		key->name[i] = name[i];
	}
}

void SMP_TexCache_Path( const smpTexCacheKey_t *key, char *out, int outSize )
{
	unsigned int lo, hi;

	rc_key_hash( key, &lo, &hi );
	snprintf( out, (size_t)outSize, "%s/%08x%08x_%u_%u.%s",
			  SMP_TEXCACHE_DIR, hi, lo, key->target, key->version, SMP_TEXCACHE_EXT );
}

static int rc_serialize_header( const smpTexCacheKey_t *key,
								unsigned int payloadLo, unsigned int payloadHi,
								unsigned int seqLo, unsigned int seqHi,
								unsigned char *out, int outSize )
{
	unsigned int klo, khi, hlo, hhi;
	int nameLen = (int)strlen( key->name );
	int pad, nameEnd, hdrLen, payloadBytes;

	if ( nameLen > SMP_TEXCACHE_NAME_MAX - 1 ) {
		nameLen = SMP_TEXCACHE_NAME_MAX - 1;
	}
	pad = ( 4 - ( nameLen & 3 ) ) & 3;
	nameEnd = SMP_TEXCACHE_HDR_FIXED + nameLen + pad;
	hdrLen = nameEnd + 8;
	payloadBytes = rc_level_bytes( key->upW, key->upH, key->levels );
	if ( outSize < hdrLen || payloadBytes <= 0 ) {
		return 0;
	}

	rc_key_hash( key, &klo, &khi );
	memset( out, 0, (size_t)hdrLen );

	rc_put32( out + 0,  SMP_TEXCACHE_MAGIC );
	rc_put32( out + 4,  key->version );
	rc_put32( out + 8,  (unsigned int)hdrLen );
	rc_put32( out + 12, key->target );
	rc_put32( out + 16, key->srcDxt );
	rc_put32( out + 20, (unsigned int)key->width );
	rc_put32( out + 24, (unsigned int)key->height );
	rc_put32( out + 28, (unsigned int)key->numMipmaps );
	rc_put32( out + 32, (unsigned int)key->picmip );
	rc_put32( out + 36, (unsigned int)key->maxTextureSize );
	rc_put32( out + 40, (unsigned int)key->base );
	rc_put32( out + 44, (unsigned int)key->upW );
	rc_put32( out + 48, (unsigned int)key->upH );
	rc_put32( out + 52, (unsigned int)key->levels );
	rc_put32( out + 56, key->srcBytes );
	rc_put32( out + 60, key->srcHashLo );
	rc_put32( out + 64, key->srcHashHi );
	rc_put32( out + 68, (unsigned int)payloadBytes );
	rc_put32( out + 72, payloadLo );
	rc_put32( out + 76, payloadHi );
	rc_put32( out + 80, (unsigned int)nameLen );
	rc_put32( out + 84, klo );
	rc_put32( out + 88, khi );
	rc_put32( out + 92, seqLo );
	rc_put32( out + 96, seqHi );

	memcpy( out + SMP_TEXCACHE_HDR_FIXED, key->name, (size_t)nameLen );

	SMP_TexCache_Hash64( out, nameEnd, &hlo, &hhi );
	rc_put32( out + nameEnd, hlo );
	rc_put32( out + nameEnd + 4, hhi );
	return hdrLen;
}

typedef struct {
	unsigned int	seqLo, seqHi;
	int				bytes;		
	char			name[80];	
} rc_entry_t;

static const smpTexCacheIO_t	*s_io;
static int						 s_enabled;
static int						 s_maxBytes;
static long long				 s_totalBytes;
static rc_entry_t				 s_entries[SMP_TEXCACHE_MAX_ENTRIES];
static int						 s_entryCount;
static unsigned int				 s_seqLo, s_seqHi;

static int rc_seq_newer( unsigned int aLo, unsigned int aHi, unsigned int bLo, unsigned int bHi )
{
	if ( aHi != bHi ) {
		return aHi > bHi;
	}
	return aLo > bLo;
}

static int rc_evict_for( int need, smpTexCacheResult_t *res )
{
	while ( s_totalBytes + need > s_maxBytes && s_entryCount > 0 ) {
		int oldest = 0, i;
		char path[SMP_TEXCACHE_PATH_MAX];

		for ( i = 1; i < s_entryCount; i++ ) {
			if ( rc_seq_newer( s_entries[oldest].seqLo, s_entries[oldest].seqHi,
							   s_entries[i].seqLo, s_entries[i].seqHi ) ) {
				oldest = i;
			}
		}
		snprintf( path, sizeof( path ), "%s/%s", SMP_TEXCACHE_DIR, s_entries[oldest].name );
		if ( s_io && s_io->removeFile ) {
			s_io->removeFile( s_io->ud, path );
		}
		s_totalBytes -= s_entries[oldest].bytes;
		if ( res ) {
			res->evicted++;
		}
		s_entries[oldest] = s_entries[s_entryCount - 1];
		s_entryCount--;
	}
	return ( s_totalBytes + need <= s_maxBytes ) ? 1 : 0;
}

static void rc_ledger_forget( const char *basename )
{
	int i;

	for ( i = 0; i < s_entryCount; i++ ) {
		if ( strcmp( s_entries[i].name, basename ) == 0 ) {
			s_totalBytes -= s_entries[i].bytes;
			s_entries[i] = s_entries[s_entryCount - 1];
			s_entryCount--;
			return;
		}
	}
}

static void rc_ledger_add( const char *basename, unsigned int seqLo, unsigned int seqHi, int bytes )
{
	if ( s_entryCount >= SMP_TEXCACHE_MAX_ENTRIES ) {
		return;
	}
	rc_ledger_forget( basename );
	s_entries[s_entryCount].seqLo = seqLo;
	s_entries[s_entryCount].seqHi = seqHi;
	s_entries[s_entryCount].bytes = bytes;
	snprintf( s_entries[s_entryCount].name, sizeof( s_entries[s_entryCount].name ),
			  "%s", basename );
	s_entryCount++;
	s_totalBytes += bytes;
}

void SMP_TexCache_Begin( const smpTexCacheIO_t *io, int enabled, int maxBytes )
{
	char **names;
	int count = 0, i;
	time_t now;

	s_io = io;
	s_enabled = ( enabled && io && io->openRead && io->read && io->writeAll ) ? 1 : 0;
	s_maxBytes = ( maxBytes > 0 ) ? maxBytes : 0;
	s_entryCount = 0;
	s_totalBytes = 0;

	now = time( NULL );
	s_seqHi = (unsigned int)( (unsigned long long)now & 0xFFFFFFFFULL );
	s_seqLo = 0;

	if ( !s_enabled ) {
		return;
	}

	names = s_io->list ? s_io->list( s_io->ud, &count ) : NULL;
	for ( i = 0; names && i < count; i++ ) {
		const char *bn = names[i];
		char path[SMP_TEXCACHE_PATH_MAX];
		unsigned char hdr[SMP_TEXCACHE_HDR_FIXED];
		smpTexCacheFile_t fh;
		long len;
		int got;

		snprintf( path, sizeof( path ), "%s/%s", SMP_TEXCACHE_DIR, bn );
		len = s_io->openRead( s_io->ud, path, &fh );
		if ( len < 0 ) {
			continue;
		}
		got = s_io->read( s_io->ud, fh, hdr, SMP_TEXCACHE_HDR_FIXED );
		s_io->closeRead( s_io->ud, fh );

		if ( len < SMP_TEXCACHE_HDR_FIXED + 8 || got != SMP_TEXCACHE_HDR_FIXED ||
			 rc_get32( hdr + 0 ) != SMP_TEXCACHE_MAGIC ||
			 rc_get32( hdr + 4 ) != SMP_TEXCACHE_ENC_VERSION ) {
			if ( s_io->removeFile ) {
				s_io->removeFile( s_io->ud, path );
			}
			continue;
		}
		rc_ledger_add( bn, rc_get32( hdr + 92 ), rc_get32( hdr + 96 ), (int)len );
	}
	if ( names && s_io->freeList ) {
		s_io->freeList( s_io->ud, names );
	}
	rc_evict_for( 0, NULL );
}

void SMP_TexCache_Shutdown( void )
{
	s_enabled = 0;
	s_io = NULL;
	s_entryCount = 0;
	s_totalBytes = 0;
}

int SMP_TexCache_Enabled( void )
{
	return s_enabled;
}

static void rc_delete_ours( const char *path )
{
	if ( s_io && s_io->removeFile && path && path[0] ) {
		const char *base = strrchr( path, '/' );

		s_io->removeFile( s_io->ud, path );
		if ( base ) {
			rc_ledger_forget( base + 1 );
		}
	}
}

int SMP_TexCache_Load( const smpTexCacheKey_t *key,
						unsigned char **blocks, int *bytes,
						void *(*alloc)( int ), void (*dealloc)( void * ),
						smpTexCacheResult_t *res )
{
	unsigned char hdr[SMP_TEXCACHE_HDR_FIXED];
	unsigned char nameBuf[SMP_TEXCACHE_NAME_MAX];
	unsigned char tail[8];
	smpTexCacheFile_t fh;
	char path[SMP_TEXCACHE_PATH_MAX];
	long len;
	int got, nameLen, pad, nameEnd, hdrLen, payloadBytes, i;

	if ( blocks ) *blocks = NULL;
	if ( bytes ) *bytes = 0;
	if ( res ) {
		memset( res, 0, sizeof( *res ) );
	}
	if ( !s_enabled || !key ) {
		if ( res ) res->stat = SMP_TEXCACHE_STAT_DISABLED;
		return 0;
	}
	SMP_TexCache_Path( key, path, (int)sizeof( path ) );
	if ( res ) {
		snprintf( res->path, sizeof( res->path ), "%s", path );
	}

	len = s_io->openRead( s_io->ud, path, &fh );
	if ( len < 0 ) {
		if ( res ) res->stat = SMP_TEXCACHE_STAT_MISS;
		return 0;
	}
	if ( len < SMP_TEXCACHE_HDR_FIXED + 8 ) {
		s_io->closeRead( s_io->ud, fh );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
		return 0;
	}
	got = s_io->read( s_io->ud, fh, hdr, SMP_TEXCACHE_HDR_FIXED );
	if ( got != SMP_TEXCACHE_HDR_FIXED ||
		 rc_get32( hdr + 0 ) != SMP_TEXCACHE_MAGIC ||
		 rc_get32( hdr + 4 ) != SMP_TEXCACHE_ENC_VERSION ) {
		s_io->closeRead( s_io->ud, fh );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
		return 0;
	}

	nameLen = (int)rc_get32( hdr + 80 );
	if ( nameLen < 0 || nameLen >= SMP_TEXCACHE_NAME_MAX ) {
		s_io->closeRead( s_io->ud, fh );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
		return 0;
	}
	pad = ( 4 - ( nameLen & 3 ) ) & 3;
	nameEnd = SMP_TEXCACHE_HDR_FIXED + nameLen + pad;
	hdrLen = nameEnd + 8;

	memset( nameBuf, 0, sizeof( nameBuf ) );
	got = s_io->read( s_io->ud, fh, nameBuf, nameLen + pad );
	if ( got != nameLen + pad ) {
		s_io->closeRead( s_io->ud, fh );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
		return 0;
	}
	got = s_io->read( s_io->ud, fh, tail, 8 );
	if ( got != 8 ) {
		s_io->closeRead( s_io->ud, fh );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
		return 0;
	}

	{
		unsigned int hlo, hhi;
		unsigned long long h = rc_fnv( RC_FNV_OFFSET, hdr, SMP_TEXCACHE_HDR_FIXED );

		h = rc_fnv( h, nameBuf, nameLen + pad );
		hlo = (unsigned int)(h & 0xFFFFFFFFULL);
		hhi = (unsigned int)(h >> 32);
		if ( rc_get32( tail ) != hlo || rc_get32( tail + 4 ) != hhi ) {
			s_io->closeRead( s_io->ud, fh );
			rc_delete_ours( path );
			if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
			return 0;
		}
	}

	payloadBytes = (int)rc_get32( hdr + 68 );
	{
		unsigned int klo, khi;
		char name[SMP_TEXCACHE_NAME_MAX];

		rc_key_hash( key, &klo, &khi );
		memcpy( name, nameBuf, (size_t)SMP_TEXCACHE_NAME_MAX );

		if ( rc_get32( hdr + 12 ) != key->target ||
			 rc_get32( hdr + 16 ) != key->srcDxt ||
			 (int)rc_get32( hdr + 20 ) != key->width ||
			 (int)rc_get32( hdr + 24 ) != key->height ||
			 (int)rc_get32( hdr + 28 ) != key->numMipmaps ||
			 (int)rc_get32( hdr + 32 ) != key->picmip ||
			 (int)rc_get32( hdr + 36 ) != key->maxTextureSize ||
			 (int)rc_get32( hdr + 40 ) != key->base ||
			 (int)rc_get32( hdr + 44 ) != key->upW ||
			 (int)rc_get32( hdr + 48 ) != key->upH ||
			 (int)rc_get32( hdr + 52 ) != key->levels ||
			 rc_get32( hdr + 56 ) != key->srcBytes ||
			 rc_get32( hdr + 60 ) != key->srcHashLo ||
			 rc_get32( hdr + 64 ) != key->srcHashHi ||
			 rc_get32( hdr + 84 ) != klo ||
			 rc_get32( hdr + 88 ) != khi ||
			 memcmp( name, key->name, SMP_TEXCACHE_NAME_MAX ) != 0 ) {
			s_io->closeRead( s_io->ud, fh );
			if ( res ) res->stat = SMP_TEXCACHE_STAT_MISS;
			return 0;
		}
	}

	if ( (int)rc_get32( hdr + 8 ) != hdrLen ||
		 payloadBytes != rc_level_bytes( key->upW, key->upH, key->levels ) ||
		 payloadBytes <= 0 ||
		 len != (long)hdrLen + (long)payloadBytes ) {
		s_io->closeRead( s_io->ud, fh );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
		return 0;
	}

	if ( !alloc ) {
		s_io->closeRead( s_io->ud, fh );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_MISS;
		return 0;
	}
	{
		unsigned char *buf = (unsigned char *)alloc( payloadBytes );
		unsigned int plo, phi;

		if ( !buf ) {
			s_io->closeRead( s_io->ud, fh );
			if ( res ) res->stat = SMP_TEXCACHE_STAT_MISS;
			return 0;
		}
		for ( i = 0; i < payloadBytes; ) {
			int chunk = payloadBytes - i;

			if ( chunk > 65536 ) chunk = 65536;
			if ( s_io->read( s_io->ud, fh, buf + i, chunk ) != chunk ) {
				s_io->closeRead( s_io->ud, fh );
				if ( dealloc ) dealloc( buf );
				rc_delete_ours( path );
				if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
				return 0;
			}
			i += chunk;
		}
		s_io->closeRead( s_io->ud, fh );

		SMP_TexCache_Hash64( buf, payloadBytes, &plo, &phi );
		if ( plo != rc_get32( hdr + 72 ) || phi != rc_get32( hdr + 76 ) ) {
			if ( dealloc ) dealloc( buf );
			rc_delete_ours( path );
			if ( res ) res->stat = SMP_TEXCACHE_STAT_CORRUPT;
			return 0;
		}

		if ( blocks ) *blocks = buf;
		else if ( dealloc ) dealloc( buf );
		if ( bytes ) *bytes = payloadBytes;
		if ( res ) res->stat = SMP_TEXCACHE_STAT_HIT;
		return 1;
	}
}

int SMP_TexCache_Store( const smpTexCacheKey_t *key,
						 const unsigned char *blocks, int bytes,
						 smpTexCacheResult_t *res )
{
	unsigned char *hdr = NULL, *whole = NULL;
	unsigned int plo, phi;
	char path[SMP_TEXCACHE_PATH_MAX];
	int hdrLen, total, maxHdr;

	if ( res ) {
		memset( res, 0, sizeof( *res ) );
	}
	if ( !s_enabled || !key || !blocks || bytes <= 0 ) {
		if ( res ) res->stat = SMP_TEXCACHE_STAT_DISABLED;
		return 0;
	}
	SMP_TexCache_Path( key, path, (int)sizeof( path ) );
	if ( res ) {
		snprintf( res->path, sizeof( res->path ), "%s", path );
	}
	if ( bytes != rc_level_bytes( key->upW, key->upH, key->levels ) ) {
		if ( res ) res->stat = SMP_TEXCACHE_STAT_SKIP;
		return 0;
	}
	if ( bytes > s_maxBytes ) {
		if ( res ) res->stat = SMP_TEXCACHE_STAT_SKIP;
		return 0;
	}
	SMP_TexCache_Hash64( blocks, bytes, &plo, &phi );

	maxHdr = SMP_TEXCACHE_HDR_FIXED + SMP_TEXCACHE_NAME_MAX + 8;
	hdr = (unsigned char *)malloc( (size_t)maxHdr );
	if ( !hdr ) {
		if ( res ) res->stat = SMP_TEXCACHE_STAT_WRITE_FAIL;
		return 0;
	}

	s_seqLo++;
	if ( s_seqLo == 0 ) {
		s_seqHi++;
	}
	hdrLen = rc_serialize_header( key, plo, phi, s_seqLo, s_seqHi, hdr, maxHdr );
	if ( hdrLen <= 0 ) {
		free( hdr );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_WRITE_FAIL;
		return 0;
	}

	if ( !rc_evict_for( hdrLen + bytes, res ) ) {
		free( hdr );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_SKIP;
		return 0;
	}

	total = hdrLen + bytes;
	whole = (unsigned char *)malloc( (size_t)total );
	if ( !whole ) {
		free( hdr );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_WRITE_FAIL;
		return 0;
	}
	memcpy( whole, hdr, (size_t)hdrLen );
	memcpy( whole + hdrLen, blocks, (size_t)bytes );
	free( hdr );

	if ( !s_io->writeAll( s_io->ud, path, whole, total ) ) {
		free( whole );
		rc_delete_ours( path );
		if ( res ) res->stat = SMP_TEXCACHE_STAT_WRITE_FAIL;
		return 0;
	}
	free( whole );

	{
		const char *base = strrchr( path, '/' );

		rc_ledger_add( base ? base + 1 : path, s_seqLo, s_seqHi, total );
	}
	if ( res ) res->stat = SMP_TEXCACHE_STAT_STORED;
	return 1;
}

#ifndef SMP_TEXCACHE_STANDALONE
#include "tr_local.h"

static long rc_ri_open( void *ud, const char *qpath, smpTexCacheFile_t *fh )
{
	fileHandle_t h = 0;
	long len;

	(void)ud;
	len = ri.FS_OpenFile( (char *)qpath, &h, qfalse, qtrue );
	if ( len < 0 || h == 0 ) {
		return -1;
	}
	*fh = (smpTexCacheFile_t)(intptr_t)h;
	return len;
}

static int rc_ri_read( void *ud, smpTexCacheFile_t fh, void *buf, int len )
{
	(void)ud;
	return (int)ri.FS_Read( buf, (size_t)len, (fileHandle_t)(intptr_t)fh );
}

static void rc_ri_close( void *ud, smpTexCacheFile_t fh )
{
	(void)ud;
	ri.FS_CloseFile( (fileHandle_t)(intptr_t)fh );
}

static int rc_ri_write( void *ud, const char *qpath, const void *buf, int len )
{
	fileHandle_t h;

	(void)ud;
	h = ri.FS_OpenFileWrite( qpath );
	if ( h == 0 ) {
		return 0;
	}
	if ( (int)ri.FS_Write( buf, (size_t)len, h ) != len ) {
		ri.FS_CloseFile( h );
		return 0;
	}
	ri.FS_CloseFile( h );
	return 1;
}

static int rc_ri_remove( void *ud, const char *qpath )
{
	(void)ud;
	ri.FS_RemoveHomeData( qpath );
	return ri.FS_FileExists( qpath ) ? 0 : 1;
}

static char **rc_ri_list( void *ud, int *count )
{
	(void)ud;
	return ri.FS_ListFiles( SMP_TEXCACHE_DIR, SMP_TEXCACHE_EXT, count );
}

static void rc_ri_freelist( void *ud, char **names )
{
	(void)ud;
	ri.FS_FreeFileList( names );
}

static const smpTexCacheIO_t s_ri_io = {
	rc_ri_open, rc_ri_read, rc_ri_close,
	rc_ri_write, rc_ri_remove, rc_ri_list, rc_ri_freelist, NULL
};

void SMP_TexCache_InitEngine( int enabled, int maxBytes )
{
	SMP_TexCache_Begin( &s_ri_io, enabled, maxBytes );
}
#endif 
