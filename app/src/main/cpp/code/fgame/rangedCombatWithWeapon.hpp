//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/rangedCombatWithWeapon.hpp					  $
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
// GeneralCombatWithMeleeWeapon Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class GeneralCombatWithRangedWeapon;

#ifndef __RANGEDCOMBAT_WITH_WEAPON_HPP__
#define __RANGEDCOMBAT_WITH_WEAPON_HPP__

#include "behavior.h"
#include "behaviors_general.h"
#include "generalCombatWithRangedWeapon.hpp"
#include "corridorCombatWithRangedWeapon.hpp"
#include "coverCombatWithRangedWeapon.hpp"
#include "selectBestWeapon.hpp"



class RangedCombatWithWeapon : public Behavior
	{
	public:  
		typedef enum
		{	
		RCWW_EVALUATE_OPTIONS,
		RCWW_CHANGE_WEAPON,		
		RCWW_GENERAL_COMBAT,
		RCWW_CORRIDOR_COMBAT,
		RCWW_COVER_COMBAT,
		RCWW_FAILED,
		} RangedCombatWithWeapon_t;


	private: 
		str                         _aimAnim;       
		str							_fireAnim;		
		float						_approachDist;
		float						_retreatDist;
		float						_strafeChance;
		float						_postureChangeChance;

	protected:		
		void	transitionToState	( RangedCombatWithWeapon_t state );
		void	setInternalState	( RangedCombatWithWeapon_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();
		void	updateEnemy			();	
		
		bool					checkShouldDoCorridorCombat();
		bool					checkShouldDoGeneralCombat();
		bool					checkShouldDoCoverCombat();
		bool					checkShouldCheckWeapon();

		void					setupStateEvaluateOptions();
		BehaviorReturnCode_t	evaluateStateEvaluateOptions();
		void					failureStateEvaluateOptions();

		void					setupStateChangeWeapon();
		BehaviorReturnCode_t	evaluateStateChangeWeapon();
		void					failureStateChangeWeapon();

		void					setupStateGeneralCombat();
		BehaviorReturnCode_t	evaluateStateGeneralCombat();
		void					failureStateGeneralCombat();

		void					setupStateCoverCombat();
		BehaviorReturnCode_t	evaluateStateCoverCombat();
		void					failureStateCoverCombat();

		void					setupStateCorridorCombat();
		BehaviorReturnCode_t	evaluateStateCorridorCombat();
		void					failureStateCorridorCombat();

		
	public:
		CLASS_PROTOTYPE( RangedCombatWithWeapon );

									RangedCombatWithWeapon();
								   ~RangedCombatWithWeapon();

		void						SetArgs              ( Event *ev      );		
		void                        AnimDone             ( Event *ev      );
		void						PostureDone			 ( Event *ev	  );

		void						Begin                ( Actor &self    ) override;
		BehaviorReturnCode_t		Evaluate             ( Actor &self    ) override;
		void						End                  ( Actor &self    ) override;

		virtual void		        Archive              ( Archiver &arc  ) override;

		void						SetFireAnim              ( const str &anim );
		
	


	private:
		CloseInOnEnemy					_gotoEnemy;
		GeneralCombatWithRangedWeapon	_generalCombat;
		CorridorCombatWithRangedWeapon	_corridorCombat;
		CoverCombatWithRangedWeapon		_coverCombat;
		SelectBestWeapon				_selectBestWeapon;


		int								_state;          
		Actor*							_self;
		EntityPtr						_currentEnemy;
		float							_nextSelectWeaponTime;
		float							_recheckTime;
		
	};


inline void RangedCombatWithWeapon::Archive( Archiver &arc	)
	{
	Behavior::Archive( arc );

	arc.ArchiveString	( &_aimAnim						);
	arc.ArchiveString	( &_fireAnim					);
	arc.ArchiveFloat	( &_approachDist				);
	arc.ArchiveFloat	( &_retreatDist					);
	arc.ArchiveFloat	( &_strafeChance				);
	arc.ArchiveFloat	( &_postureChangeChance			);

	arc.ArchiveObject  ( &_gotoEnemy          );
	arc.ArchiveObject  ( &_generalCombat      );
	arc.ArchiveObject  ( &_corridorCombat	  );
	arc.ArchiveObject  ( &_coverCombat		  );
	arc.ArchiveObject  ( &_selectBestWeapon	  );

	arc.ArchiveInteger ( &_state              );
	arc.ArchiveObjectPointer ( ( Class ** )&_self      );
	arc.ArchiveSafePointer( &_currentEnemy );
	arc.ArchiveFloat ( &_nextSelectWeaponTime );
	arc.ArchiveFloat ( &_recheckTime );
	}

inline void RangedCombatWithWeapon::SetFireAnim( const str &anim )
	{
	_fireAnim = anim;
	}

#endif 
