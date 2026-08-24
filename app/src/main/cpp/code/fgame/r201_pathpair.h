#ifndef R201_PATHPAIR_H
#define R201_PATHPAIR_H

#include <math.h>

#define R201_PATHPAIR_MAXDIST 256.0f

#define R201_PAIRMEMO_BITS 17
#define R201_PAIRMEMO_SIZE ( 1 << R201_PAIRMEMO_BITS )
#define R201_PAIRMEMO_PROBE 8
/* Node indices are < 4096 on every shipped map (m1l1b has 370); the encoding
   is injective on [0,4095]^2 and never yields the 0 empty marker. */
#define R201_PAIRMEMO_MAXNODE 4095

static inline unsigned R201_PathPairKey( unsigned a, unsigned b ) {
	return ( ( ( a + 1u ) << 12 ) ^ ( b + 1u ) );
}

static inline unsigned R201_PathPairSlot( unsigned key ) {
	return ( key * 2654435761u ) >> ( 32 - R201_PAIRMEMO_BITS );
}

static inline int R201_PathPairTooFar( const float *aOrigin, const float *bOrigin ) {
	float	dx, dy, dz, d2;

	dz = bOrigin[2] - aOrigin[2];
	if ( ( dz < -R201_PATHPAIR_MAXDIST ) || ( dz > R201_PATHPAIR_MAXDIST ) ) {
		return 1;
	}

	dx = bOrigin[0] - aOrigin[0];
	dy = bOrigin[1] - aOrigin[1];
	d2 = ( dx * dx ) + ( dy * dy );		

	return ( (float)sqrt( (double)d2 ) >= R201_PATHPAIR_MAXDIST ) ? 1 : 0;
}

static inline int R201_PathPairMemoHit( const unsigned *keys, unsigned key ) {
	unsigned	i, n;

	i = R201_PathPairSlot( key );
	for ( n = 0; n < R201_PAIRMEMO_PROBE; n++ ) {
		if ( keys[i] == key ) {
			return 1;
		}
		if ( keys[i] == 0u ) {
			return 0;
		}
		i = ( i + 1u ) & ( unsigned )( R201_PAIRMEMO_SIZE - 1 );
	}

	return 0;
}

static inline void R201_PathPairMemoAdd( unsigned *keys, unsigned key ) {
	unsigned	i, n;

	i = R201_PathPairSlot( key );
	for ( n = 0; n < R201_PAIRMEMO_PROBE; n++ ) {
		if ( ( keys[i] == 0u ) || ( keys[i] == key ) ) {
			keys[i] = key;
			return;
		}
		i = ( i + 1u ) & ( unsigned )( R201_PAIRMEMO_SIZE - 1 );
	}

	keys[ R201_PathPairSlot( key ) ] = key;
}

#endif 
