

#include "g_local.h"
#include "mp_manager.hpp"
#include "entity.h"
#include "trigger.h"
#include "explosion.h"
#include "misc.h"
#include "navigate.h"
#include "specialfx.h"
#include "player.h"
#include "weaputils.h"
#include "g_spawn.h"
#include "g_phys.h"
#include "../script/scriptexception.h"
#include "../qcommon/gameplaymanager.h"

#define MULTI_STATE (1 << 0)  



















Event EV_UseAnim_Reset
(
	"_reset",
	EV_CODEONLY,
	NULL,
	NULL,
	"Reset's the Use Anim after it has no longer been touched."
);

Event EV_UseAnim_Thread
(
	"thread",
	EV_SCRIPTONLY,
	"s",
	"label",
	"Sets which thread to use when this UseAnim is triggered."
);
Event EV_UseAnim_Count
(
	"count",
	EV_DEFAULT,
	"i",
	"newCount",
	"Sets how many times the UseAnim can be triggered."
);
Event EV_UseAnim_TriggerTarget
(
	"triggertarget",
	EV_SCRIPTONLY,
	"s",
	"targetname",
	"Sets what should be triggered, when this UseAnim is triggered."
);
Event EV_UseAnim_SetAnim
(
	"anim",
	EV_DEFAULT,
	"s",
	"animName",
	"set the animation to use for player."
);
Event EV_UseAnim_SetKey
(
	"key",
	EV_SCRIPTONLY,
	"s",
	"keyName",
	"set the key needed to make this UseAnim function."
);
Event EV_UseAnim_SetState
(
	"state",
	EV_CHEAT,
	"s",
	"stateName",
	"set the state to use for the player."
);
Event EV_UseAnim_SetDelay
(
	"delay",
	EV_DEFAULT,
	"f",
	"delayTime",
	"how long it takes for the UseAnim to be retriggered once you leave it."
);
Event EV_UseAnim_SetActionType
(
	"action_type",
	EV_DEFAULT,
	"S",
	"actionType",
	"Action type (kick, ignite, etc)"
);

Event EV_ExplodingWall_StopRotating
(
	"stoprotating",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Stop rotating the wall."
);

Event EV_FallingRock_Bounce
(
	"bounce",
	EV_CODEONLY,
	NULL,
	NULL,
	"sent to entity when touched."
);

Event EV_PushObject_Start
(
	"start",
	EV_DEFAULT,
	NULL,
	NULL,
	"Sets up the pushobject."
);
Event EV_PushObject_SetDamage
(
	"dmg",
	EV_DEFAULT,
	"i",
	"damage",
	"Set the damage.");
Event EV_PushObject_SetPushSound
(
	"pushsound",
	EV_DEFAULT,
	"s",
	"sound",
	"Set the pushing sound"
);

Event EV_Teleporter_Teleport
(
	"teleport",
	EV_CODEONLY,
	"e",
	"entity",
	"Teleports the entity to destination."
);

Event EV_TossObject_SetBounceSound
(
	"bouncesound",
	EV_DEFAULT,
	"s",
	"sound",
	"When bouncing, what sound to play on impact"
);
Event EV_TossObject_SetBounceSoundChance
(
	"bouncesoundchance",
	EV_DEFAULT,
	"f[0,1]",
	"chance",
	"When bouncing, the chance that the bounce sound will be played"
);













Event EV_UseObject_MoveThread (	"move_thread",
	EV_SCRIPTONLY,
	"s",
	"label",
	"Sets which move thread to use when this UseObject has finshed looping."
);
Event EV_UseObject_Count
(
	"count",
	EV_DEFAULT,
	"i",
	"newCount",
	"Sets how many times the UseObject can be triggered.");
Event EV_UseObject_Cone
(
	"cone",
	EV_SCRIPTONLY,
	"f",
	"newCone",
	"Sets the cone in angles of where the Useobject can be used."
);
Event EV_UseObject_Offset
(
	"offset",
	EV_DEFAULT,
	"v",
	"newOffset",
	"Sets the offset to use for this UseObject."
);
Event EV_UseObject_State
(
	"state",
	EV_DEFAULT,
	"s",
	"newState",
	"Sets the state to use for this UseObject.");
Event EV_UseObject_TriggerTarget
(
	"triggertarget",
	EV_SCRIPTONLY,
	"s",
	"targetname",
	"Sets what should be triggered, when this UseObject is triggered.");
Event EV_UseObject_Activate
(
	"activate",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Allow the useobject to be used."
);
Event EV_UseObject_Deactivate
(
	"deactivate",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Do not allow the useobject to be used."
);
Event EV_UseObject_MoveThePlayer
(
	"movetheplayer",
	EV_DEFAULT,
	NULL,
	NULL,
	"Attempt to move the player to the UseObject location."
);






extern Event EV_FallingRock_Start;

Event EV_FallingRock_SetWait (	"wait",
	EV_SCRIPTONLY,
	"f",
	"wait",
	"How long to wait before rock starts falling."
);
Event EV_FallingRock_SetDmg (	"dmg",
	EV_SCRIPTONLY,
	"i",
	"dmg",
	"Set the damage from the rock.");
Event EV_FallingRock_SetSpeed (	"speed",
	EV_DEFAULT,
	"f",
	"speed",
	"Set the speed that the rock moves at."
);
Event EV_FallingRock_SetBounceSound (	"noise",
	EV_SCRIPTONLY,
	"s",
	"sound",
	"Set the sound to play when the rock bounces"
);




































bool UseObject::canBeUsed( const Vector &org, const Vector &dir )
{
	float    dot;
	Vector   forward;
	Vector   diff;
	Vector   ang;
	
	if ( !active )
	{
		return false;
	}
	
	if ( !count )
	{
		return false;
	}
	
	ang = vec_zero;
	ang[ YAW ] = angles[ YAW ] + yaw_offset;
	ang.AngleVectors( &forward );
	dot = forward * dir;
	if ( dot < cone )
	{
		return false;
	}
	
	
	return true;
}

void UseObject::Setup( const Entity *activator, Vector *org, Vector *ang, str *newstate )
{
	if ( ( spawnflags & MULTI_STATE ) && objectState )
	{
		*newstate = state_backwards;
	}
	else
	{
		*newstate = state;
	}

	MatrixTransformVector( offset, orientation, *org );
	*org += origin;

	*ang = angles;
	ang->y += yaw_offset;

	if ( count > 0 )
	{
		count--;
		if ( !count )
		{
			setSolidType( SOLID_NOT );
			setContents( 0 );
		}
	}
}

CLASS_DECLARATION(Entity, FuncRemove, "func_remove") {
    {NULL, NULL}
};

FuncRemove::FuncRemove()
{
    PostEvent(EV_Remove, EV_REMOVE);
}


CLASS_DECLARATION(Entity, MiscModel, "misc_model") {
    {NULL, NULL}
};

MiscModel::MiscModel()
{
    PostEvent(EV_Remove, EV_REMOVE);
}


CLASS_DECLARATION(Listener, InfoNull, "info_null") {
    {NULL, NULL}
};

InfoNull::InfoNull()
{
    PostEvent(EV_Remove, EV_REMOVE);
}


CLASS_DECLARATION(Entity, InfoNotNull, "info_notnull") {
    {NULL, NULL}
};

#define RANDOMANGLES (1 << 0)
#define LANDSHATTER  (1 << 1)
#define INVISIBLE    (1 << 5)
#define ACCUMULATIVE (1 << 6)
#define TWOSTAGE     (1 << 7)

Event EV_ExplodingWall_OnGround (    "checkonground", EV_CODEONLY,
    NULL,
    NULL,
    "Check if exploding wall is on ground.");
Event EV_ExplodingWall_AngleSpeed (    "anglespeed", EV_SCRIPTONLY,
    "f",
    "speed",
    "Set the angle speed.");
Event EV_ExplodingWall_LandRadius (    "land_radius", EV_SCRIPTONLY,
    "f",
    "radius",
    "Set the land radius.");
Event EV_ExplodingWall_LandAngles (    "land_angles", EV_SCRIPTONLY,
    "v",
    "angles",
    "Set the land angles.");
Event EV_ExplodingWall_BaseVelocity (    "base_velocity", EV_SCRIPTONLY,
    "v",
    "velocity",
    "Set the base velocity.");
Event EV_ExplodingWall_RandomVelocity (
    "random_velocity",
    EV_SCRIPTONLY,
    "v",
    "velocity",
    "Set the amount of random variation of the base velocity.");
extern Event EV_Door_SetDmg;

Event EV_ExplodingWall_SetExplosions (    "explosions", EV_SCRIPTONLY,
    "i",
    "explosions",
    "Set the number of explosions.");
Event EV_ExplodingWall_SetDmg (    "dmg",
    EV_SCRIPTONLY,
    "i",
    "dmg",
    "Set the damage from the exploding wall.");
Event EV_ExplodingWall_Setup (    "setup",
    EV_CODEONLY,
    NULL,
    NULL,
    "Initializes the exploding wall.");
extern Event EV_SlidingDoor_Setup;


CLASS_DECLARATION(Trigger, ExplodingWall, "func_explodingwall") {
    {&EV_Trigger_Effect,               &ExplodingWall::Explode       },
    {&EV_ExplodingWall_Setup,          &ExplodingWall::Setup         },
    {&EV_Damage,                       &ExplodingWall::DamageEvent   },
    {&EV_Touch,                        &ExplodingWall::TouchFunc     },
    {&EV_ExplodingWall_StopRotating,   &ExplodingWall::StopRotating  },
    {&EV_ExplodingWall_OnGround,       &ExplodingWall::CheckOnGround },
    {&EV_ExplodingWall_AngleSpeed,     &ExplodingWall::AngleSpeed    },
    {&EV_ExplodingWall_LandRadius,     &ExplodingWall::LandRadius    },
    {&EV_ExplodingWall_LandAngles,     &ExplodingWall::LandAngles    },
    {&EV_ExplodingWall_BaseVelocity,   &ExplodingWall::BaseVelocity  },
    {&EV_ExplodingWall_RandomVelocity, &ExplodingWall::RandomVelocity},
    {&EV_ExplodingWall_SetDmg,         &ExplodingWall::SetDmg        },
    {&EV_ExplodingWall_SetExplosions,  &ExplodingWall::SetExplosions },
    {&EV_SetGameplayDamage,            &ExplodingWall::setDamage     },
    {NULL,                             NULL                          }
};

void ExplodingWall::setDamage(Event *ev)
{
    if (ev->NumArgs() < 1)
        return;

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm->hasFormula("OffensiveDamage"))
        return;

    str   damagestr = ev->GetString(1);
    float damagemod = 1.0f;
    if (gpm->getDefine(damagestr) != "")
        damagemod = (float)atof(gpm->getDefine(damagestr));
    GameplayFormulaData fd(this, 0, 0, "");
    float finaldamage = gpm->calculate("OffensiveDamage", fd, damagemod);
    Event *newev      = new Event(EV_ExplodingWall_SetDmg);
    newev->AddInteger((int)finaldamage);
    ProcessEvent(newev);
}

void ExplodingWall::AngleSpeed(Event *ev)
{
    angle_speed = ev->GetFloat(1);
}

void ExplodingWall::LandRadius(Event *ev)
{
    land_radius = ev->GetFloat(1);
}

void ExplodingWall::LandAngles(Event *ev)
{
    land_angles = ev->GetVector(1);
}

void ExplodingWall::BaseVelocity(Event *ev)
{
    base_velocity = ev->GetVector(1);
}

void ExplodingWall::RandomVelocity(Event *ev)
{
    random_velocity = ev->GetVector(1);
}

void ExplodingWall::SetDmg(Event *ev)
{
    dmg = ev->GetInteger(1);
}

void ExplodingWall::SetExplosions(Event *ev)
{
    explosions = ev->GetInteger(1);
}

void ExplodingWall::Explode(Event *ev)
{
    Entity *other;
    Vector  pos;
    Vector  landMins, landMaxs;
    int     i;

    if (spawnflags & INVISIBLE) {
        showModel();
        setSolidType(SOLID_BSP);
        takedamage = DAMAGE_YES;
    }

    if (takedamage == DAMAGE_NO) {
        return;
    }

    other = ev->GetEntity(1);

    health     = 0;
    takedamage = DAMAGE_NO;

    for (i = 0; i < explosions; i++) {
        pos[0] = absmin[0] + G_Random(size[0]);
        pos[1] = absmin[1] + G_Random(size[1]);
        pos[2] = absmin[2] + G_Random(size[2]);

        CreateExplosion(pos, dmg, this, other, this);
    }

    state     = 1;
    on_ground = false;
    PostEvent(EV_ExplodingWall_OnGround, FRAMETIME);

    velocity.x = base_velocity.x + G_CRandom(random_velocity.x);
    velocity.y = base_velocity.y + G_CRandom(random_velocity.y);
    velocity.z = base_velocity.z + G_Random(random_velocity.z);

    setMoveType(MOVETYPE_BOUNCE);
    setSolidType(SOLID_BBOX);
    if (spawnflags & RANDOMANGLES) {
        avelocity[0] = G_Random(angle_speed);
        avelocity[1] = G_Random(angle_speed);
        avelocity[2] = G_Random(angle_speed);
    } else {
        Vector delta;
        float  most;
        float  time;
        int    t;

        delta = land_angles - angles;
        if (delta[0] > 180) {
            delta[0] -= 360;
        }
        if (delta[0] < -180) {
            delta[0] += 360;
        }
        if (delta[1] > 180) {
            delta[1] -= 360;
        }
        if (delta[1] < -180) {
            delta[1] += 360;
        }
        if (delta[2] > 180) {
            delta[2] -= 360;
        }
        if (delta[2] < -180) {
            delta[2] += 360;
        }
        most = MaxValue(delta);
        if (!angle_speed) {
            angle_speed = 1;
        }
        t         = 10 * most / angle_speed;
        time      = (float)t / 10;
        delta     = delta * (1.0 / time);
        avelocity = delta;
        PostEvent(EV_ExplodingWall_StopRotating, time);
        state = 2;
    }

    ActivateTargets(ev);

    if (land_radius > 0) {
        landMins[0] = landMins[1] = landMins[2] = -land_radius;
        landMaxs[0] = landMaxs[1] = landMaxs[2] = land_radius;
        setSize(landMins, landMaxs);
    }

    attack_finished = 0;
}

void ExplodingWall::DamageEvent(Event *ev)
{
    Event         *event;
    Entity        *attacker;
    float          damage;

    if (takedamage == DAMAGE_NO) {
        return;
    }

    if (on_ground) {
        GroundDamage(ev);
        return;
    }

    damage     = ev->GetFloat(1);
    attacker   = ev->GetEntity(3);

    if (spawnflags & ACCUMULATIVE) {
        health -= damage;
        if (health > 0.0f) {
            return;
        }
    } else {
        if (damage < health) {
            return;
        }
    }

    event = new Event(EV_Activate);
    event->AddEntity(attacker);
    ProcessEvent(event);
}

void ExplodingWall::GroundDamage(Event *ev)
{
    Entity        *attacker;
    Vector         pos;
    int            damage;

    if (takedamage == DAMAGE_NO) {
        return;
    }

    damage     = ev->GetInteger(1);
    attacker   = ev->GetEntity(3);

    if (spawnflags & ACCUMULATIVE) {
        health -= damage;
        if (health > 0.0f) {
            return;
        }
    } else {
        if (damage < health) {
            return;
        }
    }

    if (explosions) {
        pos[0] = absmin[0] + G_Random(size[0]);
        pos[1] = absmin[1] + G_Random(size[1]);
        pos[2] = absmin[2] + G_Random(size[2]);

        CreateExplosion(pos, damage, this, attacker, this);
    }
    takedamage = DAMAGE_NO;
    hideModel();
    BroadcastSound();
    PostEvent(EV_Remove, 0.0f);
}

void ExplodingWall::SetupSecondStage(void)
{
    health     = max_health;
    takedamage = DAMAGE_YES;
}

void ExplodingWall::StopRotating(Event *ev)
{
    avelocity = vec_zero;
    setAngles(land_angles);
    if (spawnflags & TWOSTAGE) {
        SetupSecondStage();
    }
}

void ExplodingWall::CheckOnGround(Event *ev)
{
    if ((velocity == vec_zero) && groundentity) {
        Vector delta;
        float  most;
        float  time;
        int    t;

        delta = land_angles - angles;
        if (delta.length() > 1) {
            if (delta[0] > 180) {
                delta[0] -= 360;
            }
            if (delta[0] < -180) {
                delta[0] += 360;
            }
            if (delta[1] > 180) {
                delta[1] -= 360;
            }
            if (delta[1] < -180) {
                delta[1] += 360;
            }
            if (delta[2] > 180) {
                delta[2] -= 360;
            }
            if (delta[2] < -180) {
                delta[2] += 360;
            }
            most = MaxValue(delta);
            if (angle_speed > 3) {
                t = 10.0f * most / (angle_speed / 3);
            } else {
                t = 10.0f * most;
            }
            time      = (float)t / 10;
            delta     = delta * (1.0 / time);
            avelocity = delta;
            PostEvent(EV_ExplodingWall_StopRotating, time);
        }
        state = 2;
        setSize(orig_mins, orig_maxs);
        on_ground = true;
    } else {
        PostEvent(ev, FRAMETIME);
    }
}

void ExplodingWall::TouchFunc(Event *ev)
{
    Entity *other;

    if ((velocity == vec_zero) || (level.time < attack_finished)) {
        return;
    }

    other = ev->GetEntity(1);

    if ((spawnflags & LANDSHATTER) && (other == world)) {
        Vector pos;

        takedamage = DAMAGE_NO;

        if (explosions) {
            pos[0] = absmin[0] + G_Random(size[0]);
            pos[1] = absmin[1] + G_Random(size[1]);
            pos[2] = absmin[2] + G_Random(size[2]);

            CreateExplosion(pos, dmg, this, other, this);
        }
        hideModel();
        BroadcastSound();
        PostEvent(EV_Remove, 0);
        return;
    }

    if (other->takedamage) {
        other->Damage(this, activator, dmg, origin, vec_zero, vec_zero, 20, 0, MOD_EXPLODEWALL);
        Sound("debris_generic", CHAN_WEAPON);
        attack_finished = level.time + FRAMETIME;
    }
}

void ExplodingWall::Setup(Event *ev)
{
    if (spawnflags & INVISIBLE) {
        if (Targeted()) {
            takedamage = DAMAGE_YES;
        } else {
            takedamage = DAMAGE_NO;
        }
        hideModel();
        setSolidType(SOLID_NOT);
    } else {
        showModel();
        setSolidType(SOLID_BSP);
        takedamage = DAMAGE_YES;
    }

    setMoveType(MOVETYPE_PUSH);
    setOrigin();
}

ExplodingWall::ExplodingWall()
{
    if (LoadingSavegame) {
        return;
    }

    health     = 60;
    max_health = health;
    on_ground  = false;

    state       = 0;
    angle_speed = (spawnflags & RANDOMANGLES) ? 600 : 100;
    land_radius = 16;
    dmg         = 10;
    explosions  = 1;

    base_velocity   = Vector(0, 0, 280);
    random_velocity = Vector(140, 140, 140);

    orig_mins = mins;
    orig_maxs = maxs;

    respondto = spawnflags ^ TRIGGER_PLAYERS;

    attack_finished = false;

    PostEvent(EV_ExplodingWall_Setup, EV_POSTSPAWN);
}


#define NO_EFFECTS (1 << 5)

Event EV_Teleporter_StopTeleport (    "stopteleport",
    EV_CODEONLY,
    "e",
    "entity",
    "Releases the entity at the end of the teleport.");
Event EV_Teleporter_SetThread (    "teleportthread",
    EV_SCRIPTONLY,
    "s",
    "thread_name",
    "Sets the thread to run when the player is teleported.");

CLASS_DECLARATION(Trigger, Teleporter, "trigger_teleport") {
    {&EV_Trigger_Effect,          &Teleporter::StartTeleport},
    {&EV_Teleporter_Teleport,     &Teleporter::Teleport     },
    {&EV_Teleporter_StopTeleport, &Teleporter::StopTeleport },
    {&EV_Teleporter_SetThread,    &Teleporter::SetThread    },
    {NULL,                        NULL                      }
};

void Teleporter::SetThread(Event *ev)
{
    teleport_thread = ev->GetString(1);
}

void Teleporter::StartTeleport(Event *ev)
{
    Entity  *fx;
    Entity  *other;
    Event   *event;
    qboolean is_sentient;
    Vector   new_position;

    if (in_use) {
        return;
    }

    in_use = true;

    other = ev->GetEntity(1);

    if (!other) {
        return;
    }

    if (spawnflags & NO_EFFECTS) {
        event = new Event(EV_Teleporter_Teleport);
        event->AddEntity(other);
        ProcessEvent(event);
        return;
    }

    if (other->isSubclassOf(Sentient)) {
        is_sentient = true;
    } else {
        is_sentient = false;
    }

    if (is_sentient) {
        new_position = origin;
        new_position.z += mins.z;
        other->setOrigin(new_position);
        other->NoLerpThisFrame();
    }


    fx = new Entity(ENTITY_CREATE_FLAG_ANIMATE);
    fx->setOrigin(other->origin);

    fx->animate->RandomAnimate("idle", EV_Remove);

    if (is_sentient) {
        fx->setModel("fx/fx-warpflash-federation.tik"); 
    } else {
        fx->setModel("fx/fx-warpflash-federation.tik"); 
    }

    if (is_sentient) {

        other->flags |= FL_IMMOBILE;
        other->takedamage = DAMAGE_NO;
    }


    event = new Event(EV_Teleporter_Teleport);
    event->AddEntity(other);

    if (is_sentient) {
        PostEvent(event, 4);
    } else {
        PostEvent(event, 0);
    }

    if (is_sentient) {
        other->PostEvent(EV_CODEONLY, 2);

    }
}

void Teleporter::Teleport(Event *ev)
{
    Entity  *dest;
    int     i;
    Entity *other;
    Vector  mid;
    Entity *fx;
    Event  *event;

    other = ev->GetEntity(1);

    if (!other || (other == world)) {
        return;
    }

    dest = (Entity *)G_FindTarget(NULL, Target());
    if (!dest) {
        warning("Teleport", "Couldn't find destination\n");
        return;
    }

    assert(dest);

    other->unlink();

    if (other->isSubclassOf(Sentient)) {
        other->origin   = dest->origin + Vector(0, 0, 1);
        other->velocity = vec_zero;
    } else {
        mid           = (absmax - absmin) * 0.5;
        other->origin = dest->origin + Vector(0, 0, 1);
        other->origin += mid;
    }

    other->setAngles(dest->angles);

    if (other->client) {
        client = other->client;

        client->ps.pm_time = 100;
        client->ps.pm_flags |= PMF_RESPAWNED;

        ((Player *)other)->CameraCut();

        for (i = 0; i < 3; i++) {
            client->ps.delta_angles[i] = ANGLE2SHORT(dest->angles[i] - client->cmd_angles[i]);
        }

        VectorCopy(angles, client->ps.viewangles);
    }

    if (dest->isSubclassOf(TeleporterDestination)) {
        float len;

        len = other->velocity.length();
        if (len < 400) {
            len = 400;
        }
        other->velocity = ((TeleporterDestination *)dest)->movedir * len;
    }

    KillBox(other);

    other->setOrigin(other->origin);
    other->origin.copyTo(other->edict->s.origin2);


    if (teleport_thread.length() && other->isSubclassOf(Player)) {
        if (!ExecuteThread(teleport_thread.c_str()))
            warning("RunThread", "could not process thread");
    }


    if (spawnflags & NO_EFFECTS) {
        Event *stopEvent = new Event(EV_Teleporter_StopTeleport);
        stopEvent->AddEntity(other);
        ProcessEvent(stopEvent);
        return;
    }


    fx = new Entity(ENTITY_CREATE_FLAG_ANIMATE);

    fx->setOrigin(other->origin);

    fx->animate->RandomAnimate("idle", EV_Remove);

    if (other->isSubclassOf(Sentient)) {
        fx->setModel("fx/fx-warpflash-federation.tik"); 
    } else {
        fx->setModel("fx/fx-warpflash-federation.tik"); 
    }

    event = new Event(EV_Teleporter_StopTeleport);
    event->AddEntity(other);

    if (other->isSubclassOf(Sentient)) {
        PostEvent(event, 1.75);
    } else {
        PostEvent(event, FRAMETIME);
    }
}

void Teleporter::StopTeleport(Event *ev)
{
    Entity *other;

    other = ev->GetEntity(1);

    if (other->isSubclassOf(Sentient)) {
        other->flags &= ~FL_IMMOBILE;
        other->takedamage = DAMAGE_AIM;
        other->showModel();
    }


    in_use = false;
}

Teleporter::Teleporter()
{
    if (LoadingSavegame) {
        return;
    }

    if (spawnflags & 1) {
        PostEvent(EV_Show, EV_POSTSPAWN);
    }

    respondto = spawnflags ^ (TRIGGER_PLAYERS);

    in_use = false;


    if (!(spawnflags & NO_EFFECTS)) {
    }
}


CLASS_DECLARATION(Entity, TeleporterDestination, "func_teleportdest") {
    {&EV_SetAngle, &TeleporterDestination::SetMoveDir},
    {NULL,         NULL                              }
};

TeleporterDestination::TeleporterDestination()
{
    movedir = G_GetMovedir(0);
}

void TeleporterDestination::SetMoveDir(Event *ev)
{
    float angle;

    angle   = ev->GetFloat(1);
    movedir = G_GetMovedir(angle);
    setAngles(movedir.toAngles());
}



Event EV_UseObject_SetActionType (    "action_type",
    EV_DEFAULT,
    "S",
    "actionType",
    "The type of action (kick, ignite, etc)");
extern Event EV_Path_SetAnim;

Event EV_UseAnim_SetCamera (
    "camera",
    EV_SCRIPTONLY,
    "s",
    "cameraPosition",
    "set the camera to use when in this animation.\n"
    "topdown, behind, front, side, behind_fixed, side_left, side_right");
Event EV_UseAnim_SetNumLoops (    "num_loops",
    EV_DEFAULT,
    "i",
    "loopCount",
    "set the number of times to loop an animation per use.");
    extern Event EV_Trigger_SetDelay;

CLASS_DECLARATION(Entity, UseAnim, "func_useanim") {
    {&EV_Use,                           NULL                              },
    {&EV_Touch,                         &UseAnim::Touched                 },
    {&EV_UseAnim_Reset,                 &UseAnim::Reset                   },
    {&EV_UseAnim_Thread,                &UseAnim::SetThread               },
    {&EV_UseAnim_TriggerTarget,         &UseAnim::SetTriggerTarget        },
    {&EV_UseAnim_Count,                 &UseAnim::SetCount                },
    {&EV_UseAnim_SetAnim,               &UseAnim::SetAnim                 },
    {&EV_UseAnim_SetState,              &UseAnim::SetState                },
    {&EV_UseAnim_SetKey,                &UseAnim::SetKey                  },
    {&EV_UseAnim_SetNumLoops,           &UseAnim::SetNumLoops             },
    {&EV_UseAnim_SetCamera,             &UseAnim::SetCamera               },
    {&EV_UseAnim_SetActionType,         &UseAnim::SetActionType           },
    {NULL,                              NULL                              }
};

UseAnim::UseAnim()
{
    if (LoadingSavegame) {
        return;
    }

    setMoveType(MOVETYPE_NONE);

    anim      = "stand_use";
    num_loops = 1;
    hideModel();

    if (!(spawnflags & 2)) {
        setSolidType(SOLID_BBOX);
        setContents(CONTENTS_BODY);
    } else {
        setSolidType(SOLID_TRIGGER);
        edict->svflags |= SVF_NOCLIENT;
    }

    count = -1;
    triggertarget = "";
    thread_name = "";
    delay = 3;
    last_active_time = -delay;
    active = 0;
    camera = "behind";
    if (spawnflags & 1) {
        PostEvent(EV_Show, EV_POSTSPAWN);
    }
}

void UseAnim::Touched(Event *ev)
{
    Entity *other;

    if (active && (!(spawnflags & 4))) {
        CancelEventsOfType(EV_UseAnim_Reset);
        PostEvent(EV_UseAnim_Reset, 0.25f);
        return;
    }

    if (level.time < last_active_time) {
        return;
    }

    other = ev->GetEntity(1);
    if (other->isSubclassOf(Player)) {
        ((Player *)other)->TouchedUseAnim(this);
    }
}

bool UseAnim::canBeUsed(Entity *activator)
{
    Entity *dest;

    if (!count) {
        return false;
    }

    if (level.time < last_active_time) {
        return false;
    }

    if (key.length()) {
        if (!activator->isSubclassOf(Sentient)) {
            return false;
        }
        if (!(((Sentient *)activator)->HasItem(key.c_str()))) {
            qboolean  setModel;
            Item     *item;
            ClassDef *cls;
            str       dialog;

            cls = FindClass(key.c_str(), &setModel);
            if (!cls || !checkInheritance("Item", cls->classname)) {
                gi.WDPrintf("No item named '%s'\n", key.c_str());
                return true;
            }
            item = (Item *)cls->newInstance();
            if (setModel) {
                item->setModel(key.c_str());
            }
            item->CancelEventsOfType(EV_Item_DropToFloor);
            item->CancelEventsOfType(EV_Remove);
            item->ProcessPendingEvents();
            dialog = item->GetDialogNeeded();
            if (dialog.length() > 0) {
                activator->Sound(dialog);
            } else {
                gi.centerprintf(activator->edict, CENTERPRINT_IMPORTANCE_NORMAL, "You need the %s", item->getName().c_str());
            }
            delete item;

            last_active_time = level.time + 5;
            return false;
        } else {
            return qtrue;
        }
    }

    if (isSubclassOf(TouchAnim)) {
        return true;
    }

    dest = (Entity *)G_FindTarget(NULL, Target());
    if (!dest || !dest->isSubclassOf(UseAnimDestination)) {
        warning("UseAnim", "Couldn't find destination\n");
        return false;
    }

    return true;
}

bool UseAnim::GetInformation(
    Entity *activator,
    Vector *org,
    Vector *destAngles,
    str    *animation,
    int    *loopcount,
    str    *destState,
    str    *destCamera
)
{
    Entity             *dest;
    UseAnimDestination *uadest;

    if (!count) {
        return false;
    }

    dest = (Entity *)G_FindTarget(NULL, Target());

    if (!dest || !dest->isSubclassOf(UseAnimDestination)) {
        *org = origin;
        *destAngles = this->angles;
        *animation = anim;
        *loopcount = num_loops;
        *destState = this->state;
        *destCamera = this->camera;
    } else {
        uadest = (UseAnimDestination *)dest;

        *org = uadest->origin;
        *destAngles = uadest->angles;
        *animation = uadest->GetAnim();
        *loopcount = uadest->GetNumLoops();
        *destState = uadest->GetState();
        *destCamera = this->camera;
    }

    active = qtrue;

    if (isSubclassOf(TouchAnim)) {
        dest = NULL;
        do {
            dest = (Entity *)G_FindTarget(dest, Target());
            if (dest) {
                if (dest->isSubclassOf(UseAnim)) {
                    ((UseAnim *)dest)->active = qtrue;
                }
            } else {
                break;
            }
        } while (1);
    }
    if (count > 0) {
        count--;
    }

    return true;
}

void UseAnim::TriggerTargets(Entity *activator)
{
    if (triggertarget.length()) {
        Event  *event;
        Entity *ent;

        ent = NULL;
        do {
            ent = (Entity *)G_FindTarget(ent, triggertarget.c_str());
            if (!ent) {
                break;
            }
            event = new Event(EV_Activate);
            event->AddEntity(activator);
            ent->PostEvent(event, 0);
        } while (1);
    }

    if (thread_name.length()) {
        if (!ExecuteThread(thread_name))
            warning("TriggerTargets", "Null game script");
    }
}

void UseAnim::Reset(Event *ev)
{
    if (triggertarget.length()) {
        Entity *ent;

        ent = NULL;
        do {
            ent = (Entity *)G_FindTarget(ent, triggertarget.c_str());
            if (!ent) {
                break;
            }
            if (ent->isSubclassOf(Door)) {
                if (!((Door *)ent)->isCompletelyClosed()) {
                    CancelEventsOfType(EV_UseAnim_Reset);
                    PostEvent(EV_UseAnim_Reset, 0.25f);
                    return;
                }
            }
        } while (1);
    }

    active           = qfalse;
    last_active_time = level.time + delay;

    if (isSubclassOf(TouchAnim)) {
        Entity *dest;

        dest = NULL;
        do {
            dest = (Entity *)G_FindTarget(dest, Target());
            if (dest) {
                if (dest->isSubclassOf(UseAnim)) {
                    ((UseAnim *)dest)->active           = qfalse;
                    ((UseAnim *)dest)->last_active_time = level.time + delay;
                }
            } else {
                break;
            }
        } while (1);
    }
}

void UseAnim::SetThread(Event *ev)
{
    thread_name = ev->GetString(1);
}

void UseAnim::SetActionType(Event *ev)
{
    action_type = ev->GetString(1);
}

void UseAnim::SetDelay(Event *ev)
{
    delay = ev->GetFloat(1);
}

void UseAnim::SetTriggerTarget(Event *ev)
{
    triggertarget = ev->GetString(1);
}

void UseAnim::SetCount(Event *ev)
{
    count = ev->GetInteger(1);
}

void UseAnim::SetAnim(Event *ev)
{
    anim = ev->GetString(1);
}

void UseAnim::SetState(Event *ev)
{
    state = ev->GetString(1);
}

void UseAnim::SetKey(Event *ev)
{
    key = ev->GetString(1);
}

void UseAnim::SetCamera(Event *ev)
{
    camera = ev->GetString(1);
}

void UseAnim::SetNumLoops(Event *ev)
{
    num_loops = ev->GetInteger(1);
}

CLASS_DECLARATION(UseAnim, TouchAnim, "func_touchanim") {
    {NULL, NULL}
};

TouchAnim::TouchAnim()
{
    if (LoadingSavegame) {
        return;
    }

    spawnflags |= 2;

    if (spawnflags & 8) {
        setSize(Vector("-32 -32 0"), Vector("32 32 96"));
    } else {
        setSize(Vector("-16 -16 0"), Vector("16 16 96"));
    }

    setSolidType(SOLID_TRIGGER);
    edict->svflags |= SVF_NOCLIENT;
}


CLASS_DECLARATION(Entity, UseAnimDestination, "func_useanimdest") {
    {&EV_UseAnim_SetAnim,               &UseAnimDestination::SetAnim       },
    {&EV_UseAnim_SetState,              &UseAnimDestination::SetState      },
    {&EV_UseAnim_SetNumLoops,           &UseAnimDestination::SetNumLoops   },
    {NULL,                              NULL                               }
};

UseAnimDestination::UseAnimDestination()
{
    if (LoadingSavegame) {
        return;
    }
    anim      = "stand_use";
    num_loops = 1;
    setSolidType(SOLID_NOT);
    hideModel();
}

void UseAnimDestination::SetAnim(Event *ev)
{
    anim = ev->GetString(1);
}

void UseAnimDestination::SetState(Event *ev)
{
    state = ev->GetString(1);
}

str UseAnimDestination::GetAnim(void)
{
    return anim;
}

str UseAnimDestination::GetState(void)
{
    return state;
}

void UseAnimDestination::SetNumLoops(Event *ev)
{
    num_loops = ev->GetInteger(1);
}

int UseAnimDestination::GetNumLoops(void)
{
    return num_loops;
}

Event EV_UseObject_StopThread (    "stop_thread",
    EV_SCRIPTONLY,
    "s",
    "label",
    "Sets which stop thread to use when this UseObject is finished.");
Event EV_UseObject_ResetThread (    "reset_thread",
    EV_SCRIPTONLY,
    "s",
    "label",
    "Sets which thread to call when resetting.");

Event EV_UseObject_YawOffset (    "yaw_offset",
    EV_DEFAULT,
    "f",
    "newYawOffset",
    "Sets the yaw offset to use for this UseObject.");
Event EV_UseObject_StateBackwards (    "state_backwards",
    EV_DEFAULT,
    "s",
    "newState",
    "Sets the backward state to use for this UseObject.");
Event EV_UseObject_ResetTime (    "reset_time",
    EV_DEFAULT,
    "f",
    "newResetTime",
    "Sets the time it takes for the UseObject to reset itself.");
extern Event EV_UseObject_DamageType;

Event EV_UseObject_Reset (    "_useobject_reset",
    EV_CODEONLY,
    NULL,
    NULL,
    "Resets the useobject to the start state after a certain amount of time.");
Event EV_UseObject_Resetting (    "_useobject_resetting",
    EV_CODEONLY,
    NULL,
    NULL,
    "Intermediate function for useobject reset.");
Event EV_UseObject_DamageTriggered (    "_useobject_damagetriggered",
    EV_CODEONLY,
    "e",
    "activator",
    "Intermediate function for when the useobject was triggered by damage.");
extern Event EV_FuncBeam_Activate;

extern Event EV_FuncBeam_Deactivate;

Event EV_UseObject_UseMaterial (    "usematerial",
    EV_DEFAULT,
    "s",
    "nameOfUseMaterial",
    "the name of the material that glows when active.");
Event EV_UseObject_SetActiveState (    "_setactivestate",
    EV_CODEONLY,
    NULL,
    NULL,
    "event that sets up the proper skin for the useobject.");


CLASS_DECLARATION(Entity, TossObject, "TossObject") {
    {&EV_Touch,                           &TossObject::Touch               },
    {&EV_Stop,                            &TossObject::Stop                },
    {&EV_TossObject_SetBounceSound,       &TossObject::SetBounceSound      },
    {&EV_TossObject_SetBounceSoundChance, &TossObject::SetBounceSoundChance},
    {NULL,                                NULL                             }
};

TossObject::TossObject()
{
    animate = new Animate(this);
    if (LoadingSavegame) {
        return;
    }
    setMoveType(MOVETYPE_GIB);
    setSolidType(SOLID_NOT);
    bouncesound       = "";
    bouncesoundchance = 1.0f;
}

TossObject::TossObject(const str& modelName)  
{
    animate = new Animate(this);
    setMoveType(MOVETYPE_GIB);
    setSolidType(SOLID_NOT);
    bouncesound       = "";
    bouncesoundchance = 1.0f;
    setModel(modelName);
}

void TossObject::Archive(Archiver& arc)
{
    Entity::Archive(arc);
    arc.ArchiveString(&bouncesound);
    arc.ArchiveFloat(&bouncesoundchance);
}

void TossObject::SetBounceSound(const str &bounce)
{
    bouncesound = bounce;
}

void TossObject::SetBounceSound(Event *ev)
{
    bouncesound = ev->GetString(1);
}

void TossObject::SetBounceSoundChance(float chance)
{
    bouncesoundchance = chance;
}

void TossObject::SetBounceSoundChance(Event *ev)
{
    bouncesoundchance = ev->GetFloat(1);
}

void TossObject::Stop(Event *ev)
{
    setMoveType(MOVETYPE_NONE);
    setSolidType(SOLID_NOT);
    CancelEventsOfType(EV_FadeOut);
    PostEvent(EV_FadeOut, 7 + G_Random(5));
    setAngles(Vector("0 0 0"));
    animate->RandomAnimate("landed");
}

void TossObject::Touch(Event *ev)
{
    Entity *ent;

    ent = ev->GetEntity(1);

    if (!ent || (ent != world)) {
        return;
    }
    setAngles(angles * 0.5f);

    if (bouncesound.length()) {
        if (G_Random(1) < bouncesoundchance) {
            Sound(bouncesound);
        }
    }
}

void TossObject::SetVelocity(float severity)
{
    setSolidType(SOLID_BBOX);
    velocity[0] = 100.0 * crandom();
    velocity[1] = 100.0 * crandom();
    velocity[2] = 200.0 + 100.0 * random();

    avelocity = Vector(G_Random(600), G_Random(600), G_Random(600));

    velocity *= severity;

    if (velocity[0] < -400) {
        velocity[0] = -400;
    } else if (velocity[0] > 400) {
        velocity[0] = 400;
    }
    if (velocity[1] < -400) {
        velocity[1] = -400;
    } else if (velocity[1] > 400) {
        velocity[1] = 400;
    }
    if (velocity[2] < 200) {
        velocity[2] = 200; 
    } else if (velocity[2] > 600) {
        velocity[2] = 600;
    }

    animate->RandomAnimate("idle");

    PostEvent(EV_FadeOut, 8);
}





CLASS_DECLARATION(Entity, PushObject, "func_pushobject") {
    {&EV_PushObject_Start,        &PushObject::Start       },
    {&EV_Blocked,                 &PushObject::BlockFunc   },
    {&EV_PushObject_SetDamage,    &PushObject::SetDamage   },
    {&EV_SetGameplayDamage,       &PushObject::setDamage   },
    {&EV_PushObject_SetPushSound, &PushObject::SetPushSound},
    {NULL,                        NULL                     }
};

PushObject::PushObject()
{
    if (LoadingSavegame) {
        return;
    }
    dmg             = 2;
    attack_finished = 0;

    pushsound = "object_slide";

    PostEvent(EV_PushObject_Start, EV_POSTSPAWN);
}

void PushObject::SetPushSound(Event *ev)
{
    pushsound = ev->GetString(1);
}

void PushObject::Start(Event *ev)
{
    flags |= FL_TOUCH_TRIGGERS;
    edict->clipmask = MASK_SOLID;
    setSolidType(SOLID_BSP);
    setMoveType(MOVETYPE_PUSH);

    setSize(mins + Vector(1, 1, 2), maxs - Vector(1, 1, 1));
}

qboolean PushObject::canPush(Vector dir)
{
    trace_t trace;

    Vector end(origin.x + dir.x, origin.y + dir.y, origin.z);

    trace = G_Trace(origin, mins, maxs, end, this, MASK_DEADSOLID, false, "PushObject::Push");
    return (!trace.startsolid && (trace.fraction == 1.0f));
}

qboolean PushObject::Push(Entity *pusher, Vector move)
{
    trace_t trace;

    if (pushsound.length()) {
        if (!edict->s.loopSound) {
            LoopSound(pushsound);
            PostEvent(EV_StopLoopSound, level.frametime * 5);
        }
    }

    Vector end(origin.x + move.x, origin.y + move.y, origin.z);

    trace = G_Trace(origin, mins, maxs, end, this, MASK_DEADSOLID, false, "PushObject::Push");
    if (!trace.startsolid && (trace.fraction > 0)) {
        owner = pusher;

        G_PushMove(this, trace.endpos - origin, vec_zero);

        if (edict->s.loopSound) {
            PostEvent(EV_StopLoopSound, 0.f);
        }

        return qtrue;
    }

    return qfalse;
}

Entity *PushObject::getOwner(void)
{
    return (Entity *)owner;
}

void PushObject::BlockFunc(Event *ev)
{
    Entity *other;

    if ((dmg != 0) && (level.time >= attack_finished)) {
        attack_finished = level.time + 0.5f;
        other           = ev->GetEntity(1);
        if (other != owner) {
            other->Damage(this, this, dmg, origin, vec_zero, vec_zero, 0, 0, MOD_CRUSH);
        }
    }
}

void PushObject::SetDamage(Event *ev)
{
    dmg = ev->GetInteger(1);
}

void PushObject::setDamage(Event *ev)
{
    if (ev->NumArgs() < 1)
        return;

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm->hasFormula("OffensiveDamage"))
        return;

    str   damagestr = ev->GetString(1);
    float damagemod = 1.0f;
    if (gpm->getDefine(damagestr) != "")
        damagemod = (float)atof(gpm->getDefine(damagestr));
    GameplayFormulaData fd(this, 0, 0, "");
    float finaldamage = gpm->calculate("OffensiveDamage", fd, damagemod);
    Event *newev      = new Event(EV_PushObject_SetDamage);
    newev->AddInteger((int)finaldamage);
    ProcessEvent(newev);
}

#define SPAWN_AUTO_RESET (1 << 0)
#define NO_RANDOMNESS    (1 << 1)
#define REMOVE_ON_GROUND (1 << 2)

Event EV_FallingRock_Rotate (    "rotate",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "rotates the falling rock.");
extern Event EV_Trigger_SetWait;

Event EV_FallingRock_Start (    "start",
    EV_DEFAULT,
    NULL,
    NULL,
    "Starts rock falling.");

CLASS_DECLARATION(Entity, FallingRock, "func_fallingrock") {
    {&EV_Activate,                   &FallingRock::Activate      },
    {&EV_Touch,                      &FallingRock::Touch         },
    {&EV_FallingRock_Bounce,         &FallingRock::Bounce        },
    {&EV_FallingRock_Rotate,         &FallingRock::Rotate        },
    {&EV_FallingRock_Start,          &FallingRock::StartFalling  },
    {&EV_FallingRock_SetWait,        &FallingRock::SetWait       },
    {&EV_FallingRock_SetSpeed,       &FallingRock::SetSpeed      },
    {&EV_FallingRock_SetDmg,         &FallingRock::SetDmg        },
    {&EV_FallingRock_SetBounceSound, &FallingRock::SetBounceSound},
    {&EV_SetGameplayDamage,          &FallingRock::setDamage      },
    {NULL,                           NULL                        }
};

FallingRock::FallingRock()
{
    if (LoadingSavegame) {
        return;
    }
    active  = 0;
    current = NULL;
    setMoveType(MOVETYPE_NONE);
    wait            = 0;
    dmg             = 20;
    speed           = 200;
    activator       = NULL;
    attack_finished = 0;

    SetBounceSound("impact_rock");
}

Entity *FallingRock::SetNextBounceDir(void)
{
    Entity *ent;

    if (!current->target.length()) {
        return NULL;
    }

    ent = (Entity *)G_FindTarget(NULL, current->target.c_str());
    if (!ent) {
        gi.Error(ERR_DROP, "FallingRock :: Entity with targetname of '%s' not found", current->target.c_str());
    }

    bounce_dir = ent->origin - current->origin;
    bounce_dir.normalize();

    return ent;
}

void FallingRock::NextBounce(void)
{
    float  time;
    float  distance;
    Vector delta, xydelta;
    float  xy_speed;
    float  vertical_speed;

    delta     = current->origin - origin;
    xydelta   = delta;
    xydelta.z = 0;
    xy_speed  = speed;

    distance = xydelta.normalize();

    time = distance / xy_speed;

    if (!(spawnflags & NO_RANDOMNESS)) {
        if (time > 1.0f) {
            time = 0.75f + G_Random(1);
        }

        if (time < 0.4f) {
            time = 0.4f;
        }
    }

    vertical_speed = (delta.z / time) + (0.5f * gravity * sv_currentGravity->value * time);
    if (vertical_speed < 0) {
        vertical_speed = 0;
    }

    velocity   = xydelta * xy_speed;
    velocity.z = vertical_speed;

    Vector ang(0, vectoyaw(delta), 0);
    ang.AngleVectors(NULL, &rotateaxis);

    groundentity = NULL;
}

void FallingRock::Rotate(Event *ev)
{
    float mat[3][3];
    float ang;

    ang = 360.0f * FRAMETIME;
    RotatePointAroundVector(mat[0], rotateaxis, orientation[0], ang);
    RotatePointAroundVector(mat[1], rotateaxis, orientation[1], ang);
    RotatePointAroundVector(mat[2], rotateaxis, orientation[2], ang);
    MatrixToEulerAngles(mat, angles);
    setAngles(angles);

    if (velocity != vec_zero) {
        PostEvent(EV_FallingRock_Rotate, FRAMETIME);
    }
}

void FallingRock::SetWait(Event *ev)
{
    wait = ev->GetFloat(1);
}

void FallingRock::SetSpeed(Event *ev)
{
    speed = ev->GetFloat(1);
}

void FallingRock::SetDmg(Event *ev)
{
    dmg = ev->GetInteger(1);
}

void FallingRock::setDamage(Event *ev)
{
    if (ev->NumArgs() < 1)
        return;

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm->hasFormula("OffensiveDamage"))
        return;

    str   damagestr = ev->GetString(1);
    float damagemod = 1.0f;
    if (gpm->getDefine(damagestr) != "")
        damagemod = (float)atof(gpm->getDefine(damagestr));
    GameplayFormulaData fd(this, 0, 0, "");
    float finaldamage = gpm->calculate("OffensiveDamage", fd, damagemod);
    Event *newev      = new Event(EV_FallingRock_SetDmg);
    newev->AddInteger((int)finaldamage);
    ProcessEvent(newev);
}

void FallingRock::SetBounceSound(str sound)
{
    bouncesound = sound;
    CacheResource(bouncesound.c_str(), this);
}

void FallingRock::SetBounceSound(Event *ev)
{
    SetBounceSound(ev->GetString(1));
}

void FallingRock::Activate(Event *ev)
{
    if (active == 1) {
        return;
    }

    if ((active == 2) && (spawnflags & SPAWN_AUTO_RESET)) {
        current   = NULL;
        activator = NULL;
        setMoveType(MOVETYPE_NONE);
        NoLerpThisFrame();
        setOrigin(start_origin);
    }

    activator = ev->GetEntity(1);

    if (wait) {
        PostEvent(EV_FallingRock_Start, wait);
    } else {
        ProcessEvent(EV_FallingRock_Start);
    }
}

void FallingRock::StartFalling(Event *ev)
{
    if (current) {
        return;
    }

    if (!active) {
        start_origin = origin;
    }

    active = 1;
    setMoveType(MOVETYPE_BOUNCE);
    setSolidType(SOLID_BBOX);
    PostEvent(EV_FallingRock_Rotate, FRAMETIME);
    edict->clipmask = MASK_SOLID | CONTENTS_BODY;

    last_bounce_origin = origin;
    current            = this;
    current            = SetNextBounceDir();
    if (current) {
        NextBounce();
    }
}

void FallingRock::Touch(Event *ev)
{
    Entity *other;

    other = ev->GetEntity(1);

    if (other != world) {
        if ((velocity != vec_zero) && (other->takedamage) && (level.time >= attack_finished)) {
            other->Damage(this, activator, dmg, origin, vec_zero, vec_zero, 20, 0, MOD_THROWNOBJECT);
            attack_finished = level.time + FRAMETIME;
        }
    }

    if (!current || (other != world)) {
        return;
    }
    if (bouncesound.length()) {
        Vector delta(origin - last_bounce_origin);

        if (delta.length() > 8) {
            last_bounce_origin = origin;
            Sound(bouncesound.c_str(), CHAN_VOICE);
        }
    }

    PostEvent(EV_FallingRock_Bounce, 0);
}

void FallingRock::Bounce(Event *ev)
{
    Vector delta;

    if (!current) {
        return;
    }

    do {
        delta = origin - current->origin;
        if ((delta * bounce_dir) >= (-2 * edict->radius)) {
            if (current->isSubclassOf(Trigger)) {
                current->ProcessEvent(EV_Trigger_StartThread);
            }
            current = SetNextBounceDir();
            if (!current) {
                velocity = vec_zero;
                if (spawnflags & SPAWN_AUTO_RESET) {
                    active = 2;
                } else if (spawnflags & REMOVE_ON_GROUND) {
                    PostEvent(EV_Remove, 0);
                }
                break;
            }
        } else {
            NextBounce();
            break;
        }
    } while (1);
}



void UseObject::MoveThePlayer( Event *ev)
{
	movetheplayer = true;
}


void UseObject::SetActionType( Event *ev )
{
	action_type = ev->GetString( 1 );
}






bool Teleporter::doFullTeleport( Entity *entity )
{
	if ( entity && entity->isSubclassOf( Player ) )
	{
		Player *player = (Player *)entity;

		if ( multiplayerManager.inMultiplayer() && multiplayerManager.isPlayerSpectator( player ) )
			return false;
	}

	return true;
}




bool UseObject::Loop( void )
{
	if ( !count )
		return false;
	
	return true;
}

void UseObject::Start( Event * ev )
{
	if ( move_thread.length() )
	{
		if ( !ExecuteThread( move_thread ) )
		{
			warning( "Start", "Null game script" );
		}
	}
	
	if ( ( spawnflags & MULTI_STATE ) && objectState )
	{
		animate->RandomAnimate( "move_backward", ev );
	}
	else
	{
		animate->RandomAnimate( "move", ev );
	}
	SetActiveState( NULL );
}

void UseObject::Stop( Entity *activator )
{
	if ( ( spawnflags & MULTI_STATE ) && objectState )
	{
		animate->RandomAnimate( "start" );
	}
	else
	{
		animate->RandomAnimate( "stop" );
	}
	
	if ( triggertarget.length() )
	{
		Event  *event;
		Entity *ent;
		
		ent = NULL;
		do
		{
			ent = G_FindTarget( ent, triggertarget.c_str() );
			if ( !ent )
			{
				break;
			}
			event = new Event( EV_Activate );
			event->AddEntity( activator );
			ent->PostEvent( event, 0.0f );
		}
		while ( 1 );
	}
	
	if ( ( spawnflags & MULTI_STATE ) && objectState )
	{
		if ( reset_thread.length() )
		{
			if ( !ExecuteThread( reset_thread ) )
            {
				warning( "Stop", "Null game script" );
            }
		}
	}
	else
	{
		if ( stop_thread.length() )
		{
			if ( !ExecuteThread( stop_thread ) )
            {
				warning( "Stop", "Null game script" );
            }
		}
	}
	
	objectState ^= 1;
	
	if ( reset_time )
	{
		count = 0;
		PostEvent( EV_UseObject_Reset, reset_time );
	}
	
	SetActiveState( NULL );
}

void UseObject::SetTriggerTarget( Event *ev )
{
	triggertarget = ev->GetString( 1 );
}

void UseObject::SetCount( Event *ev )
{
	count = ev->GetInteger( 1 );
}

void UseObject::SetState( Event *ev )
{
	state = ev->GetString( 1 );
}

void UseObject::DamageType( Event *ev )
{
	damage_type = MOD_NameToNum( ev->GetString( 1 ) );
}

void UseObject::Reset( Event *ev )
{
	animate->RandomAnimate( "move_backward", EV_UseObject_Resetting );
}

UseObject::UseObject()
{
	Event * e;

	animate = new Animate( this );

	if ( LoadingSavegame )
	{
		return;
	}

	setMoveType( MOVETYPE_NONE );
	setSolidType( SOLID_BBOX );
	setContents( CONTENTS_BODY );

	flags |= FL_ROTATEDBOUNDS;
	count = -1;
	triggertarget = "";
	move_thread = "";
	stop_thread = "";
	reset_thread = "";
	damage_type = MOD_NONE;
	takedamage = DAMAGE_YES;
	health = 100.0f;
	yaw_offset = 0.0f;
	cone = (float)cos( DEG2RAD( 90.0f ) );
	state = "";
	state_backwards = "";
	reset_time = 0;
	objectState = 0;
	active = true;
	movetheplayer = false;

	e = new Event( EV_Anim );
	e->AddString( "start" );
	PostEvent( e, 0.0f );
	PostEvent( EV_Show, 0.0f );
	PostEvent( EV_UseObject_SetActiveState, 0.0f );

	look_at_me = true;
}

Event EV_UseObject_DamageType (    "damage_type",
    EV_DEFAULT,
    "s",
    "newDamageType",
    "Sets what kind of damage is needed to activate the trigger.");

CLASS_DECLARATION( Entity, UseObject, "func_useobject" )
{
	{ &EV_Use,										NULL },
	{ &EV_UseObject_MoveThread,   					&UseObject::SetMoveThread },
	{ &EV_UseObject_StopThread,						&UseObject::SetStopThread },
	{ &EV_UseObject_ResetThread,					&UseObject::SetResetThread },
	{ &EV_UseObject_TriggerTarget,					&UseObject::SetTriggerTarget },
	{ &EV_UseObject_Offset,							&UseObject::SetOffset },
	{ &EV_UseObject_YawOffset,						&UseObject::SetYawOffset },
	{ &EV_UseObject_Count,							&UseObject::SetCount },
	{ &EV_UseObject_Cone,							&UseObject::SetCone },
	{ &EV_UseObject_State,							&UseObject::SetState },
	{ &EV_UseObject_StateBackwards,					&UseObject::SetBackwardsState },
	{ &EV_UseObject_ResetTime,						&UseObject::SetResetTime },
	{ &EV_UseObject_Reset,							&UseObject::Reset },
	{ &EV_UseObject_DamageType,						&UseObject::DamageType },
	{ &EV_UseObject_Resetting,						&UseObject::Resetting },
	{ &EV_UseObject_DamageTriggered,				&UseObject::DamageTriggered },
	{ &EV_Damage,									&UseObject::DamageFunc },
	{ &EV_UseObject_Activate,						&UseObject::ActivateEvent },
	{ &EV_UseObject_Deactivate,						&UseObject::DeactivateEvent },
	{ &EV_UseObject_UseMaterial,					&UseObject::UseMaterialEvent },
	{ &EV_UseObject_SetActiveState,					&UseObject::SetActiveState },
	{ &EV_UseObject_SetActionType,					&UseObject::SetActionType },
	{ &EV_UseObject_MoveThePlayer,					&UseObject::MoveThePlayer },

	{ NULL, NULL }
};






void UseObject::ActivateEvent( Event *ev )
{
	active = true;
	PostEvent( EV_UseObject_SetActiveState, 0.0f );
}

void UseObject::DamageFunc( Event *ev )
{
	Event			*e;
	Entity		*attacker;
	int         mod;
	
	if ( !count )
	{
		return;
	}
	
	mod = ev->GetInteger( 9 );
	
	if ( !MOD_matches( mod, damage_type ) )
	{
		return;
	}
	
	attacker = ev->GetEntity( 3 );
	
	if ( count > 0 )
	{
		count--;
		if ( !count )
		{
			setSolidType( SOLID_NOT );
			setContents( 0 );
		}
	}
	e = new Event( EV_UseObject_DamageTriggered );
	e->AddEntity( attacker );
	Start( e );
}

void UseObject::DamageTriggered( Event * ev )
{
	Stop( ev->GetEntity( 1 ) );
}

void UseObject::DeactivateEvent( Event *ev )
{
	active = false;
	PostEvent( EV_UseObject_SetActiveState, 0.0f );
}

void UseObject::Resetting( Event *ev )
{
	SetActiveState( NULL );
	animate->RandomAnimate( "start" );
	
	setSolidType( SOLID_BBOX );
	setContents( CONTENTS_BODY );
	
	count = 1;
	
	if ( triggertarget.length() )
	{
		Event  *event;
		Entity *ent;
		
		ent = NULL;
		do
		{
			ent = G_FindTarget( ent, triggertarget.c_str() );
			if ( !ent )
			{
				break;
			}
			event = new Event( EV_Activate );
			event->AddEntity( this );
			ent->PostEvent( event, 0.0f );
		}
		while ( 1 );
	}
	
	if ( reset_thread.length() )
	{
		if ( !ExecuteThread( reset_thread ) )
		{
			warning( "Resetting", "Null game script" );
		}
	}
}

void UseObject::SetActiveState( Event *ev )
{
	if ( !useMaterial.length() )
	{
		return;
	}
	
	if ( active && count )
	{
		SurfaceCommand( useMaterial.c_str(), "+skin1" );
	}
	else
	{
		SurfaceCommand( useMaterial.c_str(), "-skin1" );
	}
	if ( objectState )
	{
		SurfaceCommand( useMaterial.c_str(), "+skin2" );
	}
	else
	{
		SurfaceCommand( useMaterial.c_str(), "-skin2" );
	}
}

void UseObject::SetBackwardsState( Event *ev )
{
	state_backwards = ev->GetString( 1 );
}

void UseObject::SetCone( Event *ev )
{
	cone = (float)cos( DEG2RAD( ev->GetFloat( 1 ) ) );
}

void UseObject::SetMoveThread( Event *ev )
{
	move_thread = ev->GetString( 1 );
}

void UseObject::SetOffset( Event *ev )
{
	offset = ev->GetVector( 1 );
}

void UseObject::SetResetThread( Event *ev )
{
	reset_thread = ev->GetString( 1 );
}

void UseObject::SetResetTime( Event *ev )
{
	reset_time = ev->GetFloat( 1 );
}

void UseObject::SetStopThread( Event *ev )
{
	stop_thread = ev->GetString( 1 );
}

void UseObject::SetYawOffset( Event *ev )
{
	yaw_offset = ev->GetFloat( 1 );
}

void UseObject::UseMaterialEvent( Event *ev )
{
	useMaterial = ev->GetString( 1 );
}


// SDK misc.cpp:2354-2357.  This port had the class but not the registration, so
CLASS_DECLARATION(Entity, Waypoint, "info_waypoint")
{
    {NULL, NULL}
};
