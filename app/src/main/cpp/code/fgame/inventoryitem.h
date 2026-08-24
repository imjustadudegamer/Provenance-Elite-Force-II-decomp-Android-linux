

#pragma once

#include "item.h"

class InventoryItem : public Item
{
public:
    CLASS_PROTOTYPE(InventoryItem);

    InventoryItem();

    virtual void UseEvent(Event *ev);
};

extern Event EV_InventoryItem_Use;
