//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/doAttack.hpp                                   $
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
// DoAttack Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class RotateToEntity;

#ifndef __DO_ATTACK_H__
#define __DO_ATTACK_H__

#include "behavior.h"
#include "rotateToEntity.hpp"

class DoAttack : public Behavior
	{
	public:  
		typedef enum
		{
		ATTACK_STATE_SETUP,
		ATTACK_STATE_ROTATE,
		ATTACK_STATE_START_ANIM,
		ATTACK_STATE_ANIMATING,
		ATTACK_STATE_COMPLETE,
		ATTACK_STATE_FAILED   
		} attackStates_t;

	private: 
		str								_anim;
		float							_turnspeed; 
		bool							_forceAttack;
		str								_rotateAnim;
      
	protected:
		void							_setupRotate    ( Actor &self );
		void							_rotate         ( Actor &self );
		void							_playAttackAnim ( Actor &self );

		bool							_canAttack      ( Actor &self );
		void							init			( Actor &self );

   public:
      CLASS_PROTOTYPE( DoAttack );

		void							SetArgs  ( Event *ev     );      
		void							AnimDone ( Event *ev     );
		void							Begin    ( Actor &self   ) override;		
		BehaviorReturnCode_t			Evaluate ( Actor &self   ) override;
		void							End      ( Actor &self   ) override;
		virtual void					Archive  ( Archiver &arc ) override;

		void							SetAnim      ( const str &animName );
		void							SetTurnSpeed ( float turnSpeed     );
		void							SetForceAttack ( bool force        );
		void							SetRotateAnim ( const str &animName );

	private:
		unsigned int                     _state;
		RotateToEntity                   _rotateBehavior;	  
	};


inline void DoAttack::SetAnim( const str &animName )
{
	_anim = animName;
}

inline void DoAttack::SetTurnSpeed( float turnSpeed )
{
	_turnspeed = turnSpeed;
}

inline void DoAttack::SetForceAttack( bool force )
{
	_forceAttack = force;
}

inline void DoAttack::SetRotateAnim( const str &animName )
{
	_rotateAnim = animName;
}

inline void DoAttack::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );	    

	arc.ArchiveString		( &_anim            );
	arc.ArchiveFloat		( &_turnspeed       );
	arc.ArchiveBool			( &_forceAttack     );
	arc.ArchiveString		( &_rotateAnim      );


	arc.ArchiveUnsigned		( &_state          );
	arc.ArchiveObject		( &_rotateBehavior );	
}

#endif 
