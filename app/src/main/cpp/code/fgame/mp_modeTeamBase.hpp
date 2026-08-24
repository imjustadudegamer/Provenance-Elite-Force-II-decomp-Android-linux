

#ifndef __MP_MODETEAMBASE_HPP__
#define __MP_MODETEAMBASE_HPP__

#include "mp_modeBase.hpp"

class ModeTeamBase : public MultiplayerModeBase
	{
	protected:
		Container<Team *>					_teamList;      
		unsigned int                        _maxTeams ;     
		Team*								_leadTeam ;     
		bool								_useTeamSpawnpoints;

		int									_redTeamIconIndex;
		int									_blueTeamIconIndex;

		int									_redTeamHudIconIndex;
		int									_blueTeamHudIconIndex;

		int									_redTeamSpectatorHudIconIndex;
		int									_blueTeamSpectatorHudIconIndex;

											ModeTeamBase();
	
		 void					init( int maxPlayers ) override;

		 void					declareWinner( void ) override;

		 void					_endMatch() override;    
		 void                  _beginMatch() override;  

		Team*                               _findTeamByName(const str& teamName);
		void                                _deleteTeams();
		void                                _deleteTeam(Team* team);
		void                                _addTeamStartingAmmo(Team* team, const SimpleAmmoType &ammo);
		void                                _addTeamStartingWeapon(Team* team, const str& weapon);
		void                                _setTeamStartingHealth(Team* team, unsigned int startingHealth);

		void								updatePlayerSkin( Player *player );

	public:
		CLASS_PROTOTYPE( ModeTeamBase );
		virtual	~ModeTeamBase();

		 bool					isEndOfMatch() override;                            

		unsigned int                        getMaxTeams()                              { return _maxTeams ; }
		Team*                               getLeadTeam()                              { return _leadTeam ; }

		 int					getIcon( Player *player, int statNum, int value ) override;
		 int					getInfoIcon( Player *player ) override;

		virtual void                        SetMaxTeams(unsigned int maxTeams)         { _maxTeams = maxTeams ; }
		virtual void                        SetTeamStartingHealth(const str& teamName, unsigned int startingHealth);

		 bool					canJoinTeam( Player *player, const str &teamName ) override;
		 void					joinTeam( Player *player, const str &teamName ) override;


		 void					AddPlayer( Player *player ) override;                    
		 void					RemovePlayer( Player *player ) override;

		 float					playerDamaged( Player *damagedPlayer, Player *attackingPlayer, float damage, int meansOfDeath ) override;

		virtual void						addPlayerToTeam( Player *player, Team *team );
		void								changeTeams( Player *player, Team *team );

		 void					respawnPlayer( Player *player ) override;
		 Entity *				getSpawnPoint( Player *player ) override;

		virtual void                        AddTeamStartingWeapon(const str& teamName, const str& startingWeapon);
		virtual void                        AddTeamStartingAmmo(const str& teamName,    const str& ammoName, int amount);
		virtual Team*                       AddTeam(const str& name);
		Team *								getTeam( const str & teamName );
		virtual void                        RemoveTeam(const str& name);

		int									getTeamPoints( Player *player ) override;
		int									getTeamPoints( const str & teamName ) override;
		void								addTeamPoints( const str & teamName, int points ) override;

		void								teamPointsChanged( Team *team, int oldPoints, int newPoints ) override;

		 int					getHighestPoints( void ) override;

		 void					playerChangedModel( Player *player ) override;
	};

#endif 
