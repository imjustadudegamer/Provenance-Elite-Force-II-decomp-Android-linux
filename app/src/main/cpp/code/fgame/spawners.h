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


#pragma once

#include "g_local.h"
#include "scriptslave.h"

class SpawnArgs;

class Spawn : public ScriptSlave
{
private:
    Container<str> _modelNames;
    str            _spawnTargetName;
    str            _spawnTarget;
    str            _pickupThread;
    str            _spawnItem;
    float          _spawnChance;
    int            _attackMode;
    qboolean       _startHidden;
    str            _effectType;
    str            _effectName;
    str            _animName;
    Vector         _velocity;
    int            _spawnGroupID;
    str            _masterStateMap;
    str            _spawnGroupDeathThread;
    bool           _checkForSpace;

    Container<str> _keys;
    Container<str> _values;

    void SetAngleEvent(Event *ev);
    void SetPickupThread(Event *ev);
    void ModelName(Event *ev);
    void SpawnTargetName(Event *ev);
    void SpawnTarget(Event *ev);
    void AttackMode(Event *ev);
    void SetSpawnItem(Event *ev);
    void SetSpawnChance(Event *ev);
    void SetStartHidden(Event *ev);
    void SetSpawnEffect(Event *ev);
    void SetAnimName(Event *ev);
    void SpawnNow(Event *ev);
    void SetSpawnVelocity(Event *ev);
    void SetSpawnGroupID(Event *ev);
    void SetSpawnMasterStateMap(Event *ev);
    void SetSpawnGroupDeathThread(Event *ev);
    void setCheckForSpace(Event *ev);

    void setSpawnKeyValue(Event *ev);
    void clearSpawnKeyValues(Event *ev);

protected:
    virtual ~Spawn() override;
    void         SetArgs(SpawnArgs& args);
    virtual void DoSpawn(Event *ev);
    void         postSpawn(Entity *spawn);
    bool         checkStuck(Entity *spawn);

public:
    CLASS_PROTOTYPE(Spawn);

    Spawn();
    void Archive(Archiver& arc) override;
};

inline void Spawn::Archive(Archiver& arc)
{
    ScriptSlave::Archive(arc);

    _modelNames.Archive(arc);

    arc.ArchiveString(&_spawnTargetName);
    arc.ArchiveString(&_spawnTarget);
    arc.ArchiveString(&_pickupThread);
    arc.ArchiveString(&_spawnItem);
    arc.ArchiveFloat(&_spawnChance);
    arc.ArchiveInteger(&_attackMode);
    arc.ArchiveBoolean(&_startHidden);
    arc.ArchiveString(&_effectType);
    arc.ArchiveString(&_effectName);
    arc.ArchiveString(&_animName);
    arc.ArchiveVector(&_velocity);
    arc.ArchiveInteger(&_spawnGroupID);
    arc.ArchiveString(&_masterStateMap);
    arc.ArchiveString(&_spawnGroupDeathThread);
    arc.ArchiveBool(&_checkForSpace);

    _keys.Archive(arc);
    _values.Archive(arc);
}

class RandomSpawn : public Spawn
{
private:
    float min_time;
    float max_time;

    void MinTime(Event *ev);
    void MaxTime(Event *ev);
    void ToggleSpawn(Event *ev);
    void Think(Event *ev);

public:
    using Entity::Think;

    CLASS_PROTOTYPE(RandomSpawn);

    RandomSpawn();
    void Archive(Archiver& arc) override;
};

inline void RandomSpawn::Archive(Archiver& arc)
{
    Spawn::Archive(arc);

    arc.ArchiveFloat(&min_time);
    arc.ArchiveFloat(&max_time);
}

class ReSpawn : public Spawn
{
protected:
    void DoSpawn(Event *ev) override;

public:
    CLASS_PROTOTYPE(ReSpawn);
};

class SpawnOutOfSight : public Spawn
{
private:
    bool checkFOV;

protected:
    void DoSpawn(Event *ev) override;
    void CheckFOV(Event *ev);

public:
    CLASS_PROTOTYPE(SpawnOutOfSight);

    SpawnOutOfSight();
    void Archive(Archiver& arc) override;
};

inline void SpawnOutOfSight::Archive(Archiver& arc)
{
    Spawn::Archive(arc);
    arc.ArchiveBool(&checkFOV);
}

class SpawnChain : public Spawn
{
private:
    bool use3rdPersonCamera;

protected:
    void DoSpawn(Event *ev) override;

public:
    CLASS_PROTOTYPE(SpawnChain);

    SpawnChain(void);
    void Use3rdPersonCamera(Event *ev);
    void Archive(Archiver& arc) override;
};

inline void SpawnChain::Archive(Archiver& arc)
{
    Spawn::Archive(arc);

    arc.ArchiveBool(&use3rdPersonCamera);
}
