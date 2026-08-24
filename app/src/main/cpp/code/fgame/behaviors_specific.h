//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/behaviors_specific.h                           $
// $Revision:: 12                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:53a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Specific Behaviors will go in this file.  Specific, meaning, behaviors
// that are meant to be used by specific actors
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __BEHAVIORS_SPECIFIC_H__
#define __BEHAVIORS_SPECIFIC_H__

#include "behavior.h"
#include "behaviors_general.h"
#include "work.hpp"
#include "patrol.hpp"
#include "helper_node.h"
#include "actor.h"


class PatrolWorkHibernate : public Behavior
   {
	private: 

   public:  
      enum
      {
      PATROLWORKHIBERNATE_SELECT_STATE,
      PATROLWORKHIBERNATE_PATROL,
      PATROLWORKHIBERNATE_WORK,
      PATROLWORKHIBERNATE_HIBERNATE,
      PATROLWORKHIBERNATE_HOLD,
	  PATROLWORKHIBERNATE_HIBERNATE_HOLD,
      PATROLWORKHIBERNATE_FAILED,
	  PATROLWORKHIBERNATE_HACK
      };

   protected:
      void                             _init             ( Actor &self );          
      void                             _selectState      ( Actor &self );      

      void                             _setupPatrol      ( Actor &self );      
      void                             _patrol           ( Actor &self );
      
      void                             _setupHibernate   ( Actor &self );
      void                             _hibernate        ( Actor &self );
      
      void                             _setupWork        ( Actor &self );
      void                             _work             ( Actor &self );

      void                             _setupHold        ( Actor &self );
      void                             _hold             ( Actor &self );

      bool                             _wantsToWork      ( Actor &self );
      bool                             _wantsToHibernate ( Actor &self );
      
      
   public:
      CLASS_PROTOTYPE( PatrolWorkHibernate );

		void						            SetArgs  ( Event *ev );      
      void                             AnimDone ( Event *ev );

		void						            Begin    ( Actor &self ) override;
		BehaviorReturnCode_t					Evaluate ( Actor &self ) override;
		void						            End      ( Actor &self ) override;

		virtual void                     Archive  ( Archiver &arc ) override;

   private: 
      Patrol                           _patrolComponent;
      Work                             _workComponent;
      Hibernate                        _hibernateComponent;

   private:
      unsigned int                     _state;
      HelperNodePtr                    _lastWorkNodeChecked;
      HelperNodePtr                    _lastHibernateNodeChecked;      

      float                            _nextStateCheck;
      float                            _nextWorkCheck;
      float                            _nextHibernateCheck;

      static const float               MIN_DISTANCE_TO_PATROL_NODE;
      static const float               MIN_DISTANCE_TO_WORK_NODE;
      static const float               MIN_DISTANCE_TO_HIBERNATE_NODE;
      
   };

inline void PatrolWorkHibernate::Archive( Archiver &arc	)
   {
   Behavior::Archive ( arc );	    
   

   arc.ArchiveObject ( &_patrolComponent );
   arc.ArchiveObject ( &_workComponent );
   arc.ArchiveObject ( &_hibernateComponent );

   arc.ArchiveUnsigned( &_state );
   arc.ArchiveSafePointer( &_lastWorkNodeChecked );
   arc.ArchiveSafePointer( &_lastHibernateNodeChecked );

   arc.ArchiveFloat( &_nextStateCheck );
   arc.ArchiveFloat( &_nextWorkCheck );
   arc.ArchiveFloat( &_nextHibernateCheck );
   }


class PatrolWorkWander : public Behavior
{
	private: 

	public:  
		enum
			{
			PATROLWORKWANDER_SELECT_STATE,
			PATROLWORKWANDER_PATROL,
			PATROLWORKWANDER_WORK,
			PATROLWORKWANDER_WANDER,
			PATROLWORKWANDER_HOLD,	  
			PATROLWORKWANDER_FAILED
			};

	protected:

		void							_patrolFailed		( Actor &self );

      
		void							_setupWander		( Actor &self );
		void							_wander				( Actor &self );
		void							_wanderFailed		( Actor &self );
      
		void							_workFailed			( Actor &self );


		bool							_wantsToPatrol		( Actor &self );
		bool							_wantsToWander		( Actor &self );
	  
      
      
	public:
		CLASS_PROTOTYPE( PatrolWorkWander );


		BehaviorReturnCode_t			Evaluate			( Actor &self ) override;


	private: 
		Patrol                           _patrolComponent;
		Work                             _workComponent;
		MoveRandomDirection              _wanderComponent;

	private: 
		unsigned int                     _state;
		HelperNodePtr                    _lastWorkNodeChecked;
		HelperNodePtr                    _lastPatrolNodeChecked;
      
		float                            _nextStateCheck;
		float                            _nextWorkCheck;
		float                            _nextPatrolCheck;
	  
		int                              _wanderFailures;	 
		int                              _patrolFailures;
		int                              _workFailures;

		static const float               MIN_DISTANCE_TO_PATROL_NODE;
		static const float               MIN_DISTANCE_TO_WORK_NODE;
      






public:
    void SetArgs ( Event *ev ); 
    void AnimDone ( Event *ev ); 
    void Begin ( Actor &self ) override; 
    void _init ( Actor &self ); 
    void End ( Actor &self ) override; 
    void _setupPatrol ( Actor &self ); 
    void _patrol ( Actor &self ); 
    void _setupWork ( Actor &self ); 
    void _work ( Actor &self ); 
    void _setupHold ( Actor &self ); 
    void _hold ( Actor &self ); 
    bool _wantsToWork ( Actor &self ); 
    void _selectState ( Actor &self ); 
    void Archive( Archiver &arc ) override;  
};

inline void PatrolWorkWander::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	    
   

	arc.ArchiveObject      ( &_patrolComponent );
	arc.ArchiveObject      ( &_workComponent );
	arc.ArchiveObject      ( &_wanderComponent );

	arc.ArchiveUnsigned    ( &_state );
	arc.ArchiveSafePointer ( &_lastWorkNodeChecked );
	arc.ArchiveSafePointer ( &_lastPatrolNodeChecked );
	arc.ArchiveFloat       ( &_nextStateCheck );
	arc.ArchiveFloat       ( &_nextWorkCheck );   
	arc.ArchiveFloat       ( &_nextPatrolCheck );
	arc.ArchiveInteger     ( &_wanderFailures );
	arc.ArchiveInteger     ( &_patrolFailures ); 
	arc.ArchiveInteger     ( &_workFailures   );

}

#endif 
