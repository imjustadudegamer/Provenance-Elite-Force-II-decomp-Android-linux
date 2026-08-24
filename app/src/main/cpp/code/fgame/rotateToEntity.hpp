//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/rotateToEntity.h                               $
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
// RotateToEntity Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class RotateToEntity;

#ifndef __ROTATE_TO_ENTITY_H__
#define __ROTATE_TO_ENTITY_H__

#include "behavior.h"

class RotateToEntity : public Behavior
	{

	private:      
		float							_turnspeed;
		EntityPtr						_ent;
		str								_anim;
		str								_entityType;

	public:
		CLASS_PROTOTYPE( RotateToEntity );

										RotateToEntity();
									   ~RotateToEntity();


		void							SetArgs      ( Event *ev                          );      
		void							Begin        ( Actor &self                        ) override;		
		BehaviorReturnCode_t			Evaluate     ( Actor &self                        ) override;
		void							End          ( Actor &self                        ) override;
		virtual void					Archive      ( Archiver &arc                      ) override;
		
		void							SetAnim      ( const str& animName                );
		void							SetTurnSpeed ( float turnSpeed                    );
		void							SetEntity    ( Entity *entity                     );      
		void							SetEntityType( const str &ent					  );

	private:		
		float							_oldTurnSpeed;
		
	
	};

inline void RotateToEntity::SetTurnSpeed( float turnSpeed )
	{
	_turnspeed = turnSpeed;
	}


inline void RotateToEntity::SetEntity( Entity *entity )
	{
	if ( entity )
		_ent = entity;
	}

inline void RotateToEntity::SetAnim( const str &animName )
	{
	_anim = animName;   
	}

inline void RotateToEntity::SetEntityType( const str &ent )
	{
		if  ( ent == "player" || ent == "enemy" )
			_entityType = ent;
	}

inline void RotateToEntity::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );

	arc.ArchiveFloat       ( &_turnspeed );
	arc.ArchiveSafePointer ( &_ent       );
	arc.ArchiveString      ( &_anim      );
	arc.ArchiveString( &_entityType );

	arc.ArchiveFloat	   ( &_oldTurnSpeed );
}


#endif 
