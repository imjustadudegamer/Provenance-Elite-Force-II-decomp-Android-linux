//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/actor_posturecontroller.h                      $
// $Revision:: 15                                                             $
//   $Author:: Sketcher                                                       $
//     $Date:: 5/20/02 3:55p                                                  $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class PostureController;

#ifndef __ACTOR_POSTURECONTROLLER_HPP__
#define __ACTOR_POSTURECONTROLLER_HPP__

#include "actor.h"
#include "actorincludes.h"

extern Event EV_Posture_Anim_Done;
extern Event EV_PostureChanged_Completed;

class PostureController
{
	public:
						PostureController();
						PostureController( Actor *actor );
					   ~PostureController();

		virtual void	Archive( Archiver &arc ) override;
		void			DoArchive( Archiver &arc , Actor *actor ) override;
		
		void			evaluate();		

		bool			requestPosture( const str &postureState , Listener *requestor );
		
		const str&		getRequestedPostureName();
		const str&		getCurrentPostureName();

		void			setPostureStateMap( const str &stateMap , bool loading );
		void			setPostureState( const str &postureState );						
		void			setPostureState( const str &postureState , const str &requestedState );

	protected:
		void			init();		

	private:
		StateMap		*_postureStateMap;
		str				 _postureStateMap_Name;

		State			*_currentPostureState;
		str				 _currentPostureState_Name;

		State			*_requestedPostureState;
		str				 _requestedPostureState_Name;

		SafePtr<Listener> _requestor;
		Container<Conditional*>	 _postureConditionals;


		
		Actor		    *act;
};

inline const str& PostureController::getRequestedPostureName()
{
	return _requestedPostureState_Name;
}

inline const str& PostureController::getCurrentPostureName()
{
	return _currentPostureState_Name;
}

#endif 

