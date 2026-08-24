

#include "ammo.h"
#include "player.h"
#include "g_phys.h"

CLASS_DECLARATION(Item, AmmoEntity, NULL) {
    {NULL, NULL}
};

AmmoEntity::AmmoEntity()
{
    if (LoadingSavegame) {
        return;
    }
    setName("UnknownAmmo");
    amount = 0;

    _lastPrintTime = 0.0f;
}

Item *AmmoEntity::ItemPickup(Entity *other, qboolean add_to_inventory, qboolean checkautopickup)
{
    Sentient *player;
    str       realname;

    if (!other->isSubclassOf(Player)) {
        return NULL;
    }

    if (!Pickupable(other)) {
        return NULL;
    }

    player = (Sentient *)other;

    if (player->AmmoCount(item_name.c_str()) == player->MaxAmmoCount(item_name.c_str())) {
        if (level.time > _lastPrintTime + 1.0f) {
            _lastPrintTime = level.time;
            ((Player *)other)
                ->setItemText(getIcon(), va("$$CouldNotPickUp$$ $$Ammo-%s$$ $$FullAmmo$$", item_name.c_str()));
        }

        return NULL;
    }

    {
        realname = GetRandomAlias("snd_pickup");

        if (realname.length() > 1) {
            player->Sound(realname, CHAN_ITEM);
        }
    }

    CancelEventsOfType(EV_Item_DropToFloor);
    CancelEventsOfType(EV_Item_Respawn);
    CancelEventsOfType(EV_FadeOut);

    setSolidType(SOLID_NOT);
    if (_missingSkin) {
        ChangeSkin(_missingSkin, qtrue);
    } else {
        hideModel();
    }

    if (!Respawnable()) {
        PostEvent(EV_Remove, FRAMETIME);
    } else {
        PostEvent(EV_Item_Respawn, RespawnTime());
    }

    player->GiveAmmo(item_name, (int)amount, true);

    if (pickup_thread.length()) {
        ExecuteThread(pickup_thread);    
    }

    return NULL; 
}

void AmmoEntity::cacheStrings(void)
{

    G_FindConfigstringIndex(
        (char *)va("$$CouldNotPickUp$$ $$Ammo-%s$$ $$FullAmmo$$", item_name.c_str()),
        CS_GENERAL_STRINGS,
        MAX_GENERAL_STRINGS,
        qtrue
    );
    G_FindConfigstringIndex(
        (char *)va("$$PickedUp$$ %d $$Ammo-%s$$\n", (int)amount, item_name.c_str()),
        CS_GENERAL_STRINGS,
        MAX_GENERAL_STRINGS,
        qtrue
    );
}


CLASS_DECLARATION(Class, Ammo, NULL) {
    {NULL, NULL}
};

Ammo::Ammo()
    : amount(0)
    , maxamount(0)
    , name_index(0)
{
    if (LoadingSavegame) {
        return;
    }
    setName("UnknownAmmo");
    setAmount(0);
    setMaxAmount(100);
}

void Ammo::setAmount(int a)
{
    amount = a;

    if ((maxamount > 0) && (amount > maxamount)) {
        amount = maxamount;
    }
}

int Ammo::getAmount(void)
{
    return amount;
}

void Ammo::setMaxAmount(int a)
{
    maxamount = a;
}

int Ammo::getMaxAmount(void)
{
    return maxamount;
}

void Ammo::setName(str n)
{
    name       = n;
    name_index = gi.itemindex(name);
}

str Ammo::getName(void)
{
    return name;
}

int Ammo::getIndex(void)
{
    return name_index;
}
