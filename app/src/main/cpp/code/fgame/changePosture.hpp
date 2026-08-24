
class ChangePosture;

#ifndef __CHANGE_POSTURE_HPP__
#define __CHANGE_POSTURE_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class ChangePosture : public Behavior
{
	public:
		typedef enum
		{	
			CHANGE_POSTURE_SETUP,
			CHANGE_POSTURE_CHANGE,
			CHANGE_POSTURE_SUCCESS,
			CHANGE_POSTURE_FAILED
		} changePostureStates_t;

	private: 
		str							_posture;  
		
	protected:
		void	transitionToState	( changePostureStates_t state );
		void	setInternalState	( changePostureStates_t state , const str &stateName );
		void	init				();
		void	think				();
		

		void					setupStateSetup		();
		BehaviorReturnCode_t	evaluateStateSetup	();
		void					failureStateSetup	( const str& failureReason );

		void					setupStateChange	();
		BehaviorReturnCode_t	evaluateStateChange	();
		void					failureStateChange	( const str& failureReason );
		

	public:
		CLASS_PROTOTYPE( ChangePosture );

												ChangePosture();
											   ~ChangePosture();

		void									SetArgs		( Event *ev     );      
		void									PostureDone	( Event *ev		);
		void									Begin		( Actor &self   ) override;		
		BehaviorReturnCode_t					Evaluate	( Actor &self   ) override;
		void									End			( Actor &self   ) override;
		virtual void							Archive		( Archiver &arc ) override;

		void									setPosture	( const str &postureName ) { _posture = postureName; }

	
	private: 
		changePostureStates_t					_state;
		bool									_postureDone;
		bool									_canChange;

};


inline void ChangePosture::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );	 
   
	arc.ArchiveString ( &_posture );


	ArchiveEnum					( _state, changePostureStates_t);
	arc.ArchiveBool				( &_postureDone );
	arc.ArchiveBool				( &_canChange );
}  

#endif 
