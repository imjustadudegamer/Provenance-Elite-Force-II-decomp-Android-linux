//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/player.cpp                                $
// $Revision:: 590                                                            $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Class definition of the player.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "g_local.h"

#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#include "../qcommon/qa_probe.h"
#define WIDGET_PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define WIDGET_PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_widget.h"

#define PUSH_OBJECT_DISTANCE 16.0f
#include "mp_manager.hpp"
#include "bg_local.h"
#include "g_spawn.h"
#include "g_phys.h"
#include "entity.h"
#include "player.h"
#include "worldspawn.h"
#include "weapon.h"
#include "equipment.h"
#include "trigger.h"
#include "scriptmaster.h"
#include "scriptexception.h"
#include "navigate.h"
#include "misc.h"
#include "earthquake.h"
#include "gravpath.h"
#include "armor.h"
#include "inventoryitem.h"
#include "gibs.h"
#include "actor.h"
#include "actor_enemymanager.h" 
#include "usedata.h"      
#include "object.h"
#include "characterstate.h"
#include "viewthing.h"
#include "weaputils.h"
#include "body.h"
#include "playerstart.h"
#include "camera.h"
#include "../qcommon/gameplaymanager.h" 
#include "actor_locomotion.h" 

#define SLOPE_45_MIN    0.7071f
#define SLOPE_45_MAX    0.831f
#define SLOPE_22_MIN    SLOPE_45_MAX
#define SLOPE_22_MAX    0.95f

#define EARTHQUAKE_SCREENSHAKE_PITCH 2
#define EARTHQUAKE_SCREENSHAKE_YAW   2
#define EARTHQUAKE_SCREENSHAKE_ROLL  3

#define MELEE_ATTACK_LEFT  (1 << 0)
#define MELEE_ATTACK_RIGHT (1 << 1)

const Vector power_color(0.0, 1.0, 0.0);
const Vector acolor(1.0, 1.0, 1.0);
const Vector bcolor(1.0, 0.0, 0.0);
const Vector damageNormalColor(0.5f, 0.0f, 0.0f);  
const Vector damageFireColor(0.5f, 0.25f, 0.0f);   


Event EV_Player_SetDoDamageScreenFlash (    "doDamageScreenFlash",
    EV_DEFAULT,
    "B",
    "bool",
    "Makes the screen flash when the player is damaged (or turns it off).");
Event EV_Player_BackpackAttachOffset (    "backpackAttachOffset",
    EV_DEFAULT,
    "v",
    "offset",
    "Sets the attachment offset for backpacks");
Event EV_Player_BackpackAttachAngles (    "backpackAttachAngles",
    EV_DEFAULT,
    "v",
    "anglesOffset",
    "Sets the attachment angles offset for backpacks");
Event EV_Player_FlagAttachOffset (    "flagAttachOffset",
    EV_DEFAULT,
    "v",
    "offset",
    "Sets the attachment offset for flags");
Event EV_Player_FlagAttachAngles (    "flagAttachAngles",
    EV_DEFAULT,
    "v",
    "anglesOffset",
    "Sets the attachment angles offset for flags");
Event EV_Player_ValidPlayerModel (    "validPlayerModel",
    EV_TIKIONLY,
    NULL,
    NULL,
    "Specifies that this model is valid for the player.");
Event EV_Player_MeleeDamageStart (    "meleedamagestart",
    EV_DEFAULT,
    "S",
    "hand",
    "Start doing melee damage with the weapon in the specified hand.");
Event EV_Player_MeleeDamageEnd (    "meleedamageend",
    EV_DEFAULT,
    "S",
    "hand",
    "Stop doing melee damage with the weapon in the specified hand.");
extern Event EV_Player_DumpState;

Event EV_Player_ForceTorsoState (    "forcetorsostate",
    EV_DEFAULT,
    "s",
    "torsostate",
    "Force the player's torso to a certain state");
Event EV_Player_GiveAllCheat (    "wuss",
    EV_CHEAT,
    NULL,
    NULL,
    "Gives player all weapons.");
Event EV_Player_EndLevel (    "endlevel",
    EV_DEFAULT,
    NULL,
    NULL,
    "Called when the player gets to the end of the level.");
extern Event EV_Player_DevNoTargetCheat;

Event EV_Player_DevNoClipCheat (    "noclip",
    EV_CHEAT,
    NULL,
    NULL,
    "Toggles the noclip cheat.");
Event EV_Player_PrevItem (    "invprev",
    EV_CONSOLE,
    NULL,
    NULL,
    "Cycle to player's previous item.");
Event EV_Player_NextItem (    "invnext",
    EV_CONSOLE,
    NULL,
    NULL,
    "Cycle to player's next item.");
Event EV_Player_AnimateWeapon (    "animateweapon",
    EV_DEFAULT,
    "sSB",
    "anim hand animatingFlag",
    "Animates the weapon, optionally, in the specific hand (defaults to DUAL)If the animatingFlag is set to false, it will not set the WEAPON_ANIMATINGas the status for this animation.");
Event EV_Player_GiveCheat (    "give",
    EV_CHEAT | EV_TIKIONLY | EV_SCRIPTONLY,
    "sI",
    "name amount",
    "Gives the player the specified thing (weapon, ammo, item, etc.) and optionally the amount.");
Event EV_Player_GiveWeaponCheat (    "giveweapon",
    EV_CHEAT,
    "s",
    "weapon_name",
    "Gives the player the specified weapon.");
Event EV_Player_GameVersion (    "gameversion",
    EV_CONSOLE,
    NULL,
    NULL,
    "Prints the game version.");
Event EV_Player_Dead (    "dead",
    EV_CODEONLY,
    NULL,
    NULL,
    "Called when the player is dead.");
Event EV_Player_MissionFailed (    "missionfailed",
    EV_SCRIPTONLY,
    "S",
    "reason",
    "Displays the mission failed screen on the client side");
extern Event EV_ScriptThread_Spawn;

Event EV_Player_SpawnEntity (    "spawn",
    EV_CHEAT | EV_SCRIPTONLY,
    "sSSSSSSSS",
    "entityname keyname1 value1 keyname2 value2 keyname3 value3 keyname4 value4",
    "Spawns an entity."
);

Event EV_Player_SpawnActor (    "actor",
    EV_CHEAT,
    "sSSSSSSSS",
    "modelname keyname1 value1 keyname2 value2 keyname3 value3 keyname4 value4",
    "Spawns an actor.");
Event EV_Player_Respawn
(
	"respawn",
	EV_CODEONLY,
	NULL,
	NULL,
	"Respawns the player."
);

Event EV_Player_TestThread (    "testthread",
    EV_CHEAT,
    "sS",
    "scriptfile label",
    "Starts the named thread at label if provided.");
Event EV_Player_ResetState (    "resetstate",
    EV_CHEAT,
    NULL,
    NULL,
    "Reset the player's state table.");
Event EV_Player_WhatIs (    "whatis",
    EV_CHEAT,
    "i",
    "entity_number",
    "Prints info on the specified entity.");
Event EV_Player_ActorInfo (    "actorinfo",
    EV_CHEAT,
    "i",
    "actor_number",
    "Prints info on the specified actor.");
extern Event EV_Player_KillEnt;

extern Event EV_Player_KillClass;

extern Event EV_Player_RemoveEnt;

extern Event EV_Player_RemoveClass;

Event EV_Player_Jump (    "jump",
    EV_TIKIONLY,
    "f",
    "height",
    "Makes the player jump.");
extern Event EV_Warp;

extern Event EV_Actor_WarpTo;

Event EV_Player_AnimLoop_Torso (    "animloop_torso",
    EV_CODEONLY,
    NULL,
    NULL,
    "Called when the torso animation has finished.");
Event EV_Player_AnimLoop_Legs (    "animloop_legs",
    EV_CODEONLY,
    NULL,
    NULL,
    "Called when the legs animation has finished.");
Event EV_Player_DoUse (    "usestuff",
    EV_DEFAULT,
    "E",
    "usingEntity",
    "Makes the player try to use whatever is in front of her.");
Event EV_Player_ListInventory (    "listinventory",
    EV_CONSOLE,
    NULL,
    NULL,
    "List of the player's inventory.");
Event EV_Player_ActivateShield (    "activateshield",
    EV_DEFAULT,
    NULL,
    NULL,
    "Activates the player's shield");
Event EV_Player_DeactivateShield (    "deactivateshield",
    EV_DEFAULT,
    NULL,
    NULL,
    "Deactivates the player's shield");
Event EV_Player_Turn (    "turn",
    EV_SCRIPTONLY,
    "fF",
    "yawangle time",
    "Causes player to turn the specified amount.");
Event EV_Player_TurnUpdate (    "turnupdate",
    EV_SCRIPTONLY,
    "ff",
    "yaw timeleft",
    "Causes player to turn the specified amount.");
Event EV_Player_TurnLegs (    "turnlegs",
    EV_SCRIPTONLY,
    "f",
    "yawangle",
    "Turns the players legs instantly by the specified amount.");
Event EV_Player_DontTurnLegs (    "dontturnlegs",
    EV_CODEONLY,
    "b",
    "flag",
    "Specifies whether or not to turn the legs while strafing."
);

Event EV_Player_FinishUseAnim (    "finishuseanim",
    EV_DEFAULT,
    NULL,
    NULL,
    "Fires off all targets associated with a particular useanim.");
Event EV_Player_Holster (    "holster",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "Holsters all wielded weapons, or unholsters previously put away weapons");
Event EV_Player_SafeHolster (    "safeholster",
    EV_SCRIPTONLY,
    "b",
    "putaway",
    "Holsters all wielded weapons, or unholsters previously put away weapons\n"
    "preserves state, so it will not holster or unholster unless necessary");
Event EV_Player_StartUseObject (    "startuseobject",
    EV_DEFAULT,
    NULL,
    NULL,
    "starts up the useobject's animations.");
Event EV_Player_FinishUseObject (    "finishuseobject",
    EV_DEFAULT,
    NULL,
    NULL,
    "Fires off all targets associated with a particular useobject.");
Event EV_Player_WatchActor (    "watchactor",
    EV_SCRIPTONLY,
    "eFFB",
    "entity_to_watch time angle watchEntireDuration",
    "Makes the player's camera watch the specified entity."
);
Event EV_Player_StopWatchingEntity (    "stopwatchingactor",
    EV_SCRIPTONLY,
    "e",
    "entity_to_stop_watching",
    "Makes the player's camera stop watching the specified entity."
);
Event EV_Player_SetDamageMultiplier (    "damage_multiplier",
    EV_SCRIPTONLY,
    "f",
    "damage_multiplier",
    "Sets the current damage multiplier");
Event EV_Player_WaitForState (    "waitForState",
    EV_SCRIPTONLY,
    "s",
    "stateToWaitFor",
    "When set, the player will clear waitforplayer when this state is hit\n"
    "in the legs or torso.");
Event EV_Player_LogStats (    "logstats",
    EV_CHEAT,
    "b",
    "state",
    "Turn on/off the debugging playlog");
Event EV_Player_TakePain (    "takepain",
    EV_DEFAULT,
    "b",
    "bool",
    "Set whether or not to take pain");
Event EV_Player_SkipCinematic (    "skipcinematic",
    EV_CONSOLE,
    NULL,
    NULL,
    "Skip the current cinematic");
Event EV_Player_ResetHaveItem (    "resethaveitem",
    EV_CONSOLE,
    "s",
    "weapon_name",
    "Resets the game var that keeps track that we have gotten this weapon");
Event EV_Player_Score (    "score",
    EV_CONSOLE,
    NULL,
    NULL,
    "Show the score for the current deathmatch game");
Event EV_Player_DeadBody (    "deadbody",
    EV_CODEONLY,
    NULL,
    NULL,
    "Spawn a dead body");




extern Event EV_CameraManager_Fov;

extern Event EV_CameraManager_Speed;



qboolean TryPush(int entnum, vec3_t move_origin, vec3_t move_end);



Event EV_Player_Immobilize (    "immobilize",
    EV_DEFAULT,
    "B",
    "immobilize",
    "Immobilizes the player until the immobilize 0 is called (can only look around)");

Event EV_Player_ClearItemText (    "clearItemText",
    EV_CODEONLY,
    NULL,
    NULL,
    "Clears the item text.");
Event EV_Player_LoadObjectives (    "loadobjectives",
    EV_SCRIPTONLY,
    "s",
    "name",
    "Loads the a set of objectives");
Event EV_Player_SetObjectiveShow (    "setobjectiveshow",
    EV_SCRIPTONLY,
    "sb",
    "name show",
    "Sets whether or not a specified objective is shown");
Event EV_Player_SetObjectiveComplete (    "setobjectivecomplete",
    EV_SCRIPTONLY,
    "sb",
    "name complete",
    "Sets whether or not a specified objective is complete");
Event EV_Player_SetObjectiveFailed (    "setobjectivefailed",
    EV_SCRIPTONLY,
    "sb",
    "name failed",
    "Sets whether or not a specified objective is failed");
Event EV_Player_SetInformationShow (    "setinformationshow",
    EV_SCRIPTONLY,
    "sb",
    "name show",
    "Sets whether or not a specified information is shown");

extern Event EV_Player_SetViewAngles;

extern Event EV_Player_UseEntity;

extern Event EV_Player_SetStat;


extern Event EV_Weapon_ForceReload;


Event EV_Player_ShowHeuristics
(
	"showheuristics",
	EV_CHEAT,
	NULL,
	NULL,
	"Shows the current heuristic numbers"
);
Event EV_Player_ActivateNewWeapon
(
	"activatenewweapon",
	EV_DEFAULT,
	NULL,
	NULL,
	"Active the new weapon specified by useWeapon."
);
Event EV_Player_DeactivateWeapon
(
	"deactivateweapon",
	EV_DEFAULT,
	"s",
	"side",
	"Deactivate the weapon in the specified hand."
);
Event EV_Player_SetHeadTarget
(
	"headtarget",
	EV_CONSOLE,
	NULL,
	NULL,
	"Sets the Makes the player try to use whatever is in front of her."
);
Event EV_Player_ClearTarget
(
	"cleartarget",
	EV_DEFAULT,
	NULL,
	NULL,
	"Clears the target of the player"
);
Event EV_Player_AdjustTorso
(
	"adjust_torso",
	EV_DEFAULT,
	"b",
	"boolean",
	"Turn or off the torso adjustment"
);
Event EV_Player_UseDualWield
(
	"usedualwield",
	EV_CONSOLE,
	NULL,
	NULL,
	"Use the weapons that are on the dual wield list"
);
Event EV_Player_EvaluateTorsoAnim
(
	"evaluatetorsoanim",
	EV_CONSOLE,
	NULL,
	NULL,
	"Evaluate the torso anim for possible changes"
);
Event EV_Player_TurnTowardsEntity
(
	"turnTowardsEntity",
	EV_SCRIPTONLY,
	"e",
	"entity",
	"Causes the player to turn towards the specified entity."
);
Event EV_Player_Nightvision
(
	"nightvision",
	EV_CONSOLE | EV_CODEONLY,
	NULL,
	NULL,
	"Toggles player nightvision mode"
);
Event EV_Player_WatchEntity
(
	"watchentity",
	EV_SCRIPTONLY,
	"eFFB",
	"entityToWatch time angle watchEntireDuration",
	"Makes the player's camera watch the specified entity."
);
Event EV_Player_PutawayWeapon
(
	"putawayweapon",
	EV_DEFAULT,
	"s",
	"whichHand",
	"Put away or deactivate the current weapon, whichHand can be left, right or dual."
);
Event EV_Player_Weapon
(
	"weaponcommand",
	EV_CODEONLY,
	"sSSSSSSS",
	"hand arg1 arg2 arg3 arg4 arg5 arg6 arg7",
	"Pass the args to the active weapon in the specified hand"
);
Event EV_Player_ActivateDualWeapons
(
	"activatedualweapons",
	EV_DEFAULT,
	NULL,
	NULL,
	"Activates 2 weapons at once"
);
Event EV_Player_StartCoolItem
(
	"startcoolitem",
	EV_CODEONLY,
	NULL,
	NULL,
	"Player is starting to show off the cool item"
);
Event EV_Player_StopCoolItem
(
	"stopcoolitem",
	EV_CODEONLY,
	NULL,
	NULL,
	"Player is starting to show off the cool item"
);
Event EV_Player_ShowCoolItem
(
	"showcoolitem",
	EV_CODEONLY,
	NULL,
	NULL,
	"Player is showing the cool item, actually display it"
);
Event EV_Player_HideCoolItem
(
	"hidecoolitem",
	EV_CODEONLY,
	NULL,
	NULL,
	"Player is finished showing the cool item, now hide it"
);
Event EV_Player_CallVote
(
	"callvote",
	EV_CONSOLE,
	"sS",
	"arg1 arg2",
	"Player calls a vote"
);
Event EV_Player_Vote
(
	"vote",
	EV_CONSOLE,
	"s",
	"arg1",
	"Player votes either yes or no"
);
Event EV_Player_JoinTeam
(
	"joinmpteam",
	EV_CONSOLE,
	"s",
	"teamName",
	"Makes the player join the specified team."
);
Event EV_Player_MultiplayerCommand
(
	"mpCmd",
	EV_CONSOLE,
	"ss",
	"command parm",
	"Sends a command from the player to the multiplayer system."
);
Event EV_Player_SetAimType
(
	"setaim",
	EV_DEFAULT,
	"s",
	"aimtype",
	"Sets the accuracy modifiers for the player"
);
Event EV_Player_ReloadWeapon
(
	"forcereload",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Tells the weapon to reload"
);
Event EV_Player_SwitchWeaponMode
(
	"switchmode",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Tells the weapon to switch modes of fire"
);
Event EV_Player_ReloadTiki
(
	"reloadviewtiki",
	EV_CHEAT,
	"s",
	"tikiname",
	"Reloads the specified TIKI file from disk"
);
Event EV_Player_ProjDetonate
(
	"projdetonate",
	EV_CODEONLY,
	"b",
	"detonate",
	"Event that sets the whether or not to detonate a trigger projectile (grenade launcher)"
);
Event EV_Player_SetupDialog
(
	"setupdialog",
	EV_CODEONLY,
	"is",
	"entnum soundname",
	"Sets up the dialog for the player"
);
Event EV_Player_SpecialMoveChargeStart
(
	"specialmovestart",
	EV_CODEONLY,
	NULL,
	NULL,
	"Starts charging up for a special move"
);
Event EV_Player_SpecialMoveChargeEnd
(
	"specialmoveend",
	EV_CODEONLY,
	NULL,
	NULL,
	"Ends charging up for a special move"
);
Event EV_Player_SpecialMoveChargeTime
(
	"specialmovetime",
	EV_CODEONLY,
	"f",
	"time",
	"Sets how long to wait before triggering the special move."
);
Event EV_Player_ChangeChar
(
	"changechar",
	EV_SCRIPTONLY,
	"ssss",
	"statefile model replacemodelName spawnNPC",
	"Changes the character state and model."
);
Event EV_Player_SetPlayerCharacter
(
	"setplayerchar",
	EV_SCRIPTONLY,
	"ss",
	"statefile model",
	"Sets the player character."
);
Event EV_Player_PlayerKnockback
(
	"playerknockback",
	EV_CODEONLY,
	"f",
	"knockback",
	"Sets knockback for the player"
);
Event EV_Player_KnockbackMultiplier
(
	"knockbackmultiplier",
	EV_CODEONLY,
	"i",
	"knockbackmult",
	"Sets knockback multiplierfor the player"
);
Event EV_Player_Melee
(
	"melee",
	EV_DEFAULT,
	"FSF",
	"damage means_of_death knockback",
	"Makes the player do a weapon-less melee attack. "
);
Event EV_Player_ChangeStance
(
	"stance",
	EV_CONSOLE,
	"i",
	"stanceNumber",
	"Makes the player change to the specified stance"
);
Event EV_Player_ClearStanceTorso
(
	"clearstancetorso",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears internal torso stance data."
);
Event EV_Player_ClearStanceLegs
(
	"clearstancelegs",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears internal legs stance data."
);
Event EV_Player_GivePoints
(
	"givepoints",
	EV_SCRIPTONLY,
	"i",
	"points",
	"Gives the player points (added to current points)"
);
Event EV_Player_SetPoints
(
	"setpoints",
	EV_SCRIPTONLY,
	"i",
	"points",
	"Sets the players number of points."
);
Event EV_Player_ClearIncomingMelee
(
	"clearincomingmelee",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears the incoming melee flag."
);
Event EV_Player_BendTorso
(
	"bendtorso",
	EV_DEFAULT,
	"F",
	"multiplier",
	"Sets multiplier for the torso bend angles, defaults to 0.75"
);
Event EV_Player_DropRune
(
	"droprune",
	EV_CODEONLY,
	NULL,
	NULL,
	"Drops the player's current rune."
);
Event EV_Player_SetCanTransferEnergy
(
	"canTransferEnergy",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the player able to transfer energy from ammo to armor."
);
Event EV_Player_PointofView
(
	"pointofview",
	EV_DEFAULT,
	NULL,
	NULL,
	"Changes the viewpoint of the player, alternates between 1st and 3rd person"
);
Event EV_Player_FinishingMove
(
	"finishingmove",
	EV_TIKIONLY,
	"ssFFFF",
	"state direction coneangle distance enemyyaw chance",
	"Changes the viewpoint of the player, alternates between 1st and 3rd person"
);
Event EV_Player_ClearFinishingMove
(
	"clearfinishingmovelist",
	EV_DEFAULT,
	NULL,
	NULL,
	"Clears the finishing move list."
);
Event EV_Player_DoFinishingMove
(
	"dofinishingmove",
	EV_DEFAULT,
	NULL,
	NULL,
	"Fires off the finishing move."
);
Event EV_Player_ForceTimeScale
(
	"forcetimescale",
	EV_DEFAULT,
	"F",
	"timescale",
	"Sets the timescale for the game"
);
Event EV_Player_Freeze
(
	"freeze",
	EV_DEFAULT,
	"B",
	"freeze",
	"Freezes the player until the freeze 0 is called"
);
Event EV_Player_DoUseEntity
(
	"douseentity",
	EV_DEFAULT,
	NULL,
	NULL,
	"Called from the tiki on a frame to use an entity."
);
Event EV_Player_DoneUseEntity
(
	"doneuseentity",
	EV_DEFAULT,
	NULL,
	NULL,
	"When the useentity animation is complete"
);
Event EV_Player_SetAttackType
(
	"attacktype",
	EV_DEFAULT,
	"s",
	"attacktype",
	"Sets the attack type of the attack"
);
Event EV_Player_NextGameplayAnim
(
	"nextgameplayanim",
	EV_CODEONLY,
	"s",
	"objname",
	"Increments the gameplay animation index."
);
Event EV_Player_SetGameplayAnim
(
	"setgameplayanim",
	EV_CODEONLY,
	"I",
	"index",
	"Sets the gameplay animation index directly."
);
Event EV_Player_DisableUseWeapon
(
	"disableuseweapon",
	EV_DEFAULT,
	"B",
	"disable",
	"Disables the weapon use"
);
Event EV_Player_DisableInventory
(
	"disableinventory",
	EV_DEFAULT,
	"f",
	"disable",
	"Disables the player's inventory"
);
Event EV_Player_EquipItems
(
	"equipitems",
	EV_DEFAULT,
	NULL,
	NULL,
	"Equips active items from the database."
);
Event EV_Player_RemoveRosterTeammate1
(
	"removerosterteammate1",
	EV_DEFAULT,
	NULL,
	NULL,
	"Removed the teammate from roster position 1"
);
Event EV_Player_RemoveRosterTeammate2
(
	"removerosterteammate2",
	EV_DEFAULT,
	NULL,
	NULL,
	"Removed the teammate from roster position 2"
);
Event EV_Player_RemoveRosterTeammate4
(
	"removerosterteammate4",
	EV_DEFAULT,
	NULL,
	NULL,
	"Removed the teammate from roster position 4"
);
Event EV_Player_HudPrint
(
	"hudPrint",
	EV_DEFAULT,
	"s",
	"string",
	"Prints to the hud."
);
Event EV_Player_AddHud
(
	"addHud",
	EV_SCRIPTONLY,
	"s",
	"hudName",
	"Tells the player to add a hud to his display."
);
Event EV_Player_RemoveHud
(
	"removeHud",
	EV_SCRIPTONLY,
	"s",
	"hudName",
	"Tells the player to remove a hud from his display."
);
Event EV_Player_KillAllDialog
(
	"killalldialog",
	EV_DEFAULT,
	NULL,
	NULL,
	"Calls stopdialog on all the actors in the level"
);
Event EV_Player_ForceMoveType
(
	"forceMoveType",
	EV_DEFAULT,
	"s",
	"moveTypeName",
	"Forces the player to use the named move type"
);
extern Event EV_Player_IsOnGround;

Event EV_Player_BackupModel
(
	"backupModel",
	EV_DEFAULT,
	"s",
	"backupModelName",
	"Sets the name of the model to use as backup if the client doesn't have this one"
);

extern Event EV_Gib;

extern Event EV_Sentient_SetMouthAngle;

extern Event EV_Sentient_HeadWatchAllowed;

extern Event EV_Sentient_AddMeleeAttacker;

extern Event EV_Driver_AnimDone;

Event EV_Player_AddRosterTeammate1
(
	"addrosterteammate1",
	EV_DEFAULT,
	"e",
	"entity",
	"Sets the teammate to roster position 1"
);
Event EV_Player_AddRosterTeammate2
(
	"addrosterteammate2",
	EV_DEFAULT,
	"e",
	"entity",
	"Sets the teammate to roster position 2"
);
Event EV_Player_AddRosterTeammate3
(
	"addrosterteammate3",
	EV_DEFAULT,
	"e",
	"entity",
	"Sets the teammate to roster position 3"
);
Event EV_Player_AddRosterTeammate4
(
	"addrosterteammate4",
	EV_DEFAULT,
	"e",
	"entity",
	"Sets the actor to roster position 4"
);
Event EV_Player_ChangeCharFadeIn
(
	"changecharfadein",
	EV_CODEONLY,
	NULL,
	NULL,
	"Begins the fade in and swaps character."
);
Event EV_Player_ClearDialog
(
	"cleardialog",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears the dialog for the player"
);
Event EV_Player_ClearTextDialog
(
	"cleartextdialog",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears the text dialog of the player"
);
Event EV_Player_DevGodCheat
(
	"god",
	EV_CHEAT,
	"I",
	"god_mode",
	"Sets the god mode cheat or toggles it."
);
Event EV_Player_Done
(
	"playerdone",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears the waitForPlayer script command, allowing threads to run"
);
Event EV_Player_DualWield
(
	"dualwield",
	EV_CONSOLE,
	"ss",
	"weaponleft weaponright",
	"Dual wield the specified weapons"
);
Event EV_Player_NextPainTime
(
	"nextpaintime",
	EV_CODEONLY,
	"f",
	"seconds",
	"Set the next time the player experiences pain (Current time + seconds specified)."
);
Event EV_Player_RemovePowerup
(
	"removepowerup",
	EV_CODEONLY,
	NULL,
	NULL,
	"Removes the current powerup."
);
Event EV_Player_RemoveRosterTeammate3
(
	"removerosterteammate3",
	EV_DEFAULT,
	NULL,
	NULL,
	"Removed the teammate from roster position 3"
);
Event EV_Player_UseItem
(
	"use",
	EV_CONSOLE,
	"sI",
	"name weapon_hand",
	"Use the specified weapon in the hand choosen (optional)."
);

Event EV_Player_Fov
(
	"fov",
	EV_CONSOLE,
	"F",
	"fov",
	"Sets the fov."
);

Event EV_Player_DevNoTargetCheat (    "notarget",
    EV_CHEAT,
    NULL,
    NULL,
    "Toggles the notarget cheat.");

Event EV_Player_IsOnGround (    "isplayeronground",
    EV_DEFAULT,
    "@f",
    "onground",
    "Checks if the player is on the ground -- returns 1 if true 0 if false");

Event EV_Player_SetViewAngles (    "playerviewangles",
    EV_CHEAT,
    "v[0,360][0,360][0,360]",
    "newAngles",
    "set the angles of the player to newAngles.");

Event EV_Player_SetStat (    "setstat",
    EV_SCRIPTONLY,
    "si",
    "stat_name stat_value",
    "Sets a stat value");

Event EV_Player_UseEntity (    "useentity",
    EV_DEFAULT,
    "e",
    "entity",
    "makes player use the passed in entity");

Event EV_Player_KillEnt (	"killent", 
	EV_CHEAT | EV_SCRIPTONLY,
	"i",
	"entity_number",
	"Kills the specified entity.");

Event EV_Player_KillClass (	"killclass", 
	EV_CHEAT | EV_SCRIPTONLY,
	"sI",
	"classname except_entity_number",
	"Kills all of the entities in the specified class.");

Event EV_Player_RemoveEnt (	"removeent", 
	EV_CHEAT | EV_SCRIPTONLY,
	"i",
	"entity_number",
	"Removes the specified entity."
);

Event EV_Player_RemoveClass (	"removeclass", 
	EV_CHEAT | EV_SCRIPTONLY,
	"sI",
	"classname except_entity_number",
	"Removes all of the entities in the specified class.");

Event EV_Player_DumpState (    "state",
    EV_CHEAT,
    NULL,
    NULL,
    "Dumps the player's state to the console.");

CLASS_DECLARATION(Sentient, Player, "player") {

	{ &EV_ClientMove,										&Player::ClientThink },
	{ &EV_ClientEndFrame,									&Player::EndFrame },
	{ &EV_Vehicle_Enter,									&Player::EnterVehicle },
	{ &EV_Vehicle_Exit,										&Player::ExitVehicle },
	{ &EV_Player_EndLevel,									&Player::EndLevel },
	{ &EV_Player_UseItem,									&Player::EventUseItem },
	{ &EV_Player_GiveCheat,									&Player::GiveCheat },
	{ &EV_Player_GiveWeaponCheat,							&Player::GiveWeaponCheat },
	{ &EV_Player_GiveAllCheat,								&Player::GiveAllCheat },
	{ &EV_Player_DevGodCheat,								&Player::GodCheat },
	{ &EV_Player_DevNoTargetCheat,							&Player::NoTargetCheat },
	{ &EV_Player_DevNoClipCheat,							&Player::NoclipCheat },
	{ &EV_Player_GameVersion,								&Player::GameVersion },
	{ &EV_Player_DumpState,	    							&Player::DumpState },
	{ &EV_Player_ForceTorsoState,							&Player::ForceTorsoState },
	{ &EV_Player_Fov,										&Player::Fov },
	{ &EV_Kill,												&Player::Kill },
	{ &EV_Player_Dead,										&Player::Dead },
	{ &EV_Player_SpawnEntity,								&Player::SpawnEntity },
	{ &EV_Player_SpawnActor,								&Player::SpawnActor },
	{ &EV_Player_Respawn,									&Player::Respawn },
	{ &EV_Player_DoUse,										&Player::DoUse },
	{ &EV_Damage,											&Player::ArmorDamage },
	{ &EV_Pain,												&Player::Pain },
	{ &EV_Killed,											&Player::Killed },
	{ &EV_Gib,												&Player::GibEvent },
	{ &EV_GotKill,											&Player::GotKill },
	{ &EV_Player_TestThread,								&Player::TestThread },
	{ &EV_Player_ResetState,								&Player::ResetState },
	{ &EV_Player_WhatIs,									&Player::WhatIs },
	{ &EV_Player_ActorInfo,									&Player::ActorInfo },
	{ &EV_Player_ShowHeuristics,							&Player::ShowHeuristics	},
	{ &EV_Player_KillEnt,									&Player::KillEnt },
	{ &EV_Player_RemoveEnt,									&Player::RemoveEnt },
	{ &EV_Player_KillClass,									&Player::KillClass },
	{ &EV_Player_RemoveClass,								&Player::RemoveClass },
	{ &EV_Player_AnimLoop_Legs,								&Player::EndAnim_Legs },
	{ &EV_Player_AnimLoop_Torso,							&Player::EndAnim_Torso },
	{ &EV_Player_Jump,               						&Player::Jump },
	{ &EV_Sentient_JumpXY,									&Player::JumpXY },
	{ &EV_Player_ActivateNewWeapon,							&Player::ActivateNewWeapon },
	{ &EV_Player_DeactivateWeapon,							&Player::DeactivateWeapon },
	{ &EV_Player_SetHeadTarget,								&Player::SetHeadTarget },
	{ &EV_Player_ListInventory,								&Player::ListInventoryEvent },
	{ &EV_Player_ClearTarget,								&Player::ClearTarget },
	{ &EV_Player_ActivateShield,							&Player::ActivateShield },
	{ &EV_Player_DeactivateShield,							&Player::DeactivateShield },
	{ &EV_Player_AdjustTorso,								&Player::AdjustTorso },
	{ &EV_Player_DualWield,									&Player::DualWield },
	{ &EV_Player_UseDualWield,								&Player::UseDualWield },
	{ &EV_Player_EvaluateTorsoAnim,							&Player::EvaluateTorsoAnim },
	{ &EV_Player_NextPainTime,								&Player::NextPainTime },
	{ &EV_Player_Turn,										&Player::Turn },
	{ &EV_Player_TurnTowardsEntity,							&Player::turnTowardsEntity },
	{ &EV_Player_TurnUpdate,								&Player::TurnUpdate },
	{ &EV_Player_TurnLegs,									&Player::TurnLegs },
	{ &EV_Player_DontTurnLegs,								&Player::DontTurnLegs },
	{ &EV_Player_FinishUseAnim,								&Player::FinishUseAnim },
	{ &EV_Sentient_SetMouthAngle,							&Player::SetMouthAngle },
	{ &EV_Player_Holster,									&Player::HolsterToggle },
	{ &EV_Player_Nightvision,								&Player::NightvisionToggle },
	{ &EV_Player_SafeHolster,								&Player::Holster },
	{ &EV_Player_StartUseObject,							&Player::StartUseObject },
	{ &EV_Player_FinishUseObject,							&Player::FinishUseObject },
	{ &EV_Player_WatchActor,								&Player::WatchEntity },
	{ &EV_Player_WatchEntity,								&Player::WatchEntity },
	{ &EV_Player_StopWatchingEntity,						&Player::StopWatchingEntity },
	{ &EV_Player_PutawayWeapon,								&Player::PutawayWeapon },
	{ &EV_Player_Weapon,									&Player::WeaponCommand },
	{ &EV_Player_Done,										&Player::PlayerDone },
	{ &EV_Player_ActivateDualWeapons,						&Player::ActivateDualWeapons },
	{ &EV_Player_StartCoolItem,								&Player::StartCoolItem },
	{ &EV_Player_StopCoolItem,								&Player::StopCoolItem },
	{ &EV_Player_ShowCoolItem,								&Player::ShowCoolItem },
	{ &EV_Player_HideCoolItem,								&Player::HideCoolItem },
	{ &EV_Player_SetDamageMultiplier,						&Player::SetDamageMultiplier },
	{ &EV_Player_WaitForState,								&Player::WaitForState },
	{ &EV_Player_LogStats,									&Player::LogStats },
	{ &EV_Player_TakePain,									&Player::SetTakePain },
	{ &EV_Player_SkipCinematic,								&Player::SkipCinematic },
	{ &EV_Player_ResetHaveItem,								&Player::ResetHaveItem },
	{ &EV_Show,												&Player::PlayerShowModel },
	{ &EV_Player_Score,										&Player::Score },
	{ &EV_Player_CallVote,									&Player::CallVote },
	{ &EV_Player_Vote,										&Player::Vote },
	{ &EV_Player_JoinTeam,									&Player::joinTeam },
	{ &EV_Player_MultiplayerCommand,						&Player::multiplayerCommand },
	{ &EV_Player_DeadBody,									&Player::DeadBody },
	{ &EV_Player_SetAimType,								&Player::SetAimType },
	{ &EV_Player_ReloadWeapon,								&Player::ReloadWeapon },
	{ &EV_Player_AnimateWeapon,								&Player::AnimateWeapon },	
	{ &EV_Player_SwitchWeaponMode,							&Player::SwitchWeaponMode },
	{ &EV_Player_ReloadTiki,								&Player::ReloadTiki },
	{ &EV_Player_SetViewAngles,								&Player::SetViewAnglesEvent },
	{ &EV_Player_ProjDetonate,								&Player::ProjDetonate },
	{ &EV_Player_UseEntity,									&Player::UseSpecifiedEntity },
	{ &EV_Player_SetupDialog,								&Player::SetupDialog },
	{ &EV_Player_ClearDialog,								&Player::ClearDialog },
	{ &EV_Player_ClearTextDialog,							&Player::ClearTextDialog },
	{ &EV_Player_LoadObjectives,							&Player::LoadObjectives },
	{ &EV_Player_SetObjectiveShow,							&Player::SetObjectiveShow },
	{ &EV_Player_SetObjectiveComplete,						&Player::SetObjectiveComplete },
	{ &EV_Player_SetObjectiveFailed,						&Player::SetObjectiveFailed },
	{ &EV_Player_SetInformationShow,						&Player::SetInformationShow },
	{ &EV_Player_MissionFailed,								&Player::MissionFailed },
	   
	{ &EV_Player_SetStat,									&Player::SetStat },
	{ &EV_Player_SpecialMoveChargeStart,					&Player::SpecialMoveChargeStart },
	{ &EV_Player_SpecialMoveChargeEnd,						&Player::SpecialMoveChargeEnd },
	{ &EV_Player_SpecialMoveChargeTime,						&Player::SpecialMoveChargeTime },
	{ &EV_Player_ChangeChar,								&Player::ChangeChar },
	{ &EV_Player_SetPlayerCharacter,						&Player::SetPlayerChar },
	{ &EV_Player_PlayerKnockback,							&Player::PlayerKnockback },
	{ &EV_Player_KnockbackMultiplier,						&Player::KnockbackMultiplier },
	{ &EV_Player_Melee,										&Player::MeleeEvent },
	{ &EV_Player_ChangeStance,								&Player::ChangeStance },
	{ &EV_Player_ClearStanceTorso,							&Player::ClearStanceTorso },
	{ &EV_Player_ClearStanceLegs,							&Player::ClearStanceLegs },
	   
	{ &EV_Player_MeleeDamageStart,							&Player::MeleeDamageStart },
	{ &EV_Player_MeleeDamageEnd,							&Player::MeleeDamageEnd },
	{ &EV_Player_GivePoints,								&Player::GivePointsEvent },
	{ &EV_Player_SetPoints,									&Player::SetPointsEvent },
	{ &EV_Player_ChangeCharFadeIn,							&Player::ChangeCharFadeIn },
	{ &EV_Player_ClearIncomingMelee,						&Player::ClearIncomingMelee },
	{ &EV_Player_BendTorso,									&Player::SetBendTorso },
	{ &EV_Sentient_HeadWatchAllowed,						&Player::HeadWatchAllowed },
	   
	{ &EV_Player_RemovePowerup,								&Player::removePowerupEvent },
	{ &EV_Player_DropRune,									&Player::dropRune },
	{ &EV_Sentient_AddMeleeAttacker,						&Player::AddMeleeAttacker },
	   
	{ &EV_Player_SetCanTransferEnergy,						&Player::setCanTransferEnergy },
	{ &EV_Player_SetDoDamageScreenFlash,					&Player::setDoDamageScreenFlash },
	{ &EV_Player_PointofView,								&Player::pointOfView },
	{ &EV_Player_FinishingMove,								&Player::addFinishingMove },
	{ &EV_Player_ClearFinishingMove,						&Player::clearFinishingMove },
	{ &EV_Player_DoFinishingMove,							&Player::doFinishingMove },
	{ &EV_Player_ForceTimeScale,							&Player::forceTimeScale },
	{ &EV_Player_Freeze,									&Player::freezePlayer },
	{ &EV_Player_Immobilize,								&Player::immobilizePlayer },
	{ &EV_Player_DoUseEntity,								&Player::doUseEntity },
	{ &EV_Player_DoneUseEntity,								&Player::doneUseEntity },
	{ &EV_Player_SetAttackType,								&Player::setAttackType },
	{ &EV_Player_NextGameplayAnim,							&Player::nextGameplayAnim },
	{ &EV_Player_SetGameplayAnim,							&Player::setGameplayAnim },
	{ &EV_Player_DisableUseWeapon,							&Player::setDisableUseWeapon },
	{ &EV_Player_DisableInventory,							&Player::setDisableInventory },
	{ &EV_Player_EquipItems,								&Player::equipItems },
	   
	{ &EV_Use,												&Player::usePlayer },
	   
	{ &EV_Player_AddRosterTeammate1,						&Player::addRosterTeammate1 },
	{ &EV_Player_AddRosterTeammate2,						&Player::addRosterTeammate2 },
	{ &EV_Player_AddRosterTeammate3,						&Player::addRosterTeammate3 },
	{ &EV_Player_AddRosterTeammate4,						&Player::addRosterTeammate4 },
	   
	{ &EV_Player_RemoveRosterTeammate1,						&Player::removeRosterTeammate1 },
	{ &EV_Player_RemoveRosterTeammate2,						&Player::removeRosterTeammate2 },
	{ &EV_Player_RemoveRosterTeammate3,						&Player::removeRosterTeammate3 },
	{ &EV_Player_RemoveRosterTeammate4,						&Player::removeRosterTeammate4 },

	{ &EV_Player_HudPrint,									&Player::hudPrint },

	{ &EV_Player_ClearItemText,								&Player::clearItemText },

	{ &EV_Player_ValidPlayerModel,							&Player::setValidPlayerModel },

	{ &EV_Player_AddHud,									&Player::addHud },
	{ &EV_Player_RemoveHud,									&Player::removeHud },
		   
	{ &EV_Driver_AnimDone,									&Player::PassEventToVehicle },

	{ &EV_Warp,												&Player::warp },
	{ &EV_Player_KillAllDialog,								&Player::killAllDialog	},

	{ &EV_Player_ForceMoveType,								&Player::forceMoveType },
	{ &EV_Player_IsOnGround,								&Player::isPlayerOnGround },

	{ &EV_Player_BackpackAttachOffset,						&Player::setBackpackAttachOffset },
	{ &EV_Player_BackpackAttachAngles,						&Player::setBackpackAttachAngles },

	{ &EV_Player_FlagAttachOffset,							&Player::setFlagAttachOffset },
	{ &EV_Player_FlagAttachAngles,							&Player::setFlagAttachAngles },
	
	{ &EV_Player_BackupModel,								&Player::setBackupModel },
	{ NULL, NULL }
}
;

movecontrolfunc_t Player::MoveStartFuncs[] = {
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    NULL,                      
    &Player::StartPush,        
    NULL,                      
    &Player::StartUseAnim,     
    NULL,                      
    &Player::StartLoopUseAnim, 
    &Player::SetupUseObject,   
    NULL,                      
    NULL,                      
};

Player::Player()
{
    _backpackAttachOffset = Vector(-12, -9, 0);
    _backpackAttachAngles = Vector(0, 90, 90);
    _flagAttachOffset     = Vector(-96, -16, 0);
    _flagAttachAngles     = Vector(0, 90, 90);


    _viewMode = 0;

    addAffectingViewModes(gi.GetViewModeClassMask("player"));

    _totalGameFrames  = 0;
    _updateGameFrames = true;
    _secretsFound     = 0;
    _skillLevel       = 0;

    _itemIcon = 0;
    _itemText = "";


    last_camera_type             = -1;


    useanim_numloops            = 1;

    music_current_volume   = -1;
    music_saved_volume     = -1;
    music_volume_fade_time = -1;

    feetfalling        = false;
    edict->s.eType     = ET_PLAYER;
    buttons     = 0;
    new_buttons = 0;
    respawn_time       = -1.0;

    statemap_Legs      = NULL;
    statemap_Torso     = NULL;


    camera         = NULL;
    atobject       = NULL;
    atobject_dist  = 0;
    toucheduseanim = NULL;
    useitem_in_use = NULL;

    damage_blood = 0;
    damage_count = 0;
    damage_from  = vec_zero;
    damage_alpha = 0;

    last_attack_button   = 0;
    attack_blocked       = false;
    canfall              = false;

    moveresult                = 0;
    animspeed                 = 0;
    airspeed                  = 350.0f;
    weapons_holstered_by_code = false;
    actor_camera              = NULL;

    damage_multiplier = 1.0f;
    take_pain         = true;


    fov = sv_defaultFov->value;

    action_level = 0;

    {
        const char *s = Info_ValueForKey(client->pers.userinfo, "mp_autoSwitchWeapons");

        if (s && s[0]) {
            _autoSwitchWeapons = atoi(s) != 0;
        } else {
            cvar_t *cv = gi.cvar("mp_autoSwitchWeapons", "1", CVAR_ARCHIVE | CVAR_USERINFO);
            _autoSwitchWeapons = cv && cv->integer;
        }
    }

#ifdef DEDICATED
    p_heuristics = 0;
#else
    p_heuristics = new PlayerHeuristics;
#endif

    G_EntTrash_Pointer("spawn", this, p_heuristics);

    if (!LoadingSavegame) {
        SetTargetName("player");
    }

    Init();

    client->ps.pm_flags &= ~PMF_NO_HUD;
}

Player::~Player()
{
    Sentient *player;

    if (UIVK_EntTrashOn()) {
        gi.Printf(
            "[UIVK:enttrash] ~Player ent=%d class='%s' p_heuristics=%p"
            " low32=0x%08x high32=0x%08x frame=%d ltime=%.3f\n",
            entnum,
            getClassname() ? getClassname() : "?",
            (void *)p_heuristics,
            (unsigned)((uintptr_t)p_heuristics & 0xFFFFFFFFu),
            (unsigned)(((uintptr_t)p_heuristics >> 32) & 0xFFFFFFFFu),
            level.framenum,
            level.time);
    }

    G_EntTrash_Pointer("~Player", this, p_heuristics);

    if ( p_heuristics )
    {
        p_heuristics->SaveHeuristics( this );
        delete p_heuristics;
    }

    player = this;

    if ( TeamMateList.ObjectInList( player ) )
    {
        TeamMateList.RemoveObject( player );
    }

    if ( edict->svflags & SVF_BOT )
    {
        BotAIShutdownClient( entnum, qfalse );
    }

    edict->s.modelindex = 0;

    freeConditionals( legs_conditionals );
    freeConditionals( torso_conditionals );

    removePowerup();
    removeRune();
    removeHoldableItem();

    clearFinishingMove(NULL);

    if ( multiplayerManager.inMultiplayer() )
    {
        multiplayerManager.removePlayer( this );
    }

}

static qboolean logfile_started = qfalse;

void Player::Init(void)
{
    _cameraCutThisFrame = false; 

    _onLadder         = false;
    _ladderNormal     = vec_zero;

    TeamMateList.AddObject(this);
    _ladderTop        = 0.0f;
    _nextLadderTime   = 0.0f;
    dont_turn_legs    = false;

    // over these resets -- which is exactly retail's ordering.
    _totalGameFrames  = 0;
    _updateGameFrames = true;
    _secretsFound     = 0;
    setSkill(skill->integer);

    head_target                  = NULL;
    look_at_time                 = 0.0f;
    targetEnemy                  = NULL;
    targetEnemyLocked            = false;
    shield_active                = qfalse;

    dual_wield_active            = qfalse;
    dual_wield_weaponlist.ClearObjectList();
    for (int hw = 0; hw < MAX_ACTIVE_WEAPONS; hw++) {
        holsteredWeapons[hw] = NULL;
    }

    currentCallVolume            = "";
    _userFovChoice               = fov;
    vehicle                      = NULL;

    bonus_alpha                  = 0.0f;
    old_waterlevel               = 0;
    _flashAlpha                  = 0.0f;
    _flashBlend                  = vec_zero;
    _flashMaxTime                = 0.0f;
    _flashMinTime                = 0.0f;

    cool_camera                  = NULL;
    entity_to_watch              = NULL;
    maximumAngleToWatchedEntity  = 0.0f;
    watchEntityForEntireDuration = false;
    playerCameraMode             = PLAYER_CAMERA_MODE_NORMAL;

    _allowMusicDucking           = true;
    _allowActionMusic            = true;
    accumulated_pain             = 0.0f;

    base_rightfoot_pos           = vec_zero;
    base_leftfoot_pos            = vec_zero;
    rightfoot_pos                = vec_zero;
    leftfoot_pos                 = vec_zero;
    pm_lastruntime               = 0;

    cool_item                    = NULL;
    cool_dialog                  = "";
    cool_anim                    = "";
    do_rise                      = qfalse;
    projdetonate                 = qfalse;

    yawing                       = qfalse;
    yawing_left                  = qfalse;
    yawing_right                 = qfalse;
    adjust_torso                 = qfalse;
    torsoAngles                  = vec_zero;
    headAngles                   = vec_zero;
    headAimAngles                = vec_zero;
    torsoAimAngles               = vec_zero;

    fakePlayer_active            = qfalse;
    fakePlayer                   = NULL;

    specialMoveChargeTime        = 3.0f;
    specialMoveCharge            = 0.0f;
    specialMoveEndTime           = 0.0f;
    playerKnockback              = 0.0f;
    knockbackMultiplier          = 1.0f;
    changedStanceTorso           = false;
    changedStanceLegs            = false;
    incomingMeleeAttack          = false;
    stanceNumber                 = 1;
    points                       = 0;
    bendTorsoMult                = standardTorsoMult;
    lastActionType               = "";
    meleeAttackFlags             = 0;
    changingChar                 = false;

    meleeAttackerList.ClearObjectList();
    finishableList.ClearObjectList();
    finishingMoveList.ClearObjectList();

    _powerup                     = NULL;
    _rune                        = NULL;
    _holdableItem                = NULL;

    _nextEnergyTransferTime      = 0.0f;
    _canTransferEnergy           = false;
    _doDamageScreenFlash         = false;
    _isThirdPerson               = true;  
    _finishActor                 = 0;
    _finishState                 = "";
    _doingFinishingMove          = false;

    _usingEntity                 = false;
    _attackType                  = "";
    _gameplayAnimIdx             = 1;
    _disableUseWeapon            = false;
    _targetSelectedHighlight     = NULL;
    _targetLockedHighlight       = NULL;
    _infoHudOn                   = false;
    _nextRegenTime               = 0;
    _useEntityStartTimer         = 0.0f;

    _objectiveNameIndex          = 0;
    _objectiveStates             = 0;
    _informationStates           = 0;

    _dialogEntnum                = ENTITYNUM_NONE;
    _dialogSoundIndex            = -1;
    _dialogTextSoundIndex        = -1;

    _crossHairXOffset            = 0.0f;
    _crossHairYOffset            = 0.0f;
    _lastDamagedTimeFront        = 0.0f;
    _lastDamagedTimeBack         = 0.0f;
    _lastDamagedTimeLeft         = 0.0f;
    _lastDamagedTimeRight        = 0.0f;

    _nextPainShaderTime          = 0.0f;
    _lastPainShaderMod           = MOD_NONE;

    _voteText                    = "";
    _validPlayerModel            = false;

    _hudList.ClearObjectList();
    _needToSendHuds              = false;
    _started                     = false;

    _forcedMoveType              = PM_NONE;
    _branchDialogActor           = NULL;
    _needToSendBranchDialog      = false;

    InitClient();
    InitPhysics();
    InitWorldEffects();
    InitSound();
    InitView();
    InitState();
    InitEdict();
    InitWeapons();
    InitInventory();
    InitHealth();
    InitStats();
    InitModel();

    _dialogEntnum     = ENTITYNUM_NONE;
    _dialogSoundIndex = -1;
    _dialogTextSoundIndex = -1;

    client->ps.objectiveNameIndex = -1;

    edict->s.clientNum = client->ps.clientNum;

    LoadStateTable();

    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.addPlayer(this);
    } else if (!LoadingSavegame) {
        ChooseSpawnPoint();
    }

    _nextEnergyTransferTime = 0.0f;
    _nextPainShaderTime     = 0.0f;

    edict->s.renderfx &= ~RF_FORCE_ALPHA;

    setAlpha(1.0f);

    link();
    logfile_started = qfalse;

}


void Player::InitEdict(void)
{
    setSolidType(SOLID_BBOX);
    setMoveType(MOVETYPE_WALK);

    setSize(Vector(MINS_X, MINS_Y, 0.0f), Vector(MAXS_X, MAXS_Y, MAXS_Z));

    edict->clipmask = MASK_PLAYERSOLID;
    edict->svflags &= ~SVF_DEADMONSTER;
    edict->svflags &= ~SVF_HIDEOWNER;
    edict->ownerNum = ENTITYNUM_NONE;

    edict->s.eFlags   = 0;
    edict->s.frame = 0;

    edict->s.renderfx |= RF_SHADOW_PRECISE | RF_SHADOW;
}

void Player::InitSound(void)
{
    client->ps.current_music_mood  = mood_normal;
    client->ps.fallback_music_mood = mood_normal;
    ChangeMusic("normal", "normal", false);

    client->ps.music_volume           = 1.0;
    client->ps.music_volume_fade_time = 0.0;

    _allowMusicDucking                = true;
    client->ps.allowMusicDucking      = true;

    ChangeMusicVolume(1.0, 0.0);

    music_forced = false;


    client->ps.reverb_type  = eax_generic;
    client->ps.reverb_level = 0;
    SetReverb(client->ps.reverb_type, client->ps.reverb_level);
}

void Player::InitClient(void)
{
    client_persistant_t saved;

    if (g_gametype->integer != GT_SINGLE_PLAYER) {
        char       userinfo[MAX_INFO_STRING];
        char       dm_primary[MAX_QPATH];
        float      enterTime   = client->pers.enterTime;
        teamtype_t team        = client->pers.teamnum;
        int        round_kills = client->pers.round_kills;

        memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
        memcpy(dm_primary, client->pers.dm_primary, sizeof(dm_primary));
        G_InitClientPersistant(client);
        G_ClientUserinfoChanged(edict, userinfo);

        memcpy(client->pers.dm_primary, dm_primary, sizeof(client->pers.dm_primary));
        client->pers.enterTime   = enterTime;
        client->pers.teamnum     = team;
        client->pers.round_kills = round_kills;
    }

    saved = client->pers;

    memset(client, 0, sizeof(*client));
    client->pers = saved;

    client->ps.clientNum   = client - game.clients;
    client->lastActiveTime = level.inttime;

    client->ps.viewheight           = (int)sv_defaultviewheight->value;
    client->ps.pm_defaultviewheight = (int)sv_defaultviewheight->value;
}

void Player::InitState(void)
{
    // flags retail does not set on this path.
    gibbed           = false;
    pain             = 0;
    accumulated_pain = 0;
    nextpaintime     = 0.0f;
    knockdown        = false;
    pain_dir         = PAIN_NONE;
    pain_type        = MOD_NONE;
    takedamage       = DAMAGE_AIM;
    deadflag         = DEAD_NO;
    flags           &= ~FL_NO_KNOCKBACK;
    flags           |= (FL_BLOOD | FL_DIE_GIBS);

    if (!com_blood->integer) {
        flags &= ~FL_BLOOD;
        flags &= ~FL_DIE_GIBS;
    }
}

void Player::InitHealth(void)
{
    if (LoadingSavegame) {
        return;
    }

    health     = 100;
    max_health = 100;


    CancelEventsOfType(EV_Player_Dead);
}

void Player::InitModel(const char *modelName)
{
    orientation_t orient;
    int anim;
    int tagnum;
    int i;


    for ( i = 0 ; i < MAX_MODEL_SURFACES ; i++ )
    {
        edict->s.surfaces[ i ] &= ~MDL_SURFACE_NODRAW;
    }

    if ( modelName )
        setModel( modelName );
    else
        setModel( str( g_playermodel->string ) + ".tik" );

    SetControllerTag( HEAD_TAG,   gi.Tag_NumForName( edict->s.modelindex, "Bip01 Head" ) );
    SetControllerTag( TORSO_TAG,  gi.Tag_NumForName( edict->s.modelindex, "Bip01 Spine1" ) );
    SetControllerTag( R_ARM_TAG,  gi.Tag_NumForName( edict->s.modelindex, "Bip01 R UpperArm" ) );
    SetControllerTag( L_ARM_TAG,  gi.Tag_NumForName( edict->s.modelindex, "Bip01 L UpperArm" ) );
    SetControllerTag( MOUTH_TAG,  gi.Tag_NumForName( edict->s.modelindex, "tag_mouth" ) );

    anim = gi.Anim_NumForName( edict->s.modelindex, "stand_idle" );

    tagnum = gi.Tag_NumForName( edict->s.modelindex, "Bip01 R Foot" ) & TAG_MASK;
    orient = gi.Tag_Orientation( edict->s.modelindex, anim, 0, tagnum, 1.0f, NULL, NULL );
    base_rightfoot_pos = orient.origin;
    base_rightfoot_pos.z = 0;

    tagnum = gi.Tag_NumForName( edict->s.modelindex, "Bip01 L Foot" ) & TAG_MASK;
    orient = gi.Tag_Orientation( edict->s.modelindex, anim, 0, tagnum, 1.0f, NULL, NULL );
    base_leftfoot_pos = orient.origin;
    base_leftfoot_pos.z = 0;

    showModel();

    yawing_left = false;
    yawing_right = false;
}

void Player::InitPhysics(void)
{
    oldvelocity  = vec_zero;
    velocity     = vec_zero;
    old_v_angle  = v_angle;
    gravity      = 1.0;
    falling      = false;
    hardimpact = false;
    mass = 200;
    setContents(CONTENTS_BODY);
    memset(&last_ucmd, 0, sizeof(last_ucmd));

    _forcedMoveType = PM_NONE;

    client->ps.groundTrace.fraction = 1.0f;

    flags &= ~FL_PARTIAL_IMMOBILE;
}


void Player::InitWorldEffects(void)
{
    next_drown_time     = 0;
    next_painsound_time = 0;
    air_finished        = level.time + 20.0f;
    old_waterlevel      = 0;
    drown_damage        = 0.0f;
}


void Player::InitWeapons(void)
{
    if (LoadingSavegame) {
        return;
    }

    ClearNewActiveWeapon();
}


void Player::InitView(void)
{
    camera  = NULL;
    v_angle = vec_zero;
    SetViewAngles(v_angle);
    viewheight = STAND_EYE_HEIGHT;

    head_target       = NULL;
    targetEnemy       = NULL;
    targetEnemyLocked = false;
    _targetedEntity   = NULL;

    damage_blend = vec_zero;
}

void Player::ChooseSpawnPoint(void)
{
    str thread;
    SelectSpawnPoint(origin, angles, thread);
    setOrigin(origin + Vector(0, 0, 1));
    origin.copyTo(edict->s.origin2);
    edict->s.renderfx |= RF_FRAMELERP;

    KillBox(this);

    setAngles(angles);
    SetViewAngles(angles);

    if (thread.length()) {
        ExecuteThread(thread);
    }
}




// "Could not pick up ..." line in the game -- twelve retail call sites feed it and it is
void Player::setItemText(int itemIcon, const str& itemText)
{
    CancelEventsOfType(EV_Player_ClearItemText);

    _itemIcon = itemIcon;
    _itemText = itemText;

    PostEvent(EV_Player_ClearItemText, 2.0f);
}

void Player::clearItemText(void)
{
    _itemIcon = 0;
    _itemText = "";

    CancelEventsOfType(EV_Player_ClearItemText);
}

void Player::clearItemText(Event *ev)
{
    clearItemText();
}





// The HUD is NOT pushed here.  Retail's CG_DrawActiveFrame (@0x00044FE0) reads
void Player::setViewMode(const str& viewModeName)
{
    Sentient::setViewMode(viewModeName);

    client->ps.viewMode = getViewMode();
}

qboolean Player::ShouldSendToClient(Entity *entityToSend)
{
    if (multiplayerManager.inMultiplayer() && client->pers.mp_lowBandwidth
        && entityToSend->isNetworkDetail())
        return qfalse;

    if (!entityToSend->_affectingViewModes)
        return qtrue;

    if (entityToSend->_affectingViewModes & _viewMode)
        return gi.GetViewModeSendInMode(entityToSend->_affectingViewModes & _viewMode);
    else
        return gi.GetViewModeSendNotInMode(entityToSend->_affectingViewModes & (~_viewMode));
}


void Player::LoadObjectives(Event *ev)
{
    loadObjectives(ev->GetString(1));
}


#define OBJECTIVE_MAX_STATES 8
#define OBJECTIVE_SHOW_BIT(n)     (1u << (3 * ((n) - 1) + 0))
#define OBJECTIVE_COMPLETE_BIT(n) (1u << (3 * ((n) - 1) + 1))
#define OBJECTIVE_FAILED_BIT(n)   (1u << (3 * ((n) - 1) + 2))

void Player::SetObjectiveShow(Event *ev)
{
    str      ObjName;
    qboolean Show;
    bool     playSound = false;
    int      ObjIndex;

    ObjName = ev->GetString(1);
    Show    = ev->GetBoolean(2);

    ObjIndex = gi.MObjective_GetIndexFromName(ObjName.c_str());
    if (ObjIndex == 0) {
        return;
    }

    gi.MObjective_SetShowObjective(ObjName.c_str(), Show);

    if (Show) {
        unsigned int bitToChange = (ObjIndex <= OBJECTIVE_MAX_STATES) ? OBJECTIVE_SHOW_BIT(ObjIndex) : 0u;

        if (!(_objectiveStates & bitToChange)) {
            _objectiveStates |= bitToChange;
            playSound = true;
        }
        if (playSound) {
            Sound("snd_objectivechanged", CHAN_LOCAL);
        }
    }
}



void Player::SetObjectiveComplete(Event *ev)
{
    str      ObjName;
    qboolean Complete;
    int      ObjIndex;

    ObjName  = ev->GetString(1);
    Complete = ev->GetBoolean(2);

    ObjIndex = gi.MObjective_GetIndexFromName(ObjName.c_str());
    if (ObjIndex == 0) {
        return;
    }

    if (ObjIndex <= OBJECTIVE_MAX_STATES) {
        if (Complete) {
            _objectiveStates |= OBJECTIVE_COMPLETE_BIT(ObjIndex);
        } else {
            _objectiveStates &= ~OBJECTIVE_COMPLETE_BIT(ObjIndex);
        }
    }
}

void Player::SetObjectiveFailed(Event *ev)
{
    str      ObjName;
    qboolean Failed;
    int      ObjIndex;

    ObjName = ev->GetString(1);
    Failed  = ev->GetBoolean(2);

    ObjIndex = gi.MObjective_GetIndexFromName(ObjName.c_str());
    if (ObjIndex == 0) {
        return;
    }

    if (ObjIndex <= OBJECTIVE_MAX_STATES) {
        if (Failed) {
            _objectiveStates |= OBJECTIVE_FAILED_BIT(ObjIndex);
        } else {
            _objectiveStates &= ~OBJECTIVE_FAILED_BIT(ObjIndex);
        }
    }
    if (ObjIndex == 7) {
        if (Failed) {
            _objectiveStates |= OBJECTIVE_FAILED_BIT(8);
        } else {
            _objectiveStates &= ~OBJECTIVE_FAILED_BIT(8);
        }
    }
}

void Player::SetInformationShow(Event *ev)
{
    str      InfoName;
    qboolean Show;
    int      InfoIndex;

    InfoName = ev->GetString(1);
    Show     = ev->GetBoolean(2);

    InfoIndex = gi.MI_GetIndexFromName(InfoName.c_str());
    if (InfoIndex == 0) {
        return;
    }

    switch (InfoIndex)
    {
    case INFORMATION1:
        if (Show) _informationStates |= INFORMATION1_SHOW;
        else      _informationStates &= ~(INFORMATION1_SHOW);
        break;

    case INFORMATION2:
        if (Show) _informationStates |= INFORMATION2_SHOW;
        else      _informationStates &= ~(INFORMATION2_SHOW);
        break;

    case INFORMATION3:
        if (Show) _informationStates |= INFORMATION3_SHOW;
        else      _informationStates &= ~(INFORMATION3_SHOW);
        break;

    case INFORMATION4:
        if (Show) _informationStates |= INFORMATION4_SHOW;
        else      _informationStates &= ~(INFORMATION4_SHOW);
        break;

    case INFORMATION5:
        if (Show) _informationStates |= INFORMATION5_SHOW;
        else      _informationStates &= ~(INFORMATION5_SHOW);
        break;

    case INFORMATION6:
        if (Show) _informationStates |= INFORMATION6_SHOW;
        else      _informationStates &= ~(INFORMATION6_SHOW);
        break;

    case INFORMATION7:
        // source, and retail's binary really does fall through.  Do not "fix".
        if (Show) _informationStates |= INFORMATION7_SHOW;
        else      _informationStates &= ~(INFORMATION7_SHOW);

    case INFORMATION8:
        if (Show) _informationStates |= INFORMATION8_SHOW;
        else      _informationStates &= ~(INFORMATION8_SHOW);
        break;

    default:
        break;
    }
}

void Player::EndLevel(Event *ev)
{
    setViewMode("none");

    if (health > max_health) {
        health = max_health;
    }

    if (health < 1) {
        health = 1;
    }

    _updateGameFrames = false;
}

void Player::Respawn(Event *ev)
{
    if (multiplayerManager.inMultiplayer()) {
        respawn_time = level.time;

        Init();

        client->ps.pm_time = 50;
        client->ps.pm_flags |= PMF_TIME_TELEPORT;

        return;
    } else {
        logfile_started = qfalse;
    }
}

void Player::SetDeltaAngles(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        client->ps.delta_angles[i] = ANGLE2SHORT(v_angle[i]);
    }
}


void Player::Dead(Event *ev)
{
    CancelEventsOfType(EV_Player_Dead);

    animate->StopAnimatingAtEnd();

    if (edict->s.torso_anim & ANIM_BLEND) {
        animate->StopAnimatingAtEnd(torso);
    }

    if (((pain_type == MOD_VAPORIZE) || (pain_type == MOD_VAPORIZE_COMP)
         || (pain_type == MOD_VAPORIZE_DISRUPTOR) || (pain_type == MOD_VAPORIZE_PHOTON)
         || (pain_type == MOD_SNIPER))
        && (edict->s.alpha > 0.0f) && (level.time < respawn_time)) {
        PostEvent(EV_Player_Dead, FRAMETIME);
        return;
    }

    deadflag = DEAD_DEAD;

    if (p_heuristics) {
        p_heuristics->IncrementNumberOfDeaths();
    }

    ++client->ps.stats[STAT_DEATHS];

    if (p_heuristics) {
        p_heuristics->SaveHeuristics(this);
    }

    if (multiplayerManager.inMultiplayer()) {
        TIKIANIM_PROBE("dead:mp", "ent=%d -> playerDead (DeadBody+hideModel), STAT_DEATHS=%d",
                   entnum, client->ps.stats[STAT_DEATHS]);
        multiplayerManager.playerDead(this);
    } else {
        TIKIANIM_PROBE("dead:sp", "ent=%d -> G_MissionFailed (SINGLE-PLAYER arm taken)", entnum);
        int i            = (rand() % 10) + 1;
        str playerKilled = "PlayerKilled";
        playerKilled += i;
        G_MissionFailed(playerKilled);
    }


    _updateGameFrames = false;
}


void Player::Killed(Event *ev)
{
    Entity   *attacker;
    Entity   *inflictor;
    int      meansofdeath;
    Vector   direction;

    attacker     = ev->GetEntity(1);
    inflictor    = ev->GetEntity(3);
    meansofdeath = ev->GetInteger(4);
    direction    = ev->GetVector(7);

    pain_type   = (meansOfDeath_t)meansofdeath;

    damage_from = direction;

    if (multiplayerManager.inMultiplayer()) {
        if (attacker && attacker->isSubclassOf(Player)) {
            multiplayerManager.playerKilled(this, (Player *)attacker, inflictor, meansofdeath);
        } else {
            multiplayerManager.playerKilled(this, NULL, inflictor, meansofdeath);
        }
    }

    SpawnDamageEffect((meansOfDeath_t)meansofdeath);

    animate->ClearTorsoAnim();
    animate->ClearLegsAnim();



    deadflag = DEAD_DYING;

    respawn_time = level.time + 1.0f;

    TIKIANIM_PROBE("die", "ent=%d mp=%d attackerIsPlayer=%d attackerEnt=%d mod=%d respawn_time=%.2f deadflag=DYING",
               entnum, multiplayerManager.inMultiplayer() ? 1 : 0,
               (attacker && attacker->isSubclassOf(Player)) ? 1 : 0,
               attacker ? attacker->entnum : -1,
               meansofdeath, (float)respawn_time);

    health   = 0;

    if (_targetedEntity != NULL) {
        _targetedEntity->edict->s.eFlags &= ~EF_DISPLAY_INFO;
        _targetedEntity = NULL;
    }



    if (g_gametype->integer != GT_SINGLE_PLAYER) {
        if (HasItem("Binoculars")) {
            takeItem("Binoculars");
        }
    }

    if (multiplayerManager.inMultiplayer() && !multiplayerManager.checkFlag(MP_FLAG_NO_DROP_WEAPONS) &&
        multiplayerManager.checkRule("dropWeapons", true)) {
        int i;

        for (i = 0; i < MAX_ACTIVE_WEAPONS; i++) {
            Weapon *weap = activeWeaponList[i];

            if (weap) {
                weap->Drop();
                activeWeaponList[i] = NULL;
            }
        }
    }

    ClearNewActiveWeapon();

    FreeInventory();

    dropRune();
    dropPowerup();
    removeHoldableItem();

    edict->clipmask  = MASK_DEADSOLID;
    edict->svflags |= SVF_DEADMONSTER;  
    setContents(CONTENTS_CORPSE);
    setMoveType(MOVETYPE_NONE);

    angles.x = 0;
    angles.z = 0;
    setAngles(angles);

    ChangeMusic("failure", "normal", true);

    takedamage = DAMAGE_NO;

    PostEvent(EV_Player_Dead, 5.0f);

    Sound("player_death");

    _updateGameFrames = false;

}


void Player::MissionFailed(Event *ev)
{
    str reason = "DefaultFailure";

    if (ev->NumArgs() > 0) {
        reason = ev->GetString(1);
    }

    G_MissionFailed(reason);
}

void Player::setMissionFailed(void)
{
    client->ps.missionStatus |= MISSION_FAILED;
}


void Player::Pain( Event *ev )
{
	float		damage,yawdiff;
	[[maybe_unused]] Entity	*attacker;
	int      meansofdeath;
	Vector   dir,pos,attack_angle;
	
	damage	      = ev->GetFloat( 1 );
	attacker       = ev->GetEntity( 2 );
	meansofdeath   = ev->GetInteger( 3 );
	pos            = ev->GetVector( 4 );
	dir            = ev->GetVector( 5 );
	
	if ( !damage && !knockdown )
		return;
	
	client->ps.stats[ STAT_LAST_PAIN ] = (int) damage;
	
	attack_angle = dir.toAngles();
	yawdiff = torsoAngles[YAW] - attack_angle[YAW] + 180.0f;
	yawdiff = AngleNormalize180( yawdiff );
	
	if ( ( yawdiff > -45.0f ) && ( yawdiff < 45.0f ) )
		pain_dir = PAIN_FRONT;
	else if ( ( yawdiff < -45.0f ) && ( yawdiff > -135.0f ) )
		pain_dir = PAIN_LEFT;
	else if ( ( yawdiff > 45.0f ) && ( yawdiff < 135.0f ) )
		pain_dir = PAIN_RIGHT;
	else
		pain_dir = PAIN_REAR;
	
	if ( take_pain )
	{
		accumulated_pain   += damage;
	}
	
	SpawnDamageEffect( (meansOfDeath_t)meansofdeath );
	
	pain_type          = (meansOfDeath_t)meansofdeath;
	
	if ( ( level.time > nextpaintime ) && take_pain )
	{
		pain = damage;
	}

	if ( ( level.time > next_painsound_time ) && ( health > 0.0f ) )
	{
		if ( G_GetDatabaseFloat( "MOD", MOD_NumToName( meansofdeath ), "DoPainSound" ) )
		{
			next_painsound_time = level.time + 0.25f + G_Random( 0.25f );
			Sound( "snd_pain", CHAN_VOICE, DEFAULT_VOL, 300.0f );
		}
	}
	

	damage_blood += damage;

	if ( deadflag == DEAD_NO )
	{
		if ( meansofdeath == MOD_DEATH_QUAD )
			damage_from = vec_zero;
		else
			damage_from = ev->GetVector( 5 ) * damage;
	}


	if ( meansofdeath == MOD_FIRE || meansofdeath == MOD_ON_FIRE )
		damage_blend = damageFireColor;
	else
		damage_blend = damageNormalColor;


	if ( damage_from == vec_zero )
	{
		_lastDamagedTimeFront = level.time;
		_lastDamagedTimeLeft  = level.time;
		_lastDamagedTimeRight = level.time;
		_lastDamagedTimeBack  = level.time;
	}
	else
	{
		Vector damageAngles;
		float yaw;

		damageAngles = damage_from * -1.0f;
		damageAngles = damageAngles.toAngles();

		yaw = AngleNormalize180( AngleNormalize180( damageAngles[ YAW ] ) - AngleNormalize180( v_angle[ YAW ] ) );

		if ( ( yaw >= -45.0f ) && ( yaw <= 45.0f ) )
			_lastDamagedTimeFront = level.time;
		else if ( ( yaw > 45.0f ) && ( yaw < 135.0f ) )
			_lastDamagedTimeLeft = level.time;
		else if ( ( yaw < -45.0f ) && ( yaw > -135.0f ) )
			_lastDamagedTimeRight = level.time;
		else
			_lastDamagedTimeBack = level.time;
	}
}

void Player::SpawnDamageEffect(meansOfDeath_t mod)
{
}

void Player::DoUse(Event *ev)
{
    int        i;
    int        num;
    int        touch[MAX_GENTITIES];
    gentity_t *hit;
    Event     *event;
    Vector     min;
    Vector     max;
    Vector     offset;
    trace_t    trace;
    Vector     start;
    Vector     end;
    float      t;
    Entity    *usingEntity;
    bool       entityActuallyUsed;
    Entity    *bestActor;

    if (multiplayerManager.inMultiplayer() && multiplayerManager.isPlayerSpectator(this)) {
        return;
    }

    if (ev->NumArgs() > 0) {
        usingEntity = ev->GetEntity(1);
    } else {
        usingEntity = this;
    }

    if (vehicle) {
        event = new Event(EV_Use);
        event->AddEntity(this);
        vehicle->ProcessEvent(event);
        return;
    }

    start    = origin;
    start.z += client->ps.viewheight;
    end      = start + (yaw_forward * 64.0f);

    trace = G_Trace(start, vec_zero, vec_zero, end, this, MASK_USABLE, true, "Player::DoUse");

    t = 64.0f * trace.fraction - maxs[0];
    if (t < 0.0f) {
        t = 0.0f;
    }

    offset = yaw_forward * t;

    min = start + offset + Vector("-31 -31 -31");
    max = start + offset + Vector("31 31 31");

    num = gi.AreaEntities(min, max, touch, MAX_GENTITIES, qfalse);

    entityActuallyUsed = false;

    bestActor = getBestActorToUse(touch, num);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (!hit->inuse) {
            continue;
        }

        if (usingEntity == hit->entity) {
            continue;
        }

        if (hit->entity->isSubclassOf(Actor) && (hit->entity != bestActor)) {
            continue;
        }

        assert(hit->entity);

        event = new Event(EV_Use);
        event->AddEntity(usingEntity);
        hit->entity->ProcessEvent(event);

        if (hit->entity->isSubclassOf(Weapon)) {
            continue;
        }

        if (hit->entity->isSubclassOf(Projectile)) {
            continue;
        }

        if (hit->entity->getSolidType() == SOLID_NOT) {
            continue;
        }

        entityActuallyUsed = true;
    }

    if (!entityActuallyUsed) {
        useHoldableItem();
    }
}

Actor *Player::getBestActorToUse(int *entityList, int count)
{
    int       i;
    float     bestYawDiff = 1000.0f;
    float     yawDiff;
    Actor    *bestActor = NULL;
    Actor    *currentActor;
    gentity_t *gent;
    Vector    dir;
    Vector    ang;

    for (i = 0; i < count; i++) {
        gent = &g_entities[entityList[i]];

        if (!gent->inuse || !gent->entity || !gent->entity->isSubclassOf(Actor)) {
            continue;
        }

        currentActor = (Actor *)gent->entity;

        dir    = currentActor->centroid - centroid;
        dir.normalize();
        ang = dir.toAngles();

        yawDiff = AngleNormalize180(AngleNormalize180(v_angle[YAW]) - AngleNormalize180(ang[YAW]));
        yawDiff = abs((int)yawDiff);

        if (yawDiff < bestYawDiff) {
            bestYawDiff = yawDiff;
            bestActor   = currentActor;
        }
    }

    return bestActor;
}

void Player::useHoldableItem(void)
{
    if (_holdableItem) {
        if (_holdableItem->use()) {
            if (_holdableItem) {
                setItemText(_holdableItem->getIcon(), va("$$Used$$ $$Item-%s$$", _holdableItem->getName().c_str()));
                removeHoldableItem();
            }
        }
    }
}

void Player::removeHoldableItem(void)
{
    if (_holdableItem) {
        _holdableItem->PostEvent(EV_Remove, 0.0f);

        _holdableItem = NULL;
    }
}


void Player::TouchStuff(pmove_t *pmove)
{
    gentity_t *other;
    Event     *event;
    int        i;
    int        j;

    toucheduseanim = NULL;

    if (GetMovePlayerMoveType() != PM_NOCLIP) {

        if (multiplayerManager.inMultiplayer() && multiplayerManager.isPlayerSpectator(this)) {
            G_TouchTeleporters(this);
        } else {
            G_TouchTriggers(this);
        }
    }

    for (i = 0; i < pmove->numtouch; i++) {
        other = &g_entities[pmove->touchents[i]];

        for (j = 0; j < i; j++) {
            gentity_t *ge = &g_entities[j];

            if (ge == other) {
                break;
            }
        }

        if (j != i) {
            continue;
        }

        if ((!other->entity) || (other->entity == world)) {
            continue;
        }

        event = new Event(EV_Touch);
        event->AddEntity(this);
        other->entity->ProcessEvent(event);

        event = new Event(EV_Touch);
        event->AddEntity(other->entity);
        ProcessEvent(event);
    }
}

// Kept deliberately, against retail:
void Player::GetMoveInfo(pmove_t *pmove)
{
    moveresult = pmove->moveresult;

    if (!deadflag || (multiplayerManager.inMultiplayer() && multiplayerManager.isPlayerSpectator(this))) {
        v_angle[0] = pmove->ps->viewangles[0];
        v_angle[1] = pmove->ps->viewangles[1];
        v_angle[2] = pmove->ps->viewangles[2];

        if (moveresult == MOVERESULT_TURNED) {
            angles.y = v_angle[1];
            setAngles(angles);
            SetViewAngles(angles);
        }
    }

    setOrigin(Vector(pmove->ps->origin[0], pmove->ps->origin[1], pmove->ps->origin[2]));
    velocity = Vector(pmove->ps->velocity[0], pmove->ps->velocity[1], pmove->ps->velocity[2]);

    if ((client->ps.pm_flags & PMF_FROZEN) || (client->ps.pm_flags & PMF_NO_MOVE)) {
        velocity = vec_zero;
    } else {
        setSize(pmove->mins, pmove->maxs);
    }

    waterlevel = pmove->waterlevel;
    watertype  = pmove->watertype;

    groundentity = NULL;
    if (pmove->ps->groundEntityNum != ENTITYNUM_NONE) {
        groundentity = &g_entities[pmove->ps->groundEntityNum];
        airspeed     = 350;  
    }

    // WAS_RUNNING conditional stays true. That latch is retail's.
    if (pmove->ps->pm_runtime) {
        pm_lastruntime = pmove->ps->pm_runtime;
    }
}

// ⚠️ DELIBERATE DIVERGENCE (1): retail dereferences G_GetEntity()'s result with no NULL
// ⚠️ DELIBERATE DIVERGENCE (2): retail posts EV_Actor_Push ("push", EV_CODEONLY, "v",
qboolean TryPush(int entnum, vec3_t move_origin, vec3_t move_end)
{
    Actor  *act;
    Vector  dir;
    Vector  dir2;
    Entity *ent;

    if (entnum == ENTITYNUM_NONE) {
        return qfalse;
    }

    ent = G_GetEntity(entnum);

    if (ent && ent->isSubclassOf(Actor) && ((Actor *)ent)->movementSubsystem) {
        act = (Actor *)ent;

        dir   = act->origin - move_origin;
        dir.z = 0.0f;
        dir.normalize();

        dir2 = move_end;
        dir2 -= move_origin;

        if (act->flags & FL_FLY) {
            dir *= dir2.length() / 2.0f;

            if (act->movementSubsystem->Push(dir)) {
                return qtrue;
            }
        } else {
            dir *= dir2.length();

            act->movementSubsystem->Push(dir);
        }
    }

    return qfalse;
}


void Player::SetMoveInfo( pmove_t *pmove, const usercmd_t *ucmd )
{
	static int lastEasyJump = -1;
	Vector move;
	
	memset( pmove, 0, sizeof( pmove_t ) );

	velocity.copyTo( client->ps.velocity );
	
	pmove->ps             = &client->ps;
	
	if ( ucmd )
	{
		pmove->cmd            = *ucmd;
	}
	
	pmove->easyJump       = cl_easyJump->integer ? qtrue : qfalse;

	if ( PROBE_On() && lastEasyJump != pmove->easyJump )
	{
		PROBE( "easyjump", "srv state easy=%d was=%d\n", (int)pmove->easyJump, lastEasyJump );
		lastEasyJump = pmove->easyJump;
	}

	pmove->tracemask      = edict->clipmask;
pmove->trace          = gi.trace;
	pmove->pointcontents  = gi.pointcontents;
	pmove->trypush        = TryPush;
	
	pmove->ps->origin[ 0 ] = origin.x;
	pmove->ps->origin[ 1 ] = origin.y;
	pmove->ps->origin[ 2 ] = origin.z;
	
	pmove->ps->velocity[ 0 ] = velocity.x;
	pmove->ps->velocity[ 1 ] = velocity.y;
	pmove->ps->velocity[ 2 ] = velocity.z;
	
	if ( pmove->ps->pm_runtime )
		pm_lastruntime = pmove->ps->pm_runtime;
}

pmtype_t Player::GetMovePlayerMoveType(void)
{
    if (multiplayerManager.isPlayerSpectator(this, SPECTATOR_TYPE_FOLLOW)) {
        return PM_SPECTATOR_FOLLOW;
    } else if (multiplayerManager.isPlayerSpectator(this, SPECTATOR_TYPE_FREEFORM)) {
        return PM_NOCLIP;
    } else if (multiplayerManager.isPlayerSpectator(this)) {
        return PM_SPECTATOR;
    } else if (getMoveType() == MOVETYPE_NOCLIP) {
        return PM_NOCLIP;
    } else if (deadflag) {
        return PM_DEAD;
    } else if (_forcedMoveType != PM_NONE) {
        // command path (retail CL_CreateCmd @0x16EF20) could never be selected.
        return _forcedMoveType;
    } else {
        return PM_NORMAL;
    }
}

void Player::CheckGround(void)
{
    pmove_t pmove;

    SetMoveInfo(&pmove, current_ucmd);
    Pmove_GroundTrace(&pmove);
    GetMoveInfo(&pmove);
}

qboolean Player::AnimMove(Vector& move, Vector *endpos)
{
    Vector  up;
    Vector  down;
    trace_t trace;
    int     mask;
    Vector  start(origin);
    Vector  end(origin + move);

    mask = MASK_PLAYERSOLID;

    trace = G_Trace(start, mins, maxs, end, this, mask, true, "AnimMove");
    if (trace.fraction < 1) {
        if ((movecontrol == MOVECONTROL_HANGING) || (movecontrol == MOVECONTROL_CLIMBWALL)) {
            up = origin;
            up.z += move.z;
            trace = G_Trace(origin, mins, maxs, up, this, mask, true, "AnimMove");
            if (trace.fraction < 1) {
                if (endpos) {
                    *endpos = origin;
                }
                return qfalse;
            }

            origin = trace.endpos;
            end    = origin;
            end.x += move.x;
            end.y += move.y;

            trace = G_Trace(origin, mins, maxs, end, this, mask, true, "AnimMove");
            if (endpos) {
                *endpos = trace.endpos;
            }

            return (trace.fraction > 0);
        } else {
            return TestMove(move, endpos);
        }
    } else {
        if (endpos) {
            *endpos = trace.endpos;
        }

        return qtrue;
    }
}

qboolean Player::TestMove(Vector& move, Vector *endpos)
{
    trace_t trace;
    Vector  pos(origin + move);

    trace = G_Trace(origin, mins, maxs, pos, this, MASK_PLAYERSOLID, true, "TestMove");
    if (trace.allsolid) {
        if (endpos) {
            *endpos = origin;
        }
        return qfalse;
    }

    if (trace.fraction < 1.0f) {
        Vector up(origin);
        up.z += STEPSIZE;

        trace = G_Trace(origin, mins, maxs, up, this, MASK_PLAYERSOLID, true, "TestMove");
        if (trace.fraction == 0.0f) {
            if (endpos) {
                *endpos = origin;
            }
            return qfalse;
        }

        Vector temp(trace.endpos);
        Vector end(temp + move);

        trace = G_Trace(temp, mins, maxs, end, this, MASK_PLAYERSOLID, true, "TestMove");
        if (trace.fraction == 0.0f) {
            if (endpos) {
                *endpos = origin;
            }
            return qfalse;
        }

        temp = trace.endpos;

        Vector down(trace.endpos);
        down.z = origin.z;

        trace = G_Trace(temp, mins, maxs, down, this, MASK_PLAYERSOLID, true, "TestMove");
    }

    if (endpos) {
        *endpos = trace.endpos;
    }

    return qtrue;
}


static Vector vec_up = Vector(0, 0, 1);

void Player::showObjectInfo(void)
{
    str  desc;
    char addstr[512];

    Entity *ent = (Entity *)atobject;
    desc        = "tiki  : " + ent->model + "\n";
    sprintf(addstr, "entnum: %d\n", ent->entnum);
    desc += addstr;
    sprintf(addstr, "origin: (%.2f, %.2f, %.2f)\n", ent->origin.x, ent->origin.y, ent->origin.z);
    desc += addstr;
    sprintf(addstr, "angles: (%.2f, %.2f, %.2f)\n", ent->angles.x, ent->angles.y, ent->angles.z);
    desc += addstr;
    sprintf(addstr, "bounds Mins: ( %.2f, %.2f, %.2f )\n", ent->mins.x, ent->mins.y, ent->mins.z);
    desc += addstr;
    sprintf(addstr, "bounds Maxs: ( %.2f, %.2f, %.2f )\n", ent->maxs.x, ent->maxs.y, ent->maxs.z);
    desc += addstr;
    sprintf(addstr, "size: ( %.2f, %.2f, %.2f )\n", ent->size.x, ent->size.y, ent->size.z);
    desc += addstr;
    sprintf(addstr, "velocity: ( %.2f, %.2f, %.2f )\n\n", ent->velocity.x, ent->velocity.y, ent->velocity.z);
    desc += addstr;

    if (ent->isSubclassOf(Actor)) {
        Actor *act = (Actor *)ent;
        desc += "--------- SUBCLASS OF ACTOR ------------\n";
        sprintf(addstr, "Class ID: %s\n", act->getClassID());
        desc += addstr;
        sprintf(addstr, "Classname: %s\n", act->getClassname());
        desc += addstr;
        sprintf(addstr, "Name: %s\n", act->targetname.c_str());
        desc += addstr;
        if (act->part_name.length() > 0) {
            sprintf(addstr, "Part name: %s\n", act->part_name.c_str());
            desc += addstr;
        }
        sprintf(addstr, "Targetname: %s\n", act->TargetName());
        desc += addstr;

        if (act->behavior) {
            sprintf(addstr, "Behavior: %s\n", act->behavior->getClassname());
        } else {
            sprintf(addstr, "Behavior: NULL -- was '%s'\n", act->currentBehavior.c_str());
        }
        desc += addstr;


        if (act->torsoBehavior) {
            sprintf(addstr, "Torso Behavior: %s\n", act->torsoBehavior->getClassname());
        } else {
            sprintf(addstr, "Torso Behavior: NULL -- was %s\n", act->currentTorsoBehavior.c_str());
        }
        desc += addstr;

        if (act->currentState) {
            sprintf(addstr, "State: %s\n", act->currentState->getName());
        } else {
            sprintf(addstr, "State: NONE\n");
        }
        desc += addstr;

        if (act->GetActorFlag(ACTOR_FLAG_AI_ON)) {
            sprintf(addstr, "AI is ON\n");
        } else {
            sprintf(addstr, "AI is OFF\n");
        }
        desc += addstr;

        if (act->mode == ACTOR_MODE_IDLE) {
            sprintf(addstr, "Mode: IDLE\n");
        } else if (act->mode == ACTOR_MODE_AI) {
            sprintf(addstr, "Mode: AI\n");
        } else if (act->mode == ACTOR_MODE_SCRIPT) {
            sprintf(addstr, "Mode: SCRIPT\n");
        } else if (act->mode == ACTOR_MODE_TALK) {
            sprintf(addstr, "Mode: TALK\n");
        }
        desc += addstr;

        sprintf(addstr, "Actortype: %d\n", act->actortype);
        desc += addstr;
        sprintf(addstr, "Anim: %s\n", act->animname.c_str());
        desc += addstr;
        sprintf(addstr, "Health: %f\n", act->health);
        desc += addstr;

        sprintf(addstr, "CurrentEnemy: ");
        desc += addstr;

        Entity *currentEnemy;
        currentEnemy = act->enemyManager->GetCurrentEnemy();
        if (currentEnemy) {
            sprintf(addstr, "%d : '%s'\n", currentEnemy->entnum, currentEnemy->targetname.c_str());
        } else {
            sprintf(addstr, "None\n");
        }
        desc += addstr;

        switch (act->deadflag) {
        case DEAD_NO:
            sprintf(addstr, "deadflag: NO\n");
            break;
        case DEAD_DYING:
            sprintf(addstr, "deadflag: DYING\n");
            break;
        case DEAD_DEAD:
            sprintf(addstr, "deadflag: DEAD\n");
            break;
        case DEAD_RESPAWNABLE:
            sprintf(addstr, "deadflag: RESPAWNABLE\n");
            break;
        }
        desc += addstr;
    }

    G_SendCommandToPlayer(edict, "ui_addhud showinfo");
    G_EnableWidgetOfPlayer(edict, "InfoHUD", true);
    G_SetWidgetTextOfPlayer(edict, "InfoHUD", desc);
}

void Player::clearActionType()
{
    if (lastActionType.length()) {
        G_EnableWidgetOfPlayer(edict, lastActionType.c_str(), false);
        lastActionType = "";
    }

    atobject = NULL;
}

void Player::handleUseEntity(Entity *ent, float real_dist)
{
    if (ent->hasUseData()) {
        if (real_dist > ent->useData->getUseMaxDist()) {
            clearActionType();
            return;
        }

        if (ent->useData->getUseCount() == 0) {
            delete ent->useData;
            ent->useData = NULL;
            clearActionType();
            return;
        }

        if (ent->useData->getUseType().length() > 0 && !lastActionType.length()) {
            G_EnableWidgetOfPlayer(edict, ent->useData->getUseType().c_str(), true);
            lastActionType = ent->useData->getUseType();
        }
    } else {
        clearActionType();
    }
}

void Player::handleUseObject(UseObject *uo)
{
    if (uo->canBeUsed(origin, yaw_forward)) {
        if (uo->action_type.length() > 0 && !lastActionType.length()) {
            G_EnableWidgetOfPlayer(edict, uo->action_type.c_str(), true);
            lastActionType = uo->action_type;
        }
    } else { 
        clearActionType();
    }
}

void Player::CheckMoveFlags(void)
{
    trace_t trace;
    Vector  start;
    Vector  end;
    float   oldsp;
    Vector  olddir(oldvelocity.x, oldvelocity.y, 0);

    MatrixTransformVector(base_rightfoot_pos, orientation, rightfoot_pos);
    MatrixTransformVector(base_leftfoot_pos, orientation, leftfoot_pos);
    rightfoot_pos += origin;
    leftfoot_pos += origin;

    start = origin + yaw_forward * 52.0f;
    end   = start;
    end.z -= STEPSIZE * 2;

    trace   = G_Trace(start, mins, maxs, end, this, edict->clipmask, true, "CheckMoveFlags");
    canfall = !(trace.fraction < 1.0f);

    CheckGround();

    if (!groundentity && (velocity.z < -250.0f)) {
        falling    = true;
        hardimpact = false;
    } else {
        hardimpact = (oldvelocity.z < -1000.0f);
        falling    = false;
    }

    oldsp = VectorNormalize(olddir);
    if ((oldsp > 250.0f) && (velocity * olddir < 5.0f)) {
        moveresult = MOVERESULT_HITWALL;
    }

    {
        Vector fwd, yfwd;
        float  tracelen, real_dist;
        int    content;

        start[0] = origin[0];
        start[1] = origin[1];
        start[2] = origin[2] + viewheight;
        GetVAngles().AngleVectors(&fwd);
        yfwd = yaw_forward;
        yfwd.normalize();

        tracelen = 96.0f;
        content  = MASK_USABLE;

        if (sv_showinfo->integer) {
            _infoHudOn = true;
            tracelen   = sv_showinfodist->value;

            if (sv_showinfo->integer == 1) {
                content = MASK_ALL;
            } else {
                content = CONTENTS_BODY | CONTENTS_CORPSE;
            }
        }

        if (!sv_showinfo->integer && _infoHudOn) {
            G_EnableWidgetOfPlayer(edict, "InfoHUD", false);
            _infoHudOn = false;
        }

        float tracedist = tracelen / (Vector::Dot(fwd, yfwd)); 
        end             = start + (fwd * tracedist);
        trace = G_Trace(start, Vector(-5, -5, 5), Vector(-5, -5, 5), end, this, content, true, "CheckMoveFlags -- Checking for UseEntities");
        if (trace.ent && trace.ent->entity && (trace.ent->entity != world)) {
            atobject = trace.ent->entity;
            if (trace.startsolid) {
                trace.fraction = 0.0;
            }
            atobject_dist = trace.fraction * tracedist;
            real_dist     = (atobject_dist / tracedist) * tracelen; 
            atobject_dir.setXYZ(-trace.plane.normal[0], -trace.plane.normal[1], -trace.plane.normal[2]);

            if (sv_showinfo->integer) {
                showObjectInfo();
            }

            if (atobject->isSubclassOf(UseObject)) {
                UseObject *uo = (UseObject *)(Entity *)atobject;
                handleUseObject(uo);
            } else {
                Entity *ent = (Entity *)atobject;
                handleUseEntity(ent, real_dist); 
            }
        } else { 
            if (sv_showinfo->integer) {
                G_EnableWidgetOfPlayer(edict, "InfoHUD", false);
            }

            clearActionType();
        }
    }

}

qboolean Player::CheckMove(Vector& move, Vector *endpos)
{
    return AnimMove(move, endpos);
}


void Player::ClientMoveDuck(usercmd_t *ucmd)
{
    if (client->ps.pm_flags & PMF_DUCKED) {
        Vector  newmins(mins);
        Vector  newmaxs(maxs);
        Vector  tmporg(origin);
        trace_t trace;

        tmporg[2] += 2.0f;
        newmins[2] = MINS_Z;
        newmaxs[2] = MAXS_Z;

        trace = G_Trace(tmporg, newmins, newmaxs, tmporg, this, edict->clipmask, true, "checkcanstand");
        if (trace.startsolid) {
            client->ps.pm_flags |= PMF_DUCKED;
        } else {
            client->ps.pm_flags &= ~PMF_DUCKED;
        }
    }

    if (ucmd->upmove < 0 && sv_cancrouch->integer) {
        client->ps.pm_flags |= PMF_DUCKED;
    }
}

void Player::touchingLadder(Trigger *ladder, const Vector& normal, float top)
{
    if (level.time < _nextLadderTime) {
        return;
    }
    if (groundentity && origin.z > top - 48) {
        return;
    }

    _onLadder     = true;
    _ladderNormal = normal;
    _ladderTop    = top;
}

void Player::ClientMoveLadder(usercmd_t *ucmd)
{
    if (!_onLadder) {
        return;
    }

    if (ucmd->upmove > 0) {
        velocity        = _ladderNormal * sv_jumpvelocity->value;  
        _nextLadderTime = level.time + 0.20f;
    } else {
        Vector  playerForward, playerLeft;
        Vector  forward, left;
        Vector  climbDir, climbAngles, climbForward, climbLeft, climbUp;
        bool    goingUp;
        float   upAmount, rightAmount, forwardAmount;
        trace_t trace;
        bool    pushForward;

        v_angle.AngleVectors(&playerForward, &playerLeft);

        goingUp = (playerForward.z > 0.0f);
        if (ucmd->forwardmove < 0) {
            goingUp = !goingUp;
        }

        climbDir    = _ladderNormal * -1.0f;
        climbAngles = climbDir.toAngles();
        climbAngles.AngleVectors(&climbForward, &climbLeft, &climbUp);

        trace       = G_Trace(origin, mins, maxs, origin + climbForward * 4, this, edict->clipmask, qtrue, "ClientMoveLadder");
        pushForward = (trace.fraction >= 1.0f);

        if (ucmd->forwardmove < 0) {
            playerForward *= -1;
        }

        upAmount = playerForward * climbUp;
        if (upAmount >= -0.1f && upAmount <= 0.1f) {
            upAmount = 0.1f;
        }
        forwardAmount = (float)abs((int)(playerForward * climbForward));
        if (upAmount >= 0.0f) {
            upAmount += forwardAmount;
        } else {
            upAmount -= forwardAmount;
        }

        rightAmount = playerForward * climbLeft;
        if (rightAmount >= -0.1f && rightAmount <= 0.1f) {
            rightAmount = 0.0f;
        }

        forward = (climbUp * upAmount + climbLeft * rightAmount) * 0.5f;
        forward.normalize();
        if (pushForward) {
            forward = (climbForward + forward) * 0.5f;
        }

        if (ucmd->rightmove > 0) {
            playerLeft *= -1;
        }

        upAmount      = playerLeft * climbUp;
        forwardAmount = playerLeft * climbForward;
        upAmount += forwardAmount;

        rightAmount = playerLeft * climbLeft;
        if (rightAmount >= -0.1f && rightAmount <= 0.1f) {
            rightAmount = 0.0f;
        }

        left = (climbUp * upAmount + climbLeft * rightAmount) * 0.5f;
        left.normalize();
        if (pushForward) {
            left = (climbForward + left) * 0.5f;
        }

        forward.normalize();

        // MOHAA's sv_runspeed (a cvar retail does not have).
        forward *= fabs((float)ucmd->forwardmove) / 127.0f * sv_maxspeed->value * 0.75f;
        left *= fabs((float)ucmd->rightmove) / 127.0f * sv_maxspeed->value * 0.75f;

        velocity          = forward + left;
        ucmd->forwardmove = 0;

        client->ps.pm_flags |= PMF_NO_GRAVITY;

        if (groundentity && !goingUp) {
            _nextLadderTime = level.time + 0.20f;
        }
    }

    _onLadder = false;
}

void Player::ClientMove(usercmd_t *ucmd)
{
    pmove_t pmove;
    Vector  move;

    int moveSpeed;
    int noclipSpeed = (int)sv_noclipspeed->value;
    int crouchSpeed;
    int airSpeed;

    if (world->getPhysicsVar(WORLD_PHYSICS_MAXSPEED) != -1.0f) {
        moveSpeed = (int)world->getPhysicsVar(WORLD_PHYSICS_MAXSPEED);
    } else {
        moveSpeed = (int)sv_maxspeed->value;
    }

    if (sv_crouchspeed->value > 0.0f) {
        crouchSpeed = (int)sv_crouchspeed->value;
    } else {
        crouchSpeed = (int)((float)moveSpeed * 0.5);
    }

    if (sv_airmaxspeed->value > 0.0f) {
        airSpeed = (int)sv_airmaxspeed->value;
    } else {
        airSpeed = moveSpeed;
    }

    oldorigin = origin;

    client->ps.pm_type = GetMovePlayerMoveType();

        client->ps.pm_flags &= ~(PMF_FLIGHT | PMF_FROZEN | PMF_NO_PREDICTION | PMF_NO_MOVE
                             | PMF_SPECTATING | PMF_NO_GRAVITY);

    ApplyPowerupEffects(moveSpeed);
    applyWeaponSpeedModifiers(&moveSpeed);
    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.applySpeedModifiers(this, &moveSpeed);
    }

    ApplyPowerupEffects(crouchSpeed);
    applyWeaponSpeedModifiers(&crouchSpeed);
    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.applySpeedModifiers(this, &crouchSpeed);
    }

    ApplyPowerupEffects(airSpeed);
    applyWeaponSpeedModifiers(&airSpeed);
    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.applySpeedModifiers(this, &airSpeed);
    }

    ClientMoveDuck(ucmd);

    ClientMoveLean(ucmd);

    ClientMoveFlagsAndSpeeds(moveSpeed, noclipSpeed, crouchSpeed, airSpeed);

    ClientMoveMisc(ucmd);

    ClientMoveLadder(ucmd);

    client->ps.gravity = (int)(sv_currentGravity->value * gravity);

    CheckGround();
    SetMoveInfo(&pmove, ucmd);
    Pmove(&pmove);
    GetMoveInfo(&pmove);
    if (pmove.jumpResult == 1) {
        /* ent-training1's crate lesson: the grey case2 pair tops 108 over the z-172 floor
           and the walkway 112 more, so with STEPSIZE 33 the two rises need 75 and 79 */
        float v0   = (float)client->ps.jumpvelocity
                     + (pmove.easyJump ? (float)client->ps.crouchjumpvelocity : 0.0f);
        float apex = v0 * v0 / (2.0f * (float)client->ps.gravity);
        float hop  = apex - (108.0f - 33.0f);
        float gate = apex - (112.0f - 33.0f);

        PROBE("jump", "srv takeoff easy=%d duck=%d up=%d vz=%d v0=%d apex=%.2f hop=%+.2f gate=%+.2f\n",
              (int)pmove.easyJump, (client->ps.pm_flags & PMF_DUCKED) ? 1 : 0, (int)ucmd->upmove,
              (int)client->ps.velocity[2], (int)v0, apex, hop, gate);
    } else if (pmove.jumpResult == 2) {
        PROBE("jump", "srv eaten easy=%d duck=1 up=%d vz=%d\n", (int)pmove.easyJump, (int)ucmd->upmove,
              (int)client->ps.velocity[2]);
    }
    ProcessPmoveEvents(pmove.pmoveEvent);

    if ((getMoveType() == MOVETYPE_NOCLIP) || !(client->ps.pm_flags & PMF_NO_PREDICTION)) {
        total_delta = vec_zero;
    } else {
        if (movecontrol == MOVECONTROL_ABSOLUTE) {
            velocity = vec_zero;
        }

        if (total_delta != vec_zero) {
            MatrixTransformVector(total_delta, orientation, move);
            CheckMove(move, &origin);
            setOrigin(origin);
            CheckGround();
        }
    }

    total_delta = vec_zero;

    TouchStuff(&pmove);

    if ((whereami->integer) && (origin != oldorigin)) {
        gi.DPrintf("x %8.2f y%8.2f z %8.2f area %2d\n", origin[0], origin[1], origin[2], edict->areanum);
    }
}

// retail's Player never had a vehicle move function at all, only the two ifs now in


void Player::GetViewTrace(trace_t& trace, int contents, float maxDistance)
{
    Vector f, r, u, p, s;
    Vector pos, forward, endpoint, vorg, dir;
    vec3_t viewAngles;

    memset(&trace, 0, sizeof(trace_t));

    if (client->ps.pm_flags & PMF_CAMERA_VIEW) 
    {
        vorg = client->ps.camera_origin;
        VectorCopy(client->ps.camera_angles, viewAngles);
    } else if (!_isThirdPerson) 
    {
        vorg = origin;
        vorg.z += client->ps.viewheight;
        VectorCopy(GetVAngles(), viewAngles);
    } else 
    {
        VectorCopy(GetVAngles(), viewAngles);
    }

    vec3_t left;
    if (client->ps.leanDelta != 0) {
        viewAngles[2] -= client->ps.leanDelta / 2.0f;

        AngleVectorsLeft(viewAngles, NULL, left, NULL);
        VectorMA(vorg, client->ps.leanDelta, left, vorg);
    }

    VectorSubtract(viewAngles, client->ps.damage_angles, viewAngles);

    AngleVectorsLeft(viewAngles, f, r, u);

    float xmax, ymax, fov_x, fov_y, x;

    fov_x = client->ps.fov;
    x     = 640.0f / (float)tan(fov_x / 360.0 * M_PI);
    fov_y = (float)atan2(480.0, x);
    fov_y *= 360.0f / M_PI;

    ymax = 4.0f * (float)tan(fov_y * M_PI / 360.0);
    xmax = 4.0f * (float)tan(fov_x * M_PI / 360.0);

    p.x = (float)_crossHairXOffset * (-xmax / 320.0f);
    p.y = (float)_crossHairYOffset * (-ymax / 240.0f);
    s   = (4.0f * f) + (p.x * r) + (p.y * u);
    s.normalize();
    endpoint = vorg + (s * maxDistance);

    trace = G_FullTrace(
        vorg,
        vec_zero,
        vec_zero,
        endpoint,
        this,
        contents,
        qtrue,
        "Player::GetViewEndPoint"
    );
}

void Player::CheckForTargetedEntity(void)
{
    trace_t viewTrace;

    GetViewTrace(viewTrace, MASK_SHOT | CONTENTS_TARGETABLE);

    if (viewTrace.ent && viewTrace.entityNum != ENTITYNUM_WORLD) {
        Entity *hit = viewTrace.ent->entity;

        if (hit && hit->isSubclassOf(Actor) && hit->health <= 0) {
            SetTargetedEntity(NULL);
        } else {
            SetTargetedEntity(hit);
        }
        return;
    }

    SetTargetedEntity(NULL);
}

void Player::SetTargetedEntity(Entity *entity)
{
    if (_targetedEntity) {
        PROBE_CENSUS_EMIT("[G:", "target",
                          "drop ent=%d classname='%s'\n", _targetedEntity->entnum,
                          _targetedEntity->getClassname());
        _targetedEntity->edict->s.eFlags &=
            ~(EF_DISPLAY_INFO | EF_DISPLAY_DESC1 | EF_DISPLAY_DESC2 | EF_DISPLAY_DESC3);
    }

    _targetedEntity = entity;

    if (_targetedEntity) {
        PROBE_CENSUS_EMIT("[G:", "target",
                          "acquire ent=%d classname='%s' health=%.0f\n", _targetedEntity->entnum,
                          _targetedEntity->getClassname(), _targetedEntity->health);
        _targetedEntity->edict->s.eFlags |= EF_DISPLAY_INFO;
    }
}

Entity *Player::GetTargetedEntity(void) const
{
    return _targetedEntity;
}

void Player::ProcessTargetedEntity(void)
{
    if (!_targetedEntity) {
        return;
    }

    _targetedEntity->edict->s.eFlags |= EF_DISPLAY_DESC1;
}

void Player::ClientThink(Event *ev)
{
    if (current_ucmd->serverTime > level.inttime) {
        return;
    }

    if (current_ucmd->serverTime < level.inttime - 1000) {
        current_ucmd->serverTime = level.inttime - 1000;
    }

    if ((current_ucmd->serverTime - client->ps.commandTime) < 1) {
        return;
    }

    _started = true;

    last_ucmd  = *current_ucmd;
    new_buttons = current_ucmd->buttons & ~buttons;
    buttons     = current_ucmd->buttons;

    {
        GameplayManager *gpm = GameplayManager::getTheGameplayManager();
        str scopestr = "CurrentPlayer.HPRegen";
        if (gpm->hasObject(scopestr)) {
            int value = (int)gpm->getFloatValue(scopestr, "value");
            if (level.inttime > _nextRegenTime && value > 0) {
                _nextRegenTime = level.inttime + ((6 - value) * 1000);
                AddHealth(1.0f);
            }
        }
    }

    if (_useEntityStartTimer != 0.0f) {
        if (level.time > _useEntityStartTimer) {
            _usingEntity = false;
            _useEntityStartTimer = 0.0f;
        }
    }

    if (current_ucmd->thirdperson) {
        _isThirdPerson = true;
    } else {
        _isThirdPerson = false;
    }

    if (world->getPhysicsVar(WORLD_PHYSICS_AIRACCELERATE) != -1.0f) {
        client->ps.pm_airaccelerate = (int)world->getPhysicsVar(WORLD_PHYSICS_AIRACCELERATE);
    } else {
        client->ps.pm_airaccelerate = (int)sv_airaccelerate->value;
    }

    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.applyAirAccelerationModifiers(this, &client->ps.pm_airaccelerate);
    }

    client->ps.pm_wateraccelerate = (int)sv_wateraccelerate->value;
    client->ps.pm_stopspeed       = (int)sv_stopspeed->value;
    client->ps.pm_friction        = (int)sv_friction->value;
    client->ps.pm_waterfriction   = (int)sv_waterfriction->value;
    client->ps.jumpvelocity       = (int)sv_jumpvelocity->value;

    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.applyJumpModifiers(this, &client->ps.jumpvelocity);
    }

    client->ps.crouchjumpvelocity = (int)sv_crouchjumpvelocity->value;

    client->ps.pm_accelerate       = (int)sv_accelerate->value;
    client->ps.pm_defaultviewheight = (int)sv_defaultviewheight->value;

    viewheight = client->ps.pm_defaultviewheight;

    if (level.intermissiontime) {
        static int missionend_start, missionend_cross;

        client->ps.pm_flags |= PMF_FROZEN;

        if (PROBE_Throttle(&missionend_start, 1, 1 << 30)) {
            WIDGET_PROBE("missionend",
                         "think first time=%.3f intermission=%.3f advance=%.3f dt=%.3f exit=%d\n",
                         level.time, level.intermissiontime, level.intermission_advancetime,
                         level.time - level.intermissiontime, level.exitintermission ? 1 : 0);
        }
        if ((level.intermission_advancetime != 0.0f)
            && ((level.time - level.intermissiontime) > level.intermission_advancetime)
            && PROBE_Throttle(&missionend_cross, 1, 1 << 30)) {
            WIDGET_PROBE("missionend",
                         "think crossed time=%.3f intermission=%.3f advance=%.3f dt=%.3f\n",
                         level.time, level.intermissiontime, level.intermission_advancetime,
                         level.time - level.intermissiontime);
        }

        if (g_endintermission->integer > 0) {
            g_endintermission->integer = 0;
            level.exitintermission = true;
        }
        if (((level.time - level.intermissiontime) > level.intermission_advancetime)
            && (level.intermission_advancetime != 0)) {
            if (multiplayerManager.inMultiplayer()) {
                if ((new_buttons & BUTTON_ATTACKRIGHT) || (new_buttons & BUTTON_ATTACKLEFT)) {
                    level.exitintermission = true;
                }
            } else {
                level.exitintermission = true;
            }
        }

        client->cmd_angles[0] = SHORT2ANGLE(current_ucmd->angles[0]);
        client->cmd_angles[1] = SHORT2ANGLE(current_ucmd->angles[1]);
        client->cmd_angles[2] = SHORT2ANGLE(current_ucmd->angles[2]);

        return;
    }

    moveresult = MOVERESULT_NONE;

    if (!vehicle || !vehicle->Drive(current_ucmd)) {
        ClientMove(current_ucmd);
    }

    if (vehicle) {
        pmove_t pmove;

        SetMoveInfo(&pmove, current_ucmd);
        Pmove(&pmove);
    }

    if (current_ucmd) {
        _crossHairXOffset = current_ucmd->choffset[0];
        _crossHairYOffset = current_ucmd->choffset[1];
    }

    client->ps.pm_flags &= ~(PMF_RADAR_MODE | PMF_SCANNER);

    CheckForTargetedEntity();
    ProcessTargetedEntity();

    {
        Weapon *weap;
        weap = GetActiveWeapon(WEAPON_DUAL);
        if (weap) {
            if (weap->GetPutaway()) {
            } else {
                weap->ProcessTargetedEntity(GetTargetedEntity());
            }

            if (((weap->GetFireType(FIRE_MODE1) == FT_CONTROL_PROJECTILE)
                 && (new_buttons & BUTTON_ATTACKLEFT))
                || ((weap->GetFireType(FIRE_MODE2) == FT_CONTROL_PROJECTILE)
                    && (new_buttons & BUTTON_ATTACKRIGHT))) {
                weap->toggleProjectileControl();
            }

            if (((weap->GetFireType(FIRE_MODE1) == FT_CONTROL_ZOOM) && (new_buttons & BUTTON_ATTACKLEFT))
                || ((weap->GetFireType(FIRE_MODE2) == FT_CONTROL_ZOOM)
                    && (new_buttons & BUTTON_ATTACKRIGHT))) {
                weap->changeZoomStage(40.0f, 10.0f);
            }
        }
    }

    if (specialMoveCharge > 0.0f) {
        specialMoveCharge = level.time;
    }

    // retail traces for every client frame -- was defined and never called at all.
    if (getMoveType() == MOVETYPE_NOCLIP) {
        SetAnim("idle", all);
    } else {
        CheckMoveFlags();
        EvaluateState();

        if (groundentity && groundentity->entity && groundentity->entity->isSubclassOf(Actor)) {
            Event *event = new Event(EV_Actor_Push);
            event->AddVector(Vector(0.0f, 0.0f, -10.0f));
            groundentity->entity->PostEvent(event, 0.0f);
        }
    }

    oldvelocity = velocity;
    old_v_angle = v_angle;

    if (deadflag == DEAD_DEAD && (level.time > respawn_time)) {
        TIKIANIM_PROBE("respawn:gate", "ent=%d DEAD_DEAD t=%.2f respawn_time=%.2f buttons=0x%x frozen=%d",
                   entnum, (float)level.time, (float)respawn_time, new_buttons,
                   level.playerfrozen ? 1 : 0);
        if ((new_buttons & BUTTON_ATTACKRIGHT) || (new_buttons & BUTTON_ATTACKLEFT)
            || (multiplayerManager.inMultiplayer() && multiplayerManager.checkFlag(MP_FLAG_FORCE_RESPAWN))) {
            if (multiplayerManager.inMultiplayer()) {
                TIKIANIM_PROBE("respawn:fire", "ent=%d -> multiplayerManager.respawnPlayer(forced=false)", entnum);
                multiplayerManager.respawnPlayer(this, false);
            } else {
                G_RestartLevelWithDelay(1.0f);
            }
        }
    }

    client->cmd_angles[0] = SHORT2ANGLE(current_ucmd->angles[0]);
    client->cmd_angles[1] = SHORT2ANGLE(current_ucmd->angles[1]);
    client->cmd_angles[2] = SHORT2ANGLE(current_ucmd->angles[2]);

    if (client->ps.pm_flags & PMF_CAMERA_VIEW) {
        VectorCopy(current_ucmd->realvieworigin, client->ps.camera_origin);
        VectorCopy(current_ucmd->realviewangles, client->ps.camera_angles);
    }

    if (g_logstats->integer) {
        if (!logfile_started) {
            ProcessEvent(EV_Player_LogStats);
            logfile_started = true;
        }
    }

    if ((last_ucmd.buttons & BUTTON_USE) != 0) {
        ProcessEvent(EV_Player_DoUse);
    }

    if ((last_ucmd.buttons & BUTTON_DROP_RUNE) != 0) {
        dropRune();
    }

    if ((last_ucmd.buttons & BUTTON_TRANSFER_ENERGY) != 0) {
        transferEnergy();
    }

    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.playerInput(this, new_buttons);
    }

    incomingMeleeAttack = false;
    if (meleeAttackerList.NumObjects()) {
        Entity *attacker;
        int     i;
        for (i = meleeAttackerList.NumObjects(); i >= 1; i--) {
            attacker = (Entity *)meleeAttackerList.ObjectAt(i);
            if (!attacker) {
                continue;
            }

            if (attacker->isSubclassOf(Actor)) {
                Actor *act = (Actor *)attacker;
                if (act->in_melee_attack) {
                    incomingMeleeAttack = true;
                } else {
                    meleeAttackerList.RemoveObjectAt(i);
                }
            }
        }

        if (!incomingMeleeAttack) {
            meleeAttackerList.ClearObjectList();
        }
    }
}


void Player::LoadStateTable(void)
{
    int          i;
    Conditional *cond;

    statemap_Legs  = NULL;
    statemap_Torso = NULL;

    for (i = legs_conditionals.NumObjects(); i > 0; i--) {
        cond = legs_conditionals.ObjectAt(i);
        delete cond;
    }
    legs_conditionals.FreeObjectList();

    for (i = torso_conditionals.NumObjects(); i > 0; i--) {
        cond = torso_conditionals.ObjectAt(i);
        delete cond;
    }
    torso_conditionals.FreeObjectList();

    statemap_Legs =
        GetStatemap(str(g_statefile->string) + "_Legs.st", (Condition<Class> *)Conditions, &legs_conditionals, false);
    statemap_Torso = GetStatemap(
        str(g_statefile->string) + "_Torso.st", (Condition<Class> *)Conditions, &torso_conditionals, false
    );

    SetState("STAND", "START");
}

void Player::cinematicStarted(void)
{
    setViewMode("normal");

    Weapon *weapon = GetActiveWeapon(WEAPON_DUAL);
    if (weapon) {
        weapon->ProcessEvent("endZoom");

        weapon->ForceIdle();
    }
}

void Player::cinematicStopped(void)
{
    Weapon    *weapon;
    Equipment *equipment;

    weapon = GetActiveWeapon(WEAPON_DUAL);
    if (weapon && weapon->isSubclassOf(Equipment)) {
        equipment = (Equipment *)weapon;
        equipment->updateMode();
    }

    SetAnim("stand_idle", legs, true);  
    SetAnim("stand_idle", torso, true); 
    LoadStateTable();
}

void Player::ResetState(Event *ev)
{
    movecontrol = MOVECONTROL_LEGS;
    LoadStateTable();
}

void Player::StartPush(void)
{
    trace_t trace;
    Vector  end(origin + yaw_forward * 64.0f);

    trace = G_Trace(origin, mins, maxs, end, this, MASK_SOLID, true, "StartPush");
    if (trace.fraction == 1.0f) {
        return;
    }
    v_angle.y = vectoyaw(trace.plane.normal) - 180;
    SetViewAngles(v_angle);

    setOrigin(trace.endpos - yaw_forward * 0.4f);
}

void Player::StartUseAnim(void)
{
    UseAnim *ua;
    Vector   neworg;
    Vector   newangles;
    str      newanim;
    str      state;
    str      cameraName;
    trace_t  trace;

    if (toucheduseanim) {
        ua = (UseAnim *)(Entity *)toucheduseanim;
    } else if (atobject) {
        ua = (UseAnim *)(Entity *)atobject;
    } else {
        return;
    }

    useitem_in_use = ua;
    toucheduseanim = NULL;
    atobject       = NULL;

    if (ua->GetInformation(this, &neworg, &newangles, &newanim, &useanim_numloops, &state, &cameraName)) {
        trace = G_Trace(origin, mins, maxs, neworg, this, MASK_PLAYERSOLID, true, "StartUseAnim");
        if (trace.startsolid || (trace.fraction < 1.0f)) {
            gi.WDPrintf("Move to UseAnim was blocked.\n");
        }

        if (!trace.startsolid) {
            setOrigin(trace.endpos);
        }

        setAngles(newangles);
        v_angle.y = newangles.y;
        SetViewAngles(v_angle);

        movecontrol = MOVECONTROL_ABSOLUTE;

        if (state.length()) {
            State *newState;

            newState = statemap_Torso->FindState(state);
            if (newState) {
                EvaluateState(newState);
            } else {
                gi.WDPrintf("Could not find state %s on UseAnim\n", state.c_str());
            }
        } else {
            if (currentState_Torso) {
                if (cameraName.length()) {
                    currentState_Torso->setCameraType(cameraName);
                } else {
                    currentState_Torso->setCameraType("behind");
                }
            }
            SetAnim(newanim, legs);  
        }
    }
}

void Player::StartLoopUseAnim(void)
{
    useanim_numloops--;
}

void Player::FinishUseAnim(Event *ev)
{
    UseAnim *ua;

    if (!useitem_in_use) {
        return;
    }

    ua = (UseAnim *)(Entity *)useitem_in_use;
    ua->TriggerTargets(this);
    useitem_in_use = NULL;
}

void Player::SetupUseObject(void)
{
    UseObject *uo;
    Vector     neworg;
    Vector     newangles;
    str        state;
    trace_t    trace;

    if (atobject) {
        uo = (UseObject *)(Entity *)atobject;
    } else {
        return;
    }

    useitem_in_use = uo;

    uo->Setup(this, &neworg, &newangles, &state);
    {
        trace = G_Trace(neworg, mins, maxs, neworg, this, MASK_PLAYERSOLID, true, "SetupUseObject - 1");
        if (trace.startsolid || trace.allsolid) {
            trace = G_Trace(origin, mins, maxs, neworg, this, MASK_PLAYERSOLID, true, "SetupUseObject - 2");
            if (trace.startsolid || (trace.fraction < 1.0f)) {
                gi.WDPrintf("Move to UseObject was blocked.\n");
            }
        }

        if (!trace.startsolid) {
            setOrigin(trace.endpos);
        }

        setAngles(newangles);
        v_angle.y = newangles.y;
        SetViewAngles(v_angle);

        movecontrol = MOVECONTROL_ABSOLUTE;

        if (state.length()) {
            State *newState;

            newState = statemap_Torso->FindState(state);
            if (newState) {
                EvaluateState(newState);
            } else {
                gi.WDPrintf("Could not find state %s on UseObject\n", state.c_str());
            }
        }
    }
}

void Player::StartUseObject(Event *ev)
{
    UseObject *uo;

    if (!useitem_in_use) {
        return;
    }

    uo = (UseObject *)(Entity *)useitem_in_use;
    uo->Start();
}

void Player::FinishUseObject(Event *ev)
{
    UseObject *uo;

    if (!useitem_in_use) {
        return;
    }

    uo = (UseObject *)(Entity *)useitem_in_use;
    uo->Stop(this);
    useitem_in_use = NULL;
}

void Player::Turn(Event *ev)
{
    float yaw;
    float time;
    int   numFrames;

    yaw = ev->GetFloat(1);

    if (ev->NumArgs() > 1) {
        time = ev->GetFloat(2);
    } else {
        time = 0.5;
    }

    numFrames = (int)(time * (1 / level.frametime));
    time = numFrames * level.frametime;

    CancelEventsOfType(EV_Player_TurnUpdate);

    if (time > 0) {
        ev = new Event(EV_Player_TurnUpdate);
        ev->AddFloat(yaw / numFrames);
        ev->AddFloat(time);
        ProcessEvent(ev);
    } else {
        v_angle[YAW] += yaw;
        SetViewAngles(v_angle);
    }
}

void Player::TurnUpdate(Event *ev)
{
    float yaw;
    float timeleft;

    yaw      = ev->GetFloat(1);
    timeleft = ev->GetFloat(2);


    timeleft -= 0.05f;


    v_angle[YAW] += yaw;
    SetViewAngles(v_angle);


    if (timeleft > 0.0f) {
        ev = new Event(EV_Player_TurnUpdate);
        ev->AddFloat(yaw);
        ev->AddFloat(timeleft);
        PostEvent(ev, 0.05f);
    }
}

void Player::TurnLegs(Event *ev)
{
    float yaw;

    yaw = ev->GetFloat(1);

    angles[YAW] += yaw;
    setAngles(angles);
}

void Player::DontTurnLegs(Event *ev)
{
    dont_turn_legs = ev->GetBoolean(1);
}


void Player::EvaluateState(State *forceTorso, State *forceLegs)
{
    int           count;
    State        *laststate_Legs;
    State        *laststate_Torso;
    State        *startstate_Legs;
    State        *startstate_Torso;
    movecontrol_t move;

    if (getMoveType() == MOVETYPE_NOCLIP) {
        return;
    }

    if (flags & FL_IMMOBILE) {
        return;
    }

    startstate_Torso = laststate_Torso = currentState_Torso;
    count                              = 0;
    do {
        count++;
        if (count > 10) {
            gi.WDPrintf("Possible infinite loop in state '%s'\n", currentState_Torso->getName());
            assert(0);
            if (count > 20) {
                gi.Error(ERR_DROP, "Stopping due to possible infinite state loop\n");
                break;
            }
        }

        laststate_Torso = currentState_Torso;

        if (currentState_Torso) {
            laststate_Torso = currentState_Torso;

            if (forceTorso) {
                currentState_Torso = forceTorso;
            } else {
                currentState_Torso = currentState_Torso->Evaluate(*this, &torso_conditionals);
            }
        } else {
            if (forceTorso) {
                currentState_Torso = forceTorso;
            } else {
                currentState_Torso = statemap_Torso->FindState("STAND");
            }

            laststate_Torso = NULL;
        }

        animdone_Torso = false;
        if (movecontrol != MOVECONTROL_LEGS) {
            animdone_Legs = false;
        }

        if (laststate_Torso != currentState_Torso) {
            // Process exit commands of the last state.  The NULL test is not retail's:
            // (SDK player.cpp:6196-6201) so its laststate_Torso can never be NULL here.
            if (laststate_Torso) {
                laststate_Torso->ProcessExitCommands(this);
            }

            currentState_Torso->ProcessEntryCommands(this);

            if (waitForState.length() && (!waitForState.icmpn(currentState_Torso->getName(), waitForState.length()))) {
                waitForState = "";
                PlayerDone(NULL);  
            }

            move = currentState_Torso->getMoveType();

            str legsAnim(currentState_Torso->getLegAnim(*this, &torso_conditionals));
            if (!animate->HasAnim(legsAnim)) {
                legsAnim = getGameplayAnim(legsAnim);
            }
            str torsoAnim(currentState_Torso->getTorsoAnim(*this, &torso_conditionals));
            if (!animate->HasAnim(torsoAnim)) {
                torsoAnim = getGameplayAnim(torsoAnim);
            }

            if (legsAnim != "") {
                animdone_Legs = false;
                if (!vehicle) {
                    SetAnim(legsAnim, legs);
                }
            } else if (move == MOVECONTROL_LEGS) {
                if (!currentState_Legs) {
                    animdone_Legs     = false;
                    currentState_Legs = statemap_Legs->FindState("STAND");
                    legsAnim          = currentState_Legs->getLegAnim(*this, &legs_conditionals);

                    if (!animate->HasAnim(legsAnim)) {
                        legsAnim = getGameplayAnim(legsAnim);
                    }
                    if (legsAnim == "") {
                        partAnim[legs] = "";
                        animate->ClearLegsAnim();
                    } else if (legsAnim != "none") {
                        if (!vehicle) {
                            SetAnim(legsAnim, legs);
                        }
                    }
                }
            } else {
                partAnim[legs] = "";
                animate->ClearLegsAnim();
            }

            if (torsoAnim == "") {
                partAnim[torso] = "";
                animate->ClearTorsoAnim();
            } else if (torsoAnim != "none") {
                SetAnim(torsoAnim.c_str(), torso);
            }

            static int uirows_botanim;
            if (PROBE_On() && PROBE_Throttle(&uirows_botanim, 24, 400)) {
                Weapon *uirows_w = GetActiveWeapon(WEAPON_DUAL);
                UIROWS_PROBE(
                    "bot:anim",
                    "ent=%d model='%s' state='%s' move=%d movecontrol=%d wname='%s' "
                    "legsAnim='%s' has=%d torsoAnim='%s' has=%d\n",
                    entnum,
                    model.c_str(),
                    currentState_Torso ? currentState_Torso->getName() : "(null)",
                    move,
                    movecontrol,
                    uirows_w ? uirows_w->item_name.c_str() : "(none)",
                    legsAnim.c_str(),
                    animate->HasAnim(legsAnim) ? 1 : 0,
                    torsoAnim.c_str(),
                    animate->HasAnim(torsoAnim) ? 1 : 0);
            }

            if (movecontrol != move) {
                movecontrol = move;
                if ((move < (sizeof(MoveStartFuncs) / sizeof(MoveStartFuncs[0]))) && (MoveStartFuncs[move])) {
                    (this->*MoveStartFuncs[move])();
                }
            }

            // is not updated correctly.  I don't think it's necessary to call it anyway,
        }
    } while (laststate_Torso != currentState_Torso);

    startstate_Legs = laststate_Legs = currentState_Legs;
    if (movecontrol == MOVECONTROL_LEGS) {
        count = 0;
        do {
            count++;
            if (count > 10) {
                gi.WDPrintf("Possible infinite loop in state '%s'\n", currentState_Legs->getName());
                assert(0);
                if (count > 20) {
                    gi.Error(ERR_DROP, "Stopping due to possible infinite state loop\n");
                    break;
                }
            }

            if (currentState_Legs) {
                laststate_Legs = currentState_Legs;

                if (forceLegs) {
                    currentState_Legs = forceLegs;
                } else {
                    currentState_Legs = currentState_Legs->Evaluate(*this, &legs_conditionals);
                }
            } else {
                if (forceLegs) {
                    currentState_Legs = forceLegs;
                } else {
                    currentState_Legs = statemap_Legs->FindState("STAND");
                }

                laststate_Legs = NULL;
            }

            animdone_Legs = false;

            if (laststate_Legs != currentState_Legs) {
                if (laststate_Legs) {
                    laststate_Legs->ProcessExitCommands(this);
                }

                currentState_Legs->ProcessEntryCommands(this);

                if (waitForState.length()
                    && (!waitForState.icmpn(currentState_Legs->getName(), waitForState.length()))) {
                    waitForState = "";
                    PlayerDone(NULL);
                }

                str legsAnim(currentState_Legs->getLegAnim(*this, &legs_conditionals));

                if (!animate->HasAnim(legsAnim)) {
                    legsAnim = getGameplayAnim(legsAnim);
                }

                if (legsAnim == "") {
                    partAnim[legs] = "";
                    animate->ClearLegsAnim();
                } else if (legsAnim != "none") {
                    if (!vehicle) {
                        SetAnim(legsAnim, legs);
                    }
                }

                {
                    static int widget_legs;

                    if (WIDGET_On() && WIDGET_Throttle(&widget_legs, 12, 48)) {
                        WIDGET_A1_PROBE(
                            "legs:loop",
                            "ent=%d move=%d movecontrol=%d state='%s' anim='%s' has=%d "
                            "clear=%d curlegs='%s' ucmdfwd=%d ucmdright=%d\n",
                            entnum,
                            movecontrol,
                            movecontrol,
                            currentState_Legs ? currentState_Legs->getName() : "(null)",
                            legsAnim.c_str(),
                            animate->HasAnim(legsAnim) ? 1 : 0,
                            (legsAnim == "") ? 1 : 0,
                            animate->AnimName(legs) ? animate->AnimName(legs) : "?",
                            last_ucmd.forwardmove,
                            last_ucmd.rightmove);
                    }
                }
            }
        } while (laststate_Legs != currentState_Legs);
    } else {
        currentState_Legs = NULL;
    }

    if (g_showplayeranim->integer) {
        str sNewAnim;

        sNewAnim = animate->AnimName(legs);
        if (last_leg_anim_name != sNewAnim) {
            gi.DPrintf("Legs change from %s to %s\n", last_leg_anim_name.c_str(), sNewAnim.c_str());
            last_leg_anim_name = sNewAnim;
        }

        sNewAnim = animate->AnimName(torso);
        if (last_torso_anim_name != sNewAnim) {
            gi.DPrintf("Torso change from %s to %s\n", last_torso_anim_name.c_str(), sNewAnim.c_str());
            last_torso_anim_name = sNewAnim;
        }
    }

    if (g_showplayerstate->integer) {
        if (startstate_Legs != currentState_Legs) {
            gi.DPrintf(
                "Legs change from %s to %s\n",
                startstate_Legs ? startstate_Legs->getName() : "NULL",
                currentState_Legs ? currentState_Legs->getName() : "NULL"
            );
        }

        if (startstate_Torso != currentState_Torso) {
            gi.DPrintf(
                "Torso change from %s to %s\n",
                startstate_Torso ? startstate_Torso->getName() : "NULL",
                currentState_Torso ? currentState_Torso->getName() : "NULL"
            );
        }
    }

    if (g_showplayerweapon->integer) {
        Weapon *weapon;

        weapon = GetActiveWeapon(WEAPON_DUAL);

        if (!weapon) {
            weapon = GetActiveWeapon(WEAPON_RIGHT);
        }

        if (!weapon) {
            weapon = GetActiveWeapon(WEAPON_LEFT);
        }

        if (weapon) {
            gi.DPrintf("Weapon - anim %s, frame %d\n", weapon->animate->GetName().c_str(), weapon->animate->CurrentFrame(legs));
        }
    }

    pain = 0;
}



void Player::AnimateWeapon(Event *ev)
{
    Weapon      *weapon = NULL;
    weaponhand_t hand = WEAPON_DUAL;
    bool         animatingFlag = true;

    if (ev->NumArgs() > 1) {
        hand = WeaponHandNameToNum(ev->GetString(2));
    }

    if (ev->NumArgs() > 2) {
        animatingFlag = ev->GetBoolean(3);
    }

    weapon = GetActiveWeapon(hand);
    if (!weapon) {
        return;
    }

    weapon->playAnim(ev->GetString(1), animatingFlag);
}



void Player::GiveWeaponCheat(Event *ev)
{
    giveItem(ev->GetString(1));
}

void Player::GiveCheat(Event *ev)
{
    str name;

    if (deadflag) {
        return;
    }

    name = ev->GetString(1);

    if (!name.icmp("all")) {
        GiveAllCheat(ev);
        return;
    }
    EventGiveItem(ev);
}

void Player::GiveAllCheat(Event *ev)
{
    char *buffer;
    char *buf;
    char  com_token[MAX_STRING_CHARS];
    int   numEvents;
    float postTime;

    if (deadflag) {
        return;
    }

    if (!gi.isClientActive(this->edict)) {
        PostEvent(*ev, FRAMETIME);
        return;
    }

    numEvents = 0;
    postTime = 0.0f;

    if (gi.FS_ReadFile("global/giveall.scr", (void **)&buf, true) != -1) {
        buffer = buf;
        while (1) {
            Q_strncpyz(com_token, COM_ParseExt(&buffer, qtrue), sizeof(com_token));

            if (!com_token[0]) {
                break;
            }

            ev = new Event(com_token);

            while (1) {
                Q_strncpyz(com_token, COM_ParseExt(&buffer, qfalse), sizeof(com_token));
                if (!com_token[0]) {
                    break;
                }

                ev->AddToken(com_token);
            }

            if (numEvents >= 5) {
                numEvents = 0;
                postTime += 1.0f;
            }

            PostEvent(ev, postTime);

            numEvents++;
        }
        gi.FS_FreeFile(buf);
    }
}

void Player::GodCheat(Event *ev)
{
    const char *msg;

    if (ev->NumArgs() > 0) {
        if (ev->GetInteger(1)) {
            flags |= FL_GODMODE;
        } else {
            flags &= ~FL_GODMODE;
        }
    } else {
        flags ^= FL_GODMODE;
    }

    if (ev->GetSource() == EV_FROM_CONSOLE) {
        if (!(flags & FL_GODMODE)) {
            msg = "godmode OFF\n";
        } else {
            msg = "godmode ON\n";
        }

        gi.SendServerCommand(edict - g_entities, "print \"%s\"", msg);
    }
}

void Player::Kill(Event *ev)
{
    if ((level.time - respawn_time) < 5) {
        return;
    }

    flags &= ~FL_GODMODE;
    health = 1;
    Damage(this, this, 10, origin, vec_zero, vec_zero, 0, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
}

void Player::NoTargetCheat(Event *ev)
{
    const char *msg;

    flags ^= FL_NOTARGET;
    if (!(flags & FL_NOTARGET)) {
        msg = "notarget OFF\n";
    } else {
        msg = "notarget ON\n";
    }

    gi.SendServerCommand(edict - g_entities, "print \"%s\"", msg);
}

void Player::NoclipCheat(Event *ev)
{
    const char *msg;

    if (vehicle) {
        msg = "Must exit vehicle first\n";
    } else if (getMoveType() == MOVETYPE_NOCLIP) {
        setMoveType(MOVETYPE_WALK);
        msg = "noclip OFF\n";

        ResetState(NULL);
    } else {
        client->ps.feetfalling = false;
        movecontrol            = MOVECONTROL_LEGS;

        setMoveType(MOVETYPE_NOCLIP);
        msg = "noclip ON\n";
    }

    gi.SendServerCommand(edict - g_entities, "print \"%s\"", msg);
}

void Player::GameVersion(Event *ev)
{
    gi.SendServerCommand(edict - g_entities, "print \"%s : %s\n\"", GAMEVERSION, __DATE__);
}

void Player::SetFov(float newFov, bool forced)
{
    fov = newFov;

    if (!forced && (fov != sv_defaultFov->value) && multiplayerManager.checkFlag(MP_FLAG_FIXED_FOV)) {
        fov = sv_defaultFov->value;
        return;
    }

    if (fov < 1.0f) {
        fov = sv_defaultFov->value;
    } else if (fov > 160.0f) {
        fov = 160.0f;
    }

    str tempString(fov);
    gi.cvar_set("r_fov", tempString.c_str());
}

void Player::Fov(Event *ev)
{
    CancelEventsOfType(EV_Player_Fov);
    if (ev->NumArgs() > 1) {
        Event *event = new Event(EV_Player_Fov);
        event->AddFloat(ev->GetFloat(1));
        PostEvent(event, ev->GetFloat(2));
        return;
    }

    if (ev->NumArgs() < 1) {
        gi.SendServerCommand(edict - g_entities, "print \"Fov = %d\n\"", (int)fov);
        return;
    }

    SetFov(ev->GetFloat(1));
}


float Player::CalcRoll(void)
{
    float  sign;
    float  side;
    float  value;
    Vector l;

    angles.AngleVectors(NULL, &l, NULL);
    side = velocity * l;
    sign = side < 0 ? 4 : -4;
    side = fabs(side);

    value = sv_rollangle->value;

    if (side < sv_rollspeed->value) {
        side = side * value / sv_rollspeed->value;
    } else {
        side = value;
    }

    return side * sign;
}

void Player::ProcessPmoveEvents(int event)
{
    float damage;

    switch (event) {
    case EV_NONE:
        break;
    case EV_FALL_VERY_SHORT:
    case EV_FALL_SHORT:
    case EV_FALL_MEDIUM:
    case EV_FALL_FAR:
    case EV_FALL_VERY_FAR:
    case EV_FALL_FATAL:
        damage = 0.0f;
        if (event == EV_FALL_VERY_SHORT) {
            damage = 5.0f;
        } else if (event == EV_FALL_SHORT) {
            damage = 10.0f;
        } else if (event == EV_FALL_MEDIUM) {
            damage = 25.0f;
        } else if (event == EV_FALL_FAR) {
            damage = 50.0f;
        } else if (event == EV_FALL_VERY_FAR) {
            damage = 100.0f;
        } else if (event == EV_FALL_FATAL) {
            damage = 1000.0f;
        }

        if (!multiplayerManager.checkFlag(MP_FLAG_NO_FALLING)) {
            Damage(world, world, damage, origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_FALLING);
        }
        break;
    case EV_TERMINAL_VELOCITY:
        Sound("snd_fall", CHAN_VOICE);
        break;
    case EV_WATER_LEAVE: 
        Sound("impact_playerleavewater", CHAN_AUTO);
        break;
    case EV_WATER_UNDER: 
        Sound("impact_playersubmerge", CHAN_AUTO);
        break;
    case EV_WATER_CLEAR: 
        Sound("snd_gasp", CHAN_LOCAL);
        break;
    }
}

void Player::WorldEffects(void)
{
    if (deadflag == DEAD_DEAD || getMoveType() == MOVETYPE_NOCLIP) {
        return;
    }

    if (on_fire) {
        if (next_painsound_time < level.time) {
            next_painsound_time = level.time + 4;
            Sound("snd_onfire", CHAN_LOCAL);
        }
    }

    if (watertype & CONTENTS_LAVA) {
        if (next_drown_time < level.time) {
            next_drown_time = level.time + 0.2f;
            Damage(world, world, 10.0f * waterlevel, origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_LAVA);
        }
        if (next_painsound_time < level.time) {
            next_painsound_time = level.time + 3.0f;
            Sound("snd_burned", CHAN_LOCAL);
        }
    }

    if (watertype & CONTENTS_SLIME) {
        if (next_drown_time < level.time) {
            next_drown_time = level.time + 0.4f;
            Damage(world, world, 7.0f * waterlevel, origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_SLIME);
        }
        if (next_painsound_time < level.time) {
            next_painsound_time = level.time + 5.0f;
            Sound("snd_burned", CHAN_LOCAL);
        }
    }

    if (waterlevel == 3) {
        if ((air_finished < level.time) && !(flags & FL_GODMODE)) {
            if ((next_drown_time < level.time) && (health > 0)) {
                next_drown_time = level.time + 1.0f;

                drown_damage += 2.0f;
                if (drown_damage > 15.0f) {
                    drown_damage = 15.0f;
                }

                if (health <= drown_damage) {
                    Sound("snd_drown", CHAN_LOCAL);
                } else {
                    Sound("snd_choke", CHAN_LOCAL);
                }

                Damage(world, world, drown_damage, origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_DROWN);
            }
        }
    } else {
        air_finished = level.time + 20.0f;
        drown_damage = 2.0f;
    }
}

void Player::AddBlend(float r, float g, float b, float a)
{
    float a2;
    float a3;

    if (a <= 0) {
        return;
    }

    a2 = blend[3] + (1 - blend[3]) * a;

    a3 = blend[3] / a2;

    blend[0] = blend[0] * a3 + r * (1 - a3);
    blend[1] = blend[1] * a3 + g * (1 - a3);
    blend[2] = blend[2] * a3 + b * (1 - a3);
    blend[3] = a2;
}

void Player::SetBlend(float r, float g, float b, float a, int additive)
{
    client->ps.blend[0] = r;
    client->ps.blend[1] = g;
    client->ps.blend[2] = b;
    client->ps.blend[3] = a;

    client->ps.stats[STAT_ADDFADE] = additive;
}

void Player::SetBlend(int additive)
{
    SetBlend(blend[0], blend[1], blend[2], blend[3], additive);
}

void Player::setFlash(const Vector &color, float alpha, float minTime, float maxTime)
{
    _flashBlend   = color;
    _flashAlpha   = alpha;
    _flashMinTime = level.time + minTime;
    _flashMaxTime = level.time + maxTime;
}

void Player::CalcBlend(void)
{
    int      contents;
    Vector   vieworg;
    Vector   viewModeColor;
    float    viewModeAlpha;
    qboolean viewModeAdditive;

    client->ps.stats[STAT_ADDFADE] = 0;
    blend[0] = blend[1] = blend[2] = blend[3] = 0;
    SetBlend(false);

    if (gi.GetViewModeScreenBlend(getViewMode(), viewModeColor, &viewModeAlpha, &viewModeAdditive)) {
        AddBlend(viewModeColor[0], viewModeColor[1], viewModeColor[2], viewModeAlpha);
        SetBlend(viewModeAdditive);

        return;
    }

    vieworg     = origin;
    vieworg[2] += viewheight;

    contents = gi.pointcontents(vieworg, 0);

    if (flags & FL_STUNNED) {
        AddBlend(0.0f, 0.0f, 0.5f, 0.1f);
        SetBlend(true);
    } else if (contents & CONTENTS_SOLID) {
    } else if (contents & CONTENTS_LAVA) {
        AddBlend(level.lava_color[0], level.lava_color[1], level.lava_color[2], level.lava_alpha);
        SetBlend(true);
    } else if (contents & CONTENTS_WATER) {
        AddBlend(level.water_color[0], level.water_color[1], level.water_color[2], level.water_alpha);
        SetBlend(true);
    } else if (contents & CONTENTS_SLIME) {
        AddBlend(level.slime_color[0], level.slime_color[1], level.slime_color[2], level.slime_alpha);
        SetBlend(true);
    }

    if (_doDamageScreenFlash && (damage_alpha > 0)) {
        AddBlend(damage_blend[0], damage_blend[1], damage_blend[2], damage_alpha);
        SetBlend(false);

    }


    if (level.time < _flashMaxTime) {
        float currentFlashAlpha;

        currentFlashAlpha = _flashAlpha;

        if (level.time > _flashMinTime) {
            currentFlashAlpha = _flashAlpha * (_flashMaxTime - level.time) / (_flashMaxTime - _flashMinTime);
        }

        AddBlend(_flashBlend[0], _flashBlend[1], _flashBlend[2], currentFlashAlpha);
        SetBlend(false);
    }

    float alpha = 1;

    level.m_fade_time -= level.frametime;

    if ((level.m_fade_time <= 0.0f) && (level.m_fade_type == fadein)) {
        return;
    }

    if (level.m_fade_time_start > 0.0f) {
        alpha = level.m_fade_time / level.m_fade_time_start;
    }

    if (level.m_fade_type == fadeout) {
        alpha = 1.0f - alpha;
    }

    if (alpha < 0.0f) {
        alpha = 0.0f;
    }

    if (alpha > 1.0f) {
        alpha = 1.0f;
    }

    if (level.m_fade_style == additive) {
        if (client->ps.stats[STAT_ADDFADE] == 1) {
            AddBlend(level.m_fade_color[0] * level.m_fade_alpha * alpha, level.m_fade_color[1] * level.m_fade_alpha * alpha,
                level.m_fade_color[2] * level.m_fade_alpha * alpha, level.m_fade_alpha * alpha);
            SetBlend(true);
        } else {
            SetBlend(level.m_fade_color[0] * level.m_fade_alpha * alpha, level.m_fade_color[1] * level.m_fade_alpha * alpha,
                level.m_fade_color[2] * level.m_fade_alpha * alpha, level.m_fade_alpha * alpha, true);
        }
    } else {
        SetBlend(level.m_fade_color[0], level.m_fade_color[1], level.m_fade_color[2], level.m_fade_alpha * alpha, false);
    }
}



void Player::DamageFeedback(void)
{
    if (_lastDamagedTimeFront + 0.25f >= level.time) {
        client->ps.pm_flags |= PMF_DAMAGE_FRONT;
    } else {
        client->ps.pm_flags &= ~PMF_DAMAGE_FRONT;
    }

    if (_lastDamagedTimeBack + 0.25f >= level.time) {
        client->ps.pm_flags |= PMF_DAMAGE_BACK;
    } else {
        client->ps.pm_flags &= ~PMF_DAMAGE_BACK;
    }

    if (_lastDamagedTimeLeft + 0.25f >= level.time) {
        client->ps.pm_flags |= PMF_DAMAGE_LEFT;
    } else {
        client->ps.pm_flags &= ~PMF_DAMAGE_LEFT;
    }

    if (_lastDamagedTimeRight + 0.25f >= level.time) {
        client->ps.pm_flags |= PMF_DAMAGE_RIGHT;
    } else {
        client->ps.pm_flags &= ~PMF_DAMAGE_RIGHT;
    }

    if (health <= 0.0f) {
        damage_count = 0;
        damage_blood = 0;
        damage_alpha = 0;
        VectorClear(damage_angles);
        return;
    }

    if (damage_blood > damage_count) {
        Vector delta;
        Vector damageDir;

        damageDir = damage_from;

        delta = damageDir.toAngles();

        delta[PITCH] = AngleDelta(delta[PITCH], v_angle[PITCH]);
        delta[YAW]   = AngleDelta(delta[YAW], v_angle[YAW]);
        delta[ROLL]  = AngleDelta(delta[ROLL], v_angle[ROLL]);


        delta *= 0.01f;

        damage_angles = delta;
    }

    if (damage_count > 0) {

        if (damage_count * 0.1f < 0.25f) {
            damage_count -= 0.25f;
        } else {
            damage_count *= 0.9f;
        }

        if (damage_count < 0.1f) {
            damage_count = 0;
        }
    }

    damage_count += damage_blood;

    if (damage_count <= 0.0f) {
        damage_alpha = 0.0f;
        return;
    }


    damage_alpha = damage_count / 25.0f;


    if (damage_alpha < 0.1f) {
        damage_alpha = 0.1f;
    } else if (damage_alpha > 0.6f) {
        damage_alpha = 0.6f;
    }



    damage_blood = 0;
}

void Player::GetPlayerView(Vector *pos, Vector *angle)
{
    if (pos) {
        *pos = origin;
        pos->z += viewheight;
    }

    if (angle) {
        *angle = Vector(client->ps.viewangles);
    }
}


void Player::SetupView(void)
{
    switch (playerCameraMode) {
    case PLAYER_CAMERA_MODE_NORMAL:
    case PLAYER_CAMERA_MODE_CINEMATIC:
        break;

    case PLAYER_CAMERA_MODE_ACTOR: {
        if (!entity_to_watch || entity_to_watch->deadflag) {
            DestroyActorCamera();
        } else {
            trace_t trace = G_Trace(actor_camera->origin,
                                    Vector(-5, -5, -5),
                                    Vector(5, 5, 5),
                                    actor_camera->origin,
                                    actor_camera,
                                    MASK_DEADSOLID,
                                    false,
                                    "SetupView");
            if (trace.startsolid) {
                DestroyActorCamera();
            }
        }
    } break;

    case PLAYER_CAMERA_MODE_ENTITY_WATCHING: {
        if (entity_to_watch) {
            Vector cameraPosition;
            Vector cameraAngles;
            GetPlayerView(&cameraPosition, &cameraAngles);
            Vector directionToWatchedEntity(entity_to_watch->origin - cameraPosition);
            Vector cameraForward;
            cameraAngles.AngleVectors(&cameraForward);
            const float angleToWatchedEntity = RAD2DEG(Vector::AngleBetween(directionToWatchedEntity, cameraForward));
            if (angleToWatchedEntity > maximumAngleToWatchedEntity) {
                if (watchEntityForEntireDuration) {
                    client->ps.useCameraFocalPoint = false;
                } else {
                    StopWatchingEntity();
                }
            } else {
                client->ps.useCameraFocalPoint = true;
            }
        }
    } break;

    case PLAYER_CAMERA_MODE_NO_CLIP:
        break;

    default:
        assert(false); 
    }

    if (actor_camera) {
        playerCameraMode = PLAYER_CAMERA_MODE_ACTOR;
        if (!camera) {
            AutomaticallySelectedNewCamera();
        }
    } else if (entity_to_watch) {
        playerCameraMode = PLAYER_CAMERA_MODE_ENTITY_WATCHING;
    } else if (camera) {
        playerCameraMode = PLAYER_CAMERA_MODE_CINEMATIC;
    } else if (getMoveType() == MOVETYPE_NOCLIP) {
        playerCameraMode = PLAYER_CAMERA_MODE_NO_CLIP;
    } else {
        playerCameraMode = PLAYER_CAMERA_MODE_NORMAL;
    }

    // port used to call -- does not exist in retail: the retail binary exports only the
    switch (playerCameraMode) {
    case PLAYER_CAMERA_MODE_NORMAL:
        SetClientViewAngles(origin, client->ps.viewheight, v_angle, velocity, fov);
        SetPlayerViewNormal();
        break;

    case PLAYER_CAMERA_MODE_ACTOR:
        {
            if (!actor_camera) {
                actor_camera = new Camera();
            }
            if (!camera) {
                SetCamera(actor_camera, 0.5f);
            }
            SetClientViewAngles(origin, viewheight, v_angle, velocity, camera->Fov());
            SetPlayerViewUsingActorController(camera);
        }
        break;

    case PLAYER_CAMERA_MODE_ENTITY_WATCHING:
        SetClientViewAngles(origin, client->ps.viewheight, v_angle, velocity, fov);
        SetPlayerViewUsingEntityWatchingController();
        break;

    case PLAYER_CAMERA_MODE_NO_CLIP:
        SetClientViewAngles(origin, client->ps.viewheight, v_angle, velocity, fov);
        SetPlayerViewUsingNoClipController();
        break;

    case PLAYER_CAMERA_MODE_CINEMATIC:
        SetClientViewAngles(origin, viewheight, v_angle, velocity, camera->Fov());
        SetPlayerViewUsingCinematicController(camera);
        break;

    default:
        assert(false); 
    }
}

Vector Player::GetAngleToTarget(Entity *ent, str tag, float yawclamp, float pitchclamp, Vector baseangles)
{
    assert(ent);

    if (ent) {
        Vector        delta, angs;
        orientation_t tag_or;

        int tagnum = gi.Tag_NumForName(edict->s.modelindex, tag.c_str());

        if (tagnum < 0) {
            return Vector(0, 0, 0);
        }

        GetTagPositionAndOrientation(tagnum, &tag_or);

        delta = ent->centroid - tag_or.origin;
        delta.normalize();

        angs = delta.toAngles();

        /* retail ships the plain subtract: its own AnglesSubtract line is commented
           out, so the wrap case keeps its sign instead of flipping past +-180 */
        angs -= baseangles;

        angs[PITCH] = AngleNormalize180(angs[PITCH]);
        angs[YAW]   = AngleNormalize180(angs[YAW]);

        if (angs[PITCH] > pitchclamp) {
            angs[PITCH] = pitchclamp;
        } else if (angs[PITCH] < -pitchclamp) {
            angs[PITCH] = -pitchclamp;
        }

        if (angs[YAW] > yawclamp) {
            angs[YAW] = yawclamp;
        } else if (angs[YAW] < -yawclamp) {
            angs[YAW] = -yawclamp;
        }

        return angs;
    } else {
        return Vector(0, 0, 0);
    }
}

void Player::DebugWeaponTags(int controller_tag, Weapon *weapon, str weapon_tagname)
{
    int           i;
    orientation_t bone_or, tag_weapon_or, barrel_or, final_barrel_or;

    GetTagPositionAndOrientation(edict->s.bone_tag[controller_tag], &bone_or);

    GetTagPositionAndOrientation(gi.Tag_NumForName(edict->s.modelindex, weapon_tagname), &tag_weapon_or);

    weapon->GetRawTag("tag_barrel", &barrel_or);
    VectorCopy(tag_weapon_or.origin, final_barrel_or.origin);

    for (i = 0; i < 3; i++) {
        VectorMA(final_barrel_or.origin, barrel_or.origin[i], tag_weapon_or.axis[i], final_barrel_or.origin);
    }

    MatrixMultiply(barrel_or.axis, tag_weapon_or.axis, final_barrel_or.axis);

}

Entity *Player::FindClosestEntityInRadius(const float horizontalFOVDegrees, const float verticalFOVDegrees, const float maxDistance)
{
    Vector torsoForward;
    Vector torsoRight;
    Vector torsoUp;

    torsoAngles.AngleVectors(&torsoForward, &torsoRight, &torsoUp);

    const float horizontalFOVComponent = static_cast<float>(sin(DEG2RAD(horizontalFOVDegrees / 2.0f)));
    const float verticalFOVComponent   = static_cast<float>(sin(DEG2RAD(verticalFOVDegrees / 2.0f)));

    Entity *bestEntity   = NULL;
    float   bestDistance = maxDistance;

    Entity *currentEntity = NULL;

    while ((currentEntity = findradius(currentEntity, centroid, maxDistance)) != NULL) {
        bool validEntity = false;

        if (currentEntity->flags & FL_AUTOAIM) {
            validEntity = true;
        } else if (currentEntity->isSubclassOf(Actor) && !currentEntity->deadflag) {
            Actor *actor = static_cast<Actor *>(currentEntity);

            if ((actor->actortype == IS_ENEMY) && actor->CanTarget() && !(actor->bind_info && actor->bind_info->bindmaster) && (actor->edict->s.parent == ENTITYNUM_NONE)) {
                validEntity = true;
            }
        }

        if (validEntity) {
            Vector delta(currentEntity->centroid - centroid);

            const float dist = delta.length();

            if (dist < bestDistance) {
                delta.normalize();

                const float forwardDot   = DotProduct(torsoForward, delta);
                const float horizontalDot = DotProduct(torsoRight, delta);
                const float verticalDot   = DotProduct(torsoUp, delta);

                if (
                    forwardDot > 0.0f &&
                    fabs(horizontalDot) < horizontalFOVComponent &&
                    ((fabs(verticalDot) < verticalFOVComponent) || (dist < 96.0f))
                    )
                {
                    trace_t trace;
                    trace = G_Trace(
                        centroid,
                        vec_zero,
                        vec_zero,
                        currentEntity->centroid,
                        NULL,
                        MASK_OPAQUE,
                        false,
                        "FindClosestEntityInRadius");

                    if (trace.ent || (trace.fraction == 1)) {
                        bestEntity  = currentEntity;
                        bestDistance = dist;
                    }
                }
            }
        }
    }
    return bestEntity;
}

Entity const *Player::GetTarget(void) const
{
    return targetEnemy;
}

void Player::AcquireTarget(void)
{
    targetEnemyLocked = false;

    if (GetTarget()) {
        GetTarget()->edict->s.eFlags &= ~EF_ENEMY_TARGETED;
    }


    Weapon *weapon = GetActiveWeapon(WEAPON_RIGHT);
    if (!(weapon && weapon->autoAimTargetSelectionAngle)) {
        weapon = GetActiveWeapon(WEAPON_LEFT);
    }
    if (weapon && weapon->autoAimTargetSelectionAngle) {
        Entity *newTarget = FindClosestEntityInRadius(weapon->autoAimTargetSelectionAngle, 170.0f, weapon->GetMaxRange());
        targetEnemy       = newTarget;
        if (newTarget) {
            if (FindClosestEntityInRadius(weapon->autoAimLockonAngle, 170.0f, weapon->GetMaxRange())) {
                targetEnemyLocked = true;
                targetEnemy->edict->s.eFlags |= EF_ENEMY_TARGETED;
            }
            return;
        }
    }
}

void Player::AutoAim(void)
{
    if (deadflag) {
        return;
    }

    AcquireTarget();

    if (_targetSelectedHighlight) {
        _targetSelectedHighlight->hideModel();
    }

    if (_targetLockedHighlight) {
        _targetLockedHighlight->hideModel();
    }

    EntityPtr currentHighlight = _targetSelectedHighlight;
    if (targetEnemyLocked) {
        currentHighlight = _targetLockedHighlight;
    }

    if (currentHighlight && !level.cinematic) {
        if (!GetTarget()) {
            return;
        }
        Weapon *weapon = GetActiveWeapon(WEAPON_RIGHT);
        if (!(weapon && weapon->crosshair)) {
            weapon = GetActiveWeapon(WEAPON_LEFT);
        }
        if (weapon && weapon->crosshair) {
            Vector newCrosshairLocation(GetTarget()->origin);
            currentHighlight->setOrigin(newCrosshairLocation);
            currentHighlight->showModel();
        }
    }
}


void Player::PlayerAngles(void)
{
    float    deltayaw;
    Vector   moveangles;
    float    speed;
    float    yawAngle;
    float    speedscale;
    vec3_t   temp;
    Vector   dir;
    Vector   newAimAngles;

    if (gi.Anim_Flags(edict->s.modelindex, animate->CurrentAnim(0)) & MDL_ANIM_DEFAULT_ANGLES) {
        SetControllerAngles(TORSO_TAG, vec_zero);
        setAngles(Vector(0.0f, v_angle.y, 0.0f));
        return;
    }

    headAngles.setXYZ(v_angle.x, AngleMod(v_angle.y), v_angle.z);
    torsoAngles.setXYZ(v_angle.x, AngleMod(v_angle.y), v_angle.z);

    dir   = Vector(velocity.x, velocity.y, 0.0f);
    speed = VectorNormalize(dir);

    if ((speed > 32.0f) && groundentity && (last_ucmd.forwardmove || last_ucmd.rightmove)) {
        speedscale = 3;
        yawing     = true; 
        deltayaw   = AngleSubtract(dir.toYaw(), headAngles[YAW]);
    } else {
        speedscale = 1;
        deltayaw   = 0;
    }

    if (fabs(deltayaw) > 90.0f) {
        deltayaw = AngleSubtract(deltayaw, 180.0f);
    }

    if (deltayaw > g_legclampangle->value) {
        deltayaw = g_legclampangle->value;
    } else if (deltayaw < -g_legclampangle->value) {
        deltayaw = -g_legclampangle->value;
    }

    yawAngle = headAngles[YAW] + deltayaw;

    yawing_left  = false;
    yawing_right = false;

    if (client->ps.walking && client->ps.groundPlane && (movecontrol == MOVECONTROL_LEGS) &&
        !last_ucmd.forwardmove && !last_ucmd.rightmove && (client->ps.groundTrace.plane.normal[2] < 0.95f))
    {
        float groundyaw;
        float yawdelta;

        groundyaw = (int)vectoyaw(client->ps.groundTrace.plane.normal);
        yawdelta  = anglemod(v_angle.y - groundyaw);
        yawdelta  = (float)((((int)yawdelta + 45) / 90) * 90.0f);
        angles.y  = groundyaw + yawdelta;
    } else {
        if (dont_turn_legs) {
            setAngles(Vector(0, v_angle.y, 0));
        } else {
            SwingAngles(yawAngle, g_legtolerance->value, g_legclamptolerance->value, g_legswingspeed->value * speedscale, &angles[YAW], &yawing);

            if (last_ucmd.deltaAngles[YAW] > 0.0f) {
                yawing_left = true;
            }
            if (last_ucmd.deltaAngles[YAW] < 0.0f) {
                yawing_right = true;
            }
        }
    }

    if ((deadflag == DEAD_DYING) || (deadflag == DEAD_DEAD)) {
        headAngles[PITCH] = 0.0f;
    }

    headAngles[PITCH] *= 0.65f; 

    if (headAngles[PITCH] > 180.0f) {
        torsoAngles[PITCH] = (-360.0f + headAngles[PITCH]) * bendTorsoMult;
    } else {
        torsoAngles[PITCH] = headAngles[PITCH] * bendTorsoMult;
    }

    if (_headWatchAllowed) {
        AcquireHeadTarget();
    } else {
        head_target = NULL;
    }

    if (head_target) {
        newAimAngles = GetAngleToTarget(head_target, "Bip01 Head", 40.0f, 30.0f, torsoAngles);

        headAimAngles[PITCH] = LerpAngle(headAimAngles[PITCH], newAimAngles[PITCH], 0.25f);
        headAimAngles[YAW]   = LerpAngle(headAimAngles[YAW], newAimAngles[YAW], 0.25f);

        torsoAimAngles[PITCH] = LerpAngle(torsoAimAngles[PITCH], 0.0f, 0.5f);
        torsoAimAngles[YAW]   = LerpAngle(torsoAimAngles[YAW], 0.0f, 0.5f);
    } else { 
        headAimAngles[PITCH] = LerpAngle(headAimAngles[PITCH], 0.0f, 0.5f);
        headAimAngles[YAW]   = LerpAngle(headAimAngles[YAW], 0.0f, 0.1f);

        torsoAimAngles[PITCH] = LerpAngle(torsoAimAngles[PITCH], 0.0f, 0.5f);
        torsoAimAngles[YAW]   = LerpAngle(torsoAimAngles[YAW], 0.0f, 0.5f);
    }

    {
        AnglesSubtract(headAngles, torsoAngles, temp);

        VectorAdd(temp, headAimAngles, temp);

        SetControllerAngles(HEAD_TAG, temp);

        AnglesSubtract(torsoAngles, angles, temp);

        VectorAdd(temp, torsoAimAngles, temp);

        SetControllerAngles(TORSO_TAG, temp);
    }

    setAngles(angles);

    {
        static int widget_sv[MAX_GENTITIES];

        if (WIDGET_On() && entnum < MAX_GENTITIES && WIDGET_Throttle(&widget_sv[entnum], 6, 20)) {
            vec3_t widget_velYawVec;

            VectorCopy(velocity, widget_velYawVec);

            WIDGET_A1_PROBE(
                "bot:svangles",
                "ent=%d isbot=%d sv=(%.1f,%.1f) body=%.1f v_angle=(%.1f,%.1f) "
                "ps=(%.1f,%.1f) speed=%.0f velyaw=%.1f ucmdyaw=%.1f movecontrol=%d\n",
                entnum,
                isSubclassOf(Actor) ? 1 : 0,
                edict->s.viewangles[1], edict->s.viewangles[0],
                edict->s.angles[1],
                v_angle.x, v_angle.y,
                client->ps.viewangles[0], client->ps.viewangles[1],
                VectorLength(velocity),
                (VectorLengthSquared(velocity) > 1.0f) ? vectoyaw(widget_velYawVec) : 0.0f,
                last_ucmd.angles[YAW] != 0 ? AngleMod(SHORT2ANGLE(last_ucmd.angles[YAW]) + SHORT2ANGLE(client->ps.delta_angles[YAW])) : -999.0f,
                movecontrol);
        }
    }
}

void Player::SwingAngles(float destination, float swingTolerance, float clampTolerance, float speed, float *angle, qboolean *swinging)
{
    float swing;
    float move;
    float scale;

    if (!*swinging) {
        swing = AngleSubtract(*angle, destination);
        if ((swing > swingTolerance) || (swing < -swingTolerance)) {
            *swinging = true;
            return;
        }
    }

    if (!*swinging) {
        return;
    }

    swing = AngleSubtract(destination, *angle);
    scale = fabs(swing);

    scale = 1.0f;

    if (swing >= 0.0f) {
        move = level.frametime * scale * speed;
        if (move >= swing) {
            move       = swing;
            *swinging  = false;
        }

        *angle = AngleMod(*angle + move);
    } else if (swing < 0.0f) {
        move = level.frametime * scale * -speed;
        if (move <= swing) {
            move       = swing;
            *swinging  = false;
        }
        *angle = AngleMod(*angle + move);
    }

    swing = AngleSubtract(destination, *angle);
    if (swing > clampTolerance) {
        *angle = AngleMod(destination - (clampTolerance - 1.0f));
    } else if (swing < -clampTolerance) {
        *angle = AngleMod(destination + (clampTolerance - 1.0f));
    }
}

const bool IsValidHeadTarget(const Entity &entity)
{
    if (entity.isSubclassOf(Actor) && !entity.deadflag) {
        const Actor &actor = static_cast<const Actor &>(entity);
        if ((actor.actortype == IS_ENEMY) && actor.CanTarget() && !(actor.bind_info && actor.bind_info->bindmaster) && (actor.edict->s.parent == ENTITYNUM_NONE)) {
            return true;
        }
    } else if (entity.isSubclassOf(Item)) {
        const Item &item = static_cast<const Item &>(entity);

        if (!item.GetOwner() && !item.has_been_looked_at && entity.look_at_me) {
            return true;
        }
    }
    return false;
}

Entity *Player::FindHeadTarget(const Vector &startOrigin, const Vector &viewForward, const float fovDegrees, const float maxDist)
{
    const int maximumNumberOfCandidates = 10;
    int       numberOfCandidates        = 0;
    float     validTargetRadiusSquared  = maxDist * maxDist;
    float     fovdot                    = cos(DEG2RAD(fovDegrees * 0.5f));

    Container<Entity *> possibleHeadTargets;
    finishableList.FreeObjectList();

    for (gentity_t *currentEdict = active_edicts.next; currentEdict != &active_edicts && numberOfCandidates < maximumNumberOfCandidates; currentEdict = currentEdict->next) {
        Entity &currentEntity = *currentEdict->entity;
        if (IsValidHeadTarget(currentEntity)) {
            Vector       delta               = (currentEntity.centroid) - startOrigin;
            float        lengthOfDeltaSquared = delta.lengthSquared();
            if (lengthOfDeltaSquared < validTargetRadiusSquared) {
                delta.normalize();
                float dot = DotProduct(viewForward, delta);
                if (dot > fovdot) {
                    int   insertionPoint            = 1;
                    const float distanceSquaredToCurrentEntity = Vector::DistanceSquared(startOrigin, currentEntity.centroid);
                    for (; insertionPoint < possibleHeadTargets.NumObjects(); insertionPoint++) {
                        if (distanceSquaredToCurrentEntity < Vector::DistanceSquared(startOrigin, possibleHeadTargets.ObjectAt(insertionPoint)->centroid)) {
                            break;
                        }
                    }
                    numberOfCandidates++;
                    possibleHeadTargets.InsertObjectAt(insertionPoint, &currentEntity);
                }

                if (lengthOfDeltaSquared < 6400.0f) { 
                    if (currentEntity.isSubclassOf(Actor)) {
                        Actor *act = (Actor *)&currentEntity;
                        if (act->IsFinishable()) {
                            finishableList.AddObject(act);
                        }
                    }
                }
            }
        }
    }

    Entity *closestValidEntity = NULL;

    for (int i = 1; i <= possibleHeadTargets.NumObjects(); i++) {
        Entity  *currentCandidate = possibleHeadTargets.ObjectAt(i);
        trace_t trace             = G_Trace(startOrigin, vec_zero, vec_zero, currentCandidate->centroid, NULL, MASK_OPAQUE, false, "FindHeadTarget");
        if ((trace.ent && trace.entityNum == currentCandidate->entnum) || (trace.fraction == 1.0f)) {
            closestValidEntity = currentCandidate;
            break;
        }
    }
    return closestValidEntity;
}

void Player::AcquireHeadTarget(void)
{
    vec3_t  mat[3];
    Entity *new_head_target;
    Entity *ent;
    Item   *item;


    if (targetEnemy) {
        head_target = targetEnemy;
    } else {
        AnglesToAxis(headAngles, mat);


        if ((look_at_time <= level.time) && head_target && head_target->isSubclassOf(Item)) {
            item               = (Item *)(Entity *)head_target;
            item->has_been_looked_at = true;
        }

        new_head_target = FindHeadTarget(centroid, mat[0], 160.0f, 1000.0f);

        if (!new_head_target) {
            head_target = NULL;
            return;
        }

        if (new_head_target != head_target) {

            if (head_target && head_target->isSubclassOf(Item)) {
                item               = (Item *)(Entity *)head_target;
                item->has_been_looked_at = true;
            }


            head_target = new_head_target;

            look_at_time = level.time + 5.0f;


            if (head_target && head_target->isSubclassOf(Item)) {
                ent = findradius(NULL, head_target->origin, 50.0f);

                while (ent) {
                    if (ent != head_target && ent->isSubclassOf(Item)) {
                        item                    = (Item *)ent;
                        item->has_been_looked_at = true;
                    }

                    ent = findradius(ent, head_target->origin, 50.0f);
                }
            }
        }
    }
}

void Player::HandleFinishableList()
{
    int i, j;
    for (i = 1; i <= finishableList.NumObjects(); i++) {
        Actor *act = finishableList.ObjectAt(i);
        for (j = 1; j <= finishingMoveList.NumObjects(); j++) {
            FinishingMove *fm    = finishingMoveList.ObjectAt(j);
            float          fovdot = cos(DEG2RAD(fm->coneangle * 0.5f));
            Vector         delta = (act->origin) - origin;
            if (delta.length() > fm->distance) {
                continue;
            }


            delta.z = 0.0f;
            delta.normalize();
            float dot = 0.0f;
            switch (fm->direction) {
            case 0:
                dot = DotProduct(yaw_forward, delta);
                break;
            case 1:
                dot = DotProduct(yaw_forward * -1.0f, delta);
                break;
            case 2:
                dot = DotProduct(yaw_left, delta);
                break;
            case 3:
                dot = DotProduct(yaw_left * -1.0f, delta);
                break;
            }

            if (dot > fovdot) {
                _finishActor = act;
                _finishState = fm->statename;
                return;
            }
        }
    }

    _finishActor = NULL;
    _finishState = "";
}

meansOfDeath_t Player::changetMeansOfDeath(meansOfDeath_t meansOfDeath)
{
    meansOfDeath_t realMeansOfDeath;

    realMeansOfDeath = meansOfDeath;

    if (_powerup) {
        realMeansOfDeath = _powerup->changetMeansOfDeath(realMeansOfDeath);
    }

    if (_rune) {
        realMeansOfDeath = _rune->changetMeansOfDeath(realMeansOfDeath);
    }

    if (_finishActor && _doingFinishingMove) {
        realMeansOfDeath = MOD_REDEMPTION; 
    }

    return realMeansOfDeath;
}

void Player::AdvancedMeleeAttack(weaponhand_t hand)
{
    Weapon *weapon;
    bool    critical = false;

    if (hand == WEAPON_ERROR) {
        return;
    }

    weapon = GetActiveWeapon(hand);
    if (!weapon) {
        return;
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (gpm->hasProperty("CurrentPlayer.Criticals", "value")) {
        float criticalSkill        = gpm->getFloatValue("CurrentPlayer.Criticals", "value");
        float criticalBaseChance   = gpm->getFloatValue("Criticals", "baseChance");
        float criticalChance       = criticalSkill * criticalBaseChance;
        if (G_Random() < criticalChance) {
            critical = true;
        }
    }

    weapon->AdvancedMeleeAttack("tag_swipe1", "tag_swipe2", critical);
}

void Player::MeleeDamageStart(Event *ev)
{
    Weapon      *weapon;
    weaponhand_t hand = WEAPON_RIGHT;

    if (ev->NumArgs() > 0) {
        hand = WeaponHandNameToNum(ev->GetString(1));
    }

    weapon = GetActiveWeapon(hand);
    if (!weapon) {
        return;
    }

    int tag_num = 0;
    tag_num += gi.Tag_NumForName(weapon->edict->s.modelindex, "tag_swipe1");
    tag_num += gi.Tag_NumForName(weapon->edict->s.modelindex, "tag_swipe2");
    if (tag_num < 0) {
        return;
    }

    weapon->ClearMeleeVictims();
    if (hand == WEAPON_RIGHT) {
        meleeAttackFlags |= MELEE_ATTACK_RIGHT;
    }
    if (hand == WEAPON_LEFT) {
        meleeAttackFlags |= MELEE_ATTACK_LEFT;
    }
}

void Player::MeleeDamageEnd(Event *ev)
{
    weaponhand_t hand = WEAPON_RIGHT;

    if (ev->NumArgs() > 0) {
        hand = WeaponHandNameToNum(ev->GetString(1));
    }

    if (hand == WEAPON_RIGHT) {
        meleeAttackFlags &= ~MELEE_ATTACK_RIGHT;
    }
    if (hand == WEAPON_LEFT) {
        meleeAttackFlags &= ~MELEE_ATTACK_LEFT;
    }
}


void Player::FinishMove(void)
{
    if (!(client->ps.pm_flags & PMF_FROZEN) && !(client->ps.pm_flags & PMF_NO_MOVE)) {
        origin.copyTo(client->ps.origin);
        velocity.copyTo(client->ps.velocity);
    }

    if (!(client->ps.pm_flags & PMF_FROZEN)) {
        PlayerAngles();
    }

    WorldEffects();

    DamageFeedback();
    CalcBlend();
}


void Player::UpdateStats( void )
{
	int i,count;
	float healthToDisplay;
	float armorToDisplay;
	
	if ( multiplayerManager.inMultiplayer() )
	{


		client->ps.stats[STAT_RED_TEAM_SCORE]  = multiplayerManager.getTeamPoints( "Red" );
		client->ps.stats[STAT_BLUE_TEAM_SCORE] = multiplayerManager.getTeamPoints( "Blue" );

		client->ps.stats[STAT_SCORE]  = multiplayerManager.getPoints( this );
		client->ps.stats[STAT_KILLS]  = multiplayerManager.getKills( this );
		client->ps.stats[STAT_DEATHS] = multiplayerManager.getDeaths( this );
		
		client->ps.stats[STAT_MP_GENERIC1] = multiplayerManager.getStat( this, STAT_MP_GENERIC1 );
		client->ps.stats[STAT_MP_GENERIC2] = multiplayerManager.getStat( this, STAT_MP_GENERIC2 );
		client->ps.stats[STAT_MP_GENERIC3] = multiplayerManager.getStat( this, STAT_MP_GENERIC3 );
		client->ps.stats[STAT_MP_GENERIC4] = multiplayerManager.getStat( this, STAT_MP_GENERIC4 );
		client->ps.stats[STAT_MP_GENERIC5] = multiplayerManager.getStat( this, STAT_MP_GENERIC5 );
		client->ps.stats[STAT_MP_GENERIC6] = multiplayerManager.getStat( this, STAT_MP_GENERIC6 );
		client->ps.stats[STAT_MP_GENERIC7] = multiplayerManager.getStat( this, STAT_MP_GENERIC7 );
		client->ps.stats[STAT_MP_GENERIC8] = multiplayerManager.getStat( this, STAT_MP_GENERIC8 );

		client->ps.stats[STAT_MP_SPECTATING_ENTNUM] = multiplayerManager.getStat( this, STAT_MP_SPECTATING_ENTNUM );
		
		client->ps.stats[STAT_MP_MODE_ICON]			= multiplayerManager.getIcon( this, STAT_MP_MODE_ICON );
		client->ps.stats[STAT_MP_TEAM_ICON]			= multiplayerManager.getIcon( this, STAT_MP_TEAM_ICON );
		client->ps.stats[STAT_MP_TEAMHUD_ICON]		= multiplayerManager.getIcon( this, STAT_MP_TEAMHUD_ICON );
		client->ps.stats[STAT_MP_SPECIALTY_ICON]	= multiplayerManager.getIcon( this, STAT_MP_SPECIALTY_ICON );

		client->ps.stats[STAT_MP_OTHERTEAM_ICON]	= multiplayerManager.getIcon( this, STAT_MP_OTHERTEAM_ICON );

		if ( multiplayerManager.inMultiplayer() )
			edict->s.infoIcon = multiplayerManager.getInfoIcon( this, last_ucmd.buttons );
		else
			edict->s.infoIcon = 0;

		client->ps.stats[STAT_MP_AWARD_ICON]  = multiplayerManager.getIcon( this, STAT_MP_AWARD_ICON );
		client->ps.stats[STAT_MP_AWARD_COUNT] = multiplayerManager.getStat( this, STAT_MP_AWARD_COUNT );

		client->ps.stats[STAT_MP_STATE] = multiplayerManager.getStat( this, STAT_MP_STATE );
		
		if ( _holdableItem )
			client->ps.stats[STAT_MP_HOLDABLEITEM_ICON]	= _holdableItem->getIcon();
		else
			client->ps.stats[STAT_MP_HOLDABLEITEM_ICON]	= -1;
		
		if ( _rune )
			client->ps.stats[STAT_MP_RUNE_ICON]			= _rune->getIcon();
		else
			client->ps.stats[STAT_MP_RUNE_ICON]			= -1;
		
		if ( _powerup )
			client->ps.stats[STAT_MP_POWERUP_ICON]		= _powerup->getIcon();
		else
			client->ps.stats[STAT_MP_POWERUP_ICON]		= -1;
		
		
		client->ps.stats[STAT_TIMELEFT_SECONDS] = multiplayerManager.getStat( this, STAT_TIMELEFT_SECONDS );
	}
	


	healthToDisplay = health;
	armorToDisplay = GetArmorValue();

	if ( multiplayerManager.inMultiplayer() && multiplayerManager.isPlayerSpectator( this, SPECTATOR_TYPE_FOLLOW ) )
	{
		Player *playerSpectating;

		playerSpectating = multiplayerManager.getPlayerSpectating( this );

		if ( playerSpectating )
		{

			healthToDisplay = playerSpectating->getHealth();
			armorToDisplay = playerSpectating->GetArmorValue();
		}
	}


	if ( ( healthToDisplay < 1.0f ) && ( healthToDisplay > 0.0f ) )
	{
		client->ps.stats[ STAT_HEALTH ] = 1;
	}
	else
	{
		client->ps.stats[ STAT_HEALTH ] = (int)healthToDisplay;
	}


	if ( health <= 0.0f )
	{
		Vector damageAngles = damage_from * -1;
		damageAngles = damageAngles.toAngles();
		client->ps.stats[ STAT_DEAD_YAW ] = (int)  damageAngles[ YAW ];
	}
	else
	{
		client->ps.stats[ STAT_DEAD_YAW ] = 0;
	}
	
	client->ps.stats[ STAT_ARMOR_LEVEL ] = (int) armorToDisplay;
	
	Weapon *leftweapon  = GetActiveWeapon( WEAPON_LEFT );
	Weapon *rightweapon = GetActiveWeapon( WEAPON_RIGHT );
	Weapon *dualweapon  = GetActiveWeapon( WEAPON_DUAL );
	
	client->ps.stats[STAT_AMMO_LEFT]                 = 0;
	client->ps.stats[STAT_AMMO_RIGHT]                = 0;
	client->ps.stats[STAT_CLIPAMMO_LEFT]             = 0;
	client->ps.stats[STAT_CLIPAMMO_RIGHT]            = 0;
	client->ps.stats[STAT_NUM_SHOTS_LEFT]			 = 0;
	client->ps.stats[STAT_MAX_NUM_SHOTS_LEFT]		 = 0;
	client->ps.stats[STAT_NUM_SHOTS_RIGHT]			 = 0;
	client->ps.stats[STAT_MAXAMMO_LEFT]              = 0;
	client->ps.stats[STAT_MAXAMMO_RIGHT]             = 0;
	client->ps.stats[STAT_MAXCLIPAMMO_LEFT]          = 0;
	client->ps.stats[STAT_MAXCLIPAMMO_RIGHT]         = 0;

	client->ps.stats[STAT_AMMO_TYPE1]				 = 0;
	client->ps.stats[STAT_AMMO_TYPE2]				 = 0;
	client->ps.stats[STAT_AMMO_TYPE3]				 = 0;
	client->ps.stats[STAT_AMMO_TYPE4]				 = 0;

	if ( _powerup )
		client->ps.stats[ STAT_POWERUPTIME ] = static_cast<int>( _powerup->getTimeLeft() );
	else
		client->ps.stats[ STAT_POWERUPTIME ] = 0;
	
	client->ps.stats[ STAT_ACCUMULATED_PAIN]         = (int) accumulated_pain;
	
	client->ps.activeItems[ ITEM_NAME_AMMO_LEFT		]		= -1;
	client->ps.activeItems[ ITEM_NAME_AMMO_RIGHT	]		= -1;
	client->ps.activeItems[ ITEM_NAME_WEAPON_LEFT	]		= -1;
	client->ps.activeItems[ ITEM_NAME_WEAPON_RIGHT	]		= -1;
	client->ps.activeItems[ ITEM_NAME_WEAPON_DUAL	]		= -1;
#ifndef DEDICATED	
	client->ps.stats[STAT_NUM_OBJECTIVES]			= gi.MObjective_GetNumActiveObjectives();
	client->ps.stats[STAT_COMPLETE_OBJECTIVES]		= gi.MObjective_GetNumCompleteObjectives();
	client->ps.stats[STAT_FAILED_OBJECTIVES]		= gi.MObjective_GetNumFailedObjectives();
	client->ps.stats[STAT_INCOMPLETE_OBJECTIVES]	= gi.MObjective_GetNumIncompleteObjectives();
#endif

	if ( client->ps.stats[ STAT_SHOTS_FIRED ] > 0 )
		client->ps.stats[STAT_ACCURACY] = (int)((float)( (float)client->ps.stats[ STAT_SHOTS_HIT ] / (float)client->ps.stats[ STAT_SHOTS_FIRED ] ) * 100.0f);
	else
		client->ps.stats[STAT_ACCURACY] = 100;
		
	client->ps.stats[STAT_MISSION_DURATION] = (int) level.timeInLevel;
	
	client->ps.stats[ STAT_WEAPON_GENERIC1 ] = 0;
	client->ps.stats[ STAT_WEAPON_GENERIC2 ] = 0;
	
	if ( dualweapon )
	{
		if ( dualweapon->GetClipSize( FIRE_MODE1 ) > 0 )
		{
			client->ps.stats[STAT_AMMO_LEFT]        = AmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) ) / dualweapon->GetClipSize( FIRE_MODE1 );
			client->ps.stats[STAT_MAXAMMO_LEFT]     = MaxAmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) ) / dualweapon->GetClipSize( FIRE_MODE1 );

			client->ps.stats[STAT_CLIPAMMO_LEFT]    = dualweapon->ClipAmmo( FIRE_MODE1 );
			client->ps.stats[STAT_MAXCLIPAMMO_LEFT] = dualweapon->GetClipSize( FIRE_MODE1 );

			if( dualweapon->ammorequired[FIRE_MODE1] != 0)
			{
				client->ps.stats[STAT_NUM_SHOTS_LEFT]		= dualweapon->ClipAmmo( FIRE_MODE1 ) / dualweapon->ammorequired[FIRE_MODE1]; 
				client->ps.stats[STAT_MAX_NUM_SHOTS_LEFT]	= dualweapon->GetClipSize( FIRE_MODE1 ) / dualweapon->ammorequired[FIRE_MODE1];
			}
		}
		else
		{
			client->ps.stats[STAT_AMMO_LEFT]        = AmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.stats[STAT_MAXAMMO_LEFT]     = MaxAmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) );

			client->ps.stats[STAT_CLIPAMMO_LEFT]    = AmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.stats[STAT_MAXCLIPAMMO_LEFT] = MaxAmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) );

			if( dualweapon->ammorequired[FIRE_MODE1] != 0)
			{
				client->ps.stats[STAT_NUM_SHOTS_LEFT]		= AmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) ) / dualweapon->ammorequired[FIRE_MODE1]; 
				client->ps.stats[STAT_MAX_NUM_SHOTS_LEFT]	= MaxAmmoCount( dualweapon->GetAmmoType( FIRE_MODE1 ) ) / dualweapon->ammorequired[FIRE_MODE1];
			}
			

		}


		
		client->ps.activeItems[ITEM_NAME_AMMO_LEFT]     = AmmoIndex( dualweapon->GetAmmoType( FIRE_MODE1 ) );
		
		client->ps.stats[STAT_AMMO_RIGHT]               = AmmoCount( dualweapon->GetAmmoType( FIRE_MODE2 ) );
		client->ps.stats[STAT_MAXAMMO_RIGHT]            = MaxAmmoCount( dualweapon->GetAmmoType( FIRE_MODE2 ) );
		
		if ( dualweapon->GetClipSize( FIRE_MODE2 ) > 0 )
		{
			client->ps.stats[STAT_CLIPAMMO_RIGHT]    = dualweapon->ClipAmmo( FIRE_MODE2 );
			client->ps.stats[STAT_MAXCLIPAMMO_RIGHT] = dualweapon->GetClipSize( FIRE_MODE2 );

			if(dualweapon->ammorequired[FIRE_MODE2] != 0)
				client->ps.stats[STAT_NUM_SHOTS_RIGHT]	 = dualweapon->ClipAmmo( FIRE_MODE2 ) / dualweapon->ammorequired[FIRE_MODE2]; 
		}
		else
		{
			client->ps.stats[STAT_CLIPAMMO_RIGHT]    = AmmoCount( dualweapon->GetAmmoType( FIRE_MODE2 ) );
			client->ps.stats[STAT_MAXCLIPAMMO_RIGHT] = MaxAmmoCount( dualweapon->GetAmmoType( FIRE_MODE2 ) );

			if(dualweapon->ammorequired[FIRE_MODE2] != 0)
				client->ps.stats[STAT_NUM_SHOTS_RIGHT]	 = AmmoCount( dualweapon->GetAmmoType( FIRE_MODE2 ) ) / dualweapon->ammorequired[FIRE_MODE2]; 

		}
		


		
		client->ps.activeItems[ITEM_NAME_AMMO_RIGHT]    = AmmoIndex( dualweapon->GetAmmoType( FIRE_MODE2 ) );
		
		client->ps.activeItems[ITEM_NAME_WEAPON_DUAL] = dualweapon->getIndex();


		for( int i_shadow1 = 1; i_shadow1 <= ammo_inventory.NumObjects(); i_shadow1++ )
		{
			if(ammo_inventory.ObjectAt(i_shadow1)->getName() == "Plasma")
			{
				client->ps.stats[STAT_AMMO_TYPE1] = ammo_inventory.ObjectAt(i_shadow1)->getAmount();
			}
			else if(ammo_inventory.ObjectAt(i_shadow1)->getName() == "Fed")
			{
				client->ps.stats[STAT_AMMO_TYPE2] = ammo_inventory.ObjectAt(i_shadow1)->getAmount();
			}
			else if(ammo_inventory.ObjectAt(i_shadow1)->getName() == "Idryll")
			{
				client->ps.stats[STAT_AMMO_TYPE3] = ammo_inventory.ObjectAt(i_shadow1)->getAmount();
			}
			else if( ammo_inventory.ObjectAt(i_shadow1)->getName() == "Phaser" )
			{
				client->ps.stats[STAT_AMMO_TYPE4] = ammo_inventory.ObjectAt(i_shadow1)->getAmount();
			}
		}

		client->ps.stats[ STAT_WEAPON_GENERIC1 ] = dualweapon->getStat( STAT_WEAPON_GENERIC1 );
		client->ps.stats[ STAT_WEAPON_GENERIC2 ] = dualweapon->getStat( STAT_WEAPON_GENERIC2 );
	}
	else
	{
		if ( leftweapon )
		{
			client->ps.stats[STAT_AMMO_LEFT]                = AmmoCount( leftweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.stats[STAT_MAXAMMO_LEFT]             = MaxAmmoCount( leftweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.stats[STAT_CLIPAMMO_LEFT]            = leftweapon->ClipAmmo( FIRE_MODE1 );
			client->ps.stats[STAT_MAXCLIPAMMO_LEFT]         = leftweapon->GetClipSize( FIRE_MODE1 );
			client->ps.activeItems[ITEM_NAME_AMMO_LEFT]     = AmmoIndex( leftweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.activeItems[ITEM_NAME_WEAPON_LEFT]   = leftweapon->getIndex();
			
		}
		
		if ( rightweapon )
		{
			client->ps.stats[STAT_AMMO_RIGHT]               = AmmoCount( rightweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.stats[STAT_MAXAMMO_RIGHT]            = MaxAmmoCount( rightweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.stats[STAT_CLIPAMMO_RIGHT]           = rightweapon->ClipAmmo( FIRE_MODE1 );
			client->ps.stats[STAT_MAXCLIPAMMO_RIGHT]        = rightweapon->GetClipSize( FIRE_MODE1 );
			client->ps.activeItems[ITEM_NAME_AMMO_RIGHT]    = AmmoIndex( rightweapon->GetAmmoType( FIRE_MODE1 ) );
			client->ps.activeItems[ITEM_NAME_WEAPON_RIGHT]  = rightweapon->getIndex();
			
		}
	}
	
	
	
	if ( bosshealth->value > 0.0f )
	{
		client->ps.stats[ STAT_BOSSHEALTH ] = (int)(bosshealth->value * 100.0f);
		
		if ( ( ( bosshealth->value * 100.0f ) > 0 ) && ( client->ps.stats[ STAT_BOSSHEALTH ] == 0 ) )
		{
			client->ps.stats[ STAT_BOSSHEALTH ] = 1;
		}
		
		client->ps.stats[ STAT_BOSSNAME_CONFIGINDEX ] = G_FindConfigstringIndex( bossname->string, CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true ) + CS_GENERAL_STRINGS;
	}
	else
	{
		client->ps.stats[ STAT_BOSSHEALTH ] = 0;
	}

	/* The boss bar's game half: what the cvar holds and what stat it produced.  A
	   frozen bar with this row stepping down convicts the renderer; a frozen bar
	   with this row flat convicts the game.  1 Hz, site-capped, qa_probe only. */
	if ( PROBE_On() )
	{
		static int bossstat_lastsec = -1;

		if ( level.inttime / 1000 != bossstat_lastsec )
		{
			bossstat_lastsec = level.inttime / 1000;
			PROBE( "bossstat", "t=%d cvar='%s' value=%.5f stat=%d name='%s'",
				   level.inttime, bosshealth->string, bosshealth->value,
				   client->ps.stats[ STAT_BOSSHEALTH ], bossname->string );
		}
	}

	if ( _itemText.length() > 0  )
	{
		client->ps.stats[ STAT_ITEMICON ] = _itemIcon;
		
		client->ps.stats[ STAT_ITEMTEXT ] = G_FindConfigstringIndex( _itemText.c_str(), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true ) + CS_GENERAL_STRINGS;
	}
	else
	{
		client->ps.stats[ STAT_ITEMTEXT ] = -1;
	}

	if ( _voteText.length() > 0  )
	{	
		client->ps.stats[ STAT_VOTETEXT ] = G_FindConfigstringIndex( _voteText.c_str(), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true ) + CS_GENERAL_STRINGS;
	}
	else
	{
		client->ps.stats[ STAT_VOTETEXT ] = -1;
	}
	
	if ( specialMoveEndTime > 0.0f )
	{
		float beginTime = specialMoveEndTime - specialMoveChargeTime;
		float timeElapsed = specialMoveCharge - beginTime;
		float percentElapsed = timeElapsed / specialMoveChargeTime;
		float finalValue = percentElapsed * 100.0f;
		if ( finalValue > 100.0f )
			finalValue = 100.0f;
		client->ps.stats[ STAT_SPECIALMOVETIMER ] = (int) finalValue;
	}	
	else
		client->ps.stats[ STAT_SPECIALMOVETIMER ] = 0;
	
	client->ps.stats[ STAT_POINTS ] = points;

	client->ps.stats[ STAT_SECRETS_TOTAL ] = level.total_secrets;
	client->ps.stats[ STAT_SECRETS_FOUND ] = level.found_secrets;
	client->ps.stats[ STAT_ITEMS_TOTAL ] = level.total_specialItems;
	client->ps.stats[ STAT_ITEMS_FOUND ] = level.found_specialItems;
	
	
	
	client->ps.stats[ STAT_CINEMATIC ] = 0;
	
	if ( level.cinematic )
		client->ps.stats[ STAT_CINEMATIC ] = (1<<0);
	
	if ( actor_camera )
		client->ps.stats[ STAT_CINEMATIC ] += (1<<1);
	
	memset( client->ps.inventory_name_index, 0, sizeof( client->ps.inventory_name_index ) );
	memset( client->ps.ammo_in_clip, 0, sizeof(client->ps.ammo_in_clip));
	
	count = inventory.NumObjects();
	
	if ( count > MAX_INVENTORY )
	{
		count = MAX_INVENTORY;
		warning( "Player::UpdateStats", "Max inventory exceeded\n" );
	}
	
	Weapon* weapon;
	Ammo*	ammo;
	int		ammoRequired;
	for ( i=1; i<=count; i++ )
	{
		int entnum_shadow1 = inventory.ObjectAt( i );
		Item *item = ( Item * )G_GetEntity( entnum_shadow1 );

		if ( item )
		{
			client->ps.inventory_name_index[i-1] = item->getIndex();
			{
				static int probe_n = 0;
				if (PROBE_Throttle(&probe_n, 24, 200)) {
					str probe_name = item->getName();
					PROBE(
						"inv:mirror-sv",
						"client=%d slot=%d item='%s' csindex=%d",
						client->ps.clientNum,
						i - 1,
						probe_name.c_str(),
						item->getIndex()
					);
				}
			}
			if( item->isSubclassOf(Weapon) )
			{
				weapon = (Weapon*) item;
				ammo = FindAmmoByName(weapon->GetAmmoType(FIRE_MODE1));
				if( ammo == 0)
					continue;
				client->ps.inventory_weapon_ammo_index[ i - 1 ]		= ammo->getIndex();
				
				ammoRequired = weapon->GetRequiredAmmo(FIRE_MODE1);
				client->ps.inventory_weapon_required_ammo[i - 1]	= ammoRequired;
				
				client->ps.ammo_in_clip[ i - 1 ] = weapon->getAmmoInClip(FIRE_MODE1);
			}
		}
	}
	
	memset( client->ps.ammo_amount, 0, sizeof( client->ps.ammo_amount ) );
	count = ammo_inventory.NumObjects();
	
	assert( count < MAX_AMMO );
	if ( count > MAX_AMMO )
	{
		gi.Error( ERR_DROP, "Player::UpdateStats : Exceeded MAX_AMMO\n" );
	}
	
	for ( i=1; i<=count; i++ )
	{
		Ammo *ammo_shadow1 = ammo_inventory.ObjectAt( i );

		if ( ammo_shadow1 )
		{
			client->ps.ammo_amount[i-1]     = ammo_shadow1->getAmount();
			client->ps.max_ammo_amount[i-1] = ammo_shadow1->getMaxAmount();
			client->ps.ammo_name_index[i-1] = ammo_shadow1->getIndex();
		}
	}
	
	
	if ( ( level.m_letterbox_time <= 0.0f ) && ( level.m_letterbox_dir == letterbox_in ) )
	{
		client->ps.stats[STAT_LETTERBOX] = (int)(level.m_letterbox_fraction * MAX_LETTERBOX_SIZE);
		return;
	}
	else if ( ( level.m_letterbox_time <= 0.0f ) && ( level.m_letterbox_dir == letterbox_out ) )
	{
		client->ps.stats[STAT_LETTERBOX] = 0;
		return;
	}
	
	float frac;
	
	level.m_letterbox_time -= level.frametime;
	
	frac = level.m_letterbox_time / level.m_letterbox_time_start;
	
	if ( frac > 1.0f )
		frac = 1.0f;
	if ( frac < 0.0f )
		frac = 0.0f;
	
	if ( level.m_letterbox_dir == letterbox_in )
		frac = 1.0f - frac;
	
	client->ps.stats[STAT_LETTERBOX] = (int)((float)( frac * level.m_letterbox_fraction ) * MAX_LETTERBOX_SIZE);
	
}

void Player::UpdateMusic(void)
{


    if (!g_allowActionMusic->integer) {
        action_level = 0.0f;
    }

    if (music_forced) {
        client->ps.current_music_mood  = music_current_mood;
        client->ps.fallback_music_mood = music_fallback_mood;
    } else if (action_level > 30.0f) {
        client->ps.current_music_mood  = mood_action;
        client->ps.fallback_music_mood = mood_normal;
    } else if (action_level < 15.0f) {
        client->ps.current_music_mood  = music_current_mood;
        client->ps.fallback_music_mood = music_fallback_mood;
    }

    if (action_level > 0.0f) {
        if (g_gametype->integer != GT_SINGLE_PLAYER) {
            action_level -= level.fixedframetime * 2.0f;
        } else {
            action_level -= level.fixedframetime * 15.0f;
        }

        if (action_level > 80.0f) {
            action_level = 80.0f;
        }
    } else {
        action_level = 0.0f;
    }

    if (s_debugmusic && s_debugmusic->integer) {
        gi.DPrintf("%s's action_level = %4.2f\n", client->pers.netname, action_level);
    }

    client->ps.music_volume           = music_current_volume;
    client->ps.music_volume_fade_time = music_volume_fade_time;

    client->ps.allowMusicDucking = _allowMusicDucking;
}

void Player::IncreaseActionLevel(float action_level_increase)
{
    action_level += action_level_increase;
}

int Player::AwardPoints(int numPoints)
{
    points += numPoints;

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) {
        return points;
    }

    float maxPoints  = 0.0f;
    float experience = 0.0f;

    if (gpm->hasProperty("CurrentPlayer", "maxPoints")) {
        maxPoints  = gpm->getFloatValue("CurrentPlayer", "maxPoints");
        experience = 100.0f * (points / maxPoints);
        gpm->setFloatValue("CurrentPlayer", "experience", experience);

        if (experience >= 100.0f) {
            float playerLevel = gpm->getFloatValue("CurrentPlayer", "level");
            float skillPoints = gpm->getFloatValue("CurrentPlayer", "SkillPoints");

            playerLevel += 1.0;
            skillPoints += playerLevel;
            maxPoints    = playerLevel * playerLevel * 100.0f;
            experience   = 0.0f;

            gpm->setFloatValue("CurrentPlayer", "level", playerLevel);
            gpm->setFloatValue("CurrentPlayer", "SkillPoints", skillPoints);
            gpm->setFloatValue("CurrentPlayer", "maxPoints", maxPoints);
            gpm->setFloatValue("CurrentPlayer", "experience", experience);
            points = 0;

            G_EnableWidgetOfPlayer(edict, "level_up", true);
        }
    }

    return points;
}

void Player::SetReverb(int type, float reverbLevel)
{
    reverb_type  = type;
    reverb_level = reverbLevel;
}

void Player::SetReverb(str type, float reverbLevel)
{
    reverb_type  = EAXMode_NameToNum(type);
    reverb_level = reverbLevel;
}

void Player::SetReverb(Event *ev)
{
    if (ev->NumArgs() < 2) {
        return;
    }

    SetReverb(ev->GetInteger(1), ev->GetFloat(2));
}

void Player::UpdateReverb(void)
{
    client->ps.reverb_type  = reverb_type;
    client->ps.reverb_level = reverb_level;
}

void Player::UpdateMisc(void)
{
    client->ps.pm_flags &= ~PMF_LEVELEXIT;

    if (camera && camera->IsLevelExit()) {
        client->ps.pm_flags |= PMF_LEVELEXIT;
    } else if (level.near_exit) {
        client->ps.pm_flags |= PMF_LEVELEXIT;
    }

    if (client->ps.pm_flags & PMF_LEVELEXIT) {
        if (music_current_mood != mood_success) {
            ChangeMusic("success", "normal", false);
        }
    }

    if (_updateGameFrames == true) {
        _totalGameFrames++;
    }
}

void Player::CheckReloadWeapons(void)
{
    Weapon *weap;

    weap = GetActiveWeapon(WEAPON_DUAL);

    if (weap) {
        weap->CheckReload();
    } else {
        weap = GetActiveWeapon(WEAPON_LEFT);
        if (weap) {
            weap->CheckReload();
        }

        weap = GetActiveWeapon(WEAPON_RIGHT);
        if (weap) {
            weap->CheckReload();
        }
    }
}

void Player::EndFrame(Event *ev)
{
    client->ps.dialogEntnum         = _dialogEntnum;
    client->ps.dialogSoundIndex     = _dialogSoundIndex;
    client->ps.dialogTextSoundIndex = _dialogTextSoundIndex;

    AutoAim();

    if (meleeAttackFlags & MELEE_ATTACK_LEFT) {
        AdvancedMeleeAttack(WEAPON_LEFT);
    }

    if (meleeAttackFlags & MELEE_ATTACK_RIGHT) {
        AdvancedMeleeAttack(WEAPON_RIGHT);
    }

    if (finishableList.NumObjects() > 0) {
        HandleFinishableList();
    } else {
        _doingFinishingMove = false;
        _finishActor        = NULL;
        _finishState        = "";
    }

    FinishMove();
    CheckReloadWeapons();
    UpdateStats();
    UpdateMusic();
    UpdateReverb();
    UpdateMisc();

    SetupView();

    UpdateObjectiveStatus();

    if (_powerup)
        _powerup->update(level.frametime);

    if (_rune)
        _rune->update(level.frametime);

    if (_holdableItem)
        _holdableItem->update(level.frametime);

    if (multiplayerManager.inMultiplayer() && (_lastPainShaderMod != MOD_NONE)
        && (_nextPainShaderTime < level.time)) {
        str painShader = getPainShader(_lastPainShaderMod, false);

        clearCustomShader("ArmorDeflection");

        if (painShader.length() > 0) {
            clearCustomShader(painShader);

            _lastPainShaderMod = MOD_NONE;
        }
    }

    if (needToSendAllHudsToClient()) {
        sendAllHudsToClient();
    }

    if (_needToSendBranchDialog == true && _started) {
        _needToSendBranchDialog = false;
        if (_branchDialogActor) {
            _branchDialogActor->setBranchDialog();
        }
    }

    _cameraCutThisFrame = false;
}

void Player::setBranchDialogActor(const Actor *actor)
{
    if (_branchDialogActor != 0) {
        _branchDialogActor->clearBranchDialog();
    }

    _branchDialogActor = (Actor *)actor;
}

void Player::clearBranchDialogActor(void)
{
    if (_branchDialogActor != 0) {
        _branchDialogActor->clearBranchDialog();
    }

    _branchDialogActor = 0;
}

void Player::GotKill(Event *ev)
{
}

void Player::ChangeMusic(const char *current, const char *fallback, qboolean force)
{
    int current_mood_num;
    int fallback_mood_num;

    music_forced = force;

    if (current && !strcmp(current, "normal")) {
        music_forced = qfalse;
    }

    action_level = 0;

    if (current) {
        current_mood_num = MusicMood_NameToNum(current);
        if (current_mood_num < 0) {
            gi.DPrintf("current music mood %s not found", current);
        } else {
            music_current_mood = current_mood_num;
        }
    }

    if (fallback) {
        fallback_mood_num = MusicMood_NameToNum(fallback);
        if (fallback_mood_num < 0) {
            gi.DPrintf("fallback music mood %s not found", fallback);
            fallback = NULL;
        } else {
            music_fallback_mood = fallback_mood_num;
        }
    }
}

void Player::ChangeMusicVolume(float volume, float fade_time)
{
    music_volume_fade_time = fade_time;
    music_saved_volume     = music_current_volume;
    music_current_volume   = volume;
}

void Player::RestoreMusicVolume(float fade_time)
{
    music_volume_fade_time = fade_time;
    music_current_volume   = music_saved_volume;
    music_saved_volume     = -1.0;
}


void Player::Jump(Event *ev)
{
    float maxheight;

    maxheight = ev->GetFloat(1);

    if (maxheight > 16.0f) {
        velocity[2] += sqrt(2.0f * sv_currentGravity->integer * maxheight);

        client->ps.walking = qfalse;
    }
}

void Player::JumpXY(Event *ev)
{
    float forwardmove;
    float sidemove;
    float distance;
    float time;
    float speed;

    if (vehicle) {
        return;
    }

    forwardmove = ev->GetFloat(1);
    sidemove    = ev->GetFloat(2);
    speed       = ev->GetFloat(3);

    velocity = yaw_forward * forwardmove - yaw_left * sidemove;
    distance = velocity.length();
    velocity *= speed / distance;
    time        = distance / speed;
    velocity[2] = sv_currentGravity->integer * time * 0.5f;

    if (client->ps.groundEntityNum != ENTITYNUM_NONE) {
        velocity += G_GetEntity(client->ps.groundEntityNum)->velocity;
    }

    airspeed = distance;

    client->ps.walking = qfalse;
}

void Player::SetViewAngles(Vector newViewangles)
{
    client->ps.delta_angles[0] = ANGLE2SHORT(newViewangles.x - client->cmd_angles[0]);
    client->ps.delta_angles[1] = ANGLE2SHORT(newViewangles.y - client->cmd_angles[1]);
    client->ps.delta_angles[2] = ANGLE2SHORT(newViewangles.z - client->cmd_angles[2]);

    v_angle = newViewangles;


    newViewangles.x = angles.x;
    newViewangles.z = angles.z;
    AnglesToMat(newViewangles, orientation);
    yaw_forward = orientation[0];
    yaw_left    = orientation[1];
}


void Player::DumpState(Event *ev)
{
    gi.DPrintf(
        "Legs: %s Torso: %s\n", currentState_Legs ? currentState_Legs->getName() : "NULL", currentState_Torso->getName()
    );
}

void Player::ForceTorsoState(Event *ev)
{
    State *ts = statemap_Torso->FindState(ev->GetString(1));
    EvaluateState(ts);
}


void Player::TouchedUseAnim(Entity *ent)
{
    toucheduseanim = ent;
}

void Player::NextPainTime(Event *ev)
{
    nextpaintime = level.time + ev->GetFloat(1);
    pain_type    = MOD_NONE;
    pain         = 0;
}

void Player::EnterVehicle(Event *ev)
{
    Entity *ent;

    ent = ev->GetEntity(1);
    if (ent && ent->isSubclassOf(Vehicle)) {
        flags |= FL_PARTIAL_IMMOBILE;
        viewheight = STAND_EYE_HEIGHT;
        velocity   = vec_zero;
        vehicle = (Vehicle *)ent;
        if (vehicle->IsDrivable()) {
            setMoveType(MOVETYPE_VEHICLE);
        } else {
            setMoveType(MOVETYPE_NOCLIP);
        }
    }
}

//     (SDK vehicle.cpp:746-752), and retail deliberately leaves a NON-drivable vehicle's
void Player::ExitVehicle(Event *ev)
{
    flags &= ~FL_PARTIAL_IMMOBILE;
    setMoveType(MOVETYPE_WALK);
    vehicle = NULL;
}

void Player::HolsterToggle(Event *ev)
{
    if (WeaponsOut()) {
        Holster(qtrue);
    } else {
        Holster(qfalse);
    }
}

void Player::Holster(qboolean putaway)
{
    Weapon *leftWeap, *rightWeap, *dualWeap;

    leftWeap  = GetActiveWeapon(WEAPON_LEFT);
    rightWeap = GetActiveWeapon(WEAPON_RIGHT);
    dualWeap  = GetActiveWeapon(WEAPON_DUAL);

    if (leftWeap || rightWeap || dualWeap) {
        if (putaway) {
            if (leftWeap) {
                leftWeap->SetPutAway(true);
                holsteredWeapons[WEAPON_LEFT] = leftWeap;
            }
            if (rightWeap) {
                rightWeap->SetPutAway(true);
                holsteredWeapons[WEAPON_RIGHT] = rightWeap;
            }
            if (dualWeap) {
                dualWeap->SetPutAway(true);
                holsteredWeapons[WEAPON_DUAL] = dualWeap;
            }

            levelVars.SetVariable("holster_active", 1);
        }
    } else {
        if (!putaway) {
            if (holsteredWeapons[WEAPON_DUAL]) {
                useWeapon(holsteredWeapons[WEAPON_DUAL], WEAPON_DUAL);
            } else if (holsteredWeapons[WEAPON_LEFT] && holsteredWeapons[WEAPON_RIGHT]) {
                Event *ev1;

                ev1 = new Event(EV_Player_DualWield);
                ev1->AddString(holsteredWeapons[WEAPON_LEFT]->getName());
                ev1->AddString(holsteredWeapons[WEAPON_RIGHT]->getName());
                ProcessEvent(ev1);
            } else if (holsteredWeapons[WEAPON_RIGHT]) {
                useWeapon(holsteredWeapons[WEAPON_RIGHT], WEAPON_RIGHT);
            } else if (holsteredWeapons[WEAPON_LEFT]) {
                useWeapon(holsteredWeapons[WEAPON_LEFT], WEAPON_LEFT);
            }

            holsteredWeapons[WEAPON_LEFT]  = NULL;
            holsteredWeapons[WEAPON_RIGHT] = NULL;
            holsteredWeapons[WEAPON_DUAL]  = NULL;

            levelVars.SetVariable("holster_active", 0);
        }
    }
}

void Player::SafeHolster(qboolean putaway)
{
    if (WeaponsOut()) {
        if (putaway) {
            weapons_holstered_by_code = qtrue;
            Holster(qtrue);
        }
    } else {
        if (putaway) {
            if (!fakePlayer_active) {
                WeaponsHolstered();
            }
        } else {
            if (weapons_holstered_by_code) {
                weapons_holstered_by_code = qfalse;
                Holster(qfalse);
            }
        }
    }
}


void Player::Holster(Event *ev)
{
    SafeHolster(ev->GetBoolean(1));
}

void Player::setAngles(Vector ang)
{
    Entity::setAngles(ang);

    ang[YAW] = v_angle[YAW];
    AnglesToMat(ang, orientation);
    yaw_forward = orientation[0];
    yaw_left    = orientation[1];
}

painDirection_t Player::Pain_string_to_int(str painName)
{
    if (!painName.icmp(painName, "Front")) {
        return PAIN_FRONT;
    } else if (!painName.icmp(painName, "Left")) {
        return PAIN_LEFT;
    } else if (!painName.icmp(painName, "Right")) {
        return PAIN_RIGHT;
    } else if (!painName.icmp(painName, "Rear")) {
        return PAIN_REAR;
    } else {
        return PAIN_NONE;
    }
}

void Player::ArchivePersistantData(Archiver& arc, qboolean sublevelTransition)
{
    str  model_name;
    str  name;
    bool anglesArchived;

    Sentient::ArchivePersistantData(arc, sublevelTransition);

    model_name = g_playermodel->string;

    arc.ArchiveString(&model_name);

    if (arc.Loading()) {
        gi.cvar_set("g_playermodel", model_name.c_str());

        if (strchr(model_name.c_str(), '/')) {
            setModel("models/" + model_name + ".tik");
        } else {
            setModel("models/char/" + model_name + ".tik");
        }
    }

    for (int hw = 0; hw < MAX_ACTIVE_WEAPONS; hw++) {
        str hname;

        if (arc.Saving()) {
            hname = holsteredWeapons[hw] ? holsteredWeapons[hw]->getName() : str("none");
        }

        arc.ArchiveString(&hname);

        if (arc.Loading() && hname != "none") {
            holsteredWeapons[hw] = (Weapon *)FindItem(hname);
        }
    }


    if (arc.Saving()) {
        if (sublevelTransition && level._saveOrientation) {
            anglesArchived = true;
            arc.ArchiveBool(&anglesArchived);
            arc.ArchiveVector(&angles);
            arc.ArchiveVector(&v_angle);
        } else {
            anglesArchived = false;
            arc.ArchiveBool(&anglesArchived);
        }
    } else {
        arc.ArchiveBool(&anglesArchived);

        if (anglesArchived) {
            arc.ArchiveVector(&angles);
            arc.ArchiveVector(&v_angle);

            setAngles(angles);
            SetViewAngles(v_angle);
        }
    }

    LoadStateTable();

    arc.ArchiveInteger(&_totalGameFrames);
    _updateGameFrames = true;

    arc.ArchiveInteger(&_secretsFound);

    arc.ArchiveInteger(&_skillLevel);

    arc.ArchiveRaw(client->ps.stats, sizeof(client->ps.stats));
}

// NOTE (faithful, do not "fix"): by the time this runs, G_ExitLevel has already blanked
void Player::LevelCleanup(void)
{
    if (!client) {
        return;
    }

    if (!gi.areSublevels(level.mapname.c_str(), level.nextmap.c_str())) {
        client->ps.stats[STAT_ENEMIES_KILLED]   = 0;
        client->ps.stats[STAT_TEAMMATES_KILLED] = 0;
        client->ps.stats[STAT_SHOTS_FIRED]      = 0;
        client->ps.stats[STAT_SHOTS_HIT]        = 0;
        client->ps.stats[STAT_MISSION_DURATION] = 0;
    }

    client->ps.pm_flags &= ~PMF_NIGHTVISION;
}

void Player::incrementSecretsFound(void)
{
    _secretsFound++;

    if (_secretsFound > g_secretCount->integer) {
        gi.cvar_set("g_secretCount", va("%d", _secretsFound));
    }
}

void Player::setSkill(int skillValue)
{
    _skillLevel = skillValue;

    if (_skillLevel < 0) {
        _skillLevel = 0;
    } else if (_skillLevel > 3) {
        _skillLevel = 3;
    }
}

void Player::PlayerDone(Event *ev)
{
    Director.PlayerSpawned();
}

const str Player::getGameplayAnim(const str& objname)
{
    if (!objname.length() || !getArchetype().length()) {
        return "";
    }

    GameplayManager *gpm     = GameplayManager::getTheGameplayManager();
    str              scopestr = getArchetype() + "." + objname;
    if (!gpm->hasObject(scopestr)) {
        return "";
    }

    char tmpstr[6];
    sprintf(tmpstr, "%d", _gameplayAnimIdx);
    return gpm->getStringValue(scopestr, tmpstr);
}

void Player::VelocityModified(void)
{
    if (velocity.z > 32.0f) {
        do_rise = true;
    }
}

int Player::GetKnockback(int original_knockback, qboolean blocked)
{
    int new_knockback;

    new_knockback = original_knockback;

    if (blocked) {
        if (LargeShieldActive()) {
            new_knockback -= 150;
        } else {
            new_knockback -= 50;
        }
    }

    if (new_knockback >= 200 && take_pain) {
        knockdown = true;

        if (blocked) {
            float damage;

            damage = new_knockback / 50;

            if (damage > 10) {
                damage = 10;
            }

            Damage(world, world, damage, origin, vec_zero, vec_zero, 0, DAMAGE_NO_ARMOR, MOD_CRUSH);
        }
    }


    if (new_knockback < 0) {
        new_knockback = 0;
    }

    return new_knockback;
}

void Player::ResetHaveItem(Event *ev)
{
    str             fullname;
    ScriptVariable *var;

    fullname = str("playeritem_") + ev->GetString(1);

    var = gameVars.GetVariable(fullname.c_str());

    if (var) {
        var->setIntValue(0);
    }
}

void Player::ReceivedItem(Item *item)
{
    if (!item) {
        return;
    }

    // ⚠️ DIVERGENCE, inherited not introduced: retail reaches ReceivedItem from exactly one
    // restore call giveItem() directly and do not come through here.
    if (item->isSubclassOf(Weapon)) {
        setItemText(item->getIcon(), va("$$PickedUpThe$$ $$Weapon-%s$$\n", item->getName().c_str()));
    } else if (item->getAmount() > 1) {
        if (item->getName() == "health") { 
            setItemText(
                item->getIcon(), va("$$PickedUp$$ %d $$Item-%s$$\n", (int)item->getAmount(), item->getName().c_str())
            );
        } else {
            setItemText(
                item->getIcon(), va("$$PickedUp$$ %d $$Item-%s$$s\n", (int)item->getAmount(), item->getName().c_str())
            );
        }
    } else {
        setItemText(item->getIcon(), va("$$PickedUpThe$$ $$Item-%s$$\n", item->getName().c_str()));
    }

    {
        str fullname = str("playeritem_") + item->getName();
        qboolean first = qtrue;
        ScriptVariable *var;

        var = gameVars.GetVariable(fullname.c_str());
        if (!var) {
            gameVars.SetVariable(fullname.c_str(), 1);
        } else {
            int amount;

            amount = var->intValue() + 1;
            var->setIntValue(amount);
            if (amount > 1) {
                first = qfalse;
            }
        }

        var = levelVars.GetVariable(fullname.c_str());
        if (!var) {
            levelVars.SetVariable(fullname.c_str(), 1);
        } else {
            int amount;

            amount = var->intValue() + 1;
            var->setIntValue(amount);
        }

        qboolean forced;
        str      dialog;
        str      anim;

        if (item->IsItemCool(&dialog, &anim, &forced)) {
            if (first || forced) {
                cool_item   = item;
                cool_dialog = dialog;
                cool_anim   = anim;
            }
        }
    }

    // NULL guard in usePlayer, so retail survives only because retail NEVER posts EV_Use
    // preserved without the invented post.  Deleted, not guarded: a NULL check in
}

void Player::RemovedItem(Item *item)
{
    str             fullname;
    ScriptVariable *var;

    fullname = str("playeritem_") + item->getName();

    var = levelVars.GetVariable(fullname.c_str());
    if (var) {
        int amount;

        amount = var->intValue() - 1;
        if (amount < 0) {
            amount = 0;
        }
        var->setIntValue(amount);
    }

    var = gameVars.GetVariable(fullname.c_str());
    if (var) {
        int amount;

        amount = var->intValue() - 1;
        if (amount < 0) {
            amount = 0;
        }
        var->setIntValue(amount);
    }
}

void Player::loadUseItem(const str& item)
{
    Event *event;
    str    itemName;

    itemName = item;

    if (stricmp(item.c_str(), "tricorder") == 0) {
        if (HasItem("Tricorder-stx")) {
            itemName = "Tricorder-stx";
        } else if (HasItem("Tricorder-rom")) {
            itemName = "Tricorder-rom";
        }
    }

    if (itemName.length()) {
        event = new Event(EV_Player_UseItem);
        event->AddString(itemName);
        ProcessEvent(event);
    }
}

void Player::AmmoAmountChanged(Ammo *ammo, int ammo_in_clip)
{
    str             fullname;
    ScriptVariable *var;

    fullname = str("playerammo_") + ammo->getName();

    var = levelVars.GetVariable(fullname.c_str());
    if (!var) {
        levelVars.SetVariable(fullname.c_str(), ammo->getAmount() + ammo_in_clip);
    } else {
        var->setIntValue(ammo->getAmount() + ammo_in_clip);
    }
}

void Player::WaitForState(Event *ev)
{
    waitForState = ev->GetString(1);
}

void Player::SetDamageMultiplier(Event *ev)
{
    damage_multiplier = ev->GetFloat(1);
}

void Player::SetTakePain(Event *ev)
{
    take_pain = ev->GetBoolean(1);
}

void Player::Loaded(void)
{
}

void Player::PlayerShowModel(Event *ev)
{
    Entity::showModel();
}

void Player::showModel(void)
{
    Entity::showModel();
}


Vector Player::getWeaponViewShake(void)
{
    Vector  viewShake;
    Weapon *weapon;

    weapon = GetActiveWeapon(WEAPON_DUAL);

    if (weapon) {
        viewShake = weapon->getViewShake();
    } else {
        weapon = GetActiveWeapon(WEAPON_LEFT);

        if (weapon) {
            viewShake = weapon->getViewShake();
        }

        weapon = GetActiveWeapon(WEAPON_RIGHT);

        if (weapon) {
            viewShake += weapon->getViewShake();
        }
    }

    return viewShake;
}



void Player::Score(Event *ev)
{
    multiplayerManager.score(this);
}












void Player::WarpToPoint(Entity *spawnpoint)
{
    if (spawnpoint) {
        NoLerpThisFrame();
        setOrigin(spawnpoint->origin + Vector(0, 0, 1));
        origin.copyTo(edict->s.origin2);
        setAngles(spawnpoint->angles);
        SetViewAngles(angles);
        CameraCut(); 

        oldvelocity = vec_zero;
        velocity    = vec_zero;
    }
}



void Player::GibEvent(Event *ev)
{
    qboolean hidemodel;

    hidemodel = !ev->GetInteger(1);

    if (com_blood->integer) {
        if (hidemodel) {
            gibbed     = true;
            takedamage = DAMAGE_NO;
            setSolidType(SOLID_NOT);
            hideModel();
        }

        CreateGibs(this, health, 0.75f, 3);
    }
}

void Player::ArmorDamage(Event *ev)
{
    float oldHealth;

    ::Damage damage(ev);

    if (multiplayerManager.inMultiplayer() && !multiplayerManager.isFightingAllowed()) {
        return;
    }

    if (multiplayerManager.inMultiplayer() && multiplayerManager.isPlayerSpectator(this)) {
        return;
    }

    if (shield_active) {
        Weapon *weapon = GetActiveWeapon(WEAPON_RIGHT);

        if (!weapon) {
            return;
        }

        if (damage.attacker) {
            Vector attack_angle = damage.attacker->angles;
            float  yaw_diff     = AngleNormalize180(angles[YAW] - attack_angle[YAW] + 180.0f);

            if ((yaw_diff > -45.0f) && (yaw_diff < 45.0f)) {
                int tagnum = gi.Tag_NumForName(weapon->edict->s.modelindex, "tag_swipe1");

                if (tagnum >= 0) {
                    Vector pos, pos2, sparkpos;

                    weapon->GetActorMuzzlePosition(&pos, NULL, NULL, NULL, "tag_swipe1");
                    weapon->GetActorMuzzlePosition(&pos2, NULL, NULL, NULL, "tag_swipe2");
                    sparkpos = (pos + pos2) / 2.0f; 
                    WeaponEffectsAndSound(weapon, "Parry", sparkpos);
                }

                if (damage.attacker->isSubclassOf(Sentient)) {
                    Sentient *sent = (Sentient *)damage.attacker;
                    sent->SetAttackBlocked(true);
                }

                return;
            }
        }
    }

    if (_powerup) {
        damage.damage = _powerup->getDamageTaken(damage.attacker, damage.damage, damage.meansofdeath);
    }

    if (_rune) {
        damage.damage = _rune->getDamageTaken(damage.attacker, damage.damage, damage.meansofdeath);
    }

    if (multiplayerManager.inMultiplayer() && damage.attacker && damage.attacker->isSubclassOf(Player)) {
        Player *attackingPlayer = (Player *)damage.attacker;

        damage.damage    = multiplayerManager.playerDamaged(this, attackingPlayer, damage.damage, damage.meansofdeath);
        damage.knockback = (int)multiplayerManager.getModifiedKnockback(this, attackingPlayer, (float)damage.knockback);
    }

    // multiplier, exactly as retail spells it: only in single player, not for MOD_FALLING.
    if (!multiplayerManager.inMultiplayer() && (damage.meansofdeath != MOD_FALLING)) {
        GameplayManager *gpm;
        float           damageMultiplier;
        int             skillLevel;

        skillLevel = getSkill();

        gpm = GameplayManager::getTheGameplayManager();

        if (gpm->hasObject("SkillLevel-PlayerDamage")) {
            if (skillLevel == 0) {
                damageMultiplier = gpm->getFloatValue("SkillLevel-PlayerDamage", "Easy");
            } else if (skillLevel == 1) {
                damageMultiplier = gpm->getFloatValue("SkillLevel-PlayerDamage", "Normal");
            } else if (skillLevel == 2) {
                damageMultiplier = gpm->getFloatValue("SkillLevel-PlayerDamage", "Hard");
            } else {
                damageMultiplier = gpm->getFloatValue("SkillLevel-PlayerDamage", "VeryHard");
            }

            damage.damage *= damageMultiplier;
        }
    }

    oldHealth = health;

    Sentient::ArmorDamage(damage);

    if (multiplayerManager.inMultiplayer()) {
        float damageTaken = oldHealth - health;

        if (damageTaken > 0.0f) {
            if (damage.meansofdeath > MOD_LAST_SELF_INFLICTED) {
                IncreaseActionLevel(damageTaken);
            }

            if (damage.attacker && damage.attacker->isSubclassOf(Player)) {
                Player *attackingPlayer = (Player *)damage.attacker;

                multiplayerManager.playerTookDamage(this, attackingPlayer, damageTaken, damage.meansofdeath);

                if (attackingPlayer != this) {
                    attackingPlayer->IncreaseActionLevel(damageTaken);
                }
            }
        }
    }

    if (multiplayerManager.inMultiplayer() && (health <= 0.0f) && damage.inflictor
        && damage.inflictor->isSubclassOf(Projectile)) {
        Gib();
    }
}

void Player::Disconnect(void)
{
    if (multiplayerManager.inMultiplayer()) {
        multiplayerManager.removePlayer(this);
    }
}

void Player::DeadBody(Event *ev)
{
    Body *body;
    int   surfaceNum;

    if ((pain_type == MOD_VAPORIZE) || (pain_type == MOD_VAPORIZE_COMP) ||
        (pain_type == MOD_VAPORIZE_DISRUPTOR) || (pain_type == MOD_VAPORIZE_PHOTON) || (pain_type == MOD_SNIPER)) {
        return;
    }

    body = new Body;

    if (gibbed) {
        return;
    }

    body->setModel(model);
    body->ProcessInitCommands();
    body->edict->s.anim        = edict->s.anim;
    body->edict->s.frame       = edict->s.frame;

    body->edict->s.torso_anim  = edict->s.torso_anim;
    body->edict->s.torso_frame = edict->s.torso_frame;
    body->edict->s.scale       = edict->s.scale;

    body->setOrigin(origin);
    body->setAngles(angles);

    int numsurfaces_local = gi.NumSurfaces(edict->s.modelindex);
    for (surfaceNum = 0; surfaceNum < numsurfaces_local; surfaceNum++) {
        body->edict->s.surfaces[surfaceNum] = edict->s.surfaces[surfaceNum];
    }
}










void Player::SetMouthAngle(Event *ev)
{
    int    tag_num;
    float  angle_percent;
    Vector mouth_angles;

    angle_percent = ev->GetFloat(1);

    if (angle_percent < 0) {
        angle_percent = 0;
    }

    if (angle_percent > 1) {
        angle_percent = 1;
    }

    tag_num = gi.Tag_NumForName(edict->s.modelindex, "tag_mouth");

    if (tag_num != -1) {
        SetControllerTag(MOUTH_TAG, tag_num);

        mouth_angles        = vec_zero;
        mouth_angles[PITCH] = max_mouth_angle * angle_percent;

        SetControllerAngles(MOUTH_TAG, mouth_angles);
    }
}





void Player::FireWeapon(Event *ev)
{
    Sentient::FireWeapon(ev);
}


























void Player::SetStateFile(Event *ev)
{
    str stateFileName(ev->GetString(1));
    gi.cvar_set("g_statefile", stateFileName);
    LoadStateTable();
}





void Player::setBackpackAttachOffset(Event *ev)
{
    _backpackAttachOffset = ev->GetVector(1);
}

void Player::setBackpackAttachAngles(Event *ev)
{
    _backpackAttachAngles = ev->GetVector(1);
}

void Player::setFlagAttachOffset(Event *ev)
{
    _flagAttachOffset = ev->GetVector(1);
}

void Player::setFlagAttachAngles(Event *ev)
{
    _flagAttachAngles = ev->GetVector(1);
}

Vector Player::getBackpackAttachOffset(void)
{
    return _backpackAttachOffset;
}

Vector Player::getBackpackAttachAngles(void)
{
    return _backpackAttachAngles;
}

Vector Player::getFlagAttachOffset(void)
{
    return _flagAttachOffset;
}

Vector Player::getFlagAttachAngles(void)
{
    return _flagAttachAngles;
}

static const char *const playerStatNames[] = {
    "health",           
    "dead_yaw",         
    "ammo_left",        
    "clipammo_left",    
    "num_shots_left",   
    "max_num_shots_left", 
    "ammo_right",       
    "clipammo_right",   
    "num_shots_right",  
    "armor_level",      
    "maxammo_left",     
    "maxammo_right",    
    "maxclipammo_left", 
    "maxclipammo_right", 
    "ammo_type1",       
    "ammo_type2",       
    "ammo_type3",       
    "ammo_type4",       
    "last_pain",        
    "accumulated_pain", 
    "bosshealth",       
    "bossnameIndex",    
    "cinematic",        
    "addfade",          
    "letterbox",        
    "poweruptime",      
    "weapon_generic1",  
    "weapon_generic2",  
    "itemicon",         
    "itemtext",         
    "votetext",         
    "enemies_killed",   
    "teammates_killed", 
    "shots_fired",      
    "shots_hit",        
    "accuracy",         
    "mission_duration", 
    "generic",          
    "num_objectives",   
    "complete_objectives",   
    "failed_objectives",     
    "incomplete_objectives", 
    "specialmovetimer", 
    "points",           
    "secretstotal",     
    "secretsfound",     
    "itemstotal",       
    "itemsfound",       
    "redTeamScore",     
    "blueTeamScore",    
    "team",             
    "score",            
    "kills",            
    "deaths",           
    "timeleft_seconds", 
    "won_matches",      
    "lost_matches",     
    "mp_generic1",      
    "mp_generic2",      
    "mp_generic3",      
    "mp_generic4",      
    "mp_generic5",      
    "mp_generic6",      
    "mp_generic7",      
    "mp_generic8",      
    "mp_spectating_entnum", 
    "mp_mode_icon",     
    "mp_team_icon",     
    "mp_teamhud_icon",  
    "mp_otherteam_icon", 
    "mp_specialty_icon", 
    "mp_holdableitem_icon", 
    "mp_rune_icon",     
    "mp_powerup_icon",  
    "mp_award_icon",    
    "mp_award_count",   
    "mp_state"          
};

#define NUM_PLAYER_STAT_NAMES ((int)(sizeof(playerStatNames) / sizeof(playerStatNames[0])))

int PlayerStat_NameToNum(const char *name)    

{
    int i;

    if (!name) {
        return -1;
    }

    for (i = 0; i < NUM_PLAYER_STAT_NAMES; i++) {
        if (Q_stricmp(name, playerStatNames[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void Player::SetStat(Event *ev)
{
    int stat_index;
    int stat_value;

    stat_index = PlayerStat_NameToNum(ev->GetString(1));

    if (stat_index < 0) {
        gi.Printf("Couldn't find player stat %s\n", ev->GetString(1));
        return;
    }

    stat_value = ev->GetInteger(2);

    client->ps.stats[stat_index] = stat_value;
}

void Player::addRosterTeammate1(Event *ev) {}
void Player::addRosterTeammate2(Event *ev) {}
void Player::addRosterTeammate3(Event *ev) {}
void Player::addRosterTeammate4(Event *ev) {}
void Player::removeRosterTeammate3(Event *ev) {}

void Player::SetViewAnglesEvent(Event *ev)
{
    if (ev->NumArgs() > 0) {
        SetViewAngles(ev->GetVector(1));
    }
}

void Player::UseSpecifiedEntity(Event *ev)
{
    Event  *event = NULL;
    Entity *ent   = NULL;

    if (ev->NumArgs() > 0) {
        ent = ev->GetEntity(1);
        if (ent == NULL) {
            gi.Error(ERR_DROP, "UseSpecifiedEntity(): NULL entity referenced\n");
            return;
        }

        event = new Event(EV_Use);
        event->AddEntity(this);
        ent->ProcessEvent(event);
        return;
    } else 
    {
        if (!atobject) {
            return;
        }

        return;
    }
}

void Player::disableInventory(void)
{
    client->ps.pm_flags |= PMF_NO_WEAPONBAR;
}

void Player::enableInventory(void)
{
    client->ps.pm_flags &= ~PMF_NO_WEAPONBAR;
}

// EF2 subsystems that are not ported, so those two steps are absent.  Single-player retail
void Player::loadObjectives(const str& objectiveName)
{
    _objectiveStates   = 0;
    _informationStates = 0;

    gi.MObjective_Update(objectiveName.c_str());

    _objectiveNameIndex = gi.objectivenameindex(objectiveName.c_str());
}

float Player::getDamageDone(float damage, int meansOfDeath, bool inMelee)
{
    float damageDone;

    damageDone = damage;

    if (inMelee) {
        damageDone *= damage_multiplier;
    }

    if (_powerup) {
        damageDone = _powerup->getDamageDone(damageDone, meansOfDeath);
    }

    if (_rune) {
        damageDone = _rune->getDamageDone(damageDone, meansOfDeath);
    }

    return damageDone;
}

void Player::shotFired(void)
{
    if (p_heuristics) {
        p_heuristics->IncrementShotsFired();
    }

    ++client->ps.stats[STAT_SHOTS_FIRED];
}

void Player::shotHit(void)
{
    if (p_heuristics) {
        p_heuristics->IncrementShotsHit();
    }

    ++client->ps.stats[STAT_SHOTS_HIT];
}

bool Player::isButtonDown(int button)
{
    if (last_ucmd.buttons & button) {
        return true;
    } else {
        return false;
    }
}

void Player::EndAnim_Legs(Event *ev)
{
    if (vehicle) {
        return;
    }

    animdone_Legs = qtrue;
    animate->SetAnimDoneEvent(EV_Player_AnimLoop_Legs, legs);
    EvaluateState();
}

void Player::EndAnim_Torso(Event *ev)
{
    if (vehicle) {
        return;
    }

    animdone_Torso = qtrue;
    animate->SetAnimDoneEvent(EV_Player_AnimLoop_Torso, torso);
    EvaluateState();
}


void Player::AutomaticallySelectedNewCamera(void)
{
    float  bestScore  = 999;
    Camera *bestCamera = NULL;

    for (int i = 1; i <= level.automatic_cameras.NumObjects(); i++) {
        Camera *cam   = level.automatic_cameras.ObjectAt(i);
        float   score = cam->CalculateScore(this, currentState_Torso->getName());

        if (cam == camera) {
            score *= 0.9f;
        }

        if (score < bestScore) {
            bestScore  = score;
            bestCamera = cam;
        }
    }

    if (bestScore <= 1.0f) {
        if (bestCamera != camera) {
            if (camera) {
                camera->AutomaticStop(this);
            }
            SetCamera(bestCamera, bestCamera->AutomaticStart(this));
        }
    } else {
        if (camera) {
            SetCamera(NULL, camera->AutomaticStop(this));
        }
    }
}

void Player::DestroyActorCamera(void)
{
    if (actor_camera) {
        delete actor_camera;
        actor_camera = NULL;
        SetCamera(NULL, 0.5f);
    }
}


qboolean Player::checkforward(Conditional &condition)
{
    return last_ucmd.forwardmove > 0;
}

qboolean Player::checkbackward(Conditional &condition)
{
    return last_ucmd.forwardmove < 0;
}

qboolean Player::checkstrafeleft(Conditional &condition)
{
    return last_ucmd.rightmove < 0;
}

qboolean Player::checkstraferight(Conditional &condition)
{
    return last_ucmd.rightmove > 0;
}

qboolean Player::checkduck(Conditional &condition)
{
    if (client->ps.pm_flags & PMF_DUCKED)
        return qtrue;
    else
        return qfalse;
}

qboolean Player::checkrise(Conditional &condition)
{
    if ((do_rise) && (velocity.z > 32.0f)) {
        return qtrue;
    }
    do_rise = qfalse;

    return qfalse;
}

qboolean Player::checkjump(Conditional &condition)
{
    if (sv_instantjump->integer)
        return client->ps.jumped;
    else
        return last_ucmd.upmove > 0;
}

qboolean Player::checkcrouch(Conditional &condition)
{
    if (last_ucmd.upmove < 0) 
        return qtrue;

    return qfalse;
}

qboolean Player::checkattackleft( Conditional &condition )
{
	if ( level.playerfrozen || ( flags & FL_IMMOBILE ) )
	{
		return qfalse;
	}

	if ( multiplayerManager.inMultiplayer() && ( !multiplayerManager.isFightingAllowed() || multiplayerManager.isPlayerSpectator( this ) ) )
	{
		return qfalse;
	}


	if ( client->ps.leanDelta != 0 )
		return false;

	if ( last_ucmd.buttons & BUTTON_ATTACKLEFT )
	{
		Weapon *weapon;

		last_attack_button = BUTTON_ATTACKLEFT;

		weapon = GetActiveWeapon( WEAPON_LEFT );
		if ( weapon )
		{
			weapon->CheckReload( FIRE_MODE1 );
			return qtrue;
		}

		weapon = GetActiveWeapon( WEAPON_RIGHT );
		if ( weapon )
		{
			weapon->CheckReload( FIRE_MODE1 );
			return qtrue;
		}

		weapon = GetActiveWeapon( WEAPON_DUAL );
		if ( weapon )
		{
			weapon->CheckReload( FIRE_MODE1 );
			return qtrue;
		}

		return qfalse;
	}
	else
	{
		return qfalse;
	}
}

qboolean Player::checkrun(Conditional &condition)
{
    return (last_ucmd.buttons & BUTTON_RUN) != 0;
}

qboolean Player::checkwasrunning(Conditional &condition)
{
    return (pm_lastruntime > MINIMUM_RUNNING_TIME);
}

qboolean Player::checkholsterweapon(Conditional &condition)
{
    if (client->ps.pm_flags & PMF_DISABLE_INVENTORY || _disableUseWeapon) {
        return qfalse;
    }

    return (last_ucmd.buttons & BUTTON_HOLSTERWEAPON) != 0;
}

qboolean Player::checkblocked(Conditional &condition)
{
    int test_moveresult;

    test_moveresult = moveresult;

    if (flags & FL_IMMOBILE)
        test_moveresult = MOVERESULT_BLOCKED;

    if (test_moveresult == MOVERESULT_BLOCKED) {
        return true;
    }

    return false;
}

qboolean Player::checkonground(Conditional &condition)
{
    return client->ps.walking;
}

qboolean Player::checkfalling(Conditional &condition)
{
    return falling;
}

qboolean Player::checkatdoor(Conditional &condition)
{
    return (atobject && atobject->isSubclassOf(Door));
}

qboolean Player::checkdead(Conditional &condition)
{
    return (deadflag);
}

qboolean Player::checkhealth(Conditional &condition)
{
    return health < atoi(condition.getParm(1));
}

qboolean Player::checkpain(Conditional &condition)
{
    return ((pain != 0) || (knockdown != 0));
}

qboolean Player::checkknockdown(Conditional &condition)
{
    if (knockdown) {
        knockdown = false;
        return qtrue;
    } else {
        return qfalse;
    }
}

qboolean Player::checklegsstate(Conditional &condition)
{
    if (currentState_Legs) {
        str current = currentState_Legs->getName();
        str compare = condition.getParm(1);

        if (current == compare) {
            return true;
        }
    }

    return false;
}

qboolean Player::checktorsostate(Conditional &condition)
{
    if (currentState_Torso) {
        str current = currentState_Torso->getName();
        str compare = condition.getParm(1);

        if (current == compare) {
            return true;
        }
    }

    return false;
}

qboolean Player::checkchance(Conditional &condition)
{
    float percent_chance;

    percent_chance = (float)atof(condition.getParm(1));

    return (G_Random() < percent_chance);
}

qboolean Player::checkpoints(Conditional &condition)
{
    int pointnum;
    pointnum = (int)atoi(condition.getParm(1));

    if (points >= pointnum)
        return true;

    return false;
}

qboolean Player::returntrue(Conditional &condition)
{
    return true;
}

qboolean Player::checkuse( Conditional &condition )
{
	return ( last_ucmd.buttons & BUTTON_USE ) != 0;
}

qboolean Player::checkturnleft(Conditional &condition)
{
    float yaw;

    yaw = SHORT2ANGLE(last_ucmd.angles[YAW] + client->ps.delta_angles[YAW]);
    return (angledist(old_v_angle[YAW] - yaw) < -8.0f);
}

qboolean Player::checkturnright(Conditional &condition)
{
    float yaw;

    yaw = SHORT2ANGLE(last_ucmd.angles[YAW] + client->ps.delta_angles[YAW]);
    return (angledist(old_v_angle[YAW] - yaw) > 8.0f);
}

// the run key was up, which retail never does.
qboolean Player::checksneak( Conditional &condition )
{
	return qfalse; 
}


Condition<Player> Player::Conditions[] =
{
	{ "default",							&Player::returntrue },
	{ "SNEAK",								&Player::checksneak },
	{ "RUN",								&Player::checkrun },
	{ "WAS_RUNNING",						&Player::checkwasrunning },
	{ "HOLSTERWEAPON",						&Player::checkholsterweapon },
	{ "USE",								&Player::checkuse },
	{ "LEFT",								&Player::checkturnleft },
	{ "RIGHT",								&Player::checkturnright },
	{ "FORWARD",							&Player::checkforward },
	{ "BACKWARD",							&Player::checkbackward },
	{ "STRAFE_LEFT",						&Player::checkstrafeleft },
	{ "STRAFE_RIGHT",						&Player::checkstraferight },
	{ "LEAN_LEFT",							&Player::checkleanleft },
	{ "LEAN_RIGHT",							&Player::checkleanright},
	{ "DUCK",								&Player::checkduck },
	{ "JUMP",								&Player::checkjump },
	{ "RISE",								&Player::checkrise },
	{ "CROUCH",								&Player::checkcrouch },
	{ "ANIMDONE_LEGS",						&Player::checkanimdone_legs },
	{ "ANIMDONE_TORSO",						&Player::checkanimdone_torso },
	{ "CAN_TURN",							&Player::checkcanturn },
	{ "BLOCKED",							&Player::checkblocked },
	{ "ONGROUND",							&Player::checkonground },
	{ "CAN_FALL",							&Player::checkcanfall },
	{ "AT_DOOR",							&Player::checkatdoor },
	{ "FALLING",							&Player::checkfalling },
	{ "HARD_IMPACT",						&Player::checkhardimpact },
	{ "KILLED",								&Player::checkdead },
	{ "HEALTH",								&Player::checkhealth },
	{ "PAIN",								&Player::checkpain },
	{ "PAIN_TYPE",							&Player::checkpaintype },
	{ "PAIN_DIRECTION",						&Player::checkpaindirection },
	{ "PAIN_THRESHOLD",						&Player::checkpainthreshold },
	{ "PAIN_ACCUMULATED",					&Player::checkaccumulatedpain },
	{ "KNOCKDOWN",							&Player::checkknockdown },
	{ "LEGS",								&Player::checklegsstate },
	{ "TORSO",								&Player::checktorsostate },
	{ "AT_USEANIM",							&Player::checkatuseanim },
	{ "TOUCHEDUSEANIM",						&Player::checktouchuseanim },
	{ "FINISHEDUSEANIM",					&Player::checkuseanimfinished },
	{ "AT_USEOBJECT",						&Player::checkatuseobject },
	{ "AT_USEENTITY",						&Player::checkatuseentity },
	{ "LOOP_USEOBJECT",						&Player::checkloopuseobject },
	{ "CAN_PUSH",							&Player::checkpush },
	{ "CAN_PULL",							&Player::checkpull },
	{ "ON_LADDER",							&Player::checkOnLadder },
	{ "CAN_STAND",							&Player::checkcanstand },
	{ "CHANCE",								&Player::checkchance },
	{ "TURNED_LEFT",						&Player::checkturnedleft },
	{ "TURNED_RIGHT",						&Player::checkturnedright },
	{ "IN_VEHICLE",							&Player::checkinvehicle },
	{ "SOLID_FORWARD",						&Player::checksolidforward },
	{ "GROUNDENTITY",						&Player::checkgroundentity },
	{ "FAKEPLAYERACTIVE",					&Player::checkfakeplayeractive },
	{ "SLOPE_22",							&Player::check22degreeslope },
	{ "SLOPE_45",							&Player::check45degreeslope },
	{ "RIGHT_LEG_HIGH",						&Player::checkrightleghigh },
	{ "LEFT_LEG_HIGH",						&Player::checkleftleghigh },
	{ "FACING_UP_SLOPE",					&Player::checkfacingupslope },
	{ "FACING_DOWN_SLOPE",					&Player::checkfacingdownslope },
	
	{ "ATTACKLEFT",							&Player::checkattackleft },  
	{ "ATTACKRIGHT",						&Player::checkattackright }, 
	{ "ATTACKLEFTBUTTON",					&Player::checkattackbuttonleft }, 
	{ "ATTACKRIGHTBUTTON",					&Player::checkattackbuttonright },
	{ "HAS_DUAL_WEAPON",					&Player::checkhasdualweapon},
	{ "HAS_WEAPON",							&Player::checkhasweapon },
	{ "NEW_WEAPON",							&Player::checknewweapon },
	{ "IS_NEW_WEAPON",						&Player::checkuseweapon },
	{ "IS_WEAPON_ACTIVE",					&Player::checkweaponactive },
	{ "IS_WEAPON_RELOADING",				&Player::checkweaponreload },
	{ "IS_IN_MODE",							&Player::checkweaponinmode },
	{ "IS_WEAPON_SWITCHINGMODE",			&Player::checkweaponswitchmode },
	{ "IS_WEAPON_READY_TO_FIRE",			&Player::checkweaponreadytofire },
	{ "IS_DUALWEAPON_READY_TO_FIRE",		&Player::checkdualweaponreadytofire },
	{ "IS_WEAPON_FINISHED_FIRING",			&Player::checkweapondonefiring		},
	{ "PUTAWAYBOTH",						&Player::checkputawayboth },
	{ "PUTAWAYLEFT",						&Player::checkputawayleft },
	{ "PUTAWAYRIGHT",						&Player::checkputawayright },
	{ "TARGET_ACQUIRED",					&Player::checktargetacquired },
	{ "ANY_WEAPON_ACTIVE",					&Player::checkanyweaponactive },
	{ "ATTACK_BLOCKED",						&Player::checkattackblocked },
	{ "STATE_ACTIVE",						&Player::checkstatename },
	{ "BLOCK_DELAY",						&Player::checkblockdelay },
	{ "DUALWIELD",							&Player::checkdualwield },
	{ "DUALWIELDWEAPONS",					&Player::checkdualweapons },
	{ "HAS_AMMO",							&Player::checkweaponhasammo },
	{ "HAS_FULLAMMO",						&Player::checkweaponhasfullammo },
	{ "HAS_INVAMMO",						&Player::checkweaponhasinvammo },
	{ "WEAPONS_HOLSTERED",					&Player::checkweaponsholstered },
	{ "WEAPON_LOWERED",						&Player::checkweaponlowered }, 
	{ "WEAPON_FIRETIMER",					&Player::checkweaponfiretimer },
	{ "WEAPON_FORCERELOAD",					&Player::checkweaponforcereload },
	{ "WEAPON_CAN_RELOAD",					&Player::checkweaponcanreload },
	{ "WEAPON_FULLCLIP",					&Player::checkweaponfullclip },
	{ "IS_SPECIALMOVE_CHARGED",				&Player::checkspecialmovecharge },
	{ "IS_CHAR_AVAILABLE",					&Player::checkcharavailable },
	
	{ "STANCE_CHANGEDTORSO",				&Player::checkstancechangedtorso },
	{ "STANCE_CHANGEDLEGS",					&Player::checkstancechangedlegs },
	{ "STANCE",								&Player::checkstance },
	{ "POINTS",								&Player::checkpoints },
	{ "INCOMING_MELEE",						&Player::checkincomingmeleeattack },
	{ "FINISHINGMOVE",						&Player::checkfinishingmove },
	{ "USINGENTITY",						&Player::checkusingentity },
	{ "IS_THIRDPERSON",						&Player::checkthirdperson },
	{ "PROP_CHANCE",						&Player::checkPropChance },
	{ "PROP_EXISTS",						&Player::checkPropExists	},
	{ "ANIMCHAIN_END",						&Player::checkEndAnimChain },
	{ "WEAPON_TYPE",						&Player::checkWeaponType },
	{ "HAS_ANIM",							&Player::checkHasAnim },
	{ "IS_WEAPON_CONTROLLING_PROJECTILE",	&Player::checkIsWeaponControllingProjectile },
	
	{ NULL,									NULL },
};



void Player::ActivateDualWeapons( Event *ev )
{
	int i;
	
	Weapon *weapon = 0;
	for ( i=dual_wield_weaponlist.NumObjects(); i>=1; i-- )
	{
		WeaponSetItem   *dw;
		
		dw = dual_wield_weaponlist.ObjectAt( i );
		
		weapon = ( Weapon * )FindItem( dw->name, weapon );
		
		if ( !weapon )
		{
			warning( "Player::ActivateDualWeapons", "Player does not have weapon %s", dw->name.c_str() );
			return;
		}
		
		ChangeWeapon( weapon, dw->hand );
		dual_wield_weaponlist.RemoveObjectAt( i );
		delete dw;
	}
	
	ClearNewActiveWeapon();
	
	holsteredWeapons[WEAPON_LEFT]  = NULL;
	holsteredWeapons[WEAPON_RIGHT] = NULL;
	holsteredWeapons[WEAPON_DUAL]  = NULL;
	
	WeaponsNotHolstered();
}


void Player::ActivateNewWeapon( Event *ev )
{
	ChangeWeapon( newActiveWeapon.weapon, newActiveWeapon.hand );
	
	ClearNewActiveWeapon();
	
	holsteredWeapons[WEAPON_LEFT]  = NULL;
	holsteredWeapons[WEAPON_RIGHT] = NULL;
	holsteredWeapons[WEAPON_DUAL]  = NULL;
	
	WeaponsNotHolstered();
}


void Player::ActivateShield( Event *ev )
{
	shield_active = true;
}


void Player::AddMeleeAttacker( Event *ev)
{
	Entity *ent = ev->GetEntity( 1 );
	meleeAttackerList.AddUniqueObject( ent );
}


void Player::AdjustTorso( Event *ev )
{
	adjust_torso = ev->GetBoolean( 1 );
}


void Player::ApplyPowerupEffects(int &moveSpeed)
{
	if ( _powerup )
		moveSpeed *= (int)_powerup->getMoveMultiplier();

	if ( _rune )
		moveSpeed *= (int)_rune->getMoveMultiplier();

}


void Player::CallVote( Event *ev )
{
	str command;
	str arg;

	if ( multiplayerManager.inMultiplayer() )
	{
		command = ev->GetString( 1 );

		if ( ev->NumArgs() > 1 )
			arg = ev->GetString( 2 );

		multiplayerManager.callVote( this, command, arg );
	}
}


void Player::ChangeChar( Event *ev )
{
	if ( changingChar ) 
		return;
	
	str curChar = gi.cvar("g_playermodel","",0)->string;
	str curStateFile = gi.cvar("g_statefile","",0)->string;
	if ((ev->GetString( 1 ) == curStateFile) && (ev->GetString( 2 ) == curChar))
		return;
	
	Actor *act = Actor::FindActorByName(ev->GetString( 3 ));
	if ( act )
	{
		changingChar = true;
		G_FadeOut(1.0f);
		Event *ev2 = new Event(EV_Player_ChangeCharFadeIn);
		ev2->AddString(ev->GetString( 1 ));
		ev2->AddString(ev->GetString( 2 ));
		ev2->AddString(ev->GetString( 3 ));
		ev2->AddString(ev->GetString( 4 ));
		ev2->AddEntity(act);
		PostEvent(ev2, 1.0f);
	}
}


void Player::ChangeCharFadeIn( Event *ev )
{
	str statemachine = ev->GetString( 1 );
	str newmodel = ev->GetString( 2 );
	str replacemodel = ev->GetString( 3 );
	str spawnNPC = ev->GetString( 4 );
	Actor *act = (Actor*)ev->GetEntity( 5 );

	ProcessEvent(EV_DetachAllChildren);

	gi.cvar_set( "g_statefile", statemachine.c_str() );
	gi.cvar_set( "g_playermodel", newmodel.c_str() );
	InitModel();
	LoadStateTable();

	Event *e = new Event( EV_Player_SpawnActor );
	e->AddString( spawnNPC.c_str() );
	e->AddString( "origin" );
	e->AddVector(origin);
	e->AddString( "angles" );
	e->AddVector(angles);
	SpawnActor(e);

	origin = act->origin;
	SetViewAngles(act->angles);
	act->ProcessEvent(  EV_Remove );

	G_AutoFadeIn();
	changingChar = false;
}


void Player::ChangeStance(Event *ev)
{
	if ( ev->NumArgs() < 1 )
		return;
	
	changedStanceTorso = true;
	changedStanceLegs = true;
	stanceNumber = ev->GetInteger( 1 );
}


void Player::ClearDialog( Event *ev )
{
	ClearDialog();
}
void Player::ClearDialog( void )
{
	_dialogEntnum = ENTITYNUM_NONE;
	_dialogSoundIndex = -1;
}

void Player::ClearIncomingMelee(Event *ev)
{
	incomingMeleeAttack = false;
}


void Player::ClearStanceLegs(Event *ev)
{
	changedStanceLegs = false;
}


void Player::ClearStanceTorso(Event *ev)
{
	changedStanceTorso = false;
}


void Player::ClearTarget( Event *ev )
{
	targetEnemy = NULL;
}


void Player::ClearTextDialog( Event* ev )
{
	ClearTextDialog();
}


void Player::ClientMoveFlagsAndSpeeds( int moveSpeed, int noclipSpeed, int crouchSpeed, int airSpeed )
{
	if ( level.playerfrozen || ( flags & FL_STUNNED ) )
	{
		client->ps.pm_flags |= PMF_FROZEN;
	}
	
	if ( ( flags & FL_IMMOBILE ) || ( flags & FL_PARTIAL_IMMOBILE ) )
	{
		client->ps.pm_flags |= PMF_NO_MOVE;
	}
	
	if ( movecontrol == MOVECONTROL_ANIM )
		client->ps.pm_flags |= PMF_NO_PREDICTION;
	
	if ( !groundentity )
	{
		client->ps.speed = airSpeed;
	}
	else if ( getMoveType() == MOVETYPE_NOCLIP || ( waterlevel > 1 ) )
	{
		if ( last_ucmd.buttons & BUTTON_RUN )
			client->ps.speed = noclipSpeed;
		else
			client->ps.speed = noclipSpeed/2;
	}
	else
	{
		if ( client->ps.pm_flags & PMF_DUCKED )
		{
			client->ps.speed = crouchSpeed;	
		}
		else
		{
			if ( sv_useanimmovespeed->integer || movecontrol == MOVECONTROL_ANIM )
			{
				client->ps.speed = (int) animspeed;
			}
			else
			{
				if ( last_ucmd.buttons & BUTTON_RUN )
					client->ps.speed = moveSpeed;
				else
					client->ps.speed = moveSpeed/2;
			}
		}
	}
	
	if ( getMoveType() == MOVETYPE_NOCLIP )
	{
		if ( last_ucmd.buttons & BUTTON_RUN )
			client->ps.speed = noclipSpeed;
		else
			client->ps.speed = noclipSpeed/2;
	}
	
	if ( sv_instantjump->integer )
		client->ps.instantJump = true;
	else
		client->ps.instantJump = false;

	if ( sv_strafeJumpingAllowed->integer )
		client->ps.strafeJumpingAllowed = true;
	else
		client->ps.strafeJumpingAllowed = false;
}


void Player::ClientMoveLean( usercmd_t *ucmd )
{
}


void Player::ClientMoveMisc( usercmd_t *ucmd )
{
	if ( client->ps.feetfalling && ( waterlevel < 2 ) && !( client->ps.pm_time ) )
	{
		ucmd->forwardmove = 0;
		ucmd->rightmove = 0;
	}
	
	if ( fakePlayer_active || movecontrol == MOVECONTROL_ANIM )
	{
		ucmd->forwardmove = 0;
		ucmd->rightmove = 0;
		ucmd->upmove = 0;
	}
}


void Player::DeactivateShield( Event *ev )
{
	shield_active = false;
}




void Player::DeactivateWeapon( Event *ev )
{
    weaponhand_t hand;
    str   side;

    side = ev->GetString( 1 );

    hand = WeaponHandNameToNum( side );

    if ( hand == WEAPON_ERROR )
        return;

    Sentient::DeactivateWeapon( hand );

    if ( !GetActiveWeapon( WEAPON_LEFT ) && !GetActiveWeapon( WEAPON_RIGHT ) && !GetActiveWeapon( WEAPON_DUAL ) )
    {
        WeaponsHolstered();
    }
}


void Player::DualWield( Event *ev )
{
	str      leftweap, rightweap;
	Weapon   *leftactweap, *rightactweap, *dualactweap;
	
	leftweap  = ev->GetString( 1 );
	rightweap = ev->GetString( 2 );
	
	leftactweap  = GetActiveWeapon( WEAPON_LEFT );
	rightactweap = GetActiveWeapon( WEAPON_RIGHT );
	dualactweap  = GetActiveWeapon( WEAPON_DUAL );
	
	if ( dualactweap )
	{
		dualactweap->SetPutAway( true );
	}
	
	if (
        ( leftactweap && !leftweap.icmp( leftactweap->getName() ) ) &&
        ( rightactweap && !rightweap.icmp( rightactweap->getName() ) )
		)
	{
		leftactweap->SetPutAway( true );
		rightactweap->SetPutAway( true );
		return;
	}
	
	WeaponSetItem *dualweap;
	
	if ( !leftactweap )
	{
		dualweap = new WeaponSetItem;
		dualweap->name = leftweap;
		dualweap->hand = WEAPON_LEFT;
		dual_wield_weaponlist.AddObject( dualweap );
	}
	else if ( leftweap.icmp( leftactweap->getName() ) )
	{
		leftactweap->SetPutAway( true );
		
		dualweap = new WeaponSetItem;
		dualweap->name = leftweap;
		dualweap->hand = WEAPON_LEFT;
		dual_wield_weaponlist.AddObject( dualweap );
	}
	
	if ( !rightactweap )
	{
		dualweap = new WeaponSetItem;
		dualweap->name = rightweap;
		dualweap->hand = WEAPON_RIGHT;
		dual_wield_weaponlist.AddObject( dualweap );
	}
	else if ( rightweap.icmp( rightactweap->getName() ) )
	{
		rightactweap->SetPutAway( true );
		
		dualweap = new WeaponSetItem;
		dualweap->name = rightweap;
		dualweap->hand = WEAPON_RIGHT;
		dual_wield_weaponlist.AddObject( dualweap );
	}
	
	dual_wield_active = true;
}


void Player::EvaluateTorsoAnim( Event *ev )
{
	str torsoAnim( currentState_Torso->getTorsoAnim( *this, &torso_conditionals ) );
	if ( !animate->HasAnim(torsoAnim) )
		torsoAnim = getGameplayAnim(torsoAnim);
	
	if ( torsoAnim == "" )
	{
		partAnim[ torso ] = "";
		animate->ClearTorsoAnim();
	}
	else if ( torsoAnim != "none" )
	{
		SetAnim( torsoAnim.c_str(), torso );
	}
}


void Player::EventUseItem( Event *ev )
{
	const char  *name;
	weaponhand_t hand = WEAPON_DUAL;
	
	if ( deadflag || _disableUseWeapon )
	{
		return;
	}

	if ( multiplayerManager.inMultiplayer() && ( !multiplayerManager.isFightingAllowed() || multiplayerManager.isPlayerSpectator( this ) ) )
	{
		return;
	}
	
	name = ev->GetString( 1 );
	
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	
	str objectName("CurrentPlayer.");
	objectName += name ;
	if ( gpm->hasProperty( objectName, "name" ) )
	{
		name = gpm->getStringValue( objectName, "name" );
	}
	else if ( gpm->hasProperty(name, "consumable" ) )
	{
		str slotname = gpm->getStringValue(name, "invslot");
		if ( !gpm->hasProperty(slotname, "quantity") )
			return;
		
		float quantity = gpm->getFloatValue(slotname, "quantity");
		if ( quantity < 1.0f )
			return;
		
		str model_name = gpm->getStringValue(name, "model");
		giveItem(model_name);
		gpm->setFloatValue(slotname, "quantity", quantity-1.0f);
	}
	
	Item *item = ( Item * )FindItem( name );
	
	if ( item && item->isSubclassOf( InventoryItem ) )
	{
		InventoryItem *ii = ( InventoryItem * )item;
		Event *ev1;
		
		ev1 = new Event( EV_InventoryItem_Use );
		ev1->AddEntity( this );
		ii->ProcessEvent( ev1 );
		return;
	}
	else if ( ev->NumArgs() > 1 )
	{
		hand = WeaponHandNameToNum( ev->GetString( 2 ) );
	}
	
	useWeapon( name, hand );	
}


void Player::ExtraEntitiesToSendToClient( int *numExtraEntities, int *extraEntities )
{
	*numExtraEntities = 0;
	
	if ( _dialogEntnum != ENTITYNUM_NONE )
	{
		extraEntities[ *numExtraEntities ] = _dialogEntnum;
		(*numExtraEntities)++;
		
		
		if ( *numExtraEntities == MAX_EXTRA_ENTITIES_FROM_GAME )
			return;
	}
}


void Player::FakePlayer( qboolean holster )
{
	if ( fakePlayer )
	{
		return;
	}
	
	fakePlayer_active = true;
	
	if ( !holster )
	{
		StartFakePlayer();
	}
	else
	{
		if ( WeaponsOut() )
		{
			SafeHolster( true );
		}
	}
}


void Player::Gib( void )
{
}


void Player::GiveOxygen( float time )
{
	air_finished = level.time + time;
}


void Player::GivePointsEvent( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		AwardPoints(ev->GetInteger( 1 ));
}


void Player::GravityNodes( void )
{
	Vector grav;
	Vector gravnorm;
	Vector velnorm;
	float  dot;
	qboolean force;
	float max_speed;
	Vector new_velocity;
	
	
	if ( client->ps.pm_flags & PMF_TIME_WATERJUMP )
	{
		return;
	}
	
	grav = gravPathManager.CalculateGravityPull( *this, origin, &force, &max_speed );
	
	if ( force && ( grav != vec_zero ) )
	{
		velnorm = velocity;
		velnorm.normalize();
		
		gravnorm = grav;
		gravnorm.normalize();
		
		dot = ( gravnorm.x * velnorm.x ) + ( gravnorm.y * velnorm.y ) + ( gravnorm.z * velnorm.z );
		
		if ( dot < 0.0f )
		{
			float tempdot;
			Vector tempvec;
			
			tempdot = 0.2f - dot;
			tempvec = velocity * tempdot;
			velocity = velocity - tempvec;
		}
	}
	
	if ( grav != vec_zero )
	{
		new_velocity = velocity + grav;
		
		if ( new_velocity.length() < velocity.length() )
		{
			velocity = new_velocity;
		}
		else if ( velocity.length() < max_speed )
		{
			
			velocity = new_velocity;
			
			
			if ( velocity.length() > max_speed )
			{
				velocity.normalize();
				velocity *= max_speed;
			}
		}
		else
		{
			
			grav.x = 0;
			grav.y = 0;
			
			velocity = velocity + grav;
		}
	}
}


void Player::HeadWatchAllowed(Event *ev)
{
	Sentient::HeadWatchAllowed( ev );
}


void Player::SetCurrentCallVolume(const str& volumeName)
{
    currentCallVolume = volumeName;
}

str Player::GetCurrentCallVolume()
{
    return currentCallVolume;
}


void Player::HideCoolItem( Event *ev )
{
	Entity *fx;
	Vector org;
	
	org = origin;
	org.z += 128.0f;
	
	fx = new Entity( ENTITY_CREATE_FLAG_ANIMATE );
	
	fx->setOrigin( org );
	fx->setModel( "fx_coolitem_reverse.tik" );
	fx->animate->RandomAnimate( "idle" );
	fx->PostEvent( EV_Remove, 1.0f );
	
	if ( cool_item )
	{
		Event * event;
		
		cool_item->PostEvent( EV_Hide, 1.0f );
		
		event = new Event( EV_SetOrigin );
		event->AddVector( vec_zero );
		cool_item->PostEvent( event, 1.0f );
		cool_item->edict->s.renderfx &= ~RF_VIEWLENSFLARE;
	}
}


void Player::InitInventory( void )
{
}


void Player::InitStats( void )
{
	if ( p_heuristics )
	{
		p_heuristics->LoadHeuristics();
		p_heuristics->ClearLevelStatistics();
	}
}


void Player::KnockbackMultiplier(Event *ev)
{
	knockbackMultiplier = ev->GetInteger( 1 );
}


void Player::MeleeEvent( Event *ev )
{
	Vector melee_pos;
	Vector melee_end;
	float damage = 20;
	str means_of_death_string;
	meansOfDeath_t attack_means_of_death;
	float knockback;
	
	
	if ( ev->NumArgs() > 0 )
		damage = ev->GetFloat( 1 );
	
	if ( ev->NumArgs() > 1 )
		means_of_death_string = ev->GetString( 2 );
	
	if ( ev->NumArgs() > 2 )
		knockback = ev->GetFloat( 3 );
	else
		knockback = damage * 8.0f;
	
	melee_pos = centroid;
	melee_end = centroid + Vector( orientation[0] ) * 96.0f;
	
	if ( means_of_death_string.length() > 0 )
		attack_means_of_death = (meansOfDeath_t)MOD_NameToNum( means_of_death_string );
	else
		attack_means_of_death = MOD_CRUSH;
	
	MeleeAttack( melee_pos, melee_end, damage, this, attack_means_of_death, 15.0f, -45.0f, 45.0f, knockback );
}


void Player::NightvisionToggle( Event *ev )
{
	if ( level.cinematic )
		return;

	if ( ( multiplayerManager.isPlayerSpectator( this ) ) && ( client->ps.pm_flags ^ PMF_NIGHTVISION ) )
		return;

	client->ps.pm_flags ^= PMF_NIGHTVISION;

	Event *newEvent;
	newEvent = new Event( EV_Sentient_SetViewMode );
	if (client->ps.pm_flags & PMF_NIGHTVISION) 
		newEvent->AddString("nightvision");
	else
		newEvent->AddString("normal");
	ProcessEvent( newEvent );
}


void Player::PassEventToVehicle( Event *ev )
{
	if ( vehicle )
		vehicle->HandleEvent( ev );
}


void Player::PlayerKnockback(Event *ev)
{
	playerKnockback = ev->GetInteger( 1 );
}


void Player::ProjDetonate(Event *ev)
{
	if ( ev->NumArgs() > 0 )
		projdetonate = ev->GetBoolean(1);
}


void Player::PutawayWeapon( Event *ev )
{
	Weapon * weapon;
	weaponhand_t hand;
	str   side;
	
	side = ev->GetString( 1 );
	
	hand = WeaponHandNameToNum( side );
	
	if ( hand == WEAPON_ERROR )
		return;
	
	weapon = GetActiveWeapon( hand );
	if ( !weapon ) return;
	
	if ( weapon->isSubclassOf( Weapon ) )
	{
		weapon->targetidleflag = false;
		CancelEventsOfType( EV_Weapon_TargetIdleThink );

		weapon->PutAway();

		if ( gi.Anim_NumForName( weapon->edict->s.modelindex, "putaway" ) != -1 )
			weapon->SetAnim( "putaway", EV_Weapon_DonePutaway );
		else
			weapon->PostEvent( EV_Weapon_DonePutaway, 0.0f );
	}
}


void Player::ReleaseFireWeapon( Event *ev )
{
	Sentient::ReleaseFireWeapon(ev);
}


void Player::ReloadTiki( Event *ev )
{
	if ( ev->NumArgs() < 1 )
		return;
	
	int frame, anim, animstate;
	Viewthing *viewthing;
	
	viewthing = ( Viewthing * )( ( Entity * )Viewmodel.current_viewthing );
	if ( !viewthing )
		return;
	
	frame = viewthing->frame;
	anim = viewthing->CurrentAnim();
	Vector vieworigin(viewthing->origin.x, viewthing->origin.y, viewthing->origin.z);
	Vector viewangles(viewthing->angles.x, viewthing->angles.y, viewthing->angles.z);
	
	animstate = viewthing->animstate-1;
	if ( animstate < 0 ) 
		animstate = -1;
	
	Event *ev2 = new Event(EV_ViewThing_SpawnFromTS);
	ev2->AddString(ev->GetString(1));
	ev2->AddString(viewthing->model);
	Viewmodel.ProcessEvent(ev2);
	
	viewthing = ( Viewthing * )( ( Entity * )Viewmodel.current_viewthing );
	
	ev2 = new Event;
	ev2->AddFloat(vieworigin.x);
	ev2->AddFloat(vieworigin.y);
	ev2->AddFloat(vieworigin.z);		
	viewthing->ChangeOrigin(ev2);
	if ( ev2 ) delete ev2;
	
	ev2 = new Event;
	ev2->AddFloat(viewangles.x);
	ev2->AddFloat(viewangles.y);
	ev2->AddFloat(viewangles.z);		
	viewthing->SetAnglesEvent(ev2);
	
	viewthing->frame = frame;
	viewthing->SetAnim(anim);
	viewthing->animstate = animstate;
	if ( ev2 ) delete ev2;
	ev2 = new Event;
	viewthing->ToggleAnimateEvent(ev2);
}


void Player::ReloadWeapon( Event *ev )
{
	Weapon* weapon = 0;
	weapon = GetActiveWeapon( WEAPON_DUAL );
	if (!weapon )
		return;
	
	weapon->ForceReload();
}


void Player::RemoveFakePlayer( void )
{
	Actor * fake;
	
	if ( !fakePlayer )
	{
		return;
	}
	fake = fakePlayer;
	
	this->setOrigin( fake->origin );
	this->setAngles( fake->angles );
	this->SetViewAngles( fake->angles );
	this->showModel();
	this->ProcessEvent( EV_Sentient_TurnOnShadow );
	this->flags &= ~FL_IMMOBILE;
	setSolidType( SOLID_BBOX );
	fake->PostEvent ( EV_Remove, 0.f );
	fakePlayer = NULL;
	SafeHolster( false );
}


void Player::SetAimType( Event *ev )
{
	Weapon* weapon = 0;
	weapon = GetActiveWeapon( WEAPON_DUAL );
	if (!weapon )
		return;
	
	weapon->SetAimType( ev );
}


void Player::SetAnim( const char *anim, bodypart_t part, bool force )
{
	assert( anim );
	
	if ( !force && ( part != all ) && ( partAnim[ part ] == anim ) )
	{
		return;
	}
	
	if ( !force && ( part == all ) && ( partAnim[ legs ] == anim ) && ( partAnim[ torso ] == anim ) )
	{
		return;
	}
	
	if ( getMoveType() == MOVETYPE_NOCLIP )
	{
		anim = "idle";
	}
	
	if ( part != all )
	{
		partAnim[ part ] = anim;
	}
	else
	{
		partAnim[ legs ] = anim;
		partAnim[ torso ] = anim;
	}
	
	switch( part )
	{
	default :
	case all :
		animate->RandomAnimate( anim, EV_Player_AnimLoop_Legs, part );
		break;
		
	case legs :
		animate->RandomAnimate( anim, EV_Player_AnimLoop_Legs, part );
		break;
		
	case torso :
		animate->RandomAnimate( anim, EV_Player_AnimLoop_Torso, part );
		break;
	}
	
	if ( ( part == legs ) || ( part == all ) )
	{
		Vector animmove;
		float time;
		float len;
		
		time = gi.Anim_Time( edict->s.modelindex, CurrentAnim() );
		gi.Anim_Delta( edict->s.modelindex, CurrentAnim(), animmove );
		len = animmove.length();
		if ( ( len == 0.0f ) || ( time == 0.0f ) )
		{
			animspeed = 0;
		}
		else
		{
			animspeed = len / time;
		}
	}
}


void Player::SetBendTorso(Event *ev)
{
	if ( ev->NumArgs() > 0 )
		bendTorsoMult = ev->GetFloat( 1 );
	else
		bendTorsoMult = standardTorsoMult;
}


void Player::SetHeadTarget
   (
   Event *ev
   )

   {
   str ht = ev->GetString( 1 );

   head_target = G_FindTarget( 0, ht );
   }


void Player::SetPlayerChar( Event *ev )
{
	str statemachine = ev->GetString( 1 );
	str newmodel = ev->GetString( 2 );
	
	str curChar = gi.cvar("g_playermodel","",0)->string;
	str curStateFile = gi.cvar("g_statefile","",0)->string;
	if ((statemachine == curStateFile) && (newmodel == curChar))
		return;
	
	gi.cvar_set( "g_statefile", statemachine.c_str() );
	gi.cvar_set( "g_playermodel", newmodel.c_str() );
	ProcessEvent(EV_DetachAllChildren);
	InitModel();
	LoadStateTable();
}


void Player::SetPlayerViewNormal( void )
{
	client->ps.pm_flags &= ~PMF_CAMERA_VIEW;
	
	
	int camera_type = currentState_Torso->getCameraType();
	if ( last_camera_type != camera_type )
    {
		client->ps.camera_flags = client->ps.camera_flags & CF_CAMERA_CUT_BIT;
		bool do_cut = false;
		switch( camera_type )
		{
		case CAMERA_TOPDOWN:
			client->ps.camera_flags |= CF_CAMERA_ANGLES_IGNORE_PITCH;
			client->ps.camera_offset[ PITCH ] = -75;
			client->ps.camera_flags |= CF_CAMERA_ANGLES_ALLOWOFFSET;
			break;
		case CAMERA_FRONT:
			client->ps.camera_flags |= CF_CAMERA_ANGLES_IGNORE_PITCH;
			client->ps.camera_flags |= CF_CAMERA_ANGLES_ALLOWOFFSET;
			client->ps.camera_offset[ YAW ] = 180;
			client->ps.camera_offset[ PITCH ] = 0;
			do_cut = true;
			break;
		case CAMERA_SIDE:
			client->ps.camera_flags |= CF_CAMERA_ANGLES_IGNORE_PITCH;
			client->ps.camera_flags |= CF_CAMERA_ANGLES_ALLOWOFFSET;
			if ( G_Random( 1.0f ) > 0.5f )
			{
				client->ps.camera_offset[ YAW ] = -90;
			}
			else
			{
				client->ps.camera_offset[ YAW ] = 90;
			}
			client->ps.camera_offset[ PITCH ] = 0;
			break;
		case CAMERA_SIDE_LEFT:
			client->ps.camera_flags |= CF_CAMERA_ANGLES_IGNORE_PITCH;
			client->ps.camera_flags |= CF_CAMERA_ANGLES_ALLOWOFFSET;
			client->ps.camera_offset[ YAW ] = 90;
			client->ps.camera_offset[ PITCH ] = 0;
			break;
		case CAMERA_SIDE_RIGHT:
			client->ps.camera_flags |= CF_CAMERA_ANGLES_IGNORE_PITCH;
			client->ps.camera_flags |= CF_CAMERA_ANGLES_ALLOWOFFSET;
			client->ps.camera_offset[ YAW ] = -90;
			client->ps.camera_offset[ PITCH ] = 0;
			break;
		case CAMERA_BEHIND_FIXED:
			client->ps.camera_offset[ YAW ] = 0;
			client->ps.camera_offset[ PITCH ] = 0;
			client->ps.camera_flags |= CF_CAMERA_ANGLES_ALLOWOFFSET;
			break;
		case CAMERA_BEHIND_NOPITCH:
			client->ps.camera_flags |= CF_CAMERA_ANGLES_IGNORE_PITCH;
			client->ps.camera_offset[ YAW ] = 0;
			client->ps.camera_offset[ PITCH ] = 0;
			break;
		case CAMERA_BEHIND:
			client->ps.camera_offset[ YAW ] = 0;
			client->ps.camera_offset[ PITCH ] = 0;
			break;
		default:
			client->ps.camera_offset[ YAW ] = 0;
			client->ps.camera_offset[ PITCH ] = 0;
			break;
		}
		last_camera_type = camera_type;
		if ( do_cut )
			CameraCut();
	}
	
	ShakeCamera();

}


void Player::SetPlayerViewUsingActorController( Camera *camera_shadow2 )
{
	if ( actor_camera )
	{
		
		
		actor_camera->origin = origin;
		actor_camera->origin[2] += client->ps.pm_defaultviewheight + 10.f;
		
		
		Vector forward;
		Vector left;
		angles.AngleVectors( &forward, &left );
		actor_camera->origin -= forward * 15.0f;
		
		actor_camera->origin -= left * 15.0f;
		
		
		actor_camera->setOrigin( actor_camera->origin );
		
		
		actor_camera->setAngles( angles );
		
		
		SetCamera( actor_camera, 0.5f );

	}
	else if ( ( level.automatic_cameras.NumObjects() > 0 ) && ( !camera_shadow2 || camera_shadow2->IsAutomatic() ) )
	{
		AutomaticallySelectedNewCamera();
	}

	client->ps.camera_angles[ 0 ] = camera_shadow2->angles[ 0 ];
	client->ps.camera_angles[ 1 ] = camera_shadow2->angles[ 1 ];
	client->ps.camera_angles[ 2 ] = camera_shadow2->angles[ 2 ];
	
	client->ps.camera_origin[ 0 ] = camera_shadow2->origin[ 0 ];
	client->ps.camera_origin[ 1 ] = camera_shadow2->origin[ 1 ];
	client->ps.camera_origin[ 2 ] = camera_shadow2->origin[ 2 ];
	client->ps.pm_flags |= PMF_CAMERA_VIEW;
	
	client->ps.camera_flags = client->ps.camera_flags & CF_CAMERA_CUT_BIT;
}


void Player::SetPlayerViewUsingCinematicController( Camera *camera_shadow1 )
{
	client->ps.camera_angles[ 0 ] = camera_shadow1->angles[ 0 ];
	client->ps.camera_angles[ 1 ] = camera_shadow1->angles[ 1 ];
	client->ps.camera_angles[ 2 ] = camera_shadow1->angles[ 2 ];
	
	client->ps.camera_origin[ 0 ] = camera_shadow1->origin[ 0 ];
	client->ps.camera_origin[ 1 ] = camera_shadow1->origin[ 1 ];
	client->ps.camera_origin[ 2 ] = camera_shadow1->origin[ 2 ];
	client->ps.pm_flags |= PMF_CAMERA_VIEW;
	
	client->ps.camera_flags = client->ps.camera_flags & CF_CAMERA_CUT_BIT;

	ShakeCamera();
}


void Player::SetPlayerViewUsingEntityWatchingController( void )
{

	SetPlayerViewNormal();

	
	Vector focal_point;
	if ( entity_to_watch->watch_offset != vec_zero )
	{
		MatrixTransformVector( entity_to_watch->watch_offset, entity_to_watch->orientation, focal_point );
		focal_point += entity_to_watch->origin;
		focal_point[2] += client->ps.pm_defaultviewheight + 18.0f;
	}
	else
	{
		focal_point = entity_to_watch->origin;
		focal_point[2] += client->ps.pm_defaultviewheight + 18.0f;
	}

	if ( client->ps.useCameraFocalPoint )
	{
		client->ps.cameraFocalPoint[ 0 ] = focal_point[ 0 ];
		client->ps.cameraFocalPoint[ 1 ] = focal_point[ 1 ];
		client->ps.cameraFocalPoint[ 2 ] = focal_point[ 2 ];
	}
	
}


void Player::SetPlayerViewUsingNoClipController( void )
{
	client->ps.pm_flags &= ~PMF_CAMERA_VIEW;
	
	client->ps.camera_flags = client->ps.camera_flags & CF_CAMERA_CUT_BIT;
}


void Player::SetPointsEvent( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		points = ev->GetInteger( 1 );
}


void Player::SetProjDetonate(qboolean value)
{
	projdetonate = value;
}


void Player::SetState(const str& legsstate, const str& torsostate)
{
	animdone_Legs = false;
	animdone_Torso = false;
	
	movecontrol = MOVECONTROL_USER;
	
	currentState_Legs = statemap_Legs->FindState( legsstate.c_str() );
	currentState_Torso = statemap_Torso->FindState( torsostate.c_str() );
	
	str legsAnim( currentState_Legs->getLegAnim( *this, &legs_conditionals ) );
	if ( !animate->HasAnim(legsAnim) )
		legsAnim = getGameplayAnim(legsAnim);
	if ( legsAnim == "" )
	{
		partAnim[ legs ] = "";
		animate->ClearLegsAnim();
	}
	else if ( legsAnim != "none" )
	{
		SetAnim( legsAnim, legs );
	}
	
	str torsoAnim( currentState_Torso->getTorsoAnim( *this, &torso_conditionals ) );
	if ( !animate->HasAnim(torsoAnim) )
		torsoAnim = getGameplayAnim(torsoAnim);
	if ( torsoAnim == "" )
	{
		partAnim[ torso ] = "";
		animate->ClearTorsoAnim();
	}
	else if ( torsoAnim != "none" )
	{
		SetAnim( torsoAnim.c_str(), torso );
	}
	
	movecontrol = currentState_Legs->getMoveType();
	if ( ( movecontrol < ( sizeof( MoveStartFuncs ) / sizeof( MoveStartFuncs[ 0 ] ) ) ) && ( MoveStartFuncs[ movecontrol ] ) )
	{
		( this->*MoveStartFuncs[ movecontrol ] )();
	}
	
}


void Player::SetupDialog( Event *ev )
{
	Entity *entity;
	str soundName;
	
	entity = ev->GetEntity( 1 );
	soundName = ev->GetString( 2 );
	
	SetupDialog( entity, soundName );
}


void Player::ShakeCamera( void )
{
	
	VectorClear( client->ps.damage_angles );


	if ( ( playerCameraMode == PLAYER_CAMERA_MODE_CINEMATIC ) && !world->canShakeCamera() )
		return;
	
	
	float earthquakeMagnitude = level.getEarthquakeMagnitudeAtPosition( origin );

	if ( earthquakeMagnitude > 0.0f )
	{
		{
			static int uivk_dfx_n;
			if (PROBE_On() && PROBE_Throttle(&uivk_dfx_n, 48, 48)) {
				PROBE("DFX:shake", "ent=%d mag=%.3f ps_dmgang=(%.2f,%.2f,%.2f)\n",
						   entnum, earthquakeMagnitude,
						   client->ps.damage_angles[0], client->ps.damage_angles[1],
						   client->ps.damage_angles[2]);
			}
		}

		client->ps.damage_angles[ PITCH ] += G_CRandom() * earthquakeMagnitude * EARTHQUAKE_SCREENSHAKE_PITCH;
		client->ps.damage_angles[ YAW ]   += G_CRandom() * earthquakeMagnitude * EARTHQUAKE_SCREENSHAKE_YAW;
		client->ps.damage_angles[ ROLL ]  += G_CRandom() * earthquakeMagnitude * EARTHQUAKE_SCREENSHAKE_ROLL;
	}


	if ( playerCameraMode == PLAYER_CAMERA_MODE_CINEMATIC )
		return;

	VectorAdd( client->ps.damage_angles, getWeaponViewShake(), client->ps.damage_angles );
	
	
	if ( damage_count && sv_showdamageshake->integer )
	{
		if ( health <= 0.0f )
		{
			damage_angles = vec_zero;
		}
		else if ( multiplayerManager.inMultiplayer() )
		{
			damage_angles[ YAW ]   = G_CRandom( 0.05f );
			damage_angles[ PITCH ] = G_CRandom( 0.05f );
			damage_angles[ ROLL ]  = G_CRandom( 0.05f );
		}
		else
		{
			damage_angles[ YAW ]   = G_CRandom( 0.2f );
			damage_angles[ PITCH ] = G_CRandom( 0.2f );
			damage_angles[ ROLL ]  = G_CRandom( 0.2f );
		}

		client->ps.damage_angles[ YAW ]   += damage_angles[ YAW ]   * damage_count;
		client->ps.damage_angles[ PITCH ] += damage_angles[ PITCH ] * damage_count;
		client->ps.damage_angles[ ROLL ]  += damage_angles[ ROLL ]  * damage_count;
	}

	if ( client->pers.mp_lowBandwidth )
	{
		client->ps.damage_angles[ YAW ]   = 0.0f;
		client->ps.damage_angles[ PITCH ] = 0.0f;
		client->ps.damage_angles[ ROLL ]  = 0.0f;
	}
}




void Player::ShowCoolItem( Event *ev )
{
	Entity *fx;
	Vector org;
	
	org = origin;
	org.z += 128.0f;
	
	fx = new Entity( ENTITY_CREATE_FLAG_ANIMATE );
	
	fx->setOrigin( org );
	fx->setModel( "fx_coolitem.tik" );
	fx->animate->RandomAnimate( "idle" );
	fx->PostEvent( EV_Remove, 1.0f );
	
	if ( cool_item )
	{
		cool_item->setOrigin( org );
		cool_item->PostEvent( EV_Show, 0.1f );
		cool_item->edict->s.renderfx |= RF_VIEWLENSFLARE;
		
		if ( cool_dialog.length() )
		{
			Sound( cool_dialog, CHAN_DIALOG );
		}
	}
}


void Player::ShowHeuristics( Event *ev )
{
	
	if ( p_heuristics )
		p_heuristics->ShowHeuristics( this );
}


void Player::SpecialMoveChargeEnd( Event* ev )
{
	specialMoveCharge = 0.0f;
	specialMoveChargeTime = 0.0f;
	specialMoveEndTime = 0.0f;
}


void Player::SpecialMoveChargeStart( Event* ev )
{
	specialMoveCharge = level.time;
	specialMoveEndTime = level.time + specialMoveChargeTime;
}


void Player::SpecialMoveChargeTime( Event* ev )
{
	if ( ev->NumArgs() > 0 )
		specialMoveChargeTime = ev->GetFloat( 1 );
}


void Player::StartCoolItem( Event *ev )
{
	level.ai_on = false;
	takedamage = DAMAGE_NO;
	level.playerfrozen = true;
	G_StartCinematic();
	
	assert( ( Camera * )cool_camera == NULL );
	if ( music_current_mood != mood_success )
	{
		ChangeMusic( "success", MusicMood_NumToName( music_current_mood ), false );
	}
	
	cool_camera = new Camera();
	cool_camera->SetOrbitHeight( 150.0f );
	cool_camera->Orbit( this, 200.0f, this, -90.0f );
	cool_camera->Cut( NULL );
	SetCamera( cool_camera, 1.0f );
}


void Player::StartCoolItemAnim( void )
{
	movecontrol = MOVECONTROL_ABSOLUTE;
	
	if ( cool_item && cool_anim.length() )
	{
		SetAnim( cool_anim, legs );
		cool_anim = "";
	}
}


void Player::StartFakePlayer( void )
{
	Actor * fake;
	
	if ( !fakePlayer_active )
	{
		return;
	}
	
	fakePlayer_active = false;
	
	fakePlayer = new Actor;
	
	if ( !fakePlayer )
		return;
	
	fake = fakePlayer;
	
	CloneEntity( fake, this );
	
	fake->SetTargetName( "fakeplayer" );
	fake->ProcessEvent( EV_Actor_AIOff );
	
	fake->turnThinkOn();
	
	
	fake->maxs[2] -= STEPSIZE;
	fake->setSize( mins, maxs );
	
	fake->takedamage = DAMAGE_NO;
	
	this->hideModel();
	this->ProcessEvent( EV_Sentient_TurnOffShadow );
	this->flags |= FL_IMMOBILE;
	setSolidType( SOLID_NOT );
	
	PostEvent( EV_Player_Done, 0.0f );
}


void Player::StopCoolItem( Event *ev )
{
	if ( cool_item && cool_anim.length() )
	{
		State * newState;
		
		newState = statemap_Torso->FindState( "DO_COOL_ITEM_ANIM" );
		if ( newState )
		{
			currentState_Torso = newState;
			return;
		}
	}
	
	level.ai_on = true;
	
	takedamage = DAMAGE_AIM;
	
	level.playerfrozen = false;
	
	G_StopCinematic();
	
	cool_item = NULL;
	
	if ( cool_camera )
	{
		SetCamera( NULL, 1.0f );
		delete cool_camera;
		cool_camera = NULL;
	}
}


void Player::StopWatchingEntity( void )
{
	entity_to_watch = NULL;
	maximumAngleToWatchedEntity = 0.0f;
	client->ps.useCameraFocalPoint = false;
	client->ps.pm_flags &= ~PMF_CAMERA_VIEW;
}

void Player::StopWatchingEntity( Event *ev )
{
	StopWatchingEntity();
}


void Player::SwitchWeaponMode( Event *ev )
{
	Weapon* weapon = 0;
	weapon = GetActiveWeapon( WEAPON_DUAL );
	if (!weapon )
		return;
	
	weapon->SwitchMode();
}


void Player::UpdateEntityStateForClient( entityState_t *state )
{
	int i;

	if ( !state )
		return;


	if ( !multiplayerManager.inMultiplayer() )
		return;


	if ( !client->pers.mp_lowBandwidth )
		return;


	if ( state->clientNum != ENTITYNUM_NONE )
	{
		VectorClear( state->netangles );
	}


	for ( i = 0 ; i < NUM_BONE_CONTROLLERS ; i++ )
	{
		VectorClear( state->bone_angles[ i ] );
	}
}


void Player::UpdateObjectiveStatus( void )
{
	client->ps.objectiveStates = _objectiveStates;
	client->ps.informationStates = _informationStates;
	client->ps.objectiveNameIndex = _objectiveNameIndex;
}


void Player::UpdatePlayerStateForClient( playerState_t *state )
{
	if ( !state )
		return;


	if ( !multiplayerManager.inMultiplayer() )
		return;

	if ( !multiplayerManager.isPlayerSpectator( this, SPECTATOR_TYPE_FOLLOW ) && !mp_savingDemo )
	{
		VectorClear( state->viewangles );
	}
}


void Player::UseDualWield( Event *ev )
{
	if ( dual_wield_weaponlist.NumObjects() )
	{
		WeaponSetItem *dw;
		
		dw = dual_wield_weaponlist.ObjectAt( 1 );
		
		useWeapon( dw->name, dw->hand );
		dual_wield_weaponlist.RemoveObjectAt( 1 );
		delete dw;
	}
	else
	{
		dual_wield_active = false;  
	}
}


void Player::Vote( Event *ev )
{
	str vote;

	if ( multiplayerManager.inMultiplayer() )
	{
		if ( ev->NumArgs() != 1 )
		{
			multiplayerManager.HUDPrint( entnum, "$$Usage$$: vote <1|0|y|n>" );
			return;
		}

		vote = ev->GetString( 1 );

		multiplayerManager.vote( this, vote );
	}
}


void Player::WatchEntity( Event *ev )
{
	if ( camera || ( currentState_Torso->getCameraType() != CAMERA_BEHIND ) )
		return;

	watchEntityForEntireDuration = false;

	if ( ev->NumArgs() > 1 )
	{
		Event *stopWatchingEvent = new Event( EV_Player_StopWatchingEntity );
		stopWatchingEvent->AddEntity( ev->GetEntity( 1 ) );
		const float timeToWatch = ev->GetFloat( 2 );
		PostEvent( stopWatchingEvent, timeToWatch );

		maximumAngleToWatchedEntity = ev->GetFloat( 3 );

		if ( ev->NumArgs() > 3 )
		{
			watchEntityForEntireDuration = ev->GetBoolean( 4 );
		}

	}

	entity_to_watch = ev->GetEntity( 1 );
}


void Player::WeaponCommand( Event *ev )
{
	weaponhand_t   hand;
	Weapon         *weap;
	int            i;
	
	if ( ev->NumArgs() < 2 )
		return;
	
	hand = WeaponHandNameToNum( ev->GetString( 1 ) );
	weap = GetActiveWeapon( hand );
	
	if ( !weap )
		return;
	
	Event *e;
	e = new Event( ev->GetToken( 2 ) );
	
	for( i=3; i<=ev->NumArgs(); i++ )
		e->AddToken( ev->GetToken( i ) );
	
	weap->ProcessEvent( e );
}


void Player::WeaponsHolstered( void )
{
}


void Player::WeaponsNotHolstered( void )
{
}


void Player::addFinishingMove( Event *ev )
{
	str state, dirstr;
	float coneangle = 45.0f;
	float dist = 64.0f;
	float eyaw = 0.0f;
	float chance = 1.0f;
	
	state = ev->GetString( 1 );
	dirstr = ev->GetString( 2 );
	
	if ( ev->NumArgs() > 2 )
		coneangle = ev->GetFloat( 3 );
	if ( ev->NumArgs() > 3 )
		dist = ev->GetFloat( 4 );
	if ( ev->NumArgs() > 4 )
		eyaw = ev->GetFloat( 5 );
	if ( ev->NumArgs() > 5 )
		chance = ev->GetFloat( 6 );

	FinishingMove *fm = new FinishingMove();
	fm->statename = state;
	if ( dirstr == "front" )
		fm->direction = 0;
	else if ( dirstr == "behind" )
		fm->direction = 1;
	else if ( dirstr == "left" )
		fm->direction = 2;
	else if ( dirstr == "right" )
		fm->direction = 3;
	else 
		fm->direction = 0; 
	fm->coneangle = coneangle;
	fm->distance = dist;
	fm->enemyyaw = eyaw;
	fm->chance = chance;

	finishingMoveList.AddObject(fm);
}


void Player::addHud( const str &hudName )
{
	if ( !_hudList.ObjectInList( hudName ) )
	{
		addHudToClient( hudName );

		_hudList.AddObject( hudName );
	}
}

void Player::addHud( Event *ev )
{
	addHud( ev->GetString( 1 ) );
}


void Player::addHudToClient( const str &hudName )
{
	str commandString;

	commandString = "stufftext \"ui_addhud ";
	commandString += hudName;
	commandString += "\"\n";

	gi.SendServerCommand( entnum, "%s", commandString.c_str() );
}


void Player::addPowerupEffect( PowerupBase *powerup )
{
	str modelName;
	str tagName;
	float modelRemoveTime;
	str shaderName;

	if ( !powerup )
		return;


	powerup->getModelToAttachOnUse( modelName, tagName, modelRemoveTime );

	if ( modelName.length() > 0 )
	{
		Event *attachModel;

		attachModel = new Event( EV_AttachModel );

		attachModel->AddString( modelName );
		attachModel->AddString( tagName );
		attachModel->AddFloat( 1.0f );
		attachModel->AddString( "" );
		attachModel->AddInteger( 0 );
		attachModel->AddFloat( modelRemoveTime );

		ProcessEvent( attachModel );
	}
}


void Player::allowActionMusic( bool allowActionMusic )
{
	_allowActionMusic = allowActionMusic;
}


void Player::allowMusicDucking( bool allowMusicDucking )
{
	_allowMusicDucking = allowMusicDucking;
}


void Player::applyWeaponSpeedModifiers( int *moveSpeed )
{
	Weapon *weap;

	weap = GetActiveWeapon( WEAPON_DUAL );

	if ( weap )
	{
		*moveSpeed *= (int) weap->getMoveSpeedModifier();
	}

}




void Player::clearAllHuds( void )
{
	int i;
	str commandString;
	str hudName;

	for( i = 1 ; i <= _hudList.NumObjects() ; i++ )
	{
		hudName = _hudList.ObjectAt( i );

		removeHudFromClient( hudName );
	}

	_hudList.ClearObjectList();
}


void Player::clearFinishingMove( Event *ev )
{
	int i;
	for ( i=1; i<=finishingMoveList.NumObjects(); i++ )
	{
		FinishingMove *fm = finishingMoveList.ObjectAt(i);
		delete fm;
	}
	finishingMoveList.FreeObjectList();
}


void Player::clearTempAttachments( void )
{
	int i;
	Entity *child;

	if ( bind_info )
	{
		for( i = 0; i < MAX_MODEL_CHILDREN ; i++ )
		{
			if ( bind_info->children[ i ] == ENTITYNUM_NONE )
				continue;

			child = ( Entity * )G_GetEntity( bind_info->children[ i ] );

			if ( child )
			{
				if ( child->isSubclassOf( Projectile ) || child->CancelEventsOfType( EV_Remove ) )
				{
					child->PostEvent( EV_Remove, 0.0f );			
				}
			}
		}
	}
}


void Player::clearVoteText( void )
{
	_voteText = "";
}


void Player::doFinishingMove( Event *ev )
{
	movecontrol = MOVECONTROL_ABSOLUTE;
	
	if ( _finishState.length() )
	{
		State* newState = statemap_Torso->FindState( _finishState );
		if ( newState )
		{
			EvaluateState( newState );
			_doingFinishingMove = true;
			_finishActor->SetState("FINISH_ME");
		}
		else
			gi.WDPrintf( "Could not find state %s on doFinishingMove\n", _finishState.c_str() );
	}
}


void Player::doUseEntity( Event *ev )
{
	Entity *ent = NULL;
	Event *event = NULL;
	
	if ( !atobject )
		return;
	
	ent = (Entity *)atobject;
	if ( !ent->hasUseData() )
		return;
	
	if ( ent->useData->getUseThread().length() > 0 )
		ExecuteThread(ent->useData->getUseThread().c_str(), true, ent);
	
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( ent->isSubclassOf(Item) && gpm->hasProperty(ent->getArchetype(), "noautopickup"))
	{
		Item *item = (Item *)ent;
		handlePickupItem(item);
		return;
	}
	
	event = new Event( EV_Use );
	event->AddEntity( this );
	ent->ProcessEvent( event );
}


void Player::doneUseEntity( Event *ev )
{
	_usingEntity = false;
	level.playerfrozen = false;
	_useEntityStartTimer = 0.0f;
	SetState("STAND", "STAND");
}


void Player::dropPowerup( void )
{
	if ( _powerup )
	{
		if ( _powerup->canDrop() )
		{
			_powerup->spawn( centroid );
		}

		removePowerup();
	}
}


void Player::dropRune( Event *ev )
{
	if ( _rune )
	{
		dropRune();
	}
}

void Player::dropRune( void )
{
	if ( _rune )
	{
		setItemText( _rune->getIcon(), va( "$$Dropping$$ $$Item-%s$$", _rune->getName().c_str() ) );

		_rune->spawn( centroid );

		removeRune();
	}
	else
	{
		multiplayerManager.playerCommand( this, "dropItem", "" );
	}
}


void Player::equipItems( Event *ev )
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();

	str itemName, modelName;
	
	itemName	= gpm->getStringValue("CurrentPlayer.DualWield", "name");
	modelName	= gpm->getStringValue(itemName, "model");
	if ( modelName.length() )
		giveItem(modelName);

	itemName	= gpm->getStringValue("CurrentPlayer.TwoHanded", "name");
	modelName	= gpm->getStringValue(itemName, "model");
	if ( modelName.length() )
		giveItem(modelName);

	itemName	= gpm->getStringValue("CurrentPlayer.Ranged", "name");
	modelName	= gpm->getStringValue(itemName, "model");
	if ( modelName.length() )
		giveItem(modelName);

	itemName	= gpm->getStringValue("CurrentPlayer.Ring", "name");
	modelName	= gpm->getStringValue(itemName, "model");
	if ( modelName.length() )
		giveItem(modelName);

	itemName	= gpm->getStringValue("CurrentPlayer.Amulet", "name");
	modelName	= gpm->getStringValue(itemName, "model");
	if ( modelName.length() )
		giveItem(modelName);

	useWeapon("DualWield", WEAPON_RIGHT);
}


void Player::forceMoveType( Event *ev )
{
	str moveTypeName;

	moveTypeName = ev->GetString( 1 );

	if ( Q_stricmp( moveTypeName, "secret" ) == 0 )
		_forcedMoveType = PM_SECRET_MOVE_MODE;
	else if ( Q_stricmp( moveTypeName, "3rdPerson" ) == 0 )
		_forcedMoveType = PM_3RD_PERSON;
	else if ( Q_stricmp( moveTypeName, "none" ) == 0 )
		_forcedMoveType = PM_NONE;
}


void Player::forceTimeScale( Event *ev )
{
	float timescale = 1.0f;
	if ( ev->NumArgs() > 0 )
		timescale = ev->GetFloat( 1 );

	char tmp[32];
	snprintf(tmp, sizeof(tmp), "timescale %f", timescale);

	G_SendCommandToPlayer(edict, tmp);
}


void Player::freezePlayer( Event *ev )
{
	bool freeze = true;
	if ( ev->NumArgs() > 0 )
		freeze = ev->GetBoolean( 1 );

	level.playerfrozen = freeze;
}


void Player::handleDialogSetup( Entity* entity, const str& soundName )
{
	CancelEventsOfType( EV_Player_ClearDialog );
	
	
	ClearDialog();

	if ( gi.SoundLength( soundName.c_str() ) >= 0 )
	{
		_dialogEntnum = entity->entnum;
		_dialogSoundIndex = gi.soundindex( soundName.c_str() );

		PostEvent( EV_Player_ClearDialog, gi.SoundLength( soundName.c_str() ) );
	}
	else
	{
		ProcessEvent( EV_Player_ClearDialog );
	}
}


void Player::handlePickupItem( Item *item )
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str type = item->getArchetype();
	str invslot = gpm->getStringValue(type, "invslot");
	if ( invslot.length() )
	{
		float quantity = gpm->getFloatValue(invslot, "quantity");
		quantity += 1.0f;
		gpm->setFloatValue(invslot, "quantity", quantity);
		gpm->setStringValue(invslot, "name", type);
	}
	else
	{
		str slotName = getFreeInventorySlot();
		if ( !slotName.length() )
			return;
		
		gpm->setStringValue(slotName, "name", type);
	}
	
	str snd = gpm->getStringValue(type + ".Pickup", "wav");
	if ( snd.length() )
	{
		int channel = CHAN_BODY;
		float volume = -1.0f;
		float mindist = -1.0f;
		if ( gpm->hasProperty(type + ".Pickup","channel") )
			channel = (int)gpm->getFloatValue(type + ".Pickup", "channel");
		if ( gpm->hasProperty(getArchetype() + ".Pickup","volume") )
			volume = (int)gpm->getFloatValue(getArchetype() + ".Pickup", "volume");
		if ( gpm->hasProperty(getArchetype() + ".Pickup","mindist") )
			mindist = (int)gpm->getFloatValue(getArchetype() + ".Pickup", "mindist");
		item->Sound(snd, channel, volume, mindist);
	}
	
	item->ProcessEvent(EV_Remove);
}


void Player::handleTextDialogSetup( const str& soundName )
{
	CancelEventsOfType( EV_Player_ClearTextDialog );
	
	ClearTextDialog();
	
	if ( gi.SoundLength( soundName.c_str() ) >= 0 )
	{
		_dialogTextSoundIndex = gi.soundindex( soundName.c_str() );
		PostEvent( EV_Player_ClearTextDialog, gi.SoundLength( soundName.c_str() ) );
	}
	else
	{
		ProcessEvent( EV_Player_ClearTextDialog );
	}
}


void Player::hudPrint( Event *ev )
{
	hudPrint( ev->GetString( 1 ) );
}

void Player::hudPrint( const str &string )
{
	str command;


	command = "hudprint \"";
	command += string;
	command += "\"\n";


	gi.SendServerCommand( edict - g_entities, "%s", command.c_str() );
}


void Player::immobilizePlayer( Event *ev )
{
	bool freeze = true;

	if ( ev->NumArgs() > 0 )
		freeze = ev->GetBoolean( 1 );

	if ( freeze )
		flags |= FL_IMMOBILE;
	else
		flags &= ~FL_IMMOBILE;

}


void Player::isPlayerOnGround( Event *ev )
{
	float onGround;

	if ( client->ps.walking ) 
		{
		onGround = 1.0f;
		}
	else
		{
		onGround = 0.0f;
		}


	ev->ReturnFloat( onGround );
}


void Player::joinTeam( Event *ev )
{
	str teamName;

	teamName = ev->GetString( 1 );

	multiplayerManager.joinTeam( this, teamName );
}


void Player::killAllDialog( Event *ev )
{
	killAllDialog();
}


void Player::modelChanged( void )
{
	if ( _powerup )
	{
		removePowerupEffect( _powerup );
		addPowerupEffect( _powerup );
	}

	if ( _rune )
	{
		removePowerupEffect( _rune );
		addPowerupEffect( _rune );
	}
}


void Player::multiplayerCommand( Event *ev )
{
	str command;
	str parm_shadow1;

	command = ev->GetString( 1 );
	parm_shadow1    = ev->GetString( 2 );

	multiplayerManager.playerCommand( this, command, parm_shadow1 );
}


void Player::nextGameplayAnim( Event *ev )
{
	if ( ev->NumArgs() < 1 )
		return;

	str objname = ev->GetString( 1 );
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str scopestr = "CurrentPlayer.ChainedMoves";
	if ( !gpm->hasObject(scopestr) )
		return;

	int max = (int)gpm->getFloatValue(scopestr, "value");
	max++; 
	if ( _gameplayAnimIdx < max )
		_gameplayAnimIdx++;
	else
		_gameplayAnimIdx = 1;

	if ( _gameplayAnimIdx > 4 )
		_gameplayAnimIdx = 1;
}


void Player::notifyPlayerOfMultiplayerEvent( const char *eventName, const char *eventItemName, Player *eventPlayer )
{
}


void Player::pointOfView( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		bool pov = ev->GetBoolean( 1 );
		if ( pov == _isThirdPerson )
			return;
	}
	
	if ( _isThirdPerson )
	{
		G_SendCommandToPlayer(edict, "cg_3rd_person 0");
		_isThirdPerson = false;
	}
	else
	{
		G_SendCommandToPlayer(edict, "cg_3rd_person 1");
		_isThirdPerson = true;
	}
}


void Player::removeHud( Event *ev )
{
	removeHud( ev->GetString( 1 ) );
}

void Player::removeHud( const str &hudName )
{
	if ( _hudList.ObjectInList( hudName ) )
	{
		removeHudFromClient( hudName );

		_hudList.RemoveObject( hudName );
	}
}


void Player::removeHudFromClient( const str &hudName )
{
	str commandString;

	commandString = "stufftext \"ui_removehud ";
	commandString += hudName;
	commandString += "\"\n";

	gi.SendServerCommand( entnum, "%s", commandString.c_str() );
}


void Player::removePowerup( void )
{
	if ( _powerup )
	{
		removePowerupEffect( _powerup );

		delete _powerup;
		_powerup = NULL;
	}
}


void Player::removePowerupEffect( PowerupBase *powerup )
{
	str modelName;
	str tagName;
	float modelRemoveTime;
	str shaderName;

	if ( !powerup )
		return;


	powerup->getModelToAttachOnUse( modelName, tagName, modelRemoveTime );

	if ( modelName.length() > 0 )
	{
		Event *removeAttachedModel;

		removeAttachedModel = new Event( EV_RemoveAttachedModel );
		
		removeAttachedModel->AddString( tagName );
		removeAttachedModel->AddFloat( 0.0f );
		removeAttachedModel->AddString( modelName );

		ProcessEvent( removeAttachedModel );
	}


	powerup->getShaderToDisplayOnUse( shaderName );

	if ( shaderName.length() > 0 )
	{
		clearCustomShader( shaderName.c_str() );
	}
}


void Player::removePowerupEvent( Event *ev )
{
	removePowerup();
}


void Player::removeRosterTeammate1( Event* ev )
{
}


void Player::removeRosterTeammate2( Event* ev )
{
}


void Player::removeRosterTeammate4( Event* ev )
{
}


void Player::removeRune( void )
{
	if ( _rune )
	{
		removePowerupEffect( _rune );

		delete _rune;
		_rune = NULL;
	}
}


void Player::sendAllHudsToClient( void )
{
	int i;
	str commandString;
	str hudName;

	if ( _needToSendHuds )
	{
		for( i = 1 ; i <= _hudList.NumObjects() ; i++ )
		{
			hudName = _hudList.ObjectAt( i );

			addHudToClient( hudName );
		}
	}

	_needToSendHuds = false;
}


void Player::setAttackType( Event *ev )
{
	_attackType = ev->GetString( 1 );
}


void Player::setBackupModel( Event *ev )
{
	setBackupModel( ev->GetString( 1 ) );
}


void Player::setCanTransferEnergy( Event * )
{
	_canTransferEnergy = true;
}


void Player::setDisableInventory( Event* ev )
{
	if(ev->NumArgs() > 0)
	{
		disableInventory();
		return;
	}
	else if( ev->GetFloat(1))
	{
		enableInventory();
		return;
	}

	disableInventory();
}


void Player::setDisableUseWeapon( Event* ev)
{
	if(ev->NumArgs() > 0)
	{
		_disableUseWeapon = ev->GetBoolean(1);
	}
	else
	{
		_disableUseWeapon = true;
	}


	Com_Printf("DisableUseWeapon %d", _disableUseWeapon);

}


void Player::setDoDamageScreenFlash( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		_doDamageScreenFlash = ev->GetBoolean( 1 );
	else
		_doDamageScreenFlash = true;
}


void Player::setGameplayAnim( Event *ev )
{
	int val = 1;
	if ( ev->NumArgs() > 0)
		val = ev->GetInteger( 1 );

	_gameplayAnimIdx = val;
}


void Player::setHoldableItem( HoldableItem *holdableItem )
{
	removeHoldableItem();

	_holdableItem = holdableItem;
}


void Player::setPowerup( Powerup *powerup )
{

	if ( _powerup && powerup && ( _powerup->getName() == powerup->getName() ) && _powerup->canStack() && powerup->canStack() )
	{
		float newTimeLeft;
		newTimeLeft = _powerup->getTimeLeft() + powerup->getTimeLeft();
		_powerup->setTimeLeft( newTimeLeft );
	}
	else
	{
		removePowerup();

		CancelEventsOfType( EV_Player_RemovePowerup );

		_powerup = powerup;

		if ( !_powerup )
			return;

		addPowerupEffect( _powerup );
	}

	setItemText( _powerup->getIcon(), va( "$$Using$$ $$Item-%s$$", _powerup->getName().c_str() ) );
}


void Player::setRune( Rune *rune )
{
	removeRune();

	_rune = rune;

	if ( !_rune )
		return;

	addPowerupEffect( _rune );


	setItemText( rune->getIcon(), va( "$$Using$$ $$Item-%s$$", _rune->getName().c_str() ) );
}


void Player::setTargeted( bool targeted )
{
	if ( targeted )
		{
		gi.SendServerCommand( entnum, "stufftext \"ui_addhud targetedhud\"\n");
		}
	else
		{
		gi.SendServerCommand( entnum, "stufftext \"ui_removehud targetedhud\"\n");
		}
}


void Player::setValidPlayerModel( Event *ev )
{
	_validPlayerModel = true;
}


void Player::setVoteText( const str &voteText )
{
	_voteText = voteText;
}


void Player::skillChanged(const str& objname)
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(objname) )
		return;

	float value = gpm->getFloatValue(objname, "value");
	if ( objname == "CurrentPlayer.BonusHP" && value > 0.0f )
	{
		if ( gpm->hasFormula("BonusHP") )
		{
			GameplayFormulaData fd(this);
			float hp = gpm->calculate("BonusHP", fd);
			float maxhp = max_health + ( max_health * hp );
			setMaxHealth(maxhp);
		}
	}
}


void Player::transferEnergy( void )
{
	Event *armorEvent;


	if ( !_canTransferEnergy )
		return;


	if ( _nextEnergyTransferTime > level.time )
		return;

	_nextEnergyTransferTime = level.time + level.frametime;


	if ( AmmoCount( "Plasma" ) < 1 )
		return;


	if ( GetArmorValue() >= 100 )
		return;


	armorEvent = new Event( EV_Sentient_GiveArmor );

	armorEvent->AddString( "BasicArmor" );
	armorEvent->AddFloat( 1.0f );

	ProcessEvent( armorEvent );	


	UseAmmo( "Plasma", 1 );
}


void Player::turnTowardsEntity( Event *ev )
{
	Entity *entity;
	Vector dir;
	Vector angles_shadow1;
	Event *turnEvent;
	float currentYaw;
	float yawDiff;

	entity = ev->GetEntity( 1 );

	if ( !entity )
		return;


	dir = entity->origin - origin;
	dir.normalize();

	angles_shadow1 = dir.toAngles();


	currentYaw = anglemod( v_angle[ YAW ] );

	yawDiff = angles_shadow1[ YAW ] - currentYaw;

	yawDiff = AngleNormalize180( yawDiff );

	
	turnEvent = new Event( EV_Player_Turn );
	turnEvent->AddFloat( yawDiff );
	ProcessEvent( turnEvent );
}


void Player::usePlayer( Event *ev )
{
	Player *usingPlayer = NULL;
	Equipment *equipment = NULL;
	Entity *owner;

	if ( multiplayerManager.inMultiplayer() )
	{
		Entity *entity;

		entity = ev->GetEntity( 1 );

		if ( entity->isSubclassOf( Player ) )
		{
			usingPlayer = (Player *)entity;
		}
		else if ( entity->isSubclassOf( Equipment ) )
		{
			equipment = (Equipment *)entity;

			owner = equipment->GetOwner();

			if ( owner && owner->isSubclassOf( Player ) )
			{
				usingPlayer = (Player *)owner;
			}
		}

		multiplayerManager.playerUsed( this, usingPlayer, equipment );
	}
}


void Player::warp( Event *ev )
{
	setOrigin( ev->GetVector( 1 ) );
	NoLerpThisFrame();
	client->ps.pm_flags |= PMF_TIME_TELEPORT;
	CameraCut();
}

qboolean Player::GetCrouch( void )
{
	if ( last_ucmd.upmove < 0 ) 
		return true;
	return false;
}


qboolean Player::GetProjDetonate()
{
	Weapon *weapon;

	if ( projdetonate )
		return true;

	weapon = GetActiveWeapon( WEAPON_DUAL );

	if ( !weapon )
		return false;

	if ( ( weapon->GetFireType( FIRE_MODE1 ) == FT_TRIGGER_PROJECTILE ) && ( isButtonDown( BUTTON_ATTACKLEFT ) ) )
		return true;

	if ( ( weapon->GetFireType( FIRE_MODE2 ) == FT_TRIGGER_PROJECTILE ) && ( isButtonDown( BUTTON_ATTACKRIGHT ) ) )
		return true;

	return false;
}


qboolean Player::GetTagPositionAndOrientation( int tagnum, orientation_t *new_or )
{
	int            i;
	orientation_t  tag_or;
	vec3_t         axis[3];
	
	tag_or = gi.Tag_OrientationEx( edict->s.modelindex,
			CurrentAnim( legs ),
			CurrentFrame( legs ),
			tagnum & TAG_MASK,
			edict->s.scale,
			edict->s.bone_tag,
			edict->s.bone_quat,
			0,
			0,
			1.0f,
			( edict->s.anim & ANIM_BLEND ) != 0,
			( edict->s.torso_anim & ANIM_BLEND ) != 0,
			CurrentAnim( torso ),
			CurrentFrame( torso ),
			0,
			0,
			1.0f
			);
	
	AnglesToAxis( angles, axis );
	VectorCopy( origin, new_or->origin );
	
	for ( i=0;  i<3; i++ )
		VectorMA( new_or->origin, tag_or.origin[i], axis[i], new_or->origin );
	
	MatrixMultiply( tag_or.axis, axis, new_or->axis );
	return true;
}


qboolean Player::IsDualWeaponActive( void )
{
	if(GetActiveWeapon( WEAPON_LEFT ) || GetActiveWeapon( WEAPON_RIGHT ))
	{
		return qfalse;
	}
	
	return qtrue;
}


int Player::TakePoints(int numPoints)
{
	points -= numPoints;
	if ( points < 0 )
		points = 0;
	
	return points;
}


qboolean Player::WeaponsOut( void )
{
	return ( GetActiveWeapon( WEAPON_LEFT ) || GetActiveWeapon( WEAPON_RIGHT ) || GetActiveWeapon( WEAPON_DUAL ) );
}


qboolean Player::check22degreeslope( Conditional &condition )
{
	if ( client->ps.walking && client->ps.groundPlane && ( client->ps.groundTrace.plane.normal[ 2 ] < SLOPE_22_MAX ) &&
		( client->ps.groundTrace.plane.normal[ 2 ] >= SLOPE_22_MIN ) )
	{
		return qtrue;
	}
	
	return qfalse;
}


qboolean Player::check45degreeslope( Conditional &condition )
{
	if ( client->ps.walking && client->ps.groundPlane && ( client->ps.groundTrace.plane.normal[ 2 ] < SLOPE_45_MAX ) &&
		( client->ps.groundTrace.plane.normal[ 2 ] >= SLOPE_45_MIN ) )
	{
		return qtrue;
	}
	
	return qfalse;
}


qboolean Player::checkEndAnimChain( Conditional &condition )
{
	if ( condition.numParms() < 1 )
		return false;
	
	str objname = condition.getParm( 1 );
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str scopestr = "CurrentPlayer.ChainedMoves";
	if ( !gpm->hasObject(scopestr) )
		return false;
	
	int max = (int)gpm->getFloatValue(scopestr, "value");
	max++; 
	if ( _gameplayAnimIdx > max )
		return false; 
	
	if ( _gameplayAnimIdx == max )
		return true;
	
	return false;
}


qboolean Player::checkHasAnim( Conditional &condition )
{
	str animName;

	if ( condition.numParms() < 1 )
		return false;
	
	animName = condition.getParm( 1 );

	if ( gi.Anim_NumForName( edict->s.modelindex, animName.c_str() ) < 0 )
		return false;
	else
		return true;
}


qboolean Player::checkIsWeaponControllingProjectile( Conditional &condition )
{
	Weapon *weapon;

		
	weapon = GetActiveWeapon( WEAPON_DUAL );

	if ( weapon )
	{
		if ( weapon->getControllingProjectile() )
			return true;
		else
			return false;
	}

	return false;
}


qboolean Player::checkOnLadder( Conditional &condition )
{
	return _onLadder;
}


qboolean Player::checkPropChance( Conditional &condition )
{
	str propname;
	str objname = condition.getParm( 1 );
	if ( condition.numParms() > 1 )
		propname = condition.getParm( 2 );
	
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str scopestr = getArchetype() + "." + objname;
	if ( !gpm->hasObject(scopestr) )
		return false;
	
	float chance;
	if ( propname.length() )
		chance = gpm->getFloatValue(scopestr, propname);
	else
		chance = gpm->getFloatValue(scopestr, "value");
	
	return ( G_Random() <= chance );
}


qboolean Player::checkPropExists( Conditional &condition )
{
	str objname = condition.getParm( 1 );
	str propname = condition.getParm( 2 );
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	str scopestr = getArchetype() + "." + objname;
	if ( !gpm->hasProperty(scopestr, propname) )
		return false;
	
	return true;
}


qboolean Player::checkWeaponType( Conditional &condition )
{
	if ( condition.numParms() < 1 )
		return false;
	
	str type = condition.getParm( 1 );
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	Weapon* weap = GetActiveWeapon(WEAPON_RIGHT);
	if ( !weap )
		weap = GetActiveWeapon(WEAPON_LEFT);
	if ( !weap )
		return false;
	
	if ( !gpm->hasObject(weap->getArchetype()) )
		return false;
	
	str weaptype = gpm->getStringValue(weap->getArchetype(), "class");
	if ( weaptype == type )
		return true;
	
	return false;
}


qboolean Player::checkaccumulatedpain( Conditional &condition )
{
	float threshold = (float)atof( condition.getParm( 1 ) );
	
	if ( accumulated_pain >= threshold )
	{
		accumulated_pain = 0; 
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkanimdone_legs( Conditional &condition )
{
	if ( ( edict->s.anim & ANIM_BLEND ) == 0 )
		return true;
	
	return animdone_Legs;
}


qboolean Player::checkanimdone_torso( Conditional &condition )
{
	if ( ( edict->s.torso_anim & ANIM_BLEND ) == 0 )
		return true;

	return animdone_Torso;
}


qboolean Player::checkanyweaponactive( Conditional &condition )
{
	weaponhand_t   hand;
	Weapon         *weap;
	
	hand = WeaponHandNameToNum( condition.getParm( 1 ) );
	
	if ( hand == WEAPON_ERROR )
		return qfalse;
	
	weap = GetActiveWeapon( hand );
	return ( weap != NULL );
}


qboolean Player::checkattackblocked( Conditional &condition )
{
	if ( attack_blocked )
	{
		attack_blocked = false;
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkattackbuttonleft( Conditional &condition )
{
	if ( level.playerfrozen || ( flags & FL_IMMOBILE ) )
	{
		return qfalse;
	}
	
	if ( multiplayerManager.inMultiplayer() && ( !multiplayerManager.isFightingAllowed() || multiplayerManager.isPlayerSpectator( this ) ) )
	{
		return qfalse;
	}
	else
	{
		return ( last_ucmd.buttons & BUTTON_ATTACKLEFT );
	}
}


qboolean Player::checkattackbuttonright( Conditional &condition )
{
	if ( level.playerfrozen || ( flags & FL_IMMOBILE ) )
	{
		return qfalse;
	}
	
	if ( multiplayerManager.inMultiplayer() && ( !multiplayerManager.isFightingAllowed() || multiplayerManager.isPlayerSpectator( this ) ) )
	{
		return qfalse;
	}
	else
	{
		return ( last_ucmd.buttons & BUTTON_ATTACKRIGHT );
	}
}


qboolean Player::checkattackright( Conditional &condition )
{
	Weapon *weapon;
	
	if ( level.playerfrozen || ( flags & FL_IMMOBILE ) )
	{
		return qfalse;
	}
	
	if ( multiplayerManager.inMultiplayer() && ( !multiplayerManager.isFightingAllowed() || multiplayerManager.isPlayerSpectator( this ) ) )
	{
		return qfalse;
	}
	
	
	if ( client->ps.leanDelta != 0 )
		return false;
	
	
	if ( last_ucmd.buttons & BUTTON_ATTACKRIGHT )
	{
		last_attack_button = BUTTON_ATTACKRIGHT;
		
		weapon = GetActiveWeapon( WEAPON_RIGHT );
		if ( weapon )
		{
			weapon->CheckReload( FIRE_MODE2 );
			return qtrue;
		}
		
		weapon = GetActiveWeapon( WEAPON_LEFT );
		if ( weapon )
		{
			weapon->CheckReload( FIRE_MODE2 );
			return qtrue;
		}
		
		weapon = GetActiveWeapon( WEAPON_DUAL );
		if ( weapon )
		{
			weapon->CheckReload( FIRE_MODE2 );
			return qtrue;
		}
		
		return qfalse;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkatuseanim( Conditional &condition )
{
	if ( atobject && atobject->isSubclassOf( UseAnim ) )
	{
		return ( ( UseAnim * )( Entity * )atobject )->canBeUsed( this );
	}
	
	return qfalse;
}


qboolean Player::checkatuseentity(Conditional &condition)
{
	if ( atobject )
	   {
	   Entity *ent = (Entity*)atobject;
	   if ( ent->hasUseData() )
		   return qtrue;
	   }

	return qfalse;
}


qboolean Player::checkatuseobject( Conditional &condition )
{
	if ( atobject && atobject->isSubclassOf( UseObject ) )
	{
		return ( ( UseObject * )( Entity * )atobject )->canBeUsed( origin, yaw_forward );
	}
	
	return qfalse;
}


qboolean Player::checkblockdelay( Conditional &condition )
{
	float t = (float)atof ( condition.getParm( 1 ) );
	return ( level.time > ( attack_blocked_time + t ) );
}


qboolean Player::checkcanfall( Conditional &condition )
{
	return canfall;
}


qboolean Player::checkcanstand( Conditional &condition )
{
	Vector newmins( mins );
	Vector newmaxs( maxs );
	Vector tmporg( origin );
	trace_t trace;
	
	tmporg[ 2 ] += 2.0f;
	newmins[ 2 ] = MINS_Z;
	newmaxs[ 2 ] = MAXS_Z;
	
	trace = G_Trace( tmporg, newmins, newmaxs, tmporg, this, edict->clipmask, true, "checkcanstand" );
	if ( trace.startsolid )
	{
		return qfalse;
	}
	
	return qtrue;
}


qboolean Player::checkcanturn( Conditional &condition )
{
	float yaw;
	Vector oldang( v_angle );
	qboolean result;
	
	yaw = (float)atof( condition.getParm( 1 ) );
	
	v_angle[ YAW ] = (float)( ( int )( anglemod( v_angle[ YAW ] + yaw ) / 22.5f ) * 22.5f );
	SetViewAngles( v_angle );
	
	result = qtrue; 
	
	SetViewAngles( oldang );
	
	return result;
}


qboolean Player::checkcharavailable( Conditional &condition )
{
	str character = condition.getParm( 1 );
	Actor *act = Actor::FindActorByName(character.c_str());
	if ( act )
		return true;
	
	return false;
}


qboolean Player::checkdualweaponreadytofire( Conditional &condition )
{
	firemode_t  mode      = FIRE_MODE1;
	str         weaponName="None";
	Weapon      *weapon = GetActiveWeapon( WEAPON_DUAL );
	qboolean    ready;


	mode = WeaponModeNameToNum( condition.getParm( 1 ) );

	if ( condition.numParms() > 1 )
		weaponName = condition.getParm( 2 );


	if ( client->ps.leanDelta != 0 )
		return false;

	if ( !weapon )
		return qfalse;

	if ( condition.numParms() > 1 )
	{
		if ( strcmp( weaponName, weapon->item_name ) )
		{
			return qfalse;
		}
	}

	ready = weapon->ReadyToFire( mode );


	if ( !weapon->isModeNoDelay( mode ) )
	{
		if ( weapon->next_fire_time[mode] > level.time )
			return qfalse;

		if ( weapon->next_fire_time[FIRE_MODE1] > level.time )
			return qfalse;

		if ( weapon->next_fire_time[FIRE_MODE2] > level.time )
			return qfalse;
	}

	return( ready );
}


qboolean Player::checkdualweapons( Conditional &condition )
{
	int i,j;
	
	for ( i=1; i<=condition.numParms(); i++ )
	{
		str weaponName;
		weaponName = condition.getParm( i );
		
		for ( j=1; j<=dual_wield_weaponlist.NumObjects(); j++ )
		{
			WeaponSetItem *dw;
			dw = dual_wield_weaponlist.ObjectAt( j );
			
			if ( dw->name == weaponName )
            {
				goto out;
            }
		}
		return qfalse;
out:
		;
	}
	
	return qtrue;
}


qboolean Player::checkdualwield( Conditional &condition )
{
	
	return ( dual_wield_active );
}


qboolean Player::checkfacingdownslope( Conditional &condition )
{
	float groundyaw;
	float yawdelta;
	int which;
	
	groundyaw = ( int )vectoyaw( client->ps.groundTrace.plane.normal );
	yawdelta = anglemod( v_angle.y - groundyaw );
	which = ( ( int )yawdelta + 45 ) / 90;
	
	return ( ( which == 0 ) || ( which == 4 ) );
}


qboolean Player::checkfacingupslope( Conditional &condition )
{
	float groundyaw;
	float yawdelta;
	int which;
	
	groundyaw = ( int )vectoyaw( client->ps.groundTrace.plane.normal );
	yawdelta = anglemod( v_angle.y - groundyaw );
	which = ( ( int )yawdelta + 45 ) / 90;
	
	return ( which == 2 );
}


qboolean Player::checkfakeplayeractive( Conditional &condition )
{
	return fakePlayer_active;
}


qboolean Player::checkfinishingmove( Conditional &condition )
{
	if ( _finishActor )
		return true;
	return false;
}


qboolean Player::checkgroundentity( Conditional &condition )
{
	return ( groundentity != NULL );
}


qboolean Player::checkhardimpact( Conditional &condition )
{
	return hardimpact;
}


qboolean Player::checkhasdualweapon( Conditional & condition )
{
	return IsDualWeaponActive();
}


qboolean Player::checkhasweapon( Conditional &condition )
{
	if ( condition.numParms() > 0 )
	{
		weaponhand_t hand = WeaponHandNameToNum( condition.getParm( 1 ) );
		if ( GetActiveWeapon( hand ) )
			return true;
		else
			return false;
	}
	
	return WeaponsOut();
}


qboolean Player::checkincomingmeleeattack( Conditional &condition )
{
	return incomingMeleeAttack;
}


qboolean Player::checkinvehicle( Conditional &condition )
{
	
	if(vehicle == NULL || vehicle->getArchetype() != condition.getParm(1))
		return qfalse;
	
	return qtrue;
	
}


qboolean Player::checkjumpflip( Conditional &condition )
{
	return velocity.z < ( sv_currentGravity->value * 0.5f );
}


qboolean Player::checkleanleft(Conditional &condition)
{
	if( client->ps.leanDelta > 0 )
	{
		return qtrue;
	}
	
	return qfalse;
}


qboolean Player::checkleanright( Conditional &condition )
{
	
	if(client->ps.leanDelta < 0)
	{
		return qtrue;
	}
	
	return qfalse;
}


qboolean Player::checkleftleghigh( Conditional &condition )
{
	float groundyaw;
	float yawdelta;
	int which;
	
	groundyaw = ( int )vectoyaw( client->ps.groundTrace.plane.normal );
	yawdelta = anglemod( v_angle.y - groundyaw );
	which = ( ( int )yawdelta + 45 ) / 90;
	
	return ( which == 1 );
}


qboolean Player::checkloopuseobject( Conditional &condition )
{
	if ( useitem_in_use && useitem_in_use->isSubclassOf( UseObject ) )
	{
		return ( ( UseObject * )( Entity * )useitem_in_use )->Loop();
	}
	
	return qfalse;
}


qboolean Player::checknewweapon( Conditional &condition )
{
	Weapon * weapon;
	
	weapon = GetNewActiveWeapon();
	
	if ( weapon && _disableUseWeapon == false)
		return qtrue;
	else
		return qfalse;
}


qboolean Player::checkpaindirection( Conditional &condition )
{
	if ( pain_dir == Pain_string_to_int( condition.getParm( 1 ) ) )
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkpainthreshold( Conditional &condition )
{
	float threshold = (float)atof( condition.getParm( 1 ) );
	
	if ( (  pain >= threshold ) && ( level.time > nextpaintime ) )
	{
		accumulated_pain = 0; 
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkpaintype( Conditional &condition )
{
	if ( pain_type == MOD_NameToNum( condition.getParm( 1 ) ) )
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkpull( Conditional &condition )
{
	if ( atobject && atobject->isSubclassOf( PushObject ) && ( atobject_dist < ( PUSH_OBJECT_DISTANCE + 15.0f ) ) )
	{
		Vector dir;
		
		dir = atobject_dir * -64.0f;
		return ( ( PushObject * )( Entity * )atobject )->canPush( dir );
	}
	
	return qfalse;
}


qboolean Player::checkpush( Conditional &condition )
{
	if ( atobject && atobject->isSubclassOf( PushObject ) && ( atobject_dist < ( PUSH_OBJECT_DISTANCE + 15.0f ) ) )
	{
		Vector dir;
		
		dir = atobject_dir * 8.0f;
		return ( ( PushObject * )( Entity * )atobject )->canPush( dir );
	}
	
	return qfalse;
}


qboolean Player::checkputawayboth( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	return weapon && weapon->GetPutaway();
}


qboolean Player::checkputawayleft( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_LEFT );
	
	return weapon && weapon->GetPutaway();
}


qboolean Player::checkputawayright( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_RIGHT );
	
	return weapon && weapon->GetPutaway();
}


qboolean Player::checkrightleghigh( Conditional &condition )
{
	float groundyaw;
	float yawdelta;
	int which;
	
	groundyaw = ( int )vectoyaw( client->ps.groundTrace.plane.normal );
	yawdelta = anglemod( v_angle.y - groundyaw );
	which = ( ( int )yawdelta + 45 ) / 90;
	
	return ( which == 3 );
}


qboolean Player::checksolidforward( Conditional &condition )
{
	float dist = (float)atof( condition.getParm( 1 ) );
	Vector end( centroid + ( yaw_forward * dist ) );
	
	trace_t trace = G_Trace( centroid, Vector( mins.x, mins.y, -8.0f ), Vector( maxs.x, maxs.y, 8.0f ),
		end, this, MASK_SOLID, true, "Player::checksolidforward" );
	
	return ( trace.fraction < 0.7f );
}


qboolean Player::checkspecialmovecharge( Conditional &condition )
{
	if ( specialMoveCharge >= specialMoveEndTime )
		return true;
	return false;
}


qboolean Player::checkstance( Conditional &condition )
{
	int stancenum;
	stancenum = (int)atoi( condition.getParm( 1 ) );
	
	if ( stancenum == stanceNumber )
		return true;
	
	return false;
}


qboolean Player::checkstancechangedlegs( Conditional &condition )
{
	return changedStanceLegs;
}


qboolean Player::checkstancechangedtorso( Conditional &condition )
{
	return changedStanceTorso;
}


qboolean Player::checkstatename( Conditional &condition )
{
	str part      = condition.getParm( 1 );
	str statename = condition.getParm( 2 );
	
	if ( currentState_Legs && !part.icmp( "legs" ) )
	{
		return ( !statename.icmpn( currentState_Legs->getName(), statename.length() ) );
	}
	else if ( !part.icmp( "torso" ) )
	{
		return ( !statename.icmpn( currentState_Torso->getName(), statename.length() ) );
	}
	
	return qfalse;
}


qboolean Player::checktargetacquired( Conditional &condition )
{
	return (targetEnemy != NULL);
}


qboolean Player::checkthirdperson(Conditional &condition)
{
	return _isThirdPerson;
}


qboolean Player::checktouchuseanim( Conditional &condition )
{
	if ( toucheduseanim )
	{
		return ( ( UseAnim * )( Entity * )toucheduseanim )->canBeUsed( this );
	}
	
	return qfalse;
}


qboolean Player::checkturnedleft( Conditional &condition )
{
	return yawing_left;
}


qboolean Player::checkturnedright( Conditional &condition )
{
	return yawing_right;
}


qboolean Player::checkuseanimfinished( Conditional &condition )
{
	return ( useanim_numloops <= 0 );
}


qboolean Player::checkuseweapon( Conditional &condition )
{
	const char     *weaponName;
	const char     *parm_shadow1;

	weaponhand_t   hand;
	Weapon         *weap;

	weap = GetNewActiveWeapon();
	parm_shadow1 = condition.getParm( 1 );

	if ( !str::icmp( parm_shadow1, "ERROR" ) )
	{
		if ( weap )
			warning( "Player::checkuseweapon", "%s does not have a valid RAISE_WEAPON state\n", weap->item_name.c_str() );
		else
			warning( "Player::checkuseweapon", "New Active weapon does not exist\n" );
		
		ClearNewActiveWeapon();
		return qtrue;
	}
	
	hand = WeaponHandNameToNum( parm_shadow1 );
	
	if ( hand == WEAPON_ERROR )
		return qfalse;
	
	weaponName = condition.getParm( 2 );
	
	if (
        ( weap != NULL ) &&
        ( GetNewActiveWeaponHand() == hand ) &&
        ( !stricmp( weap->item_name, weaponName ) )
		)
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkusingentity(Conditional &condition)
{
	return _usingEntity;
}


qboolean Player::checkweaponactive( Conditional &condition )
{
	const char *weaponName;
	weaponhand_t hand;
	
	weaponName = condition.getParm( 2 );
	hand = WeaponHandNameToNum( condition.getParm( 1 ) );
	
	if ( hand == WEAPON_ERROR )
		return qfalse;
	
	Weapon *weapon = GetActiveWeapon( hand );
	
	return ( weapon && !strcmp( weaponName, weapon->item_name ) );
}


qboolean Player::checkweaponcanreload( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( !weapon )
		return qfalse;

	return weapon->canReload();
}


qboolean Player::checkweapondonefiring( Conditional &condition )
{
	Weapon      *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( !weapon )
		return qtrue;
	
	return weapon->IsDoneFiring();
}


qboolean Player::checkweaponfiretimer( Conditional &condition )
{
	firemode_t mode = FIRE_MODE1;
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( weapon->GetSwitchMode() )
		mode = weapon->GetCurMode();
	else 
		mode = WeaponModeNameToNum( condition.getParm( 1 ) );
	
	if ( weapon->GetFireTime(mode) > level.time )
		return qfalse;
	
	return qtrue;
}


qboolean Player::checkweaponforcereload( Conditional &condition )
{
	return ( last_ucmd.buttons & BUTTON_RELOAD ) != 0;
}


qboolean Player::checkweaponfullclip( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( !weapon )
		return qtrue;
	
	return weapon->HasFullClip();
}


qboolean Player::checkweaponhasammo( Conditional &condition )
{
	weaponhand_t   hand;
	Weapon         *weap;
	firemode_t     mode = FIRE_MODE1;
	int numShots = 1;
	
	hand = WeaponHandNameToNum( condition.getParm( 1 ) );
	
	if ( condition.numParms() > 1 )
		mode = WeaponModeNameToNum( condition.getParm( 2 ) );
	
	if ( condition.numParms() > 2 )
		numShots = atoi( condition.getParm( 3 ) );
	
	if ( hand == WEAPON_ERROR )
		return qfalse;
	
	weap = GetActiveWeapon( hand );
	
	if ( !weap )
		return qfalse;
	else
		return ( weap->HasAmmo( mode, numShots ) );
}


qboolean Player::checkweaponhasfullammo( Conditional &condition )
{
	str ammoName;

	ammoName = condition.getParm( 1 );

	if ( AmmoCount( ammoName ) == MaxAmmoCount( ammoName ) )
		return true;
	else
		return false;
}


qboolean Player::checkweaponhasinvammo( Conditional &condition )
{
	weaponhand_t   hand;
	Weapon         *weap;
	firemode_t     mode = FIRE_MODE1;
	
	hand = WeaponHandNameToNum( condition.getParm( 1 ) );
	
	if ( condition.numParms() > 1 )
		mode = WeaponModeNameToNum( condition.getParm( 2 ) );
	
	if ( hand == WEAPON_ERROR )
		return qfalse;
	
	weap = GetActiveWeapon( hand );
	
	if ( !weap )
		return qfalse;
	else
		return ( weap->HasInvAmmo( mode ) );
}


qboolean Player::checkweaponinmode( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	str mode;
	
	if ( weapon )
	{
		if ( condition.numParms() > 0 )
			mode = condition.getParm( 1 );
		if ( weapon->GetCurMode() == WeaponModeNameToNum( mode ) )
			return qtrue;
		else
			return qfalse;
	}
	
	return qfalse;
}


qboolean Player::checkweaponlowered( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( !weapon )
		return true;
	else
		return ( weapon->weaponstate == WEAPON_LOWERING );
}


qboolean Player::checkweaponreadytofire( Conditional &condition )
{
	firemode_t     mode = FIRE_MODE1;
	str            weaponName="None";
	weaponhand_t   hand;
	qboolean    ready;
	
	if ( level.playerfrozen || ( flags & FL_IMMOBILE ) )
	{
		return qfalse;
	}
	
	hand = WeaponHandNameToNum( condition.getParm( 1 ) );
	
	if ( hand == WEAPON_DUAL )
	{
		gi.DPrintf( "This check should only be used for single handed weapons\n" );
		return qfalse;
	}
	
	if ( condition.numParms() > 1 )
		weaponName = condition.getParm( 2 );
	
	if ( hand == WEAPON_ERROR )
		return qfalse;
	
	Weapon *weapon = GetActiveWeapon( hand );
	
	if ( !weapon )
		return qfalse;
	
	if ( condition.numParms() > 1 )
	{
		if ( strcmp( weaponName, weapon->item_name ) )
		{
			return qfalse;
		}
	}
	
	ready = weapon->ReadyToFire( mode );
	return( ready );
}


qboolean Player::checkweaponreload( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( weapon ) {
		if ( weapon->weaponstate == WEAPON_RELOADING ) {
			return qtrue;
		}
	}
		
		return qfalse;
}


qboolean Player::checkweaponsholstered( Conditional &condition )
{
	
	if (
		( holsteredWeapons[WEAPON_DUAL] ) ||
		( holsteredWeapons[WEAPON_LEFT] ) ||
		( holsteredWeapons[WEAPON_RIGHT] )
		)
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}


qboolean Player::checkweaponswitchmode( Conditional &condition )
{
	Weapon *weapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( weapon ) {
		if ( weapon->weaponstate == WEAPON_SWITCHINGMODE ) {
			return qtrue;
		}
	}
		
		return qfalse;
}


float Player::getDefaultFov( void )
{
	if ( multiplayerManager.inMultiplayer() && multiplayerManager.checkFlag( MP_FLAG_FIXED_FOV ) )
		return sv_defaultFov->value;
	else
		return userFov;
}


HoldableItem* Player::getHoldableItem( void )
{
	return _holdableItem;
}


int Player::getSkill( void )
{
	return _skillLevel;
}


usercmd_t Player::GetLastUcmd(void) 
{
	return last_ucmd;
}


bool Player::GetProjectileLaunchAngles( Vector &launchAngles, const Vector &launchPoint, const float initialSpeed, const float gravity_shadow1 ) const
{
	Entity const *target_shadow1 = GetTarget();
	if ( target_shadow1 && targetEnemyLocked)
	{
		const Vector targetPoint( target_shadow1->centroid );

		Trajectory projectileTrajectory( launchPoint, targetPoint, initialSpeed, gravity_shadow1 * -sv_currentGravity->value );
		if ( projectileTrajectory.GetTravelTime() > 0.0f )
		{
			launchAngles.setPitch( projectileTrajectory.GetLaunchAngle() );

			Vector direction( targetPoint - launchPoint );
			direction.z = 0.0f;
			direction.normalize();

			launchAngles.setYaw( direction.toYaw() );
			launchAngles.setRoll( 0.0f );
			return true;
		}

	}
	return false;
}


void Player::SetClientViewAngles( const Vector &position, const float cameraoffset, const Vector &ang, const Vector &vel, const float camerafov ) const
{
	if ( client->ps.useCameraFocalPoint )
	{
		client->ps.cameraFocalPoint[ 0 ] = position[ 0 ];
		client->ps.cameraFocalPoint[ 1 ] = position[ 1 ];
		client->ps.cameraFocalPoint[ 2 ] = position[ 2 ] + cameraoffset + 18.0f;
	}

	client->ps.origin[ 0 ] = position[ 0 ];
	client->ps.origin[ 1 ] = position[ 1 ];
	client->ps.origin[ 2 ] = position[ 2 ];
	
	client->ps.viewheight = (int) cameraoffset;

	client->ps.viewangles[ 0 ] = ang[ 0 ];
	client->ps.viewangles[ 1 ] = ang[ 1 ];
	client->ps.viewangles[ 2 ] = ang[ 2 ];
	
	client->ps.velocity[ 0 ] = vel[ 0 ];
	client->ps.velocity[ 1 ] = vel[ 1 ];
	client->ps.velocity[ 2 ] = vel[ 2 ];
	
	client->ps.fov = camerafov;
}


bool Player::canRegenerate( void )
{
	if ( _powerup && !_powerup->canOwnerRegenerate() )
		return false;

	if ( _rune && !_rune->canOwnerRegenerate() )
		return false;

	return true;
}


bool Player::hasRune( void )
{
	if ( _rune )
		return true;
	else
		return false;
}


bool Player::needToSendAllHudsToClient( void )
{
	if ( _needToSendHuds && _started )
		return true;
	else
		return false;
}


bool Player::useWeapon( const char *weaponname, weaponhand_t hand )
{
	Weapon *weapon;
	str name(weaponname);

	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( gpm )
	{
		str objectName("CurrentPlayer.");
		objectName += name ;
		if ( gpm->hasProperty( objectName, "name" ) )
		{
			name = gpm->getStringValue( objectName, "name" );
		}
	}

	weapon = ( Weapon * )FindItem( name );

	if ( !weapon )
	{
		warning( "Player::useWeapon", "Player does not have weapon %s", weaponname );
		return false;
	}

	return useWeapon( weapon, hand );
}


qboolean Player::ShieldActive( void )
{
	return shield_active;
}


qboolean Player::LargeShieldActive( void )
{
	Weapon *weapon;
	qboolean large_shield_active=false;
	
	weapon = GetActiveWeapon( WEAPON_LEFT );
	if ( weapon && !str::icmp( weapon->item_name, "LargeShield" ) )
		large_shield_active = true;
	
	return shield_active && large_shield_active;
}

const str Player::getFreeInventorySlot()
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();

	int i;
	char tmpstr[128];
	for ( i=1; i<15; i++ )
	{
		sprintf(tmpstr,"PartyInventory.Slot%d",i);
		str invitem = gpm->getStringValue(tmpstr, "name");
		if ( invitem == "Empty" )
			return tmpstr;
	}
	
	return "";
}


// Player::CopyStats is NOT MISSING -- it does not exist in retail (no symbol in


void Player::ClearTextDialog( void )
{
	_dialogTextSoundIndex = -1;
}

void Player::killAllDialog()
{
	Actor *theActor;
	int i;

	for ( i = 1; i <= SleepList.NumObjects(); i++ )
		{
		theActor = (Actor*)SleepList.ObjectAt( i );
		theActor->StopDialog();
		}		

	for ( i = 1; i <= ActiveList.NumObjects(); i++ )
		{
		theActor = (Actor*)ActiveList.ObjectAt( i );
		theActor->StopDialog();
		}
}

void Player::setBackupModel( const str &modelName )
{
	gi.setviewmodel( edict, modelName );
}

void Player::SetupDialog( Entity *entity, const str &soundName )
{
	if( gi.SoundLength( soundName.c_str() ) <= 0 )
		return;

	if ( entity )
	{
		handleDialogSetup(entity, soundName);
	}
	else
	{
		handleTextDialogSetup( soundName );
	}
}

bool Player::useWeapon( Weapon *weapon, weaponhand_t hand )
{
	Weapon * activeWeapon;
	
	if ( !weapon )
	{
		warning( "Player::useWeapon", "Null weapon used.\n" );
		return false;
	}
	
	
	if ( !weapon->HasAmmo( FIRE_MODE1 ) && !weapon->HasAmmo( FIRE_MODE2 ) && !weapon->GetUseNoAmmo() )
	{
		Sound( "snd_noammo" );
		return false;
	}
	
	
	if ( ( hand == WEAPON_DUAL ) && ( weapon->GetHand() != hand ) )
    {
		warning( "Player::useWeapon", "Weapon %s is not allowed in %s", weapon->getName().c_str(), WeaponHandNumToName( hand ) );
		return false;
    }
	else if ( ( weapon->GetHand() != WEAPON_ANY ) && ( weapon->GetHand() != hand ) )
    {
		warning( "Player::useWeapon", "Weapon %s is not allowed in %s", weapon->getName().c_str(), WeaponHandNumToName( hand ) );
		return false;
    }
	
	if ( weapon->GetHand() == WEAPON_DUAL )
	{
		activeWeapon = GetActiveWeapon( WEAPON_LEFT );
		if ( activeWeapon )
			activeWeapon->PutAway();
		activeWeapon = GetActiveWeapon( WEAPON_RIGHT );
		if ( activeWeapon )
			activeWeapon->PutAway();
	}
	
	activeWeapon = GetActiveWeapon( WEAPON_DUAL );
	
	if ( activeWeapon && activeWeapon != weapon )
	{
		activeWeapon->PutAway();
		if ( activeWeapon == weapon )
		{
			return false;
		}
	}
	
	activeWeapon = GetActiveWeapon( hand );
	
	if ( ( activeWeapon == weapon ) && ( !newActiveWeapon.weapon || newActiveWeapon.weapon == weapon ) )
	{
		return true;
	}
	
	
	if ( IsActiveWeapon( weapon ) )
	{
		weapon->PutAway();
	}
	
	newActiveWeapon.weapon = weapon;
	newActiveWeapon.hand   = hand;
	return true;
}

const str Player::getPainShader( meansOfDeath_t mod, bool takeArmorIntoAccount )
{
	if ( ( takeArmorIntoAccount ) && ( GetArmorValue() >= 100 ) )
		return "ArmorDeflection";
	else
		return getPainShader( MOD_NumToName( mod )  );
}

const str Player::getPainShader( const char *MODName )
{
	GameplayManager *gpm;
	str modName;
	const char *defaultName = "default";

	gpm = GameplayManager::getTheGameplayManager();

	if ( !gpm )
		return str("");

	modName = "MOD";
	modName += MODName;

	if ( !gpm->hasObject( modName ) )
	{
		if ( stricmp( MODName, defaultName ) == 0 )
			return "";
		else
			return getPainShader( defaultName );
	}

	return gpm->getStringValue( modName, "PainShaderName" );

}
