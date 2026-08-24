//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __BOTLIB_H__
#define __BOTLIB_H__

#define	BOTLIB_API_VERSION		2

struct aas_clientmove_s;
struct aas_entityinfo_s;
struct aas_areainfo_s;
struct aas_altroutegoal_s;
struct aas_predictroute_s;
struct bot_consolemessage_s;
struct bot_match_s;
struct bot_goal_s;
struct bot_moveresult_s;
struct bot_initmove_s;
struct weaponinfo_s;

#define BOTFILESBASEFOLDER		"botfiles"
#define LINECOLOR_NONE			-1
#define LINECOLOR_RED			1 
#define LINECOLOR_GREEN			2 
#define LINECOLOR_BLUE			3 
#define LINECOLOR_YELLOW		4 
#define LINECOLOR_ORANGE		5 

#define PRT_MESSAGE				1
#define PRT_WARNING				2
#define PRT_ERROR				3
#define PRT_FATAL				4
#define PRT_EXIT				5

#define CMS_NORMAL				0
#define CMS_CHAT				1

#define BLERR_NOERROR					0	
#define BLERR_LIBRARYNOTSETUP			1	
#define BLERR_INVALIDENTITYNUMBER		2	
#define BLERR_NOAASFILE					3	
#define BLERR_CANNOTOPENAASFILE			4	
#define BLERR_WRONGAASFILEID			5	
#define BLERR_WRONGAASFILEVERSION		6	
#define BLERR_CANNOTREADAASLUMP			7	 
#define BLERR_CANNOTLOADICHAT			8	
#define BLERR_CANNOTLOADITEMWEIGHTS		9	
#define BLERR_CANNOTLOADITEMCONFIG		10	
#define BLERR_CANNOTLOADWEAPONWEIGHTS	11	
#define BLERR_CANNOTLOADWEAPONCONFIG	12	

#define ACTION_ATTACK			0x0000001
#define ACTION_ATTACKRIGHT		0x0000002
#define ACTION_USE				0x0000004
#define ACTION_RESPAWN			0x0000008
#define ACTION_JUMP				0x0000010
#define ACTION_MOVEUP			0x0000020
#define ACTION_CROUCH			0x0000080
#define ACTION_MOVEDOWN			0x0000100
#define ACTION_MOVEFORWARD		0x0000200
#define ACTION_MOVEBACK			0x0000800
#define ACTION_MOVELEFT			0x0001000
#define ACTION_MOVERIGHT		0x0002000
#define ACTION_DELAYEDJUMP		0x0008000
#define ACTION_TALK				0x0010000
#define ACTION_GESTURE			0x0020000
#define ACTION_WALK				0x0080000
#define ACTION_AFFIRMATIVE		0x0100000
#define ACTION_NEGATIVE			0x0200000
#define ACTION_GETFLAG			0x0800000
#define ACTION_GUARDBASE		0x1000000
#define ACTION_PATROL			0x2000000
#define ACTION_FOLLOWME			0x8000000

typedef struct bot_input_s
{
	float thinktime;		
	vec3_t dir;				
	float speed;			
	vec3_t viewangles;		
	int actionflags;		
	int weapon,latchweapon;				
	int firestate;			
} bot_input_t;

#ifndef BSPTRACE

#define BSPTRACE

typedef struct bsp_surface_s
{
	char name[16];
	int flags;
	int value;
} bsp_surface_t;

typedef struct bsp_trace_s
{
	qboolean		allsolid;	
	qboolean		startsolid;	
	float			fraction;	
	vec3_t			endpos;		
	cplane_t		plane;		
	float			exp_dist;	
	int				sidenum;	
	bsp_surface_t	surface;	
	int				contents;	
	int				ent;		
} bsp_trace_t;

#endif	

typedef struct bot_entitystate_s
{
	int		type;			
	int		flags;			
	vec3_t	origin;			
	vec3_t	angles;			
	vec3_t	old_origin;		
	vec3_t	mins;			
	vec3_t	maxs;			
	int		groundent;		
	int		solid;			
	int		modelindex;		
	int		modelindex2;	
	int		frame;			
	int		event;			
	int		eventParm;		
	int		powerups;		
	int		weapon;			
	int		legsAnim;		
	int		torsoAnim;		
} bot_entitystate_t;

typedef struct botlib_import_s
{
	void		(QDECL *Print)(int type, char *fmt, ...) __attribute__((format(printf, 2, 3)));
	void		(*Trace)(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
	void		(*EntityTrace)(bsp_trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask);
	int			(*PointContents)(vec3_t point);
	int			(*inPVS)(vec3_t p1, vec3_t p2);
	char		*(*BSPEntityData)(void);
	void		(*BSPModelMinsMaxsOrigin)(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin);
	void		(*BotClientCommand)(int client, const char *command); 
#if !defined(__clang__)
	void		*(*GetMemory)(int size) __attribute__((returns_nonnull));		
#else
	void		*(*GetMemory)(int size);										
#endif
	void		(*FreeMemory)(void *ptr);		
	int			(*AvailableMemory)(void);		
	void		*(*HunkAlloc)(int size);		
	int			(*FS_FOpenFile)( const char *qpath, fileHandle_t *file, fsMode_t mode );
	int			(*FS_Read)( void *buffer, int len, fileHandle_t f );
	int			(*FS_Write)( const void *buffer, int len, fileHandle_t f );
	void		(*FS_FCloseFile)( fileHandle_t f );
	int			(*FS_Seek)( fileHandle_t f, long offset, int origin );
	int			(*DebugLineCreate)(void);
	void		(*DebugLineDelete)(int line);
	void		(*DebugLineShow)(int line, vec3_t start, vec3_t end, int color);
	int			(*DebugPolygonCreate)(int color, int numPoints, vec3_t *points);
	void		(*DebugPolygonDelete)(int id);
} botlib_import_t;

typedef struct aas_export_s
{
	void		(*AAS_EntityInfo)(int entnum, struct aas_entityinfo_s *info);
	int			(*AAS_Initialized)(void);
	void		(*AAS_PresenceTypeBoundingBox)(int presencetype, vec3_t mins, vec3_t maxs);
	float		(*AAS_Time)(void);
	int			(*AAS_PointAreaNum)(vec3_t point);
	int			(*AAS_PointReachabilityAreaIndex)( vec3_t point );
	int			(*AAS_TraceAreas)(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas);
	int			(*AAS_BBoxAreas)(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
	int			(*AAS_AreaInfo)( int areanum, struct aas_areainfo_s *info );
	int			(*AAS_PointContents)(vec3_t point);
	int			(*AAS_NextBSPEntity)(int ent);
	int			(*AAS_ValueForBSPEpairKey)(int ent, char *key, char *value, int size);
	int			(*AAS_VectorForBSPEpairKey)(int ent, char *key, vec3_t v);
	int			(*AAS_FloatForBSPEpairKey)(int ent, char *key, float *value);
	int			(*AAS_IntForBSPEpairKey)(int ent, char *key, int *value);
	int			(*AAS_AreaReachability)(int areanum);
	int			(*AAS_AreaTravelTimeToGoalArea)(int areanum, vec3_t origin, int goalareanum, int travelflags);
	int			(*AAS_EnableRoutingArea)(int areanum, int enable);
	int			(*AAS_PredictRoute)(struct aas_predictroute_s *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum);
	int			(*AAS_AlternativeRouteGoals)(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
										struct aas_altroutegoal_s *altroutegoals, int maxaltroutegoals,
										int type);
	int			(*AAS_Swimming)(vec3_t origin);
	int			(*AAS_PredictClientMovement)(struct aas_clientmove_s *move,
											int entnum, vec3_t origin,
											int presencetype, int onground,
											vec3_t velocity, vec3_t cmdmove,
											int cmdframes,
											int maxframes, float frametime,
											int stopevent, int stopareanum, int visualize);
} aas_export_t;

typedef struct ea_export_s
{
	void	(*EA_Command)(int client, const char *command );
	void	(*EA_Say)(int client, char *str);
	void	(*EA_SayTeam)(int client, char *str);
	void	(*EA_Action)(int client, int action);
	void	(*EA_Gesture)(int client);
	void	(*EA_Talk)(int client);
	void	(*EA_ToggleFireState)(int client);
	void	(*EA_Attack)(int client, int primarydangerous, int altdangerous);
	void	(*EA_Use)(int client);
	void	(*EA_Respawn)(int client);
	void	(*EA_MoveUp)(int client);
	void	(*EA_MoveDown)(int client);
	void	(*EA_MoveForward)(int client);
	void	(*EA_MoveBack)(int client);
	void	(*EA_MoveLeft)(int client);
	void	(*EA_MoveRight)(int client);
	void	(*EA_Crouch)(int client);

	void	(*EA_SelectWeapon)(int client, int weapon);
	void	(*EA_Jump)(int client);
	void	(*EA_DelayedJump)(int client);
	void	(*EA_Move)(int client, vec3_t dir, float speed);
	void	(*EA_View)(int client, vec3_t viewangles);
	void	(*EA_EndRegular)(int client, float thinktime);
	void	(*EA_GetInput)(int client, float thinktime, bot_input_t *input);
	void	(*EA_ResetInput)(int client);
} ea_export_t;

typedef struct ai_export_s
{
	int		(*BotLoadCharacter)(char *charfile, float skill);
	void	(*BotFreeCharacter)(int character);
	float	(*Characteristic_Float)(int character, int index);
	float	(*Characteristic_BFloat)(int character, int index, float min, float max);
	int		(*Characteristic_Integer)(int character, int index);
	int		(*Characteristic_BInteger)(int character, int index, int min, int max);
	void	(*Characteristic_String)(int character, int index, char *buf, int size);
	int		(*BotAllocChatState)(void);
	void	(*BotFreeChatState)(int handle);
	void	(*BotQueueConsoleMessage)(int chatstate, int type, char *message);
	void	(*BotRemoveConsoleMessage)(int chatstate, int handle);
	int		(*BotNextConsoleMessage)(int chatstate, struct bot_consolemessage_s *cm);
	int		(*BotNumConsoleMessages)(int chatstate);
	void	(*BotInitialChat)(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
	int		(*BotNumInitialChats)(int chatstate, char *type);
	int		(*BotReplyChat)(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
	int		(*BotChatLength)(int chatstate);
	void	(*BotEnterChat)(int chatstate, int client, int sendto);
	void	(*BotGetChatMessage)(int chatstate, char *buf, int size);
	int		(*StringContains)(char *str1, char *str2, int casesensitive);
	int		(*BotFindMatch)(char *str, struct bot_match_s *match, unsigned long int context);
	void	(*BotMatchVariable)(struct bot_match_s *match, int variable, char *buf, int size);
	void	(*UnifyWhiteSpaces)(char *string);
	void	(*BotReplaceSynonyms)(char *string, unsigned long int context);
	int		(*BotLoadChatFile)(int chatstate, char *chatfile, char *chatname);
	void	(*BotSetChatGender)(int chatstate, int gender);
	void	(*BotSetChatName)(int chatstate, char *name, int client);
	void	(*BotResetGoalState)(int goalstate);
	void	(*BotResetAvoidGoals)(int goalstate);
	void	(*BotRemoveFromAvoidGoals)(int goalstate, int number);
	void	(*BotPushGoal)(int goalstate, struct bot_goal_s *goal);
	void	(*BotPopGoal)(int goalstate);
	void	(*BotEmptyGoalStack)(int goalstate);
	void	(*BotDumpAvoidGoals)(int goalstate);
	void	(*BotDumpGoalStack)(int goalstate);
	void	(*BotGoalName)(int number, char *name, int size);
	int		(*BotGetTopGoal)(int goalstate, struct bot_goal_s *goal);
	int		(*BotGetSecondGoal)(int goalstate, struct bot_goal_s *goal);
	int		(*BotChooseLTGItem)(int goalstate, vec3_t origin, int *inventory, int travelflags);
	int		(*BotChooseNBGItem)(int goalstate, vec3_t origin, int *inventory, int travelflags,
								struct bot_goal_s *ltg, float maxtime);
	int		(*BotTouchingGoal)(vec3_t origin, struct bot_goal_s *goal);
	int		(*BotItemGoalInVisButNotVisible)(int viewer, vec3_t eye, vec3_t viewangles, struct bot_goal_s *goal);
	int		(*BotGetLevelItemGoal)(int index, char *classname, struct bot_goal_s *goal);
	int		(*BotGetNextCampSpotGoal)(int num, struct bot_goal_s *goal);
	int		(*BotGetMapLocationGoal)(char *name, struct bot_goal_s *goal);
	float	(*BotAvoidGoalTime)(int goalstate, int number);
	void	(*BotSetAvoidGoalTime)(int goalstate, int number, float avoidtime);
	void	(*BotInitLevelItems)(void);
	void	(*BotUpdateEntityItems)(void);
	int		(*BotLoadItemWeights)(int goalstate, char *filename);
	void	(*BotFreeItemWeights)(int goalstate);
	void	(*BotInterbreedGoalFuzzyLogic)(int parent1, int parent2, int child);
	void	(*BotSaveGoalFuzzyLogic)(int goalstate, char *filename);
	void	(*BotMutateGoalFuzzyLogic)(int goalstate, float range);
	int		(*BotAllocGoalState)(int client);
	void	(*BotFreeGoalState)(int handle);
	void	(*BotResetMoveState)(int movestate);
	void	(*BotMoveToGoal)(struct bot_moveresult_s *result, int movestate, struct bot_goal_s *goal, int travelflags);
	int		(*BotMoveInDirection)(int movestate, vec3_t dir, float speed, int type);
	void	(*BotResetAvoidReach)(int movestate);
	void	(*BotResetLastAvoidReach)(int movestate);
	int		(*BotReachabilityArea)(vec3_t origin, int testground);
	int		(*BotMovementViewTarget)(int movestate, struct bot_goal_s *goal, int travelflags, float lookahead, vec3_t target);
	int		(*BotPredictVisiblePosition)(vec3_t origin, int areanum, struct bot_goal_s *goal, int travelflags, vec3_t target);
	int		(*BotAllocMoveState)(void);
	void	(*BotFreeMoveState)(int handle);
	void	(*BotInitMoveState)(int handle, struct bot_initmove_s *initmove);
	void	(*BotAddAvoidSpot)(int movestate, vec3_t origin, float radius, int type);
	int		(*BotChooseBestFightWeapon)(int weaponstate, int *inventory);
	void	(*BotGetWeaponInfo)(int weaponstate, int weapon, struct weaponinfo_s *weaponinfo);
	int		(*BotLoadWeaponWeights)(int weaponstate, char *filename);
	int		(*BotAllocWeaponState)(void);
	void	(*BotFreeWeaponState)(int weaponstate);
	void	(*BotResetWeaponState)(int weaponstate);
	int		(*GeneticParentsAndChildSelection)(int numranks, float *ranks, int *parent1, int *parent2, int *child);
} ai_export_t;

typedef struct botlib_export_s
{
	aas_export_t aas;
	ea_export_t ea;
	ai_export_t ai;
	int (*BotLibSetup)(void);
	int (*BotLibShutdown)(void);
	int (*BotLibVarSet)(char *var_name, char *value);
	int (*BotLibVarGet)(char *var_name, char *value, int size);

	int (*PC_AddGlobalDefine)(char *string);
	int (*PC_LoadSourceHandle)(const char *filename);
	int (*PC_FreeSourceHandle)(int handle);
	int (*PC_SourceFileAndLine)(int handle, char *filename, int *line);

	int (*BotLibStartFrame)(float time);
	int (*BotLibLoadMap)(const char *mapname);
	int (*BotLibUpdateEntity)(int ent, bot_entitystate_t *state);
	int (*Test)(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
} botlib_export_t;

botlib_export_t *GetBotLibAPI( int apiVersion, botlib_import_t *import );

#endif
