/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "g_local.h"
#include "object.h"
#include "sentient.h"
#include "misc.h"
#include "explosion.h"
#include "gibs.h"
#include "specialfx.h"
#include "g_phys.h"
#include "game.h"
#include "../script/scriptexception.h"




Event EV_ThrowObject_Pickup
(
	"pickup",
	EV_DEFAULT,
	"es",
	"entity tag_name",
	"Picks up this throw object and attaches it to the entity."
);


// silent unhandled-command no-op retail produces (corepp/listener.cpp:3518-3522).  Do NOT
Event EV_Object_Shootable (    "shootable",
    EV_DEFAULT,
    NULL,
    NULL,
    "Make the object shootable but not necessarily solid to the player.");

Event EV_Object_SetAnim
(
	"anim",
	EV_DEFAULT,
	"s",
	"animname",
	"Sets up the object with the specified animname."
);

Event EV_Object_Setup
(
	"_setup",
	EV_CODEONLY,
	NULL,
	NULL,
	"Sets up an object.");

CLASS_DECLARATION(Entity, Object, "object") {
    {&EV_Killed,           &Object::Killed       },
    {&EV_Object_Setup,     &Object::Setup        },
    {&EV_Object_SetAnim,   &Object::SetAnim      },
    {&EV_Object_Shootable, &Object::MakeShootable},
    {NULL,                 NULL                  }
};

Object::Object()
{
    animate = new Animate(this);
    if (LoadingSavegame) {
        return;
    }

    setSolidType(SOLID_NOT);

    health = 0;

    takedamage = (spawnflags & 2) ? DAMAGE_NO : DAMAGE_YES;

    flags |= FL_ROTATEDBOUNDS;

    PostEvent(EV_Object_Setup, EV_POSTSPAWN);
}

void Object::Setup(Event *ev)
{
    if (!health) {
        health     = (maxs - mins).length();
        max_health = health;
    }
}

void Object::MakeShootable(Event *ev)
{
    setContents(CONTENTS_SHOOTABLE_ONLY);
    link();
}

void Object::Killed(Event *ev)
{
    Entity     *ent;
    Entity     *attacker;
    Event      *event;
    const char *name;

    takedamage = DAMAGE_NO;
    setSolidType(SOLID_NOT);
    hideModel();

    attacker = ev->GetEntity(1);

    if (flags & FL_DIE_EXPLODE) {
        CreateExplosion(origin, 50.0f, this, this, this);
    }

    if (flags & FL_DIE_GIBS) {
        setSolidType(SOLID_NOT);
        hideModel();

        CreateGibs(this, -150.0f, edict->s.scale, 3);
    }

    name = KillTarget();
    if (name && strcmp(name, "")) {
        ent = NULL;
        for (;;) {
            ent = G_FindTarget(ent, name);
            if (!ent) {
                break;
            }
            ent->PostEvent(EV_Remove, 0.0f);
        }
    }

    name = Target();
    if (name && strcmp(name, "")) {
        ent = NULL;
        for (;;) {
            ent = G_FindTarget(ent, name);
            if (!ent) {
                break;
            }
            event = new Event(EV_Activate);
            event->AddEntity(attacker);
            ent->ProcessEvent(event);
        }
    }

    PostEvent(EV_Remove, 0.0f);
}



Event EV_ThrowObject_Throw (    "throw",
    EV_DEFAULT,
    "efeF",
    "owner speed targetent grav",
    "Throw this throw object.");
Event EV_ThrowObject_PickupOffset (    "pickupoffset",
    EV_DEFAULT,
    "v",
    "pickup_offset",
    "Sets the pickup_offset.");
Event EV_ThrowObject_ThrowSound (    "throwsound",
    EV_DEFAULT,
    "s",
    "throw_sound",
    "Sets the sound to play when object is thrown.");

// SDK object.cpp:232 -- retail derives ThrowObject from Object, not from Animate.
CLASS_DECLARATION(Object, ThrowObject, "func_throwobject") {
    {&EV_Touch,                    &ThrowObject::Touch       },
    {&EV_ThrowObject_Pickup,       &ThrowObject::Pickup      },
    {&EV_ThrowObject_Throw,        &ThrowObject::Throw       },
    {&EV_ThrowObject_PickupOffset, &ThrowObject::PickupOffset},
    {&EV_ThrowObject_ThrowSound,   &ThrowObject::ThrowSound  },
    {NULL,                         NULL                      }
};

ThrowObject::ThrowObject()
{
    if (LoadingSavegame) {
        return;
    }
    pickup_offset = vec_zero;
    damage        = 0;
    hurt_target   = qfalse;
    owner         = ENTITYNUM_NONE;
}

void ThrowObject::PickupOffset(Event *ev)
{
    pickup_offset = edict->s.scale * ev->GetVector(1);
}

void ThrowObject::ThrowSound(Event *ev)
{
    throw_sound = ev->GetString(1);
}

void ThrowObject::Touch(Event *ev)
{
    Entity *other;

    if (movetype != MOVETYPE_BOUNCE) {
        return;
    }

    other = ev->GetEntity(1);
    assert(other);

    if (other->isSubclassOf(Teleporter)) {
        return;
    }

    if (other->entnum == owner) {
        return;
    }

    if (throw_sound.length()) {
        StopLoopSound();
    }

    if (other->takedamage && !hurt_target) {
        other->Damage(
            this,
            G_GetEntity(owner),
            damage,
            origin,
            velocity,
            level.impact_trace.plane.normal,
            32,
            0,
            MOD_THROWNOBJECT
        );
        hurt_target = qtrue;
    }

    Damage(this, this, 0.0f, origin, velocity, level.impact_trace.plane.normal, 32, 0, MOD_THROWNOBJECT);
}

void ThrowObject::Throw(const Entity *owner_shadow1, float speed, const Sentient *targetent, float gravity_shadow1, float throw_damage)
{
    float    traveltime;
    float    vertical_speed;
    Vector   target_shadow1;
    Vector   dir;
    Vector   xydir;
    Event    *e;

    e = new Event(EV_Detach);
    ProcessEvent(e);

    this->owner = owner_shadow1->entnum;
    edict->ownerNum = owner_shadow1->entnum;

    damage = throw_damage;
    target_shadow1 = targetent->origin;
    target_shadow1.z += targetent->viewheight;

    setMoveType(MOVETYPE_BOUNCE);
    setSolidType(SOLID_BBOX);
    edict->clipmask = MASK_PROJECTILE;

    dir = target_shadow1 - origin;
    xydir = dir;
    xydir.z = 0;
    traveltime = xydir.length() / speed;
    vertical_speed = (dir.z / traveltime) + (0.5f * gravity_shadow1 * sv_currentGravity->value * traveltime);
    xydir.normalize();

    if (throw_sound.length()) {
        LoopSound(throw_sound.c_str());
    }

    velocity = speed * xydir;
    velocity.z = vertical_speed;

    angles = velocity.toAngles();
    setAngles(angles);

    avelocity.x = crandom() * 200.0f;
    avelocity.y = crandom() * 200.0f;
    takedamage = DAMAGE_YES;
}

void ThrowObject::Throw(Event *ev)
{
    Entity   *ownerEnt;
    Sentient *targetent;
    float     speed;
    float     traveltime;
    float     vertical_speed;
    Vector    targetPos;
    Vector    dir;
    float     grav;
    Vector    xydir;
    Event    *e;

    ownerEnt = ev->GetEntity(1);
    assert(ownerEnt);

    if (!ownerEnt) {
        ScriptError("owner == NULL");
        return;
    }

    speed = ev->GetFloat(2);
    if (!speed) {
        speed = 1;
    }

    targetent = (Sentient *)ev->GetEntity(3);
    assert(targetent);
    if (!targetent) {
        ScriptError("targetent == NULL");
        return;
    }

    if (ev->NumArgs() > 3) {
        grav = ev->GetFloat(4);
    } else {
        grav = 1;
    }

    if (ev->NumArgs() > 4) {
        damage = ev->GetFloat(5);
    } else {
        damage = 0;
    }

    e = new Event(EV_Detach);
    ProcessEvent(e);

    this->owner       = ownerEnt->entnum;
    edict->ownerNum = ownerEnt->entnum;

    gravity = grav;

    if (targetent->isSubclassOf(Sentient)) {
        targetPos = targetent->origin;
        targetPos.z += targetent->viewheight;
    } else {
        targetPos = targetent->centroid;
    }

    setMoveType(MOVETYPE_BOUNCE);
    setSolidType(SOLID_BBOX);
    edict->clipmask = MASK_PROJECTILE;

    dir            = targetPos - origin;
    xydir          = dir;
    xydir.z        = 0;
    traveltime     = xydir.length() / speed;
    vertical_speed = (dir.z / traveltime) + (0.5f * gravity * sv_currentGravity->value * traveltime);
    xydir.normalize();

    if (throw_sound.length()) {
        LoopSound(throw_sound.c_str());
    }

    velocity   = speed * xydir;
    velocity.z = vertical_speed;

    angles = velocity.toAngles();
    setAngles(angles);

    avelocity.x = crandom() * 200;
    avelocity.y = crandom() * 200;
    takedamage  = DAMAGE_YES;
}

void ThrowObject::Pickup(Entity *ent, const str &bone)
{
    Event *e;

    setOrigin(pickup_offset);

    e = new Event(EV_Attach);
    e->AddEntity(ent);
    e->AddString(bone);
    ProcessEvent(e);
    hurt_target = false;
    edict->s.renderfx &= ~RF_FRAMELERP;
}

void ThrowObject::Pickup(Event *ev)
{
    Entity *ent;
    Event  *e;
    str     bone;

    ent = ev->GetEntity(1);

    assert(ent);
    if (!ent) {
        return;
    }
    bone = ev->GetString(2);

    setOrigin(pickup_offset);

    e = new Event(EV_Attach);
    e->AddEntity(ent);
    e->AddString(bone);
    ProcessEvent(e);

    hurt_target = qfalse;

    edict->s.renderfx &= ~RF_FRAMELERP;
}

void ThrowObject::Archive(Archiver& arc)
{
    Object::Archive(arc);

    arc.ArchiveInteger(&owner);
    arc.ArchiveVector(&pickup_offset);
    arc.ArchiveString(&throw_sound);
    arc.ArchiveFloat(&damage);
    arc.ArchiveBoolean(&hurt_target);
}




void Object::SetAnim( Event *ev )
{
	int animnum;
	
	if ( ( ev->NumArgs() >= 1 ) && gi.IsModel( edict->s.modelindex ) )
	{
		animnum = gi.Anim_Random( edict->s.modelindex, ev->GetString( 1 ) );
		if ( animnum >= 0 )
		{
			animate->NewAnim( animnum );
		}
	}
}

