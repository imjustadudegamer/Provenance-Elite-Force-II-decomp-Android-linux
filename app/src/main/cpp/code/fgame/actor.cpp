//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor.cpp                        $
// $Revision:: 557                                                            $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 9:43a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
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


#include "g_local.h"
#include "actor.h"
#include "g_spawn.h"
#include "scriptvariable.h"
#include "actor_locomotion.h"
#include "behaviors_general.h"
#include "actor_combatsubsystem.h"
#include "scriptthread.h"
#include "../corepp/script.h" 
#include "doors.h"
#include "armor.h" 
#include "equipment.h" 
#include "../qcommon/gameplaymanager.h" 
#include "gibs.h"
#include "damagemodification.h" 

#include "misc.h"
#include "specialfx.h"
#include "object.h"
#include "scriptslave.h"
#include "explosion.h"
#include "misc.h"
#include "playerstart.h"
#include "characterstate.h"
#include "actor_headwatcher.h" 
#include "actorstrategies.h"
#include "talk.hpp"
#include "actorgamecomponents.h"
#include "actor_sensoryperception.h"
#include "actor_enemymanager.h"
#include "actor_posturecontroller.h"
#include "rageai.h"
#include "fuzzyengine.h" 
#include "weaputils.h"
#include "player.h"
#include "groupcoordinator.h"
#include "bg_local.h"
#include "sentient.h"
#include "g_phys.h"
#include "debuglines.h"
#include "scriptexception.h"
#include "../corepp/tiki.h"
#include "waypoints.h" 

#include <cmath>


extern Vector PLAYER_BASE_MIN;
extern Vector PLAYER_BASE_MAX;

bool bEntinfoInit = false;

cvar_t *g_showinfo;

[[maybe_unused]] static const float DEFAULT_NEARBY_SQUAD_DIST = 1024;
[[maybe_unused]] static const float MIN_BADPLACE_UPDATE_DIST  = 256;

Container<Actor *> SleepList;   
Container<Actor *> ActiveList;  

Container<BehaviorPackageType_t *> PackageList;

Event EV_Actor_SetSelfDetonateModel
   (
   "selfdetonatemodel",
   EV_TIKIONLY,
   "s",
   "modelname",
   "Set the modelname of the explosion to be spawned when an actor self-detonates"
   );

Event EV_Actor_BlindlyFollowPath
	(
	"blindlyfollowpath",
	EV_SCRIPTONLY,
	"sFS",
	"anim_name offset pathnode",
	"Actor walks to specified path node without avoidance or collision"
	);

Event EV_Actor_SetSimplifiedThink
   (
	"setsimplifiedthink",
	EV_DEFAULT,
	"B",
	"boolean",
	"change actor to SimplifiedThink think strategy"
	);

Event EV_Actor_SetActorToActorDamageModifier
   (
	"actortoactordamage",
	EV_DEFAULT,
	"f",
	"modifier",
	"Amount to modifiy damage by 1 is full damage, 0 would be no damage"
	);
Event EV_Actor_OnUse
   (
   "onuse",
	EV_SCRIPTONLY,
	"s",
	"thread_name",
   "Sets the thread to call"
	);

Event EV_Actor_NoUse
   (
	"nouse",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Clears the on use thread"	
	);

Event EV_Actor_ClearCurrentEnemy
   (
	"clearCurrentEnemy",
	EV_DEFAULT,
	NULL,
	NULL,
	"Sets Current Enemy to Null"
	);

Event EV_Actor_SetTargetType
   (
	"setTargetType",
	EV_DEFAULT,
	"i",
	"set_target_type",
	"Set Type of Target (0) Any, (1) Player Only, (2) Actors Only (3) Scripted Only (4) Level_Interaction Triggers Only	"
	);
Event EV_Actor_Sleep
   (
   "sleep",
   EV_DEFAULT,
   NULL,
   NULL,
   "Put the actor to sleep."
   );
Event EV_Actor_Wakeup
	(
	"wakeup",
	EV_SCRIPTONLY,
   NULL,
   NULL,
   "Wake up the actor."
	);
Event	EV_Actor_Fov
	(
	"fov",
	EV_CONSOLE,
	"f",
	"fov",
	"Sets the actor's field of view (fov)."
	);
Event EV_Actor_VisionDistance
	(
	"visiondistance",
	EV_DEFAULT,
	"f",
	"vision_distance",
	"Sets the distance the actor can see."
	);
Event EV_Actor_Start
	(
	"start",
	EV_DEFAULT,
   NULL,
   NULL,
   "Initializes the actor a little, "
	"it is not meant to be called from script."
	);
Event	EV_Actor_Dead
	(
	"dead",
	EV_CODEONLY,
   NULL,
   NULL,
   "Does everything necessary when an actor dies, "
	"it is not meant to be called from script."
	);

Event EV_Actor_SetEnemyType
	(
	"enemytype",
	EV_DEFAULT,
	"s",
	"enemytype",
	"Sets the name of this actor's enemy type."
	);
Event EV_Actor_Swim
	(
	"swim",
	EV_DEFAULT,
	NULL,
	NULL,
	"Specifies actor as being able to swim."
	);
Event EV_Actor_Fly
	(
	"fly",
	EV_DEFAULT,
	"B",
	"fly_bool",
	"Specifies actor as being able to fly (optional bool can turn fly on or off)."
	);
Event EV_Actor_NotLand
	(
	"noland",
	EV_DEFAULT,
	NULL,
	NULL,
	"Specifies actor as not being able to walk on land."
	);

Event EV_Actor_RunThread
	(
	"runthread",
	EV_CODEONLY,
	"s",
	"label",
	"Runs the specified thread."
	);

Event EV_Actor_Statemap
	(
	"statemap",
	EV_DEFAULT,
	"sS",
	"statemap_name state_name",   
	"Sets which statemap file to use and optionally what the first state to go to."   
	);

Event EV_Actor_MasterStateMap
	(
	"masterstatemap",   
	EV_DEFAULT,
	"sS",
	"statemap_name state_name",   
	"Sets which masterstatemap file to use and optionally what the first state to go to."   
	);

Event EV_Actor_FuzzyEngine
	(
	"fuzzyengine",
	EV_DEFAULT,
	"s",
	"fuzzyengine_name",
	"Sets which fuzzy engine file to use"
	);

Event EV_Actor_SetBehaviorPackage
	(
	"setbehaviorpackage",
	EV_DEFAULT,
	"s",
	"package_name",
	"sets the actor to use the specified behavior package AND sets the masterstate"
	);

Event EV_Actor_UseBehaviorPackage	
	(
	"usebehaviorpackage",
	EV_DEFAULT,
	"s",
	"package_name",
	"sets the actor to use the specified behavior package but does NOT set the master state"
	);

Event EV_Actor_ChildUseBehaviorPackage
	(
	"childusebehaviorpackage",
	EV_DEFAULT,
	"ss",
	"childname package_name",
	"sets the child to use the specified behavior package but does NOT set the master state"
	);

Event EV_Actor_ChildSetAnim
	(
	"childsetanim",
	EV_DEFAULT,
	"ss",
	"childname anim_name",
	"sets the child to play the anim specified"
	);

Event EV_Actor_ChildSuicide
	(
	"childsuicide",
	EV_DEFAULT,
	"s",
	"childname",
	"sets the child to kill itself"
	);

Event EV_Actor_IfEnemyVisible
	(
	"ifenemyvisible",
	EV_SCRIPTONLY,
	"SSSSSS",
	"token1 token2 token3 token4 token5 token6",
	"Process the following command if enemy is visible"
	);
Event EV_Actor_IfNear
	(
	"ifnear",
	EV_SCRIPTONLY,
	"sfSSSSSS",
	"name distance token1 token2 token3 token4 token5 token6",
	"Process the following command if enemy is within specified distance"
	);
Event EV_Actor_ForwardSpeed
	(
	"forwardspeed",
	EV_DEFAULT,
	"f",
	"forwardspeed",
	"Sets the actor's forward speed."
	);
Event EV_Actor_Idle
	(
	"idlestate",
	EV_SCRIPTONLY,
	"S",
	"state_name",
	"Tells the actor to go into idle mode."
	);
Event EV_Actor_LookAt
	(
	"lookat",
	EV_SCRIPTONLY,
	"e",
	"ent",
	"Specifies an entity to look at."
	);
Event EV_Actor_TurnTo
	(
	"turntoangle",
	EV_SCRIPTONLY,
	"f",
	"direction",
	"Specifies the direction to look in."
	);
Event EV_Actor_HeadWatch
	(
	"headwatch",
	EV_SCRIPTONLY,
	"eF",
	"entity_to_watch max_speed",
	"Actor watches the specified entity by turning his head."
	);

Event EV_Actor_HeadAndEyeWatch
	(
	"headandeyewatch",
	EV_SCRIPTONLY,
	"eF",
	"entity_to_watch max_speed",
	"Actor watches the specified entity by turning his eyes,then head."
	);
   
Event EV_Actor_ResetHead
	(
	"resethead",
	EV_DEFAULT,
	"F",
	"max_speed",
	"Actor resets its head back to looking forwards."
	);
Event EV_Actor_EyeWatch
   (
	"eyewatch",
	EV_SCRIPTONLY,
	"eF",
	"entity_to_watch max_speed",
	"Actor watches the specified entity by turning his eyes."
	);
Event EV_Actor_ResetEye
	(
	"reseteyes",
	EV_DEFAULT,
	"F",
	"max_speed",
	"Actor resets its eyes back to looking forwards."
	);

Event EV_Actor_ResetTorso
   (
   "resettorso",
	EV_DEFAULT,
	"f",
	"max_speed",
	"Actor resets its torso to looking forwards"
	);

Event EV_Actor_BehaviorFinished
	(
	"behaviorfinished",
	EV_CODEONLY,
	"iS",
	"behaviorReturnCode behaviorFailureReason",
	"The last behavior finished with the specified "
	"return code and optionally a failure reason."
	"This is sent to controllers of the actor."
	);

Event EV_Actor_ControlLost
	(
	"controlost",
	EV_CODEONLY,
	NULL,
	NULL,
	"Sent to a controller when it loses control."
	);

Event EV_Actor_EndBehavior
	(
	"endbehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Ends the current behavior, "
	"it is not meant to be called from script."
	);

Event EV_Actor_EndHeadBehavior
	(
	"endheadbehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Ends the current head behavior "
	"it is not meant to be called from script."
	);

Event EV_Actor_EndEyeBehavior
	(
	"endeyebehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Ends the current eye behavior "
	"it is not meant to be called from script."
	);

Event EV_Actor_EndTorsoBehavior
   (
	"endtorsobehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Ends the current torso behavior "
	"it is not meant to be called from script."
	);
   
Event EV_Actor_NotifyBehavior
	(
	"notifybehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Notifies the current behavior of an event,"
	"it is not meant to be called from script."
	);
Event EV_Actor_NotifyHeadBehavior
	(
	"notifyheadbehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Notifies the current head behavior of an event"
	"it is not meant to be called from script."
	);
Event EV_Actor_NotifyEyeBehavior
	(
	"notifyeyebehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Notifies the current eye behavior of an event"
	"it is not meant to be called from script."
	);   

Event EV_Actor_NotifyTorsoBehavior
   (
   "notifytorsobehavior",
	EV_CODEONLY,
	NULL,
	NULL,
	"Notifies the current torso behavior of an event"
	"it is not meant to be called from script."
	);
Event EV_Actor_FallToDeath
   (
	"falltodeath",
	EV_SCRIPTONLY,
	"fffsssF",
	"forwardmove sidemove speed startanim fallanim deathanim anim_delay",
	"makes an actor fall to his death"
	);

Event EV_Actor_WalkTo
	(
	"walkto",
	EV_DEFAULT,
	"sSFF",
	"pathnode anim_name force maxfailures",
	"Actor walks to specified path node"
	);

Event EV_Actor_WalkWatch
	(
	"walkwatch",
	EV_SCRIPTONLY,
	"seS",
	"pathnode entity anim_name",
	"Actor walks to specified path node and watches the specified entity"
	);

Event EV_Actor_WarpTo
	(
	"warpto",
	EV_SCRIPTONLY,
	"s",
	"node_name",
	"Warps the actor to the specified node"
	);
Event EV_Actor_JumpTo
	(
	"jumpto",
	EV_SCRIPTONLY,
	"sFF",
	"pathnode_or_entity launchAngle dummy_arg",
	"Actor jumps to specified path node"
	);
Event EV_Actor_PickupEnt
	(
	"pickupent",
	EV_DEFAULT,
	"es",
	"entity_to_pickup pickup_anim_name",
	"Makes actor pick up the specified entity"
	);
Event EV_Actor_ThrowEnt
	(
	"throwent",
	EV_DEFAULT,
	"s",
	"throw_anim_name",
	"Makes actor throw the entity in hands"
	);
Event EV_Actor_Anim
	(
	"anim",
	EV_DEFAULT,
	"s",
	"anim_name",
	"Starts the PlayAnim behavior."
	);
Event EV_Actor_SetAnim
	(
	"setanim",
	EV_DEFAULT,
	"sF",
	"anim_name animationRate",
	"Sets the animation directly."
	);
Event EV_Actor_Attack
	(
	"attack",
	EV_SCRIPTONLY,
	"eB",
	"ent force",
	"Makes the actor attack the specified entity."
	);
Event EV_Actor_AttackPlayer
	(
	"attackplayer",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Makes enemies of all the players."
	);
Event EV_Actor_ReserveNode
	(
	"reservenode",
	EV_CODEONLY,
	"vf",
	"pos time",
	"Reserves a path node for the specified amount of time."
	);
Event EV_Actor_ReleaseNode
	(
	"releasenode",
	EV_CODEONLY,
	"v",
	"pos",
	"Releases a path node from being reserved."
	);
Event EV_Actor_IfCanHideAt
	(
	"ifcanhideat",
	EV_SCRIPTONLY,
	"vSSSSSS",
	"pos token1 token2 token3 token4 token5 token6",
	"Processes command if actor can hide at specified position."
	);
Event EV_Actor_IfEnemyWithin
	(
	"ifenemywithin",
	EV_SCRIPTONLY,
	"fSSSSSS",
	"distance token1 token2 token3 token4 token5 token6",
	"Processes command if actor is within distance of its current enemy."
	);
Event EV_Actor_Remove
	(
	"remove_useless",
	EV_CODEONLY,
	NULL,
	NULL,
	"Removes a useless dead body from the game."
	);
Event EV_Actor_Melee
	(
	"melee",
	EV_DEFAULT,
	"FSSVFIF",
	"damage tag_name means_of_death attack_vector knockback use_pitch_to_enemy attack_min_height",
	"Makes the actor do a melee attack. "
	"attack_vector = width length height"
	);
Event EV_Actor_PainThreshold
	(
	"painthreshold",
	EV_TIKIONLY,
	"f",
	"pain_threshold",
	"Sets the actor's pain threshold."
	);
Event EV_Actor_SetKillThread
	(
	"killthread",
	EV_SCRIPTONLY,
	"s",
	"kill_thread",
	"Sets the actor's kill thread."
	);
Event EV_Actor_EyePositionOffset
	(
	"eyeoffset",
	EV_TIKIONLY,
	"v",
	"eyeoffset",
	"Sets the actor's eye position."
	);
Event EV_Actor_DeathFade
	(
	"deathfade",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor fade when dead."
	);
Event EV_Actor_DeathEffect
(
	"deathEffect",
	EV_DEFAULT,
	"s",
	"deathEffectName",
	"Displays a display effect instead of fading, shrinking, etc."
);
Event EV_Actor_DeathShrink
	(
	"deathshrink",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor shrink when dead."
	);
Event EV_Actor_DeathSink
	(
	"deathsink",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor sink into the ground when dead."
	);
Event EV_Actor_StaySolid
	(
	"staysolid",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor stay solid after death."
	);
Event EV_Actor_NoChatter
	(
	"nochatter",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor not chatter."
	);
Event EV_Actor_TurnSpeed
	(
	"turnspeed",
	EV_DEFAULT,
	"f",
	"turnspeed",
	"Sets the actor's turnspeed."
	);
Event EV_Actor_SetActorFlag
	(
	"setactorflag",
	EV_DEFAULT,
	"sB",
	"flag_name flag_bool",
	"Sets an Actor's flag"
	);

Event EV_Actor_SetNotifyFlag
   (
   "setnotifyflag",
   EV_DEFAULT,
   "sB",
   "flag_name flag_bool",
   "Sets an Actor's Notify Flag"
   );

Event EV_Actor_SetVar
	(
	"setvar",
	EV_DEFAULT,
	"ss",
	"var_name var_value",
	"Sets a variable"
	);

Event EV_Actor_PersistData
	(
	"persistData",
	EV_CODEONLY,
	"ss",
	"var_name var_value",
	"Sets a persistant variable"
	);

Event EV_Actor_SetVarTime
	(
	"setvartime",
	EV_CODEONLY,
	"s",
	"var_name",
	"Sets the variable name to the current level time"
	);

Event EV_Anim_Done
	(
	"anim_done",
	EV_CODEONLY,
	NULL,
	NULL,
	"Called when the actor's animation is done, "
	"it is not meant to be called from script."
	);
Event EV_Torso_Anim_Done
   (
	"torso_anim_done",
	EV_CODEONLY,
	NULL,
	NULL,
	"Called when actor's torso anim is done, "
	"If you call this from script, I will hunt you down, and end you"
	);
Event EV_Posture_Anim_Done
	(
	"posture_anim_done",
	EV_CODEONLY,
	NULL,
	NULL,
	"Called when a posture animation is done"
	);

Event EV_Actor_ProjAttack
	(
	"proj",
	EV_DEFAULT,
	"ssIBFFBF",
	"tag_name projectile_name number_of_tags arc_bool speed offset lead spread",
	"Fires a projectile from the actor towards the current enemy."
	);
Event EV_Actor_BulletAttack
	(
	"bullet",
	EV_DEFAULT,
	"sbffsvF",
	"tag_name use_current_pitch damage knockback means_of_death spread range",
	"Fires a bullet from the actor from the specified tag towards the current enemy."
	);
Event EV_Actor_RadiusAttack
   (
   "radiusattack",
   EV_DEFAULT,
   "ssfffb",
   "tag_name means_of_death damage radius knockback constant_damage",
   "Does a radius attack from the tag name"
   );

Event EV_Actor_Active
	(
	"active",
	EV_SCRIPTONLY,
	"i",
	"active_flag",
	"Specifies whether the actor's is active or not."
	);
Event EV_Actor_SpawnGib
	(
	"spawngib",
	EV_DEFAULT,
	"vffssSSSSSSSS",
	"offset final_pitch width cap_name surface_name1 surface_name2 surface_name3 surface_name4 surface_name5 surface_name6 surface_name7 surface_name8 surface_name9" ,
	"Spawns a body part."
	);
Event EV_Actor_SpawnGibAtTag
	(
	"spawngibattag",
	EV_DEFAULT,
	"sffssSSSSSSSS",
	"tag_name final_pitch width cap_name surface_name1 surface_name2 surface_name3 surface_name4 surface_name5 surface_name6 surface_name7 surface_name8 surface_name9" ,
	"Spawns a body part."
	);
Event EV_Actor_SpawnNamedGib
	(
	"spawnnamedgib",
	EV_DEFAULT,
	"ssff",
	"gib_name tag_name final_pitch width",
	"Spawns a body named gib."
	);
Event EV_Actor_SpawnBlood
	(
	"spawnblood",
	EV_DEFAULT,
	"ssB",
	"blood_name tag_name use_last_spawn_result" ,
	"Spawns blood at the specified tag."
	);
Event EV_Actor_AIOn
	(
	"ai_on",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Turns the AI on for this actor."
	);
Event EV_Actor_AIOff
	(
	"ai_off",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Turns the AI off for this actor."
	);
Event EV_Actor_RespondTo
   (
	"respondto",
	EV_DEFAULT,
	"sb",
	"stimuli respond",
	"sets AI response to stimuli"
	);

Event EV_Actor_PermanentlyRespondTo
   (
	"permanentrespondto",
	EV_TIKIONLY,
	"sb",
	"stimuli respond",
	"sets AI response to stimuli"
	);

Event EV_Actor_SetIdleThread
	(
	"setidlethread",
	EV_SCRIPTONLY,
	"s",
	"thread",
	"Sets the thread that will be run if this actor gets back to the idle state again."
	);
Event EV_Actor_SetMaxInactiveTime
	(
	"max_inactive_time",
	EV_DEFAULT,
	"f",
	"max_inactive_time",
	"Sets the maximum amount of time an actor will stay idle before going to sleep.\n"
	"Also sepecifies the maximum amount of time an actor will keep looking for an\n"
	"enemy that the actor can no longer see."
	);
Event EV_ActorRegisterParts
	(
	"register_parts",
	EV_CODEONLY,
	"ei",
	"entity forward",
	"Registers the passed in part as another part of this actor and specifies\n"
	"whether or not to forward this message to the other parts."
	);
Event EV_ActorRegisterSelf
	(
	"register_self",
	EV_CODEONLY,
	NULL,
	NULL,
	"Starts registration process for multi-entity actors"
	);
Event EV_ActorSetupTriggerField
	(
	"trigger_field",
	EV_TIKIONLY,
	"vv",
	"min max",
	"Specifies to create a trigger field around the actor of the specified size."
	);
Event EV_ActorTriggerTouched
	(
	"trigger_touched",
	EV_CODEONLY,
	"e",
	"ent",
	"Notifies the actor that its trigger field has been touched."
	);

Event EV_ActorIncomingProjectile
	(
	"incoming_proj",
	EV_CODEONLY,
	"e",
	"ent",
	"Notifies the actor of an incoming projectile."
	);
Event EV_ActorSpawnActor
	(
	"spawnactor",
	EV_DEFAULT,
	"ssibffFBF",
	"model_name tag_name how_many attack width height spawn_offset force add_height",
	"Spawns the specified number of actors."
	);
Event EV_ActorSpawnActorAboveEnemy
   (
	"spawnactoraboveenemy",
	EV_DEFAULT,
	"sibfff",
	"model_name how_many attack width height how_far",
	"Spawns actors above current enemy"
	);

Event EV_ActorSpawnActorAtLocation
	(
	"spawnactoratlocation",
	EV_DEFAULT,
	"ssibff",
	"model_name pathnode_name how_many_path_nodes attack width height",
	"Spawns the specified actor at the specified pathnode."
	);
Event EV_Actor_AddDialog
	(
	"dialog",
	EV_DEFAULT,
	"sSSSSSS",
	"alias token1 token2 token3 token4 token5 token6",
	"Add a dialog to this sentient."
	);

Event EV_Actor_DialogDone
	(
	"dialogdone",
	EV_CODEONLY,
	NULL,
	NULL,
	"Called when the sentient's dialog is done, "
	"it is not meant to be called from script."
	);

Event EV_Actor_PlayDialog
	(
	"playdialog",
	EV_DEFAULT,
	"SFFBBSE",
	"sound_file volume min_dist headDisplay do_talk state_name user",
	"Plays a dialog."
	);
Event EV_Actor_StopDialog
	(
	"stopdialog",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Stops the actor's dialog."
	);

Event EV_Actor_BroadcastDialog
	(
	"broadcastdialog",
	EV_CODEONLY,
	"s",
	"context",
	"Broadcasts a context dialog"
	);

Event EV_Actor_BranchDialog
	(
	"branchdialog",
	EV_DEFAULT,
	"s",
	"dialogName",
	"Presents a branch dialog to the player."
	);
Event EV_Actor_AllowTalk
	(
	"allowtalk",
	EV_DEFAULT,
	"i",
	"allow_bool",
	"Sets whether or not the actor will bother to talk to the player."
	);
Event EV_Actor_AllowHangBack
	(
	"allowhangback",
	EV_DEFAULT,
	"i",
	"allow_bool",
	"Sets whether or not the actor will bother to hang back."
	);
Event EV_Actor_SolidMask
	(
	"solidmask",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor use a solid mask."
	);
Event EV_Actor_NotSolidMask
	(
	"notsolidmask",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor use a nonsolid mask."
	);
Event EV_Actor_NoMask
	(
	"nomask",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor use a mask of 0."
	);
Event EV_Actor_SetMask
	(
	"setmask",
	EV_DEFAULT,
	"s",
	"mask_name",
	"Sets the actor's mask to the specified mask."
	);
Event EV_Actor_Pickup
	(
	"actor_pickup",
	EV_TIKIONLY,
	"s",
	"tag_name",
	"Makes the actor pickup current pickup_ent (should only be called from a tiki)."
	);
Event EV_Actor_Throw
	(
	"actor_throw",
	EV_TIKIONLY,
	"s",
	"tag_name",
	"Makes the actor throw whatever is in its hand (should only be called from a tiki)."
	);
Event EV_Actor_DamageOnceStart
	(
	"damage_once_start",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Makes the actor only do melee damage at most once during this attack."
	);
Event EV_Actor_DamageOnceStop
	(
	"damage_once_stop",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Specifies that the actor is done with the damage once event."
	);
Event EV_Actor_DamageEnemy
	(
	"damageenemy",
	EV_DEFAULT,
	"fS",
	"damage model",
	"Damages the current enemy by the specified amount."
	);
Event EV_Actor_DamageSelf
   (
	"damageself",
	EV_DEFAULT,
	"fs",
	"damage means_of_death",
	"Damages Self"
	);
Event EV_Actor_TurnTowardsEnemy
	(
	"turntowardsenemy",
	EV_SCRIPTONLY,
	"f",
	"angle",
	"Turns the actor towards the current enemy."
	);
Event EV_Actor_TurnTowardsPlayer
   (
	"turntowardsplayer",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Turns the actor towards the player."
	);

Event EV_Actor_TurnTowardsEntity
	(
	"turntowardsentity",
	EV_SCRIPTONLY,
	"e",
	"entity",
	"Turns the actor towards the entity"
	);

Event EV_Actor_Suicide
	(
	"suicide",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor commit suicide."
	);
Event EV_Actor_GotoNextStage
	(
	"gotonextstage",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor goto his next stage."
	);
Event EV_Actor_GotoPrevStage
	(
	"gotoprevstage",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor goto his previous stage."
	);
Event EV_Actor_GotoStage
	(
	"gotostage",
	EV_DEFAULT,
	"i",
	"stage_number",
	"Makes the actor goto the specified stage."
	);
Event EV_Actor_GetStage
	(
	"getstage",
	EV_SCRIPTONLY,
	"@f",
	"Result",
	"Returns this actors current stage."
	);
Event EV_Actor_NotifyOthersAtDeath
	(
	"notifyothersatdeath",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor notify other actors of the same type when killed."
	);
Event EV_Actor_SetBounceOff
	(
	"bounceoff",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes projectiles bounce off of actor (if they can't damage actor)."
	);
Event EV_Actor_SetHaveThing
	(
	"havething",
	EV_DEFAULT,
	"ib",
	"thing_number have_bool",
	"Sets whether or not the actor has this thing number."
	);

Event EV_Actor_SetUseGravity
	(
	"usegravity",
	EV_DEFAULT,
	"b",
	"use_gravity",
	"Tells the actor whether or not to use gravity for this animation."
	);
Event EV_Actor_SetDeathSize
	(
	"deathsize",
	EV_TIKIONLY,
	"vv",
	"min max",
	"Sets the actors new size for death."
	);
Event EV_Actor_Fade
	(
	"actorfade",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor fade out."
	);
Event EV_Actor_AttackMode
	(
	"attackmode",
	EV_SCRIPTONLY,
	"b",
	"attack_bool",
	"Makes the actor go directly into attacking the player if bool is true."
	);
Event EV_Actor_BounceOff
	(
	"bounceoffevent",
	EV_CODEONLY,
	"v",
	"object_origin",
	"Lets the actor know something just bounces off of it."
	);
Event EV_Actor_SetBounceOffEffect
	(
	"bounceoffeffect",
	EV_DEFAULT,
	"s",
	"bounce_off_effect_name",
	"Sets the name of the effect to play when something bounces off the actor."
	);
Event EV_Actor_AddSpawnItem
	(
	"spawnitem",
	EV_DEFAULT,
	"s",
	"spawn_item_name",
	"Adds this names item to what will be spawned when this actor is killed."
	);
Event EV_Actor_SetSpawnChance
	(
	"spawnchance",
	EV_DEFAULT,
	"f",
	"spawn_chance",
	"Sets the chance that this actor will spawn something when killed."
	);
Event EV_Actor_ClearSpawnItems
	(
	"clearspawnitems",
	EV_DEFAULT,
	NULL,
	NULL,
	"Clears the list of items to spawn when this actor is killed."
	);
Event EV_Actor_SetAllowFall
	(
	"allowfall",
	EV_DEFAULT,
	"B",
	"allow_fall_bool",
	"Makes the actor ignore falls when trying to move."
	);
Event EV_Actor_SetCanBeFinishedBy
	(
	"canbefinishedby",
	EV_TIKIONLY,
   "sSSSSS",
   "mod1 mod2 mod3 mod4 mod5 mod6",
   "Adds to the can be finished by list for this actor."
	);
Event EV_Actor_SetFeetWidth
	(
	"feetwidth",
	EV_TIKIONLY,
   "f",
   "feet_width",
   "Sets the width of the feet for this actor if different than the bounding box size."
	);
Event EV_Actor_SetCanWalkOnOthers
	(
	"canwalkonothers",
	EV_DEFAULT,
   NULL,
   NULL,
   "Allows the actor to walk on top of others."
	);
Event EV_Actor_Push
	(
	"push",
	EV_CODEONLY,
   "v",
   "dir",
   "Pushes the actor in the specified direction."
	);
Event EV_Actor_Pushable
	(
	"pushable",
	EV_DEFAULT,
   "B",
   "flag",
   "Sets whether or not an actor can be pushed out of the way."
	);
Event EV_Actor_ChargeWater
	(
	"chargewater",
	EV_DEFAULT,
   "ff",
   "damage range",
   "Does a charge water attack."
	);
Event EV_Actor_SendCommand
	(
	"sendcommand",
	EV_CODEONLY,
   "ss",
   "command part_name",
   "Sends a command to another one of its parts."
	);

Event EV_Actor_SetTargetable
	(
	"targetable",
	EV_DEFAULT,
   "b",
   "should_target",
   "Sets whether or not this actor should be targetable by the player."
	);
Event EV_Actor_ChangeType
	(
	"changetype",
	EV_DEFAULT,
   "s",
   "new_model_name",
   "Changes the actor to the specified new type of actor."
	);
Event EV_Actor_IgnoreMonsterClip
	(
	"ignoremonsterclip",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor ignore monster clip brushes."
	);
Event EV_Actor_MinimumMeleeHeight
	(
	"minmeleeheight",
	EV_DEFAULT,
	"f",
	"minimum_height",
	"Sets the minimum height a melee attack has to be to hurt the actor."
	);
Event EV_Actor_SetDamageAngles
	(
	"damageangles",
	EV_DEFAULT,
	"f",
	"damage_angles",
	"Sets the the angles where the actor can be hurt (like fov)."
	);
Event EV_Actor_Immortal
	(
	"immortal",
	EV_DEFAULT,
	"b",
	"immortal_bool",
	"Sets whether or not the actor is immortal or not."
	);
Event EV_Actor_SetDieCompletely
	(
	"diecompletely",
	EV_DEFAULT,
	"b",
	"die_bool",
	"Sets whether or not the actor dies completely (if he doesn't he mostly just"
	"  runs his kill_thread)."
	);
Event EV_Actor_SetBleedAfterDeath
	(
	"bleed_after_death",
	EV_DEFAULT,
	"b",
	"bleed_bool",
	"Sets whether or not the actor will bleed after dying."
	);
Event EV_Actor_IgnorePlacementWarning
	(
	"ignore_placement_warning",
	EV_DEFAULT,
	"s",
	"warning_string",
	"Makes the specified placement warning not get printed for this actor."
	);
Event EV_Actor_SetIdleStateName
	(
	"set_idle_state_name",
	EV_SCRIPTONLY,
	"s",
	"new_idle_state_name",
	"Sets the actor's new idle state name."
	);
Event EV_Actor_SetNotAllowedToKill
	(
	"not_allowed_to_kill",
	EV_DEFAULT,
	NULL,
	NULL,
	"Player fails the level if he kills an actor with this set."
	);

Event EV_Actor_IgnoreWater
	(
	"ignorewater",
	EV_DEFAULT,
	"b",
	"ignore_water_bool",
	"Sets whether or not this actor will ignore water when moving."
	);

Event EV_Actor_SimplePathfinding
	(
	"simplepathfinding",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor use simplier path finding."
	);
Event EV_Actor_NoPainSounds
	(
	"nopainsounds",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor not broadcast sounds (AI stimuli) when taking pain or killed."
	);
Event EV_Actor_BroadcastAlert
	(
	"alertevent",
	EV_DEFAULT,
	"F",
	"soundRadius",
	"Alerts Entities within the radius of the enemy's location."
	);

Event EV_Actor_UpdateBossHealth
	(
	"updatebosshealth",
	EV_DEFAULT,
	"BB",
	"updateFlag forceOn",
	"Tells the actor to update the bosshealth cvar each time it thinks."
	);
Event EV_Actor_SetMaxBossHealth
	(
	"maxbosshealth",
	EV_DEFAULT,
	"f",
	"max_boss_health",
	"Sets the actor's max boss health."
	);
Event EV_Actor_IgnorePainFromActors
	(
	"ignorepainfromactors",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes this actor ignore pain from other actors."
	);
Event EV_Actor_DamageAllowed
	(
	"damageallowed",
	EV_DEFAULT,
	"b",
	"damage_allowed",
	"Turns melee damage on and off."
	);
Event EV_Actor_SetEmotion
	(
	"emotion",
	EV_DEFAULT,
	"S",
	"expression_name",
	"Sets the actors current emotion."
	);
Event EV_Actor_ReturnProjectile
   (
	"returnproj",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Returns a projectile to the current enemy"
	);
Event EV_Actor_SetRadiusDialogRange
   (
	"setradiusdialogrange",
	EV_DEFAULT,
	"f",
	"range",
	"Sets the range for playing radius dialog"
	);

Event EV_Actor_SetDialogMode
   (
	"setdialogmode",
	EV_DEFAULT,
	"s",
	"mode_type",
	"Sets the Dialog Mode for the actor, valid values are 'anxious', 'normal', or 'ignore'"
	);

Event EV_Actor_DialogAnimDone
   (
	"dialoganimdone",
	EV_CODEONLY,
	NULL,
	NULL,
	"Not meant to be called from script -- So DONT FREAKIN DO IT!!!!"
	);

Event EV_Actor_SetEyeAngleConstraints
   (
	"seteyeangleconstraints",
	EV_TIKIONLY,
	"ffff",
	"min_eye_yaw_angle max_eye_yaw_angle min_eye_pitch_angle max_eye_pitch_angle",
	"Sets the constraints on eye movement"
	);

Event EV_Actor_SetActivateThread
   (
	"setactivatethread",
	EV_SCRIPTONLY,
	"s",
	"thread_name",
	"Sets the thread to call when the AI Activates"
	);

Event EV_Actor_SetValidTarget
   (
	"setvalidtarget",
	EV_DEFAULT,
	"b",
	"valid_target",
	"Sets whether or not actor is valid target in actor to actor confrontations"
	);

Event EV_Actor_SetAlertThread
   (
	"setalertthread",
	EV_SCRIPTONLY,
	"s",
	"thread_name",
	"sets a thread to be called when AI goes to alert"
	);

Event EV_Actor_RunAlertThread
   (
	"runalertthread",
	EV_CODEONLY,
	NULL,
	NULL,
	"runs an actors alert thread - NOT MEANT TO BE CALLED FROM SCRIPT"
	);

Event EV_Actor_CheckActorDead
   (
	"checkactordead",
	EV_SCRIPTONLY,
	"@ie",
	"dead_bool entity_to_check",
	"checks if an actor is dead"
	);

Event EV_Actor_EnemyActorFlag
   (
	"setenemyactorflag",
	EV_SCRIPTONLY,
	"sB",
	"flag_name flag_bool",
	"Sets an Actor's flag"
	);

Event EV_Actor_EnemyAIOn
   (
	"enemyaion",
	EV_CODEONLY,
	NULL,
	NULL,
	"turns on the current enemy AI"
	);

Event EV_Actor_EnemyAIOff
   (
	"enemyaioff",
	EV_CODEONLY,
	NULL,
	NULL,
	"turns off the current enemy AI"
	);

Event EV_Actor_AttachCurrentEnemy
   (
	"attachcurrentenemy",
	EV_CODEONLY,
	"s",
	"bone",
	"attach current enemy to the given bone"
	);

Event EV_Actor_AttachActor
   (
	"attachactor",
	EV_DEFAULT,
	"sss",
	"model targetname bone",
	"attach actor to the given bone"
	);

Event EV_Actor_SetEnemyAttached
   (
	"setenemyattached",
	EV_DEFAULT,
	"b",
	"attached",
	"sets whether or not the current enemy is attached -- Quetzal Specific"
	);

Event EV_Actor_PickUpThrowObject
   (
	"pickupthrowobject",
	EV_SCRIPTONLY,
	"s",
	"bone",
	"bone to attach object to"
	);

Event EV_Actor_TossThrowObject
   (
	"tossthrowobject",
	EV_DEFAULT,
	"ff",
	"speed gravity",
	"throws a throw object"
	);

Event EV_Actor_SetTurretMode
   (
	"setturretmode",
	EV_DEFAULT,
	"b",
	"on_off",
	"sets turret mode on or off"
	);

Event EV_Actor_SetHitscanResponseChance
   (
	"sethitscanresponse",
	EV_DEFAULT,
	"f",
	"chance",
	"sets chance an actor will respond to hitscan attacks"
	);

Event EV_Actor_SetWeaponReady
   (
	"setweaponready",
	EV_DEFAULT,
	"b",
	"ready",
	"sets if the actor has its weapon ready or not"
	);

Event EV_Actor_SetOnDamageThread
   (
	"actorondamage",
	EV_SCRIPTONLY,
	"sI",
	"thread_name damage_threshold",
	"sets the thread that is called when actor is damaged"
	);

Event EV_Actor_SetTimeBetweenSleepChecks
   (
	"timebetweensleepchecks",
	EV_SCRIPTONLY,
	"f",
	"delay",
	"sets the time between tests to see if the actor should sleep"
	);

Event EV_Actor_SetAimLeadFactors
	(
	"setaimleadfactors",
	EV_DEFAULT,
	"ff",
	"minLeadFactor maxLeadFactor",
	"sets the lead factor for projectile aiming; 0 = don't lead, 1 = perfect lead"
	);

Event EV_Actor_SetActorType
   (
	"actortype",
	EV_DEFAULT,
	"s",
	"actor_type",
	"sets the actortype"
	);

Event EV_Actor_RegisterBehaviorPackage
	(
	"registerpackage",
	EV_TIKIONLY,
	"s",
	"package_name",
	"registers a behavior package"
	);

Event EV_Actor_UnregisterBehaviorPackage
   (
	"unregisterpackage",
	EV_TIKIONLY,
	"s",
	"package_name",
	"unregisters a behavior package"
	);

Event EV_Actor_SetBehaviorPackageTendency
   (
   "setpackagetendency",
   EV_DEFAULT,
   "sf",
   "package_name tendency",
   "sets the tendency to execute the behavior package"
   );

Event EV_Actor_SetAbsoluteMaxRange
   (
	"setabsolutemaxrange",
	EV_DEFAULT,
	"f",
	"absolute_max_range",
	"sets the absolute maximum range the actor will get from an entity"
	);

Event EV_Actor_SetAbsoluteMinRange
	(
	"setabsoluteminrange",
	EV_DEFAULT,
	"f",
	"absolute_min_range",
	"sets the absolute minimum range the actor will get from an entity" 
	);

Event EV_Actor_SetPreferredMaxRange
	(
	"setpreferredmaxrange",
	EV_DEFAULT,
	"f",
	"preferred_max_range",
	"sets the preferred maximum range the actor would like to be from an entity"
	);

Event EV_Actor_SetPreferredMinRange
   (
	"setpreferredminrange",
	EV_DEFAULT,
	"f",
	"preferred_min_range",
	"sets the preferred minimum range the actor would like to be from an entity"
	);

Event EV_Actor_DebugStates
   (
	"debugstates",
	EV_CODEONLY,
	"i",
	"debug_state",
	"sets debug level for actor statemachine"
	);

Event EV_Actor_SetHeadWatchTarget
   (
   "headwatchtarget",
   EV_SCRIPTONLY,
   "sf",
   "target speed",
   "sets the headwatch target... currently to enemy or none"
   );
Event EV_Actor_SetHeadTwitch
   (
   "headTwitch",
   EV_SCRIPTONLY,
   "b",
   "bool",
   "Sets whether or not the head should twitch."
   );

Event EV_Actor_SetFuzzyEngineActive
   (
   "fuzzyengineactive",
   EV_DEFAULT,
   "b",
   "active",
   "sets the fuzzy engine active or not"
   );

Event EV_Actor_FollowWayPoints
	(
	"followwaypoints",
	EV_SCRIPTONLY,
	"sS",
	"waypointnode_name starting_anim_name",
	"Makes an actor follow a waypoint path starting at , starting at the start_point"
	);


Event EV_Actor_Disable
	(
	"disable",
	EV_DEFAULT,
	"i",
	"disable_flag",
	"disable actor ( 1 ) or not disable actor ( 0 )"
	);

Event EV_Actor_Cripple
   (
   "cripple",
   EV_DEFAULT,
   "i",
   "cripple_flag",
   "cripple actor ( 1 ) or not cripple actor ( 0 )"
   );

Event EV_Actor_In_Alcove
	(
	"in_alcove",
	EV_DEFAULT,
	"i",
	"in_alcove_flat",
	"in alcove ( 1 ) or not in alcove ( 0 )"
	);

Event EV_Actor_GiveActorWeapon
   (
	"giveactorweapon",
	EV_DEFAULT,
	"sI",
	"weapon amount",
	"Gives a weapon to an actor"
	);

Event EV_Actor_RemoveActorWeapon
   (
	"removeactorweapon",
	EV_DEFAULT,
	"s",
	"weapon",
	"removes an actors weapon"
	);

Event EV_Actor_UseWeapon
   (
	"useactorweapon",
	EV_TIKIONLY,
	"sS",
	"weapon hand",
	"Makes the specified weapon active for the actor \n"
	"If they have the weapon"
	);

Event EV_Actor_SetMovementMode
   (
   "movementmode",
   EV_DEFAULT,
   "s",
   "movment_mode",
   "sets the movment mode of the actor"
   );

Event EV_Actor_ResetMoveDir
   (
   "resetmovedir",
   EV_CODEONLY,
   NULL,
   NULL,
   "Resets and resyncs movedir with animdir"
   );

Event EV_Actor_SetNodeID
   (
   "setnodeid",
   EV_SCRIPTONLY,
   "i",
   "id_number",
   "Sets the ID number of the helper nodes that this actor can use"
   );


Event EV_Actor_SetAggressiveness
   (
   "aggressive",
   EV_DEFAULT,
   "f",
   "aggressiveness",
   "sets the aggressiveness of the actor... valid range between 0 and 1"
   );

Event EV_Actor_SetTalkiness
   (
   "talkiness",
   EV_DEFAULT,
   "f",
   "talkiness",
   "sets the talkiness of the actor... valid range between 0 and 1"
   );

Event EV_Actor_SetTendency
   (
   "settendency",
   EV_DEFAULT,
   "sf",
   "name value",
   "Sets a tendency for the actor"
   );

Event EV_Actor_SetFloatProperty
	(
	"setfloatproperty",
	EV_DEFAULT,
	"sf",
	"name value",
	"Sets a float property on the actor"
	);

Event EV_Actor_SetGroupNumber
   (
   "groupnumber",
   EV_DEFAULT,
   "i",
   "group_number",
   "sets the group number of the actor"
   );

Event EV_Actor_Blink
   (
   "blink",
   EV_DEFAULT,
   "b",
   "shouldBlink",
   "Sets whether or not the actor should blink"
   );

Event EV_Actor_ClearArmorAdapations
   (
   "cleararmoradaptions",
   EV_SCRIPTONLY,
   NULL,
   NULL,
   "clears armor adaptions"
   );

Event EV_Actor_SetFollowTarget
   (
   "followtarget",
   EV_SCRIPTONLY,
   "e",
   "entity_to_follow",
   "Sets the following target for the actor"
   );

Event EV_Actor_SetFollowRange
   (
   "followrange",
   EV_SCRIPTONLY,
   "f",
   "maxRange",
   "Sets a range that the target considers _close enough_ while following"
   );

Event EV_Actor_SetFollowRangeMin
	(
	"followrangemin",
	EV_SCRIPTONLY,
	"f",
	"minRange",
	"Sets a minimum range for following"
	);

Event EV_Actor_SetCombatFollowRange
   (
   "followcombatrange",
   EV_SCRIPTONLY,
   "f",
   "maxRange",
   "Sets a range that the target considers _close enough_ while following in a combat situation"
   );

Event EV_Actor_SetCombatFollowRangeMin
	(
	"followcombatrangemin",
	EV_SCRIPTONLY,
	"f",
	"minRange",
	"Sets a minimum range for following in a combat situation"
	);

Event EV_Actor_SetSteeringDirectionPreference
   (
   "steeringdirectionpreference",
   EV_SCRIPTONLY,
   "s",
   "preference",
   "Sends a string to define the way actors will turn when avoiding obstacles"
   );

Event EV_Actor_SetStickToGround
   (
   "setsticktoground",
   EV_DEFAULT,
   "b",
   "stick",
   "Sets a bool that determines whether the actor ground follows"
   );

Event EV_Actor_SetDialogMorphMult
   (
   "dialogMorphMult",
   EV_DEFAULT,
   "f",
   "dialogMorphMult",
   "Sets the multiplier for all dialog morphs for this actor."
   );

Event EV_ContextDialog_InContext
   (
   "incontext",
   EV_CODEONLY,
   "s",
   "context",
   "Used to start a context dialog"
   );

Event EV_ContextDialog_IgnoreNextContext
	(
	"ignorenextcontext",
	EV_CODEONLY,
	"bs",
	"flag context",
	"Makes the actor ignore the next context event it receives"
	);

Event EV_Actor_ForceSetClip
	(
	"forcesetclip",
	EV_CODEONLY,
	NULL,
	NULL,
	"Makes the actor set his contents to setclip"
	);

Event EV_Actor_WhatAreYouDoing
	(
	"whatareyoudoing",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor print a bunch of debug state info to the console" 
	);

Event EV_Actor_WhatsWrong
	(
	"whatswrong",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor print the current behaviors failure reaon to the console"
	);

Event EV_Actor_PutawayWeapon
	(
	"putawayweapon",
	EV_DEFAULT,
   "S",
   "hand",
   "Deactivate the weapon in the specified hand."
	);

Event EV_Actor_SetCombatTraceInterval
	(
	"combattraceinterval",
	EV_DEFAULT,
	"f",
	"interval",
	"Determines how often an actor will re-trace when doing can-attack types of checks"
	);

Event EV_Actor_UseWeaponDamage
   (
	"useweapondamage",
	EV_TIKIONLY,
	"SB",
	"hand setflag",
	"Makes the melee event reference the damage of the weapon in the specified hand."
	);

Event EV_Actor_StrictlyFollowPath
   (
	"strictlyfollowpath",
	EV_DEFAULT,
	"b",
	"boolean",
	"Lets the actor know if he should follow paths exactly or if he can go directly to his goal."
	);
Event EV_Actor_EvaluateEnemies
	(
	"evaluateenemies",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor evaluate his enemy list"
	);

Event EV_Actor_ForgetEnemies
	(
	"forgetenemies",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the actor forget about all enemies for one frame"
	);

Event EV_Actor_SetMaxHeadYaw
	(
	"maxheadyaw",
	EV_DEFAULT,
	"f",
	"maxyaw",
	"Sets the max yaw the headwatcher can turn the head"
	);

Event EV_Actor_SetMaxHeadPitch
	(
	"maxheadpitch",
	EV_DEFAULT,
	"f",
	"maxpitch",
	"Sets the max pitch the headwatcher can turn the head"
	);

Event EV_Actor_SetPostureStateMap
	(
	"posturestatemap",
	EV_DEFAULT,
	"sB",
	"statemap loadingFlag",   
	"Sets the state machine for the posture controller" 
	);

Event EV_Actor_SendEventToGroup
	(
	"sendeventtogroup",
	EV_DEFAULT,
	"sSSSSS",
	"event parm parm parm parm parm",
	"sends the specified event to the entire group"
	);

Event EV_Actor_GroupAttack
	(
	"groupattack",
	EV_DEFAULT,
	NULL,
	NULL,
	"Sends and attack event to the whole group with this actors current enemy"
	);

Event EV_Actor_GroupActorType
	(
	"groupactortype",
	EV_DEFAULT,
	"s",
	"actortype",
	"Sends an actortype event to the whole group"
	);

Event EV_Actor_SetMasterState
	(
	"setmasterstate",
	EV_DEFAULT,
	"s",
	"state_name",
	"Sets the master state"
	);

Event EV_Actor_PrintDebugMessage
	(
	"printmessage",
	EV_DEFAULT,
	"s",
	"message",
	"Prints a warning message to the console"
	);

Event EV_Actor_SelectNextEnemy
	(
	"SelectNextEnemy",
	EV_DEFAULT,
	NULL,
	NULL,
	"Sets the actor's current enemy to be the next enemy in it's hate list, assuming there is one."
	);

Event EV_Actor_SelectClosestEnemy
	(
	"selectclosestenemy",
	EV_DEFAULT,
	NULL,
	NULL,
	"Sets the actor's current enemy to be the closest enemy in it's hate list "
	);

Event EV_Actor_SetGroupDeathThread
	(
	"groupdeaththread",
	EV_DEFAULT,
	"s",
	"thread_name",
	"Sets a thread to call when all the members of a group have been killed"
	);

Event EV_Actor_SetAnimSet
	(
	"useanimset",
	EV_DEFAULT,
	"s",
	"anim_set",
	"Sets the AnimSet... Valid Set Names are AnimSet1 , AnimSet2 "
	);

Event EV_Actor_SetPostureState
	(
	"setposturestate",
	EV_DEFAULT,
	"ss",
	"currentState requestedState",
	"Sets the Posture State"
	);

Event EV_Actor_SetTalkWatchMode
	(
	"settalkwatchmode",
	EV_DEFAULT,
	"sB",
	"mode useConvAnim",
	"Sets the talk watch mode -- valid entries are turnto, headwatchonly, and ignore"
	);

Event EV_Actor_PrepareMissionFailure
	(
	"failmission",
	EV_DEFAULT,
	"fS",
	"time reason",
	"Fails the mission in the time specified"
	);

Event EV_Actor_FailMission
	(
	"domissionfailure",
	EV_DEFAULT,
	"S",
	"reason",
	"Fails the mission"
	);

Event EV_Actor_DebugEvent
	(
	"debugevent",
	EV_DEFAULT,
	NULL,
	NULL,
	"Called for Debug Purposes from state machine"
	);

Event EV_Actor_UnreserveCurrentHelperNode
	(
	"unreservecurrenthelpernode",
	EV_CODEONLY,
	NULL,
	NULL,
	"Unreserves the current helper node"
	);

Event EV_Actor_ProjectileClose
	(
	"projectileclose",
	EV_CODEONLY,
	"e",
	"owener",
	"Informs Actor that a projectile is close"
	);

Event EV_Actor_SaveOffLastHitBone
	(
	"saveofflasthitbone",
	EV_CODEONLY,
	NULL,
	NULL,
	"Saves off the last hit_bone"
	);

Event EV_Actor_ClearTorsoAnim
	(
	"cleartorsoanim",
	EV_CODEONLY,
	NULL,
	NULL,
	"clears the f'ng torso anim"
	);

Event EV_Actor_SetPlayPainSoundInterval
	(
	"setplaypainsoundinterval",
	EV_DEFAULT,
	"f",
	"interval",
	"Sets the pain sound interval"
	);

Event EV_Actor_SetContextSoundInterval
	(
	"setcontextsoundinterval",
	EV_DEFAULT,
	"f",
	"interval",
	"Sets the context sound ( dialog ) interval "
	);

Event EV_Actor_SetMinPainTime
	(
	"setminpaintime",
	EV_DEFAULT,
	"f",
	"time",
	"Sets the minimum pain time"
	);

Event EV_Actor_SetEnemyTargeted
	(
	"setenemytargeted",
	EV_DEFAULT,
	"b",
	"targeted",
	"Sets whether or not the enemy should display targeted shader"
	);

Event EV_Actor_SetActivationDelay
	(
	"setactivationdelay",
	EV_DEFAULT,
	"f",
	"delay",
	"If set up to use it, actors will delay action for the specifed delay time"
	);

Event EV_Actor_SetActivationStart
	(
	"startactivationtimer",
	EV_DEFAULT,
	NULL,
	NULL,
	"Sets the activationStart time to the current level time"
	);

Event EV_Actor_SetCheckConeOfFireDistance
	(
	"SetCheckConeOfFireDistance",
	EV_DEFAULT,
	"f",
	"distance",
	"Sets how close an actor must be for IN_CONE_OF_FIRE checks from a state machine."
	);

Event EV_Actor_AddCustomThread
	(
	"addcustomthread",
	EV_DEFAULT,
	"ss",
	"threadType threadName",
	"Adds a custom thread to the actor.  The thread type is the specific type to call, the name is the thread to call"
	);

Event EV_Actor_SetHeadWatchMaxDistance
	(
	"setheadwatchmaxdistance",
	EV_DEFAULT,
	"f",
	"maxdistance",
	"Sets the max distance for the headwatcher"
	);

Event EV_Actor_AnimateOnce
	(
	"animateonce",
	EV_DEFAULT,
	"s",
	"anim_name",
	"Runs the specified animation one time, then holds the last frame"
	);

Event EV_Actor_SetDeathKnockbackValues
	(
	"setdeathknockbackvalues",
	EV_DEFAULT,
	"ff",
	"vertical_value horiz_value",
	"Sets Death Knockback Values"
	);


qboolean Actor::checkAnyEnemyInRange( float range )
{
	return enemyManager->IsAnyEnemyInRange( range );
}
qboolean Actor::checkCurrentNodeHasThisCoverType( const str &coverType )
{
	if ( !currentHelperNode.node )
		return false;

	if ( coverType == "none" )
		{
		if ( currentHelperNode.node->GetCoverType() == COVER_TYPE_NONE )
			return true;
		}

	if ( coverType == "crate" )
		{
		if ( currentHelperNode.node->GetCoverType() == COVER_TYPE_CRATE )
			return true;		
		}

	if ( coverType == "wall" )
		{
		if ( currentHelperNode.node->GetCoverType() == COVER_TYPE_WALL )
			return true;
		}
		
	return false;
}
qboolean Actor::checkEnemiesNearby(float distance)
{
	Entity *ent;
	for ( int i = 1 ; i <= ActiveList.NumObjects() ; i++ )
		{
		ent = ActiveList.ObjectAt( i );
		if ( (enemyManager->Hates( ent )) && WithinDistance(ent, distance ) )
			return true;
		}

	return false;
}

qboolean Actor::checkUsingWeaponNamed(const str &weaponName)
{
    return combatSubsystem->UsingWeaponNamed(weaponName);
}

qboolean Actor::checkForwardDirectionClear(float dist)
   {
   trace_t trace;   

   Vector endPos;
   Vector startPos;
   Vector forward;
   Vector angles_shadow5;

   startPos = origin;
   startPos.z += 32;
   
   angles_shadow5 = movementSubsystem->getAnimDir(); 
   angles_shadow5 = angles_shadow5.toAngles();
   angles_shadow5.AngleVectors( &forward );

   endPos = ( forward * dist) + startPos;

   trace = G_Trace(startPos, mins, maxs, endPos, NULL, edict->clipmask, false, "checkForwardDirectionClear" );

   if (trace.fraction == 1.0 )
		{
	   return ( movementSubsystem->CanWalkTo( trace.endpos, 0.0f, entnum ) );		
		}
   
   return false;     
   }
qboolean Actor::checkGroupAttackerCountForEntity( int checkValue, Entity* attackTarget )
{
	int count;
	ActorGroup* group;	
	
	group = (ActorGroup*)groupcoordinator->GetGroup( GetGroupID() );
	
	if ( !group )
		return true;

	count = group->CountMembersAttackingEnemy( attackTarget );

	if ( count < checkValue )
		return true;

	return false;
}
qboolean Actor::checkLeftDirectionClear(float dist)
   {
   trace_t trace;   

   Vector endPos;
   Vector startPos;
   Vector left;
   Vector angles_shadow4;

   startPos = origin;
   
   angles_shadow4 = movementSubsystem->getAnimDir(); 
   angles_shadow4 = angles_shadow4.toAngles();
   angles_shadow4.AngleVectors( NULL, &left, NULL );

   endPos = ( left * dist) + startPos;
   
   trace = Trace( endPos , "CheckMyLeft" );

   if (trace.fraction == 1.0 )
		 {
	   return ( movementSubsystem->CanWalkTo( trace.endpos, 0.0f, entnum ) );		
		 }
      
   
   return false;     
   
   }
qboolean Actor::checkRearDirectionClear(float dist)
   {
   trace_t trace;   

   Vector endPos;
   Vector startPos;
   Vector forward;
   Vector angles_shadow3;

   startPos = origin;
   startPos.z += 32;
   
   angles_shadow3 = movementSubsystem->getAnimDir(); 
   angles_shadow3 = angles_shadow3.toAngles();
   angles_shadow3[YAW] = AngleNormalize180(angles_shadow3[YAW] + 180);
   angles_shadow3.AngleVectors( &forward );
   
   endPos = ( forward * dist) + startPos;

   trace = G_Trace(startPos, mins, maxs, endPos, NULL, edict->clipmask, false, "checkForwardDirectionClear" );

   if (trace.fraction == 1.0 )
		{
	   return ( movementSubsystem->CanWalkTo( trace.endpos, 0.0f, entnum ) );		
		}
   
   return false;     
   
   }


void Actor::SetMinPainTime( float time )
{
	min_pain_time = time;
}
void Actor::SetActivationDelay( float delay )
{
	activationDelay = delay;
}
void Actor::SetCheckConeOfFireDistance( float distance )
{
	assert( strategos );
	assert( distance > 0 );
	this->strategos->SetCheckInConeDistMax( distance );
}
qboolean Actor::checkValidHibernateNodeInRange( float maxDistanceFromSelf )
{
	HelperNode* hibernateNode = NULL;

	if ( level.time < _nextCheckForHibernateNodeTime )
		return false;

	hibernateNode = HelperNode::FindClosestHelperNode(*this , "hibernate" , maxDistanceFromSelf );
	_nextCheckForHibernateNodeTime = ( G_Random() + level.time + 0.50f );

	if ( hibernateNode )
		return true;

	return false;
}


void Actor::SetVar( Event *ev	)
	{
	StateVar *checkVar = 0;
	str varName			 = ev->GetString(1);
	str varValue		 = ev->GetString(2);
	
	for (int i = 1; i <= stateVarList.NumObjects(); i++ )
		{
		checkVar = stateVarList.ObjectAt(i);
		if( !stricmp( checkVar->varName, varName ) )
			{
			checkVar->varValue = varValue;
			return;
			}
		}
	
	checkVar = new StateVar;
	checkVar->varName = varName;
	checkVar->varValue = varValue;
	
	stateVarList.AddObject( checkVar );

	}
void Actor::SetVarTime( Event *ev )
{
	StateVar *checkVar = 0;
	str varName			 = ev->GetString(1);	
	
	for (int i = 1; i <= stateVarList.NumObjects(); i++ )
		{
		checkVar = stateVarList.ObjectAt(i);
		if( !stricmp( checkVar->varName, varName ) )
			{
			checkVar->varTime = level.time;
			return;
			}
		}
	
	checkVar = new StateVar;
	checkVar->varName = varName;
	checkVar->varTime = level.time;
	
	stateVarList.AddObject( checkVar );	
}

void Actor::BranchDialog(Event *ev)
{
    Player *player = GetPlayer(0);
    player->setBranchDialogActor(this);

    _branchDialogName = ev->GetString(1);
    setBranchDialog();
}

qboolean Actor::CanBeFinished(void)
{
    if (can_be_finsihed_by_mods.NumObjects() > 0) {
        return true;
    } else {
        return false;
    }
}

qboolean Actor::CanBeFinishedBy(int meansofdeath)
{
    int number_of_mods;
    int i;

    if (!InLimbo()) {
        return false;
    }

    number_of_mods = can_be_finsihed_by_mods.NumObjects();

    for (i = 1; i <= number_of_mods; i++) {
        if (meansofdeath == can_be_finsihed_by_mods.ObjectAt(i)) {
            return true;
        }
    }

    return false;
}

qboolean Actor::DialogExists(const str& aliasName)
{
    DialogNode_t *dialog_node;

    dialog_node = dialog_list;

    while (dialog_node != NULL) {
        if (stricmp(dialog_node->alias_name, aliasName.c_str()) == 0) {
            return true;
        }

        dialog_node = dialog_node->next;
    }

    return false;
}

void Actor::Finish(int meansofdeath)
{
    if (CanBeFinishedBy(meansofdeath)) {
        SetActorFlag(ACTOR_FLAG_FINISHED, true);

        ProcessEvent(EV_Actor_Suicide);

        means_of_death = meansofdeath;
    } else {
        gi.WDPrintf("Actor can't be finished by %d means of death\n", meansofdeath);
    }
}

void Actor::FreeDialogList(void)
{
    DialogNode_t *dialog_node;

    dialog_node = dialog_list;

    while (dialog_node != NULL) {
        dialog_list = dialog_node->next;

        delete dialog_node;

        dialog_node = dialog_list;
    }
}

void Actor::StartLimbo(void)
{
    State   *temp_state;
    qboolean found_state;

    health = 1;

    found_state = false;

    if (statemap) {
        temp_state = statemap->FindState("LIMBO");

        if (temp_state) {
            currentState = temp_state;
            InitState();
            found_state  = true;
            SetActorFlag(ACTOR_FLAG_IN_LIMBO, true);
        }
    }

    if (!found_state) {
        ProcessEvent(EV_Actor_Suicide);
    }
}

qboolean Actor::InLimbo(void)
{
    return GetActorFlag(ACTOR_FLAG_IN_LIMBO);
}

void Actor::NotifyOthersAtDeath(Event *ev)
{
    SetActorFlag(ACTOR_FLAG_NOTIFY_OTHERS_AT_DEATH, true);
}

DialogNode_t *Actor::NewDialogNode(void)
{
    DialogNode_t *dialog_node;

    dialog_node = new DialogNode_t;
    memset(dialog_node, 0, sizeof(DialogNode_t));
    dialog_node->random_percent = 1.0;
    dialog_node->dType          = DIALOG_TYPE_NORMAL;

    return dialog_node;
}

void Actor::SaveAttack(const Vector& orig, const Vector& dir)
{
    Vector   attack_mins;
    Vector   attack_maxs;
    Vector   end;
    trace_t  trace;
    qboolean hit;
    Entity  *ent;

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();
    if (!currentEnemy) {
        SetActorFlag(ACTOR_FLAG_LAST_ATTACK_HIT, true);
        return;
    }

    attack_mins = Vector(-1.0f, -1.0f, -1.0f);
    attack_maxs = Vector(1.0f, 1.0f, 1.0f);

    end = orig + (dir * 8192.0f);

    trace = G_Trace(orig, attack_mins, attack_maxs, end, this, MASK_SHOT, false, "Actor::SaveAttack");

    last_attack_entity_hit = NULL;

    if (trace.ent) {
        ent = trace.ent->entity;

        if (ent == currentEnemy) {
            hit = true;
        } else if (ent->isSubclassOf(Entity) && (ent != world)) {
            last_attack_entity_hit     = ent;
            last_attack_entity_hit_pos = ent->origin;
            hit                        = false;
        } else {
            hit = false;
        }
    } else {
        hit = false;
    }

    if (currentEnemy->movetype == MOVETYPE_NOCLIP) {
        hit = true;
    }

    last_attack_pos      = origin;
    last_attack_enemy_pos = currentEnemy->origin;

    SetActorFlag(ACTOR_FLAG_LAST_ATTACK_HIT, hit);
}

qboolean Actor::TestAttack(const str& tag_name)
{
    qboolean hit;
    trace_t  trace;
    Vector   attack_mins;
    Vector   attack_maxs;
    Vector   start;

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();
    if (!currentEnemy) {
        return false;
    }

    if (currentEnemy->movetype == MOVETYPE_NOCLIP) {
        return false;
    }

    attack_mins = Vector(-1.0f, -1.0f, -1.0f);
    attack_maxs = Vector(1.0f, 1.0f, 1.0f);

    if (tag_name.length()) {
        GetTag(tag_name.c_str(), &start);
    } else {
        start = centroid;
    }

    trace = G_Trace(start, attack_mins, attack_maxs, currentEnemy->centroid, this, MASK_SHOT, false, "Actor::TestAttack");

    if (trace.ent && (trace.ent->entity != currentEnemy) && trace.ent->entity->isSubclassOf(Sentient)
        && !enemyManager->IsValidEnemy(trace.ent->entity)) {
        return false;
    }

    hit = GetActorFlag(ACTOR_FLAG_LAST_ATTACK_HIT);

    if (hit) {
        return true;
    }


    if (last_attack_pos != origin) {
        return true;
    }

    if (last_attack_enemy_pos != currentEnemy->origin) {
        return true;
    }

    if (last_attack_entity_hit && (last_attack_entity_hit_pos != last_attack_entity_hit->origin)) {
        return true;
    }

    if (last_attack_entity_hit && last_attack_entity_hit->isSubclassOf(Door)) {
        Door *door;

        door = (Door *)(Entity *)last_attack_entity_hit;

        if (door->isOpen()) {
            return true;
        }
    }

    if (last_attack_entity_hit && (last_attack_entity_hit->edict->solid == SOLID_NOT)) {
        return true;
    }

    return false;
}

qboolean Actor::TakeDamage(void)
{
    return GetActorFlag(ACTOR_FLAG_TAKE_DAMAGE);
}

void Actor::InitMovementSubsystem()
{
    movementSubsystem = NULL;
    movementSubsystem = new MovementSubsystem(this);

    if (!movementSubsystem) {
        gi.Error(ERR_FATAL, "Actor Could not create movementSubsystem");
    }
}

void Actor::InitCombatSubsystem()
{
    combatSubsystem = NULL;
    combatSubsystem = new CombatSubsystem(this);

    if (!combatSubsystem) {
        gi.Error(ERR_FATAL, "Actor Could not create personality");
    }
}


void Actor::InitHeadWatcher()
   {
   headWatcher = NULL;
   headWatcher = new HeadWatcher( this );

  	if ( !headWatcher )
		gi.Error( ERR_FATAL, "Actor Could not create personality" );

   }


void Actor::InitEnemyManager()
	{
	enemyManager = NULL;
	enemyManager = new EnemyManager( this );

	if ( !enemyManager )
		gi.Error( ERR_FATAL, "Actor Could not create enemyManager" );
	}
void Actor::InitGameComponent()
   {
	
	gameComponent = NULL;
	gameComponent = new EFGameComponent( this );

	if ( !gameComponent )
		gi.Error( ERR_FATAL, "Actor Could not create gameComponent" );
	
	}
void Actor::InitPackageManager()
	{
	packageManager = NULL;
	packageManager = new PackageManager( this );
		
	if ( !packageManager )
		gi.Error( ERR_FATAL, "Actor Could not create packageManager" );
	}
void Actor::InitPersonality()
	{
	personality = NULL;
	personality = new Personality( this );

	if ( !personality )
		gi.Error( ERR_FATAL, "Actor Could not create personality" );
	}
void Actor::InitSensoryPerception()
	{
	sensoryPerception = NULL;
	sensoryPerception = new SensoryPerception( this );

	if ( !sensoryPerception )
		gi.Error( ERR_FATAL, "Actor Could not create sensoryPerception" );

	}
void Actor::InitStrategos()
	{
	strategos = NULL;
	strategos = new DefaultStrategos ( this );

	if ( !strategos )
		gi.Error( ERR_FATAL, "Actor Could not create strategos" );
	}
void Actor::InitThinkStrategy()
	{
	thinkStrategy = NULL;
	thinkStrategy = new DefaultThink();

	if ( !thinkStrategy )
		gi.Error( ERR_FATAL, "Actor Could not create thinkStrategy" );

	}


Actor *Actor::FindPartActor(const char *searchName)
{
    int     current_part;
    part_t *part;
    Entity *partent;
    Actor  *partact;

    for (current_part = 1; current_part <= parts.NumObjects(); current_part++) {
        part = &parts.ObjectAt(current_part);

        partent = part->ent;
        partact = (Actor *)partent;

        if (partact && (partact->part_name == searchName)) {
            return partact;
        }
    }

    return NULL;
}


void Actor::SetNotAllowedToKill( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_ALLOWED_TO_KILL, false );
	}
void Actor::SetAllowFall( Event *ev )
	{
	qboolean allow_fall;

	if ( ev->NumArgs() > 0 )
		allow_fall = ev->GetBoolean( 1 );
	else
		allow_fall = true;

	SetActorFlag( ACTOR_FLAG_ALLOW_FALL, allow_fall );
	}


Actor* Actor::FindActorByName(const str &charName)
{
	int i;
	Actor *act;
	for ( i=1; i<=ActiveList.NumObjects(); i++ )
		{
		act = ActiveList.ObjectAt(i);
		if ( !act )
			continue;

		if ( act->targetname == charName )
			return act;
		}

	return NULL;
}
Event EV_ActorName
	(
	"name",
	EV_DEFAULT,
	"s",
	"name",
	"Specifies the name of this actor type."
	);
Event EV_ActorPartName
	(
	"part_name",
	EV_TIKIONLY,
	"s",
	"part_name",
	"Specifies the name of this part (implying that this is a multi-part creature."
	);

void Actor::Name( Event *ev )
	{
	name = ev->GetString( 1 );
	}

void Actor::PartName( Event *ev )
	{
	part_name = ev->GetString( 1 );
	}

CLASS_DECLARATION( Sentient, Actor, "monster_generic" )
	{

		{ &EV_Activate, 								&Actor::ActivateEvent							},
		{ &EV_Actor_BlindlyFollowPath, 					&Actor::BlindlyFollowPath						},
		{ &EV_Actor_SetSimplifiedThink, 				&Actor::SetSimplifiedThink						},
		{ &EV_Actor_SetActorToActorDamageModifier,		&Actor::SetActorToActorDamageModifier			},
		{ &EV_Use,										&Actor::UseEvent								},
		{ &EV_Actor_OnUse,								&Actor::SetOnUseThread							},
		{ &EV_Actor_NoUse,								&Actor::ClearOnUseThread						},
		{ &EV_Actor_Sleep,								&Actor::Sleep									},
		{ &EV_Actor_Wakeup, 							&Actor::Wakeup									},
		{ &EV_Actor_SetTargetType,						&Actor::SetTargetType							},
		{ &EV_Actor_Start,								&Actor::Start									},
		{ &EV_Pain, 									&Actor::Pain									},
		{ &EV_Killed,									&Actor::Killed									},
		{ &EV_Actor_Dead,								&Actor::Dead									},
		{ &EV_Actor_Suicide,							&Actor::Suicide 								},
		{ &EV_Actor_ForwardSpeed,						&Actor::ForwardSpeedEvent						},
		{ &EV_Actor_Fov,								&Actor::SetFOV									},
		{ &EV_Actor_VisionDistance, 					&Actor::SetVisionDistance						},
		{ &EV_Actor_SetEnemyType,						&Actor::SetEnemyType							},
		{ &EV_Actor_ClearCurrentEnemy,					&Actor::ClearCurrentEnemy						},
		{ &EV_Actor_Swim,								&Actor::SwimEvent								},
		{ &EV_Actor_Fly,								&Actor::FlyEvent								},
		{ &EV_Actor_NotLand,							&Actor::NotLandEvent							},
		{ &EV_Actor_SetDialogMode,						&Actor::SetDialogMode							},
		{ &EV_Actor_RunThread,							&Actor::RunThread								},
		{ &EV_Actor_Statemap,							&Actor::LoadStateMap							},
		{ &EV_Actor_MasterStateMap, 					&Actor::LoadMasterStateMap						},
		{ &EV_Actor_SetBehaviorPackage,                 &Actor::SetBehaviorPackage						},
		{ &EV_Actor_UseBehaviorPackage,                 &Actor::UseBehaviorPackage						},
		{ &EV_Actor_ChildUseBehaviorPackage,			&Actor::ChildUseBehaviorPackage					},
		{ &EV_Actor_ChildSetAnim,						&Actor::ChildSetAnim							},
		{ &EV_Actor_ChildSuicide,						&Actor::ChildSuicide							},
		{ &EV_Actor_IfEnemyVisible, 					&Actor::IfEnemyVisibleEvent 					},
		{ &EV_Actor_IfNear, 							&Actor::IfNearEvent								},
		{ &EV_Actor_Idle,								&Actor::GoIdle 									},
		{ &EV_Actor_LookAt, 							&Actor::LookAt 									},
		{ &EV_Actor_TurnTo, 							&Actor::TurnToEvent								},
		{ &EV_Actor_HeadWatch,							&Actor::HeadWatchEvent							},
		{ &EV_Actor_ResetHead,							&Actor::ResetHeadEvent							},
		{ &EV_Actor_EyeWatch,							&Actor::EyeWatchEvent							},
		{ &EV_Actor_ResetEye,							&Actor::ResetEyeEvent							},
		{ &EV_Actor_ResetTorso, 						&Actor::ResetTorsoEvent							},
		{ &EV_Actor_HeadAndEyeWatch,					&Actor::HeadAndEyeWatchEvent 					},
		{ &EV_Actor_EndBehavior,						&Actor::EndBehaviorEvent						},
		{ &EV_Actor_EndHeadBehavior,					&Actor::EndHeadBehaviorEvent					},
		{ &EV_Actor_EndEyeBehavior,						&Actor::EndEyeBehaviorEvent						},
		{ &EV_Actor_EndTorsoBehavior,					&Actor::EndTorsoBehaviorEvent					},
		{ &EV_Actor_NotifyBehavior, 					&Actor::NotifyBehavior 							},
		{ &EV_Actor_NotifyHeadBehavior, 				&Actor::NotifyHeadBehavior						},
		{ &EV_Actor_NotifyEyeBehavior,					&Actor::NotifyEyeBehavior						},
		{ &EV_Actor_NotifyTorsoBehavior,				&Actor::NotifyTorsoBehavior						},
		{ &EV_Actor_FallToDeath,						&Actor::FallToDeathEvent 						},
		{ &EV_Actor_WalkTo, 							&Actor::WalkTo 									},		
		{ &EV_Actor_WalkWatch,							&Actor::WalkWatch								},
		{ &EV_Actor_JumpTo, 							&Actor::JumpToEvent								},
		{ &EV_Actor_WarpTo, 							&Actor::WarpTo 									},
		{ &EV_Actor_Anim,								&Actor::Anim									},
		{ &EV_Anim_Done,								&Actor::AnimDone									},  
		{ &EV_Torso_Anim_Done,							&Actor::TorsoAnimDone								},  
		{ &EV_Actor_DialogAnimDone, 					&Actor::DialogAnimDone								},  
		{ &EV_Actor_SetAnim,							&Actor::SetAnim									},
		{ &EV_Actor_Attack, 							&Actor::AttackEntity							},
		{ &EV_Actor_AttackPlayer,						&Actor::AttackPlayer 							},
		{ &EV_Actor_Remove, 							&Actor::RemoveUselessBody						},
		{ &EV_Actor_ReserveNode,						&Actor::ReserveNodeEvent						},
		{ &EV_Actor_ReleaseNode,						&Actor::ReleaseNodeEvent						},
		{ &EV_Actor_IfCanHideAt,						&Actor::IfCanHideAtEvent						},
		{ &EV_Actor_IfEnemyWithin,						&Actor::IfEnemyWithinEvent						},
		{ &EV_HeardSound,								&Actor::HeardSound								},
		{ &EV_Actor_BroadcastAlert, 					&Actor::BroadcastAlert							},
		{ &EV_Actor_Melee,								&Actor::MeleeEvent								},
		{ &EV_Actor_PainThreshold,						&Actor::SetPainThresholdEvent					},
		{ &EV_Actor_SetKillThread,						&Actor::SetKillThreadEvent						},
		{ &EV_SetHealth,								&Actor::SetHealth								},
		{ &EV_SetMaxHealth, 							&Actor::SetMaxHealth							},
		{ &EV_Actor_EyePositionOffset,					&Actor::EyeOffset								},
		{ &EV_Actor_DeathFade,							&Actor::DeathFadeEvent							},
		{ &EV_Actor_DeathEffect,						&Actor::setDeathEffect							},
		{ &EV_Actor_DeathShrink,						&Actor::DeathShrinkEvent						},
		{ &EV_Actor_DeathSink,							&Actor::DeathSinkEvent							},
		{ &EV_Actor_StaySolid,							&Actor::StaySolidEvent							},
		{ &EV_Actor_NoChatter,							&Actor::NoChatterEvent							},
		{ &EV_Actor_TurnSpeed,							&Actor::SetTurnSpeed 							},
		{ &EV_Actor_SetActorFlag,						&Actor::SetActorFlag								},
		{ &EV_Actor_SetNotifyFlag,						&Actor::SetNotifyFlag							},
		{ &EV_Actor_SetVar, 							&Actor::SetVar									},
		{ &EV_Actor_PersistData,						&Actor::SetVar									},
		{ &EV_Actor_SetVarTime,							&Actor::SetVarTime								},
		{ &EV_Actor_SetMaxInactiveTime, 				&Actor::SetMaxInactiveTime						},
		{ &EV_Actor_ProjAttack, 						&Actor::FireProjectile 							},
		{ &EV_Actor_BulletAttack,						&Actor::FireBullet								},
		{ &EV_Actor_RadiusAttack,						&Actor::FireRadiusAttack 						},
		{ &EV_Actor_Active, 							&Actor::Active 									},
		{ &EV_Actor_SpawnGib,							&Actor::SpawnGib								},
		{ &EV_Actor_SpawnGibAtTag,						&Actor::SpawnGibAtTag 							},
		{ &EV_Actor_SpawnNamedGib,						&Actor::SpawnNamedGib 							},
		{ &EV_Actor_SpawnBlood, 						&Actor::SpawnBlood								},
		{ &EV_Actor_AIOn,								&Actor::TurnAIOn								},
		{ &EV_Actor_AIOff,								&Actor::TurnAIOff 								},
		{ &EV_Actor_SetIdleThread,						&Actor::SetIdleThread 							},
		{ &EV_ActorRegisterParts,						&Actor::RegisterParts 							},
		{ &EV_ActorRegisterSelf,						&Actor::RegisterSelf							},
		{ &EV_ActorName,								&Actor::Name									},
		{ &EV_ActorPartName,							&Actor::PartName								},
		{ &EV_Actor_SendCommand,						&Actor::SendCommand								},
		{ &EV_ActorSetupTriggerField,					&Actor::SetupTriggerField 						},
		{ &EV_ActorTriggerTouched,						&Actor::TriggerTouched							}, 
		{ &EV_ActorIncomingProjectile,					&Actor::IncomingProjectile						},
		{ &EV_ActorSpawnActor,							&Actor::SpawnActorAtTag							},
		{ &EV_ActorSpawnActorAtLocation,				&Actor::SpawnActorAtLocation					},
		{ &EV_ActorSpawnActorAboveEnemy,				&Actor::SpawnActorAboveEnemy 					},
		{ &EV_Actor_AddDialog,							&Actor::AddDialog								},
		{ &EV_Actor_DialogDone, 						&Actor::DialogDone								},
		{ &EV_Actor_PlayDialog, 						&Actor::PlayDialog								},
		{ &EV_Actor_StopDialog, 						&Actor::StopDialog								},
		{ &EV_Actor_BranchDialog,						&Actor::BranchDialog							},
		{ &EV_Sentient_SetMouthAngle,					&Actor::SetMouthAngle 							},
		{ &EV_Actor_AllowTalk,							&Actor::AllowTalk								},
		{ &EV_Actor_AllowHangBack,						&Actor::AllowHangBack 							},
		{ &EV_Actor_SolidMask,							&Actor::SolidMask 								},
		{ &EV_Actor_IgnoreMonsterClip,					&Actor::IgnoreMonsterClip 						},
		{ &EV_Actor_NotSolidMask,						&Actor::NotSolidMask							},
		{ &EV_Actor_NoMask, 							&Actor::NoMask									},
		{ &EV_Actor_SetMask,							&Actor::SetMask									},
		{ &EV_Actor_PickupEnt,							&Actor::PickupEnt 								},
		{ &EV_Actor_ThrowEnt,							&Actor::ThrowEnt								},
		{ &EV_Actor_Pickup, 							&Actor::Pickup									},
		{ &EV_Actor_Throw,								&Actor::Throw 									},
		{ &EV_Actor_DamageOnceStart,					&Actor::DamageOnceStart							},
		{ &EV_Actor_DamageOnceStop, 					&Actor::DamageOnceStop 							},
		{ &EV_Actor_DamageEnemy,						&Actor::DamageEnemy								},
		{ &EV_Actor_DamageSelf, 						&Actor::DamageSelf								},
		{ &EV_Actor_TurnTowardsEnemy,					&Actor::TurnTowardsEnemy						},
		{ &EV_Actor_TurnTowardsPlayer,					&Actor::TurnTowardsPlayer						},
		{ &EV_Actor_TurnTowardsEntity,					&Actor::TurnTowardsEntity						},
		{ &EV_Actor_GotoNextStage,						&Actor::GotoNextStage 							},
		{ &EV_Actor_GotoPrevStage,						&Actor::GotoPrevStage 							},
		{ &EV_Actor_GotoStage,							&Actor::GotoStage 								},
		{ &EV_Actor_GetStage,							&Actor::GetStage 								},
		{ &EV_Actor_NotifyOthersAtDeath,				&Actor::NotifyOthersAtDeath						},
		{ &EV_Actor_SetBounceOff,						&Actor::SetBounceOff							},
		{ &EV_Actor_BounceOff,							&Actor::BounceOffEvent							},
		{ &EV_Actor_SetBounceOffEffect, 				&Actor::SetBounceOffEffect						},
		{ &EV_Actor_SetHaveThing,						&Actor::SetHaveThing							},
		{ &EV_Actor_SetUseGravity,						&Actor::SetUseGravity 							},
		{ &EV_Actor_SetAllowFall,						&Actor::SetAllowFall							},
		{ &EV_Actor_SetDeathSize,						&Actor::SetDeathSize							},
		{ &EV_Actor_Fade,								&Actor::FadeEvent 								},
		{ &EV_Stun, 									&Actor::StunEvent 								},
		{ &EV_Actor_AddSpawnItem,						&Actor::AddSpawnItem							},
		{ &EV_Actor_SetSpawnChance, 					&Actor::SetSpawnChance							},
		{ &EV_Actor_ClearSpawnItems,					&Actor::ClearSpawnItems							},
		{ &EV_Actor_SetCanBeFinishedBy, 				&Actor::SetCanBeFinishedBy						},
		{ &EV_Actor_SetFeetWidth,						&Actor::SetFeetWidth							},
		{ &EV_Actor_SetCanWalkOnOthers, 				&Actor::SetCanWalkOnOthers						},
		{ &EV_Actor_Push,								&Actor::Push									},
		{ &EV_Actor_Pushable,							&Actor::Pushable								},
		{ &EV_Actor_ChargeWater,						&Actor::ChargeWater								},
		{ &EV_Actor_SetTargetable,						&Actor::SetTargetable 							},
		{ &EV_Actor_ChangeType, 						&Actor::ChangeType								},
		{ &EV_Actor_MinimumMeleeHeight, 				&Actor::SetMinimumMeleeHeight 					},
		{ &EV_Actor_SetDamageAngles,					&Actor::SetDamageAngles							},
		{ &EV_Actor_Immortal,							&Actor::SetImmortal								},
		{ &EV_Actor_SetDieCompletely,					&Actor::SetDieCompletely						},
		{ &EV_Actor_SetBleedAfterDeath, 				&Actor::SetBleedAfterDeath						},
		{ &EV_Actor_IgnorePlacementWarning, 			&Actor::IgnorePlacementWarning					},
		{ &EV_Actor_SetIdleStateName,					&Actor::SetIdleStateName						},
		{ &EV_Actor_SetNotAllowedToKill,				&Actor::SetNotAllowedToKill						},
		{ &EV_TouchTriggers,							&Actor::TouchTriggers 							},
		{ &EV_Actor_IgnoreWater,						&Actor::IgnoreWater								},
		{ &EV_Actor_SimplePathfinding,					&Actor::SimplePathfinding 						},
		{ &EV_Actor_NoPainSounds,						&Actor::NoPainSounds							},
		{ &EV_Actor_UpdateBossHealth,					&Actor::UpdateBossHealth						},
		{ &EV_Actor_SetMaxBossHealth,					&Actor::SetMaxBossHealth						},
		{ &EV_Actor_IgnorePainFromActors,				&Actor::IgnorePainFromActors 					},
		{ &EV_Actor_DamageAllowed,						&Actor::DamageAllowed 							},
		{ &EV_Touch,									&Actor::Touched									},
		{ &EV_Actor_SetEmotion, 						&Actor::SetEmotion								},
		{ &EV_Actor_ReturnProjectile,					&Actor::ReturnProjectile 						},
		{ &EV_Actor_SetRadiusDialogRange,				&Actor::SetRadiusDialogRange 					},
		{ &EV_Actor_SetEyeAngleConstraints, 			&Actor::SetEyeAngles 							},
		{ &EV_Actor_SetActivateThread,					&Actor::SetActivateThread						},
		{ &EV_Actor_SetValidTarget, 					&Actor::SetValidTarget							},
		{ &EV_Actor_SetAlertThread, 					&Actor::SetAlertThread							},
		{ &EV_Actor_RunAlertThread, 					&Actor::RunAlertThread							},
		{ &EV_Actor_CheckActorDead, 					&Actor::checkActorDead							},
		{ &EV_Actor_EnemyActorFlag, 					&Actor::SetFlagOnEnemy							},
		{ &EV_Actor_EnemyAIOn,							&Actor::TurnOnEnemyAI							},
		{ &EV_Actor_EnemyAIOff, 						&Actor::TurnOffEnemyAI							},
		{ &EV_Actor_AttachCurrentEnemy, 				&Actor::AttachCurrentEnemy						},
		{ &EV_Actor_AttachActor,						&Actor::AttachActor								},
		{ &EV_Actor_SetEnemyAttached,					&Actor::SetEnemyAttached 						},
		{ &EV_Actor_PickUpThrowObject,					&Actor::PickupThrowObject						},
		{ &EV_Actor_TossThrowObject,					&Actor::TossThrowObject							},
		{ &EV_Actor_SetTurretMode,						&Actor::SetTurretMode							},
		{ &EV_Actor_SetHitscanResponseChance,			&Actor::SetHitscanResponse						},
		{ &EV_Actor_SetWeaponReady, 					&Actor::SetWeaponReady							},
		{ &EV_Actor_GiveActorWeapon,					&Actor::GiveActorWeapon							},
		{ &EV_Actor_RemoveActorWeapon,					&Actor::RemoveActorWeapon						},
		{ &EV_Actor_SetOnDamageThread,					&Actor::SetOnDamageThread						},
		{ &EV_Actor_SetTimeBetweenSleepChecks,			&Actor::SetTimeBetweenSleepChecks				},
		{ &EV_Actor_FollowWayPoints,					&Actor::FollowWayPoints							},
		{ &EV_Actor_SetAimLeadFactors,					&Actor::SetAimLeadFactors						},
		{ &EV_Actor_SetActorType,						&Actor::SetActorType 							},
		{ &EV_Actor_RespondTo,							&Actor::RespondTo								},
		{ &EV_Actor_PermanentlyRespondTo,				&Actor::PermanentlyRespondTo 					},
		{ &EV_Actor_RegisterBehaviorPackage,			&Actor::RegisterBehaviorPackage					},
		{ &EV_Actor_UnregisterBehaviorPackage,			&Actor::UnRegisterBehaviorPackage				},
		{ &EV_Actor_SetBehaviorPackageTendency,			&Actor::SetPackageTendency						},
		{ &EV_Actor_FuzzyEngine,						&Actor::LoadFuzzyEngine							},
		{ &EV_Actor_SetAbsoluteMaxRange,				&Actor::SetAbsoluteMax							},
		{ &EV_Actor_SetAbsoluteMinRange,				&Actor::SetAbsoluteMin							},
		{ &EV_Actor_SetPreferredMaxRange,				&Actor::SetPreferredMax							},
		{ &EV_Actor_SetPreferredMinRange,				&Actor::SetPreferredMin							},
		
		{ &EV_Actor_UseWeapon,							&Actor::UseActorWeapon							},
		{ &EV_Sentient_Attack,							&Actor::FireWeapon								},
		{ &EV_Sentient_StopFire,						&Actor::StopFireWeapon							},
		{ &EV_Actor_DebugStates,						&Actor::DebugStates								},
		{ &EV_Actor_Disable,							&Actor::SetDisabled								},
		{ &EV_Actor_Cripple,							&Actor::SetCrippled								},
		
		{ &EV_Actor_In_Alcove,							&Actor::SetInAlcove								},
		{ &EV_Actor_ResetMoveDir,						&Actor::ResetMoveDir 							},
		
		{ &EV_Actor_SetMovementMode,					&Actor::SetMovementMode							},
		{ &EV_Actor_SetHeadWatchTarget, 				&Actor::SetHeadWatchTarget						},
		{ &EV_Actor_SetHeadTwitch,		 				&Actor::setHeadTwitch							},
		{ &EV_Actor_SetFuzzyEngineActive,				&Actor::SetFuzzyEngineActive 					},
		{ &EV_Actor_SetNodeID,							&Actor::SetNodeID								},
		{ &EV_Actor_WhatAreYouDoing,					&Actor::WhatAreYouDoing							},
		{ &EV_Actor_WhatsWrong,                         &Actor::WhatsWrong								},
		
		{ &EV_Actor_SetAggressiveness,					&Actor::SetAggressiveness						},
		{ &EV_Actor_SetTalkiness,						&Actor::SetTalkiness 							},
		{ &EV_Actor_SetGroupNumber, 					&Actor::SetGroupNumber							},
		{ &EV_Actor_SetTendency,						&Actor::SetTendency								},
		{ &EV_Actor_SetFloatProperty,					&Actor::SetTendency								},
		
		{ &EV_Actor_Blink,								&Actor::SetBlink								},
		{ &EV_Actor_ClearArmorAdapations,				&Actor::ClearArmorAdaptions						},
		{ &EV_Actor_SetFollowTarget,					&Actor::SetFollowTarget							},
		{ &EV_Actor_SetFollowRange, 					&Actor::SetFollowRange							},
		{ &EV_Actor_SetFollowRangeMin,					&Actor::SetFollowRangeMin						},
		{ &EV_Actor_SetCombatFollowRange,				&Actor::SetFollowCombatRange					},
		{ &EV_Actor_SetCombatFollowRangeMin,			&Actor::SetFollowCombatRangeMin					},
		{ &EV_Actor_SetSteeringDirectionPreference,		&Actor::SetSteeringDirectionPreference			},
		{ &EV_Actor_SetStickToGround,					&Actor::SetStickToGround 						},
		
		{ &EV_Actor_SetDialogMorphMult, 				&Actor::setDialogMorphMult						},
		
		{ &EV_ContextDialog_InContext,					&Actor::InContext 								},
		{ &EV_ContextDialog_IgnoreNextContext,			&Actor::SetIgnoreNextContext					},
		{ &EV_Actor_BroadcastDialog,					&Actor::BroadcastDialog							},

		{ &EV_Actor_ForceSetClip,						&Actor::ForceSetClip							},
		{ &EV_Actor_SetCombatTraceInterval,				&Actor::SetCombatTraceInterval					},
		
		{ &EV_Actor_PutawayWeapon,						&Actor::PutawayWeapon							},
		{ &EV_Actor_UseWeaponDamage,					&Actor::UseWeaponDamage							},
		{ &EV_Actor_StrictlyFollowPath,					&Actor::StrictlyFollowPath						},
		{ &EV_HelperNodeCommand,						&Actor::HelperNodeCommand						},
		{ &EV_Actor_SetMaxHeadYaw,						&Actor::SetMaxHeadYaw							},
		{ &EV_Actor_SetMaxHeadPitch,					&Actor::SetMaxHeadPitch							},
		{ &EV_Actor_SetPostureState,					&Actor::SetPostureState							},
		{ &EV_Actor_SaveOffLastHitBone,					&Actor::SaveOffLastHitBone						},
		{ &EV_Actor_SetPlayPainSoundInterval,			&Actor::SetPlayPainSoundInterval				},
		

		{ &EV_Sentient_GroupMemberInjured,				&Actor::GroupMemberInjured						},
		{ &EV_Actor_EvaluateEnemies,					&Actor::EvaluateEnemies							},
		{ &EV_Actor_ForgetEnemies,						&Actor::ForgetEnemies							},
		{ &EV_Actor_SetPostureStateMap,					&Actor::LoadPostureStateMachine					},
		{ &EV_Posture_Anim_Done,						&Actor::PostureAnimDone							},
		{ &EV_Actor_SendEventToGroup,					&Actor::SendEventToGroup						},
		{ &EV_Actor_GroupAttack,						&Actor::GroupAttack								},
		{ &EV_Actor_GroupActorType,						&Actor::GroupActorType							},
		{ &EV_Actor_SetMasterState,						&Actor::SetMasterState							},
		{ &EV_Actor_PrintDebugMessage,					&Actor::PrintDebugMessage						},
		{ &EV_ProcessGameplayData,						&Actor::processGameplayData						},
		{ &EV_Actor_SelectNextEnemy,					&Actor::SelectNextEnemy							},
		{ &EV_Actor_SelectClosestEnemy,					&Actor::SelectClosestEnemy						},
		{ &EV_Actor_SetGroupDeathThread,				&Actor::SetGroupDeathThread						},
		{ &EV_Actor_SetAnimSet,							&Actor::SetAnimSet								},
		{ &EV_Actor_SetSelfDetonateModel,				&Actor::SetSelfDetonateModel					},
		{ &EV_Actor_SetTalkWatchMode,					&Actor::SetTalkWatchMode						},
		{ &EV_Actor_PrepareMissionFailure,				&Actor::PrepareToFailMission					},
		{ &EV_Actor_FailMission,						&Actor::FailMission								},
		{ &EV_Actor_DebugEvent,							&Actor::DebugEvent								},
		{ &EV_Actor_UnreserveCurrentHelperNode,			&Actor::UnreserveCurrentHelperNode				},
		{ &EV_Actor_ProjectileClose,					&Actor::ProjectileClose							},
		{ &EV_Actor_ClearTorsoAnim,						&Actor::ClearTorsoAnim							},
		{ &EV_Actor_SetContextSoundInterval,			&Actor::SetContextInterval						},
		{ &EV_Actor_SetMinPainTime,						&Actor::SetMinPainTime							},
		{ &EV_Actor_SetEnemyTargeted,					&Actor::SetEnemyTargeted						},
		{ &EV_Actor_SetActivationDelay,					&Actor::SetActivationDelay						},
		{ &EV_Actor_SetActivationStart,					&Actor::SetActivationStart						},
		{ &EV_Actor_SetCheckConeOfFireDistance,			&Actor::SetCheckConeOfFireDistance				},
		{ &EV_Actor_AddCustomThread,					&Actor::AddCustomThread							},
		{ &EV_Actor_SetHeadWatchMaxDistance,			&Actor::SetHeadWatchMaxDistance					},
		{ &EV_Actor_AnimateOnce,						&Actor::AnimateOnce								},
		{ &EV_Actor_SetDeathKnockbackValues,			&Actor::SetDeathKnockbackValues					},

		{ NULL, NULL }
	}
;



Actor::Actor()
    : statemap(NULL)
    , currentState(NULL)
    , lastState(NULL)
    , globalState(NULL)
    , masterstatemap(NULL)
    , currentMasterState(NULL)
    , lastMasterState(NULL)
    , fuzzyEngine(NULL)
    , fuzzyEngine_active(qfalse)
    // IS_INANIMATE in ZERO of them -- retail never ships an actor of type 0.
    , actortype(IS_ENEMY)
    , state_flags(0)       
    , max_boss_health(0.0f)  
    , pain_threshold(20.0f)
    , min_pain_time(1.0f)
    , next_pain_time(0.0f)
    , max_pain_time(2.5f)
    , next_forced_pain_time(0.0f)
    , pain_type(PAIN_SMALL)
    , bounce_off_velocity(-0.5f)
    , minLeadFactor(0.0f)
    , maxLeadFactor(2.0f)
    , saved_bone_hit(-9999) // SDK actor.cpp:2990 -- sentinel "no hit latched"
    , activationStart(0.0f)  
    , activationDelay(0.0f)  
    , dialog_done_time(0.0f)
    , next_blink_time(0.0f)
    , headWatcher(NULL)
    , real_head_pitch(0.0f)
    , dialog_list(NULL)  
    , _ignoreNextContext(false)
    , _nextContextTime(0.0f)
    , _contextInterval(15.0f)
{
    actor_flags1 = 0;
    actor_flags2 = 0;
    actor_flags3 = 0;
    actor_flags4 = 0;

    animset = "AnimSet1";

    SetActorFlag( ACTOR_FLAG_TAKE_DAMAGE,               true );
    SetActorFlag( ACTOR_FLAG_USE_DAMAGESKINS,           true );
    SetActorFlag( ACTOR_FLAG_AI_ON,                     true );
    SetActorFlag( ACTOR_FLAG_ALLOW_TALK,                true );
    SetActorFlag( ACTOR_FLAG_ALLOW_HANGBACK,            true );
    SetActorFlag( ACTOR_FLAG_LAST_ATTACK_HIT,           true );
    SetActorFlag( ACTOR_FLAG_USE_GRAVITY,               true );

    SetActorFlag( ACTOR_FLAG_TARGETABLE,                true );
    SetActorFlag( ACTOR_FLAG_ALLOWED_TO_KILL,           true );
    SetActorFlag( ACTOR_FLAG_DIE_COMPLETELY,            true );
    SetActorFlag( ACTOR_FLAG_BLEED_AFTER_DEATH,         true );
    SetActorFlag( ACTOR_FLAG_TOUCH_TRIGGERS,            true );
    SetActorFlag( ACTOR_FLAG_DAMAGE_ALLOWED,            true );
    SetActorFlag( ACTOR_FLAG_SHOULD_BLINK,              true );
    SetActorFlag( ACTOR_FLAG_MELEE_ALLOWED,             true );

    SetActorFlag( ACTOR_FLAG_LAST_CANSEEENEMY,          false );
    SetActorFlag( ACTOR_FLAG_LAST_CANSEEENEMY_NOFOV,    false );
    SetActorFlag( ACTOR_FLAG_CAPTURED,                  false );
    SetActorFlag( ACTOR_FLAG_TURRET_MODE,               false );
    SetActorFlag( ACTOR_FLAG_INCOMING_HITSCAN,          false );
    SetActorFlag( ACTOR_FLAG_RESPONDING_TO_HITSCAN,     false );
    SetActorFlag( ACTOR_FLAG_MELEE_HIT_WORLD,           false );
    SetActorFlag( ACTOR_FLAG_INACTIVE,                  false );
    SetActorFlag( ACTOR_FLAG_ANIM_DONE,                 false );
    SetActorFlag( ACTOR_FLAG_STATE_DONE_TIME_VALID,     false );
    SetActorFlag( ACTOR_FLAG_MASTER_STATE_DONE_TIME_VALID, false );
    SetActorFlag( ACTOR_FLAG_NOISE_HEARD,               false );
    SetActorFlag( ACTOR_FLAG_INVESTIGATING,             false );
    SetActorFlag( ACTOR_FLAG_DIALOG_PLAYING,            false );
    SetActorFlag( ACTOR_FLAG_RADIUS_DIALOG_PLAYING,     false );
    SetActorFlag( ACTOR_FLAG_NOTIFY_OTHERS_AT_DEATH,    false );
    SetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_ON,            false );
    SetActorFlag( ACTOR_FLAG_BOUNCE_OFF,                false );
    SetActorFlag( ACTOR_FLAG_HAS_THING1,                false );
    SetActorFlag( ACTOR_FLAG_HAS_THING2,                false );
    SetActorFlag( ACTOR_FLAG_HAS_THING3,                false );
    SetActorFlag( ACTOR_FLAG_HAS_THING4,                false );
    SetActorFlag( ACTOR_FLAG_SPAWN_FAILED,              false );
    SetActorFlag( ACTOR_FLAG_FADING_OUT,                false );
    SetActorFlag( ACTOR_FLAG_ALLOW_FALL,                false );
    SetActorFlag( ACTOR_FLAG_STUNNED,                   false );
    SetActorFlag( ACTOR_FLAG_PUSHABLE,                  false );
    SetActorFlag( ACTOR_FLAG_WAIT_FOR_NEW_ENEMY,        false );
    SetActorFlag( ACTOR_FLAG_DEATHFADE,                 false );
    SetActorFlag( ACTOR_FLAG_DEATHSHRINK,               false );
    SetActorFlag( ACTOR_FLAG_DEATHSINK,                 false );
    SetActorFlag( ACTOR_FLAG_NOCHATTER,                 false );
    SetActorFlag( ACTOR_FLAG_STAYSOLID,                 false );
    SetActorFlag( ACTOR_FLAG_FINISHED,                  false );
    SetActorFlag( ACTOR_FLAG_IN_LIMBO,                  false );
    SetActorFlag( ACTOR_FLAG_CAN_WALK_ON_OTHERS,        false );
    SetActorFlag( ACTOR_FLAG_LAST_TRY_TALK,             false );
    SetActorFlag( ACTOR_FLAG_IMMORTAL,                  false );
    SetActorFlag( ACTOR_FLAG_AT_COVER_NODE,             false );
    SetActorFlag( ACTOR_FLAG_TURNING_HEAD,              false );
    SetActorFlag( ACTOR_FLAG_IGNORE_STUCK_WARNING,      false );
    SetActorFlag( ACTOR_FLAG_IGNORE_OFF_GROUND_WARNING, false );
    SetActorFlag( ACTOR_FLAG_IGNORE_WATER,              false );
    SetActorFlag( ACTOR_FLAG_NEVER_IGNORE_SOUNDS,       false );
    SetActorFlag( ACTOR_FLAG_SIMPLE_PATHFINDING,        false );
    SetActorFlag( ACTOR_FLAG_NO_PAIN_SOUNDS,            false );
    SetActorFlag( ACTOR_FLAG_UPDATE_BOSS_HEALTH,        false );
    SetActorFlag( ACTOR_FLAG_IGNORE_PAIN_FROM_ACTORS,   false );
    SetActorFlag( ACTOR_FLAG_STARTED,                   false );
    SetActorFlag( ACTOR_FLAG_DEATHGIB,                  false );
    SetActorFlag( ACTOR_FLAG_WEAPON_READY,              false );
    SetActorFlag( ACTOR_FLAG_DISABLED,                  false );
    SetActorFlag( ACTOR_FLAG_CRIPPLED,                  false );
    SetActorFlag( ACTOR_FLAG_IN_ALCOVE,                 false );
    SetActorFlag( ACTOR_FLAG_IN_CONE_OF_FIRE,           false );
    SetActorFlag( ACTOR_FLAG_IN_PLAYER_CONE_OF_FIRE,    false );
    SetActorFlag( ACTOR_FLAG_PLAYER_IN_CALL_VOLUME,     false );
    SetActorFlag( ACTOR_FLAG_IN_CALL_VOLUME,            false );
    SetActorFlag( ACTOR_FLAG_OUT_OF_TORSO_RANGE,        false );
    SetActorFlag( ACTOR_FLAG_DUCKED,                    false );
    SetActorFlag( ACTOR_FLAG_PRONE,                     false );
    SetActorFlag( ACTOR_FLAG_RETREATING,                false );
    SetActorFlag( ACTOR_FLAG_HIDDEN,                    false );
    SetActorFlag( ACTOR_FLAG_FOLLOWING_IN_FORMATION,    false );
    SetActorFlag( ACTOR_FLAG_DISPLAYING_FAILURE_FX,     false );
    SetActorFlag( ACTOR_FLAG_GROUPMEMBER_INJURED,       false );
    SetActorFlag( ACTOR_FLAG_CAN_HEAL_OTHER,            false );
    SetActorFlag( ACTOR_FLAG_STRICTLY_FOLLOW_PATHS,     false );
    SetActorFlag( ACTOR_FLAG_ATTACKING_ENEMY,           false );
    SetActorFlag( ACTOR_FLAG_UPDATE_HATE_WITH_ATTACKERS, false );

    SetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER,          false );
    SetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER_NOFOV,    false );
    SetActorFlag( ACTOR_FLAG_PLAYING_DIALOG_ANIM,        false );
    SetActorFlag( ACTOR_FLAG_USING_HUD,                  false );
    SetActorFlag( ACTOR_FLAG_FORCE_LIFEBAR,              false );
    SetActorFlag( ACTOR_FLAG_UPDATE_ACTION_LEVEL,        true );
    SetActorFlag( ACTOR_FLAG_CAN_CHANGE_ANIM,            true );
    SetActorFlag( ACTOR_FLAG_USE_FOLLOWRANGE_FOR_NODES,  false );
    SetActorFlag( ACTOR_FLAG_IMMEDIATE_ACTIVATE,         false );
    SetActorFlag( ACTOR_FLAG_CANNOT_USE,                 false );
    SetActorFlag( ACTOR_FLAG_CANNOT_FREEZE,              false );

    currentHelperNode.node   = NULL;
    currentHelperNode.mask   = 0;

    ignoreHelperNode.node   = NULL;
    ignoreHelperNode.mask   = 0;
    ignoreHelperNode.nodeID = 0;

    thinkStrategy     = NULL;
    gameComponent     = NULL;
    sensoryPerception = NULL;
    strategos         = NULL;
    enemyManager      = NULL;
    packageManager    = NULL;
    personality       = NULL;
    postureController = NULL;
    movementSubsystem = NULL;
    combatSubsystem   = NULL;

    InitThinkStrategy();
    InitGameComponent();
    InitSensoryPerception();
    InitEnemyManager();
    InitStrategos();
    InitPackageManager();
    InitMovementSubsystem();
    InitPersonality();
    InitCombatSubsystem();
    InitHeadWatcher();
    InitPostureController();

    feet_width  = 0.0f;
    last_origin = vec_zero;
    // in the port, which is NOT retail and is observable: every helper-node search gates
    currentHelperNode.nodeID = 0;


    if (LoadingSavegame) {
        return;
    }








    edict->s.eType = ET_MODELANIM;
    edict->svflags |= SVF_MONSTER;
    edict->ownerNum = ENTITYNUM_NONE;
    edict->clipmask   = MASK_MONSTERSOLID;
    setMoveType(MOVETYPE_STEP);

    mass = 200;
    // MOHAA DIVERGENCE REMOVED: `flags |= FL_THINK;` used to stand here.  Retail NEVER
    flags &= ~FL_THINK;
    flags &= ~FL_ANIMATE;

    if (com_blood->integer) {
        flags |= FL_BLOOD;
        flags |= FL_DIE_GIBS;
    }

    {
        Event *immunity_event = new Event(EV_Sentient_AddImmunity);
        immunity_event->AddString("falling");
        ProcessEvent(immunity_event);
    }

    setSolidType(SOLID_BBOX);

    takedamage = DAMAGE_AIM;

    health     = 100;
    max_health = 100;

    SetActorFlag(ACTOR_FLAG_ALLOWED_TO_KILL, qtrue);





    minEyeYawAngle   = -30.0f;
    maxEyeYawAngle   = 30.0f;
    minEyePitchAngle = -30.0f;
    maxEyePitchAngle = 30.0f;

    ondamage_threshold = 0;
    last_used_time     = 0.0f;
    forcedEnemy        = NULL;  

    VectorClear(edict->s.viewangles);





























    setAngles(angles);
    movementSubsystem->setAnimDir(orientation[0]);
    movementSubsystem->setMoveDir(orientation[0]);

    newanimnum      = -1;  
    animnum         = -1;
    newanimevent    = NULL;
    newTorsoAnimNum = -1;
    newTorsoAnimEvent = NULL;

    absoluteMin  = 20.0f;
    absoluteMax  = 175.0f;
    preferredMin = 50.0f;
    preferredMax = 175.0f;

    deathKnockbackVerticalValue   = 300;
    deathKnockbackHorizontalValue = 500;

    chattime = G_Random( 20.0f );

    targetType  = ATTACK_ANY;
    validTarget = true;

    behaviorCode      = BEHAVIOR_SUCCESS;
    headBehaviorCode  = BEHAVIOR_SUCCESS;
    eyeBehaviorCode   = BEHAVIOR_SUCCESS;
    torsoBehaviorCode = BEHAVIOR_SUCCESS;

    haveThrowObject = false;
    notify_flags1   = 0;  

    chattime          = 0;
    nextsoundtime     = 0;
    DialogMode        = DIALOG_MODE_NORMAL;
    radiusDialogRange = 125.0f;  

    next_drown_time = 0;
    // SDK actor.cpp:3048 -- retail seeds a full breath, not zero.  It was 0 here while
    air_finished    = level.time + 5.0f;
    bullet_hits     = 0;

    state_time             = level.time;
    masterstate_time       = level.time;
    times_done             = 0;
    masterstate_times_done = 0;
    state_done_time        = 0;
    masterstate_done_time  = 0;
    last_time_active       = 0;
    showStates             = DEBUG_NONE;
    talkMode               = TALK_TURNTO;
    useConvAnims           = true;

    setSize(Vector(-16, -16, 0), Vector(16, 16, 116));

    saved_mode = 0;

    deadflag    = DEAD_NO;
    groundentity = NULL;

    // :3050-3051.  global_state_name was NEVER set, so `Actor::SetMasterState`'s very
    globalState       = NULL;
    global_state_name = "GLOBAL_MAIN";

    emotion = "none";


    gunoffset   = Vector(0, 0, 44);
    eyeoffset   = Vector(0, 0, 0);
    eyeposition = Vector(0, 0, 64);
    velocity    = Vector(0, 0, -20);

    saved_behavior      = NULL;
    saved_headBehavior  = NULL;
    saved_eyeBehavior   = NULL;
    saved_torsoBehavior = NULL;
    currentBehavior     = "";
    currentTorsoBehavior = "";

    incoming_time   = 0;
    incoming_bullet = false;

    max_inactive_time    = MAX_INACTIVE_TIME;
    last_jump_time       = 0;
    actorrange_time      = 0;
    last_height          = 0;
    canseeenemy_time     = 0;
    canseeplayer_time    = 0;
    stage                = 1;
    num_of_spawns        = 0;
    mode                 = ACTOR_MODE_AI;
    next_find_enemy_time = 0;
    minimum_melee_height = -100.0f;
    damage_angles        = 0;
    next_pain_sound_time = 0;
    last_ground_z        = 0;

    actor_to_actor_damage_modifier = 1.0f;
    hitscan_response_chance        = 0;
    shotsFired                     = 0;
    timeBetweenSleepChecks         = 10.0f;

    _controlType = ACTOR_CONTROL_NONE;

    followTarget.currentFollowTarget   = NULL;    
    followTarget.specifiedFollowTarget = NULL;    
    followTarget.maxRangeIdle          = 384.0f;  
    followTarget.minRangeIdle          = 256.0;   
    followTarget.maxRangeCombat        = 512.0f;  
    followTarget.minRangeCombat        = 256.0f;  
    _steeringDirectionPreference       = STEER_RIGHT_ALWAYS;  

    next_player_near = 0;
    stunned_end_time = 0;
    spawn_chance     = 0;
    haveAttached     = false;

    currentSplineTime               = 0;
    _dialogMorphMult                = 1.0f;
    _useWeaponDamage                = WEAPON_ERROR;
    _nextCheckForWorkNodeTime       = 0;
    _nextCheckForHibernateNodeTime  = 0;
    _nextCheckForEnemyPath          = 0;
    _havePathToEnemy                = false;
    _nextPathDistanceToFollowTargetCheck = 0;
    _checkedChance                  = false;
    _levelAIOff                     = false;
    _nextPlayPainSoundTime          = 0;
    _playPainSoundInterval          = 2.0f;

    lastPathCheck_Work   = 0;
    lastPathCheck_Flee   = 0;
    lastPathCheck_Patrol = 0;
    testing              = false;


    showModel();

    if (!LoadingSavegame) {
        PostEvent(EV_Actor_Start, EV_POSTSPAWN);
    }
}

bool Actor::IsEntityAlive(const Entity *ent)
{
    return (ent && !ent->deadflag && (ent->health > 0.0f) && !(ent->flags & FL_NOTARGET) && level.ai_on);
}

Actor::~Actor()
{
    if (groupcoordinator) {
        groupcoordinator->RemoveEntityFromGroup(this, GetGroupID());
    }

    if (GetActorFlag(ACTOR_FLAG_DIALOG_PLAYING)) {
        StopDialog();
    }

    if (SleepList.ObjectInList(this)) {
        SleepList.RemoveObject(this);
    }
    if (ActiveList.ObjectInList(this)) {
        ActiveList.RemoveObject(this);
    }
    if (TeamMateList.ObjectInList(this)) {
        TeamMateList.RemoveObject(this);    
    }

    for (int i = threadList.NumObjects(); i > 0; i--) {
        threadlist_t *threadEntry = threadList.ObjectAt(i);
        delete threadEntry;
    }
    threadList.FreeObjectList();

    if (headWatcher) {
        delete headWatcher;
        headWatcher = NULL;
    }

    if (movementSubsystem) {
        delete movementSubsystem;
        movementSubsystem = NULL;
    }

    if (combatSubsystem) {
        delete combatSubsystem;
        combatSubsystem = NULL;
    }

    if (thinkStrategy) {
        delete thinkStrategy;
        thinkStrategy = NULL;
    }
    if (gameComponent) {
        delete gameComponent;
        gameComponent = NULL;
    }
    if (sensoryPerception) {
        delete sensoryPerception;
        sensoryPerception = NULL;
    }
    if (strategos) {
        delete strategos;
        strategos = NULL;
    }
    if (enemyManager) {
        delete enemyManager;
        enemyManager = NULL;
    }
    if (packageManager) {
        delete packageManager;
        packageManager = NULL;
    }
    if (personality) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
        delete personality;
#pragma GCC diagnostic pop
        personality = NULL;
    }
    if (postureController) {
        delete postureController;
        postureController = NULL;
    }

    for (int i = stateVarList.NumObjects(); i > 0; i--) {
        StateVar *var = stateVarList.ObjectAt(i);
        delete var;
    }
    stateVarList.FreeObjectList();

    if (behavior) {
        delete behavior;
        behavior = NULL;
    }
    if (headBehavior) {
        delete headBehavior;
        headBehavior = NULL;
    }
    if (eyeBehavior) {
        delete eyeBehavior;
        eyeBehavior = NULL;
    }
    if (torsoBehavior) {
        delete torsoBehavior;
        torsoBehavior = NULL;
    }

    freeConditionals(conditionals);
    freeConditionals(master_conditionals);
    freeConditionals(fuzzy_conditionals);

    FreeDialogList();
}




void Actor::processGameplayData(Event *ev)
{
    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm->hasObject(getArchetype())) {
        return;
    }

    str objname = getArchetype();

    if (gpm->hasProperty(objname, "fov")) {
        sensoryPerception->SetFOV(gpm->getFloatValue(objname, "fov"));
    }

    if (gpm->hasProperty(objname, "visiondistance")) {
        sensoryPerception->SetVisionDistance(gpm->getFloatValue(objname, "visiondistance"));
    }
}

void Actor::setSize(Vector min, Vector max)
{
    Sentient::setSize(min * edict->s.scale, max * edict->s.scale);
}

void Actor::Start(Event *ev)
{
    if (target.length() > 0) {
        PostEvent(EV_ActorRegisterSelf, FRAMETIME);
    }

    // printed "stuck in world" that retail does not print -- 116 tall does not fit where

    // called below at retail's own position in Start), never with an unbounded drop.




    {
        Vector animDir;
        angles.AngleVectors(&animDir);

        movementSubsystem->setMoveDir(animDir);
        movementSubsystem->setAnimDir(animDir);
    }

    _dropActorToGround();

    Wakeup();
    enemyManager->TrySleep();

    SetActorFlag(ACTOR_FLAG_STARTED, qtrue);

    if (!statemap) {
        SetAnim("idle");
    }

    // max_health) was a divergence -- retail never writes max_health from the archetype.
    {
        GameplayManager *gpm = GameplayManager::getTheGameplayManager();
        GameplayFormulaData fd(this);

        if (gpm && gpm->hasObject(getArchetype()) && gpm->hasFormula("Health")) {
            health = gpm->calculate("Health", fd);
        }
    }
}


























bool Actor::CanTarget(void) const
{
    return GetActorFlag( ACTOR_FLAG_TARGETABLE );
}

bool Actor::IsFinishable(void)
{
    if (health > 0 && health < 30) {
        return true;
    }
    return false;
}

bool Actor::IsImmortal(void)
{
    return GetActorFlag(ACTOR_FLAG_IMMORTAL) != 0;
}




trace_t Actor::Trace(const Vector& end, const char *reason) const
{
    return Trace(origin, end, reason);
}

trace_t Actor::Trace(const float distance, const char *reason) const
{
    Vector endPoint(orientation[0][0], orientation[0][1], orientation[0][2]);
    endPoint.normalize();
    endPoint *= distance;
    endPoint += origin;
    return Trace(endPoint, reason);
}

trace_t Actor::Trace(const float angle, const float distance, const char *reason) const
{
    vec3_t end;
    RotatePointAroundVector(end, orientation[2], orientation[0], angle);
    Vector endPoint(end);
    endPoint.normalize();
    endPoint *= distance;
    endPoint += origin;
    return Trace(endPoint, reason);
}

trace_t Actor::Trace(const Vector& begin, const Vector& end, const char *reason) const
{
    return Trace(begin, end, edict->clipmask, reason);
}

trace_t Actor::Trace(const Vector& begin, const Vector& end, const int contentMask, const char *reason) const
{
    Vector beginPoint;
    Vector endPoint;
    trace_t fullStepUpTrace;
    trace_t halfStepUpTrace;
    trace_t straightTrace;
    trace_t *bestTrace;
    float bestFraction;
    bool betterTrace;


    beginPoint = begin + movementSubsystem->getStep();
    endPoint   = end   + movementSubsystem->getStep();

    fullStepUpTrace = G_Trace( beginPoint, mins , maxs , endPoint, this, contentMask, false, reason );

    bestTrace = &fullStepUpTrace;
    bestFraction = fullStepUpTrace.fraction;


    if ( bestFraction < 1.0f || bestTrace->startsolid )
    {
        beginPoint = begin + ( movementSubsystem->getStep() / 2.0f );
        endPoint   = end   + ( movementSubsystem->getStep() / 2.0f );

        halfStepUpTrace = G_Trace( beginPoint, mins , maxs , endPoint, this, contentMask, false, reason );


        if ( halfStepUpTrace.fraction > bestFraction && ( !halfStepUpTrace.startsolid || bestTrace->startsolid ) )
            betterTrace = true;
        else if ( !halfStepUpTrace.startsolid && bestTrace->startsolid )
            betterTrace = true;
        else
            betterTrace = false;

        if ( betterTrace )
        {
            bestTrace = &halfStepUpTrace;
            bestFraction = halfStepUpTrace.fraction;
        }
    }


    if ( bestFraction < 1.0f || bestTrace->startsolid )
    {
        beginPoint = begin;
        endPoint   = end;

        straightTrace = G_Trace( beginPoint, mins , maxs , endPoint, this, contentMask, false, reason );


        if ( straightTrace.fraction > bestFraction && ( !straightTrace.startsolid || bestTrace->startsolid ) )
            betterTrace = true;
        else if ( !straightTrace.startsolid && bestTrace->startsolid )
            betterTrace = true;
        else
            betterTrace = false;

        if ( betterTrace )
        {
            bestTrace = &straightTrace;
            bestFraction = straightTrace.fraction;
        }
    }

    if ( g_showactortrace->integer )
    {
        G_DebugLine( bestTrace->endpos, endPoint,    1.0f, 0.0f, 0.0f, 1.0f );
        G_DebugLine( beginPoint, bestTrace->endpos , 0.0f, 0.0f, 1.0f, 1.0f );
        G_DebugLine( beginPoint, endPoint,    0.0f, 1.0f, 0.0f, 1.0f );
    }

    return *bestTrace;
}

qboolean Actor::CanJump(void)
{
    return (animate->HasAnim("jump") && animate->HasAnim("fall") && animate->HasAnim("land"));
}

void Actor::StrictlyFollowPath(Event *ev)
{
    SetActorFlag(ACTOR_FLAG_STRICTLY_FOLLOW_PATHS, ev->GetBoolean(1));
}

void Actor::SetSteeringDirectionPreference(Event *ev)
{
    str preference = ev->GetString(1);

    if (preference == "steer_left_always") {
        _steeringDirectionPreference = STEER_LEFT_ALWAYS;
    } else if (preference == "steer_randomly") {
        _steeringDirectionPreference = STEER_RANDOMLY;
    } else if (preference == "steer_best") {
        _steeringDirectionPreference = STEER_RANDOMLY;
    } else {
        _steeringDirectionPreference = STEER_RIGHT_ALWAYS;
    }
}


void Actor::ForwardSpeedEvent(Event *ev)
{
    movementSubsystem->setForwardSpeed( ev->GetFloat( 1 ) );
}



// occupy and do not survive a save.  That is a property of the flags, not of the
// archive: retail never had them.

#define RETAIL_ACTOR_FLAG_FORCE_LIFEBAR 94



// The base call is Sentient::Archive, NOT SimpleActor::Archive: retail's Actor derives
void Actor::Archive(Archiver& arc)
{
    int      i;
    int      num;
    str      temp_state_name;
    str      temp_global_state_name;
    str      temp_last_state_name;
    str      temp_master_state_name;
    str      temp_last_master_state_name;
    qboolean behavior_bool;
    qboolean hBehavior_bool;
    qboolean eBehavior_bool;
    qboolean tBehavior_bool;

    Sentient::Archive(arc); 

    arc.ArchiveSafePointer(&forcedEnemy);          
    arc.ArchiveString(&newanim);                   
    arc.ArchiveInteger(&newanimnum);               
    arc.ArchiveInteger(&animnum);                  
    arc.ArchiveString(&animname);                  
    arc.ArchiveEventPointer(&newanimevent);        
    arc.ArchiveString(&last_anim_event_name);      

    arc.ArchiveString(&newTorsoAnim);              
    arc.ArchiveInteger(&newTorsoAnimNum);          
    arc.ArchiveString(&TorsoAnimName);             
    arc.ArchiveEventPointer(&newTorsoAnimEvent);   
    arc.ArchiveString(&last_torso_anim_event_name);

    arc.ArchiveFloat(&absoluteMin);                
    arc.ArchiveFloat(&absoluteMax);                
    arc.ArchiveFloat(&preferredMin);               
    arc.ArchiveFloat(&preferredMax);               
    arc.ArchiveFloat(&activationDelay);            
    arc.ArchiveFloat(&activationStart);            

    ArchiveEnum(actortype, actortype_t);     
    ArchiveEnum(targetType, targetType_t);         
    arc.ArchiveBoolean(&validTarget);              
    arc.ArchiveBool(&_checkedChance);              
    arc.ArchiveBool(&_levelAIOff);                 
    arc.ArchiveFloat(&bounce_off_velocity);        

    if (arc.Saving()) {
        behavior_bool = behavior != NULL;
        arc.ArchiveBoolean(&behavior_bool);
        if (behavior_bool) {
            arc.ArchiveObject(behavior);
        }

        hBehavior_bool = false;
        arc.ArchiveBoolean(&hBehavior_bool);

        eBehavior_bool = false;
        arc.ArchiveBoolean(&eBehavior_bool);

        tBehavior_bool = torsoBehavior != NULL;
        arc.ArchiveBoolean(&tBehavior_bool);
        if (tBehavior_bool) {
            arc.ArchiveObject(torsoBehavior);
        }
    } else {
        arc.ArchiveBoolean(&behavior_bool);
        behavior = behavior_bool ? (Behavior *)arc.ReadObject() : NULL;

        arc.ArchiveBoolean(&hBehavior_bool);
        if (hBehavior_bool) {
            arc.ReadObject(); 
        }

        arc.ArchiveBoolean(&eBehavior_bool);
        if (eBehavior_bool) {
            arc.ReadObject(); 
        }

        arc.ArchiveBoolean(&tBehavior_bool);
        torsoBehavior = tBehavior_bool ? (Behavior *)arc.ReadObject() : NULL;
    }

    ArchiveEnum(behaviorCode, BehaviorReturnCode_t);      
    ArchiveEnum(headBehaviorCode, BehaviorReturnCode_t);  
    ArchiveEnum(eyeBehaviorCode, BehaviorReturnCode_t);   
    ArchiveEnum(torsoBehaviorCode, BehaviorReturnCode_t); 

    arc.ArchiveBoolean(&haveThrowObject);                 
    arc.ArchiveString(&animset);                          

    arc.ArchiveUnsigned(&actor_flags1);
    arc.ArchiveUnsigned(&actor_flags2);
    arc.ArchiveUnsigned(&actor_flags3);
    arc.ArchiveUnsigned(&actor_flags4);

    arc.ArchiveUnsigned(&notify_flags1);                  
    arc.ArchiveUnsigned(&state_flags);                    

    arc.ArchiveFloat(&chattime);                          
    arc.ArchiveFloat(&nextsoundtime);                     
    arc.ArchiveFloat(&_nextCheckForEnemyPath);            
    arc.ArchiveBool(&_havePathToEnemy);                   
    arc.ArchiveFloat(&_nextPathDistanceToFollowTargetCheck); 

    arc.ArchiveFloat(&_nextPlayPainSoundTime);            
    arc.ArchiveFloat(&_playPainSoundInterval);            

    ArchiveEnum(DialogMode, DialogMode_t);                
    arc.ArchiveFloat(&radiusDialogRange);                 

    if (arc.Saving()) {
        DialogNode_t *dialog_node;
        byte more;
        str alias_name;
        str parm_shadow1;
        int current_parm;

        dialog_node = dialog_list;

        while (dialog_node) {
            more = true;
            arc.ArchiveByte(&more);

            alias_name = dialog_node->alias_name;

            arc.ArchiveString(&alias_name);
            arc.ArchiveInteger(&dialog_node->random_flag);
            arc.ArchiveInteger(&dialog_node->number_of_parms);
            arc.ArchiveFloat(&dialog_node->random_percent);
            ArchiveEnum(dialog_node->dType, DialogType_t);

            for (current_parm = 0; current_parm < dialog_node->number_of_parms; current_parm++) {
                arc.ArchiveByte(&dialog_node->parms[current_parm].type);

                parm_shadow1 = dialog_node->parms[current_parm].parm;

                arc.ArchiveString(&parm_shadow1);

                parm_shadow1 = dialog_node->parms[current_parm].parm2;

                arc.ArchiveString(&parm_shadow1);
            }

            dialog_node = dialog_node->next;
        }

        more = false;
        arc.ArchiveByte(&more);
    } else {
        byte more;
        DialogNode_t *new_dialog_node;
        DialogNode_t *current_dialog_node = NULL;
        str alias_name;
        str parm_shadow1;
        int current_parm;

        arc.ArchiveByte(&more);

        while (more) {
            new_dialog_node = NewDialogNode();

            if (current_dialog_node) {
                current_dialog_node->next = new_dialog_node;
            } else {
                dialog_list = new_dialog_node;
            }

            current_dialog_node = new_dialog_node;
            new_dialog_node->next = NULL;

            arc.ArchiveString(&alias_name);
            strcpy(new_dialog_node->alias_name, alias_name.c_str());

            arc.ArchiveInteger(&new_dialog_node->random_flag);
            arc.ArchiveInteger(&new_dialog_node->number_of_parms);
            arc.ArchiveFloat(&new_dialog_node->random_percent);
            ArchiveEnum(new_dialog_node->dType, DialogType_t);

            for (current_parm = 0; current_parm < new_dialog_node->number_of_parms; current_parm++) {
                arc.ArchiveByte(&new_dialog_node->parms[current_parm].type);

                arc.ArchiveString(&parm_shadow1);
                strcpy(new_dialog_node->parms[current_parm].parm, parm_shadow1.c_str());

                arc.ArchiveString(&parm_shadow1);
                strcpy(new_dialog_node->parms[current_parm].parm2, parm_shadow1.c_str());
            }

            arc.ArchiveByte(&more);
        }
    }

    arc.ArchiveString(&_branchDialogName);      
    arc.ArchiveFloat(&dialog_done_time);        
    arc.ArchiveString(&dialog_state_name);      
    arc.ArchiveString(&dialog_old_state_name);  
    arc.ArchiveBool(&_ignoreNextContext);       
    arc.ArchiveString(&_nextContextToIgnore);   
    arc.ArchiveFloat(&_nextContextTime);        
    arc.ArchiveFloat(&_contextInterval);        

    arc.ArchiveSafePointer(&scriptthread);      

    arc.ArchiveString(&kill_thread);            
    arc.ArchiveString(&escape_thread);          
    arc.ArchiveString(&captured_thread);        
    arc.ArchiveString(&activate_thread);        
    arc.ArchiveString(&onuse_thread_name);      
    arc.ArchiveString(&ondamage_thread);        
    arc.ArchiveString(&alert_thread);           
    arc.ArchiveString(&idle_thread);            

    arc.ArchiveFloat(&pain_threshold);          
    arc.ArchiveFloat(&next_drown_time);         
    arc.ArchiveFloat(&air_finished);            
    arc.ArchiveInteger(&pain_type);             
    arc.ArchiveVector(&pain_angles);            
    arc.ArchiveInteger(&bullet_hits);           
    arc.ArchiveFloat(&next_pain_time);          
    arc.ArchiveFloat(&min_pain_time);           
    arc.ArchiveFloat(&next_forced_pain_time);   
    arc.ArchiveFloat(&max_pain_time);           
    arc.ArchiveString(&_deathEffect);           

    if (arc.Saving()) {
        temp_state_name             = currentState ? currentState->getName() : "";
        temp_global_state_name      = globalState ? globalState->getName() : "";
        temp_last_state_name        = lastState ? lastState->getName() : "";
        temp_master_state_name      = currentMasterState ? currentMasterState->getName() : "";
        temp_last_master_state_name = lastMasterState ? lastMasterState->getName() : "";

        arc.ArchiveString(&statemap_name);
        arc.ArchiveString(&masterstatemap_name);
        arc.ArchiveString(&temp_state_name);
        arc.ArchiveString(&temp_global_state_name);
        arc.ArchiveString(&temp_last_state_name);
        arc.ArchiveString(&temp_master_state_name);
        arc.ArchiveString(&temp_last_master_state_name);
    } else {
        arc.ArchiveString(&statemap_name);
        arc.ArchiveString(&masterstatemap_name);

        if (statemap_name.length()) {
            Event *event;

            event = new Event(EV_Actor_Statemap);
            event->AddString(statemap_name.c_str());
            event->AddString("");
            event->AddInteger(1);
            ProcessEvent(event);
        }

        arc.ArchiveString(&temp_state_name);
        arc.ArchiveString(&temp_global_state_name);
        arc.ArchiveString(&temp_last_state_name);

        if (statemap) {
            currentState = statemap->FindState(temp_state_name.c_str());
            globalState  = statemap->FindGlobalState(temp_global_state_name.c_str());
            lastState    = statemap->FindState(temp_last_state_name.c_str());
        }

        if (masterstatemap_name.length()) {
            Event *event;

            event = new Event(EV_Actor_MasterStateMap);
            event->AddString(masterstatemap_name.c_str());
            event->AddString("");
            event->AddInteger(1);
            ProcessEvent(event);
        }

        arc.ArchiveString(&temp_master_state_name);
        arc.ArchiveString(&temp_last_master_state_name);

        if (masterstatemap) {
            currentMasterState = masterstatemap->FindState(temp_master_state_name.c_str());
            lastMasterState    = masterstatemap->FindState(temp_last_master_state_name.c_str());
        }
    }

    arc.ArchiveFloat(&state_time);              
    arc.ArchiveFloat(&masterstate_time);        
    arc.ArchiveInteger(&times_done);            
    arc.ArchiveInteger(&masterstate_times_done);
    arc.ArchiveFloat(&state_done_time);         
    arc.ArchiveFloat(&masterstate_done_time);   

    arc.ArchiveFloat(&last_time_active);        
    ArchiveEnum(showStates, stateDebugType_t);  
    ArchiveEnum(talkMode, talkModeStates_t);    
    arc.ArchiveBool(&useConvAnims);             


    arc.ArchiveString(&fuzzyengine_name);      
    arc.ArchiveBoolean(&fuzzyEngine_active);   

    if (arc.Loading() && fuzzyengine_name.length()) {  
        Event *event;

        event = new Event(EV_Actor_FuzzyEngine);
        event->AddString(fuzzyengine_name.c_str());
        ProcessEvent(event);
    }

    arc.ArchiveFloat(&maxEyeYawAngle);          
    arc.ArchiveFloat(&minEyeYawAngle);          
    arc.ArchiveFloat(&maxEyePitchAngle);        
    arc.ArchiveFloat(&minEyePitchAngle);        

    arc.ArchiveInteger(&saved_mode);            

    // matches retail content that never runs a head/eye behaviour while talking.
    if (arc.Saving()) {
        behavior_bool  = saved_behavior     != NULL;
        hBehavior_bool = saved_headBehavior != NULL;
        eBehavior_bool = saved_eyeBehavior  != NULL;
        tBehavior_bool = saved_torsoBehavior != NULL;
        arc.ArchiveBoolean(&behavior_bool);
        if (behavior_bool) {
            arc.ArchiveObject(saved_behavior);
        }
        arc.ArchiveBoolean(&hBehavior_bool);
        if (hBehavior_bool) {
            arc.ArchiveObject(saved_headBehavior);
        }
        arc.ArchiveBoolean(&eBehavior_bool);
        if (eBehavior_bool) {
            arc.ArchiveObject(saved_eyeBehavior);
        }
        arc.ArchiveBoolean(&tBehavior_bool);
        if (tBehavior_bool) {
            arc.ArchiveObject(saved_torsoBehavior);
        }
    } else {
        arc.ArchiveBoolean(&behavior_bool);
        if (behavior_bool) {
            saved_behavior = (Behavior *)arc.ReadObject();
        }
        arc.ArchiveBoolean(&hBehavior_bool);
        if (hBehavior_bool) {
            saved_headBehavior = (Behavior *)arc.ReadObject();
        }
        arc.ArchiveBoolean(&eBehavior_bool);
        if (eBehavior_bool) {
            saved_eyeBehavior = (Behavior *)arc.ReadObject();
        }
        arc.ArchiveBoolean(&tBehavior_bool);
        if (tBehavior_bool) {
            saved_torsoBehavior = (Behavior *)arc.ReadObject();
        }
    }

    arc.ArchiveSafePointer(&saved_scriptthread);  
    arc.ArchiveSafePointer(&saved_actorthread);   
    arc.ArchiveString(&saved_anim_name);          
    arc.ArchiveString(&saved_state_name);         
    arc.ArchiveString(&saved_anim_event_name);    

    arc.ArchiveString(&part_name);                

    { 
        part_t part;
        int    number_of_parts;
        part_t *part_ptr;

        if (arc.Saving()) {
            number_of_parts = parts.NumObjects();
        } else {
            parts.ClearObjectList();
            number_of_parts = 0;
        }

        arc.ArchiveInteger(&number_of_parts);

        if (arc.Loading()) {
            parts.Resize(number_of_parts);
        }

        for (i = 1; i <= number_of_parts; i++) {
            if (arc.Saving()) {
                part     = parts.ObjectAt(i);
                part_ptr = &part;
            } else {
                part.state_flags = 0;
                parts.AddObject(part);
                part_ptr = parts.AddressOfObjectAt(i);
            }

            arc.ArchiveSafePointer(&part_ptr->ent);
            arc.ArchiveUnsigned(&part_ptr->state_flags);
        }
    }

    arc.ArchiveSafePointer(&incoming_proj);       
    arc.ArchiveFloat(&incoming_time);             
    arc.ArchiveBoolean(&incoming_bullet);         

    arc.ArchiveString(&name);                 
    arc.ArchiveFloat(&max_inactive_time);         

    arc.ArchiveVector(&eyeoffset);                
    arc.ArchiveFloat(&last_jump_time);            

    arc.ArchiveString(&enemytype);                

    arc.ArchiveFloat(&actorrange_time);           
    arc.ArchiveFloat(&last_height);               
    arc.ArchiveSafePointer(&last_ent);            

    arc.ArchiveFloat(&canseeenemy_time);          
    arc.ArchiveFloat(&canseeplayer_time);         
    arc.ArchiveInteger(&stage);                   
    arc.ArchiveInteger(&num_of_spawns);           
    arc.ArchiveSafePointer(&spawnparent);         
    arc.ArchiveVector(&last_attack_pos);          
    arc.ArchiveVector(&last_attack_enemy_pos);    
    arc.ArchiveSafePointer(&last_attack_entity_hit);     
    arc.ArchiveVector(&last_attack_entity_hit_pos);      
    arc.ArchiveInteger(&mode);                    
    arc.ArchiveVector(&last_known_enemy_pos);     
    arc.ArchiveVector(&last_known_player_pos);    

    arc.ArchiveFloat(&feet_width);                
    arc.ArchiveVector(&last_origin);              

    arc.ArchiveFloat(&next_find_enemy_time);      
    arc.ArchiveFloat(&minimum_melee_height);      
    arc.ArchiveFloat(&damage_angles);             

    arc.ArchiveFloat(&real_head_pitch);           
    arc.ArchiveFloat(&next_pain_sound_time);      
    arc.ArchiveFloat(&last_ground_z);             

    arc.ArchiveString(&emotion);                  
    arc.ArchiveFloat(&next_blink_time);           

    arc.ArchiveFloat(&actor_to_actor_damage_modifier); 

    arc.ArchiveFloat(&last_used_time);            

    arc.ArchiveFloat(&hitscan_response_chance);   
    arc.ArchiveInteger(&shotsFired);              
    arc.ArchiveInteger(&ondamage_threshold);      
    arc.ArchiveFloat(&timeBetweenSleepChecks);    

    arc.ArchiveInteger(&saved_bone_hit);          

    arc.ArchiveSafePointer(&_controller);         
    ArchiveEnum(_controlType, ActorControlType);  

    arc.ArchiveSafePointer(&currentHelperNode.node);  
    arc.ArchiveInteger(&currentHelperNode.mask);      
    arc.ArchiveInteger(&currentHelperNode.nodeID);    

    arc.ArchiveSafePointer(&ignoreHelperNode.node);   
    arc.ArchiveInteger(&ignoreHelperNode.mask);       
    arc.ArchiveInteger(&ignoreHelperNode.nodeID);     

    arc.ArchiveSafePointer(&followTarget.currentFollowTarget);   
    arc.ArchiveSafePointer(&followTarget.specifiedFollowTarget); 
    arc.ArchiveFloat(&followTarget.maxRangeIdle);                
    arc.ArchiveFloat(&followTarget.minRangeIdle);                
    arc.ArchiveFloat(&followTarget.maxRangeCombat);              
    arc.ArchiveFloat(&followTarget.minRangeCombat);              

    arc.ArchiveInteger(&_steeringDirectionPreference); 

    { 
        StateVar *stateVar;

        if (arc.Saving()) {
            num = stateVarList.NumObjects();
            arc.ArchiveInteger(&num);

            for (i = 1; i <= num; i++) {
                stateVar = stateVarList.ObjectAt(i);
                arc.ArchiveString(&stateVar->varName);
                arc.ArchiveString(&stateVar->varValue);
                arc.ArchiveFloat(&stateVar->varTime);
            }
        } else {
            arc.ArchiveInteger(&num);

            for (i = 1; i <= num; i++) {
                stateVar = new StateVar;
                stateVar->varTime = 0.0f;
                stateVarList.AddObject(stateVar);
                arc.ArchiveString(&stateVar->varName);
                arc.ArchiveString(&stateVar->varValue);
                arc.ArchiveFloat(&stateVar->varTime);
            }
        }
    }

    { 
        if (arc.Saving()) {
            num = threadList.NumObjects();
            arc.ArchiveInteger(&num);

            for (i = 1; i <= num; i++) {
                threadlist_t *threadListEntry = threadList.ObjectAt(i);

                arc.ArchiveString(&threadListEntry->threadType);
                arc.ArchiveString(&threadListEntry->threadName);
            }
        } else {
            arc.ArchiveInteger(&num);

            threadList.FreeObjectList();

            for (i = 1; i <= num; i++) {
                threadlist_t *threadListEntry = new threadlist_t;

                threadList.AddObject(threadListEntry);

                arc.ArchiveString(&threadListEntry->threadType);
                arc.ArchiveString(&threadListEntry->threadName);
            }
        }
    }

    arc.ArchiveSafePointer(&trigger);             
    arc.ArchiveString(&command);                  

    arc.ArchiveString(&idle_state_name);          
    arc.ArchiveString(&master_idle_state_name);   
    arc.ArchiveString(&global_state_name);        

    arc.ArchiveFloat(&next_player_near);          
    arc.ArchiveSafePointer(&pickup_ent);          
    arc.ArchiveFloat(&stunned_end_time);          

    spawn_items.Archive(arc);                     
    arc.ArchiveFloat(&spawn_chance);              

    arc.ArchiveString(&bounce_off_effect);        

    can_be_finsihed_by_mods.Archive(arc);         

    arc.ArchiveFloat(&max_boss_health);           

    arc.ArchiveBoolean(&haveAttached);            

    arc.ArchiveFloat(&currentSplineTime);         
    arc.ArchiveFloat(&_dialogMorphMult);          
    ArchiveEnum(_useWeaponDamage, weaponhand_t);  
    arc.ArchiveFloat(&_nextCheckForWorkNodeTime); 
    arc.ArchiveFloat(&_nextCheckForHibernateNodeTime); 

    arc.ArchiveFloat(&minLeadFactor);             
    arc.ArchiveFloat(&maxLeadFactor);             

    if (arc.Saving()) {
        bool simplifiedThink = thinkStrategy && thinkStrategy->isSimple();

        arc.ArchiveBool(&simplifiedThink);

        thinkStrategy->DoArchive(arc);
    } else {
        bool simplifiedThink;

        arc.ArchiveBool(&simplifiedThink);

        if (simplifiedThink) {
            delete thinkStrategy;
            thinkStrategy = new SimplifiedThink(this);
        }

        thinkStrategy->DoArchive(arc);
    }

    gameComponent->DoArchive(arc, this);     
    sensoryPerception->DoArchive(arc, this); 
    strategos->DoArchive(arc, this);         
    enemyManager->DoArchive(arc, this);      
    packageManager->DoArchive(arc, this);    
    movementSubsystem->DoArchive(arc, this); 
    personality->DoArchive(arc, this);       
    combatSubsystem->DoArchive(arc, this);   
    headWatcher->DoArchive(arc, this);       
    postureController->DoArchive(arc, this); 

    arc.ArchiveFloat(&lastPathCheck_Work);   
    arc.ArchiveFloat(&lastPathCheck_Flee);   
    arc.ArchiveFloat(&lastPathCheck_Patrol); 
    arc.ArchiveBoolean(&testing);            

    if (isThinkOn()) {
        Wakeup();
    } else {
        Sleep();
    }
}





void Actor::WarpTo(Event *ev)
{
    PathNode *goal_node;

    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_WarpTo);
        }
        return;
    }

    if (ev->NumArgs() > 0) {
        goal_node = thePathManager.FindNode(ev->GetString(1));

        if (goal_node) {
            origin = goal_node->origin;
            setOrigin(origin);

            angles = goal_node->angles;
            setAngles(angles);

            NoLerpThisFrame();
        } else {
            gi.WDPrintf("Warpto failed : couldn't find goal node %s\n", ev->GetString(1));
        }
    }
}


void Actor::BlindlyFollowPath(Event *ev)
{
    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_BlindlyFollowPath);
        } else if (mode == ACTOR_MODE_AI) {
            SendMoveDone(ev->GetThread());
        }
        return;
    }

    StartMode(ACTOR_MODE_SCRIPT);

    Event *e = new Event(EV_Behavior_Args);

    e->SetSource(ev->GetSource());

    if (ev->GetSource() == EV_FROM_SCRIPT) {
        e->SetThread(ev->GetThread());
        e->SetLineNumber(ev->GetLineNumber());
    }

    if (ev->NumArgs() > 0) {
        e->AddString(ev->GetString(1));
    } else {
        e->AddString("walk");
    }

    if (ev->NumArgs() > 1) {
        e->AddFloat(ev->GetFloat(2));
    }

    if (ev->NumArgs() > 2) {
        e->AddString(ev->GetString(3));
    }

    FollowPathBlindly *b = new FollowPathBlindly;
    b->ProcessEvent(e);
    SetBehavior(b, NULL, ev->GetThread());
}

void Actor::ShowInfo(void)
{
    gi.Printf("\nEntity #   : %d\n", entnum);
    gi.Printf("Class ID   : %s\n", getClassID());
    gi.Printf("Classname  : %s\n", getClassname());
    gi.Printf("Name       : %s\n", name.c_str());

    if (part_name.length() > 0)
        gi.Printf("Part name  : %s\n", part_name.c_str());

    gi.Printf("\n");

    gi.Printf("Targetname : %s\n", TargetName());
    gi.Printf("Origin     : ( %f, %f, %f )\n", origin.x, origin.y, origin.z);
    gi.Printf("Bounds     : Mins( %.2f, %.2f, %.2f ) Maxs( %.2f, %.2f, %.2f )\n",
              mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z);

    gi.Printf("\n");

    if (behavior)
        gi.Printf("Behavior   : %s\n", behavior->getClassname());
    else
        gi.Printf("Behavior   : NULL -- was '%s'\n", currentBehavior.c_str());

    if (headBehavior)
        gi.Printf("Head Behavior   : %s\n", headBehavior->getClassname());
    else
        gi.Printf("Head Behavior   : NULL -- was '%s'\n", currentHeadBehavior.c_str());

    if (eyeBehavior)
        gi.Printf("Eye Behavior   : %s\n", eyeBehavior->getClassname());
    else
        gi.Printf("Eye Behavior   : NULL -- was '%s'\n", currentEyeBehavior.c_str());

    if (torsoBehavior)
        gi.Printf("Torso Behavior   : %s\n", torsoBehavior->getClassname());
    else
        gi.Printf("Torso Behavior   : NULL -- was '%s'\n", currentTorsoBehavior.c_str());

    if (currentState)
        gi.Printf("State   : %s\n", currentState->getName());
    else
        gi.Printf("State   : NONE\n");

    if (GetActorFlag(ACTOR_FLAG_AI_ON))
        gi.Printf("AI is ON\n");
    else
        gi.Printf("AI is OFF\n");

    if (sensoryPerception) {
        sensoryPerception->ShowInfo();
    }

    if (isThinkOn())
        gi.Printf("Think is ON\n");
    else
        gi.Printf("Think is OFF\n");

    if (mode == ACTOR_MODE_IDLE)
        gi.Printf("Mode : IDLE\n");
    else if (mode == ACTOR_MODE_AI)
        gi.Printf("Mode : AI\n");
    else if (mode == ACTOR_MODE_SCRIPT)
        gi.Printf("Mode : SCRIPT\n");
    else if (mode == ACTOR_MODE_TALK)
        gi.Printf("Mode : TALK\n");

    gi.Printf("\n");

    gi.Printf("Actortype  : %d\n", actortype);

    gi.Printf("Model      : %s\n", model.c_str());
    gi.Printf("Anim       : %s\n", animname.c_str());
    gi.Printf("Health     : %f\n", health);

    gi.Printf("\ncurrentEnemy: ");

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();
    if (currentEnemy) {
        gi.Printf("%d : '%s'\n", currentEnemy->entnum, currentEnemy->targetname.c_str());
    } else {
        gi.Printf("None\n");
    }

    gi.Printf("actortype: %d\n", actortype);

    switch (deadflag) {
    case DEAD_NO:
        gi.Printf("deadflag: NO\n");
        break;
    case DEAD_DYING:
        gi.Printf("deadflag: DYING\n");
        break;
    case DEAD_DEAD:
        gi.Printf("deadflag: DEAD\n");
        break;
    case DEAD_RESPAWNABLE:
        gi.Printf("deadflag: RESPAWNABLE\n");
        break;
    }

    gi.Printf("\n");
    if (behavior) {
        gi.Printf("Behavior Info:\n");
        gi.Printf("Game time: %f\n", level.time);
        behavior->ShowInfo(*this);
        gi.Printf("\n");
    }

    if (headBehavior) {
        gi.Printf("Head Behavior Info:\n");
        gi.Printf("Game time: %f\n", level.time);
        headBehavior->ShowInfo(*this);
        gi.Printf("\n");
    }

    if (eyeBehavior) {
        gi.Printf("Eye Behavior Info:\n");
        gi.Printf("Game time: %f\n", level.time);
        eyeBehavior->ShowInfo(*this);
        gi.Printf("\n");
    }

    if (torsoBehavior) {
        gi.Printf("Torso Behavior Info:\n");
        gi.Printf("Game time: %f\n", level.time);
        torsoBehavior->ShowInfo(*this);
        gi.Printf("\n");
    }
}




void Actor::WalkTo(Event *ev)
{
    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_WalkTo);
        } else if (mode == ACTOR_MODE_AI) {
            SendMoveDone(ev->GetThread());
        }
        return;
    }

    StartMode(ACTOR_MODE_SCRIPT);

    Event *e = new Event(EV_Behavior_Args);

    e->SetSource(ev->GetSource());

    if (ev->GetSource() == EV_FROM_SCRIPT) {
        e->SetThread(ev->GetThread());
        e->SetLineNumber(ev->GetLineNumber());
    }

    if (ev->NumArgs() > 1)
        e->AddString(ev->GetString(2));
    else
        e->AddString("walk");

    if (ev->NumArgs() > 0)
        e->AddToken(ev->GetToken(1));

    if (ev->NumArgs() > 2) {
        e->AddInteger(0);
        e->AddInteger(ev->GetInteger(3));
    }

    if (ev->NumArgs() > 3)
        e->AddInteger(ev->GetInteger(4));

    SetBehavior(new GotoSpecified, e, ev->GetThread());
}

void Actor::SetMovementMode(Event *ev)
{
    str modeType = ev->GetString(1);

    if (!Q_stricmp("normal", modeType.c_str())) {
        movementSubsystem->setMovementType(MOVEMENT_TYPE_NORMAL);
    } else if (!Q_stricmp("anim", modeType.c_str())) {
        movementSubsystem->setMovementType(MOVEMENT_TYPE_ANIM);
    }
}







void Actor::LookAt(Event *ev)
{
    Entity   *ent;
    ::TurnTo *turnTo;

    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_LookAt);
        } else if (mode == ACTOR_MODE_AI) {
            SendMoveDone(ev->GetThread());
        }
        return;
    }

    StartMode(ACTOR_MODE_SCRIPT);

    ent = ev->GetEntity(1);
    if (ent && (ent != world)) {
        turnTo = new ::TurnTo;
        turnTo->SetTarget(ent);
        SetBehavior(turnTo, NULL, ev->GetThread());
    }
}

void Actor::EyeWatchEvent(Event *ev)
{
    Event *event;

    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_EyeWatch);
        } else if (mode == ACTOR_MODE_AI) {
            SendMoveDone(ev->GetThread());
        }
        return;
    }

    StartMode(ACTOR_MODE_SCRIPT);

    event = new Event(EV_Behavior_Args);
    event->AddEntity(ev->GetEntity(1));

    if (ev->NumArgs() > 1) {
        event->AddFloat(ev->GetFloat(2));
    }

    EyeWatch *eyeWatch = new EyeWatch;
    eyeWatch->ProcessEvent(event);
    SetBehavior(eyeWatch, NULL, ev->GetThread());
}

void Actor::HeadAndEyeWatchEvent(Event *ev)
{
    Event *event;

    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_HeadWatch);
        } else if (mode == ACTOR_MODE_AI) {
            SendMoveDone(ev->GetThread());
        }
        return;
    }

    StartMode(ACTOR_MODE_SCRIPT);

    event = new Event(EV_Behavior_Args);
    event->AddEntity(ev->GetEntity(1));

    if (ev->NumArgs() > 1) {
        event->AddFloat(ev->GetFloat(2));
    }

    HeadAndEyeWatch *headAndEyeWatch = new HeadAndEyeWatch;
    headAndEyeWatch->ProcessEvent(event);
    SetBehavior(headAndEyeWatch, NULL, ev->GetThread());
}

void Actor::SetEyeAngles(Event *ev)
{
    minEyeYawAngle   = ev->GetFloat(1);
    maxEyeYawAngle   = ev->GetFloat(2);
    minEyePitchAngle = ev->GetFloat(3);
    maxEyePitchAngle = ev->GetFloat(4);

    if (minEyeYawAngle < -180.0f) {
        minEyeYawAngle = -180.0f;
    }
    if (maxEyeYawAngle > 180.0f) {
        maxEyeYawAngle = 180.0f;
    }
    if (minEyePitchAngle < -180.0f) {
        minEyePitchAngle = -180.0f;
    }
    if (maxEyePitchAngle > 180.0f) {
        maxEyePitchAngle = 180.0f;
    }
}












void Actor::turnTowardsEntity(Entity *ent, float extraYaw)
{
    Vector dir;
    Vector new_angles;

    if (!ent) {
        return;
    }

    dir        = ent->centroid - origin;
    new_angles = dir.toAngles();
    new_angles[YAW] += extraYaw;

    angles[YAW]   = new_angles[YAW];
    angles[ROLL]  = 0;
    angles[PITCH] = 0;

    angles.AngleVectors(&dir);

    setAngles(angles);

    if (movementSubsystem) {
        movementSubsystem->setAnimDir(dir);
        movementSubsystem->setMoveDir(dir);
    }
}

void Actor::TurnTowardsEnemy(Event *ev)
{
    float extraYaw;

    Entity *currentEnemy = enemyManager->GetCurrentEnemy();  
    if (!currentEnemy || (currentEnemy->flags & FL_NOTARGET)) {
        return;
    }

    if (ev->NumArgs() > 0) {
        extraYaw = ev->GetInteger(1);
    } else {
        extraYaw = 0;
    }

    turnTowardsEntity(currentEnemy, extraYaw);
}

void Actor::TurnTowardsPlayer(Event *ev)
{
    float   extraYaw;
    // SDK actor.cpp:3648 -- retail uses GetPlayer(0), not G_GetEntity(0).
    Player *player = GetPlayer(0);

    if (!player || (player->flags & FL_NOTARGET)) {
        return;
    }

    if (ev->NumArgs() > 0) {
        extraYaw = ev->GetInteger(1);
    } else {
        extraYaw = 0;
    }

    turnTowardsEntity(player, extraYaw);
}

// steering is the faithful shape here; do not "unify" it with the other two.
void Actor::TurnTowardsEntity(Event *ev)
{
    // return retail does not have.  Consequence: `turntowardsentity` issued while the
    TurnTo *turnTo;

    if (!ModeAllowed(ACTOR_MODE_SCRIPT)) {
        if (mode == ACTOR_MODE_TALK) {
            RepostEvent(ev, EV_Actor_TurnTo);
        } else if (mode == ACTOR_MODE_AI) {
            SendMoveDone(ev->GetThread());
        }
        return;
    }

    StartMode(ACTOR_MODE_SCRIPT);

    turnTo = new ::TurnTo;
    turnTo->SetTarget(ev->GetEntity(1));

    SetBehavior(turnTo, NULL, ev->GetThread());
}












void Actor::Killed(Event *ev)
{
    ClassDef *cls;
    str        deathanim;
    str        newdeathanim;
    Entity    *attacker;
    float      damage;
    qboolean   fallingDeath;
    Weapon    *weapon = 0;

    attacker = ev->GetEntity(1);
    damage   = ev->GetFloat(2);

    G_EntTrash_KilledEntry(this, attacker, damage);

    if (ev->NumArgs() > 4) {
        fallingDeath = ev->GetBoolean(5);
    } else {
        fallingDeath = false;
    }

    if (ev->NumArgs() > 5) {
        weapon = (Weapon *)ev->GetEntity(6);
    }

    if (GetActorFlag(ACTOR_FLAG_UPDATE_BOSS_HEALTH) && max_boss_health) {
        char bosshealth_string[20];
        sprintf(bosshealth_string, "%.5f", health / max_boss_health);
        gi.cvar_set("bosshealth", bosshealth_string);
    }

    if (actortype == IS_TEAMMATE) {
        Player *player;
        player = GetPlayer(0);

        if (player->p_heuristics) {
            player->p_heuristics->IncrementTeammatesKilled();
        }
        if (player->client) {
            ++player->client->ps.stats[STAT_TEAMMATES_KILLED];
        }
    } else {
        UnreserveCurrentHelperNode();
    }

    if (damage && attacker && attacker->isSubclassOf(Player)) {
        Player *player = (Player *)attacker;

        G_EntTrash_Pointer("Killed", player, player->p_heuristics);

        float            points = 0.0f;
        GameplayManager *gpm    = GameplayManager::getTheGameplayManager();
        GameplayFormulaData fd(player, this, weapon, player->getAttackType());
        if (gpm->hasObject(player->getArchetype()) && gpm->hasFormula("Points")) {
            points = gpm->calculate("Points", fd);
        }
        player->AwardPoints((int)points);

        if (actortype == IS_ENEMY) {
            if (player->p_heuristics) {
                player->p_heuristics->IncrementEnemiesKilled();
            }
            if (player->client) {
                ++player->client->ps.stats[STAT_ENEMIES_KILLED];
            }
        }
    }

    if (damage && attacker && attacker->isSubclassOf(Actor)) {
        Actor *actor = (Actor *)attacker;

        actor->InContext("killedenemy", 0);
    }

    if (attacker) {
        KilledEffects(attacker);
    }

    if (next_pain_sound_time <= level.time && !GetActorFlag(ACTOR_FLAG_NO_PAIN_SOUNDS)) {
        next_pain_sound_time = level.time + 0.4f + G_Random(0.2f);
        BroadcastSound();
    }

    if (!GetActorFlag(ACTOR_FLAG_DIE_COMPLETELY)) {
        return;
    }

    if (means_of_death == MOD_BULLET) {
        if (bullet_hits < 5) {
            bullet_hits = 0;
        }

        if ((int)G_Random(100) < (bullet_hits * 10)) {
            means_of_death = MOD_FAST_BULLET;
        }
    }

    if (means_of_death == MOD_ELECTRIC) {
        Event *event;

        event = new Event(EV_DisplayEffect);
        event->AddString("electric");
        ProcessEvent(event);

        event = new Event(EV_DisplayEffect);
        event->AddString("noelectric");
        PostEvent(event, 3.0f + G_Random(2.0f));
    }

    if (attacker
        && (means_of_death == MOD_EXPLOSION
            || means_of_death == MOD_SMALL_EXPLOSION
            || means_of_death == MOD_PLASMASHOTGUN)) {
        float chance = G_Random();

        if (getenv("EF2_DEADWEDGE") && gi.cvar("qa_probe", "0", 0)->integer) {
            gi.Printf("[UIVK:nanseed] ent=%d mod=%d(%s) roll=%.3f take=%d "
                      "org=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) atk=%d "
                      "atkorg=(%.1f,%.1f,%.1f) mass=%d scale=%.6g mv=%d ge=%d "
                      "dkv=%.1f dkh=%.1f hp=%.1f df=%d\n",
                      entnum,
                      means_of_death,
                      MOD_NumToName(means_of_death),
                      chance,
                      chance >= .45 ? 1 : 0,
                      origin[0], origin[1], origin[2],
                      velocity[0], velocity[1], velocity[2],
                      attacker ? attacker->entnum : -1,
                      attacker ? attacker->origin[0] : 0.0f,
                      attacker ? attacker->origin[1] : 0.0f,
                      attacker ? attacker->origin[2] : 0.0f,
                      mass,
                      edict->s.scale,
                      (int)movetype,
                      groundentity ? (int)(groundentity - g_entities) : -1,
                      deathKnockbackVerticalValue,
                      deathKnockbackHorizontalValue,
                      health,
                      (int)deadflag);
        }

        if (chance >= .45) {
            Vector attackerToSelf = origin - attacker->origin;
            attackerToSelf.z = deathKnockbackVerticalValue;
            attackerToSelf.normalize();
            attackerToSelf *= deathKnockbackHorizontalValue;
            velocity = attackerToSelf;

            if (getenv("EF2_KNOCKBACK") && gi.cvar("qa_probe", "0", 0)->integer) {
                Vector uivk_kb_raw = origin - attacker->origin;
                int    uivk_kb_ge  = -1;

                if (groundentity) {
                    uivk_kb_ge = groundentity - g_entities;
                }

                gi.Printf("[UIVK:knockback] "
                          "ent=%d mod=%d(%s) roll=%.4f fire=1 "
                          "imp=(%.1f,%.1f,%.1f) spd=%.1f "
                          "raw=(%.1f,%.1f,%.1f) dkh=%.1f dkv=%.1f "
                          "atk=%d aorg=(%.1f,%.1f,%.1f) org=(%.1f,%.1f,%.1f) "
                          "mass=%d ge=%d mv=%d svtime=%d\n",
                          entnum,
                          means_of_death,
                          MOD_NumToName(means_of_death),
                          chance,
                          velocity[0], velocity[1], velocity[2],
                          velocity.length(),
                          uivk_kb_raw[0], uivk_kb_raw[1], uivk_kb_raw[2],
                          deathKnockbackHorizontalValue,
                          deathKnockbackVerticalValue,
                          attacker->entnum,
                          attacker->origin[0], attacker->origin[1], attacker->origin[2],
                          origin[0], origin[1], origin[2],
                          mass,
                          uivk_kb_ge,
                          (int)movetype,
                          level.inttime);
            }

            UIVK_KB_Arm(entnum);
        }
    }

    if (edict->s.bone_tag[ACTOR_MOUTH_TAG] != -1) {
        SetControllerAngles(ACTOR_MOUTH_TAG, vec_zero);
    }
    if (edict->s.bone_tag[ACTOR_HEAD_TAG] != -1) {
        SetControllerAngles(ACTOR_HEAD_TAG, vec_zero);
    }
    if (edict->s.bone_tag[ACTOR_TORSO_TAG] != -1) {
        SetControllerAngles(ACTOR_TORSO_TAG, vec_zero);
    }
    if (edict->s.bone_tag[ACTOR_LEYE_TAG] != -1) {
        SetControllerAngles(ACTOR_LEYE_TAG, vec_zero);
    }
    if (edict->s.bone_tag[ACTOR_REYE_TAG] != -1) {
        SetControllerAngles(ACTOR_REYE_TAG, vec_zero);
    }

    if (!fallingDeath) {
        cls = getClass("idle");
        SetBehavior((Behavior *)cls->newInstance());
    }

    if (!GetActorFlag(ACTOR_FLAG_ALLOWED_TO_KILL) && attacker && attacker->isSubclassOf(Player)) {
        G_MissionFailed("CivilianKilled");
    }

    flags &= ~(FL_SWIM | FL_FLY);

    deadflag = DEAD_DYING;

    groundentity = NULL;

    edict->svflags |= SVF_DEADMONSTER;

    stopStasis();

    if (!GetActorFlag(ACTOR_FLAG_STAYSOLID)) {
        edict->clipmask = MASK_DEADSOLID;

        if (edict->solid != SOLID_NOT) {
            setContents(CONTENTS_CORPSE);
        }
    }

    if (GetActorFlag(ACTOR_FLAG_DIALOG_PLAYING)) {
        CancelEventsOfType(EV_SetControllerAngles);
        StopSound(CHAN_DIALOG);
    }

    groupcoordinator->MemberDied(this, GetGroupID());

    if (!fallingDeath) {
        deathanim = "death";

        if (GetActorFlag(ACTOR_FLAG_IN_ALCOVE)) {
            deathanim = "death_alcove";
        }

        if (statemap) {
            int   count = 0;
            State *temp_state;
            State *last_temp_state = NULL;
            str   temp_anim;

            if (behavior) {
                behavior->End(*this);
                delete behavior;
                behavior = NULL;
            }

            if (torsoBehavior) {
                torsoBehavior->End(*this);
                delete torsoBehavior;
                torsoBehavior = NULL;
            }

            temp_state = statemap->FindState("DEATH");

            if (temp_state) {
                do {
                    count++;
                    if (count > 50) {
                        gi.Error(ERR_DROP, "Stopping due to possible infinite state loop in death state\n");
                        break;
                    }

                    if (last_temp_state != temp_state) {
                        temp_anim = temp_state->getLegAnim(*this, &conditionals);

                        if (temp_anim.length()) {
                            newdeathanim = temp_anim;
                        }

                        if (last_temp_state) {
                            last_temp_state->ProcessExitCommands(this);
                        }

                        temp_state->ProcessEntryCommands(this);
                    }

                    last_temp_state = temp_state;
                    temp_state = temp_state->Evaluate(*this, &conditionals);
                } while (last_temp_state != temp_state);
            }

            if (newdeathanim.length() > 0) {
                deathanim = newdeathanim;
            }
        }

        if (animate) {
            animate->ClearTorsoAnim();
            animate->ClearLegsAnim();
        }
        SetAnim(deathanim, EV_Actor_Dead, legs);

        ChangeAnim();
    }

    if (spawnparent) {
        spawnparent->num_of_spawns--;
    }

    if (GetActorFlag(ACTOR_FLAG_NOTIFY_OTHERS_AT_DEATH)) {
        NotifyOthersOfDeath();
    }

    if (gameComponent && attacker) {
        gameComponent->HandleDeath(attacker);
    }

    SpawnItems();
    DropItemsOnDeath();

    if (means_of_death == MOD_GIB) {
        Event *event;
        int    i;

        if (blood_model.length() == 0) {
            blood_model = "fx/fx_bspurt.tik";
        }

        if (max_gibs == 0) {
            max_gibs = 4;
        }

        for (i = 0; i < 4; i++) {
            event = new Event(EV_Sentient_SpawnBloodyGibs);
            event->AddInteger(max_gibs);
            ProcessEvent(event);
        }

        PostEvent(EV_Remove, 0.0f);
    }

    if (fallingDeath) {
        PostEvent(EV_Actor_Dead, 0.0f);
        PostEvent(EV_Remove, 5.0f);
    }
}




void Actor::FireWeapon(Event *ev)
{
    combatSubsystem->FireWeapon();
}




































































qboolean Actor::SetAnim(const str& anim, Event *ev, bodypart_t part, const float animationRate)
{
    int num;
    if (!anim.length()) {
        return false;
    }

    if (!GetActorFlag(ACTOR_FLAG_CAN_CHANGE_ANIM)) {
        return false;
    }

    num = gi.Anim_Random(edict->s.modelindex, anim.c_str());


    if (num != -1) {
        if (gi.cvar("qa_probe", "0", 0)->integer && !strncmp(anim.c_str(), "assimilate", 10)) {
            Entity *gameplay_enemy = enemyManager ? enemyManager->GetCurrentEnemy() : NULL;
            float   gameplay_dist  = (gameplay_enemy ? (gameplay_enemy->centroid - centroid).length() : -1.0f);

            gi.Printf("[GAMEPLAY:asml] ent=%d '%s' anim='%s' num=%d part=%d enemy=%d edist=%.1f hp=%.0f\n",
                      entnum, model.c_str(), anim.c_str(), num, (int)part,
                      gameplay_enemy ? gameplay_enemy->entnum : -1, gameplay_dist, health);
        }

        if (part == legs) {
            newanim = anim;
            newanimnum = num;
            animnum = newanimnum;

            if (newanimevent) {
                delete newanimevent;
            }

            newanimevent = ev;

            if (newanimevent) {
                last_anim_event_name = newanimevent->getName();
            } else {
                last_anim_event_name = "";
            }
        }

        if (part == torso) {
            newTorsoAnim = anim;
            newTorsoAnimNum = num;

            if (newTorsoAnimEvent) {
                delete newTorsoAnimEvent;
            }

            newTorsoAnimEvent = ev;

            if (newTorsoAnimEvent) {
                last_torso_anim_event_name = newTorsoAnimEvent->getName();
            } else {
                last_torso_anim_event_name = "";
            }
        }

        if (actortype == IS_INANIMATE) {
            ChangeAnim();
        }

        edict->s.animationRate = animationRate;
        return true;
    }

    warning("Actor::SetAnim", "Actor \"%s\" has no anim named \"%s\"\n", model.c_str(), anim.c_str());

    if (ev) {
        delete ev;
    }

    return false;
}

qboolean Actor::SetAnim(const str& anim, Event& ev, bodypart_t part, const float animationRate)
{
    Event *event;

    event = new Event(ev);
    assert(event);

    return SetAnim(anim, event, part, animationRate);
}





void Actor::ChangeAnim(void)
{
    Vector totalmove;

    if ((newanimnum == -1) && (newTorsoAnimNum == -1))
        return;

    if (newTorsoAnimNum != -1) {
        if ((newTorsoAnimNum == CurrentAnim(torso)) && (TorsoAnimName == newTorsoAnim) && !(edict->s.torso_frame & FRAME_EXPLICIT)) {
            animate->SetAnimDoneEvent(newTorsoAnimEvent, torso);
        } else {
            TorsoAnimName = newTorsoAnim;
            {
                static int torso_sel_rows;

                /* names the torso anim selection itself: which state or behavior
                   resolved what, and to which alias index */
                if (torso_sel_rows < 4000 && gi.cvar("qa_probe", "0", 0)->integer) {
                    torso_sel_rows++;
                    gi.Printf(
                        "[AIM:torso] ent=%d name='%s' num=%d prev=%d\n",
                        entnum,
                        newTorsoAnim.c_str(),
                        newTorsoAnimNum,
                        CurrentAnim(torso)
                    );
                }
            }
            animate->NewAnim(newTorsoAnimNum, newTorsoAnimEvent, torso);
        }

        newTorsoAnimNum = -1;
        newTorsoAnim = "";
        newTorsoAnimEvent = NULL;
    }

    if (newanimnum != -1) {
        if ((newanimnum == CurrentAnim(legs)) && (animname == newanim) && !(edict->s.frame & FRAME_EXPLICIT)) {
            animate->SetAnimDoneEvent(newanimevent);
        } else {
            animname = newanim;
            animate->NewAnim(newanimnum, newanimevent, legs);
        }

        newanimnum = -1;
        newanim = "";
        newanimevent = NULL;
    }
}





void Actor::Think(void)
{
    if (!Director.PlayerReady()) {
        last_time_active = level.time;
        return;
    }

    if (thinkStrategy) {
        thinkStrategy->Think(*this);
    }

    if (!level.ai_on) {
        LevelAIOff();
    } else {
        LevelAIOn();
    }
}














































void Actor::AttackPlayer(Event *ev)
{
    if (!GetActorFlag(ACTOR_FLAG_STARTED)) {
        PostEvent(*ev, FRAMETIME);
        return;
    }

    int i;
    gentity_t *ent;

    for (i = 0; i < maxclients->integer; i++) {
        ent = &g_entities[i];
        if (!ent->inuse || !ent->client || !ent->entity) {
            continue;
        }

        strategos->Attack(ent->entity);
    }
}


void Actor::HeardSound(Event *ev)
{
    Vector location;
    int    soundType = SOUNDTYPE_GENERAL;

    location = ev->GetVector(2);

    if (ev->NumArgs() > 2) {
        soundType = ev->GetInteger(3);
    }

    Entity *soundEnt;
    Entity *theEntity;
    soundEnt = ev->GetEntity(1);

    theEntity = soundEnt;
    if (soundEnt && soundEnt->isSubclassOf(Weapon)) {
        Weapon *soundWeapon;
        soundWeapon = (Weapon *)soundEnt;
        theEntity   = soundWeapon->GetOwner();
    }

    sensoryPerception->Stimuli(STIMULI_SOUND, location, soundType);
    if (GetActorFlag(ACTOR_FLAG_NOISE_HEARD)) {
        enemyManager->TryToAddToHateList(theEntity);
    }

}

void Actor::RespondTo(Event *ev)
{
    str      stimuli = ev->GetString(1);
    qboolean respond = ev->GetBoolean(2);

    sensoryPerception->RespondTo(stimuli, respond);
}

void Actor::PermanentlyRespondTo(Event *ev)
{
    str      stimuli = ev->GetString(1);
    qboolean respond = ev->GetBoolean(2);

    sensoryPerception->PermanentlyRespondTo(stimuli, respond);
}

void Actor::BroadcastAlert(float rad)
{
    Vector enemypos;

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();
    if (!currentEnemy) {
        return;
    }

    if (!(this->flags & FL_NOTARGET)) {
        enemypos = currentEnemy->centroid;
        G_BroadcastAlert(this, centroid, enemypos, rad);
    }
}

void Actor::BroadcastAlert(float rad, int soundtype)
{
    Vector enemypos;

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();
    if (!currentEnemy) {
        return;
    }

    if (!(this->flags & FL_NOTARGET)) {
        enemypos = currentEnemy->centroid;
        G_BroadcastSound(this, centroid, rad, soundtype);
    }
}

void Actor::BroadcastAlert(Event *ev)
{
    float rad = ev->GetFloat(1);
    BroadcastAlert(rad, SOUNDTYPE_ALERT);
}




void Actor::SetTurretMode(Event *ev)
{
    qboolean tmode = true;

    if (ev->NumArgs() > 0) {
        tmode = ev->GetBoolean(1);
    }

    SetActorFlag(ACTOR_FLAG_TURRET_MODE, tmode);
}






































































void Actor::Dead(Event *ev)
{
    {
        static int    uivk_dw_calls[MAX_GENTITIES];
        static int    uivk_dw_frame[MAX_GENTITIES];
        static Vector uivk_dw_org[MAX_GENTITIES];

        if (getenv("EF2_DEADWEDGE") && entnum >= 0 && entnum < MAX_GENTITIES) {
            int uivk_dw_n;

            if (uivk_dw_frame[entnum] > level.framenum) {
                uivk_dw_calls[entnum] = 0;
            }

            uivk_dw_n = uivk_dw_calls[entnum]++;

            if (uivk_dw_n > 0 && gi.cvar("qa_probe", "0", 0)->integer) {
                int uivk_dw_ge = -1;

                if (groundentity) {
                    uivk_dw_ge = groundentity - g_entities;
                }

                gi.Printf("[UIVK:deadwedge] "
                          "ent=%d n=%d ge=%d vel=(%.1f,%.1f,%.1f) mv=%d "
                          "org=(%.1f,%.1f,%.1f) porg=(%.1f,%.1f,%.1f) "
                          "td=(%.2f,%.2f,%.2f) mb=%d fl=%d par=%d imm=%d "
                          "tkd=%d df=%d hp=%.1f "
                          "mt=%d "
                          "svtime=%d\n",
                          entnum,
                          uivk_dw_n,
                          uivk_dw_ge,
                          velocity[0], velocity[1], velocity[2],
                          (int)movetype,
                          origin[0], origin[1], origin[2],
                          uivk_dw_org[entnum][0],
                          uivk_dw_org[entnum][1],
                          uivk_dw_org[entnum][2],
                          total_delta[0], total_delta[1], total_delta[2],
                          (int)M_CheckBottom(this),
                          (int)flags,
                          edict->s.parent,
                          (flags & (FL_IMMOBILE | FL_PARTIAL_IMMOBILE)) ? 1 : 0,
                          (int)takedamage,
                          (int)deadflag,
                          health,
                          movementSubsystem ? (int)movementSubsystem->getMovementType() : -1,
                          level.inttime);
            }

            uivk_dw_frame[entnum] = level.framenum;
            uivk_dw_org[entnum]   = origin;
        }
    }

    // once"; retail calls it unconditionally (decomp __dt__ ordering: the
    animate->StopAnimatingAtEnd();

    edict->clipmask = MASK_DEADSOLID;
    if (edict->solid != SOLID_NOT) {
        setContents(CONTENTS_CORPSE);
    }

    if (edict->s.torso_anim & ANIM_BLEND) {
        animate->StopAnimatingAtEnd(torso);
    }

    edict->s.eFlags |= EF_DONT_PROCESS_COMMANDS;

    if (!groundentity && (velocity != vec_zero) && (movetype != MOVETYPE_STATIONARY)) {
        PostEvent(EV_Actor_Dead, FRAMETIME);
        return;
    }

    flags &= ~(FL_SWIM | FL_FLY);
    turnThinkOff();

    deadflag = DEAD_DEAD;
    setMoveType(MOVETYPE_NONE);
    setOrigin(origin);

    if (trigger) {
        trigger->ProcessEvent(EV_Remove);
    }

    if (spawnparent) {
        PostEvent(EV_Actor_Fade, 0.5f);
    } else {
        PostEvent(EV_Actor_Fade, 5.0f);
    }
}


























































void Actor::Active( Event *ev )
	{
	int active_flag;

	if ( ev->NumArgs() > 0 )
		{
		active_flag = ev->GetInteger( 1 );

		if ( active_flag )
			SetActorFlag( ACTOR_FLAG_INACTIVE, false );
		else
			SetActorFlag( ACTOR_FLAG_INACTIVE, true );
		}
	}


void Actor::AddCustomThread( Event *ev )
{
	AddCustomThread( ev->GetString( 1 ) , ev->GetString( 2 ) );
}


void Actor::AllowHangBack( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_ALLOW_HANGBACK, ev->GetBoolean( 1 ) );
	}


void Actor::AllowTalk( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_ALLOW_TALK, ev->GetBoolean( 1 ) );
	}


void Actor::AttachActor( Event *ev )	
	{
	Actor* new_actor = 0;
	new_actor = new Actor;

	if ( !new_actor )
		return;
	
	str modelName = ev->GetString( 1 );
	str targetName = ev->GetString( 2 );
	str bone = ev->GetString( 3 );
	Vector offset;

	new_actor->setModel( modelName );
	new_actor->SetTargetName( targetName.c_str() );

	if ( ev->NumArgs() > 3 )
		offset = ev->GetVector( 4 );

	int tagnum = gi.Tag_NumForName( this->edict->s.modelindex, bone );

	new_actor->attach(this->entnum , tagnum , false , offset );	

	}


void Actor::AttachCurrentEnemy( Event *ev	)
	{
	Actor *act = 0;
	Vector offset;
	str bone;
	int tagnum;

	bone = ev->GetString( 1 );
	offset = ev->GetVector( 2 );
	tagnum = gi.Tag_NumForName( this->edict->s.modelindex, bone );
	
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	if ( !currentEnemy->isSubclassOf( Actor ) )
		return;

	act = (Actor*)(Entity*)currentEnemy;

	if ( act )
		{
		act->attach(this->entnum , tagnum , false , offset );
	   haveAttached = true;
		}
	}


void Actor::AttachModelToTag(const str &modelName, const str &tagName)
{
    Event *attach_event = new Event(EV_AttachModel);
    attach_event->AddString(modelName);
    attach_event->AddString(tagName);
    PostEvent(attach_event, 0.0f);
}


void Actor::AttackEntity( Event *ev	)
	{
	Entity *target_shadow2 = ev->GetEntity( 1 );

	bool forceEnemy = true;

	if ( !GetActorFlag( ACTOR_FLAG_STARTED ) )
		{
		PostEvent( *ev, FRAMETIME );
		return;
		}

	if ( ev->NumArgs() > 1 )
		forceEnemy = ev->GetBoolean( 2 );

	if ( forceEnemy )
		forcedEnemy = target_shadow2;


	if ( forceEnemy )
		{
		enemyManager->SetCurrentEnemy( target_shadow2 );
		enemyManager->LockOnCurrentEnemy( true );
		return;
		}

	sensoryPerception->Stimuli( STIMULI_SIGHT, target_shadow2 );
				
	if ( enemyManager->IsInHateList( target_shadow2 ) )
		{
		enemyManager->SetCurrentEnemy( target_shadow2 );
		}
	}


void Actor::BroadcastDialog(Event *ev)
	{
	Entity *ent;
	Actor *act;
	Actor *bestAct = NULL;
	Vector delta;
	float dist;
	float bestDist;
	str context = ev->GetString( 1 );
	str responseDialog;
	str bestResponseDialog;
	
	bestDist = SOUND_RADIUS;
	for( int i = 1; i <= SentientList.NumObjects(); i++ )
		{
		ent = SentientList.ObjectAt( i );
		if ( ( ent == this ) || ent->deadflag )
			{
			continue;
			}

		if ( ent->isSubclassOf( Actor ) )
			{
			act = (Actor*)ent;
			delta = origin - act->centroid;
			dist = delta.length();

			if ( dist <= SOUND_RADIUS && dist < bestDist )
				{
				if ( edict->areanum == ent->edict->areanum || gi.AreasConnected( edict->areanum, ent->edict->areanum ) )
					{
					responseDialog = act->FindDialog( act, DIALOG_TYPE_CONTEXT_RESPONSE, context );
					if ( responseDialog.length() )
						{
						bestAct = act;
						bestDist = dist;
						bestResponseDialog = responseDialog;
						}
					}
				}
		
			}
		}

	if ( bestAct )
		{
		bestAct->PlayDialog( (Sentient*)bestAct, DEFAULT_VOL, -1.0f, bestResponseDialog.c_str() , NULL );
		}

	}


void Actor::ChangeType( Event *ev )
	{
	velocity = vec_zero;
	setModel( ev->GetString( 1 ) );
	PostEvent( EV_Actor_Wakeup, FRAMETIME );
	NoLerpThisFrame();
	}


void Actor::CheckStun( void )
	{
	if ( GetActorFlag( ACTOR_FLAG_STUNNED ) && ( stunned_end_time <= level.time ) )
		SetActorFlag( ACTOR_FLAG_STUNNED, false );
	}


void Actor::ChildSetAnim( Event *ev )
{
	ChildSetAnim(ev->GetString( 1 ) , ev->GetString( 2 ) );
}


void Actor::ChildSuicide( Event *ev )
{
	Actor *childActor = 0;
	str childName = ev->GetString( 1 );

	childActor = GetAttachedChildActor(childName);

	if ( childActor )
		childActor->ProcessEvent(EV_Actor_Suicide);

}


void Actor::ChildUseBehaviorPackage( Event *ev )
{
	ChildUseBehaviorPackage(ev->GetString( 1 ) , ev->GetString( 2 ) );
}


void Actor::ClearArmorAdaptions( Event *ev )
   {
   AdaptiveArmor::ClearAdaptionList();
   }


void Actor::ClearCurrentEnemy( Event *ev )
	{
	enemyManager->ClearCurrentEnemy();
	}



void Actor::ClearLegAnim()
{
	
	newanimnum = -1;
	newanim = "";
	newanimevent = NULL;
	animate->ClearLegsAnim();
}


void Actor::ClearTorsoAnim( Event *ev )
{
	ClearTorsoAnim();
}


void Actor::DeathFadeEvent( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_DEATHFADE, true );
	}


void Actor::DeathShrinkEvent(	Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_DEATHSHRINK, true );
	}


void Actor::DeathSinkEvent( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_DEATHSINK, true );
	}


void Actor::DebugEvent( Event *ev )
{
	[[maybe_unused]] int x;
	x = 0;
}


void Actor::DebugStates( Event *ev )
	{
	int state = ev->GetInteger( 1 );

	if ( state >= MAX_DEBUG_TYPES )
		return;

	showStates = ( stateDebugType_t )ev->GetInteger( 1 );
	}


void Actor::DetachModelFromTag(const str &tagName)
{
    Event *detach_event = new Event(EV_RemoveAttachedModel);
    detach_event->AddString(tagName);
    PostEvent(detach_event, 0.0f);
}


void Actor::EndAllBehaviors( void )
	{
	EndBehavior();
	EndHeadBehavior();
	EndEyeBehavior();
	EndTorsoBehavior();
	}


void Actor::EndBehavior( void	)
	{
	Event *event;
	if ( behavior )
		{
		behavior->End( *this );
		if ( _controller && (_controller == behavior->GetController()) )
		{
			event = new Event ( EV_Actor_BehaviorFinished );
			event->AddInteger( behaviorCode );
			event->AddString( behaviorFailureReason );
			_controller->ProcessEvent( event );
		}

		delete behavior;
		behavior = NULL;
		}



	if ( scriptthread )
		{
			CThread *t = scriptthread;
			scriptthread = NULL;

         if ( t )
            {
			   event = new Event( EV_MoveDone );
			   event->AddEntity( this );
			   t->ProcessEvent( event );
            }
         }


   CancelEventsOfType( EV_Actor_EndBehavior );
	}


void Actor::EndBehaviorEvent( Event *ev )
	{
	EndBehavior();
	}


void Actor::EndEyeBehavior( void	)
	{
	if ( eyeBehavior )
		{
		eyeBehavior->End( *this );
		delete eyeBehavior;
		eyeBehavior = NULL;
		}

   CancelEventsOfType( EV_Actor_EndEyeBehavior );	
	}


void Actor::EndEyeBehaviorEvent( Event *ev )
	{
   EndEyeBehavior();
	}


void Actor::EndHeadBehavior( void )
	{

	if ( headBehavior )
		{
		headBehavior->End( *this );
		delete headBehavior;
		headBehavior = NULL;
		}

   CancelEventsOfType( EV_Actor_EndHeadBehavior );	
	}


void Actor::EndHeadBehaviorEvent( Event *ev )
	{
	EndHeadBehavior();
	}


void Actor::EndMode( void )
	{
	str currentanim;

	if ( mode == ACTOR_MODE_AI )
		{
		if ( GetActorFlag( ACTOR_FLAG_UPDATE_BOSS_HEALTH ) && max_boss_health )
			{
			char bosshealth_string[20];
			sprintf( bosshealth_string, "%.5f", 0.0 );
			gi.cvar_set( "bosshealth", bosshealth_string );
			}

		mode = ACTOR_MODE_IDLE;
		ProcessEvent( EV_Actor_Idle );

		if ( currentState )
			{
			currentanim = currentState->getLegAnim( *this, &conditionals );

			if ( currentanim.length() && ( currentanim != animname ) )
				SetAnim( currentanim, EV_Anim_Done );							
			}

		enemyManager->ClearCurrentEnemy();
		}
	else if ( mode == ACTOR_MODE_TALK )
		{
		next_player_near = level.time + 5.0f;
		RestoreMode();
		}
	}


void Actor::EndTorsoBehavior( void )
	{
	if ( torsoBehavior )
		{
		torsoBehavior->End( *this );
		delete torsoBehavior;
		torsoBehavior = NULL;
		}

   CancelEventsOfType( EV_Actor_EndTorsoBehavior );	
	}


void Actor::EndTorsoBehaviorEvent( Event *ev	)
	{
   EndTorsoBehavior();
	}


void Actor::EvaluateEnemies( Event *ev )
{
	if ( !enemyManager->IsLockedOnCurrentEnemy() )		
		enemyManager->FindHighestHateEnemy();		
}


void Actor::FadeEvent( Event *ev	)
	{
	SetActorFlag( ACTOR_FLAG_FADING_OUT, true );

	if ( GetActorFlag( ACTOR_FLAG_DEATHFADE ) )
		{
		ProcessEvent( EV_ForceAlpha );
		ProcessEvent( EV_Fade );
		}
	else if ( GetActorFlag( ACTOR_FLAG_DEATHSHRINK ) )
      ProcessEvent( EV_FadeOut );
	else if ( GetActorFlag( ACTOR_FLAG_DEATHSINK ) )
		ProcessEvent( EV_DeathSinkStart );
	else if ( _deathEffect.length() > 0 )
	{
		Event *newEvent;

		newEvent = new Event( EV_DisplayEffect );
		newEvent->AddString( _deathEffect );
		ProcessEvent( newEvent );

		PostEvent( EV_Remove, 5.0f );
	}
	else
		SetActorFlag( ACTOR_FLAG_FADING_OUT, false );
	}


void Actor::FailMission( Event *ev )
{
	str reason = "DefaultFailure";
	if ( ev->NumArgs() > 0 )
		reason = ev->GetString( 1 );

	G_MissionFailed(reason);
}


void Actor::FallToDeathEvent(	Event *ev )
	{
	Event *e;
	
	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_FallToDeath );
		else if ( mode == ACTOR_MODE_AI )
		   SendMoveDone( ev->GetThread() );
		return;
		}
	
	StartMode( ACTOR_MODE_SCRIPT );

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}
	
	e->AddFloat( ev->GetFloat( 1 ) );
	e->AddFloat( ev->GetFloat( 2 ) );
	e->AddFloat( ev->GetFloat( 3 ) );
	e->AddString( ev->GetString( 4 ) );
	e->AddString( ev->GetString( 5 ) );
	e->AddString( ev->GetString( 6 ) );
	if (ev->NumArgs() > 6 )
		e->AddFloat( ev->GetFloat( 7 ) );

   SetBehavior( new FallToDeath, e, ev->GetThread() );
	
	}


void Actor::FollowWayPoints (	Event *ev )
	{
	Event *e;


	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_WalkTo );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}
	
	e->AddString( ev->GetString( 1 ) );
	
	if ( ev->NumArgs() > 1 )
		e->AddString( ev->GetString( 2 ) );
	else
		e->AddString( "walk" );
	
	if ( ev->NumArgs() > 2 )
      e->AddString( ev->GetString( 3 ) );	

	SetBehavior( new GotoWayPoint, e, ev->GetThread() );
	}


void Actor::ForceSetClip( Event *ev )
{
	edict->contents = CONTENTS_SETCLIP ;
	edict->clipmask = MASK_SETCLIP ;
}


void Actor::ForgetEnemies( Event* ev )
{
	if( enemyManager )
	{
		enemyManager->ClearCurrentEnemy();
		enemyManager->ClearHateList();
	}
}


void Actor::GetStateAnims( Container<const char *> *c )
   {
	if ( statemap )
		statemap->GetAllAnims( c );
   }


void Actor::GiveActorWeapon( Event *ev	)
	{
	const char	*type;
	float       amount;
	float		skillLevel;
	
	amount = 1.0;
	skillLevel = 1.0f;
	type     = ev->GetString( 1 );
	
	if ( ev->NumArgs() > 1 )
		skillLevel = ev->GetFloat( 2 );

	giveItem( type, (int)amount, false, skillLevel );
	}


void Actor::GoIdle (	Event *ev )
	{
	const char *state_name;

	if ( !ModeAllowed( ACTOR_MODE_IDLE ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_Idle );
		return;
		}

	if ( ev->NumArgs() > 0 )
		state_name = ev->GetString( 1 );
	else
		state_name = idle_state_name;

	SetState( state_name );

	StartMode( ACTOR_MODE_IDLE );
	}


void Actor::GroupActorType( Event *ev )
{
	Event *typeEvent;
	str type;

	typeEvent = new Event(EV_Actor_SetActorType);
	typeEvent->AddString( ev->GetString( 1 ) );
	
	// overload, which takes ownership of the event and deletes it.  Going
	groupcoordinator->SendEventToGroup(typeEvent, GetGroupID());

}


void Actor::GroupAttack( Event *ev )
{
	Event *attackEvent;
	Entity *enemy_shadow11;
	bool force = false;

	enemy_shadow11 = enemyManager->GetCurrentEnemy();
	if ( !enemy_shadow11 )
		return;

	if ( ev->NumArgs() > 0 )
		force = ev->GetBoolean( 1 );

	attackEvent = new Event(EV_Actor_Attack);
	attackEvent->AddEntity( enemy_shadow11 );
	attackEvent->AddInteger( force );

	groupcoordinator->SendEventToGroup(attackEvent, GetGroupID());

}


void Actor::GroupMemberInjured( Event *ev )
{
	bool injured = ev->GetBoolean( 1 );

	if ( injured )
		SetActorFlag( ACTOR_FLAG_GROUPMEMBER_INJURED , true );
	else
		SetActorFlag( ACTOR_FLAG_GROUPMEMBER_INJURED , false );
}


void Actor::HandleGameSpecificEvent( Event *ev )
	{
	if ( gameComponent )
		gameComponent->HandleEvent( ev );
	}


void Actor::HeadWatchEvent ( Event *ev	)
	{

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_HeadWatch );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	Entity* entToWatch;
	entToWatch = ev->GetEntity( 1 );

	if ( !entToWatch ) return;

	headWatcher->SetWatchTarget( entToWatch );
	if ( ev->NumArgs() > 1 )
		headWatcher->SetWatchSpeed( ev->GetFloat(2) );

	}


void Actor::HelperNodeCommand( Event *ev )
{
}


void Actor::InContext( Event *ev )
{  
  str theContext = ev->GetString( 1 );
  bool useDefaultMinDist = false;

  if ( ev->NumArgs() > 1 )
	  useDefaultMinDist = ev->GetBoolean( 2 );
  
  InContext( theContext , useDefaultMinDist );
}


void Actor::InitMasterState( void )
	{
	float min_time;
	float max_time;

	if ( !currentMasterState )
		return;

	min_time = currentMasterState->getMinTime();
	max_time = currentMasterState->getMaxTime();

	if ( ( min_time != -1.0f ) && ( max_time != -1.0f ) )
		{
		SetActorFlag( ACTOR_FLAG_MASTER_STATE_DONE_TIME_VALID, true );

		masterstate_done_time = level.time + min_time + G_Random( max_time - min_time );
		}
	else
		{
		SetActorFlag( ACTOR_FLAG_MASTER_STATE_DONE_TIME_VALID, false );
		}

	masterstate_time = level.time;
	masterstate_times_done = 0;	
	}


void Actor::InitState( void )
	{
	float min_time;
	float max_time;

	if ( !currentState )
		return;

	min_time = currentState->getMinTime();
	max_time = currentState->getMaxTime();

	if ( ( min_time != -1.0f ) && ( max_time != -1.0f ) )
		{
		SetActorFlag( ACTOR_FLAG_STATE_DONE_TIME_VALID, true );

		state_done_time = level.time + min_time + G_Random( max_time - min_time );
		}
	else
		{
		SetActorFlag( ACTOR_FLAG_STATE_DONE_TIME_VALID, false );
		}

	state_time = level.time;
	times_done = 0;

	ClearStateFlags();	
	command = "";

	SetActorFlag( ACTOR_FLAG_ANIM_DONE, false );
	SetActorFlag( ACTOR_FLAG_NOISE_HEARD, false );


	}


void Actor::JumpToEvent( Event *ev )
	{
	Event *e;
	int i;
	int n;

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			PostEvent( *ev, FRAMETIME );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}

	n = ev->NumArgs();

	for( i = 1; i <= n; i++ )
		{
		e->AddToken( ev->GetToken( i ) );
		}

	SetBehavior( new JumpToPathNode, e, ev->GetThread() );
	}


void Actor::KilledEffects( Entity *attacker )
   {
   Entity   *ent;
   Event    *event;
	str		target_name;

   if ( g_debugtargets->integer )
      {
      G_DebugTargets( this, "Actor::KilledEffects" );
      }

	target_name = KillTarget();
	if ( target_name && strcmp( target_name, "" ) )
		{
      ent = NULL;
		do
			{
			ent = G_FindTarget( ent, target_name );
			if ( !ent )
				{
				break;
				}
			ent->PostEvent( EV_Remove, 0.0f );
			}
		while ( 1 );
		}

	target_name = Target();
	if ( target_name && strcmp( target_name, "" ) )
		{
      ent = NULL;
		do
			{
			ent = G_FindTarget( ent, target_name );
			if ( !ent )
				{
				break;
				}

			event = new Event( EV_Activate );
			event->AddEntity( attacker );
			ent->PostEvent( event, 0.0f );
			}
		while ( 1 );
		}
	if ( kill_thread.length() > 1 )
      {
      CThread * thread;
      thread = ExecuteThread( kill_thread, false, this );
      if ( !thread )
         warning( "Killed", "could not process kill_thread" );

      }
   }




void Actor::LevelAIOff()
{
	if ( GetActorFlag( ACTOR_FLAG_AI_ON ) )
		{
		_levelAIOff = true;
		TurnAIOff();
		}
}


void Actor::LevelAIOn()
{
	if ( GetActorFlag( ACTOR_FLAG_AI_ON ) ) return;

	if ( _levelAIOff )
		{
		_levelAIOff = false;
		TurnAIOn();
		}
}


void Actor::LoadFuzzyEngine( Event *ev	)
	{

	fuzzyengine_name = ev->GetString( 1 );
	freeConditionals( fuzzy_conditionals );
	fuzzyEngine = GetFuzzyEngine( fuzzyengine_name, ( Condition<Class> * )Conditions, &fuzzy_conditionals, false );
   fuzzyEngine_active = true;

	}


void Actor::LoadMasterStateMap( Event *ev	)
	{
	qboolean loading;	

	masterstatemap_name = ev->GetString( 1 );

	freeConditionals( master_conditionals );
	masterstatemap = GetStatemap( masterstatemap_name, ( Condition<Class> * )Conditions, &master_conditionals, false );
	
	master_idle_state_name = "START";
	
	if ( ev->NumArgs() > 2 )
		loading = ev->GetBoolean( 3 );
	else
		loading = false;


	if ( !loading )
		{
		SetMasterState( master_idle_state_name.c_str() );		
		}
	}


void Actor::LoadPostureStateMachine( Event *ev )
{
	bool loading;
	if ( ev->NumArgs() > 1 )
		loading = ev->GetBoolean( 2 );
	else
		loading = false;

	postureController->setPostureStateMap( ev->GetString( 1 ) , loading );
}


void Actor::LoadStateMap( Event *ev	)
	{
	str anim_name;
	qboolean loading;
	[[maybe_unused]] qboolean packageChange;


	statemap_name = ev->GetString( 1 );

	freeConditionals( conditionals );
	statemap = GetStatemap( statemap_name, ( Condition<Class> * )Conditions, &conditionals, false );

	packageChange = false;
	if ( ev->NumArgs() > 3 )
		{
		packageChange = ev->GetBoolean( 4 );
		}


	if ( ev->NumArgs() > 1 )
		idle_state_name = ev->GetString( 2 );
	else if ( fuzzyEngine )
		idle_state_name = "START";
	else
		idle_state_name = "IDLE";

	if ( ev->NumArgs() > 2 )
		loading = ev->GetBoolean( 3 );
	else
		loading = false;


	if ( !loading )
		{
		SetState( idle_state_name.c_str() );
		SetGlobalState(global_state_name.c_str() );

		if ( currentState )
			anim_name = currentState->getLegAnim( *this, &conditionals );

		if ( anim_name == "" && !newanim.length() )
			anim_name = "idle";
			

		SetAnim( anim_name.c_str(), EV_Anim_Done );
		ChangeAnim();

		}
	}


void Actor::NotifyBehavior( Event *ev )
	{
	if ( behavior )
		{
		behavior->ProcessEvent( EV_Behavior_AnimDone );
		SetActorFlag( ACTOR_FLAG_ANIM_DONE, true );
		}
	}


void Actor::NotifyEyeBehavior( Event *ev )
	{
	if ( eyeBehavior )
		{
		eyeBehavior->ProcessEvent( EV_Behavior_AnimDone );
		SetActorFlag( ACTOR_FLAG_ANIM_DONE, true );
		}
	}


void Actor::NotifyHeadBehavior( Event *ev	)
	{
	if ( headBehavior )
		{
		headBehavior->ProcessEvent( EV_Behavior_AnimDone );
		SetActorFlag( ACTOR_FLAG_ANIM_DONE, true );
		}
	}


void Actor::NotifyTorsoBehavior( Event *ev )
	{
	if ( torsoBehavior )
		{
		torsoBehavior->ProcessEvent( EV_Behavior_AnimDone );
		SetActorFlag( ACTOR_FLAG_TORSO_ANIM_DONE, true );
		}
	}


void Actor::PickupEnt( Event *ev	)
	{
	Event *e;

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_PickupEnt );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}

	e->AddEntity( ev->GetEntity( 1 ) );
	e->AddString( ev->GetString( 2 ) );

	SetBehavior( new PickupEntity, e, ev->GetThread() );
	}


void Actor::PickupThrowObject( Event *ev )
	{
	
	Entity *ent;
	str bone;

	ent = (Entity*)enemyManager->GetAlternateTarget();
	if (!ent->isSubclassOf(ThrowObject) )
		return;
	
	ThrowObject* tobj = 0;
	tobj = (ThrowObject*)ent;

	if (!tobj) return;
	
	bone = ev->GetString( 1 );
	tobj->Pickup(this , bone );
	haveThrowObject = true;
	
	}


void Actor::PostureAnimDone(Event *ev)
{
    SetActorFlag(ACTOR_FLAG_POSTURE_ANIM_DONE, true);
}


void Actor::PrepareToFailMission( Event *ev )
{
	float time; 
	str reason;

	time = ev->GetFloat( 1 );
	reason = "DefaultFailure";
	if ( ev->NumArgs() > 1 )
		reason = ev->GetString( 2 );

	Event *failureEvent;
	failureEvent = new Event(EV_Actor_FailMission);
	failureEvent->AddString( reason );

	PostEvent( failureEvent, time );
}


void Actor::PrintBehaviorPackageInfo()
{
	gi.Printf( "\nBehaviorPackage Information:"                                                 );
	gi.Printf( "\nCurrent Behavior Package Name: %s"  , packageManager->GetCurrentPackageName().c_str() );

	if ( currentState )
		gi.Printf( "\nCurrent Behavior Package State: %s" , currentState->getName()                 );
	else
		gi.Printf( "\nCurrent Behavior Package State: !!!!!NULL!!!!!");

	gi.Printf( "\nLast Behavior Package State: %s"    , lastState->getName()                    );
	gi.Printf( " \n" );	
	gi.Printf( " \n" );	
}


void Actor::PrintDebugMessage( Event *ev )
{
	str msg = ev->GetString( 1 );
	gi.WDPrintf( "\n--------------------------------------------------------------\n" );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
	gi.WDPrintf( msg + "\n" );
#pragma GCC diagnostic pop
	gi.WDPrintf( "--------------------------------------------------------------\n" );
		
}


void Actor::PrintMasterStateInfo()
{
	gi.Printf( "\nMasterState Information:"                                      );
	gi.Printf( "\nMasterState File: %s"          , masterstatemap->Filename()    );
	gi.Printf( "\nMasterState Current State: %s" , currentMasterState->getName() );
	gi.Printf( "\nMasterState Last State: %s"    , lastMasterState->getName()    );
	gi.Printf( " \n" );	
	gi.Printf( " \n" );	

}


void Actor::PrintStateMapInfo()
{
	gi.Printf( "\nState Map Information: "                      );
	gi.Printf( "\nState Map File: %s" , statemap->Filename()    );
	gi.Printf( "\nCurrent State: %s"  , currentState->getName() );
	gi.Printf( "\nLast State: %s"     , lastState->getName()    );
	gi.Printf( " \n" );	
	gi.Printf( " \n" );	
}


void Actor::ProcessActorStateMachine( void )
{
	int count;
	str behavior_arg;
	str behavior_name;
	str headBehavior_name;
	str eyeBehavior_name;
	str torsoBehavior_name;
	str currentTorsoAnim;
	str currentanim;
	str stateLegAnim;
	str stateTorsoAnim;
	ClassDef *cls = NULL;
	State *laststate = NULL;

	stateLegAnim = animname;
	stateTorsoAnim = TorsoAnimName;

	count = 0;

	if ( deadflag || !currentState )
		return;

	if ( ( mode != ACTOR_MODE_AI ) && ( mode != ACTOR_MODE_IDLE ) )
		return;
	
	do
		{
		count++;
		if ( count > 10 )
			{
			gi.WDPrintf( "Possible infinite loop in state '%s'\n", currentState->getName() );
			if ( count > 50 )
				{
				gi.Error( ERR_DROP, "Stopping due to possible infinite state loop\n" );
				break;
				}
			}

		laststate = currentState;


		if ( !currentState->IgnoreGlobalStates() && globalState )
			{
			currentState = globalState->Evaluate( *this , &conditionals );

			if ( currentState == globalState )
				{
				currentState = laststate;
				}
			else
				{
				ClearStateFlags();

				laststate->ProcessExitCommands( this );
				currentState->ProcessEntryCommands( this );
				}
			}
		
		currentState = currentState->Evaluate( *this, &conditionals );

		if ( !currentState )
			return;

		if ( laststate != currentState )
			{
			_checkedChance = false;

			laststate->ProcessExitCommands( this );
			lastState = laststate;

			currentState->ProcessEntryCommands( this );


			if ( behavior )
				{
				EndBehavior();
				}

			if ( torsoBehavior )
			   {
				EndTorsoBehavior();
			   }

			behavior_name = currentState->getBehaviorName();
			
			if ( !behavior_name.length() )
				{
				GameplayManager *gpm = GameplayManager::getTheGameplayManager();
				str stateName = currentState->getName();
				str objname = getArchetype();

				stateName = objname + "." + stateName;
				if ( gpm->hasObject( stateName ) )
					{
					if ( gpm->hasProperty( stateName , "behavior" ) )
						{
						behavior_name = gpm->getStringValue( stateName , "behavior" );
						}
					}
	
				}

			if ( behavior_name.length() )
				{
				cls = getClass( currentState->getBehaviorName() );
				}
            

			if ( cls )
				{
				if ( currentState->numBehaviorParms() )
					{
					int i;
					Event *e = new Event( EV_Behavior_Args );

					for ( i = 1 ; i <= currentState->numBehaviorParms() ; i++ )
						e->AddString( currentState->getBehaviorParm( i ) );

					SetBehavior( ( Behavior * )cls->newInstance(), e );
					}
				else
					{
					SetBehavior( ( Behavior * )cls->newInstance() );
					}
				}
			else if ( behavior_name.length() )
				{
				gi.WDPrintf( "Invalid behavior name %s\n", behavior_name.c_str() );
				}


			torsoBehavior_name = currentState->getTorsoBehaviorName();
			cls = 0;

			if ( torsoBehavior_name.length() )
				cls = getClass( currentState->getTorsoBehaviorName() );

			if ( cls )
				{
				if ( currentState->numTorsoBehaviorParms() )
					{
					int i;
					Event *e = new Event( EV_Behavior_Args );

					for ( i = 1 ; i <= currentState->numTorsoBehaviorParms() ; i++ )
						e->AddString( currentState->getTorsoBehaviorParm( i ) );

					SetTorsoBehavior( ( Behavior * )cls->newInstance(), e );
					}
				else
					{
					SetTorsoBehavior( ( Behavior * )cls->newInstance() );
					}
					
				}
			else if ( torsoBehavior_name.length() )
				{
				gi.WDPrintf( "Invalid torso behavior name %s\n", torsoBehavior_name.c_str() );
				}



			InitState();
			}

		behavior_name = currentState->getBehaviorName();
		if ( !behavior_name.length() )
			{
			GameplayManager *gpm = GameplayManager::getTheGameplayManager();
			str stateName = currentState->getName();
			str objname = getArchetype();

			stateName = objname + "." + stateName;
			if ( gpm->hasObject( stateName ) )
				{
				if ( gpm->hasProperty( stateName , "behavior" ) )
					{
					behavior_name = gpm->getStringValue( stateName , "behavior" );
					}
				}
			}

		if ( behavior && !behavior_name.length() )
			{
			EndBehavior();
			}
            


		currentanim = currentState->getLegAnim( *this, &conditionals );
		currentTorsoAnim = currentState->getTorsoAnim( *this, &conditionals );		

		if ( deadflag ) return;

		if ( !GetActorFlag(ACTOR_FLAG_PLAYING_DIALOG_ANIM) )
			{
			if ( currentanim.length() && ( strcmp( stateLegAnim , currentanim.c_str() ) != 0 ) )
				{
				SetAnim( currentanim, EV_Anim_Done, legs );
				stateLegAnim = currentanim;
				}

			if ( currentTorsoAnim.length() && ( strcmp( stateTorsoAnim , currentTorsoAnim.c_str() ) != 0 ) )
				{
				SetAnim( currentTorsoAnim, EV_Torso_Anim_Done, torso );
				stateTorsoAnim = currentTorsoAnim;
				}
			}
			
		if ( showStates != DEBUG_NONE && laststate != currentState )
			_printDebugInfo(laststate->getName() , currentState->getName() , currentanim , currentTorsoAnim );

      }
 	while( laststate != currentState );
}


void Actor::ProcessMasterStateMachine( void )
	{
	int count;
	State	*laststate = NULL;


	if ( !masterstatemap )
		return;

	if ( deadflag || !currentMasterState )
		return;

	if ( ( mode != ACTOR_MODE_AI ) && ( mode != ACTOR_MODE_IDLE ) )
		return;
	

	count = 0;
	do
		{

		count++;
		if ( count > 10 )
			{
			gi.WDPrintf( "Possible infinite loop in Master State '%s'\n", currentMasterState->getName() );
			if ( count > 50 )
				{
				gi.Error( ERR_DROP, "Stopping due to possible infinite state loop\n" );
				break;
				}
			}

		laststate = currentMasterState;

		
		currentMasterState = currentMasterState->Evaluate( *this, &master_conditionals );			
		if ( !currentMasterState )
			return;
			

		if ( laststate != currentMasterState )
			{
			laststate->ProcessExitCommands( this );
			lastMasterState = laststate;

			currentMasterState->ProcessEntryCommands( this );
			InitMasterState();
			}
		
		} while( laststate != currentMasterState );

	}


void Actor::ProjectileClose( Event *ev )
{
	Entity *owner;
	owner = ev->GetEntity( 1 );
	if ( !owner ) return;

	if ( enemyManager->Hates( owner ) )
		{
		AddStateFlag(STATE_FLAG_ENEMY_PROJECTILE_CLOSE);
		}
}


void Actor::PutawayWeapon( Event *ev )
{
	weaponhand_t hand = WEAPON_RIGHT;
	if ( ev->NumArgs() > 0 )
		hand = WeaponHandNameToNum(ev->GetString( 1 ));

	Sentient::DeactivateWeapon(hand);
}


void Actor::RealSpawnGib( qboolean use_tag, Event *ev	)
	{
	str tag_name;
	Gib *gib;
	Entity *ent = NULL;
	Vector final_gib_offset;
	Vector orig;
	Vector dir;
	int current_arg;
	float final_pitch;
	float vel;
	trace_t trace;
	float time;
	float pitch_change;
	float pitch_vel;
	str attach_tag_name;
	Vector offset;
	str cap_name;
	float width;
	const char *current_arg_str;
	int current_surface;
	const char *current_surface_name;
	qboolean use_blood;
	str blood_name;
	str blood_splat_name;
	str blood_model_name;
	float blood_splat_size;
	Vector gib_mins;
	Vector gib_maxs;
	float m;
	qboolean at_least_one_visible_surface = false;
	int surface_length;
	int tagnum;
	orientation_t orn;
	float raw_offset;
	Vector raw_offset_dir;
	Vector real_tag_pos;
	Vector real_tag_dir;
	Vector real_tag_angles;

	SetActorFlag( ACTOR_FLAG_SPAWN_FAILED, true );

	if ( !com_blood->integer )
		return;

	if ( GetActorFlag( ACTOR_FLAG_FADING_OUT ) )
		return;

	if ( ev->NumArgs() < 5 )
		return;

	if ( use_tag )
		{
		attach_tag_name = ev->GetString( 1 );
		raw_offset      = ev->GetFloat( 2 );
		width           = ev->GetFloat( 3 );


		tagnum = gi.Tag_NumForName( edict->s.modelindex, attach_tag_name.c_str() );

		if ( tagnum == -1 )
			return;

		GetRawTag( tagnum, &orn );
		GetTag( tagnum, &real_tag_pos, &real_tag_dir );

		real_tag_angles = real_tag_dir.toAngles();


		final_pitch = AngleNormalize180( angles[PITCH] - real_tag_angles[PITCH] );


		raw_offset_dir = orn.axis[0];

		offset = orn.origin;
		offset += raw_offset * raw_offset_dir;
		MatrixTransformVector( offset, orientation, orig );
		orig += origin;
		}
	else
		{
		offset      = ev->GetVector( 1 );
		final_pitch = ev->GetFloat( 2 );
		width       = ev->GetFloat( 3 );

		MatrixTransformVector( offset, orientation, orig );
		orig += origin;
		}


	m = mass;

	if ( m < 50.0f )
		m = 50.0f;
	else if ( m > 250.0f )
		m = 250.0f;


	blood_name       = GetBloodSpurtName();
	blood_splat_name = GetBloodSplatName();
	blood_splat_size = GetBloodSplatSize();

	if ( blood_name.length() && blood_splat_name.length() )
		use_blood = true;
	else
		use_blood = false;

	if ( GetActorFlag( ACTOR_FLAG_BLEED_AFTER_DEATH ) )
		blood_model_name = blood_model;


	gib_mins = Vector( -width, -width, -width );
	gib_maxs = Vector(  width,  width,  width );


	trace = G_Trace( orig, gib_mins, gib_maxs, orig, NULL, MASK_DEADSOLID, false, "spawngib" );

	if ( trace.allsolid || trace.startsolid )
		return;


	for( current_arg = 5 ; current_arg <= ev->NumArgs() ; current_arg++ )
		{
		current_arg_str = ev->GetString( current_arg );

		for( current_surface = 0 ; current_surface < numsurfaces ; current_surface++ )
			{
			current_surface_name = gi.Surface_NumToName( edict->s.modelindex, current_surface );

			if ( current_arg_str[ strlen( current_arg_str ) - 1 ] == '*' )
				surface_length = strlen( current_arg_str ) - 1;
			else
				surface_length = strlen( current_arg_str );

			if ( Q_stricmpn( current_surface_name, current_arg_str, surface_length ) == 0 )
				{
				if ( !( edict->s.surfaces[ current_surface ] & MDL_SURFACE_NODRAW ) )
					at_least_one_visible_surface = true;
				}
			}
		}

	if ( !at_least_one_visible_surface )
		return;


	vel = 100.0f + G_Random( 200.0f * ( 2.0f - ( ( m - 50.0f ) / 200.0f ) ) );

	time = SpawnGetTime( vel , orig, gib_mins, gib_maxs );


	if ( G_Random() > .5f )
		{
		final_pitch += 180.0f;
		final_pitch = AngleNormalize360( final_pitch );
		}


	pitch_change = AngleNormalize180( final_pitch - angles[PITCH] );
	pitch_vel = pitch_change / time;


	gib = new Gib( "", use_blood, blood_name, blood_model_name, blood_splat_name, blood_splat_size, final_pitch );

	gib->setOrigin( orig );

	gib->angles[PITCH] = angles[PITCH];
	gib->angles[ROLL]  = 0;

	if ( use_tag )
		gib->angles[YAW]   = real_tag_angles[YAW];
	else
		gib->angles[YAW]   = angles[YAW];

	gib->setAngles( gib->angles );

	gib->velocity  = Vector( G_CRandom( 150.0f * ( 2.0f - ( ( m - 50.0f ) / 200.0f ) ) ), 
			G_CRandom( 150.0f * ( 2.0f - ( ( m - 50.0f ) / 200.0f ) ) ), vel );
	gib->avelocity = Vector( pitch_vel, G_CRandom( 300.0f ), 0.0f );

	gib->setSize( gib_mins, gib_maxs );

	gib->edict->svflags |= SVF_DEADMONSTER;
	gib->edict->clipmask = MASK_DEADSOLID;

	gib->setSolidType( SOLID_BBOX );
	gib->setContents( CONTENTS_SHOOTABLE_ONLY );
	gib->link();


	ent = new Entity( ENTITY_CREATE_FLAG_ANIMATE );
	ent->setModel( model );


	ent->CancelPendingEvents();


	ent->edict->s.eFlags |= EF_DONT_PROCESS_COMMANDS;

	ent->animate->RandomAnimate( animate->AnimName() );
	ent->animate->SetFrame( animate->CurrentFrame(legs) );

	ent->setAngles( angles );
	ent->bind( gib, true );
	ent->gravity = 0;

	final_gib_offset = offset * -1.0f;
	ent->setOrigin( final_gib_offset );

	ent->setMoveType( MOVETYPE_BOUNCE );
	ent->setSolidType( SOLID_NOT );
	ent->showModel();


	ent->SurfaceCommand( "all", "+nodraw" );

	cap_name = ev->GetString( 4 );


	if ( cap_name.length() )
		{
		ent->SurfaceCommand( cap_name.c_str(), "-nodraw" );

		SurfaceCommand( cap_name.c_str(), "-nodraw" );
		}


	for ( current_arg = 5 ; current_arg <= ev->NumArgs() ; current_arg++ )
		{
		current_arg_str = ev->GetString( current_arg );

		if ( current_arg_str[ strlen( current_arg_str ) - 1 ] == '*' )
			surface_length = strlen( current_arg_str ) - 1;
		else
			surface_length = strlen( current_arg_str );

		for( current_surface = 0 ; current_surface < numsurfaces ; current_surface++ )
			{
			current_surface_name = gi.Surface_NumToName( edict->s.modelindex, current_surface );

			if ( Q_stricmpn( current_surface_name, current_arg_str, surface_length ) == 0 )
				{
				if ( !( edict->s.surfaces[ current_surface ] & MDL_SURFACE_NODRAW ) )
					{
					ent->SurfaceCommand( current_surface_name, "-nodraw" );

					SurfaceCommand( current_surface_name, "+nodraw" );
					}
				}
			}
		}


	if ( GetActorFlag( ACTOR_FLAG_DEATHFADE ) )
		{
		ent->PostEvent( EV_Fade, 10.0f );
		gib->PostEvent( EV_Fade, 10.5f );
		}
	else if ( GetActorFlag( ACTOR_FLAG_DEATHSHRINK ) )
		{
		ent->PostEvent( EV_FadeOut, 10.0f );
		gib->PostEvent( EV_FadeOut, 10.5f );
		}
	else
		{
		ent->PostEvent( EV_Unbind, 10.0f );
		ent->PostEvent( EV_DeathSinkStart, 12.0f );
		gib->PostEvent( EV_Remove, 10.5f );
		}


	SetActorFlag( ACTOR_FLAG_SPAWN_FAILED, false );


	ent->Sound( "impact_sever", CHAN_BODY );
	}


void Actor::RegisterBehaviorPackage( Event *ev )
	{
	packageManager->RegisterPackage(ev->GetString( 1 ) );
	}


void Actor::RemoveActorWeapon( Event *ev )
	{
	takeItem( ev->GetString( 1 ) );
	}


void Actor::RemoveUselessBody( Event *ev )
   {

   PostEvent( EV_FadeOut, 5.0f );
   }


void Actor::RepostEvent( Event *ev,	const Event &event_type )
	{
	Event *event;
	int i;
	str token;

	event = new Event( event_type );

	for( i = 1 ; i <= ev->NumArgs() ; i++ )
		{
		token = ev->GetString( i );
		event->AddString( token.c_str() );
		}

	event->SetThread( ev->GetThread() );

	PostEvent( event, FRAMETIME );
	}


void Actor::ResetEyeEvent(	Event *ev )
	{
	Event *event;
	
	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_ResetEye );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );
	
	event = new Event( EV_Behavior_Args );
	event->AddEntity( NULL );

	if ( ev->NumArgs() > 0 )
		event->AddFloat( ev->GetFloat( 1 ) );

	SetEyeBehavior( new EyeWatch, event, ev->GetThread() );
	}


void Actor::ResetHeadEvent ( Event *ev	)
	{
	
	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_ResetHead );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );
	
	headWatcher->SetWatchTarget( NULL );
	}


void Actor::ResetTorsoEvent( Event *ev	)
	{
   SetControllerTag( ACTOR_TORSO_TAG, gi.Tag_NumForName( edict->s.modelindex, "Bip01 Spine1" ) );
   SetControllerAngles( ACTOR_TORSO_TAG, vec_zero );
	}


void Actor::RestoreMode( void )
	{
	Event *idle_event;

	if ( saved_mode == ACTOR_MODE_IDLE )
		{
		mode = ACTOR_MODE_IDLE;

		idle_event = new Event( EV_Actor_Idle );
		idle_event->AddString( saved_state_name );
		ProcessEvent( idle_event );		
		}
	if ( saved_mode == ACTOR_MODE_AI )
		{
		mode = ACTOR_MODE_AI;

		SetState( saved_state_name.c_str() );
		}
	else if ( saved_mode == ACTOR_MODE_SCRIPT )
		{
		StartMode( ACTOR_MODE_SCRIPT );

		behavior     = saved_behavior;
		headBehavior = saved_headBehavior;
		eyeBehavior  = saved_eyeBehavior;
		torsoBehavior = saved_torsoBehavior;
		scriptthread = saved_scriptthread;
		
		if ( saved_behavior )
			currentBehavior = saved_behavior->getClassname();
		else
			currentBehavior = "";

		if ( saved_torsoBehavior )
			currentTorsoBehavior = saved_torsoBehavior->getClassname();
		else
			currentTorsoBehavior = "";

		if ( saved_anim_event_name.length() )
			{
			Event *event = new Event( saved_anim_event_name.c_str() );
			SetAnim( saved_anim_name, event );
			}
		else
			SetAnim( saved_anim_name );
		}
	else
		{
		gi.WDPrintf( "Can't restore specified mode: %d\n", saved_mode );
		}

	saved_mode = ACTOR_MODE_NONE;
	}


void Actor::RunCustomThread( const str& threadType )
{

	threadlist_t* threadListEntry;
	str	threadName;

	if ( threadList.NumObjects() )
		{
		for ( int i = 1; i <= threadList.NumObjects() ; i++ )
			{
			threadListEntry = threadList.ObjectAt( i );
			if ( !stricmp(threadListEntry->threadType.c_str() , threadType.c_str() ) )
				{
				threadName = threadListEntry->threadName;
				if ( threadName.length() )
					{
					ExecuteThread(threadName,true,this);
					}
				}
			}
		}
}



void Actor::SaveMode( void )
	{
	if ( mode == ACTOR_MODE_IDLE )
		{
		saved_mode       = ACTOR_MODE_IDLE;

		if ( currentState )
			saved_state_name = currentState->getName();
		else
			saved_state_name = "";
		}
	else if ( mode == ACTOR_MODE_AI )
		{
		saved_mode = ACTOR_MODE_AI;

		if ( currentState )
			saved_state_name = currentState->getName();
		else
			saved_state_name = "";
			
		}
	else if ( mode == ACTOR_MODE_SCRIPT )
		{
		saved_mode            = ACTOR_MODE_SCRIPT;
		saved_behavior        = behavior;
		saved_headBehavior    = headBehavior;
		saved_eyeBehavior     = eyeBehavior;
		saved_torsoBehavior   = torsoBehavior;
		saved_scriptthread    = scriptthread;		
		saved_anim_name       = animname;
		saved_anim_event_name = last_anim_event_name;

		if(headBehavior)
			headBehavior->End(*this);
		
		if(eyeBehavior)
			eyeBehavior->End(*this);

		if(torsoBehavior)
			torsoBehavior->End(*this);

		behavior      = NULL;
		headBehavior  = NULL;
		eyeBehavior   = NULL;
		torsoBehavior = NULL;
		scriptthread  = NULL;		
		}
	else
		{
		gi.WDPrintf( "Can't saved specified mode: %d\n", mode );
		}
	}


void Actor::SaveOffLastHitBone( Event *ev )
{
	saved_bone_hit = last_bone_hit;
}


void Actor::SelectClosestEnemy( Event *ev )
{
	enemyManager->FindClosestEnemy();
}


void Actor::SelectNextEnemy( Event* ev )
{
	enemyManager->FindNextEnemy();
}


void Actor::SendEventToGroup( Event *ev )
{
	int parmCount = ev->NumArgs();
	str eventToSend = ev->GetString( 1 );

	Event *event = new Event( eventToSend.c_str() );

	for ( int i = 2 ; i <= parmCount ; i++ )
		{
		event->AddToken( ev->GetToken( i ) );
		}

	groupcoordinator->SendEventToGroup(event, GetGroupID());
}


void Actor::SendMoveDone( CThread *script_thread )
	{
	Event *event;

	if ( script_thread )
		{
		event = new Event( EV_MoveDone );
		event->AddEntity( this );
		script_thread->PostEvent( event, FRAMETIME );
      }
	}


void Actor::SetAbsoluteMax( Event *ev )
	{
	absoluteMax = ev->GetFloat( 1 );
	}


void Actor::SetAbsoluteMin( Event *ev )
	{
	absoluteMin = ev->GetFloat( 1 );
	}


void Actor::SetActivateThread ( Event *ev	)
	{
   activate_thread = ev->GetString( 1 );
	}


void Actor::SetActivationDelay( Event *ev )
{
	SetActivationDelay(ev->GetFloat(1) );
}


void Actor::SetActivationStart( Event *ev )
{
	SetActivationStart();
}


void Actor::SetActorType( Event *ev )
	{
	str aType;
	aType = ev->GetString( 1 );

	if ( !Q_stricmp( aType.c_str() , "inanimate" ) )
		{
		actortype = IS_INANIMATE;	
		edict->svflags	&= ~SVF_MONSTER;
      setMoveType( MOVETYPE_STATIONARY );
      flags &= ~FL_BLOOD;
		flags &= ~FL_DIE_GIBS;
		return;
		}
	else if ( !Q_stricmp( aType.c_str() , "monster" ) )
		{
		actortype = IS_MONSTER;
		edict->s.eFlags |= EF_ENEMY;

		level.enemySpawned( this );
		}
	else if ( !Q_stricmp( aType.c_str() , "enemy" ) )
		{
		actortype = IS_ENEMY;
		edict->s.eFlags |= EF_ENEMY;

		level.enemySpawned( this );
		}
	else if ( !Q_stricmp( aType.c_str() , "civilian" ) )
		{
		actortype = IS_CIVILIAN;
		edict->s.eFlags |= EF_FRIEND;
		}
	else if ( !Q_stricmp( aType.c_str() , "friend" ) )
		{
		actortype = IS_FRIEND;
		edict->s.eFlags |= EF_FRIEND;
		}
	else if ( !Q_stricmp( aType.c_str() , "animal" ) )
		{
		actortype = IS_ANIMAL;
		edict->s.eFlags |= EF_FRIEND;
		}
	else if ( !Q_stricmp( aType.c_str() , "teammate" ) )
		{
      TeamMateList.AddUniqueObject(this);
      actortype = IS_TEAMMATE;

      max_inactive_time = -1.0f;

		edict->s.eFlags |= EF_FRIEND;
      edict->clipmask = MASK_MONSTERSOLID | MASK_PLAYERSOLID;
		}

	}


void Actor::SetAimLeadFactors( Event *ev )
	{
	minLeadFactor = ev->GetFloat( 1 );
	maxLeadFactor = ev->GetFloat( 2 );
	}


void Actor::SetAlertThread ( Event *ev	)
	{
	alert_thread = ev->GetString( 1 );
	}


const str& Actor::GetAnimSet(void) 
{
    return animset;
}

void Actor::SetAnimSet( Event *ev )
{
	SetAnimSet(ev->GetString( 1 ) );
}




void Actor::SetBehavior ( Behavior *newbehavior, Event *startevent, CThread *newthread	)
	{
	if ( ( deadflag ) && ( actortype != IS_INANIMATE ) )
		{

		if ( newbehavior )
			delete newbehavior;
		if ( startevent )
			delete startevent;

		return;
		}

	if ( scriptthread == newthread )
		{
		scriptthread = NULL;
		}
	EndBehavior();

	behavior = newbehavior;
   behaviorCode = BEHAVIOR_EVALUATING;
	if ( behavior )
		{
		Wakeup();

		if ( startevent )
			{
			behavior->ProcessEvent( startevent );
			}
		currentBehavior = behavior->getClassname();
		if ( _controller ) behavior->SetController( _controller );
		behavior->SetSelf( this );
		behavior->Begin( *this );
		scriptthread = newthread;
		}
	}


void Actor::SetBehaviorPackage( Event *ev )
{
	SetBehaviorPackage(ev->GetString( 1 ) );	
}


void Actor::SetBleedAfterDeath( Event *ev	)
	{
	SetActorFlag( ACTOR_FLAG_BLEED_AFTER_DEATH, ev->GetBoolean( 1 ) );
	}


void Actor::SetBounceOffVelocity( Event *ev )
{
	bounce_off_velocity = ev->GetFloat( 1 );
}


void Actor::SetCheckConeOfFireDistance( Event* ev )
{
	SetCheckConeOfFireDistance( ev->GetFloat( 1 ) );
}


void Actor::SetCinematicAnim(const str &animName)
	{
	Entity::SetCinematicAnim( animName);						
	movementSubsystem->setMovementType( MOVEMENT_TYPE_ANIM );
	}


void Actor::SetCombatTraceInterval( Event *ev )
{
	combatSubsystem->SetTraceInterval( ev->GetFloat( 1 ) );
}


void Actor::SetContextInterval( Event *ev )
{
	SetContextInterval(ev->GetFloat(1));
}


void Actor::SetCrippled( Event *ev )
   {
   qboolean crippled = ev->GetBoolean( 1 );

   SetActorFlag(ACTOR_FLAG_CRIPPLED , crippled );
   }


void Actor::SetDamageAngles ( Event *ev )
   {
	damage_angles = ev->GetFloat( 1 );
	}


void Actor::SetDeathKnockbackValues(Event *ev)
{
	deathKnockbackVerticalValue = ev->GetFloat( 1 );
	deathKnockbackHorizontalValue = ev->GetFloat( 2 );
}


void Actor::SetDeathSize( Event *ev	)
	{
   Vector death_min;
	Vector death_max;
	trace_t trace;

	death_min = ev->GetVector( 1 );
	death_max = ev->GetVector( 2 );


	trace = G_Trace( origin, death_min, death_max, origin, this, edict->clipmask, false, "Actor::SetDeathSize" );

	if ( !trace.startsolid )
		{
		setSize( death_min, death_max );
		return;
		}


	death_min = (death_min + mins) * .5f;
	death_max = (death_max + maxs) * .5f;

	trace = G_Trace( origin, death_min, death_max, origin, this, edict->clipmask, false, "Actor::SetDeathSize2" );

	if ( !trace.startsolid )
		setSize( death_min, death_max );
	}


void Actor::SetDieCompletely(	Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_DIE_COMPLETELY, ev->GetBoolean( 1 ) );
	}


void Actor::SetDisabled( Event *ev )
	{
	qboolean disabled = ev->GetBoolean( 1 );
	
	SetActorFlag(ACTOR_FLAG_DISABLED , disabled );
	}


void Actor::SetEnemyAttached( Event *ev )
	{
   haveAttached = ev->GetBoolean( 1 );
	}


void Actor::SetEnemyTargeted( Event *ev )
{
	SetEnemyTargeted(ev->GetBoolean(1) );
}


void Actor::SetEnemyType (	Event *ev )
	{
	enemytype = ev->GetString( 1 );
	}


void Actor::SetEyeBehavior ( Behavior *newEyeBehavior, Event *startevent, CThread *newthread	)   
	{
	if ( ( deadflag ) && ( actortype != IS_INANIMATE ) )
		{

		if ( newEyeBehavior )
			delete newEyeBehavior;
		if ( startevent )
			delete startevent;

		return;
		}

	if ( scriptthread == newthread )
		{
		scriptthread = NULL;
		}
	EndEyeBehavior();

	eyeBehavior = newEyeBehavior;
	if ( eyeBehavior )
		{
		Wakeup();

		if ( startevent )
			{
			eyeBehavior->ProcessEvent( startevent );
			}
		currentEyeBehavior = eyeBehavior->getClassname();
		eyeBehavior->Begin( *this );
		scriptthread = newthread;
		}
	}


void Actor::SetFOV( Event *ev )
	{
	sensoryPerception->SetFOV( ev->GetFloat( 1 ) );	
	}


void Actor::SetFlagOnEnemy( Event *ev )
	{
	str flag_name;
	int flag;
	qboolean flag_bool;
	Actor* act = 0;

	flag_name = ev->GetString(1);
	flag_bool = ev->GetBoolean(2);

	flag = ActorFlag_string_to_int( flag_name );
	
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	if ( !currentEnemy->isSubclassOf( Actor ) )
		return;

	act = (Actor*)(Entity*)currentEnemy;
	
	if ( act )
		act->SetActorFlag( flag, flag_bool );	
	}


void Actor::SetFollowCombatRange( Event *ev )
{
	followTarget.maxRangeCombat = ev->GetFloat( 1 );
}


void Actor::SetFollowCombatRangeMin( Event *ev )
{
	followTarget.minRangeCombat = ev->GetFloat( 1 );
}


void Actor::SetFollowRange( Event *ev )
   {
   followTarget.maxRangeIdle = ev->GetFloat( 1 );
   }


void Actor::SetFollowRangeMin( Event *ev )
	{
	followTarget.minRangeIdle = ev->GetFloat( 1 );
	}


void Actor::SetFollowTarget( Event *ev )
   {
   Entity *ent;
   ent = NULL;

   ent = ev->GetEntity( 1 );

   if ( ent )
      followTarget.specifiedFollowTarget = ent;
   }


void Actor::SetFuzzyEngineActive( Event *ev )
   {
   qboolean active = ev->GetBoolean( 1 );
   fuzzyEngine_active = active;
   }


void Actor::SetGlobalState( const char *state_name )
	{
	if ( !statemap )
		return;

	if ( deadflag )
		return;

	if ( globalState )
		globalState->ProcessExitCommands( this );

   globalState = statemap->FindGlobalState( state_name );
	}


void Actor::SetGroupDeathThread( Event *ev )
{
	groupcoordinator->SetGroupDeathThread( ev->GetString( 1 ) , GetGroupID() );
}


void Actor::SetHeadBehavior( Behavior *newHeadBehavior, Event *startevent, CThread *newthread )
	{
	if ( ( deadflag ) && ( actortype != IS_INANIMATE ) )
		{

		if ( newHeadBehavior )
			delete newHeadBehavior;
		if ( startevent )
			delete startevent;

		return;
		}

	if ( scriptthread == newthread )
		{
		scriptthread = NULL;
		}

	if ( headBehavior )
		{
		currentHeadBehavior = headBehavior->getClassname();
		str newHeadBehaviorName = newHeadBehavior->getClassname();

		if ( currentHeadBehavior == newHeadBehaviorName )
			{
			if ( startevent )
				{
				headBehavior->ProcessEvent( startevent );
				}

			return;
			}
		}


	EndHeadBehavior();

	headBehavior = newHeadBehavior;
	if ( headBehavior )
		{
		Wakeup();

		if ( startevent )
			{
			headBehavior->ProcessEvent( startevent );
			}
		currentHeadBehavior = headBehavior->getClassname();
		headBehavior->Begin( *this );
		scriptthread = newthread;
		}
	}


void Actor::SetHeadWatchMaxDistance( Event *ev )
{
	headWatcher->SetMaxDistance( ev->GetFloat( 1 ) );
}


void Actor::SetHeadWatchSpeed( Event *ev )
   {
   float speed = ev->GetFloat( 1 );
   headWatcher->SetWatchSpeed( speed );
   }








void Actor::SetHeadWatchTarget( Entity *ent ) 
   {
   if ( !ent )
      ent = NULL;

   headWatcher->SetWatchTarget( ent );
   }

void Actor::SetHeadWatchTarget( Event *ev )
   {
   str watchTarget;
   float speed;

   watchTarget = ev->GetString( 1 );
   if ( ev->NumArgs() > 1 )
      {
      speed = ev->GetFloat( 2 );
      headWatcher->SetWatchSpeed( speed );
      }

   
   if ( !Q_stricmp( "enemy" , watchTarget.c_str() ) )
      {
      headWatcher->SetWatchTarget( enemyManager->GetCurrentEnemy() );
      return;
      }

   if ( !Q_stricmp( "none" , watchTarget.c_str() ) )
      {
      headWatcher->SetWatchTarget( NULL );
      return;
      }
   
   if ( !Q_stricmp( "player" , watchTarget.c_str() ) )
		{
		Player *player = GetPlayer(0);
		headWatcher->SetWatchTarget( player );
		}

   if ( !Q_stricmp( "teammate" , watchTarget.c_str() ) )
		{
		float bestDist = 99999;
		Vector selfToTeammate;
		Sentient *teammate = GetPlayer(0);
		Sentient *closestTeammate = NULL;

		for ( int i = 1 ; i <= TeamMateList.NumObjects() ; i++ )
			{
			teammate = TeamMateList.ObjectAt( i );
			if ( teammate->entnum == entnum)
				continue;

			selfToTeammate = teammate->origin - origin;
			if ( selfToTeammate.length() <= bestDist )
				{
				closestTeammate = teammate;
				bestDist = selfToTeammate.length();
				}
		
			}

		if ( !closestTeammate )
			closestTeammate = teammate;

		headWatcher->SetWatchTarget( closestTeammate );
		return;
		}
		

   headWatcher->SetWatchTarget( watchTarget );
   
   }


void Actor::SetHitscanResponse( Event *ev )
	{
	hitscan_response_chance = ev->GetFloat ( 1 );

	if (hitscan_response_chance > 1.0f )
		hitscan_response_chance = 1.0f;

	if (hitscan_response_chance < 0.0f )
		hitscan_response_chance = 0.0f;
	}


void Actor::SetIdleStateName( Event *ev )
	{
	idle_state_name = ev->GetString( 1 );
	}


void Actor::SetIgnoreNextContext( Event *ev )
{
	_ignoreNextContext = ev->GetBoolean( 1 );
	_nextContextToIgnore = ev->GetString( 2 );
}


void Actor::SetIgnoreWatchTarget( bool ignore )
{
	headWatcher->SetIgnoreWatchTarget( ignore );
}


void Actor::SetImmortal ( Event *ev )
   {
	qboolean isImmortal = true;

	if ( ev->NumArgs() > 0 )
		isImmortal = ev->GetBoolean( 1 );
	
	SetActorFlag( ACTOR_FLAG_IMMORTAL, isImmortal );
	}


void Actor::SetInAlcove( Event *ev )
	{
	qboolean inalcove = ev->GetBoolean( 1 );

	SetActorFlag( ACTOR_FLAG_IN_ALCOVE , inalcove );
	}


void Actor::SetKillThreadEvent( Event *ev )
   {
   kill_thread = ev->GetString( 1 );
   }


void Actor::SetMasterState( Event *ev )
{
	SetMasterState(ev->GetString( 1 ) );
}

void Actor::SetMasterState( const str &state_name )
	{
	if ( !masterstatemap )
		return;

	if ( deadflag )
		return;

	if ( currentMasterState )
		currentMasterState->ProcessExitCommands( this );

	currentMasterState = masterstatemap->FindState( state_name );
	masterstate_time = level.time;

	if ( currentMasterState  )
		{
		InitMasterState();
		currentMasterState->ProcessEntryCommands( this );
		}
	else
		{
		gi.WDPrintf( "Master State %s not found\n", state_name.c_str() );
		}
	}


void Actor::SetMaxHeadPitch( Event *ev )
{
	headWatcher->SetMaxHeadPitch( ev->GetFloat( 1 ) );
}


void Actor::SetMaxHeadYaw( Event *ev )
{
	headWatcher->SetMaxHeadYaw( ev->GetFloat( 1 ) );
}





void Actor::SetMinimumMeleeHeight ( Event *ev )
   {
	minimum_melee_height = ev->GetFloat( 1 );
	}


void Actor::SetNodeID( Event *ev )
   {
   currentHelperNode.nodeID = ev->GetInteger( 1 );
   }


void Actor::SetOnDamageThread( Event *ev )
	{
	ondamage_thread = ev->GetString( 1 );

	if ( ev->NumArgs() > 1 )
		ondamage_threshold = ev->GetInteger( 2 );
	}


void Actor::SetPackageTendency( Event *ev )
   {
   str packageName = ev->GetString( 1 );
   float tendency  = ev->GetFloat( 2 );

   personality->SetBehaviorTendency( packageName, tendency );
   }


void Actor::SetPainThresholdEvent( Event *ev )
   {
	pain_threshold = ( ev->GetFloat( 1 ) );
   }


void Actor::SetPlayPainSoundInterval( Event *ev )
{
	_playPainSoundInterval	= ev->GetFloat( 1 );
}


void Actor::SetPostureState( Event *ev )
{
	postureController->setPostureState( ev->GetString( 1 ) , ev->GetString( 2 ) );
}


void Actor::SetPreferredMax( Event *ev )
	{
	preferredMax = ev->GetFloat( 1 );
	}


void Actor::SetPreferredMin( Event *ev )
	{
   preferredMin = ev->GetFloat( 1 );
	}


void Actor::SetSelfDetonateModel ( Event * ev )
{
	explosionModel = ev->GetString ( 1 );
}


void Actor::SetState( const char *state_name )
	{
	ClassDef *cls;
	int i;
	Event *e;

	if ( !statemap )
		return;

	if ( deadflag )
		return;

	if ( currentState )
		currentState->ProcessExitCommands( this );

   currentState = statemap->FindState( state_name );
	state_time = level.time;


	if ( currentState  )
		{
		cls = getClass( currentState->getBehaviorName() );

		if ( cls )
			{
			if ( currentState->numBehaviorParms() )
				{
				e = new Event( EV_Behavior_Args );

				for ( i = 1 ; i <= currentState->numBehaviorParms() ; i++ )
					e->AddString( currentState->getBehaviorParm( i ) );

				SetBehavior( ( Behavior * )cls->newInstance(), e );
				}
			else
				{
				SetBehavior( ( Behavior * )cls->newInstance() );
				}
			}
		
		cls = getClass( currentState->getHeadBehaviorName() );

		if ( cls )
			{
			if ( currentState->numHeadBehaviorParms() )
				{
				e = new Event( EV_Behavior_Args );

				for ( i = 1; i <= currentState->numHeadBehaviorParms() ; i++ )
					e->AddString( currentState->getHeadBehaviorParm( i ) );

				SetHeadBehavior( ( Behavior * )cls->newInstance(), e );
				}
			else
				{
				SetHeadBehavior( ( Behavior * )cls->newInstance() );
				}
			}
		
		cls = getClass( currentState->getEyeBehaviorName() );

		if ( cls )
			{
			if ( currentState->numEyeBehaviorParms() )
				{
				e = new Event ( EV_Behavior_Args );

				for ( i = 1; i <= currentState->numEyeBehaviorParms() ; i++ )
					e->AddString( currentState->getEyeBehaviorParm( i ) );

				SetEyeBehavior( ( Behavior * )cls->newInstance(), e );
				}
			else
				{
				SetEyeBehavior( ( Behavior * )cls->newInstance() );
				}
			}

		cls = getClass( currentState->getTorsoBehaviorName() );

		if ( cls )
			{
			if ( currentState->numTorsoBehaviorParms() )
				{
				e = new Event ( EV_Behavior_Args );

				for ( i = 1; i <= currentState->numTorsoBehaviorParms(); i++ )
					e->AddString( currentState->getTorsoBehaviorParm( i ) );

				SetTorsoBehavior( (Behavior *)cls->newInstance(), e );
				}
			else
				{
				SetTorsoBehavior( (Behavior *)cls->newInstance() );
				}
			}

		InitState();
		currentState->ProcessEntryCommands( this );
		}
	else
		{
		gi.WDPrintf( "State %s not found\n", state_name );
		}
	}


void Actor::SetTakeDamage ( Event *ev )
	{
	qboolean damage = true;

	if ( ev->NumArgs() > 0 )
		damage = ev->GetBoolean( 1 );

	SetActorFlag( ACTOR_FLAG_TAKE_DAMAGE, damage );
	}


void Actor::SetTalkWatchMode( Event *ev )
{
	str mode_shadow1 = ev->GetString( 1 );

	if ( mode_shadow1 == "ignore" )
		talkMode = TALK_IGNORE;

	if ( mode_shadow1 == "headwatchonly" )
		talkMode = TALK_HEADWATCH;

	if ( mode_shadow1 == "turnto" )
		talkMode = TALK_TURNTO;

	if ( ev->NumArgs() > 1 )
		useConvAnims = ev->GetBoolean(2);
}


void Actor::SetTalkiness( Event *ev )
   {
   personality->SetTalkiness( ev->GetFloat( 1 ) );
   }


void Actor::SetTargetType ( Event *ev )
	{
	targetType = (targetType_t)ev->GetInteger( 1 );
	}


void Actor::SetTendency( Event *ev )
   {
   personality->SetTendency( ev->GetString( 1 ) , ev->GetFloat( 2 ) );
   }


void Actor::SetTimeBetweenSleepChecks( Event *ev )
{
	timeBetweenSleepChecks = ev->GetFloat( 1 );
}


void Actor::SetTorsoBehavior(	Behavior *newTorsoBehavior, Event *startevent, CThread *newthread	)   
	{
	if ( ( deadflag ) && ( actortype != IS_INANIMATE ) )
		{

		if ( newTorsoBehavior )
			delete newTorsoBehavior;
		if ( startevent )
			delete startevent;

		return;
		}

	if ( scriptthread == newthread )
		{
		scriptthread = NULL;
		}
	EndTorsoBehavior();

	torsoBehavior = newTorsoBehavior;
	torsoBehaviorCode = BEHAVIOR_EVALUATING;
	if ( torsoBehavior )
		{
		Wakeup();

		if ( startevent )
			{
			torsoBehavior->ProcessEvent( startevent );
			}
		currentTorsoBehavior = torsoBehavior->getClassname();
		torsoBehavior->Begin( *this );
		scriptthread = newthread;
		}
	}


void Actor::SetValidTarget ( Event *ev )
	{
   Event *flagEvent;
   qboolean valid;
   
   valid = ev->GetBoolean( 1 );

   flagEvent = new Event(EV_EntityFlags);

   if( valid )
      flagEvent->AddString( "-notarget" );
   else
      flagEvent->AddString( "+notarget" );

   ProcessEvent(flagEvent);
   
	}


void Actor::SetVisionDistance( Event *ev )
   {
   sensoryPerception->SetVisionDistance( ev->GetFloat( 1 ) );
	}


void Actor::SetWeaponReady ( Event *ev	)
	{
	SetActorFlag( ACTOR_FLAG_WEAPON_READY , ev->GetBoolean( 1 ) );
	}


void Actor::Sleep( Event *ev )
   {
   Sleep();
   }


void Actor::SpawnActor(	const str &model_name, const Vector	&orig, const Vector &ang, int	how_many,
	                     qboolean attack, float width, float height, qboolean force )
	{
	Actor *new_actor;
	int current_actor;
	trace_t trace;
	Vector spawn_mins;
	Vector spawn_maxs;


	spawn_mins[0] = -width;
	spawn_mins[1] = -width;
	spawn_mins[2] = 0;

	spawn_maxs[0] = width;
	spawn_maxs[1] = width;
	spawn_maxs[2] = height;

	trace = G_Trace( orig, spawn_mins, spawn_maxs, orig, NULL, MASK_MONSTERSOLID, false, "Actor::SpawnActor" );
	
	
	if ( (trace.fraction != 1.0f || trace.allsolid) && !force )
		return;
	


	for( current_actor = 0 ; current_actor < how_many ; current_actor++ )
		{
		new_actor = new Actor;
		new_actor->setModel( model_name );

		new_actor->setOrigin( orig );
		new_actor->setAngles( ang );


		if ( attack )
			new_actor->PostEvent( EV_Actor_AttackPlayer, 0.0f );


		num_of_spawns++;


		new_actor->spawnparent = this;


		new_actor->targetname = targetname;
		new_actor->targetname += "_spawned";

		new_actor->SetTargetName( new_actor->targetname );
		}
	}


void Actor::SpawnActorAboveEnemy( Event *ev )
	{
	str model_name;
	int how_many;
	qboolean attack;
	float width;
	float height;
	float how_far;
	trace_t trace;
	
	Vector spawn_mins;
	Vector spawn_maxs;
	Vector new_orig;
	Vector new_dir;
	Vector Enemy_orig;
	Vector Enemy_dir;
	
	model_name = ev->GetString( 1 );
	how_many   = ev->GetInteger( 2 );
	attack     = ev->GetBoolean( 3 );
	width	     = ev->GetFloat( 4 );
	height	  = ev->GetFloat( 5 );
	how_far    = ev->GetFloat( 6 );

	
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	if ( currentEnemy->isSubclassOf (Player ) )
		{
		Player* player;
		player = (Player*)(Entity*)currentEnemy;
		Enemy_orig = player->origin;
		Enemy_dir = player->angles;
		}
	else
		{
		Actor* act;
		act = (Actor*)(Entity*)currentEnemy;
		Enemy_orig = act->origin;
		Enemy_dir = act->angles;
		}
	
	new_orig = Enemy_orig;
	new_orig.z += how_far;	
	
	new_dir = Enemy_dir;

	spawn_mins.x = -width;
	spawn_mins.y = -width;
	spawn_mins.z = 0;

	spawn_maxs.x = width;
	spawn_maxs.y = width;
	spawn_maxs.z = height;
	
   trace = G_Trace( Enemy_orig, spawn_mins, spawn_maxs, new_orig, NULL, MASK_MONSTERSOLID, false, "SpawnActorAbovePlayer" );
   
	SpawnActor( model_name, trace.endpos, new_dir, how_many, attack, width, height, true);
	}


void Actor::SpawnActorAtLocation( Event *ev )
	{
	str model_name;
	str pathnode_name;
	qboolean attack;
	Vector orig;
	Vector ang;
	float width;
	float height;
	PathNode *goal;
	int number_of_pathnodes;
	Entity *effect;
	trace_t trace;
	Vector spawn_mins;
	Vector spawn_maxs;


	model_name          = ev->GetString( 1 );
	pathnode_name       = ev->GetString( 2 );
	number_of_pathnodes = ev->GetInteger( 3 );
	attack              = ev->GetBoolean( 4 );
	width	              = ev->GetFloat( 5 );
	height	           = ev->GetFloat( 6 );


	pathnode_name += (int)G_Random( (float)number_of_pathnodes ) + 1;


	goal = thePathManager.FindNode( pathnode_name );

	if ( !goal )
		{
		gi.WPrintf( "Can't find position %s\n", pathnode_name.c_str() );
		return;
		}


	orig = goal->origin;
	ang = goal->angles;

	spawn_mins.x = -width;
	spawn_mins.y = -width;
	spawn_mins.z = 0;

	spawn_maxs.x = width;
	spawn_maxs.y = width;
	spawn_maxs.z = height;

	trace = G_Trace( orig + Vector( "0 0 64" ), spawn_mins, spawn_maxs, orig - Vector( "0 0 128" ), NULL, MASK_MONSTERSOLID, false, "SpawnActorAtLocation" );

	if ( trace.allsolid )
		return;

	orig = trace.endpos;

	SpawnActor( model_name, orig, ang, 1, attack, width, height );


	effect = new Entity( ENTITY_CREATE_FLAG_ANIMATE );
	effect->setModel( "fx_teleport3.tik" );
	effect->setOrigin( orig );
	effect->setSolidType( SOLID_NOT );
	effect->animate->RandomAnimate( "idle", EV_Remove );
	effect->Sound( "snd_teleport" );
	}


void Actor::SpawnActorAtTag( Event *ev	)
	{
	str model_name;
	str tag_name;
	int how_many;
	qboolean attack;
	float spawn_offset = 0;
	Vector tag_orig;
	Vector tag_dir;
	Vector new_orig;
	Vector new_dir;
	float width;
	float height;
   qboolean force;
	float addHeight;

	model_name = ev->GetString( 1 );
	tag_name   = ev->GetString( 2 );
	how_many   = ev->GetInteger( 3 );
	attack     = ev->GetBoolean( 4 );
	width	     = ev->GetFloat( 5 );
	height	  = ev->GetFloat( 6 );

	if ( ev->NumArgs() > 6 )
		spawn_offset = ev->GetFloat( 7 );
	else
		spawn_offset = 0.0f;
	
	if ( spawn_offset < 0.0f ) spawn_offset = 0.0f;

	if ( ev->NumArgs() > 7 )
		force = ev->GetBoolean( 8 );
	else
	   force = false;

	if ( ev->NumArgs() > 8 )
		addHeight = ev->GetFloat( 9 );
	else
		addHeight = 0;




	GetTag( tag_name.c_str(), &tag_orig, &tag_dir );
	
	if(spawn_offset)
		{
		new_orig = tag_orig + tag_dir * spawn_offset;		
		new_orig[2] = new_orig[2] + addHeight;

		new_dir  = tag_dir * -1.0f;
		
		SpawnActor( model_name, new_orig, new_dir, how_many, attack, width, height, force );
		return;
		}
	
	float offset = 250.0f;
	for ( int i = 0; i < how_many; i++)
		{
		Vector side;
		origin.AngleVectors(NULL,&side,NULL);
		side.normalize();

		new_orig = side*offset+origin;

		if ( i%2 == 0 )
			new_orig*=-1.0f;
		else
			offset*=2.25f;

		new_dir  = tag_dir * -1.0f;
		SpawnActor( model_name, new_orig, new_dir, how_many, attack, width, height, force );
		}
	}


void Actor::SpawnBlood(	Event *ev )
	{
	str blood_name;
	str tag_name;
	qboolean use_last_result = false;
	Event *attach_event;


	if ( !com_blood->integer )
		return;

	blood_name = ev->GetString( 1 );
	tag_name   = ev->GetString( 2 );


	if ( ev->NumArgs() > 2 )
		use_last_result = ev->GetBoolean( 3 );

	if ( use_last_result && GetActorFlag( ACTOR_FLAG_SPAWN_FAILED ) )
		return;


	attach_event = (Event *)new Event( EV_AttachModel );

	attach_event->AddString( blood_name );
	attach_event->AddString( tag_name );
	attach_event->AddInteger( 1 );
	attach_event->AddString( "" );
	attach_event->AddInteger( 0 );
	attach_event->AddInteger( 5 );
	PostEvent( attach_event, 0.0f );
	}


void Actor::SpawnGib( Event *ev )
	{
	RealSpawnGib( false, ev );
	}


void Actor::SpawnGibAtTag(	Event *ev )
	{
	RealSpawnGib( true, ev );
	}


void Actor::SpawnNamedGib( Event *ev )
	{
	str gib_name;
	str tag_name;
	Gib *gib;
	Vector orig;
	float final_pitch;
	float vel;
	trace_t trace;
	float time;
	float pitch_change;
	float pitch_vel;
	float width;
	Vector gib_mins;
	Vector gib_maxs;


	SetActorFlag( ACTOR_FLAG_SPAWN_FAILED, true );

	if ( GetActorFlag( ACTOR_FLAG_FADING_OUT ) )
		return;


	gib_name      = ev->GetString( 1 );
	tag_name      = ev->GetString( 2 );
	final_pitch   = ev->GetFloat( 3 );
	width         = ev->GetFloat( 4 );


	GetTag( tag_name, &orig );


	gib_mins = Vector( -width, -width, -width );
	gib_maxs = Vector(  width,  width,  width );


	trace = G_Trace( orig, gib_mins, gib_maxs, orig, NULL, MASK_DEADSOLID, false, "spawnnamedgib1" );

	if ( trace.allsolid || trace.startsolid )
		SetActorFlag( ACTOR_FLAG_SPAWN_FAILED, true );


	vel = 400.0f + G_Random( 400.0f );

	time = SpawnGetTime( vel , orig, gib_mins, gib_maxs );

	pitch_change = AngleNormalize180( final_pitch - angles[PITCH] );
	pitch_vel    = pitch_change / time;


	gib = new Gib( gib_name, false, "", "", "", final_pitch );

	gib->setOrigin( orig );
	gib->setAngles( angles );

	gib->velocity  = Vector( G_CRandom( 200.0f ), G_CRandom( 200.0f ), vel );
   gib->avelocity = Vector( pitch_vel, G_CRandom( 300.0f ), 0.0f );

	gib->edict->svflags |= SVF_DEADMONSTER;
	gib->edict->clipmask = MASK_DEADSOLID;

	gib->setSolidType( SOLID_BBOX );
   gib->setContents( CONTENTS_CORPSE );


	if ( GetActorFlag( ACTOR_FLAG_DEATHFADE ) )
		gib->PostEvent( EV_Fade, 10.0f );
	else if ( GetActorFlag( ACTOR_FLAG_DEATHSHRINK ) )
		gib->PostEvent( EV_FadeOut, 10.0f );
	else
		gib->PostEvent( EV_DeathSinkStart, 10.0f );


	SetActorFlag( ACTOR_FLAG_SPAWN_FAILED, false );
	}


void Actor::StartMode( int new_mode )
	{
	if ( new_mode == ACTOR_MODE_TALK )
		{
		SaveMode();
		CancelEventsOfType( EV_Actor_EndBehavior );
		CancelEventsOfType( EV_Actor_EndHeadBehavior );
		CancelEventsOfType( EV_Actor_EndEyeBehavior );
		CancelEventsOfType( EV_Actor_EndTorsoBehavior );
		RemoveAnimDoneEvent();
		}

	mode = new_mode;
	}


void Actor::StaySolidEvent( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_STAYSOLID, true );
	}


void Actor::StunEvent( Event *ev	)
	{
	float time;

	time = ev->GetFloat( 1 );

	SetActorFlag( ACTOR_FLAG_STUNNED, true );

	stunned_end_time = level.time + time;
	}


void Actor::Suicide( Event *ev )
	{
	Event *event;
	qboolean use_last_mod;

	health = 0;

	if ( ev->NumArgs() > 0 )
		use_last_mod = ev->GetBoolean( 1 );
	else
		use_last_mod = false;

	if ( !use_last_mod )
		means_of_death = MOD_SUICIDE;

	event = new Event( EV_Killed );
	event->AddEntity( this );
	event->AddInteger( 0 );
	event->AddEntity( this );
	event->AddInteger( MOD_SUICIDE );
	ProcessEvent( event );
	}


void Actor::ThrowEnt( Event *ev )
	{
	Event *e;

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_ThrowEnt );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}

	e->AddString( ev->GetString( 1 ) );

	SetBehavior( new ThrowEntity, e, ev->GetThread() );
	}


void Actor::TossThrowObject( Event *ev	)
	{
	ThrowObject* tobj = 0;
	float speed = 0;
	float gravity_shadow1 = 1;
	float damage = 25;
	
	Entity* currentEnt;
	currentEnt = enemyManager->GetCurrentEnemy();
	if ( !currentEnt )
		return;

	tobj = (ThrowObject*)(Entity*)enemyManager->GetAlternateTarget();
	if(!tobj || !currentEnt->isSubclassOf(Sentient)) return;
	
	speed = ev->GetFloat( 1 );
	gravity_shadow1 = ev->GetFloat( 2 );
	if (ev->NumArgs() > 2 )
		damage = ev->GetFloat( 3 );
	
	tobj->Throw(this, speed , (Sentient*)currentEnt , gravity_shadow1, damage );
		
	}


void Actor::Touched(	Event *ev )
	{
	Entity *other;

	other = ev->GetEntity( 1 );

	sensoryPerception->Stimuli( STIMULI_SIGHT, other );

   if ( other->isSubclassOf( Player) )
      {
      AddStateFlag( STATE_FLAG_TOUCHED_BY_PLAYER );
      }

	}


void Actor::TryTalkToPlayer( void )
	{
	int player_near = false;
	Entity *ent_in_range;
   int i;
	Vector delta;
	gentity_t *ed;
	float dist2;
	Sentient	*user = NULL;
	Talk *talk;



	if ( level.cinematic )
		next_player_near = level.time + 5.0f;

	if ( deadflag || actortype != IS_FRIEND || next_player_near > level.time || !ModeAllowed( ACTOR_MODE_TALK ) )
		return;

	next_player_near = level.time + .2f + G_Random( .1f );


	for( i = 0 ; i < game.maxclients; i++ )
      {
      ed = &g_entities[ i ];

		if ( !ed->inuse || !ed->entity )
			continue;

		ent_in_range = ed->entity;

		if ( EntityHasFireType( ent_in_range, FT_MELEE ) || EntityHasFireType( ent_in_range, FT_BULLET ) ||
			  EntityHasFireType( ent_in_range, FT_PROJECTILE ) )
			continue;

		if ( IsEntityAlive( ent_in_range ) && ( ent_in_range->velocity == vec_zero ) && sensoryPerception)
         {
         delta = centroid - ent_in_range->centroid;


			dist2 = delta * delta;

		   if ( ( dist2 <= 100.0f * 100.0f ) && sensoryPerception->CanSeeEntity( this , ent_in_range , true , true ) )
				{
				player_near = true;
				user = (Sentient *)ent_in_range;
				}
         }
		}

	if ( !player_near )
		{
		SetActorFlag( ACTOR_FLAG_LAST_TRY_TALK, false );
		return;
		}

	if ( !GetActorFlag( ACTOR_FLAG_LAST_TRY_TALK ) )
		{
		SetActorFlag( ACTOR_FLAG_LAST_TRY_TALK, true );
		return;
		}

	SetActorFlag( ACTOR_FLAG_LAST_TRY_TALK, false );


	StartMode( ACTOR_MODE_TALK );

	talk = new Talk;
	talk->SetUser( user );
	SetBehavior( talk );
	}


void Actor::TurnOffEnemyAI( Event *ev )
	{
	Actor* act = 0;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	if ( !currentEnemy->isSubclassOf( Actor ) )
		return;

	act = (Actor*)(Entity*)currentEnemy;
	
	if ( act )
		act->TurnAIOff();
	}


void Actor::TurnOnEnemyAI(	Event *ev )
	{
	Actor* act = 0;
	
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	if ( !currentEnemy->isSubclassOf( Actor ) )
		return;

	act = (Actor*)(Entity*)currentEnemy;
	
	if ( act )
		act->TurnAIOn();
	}


void Actor::TurnToEvent( Event *ev	)
	{
	TurnTo *turnTo;

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_TurnTo );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	turnTo = new TurnTo;
	turnTo->SetDirection( ev->GetFloat( 1 ) );

	if ( ev->NumArgs() > 1 )
		turnTo->useAnims( ev->GetBoolean( 2 ) );

	SetBehavior( turnTo, NULL, ev->GetThread() );
	}


void Actor::UnRegisterBehaviorPackage( Event *ev )
	{
   packageManager->UnregisterPackage(ev->GetString( 1 ) );
	}


void Actor::UnreserveCurrentHelperNode( Event *ev )
{
	UnreserveCurrentHelperNode();
}


void Actor::UseActorWeapon( Event *ev )
	{
	str weaponName;
	str handToUse;
	weaponhand_t hand;

	weaponName = ev->GetString( 1 );
	hand = WEAPON_DUAL;

	if ( ev->NumArgs() > 1 )
		{
		handToUse = ev->GetString( 2 );

		if ( !Q_stricmp( handToUse.c_str() , "right" ) )
			hand = WEAPON_RIGHT;

		if ( !Q_stricmp( handToUse.c_str() , "left" ) )
			hand = WEAPON_LEFT;

		if ( !Q_stricmp( handToUse.c_str() , "dual" ) )
			hand = WEAPON_DUAL;
		}

	combatSubsystem->UseActorWeapon( weaponName , hand );
	}


void Actor::UseBehaviorPackage( Event *ev )
{
	UseBehaviorPackage(ev->GetString( 1 ) );
}


void Actor::UseWeaponDamage( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		_useWeaponDamage = WeaponHandNameToNum(ev->GetString( 1 ));
	else
		_useWeaponDamage = WEAPON_RIGHT;

	if ( ev->NumArgs() > 1 )
	{
		if ( !ev->GetBoolean( 2 ) )
			_useWeaponDamage = WEAPON_ERROR;
	}
}


void Actor::Wakeup( void )
   {
   Actor *actor;

   if ( isThinkOn() && !LoadingSavegame )
      return;

   if ( actortype == IS_INANIMATE )
      return;

   actor = this;

   if ( SleepList.ObjectInList( actor ) )
      SleepList.RemoveObject( actor );

   if ( !ActiveList.ObjectInList( actor ) )
      ActiveList.AddObject( actor );

   turnThinkOn();

   last_time_active = level.time;
   }

void Actor::Wakeup( Event *ev )
   {
   Wakeup();
   }


void Actor::WalkWatch (	Event *ev )
	{
	Event *e;


	if ( ev->NumArgs() < 2 )
		return;

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			RepostEvent( ev, EV_Actor_WalkWatch );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	StartMode( ACTOR_MODE_SCRIPT );

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}


	if ( ev->NumArgs() > 2 )
		e->AddString( ev->GetString( 3 ) );
	else
		e->AddString( "walk" );


	e->AddToken( ev->GetToken( 1 ) );


	e->AddEntity( ev->GetEntity( 2 ) );

	SetBehavior( new GotoPathNode, e, ev->GetThread() );
	}


void Actor::WhatAreYouDoing( Event *ev )
{
	str tName;
	tName = targetname;
	if ( !tName.length() )
		tName = "None";

	gi.Printf( "\n-----------------------------------------------" );
	gi.Printf( "\nTargetName : %s\n" , tName.c_str()               );
	gi.Printf( "\n-----------------------------------------------" );
	
	if ( masterstatemap )
		{
		PrintMasterStateInfo();
		PrintBehaviorPackageInfo();
		}
	else
		{
		gi.Printf( "\nNo Master State" );
		gi.Printf( "\n" );
		
		if ( statemap )
			PrintStateMapInfo();
		else
			gi.Printf( "\nNo State Map\n" );
		}

	gi.Printf( "\n-----------------------------------------------" );
}





void Actor::resetStateMachine( void )
{
	SetState( idle_state_name.c_str() );
}


void Actor::setDeathEffect( Event *ev )
{
	_deathEffect = ev->GetString( 1 );
}


void Actor::setDialogMorphMult( Event *ev )
{
	_dialogMorphMult = ev->GetFloat( 1 );
}


Actor* Actor::GetAttachedChildActor(const str &childName)
{
	[[maybe_unused]] int num;
	Entity *child = 0;
	Actor *childActor = 0;

	if ( !bind_info )
		return NULL;

	num			= bind_info->numchildren;	
	child		= NULL;
	childActor	= NULL;

	for ( int i=0; i < MAX_MODEL_CHILDREN; i++ )
		{
		if ( bind_info->children[i] == ENTITYNUM_NONE )
			continue;

		child = ( Entity * )G_GetEntity( bind_info->children[i] );

		if ( !Q_stricmp(child->TargetName() , childName.c_str() ) )
			{
			if ( child->isSubclassOf(Actor) )
				childActor = (Actor*)child;

			if ( childActor )
				return childActor;
			}
		}

	return NULL;
}


qboolean Actor::GetClosestTag( const str &tag_name, int number_of_tags, const Vector &target_shadow3, Vector *orig )
	{
	str temp_tag_name;
	Vector temp_orig;
	Vector diff;
	float dist;
	float best_dist = -1;
	qboolean found = false;
	int i;
	char number[12];

	if ( number_of_tags == 1 )
      {
		return GetTag( tag_name.c_str(), orig );
      }

	for( i = 1 ; i <= number_of_tags ; i++ )
		{
		snprintf( number, sizeof( number ), "%d", i );

		temp_tag_name = tag_name + str( number );

		if ( GetTag( temp_tag_name.c_str(), &temp_orig ) )
			{
			diff = target_shadow3 - temp_orig;
			dist = diff * diff;

			if ( ( dist < best_dist ) || ( best_dist < 0 ) )
				{
				best_dist = dist;
				found = true;
				*orig = temp_orig;
				}
			}
		}

	return found;
	}





qboolean Actor::RespondToHitscan( void )
	{
	if( G_Random() > hitscan_response_chance )
		{
		SetActorFlag( ACTOR_FLAG_INCOMING_HITSCAN , false );
		return false;
		}
	

	if ( statemap )
		{

		State	*temp_state;
		temp_state = statemap->FindState( "INCOMING_HITSCAN" );

		if ( temp_state )
			{
			SetActorFlag( ACTOR_FLAG_RESPONDING_TO_HITSCAN, true );
			currentState = temp_state;
			ProcessActorStateMachine();
			}
		else
			{
			SetActorFlag( ACTOR_FLAG_INCOMING_HITSCAN , false );
			return false;
			}
		}

	SetActorFlag( ACTOR_FLAG_INCOMING_HITSCAN , false );
	SetActorFlag( ACTOR_FLAG_RESPONDING_TO_HITSCAN, false );

	return true;	
	}


float Actor::SpawnGetTime( float vel, const Vector &orig, const Vector &gib_mins, const Vector &gib_maxs )
	{
	float grav;
	Vector end_pos;
	Vector dir;
	float time;
	float other;
	trace_t trace;
	float dist;

	grav = -sv_currentGravity->value;

	end_pos = orig;
	end_pos[2] = -10000.0f;

	trace = G_Trace( orig, gib_mins, gib_maxs, end_pos, NULL, MASK_DEADSOLID, false, "SpawnGetTime" );

	end_pos = trace.endpos;

	dir = end_pos - orig;
	dist = dir.length();

	time = ( ( grav / -20.0f ) - vel ) / grav;

	other = sqrt( ( grav / 20.0f + vel ) * ( grav / 20.0f + vel ) - ( 2.0f * grav * dist ) );

	time = time - ( other / grav );

	return time;
	}


qboolean Actor::WantsToTalk()
   {
   return ( G_Random() <= personality->GetTalkiness() );
   }


qboolean Actor::_FleeNodeInDistance( float dist )
   {
   int wtf;
   wtf = (int)(lastPathCheck_Flee + HACK_PATH_CHECK);
   
   if ( wtf >= level.time )
      return false;
   
   lastPathCheck_Flee = level.time + HACK_PATH_CHECK + G_Random();
   
  
   Vector delta;
   Vector pos;
   Vector nodeOrigin;
   
	for ( int i = 1 ; i <= thePathManager.NumberOfSpecialNodes(); i++ )   
	{   
		PathNode *node = thePathManager.GetSpecialNode( i );		
         
         pos = origin;
         pos.z += 80;

         nodeOrigin = node->origin;

         if (!sensoryPerception->CanSeePosition( pos , nodeOrigin , true , true ) )
            continue;

 	         if ( node && ( node->nodeflags & ( AI_FLEE ) ) &&
			      ( ( node->occupiedTime <= level.time ) ) && ( node->entnum == 0 || node->entnum == entnum ) )
			      {
               delta = node->origin - origin;

               if ( delta.length() <= dist )
                  return true;
	      
			      }
      
		}

   return false;
   }


qboolean Actor::_WorkNodeInDistance( float dist )
   {  
    int wtf;
   wtf = (int)(lastPathCheck_Work + HACK_PATH_CHECK);
   
   if ( wtf >= level.time )
      return false;
   
   lastPathCheck_Work = level.time + G_Random();

   Vector delta;
   Vector pos;
   Vector nodeOrigin;

	for ( int i = 1 ; i <= thePathManager.NumberOfSpecialNodes(); i++ )   
	{   
		PathNode *node = thePathManager.GetSpecialNode( i );		

         pos = origin;
         pos.z += 80;
     
         nodeOrigin = node->origin;

         if (!sensoryPerception->CanSeePosition( pos , nodeOrigin , true , true ) )
            continue;
         
 	         if ( node && ( node->nodeflags & ( AI_WORK ) ) &&
			      ( ( node->occupiedTime <= level.time ) ) && ( node->entnum == 0 || node->entnum == entnum ) )
			      {
               if ( !_isWorkNodeValid( node ) )
                  continue;
         
               delta = node->origin - origin;

               if ( delta.length() <= dist )
                  return true;
	      
			      }

      
		}

   return false;
   }


qboolean Actor::_isWorkNodeValid( PathNode *node )
   {
   WorkTrigger *target_shadow1 = NULL;

	if ( node->targetEntity )
		{
		Entity *entity = node->targetEntity;

		if( entity->isSubclassOf( WorkTrigger ) )
		   {
			target_shadow1 = (WorkTrigger*)entity;
         
         if ( node->occupiedTime > level.time && !target_shadow1->isReserved() )
            return false;

         if ( target_shadow1->isAllowedToWork( targetname , entnum ) )
            return true;
		   }
		}

   
   if ( !target_shadow1 )
      return true;


   return false;
   }


qboolean Actor::checkActivationDelayTime( Conditional &condition )
{
	return checkActivationDelayTime();
}


qboolean Actor::checkActorType( Conditional &condition )
{
	str aType = condition.getParm( 1 );
	
	if ( !Q_stricmp( aType.c_str() , "inanimate" ) )
		{
		if ( actortype == IS_INANIMATE )
			return true;
		else
			return false;
		}
	else if ( !Q_stricmp( aType.c_str() , "monster" ) )
		{
		if ( actortype == IS_MONSTER )
			return true;
		else
			return false;
		}
	else if ( !Q_stricmp( aType.c_str() , "enemy" ) )
		{
		if ( actortype == IS_ENEMY )
			return true;
		else
			return false;		
		}
	else if ( !Q_stricmp( aType.c_str() , "civilian" ) )
		{
		if ( actortype == IS_CIVILIAN )
			return true;
		else
			return false;		
		}
	else if ( !Q_stricmp( aType.c_str() , "friend" ) )
		{
		if ( actortype == IS_FRIEND )
			return true;
		else
			return false;		
		}
	else if ( !Q_stricmp( aType.c_str() , "animal" ) )
		{
		if ( actortype == IS_ANIMAL )
			return true;
		else
			return false;	
		}
	else if ( !Q_stricmp( aType.c_str() , "teammate" ) )
	{
		if ( actortype == IS_TEAMMATE )
			return true;
		else
			return false;
	}
	
	return false;
}


qboolean Actor::checkAllowedToMeleeEnemy( Conditional &condition )
{
	return checkAllowedToMeleeEnemy();
}


qboolean Actor::checkAnyEnemyInRange( Conditional &condition )
{
	float range = atof( condition.getParm( 1 ) );
	return checkAnyEnemyInRange( range );
}


qboolean Actor::checkAttacked( Conditional &condition )
{
	return checkAttacked();
}


qboolean Actor::checkAttackedByPlayer( Conditional &condition )
{
	return checkAttackedByPlayer();
}


qboolean Actor::checkBlockedByEnemy( Conditional &condition )
{
	if ( !(state_flags & STATE_FLAG_BLOCKED_BY_ENTITY) )
		return false;

	Entity *blockingEntity = movementSubsystem->getBlockingEntity();

	if ( !blockingEntity )
		return false;	

	sensoryPerception->Stimuli( STIMULI_SIGHT, blockingEntity );
				

	if ( enemyManager->Hates( blockingEntity ) )
		return true;

	return false;
	
}


qboolean Actor::checkBoneDamaged( Conditional &condition	)
	{
	int bone_number;
	str bone_name = condition.getParm( 1 );

	if ( saved_bone_hit == -9999 )
		return false;

	bone_number = gi.Tag_NumForName( edict->s.modelindex, bone_name.c_str() );

	if ( bone_number == saved_bone_hit )
		return true;
	else
		return false;
	}


qboolean Actor::checkCanAttackAnyEnemy ( Conditional &condition )
   {
   return enemyManager->CanAttackAnyEnemy();
   }


qboolean Actor::checkCanAttackEnemy(Conditional &condition)
{
	return checkCanAttackEnemy();
}


qboolean Actor::checkCanWalkForward( Conditional &condition )
	{
   trace_t trace;
	Vector endpos;
	Vector startpos;
	str rangestr;
	float range;
	
	angles.AngleVectors(&endpos); 

	endpos += origin;
	endpos.z += 50.0f; 

   rangestr = condition.getParm( 1 );
   range = (float)atof( rangestr.c_str() );

	endpos *= range;
	
	startpos = origin;
	startpos.z += 50.0f;

	trace = G_Trace( startpos, mins, maxs, endpos, this, edict->clipmask, false, "Actor::start" );

	if (trace.fraction == 1.0f )
		return true;
	else
		return false;
	}


qboolean Actor::checkCaptured( Conditional &condition	)
	{
   return ( GetActorFlag(ACTOR_FLAG_CAPTURED) );
	}


qboolean Actor::checkCountOfIdenticalNamesInGroup( Conditional &condition )
{
	int checkValue;
	str checkName;

	checkName = condition.getParm( 1 );
	checkValue = atoi(condition.getParm( 2 ));

	return checkCountOfIdenticalNamesInGroup( checkName , checkValue );
}


qboolean Actor::checkCoverNodes( Conditional &condition )
	{
	for ( int i = 1 ; i <= thePathManager.NumberOfSpecialNodes(); i++ )   
	{   
		PathNode *node = thePathManager.GetSpecialNode( i );		
		
		if ( node && ( node->nodeflags & ( AI_DUCK | AI_COVER ) ) &&
			( ( node->occupiedTime <= level.time ) || ( node->entnum == entnum ) ) )
		{
			return true;
		}
	}
	
	return false;
	}


qboolean Actor::checkCrippled( Conditional &condition )
   {
   return GetActorFlag( ACTOR_FLAG_CRIPPLED );
   }


qboolean Actor::checkCurrentEnemyGroupAttackerCount( Conditional &condition )
{
	Entity* e = enemyManager->GetCurrentEnemy();
	return checkGroupAttackerCountForEntity( condition, e );
}


qboolean Actor::checkCurrentNodeHasThisCoverType( Conditional &condition )
{
	str coverType = condition.getParm( 1 );
	return checkCurrentNodeHasThisCoverType( coverType );
}


qboolean Actor::checkDamageThresholdExceeded( Conditional &condition )
{
	return checkDamageThresholdExceeded();
}


qboolean Actor::checkDisabled( Conditional &condition )
	{
	return GetActorFlag( ACTOR_FLAG_DISABLED );
	}


qboolean Actor::checkEnemiesNearby(Conditional &condition )
{
	float dist = atof(condition.getParm(1) );
	return checkEnemiesNearby(dist);
}


qboolean Actor::checkEnemyAttached( Conditional &condition )
	{
   return haveAttached;
	}


qboolean Actor::checkEnemyCanSeeCurrentNode( Conditional &condition )
{
	return checkEnemyCanSeeCurrentNode();
}


qboolean Actor::checkEnemyIsThrowObject( Conditional &condition )
	{
	Entity* ent = 0;
	
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;


	ent = (Entity*)currentEnemy;
	if ( ent->isSubclassOf(ThrowObject) )
		return true;
	else
		return false;
		
	}


qboolean Actor::checkEnemyProjectileClose( Conditional &condition )
{
	return checkEnemyProjectileClose();
}


qboolean Actor::checkEnemyWeaponNamed( Conditional &condition )
{
	str weaponName = condition.getParm( 1 );
	return checkEnemyWeaponNamed( weaponName );
	
}


qboolean Actor::checkEnemyWithinRange( Conditional &condition )
{
	return checkEnemyWithinRange( atof(condition.getParm( 1 ) ) , atof(condition.getParm(2) ) );		
}


qboolean Actor::checkForwardDirectionClear( Conditional &condition )
   {
   return checkForwardDirectionClear(atof(condition.getParm( 1 ) ));
   }


qboolean Actor::checkGameSpecific( Conditional &condition )
	{
	if (!gameComponent)
		return false;
	
	return gameComponent->DoCheck( condition );
	
	}


qboolean Actor::checkGroupAttackerCount( Conditional &condition )
{
	return checkGroupAttackerCountForEntity( condition, NULL );
}


qboolean Actor::checkGroupAttackerCountForEntity( Conditional& condition, Entity* attackTarget )
{
	int checkValue = atoi(condition.getParm( 1 ));
	
	return checkGroupAttackerCountForEntity( checkValue , attackTarget );
}


qboolean Actor::checkGroupMememberRange( Conditional &condition )
   {
   Actor *act;   
   float  dist;   
   float  reqDist;
   Vector actToSelf;

   
   reqDist = atof(condition.getParm( 1 ));

   for( int i = 1; i <= ActiveList.NumObjects(); i++ )   
      {
      act = ActiveList.ObjectAt( i );
      if ( act && act != this && act->GetGroupID() == GetGroupID() )
         {
         actToSelf = origin - act->origin;
         dist = actToSelf.length();

         if ( dist <= reqDist )
            {
            return true;
            }
         }

      }

   return false;
   }


qboolean Actor::checkHasThrowObject( Conditional &condition	)
	{
	return haveThrowObject;
	}


qboolean Actor::checkHaveActiveWeapon( Conditional &condition )
{
	return combatSubsystem->HaveWeapon();
}


qboolean Actor::checkHaveArmor( Conditional &condition )
{
	return checkHaveArmor();
}


qboolean Actor::checkHaveBestWeapon( Conditional &condition )
{
	return checkHaveBestWeapon();
}


qboolean Actor::checkHavePathToEnemy( Conditional &condition )
{
	return checkHavePathToEnemy();
}


qboolean Actor::checkHelperNodeWithFlagInRange( Conditional &condition )
{
	str flagName = condition.getParm( 1 );
	float range = atof(condition.getParm( 2 ) );

	return checkHelperNodeWithFlagInRange( flagName , range );
}


qboolean Actor::checkInAbsoluteRange( Conditional &condition )
	{
   
	if ( !Q_stricmp( condition.getParm( 1 ) , "player" ) )
		{
      Player *player;
      Entity *enemy_shadow10;

		Vector dist;
		float length;
			
		player = GetPlayer( 0 );
		if ( !player )
			return false;

		dist = origin - player->origin;
		length = dist.length();
      
      enemy_shadow10 = NULL;
      enemy_shadow10 = enemyManager->GetCurrentEnemy();

      if ( enemy_shadow10 )
         {
   		if ( ( length < (absoluteMax * 2 ) ) && ( length > absoluteMin ) )
	   		return true;
         }

		if ( ( length < absoluteMax ) && ( length > absoluteMin ) )
			return true;
		}

	return false;

	}


qboolean Actor::checkInAlcove( Conditional &condition )
	{
	return GetActorFlag( ACTOR_FLAG_IN_ALCOVE );
	}


qboolean Actor::checkInCallVolume( Conditional &condition )
   {
   return GetActorFlag(ACTOR_FLAG_IN_CALL_VOLUME);
   }


qboolean Actor::checkInPreferredRange( Conditional &condition )
	{
	if ( !Q_stricmp( condition.getParm( 1 ) , "player" ) )
		{
      Player *player;
		Vector dist;
		float length;

		player = GetPlayer( 0 );
		if ( !player )
			return false;

		dist = origin - player->origin;
		length = dist.length();

		if ( ( length < preferredMax ) && ( length > preferredMin ) )
			return true;
		}

	return false;
	}


qboolean Actor::checkInTheWay( Conditional &condition )
{
	return checkInTheWay();
}


qboolean Actor::checkIsTeammate( Conditional &condition )
{
	if ( actortype == IS_TEAMMATE )
		return true;
	else
		return false;
}


qboolean Actor::checkLastState( Conditional &condition )
   {
   str lastStateName;
   lastStateName = condition.getParm( 1 );

   if ( !Q_stricmp( lastStateName.c_str() , lastState->getName() ) )
      return true;
   else
      return false;
   }


qboolean Actor::checkLeftDirectionClear( Conditional &condition )
   {
   return checkLeftDirectionClear(atof(condition.getParm( 1 ) ));
   }


qboolean Actor::checkMeleeHitWorld(	Conditional &condition )
	{
	qboolean hit;
	if ( GetActorFlag( ACTOR_FLAG_MELEE_HIT_WORLD ) )
		{
		hit = true;
		SetActorFlag( ACTOR_FLAG_MELEE_HIT_WORLD, false );
		}
	else
		hit = false;

	return hit;
	}


qboolean Actor::checkNodeExists(	Conditional &condition )
	{
	str nodeName        = condition.getParm( 1 );
	PathNode *testNode  = 0;
	
	
	nodeName += "0";
   
	testNode = thePathManager.FindNode( nodeName );

	if( testNode )
		{
		return true;
		}

	return false;
	
	}


qboolean Actor::checkOutOfTorsoRange( Conditional &condition )
   {
   return GetActorFlag( ACTOR_FLAG_OUT_OF_TORSO_RANGE );
   }


qboolean Actor::checkPathNodeTypeInDistance( Conditional &condition )
   {  
   str nodeType = condition.getParm( 1 );
   float distance = atof(condition.getParm( 2 ));

   if ( !Q_stricmp( "work" , nodeType.c_str() ) )
      return _WorkNodeInDistance( distance );

   if ( !Q_stricmp( "flee" , nodeType.c_str() ) )
      return _FleeNodeInDistance( distance );

   return false;
   }


qboolean Actor::checkPlayerInCallVolume( Conditional &condition )
   {
   return GetActorFlag(ACTOR_FLAG_PLAYER_IN_CALL_VOLUME);
   }


qboolean Actor::checkPlayerValid( Conditional &condition )
	{
 	Player *player = GetPlayer(0);

	if ( !player ) 
		return false;

	if ( EntityIsValidTarget( player ) )
		return true;

	return false;
	}


qboolean Actor::checkPlayerWeaponNamed( Conditional &condition )
{
	str weaponName = condition.getParm( 1 );
	return checkPlayerWeaponNamed( weaponName );
	
}


qboolean Actor::checkPosture( Conditional &condition )
{
	return checkPosture( condition.getParm( 1 ) );
}


qboolean Actor::checkPostureAnimDone( Conditional &condition )
{
	return GetActorFlag(ACTOR_FLAG_POSTURE_ANIM_DONE );
}


qboolean Actor::checkPropChance( Conditional &condition )
	{
	str propname;
	str objname = condition.getParm( 1 );
	str scopestr;

	if ( condition.numParms() > 1 )
		propname = condition.getParm( 2 );

	GameplayManager *gpm = GameplayManager::getTheGameplayManager();

	if ( !objname.length() )		
		scopestr = getArchetype();		
	else
		scopestr = getArchetype() + "." + objname;

	if ( !gpm->hasObject(scopestr) )
		return false;

	float chance;
	if ( propname.length() )
		chance = gpm->getFloatValue(scopestr, propname);
	else
		chance = gpm->getFloatValue(scopestr, "value");

	return ( G_Random() <= chance );
	}


qboolean Actor::checkPropEnemyRange( Conditional &condition )
{
	str propname;
	str objname = condition.getParm( 1 );
	if ( condition.numParms() > 1 )
		propname = condition.getParm( 2 );

	return checkPropEnemyRange( objname , propname );
}


qboolean Actor::checkPropExists( Conditional &condition )
	{
	str objname = condition.getParm( 1 );
	str propname = condition.getParm( 2 );
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str scopestr = getArchetype() + "." + objname;
	if ( !gpm->hasProperty(scopestr, propname) )
		return false;

	return true;
	}


qboolean Actor::checkRearDirectionClear( Conditional &condition )
   {
   return checkRearDirectionClear(atof(condition.getParm( 1 ) ));
   }


qboolean Actor::checkRegionDamaged( Conditional &condition )
	{
	int region_bit;
	str region_name;


	if ( last_region_hit == 0 )
		return false;


	region_name = condition.getParm( 1 );


	region_bit = 0;

	if ( region_name == "back" )
		region_bit = REGIONAL_DAMAGE_BACK;
	else if ( region_name == "front" )
		region_bit = REGIONAL_DAMAGE_FRONT;

	
	return ( last_region_hit & region_bit );
	}


qboolean Actor::checkRequestedPosture( Conditional &condition )
{
	str postureName = condition.getParm( 1 );
	str requestedPostureName = postureController->getRequestedPostureName();

	if ( postureName == requestedPostureName )
		return true;

	return false;		
}


qboolean Actor::checkRightDirectionClear( Conditional &condition )
   {
   return checkRightDirectionClear(atof(condition.getParm( 1 ) ));
   }


qboolean Actor::checkShouldDoAction( Conditional &condition )
{
	str tendencyName = condition.getParm( 1 );
	return checkShouldDoAction( tendencyName );
}


qboolean Actor::checkShowPain( Conditional &condition )
{
	return checkShowPain();
}


qboolean Actor::checkSpecifiedFollowTargetOutOfRange( Conditional &condition )
{
	return checkSpecifiedFollowTargetOutOfRange();
}


qboolean Actor::checkSteeringFailed( Conditional &condition )
{
	return ( state_flags & STATE_FLAG_STEERING_FAILED );
}


qboolean Actor::checkSurfaceDamaged( Conditional &condition )
	{
	int surface_number;
	str surface_name = condition.getParm( 1 );

	if ( last_surface_hit == -1 )
		return false;

	surface_number = gi.Surface_NameToNum( edict->s.modelindex, surface_name.c_str() );

	if ( surface_number == last_surface_hit )
		return true;
	else
		return false;
	}


qboolean Actor::checkTalking( Conditional &condition )
{
	return checkTalking();
}

qboolean Actor::checkTalking()
{
    if (mode == ACTOR_MODE_TALK)
        return true;

    return false;
}



qboolean Actor::checkTurretMode(	Conditional &condition )
	{
	return GetActorFlag( ACTOR_FLAG_TURRET_MODE );
	}


qboolean Actor::checkUsingWeaponNamed( Conditional &condition )
   {
   str weaponName = condition.getParm( 1 );
   return checkUsingWeaponNamed ( weaponName );

   }


qboolean Actor::checkValidCombatNodeInRange( Conditional &condition )
{
	float minDistanceFromPlayer = 96.0f;
	float maxDistanceFromSelf = atof(condition.getParm( 1 ));

	if ( GetActorFlag(ACTOR_FLAG_USE_FOLLOWRANGE_FOR_NODES) )
		{
		if ( enemyManager->HasEnemy() )
			{
			maxDistanceFromSelf = followTarget.maxRangeCombat;
			}
		else
			maxDistanceFromSelf = followTarget.minRangeCombat;
		}

	int unreserveCurrentNode = true; 
	if ( condition.numParms() > 1 )
		minDistanceFromPlayer = atof(condition.getParm( 2 ) );
	if( condition.numParms() > 2 )
		unreserveCurrentNode = atoi( condition.getParm( 3 ) );

	return checkValidCombatNodeInRange( maxDistanceFromSelf , minDistanceFromPlayer, (unreserveCurrentNode != 0) );
}


qboolean Actor::checkValidCoverNodeInRange( Conditional &condition )
{
	float minDistanceFromPlayer = 96.0f;

	float maxDistanceFromSelf = atof(condition.getParm( 1 ));

	if ( GetActorFlag(ACTOR_FLAG_USE_FOLLOWRANGE_FOR_NODES) )
		{
		if ( enemyManager->HasEnemy() )
			{
			maxDistanceFromSelf = followTarget.maxRangeCombat;
			}
		else
			maxDistanceFromSelf = followTarget.minRangeCombat;

		maxDistanceFromSelf = maxDistanceFromSelf * .85;
		}


	float minDistanceFromCurrentEnemy = atof(condition.getParm( 2 ));

	if ( condition.numParms() > 2 )
		minDistanceFromPlayer = atof(condition.getParm(3) );

	return checkValidCoverNodeInRange( maxDistanceFromSelf , minDistanceFromCurrentEnemy, minDistanceFromPlayer );
}


qboolean Actor::checkValidCustomNodeInRange( Conditional &condition )
{
	str customType = condition.getParm( 1 );
	float maxDistanceFromSelf = atof( condition.getParm( 2 ) );

	return checkValidCustomNodeInRange( customType , maxDistanceFromSelf );
}


qboolean Actor::checkValidHibernateNodeInRange( Conditional &condition )
{
	float maxDistanceFromSelf = atof( condition.getParm( 1 ) );
	return checkValidHibernateNodeInRange( maxDistanceFromSelf );
}


qboolean Actor::checkValidPatrolNodeInRange( Conditional &condition )
{
	float maxDistanceFromSelf = atof(condition.getParm( 1 ));
	return checkValidPatrolNodeInRange( maxDistanceFromSelf );
}


qboolean Actor::checkValidSniperNodeInRange( Conditional &condition )
{
	float maxDistanceFromSelf = atof(condition.getParm( 1 ));
	return checkValidSniperNodeInRange( maxDistanceFromSelf );
}


qboolean Actor::checkValidWorkNodeInRange( Conditional &condition )
{
	int unreserveCurrentNode = true; 
	float maxDistanceFromSelf = atof(condition.getParm( 1 ));
	if( condition.numParms() > 1 )
		unreserveCurrentNode = atoi( condition.getParm( 2 ) );
	return checkValidWorkNodeInRange( maxDistanceFromSelf, (unreserveCurrentNode != 0) );
}


qboolean Actor::checkVar( Conditional &condition )
	{
	StateVar	*checkVar = 0;
	str varName			 = condition.getParm( 1 );
	str varValue		 = condition.getParm( 2 );
	
	for (int i = 1; i <= stateVarList.NumObjects() ; i++ )
		{
		checkVar = stateVarList.ObjectAt( i );
		if( !stricmp ( checkVar->varName, varName ) )
			{
			if (checkVar->varValue == varValue ) 
				return true;
			else
				return false;			
			}
		}
	
	return false;
	}


qboolean Actor::checkVarTimeDifference( Conditional &condition )
{
	StateVar	*checkVar = NULL;
	str varName			 = condition.getParm( 1 );
	float varTime		 = atof(condition.getParm( 2 ));
	
	for (int i = 1; i <= stateVarList.NumObjects() ; i++ )
		{
		checkVar = stateVarList.ObjectAt( i );
		if( !stricmp ( checkVar->varName, varName ) )
			{
			if ( level.time - checkVar->varTime >= varTime )
				return true;
			else
				return false;
			}
		}
	
	return false;
}


qboolean Actor::checkWeaponChanged( Conditional &condition )
{
	return ( state_flags & STATE_FLAG_CHANGED_WEAPON );
}


qboolean Actor::checkWeaponIsMelee( Conditional &condition )
{
	return combatSubsystem->WeaponIsFireType( FT_MELEE );
}


qboolean Actor::checkWeaponReady( Conditional &condition	)
	{
	return GetActorFlag( ACTOR_FLAG_WEAPON_READY );
	}


qboolean Actor::checkWithinFollowRangeMin( Conditional &condition )
{
	return checkWithinFollowRangeMin();
}


qboolean Actor::checkactivated( Conditional &condition )
	{
	return state_flags & STATE_FLAG_ACTIVATED;
	}


qboolean Actor::checkallowhangback( Conditional &condition )
   {
	return GetActorFlag( ACTOR_FLAG_ALLOW_HANGBACK );
   }


qboolean Actor::checkatcovernode( Conditional &condtion )
	{
	return GetActorFlag( ACTOR_FLAG_AT_COVER_NODE );
	}


qboolean Actor::checkbehaviordone( Conditional &condition )
   {
	return ( behavior == NULL );
   }


qboolean Actor::checkbehaviorfailed( Conditional &condition )
   {
   if ( behaviorCode == BEHAVIOR_FAILED                              ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_BLOCKED_BY_ENEMY    ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_BLOCKED_BY_CIVILIAN ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_BLOCKED_BY_FRIEND   ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_BLOCKED_BY_TEAMMATE ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_BLOCKED_BY_WORLD    ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_BLOCKED_BY_DOOR     ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_CANNOT_GET_TO_PATH  ||
        behaviorCode == BEHAVIOR_FAILED_STEERING_NO_PATH             
        
      )
      return true;

   return false;
   }


qboolean Actor::checkbehaviorsuccess( Conditional &condition )
   {
   if ( behaviorCode == BEHAVIOR_SUCCESS )
      return true;

   return false;
   }


qboolean Actor::checkblocked( Conditional &condition )
   {
	if ( attack_blocked && ( attack_blocked_time + .75 > level.time ) )
		{
		attack_blocked = false;
		return true;
		}
	else
		return false;
   }


qboolean Actor::checkblockedhit( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_BLOCKED_HIT );
   }


qboolean Actor::checkcanflytoenemy( Conditional &condition )
   {
	trace_t trace;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	trace = G_Trace( origin, mins, maxs, currentEnemy->centroid, this, edict->clipmask, false, "Actor::checkcanflytoenemy" );

	if ( trace.startsolid || trace.allsolid )
		return false;

	if ( trace.entityNum == currentEnemy->entnum )
		return true;

	return false;
   }


qboolean Actor::checkcanjumptoenemy( Conditional &condition )
   {
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	return ( movementSubsystem->CanWalkTo( currentEnemy->origin, 0.0f, currentEnemy->entnum ) );
   }


qboolean Actor::checkchance( Conditional &condition )
	{
	float percent_chance;
	bool checkedChance = false;

	percent_chance = (float)atof( condition.getParm( 1 ) );

	if ( condition.numParms() > 1 )
		{
		int value = atoi( condition.getParm( 2 ) );
		if ( value > 0 )
			checkedChance = true;
		}
		

	if ( checkedChance && _checkedChance )
		return false;


	if ( checkedChance && !_checkedChance )
		_checkedChance = true;			


	return ( G_Random() < percent_chance );
	}


qboolean Actor::checkcommand( Conditional &condition )
	{
	return ( command == condition.getParm( 1 ) );
	}


qboolean Actor::checkdone ( Conditional &condition )
   {
	return ( checkbehaviordone( condition ) || checktimedone( condition ) );
   }


qboolean Actor::checkenemyinfov( Conditional &condition )
	{
	float check_fov;
	float check_fovdot;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;
	
	if ( !sensoryPerception )
		return false;

	if ( condition.numParms() > 0 )
		{
		check_fov = (float)atof( condition.getParm( 1 ) );
		check_fovdot = (float) cos( check_fov * 0.5 * M_PI / 180.0 );

		return sensoryPerception->InFOV( currentEnemy->centroid, check_fov, check_fovdot );
		}
	else
		{
		return sensoryPerception->InFOV( currentEnemy );
		}
	}


qboolean Actor::checkenemyinpath( Conditional &condition )
	{
	trace_t  trace;
	Vector   end_pos;
	int mask;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( IsEntityAlive( currentEnemy ) )
		{
	   Vector forward( orientation[ 0 ] );

		forward *= 1000.0f;
		end_pos = origin + forward;


		if ( edict->clipmask & CONTENTS_BODY )
			mask = edict->clipmask;
		else
			mask = MASK_SHOT;

		trace = G_Trace( centroid, vec_zero, vec_zero, end_pos, this, mask, false, "Actor::checkenemyinpath" );

		if ( trace.entityNum == currentEnemy->entnum )
         {
			return true;
         }
		}

	return false;
	}


qboolean Actor::checkenemymelee( Conditional &condition )
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	return ( EntityHasFireType( currentEnemy, FT_MELEE ) );
	}


qboolean Actor::checkenemyonground( Conditional &condition )
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( currentEnemy->groundentity )
		return true;
	else
		return false;
	}


qboolean Actor::checkenemyrange ( Conditional &condition )
	{
	float range;
	float min_height;
	float max_height;
	int XYOnly;
	bool onlyXY;
	
	Entity *currentEnemy;

	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		enemyManager->FindHighestHateEnemy();
	
	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		enemyManager->FindHighestHateEnemy();

	if ( !currentEnemy )
		return false;

	range = (float)atof( condition.getParm( 1 ) );

	if ( condition.numParms() > 1 )
		max_height = (float)atof( condition.getParm( 2 ) );
   else
		max_height = 0;

	if ( condition.numParms() > 2 )
		min_height = (float)atof( condition.getParm( 3 ) );
   else
		min_height = -max_height;

   if ( condition.numParms() > 3 )
	   XYOnly = atoi( condition.getParm( 4 ) );
   else
	   XYOnly = 0;

	if ( XYOnly )
		onlyXY = true;
	else
		onlyXY = false;

	return EntityInRange( currentEnemy, range, min_height, max_height , onlyXY );

	}


qboolean Actor::checkenemyranged( Conditional &condition )
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	return ( EntityHasFireType( currentEnemy, FT_BULLET ) || EntityHasFireType( currentEnemy, FT_PROJECTILE ) );
	}


qboolean Actor::checkenemyrelativeyaw( Conditional &condition )
   {
	Vector dir;
	Vector dir_angles;
	float relative_yaw;
	float check_yaw_min;
	float check_yaw_max;
	Vector temp_angles;
	qboolean use_range;
	
	Entity *currentEnemy;

   currentEnemy = GetPlayer(0);
	
	check_yaw_min = (float)atof( condition.getParm( 1 ) );

	if ( condition.numParms() > 1 )
		{
		check_yaw_max = atof( condition.getParm( 2 ) );
		use_range = true;
		}
	else
		{
		check_yaw_max = 0;
		use_range = false;
		}

	dir = origin - currentEnemy->origin;

	dir_angles = dir.toAngles();

	temp_angles = currentEnemy->angles;

	relative_yaw = AngleNormalize180( currentEnemy->angles[YAW] - dir_angles[YAW] );

	if (use_range)
		{
		if ( check_yaw_max < check_yaw_min )
			{
			if ( ( relative_yaw < check_yaw_max ) && ( relative_yaw >= -180 ) )
				return true;
			if ( ( relative_yaw > check_yaw_min ) && ( relative_yaw <= 180 ) )
				return true;
			
			return false;
			}
		
		if ( ( relative_yaw >= check_yaw_min ) && ( relative_yaw <= check_yaw_max ) )
			return true;
		else
			return false;
		}

	if ( relative_yaw < check_yaw_min )
		return true;
	else
		return false;
   }


qboolean Actor::checkenemystunned( Conditional &condition )
	{
	Sentient *enemy_shadow9;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( IsEntityAlive( currentEnemy ) )
		{
		if ( currentEnemy->isSubclassOf( Sentient ) )
			{
			enemy_shadow9 = (Sentient *)(Entity *)currentEnemy;

			if ( enemy_shadow9->in_stun )
				return true;
			}
		}

	return false;
	}


qboolean Actor::checkenemyyawrange ( Conditional &condition )
	{
	Vector dir;
	float check_yaw_min;
	float check_yaw_max;
	float dirYaw;
	float originYaw;
	float angleCheck;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;
	
	check_yaw_min = (float)atof( condition.getParm( 1 ) );
	check_yaw_max = (float)atof( condition.getParm( 2 ) );
	
	check_yaw_min = AngleNormalize180(check_yaw_min);
	check_yaw_max = AngleNormalize180(check_yaw_max);

	dir = currentEnemy->origin - origin;
	dirYaw = dir.toYaw();

	originYaw = angles[YAW];
	
	angleCheck = AngleNormalize180(dirYaw - originYaw);

	if ( check_yaw_min > check_yaw_max )
		{
		if ( ( angleCheck < 180.0f ) && ( angleCheck > check_yaw_min ) )
			return true;

		if ( ( angleCheck > -180.0f ) && ( angleCheck < check_yaw_max ) )
			return true;
		}

	if ( ( angleCheck >= check_yaw_min ) && ( angleCheck <= check_yaw_max ) )
		return true;
	else
		return false;
	
	}


qboolean Actor::checkeyebehaviordone( Conditional &condition )
	{
	return ( eyeBehavior == NULL );
	}


qboolean Actor::checkfinished( Conditional &condition )
   {
	return GetActorFlag( ACTOR_FLAG_FINISHED );
   }


qboolean Actor::checkhasthing( Conditional &condition )
   {
	int thing_number;
	int i;


	for( i = 1 ; i <= condition.numParms() ; i++ )
		{
		thing_number = atoi( condition.getParm( i ) );

		switch( thing_number )
			{
			case 1 :
				if ( GetActorFlag( ACTOR_FLAG_HAS_THING1 ) )
					return true;
				break;
			case 2 :
				if ( GetActorFlag( ACTOR_FLAG_HAS_THING2 ) )
					return true;
				break;
			case 3 :
				if ( GetActorFlag( ACTOR_FLAG_HAS_THING3 ) )
					return true;
				break;
			case 4 :
				if ( GetActorFlag( ACTOR_FLAG_HAS_THING4 ) )
					return true;
				break;
			}
		}

	return false;
   }


qboolean Actor::checkheadbehaviordone( Conditional &condition )	
	{
	return ( headBehavior == NULL );
	}


qboolean Actor::checkhealth( Conditional &condition )
	{
	return ( health < (float)atof( condition.getParm( 1 ) ) );
	}


qboolean Actor::checkhealthpercent( Conditional &condition )
{
	float percent = (.01 ) * atof( condition.getParm( 1 ) );
	
	return ( health <= ( percent * max_health ) );
}


qboolean Actor::checkhealthpercentinrange( Conditional &condition )
{
	float minpercent = (.01 ) * atof( condition.getParm( 1 ) );
	float maxpercent = (.01 ) * atof( condition.getParm( 2 ) );
	
	return ( (health >= ( minpercent * max_health ) && (health <= ( maxpercent * max_health ) ) ) );
}


qboolean Actor::checkheld( Conditional &condition )
	{
	return ( edict->s.parent != ENTITYNUM_NONE );
	}


qboolean Actor::checkincomingmeleeattack( Conditional &condition )
	{
	return checkincomingmeleeattack();
	}


qboolean Actor::checkincomingprojectile( Conditional &condition )
	{
	trace_t trace;
	Vector forward;
	Vector end_pos;
	float time = 0;
	float time_left;
	Vector dir;
	float dist;

	if ( condition.numParms() == 1 )
      {
		time = (float)atof( condition.getParm( 1 ) );
      }

	if ( incoming_proj && ( incoming_time <= level.time ) )
		{
		incoming_proj->angles.AngleVectors( &forward );
		end_pos = ( forward * 1000.0f ) + incoming_proj->centroid;
		trace = G_Trace( incoming_proj->centroid, vec_zero, vec_zero, end_pos, incoming_proj, MASK_SHOT, false, "Actor::checkincomingprojectile" );

		if ( trace.entityNum == entnum )
         {
			if ( time )
				{
				dir = trace.endpos - incoming_proj->centroid;
				dist = dir.length();
				time_left = dist / incoming_proj->velocity.length();

				return ( time_left <= time );
				}

			return true;
         }
		}

	return false;
	}


qboolean Actor::checkincomingrangedattack( Conditional &condition )
   {  
	Sentient *enemy_shadow8;
	trace_t trace;
	Vector forward;
	Vector end_pos;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( IsEntityAlive( currentEnemy ) )
		{
		if ( currentEnemy->isSubclassOf( Sentient ) )
			{
			enemy_shadow8 = ( Sentient * )( Entity * )currentEnemy;

			if ( enemy_shadow8->in_ranged_attack )
				{
				enemy_shadow8->angles.AngleVectors( &forward );
				end_pos = ( forward * 125.0f ) + enemy_shadow8->centroid;
				trace = G_Trace( enemy_shadow8->centroid, vec_zero, vec_zero, origin, enemy_shadow8, MASK_SHOT, false, "Actor::checkincomingrangedattack" );

				if ( trace.entityNum == entnum )
               {
					return true;
               }
				}
			}
		}

	return false;   
   }


qboolean Actor::checkinpain( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_IN_PAIN );
   }


qboolean Actor::checkinwater( Conditional &condition )
	{
	return (waterlevel > 0 );
	}


qboolean Actor::checkmeansofdeath( Conditional &condition )
	{
	int mod;

	mod = MOD_NameToNum( condition.getParm( 1 ) );

	return ( mod == means_of_death );
	}


qboolean Actor::checkmeleehit( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_MELEE_HIT );
   }


qboolean Actor::checkmovingactorrange( Conditional &condition )
   {
	float       range;
	float       height = 0;
	float       height_diff;
	Entity      *ent_in_range;
   int         i;
	Vector      delta;
   float       r2;
	gentity_t   *ed;
	float			smallest_dist2;
	float			dist2;


	range = (float)atof( condition.getParm( 1 ) );

	if ( condition.numParms() == 2 )
      {
		height = (float)atof( condition.getParm( 2 ) );
      }

	r2 = range * range;

	if ( ( actorrange_time > level.time ) && ( height == last_height ) )
      {
		ent_in_range = last_ent;

		if ( IsEntityAlive( ent_in_range ) )
			{
			delta = origin - ent_in_range->centroid;

			if ( height )
				{
				height_diff = delta[ 2 ];
				delta[ 2 ] = 0;

				if ( ( height_diff < -height ) || ( height_diff > height ) )
					return false;
				}

			if ( sensoryPerception )
				{
				if ( ( ( delta * delta ) <= r2 ) && sensoryPerception->CanSeeEntity( this, ent_in_range, true, true ))
					return true;

				}
			}

		return false;
      }

   actorrange_time = level.time + 0.2f + G_Random( 0.1f );

	last_height = height;
	last_ent    = NULL;

	smallest_dist2 = 99999999.0f;

	for( i = 0 ; i < game.maxclients; i++ )
      {
      ed = &g_entities[ i ];

		if ( !ed->inuse || !ed->entity )
         {
			continue;
         }

		ent_in_range = ed->entity;

		if ( IsEntityAlive( ent_in_range ) )
         {
         delta = origin - ent_in_range->centroid;

			if ( height > 0.0f )
				{
				height_diff = delta[ 2 ];

				if ( ( height_diff < -height ) || ( height_diff > height ) )
               {
					continue;
               }

				delta[ 2 ] = 0;
				}


			dist2 = delta * delta;

			if ( sensoryPerception )
				{
				if ( ( dist2 <= r2 ) && sensoryPerception->CanSeeEntity( this , ent_in_range, true, true ) )
					{
					if ( dist2 < smallest_dist2 )
						{
						smallest_dist2 = dist2;
						last_ent = ent_in_range;
						}
					}

				}
         }
		}

   for( i = 1; i <= ActiveList.NumObjects(); i++ )
      {
      ent_in_range = ActiveList.ObjectAt( i );

		if (
            ( ent_in_range->movetype != MOVETYPE_NONE ) &&
            ( ent_in_range->movetype != MOVETYPE_STATIONARY ) &&
            ( this != ent_in_range ) &&
            ( ent_in_range->health > 0 ) &&
            !( ent_in_range->flags & FL_NOTARGET )
         )
         {
         delta = origin - ent_in_range->centroid;

			if ( height > 0.0f )
				{
				height_diff = delta[ 2 ];

				if ( ( height_diff < -height ) || ( height_diff > height ) )
               {
					continue;
               }

				delta[ 2 ] = 0.0f;
				}


			dist2 = delta * delta;

			if ( sensoryPerception )
				{
				if ( ( dist2 <= r2 ) && sensoryPerception->CanSeeEntity( this, ent_in_range , true , true ) )
					{
					if ( dist2 < smallest_dist2 )
						{
						smallest_dist2 = dist2;
						last_ent = ent_in_range;
						}
					}

				}
         }
      }

	if ( last_ent )
		return true;

	return false;
	}


qboolean Actor::checkname( Conditional &condition )
	{
	return ( name == condition.getParm( 1 ) );
	}


qboolean Actor::checknoiseheard( Conditional &condition )
	{
	
	str soundTypeStr;
	int soundTypeIdx;

	if ( !sensoryPerception )
		return false;

	if (condition.numParms() > 0 )
		{
		soundTypeStr = condition.getParm( 1 );
		soundTypeIdx = Soundtype_string_to_int( soundTypeStr );
		if ( soundTypeIdx == sensoryPerception->GetLastSoundType())
			{
			sensoryPerception->SetLastSoundType( SOUNDTYPE_NONE );

			return true;
			}
		else
			{
			return false;
			}
		}
		
	return GetActorFlag( ACTOR_FLAG_NOISE_HEARD );
	}


qboolean Actor::checknopath( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_NO_PATH );
   }


qboolean Actor::checknumspawns( Conditional &condition )
	{
	int check_num;

	check_num = atoi( condition.getParm( 1 ) );

	return ( num_of_spawns < check_num );
	}


qboolean Actor::checkonfire( Conditional &condition )
   {
	return on_fire;
   }


qboolean Actor::checkonground( Conditional &condition )
	{
	CheckGround();
	return groundentity != NULL;
	}


qboolean Actor::checkotherdied( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_OTHER_DIED );
   }


qboolean Actor::checkpainpitch( Conditional &condition )
   {
	float check_pitch;

	check_pitch = (float)atof( condition.getParm( 1 ) );

	if ( pain_angles[PITCH] <= check_pitch )
		return true;
	else
		return false;
   }


qboolean Actor::checkpainyaw( Conditional &condition )
   {
	float check_yaw;

	check_yaw = (float)atof( condition.getParm( 1 ) );

	if ( pain_angles[YAW] <= check_yaw )
		return true;
	else
		return false;
   }


qboolean Actor::checkparentrange( Conditional &condition )
	{
	float range;
	float height;
	Actor *act;

	if ( !spawnparent ) return false;


	range = (float)atof( condition.getParm( 1 ) );

	if ( condition.numParms() == 2 )
		height = (float)atof( condition.getParm( 2 ) );
   else
		height = 0.0f;

	act = (Actor*)(Entity*)spawnparent;

		if( EntityInRange( act, range, -height, height ) )
			{
			return true;
			}
	
	
	return false;
	}


qboolean Actor::checkpartdead( Conditional &condition )
	{
	str part_name_shadow3;
	str state_name;
	Actor *part;

	part_name_shadow3 = condition.getParm( 1 );

	part = FindPartActor( part_name_shadow3 );

	if ( !part )
		return false;

	return ( part->deadflag || ( part->health <= 0 ) );
	}


qboolean Actor::checkpartflag( Conditional &condition )
	{
	str part_name_shadow2;
	str flag_name;
	unsigned int flag;
	int current_part;
	part_t *part;
	Entity *partent;
	Actor *partact;

	part_name_shadow2  = condition.getParm( 1 );
	flag_name  = condition.getParm( 2 );

	if ( Q_stricmp( flag_name, "pain" ) == 0 )
      {
		flag = STATE_FLAG_IN_PAIN;
      }
	else if ( Q_stricmp( flag_name, "small_pain" ) == 0 )
      {
		flag = STATE_FLAG_SMALL_PAIN;
      }
	else if ( Q_stricmp( flag_name, "melee_hit" ) == 0 )
      {
		flag = STATE_FLAG_MELEE_HIT;
      }
	else if ( Q_stricmp( flag_name, "touched" ) == 0 )
      {
		flag = STATE_FLAG_TOUCHED;
      }
	else if ( Q_stricmp( flag_name, "activated" ) == 0 )
      {
		flag = STATE_FLAG_ACTIVATED;
      }
	else if ( Q_stricmp( flag_name, "used" ) == 0 )
      {
		flag = STATE_FLAG_USED;
      }
	else if ( Q_stricmp( flag_name, "twitch" ) == 0 )
      {
		flag = STATE_FLAG_TWITCH;
      }
	else
		{
		gi.WDPrintf( "Unknown flag name (%s) in checkpartflag.", flag_name.c_str() );
		flag = 0;
		}

	for( current_part = 1; current_part <= parts.NumObjects(); current_part++ )
		{
		part = &parts.ObjectAt( current_part );

		partent = part->ent;
		partact = (Actor *)partent;

		if ( partact && ( partact->part_name == part_name_shadow2 ) )
			{
			if ( part->state_flags & flag )
            {
				return true;
            }
			}
		}

	return false;
	}


qboolean Actor::checkpartstate( Conditional &condition )
	{
	str part_name_shadow1;
	str state_name;
	Actor *part;

	part_name_shadow1  = condition.getParm( 1 );
	state_name = condition.getParm( 2 );

	part = FindPartActor( part_name_shadow1 );

	return ( part && part->currentState && ( strnicmp( part->currentState->getName(), state_name.c_str(), strlen( state_name.c_str() ) ) == 0 ) );
	}


qboolean Actor::checkplayerrange( Conditional &condition )
	{
	float range;
	float height;
	bool XYOnly;

	Player *player;

	range = (float)atof( condition.getParm( 1 ) );

	if ( condition.numParms() == 2 )
		height = (float)atof( condition.getParm( 2 ) );
   else
		height = 0.0f;

   XYOnly = false;
   if ( condition.numParms() == 3 )
   {
		int XYCheck = atoi(condition.getParm( 3 ) );
		if ( XYCheck )
			XYOnly = true;
   }

	for(int i = 0; i < game.maxclients; i++)
		{
		player = GetPlayer(i);

		if( EntityInRange( player, range, -height, height, XYOnly ) )
			{
			return true;
			}
		}
	
	return false;
	}


qboolean Actor::checkplayerranged( Conditional &condition )
   {
	return checkplayerranged();
   }


qboolean Actor::checksmallpain( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_SMALL_PAIN );
   }


qboolean Actor::checkstage( Conditional &condition )
	{
	return ( stage == (float)atoi( condition.getParm( 1 ) ) );
	}


qboolean Actor::checkstatetime( Conditional &condition )
	{
	float time_to_wait;

	time_to_wait = (float)atof( condition.getParm( 1 ) );

	return ( state_time + time_to_wait < level.time );
	}


qboolean Actor::checkstuck( Conditional &condition )
   {
	return ( state_flags & STATE_FLAG_STUCK );
   }


qboolean Actor::checkstunned( Conditional &condition )
   {
	return GetActorFlag( ACTOR_FLAG_STUNNED );
   }


qboolean Actor::checktimedone( Conditional &condition )
   {
	if ( GetActorFlag( ACTOR_FLAG_STATE_DONE_TIME_VALID ) )
      {
		return ( state_done_time < level.time );
      }

	return false;
   }


qboolean Actor::checktimesdone( Conditional &condition )
	{
	return ( times_done == atoi( condition.getParm( 1 ) ) );
	}


qboolean Actor::checktorsobehaviordone( Conditional &condition )
	{
	return ( torsoBehavior == NULL );
	}


qboolean Actor::checktorsobehaviorfailed( Conditional &condition )
{
	return ( 
			(torsoBehaviorCode != BEHAVIOR_SUCCESS) 
		&&	(torsoBehaviorCode != BEHAVIOR_EVALUATING )
		);
}


qboolean Actor::checktorsobehaviorsuccess( Conditional &condition )
{
	return (torsoBehaviorCode == BEHAVIOR_SUCCESS);
}


qboolean Actor::checktouched( Conditional &condition )
	{
	return state_flags & STATE_FLAG_TOUCHED;
	}


qboolean Actor::checktouchedbyplayer ( Conditional &condition )
   {
	return checktouchedbyplayer();
   }


qboolean Actor::checktwitch( Conditional &condition )
	{
	return ( state_flags & STATE_FLAG_TWITCH );
	}


qboolean Actor::checkused( Conditional &condition )
	{
	return ( state_flags & STATE_FLAG_USED );
	}








bool Actor::ReleaseControl(Listener *controller)
{
    if (!_controller) {
        return false;
    }
    if (_controller != controller) {
        return false;
    }

    _controller  = NULL;
    _controlType = ACTOR_CONTROL_NONE;
    return true;
}
















qboolean Actor::CheckBottom( void )
	{
	Vector	start, stop;
	trace_t	trace;
	int		x, y;
	int		corners_ok;
	int		middle_ok;
	float		width;
	int			mask;


	width = maxs[0];


	start = origin - Vector( 0.0f, 0.0f, 1.0f );

	if ( gi.pointcontents( start, 0 ) == CONTENTS_SOLID )
		return true;


	corners_ok = 0;

	start[ 2 ] = absmin[ 2 ] - 1.0f;

	for( x = 0; x <= 1; x++ )
		{
		for( y = 0; y <= 1; y++ )
			{
			start[ 0 ] = x ? absmax[ 0 ] : absmin[ 0 ];
			start[ 1 ] = y ? absmax[ 1 ] : absmin[ 1 ];

			if ( gi.pointcontents( start, 0 ) == CONTENTS_SOLID )
				corners_ok++;
			}
		}

	if ( corners_ok >= 3 )
		return true;


	corners_ok = 0;
	middle_ok = 0;


	start = origin;
	stop = start - Vector( 0.0f, 0.0f, width ); 


	mask = edict->clipmask & ~CONTENTS_BODY;

	trace = G_Trace( start, vec_zero, vec_zero, stop, this, mask, false, "CheckBottom 1" );

	if ( trace.fraction < 1.0f && trace.plane.normal[2] > .7f )
		middle_ok = 1;


	for( x = 0; x <= 1; x++ )
		{
		for( y = 0; y <= 1; y++ )
			{
			start[ 0 ] = x ? absmax[ 0 ] : absmin[ 0 ];
			start[ 1 ] = y ? absmax[ 1 ] : absmin[ 1 ];
			start[ 2 ] = origin[ 2 ];

			stop = start - Vector( 0.0f, 0.0f, 2.0f * width );

			trace = G_Trace( start, vec_zero, vec_zero, stop, this, mask, false, "CheckBottom 2" );

			if ( ( trace.fraction < 1.0f ) && ( trace.plane.normal[2] > .7f ) )
				corners_ok++;

			if ( ( middle_ok && corners_ok >= 1 ) || ( corners_ok >= 3 ) )
				return true;
			}
		}

	return false;
	}





int Actor::NotifyFlag_string_to_int( const str &notifyflagstr )
   {
	extern char actor_notify_strings[ACTOR_FLAG_MAX][32];
	str test;

	for (int i = 0; i < NOTIFY_FLAG_MAX; i++)
		{
		test = actor_notify_strings[i];
		if ( !notifyflagstr.icmp( actor_notify_strings[ i ] ) )
			return i;
		}

	gi.WDPrintf( "Unknown Notify Flag - %s\n", notifyflagstr.c_str() );
	return -1;
   }







void Actor::SetMinPainTime( Event *ev )
{
	SetMinPainTime( ev->GetFloat( 1 ) );
}



void Actor::WhatsWrong( Event *ev )
{
	gi.Printf( "\n------------------------------------------" );

	
	if ( !behaviorFailureReason.length() )
		gi.Printf( "\nNo Failure Reason given for behavior %s\n", currentBehavior.c_str() );			
	else
		gi.Printf( "\nReason: %s" , behaviorFailureReason.c_str() );		


	
	gi.Printf( "\n------------------------------------------" );

}




qboolean Actor::returntrue( Conditional &condition )
   {
   return true;
   }




void Actor::setHeadTwitch( Event *ev )
{
	headWatcher->setHeadTwitch( ev->GetBoolean( 1 ) );
}





Entity* Group::GetNextMember( Entity *entity )
{
	Entity *ent;
	
	if ( _memberList.NumObjects() == 0 )
		return NULL;
	
	if ( !entity )
		return _memberList.ObjectAt( 1 );
	
	
	for ( int i = 1 ; i <= _memberList.NumObjects() ; i++ )
	{
		ent = _memberList.ObjectAt( i );
		
		if ( i == _memberList.NumObjects() )
			return NULL;
		
		if ( ent == entity )
			return _memberList.ObjectAt( i + 1 );
	}
	
	return NULL;
}






unsigned int Actor::ActorTypeStringToInt( const str &type )
{
	unsigned int retValue = 99999;

	if ( type ==  "inanimate" )
		retValue = IS_INANIMATE;
	else if ( type == "monster" )
		retValue = IS_MONSTER;
	else if ( type == "enemy" )
		retValue = IS_ENEMY;
	else if ( type == "civilian" )
		retValue = IS_CIVILIAN;
	else if ( type == "friend" )
		retValue = IS_FRIEND;
	else if ( type == "animal" )
		retValue = IS_ANIMAL;
	else if ( type == "teammate" )
		retValue = IS_TEAMMATE;

	assert( retValue != 99999 );
	return retValue;
}



qboolean Actor::checkhaveenemy(Conditional &condition)
{
    Entity *currentEnemy;

    currentEnemy = enemyManager->GetCurrentEnemy();

    if (currentEnemy && IsEntityAlive(currentEnemy)) {
        return true;
    }

    if (enemyManager->getEnemyCount()) {
        return true;
    }

    return false;
}

qboolean Actor::checkcanseeenemy(Conditional &condition)
{
    qboolean use_fov = true;
    qboolean can_see;
    qboolean in_fov;
    qboolean real_can_see;

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();

    if (!currentEnemy) {
        enemyManager->FindHighestHateEnemy();
    }

    currentEnemy = enemyManager->GetCurrentEnemy();
    if (!currentEnemy) {
        return false;
    }

    if (condition.numParms() > 0) {
        use_fov = atoi(condition.getParm(1));
    }

    if (canseeenemy_time > level.time) {
        if (use_fov) {
            return GetActorFlag(ACTOR_FLAG_LAST_CANSEEENEMY);
        } else {
            return GetActorFlag(ACTOR_FLAG_LAST_CANSEEENEMY_NOFOV);
        }
    }

    can_see = true;
    in_fov = true;

    if (!IsEntityAlive(currentEnemy)) {
        can_see = false;
        in_fov = false;
    }

    if (sensoryPerception) {
        if (can_see && !sensoryPerception->CanSeeEntity(this, currentEnemy, true, true)) {
            can_see = false;
        }
    }

    SetActorFlag(ACTOR_FLAG_LAST_CANSEEENEMY, can_see && in_fov);
    SetActorFlag(ACTOR_FLAG_LAST_CANSEEENEMY_NOFOV, can_see);

    canseeenemy_time = level.time + 0.2f + G_Random(0.1f);

    if (use_fov) {
        real_can_see = GetActorFlag(ACTOR_FLAG_LAST_CANSEEENEMY);
    } else {
        real_can_see = GetActorFlag(ACTOR_FLAG_LAST_CANSEEENEMY_NOFOV);
    }

    if (real_can_see) {
        last_known_enemy_pos = currentEnemy->centroid;
    }

    return real_can_see;
}

qboolean Actor::checkcanshootenemy(Conditional &condition)
{
    str tag_name;

    Entity *currentEnemy;
    currentEnemy = enemyManager->GetCurrentEnemy();
    if (!currentEnemy) {
        return false;
    }

    return combatSubsystem->CanAttackTarget(currentEnemy);
}

qboolean Actor::checkWantsToExecutePackage(Conditional &condition)
{
    float interval;

    if (condition.numParms() > 0) {
        interval = atof(condition.getParm(1));
    } else {
        interval = 0.0f;
    }

    return personality->WantsToExecuteCurrentPackage(interval);
}

qboolean Actor::checkExecutedPackageInLastTimeFrame(Conditional &condition)
{
    float interval;

    if (condition.numParms() > 0) {
        interval = atof(condition.getParm(1));
    } else {
        interval = 0.0f;
    }

    return personality->ExecutedPackageInLastTimeFrame(interval);
}

qboolean Actor::checkPatrolWaypointNodeInDistance(Conditional &condition)
{
    float distance = atof(condition.getParm(1));

    Entity *ent_in_range;
    Vector NodeToSelf;
    gentity_t *ed;

    Vector pos;
    Vector nodeOrigin;

    for (int i = 0; i < MAX_GENTITIES; i++) {
        ed = &g_entities[i];

        if (!ed->inuse || !ed->entity) {
            continue;
        }

        ent_in_range = g_entities[i].entity;

        pos = origin;
        pos.z += 80;

        nodeOrigin = ent_in_range->origin;

        if (ent_in_range->isSubclassOf(PatrolWayPointNode)) {
            NodeToSelf = ent_in_range->origin - origin;
            if (NodeToSelf.length() <= distance)
                return true;
        }
    }

    return false;
}






void Actor::ActivateAI( void )
	{
	if ( !statemap && !fuzzyEngine && !masterstatemap)
		return;
	
	last_time_active = level.time;
	
	if ( (mode  == ACTOR_MODE_AI) || mode == ACTOR_MODE_TALK )
		return;

	StartMode( ACTOR_MODE_AI );
   
   if ( fuzzyEngine )
      SetState( "START" );

	if ( sensoryPerception )
		sensoryPerception->RespondTo(STIMULI_ALL , true );
	
	if (activate_thread.length() )
		RunThread(activate_thread);
	}

void Actor::ActivateEvent(	Event *ev )
	{
	if ( ( deadflag ) && ( actortype != IS_INANIMATE ) )
		{
		return;
		}

	ProcessEvent( EV_Actor_AttackPlayer );

	AddStateFlag( STATE_FLAG_ACTIVATED );
	}

void Actor::AddCustomThread( const str& threadType , const str& threadName )
{
	threadlist_t* threadListEntry;

	if ( threadList.NumObjects() )
		{
		for ( int i = 1; i <= threadList.NumObjects() ; i++ )
			{
			threadListEntry = threadList.ObjectAt( i );
			if ( !stricmp(threadListEntry->threadType.c_str() , threadType.c_str() ) )
				{
				threadListEntry->threadName = threadName;
				return;
				}
			}
		}

	threadListEntry = new threadlist_t;

	threadListEntry->threadType = threadType;
	threadListEntry->threadName = threadName;

	threadList.AddObject( threadListEntry );	

}

void Actor::AddDialog( Event *ev )
   {
	DialogNode_t *dialog_node;
	DialogNode_t *new_node;

	new_node = NewDialogNode();

	if (new_node != NULL)
	   {
		strcpy(new_node->alias_name, ev->GetString( 1 ));

		AddDialogParms( new_node, ev );

		if ( dialog_list == NULL )
			{
			new_node->next = NULL;
			dialog_list = new_node;
			return;
			}

		dialog_node = dialog_list;
		while ( dialog_node->next != NULL )
			{	
			dialog_node = dialog_node->next;			
			}

		dialog_node->next = new_node;
		new_node->next = NULL;
		return;
   		}
   }

void Actor::AddSpawnItem( Event *ev )
	{
	str spawn_item_name;

	spawn_item_name = ev->GetString( 1 );

	spawn_items.AddObject( spawn_item_name );
	}

void Actor::AnimateOnce( Event *ev )
{
	animate->RandomAnimate( ev->GetString( 1 ), EV_StopAnimating );
}

void Actor::Anim( Event *ev )
	{
	Event *e;

	if ( !ModeAllowed( ACTOR_MODE_SCRIPT ) )
		{
		if ( mode == ACTOR_MODE_TALK )
			PostEvent( *ev, FRAMETIME );
		else if ( mode == ACTOR_MODE_AI )
			SendMoveDone( ev->GetThread() );
		return;
		}

	if ( ( deadflag ) && ( actortype != IS_INANIMATE ) )
		{
		return;
		}

	e = new Event( EV_Behavior_Args );

	e->SetSource( ev->GetSource() );

	if ( ev->GetSource() == EV_FROM_SCRIPT )
		{
		StartMode( ACTOR_MODE_SCRIPT );

		e->SetThread( ev->GetThread() );
		e->SetLineNumber( ev->GetLineNumber() );
		}

	e->AddToken( ev->GetToken( 1 ) );

	SetBehavior( new PlayAnim, e, ev->GetThread() );
	}

void Actor::BounceOffEvent( Event *ev )
	{
	Vector object_origin;
	Vector object_angles;

	if ( bounce_off_effect.length() )
		{
		object_origin = ev->GetVector( 1 );

		object_angles = object_origin - centroid;
		object_angles.toAngles();

		SpawnEffect( bounce_off_effect , object_origin, object_angles , 5.0f );		
		}
	}

bool Actor::canBeDamagedBy(meansOfDeath_t MeansOfDeath)
{
	float resistance;
	resistance = GetResistanceModifier( MeansOfDeath );

	if ( resistance >= 100.0 )
		return false;

	if ( currentBaseArmor )
		{
		return currentBaseArmor->CanBeDamagedBy( MeansOfDeath );
		}

	return true;
}

void Actor::ChargeWater( Event *ev )
	{
	int cont;
	float damage;
	float radius;
	Entity *ent;
	int brushnum;
	int entity_brushnum;
	float real_damage;
	Vector dir;
	float dist;


	cont = gi.pointcontents( origin, 0 );

	if ( !(cont & MASK_WATER) )
		return;


	damage = ev->GetFloat( 1 );
	radius = ev->GetFloat( 2 );

	if ( !damage || !radius )
		return;


	brushnum = gi.pointbrushnum( origin, 0 );


	ent = NULL;

	for( ent = findradius( ent, origin, radius ) ; ent ; ent = findradius( ent, origin, radius ) )
		{
		if ( ent->takedamage )
			{
			entity_brushnum = gi.pointbrushnum( origin, 0 );

			if ( brushnum == entity_brushnum )
				{
				dir = ent->origin - origin;
				dist = dir.length();

				if ( dist < radius )
					{
					real_damage = damage - ( damage * ( dist / radius ) );
					ent->Damage( this, this, real_damage, origin, dir, vec_zero, 0, 0, MOD_ELECTRICWATER );
					}
				}
			}
		}
	}

void Actor::Chatter( const char *snd, float chance, float volume,	int channel	)
	{
   str realname;

	if ( GetActorFlag( ACTOR_FLAG_NOCHATTER ) || ( chattime > level.time ) )
		{
		return;
		}

	if ( G_Random( 10.0f ) > chance )
		{
		chattime = level.time + 1.0f + G_Random( 2.0f );
		return;
		}

   realname = GetRandomAlias( snd );
	if ( realname.length() > 1 )
		{
      float delay;

      delay = gi.SoundLength( realname.c_str() );

		if ( delay < 0.0f )
			gi.WDPrintf( "Lip file not found for dialog %s\n", realname.c_str() );

		chattime = level.time + delay + 4.0f + G_Random( 5.0f );
		Sound( realname, channel, volume );
		}
	else
		{
		chattime = level.time + 1.0f;
		}
	}

qboolean Actor::checkActivationDelayTime()
{
	if ( level.time >= activationStart + activationDelay )
		return true;

	return false;
}

void Actor::checkActorDead( Event *ev )
	{
	
	Actor* act = (Actor*)ev->GetEntity( 1 );

	if ( act )
		ev->ReturnInteger( act->checkActorDead() );
	else
		ev->ReturnInteger( false );
	}

qboolean Actor::checkActorDead( )
	{
	if ( deadflag || ( health <= 0 ) )
		return true;

	return false;
	}

qboolean Actor::checkAllowedToMeleeEnemy()
{
	Entity *enemy_shadow7 = enemyManager->GetCurrentEnemy();

	if ( !enemy_shadow7  ) return false;

	if ( enemy_shadow7->isSubclassOf(Player) ) return true;

	if ( !enemy_shadow7->isSubclassOf(Actor) ) return false;


	Actor *actor = (Actor*)enemy_shadow7;
	if ( actor->GetActorFlag(ACTOR_FLAG_MELEE_ALLOWED) )
		return true;

	return false;
	
}

qboolean Actor::checkAttacked()
{
	return state_flags & STATE_FLAG_ATTACKED;	
}

qboolean Actor::checkAttackedByPlayer()
{
	return state_flags & STATE_FLAG_ATTACKED_BY_PLAYER;
}

qboolean Actor::checkCanAttackEnemy()
{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

   return combatSubsystem->CanAttackTarget( currentEnemy );
}

qboolean Actor::checkCountOfIdenticalNamesInGroup( const str &checkName , int checkValue )
{
	ActorGroup* group;	
	int count;

	group = (ActorGroup*)groupcoordinator->GetGroup( GetGroupID() );
	
	if ( !group )
		return true;

	count = group->CountMembersWithThisName( checkName );

	if ( count < checkValue )
		return true;

	return false;
}

qboolean Actor::checkDamageThresholdExceeded()
{
	return state_flags & STATE_FLAG_DAMAGE_THRESHOLD_EXCEEDED;	
}

qboolean Actor::checkEnemyCanSeeCurrentNode()
{
	trace_t trace;
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentHelperNode.node )
		return false;

	if ( !currentEnemy )
		{
		enemyManager->FindClosestEnemy();
		currentEnemy = enemyManager->GetCurrentEnemy();
		}

	if ( !currentEnemy )
		return false;

	if ( currentEnemy->isSubclassOf(Sentient) )
		{
		Sentient* theEnemy;
		theEnemy = (Sentient*)currentEnemy;
		trace = G_Trace( currentHelperNode.node->origin , vec_zero , vec_zero , theEnemy->EyePosition() , NULL , MASK_OPAQUE, false, "CoverCombatWithRangedWeapon::think");
		}
	else
		{
		trace = G_Trace( currentHelperNode.node->origin , vec_zero , vec_zero , currentEnemy->centroid , NULL , MASK_OPAQUE, false, "CoverCombatWithRangedWeapon::think");
		}
	

	if ( trace.fraction >= .95 )
		return true;

	return false;
}

qboolean Actor::checkEnemyProjectileClose()
{
	return ( state_flags & STATE_FLAG_ENEMY_PROJECTILE_CLOSE );
}

qboolean Actor::checkEnemyWeaponNamed( const str& weaponName )
{
	Entity *enemy_shadow6;
	enemyManager->FindHighestHateEnemy();
	enemy_shadow6 = enemyManager->GetCurrentEnemy();

	if ( enemy_shadow6 )
	{
		if ( enemy_shadow6->isSubclassOf( Actor ) )
		{
		Actor *act;
		act = (Actor*)enemy_shadow6;
		return act->combatSubsystem->UsingWeaponNamed( weaponName );
		}

		if ( enemy_shadow6->isSubclassOf( Player ) )
		{
		Player *player;
		Weapon *pWeapon;
		player = (Player*)enemy_shadow6;

		pWeapon = player->GetActiveWeapon(WEAPON_DUAL);
		if ( pWeapon && pWeapon->getName() == weaponName )
			return true;

		pWeapon = player->GetActiveWeapon(WEAPON_LEFT);		
		if ( pWeapon && pWeapon->getName() == weaponName )
			return true;

		pWeapon = player->GetActiveWeapon(WEAPON_RIGHT);
		if ( pWeapon && pWeapon->getName() == weaponName )
			return true;

		}
	}

	return false;

}

qboolean Actor::checkEnemyWithinRange( float min , float max )
{
	float dist = enemyManager->GetDistanceFromEnemy();

	return ( dist <= max && dist >= min );
}

qboolean Actor::checkHaveArmor()
{
	if ( !currentBaseArmor )
		return false;

	if ( currentBaseArmor->getAmount() <= 0 )
		return false;

	return true;
}

qboolean Actor::checkHaveBestWeapon()
{
	str bestWeaponName;
	float powerRating;
	str currentPostureState;
	Entity *currentEnemy;
	Weapon *bestWeapon;

	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return true;

	powerRating = combatSubsystem->GetActiveWeaponPowerRating( currentEnemy );
	if ( powerRating < .05 )
		{
		}

	bestWeapon = combatSubsystem->GetBestAvailableWeapon( currentEnemy );
	if( !bestWeapon )
		return false;

	if ( combatSubsystem->GetActiveWeaponName() == bestWeapon->getName() )
		return true;

	return false;

}

qboolean Actor::checkHavePathToEnemy()
{
	if ( !enemyManager->HasEnemy() )
		{
		_nextCheckForEnemyPath = 0;
		return false;
		}

	if ( level.time >= _nextCheckForEnemyPath )
		{
		Entity *currentEnemy = enemyManager->GetCurrentEnemy();
		if ( !currentEnemy ) return false;

		FindMovementPath find;
		Path *path;

		_nextCheckForEnemyPath = G_Random() + DEFAULT_PATH_TO_ENEMY_INTERVAL;
		find.heuristic.self = this;
		find.heuristic.setSize( size );
		find.heuristic.entnum = entnum;

		path = find.FindPath( origin, currentEnemy->origin );		
		
		if ( !path )
			{
			_havePathToEnemy = false;
			return false;
			}

		delete path;
		path = NULL;

		
		_havePathToEnemy = true;
		}

	return _havePathToEnemy;
}

qboolean Actor::checkHelperNodeWithFlagInRange( const str &flag, float range )
{
	int mask;
	mask = HelperNode::GetHelperNodeMask( flag );

	return HelperNode::isHelperNodeInRange( *this , mask , range );
}

qboolean Actor::checkincomingmeleeattack()
   {
	Sentient *enemy_shadow5;
	trace_t trace;
	Vector forward;
	Vector end_pos;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( IsEntityAlive( currentEnemy ) )
		{
		if ( currentEnemy->isSubclassOf( Sentient ) )
			{
			enemy_shadow5 = ( Sentient * )( Entity * )currentEnemy;

			if ( enemy_shadow5->in_melee_attack )
				{
				enemy_shadow5->angles.AngleVectors( &forward );
				end_pos = ( forward * 160.0f ) + enemy_shadow5->centroid;
				trace = G_Trace( enemy_shadow5->centroid, vec_zero, vec_zero, end_pos, enemy_shadow5, MASK_SHOT, false, "Actor::checkincomingmeleeattack" );

				if ( trace.entityNum == entnum )
               {
					return true;
               }
				}
			}
		}

	return false;   
   }

qboolean Actor::checkInTheWay()
{
	if ( state_flags & STATE_FLAG_IN_THE_WAY )
		return true;

	if ( state_flags & STATE_FLAG_TOUCHED_BY_PLAYER )
		return true;

	return false;
	
}

qboolean Actor::checkplayerranged()
	{
	Player *player;
	player = NULL;
	player = GetPlayer( 0 );

	if ( !player ) return false;

	return ( EntityHasFireType( player, FT_BULLET ) || EntityHasFireType( player, FT_PROJECTILE ) );
	}

qboolean Actor::checkPlayerWeaponNamed( const str& weaponName )
{
	Player *player;
	player = GetPlayer ( 0 );

	if ( player )
	{
		Weapon *pWeapon;

		pWeapon = player->GetActiveWeapon(WEAPON_DUAL);
		if ( pWeapon && pWeapon->getName() == weaponName )
			return true;

		pWeapon = player->GetActiveWeapon(WEAPON_LEFT);		
		if ( pWeapon && pWeapon->getName() == weaponName )
			return true;

		pWeapon = player->GetActiveWeapon(WEAPON_RIGHT);
		if ( pWeapon && pWeapon->getName() == weaponName )
			return true;
	}

	return false;

}

qboolean Actor::checkPosture( const str &postureName )
{
	if ( postureController->getCurrentPostureName() == postureName )
		return true;

	return false;
}

qboolean Actor::checkPropEnemyRange( const str& objname , const str& propname )
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str scopestr = getArchetype() + "." + objname;
	if ( !gpm->hasObject(scopestr) )
		return false;

	float range;
	if ( propname.length() )
		range = gpm->getFloatValue(scopestr, propname);
	else
		range = gpm->getFloatValue(scopestr, "value");

	Entity *currentEnemy;

	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		enemyManager->FindHighestHateEnemy();
	
	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		enemyManager->FindHighestHateEnemy();

	if ( !currentEnemy )
		return false;

	return EntityInRange( currentEnemy, range, 0, 0 , false );	
}

qboolean Actor::checkRightDirectionClear(float dist)
   {
   trace_t trace;   

   Vector endPos;
   Vector startPos;
   Vector left;
   Vector angles_shadow2;

   startPos = origin;
   
   angles_shadow2 = movementSubsystem->getAnimDir();  
   angles_shadow2 = angles_shadow2.toAngles();

   angles_shadow2[YAW] = AngleNormalize180(angles_shadow2[YAW] + 180);
   angles_shadow2.AngleVectors( NULL, &left, NULL );

   endPos = ( left * dist) + startPos;

   trace = Trace( endPos , "CheckMyRight" );

   if (trace.fraction == 1.0 )
		{
	   return ( movementSubsystem->CanWalkTo( trace.endpos, 0.0f, entnum ) );		
		 }
   
   return false;     
   
   }

qboolean Actor::checkShouldDoAction( const str &tendencyName )
{
	float tendency = personality->GetTendency( tendencyName );

	return ( G_Random() < tendency );
}

qboolean Actor::checkShowPain()
{
	return state_flags & STATE_FLAG_SHOW_PAIN;
}

qboolean Actor::checkSpecifiedFollowTargetOutOfRange()
{
	if ( !followTarget.specifiedFollowTarget )
		followTarget.specifiedFollowTarget = GetPlayer( 0 );

	if ( followTarget.specifiedFollowTarget )
		{
		float range = followTarget.maxRangeIdle;
		Entity *enemy_shadow4;

		enemy_shadow4 = enemyManager->GetCurrentEnemy();
		if ( !enemy_shadow4 )
			{
			enemyManager->FindHighestHateEnemy();
			enemy_shadow4 = enemyManager->GetCurrentEnemy();
			}

		if ( enemy_shadow4 )
			range = followTarget.maxRangeCombat;
		
		if ( EntityInRange( followTarget.specifiedFollowTarget, range, 0, 0 , false ) )
			return false;
		else
			return true;			
		}

	return false;	
}

qboolean Actor::checktouchedbyplayer()
	{
	return state_flags & STATE_FLAG_TOUCHED_BY_PLAYER;	
	}

qboolean Actor::checkValidCombatNodeInRange( float maxDistanceFromSelf , float minDistanceFromPlayer, bool unreserveCurrentNode )
{
	HelperNode* coverNode = NULL;
	
	if ( currentHelperNode.node && (currentHelperNode.node->target.length()))
		{
		coverNode = currentHelperNode.node->GetTargetedHelperNode(currentHelperNode.node->target);
		if ( coverNode )
			{
			if ( coverNode->isOfType( NODETYPE_COMBAT ) )
				{
				currentHelperNode.node->UnreserveNode();
				currentHelperNode.node = coverNode;
				currentHelperNode.mask = NODETYPE_COMBAT;
				currentHelperNode.node->ReserveNode();
				return true;
				}
			}
		}

	coverNode = HelperNode::FindClosestHelperNode(*this , NODETYPE_COMBAT , maxDistanceFromSelf, minDistanceFromPlayer, unreserveCurrentNode);

	if ( coverNode )
		return true;

	return false;
}

qboolean Actor::checkValidCoverNodeInRange( float maxDistanceFromSelf, float minDistanceFromCurrentEnemy, float minDistanceFromPlayer )
{
	HelperNode* coverNode = NULL;
	Entity* currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		{
		enemyManager->FindHighestHateEnemy();
		currentEnemy = enemyManager->GetCurrentEnemy();

		if ( !currentEnemy )
			return false;
		}

	coverNode = HelperNode::FindClosestHelperNodeThatCannotSeeEntity(*this , NODETYPE_COVER , edict->clipmask, maxDistanceFromSelf, minDistanceFromCurrentEnemy, currentEnemy , minDistanceFromPlayer );

	if ( coverNode )
		return true;

	return false;
}

qboolean Actor::checkValidCustomNodeInRange( const str &customType , float maxDistanceFromSelf )
{
	HelperNode* customNode = NULL;
	
	customNode = HelperNode::FindClosestHelperNode(*this , customType , maxDistanceFromSelf);

	if ( customNode )
		return true;

	return false;
}

qboolean Actor::checkValidPatrolNodeInRange( float maxDistanceFromSelf )
{
	HelperNode* patrolNode = NULL;
	
	patrolNode = HelperNode::FindClosestHelperNode(*this , NODETYPE_PATROL , maxDistanceFromSelf, 0);

	if ( patrolNode )
		return true;

	return false;
}

qboolean Actor::checkValidSniperNodeInRange( float maxDistanceFromSelf )
{
	HelperNode* sniperNode = NULL;
	
	sniperNode = HelperNode::FindClosestHelperNode(*this , NODETYPE_SNIPER , maxDistanceFromSelf);

	if ( sniperNode )
		return true;

	return false;
}

qboolean Actor::checkValidWorkNodeInRange( float maxDistanceFromSelf, bool unreserveCurrentNode )
{
	HelperNode* coverNode = NULL;

	if ( level.time < _nextCheckForWorkNodeTime )
		return false;

	coverNode = HelperNode::FindClosestHelperNode(*this , NODETYPE_WORK , maxDistanceFromSelf , 0.0f, unreserveCurrentNode );
	_nextCheckForWorkNodeTime = ( G_Random() + level.time + 0.50f );

	if ( coverNode )
		return true;

	return false;
}

qboolean Actor::checkWithinFollowRangeMin()
{
	if ( !followTarget.specifiedFollowTarget )
		followTarget.specifiedFollowTarget = GetPlayer( 0 );

	if ( followTarget.specifiedFollowTarget )
		{
		float range = followTarget.minRangeIdle;
		Entity *enemy_shadow3;

		enemy_shadow3 = enemyManager->GetCurrentEnemy();
		if ( !enemy_shadow3 )
			{
			enemyManager->FindHighestHateEnemy();
			enemy_shadow3 = enemyManager->GetCurrentEnemy();
			}

		if ( enemy_shadow3 )
			range = followTarget.minRangeCombat;
		
		if ( EntityInRange( followTarget.specifiedFollowTarget, range, 0, 0 , false ) )
			{
			if ( level.time > _nextPathDistanceToFollowTargetCheck )
				{
				FindMovementPath find;
				Path *path;
				float pathLen;

				_nextPathDistanceToFollowTargetCheck = G_Random(.33 ) + DEFAULT_PATH_TO_ENEMY_INTERVAL + level.time;

				find.heuristic.self = this;
				find.heuristic.setSize( size );
				find.heuristic.entnum = entnum;

				path = find.FindPath( origin, followTarget.specifiedFollowTarget->origin );		
				
				if ( !path )
					{
					return true;
					}

				pathLen = path->Length();

				delete path;
				path = NULL;
				

				if ( pathLen > range )
					return false;
				else
					return true;

				}
			}			
		else
			return false;			
		}

	return false;
}

void Actor::ChildSetAnim( const str &childName , const str &animName )
{
	Actor *childActor = 0;
	
	childActor = GetAttachedChildActor(childName);
	if ( childActor )
		childActor->SetAnim(animName);
}

void Actor::ChildUseBehaviorPackage( const str &childName , const str &packageName )
{
	Actor *childActor = 0;
	
	childActor = GetAttachedChildActor(childName);
	if ( childActor )
		childActor->SetBehaviorPackage( packageName );
}

void Actor::clearBranchDialog( void )
{
	_branchDialogName = "";
}

void Actor::ClearOnUseThread( Event *ev )
	{
	onuse_thread_name = "";
	}

void Actor::ClearSpawnItems( Event *ev )
	{
	spawn_items.ClearObjectList();
	}

void Actor::ClearStateFlags( void )
	{
	int current_part;
	part_t *part;



	state_flags = 0;


	for ( current_part = 1 ; current_part <= parts.NumObjects() ; current_part++ )
		{
		part = &parts.ObjectAt( current_part );

		part->state_flags = 0;
		}

	movementSubsystem->clearBlockingEntity();
	}

void Actor::ClearTorsoAnim()
{
	newTorsoAnimNum = -1;
	newTorsoAnim = "";
	newTorsoAnimEvent = NULL;
	TorsoAnimName = "";
	animate->ClearTorsoAnim();
}


qboolean Actor::CloseToEnemy ( const Vector &pos, float howclose	)
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( !IsEntityAlive( currentEnemy ) )
		{
		return false;
		}

	if ( WithinDistance( currentEnemy, howclose ) )
		{
		return true;
		}

	return false;
	}

void Actor::DamageAllowed( Event *ev )
   {
	SetActorFlag( ACTOR_FLAG_DAMAGE_ALLOWED, ev->GetBoolean( 1 ) );
	}

void Actor::DamageEnemy( Event *ev )
	{
	Entity *currentEnemy;
	Vector dir;

	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	dir = currentEnemy->origin - origin;
	dir.normalize();

	float damage = 0.0f;
	float knockback = 0.0f;
	
	if ( ev->NumArgs() > 0 )
		damage = ev->GetFloat( 1 );

	if ( ev->NumArgs() > 1 )
		{
		str modelName = ev->GetString( 2 );

		Event *attachEvent = new Event(EV_SpawnEffect);
		if ( !attachEvent )
			return;

		attachEvent->AddString( modelName );
		attachEvent->AddString( "Bip01" );
		attachEvent->AddFloat( 2.5 );
		
		currentEnemy->ProcessEvent( attachEvent );
		}


	if ( ev->NumArgs() > 2 )
		knockback = ev->GetFloat( 3 );


	if ( damage > 0.0f )
		currentEnemy->Damage( this, this, damage, vec_zero, dir, vec_zero, (int)knockback, 0, MOD_CRUSH );

	}

void Actor::DamageOnceStart( Event *ev )
   {
	SetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_ON, true );
	SetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_DAMAGED, false );
	}

void Actor::DamageOnceStop( Event *ev )
   {
	SetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_ON, false );
	}

void Actor::DamageSelf( Event *ev )
	{
	float damage  = 0;
	str MOD_String = "crush";
	int attack_means_of_death;
	Event* event;

	if (ev->NumArgs() > 0 )
		damage = ev->GetFloat( 1 );

	if (ev->NumArgs() > 1 )
		MOD_String = ev->GetString( 2 );

	attack_means_of_death = MOD_NameToNum( MOD_String );
	
	event = new Event( EV_Damage );
	event->AddFloat(damage);
	event->AddEntity(this);
	event->AddEntity(this);
	event->AddVector(vec_zero);
	event->AddVector(vec_zero);
	event->AddVector(vec_zero);
	event->AddInteger( 0 );
	event->AddInteger( 0 );
	event->AddInteger( attack_means_of_death );
	ProcessEvent( event );
	}

void Actor::DialogDone( Event *ev )
   {
	SetActorFlag( ACTOR_FLAG_DIALOG_PLAYING, false );
	SetActorFlag( ACTOR_FLAG_RADIUS_DIALOG_PLAYING, false );

	if ( dialog_state_name )
		{
		dialog_state_name = "";

		if ( ( mode != ACTOR_MODE_AI ) && ( mode != ACTOR_MODE_TALK ) )
			{
			if ( dialog_old_state_name.length() )
				SetState( dialog_old_state_name.c_str() );
			}
		}
   }



void Actor::_dropActorToGround()
	{
	trace_t	trace;
	Vector	end;
   Vector   start;
	qboolean stuck;
	str      actor_name;
	
	stuck = false;

   start = origin + Vector( "0 0 1" );
   end = origin;
	end[ 2 ] -= 16;

	
	trace = G_Trace( start, mins, maxs, end, this, edict->clipmask, false, "Actor::start" );

	if ( trace.startsolid || trace.allsolid )
		stuck = true;
	else if ( !( flags & FL_FLY ) )
		{
		setOrigin( trace.endpos );
		groundentity = trace.ent;
		}

	if ( name.length() )
		actor_name = name;
	else
		actor_name = getClassID();

	if ( trace.fraction == 1 && movetype == MOVETYPE_STATIONARY && !GetActorFlag( ACTOR_FLAG_IGNORE_OFF_GROUND_WARNING ) )
		gi.WDPrintf( "%s (%d) off of ground at '%5.1f %5.1f %5.1f'\n", actor_name.c_str(), entnum, origin.x, origin.y, origin.z );
	
	if ( stuck && !GetActorFlag( ACTOR_FLAG_IGNORE_STUCK_WARNING ) )
		{
		groundentity = world->edict;
		gi.WDPrintf( "%s (%d) stuck in world at '%5.1f %5.1f %5.1f'\n", actor_name.c_str(), entnum, origin.x, origin.y, origin.z );
		}

	SetActorFlag( ACTOR_FLAG_HAVE_MOVED, false );

	last_origin = origin;
	last_ground_z = origin.z;
	}

qboolean Actor::EntityHasFireType( Entity *ent, firetype_t fire_type	)
	{
	Player *player;
	Weapon *weapon;
	firetype_t weapon_fire_type;


	if ( !ent )
		return false;

	if ( !ent->isSubclassOf( Player ) )
		return true;

	player = (Player *)(Entity *)ent;


	weapon = player->GetActiveWeapon( WEAPON_LEFT );

	if ( weapon )
		{
		weapon_fire_type = weapon->GetFireType( FIRE_MODE1 );

		if ( weapon_fire_type == fire_type )
			return true;
		}


	weapon = player->GetActiveWeapon( WEAPON_RIGHT );

	if ( weapon )
		{
		weapon_fire_type = weapon->GetFireType( FIRE_MODE1 );

		if ( weapon_fire_type == fire_type )
			return true;
		}


	weapon = player->GetActiveWeapon( WEAPON_DUAL );

	if ( weapon )
		{
		weapon_fire_type = weapon->GetFireType( FIRE_MODE1 );

		if ( weapon_fire_type == fire_type )
			return true;
		}

	return false;
	}

qboolean Actor::EntityInRange ( Entity *ent,	float range, float min_height, float max_height , bool XYOnly )
	{
	float    r;
   Vector   delta , cent , enemyCent;
	float    height_diff;



	if ( !IsEntityAlive( ent ) )
      {
		return false;
      }

	cent = centroid;
	enemyCent = ent->centroid;

	if ( XYOnly )
		{
		cent.z = 0.0f;
		enemyCent.z = 0.0f;
		}

   delta = enemyCent - cent;

	if ( ( max_height != 0 ) || ( min_height != 0 ) )
		{
		height_diff = delta[ 2 ];

		if ( ( height_diff < min_height ) || ( height_diff > max_height ) )
         {
			return false;
         }

		delta[ 2 ] = 0;
		}

	r = delta * delta;

	return ( r < ( range * range ) );
	}

void Actor::EyeOffset (	Event *ev )
	{
   eyeposition -= eyeoffset;
   eyeoffset = ev->GetVector( 1 );
   eyeposition += eyeoffset;
   }

const str Actor::FindDialog( Sentient *user, DialogType_t dialogType , const str& context )
{
	DialogNode_t *dialog_node;
	int good_dialog;
	ScriptVariable *script_var;
	const char* the_dialog;
	str dialog_name;
	bool usingContext = false;

	dialog_node = dialog_list;

	while(dialog_node != NULL)
		{
		good_dialog = true;

		if ( dialogType == DIALOG_TYPE_NORMAL )
			{
			if ( dialog_node->dType != DIALOG_TYPE_NORMAL )
				{
				dialog_node = dialog_node->next;
				continue;
				}
			}
		
		if ( dialogType == DIALOG_TYPE_RADIUS )
			{
			if ( dialog_node->dType != DIALOG_TYPE_RADIUS )
				{
				dialog_node = dialog_node->next;
				continue;
				}
			}

		if ( dialogType == DIALOG_TYPE_GREETING )
			{
			if ( dialog_node->dType != DIALOG_TYPE_GREETING )
				{
				dialog_node = dialog_node->next;
				continue;
				}
			}

		if ( dialogType == DIALOG_TYPE_COMBAT )
			{
			if ( dialog_node->dType != DIALOG_TYPE_COMBAT )
				{
				dialog_node = dialog_node->next;
				continue;
				}
			}

		if ( dialogType == DIALOG_TYPE_CONTEXT_INITIATOR || dialogType == DIALOG_TYPE_CONTEXT_RESPONSE )
			{
			if ( dialog_node->number_of_parms < 1 )
				{
				dialog_node = dialog_node->next;
				continue;
				}
			}

		for(int i = 0 ; i < dialog_node->number_of_parms ; i++)
			{
			switch(dialog_node->parms[ i ].type)
   				{
				case DIALOG_PARM_TYPE_PLAYERHAS :
					if (!user || !user->HasItem(dialog_node->parms[ i ].parm))
						good_dialog = false;
					break;
				case DIALOG_PARM_TYPE_PLAYERHASNOT :
					if (!user || user->HasItem(dialog_node->parms[ i ].parm))
						good_dialog = false;
					break;
				case DIALOG_PARM_TYPE_HAS :
					if (!HasItem(dialog_node->parms[ i ].parm))
						good_dialog = false;
					break;
				case DIALOG_PARM_TYPE_HASNOT :
					if (HasItem(dialog_node->parms[ i ].parm))
						good_dialog = false;
					break;
				case DIALOG_PARM_TYPE_DEPENDS :
					script_var = NULL;

					if ( strnicmp( dialog_node->parms[ i ].parm, "game.", 5 ) == 0 )
						script_var = gameVars.GetVariable( dialog_node->parms[ i ].parm + 5 );
					else if ( strnicmp( dialog_node->parms[ i ].parm, "level.", 6 ) == 0 )
						script_var = levelVars.GetVariable( dialog_node->parms[ i ].parm + 6 );
					
					if ( !script_var || !script_var->intValue() )
						good_dialog = false;

					break;
				case DIALOG_PARM_TYPE_DEPENDSNOT :
					script_var = NULL;

					if ( strnicmp( dialog_node->parms[ i ].parm, "game.", 5 ) == 0 )
						script_var = gameVars.GetVariable( dialog_node->parms[ i ].parm + 5 );
					else if ( strnicmp( dialog_node->parms[ i ].parm, "level.", 6 ) == 0 )
						script_var = levelVars.GetVariable( dialog_node->parms[ i ].parm + 6 );
					
					if ( script_var && script_var->intValue() )
						good_dialog = false;

					break;
				case DIALOG_PARM_TYPE_DEPENDSINT :
					script_var = NULL;

					if ( strnicmp( dialog_node->parms[ i ].parm, "game.", 5 ) == 0 )
						script_var = gameVars.GetVariable( dialog_node->parms[ i ].parm + 5 );
					else if ( strnicmp( dialog_node->parms[ i ].parm, "level.", 6 ) == 0 )
						script_var = levelVars.GetVariable( dialog_node->parms[ i ].parm + 6 );
					
					if ( !script_var || ( script_var->intValue() != atoi( dialog_node->parms[ i ].parm2 ) ) )
						good_dialog = false;

					break;

					case DIALOG_PARM_TYPE_CONTEXT_INITIATOR:
						if ( dialogType != DIALOG_TYPE_CONTEXT_INITIATOR )
							good_dialog = false;

						if ( stricmp( dialog_node->parms[i].parm , context.c_str() ) )
							good_dialog = false;

						usingContext = true;
					break;

					case DIALOG_PARM_TYPE_CONTEXT_RESPONSE:
						if ( dialogType != DIALOG_TYPE_CONTEXT_RESPONSE )
							good_dialog = false;

						if ( stricmp( dialog_node->parms[i].parm , context.c_str() ) )
							good_dialog = false;

						usingContext = true;
					break;
	   				}


				if (!good_dialog)
					break;
			}


			if ( ( dialog_node->random_percent < 1.0f ) && ( G_Random() > dialog_node->random_percent ) )
				good_dialog = false;
			
			if ( dialogType == DIALOG_TYPE_CONTEXT_INITIATOR || dialogType == DIALOG_TYPE_CONTEXT_RESPONSE )
			{
				if ( !usingContext )
					good_dialog = false;
			}

			if (good_dialog)
				{
				the_dialog = NULL;
				the_dialog = gi.Alias_FindDialog( edict->s.modelindex, dialog_node->alias_name, dialog_node->random_flag, entnum );
			
				if ( the_dialog )
					{
					dialog_name = the_dialog;
					break;
					}
				}

			dialog_node = dialog_node->next;
		}

	return dialog_name;
}

void Actor::FireBullet( Event *ev )
	{
	str tag_name;
	qboolean use_current_pitch;
	float range = 1000;
	float damage;
	float knockback;
	str means_of_death_string;
	int attack_means_of_death;
	Vector spread;
	Vector pos;
	Vector forward;
	Vector left;
	Vector right;
	Vector up;
	Vector attack_angles;
	Vector dir;
	Vector enemy_angles;

	tag_name						= ev->GetString( 1 );
	use_current_pitch			= ev->GetBoolean( 2 );
	damage						= ev->GetFloat( 3 );
	knockback					= ev->GetFloat( 4 );
	means_of_death_string	= ev->GetString( 5 );
	spread						= ev->GetVector( 6 );

	if ( ev->NumArgs() > 6 )
		range = ev->GetFloat( 7 );

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	

	GetTag( tag_name, &pos, &forward, &left, &up );

	right = left * -1.0f;


	if ( !use_current_pitch && currentEnemy )
		{
		attack_angles = forward.toAngles();

		dir = currentEnemy->centroid - pos;
		dir.z += 10;

		enemy_angles = dir.toAngles();

		attack_angles[PITCH] = enemy_angles[PITCH];

		enemy_angles.AngleVectors( &forward, &left, &up );

		right = left * -1.0f;
		}

	if ( shotsFired > 1000 )
	   shotsFired = 1000;

	attack_means_of_death = MOD_NameToNum( means_of_death_string );

	BulletAttack( pos, forward, right, up, range, damage, knockback, 0, attack_means_of_death, spread, 1, this, NULL );

	SaveAttack( pos, forward );
	}

void Actor::FireProjectile( Event *ev )
	{
	Vector orig;
	Vector dir;
	str tag_name;
	str projectile_name;
	int number_of_tags = 1;
	qboolean arc = false;
	float speed = 0.0f;
	float offset = 0.0f;
	bool leadTarget = false;
	float spread = 0.0f;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( !currentEnemy )
		enemyManager->FindHighestHateEnemy();

	if ( !currentEnemy )
		return;


	tag_name        = ev->GetString( 1 );
	projectile_name = ev->GetString( 2 );

	if ( ev->NumArgs() > 2 )
		number_of_tags = ev->GetInteger( 3 );

	if ( ev->NumArgs() > 3 )
		arc = ev->GetBoolean( 4 );

	if ( ev->NumArgs() > 4 )
		speed = ev->GetFloat( 5 );

	if ( ev->NumArgs() > 5 )
		offset = ev->GetFloat( 6 );

	if ( ev->NumArgs() > 6 )
		leadTarget = ev->GetBoolean( 7 );

	if ( ev->NumArgs() > 7 )
		spread = ev->GetFloat( 8 );



	if ( !GetClosestTag( tag_name, number_of_tags, currentEnemy->centroid, &orig ) )
		{
		orig[0] = centroid[0];
		orig[1] = centroid[1];
		orig[2] = centroid[2];
		}


		[[maybe_unused]] Vector targetVelocity = currentEnemy->velocity;
		Vector targetPos = currentEnemy->centroid;
		Vector newTargetPos = targetPos;

	if ( leadTarget )
		{
		float projSpeed = speed;
		if ( projSpeed <= 0 )
			projSpeed = 1;

		newTargetPos = combatSubsystem->GetLeadingTargetPos( projSpeed , currentEnemy->centroid , currentEnemy );
		}


	if ( spread > 0 )
		{
		newTargetPos.x = newTargetPos.x + ( G_CRandom(spread) );
		newTargetPos.y = newTargetPos.y + ( G_CRandom(spread) );
		newTargetPos.z = newTargetPos.z + ( G_CRandom(spread) );
		}

		dir = newTargetPos - orig;

	if ( arc )
		{
		Vector xydir;
		float traveltime;
		float vertical_speed;
		Vector proj_velocity;

		xydir = dir;
		xydir.z = 0.0f;

		if ( speed == 0.0f )
			speed = 500.0f;

		traveltime = xydir.length() / speed;

      vertical_speed = ( dir.z  / traveltime ) + ( 0.5f * gravity * sv_currentGravity->value * traveltime );

		xydir.normalize();

		proj_velocity = speed * xydir;
		proj_velocity.z = vertical_speed;

		speed = proj_velocity.length();
		proj_velocity.normalize();	
		dir = proj_velocity;
		}
   
	if ( offset )
		{
		Vector offset_angle = dir.toAngles();
		offset_angle[YAW] += offset;		
		offset_angle.AngleVectors( &dir );		
		}



	dir.normalize();

	ProjectileAttack( orig, dir, this, projectile_name.c_str(), 1.0f, speed );

	SaveAttack( orig, dir );
	}

void Actor::FireRadiusAttack ( Event *ev )
   {
   str tagName        = ev->GetString( 1 );
   str meansOfDeath  = ev->GetString( 2 );
   float damage      = ev->GetFloat( 3 );
   float radius      = ev->GetFloat( 4 );
   float knockback   = ev->GetFloat( 5 );
   qboolean constant = ev->GetBoolean( 6 );

   int MOD = MOD_NameToNum( meansOfDeath );
   RadiusDamage( this , this, damage, this, MOD, radius, knockback, constant );

   }

void Actor::FlyEvent( Event *ev )
   {
	if ( ev->NumArgs() == 0 )
		{
		flags &= ~FL_SWIM;
		flags |= FL_FLY;		
		}
	else
		{
		if ( ev->GetBoolean( 1 ) )
			{
			flags &= ~FL_SWIM;
			flags |= FL_FLY;			
			}
		else
			{
			flags &= ~FL_FLY;
			}
		}
   }

float Actor::GetDialogRemainingTime( void )
   {
	if ( GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) )
		{
		return dialog_done_time - level.time;
		}
	else
		{
		return 0;
		}
   }

void Actor::GetStage( Event *ev )
	{
	ev->ReturnFloat( (float)stage );
	}

const str Actor::GetStateVar( const str& varName )
{
	StateVar			*checkVar = 0;
		
	for (int i = 1; i <= stateVarList.NumObjects() ; i++ )
		{
		checkVar = stateVarList.ObjectAt( i );
		if( checkVar->varName == varName )
			{
				return checkVar->varValue;
			}
		}
	
	return "";
}

void Actor::GotoNextStage( Event *ev )
	{
	stage++;
	}

void Actor::GotoPrevStage( Event *ev )
	{
	stage--;

	if ( stage < 1 )
		stage = 1;
	}

void Actor::GotoStage( Event *ev )
	{
	stage = ev->GetInteger( 1 );
	}

void Actor::IfCanHideAtEvent( Event *ev )
	{
	PathNode *node;
	Vector pos;
	CThread *thread;
   bool result;

	thread = ev->GetThread();
	assert( thread );
	if ( !thread )
		{
		return;
		}

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;

	pos = ev->GetVector( 1 );
	node = thePathManager.NearestNode( pos, this );

   result = false;
	if ( sensoryPerception )
		{
		if ( node && ( node->nodeflags & ( AI_DUCK | AI_COVER ) ) && 
			!sensoryPerception->CanSeeEntity( pos , currentEnemy , true , true ) )
			{
			if ( !node->entnum || ( node->entnum == entnum ) || ( node->occupiedTime < level.time ) )
				{
				result = true;
				}
			}
		}

   ev->ReturnInteger( result );
	}


void Actor::IfEnemyVisibleEvent( Event *ev )
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();

	if ( currentEnemy )
		ev->ReturnInteger( sensoryPerception->CanSeeEntity( this , currentEnemy, true, true ) );	
	else
		ev->ReturnInteger( false );

	
	}

void Actor::IfEnemyWithinEvent( Event *ev	)
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		{
      ev->ReturnInteger( false );
		return;
		}

   ev->ReturnInteger( WithinDistance( currentEnemy, ev->GetFloat( 1 ) ) );
	}

void Actor::IfNearEvent( Event *ev )
	{
	CThread        *thread;
	Entity			*ent;
	Entity			*bestent;
	float				bestdist;
	float				dist;
	str				classname;
   Vector         delta;
	float				distance;
	TargetList		*tlist;
	int				n;
	int				i;

	thread = ev->GetThread();
	assert( thread );
	if ( !thread )
		{
		return;
		}

	classname = ev->GetString( 1 );
	distance = ev->GetFloat( 2 );

	if ( classname[ 0 ] == '*' )
		{
		ent = ev->GetEntity( 1 );
      ev->ReturnInteger( WithinDistance( ent, distance ) );
		}
	else if ( classname[ 0 ] == '$' )
		{
		bestent = NULL;
		bestdist = distance * distance;

		tlist = world->GetTargetList( str( &classname[ 1 ] ) );
		n = tlist->list.NumObjects();
		for( i = 1; i <= n; i++ )
			{
			ent = tlist->list.ObjectAt( i );
         delta = centroid - ent->centroid;
			dist = delta * delta;
			if ( dist <= bestdist )
				{
				bestent = ent;
				bestdist = dist;
				}
			}

      ev->ReturnInteger( ( bestent != NULL ) );
		}
	else
		{
		bestent = NULL;
		bestdist = distance * distance;

		ent = NULL;

		for( ent = findradius( ent, origin, distance ) ; ent ; ent = findradius( ent, origin, distance ) )
			{
			if ( ent->inheritsFrom( classname.c_str() ) )
				{
            delta = centroid - ent->centroid;
			   dist = delta * delta;
				if ( dist <= bestdist )
					{
					bestent = ent;
					bestdist = dist;
					}
				}
			}

      ev->ReturnInteger( bestent != NULL );
		}
	}

void Actor::IgnoreMonsterClip( Event *ev )
	{
	edict->clipmask &= ~CONTENTS_MONSTERCLIP;
	}


void Actor::IgnorePainFromActors( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_IGNORE_PAIN_FROM_ACTORS, true );
	}

void Actor::IgnorePlacementWarning( Event *ev )
	{
	str warning;

	warning = ev->GetString( 1 );

	if ( warning == "stuck" )
		SetActorFlag( ACTOR_FLAG_IGNORE_STUCK_WARNING, true );
	else if ( warning == "off_ground" )
		SetActorFlag( ACTOR_FLAG_IGNORE_OFF_GROUND_WARNING, true );
	}

void Actor::IgnoreWater( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_IGNORE_WATER, ev->GetBoolean( 1 ) );
	}


void Actor::IncomingProjectile( Event *ev )
	{
	incoming_proj = ev->GetEntity( 1 );
	incoming_time = level.time + .1f; 
	}

void Actor::InContext( const str &theContext , bool useDefaultMinDist )
{
   Event *ignoreEvent;   
   Event *broadcastEvent;   
   str realDialog;
   float dialogLength;

   if ( !WantsToTalk() ) return;

   if ( GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) ) return;
   
   if ( _nextContextTime > level.time ) return;

   if ( _ignoreNextContext )
	{
	   if ( !stricmp( theContext.c_str() , _nextContextToIgnore.c_str() ) )
		{
		_ignoreNextContext = false;		
		_nextContextToIgnore = "";
		return;
	   }
	}

	realDialog = FindDialog( this, DIALOG_TYPE_CONTEXT_INITIATOR, theContext );

	if ( !realDialog.length() )
		return;

	ignoreEvent = new Event ( EV_ContextDialog_IgnoreNextContext );
	ignoreEvent->AddInteger( 1 );
	ignoreEvent->AddString( theContext );
	groupcoordinator->SendEventToGroup( ignoreEvent , GetGroupID() );

	char localizedDialogName[MAX_QPATH];
	gi.LocalizeFilePath( realDialog, localizedDialogName );

	_nextContextTime = level.time + G_Random() + _contextInterval;

	dialogLength = gi.SoundLength( localizedDialogName );
	broadcastEvent = new Event ( EV_Actor_BroadcastDialog );
	broadcastEvent->AddString( theContext );
	PostEvent( broadcastEvent, dialogLength );


	if ( useDefaultMinDist )
		PlayDialog( this, DEFAULT_VOL, DEFAULT_MIN_DIST, realDialog.c_str() , NULL );
	else
		PlayDialog( this, DEFAULT_VOL, CONTEXT_WIDE_MIN_DIST, realDialog.c_str() , NULL );
	
}

void Actor::InitPostureController()
{
	postureController = NULL;
	postureController = new PostureController( this );

	if ( !postureController )
		gi.Error( ERR_FATAL, "Actor Could not create postureController" );
}

void Actor::MeleeEvent( Event *ev )
   {
	Vector pos;
	Vector end;
	Vector   dir;
	float damage = 20;
	qboolean success;
	str tag_name;
	Vector attack_vector;
	float attack_width  = 0;
	float attack_max_height = 0;
	float attack_min_height = 0;
	float attack_length = 100;
	str means_of_death_string;
	meansOfDeath_t attack_means_of_death;
	float knockback;
	qboolean use_pitch_to_enemy = false;
	float attack_final_height;

	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();


	if ( GetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_ON ) && GetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_DAMAGED ) )
		{
		if ( gi.cvar("qa_probe", "0", 0)->integer )
			gi.Printf( "[GAMEPLAY:melee] ent=%d '%s' GATED mod=%s dmg=%.2f flag=damage_once_already_damaged\n",
					   entnum, model.c_str(), ( ev->NumArgs() > 2 ) ? ev->GetString( 3 ) : "-",
					   ( ev->NumArgs() > 0 ) ? ev->GetFloat( 1 ) : 0.0f );
		return;
		}

	if ( !GetActorFlag( ACTOR_FLAG_DAMAGE_ALLOWED ) )
		{
		if ( gi.cvar("qa_probe", "0", 0)->integer )
			gi.Printf( "[GAMEPLAY:melee] ent=%d '%s' GATED mod=%s dmg=%.2f flag=damage_not_allowed\n",
					   entnum, model.c_str(), ( ev->NumArgs() > 2 ) ? ev->GetString( 3 ) : "-",
					   ( ev->NumArgs() > 0 ) ? ev->GetFloat( 1 ) : 0.0f );
		return;
		}


	if ( ev->NumArgs() > 0 )
		damage = ev->GetFloat( 1 );

	if ( _useWeaponDamage != WEAPON_ERROR )
		{
		Weapon *weap = GetActiveWeapon(_useWeaponDamage);
		if ( weap )
			damage = weap->GetBulletDamage();
		}

	if ( ev->NumArgs() > 1 )
		tag_name = ev->GetString( 2 );

	if ( ev->NumArgs() > 2 )
		means_of_death_string = ev->GetString( 3 );

	if ( ev->NumArgs() > 3 )
		{
		attack_vector = ev->GetVector( 4 );

		attack_width      = attack_vector[0];
		attack_length     = attack_vector[1];
		attack_max_height = attack_vector[2];
		}

	if ( ev->NumArgs() > 4 )
		{
		knockback = ev->GetFloat( 5 );
		}
	else
		{
		knockback = damage * 8.0f;
		}

	if ( ev->NumArgs() > 5 )
		{
		use_pitch_to_enemy = ev->GetInteger( 6 );
		}

	if ( ev->NumArgs() > 6 )
		attack_min_height = ev->GetFloat( 7 );
	else
		attack_min_height = -attack_max_height;

	if ( ev->NumArgs() > 7 )
		attack_final_height = ev->GetFloat( 8 );
	else
		attack_final_height = 50.0f;

	if ( tag_name.length() && GetTag( tag_name.c_str(), &pos, &dir ) )
		{
		end = pos + ( dir * attack_length );
		}
	else
		{
		pos = centroid;
		dir = orientation[0];
		dir.normalize();
		end = pos + ( dir * attack_length );

		if ( attack_length )
			end[2] += attack_final_height;
		}

	if ( use_pitch_to_enemy )
		{
		if ( currentEnemy )
			{
			Vector enemy_dir;
			Vector angles_shadow1;
			Vector enemy_angles;
			float length;

			dir    = end - pos;
			length = dir.length();
			angles_shadow1 = dir.toAngles();

			enemy_dir    = currentEnemy->centroid - pos;
			enemy_angles = enemy_dir.toAngles();

			angles_shadow1[PITCH] = enemy_angles[PITCH];
			angles_shadow1.AngleVectors( &dir );
			end = pos + ( dir * length );
			}
		}

	if ( means_of_death_string.length() > 0 )
		attack_means_of_death = (meansOfDeath_t)MOD_NameToNum( means_of_death_string );
	else
		attack_means_of_death = MOD_CRUSH;

	Weapon *weap = 0;
	if ( _useWeaponDamage != WEAPON_ERROR )
		weap = GetActiveWeapon(_useWeaponDamage);

	if ( weap )
		success = MeleeAttack( pos, end, damage, this, attack_means_of_death, attack_width, attack_min_height, attack_max_height, knockback, true, NULL, weap );
	else
		success = MeleeAttack( pos, end, damage, this, attack_means_of_death, attack_width, attack_min_height, attack_max_height, knockback );

	if ( success )
		{
		AddStateFlag( STATE_FLAG_MELEE_HIT );

		RunCustomThread( "meleehit" );

		if ( GetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_ON ) )
			SetActorFlag( ACTOR_FLAG_DAMAGE_ONCE_DAMAGED, true );
		}
	}



qboolean Actor::ModeAllowed( int new_mode )
	{
	if ( deadflag && ( actortype != IS_INANIMATE ) )
		return false;

	if ( ( new_mode == ACTOR_MODE_SCRIPT ) || ( new_mode == ACTOR_MODE_IDLE ) )
		{
		if ( ( mode == ACTOR_MODE_AI ) || ( mode == ACTOR_MODE_TALK ) )
			return false;
		}
	else if ( new_mode == ACTOR_MODE_TALK )
		{
		if ( GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) )
			return false;

		if (  ( mode == ACTOR_MODE_TALK ) || ( actortype == IS_ENEMY ) || !GetActorFlag( ACTOR_FLAG_ALLOW_TALK ) ||
			  !dialog_list || level.cinematic )
			return false;
		}

	return true;
	}


PathNode *Actor::NearestNodeInPVS (	const Vector &pos	)
	{
	Vector	delta;
	PathNode	*node;
	PathNode	*bestnode;
	float		bestdist;
	float		dist;
	int		number_nodes;
	int		i;
	MapCell	*cell;
	Vector	min;
	Vector	max;


	cell = thePathManager.GetMapCell( pos );

	if ( !cell )
		return NULL;

	number_nodes = cell->NumNodes();

	bestnode = NULL;
	bestdist = 999999999.0f; 

	for( i = 0; i < number_nodes; i++ )
		{
		node = ( PathNode * )cell->GetNode( i );

		if ( !node )
			continue;

      delta = node->origin - pos;

		dist = delta * delta;

      if ( ( dist < bestdist ) && gi.inPVS( node->origin, (Vector)pos ) )
			{
			bestnode = node;
			bestdist = dist;

         if ( dist < 16.0f )
            break;
			}
		}

	return bestnode;
	}

void Actor::NoChatterEvent( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_NOCHATTER, true );
	}

void Actor::NoMask( Event *ev )
	{
	edict->clipmask = 0;
	}


void Actor::NoPainSounds( Event *ev	)
	{
	SetActorFlag( ACTOR_FLAG_NO_PAIN_SOUNDS, true );
	}

void Actor::NotLandEvent( Event *ev )
   {
   flags &= FL_SWIM | FL_FLY;
   }

void Actor::NotSolidMask( Event *ev )
	{
	edict->clipmask = MASK_SOLID;
	}


void Actor::Pain( Event *ev )
	{
	float damage;
	[[maybe_unused]] Entity *ent;
	int mod;
	Vector direction;
	Vector position;
	Vector dir;
	State	*tempState;
	bool	showPain;


	damage		= ev->GetFloat( 1 );
	ent			= ev->GetEntity( 2 );
	mod			= ev->GetInteger( 3 );
	position		= ev->GetVector( 4 );
	direction	= ev->GetVector( 5 );

	if ( ev->NumArgs() > 5 )
		{
		showPain = ev->GetBoolean( 6 );
		}
	else
		showPain = false;



	
	if ( deadflag )
		{

		if ( statemap )
			{
			tempState = statemap->FindState( "GIB" );

			if ( tempState )
				tempState = tempState->Evaluate( *this, &conditionals );

			if ( tempState )
				{
				tempState->ProcessEntryCommands( this );
				}
			}
		return;
		}

	if ( ( damage > 0.0f ) && ( next_pain_sound_time <= level.time ) && !GetActorFlag( ACTOR_FLAG_NO_PAIN_SOUNDS ) )
		{
		next_pain_sound_time = level.time + 0.4f + G_Random( 0.2f );
		BroadcastSound();
		}

	
	if ( level.time >= _nextPlayPainSoundTime && !GetActorFlag( ACTOR_FLAG_NO_PAIN_SOUNDS ))
		{
		_nextPlayPainSoundTime = level.time + _playPainSoundInterval;
		Sound( "snd_generalpain" );
		}

	if ( ( mod == MOD_BULLET ) && behavior && ( currentBehavior == "Pain" ) )
		{
		bullet_hits++;
		}


	AddStateFlag( STATE_FLAG_SMALL_PAIN );

	if ( damage < pain_threshold )
		{
		if ( G_Random( 1.0f ) < ( damage / pain_threshold ) )
			{
			if ( ( means_of_death == MOD_SWORD ) || ( means_of_death == MOD_AXE ) || ( means_of_death == MOD_FIRESWORD ) ||
				  ( means_of_death == MOD_ELECTRICSWORD ) )
				pain_type = PAIN_BIG;
			else
				pain_type = PAIN_SMALL;
			AddStateFlag( STATE_FLAG_IN_PAIN );
			}
		}
	else
		{
		pain_type = PAIN_BIG;
		AddStateFlag( STATE_FLAG_IN_PAIN );
		}

	
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( gpm->hasObject(getArchetype()) )
		{
		str propName = MOD_NumToName(mod);
		float painChanceForActor = 0.0f;
		
		if ( propName.length() )
			{
			str objName;
			objName = getArchetype();
			objName += ".PainChance";

			if ( gpm->hasObject( objName ) )
				{
				if ( gpm->hasProperty( objName , propName ) )
					painChanceForActor = gpm->getFloatValue( objName, propName );
				}
			}

		if ( G_Random() <= painChanceForActor )
			showPain = true;
		else 
			showPain = false;
		}

	if ( ( level.time >= next_forced_pain_time ) || ( level.time >= next_pain_time && showPain ) )
		{
		next_pain_time = level.time + min_pain_time;
		next_forced_pain_time = level.time + max_pain_time;
		AddStateFlag( STATE_FLAG_SHOW_PAIN );
		}
		


	if ( Vector( position - centroid ).length() > 1.0f )
		dir = centroid - position;
	else
		dir = direction;

	dir = dir * -1.0f;

	pain_angles = dir.toAngles();

	pain_angles[YAW]   = AngleNormalize180( angles[YAW] - pain_angles[YAW] );
	pain_angles[PITCH] = AngleNormalize180( angles[PITCH] - pain_angles[PITCH] );
	}

void Actor::Pickup( Event *ev )
   {
	str tag_name;
	int tag_num;
	Vector new_angles;

	tag_name = ev->GetString( 1 );
	tag_num  = gi.Tag_NumForName( edict->s.modelindex, tag_name.c_str() );

	new_angles = Vector(0, 0, 0);

	if ( pickup_ent )
		{
		pickup_ent->setAngles( new_angles );
		pickup_ent->attach( entnum, tag_num );
		}
	}

void Actor::PlayDialog(	Event *ev )
	{
	Sentient *user = NULL;
	const char *dialog_name = NULL;
	const char *state_name = NULL;
	float volume   = DEFAULT_VOL;
	float min_dist = DEFAULT_MIN_DIST;
	qboolean headDisplay = false;
	bool useTalk = false;

	if (ev->NumArgs() > 0)
	{
		dialog_name = ev->GetString( 1 );
		
		if ( strcmp( dialog_name, "" ) == 0 )
			dialog_name = NULL;
	}

	if ( ev->NumArgs() > 1 )
		volume = ev->GetFloat( 2 );

	if ( ev->NumArgs() > 2 )
	{
		str minDistString;

		min_dist = ev->GetFloat( 3 );

		minDistString = ev->GetString( 3 );

		if ( stricmp( minDistString.c_str(), LEVEL_WIDE_STRING ) == 0 )
			min_dist = LEVEL_WIDE_MIN_DIST;
		else
			min_dist = ev->GetFloat( 3 );

		if ( min_dist >= LEVEL_WIDE_MIN_DIST_CUTOFF )
			min_dist = LEVEL_WIDE_MIN_DIST;
	}

	if ( ev->NumArgs() > 3 )
		headDisplay = ev->GetBoolean( 4 );

	if ( ev->NumArgs() > 4 )
		useTalk = ev->GetBoolean( 4 );

	if ( ev->NumArgs() > 5 )
		{
		state_name = ev->GetString( 6 );

		if ( strcmp( state_name, "" ) == 0 )
			state_name = NULL;
		}

	if ( ev->NumArgs() > 6 )
		user = (Sentient *)ev->GetEntity( 7 );

	PlayDialog( user, volume, min_dist, dialog_name, state_name, headDisplay, useTalk , true );
	}

void Actor::PlayRadiusDialog( Sentient* user )
   {
	str real_dialog;

	if ( GetActorFlag( ACTOR_FLAG_RADIUS_DIALOG_PLAYING ) )
		return;
	
	real_dialog = FindDialog( user, DIALOG_TYPE_RADIUS );

	if ( !real_dialog.length() )
		return;

	StopDialog();
	SetActorFlag( ACTOR_FLAG_RADIUS_DIALOG_PLAYING, true );
	PlayDialog( user, DEFAULT_VOL, -1.0f, real_dialog.c_str() , NULL );
	}

void Actor::_printDebugInfo(const str &laststate , const str &currentState_shadow1 , const str &legAnim , const str &torsoAnim )
	{
   gi.Printf( "\n");
	gi.Printf( "Name         : %s\n", name.c_str()            );
	gi.Printf( "TargetName   : %s\n", targetname.c_str()      );
	gi.Printf( "EntNum       : %d\n", entnum                  );
	gi.Printf( "\n" );
	
	switch( showStates )
		{
		case DEBUG_STATES_ONLY:
         gi.Printf( "StateMap or Package: %s\n", statemap_name.c_str()       );
         gi.Printf( "LastState     : %s\n", laststate.c_str()                );
			gi.Printf( "CurrentState  : %s\n", currentState_shadow1.c_str()             );
			break;

		case DEBUG_STATES_BEHAVIORS:
         gi.Printf( "StateMap or Package: %s\n", statemap_name.c_str()       );
         gi.Printf( "LastState     : %s\n", laststate.c_str()                );
			gi.Printf( "CurrentState  : %s\n", currentState_shadow1.c_str()             );
			gi.Printf( "\n" );
			if ( behavior )
				{
				gi.Printf( "Behavior      : %s\n", behavior->getClassname()      );
				if ( behavior->GetInternalStateName().length() )
					gi.Printf( "Internal Behavior State: %s\n" , behavior->GetInternalStateName().c_str() );
				else
					gi.Printf( "Internal Behavior State: Unspecified\n" );
				}

			if ( headBehavior )
				gi.Printf( "HeadBehavior  : %s\n", headBehavior->getClassname()  );
			
			if ( eyeBehavior )
				gi.Printf( "EyeBehavior   : %s\n", eyeBehavior->getClassname()   );

			if ( torsoBehavior )
				gi.Printf( "TorsoBehavior : %s\n", torsoBehavior->getClassname() );
			break;

		case	DEBUG_ALL:
         gi.Printf( "StateMap or Package: %s\n", statemap_name.c_str()       );
         gi.Printf( "LastState     : %s\n", laststate.c_str()                );
			gi.Printf( "CurrentState  : %s\n", currentState_shadow1.c_str()             );
			gi.Printf( "\n" );
			if ( behavior )
				{
				gi.Printf( "Behavior      : %s\n", behavior->getClassname()      );
				if ( behavior->GetInternalStateName().length() )
					gi.Printf( "Internal Behavior State: %s\n" , behavior->GetInternalStateName().c_str() );
				else
					gi.Printf( "Internal Behavior State: Unspecified\n" );
				}

			if ( headBehavior )
				gi.Printf( "HeadBehavior  : %s\n", headBehavior->getClassname()  );
			
			if ( eyeBehavior )
				gi.Printf( "EyeBehavior   : %s\n", eyeBehavior->getClassname()   );

			if ( torsoBehavior )
				gi.Printf( "TorsoBehavior : %s\n", torsoBehavior->getClassname() );
			gi.Printf( "\n" );
			gi.Printf( "LegAnim       : %s\n", legAnim.c_str()                  );
			gi.Printf( "TorsoAnim     : %s\n", torsoAnim.c_str()                );
			
			break;

		default:
			return;
		}
	}

void Actor::Pushable( Event *ev )
	{
	bool flag;

	if ( ev->NumArgs() )
		flag = ev->GetBoolean( 1 );
	else
		flag = true;	

	SetActorFlag( ACTOR_FLAG_PUSHABLE, flag );
	}

void Actor::Push( Event *ev )
	{
	movementSubsystem->Push(ev->GetVector( 1 ) );
	}


void Actor::RegisterParts( Event *ev )
	{
	Entity *targetent;
	qboolean forward;
	int current_part;
	part_t *forward_part;
	part_t new_part;
	Event *event;

	targetent = ev->GetEntity( 1 );
	forward   = ev->GetInteger( 2 );

	if ( !targetent )
		return;

	
	if ( forward )
		{

		for ( current_part = 1 ; current_part <= parts.NumObjects() ; current_part++ )
			{
			forward_part = &parts.ObjectAt( current_part );

			if ( forward_part )
				{

				event = new Event( EV_ActorRegisterParts );
				event->AddEntity( targetent );
				event->AddInteger( false );
				forward_part->ent->PostEvent( event, 0.0f );


				event = new Event( EV_ActorRegisterParts );
				event->AddEntity( forward_part->ent );
				event->AddInteger( false );
				targetent->PostEvent( event, 0.0f );
				}
			}
		}


	new_part.ent         = targetent;
	new_part.state_flags = 0;

	parts.AddObject( new_part );
	}

void Actor::RegisterSelf( Event *ev	)
	{
	Entity *targetent;
	Actor *targetact;
	Event *event;
	part_t new_part;

	if ( target.length() > 0 )
		{

		targetent = G_FindTarget( this, target.c_str() );

		if ( !targetent )
			return;


		if ( targetent->isSubclassOf( Actor ) )
			{
			targetact = (Actor *)targetent;

			if ( ( name.length() > 0 ) && ( targetact->name == name ) )
				{

				event = new Event( EV_ActorRegisterParts );
				event->AddEntity( this );
				event->AddInteger( true );
				targetent->PostEvent( event, 0.0f );


				new_part.ent         = targetent;
				new_part.state_flags = 0;
				parts.AddObject( new_part );
				}
			}
      }
	}

void Actor::ReleaseNodeEvent( Event *ev )
	{
	PathNode *node;
	Vector pos;

	pos = ev->GetVector( 1 );
	node = thePathManager.NearestNode( pos, this );

	if ( node && ( node->entnum == entnum ) )
		{
		node->occupiedTime = 0;
		node->entnum = 0;
		}
	}


void Actor::RemoveAnimDoneEvent( void )
	{
	animate->SetAnimDoneEvent( NULL );

	if ( newanimevent )
		{
		delete newanimevent;
		newanimevent = NULL;
		}

	last_anim_event_name = "";
	}



bool Actor::RequestControl
(
	Listener			*controller,
	ActorControlType	 controlType
)
{
	if ( !_controller )
	{
		_controller		= controller ;
		_controlType	= controlType ;
		return true ;
	}

	if ( _controlType != ACTOR_CONTROL_LOCKED )
	{
		Event *ev = new Event( EV_Actor_ControlLost );
		_controller->ProcessEvent( ev );
		_controller = controller ;
		return true ;
	}

	return false ;
}

// frees `_path` in its destructor (SDK actor_locomotion.cpp:273), so the ownership
void Actor::SetPath( Path *newpath )
	{
	movementSubsystem->setPath( newpath );
	}

void Actor::ReserveNodeEvent(	Event *ev )
	{
	PathNode *node;
	Vector pos;

	pos = ev->GetVector( 1 );
	node = thePathManager.NearestNode( pos, this );

	if ( node && ( !node->entnum || ( node->entnum == entnum ) || ( node->occupiedTime < level.time ) ) )
		{
		node->occupiedTime = level.time + ev->GetFloat( 2 );
		node->entnum = entnum;
		}
	}

void Actor::ResetMoveDir( Event *ev )
   {
   Vector newForward;
   angles.AngleVectors( &newForward );

   movementSubsystem->setMoveDir( newForward );
   }

void Actor::ReturnProjectile(	Event *ev )
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return;


	if (!incoming_proj) return;
	
	Vector dir = currentEnemy->origin - origin;
	Vector vel = incoming_proj->velocity;

	Vector Angles = dir.toAngles();
	vel*=-1.0f;
	
	incoming_proj->setAngles(Angles);
	incoming_proj->velocity = vel;
	

	}

void Actor::RunAlertThread ( Event *ev	)
	{
	if (alert_thread.length() )
		RunThread(alert_thread);
	}


void Actor::RunThread( Event *ev	)
	{
	str thread_name;
	thread_name = ev->GetString( 1 );
	RunThread(thread_name);
	}

void Actor::RunThread( const str &thread_name )
	{
	if ( thread_name.length() <= 0 )
		return;
	
	ExecuteThread(thread_name,true,this);
	

	}

void Actor::SendCommand( Event *ev )
	{
	str command_shadow1;
	str part_to_send_to;
	int i;
	part_t *part;
	Actor *partact;

	command_shadow1 = ev->GetString( 1 );
	part_to_send_to = ev->GetString( 2 );

	if ( ( command_shadow1.length() == 0 ) || ( part_to_send_to.length() == 0 ) )
		return;

	for( i = 1 ; i <= parts.NumObjects(); i++ )
		{
		part = &parts.ObjectAt( i );

		partact = ( Actor * )(Entity *)part->ent;

		if ( partact && ( partact->part_name == part_to_send_to ) )
			{
			partact->command = command_shadow1;
			}
		}
	}

void Actor::SetActivationStart()
{
	activationStart = level.time;
}

void Actor::SetActorToActorDamageModifier( Event *ev )
	{
	float modifier = ev->GetFloat( 1 );

	if ( modifier > 2.0f ) modifier = 2.0f; 
	if ( modifier < 0.0f ) modifier = 0.0f; 

	actor_to_actor_damage_modifier = modifier;

	}

void Actor::SetAggressiveness( Event *ev )
   {
   personality->SetAggressiveness( ev->GetFloat( 1 ) );
   }

void Actor::SetAnim( Event *ev	)
{
	
	if ( ev->NumArgs() > 1 )
	{
		SetAnim( ev->GetString( 1 ), NULL, legs, ev->GetFloat(2) );
	}
	else
	{
		SetAnim( ev->GetString( 1 ) );
	}
	ChangeAnim();
}

void Actor::SetAnimSet( const str& animSet )
{
	animset = animSet;
}

void Actor::SetBehaviorPackage( const str &packageName )
{
	if ( !masterstatemap )
	{
	assert ( masterstatemap );
	gi.WDPrintf( "You cannot set a behavior package on actor %s because it does not have a masterstatemap, please report this to the AI Programmer\n" , targetname.c_str() );
	return;
	}

	if ( !stricmp( packageName.c_str() , "auto" ) )
		{
		SetMasterState( "START" );
		}
	else
		{
		SetMasterState( "SCRIPTED" );
		strategos->SetBehaviorPackage( packageName );
		}
	
}

void Actor::SetBlink( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_SHOULD_BLINK, ev->GetBoolean( 1 ) );
	}

void Actor::SetBounceOffEffect( Event *ev )
	{
	bounce_off_effect = ev->GetString( 1 );
	}

void Actor::SetBounceOff( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_BOUNCE_OFF, true );
	}

void Actor::setBranchDialog( void )
{
	Player* player = GetPlayer(0);
	str commandString;
	if( player )
	{
		commandString = "stufftext \"displaybranchdialog ";
		commandString += _branchDialogName;
		commandString += "\"\n";
		gi.SendServerCommand(player->entnum, "%s", commandString.c_str());
		gi.SendServerCommand(player->entnum, "stufftext \"pushmenu branchdialog\"\n");
	}
}

void Actor::SetCanBeFinishedBy( Event *ev	)
	{
	str mod_string;
	int new_mod;
	int number_of_mods;
	int i;


	number_of_mods = ev->NumArgs();

	for ( i = 1 ; i <= number_of_mods ; i++ )
		{
		mod_string = ev->GetString( i );

		new_mod = MOD_NameToNum( mod_string );

		if ( new_mod != -1 )
			can_be_finsihed_by_mods.AddObject( new_mod );
		}
	}

void Actor::SetCanWalkOnOthers( Event *ev )
   {
   SetActorFlag( ACTOR_FLAG_CAN_WALK_ON_OTHERS, true );
   }

void Actor::SetContextInterval( float interval )
{
	_contextInterval = interval;
}


void Actor::SetDialogMode( Event *ev )
	{
	str modeType = ev->GetString(1);
	if(stricmp(modeType,"anxious") == 0 )
		{
		DialogMode = DIALOG_MODE_ANXIOUS;
		return;
		}

	if(stricmp(modeType,"normal") == 0 )
		{
		DialogMode = DIALOG_MODE_NORMAL;
		return;
		}

	if(stricmp(modeType,"ignore") == 0)
		{
		DialogMode = DIALOG_MODE_IGNORE;
		SetActorFlag( ACTOR_FLAG_ALLOW_TALK, 0 );
		return;
		}
	
	gi.WDPrintf( "SetDialogMode has an unknown dialog type - - Valid strings are 'anxious', 'normal', or 'ignore'\n");
	gi.WDPrintf( "Defaulting to type 'normal'");
	
	DialogMode = DIALOG_MODE_NORMAL;
	}

void Actor::SetEmotion(	Event *ev )
	{
	Event *new_event;


	if ( emotion != "none" )
		{
		new_event = new Event( EV_Unmorph );
		new_event->AddString( emotion );
		ProcessEvent( new_event );
		}

	if ( ev->NumArgs() > 0 )
		emotion = ev->GetString( 1 );


	if ( emotion != "none" )
		{
		new_event = new Event( EV_Morph );
		new_event->AddString( emotion );
		ProcessEvent( new_event );
		}
	}

void Actor::SetEnemyTargeted( bool targeted )
{
	Entity* enemy_shadow2;
	enemy_shadow2 = enemyManager->GetCurrentEnemy();

	if ( !enemy_shadow2 ) return;

	if ( enemy_shadow2->isSubclassOf(Player) )
		{
		Player* player;
		player = (Player*)enemy_shadow2;
		player->setTargeted(targeted);
		}
}

void Actor::SetFeetWidth( Event *ev )
   {
   feet_width = ev->GetFloat( 1 );
   }

void Actor::SetGroupNumber( Event *ev )
	{
	AddToGroup( ev->GetInteger( 1 ) );
	}

void Actor::SetHaveThing( Event *ev )
	{
	int thing_number;
	qboolean thing_bool;

	thing_number = ev->GetInteger( 1 );
	thing_bool   = ev->GetBoolean( 2 );

	SetHaveThing ( thing_number , thing_bool );
	}

void Actor::SetHaveThing( int thing_number, qboolean thing_bool )
	{
	switch( thing_number )
		{
		case 1 :
			SetActorFlag( ACTOR_FLAG_HAS_THING1, thing_bool );
			break;
		case 2 :
			SetActorFlag( ACTOR_FLAG_HAS_THING2, thing_bool );
			break;
		case 3 :
			SetActorFlag( ACTOR_FLAG_HAS_THING3, thing_bool );
			break;
		case 4 :
			SetActorFlag( ACTOR_FLAG_HAS_THING4, thing_bool );
			break;
		default :
			gi.WDPrintf( "Has thing %d out of range\n", thing_number );
			return;
		}
   }

void Actor::SetHealth( Event *ev	)
	{
	health = ev->GetFloat( 1 ); 

   if (max_health < health )
		{
		max_health = health;
		}
	
	}

void Actor::SetIdleThread( Event *ev )
	{
	idle_thread = ev->GetString( 1 );
	}

void Actor::SetMask( Event *ev )
	{
	str mask_name;

	mask_name = ev->GetString( 1 );

	if ( mask_name == "monstersolid" )
		edict->clipmask = MASK_MONSTERSOLID;
	else if ( mask_name == "deadsolid" )
		edict->clipmask = MASK_DEADSOLID;
	else if ( mask_name == "none" )
		edict->clipmask = 0;
	else if ( mask_name == "pathsolid" )
		edict->clipmask = MASK_PATHSOLID;
	else
		gi.WDPrintf( "Unknown mask name - %s\n", mask_name.c_str() );
	}

void Actor::SetMaxBossHealth( Event *ev )
	{
	max_boss_health = ev->GetFloat( 1 );
	}

void Actor::SetMaxHealth( Event *ev	)
	{
	max_health = ev->GetFloat( 1 ); 
	}

void Actor::SetMaxInactiveTime( Event *ev	)
	{
	max_inactive_time = ev->GetFloat( 1 );
	}

void Actor::SetMouthAngle( Event *ev )
	{
	int tag_num;
	float angle_percent;
	Vector mouth_angles;


	angle_percent = ev->GetFloat( 1 );

	if ( angle_percent < 0.0f )
		angle_percent = 0.0f;

	if ( angle_percent > 1.0f )
		angle_percent = 1.0f;

	tag_num = gi.Tag_NumForName( edict->s.modelindex, "tag_mouth" );

	if ( tag_num != -1 )
		{
		SetControllerTag( ACTOR_MOUTH_TAG, tag_num );

		mouth_angles = vec_zero;
		mouth_angles[PITCH] = max_mouth_angle * angle_percent;

		SetControllerAngles( ACTOR_MOUTH_TAG, mouth_angles );
		}
	}

void Actor::SetNotifyFlag( Event *ev )
	{
	str flag_name;
	int flag;
	qboolean flag_bool;

	flag_name = ev->GetString(1);
	flag_bool = ev->GetBoolean(2);

	flag = NotifyFlag_string_to_int( flag_name );
	
	SetNotifyFlag( flag, flag_bool );	

	}

void Actor::SetOnUseThread( Event *ev )
	{
	onuse_thread_name = ev->GetString( 1 );
	}

void Actor::SetRadiusDialogRange( Event *ev )
	{
	radiusDialogRange = ev->GetFloat(1);
	}

void Actor::SetSimplifiedThink( Event *ev )
	{
	if ( ( ev->NumArgs() > 0 ) && ( ev->GetBoolean( 1 ) == false ) )
	{
		if ( thinkStrategy != NULL )
			{
			delete thinkStrategy;
			thinkStrategy = NULL;
			}

		thinkStrategy = new DefaultThink();
	}
	else
	{
		if ( thinkStrategy != NULL )
			{
			delete thinkStrategy;
			thinkStrategy = NULL;
			}

		thinkStrategy = new SimplifiedThink( (Actor *)this );
	}

	if ( !thinkStrategy )
		gi.Error( ERR_FATAL, "Actor Could not create thinkStrategy" );

	}

void Actor::SetSpawnChance( Event *ev )
	{
	spawn_chance = ev->GetFloat( 1 );
	}

void Actor::SetStickToGround( Event *ev )
{
	movementSubsystem->SetStickToGround( ev->GetBoolean( 1 ) );
}

void Actor::SetTargetable( Event *ev )
	{
	qboolean targetable;

	targetable = ev->GetBoolean( 1 );

	SetActorFlag( ACTOR_FLAG_TARGETABLE, targetable );
	}

void Actor::SetTurnSpeed( Event *ev	)
	{
   movementSubsystem->setTurnSpeed(ev->GetFloat( 1 ) );	
	}

void Actor::SetupTriggerField( Event *ev )
	{
	Vector min;
	Vector max;
	TouchField *trig;

	min = ev->GetVector( 1 );
	max = ev->GetVector( 2 );

	min = min + origin;
	max = max + origin;

	trig = new TouchField;
	trig->Setup( this, EV_ActorTriggerTouched, min, max, TRIGGER_PLAYERS | TRIGGER_MONSTERS );
	trigger = trig;
	}

void Actor::SetUseGravity( Event *ev )
	{
	qboolean use_gravity = (qboolean) gravity ;

	if (ev->NumArgs() > 0 )
		{
		use_gravity = ev->GetBoolean( 1 );
		}
	else
		gi.WDPrintf( "SetUseGravity missing boolean argument.  Currently set to %d\n", ( int )gravity);

	SetActorFlag( ACTOR_FLAG_USE_GRAVITY, use_gravity );

	if ( use_gravity )
		gravity = 1;
	else
		gravity = 0;
	}


void Actor::SimplePathfinding( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_SIMPLE_PATHFINDING, true );
	}

void Actor::Sleep( void )
   {
	Actor *actor;

   if ( actortype == IS_INANIMATE )
      return;

	actor = this;

   if ( !SleepList.ObjectInList( actor ) )
      SleepList.AddObject( actor );

	if ( ActiveList.ObjectInList( actor ) )
      ActiveList.RemoveObject( actor );

   enemyManager->ClearCurrentEnemy();
   turnThinkOff();
   }

void Actor::SolidMask( Event *ev )
	{
	edict->clipmask = MASK_MONSTERSOLID;
	}

void Actor::StopDialog()
   {
	Event *new_event;

	if ( !GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) )
		return;

	StopSound( CHAN_DIALOG );


	CancelEventsOfType( EV_Morph );
	CancelEventsOfType( EV_Unmorph );


	new_event = new Event ( EV_Morph );

	new_event->AddString( "morph_base" );
	new_event->AddFloat( 100.0f );
	new_event->AddFloat( 0.10f );

	ProcessEvent( new_event );

	CancelEventsOfType( EV_Actor_DialogDone );
	CancelEventsOfType( EV_Actor_BroadcastDialog );
	ProcessEvent( EV_Actor_DialogDone );

	if ( GetActorFlag( ACTOR_FLAG_USING_HUD ) )
	{
		Player *player = GetPlayer( 0 );
		
		if ( player )
		{
			player->ClearDialog();
		}
	}
}

void Actor::StopDialog(	Event *ev )
	{
	StopDialog();
	}

void Actor::StopFireWeapon( Event *ev )
	{
	combatSubsystem->StopFireWeapon();
	}

void Actor::SwimEvent( Event *ev )
   {
   flags &= ~FL_FLY;
   flags |= FL_SWIM;
   }

void Actor::Throw( Event *ev )
   {
	int i;
	int num;
	Entity *child;
	Vector pos;
	Vector forward;
	str tag_name;
	int tag_num;


	tag_name = ev->GetString( 1 );
	tag_num  = gi.Tag_NumForName( edict->s.modelindex, tag_name.c_str() );


	if ( bind_info )
		{
		for ( i=0,num = bind_info->numchildren; i < MAX_MODEL_CHILDREN; i++ )
			{
			if ( bind_info->children[i] == ENTITYNUM_NONE )
				{
				continue;
				}

			child = ( Entity * )G_GetEntity( bind_info->children[i] );

			if ( child->edict->s.tag_num == tag_num )
				{
				child->detach();

				child->setSolidType( SOLID_BBOX );

				child->setAngles( angles );

				child->groundentity = NULL;

				tag_num = gi.Tag_NumForName( edict->s.modelindex, tag_name.c_str() );
				GetTag( tag_num, &pos, &forward );

				child->velocity = orientation[0];
				child->velocity *= 500.0f;

				child->velocity.z = 400.0f;
				}

			num--;

			if ( !num )
				break;
			}
		}
	}

void Actor::TouchTriggers( Event *ev )
	{
	SetActorFlag( ACTOR_FLAG_TOUCH_TRIGGERS, ev->GetBoolean( 1 ) );
	}

void Actor::TriggerTouched( Event *ev )
	{
	Entity *other;

	other = ev->GetEntity( 1 );

	if (
         ( other->movetype != MOVETYPE_NONE ) &&
         ( other->movetype != MOVETYPE_STATIONARY ) &&
         ( IsEntityAlive( other ) )
      )
		AddStateFlag( STATE_FLAG_TOUCHED );
	}

void Actor::TryBlink( void	)
	{
	float average_blink_time;
	Event *new_event;

	if ( next_blink_time <= level.time )
		{
		if ( emotion == "angry" )
			average_blink_time = 6.0f;
		else if ( emotion == "nervous" )
			average_blink_time = 2.0f;
		else
			average_blink_time = 4.0f;

		new_event = new Event( EV_Morph );
		new_event->AddString( "morph_lid-lshut" );
		new_event->AddFloat( 100.0f );
		new_event->AddFloat( .15f );
		new_event->AddInteger( true );
		ProcessEvent( new_event );

		new_event = new Event( EV_Morph );
		new_event->AddString( "morph_lid-rshut" );
		new_event->AddFloat( 100.0f );
		new_event->AddFloat( .15f );
		new_event->AddInteger( true );
		ProcessEvent( new_event );

		next_blink_time = level.time + average_blink_time + G_CRandom( average_blink_time / 4.0f );
		}
	}

void Actor::TurnAIOff( void )
	{
	SetActorFlag( ACTOR_FLAG_AI_ON, false );
	if ( sensoryPerception )
		sensoryPerception->RespondTo( STIMULI_NONE, true );

	if ( mode == ACTOR_MODE_AI )
		{
		enemyManager->SetCurrentEnemy( NULL );
		enemyManager->LockOnCurrentEnemy( false );
		EndMode();
		}
	}

void Actor::TurnAIOff( Event *ev	)
	{
   TurnAIOff();
	}

void Actor::TurnAIOn( void	)
	{
	if ( GetActorFlag( ACTOR_FLAG_AI_ON ) )
		return;

	SetActorFlag( ACTOR_FLAG_AI_ON, true );
	
	if ( sensoryPerception )
		sensoryPerception->RespondTo( STIMULI_ALL, true );

   EndMode(); 
   mode = ACTOR_MODE_AI;
   Wakeup();
	
	}


void Actor::TurnAIOn(Event *ev)
	{
   TurnAIOn();
	}

void Actor::UnreserveCurrentHelperNode()
{
	if ( currentHelperNode.node )
		currentHelperNode.node->UnreserveNode();
}

void Actor::UpdateBossHealth( Event *ev )
	{
	bool update = true;
	bool forceBar = false;

	if ( ev->NumArgs() > 0 )
		update = ev->GetBoolean( 1 );

	if ( ev->NumArgs() > 1 )
		forceBar = ev->GetBoolean( 2 );

	if ( !update && GetActorFlag( ACTOR_FLAG_UPDATE_BOSS_HEALTH ) )
		{
		char bosshealth_string[20];
		sprintf( bosshealth_string, "%.5f", 0.0 );
		gi.cvar_set( "bosshealth", bosshealth_string );
		}	

	SetActorFlag( ACTOR_FLAG_UPDATE_BOSS_HEALTH, update );
	SetActorFlag( ACTOR_FLAG_FORCE_LIFEBAR , forceBar );

	}

void Actor::UseBehaviorPackage( const str &packageName )
{
	strategos->SetBehaviorPackage( packageName );
}

void Actor::UseEvent( Event *ev )
{
	Entity *entity;

	if ( last_used_time + 0.25f >= level.time )
		return;

	if ( GetActorFlag(ACTOR_FLAG_CANNOT_USE) )
		return;

	if ( level.cinematic )
		return;


	entity = ev->GetEntity( 1 );

	if ( entity->isSubclassOf( Equipment ) )
		return;

	last_used_time = level.time;

	AddStateFlag( STATE_FLAG_USED );

	if ( onuse_thread_name.length() > 0 )
	{
		RunThread(onuse_thread_name);
	}

	if ( entity->isSubclassOf( Sentient ) && getSolidType() == SOLID_BBOX && !hidden() )
	{
		Sentient *user;
		user = (Sentient *)entity;
		
		StartTalkBehavior( user );
	}
}





void Actor::AddDialogParms( DialogNode_t *dialog_node, Event *ev )
   {
	const char *token;
	int parm_type;
	float temp_float;
	int current_parm;
	int num_parms;


	if ( dialog_node == NULL )
		return;

	current_parm = 2;
	num_parms    = ev->NumArgs();


	while( 1 )
		{
		if ( current_parm > num_parms )
			break;

		token = ev->GetString( current_parm );
		current_parm++;

		parm_type = DIALOG_PARM_TYPE_NONE;

		if (stricmp(token, "randompick") == 0)
			dialog_node->random_flag = true;
		else if (stricmp(token, "radiusdialog" ) == 0)
			dialog_node->dType = DIALOG_TYPE_RADIUS;
		else if (stricmp(token, "greetingdialog" ) == 0)
			dialog_node->dType = DIALOG_TYPE_GREETING;
		else if (stricmp(token, "combatdialog" ) == 0)
			dialog_node->dType = DIALOG_TYPE_COMBAT;
		else if (stricmp(token, "playerhas") == 0)
			parm_type = DIALOG_PARM_TYPE_PLAYERHAS;
		else if (stricmp(token, "playerhasnot") == 0)
			parm_type = DIALOG_PARM_TYPE_PLAYERHASNOT;
		else if (stricmp(token, "has") == 0)
			parm_type = DIALOG_PARM_TYPE_HAS;
		else if (stricmp(token, "has_not") == 0)
			parm_type = DIALOG_PARM_TYPE_HASNOT;
		else if (stricmp(token, "depends") == 0)
			parm_type = DIALOG_PARM_TYPE_DEPENDS;
		else if (stricmp(token, "dependsnot") == 0)
			parm_type = DIALOG_PARM_TYPE_DEPENDSNOT;
		else if (stricmp(token, "dependsint") == 0)
			parm_type = DIALOG_PARM_TYPE_DEPENDSINT;
		else if (stricmp(token, "contextinitiator" ) == 0)
			parm_type = DIALOG_PARM_TYPE_CONTEXT_INITIATOR;
		else if (stricmp(token, "contextresponse" ) == 0)
			parm_type = DIALOG_PARM_TYPE_CONTEXT_RESPONSE;	
		else if (stricmp(token, "random") == 0)
			{
			if ( current_parm > num_parms )
				break;

			token = ev->GetString( current_parm );
			current_parm++;

			temp_float = (float)atof(token);

			if ( ( temp_float >= 0.0f ) && ( temp_float <= 1.0f ) )
				dialog_node->random_percent = temp_float;
			else
				gi.WDPrintf("Random percent out of range for dialog (alias %s)\n", dialog_node->alias_name);
			}
		else
			gi.WDPrintf("Unknown parameter for dialog (alias %s)\n", dialog_node->alias_name);

		if (parm_type != DIALOG_PARM_TYPE_NONE)
			{
			if ( current_parm > num_parms )
				break;

			token = ev->GetString( current_parm );
			current_parm++;

			if (dialog_node->number_of_parms < MAX_DIALOG_PARMS)
				{
				strcpy(dialog_node->parms[dialog_node->number_of_parms].parm, token);
				dialog_node->parms[dialog_node->number_of_parms].type = parm_type;

				if ( parm_type == DIALOG_PARM_TYPE_DEPENDSINT )
					{
					token = ev->GetString( current_parm );
					current_parm++;
					strcpy( dialog_node->parms[ dialog_node->number_of_parms ].parm2, token );
					}

				dialog_node->number_of_parms++;
				}
			else
				{
				gi.WDPrintf("Too many parms for  dialog (alias %s)\n", dialog_node->alias_name);
				}
			}
		}
   }

void Actor::AddStateFlag( unsigned int flag )
	{
	int current_other_part;
	part_t *other_part;
	Entity *other_ent;
	Actor *other_act;
	int current_part;
	part_t *part;



	state_flags |= flag;


	for ( current_other_part = 1 ; current_other_part <= parts.NumObjects() ; current_other_part++ )
		{
		other_part = &parts.ObjectAt( current_other_part );

		other_ent = other_part->ent;
		other_act = (Actor *)other_ent;


		for ( current_part = 1 ; current_part <= other_act->parts.NumObjects() ; current_part++ )
			{
			part = &other_act->parts.ObjectAt( current_part );

			if ( part->ent == this )
				{

				part->state_flags |= flag;
				}
			}
		}
	}

void Actor::ArmorDamage( Event *ev )
	{	
	
	AddStateFlag( STATE_FLAG_ATTACKED );

	
	Entity *enemy_shadow1;
	enemy_shadow1 = ev->GetEntity( 3 );
	
	if ( enemy_shadow1->isSubclassOf( Player) )
		{
		if ( actortype == IS_TEAMMATE )
			{
			int MOD = ev->GetInteger(9);
			if ( MOD != MOD_EXPLOSION )
				AddStateFlag( STATE_FLAG_ATTACKED_BY_PLAYER );
			}
		else
			{
			AddStateFlag( STATE_FLAG_ATTACKED_BY_PLAYER );
			}
		}

	if ( !enemy_shadow1 )
		return;

	::Damage damage(ev);

	if ( sensoryPerception && sensoryPerception->ShouldRespondToStimuli( STIMULI_PAIN ) )
	{
		enemyManager->TryToAddToHateList( enemy_shadow1 );
	}

	enemyManager->AdjustDamageCaused( enemy_shadow1 , damage.damage );
	strategos->NotifyDamageChanged( enemy_shadow1 , damage.damage );

	if( GetActorFlag( ACTOR_FLAG_RESPONDING_TO_HITSCAN ) )
		return;

	if ( GetActorFlag(ACTOR_FLAG_INCOMING_HITSCAN) )
		{
		if ( RespondToHitscan() )
			return;
		}

	if ( ondamage_thread.length() )
		{
		RunDamageThread();
		}

	RunCustomThread( "damaged" );

	if ( gameComponent )
		gameComponent->HandleArmorDamage( ev ); 

   if ( GetNotifyFlag(NOTIFY_FLAG_DAMAGED) )
      _notifyGroupOfDamage();
  
	

	Sentient::ArmorDamage(damage);
	sensoryPerception->Stimuli( STIMULI_PAIN , enemy_shadow1 );
	}

qboolean Actor::CanAttack( Entity *ent,	qboolean usecurrentangles )
	{
   return CanAttackFrom( origin, ent, usecurrentangles );
	}

qboolean Actor::CanAttackFrom( const Vector &pos, const Entity *ent, qboolean usecurrentangles )
	{
   int      mask;
	Vector	delta;
	Vector	start;
	Vector	end;
	float		len;
	trace_t	trace;
	Entity	*t;
   Vector   ang;

	if ( usecurrentangles )
		{
   	Vector	dir;

		start = pos + centroid - origin;
		end = ent->centroid;
		end.z += ( ent->absmax.z - ent->centroid.z ) * 0.75f;
		delta = end - start;
      ang = delta.toAngles();
      ang.x = ang.x;
      ang.y = angles.y;
		len = delta.length();
   	ang.AngleVectors( &dir, NULL, NULL );
      dir *= len;
      end = start + dir;
		}
	else
		{
		start = pos + centroid - origin;
		end = ent->centroid;
		end.z += ( ent->absmax.z - ent->centroid.z ) * 0.75f;
      delta = end - start;
      len = delta.length();
		}

	end += delta * 4.0f;

   mask = MASK_SHOT;
	trace = G_Trace( start, vec_zero, vec_zero, end, this, mask, false, "Actor::CanShootFrom" );
	if ( trace.startsolid )
		{
		return false;
		}

   if ( !trace.ent )
      {
      return false;
      }

	if ( trace.ent == ent->edict )
		{
		return true;
		}

	t = trace.ent->entity;
	if ( enemyManager->IsValidEnemy( t ) )
		{
		return true;
		}

	if ( t->isSubclassOf( Object ) ||
		t->isSubclassOf( ScriptModel ) )
		{
      trace = G_Trace( Vector( trace.endpos ), vec_zero, vec_zero, end, t, mask, false, "Actor::CanShootFrom 2" );
	   if ( trace.startsolid )
		   {
		   return false;
		   }
      if ( !trace.ent )
         {
         return false;
         }

	   if ( trace.ent == ent->edict )
		   {
		   return true;
		   }

	   if ( enemyManager->IsValidEnemy( trace.ent->entity ) )
		   {
		   return true;
		   }

      return false;
		}

	return false;
	}


void Actor::SetActorFlag( int flag, qboolean flag_value )
	{
	unsigned int *flags_shadow3;
	int index;
	int bit = 0;

	if ( flag > ACTOR_FLAG_MAX )
		{
		gi.WDPrintf( "Actor flag %d out of range\n", flag );
		return;
		}

	index = flag / 32;

	switch( index )
		{
		case 0 :
			flags_shadow3 = &actor_flags1;
			bit = flag;
			break;
		case 1 :
			flags_shadow3 = &actor_flags2;
			bit = flag - 32;
			break;
		case 2 :
			flags_shadow3 = &actor_flags3;
			bit = flag - 64;
			break;
		case 3 :
			flags_shadow3 = &actor_flags4;
			bit = flag - 96;
			break;

		default :
			gi.WDPrintf( "Actor flag %d out of range\n", flag );
			return;
		}

	if ( flag_value )
		*flags_shadow3 |= 1 << bit;
	else
		*flags_shadow3 &= ~( 1 << bit );
	}

void Actor::SetActorFlag( Event *ev )
	{
	str flag_name;
	int flag;
	qboolean flag_bool;

	flag_name = ev->GetString(1);
	flag_bool = ev->GetBoolean(2);

	flag = ActorFlag_string_to_int( flag_name );
	
	SetActorFlag( flag, flag_bool );	

	}

void Actor::SetNotifyFlag( int flag, qboolean flag_value )
	{
	unsigned int *flags_shadow2;
	int index;

	if ( flag > NOTIFY_FLAG_MAX )
		{
		gi.WDPrintf( "Actor flag %d out of range\n", flag );
		return;
		}

	index = flag / 32;

	switch( index )
		{
		case 0 :
			flags_shadow2 = &notify_flags1;
			break;

		default :
			gi.WDPrintf( "Notify flag %d out of range\n", flag );
			return;
		}

	if ( flag_value )
		*flags_shadow2 |= 1 << flag;
	else
		*flags_shadow2 &= ~( 1 << flag );
	}

void Actor::StartTalkBehavior(Sentient *user)
{
	Talk *talk;

	if ( !ModeAllowed( ACTOR_MODE_TALK ) )
		return;

	StartMode( ACTOR_MODE_TALK );

	talk = new Talk;
	talk->SetUser( user );	
	SetBehavior( talk );
}




void AI_SenseEnemies	( void )
	{
	Actor    *actor;
	[[maybe_unused]] Player	 *player;

	player = GetPlayer( 0 );
	
	for( int i = SleepList.NumObjects(); i > 0; i-- )
		{
		actor = SleepList.ObjectAt( i );


		if ( actor )
			actor->sensoryPerception->SenseEnemies();
		}

	}

Actor *GetActor (	const str &actor_name )
	{
	Actor* testActor;
	int i;

	for ( i = 1; i <= SleepList.NumObjects(); i++ )
		{
		testActor = (Actor*)SleepList.ObjectAt( i );
		if (testActor->targetname == actor_name)
			return testActor;
				
		}		

	for ( i = 1; i <= ActiveList.NumObjects(); i++ )
		{
		testActor = (Actor*)ActiveList.ObjectAt( i );
		if (testActor->targetname == actor_name)
			return testActor;

		}
	
	return NULL;
	}


Player *GetPlayer( int index )
	{
	gentity_t   *ed;
	
	if( index > game.maxclients )
		return 0;

	ed = &g_entities[ index ];

	if ( !ed->inuse || !ed->entity )
		return 0;
	
	return ( Player * )g_entities[index].entity;

	}



qboolean Actor::GetNotifyFlag( int flag )
	{
	unsigned int *flags_shadow1;
	int index;

	if ( flag > NOTIFY_FLAG_MAX )
		{
		gi.WDPrintf( "Actor flag %d out of range\n", flag );
		return false;
		}

	index = flag / 32;

	switch( index )
		{
		case 0 :
			flags_shadow1 = &notify_flags1;
			break;

		default :
			gi.WDPrintf( "Notify flag %d out of range\n", flag );
			return false;
		}

	if ( *flags_shadow1 & ( 1 << flag ) )
		return true;
	else
		return false;
	}

void Actor::_notifyGroupOfDamage()
   {
	_notifyGroupOfEnemy();
   }

void Actor::_notifyGroupOfKilled()
   {
	_notifyGroupOfEnemy();
   }

void Actor::_notifyGroupSpottedEnemy()
   {
	_notifyGroupOfEnemy();
   }

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#include "../qcommon/qa_probe.h"

void Actor::PlayDialog( Sentient *user, float volume, float min_dist, const char *dialog_name, const char *state_name, qboolean headDisplay , bool useTalk , bool important )
{
	str real_dialog;
	char *morph_target_name;
	const char *dialogAnim;
	str dialog_anim;		
	int frame_number;
	int index;
	int morph_channel;
	float dialog_length;		
	float morph_time;	
	float amplitude;
	Event *new_event;
	bool changedAnim;
	
	if ( GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) && !important )
		return;
	
	if ( GetActorFlag( ACTOR_FLAG_DIALOG_PLAYING ) )
		StopDialog();

	SetActorFlag( ACTOR_FLAG_USING_HUD,	false );

	if ( dialog_name )
		real_dialog = dialog_name;
	else
		real_dialog = FindDialog( user , DIALOG_TYPE_NORMAL );
	
	if ( !real_dialog.length())
		return;	

	char localizedDialogName[MAX_QPATH];
	gi.LocalizeFilePath( real_dialog, localizedDialogName );

	dialogAnim = gi.Alias_FindSpecificAnim( edict->s.modelindex , localizedDialogName );

	if(dialogAnim)
		dialog_anim = dialogAnim;

	if ( PROBE_On() ) {
		static int widget_dlg;

		PROBE_Throttle( &widget_dlg, 40, 1 );
		WIDGET_PROBE(
			"dlg:play",
			"actor='%s' raw='%s' loc='%s' len=%.2f anim='%s' loop=%d\n",
			name.c_str(),
			real_dialog.c_str(),
			localizedDialogName,
			gi.SoundLength( localizedDialogName ),
			dialog_anim.c_str(),
			dialog_anim.length() > 0
				? (int)gi.Alias_CheckLoopAnim( edict->s.modelindex, localizedDialogName )
				: -1 );
	}

	if ( dialog_anim.length() > 0 )
	{
		if (gi.Alias_CheckLoopAnim( edict->s.modelindex , localizedDialogName ))
			SetAnim(dialog_anim);
		else
			SetAnim(dialog_anim, EV_Actor_DialogAnimDone );
	}
	
	
	Sound( str( localizedDialogName ), CHAN_DIALOG, volume, min_dist );
	SetActorFlag( ACTOR_FLAG_DIALOG_PLAYING, true );
	
	dialog_length = gi.SoundLength( localizedDialogName );
	
	dialog_done_time = level.time + dialog_length;
	
	
	Player *player = GetPlayer( 0 );
	
	if ( player )
	{
		if ( headDisplay )
			{
			player->SetupDialog( this, localizedDialogName );
			SetActorFlag( ACTOR_FLAG_USING_HUD,	true );
			}
		else
			player->SetupDialog( NULL, localizedDialogName );
	}
	
	if ( dialog_length > 0.0f )
	{
		Event *headTwitchEvent;
		
		if ( state_name != NULL && currentState )
		{
			if ( ( mode == ACTOR_MODE_SCRIPT ) || ( mode == ACTOR_MODE_IDLE ) )
			{
				dialog_old_state_name = currentState->getName();
				dialog_state_name = state_name;
				
				Event *idle_event = new Event( EV_Actor_Idle );
				idle_event->AddString( state_name );
				ProcessEvent( idle_event );
			}
		}
		
		index = -1;
		changedAnim = false;
		
		
		CancelEventsOfType( EV_Actor_SetHeadTwitch );

		headTwitchEvent = new Event( EV_Actor_SetHeadTwitch );
		headTwitchEvent->AddInteger( true );
		ProcessEvent( headTwitchEvent );
		
		
		headTwitchEvent = new Event( EV_Actor_SetHeadTwitch );
		headTwitchEvent->AddInteger( false );
		PostEvent( headTwitchEvent, dialog_length );
		
		while( 1 )
		{
			morph_target_name = (char *)gi.GetNextMorphTarget( localizedDialogName, &index, &frame_number, &amplitude );
			
			if ( index == -1 || !morph_target_name )
				break;

				
			morph_time = frame_number * ( 1.0f / LIP_SYNC_HZ );
			
			
			morph_time -= 2.0f * FRAMETIME;
			
			if ( morph_time < 0.0f )
				morph_time = 0.0f;
			
			if ( strnicmp( morph_target_name, "emt_", 4 ) == 0 )
			{

				new_event = new Event( EV_Actor_SetEmotion );
				new_event->AddString( morph_target_name + 4 );
				PostEvent( new_event, morph_time );
			}
			else if ( strnicmp( morph_target_name, "anm_", 4 ) == 0 )
			{
				if (mode != ACTOR_MODE_TALK  )
				{
					new_event = new Event( EV_Actor_Anim );
					new_event->AddString( morph_target_name + 4 );
					PostEvent( new_event, morph_time );
					SetActorFlag(ACTOR_FLAG_PLAYING_DIALOG_ANIM , true );

					changedAnim = true;
				}
			}
			else
			{
				morph_channel = GetMorphChannel( morph_target_name );
				
				new_event = new Event ( EV_Morph );
				
				new_event->AddString( morph_target_name );
				
				if ( morph_channel == MORPH_CHAN_MOUTH )
					amplitude *= _dialogMorphMult;
				
				new_event->AddFloat( amplitude );
				
				if ( morph_channel == MORPH_CHAN_EYES || morph_channel == MORPH_CHAN_LEFT_LID || morph_channel == MORPH_CHAN_RIGHT_LID )
					new_event->AddFloat( 0.10f );
				else if ( morph_channel == MORPH_CHAN_MOUTH )
					new_event->AddFloat( 0.15f );
				else
					new_event->AddFloat( 0.25f );
				
				PostEvent( new_event, morph_time, EVENT_DIALOG_ANIM );
			}
		}
		
		
		new_event = new Event ( EV_Morph );
		
		new_event->AddString( "morph_mouth_base" );
		new_event->AddFloat( 0.0f );
		new_event->AddFloat( 0.10f );
		
		PostEvent( new_event, dialog_length, EVENT_DIALOG_ANIM );
		
		
		PostEvent( EV_Actor_DialogDone, dialog_length );			
		
		if ( emotion != "none" )
		{
			new_event = new Event( EV_Actor_SetEmotion );
			new_event->AddString( emotion.c_str() );
			PostEvent( new_event, dialog_length );
		}
		
		
		if ( changedAnim )
		{
			new_event = new Event( EV_Actor_Anim );
			new_event->AddString( "idle" );
			PostEvent( new_event, dialog_length );

			Event *clearFlag;
			clearFlag = new Event ( EV_Actor_SetActorFlag );
			clearFlag->AddString( "playingdialoganim" );
			clearFlag->AddInteger( 0 );
			PostEvent( clearFlag , dialog_length );			
		}
		
	}
	else
	{
		SetActorFlag( ACTOR_FLAG_DIALOG_PLAYING, false );
		gi.WDPrintf( "Lip file not found for dialog %s\n", localizedDialogName );
	}

	if(useTalk)
		StartTalkBehavior(user);
}

void Actor::RunDamageThread ( void )
	{
	if ( ondamage_threshold > 0 )
		{
		if ( ( health <= ondamage_threshold ) && ondamage_thread.length() )
			RunThread( ondamage_thread );
		return;
		}
	
	if ( ondamage_thread.length() )
		RunThread( ondamage_thread );

	}



void Actor::_notifyGroupOfEnemy()
{
   Actor *act;
   Entity *currentEnemy;
   int i;

   enemyManager->FindHighestHateEnemy();
   currentEnemy = enemyManager->GetCurrentEnemy();

   if ( !currentEnemy )
      return;

   for( i = 1; i <= ActiveList.NumObjects(); i++ )
      {
      act = ActiveList.ObjectAt( i );
		
		if ( act->GetGroupID() == GetGroupID() )
         {
         act->sensoryPerception->Stimuli(STIMULI_ALL);
         act->enemyManager->TryToAddToHateList( currentEnemy );
         act->enemyManager->SetCurrentEnemy( currentEnemy );
         act->personality->SetAggressiveness( 1.0f);
         }
		}
   
   for ( i = 1; i <= SleepList.NumObjects(); i++ )
      {
      act = SleepList.ObjectAt( i );
		
		if ( act->GetGroupID() == GetGroupID() )
         {
			act->sensoryPerception->Stimuli(STIMULI_ALL);
         act->enemyManager->TryToAddToHateList( currentEnemy );
         act->enemyManager->SetCurrentEnemy( currentEnemy );
         act->personality->SetAggressiveness( 1.0f);
         }      
      }   
}



qboolean Actor::checkActorFlag( Conditional &condition )
   {  
   str flagName = condition.getParm( 1 );
   
   return GetActorFlag( flagName );
  
   }



qboolean Actor::checkInAIMode( Conditional &condition )
	{
	if ( mode == ACTOR_MODE_AI )
		return true;

	return false;
	}

qboolean Actor::checkInConeOfFire( Conditional &condition )
   {
   return GetActorFlag( ACTOR_FLAG_IN_CONE_OF_FIRE );
   }

qboolean Actor::checkInPlayerConeOfFire( Conditional &condition )
{
	return GetActorFlag( ACTOR_FLAG_IN_PLAYER_CONE_OF_FIRE );
}

qboolean Actor::checkIsAggressive( Conditional &condition )
   {  
   float baseLine;

   baseLine = atof(condition.getParm( 1 ));

   return ( baseLine <= personality->GetAggressiveness() );
      
   }

qboolean Actor::checkanimdone( Conditional &condition )
   {
	return GetActorFlag( ACTOR_FLAG_ANIM_DONE );
   }

qboolean Actor::checkanimname( Conditional &condition )
   {
	str anim_name_test;
	int use_length;
	int result;

	anim_name_test = condition.getParm( 1 );

	if ( ( animname.length() == 0 ) || ( anim_name_test.length() == 0 ) )
		return false;

	if ( condition.numParms() > 1 )
		use_length = atoi( condition.getParm( 2 ) );
	else
		use_length = false;

	if ( use_length )
		result = strncmp( animname.c_str(), anim_name_test.c_str(), anim_name_test.length() );
	else
		result = strcmp( animname.c_str(), anim_name_test.c_str() );

	return (result == 0);
   }

qboolean Actor::checkcanseeplayer( Conditional &condition )
   {
	qboolean use_fov = true;
	qboolean can_see;
	qboolean in_fov;
	qboolean real_can_see;

	Entity *player;
	player = GetPlayer ( 0 );

	if ( !player )
		return false;

	if ( condition.numParms() > 0 )
		use_fov = atoi( condition.getParm( 1 ) );


   if ( canseeplayer_time > level.time )
      {
		if ( use_fov )
			return GetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER );
		else
			return GetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER_NOFOV );
      }

	can_see = true;
	in_fov  = sensoryPerception->InFOV( player );


	if ( !IsEntityAlive( player ) )
		{
		can_see = false;
		in_fov  = false;
		}
   
	if ( sensoryPerception )
		{
		if ( can_see && !sensoryPerception->CanSeeEntity( this , player , true , true ) )
			can_see = false;
		}


	SetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER, can_see && in_fov );
	SetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER_NOFOV, can_see );

   canseeplayer_time = level.time + 0.2f + G_Random( 0.1f );

	if ( use_fov )
		real_can_see = GetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER );
	else
		real_can_see = GetActorFlag( ACTOR_FLAG_LAST_CANSEEPLAYER_NOFOV );


	if ( real_can_see )
		last_known_player_pos = player->origin;

	return real_can_see;
   }

qboolean Actor::checkdead( Conditional &condition )
   {
	return deadflag != 0;
   }

qboolean Actor::checkenemydead( void )
	{
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if (  currentEnemy->deadflag || ( currentEnemy->health <= 0 ) ) 
		return true;

	return false;
	}

qboolean Actor::GetActorFlag( const str &flag_name ) const
   {
   int flag;
   flag = ActorFlag_string_to_int( flag_name );

   return GetActorFlag( flag );
   }

qboolean Actor::checkenemydead( Conditional &condition )
   {
	return checkenemydead();
   }

qboolean Actor::checkenemynoclip( Conditional &condition )
   {
	Entity *currentEnemy;
	currentEnemy = enemyManager->GetCurrentEnemy();
	if ( !currentEnemy )
		return false;

	if ( currentEnemy->movetype == MOVETYPE_NOCLIP )
		return true;

	return false;
   }

qboolean Actor::checkinactive( Conditional &condition )
   {
	return GetActorFlag( ACTOR_FLAG_INACTIVE );
   }

qboolean Actor::checktorsoanimdone( Conditional &condition )
   {
	return GetActorFlag( ACTOR_FLAG_TORSO_ANIM_DONE );
   }

Condition<Actor> Actor::Conditions[] =
   {
		{ "default",							&Actor::returntrue						},
		{ "INACTIVE",							&Actor::checkinactive					},
		{ "ANIM_DONE",							&Actor::checkanimdone							},
		{ "TORSO_ANIM_DONE",					&Actor::checktorsoanimdone   					},
		{ "DEAD",								&Actor::checkdead								},
		{ "ACTOR_FLAG",							&Actor::checkActorFlag							},
		{ "HAVE_ENEMY",							&Actor::checkhaveenemy							},
		{ "ENEMY_DEAD",							&Actor::checkenemydead							},
		{ "ENEMY_NOCLIP",						&Actor::checkenemynoclip						},
		{ "CAN_ATTACK_ENEMY",					&Actor::checkCanAttackEnemy						},
		{ "CAN_SEE_ENEMY",						&Actor::checkcanseeenemy						},		
		{ "CAN_SEE_PLAYER",						&Actor::checkcanseeplayer						},		
		{ "ENEMY_IN_FOV",						&Actor::checkenemyinfov							},
		{ "ENEMY_RELATIVE_YAW",					&Actor::checkenemyrelativeyaw					},
		{ "CHECK_ENEMY_YAW_RANGE",				&Actor::checkenemyyawrange						},
		{ "CAN_SHOOT_ANY_ENEMY",				&Actor::checkCanAttackAnyEnemy					},
		{ "ENEMY_ON_GROUND",					&Actor::checkenemyonground						},
		{ "CAN_JUMP_TO_ENEMY",					&Actor::checkcanjumptoenemy						},
		{ "CAN_FLY_TO_ENEMY",					&Actor::checkcanflytoenemy						},
		{ "PAIN",								&Actor::checkinpain								},
		{ "SMALL_PAIN",							&Actor::checksmallpain							},		
		{ "PAIN_YAW",							&Actor::checkpainyaw							},
		{ "PAIN_PITCH",							&Actor::checkpainpitch							},
		{ "SHOW_PAIN",							&Actor::checkShowPain							},
		{ "STUNNED",							&Actor::checkstunned							},
		{ "FINISHED",							&Actor::checkfinished							},
		{ "MELEE_HIT",							&Actor::checkmeleehit							},
		{ "HIT_WORLD",							&Actor::checkMeleeHitWorld						},
		{ "BLOCKED_HIT",						&Actor::checkblockedhit							},
		{ "BLOCKED",							&Actor::checkblocked							},
		{ "OTHER_DIED",							&Actor::checkotherdied							},
		{ "STUCK",								&Actor::checkstuck								},
		{ "NO_PATH",							&Actor::checknopath								},
		{ "STEERING_FAILED",					&Actor::checkSteeringFailed						},
		{ "HAVE_PATH_TO_ENEMY",					&Actor::checkHavePathToEnemy					},
		{ "ON_FIRE",							&Actor::checkonfire								},
		{ "BEHAVIOR_DONE",						&Actor::checkbehaviordone						},
		{ "BEHAVIOR_SUCCESS",					&Actor::checkbehaviorsuccess					},
		{ "BEHAVIOR_FAILED",					&Actor::checkbehaviorfailed						},
		{ "HEAD_BEHAVIOR_DONE",					&Actor::checkheadbehaviordone					},
		{ "EYE_BEHAVIOR_DONE",					&Actor::checkeyebehaviordone					},
		{ "TORSO_BEHAVIOR_DONE",				&Actor::checktorsobehaviordone					},
		{ "TORSO_BEHAVIOR_FAILED",				&Actor::checktorsobehaviorfailed				},
		{ "TORSO_BEHAVIOR_SUCCESS",				&Actor::checktorsobehaviorsuccess				},
		{ "TIME_DONE",							&Actor::checktimedone							},
		{ "DONE",								&Actor::checkdone								},
		{ "RANGE",								&Actor::checkenemyrange							},
		{ "ENEMY_WITHIN_RANGE",					&Actor::checkEnemyWithinRange					},
		{ "ENEMY_ATTACHED",						&Actor::checkEnemyAttached						},
		{ "PLAYER_RANGE",						&Actor::checkplayerrange						},
		{ "PARENT_RANGE",						&Actor::checkparentrange						},
		{ "CHANCE",								&Actor::checkchance								},
		{ "MOVING_ACTOR_RANGE",					&Actor::checkmovingactorrange					},
		{ "STATE_TIME",							&Actor::checkstatetime							},
		{ "TIMES_DONE",							&Actor::checktimesdone							},
		{ "MOD",								&Actor::checkmeansofdeath						},
		{ "NOISE_HEARD",						&Actor::checknoiseheard							},
		{ "PART_STATE",							&Actor::checkpartstate							},
		{ "PART_DEAD",							&Actor::checkpartdead							},
		{ "PART_FLAG",							&Actor::checkpartflag							},
		{ "NUM_SPAWNS",							&Actor::checknumspawns							},
		{ "COMMAND",							&Actor::checkcommand							},
		{ "TOUCHED",							&Actor::checktouched							},
		{ "TOUCHED_BY_PLAYER",					&Actor::checktouchedbyplayer					},
		{ "ACTIVATED",							&Actor::checkactivated							},
		{ "USED",								&Actor::checkused								},
		{ "TWITCH",								&Actor::checktwitch								},
		{ "HEALTH",								&Actor::checkhealth								},
		{ "HEALTH_PERCENT_LESS_THAN",			&Actor::checkhealthpercent						},
		{ "HEALTH_PERCENT_IN_RANGE",			&Actor::checkhealthpercentinrange				},
		{ "ON_GROUND",							&Actor::checkonground							},
		{ "IN_WATER",							&Actor::checkinwater							},
		{ "INCOMING_MELEE_ATTACK",				&Actor::checkincomingmeleeattack				},
		{ "INCOMING_RANGED_ATTACK",				&Actor::checkincomingrangedattack				},
		{ "INCOMING_PROJECTILE",				&Actor::checkincomingprojectile					},
		{ "ENEMY_STUNNED",						&Actor::checkenemystunned						},
		{ "ENEMY_IN_PATH",						&Actor::checkenemyinpath						},
		{ "STAGE",								&Actor::checkstage								},
		{ "HELD",								&Actor::checkheld								},
		{ "ENEMY_HAS_MELEE",					&Actor::checkenemymelee							},
		{ "ENEMY_HAS_RANGED",					&Actor::checkenemyranged						},
		{ "PLAYER_HAS_WEAPON",					&Actor::checkplayerranged						},
		{ "HAS_THING",							&Actor::checkhasthing							},
		{ "AT_COVER_NODE",						&Actor::checkatcovernode						},
		{ "ALLOW_HANGBACK",						&Actor::checkallowhangback						},
		{ "NAME",								&Actor::checkname								},
		{ "ANIM_NAME",							&Actor::checkanimname							},
		{ "CHECK_VAR",							&Actor::checkVar								},
		{ "CHECK_VAR_TIME_DIFFERENCE",			&Actor::checkVarTimeDifference					},
		{ "NODE_EXISTS",						&Actor::checkNodeExists							},
		{ "COVER_NODES",						&Actor::checkCoverNodes							},
		{ "SURFACE_DAMAGED",					&Actor::checkSurfaceDamaged						},
		{ "BONE_DAMAGED",						&Actor::checkBoneDamaged						},
		{ "REGION_DAMAGED",						&Actor::checkRegionDamaged						},
		{ "CAPTURED",							&Actor::checkCaptured							},
		{ "CAN_WALK_FORWARD",					&Actor::checkCanWalkForward						},
		{ "HAS_THROWOBJECT",					&Actor::checkHasThrowObject						},
		{ "ENEMY_IS_THROWOBJECT",				&Actor::checkEnemyIsThrowObject					},
		{ "TURRET_MODE",						&Actor::checkTurretMode							},
		{ "WEAPON_READY",						&Actor::checkWeaponReady						},
		{ "PLAYER_VALID",						&Actor::checkPlayerValid						},
		{ "IN_PREFERRED_RANGE",					&Actor::checkInPreferredRange 					},
		{ "IN_ABSOLUTE_RANGE",					&Actor::checkInAbsoluteRange					},
		{ "IN_AI_MODE",							&Actor::checkInAIMode							},
		{ "DISABLED",							&Actor::checkDisabled							},
		{ "CRIPPLED",							&Actor::checkCrippled							},
		{ "IN_ALCOVE",							&Actor::checkInAlcove							},
		{ "PLAYER_IN_CALL_VOLUME",				&Actor::checkPlayerInCallVolume					},
		{ "IN_CALL_VOLUME",						&Actor::checkInCallVolume						},
		{ "IS_AGGRESSIVE",						&Actor::checkIsAggressive						},
		{ "IN_CONE_OF_FIRE",					&Actor::checkInConeOfFire						},
		{ "IN_PLAYER_CONE_OF_FIRE",				&Actor::checkInPlayerConeOfFire					},
		{ "PATROL_NODE_IN_DISTANCE",			&Actor::checkPatrolWaypointNodeInDistance		},
		{ "PATH_NODE_IN_DISTANCE",				&Actor::checkPathNodeTypeInDistance				},
		{ "WEAPON_NAMED",						&Actor::checkUsingWeaponNamed					},
		{ "OUT_OF_TORSO_RANGE",					&Actor::checkOutOfTorsoRange					},
		{ "WANTS_TO_EXECUTE_PACKAGE",			&Actor::checkWantsToExecutePackage				},
		{ "EXECUTED_IN_LAST",					&Actor::checkExecutedPackageInLastTimeFrame		},
		{ "FORWARD_CLEAR",						&Actor::checkForwardDirectionClear				},
		{ "REAR_CLEAR",							&Actor::checkRearDirectionClear					},
		{ "LEFT_CLEAR",							&Actor::checkLeftDirectionClear					},
		{ "RIGHT_CLEAR",						&Actor::checkRightDirectionClear				},
		{ "LAST_STATE",							&Actor::checkLastState							},
		{ "GROUP_MEMBER_IN_RANGE",				&Actor::checkGroupMememberRange					},
		{ "ACTORTYPE",							&Actor::checkActorType							},
		{ "IS_TEAMMATE",						&Actor::checkIsTeammate							},
		{ "HAVE_ACTIVE_WEAPON",					&Actor::checkHaveActiveWeapon					},
		{ "WEAPON_IS_MELEE",					&Actor::checkWeaponIsMelee						},
		{ "WEAPON_CHANGED",						&Actor::checkWeaponChanged						},
		{ "GROUP_HAS_THIS_NAME_LESS_THAN",		&Actor::checkCountOfIdenticalNamesInGroup		},
		{ "REQUESTED_POSTURE",					&Actor::checkRequestedPosture					},
		{ "POSTURE_ANIM_DONE",					&Actor::checkPostureAnimDone					},
		{ "DAMAGE_THRESHOLD_EXCEEDED",			&Actor::checkDamageThresholdExceeded			},
		{ "ATTACKED",							&Actor::checkAttacked							},
		{ "ATTACKED_BY_PLAYER",					&Actor::checkAttackedByPlayer					},
		{ "HELPERNODE_FLAGGED_IN_RANGED" ,		&Actor::checkHelperNodeWithFlagInRange			},
		{ "ENEMY_USING_WEAPON_NAMED",			&Actor::checkEnemyWeaponNamed					},
		{ "PLAYER_USING_WEAPON_NAMED",			&Actor::checkPlayerWeaponNamed					},
		{ "GROUP_HAS_NUMATTACKERS_LESS_THAN",	&Actor::checkGroupAttackerCount					},
		{ "CURRENT_ENEMY_HAS_NUMATTACKERS_LESS_THAN",	&Actor::checkCurrentEnemyGroupAttackerCount		},
		{ "HAVE_BEST_WEAPON" ,					&Actor::checkHaveBestWeapon						},
		{ "POSTURE",							&Actor::checkPosture							},
		{ "ANY_ENEMY_IN_RANGE",					&Actor::checkAnyEnemyInRange					},
		{ "VALID_COVER_NODE_IN_RANGE",			&Actor::checkValidCoverNodeInRange				},
		{ "VALID_COMBAT_NODE_IN_RANGE",			&Actor::checkValidCombatNodeInRange				},
		{ "VALID_WORK_NODE_IN_RANGE",			&Actor::checkValidWorkNodeInRange				},
		{ "VALID_HIBERNATE_NODE_IN_RANGE",		&Actor::checkValidHibernateNodeInRange			},
		{ "VALID_PATROL_NODE_IN_RANGE",			&Actor::checkValidPatrolNodeInRange				},
		{ "VALID_CUSTOM_NODE_IN_RANGE",			&Actor::checkValidCustomNodeInRange				},
		{ "VALID_SNIPER_NODE_IN_RANGE",			&Actor::checkValidSniperNodeInRange				},
		{ "ENEMY_CAN_SEE_CURRENT_NODE",			&Actor::checkEnemyCanSeeCurrentNode				},
		{ "FOLLOW_TARGET_OUT_OF_RANGE",			&Actor::checkSpecifiedFollowTargetOutOfRange	},
		{ "WITHIN_FOLLOW_TARGET_MIN_RANGE",		&Actor::checkWithinFollowRangeMin				},
		{ "IN_THE_WAY",							&Actor::checkInTheWay							},
		{ "SHOULD_DO_ACTION",					&Actor::checkShouldDoAction						},
		{ "HAVE_ARMOR",							&Actor::checkHaveArmor							},
		{ "ALLOWED_TO_MELEE_ENEMY",				&Actor::checkAllowedToMeleeEnemy				},
		{ "CURRENT_NODE_COVERTYPE",				&Actor::checkCurrentNodeHasThisCoverType		},
		{ "BLOCKED_BY_ENEMY",					&Actor::checkBlockedByEnemy						},
		{ "ENEMY_PROJECTILE_CLOSE",				&Actor::checkEnemyProjectileClose				},
		{ "ACTIVATION_DELAY_DONE",				&Actor::checkActivationDelayTime				},
		{ "TALKING",							&Actor::checkTalking							},
		{ "ANY_ENEMIES_NEARBY",					&Actor::checkEnemiesNearby						},



		{ "PROP_EXISTS",						&Actor::checkPropExists							},
		{ "PROP_CHANCE",						&Actor::checkPropChance							},		
		{ "PROP_ENEMY_RANGE",					&Actor::checkPropEnemyRange						},


		{ "CAN_SHOOT_ENEMY",					&Actor::checkcanshootenemy						},

		{ NULL,	NULL }
   };

char actor_flag_strings[ ACTOR_FLAG_MAX ][ 32 ] =
   {
	"noiseheard",
	"investigating",
	"deathgib",
	"deathfade",
	"nochatter",
	"inactive",
	"animdone",
	"statedonetimevalid",
	"masterstatedonetimevalid",
	"AIon",
	"lastcanseeenemy",
	"lastcanseeenemynofov",
	"dialogplaying",
	"radiusdialogplaying",
	"allowtalk",
	"damageonceon",
	"damageoncedamaged",
	"bounceoff",
	"notifiyothersatdeath",
	"hasthing1",
	"hasthing2",
	"hasthing3",
	"hasthing4",
	"lastattackhit",
	"started",
	"allowhangback",
	"usegravity",
	"spawnfailed",
	"fadingout",
	"deathshrink",
	"deathsink",
	"staysolid",
	"stunned",
	"allowfall",
	"finished",
	"inlimbo",
	"canwalkonothers",
	"pushable",
	"lasttrytalk",	
	"targetable",
	"immortal",
	"turninghead",
	"movingeyes",
	"diecompletely",
	"bleedafterdeath",
	"ignorestuckwarning",
	"ignoreoffgroundwarning",
	"allowedtokill",
	"touchtriggers",
	"ignorewater",
	"neverignoresounds",
	"simplepathfinding",
	"havemoved",
	"nopainsounds",
	"updatebosshealth",
	"ignorepainfromactors",
	"damageallowed",
	"atcovernode",
	"waitfornewenemy",
	"takedamage",
	"usedamageskins",
	"captured",
	"turretmode",
	"incominghitscan",
	"respondingtohitscan",
	"meleehitworld",
	"torsoanimdone",
	"weaponready",
	"disabled",
	"inalcove",
   "inconeoffire",
   "inplayerconeoffire",
   "playerincallvolume",
   "incallvolume",
   "outoftorsorange",
   "ducked",
   "prone",
   "shouldblink",
   "crippled",
   "retreating",
   "hidden",
   "followinginformation",
   "displayingfailureFX",
   "groupmemberinjured",
   "canhealother",
   "strictlyfollowpath",
   "postureanimdone",
   "attackingenemy",
   "updatehatebasedonattackers",
   "lastcanseeplayer",
   "lastcanseeplayernofov",   
   "meleeallowed",
   "playingdialoganim",
   "usinghud",
   "forcelifebar",
   "updateactionlevel",
   "canchangeanim",
   "usefollowrangefornodes",
   "immediateactivate",
   "cannotdisintegrate",
   "cannotuse",
   "cannotfreeze"


   };

char actor_notify_strings[ ACTOR_FLAG_MAX ][ 32 ] =
   {
   "on_damage",
   "on_killed",
   "on_spottedenemy"
   };


int Actor::ActorFlag_string_to_int(	const str &actorflagstr ) const
	{
	str test;

	for (int i = 0; i < ACTOR_FLAG_MAX; i++)
		{
		test = actor_flag_strings[i];
		if ( !actorflagstr.icmp( actor_flag_strings[ i ] ) )
			return i;
		}
	
	gi.WDPrintf( "Unknown actor flag - %s\n", actorflagstr.c_str() );
	return -1;
	}

qboolean Actor::GetActorFlag( int flag ) const
	{
	const unsigned int *flags_shadow1;
	int index;
	int bit = 0;

	if ( flag > ACTOR_FLAG_MAX )
		{
		gi.WDPrintf( "Actor flag %d out of range\n", flag );
		return false;
		}

	index = flag / 32;

	switch( index )
		{
		case 0 :
			flags_shadow1 = &actor_flags1;
			bit = flag;
			break;
		case 1 :
			flags_shadow1 = &actor_flags2;
			bit = flag - 32;
			break;
		case 2 :
			flags_shadow1 = &actor_flags3;
			bit = flag - 64;
			break;
		case 3 :
			flags_shadow1 = &actor_flags4;
			bit = flag - 96;
			break;

		default :
			gi.WDPrintf( "Actor flag %d out of range\n", flag );
			return false;
		}

	if ( *flags_shadow1 & ( 1 << bit ) )
		return true;
	else
		return false;
	}

void Actor::SpawnItems( void )
	{
	int number_of_spawn_items;
	int i;
	qboolean spawn_random = false;
	str spawn_item_name;
	Player *player;
	[[maybe_unused]] float health_chance;
	[[maybe_unused]] float plasma_chance;
	[[maybe_unused]] float bullets_chance;
	float player_health;
	int player_plasma;
	int player_bullets;

	if ( spawn_chance == 0.0f ) return ;

	number_of_spawn_items = spawn_items.NumObjects();


	if ( (number_of_spawn_items) && (G_Random( 100.0f) < spawn_chance) )
		{
		for( i = 1 ; i <= number_of_spawn_items ; i++ )
			{
			spawn_item_name = spawn_items.ObjectAt( i );

			if ( spawn_item_name == "random" )
				spawn_random = true;
			else
				SpawnItem( spawn_item_name );
			}
		}
	else
		{
		spawn_random = true;
		}


	if ( spawn_random )
		{
		if ( G_Random( 100.0f ) < spawn_chance )
			{

			health_chance  = 1.0f;
			plasma_chance  = 1.0f;
			bullets_chance = 1.0f;


			player = (Player *)g_entities[ 0 ].entity;

			player_health  = player->health;
			player_plasma  = player->AmmoCount( "Plasma" );
			player_bullets = player->AmmoCount( "Bullet" );


			if ( player_health <= 50.0f )
				health_chance *= ( 60.0f - player_health ) / 10.0f;

			if ( player_plasma <= 20.0f )
				plasma_chance *= ( 30.0f - player_plasma ) / 10.0f;

			if ( player_bullets <= 50.0f )
				bullets_chance *= ( 60.0f - player_bullets ) / 10.0f;

			}
		}
	}

void Actor::SpawnItem( const str &spawn_item_name )
	{
	SpawnArgs      args;
	Entity *ent;
	Item *item;


	args.setArg( "model", spawn_item_name );
	ent = args.Spawn();

	if ( !ent || !ent->isSubclassOf( Item ) )
		return;

	item = (Item *)ent;

	item->setOrigin( centroid );

	item->ProcessPendingEvents();

	item->PlaceItem();
	item->setOrigin( centroid );
	item->velocity = Vector( G_CRandom( 100.0f ), G_CRandom( 100.0f ), 200.0f + G_Random( 200.0f ) );
	item->edict->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;


	item->targetname = targetname;
	item->targetname += "_item";

	item->SetTargetName( item->targetname );
	}


void Actor::SetStickToGround( const bool stick )
{
	movementSubsystem->SetStickToGround( stick );
}

const bool Actor::GetStickToGround( void ) const
{
	return movementSubsystem->GetStickToGround();
}

void Actor::AnimDone(Event *ev)
{
    SetActorFlag(ACTOR_FLAG_ANIM_DONE, true);
}

void Actor::TorsoAnimDone(Event *ev)
{
    SetActorFlag(ACTOR_FLAG_TORSO_ANIM_DONE, true);
}

void Actor::DialogAnimDone(Event *ev)
{
    SetAnim("idle");
}

void Actor::CinematicAnimDone(void)
{
    Entity::CinematicAnimDone(); 
    movementSubsystem->setMovementType(MOVEMENT_TYPE_NORMAL);
}

void Actor::NotifyOthersOfDeath(void)
{
    int   i;
    Actor *act;

    for (i = 1; i <= ActiveList.NumObjects(); i++) {
        act = ActiveList.ObjectAt(i);

        if (name.length() && (name == act->name)) {
            act->AddStateFlag(STATE_FLAG_OTHER_DIED);
        }
    }
}

void AI_DisplayInfo( void )
   {
   if ( ai_numactive->integer )
		{
		gi.Printf( "Active actors - %d\n", ActiveList.NumObjects() );
		}
   }

part_t &part_t::operator=(const part_t &other)
{
	ent         = other.ent;
	state_flags = other.state_flags;

	return *this;
}
