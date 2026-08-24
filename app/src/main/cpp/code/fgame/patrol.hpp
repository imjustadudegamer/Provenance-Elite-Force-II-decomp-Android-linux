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

class Patrol;

#ifndef __PATROL_HPP__
#define __PATROL_HPP__

class Patrol : public Behavior
   {
	public: 
		typedef enum
		{
			PATROL_FIND_CLOSEST_PATROL_NODE,
			PATROL_MOVING_TO_PATROL_NODE,
			PATROL_AT_PATROL_NODE,
			PATROL_WAITING_AT_NODE,
			PATROL_WAITING_AT_NODE_FOR_ANIM,
			PATROL_FIND_NEXT_PATROL_NODE,
			PATROL_HOLD,
			PATROL_FAILED,
			PATROL_SUCCESSFUL,
		} patrolStates_t;

	private: 

	protected:
		void		_init( Actor &self );      

		bool		_setupFindClosestPatrolNode            ( Actor &self );
		void		_setupFindClosestPatrolNodeFailed      ( Actor &self );
		void		_findClosestPatrolNode                 ( Actor &self );
		void		_findClosestPatrolNodeFailed           ( Actor &self );


		bool		_setupMovingToPatrolNode               ( Actor &self );
		void		_setupMovingToPatrolNodeFailed         ( Actor &self );
		void		_moveToPatrolNode                      ( Actor &self );
		void		_moveToPatrolNodeFailed                ( Actor &self );

		bool		_setupAtPatrolNode                     ( Actor &self );
		void		_setupAtPatrolNodeFailed               ( Actor &self );
		void		_atPatrolNode                          ( Actor &self );

		bool		_setupWaitingAtPatrolNode              ( Actor &self );
		void		_setupWaitingAtPatrolNodeFailed        ( Actor &self );
		void		_waitAtPatrolNode                      ( Actor &self );

		bool		_setupWaitingAtPatrolNodeForAnim       ( Actor &self );
		void		_setupWaitingAtPatrolNodeForAnimFailed ( Actor &self );
		void		_waitAtPatrolNodeForAnim               ( Actor &self );

		bool		_setupFindNextPatrolNode               ( Actor &self );
		void		_setupFindNextPatrolNodeFailed         ( Actor &self );
		void		_findNextPatrolNode                    ( Actor &self );

		bool		_setupHold                             ( Actor &self );
		void		_setupHoldFailed                       ( Actor &self );
		void		_hold                                  ( Actor &self );

	public:
		CLASS_PROTOTYPE( Patrol );

									Patrol();
								   ~Patrol();
		void						SetArgs  ( Event *ev );      
		void						AnimDone ( Event *ev );

		void						Begin    ( Actor &self ) override;		
		BehaviorReturnCode_t		Evaluate ( Actor &self ) override;
		void						End      ( Actor &self ) override;

		virtual void				Archive  ( Archiver &arc ) override;
		void						SetAnim	 ( const str& anim );
		void						SetInitialNode( HelperNode* node );

	private: 
		GotoPoint					_gotoHelperNode;

	private: 
		HelperNodePtr				_node;
		HelperNodePtr				_lastNode;
		unsigned int				_state;
		int							_moveFailures;
		float						_nextMoveAttempt;
		float						_waitTime;
		str							_torsoAnim;
		str							_anim;
   };

inline void Patrol::SetInitialNode( HelperNode *node )
{
	_node = node;
}

inline void Patrol::SetAnim( const str &anim )
{
	_anim = anim;
}

inline void Patrol::Archive( Archiver &arc	)
   {
   Behavior::Archive      ( arc );	    
   

   arc.ArchiveObject      ( &_gotoHelperNode  );

   arc.ArchiveSafePointer ( &_node            );
   arc.ArchiveSafePointer ( &_lastNode        );
   arc.ArchiveUnsigned    ( &_state           );
   arc.ArchiveInteger     ( &_moveFailures    );
   arc.ArchiveFloat       ( &_nextMoveAttempt );   
   arc.ArchiveFloat       ( &_waitTime        );
   arc.ArchiveString	  ( &_torsoAnim		  );
   arc.ArchiveString	  ( &_anim			  );
   }

#endif 
