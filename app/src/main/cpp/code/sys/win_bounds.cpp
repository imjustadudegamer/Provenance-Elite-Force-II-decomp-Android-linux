

extern "C"
{
	void* b_malloc(unsigned int size);
	void  b_free(void*);
};

#define DISABLE_BOUNDS

#if defined(GAME_DLL) || defined(CGAME_DLL)
#define DISABLE_BOUNDS
#endif



#ifdef DISABLE_BOUNDS

#include <stdlib.h>

void* b_malloc(unsigned int size)
{
	return malloc(size);
}

void b_free(void* what)
{
	free(what);
}

#else

#pragma message ("win_bounds.cpp: Warning - Bounds checking is enabled\n" )

#include <stdio.h>
#include <windows.h>

typedef struct {
	unsigned int header;
	int size;
	void* returned;
} bounds_type_t;

#define PAGE_SIZE ( 4096 )

#define NORMAL_HEADER 0xdeadbeef
#define ARRAY_HEADER 0xdeadbabe

static unsigned int bounds_numpages(unsigned int size) {
	unsigned int ret;

	ret = size / PAGE_SIZE + 3;
	if (size % PAGE_SIZE)
		ret++;

	return ret;
}

void* bounds_malloc(unsigned int size, unsigned head = NORMAL_HEADER) {
	bounds_type_t* where;
	unsigned int num_pages;
	void* mainaddress;

	num_pages = bounds_numpages(size);

	mainaddress = VirtualAlloc(NULL, num_pages * PAGE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
	VirtualAlloc(mainaddress, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
	VirtualAlloc((char*)mainaddress + PAGE_SIZE * 2, (num_pages - 3) * PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
	where = (bounds_type_t*)mainaddress;

	where->header = head;
	where->size = size;
#ifdef BOUNDS_ENDOFPAGE
	where->returned = (char*)mainaddress + (num_pages - 1) * PAGE_SIZE - size;
#else
	where->returned = (char*)mainaddress + PAGE_SIZE * 2;
#endif
	memset(where->returned, 0xdc, size);

	return where->returned;
}

void bounds_free(void* address, unsigned head = NORMAL_HEADER) {
	bounds_type_t* where;
	unsigned int num_pages;
	void* mainaddress;

	mainaddress = (char*)address - PAGE_SIZE * 2;
#ifdef BOUNDS_ENDOFPAGE
	mainaddress = (char*)mainaddress - ((unsigned int)mainaddress % 4096);
#endif

	where = (bounds_type_t*)mainaddress;

	if (where->header != head || where->returned != address) {
		__asm int 3 
	}
	num_pages = bounds_numpages(where->size);

	VirtualFree(mainaddress, PAGE_SIZE, MEM_DECOMMIT);
	VirtualFree((char*)mainaddress + PAGE_SIZE * 2, (num_pages - 3) * PAGE_SIZE, MEM_DECOMMIT);
	VirtualFree(mainaddress, 0, MEM_RELEASE);
}

void* b_malloc(unsigned int size)
{
	return bounds_malloc(size);
}

void b_free(void* what)
{
	bounds_free(what);
}

void* operator new[](size_t size)
{
	return bounds_malloc(size, ARRAY_HEADER);
}

void* operator new (size_t size)
{
	return bounds_malloc(size);
}

void operator delete [](void* what)
{
	if (what)
		bounds_free(what, ARRAY_HEADER);
}

void operator delete (void* what)
{
	if (what)
		bounds_free(what);
}

#endif 