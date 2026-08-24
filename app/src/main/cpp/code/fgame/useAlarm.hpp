
class UseAlarm;

#ifndef __USE_ALARM_HPP__
#define __USE_ALARM_HPP__

#include "behavior.h"
#include "behaviors_general.h"
#include "gotoHelperNode.hpp"

class UseAlarm : public Behavior
	{
	public:
		typedef enum
		{
		USE_ALARM_FIND_NODE,
		USE_ALARM_MOVE_TO_NODE,
		USE_ALARM_AT_NODE,		
		USE_ALARM_WAIT_ON_ANIM,
		USE_ALARM_ROTATE_TO_ENEMY,
		USE_ALARM_SUCCESSFUL,
		USE_ALARM_FAILED
		} useAlarmStates_t;

	private: 
		str							_movementAnimName ; 
		float						_maxDistance;	    

	protected:
		void	transitionToState	( useAlarmStates_t state );
		void	setInternalState	( useAlarmStates_t state , const str &stateName );
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

		void					setupStateWaitOnAnim					();
		BehaviorReturnCode_t	evaluateStateWaitOnAnim					();
		void					failureStateWaitOnAnim					( const str& failureReason );

		void					setupStateRotateToEnemy					();
		BehaviorReturnCode_t	evaluateStateRotateToEnemy				();
		void					failureStateRotateToEnemy				( const str& failureReason );


	public:
		CLASS_PROTOTYPE( UseAlarm );
												UseAlarm();
											   ~UseAlarm();

		void									SetArgs				( Event *ev );      
		void									AnimDone			( Event *ev );
		void									HandleNodeCommand	( Event *ev );

		void									Begin				( Actor &self ) override;		
		BehaviorReturnCode_t					Evaluate			( Actor &self ) override;
		void									End					( Actor &self ) override;

		virtual void							Archive  ( Archiver &arc ) override;

	private: 
		GotoHelperNode							_gotoHelperNode;
		RotateToEntity							_rotateToEntity;

	private: 
		HelperNodePtr							_node;            
		unsigned int							_state;
		bool									_animDone;
		Actor*									_self;
		
	};

inline void UseAlarm::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	    
   
	arc.ArchiveString			( &_movementAnimName);
	arc.ArchiveFloat			( &_maxDistance     );

	arc.ArchiveObject			( &_gotoHelperNode  );
	arc.ArchiveObject			( &_rotateToEntity  );
	

	arc.ArchiveSafePointer		( &_node				);
	arc.ArchiveUnsigned			( &_state				);
	arc.ArchiveBool				( &_animDone			);
	arc.ArchiveObjectPointer	( ( Class ** )&_self	);
}

#endif 
