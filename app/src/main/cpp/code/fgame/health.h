

#pragma once

#include "g_local.h"
#include "item.h"
#include "sentient.h"

class Health : public Item
{
public:
    CLASS_PROTOTYPE(Health);

    Health();
    virtual void PickupHealth(Event *ev);
};

class HealthInventoryItem : public Item
{
public:
    CLASS_PROTOTYPE(HealthInventoryItem);

    virtual void processGameplayData(Event *ev) override;
    using Item::Use;
    void         Use(Event *ev);
};
