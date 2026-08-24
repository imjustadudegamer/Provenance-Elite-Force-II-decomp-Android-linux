//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/closeInOnEnemy.hpp				              $
// $Revision:: 169                                                            $
//   $Author:: sketcher														  $
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
// CloseInOnEnemy Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class CloseInOnPlayer;

#ifndef __CLOSE_IN_ON_PLAYER_HPP__
#define __CLOSE_IN_ON_PLAYER_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class CloseInOnPlayer : public Behavior
{
	public:
		typedef enum
		{	
			CLOSE_IN_ON_PLAYER_APPROACH,
			CLOSE_IN_ON_PLAYER_SUCCESS,
			CLOSE_IN_ON_PLAYER_FAILED
		} closeInOnPlayerStates_t;

	private: 
		str							_anim;  
		str							_torsoAnim;
		float						_dist;

	protected:
		void	transitionToState	( closeInOnPlayerStates_t state );
		void	setInternalState	( closeInOnPlayerStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();
		void	findPlayer			();
		void	setTorsoAnim		();	

		void					setupStateApproach		();
		BehaviorReturnCode_t	evaluateStateApproach	();
		void					failureStateApproach	( const str& failureReason );

	public:
		CLASS_PROTOTYPE( CloseInOnPlayer );

												CloseInOnPlayer();
											   ~CloseInOnPlayer();

		void									SetArgs		( Event *ev     );      
		void									Begin		( Actor &self   ) override;		
		BehaviorReturnCode_t					Evaluate	( Actor &self   ) override;
		void									End			( Actor &self   ) override;
		virtual void							Archive		( Archiver &arc ) override;

		void									setAnim		( const str &animName );
		void									setTorsoAnim( const str &animName );
		void									setDist		( float distance      );

	private:      
		GotoEntity								_chase;

	private: 
		closeInOnPlayerStates_t					_state;
		EntityPtr								_player;
		Actor								   *_self;

};

inline void CloseInOnPlayer::setAnim( const str &animName )
{
	_anim = animName;
}

inline void CloseInOnPlayer::setTorsoAnim( const str &animName )
{
	_torsoAnim = animName;
}

inline void CloseInOnPlayer::setDist( float distance )
{
	_dist = distance;
}

inline void CloseInOnPlayer::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );	 
   
	arc.ArchiveString ( &_anim );
	arc.ArchiveString ( &_torsoAnim );
	arc.ArchiveFloat  ( &_dist );

	arc.ArchiveObject ( &_chase );

	ArchiveEnum					( _state, closeInOnPlayerStates_t);
	arc.ArchiveSafePointer		( &_player						);
	arc.ArchiveObjectPointer	( ( Class ** )&_self			);
}  

#endif 
