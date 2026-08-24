
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
