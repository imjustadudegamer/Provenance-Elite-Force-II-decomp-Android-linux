//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/WeaponDualWield.h           $
// $Revision:: 5                                                             $
//   $Author:: Steven                                                  $
//     $Date:: 10/11/02 4:05a                                    $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class WeaponDualWield;

#ifndef __WEAPONDUALWIELD_H__
#define __WEAPONDUALWIELD_H__

#include "g_local.h"
#include "item.h"
#include "ammo.h"
#include "sentient.h"
#include "weapon.h"

class WeaponDualWield : public Weapon
{
private:
	Weapon			*_leftweapon;
	Weapon			*_rightweapon;
	
protected:
	void			PassToAnimate( Event *ev ) override;
	
public:
	CLASS_PROTOTYPE( WeaponDualWield );
	
						WeaponDualWield();
						~WeaponDualWield();
	
	Weapon*				getRightWeapon()	{ return _rightweapon; }
	Weapon*				getLeftWeapon()		{ return _leftweapon; }
	
	void				processGameplayData( Event *ev ) override;
	
	virtual void		AttachToOwner( weaponhand_t hand ) override;
	void				Archive(	Archiver &arc ) override;
};

inline void WeaponDualWield::Archive( Archiver &arc )
{
	Weapon::Archive( arc );

	arc.ArchiveObjectPointer( (Class **)&_leftweapon );
	arc.ArchiveObjectPointer( (Class **)&_rightweapon );
}

#endif
