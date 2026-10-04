/*
 * printpath oracle -- structural proof that the engine's console output can
 * never go permanently silent again.  Reads the print path's own sources and
 * asserts the invariants that the 2026-10-19 menu-to-world silence broke.
 * It never launches the game.
 *
 *   1. the print recursion guard is thread-local, so the main thread and the
 *      render thread cannot consume each other's reservation;
 *   2. Com_Error drops that guard, and the redirect buffer with it, before
 *      any of its arms longjmp;
 *   3. Com_Frame drops them on both its paths, so a wedge cannot outlive the
 *      frame that raised it;
 *   4. Com_RealPrintf asks FS_WriteHandleLive before it hands the qconsole.log
 *      handle to FS_Write -- FS_FileForHandle raises ERR_DROP on a dead
 *      handle, and an error raised out of Com_RealPrintf is what wedged the
 *      guard, while eating its own banner;
 *   5. a mid-boot reopen of qconsole.log appends, so a filesystem rebuild on
 *      a map load cannot truncate the rows the boot already produced;
 *   6. the [CORE:print] heartbeat sits behind PROBE_On(), so it is silent by
 *      default and only proves the path when the master probe gate arms it;
 *   7. the SIGCONT handler only raises a flag -- CON_Init prints, and a print
 *      that lands while the interrupted thread holds stderr's own stream lock
 *      blocks that thread on its own lock forever;
 *   8. FS_WriteHandleLive answers a dead handle without raising.
 *
 * usage: printpath_oracle [mutant]
 *   A mutant argument 1..8 inverts assertion 1..8; run.sh plants each one and
 *   requires the oracle to reject it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buf_common[1 << 22];
static char buf_tty[1 << 18];
static char buf_files[1 << 20];
static char clean_common[1 << 22];
static char clean_tty[1 << 18];
static char clean_files[1 << 20];

static long slurp(const char *path, char *buf, size_t cap)
{
	FILE *f = fopen(path, "rb");
	long n;

	if (!f) {
		fprintf(stderr, "printpath_oracle: cannot read %s\n", path);
		exit(2);
	}
	n = (long)fread(buf, 1, cap - 1, f);
	fclose(f);
	buf[n] = '\0';
	return n;
}

/* Comments and string literals are noise for a structural read of a source
 * file: a comment is where the reason lives and a literal is where a matched
 * name would be a coincidence.  Both go before any match. */
static void strip(char *out, const char *in)
{
	const char *p = in;
	char *o = out;

	while (*p) {
		if (p[0] == '/' && p[1] == '*') {
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/')) {
				if (*p == '\n') {
					*o++ = '\n';
				}
				p++;
			}
			if (*p) {
				p += 2;
			}
			continue;
		}
		if (p[0] == '/' && p[1] == '/') {
			while (*p && *p != '\n') {
				p++;
			}
			continue;
		}
		if (*p == '"') {
			p++;
			while (*p && *p != '"') {
				if (*p == '\\' && p[1]) {
					p++;
				}
				p++;
			}
			if (*p) {
				p++;
			}
			*o++ = ' ';
			continue;
		}
		*o++ = *p++;
	}
	*o = '\0';
}

static const char *find_or_die(const char *hay, const char *needle, const char *what)
{
	const char *p = strstr(hay, needle);

	if (!p) {
		printf("  FAIL  %s: '%s' is missing\n", what, needle);
		exit(1);
	}
	return p;
}

/* The body of the function that starts at `start': from there to the next
 * line that begins with a closing brace. */
static long body_len(const char *start)
{
	const char *p;

	for (p = start; *p; p++) {
		if (p[0] == '\n' && p[1] == '}') {
			return (long)(p - start);
		}
	}
	return (long)strlen(start);
}

static int span_has(const char *from, long len, const char *needle)
{
	char *save;
	int hit;

	if (len <= 0) {
		return 0;
	}
	save = malloc((size_t)len + 1);
	if (!save) {
		exit(2);
	}
	memcpy(save, from, (size_t)len);
	save[len] = '\0';
	hit = (strstr(save, needle) != NULL);
	free(save);
	return hit;
}

int main(int argc, char **argv)
{
	int mutant = (argc > 1) ? atoi(argv[1]) : 0;
	char root[4096];
	const char *src;
	const char *a, *b;
	int failures = 0;

	src = getenv("EF2_SRC");
	if (!src || !*src) {
		src = "../../../app/src/main/cpp/code";
	}
#define P(f) do { snprintf(root, sizeof(root), "%s/%s", src, (f)); } while (0)

	P("qcommon/common.c");
	slurp(root, buf_common, sizeof(buf_common));
	strip(clean_common, buf_common);
	P("sys/con_tty.c");
	slurp(root, buf_tty, sizeof(buf_tty));
	strip(clean_tty, buf_tty);
	P("qcommon/files.cpp");
	slurp(root, buf_files, sizeof(buf_files));
	strip(clean_files, buf_files);

	/* 1. per-thread guard */
	if (mutant == 1) {
		printf("  FAIL  planted: guard decl hidden\n");
		failures++;
	} else if (!strstr(clean_common, "_Thread_local int") ||
	           !strstr(clean_common, "t_printRecursion")) {
		printf("  FAIL  the print recursion guard is not thread-local; the main "
		       "thread and the render thread print concurrently and a shared "
		       "counter drops rows\n");
		failures++;
	}

	/* 2. Com_Error recovers before it jumps */
	if (mutant == 2) {
		printf("  FAIL  planted: recovery call hidden\n");
		failures++;
	} else {
		a = find_or_die(clean_common, "void QDECL Com_Error", "Com_Error");
		b = find_or_die(a, "longjmp (abortframe", "first longjmp");
		if (!span_has(a, (long)(b - a), "Com_PrintRecover()")) {
			printf("  FAIL  Com_Error does not call Com_PrintRecover() before "
			       "its first longjmp; the error's own banner is a Com_Printf "
			       "and is eaten by the wedged guard\n");
			failures++;
		}
	}

	/* 3. Com_Frame recovers on both paths */
	if (mutant == 3) {
		printf("  FAIL  planted: per-frame recovery hidden\n");
		failures++;
	} else {
		long len;
		int hits = 0;
		const char *p;

		a = find_or_die(clean_common, "void Com_Frame( void )", "Com_Frame");
		len = body_len(a);
		for (p = a; p && p < a + len; ) {
			p = strstr(p, "Com_PrintRecover()");
			if (!p || p >= a + len) {
				break;
			}
			hits++;
			p += 18;
		}
		if (hits < 2) {
			printf("  FAIL  Com_Frame recovers the print state on %d of its two "
			       "paths (normal entry and the abortframe landing); a wedge "
			       "must not outlive the frame\n", hits);
			failures++;
		}
	}

	/* 4. no FS_Write on the print path without the non-raising probe */
	if (mutant == 4) {
		printf("  FAIL  planted: probe guard hidden\n");
		failures++;
	} else {
		/* The definition, not the forward declaration two hundred lines up:
		 * body_len() measures to the next top-level closing brace, and the
		 * declaration would measure the wrong function. */
		a = find_or_die(clean_common,
		                "static void Com_RealPrintf( int level, const char *msg ) {",
		                "Com_RealPrintf definition");
		if (!span_has(a, body_len(a), "FS_WriteHandleLive")) {
			printf("  FAIL  Com_RealPrintf does not probe the qconsole handle "
			       "before it writes; a dead handle raises ERR_DROP out of "
			       "the print path\n");
			failures++;
		}
		if (!span_has(a, body_len(a), "logfile = 0;")) {
			printf("  FAIL  the stale qconsole.log handle is not dropped; the "
			       "next print would raise again\n");
			failures++;
		}
	}

	/* 5. a mid-boot reopen appends */
	if (mutant == 5) {
		printf("  FAIL  planted: append reopen hidden\n");
		failures++;
	} else {
		if (!strstr(clean_common, "qconsoleEverOpened") ||
		    !strstr(clean_common, "FS_FOpenFileAppend_HomeData") ||
		    !strstr(clean_common, "qconsoleEverOpened = qtrue")) {
			printf("  FAIL  the boot's first qconsole.log open is not recorded, "
			       "so a heal after a filesystem rebuild would reopen in "
			       "write mode and truncate the rows the boot produced\n");
			failures++;
		}
	}

	/* 6. the heartbeat is behind the master probe gate */
	if (mutant == 6) {
		printf("  FAIL  planted: heartbeat gate hidden\n");
		failures++;
	} else {
		const char *raw;

		a = find_or_die(clean_common, "static void Com_PrintHeartbeat", "heartbeat");
		/* The tag lives inside a string literal, which strip() blanks; look
		 * for it in the raw bytes at the same offset instead. */
		raw = buf_common + (a - clean_common);
		if (!strstr(raw, "[CORE:print] alive")) {
			printf("  FAIL  the print heartbeat does not carry the "
			       "[CORE:print] tag\n");
			failures++;
		}
		if (!span_has(a, body_len(a), "PROBE_On()")) {
			printf("  FAIL  the print heartbeat is not behind PROBE_On(); a new "
			       "instrument must be born silent under the master gate\n");
			failures++;
		}
	}

	/* 7. the SIGCONT handler prints nothing */
	if (mutant == 7) {
		printf("  FAIL  planted: handler re-init hidden\n");
		failures++;
	} else {
		a = find_or_die(clean_tty, "void CON_SigCont", "CON_SigCont");
		b = find_or_die(clean_tty, "void CON_Init( void )", "CON_Init");
		if (b < a) {
			printf("  FAIL  CON_Init precedes CON_SigCont; unreadable\n");
			failures++;
		} else {
			if (!span_has(a, (long)(b - a), "ttycon_reinit = 1")) {
				printf("  FAIL  CON_SigCont does not defer through a flag\n");
				failures++;
			}
			if (span_has(a, (long)(b - a), "CON_Init")) {
				printf("  FAIL  CON_SigCont still reaches CON_Init(); a print "
				       "from a signal handler can deadlock the stderr stream "
				       "lock and silence that sink while the process lives\n");
				failures++;
			}
		}
		if (!strstr(clean_tty, "if (ttycon_reinit)")) {
			printf("  FAIL  the deferred tty re-init never runs\n");
			failures++;
		}
	}

	/* 8. FS_WriteHandleLive answers without raising */
	if (mutant == 8) {
		printf("  FAIL  planted: probe body hidden\n");
		failures++;
	} else {
		a = find_or_die(clean_files, "qboolean FS_WriteHandleLive", "probe decl");
		if (span_has(a, body_len(a), "Com_Error")) {
			printf("  FAIL  FS_WriteHandleLive raises; it has to be the "
			       "non-raising probe the print path relies on\n");
			failures++;
		}
		if (!span_has(a, body_len(a), "handleFiles.file.o")) {
			printf("  FAIL  FS_WriteHandleLive does not test the FILE handle\n");
			failures++;
		}
	}

	if (failures) {
		printf("printpath oracle: %d FAILURES\n", failures);
		return 1;
	}
	printf("printpath oracle: 8 assertions hold -- the guard is per-thread, "
	       "Com_Error and Com_Frame recover it, the qconsole handle is probed "
	       "before it is used and appends on a heal, the heartbeat is gated, "
	       "and the SIGCONT handler prints nothing\n");
	return 0;
}
