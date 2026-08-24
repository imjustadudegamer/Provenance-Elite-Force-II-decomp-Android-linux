//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/work.hpp										  $
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
// CooridorCombatWithRangedWeapon Behavior Definition
//
//--------------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class Work;

#ifndef __WORK_HPP__
#define __WORK_HPP__

#include "behavior.h"
#include "behaviors_general.h"
#include "gotoHelperNode.hpp"

class Work : public Behavior
	{
	public:
		typedef enum
		{
		WORK_FIND_NODE,
		WORK_MOVE_TO_NODE,
		WORK_AT_NODE,		
		WORK_ANIMATE_WAIT_ON_TIME,
		WORK_ANIMATE_WAIT_ON_ANIM,
		WORK_ANIMATE_WAIT_ON_SIGNAL,
		WORK_ANIMATE_CONSTANT,
		WORK_ANIMATE_LIST_WAIT_ON_TIME,
		WORK_ANIMATE_LIST_WAIT_ON_ANIM,
		WORK_ANIMATE_LIST_WAIT_ON_SIGNAL,
		WORK_SELECT_ANIM_MODE,
		WORK_SUCCESSFUL,
		WORK_FAILED
		} workStates_t;

	private: 
		str							_gotoWorkAnimName ; 
		float						_maxDistance;	    

	protected:
		void	transitionToState	( workStates_t state );
		void	setInternalState	( workStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();		

		void					setupStateFindNode						();
		BehaviorReturnCode_t	evaluateStateFindNode					();
		void					failureStateFindNode					( const str& failureReason );

		void					setupStateMoveToNode					();
		BehaviorReturnCode_t	evaluateStateMoveToNode					();
		void					failureStateMoveToNode					( const str& failureReason );

		void					setupStateAtNode						();
		BehaviorReturnCode_t	evaluateStateAtNode						();
		void					failureStateAtNode						( const str& failureReason );
		
		void					setupStateAnimateWaitOnTime				();
		BehaviorReturnCode_t	evaluateStateAnimateWaitOnTime			();
		void					failureStateAnimateWaitOnTime			( const str& failureReason );

		void					setupStateAnimateWaitOnAnim				();
		BehaviorReturnCode_t	evaluateStateAnimateWaitOnAnim			();
		void					failureStateAnimateWaitOnAnim			( const str& failureReason );

		void					setupStateAnimateWaitOnSignal			();
		BehaviorReturnCode_t	evaluateStateAnimateWaitOnSignal		();
		void					failureStateAnimateWaitOnSignal			( const str& failureReason );

		void					setupStateAnimateConstant				();
		BehaviorReturnCode_t	evaluateStateAnimateConstant			();
		void					failureStateAnimateConstant				( const str& failureReason );

		void					setupStateAnimateListWaitOnTime			();
		BehaviorReturnCode_t	evaluateStateAnimateListWaitOnTime		();
		void					failureStateAnimateListWaitOnTime		( const str& failureReason );

		void					setupStateAnimateListWaitOnAnim			();
		BehaviorReturnCode_t	evaluateStateAnimateListWaitOnAnim		();
		void					failureStateAnimateListWaitOnAnim		( const str& failureReason );

		void					setupStateAnimateListWaitOnSignal		();
		BehaviorReturnCode_t	evaluateStateAnimateListWaitOnSignal	();
		void					failureStateAnimateListWaitOnSignal		( const str& failureReason );


	public:
		CLASS_PROTOTYPE( Work );
												Work();
											   ~Work();

		void									SetArgs				( Event *ev );      
		void									AnimDone			( Event *ev );
		void									HandleNodeCommand	( Event *ev );

		void									Begin				( Actor &self ) override;		
		BehaviorReturnCode_t					Evaluate			( Actor &self ) override;
		void									End					( Actor &self ) override;

		void									SetNode	( HelperNode* node );
		virtual void							Archive  ( Archiver &arc ) override;

	private: 
		GotoHelperNode					_gotoHelperNode;

	private: 
		HelperNodePtr							_node;            
		unsigned int							_state;
		bool									_animDone;
		float									_endTime;
		Actor*									_self;
		
	};

inline void Work::SetNode( HelperNode *node )
{
	_node = node;
}

inline void Work::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	    
   
	arc.ArchiveString			( &_gotoWorkAnimName);
	arc.ArchiveFloat			( &_maxDistance     );

	arc.ArchiveObject			( &_gotoHelperNode  );
	

	arc.ArchiveSafePointer		( &_node				);
	arc.ArchiveUnsigned			( &_state				);
	arc.ArchiveBool				( &_animDone			);
	arc.ArchiveFloat			( &_endTime				);
	arc.ArchiveObjectPointer	( ( Class ** )&_self	);
}

#endif 
