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

#ifndef __BE_CGPROBE_H__
#define __BE_CGPROBE_H__

typedef enum {
	BE_CGZ_clear = 0, BE_CGZ_predict, BE_CGZ_calcview, BE_CGZ_ents,
	BE_CGZ_audio, BE_CGZ_fx, BE_CGZ_viewmode, BE_CGZ_draw,
	BE_CGZ_other, BE_CG_ZONES
} be_cg_zone_t;

typedef enum {
	BE_CGC_snapents = 0,
	BE_CG_COUNTERS
} be_cg_counter_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int	be_cgOn;

int		BE_CgNow( void );			
int		BE_CgRoot( void );		
void	BE_CgZone( int zone, int usec );
void	BE_CgCount( int counter, int n );
void	BE_CgFrameEnd( int frameStamp );	

int		BE_CgFrameOpen( void );
int		BE_SysMilliseconds( void );

#ifdef __cplusplus
}
#endif

#define BE_STAMP(t)			((t) + 1)
#define BE_UNSTAMP(s)			((s) - 1)

#define BE_CG_T0()			( be_cgOn ? BE_STAMP( BE_CgNow() ) : 0 )
#define BE_CG_ZONE_END(z,t)	do { if ( be_cgOn && (t) ) BE_CgZone( (z), BE_CgNow() - BE_UNSTAMP( (t) ) ); } while ( 0 )

#define BE_CG_COUNT(c,n)		do { if ( be_cgOn ) BE_CgCount( (c), (n) ); } while ( 0 )

#endif 
