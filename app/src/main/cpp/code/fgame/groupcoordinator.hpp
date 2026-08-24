//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/groupcoordinator.h                             $
// $Revision:: 158                                                            $
//   $Author:: Sketcher                                                       $
//     $Date:: 4/11/02 11:34a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Classes for groups and group coordination.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#ifndef __GROUP_COORDINATOR_HPP__
#define __GROUP_COORDINATOR_HPP__

#include "entity.h"

class Group	: public Listener
{
	public:
		CLASS_PROTOTYPE( Group );
		 Group();
		~Group();

		virtual void AddMember				 ( Entity *entity );
		virtual void RemoveMember			 ( Entity *entity );
		virtual void SetGroupDeathThread	 ( const str &threadName );		
		virtual void ClearList				 ();
		virtual void RunGroupDeathThread	 ( Entity *entity );
		virtual int  CountMembers			 ();
		bool		 IsThisTargetNameInGroup ( const str &name );
		bool		 IsThisEntityInGroup     ( Entity *entity  );
		void		 SendEventToGroup		 ( Event *ev );	
		void		 SendEventToGroup		 ( Event &ev );
		void		 GroupCancelEventsOfType ( Event *ev );

		int			 GetGroupID				 ();
		void         SetGroupID				 ( int ID );
		Entity*		 GetNextMember			 ( Entity *entity );
		Entity*		 GetPreviousMember		 ( Entity *entity );

		virtual void Archive				 ( Archiver &arc ) override;

	protected:

	private:
		Container< EntityPtr >	_memberList;		
		int						_id;
		str						_groupDeathThread;
};

inline int Group::CountMembers()
{
	return _memberList.NumObjects();
}

inline int Group::GetGroupID()
{
	return _id;
}

inline void Group::SetGroupID( int ID )
{
	_id = ID;
}

inline void Group::SetGroupDeathThread( const str &threadName )
{
	_groupDeathThread = threadName;
}





class ActorGroup : public Group
{
	public:
		CLASS_PROTOTYPE( ActorGroup );
		 ActorGroup();
		~ActorGroup();

		int CountMembersWithThisName( const str &name );
		int CountMembersAttackingEnemy( Entity *enemy = 0);

	protected:
	private:	
};







class GroupCoordinator : public Listener
{
	public:
		CLASS_PROTOTYPE( GroupCoordinator );
		 GroupCoordinator();
		~GroupCoordinator();

		Group* GetGroup			( int ID );
		void   AddEntityToGroup	( Entity *ent , int ID );
		void   RemoveEntityFromGroup ( Entity *ent , int ID );		
		int    CountGroups		();
		void   RemoveGroup		( int ID );
		void   ClearGroupList	();
		void   SendEventToGroup	( Event *ev , int ID );	
		void   SendEventToGroup ( Event &ev , int ID );
		void   GroupCancelEventsOfType ( Event *ev , int ID );
		void   MemberDied			 ( Entity *entity , int ID );
		void   SetGroupDeathThread	 ( const str &threadName , int ID );	
		virtual void Archive	( Archiver &arc ) override;

	protected:		
		
	private:
		Container< Group* > _groupList;
};

inline int GroupCoordinator::CountGroups()
{
	return _groupList.NumObjects();
}


typedef SafePtr<GroupCoordinator> GroupCoordinatorPtr;
extern GroupCoordinatorPtr groupcoordinator;

#endif 
