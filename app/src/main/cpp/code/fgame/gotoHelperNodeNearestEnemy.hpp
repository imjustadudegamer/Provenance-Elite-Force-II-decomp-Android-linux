
class GotoHelperNodeNearestEnemy;

#ifndef __GOTO_HELPER_NODE_NEAREST_ENEMY___
#define __GOTO_HELPER_NODE_NEAREST_ENEMY__

#include "behavior.h"
#include "behaviors_general.h"
#include "gotoHelperNode.hpp"

class GotoHelperNodeNearestEnemy : public Behavior
	{
	public:  
		typedef enum
		{
			GOTO_HNODE_FIND_NODE,
			GOTO_HNODE_MOVE_TO_NODE,
			GOTO_HNODE_SUCCESS,
			GOTO_HNODE_FAILED
		} GotoHelperNodeStates_t;

	private:
		str			    _nodeType;
		str				_movementAnim;
		float			_maxDistance;
		

	protected:
		void	transitionToState	( GotoHelperNodeStates_t state );
		void	setInternalState	( GotoHelperNodeStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();
		void	updateEnemy			();
	

		void					setupStateFindNode			();
		BehaviorReturnCode_t	evaluateStateFindNode		();
		void					failureStateFindNode		( const str& failureReason );

		void					setupStateMoveToNode		();
		BehaviorReturnCode_t	evaluateStateMoveToNode		();
		void					failureStateMoveToNode		( const str& failureReason );

	public:
		CLASS_PROTOTYPE( GotoHelperNodeNearestEnemy );

										GotoHelperNodeNearestEnemy();
									   ~GotoHelperNodeNearestEnemy();

		void							SetArgs			( Event *ev );      
		void							AnimDone		( Event *ev );
	
		void							Begin			( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate		( Actor &self ) override;
		void							End				( Actor &self ) override;

		void							SetNode			( HelperNode *node );
		void							SetMovementAnim ( const str &anim );


		virtual void					Archive  ( Archiver &arc ) override;

	private: 	
		GotoHelperNode					_gotoHNode;


	private: 
		GotoHelperNodeStates_t			_state;		 				
		HelperNodePtr					_node;
		EntityPtr						_currentEnemy;

		Actor						   *_self;		


	};



inline void GotoHelperNodeNearestEnemy::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	arc.ArchiveString		( &_nodeType     );
	arc.ArchiveString		( &_movementAnim );
	arc.ArchiveFloat		( &_maxDistance  );
	
	arc.ArchiveObject		( &_gotoHNode );

	ArchiveEnum				( _state, GotoHelperNodeStates_t   );
	arc.ArchiveSafePointer	( &_node						);
	arc.ArchiveSafePointer	( &_currentEnemy					);
	arc.ArchiveObjectPointer( ( Class ** )&_self			);
}

#endif 

