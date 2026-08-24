/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction
#ifndef SMP_PATHPAIR_H
#define SMP_PATHPAIR_H

#include <math.h>

#define SMP_PATHPAIR_MAXDIST 256.0f

#define SMP_PAIRMEMO_BITS 17
#define SMP_PAIRMEMO_SIZE ( 1 << SMP_PAIRMEMO_BITS )
#define SMP_PAIRMEMO_PROBE 8
#define SMP_PAIRMEMO_MAXNODE 4095

static inline unsigned SMP_PathPairKey( unsigned a, unsigned b ) {
	return ( ( ( a + 1u ) << 12 ) ^ ( b + 1u ) );
}

static inline unsigned SMP_PathPairSlot( unsigned key ) {
	return ( key * 2654435761u ) >> ( 32 - SMP_PAIRMEMO_BITS );
}

static inline int SMP_PathPairTooFar( const float *aOrigin, const float *bOrigin ) {
	float	dx, dy, dz, d2;

	dz = bOrigin[2] - aOrigin[2];
	if ( ( dz < -SMP_PATHPAIR_MAXDIST ) || ( dz > SMP_PATHPAIR_MAXDIST ) ) {
		return 1;
	}

	dx = bOrigin[0] - aOrigin[0];
	dy = bOrigin[1] - aOrigin[1];
	d2 = ( dx * dx ) + ( dy * dy );		

	return ( (float)sqrt( (double)d2 ) >= SMP_PATHPAIR_MAXDIST ) ? 1 : 0;
}

static inline int SMP_PathPairMemoHit( const unsigned *keys, unsigned key ) {
	unsigned	i, n;

	i = SMP_PathPairSlot( key );
	for ( n = 0; n < SMP_PAIRMEMO_PROBE; n++ ) {
		if ( keys[i] == key ) {
			return 1;
		}
		if ( keys[i] == 0u ) {
			return 0;
		}
		i = ( i + 1u ) & ( unsigned )( SMP_PAIRMEMO_SIZE - 1 );
	}

	return 0;
}

static inline void SMP_PathPairMemoAdd( unsigned *keys, unsigned key ) {
	unsigned	i, n;

	i = SMP_PathPairSlot( key );
	for ( n = 0; n < SMP_PAIRMEMO_PROBE; n++ ) {
		if ( ( keys[i] == 0u ) || ( keys[i] == key ) ) {
			keys[i] = key;
			return;
		}
		i = ( i + 1u ) & ( unsigned )( SMP_PAIRMEMO_SIZE - 1 );
	}

	keys[ SMP_PathPairSlot( key ) ] = key;
}

#endif 
