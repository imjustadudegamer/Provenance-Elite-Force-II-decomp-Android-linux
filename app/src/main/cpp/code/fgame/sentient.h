

#pragma once

#include "g_local.h"
#include "../corepp/container.h"
#include "animate.h"
#include "characterstate.h"
// sentient.h never mentions one) -- the driver link is Player::vehicle (SDK player.h:246).

extern Event EV_Sentient_AddToMyArmor;
extern Event EV_Sentient_SetMyArmorAmount;
extern Event EV_Sentient_AddImmunity;
extern Event EV_Sentient_AddResistance;
extern Event EV_Sentient_RemoveResistance;
extern Event EV_Sentient_WeaponAnim;
extern Event EV_Sentient_SetDamageThreshold;
extern Event EV_Sentient_ClearDamageThreshold;
extern Event EV_Sentient_SetArmorActiveStatus;
extern Event EV_Sentient_SetArmorMultiplier;
extern Event EV_Armor_SetActiveStatus;
extern Event EV_Armor_SetMultiplier;
extern Event EV_Sentient_GiveInventoryItem;
extern Event EV_Sentient_GiveHealth;
extern Event EV_Sentient_GiveArmor;  
extern Event EV_Sentient_SetBloodModel;
extern Event EV_Sentient_SetCriticalHealthPercentage;  
extern Event EV_Sentient_SetViewMode;  
extern Event EV_Sentient_UselessCheck;
extern Event EV_Sentient_TurnOffShadow;
extern Event EV_Sentient_TurnOnShadow;
extern Event EV_Sentient_UpdateOffsetColor;
extern Event EV_Sentient_JumpXY;
extern Event EV_Sentient_MeleeAttackStart;
extern Event EV_Sentient_MeleeAttackEnd;
extern Event EV_Sentient_AddMeleeAttacker;
extern Event EV_Sentient_BlockStart;
extern Event EV_Sentient_BlockEnd;
extern Event EV_Sentient_SetMouthAngle;
extern Event EV_Sentient_SpawnBloodyGibs;
extern Event EV_Sentient_StopOnFire;
extern Event EV_Sentient_UseLastWeapon;
extern Event EV_Sentient_UseWeaponClass;
extern Event EV_Sentient_ToggleItemUse;
extern Event EV_Sentient_DontDropWeapons;
extern Event EV_Sentient_ForceDropWeapon;
extern Event EV_Sentient_ForceDropHealth;
extern Event EV_Sentient_GetForceDropHealth;
extern Event EV_Sentient_GetForceDropWeapon;

class Weapon;
class Item;
class Armor;
class InventoryItem;
class Ammo;
class Vehicle;

#define MAX_ACTIVE_WEAPONS     NUM_ACTIVE_WEAPONS

#define REGIONAL_DAMAGE_BACK   (1 << 0)
#define REGIONAL_DAMAGE_FRONT  (1 << 1)

#define THREATBIAS_IGNOREME    0xFFFFE4C7

typedef SafePtr<Weapon> WeaponPtr;
typedef SafePtr<Armor> ArmorPtr;   

typedef struct {
    int MODIndex;
    int ResistanceAmount;
} Resistance;

typedef struct {
    float duration;
    float startTime;
    float maxDamage;
    float currentDamage;
} damagethreshold_t;

class ActiveWeapon : public Class
{
public:
    WeaponPtr    weapon;
    weaponhand_t hand;
    ActiveWeapon();
    void Archive(Archiver& arc);
};

class Sentient : public Entity
{

    virtual const str getName(void) const override;  
private:
    float              _criticalHealthPercentage;
    damagethreshold_t  _damageThreshold;
    float              _hateModifier;

protected:
    Container<int>     inventory;
    Container<Ammo *>  ammo_inventory;

    WeaponPtr          newWeapon;
    ArmorPtr           currentBaseArmor;  
    Vector             offset_color;
    Vector             offset_delta;
    float              offset_time;       
    float              knock_start_time;
    str                blood_model;
    // on Actor, so once Actor::Archive became retail's -- which does NOT write it --
    int                last_surface_hit;
    int                last_bone_hit;
    unsigned int       last_region_hit;
    SafePtr<Weapon>    activeWeaponList[MAX_ACTIVE_WEAPONS];
    Container<int>          immunities;
    Container<Resistance *> resistances;

    virtual void EventTake(Event *ev);
    virtual void EventGiveAmmo(Event *ev);
    virtual void EventGiveItem(Event *ev);
    virtual void EventGiveArmor(Event *ev);
    virtual void AddResistance(Event *ev);
    virtual void RemoveResistance(Event *ev);
public:
    void         AddHealth(Event *ev);
    void         AddHealth(float healthToAdd, float maxHealth = 0.0f);  
protected:

    void HeadWatchAllowed(bool allowed);
    void HeadWatchAllowed(Event *ev);  
    void         SetBloodModel(Event *ev);
    virtual void EventGiveTargetname(Event *ev);

    void         SetHateModifier(Event *ev);
    void         SetHateModifier(float modifier);  

    void         SetCriticalHealthPercentage(float percentage);
    void         SetCriticalHealthPercentage(Event *ev);  
    float        GetCriticalHealthPercentage();

    // sentient.cpp:2661-2672).  The single writer of last_surface_hit / last_bone_hit /
    void             SetRegionalDamage(int surface_number, int bone_number, Vector direction);

    virtual void     ArmorDamage(Event *ev);
    void             ArmorDamage(::Damage &damage);  
    void             ArmorDamage(float damage, Entity *inflictor, Entity *attacker,
        const Vector &position, const Vector &direction, const Vector &normal,
        float knockback, int dflags, int meansofdeath, int surfaceNumber,
        int boneNumber, Entity *weapon, qboolean showPain);  
    virtual qboolean CanBlock(int meansofdeath, qboolean full_block);
    void             AddBloodSpurt(const Vector &position, const Vector &direction, int bone_number);  
    qboolean         ShouldBleed(int meansofdeath, qboolean dead);
    qboolean         ShouldGib(int meansofdeath, float damage);
    str              GetBloodSpurtName(void);
    str              GetBloodSplatName(void);
    float            GetBloodSplatSize(void);
    str              GetGibName(void);
    virtual void     TurnOffShadow(Event *ev);
    virtual void     TurnOnShadow(Event *ev);
    virtual void     WeaponKnockedFromHands(void);

    void         RangedAttackStart(Event *ev);  
    void         RangedAttackEnd(Event *ev);    
    void         StartChargeFire(Event *ev);    
    void         SwipeOn(Event *ev);            
    void         SwipeOff(Event *ev);           

    void         DropItemsOnDeath(void);      
    void         DropItem(Item *itemToDrop);  
    void         DetachAllActiveWeapons(void);
    void         AttachAllActiveWeapons(void);
    qboolean     IsActiveWeapon(const Weapon *weapon);  
    void         CheckAnimations(Event *ev);
    // The int-based FireWeapon/ReleaseFireWeapon overloads were port-only API: retail
    virtual void FireWeapon(Event *ev);  
    virtual void ReleaseFireWeapon(Event *ev);  

public:
    float GetHateModifier(void);    

    bool getHeadWatchAllowed(void) const { return _headWatchAllowed; }  

    void DeactivateWeapon(Weapon *weapon);
    void DeactivateWeapon(weaponhand_t hand);  
    void ActivateWeapon(Weapon *weapon, weaponhand_t hand);  

    Vector            gunoffset;
    Vector            eyeposition;
    int               viewheight;
    int               means_of_death;
    qboolean          in_melee_attack;
    qboolean          in_ranged_attack;
    qboolean          in_block;
    qboolean          in_stun;
    qboolean          on_fire;
    float             on_fire_stop_time;
    float             next_catch_on_fire_time;
    int               on_fire_tagnums[3];
    SafePtr<Entity>   fire_owner;
    bool              _displayFireEffect; // SDK sentient.h:144 -- gates fx_catchfire attach/remove (port-only; retail's flag lives on Actor)
    qboolean          attack_blocked;
    float             attack_blocked_time;
    float             max_mouth_angle;
    int               max_gibs;
    float             next_bleed_time;
    float shotsFiredThisVolley;  


    unsigned int _viewMode;
    bool         _canSendInjuredEvent;
    bool _headWatchAllowed;

    CLASS_PROTOTYPE(Sentient);

    Sentient();
    virtual ~Sentient() override;
    virtual Vector EyePosition(void);

    virtual Vector GunPosition(void);
    void           StopFireWeapon(Event *ev);
    bool           ChangeWeapon(Weapon *weapon, weaponhand_t hand);  
    Weapon        *GetActiveWeapon(weaponhand_t hand);  
    Weapon        *BestWeapon(Weapon *ignore = NULL);  
    Weapon        *NextWeapon(Weapon *weapon);
    Weapon        *PreviousWeapon(Weapon *weapon);
    virtual bool   useWeapon(const char *weaponname, weaponhand_t hand = WEAPON_DUAL);
    int            NumWeapons(void);
    int            AmmoCount(const str &ammo_type);  
    int            MaxAmmoCount(const str &ammo_type);  
    int            AmmoIndex(const str &ammo_type);  
    int            UseAmmo(const str &ammo_type, int amount);  
    // SDK sentient.h:288.  Returns the amount actually taken; `pickedUp` deliberately has
    int GiveAmmo(const str& type, int amount, bool pickedUp, int max_amount = -1);

    int GiveAmmo(const str& type, int amount, int max_amount) = delete;
    Ammo          *FindAmmoByName(const str &name);  
    Item          *giveItem(const str& itemname, int amount = 1, bool pickedUp = false, float skillLevel = 1.0f);  
    void           takeItem(const char *itemname);
    void           AddItem(const Item *object);  
    void           RemoveItem(Item *object);
    Item          *FindItemByClassName(const char *classname, Item *current = 0);  
    Item          *FindItemByModelname(const char *modelname, Item *current = 0);  
    Item          *FindItemByExternalName(const char *externalname, Item *current = 0);  
    Item          *FindItem(const char *itemname, Item *current = 0);  
    void           FreeInventory(void);
    void           FreeInventory(Event *ev);  
    qboolean       HasItem(const char *itemname);
    int            NumInventoryItems(void);
    Item          *NextItem(Item *item);
    Item          *PrevItem(Item *item);
    virtual void   DropInventoryItems(void);
    void           ListInventory(void);


    void             setModel(const char *mdl) override;
    void             Archive(Archiver& arc) override;
    // never reads it (SDK sentient.cpp:3289-3425); only Player does (SDK
    virtual void     ArchivePersistantData(Archiver& arc, qboolean sublevelTransition);
    Item           *FindBaseArmor(void);  
    void             ArmorEvent(Event *ev);  
    int              GetArmorValue(void);
    void             SetArmorValue(int armorVal);
    void             AddToMyArmor(Event *ev);  
    void             AddToMyArmor(float amountToAdd);
    void             SetMyArmorAmount(Event *ev);  
    void             SetMyArmorAmount(float amount);

    void SetArmorActiveStatus(Event *ev);
    void SetArmorActiveStatus(bool status);  
    void SetArmorMultiplier(Event *ev);
    void SetArmorMultiplier(float multiplier);  

    void SetDamageThreshold(Event *ev);
    void ClearDamageThreshold(Event *ev);
    void ClearDamageThreshold(void);  
    void CheckDamageThreshold(float damageValue);

    qboolean Resistant(int meansofdeath);
    int      GetResistanceModifier(int meansofdeath);
    virtual qboolean DoGib(int meansofdeath);  
    void             JumpXY(Event *ev);
    void             MeleeAttackStart(Event *ev);
    void             MeleeAttackEnd(Event *ev);
    void             SetMeleeAttack(bool value);
    void             BlockStart(Event *ev);
    void             BlockEnd(Event *ev);
    void             StunStart(Event *ev);
    void             StunEnd(Event *ev);
    void             SetAttackBlocked(qboolean blocked);  
    virtual void     ReceivedItem(Item *item);
    virtual void     RemovedItem(Item *item);
    virtual void     AmmoAmountChanged(Ammo *ammo, int inclip = 0);
    void             AmmoAmountInClipChanged(const str &ammo_type, int amount);  

    void         SetMaxMouthAngle(Event *ev);
    void         TryLightOnFire(int meansofdeath, Entity *attacker);
    void         OnFire(Event *ev);
    void         StopOnFire(Event *ev);
    void         SpawnBloodyGibs(Event *ev);
    void         SetMaxGibs(Event *ev);
    virtual void GetStateAnims(Container<const char *> *c);



    // SetVehicle was also a latent trap: its body was `m_pVehicle = NULL;`, ignoring its


    void RemoveImmunity(Event *ev);
    void SetHealth(float newHealth);
    void getActiveWeaponName(Event *ev);
    void getActiveWeaponName(weaponhand_t hand, str& weaponName);  
    void setViewMode(Event *ev);
    // (player.h:626, `virtual`) therefore did NOT override it -- it was a fresh
    // (`client->ps.viewMode = getViewMode();`, SDK player.cpp:12015-12019) -- NEVER RAN.
    virtual void setViewMode(const str &viewModeName);  
    unsigned int getViewMode(void);  
    void AddHealthAtInterval(Event *ev);
    void AddHealthOverTime(Event *ev);
    void AddImmunity(Event *ev);
    void BeginAttack(Event *ev);
    void CatchOnFire(Event *ev);
    void DisplayFireEffect(Event *ev);
    void DropItemEvent(Event *ev);
    void EndAttack(Event *ev);
    void EventGiveHealth(Event *ev);
    void SetOffsetColor(Event *ev);
    void SetOffsetColor(float r, float g, float b, float time);  
    void SetStateFile(Event *ev);
    void SetWeaponAnim(Event *ev);
    void UpdateOffsetColor(Event *ev);
    void WeaponEffectsAndSound(Entity *weapon, const str& objname, Vector pos);  
    void cacheStateMachineAnims(Event *ev);
    void freeConditionals(Container<Conditional *> &conditionalsToDelete);  
    void giveAmmoOverTime(Event *ev);
    qboolean Immune(int meansofdeath);  
     Sentient(Event *ev);
};

inline void Sentient::SetCriticalHealthPercentage(float percentage)
{
    _criticalHealthPercentage = percentage;
}

inline void Sentient::SetCriticalHealthPercentage(Event *ev)
{
    SetCriticalHealthPercentage(ev->GetFloat(1));
}

inline float Sentient::GetCriticalHealthPercentage()
{
    return _criticalHealthPercentage;
}

typedef SafePtr<Sentient> SentientPtr;

extern Container<Sentient *> SentientList;
extern Container<Sentient *> TeamMateList;    
