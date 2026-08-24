

#pragma once

#include "g_local.h"
#include "../corepp/class.h"
#include "../corepp/vector.h"
#include "../corepp/script.h"
#include "../corepp/listener.h"
#include "scriptvariable.h"
#include "program.h"
typedef int entflags_t;

extern Event EV_LeftVector;
extern Event EV_Anim;
extern Event EV_RightVector;
extern Event EV_UpVector;

extern Event EV_ClientMove;
extern Event EV_ClientEndFrame;

class Entity;
typedef SafePtr<Entity> EntityPtr;

typedef Entity SimpleEntity;
typedef EntityPtr SimpleEntityPtr;

#include "damagemodification.h"

#define FLAG_IGNORE 0
#define FLAG_CLEAR  1
#define FLAG_ADD    2

#define ENTITY_CREATE_FLAG_ANIMATE (1 << 0)
#define ENTITY_CREATE_FLAG_MOVER   (1 << 1)

class UseData;
class Mover;
class Animate;

typedef enum {
    DAMAGE_NO,
    DAMAGE_YES, 
    DAMAGE_AIM  



} damage_t;

typedef enum {
    legs,
    torso,
    all
} bodypart_t;

enum ef2AnimChannel_t {
    EF2ANIM_INFER = -2,
    // aim triples land here.  Retail has no third channel; do not invent one.
    EF2ANIM_NONE  = -1,
    EF2ANIM_LEGS  = 0, 
    EF2ANIM_TORSO = 1, 
};


#define DEAD_NO          0
#define DEAD_DYING       1
#define DEAD_DEAD        2
#define DEAD_RESPAWNABLE 3

extern Event EV_Activate;
extern Event EV_ProcessInitCommands;  
extern Event EV_ForceActivate;
extern Event EV_Killed;
extern Event EV_DisplayEffect;  
extern Event EV_GotKill;
extern Event EV_Pain;
extern Event EV_Damage;
extern Event EV_Use;
extern Event EV_Stun;
extern Event EV_Gib;
extern Event EV_Kill;
extern Event EV_DeathSinkStart;
extern Event EV_Entity_AddImmunity;
extern Event EV_Entity_RemoveImmunity;
extern Event EV_Fade;
extern Event EV_FadeOut;
extern Event EV_FadeIn;
extern Event EV_FadeNoRemove;
extern Event EV_ForceAlpha;
extern Event EV_TouchTriggers;
extern Event EV_SetMaxHealth;
extern Event EV_ProcessGameplayData;

extern Event EV_MoveDone;
extern Event EV_Touch;
extern Event EV_Contact;
extern Event EV_LostContact;
extern Event EV_Blocked;
extern Event EV_Attach;
extern Event EV_AttachModel;
extern Event EV_RemoveAttachedModelByTargetname;  
extern Event EV_SetOriginEveryFrame;  
extern Event EV_RemoveAttachedModel;
extern Event EV_Detach;
extern Event EV_DetachAllChildren;

extern Event EV_NewAnim;
extern Event EV_LastFrame;
extern Event EV_TakeDamage;
extern Event EV_NoDamage;


extern Event EV_LookAtMe;

extern Event EV_Model;
extern Event EV_GetModel;
extern Event EV_Hide;
extern Event EV_Show;
extern Event EV_BecomeSolid;
extern Event EV_BecomeNonSolid;
extern Event EV_Sound;
extern Event EV_StopSound;
extern Event EV_Bind;
extern Event EV_Unbind;
extern Event EV_JoinTeam;
extern Event EV_QuitTeam;
extern Event EV_SetHealth;
extern Event EV_SetHealth2;
extern Event EV_Entity_GetHealth;
extern Event EV_Entity_ProcessGameplayData;  
extern Event EV_SetSize;
extern Event EV_SetAlpha;
extern Event EV_SetKillTarget;
extern Event EV_SetAngles;                   
extern Event EV_SetAngle;                    
extern Event EV_SetOrigin;                   
extern Event EV_SetGameplayDamage;           
extern Event EV_SetTargetName;               
extern Event EV_SetTarget;                   
extern Event EV_GetTargetName;              
extern Event EV_GetRawTargetName;           
extern Event EV_GetTarget;                  
extern Event EV_GetTargetEntity;            
extern Event EV_SetRandomScale;             
extern Event EV_TikiNote;                   
extern Event EV_TikiTodo;                   
extern Event EV_SetGameplayHealth;           
extern Event EV_SetFloatVar;                 
extern Event EV_SetVectorVar;                
extern Event EV_SetStringVar;                
extern Event EV_GetFloatVar;                 
extern Event EV_GetVectorVar;                
extern Event EV_GetStringVar;                
extern Event EV_RemoveVariable;              
extern Event EV_DoesVarExist;                
extern Event EV_WatchOffset;                
extern Event EV_StartStasis;                
extern Event EV_StopStasis;                  
extern Event EV_StartAnimating;
extern Event EV_StopAnimating;                
extern Event EV_SetFrame;                     
extern Event EV_Torso_StopAnimating;          
extern Event EV_SimplePlayDialog;             
extern Event EV_SurfaceModelEvent;
extern Event EV_Stop;
extern Event EV_StopLoopSound;
extern Event EV_SetControllerAngles;

extern Event EV_SelfDetonate;               
extern Event EV_SetEntityExplosionModel;    
extern Event EV_SpawnEffect;                
extern Event EV_CreateEarthquake;           
extern Event EV_Warp;                       
extern Event EV_ProjectileAttackEntity;     
extern Event EV_TraceHitsEntity;            
extern Event EV_AffectingViewMode;          
extern Event EV_AddHealthOverTime;          
extern Event EV_SetCustomShader;            
extern Event EV_ClearCustomShader;          


extern Event EV_BroadcastSound;
extern Event EV_HeardSound;
extern Event EV_Hurt;

class ScriptMaster;


#define MAX_MODEL_CHILDREN 8

class Entity;

typedef SafePtr<Entity> EntityPtr;

class BindInfo
{
public:
    int numchildren;
    int children[MAX_MODEL_CHILDREN];

    str           moveteam;
    class Entity *teamchain;
    class Entity *teammaster;

    class Entity *bindmaster;
    qboolean      bind_use_my_angles;

    qboolean detach_at_death;

    BindInfo();

    void Archive(Archiver& arc);
};

inline BindInfo::BindInfo()
{
    int i;

    numchildren = 0;

    for (i = 0; i < MAX_MODEL_CHILDREN; i++) {
        children[i] = ENTITYNUM_NONE;
    }

    detach_at_death = true;

    teamchain  = NULL;
    teammaster = NULL;

    bindmaster = NULL;

    bind_use_my_angles = false;
}

inline void BindInfo::Archive(Archiver& arc)
{
    arc.ArchiveInteger(&numchildren);
    arc.ArchiveRaw(children, sizeof(children));
    arc.ArchiveString(&moveteam);
    arc.ArchiveObjectPointer((Class **)&teamchain);
    arc.ArchiveObjectPointer((Class **)&teammaster);
    arc.ArchiveObjectPointer((Class **)&bindmaster);
    arc.ArchiveBoolean(&bind_use_my_angles);
    arc.ArchiveBoolean(&detach_at_death);
}

BindInfo *CreateBindInfo(void);

extern Event EV_Morph;
extern Event EV_Unmorph;
extern Event EV_MorphControl;

typedef struct {
    int      index;           
    float    current_percent; 
    float    speed;           
    float    final_percent;   
    qboolean return_to_zero;  
    int      channel;         
} morph_t;

typedef enum {
    MORPH_CHAN_NONE,
    MORPH_CHAN_MOUTH,
    MORPH_CHAN_BROW,
    MORPH_CHAN_LEFT_BROW,
    MORPH_CHAN_RIGHT_BROW,
    MORPH_CHAN_EYES,
    MORPH_CHAN_LEFT_LID,
    MORPH_CHAN_RIGHT_LID
} morphChannel_t;

class MorphInfo
{
public:
    void Archive(Archiver&);
    MorphInfo();

    morph_t  controllers[NUM_MORPH_CONTROLLERS];
    qboolean controller_on;

    // SDK entity.h:367.  MorphInfo does NOT derive from Class, so it cannot go through
};

inline MorphInfo::MorphInfo()
{
    for (int i = 0; i < NUM_MORPH_CONTROLLERS; i++) {
        controllers[i].index           = -1;
        controllers[i].current_percent = 0.0f;
        controllers[i].speed           = 0.0f;
        controllers[i].final_percent   = 0.0f;
        controllers[i].return_to_zero  = false;
        controllers[i].channel         = MORPH_CHAN_NONE;
    }

    controller_on = false;
}

inline void MorphInfo::Archive(Archiver& arc)
{
    int i;

    for (i = 0; i < NUM_MORPH_CONTROLLERS; i++) {
        arc.ArchiveInteger(&controllers[i].index);
        arc.ArchiveFloat(&controllers[i].current_percent);
        arc.ArchiveFloat(&controllers[i].speed);
        arc.ArchiveFloat(&controllers[i].final_percent);
        arc.ArchiveBoolean(&controllers[i].return_to_zero);
        arc.ArchiveInteger(&controllers[i].channel);
    }

    arc.ArchiveBoolean(&controller_on);
}

MorphInfo *CreateMorphInfo(void);

// retail-exact requirement the port can meet is the ORDER, not the numbers.
class Entity : public Listener
{
private:
    Vector               _localOrigin;
    Container<EntityPtr> _lastTouchedList;
    bool                 _fulltrace;
    int                  _groupID;
    str                  _archetype;
    bool                 _missionObjective;
    str                  _targetPos;
    bool                 _networkDetail;

protected:
    // SDK entity.h:388-389.  Creates the UseData record if there is not one already.
    void buildUseData(void);

public:
    CLASS_PROTOTYPE(Entity);

    void turnThinkOn(void) { flags |= FL_THINK; }
    void turnThinkOff(void) { flags &= ~FL_THINK; }
    bool isThinkOn(void) { return (flags & FL_THINK) ? true : false; }

    int        entnum;
    gentity_t *edict;
    gclient_t *client;
    int        spawnflags;

    str model;

    Vector total_delta;  
    Vector mins;
    Vector maxs;
    Vector absmin;
    Vector absmax;
    Vector centroid;    
    Vector velocity;
    Vector avelocity;
    Vector origin;
    Vector angles;
    Vector size;
    int    movetype;
    int    mass;
    float  gravity;     
    float  orientation[3][3];
    Vector localangles;

    gentity_t *groundentity;
    cplane_t   groundplane;
    int        groundcontents;

    int numsurfaces;

    float lightRadius;

    str target;
    str targetname;
    str killtarget;

    float health;
    float max_health;
    int   deadflag;
    int   flags;

    int watertype;
    int waterlevel;

    damage_t takedamage;
    int      damage_type;

    qboolean look_at_me;
    bool     projectilesCanStickToMe;

    str explosionModel;

    ScriptVariableList entityVars;

    unsigned int _affectingViewModes;

    Vector watch_offset;

    Animate                  *animate;
    Mover                    *mover;
    BindInfo                 *bind_info;
    MorphInfo                *morph_info;
    Program                  *ObjectProgram;
    DamageModificationSystem *damageModSystem;
    UseData                  *useData;

    const Vector &GetLocalOrigin(void) const { return _localOrigin; }
    void SetLocalOrigin(const Vector &localOrigin) { _localOrigin = localOrigin; }

    float getHealth(void) { return health; }  




public:
    void IncreaseShotCount(Event *ev);
    void ProjectilesCanStickToMe(Event *ev);
    void SetCinematicAnim(const str &AnimName);
    void SetCinematicAnim(Event *ev); 
    void CinematicAnimDone(void);
    void CinematicAnimDone(Event *ev); 
    void SetObjectProgram(Event *ev);
    void ExecuteProgram(Event *ev);
    void ProjectileAtk(Event *ev);
    void ProjectileAttackPoint(Event *ev);
    void ProjectileAttackFromTag(Event *ev);
    void ProjectileAttackFromPoint(Event *ev);
    void TraceAtk(Event *ev);
    void SetWatchOffset(Event *ev);


public:
    Entity();
    explicit Entity(int create_flag);
    virtual ~Entity() override;

    bool hasUseData(void) const { return useData != NULL; }

    void useDataAnim(Event *ev);
    void useDataType(Event *ev);
    void useDataThread(Event *ev);
    void useDataEvent(Event *ev);
    void useDataMaxDist(Event *ev);
    void useDataCount(Event *ev);

    void LookAtMe(Event *ev);

    virtual void PassToAnimate(Event *ev);

    void ClassnameEvent(Event *ev);
    void SpawnFlagsEvent(Event *ev);


    qboolean DistanceTo(Vector pos) const;
    qboolean DistanceTo(Entity *ent) const;
    qboolean WithinDistance(Vector pos, float dist) const;
    qboolean WithinDistance(Entity *ent, float dist) const;

    Vector   GetControllerAngles(int num);
    void     SetControllerAngles(int num, const vec3_t angles);
    void     simplePlayDialog(Event *ev); 
    void     SetControllerTag(int num, int tag_num);
    qboolean IsTouching(const Entity *e1);  
    qboolean isBoundTo(const Entity *master);  
    virtual qboolean HitSky(void);  

    void        SetKillTarget(const char *killtarget);
    const char *KillTarget(void);


    virtual void     setModel(const char *model);
    void             setModel(const str &mdl);
    virtual void setModel(void);
    void             SetSize(void);
    void             SetModelEvent(Event *ev);

    void         SetTeamEvent(Event *ev);
    virtual void TriggerEvent(Event *ev);
    void         hideModel(void);
    void         EventHideModel(Event *ev);
    virtual void showModel(void);
    void         EventShowModel(Event *ev);
    qboolean     hidden(void);
    void         ProcessInitCommands(void);
    void         ProcessInitCommands(int index, qboolean cache_only = qfalse);  

    void  setAlpha(float alpha);
    float alpha(void);

    void setMoveType(int type);
    int  getMoveType(void);

    void setSolidType(solid_t type);
    int  getSolidType(void);


    virtual Vector getParentVector(Vector vec);
    Vector         getLocalVector(Vector vec);

    virtual void setSize(Vector min, Vector max);
    virtual void setOrigin(const Vector &org); 
    virtual void setOrigin(void);
    virtual void setOriginEveryFrame(Event *ev);
    virtual void addOrigin(const Vector &org); 

    void     GetRawTag(int tagnum, orientation_t *orient, bodypart_t part = legs);
    qboolean GetRawTag(const char *tagname, orientation_t *orient, bodypart_t part = legs);

    void     GetTag(int tagnum, orientation_t *orient);
    qboolean GetTag(const char *name, orientation_t *orient);
    void     GetTag(int tagnum, Vector *pos, Vector *forward = NULL, Vector *left = NULL, Vector *up = NULL);
    qboolean GetTag(const char *name, Vector *pos, Vector *forward = NULL, Vector *left = NULL, Vector *up = NULL);

    virtual int   CurrentAnim(int slot = 0) const;




    virtual void setAngles(Vector ang);
    virtual void setAngles(void);  

    void link(void);
    void unlink(void);

    void setContents(int type);
    int  getContents(void);
    void setScale(float scale);

    qboolean droptofloor(float maxfall);
    qboolean isClient(void);

    virtual void SetDeltaAngles(void);
    void         RegisterAlias(Event *ev);         
    void         RegisterAliasAndCache(Event *ev); 
    virtual void DamageEvent(Event *event);
    void         Damage(
                Entity *inflictor,
                Entity *attacker,
                float damage,
                const Vector &position,
                const Vector &direction,
                const Vector &normal,
                int knockback,
                int flags,
                int meansofdeath,
                int surface_number = -1,
                int bone_number = -1,
                Entity *weapon = 0);

    void     DamageType(Event *ev);
    // virtual, so the Entity vtable referenced a body that did not exist -- that is the
    void         FadeNoRemove(Event *ev);
    void         FadeOut(Event *ev);
    void         FadeIn(Event *ev);
    void         Fade(Event *ev);
    void         AddDamageModifier(Event *ev);
    void         ResolveDamage(::Damage& damage);
    const str    getArchetype() const; 

    str  getTargetPos();
    void setTargetPos(const str& targetPos) { _targetPos = targetPos; }

    virtual void processGameplayData(Event *ev) {}


    void         SetUserVar1(Event *ev);
    void         SetUserVar2(Event *ev);
    void         SetUserVar3(Event *ev);
    void         SetUserVar4(Event *ev);
    void         RemoveVariable(Event *ev);
    void         SetFloatVar(Event *ev);
    void         SetVectorVar(Event *ev);
    void         SetStringVar(Event *ev);
    void         GetFloatVar(Event *ev);
    void         GetVectorVar(Event *ev);
    void         GetStringVar(Event *ev);
    void         GetTargetEntity(Event *ev); 

    void SetMaxHealth(Event *ev);
    void setMaxHealth(float newMaxHealth) { max_health = newMaxHealth; }  
    float getMaxHealth(void) { return max_health; }  
    void GetModelName(Event *ev);
    void setMask(Event *ev);         
    void DoRadiusDamage(Event *ev);  
    void setNetworkDetail(Event *ev);
    bool isNetworkDetail(void);

    void SetFullTraceEvent(Event *ev);

    void setFullTrace(bool fulltrace) { _fulltrace = fulltrace; }

    bool usesFullTrace(void) { return _fulltrace; }

    void         DisplayEffect(Event *ev);
    void         attachEffect(const str& modelName, const str& tagName, float removeTime);

    void AddToGroup(int ID);
    void SetGroupID(Event *ev);
    void SetGroupID(int ID) { _groupID = ID; }
    int  GetGroupID() const { return _groupID; }
    Container<EntityPtr>& GetLastTouchedList() { return _lastTouchedList; }

    void         hideFeaturesForFade(void);
    void         showFeaturesForFade(void);
    void         ForceAlpha(Event *ev);


    void SelfDetonate(Event *ev);
    void SetEntityExplosionModel(Event *ev);

    void    SpawnEffect(Event *ev);
    void    CreateEarthquake(Event *ev);  
    void    warp(Event *ev);                   
    void    ProjectileAttackEntity(Event *ev);  
    void    traceHitsEntity(Event *ev);        
    Entity *SpawnEffect(const str& name, const Vector& origin, const Vector& angles, float removeTime);

    void addHealthOverTime(Event *ev);
    void addHealth(float healthToAdd, float maxHealth = 0.0f);
    void setHealth(float newHealth) { health = newHealth; };

    void getCustomShaderInfo(const str& customShader, str& shaderName, str& soundName);
    void setCustomShader(const char *customShader);
    void clearCustomShader(const char *customShader = NULL);
    void startStasis(void);
    void startStasis(Event *ev); 
    void stopStasis(void);
    void stopStasis(Event *ev); 
    bool hasCustomShader(const char *customShader = NULL);

    void setCustomEmitter(const char *customEmitter);
    void clearCustomEmitter(const char *customEmitter = NULL);

    void affectingViewMode(Event *ev);
    void addAffectingViewModes(unsigned int mask);
    void removeAffectingViewModes(unsigned int mask);


    virtual void     CheckGround(void);
    virtual qboolean HitSky(trace_t *trace);

    void BecomeSolid(Event *ev);
    void BecomeNonSolid(Event *ev);
    void SetMins(Event *ev);
    void SetMaxs(Event *ev);
    void GetMins(Event *ev);
    void GetMaxs(Event *ev);
    void SetScale(Event *ev);
    void SetAlpha(Event *ev);
    void TouchTriggersEvent(Event *ev);

    str  GetRandomAlias(const str &name);  
    void SetWaterType(void);

    qboolean
    attach(int parent_entity_num, int tag_num, qboolean use_angles = qtrue, Vector attach_offset = Vector("0 0 0"), Vector angles_offset = Vector("0 0 0"));
    void detach(void);

    void         Sound(Event *ev);
    virtual void Sound(
        str     sound_name,
        int     channel       = CHAN_BODY,
        float   volume        = -1.0,
        float   min_dist      = -1.0,
        Vector *origin        = NULL,
        float   pitch         = -1.0f,
        int     argstype      = 0,
        int     checkSubtitle = 1,
        float   max_dist      = -1.0f
    );
    void StopSound(int channel);
    void LoopSound(Event *ev);
    void LoopSound(const str &sound_name, float volume = -1.0f, float min_dist = -1.0f);
    void StopLoopSound(Event *ev);

    void SetLight(Event *ev);
    void LightOn(Event *ev);
    void LightOff(Event *ev);
    void LightRed(Event *ev);
    void LightGreen(Event *ev);
    void LightBlue(Event *ev);
    void LightRadius(Event *ev);
    void LightStyle(Event *ev);
    void Flags(Event *ev);
    void Effects(Event *ev);
    void RenderEffects(Event *ev);
    void SVFlags(Event *ev);

    void         BroadcastSound(float pos = SOUND_RADIUS, int soundType = SOUNDTYPE_GENERAL);
    float        ModifyFootstepSoundRadius(float radius, int soundTypeIdx);
    void         Kill(Event *ev);
    void         SurfaceModelEvent(Event *ev);
    void         SurfaceCommand(const char *surf_name, const char *token);
    virtual void Postthink(void);
    virtual void Think(void);

    void DamageSkin(trace_t *trace, float damage);

    void AttachEvent(Event *ev);
    void AttachModelEvent(Event *ev);
    void SetAnimOnAttachedModel(Event *ev); 
    void RemoveAttachedModelEvent(Event *ev);
    void DetachEvent(Event *ev);
    void TakeDamageEvent(Event *ev);
    void NoDamageEvent(Event *ev);
    void Gravity(Event *ev);
    void UseBoundingBoxEvent(Event *ev);
    void HurtEvent(Event *ev);
    void SetMassEvent(Event *ev);
    void Censor(Event *ev);
    void Ghost(Event *ev);

    void StationaryEvent(Event *ev);
    void Explosion(Event *ev);

    void Shader(Event *ev);

    void KillAttach(Event *ev);

    void DropToFloorEvent(Event *ev);

    void         joinTeam(Entity *teammember);
    void         quitTeam(void);
    virtual void bind(Entity *master, qboolean use_my_angles = qfalse);
    virtual void unbind(void);

    void JoinTeam(Event *ev);
    void EventQuitTeam(Event *ev);
    void BindEvent(Event *ev);
    void EventUnbind(Event *ev);
    void AddToSoundManager(Event *ev);
    void NoLerpThisFrame(void);

    virtual void addAngles(Vector add);


    void DetachAllChildren(Event *ev);

    virtual void VelocityModified(void);


    void GetVelocity(Event *ev);
    void SetVelocity(Event *ev);





    void             Archive(Archiver& arc) override;

    int CurrentFrame(int part = 0) const;  
    void             SetHealth(Event *ev);
    void             GetHealth(Event *ev);


    const char   *Target();  
    virtual const str getName(void) const;  
    const char   *TargetName();
    void          SetTargetName(const char *target);
    void          SetTargetName(Event *ev);  
    qboolean      Targeted();
    void           SetTarget(const char *target);  
    void           SetTarget(Event *ev);
    void           SetAngleEvent(Event *ev);
    void           SetAngles(Event *ev);
    void           GetAngles(Event *ev);
    void           SetOrigin(Event *ev);
    void           GetOrigin(Event *ev);




    void     MorphEvent(Event *ev);
    void     UnmorphEvent(Event *ev);
    void     MorphControl(Event *ev);
    int      GetMorphChannel(const char *morph_name);
    void     StartMorphController(void);
    qboolean MorphChannelMatches(int morph_channel1, int morph_channel2);


    void TikiTodo(Event *ev);
    void TikiNote(Event *ev);

    void Cache(Event *ev);


    void IfSkillEvent(Event *ev);

    void MultiplayerEvent(Event *ev);

    void ProcessInitCommandsEvent(Event *ev);

    void setArchetype(Event *ev);

    void setGameplayHealth(Event *ev);

    void setMissionObjective(Event *ev);

    void setRandomScale(Event *ev);

    void clearDisplayEffects(void);

    void Stun(float time);

    void doesVarExist(Event *ev);

    void getTarget(Event *ev);
    void setTargetPos(Event *ev);

    void isWithinDistanceOf(Event *ev);

    Entity *SpawnSound(const str &sound, const Vector &pos, float volume, float removeTime);

    Entity *FindEntityByName(const str &entityName);

    void SetEntNum(int num);

    void Setup(void);

    qboolean CanDamage(const Entity *target, const Entity *skip_ent);

    void removeAttachedModelByTargetname(Event *ev);

    Vector GetClosestCorner(const Vector &position);

    void setViewModel(const char *mdl);

    qboolean GlobalAliasExists(const char *name);
    qboolean AliasExists(const char *name);
    const Vector InterceptTarget(const Vector &targetPosition, const Vector &targetVelocity, const float maxSpeed) const;
    const Vector InterceptTargetXY(const Vector &targetPosition, const Vector &targetVelocity, const float maxSpeed) const;
    bool WithinDistanceXY(const Vector &pos, float dist);
    bool WithinDistanceXY(const Entity *ent, float dist);


public:
    void setCustomShader( Event *ev ); 
    void clearCustomShader( Event *ev ); 
    void setCustomEmitter( Event *ev ); 
    void clearCustomEmitter( Event *ev ); 
    void StopLoopSound( void ); 
    void StopSound( Event *ev ); 
    void SetSize( Event *ev ); 
    void SetKillTarget( Event *ev ); 
    void SetControllerAngles( Event *ev ); 
    void BroadcastSound( Event *ev ); 
    void DeathSinkStart( Event *ev ); 
    void DeathSink( Event *ev ); 
    void setMoveType( Event *ev ); 
    void Contents(Event* ev); 
    void removeAttachedModelByTargetname( const str &targetNameToRemove ); 
    void GetRawTargetName( Event *ev ); 
    void GetTargetName( Event *ev ); 
};


inline int Entity::getSolidType(void)
{
    return edict->solid;
}

inline qboolean Entity::DistanceTo(Vector pos) const
{
    Vector delta;

    delta = origin - pos;
    return delta.length() != 0 ? qtrue : qfalse;
}

inline qboolean Entity::DistanceTo(Entity *ent) const
{
    Vector delta;

    assert(ent);

    if (!ent) {
        return 999999;
    }

    delta = origin - ent->origin;
    return delta.length() != 0 ? qtrue : qfalse;
}

inline qboolean Entity::WithinDistance(Vector pos, float dist) const
{
    Vector delta;

    delta = origin - pos;

    return ((delta * delta) < (dist * dist)) ? qtrue : qfalse;
}

inline qboolean Entity::WithinDistance(Entity *ent, float dist) const
{
    Vector delta;

    assert(ent);

    if (!ent) {
        return false;
    }

    delta = origin - ent->origin;

    return ((delta * delta) < (dist * dist));
}

inline const char *Entity::KillTarget(void)
{
    return killtarget.c_str();
}

inline qboolean Entity::hidden(void)
{
    if (edict->s.renderfx & RF_DONTDRAW) {
        return true;
    }
    return false;
}



inline void Entity::hideModel(void)
{
    edict->s.renderfx |= RF_DONTDRAW;
    if (getSolidType() <= SOLID_TRIGGER) {
        edict->svflags |= SVF_NOCLIENT;
    }
}

inline void Entity::showModel(void)
{
    edict->s.renderfx &= ~RF_DONTDRAW;
    edict->svflags &= ~SVF_NOCLIENT;
}

inline float Entity::alpha(void)
{
    return edict->s.alpha;
}

inline void Entity::setMoveType(int type)
{
    movetype = type;
}

inline int Entity::getMoveType(void)
{
    return movetype;
}

inline void Entity::unlink(void)
{
    gi.unlinkentity(edict);
}

inline void Entity::setContents(int type)
{
    edict->contents = type;
}

inline int Entity::getContents(void)
{
    return edict->contents;
}

inline qboolean Entity::isClient(void)
{
    if (client) {
        return true;
    }
    return false;
}

inline void Entity::SetDeltaAngles(void)
{
    int i;

    if (client) {
        for (i = 0; i < 3; i++) {
            client->ps.delta_angles[i] = ANGLE2SHORT(client->ps.viewangles[i]);
        }
    }
}

inline str Entity::GetRandomAlias(const str &name)
{
    str         realname;
    const char *s;

    s = gi.Alias_FindRandom(edict->s.modelindex, name.c_str());
    if (s) {
        realname = s;
    } else {
        s = gi.GlobalAlias_FindRandom(name.c_str());
        if (s) {
            realname = s;
        }
    }

    return realname;
}

inline qboolean Entity::GlobalAliasExists(const char *name)
{
    return gi.GlobalAlias_FindRandom(name) != NULL;
}

inline qboolean Entity::AliasExists(const char *name)
{
    return gi.Alias_FindRandom(edict->s.modelindex, name) != NULL;
}

inline const Vector Entity::InterceptTarget(
    const Vector &targetPosition, const Vector &targetVelocity, const float maxSpeed) const
{
    Vector myPosition(origin);
    Vector predictedPosition = targetPosition + targetVelocity * ( Vector::Distance(targetPosition, myPosition) / maxSpeed );
    Vector desiredDirection  = predictedPosition - myPosition;

    return desiredDirection.toAngles();
}

inline const Vector Entity::InterceptTargetXY(
    const Vector &targetPosition, const Vector &targetVelocity, const float maxSpeed) const
{
    Vector myPosition(origin);
    float  distanceToTargetPosition = Vector::Distance(targetPosition, myPosition);
    if ( !fSmallEnough( distanceToTargetPosition, 0.1f ) )
    {
        Vector predictedPosition = targetPosition + targetVelocity * ( distanceToTargetPosition / maxSpeed );
        Vector desiredDirection  = predictedPosition - myPosition;
        desiredDirection.z = 0.0f;
        return desiredDirection.toAngles();
    }
    return angles;
}

inline bool Entity::WithinDistanceXY(const Vector &pos, float dist)
{
    Vector delta;
    float  d;

    delta = pos - origin;
    delta[2] = 0.0f;
    d = delta.length();

    return (d <= dist);
}

inline bool Entity::WithinDistanceXY(const Entity *ent, float dist)
{
    return WithinDistanceXY(ent->origin, dist);
}

#include "worldspawn.h"

#include "simpleentity.h"
