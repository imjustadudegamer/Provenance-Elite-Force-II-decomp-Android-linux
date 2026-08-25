

#include "g_local.h"
#include "scriptvariable.h"
#include "armor.h"
#include "level.h"      
#include "gamecvars.h"  
#include "sentient.h"   
#include "player.h"     
#include "actor.h"      
#include "game.h"       
#include "mp_manager.hpp"  

Event EV_Armor_SetAdaptionFX (	"setadaptionfx",
	EV_DEFAULT,
	"s",
	"adaption_fx",
	"set the adaption fx model"
);


CLASS_DECLARATION(Item, Armor, NULL) {
    {NULL, NULL}
};

Armor::Armor()
{
    if (LoadingSavegame) {
        return;
    }

    // SDK armor.cpp:114 -- retail's switch is the mp_flags bit, not MOHAA's dmflags.
    if (multiplayerManager.checkFlag(MP_FLAG_NO_ARMOR)) {
        PostEvent(EV_Remove, EV_REMOVE);
        return;
    }

    setName("UnknownArmor");
    amount = 0;
    SetMax(200);  

}

Armor::~Armor() {}

Item *Armor::ItemPickup(Entity *other, qboolean add_to_inventory, qboolean checkautopickup)
{
    Sentient *sent;

    if (!other->isSubclassOf(Player)) {
        return NULL;
    }

    if (!Pickupable(other)) {
        return NULL;
    }

    sent = (Sentient *)other;

    if (!_needThisArmor(sent)) {
        return NULL;
    }

    _pickupArmor(sent);

    Event *armorEvent = new Event(EV_Sentient_GiveArmor);
    armorEvent->AddString(item_name.c_str());
    armorEvent->AddFloat(amount);
    armorEvent->AddInteger(true);

    sent->ProcessEvent(armorEvent);

    return NULL; 
}

qboolean Armor::_needThisArmor(Sentient *sentient)
{
    int currentArmorValue = sentient->GetArmorValue();

    if (currentArmorValue >= amount) {
        return qfalse;
    }

    return qtrue;
}

void Armor::_pickupArmor(Sentient *sentient)
{
    str realname;

    realname = GetRandomAlias("snd_pickup");
    if (realname.length() > 1) {
        sentient->Sound(realname, CHAN_ITEM);
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

}

float Armor::ResolveDamage(float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker)
{
    gi.Error(ERR_FATAL, "Armor::ResolveDamage -- Do Not Use Armor Base Class -- You MUST use a subclassed");
    return 0.0f;
}

bool Armor::CanBeDamagedBy(int meansOfDeath)
{
    gi.Error(ERR_FATAL, "Armor::CanBeDamagedBy -- Do Not Use Armor Base Class -- You MUST use a subclassed");
    return true;
}

const float BasicArmor::_maxAmount       = 200.0f;
const float BasicArmor::_normalMaxAmount = 100.0f;
const float BasicArmor::_lossRate        = 1.0f;

const float BasicArmor::_highRangeCutoffSinglePlayer = 66.0f;
const float BasicArmor::_midRangeCutoffSinglePlayer  = 33.0f;

const float BasicArmor::_highRangeCutoffMultiplayer = 100.0f;
const float BasicArmor::_midRangeCutoffMultiplayer  = 50.0f;

const float BasicArmor::_highRangePercent = 0.75f;
const float BasicArmor::_midRangePercent  = 0.50f;
const float BasicArmor::_lowRangePercent  = 0.25f;

CLASS_DECLARATION(Armor, BasicArmor, NULL) {
    {NULL, NULL}
};

BasicArmor::BasicArmor()
{
    maximum_amount = _maxAmount;
    setName("BasicArmor");

    flags |= FL_THINK;

    _lastAddTime = 0.0f;
}

void BasicArmor::Add(int num)
{
    Armor::Add(num);

    _lastAddTime = level.time;
}

void BasicArmor::cacheStrings(void)
{

    G_FindConfigstringIndex(
        (char *)va("$$PickedUp$$ %d $$Item-Armor$$", (int)amount), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, qtrue
    );
}

qboolean BasicArmor::_needThisArmor(Sentient *sentient)
{
    int currentArmorValue = sentient->GetArmorValue();

    if (currentArmorValue < _maxAmount) {
        return qtrue;
    }

    return qfalse;
}

float BasicArmor::ResolveDamage(float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker)
{
    float tempDamage;
    float damageAbsorbed;
    float percent;
    float highRangeCutoff;
    float midRangeCutoff;

    (void)direction;
    (void)position;
    (void)attacker;

    if ((meansOfDeath == MOD_ARMOR_PIERCING) || (meansOfDeath == MOD_IMOD_PRIMARY)
        || (meansOfDeath == MOD_IMOD_SECONDARY) || (meansOfDeath == MOD_DEATH_QUAD)) {
        return damage;
    }

    if (g_gametype->integer != GT_SINGLE_PLAYER) {
        highRangeCutoff = _highRangeCutoffMultiplayer;
        midRangeCutoff  = _midRangeCutoffMultiplayer;
    } else {
        highRangeCutoff = _highRangeCutoffSinglePlayer;
        midRangeCutoff  = _midRangeCutoffSinglePlayer;
    }

    if (amount > highRangeCutoff) {
        percent = _highRangePercent;
    } else if (amount > midRangeCutoff) {
        percent = _midRangePercent;
    } else {
        percent = _lowRangePercent;
    }

    tempDamage = damage * percent;

    if (amount > tempDamage) {
        damageAbsorbed = tempDamage;
    } else {
        damageAbsorbed = amount;
    }

    amount -= damageAbsorbed;
    if (amount < 0.0) {
        amount = 0.0;
    }

    return damage - damageAbsorbed;
}

bool BasicArmor::CanBeDamagedBy(int meansOfDeath)
{
    return true;
}

void BasicArmor::Think(void)
{
    if ((level.time > _lastAddTime + 1.0f) && (amount > _normalMaxAmount)) {
        amount -= level.frametime * _lossRate;

        if (amount < _normalMaxAmount) {
            amount = _normalMaxAmount;
        }
    }
}


Container<MODHitCounter *> AdaptiveArmor::_AdaptionList;
Container<int>             AdaptiveArmor::_CannotAdaptToList;
Container<str>             AdaptiveArmor::_fxList;

Event EV_Armor_SetFXLife (    "setFXlife",
    EV_DEFAULT,
    "ff",
    "Normal_Time Explosive_Time",
    "Sets the FX Time");


Event EV_Armor_LoadAdaptionDataFromGameVars (    "loadadaptiondata",
    EV_DEFAULT,
    NULL,
    NULL,
    "Used to control timing on adaption information loads");

CLASS_DECLARATION(Armor, AdaptiveArmor, NULL) {
    {&EV_Armor_SetAdaptionFX,                &AdaptiveArmor::SetAdaptionFX       },
    {&EV_Armor_LoadAdaptionDataFromGameVars, &AdaptiveArmor::LoadDataFromGameVars},
    {&EV_Armor_SetFXLife,                    &AdaptiveArmor::SetFXLife           },
    {NULL,                                   NULL                                }
};


void AdaptiveArmor::SetFXLife(Event *ev)
{
    _fxTimeNormal    = ev->GetFloat(1);
    _fxTimeExplosive = ev->GetFloat(2);
}

AdaptiveArmor::AdaptiveArmor()
{
    amount = 0;
    SetMax(200);

    _fxList.AddObject("fx/fx-borg-sheild-01.tik");
    _fxList.AddObject("fx/fx-borg-sheild-02.tik");
    _fxList.AddObject("fx/fx-borg-sheild-03.tik");

    _AdaptionFX = "fx/fx-borg-sheild-01.tik";

    _currentFXIndex  = 1;
    _fxTimeNormal    = 0.15f;
    _fxTimeExplosive = 1.0f;

    _fxTime        = 0.0f;
    _adaptionLimit = 0.0f;

    // The one thing the Borg can NEVER adapt to: the I-MOD (it re-modulates every
    _AddMODToCannotAdaptList(MOD_IMOD_PRIMARY);
    _AddMODToCannotAdaptList(MOD_IMOD_SECONDARY);
    _AddMODToCannotAdaptList(MOD_SUICIDE);

    if (!LoadingSavegame) {
        Event *loadEvent = new Event(EV_Armor_LoadAdaptionDataFromGameVars);

        PostEvent(loadEvent, 3.0f);
    }
}

AdaptiveArmor::~AdaptiveArmor() {}

void AdaptiveArmor::SetAdaptionFX(Event *ev)
{
    SetAdaptionFX(ev->GetString(1));
}

void AdaptiveArmor::SetAdaptionFX(const str& fxName)
{
    _AdaptionFX = fxName;
}

float AdaptiveArmor::ResolveDamage(
    float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker
)
{
    if (!_CanAdaptTo(meansOfDeath)) {
        return damage; 
    }

    if (attacker && attacker->isSubclassOf(Player)) {
        if (!_AdaptionInList(meansOfDeath)) {
            _AddAdaption(meansOfDeath, damage);
        } else {
            _UpdateAdaption(meansOfDeath, damage);
        }
    }

    int damageTotal = (int)_GetDamageTotal(meansOfDeath);

    _adaptionLimit = g_armoradaptionlimit->value;
    if (damageTotal > _adaptionLimit) {
        if (meansOfDeath == MOD_EXPLOSION || meansOfDeath == MOD_VAPORIZE_COMP) {
            _fxTime = _fxTimeExplosive;
        } else {
            _fxTime = _fxTimeNormal;
        }
        _PlayAdaptionFX(direction, position);
        return 0.0f;
    }

    return damage;
}

// DELIBERATE DEVIATION: retail allocates the MODHitCounter at SDK armor.cpp:440-441,
void AdaptiveArmor::_AddAdaption(int meansOfDeath, float damage)
{
    if (_AdaptionList.NumObjects() > 2) {
        return;
    }
    if (_AdaptionInList(meansOfDeath)) {
        return;
    }

    MODHitCounter *adaption = new MODHitCounter;
    adaption->MODIndex      = meansOfDeath;
    adaption->damage        = damage + 0.1f; 
    adaption->adapted       = false;

    str MODName = MOD_NumToName(meansOfDeath);
    AddGameVar(MODName, adaption->damage);

    _AdaptionList.AddObject(adaption);
}

qboolean AdaptiveArmor::_AdaptionInList(int meansOfDeath)
{
    for (int i = _AdaptionList.NumObjects(); i > 0; i--) {
        if (_AdaptionList.ObjectAt(i)->MODIndex == meansOfDeath) {
            return qtrue;
        }
    }
    return qfalse;
}

void AdaptiveArmor::_UpdateAdaption(int meansOfDeath, float damage)
{
    for (int i = _AdaptionList.NumObjects(); i > 0; i--) {
        MODHitCounter *adaption = _AdaptionList.ObjectAt(i);
        if (adaption->MODIndex == meansOfDeath) {
            adaption->damage += damage;

            const str MODName = MOD_NumToName(meansOfDeath);
            UpdateGameVar(MODName, adaption->damage);

            if ((adaption->damage >= g_armoradaptionlimit->value) && !adaption->adapted) {
                adaption->adapted = true;

                Actor *act;
                for (int i2 = 1; i2 <= ActiveList.NumObjects(); i2++) {
                    act = ActiveList.ObjectAt(i2);
                    if (!act) {
                        continue;
                    }

                    act->InContext("enemyadapted", 0);
                }
            }
            return;
        }
    }
}

float AdaptiveArmor::_GetDamageTotal(int meansOfDeath)
{
    for (int i = _AdaptionList.NumObjects(); i > 0; i--) {
        MODHitCounter *adaption = _AdaptionList.ObjectAt(i);
        if (adaption->MODIndex == meansOfDeath && adaption->damage > 0) {
            return adaption->damage;
        }
    }
    return 0.0f;
}

void AdaptiveArmor::_PlayAdaptionFX(const Vector& direction, const Vector& position)
{
    Entity *theFX;

    Vector dir = direction;
    dir *= -1.0f;
    dir.normalize();

    Vector fxOrigin = position + dir * 15.0f;

    dir = dir.toAngles();

    Vector posDelta = _currentFXPosition - fxOrigin;
    if (posDelta.length() > FX_CHANGE_DELTA) {
        _changeFX();
        _currentFXPosition = fxOrigin;
    }

    theFX = SpawnEffect(_AdaptionFX, fxOrigin, dir, _fxTime);

    if (!theFX) {
        return;
    }

    if (owner) {
        theFX->bind(owner);
    }

    theFX->PostEvent(EV_SetOriginEveryFrame, FRAMETIME);
}

qboolean AdaptiveArmor::_CanAdaptTo(int mod)
{
    for (int i = _CannotAdaptToList.NumObjects(); i > 0; i--) {
        if (_CannotAdaptToList.ObjectAt(i) == mod) {
            return qfalse;
        }
    }
    return qtrue;
}

void AdaptiveArmor::_AddMODToCannotAdaptList(int mod)
{
    _CannotAdaptToList.AddObject(mod);
}

void AdaptiveArmor::_changeFX()
{
    int maxNumber = _fxList.NumObjects();
    if (maxNumber == 0) {
        return;
    }
    _currentFXIndex++;
    if (_currentFXIndex > maxNumber) {
        _currentFXIndex = 1;
    }
    _AdaptionFX = _fxList.ObjectAt(_currentFXIndex);
}

bool AdaptiveArmor::CanBeDamagedBy(int meansOfDeath)
{
    float DamageTotal;

    if (!_AdaptionInList(meansOfDeath)) {
        return true;
    }

    DamageTotal = _GetDamageTotal(meansOfDeath);
    if (DamageTotal > ADAPTION_LIMIT) {
        return false;
    }

    return true;
}

void AdaptiveArmor::ClearAdaptionList()
{
    for (int i = _AdaptionList.NumObjects(); i > 0; i--) {
        delete _AdaptionList.ObjectAt(i);
    }
    _AdaptionList.FreeObjectList();
}

void AdaptiveArmor::AddGameVar(const str& name, float value)
{
    if (gameVars.VariableExists(name.c_str())) {
        return;
    }

    gameVars.SetVariable(name.c_str(), value);
}

void AdaptiveArmor::UpdateGameVar(const str& name, float value)
{
    ScriptVariable     *script_var;
    float               currentValue;

    if (!gameVars.VariableExists(name.c_str())) {
        return;
    }

    script_var   = gameVars.GetVariable(name.c_str());
    currentValue = script_var->floatValue();
    currentValue = currentValue + value;
    script_var->setFloatValue(currentValue);
}

void AdaptiveArmor::LoadAdaptionData(const str& name)
{
    ScriptVariable     *script_var;
    float               lastValue;
    unsigned int        theMOD;

    theMOD = MOD_NameToNum(name);

    if (!gameVars.VariableExists(name.c_str())) {
        return;
    }

    script_var = gameVars.GetVariable(name.c_str());
    lastValue  = script_var->floatValue();

    _AddAdaption(theMOD, lastValue);

    if (lastValue >= g_armoradaptionlimit->value) {
        for (int i = _AdaptionList.NumObjects(); i > 0; i--) {
            MODHitCounter *adaption = _AdaptionList.ObjectAt(i);
            if (adaption->MODIndex == ( int )theMOD) {
                adaption->adapted = true;
            }
        }
    }
}

void AdaptiveArmor::LoadDataFromGameVars(Event *ev)
{
    LoadDataFromGameVars();
}

void AdaptiveArmor::LoadDataFromGameVars(void)
{
    LoadAdaptionData("phaser");
    LoadAdaptionData("vaporize");
    LoadAdaptionData("comp_rifle");
    LoadAdaptionData("vaporize_comp");
}


Event EV_Armor_AddToNoProtectionList (    "noprotection",
    EV_DEFAULT,
    "s",
    "MOD",
    "Adds a MOD to the No Protection List");

Event EV_Armor_UseArmorDirection (    "usearmordirection",
    EV_DEFAULT,
    "ff",
    "directionAngleMin directionAngleMax",
    "Only allows armor to work if attack is within the specifed yaw range");

Event EV_Armor_SetActiveStatus (    "setarmoractivestatus",
    EV_DEFAULT,
    "b",
    "active",
    "sets the active status");

Event EV_Armor_SetMultiplier (    "setarmormultiplier",
    EV_DEFAULT,
    "f",
    "multiplier",
    "sets the damage multiplier for the armor");

CLASS_DECLARATION(Armor, ShieldArmor, NULL) {
    {&EV_Armor_SetActiveStatus,       &ShieldArmor::SetActiveStatus         },
    {&EV_Armor_SetMultiplier,         &ShieldArmor::SetMultiplier           },
    {&EV_Armor_AddToNoProtectionList, &ShieldArmor::AddMODToNoProtectionList},
    {&EV_Armor_UseArmorDirection,     &ShieldArmor::SetShieldDirection      }, 
    {NULL,                            NULL                                 }
};

ShieldArmor::ShieldArmor()
{
    maximum_amount = 100.0f;
    setName("ShieldArmor");
    _active       = true;
    _multiplier   = 0.0f;
    _directionMin = -90.0f;
    _directionMax = 90.0f;
    _useDirection = false;
}

void ShieldArmor::SetActiveStatus(Event *ev)
{
    _active = ev->GetBoolean(1);
}

void ShieldArmor::SetMultiplier(Event *ev)
{
    _multiplier = ev->GetFloat(1);
}

bool ShieldArmor::CanBeUsed(void)
{
    if (amount > 0) {
        return true;
    }

    return false;
}

float ShieldArmor::ResolveDamage(
    float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker
)
{
    (void)attacker;

    if (!_active) {
        return damage;
    }

    int inNoProtectionList = 0;

    inNoProtectionList = _noProtectionList.IndexOfObject((unsigned int)meansOfDeath);

    if (inNoProtectionList) {
        return damage;
    }

    if (meansOfDeath == MOD_ARMOR_PIERCING) {
        return damage;
    }

    if (amount <= 0) {
        amount = 0.0f;
        return damage;
    }

    if (_useDirection) {
        Vector directionAngle = direction.toAngles();
        float  directionYaw   = AngleNormalize180(directionAngle[YAW]);
        if ((directionYaw > _directionMax) || (directionYaw < _directionMin)) {
            return damage;
        }
    }

    amount -= damage;

    if (amount < 0) {
        amount = 0.0f;
    }

    if (amount > 0) {
        if (meansOfDeath >= 0 && meansOfDeath < MOD_TOTAL_NUMBER) {
            str modelName;

            modelName = "ReflectionModel-";
            modelName += means_of_death_strings[meansOfDeath];
            modelName += ".gdb";

            SpawnEffect(modelName, position, vec_zero, 1.0f);
        }

        return (damage * _multiplier);
    }

    return damage;
}

bool ShieldArmor::CanBeDamagedBy(int meansOfDeath)
{
    (void)meansOfDeath;
    return true;
}

void ShieldArmor::AddMODToNoProtectionList(Event *ev)
{
    str MODName = ev->GetString(1);
    AddMODToNoProtectionList(MODName);
}

void ShieldArmor::AddMODToNoProtectionList(const str& MODName)
{
    unsigned int theMOD = MOD_NameToNum(MODName.c_str());  

    _noProtectionList.AddUniqueObject(theMOD);
}

void ShieldArmor::SetShieldDirection(Event *ev)
{
    SetShieldDirection(ev->GetFloat(1), ev->GetFloat(2));
}

void ShieldArmor::SetShieldDirection(float minAngle, float maxAngle)
{
    _useDirection = true;
    _directionMin = minAngle;
    _directionMax = maxAngle;
}
