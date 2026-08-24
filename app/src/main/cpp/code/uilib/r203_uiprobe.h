
#ifndef __R203_UIPROBE_H__
#define __R203_UIPROBE_H__

typedef enum {
	R203_FAM_VW = 0,	
	R203_FAM_UIS,		
	R203_FAM_UIT,		
	R203_FAM_SUB,		
	R203_FAM_COUNT
} r203_family_t;

typedef enum {
	R203_VWZ_dispself = 0, R203_VWZ_disploop, R203_VWZ_updself, R203_VWZ_updloop,
	R203_VWZ_other, R203_VW_ZONES
} r203_vw_zone_t;

typedef enum {
	R203_VWC_disp = 0, R203_VWC_upd,
	R203_VW_COUNTERS
} r203_vw_counter_t;

typedef enum {
	R203_UISZ_nomenu = 0, R203_UISZ_machine, R203_UISZ_hudhide, R203_UISZ_hudshow,
	R203_UISZ_other, R203_UIS_ZONES
} r203_uis_zone_t;

typedef enum {
	R203_UITZ_aspect = 0, R203_UITZ_cine, R203_UITZ_srvref,
	R203_UITZ_srvnone, R203_UITZ_srvping, R203_UITZ_srvlist,
	R203_UITZ_printpop, R203_UIT_ZONES
} r203_uit_zone_t;

typedef enum {
	R203_UTC_srvrun = 0, R203_UTC_srvn,
	R203_UIT_COUNTERS
} r203_uit_counter_t;

typedef enum {
	R203_SUBZ_abars = 0, R203_SUBZ_submit, R203_SUBZ_push,
	R203_SUB_ZONES
} r203_sub_zone_t;

typedef enum {
	R203_SUBC_armed = 0, R203_SUBC_wins, R203_SUBC_submits,
	R203_SUB_COUNTERS
} r203_sub_counter_t;

#ifdef __cplusplus
extern "C" {
#endif

int		R203_VW_Enter( void );
int		R203_VW_Self( int t0, const char *cls, const char *name );
void	R203_VW_Exit( int tStamp );

int		R203_VW_UpdEnter( void );
int		R203_VW_UpdSelf( int t0, const char *cls, const char *name );
void	R203_VW_UpdExit( int tStamp );

int		R203_Leg( void );
void	R203_LegEnd( int family, int zone, int t0 );
void	R203_Count( int family, int counter, int n );
void	R203_StateBegin( void );
void	R203_StateEnd( void );

void	R203_SubArm( void );
int		R203_SubT0( void );
void	R203_SubPushEnd( int t0 );
void	R203_SubZoneEnd( int zone, int t0 );

void	R203_UiFrameEnd( void );

#ifdef __cplusplus
}
#endif

#endif 
