/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "../gsCommon.h"

time_t time(time_t *timer)
{
	time_t t;

	assert(OS_IsTickAvailable() == TRUE);
	t = (time_t)OS_TicksToSeconds(OS_GetTick());

	if(timer)
		*timer = t;

	return t;
}