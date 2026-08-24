

#include "inventoryitem.h"
#include "level.h"

Event EV_InventoryItem_Use (	"useinvitem",
	EV_CODEONLY,
	NULL,
	NULL,
	"Use this inventory item."
);



CLASS_DECLARATION(Item, InventoryItem, NULL) {
    {&EV_InventoryItem_Use, &InventoryItem::UseEvent},
    {NULL,                  NULL                    }
};

InventoryItem::InventoryItem()
{
    if (LoadingSavegame) {
        return;
    }
}


void InventoryItem::UseEvent(Event *ev) {}
