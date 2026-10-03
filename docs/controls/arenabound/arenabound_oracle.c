/*
 * arenabound_oracle.c -- the boundary's two pools, driven through the SHIPPING
 * zone allocator (code/qcommon/memory.c, compiled from source here) under
 * AddressSanitizer.
 *
 * The client binds the renderer's two allocation pools in client/cl_main.cpp:
 *   ri.Hunk_Alloc  = CL_RefStaticMalloc -> Z_TagMalloc( size, TAG_STATIC_RENDERER )
 *   ri.Malloc      = CL_RefMalloc       -> Z_TagMalloc( size, TAG_RENDERER )
 *   ri.Hunk_Clear  = CL_RefStaticClear  -> Z_FreeTags( TAG_STATIC_RENDERER )
 * and RE_BeginRegistration calls ri.Hunk_Clear at the map-change boundary.
 *
 * mode 0  the shipping shape: the frame's command buffer comes from the pool
 *         (ri.Malloc / TAG_RENDERER).  The purge must free the arena block and
 *         leave the pool block live.
 * mode 1  planted fault: the command buffer comes from the arena (the shipped
 *         regression, tr_cmds.c:914 `ri.Hunk_Alloc(...)`).  The oracle must go
 *         red: the purge poisons the block the front end still records into.
 * mode 2  planted fault: the pool is purged too, so even the fixed allocation
 *         is gone.
 *
 * Never launches the game.
 */
#include <stdio.h>
#include <stdlib.h>

#include "qcommon.h"

extern void Z_InitMemory(void);

extern int __asan_address_is_poisoned(void const volatile *addr);

static int failures;
static int checks;

#define CHECK(cond, ...) do { \
	checks++; \
	if (!(cond)) { failures++; printf("  FAIL  "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

/* the seven externals code/qcommon/memory.c needs */
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int level, const char *fmt, ...) { (void)level; (void)fmt; exit(3); }
void Cmd_AddCommand(const char *name, void (*fn)(void)) { (void)name; (void)fn; }
int  Cmd_Argc(void) { return 1; }
int  Cvar_VariableIntegerValue(const char *name) { (void)name; return 0; }
int  Sys_Milliseconds(void) { return 0; }

/* the client's own bindings, quoted */
static void *CL_RefStaticMalloc(int size) { return Z_TagMalloc(size, TAG_STATIC_RENDERER); }
static void *CL_RefMalloc(int size)       { return Z_TagMalloc(size, TAG_RENDERER); }
static void  CL_RefStaticClear(void)      { Z_FreeTags(TAG_STATIC_RENDERER); }

int main(int argc, char **argv)
{
	int   mode = (argc > 1) ? atoi(argv[1]) : 0;
	void *list;
	void *arena;

	Z_InitMemory();

	/* the world object the boundary is meant to destroy */
	arena = CL_RefStaticMalloc(256);

	/* the frame's render-command buffer */
	list = (mode == 1) ? CL_RefStaticMalloc(4096) : CL_RefMalloc(4096);

	/* the boundary: ri.Hunk_Clear */
	CL_RefStaticClear();
	if (mode == 2) {
		Z_FreeTags(TAG_RENDERER);	/* planted: the pool is purged too */
	}

	CHECK(__asan_address_is_poisoned(arena),
		  "the arena block is not freed by the boundary purge");
	CHECK(!__asan_address_is_poisoned(list),
		  "the render-command buffer was freed by the boundary purge");

	/* the front end's next write, the operation the crash's 256 KB memcpy is */
	if (!__asan_address_is_poisoned(list)) {
		((volatile unsigned *)list)[0] = 0xfeedfaceu;
		CHECK(((volatile unsigned *)list)[0] == 0xfeedfaceu,
			  "the render-command buffer did not keep its write");
	}

	printf("  %s  mode %d: %d checks, %d failures\n",
		   failures ? "RED " : "ok  ", mode, checks, failures);

	return failures ? 1 : 0;
}
