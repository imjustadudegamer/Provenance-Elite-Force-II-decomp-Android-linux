
#pragma once

#include "g_local.h"
#include "../corepp/vector.h"
#include "entity.h"
#include "weapon.h"
#include "sentient.h"

extern Event EV_Player_RemovePowerup;  
extern Event EV_Player_SetStat;  
extern Event EV_Player_UseItem;
#include "navigate.h"
#include "misc.h"
#include "bspline.h"
#include "camera.h"
#include "specialfx.h"
#include "characterstate.h"
#include "playerheuristics.h"
#include "actor.h"
#include "vehicle.h"
#include "powerups.h"

extern Event EV_Player_EndLevel;
extern Event EV_Player_MissionFailed;  
extern Event EV_Player_GiveCheat;
extern Event EV_Player_GodCheat;
extern Event EV_Player_NoTargetCheat;
extern Event EV_Player_NoClipCheat;
extern Event EV_Player_GameVersion;
extern Event EV_Player_Fov;
extern Event EV_Player_WhatIs;
extern Event EV_Player_Respawn;
extern Event EV_Player_WatchActor;
extern Event EV_Player_StopWatchingActor;
extern Event EV_Player_DoStats;
extern Event EV_Player_EnterIntermission;
extern Event EV_Player_StuffText;
extern Event EV_Weapon_TargetIdleThink;  
extern Event EV_ViewThing_SpawnFromTS;   

enum painDirection_t {
    PAIN_NONE,
    PAIN_FRONT,
    PAIN_LEFT,
    PAIN_RIGHT,
    PAIN_REAR
};

typedef enum jailstate_e {
    JAILSTATE_NONE,
    JAILSTATE_ESCAPE,
    JAILSTATE_ASSIST_ESCAPE
} jailstate_t;

typedef void (Player::*movecontrolfunc_t)(void);


#define MAX_ANIM_SLOT         16
class Powerup;
class Rune;
class HoldableItem;

typedef enum {
    PLAYER_CAMERA_MODE_NORMAL,
    PLAYER_CAMERA_MODE_ACTOR,
    PLAYER_CAMERA_MODE_ENTITY_WATCHING,
    PLAYER_CAMERA_MODE_NO_CLIP,
    PLAYER_CAMERA_MODE_CINEMATIC
} playerCameraMode_t;

const float standardTorsoMult = 0.75f;

// must exist and its Archive() must be byte-identical to retail's.
class FinishingMove : public Class
{
public:
    FinishingMove() { direction = 0; coneangle = 0.0f; distance = 0.0f; chance = 0.0f; enemyyaw = 0.0f; }
    ~FinishingMove() {}

    str   statename;
    int   direction;
    float coneangle;
    float distance;
    float chance;
    float enemyyaw;

    void Archive(Archiver& arc);
};

inline void FinishingMove::Archive(Archiver& arc)
{
    Class::Archive(arc);

    arc.ArchiveString(&statename);
    arc.ArchiveInteger(&direction);
    arc.ArchiveFloat(&coneangle);
    arc.ArchiveFloat(&distance);
    arc.ArchiveFloat(&chance);
    arc.ArchiveFloat(&enemyyaw);
}

class WeaponSetItem : public Class
{
public:
    void Archive(Archiver&) ;

    str          name;
    weaponhand_t hand;

};

inline void WeaponSetItem::Archive(Archiver& arc)
{
    Class::Archive(arc);

    arc.ArchiveString(&name);
    ArchiveEnum(hand, weaponhand_t);
}

class Player : public Sentient
{
    friend class Camera;
    friend class Vehicle;
    friend class HorseVehicle;  

private:
    static movecontrolfunc_t MoveStartFuncs[];

    StateMap *statemap_Legs;
    StateMap *statemap_Torso;

    State *currentState_Legs;
    State *currentState_Torso;

    str   last_torso_anim_name;
    str   last_leg_anim_name;
    str   partAnim[3];

    bool                     animdone_Legs;
    bool                     animdone_Torso;
    Container<Conditional *> legs_conditionals;
    Container<Conditional *> torso_conditionals;

    movecontrol_t movecontrol;
    int           last_camera_type;

    Vector oldvelocity;
    Vector old_v_angle;
    Vector oldorigin;
    float  animspeed;
    float  airspeed;

    float    blend[4];
    float    fov;

    Vector v_angle;

    bool   _onLadder;
    Vector _ladderNormal;
    float  _ladderTop;
    float  _nextLadderTime;

public:
    PlayerHeuristics *p_heuristics;

    void SwingAngles(float destination, float swingTolerance, float clampTolerance, float speed, float *angle, qboolean *swinging);
    Actor *getBestActorToUse(int *entityList, int count);  
    void   useHoldableItem(void);                         
    void   removeHoldableItem(void);                      
    void showObjectInfo(void);        
    void clearActionType(void);       
    void handleUseEntity(Entity *ent, float real_dist);  
    void handleUseObject(UseObject *uo);                 
    void AcquireHeadTarget(void);             
    void HandleFinishableList(void);         
    Entity *FindHeadTarget(const Vector &startOrigin, const Vector &viewForward, const float fovDegrees, const float maxDist);  
    Entity *FindClosestEntityInRadius(const float horizontalFOVDegrees, const float verticalFOVDegrees, const float maxDistance);  
    Entity const *GetTarget(void) const;      

    bool   dont_turn_legs;

    int buttons;
    int new_buttons;

    float respawn_time;
    int   last_attack_button;

    float  damage_blood;
    float  damage_alpha;
    Vector damage_blend;
    Vector damage_from;
    Vector damage_angles;
    float  damage_count;
    float  next_painsound_time;
    float  air_finished;    
    float  next_drown_time; 
    float  drown_damage;    
    str    waitForState;

    SafePtr<Camera> camera;
    SafePtr<Camera> actor_camera;

    bool _cameraCutThisFrame;

    bool _updateGameFrames;
    int  _totalGameFrames;
    int  _secretsFound;
    int  _skillLevel;

    int _itemIcon;
    str _itemText;


    float action_level;
    int   music_current_mood;
    int   music_fallback_mood;
    float music_current_volume;
    float music_saved_volume;
    float music_volume_fade_time;

    int   reverb_type;
    float reverb_level;

    qboolean        gibbed;
    float           pain;
    painDirection_t pain_dir;
    meansOfDeath_t  pain_type;
    bool            take_pain;
    float           nextpaintime;
    bool            knockdown;

    bool   canfall;
    bool   falling;
    int    feetfalling;
    Vector falldir;

    bool hardimpact;

    qboolean music_forced;

    usercmd_t  last_ucmd;

    float  animheight;
    Vector yaw_forward;
    Vector yaw_left;

    SafePtr<Entity> atobject;
    float           atobject_dist;
    Vector          atobject_dir;

    SafePtr<Entity> toucheduseanim;
    int             useanim_numloops;
    SafePtr<Entity> useitem_in_use;

    SafePtr<Entity> _targetedEntity;

    // and did not exist in this port, which is why a retail .sav could never be read
    // Some are not yet consumed by gameplay code -- retail archives several fields it
    EntityPtr                  head_target;                   
    float                      look_at_time;                 
    EntityPtr                  targetEnemy;                  
    bool                       targetEnemyLocked;            
    qboolean                   shield_active;                

    qboolean                   dual_wield_active;            
    Container<WeaponSetItem *> dual_wield_weaponlist;        
    WeaponPtr                  holsteredWeapons[MAX_ACTIVE_WEAPONS]; 

    str                        currentCallVolume;            
    float                      _userFovChoice;               
    float                      userFov;                       
    qboolean                   mp_savingDemo;                 
    ActiveWeapon               newActiveWeapon;              
    SafePtr<Vehicle>           vehicle;                      

    float                      bonus_alpha;                  
    int                        old_waterlevel;               
    float                      _flashAlpha;                  
    Vector                     _flashBlend;                  
    float                      _flashMaxTime;                
    float                      _flashMinTime;                

    CameraPtr                  cool_camera;                  
    EntityPtr                  entity_to_watch;              
    float                      maximumAngleToWatchedEntity;  
    bool                       watchEntityForEntireDuration; 
    int                        playerCameraMode;             

    bool                       _allowMusicDucking;           
    bool                       _allowActionMusic;            
    float                      accumulated_pain;             

    Vector                     base_rightfoot_pos;           
    Vector                     base_leftfoot_pos;            
    Vector                     rightfoot_pos;                
    Vector                     leftfoot_pos;                 
    int                        pm_lastruntime;               
    EntityPtr                  cool_item;                    
    str                        cool_dialog;                  
    str                        cool_anim;                    
    qboolean                   do_rise;                      
    qboolean                   weapons_holstered_by_code;    
    qboolean                   projdetonate;                 

    qboolean                   yawing;                       
    qboolean                   yawing_left;                  
    qboolean                   yawing_right;                 
    qboolean                   adjust_torso;                 
    Vector                     torsoAngles;                  
    inline Vector                GetTorsoAngles(void) { return torsoAngles; }  
    Vector                     headAngles;                   
    Vector                     headAimAngles;                
    Vector                     torsoAimAngles;               

    qboolean                   fakePlayer_active;            
    ActorPtr                   fakePlayer;                   

    float                      specialMoveCharge;            
    float                      specialMoveEndTime;           
    float                      specialMoveChargeTime;        
    int                        points;                       
    float                      playerKnockback;              
    float                      knockbackMultiplier;          
    bool                       changedStanceTorso;           
    bool                       changedStanceLegs;            
    int                        stanceNumber;                 
    bool                       incomingMeleeAttack;          
    float                      bendTorsoMult;                
    str                        lastActionType;               
    int                        meleeAttackFlags;             
    bool                       changingChar;                 

    Container<EntityPtr>       meleeAttackerList;            
    Container<ActorPtr>        finishableList;               
    Container<FinishingMove *> finishingMoveList;            

    SafePtr<Powerup>           _powerup;                     
    SafePtr<Rune>              _rune;                        
    SafePtr<HoldableItem>      _holdableItem;                

    float                      _nextEnergyTransferTime;      
    bool                       _canTransferEnergy;           
    bool                       _doDamageScreenFlash;         
    bool                       _isThirdPerson;               

    Actor                     *_finishActor;                 
    str                        _finishState;                 
    bool                       _doingFinishingMove;          

    bool                       _usingEntity;                 
    str                        _attackType;                  
    int                        _gameplayAnimIdx;             
    bool                       _disableUseWeapon;            
    EntityPtr                  _targetSelectedHighlight;     
    EntityPtr                  _targetLockedHighlight;       

    bool                       _infoHudOn;                   
    int                        _nextRegenTime;               
    float                      _useEntityStartTimer;         

    int                        _objectiveNameIndex;          
    unsigned int               _objectiveStates;             
    unsigned int               _informationStates;           

    int                        _dialogEntnum;                
    int                        _dialogSoundIndex;            
    int                        _dialogTextSoundIndex;        

    float                      _crossHairXOffset;            
    float                      _crossHairYOffset;            
    float                      _lastDamagedTimeFront;        
    float                      _lastDamagedTimeBack;         
    float                      _lastDamagedTimeLeft;         
    float                      _lastDamagedTimeRight;        

    float                      _nextPainShaderTime;          
    meansOfDeath_t             _lastPainShaderMod;           

    str                        _voteText;                    
    bool                       _validPlayerModel;            

    Container<str>             _hudList;                     
    bool                       _needToSendHuds;              
    bool                       _started;                     

    pmtype_t                   _forcedMoveType;              
    ActorPtr                   _branchDialogActor;           
    bool                       _needToSendBranchDialog;      

    int   moveresult;

    float damage_multiplier;




public:


public:
    void SetCurrentCallVolume(const str& volumeName);
    str  GetCurrentCallVolume();



private:

    bool _autoSwitchWeapons;

public:  





    void StartPush(void);
    void StartUseAnim(void);
    void StartLoopUseAnim(void);
    void SetupUseObject(void);

    void StartUseObject(Event *ev);
    void FinishUseObject(Event *ev);
    void FinishUseAnim(Event *ev);
    void Turn(Event *ev);
    void TurnUpdate(Event *ev);
    void TurnLegs(Event *ev);
    void DontTurnLegs(Event *ev); 

    CLASS_PROTOTYPE(Player);

    Player();
    ~Player();
    void Init(void);

    void InitSound(void);
    void InitEdict(void);
    void InitClient(void);
    void InitPhysics(void);
    void InitWorldEffects(void);
    void InitWeapons(void);
    void InitView(void);
    void InitModel(const char *modelName = NULL);  
    void InitState(void);
    void InitHealth(void);
    void ChooseSpawnPoint(void);

    void EndLevel(Event *ev);
    void Respawn(Event *ev);


    void setItemText(int itemIcon, const str& itemText);
    void clearItemText(void);


    virtual void setViewMode(const str& viewModeName) override;  

    qboolean ShouldSendToClient(Entity *entityToSend);

    void LoadObjectives(Event *ev);     
    void SetObjectiveShow(Event *ev);   
    void SetObjectiveComplete(Event *ev); 
    void SetObjectiveFailed(Event *ev); 
    void SetInformationShow(Event *ev); 

    void SetDeltaAngles(void) override;
    void setAngles(Vector ang) override;

    void DoUse(Event *ev);
    void Killed(Event *ev);
    void Dead(Event *ev);
    void MissionFailed(Event *ev);
    void setMissionFailed(void);
    void Pain(Event *ev);

    void TouchStuff(pmove_t *pm);


    void     GetMoveInfo(pmove_t *pm);
    void     SetMoveInfo(pmove_t *pmove, const usercmd_t *ucmd); 
    pmtype_t GetMovePlayerMoveType(void);
    void     ClientMove(usercmd_t *ucmd);
    void     ClientMoveDuck(usercmd_t *ucmd);   
    void     ClientMoveLadder(usercmd_t *ucmd); 
    void     touchingLadder(Trigger *ladder, const Vector& normal, float top); 
    void     CheckMoveFlags(void);

    void    GetViewTrace(trace_t& trace, int contents, float maxDistance = 3000.0f);
    void    CheckForTargetedEntity(void);
    void    SetTargetedEntity(Entity *entity);
    Entity *GetTargetedEntity(void) const;
    void    ProcessTargetedEntity(void);

    void LoadStateTable(void);
    void cinematicStarted(void);
    void cinematicStopped(void);
    void ResetState(Event *ev);
    void EvaluateState(State *forceTorso = NULL, State *forceLegs = NULL);

    void     CheckGround(void) override;
    void     UpdateViewAngles(usercmd_t *cmd);
    qboolean AnimMove(Vector& move, Vector *endpos = NULL);
    qboolean TestMove(Vector& pos, Vector *endpos = NULL);
    qboolean CheckMove(Vector& move, Vector *endpos = NULL);


    void EndAnim_Legs(Event *ev);
    void EndAnim_Torso(Event *ev);
    void TouchedUseAnim(Entity *ent);

    void AnimateWeapon(Event *ev); 
    void loadUseItem(const str& item);

    void GiveCheat(Event *ev);
    void GiveWeaponCheat(Event *ev);
    void GiveAllCheat(Event *ev);
    void GodCheat(Event *ev);
    void NoTargetCheat(Event *ev);
    void NoclipCheat(Event *ev);
    void Kill(Event *ev);
    void SpawnEntity(Event *ev);
    void SpawnActor(Event *ev);

    void GameVersion(Event *ev);

    void Fov(Event *ev);
    void GetPlayerView(Vector *pos, Vector *angle);

    float CalcRoll(void);
    void  WorldEffects(void);
    void  AddBlend(float r, float g, float b, float a);
    void  SetBlend(float r, float g, float b, float a, int additive);  
    void  SetBlend(int additive);  
    void      UpdateStats(void);
    usercmd_t GetLastUcmd(void);
    bool      hasRune(void);
    const str getPainShader(meansOfDeath_t mod, bool takeArmorIntoAccount);  
    const str getPainShader(const char *MODName);  
    bool      needToSendAllHudsToClient(void);
    bool      canRegenerate(void);
    bool      useWeapon(const char *weaponname, weaponhand_t hand) override;
    bool      useWeapon(Weapon *weapon, weaponhand_t hand);
    bool      GetProjectileLaunchAngles(Vector &launchAngles, const Vector &launchPoint, const float initialSpeed, const float gravity) const;
    qboolean  GetTagPositionAndOrientation(const str &tagname, orientation_t *new_or);
    qboolean  GetTagPositionAndOrientation(int tagnum, orientation_t *new_or);
    void  CalcBlend(void);
    void  setFlash(const Vector &color, float alpha, float minTime, float maxTime);  
    void  DamageFeedback(void);

    void UpdateMusic(void);
    void UpdateReverb(void);
    void UpdateMisc(void);

    void SetReverb(str type, float level);
    void SetReverb(Event *ev);  

    Camera *CurrentCamera(void);
    void    SetCamera(Camera *ent, float switchTime);
    void    CameraCut(void);
    void    CameraCut(Camera *ent);

    void    AutomaticallySelectedNewCamera(void);
    void    DestroyActorCamera(void);

    static Condition<Player> Conditions[];
    qboolean returntrue(Conditional &condition);
    qboolean checksneak(Conditional &condition);
    qboolean checkrun(Conditional &condition);
    qboolean checkwasrunning(Conditional &condition);
    qboolean checkholsterweapon(Conditional &condition);
    qboolean checkuse(Conditional &condition);
    qboolean checkturnleft(Conditional &condition);
    qboolean checkturnright(Conditional &condition);
    qboolean checkforward(Conditional &condition);
    qboolean checkbackward(Conditional &condition);
    qboolean checkstrafeleft(Conditional &condition);
    qboolean checkstraferight(Conditional &condition);
    qboolean checkduck(Conditional &condition);
    qboolean checkjump(Conditional &condition);
    qboolean checkrise(Conditional &condition);
    qboolean checkcrouch(Conditional &condition);
    qboolean checkattackleft(Conditional &condition);
    qboolean checkblocked(Conditional &condition);
    qboolean checkonground(Conditional &condition);
    qboolean checkfalling(Conditional &condition);
    qboolean checkatdoor(Conditional &condition);
    qboolean checkdead(Conditional &condition);
    qboolean checkhealth(Conditional &condition);
    qboolean checkpain(Conditional &condition);
    qboolean checkknockdown(Conditional &condition);
    qboolean checklegsstate(Conditional &condition);
    qboolean checktorsostate(Conditional &condition);
    qboolean checkchance(Conditional &condition);
    qboolean checkpoints(Conditional &condition);

    // removed: it does not exist in retail.  The retail binary exports only
    void SetupView(void);

    void ProcessPmoveEvents(int event);

    void PlayerAngles(void);
    void FinishMove(void);
    void CheckReloadWeapons(void);
    void EndFrame(Event *ev);  

    void   TestThread(Event *ev);

    void GotKill(Event *ev);

    void WhatIs(Event *ev);
    void ActorInfo(Event *ev);
    void Taunt(Event *ev);

    void ChangeMusic(const char *current, const char *fallback, qboolean force);
    void ChangeMusicVolume(float volume, float fade_time);
    void RestoreMusicVolume(float fade_time);

    void Archive(Archiver& arc) override;
    void ArchivePersistantData(Archiver& arc, qboolean sublevelTransition) override;

    void LevelCleanup(void);

    int  getTotalGameFrames(void) const { return _totalGameFrames; }
    void incrementSecretsFound(void);
    void setSkill(int skill);
    int  getSkill(void);  
    HoldableItem *getHoldableItem(void);  
    void  SetClientViewAngles(const Vector &position, const float cameraoffset, const Vector &ang, const Vector &vel, const float camerafov) const; 

    void KillEnt(Event *ev);
    void RemoveEnt(Event *ev);
    void KillClass(Event *ev);
    void RemoveClass(Event *ev);

    void Jump(Event *ev);
    void JumpXY(Event *ev);

    void SetViewAngles(Vector angles);

    Vector getViewAngles(void) { return v_angle; };  

    void  SetFov(float newFov, bool forced = false);
    void  setHoldableItem(HoldableItem *holdableItem);
    float GetFov() const;


    Vector   getWeaponViewShake(void);
    void     DumpState(Event *ev);
    void     ForceTorsoState(Event *ev);

    Vector GetAngleToTarget(Entity *ent, str tag, float yawclamp, float pitchclamp, Vector baseangles);

    void AutoAim(void);
    void AcquireTarget(void);

    void DebugWeaponTags(int controller_tag, Weapon *weapon, str weapon_tagname);
    void NextPainTime(Event *ev);
    void SetTakePain(Event *ev);

    void SetMouthAngle(Event *ev);

    void EnterVehicle(Event *ev);
    void ExitVehicle(Event *ev);
    void Holster(Event *ev);
    void HolsterToggle(Event *ev);

    void SafeHolster(qboolean putaway);

    painDirection_t Pain_string_to_int(str pain);

    inline Vector GetVAngles(void) { return v_angle; }

    void GetStateAnims(Container<const char *> *c) override;
    void VelocityModified(void) override;
    int  GetKnockback(int original_knockback, qboolean blocked);
    void ReceivedItem(Item *item) override;
    void RemovedItem(Item *item) override;

    void setAutoSwitchWeapons(bool autoSwitchWeapons) { _autoSwitchWeapons = autoSwitchWeapons; }
    bool getAutoSwitchWeapons(void) { return _autoSwitchWeapons; }

    void IncreaseActionLevel(float action_level_increase);
    int  AwardPoints(int numPoints);
    void AmmoAmountChanged(Ammo *ammo, int inclip = 0) override;

    void WaitForState(Event *ev);
    void SkipCinematic(Event *ev);
    void SetDamageMultiplier(Event *ev);
    void Loaded(void);
    void PlayerShowModel(Event *ev);
    void showModel(void) override;
    void ResetHaveItem(Event *ev);


    void       Score(Event *ev);
    void       WarpToPoint(Entity *spawnpoint);

    void ArmorDamage(Event *ev) override;
    void Disconnect(void);

    void DeadBody(Event *ev);
    void Gib();








    // SDK player.h:905.  This was an int-based port-only overload; retail's
    void  FireWeapon(Event *ev) override;








    void SetStateFile(Event *ev);


    void setBranchDialogActor(const Actor *actor);
    void clearBranchDialogActor(void);

    void   setBackpackAttachOffset(Event *ev);
    void   setBackpackAttachAngles(Event *ev);
    void   setFlagAttachOffset(Event *ev);
    void   setFlagAttachAngles(Event *ev);
    Vector getBackpackAttachOffset(void);
    Vector getBackpackAttachAngles(void);
    Vector getFlagAttachOffset(void);
    Vector getFlagAttachAngles(void);

    Vector _backpackAttachOffset;
    Vector _backpackAttachAngles;
    Vector _flagAttachOffset;
    Vector _flagAttachAngles;


    void     GibEvent(Event *ev);

    void SetViewAnglesEvent(Event *ev); 
    void UseSpecifiedEntity(Event *ev); 
    void SetStat(Event *ev);            
    void addRosterTeammate1(Event *ev);
    void addRosterTeammate2(Event *ev);
    void addRosterTeammate3(Event *ev);
    void addRosterTeammate4(Event *ev);
    void removeRosterTeammate3(Event *ev);


    float getDamageDone(float damage, int meansOfDeath, bool inMelee);
    void   PlayerDone(Event *ev);            
    const str getGameplayAnim(const str& objname); 

    float GetKnockbackMultiplier() { return knockbackMultiplier; }  
    float GetPlayerKnockback() { return playerKnockback; }          
    str   getAttackType() const { return _attackType; }             
    meansOfDeath_t changetMeansOfDeath(meansOfDeath_t meansOfDeath); 
    void SpawnDamageEffect(meansOfDeath_t mod);                     
    void AdvancedMeleeAttack(weaponhand_t hand);                   
    void MeleeDamageStart(Event *ev);                              
    void MeleeDamageEnd(Event *ev);                                

    void shotFired(void);
    void shotHit(void);

    bool isButtonDown(int button);

    void loadObjectives(const str& objectiveName);






public:
    void clearItemText( Event *ev ); 
    void ClientThink( Event *ev ); 
    void SetReverb( int type, float level ); 
    void Holster( qboolean putaway ); 
    void disableInventory( void ); 
    void enableInventory( void ); 
    void ActivateDualWeapons( Event *ev ); 
    void ActivateNewWeapon( Event *ev ); 
    void ActivateShield( Event *ev ); 
    void AddMeleeAttacker( Event *ev ); 
    void AdjustTorso( Event *ev ); 
    void ApplyPowerupEffects(int &moveSpeed ); 
    void CallVote( Event *ev ); 
    void ChangeChar( Event *ev ); 
    void ChangeCharFadeIn( Event *ev ); 
    void ChangeStance( Event *ev ); 
    void ClearDialog( Event *ev ); 
    void ClearDialog( void );  
    void ClearIncomingMelee( Event *ev ); 
    void ClearStanceLegs( Event *ev ); 
    void ClearStanceTorso( Event *ev ); 
    void ClearTarget( Event *ev ); 
    void ClearTextDialog( Event* ev ); 
    void ClearTextDialog( void );  
    void ClientMoveFlagsAndSpeeds( int moveSpeed, int noclipSpeed, int crouchSpeed, int airSpeed ); 
    void ClientMoveLean(usercmd_t *ucmd); 
    void ClientMoveMisc( usercmd_t *ucmd ); 
    void DeactivateShield( Event *ev ); 
    void DeactivateWeapon( Event *ev ); 
    void DualWield( Event *ev ); 
    void EvaluateTorsoAnim( Event *ev ); 
    void EventUseItem( Event *ev ); 
    void ExtraEntitiesToSendToClient( int *numExtraEntities, int *extraEntities ); 
    void FakePlayer( qboolean holster ); 
    void GiveOxygen( float time ); 
    void GivePointsEvent( Event *ev ); 
    void GravityNodes( void ); 
    void HeadWatchAllowed( Event *ev ); 
    void HideCoolItem( Event *ev ); 
    void InitInventory( void ); 
    void InitStats( void ); 
    void KnockbackMultiplier( Event *ev ); 
    void MeleeEvent( Event *ev ); 
    void NightvisionToggle (Event *ev); 
    void PassEventToVehicle( Event *ev ); 
    void PlayerKnockback( Event *ev ); 
    void ProjDetonate(Event *ev); 
    void PutawayWeapon( Event *ev ); 
    void ReleaseFireWeapon(Event *ev) override; 
    void ReloadTiki( Event *ev ); 
    void ReloadWeapon(Event *ev); 
    void RemoveFakePlayer( void ); 
    void SetAimType( Event *ev ); 
    void SetAnim( const char *anim, bodypart_t part = legs, bool force = false ); 
    void SetBendTorso( Event *ev ); 
    void SetHeadTarget( Event *ev ); 
    void SetPlayerChar( Event *ev ); 
    void SetPlayerViewNormal( void ); 
    void SetPlayerViewUsingActorController( Camera *camera ); 
    void SetPlayerViewUsingCinematicController( Camera* camera ); 
    void SetPlayerViewUsingEntityWatchingController( void ); 
    void SetPlayerViewUsingNoClipController( void ); 
    void SetPointsEvent( Event *ev ); 
    void SetProjDetonate(qboolean value); 
    void SetState(const str& legsstate, const str& torsostate); 
    void SetupDialog( Event *ev ); 
    void SetupDialog( Entity *entity, const str &soundName );  
    void ShakeCamera( void ); 
    void ShowCoolItem( Event *ev ); 
    void ShowHeuristics( Event *ev ); 
    void SpecialMoveChargeEnd( Event *ev ); 
    void SpecialMoveChargeStart( Event *ev ); 
    void SpecialMoveChargeTime( Event *ev ); 
    void StartCoolItem( Event *ev ); 
    void StartCoolItemAnim( void ); 
    void StartFakePlayer( void ); 
    void StopCoolItem( Event *ev ); 
    void StopWatchingEntity( void ); 
    void StopWatchingEntity( Event *ev );  
    void SwitchWeaponMode(Event *ev); 
    void UpdateEntityStateForClient( entityState_t *state ); 
    void UpdateObjectiveStatus( void ); 
    void UpdatePlayerStateForClient( playerState_t *state ); 
    void UseDualWield( Event *ev ); 
    void Vote( Event *ev ); 
    void WatchEntity( Event *ev ); 
    void WeaponCommand( Event *ev ); 
    void WeaponsHolstered( void ); 
    void WeaponsNotHolstered( void ); 
    void addFinishingMove( Event *ev ); 
    void addHud( Event *ev ); 
    void addHudToClient( const str &hudName ); 
    void addHud( const str &hudName );  
    void addPowerupEffect( PowerupBase *powerup ); 
    void allowActionMusic( bool allowActionMusic ); 
    void allowMusicDucking( bool allowMusicDucking ); 
    void applyWeaponSpeedModifiers( int *moveSpeed ); 
    void clearAllHuds( void ); 
    void clearFinishingMove( Event *ev ); 
    void clearTempAttachments( void ); 
    void clearVoteText( void ); 
    void doFinishingMove( Event *ev ); 
    void doUseEntity( Event *ev ); 
    void doneUseEntity( Event *ev ); 
    void dropPowerup( void ); 
    void dropRune( Event *ev ); 
    void dropRune( void );  
    void equipItems( Event *ev ); 
    void forceMoveType( Event *ev ); 
    void forceTimeScale( Event *ev ); 
    void freezePlayer( Event *ev ); 
    void handleDialogSetup( Entity* entity, const str& soundName ); 
    void handlePickupItem(Item *item); 
    void handleTextDialogSetup( const str& soundName ); 
    void hudPrint( Event *ev ); 
    void hudPrint( const str &string );  
    void immobilizePlayer( Event *ev ); 
    void isPlayerOnGround( Event *ev ); 
    void joinTeam( Event *ev ); 
    void killAllDialog(Event *ev); 
    void killAllDialog(void);  
    const str getFreeInventorySlot();  
    void modelChanged( void ); 
    void multiplayerCommand( Event *ev ); 
    void nextGameplayAnim( Event *ev ); 
    void notifyPlayerOfMultiplayerEvent( const char *eventName, const char *eventItemName, Player *eventPlayer ); 
    void pointOfView( Event *ev ); 
    void removeHud( Event *ev ); 
    void removeHud( const str &hudName );  
    void removeHudFromClient( const str &hudName ); 
    void removePowerup( void ); 
    void removePowerupEffect( PowerupBase *powerup ); 
    void removePowerupEvent( Event *ev ); 
    void removeRosterTeammate1( Event* ev ); 
    void removeRosterTeammate2( Event* ev ); 
    void removeRosterTeammate4( Event* ev ); 
    void removeRune( void ); 
    void sendAllHudsToClient( void ); 
    void setAttackType(Event *ev); 
    void setBackupModel( Event *ev ); 
    void setBackupModel( const str &modelName );  
    void setCanTransferEnergy( Event * ); 
    void setDisableInventory( Event* ev ); 
    void setDisableUseWeapon( Event* ev ); 
    void setDoDamageScreenFlash( Event *ev ); 
    void setGameplayAnim( Event *ev ); 
    void setPowerup( Powerup *powerup ); 
    void setRune( Rune *rune ); 
    void setTargeted( bool targeted ); 
    void setValidPlayerModel( Event *ev ); 
    void setVoteText( const str &voteText ); 
    void skillChanged( const str& objname ); 
    void transferEnergy( void ); 
    void turnTowardsEntity( Event *ev ); 
    void usePlayer( Event * ); 
    void warp( Event *ev ); 
    qboolean GetCrouch(); 
    qboolean GetProjDetonate(); 
    qboolean IsDualWeaponActive( void ); 
    qboolean LargeShieldActive( void ); 
    qboolean ShieldActive( void ); 
    int TakePoints(int numPoints); 
    qboolean WeaponsOut( void ); 
    Weapon *GetNewActiveWeapon(void);  
    weaponhand_t GetNewActiveWeaponHand(void);  
    void ClearNewActiveWeapon(void);  
    qboolean check22degreeslope( Conditional &condition ); 
    qboolean check45degreeslope( Conditional &condition ); 
    qboolean checkEndAnimChain( Conditional &condition ); 
    qboolean checkHasAnim( Conditional &condition ); 
    qboolean checkIsWeaponControllingProjectile( Conditional &condition ); 
    qboolean checkOnLadder( Conditional &condition ); 
    qboolean checkPropChance( Conditional &condition ); 
    qboolean checkPropExists( Conditional &condition ); 
    qboolean checkWeaponType( Conditional &condition ); 
    qboolean checkaccumulatedpain( Conditional &condition ); 
    qboolean checkanimdone_legs( Conditional &condition ); 
    qboolean checkanimdone_torso( Conditional &condition ); 
    qboolean checkanyweaponactive( Conditional &condition ); 
    qboolean checkattackblocked( Conditional &condition ); 
    qboolean checkattackbuttonleft( Conditional &condition ); 
    qboolean checkattackbuttonright( Conditional &condition ); 
    qboolean checkattackright( Conditional &condition ); 
    qboolean checkatuseanim( Conditional &condition ); 
    qboolean checkatuseentity( Conditional &condition ); 
    qboolean checkatuseobject( Conditional &condition ); 
    qboolean checkblockdelay( Conditional &condition ); 
    qboolean checkcanfall( Conditional &condition ); 
    qboolean checkcanstand( Conditional &condition ); 
    qboolean checkcanturn( Conditional &condition ); 
    qboolean checkcharavailable( Conditional &condition ); 
    qboolean checkdualweaponreadytofire( Conditional &condition ); 
    qboolean checkdualweapons( Conditional &condition ); 
    qboolean checkdualwield( Conditional &condition ); 
    qboolean checkfacingdownslope( Conditional &condition ); 
    qboolean checkfacingupslope( Conditional &condition ); 
    qboolean checkfakeplayeractive( Conditional &condition ); 
    qboolean checkfinishingmove( Conditional &condition ); 
    qboolean checkgroundentity( Conditional &condition ); 
    qboolean checkhardimpact( Conditional &condition ); 
    qboolean checkhasdualweapon( Conditional &condition ); 
    qboolean checkhasweapon( Conditional &condition ); 
    qboolean checkincomingmeleeattack( Conditional &condition ); 
    qboolean checkinvehicle( Conditional &condition ); 
    qboolean checkjumpflip( Conditional &condition ); 
    qboolean checkleanleft(Conditional &condition); 
    qboolean checkleanright(Conditional &condition); 
    qboolean checkleftleghigh( Conditional &condition ); 
    qboolean checkloopuseobject( Conditional &condition ); 
    qboolean checknewweapon( Conditional &condition ); 
    qboolean checkpaindirection( Conditional &condition ); 
    qboolean checkpainthreshold( Conditional &condition ); 
    qboolean checkpaintype( Conditional &condition ); 
    qboolean checkpull( Conditional &condition ); 
    qboolean checkpush( Conditional &condition ); 
    qboolean checkputawayboth( Conditional &condition ); 
    qboolean checkputawayleft( Conditional &condition ); 
    qboolean checkputawayright( Conditional &condition ); 
    qboolean checkrightleghigh( Conditional &condition ); 
    qboolean checksolidforward( Conditional &condition ); 
    qboolean checkspecialmovecharge( Conditional &condition ); 
    qboolean checkstance( Conditional &condition ); 
    qboolean checkstancechangedlegs( Conditional &condition ); 
    qboolean checkstancechangedtorso( Conditional &condition ); 
    qboolean checkstatename( Conditional &condition ); 
    qboolean checktargetacquired( Conditional &condition ); 
    qboolean checkthirdperson( Conditional &condition ); 
    qboolean checktouchuseanim( Conditional &condition ); 
    qboolean checkturnedleft( Conditional &condition ); 
    qboolean checkturnedright( Conditional &condition ); 
    qboolean checkuseanimfinished( Conditional &condition ); 
    qboolean checkuseweapon( Conditional &condition ); 
    qboolean checkusingentity( Conditional &condition ); 
    qboolean checkweaponactive( Conditional &condition ); 
    qboolean checkweaponcanreload( Conditional &condition ); 
    qboolean checkweapondonefiring( Conditional &condition ); 
    qboolean checkweaponfiretimer( Conditional &condition ); 
    qboolean checkweaponforcereload( Conditional &condition ); 
    qboolean checkweaponfullclip( Conditional &condition ); 
    qboolean checkweaponhasammo( Conditional &condition ); 
    qboolean checkweaponhasfullammo( Conditional &condition ); 
    qboolean checkweaponhasinvammo( Conditional &condition ); 
    qboolean checkweaponinmode( Conditional &condition ); 
    qboolean checkweaponlowered( Conditional &condition ); 
    qboolean checkweaponreadytofire( Conditional &condition ); 
    qboolean checkweaponreload( Conditional &condition ); 
    qboolean checkweaponsholstered( Conditional &condition ); 
    qboolean checkweaponswitchmode( Conditional &condition ); 
    float getDefaultFov( void ); 
    void ListInventoryEvent( Event *ev ); 
    void LogStats( Event *ev ); 
};


inline void Player::Archive(Archiver& arc)
{
    str            tempStr;
    int            i, num;
    WeaponSetItem *tempDualWeapon;

    Sentient::Archive(arc);


    if (arc.Loading()) {
        LoadStateTable();
    }

    if (arc.Saving()) {
        if (currentState_Legs) {
            tempStr = currentState_Legs->getName();
        } else {
            tempStr = "NULL";
        }
        arc.ArchiveString(&tempStr);

        if (currentState_Torso) {
            tempStr = currentState_Torso->getName();
        } else {
            tempStr = "NULL";
        }
        arc.ArchiveString(&tempStr);
    } else {
        arc.ArchiveString(&tempStr);
        if (tempStr != "NULL" && statemap_Legs) {
            currentState_Legs = statemap_Legs->FindState(tempStr);
        } else {
            currentState_Legs = NULL;
        }

        arc.ArchiveString(&tempStr);
        if (tempStr != "NULL" && statemap_Torso) {
            currentState_Torso = statemap_Torso->FindState(tempStr);
        } else {
            currentState_Torso = NULL;
        }
    }

    arc.ArchiveString(&last_torso_anim_name);
    arc.ArchiveString(&last_leg_anim_name);

    ArchiveEnum(movecontrol, movecontrol_t);

    arc.ArchiveInteger(&last_camera_type);
    if (arc.Loading()) {
        last_camera_type = -1;
    }

    newActiveWeapon.Archive(arc);

    arc.ArchiveSafePointer(&head_target);
    arc.ArchiveFloat(&look_at_time);

    arc.ArchiveSafePointer(&targetEnemy);
    arc.ArchiveBool(&targetEnemyLocked);

    arc.ArchiveBoolean(&shield_active);

    arc.ArchiveBoolean(&dual_wield_active);

    if (arc.Saving()) {
        num = dual_wield_weaponlist.NumObjects();
    } else {
        num = 0;
        dual_wield_weaponlist.ClearObjectList();
    }

    arc.ArchiveInteger(&num);

    for (i = 1; i <= num; i++) {
        if (arc.Saving()) {
            tempDualWeapon = dual_wield_weaponlist.ObjectAt(i);
        } else {
            tempDualWeapon = new WeaponSetItem;
            dual_wield_weaponlist.AddObject(tempDualWeapon);
        }
        tempDualWeapon->Archive(arc);
    }

    for (i = 0; i < MAX_ACTIVE_WEAPONS; i++) {
        arc.ArchiveSafePointer(&holsteredWeapons[i]);
    }

    arc.ArchiveString(&partAnim[0]);
    arc.ArchiveString(&partAnim[1]);
    arc.ArchiveString(&partAnim[2]);

    arc.ArchiveBool(&animdone_Legs);
    arc.ArchiveBool(&animdone_Torso);

    arc.ArchiveVector(&oldvelocity);
    arc.ArchiveVector(&old_v_angle);
    arc.ArchiveVector(&oldorigin);
    arc.ArchiveFloat(&animspeed);
    arc.ArchiveFloat(&airspeed);

    arc.ArchiveString(&currentCallVolume);

    arc.ArchiveRaw(blend, sizeof(blend));
    arc.ArchiveFloat(&fov);
    arc.ArchiveFloat(&_userFovChoice);

    arc.ArchiveSafePointer(&vehicle);
    arc.ArchiveVector(&v_angle);

    arc.ArchiveInteger(&buttons);
    arc.ArchiveInteger(&new_buttons);
    arc.ArchiveFloat(&respawn_time);

    arc.ArchiveInteger(&last_attack_button);

    arc.ArchiveFloat(&damage_blood);
    arc.ArchiveFloat(&damage_alpha);
    arc.ArchiveVector(&damage_blend);
    arc.ArchiveVector(&damage_from);
    arc.ArchiveVector(&damage_angles);
    arc.ArchiveFloat(&damage_count);

    arc.ArchiveFloat(&bonus_alpha);

    arc.ArchiveFloat(&next_drown_time);
    arc.ArchiveFloat(&next_painsound_time);
    arc.ArchiveFloat(&air_finished);

    arc.ArchiveInteger(&old_waterlevel);
    arc.ArchiveFloat(&drown_damage);

    arc.ArchiveFloat(&_flashAlpha);
    arc.ArchiveVector(&_flashBlend);
    arc.ArchiveFloat(&_flashMaxTime);
    arc.ArchiveFloat(&_flashMinTime);

    arc.ArchiveString(&waitForState);

    arc.ArchiveSafePointer(&camera);
    arc.ArchiveSafePointer(&actor_camera);
    arc.ArchiveSafePointer(&cool_camera);
    arc.ArchiveSafePointer(&entity_to_watch);
    arc.ArchiveFloat(&maximumAngleToWatchedEntity);
    arc.ArchiveBool(&watchEntityForEntireDuration);
    arc.ArchiveInteger(&playerCameraMode);

    arc.ArchiveFloat(&action_level);
    arc.ArchiveInteger(&music_current_mood);
    arc.ArchiveInteger(&music_fallback_mood);
    arc.ArchiveFloat(&music_current_volume);
    arc.ArchiveFloat(&music_saved_volume);
    arc.ArchiveFloat(&music_volume_fade_time);
    arc.ArchiveInteger(&reverb_type);
    arc.ArchiveFloat(&reverb_level);
    arc.ArchiveBoolean(&music_forced);
    arc.ArchiveBool(&_allowMusicDucking);
    arc.ArchiveBool(&_allowActionMusic);

    arc.ArchiveBoolean(&gibbed);
    arc.ArchiveFloat(&pain);

    ArchiveEnum(pain_dir, painDirection_t);
    ArchiveEnum(pain_type, meansOfDeath_t);
    arc.ArchiveBool(&take_pain);

    arc.ArchiveFloat(&accumulated_pain);
    arc.ArchiveFloat(&nextpaintime);

    arc.ArchiveBool(&knockdown);
    arc.ArchiveBool(&canfall);
    arc.ArchiveBool(&falling);
    arc.ArchiveInteger(&feetfalling);
    arc.ArchiveVector(&falldir);

    arc.ArchiveBool(&hardimpact);

    arc.ArchiveRaw(&last_ucmd, sizeof(last_ucmd));


    arc.ArchiveVector(&base_rightfoot_pos);
    arc.ArchiveVector(&base_leftfoot_pos);
    arc.ArchiveVector(&rightfoot_pos);
    arc.ArchiveVector(&leftfoot_pos);

    arc.ArchiveBool(&_onLadder);
    arc.ArchiveVector(&_ladderNormal);
    arc.ArchiveFloat(&_ladderTop);
    arc.ArchiveFloat(&_nextLadderTime);

    arc.ArchiveInteger(&pm_lastruntime);
    arc.ArchiveFloat(&animheight);

    arc.ArchiveVector(&yaw_forward);
    arc.ArchiveVector(&yaw_left);

    arc.ArchiveSafePointer(&atobject);
    arc.ArchiveFloat(&atobject_dist);
    arc.ArchiveVector(&atobject_dir);

    arc.ArchiveSafePointer(&toucheduseanim);
    arc.ArchiveInteger(&useanim_numloops);
    arc.ArchiveSafePointer(&useitem_in_use);
    arc.ArchiveSafePointer(&cool_item);
    arc.ArchiveString(&cool_dialog);
    arc.ArchiveString(&cool_anim);

    arc.ArchiveInteger(&moveresult);

    arc.ArchiveBoolean(&do_rise);
    arc.ArchiveBoolean(&weapons_holstered_by_code);
    arc.ArchiveBoolean(&projdetonate);

    arc.ArchiveBoolean(&yawing);
    arc.ArchiveBoolean(&yawing_left);
    arc.ArchiveBoolean(&yawing_right);
    arc.ArchiveBoolean(&adjust_torso);

    arc.ArchiveVector(&torsoAngles);
    arc.ArchiveVector(&headAngles);
    arc.ArchiveVector(&headAimAngles);
    arc.ArchiveVector(&torsoAimAngles);

    arc.ArchiveFloat(&damage_multiplier);


    arc.ArchiveBoolean(&fakePlayer_active);
    arc.ArchiveSafePointer(&fakePlayer);

    arc.ArchiveBool(&dont_turn_legs);
    arc.ArchiveFloat(&specialMoveCharge);
    arc.ArchiveFloat(&specialMoveEndTime);
    arc.ArchiveFloat(&specialMoveChargeTime);
    arc.ArchiveInteger(&points);
    arc.ArchiveFloat(&playerKnockback);
    arc.ArchiveFloat(&knockbackMultiplier);
    arc.ArchiveBool(&changedStanceTorso);
    arc.ArchiveBool(&changedStanceLegs);
    arc.ArchiveInteger(&stanceNumber);
    arc.ArchiveBool(&incomingMeleeAttack);
    arc.ArchiveFloat(&bendTorsoMult);
    arc.ArchiveString(&lastActionType);
    arc.ArchiveInteger(&meleeAttackFlags);
    arc.ArchiveBool(&changingChar);

    int numEntries;
    if (arc.Saving()) {
        numEntries = meleeAttackerList.NumObjects();
        arc.ArchiveInteger(&numEntries);

        EntityPtr eptr;
        for (i = 1; i <= numEntries; i++) {
            eptr = meleeAttackerList.ObjectAt(i);
            arc.ArchiveSafePointer(&eptr);
        }
    } else {
        EntityPtr  eptr;
        EntityPtr *eptrptr;
        arc.ArchiveInteger(&numEntries);

        meleeAttackerList.ClearObjectList();
        meleeAttackerList.Resize(numEntries);

        for (i = 1; i <= numEntries; i++) {
            meleeAttackerList.AddObject(eptr);
            eptrptr = &meleeAttackerList.ObjectAt(i);
            arc.ArchiveSafePointer(eptrptr);
        }
    }


    if (arc.Saving()) {
        numEntries = finishableList.NumObjects();
        arc.ArchiveInteger(&numEntries);

        ActorPtr eptr;
        for (i = 1; i <= numEntries; i++) {
            eptr = finishableList.ObjectAt(i);
            arc.ArchiveSafePointer(&eptr);
        }
    } else {
        ActorPtr  eptr;
        ActorPtr *eptrptr;
        arc.ArchiveInteger(&numEntries);

        finishableList.ClearObjectList();
        finishableList.Resize(numEntries);

        for (i = 1; i <= numEntries; i++) {
            finishableList.AddObject(eptr);
            eptrptr = &finishableList.ObjectAt(i);
            arc.ArchiveSafePointer(eptrptr);
        }
    }

    if (arc.Saving()) {
        numEntries = finishingMoveList.NumObjects();
        arc.ArchiveInteger(&numEntries);

        FinishingMove *eptr;
        for (i = 1; i <= numEntries; i++) {
            eptr = finishingMoveList.ObjectAt(i);
            arc.ArchiveObjectPointer((Class **)&eptr);
        }
    } else {
        FinishingMove  *eptr = NULL;
        FinishingMove **eptrptr;
        arc.ArchiveInteger(&numEntries);

        finishingMoveList.ClearObjectList();
        finishingMoveList.Resize(numEntries);

        for (i = 1; i <= numEntries; i++) {
            finishingMoveList.AddObject(eptr);
            eptrptr = &finishingMoveList.ObjectAt(i);
            arc.ArchiveObjectPointer((Class **)eptrptr);
        }
    }

    arc.ArchiveSafePointer(&_powerup);
    arc.ArchiveSafePointer(&_rune);
    arc.ArchiveSafePointer(&_holdableItem);

    arc.ArchiveFloat(&_nextEnergyTransferTime);
    arc.ArchiveBool(&_canTransferEnergy);
    arc.ArchiveBool(&_doDamageScreenFlash);
    arc.ArchiveBool(&_isThirdPerson);
    arc.ArchiveObjectPointer((Class **)&_finishActor);
    arc.ArchiveString(&_finishState);
    arc.ArchiveBool(&_doingFinishingMove);

    arc.ArchiveBool(&_autoSwitchWeapons);

    arc.ArchiveBool(&_usingEntity);
    arc.ArchiveString(&_attackType);
    arc.ArchiveInteger(&_gameplayAnimIdx);

    arc.ArchiveBool(&_disableUseWeapon);
    arc.ArchiveSafePointer(&_targetSelectedHighlight);
    arc.ArchiveSafePointer(&_targetLockedHighlight);
    arc.ArchiveBool(&_infoHudOn);
    arc.ArchiveInteger(&_nextRegenTime);
    arc.ArchiveFloat(&_useEntityStartTimer);

    str objectiveName;
    if (arc.Saving()) {
        objectiveName = gi.getConfigstring(CS_OBJECTIVE_NAME + _objectiveNameIndex);
        arc.ArchiveString(&objectiveName);
    }

    if (arc.Loading()) {
        arc.ArchiveString(&objectiveName);
        loadObjectives(objectiveName);
    }

    arc.ArchiveUnsigned(&_objectiveStates);
    arc.ArchiveUnsigned(&_informationStates);

    arc.ArchiveSafePointer(&_targetedEntity);

    str dialogSound;
    str dialogTextSound;
    if (arc.Saving()) {
        arc.ArchiveInteger(&_dialogEntnum);

        dialogSound = gi.getConfigstring(CS_SOUNDS + _dialogSoundIndex);
        arc.ArchiveString(&dialogSound);

        dialogTextSound = gi.getConfigstring(CS_SOUNDS + _dialogTextSoundIndex);
        arc.ArchiveString(&dialogTextSound);
    }

    if (arc.Loading()) {
        arc.ArchiveInteger(&_dialogEntnum);

        arc.ArchiveString(&dialogSound);
        _dialogSoundIndex = dialogSound.length() ? gi.soundindex(dialogSound.c_str()) : 0;

        arc.ArchiveString(&dialogTextSound);
        _dialogTextSoundIndex = dialogTextSound.length() ? gi.soundindex(dialogTextSound.c_str()) : 0;
    }

    arc.ArchiveFloat(&_crossHairXOffset);
    arc.ArchiveFloat(&_crossHairYOffset);

    arc.ArchiveFloat(&_lastDamagedTimeFront);
    arc.ArchiveFloat(&_lastDamagedTimeBack);
    arc.ArchiveFloat(&_lastDamagedTimeLeft);
    arc.ArchiveFloat(&_lastDamagedTimeRight);

    arc.ArchiveInteger(&_totalGameFrames);

    arc.ArchiveFloat(&_nextPainShaderTime);
    ArchiveEnum(_lastPainShaderMod, meansOfDeath_t);

    arc.ArchiveInteger(&_itemIcon);
    arc.ArchiveString(&_itemText);

    arc.ArchiveString(&_voteText);

    arc.ArchiveBool(&_validPlayerModel);

    arc.ArchiveInteger(&_secretsFound);

    _hudList.Archive(arc);

    if (arc.Loading() && (_hudList.NumObjects() > 0)) {
        _needToSendHuds = true;
    } else {
        _needToSendHuds = false;
    }

    _started = false;

    arc.ArchiveInteger(&_skillLevel);

    ArchiveEnum(_forcedMoveType, pmtype_t);

    arc.ArchiveSafePointer(&_branchDialogActor);
    _needToSendBranchDialog = true;


    arc.ArchiveBool(&_cameraCutThisFrame);

    if (arc.Loading()) {
        SetViewAngles(v_angle);
    }
}

inline Camera *Player::CurrentCamera(void)
{
    return camera;
}

inline void Player::CameraCut(void)
{
    if (!client) {
        return;
    }
    if (_cameraCutThisFrame) {
        return;
    }
    client->ps.camera_flags = ((client->ps.camera_flags & CF_CAMERA_CUT_BIT) ^ CF_CAMERA_CUT_BIT)
                            | (client->ps.camera_flags & ~CF_CAMERA_CUT_BIT);

    _cameraCutThisFrame = true;
}

inline void Player::CameraCut(Camera *ent)
{
    if (ent == camera) {
        CameraCut();
    }
}

inline void Player::SetCamera(Camera *ent, float switchTime)
{
    if (!client) {
        return;
    }

    camera                 = ent;
    client->ps.camera_time = switchTime;
    if (switchTime <= 0.0f) {
        CameraCut();
    }
}

inline Weapon *Player::GetNewActiveWeapon(void)
{
    return newActiveWeapon.weapon;
}

inline weaponhand_t Player::GetNewActiveWeaponHand(void)
{
    return newActiveWeapon.hand;
}

inline void Player::ClearNewActiveWeapon(void)
{
    newActiveWeapon.weapon = NULL;
    newActiveWeapon.hand   = WEAPON_ERROR;
}
