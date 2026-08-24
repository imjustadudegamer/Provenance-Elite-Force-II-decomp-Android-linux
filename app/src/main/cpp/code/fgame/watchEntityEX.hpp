//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/watchEntityEX.hpp				              $
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
// watchEntityEX Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class WatchEntityEX;

#ifndef __WATCH_ENTITY_EX_HPP__
#define __WATCH_ENTITY_EX_HPP__

#include "behavior.h"
#include "behaviors_general.h"
#include "rotateToEntity.hpp"

class WatchEntityEX : public Behavior
	{
	public:  
		typedef enum
		{
			WATCH_ENTITY_EX_SETUP,
			WATCH_ENTITY_EX_ROTATE,
			WATCH_ENTITY_EX_HOLD,			
			WATCH_ENTITY_EX_SUCCESS,
			WATCH_ENTITY_EX_FAILED,
		} watchEntityStates_t;

	private:
		str								_stance;
		str								_shuffleAnim;
		str								_torsoAnim;
		EntityPtr						_ent;

	protected:
		void	transitionToState	( watchEntityStates_t state );
		void	setInternalState	( watchEntityStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();		
		
		void					setupStateSetup				();
		BehaviorReturnCode_t	evaluateStateSetup			();
		void					failureStateSetup			( const str& failureReason );

		void					setupStateRotate			();
		BehaviorReturnCode_t	evaluateStateRotate			();
		void					failureStateRotate			( const str& failureReason );

		void					setupStateHold				();
		BehaviorReturnCode_t	evaluateStateHold			();
		void					failureStateHold			( const str& failureReason );


	

	public:
		CLASS_PROTOTYPE( WatchEntityEX );

										WatchEntityEX();
									   ~WatchEntityEX();

		void							SetArgs					( Event *ev );      
		void							AnimDone				( Event *ev );

		void							Begin					( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate				( Actor &self ) override;
		void							End						( Actor &self ) override;
		
		void							SetEntity				( Entity *ent );

		virtual void					Archive  ( Archiver &arc ) override;

	private: 		
		RotateToEntity					_rotateToEntity;

	private: 
		unsigned int					_state;		
		float							_time;
		float							_turnspeed;	

	};

inline void WatchEntityEX::SetEntity( Entity *ent )
{
	if ( ent )
		_ent = ent;
}

inline void WatchEntityEX::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	arc.ArchiveString			( &_stance	  );
	arc.ArchiveString			( &_shuffleAnim	  );
	arc.ArchiveString			( &_torsoAnim );
	arc.ArchiveSafePointer		( &_ent		  );

	arc.ArchiveObject			( &_rotateToEntity			);

	arc.ArchiveUnsigned			( &_state					);
	arc.ArchiveFloat			( &_time					);
	arc.ArchiveFloat			( &_turnspeed				);
	
}



#endif 

