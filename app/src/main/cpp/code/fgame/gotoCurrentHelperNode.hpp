
class GotoCurrentHelperNode;

#ifndef __GOTO_CURRENT_HELPER_NODE__
#define __GOTO_CURRENT_HELPER_NODE__

#include "behavior.h"
#include "behaviors_general.h"

class GotoCurrentHelperNode : public Behavior
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
		str				_movementAnim;		
		
	protected:
		void	transitionToState	( GotoHelperNodeStates_t state );
		void	setInternalState	( GotoHelperNodeStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();		
	

		void					setupStateMoveToNode		();
		BehaviorReturnCode_t	evaluateStateMoveToNode		();
		void					failureStateFindNode		( const str& failureReason );

		void					setupStateFindNode		    ();
		BehaviorReturnCode_t    evaluateStateFindNode       ();
		void					failureStateMoveToNode		( const str& failureReason );

	public:
		CLASS_PROTOTYPE( GotoCurrentHelperNode );

										GotoCurrentHelperNode();
									   ~GotoCurrentHelperNode();

		void							SetArgs			( Event *ev );      
		void							AnimDone		( Event *ev );
	
		void							Begin			( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate		( Actor &self ) override;
		void							End				( Actor &self ) override;

		void							SetMovementAnim ( const str &anim );


		virtual void					Archive  ( Archiver &arc ) override;

	private: 	
		GotoPoint						_gotoPoint;


	private: 
		GotoHelperNodeStates_t			_state;		 				
		HelperNodePtr					_node;
		bool							_faceEnemy;		
		static const float				NODE_RADIUS;

	};

inline void	GotoCurrentHelperNode::SetMovementAnim ( const str &anim )
{
	_movementAnim = anim;
}


inline void GotoCurrentHelperNode::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	arc.ArchiveString		( &_movementAnim );	
	arc.ArchiveObject		( &_gotoPoint );

	ArchiveEnum				( _state, GotoHelperNodeStates_t   );			
	arc.ArchiveSafePointer	( &_node		);
	arc.ArchiveBool			( &_faceEnemy );
}

#endif 

