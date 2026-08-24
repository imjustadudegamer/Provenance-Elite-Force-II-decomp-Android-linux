
#pragma once

#include "animate.h"
#include "sentient.h"

class Dispenser : public Entity
{
public:
    typedef enum {
        DISPENSE_TYPE_NONE,
        DISPENSE_TYPE_HEALTH,
        DISPENSE_TYPE_AMMO,
        DISPENSE_TYPE_ARMOR
    } DispenseType;

    typedef enum {
        DISPENSER_STATE_CLOSED,
        DISPENSER_STATE_CLOSING,
        DISPENSER_STATE_OPEN,
        DISPENSER_STATE_OPENING,
        DISPENSER_STATE_EMPTY,
        DISPENSER_STATE_EMPTYING
    } DispenserState;

private:
    float          _maxAmount;
    float          _amount;
    float          _dispenseRate;
    float          _regenRate;
    DispenseType   _dispenseType;
    str            _dispenseSubtype;
    float          _lastTimeUsed;
    DispenserState _state;
    float          _openDistance;
    bool           _instant;
    SentientPtr    _entityDispensingTo;
    str            _useSoundName;
    bool           _playingSound;
    float          _lastSoundTime;

protected:
    void setDispenserAlpha(void);
    bool dispenseStuff(Sentient *sent);

public:
    CLASS_PROTOTYPE(Dispenser);

    Dispenser();

    virtual void Think(void) override;

    void useEvent(Event *ev);

    void setMaxAmount(Event *ev);
    void setDispenseRate(Event *ev);
    void setRegenRate(Event *ev);
    void setType(Event *ev);
    void setSubtype(Event *ev);
    void setOpenDistance(Event *ev);
    void setInstant(Event *ev);
    void setSoundName(Event *ev);

    void animDone(Event *ev);

    virtual void Archive(Archiver& arc) override;
};

inline void Dispenser::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveFloat(&_maxAmount);
    arc.ArchiveFloat(&_amount);
    arc.ArchiveFloat(&_dispenseRate);
    arc.ArchiveFloat(&_regenRate);

    ArchiveEnum(_dispenseType, DispenseType);

    arc.ArchiveString(&_dispenseSubtype);
    arc.ArchiveFloat(&_lastTimeUsed);

    ArchiveEnum(_state, DispenserState);

    arc.ArchiveFloat(&_openDistance);
    arc.ArchiveBool(&_instant);
    arc.ArchiveSafePointer(&_entityDispensingTo);
    arc.ArchiveString(&_useSoundName);
    arc.ArchiveBool(&_playingSound);
    arc.ArchiveFloat(&_lastSoundTime);
}
