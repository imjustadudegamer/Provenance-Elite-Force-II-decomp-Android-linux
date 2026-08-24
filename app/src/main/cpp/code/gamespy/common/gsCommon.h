/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __GSCOMMON_H__
#define __GSCOMMON_H__




#ifdef GS_PEER
	#define	UNIQUEID			
#endif

#include "gsPlatform.h"
#include "gsPlatformSocket.h"
#include "gsPlatformThread.h"
#include "gsPlatformUtil.h"

#include "gsMemory.h"
#include "gsDebug.h"
#include "gsAssert.h"
#include "gsStringUtil.h"



#define	GSI_MIN(a,b)				(((a) < (b)?(a):(b)))
#define	GSI_MAX(a,b)				(((a) > (b)?(a):(b)))
#define	GSI_LIMIT(x, minx, maxx)	(((x) < (minx)? (minx) : ((x) > (maxx)? (maxx) : (x))))
#define	GSI_WRAP(x, minx, maxx)		(((x) < (minx)? (maxx-1 ) : ((x) >= (maxx)? (minx) : (x))))
#define GSI_DIM( x )				( sizeof( x ) / sizeof((x)[ 0 ]))

#endif 
