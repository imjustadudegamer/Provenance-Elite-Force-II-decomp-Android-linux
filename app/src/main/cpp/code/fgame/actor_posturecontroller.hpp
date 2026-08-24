
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

