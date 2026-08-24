//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/powerups.cpp                              $
// $Revision:: 58                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
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

#include "powerups.h"
#include "mp_manager.hpp"  
#include "player.h"
#include "weaputils.h"
#include "level.h"
#include "g_spawn.h"
#include "g_utils.h"







Event EV_Rune_Respawn
(
	"powerup_respawn",
	EV_CODEONLY,
	NULL,
	NULL,
	"Makes the rune respawn in its original position."
);

Event EV_HoldableItem_PowerupToSpawn
(
	"powerupToSpawn",
	EV_TIKIONLY,
	"s",
	"powerupName",
	"Sets the powerup to spawn when used."
);





Event EV_Powerup_ModelToAttach (	"powerup_modelToAttach",
	EV_DEFAULT,
	"sSF",
	"modelName tagName removeTime",
	"Sets the model info to use when the powerup is used."
);














    Event EV_Powerup_Shader (
    "powerup_shader",
    EV_DEFAULT,
    "s",
    "shaderName",
    "Sets the shader use when the powerup is used."
);
    Event EV_Powerup_ModelToSpawn (
    "powerup_modelToSpawn",
    EV_DEFAULT,
    "s",
    "modelName",
    "Sets the model to spawn when the powerup is used (only works on certain models)."
);

CLASS_DECLARATION(Item, PowerupBase, NULL) {
    {&EV_Item_SetAmount,        &PowerupBase::amountEvent           },
    {&EV_Powerup_ModelToAttach, &PowerupBase::setModelToAttachOnUse },
    {&EV_Powerup_ModelToSpawn,  &PowerupBase::setModelToSpawnOnUse  },
    {&EV_Powerup_Shader,        &PowerupBase::setShaderToDisplayOnUse},
    {NULL,                      NULL                                }
};

PowerupBase::PowerupBase()
{
    _modelToAttachOnUseRemoveTime = 0.0f;

    if (LoadingSavegame) {
        return;
    }

    setName("UnknownItem");
    amount = 30;
}

void PowerupBase::init(const str& modelName, Sentient *ownerSent)
{
    _modelName = modelName;
    _owner     = ownerSent;

    setModel(modelName);

    CancelEventsOfType(EV_ProcessInitCommands);
    ProcessInitCommands(gi.modelindex(modelName.c_str()));

    CancelEventsOfType(EV_Item_DropToFloor);

    setSolidType(SOLID_NOT);
    hideModel();
}

void PowerupBase::amountEvent(Event *ev)
{
    amount = ev->GetInteger(1);
}

void PowerupBase::setModelToAttachOnUse(Event *ev)
{
    _modelToAttachOnUse = ev->GetString(1);

    if (ev->NumArgs() > 1) {
        _modelToAttachOnUseTag = ev->GetString(2);
    } else {
        _modelToAttachOnUseTag = "Bip01";
    }

    if (ev->NumArgs() > 2) {
        _modelToAttachOnUseRemoveTime = ev->GetFloat(3);
    } else {
        _modelToAttachOnUseRemoveTime = 0.0f;
    }
}

void PowerupBase::setModelToSpawnOnUse(Event *ev)
{
    _modelToSpawn = ev->GetString(1);
}

void PowerupBase::setShaderToDisplayOnUse(Event *ev)
{
    _shaderToDisplayOnUse = ev->GetString(1);
}

void PowerupBase::getModelToAttachOnUse(str& modelName, str& tagName, float& modelRemoveTime)
{
    modelName       = _modelToAttachOnUse;
    tagName         = _modelToAttachOnUseTag;
    modelRemoveTime = _modelToAttachOnUseRemoveTime;
}

void PowerupBase::getShaderToDisplayOnUse(str& modelName)
{
    modelName = _shaderToDisplayOnUse;
}

// Everything else is 1:1, including the deliberate `return NULL` -- a powerup
Item *PowerupBase::ItemPickup(Entity *other, qboolean add_to_inventory, qboolean checkautopickup)
{
    Player *player;
    str     realname;

    if (!other->isSubclassOf(Player)) {
        return NULL;
    }

    if (!Pickupable(other)) {
        return NULL;
    }

    player = (Player *)other;

    {
        realname = GetRandomAlias("snd_pickup");

        if (realname.length() > 1) {
            player->Sound(realname, CHAN_ITEM);
        }
    }

    CancelEventsOfType(EV_Item_DropToFloor);
    CancelEventsOfType(EV_Item_Respawn);
    CancelEventsOfType(EV_FadeOut);

    setSolidType(SOLID_NOT);

    if (_missingSkin) {
        ChangeSkin(_missingSkin, qtrue);
    } else {
        hideModel();
    }

    if (!Respawnable()) {
        PostEvent(EV_Remove, FRAMETIME);
    } else {
        PostEvent(EV_Item_Respawn, RespawnTime());
    }

    givePlayerItem(player);

    return NULL; 
}

CLASS_DECLARATION(PowerupBase, Powerup, NULL) {
    {&EV_Item_IconName,  &Item::iconNameEvent },
    {&EV_Item_SetAmount, &Powerup::amountEvent},
    {NULL,               NULL                 }
};

CLASS_DECLARATION(Powerup, PowerupSpeed, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Powerup, PowerupStrength, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Powerup, PowerupProtection, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(PowerupProtection, PowerupProtectionTemp, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Powerup, PowerupRegen, NULL) {
    {NULL, NULL}
};

const float PowerupRegen::REGEN_SPEED = 10.0f;  

CLASS_DECLARATION(Powerup, PowerupInvisibility, NULL) {
    {NULL, NULL}
};

Powerup::Powerup()
{
    _timeLeft = 0.0f;

    if (LoadingSavegame) {
        return;
    }

    setRespawn(qtrue);
    setRespawnTime(60);
}

void Powerup::amountEvent(Event *ev)
{
    _timeLeft = ev->GetInteger(1);
    amount    = (int)_timeLeft;
}


CLASS_DECLARATION(PowerupBase, Rune, NULL) {
    {&EV_Rune_Respawn, &Rune::respawnAtOriginalOrigin},
    {NULL,             NULL                          }
};

CLASS_DECLARATION(Rune, RuneDeathQuad, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Rune, RuneAmmoRegen, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Rune, RuneEmpathyShield, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Rune, RuneArmorPiercing, NULL) {
    {NULL, NULL}
};

Rune::Rune()
{
    _originalOriginSet = false;

    if (LoadingSavegame) {
        return;
    }

    setRespawn(qfalse);
}

void Rune::setOrigin(const Vector &point)
{
    PowerupBase::setOrigin(point);

    setOriginalOrigin(point, false);
}

void Rune::setOriginalOrigin(const Vector& point, bool force)
{
    if (!_originalOriginSet || force) {
        _originalOrigin    = point;
        _originalOriginSet = true;
    }
}

void Rune::respawnAtOriginalOrigin(Event *ev)
{
    if (_originalOriginSet) {
        NoLerpThisFrame();
        setOrigin(_originalOrigin);
        ProcessEvent(EV_Item_DropToFloor);
    }
}

CLASS_DECLARATION(PowerupBase, HoldableItem, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(HoldableItem, HoldableItemHealth, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(HoldableItem, HoldableItemProtection, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(HoldableItem, HoldableItemTransporter, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(HoldableItem, HoldableItemExplosive, NULL) {
    {NULL, NULL}
};

HoldableItem::HoldableItem()
{
    if (LoadingSavegame) {
        return;
    }

    amount = 0;

    setRespawn(qtrue);
    setRespawnTime(60);
}

CLASS_DECLARATION(HoldableItem, HoldableItemSpawnPowerup, NULL) {
    {&EV_HoldableItem_PowerupToSpawn, &HoldableItemSpawnPowerup::powerupToSpawn},
    {NULL,                            NULL                                     }
};

void HoldableItemSpawnPowerup::powerupToSpawn(Event *ev)
{
    _powerupToSpawn = ev->GetString(1);
}

bool HoldableItemSpawnPowerup::use(void)
{
    SpawnArgs args;
    Entity   *ent;
    Item     *item;

    if (_powerupToSpawn.length() == 0) {
        return true;
    }

    if (_owner) {
        args.setArg("model", _powerupToSpawn);

        ent = (Entity *)args.Spawn();

        if (!ent || !ent->isSubclassOf(Item)) {
            return true;
        }

        item = (Item *)ent;

        item->setOrigin(_owner->centroid);

        item->ProcessPendingEvents();

        item->PlaceItem();
        item->setOrigin(_owner->centroid);

        if (_owner->isSubclassOf(Player)) {
            Vector  viewAngles;
            Vector  viewDir;
            Player *player = (Player *)(Sentient *)_owner;
            Vector  pos;

            player->GetPlayerView(&pos, &viewAngles);

            viewAngles.AngleVectors(&viewDir);

            viewDir.normalize();
            viewDir *= 500.0f;

            item->velocity = viewDir;

            item->setOrigin(pos);
        } else {
            item->velocity = Vector(G_CRandom(100.0f), G_CRandom(100.0f), 200.0f + G_Random(200.0f));
        }

        item->edict->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;

        item->setRespawn(qfalse);

        item->setAmount((int)(item->getAmount() / 2.0f));

        item->PostEvent(EV_Remove, 10.0f);

        return true;
    }

    return false;
}



Powerup *Powerup::CreatePowerup( const str &className, const str &modelName, Sentient *sentient )
{
	str fullname;
	SpawnArgs args;
	ClassDef *cls;
	Powerup *powerup;


	fullname = "Powerup";
	fullname += className;

	args.setArg( "classname", fullname.c_str() );

	cls = args.getClassDef();

	if ( !cls )
		return NULL;

	if ( !checkInheritance( &Powerup::ClassInfo, cls ) )
		return NULL;

	powerup = (Powerup *)cls->newInstance();

	if ( !powerup )
		return NULL;

	powerup->init( modelName, sentient );

	return powerup;
}

void Powerup::cacheStrings( void )
{
	G_FindConfigstringIndex( va( "$$Using$$ $$Item-%s$$", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
	G_FindConfigstringIndex( va( "$$Item-%s$$ $$TimeRanOut$$", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
}


void Powerup::givePlayerItem( Player *player )
{
	Powerup *powerup;
	const char *modelName;

	modelName = gi.NameForNum( edict->s.modelindex );

	if ( !modelName )
		return;

	powerup = Powerup::CreatePowerup( item_name, modelName, player );

	Event *event = new Event( EV_Item_SetAmount );
	event->AddFloat( amount );
	powerup->ProcessEvent( event );

	player->setPowerup( powerup );
}


void Powerup::spawn( const Vector &origin_shadow2 )
{
	SpawnArgs args;
	Entity *ent;
	Item *item;

	if ( _timeLeft <= 0.0f )
		return;

	args.setArg( "model", _modelName );

	ent = (Entity *)args.Spawn();

	if ( !ent || !ent->isSubclassOf( Item ) )
		return;

	item = (Item *)ent;

	item->setOrigin( origin_shadow2 );

	item->ProcessPendingEvents();

	item->PlaceItem();
	item->setOrigin( origin_shadow2 );
	item->edict->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	item->_nextPickupTime = level.time + 1.0f;

	item->setAmount( (int)( _timeLeft + 1.0f ) );

	item->setRespawn( false );
}


void Powerup::update( float frameTime )
{
	_timeLeft -= frameTime;

	amount = _timeLeft;

	if ( _timeLeft <= 0 && _owner )
	{
		if ( _owner->isSubclassOf( Player ) )
		{
			Player *player = (Player *)(Sentient *)_owner;
			player->setItemText( getIcon(), va( "$$Item-%s$$ $$TimeRanOut$$", getName().c_str() ) );
		}

		_owner->PostEvent( EV_Player_RemovePowerup, 0.0f );
	}


	if ( _owner && _owner->isSubclassOf( Player ) )
	{
		str shaderName;
		Player *player = (Player *)(Sentient *)_owner;

		getShaderToDisplayOnUse( shaderName );

		if ( ( shaderName.length() > 0 ) && !player->hasCustomShader() )
		{
			if ( !player->hasCustomShader() )
			{
				player->setCustomShader( shaderName.c_str() );
			}
		} 
	}

	specificUpdate( frameTime );
}




void Rune::cacheStrings( void )
{
	G_FindConfigstringIndex( va( "$$Using$$ $$Item-%s$$", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
	G_FindConfigstringIndex( va( "$$Dropping$$ $$Item-%s$$", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
}


Item *Rune::ItemPickup( Entity *other, qboolean add_to_inventory, qboolean checkautopickup )
{
	Player *player;

	if ( !Pickupable( other ) )
		return NULL;

	if ( !other->isSubclassOf( Player ) )
		return NULL;

	player = ( Player * )other;

	if ( player->hasRune() )
		return NULL;

	return PowerupBase::ItemPickup( other, add_to_inventory );
}

Rune *Rune::CreateRune( const str &className, const str &modelName, Sentient *sentient )
{
	str fullname;
	SpawnArgs args;
	ClassDef *cls;
	Rune *rune;


	fullname = "Rune";
	fullname += className;

	args.setArg( "classname", fullname.c_str() );

	cls = args.getClassDef();

	if ( !cls )
		return NULL;

	if ( !checkInheritance( &Rune::ClassInfo, cls ) )
		return NULL;

	rune = (Rune *)cls->newInstance();

	if ( !rune )
		return NULL;

	rune->init( modelName, sentient );

	return rune;
}

void Rune::givePlayerItem( Player *player )
{
	Rune *rune;

	rune = Rune::CreateRune( item_name, gi.NameForNum( edict->s.modelindex ), player );

	player->setRune( rune );

	rune->setOriginalOrigin( getOriginalOrigin(), true );
}


void Rune::spawn( const Vector &origin_shadow1 )
{
	SpawnArgs args;
	Entity *ent;
	Item *item;

	args.setArg( "model", _modelName );

	ent = (Entity *)args.Spawn();

	if ( !ent || !ent->isSubclassOf( Item ) )
		return;

	item = (Item *)ent;

	item->setOrigin( origin_shadow1 );

	item->ProcessPendingEvents();

	item->PlaceItem();
	item->setOrigin( origin_shadow1 );
	item->edict->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	item->_nextPickupTime = level.time + 1.0f;

	item->animate->RandomAnimate( "idle" );

	if ( item->isSubclassOf( Rune ) )
	{
		Rune *rune = (Rune *)item;

		rune->setOriginalOrigin( getOriginalOrigin(), true );

		rune->PostEvent( EV_Rune_Respawn, 60.0f );
	}
}




void HoldableItem::cacheStrings( void )
{
	G_FindConfigstringIndex( va( "$$Using$$ $$Item-%s$$", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
	G_FindConfigstringIndex( va( "$$PickedUp$$ $$Item-%s$$", getName().c_str() ), CS_GENERAL_STRINGS, MAX_GENERAL_STRINGS, true );
}


HoldableItem *HoldableItem::createHoldableItem( const str &className, const str &modelName, Sentient *sentient )
{
	str fullname;
	SpawnArgs args;
	ClassDef *cls;
	HoldableItem *holdableItem;


	fullname = "HoldableItem";
	fullname += className;

	args.setArg( "classname", fullname.c_str() );

	cls = args.getClassDef();

	if ( !cls )
		return NULL;

	if ( !checkInheritance( &HoldableItem::ClassInfo, cls ) )
		return NULL;

	holdableItem = (HoldableItem *)cls->newInstance();

	if ( !holdableItem )
		return NULL;

	holdableItem->init( modelName, sentient );

	return holdableItem;
}

void HoldableItem::givePlayerItem( Player *player )
{
	HoldableItem *holdableItem;

	holdableItem = HoldableItem::createHoldableItem( item_name, model, player );

	if ( holdableItem )
	{
		player->setItemText( holdableItem->getIcon(), va( "$$PickedUp$$ $$Item-%s$$", holdableItem->getName().c_str() ) );

		player->setHoldableItem( holdableItem );
	}
}




float PowerupBase::RespawnTime( void )
{
	if ( multiplayerManager.inMultiplayer() )
		return respawntime * multiplayerManager.getPowerupRespawnMultiplayer();
	else
		return respawntime;
}






HoldableItemExplosive::HoldableItemExplosive()
{
	_explosiveArmed = false;
	_explosiveAlive = true;

	_explosiveArmTime = 0.0f;

	_explosive = NULL;

	_lastSoundTime = 0.0f;

	_nextProximitySoundTime = 0.0f;
}




bool HoldableItemExplosive::findPlaceToSet( Vector &newOrigin, Vector &newAngles )
{
	Player *player;
	trace_t viewTrace;
	Vector forward;
	Vector left;
	Vector up;
	float axis[3][3];
	vec3_t newAnglesVec;

	if ( !_owner || !_owner->isSubclassOf( Player ) )
		return false;

	player = (Player *)(Sentient *)_owner;
		
	memset( &viewTrace, 0, sizeof(trace_t) );

	player->GetViewTrace( viewTrace, MASK_SHOT, 16.0f * 7.0f );

	if ( ( viewTrace.fraction < 1.0f ) && ( viewTrace.ent && viewTrace.entityNum == ENTITYNUM_WORLD ) )
	{
		newOrigin = viewTrace.endpos;

		up = viewTrace.plane.normal;
		PerpendicularVector( forward, viewTrace.plane.normal );
		left.CrossProduct( up, forward );

		forward.copyTo( axis[ AXIS_FORWARD_VECTOR ] );
		left.copyTo( axis[ AXIS_RIGHT_VECTOR ] );
		up.copyTo( axis[ AXIS_UP_VECTOR ] );

		AxisToAngles( axis, newAnglesVec );

		newAngles = newAnglesVec;

		return true;
	}

	return false;
}




bool HoldableItemExplosive::isPlayerInRange( const Vector &position, float maxDistance )
{
	int i;
	Entity *entity;
	gentity_t *edict_shadow1;
	trace_t trace;
	Vector dir;
	float distance;

	for( i = 0; i < game.maxclients; i++ )
	{
		edict_shadow1 = &g_entities[ i ];

		if ( !edict_shadow1->inuse || !edict_shadow1->entity || !edict_shadow1->client )
		{
			continue;
		}
		
		entity = edict_shadow1->entity;

		if ( entity->health < 0.0f )
		{
			continue;
		}

		dir = entity->centroid - position;
		distance = dir.length();

		if ( distance < maxDistance )
		{
			trace = G_Trace( position, vec_zero, vec_zero, entity->centroid, _explosive, MASK_SHOT, false, "HoldableItemExplosive::isPlayerInRange" );

			if ( ( trace.ent ) && ( trace.ent->entity == entity ) )
				return true;
		}
	}

	return false;
}




void HoldableItemExplosive::specificUpdate( float frameTime )
{
	if ( _explosiveArmed )
	{
		if ( _explosiveAlive )
		{

			if ( !_explosive || ( _explosive->getHealth() <= 0.0f ) )
			{

				_explosiveAlive = false;


				_explosive->SpawnEffect( "fx/fx-explosion-debris-rocks-dust-brown.tik", _explosive->origin, _explosive->angles, 2.0f );


				_explosive->PostEvent( EV_Remove, 0.0f );
				_explosive = NULL;

				if ( _owner && _owner->isSubclassOf( Player ) )
				{
					Player *player = (Player *)(Sentient *)_owner;

					player->removeHoldableItem();
				}
			}
			else
			{
				if ( level.time > _nextProximitySoundTime )
				{
					_nextProximitySoundTime = level.time + 1.0f;

					if ( isPlayerInRange( _explosive->centroid, 300.0f ) )
					{
						if ( _owner )
						{
							_owner->Sound( "snd_proximity", CHAN_AUTO, DEFAULT_VOL, DEFAULT_MIN_DIST, NULL, 1.0f, true );
						}
					}
				}
			}
		}
	}
}




bool HoldableItemExplosive::use( void )
{
	if ( _owner )
	{
		if ( !_owner->isSubclassOf( Player ) )
			return false;

		[[maybe_unused]] Player *player;

		player = (Player *)(Sentient *)_owner;


		if ( _explosiveArmed && ( _explosiveArmTime + 1.0f < level.time ) )
		{
			if ( _explosiveAlive && _explosive )
			{

				ExplosionAttack( _explosive->origin, _owner, "models/weapons/explosion-holdable.tik" );


				if ( _explosive )
				{
					_explosive->PostEvent( EV_Remove, 0.0f );
					_explosive = NULL;
				}

				_explosiveAlive = false;
			}

			return true;
		}
		else if ( !_explosiveArmed )
		{
			Vector newOrigin;
			Vector newAngles;


			if ( !findPlaceToSet( newOrigin, newAngles ) )
			{
				if ( _lastSoundTime + 0.5f < level.time )
				{
					Sound( "snd_noammo" );

					_lastSoundTime = level.time;
				}

				return false;
			}

			_explosiveArmTime = level.time;
			_explosiveArmed = true;


			_explosive = new Entity( ENTITY_CREATE_FLAG_ANIMATE );

			_explosive->setModel( "models/item/holdable_explosive.tik" );

			_explosive->CancelEventsOfType( EV_ProcessInitCommands );
			_explosive->ProcessInitCommands( _explosive->edict->s.modelindex );

			_explosive->setSolidType( SOLID_BBOX );
			_explosive->setContents( CONTENTS_SHOOTABLE_ONLY );  

			_explosive->setSize( Vector( -16, -16, 0 ), Vector( 16, 16, 32 ) );

			_explosive->setHealth( 5.0f );

			_explosive->takedamage = DAMAGE_YES;

			_explosive->setOrigin( newOrigin );
			_explosive->setAngles( newAngles );
			_explosive->animate->RandomAnimate( "idle_placed" );
		}
	}

	return false;
}




HoldableItemExplosive::~HoldableItemExplosive()
{
	if ( _explosiveAlive && _explosive )
	{

		_explosive->SpawnEffect( "fx/fx-explosion-debris-rocks-dust-brown.tik", _explosive->origin, _explosive->angles, 2.0f );


		_explosive->PostEvent( EV_Remove, 0.0f );
	}
}






PowerupInvisibility::PowerupInvisibility()
{
	_started = false;
}




void PowerupInvisibility::specificUpdate( float frameTime )
{
	if ( _owner )
	{

		Event *event;

		event = new Event( EV_DisplayEffect );
		event->AddString( "start_invisibility" );
		_owner->ProcessEvent( event );

		_started = true;
	}
}




PowerupInvisibility::~PowerupInvisibility()
{
	if ( _owner && _started )
	{

		Event *event;

		event = new Event( EV_DisplayEffect );
		event->AddString( "stop_invisibility" );
		_owner->ProcessEvent( event );
	}
}






RuneAmmoRegen::RuneAmmoRegen()
{
	_nextGiveTime = 0.0f;
}




void RuneAmmoRegen::specificUpdate( float frameTime )
{
	str ammoType;
	Weapon *weapon;

	if ( _owner && ( level.time > _nextGiveTime ) && _owner->isSubclassOf( Player ) )
	{
		Player *player = (Player *)(Sentient *)_owner;

		weapon = player->GetActiveWeapon( WEAPON_DUAL );

		if ( weapon )
		{
			ammoType = weapon->GetAmmoType( FIRE_MODE1 );
	
			player->GiveAmmo( ammoType, 1, false );
		}

		_nextGiveTime = level.time + 1.0f;
	}
}






float PowerupProtection::getDamageTaken( Entity *attacker, float damage, int meansOfDeath )
{

	if ( meansOfDeath == MOD_TELEFRAG )
	{
		_timeLeft = 0.0f;
		return damage;
	}

	return 0.0f;
}






void PowerupRegen::specificUpdate( float frameTime )
{
	if ( _owner )
	{
		if ( _owner->getHealth() > 0.0f )
		{
			if ( _owner->isSubclassOf( Player ) )
			{
				Player *player = (Player *)(Sentient *)_owner;

				if ( !player->canRegenerate() )
					return;
			}

			_owner->AddHealth( frameTime * REGEN_SPEED, 200.0f );
		}
	}
}






bool HoldableItemHealth::use( void )
{
	if ( _owner )
	{
		if ( _owner->getHealth() < _owner->getMaxHealth() )
		{
			_owner->addHealth( amount );
			return true;
		}
	}

	return false;
}






bool HoldableItemTransporter::use( void )
{
	Entity *spawnPoint;

	if ( _owner )
	{
		if ( !_owner->isSubclassOf( Player ) )
			return false;

		Player *player;

		player = (Player *)(Sentient *)_owner;

		if ( multiplayerManager.inMultiplayer() )
		{
			spawnPoint = multiplayerManager.getSpawnPoint( player );

			if ( spawnPoint )
			{
				if ( _modelToSpawn.length() > 0 )
				{
					SpawnEffect( _modelToSpawn, player->origin, vec_zero, 1.0f );
				}

				player->WarpToPoint( spawnPoint );

				KillBox( player );
				return true;
			}
		}
	}

	return false;
}






void RuneDeathQuad::specificUpdate( float frameTime )
{
	Event *event;

	if ( _owner )
	{
		event = new Event( EV_Hurt );
		event->AddFloat( frameTime * 5.0f );
		event->AddString( "deathQuad" );
		_owner->ProcessEvent( event );
	}
}






float RuneEmpathyShield::getDamageTaken( Entity *attacker, float damage, int meansOfDeath )
{
	float realDamage;
	Event *event;

	realDamage = damage;

	if ( attacker && ( meansOfDeath != MOD_EMPATHY_SHIELD ) )
	{
		realDamage *= 0.5;

		event = new Event( EV_Hurt );
		event->AddFloat( realDamage );
		event->AddString( "empathyShield" );
		attacker->ProcessEvent( event );
	}

	return realDamage;
}






float PowerupProtectionTemp::getDamageDone( float damage, int meansOfDeath )
{
	_timeLeft = 0.0f;

	if ( owner )
	{
		owner->Sound( "snd_powerupFizzle", CHAN_ITEM );
	}

	return damage;
}






bool HoldableItemProtection::use( void )
{
	Event *event;

	if ( _owner && !multiplayerManager.checkFlag( MP_FLAG_NO_POWERUPS ) )
	{
		if ( !_owner->isSubclassOf( Player ) )
			return false;

		Powerup *powerup;
		Player *player;

		player = (Player *)(Sentient *)_owner;

		powerup = Powerup::CreatePowerup( "Protection", "models/item/powerup_protection.tik", player );

		event = new Event( EV_Item_SetAmount );
		event->AddFloat( 15.0f );
		powerup->ProcessEvent( event );

		if ( powerup )
		{
			player->setPowerup( powerup );
			return true;
		}
	}

	return false;
}

