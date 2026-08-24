

#pragma once

#include "g_local.h"
#include "animate.h"

#define NO_FINAL_PITCH -1000

class Gib : public Entity
{
private:
    int     sprayed;
    float   scale;
    Entity *blood;
    str    blood_splat_name;
    float  blood_splat_size;
    str    blood_spurt_name;
    float  final_pitch;
    float  next_bleed_time;

public:
    CLASS_PROTOTYPE(Gib);

    qboolean fadesplat;
    Gib();
    ~Gib();
    Gib(str      name,
        qboolean blood_trail,
        str      bloodtrailname   = "",
        str      bloodspurtname   = "",
        str      bloodsplatname   = "",
        float    blood_splat_size = 8,
        float    pitch            = NO_FINAL_PITCH);
    void SetVelocity(float health);
    void SprayBlood(Vector start);
    void Throw(Event *ev);
    void Splat(Event *ev);
    void Stop(Event *ev);
    void Damage(Event *ev);
    void ClipGibVelocity(void);
    void Archive(Archiver& arc) override;
};

inline void Gib::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveBoolean(&sprayed);
    arc.ArchiveFloat(&scale);
    arc.ArchiveObjectPointer((Class **)&blood);
    arc.ArchiveString(&blood_splat_name);
    arc.ArchiveFloat(&blood_splat_size);
    arc.ArchiveString(&blood_spurt_name);
    arc.ArchiveFloat(&final_pitch);
    arc.ArchiveBoolean(&fadesplat);
    arc.ArchiveFloat(&next_bleed_time);
}

void CreateGibs(Entity *ent, float damage = -50, float scale = 1.0f, int num = 1, const char *modelname = NULL);

extern Event EV_ThrowGib;
