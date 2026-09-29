/*
 * jobsizing_oracle.c -- the pool-sizing rule's offline table proof.
 *
 * It includes the shipping `code/qcommon/q_jobs_workers.h` and prints, for
 * every core count a device or PC can report, the headroom and each module
 * pool's share, then asserts the properties the operator's expectation needs:
 *
 *   - all the headroom is handed out, on every machine;
 *   - the renderer's pool is never smaller than the client's, and is strictly
 *     larger once there are four workers to split;
 *   - no share exceeds the pool's declared array;
 *   - the headroom never exceeds cores - 2.
 *
 * Build/run: docs/controls/jobsizing/run.sh
 */

#include <stdio.h>

#include "../../../app/src/main/cpp/code/qcommon/q_jobs_workers.h"

int main( void )
{
	int	cores;
	int	failures = 0;
	int	checks = 0;

	printf( "cores headroom renderer client\n" );

	for ( cores = 1; cores <= 64; cores++ ) {
		int	total = cores - 2;
		int	renderer, client, head;

		if ( total < 0 ) {
			total = 0;
		}
		head = ( total > JOBPOOL_MAX_WORKERS ) ? JOBPOOL_MAX_WORKERS : total;

		JobPool_SplitWorkersRule( cores, 0, -1, 1, &renderer );
		JobPool_SplitWorkersRule( cores, 0, -1, 0, &client );

		if ( cores <= 20 || ( cores % 8 ) == 0 || cores == 64 ) {
			printf( "%5d %8d %8d %6d\n", cores, head, renderer, client );
		}

		checks++;
		if ( renderer + client != head ) {
			printf( "  FAIL  cores=%d: %d + %d != %d\n", cores, renderer, client, head );
			failures++;
		}
		checks++;
		if ( renderer < client ) {
			printf( "  FAIL  cores=%d: renderer %d < client %d\n", cores, renderer, client );
			failures++;
		}
		checks++;
		if ( head >= 4 && renderer <= client ) {
			printf( "  FAIL  cores=%d: renderer %d is not the larger share of %d\n",
					cores, renderer, head );
			failures++;
		}
		checks++;
		if ( renderer > JOBPOOL_MAX_WORKERS || client > JOBPOOL_MAX_WORKERS ) {
			printf( "  FAIL  cores=%d: share exceeds the array (%d, %d)\n", cores, renderer, client );
			failures++;
		}
		checks++;
		if ( head > cores - 2 && cores >= 2 ) {
			printf( "  FAIL  cores=%d: headroom %d exceeds cores-2\n", cores, head );
			failures++;
		}
	}

	/* the operator's own settings: r_jobs/cl_jobs 0 off, a positive cap binds */
	{
		int	w = -1;

		JobPool_SplitWorkersRule( 12, 0, 0, 1, &w );
		checks++; if ( w != 0 ) { printf( "  FAIL  requested 0 gave %d\n", w ); failures++; }

		JobPool_SplitWorkersRule( 12, 0, 2, 1, &w );
		checks++; if ( w != 2 ) { printf( "  FAIL  requested 2 gave %d\n", w ); failures++; }

		JobPool_SplitWorkersRule( 12, 0, 3, 1, &w );
		checks++; if ( w != 3 ) { printf( "  FAIL  requested 3 gave %d\n", w ); failures++; }
	}

	printf( "jobsizing oracle: %d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}
