/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */


#include "spursAlignedAllocator.h"

#include <stdlib.h>

int numAllocs = 0;
int numFree = 0;

void*	spursAlignedAlloc	(int size, int alignment)
{
	numAllocs++;
	return memalign(alignment, size);
}

void	spursAlignedFree	(void* ptr)
{
	numFree++;
	free(ptr);
}


