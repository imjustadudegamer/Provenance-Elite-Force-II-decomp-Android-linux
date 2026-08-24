//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/shrapnelbomb.h                                $
// $Revision:: 7                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

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
