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
#include "item.h"

class AmmoEntity : public Item
{
private:
    float _lastPrintTime;

public:
    CLASS_PROTOTYPE(AmmoEntity);

    AmmoEntity();
    Item *ItemPickup(Entity *other, qboolean add_to_inventory, qboolean checkautopickup = qtrue) override;

    void cacheStrings(void) override;

    void Archive(Archiver& arc) override;
};

inline void AmmoEntity::Archive(Archiver& arc)
{
    Item::Archive(arc);

    arc.ArchiveFloat(&_lastPrintTime);
}

class Ammo : public Class
{
    int amount;
    int maxamount;
    str name;
    int name_index;

public:
    CLASS_PROTOTYPE(Ammo);

    Ammo();
    Ammo(str name, int amount, int name_index);

    void setAmount(int a);
    int  getAmount(void);
    void setMaxAmount(int a);
    int  getMaxAmount(void);
    void setName(str name);
    str  getName(void);
    int  getIndex(void);
    void Archive(Archiver& arc) override;
};

inline void Ammo::Archive(Archiver& arc)
{
    Class::Archive(arc);

    arc.ArchiveInteger(&amount);
    arc.ArchiveInteger(&maxamount);
    arc.ArchiveString(&name);
    // that retail never writes, desynchronising every field after it.  setName() below
    if (arc.Loading()) {
        setName(name);
    }
}
