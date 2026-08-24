
#ifndef __QA_R202_H__
#define __QA_R202_H__

typedef enum {
	R202_FAM_UI = 0,
	R202_FAM_DISP,
	R202_FAM_V3D,
	R202_FAM_WALK,
	R202_FAM_SPIN,
	R202_FAM_COUNT
} r202_family_t;

typedef enum {
	R202_UIZ_setrt = 0, R202_UIZ_filluid, R202_UIZ_service, R202_UIZ_consoles,
	R202_UIZ_state, R202_UIZ_menus, R202_UIZ_views, R202_UIZ_other,
	R202_UI_ZONES
} r202_ui_zone_t;

typedef enum {
	R202_DZ_walkup = 0, R202_DZ_disp, R202_DZ_cursor, R202_DZ_printstr,
	R202_DZ_other, R202_DISP_ZONES
} r202_disp_zone_t;

typedef enum {
	R202_VZ_scrdraw = 0, R202_VZ_set2d, R202_VZ_draw2d, R202_VZ_cgdraw,
	R202_VZ_other, R202_V3D_ZONES
} r202_v3d_zone_t;

typedef enum {
	R202_WZ_vis = 0, R202_WZ_hid, R202_WZ_other, R202_WALK_ZONES
} r202_walk_zone_t;

typedef enum {
	R202_SPZ_cfg = 0, R202_SPZ_vlog, R202_SPZ_fnum, R202_SPZ_evloop,
	R202_SPZ_other, R202_SPIN_ZONES
} r202_spin_zone_t;

typedef enum {
	R202_UC_upd = 0, R202_UC_disp, R202_UC_draw, R202_UC_text,
	R202_UI_COUNTERS
} r202_ui_counter_t;

typedef enum {
	R202_DC_children = 0, R202_DC_disps, R202_DISP_COUNTERS
} r202_disp_counter_t;

typedef enum {
	R202_WC_menus = 0, R202_WC_visw, R202_WC_hidw, R202_WALK_COUNTERS
} r202_walk_counter_t;

typedef enum {
	R202_SC_iters = 0, R202_SC_limited, R202_SC_min, R202_SPIN_COUNTERS
} r202_spin_counter_t;

#ifdef __cplusplus
extern "C" {
#endif

int			R202_Microseconds(void);
void		R202_On(void);
void		R202_RootBegin(int family);
void		R202_RootEnd(int family);
void		R202_Frame(int family, int totalUsec);
void		R202_Add(int family, int zone, int usec);
void		R202_Bump(int family, int counter, int n);

extern int	r202_statsOn;

#ifdef __cplusplus
}
#endif

#define R202_ROOT_BEGIN(fam)	R202_RootBegin( (fam) )
#define R202_ROOT_END(fam)		R202_RootEnd( (fam) )

#define R202_ZONE_BEGIN(z) \
	int r202_zt_##z = ( r202_statsOn ? R202_Microseconds() : 0 )
#define R202_ZONE_END(fam, z) \
	do { if ( r202_statsOn ) { R202_Add( (fam), (z), R202_Microseconds() - r202_zt_##z ); } } while (0)

#define R202_ZONE(fam, z, stmt) \
	do { \
		int r202_zoneT0 = ( r202_statsOn ? R202_Microseconds() : 0 ); \
		stmt; \
		if ( r202_statsOn ) { R202_Add( (fam), (z), R202_Microseconds() - r202_zoneT0 ); } \
	} while (0)

#define R202_COUNT(fam, c, n) \
	do { if ( r202_statsOn ) { R202_Bump( (fam), (c), (n) ); } } while (0)

#endif 
