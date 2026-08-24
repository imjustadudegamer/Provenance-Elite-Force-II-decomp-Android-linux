//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#pragma once

#include "weapon.h"

class Equipment : public Weapon
{
public:
    CLASS_PROTOTYPE(Equipment);

    Equipment();
    Equipment(const char *file);
    ~Equipment();

    void scanStart(Event *ev);
    void scanEnd(Event *ev);
    void scan(Event *ev);

    void airStrike(Event *ev);

    void setScanner(Event *ev);
    void setRadar(Event *ev);
    void hasModes(Event *ev);
    void setTypeName(Event *ev);

    void changeMode(Event *ev);
    void resetMode(void);
    void updateMode(void);

    void AttachToOwner(weaponhand_t hand) override;
    void Uninitialize(void) override;

    void Think(void) override;

    void Archive(Archiver& arc) override;

    bool hasScanner(void) const { return scanner; }
    bool hasRadar(void) const { return radar; }
    bool isScanning(void) const { return scanning; }
    str  getTypeName(void) const;

    void ProcessTargetedEntity(EntityPtr entity) override;

private:
    void init(void);




    void updateRadarState(void);            

    bool scanner;  
    bool scanning; 
    bool radar;    

    int scanTime;

    Container<str> _modes; 
    int            _currentMode;
    int            _lastMode;
    bool           _active;
    int            _scanEndFrame;

    EntityPtr _scannedEntity;

    str   _typeName;
    float _nextUseTime;


public:
    void changeMode( int newMode ); 
    void PutAway(void); 
    int getStat( int statNum ) override; 
};
