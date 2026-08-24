//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/healGroupMember.hpp				              $
// $Revision:: 1                                                              $
//   $Author:: Sketcher                                                       $
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
// watchEntity Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class WatchEntity;

#ifndef __WATCH_ENTITY_HPP__
#define __WATCH_ENTITY_HPP__

#include "behavior.h"
#include "behaviors_general.h"
#include "rotateToEntity.hpp"

class WatchEntity : public Behavior
	{
	public:  
		typedef enum
		{
			WATCH_HOLD,
			WATCH_ROTATE,
			WATCH_SUCCESS,
			WATCH_FAILED
		} watchEntityStates_t;

	private:
		float							_time;
		float							_turnspeed;
		float							_oldTurnSpeed;
		str								_anim;
		EntityPtr						_ent;
		unsigned int					_waitForAnim;
		str								_holdAnim;
		bool							_forcePlayer;

	protected:
		void	transitionToState	( watchEntityStates_t state );
		void	setInternalState	( watchEntityStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();				
		
		
		void							setupStateRotate			();
		BehaviorReturnCode_t			evaluateStateRotate			( Actor &self );
		void							rotateFailed				( Actor &self );

		void							setupStateHold				();
		BehaviorReturnCode_t			evaluateStateHold			( Actor &self );
		void							holdFailed					( Actor &self );

	public:
		CLASS_PROTOTYPE( WatchEntity );

										WatchEntity();
									   ~WatchEntity();

		void							SetArgs					( Event *ev );      
		void							AnimDone				( Event *ev );

		void							Begin					( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate				( Actor &self ) override;
		void							End						( Actor &self ) override;
		
		void							SetEntity				( Entity *ent );

		virtual void					Archive  ( Archiver &arc ) override;


	private: 
		unsigned int					_state;	
		unsigned int					_animDone;
		
	};

inline void WatchEntity::SetEntity( Entity *ent )
{
	if ( ent )
		_ent = ent;
}

inline void WatchEntity::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	arc.ArchiveFloat			( &_time );
	arc.ArchiveFloat			( &_turnspeed );
	arc.ArchiveFloat			( &_oldTurnSpeed );
	arc.ArchiveString			( &_anim );
	arc.ArchiveSafePointer		( &_ent );
	arc.ArchiveUnsigned			( &_waitForAnim );
	arc.ArchiveString			( &_holdAnim );
	arc.ArchiveBool				( &_forcePlayer );

	arc.ArchiveUnsigned			( &_state					);	
	arc.ArchiveUnsigned			( &_animDone				);	
}



#endif 

