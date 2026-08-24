//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actorincludes.h                                $
// $Revision:: 84                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
//
// DESCRIPTION:
// Centralized repository for externs, defines, enums, and structs that are required
// by actor and all of actor's helper classes, like sensoryPerception, thinkStrategy, etc...
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __ACTORINCLUDES_H__
#define __ACTORINCLUDES_H__

#include "helper_node.h" 
class Actor;            
class Sentient;         

extern Event EV_Actor_Start;
extern Event EV_Actor_LookAt;
extern Event EV_Actor_TurnTo;
extern Event EV_Actor_BehaviorFinished ;
extern Event EV_Actor_ControlLost ;
extern Event EV_Actor_EndBehavior;
extern Event EV_Actor_EndHeadBehavior;
extern Event EV_Actor_EndEyeBehavior;
extern Event EV_Actor_NotifyBehavior;
extern Event EV_Actor_NotifyTorsoBehavior;
extern Event EV_Actor_NotifyHeadBehavior;
extern Event EV_Actor_NotifyEyeBehavior;
extern Event EV_Actor_FinishedMove;
extern Event EV_Actor_FinishedAnim;
extern Event EV_Actor_WalkTo;
extern Event EV_Actor_FallToDeath;
extern Event EV_Actor_RunTo;
extern Event EV_Actor_Anim;
extern Event EV_Actor_AttackFinished;
extern Event EV_Actor_Attack;
extern Event EV_Actor_AttackPlayer;
extern Event EV_Actor_AIOn;
extern Event EV_Actor_AIOff;
extern Event EV_Actor_AIDeaf;
extern Event EV_Actor_AIDumb;
extern Event EV_Actor_RespondTo;
extern Event EV_Actor_PermanentlyRespondTo;
extern Event EV_ActorIncomingProjectile;
extern Event EV_Anim_Done;
extern Event EV_Torso_Anim_Done;  
extern Event EV_ActorOnlyShootable;
extern Event EV_Actor_BounceOff;
extern Event EV_Actor_Push;
extern Event EV_Actor_Statemap;
extern Event EV_Actor_SetTargetable;
extern Event EV_Actor_SetUseGravity ;
extern Event EV_Actor_SetSimplifiedThink ;
extern Event EV_Actor_SetStickToGround ;
extern Event EV_Actor_SetMovementMode ;
extern Event EV_Sentient_StopFire;
extern Event EV_Sentient_Attack;
extern Event EV_Sentient_GroupMemberInjured;

#define MAX_ALIAS_NAME_LENGTH     32
#define MAX_INACTIVE_TIME         30.0
#define DEFAULT_FOV               150
#define DEFAULT_VISION_DISTANCE   1536
#define MIN_SIGHT_DELAY           2
#define RANDOM_SIGHT_DELAY        1.5
#define DEFAULT_INITIAL_HATE      100
#define TURN_SPEED		          60 
#define HELPER_NODE_MAX_DISTANCE  96.0
#define HELPER_NODE_ARRIVAL_DISTANCE 24.0

#define HACK_PATH_CHECK           1.0



#define STIMULI_ALL    -1
#define STIMULI_NONE    0
#define STIMULI_SIGHT   (1<<0)
#define STIMULI_SOUND   (1<<1)
#define STIMULI_PAIN    (1<<2)
#define STIMULI_SCRIPT  (1<<3)

#define ACTOR_MOUTH_TAG         0
#define ACTOR_HEAD_TAG          1
#define ACTOR_TORSO_TAG         2
#define ACTOR_LEYE_TAG   3
#define ACTOR_REYE_TAG   4


#define MAX_DIALOG_PARAMETERS_LENGTH   100
#define MAX_DIALOG_PARM_LENGTH         64
#define MAX_DIALOG_PARMS               10
#define DIALOG_PARM_TYPE_NONE          0
#define DIALOG_PARM_TYPE_PLAYERHAS     1
#define DIALOG_PARM_TYPE_PLAYERHASNOT  2
#define DIALOG_PARM_TYPE_HAS           3
#define DIALOG_PARM_TYPE_HASNOT        4
#define DIALOG_PARM_TYPE_DEPENDS       5
#define DIALOG_PARM_TYPE_DEPENDSNOT    6
#define DIALOG_PARM_TYPE_DEPENDSINT    7
#define DIALOG_PARM_TYPE_CONTEXT_INITIATOR 8
#define DIALOG_PARM_TYPE_CONTEXT_RESPONSE  9

#define STATE_FLAG_IN_PAIN					 ( 1<<0  )
#define STATE_FLAG_MELEE_HIT				 ( 1<<1  )
#define STATE_FLAG_TOUCHED					 ( 1<<2  )
#define STATE_FLAG_ACTIVATED				 ( 1<<3  )
#define STATE_FLAG_USED				         ( 1<<4  )
#define STATE_FLAG_TWITCH			         ( 1<<5  )
#define STATE_FLAG_BLOCKED_HIT	             ( 1<<6  )
#define STATE_FLAG_SMALL_PAIN		         ( 1<<7  )
#define STATE_FLAG_OTHER_DIED		         ( 1<<8  )
#define STATE_FLAG_STUCK			         ( 1<<9  )
#define STATE_FLAG_NO_PATH			         ( 1<<10 )
#define STATE_FLAG_TOUCHED_BY_PLAYER	  	 ( 1<<11 )
#define STATE_FLAG_CHANGED_WEAPON			 ( 1<<12 )
#define STATE_FLAG_DAMAGE_THRESHOLD_EXCEEDED ( 1<<13 )
#define STATE_FLAG_ATTACKED					 ( 1<<14 )
#define STATE_FLAG_ATTACKED_BY_PLAYER		 ( 1<<15 )
#define STATE_FLAG_SHOW_PAIN				 ( 1<<16 )
#define STATE_FLAG_IN_THE_WAY				 ( 1<<17 )
#define STATE_FLAG_STEERING_FAILED			 ( 1<<18 )
#define STATE_FLAG_BLOCKED_BY_ENTITY		 ( 1<<19 )
#define STATE_FLAG_ENEMY_PROJECTILE_CLOSE	 ( 1<<20 )

#define DEFAULT_TRACE_INTERVAL   .05f

#define DEFAULT_PATH_TO_ENEMY_INTERVAL .05f

#define ACTOR_MODE_NONE			0
#define ACTOR_MODE_IDLE			1
#define ACTOR_MODE_AI			2
#define ACTOR_MODE_SCRIPT		3
#define ACTOR_MODE_TALK			4

#define PAIN_SMALL  0
#define PAIN_BIG    1

#define SAVE_FLAG_NEW_ANIM						(1<<0)
#define SAVE_FLAG_FORWARD_SPEED				(1<<1)
#define SAVE_FLAG_BEHAVIOR						(1<<2)
#define SAVE_FLAG_PATH							(1<<3)
#define SAVE_FLAG_NOISE							(1<<4)
#define SAVE_FLAG_SCRIPT_THREAD				(1<<5)
#define SAVE_FLAG_ACTOR_THREAD				(1<<6)
#define SAVE_FLAG_KILL_THREAD					(1<<7)
#define SAVE_FLAG_STATE							(1<<8)
#define SAVE_FLAG_IDLE_THREAD					(1<<7)
#define SAVE_FLAG_PARTS							(1<<10)
#define SAVE_FLAG_TRIGGER						(1<<11)
#define SAVE_FLAG_STATE_FLAGS					(1<<12)
#define SAVE_FLAG_COMMAND						(1<<13)
#define SAVE_FLAG_STAGE							(1<<14)
#define SAVE_FLAG_NUM_OF_SPAWNS				(1<<15)
#define SAVE_FLAG_SPAWN_PARENT				(1<<16)
#define SAVE_FLAG_DIALOG						(1<<17)
#define SAVE_FLAG_SAVED_STUFF					(1<<18)
#define SAVE_FLAG_LAST_ANIM_EVENT			(1<<19)
#define SAVE_FLAG_PICKUP_ENT					(1<<20)
#define SAVE_FLAG_PAIN							(1<<21)
#define SAVE_FLAG_SPAWN_ITEMS					(1<<22)

#define DEFAULT_EVALUATE_INTERVAL     .05f 
#define DEFAULT_SIGHT_BASED_HATE      5.0f



#ifndef __ACTORINCLUDES_ENUMS_DEFINED__
#define __ACTORINCLUDES_ENUMS_DEFINED__
typedef struct
{
	byte type;
	char parm[ MAX_DIALOG_PARM_LENGTH ];
	char parm2[ MAX_DIALOG_PARM_LENGTH ];
} DialogParm_t;

typedef enum
{
	DIALOG_TYPE_NORMAL,
	DIALOG_TYPE_RADIUS,
	DIALOG_TYPE_GREETING,	
	DIALOG_TYPE_COMBAT,
	DIALOG_TYPE_CONTEXT_INITIATOR,
	DIALOG_TYPE_CONTEXT_RESPONSE
} DialogType_t;


typedef struct
   {
   int entNum;
   float time;
   qboolean inLineOfSight;
   } LineOfSight_t;

typedef struct
{

  EntityPtr enemy;               
  float     lastSightTime;       
  float     nextSightTime;       
  qboolean  canSee;              
  float     damageCaused;        
  float     hate;                

  float     lastDistance;

} HateListEntry_t;

typedef struct
{
  str packageName;
  str stateFile;
} BehaviorPackageType_t;

typedef struct
{
   HelperNodePtr node;
   int         mask;
   int         nodeID;
} CurrentHelperNodeData_t;

typedef struct
{
   HelperNodePtr node;
   int         mask;
   int         nodeID;
} IgnoreHelperNodeData_t;

typedef struct
{
   EntityPtr	currentFollowTarget;
   EntityPtr	specifiedFollowTarget;
   float		maxRangeIdle;
   float		minRangeIdle;
   float		maxRangeCombat;
   float		minRangeCombat;
} FollowTargetData_t;

typedef struct
{
  int          packageIndex;
  float        currentScore;
  float        lastScore;  
  float        lastTimeExecuted;
  float        priority;

} BehaviorPackageEntry_t;

typedef struct
{
  int          packageIndex;
  float        tendency;
  float        lastTendencyCheck;
} PackageTendency_t;

typedef struct
{
  str tendencyName;
  float tendencyValue;
} Tendency_t;

typedef struct
{
	str varName;
	str varValue;
	float varTime;
} StateVar;

typedef struct part_t
{
	EntityPtr ent;
	unsigned int state_flags;

	part_t &operator=( const part_t &other );
} part_t;

typedef struct
{
	str threadType;
	str threadName;
} threadlist_t;


typedef enum{
  MOVEMENT_TYPE_NORMAL,  
  MOVEMENT_TYPE_ANIM
} MovementType_t;

typedef enum{
	DIALOG_MODE_ANXIOUS,
	DIALOG_MODE_NORMAL,
	DIALOG_MODE_IGNORE
	} DialogMode_t;


typedef enum
	{
   IS_INANIMATE,
	IS_MONSTER,
	IS_ENEMY,
	IS_CIVILIAN,
	IS_FRIEND,
   IS_ANIMAL,	
	IS_TEAMMATE,
	NUM_ACTORTYPES
	} actortype_t;



typedef enum
   {
	ATTACK_ANY,
	ATTACK_PLAYER_ONLY,
	ATTACK_ACTORS_ONLY,
	ATTACK_SCRIPTED_ONLY,
	ATTACK_LEVEL_INTERACTION
	} targetType_t;

typedef enum
	{
	DEBUG_NONE,
	DEBUG_STATES_ONLY,
	DEBUG_STATES_BEHAVIORS,
	DEBUG_ALL,
	MAX_DEBUG_TYPES
	} stateDebugType_t;

typedef enum
	{
	TALK_TURNTO,
	TALK_HEADWATCH,
	TALK_IGNORE		
	} talkModeStates_t;

typedef enum{
	ACTOR_FLAG_NOISE_HEARD,
	ACTOR_FLAG_INVESTIGATING,
	ACTOR_FLAG_DEATHGIB,
	ACTOR_FLAG_DEATHFADE,
	ACTOR_FLAG_NOCHATTER,
	ACTOR_FLAG_INACTIVE,
	ACTOR_FLAG_ANIM_DONE,
	ACTOR_FLAG_STATE_DONE_TIME_VALID,
	ACTOR_FLAG_MASTER_STATE_DONE_TIME_VALID,
	ACTOR_FLAG_AI_ON,
	ACTOR_FLAG_LAST_CANSEEENEMY,
	ACTOR_FLAG_LAST_CANSEEENEMY_NOFOV,
	ACTOR_FLAG_DIALOG_PLAYING,
	ACTOR_FLAG_RADIUS_DIALOG_PLAYING,
	ACTOR_FLAG_ALLOW_TALK,
	ACTOR_FLAG_DAMAGE_ONCE_ON,
	ACTOR_FLAG_DAMAGE_ONCE_DAMAGED,
	ACTOR_FLAG_BOUNCE_OFF,
	ACTOR_FLAG_NOTIFY_OTHERS_AT_DEATH,
	ACTOR_FLAG_HAS_THING1,
	ACTOR_FLAG_HAS_THING2,
	ACTOR_FLAG_HAS_THING3,
	ACTOR_FLAG_HAS_THING4,
	ACTOR_FLAG_LAST_ATTACK_HIT,
	ACTOR_FLAG_STARTED,
	ACTOR_FLAG_ALLOW_HANGBACK,
	ACTOR_FLAG_USE_GRAVITY,
	ACTOR_FLAG_SPAWN_FAILED,
	ACTOR_FLAG_FADING_OUT,
	ACTOR_FLAG_DEATHSHRINK,
	ACTOR_FLAG_DEATHSINK,
	ACTOR_FLAG_STAYSOLID,
	ACTOR_FLAG_STUNNED,
	ACTOR_FLAG_ALLOW_FALL,
	ACTOR_FLAG_FINISHED,
	ACTOR_FLAG_IN_LIMBO,
	ACTOR_FLAG_CAN_WALK_ON_OTHERS,
	ACTOR_FLAG_PUSHABLE,
	ACTOR_FLAG_LAST_TRY_TALK,
	ACTOR_FLAG_TARGETABLE,
	ACTOR_FLAG_IMMORTAL,
	ACTOR_FLAG_TURNING_HEAD,
	ACTOR_FLAG_MOVING_EYES,
	ACTOR_FLAG_DIE_COMPLETELY,
	ACTOR_FLAG_BLEED_AFTER_DEATH,
	ACTOR_FLAG_IGNORE_STUCK_WARNING,
	ACTOR_FLAG_IGNORE_OFF_GROUND_WARNING,
	ACTOR_FLAG_ALLOWED_TO_KILL,
	ACTOR_FLAG_TOUCH_TRIGGERS,
	ACTOR_FLAG_IGNORE_WATER,
	ACTOR_FLAG_NEVER_IGNORE_SOUNDS,
	ACTOR_FLAG_SIMPLE_PATHFINDING,
	ACTOR_FLAG_HAVE_MOVED,
	ACTOR_FLAG_NO_PAIN_SOUNDS,
	ACTOR_FLAG_UPDATE_BOSS_HEALTH,
	ACTOR_FLAG_IGNORE_PAIN_FROM_ACTORS,
	ACTOR_FLAG_DAMAGE_ALLOWED,
	ACTOR_FLAG_AT_COVER_NODE,
	ACTOR_FLAG_WAIT_FOR_NEW_ENEMY,
	ACTOR_FLAG_TAKE_DAMAGE,
	ACTOR_FLAG_USE_DAMAGESKINS,
	ACTOR_FLAG_CAPTURED,
	ACTOR_FLAG_TURRET_MODE,
	ACTOR_FLAG_INCOMING_HITSCAN,
	ACTOR_FLAG_RESPONDING_TO_HITSCAN,
	ACTOR_FLAG_MELEE_HIT_WORLD,
	ACTOR_FLAG_TORSO_ANIM_DONE,
	ACTOR_FLAG_WEAPON_READY,	
	ACTOR_FLAG_DISABLED,
	ACTOR_FLAG_IN_ALCOVE,
	ACTOR_FLAG_IN_CONE_OF_FIRE,
	ACTOR_FLAG_IN_PLAYER_CONE_OF_FIRE,
	ACTOR_FLAG_PLAYER_IN_CALL_VOLUME,
	ACTOR_FLAG_IN_CALL_VOLUME,
	ACTOR_FLAG_OUT_OF_TORSO_RANGE,
	ACTOR_FLAG_DUCKED,
	ACTOR_FLAG_PRONE,
	ACTOR_FLAG_SHOULD_BLINK,
	ACTOR_FLAG_CRIPPLED,
	ACTOR_FLAG_RETREATING,
	ACTOR_FLAG_HIDDEN,
	ACTOR_FLAG_FOLLOWING_IN_FORMATION,
	ACTOR_FLAG_DISPLAYING_FAILURE_FX,
	ACTOR_FLAG_GROUPMEMBER_INJURED,
	ACTOR_FLAG_CAN_HEAL_OTHER,
	ACTOR_FLAG_STRICTLY_FOLLOW_PATHS,
	ACTOR_FLAG_POSTURE_ANIM_DONE,
	ACTOR_FLAG_ATTACKING_ENEMY,
	ACTOR_FLAG_UPDATE_HATE_WITH_ATTACKERS,
	ACTOR_FLAG_LAST_CANSEEPLAYER,
	ACTOR_FLAG_LAST_CANSEEPLAYER_NOFOV,	
	ACTOR_FLAG_MELEE_ALLOWED,
	ACTOR_FLAG_PLAYING_DIALOG_ANIM,
	ACTOR_FLAG_USING_HUD,
	ACTOR_FLAG_FORCE_LIFEBAR,
	ACTOR_FLAG_UPDATE_ACTION_LEVEL,
	ACTOR_FLAG_CAN_CHANGE_ANIM,
	ACTOR_FLAG_USE_FOLLOWRANGE_FOR_NODES,
	ACTOR_FLAG_IMMEDIATE_ACTIVATE,
	ACTOR_FLAG_CANNOT_DISINTEGRATE,
	ACTOR_FLAG_CANNOT_USE,
	ACTOR_FLAG_CANNOT_FREEZE,

	ACTOR_FLAG_MAX
	
	} ActorFlags;


typedef enum {
   NOTIFY_FLAG_DAMAGED,
   NOTIFY_FLAG_KILLED,
   NOTIFY_FLAG_SPOTTED_ENEMY,

   NOTIFY_FLAG_MAX
   } NotifyFlags;


typedef enum {
   MOVEMENT_STYLE_NONE,
   MOVEMENT_STYLE_WALK,
   MOVEMENT_STYLE_RUN,

   MOVEMENT_STYLE_MAX
   } MovementStyle;


extern Container<Actor *> SleepList;
extern Container<Actor *> ActiveList;
extern Container<Sentient *> TeamMateList;
extern Container<BehaviorPackageType_t *> PackageList;

#endif 
#endif 
