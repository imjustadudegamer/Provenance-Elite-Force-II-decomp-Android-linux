#!/usr/bin/env python3
"""
Fix build errors in the EF2 port.
"""
import os
import re

def fix_bspline_h():
    """Fix bspline.h - add missing declarations."""
    filepath = "bspline.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add missing declarations to BSplineControlPoint
    old = """    void    Clear(void);
    void    Set(Vector pos);
    void    Get(Vector& pos, Vector& orient, float& speed);
    Vector *GetPosition(void);"""
    new = """    void    Clear(void);
    void    Set(Vector pos);
    void    Set(Vector pos, float pointspeed);
    void    Set(Vector pos, Vector orient, float pointspeed);
    void    Get(Vector& pos);
    void    Get(Vector& pos, Vector& orient, float& speed);
    Vector *GetPosition(void);"""
    content = content.replace(old, new)

    # Add missing SetControlPoint declaration to BSpline
    old = """    Vector *GetControlPoint(int id);
    void    GetControlPoint(int id, Vector& pos, Vector& orient, float& speed);
    void    SetControlPoint(int id, const Vector& new_control_point);
    void Archive(Archiver& arc) override;"""
    new = """    Vector *GetControlPoint(int id);
    void    GetControlPoint(int id, Vector& pos, Vector& orient, float& speed);
    void    SetControlPoint(int id, const Vector& new_control_point);
    void    SetControlPoint(int id, const Vector& new_control_point, const Vector& new_control_orient, const float& speed);
    void    Clear(void);
    void Archive(Archiver& arc) override;"""
    content = content.replace(old, new)

    # Add missing Set declarations to BSpline
    old = """    void
           Set(Vector      *control_points_,
               Vector      *control_orients_,
               float       *control_speeds_,
               int          num_control_points_,
               splinetype_t type);
    void   AppendControlPoint(const Vector& new_control_point);"""
    new = """    void
           Set(Vector      *control_points_,
               Vector      *control_orients_,
               float       *control_speeds_,
               int          num_control_points_,
               splinetype_t type);
    void
           Set(Vector      *control_points_,
               int          num_control_points_,
               splinetype_t type);
    void   AppendControlPoint(const Vector& new_control_point);"""
    content = content.replace(old, new)

    # Add missing SetTriggerTarget and SetThread declarations to SplinePath
    old = """    void CreatePath(Event *ev);
    void SetLoop(Event *ev);
    void SetSpeed(Event *ev);
    void SetTriggerTarget(Event *ev);
    void SetThread(Event *ev);
    void SetFov(Event *ev);
    void SetWatch(Event *ev);
    void SetFadeTime(Event *ev);"""
    new = """    void CreatePath(Event *ev);
    void SetLoop(Event *ev);
    void SetSpeed(Event *ev);
    void SetTriggerTarget(Event *ev);
    void SetThread(Event *ev);
    void SetFov(Event *ev);
    void SetWatch(Event *ev);
    void SetFadeTime(Event *ev);
    void SetTriggerTarget(const char *name);
    void SetThread(const char *name);"""
    content = content.replace(old, new)

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_bspline_cpp():
    """Fix bspline.cpp - add Clear implementation."""
    filepath = "bspline.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add Clear implementation before Set
    old = """void BSpline::Set(Vector *control_points_, int num_control_points_, splinetype_t type)"""
    new = """void BSpline::Clear(void)
{
    if (control_points) {
        delete[] control_points;
        control_points = NULL;
    }
    num_control_points = 0;
    has_orientation = false;
}

void BSpline::Set(Vector *control_points_, int num_control_points_, splinetype_t type)"""
    content = content.replace(old, new)

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_player_h():
    """Fix player.h - remove incorrect overrides."""
    filepath = "player.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove override from functions that don't override
    content = content.replace("void Killed(Event *ev) override;", "void Killed(Event *ev);")
    content = content.replace("void  CalcBlend(void) override;", "void  CalcBlend(void);")
    content = content.replace("void EndFrame(void) override;", "void EndFrame(void);")
    content = content.replace("Vector GunTarget(bool bNoCollision, const vec3_t position, const vec3_t forward) override;", "Vector GunTarget(bool bNoCollision, const vec3_t position, const vec3_t forward);")
    content = content.replace("void SetViewAngles(Vector angles) override;", "void SetViewAngles(Vector angles);")
    content = content.replace("Vector GetViewAngles(void) override { return v_angle; };", "Vector GetViewAngles(void) { return v_angle; };")

    # Fix WeaponSetItem::Archive visibility
    content = content.replace("""class WeaponSetItem : public Class
{    void Archive(Archiver&) ;

public:""", """class WeaponSetItem : public Class
{
public:
    void Archive(Archiver&) ;""")

    # Fix Class** to LightClass** conversion
    content = content.replace("arc.ArchiveObjectPointer((Class **)eptrptr);", "arc.ArchiveObjectPointer((LightClass **)eptrptr);")

    # Add CameraCut(Camera*) declaration
    content = content.replace("void    CameraCut(void);", "void    CameraCut(void);\n    void    CameraCut(Camera *ent);")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_entity_h():
    """Fix entity.h - add missing declarations."""
    filepath = "entity.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add Entity* overloads for DistanceTo and WithinDistance
    content = content.replace("""    qboolean DistanceTo(Vector pos) const;
    qboolean WithinDistance(Vector pos, float dist) const;""",
"""    qboolean DistanceTo(Vector pos) const;
    qboolean DistanceTo(Entity *ent) const;
    qboolean WithinDistance(Vector pos, float dist) const;
    qboolean WithinDistance(Entity *ent, float dist) const;""")

    # Add WithinDistanceXY(Entity*) overload
    content = content.replace("    bool WithinDistanceXY(const Vector &pos, float dist);",
"""    bool WithinDistanceXY(const Vector &pos, float dist);
    bool WithinDistanceXY(const Entity *ent, float dist);""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_h():
    """Fix actor.h - fix InFOV, add missing declarations."""
    filepath = "actor.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Fix InFOV to return false
    content = content.replace("""inline bool Actor::InFOV(Entity *ent)
{
    if (ent == m_Enemy) {
    } else {
    }
}""",
"""inline bool Actor::InFOV(Entity *ent)
{
    // TODO: implement properly using sensoryPerception->InFOV
    return false;
}""")

    # Add ScriptThreadLabel overloads for StartAnimation and DesiredAnimation
    content = content.replace("""    void                DesiredAnimation(int eAnimMode, const_str csAnimString);
    void                StartAnimation(int eAnimMode, const_str csAnimString);
    void                ContinueAnimation(void);""",
"""    void                DesiredAnimation(int eAnimMode, const_str csAnimString);
    void                StartAnimation(int eAnimMode, const_str csAnimString);
    void                DesiredAnimation(int eAnimMode, ScriptThreadLabel AnimLabel);
    void                StartAnimation(int eAnimMode, ScriptThreadLabel AnimLabel);
    void                ContinueAnimation(void);""")

    # Add ActorTypeStringToInt declaration
    content = content.replace("""    // Statemap condition vocabulary (see Actor::Conditions[] in actor.cpp).
    static Condition<Actor> Conditions[];""",
"""    // Statemap condition vocabulary (see Actor::Conditions[] in actor.cpp).
    static Condition<Actor> Conditions[];

    // EF2: Convert actor type string to integer
    static unsigned int ActorTypeStringToInt(const str &type);""")

    # Add SetAnim default parameter
    content = content.replace("qboolean       SetAnim(const str& anim, Event *ev, bodypart_t part = legs, const float animationRate = 1.0f);",
                             "qboolean       SetAnim(const str& anim, Event *ev = NULL, bodypart_t part = legs, const float animationRate = 1.0f);")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_cpp():
    """Fix actor.cpp - add ActorTypeStringToInt implementation."""
    filepath = "actor.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add ActorTypeStringToInt implementation at end
    impl = """

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

    return retValue;
}"""
    content = content.rstrip() + "\n" + impl

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_navigate_h():
    """Fix navigate.h - add NumNodes declaration."""
    filepath = "navigate.h"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace("""    PathNode *GetNode(int num);
    void     ResetNodes(void);
    int      NumLoadNodes(void);""",
"""    PathNode *GetNode(int num);
    void     ResetNodes(void);
    int      NumNodes(void);
    int      NumLoadNodes(void);""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_archive_h():
    """Fix archive.h - add Length accessor and ReadObject declaration."""
    filepath = "../corepp/../fgame/archive.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add Length accessor to ArchiveFile
    content = content.replace("""public:
    const char *Filename(void);
    qboolean    IsOpen() const { return opened; } // retail Archiver::Close's `if (file)`
    const byte *Buffer(void) const { return buffer; }
};""",
"""public:
    const char *Filename(void);
    qboolean    IsOpen() const { return opened; } // retail Archiver::Close's `if (file)`
    const byte *Buffer(void) const { return buffer; }
    size_t      Length(void) const { return length; }
};""")

    # Add ReadObject declaration
    content = content.replace("""    void ArchiveObject(Class *obj);



    // Diagnostic: report where inside the current object body the stream sits.""",
"""    void ArchiveObject(Class *obj);
    Class *ReadObject(void);



    // Diagnostic: report where inside the current object body the stream sits.""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_vehicle_h():
    """Fix vehicle.h - remove incorrect override."""
    filepath = "vehicle.h"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace("virtual void Killed(Event *ev) override;", "virtual void Killed(Event *ev);")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_object_h():
    """Fix object.h - remove incorrect override."""
    filepath = "object.h"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace("void Killed(Event *ev) override; // SDK object.cpp:123", "void Killed(Event *ev); // SDK object.cpp:123")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_cinematic_armature_h():
    """Fix CinematicArmature.h - fix Class** to LightClass** and add ReadObject."""
    filepath = "CinematicArmature.h"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace("arc.ArchiveObjectPointer( ( Class**) &_cinematicCamera );", "arc.ArchiveObjectPointer( ( LightClass**) &_cinematicCamera );")
    content = content.replace("arc.ArchiveObjectPointer( (Class**) &_cinematic );", "arc.ArchiveObjectPointer( (LightClass**) &_cinematic );")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_damage_modification_hpp():
    """Fix DamageModification.hpp - move forward declarations after include guard."""
    filepath = "DamageModification.hpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Move forward declarations after include guard
    old = """class DamageModificationSystem;
class Damage;
class DamageModifier;
class DamageModifierTikiName;
class DamageModifierName;
class DamageModifierGroup;
class DamageModifierActorType;
class DamageModifierTargetName;
class DamageModifierDamageType;

#ifndef __DAMAGEMODIFICATION_H__
#define __DAMAGEMODIFICATION_H__"""
    new = """#ifndef __DAMAGEMODIFICATION_H__
#define __DAMAGEMODIFICATION_H__

class DamageModificationSystem;
class Damage;
class DamageModifier;
class DamageModifierTikiName;
class DamageModifierName;
class DamageModifierGroup;
class DamageModifierActorType;
class DamageModifierTargetName;
class DamageModifierDamageType;"""
    content = content.replace(old, new)

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_follow_path_h():
    """Fix FollowPath.h - remove duplicate definitions."""
    filepath = "FollowPath.h"
    content = """//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/FollowPath.h                                  $
// $Revision:: 20                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
//
// DESCRIPTION:
// Provides base functionality for path following with avoidance
//

#ifndef __FOLLOW_PATH_H__
#define __FOLLOW_PATH_H__

// All FollowPath classes are defined in steering.h
#include "steering.h"

#endif // __FOLLOW_PATH_H__
"""
    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_follow_path_to_point_h():
    """Fix FollowPathToPoint.h - remove duplicate definitions."""
    filepath = "FollowPathToPoint.h"
    content = """//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/FollowPathToPoint.h                          $
// $Revision:: 6                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Specialization of FollowPath used to follow a path to a static point
//

#ifndef __FOLLOW_PATH_TO_POINT_H__
#define __FOLLOW_PATH_TO_POINT_H__

// FollowPathToPoint is defined in steering.h
#include "steering.h"

#endif // FollowPathToPoint.h
"""
    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_follow_path_to_entity_h():
    """Fix FollowPathToEntity.h - remove duplicate definitions."""
    filepath = "FollowPathToEntity.h"
    content = """//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/FollowPathToEntity.h                          $
// $Revision:: 7                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
//
// DESCRIPTION:
// Specialization of FollowPath used to follow moving entities
//

#ifndef __FOLLOW_PATH_TO_ENTITY_H__
#define __FOLLOW_PATH_TO_ENTITY_H__

// FollowPathToEntity is defined in steering.h
#include "steering.h"

#endif // FollowPathToEntity.h
"""
    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_follow_path_cpp():
    """Fix FollowPath.cpp - add includes and fix naming."""
    filepath = "FollowPath.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add includes
    content = content.replace("""#include "_pch_cpp.h"
#include "actor.h"
#include "FollowPath.h\"""",
"""#include "_pch_cpp.h"
#include "actor.h"
#include "actor_locomotion.h"
#include "FollowPath.h"
#include "debuglines.h"
#include <algorithm>""")

    # Fix WDPrintf to DPrintf
    content = content.replace("WDPrintf", "DPrintf")

    # Fix TargetName to targetname
    content = content.replace("TargetName", "targetname")

    # Fix actortype to m_actortype
    content = content.replace("actortype", "m_actortype")

    # Fix total_delta to velocity
    content = content.replace("total_delta", "velocity")

    # Fix const qualifiers on FollowNodePath methods
    content = content.replace("FollowNodePtr FollowNodePath::GetPreviousNode(const FollowNodePtr node ) const",
                             "FollowNodePtr FollowNodePath::GetPreviousNode(const FollowNodePtr node )")
    content = content.replace("FollowNodePtr FollowNodePath::GetNextNode(const FollowNodePtr node ) const",
                             "FollowNodePtr FollowNodePath::GetNextNode(const FollowNodePtr node )")
    content = content.replace("void FollowNodePath::Draw( void ) const",
                             "void FollowNodePath::Draw( void )")

    # Fix CheckBlocked - add Event declaration
    content = content.replace("""const bool FollowPath::CheckBlocked( Actor &self )
{
	trace_t trace = self.Trace( GetAvoidanceDistance() , "FollowPath" );

	if ( trace.ent )
	{
		Entity *door = trace.ent->entity;
		if ( door && door->isSubclassOf( Door ) )
		{
			ev->AddEntity( &self );
			door->PostEvent( ev, 0 );

			return true;
		}
	}
	return false;
}""",
"""const bool FollowPath::CheckBlocked( Actor &self )
{
	trace_t trace = self.Trace( GetAvoidanceDistance() , "FollowPath" );

	if ( trace.ent )
	{
		Entity *door = trace.ent->entity;
		if ( door && door->isSubclassOf( Door ) )
		{
			Event *ev = new Event( EV_Use );
			ev->AddEntity( &self );
			door->PostEvent( ev, 0 );

			return true;
		}
	}
	return false;
}""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_g_utils_h():
    """Fix g_utils.h - add MOD functions and CacheResource overload."""
    filepath = "g_utils.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add MOD functions
    content = content.replace("qboolean MOD_matches(int incoming_damage, int damage_type);",
"""qboolean MOD_matches(int incoming_damage, int damage_type);
int      MOD_NameToNum(const str &meansOfDeath);
const char *MOD_NumToName(int meansOfDeath);""")

    # Fix CacheResource signature
    content = content.replace("void CacheResource(const char *stuff);",
                             "void CacheResource(const char *stuff, Entity *ent = NULL);")

    # Add G_CacheStateMachineAnims
    content = content.replace("void CacheResource(const char *stuff, Entity *ent = NULL);",
"""void CacheResource(const char *stuff, Entity *ent = NULL);
void G_CacheStateMachineAnims(Entity *ent, const char *stateMachineName);""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_g_utils_cpp():
    """Fix g_utils.cpp - add MOD functions and G_CacheStateMachineAnims."""
    filepath = "g_utils.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add MOD functions after MOD_matches
    old = """qboolean MOD_matches(int incoming_damage, int damage_type)
{
    if (damage_type == -1) {
        return true;
    } else {
        return incoming_damage == damage_type;
    }
}"""
    new = """qboolean MOD_matches(int incoming_damage, int damage_type)
{
    if (damage_type == -1) {
        return true;
    } else {
        return incoming_damage == damage_type;
    }
}

int MOD_NameToNum(const str &meansOfDeath)
{
    int i;

    for (i = 0; i < MOD_TOTAL_NUMBER; i++) {
        if (!meansOfDeath.icmp(means_of_death_strings[i])) {
            return i;
        }
    }

    gi.WDPrintf("Unknown means of death - %s\\n", meansOfDeath.c_str());
    return -1;
}

const char *MOD_NumToName(int meansOfDeath)
{
    if ((meansOfDeath > MOD_TOTAL_NUMBER) || (meansOfDeath < 0)) {
        gi.WDPrintf("Unknown means of death num - %d\\n", meansOfDeath);
        return "";
    }

    return means_of_death_strings[meansOfDeath];
}"""
    content = content.replace(old, new)

    # Add G_CacheStateMachineAnims after MOD_NumToName
    old = """const char *MOD_NumToName(int meansOfDeath)
{
    if ((meansOfDeath > MOD_TOTAL_NUMBER) || (meansOfDeath < 0)) {
        gi.WDPrintf("Unknown means of death num - %d\\n", meansOfDeath);
        return "";
    }

    return means_of_death_strings[meansOfDeath];
}"""
    new = """const char *MOD_NumToName(int meansOfDeath)
{
    if ((meansOfDeath > MOD_TOTAL_NUMBER) || (meansOfDeath < 0)) {
        gi.WDPrintf("Unknown means of death num - %d\\n", meansOfDeath);
        return "";
    }

    return means_of_death_strings[meansOfDeath];
}

void G_CacheStateMachineAnims(Entity *ent, const char *stateMachineName)
{
    Container<const char *> animNames;
    const char *animName;
    StateMap *stateMap;
    int i;

    // Cache the statemap

    if (ent->isSubclassOf(Player)) {
        stateMap = GetStatemap(stateMachineName, (Condition<Class> *)Player::Conditions, NULL, false, true);
    } else if (ent->isSubclassOf(Actor)) {
        stateMap = GetStatemap(stateMachineName, (Condition<Class> *)Actor::Conditions, NULL, false, true);
    } else {
        return;
    }

    // Get all of the anims in the statemachine

    stateMap->GetAllAnims(&animNames);

    // Loop through all of the anims in this statemachine and cache them all

    for (i = 1; i <= animNames.NumObjects(); i++) {
        animName = animNames.ObjectAt(i);
        CacheResource(animName, ent);
    }
}"""
    content = content.replace(old, new)

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_player_start_cpp():
    """Fix PlayerStart.cpp - fix Event declarations."""
    filepath = "PlayerStart.cpp"
    content = """//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/PlayerStart.cpp                           $
// $Revision:: 7                                                              $
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
// Player start location entity declarations
//

#include "_pch_cpp.h"
#include "entity.h"
#include "trigger.h"
#include "PlayerStart.h"

Event EV_PlayerStart_SetThread
(
	"thread",
	EV_SCRIPTONLY,
	"s",
	"thread",
	"Set the thread to execute when this player start is used"
);

Event EV_PlayerDeathmatchStart_SetType
(
	"spawnpoint_type",
	EV_DEFAULT,
	"s",
	"spawnpointType",
	"Sets the named type of this spawnpoint"
);

CLASS_DECLARATION(Listener, PlayerStart, NULL)
{
	{&EV_PlayerStart_SetThread,		&PlayerStart::SetThread},
	{NULL,							NULL}
};

PlayerStart::PlayerStart()
{
	if (LoadingSavegame) {
		return;
	}

	//setContents(CONTENTS_BODY);
	setSolidType(SOLID_NOT);
	edict->r.svFlags |= SVF_NOCLIENT;
}

void PlayerStart::SetThread(Event *ev)
{
	thread = ev->GetString(1);
}

CLASS_DECLARATION(PlayerStart, PlayerDeathmatchStart, NULL)
{
	{&EV_PlayerDeathmatchStart_SetType,	&PlayerDeathmatchStart::SetType},
	{NULL,								NULL}
};

void PlayerDeathmatchStart::SetType(Event *ev)
{
	_type = ev->GetString(1);
}
"""
    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_rage_ai_cpp():
    """Fix RageAI.cpp - add includes and fix naming."""
    filepath = "RageAI.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add includes
    content = content.replace("""#include "_pch_cpp.h"
//#include "g_local.h"
#include "RageAI.h"
#include "player.h\"""",
"""#include "_pch_cpp.h"
//#include "g_local.h"
#include "RageAI.h"
#include "player.h"
#include "actor_enemymanager.h"
#include "actor_locomotion.h"
#include "fuzzyengine.h\"""")

    # Fix naming conventions
    content = content.replace("fuzzyEngine", "m_fuzzyEngine")
    content = content.replace("fuzzyEngine_active", "m_fuzzyEngineActive")
    content = content.replace("statemap_name", "m_statemapName")
    content = content.replace("WDPrintf", "DPrintf")
    content = content.replace("fuzzy_conditionals", "m_fuzzyConditionals")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_locomotion_h():
    """Fix actor_locomotion.h - remove duplicate enums, add LocomotionController."""
    filepath = "actor_locomotion.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove duplicate enum definitions
    content = content.replace("""// SDK actorincludes.h:79 -- applied per Accelerate() call, i.e. per server frame.
// At EF2's 20 fps that is 1200 deg/sec, which matches the port's existing turn rate.
#define TURN_SPEED 60

// SDK actorincludes.h:347
typedef enum {
    MOVEMENT_TYPE_NORMAL,
    MOVEMENT_TYPE_ANIM
} MovementType_t;

// SDK actorincludes.h:520
typedef enum {
    MOVEMENT_STYLE_NONE,
    MOVEMENT_STYLE_WALK,
    MOVEMENT_STYLE_RUN,

    MOVEMENT_STYLE_MAX
} MovementStyle;""",
"""// SDK actorincludes.h:79 -- applied per Accelerate() call, i.e. per server frame.
// At EF2's 20 fps that is 1200 deg/sec, which matches the port's existing turn rate.
#define TURN_SPEED 60

// MovementType_t and MovementStyle are defined in actorincludes.h""")

    # Add LocomotionController forward declaration and class
    content = content.replace("""class Actor;
class Archiver;
class PathNode;""",
"""class Actor;
class Archiver;
class PathNode;
class LocomotionController;

// SDK actor_locomotion.h:31-63
class LocomotionController
{
public:
    LocomotionController();
    LocomotionController(Actor *actor);
    ~LocomotionController();

    void Begin();
    void Evaluate();
    void End();

    // Accessors & Mutators
    void           SetMovementStyle(MovementStyle style);
    MovementStyle  GetMovementStyle();

    // Archiving
    virtual void   Archive(Archiver &arc);
    void           DoArchive(Archiver &arc, Actor *actor);

protected:
    void           _init();

private:
    Actor          *act;
    MovementStyle  _movementStyle;
    FollowPath     _chase;
};""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_locomotion_cpp():
    """Fix actor_locomotion.cpp - add ActorTrace helper."""
    filepath = "actor_locomotion.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add ActorTrace helper before IsMoveValid
    content = content.replace("""//
stepmoveresult_t MovementSubsystem::IsMoveValid(""",
"""// Helper function for actor traces
static trace_t ActorTrace(const Vector& begin, const Vector& end, const char *reason)
{
    // This is a simplified version - the full version would use the actor's clipmask
    return G_Trace(begin, vec_zero, vec_zero, end, NULL, MASK_PLAYERSOLID, qfalse, reason);
}

//
stepmoveresult_t MovementSubsystem::IsMoveValid(""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_enemymanager_h():
    """Fix actor_enemymanager.h - remove duplicate HateListEntry_t."""
    filepath = "actor_enemymanager.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove duplicate HateListEntry_t definition
    content = content.replace("""// Always included after "actor.h", which supplies Container / EntityPtr / Archiver.

class Actor;
class Entity;
class Archiver;

// SDK actorincludes.h:246
typedef struct {
    EntityPtr enemy;         // Pointer to the entity
    float     lastSightTime; // Last time I tried to see this entity
    float     nextSightTime; // Next time I try and see this entity
    qboolean  canSee;        // Can I see the enemy (based on last time I tried)
    float     damageCaused;  // Total damage that entity has done to me
    float     hate;          // How much hate I have for this entity
    float     lastDistance;
} HateListEntry_t;

// SDK actorincludes.h:76 / :78
#define EF2_MIN_SIGHT_DELAY      2
#define EF2_DEFAULT_INITIAL_HATE 100""",
"""// Always included after "actor.h", which supplies Container / EntityPtr / Archiver.
// HateListEntry_t is defined in actorincludes.h

class Actor;
class Entity;
class Archiver;

// SDK actorincludes.h:76 / :78
#define EF2_MIN_SIGHT_DELAY      2
#define EF2_DEFAULT_INITIAL_HATE 100""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_rageai_h():
    """Fix rageai.h - remove duplicate typedefs."""
    filepath = "rageai.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove duplicate BehaviorPackageEntry_t
    content = content.replace("""//============================
// PackageManager -- SDK RageAI.h:156
//============================
//
// SDK actorincludes.h:293, verbatim.  `packageIndex` indexes the GLOBAL package list
// -- retail's `PackageList`, which in this port is Actor::s_behaviorPackages: same
// file (global/BehaviorPackages.txt), same parse order, so the index means the same
// thing here as in a 2003 savegame.  The corpus carries real ones: 874 Actor frames
// hold _currentPackageIndex 61, 97 hold 6, 67 hold 169.
typedef struct {
    int   packageIndex;
    float currentScore;
    float lastScore;
    float lastTimeExecuted;
    float priority;
} BehaviorPackageEntry_t;

class PackageManager""",
"""//============================
// PackageManager -- SDK RageAI.h:156
//============================
//
// BehaviorPackageEntry_t is defined in actorincludes.h
// `packageIndex` indexes the GLOBAL package list
// -- retail's `PackageList`, which in this port is Actor::s_behaviorPackages: same
// file (global/BehaviorPackages.txt), same parse order, so the index means the same
// thing here as in a 2003 savegame.  The corpus carries real ones: 874 Actor frames
// hold _currentPackageIndex 61, 97 hold 6, 67 hold 169.

class PackageManager""")

    # Remove duplicate PackageTendency_t and Tendency_t
    content = content.replace("""//============================
// Class Personality -- SDK RageAI.h:197
//============================
// SDK actorincludes.h:303 / :313, verbatim.  packageIndex is the same global index
// BehaviorPackageEntry_t uses.
typedef struct {
    int   packageIndex;
    float tendency;
    float lastTendencyCheck;
} PackageTendency_t;

typedef struct {
    str   tendencyName;
    float tendencyValue;
} Tendency_t;""",
"""//============================
// Class Personality -- SDK RageAI.h:197
//============================
// PackageTendency_t and Tendency_t are defined in actorincludes.h
// packageIndex is the same global index BehaviorPackageEntry_t uses.""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_sensoryperception_h():
    """Fix actor_sensoryperception.h - remove duplicate LineOfSight_t."""
    filepath = "actor_sensoryperception.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Remove duplicate LineOfSight_t definition
    content = content.replace("""// Always included after "actor.h", which supplies Vector / str / Entity / Archiver
// and the STIMULI_* masks.

class Actor;
class Entity;
class Archiver;

// SDK actorincludes.h:238
typedef struct {
    int      entNum;
    float    time;
    qboolean inLineOfSight;
} LineOfSight_t;

// SDK actorincludes.h:74 / :75
#define EF2_DEFAULT_FOV              150.0f
#define EF2_DEFAULT_VISION_DISTANCE  1536.0f""",
"""// Always included after "actor.h", which supplies Vector / str / Entity / Archiver
// and the STIMULI_* masks.
// LineOfSight_t is defined in actorincludes.h

class Actor;
class Entity;
class Archiver;

// SDK actorincludes.h:74 / :75
#define EF2_DEFAULT_FOV              150.0f
#define EF2_DEFAULT_VISION_DISTANCE  1536.0f""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_listener_h():
    """Fix listener.h - fix container.h include."""
    filepath = "listener.h"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace('#include "container.h"', '#include "../corepp/container.h"')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_scriptvariable_h():
    """Fix scriptvariable.h - fix container.h include."""
    filepath = "scriptvariable.h"
    with open(filepath, 'r') as f:
        f.read()
        content = f.read()

    content = content.replace('#include "container.h"', '#include "../corepp/container.h"')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_camera_path_h():
    """Fix CameraPath.h - fix quaternion.h include."""
    filepath = "CameraPath.h"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace('#include <qcommon/quaternion.h>', '#include "../qcommon/quaternion.h"')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_weapon_dual_wield_cpp():
    """Fix WeaponDualWield.cpp - fix gameplaymanager.h include."""
    filepath = "WeaponDualWield.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace('#include <qcommon/gameplaymanager.h>', '#include "../qcommon/gameplaymanager.h"')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_move_random_direction_cpp():
    """Fix MoveRandomDirection.cpp - fix gameplaymanager.h include."""
    filepath = "MoveRandomDirection.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    content = content.replace('#include <qcommon/gameplaymanager.h>', '#include "../qcommon/gameplaymanager.h"')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_combatsubsystem_cpp():
    """Fix actor_combatsubsystem.cpp - fix includes and naming."""
    filepath = "actor_combatsubsystem.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Fix gameplaymanager.h include
    content = content.replace('#include "gameplaymanager.h"', '#include "../qcommon/gameplaymanager.h"')

    # Fix SetWeaponAnim to playAnim
    content = content.replace('weapon->SetWeaponAnim("endfire");', 'weapon->playAnim("endfire");')

    # Fix GetLeadingTargetPos call
    content = content.replace('act->GetLeadingTargetPos(', 'GetLeadingTargetPos(')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_actor_combatsubsystem_h():
    """Fix actor_combatsubsystem.h - add missing declarations."""
    filepath = "actor_combatsubsystem.h"
    with open(filepath, 'r') as f:
        content = f.read()

    # Add AimWeaponTag(Vector) and GetLeadingTargetPos declarations
    content = content.replace("void AimWeaponTag(Entity *target);",
"""void AimWeaponTag(const Vector& targetPos);
    void AimWeaponTag(Entity *target);
    Vector GetLeadingTargetPos(float projSpeed, Vector originalTargetPos, Entity *target);""")

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_damage_modification_cpp():
    """Fix DamageModification.cpp - fix includes and naming."""
    filepath = "DamageModification.cpp"
    with open(filepath, 'r') as f:
        content = f.read()

    # Fix include
    content = content.replace('#include "DamageModification.hpp"', '#include "damagemodification.h"')

    # Fix naming
    content = content.replace('->name', '->targetname')
    content = content.replace('->actortype', '->m_actortype')

    with open(filepath, 'w') as f:
        f.write(content)
    print(f"Fixed {filepath}")

def fix_qcommon_h():
    """Fix qcommon headers - fix includes."""
    # Fix gameplaymanager.h
    with open("gameplaymanager.h", 'r') as f:
        content = f.read()
    content = content.replace('#include <game/g_local.h>', '#include "../fgame/g_local.h"')
    content = content.replace('#include <game/actor.h>', '#include "../fgame/actor.h"')
    content = content.replace('#include <game/weapon.h>', '#include "../fgame/weapon.h"')
    content = content.replace('#include <game/player.h>', '#include "../fgame/player.h"')
    content = content.replace('#include <game/gamecmds.h>', '#include "../fgame/gamecmds.h"')
    with open("gameplaymanager.h", 'w') as f:
        f.write(content)

    # Fix gameplayformulamanager.h
    with open("gameplayformulamanager.h", 'r') as f:
        content = f.read()
    content = content.replace('#include <game/g_local.h>', '#include "../fgame/g_local.h"')
    content = content.replace('#include <game/actor.h>', '#include "../fgame/actor.h"')
    content = content.replace('#include <game/weapon.h>', '#include "../fgame/weapon.h"')
    content = content.replace('#include <game/entity.h>', '#include "../fgame/entity.h"')
    content = content.replace('#include <game/player.h>', '#include "../fgame/player.h"')
    with open("gameplayformulamanager.h", 'w') as f:
        f.write(content)

    # Fix gameplaydatabase.h
    with open("gameplaydatabase.h", 'r') as f:
        content = f.read()
    content = content.replace('#include <game/g_local.h>', '#include "../fgame/g_local.h"')
    content = content.replace('#include <game/script.h>', '#include "../fgame/script.h"')
    content = content.replace('#include <game/class.h>', '#include "../fgame/class.h"')
    content = content.replace('#include <game/listener.h>', '#include "../fgame/listener.h"')
    content = content.replace('#include <game/str.h>', '#include "../fgame/str.h"')
    content = content.replace('#include <game/container.h>', '#include "../corepp/container.h"')
    content = content.replace('#include <qcommon/tiki_script.h>', '#include "tiki_script.h"')
    with open("gameplaydatabase.h", 'w') as f:
        f.write(content)

    # Fix quaternion.h
    with open("quaternion.h", 'r') as f:
        content = f.read()
    content = content.replace('#include "vector.h"', '#include "../corepp/vector.h"')
    with open("quaternion.h", 'w') as f:
        f.write(content)

    # Fix output.h
    with open("output.h", 'r') as f:
        content = f.read()
    content = content.replace('#include <game/g_local.h>', '#include "../fgame/g_local.h"')
    content = content.replace('#include <game/listener.h>', '#include "../fgame/listener.h"')
    content = content.replace('#include <game/class.h>', '#include "../fgame/class.h"')
    with open("output.h", 'w') as f:
        f.write(content)

    print("Fixed qcommon headers")

if __name__ == "__main__":
    # Change to fgame directory
    os.chdir("<repo-root>/app/src/main/cpp/code/fgame")

    # Fix all files
    fix_bspline_h()
    fix_bspline_cpp()
    fix_player_h()
    fix_entity_h()
    fix_actor_h()
    fix_actor_cpp()
    fix_navigate_h()
    fix_archive_h()
    fix_vehicle_h()
    fix_object_h()
    fix_cinematic_armature_h()
    fix_damage_modification_hpp()
    fix_follow_path_h()
    fix_follow_path_to_point_h()
    fix_follow_path_to_entity_h()
    fix_follow_path_cpp()
    fix_g_utils_h()
    fix_g_utils_cpp()
    fix_player_start_cpp()
    fix_rage_ai_cpp()
    fix_actor_locomotion_h()
    fix_actor_locomotion_cpp()
    fix_actor_enemymanager_h()
    fix_rageai_h()
    fix_actor_sensoryperception_h()
    fix_listener_h()
    fix_scriptvariable_h()
    fix_camera_path_h()
    fix_weapon_dual_wield_cpp()
    fix_move_random_direction_cpp()
    fix_actor_combatsubsystem_cpp()
    fix_actor_combatsubsystem_h()
    fix_damage_modification_cpp()

    # Fix qcommon headers
    os.chdir("<repo-root>/app/src/main/cpp/code/qcommon")
    fix_qcommon_h()

    print("All fixes applied!")
