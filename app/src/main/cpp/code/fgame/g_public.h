

#pragma once

#include "bg_public.h"

#include "botlib.h"

#define GAME_API_VERSION 4

#define SVF_NOCLIENT                    (1<<0)	
#define SVF_BOT                         (1<<1)
#define SVF_BROADCAST                   (1<<2)	
#define SVF_PORTAL                      (1<<3)	
#define SVF_SENDPVS                     (1<<4)	
#define SVF_USE_CURRENT_ORIGIN          (1<<5)	
#define SVF_DEADMONSTER                 (1<<6)	 
#define SVF_MONSTER                     (1<<7)	
#define SVF_USEBBOX                     (1<<9)	
#define SVF_ONLYPARENT                  (1<<10)	 
#define SVF_HIDEOWNER                   (1<<11)	
#define SVF_MONSTERCLIP                 (1<<12)	
#define SVF_PLAYERCLIP                  (1<<13)	
#define SVF_SENDONCE                    (1<<14) 
#define SVF_SENT                        (1<<15) 

#define SVF_NOSERVERINFO                (1<<16) 

#ifdef GAME_DLL
class Entity;
#endif

typedef struct gclient_s   gclient_t;
typedef struct dtiki_s     dtiki_t;
typedef struct tiki_cmd_s  tiki_cmd_t;
typedef struct dtikianim_s dtikianim_t;
typedef struct tiki_s      tiki_t;

typedef struct tikiAnim_s      tikiAnim_t;
typedef struct baseshader_s    baseshader_t;
typedef struct AliasListNode_s AliasListNode_t;
typedef void (*xcommand_t)(void);

// retail .cpp + 181 .h there is not one `edict->r.`, one `edict->tiki` or one
// and it is the port's, not retail's: retail splits this struct across g_local.h
typedef struct gentity_s gentity_t;  
struct gentity_s {
    entityState_t s; 

    struct gclient_s *client; 
    qboolean          inuse;
    qboolean          linked; 
    int               linkcount;

    int svflags; 

    qboolean bmodel; 
    vec3_t mins, maxs;
    int    contents; 

    vec3_t absmin, absmax; 

    float radius;  
    vec3_t centroid; 
    int   areanum; 

    vec3_t currentOrigin;
    vec3_t currentAngles;

    int     ownerNum; 

    solid_t solid; 

    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER


#ifdef GAME_DLL
    Entity *entity;
    float   freetime;  
    float   spawntime; 

    float radius2; 

    char entname[64];

    int clipmask;

    gentity_t *next;
    gentity_t *prev;
#endif
};

#ifndef MAX_NETNAME
#define MAX_NETNAME 36  
#endif

typedef struct {
    char userinfo[MAX_INFO_STRING];
    char netname[MAX_NETNAME];  
    char mp_playermodel[MAX_QPATH];
    char dm_morph_c1[MAX_INFO_STRING];  
    char dm_primary[MAX_QPATH];

    float enterTime;

    qboolean mp_lowBandwidth;  

    teamtype_t teamnum;
    int        round_kills;

    char ip[17];
    int  port;
} client_persistant_t;

struct gclient_s {
    playerState_t ps; 

#ifdef GAME_DLL
    int                 ping;
    client_persistant_t pers;
    float               cmd_angles[3];

    int lastActiveTime;
    int activeWarning;

    // references anywhere in the codebase and do not exist in retail's gclient_t, so
#endif
};

typedef enum {
    MOVER_POS1,
    MOVER_POS2,
    MOVER_1TO2,
    MOVER_2TO1
} moverState_t;


// (c) why it has not been folded into the retail slot yet.
#ifndef DTIKI_EXPRESSION_TYPES_DEFINED
#define DTIKI_EXPRESSION_TYPES_DEFINED
#define MAX_EXPRESSION_NAME_LENGTH 32

typedef struct
{
   int         morph_index;
   float       percent;
} dtikimorphtarget_t;

typedef struct
{
   char        alias[ MAX_EXPRESSION_NAME_LENGTH ];
   int         num_morph_targets;
   int         ofs_morph_targets;
} dtikiexpression_t;
#endif

typedef struct
   {

	void	         (*Printf)( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
   void           (*DPrintf)( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
	void	         (*WPrintf)( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
   void           (*WDPrintf)( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);
   void           (*DebugPrintf)( const char *fmt, ... ) Q_PRINTF_FUNC(1, 2);


   void			  (*LocalizeFilePath)(const char* path, char* localizedPath);

	void	         (*Error)( int errorLevel, const char *fmt, ... ) Q_PRINTF_FUNC(2, 3);

	int	         (*Milliseconds)( void );

	void	         *(*Malloc)( int size );
	void	         (*Free)( void *block );

	cvar_t	      *(*cvar)( const char *var_name, const char *value, int flags );
	cvar_t		  *(*cvar_get)( const char *var_name);
	void           (*cvar_set)( const char *var_name, const char *value );
	void			(*Cvar_VariableStringBuffer)( const char *var_name, char *buffer, int bufsize );
	void			(*Cvar_Register)( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
	float			(*Cvar_VariableValue)( const char *var_name );
	void			(*Cvar_Update)( vmCvar_t *vmCvar );
	int				(*Cvar_VariableIntegerValue)( const char *var_name );

	int	         (*argc)( void );
	char	         *(*argv)( int n );
	const char	   *(*args)(void);
   void           (*AddCommand)( const char *cmd );

	int	         (*FS_ReadFile)( const char *name, void **buf, qboolean quiet );
	qboolean	 (*FS_Exists)( const char *filename );
	void	         (*FS_FreeFile)( void *buf );
	void	         (*FS_WriteFile)( const char *qpath, const void *buffer, int size );
   fileHandle_t   (*FS_FOpenFileWrite)( const char *qpath );
   fileHandle_t   (*FS_FOpenFileAppend)( const char *filename );
	char**        (*FS_ListFiles)( const char *path, const char *extension, int *numfiles );

   char *         (*FS_PrepFileWrite)( const char *filename );
   int	         (*FS_Write)( const void *buffer, int len, fileHandle_t f );
   int	         (*FS_Read)( void *buffer, int len, fileHandle_t f );
   void	         (*FS_FCloseFile)( fileHandle_t f );
   int		      (*FS_FTell)( fileHandle_t f );
   int		      (*FS_FSeek)( fileHandle_t f, long offset, int origin );
   void           (*FS_Flush)( fileHandle_t f );
	void				(*FS_DeleteFile)(const char *filename);
	int					(*FS_GetFileList)(  const char *path, const char *extension, char *listbuf, int bufsize ); 
	
   const char *   (*GetArchiveFileName)( const char *gameName, const char *filename, const char *extension, qboolean tempFile );
	void	         (*SendConsoleCommand)( const char *text );
	void	         (*DebugGraph)( float value, int color );


	void	         (*SendServerCommand)( int clientnum, const char *fmt, ... ) Q_PRINTF_FUNC(2, 3);
	
	int				(*GetNumFreeReliableServerCommands) ( int clientNum );

   void           (*setConfigstring)( int index, const char *val );
   char *         (*getConfigstring)( int index );

   void           (*setUserinfo)( int index, const char *val );
   void           (*getUserinfo)( int index, char *buffer, int bufferSize );

	void	         (*SetBrushModel)( gentity_t *ent, const char *name );

	void           (*trace)( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder );
	void           (*fulltrace)( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean cylinder );
	int		      (*pointcontents)( const vec3_t point, int passEntityNum );
	int		      (*pointbrushnum)( const vec3_t point, int passEntityNum );
	qboolean	      (*inPVS)( const vec3_t p1, const vec3_t p2 );
	qboolean	      (*inPVSIgnorePortals)( const vec3_t p1, const vec3_t p2 );
	void		      (*AdjustAreaPortalState)( gentity_t *ent, qboolean open );
	qboolean	      (*AreasConnected)( int area1, int area2 );

	int				(*GetLightingGroup)( const char *group_name );

	void				(*SetDynamicLight)( int dynamic_light, float intensity );
	void				(*SetDynamicLightDefault)( int dynamic_light, float intensity );

	void				(*SetWindDirection)( vec3_t direction );
	void				(*SetWindIntensity)( float wind_intensity );

	void				(*SetWeatherInfo)( weather_t type, int intensity );

	void				(*SetTimeScale)( float time_scale );
	void	         (*linkentity)( gentity_t *ent );
	void	         (*unlinkentity)( gentity_t *ent );		

   int            (*AreaEntities)( const vec3_t mins, const vec3_t maxs, int *list, int maxcount, qboolean fullTrace );
	void				(*ClipToEntity)( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask );

	int			 (*objectivenameindex)( const char* name );
	int			 (*archetypeindex)(const char* name);
	int	         (*imageindex)( const char *name );
	int			 (*failedcondition)( const char *name );
	int	         (*itemindex)( const char *name );
	int	         (*soundindex)( const char *name );
	int	         (*modelindex)( const char *name );

  	void	         (*SetLightStyle)( int i, const char *data );

   const char	   *(*GameDir)( void );

   qboolean       (*IsModel)( int index );
	void		      (*setmodel)( gentity_t *ent, const char *name );
	void		      (*setviewmodel)( gentity_t *ent, const char *name );

   int				(*NumAnims)( int modelindex );
   int				(*NumSkins)( int modelindex );
   int				(*NumSurfaces)( int modelindex );
   int				(*NumTags)( int modelindex );
	int				(*NumMorphs)( int modelindex );
   qboolean       (*InitCommands)( int modelindex, tiki_cmd_t * tiki_cmd );
   void				(*CalculateBounds)( int modelindex, float scale, vec3_t mins, vec3_t maxs );

   void				(*TIKI_CacheAnim)( const char * path );

   const char *   (*Anim_NameForNum)( int modelindex, int animnum );
   int				(*Anim_NumForName)( int modelindex, const char * name );
   int				(*Anim_Random)( int modelindex, const char * name );
   int				(*Anim_NumFrames)( int modelindex, int animnum );
   float				(*Anim_Time)( int modelindex, int animnum );
   void				(*Anim_Delta)( int modelindex, int animnum, vec3_t delta );
   void				(*Anim_AbsoluteDelta)( int modelindex, int animnum, vec3_t delta );
   int				(*Anim_Flags)( int modelindex, int animnum );
   qboolean			(*Anim_HasCommands) ( int modelindex, int animnum );

   qboolean       (*Frame_Commands)( int modelindex, int animnum, int framenum, tiki_cmd_t * tiki_cmd );
   void				(*Frame_Delta)( int modelindex, int animnum, int framenum, vec3_t delta );
   float				(*Frame_Time)( int modelindex, int animnum, int framenum );
   void				(*Frame_Bounds)( int modelindex, int animnum, int framenum, float scale, vec3_t mins, vec3_t maxs );

   int				(*Surface_NameToNum)( int modelindex, const char * name );
   const char *   (*Surface_NumToName)( int modelindex, int num );
   int				(*Surface_Flags)( int modelindex, int num );
   int				(*Surface_NumSkins)( int modelindex, int num );

	int				(*Morph_NumForName)( int modelindex, const char * name );
	const char *   (*Morph_NameForNum)( int modelindex, int num );

	dtikimorphtarget_t *(*GetExpression)( int modelindex, const char *expression_name, int *number_of_morph_targets );

	int				(*Tag_NumForName)( int modelindex, const char * name );
   const char *   (*Tag_NameForNum)( int modelindex, int num );
   orientation_t  (*Tag_Orientation)( int modelindex, int anim, int frame, int num, float scale, int *bone_tag, vec4_t *bone_quat );
   orientation_t  (*Tag_OrientationEx)( int modelindex, int anim, int frame, int num, float scale, int *bone_tag, vec4_t *bone_quat,
      int crossblend_anim, int crossblend_frame, float crossblend_lerp, qboolean uselegs, qboolean usetorso, int torso_anim, int torso_frame,
      int torso_crossblend_anim, int torso_crossblend_frame, float torso_crossblend_lerp );
	int			   (*Bone_GetParentNum)( int modelindex, int bonenum );

   qboolean			(*Alias_Add)( int modelindex, const char * alias, const char * name, const char *parameters );
   const char *   (*Alias_FindRandom)( int modelindex, const char * alias );
   const char *   (*Alias_Find)( int modelindex, const char * alias );
   void				(*Alias_Dump)( int modelindex );
   void				(*Alias_Clear)( int modelindex );
	const char *   (*Alias_FindDialog)( int modelindex, const char * alias, int random, int entity_number );
	const char*    (*Alias_FindSpecificAnim)(int modelindex, const char *name );
	qboolean       (*Alias_CheckLoopAnim)(int modelindex, const char *name );
	void				*(*Alias_GetList)( int modelindex );
	void				(*Alias_UpdateDialog)( int model_index, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played );
	void				(*Alias_AddActorDialog)( int model_index, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop, int last_time_played );


	const char *	(*NameForNum)( int modelindex );

   qboolean			(*GlobalAlias_Add)( const char * alias, const char * name, const char *parameters );
   const char *   (*GlobalAlias_FindRandom)( const char * alias );
   const char *   (*GlobalAlias_Find)( const char * alias );
   void				(*GlobalAlias_Dump)( void );
   void			   (*GlobalAlias_Clear)( void );

	qboolean			(*isClientActive)( const gentity_t *ent );

	void		      (*centerprintf)( const gentity_t *ent, CenterPrintImportance importance, const char *fmt, ...) Q_PRINTF_FUNC(3, 4);
	void		      (*locationprintf)( const gentity_t *ent, int x, int y, const char *fmt, ...) Q_PRINTF_FUNC(4, 5);

	void				(*Sound)( vec3_t *org, int entnum, int channel, const char *sound_name, float volume, float attenuation, float pitch_modifier, qboolean onlySendToSameEntity );
	void				(*StopSound)( int entnum, int channel );
 	float 		   (*SoundLength) ( const char *path );
	char	         *(*GetNextMorphTarget)( const char *name, int *index, int *number_of_amplitudes, float *amplitude );


   unsigned short (*CalcCRC)(const unsigned char *start, int count);

   debugline_t		**DebugLines;
	int				*numDebugLines;

   void           (*LocateGameData)( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t,
                                     playerState_t *clients, int sizeofGameClient );

   void           (*SetFarPlane)( int farplane, qboolean farplane_cull );
	void				(*TikiReload)( const char * modelstr );
	void				(*TikiLoadFromTS)( const char * path, const char * tikidata);
	void*				(*ToolServerGetData)( void );
   void           (*SetSkyPortal)( qboolean skyportal );

	void				(*WidgetPrintf)( const char *widgetName, const char *fmt, ... );

	void				(*ProcessLoadingScreen)( const char *loadingStatus );

   
   const char*  (*MObjective_GetDescription)( const char* objectivename );
   void         (*MObjective_SetDescription)( const char* objectivename , const char* desc );

   qboolean     (*MObjective_GetShowObjective)( const char* objectivename );
   void         (*MObjective_SetShowObjective)( const char* objectivename , qboolean show );

   qboolean     (*MObjective_GetObjectiveComplete)( const char* objectivename );
   void         (*MObjective_SetObjectiveComplete)( const char* objectivename , qboolean complete );

   qboolean     (*MObjective_GetObjectiveFailed)( const char* objectivename );
   void         (*MObjective_SetObjectiveFailed)( const char* objectivename , qboolean failed );

   const char*  (*MObjective_GetNameFromIndex)( unsigned int index );
   int          (*MObjective_GetIndexFromName) (const char* name );
   void         (*MObjective_NewObjective)( const char* name );
   void         (*MObjective_ClearObjectiveList) (void);
   void         (*MObjective_ParseObjectiveFile) (const char* levelname );
   void			(*MObjective_Update)( const char* objectivesName );
	int			 (*MObjective_GetNumObjectives)(void);
	int			 (*MObjective_GetNumActiveObjectives)(void);
	int			 (*MObjective_GetNumCompleteObjectives)(void);
	int			 (*MObjective_GetNumFailedObjectives)(void);
	int			 (*MObjective_GetNumIncompleteObjectives)(void);



   const char*  (*MI_GetShader)( const char* informationname );
   void         (*MI_SetShader)( const char* informationname , const char* shader );

   const char*  (*MI_GetInformationData)( const char* informationname );
   void         (*MI_SetInformationData)( const char* informationname , const char* data );

   const char*  (*MI_GetNameFromIndex)( unsigned int index );
   int          (*MI_GetIndexFromName)( const char* name );
   void         (*MI_NewInformation)( const char* name );
   void         (*MI_ClearInformationList)( void );   

   void         (*MI_SetShowInformation) (const char* informationname , qboolean show );
   qboolean     (*MI_GetShowInformation) (const char* informationname );

	void		(*SR_InitializeStringResource)( void );
	void		(*SR_UninitializeStringResource)( void );
	void		(*SR_LoadLevelStrings)(const char* levelName);

	unsigned int	(*GetViewModeMask)( const char *viewModeName );
	unsigned int	(*GetViewModeClassMask)( const char *className );
	qboolean			(*GetViewModeSendInMode)( unsigned int viewModeMask );
	qboolean			(*GetViewModeSendNotInMode)( unsigned int viewModeMask );
	qboolean			(*GetViewModeScreenBlend)( unsigned int viewModeMask, vec3_t color, float *alpha, qboolean *additive );

	void				(*GetLevelDefs)( const char *name, const char **environmentName, const char **levelName, const char **sublevelName );
	qboolean			(*areSublevels)( const char *oldMapName, const char *newMapName );

	const char *		(*SurfaceTypeToName)( unsigned int surfaceType );


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


	void		(*Print)(int type, char *fmt, ...);

	int			(*PointContents)(vec3_t point);
	char		*(*BSPEntityData)(void);
	void		(*BSPModelMinsMaxsOrigin)(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin);
	void		(*BotClientCommand)(int client, const char *command); 
	int			(*AvailableMemory)(void);		
	void		*(*HunkAlloc)(int size);		
	int			(*FS_FOpenFile)( const char *qpath, fileHandle_t *file, fsMode_t mode );
	int			(*FS_Seek)( fileHandle_t f, long offset, int origin );
	int			(*DebugLineCreate)(void);
	void		(*DebugLineDelete)(int line);
	void		(*DebugLineShow)(int line, vec3_t start, vec3_t end, int color);
	int			(*DebugPolygonCreate)(int color, int numPoints, vec3_t *points);
	void		(*DebugPolygonDelete)(int id);

	void		(*DropClient)( int clientNum, const char *reason ); 

	void		(*SV_GetServerinfo)( char *buffer, int bufferSize ) ;
	int			(*BotAllocateClient)(void); 
	int			(*BotGetSnapshotEntity)( int client, int ent ); 
	int			(*BotGetConsoleMessage)( int client, char *buf, int size ); 

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
	void (*BotUserCommand)(int cl, usercmd_t *cmd); 
	

	// PORT-ONLY SLOTS -- appended, NOT part of retail's game_import_t.




	// retail one-argument slot now serves them and this port-only slot is gone.
	// FS_FOpenFileByMode), so the retail slot serves them and this port-only

   } game_import_t;

typedef struct gameExport_s {
    int apiversion;

    void (*Init)(int startTime, int randomSeed);
    void (*Shutdown)(void);
    void (*Cleanup)(qboolean samemap);

    void (*SpawnEntities)(const char *mapname, const char *entstring, int levelTime);
    void (*PostLoad)(void);
    void (*PostSublevelLoad)(const char *spawnPosName);

    const char *(*ClientConnect)(int clientNum, qboolean firstTime, qboolean isBot, qboolean checkPassword);

    void (*ClientBegin)(gentity_t *ent, const usercmd_t *cmd);
    void (*ClientUserinfoChanged)(gentity_t *ent, const char *userinfo);
    void (*ClientDisconnect)(gentity_t *ent);
    void (*ClientCommand)(gentity_t *ent);
    void (*ClientThink)(gentity_t *ent, usercmd_t *cmd);

    void (*PrepFrame)(void);
    void (*RunFrame)(int levelTime, int frameTime);
    qboolean (*SendEntity)(gentity_t *clientEntity, gentity_t *entityToSend);
    void (*UpdateEntityStateForClient)(gentity_t *clientEntity, entityState_t *state);
    void (*UpdatePlayerStateForClient)(gentity_t *clientEntity, playerState_t *state);
    void (*ExtraEntitiesToSend)(gentity_t *clientEntity, int *numExtraEntities, int *extraEntities);
    int (*GetEntityCurrentAnimFrame)(int entityNum, int bodyPart);

    qboolean (*ConsoleCommand)(void);

    void (*WritePersistant)(const char *filename, qboolean sublevelTransition);
    qboolean (*ReadPersistant)(const char *filename, qboolean sublevelTransition);

    void (*WriteLevel)(const char *filename, qboolean autosave);
    qboolean (*ReadLevel)(const char *filename);
    qboolean (*LevelArchiveValid)(const char *filename);

    qboolean (*inMultiplayerGame)(void);
    qboolean (*isDefined)(const char *propname);
    const char *(*getDefine)(const char *propname);

    int (*BotAIStartFrame)(int time); 
    void (*AddBot_f)(void);           

    int (*GetTotalGameFrames)(void);

    struct gentity_s *gentities;
    int               gentitySize;
    int               num_entities; 
    int               max_entities;

    const char *error_message;
} game_export_t;

#ifdef __cplusplus
extern "C"
#endif

#ifdef GAME_DLL
#    ifdef WIN32
    __declspec(dllexport)
#    else
__attribute__((visibility("default")))
#    endif
#endif
        game_export_t *GetGameAPI(game_import_t *import);

#if 0
typedef enum {

	G_PRINT,		

	G_ERROR,		

	G_MILLISECONDS,	

	G_CVAR_REGISTER,	
	G_CVAR_UPDATE,	
	G_CVAR_SET,		
	G_CVAR_VARIABLE_INTEGER_VALUE,	

	G_CVAR_VARIABLE_STRING_BUFFER,	

	G_ARGC,			

	G_ARGV,			

	G_FS_FOPEN_FILE,	
	G_FS_READ,		
	G_FS_WRITE,		
	G_FS_FCLOSE_FILE,		

	G_SEND_CONSOLE_COMMAND,	



	G_LOCATE_GAME_DATA,		

	G_DROP_CLIENT,		

	G_SEND_SERVER_COMMAND,	

	G_SET_CONFIGSTRING,	

	G_GET_CONFIGSTRING,	

	G_GET_USERINFO,		

	G_SET_USERINFO,		

	G_GET_SERVERINFO,	

	G_SET_BRUSH_MODEL,	

	G_TRACE,	

	G_POINT_CONTENTS,	

	G_IN_PVS,			

	G_IN_PVS_IGNORE_PORTALS,	

	G_ADJUST_AREA_PORTAL_STATE,	

	G_AREAS_CONNECTED,	

	G_LINKENTITY,		

	G_UNLINKENTITY,		

	g_entities_IN_BOX,	

	G_ENTITY_CONTACT,	

	G_BOT_ALLOCATE_CLIENT,	

	G_BOT_FREE_CLIENT,	

	G_GET_USERCMD,	

	G_GET_ENTITY_TOKEN,	

	G_FS_GETFILELIST,
	G_DEBUG_POLYGON_CREATE,
	G_DEBUG_POLYGON_DELETE,
	G_REAL_TIME,
	G_SNAPVECTOR,

	G_TRACECAPSULE,	
	G_ENTITY_CONTACTCAPSULE,	
	
	G_FS_SEEK,

	BOTLIB_SETUP = 200,				
	BOTLIB_SHUTDOWN,				
	BOTLIB_LIBVAR_SET,
	BOTLIB_LIBVAR_GET,
	BOTLIB_PC_ADD_GLOBAL_DEFINE,
	BOTLIB_START_FRAME,
	BOTLIB_LOAD_MAP,
	BOTLIB_UPDATENTITY,
	BOTLIB_TEST,

	BOTLIB_GET_SNAPSHOT_ENTITY,		
	BOTLIB_GET_CONSOLE_MESSAGE,		
	BOTLIB_USER_COMMAND,			

	BOTLIB_AAS_ENABLE_ROUTING_AREA = 300,
	BOTLIB_AAS_BBOX_AREAS,
	BOTLIB_AAS_AREA_INFO,
	BOTLIB_AAS_ENTITY_INFO,

	BOTLIB_AAS_INITIALIZED,
	BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX,
	BOTLIB_AAS_TIME,

	BOTLIB_AAS_POINT_AREA_NUM,
	BOTLIB_AAS_TRACE_AREAS,

	BOTLIB_AAS_POINT_CONTENTS,
	BOTLIB_AAS_NEXT_BSP_ENTITY,
	BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY,
	BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY,
	BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY,
	BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY,

	BOTLIB_AAS_AREA_REACHABILITY,

	BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA,

	BOTLIB_AAS_SWIMMING,
	BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT,

	BOTLIB_EA_SAY = 400,
	BOTLIB_EA_SAY_TEAM,
	BOTLIB_EA_COMMAND,

	BOTLIB_EA_ACTION,
	BOTLIB_EA_GESTURE,
	BOTLIB_EA_TALK,
	BOTLIB_EA_ATTACK,
	BOTLIB_EA_USE,
	BOTLIB_EA_RESPAWN,
	BOTLIB_EA_CROUCH,
	BOTLIB_EA_MOVE_UP,
	BOTLIB_EA_MOVE_DOWN,
	BOTLIB_EA_MOVE_FORWARD,
	BOTLIB_EA_MOVE_BACK,
	BOTLIB_EA_MOVE_LEFT,
	BOTLIB_EA_MOVE_RIGHT,

	BOTLIB_EA_SELECT_WEAPON,
	BOTLIB_EA_JUMP,
	BOTLIB_EA_DELAYED_JUMP,
	BOTLIB_EA_MOVE,
	BOTLIB_EA_VIEW,

	BOTLIB_EA_END_REGULAR,
	BOTLIB_EA_GET_INPUT,
	BOTLIB_EA_RESET_INPUT,


	BOTLIB_AI_LOAD_CHARACTER = 500,
	BOTLIB_AI_FREE_CHARACTER,
	BOTLIB_AI_CHARACTERISTIC_FLOAT,
	BOTLIB_AI_CHARACTERISTIC_BFLOAT,
	BOTLIB_AI_CHARACTERISTIC_INTEGER,
	BOTLIB_AI_CHARACTERISTIC_BINTEGER,
	BOTLIB_AI_CHARACTERISTIC_STRING,

	BOTLIB_AI_ALLOC_CHAT_STATE,
	BOTLIB_AI_FREE_CHAT_STATE,
	BOTLIB_AI_QUEUE_CONSOLE_MESSAGE,
	BOTLIB_AI_REMOVE_CONSOLE_MESSAGE,
	BOTLIB_AI_NEXT_CONSOLE_MESSAGE,
	BOTLIB_AI_NUM_CONSOLE_MESSAGE,
	BOTLIB_AI_INITIAL_CHAT,
	BOTLIB_AI_REPLY_CHAT,
	BOTLIB_AI_CHAT_LENGTH,
	BOTLIB_AI_ENTER_CHAT,
	BOTLIB_AI_STRING_CONTAINS,
	BOTLIB_AI_FIND_MATCH,
	BOTLIB_AI_MATCH_VARIABLE,
	BOTLIB_AI_UNIFY_WHITE_SPACES,
	BOTLIB_AI_REPLACE_SYNONYMS,
	BOTLIB_AI_LOAD_CHAT_FILE,
	BOTLIB_AI_SET_CHAT_GENDER,
	BOTLIB_AI_SET_CHAT_NAME,

	BOTLIB_AI_RESET_GOAL_STATE,
	BOTLIB_AI_RESET_AVOID_GOALS,
	BOTLIB_AI_PUSH_GOAL,
	BOTLIB_AI_POP_GOAL,
	BOTLIB_AI_EMPTY_GOAL_STACK,
	BOTLIB_AI_DUMP_AVOID_GOALS,
	BOTLIB_AI_DUMP_GOAL_STACK,
	BOTLIB_AI_GOAL_NAME,
	BOTLIB_AI_GET_TOP_GOAL,
	BOTLIB_AI_GET_SECOND_GOAL,
	BOTLIB_AI_CHOOSE_LTG_ITEM,
	BOTLIB_AI_CHOOSE_NBG_ITEM,
	BOTLIB_AI_TOUCHING_GOAL,
	BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE,
	BOTLIB_AI_GET_LEVEL_ITEM_GOAL,
	BOTLIB_AI_AVOID_GOAL_TIME,
	BOTLIB_AI_INIT_LEVEL_ITEMS,
	BOTLIB_AI_UPDATE_ENTITY_ITEMS,
	BOTLIB_AI_LOAD_ITEM_WEIGHTS,
	BOTLIB_AI_FREE_ITEM_WEIGHTS,
	BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC,
	BOTLIB_AI_ALLOC_GOAL_STATE,
	BOTLIB_AI_FREE_GOAL_STATE,

	BOTLIB_AI_RESET_MOVE_STATE,
	BOTLIB_AI_MOVE_TO_GOAL,
	BOTLIB_AI_MOVE_IN_DIRECTION,
	BOTLIB_AI_RESET_AVOID_REACH,
	BOTLIB_AI_RESET_LAST_AVOID_REACH,
	BOTLIB_AI_REACHABILITY_AREA,
	BOTLIB_AI_MOVEMENT_VIEW_TARGET,
	BOTLIB_AI_ALLOC_MOVE_STATE,
	BOTLIB_AI_FREE_MOVE_STATE,
	BOTLIB_AI_INIT_MOVE_STATE,

	BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON,
	BOTLIB_AI_GET_WEAPON_INFO,
	BOTLIB_AI_LOAD_WEAPON_WEIGHTS,
	BOTLIB_AI_ALLOC_WEAPON_STATE,
	BOTLIB_AI_FREE_WEAPON_STATE,
	BOTLIB_AI_RESET_WEAPON_STATE,

	BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION,
	BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC,
	BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC,
	BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL,
	BOTLIB_AI_GET_MAP_LOCATION_GOAL,
	BOTLIB_AI_NUM_INITIAL_CHATS,
	BOTLIB_AI_GET_CHAT_MESSAGE,
	BOTLIB_AI_REMOVE_FROM_AVOID_GOALS,
	BOTLIB_AI_PREDICT_VISIBLE_POSITION,

	BOTLIB_AI_SET_AVOID_GOAL_TIME,
	BOTLIB_AI_ADD_AVOID_SPOT,
	BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL,
	BOTLIB_AAS_PREDICT_ROUTE,
	BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX,

	BOTLIB_PC_LOAD_SOURCE,
	BOTLIB_PC_FREE_SOURCE,
	BOTLIB_PC_READ_TOKEN,
	BOTLIB_PC_SOURCE_FILE_AND_LINE,

	G_TIKI_REGISTERMODEL,
	G_TIKI_GETBONES,
	G_TIKI_SETCHANNELS,
	G_TIKI_APPENDFRAMEBOUNDSANDRADIUS,
	G_TIKI_ANIMATE,
	G_TIKI_GETBONENAMEINDEX,
	G_MSG_WRITEBITS,
	G_MSG_WRITECHAR,
	G_MSG_WRITEBYTE,
	G_MSG_WRITESVC,
	G_MSG_WRITESHORT,
	G_MSG_WRITELONG,
	G_MSG_WRITEFLOAT,
	G_MSG_WRITESTR,
	G_MSG_WRITEANGLE8,
	G_MSG_WRITEANGLE16,
	G_MSG_WRITECOORD,
	G_MSG_WRITEDIR,
	G_MSG_STARTCGM,
	G_MSG_ENDCGM,
	G_MSG_SETCLIENT,
	G_MSG_BROADCASTALL,
	G_MSG_BROADCASTVISIBLE,
	G_MSG_BROADCASTHEARABLE,
} game_import_t;


typedef enum {
	GAME_INIT,	

	GAME_SHUTDOWN,	

	GAME_CLIENT_CONNECT,	

	GAME_CLIENT_BEGIN,				

	GAME_CLIENT_USERINFO_CHANGED,	

	GAME_CLIENT_DISCONNECT,			

	GAME_CLIENT_COMMAND,			

	GAME_CLIENT_THINK,				

	GAME_RUN_FRAME,					

	GAME_CONSOLE_COMMAND,			

	BOTAI_START_FRAME				
} game_export_t;
#endif
