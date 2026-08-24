//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/healGroupMember.hpp				              $
// $Revision:: 1                                                            $
//   $Author:: Sketcher                                                     $
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
// healGroupMember Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class HealGroupMember;

#ifndef __HEAL_GROUP_MEMBER_HPP__
#define __HEAL_GROUP_MEMBER_HPP__

#include "behavior.h"
#include "behaviors_general.h"
#include "rotateToEntity.hpp"

class HealGroupMember : public Behavior
	{
	public:  
		typedef enum
		{			
		HGM_STATE_GOTO_ENTITY,		
		HGM_STATE_FACE_TARGET,
		HGM_STATE_ANIMATE,
		HGM_STATE_HEAL,
		HGM_STATE_FAILED,
		HGM_STATE_SUCCESS,
		} healGroupMemberStates_t;

		typedef struct
		{
		EntityPtr	ent;
		float		health;
		float		dist;
		bool		treated;
		} triageEntry_t;

	private:
		str								_anim;		
		float							_healDistance;
		float							_maxDistance;	
		float							_initialHealPercentage;
		float							_regenHealPercentage;
		float							_regenInterval;
		float							_maxPercentage;

	protected:
		void							init						( Actor &self );
		void							doHeal						( Actor &self );		

		void							setupGotoEntity				( Actor &self );
		void							doGotoEntity				( Actor &self );
		void							gotoEntityFailed			( Actor &self );

		void							setupRotateToEntity			( Actor &self );
		void							doRotateToEntity			( Actor &self );
		void							rotateToEntityFailed		( Actor &self );

		void							setupAnimate				( Actor &self );
		void							doAnimate					( Actor &self );
		void							animateFailed				( Actor &self );

		void							updateTriageList			( Actor &self );
		void							updateTriageEntry			( Actor &self , Entity *ent );
		bool							allPatientsTreated			( Actor &self );
		bool							patientTreated				( Actor &self , Entity *ent );
		void							treatedPatient				( Actor &self , Entity *ent );
		Actor*							findHighestPriorityPatient	( Actor &self );
	

	public:
		CLASS_PROTOTYPE( HealGroupMember );

										HealGroupMember();
									   ~HealGroupMember();

		void							SetArgs					( Event *ev );      
		void							AnimDone				( Event *ev );

		void							Begin					( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate				( Actor &self ) override;
		void							End						( Actor &self ) override;
		
		virtual void					Archive  ( Archiver &arc ) override;

	private: 
		GotoEntity						_gotoEntity;
		RotateToEntity					_rotateToEntity;

	private: 
		unsigned int					_state;	
		ActorPtr						_currentPatient;
		str								_legAnim;
		float							_nextTriageUpdate;
		Container<triageEntry_t*>		_triageList;
		

	};


inline void HealGroupMember::Archive( Archiver &arc	)
{
	int num , i;
	triageEntry_t* checkEntry;

	Behavior::Archive ( arc );	     

	arc.ArchiveString			( &_anim					);	
	arc.ArchiveFloat			( &_healDistance			);
	arc.ArchiveFloat			( &_maxDistance				);
	arc.ArchiveFloat			( &_initialHealPercentage	);
	arc.ArchiveFloat			( &_regenHealPercentage		);
	arc.ArchiveFloat			( &_regenInterval			);
	arc.ArchiveFloat			( &_maxPercentage			);

	arc.ArchiveObject			( &_gotoEntity				);
	arc.ArchiveObject			( &_rotateToEntity			);

	arc.ArchiveUnsigned			( &_state					);
	arc.ArchiveSafePointer		( &_currentPatient			);
	arc.ArchiveString			( &_legAnim					);
	arc.ArchiveFloat			( &_nextTriageUpdate		);
	
	if ( arc.Saving() )
		{
		num = _triageList.NumObjects();
		arc.ArchiveInteger( &num );

		for ( i = num ; i > 0 ; i-- )
			{
			checkEntry = _triageList.ObjectAt( i );
			arc.ArchiveSafePointer( &checkEntry->ent );
			arc.ArchiveFloat( &checkEntry->health );
			arc.ArchiveFloat( &checkEntry->dist );
			arc.ArchiveBool( &checkEntry->treated );		
			}
		}
	else
		{
		arc.ArchiveInteger( &num );
		_triageList.ClearObjectList();
		_triageList.Resize( num );

		for ( i = 1 ; i<= num ; i++ )
			{
			checkEntry = new triageEntry_t;			
			arc.ArchiveSafePointer( &checkEntry->ent);
			arc.ArchiveFloat( &checkEntry->health );
			arc.ArchiveFloat( &checkEntry->dist );
			arc.ArchiveBool( &checkEntry->treated );			
			}

		}
}



#endif 

