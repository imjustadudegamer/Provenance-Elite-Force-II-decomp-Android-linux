

#pragma once

#include "g_local.h"
#include "entity.h"
#include "trigger.h"

class Exploder : public Trigger
{
private:
    float damage; // SDK explosion.h:33 — float, not int

    void MakeExplosion(Event *ev);
    void SetDmg(Event *ev);
    void setDamage(Event *ev);  
    // SDK explosion.cpp:212-216 -- retail's `health` row does not just set health, it
    void SetHealth(Event *ev);

public:
    CLASS_PROTOTYPE(Exploder);

    Exploder();
    void Archive(Archiver& arc) override;
};

inline void Exploder::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&damage);
}

class MultiExploder : public Trigger
{
protected:
    float explodewait;
    float explode_time;
    float duration;
    float damage; 
    float randomness;

    void MakeExplosion(Event *ev);  
    void SetDuration(Event *ev);
    void SetWait(Event *ev);
    void SetRandom(Event *ev);

public:
    CLASS_PROTOTYPE(MultiExploder);

    MultiExploder();
    void Archive(Archiver& arc) override;






public:
    void SetHealth( Event *ev ); 
    void SetDmg( Event *ev ); 
    void setDamage( Event *ev ); 
};

inline void MultiExploder::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&explodewait);
    arc.ArchiveFloat(&explode_time);
    arc.ArchiveFloat(&duration);
    arc.ArchiveFloat(&damage);
    arc.ArchiveFloat(&randomness);
}

void CreateExplosion(
    Vector      pos,
    float       damage         = 120,
    Entity     *inflictor      = NULL,
    Entity     *attacker       = NULL,
    Entity     *ignore         = NULL,
    const char *explosionModel = NULL,
    float       scale          = 1.0f,
    float       radius         = 0.0f
);

class ExplodeObject : public MultiExploder
{
private:
    Container<str> debrismodels;
    int            debrisamount;
    float          severity;

    str   debrissound;
    str   explosionmodel;
    float explosionradius;

    // origin.  `_haveSpawnedEarthquake` is retail's one-shot latch.
    bool  _spawnEarthquake;
    bool  _haveSpawnedEarthquake;
    float _earthquakeMagnitude;
    float _earthquakeDuration;
    float _earthquakeDistance;

    void Damage(Event *ev);  
    void SetDebrisModel(Event *ev);
    void SetSeverity(Event *ev);
    void SetDebrisAmount(Event *ev);

    void ExplodeTiki(Event *ev);

    void SetSound(Event *ev);
    void SetExplosion(Event *ev);

    void setSpawnEarthquake(Event *ev);
    void setEarthquakeDuration(Event *ev);
    void setEarthquakeMagnitude(Event *ev);
    void setEarthquakeDistance(Event *ev);

public:
    CLASS_PROTOTYPE(ExplodeObject);

    ExplodeObject();
    void Archive(Archiver& arc) override;






public:
    void MakeExplosion( Event *ev ); 
};

inline void ExplodeObject::Archive(Archiver& arc)
{
    MultiExploder::Archive(arc);

    debrismodels.Archive(arc);
    arc.ArchiveInteger(&debrisamount);
    arc.ArchiveFloat(&severity);

    arc.ArchiveString(&debrissound);
    arc.ArchiveString(&explosionmodel);
    arc.ArchiveFloat(&explosionradius);

    arc.ArchiveBool(&_spawnEarthquake);
    arc.ArchiveBool(&_haveSpawnedEarthquake);
    arc.ArchiveFloat(&_earthquakeMagnitude);
    arc.ArchiveFloat(&_earthquakeDuration);
    arc.ArchiveFloat(&_earthquakeDistance);
}
