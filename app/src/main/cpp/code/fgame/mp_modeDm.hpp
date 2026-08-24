//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/multiplayerArena.h                             $
// $Revision:: 38                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 7/23/02 3:55p                                                  $
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

