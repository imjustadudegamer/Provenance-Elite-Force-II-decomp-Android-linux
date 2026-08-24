/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction

#ifndef __LIMIT_UIPROBE_H__
#define __LIMIT_UIPROBE_H__

typedef enum {
	LIMIT_FAM_VW = 0,	
	LIMIT_FAM_UIS,		
	LIMIT_FAM_UIT,		
	LIMIT_FAM_SUB,		
	LIMIT_FAM_COUNT
} limit_family_t;

typedef enum {
	LIMIT_VWZ_dispself = 0, LIMIT_VWZ_disploop, LIMIT_VWZ_updself, LIMIT_VWZ_updloop,
	LIMIT_VWZ_other, LIMIT_VW_ZONES
} limit_vw_zone_t;

typedef enum {
	LIMIT_VWC_disp = 0, LIMIT_VWC_upd,
	LIMIT_VW_COUNTERS
} limit_vw_counter_t;

typedef enum {
	LIMIT_UISZ_nomenu = 0, LIMIT_UISZ_machine, LIMIT_UISZ_hudhide, LIMIT_UISZ_hudshow,
	LIMIT_UISZ_other, LIMIT_UIS_ZONES
} limit_uis_zone_t;

typedef enum {
	LIMIT_UITZ_aspect = 0, LIMIT_UITZ_cine, LIMIT_UITZ_srvref,
	LIMIT_UITZ_srvnone, LIMIT_UITZ_srvping, LIMIT_UITZ_srvlist,
	LIMIT_UITZ_printpop, LIMIT_UIT_ZONES
} limit_uit_zone_t;

typedef enum {
	LIMIT_UTC_srvrun = 0, LIMIT_UTC_srvn,
	LIMIT_UIT_COUNTERS
} limit_uit_counter_t;

typedef enum {
	LIMIT_SUBZ_abars = 0, LIMIT_SUBZ_submit, LIMIT_SUBZ_push,
	LIMIT_SUB_ZONES
} limit_sub_zone_t;

typedef enum {
	LIMIT_SUBC_armed = 0, LIMIT_SUBC_wins, LIMIT_SUBC_submits,
	LIMIT_SUB_COUNTERS
} limit_sub_counter_t;

#ifdef __cplusplus
extern "C" {
#endif

int		LIMIT_VW_Enter( void );
int		LIMIT_VW_Self( int t0, const char *cls, const char *name );
void	LIMIT_VW_Exit( int tStamp );

int		LIMIT_VW_UpdEnter( void );
int		LIMIT_VW_UpdSelf( int t0, const char *cls, const char *name );
void	LIMIT_VW_UpdExit( int tStamp );

int		LIMIT_Leg( void );
void	LIMIT_LegEnd( int family, int zone, int t0 );
void	LIMIT_Count( int family, int counter, int n );
void	LIMIT_StateBegin( void );
void	LIMIT_StateEnd( void );

void	LIMIT_SubArm( void );
int		LIMIT_SubT0( void );
void	LIMIT_SubPushEnd( int t0 );
void	LIMIT_SubZoneEnd( int zone, int t0 );

void	LIMIT_UiFrameEnd( void );

#ifdef __cplusplus
}
#endif

#endif 
