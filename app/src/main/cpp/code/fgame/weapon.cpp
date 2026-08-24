
#include "_pch_cpp.h"
#include "entity.h"
#include "usedata.h"
#include "item.h"
#include "weapon.h"
#include "scriptmaster.h"
#include "sentient.h"
#include "misc.h"
#include "specialfx.h"
#include "actor.h"
#include "weaputils.h"
#include "player.h"
#include "decals.h"
#include "mp_manager.hpp"
#include "../qcommon/gameplaymanager.h"

#include "g_local.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_ea.h"
#include "be_ai_char.h"
#include "be_ai_chat.h"
#include "be_ai_gen.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"
#include "be_ai_weap.h"
#include "ai_main.h"
extern bot_state_t	*botstates[MAX_CLIENTS];

#define TargetIdleTime 0.2

Event EV_Weapon_Shoot
(
	"shoot",
	EV_DEFAULT,
	"S",
	"mode",
	"Shoot the weapon"
 );
Event EV_Weapon_DoneRaising
(
    "ready",
    EV_TIKIONLY,
    NULL,
    NULL,
    "Signals the end of the ready animation so the weapon can be used"
);

Event EV_Weapon_DoneAnimating
(
	"weapon_done_animate",
	EV_CODEONLY,
	NULL,
	NULL,
	"Signals the end of the ready animation so the weapon can be used"
);
Event EV_Weapon_DoneFiring
(
	"donefiring",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Signals the end of the fire animation"
);
Event EV_Weapon_Idle
(
	"idle",
	EV_DEFAULT,
	NULL,
	NULL,
	"Puts the weapon into an idle state"
);
Event	EV_Weapon_SecondaryUse
(
	"secondaryuse",
	EV_DEFAULT,
	NULL,
	NULL,
	"Puts the weapon into its secondary mode of operation"
);
Event EV_Weapon_DoneReloading
(
	"donereloading",
	EV_CODEONLY,
	NULL,
	NULL,
	"Signals the end of the reload animation"
);
Event EV_Weapon_DoneReloadingBurst
(
	"doneReloadingBurst",
	EV_CODEONLY,
	NULL,
	NULL,
	"Signals the end of the reload animation for a burst"
);
Event EV_Weapon_SetAmmoClipSize
(
	"clipsize",
	EV_TIKIONLY,
	"i",
	"ammoClipSize",
	"Set the amount of rounds a clip of the weapon holds"
);
Event EV_Weapon_SetAmmoInClip
(
	"ammo_in_clip",
	EV_TIKIONLY,
	"i",
	"ammoInClip",
	"Set the amount of ammo in the clip"
);
Event EV_Weapon_ProcessModelCommands
(
	"process_mdl_cmds",
	EV_CODEONLY,
	NULL,
	NULL,
	"Forces a processing of the init commands for the model"
);
Event EV_Weapon_SetMaxRange
(
	"maxrange",
	EV_TIKIONLY,
	"f",
	"maxRange",
	"Set the maximum range of a weapon so the AI knows how to use it"
);
Event EV_Weapon_SetMinRange
(
	"minrange",
	EV_TIKIONLY,
	"f",
	"minRange",
	"Set the minimum range of a weapon so the AI knows how to use it"
);
Event EV_Weapon_ActionIncrement
(
	"actionincrement",
	EV_TIKIONLY,
	"i",
	"actionLevelIncrement",
	"Set the action level increment of the weapon.\n"
	"When the weapon is fired, it will raise the action level by this amount"
);
Event EV_Weapon_NotDroppable
(
	"notdroppable",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Makes a weapon not droppable"
);
Event EV_Weapon_SetAimAnim
(
	"setaimanim",
	EV_TIKIONLY,
	"si",
	"aimAnimation aimFrame",
	"Set the aim animation and frame for when a weapon fires"
);
Event EV_Weapon_SetFireType
(
	"firetype",
	EV_TIKIONLY,
	"s",
	"firingType",
	"Set the firing type of the weapon (projectile or bullet)"
);
Event EV_Weapon_SetProjectile
(
	"projectile",
	EV_TIKIONLY,
	"s",
	"projectileModel",
	"Set the model of the projectile that this weapon fires"
);
Event EV_Weapon_SetBulletDamage
(
	"bulletdamage",
	EV_TIKIONLY,
	"f",
	"bulletDamage",
	"Set the damage that the bullet causes"
);
Event EV_Weapon_SetBulletKnockback
(
	"bulletknockback",
	EV_TIKIONLY,
	"f",
	"bulletKnockback",
	"Set the knockback that the bullet causes"
);
Event EV_Weapon_SetBulletCount
(
	"bulletcount",
	EV_TIKIONLY,
	"f",
	"bulletCount",
	"Set the number of bullets this weapon shoots when fired"
);
Event EV_Weapon_SetBulletRange
(
	"bulletrange",
	EV_TIKIONLY,
	"f",
	"bulletRange",
	"Set the range of the bullets"
);
Event EV_Weapon_SetBulletSpread
(
	"bulletspread",
	EV_TIKIONLY,
	"ffFFF",
	"bulletSpreadX bulletSpreadY endSpreadX endSpreadY spreadTime",
	"Set the max spread of the bullet in the x and y axis, optionally with a different end spread and time interval"
);
Event EV_Weapon_SetRange
(
	"range",
	EV_TIKIONLY,
	"f",
	"range",
	"Set the range of the weapon"
);
Event EV_Weapon_Hand
(
	"hand",
	EV_TIKIONLY,
	"s",
	"weaponHand",
	"Set the hand the weapon can be wielded in (lefthand, righthand, both, or any)"
);
Event EV_Weapon_Mode
(
	"modeset",
	EV_TIKIONLY,
	"SSSSSSSSS",
	"arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8 arg9",
	"Set a value for a mode by passing commands through"
);
Event EV_Weapon_AmmoType
(
	"ammotype",
	EV_TIKIONLY,
	"s",
	"name",
	"Set the type of ammo this weapon uses"
);
Event EV_Weapon_StartAmmo
(
	"startammo",
	EV_TIKIONLY,
	"i",
	"amount",
	"Set the starting ammo of this weapon"
);
Event EV_Weapon_AmmoBoost
(
	"ammoBoost",
	EV_TIKIONLY,
	"i",
	"amount",
	"Set the ammo given to a sentient that picks up this weapon but already had a weapon of this type."
);
Event EV_Weapon_AmmoRequired
(
	"ammorequired",
	EV_TIKIONLY,
	"i",
	"amount",
	"Set the amount of ammo this weapon requires to fire"
);
Event EV_Weapon_MaxChargeTime
(
	"maxchargetime",
	EV_TIKIONLY,
	"i",
	"time",
	"Set the maximum time the weapon may be charged up"
);
Event EV_Weapon_GiveStartingAmmo
(
	"startingammotoowner",
	EV_CODEONLY,
	NULL,
	NULL,
	"Internal event used to give ammo to the owner of the weapon"
);
Event EV_Weapon_GiveAmmoBoost
(
	"giveAmmoBoost",
	EV_CODEONLY,
	NULL,
	NULL,
	"Internal event used to give ammo boost to the player that just picked up this weapon."
);
Event EV_Weapon_AutoAim
(
	"autoaim",
	EV_TIKIONLY,
	"FF",
	"selection_angle lockon_angle",
	"Turn on auto aiming for the weapon"
);
Event EV_Weapon_Crosshair
(
	"crosshair",
	EV_TIKIONLY,
	"b",
	"bool",
	"Turn on/off the crosshair for this weapon"
);
Event EV_Weapon_TorsoAim
(
	"torsoaim",
	EV_DEFAULT,
	"b",
	"bool",
	"Turn on/off the torsoaim for this weapon"
);
Event EV_Weapon_SetQuiet
(
	"quiet",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Makes the weapon make no noise."
);
Event EV_Weapon_SetLoopFire
(
	"loopfire",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Makes the weapon fire by looping the fire animation."
);
Event EV_Weapon_LeftAttachToTag
(
	"leftattachtotag",
	EV_TIKIONLY,
	"s",
	"tagname",
	"Set the name of the tag to attach this to it's owner when wielded in the left hand."
);
Event EV_Weapon_RightAttachToTag
(
	"rightattachtotag",
	EV_TIKIONLY,
	"s",
	"tagname",
	"Set the name of the tag to attach this to it's owner when wielded in the right hand."
);
Event EV_Weapon_DualAttachToTag
(
	"dualattachtotag",
	EV_TIKIONLY,
	"s",
	"tagname",
	"Set the name of the tag to attach this to it's owner when wielded dual handed."
);
Event EV_Weapon_HolsterTagLeft
(
	"leftholstertag",
	EV_TIKIONLY,
	"s",
	"tagname",
	"Set the name of the tag to attach this to when the weapon is put away from the left hand."
);
Event EV_Weapon_HolsterTagRight
(
	"rightholstertag",
	EV_TIKIONLY,
	"s",
	"tagname",
	"Set the name of the tag to attach this to when the weapon is put away from the right hand."
);
Event EV_Weapon_HolsterTagDual
(
	"dualholstertag",
	EV_TIKIONLY,
	"s",
	"tagname",
	"Set the name of the tag to attach this to when the weapon is put away from dual handed"
);
Event EV_Weapon_LeftHolsterAngles
(
	"leftholsterangles",
	EV_TIKIONLY,
	"v",
	"angles",
	"Set the angles of this weapon when it's attached to the left holster"
);
Event EV_Weapon_RightHolsterAngles
(
	"rightholsterangles",
	EV_TIKIONLY,
	"v",
	"angles",
	"Set the angles of this weapon when it's attached to the right holster"
);
Event EV_Weapon_DualHolsterAngles
(
	"dualholsterangles",
	EV_TIKIONLY,
	"v",
	"angles",
	"Set the angles of this weapon when it's attached to the dual holster"
);
Event EV_Weapon_HolsterScale
(
	"holsterscale",
	EV_TIKIONLY,
	"f",
	"scale",
	"Set the scale of the weapon when it's attached to the holster"
);
Event EV_Weapon_WeildedScale
(
	"weildedScale",
	EV_TIKIONLY,
	"f",
	"scale",
	"Sets the scale of the weapon when it's weilded"
);
Event EV_Weapon_AutoPutaway
(
	"autoputaway",
	EV_TIKIONLY,
	"b",
	"bool",
	"Set the weapon to be automatically put away when out of ammo"
);
Event EV_Weapon_UseNoAmmo
(
	"usenoammo",
	EV_TIKIONLY,
	"b",
	"bool",
	"Set the weapon to be able to be used when it's out of ammo"
);
Event EV_Weapon_SetMeansOfDeath
(
	"meansofdeath",
	EV_TIKIONLY,
	"s",
	"meansOfDeath",
	"Set the meansOfDeath of the weapon."
);
Event EV_Weapon_SetWorldHitSpawn
(
	"worldhitspawn",
	EV_TIKIONLY,
	"s",
	"modelname",
	"Set a model to be spawned when the weapon strikes the world."
);
Event EV_Weapon_MakeNoise
(
	"makenoise",
	EV_TIKIONLY,
	"FB",
	"noise_radius force",
	"Makes the weapon make noise that actors can hear."
);
Event EV_Weapon_SetViewModel
(
	"weaponviewmodel",
	EV_TIKIONLY,
	"s",
	"filename",
	"Set the view model name"
);
Event EV_Weapon_DonePutaway
(
	"doneputaway",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Called when the weapon is done with it's putaway anim"
);
Event EV_Weapon_SetRegenAmmo
(
	"regenammo",
	EV_TIKIONLY,
	"ii",
	"regenamount regentime",
	"Specifics that a weapon regenerates ammo over time"
);
Event EV_Weapon_SetRegenOnlyWhenIdle
(
	"regenonlywhenidle",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Specifics that the weapon only regenerates ammo when idle, must still use regen ammo to set how much."
);
Event EV_Weapon_ChangeIdle
(
	"changeidle",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Changes the idle animation"
);
Event EV_Weapon_DrawBowStrain
(
	"drawbowstrain",
	EV_DEFAULT,
	NULL,
	NULL,
	"Starts the bow draw strain animation"
);
Event EV_Weapon_AltDrawBowStrain
(
	"altdrawbowstrain",
	EV_DEFAULT,
	NULL,
	NULL,
	"Starts the alternate bow draw strain animation"
);
Event EV_Weapon_SetAccuracy
(
	"setaccuracy",
	EV_TIKIONLY,
	"sfffff",
	"firemode stoppedac acchange walkac runac crouchac",
	"Sets the accuracy of the weapon"
);
Event EV_Weapon_SetReticuleTime
(
	"setreticuletime",
	EV_TIKIONLY,
	"f",
	"reticuletime",
	"Reticule time"
);
Event EV_Weapon_SetZoomFOV
(
	"setzoomfov",
	EV_TIKIONLY,
	"f",
	"zoomfov",
	"Zoom FOV"
);
Event EV_Weapon_IncrementZoomFOV
(
	"inczoomfov",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Increments the zoom fov"
);
Event EV_Weapon_SetZoomStage
(
	"setzoomstage",
	EV_TIKIONLY,
	"ff",
	"fov1 fov2",
	"Sets the fov stage"
);
Event EV_Weapon_SetStartZoom
(
	"setstartzoom",
	EV_TIKIONLY,
	"f",
	"startzoom",
	"Sets the start zoom fov"
);
Event EV_Weapon_SetEndZoom
(
	"setendzoom",
	EV_TIKIONLY,
	"f",
	"endzoom",
	"Sets the end zoom fov"
);
Event EV_Weapon_SetZoomTime
(
	"setzoomtime",
	EV_TIKIONLY,
	"f",
	"zoomtime",
	"Sets the time it takes to zoom from startzoom to endzoom"
);
Event EV_Weapon_SetAimType
(
	"setaim",
	EV_DEFAULT,
	"s",
	"aimtype",
	"Sets the aimtype of the weapon (changes according to player state)"
);
Event EV_Weapon_SetFireTimer
(
	"setfiretimer",
	EV_TIKIONLY,
	"sf",
	"mode mintime",
	"Specifies the minimum time between shots"
);
Event EV_Weapon_UseSameClip
(
	"usesameclip",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Specifies that ammo comes out of the same clip in all fire modes"
);
Event EV_Weapon_SetMaxModes
(
	"maxmode",
	EV_TIKIONLY,
	"s",
	"maxmode",
	"Overrides the maximum number of modes for this weapon (default is 2)"
);
Event EV_Weapon_SetSwitchMode
(
	"switchmode",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Specifies that this is a switch mode weapon (right button switches)"
);
Event	EV_Weapon_DoneSwitching
(
	"doneswitching",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Signals the end of the switching to mode animation"
);
Event	EV_Weapon_DoneSwitchToMiddle
(
	"doneswitchtomiddle",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Signals the end of the switching to 'neutral' animation"
);
Event	EV_Weapon_TargetIdle
(
	"targetidle",
	EV_CODEONLY,
	NULL,
	NULL,
	"This weapon have a specific on-target idle"
);
Event EV_Weapon_TargetIdleThink
(
	"targetidlethink",
	EV_DEFAULT,
	NULL,
	NULL,
	"Think event to check for target on target idle weapons"
);
Event EV_Weapon_SetBurstMode
(
	"burstmode",
	EV_TIKIONLY,
	"i",
	"burstcount",
	"Set this mode to be burst mode, that uses burstcount ammo"
);
Event EV_Weapon_StartFiring
(
	"startfiring",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Sets the time that the weapon starts firing (for spread computations)"
);
Event EV_Weapon_FinishedFiring
(
	"finishedfiring",
	EV_TIKIONLY,
	"i",
	"finished_firing",
	"Sets if the gun is finished firing"
);
Event EV_Weapon_Zoom
(
	"zoom",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Zooms in"
);
Event EV_Weapon_EndZoom
(
	"endZoom",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Ends the zoom"
);
Event EV_Weapon_Rezoom
(
	"rezoom",
	EV_TIKIONLY,
	NULL,
	NULL,
	"Rezooms to the last zoom fov"
);
Event EV_Weapon_SetTargetingSkin
(
	"targetingskin",
	EV_TIKIONLY,
	"i",
	"skinNum",
	"Sets the skin number to use when targeting something"
);
Event EV_Weapon_SetShootingSkin
(
	"shootingskin",
	EV_TIKIONLY,
	"i",
	"skinNum",
	"Sets the skin number to use when shooting"
);
Event EV_Weapon_SetFullAmmoSkin
(
	"fullAmmoSkin",
	EV_TIKIONLY,
	"ii",
	"skinNum modeNum",
	"Sets the skin number to use when the weapon has full ammo"
);
Event EV_Weapon_SetMoveSpeedModifier
(
	"moveSpeedModifier",
	EV_DEFAULT,
	"f",
	"modifier",
	"Sets the move speed modifier for this weapon (when not shooting)."
);
Event EV_Weapon_SetShootingMoveSpeedModifier
(
	"shootingMoveSpeedModifier",
	EV_DEFAULT,
	"f",
	"modifier",
	"Sets the move speed modifier for this weapon while shooting."
);
Event EV_Weapon_UseActorAiming
(
	"useactoraiming",
	EV_DEFAULT,
	"B",
	"flag",
	"This weapon aims with it's tag_barrel.  Regardless of who is holding it."
);
Event EV_Weapon_ViewShake
(
	"viewShake",
	EV_DEFAULT,
	"fF",
	"viewShakeMagnitude viewShakeDuration",
	"Sets the magnitude & duration of the view shaking while firing this weapon.\n"
	"Duration defaults to .05 if not set"
);
Event EV_Weapon_AdvancedViewShake
(
	"advancedViewShake",
	EV_DEFAULT,
	"vvFB",
	"minViewShake maxViewShake viewShakeDuration override",
	"Sets the min shake vector, max shake vector, & duration of the view shaking while firing this weapon.\n"
	"Duration defaults to .05 if not set"
);
Event EV_Weapon_ClearViewShake
(
	"clearViewShake",
	EV_CODEONLY,
	NULL,
	NULL,
	"Clears the current view shake."
);
Event EV_Weapon_ReduceViewShake
(
	"reduceViewShake",
	EV_CODEONLY,
	NULL,
	NULL,
	"Reduces the current view shake."
);
Event EV_Weapon_SetPowerRating
(
	"powerrating",
	EV_DEFAULT,
	"f",
	"rating",
	"Specifies how much damage is done per second by the weapon"
);
Event EV_Weapon_SetProjectileSpeed
(
	"projectilespeed",
	EV_DEFAULT,
	"f",
	"proj_speed",
	"Specifies the speed of the projectile -- Used by AI"
);
Event EV_Weapon_SetProjectileDamage
(
	"projectiledamage",
	EV_DEFAULT,
	"f",
	"proj_damage",
	"Specifies the damage of the projectile"
);
Event EV_Weapon_SetBurstModeDelay
(
	"burstModeDelay",
	EV_DEFAULT,
	"f",
	"burstModeDelay",
	"Specifies the length of time to use between bursts (do not use this if you want to use the animation timing)"
);
Event EV_Weapon_SetArcProjectile
(
	"arcprojectile",
	EV_DEFAULT,
	"b",
	"arc_bool",
	"Specifies for actors if the projectile should arc"
);
Event EV_Weapon_SetLowArcRange
(
	"lowarcrange",
	EV_DEFAULT,
	"f",
	"range",
	"Specifies for actors the range at which to change from high trajectory to normal"
);

Event EV_Weapon_SetPlayMissSound
(
	"playmisssound",
	EV_DEFAULT,
	"b",
	"playsound",
	"Sets whether or not to play a miss sound( snd_ricochet ) at the point of impact"
);

Event EV_Weapon_NoAmmoMode
(
	"noAmmoMode",
	EV_DEFAULT,
	NULL,
	NULL,
	"Specifies that the current mode also has a no ammo mode."
);
Event EV_Weapon_NoDelay
(
	"noDelay",
	EV_DEFAULT,
	NULL,
	NULL,
	"Specifies that this mode has no delay."
);
Event EV_Weapon_PauseRegen
(
	"pauseRegen",
	EV_DEFAULT,
	NULL,
	NULL,
	"Pauses the regen of ammo for a little bit."
);
Event EV_Weapon_SetChargedModels
(
	"chargedModels",
	EV_DEFAULT,
	"i",
	"numChargedModels",
	"Sets the number of charged models there are to use."
);
Event EV_Weapon_SetControllingProjectile
(
	"controllingProjectile",
	EV_DEFAULT,
	"B",
	"bool",
	"Sets whether or not the weapon is currently controlling a projectile or not."
);
Event EV_Weapon_SetCanInterruptFiringState
(
	"canInterruptFiringState",
	EV_DEFAULT,
	"B",
	"bool",
	"Sets whether or not the weapon can be interrupted during its firing state."
);
Event EV_Weapon_StartViewShake
(
	"startViewShake",
	EV_CODEONLY,
	NULL,
	NULL,
	"Starts the weapons view shake."
);
Event EV_Weapon_SpreadAnimData
(
	"spreadAnimData",
	EV_DEFAULT,
	"if",
	"numAnims totalTime",
	"Sets up the info for spread over time using different fire anims."
);
Event EV_Weapon_MaxViewShakeChange
(
	"maxViewShakeChange",
	EV_DEFAULT,
	"f",
	"maxViewChange",
	"Sets the max view change for this weapon."
);
Event EV_Weapon_ControlParms
(
	"controlProjParms",
	EV_DEFAULT,
	"ss",
	"emitterName soundName",
	"Sets the paramaters to use when controlling a projectile."
);
Event EV_Weapon_ProjectileControlHidden
(
	"controlProjHidden",
	EV_DEFAULT,
	"b",
	"hiddenBool",
	"Sets whether or not the projectile control is currently hidden."
);

Event EV_Weapon_MeleeParms
(
	"meleeParms",
	EV_DEFAULT,
	"fff",
	"width height length",
	"Sets the parms of the melee attack."
);
Event EV_Weapon_FireOffset
(
	"fireOffset",
	EV_DEFAULT,
	"v",
	"fireOffset",
	"Sets the offset where the weapon will fire from."
);
Event EV_Weapon_AutoReload
(
	"autoReload",
	EV_DEFAULT,
	"b",
	"autoReload",
	"Specifies whether or not the weapon automatically reloads."
);
Event EV_Weapon_AllowAutoSwitch
(
	"allowAutoSwitch",
	EV_DEFAULT,
	"b",
	"allowAutoSwitch",
	"Specifies whether or not the weapon automatically switches to another weapon when it's out of ammo."
);
Event EV_Weapon_ForceReload
(
	"forceReload",
	EV_DEFAULT,
	NULL,
	NULL,
	"Forces a reload to occur."
);
Event EV_Weapon_NextSwitchTime
(
	"nextSwitchTime",
	EV_DEFAULT,
	"f",
	"time",
	"Sets the next switch time."
);

CLASS_DECLARATION( Item, Weapon, NULL )
{
	{ &EV_Item_Pickup,							&Weapon::PickupWeapon },
	{ &EV_Weapon_DoneRaising,					&Weapon::DoneRaising },
	{ &EV_Weapon_DoneFiring,					&Weapon::DoneFiring },
	{ &EV_Weapon_DoneAnimating,					&Weapon::DoneAnimating },
	{ &EV_Weapon_Idle,							&Weapon::Idle },
	{ &EV_BroadcastSound,						&Weapon::WeaponSound },
	{ &EV_Weapon_DoneReloading,					&Weapon::DoneReloading },
	{ &EV_Weapon_DoneReloadingBurst,			&Weapon::DoneReloadingBurst },
	{ &EV_Weapon_SetAmmoClipSize,				&Weapon::SetAmmoClipSize },
	{ &EV_Weapon_SetAmmoInClip,					&Weapon::SetAmmoInClip },
	{ &EV_Weapon_ProcessModelCommands,			&Weapon::ProcessWeaponCommandsEvent },
	{ &EV_Weapon_SetMaxRange,					&Weapon::SetMaxRangeEvent },
	{ &EV_Weapon_SetMinRange,					&Weapon::SetMinRangeEvent },
	{ &EV_Weapon_ActionIncrement,				&Weapon::SetActionLevelIncrement },
	{ &EV_Weapon_NotDroppable,					&Weapon::NotDroppableEvent },
	{ &EV_Weapon_SetAimAnim,					&Weapon::SetAimAnim },
	{ &EV_Weapon_Shoot,							&Weapon::Shoot },
	{ &EV_Weapon_SetFireType,					&Weapon::SetFireType },
	{ &EV_Weapon_DonePutaway,					&Weapon::DonePutaway },
	{ &EV_Weapon_SetProjectile,					&Weapon::SetProjectile },
	{ &EV_Weapon_SetBulletDamage,				&Weapon::SetBulletDamage },
	{ &EV_Weapon_SetBulletCount,				&Weapon::SetBulletCount },
	{ &EV_Weapon_SetBulletKnockback,			&Weapon::SetBulletKnockback },
	{ &EV_Weapon_SetBulletRange,				&Weapon::SetBulletRange },
	{ &EV_Weapon_SetRange,						&Weapon::SetRange },
	{ &EV_Weapon_SetBulletSpread,				&Weapon::SetBulletSpread },
	{ &EV_Weapon_Hand,							&Weapon::SetHand },
	{ &EV_Weapon_Mode,							&Weapon::ModeSet },
	{ &EV_Weapon_AmmoType,						&Weapon::SetAmmoType },
	{ &EV_Weapon_StartAmmo,						&Weapon::SetStartAmmo },
	{ &EV_Weapon_AmmoBoost,						&Weapon::setAmmoBoost },
	{ &EV_Weapon_AmmoRequired,					&Weapon::SetAmmoRequired },
	{ &EV_Weapon_MaxChargeTime,					&Weapon::SetMaxChargeTime },
	{ &EV_Weapon_GiveStartingAmmo,				&Weapon::GiveStartingAmmo },
	{ &EV_Weapon_GiveAmmoBoost,					&Weapon::giveAmmoBoost },
	{ &EV_Weapon_AutoAim,						&Weapon::AutoAim },
	{ &EV_Weapon_Crosshair,						&Weapon::Crosshair },
	{ &EV_Weapon_TorsoAim,						&Weapon::TorsoAim },
	{ &EV_Weapon_LeftAttachToTag,				&Weapon::LeftAttachToTag },
	{ &EV_Weapon_RightAttachToTag,				&Weapon::RightAttachToTag },
	{ &EV_Weapon_DualAttachToTag,				&Weapon::DualAttachToTag },
	{ &EV_Weapon_HolsterTagLeft,				&Weapon::LeftHolsterAttachToTag },
	{ &EV_Weapon_HolsterTagRight,				&Weapon::RightHolsterAttachToTag },
	{ &EV_Weapon_HolsterTagDual,				&Weapon::DualHolsterAttachToTag },
	{ &EV_Weapon_RightHolsterAngles,			&Weapon::SetRightHolsterAngles },
	{ &EV_Weapon_LeftHolsterAngles,				&Weapon::SetLeftHolsterAngles },
	{ &EV_Weapon_DualHolsterAngles,				&Weapon::SetDualHolsterAngles },
	{ &EV_Weapon_HolsterScale,					&Weapon::SetHolsterScale },
	{ &EV_Weapon_WeildedScale,					&Weapon::setWeildedScale },
	{ &EV_Weapon_SetQuiet,						&Weapon::SetQuiet },
	{ &EV_Weapon_SetLoopFire,					&Weapon::SetLoopFire },
	{ &EV_Weapon_AutoPutaway,					&Weapon::SetAutoPutaway },
	{ &EV_Weapon_UseNoAmmo,						&Weapon::SetUseNoAmmo },
	{ &EV_Weapon_SetMeansOfDeath,				&Weapon::SetMeansOfDeath },
	{ &EV_Weapon_SetWorldHitSpawn,				&Weapon::SetWorldHitSpawn },
	{ &EV_Weapon_MakeNoise,						&Weapon::MakeNoise },
	{ &EV_Weapon_SetViewModel,					&Weapon::SetViewModel },
	{ &EV_Weapon_SetRegenAmmo,					&Weapon::SetRegenAmmo },
	{ &EV_Weapon_SetRegenOnlyWhenIdle,			&Weapon::SetRegenOnlyWhenIdle },
	{ &EV_Weapon_ChangeIdle,					&Weapon::ChangeIdle },
	{ &EV_Weapon_DrawBowStrain,					&Weapon::DrawBowStrain },
	{ &EV_Weapon_AltDrawBowStrain,				&Weapon::AltDrawBowStrain },
	{ &EV_Weapon_SetAccuracy,					&Weapon::SetAccuracy },
	{ &EV_Weapon_SetReticuleTime,				&Weapon::SetReticuleTime },
	{ &EV_Weapon_SetZoomFOV,					&Weapon::SetZoomFOV },
	{ &EV_Weapon_IncrementZoomFOV,				&Weapon::IncrementZoom	},
	{ &EV_Weapon_SetZoomStage,					&Weapon::setZoomStage	},
	{ &EV_Weapon_SetStartZoom,					&Weapon::SetStartZoom },
	{ &EV_Weapon_SetEndZoom,					&Weapon::SetEndZoom },
	{ &EV_Weapon_SetZoomTime,					&Weapon::SetZoomTime },
	{ &EV_Weapon_SetAimType,					&Weapon::SetAimType },
	{ &EV_Weapon_SetFireTimer,					&Weapon::SetFireTimer },
	{ &EV_Weapon_UseSameClip,					&Weapon::UseSameClip },
	{ &EV_Weapon_SetMaxModes,					&Weapon::SetMaxModes },
	{ &EV_Weapon_SetSwitchMode,					&Weapon::SetSwitchMode },
	{ &EV_Weapon_DoneSwitchToMiddle,			&Weapon::DoneSwitchToMiddle },
	{ &EV_Weapon_DoneSwitching,					&Weapon::DoneSwitching },
	{ &EV_Weapon_TargetIdle,					&Weapon::TargetIdle },
	{ &EV_Weapon_TargetIdleThink,				&Weapon::TargetIdleThink },
	{ &EV_Weapon_SetBurstMode,					&Weapon::SetBurstMode },
	{ &EV_Weapon_FinishedFiring,				&Weapon::FinishedFiring		},
	{ &EV_Weapon_StartFiring,					&Weapon::StartFiring		},
	{ &EV_Weapon_Zoom,							&Weapon::Zoom },
	{ &EV_Weapon_EndZoom,						&Weapon::endZoom },
	{ &EV_Weapon_Rezoom,						&Weapon::rezoom },
	{ &EV_Weapon_SetTargetingSkin,				&Weapon::SetTargetingSkin		},
	{ &EV_Weapon_SetShootingSkin,				&Weapon::SetShootingSkin		},
	{ &EV_Weapon_SetFullAmmoSkin,				&Weapon::setFullAmmoSkin },
	{ &EV_Weapon_SetMoveSpeedModifier,			&Weapon::setMoveSpeedModifier },
	{ &EV_Weapon_SetShootingMoveSpeedModifier,	&Weapon::setShootingMoveSpeedModifier },
	{ &EV_Weapon_UseActorAiming,				&Weapon::UseActorAiming },
	{ &EV_Weapon_SetPowerRating,				&Weapon::SetPowerRating },
	{ &EV_Weapon_SetProjectileSpeed,			&Weapon::SetProjectileSpeed },
	{ &EV_Weapon_SetProjectileDamage,			&Weapon::SetProjectileDamage  },
	{ &EV_Weapon_ViewShake,						&Weapon::setViewShakeInfo },
	{ &EV_Weapon_AdvancedViewShake,				&Weapon::setAdvancedViewShakeInfo },
	{ &EV_Weapon_ReduceViewShake,				&Weapon::reduceViewShake },
	{ &EV_Weapon_ClearViewShake,				&Weapon::clearViewShake },
	{ &EV_ProcessGameplayData,					&Weapon::processGameplayData },

	{ &EV_Weapon_SetArcProjectile,				&Weapon::SetArcProjectile	},
	{ &EV_Weapon_SetLowArcRange,				&Weapon::SetLowArcRange		},

	{ &EV_Weapon_NoAmmoMode,					&Weapon::noAmmoMode },
	{ &EV_Weapon_NoDelay,						&Weapon::setNoDelay },
	{ &EV_Weapon_PauseRegen,					&Weapon::pauseRegen },

	{ &EV_Weapon_SetBurstModeDelay,				&Weapon::setBurstModeDelay },
	{ &EV_Weapon_SetChargedModels,				&Weapon::setChargedModels },
	{ &EV_Weapon_SetControllingProjectile,		&Weapon::setControllingProjectile },

	{ &EV_Weapon_SetCanInterruptFiringState,	&Weapon::setCanInterruptFiringState },

	{ &EV_Weapon_StartViewShake,				&Weapon::startViewShake },

	{ &EV_Weapon_SpreadAnimData,				&Weapon::setSpreadAnimData },

	{ &EV_Weapon_MaxViewShakeChange,			&Weapon::setMaxViewShakeChange },
	{ &EV_Weapon_ControlParms,					&Weapon::setControlParms },
	{ &EV_Weapon_ProjectileControlHidden,		&Weapon::setProjectileControlHidden },

	{ &EV_Weapon_MeleeParms,					&Weapon::setMeleeParms },
	{ &EV_Weapon_FireOffset,					&Weapon::setFireOffset },

	{ &EV_Weapon_AutoReload,					&Weapon::setAutoReload },
	{ &EV_Weapon_AllowAutoSwitch,				&Weapon::setAllowAutoSwitch },
	{ &EV_Weapon_ForceReload,					&Weapon::forceReload },
	{ &EV_Weapon_SetPlayMissSound,				&Weapon::SetPlayMissSound },

	{ &EV_Weapon_NextSwitchTime,				&Weapon::setNextSwitchTime },

	{ &EV_Anim,									&Weapon::PassToAnimate },
	{ &EV_NewAnim,								&Weapon::PassToAnimate },

	{ NULL, NULL }
};

Weapon::Weapon()
{
	if ( LoadingSavegame )
	{
		return;
	}
	
	owner					      = NULL;
	
	rank					      = 0;
	
	INITIALIZE_WEAPONMODE_VAR( ammorequired, 0 );
	
	INITIALIZE_WEAPONMODE_VAR( startammo, 0 );

	INITIALIZE_WEAPONMODE_VAR( _ammoBoost, 0 );
	
	INITIALIZE_WEAPONMODE_VAR( ammo_clip_size, 0 );
	
	INITIALIZE_WEAPONMODE_VAR( ammo_in_clip, 0 );
	
	nextweaponsoundtime     = 0;
	
	weaponstate			      = WEAPON_HOLSTERED;
	
	notdroppable            = false;
	
	aimanim                 = -1;
	aimframe                = 0;
	
	attached = false;
	
	maxrange = 1000;
	
	minrange = 0;
	
	memset( projectilespeed, 0, sizeof( projectilespeed ) );
	
	INITIALIZE_WEAPONMODE_VAR( action_level_increment, 2 );
	
	setMoveType( MOVETYPE_NONE );
	
	INITIALIZE_WEAPONMODE_VAR( firetype, (firetype_t)0 );
	
	INITIALIZE_WEAPONMODE_VAR( bulletdamage,    0 );
	INITIALIZE_WEAPONMODE_VAR( bulletcount,     1 );
	INITIALIZE_WEAPONMODE_VAR( bulletrange,     1024 );
	INITIALIZE_WEAPONMODE_VAR( bulletknockback, 0 );
	INITIALIZE_WEAPONMODE_VAR( ammo_type,       "" );
	INITIALIZE_WEAPONMODE_VAR( loopfire,        false );
	INITIALIZE_WEAPONMODE_VAR( burstmode,		  false);
	burstcount = 0;
	burstcountmax = 0;
	
	INITIALIZE_WEAPONMODE_VAR( max_charge_time,   5 );
	charge_fraction = 1.0f;
	
	left_attachToTag  = "tag_lhand";
	right_attachToTag = "tag_rhand";
	dual_attachToTag  = "tag_weapon_dual";
	
	putaway = false;
	
	hand    = WEAPON_ANY;
	
	firemodeindex = FIRE_MODE1;
	
	setName( "Unnamed Weapon" );
	
	edict->s.renderfx |= RF_EXTRALIGHT;
	
	edict->s.renderfx |= RF_WEAPONMODEL;
	
	// Weapons do not auto aim automatically
	autoAimTargetSelectionAngle = 0;
	autoAimLockonAngle = 0;
	crosshair = false;
	
	torsoaim = false;
	
	quiet = false;
	next_noise_time = 0;
	next_noammo_time = 0;
	
	lastValid = false;
	lastScale = 1.0f;
	holsterScale = 1.0f;
	_weildedScale = 0.5f;
	
	auto_putaway = false;
	
	use_no_ammo  = true;
	
	for ( int i=0; i<MAX_FIREMODES; i++ )
	{
		for ( int j=0; j<MAX_ACCURACYTYPES; j++ )
		{
			accuracy[i][j] = 0.0; 
		}
	}
		
	aimtype = ACCURACY_STOPPED;
	
	reticuletime = 0.0;
	zoomfov = 90.0;
	_lastZoomFov = 90.0f;
	
	usesameclip = false;
	
	curmode = FIRE_MODE1;
	maxmode = FIRE_MODE2;
	
	chargetime = 0.0;
	
	switchmode = false;
	targetidle = false;
	targetidleflag = false;
	
	chx = 0;
	chy = 0;
	
	donefiring = false;
	
	zoomed = false;
	useActorAiming = false;
	
	INITIALIZE_WEAPONMODE_VAR(next_fire_time, 0.0f);
	INITIALIZE_WEAPONMODE_VAR(fire_timer, 0.0f);
	
	INITIALIZE_WEAPONMODE_VAR( meansofdeath, MOD_SWORD );
	
	targetingSkin = 0;
	shootingSkin = 0;
	_fullAmmoSkin = 0;
	_fullAmmoMode = FIRE_MODE1;
	
	_powerRating	  = 0.0f;
	_projectileDamage = 0.0f;
	_projectileSpeed  = 0.0f;
	_arcProjectile = false;
	_lowArcRange = 256.0f;
	_playMissSound = false;
	
	defaultMoveSpeedModifier = 1.0f;
	INITIALIZE_WEAPONMODE_VAR( shootingMoveSpeedModifier, 1.0f );
	
	INITIALIZE_WEAPONMODE_VAR( _viewShakeMagnitude, 0.0f );
	INITIALIZE_WEAPONMODE_VAR( _viewShakeDuration, 0.0f );

	_maxViewShakeChange = 10.0f;
	
	INITIALIZE_WEAPONMODE_VAR( _burstModeDelay, 0.0f );
	
	INITIALIZE_WEAPONMODE_VAR( _noAmmoMode, false );
	
	INITIALIZE_WEAPONMODE_VAR( _noDelay, false );

	INITIALIZE_WEAPONMODE_VAR( _chargedModels, 0 );
	
	PostEvent( EV_Weapon_Idle, 0.0f );

	_mpItemType = MP_ITEM_TYPE_WEAPON;

	_controllingProjectile = false;
	_controllingProjectileHidden = false;

	startfiretime = 0.0f;

	INITIALIZE_WEAPONMODE_VAR( _spreadAnims, 0 );
	INITIALIZE_WEAPONMODE_VAR( _spreadTime, 0.0f );

	INITIALIZE_WEAPONMODE_VAR( _meleeWidth,   15.0f );
	INITIALIZE_WEAPONMODE_VAR( _meleeHeight , 45.0f );
	INITIALIZE_WEAPONMODE_VAR( _meleeLength,  96.0f );


	INITIALIZE_WEAPONMODE_VAR( _regenAmount,  0 );
	INITIALIZE_WEAPONMODE_VAR( _regenTime,  0.0f );
	INITIALIZE_WEAPONMODE_VAR( _regenOnlyWhenIdle,  false );
	INITIALIZE_WEAPONMODE_VAR( _nextRegenTime,  0.0f );

	_zoomStage = ZOOM_STAGE_1;

	setRespawnTime( 10 );

	_autoReload = true;

	_allowAutoSwitch = true;

	edict->s.renderfx |= RF_CHILDREN_DONT_INHERIT_ALPHA;

	_nextSwitchTime = 0.0f;
}


Weapon::Weapon( const char *file )
{
	Weapon();
}

Weapon::~Weapon()
{
	DetachGun();
}

int Weapon::GetRank( void )
{
	return rank;
}

int Weapon::GetOrder( void )
{
	return order;
}

void Weapon::SetRank( int order_shadow1, int rank_shadow1 )
{
	this->order = order_shadow1;
	this->rank = rank_shadow1;
}

void Weapon::SetAutoPutaway( Event *ev )
{
	auto_putaway = ev->GetBoolean( 1 );
}

void Weapon::SetUseNoAmmo( Event *ev )
{
	use_no_ammo = ev->GetBoolean( 1 );
}

void Weapon::SetStartAmmo( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	startammo[firemodeindex] = ev->GetInteger( 1 );
}

void Weapon::setAmmoBoost( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );

	_ammoBoost[ firemodeindex ] = ev->GetInteger( 1 );
}

void Weapon::SetMaxChargeTime( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	max_charge_time[firemodeindex] = ev->GetFloat( 1 );
}

void Weapon::SetAmmoRequired( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	ammorequired[firemodeindex] = ev->GetInteger( 1 );
}

int Weapon::GetStartAmmo( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) )
		return startammo[mode];
	else
	{
		warning( "Weapon::GetStartAmmo", "Invalid mode %d\n", mode );
		return 0;
	}
}

int Weapon::getAmmoBoost( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) )
	{
		return _ammoBoost[ mode ];
	}
	else
	{
		warning( "Weapon::getAmmoBoost", "Invalid mode %d\n", mode );
		return 0;
	}
}

str Weapon::GetAmmoType( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) )
		return ammo_type[mode];
	else
	{
		warning( "Weapon::GetAmmoType", "Invalid mode %d\n", mode );
		return "UnknownAmmo";
	}
}

void Weapon::SetAmmoType( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	
	if ( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) )
		ammo_type[firemodeindex] = ev->GetString( 1 );
	else
	{
		warning( "Weapon::SetAmmoType", "Invalid mode %d\n", firemodeindex );
		return;
	}
}

void Weapon::SetAmmoAmount( int amount_shadow4, firemode_t mode )
{
	firemode_t clipToUse;
	
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;
	
	if ( ( clipToUse >= 0 ) && ( clipToUse < MAX_FIREMODES ) )
	{
		if ( ammo_clip_size[clipToUse] )
			ammo_in_clip[clipToUse] = amount_shadow4;
	}
	else
	{
		warning( "Weapon::SetAmmoAmount", "Invalid mode %d\n", clipToUse );
		return;
	}
}


int Weapon::GetRequiredAmmo( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );

	return ammorequired[mode];
}


int Weapon::GetClipSize( firemode_t mode )
{
	firemode_t clipToUse;
	
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;
	
	if ( ( clipToUse >= 0 ) && ( clipToUse < MAX_FIREMODES ) )
		return ammo_clip_size[clipToUse];
	else
	{
		warning( "Weapon::GetClipSize", "Invalid mode %d\n", clipToUse );
		return 0;
	}
}

void Weapon::UseAmmo( int amount_shadow3, firemode_t mode )
{
	firemode_t clipToUse;

	if ( UnlimitedAmmo( mode ) )
		return;

	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );

	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
		warning( "Weapon::UseAmmo", "Invalid mode %d\n", mode );

	if ( !HasAmmo( mode ) && hasNoAmmoMode( mode ) )
	{
		return;
	}


	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;


	if ( ammo_clip_size[clipToUse] )
	{
		ammo_in_clip[clipToUse] -= amount_shadow3;
		if (ammo_in_clip[clipToUse] < 0)
		{
			warning("UseAmmo","Used more ammo than in clip.\n");
			ammo_in_clip[clipToUse] = 0;
		}
		owner->AmmoAmountInClipChanged( ammo_type[mode], ammo_in_clip[clipToUse] );
	}
	else
	{
		assert( owner );
		if ( owner && owner->isClient() && !UnlimitedAmmo( mode ) )
		{
			owner->UseAmmo( ammo_type[mode], ammorequired[mode] );
		}
	}

	if ( burstmode[mode] ) 
	{
		burstcount--;
	}
}

void Weapon::GetActorMuzzlePosition( Vector *position, Vector *forward, Vector *right, Vector *up, const char* tagname )
{
	int tag_num;
	
	str tag_name;
	if ( !tagname )
		tag_name = "tag_barrel";
	else
		tag_name = tagname;
	
	tag_num = gi.Tag_NumForName( edict->s.modelindex, tag_name );
	
	if ( tag_num < 0 )
		gi.Error( ERR_FATAL, "Weapon::GetActorMuzzlePosition, invalid tag name." );
	
	setOrigin();
	setAngles();
	
	
	orientation_t  weap_or, barrel_or, orn;
	Vector         pos;
	vec3_t         mat[3]={{0,0,0},{0,0,0},{0,0,0}};
	vec3_t         orient[3];
	int            i, mi, tagnum;
	Sentient *owner_shadow6;   
	owner_shadow6 = this->owner;
	
	mi = owner_shadow6->edict->s.modelindex;
	
	tagnum = gi.Tag_NumForName( owner_shadow6->edict->s.modelindex, current_attachToTag.c_str() );
	
	
	
	AnglesToAxis( owner_shadow6->angles, owner_shadow6->orientation );

	orn	= gi.Tag_OrientationEx( mi,
								owner_shadow6->CurrentAnim( legs ),
								owner_shadow6->CurrentFrame( legs ),
								tagnum & TAG_MASK,
								owner_shadow6->edict->s.scale,
								owner_shadow6->edict->s.bone_tag,
								owner_shadow6->edict->s.bone_quat,
								0,
								0,
								1.0f,
								( owner_shadow6->edict->s.anim & ANIM_BLEND ) != 0,
								( owner_shadow6->edict->s.torso_anim & ANIM_BLEND ) != 0,
								owner_shadow6->CurrentAnim( torso ),
								owner_shadow6->CurrentFrame( torso ),
								0,
								0,
								1.0f
								);

	AnglesToAxis( owner_shadow6->angles, orient );
	VectorCopy( owner_shadow6->origin, weap_or.origin );
	for ( i=0;  i<3; i++ )
	{
		VectorMA( weap_or.origin, orn.origin[i], orient[i], weap_or.origin );
	}
	
	MatrixMultiply( orn.axis, orient, weap_or.axis );
	
	if ( !this->GetRawTag( tag_name, &barrel_or ) )
	{
		pos = owner_shadow6->centroid;
		AnglesToAxis( owner_shadow6->angles, mat );
		return;
	}
	
	VectorCopy( weap_or.origin, pos );
	
	for ( i = 0 ; i < 3 ; i++ )
	{
		VectorMA( pos, barrel_or.origin[i], weap_or.axis[i], pos );
	}
	
	MatrixMultiply( barrel_or.axis, weap_or.axis, mat );
	
	if ( position )
		*position = pos;
	
	if ( forward )
		*forward = mat[0];
	
	if ( right )
		*right = mat[1];
	
	if ( up )
		*up = mat[2];
}

void Weapon::GetMuzzlePosition( Vector *position, Vector *forward, Vector *right, Vector *up )
{
	Vector endpoint, vorg;
	orientation_t barrel_or;
	Vector f, r, u, viewWeapOrg, pview, pos;
	Sentient *owner_shadow5;
	Player *player;
	int      tagnum;
	
	owner_shadow5 = this->owner;
	assert( owner_shadow5 );
	
	if ( owner_shadow5->isSubclassOf( Player ) )
		player = ( Player * )owner_shadow5;
	else
	{
		warning("GetMuzzlePosition called from Weapon class for a non-player character.",NULL);
		return;
	}
	

	vorg = player->origin;
	vorg.z += player->client->ps.viewheight;

	pview = player->GetVAngles();
	
	
	AngleVectors( pview, f, r, u );
	
	trace_t viewTrace;
	player->GetViewTrace(viewTrace, MASK_SHOT);
	endpoint = viewTrace.endpos;
	Vector dir = endpoint - vorg;
	float len = dir.length();
	dir.normalize();
	endpoint = vorg + dir * (len + 5.0f);
	
	viewWeapOrg = vorg;
	
	
	
	trace_t trace;
	
	if ( !multiplayerManager.inMultiplayer() || multiplayerManager.fullCollision() )
		trace = G_FullTrace( vorg, vec_zero, vec_zero, endpoint, owner_shadow5, MASK_SHOT, true, "Weapon::GetMuzzlePosition" );
	else
		trace = G_Trace( vorg, vec_zero, vec_zero, endpoint, owner_shadow5, MASK_SHOT, true, "Weapon::GetMuzzlePosition" );
	
	
	tagnum = gi.Tag_NumForName( edict->s.viewmodelindex, "tag_barrel" );

	if ( tagnum >= 0 )
	{
		// (slot 110, Tag_OrientationEx) is never loaded in the whole 1648-byte
		barrel_or = gi.Tag_Orientation( edict->s.viewmodelindex, 0, 0, tagnum & TAG_MASK, 1.0f, edict->s.bone_tag, edict->s.bone_quat );
	}
	else
	{
		VectorClear( barrel_or.origin );
	}

	

	pos = viewWeapOrg;


	if ( _fireOffset[ curmode ] != vec_zero )
	{
		float viewOrientation[3][3];

		AnglesToAxis( pview, viewOrientation );
		MatrixTransformVector( _fireOffset[ curmode ], viewOrientation, pos );
		pos += viewWeapOrg;
	}
	
	// For now lets shoot from the viewpoint not the weapon
	
	*position = pos;
	
	Vector f2 = trace.endpos - *position;
	AngleVectors( f2.toAngles(), f, r, u );
	
	if ( forward ) 
		*forward = f;
	if ( right )
		*right = r;
	if ( up )
		*up = u;
	
}


void Weapon::SetAmmoClipSize( Event * ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	ammo_clip_size[firemodeindex] = ev->GetInteger( 1 );
}

void Weapon::SetAmmoInClip( Event * ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	ammo_in_clip[firemodeindex] = ev->GetInteger( 1 );
}

void Weapon::Shoot( Event *ev )
{
	Vector      pos, forward, right, up, delta;
	firemode_t  mode = FIRE_MODE1;
	firemode_t  realmode = FIRE_MODE1;
	firemode_t  regenMode;
	
	if ( ev->NumArgs() > 0 )
	{
		mode = WeaponModeNameToNum( ev->GetString( 1 ) );
		realmode = mode;
		if ( mode == FIRE_ERROR )
			return;
	}
	
	if ( switchmode ) 
		mode = curmode;
	else 
		curmode = mode;
	
	
	if ( fire_timer[mode] > 0.0 )
		next_fire_time[mode] = fire_timer[mode] + level.time;
	
	
	
	if ( loopfire[mode] )
	{
		if ( HasAmmo( mode ) || hasNoAmmoMode( mode ) ||UnlimitedAmmo( mode ) )
		{
			UseAmmo( ammorequired[mode], mode );
			if ( switchmode ) 
				mode = curmode;
		}
		else
		{
			if ( switchmode ) 
				mode = curmode;
			ForceIdle();
			return;
		}
	}
	
	mode = realmode;
	
	if ( ( usesameclip ) || ( ammo_clip_size[ mode ] == 0 ) )
		regenMode = FIRE_MODE1;
	else
		regenMode = mode;

	if ( ( _regenOnlyWhenIdle[ regenMode ] ) && ( _regenAmount[ regenMode ] > 0 ) )
	{
		_nextRegenTime[ regenMode ] = level.time + _regenTime[ regenMode ] + 0.5f;
	}
	
	Sentient *owner_shadow4;   
	owner_shadow4 = this->owner;
	
	// If I am owned by a player, I need to get the muzzle position, otherwise, I just
	if ( useActorAiming || !owner_shadow4->isSubclassOf( Player ) )
	{
		owner_shadow4->shotsFiredThisVolley++;
		GetActorMuzzlePosition( &pos , &forward, &right, &up );
	}
	else if ( owner_shadow4->isSubclassOf( Player ) )
	{
		Player *player = (Player *)owner_shadow4;

		GetMuzzlePosition( &pos, &forward, &right, &up );
		if (g_aimviewangles->integer || ( player->client->ps.pm_type == PM_SECRET_MOVE_MODE ) ) 
		{
			vec3_t fwd;
			AngleVectors(owner_shadow4->client->ps.viewangles,fwd,NULL,NULL);
			forward = fwd;
		}
	
		if ( firetype[mode] != FT_BULLET )
		{
			applySpread( &forward, &right, &up );
		}
	}
	
	PostEvent( EV_Weapon_StartViewShake, level.frametime * 2 );
	
	Vector testForward;
	pos.AngleVectors( &testForward );
	
	if ( firetype[mode] == FT_PROJECTILE )
	{

		str projectileModelName;
		

		if ( _chargedModels[ mode ] )
		{
			int modelIndexToUse;


			projectileModelName = projectileModel[mode];

			modelIndexToUse = ( _chargedModels[ mode ] * charge_fraction ) + 1;

			if ( modelIndexToUse > _chargedModels[ mode ] )
				modelIndexToUse = _chargedModels[ mode ];

			projectileModelName.CapLength( projectileModelName.length() - 4 );
			projectileModelName += modelIndexToUse;
			projectileModelName += ".tik";
		}
		else
		{
			projectileModelName = projectileModel[mode];
		}

		// and read as "actor never fires".  Same verdict shape, same qa_r120 gate.
		if (gi.cvar("qa_r120", "0", 0)->integer) {
			gi.Printf("[R133:shootpj] own=%d proj='%s' pos=(%.1f,%.1f,%.1f) dir=(%.3f,%.3f,%.3f)\n",
				  owner_shadow4 ? owner_shadow4->entnum : -1,
				  projectileModelName.c_str(),
				  pos.x,
				  pos.y,
				  pos.z,
				  forward.x,
				  forward.y,
				  forward.z);
		}

		ProjectileAttack( pos,
			forward,
			owner_shadow4,
			projectileModelName,
			charge_fraction
			);
	}
	else if ( firetype[mode] == FT_BULLET )
	{
		Vector spread;
		float range;

		if ( owner_shadow4->isSubclassOf( Player ) )
		{
			

			spread = getSpread();
			range = bulletrange[mode];
		}
		else
		{
			spread = bulletspread[ mode ];
			range = bulletrange[mode];
		}
		
		BulletAttack( pos,
			forward,
			right,
			up,
			range,
			bulletdamage[mode],
			bulletknockback[mode],
			0,
			GetMeansOfDeath( mode ),
			spread,
			bulletcount[mode],
			owner_shadow4,
			this
			);
	}
	else if ( firetype[mode] == FT_EXPLOSION )
	{
		if ( owner_shadow4 && owner_shadow4->isSubclassOf( Player ) )
		{
			Player *player = (Player *)owner_shadow4;

			player->shotFired();
		}

		ExplosionAttack( pos, owner_shadow4, projectileModel[mode], forward, owner_shadow4 );
	}
	else if ( firetype[mode] == FT_MELEE ) 
	{
		Vector melee_pos, melee_end;
		Vector dir;
		float damage;
		meansOfDeath_t meansofdeath_shadow1;
		float knockback;

		if ( owner_shadow4->isSubclassOf( Player ) )
		{
			Vector forward_shadow1;

			Player *player = (Player *)owner_shadow4;
			melee_pos = player->origin + Vector( 0.0f, 0.0f, player->client->ps.viewheight );

			player->GetVAngles().AngleVectors( &forward_shadow1 );

			melee_end = melee_pos + forward_shadow1 * _meleeLength[ mode ];
		}
		else
		{
			melee_pos = owner_shadow4->centroid;
			melee_end = owner_shadow4->centroid + Vector( owner_shadow4->orientation[0] ) * _meleeLength[ mode ];
		}

		damage = bulletdamage[mode];
		knockback = bulletknockback[mode];

		meansofdeath_shadow1 = GetMeansOfDeath( mode );

		if ( owner_shadow4->isSubclassOf( Player ) )
		{
			Player *player = (Player *)(Sentient *)owner_shadow4;

			meansofdeath_shadow1 = player->changetMeansOfDeath( meansofdeath_shadow1 );
			damage = player->getDamageDone( damage, meansofdeath_shadow1, true );

			knockback = (knockback + player->GetPlayerKnockback()) * player->GetKnockbackMultiplier();
		}

		Container<EntityPtr>victimlist;

		if ( MeleeAttack( melee_pos, melee_end, damage, owner_shadow4, meansofdeath_shadow1, _meleeWidth[ mode ], -_meleeHeight[ mode ],
			 _meleeHeight[ mode ], knockback, true, &victimlist ) )
		{

			Sound( "impact_flesh", CHAN_VOICE );
		}
		else
		{

			trace_t trace = G_Trace( melee_pos, Vector( -8.0f, -8.0f, -8.0f ), Vector( 8.0f, 8.0f, 8.0f ), melee_end, owner_shadow4, MASK_MELEE, false, "Weapon::Shoot" );
			
			Entity *victim = G_GetEntity( trace.entityNum );
			
			if ( victim && ( ( victim == world ) || ( victim->takedamage == DAMAGE_NO ) ) )
			{
				vec3_t  newangles;
				vectoangles( trace.plane.normal, newangles );
				WorldHitSpawn( mode, trace.endpos, newangles, 0.1f );
				
				str realname = this->GetRandomAlias( "impact_world" );
				if ( realname.length() > 1 )
					this->Sound( realname, CHAN_VOICE );
			}
		}
	}
	
	if ( !quiet )
	{
		if ( next_noise_time <= level.time )
		{
			BroadcastSound();
			next_noise_time = level.time + 1.0f;
		}
	}
}

void Weapon::SetAimAnim( Event *ev )
{
	str anim;
	
	anim     = ev->GetString( 1 );
	aimanim  = gi.Anim_NumForName( edict->s.modelindex, anim.c_str() );
	aimframe = ev->GetInteger( 2 );
}

void Weapon::SetOwner( Sentient *ent )
{
	assert( ent );
	if ( !ent )
	{
		return;
	}
	
	Item::SetOwner( ent );
	
	setOrigin( vec_zero );
	setAngles( vec_zero );
}

int Weapon::AmmoAvailable( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
	{
		warning( "Weapon::AmmoAvailable", "Invalid mode %d\n", mode );
		return 0;
	}
	
	if ( owner )
	{
		return owner->AmmoCount( ammo_type[mode] );
	}
	else
	{
		warning( "Weapon::AmmoAvailable", "Weapon does not have an owner.\n" );
		return 0;
	}
}

qboolean Weapon::UnlimitedAmmo( firemode_t mode )
{
	if ( !owner )
	{
		return false;
	}
	
	if ( !ammorequired[mode] )
		return true;
	
	if ( !owner->isClient() || ( owner->flags & FL_GODMODE ) || multiplayerManager.checkFlag( MP_FLAG_INFINITE_AMMO ) )
	{
		return true;
	}
	else if ( !stricmp( ammo_type[mode], "None" ) )
	{
		return true;
	}
	
	return false;
}

qboolean Weapon::HasInvAmmo( firemode_t mode )
{
	if ( !ammorequired[mode] )
		return false;
	
	return ( AmmoAvailable( mode ) >= ammorequired[mode] );
}

qboolean Weapon::HasAmmo( firemode_t mode, int numShots )
{
	firemode_t clipToUse;

	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
	{
		warning( "Weapon::HasAmmo", "Invalid mode %d\n", mode );
		return false;
	}
	
	if ( !owner )
	{
		return false;
	}
	
	if ( UnlimitedAmmo( mode ) )
	{
		return true;
	}
	
	if ( burstmode[mode] && burstcount <= 0 ) 
	{
		return false;
	}

	if ( usesameclip )
	{
		clipToUse = FIRE_MODE1;
	}
	else
	{
		clipToUse = mode;
	}

	if ( ammo_clip_size[clipToUse] )
	{
		return HasAmmoInClip( mode, numShots );
	}
	else 
	{
		if ( !ammorequired[mode] )
			return false;
		
		return ( AmmoAvailable( mode ) >= ammorequired[mode] * numShots );
	}
}

qboolean Weapon::HasAmmoInClip( firemode_t mode, int numShots )
{
	firemode_t clipToUse;

	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
		warning( "Weapon::HasAmmoInClip", "Invalid mode %d\n", mode );

	if ( usesameclip )
	{
		clipToUse = FIRE_MODE1;
	}
	else
	{
		clipToUse = mode;
	}
	
	if ( ammo_clip_size[clipToUse] )
	{
		if ( ammo_in_clip[clipToUse] >= ammorequired[mode] * numShots )
		{
			return true;
		}
	}
	return false;
}

void Weapon::ForceState( weaponstate_t state )
{
	weaponstate = state;
}

qboolean Weapon::ReadyToFire( firemode_t mode, qboolean playsound )
{

	if ( _canInterruptFiringState )
	{
		if ( weaponstate != WEAPON_READY && weaponstate != WEAPON_FIRING )
			return false;
	}
	else
	{
		if ( weaponstate != WEAPON_READY )
			return false;
	}

	str ammotype = GetAmmoType( ( firemode_t )mode );
	if ( ammotype == "None" )
		return true;
	
	
	if ( HasAmmo( mode ) )
	{
		return true;
	}
	
	if ( hasNoAmmoMode( mode ) )
	{
		return true;
	}
	
	if ( playsound && ( level.time > next_noammo_time ) && ( AmmoAvailable( mode ) < ammorequired[mode] ) )
	{
		Sound( "snd_noammo" );
		next_noammo_time = level.time + 0.25f;


		if ( owner && owner->isSubclassOf( Player ) )
		{
			{

				if ( shouldAutoSwitch( mode ) )
				{
					autoSwitch();
				}

			}
		}
	}

	return false;
}

void Weapon::PutAway( void )
{
	
	if ( owner && owner->isSubclassOf( Actor ) )
	{
		Event *newEvent = new Event( EV_DisplayEffect );
		newEvent->AddString( "TransportOut" );
		newEvent->AddString( "FederationWeapons" );
		ProcessEvent( newEvent );
	}

	if ( _controllingProjectile )
	{
		toggleProjectileControl();
	}
	
	putaway = true;
	
	if ( !owner )
		return;

	Uninitialize();
	
}

void Weapon::DetachFromOwner( void )
{
	if ( edict->s.renderfx | RF_DONTDRAW )
		animate->StopAnimatingAtEnd();
	else
		animate->StopAnimating();

	DetachGun();
	weaponstate = WEAPON_HOLSTERED;
	current_attachToTag = "";

	Uninitialize();
}

void Weapon::AttachToOwner( weaponhand_t hand_shadow3 )
{
	AttachGun( hand_shadow3 , false );
	
	if ( targetidle )
		PostEvent(EV_Weapon_TargetIdleThink, TargetIdleTime);

	ForceIdle();
}

void Weapon::AttachToHolster( weaponhand_t hand_shadow2 )
{
	AttachGun( hand_shadow2, true );
	animate->RandomAnimate( "holster", EV_Weapon_Idle );
}


float Weapon::GetRange(firemode_t idx)
{
    return bulletrange[idx];
}

str Weapon::GetLeftHolsterTag(void)
{
    return leftholster_attachToTag;
}

str Weapon::GetRightHolsterTag(void)
{
    return rightholster_attachToTag;
}

str Weapon::GetDualHolsterTag(void)
{
    return dualholster_attachToTag;
}

str Weapon::GetCurrentAttachToTag( void )
{
	return current_attachToTag;
}

qboolean Weapon::Drop( void )
{
	float radius;
	Vector temp;
	int i;
	
	if ( !owner )
	{
		return false;
	}
	
	if ( !IsDroppable() )
	{
		return false;
	}

	if ( multiplayerManager.inMultiplayer() && !multiplayerManager.checkRule( "weapon-candrop", true, NULL ) )
	{
		return false;
	}
	
	DetachGun();


	CancelEventsOfType( EV_DisplayEffect );
	CancelEventsOfType( EV_FadeIn );
	clearCustomShader();
	setAlpha( 1.0f );
	edict->s.renderfx &= ~RF_FORCE_ALPHA_EFFECTS;
	edict->s.renderfx &= ~RF_FORCE_ALPHA;

	if ( _controllingProjectile )
	{
		toggleProjectileControl();
	}

	temp[ 1 ] = 64.0;
	temp[ 2 ] = 48.0;
	if ( owner )
	{
		setOrigin( owner->origin + temp );
	}
	else
	{
		setOrigin( origin + temp );
	}
	
	flags |= FL_ROTATEDBOUNDS;
	
	if ( ( mins == vec_zero ) && ( maxs == vec_zero ) )
	{
		gi.CalculateBounds( edict->s.modelindex, edict->s.scale, mins, maxs );
		radius = ( mins - maxs ).length() * 2.0f;
		mins.x = mins.y = -radius;
		maxs.x = maxs.y = radius;
		setSize( mins, maxs );
	}
	
	ForceIdle();
	animate->SetFrame( 0 );
	
	PlaceItem();
	if ( owner )
	{
		temp[ 0 ] = G_CRandom( 50.0f );
		temp[ 1 ] = G_CRandom( 50.0f );
		temp[ 2 ] = 250.0f;
		velocity = owner->velocity * 0.5f + temp;
		setAngles( owner->angles );
	}
	
	avelocity = Vector( 0.0f, G_CRandom( 360.0f ), 0.0f );


	for ( i = 0 ; i < MAX_FIREMODES ; i++ )
	{
		ammo_in_clip[ i ] = 0;
	}
	
	
	last_owner = owner;
	last_owner_trigger_time = level.time + 2.5f;
	
	CancelEventsOfType( EV_Weapon_DoneReloading );
	CancelEventsOfType( EV_Weapon_DoneReloadingBurst );
	
	if ( owner )
	{
		owner->RemoveItem( this );
	}
	
	owner = NULL;

	if ( multiplayerManager.inMultiplayer() && gi.Anim_NumForName( edict->s.modelindex, "idle_onground" ) >= 0 )
	{
		animate->RandomAnimate( "idle_onground" );
		edict->s.eType = ET_MODELANIM;
	}

	if ( multiplayerManager.inMultiplayer() )
	{
		edict->s.renderfx |= RF_FULLBRIGHT;
	}

	PostEvent( EV_FadeOut, 30.0f );

	Uninitialize();
	return true;
}

void Weapon::Charge( firemode_t mode )
{
}

void Weapon::ReleaseFire( firemode_t mode, float charge_time )
{
	
	if ( charge_time > max_charge_time[mode] )
		charge_fraction = 1.0f;
	else
		charge_fraction = charge_time / max_charge_time[mode];
	
	Fire( mode );
}

void Weapon::Fire( firemode_t mode )
{
	Event *done_event=NULL;
	firemode_t realmode;
	Vector pos;
	str fireAnimName;
	
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
		warning( "Weapon::Fire", "Invalid mode %d\n", mode );
	
	realmode = mode;
	
	if ( switchmode )
		mode = curmode;
	else
		curmode = mode;
	
	if ( next_fire_time[mode] > level.time )
		return;
	
	if ( fire_timer[mode] > 0.0f )
		next_fire_time[mode] = fire_timer[mode] + level.time;
	
	if ( !loopfire[mode] )
	{
		done_event = new Event( EV_Weapon_DoneFiring );
		done_event->AddInteger( mode );
	}
	
	
	weaponstate = WEAPON_FIRING;
	
	if ( shootingSkin )
		ChangeSkin( shootingSkin, true );
	
	CancelEventsOfType( EV_Weapon_DoneFiring );
	
	if ( !switchmode )
	{
		if ( mode == FIRE_MODE1 )
			fireAnimName = "fire";
		else if ( mode == FIRE_MODE2 )
			fireAnimName = "alternatefire";
	}
	else
	{
		fireAnimName = "fire_mode";
		fireAnimName += (int)mode+1;
	}
	
	if ( world && world->isThingBroken( item_name.c_str() ) )
	{
		fireAnimName += "_broken";
		animate->RandomAnimate( fireAnimName.c_str(), done_event );
		return;
	}
	else if ( !HasAmmo( mode ) && hasNoAmmoMode( mode ) )
	{
		fireAnimName += "_noammo";
	}
	else if ( _spreadTime[ mode ] && _spreadAnims[ mode ] )
	{
		float firingTime;
		int animNum;

		firingTime = level.time - startfiretime;

		animNum = 1 + ( firingTime / _spreadTime[ mode ] ) * ( _spreadAnims[ mode ] + 1 );

		if ( animNum > _spreadAnims[ mode ] )
			animNum = _spreadAnims[ mode ];

		fireAnimName += "_spread";
		fireAnimName += animNum;
	}
	
	animate->RandomAnimate( fireAnimName.c_str(), done_event );
	
	
	if ( !loopfire[mode] )
	{
		if ( ammorequired[mode] != ammorequired[realmode] )
			UseAmmo(ammorequired[realmode], mode);
		else
			UseAmmo( ammorequired[mode], mode );
		if ( mode != realmode )
		{
			if ( burstmode[realmode] )
				burstcount -= ammorequired[realmode];
		}
	}
}

void Weapon::DetachGun( void )
{
	if ( attached )
	{
		StopSound( CHAN_WEAPONIDLE );
		attached = false;
		detach();
		hideModel();

		clearDisplayEffects();

		if ( lastValid )
		{

			setScale( lastScale );
			lastValid = false;
		}
	}
}

void Weapon::AttachGun( weaponhand_t hand_shadow1, qboolean holstering )
{
	int tag_num = 0;
	
	if ( !owner )
	{
		current_attachToTag = "";
		return;
	}
	
	if ( attached )
	{
		DetachGun();
	}
	
	if ( holstering )
	{
		lastAngles = this->angles;
		lastScale  = this->edict->s.scale;
		lastValid  = true;
	}
	else
	{
		lastScale  = this->edict->s.scale;
		lastValid  = true;

		if ( owner->isSubclassOf( Player ) )
		{
			if ( mp_bigGunMode->integer )
				setScale( _weildedScale * 2.0f );
			else
				setScale( _weildedScale );
		}
	}

	
	switch( hand_shadow1 )
	{
	case WEAPON_LEFT:
		if ( holstering )
		{
			current_attachToTag = leftholster_attachToTag;
			setAngles( leftHolsterAngles );
			setScale( holsterScale );
		}
		else
		{
			current_attachToTag = left_attachToTag;
		}
		break;
	case WEAPON_RIGHT:
	case WEAPON_DUAL:
		if ( holstering )
		{
			current_attachToTag = rightholster_attachToTag;
			setAngles( rightHolsterAngles );
			setScale( holsterScale );
		}
		else
		{
			current_attachToTag = right_attachToTag;
		}
		break;
		
	default:
		warning( "Weapon::AttachGun", "Invalid hand for attachment of weapon specified" );
		break;
	}
	
	if ( !current_attachToTag.length() )
		return;
	
	tag_num = gi.Tag_NumForName( owner->edict->s.modelindex, this->current_attachToTag.c_str() );
	
	NoLerpThisFrame();
	if	( tag_num >= 0 )
	   {
		attached = true;
		
		attach( owner->entnum, tag_num, false );
		
		showModel();
		setOrigin();

		if ( owner && owner->isSubclassOf( Actor ) )
		{
			Event *newEvent = new Event( EV_DisplayEffect );
			newEvent->AddString( "TransportIn" );
			newEvent->AddString( "FederationWeapons" );
			ProcessEvent( newEvent );
		}
	}
	else
	{
		warning( "Weapon::AttachGun", "Attachment of weapon to tag \"%s\": Tag Not Found\n", this->current_attachToTag.c_str() );
	}
}

void Weapon::GiveStartingAmmo( Event *ev )
{
	str   ammotype;
	int   mode;
	Entity *entityToGiveAmmo;
	Sentient *sentient;
	
	if ( ev->NumArgs() > 0 )
		entityToGiveAmmo = ev->GetEntity( 1 );
	else
		entityToGiveAmmo = owner;
	
	if ( !entityToGiveAmmo || !entityToGiveAmmo->isSubclassOf( Sentient ) )
	{
		assert( entityToGiveAmmo );
		
		warning( "Weapon::GiveStartingAmmo", "Could not give ammo\n" );
		return;
	}
	
	sentient = reinterpret_cast<Sentient *>(entityToGiveAmmo);
	
	
	for ( mode=FIRE_MODE1; mode<MAX_FIREMODES; mode++ )
	{
		ammotype = GetAmmoType( ( firemode_t )mode );
		
		if ( ammotype.length() )
		{
			sentient->GiveAmmo( ammotype, this->GetStartAmmo( ( firemode_t )mode ), false );
		}
	}
}

void Weapon::giveAmmoBoost( Event *ev )
{
	str   ammotype;
	int   mode;
	Entity *entityToGiveAmmo;
	Sentient *sentient;
	
	

	entityToGiveAmmo = ev->GetEntity( 1 );
	
	if ( !entityToGiveAmmo || !entityToGiveAmmo->isSubclassOf( Sentient ) )
	{
		assert( entityToGiveAmmo );
		
		warning( "Weapon::GiveAmmoBoost", "Could not give ammo\n" );
		return;
	}
	
	sentient = reinterpret_cast<Sentient *>(entityToGiveAmmo);
	
	
	for ( mode = FIRE_MODE1 ; mode < MAX_FIREMODES ; mode++ )
	{
		ammotype = GetAmmoType( ( firemode_t )mode );
		
		if ( ammotype.length() )
		{
			sentient->GiveAmmo( ammotype, this->getAmmoBoost( ( firemode_t )mode ), true );
		}
	}
}

void Weapon::PickupWeapon( Event *ev )
{
	Sentient       *sen;
	Entity         *other;
	Weapon         *weapon;
	qboolean       hasweapon;
	
	other = ev->GetEntity( 1 );
	
	if ( !other )
		return;
	
	if ( !other->isSubclassOf( Sentient ) )
	{
		return;
	}
	
	sen = ( Sentient * )other;

	if ( !Pickupable( other ) )
		return;
	
	if ( ( sen == last_owner ) && ( level.time < last_owner_trigger_time ) )
	{
		return;
	}
	
	hasweapon = sen->HasItem( item_name );
	
	
	if ( hasweapon && !multiplayerManager.checkFlag( MP_FLAG_WEAPONS_STAY ) )
	{
		Event *event;
		
		event = new Event( EV_Weapon_GiveAmmoBoost );
		event->AddEntity( other );
		ProcessEvent( event );
		
		ItemPickup( other, false );
	}
	else
	{
		weapon = ( Weapon * )ItemPickup( other );
		
		if ( !weapon )
		{
			return;
		}
	}
}

void Weapon::ForceIdle( void )
{
	weaponstate = WEAPON_READY;
	
	
	float r = G_Random(100.0f);
	char anim[128];
	
	
	int animNum = gi.Anim_NumForName( edict->s.modelindex, "idle_onground" );
	if ( multiplayerManager.inMultiplayer() && animNum >= 0	&& animate->CurrentAnim() == animNum )
	{
		return;
	}
	
	if ( switchmode )
	{
		if ( r < 5.0f )
		{
			sprintf(anim,"idle_rare_mode%d",(int)curmode+1);
			if ( animate->HasAnim( anim ) )
			{
				animate->RandomAnimate( anim );
				return;
			}
		}
		
		if ( r < 15.0f )
		{
			sprintf(anim,"idle_uncommon_mode%d",(int)curmode+1);
			if ( animate->HasAnim( anim ) )
			{
				animate->RandomAnimate( anim );
				return;
			}
		}
		
		sprintf(anim,"idle_common_mode%d",(int)curmode+1);
		if ( animate->HasAnim( anim ) )
		{
			animate->RandomAnimate( anim );
			return;
		}
		
		animate->RandomAnimate("idle");
		return;
	}
	
	if ( r < 5.0f && animate->HasAnim("idle_rare") )
	{
		animate->RandomAnimate( "idle_rare" );
		return;
	}
	
	if ( r < 15.0 && animate->HasAnim("idle_uncommon") )
	{
		animate->RandomAnimate("idle_uncommon");
		return;
	}
	
	if ( animate->HasAnim("idle_common") )
	{
		animate->RandomAnimate( "idle_common" );
		return;
	}
	
	animate->RandomAnimate("idle");
}

void Weapon::DoneRaising( Event *ev )
{
	Event *event;
	
	weaponstate = WEAPON_READY;
	ForceIdle();
	
	if ( !owner )
	{
		PostEvent( EV_Remove, 0.0f );
		return;
	}
	
	event = new Event( EV_Weapon_DoneReloading );
	event->AddInteger( FIRE_MODE1 );
	ProcessEvent( event );
	
	event = new Event( EV_Weapon_DoneReloading );
	event->AddInteger( FIRE_MODE2 );
	ProcessEvent( event );
	
	if ( targetidle )
		PostEvent(EV_Weapon_TargetIdleThink, TargetIdleTime);
}

void Weapon::playAnim( const str &animName, bool animatingFlag )
{ 
	if ( animate->HasAnim( animName.c_str() ) )
	{
		if ( animatingFlag )
			weaponstate = WEAPON_ANIMATING;

		SetAnim( animName, EV_Weapon_DoneAnimating );
	}
}



void Weapon::SetAnim( const str	&animName, Event *endevent, bodypart_t bodypart )
{
	assert( animate );
	animate->RandomAnimate( animName.c_str(), endevent, bodypart );
	AddEffectsAnims();
}


void Weapon::SetAnim( const str	&animName, const Event &endevent, bodypart_t bodypart )
{
	assert( animate );
	animate->RandomAnimate( animName.c_str(), endevent, bodypart );
	AddEffectsAnims();
}

void Weapon::PassToAnimate( Event *ev )
{
	if ( !animate )
	{
		animate = new Animate( this );
	}
	Event *new_event = new Event( ev );
	animate->ProcessEvent( new_event );
	AddEffectsAnims();
}

void Weapon::AddEffectsAnims( void )
{
}

void Weapon::DoneAnimating( Event *ev )
{
	weaponstate = WEAPON_READY;
	
	if ( !owner )
		PostEvent( EV_Remove, 0.0f );
	
	return;
}

void Weapon::ClientFireDone( void )
{
}

void Weapon::DoneFiring( Event *ev )
{
	
	if ( shootingSkin )
		ChangeSkin( shootingSkin, false );
	
	if ( animate->HasAnim("fire_stop") )
		animate->RandomAnimate("fire_stop");
	else
		ForceIdle();
	
	if (
		( !HasAmmo( FIRE_MODE1 )   || ( !stricmp( ammo_type[ FIRE_MODE1 ], "None" ) ) ) &&
		( !HasAmmo( FIRE_MODE2 ) || ( !stricmp( ammo_type[ FIRE_MODE2 ], "None" ) ) ) &&
		auto_putaway
		)
	{
		PutAway();
	}
}

void Weapon::DoneReloading( Event *ev )
{
	int         amount_shadow1;
	int         amount_used;
	firemode_t  mode;
	firemode_t  clipToUse;
	
	mode = (firemode_t)ev->GetInteger( 1 );
	
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
	{
		warning( "Weapon::DoneReloading", "Invalid mode %d\n", mode );
	}
	
	
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;
	
	amount_shadow1 = ammo_clip_size[clipToUse] - ammo_in_clip[clipToUse];
	
	assert( owner );
	if ( owner && owner->isClient() && !UnlimitedAmmo( mode ) )
	{
		amount_used = owner->UseAmmo( ammo_type[mode], amount_shadow1 );
		
		if ( ammo_clip_size[clipToUse] )
			ammo_in_clip[clipToUse] = amount_used + ammo_in_clip[clipToUse];
		
		assert( ammo_in_clip[clipToUse] <= ammo_clip_size[clipToUse] );
		if ( ammo_in_clip[clipToUse] > ammo_clip_size[clipToUse] )
			ammo_in_clip[clipToUse] = ammo_clip_size[clipToUse];
		
		if ( burstmode[curmode] && ( ammo_in_clip[clipToUse] >= burstcountmax ) )
			burstcount = burstcountmax;
	}
	owner->AmmoAmountInClipChanged( ammo_type[mode], ammo_in_clip[clipToUse] );
	
	ForceIdle();
}

void Weapon::DoneReloadingBurst( Event *ev )
{
	firemode_t  mode;
	int		inBurstMode;
	firemode_t clipToUse; 

	mode = (firemode_t)ev->GetInteger( 1 );

	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );

	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
	{
		warning( "Weapon::DoneReloading", "Invalid mode %d\n", mode );
	}

	firemode_t realmode = mode;


	inBurstMode = false;

	if ( switchmode )
		inBurstMode = burstmode[curmode];
	else
		inBurstMode = burstmode[realmode];

	if ( !inBurstMode )
		return;


	int tmpamount = 0;
	Ammo *ammo;

	ammo = owner->FindAmmoByName( ammo_type[mode] );

	if ( ammo ) 
	{
		tmpamount = owner->AmmoCount( ammo_type[mode] );
		

		if ( usesameclip )
			clipToUse = FIRE_MODE1;
		else
			clipToUse = mode;

		if ( tmpamount == 0 ) 
			tmpamount = ammo_in_clip[clipToUse];

		if ( tmpamount > burstcountmax )
			burstcount = burstcountmax;
		else 
			burstcount = tmpamount;
		

		if ( burstcount == 0 )
			burstcount = -1;

		ForceIdle();
		return;
	}
}

qboolean Weapon::CheckReload( void )
{
	return CheckReload( curmode );
}

qboolean Weapon::CheckReload( firemode_t mode )
{
	int inBurstMode;
	firemode_t clipToUse;

	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );

	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
	{
		warning( "Weapon::CheckReload", "Invalid mode %d\n", mode );
		return false;
	}

	if ( !_autoReload )
	{
		return false;
	}

	if ( UnlimitedAmmo( mode ) )
	{
		return false;
	}

	if ( putaway )
	{
		return false;
	}

	if ( weaponstate != WEAPON_READY )
		return false;
	

	inBurstMode = false;

	if ( switchmode )
		inBurstMode = burstmode[curmode];
	else
		inBurstMode = burstmode[mode];

	firemode_t oldmode = mode;

	if ( inBurstMode && ( weaponstate != WEAPON_RELOADING ) && ( burstcount <= 0 ) )
	{
		Event *doneReloadingEvent;
		doneReloadingEvent = new Event( EV_Weapon_DoneReloadingBurst );
		doneReloadingEvent->AddInteger( oldmode );

		if ( _burstModeDelay[ mode ] > 0.0f )
		{
			animate->RandomAnimate( "reload_burst" );
			PostEvent( doneReloadingEvent, _burstModeDelay[ mode ] );
		}
		else
		{
			animate->RandomAnimate( "reload_burst", doneReloadingEvent );
		}

		weaponstate = WEAPON_RELOADING;
		return true;
	}

	mode = oldmode;

	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;

	if ( ammo_clip_size[clipToUse] && ( ammo_in_clip[clipToUse] < ammorequired[mode] ) && ( AmmoAvailable( mode ) >= ammorequired[mode] ) )
	{
		Event *ev1;

		ev1 = new Event( EV_Weapon_DoneReloading );
		ev1->AddInteger( mode );

		if ( animate->HasAnim( "reload_normal" ) && !multiplayerManager.skipWeaponReloads() )
		{
			weaponstate = WEAPON_RELOADING;
			animate->RandomAnimate( "reload_normal", ev1 );
		}
		else
		{
			ProcessEvent( ev1 );
		}
		return true;
	}


	return false;
}

//Weapon::ForceReload - Reload the clip if it's not full already
qboolean Weapon::ForceReload( void )
{
	firemode_t mode = FIRE_MODE1;
	firemode_t clipToUse;
	

	if ( !canReload() )
		return false;
	
	// Make sure it's not put away
	if ( putaway )
		return false;
	
	if ( _regenAmount[ FIRE_MODE1 ] > 0 )
		return false;
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;
	
	// If our clip is not full and we have ammo available...
	if ( ( ( ammo_clip_size[clipToUse] != ammo_in_clip[clipToUse] ) && AmmoAvailable( mode ) ) || targetidle)
	{
		Event *ev1;
		
		ev1 = new Event( EV_Weapon_DoneReloading );
		ev1->AddInteger( mode );
		
		if ( animate->HasAnim( "reload_normal" ) )
		{
			weaponstate = WEAPON_RELOADING;
			animate->RandomAnimate( "reload_normal", ev1 );
		}
		else
		{
			ProcessEvent( ev1 );
		}
		return true;
	}
	
	return false;
}

void Weapon::Idle( Event *ev )
{
	ForceIdle();
}

float	Weapon::GetMaxRange( void )
{
	return maxrange;
}

float	Weapon::GetMinRange( void )
{
	return minrange;
}

void Weapon::SetMaxRangeEvent( Event *ev )
{
	maxrange = ev->GetFloat( 1 );
}

void Weapon::SetMinRangeEvent( Event *ev )
{
	minrange = ev->GetFloat( 1 );
}

void Weapon::NotDroppableEvent( Event *ev )
{
	notdroppable = true;
}

void Weapon::SetMaxRange( float val )
{
	maxrange = val;
}

void Weapon::SetMinRange( float val )
{
	minrange = val;
}

void Weapon::WeaponSound( Event *ev )
{
	Event *e;
	
	if ( nextweaponsoundtime > level.time )
	{
		if ( owner )
		{
			owner->BroadcastSound(SOUND_RADIUS, SOUNDTYPE_WEAPONFIRE);;
		}
		else
		{
			BroadcastSound(SOUND_RADIUS, SOUNDTYPE_WEAPONFIRE);;
		}
		return;
	}
	
	if ( owner )
	{
		e = new Event( ev );
		owner->ProcessEvent( e );
	}
	else
	{
		Item::BroadcastSound();
	}
	
	nextweaponsoundtime = level.time + 0.4f;
}

qboolean Weapon::Removable( void )
{
	if ( multiplayerManager.checkFlag( MP_FLAG_WEAPONS_STAY ) && Respawnable() )
		return false;
	else
		return true;
}

qboolean Weapon::Pickupable( Entity *other )
{
	Sentient *sen;
	
	if ( !other->isSubclassOf( Sentient ) )
	{
		return false;
	}
	else if ( !other->isClient() )
	{
		return false;
	}

	if ( getSolidType() == SOLID_NOT )
	{
		return ( qboolean )NULL;
	}

	if ( multiplayerManager.inMultiplayer() && other->isSubclassOf( Player ) )
	{
		if ( !multiplayerManager.canPickup( (Player *)other, getMultiplayerItemType(), getName().c_str() ) )
			return false;
	}
	
	sen = ( Sentient * )other;
	
	
	if ( ( multiplayerManager.checkFlag( MP_FLAG_WEAPONS_STAY ) ) && Respawnable() )
	{
		Weapon   *weapon;
		
		weapon = ( Weapon * )sen->FindItem( getName() );
		
		if ( weapon )
			return false;
	}
	
	return true;
}

qboolean Weapon::AutoChange( void )
{
	return true;
}

int Weapon::ClipAmmo( firemode_t mode )
{
	firemode_t clipToUse;
	
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( !( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) ) )
		warning( "Weapon::ClipAmmo", "Invalid mode %d\n", mode );
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;
	
	if (ammo_clip_size[clipToUse])
		return ammo_in_clip[clipToUse];
	else
		return -1;
}

void Weapon::ProcessWeaponCommandsEvent( Event *ev )
{
	int index;
	
	index = ev->GetInteger( 1 );
	ProcessInitCommands( index );
}

void Weapon::SetActionLevelIncrement( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	action_level_increment[firemodeindex] = ev->GetInteger( 1 );
}

int Weapon::ActionLevelIncrement( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) )
		return action_level_increment[mode];
	else
	{
		warning( "Weapon::ActionLevelIncrement", "Invalid mode %d\n", mode );
		return 0;
	}
}

qboolean Weapon::IsDroppable( void )
{
	if ( notdroppable )
	{
		return false;
	}
	else
	{
		return true;
	}
}

void Weapon::SetFireType( Event *ev )
{
	str ftype;
	
	ftype = ev->GetString( 1 );
	
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	
	if ( !ftype.icmp( "projectile" ) )
		firetype[firemodeindex] = FT_PROJECTILE;
	else if ( !ftype.icmp( "bullet" ) )
		firetype[firemodeindex] = FT_BULLET;
	else if ( !ftype.icmp( "melee" ) )
		firetype[firemodeindex] = FT_MELEE;
	else if ( !ftype.icmp( "special_projectile" ) )
		firetype[firemodeindex] = FT_SPECIAL_PROJECTILE;
	else if ( !ftype.icmp( "none" ) )
		firetype[firemodeindex] = FT_NONE;
	else if ( !ftype.icmp( "explosion" ) )
		firetype[firemodeindex] = FT_EXPLOSION;
	else if ( !ftype.icmp( "triggerProjectile" ) )
		firetype[firemodeindex] = FT_TRIGGER_PROJECTILE;
	else if ( !ftype.icmp( "controlProjectile" ) )
		firetype[firemodeindex] = FT_CONTROL_PROJECTILE;
	else if ( !ftype.icmp( "controlZoom" ) )
		firetype[firemodeindex] = FT_CONTROL_ZOOM;
	else
		warning( "Weapon::SetFireType", "unknown firetype: %s\n", ftype.c_str() );
}

firetype_t Weapon::GetFireType( firemode_t mode )
{
	return firetype[mode];
}

void Weapon::SetProjectile( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	projectileModel[firemodeindex] = ev->GetString( 1 );
	CacheResource( projectileModel[firemodeindex].c_str(), this );
}

void Weapon::SetBulletDamage( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	bulletdamage[firemodeindex] = ev->GetFloat( 1 );
}

void Weapon::SetBulletKnockback( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	bulletknockback[firemodeindex] = ev->GetFloat( 1 );
}

void Weapon::SetBulletRange( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	bulletrange[firemodeindex] = ev->GetFloat( 1 );
}

void Weapon::SetRange( Event *ev )
{
	SetBulletRange( ev );
}

void Weapon::SetBulletCount( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	bulletcount[firemodeindex] = ev->GetFloat( 1 );
}

void Weapon::SetBulletSpread( float spreadX , float spreadY )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	bulletspread[firemodeindex].x = spreadX;
	bulletspread[firemodeindex].y = spreadY;
}

void Weapon::SetBulletSpread( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	bulletspread[firemodeindex].x = ev->GetFloat( 1 );
	if (ev->NumArgs() < 2) 
		bulletspread[firemodeindex].y = bulletspread[firemodeindex].x;
	else
		bulletspread[firemodeindex].y = ev->GetFloat( 2 );
	if (ev->NumArgs() == 5) 
	   {
		endbulletspread[firemodeindex].x = ev->GetFloat(3);
		endbulletspread[firemodeindex].y = ev->GetFloat(4);
		endbulletspread[firemodeindex].z = ev->GetFloat(5); 
	}
	else
	   {
		endbulletspread[firemodeindex].z = 0; 
	   }
}

void Weapon::SetHand( Event *ev )
{
	str side;
	
	side = ev->GetString( 1 );
	
	if ( !stricmp( side.c_str(), "righthand" ) || !stricmp( side.c_str(), "right" ) )
		hand = WEAPON_RIGHT;
	else if ( !stricmp( side.c_str(), "lefthand" ) || !stricmp( side.c_str(), "left" ) )
		hand = WEAPON_LEFT;
	else if ( !stricmp( side.c_str(), "dualhand" ) || !stricmp( side.c_str(), "dual" ) )
		hand = WEAPON_DUAL;
	else if ( !stricmp( side.c_str(), "any" ) )
		hand = WEAPON_ANY;
	else
	{
		warning( "Weapon::SetHand", "Unknown side %s\n", side.c_str() );
		assert( 0 );
	}
}

void Weapon::ModeSet( Event *ev )
{
	int i;
	str modestr;
	
	Event *modeev = new Event( ev->GetToken( 2 ) );
	
	modestr = ev->GetToken( 1 );
	
	firemodeindex = WeaponModeNameToNum(modestr);
	
	for( i=3; i<=ev->NumArgs(); i++ )
	{
		modeev->AddToken( ev->GetToken( i ) );
	}
	
	ProcessEvent( modeev );
	firemodeindex = FIRE_MODE1;
}

void Weapon::AutoAim( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		autoAimTargetSelectionAngle = ev->GetInteger( 1 );
		autoAimLockonAngle = ev->GetInteger( 2 );
	}
	else
	{
		autoAimTargetSelectionAngle = 60;
		autoAimLockonAngle = 15;
	}
}

void Weapon::Crosshair( Event *ev )
{
	crosshair = ev->GetBoolean( 1 );
}

void Weapon::TorsoAim( Event *ev )
{
	torsoaim = ev->GetBoolean( 1 );
}


void Weapon::AttachToTag(Event *ev)
{
    current_attachToTag = ev->GetString(1);
}

void Weapon::SetSpecialMove(Event *ev)
{
    special_move = ev->GetBoolean(1);
}

void Weapon::SetSecondaryAmmo(const char *type, int amount_shadow2, int startamount)
{
    ammo_type[1]      = type;
    ammorequired[1]   = amount_shadow2;
    startammo[1]      = startamount;
}

Vector Weapon::GetViewEndPoint()
{
    if (owner && owner->isSubclassOf(Player)) {
        trace_t trace;

        ((Player *)(Sentient *)owner)->GetViewTrace(trace, MASK_SHOT, 8192.0f);
        return trace.endpos;
    }

    return owner ? owner->centroid : origin;
}

WeaponProperties::WeaponProperties()
{
    _weaponProperties.ClearObjectList();
}

WeaponProperty WeaponProperties::getWeaponProperty(const str& name)
{
    int i;

    for (i = 1; i <= _weaponProperties.NumObjects(); i++) {
        if (_weaponProperties.ObjectAt(i).getProperty() == name) {
            return _weaponProperties.ObjectAt(i);
        }
    }

    return WeaponProperty(name);
}

void WeaponProperties::addWeaponProperty(const str& weaponProperty)
{
    int i;

    for (i = 1; i <= _weaponProperties.NumObjects(); i++) {
        if (_weaponProperties.ObjectAt(i).getProperty() == weaponProperty) {
            return;
        }
    }

    _weaponProperties.AddObject(WeaponProperty(weaponProperty));
}

void Weapon::LeftAttachToTag( Event *ev )
{
	left_attachToTag = ev->GetString( 1 );
}

void Weapon::RightAttachToTag( Event *ev )
{
	right_attachToTag = ev->GetString( 1 );
}

void Weapon::DualAttachToTag( Event *ev )
{
	dual_attachToTag = ev->GetString( 1 );
}

void Weapon::LeftHolsterAttachToTag( Event *ev )
{
	leftholster_attachToTag = ev->GetString( 1 );
}

void Weapon::RightHolsterAttachToTag( Event *ev )
{
	rightholster_attachToTag = ev->GetString( 1 );
}

void Weapon::DualHolsterAttachToTag( Event *ev )
{
	dualholster_attachToTag = ev->GetString( 1 );
}

void Weapon::SetLeftHolsterAngles( Event *ev )
{
	leftHolsterAngles = ev->GetVector( 1 );
}

void Weapon::SetRightHolsterAngles( Event *ev )
{
	rightHolsterAngles = ev->GetVector( 1 );
}

void Weapon::SetDualHolsterAngles( Event *ev )
{
	dualHolsterAngles = ev->GetVector( 1 );
}

void Weapon::SetHolsterScale( Event *ev )
{
	holsterScale = ev->GetFloat( 1 );
}

void Weapon::setWeildedScale( Event *ev )
{
	_weildedScale = ev->GetFloat( 1 );
}

void Weapon::SetQuiet( Event *ev )
{
	quiet = true;
}

void Weapon::SetLoopFire( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	loopfire[firemodeindex] = true;
}

void Weapon::SetMeansOfDeath( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	meansofdeath[firemodeindex] = (meansOfDeath_t )MOD_NameToNum( ev->GetString( 1 ) );
}

meansOfDeath_t Weapon::GetMeansOfDeath( firemode_t mode )
{
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) )
		return meansofdeath[mode];
	else
	{
		warning( "Weapon::GetMeansOfDeath", "Invalid mode %d\n", mode );
		return MOD_NONE;
	}
}

void Weapon::SetAimTarget( Entity *ent )
{
	aim_target = ent;
}

void Weapon::WorldHitSpawn( firemode_t mode, const Vector &origin_shadow1, const Vector &angles_shadow1, float life )
{
	if ( !worldhitspawn[mode].length() )
		return;
	
	Entity::SpawnEffect(worldhitspawn[mode], origin_shadow1, angles_shadow1, life);
}

void Weapon::SetWorldHitSpawn( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	worldhitspawn[firemodeindex] = ev->GetString( 1 );
}

void Weapon::MakeNoise( Event *ev )
{
	float radius = 500.0f;
	qboolean force = false;
	
	if ( ev->NumArgs() > 0 )
		radius = ev->GetFloat( 1 );
	
	if ( ev->NumArgs() > 1 )
		force = ev->GetBoolean( 2 );
	
	if ( attached && ( next_noise_time <= level.time || force ) )
	{
		BroadcastSound( radius , SOUNDTYPE_WEAPONFIRE );
		next_noise_time = level.time + 1.0f;
	}
}

void Weapon::SetViewModel( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		viewmodel = ev->GetString( 1 );
		
		gi.setviewmodel( edict, viewmodel );
	}
}

void Weapon::DonePutaway( Event *ev )
{

	weaponstate = WEAPON_LOWERING;

	if ( zoomed )
	{
		CancelEventsOfType( EV_Weapon_Zoom );	
		ProcessEvent( EV_Weapon_Zoom );	
	}

	if ( _controllingProjectile )
	{
		toggleProjectileControl();
	}



	if ( targetidle )
		CancelEventsOfType( EV_Weapon_TargetIdleThink );


}

void Weapon::SetRegenAmmo( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		_regenAmount[ firemodeindex ] = ev->GetInteger( 1 );	

		_regenTime[ firemodeindex ] = 1.0; 
	}

	if ( ev->NumArgs() > 1 )
	{
		_regenTime[ firemodeindex ] = ev->GetFloat( 2 );
	}

	turnThinkOn();
}

// Description:		Specifies that this weapon only regenerates ammo when it is idle (not shooting)

void Weapon::SetRegenOnlyWhenIdle( Event *ev )
{
	_regenOnlyWhenIdle[ firemodeindex ] = true;
}

void Weapon::ChangeIdle( Event *ev )
{
	ForceIdle();
	
	if ( !owner )
	{
		return;
	}
}

void Weapon::DrawBowStrain( Event *ev )
{
	if ( animate->HasAnim( "draw_strain" ) )
		animate->RandomAnimate( "draw_strain" );
}

void Weapon::AltDrawBowStrain( Event *ev )
{
	if ( animate->HasAnim( "alternate_draw_strain" ) )
		animate->RandomAnimate( "alternate_draw_strain" );
}

void Weapon::SetAccuracy( Event *ev )
{
	if ( ev->NumArgs() < 6 )
	{
		gi.DPrintf("SetAccuracy: Too few parameters\n");
		return;
	}
	firemode_t mode = WeaponModeNameToNum( ev->GetToken( 1 ) );
	accuracy[mode][ACCURACY_STOPPED] = ev->GetFloat( 2 );
	accuracy[mode][ACCURACY_CHANGE] = ev->GetFloat( 3 );
	accuracy[mode][ACCURACY_WALK] = ev->GetFloat( 4 );
	accuracy[mode][ACCURACY_RUN] = ev->GetFloat( 5 );
	accuracy[mode][ACCURACY_CROUCH] = ev->GetFloat( 6 );
}

void Weapon::SetZoomFOV( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		zoomfov = ev->GetFloat( 1 );
	}
}

void Weapon::SetReticuleTime( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		reticuletime = ev->GetFloat( 1 );
}

void Weapon::SetStartZoom( Event* ev )
{
	if(ev == 0)
		return;
	
	startzoom = ev->GetFloat(1);
}

void Weapon::SetEndZoom( Event* ev )
{
	if(ev == 0)
		return;
	
	endzoom = ev->GetFloat(1);
}

void Weapon::SetZoomTime( Event* ev )
{
	if(ev == 0)
		return;
	
	zoomtime = ev->GetFloat(1);
}

void Weapon::SetAimType( Event *ev )
{
	
	str aimstr;
	
	Sentient *owner_shadow3;
	Player *player = NULL;
	owner_shadow3 = this->owner;
	assert( owner_shadow3 );
	if ( owner_shadow3->isSubclassOf( Player ) )
		player = ( Player * )owner_shadow3;
	
	if ( ev->NumArgs() > 0 )
		aimstr = ev->GetString( 1 );
	
	if ( ( player && player->GetCrouch() ) || ( aimstr == "crouch" ) )
		aimtype = ACCURACY_CROUCH;
	else 
	{
		if ( aimstr == "stopped" )
			aimtype = ACCURACY_STOPPED;	
		if ( aimstr == "walk" )
			aimtype = ACCURACY_WALK;
		if ( aimstr == "run" )
			aimtype = ACCURACY_RUN;
		if ( aimstr == "change" )
			aimtype = ACCURACY_CHANGE;
	}
	
	if ( ( aimstr != "stopped" ) && ( aimstr != "walk" ) && ( aimstr != "run" ) && ( aimstr != "crouch" ) && ( aimstr != "change" ) )
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
		warning("SetAimType: Invalid aimtype, %s, defaulting to stopped",aimstr.c_str());
#pragma GCC diagnostic pop
		aimtype = ACCURACY_STOPPED;
	}
	
	if ( g_showaccuracymod->integer )
		gi.DPrintf("Accuracy Mod: %s = %f\n", aimstr.c_str(), accuracy[curmode][aimtype] );
}

void Weapon::SetFireTimer( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	
	fire_timer[firemodeindex] = ev->GetFloat( 1 );
}

void Weapon::UseSameClip( Event *ev )
{
	usesameclip = true;
}

qboolean Weapon::HasFullClip( void )
{
	firemode_t mode = FIRE_MODE1;
	firemode_t clipToUse;
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;
	
	if ( ammo_in_clip[clipToUse] == ammo_clip_size[clipToUse] )
		return true;
	
	return false;
}

void Weapon::SetMaxModes( Event *ev )
{
	if ( ev->NumArgs() < 1 )
	{
		maxmode = FIRE_MODE2;
		return;
	}
	
	maxmode = WeaponModeNameToNum( ev->GetToken( 1 ) );
}

void Weapon::SetSwitchMode( Event *ev )
{
	switchmode = true;
}

void Weapon::TargetIdle( Event *ev )
{
	targetidle = true;
}

void Weapon::TargetIdleThink( Event *ev )
{
	CancelEventsOfType( EV_Weapon_TargetIdleThink );
	
	if  ( animate->GetName() == "putaway" )
		return;
	
	PostEvent( EV_Weapon_TargetIdleThink, TargetIdleTime );
	
	Sentient *owner_shadow2;
	Player *player;
	owner_shadow2 = this->owner;
	assert( owner_shadow2 );
	
	if ( owner_shadow2->isSubclassOf( Player ) )
	{
		player = ( Player * )owner_shadow2;
		Vector pos, forward, right, up, endpoint, vorg;   
		GetMuzzlePosition( &pos, &forward, &right, &up );
		vorg = player->origin;
		vorg.z += player->viewheight;
		
		endpoint = vorg + (forward * 4000.0f);
		trace_t trace = G_Trace( pos, vec_zero, vec_zero, endpoint, player, MASK_SHOT, true, "Weapon::TargetIdleThink" );
		if ( trace.ent && ( trace.entityNum != ENTITYNUM_WORLD ) && trace.ent->entity->isSubclassOf( Sentient ))
		{
			if ( !trace.ent->entity->deadflag )
			{
				if ( !strstr(animate->GetName(), "idle_target") )
					animate->RandomAnimate( "idle_target" );
				targetidleflag = true;
			}
			return;
		}
		else 
		{
			if ( targetidleflag )
			{
				targetidleflag = false;
				ForceIdle();
			}
		}
			
	}
}

void Weapon::SwitchMode( void )
{
	char anim[128];
	
	Event *ev1;
	ev1 = new Event( EV_Weapon_DoneSwitchToMiddle );
	
	strcpy(anim,"");
	sprintf(anim,"switch_mode%d_neutral",(int)curmode+1);
	
	if ( animate->HasAnim( anim ) )
	{
		weaponstate = WEAPON_SWITCHINGMODE;
		animate->RandomAnimate( anim, ev1 );
	}
	else
	{
		ProcessEvent( ev1 );
	}
}

void Weapon::DoneSwitchToMiddle( Event *ev )
{
	int mode;
	char anim[128];
	
	Event *ev1;
	ev1 = new Event( EV_Weapon_DoneSwitching );
	
	if ( curmode == maxmode )
		mode = FIRE_MODE1;
	else
		mode = (int)curmode + 1;
	
	strcpy(anim,"");
	sprintf(anim,"switch_mode%d",mode);
	
	if ( animate->HasAnim( anim ) )
	{
		weaponstate = WEAPON_SWITCHINGMODE;
		animate->RandomAnimate( anim, ev1 );
	}
	else
	{
		ProcessEvent( ev1 );
	}
}

void Weapon::DoneSwitching( Event *ev )
{
	if ( curmode == maxmode )
		curmode = FIRE_MODE1;
	else
		curmode = (firemode_t)((int)curmode + 1);
	
	weaponstate = WEAPON_READY;
	
	INITIALIZE_WEAPONMODE_VAR(next_fire_time, 0.0);
	
	ForceIdle();
}

void Weapon::SetBurstMode( Event *ev )
{
	burstmode[firemodeindex] = true;
	if ( ev->NumArgs() > 0 )
		burstcountmax = ev->GetInteger( 1 );
	else
		burstcountmax = 10; 
	
	burstcount = burstcountmax;
}

void Weapon::setBurstModeDelay( Event *ev )
{
	_burstModeDelay[ firemodeindex ] = ev->GetFloat( 1 );
}

void Weapon::SetCHOffset(int chx_shadow1, int chy_shadow1)
{
	this->chx = chx_shadow1;
	this->chy = chy_shadow1;
}



void Weapon::CheckForTargetedEntity( void )
{

}

void Weapon::SetRealViewOrigin( const Vector &rv )
{
	realvieworg = rv;
}

void Weapon::SetThirdPerson( qboolean tp )
{
	thirdperson = tp;
}

qboolean Weapon::IsDoneFiring()
{
	return donefiring;
}

void Weapon::StartFiring( Event *ev )
{
	startfiretime = level.time;
}

void Weapon::FinishedFiring( Event *ev )
{
	int oldDoneFiring;

	oldDoneFiring = donefiring;

	donefiring = ev->GetInteger( 1 );

	if ( shootingSkin )
		ChangeSkin( shootingSkin, false );

	if ( donefiring && !oldDoneFiring )
	{
		CheckReload();
	}
}

void Weapon::Zoom( Event *ev )
{
	Player *player = NULL;
	
	if ( owner && owner->isSubclassOf( Player ) )
		player = (Player *)(Sentient *)owner;
	
	if ( !player )
		return;
	
	if ( zoomed )
	{
		endZoom();
	}
	else
	{
		zoomfov = startzoom;
		startzoomtime = level.time;
		zoomed = true;
		
		player->SetFov( zoomfov, true );
		player->client->ps.pm_flags |= PMF_ZOOM;

	}
}

void Weapon::endZoom( Event *ev )
{
	if ( zoomed )
		_lastZoomFov = zoomfov;
	else
		_lastZoomFov = sv_defaultFov->value;

	endZoom();
}

void Weapon::rezoom( Event *ev )
{
	Player *player = NULL;
	
	if ( owner && owner->isSubclassOf( Player ) )
		player = (Player *)(Sentient *)owner;
	
	if ( !player )
		return;

	if ( _lastZoomFov < sv_defaultFov->value )
	{
		zoomed = true;
		zoomfov = _lastZoomFov;
			
		player->SetFov( zoomfov, true );
		player->client->ps.pm_flags |= PMF_ZOOM;
	}
}

void Weapon::endZoom( void )
{
	Player *player = NULL;
	float defaultFov;
	
	if ( owner && owner->isSubclassOf( Player ) )
		player = (Player *)(Sentient *)owner;
	
	if ( !player )
		return;

	defaultFov = player->getDefaultFov();

	player->SetFov( defaultFov, true );
		
	player->client->ps.pm_flags &= ~PMF_ZOOM;

	player->removeHud( "zoomhud" );

	_zoomStage = ZOOM_NORMAL_FOV;
	zoomed = false;
}

void Weapon::IncrementZoom( Event* ev )
{
	
	if(!zoomed)
		return;
	
	Player *player = NULL;
	
	if ( owner && owner->isSubclassOf( Player ) )
		player = (Player *)(Sentient *)owner;
	
	
	float zoomPercentage = ((float)level.time - startzoomtime) / zoomtime;
	zoomfov = ((endzoom - startzoom) * zoomPercentage) + startzoom;
	if(zoomfov < endzoom)
		zoomfov = endzoom;
	
	player->SetFov( zoomfov, true );
}


void Weapon::setZoomStage( Event* ev )
{
	changeZoomStage( ev->GetFloat( 1 ), ev->GetFloat( 2 ) );
}

void Weapon::changeZoomStage( float firstZoom, float secondZoom )
{
	Player* player = NULL;

	if ( owner && owner->isSubclassOf( Player ) )
		player = (Player *)(Sentient *)owner;

	if(player == NULL)
		return;


	switch( _zoomStage )
	{
		case ZOOM_NORMAL_FOV:
			_zoomStage = ZOOM_STAGE_1;
			player->SetFov( firstZoom, true );
			player->addHud( "zoomhud" );
			break;

		case ZOOM_STAGE_1:
			_zoomStage = ZOOM_STAGE_2;
			player->SetFov( secondZoom, true );
			player->addHud( "zoomhud" );
			break;

		case ZOOM_STAGE_2:
			_zoomStage = ZOOM_NORMAL_FOV;
			endZoom();
			return;
		default:
			break;
		}

	player->client->ps.pm_flags |= PMF_ZOOM;
}


void Weapon::SetTargetingSkin( Event *ev )
{
	targetingSkin = ev->GetInteger( 1 );
}

void Weapon::SetShootingSkin( Event *ev )
{
	shootingSkin = ev->GetInteger( 1 );
}

void Weapon::setFullAmmoSkin( Event *ev )
{
	_fullAmmoSkin = ev->GetInteger( 1 );

	_fullAmmoMode = WeaponModeNameToNum( ev->GetString( 2 ) );

	turnThinkOn();
}


void Weapon::ProcessTargetedEntity( EntityPtr entity )
{
	
	Player* player;
	assert(owner);

	if ( !owner )
		return;

	player =  (Player*) (Sentient*)owner;
	if(player == 0)
		return;
	
	if ( targetingSkin )
	{
		ChangeSkin( targetingSkin, false );
		
		if ( entity && entity->isSubclassOf( Actor ) )
		{
			
			Actor *actor = (Actor *)entity.Pointer();
			
			if ( !actor->deadflag && ( actor->actortype == IS_ENEMY ) )
				ChangeSkin( targetingSkin, true );
				
		}
	}

	if(entity != 0)
	{
		entity->edict->s.eFlags |= EF_DISPLAY_DESC1;
	}

}

// Description:		Sets the move speed modifier (when not firing)
void Weapon::Uninitialize(void)
{
	if( _zoomStage == ZOOM_STAGE_1 || _zoomStage == ZOOM_STAGE_2 )
	{
		endZoom();
	}
}



// Description:		Sets the move speed modifier (when not firing)
void Weapon::setMoveSpeedModifier( Event *ev )
{
	defaultMoveSpeedModifier = ev->GetFloat( 1 );
}

void Weapon::setShootingMoveSpeedModifier( Event *ev )
{
	shootingMoveSpeedModifier[ firemodeindex ] = ev->GetFloat( 1 );
}

float Weapon::getMoveSpeedModifier( void )
{
	if ( weaponstate == WEAPON_FIRING )
		return shootingMoveSpeedModifier[ curmode ];
	else
		return defaultMoveSpeedModifier;
}

void Weapon::UseActorAiming( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		useActorAiming = ev->GetBoolean( 1 );
	else
		useActorAiming = true;
}

void Weapon::updateViewShake( void )
{
	if ( _viewShakeMagnitude[ curmode ] > 0.0f )
	{

		viewShake[ PITCH ] = G_CRandom() * _viewShakeMagnitude[ curmode ];
		viewShake[ YAW ]   = G_CRandom() * _viewShakeMagnitude[ curmode ];
		viewShake[ ROLL ]  = G_CRandom() * _viewShakeMagnitude[ curmode ] * 1.5;
	}
	else if ( ( _viewMinShake[ curmode ] != vec_zero ) || ( _viewMaxShake[ curmode ] != vec_zero ) )
	{
		Vector minShake;
		Vector maxShake;


		minShake = _viewMinShake[ curmode ];
		maxShake = _viewMaxShake[ curmode ];

		if ( _viewShakeOverride[ curmode ] )
		{
			viewShake[ PITCH ] = G_Random() * ( maxShake[ PITCH ] - minShake[ PITCH ] ) + minShake[ PITCH ];
			viewShake[ YAW   ] = G_Random() * ( maxShake[ YAW   ] - minShake[ YAW   ] ) + minShake[ YAW   ];
			viewShake[ ROLL  ] = G_Random() * ( maxShake[ ROLL  ] - minShake[ ROLL  ] ) + minShake[ ROLL  ];
		}
		else
		{
			viewShake[ PITCH ] += G_Random() * ( maxShake[ PITCH ] - minShake[ PITCH ] ) + minShake[ PITCH ];
			viewShake[ YAW   ] += G_Random() * ( maxShake[ YAW   ] - minShake[ YAW   ] ) + minShake[ YAW   ];
			viewShake[ ROLL  ] += G_Random() * ( maxShake[ ROLL  ] - minShake[ ROLL  ] ) + minShake[ ROLL  ];
		}

		for ( int i = 0 ; i < 3 ; i++ )
		{
			if ( viewShake[ i ] > _maxViewShakeChange )
				viewShake[ i ] = _maxViewShakeChange;
			else if ( viewShake[ i ] < -_maxViewShakeChange )
				viewShake[ i ] = -_maxViewShakeChange;

		}
	}
}

void Weapon::startViewShake( Event *ev )
{
	startViewShake();
}
void Weapon::startViewShake( void )
{
	if ( ( _viewShakeMagnitude[ curmode ] > 0.0f ) || ( _viewMinShake[ curmode ] != vec_zero ) || ( _viewMaxShake[ curmode ] != vec_zero ) )
	{

		updateViewShake();

		{
			CancelEventsOfType( EV_Weapon_ReduceViewShake );
			PostEvent( EV_Weapon_ReduceViewShake, FRAMETIME );
		}

	}
}

void Weapon::reduceViewShake( Event *ev )
{
	int i;
	bool repost;


	viewShake *= 0.75;


	repost = false;

	for ( i = 0 ; i < 3 ; i++ )
	{
		if ( ( viewShake[ i ] > 0.1f ) || ( viewShake[ i ] < -0.1f ) )
		{
			repost = true;
		}
	}

	if ( repost )
	{
		PostEvent( EV_Weapon_ReduceViewShake, FRAMETIME );
	}
	else
	{
		viewShake = vec_zero;
	}
}

void Weapon::clearViewShake( Event *ev )
{
	viewShake = vec_zero;
	CancelEventsOfType( EV_Weapon_ReduceViewShake );
}

Vector Weapon::getViewShake( void )
{
	return viewShake;
}

void Weapon::setViewShakeInfo( Event *ev )
{
	_viewShakeMagnitude[ firemodeindex ] = ev->GetFloat( 1 );

	if ( ev->NumArgs() > 1 )
	{
		_viewShakeDuration[ firemodeindex ] = ev->GetFloat( 2 );
	}
	else
	{
		_viewShakeDuration[ firemodeindex ] = FRAMETIME;
	}
}

void Weapon::setAdvancedViewShakeInfo( Event *ev )
{
	_viewMinShake[ firemodeindex ] = ev->GetVector( 1 );
	_viewMaxShake[ firemodeindex ] = ev->GetVector( 2 );

	if ( ev->NumArgs() > 2 )
	{
		_viewShakeDuration[ firemodeindex ] = ev->GetFloat( 3 );
	}
	else
	{
		_viewShakeDuration[ firemodeindex ] = FRAMETIME;
	}

	if ( ev->NumArgs() > 3 )
	{
		_viewShakeOverride[ firemodeindex ] = ev->GetBoolean( 4 );
	}
	else
	{
		_viewShakeOverride[ firemodeindex ] = false;
	}
}


void Weapon::SetPowerRating( Event *ev )
{
	SetPowerRating( ev->GetFloat( 1 ) );
}

float Weapon::GetPowerRating()
{
	if ( world && world->isThingBroken( item_name.c_str() ) )
		return 0.0f;

	return _powerRating;
}


void Weapon::SetProjectileDamage( Event *ev )
{
	SetProjectileDamage(ev->GetFloat( 1 ) );
}

void Weapon::SetProjectileSpeed( Event *ev )
{
	SetProjectileSpeed(ev->GetFloat( 1 ) );
}

void Weapon::AdvancedMeleeAttack(const char* tag1, const char* tag2, bool criticalHit)
{
	Vector startpoint, endpoint;
	Vector tagpos, tagpos2, vect;
	float damage, knockback;
	meansOfDeath_t meansofdeath_shadow1;
	
	GetActorMuzzlePosition(&tagpos, NULL, NULL, NULL, tag1); 
	GetActorMuzzlePosition(&tagpos2, NULL, NULL, NULL, tag2); 
	startpoint = tagpos2;
	vect = tagpos - tagpos2;
	vect.z = 0;
	vect.normalize();
	endpoint = startpoint + (vect*48);
	
	
	damage = bulletdamage[FIRE_MODE1];
	knockback = bulletknockback[FIRE_MODE1];
	
	meansofdeath_shadow1 = GetMeansOfDeath( FIRE_MODE1 );
	
	if ( owner->isSubclassOf( Player ) )
	{
		Player *player = (Player *)(Sentient *)owner;
		
		meansofdeath_shadow1 = player->changetMeansOfDeath( meansofdeath_shadow1 );
		
		damage = player->getDamageDone( damage, meansofdeath_shadow1, true );
		
		knockback = (knockback + player->GetPlayerKnockback()) * player->GetKnockbackMultiplier();
	}
	
	Sentient *owner_shadow1;
	Player *player;
	owner_shadow1 = this->owner;
	assert( owner_shadow1 );
	if ( owner_shadow1->isSubclassOf( Player ) )
	{
		player = ( Player * )owner_shadow1;
		if ( !player->in_melee_attack )
			meleeVictims.ClearObjectList();
	}
	
	if ( !MeleeAttack( startpoint, endpoint, damage, owner_shadow1, meansofdeath_shadow1, 15.0f, -45.0f, 45.0f, knockback, true, &meleeVictims, this, criticalHit ) )
	{
		trace_t trace = G_Trace( startpoint, Vector( -8.0f, -8.0f, -8.0f ), Vector( 8.0f, 8.0f, 8.0f ), endpoint, owner_shadow1, MASK_MELEE, false, "Weapon::Shoot" );
		
		Entity *victim = G_GetEntity( trace.entityNum );
		
		if ( victim && ( ( victim == world ) || ( victim->takedamage == DAMAGE_NO ) ) )
		{
			vec3_t  newangles;
			vectoangles( trace.plane.normal, newangles );
			WorldHitSpawn( FIRE_MODE1, trace.endpos, newangles, 0.1f );
			str realname = this->GetRandomAlias( "impact_world" );
			if ( realname.length() > 1 )
				this->Sound( realname, CHAN_VOICE );
		}
	}
	
	if ( !quiet )
	{
		if ( next_noise_time <= level.time )
		{
			BroadcastSound();
			next_noise_time = level.time + 1.0f;
		}
	}
}


void Weapon::applySpread( Vector *forward, Vector *right, Vector *up )
{
	Vector spread;


	if (( !forward ) || (!up) || (!right))
		return;

	spread = getSpread();
	
	*forward = ( *forward * bulletrange[curmode]) + 
		( *right * G_CRandom( spread.x ) ) +
		( *up * G_CRandom( spread.y ) );
	
	forward->normalize();
	*right = Vector::Cross(*forward,*up);
	*up = Vector::Cross(*right,*forward);	
}

Vector Weapon::getSpread( void )
{
	Vector spread;
	

	if ( endbulletspread[ curmode ].z )
	{
		float timemult = (level.time - startfiretime) / endbulletspread[curmode].z;

		if (timemult > 1) 
			timemult = 1;

		spread.x = timemult * (endbulletspread[curmode].x - bulletspread[curmode].x) + bulletspread[curmode].x;
		spread.y = timemult * (endbulletspread[curmode].y - bulletspread[curmode].y) + bulletspread[curmode].y;
	}
	else
	{
		spread.x = bulletspread[curmode].x;
		spread.y = bulletspread[curmode].y;
	}

	return spread;
}


// Parameters:		Event *ev -- not used
void Weapon::processGameplayData( Event *ev )
{
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(getArchetype()) )
		return;

	str objname = getArchetype();
	str useanim, usetype, usethread;
	if ( gpm->hasProperty(objname, "useanim") )
		useanim = gpm->getStringValue(objname, "useanim");
	if ( gpm->hasProperty(objname, "usethread") )
		usethread = gpm->getStringValue(objname, "usethread");
	if ( gpm->hasProperty(objname + ".Pickup", "icon") )
		usetype = gpm->getStringValue(objname + ".Pickup", "icon");

	if ( useanim.length() || usethread.length() || usetype.length() )
	{
		if ( !useData )
			useData = new UseData();
		
		useData->setUseAnim(useanim);
		useData->setUseThread(usethread);
		useData->setUseType(usetype);
	}
}

float Weapon::RespawnTime( void )
{
	if ( multiplayerManager.inMultiplayer() )
		return respawntime * multiplayerManager.getWeaponRespawnMultiplayer();
	else
		return respawntime;
}

void Weapon::SetArcProjectile( Event *ev )
{
	_arcProjectile = ev->GetBoolean( 1 );
}

void Weapon::SetLowArcRange( Event *ev )
{
	_lowArcRange = ev->GetFloat( 1 );
}

void Weapon::SetPlayMissSound( Event *ev )
{
	_playMissSound = ev->GetBoolean( 1 );
}

void Weapon::noAmmoMode( Event * )
{
	_noAmmoMode[ firemodeindex ] = true;
}

bool Weapon::hasNoAmmoMode( firemode_t mode )
{
	
	return _noAmmoMode[ mode ];
}

void Weapon::setNoDelay( Event *ev )
{
	_noDelay[ firemodeindex ] = true;
}

bool Weapon::isModeNoDelay( firemode_t mode )
{
	
	return _noDelay[ mode ];
}

void Weapon::pauseRegen( Event *ev )
{
	int i;

	for( i = 0 ; i < MAX_FIREMODES ; i++ )
	{
		if ( ( _regenAmount[ i ] > 0 ) )
		{
			_nextRegenTime[ i ] = level.time + _regenTime[ i ] + 0.5f;
		}
	}
}

void Weapon::setChargedModels( Event *ev )
{
	assert( ( firemodeindex >= 0 ) && ( firemodeindex < MAX_FIREMODES ) );
	_chargedModels[ firemodeindex ] = ev->GetInteger( 1 );
}

void Weapon::setControllingProjectile( Event *ev )
{
	if ( ev->NumArgs() > 0 )
	{
		_controllingProjectile = ev->GetBoolean( 1 );
	}
	else
	{
		_controllingProjectile = true;
	}
}

bool Weapon::getControllingProjectile( void )
{
	return _controllingProjectile;
}

void Weapon::setCanInterruptFiringState( Event *ev )
{
	if ( ev->NumArgs() > 0 )
		_canInterruptFiringState = ev->GetBoolean( 1 );
	else
		_canInterruptFiringState = true;
}

void Weapon::setSpreadAnimData( Event *ev )
{
	_spreadAnims[ firemodeindex ] = ev->GetInteger( 1 );
	_spreadTime[ firemodeindex ]  = ev->GetFloat( 2 );
}

bool Weapon::canReload( void )
{
	if ( weaponstate != WEAPON_READY )
		return false;

	return true;
}

int Weapon::getAmmoInClip( firemode_t mode )
{
	firemode_t clipToUse;
	
	assert( ( mode >= 0 ) && ( mode < MAX_FIREMODES ) );
	
	if ( usesameclip )
		clipToUse = FIRE_MODE1;
	else
		clipToUse = mode;

	if ( ( clipToUse >= 0 ) && ( clipToUse < MAX_FIREMODES ) )
	{
		if ( ammo_clip_size[ clipToUse ] )
		{
			return ammo_in_clip[ clipToUse ];
		}
	}

	return 0;
}

void Weapon::setMaxViewShakeChange( Event *ev )
{
	_maxViewShakeChange = ev->GetFloat( 1 );
}

void Weapon::setControlParms( Event *ev )
{
	_controlEmitterName = ev->GetString( 1 );
	_controlSoundName   = ev->GetString( 2 );
}

void Weapon::toggleProjectileControl( void )
{
	if ( _controllingProjectile )
	{

		_controllingProjectile = false;


		StopLoopSound();


		clearCustomEmitter( _controlEmitterName );
	}
	else
	{

		_controllingProjectile = true;


		LoopSound( _controlSoundName );

		if ( !_controllingProjectileHidden )
		{

			setCustomEmitter( _controlEmitterName );
		}
	}
}

void Weapon::setProjectileControlHidden( Event *ev )
{
	_controllingProjectileHidden = ev->GetBoolean( 1 );

	if ( _controllingProjectileHidden && _controllingProjectile )
	{

		clearCustomEmitter( _controlEmitterName );
	}
	else if ( !_controllingProjectileHidden && _controllingProjectile )
	{

		setCustomEmitter( _controlEmitterName );
	}
}

void Weapon::setMeleeParms( Event *ev )
{
	_meleeWidth[ firemodeindex ] = ev->GetFloat( 1 );
	_meleeHeight[ firemodeindex ] = ev->GetFloat( 2 );
	_meleeLength[ firemodeindex ] = ev->GetFloat( 3 );
}

void Weapon::setFireOffset( Event *ev )
{
	_fireOffset[ firemodeindex ] = ev->GetVector( 1 );
}

void Weapon::cacheStrings( void )
{
	G_FindConfigstringIndex( va( "$$PickedUpThe$$ $$Weapon-%s$$\n", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
}

void Weapon::setAutoReload( Event *ev )
{
	_autoReload = ev->GetBoolean( 1 );
}

void Weapon::setAllowAutoSwitch( Event *ev )
{
	_allowAutoSwitch = ev->GetBoolean( 1 );
}

void Weapon::forceReload( Event *ev )
{
	ForceReload();
}

void Weapon::Think( void )
{
	int i;

	for( i = 0 ; i < MAX_FIREMODES ; i++ )
	{
		if ( owner && ( _regenAmount[ i ] > 0 ) && ( _nextRegenTime[ i ] < level.time ) )
		{
			owner->GiveAmmo( ammo_type[ i ], _regenAmount[ i ], false );

			_nextRegenTime[ i ] = level.time + _regenTime[ i ];
		}
	}

	if ( attached && _fullAmmoSkin )
	{
		bool full = false;
		firemode_t modeToCheck;

		if ( usesameclip )
			modeToCheck = FIRE_MODE1;
		else
			modeToCheck = _fullAmmoMode;

		if ( ammo_clip_size[ modeToCheck ] > 0 )
		{
			if ( ammo_in_clip[ modeToCheck ] == ammo_clip_size[ modeToCheck ] )
				full = true;
		}
		else
		{
			if ( ( owner ) && ( owner->AmmoCount( ammo_type[ modeToCheck ] ) == owner->MaxAmmoCount( ammo_type[ modeToCheck ] ) ) )
				full = true;
		}

		if ( full )
		{
			ChangeSkin( _fullAmmoSkin, true );
		}
		else
		{
			ChangeSkin( _fullAmmoSkin, false );
		}
	}
}

bool Weapon::shouldAutoSwitch( firemode_t mode )
{
	Player *player;
	int i;
	firemode_t modeToCheck;


	if ( !owner || !owner->isSubclassOf( Player ) )
		return false;

	player = (Player *)(Sentient *)owner;

	if ( HasAmmo( mode ) )
		return false;

	if ( !_allowAutoSwitch )
		return false;

	if ( player->edict->svflags & SVF_BOT )
		return true;

	if ( !player->getAutoSwitchWeapons() )
		return false;

	for ( i = 0 ; i < MAX_FIREMODES ; i++ )
	{
		if ( usesameclip )
			modeToCheck = FIRE_MODE1;
		else
			modeToCheck = (firemode_t)i;

		if ( HasAmmo( modeToCheck ) )
			return false;

		if ( firetype[ i ] == FT_TRIGGER_PROJECTILE )
		{
			if ( _nextSwitchTime > level.time )
			{
				return false;
			}
		}
	}

	return true;
}

int Weapon::getWeaponPriority( void )
{
	int priority;
	str currentWeaponName;

	for ( priority = 0 ; priority < 14 ; priority++ )
	{
		currentWeaponName = getWeaponByPriority( priority );

		if ( currentWeaponName == getName() )
			return priority;
	}

	return 100;
}

str Weapon::getWeaponByPriority( int priority )
{
	str weaponName;

	switch ( priority )
	{
	case 0 :
		weaponName = "PhotonBurst";
		break;
	case 1 :
		weaponName = "TetryonGatlingGun";
		break;
	case 2 :
		weaponName = "FederationSniperRifle";
		break;
	case 3 :
		weaponName = "RomulanRadGun";
		break;
	case 4 :
		weaponName = "AttrexianRifle";
		break;
	case 5 :
		weaponName = "BurstRifle";
		break;
	case 6 :
		weaponName = "GrenadeLauncher";
		break;
	case 7 :
		weaponName = "FieldAssaultRifle";
		break;
	case 8 :
		weaponName = "I-Mod";
		break;
	case 9 :
		weaponName = "CompressionRifle";
		break;
	case 10 :
		weaponName = "RomulanDisruptor";
		break;
	case 11 :
		weaponName = "DrullStaff";
		break;
	case 12 :
		weaponName = "Phaser-stx";
		break;
	case 13 :
		weaponName = "Phaser";
		break;
	}

	return weaponName;
}

void Weapon::autoSwitch()
{
	str weaponName;
	Weapon *weapon;
	Item *item;
	int i;
	Player *player;
	int ammoAvailable;

	if ( !owner || !owner->isSubclassOf( Player ) )
		return;

	player = (Player *)(Sentient *)owner;

	for ( i = 0 ; i < 14 ; i++ )
	{
		weaponName = getWeaponByPriority( i );

		if ( player->HasItem( weaponName ) )
		{
			item = player->FindItem( weaponName );

			if ( item && item->isSubclassOf( Weapon ) )
			{
				weapon = (Weapon *)item;

				ammoAvailable = weapon->getAmmoInClip( FIRE_MODE1 ) + weapon->AmmoAvailable( FIRE_MODE1 );

				if ( ammoAvailable > weapon->ammorequired[ FIRE_MODE1 ] )
				{
					Event *newEvent = new Event( EV_Player_UseItem );
					newEvent->AddString( weaponName );
					player->ProcessEvent( newEvent );

					return;
				}
			}
		}
	}
}

void Weapon::setNextSwitchTime( float time )
{
	_nextSwitchTime = level.time + time;
}

void Weapon::setNextSwitchTime( Event *ev )
{
	setNextSwitchTime( ev->GetFloat( 1 ) );
}
