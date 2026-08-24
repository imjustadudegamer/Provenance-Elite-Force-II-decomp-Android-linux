
#include "ui_local.h"
#include "r203_uiprobe.h"
#include "../qcommon/qa_r202.h"


#include <string.h>
#include <stdint.h>

#define R203_WINDOW_MS		1000
#define R203_MAX_FRAMES		3600
#define R203_MAX_ROWS		300
#define R203_VW_CLASSES		96	
#define R203_VW_INSTS		128
#define R203_VW_TOP		3
#define R203_MAX_ZONES		8
#define R203_MAX_COUNTERS	3
#define R203_MAX_DEPTH		16
#define R203_VW_LINE		1400

static const char *r203_famName[R203_FAM_COUNT] = {
	"vw", "uis", "uit", "sub"
};

static const int r203_zoneCount[R203_FAM_COUNT] = {
	R203_VW_ZONES, R203_UIS_ZONES, R203_UIT_ZONES, R203_SUB_ZONES
};

static const int r203_counterCount[R203_FAM_COUNT] = {
	R203_VW_COUNTERS, 0, R203_UIT_COUNTERS, R203_SUB_COUNTERS
};

static const int r203_hasTotal[R203_FAM_COUNT] = {
	1, 1, 0, 0
};

static const char *r203_zoneName[R203_FAM_COUNT][R203_MAX_ZONES] = {
	{ "dispself", "disploop", "updself", "updloop", "other", "", "", "" },
	{ "nomenu", "machine", "hudhide", "hudshow", "other", "", "", "" },
	{ "aspect", "cine", "srvref", "srvnone", "srvping", "srvlist", "printpop", "" },
	{ "abars", "submit", "push", "", "", "", "", "" }
};

static const char *r203_counterName[R203_FAM_COUNT][R203_MAX_COUNTERS] = {
	{ "disp", "upd", "" },
	{ "", "", "" },
	{ "srvrun", "srvn", "" },
	{ "armed", "wins", "submits" }
};

typedef char r203_assert_zones[(R203_VW_ZONES <= R203_MAX_ZONES
	&& R203_UIS_ZONES <= R203_MAX_ZONES && R203_UIT_ZONES <= R203_MAX_ZONES
	&& R203_SUB_ZONES <= R203_MAX_ZONES) ? 1 : -1];
typedef char r203_assert_counters[(R203_VW_COUNTERS <= R203_MAX_COUNTERS
	&& R203_UIT_COUNTERS <= R203_MAX_COUNTERS
	&& R203_SUB_COUNTERS <= R203_MAX_COUNTERS) ? 1 : -1];

static int	r203_frames;
static int	r203_windowStart;
static int	r203_windowArmed;
static int	r203_rowsEmitted[R203_FAM_COUNT];
static int	r203_zoneSum[R203_FAM_COUNT][R203_MAX_ZONES];
static int	r203_zoneMax[R203_FAM_COUNT][R203_MAX_ZONES];
static int	r203_cntSum[R203_FAM_COUNT][R203_MAX_COUNTERS];
static int	r203_cntMax[R203_FAM_COUNT][R203_MAX_COUNTERS];
static int	r203_totalSum[R203_FAM_COUNT];
static int	r203_totalMax[R203_FAM_COUNT];
static int	r203_frameNamed[R203_FAM_COUNT];
static int	r203_frameTotal[R203_FAM_COUNT];
static int	r203_frameZone[R203_FAM_COUNT][R203_MAX_ZONES];
static int	r203_frameCnt[R203_FAM_COUNT][R203_MAX_COUNTERS];

typedef struct r203_cls_s {
	const char	*key;			
	char		cls[24];
	int		dispSum, dispMax;	
	int		kidsSum, kidsMax;
	int		updSum, updMax;
	int		dispCalls, updCalls;
} r203_cls_t;

static r203_cls_t	r203_clsTab[R203_VW_CLASSES] = {
	{ "(other)", "(other)", 0, 0, 0, 0, 0, 0, 0, 0 }
};
static int		r203_frameClsDisp[R203_VW_CLASSES];
static int		r203_frameClsKids[R203_VW_CLASSES];
static int		r203_frameClsUpd[R203_VW_CLASSES];
static int		r203_clsUsed;		
static int		r203_clsOver;		

typedef struct r203_inst_s {
	const char	*clsKey;
	const char	*nameKey;
	char		cls[24];
	char		name[32];
	int		selfSum, selfMax;	
	int		kidsSum, kidsMax;	
	int		calls;
} r203_inst_t;

static r203_inst_t	r203_instTab[R203_VW_INSTS];
static int		r203_instUsed;
static int		r203_instDrop;
static int		r203_instNamed;		

typedef struct r203_walk_s {
	int		depth;
	int		frameT0;
	int		self[R203_MAX_DEPTH];
	int		childSum[R203_MAX_DEPTH];
	int		clsSlot[R203_MAX_DEPTH];
	int		instSlot[R203_MAX_DEPTH];
	int		topTotalTenths;		
} r203_walk_t;

static r203_walk_t	r203_dispWalk;
static r203_walk_t	r203_updWalk;

#define R203_STAMP(t)		((t) + 1)
#define R203_UNSTAMP(s)		((s) - 1)

static int		r203_stateT0;

static int		r203_subArmed;


static void r203_cpy(char *dst, int size, const char *src)
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

static void r203_app(char *dst, int size, const char *src)
{
	int l = (int)strlen(dst);
	int i = 0;

	while (l + i < size - 1 && src[i]) {
		dst[l + i] = src[i];
		i++;
	}

	dst[l + i] = '\0';
}

static void r203_zoneAdd(int family, int zone, int usec)
{
	int tenths;

	if (usec < 0) {
		usec = 0;
	}

	tenths = usec / 100;

	r203_frameZone[family][zone] += tenths;
	r203_frameNamed[family] += tenths;
}

static void r203_cntBump(int family, int counter, int n)
{
	r203_frameCnt[family][counter] += n;
}

static void r203_clsReserve(void)
{
	r203_clsTab[0].key = "(other)";
	r203_cpy(r203_clsTab[0].cls, (int)sizeof(r203_clsTab[0].cls), "(other)");
}

static int r203_clsFind(const char *classname)
{
	unsigned long h;
	int	probe, i, idx;

	if (!classname) {
		classname = "(null)";
	}

	h = (unsigned long)((uintptr_t)classname >> 4);
	probe = 1 + (int)(h % (unsigned)(R203_VW_CLASSES - 1));

	for (i = 0; i < R203_VW_CLASSES - 1; i++) {
		idx = 1 + (probe - 1 + i) % (R203_VW_CLASSES - 1);

		if (r203_clsTab[idx].key == classname) {
			return idx;
		}

		if (!r203_clsTab[idx].key) {
			r203_clsTab[idx].key = classname;
			r203_cpy(r203_clsTab[idx].cls, (int)sizeof(r203_clsTab[idx].cls), classname);
			r203_clsUsed++;
			return idx;
		}
	}

	r203_clsOver++;
	return 0;
}

static int r203_instFind(const char *classname, const char *name)
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

	for (i = 0; i < R203_VW_INSTS; i++) {
		idx = (int)((h + (unsigned long)i) % (unsigned) R203_VW_INSTS);

		if (r203_instTab[idx].clsKey == classname && r203_instTab[idx].nameKey == name) {
			return idx;
		}

		if (!r203_instTab[idx].clsKey) {
			r203_instTab[idx].clsKey  = classname;
			r203_instTab[idx].nameKey = name;
			r203_cpy(r203_instTab[idx].cls, (int)sizeof(r203_instTab[idx].cls), classname);
			r203_cpy(r203_instTab[idx].name, (int)sizeof(r203_instTab[idx].name), name);
			r203_instUsed++;
			return idx;
		}
	}

	r203_instDrop++;
	return -1;
}


static int r203_walkEnter(r203_walk_t *w)
{
	int t;

	if (!r202_statsOn) {
		return 0;
	}

	t = R202_Microseconds();

	if (w->depth >= R203_MAX_DEPTH) {
		return 0;
	}

	if (w->depth == 0) {
		w->frameT0 = t;
	}

	w->childSum[w->depth] = 0;
	w->clsSlot[w->depth]  = 0;
	w->instSlot[w->depth] = -1;
	w->depth++;

	return R203_STAMP(t);
}

static int r203_walkSelf(r203_walk_t *w, int t0, int zoneSelf, int counter,
						 const char *classname, const char *name, int isUpdate)
{
	int now, self, tenths, clsIdx, inst;

	if (!r202_statsOn || !t0) {
		return 0;
	}

	now  = R202_Microseconds();
	self = now - R203_UNSTAMP(t0);
	if (self < 0) {
		self = 0;
	}

	clsIdx = r203_clsFind(classname);
	inst   = isUpdate ? -1 : r203_instFind(classname, name);

	w->self[w->depth - 1]     = self;
	w->clsSlot[w->depth - 1]  = clsIdx;
	w->instSlot[w->depth - 1] = inst;

	r203_zoneAdd(R203_FAM_VW, zoneSelf, self);
	r203_cntBump(R203_FAM_VW, counter, 1);

	tenths = self / 100;

	if (isUpdate) {
		r203_frameClsUpd[clsIdx] += tenths;
		r203_clsTab[clsIdx].updCalls++;
	} else {
		r203_frameClsDisp[clsIdx] += tenths;
		r203_clsTab[clsIdx].dispCalls++;

		if (inst >= 0) {
			r203_instTab[inst].selfSum += tenths;
			if (tenths > r203_instTab[inst].selfMax) {
				r203_instTab[inst].selfMax = tenths;
			}
			r203_instTab[inst].calls++;
			r203_instNamed += tenths;
		}
	}

	return R203_STAMP(now);
}

static void r203_walkExit(r203_walk_t *w, int tStamp, int zoneLoop)
{
	int clsIdx, inst;
	int now, kids, loopoh, total, kt;

	if (!r202_statsOn || !tStamp) {
		return;
	}

	now  = R202_Microseconds();
	kids = now - R203_UNSTAMP(tStamp);
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

	r203_zoneAdd(R203_FAM_VW, zoneLoop, loopoh);

	kt = kids / 100;

	r203_frameClsKids[clsIdx] += kt;

	if (inst >= 0) {
		r203_instTab[inst].kidsSum += kt;
		if (kt > r203_instTab[inst].kidsMax) {
			r203_instTab[inst].kidsMax = kt;
		}
	}

	w->depth--;

	if (w->depth == 0) {
		w->topTotalTenths += total / 100;
	} else {
		w->childSum[w->depth - 1] += total;	
	}
}


static void r203_resetWindow(void)
{
	int family, i;

	r203_frames = 0;
	r203_windowStart = 0;
	r203_windowArmed = 0;

	for (family = 0; family < R203_FAM_COUNT; family++) {
		r203_totalSum[family] = r203_totalMax[family] = 0;
		r203_frameNamed[family] = r203_frameTotal[family] = 0;

		for (i = 0; i < R203_MAX_ZONES; i++) {
			r203_zoneSum[family][i] = r203_zoneMax[family][i] = 0;
			r203_frameZone[family][i] = 0;
		}
		for (i = 0; i < R203_MAX_COUNTERS; i++) {
			r203_cntSum[family][i] = r203_cntMax[family][i] = 0;
			r203_frameCnt[family][i] = 0;
		}
	}

	for (i = 0; i < R203_VW_CLASSES; i++) {
		r203_clsTab[i].key       = NULL;
		r203_clsTab[i].cls[0]    = '\0';
		r203_clsTab[i].dispSum   = r203_clsTab[i].dispMax = 0;
		r203_clsTab[i].kidsSum   = r203_clsTab[i].kidsMax = 0;
		r203_clsTab[i].updSum    = r203_clsTab[i].updMax  = 0;
		r203_clsTab[i].dispCalls = r203_clsTab[i].updCalls = 0;
		r203_frameClsDisp[i] = r203_frameClsKids[i] = r203_frameClsUpd[i] = 0;
	}

	r203_clsReserve();
	r203_clsUsed = 0;
	r203_clsOver = 0;

	for (i = 0; i < R203_VW_INSTS; i++) {
		r203_instTab[i].clsKey  = NULL;
		r203_instTab[i].nameKey = NULL;
		r203_instTab[i].cls[0]  = '\0';
		r203_instTab[i].name[0] = '\0';
		r203_instTab[i].selfSum = r203_instTab[i].selfMax = 0;
		r203_instTab[i].kidsSum = r203_instTab[i].kidsMax = 0;
		r203_instTab[i].calls   = 0;
	}

	r203_instUsed  = 0;
	r203_instDrop  = 0;
	r203_instNamed = 0;
	r203_subArmed  = 0;
}

static void r203_printFamily(int family, int window)
{
	char	line[R203_VW_LINE], part[256];
	int	frames = r203_frames < 1 ? 1 : r203_frames;
	int	i, mean10;

	line[0] = '\0';

	for (i = 0; i < r203_zoneCount[family]; i++) {
		int sum = r203_zoneSum[family][i];

		mean10 = (int)(((long long)sum + frames / 2) / frames);
		Com_sprintf(part, sizeof(part), " %s=%d.%d/%d.%d", r203_zoneName[family][i],
					mean10 / 10, mean10 % 10,
					r203_zoneMax[family][i] / 10, r203_zoneMax[family][i] % 10);
		r203_app(line, (int)sizeof(line), part);
	}

	for (i = 0; i < r203_counterCount[family]; i++) {
		int sum = r203_cntSum[family][i];

		mean10 = (int)(((long long)sum * 10 + frames / 2) / frames);
		Com_sprintf(part, sizeof(part), " %s=%d.%d/%d", r203_counterName[family][i],
					mean10 / 10, mean10 % 10, r203_cntMax[family][i]);
		r203_app(line, (int)sizeof(line), part);
	}

	if (r203_hasTotal[family]) {
		int sum = r203_totalSum[family];

		mean10 = (int)(((long long)sum + frames / 2) / frames);
		Com_sprintf(part, sizeof(part), " total=%d.%d/%d.%d",
					mean10 / 10, mean10 % 10,
					r203_totalMax[family] / 10, r203_totalMax[family] % 10);
		r203_app(line, (int)sizeof(line), part);
	}

	if (family == R203_FAM_VW) {
		int used[R203_VW_TOP];
		int k, j, pick;
		int dispSelfSum = r203_zoneSum[R203_FAM_VW][R203_VWZ_dispself];
		int clsSum = 0;

		for (i = 0; i < R203_VW_CLASSES; i++) {
			clsSum += r203_clsTab[i].dispSum;
		}

		Com_sprintf(part, sizeof(part), " unacc=%d.%d unname=%d.%d cls=%d/%d inst=%d/%d drop=%d over=%d",
					(dispSelfSum - clsSum) / 10, (dispSelfSum - clsSum) % 10,
					(dispSelfSum - r203_instNamed) / 10, (dispSelfSum - r203_instNamed) % 10,
					r203_clsUsed, R203_VW_CLASSES,
					r203_instUsed, R203_VW_INSTS, r203_instDrop, r203_clsOver);
		r203_app(line, (int)sizeof(line), part);

		for (k = 0; k < R203_VW_TOP; k++) {
			used[k] = -1;
		}

		for (k = 0; k < R203_VW_TOP; k++) {
			pick = -1;

			for (i = 0; i < R203_VW_CLASSES; i++) {
				int taken = 0;

				for (j = 0; j < k; j++) {
					if (used[j] == i) {
						taken = 1;
						break;
					}
				}

				if (taken || r203_clsTab[i].dispSum <= 0) {
					continue;
				}

				if (pick < 0 || r203_clsTab[i].dispSum > r203_clsTab[pick].dispSum) {
					pick = i;
				}
			}

			if (pick < 0) {
				break;
			}

			used[k] = pick;

			mean10 = (int)(((long long)r203_clsTab[pick].dispSum + frames / 2) / frames);
			Com_sprintf(part, sizeof(part),
						" | cls '%s' self=%d.%d/%d.%d kids=%d.%d/%d.%d upd=%d.%d n=%d",
						r203_clsTab[pick].cls,
						mean10 / 10, mean10 % 10,
						r203_clsTab[pick].dispMax / 10, r203_clsTab[pick].dispMax % 10,
						r203_clsTab[pick].kidsSum / 10, r203_clsTab[pick].kidsSum % 10,
						r203_clsTab[pick].kidsMax / 10, r203_clsTab[pick].kidsMax % 10,
						r203_clsTab[pick].updSum / 10, r203_clsTab[pick].updSum % 10,
						r203_clsTab[pick].dispCalls);
			r203_app(line, (int)sizeof(line), part);
		}

		for (k = 0; k < R203_VW_TOP; k++) {
			used[k] = -1;
		}

		for (k = 0; k < R203_VW_TOP; k++) {
			pick = -1;

			for (i = 0; i < R203_VW_INSTS; i++) {
				int taken = 0;

				for (j = 0; j < k; j++) {
					if (used[j] == i) {
						taken = 1;
						break;
					}
				}

				if (taken || r203_instTab[i].selfSum <= 0) {
					continue;
				}

				if (pick < 0 || r203_instTab[i].selfSum > r203_instTab[pick].selfSum) {
					pick = i;
				}
			}

			if (pick < 0) {
				break;
			}

			used[k] = pick;

			mean10 = (int)(((long long)r203_instTab[pick].selfSum + frames / 2) / frames);
			Com_sprintf(part, sizeof(part),
						" | inst '%s'/'%s' self=%d.%d/%d.%d kids=%d.%d/%d.%d n=%d",
						r203_instTab[pick].cls, r203_instTab[pick].name,
						mean10 / 10, mean10 % 10,
						r203_instTab[pick].selfMax / 10, r203_instTab[pick].selfMax % 10,
						r203_instTab[pick].kidsSum / 10, r203_instTab[pick].kidsSum % 10,
						r203_instTab[pick].kidsMax / 10, r203_instTab[pick].kidsMax % 10,
						r203_instTab[pick].calls);
			r203_app(line, (int)sizeof(line), part);
		}
	}

	Com_Printf("[R202:%s] frames=%d window=%dms%s\n", r203_famName[family], frames, window, line);

	r203_rowsEmitted[family]++;
}


int R203_VW_Enter(void)
{
	return r203_walkEnter(&r203_dispWalk);
}

int R203_VW_Self(int t0, const char *classname, const char *name)
{
	if (!r202_statsOn || !t0) {
		return 0;
	}

	return r203_walkSelf(&r203_dispWalk, t0, R203_VWZ_dispself, R203_VWC_disp,
						 classname, name, 0);
}

void R203_VW_Exit(int tStamp)
{
	r203_walkExit(&r203_dispWalk, tStamp, R203_VWZ_disploop);
}

int R203_VW_UpdEnter(void)
{
	return r203_walkEnter(&r203_updWalk);
}

int R203_VW_UpdSelf(int t0, const char *classname, const char *name)
{
	if (!r202_statsOn || !t0) {
		return 0;
	}

	return r203_walkSelf(&r203_updWalk, t0, R203_VWZ_updself, R203_VWC_upd,
						 classname, name, 1);
}

void R203_VW_UpdExit(int tStamp)
{
	r203_walkExit(&r203_updWalk, tStamp, R203_VWZ_updloop);
}

int R203_Leg(void)
{
	if (!r202_statsOn) {
		return 0;
	}

	return R203_STAMP(R202_Microseconds());
}

void R203_LegEnd(int family, int zone, int t0)
{
	int now, usec;

	if (!r202_statsOn || !t0) {
		return;
	}

	now  = R202_Microseconds();
	usec = now - R203_UNSTAMP(t0);
	if (usec < 0) {
		usec = 0;
	}

	r203_zoneAdd(family, zone, usec);
}

void R203_Count(int family, int counter, int n)
{
	if (!r202_statsOn || counter < 0 || counter >= r203_counterCount[family]) {
		return;
	}

	r203_cntBump(family, counter, n);
}

void R203_StateBegin(void)
{
	if (!r202_statsOn) {
		r203_stateT0 = 0;
		return;
	}

	r203_stateT0 = R203_STAMP(R202_Microseconds());
}

void R203_StateEnd(void)
{
	int now, usec;

	if (!r202_statsOn || !r203_stateT0) {
		r203_stateT0 = 0;
		return;
	}

	now  = R202_Microseconds();
	usec = now - R203_UNSTAMP(r203_stateT0);
	if (usec < 0) {
		usec = 0;
	}

	r203_frameTotal[R203_FAM_UIS] += usec / 100;
	r203_stateT0 = 0;
}


void R203_SubArm(void)
{
	if (!r202_statsOn) {
		r203_subArmed = 0;
		return;
	}

	r203_subArmed = 1;
	r203_cntBump(R203_FAM_SUB, R203_SUBC_armed, 1);
}

int R203_SubT0(void)
{
	if (!r202_statsOn) {
		return 0;
	}

	return R203_STAMP(R202_Microseconds());
}

void R203_SubPushEnd(int t0)
{
	int now, usec, zone;

	if (!r202_statsOn || !t0) {
		return;
	}

	now  = R202_Microseconds();
	usec = now - R203_UNSTAMP(t0);
	if (usec < 0) {
		usec = 0;
	}

	if (r203_subArmed) {
		r203_subArmed = 0;
		zone = R203_SUBZ_submit;
		r203_cntBump(R203_FAM_SUB, R203_SUBC_submits, 1);
	} else {
		zone = R203_SUBZ_push;
	}

	r203_cntBump(R203_FAM_SUB, R203_SUBC_wins, 1);
	r203_zoneAdd(R203_FAM_SUB, zone, usec);
}

void R203_SubZoneEnd(int zone, int t0)
{
	if (!r202_statsOn || !t0) {
		return;
	}

	r203_zoneAdd(R203_FAM_SUB, zone, R202_Microseconds() - R203_UNSTAMP(t0));
}

void R203_UiFrameEnd(void)
{
	int family, i, window;

	if (!r202_statsOn) {
		r203_windowStart = 0;
		r203_windowArmed = 0;
		r203_frames = 0;
		r203_subArmed = 0;
		return;
	}

	r203_frameTotal[R203_FAM_VW] += r203_dispWalk.topTotalTenths + r203_updWalk.topTotalTenths;
	r203_dispWalk.topTotalTenths = 0;
	r203_updWalk.topTotalTenths = 0;

	for (i = 0; i < R203_VW_CLASSES; i++) {
		int v;

		v = r203_frameClsDisp[i];
		r203_clsTab[i].dispSum += v;
		if (v > r203_clsTab[i].dispMax) {
			r203_clsTab[i].dispMax = v;
		}
		r203_frameClsDisp[i] = 0;

		v = r203_frameClsKids[i];
		r203_clsTab[i].kidsSum += v;
		if (v > r203_clsTab[i].kidsMax) {
			r203_clsTab[i].kidsMax = v;
		}
		r203_frameClsKids[i] = 0;

		v = r203_frameClsUpd[i];
		r203_clsTab[i].updSum += v;
		if (v > r203_clsTab[i].updMax) {
			r203_clsTab[i].updMax = v;
		}
		r203_frameClsUpd[i] = 0;
	}

	for (family = 0; family < R203_FAM_COUNT; family++) {
		if (r203_hasTotal[family]) {
			int other = r203_frameTotal[family] - r203_frameNamed[family];
			int last  = r203_zoneCount[family] - 1;

			if (other < 0) {
				other = 0;
			}

			r203_frameZone[family][last] += other;

			r203_totalSum[family] += r203_frameTotal[family];
			if (r203_frameTotal[family] > r203_totalMax[family]) {
				r203_totalMax[family] = r203_frameTotal[family];
			}
		}

		for (i = 0; i < r203_zoneCount[family]; i++) {
			int v = r203_frameZone[family][i];

			r203_zoneSum[family][i] += v;
			if (v > r203_zoneMax[family][i]) {
				r203_zoneMax[family][i] = v;
			}
			r203_frameZone[family][i] = 0;
		}

		for (i = 0; i < r203_counterCount[family]; i++) {
			int v = r203_frameCnt[family][i];

			r203_cntSum[family][i] += v;
			if (v > r203_cntMax[family][i]) {
				r203_cntMax[family][i] = v;
			}
			r203_frameCnt[family][i] = 0;
		}

		r203_frameNamed[family] = 0;
		r203_frameTotal[family] = 0;
	}

	r203_frames++;

	if (!r203_windowArmed) {
		r203_windowStart = Sys_Milliseconds();
		r203_windowArmed = 1;
	}

	window = Sys_Milliseconds() - r203_windowStart;

	if (window < R203_WINDOW_MS && r203_frames < R203_MAX_FRAMES) {
		return;
	}

	for (family = 0; family < R203_FAM_COUNT; family++) {
		if (r203_rowsEmitted[family] < R203_MAX_ROWS) {
			r203_printFamily(family, window);
		} else if (r203_rowsEmitted[family] == R203_MAX_ROWS) {
			r203_rowsEmitted[family]++;
			Com_Printf("[R202:%s] capped at %d rows; no further lines\n",
					   r203_famName[family], R203_MAX_ROWS);
		}
	}

	r203_resetWindow();
	r203_windowStart = Sys_Milliseconds();
	r203_windowArmed = 1;
}

