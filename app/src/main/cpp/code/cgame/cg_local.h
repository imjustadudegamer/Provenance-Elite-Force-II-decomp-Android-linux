

#pragma once

#include "../qcommon/q_shared.h"
#include "tr_types.h"
#include "../fgame/bg_public.h"
#include "cm_public.h"
#include "cg_public.h"

#ifdef __cplusplus

extern "C" {
#endif

    typedef struct {
        int   anim;
        int   frame;
        int   oldanim;
        int   oldframe;
        int   starttime;
        float framelerp;
    } lerpstate_t;

    typedef struct {
        lerpstate_t base;

        lerpstate_t crossblend;
        int         crossblend_totaltime; 
        float       crossblend_lerp;      

        int      numframes;
        float    time_per_frame;  
        qboolean driven;


        int next_evaluate_time;

        int    last_driven_time;
        vec3_t last_origin;
        vec3_t frame_delta;      
        float  frame_distance;   
        float  current_distance; 

        qboolean has_commands;
        int      last_cmd_frame;
        int      last_cmd_anim;
        int      last_cmd_time;
    } animstate_t;


#define CF_UPDATESWIPE        0x0000001
#define CF_COMMANDS_PROCESSED 0x0000002

    typedef struct centity_s {
        entityState_t currentState; 
        entityState_t nextState;    
        qboolean      teleported;   
        qboolean      interpolate;  
        qboolean      currentValid; 

        int miscTime;

        int    errorTime; 
        vec3_t errorOrigin;
        vec3_t errorAngles;

        qboolean extrapolated; 
        vec3_t   rawOrigin;
        vec3_t   rawAngles;

        vec3_t beamEnd;

        vec3_t lerpOrigin;
        vec3_t lerpAngles;

        sfxHandle_t tikiLoopSound;
        float       tikiLoopSoundVolume;
        float       tikiLoopSoundMinDist;
        float       tikiLoopSoundMaxDist;
        float       tikiLoopSoundPitch;
        int         tikiLoopSoundFlags;

        float color[4];
        float client_color[4]; 
        int   clientFlags;

        qhandle_t itemRingShader;

        int splash_last_spawn_time;
        int splash_still_count;

        animstate_t animstate[2];

        qboolean bFootOnGround_Right;
        qboolean bFootOnGround_Left;
        int      iNextLandTime;
    } centity_t;



// [UIVK:DO] retail's mark record holds TEN vertices, not eight.  Retail CG_ImpactMark
#define MAX_VERTS_ON_POLY 10
#define MAX_MARK_POLYS    64

    typedef struct markPoly_s {
        struct markPoly_s* nextPoly;
        int numVerts;
        polyVert_t verts[MAX_VERTS_ON_POLY];
        int iIndex;
    } markPoly_t;

    typedef struct markObj_s {
        struct markObj_s* prevMark;
        struct markObj_s* nextMark;
        int time;
        int lastVisTime;
        int leafnum;
        vec3_t pos;
        float radius;
        int markShader;
        int alphaFade;
        int fadein;
        markPoly_t* markPolys;
        int lastProbeTime;   
    } markObj_t;

#define MAX_CUSTOM_SOUNDS 32

    typedef struct {
        int  team;
        char name[64];
    } clientInfo_t;

#define MAX_RAIN_SHADERS 16



    typedef struct {
        int clientFrame; 
        int clientNum;

        qboolean demoPlayback;
        qboolean levelShot; 

        int         latestSnapshotNum;  
        int         latestSnapshotTime; 
        snapshot_t *snap;               
        snapshot_t *nextSnap;           
        snapshot_t  activeSnapshots[2];

        float
            frameInterpolation; 

        qboolean thisFrameTeleport;
        qboolean nextFrameTeleport;
        qboolean nextFrameCameraCut;

        int frametime;      
        int oldFrameTime;    

        int time; 

        int physicsTime; 

        qboolean renderingThirdPerson; 

        qboolean      hyperspace; 
        playerState_t predicted_player_state;
        qboolean      validPPS; 
        int           predictedErrorTime;
        vec3_t        predictedError;

        float duckChange; 
        int   duckTime;   
        float landChange; 
        int   landTime;   

        vec3_t autoAngles;
        vec3_t autoAxis[3];
        vec3_t autoAnglesSlow;
        vec3_t autoAxisSlow[3];
        vec3_t autoAnglesFast;
        vec3_t autoAxisFast[3];

        refdef_t refdef;
        vec3_t   playerHeadPos;     
        vec3_t   refdefViewAngles;  
        vec3_t   currentViewPos;    
        vec3_t   currentViewAngles; 

        float    fRefFovXSin;
        float    fRefFovXCos;
        float    fRefFovYSin;
        float    fRefFovYCos;
        float    fCurrentViewHeight;
        float    fCurrentViewBobPhase;
        float    fCurrentViewBobAmp;

        int      pLastPlayerWorldModel;
        int      pPlayerFPSModel;
        qhandle_t hPlayerFPSModelHandle;
        qboolean  bFPSModelLastFrame;
        qboolean  bFPSOnGround;
        int      pAlliedPlayerModel;
        qhandle_t hAlliedPlayerModelHandle;
        int      pAxisPlayerModel;
        qhandle_t hAxisPlayerModelHandle;
        qboolean  serverAlliedModelValid;
        qboolean  serverAxisModelValid;

        vec3_t vOffsetViewAngles;
        vec3_t vLastHeadAngles;
        vec3_t vLastViewAngles;
        vec3_t vEyeOffsetMax;
        float  fEyeOffsetFrac;

        float  SoundOrg[3];  
        vec3_t SoundAxis[3]; 

        int   viewInterpTime;         
        float viewInterpBlendLeft;    
        int   viewInterpCamera;       

        vec3_t camera_origin; 
        vec3_t camera_angles; 
        float  camera_fov;    
        int    viewInterpCameraFlags; 

        vec3_t snapDamageAngles;

        float zoomSensitivity;

        qboolean bIntermissionDisplay;


        int  centerPrintTime;
        int  centerPrintCharWidth;
        int  centerPrintY;
        char centerPrint[1024];
        int  centerPrintLines;

        int matchStartTime;

        refEntity_t testModelEntity;
        char        testModelName[MAX_QPATH];
        qboolean    testGun;

        float    farplane_distance;
        float    farplane_bias;
        vec3_t   farplane_color;
        qboolean farplane_cull;
        qboolean farplane_fog;

        qboolean sky_portal;
        float    sky_alpha;
        vec3_t   sky_origin;
        vec3_t   sky_axis[3]; 


        int          cachedViewMode;

		clientInfo_t clientinfo[MAX_CLIENTS];
    } cg_t;

    typedef struct {
        qhandle_t     backTileShader;
        qhandle_t     lagometerShader;
        qhandle_t     shadowMarkShader;
        qhandle_t     footShadowMarkShader;
        qhandle_t     wakeMarkShader;
        qhandle_t     pausedShader;
        qhandle_t     identificationShader; 
        qhandle_t     disconnectedShader;       
        qhandle_t     itemRingShaderDefault;    // retail cgs+0x195C8, never written
        qhandle_t     crosshairShader;
        qhandle_t     useCrosshairShader;       
        qhandle_t     tricorderCrosshairShader; 
        qhandle_t     damageFrontShader;
        qhandle_t     damageBackShader;
        qhandle_t     damageLeftShader;
        qhandle_t     damageRightShader;
        qhandle_t     levelExitShader;
        qhandle_t     zoomOverlayShader;
        qhandle_t     kar98TopOverlayShader;
        qhandle_t     kar98BottomOverlayShader;
        qhandle_t     binocularsOverlayShader;
        fontheader_t *hudDrawFont;
        fontheader_t *attackerFont;
        fontheader_t *objectiveFont;
        qhandle_t     objectivesBackShader;
        qhandle_t     checkedBoxShader;
        qhandle_t     uncheckedBoxShader;
    } media_t;

    typedef struct {
        gameState_t gameState;    
        glconfig_t  glconfig;     
        float       screenXScale; 
        float       screenYScale;
        float       screenXBias;
        vec2_t      uiHiResScale;
        int serverCommandSequence; 
        int processedSnapshotNum;  

        qboolean localServer;    
        int      levelStartTime; 

        int        matchEndTime;
        int        serverLagTime;
        gametype_t gametype;
        int        mp_flags;  
        int        teamflags;
        int        pointlimit;  
        int        timelimit;   
        int        maxclients;
        int        cinematic;
        char       mapname[MAX_QPATH];
        char       rawMapName[MAX_QPATH];

        int voteTime;
        int numVotesYes;
        int numVotesNo;
        int numUndecidedVotes;
        qboolean voteRefreshed;
        char voteString[1024];

        qhandle_t   model_draw[MAX_MODELS];
        int         model_tiki[MAX_MODELS];    
        sfxHandle_t sound_precache[MAX_SOUNDS];
        int         numInlineModels;
        qhandle_t   inlineDrawModel[MAX_MODELS];
        vec3_t      inlineModelMidpoints[MAX_MODELS];

        media_t media;
    } cgs_t;


    extern cgs_t              cgs;
    extern cg_t               cg;
    extern clientGameImport_t cgi;

    static inline dtiki_t *CG_TikiFromHandle(int handle)
    {
        return handle >= 0 ? cgi.TIKI_GetTiki(handle) : NULL;
    }
    extern int                cg_protocol;
    extern centity_t          cg_entities[MAX_GENTITIES];
    extern markPoly_t         *cg_markPolys;

    extern cvar_t *cg_fsDebug;
    cvar_t *CG_FsDebug(void);

    extern cvar_t *cg_viewsize;                    
    extern cvar_t *cg_viewposx;                    
    extern cvar_t *cg_viewposy;                    
    extern cvar_t *cg_viewsizewidth;               
    extern cvar_t *cg_viewsizeheight;              
    extern cvar_t *cg_android_aspect;              
    extern cvar_t *zoomfov;                        
    extern cvar_t *cg_relativeyaw;                 
    extern cvar_t *cg_marks;                       
    extern cvar_t *cg_markTime;                    
    extern cvar_t *cg_animSpeed;                    
    extern cvar_t *cg_debugAnim;                    
    extern cvar_t *cg_debugAnimWatch;               
    extern cvar_t *cg_errorDecay;                  
    extern cvar_t *cg_nopredict;                   
    extern cvar_t *cg_showmiss;                    
    extern cvar_t *cg_stats;                       
    extern cvar_t *cg_hidetempmodels;              
    extern cvar_t *cg_synchronousClients;          
    extern cvar_t *cg_stereoSeparation;            
    extern cvar_t *cg_lagometer;                   
    extern cvar_t *paused;                         
    extern cvar_t *cg_added_objective;             
    extern cvar_t *r_lerpmodels;                   
    extern cvar_t *cg_3rd_person;                  
    extern cvar_t *cg_camerahorizontaloffset;      
    extern cvar_t *cg_cameraheight;                
    extern cvar_t *cg_cameradist;                  
    extern cvar_t *cg_cameraverticaldisplacement;  
    extern cvar_t *cg_camerascale;                 
    extern cvar_t *cg_traceinfo;                   
    extern cvar_t *cg_debugFootsteps;               
    extern cvar_t *cg_shadows;                     
    extern cvar_t *developer;                       
    extern cvar_t *cg_crosshair;                   
    extern cvar_t *cg_crosshairtexture;            
    extern cvar_t *r_fadeDistMod;                  
    extern cvar_t *cg_viewBob;                     
    extern cvar_t *cg_viewStrafeTilt;              
    extern cvar_t *cg_viewBobMaxHorz;              
    extern cvar_t *cg_viewBobMaxVert;              
    extern cvar_t *cg_viewBobMaxRoll;              
    extern cvar_t *cg_viewBobTime;                 
    extern cvar_t *cg_viewWeaponBob;               
    extern cvar_t *cg_viewKick;                    
    extern cvar_t *cg_targetedPlayerName;          
    extern cvar_t *cg_spectatingPlayerName;        
    extern cvar_t *cg_debug;                       

    int CG_OffsetFirstPersonView(void);

    extern float ef2_bobfracsin;

    // The flag is dead in retail's own cgame -- the Mac body never reads its argument --
    void CG_InterpolateView(int personView);

    void        CG_ProcessConfigString(int num);
    void        CG_FlushCommandManager(void);
    const char *CG_ConfigString(int index);
    void        CG_AddToTeamChat(const char *str);
    void        CG_NewClientinfo(int clientNum);
    sfxHandle_t CG_CustomSound(int entityNum, const char *soundName);
    int         CG_CrosshairPlayer(void);
    int         CG_LastAttacker(void);
    void        CG_Init(clientGameImport_t *imported, int serverMessageNum, int serverCommandSequence);
    void        CG_Shutdown(void);
    void        CG_ServerRestarted(void);
    void        CG_ParseFogInfo(const char *str);

    void CG_ModelAnim(centity_t *cent, qboolean bDoShaderTime);
    void CalcModelAlpha(refEntity_t *model, int tiki);
    void CG_ClearModelAnimation(
        int tiki, animstate_t *state, int anim, int time, const vec3_t origin, int entnum, float animationRate
    );
    void CG_AttachEntity(
        refEntity_t *entity, refEntity_t *parent, int tiki, int tagnum, qboolean use_angles, vec3_t attach_offset,
        vec3_t attach_angles_offset
    );

    void     CG_Event(centity_t *cent);
    void     CG_UpdateEntityEmitters(int tiki, refEntity_t *ent, centity_t *cent);
    void     CG_RemoveClientEntity(int number, int tiki, centity_t *cent);
    void     CG_UpdateTestEmitter(void);
    void     CG_InitializeCommandManager(void);
    void     CG_InitializeClientEmitters(void);
    void     CG_TurnEmitterOn(const char *emittername, entityState_t *es);
    void     CG_ProcessInitCommands(int tiki, refEntity_t *ent);
    void     CG_EndTiki(dtiki_t *tiki);
    qboolean CG_Command_ProcessFile(const char *filename, qboolean quiet, int curTiki);
    void     CG_RestartCommandManager(int time);
    void     CG_ProcessEntityCommands(int tiki, int frame, int anim, int entnum, refEntity_t *ent, centity_t *cent);
    void     CG_ClientCommands(int tiki, int frame, int anim, animstate_t *state, refEntity_t *ent, centity_t *cent);
    float    RandomizeRange(float start, float end);

    extern cvar_t *cg_showtempmodels;
    extern cvar_t *cg_detail;
    extern cvar_t *cg_effectdetail;
    extern cvar_t *cg_physics_fps;

    void CG_AddTempModels(void);
    void CG_ResetTempModels(void);

    void CG_Splash(centity_t *cent);

    void CG_EventList_f(void);
    void CG_EventHelp_f(void);
    void CG_DumpEventHelp_f(void);
    void CG_PendingEvents_f(void);
    void CG_ClassList_f(void);
    void CG_ClassTree_f(void);
    void CG_ClassEvents_f(void);
    void CG_DumpClassEvents_f(void);
    void CG_DumpAllClasses_f(void);

    void L_InitEvents(void);
    void L_ShutdownEvents(void);

    void CG_TestModel_f(void);
    void CG_TestGun_f(void);
    void CG_TestModelNextFrame_f(void);
    void CG_TestModelPrevFrame_f(void);
    void CG_TestModelNextSkin_f(void);
    void CG_TestModelPrevSkin_f(void);

    void CG_ZoomDown_f(void);
    void CG_ZoomUp_f(void);

    void  CG_GetViewOrigin(float *x, float *y, float *z);
    void  CG_GetViewAngles(float *x, float *y, float *z);
    int   CG_GetThirdPerson(void);
    qboolean CG_FrustumCullSphere(const vec3_t vPos, float fRadius);
    qboolean CG_SimpleDistanceCull(const vec3_t origin, float maxDist);
    void  CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback);
    void  CG_UpdateClient(int serverTime);
    void  CG_GetInterpState(int entNum, vec3_t *origin, vec3_t *angles);


    // MAX_TEAM_INFO) in bg_public.h; cgame does not see that header.  Both of retail's
    #define CG_CS_ARCHETYPE 1586

    const char *CG_EntityInteractiveTypeName(const centity_t *cent, char *out, int outlen);
    void        CG_InteractiveTypeColor(const char *itypeName, vec4_t out);
    void        CG_GetIdentificationData(const centity_t *cent, vec4_t out);
    void CG_AdjustFrom640(float *x, float *y, float *w, float *h);
    void CG_AndroidDisplayRect(int surfaceW, int surfaceH, int *outX, int *outY, int *outW, int *outH);
    void CG_TileClear(void);
    void CG_Precache(void);                 
    void CG_Draw2D(void);
    void CG_UpdateTargetIdentifier(const centity_t *cent); 
    void CG_DrawTargetIdentifier(void);    

    void CG_AddLagometerFrameInfo(void);
    void CG_AddLagometerSnapshotInfo(snapshot_t *snap);
    void CG_CenterPrint(const char *str, int y, int charWidth);
    void CG_DrawHead(float x, float y, float w, float h, int clientNum, vec3_t headAngles);
    void CG_DrawActive(stereoFrame_t stereoView);
    void CG_DrawFlagModel(float x, float y, float w, float h, int team);

    void CG_BuildSolidList(void);
    int  CG_PointContents(const vec3_t point, int passEntityNum);
    void CG_ClipMoveToEntities(
        const vec3_t start,
        const vec3_t mins,
        const vec3_t maxs,
        const vec3_t end,
        int          skipNumber,
        int          mask,
        trace_t     *tr,
        qboolean     cylinder
    );
    void CG_Trace(
        trace_t     *result,
        const vec3_t start,
        const vec3_t mins,
        const vec3_t maxs,
        const vec3_t end,
        int          skipNumber,
        int          mask,
        qboolean     cylinder,
        qboolean     cliptoentities,
        const char  *description
    );
    void CG_PredictPlayerState(void);

    void BG_EvaluateTrajectory(const trajectory_t *tr, int atTime, vec3_t result);
    void CG_SetEntitySoundPosition(centity_t *cent);
    void CG_AddPacketEntities(void);
    void CG_Beam(centity_t *cent);
    void CG_AdjustPositionForMover(const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out);

    void CG_PositionEntityOnTag(refEntity_t *entity, const refEntity_t *parent, qhandle_t parentModel, char *tagName);
    void
    CG_PositionRotatedEntityOnTag(refEntity_t *entity, const refEntity_t *parent, qhandle_t parentModel, char *tagName);
    void CG_GetOrigin(centity_t *cent, vec3_t origin);
    void CG_EntityEffects(centity_t *cent);

    void CG_InitMarks(void);
    void CG_AddMarks(void);
    void CG_ImpactMark(
        qhandle_t    markShader,
        const vec3_t origin,
        const vec3_t dir,
        float        orientation,
        float        red,
        float        green,
        float        blue,
        float        alpha,
        int          flag,
        float        radius,
        qboolean     temporary,
        int          lighting,
        qboolean     fadeColour
    );
    void CG_ImpactMarkSimple(
        qhandle_t    markShader,
        const vec3_t origin,
        const vec3_t dir,
        float        orientation,
        float        fRadius,
        float        red,
        float        green,
        float        blue,
        float        alpha,
        qboolean     alphaFade,
        qboolean     temporary,
        qboolean     dolighting,
        qboolean     fadein
    );
    void CG_AssembleFinalMarks(
        vec3_t           *markPoints,
        markFragment_t   *markFragments,
        int              numFragments,
        qboolean         (*PerPolyCallback)(const vec3_t* markPoints, markFragment_t* mf, polyVert_t* verts, void* pCustom),
        int              (*GetLeafCallback)(markFragment_t* mf, void* pCustom),
        void             *pCustom,
        vec3_t           pos,
        float            radius,
        qhandle_t        markShader,
        qboolean         fadein,
        qboolean         alphaFade
    );
    qboolean CG_CheckMakeMarkOnEntity(int iEntIndex);

    void CG_ProcessSnapshots(int serverTime);

    qboolean CG_ConsoleCommand(void);
    void     CG_InitConsoleCommands(void);

    void CG_ExecuteNewServerCommands(int latestSequence);
    void CG_ParseServerinfo(void);

    void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops);

    void CG_ResetPlayerEntity(centity_t *cent);
    void CG_Player(centity_t *cent);

    void CG_ProcessSound(server_sound_t *sound);

    void CG_AddBeams(void);
    void CG_MultiBeamBegin(void);
    void CG_MultiBeamAddPoints(
        vec3_t start, vec3_t end, int numsegments, int flags, float minoffset, float maxoffset, qboolean addstartpoint
    );
    void CG_MultiBeamEnd(
        float scale, int renderfx, const char *beamshadername, byte modulate[4], int flags, int owner, float life
    );
    void CG_CreateBeam(
        const vec3_t  start,
        const vec3_t  dir,
        int           owner,
        qhandle_t     hModel,
        float         alpha,
        float         scale,
        int           flags,
        float         length,
        int           life,
        qboolean      create,
        const vec3_t  endpointvec,
        float         min_offset,
        float         max_offset,
        int           overlap,
        int           subdivisions,
        int           delay,
        const char    *beamshader,
        const byte    *modulate,
        int           numspherebeams,
        float         sphereradius,
        int           toggledelay,
        float         endalpha,
        int           renderfx,
        const char    *name,
        float         wavespeed,
        qboolean      canendearly,
        float         amp
    );

    void CG_KillBeams(int entity_number);
    void CG_Rope(centity_t *cent);
    void CG_RestartBeams(int timedelta);

    void CG_Emitter(centity_t *cent);

    void CG_UpdateTestEmitter(void);
    void CG_InitTestEmitter(void);
    void CG_ResetTestEmitter(void);
    void CG_TestEmitter_f(void);
    void CG_ReloadEmitter_f(void);
    void CG_DumpEmitter_f(void);
    void CG_NextEmitter_f(void);
    void CG_PrevEmitter_f(void);

    void RemoveBeamList(int owner);
    void CG_MultiBeam(centity_t *cent);

    void CG_InitializeWeather(void);
    void CG_Weather(void);

    void CG_SetLightStyle(int num, const char *s);
#ifdef __cplusplus
    qboolean CG_LightStyleColor(int style, int realtime, vec4_t color, qboolean clamp = qfalse);
#else
qboolean CG_LightStyleColor(int style, int realtime, vec4_t color, qboolean clamp);
#endif
    void CG_ClearLightStyles(void);
    int  CG_RegisterLightStyle(const char *name);

    void CG_ClearSwipes(void);
    void CG_UpdateSwipes(void);

    void CG_MessageMode_f(void);
    void CG_TeamMessageMode_f(void);
    void CG_HudPrint_f(void);

#ifdef __cplusplus
#endif


    void CG_InitFootprintMap(void);
    void CG_ClearFootprintMap(void);

#ifdef __cplusplus
}
#endif
