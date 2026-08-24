
#pragma once

#include "weapon.h"
#include "weaputils.h"

class ShrapnelBomb : public Projectile
{
private:
    str   shrapnelModel;
    int   shrapnelCount;
    bool  _splitOnDescent;
    bool  _randomSpread;
    float _startTime; 

public:
    CLASS_PROTOTYPE(ShrapnelBomb);

    ShrapnelBomb();

    void SetShrapnelModel(Event *ev);
    void SetShrapnelCount(Event *ev);

    void Explode(Event *ev) override;
    void Explode(void);

    void Think(void) override;

    void Archive(Archiver& arc) override;
};

inline void ShrapnelBomb::Archive(Archiver& arc)
{
    Projectile::Archive(arc);

    arc.ArchiveString(&shrapnelModel);
    arc.ArchiveInteger(&shrapnelCount);
    arc.ArchiveBool(&_splitOnDescent);
    arc.ArchiveBool(&_randomSpread);
}
