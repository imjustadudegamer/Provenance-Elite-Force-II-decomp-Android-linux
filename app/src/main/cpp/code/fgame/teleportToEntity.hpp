//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/teleportToEntity.h                               $
// $Revision:: 169                                                            $
//   $Author:: Bschofield                                                     $
//     $Date:: 4/26/02 2:22p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// TeleportToEntity Behavior Definition ( Behavior Name Needs To Be Changed )
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class AnimatedTeleportToPlayer;

#ifndef __TELEPORT_TO_ENTITY_H__
#define __TELEPORT_TO_ENTITY_H__

#include "behavior.h"


class AnimatedTeleportToPlayer : public Behavior
	{
	public:
		typedef enum
			{
			ANIM_TELEPORT_BEGIN,
			ANIM_TELEPORT_START_ANIM,
			ANIM_TELEPORT_START_ANIMATING,
			ANIM_TELEPORT_TELEPORT,
			ANIM_TELEPORT_END_ANIM,
			ANIM_TELEPORT_END_ANIMATING,
			} animTeleportStates_t;
		
		typedef enum
			{
			TELEPORT_BEHIND,
			TELEPORT_TOLEFT,
			TELEPORT_TORIGHT,
			TELEPORT_INFRONT,
			TELEPORT_NUMBER_OF_POSITIONS
			} animTeleportPositionModes_t;

	private:
      str						_startAnim;
      str						_endAnim;

	protected:	
		bool					testPosition ( Actor &self, int test_pos, Vector &good_position, Entity* player, bool use_player_dir );


	public:
		CLASS_PROTOTYPE( AnimatedTeleportToPlayer );

		void					SetArgs  ( Event *ev      );		
		void					Begin    ( Actor &self    ) override;
		
		BehaviorReturnCode_t	Evaluate ( Actor &self    ) override;
		void					End      ( Actor &self    ) override;
		virtual void			Archive  ( Archiver &arc ) override;

	private:		
		Vector					_teleportPosition;
		animTeleportStates_t	_state;
	};

inline void AnimatedTeleportToPlayer::Archive( Archiver &arc )
{
	Behavior::Archive ( arc );

	arc.ArchiveString (  &_startAnim                 );
	arc.ArchiveString (  &_endAnim                   );

	arc.ArchiveVector ( &_teleportPosition           );
	ArchiveEnum       ( _state, animTeleportStates_t );
}



#endif 
