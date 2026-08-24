
#ifndef __MP_MODEDM_HPP__
#define __MP_MODEDM_HPP__

#include "mp_modeBase.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
class ModeDeathmatch : public MultiplayerModeBase
	{
	public:
		CLASS_PROTOTYPE( ModeDeathmatch );
											ModeDeathmatch();
		virtual								~ModeDeathmatch();
		
		 void					AddPlayer( Player *player ) override;

		 void					respawnPlayer( Player *player ) override;

		 void					init( int maxPlayers ) override;

		 bool					checkGameType( const char *rule ) override;

		 void					update( float frameTime ) override;

		 bool					checkRule( const char *rule, bool defaultValue, Player *player = NULL ) override;

	private:

		int									getHighestPoints( int entnum );
	};
#pragma GCC diagnostic pop

#endif 

