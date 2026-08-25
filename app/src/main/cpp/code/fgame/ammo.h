

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
