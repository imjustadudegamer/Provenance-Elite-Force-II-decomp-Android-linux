//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/holdPosition.hpp		              $
// $Revision:: 169                                                            $
//   $Author:: sketcher                                                       $
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
// holdPosition Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


class HoldPosition;

#ifndef __HOLD_POSITION__
#define __HOLD_POSITION__

#include "behavior.h"
#include "behaviors_general.h"

class HoldPosition : public Behavior
	{
	public:  
		typedef enum
		{
			HOLD_POSITION_HOLD,
			HOLD_POSITION_TWITCH,			
			HOLD_POSITION_WEAPON_TWITCH,
			HOLD_POSITION_SUCCESS,
			HOLD_POSITION_FAILED
		} HoldPositionStates_t;

	private:

		
	protected:
		void	transitionToState	( HoldPositionStates_t state );
		void	setInternalState	( HoldPositionStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();		

		void					setupStateHold				();
		BehaviorReturnCode_t	evaluateStateHold			();
		void					failureStateHold			( const str& failureReason );

		void					setupStateTwitch			();
		BehaviorReturnCode_t	evaluateStateTwitch			();
		void					failureStateTwitch			( const str& failureReason );

		void					setupStateWeaponTwitch		();
		BehaviorReturnCode_t	evaluateStateWeaponTwitch	();
		void					failureStateWeaponTwitch	( const str& failureReason );


	public:
		CLASS_PROTOTYPE( HoldPosition );

										HoldPosition();
									   ~HoldPosition();

		void							SetArgs			( Event *ev );      
		void							AnimDone		( Event *ev );
	
		void							Begin			( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate		( Actor &self ) override;
		void							End				( Actor &self ) override;

		virtual void					Archive  ( Archiver &arc ) override;

	private: 	
		str								_legAnim;
		str								_torsoAnim;
		str								_twitchAnim;
		str								_weaponTwitchAnim;

		float							_holdTimeMin;
		float							_holdTimeMax;
		float							_endHoldTime;
		float							_nextTwitchTime;
		float							_twitchInterval;
		bool							_canTwitch;
		bool							_animDone;
		

	private: 
		HoldPositionStates_t			_state;	
		Actor						   *_self;		
		

	};

inline void HoldPosition::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	

	ArchiveEnum				( _state, HoldPositionStates_t	);
	arc.ArchiveObjectPointer( ( Class ** )&_self				);

	arc.ArchiveString( &_legAnim );
	arc.ArchiveString( &_torsoAnim );
	arc.ArchiveString( &_twitchAnim );
	arc.ArchiveString( &_weaponTwitchAnim );
	arc.ArchiveFloat( &_holdTimeMin );
	arc.ArchiveFloat( &_holdTimeMax );
	arc.ArchiveFloat( &_endHoldTime );
	arc.ArchiveFloat( &_nextTwitchTime );
	arc.ArchiveFloat( &_twitchInterval );
	arc.ArchiveBool ( &_canTwitch );
	arc.ArchiveBool ( &_animDone );
}


#endif 

