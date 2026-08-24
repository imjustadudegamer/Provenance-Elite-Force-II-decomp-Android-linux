//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor.h                                        $
// $Revision:: 280                                                            $
//   $Author:: Sketcher                                                       $
//     $Date:: 5/04/03 5:49p                                                  $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Base class for character AI.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#pragma once

#include "g_local.h"
#include "g_phys.h" 
#include "helper_node.h"  
class Actor;     
class Sentient;  
#include "actorincludes.h" 
#include "characterstate.h"

#include "actorgamecomponents.h"
#include "actor_sensoryperception.h"
#include "actor_enemymanager.h"
#include "actor_locomotion.h"
#include "actor_combatsubsystem.h"
#include "actor_headwatcher.h"


typedef struct DialogNode_s {
    char         alias_name[MAX_ALIAS_NAME_LENGTH];
    int          random_flag;
    int          number_of_parms;
    float        random_percent;
    DialogType_t dType;
    DialogParm_t parms[MAX_DIALOG_PARMS];
    struct DialogNode_s *next;  
} DialogNode_t;

class Player;
Player *GetPlayer(int index);  
#include "weapon.h"
#include "sentient.h"
#include "../corepp/container.h"
#include "stack.h"
#include "navigate.h"
#include "actorutil.h"
#include "scriptmaster.h"
#include "characterstate.h"



#include "level.h"
#include "game.h"
#include "gamescript.h"
#include "scriptmaster.h"
#include "characterstate.h" 
#include "behavior.h"       
#include "helper_node.h"    

class FuzzyEngine; 
class MovementSubsystem; 
class CombatSubsystem;   

class ActorThink;         
class ActorGameComponent; 
class SensoryPerception;  
class Strategos;          
class EnemyManager;       
class PackageManager;     
class Personality;        
class HeadWatcher;        
class PostureController;  

typedef enum {
    ACTOR_CONTROL_NONE,
    ACTOR_CONTROL_AUTO_RELEASE,
    ACTOR_CONTROL_SHARED,
    ACTOR_CONTROL_LOCKED
} ActorControlType;

extern Event EV_EntityFlags;  
extern Event EV_Actor_BounceOff;
extern Event EV_Actor_Dead;  

extern Event EV_ActorIncomingProjectile;
extern Event EV_Actor_ControlLost;

extern Event EV_Actor_SetTurretMode;





#define LIP_SYNC_HZ                   20.0

#define MAX_DIALOG_PARAMETERS_LENGTH  100

#define CONTEXT_WIDE_MIN_DIST         1024

#define MAX_DIALOG_PARM_LENGTH        64
#define MAX_DIALOG_PARMS              10

#define DIALOG_PARM_TYPE_NONE              0
#define DIALOG_PARM_TYPE_PLAYERHAS         1
#define DIALOG_PARM_TYPE_PLAYERHASNOT      2
#define DIALOG_PARM_TYPE_HAS               3
#define DIALOG_PARM_TYPE_HASNOT            4
#define DIALOG_PARM_TYPE_DEPENDS           5
#define DIALOG_PARM_TYPE_DEPENDSNOT        6
#define DIALOG_PARM_TYPE_DEPENDSINT        7
#define DIALOG_PARM_TYPE_CONTEXT_INITIATOR 8
#define DIALOG_PARM_TYPE_CONTEXT_RESPONSE  9


enum SteeringDirectionPreference {
    STEER_RIGHT_ALWAYS,
    STEER_LEFT_ALWAYS,
    STEER_RANDOMLY,
    STEER_BEST
};



#define MAX_INACTIVE_TIME 30.0




#define ACTOR_MODE_NONE   0
#define ACTOR_MODE_IDLE   1
#define ACTOR_MODE_AI     2
#define ACTOR_MODE_SCRIPT 3
#define ACTOR_MODE_TALK   4


#define PAIN_SMALL 0
#define PAIN_BIG   1


#define ACTOR_FLAG_NOISE_HEARD 0
#define ACTOR_FLAG_INVESTIGATING 1
#define ACTOR_FLAG_DEATHGIB 2
#define ACTOR_FLAG_DEATHFADE 3
#define ACTOR_FLAG_NOCHATTER 4
#define ACTOR_FLAG_INACTIVE 5
#define ACTOR_FLAG_ANIM_DONE 6
#define ACTOR_FLAG_STATE_DONE_TIME_VALID 7
#define ACTOR_FLAG_MASTER_STATE_DONE_TIME_VALID 8
#define ACTOR_FLAG_AI_ON 9
#define ACTOR_FLAG_LAST_CANSEEENEMY 10
#define ACTOR_FLAG_LAST_CANSEEENEMY_NOFOV 11
#define ACTOR_FLAG_DIALOG_PLAYING 12
#define ACTOR_FLAG_RADIUS_DIALOG_PLAYING 13
#define ACTOR_FLAG_ALLOW_TALK 14
#define ACTOR_FLAG_DAMAGE_ONCE_ON 15
#define ACTOR_FLAG_DAMAGE_ONCE_DAMAGED 16
#define ACTOR_FLAG_BOUNCE_OFF 17
#define ACTOR_FLAG_NOTIFY_OTHERS_AT_DEATH 18
#define ACTOR_FLAG_HAS_THING1 19
#define ACTOR_FLAG_HAS_THING2 20
#define ACTOR_FLAG_HAS_THING3 21
#define ACTOR_FLAG_HAS_THING4 22
#define ACTOR_FLAG_LAST_ATTACK_HIT 23
#define ACTOR_FLAG_STARTED 24
#define ACTOR_FLAG_ALLOW_HANGBACK 25
#define ACTOR_FLAG_USE_GRAVITY 26
#define ACTOR_FLAG_SPAWN_FAILED 27
#define ACTOR_FLAG_FADING_OUT 28
#define ACTOR_FLAG_DEATHSHRINK 29
#define ACTOR_FLAG_DEATHSINK 30
#define ACTOR_FLAG_STAYSOLID 31
#define ACTOR_FLAG_STUNNED 32
#define ACTOR_FLAG_ALLOW_FALL 33
#define ACTOR_FLAG_FINISHED 34
#define ACTOR_FLAG_IN_LIMBO 35
#define ACTOR_FLAG_CAN_WALK_ON_OTHERS 36
#define ACTOR_FLAG_PUSHABLE 37
#define ACTOR_FLAG_LAST_TRY_TALK 38
#define ACTOR_FLAG_TARGETABLE 39
#define ACTOR_FLAG_IMMORTAL 40
#define ACTOR_FLAG_TURNING_HEAD 41
#define ACTOR_FLAG_MOVING_EYES 42
#define ACTOR_FLAG_DIE_COMPLETELY 43
#define ACTOR_FLAG_BLEED_AFTER_DEATH 44
#define ACTOR_FLAG_IGNORE_STUCK_WARNING 45
#define ACTOR_FLAG_IGNORE_OFF_GROUND_WARNING 46
#define ACTOR_FLAG_ALLOWED_TO_KILL 47
#define ACTOR_FLAG_TOUCH_TRIGGERS 48
#define ACTOR_FLAG_IGNORE_WATER 49
#define ACTOR_FLAG_NEVER_IGNORE_SOUNDS 50
#define ACTOR_FLAG_SIMPLE_PATHFINDING 51
#define ACTOR_FLAG_HAVE_MOVED 52
#define ACTOR_FLAG_NO_PAIN_SOUNDS 53
#define ACTOR_FLAG_UPDATE_BOSS_HEALTH 54
#define ACTOR_FLAG_IGNORE_PAIN_FROM_ACTORS 55
#define ACTOR_FLAG_DAMAGE_ALLOWED 56
#define ACTOR_FLAG_AT_COVER_NODE 57
#define ACTOR_FLAG_WAIT_FOR_NEW_ENEMY 58
#define ACTOR_FLAG_TAKE_DAMAGE 59
#define ACTOR_FLAG_USE_DAMAGESKINS 60
#define ACTOR_FLAG_CAPTURED 61
#define ACTOR_FLAG_TURRET_MODE 62
#define ACTOR_FLAG_INCOMING_HITSCAN 63
#define ACTOR_FLAG_RESPONDING_TO_HITSCAN 64
#define ACTOR_FLAG_MELEE_HIT_WORLD 65
#define ACTOR_FLAG_TORSO_ANIM_DONE 66
#define ACTOR_FLAG_WEAPON_READY 67
#define ACTOR_FLAG_DISABLED 68
#define ACTOR_FLAG_IN_ALCOVE 69
#define ACTOR_FLAG_IN_CONE_OF_FIRE 70
#define ACTOR_FLAG_IN_PLAYER_CONE_OF_FIRE 71
#define ACTOR_FLAG_PLAYER_IN_CALL_VOLUME 72
#define ACTOR_FLAG_IN_CALL_VOLUME 73
#define ACTOR_FLAG_OUT_OF_TORSO_RANGE 74
#define ACTOR_FLAG_DUCKED 75
#define ACTOR_FLAG_PRONE 76
#define ACTOR_FLAG_SHOULD_BLINK 77
#define ACTOR_FLAG_CRIPPLED 78
#define ACTOR_FLAG_RETREATING 79
#define ACTOR_FLAG_HIDDEN 80
#define ACTOR_FLAG_FOLLOWING_IN_FORMATION 81
#define ACTOR_FLAG_DISPLAYING_FAILURE_FX 82
#define ACTOR_FLAG_GROUPMEMBER_INJURED 83
#define ACTOR_FLAG_CAN_HEAL_OTHER 84
#define ACTOR_FLAG_STRICTLY_FOLLOW_PATHS 85
#define ACTOR_FLAG_POSTURE_ANIM_DONE 86
#define ACTOR_FLAG_ATTACKING_ENEMY 87
#define ACTOR_FLAG_UPDATE_HATE_WITH_ATTACKERS 88
#define ACTOR_FLAG_LAST_CANSEEPLAYER 89
#define ACTOR_FLAG_LAST_CANSEEPLAYER_NOFOV 90
#define ACTOR_FLAG_MELEE_ALLOWED 91
#define ACTOR_FLAG_PLAYING_DIALOG_ANIM 92
#define ACTOR_FLAG_USING_HUD 93
#define ACTOR_FLAG_FORCE_LIFEBAR 94
#define ACTOR_FLAG_UPDATE_ACTION_LEVEL 95
#define ACTOR_FLAG_CAN_CHANGE_ANIM 96
#define ACTOR_FLAG_USE_FOLLOWRANGE_FOR_NODES 97
#define ACTOR_FLAG_IMMEDIATE_ACTIVATE 98
#define ACTOR_FLAG_CANNOT_DISINTEGRATE 99
#define ACTOR_FLAG_CANNOT_USE 100
#define ACTOR_FLAG_CANNOT_FREEZE 101

#define ACTOR_FLAG_MAX     102


class Actor;
typedef SafePtr<Actor> ActorPtr;


Actor *GetActor(const str& actor_name);

qboolean EntityIsValidTarget(const Entity *ent);

void AI_SenseEnemies(void);

void AI_DisplayInfo(void);

class Actor : public Sentient
{
public:

public:
    virtual void setSize(Vector min, Vector max) override;


protected:


public:
    void SetPath(Path *newpath);  

public:

public:
    CLASS_PROTOTYPE(Actor);

protected:
    void WalkTo(Event *ev);
    void SetMovementMode(Event *ev); 

public:
    Actor();
    ~Actor();

    void                ChangeAnim(void);
    virtual void Think(void) override;
    void         FireWeapon(Event *ev) override;
    virtual bool CanTarget(void) const;  
    bool IsFinishable(void);  
    virtual bool IsImmortal(void);

    void HeardSound(Event *ev);
    void RespondTo(Event *ev);
    void PermanentlyRespondTo(Event *ev);
    void BroadcastAlert(float rad = SOUND_RADIUS);
    void BroadcastAlert(Event *ev);  

    void  SetVisionDistance(float distance);
    void           ShowInfo(void);
    str            name;

    const str getName() const override { return name; }  

    void           FadeEvent(Event *ev);  
    void           LookAt(Event *ev);
    void           EyeWatchEvent(Event *ev);
    void           HeadAndEyeWatchEvent(Event *ev);
    qboolean       SetAnim(const str& anim, Event *ev = NULL, bodypart_t part = legs, const float animationRate = 1.0f);  
    void           processGameplayData(Event *ev) override;  
    void           SetTurretMode(Event *ev);
    void         Dead(Event *ev);            
    virtual void Archive(Archiver& arc) override;

    StateMap                *statemap;
    State                   *currentState;  
    State                   *lastState;     
    State                   *globalState;   
    str                      statemap_name;  
    BehaviorPtr              behavior;      
    BehaviorPtr              headBehavior;   
    BehaviorPtr              eyeBehavior;    
    BehaviorPtr              torsoBehavior; 
    Container<Conditional *> conditionals;  



    static Condition<Actor> Conditions[];

    static unsigned int ActorTypeStringToInt(const str &type);

    StateMap                *masterstatemap;     
    State                   *currentMasterState; 
    State                   *lastMasterState;    
    str                      masterstatemap_name; 
    Container<Conditional *> master_conditionals; 

    void ProcessMasterStateMachine(void);
    void SetMasterState(const str &state_name);  

    void               SetBehaviorPackage(Event *ev);
    void               SetBehaviorPackage(const str &packageName);
    void               UseBehaviorPackage(const str& packageName);
    // index, exactly as retail does.  1-based, 0 == not found.

    FuzzyEngine             *fuzzyEngine;       
    qboolean                 fuzzyEngine_active; 
    str                      fuzzyengine_name;   
    Container<Conditional *> fuzzy_conditionals; 

    void SetFuzzyEngineActive(Event *ev);
    void RegisterBehaviorPackage(const str& packageName);

    void GiveActorWeapon(Event *ev);
    void SetMaxInactiveTime(Event *ev); 

    actortype_t actortype;
    void            SetActorType(const str& typeName);

    unsigned int state_flags;
    void         AddStateFlag(unsigned int flag);
    void         ClearStateFlags(void);

    void         SendEventToGroup(Event *ev);
    void         GroupAttack(Event *ev);
    void         GroupActorType(Event *ev);
    void         SetGroupDeathThread(Event *ev);

    float        max_boss_health;


    void         _dropActorToGround(void);

    void         CheckStun(void);


    float  pain_threshold;
    float  min_pain_time;
    float  next_pain_time;
    float  max_pain_time;
    float  next_forced_pain_time;
    int    pain_type;
    Vector pain_angles;

    void SetPainThresholdEvent(Event *ev);  
    void SetMinPainTime(Event *ev);
    void SetMinPainTime(float time); 

    // Retail folds the offset straight into Sentient::eyeposition, which retail never
    Vector eyeoffset;
    void   EyeOffset(Event *ev);  

    // BULLETS DO NOT BOUNCE -- see the actor bounce-off blocks in Projectile::Touch.
    str   bounce_off_effect;
    float bounce_off_velocity;
    void  SetBounceOff(Event *ev);       
    void  BounceOffEvent(Event *ev);     
    void  SetBounceOffEffect(Event *ev); 

    float  minLeadFactor;
    float  maxLeadFactor;
    void   SetAimLeadFactors(Event *ev); 

    void  SetCheckConeOfFireDistance(Event *ev);
    void SetCheckConeOfFireDistance(float distance); 

    void AttachActor(Event *ev); 

    void FlyEvent(Event *ev);  

    int  saved_bone_hit;
    void SaveOffLastHitBone(Event *ev);

    float activationStart;
    float activationDelay;
    void  SetActivationStart(Event *ev);
    void  SetActivationStart(); 
    void  SetActivationDelay(Event *ev);
    void SetActivationDelay(float delay); 

    str  kill_thread;
    void SetKillThreadEvent(Event *ev);

    str   onuse_thread_name;
    str   ondamage_thread;
    int   ondamage_threshold;
    float last_used_time;

    void SetOnUseThread(Event *ev);    
    void ClearOnUseThread(Event *ev);  
    void SetOnDamageThread(Event *ev); 
    void RunDamageThread(void);         
    void StartTalkBehavior(Sentient *user); 
    void RunThread(Event *ev);  
    void RunThread(const str& thread_name);  

    void ArmorDamage(Event *ev) override;

    EntityPtr forcedEnemy;
    void      AttackEntity(Event *ev);

    void RemoveActorWeapon(Event *ev);  
    void ClearCurrentEnemy(Event *ev);  
    void ForgetEnemies(Event *ev);      
    void PutawayWeapon(Event *ev);           
    void NotifyOthersOfDeath(void);           

    void     SetNotifyFlag(int flag, qboolean flag_value);
    qboolean GetNotifyFlag(int flag);
    int      NotifyFlag_string_to_int(const str& notifyflagstr);

    void _notifyGroupOfEnemy(void);      
    void _notifyGroupOfDamage(void);     
    void _notifyGroupOfKilled(void);     
    void _notifyGroupSpottedEnemy(void); 

    void Sleep(void);  
    void Sleep(Event *ev);   
    void Wakeup(void);
    void Wakeup(Event *ev);  

    qboolean ModeAllowed(int new_mode);   
    void     StartMode(int new_mode);     
    void     EndMode(void);               
    void     SaveMode(void);              
    void     RestoreMode(void);           
    void     ActivateAI(void);             
    void     GoIdle(Event *ev);            
    void     TurnAIOn(void);               
    void     TurnAIOff(void);              
    void     LevelAIOff(void);            
    void     LevelAIOn(void);             
    void     RemoveAnimDoneEvent(void);    
    void     SendMoveDone(CThread *script_thread);  
    void     SetGlobalState(const char *state_name);  
    void     RepostEvent(Event *ev, const Event& event_type);  
    void     BlindlyFollowPath(Event *ev);  

    void AddCustomThread(Event *ev);
    void AddCustomThread(const str& threadType, const str& threadName);  
    void RunCustomThread(const str& threadType);

    qboolean RespondToHitscan(void);

    void   turnTowardsEntity(Entity *ent, float extraYaw);
    void   SelectClosestEnemy(Event *ev);
    void   EvaluateEnemies(Event *ev);
    void   PrepareToFailMission(Event *ev);
    void   FailMission(Event *ev);
    void   PrintDebugMessage(Event *ev);
    Actor *GetAttachedChildActor(const str& childName);
    void   ChildUseBehaviorPackage(Event *ev);
    void   ChildUseBehaviorPackage(const str& childName, const str& packageName);  
    void   SetHeadWatchMaxDistance(Event *ev);
    void   ResetTorsoEvent(Event *ev);
    void   ClearArmorAdaptions(Event *ev);
    void   ResetMoveDir(Event *ev);
    void   Suicide(Event *ev);
    void   AnimateOnce(Event *ev);

    void ClearLegAnim(void);
    void ClearTorsoAnim(void);

    void EndBehavior(void);               
    void EndHeadBehavior(void);           
    void EndEyeBehavior(void);            
    void EndTorsoBehavior(void);          
    void EndBehaviorEvent(Event *ev);     
    void EndHeadBehaviorEvent(Event *ev); 
    void EndEyeBehaviorEvent(Event *ev);  
    void EndTorsoBehaviorEvent(Event *ev); 
    void NotifyBehavior(Event *ev);
    void NotifyTorsoBehavior(Event *ev);

    void  SetTendency(const str& name, float value);


    void FireProjectile(Event *ev);


    CurrentHelperNodeData_t currentHelperNode;

    IgnoreHelperNodeData_t ignoreHelperNode;

    MovementSubsystem *movementSubsystem;

    CombatSubsystem *combatSubsystem;
    void                    UnreserveCurrentHelperNode();



    bool IsEntityAlive(const Entity *ent);



    void      ProcessActorStateMachine(void);

    qboolean EntityInRange(Entity *ent, float range, float min_height, float max_height, bool XYOnly = false);

    qboolean checkAttacked();
    qboolean checkhaveenemy(Conditional& condition);                   
    qboolean checkcanseeenemy(Conditional& condition);                 
    qboolean checkcanshootenemy(Conditional& condition);               
    qboolean checkPatrolWaypointNodeInDistance(Conditional& condition);  
    qboolean checkWantsToExecutePackage(Conditional& condition);       
    qboolean checkExecutedPackageInLastTimeFrame(Conditional& condition);  
    qboolean checkAttackedByPlayer();
    qboolean checkDamageThresholdExceeded();

    qboolean EntityHasFireType(Entity *ent, firetype_t fire_type);

    void IncomingProjectile(Event *ev);


    qboolean checkCanAttackAnyEnemy(Conditional& condition);
    qboolean checkatcovernode(Conditional& condition);
    qboolean checkCaptured(Conditional& condition);
    qboolean checkWeaponReady(Conditional& condition);
    qboolean checkOutOfTorsoRange(Conditional& condition);
    qboolean checkInCallVolume(Conditional& condition);
    qboolean checkMeleeHitWorld(Conditional& condition);
    qboolean checkRegionDamaged(Conditional& condition);
    qboolean checkparentrange(Conditional& condition);
    qboolean checkPropEnemyRange(Conditional& condition);
    qboolean checkPropEnemyRange(const str &objname, const str &propname); 
    qboolean checkCanWalkForward(Conditional& condition);
    qboolean checkBlockedByEnemy(Conditional& condition);
    qboolean checkCoverNodes(Conditional& condition);
    qboolean checkPathNodeTypeInDistance(Conditional& condition);
    qboolean checkmovingactorrange(Conditional& condition);
    qboolean checkpartstate(Conditional& condition);
    qboolean checkpartdead(Conditional& condition);
    qboolean checkpartflag(Conditional& condition);
    qboolean checkHaveBestWeapon(Conditional& condition);
    qboolean checkHaveBestWeapon(); 
    qboolean checkPosture(Conditional& condition);
    qboolean checkPosture(const str &postureName); 

    qboolean checkplayerrange(Conditional& condition);
    qboolean checkstage(Conditional& condition);
    qboolean checkmeansofdeath(Conditional& condition);
    qboolean checkAllowedToMeleeEnemy(Conditional& condition);
    qboolean checkAllowedToMeleeEnemy(); 
    qboolean checkInPlayerConeOfFire(Conditional& condition);
    qboolean checkInTheWay(Conditional& condition);
    qboolean checkInTheWay(); 
    qboolean checkBoneDamaged(Conditional& condition);
    qboolean checkPlayerInCallVolume(Conditional& condition);
    qboolean checkcanseeplayer(Conditional& condition);
    qboolean checkotherdied(Conditional& condition);
    qboolean checkInConeOfFire(Conditional& condition);
    qboolean checkTalking(Conditional& condition);
    qboolean checkTalking(); 
    qboolean checkEnemyWithinRange(Conditional& condition);
    qboolean checkEnemyWithinRange(float min, float max); 
    qboolean checkhealth(Conditional& condition);
    qboolean checkhealthpercentinrange(Conditional& condition);
    qboolean checkActorType(Conditional& condition);
    qboolean checkchance(Conditional& condition);
    qboolean checkbehaviordone(Conditional& condition);
    qboolean checkInAIMode(Conditional& condition);
    qboolean checkCanAttackEnemy(Conditional& condition);
    qboolean checkCanAttackEnemy(); 
    virtual qboolean CanAttack(Entity *ent, qboolean usecurrentangles);  
    virtual qboolean CanAttackFrom(const Vector &pos, const Entity *ent, qboolean usecurrentangles);  
    qboolean checktimedone(Conditional& condition);
    qboolean checktorsobehaviordone(Conditional& condition);
    qboolean checkShowPain(Conditional& condition);
    qboolean checkShowPain(); 
    qboolean checksmallpain(Conditional& condition);
    qboolean checkTurretMode(Conditional& condition);
    qboolean checkHaveActiveWeapon(Conditional& condition);
    qboolean checkanimdone(Conditional& condition);
    qboolean checktorsoanimdone(Conditional& condition);
    qboolean checkShouldDoAction(Conditional& condition);
    qboolean checkShouldDoAction(const str &tendencyName); 
    qboolean checkHavePathToEnemy(Conditional& condition);
    qboolean checkHavePathToEnemy(); 
    qboolean checknopath(Conditional& condition);
    qboolean checknoiseheard(Conditional& condition);

    qboolean checkbehaviorsuccess(Conditional& condition);
    qboolean checkbehaviorfailed(Conditional& condition);
    qboolean checkValidCombatNodeInRange(Conditional& condition);
    qboolean checkValidCombatNodeInRange(float maxDistanceFromSelf, float minDistanceFromPlayer, bool unreserveCurrentNode = true); 
    qboolean checkValidCoverNodeInRange(Conditional& condition);
    qboolean checkValidCoverNodeInRange(float maxDistanceFromSelf, float minDistanceFromCurrentEnemy, float minDistanceFromPlayer); 
    qboolean checkValidPatrolNodeInRange(Conditional& condition);
    qboolean checkValidPatrolNodeInRange(float maxDistanceFromSelf); 
    qboolean checkValidWorkNodeInRange(Conditional& condition);
    qboolean checkValidWorkNodeInRange(float maxDistanceFromSelf, bool unreserveCurrentNode = true); 
    qboolean checkValidCustomNodeInRange(Conditional& condition);
    qboolean checkValidCustomNodeInRange(const str &customType, float maxDistanceFromSelf); 
    qboolean checkValidSniperNodeInRange(Conditional& condition);
    qboolean checkValidSniperNodeInRange(float maxDistanceFromSelf); 
    qboolean checkValidHibernateNodeInRange(Conditional& condition);
    qboolean checkValidHibernateNodeInRange(float maxDistanceFromSelf);  

    qboolean checkblocked(Conditional& condition);
    qboolean checkHaveArmor(Conditional& condition);
    qboolean checkHaveArmor(); 
    qboolean checkWeaponIsMelee(Conditional& condition);
    qboolean checkincomingmeleeattack(Conditional& condition);
    qboolean checkincomingmeleeattack(); 
    qboolean checkAnyEnemyInRange(Conditional& condition);
    qboolean checkAnyEnemyInRange(float range); 
    qboolean checkActorFlag(Conditional& condition);
    qboolean checkhasthing(Conditional& condition);
    qboolean checkCrippled(Conditional& condition);
    qboolean checkDisabled(Conditional& condition);
    qboolean checkInAlcove(Conditional& condition);
    qboolean checkPostureAnimDone(Conditional& condition);
    qboolean checkRequestedPosture(Conditional& condition);
    qboolean checkIsAggressive(Conditional& condition);
    qboolean checktorsobehaviorfailed(Conditional& condition);
    qboolean checkincomingprojectile(Conditional& condition);
    qboolean checkEnemyProjectileClose(Conditional& condition);
    qboolean checkEnemyProjectileClose(); 
    qboolean checkinactive(Conditional& condition);
    qboolean checkdead(Conditional& condition);
    qboolean checkenemydead(Conditional& condition);
    qboolean checkenemydead(void);  
    qboolean checkenemynoclip(Conditional& condition);
    qboolean checkenemyinfov(Conditional& condition);
    qboolean checkenemyrelativeyaw(Conditional& condition);
    qboolean checkenemyonground(Conditional& condition);
    qboolean checkcanjumptoenemy(Conditional& condition);
    qboolean checkcanflytoenemy(Conditional& condition);
    qboolean checkstunned(Conditional& condition);
    qboolean checkfinished(Conditional& condition);
    qboolean checkmeleehit(Conditional& condition);
    qboolean checkblockedhit(Conditional& condition);
    qboolean checkstuck(Conditional& condition);
    qboolean checkSteeringFailed(Conditional& condition);
    qboolean checkonfire(Conditional& condition);
    qboolean checkheadbehaviordone(Conditional& condition);
    qboolean checkeyebehaviordone(Conditional& condition);
    qboolean checktorsobehaviorsuccess(Conditional& condition);
    qboolean checkdone(Conditional& condition);
    qboolean checkEnemyAttached(Conditional& condition);
    qboolean checkstatetime(Conditional& condition);
    qboolean checktimesdone(Conditional& condition);
    qboolean checkcommand(Conditional& condition);
    qboolean checktouched(Conditional& condition);
    qboolean checktouchedbyplayer(Conditional& condition);
    qboolean checktouchedbyplayer(); 
    qboolean checkactivated(Conditional& condition);
    qboolean checkused(Conditional& condition);
    qboolean checktwitch(Conditional& condition);
    qboolean checkonground(Conditional& condition);
    qboolean checkinwater(Conditional& condition);
    qboolean checkincomingrangedattack(Conditional& condition);
    qboolean checkenemystunned(Conditional& condition);
    qboolean checkenemyinpath(Conditional& condition);
    qboolean checkheld(Conditional& condition);
    qboolean checknumspawns(Conditional& condition);
    qboolean checkallowhangback(Conditional& condition);
    qboolean checkname(Conditional& condition);
    qboolean checkanimname(Conditional& condition);
    qboolean checkNodeExists(Conditional& condition);
    qboolean checkSurfaceDamaged(Conditional& condition);
    qboolean checkHasThrowObject(Conditional& condition);
    qboolean checkEnemyIsThrowObject(Conditional& condition);
    qboolean checkPlayerValid(Conditional& condition);
    qboolean checkInPreferredRange(Conditional& condition);
    qboolean checkInAbsoluteRange(Conditional& condition);
    qboolean checkUsingWeaponNamed(Conditional& condition);
    qboolean checkUsingWeaponNamed(const str &name);  
    qboolean checkIsTeammate(Conditional& condition);
    qboolean checkLastState(Conditional& condition);
    qboolean checkWeaponChanged(Conditional& condition);
    qboolean checkPropExists(Conditional& condition);
    qboolean checkPropChance(Conditional& condition);
    const str GetStateVar(const str& varName);
    bool RequestControl(Listener *controller, ActorControlType controlType = ACTOR_CONTROL_AUTO_RELEASE);
    bool ReleaseControl(Listener *controller);

    float dialog_done_time;  

    float next_blink_time;
    void  TryBlink(void);

    class HeadWatcher *headWatcher;
    float              real_head_pitch;

    float              maxEyeYawAngle;
    float              minEyeYawAngle;
    float              maxEyePitchAngle;
    float              minEyePitchAngle;
    void               SetEyeAngles(Event *ev);

    void               HeadWatchEvent(Event *ev);
    void               ResetHeadEvent(Event *ev);
    void               SetHeadWatchTarget(Event *ev);
    void InitHeadWatcher(void);
    void FindPartActor(void);
    void InitThinkStrategy(void);
    void SetAllowFall(Event *ev);
    void SetNotAllowedToKill(Event *ev);

    Actor *FindPartActor(const char *name);  
    static Actor *FindActorByName(const str &name);  
    void InitStrategos(void);
    void InitSensoryPerception(void);
    void InitPersonality(void);
    void InitPackageManager(void);
    void InitGameComponent(void);
    void InitEnemyManager(void);
    void SetVarTime(Event *ev);
    void SetVar(Event *ev);
    void               SetHeadWatchTarget(Entity *ent);

    void  SetNodeID(Event *ev);     
    void  UseActorWeapon(Event *ev); 
    void  PlayDialog(
        Sentient   *user,
        float       volume = -1.0f,
        float       min_dist = -1.0f,
        const char *dialog_name = NULL,
        const char *state_name = NULL,
        qboolean    headDisplay = false,
        bool        useTalk = false,
        bool        important = false);
    void  PlayRadiusDialog(Sentient *user); 
    void  StopDialog(void);
    float GetDialogRemainingTime(void);


    DialogNode_t *dialog_list;

    bool  _ignoreNextContext;    
    str   _nextContextToIgnore;
    float _nextContextTime;      
    float _contextInterval;      

    const str FindDialog(Sentient *user, DialogType_t dialogType, const str& context = "");
    qboolean  WantsToTalk(void);
    void      InContext(const str& theContext, bool useDefaultMinDist);
    void      BroadcastDialog(Event *ev);
    void      SetIgnoreNextContext(Event *ev);
    void      SetContextInterval(Event *ev);
    void      SetContextInterval(float interval); 
    void      SetTalkiness(Event *ev);

    void AddDialogParms(DialogNode_t *dialog_node, Event *ev);
    void SetTalkWatchMode(Event *ev);

    void setBranchDialog(void);
    void clearBranchDialog(void);


    unsigned int        actor_flags1;
    unsigned int        actor_flags2;
    unsigned int        actor_flags3;
    unsigned int        actor_flags4;
    Container<StateVar *> stateVarList;

    qboolean GetActorFlag(int flag) const;
    qboolean GetActorFlag(const str& flag_name) const;  
    void     SetActorFlag(int flag, qboolean value);
    void     SetActorFlag(Event *ev);  
    void     AnimDone(Event *ev);  
    int      ActorFlag_string_to_int(const str& flag_name) const;



    float getForwardSpeed(void) const;          
    void  ForwardSpeedEvent(Event *ev);  

    trace_t Trace(const Vector& end, const char *reason) const;
    trace_t Trace(const float distance, const char *reason) const;
    trace_t Trace(const float angle, const float distance, const char *reason) const;
    trace_t Trace(const Vector& begin, const Vector& end, const char *reason) const;
    trace_t Trace(const Vector& begin, const Vector& end, const int contentMask, const char *reason) const;

    qboolean CanJump(void);

    void StrictlyFollowPath(Event *ev);          
    void SetSteeringDirectionPreference(Event *ev); 

    void  WarpTo(Event *ev);      
    void SetDisabled(Event *ev);  
    void SetCrippled(Event *ev);  
    void SetInAlcove(Event *ev);  

    // read by nothing in this port yet -- that is deliberate.  A retail `.sav`
    str    newanim;                         
    int    newanimnum;                      
    int    animnum;                         
    str    animname;                        
    Event *newanimevent;                    
    str    last_anim_event_name;            
    str    newTorsoAnim;                    
    int    newTorsoAnimNum;                 
    str    TorsoAnimName;                   
    Event *newTorsoAnimEvent;               
    str    last_torso_anim_event_name;      

    float absoluteMin;                      
    float absoluteMax;                      
    float preferredMin;                     
    float preferredMax;                     

    targetType_t targetType;                
    qboolean     validTarget;               

    BehaviorReturnCode_t behaviorCode;      
    BehaviorReturnCode_t headBehaviorCode;  
    BehaviorReturnCode_t eyeBehaviorCode;   
    BehaviorReturnCode_t torsoBehaviorCode; 

    str currentBehavior;         
    str currentHeadBehavior;     
    str currentEyeBehavior;      
    str currentTorsoBehavior;    

    str behaviorFailureReason;              

    qboolean haveThrowObject;               
    str      animset;                       
    unsigned int notify_flags1;             

    float        chattime;                  
    float        nextsoundtime;             
    DialogMode_t DialogMode;                
    float        radiusDialogRange;         
    str          dialog_state_name;         
    str          dialog_old_state_name;     
    str          _branchDialogName;         

    SafePtr<CThread> scriptthread;          
    str       escape_thread;                
    str       captured_thread;              
    str       activate_thread;              
    str       alert_thread;                 
    str       idle_thread;                  
    Container<threadlist_t *> threadList;

    float next_drown_time;                  
    float air_finished;                     
    int   bullet_hits;                      
    str   _deathEffect;                     


    float state_time;                       
    float masterstate_time;                 
    int   times_done;                       
    int   masterstate_times_done;           
    float state_done_time;                  
    float masterstate_done_time;            
    float last_time_active;                 
    stateDebugType_t showStates;            
    talkModeStates_t talkMode;              
    bool  useConvAnims;                     

    int   saved_mode;                       
    BehaviorPtr saved_behavior;            
    BehaviorPtr saved_headBehavior;        
    BehaviorPtr saved_eyeBehavior;         
    BehaviorPtr saved_torsoBehavior;       
    SafePtr<CThread> saved_scriptthread;    
    SafePtr<CThread> saved_actorthread;     
    str   saved_anim_name;                  
    str   saved_state_name;                 
    str   saved_anim_event_name;            

    str               part_name;            
    Container<part_t> parts;                

    EntityPtr incoming_proj;                
    float     incoming_time;                
    qboolean  incoming_bullet;              

    float     max_inactive_time;            
    float     last_jump_time;               
    str       enemytype;                    
    float     actorrange_time;              
    float     last_height;                  
    EntityPtr last_ent;                     
    float     canseeenemy_time;             
    float     canseeplayer_time;            
    int       stage;                        
    int       num_of_spawns;                
    ActorPtr  spawnparent;                  
    Vector    last_attack_pos;              
    Vector    last_attack_enemy_pos;        
    EntityPtr last_attack_entity_hit;       
    Vector    last_attack_entity_hit_pos;   
    int       mode;                         
    Vector    last_known_enemy_pos;         
    Vector    last_known_player_pos;        
    float     next_find_enemy_time;         
    float     minimum_melee_height;         
    float     damage_angles;                
    float     next_pain_sound_time;         
    float     last_ground_z;                
    str       emotion;                      
    float     actor_to_actor_damage_modifier;  
    float     hitscan_response_chance;      
    int       shotsFired;                   
    float     timeBetweenSleepChecks;       
    float     deathKnockbackVerticalValue;    
    float     deathKnockbackHorizontalValue;  

    ListenerPtr      _controller;           
    ActorControlType _controlType;          

    FollowTargetData_t followTarget;        
    int   _steeringDirectionPreference;     

    EntityPtr trigger;                      
    str       command;                      
    str       idle_state_name;              
    str       master_idle_state_name;       
    str       global_state_name;            
    float     next_player_near;             
    EntityPtr pickup_ent;                   
    float     stunned_end_time;             
    Container<str> spawn_items;             
    float     spawn_chance;                 
    void      SpawnItems(void);            
    void      SpawnItem(const str& spawn_item_name); 
    void      AddSpawnItem(Event *ev);     
    void      ClearSpawnItems(Event *ev);  
    Container<int> can_be_finsihed_by_mods; 
    qboolean  haveAttached;                 
    float     currentSplineTime;            
    float     _dialogMorphMult;             
    weaponhand_t _useWeaponDamage;          
    float     _nextCheckForWorkNodeTime;    
    float     _nextCheckForHibernateNodeTime;  
    float     _nextCheckForEnemyPath;       
    bool      _havePathToEnemy;             
    float     _nextPathDistanceToFollowTargetCheck;  
    bool      _checkedChance;               
    bool      _levelAIOff;                  
    float     _nextPlayPainSoundTime;       
    float     _playPainSoundInterval;       

    float    lastPathCheck_Work;            
    float    lastPathCheck_Flee;            
    float    lastPathCheck_Patrol;          
    qboolean testing;                       

    ActorThink         *thinkStrategy;       
    ActorGameComponent *gameComponent;       
    SensoryPerception  *sensoryPerception;   
    Strategos          *strategos;           
    EnemyManager       *enemyManager;        
    PackageManager     *packageManager;      
    Personality        *personality;         
    PostureController  *postureController;   




    void SetPostureState(Event *ev);
    void PostureAnimDone(Event *ev);
    void InitPostureController(void);  
    void TurnTowardsEnemy(Event*) ;
    void TurnTowardsPlayer(Event*) ;
    void TurnTowardsEntity(Event*) ;
    qboolean SetAnim(const str&, Event&, bodypart_t = legs, const float = 1.0f) ;
    void SetAnim(Event *ev);  
    void AttachCurrentEnemy(Event*) ;
    void AttachModelToTag(const str&, const str&) ;
    void ChangeType(Event*) ;
    void ChildSetAnim(Event*);
    void ChildSetAnim(const str& childName, const str& animName);  
    void ChildSuicide(Event*) ;
    void CinematicAnimDone() ;
    void AllowHangBack(Event*) ;
    void AllowTalk(Event*) ;
    void DeathFadeEvent(Event*) ;
    void DeathShrinkEvent(Event*) ;
    void DeathSinkEvent(Event*) ;
    void DebugEvent(Event*) ;
    void DebugStates(Event*) ;
    void DetachModelFromTag(const str&) ;
    void EndAllBehaviors() ;
    void FallToDeathEvent(Event*) ;
    void FollowWayPoints(Event*) ;
    void ForceSetClip(Event*) ;
    void GetStateAnims(Container<const char*>*)  override;
    void GroupMemberInjured(Event*) ;
    void HandleGameSpecificEvent(Event*) ;
    void HelperNodeCommand(Event*) ;
    void InitMasterState() ;
    void InitState() ;
    void JumpToEvent(Event*) ;
    void KilledEffects(Entity*) ;
    void LoadPostureStateMachine(Event*) ;
    void NotifyEyeBehavior(Event*) ;
    void NotifyHeadBehavior(Event*) ;
    void PickupEnt(Event*) ;
    void PickupThrowObject(Event*) ;
    void PrintBehaviorPackageInfo() ;
    void PrintMasterStateInfo() ;
    void PrintStateMapInfo() ;
    bool InFOV(Entity*) ;

    float  feet_width;
    Vector last_origin;

    void ActivateEvent(Event *ev);
    void SetSimplifiedThink(Event *ev);
    void SetActorToActorDamageModifier(Event *ev);
    void UseEvent(Event *ev);
    void Start(Event *ev);
    void Pain(Event *ev);
    void Killed(Event *ev, bool bPlayDeathAnim);
    void SwimEvent(Event *ev);
    void NotLandEvent(Event *ev);
    void SetDialogMode(Event *ev);
    void DialogAnimDone(Event *ev);
    void IfEnemyVisibleEvent(Event *ev);
    void IfNearEvent(Event *ev);
    void Anim(Event *ev);
    void AttackPlayer(Event *ev);
    void ReserveNodeEvent(Event *ev);
    void ReleaseNodeEvent(Event *ev);
    void IfCanHideAtEvent(Event *ev);
    void IfEnemyWithinEvent(Event *ev);
    void MeleeEvent(Event *ev);
    void SetHealth(Event *ev);
    void SetMaxHealth(Event *ev);
    void NoChatterEvent(Event *ev);
    virtual void Chatter(const char *sound, float chance = 10, float volume = 1.0f, int channel = CHAN_VOICE);  
    void SetTurnSpeed(Event *ev);
    void SetNotifyFlag(Event *ev);
    void TorsoAnimDone(Event *ev);
    void FireBullet(Event *ev);
    void FireRadiusAttack(Event *ev);
    void TurnAIOn(Event *ev);
    void TurnAIOff(Event *ev);
    void SetIdleThread(Event *ev);
    void RegisterParts(Event *ev);
    void RegisterSelf(Event *ev);
    void PartName(Event *ev);
    void Name(Event *ev); 
    void SendCommand(Event *ev);
    void SetupTriggerField(Event *ev);
    void TriggerTouched(Event *ev);
    void AddDialog(Event *ev);
    void DialogDone(Event *ev);
    void PlayDialog(Event *ev);
    void StopDialog(Event *ev);
    void SetMouthAngle(Event *ev);
    void SolidMask(Event *ev);
    void IgnoreMonsterClip(Event *ev);
    void NotSolidMask(Event *ev);
    void NoMask(Event *ev);
    void SetMask(Event *ev);
    void Pickup(Event *ev);
    void Throw(Event *ev);
    void DamageOnceStart(Event *ev);
    void DamageOnceStop(Event *ev);
    void DamageEnemy(Event *ev);
    void DamageSelf(Event *ev);
    void GotoNextStage(Event *ev);
    void GotoPrevStage(Event *ev);
    void GotoStage(Event *ev);
    void GetStage(Event *ev);
    void SetHaveThing(Event *ev);
    void SetHaveThing(int thing_number, qboolean thing_bool);  
    void SetUseGravity(Event *ev);
    void SetSpawnChance(Event *ev);
    void SetCanBeFinishedBy(Event *ev);
    qboolean CanBeFinished(void);            
    qboolean CanBeFinishedBy(int meansofdeath); 
    void     Finish(int meansofdeath);       
    void     StartLimbo(void);               
    qboolean InLimbo(void);                  
    void     BranchDialog(Event *ev);        
    void     NotifyOthersAtDeath(Event *ev); 
    qboolean DialogExists(const str& aliasName); 
    void     FreeDialogList(void);           
    DialogNode_t *NewDialogNode(void);       
    void     SaveAttack(const Vector& orig, const Vector& dir); 
    qboolean TestAttack(const str& tag_name); 
    qboolean TakeDamage(void);               
    void     InitMovementSubsystem(void);    
    void     InitCombatSubsystem(void);      
    void SetFeetWidth(Event *ev);
    void SetCanWalkOnOthers(Event *ev);
    void Push(Event *ev);
    void Pushable(Event *ev);
    void ChargeWater(Event *ev);
    void SetTargetable(Event *ev);
    void IgnorePlacementWarning(Event *ev);
    void TouchTriggers(Event *ev);
    void IgnoreWater(Event *ev);
    void SimplePathfinding(Event *ev);
    void NoPainSounds(Event *ev);
    void UpdateBossHealth(Event *ev);
    void SetMaxBossHealth(Event *ev);
    void IgnorePainFromActors(Event *ev);
    void DamageAllowed(Event *ev);
    void SetEmotion(Event *ev);
    void ReturnProjectile(Event *ev);
    void SetRadiusDialogRange(Event *ev);
    void RunAlertThread(Event *ev);
    void checkActorDead(Event *ev);
    qboolean checkActorDead(void);  
    void SetActorType(Event *ev);
    void StopFireWeapon(Event *ev);
    void SetAggressiveness(Event *ev);
    void SetGroupNumber(Event *ev);
    void SetTendency(Event *ev);
    void SetBlink(Event *ev);
    void SetStickToGround(const bool stick);  
    void SetStickToGround(Event *ev);
    const bool GetStickToGround(void) const;  
    void InContext(Event *ev);
    void UnreserveCurrentHelperNode(Event *ev);
    void ProjectileClose(Event *ev);
    void RealSpawnGib(Event *ev);
    void RemoveUselessBody(Event *ev);
    void ResetEyeEvent(Event *ev);
    void SelectNextEnemy(Event *ev);
    void SetAbsoluteMax(Event *ev);
    void SetAbsoluteMin(Event *ev);
    void SetActivateThread(Event *ev);
    void SetAlertThread(Event *ev);
    void SetAnimSet(Event *ev);
    void SetAnimSet(const str &animSet);
    const str& GetAnimSet(void);  
    void SetBehavior(Event *ev);
    void SetBleedAfterDeath(Event *ev);
    void SetBounceOffVelocity(Event *ev);
    void SetCinematicAnim(Event *ev);
    void SetCombatTraceInterval(Event *ev);
    void SetDamageAngles(Event *ev);
    void SetDeathKnockbackValues(Event *ev);
    void SetDeathSize(Event *ev);
    void SetDieCompletely(Event *ev);
    void SetEnemyAttached(Event *ev);
    void SetEnemyTargeted(Event *ev);
    void SetEnemyTargeted(bool targeted);
    void SetEnemyType(Event *ev);
    void SetEyeBehavior(Event *ev);
    void SetFOV(Event *ev);
    void SetFlagOnEnemy(Event *ev);
    void SetFollowCombatRange(Event *ev);
    void SetFollowCombatRangeMin(Event *ev);
    void SetFollowRange(Event *ev);
    void SetFollowRangeMin(Event *ev);
    void SetFollowTarget(Event *ev);
    void SetHeadBehavior(Event *ev);
    void SetHeadWatchSpeed(Event *ev);
    void SetHitscanResponse(Event *ev);
    void SetIdleStateName(Event *ev);
    void SetIgnoreWatchTarget(Event *ev);
    void SetImmortal(Event *ev);
    void SetMaxHeadPitch(Event *ev);
    void SetMaxHeadYaw(Event *ev);
    void SetMinimumMeleeHeight(Event *ev);
    void SetPackageTendency(Event *ev);
    void SetPlayPainSoundInterval(Event *ev);
    void SetPreferredMax(Event *ev);
    void SetPreferredMin(Event *ev);
    void SetSelfDetonateModel(Event *ev);
    void SetTakeDamage(Event *ev);
    void SetTargetType(Event *ev);
    void SetTimeBetweenSleepChecks(Event *ev);
    void SetTorsoBehavior(Event *ev);
    void SetValidTarget(Event *ev);
    void SetWeaponReady(Event *ev);
    void SpawnActor(Event *ev);
    void SpawnActorAboveEnemy(Event *ev);
    void SpawnActorAtLocation(Event *ev);
    void SpawnActorAtTag(Event *ev);
    void SpawnBlood(Event *ev);
    void SpawnGib(Event *ev);
    void SpawnGibAtTag(Event *ev);
    void SpawnNamedGib(Event *ev);
    void StaySolidEvent(Event *ev);
    void StunEvent(Event *ev);
    void ThrowEnt(Event *ev);
    void TossThrowObject(Event *ev);
    void Touched(Event *ev);
    void TryTalkToPlayer(Event *ev);
    void TurnOffEnemyAI(Event *ev);
    void TurnOnEnemyAI(Event *ev);
    void TurnToEvent(Event *ev);
    void UnRegisterBehaviorPackage(Event *ev);
    void UseWeaponDamage(Event *ev);
    void WalkWatch(Event *ev);
    void WhatAreYouDoing(Event *ev);
    void WhatsWrong(Event *ev);




public:
    void Killed( Event *ev ); 
    void BroadcastAlert( float pos, int soundtype ); 
    void Active( Event *ev ); 
    void ClearTorsoAnim( Event *ev ); 
    void LoadFuzzyEngine( Event *ev ); 
    void LoadMasterStateMap( Event *ev ); 
    void LoadStateMap( Event *ev ); 
    void RealSpawnGib( qboolean use_tag, Event *ev ); 
    void RegisterBehaviorPackage( Event *ev ); 
    void SetBehavior( Behavior *newbehavior, Event *argevent = NULL, CThread *thread = NULL ); 
    void SetCinematicAnim( const str &anim ); 
    void SetEyeBehavior( Behavior *newbehavior, Event *argevent = NULL, CThread *thread = NULL ); 
    void SetHeadBehavior( Behavior *newbehavior, Event *argevent = NULL, CThread *thread = NULL ); 
    void SetIgnoreWatchTarget( bool ignore ); 
    void SetMasterState( Event *ev ); 
    void SetState( const char *state_name ); 
    void SetTorsoBehavior( Behavior *newbehavior, Event *argevent = NULL, CThread *thread = NULL ); 
    void SetVisionDistance( Event *ev ); 
    void SpawnActor( const str &model_name, const Vector &orig, const Vector &ang, int how_many, qboolean attack, float width, float height, qboolean force = false ); 
    void TryTalkToPlayer( void ); 
    void UseBehaviorPackage( Event *ev ); 
    void resetStateMachine( void ); 
    void setDeathEffect( Event *ev ); 
    void setDialogMorphMult( Event *ev ); 
    qboolean GetClosestTag( const str &tag_name, int number_of_tags, const Vector &target, Vector *orig ); 
    float SpawnGetTime( float vel, const Vector &orig, const Vector &gib_mins, const Vector &gib_maxs ); 
    qboolean _FleeNodeInDistance( float dist ); 
    qboolean _WorkNodeInDistance( float dist ); 
    qboolean _isWorkNodeValid(PathNode* node); 
    PathNode *NearestNodeInPVS(const Vector &pos);  
    qboolean CloseToEnemy(const Vector &pos, float howclose);  
    qboolean checkActivationDelayTime( Conditional &condition ); 
    qboolean checkActivationDelayTime(); 
    qboolean checkAttacked( Conditional &condition ); 
    qboolean checkAttackedByPlayer( Conditional &condition ); 
    qboolean checkCountOfIdenticalNamesInGroup( Conditional &condition ); 
    qboolean checkCountOfIdenticalNamesInGroup( const str &checkName, int checkValue ); 
    qboolean checkCurrentEnemyGroupAttackerCount( Conditional &condition ); 
    qboolean checkCurrentNodeHasThisCoverType( Conditional &condition );
    qboolean checkCurrentNodeHasThisCoverType(const str &coverType); 
    qboolean checkDamageThresholdExceeded( Conditional &condition ); 
    qboolean checkEnemiesNearby( Conditional &condition );
    qboolean checkEnemiesNearby(float distance); 
    qboolean checkEnemyCanSeeCurrentNode( Conditional &condition ); 
    qboolean checkEnemyCanSeeCurrentNode(); 
    qboolean checkEnemyWeaponNamed( Conditional &condition );
    qboolean checkEnemyWeaponNamed(const str& name); 
    qboolean checkForwardDirectionClear( Conditional &condition );
    qboolean checkForwardDirectionClear(float dist); 
    qboolean checkGameSpecific( Conditional &condition ); 
    qboolean checkGroupAttackerCount( Conditional &condition ); 
    qboolean checkGroupAttackerCountForEntity( Conditional& condition, Entity* attackTarget );
    qboolean checkGroupAttackerCountForEntity(int checkValue, Entity* attackTarget); 
    qboolean checkGroupMememberRange ( Conditional &condition ); 
    qboolean checkHelperNodeWithFlagInRange( Conditional &condition ); 
    qboolean checkHelperNodeWithFlagInRange(const str &flag, float range); 
    qboolean checkLeftDirectionClear( Conditional &condition );
    qboolean checkLeftDirectionClear(float dist); 
    qboolean checkPlayerWeaponNamed( Conditional &condition );
    qboolean checkPlayerWeaponNamed(const str& name); 
    qboolean checkRearDirectionClear( Conditional &condition );
    qboolean checkRearDirectionClear(float dist); 
    qboolean checkRightDirectionClear( Conditional &condition ); 
    qboolean checkRightDirectionClear(float dist); 
    qboolean checkSpecifiedFollowTargetOutOfRange( Conditional &condition ); 
    qboolean checkSpecifiedFollowTargetOutOfRange(); 
    qboolean checkVar( Conditional &condtion ); 
    qboolean checkVarTimeDifference( Conditional &condition ); 
    qboolean checkWithinFollowRangeMin( Conditional &condition ); 
    qboolean checkWithinFollowRangeMin(); 
    qboolean checkenemymelee( Conditional &condition ); 
    qboolean checkenemyrange( Conditional &condition ); 
    qboolean checkenemyranged( Conditional &condition ); 
    qboolean checkenemyyawrange( Conditional &condition ); 
    qboolean checkhealthpercent( Conditional &condition ); 
    qboolean checkinpain( Conditional &condition ); 
    qboolean checkpainpitch( Conditional &condition ); 
    qboolean checkpainyaw( Conditional &condition ); 
    qboolean checkplayerranged( Conditional &condition ); 
    qboolean checkplayerranged(); 
    bool canBeDamagedBy(meansOfDeath_t MeansOfDeath ); 
    qboolean CheckBottom( void ); 
    qboolean returntrue( Conditional &condition ); 
    void setHeadTwitch( Event *ev ); 
    void _printDebugInfo(const str &laststate, const str &currentState, const str &legAnim, const str &torsoAnim);  
};
















#include "actorutil.h"

typedef PathFinder<FindMovement> FindMovementPath;
typedef PathFinder<FindCoverMovement> FindCoverPath;
typedef PathFinder<FindFleeMovement> FindFleePath;
typedef PathFinder<FindEnemyMovement> FindEnemyPath;












