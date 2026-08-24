

#include "WeaponDualWield.h"
#include "g_spawn.h"
#include "../qcommon/gameplaymanager.h"

CLASS_DECLARATION( Weapon, WeaponDualWield, NULL )
{
	{ &EV_Anim,									&WeaponDualWield::PassToAnimate },
	{ &EV_NewAnim,								&WeaponDualWield::PassToAnimate },

	{ NULL, NULL }
};

WeaponDualWield::WeaponDualWield()
	:	_leftweapon(0),
		_rightweapon(0)
{
}

WeaponDualWield::~WeaponDualWield()
{

}

void WeaponDualWield::AttachToOwner( weaponhand_t hand_shadow1 )
{
	_leftweapon->SetOwner(owner);
	_rightweapon->SetOwner(owner);
	_leftweapon->AttachGun( WEAPON_LEFT );
	_rightweapon->AttachGun( WEAPON_RIGHT );
	_rightweapon->ForceIdle();
	_leftweapon->ForceIdle();
}

void WeaponDualWield::processGameplayData( Event *ev )
{
	ClassDef *cls;

	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm->hasObject(getArchetype()) )
		return;

	str leftmodel = gpm->getStringValue(getArchetype(), "leftmodel");
	str rightmodel = gpm->getStringValue(getArchetype(), "rightmodel");

	if ( !_leftweapon )
	{
		cls = getClass( leftmodel );
		if ( !cls )
		{
			SpawnArgs args;
			args.setArg( "model", leftmodel );
			cls = args.getClassDef();
			if ( !cls )
				return;
		}
		_leftweapon = ( Weapon * )cls->newInstance();
		_leftweapon->setModel( leftmodel );
		_leftweapon->ProcessPendingEvents();
		_leftweapon->hideModel();
	}

	if ( !_rightweapon )
	{
		cls = getClass( rightmodel );
		if ( !cls )
		{
			SpawnArgs args;
			args.setArg( "model", rightmodel );
			cls = args.getClassDef();
			if ( !cls )
				return;
		}
		_rightweapon = ( Weapon * )cls->newInstance();
		_rightweapon->setModel( rightmodel );
		_rightweapon->ProcessPendingEvents();
		_rightweapon->hideModel();
	}
	
	Weapon::processGameplayData( NULL );
}

void WeaponDualWield::PassToAnimate( Event *ev )
{
	if ( _leftweapon )
	{
		Event *leftEvent = new Event( ev );
		_leftweapon->ProcessEvent( leftEvent );
	}
	if ( _rightweapon )
	{
		Event *rightEvent = new Event( ev );
		_rightweapon->ProcessEvent( rightEvent );
	}
}