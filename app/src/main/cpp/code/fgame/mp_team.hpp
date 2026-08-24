
#include "_pch_cpp.h"

#include "mp_shared.hpp"


#ifndef __MP_TEAM_HPP__
#define __MP_TEAM_HPP__

#include "g_local.h"          
#include "player.h"           
#include "item.h"
#include "playerstart.h"      
#include "container.h"        
#include "str.h"              

class SimpleAmmoType : public Class
	{
	public:
		SimpleAmmoType() : type(""), amount(0) { } 
		SimpleAmmoType(const str& ammoType, int ammoAmount) : type(ammoType), amount(ammoAmount) { }
		str   type;
		int   amount;
	};

class Team : public Class
	{
	private:
		Container<Player*>                  _playerList ;       
		Container<str>                      _weaponList ;       
		Container<SimpleAmmoType>           _ammoList ;         
		Container<PlayerDeathmatchStart*>   _spawnpointList;    
		unsigned int                        _maxPlayers ;       
		unsigned int                        _startingHealth ;   
		unsigned int                        _wins ;             
		unsigned int                        _losses ;           
		unsigned int                        _kills ;            
		int									_points;
		unsigned int                        _deaths ;           
		bool                                _isFighting ;       
		str                                 _name ;             

	protected:
		void                                _giveInitialConditions(Player *player); 
		void                                _activatePlayer(Player* player);        
		void                                _deactivatePlayer(Player* player);      

	public:
		CLASS_PROTOTYPE( Team );
		Team(const str& name="Unnamed Team");
		virtual ~Team();

		bool                                isStartingHealth()                  { return (_startingHealth > 0) ? true : false ; }
		bool                                isFighting()                        { return _isFighting ; }

		const str&                          getName()                           { return _name ; }
		unsigned int                        getKills()                          { return _kills ; }
		unsigned int                        getDeaths()                         { return _deaths ; }
		int			                        getPoints()                         { return _points ; }
		unsigned int                        getWins()                           { return _wins ; }
		unsigned int                        getLosses()                         { return _losses ; }
		unsigned int                        getMaxPlayers()                     { return _maxPlayers ; }
		unsigned int                        getActivePlayers()                  { return _playerList.NumObjects(); }

		void                                setStartingHealth(unsigned int h)   { _startingHealth = h ; }
		void                                setMaxPlayers(unsigned int players) { _maxPlayers = players ; }
		
		void                                BeginMatch();
		void                                EndMatch();

		void                                AddPlayer(Player* player);
		void                                RemovePlayer(Player* player);

		void                                AddStartingWeapon(const str& weaponName);
		void                                AddStartingAmmo( const SimpleAmmoType &ammo);
		void                                AddSpawnpoint(PlayerDeathmatchStart* spawnpoint);

		void                                AddKill(Player* killer, Player* victim=NULL);  
		void                                AddDeath(Player* victim, Player* killer=NULL); 
		void								addPoints( Player *killer, int points );
		void                                Win();
		void                                Lose();
};

#endif 
