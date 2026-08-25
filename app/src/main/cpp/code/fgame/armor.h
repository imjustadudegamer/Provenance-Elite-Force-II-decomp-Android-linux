

#pragma once

#include "weapon.h"

class Armor : public Item
{
protected:
    virtual qboolean _needThisArmor(Sentient *sentient);
    virtual void     _pickupArmor(Sentient *sentient);

public:
    CLASS_PROTOTYPE(Armor);

    Armor();
    ~Armor();

    Item *ItemPickup(Entity *other, qboolean add_to_inventory = qtrue, qboolean checkautopickup = qtrue) override;

    virtual float ResolveDamage(
        float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker
    );

    virtual bool CanBeDamagedBy(int meansOfDeath);

    void Archive(Archiver& arc) override;
};

inline void Armor::Archive(Archiver& arc)
{
    Item::Archive(arc);
}

typedef struct
{
    int   MODIndex;
    float damage;
    bool  adapted;
} MODHitCounter;

#define ADAPTION_LIMIT 200

#define FX_CHANGE_DELTA 25

class AdaptiveArmor : public Armor
{
protected:
    static Container<MODHitCounter *> _AdaptionList;      
    static Container<int>             _CannotAdaptToList; 
    static Container<str>             _fxList;

    int    _currentFXIndex;
    str    _AdaptionFX;
    Vector _currentFXPosition;
    float  _fxTime;
    float  _fxTimeNormal;
    float  _fxTimeExplosive;
    float  _adaptionLimit;

    void     _AddAdaption(int meansOfDeath, float damage);
    void     _UpdateAdaption(int meansOfDeath, float damage);
    qboolean _AdaptionInList(int meansOfDeath);
    float    _GetDamageTotal(int meansOfDeath);
    void     _PlayAdaptionFX(const Vector& direction, const Vector& position);
    void     _AddMODToCannotAdaptList(int mod);
    qboolean _CanAdaptTo(int mod);
    void     _changeFX();
    void     SetFXLife(Event *ev);  

public:
    CLASS_PROTOTYPE(AdaptiveArmor);

    AdaptiveArmor();
    ~AdaptiveArmor();

    float ResolveDamage(
        float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker
    ) override;

    void SetAdaptionFX(Event *ev);

    bool        CanBeDamagedBy(int meansOfDeath) override;
    static void ClearAdaptionList();

    void AddGameVar(const str& name, float value);
    void UpdateGameVar(const str& name, float value);
    void LoadAdaptionData(const str& name);
    void LoadDataFromGameVars(Event *ev);

    void Archive(Archiver& arc) override;






public:
    void SetAdaptionFX ( const str &fxName ); 
    void LoadDataFromGameVars(); 
};

inline void AdaptiveArmor::Archive(Archiver& arc)
{
    int            num = 0;
    MODHitCounter *modHitCounter;
    int            i;

    Armor::Archive(arc);

    if (arc.Loading()) {
        ClearAdaptionList();
        _CannotAdaptToList.ClearObjectList();
        _fxList.ClearObjectList();
    }

    if (arc.Saving()) {
        num = _AdaptionList.NumObjects();

        arc.ArchiveInteger(&num);

        for (i = 1; i <= num; i++) {
            modHitCounter = _AdaptionList.ObjectAt(i);

            arc.ArchiveInteger(&modHitCounter->MODIndex);
            arc.ArchiveFloat(&modHitCounter->damage);
            arc.ArchiveBool(&modHitCounter->adapted);
        }
    } else {
        arc.ArchiveInteger(&num);

        for (i = 1; i <= num; i++) {
            modHitCounter = new MODHitCounter;

            arc.ArchiveInteger(&modHitCounter->MODIndex);
            arc.ArchiveFloat(&modHitCounter->damage);
            arc.ArchiveBool(&modHitCounter->adapted);

            _AdaptionList.AddObject(modHitCounter);
        }
    }

    _CannotAdaptToList.Archive(arc);

    _fxList.Archive(arc);

    arc.ArchiveInteger(&_currentFXIndex);
    arc.ArchiveString(&_AdaptionFX);
    arc.ArchiveVector(&_currentFXPosition);
    arc.ArchiveFloat(&_fxTime);
    arc.ArchiveFloat(&_fxTimeNormal);
    arc.ArchiveFloat(&_fxTimeExplosive);
    arc.ArchiveFloat(&_adaptionLimit);
}

class BasicArmor : public Armor
{
protected:
    static const float _maxAmount;
    static const float _normalMaxAmount;
    static const float _lossRate;

    static const float _highRangeCutoffSinglePlayer;
    static const float _midRangeCutoffSinglePlayer;

    static const float _highRangeCutoffMultiplayer;
    static const float _midRangeCutoffMultiplayer;

    static const float _highRangePercent;
    static const float _midRangePercent;
    static const float _lowRangePercent;

    float _lastAddTime;

    qboolean _needThisArmor(Sentient *sentient) override;

public:
    CLASS_PROTOTYPE(BasicArmor);

    BasicArmor();

    float ResolveDamage(
        float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker
    ) override;
    bool  CanBeDamagedBy(int meansOfDeath) override;
    void  Think(void) override;
    void  Add(int num) override;

    void cacheStrings(void) override;

    void  Archive(Archiver& arc) override;
};

inline void BasicArmor::Archive(Archiver& arc)
{
    Armor::Archive(arc);
    arc.ArchiveFloat(&_lastAddTime);
}

class ShieldArmor : public Armor
{
public:
    CLASS_PROTOTYPE(ShieldArmor);

    ShieldArmor();

    void SetActiveStatus(Event *ev);
    void SetMultiplier(Event *ev);
    void AddMODToNoProtectionList(Event *ev);
    void AddMODToNoProtectionList(const str& MODName);  
    void SetShieldDirection(Event *ev);
    void SetShieldDirection(float minAngle, float maxAngle);  
    bool CanBeUsed(void);

    float ResolveDamage(
        float damage, int meansOfDeath, const Vector& direction, const Vector& position, Entity *attacker
    ) override;
    bool CanBeDamagedBy(int meansOfDeath) override;
    void Archive(Archiver& arc) override;

private:
    bool                    _active;
    float                   _multiplier;
    float                   _directionMin;
    float                   _directionMax;
    bool                    _useDirection;
    Container<unsigned int> _noProtectionList;
};

inline void ShieldArmor::Archive(Archiver& arc)
{
    Armor::Archive(arc);

    arc.ArchiveBool(&_active);
    arc.ArchiveFloat(&_multiplier);
    arc.ArchiveFloat(&_directionMin);
    arc.ArchiveFloat(&_directionMax);
    arc.ArchiveBool(&_useDirection);

    int num, i;

    if (arc.Saving()) {
        num = _noProtectionList.NumObjects();
        arc.ArchiveInteger(&num);
        for (i = 1; i <= num; i++) {
            arc.ArchiveUnsigned(&_noProtectionList.ObjectAt(i));
        }
    } else {
        arc.ArchiveInteger(&num);
        for (i = 1; i <= num; i++) {
            unsigned int theMod;
            arc.ArchiveUnsigned(&theMod);
            _noProtectionList.AddObject(theMod);
        }
    }
}
