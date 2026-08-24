/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

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


#pragma once

#include "g_local.h"
#include "entity.h"
#include "trigger.h"
#include "scriptslave.h"

extern Event EV_Door_TryOpen;
extern Event EV_Door_GoDown;
extern Event EV_Door_GoUp;
extern Event EV_Door_HitBottom;
extern Event EV_Door_HitTop;
extern Event EV_Door_Fire;
extern Event EV_Door_Link;
extern Event EV_Door_SetSpeed;
extern Event EV_Door_Lock;
extern Event EV_Door_Unlock;

class Door;

typedef SafePtr<Door> DoorPtr;

class Door : public ScriptSlave
{
protected:
    str     sound_stop;
    str     sound_move;
    str     sound_message;
    str     sound_locked;
    float   lastblocktime;
    float   angle;
    Vector  dir;
    float   diropened;
    int     state;
    int     previous_state;
    int     trigger;
    int     nextdoor;
    DoorPtr master;
    float   next_locked_time;
    Vector  _extraTriggerSize;
    bool    m_bAlwaysAway;

    void     SetDir(Event *ev);
    void     OpenEnd(Event *ev);
    void     CloseEnd(Event *ev);
    void     Close(Event *ev);
    void     Open(Event *ev);
    void     DoorUse(Event *ev);
    void     DoorFire(Event *ev);
    void     DoorBlocked(Event *ev);
    void     FieldTouched(Event *ev);
    void     TryOpen(Event *ev);
    void     SpawnTriggerField(Vector fmins, Vector fmaxs);
    qboolean DoorTouches(Door *e1);
    void     LinkDoors(Event *ev);
    void     SetTime(Event *ev);
    void     LockDoor(Event *ev);
    void     UnlockDoor(Event *ev);
    void     SetStopSound(const str& sound);
    void     SetMoveSound(const str& sound);
    void     SetMessageSound(const str& sound);
    void     SetWait(Event *ev);
    void     SetDmg(Event *ev);



public:
    CLASS_PROTOTYPE(Door);

    qboolean locked;

    Door();
    qboolean isOpen(void);
    qboolean isCompletelyClosed(void);
    void     GetIsOpen(Event *ev); 
    qboolean CanBeOpenedBy(Entity *ent);
    void     setExtraTriggerSize(Event *ev);  
    int      GetState(void) { return state; }
    int      GetNextDoor(void) { return nextdoor; }
    void     Archive(Archiver    &arc) override;






public:
    void SetStopSound( Event *ev ); 
    void SetMoveSound( Event *ev ); 
    void SetMessageSound( Event *ev ); 
    void SetLockedSound( const str &sound ); 
    void SetLockedSound( Event *ev );  
    void setDamage( Event *ev ); 
};

// after locked, which retail never writes -- one byte long.
inline void Door::Archive(Archiver& arc)
{
    ScriptSlave::Archive(arc);

    arc.ArchiveString(&sound_stop);
    arc.ArchiveString(&sound_move);
    arc.ArchiveString(&sound_message);
    arc.ArchiveString(&sound_locked);

    if (arc.Loading()) {
        SetStopSound(sound_stop);
        SetMoveSound(sound_move);
        SetMessageSound(sound_message);
    }

    arc.ArchiveFloat(&lastblocktime);
    arc.ArchiveFloat(&angle);
    arc.ArchiveVector(&dir);
    arc.ArchiveFloat(&diropened);
    arc.ArchiveInteger(&state);
    arc.ArchiveInteger(&previous_state);
    arc.ArchiveInteger(&trigger);
    arc.ArchiveInteger(&nextdoor);
    arc.ArchiveSafePointer(&master);
    arc.ArchiveFloat(&next_locked_time);

    arc.ArchiveVector(&_extraTriggerSize);

    arc.ArchiveBoolean(&locked);
}

class RotatingDoor : public Door
{
protected:
    float  angle;
    Vector startangle;
    int    init_door_direction;

public:
    CLASS_PROTOTYPE(RotatingDoor);

    void DoOpen(Event *ev);
    void DoClose(Event *ev);
    void OpenAngle(Event *ev);
    void Archive(Archiver& arc) override;

    RotatingDoor();
};

inline void RotatingDoor::Archive(Archiver& arc)
{
    Door::Archive(arc);

    arc.ArchiveFloat(&angle);
    arc.ArchiveVector(&startangle);
    arc.ArchiveInteger(&init_door_direction);
}

class SlidingDoor : public Door
{
protected:
    float  totalmove;
    float  lip;
    Vector pos1;
    Vector pos2;
    float  basespeed;
    Vector movedir;

public:
    CLASS_PROTOTYPE(SlidingDoor);

    void SetMoveDir(Event *ev);
    void Setup(Event *ev);
    void SetLip(Event *ev);
    void SetSpeed(Event *ev);
    void DoOpen(Event *ev);   
    void DoClose(Event *ev);  
    void Archive(Archiver& arc) override;

    SlidingDoor();
};

inline void SlidingDoor::Archive(Archiver& arc)
{
    Door::Archive(arc);

    arc.ArchiveFloat(&totalmove);
    arc.ArchiveFloat(&lip);
    arc.ArchiveVector(&pos1);
    arc.ArchiveVector(&pos2);
    arc.ArchiveFloat(&basespeed);
    arc.ArchiveVector(&movedir);
}

// appears in 0 of the 295 retail savegames, so this is not corpus-provable either way.
class ScriptDoor : public Door
{
protected:
    bool   _toggle;
    bool   _targeted;
    float  _openPercentage;
    float  _currentPercentage;
    float  _totalMove;
    float  _currentMove;
    float  _baseSpeed;
    float  _speed;
    Vector _originalPos;
    Vector _currentPos;
    Vector _destinationPos;
    Vector _moveDir;

public:
    CLASS_PROTOTYPE(ScriptDoor);

    ScriptDoor();

    void SetToggle(Event *ev);                     
    void SetToggle(bool toggle);                   

    void SetTargeted(Event *ev);                   
    void SetTargeted(bool targeted);               

    void SetOpenPercentage(Event *ev);             
    void SetOpenPercentage(float openPercentage);  

    void SetMoveDir(Event *ev);                    
    void SetMoveDir(float moveDir);                

    void DoClose(Event *ev);                       
    void DoClose();

    void DoOpen(Event *ev);                        
    void DoOpen();

    void SetUpMovement();                          
    void SetUpScriptDoor();                        




    void ScriptDoorGetState(Event *ev);            

    void SetUpMovement(Event *ev);                 

    void SetUpScriptDoor(Event *ev);               

    void ScriptDoorUse(Event *ev);                 
    void ScriptDoorOpen(Event *ev);                
    void ScriptDoorClose(Event *ev);               
    void ScriptDoorFieldTouched(Event *ev);        

    void ScriptDoorForceOpen(Event *ev);           
    void ScriptDoorForceClose(Event *ev);          

    void Archive(Archiver& arc) override;
};

inline void ScriptDoor::Archive(Archiver& arc)
{
    Door::Archive(arc);

    arc.ArchiveBool(&_toggle);
    arc.ArchiveBool(&_targeted);
    arc.ArchiveFloat(&_openPercentage);
    arc.ArchiveFloat(&_currentPercentage);
    arc.ArchiveFloat(&_totalMove);
    arc.ArchiveFloat(&_currentMove);
    arc.ArchiveFloat(&_baseSpeed);
    arc.ArchiveFloat(&_speed);
    arc.ArchiveVector(&_originalPos);
    arc.ArchiveVector(&_currentPos);
    arc.ArchiveVector(&_destinationPos);
    arc.ArchiveVector(&_moveDir);
}
