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
#include "mover.h"
#include "../corepp/script.h"
#include "scriptmaster.h"
#include "misc.h"
#include "bspline.h"


class ScriptSlave : public Trigger
{
private:
    bool _handlesDamage; 

    SafePtr<CThread> touchthread;   
    SafePtr<CThread> blockthread;   
    SafePtr<CThread> triggerthread; 
    SafePtr<CThread> usethread;     
    SafePtr<CThread> damagethread;  

    str touchlabel;                 
    str uselabel;                   
    str blocklabel;                 
    str triggerlabel;               
    str damagelabel;                

protected:
    float attack_finished;

    float dmg;  
    int   dmg_means_of_death;

    qboolean thinking;

    SafePtr<CThread> movethread;

    float     _forcefieldNumber;
    float     _triggerNumber;
    EntityPtr _scanner;

    // portal on a real state change.  Declared `bool` (not qboolean) because retail
    bool _portalOpen;

    float _nextNeedToUseTime;

    str            _damageEffect;
    str            _bloodModel;
    Container<str> _requiredMODlist;               
    bool           _canBeAttackedByOtherScriptSlaves;

public:
    qboolean commandswaiting;

    Vector TotalRotation;
    Vector _newAngles;
    Vector _newPosition;
    const Vector &GetNewPosition(void) const { return _newPosition; }
    void SetNewPosition(const Vector &newPosition) { _newPosition = newPosition; }
    const Vector &GetNewAngles(void) const { return _newAngles; }
    void SetNewAngles(const Vector &newAngles) { _newAngles = newAngles; }
    Vector ForwardDir;
    float  speed;

    Waypoint *waypoint;
    float     traveltime;
    BSpline  *splinePath;
    float     splineTime;

    SplinePathPtr currentNode;
    int           lastTime;
    int           newTime;

    qboolean splineangles;
    qboolean ignoreangles;
    qboolean moving; 

protected:

    CLASS_PROTOTYPE(ScriptSlave);

    ScriptSlave();
    ~ScriptSlave();

    void BindEvent(Event *ev);
    void EventUnbind(Event *ev);
    void NewOrders(Event *ev);
    void StopEvent(Event *ev);
    void ContinueEvent(Event *ev);
    void MoveEnd(Event *ev);
    void SetAnglesEvent(Event *ev);
    void SetAngleEvent(Event *ev);  
    void SetModelEvent(Event *ev);
    void TriggerEvent(Event *ev) override;
    void GotoNextWaypoint(Event *ev);
    void JumpTo(Event *ev);
    void MoveToEvent(Event *ev);
    void MoveToPositionEvent(Event *ev);
    void SetSpeed(Event *ev);
    void SetTime(Event *ev);
    void MoveUp(Event *ev);
    void MoveDown(Event *ev);
    void MoveNorth(Event *ev);
    void MoveSouth(Event *ev);
    void MoveEast(Event *ev);
    void MoveWest(Event *ev);
    void MoveForward(Event *ev);
    void MoveBackward(Event *ev);
    void MoveLeft(Event *ev);
    void MoveRight(Event *ev);
    void RotateXdownto(Event *ev);
    void RotateYdownto(Event *ev);
    void RotateZdownto(Event *ev);
    void RotateAxisdownto(Event *ev);
    void RotateXupto(Event *ev);
    void RotateYupto(Event *ev);
    void RotateZupto(Event *ev);
    void RotateAxisupto(Event *ev);
    void Rotateupto(Event *ev);
    void Rotatedownto(Event *ev);
    void Rotateto(Event *ev);
    void RotateXdown(Event *ev);
    void RotateYdown(Event *ev);
    void RotateZdown(Event *ev);
    void RotateAxisdown(Event *ev);
    void RotateXup(Event *ev);
    void RotateYup(Event *ev);
    void RotateZup(Event *ev);
    void RotateAxisup(Event *ev);
    void RotateX(Event *ev);
    void RotateY(Event *ev);
    void RotateZ(Event *ev);
    void RotateAxis(Event *ev);
    void TouchFunc(Event *ev);
    void BlockFunc(Event *ev);
    void TriggerFunc(Event *ev);
    void UseFunc(Event *ev);
    void DamageFunc(Event *ev);
    void NoDamage(Event *ev);
    void SetDamage(Event *ev);
    void SetMeansOfDeath(Event *ev);
    void FollowPath(Event *ev);
    void EndPath(Event *ev);
    void FollowingPath(Event *ev);
    void CreatePath(SplinePath *path, splinetype_t type);
    void Explode(Event *ev);
    void NotShootable(Event *ev);
    void OpenPortal(Event *ev);
    void ClosePortal(Event *ev);
    void PhysicsOn(Event *ev);
    void PhysicsOff(Event *ev);
    void PhysicsVelocity(Event *ev);

    void Think(void) override;  
    virtual void setOrigin(const Vector &point) override;  
    virtual void setOrigin(void) override;  
    void Archive(Archiver& arc) override;







public:
    void Continue( void ); 
    void DoMove( Event *ev ); 
    void Hack_AddParms( Event *ev ); 
    void Hack_GetForceFieldNumber( Event *ev ); 
    void Hack_GetScanner( Event *ev ); 
    void Hack_GetTriggerNumber( Event *ev ); 
    void HandlesDamage( Event *ev ); 
    void NoBlock( Event *ev ); 
    void NoTouch( Event *ev ); 
    void NoTrigger( Event *ev ); 
    void NoUse( Event *ev ); 
    void OnBlock( Event *ev ); 
    void OnDamage( Event *ev ); 
    void OnTouch( Event *ev ); 
    void OnTrigger( Event *ev ); 
    void OnUse( Event *ev ); 
    void Stop( void ); 
    void addRequiredDamageMOD( Event *ev ); 
    void setBloodModel( Event *ev ); 
    void setCanBeAttackedByOtherSlaves( Event *ev ); 
    void setDamage( Event *ev ); 
    void setDamageEffect( Event *ev ); 
};

inline void ScriptSlave::Archive(Archiver& arc)
{
    int tempInt;
    int num = 0;

    // moveflags between Trigger and ScriptSlave -- bytes retail never writes there.
    Trigger::Archive(arc);

    arc.ArchiveVector(&_newAngles);
    arc.ArchiveVector(&_newPosition);

    arc.ArchiveBool(&_handlesDamage);

    arc.ArchiveSafePointer(&touchthread);
    arc.ArchiveSafePointer(&blockthread);
    arc.ArchiveSafePointer(&triggerthread);
    arc.ArchiveSafePointer(&usethread);
    arc.ArchiveSafePointer(&damagethread);
    arc.ArchiveSafePointer(&movethread);

    arc.ArchiveString(&touchlabel);
    arc.ArchiveString(&uselabel);
    arc.ArchiveString(&blocklabel);
    arc.ArchiveString(&triggerlabel);
    arc.ArchiveString(&damagelabel);

    arc.ArchiveFloat(&attack_finished);
    arc.ArchiveFloat(&dmg);
    arc.ArchiveInteger(&dmg_means_of_death);
    arc.ArchiveBoolean(&thinking);

    arc.ArchiveFloat(&_forcefieldNumber);
    arc.ArchiveFloat(&_triggerNumber);
    arc.ArchiveSafePointer(&_scanner);

    arc.ArchiveBool(&_portalOpen);

    arc.ArchiveString(&_damageEffect);
    arc.ArchiveString(&_bloodModel);

    if (arc.Saving()) {
        num = _requiredMODlist.NumObjects();
        arc.ArchiveInteger(&num);
        for (int i = 1; i <= num; i++) {
            arc.ArchiveString(&_requiredMODlist.ObjectAt(i));
        }
    } else {
        arc.ArchiveInteger(&num);

        if (num < 0 || num > 4096) {
            arc.FileError("ScriptSlave '%s': _requiredMODlist count %d is out of range", getClassname(), num);
            return;
        }

        _requiredMODlist.ClearObjectList();
        for (int i = 0; i < num; i++) {
            str s;
            arc.ArchiveString(&s);
            _requiredMODlist.AddObject(s);
        }
    }

    arc.ArchiveFloat(&_nextNeedToUseTime);

    arc.ArchiveBoolean(&commandswaiting);
    arc.ArchiveVector(&TotalRotation);
    arc.ArchiveVector(&ForwardDir);
    arc.ArchiveFloat(&speed);
    arc.ArchiveObjectPointer((Class **)&waypoint);
    arc.ArchiveFloat(&traveltime);

    if (arc.Saving()) {
        if (splinePath) {
            tempInt = ARCHIVE_POINTER_VALID;
        } else {
            tempInt = ARCHIVE_POINTER_NULL;
        }
        arc.ArchiveInteger(&tempInt);
        if (tempInt == ARCHIVE_POINTER_VALID) {
            splinePath->Archive(arc);
        }
    } else {
        arc.ArchiveInteger(&tempInt);
        if (tempInt == ARCHIVE_POINTER_VALID) {
            splinePath = new BSpline;
            splinePath->Archive(arc);
        } else {
            splinePath = NULL;
        }
    }
    arc.ArchiveFloat(&splineTime);

    arc.ArchiveSafePointer(&currentNode);
    arc.ArchiveInteger(&lastTime);
    arc.ArchiveInteger(&newTime);

    arc.ArchiveBoolean(&splineangles);
    arc.ArchiveBoolean(&ignoreangles);
    arc.ArchiveBoolean(&moving);

    arc.ArchiveBool(&_canBeAttackedByOtherScriptSlaves);
}

class ScriptModel : public ScriptSlave
{
private:
    void GibEvent(Event *ev);

    qboolean animationDriven;

public:
    CLASS_PROTOTYPE(ScriptModel);

    ScriptModel();
    void SetAnimEvent(Event *ev);
    void SetAnimDrivenEvent(Event *ev);  

    void Archive(Archiver& arc) override;
    void Stop(Event *ev);
    void Think(void) override;  
    using ScriptSlave::setOrigin;
    void setOrigin(Event *ev);






public:
    void AnimOnceEvent( Event *ev ); 
    void SetModelEvent( Event *ev ); 
    void SetAngleEvent( Event *ev ); 
};

inline void ScriptModel::Archive(Archiver& arc)
{
    ScriptSlave::Archive(arc);

    arc.ArchiveBoolean(&animationDriven);
}

class ScriptOrigin : public ScriptSlave
{
public:
    CLASS_PROTOTYPE(ScriptOrigin);
    ScriptOrigin();

};

class ScriptSkyOrigin : public ScriptSlave
{
public:
    qboolean use_base_position;
    Vector   base_position;
    qboolean use_player_base_position;
    Vector   player_base_position;
    float    translation_multiplier;
    float    max_distance;

    CLASS_PROTOTYPE(ScriptSkyOrigin);

    ScriptSkyOrigin();

    void Think(void) override;                     
    void SetBasePosition(Event *ev);               
    void SetPlayerBasePosition(Event *ev);         
    void SetTranslationMultiplier(Event *ev);      
    void SetMaxDistance(Event *ev);                

    void Archive(Archiver& arc) override;
};

inline void ScriptSkyOrigin::Archive(Archiver& arc)
{
    ScriptSlave::Archive(arc);

    arc.ArchiveBoolean(&use_base_position);
    arc.ArchiveVector(&base_position);
    arc.ArchiveBoolean(&use_player_base_position);
    arc.ArchiveVector(&player_base_position);
    arc.ArchiveFloat(&translation_multiplier);
    arc.ArchiveFloat(&max_distance);
}

