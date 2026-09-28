/*
 * bonearm_oracle.c -- the skeleton arena's per-frame property, proven offline.
 *
 * A frame can hold more than one view.  `R_AddSkelSurfaces' poses each skinned
 * model into TIKI_Skel_Bones_Arena[tr.smpFrame] at TIKI_Skel_Bones_Cursor and
 * stamps the row it used on the entity:
 *
 *     ent->e.bonestart = TIKI_Skel_Bones_Cursor[arena];
 *     TIKI_Skel_Bones_Cursor[arena] += boneCount;          (tr_model.cpp)
 *
 * The BACK END draws those entities later, on the walk thread, reading the
 * arena row the entity carries.  Every pose a frame records must therefore stay
 * where it was written until that frame's walk has read it -- which the double
 * arena (one per smpFrame parity) provides, and which a reset of the CURSOR in
 * the middle of a frame destroys: the next view's poses overwrite the previous
 * view's rows, so the earlier view draws with the later view's skeleton.
 *
 * That is the CONTROLS background: the screen runs two UIRenderModels (the
 * stardome from background.inc and the player preview), the dome is posed
 * first, the preview resets the cursor and poses at row 0, and the dome's six
 * surfaces then draw from the preview's bones -- no pixels where the starfield
 * belongs, while the preview itself is correct.  It is the same mechanism for
 * any two-view gameplay frame (the world view posed before a HUD 3D model).
 *
 * This harness LINKS the shipping `tiki/tiki_mesh.cpp' (so the arena, the
 * cursor and the reset are the real ones) and reads the shipping call site out
 * of `tr_scene.c' rather than assuming it, then drives a two-view frame and
 * asserts the two poses occupy disjoint rows of the same arena.
 *
 * Build/run: docs/controls/bonearm/run.sh
 */
#include "corepp/tiki.h"
#include "tiki/tiki_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond, ...) do { \
	checks++; \
	if (!(cond)) { failures++; printf("  FAIL  "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

/* the two shipping lanes this property must hold in */
static const char *LANES[] = {
	"renderervulkan/tr_scene.c",
	"renderergles3/tr_scene.c",
};

static const char *CODE_DIR;

static char *slurp(const char *rel)
{
	char	path[1024];
	FILE	*f;
	long	n;
	char	*buf;

	snprintf(path, sizeof(path), "%s/%s", CODE_DIR, rel);
	f = fopen(path, "rb");
	if (!f) {
		printf("  FAIL  cannot read %s\n", path);
		failures++;
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = (char *)malloc((size_t)n + 1);
	if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
		printf("  FAIL  short read %s\n", path);
		failures++;
	}
	buf[n] = 0;
	fclose(f);
	return buf;
}

/* Which function of a lane contains the arena reset?  The answer decides the
   behaviour, so the harness reads it from the shipping text. */
typedef enum { SITE_NONE = 0, SITE_FRAME, SITE_VIEW } site_t;

static site_t reset_site(const char *text)
{
	const char	*call = "TIKI_Reset_Caches_Arena( tr.smpFrame );";
	const char	*i_frame = strstr(text, "void R_InitNextFrame( void )");
	const char	*i_view = strstr(text, "void RE_RenderScene( const refdef_t *fd )");
	const char	*p;

	if (!i_frame || !i_view) {
		return SITE_NONE;
	}
	p = strstr(text, call);
	if (!p) {
		return SITE_NONE;
	}
	/* the call belongs to whichever of the two bodies starts last before it */
	if (p > i_frame && p < i_view) {
		return SITE_FRAME;
	}
	if (p > i_view) {
		return SITE_VIEW;
	}
	return SITE_NONE;
}

/* The shipping pose step: append boneCount rows at the cursor. */
static int pose(int arena, int boneCount)
{
	int	start = TIKI_Skel_Bones_Cursor[arena];

	CHECK(start + boneCount <= MAX_SKELBONES,
		  "pose of %d bones at cursor %d overflows the %d-row arena",
		  boneCount, start, (int)MAX_SKELBONES);
	TIKI_Skel_Bones_Cursor[arena] += boneCount;
	return start;
}

int main(int argc, char **argv)
{
	int		mutant = (argc > 1) ? atoi(argv[1]) : 0;
	int		lane;
	int		bad = 0;

	CODE_DIR = (argc > 2) ? argv[2] : "../../../app/src/main/cpp/code";

	printf("bonearm oracle: %d lanes, arena rows %d\n",
		   (int)(sizeof(LANES) / sizeof(LANES[0])), (int)MAX_SKELBONES);

	for (lane = 0; lane < (int)(sizeof(LANES) / sizeof(LANES[0])); lane++) {
		char	*text = slurp(LANES[lane]);
		site_t	site;

		if (!text) {
			bad = 1;
			continue;
		}
		site = reset_site(text);

		/* planted faults select the other call sites, so no arm passes vacuously */
		if (mutant == 1) {
			site = SITE_VIEW;			/* the per-view rewind: the defect */
		} else if (mutant == 2) {
			site = SITE_NONE;			/* no reset: the cursor never rewinds */
		}

		CHECK(site != SITE_NONE,
			  "%s has no live TIKI_Reset_Caches_Arena( tr.smpFrame ) call", LANES[lane]);
		CHECK(site != SITE_VIEW,
			  "%s resets the bone cursor inside RE_RenderScene: a second view of the "
			  "same frame overwrites the first view's poses before the walk reads them",
			  LANES[lane]);

		if (site == SITE_VIEW || site == SITE_NONE) {
			bad = 1;
		}

		/* ---- the two-view frame, driven through the real arena ------------ */
		{
			const int	arena = 1;			/* any parity; the bug is not about parity */
			const int	domeBones = 8;		/* models/sky/sky_UI-stardome02.tik */
			const int	bodyBones = 48;		/* the player preview */
			int			domeStart, bodyStart, i, frame;

			memset(TIKI_Skel_Bones_Arena, 0, sizeof(TIKI_Skel_Bones_Arena));
			memset(TIKI_Skel_Bones_Cursor, 0, sizeof(TIKI_Skel_Bones_Cursor));

			/* frame boundary (R_InitNextFrame) */
			if (site == SITE_FRAME) {
				TIKI_Reset_Caches_Arena(arena);
			}

			/* view 1: the background model */
			domeStart = pose(arena, domeBones);

			/* the next RE_RenderScene */
			if (site == SITE_VIEW) {
				TIKI_Reset_Caches_Arena(arena);
			}

			/* view 2: the player preview */
			bodyStart = pose(arena, bodyBones);

			if (mutant == 3) {
				domeStart = bodyStart;		/* planted: the poses are forced to alias */
			}

			/* the walk draws view 1, then view 2, in the frame's own order */
			CHECK(domeStart + domeBones <= bodyStart || bodyStart + bodyBones <= domeStart,
				  "%s: the frame's first pose [%d,%d) and its second [%d,%d) overlap in "
				  "arena %d -- the first view draws with the second view's skeleton",
				  LANES[lane], domeStart, domeStart + domeBones, bodyStart, bodyStart + bodyBones,
				  arena);

			/* ---- and the cursor must rewind at least once per frame -------
			   With no reset at all the arena fills within a frame; the SITE_NONE
			   check above names that, and four frames of posing must stay inside
			   the arena either way. */
			for (frame = 0; frame < 4; frame++) {
				if (site == SITE_FRAME || site == SITE_VIEW) {
					TIKI_Reset_Caches_Arena(arena);
				}
				(void)pose(arena, domeBones);
			}
			CHECK(site != SITE_VIEW || TIKI_Skel_Bones_Cursor[arena] == domeBones,
				  "%s: the per-view reset leaves the cursor at %d rows after four frames",
				  LANES[lane], TIKI_Skel_Bones_Cursor[arena]);
			(void)i;
		}
		free(text);
	}

	printf("bonearm oracle: %d checks, %d failures\n", checks, failures);
	return (failures || bad) ? 1 : 0;
}
