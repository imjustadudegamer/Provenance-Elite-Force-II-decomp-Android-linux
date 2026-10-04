/*
 * cfgwiring_oracle.c -- the cvar resolution the launcher's override depends
 * on, driven through the SHIPPING cvar.c / cmd.c / memory.c.
 *
 * One mode, the shipping shape:
 *   a LATCHed cvar set with force=0 only latches, so a set landing after the
 *   renderer has registered is not a boot's value -- which is what rules out a
 *   late +exec override for every LATCHed setting;
 *   registration applies the pending latch with force, which is what makes a
 *   pre-registration override bind;
 *   a forced set binds a ROM cvar and a non-forced one is refused, the
 *   property the command line's re-application carries;
 *   `setx' binds and the shutdown archive skips the cvar, `seta' binds and the
 *   archive writes it.
 * The planted faults in run.sh mutate the shipping cvar.c and re-link, so a
 * lost property turns one of these checks red.
 *
 * Never launches the game.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon.h"


extern void Z_InitMemory(void);

static int failures;
static int checks;

#define CHECK(cond, ...) do { \
	checks++; \
	if (!(cond)) { failures++; printf("  FAIL  "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int level, const char *fmt, ...) { (void)level; (void)fmt; exit(3); }
int Sys_Milliseconds(void) { return 0; }

/* the cvar.c/cmd.c externals that live in common.c: the copy uses the shipping
   zone allocator, Com_Filter is only reached by cvarlist */
char *CopyString( const char *in ) { return strcpy( (char *)Z_Malloc( strlen( in ) + 1 ), in ); }
int Com_Filter( const char *filter, const char *name, int casesensitive )
{
	(void)filter; (void)name; (void)casesensitive; return qtrue;
}
cvar_t *developer;
qboolean com_errorEntered;
void Com_WPrintf( const char *fmt, ... ) { (void)fmt; }
void FS_Printf( fileHandle_t f, const char *fmt, ... ) { (void)f; (void)fmt; }
long FS_ReadFile( const char *qpath, void **buffer ) { (void)qpath; *buffer = NULL; return 0; }
void FS_FreeFile( void *buffer ) { (void)buffer; }
cvar_t *com_cl_running;
cvar_t *com_sv_running;
void CL_ForwardCommandToServer( const char *string ) { (void)string; }
qboolean CL_GameCommand( void ) { return qfalse; }
qboolean SV_GameCommand( void ) { return qfalse; }

/* the captured archive: Cvar_WriteVariables writes through FS_Write */
static char archive[16384];

size_t FS_Write(const void *buffer, size_t len, fileHandle_t f)
{
	(void)f;
	if (len >= sizeof(archive)) {
		len = sizeof(archive) - 1;
	}
	strncat(archive, (const char *)buffer, len);
	return len;
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	cvar_t *latched;
	cvar_t *rom;
	cvar_t *a;
	cvar_t *b;

	Z_InitMemory();
	Cmd_Init();
	Cvar_Init();

	/* 1. a LATCHed cvar set the way a config line sets it only latches */
	latched = Cvar_Get("r_ov_latch", "1", CVAR_ARCHIVE | CVAR_LATCH);
	CHECK(latched != NULL, "register r_ov_latch");
	Cvar_Set2("r_ov_latch", "0", qfalse);
	CHECK(latched->latchedString != NULL && !strcmp(latched->latchedString, "0"),
		  "the non-forced set on a LATCHed cvar only latches");
	CHECK(strcmp(latched->string, "0") != 0,
		  "the non-forced set on a LATCHed cvar must not bind (bound %s)", latched->string);
	/* registration applies the pending latch with force: the property that
	   makes an override landing before registration bind for this boot */
	(void)Cvar_Get("r_ov_latch", "1", CVAR_ARCHIVE | CVAR_LATCH);
	CHECK(strcmp(latched->string, "0") == 0,
		  "registration applies the pending latch (string=%s)", latched->string);
	CHECK(latched->latchedString == NULL, "the applied latch is consumed");

	/* 2. forced vs non-forced on a ROM cvar */
	rom = Cvar_Get("r_ov_rom", "1", CVAR_ROM);
	Cvar_Set2("r_ov_rom", "2", qfalse);
	CHECK(strcmp(rom->string, "1") == 0, "a non-forced set cannot write a ROM cvar");
	Cvar_Set2("r_ov_rom", "3", qtrue);
	CHECK(strcmp(rom->string, "3") == 0, "a forced set writes a ROM cvar");

	/* 3. setx vs seta through the real dispatcher */
	Cmd_ExecuteString("seta r_ov_seta 7");
	Cmd_ExecuteString("setx r_ov_setx 9");
	a = Cvar_FindVar("r_ov_seta");
	b = Cvar_FindVar("r_ov_setx");
	CHECK(a != NULL && strcmp(a->string, "7") == 0, "seta binds (%s)", a ? a->string : "(null)");
	CHECK(b != NULL && strcmp(b->string, "9") == 0, "setx binds (%s)", b ? b->string : "(null)");
	CHECK(a && (a->flags & CVAR_ARCHIVE) != 0, "seta leaves the cvar archived");
	CHECK(b && (b->flags & CVAR_NOARCHIVE) != 0, "setx marks the cvar CVAR_NOARCHIVE");

	/* 4. the shutdown archive: the setx cvar must be absent, the seta one present */
	Cvar_WriteVariables((fileHandle_t)1);
	CHECK(strstr(archive, "r_ov_setx") == NULL,
		  "the archive skipped the setx cvar");
	CHECK(strstr(archive, "r_ov_seta") != NULL, "the archive kept the seta cvar");
	/* and the LATCHed cvar is written with its applied value, never a pending one */
	CHECK(strstr(archive, "r_ov_latch") != NULL, "the archive kept the latched cvar");

	printf("  %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
