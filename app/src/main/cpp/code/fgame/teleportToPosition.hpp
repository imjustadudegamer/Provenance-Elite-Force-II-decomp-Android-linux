//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/teleportToPosition.h                           $
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
// TeleportToPosition Behavior Definition ( Behavior Name Needs To Be Changed )
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class AnimatedTeleportToPosition;

#ifndef __TELEPORT_TO_POSITION_H__
#define __TELEPORT_TO_POSITION_H__

#include "behavior.h"


class AnimatedTeleportToPosition : public Behavior
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

	private:
		str						_teleportPositionName;
		int						_numberOfTeleportPositions;
		str						_startAnim;
		str						_endAnim;


	public:
		CLASS_PROTOTYPE( AnimatedTeleportToPosition );

		void					SetArgs( Event *ev );
		void					Begin( Actor &self ) override;
		BehaviorReturnCode_t	Evaluate( Actor &self ) override;
		void					End( Actor &self ) override;
		virtual void			Archive( Archiver &arc ) override;

	private:		
		animTeleportStates_t	_state;
		PathNodePtr				_goal;

	};

inline void AnimatedTeleportToPosition::Archive( Archiver &arc )
{
	Behavior::Archive( arc );

	arc.ArchiveString		( &_teleportPositionName	    );
	arc.ArchiveInteger		( &_numberOfTeleportPositions   );
	arc.ArchiveString		( &_startAnim				    );
	arc.ArchiveString		( &_endAnim				        );

	ArchiveEnum				(  _state, animTeleportStates_t );
	arc.ArchiveSafePointer	( &_goal					    );
}



#endif 
