//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/teamArena.h                                    $
// $Revision:: 20                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 7/17/02 4:00p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
// 
// 
// Description:
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

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
