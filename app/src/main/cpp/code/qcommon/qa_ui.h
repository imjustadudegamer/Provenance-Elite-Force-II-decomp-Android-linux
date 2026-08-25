
#ifndef __QA_UI_H__
#define __QA_UI_H__

typedef enum {
	UI_FAM_UI = 0,
	UI_FAM_DISP,
	UI_FAM_V3D,
	UI_FAM_WALK,
	UI_FAM_SPIN,
	UI_FAM_COUNT
} ui_family_t;

typedef enum {
	UI_UIZ_setrt = 0, UI_UIZ_filluid, UI_UIZ_service, UI_UIZ_consoles,
	UI_UIZ_state, UI_UIZ_menus, UI_UIZ_views, UI_UIZ_other,
	UI_UI_ZONES
} ui_ui_zone_t;

typedef enum {
	UI_DZ_walkup = 0, UI_DZ_disp, UI_DZ_cursor, UI_DZ_printstr,
	UI_DZ_other, UI_DISP_ZONES
} ui_disp_zone_t;

typedef enum {
	UI_VZ_scrdraw = 0, UI_VZ_set2d, UI_VZ_draw2d, UI_VZ_cgdraw,
	UI_VZ_other, UI_V3D_ZONES
} ui_v3d_zone_t;

typedef enum {
	UI_WZ_vis = 0, UI_WZ_hid, UI_WZ_other, UI_WALK_ZONES
} ui_walk_zone_t;

typedef enum {
	UI_SPZ_cfg = 0, UI_SPZ_vlog, UI_SPZ_fnum, UI_SPZ_evloop,
	UI_SPZ_other, UI_SPIN_ZONES
} ui_spin_zone_t;

typedef enum {
	UI_UC_upd = 0, UI_UC_disp, UI_UC_draw, UI_UC_text,
	UI_UI_COUNTERS
} ui_ui_counter_t;

typedef enum {
	UI_DC_children = 0, UI_DC_disps, UI_DISP_COUNTERS
} ui_disp_counter_t;

typedef enum {
	UI_WC_menus = 0, UI_WC_visw, UI_WC_hidw, UI_WALK_COUNTERS
} ui_walk_counter_t;

typedef enum {
	UI_SC_iters = 0, UI_SC_limited, UI_SC_min, UI_SPIN_COUNTERS
} ui_spin_counter_t;

#ifdef __cplusplus
extern "C" {
#endif

int			UI_Microseconds(void);
void		UI_On(void);
void		UI_RootBegin(int family);
void		UI_RootEnd(int family);
void		UI_Frame(int family, int totalUsec);
void		UI_Add(int family, int zone, int usec);
void		UI_Bump(int family, int counter, int n);

extern int	ui_statsOn;

#ifdef __cplusplus
}
#endif

#define UI_ROOT_BEGIN(fam)	UI_RootBegin( (fam) )
#define UI_ROOT_END(fam)		UI_RootEnd( (fam) )

#define UI_ZONE_BEGIN(z) \
	int ui_zt_##z = ( ui_statsOn ? UI_Microseconds() : 0 )
#define UI_ZONE_END(fam, z) \
	do { if ( ui_statsOn ) { UI_Add( (fam), (z), UI_Microseconds() - ui_zt_##z ); } } while (0)

#define UI_ZONE(fam, z, stmt) \
	do { \
		int ui_zoneT0 = ( ui_statsOn ? UI_Microseconds() : 0 ); \
		stmt; \
		if ( ui_statsOn ) { UI_Add( (fam), (z), UI_Microseconds() - ui_zoneT0 ); } \
	} while (0)

#define UI_COUNT(fam, c, n) \
	do { if ( ui_statsOn ) { UI_Bump( (fam), (c), (n) ); } } while (0)

#endif 
