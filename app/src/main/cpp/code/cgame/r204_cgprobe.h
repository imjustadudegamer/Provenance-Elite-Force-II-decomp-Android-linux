
#ifndef __R204_CGPROBE_H__
#define __R204_CGPROBE_H__

typedef enum {
	R204_CGZ_clear = 0, R204_CGZ_predict, R204_CGZ_calcview, R204_CGZ_ents,
	R204_CGZ_audio, R204_CGZ_fx, R204_CGZ_viewmode, R204_CGZ_draw,
	R204_CGZ_other, R204_CG_ZONES
} r204_cg_zone_t;

typedef enum {
	R204_CGC_snapents = 0,
	R204_CG_COUNTERS
} r204_cg_counter_t;

#ifdef __cplusplus
extern "C" {
#endif

extern int	r204_cgOn;

int		R204_CgNow( void );			
int		R204_CgRoot( void );		
void	R204_CgZone( int zone, int usec );
void	R204_CgCount( int counter, int n );
void	R204_CgFrameEnd( int frameStamp );	

int		R204_CgFrameOpen( void );
int		R204_SysMilliseconds( void );

#ifdef __cplusplus
}
#endif

#define R204_STAMP(t)			((t) + 1)
#define R204_UNSTAMP(s)			((s) - 1)

#define R204_CG_T0()			( r204_cgOn ? R204_STAMP( R204_CgNow() ) : 0 )
#define R204_CG_ZONE_END(z,t)	do { if ( r204_cgOn && (t) ) R204_CgZone( (z), R204_CgNow() - R204_UNSTAMP( (t) ) ); } while ( 0 )

#define R204_CG_COUNT(c,n)		do { if ( r204_cgOn ) R204_CgCount( (c), (n) ); } while ( 0 )

#endif 
