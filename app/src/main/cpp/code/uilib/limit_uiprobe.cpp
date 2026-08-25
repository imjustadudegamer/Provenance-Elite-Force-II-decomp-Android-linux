
#include "ui_local.h"
#include "limit_uiprobe.h"
#include "../qcommon/qa_ui.h"


#include <string.h>
#include <stdint.h>

#define LIMIT_WINDOW_MS		1000
#define LIMIT_MAX_FRAMES		3600
#define LIMIT_MAX_ROWS		300
#define LIMIT_VW_CLASSES		96	
#define LIMIT_VW_INSTS		128
#define LIMIT_VW_TOP		3
#define LIMIT_MAX_ZONES		8
#define LIMIT_MAX_COUNTERS	3
#define LIMIT_MAX_DEPTH		16
#define LIMIT_VW_LINE		1400

static const char *limit_famName[LIMIT_FAM_COUNT] = {
	"vw", "uis", "uit", "sub"
};

static const int limit_zoneCount[LIMIT_FAM_COUNT] = {
	LIMIT_VW_ZONES, LIMIT_UIS_ZONES, LIMIT_UIT_ZONES, LIMIT_SUB_ZONES
};

static const int limit_counterCount[LIMIT_FAM_COUNT] = {
	LIMIT_VW_COUNTERS, 0, LIMIT_UIT_COUNTERS, LIMIT_SUB_COUNTERS
};

static const int limit_hasTotal[LIMIT_FAM_COUNT] = {
	1, 1, 0, 0
};

static const char *limit_zoneName[LIMIT_FAM_COUNT][LIMIT_MAX_ZONES] = {
	{ "dispself", "disploop", "updself", "updloop", "other", "", "", "" },
	{ "nomenu", "machine", "hudhide", "hudshow", "other", "", "", "" },
	{ "aspect", "cine", "srvref", "srvnone", "srvping", "srvlist", "printpop", "" },
	{ "abars", "submit", "push", "", "", "", "", "" }
};

static const char *limit_counterName[LIMIT_FAM_COUNT][LIMIT_MAX_COUNTERS] = {
	{ "disp", "upd", "" },
	{ "", "", "" },
	{ "srvrun", "srvn", "" },
	{ "armed", "wins", "submits" }
};

typedef char limit_assert_zones[(LIMIT_VW_ZONES <= LIMIT_MAX_ZONES
	&& LIMIT_UIS_ZONES <= LIMIT_MAX_ZONES && LIMIT_UIT_ZONES <= LIMIT_MAX_ZONES
	&& LIMIT_SUB_ZONES <= LIMIT_MAX_ZONES) ? 1 : -1];
typedef char limit_assert_counters[(LIMIT_VW_COUNTERS <= LIMIT_MAX_COUNTERS
	&& LIMIT_UIT_COUNTERS <= LIMIT_MAX_COUNTERS
	&& LIMIT_SUB_COUNTERS <= LIMIT_MAX_COUNTERS) ? 1 : -1];

static int	limit_frames;
static int	limit_windowStart;
static int	limit_windowArmed;
static int	limit_rowsEmitted[LIMIT_FAM_COUNT];
static int	limit_zoneSum[LIMIT_FAM_COUNT][LIMIT_MAX_ZONES];
static int	limit_zoneMax[LIMIT_FAM_COUNT][LIMIT_MAX_ZONES];
static int	limit_cntSum[LIMIT_FAM_COUNT][LIMIT_MAX_COUNTERS];
static int	limit_cntMax[LIMIT_FAM_COUNT][LIMIT_MAX_COUNTERS];
static int	limit_totalSum[LIMIT_FAM_COUNT];
static int	limit_totalMax[LIMIT_FAM_COUNT];
static int	limit_frameNamed[LIMIT_FAM_COUNT];
static int	limit_frameTotal[LIMIT_FAM_COUNT];
static int	limit_frameZone[LIMIT_FAM_COUNT][LIMIT_MAX_ZONES];
static int	limit_frameCnt[LIMIT_FAM_COUNT][LIMIT_MAX_COUNTERS];

typedef struct limit_cls_s {
	const char	*key;			
	char		cls[24];
	int		dispSum, dispMax;	
	int		kidsSum, kidsMax;
	int		updSum, updMax;
	int		dispCalls, updCalls;
} limit_cls_t;

static limit_cls_t	limit_clsTab[LIMIT_VW_CLASSES] = {
	{ "(other)", "(other)", 0, 0, 0, 0, 0, 0, 0, 0 }
};
static int		limit_frameClsDisp[LIMIT_VW_CLASSES];
static int		limit_frameClsKids[LIMIT_VW_CLASSES];
static int		limit_frameClsUpd[LIMIT_VW_CLASSES];
static int		limit_clsUsed;		
static int		limit_clsOver;		

typedef struct limit_inst_s {
	const char	*clsKey;
	const char	*nameKey;
	char		cls[24];
	char		name[32];
	int		selfSum, selfMax;	
	int		kidsSum, kidsMax;	
	int		calls;
} limit_inst_t;

static limit_inst_t	limit_instTab[LIMIT_VW_INSTS];
static int		limit_instUsed;
static int		limit_instDrop;
static int		limit_instNamed;		

typedef struct limit_walk_s {
	int		depth;
	int		frameT0;
	int		self[LIMIT_MAX_DEPTH];
	int		childSum[LIMIT_MAX_DEPTH];
	int		clsSlot[LIMIT_MAX_DEPTH];
	int		instSlot[LIMIT_MAX_DEPTH];
	int		topTotalTenths;		
} limit_walk_t;

static limit_walk_t	limit_dispWalk;
static limit_walk_t	limit_updWalk;

#define LIMIT_STAMP(t)		((t) + 1)
#define LIMIT_UNSTAMP(s)		((s) - 1)

static int		limit_stateT0;

static int		limit_subArmed;


static void limit_cpy(char *dst, int size, const char *src)
{
	int i = 0;

	if (!src) {
		src = "(null)";
	}

	while (i < size - 1 && src[i]) {
		dst[i] = src[i];
		i++;
	}

	dst[i] = '\0';
}

static void limit_app(char *dst, int size, const char *src)
{
	int l = (int)strlen(dst);
	int i = 0;

	while (l + i < size - 1 && src[i]) {
		dst[l + i] = src[i];
		i++;
	}

	dst[l + i] = '\0';
}

static void limit_zoneAdd(int family, int zone, int usec)
{
	int tenths;

	if (usec < 0) {
		usec = 0;
	}

	tenths = usec / 100;

	limit_frameZone[family][zone] += tenths;
	limit_frameNamed[family] += tenths;
}

static void limit_cntBump(int family, int counter, int n)
{
	limit_frameCnt[family][counter] += n;
}

static void limit_clsReserve(void)
{
	limit_clsTab[0].key = "(other)";
	limit_cpy(limit_clsTab[0].cls, (int)sizeof(limit_clsTab[0].cls), "(other)");
}

static int limit_clsFind(const char *classname)
{
	unsigned long h;
	int	probe, i, idx;

	if (!classname) {
		classname = "(null)";
	}

	h = (unsigned long)((uintptr_t)classname >> 4);
	probe = 1 + (int)(h % (unsigned)(LIMIT_VW_CLASSES - 1));

	for (i = 0; i < LIMIT_VW_CLASSES - 1; i++) {
		idx = 1 + (probe - 1 + i) % (LIMIT_VW_CLASSES - 1);

		if (limit_clsTab[idx].key == classname) {
			return idx;
		}

		if (!limit_clsTab[idx].key) {
			limit_clsTab[idx].key = classname;
			limit_cpy(limit_clsTab[idx].cls, (int)sizeof(limit_clsTab[idx].cls), classname);
			limit_clsUsed++;
			return idx;
		}
	}

	limit_clsOver++;
	return 0;
}

static int limit_instFind(const char *classname, const char *name)
{
	unsigned long h;
	int		i, idx;

	if (!classname) {
		classname = "(null)";
	}
	if (!name) {
		name = "";
	}

	h = (unsigned long)(((uintptr_t)classname >> 4) ^ ((uintptr_t)name >> 4));

	for (i = 0; i < LIMIT_VW_INSTS; i++) {
		idx = (int)((h + (unsigned long)i) % (unsigned) LIMIT_VW_INSTS);

		if (limit_instTab[idx].clsKey == classname && limit_instTab[idx].nameKey == name) {
			return idx;
		}

		if (!limit_instTab[idx].clsKey) {
			limit_instTab[idx].clsKey  = classname;
			limit_instTab[idx].nameKey = name;
			limit_cpy(limit_instTab[idx].cls, (int)sizeof(limit_instTab[idx].cls), classname);
			limit_cpy(limit_instTab[idx].name, (int)sizeof(limit_instTab[idx].name), name);
			limit_instUsed++;
			return idx;
		}
	}

	limit_instDrop++;
	return -1;
}


static int limit_walkEnter(limit_walk_t *w)
{
	int t;

	if (!ui_statsOn) {
		return 0;
	}

	t = UI_Microseconds();

	if (w->depth >= LIMIT_MAX_DEPTH) {
		return 0;
	}

	if (w->depth == 0) {
		w->frameT0 = t;
	}

	w->childSum[w->depth] = 0;
	w->clsSlot[w->depth]  = 0;
	w->instSlot[w->depth] = -1;
	w->depth++;

	return LIMIT_STAMP(t);
}

static int limit_walkSelf(limit_walk_t *w, int t0, int zoneSelf, int counter,
						 const char *classname, const char *name, int isUpdate)
{
	int now, self, tenths, clsIdx, inst;

	if (!ui_statsOn || !t0) {
		return 0;
	}

	now  = UI_Microseconds();
	self = now - LIMIT_UNSTAMP(t0);
	if (self < 0) {
		self = 0;
	}

	clsIdx = limit_clsFind(classname);
	inst   = isUpdate ? -1 : limit_instFind(classname, name);

	w->self[w->depth - 1]     = self;
	w->clsSlot[w->depth - 1]  = clsIdx;
	w->instSlot[w->depth - 1] = inst;

	limit_zoneAdd(LIMIT_FAM_VW, zoneSelf, self);
	limit_cntBump(LIMIT_FAM_VW, counter, 1);

	tenths = self / 100;

	if (isUpdate) {
		limit_frameClsUpd[clsIdx] += tenths;
		limit_clsTab[clsIdx].updCalls++;
	} else {
		limit_frameClsDisp[clsIdx] += tenths;
		limit_clsTab[clsIdx].dispCalls++;

		if (inst >= 0) {
			limit_instTab[inst].selfSum += tenths;
			if (tenths > limit_instTab[inst].selfMax) {
				limit_instTab[inst].selfMax = tenths;
			}
			limit_instTab[inst].calls++;
			limit_instNamed += tenths;
		}
	}

	return LIMIT_STAMP(now);
}

static void limit_walkExit(limit_walk_t *w, int tStamp, int zoneLoop)
{
	int clsIdx, inst;
	int now, kids, loopoh, total, kt;

	if (!ui_statsOn || !tStamp) {
		return;
	}

	now  = UI_Microseconds();
	kids = now - LIMIT_UNSTAMP(tStamp);
	if (kids < 0) {
		kids = 0;
	}

	clsIdx = w->clsSlot[w->depth - 1];
	inst   = w->instSlot[w->depth - 1];

	loopoh = kids - w->childSum[w->depth - 1];
	if (loopoh < 0) {
		loopoh = 0;
	}

	total = w->self[w->depth - 1] + kids;

	limit_zoneAdd(LIMIT_FAM_VW, zoneLoop, loopoh);

	kt = kids / 100;

	limit_frameClsKids[clsIdx] += kt;

	if (inst >= 0) {
		limit_instTab[inst].kidsSum += kt;
		if (kt > limit_instTab[inst].kidsMax) {
			limit_instTab[inst].kidsMax = kt;
		}
	}

	w->depth--;

	if (w->depth == 0) {
		w->topTotalTenths += total / 100;
	} else {
		w->childSum[w->depth - 1] += total;	
	}
}


static void limit_resetWindow(void)
{
	int family, i;

	limit_frames = 0;
	limit_windowStart = 0;
	limit_windowArmed = 0;

	for (family = 0; family < LIMIT_FAM_COUNT; family++) {
		limit_totalSum[family] = limit_totalMax[family] = 0;
		limit_frameNamed[family] = limit_frameTotal[family] = 0;

		for (i = 0; i < LIMIT_MAX_ZONES; i++) {
			limit_zoneSum[family][i] = limit_zoneMax[family][i] = 0;
			limit_frameZone[family][i] = 0;
		}
		for (i = 0; i < LIMIT_MAX_COUNTERS; i++) {
			limit_cntSum[family][i] = limit_cntMax[family][i] = 0;
			limit_frameCnt[family][i] = 0;
		}
	}

	for (i = 0; i < LIMIT_VW_CLASSES; i++) {
		limit_clsTab[i].key       = NULL;
		limit_clsTab[i].cls[0]    = '\0';
		limit_clsTab[i].dispSum   = limit_clsTab[i].dispMax = 0;
		limit_clsTab[i].kidsSum   = limit_clsTab[i].kidsMax = 0;
		limit_clsTab[i].updSum    = limit_clsTab[i].updMax  = 0;
		limit_clsTab[i].dispCalls = limit_clsTab[i].updCalls = 0;
		limit_frameClsDisp[i] = limit_frameClsKids[i] = limit_frameClsUpd[i] = 0;
	}

	limit_clsReserve();
	limit_clsUsed = 0;
	limit_clsOver = 0;

	for (i = 0; i < LIMIT_VW_INSTS; i++) {
		limit_instTab[i].clsKey  = NULL;
		limit_instTab[i].nameKey = NULL;
		limit_instTab[i].cls[0]  = '\0';
		limit_instTab[i].name[0] = '\0';
		limit_instTab[i].selfSum = limit_instTab[i].selfMax = 0;
		limit_instTab[i].kidsSum = limit_instTab[i].kidsMax = 0;
		limit_instTab[i].calls   = 0;
	}

	limit_instUsed  = 0;
	limit_instDrop  = 0;
	limit_instNamed = 0;
	limit_subArmed  = 0;
}

static void limit_printFamily(int family, int window)
{
	char	line[LIMIT_VW_LINE], part[256];
	int	frames = limit_frames < 1 ? 1 : limit_frames;
	int	i, mean10;

	line[0] = '\0';

	for (i = 0; i < limit_zoneCount[family]; i++) {
		int sum = limit_zoneSum[family][i];

		mean10 = (int)(((long long)sum + frames / 2) / frames);
		Com_sprintf(part, sizeof(part), " %s=%d.%d/%d.%d", limit_zoneName[family][i],
					mean10 / 10, mean10 % 10,
					limit_zoneMax[family][i] / 10, limit_zoneMax[family][i] % 10);
		limit_app(line, (int)sizeof(line), part);
	}

	for (i = 0; i < limit_counterCount[family]; i++) {
		int sum = limit_cntSum[family][i];

		mean10 = (int)(((long long)sum * 10 + frames / 2) / frames);
		Com_sprintf(part, sizeof(part), " %s=%d.%d/%d", limit_counterName[family][i],
					mean10 / 10, mean10 % 10, limit_cntMax[family][i]);
		limit_app(line, (int)sizeof(line), part);
	}

	if (limit_hasTotal[family]) {
		int sum = limit_totalSum[family];

		mean10 = (int)(((long long)sum + frames / 2) / frames);
		Com_sprintf(part, sizeof(part), " total=%d.%d/%d.%d",
					mean10 / 10, mean10 % 10,
					limit_totalMax[family] / 10, limit_totalMax[family] % 10);
		limit_app(line, (int)sizeof(line), part);
	}

	if (family == LIMIT_FAM_VW) {
		int used[LIMIT_VW_TOP];
		int k, j, pick;
		int dispSelfSum = limit_zoneSum[LIMIT_FAM_VW][LIMIT_VWZ_dispself];
		int clsSum = 0;

		for (i = 0; i < LIMIT_VW_CLASSES; i++) {
			clsSum += limit_clsTab[i].dispSum;
		}

		Com_sprintf(part, sizeof(part), " unacc=%d.%d unname=%d.%d cls=%d/%d inst=%d/%d drop=%d over=%d",
					(dispSelfSum - clsSum) / 10, (dispSelfSum - clsSum) % 10,
					(dispSelfSum - limit_instNamed) / 10, (dispSelfSum - limit_instNamed) % 10,
					limit_clsUsed, LIMIT_VW_CLASSES,
					limit_instUsed, LIMIT_VW_INSTS, limit_instDrop, limit_clsOver);
		limit_app(line, (int)sizeof(line), part);

		for (k = 0; k < LIMIT_VW_TOP; k++) {
			used[k] = -1;
		}

		for (k = 0; k < LIMIT_VW_TOP; k++) {
			pick = -1;

			for (i = 0; i < LIMIT_VW_CLASSES; i++) {
				int taken = 0;

				for (j = 0; j < k; j++) {
					if (used[j] == i) {
						taken = 1;
						break;
					}
				}

				if (taken || limit_clsTab[i].dispSum <= 0) {
					continue;
				}

				if (pick < 0 || limit_clsTab[i].dispSum > limit_clsTab[pick].dispSum) {
					pick = i;
				}
			}

			if (pick < 0) {
				break;
			}

			used[k] = pick;

			mean10 = (int)(((long long)limit_clsTab[pick].dispSum + frames / 2) / frames);
			Com_sprintf(part, sizeof(part),
						" | cls '%s' self=%d.%d/%d.%d kids=%d.%d/%d.%d upd=%d.%d n=%d",
						limit_clsTab[pick].cls,
						mean10 / 10, mean10 % 10,
						limit_clsTab[pick].dispMax / 10, limit_clsTab[pick].dispMax % 10,
						limit_clsTab[pick].kidsSum / 10, limit_clsTab[pick].kidsSum % 10,
						limit_clsTab[pick].kidsMax / 10, limit_clsTab[pick].kidsMax % 10,
						limit_clsTab[pick].updSum / 10, limit_clsTab[pick].updSum % 10,
						limit_clsTab[pick].dispCalls);
			limit_app(line, (int)sizeof(line), part);
		}

		for (k = 0; k < LIMIT_VW_TOP; k++) {
			used[k] = -1;
		}

		for (k = 0; k < LIMIT_VW_TOP; k++) {
			pick = -1;

			for (i = 0; i < LIMIT_VW_INSTS; i++) {
				int taken = 0;

				for (j = 0; j < k; j++) {
					if (used[j] == i) {
						taken = 1;
						break;
					}
				}

				if (taken || limit_instTab[i].selfSum <= 0) {
					continue;
				}

				if (pick < 0 || limit_instTab[i].selfSum > limit_instTab[pick].selfSum) {
					pick = i;
				}
			}

			if (pick < 0) {
				break;
			}

			used[k] = pick;

			mean10 = (int)(((long long)limit_instTab[pick].selfSum + frames / 2) / frames);
			Com_sprintf(part, sizeof(part),
						" | inst '%s'/'%s' self=%d.%d/%d.%d kids=%d.%d/%d.%d n=%d",
						limit_instTab[pick].cls, limit_instTab[pick].name,
						mean10 / 10, mean10 % 10,
						limit_instTab[pick].selfMax / 10, limit_instTab[pick].selfMax % 10,
						limit_instTab[pick].kidsSum / 10, limit_instTab[pick].kidsSum % 10,
						limit_instTab[pick].kidsMax / 10, limit_instTab[pick].kidsMax % 10,
						limit_instTab[pick].calls);
			limit_app(line, (int)sizeof(line), part);
		}
	}

	Com_Printf("[UI:%s] frames=%d window=%dms%s\n", limit_famName[family], frames, window, line);

	limit_rowsEmitted[family]++;
}


int LIMIT_VW_Enter(void)
{
	return limit_walkEnter(&limit_dispWalk);
}

int LIMIT_VW_Self(int t0, const char *classname, const char *name)
{
	if (!ui_statsOn || !t0) {
		return 0;
	}

	return limit_walkSelf(&limit_dispWalk, t0, LIMIT_VWZ_dispself, LIMIT_VWC_disp,
						 classname, name, 0);
}

void LIMIT_VW_Exit(int tStamp)
{
	limit_walkExit(&limit_dispWalk, tStamp, LIMIT_VWZ_disploop);
}

int LIMIT_VW_UpdEnter(void)
{
	return limit_walkEnter(&limit_updWalk);
}

int LIMIT_VW_UpdSelf(int t0, const char *classname, const char *name)
{
	if (!ui_statsOn || !t0) {
		return 0;
	}

	return limit_walkSelf(&limit_updWalk, t0, LIMIT_VWZ_updself, LIMIT_VWC_upd,
						 classname, name, 1);
}

void LIMIT_VW_UpdExit(int tStamp)
{
	limit_walkExit(&limit_updWalk, tStamp, LIMIT_VWZ_updloop);
}

int LIMIT_Leg(void)
{
	if (!ui_statsOn) {
		return 0;
	}

	return LIMIT_STAMP(UI_Microseconds());
}

void LIMIT_LegEnd(int family, int zone, int t0)
{
	int now, usec;

	if (!ui_statsOn || !t0) {
		return;
	}

	now  = UI_Microseconds();
	usec = now - LIMIT_UNSTAMP(t0);
	if (usec < 0) {
		usec = 0;
	}

	limit_zoneAdd(family, zone, usec);
}

void LIMIT_Count(int family, int counter, int n)
{
	if (!ui_statsOn || counter < 0 || counter >= limit_counterCount[family]) {
		return;
	}

	limit_cntBump(family, counter, n);
}

void LIMIT_StateBegin(void)
{
	if (!ui_statsOn) {
		limit_stateT0 = 0;
		return;
	}

	limit_stateT0 = LIMIT_STAMP(UI_Microseconds());
}

void LIMIT_StateEnd(void)
{
	int now, usec;

	if (!ui_statsOn || !limit_stateT0) {
		limit_stateT0 = 0;
		return;
	}

	now  = UI_Microseconds();
	usec = now - LIMIT_UNSTAMP(limit_stateT0);
	if (usec < 0) {
		usec = 0;
	}

	limit_frameTotal[LIMIT_FAM_UIS] += usec / 100;
	limit_stateT0 = 0;
}


void LIMIT_SubArm(void)
{
	if (!ui_statsOn) {
		limit_subArmed = 0;
		return;
	}

	limit_subArmed = 1;
	limit_cntBump(LIMIT_FAM_SUB, LIMIT_SUBC_armed, 1);
}

int LIMIT_SubT0(void)
{
	if (!ui_statsOn) {
		return 0;
	}

	return LIMIT_STAMP(UI_Microseconds());
}

void LIMIT_SubPushEnd(int t0)
{
	int now, usec, zone;

	if (!ui_statsOn || !t0) {
		return;
	}

	now  = UI_Microseconds();
	usec = now - LIMIT_UNSTAMP(t0);
	if (usec < 0) {
		usec = 0;
	}

	if (limit_subArmed) {
		limit_subArmed = 0;
		zone = LIMIT_SUBZ_submit;
		limit_cntBump(LIMIT_FAM_SUB, LIMIT_SUBC_submits, 1);
	} else {
		zone = LIMIT_SUBZ_push;
	}

	limit_cntBump(LIMIT_FAM_SUB, LIMIT_SUBC_wins, 1);
	limit_zoneAdd(LIMIT_FAM_SUB, zone, usec);
}

void LIMIT_SubZoneEnd(int zone, int t0)
{
	if (!ui_statsOn || !t0) {
		return;
	}

	limit_zoneAdd(LIMIT_FAM_SUB, zone, UI_Microseconds() - LIMIT_UNSTAMP(t0));
}

void LIMIT_UiFrameEnd(void)
{
	int family, i, window;

	if (!ui_statsOn) {
		limit_windowStart = 0;
		limit_windowArmed = 0;
		limit_frames = 0;
		limit_subArmed = 0;
		return;
	}

	limit_frameTotal[LIMIT_FAM_VW] += limit_dispWalk.topTotalTenths + limit_updWalk.topTotalTenths;
	limit_dispWalk.topTotalTenths = 0;
	limit_updWalk.topTotalTenths = 0;

	for (i = 0; i < LIMIT_VW_CLASSES; i++) {
		int v;

		v = limit_frameClsDisp[i];
		limit_clsTab[i].dispSum += v;
		if (v > limit_clsTab[i].dispMax) {
			limit_clsTab[i].dispMax = v;
		}
		limit_frameClsDisp[i] = 0;

		v = limit_frameClsKids[i];
		limit_clsTab[i].kidsSum += v;
		if (v > limit_clsTab[i].kidsMax) {
			limit_clsTab[i].kidsMax = v;
		}
		limit_frameClsKids[i] = 0;

		v = limit_frameClsUpd[i];
		limit_clsTab[i].updSum += v;
		if (v > limit_clsTab[i].updMax) {
			limit_clsTab[i].updMax = v;
		}
		limit_frameClsUpd[i] = 0;
	}

	for (family = 0; family < LIMIT_FAM_COUNT; family++) {
		if (limit_hasTotal[family]) {
			int other = limit_frameTotal[family] - limit_frameNamed[family];
			int last  = limit_zoneCount[family] - 1;

			if (other < 0) {
				other = 0;
			}

			limit_frameZone[family][last] += other;

			limit_totalSum[family] += limit_frameTotal[family];
			if (limit_frameTotal[family] > limit_totalMax[family]) {
				limit_totalMax[family] = limit_frameTotal[family];
			}
		}

		for (i = 0; i < limit_zoneCount[family]; i++) {
			int v = limit_frameZone[family][i];

			limit_zoneSum[family][i] += v;
			if (v > limit_zoneMax[family][i]) {
				limit_zoneMax[family][i] = v;
			}
			limit_frameZone[family][i] = 0;
		}

		for (i = 0; i < limit_counterCount[family]; i++) {
			int v = limit_frameCnt[family][i];

			limit_cntSum[family][i] += v;
			if (v > limit_cntMax[family][i]) {
				limit_cntMax[family][i] = v;
			}
			limit_frameCnt[family][i] = 0;
		}

		limit_frameNamed[family] = 0;
		limit_frameTotal[family] = 0;
	}

	limit_frames++;

	if (!limit_windowArmed) {
		limit_windowStart = Sys_Milliseconds();
		limit_windowArmed = 1;
	}

	window = Sys_Milliseconds() - limit_windowStart;

	if (window < LIMIT_WINDOW_MS && limit_frames < LIMIT_MAX_FRAMES) {
		return;
	}

	for (family = 0; family < LIMIT_FAM_COUNT; family++) {
		if (limit_rowsEmitted[family] < LIMIT_MAX_ROWS) {
			limit_printFamily(family, window);
		} else if (limit_rowsEmitted[family] == LIMIT_MAX_ROWS) {
			limit_rowsEmitted[family]++;
			Com_Printf("[UI:%s] capped at %d rows; no further lines\n",
					   limit_famName[family], LIMIT_MAX_ROWS);
		}
	}

	limit_resetWindow();
	limit_windowStart = Sys_Milliseconds();
	limit_windowArmed = 1;
}

