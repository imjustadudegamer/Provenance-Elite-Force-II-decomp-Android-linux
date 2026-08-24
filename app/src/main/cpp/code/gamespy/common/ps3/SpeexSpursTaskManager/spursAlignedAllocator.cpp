// NOTE:  this code has been provided by Sony for usage in Speex SPURS Manager


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


