
#ifndef __MP_MODETEAMDM_HPP__
#define __MP_MODETEAMDM_HPP__

#include "mp_modeTeamBase.hpp" 

class ModeTeamDeathmatch : public ModeTeamBase
	{
	private:
		Team*                               _redTeam ;
		Team*                               _blueTeam ;

	protected:

	public:
		CLASS_PROTOTYPE( ModeTeamDeathmatch );
											ModeTeamDeathmatch();
											~ModeTeamDeathmatch();

		 void					init( int maxPlayers ) override;

		void								playerKilled( Player *killedPlayer, Player *attackingPlayer, Entity *inflictor, int meansOfDeath ) override;
		bool								isEndOfMatch( void ) override;
		 int					getTeamPoints( Player *player ) override;

		 void					setupMultiplayerUI( Player *player ) override;

		 void					score( const Player *player ) override;

		 bool					checkGameType( const char *rule ) override;
	};

#endif 
