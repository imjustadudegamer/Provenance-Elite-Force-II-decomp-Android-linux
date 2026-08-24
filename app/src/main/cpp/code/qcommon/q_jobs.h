#ifndef __QJOBS_H__
#define __QJOBS_H__

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void JobPool_Init( const char *name, int requestedWorkers );

#define JOBPOOL_MAX_TOTAL			7
#define JOBPOOL_CRITICAL_ANDROID	3
#define JOBPOOL_CRITICAL_OTHER		4

void JobPool_SplitWorkers( int coreCount, int isAndroid, int requested, int critical,
						   int *outWorkers );

void JobPool_InitCritical( const char *name, int requestedWorkers );

void JobPool_Shutdown( void );

qboolean JobPool_Enabled( void );

int JobPool_NumWorkers( void );

const char *JobPool_Name( void );

void JobPool_BeginBatch( void );
void JobPool_Add( void ( *fn )( void *ctx, int begin, int end ), void *ctx, int begin, int end );
void JobPool_RunAndWait( void );

void JobPool_ParallelRange( int count, int minChunk,
							void ( *fn )( void *ctx, int begin, int end ), void *ctx );

#if defined( __GNUC__ ) || defined( __clang__ )
typedef void ( *JobPoolPrint )( const char *fmt, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
#else
typedef void ( *JobPoolPrint )( const char *fmt, ... );
#endif
qboolean JobPool_SelfTest( JobPoolPrint print );

typedef struct jobpoolStats_s {
	int			workers;			
	int			batches;			
	int			inlineCalls;		
	long long	parallelElements;	
	long long	inlineElements;		
} jobpoolStats_t;

void JobPool_GetStats( jobpoolStats_t *out );
void JobPool_ResetStats( void );

#define JOBPOOL_METER_BUCKETS 200	

typedef struct jobpoolMeter_s {
	int		frames;
	int		frontMsec;
	int		backMsec;
	int		handoffMsec;
	int		windowStart;
	int		prevFrameMs;
	int		maxInterval;
	int		histTotal;
	int		hist[JOBPOOL_METER_BUCKETS];
} jobpoolMeter_t;

void JobPool_MeterFrame( jobpoolMeter_t *m, JobPoolPrint print, int nowMs,
						 int frontMsec, int backMsec, int handoffMsec, int smpActive );

#ifdef __cplusplus
}
#endif

#endif 
