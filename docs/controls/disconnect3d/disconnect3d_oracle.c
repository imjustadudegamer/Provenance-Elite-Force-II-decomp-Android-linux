/*
 * disconnect3d oracle -- structural proof that a disconnect off a mission-failure
 * screen reaches the main menu instead of dying in the renderer.
 *
 * The 2026-10-20 boot (run-20261020-160847) caught it whole: the operator pressed
 * Main Menu, `disconnect' ran, Com_Error(ERR_DISCONNECT, "") longjmp'd cleanly and
 * the engine kept going, and the process still died 190 rows later with
 *
 *     signal 11, si_code 1, faulting address 0x3f4
 *     R_AddDrawSurf <- R_AddSkelSurfaces <- R_AddEntitySurfaces
 *       <- R_GenerateDrawSurfs <- R_RenderView <- RE_RenderScene
 *       <- CL_Draw3DModel <- UIRenderModel::Draw <- SCR_UpdateScreen
 *
 * 0x3f4 is offsetof(shader_t, detailShader), so R_AddDrawSurf was handed a NULL
 * shader.  This control holds the invariants that close that:
 *
 *   1. shader_t::detailShader really is at 0x3f4, so the fault address names the
 *      deref and not some other access;
 *   2. R_SetupShaders zeroes tr.shaders[] and resets tr.numShaders, which is what
 *      makes every surviving handle stale;
 *   3. R_FreeNonTikiModels skips MOD_TIKI, so TIKI models survive that invalidation;
 *   4. the Vulkan tiki cache-hit arm re-derives the surface handles once the
 *      registration sequence has moved, instead of leaving them on the dead table;
 *   5. the GLES3 twin does the same;
 *   6. UIRenderModel's registration memo dies with the sequence that produced it,
 *      so the model is re-registered at all;
 *   7. R_AddDrawSurf carries no NULL-shader guard -- the fix is the mechanism, not
 *      a catch-all that would mask a real NULL;
 *   8. CL_Disconnect_f raises ERR_DISCONNECT with an empty format, which is the
 *      silent-disconnect arm, never a message;
 *   9. Com_Error substitutes "" for a NULL format before it formats, so a NULL-fmt
 *      error cannot fault before the longjmp.
 *
 * usage: disconnect3d_oracle [mutant]
 *   A mutant argument 1..9 inverts assertion 1..9; run.sh plants each one and
 *   requires the oracle to reject it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* The real data model, so assertion 1 is a fact about the shipped struct and not
 * about a number copied out of a log. */
#include "../../../app/src/main/cpp/code/qcommon/q_shared.h"
#include "../../../app/src/main/cpp/code/qcommon/qfiles.h"
#include "../../../app/src/main/cpp/code/qcommon/qcommon.h"
#include "../../../app/src/main/cpp/code/renderercommon/tr_public.h"
#include "../../../app/src/main/cpp/code/renderercommon/tr_common.h"
#include "../../../app/src/main/cpp/code/qcommon/q_jobs.h"
#include "../../../app/src/main/cpp/code/renderervulkan/tr_hwtex.h"
#include "../../../app/src/main/cpp/code/renderervulkan/tr_texcache.h"
#include "../../../app/src/main/cpp/code/renderervulkan/vk_local.h"
#include "../../../app/src/main/cpp/code/renderervulkan/tr_local.h"

static char buf_a[1 << 22];
static char buf_b[1 << 20];
static char buf_c[1 << 21];
static char clean_a[1 << 22];
static char clean_b[1 << 20];
static char clean_c[1 << 21];
static char raw_a[1 << 22];

static long slurp(const char *path, char *buf, size_t cap)
{
	FILE *f = fopen(path, "rb");
	long n;

	if (!f) {
		fprintf(stderr, "disconnect3d_oracle: cannot read %s\n", path);
		exit(2);
	}
	n = (long)fread(buf, 1, cap - 1, f);
	fclose(f);
	buf[n] = '\0';
	return n;
}

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
	size_t off_detail;
	long len;

	src = getenv("EF2_SRC");
	if (!src || !*src) {
		src = "../../../app/src/main/cpp/code";
	}
#define P(f) do { snprintf(root, sizeof(root), "%s/%s", src, (f)); } while (0)

	/* 1. the fault address names shader_t::detailShader */
	off_detail = offsetof(shader_t, detailShader);
	if (mutant == 1) {
		printf("  FAIL  planted: detailShader offset read as 0x400\n");
		failures++;
	} else if (off_detail != 0x3f4) {
		printf("  FAIL  shader_t::detailShader is at 0x%zx, the boot faulted at 0x3f4\n",
			   off_detail);
		failures++;
	}

	P("renderervulkan/tr_shader.c");
	slurp(root, buf_a, sizeof(buf_a));
	strip(clean_a, buf_a);

	P("renderervulkan/tr_model.cpp");
	slurp(root, buf_b, sizeof(buf_b));
	strip(clean_b, buf_b);

	P("uilib/uirendermodel.cpp");
	slurp(root, buf_c, sizeof(buf_c));
	strip(clean_c, buf_c);

	/* 2. the registration wipes the shader table the handles index into */
	a = find_or_die(clean_a, "void R_SetupShaders(void)", "R_SetupShaders");
	len = body_len(a);
	if (mutant == 2) {
		printf("  FAIL  planted: table wipe hidden\n");
		failures++;
	} else if (!span_has(a, len, "tr.numShaders = 0")
			   || !span_has(a, len, "Com_Memset( tr.shaders, 0, sizeof( tr.shaders ) )")) {
		printf("  FAIL  R_SetupShaders no longer zeroes tr.shaders[]\n");
		failures++;
	}

	/* 3. TIKI models survive that wipe */
	a = find_or_die(clean_b, "void R_FreeNonTikiModels(void)", "R_FreeNonTikiModels");
	len = body_len(a);
	if (mutant == 3) {
		printf("  FAIL  planted: TIKI survival hidden\n");
		failures++;
	} else if (!span_has(a, len, "type == MOD_TIKI") || !span_has(a, len, "continue")) {
		printf("  FAIL  R_FreeNonTikiModels no longer keeps TIKI models across a registration\n");
		failures++;
	}

	/* 4. the Vulkan cache-hit arm re-derives the handles on a sequence move */
	a = find_or_die(clean_b, "qhandle_t R_RegisterModelInternal", "R_RegisterModelInternal");
	len = body_len(a);
	if (mutant == 4) {
		printf("  FAIL  planted: Vulkan handle refresh hidden\n");
		failures++;
	} else if (!span_has(a, len, "else if (mod->r_sequence != r_sequencenumber)")
			   || !span_has(a, len, "mod->r_sequence = r_sequencenumber;")
			   || !span_has(a, len, "R_RegisterShaders(mod)")) {
		printf("  FAIL  Vulkan R_RegisterModelInternal leaves a cache-hit tiki on the dead table\n");
		failures++;
	}

	P("renderergles3/tr_model.cpp");
	slurp(root, buf_a, sizeof(buf_a));
	strip(clean_a, buf_a);

	/* 5. the GLES3 twin does the same */
	a = find_or_die(clean_a, "qhandle_t R_RegisterModelInternal", "R_RegisterModelInternal");
	len = body_len(a);
	if (mutant == 5) {
		printf("  FAIL  planted: GLES3 handle refresh hidden\n");
		failures++;
	} else if (!span_has(a, len, "else if (mod->r_sequence != r_sequencenumber)")
			   || !span_has(a, len, "mod->r_sequence = r_sequencenumber;")
			   || !span_has(a, len, "R_RegisterShaders(mod)")) {
		printf("  FAIL  GLES3 R_RegisterModelInternal leaves a cache-hit tiki on the dead table\n");
		failures++;
	}

	/* 6. the UI memo dies with the sequence that produced it */
	a = find_or_die(clean_c, "void UIRenderModel::Draw(void)", "UIRenderModel::Draw");
	len = body_len(a);
	if (mutant == 6) {
		printf("  FAIL  planted: memo sequence test hidden\n");
		failures++;
	} else if (!span_has(a, len, "re.GetRegistrationSequence()")) {
		printf("  FAIL  UIRenderModel::Draw keeps its model memo across a registration\n");
		failures++;
	}

	/* 7. no catch-all in R_AddDrawSurf: the fix is the mechanism */
	P("renderervulkan/tr_main.c");
	slurp(root, buf_a, sizeof(buf_a));
	strip(clean_a, buf_a);

	a = find_or_die(clean_a, "void R_AddDrawSurf(surfaceType_t* surface, shader_t* shader", "R_AddDrawSurf");
	len = body_len(a);
	if (mutant == 7) {
		printf("  FAIL  planted: catch-all present\n");
		failures++;
	} else if (span_has(a, len, "shader == NULL") || span_has(a, len, "!shader")
			   || span_has(a, len, "defaultShader")) {
		printf("  FAIL  R_AddDrawSurf grew a NULL-shader catch-all that would mask a real NULL\n");
		failures++;
	}

	P("client/cl_main.cpp");
	slurp(root, buf_a, sizeof(buf_a));
	memcpy(raw_a, buf_a, strlen(buf_a) + 1);
	strip(clean_a, buf_a);

	/* 8. the disconnect raiser passes the empty string, never a message.  The literal
	 * only survives in the raw text, since strip() erases string contents. */
	a = find_or_die(clean_a, "void CL_Disconnect_f( void )", "CL_Disconnect_f");
	len = body_len(a);
	if (mutant == 8) {
		printf("  FAIL  planted: raiser format hidden\n");
		failures++;
	} else if (!span_has(raw_a, (long)strlen(raw_a), "Com_Error( ERR_DISCONNECT, \"\" )")) {
		printf("  FAIL  CL_Disconnect_f no longer raises the empty-text disconnect\n");
		failures++;
	}

	P("qcommon/common.c");
	slurp(root, buf_a, sizeof(buf_a));
	memcpy(raw_a, buf_a, strlen(buf_a) + 1);
	strip(clean_a, buf_a);

	/* 9. a NULL format is tolerated before it is formatted */
	a = find_or_die(clean_a, "void QDECL Com_Error( int code, const char *fmt, ... )", "Com_Error");
	len = body_len(a);
	if (mutant == 9) {
		printf("  FAIL  planted: NULL-format guard hidden\n");
		failures++;
	} else if (!span_has(clean_a, (long)strlen(clean_a), "if ( fmt == NULL )")) {
		printf("  FAIL  Com_Error no longer tests for a NULL format\n");
		failures++;
	} else if (!span_has(raw_a, (long)strlen(raw_a), "fmt = \"\";")) {
		printf("  FAIL  Com_Error no longer substitutes \"\" for a NULL format\n");
		failures++;
	}

	if (failures) {
		printf("disconnect3d control: %d FAIL\n", failures);
		return 1;
	}
	printf("disconnect3d control: GREEN\n");
	return 0;
}
