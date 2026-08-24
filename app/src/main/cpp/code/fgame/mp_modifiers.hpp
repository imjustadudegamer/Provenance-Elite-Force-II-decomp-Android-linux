
#ifndef __MP_MODIFIERS_HPP__
#define __MP_MODIFIERS_HPP__

#include "g_local.h"          
#include "player.h"           
#include "item.h"
#include "playerstart.h"      
#include "container.h"        
#include "str.h"              
#include "mp_shared.hpp"
#include "equipment.h"

class MultiplayerModifier
{
private:

public:
								MultiplayerModifier() {};
	virtual						~MultiplayerModifier() {};

	virtual void				init( int maxPlayers ) {};
	virtual void				initItems( void ) {};
	virtual void				start( void ) {};

	virtual bool				shouldKeepItem( MultiplayerItem *item ) { return false; }
	virtual bool				shouldKeepNormalItem( Item *item ) { return true; }
	virtual void				itemKept( MultiplayerItem *item ) {};
	virtual bool				checkRule( const char *rule, bool defaultValue, Player *player = NULL ) { return defaultValue; }
	virtual bool				checkGameType( const char *rule ) { return false; }
	virtual bool				doesPlayerHaveItem( Player *player, const char *itemName ) { return false; }

	virtual int					getStat( Player *player, int statNum, int value ) { return value; }
	virtual int					getIcon( Player *player, int statNum, int value ) { return value; }
	virtual int					getScoreIcon( Player *player, int index, int value ) { return value; }
	virtual int					getInfoIcon( Player *player ) { return 0; }

	virtual float				playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath ) { return damage; };
	virtual void				playerFired( Player *attackingPlayer ) {};
	virtual void				playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath ) {};
	virtual void				playerSpawned( Player *player ) {};

	virtual void				matchOver( void ) {};

	virtual void				itemTouched( Player *player, MultiplayerItem *item ) {};
	virtual void				itemDestroyed( Player *player, MultiplayerItem *item ) {};
	virtual float				itemDamaged( MultiplayerItem *item, Player *attackingPlayer, float damage, int meansOfDeath ) { return damage; }
	virtual void				itemUsed( Entity *entity, MultiplayerItem *item ) {};

	virtual void				playerUsed( Player *usedPlayer, Player *usingPlayer, Equipment *equipment ) {};

	virtual bool				canGivePlayerItem( int entnum, const str &itemName ) { return true; }

	virtual void				addPlayer( Player *player ) {};
	virtual void				removePlayer( Player *player ) {};

	virtual void				joinedTeam( Player *player, const str &teamName ) {};

	virtual void				applySpeedModifiers( Player *player, int *moveSpeed ) {};
	virtual void				applyJumpModifiers( Player *player, int *jumpSpeed ) {};
	virtual void				applyAirAccelerationModifiers( Player *player, int *airAcceleration ) {};

	virtual bool				canPickup( Player *player, MultiplayerItemType itemType, const char *item_name ) { return true; }
	virtual void				pickedupItem( Player *player, MultiplayerItemType itemType, const char *itemName ) {};

	virtual void				update( float frameTime ) {};

	virtual int					getPointsForKill( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath, int points ) { return points; }

	virtual void				playerEventNotification( const char *eventName, const char *eventItemName, Player *eventPlayer ) {};

	virtual void				playerCommand( Player *player, const char *command, const char *parmName ) {};

	virtual void				matchStarted( void ) {};
	virtual void				matchStarting( void ) {};
	virtual void				matchRestarted( void ) {};
	virtual void				matchEnded( void ) {};

	virtual str					getSpawnPointType( Player *player ) { return ""; }
	virtual float				getSpawnPointPriority( Player *player ) { return 0.0f; }

	virtual bool				isValidPlayerModel( Player *player, str modelToUse, bool defaultValue ) { return defaultValue; }
	virtual str					getDefaultPlayerModel( Player *player, str modelName ) { return modelName; }

	virtual void				playerChangedModel( Player *player ) {};
	virtual bool				skipWeaponReloads( void ) { return false; }
};

class DestructionPlayerData
{
public:

	float				_lastHealTime;
	float				_damageDone;
	float				_healthHealed;

						DestructionPlayerData() { reset(); }
	void				reset( void ) { _lastHealTime = 0.0f; _damageDone = 0.0f; _healthHealed = 0.0f; }
};

class ModifierDestruction : public MultiplayerModifier
{
private:
	static const float			_defaultObjectHealth;
	static const int			_pointsForDestroyingObject;
	static const float			_objectHealRate;
	static const float			_maxGuardingDist;

	static const float			_minDamageForPoints;
	static const float			_minHealingForPoints;

	static const int			_pointsForDamage;
	static const int			_pointsForHealing;
	static const int			_pointsForDestroying;
	static const int			_pointsForGuarding;

	MultiplayerItem *			_redDestructionObject;
	MultiplayerItem *			_blueDestructionObject;

	float						_redLastDamageSoundTime;
	float						_blueLastDamageSoundTime;

	int							_maxPlayers;
	DestructionPlayerData *		_destructionPlayerData;

	float						_redObjectLasthealth;
	float						_blueObjectLasthealth;

	bool						_blueObjectDestroyed;
	bool						_redObjectDestroyed;

	float						_respawnTime;

	float						findDistanceToTeamsObject( const str &teamName, const Vector &position );
	void						updateObjectAnim( MultiplayerItem *destructionObject, float health, float lastHealth, float maxHealth );
	int							getStage( float health, float maxHealth );

public:
								ModifierDestruction();
								~ModifierDestruction();

	 void			init( int maxPlayers );

	 int			getStat( Player *player, int statNum, int value );

	 void			addPlayer( Player *player );
	 void			playerSpawned( Player *player );
	 void			playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );

	 bool			shouldKeepItem( MultiplayerItem *item );
	 void			itemDestroyed( Player *player, MultiplayerItem *item );
	 float			itemDamaged( MultiplayerItem *item, Player *attackingPlayer, float damage, int meansOfDeath );
	 void			itemUsed( Entity *entity, MultiplayerItem *item );
	 void			update( float frameTime );
	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );
	 bool			checkGameType( const char *rule );
};

class ModifierOneFlag : public MultiplayerModifier
{
private:

public:
								ModifierOneFlag() {};
								~ModifierOneFlag() {};
	 bool			shouldKeepItem( MultiplayerItem *item );
	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );
	 bool			checkGameType( const char *rule );
	 void			addPlayer( Player *player );
};

class EliminationPlayerData
{
public:

	bool				_eliminated;

						EliminationPlayerData() { reset(); }
	void				reset( void ) { _eliminated = false; }
};

class ModifierElimination : public MultiplayerModifier
{
private:
	static const int			_pointsForBeingLastAlive;

	bool						_respawning;
	bool						_playerEliminated;
	int							_maxPlayers;

	EliminationPlayerData*		_playerEliminationData;

	bool						_matchOver;
	float						_matchStartTime;

	int							_eliminatedIconIndex;

	bool						_needPlayers;

	int							_eliminatedTextIndex;
	int							_nextRoundTextIndex;

	void						reset( void );
	int							numPlayersAliveOnTeam( const str &teamName );

public:
								ModifierElimination();
								~ModifierElimination();

	 void			init( int maxPlayers );
	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );
	 void			update( float frameTime );

	 int			getStat( Player *player, int statNum, int value );
	 int			getScoreIcon( Player *player, int index, int value );

	 void			addPlayer( Player *player );
	 void			playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );

	 void			matchStarting( void );
};


class DiffusionBombPlace
{
public:

	MultiplayerItem *	_item;

	bool				_armed;

	float				_totalArmingTime;
	float				_totalDisarmingTime;

	float				_lastArmingTime;
	float				_lastDisarmingTime;

	float				_totalArmedTime;

						DiffusionBombPlace();
	void				reset( void );
};

class ModifierDiffusion : public MultiplayerModifier
{
private:
	static const float			_timeNeededToArmBomb;
	static const float			_timeNeededToDisarmBomb;
	static const float			_maxGuardingDist;
	static const float			_maxArmingPause;
	static const float			_maxDisarmingPause;
	static const float			_maxBombOnGroundTime;

	static const int			_pointsForArmingBomb;
	static const int			_pointsForExplodingBomb;
	static const int			_pointsForDisarmingBomb;
	static const int			_pointsForGuardingBase;
	static const int			_pointsForGuardingTheBomber;
	static const int			_pointsForGuardingBomb;
	static const int			_pointsForKillingTheBomber;

	static const int			_teamPointsForBombing;


	int							_bomber;
	int							_bombArmedByPlayer;

	int							_lastBomber;
	float						_bombDroppedTime;

	float						_timeNeededForBombToExplode;

	DiffusionBombPlace			_redBombPlace;
	DiffusionBombPlace			_blueBombPlace;

	MultiplayerItem *			_bomb;
	MultiplayerItem *			_tempBombItem;
	Entity *					_attachedBomb;

	float						_tempBombItemTime;

	int							_maxPlayers;

	int							_bomberIconIndex;
	int							_diffuserIconIndex;

	int							_bombNormalIconIndex;
	int							_bombPlacedIconIndex;
	int							_bombArmedIconIndex;

	int							_redBombPlaceArmedIconIndex;
	int							_blueBombPlaceArmedIconIndex;
	int							_bombCarriedByRedTeamIconIndex;
	int							_bombCarriedByBlueTeamIconIndex;
	int							_bombInBaseIconIndex;
	int							_bombOnGroundIconIndex;

	float						_respawnTime;


	void						dropBomb( Player *player );
	void						respawnBomb( bool quiet = false );
	bool						playerOnOffense( Player *player );

	Player *					getBomber( void );
	void						makeBomber( Player *player );
	void						clearBomber( void );
	bool						withinGuardDistance( const Vector &origin1, const Vector &origin2 );

	bool						playerGuardedBomber( Player *attackingPlayer, Player *killedPlayer );
	bool						playerGuardedBase( Player *attackingPlayer, Player *killedPlayer );
	bool						playerGuardedBomb( Player *attackingPlayer, Player *killedPlayer );

	void						attachBomb( Player *player );

public:
								ModifierDiffusion();
								~ModifierDiffusion();

	 void			init( int maxPlayers );
	 bool			shouldKeepItem( MultiplayerItem *item );
	 void			matchStarted( void );

	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );

	 void			update( float frameTime );

	 void			itemTouched( Player *player, MultiplayerItem *item );
	 void			itemUsed( Entity *entity, MultiplayerItem *item );

	 int			getStat( Player *player, int statNum, int value );


	 void			addPlayer( Player *player );
	 void			removePlayer( Player *player );

	 void			playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );
	 void			playerCommand( Player *player, const char *command, const char *parmName );

	 void			playerSpawned( Player *player );
	 void			playerEventNotification( const char *eventName, const char *eventItemName, Player *eventPlayer );

	 int			getIcon( Player *player, int statNum, int value );
	 int			getScoreIcon( Player *player, int index, int value );

	 void			playerChangedModel( Player *player );
};

typedef enum
{
	SPECIALTY_NONE,
	SPECIALTY_INFILTRATOR,
	SPECIALTY_MEDIC,
	SPECIALTY_TECHNICIAN,
	SPECIALTY_DEMOLITIONIST,
	SPECIALTY_HEAVY_WEAPONS,
	SPECIALTY_SNIPER
} SpecialtyType;

class SpecialtyPlayerData
{
public:

	static const SpecialtyType	_defaultSpecialty;

	SpecialtyType		_specialty;
	MultiplayerItem *	_item;
	float				_lastUseTime;
	bool				_forced;
	float				_amountOfHealing;
	int					_lastPlayer;
	float				_lastPlayerTime;
	float				_regenHoldableItemTime;

						SpecialtyPlayerData();
	void				reset( void );
};

class SpecialtyItem
{
public:
	MultiplayerItem *	_item;
	SpecialtyType		_type;
	str					_teamName;

	bool				_needToRespawn;
	float				_respawnTime;
	float				_minimumRespawnTime;
	float				_pickedupTime;
};

class ModifierSpecialties : public MultiplayerModifier
{
private:
	static const float			_infiltratorMinRespawnTime;
	static const float			_medicMinRespawnTime;
	static const float			_technicianMinRespawnTime;
	static const float			_demolitionistMinRespawnTime;
	static const float			_heavyweaponsMinRespawnTime;
	static const float			_sniperMinRespawnTime;

	static const float			_startingInfiltratorHealth;
	static const float			_startingInfiltratorMassModifier;
	static const float			_startingMedicArmor;
	static const float			_startingTechnicianArmor;
	static const float			_startingDemolitionistArmor;
	static const float			_startingHeavyWeaponsHealth;
	static const float			_startingHeavyWeaponsArmor;
	static const float			_startingHeavyWeaponsMassModifier;
	static const float			_startingSniperArmor;
	static const float			_startingNormalHealth;

	static const float			_medicHealOtherRate;
	static const float			_medicHealSelfRate;
	static const float			_infiltratorMoveSpeedModifier;
	static const float			_heavyWeaponsMoveSpeedModifier;
	static const float			_infiltratorJumpSpeedModifier;
	static const float			_infiltratorAirAccelerationModifier;

	static const float			_technicianHoldableItemRegenTime;
	static const float			_demolitionistHoldableItemRegenTime;

	static const bool			_medicCanSeeEnemyHealth;

	static const float			_amountOfHealingForPoints;
	static const int			_pointsForHealing;

	static const bool			_removeItems;

	SpecialtyPlayerData			*_playerSpecialtyData;
	int							_maxPlayers;

	Container<SpecialtyItem>	_specialtyItems;

	int							_infiltratorIconIndex;
	int							_medicIconIndex;
	int							_technicianIconIndex;
	int							_demolitionistIconIndex;
	int							_heavyweaponsIconIndex;
	int							_sniperIconIndex;

	void						putItemBack( Player *player );
	void						removeItem( MultiplayerItem *item );
	void						setupSpecialty( Player *player, SpecialtyType specialty, bool chosen );

	SpecialtyItem *				findSpecialtyItem( MultiplayerItem *item );
	void						attachBackpack( Player *player );

public:

								ModifierSpecialties();
								~ModifierSpecialties();

	 void			init( int maxPlayers );

	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );

	 bool			shouldKeepItem( MultiplayerItem *item );
	 bool			shouldKeepNormalItem( Item *item );
	 void			itemTouched( Player *player, MultiplayerItem *item );
	 void			addPlayer( Player *player );
	 void			removePlayer( Player *player );

	 void			playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );
	 void			playerSpawned( Player *player );

	 void			applySpeedModifiers( Player *player, int *moveSpeed );
	 void			applyJumpModifiers( Player *player, int *jumpSpeed );
	 void			applyAirAccelerationModifiers( Player *player, int *airAcceleration );

	 bool			canPickup( Player *player, MultiplayerItemType itemType, const char *item_name );

	 void			update( float frameTime );

	 int			getStat( Player *player, int statNum, int value );
	 int			getIcon( Player *player, int statNum, int value );
	 int			getScoreIcon( Player *player, int index, int value );

	 void			playerUsed( Player *usedPlayer, Player *usingPlayer, Equipment *equipment );

	 void			playerCommand( Player *player, const char *command, const char *parmName );

	 str			getSpawnPointType( Player *player );
	 float			getSpawnPointPriority( Player *player ) { return 100.0f; }

	 void			playerChangedModel( Player *player );
};

class HandicapPlayerData
{
public:
	float				_handicap;

						HandicapPlayerData() { _handicap = 1.0f; }
	void				reset( void ) { _handicap = 1.0f; }
};

class ModifierAutoHandicap : public MultiplayerModifier
{
private:
	static const float			_attackerHandicapModifier;
	static const float			_victimHandicapModifier;

	HandicapPlayerData			*_playerHandicapData;
public:
								ModifierAutoHandicap();
								~ModifierAutoHandicap();

	 void			init( int maxPlayers );
	 void			addPlayer( Player *player );

	 float			playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath );
	 void			playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );
};

class PointsPerWeaponData
{
public:
	str				_name;
	int				_points;
};

class ModifierPointsPerWeapon : public MultiplayerModifier
{
private:
	Container<PointsPerWeaponData>	_weapons;
	Container<PointsPerWeaponData>	_projectiles;

public:
								ModifierPointsPerWeapon() {};
								~ModifierPointsPerWeapon();

	 void			init( int maxPlayers );

	void						readMultiplayerConfig( const char *configName );
	bool						parseConfigToken( const char *key, Script *buffer );

	 int			getPointsForKill( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath, int points );
};

typedef enum
{
	CONTROL_POINT_NONE,
	CONTROL_POINT_ALPHA,
	CONTROL_POINT_BETA,
	CONTROL_POINT_DELTA,
	CONTROL_POINT_GAMMA
} ControlPointType;

struct ControlPointData
{
	MultiplayerItem *	_controlPoint;
	ControlPointType	_controlPointType;
	Team *				_controllingTeam;
	Team *				_lastControllingTeam;
	float				_lastTime;
	int					_lastPlayerToTouch;
	int					_playerPointsAwardedForCapture;
};

class ModifierControlPoints : public MultiplayerModifier
{
private:
	static const float			_timeNeededToControl;
	static const int			_maxNumberAwardsForPlayerCapture;
	static const int			_pointsForEachAwardForCapture;
	static const int			_pointsForProtectingControlPoint;
	static const int			_maxGuardingDist;

	Container<ControlPointData>	_controlPoints;

	int							_alphaControlPointRedControlledIndex;
	int							_alphaControlPointBlueControlledIndex;
	int							_alphaControlPointNeutralControlledIndex;

	int							_betaControlPointRedControlledIndex;
	int							_betaControlPointBlueControlledIndex;
	int							_betaControlPointNeutralControlledIndex;

	int							_deltaControlPointRedControlledIndex;
	int							_deltaControlPointBlueControlledIndex;
	int							_deltaControlPointNeutralControlledIndex;

	int							_gammaControlPointRedControlledIndex;
	int							_gammaControlPointBlueControlledIndex;
	int							_gammaControlPointNeutralControlledIndex;

	float						findNearestControlledControlPoint( const str &teamName, const Vector &position );
	ControlPointType			getControlPointType( const str &name );

public:
								ModifierControlPoints();
								~ModifierControlPoints();

	 void			init( int maxPlayers );

	 void			addPlayer( Player *player );
	 int			getStat( Player *player, int statNum, int value );

	 bool			shouldKeepItem( MultiplayerItem *item );
	 void			itemTouched( Player *player, MultiplayerItem *item );
	 void			playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );


	 void			update( float frameTime );

	 str			getSpawnPointType( Player *player );
	 float			getSpawnPointPriority( Player *player ) { return 10.0f; }

	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );
};

class ModifierInstantKill : public MultiplayerModifier
{
private:
	static const float			_instantKillDamage;

	static const float			_regenTime;
	static const int			_regenAmount;

	float						_lastRegenTime;

public:
								ModifierInstantKill();
								~ModifierInstantKill() {};

	 void			init( int maxPlayers );
	float						playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath );
	 bool			canGivePlayerItem( int entnum, const str &itemName );
	 bool			checkRule( const char *rule, bool defaultValue, Player *player = NULL );
	 void			playerSpawned( Player *player );
	 bool			shouldKeepNormalItem( Item *item );
	 void			update( float frameTime );
	 bool			skipWeaponReloads( void ) { return true; }
};

class ModifierActionHero : public MultiplayerModifier
{
private:
	static const float			_actionHeroRegenRate;
	static const int			_extraPointsForKillingActionHero;

	int							_actionHeroNum;
	float						_healthRegenRate;

	int							_actionHeroIconIndex;
	int							_actionHeroInfoIconIndex;

public:
								ModifierActionHero();
								~ModifierActionHero() {};
	 void			init( int maxPlayers );
	virtual void				playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath );
	virtual void				removePlayer( Player *player );
	virtual void				update( float frameTime );

	 void			matchStarting( void );

	 int			getIcon( Player *player, int statNum, int value );
	 int			getScoreIcon( Player *player, int index, int value );
	 int			getInfoIcon( Player *player );
};

#endif 
