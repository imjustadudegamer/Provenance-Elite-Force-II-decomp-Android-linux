

#pragma once

#include "../renderercommon/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAGIC_UNUSED_NUMBER 7777
#define CMD_BACKUP          64
#define CMD_MASK            (CMD_BACKUP - 1)
    // correctness, but it is not retail's constant and CG_PredictPlayerState replays

#define MAX_ENTITIES_IN_SNAPSHOT 1024


    typedef struct {
        int snapFlags; 
        int ping;

        int serverTime; 

        byte areamask[MAX_MAP_AREA_BYTES]; 

        playerState_t ps; 

        int           numEntities;                        
        entityState_t entities[MAX_ENTITIES_IN_SNAPSHOT]; 

        int numServerCommands;     
        int serverCommandSequence; 

        int            number_of_sounds;
        server_sound_t sounds[MAX_SERVER_SOUNDS];

        // clSnapshot_t::ws, which is exactly why CG_Weather was never portable (ledger row
        worldState_t ws;
    } snapshot_t;

    typedef struct stopWatch_s {
        int iStartTime;
        int iEndTime;
        int eType;
    } stopWatch_t;

    typedef struct AliasList_s     AliasList_t;
    typedef struct AliasListNode_s AliasListNode_t;
    typedef struct dtiki_s         dtiki_t;
    typedef struct tiki_cmd_s      tiki_cmd_t;



    typedef struct clientGameImport_s {
        /* [0] 0x000 RESERVED -- retail memsets and never writes offset 0 */
        void (*reserved_0x000)(void);


        void (*Printf)(const char *fmt, ...);
        void (*DPrintf)(const char *fmt, ...);
        void (*Com_WPrintf)(const char *fmt, ...);
        void (*Com_WDPrintf)(const char *fmt, ...);
        void (*DebugPrintf)(const char *fmt, ...);

        void *(*Malloc)(int size);
        void (*Free)(void *block);

        void (*Error)(int errorLevel, const char *fmt, ...);

        int (*Milliseconds)(void);

        cvar_t *(*Cvar_Get)(const char *var_name, const char *value, int flags);
        void (*Cvar_SetValue)(const char *var_name, float value);
        void (*Cvar_Set)(const char *var_name, const char *value);

        int (*Argc)(void);
        char *(*Argv)(int n);
        char *(*Args)(void); 
        void (*AddCommand)(const char *cmd);
        void (*Cmd_Stuff)(const char *text);
        void (*Cmd_Execute)(int execWhen, const char *text);
        void (*Cmd_TokenizeString)(const char *textIn);

        long (*FS_ReadFile)(const char *name, void **buf, qboolean quiet);
        /* [21] 0x054 RESERVED -- retail declares the FS slot but never fills it */
        void (*reserved_0x054)(void);
        void (*FS_FreeFile)(void *buf);
        int (*FS_WriteFile)(const char *qpath, const void *buffer, int size);
        void (*FS_WriteTextFile)(const char *qpath, const void *buffer, int size);
        void (*SendConsoleCommand)(const char *text);


        void (*SendClientCommand)(const char *s);

        void (*CM_LoadMap)(const char *name, int *checksum);
        clipHandle_t (*CM_InlineModel)(int index); 
        int (*CM_NumInlineModels)(void);
        int (*CM_PointContents)(const vec3_t p, int headnode);
        int (*CM_TransformedPointContents)(
            const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles
        );
        void (*CM_BoxTrace)(
            trace_t     *results,
            const vec3_t start,
            const vec3_t end,
            const vec3_t mins,
            const vec3_t maxs,
            int          headnode,
            int          brushmask,
            qboolean     cylinder
        );
        void (*CM_BoxTraceEx)(
            trace_t     *results,
            const vec3_t start,
            const vec3_t end,
            const vec3_t mins,
            const vec3_t maxs,
            int          headnode,
            int          brushmask,
            qboolean     cylinder,
            int          leafFlags
        );
        void (*CM_TransformedBoxTrace)(
            trace_t     *results,
            const vec3_t start,
            const vec3_t end,
            const vec3_t mins,
            const vec3_t maxs,
            int          headnode,
            int          brushmask,
            const vec3_t origin,
            const vec3_t angles,
            qboolean     cylinder
        );
        clipHandle_t (*CM_TempBoxModel)(const vec3_t mins, const vec3_t maxs, int contents);
        int (*R_MarkFragments)(
            int             numPoints,
            const vec3_t   *points,
            const vec3_t    projection,
            int             maxPoints,
            vec3_t          pointBuffer,
            int             maxFragments,
            markFragment_t *fragmentBuffer,
            float           fPushDist           
        );
        char *(*Com_GetGameTypeName)(int gametype, char *buf, int bufsize);
        void (*CL_MapLoadingClientOnly)(const char *pszMapName);


        void (*S_StartSound)(
            const vec3_t origin,
            int          entNum,
            int          entChannel,
            sfxHandle_t  sfxHandle,
            float        volume,
            float        minDist,
            float        pitch
        );
        void (*S_StartLocalSound)(const char *soundName, qboolean forceLoad);
        void (*S_StopSound)(int entnum, int channel);
        void (*S_ClearLoopingSounds)(void);
        void (*S_AddLoopingSound)(
            const vec3_t origin,
            const vec3_t velocity,
            sfxHandle_t  sfxHandle,
            float        volume,
            float        minDist,
            int          entityNum,
            int          unused7,
            int          unused8
        );
        void (*S_Respatialize)(int entityNum, const vec3_t origin, vec3_t axis[3]);
        void (*S_BeginRegistration)(void);
        sfxHandle_t (*S_RegisterSound)(const char *sample);
        void (*S_EndRegistration)(void);
        void (*S_UpdateEntity)(int entityNum, const vec3_t origin, const vec3_t velocity, qboolean use_listener);
        void (*S_SetReverb)(int reverb_type, float reverb_level);


        void (*MUSIC_NewSoundtrack)(const char *name);
        void (*MUSIC_UpdateMood)(int current_mood, int fallback_mood);
        void (*MUSIC_UpdateVolume)(float volume, float fade_time);
        void (*MUSIC_UpdateDucking)(int ducking);


        float (*get_lip_length)(const char *soundName);
        char *(*GetNextMorphTarget)(const char *name, int *index, int *frame, float *amplitude);
        float *(*get_camera_offset)(qboolean *lookactive, qboolean *resetview);
        float *(*get_crosshairoffset)(void);


        void (*CL_BeginRegistration)(void);
        void (*RE_EndRegistration)(void);
        void (*R_ClearScene)(void);
        void (*RE_ClearFlares)(void);
        void (*R_RenderScene)(const refdef_t *fd);
        void (*R_LoadWorldMap)(const char *mapname);
        qhandle_t (*R_RegisterModel)(const char *name);
        qhandle_t (*RE_ForceRegisterModel)(const char *name);
        qhandle_t (*RE_RegisterSkin)(const char *name);
        qhandle_t (*R_RegisterShader)(const char *name);
        qhandle_t (*R_RegisterShaderNoMip)(const char *name);
        void (*R_AddRefEntityToScene)(const refEntity_t *ent);
        void (*R_AddRefSpriteToScene)(const refEntity_t *ent);
        void (*R_AddLightToScene)(const vec3_t origin, float intensity, float r, float g, float b, int type);
        qboolean (*R_AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t *verts, int renderfx);
        // across frames); it is never dereferenced.  Argument order is retail's, from
        void (*R_SetColor)(const vec4_t rgba); 
        void (*R_DrawStretchPic)(
            float     x,
            float     y,
            float     w,
            float     h,
            float     s1,
            float     t1,
            float     s2,
            float     t2,
            qhandle_t hShader
        ); 
        refEntity_t *(*R_GetRenderEntity)(int entityNumber);
        void (*R_ModelBounds)(clipHandle_t model, vec3_t mins, vec3_t maxs);
        float (*R_ModelRadius)(clipHandle_t model);
        float (*R_Noise)(float x, float y, float z, double t);
        void (*R_DebugLine)(const vec3_t start, const vec3_t end, float r, float g, float b, float alpha);
        void (*R_AddLensflareToScene)(
            void *key, const vec3_t origin, int lensflareNum, float r, float g, float b, float scale, qhandle_t replacementShader
        );
        int (*R_LensflareNameToNum)(const char *name);
        void (*R_SwipeBegin)(float thistime, float life, qhandle_t shader);
        void (*R_SwipePoint)(vec3_t p1, vec3_t p2, float time);
        void (*R_SwipeEnd)(void);
        int (*R_GetShaderWidth)(qhandle_t shader);
        int (*R_GetShaderHeight)(qhandle_t shader);
        void (*R_DrawBox)(float x, float y, float w, float h);

        void (*GetGameState)(gameState_t *gamestate);
        int (*GetSnapshot)(int snapshotNumber, snapshot_t *snapshot);
        void (*GetCurrentSnapshotNumber)(int *snapshotNumber, int *serverTime);
        void (*GetGlconfig)(glconfig_t *glconfig);
        qboolean (*GetParseEntityState)(int parseEntityNumber, entityState_t *state);
        int (*GetCurrentCmdNumber)(void); 
        qboolean (*GetUserCmd)(int cmdNumber, usercmd_t *ucmd);
        qboolean (*GetServerCommand)(int serverCommandNumber);

        qboolean (*Alias_Add)(const char *alias, const char *name, const char *parameters);
        const char *(*Alias_FindRandom)(const char *alias);
        void (*Alias_Dump)(void);
        void (*Alias_Clear)(void);

        int (*R_Model_GetHandle)(qhandle_t handle);
        qboolean (*TIKI_AnimLoaded)(int handle, int animnum);
        int (*TIKI_NumAnims)(int handle);
        /* [103] 0x19C RESERVED -- retail declares the TIKI slot but never fills it */
        void (*reserved_0x19C)(void);
        int (*TIKI_NumSurfaces)(int handle);
        int (*TIKI_NumTags)(int handle);
        int (*TIKI_InitCommands_Client)(int handle, tiki_cmd_t *tiki_cmd);
        void (*TIKI_CalculateBounds)(int handle, float scale, vec3_t mins, vec3_t maxs);
        void (*TIKI_FlushAll)(void);
        const char *(*TIKI_Name)(int handle);
        float (*TIKI_GetFadeDistMod)(int handle);
        dtiki_t *(*TIKI_GetTiki)(int handle);
        void (*TIKI_CacheAnim)(const char *path);
        const char *(*Anim_NameForNum)(int handle, int animnum);
        int (*Anim_NumForName)(int handle, const char *name);
        int (*Anim_Random)(int handle, const char *name);
        int (*Anim_NumFrames)(int handle, int animnum);
        float (*Anim_Time)(int handle, int animnum);
        void (*Anim_Delta)(int handle, int animnum, vec3_t delta);
        int (*Anim_Flags)(int handle, int animnum);
        int (*Anim_CrossblendTime)(int handle, int animnum);
        qboolean (*Anim_HasCommands)(int handle, int animnum);
        qboolean (*Frame_Commands)(int handle, int animnum, int framenum, tiki_cmd_t *tiki_cmd, int *extra_anims);
        void (*TIKI_Frame_Delta)(int handle, int animnum, int framenum, vec3_t delta);
        float (*TIKI_Frame_Time)(int handle, int animnum, int framenum);
        void (*TIKI_Frame_Bounds)(int handle, int animnum, int framenum, float scale, vec3_t mins, vec3_t maxs);
        void (*TIKI_Frame_Coordinates)(int handle, int animnum, int framenum, float scale, float *coordinates);
        float (*TIKI_Frame_Radius)(int handle, int animnum, int framenum);
        int (*Surface_NameToNum)(int handle, const char *name);
        const char *(*TIKI_Surface_NumToName)(int handle, int num);
        int (*TIKI_Surface_Flags)(int handle, int num);
        int (*TIKI_Surface_NumSkins)(int handle, int num);
        int (*Tag_NumForName)(int handle, const char *name);
        const char *(*Tag_NameForNum)(int handle, int num);
        void (*TIKI_Orientation)(
            orientation_t *out,
            int            handle,
            int            anim,
            int            frame,
            int            tagnum,
            float          scale,
            const int     *bone_tag,
            const vec4_t  *bone_quat
        );
        void (*TIKI_Tag_OrientationEx)(
            orientation_t *out,
            int            handle,
            int            anim,
            int            frame,
            int            tagnum,
            float          scale,
            const int     *bone_tag,
            const vec4_t  *bone_quat,
            int            crossblend_anim,
            int            crossblend_frame,
            float          crossblend_lerp,
            qboolean       uselegs,
            qboolean       usetorso,
            int            torso_anim,
            int            torso_frame,
            int            torso_crossblend_anim,
            int            torso_crossblend_frame,
            float          torso_crossblend_lerp
        );
        void (*TIKI_Tag_NamedOrientation)(
            orientation_t *out,
            int            handle,
            int            anim,
            int            frame,
            float          scale,
            const char    *tagName,
            const int     *bone_tag,
            const vec4_t  *bone_quat
        );
        void (*TIKI_Tag_LerpOrientation)(orientation_t *out, int handle, const refEntity_t *ent, int tagnum);
        qboolean (*TIKI_Alias_Add)(int handle, const char *alias, const char *name, const char *parameters);
        const char *(*TIKI_Alias_FindRandom)(int handle, const char *alias);
        void (*TIKI_Alias_Dump)(int handle);
        void (*TIKI_Alias_Clear)(int handle);
        const char *(*TIKI_Alias_FindDialog)(
            int handle, const char *alias, int random, int entity_number
        );
        const char *(*TIKI_Alias_FindSpecificAnim)(int handle, const char *name);
        qboolean (*TIKI_Alias_CheckLoopAnim)(int handle, const char *name);
        int (*TIKI_RegisterTiki)(const char *path);

        qboolean (*UI_ParseScores)(void);
        void (*Dialog_LoadDialog)(const char *mapname);
        void (*UI_DisplayDialogConsole)(const char *dialogName, const char *dialogTextSoundName, int entnum);
        void (*Dialog_ResetConsole)(const char *consoleName);
        void (*UI_AddPrintString)(const char *text, float *pos, int width, float *color, const char *fontname);
        void (*UI_PrintString)(const char *fontname, float x, float y, const char *text, float alpha);
        void (*UI_AddNotifyString)(const char *text);
        int (*UI_GetGlobalFontHeight)(void);
        int (*UI_GetFontHeight)(const char *fontname, qboolean virtualScreen);
        void (*UI_NewMap)(void);
        void (*UI_ShowMenu)(const char *name, qboolean bForce);
        void (*UI_HideMenu)(const char *name, qboolean bForce);
        qboolean (*UI_IsMenuActive)(void);

        const char *(*MissionObjective_GetDescription)(const char *objectivename);
        void (*MissionObjective_SetDescription)(const char *objectivename, const char *desc);
        qboolean (*MissionObjective_GetShowObjective)(const char *objectivename);
        void (*MissionObjective_SetShowObjective)(const char *objectivename, qboolean show);
        qboolean (*MissionObjective_GetObjectiveComplete)(const char *objectivename);
        void (*MissionObjective_SetObjectiveComplete)(const char *objectivename, qboolean complete);
        qboolean (*MissionObjective_GetObjectiveFailed)(const char *objectivename);
        void (*MissionObjective_SetObjectiveFailed)(const char *objectivename, qboolean failed);
        const char *(*MissionObjective_GetNameFromIndex)(unsigned int index);
        int (*MissionObjective_GetIndexFromName)(const char *name);
        void *(*MissionObjective_NewObjective)(const char *name);    
        void (*MissionObjective_ClearObjectiveList)(void);
        qboolean (*MissionObjectives_ParseObjectiveFile)(const char *levelname);

        const char *(*MissionInformation_GetShader)(const char *informationname);
        void (*MissionInformation_SetShader)(const char *informationname, const char *shader);
        const char *(*MissionInformation_GetInformationData)(const char *informationname);
        void (*MissionInformation_SetInformationData)(const char *informationname, const char *data);
        const char *(*MissionInformation_GetNameFromIndex)(unsigned int index);
        int (*MissionInformation_GetIndexFromName)(const char *name);
        void *(*MissionInformation_NewInformation)(const char *name);  
        void (*MissionInformation_ClearInformationList)(void);
        void (*MissionInformation_SetShowInformation)(const char *informationname, qboolean show);
        qboolean (*MissionInformation_GetShowInformation)(const char *informationname);

        void (*Radar_AddBlip)(
            const vec3_t pos, const vec3_t color, const char *name, int selected, int alwaysShow
        );
        void (*Radar_AddMissionObjectiveBlip)(
            const vec3_t pos, const vec3_t color, const char *name, int selected, int inRange
        );
        void (*Radar_SetViewAngles)(const vec3_t angles);

        void (*ProcessLoadingScreen)(const char *loadingStatus);
        const char *(*CL_ConfigString)(int index);

        qboolean (*GetViewModeEntityColor)(unsigned int viewModeMask, vec3_t color);
        qboolean (*GetViewModeEntityShader)(unsigned int viewModeMask, const char **shader);
        qboolean (*GetViewModeEntityAlpha)(unsigned int viewModeMask, float *alpha);
        qboolean (*GetViewModeHud)(unsigned int viewModeMask, const char **hudName);
        qboolean (*GetViewModeUseFullbright)(unsigned int viewModeMask);
        int (*GetNumViewModes)(void);

        void (*GetLevelDefs)(
            const char *name, const char **environmentName, const char **levelName, const char **sublevelName
        );
        qboolean (*areSublevels)(const char *oldMapName, const char *newMapName);
        void (*loadLevelStrings)(const char *levelName);
        const char *(*getStringResource)(const char *stringId);
        qboolean (*isStringResource)(const char *text);

        // Signatures are the decompiled retail ones, not the 4-argument
        void (*SetGameplayFloatProperty)(
            const char *objectName, const char *propertyName, float value, qboolean create
        );
        float (*GetGameplayFloatProperty)(
            const char *objectName, const char *propertyName
        );
        void (*SetGameplayStringProperty)(
            const char *objectName, const char *propertyName, const char *stringValue, qboolean create
        );
        void (*GetGameplayStringProperty)(
            const char *objectName, const char *propertyName, char *buffer, int bufferLen
        );
        qboolean (*HasGameplayProperty)(const char *objectName, const char *propertyName);

        int (*SurfaceNameToType)(const char *name);
        const char *(*SurfaceTypeToName)(unsigned int surfaceType);

        // not an ABI change.  Each entry records why it is not retail and which
        cvar_t *(*Cvar_Find)(const char *);        // MOHAA; retail has Cvar_FindVar but never
        void (*Cvar_CheckRange)(cvar_t *, float, float, qboolean);  
        char **(*FS_ListFilteredFiles)(const char *, const char *, const char *, qboolean, int *, qboolean); 
        void (*FS_FreeFileList)(char **);          
        fileHandle_t (*FS_FOpenFileWrite)(const char *); 
        size_t (*FS_Write)(const void *, size_t, fileHandle_t); 
        void (*FS_FCloseFile)(fileHandle_t);       
        void (*FS_CanonicalFilename)(char *);      
        void (*CM_PrintBSPFileSizes)(void);        
        qboolean (*CM_LeafInPVS)(int, int);        
        int (*CM_PointLeafnum)(const vec3_t);      
        int (*R_MarkFragmentsForInlineModel)(
            clipHandle_t, const vec3_t, const vec3_t, int, const vec3_t *, const vec3_t,
            int, vec3_t, int, markFragment_t *);                  
        void (*R_GetInlineModelBounds)(int, vec3_t, vec3_t);     
        void (*R_GetLightingForDecal)(vec3_t, const vec3_t, const vec3_t); 
        void (*R_GetLightingForSmoke)(vec3_t, const vec3_t);     
        int (*R_GatherLightSources)(const vec3_t, vec3_t *, vec3_t *, int); 
        void (*R_PrintBSPFileSizes)(void);         
        int (*MapVersion)(void);                   
        int (*R_MapVersion)(void);                 
        qhandle_t (*R_SpawnEffectModel)(const char *, vec3_t, vec3_t[3]); 
        qhandle_t (*R_RegisterServerModel)(const char *);         
        void (*R_UnregisterServerModel)(qhandle_t);               
        const char *(*R_GetModelName)(qhandle_t);  
        const char *(*R_GetShaderName)(qhandle_t); 
        void (*R_AddTerrainMarkToScene)(int, qhandle_t, int, const polyVert_t *, int); 
        fontheader_t *(*R_LoadFont)(const char *);  
        void (*R_DrawString)(fontheader_t *, const char *, float, float, int, const float *); 
        dshader_t *(*GetShader)(int);              
        void (*S_SetGlobalAmbientVolumeLevel)(float); 
        float (*S_GetSoundTime)(sfxHandle_t);       
        int (*S_ChannelNameToNum)(const char *);   
        const char *(*S_ChannelNumToName)(int);    
        int (*S_IsSoundPlaying)(int, const char *);
        void (*SetTime)(int);                       
        qboolean (*Alias_ListAdd)(AliasList_t *, const char *, const char *, const char *);   
        const char *(*Alias_ListFindRandom)(AliasList_t *, const char *);  
        AliasList_t *(*AliasList_New)(const char *);   
        void (*UI_GetHighResolutionScale)(vec2_t);  
        void *(*TIKI_GetSkeletor)(dtiki_t *, int);  
        void (*TIKI_SetEyeTargetPos)(dtiki_t *, int, vec3_t);  
        int (*Anim_FlagsSkel)(dtiki_t *, int);     
        qboolean (*Frame_CommandsTime)(dtiki_t *, int, float, float, tiki_cmd_t *); 
        void (*ForceUpdatePose)(refEntity_t *);    
        qboolean (*TIKI_IsOnGround)(refEntity_t *, int, float);  
        dtiki_t *(*TIKI_FindTiki)(const char *);    
    } clientGameImport_t;

    // retail CG_Init copies exactly 820 bytes, so the engine never hands cgame
#ifdef __cplusplus
static_assert(offsetof(clientGameImport_t, Cvar_Find) == 205 * sizeof(void *),
              "clientGameImport_t: the retail region must be exactly 205 slots");
static_assert(offsetof(clientGameImport_t, Printf) == 1 * sizeof(void *),
              "clientGameImport_t: slot 1 must be Printf (retail 0x004)");
static_assert(offsetof(clientGameImport_t, R_SetColor) == 73 * sizeof(void *),
              "clientGameImport_t: slot 73 must be R_SetColor (retail 0x124)");
static_assert(offsetof(clientGameImport_t, GetSnapshot) == 89 * sizeof(void *),
              "clientGameImport_t: slot 89 must be GetSnapshot (retail 0x164)");
static_assert(offsetof(clientGameImport_t, Radar_AddBlip) == 182 * sizeof(void *),
              "clientGameImport_t: slot 182 must be Radar_AddBlip (retail 0x2D8)");
static_assert(offsetof(clientGameImport_t, SurfaceTypeToName) == 204 * sizeof(void *),
              "clientGameImport_t: slot 204 must be SurfaceTypeToName (retail 0x330)");
#endif


    typedef struct {
        void (*CG_Init)(clientGameImport_t *imported, int serverMessageNum, int serverCommandSequence);
        void (*CG_Shutdown)(void);
        void (*CG_DrawActiveFrame)(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback);
        qboolean (*CG_ConsoleCommand)(void);
        void (*CG_GetRendererConfig)(void);
        void (*CG_Draw2D)(void);
        void (*CG_GetInterpState)(int entNum, vec3_t *origin, vec3_t *angles);
        void (*CG_GetViewOrigin)(float *x, float *y, float *z);
        void (*CG_GetViewAngles)(float *x, float *y, float *z);
        int (*CG_GetThirdPerson)(void);
        void (*CG_ProcessSnapshots)(int serverTime);
        void (*CG_UpdateClient)(int serverTime);
    } clientGameExport_t;

#ifdef __cplusplus
static_assert(sizeof(clientGameExport_t) == 12 * sizeof(void *),
              "clientGameExport_t: retail's cgame export table is exactly 12 slots");
static_assert(offsetof(clientGameExport_t, CG_Shutdown) == 1 * sizeof(void *),
              "clientGameExport_t: slot 1 must be CG_Shutdown (retail +0x004)");
static_assert(offsetof(clientGameExport_t, CG_DrawActiveFrame) == 2 * sizeof(void *),
              "clientGameExport_t: slot 2 must be CG_DrawActiveFrame (retail +0x008)");
static_assert(offsetof(clientGameExport_t, CG_GetInterpState) == 6 * sizeof(void *),
              "clientGameExport_t: slot 6 must be CG_GetInterpState (retail +0x018)");
static_assert(offsetof(clientGameExport_t, CG_GetThirdPerson) == 9 * sizeof(void *),
              "clientGameExport_t: slot 9 must be CG_GetThirdPerson (retail +0x024)");
static_assert(offsetof(clientGameExport_t, CG_ProcessSnapshots) == 10 * sizeof(void *),
              "clientGameExport_t: slot 10 must be CG_ProcessSnapshots (retail +0x028)");
static_assert(offsetof(clientGameExport_t, CG_UpdateClient) == 11 * sizeof(void *),
              "clientGameExport_t: slot 11 must be CG_UpdateClient (retail +0x02C)");
#endif

#ifdef CGAME_DLL
#    ifdef WIN32
    __declspec(dllexport)
#    else
    __attribute__((visibility("default")))
#    endif
#endif
        clientGameExport_t *GetCGameAPI(void);

#ifdef __cplusplus
}
#endif
