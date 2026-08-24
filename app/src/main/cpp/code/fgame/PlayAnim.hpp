
class PlayAnim;

#ifndef __PLAY_ANIM_HPP__
#define __PLAY_ANIM_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class PlayAnim : public Behavior
{
	public:
		typedef enum
		{	
			PLAYANIM_SETUP,
			PLAYANIM_ANIMATE,
			PLAYANIM_SUCCESS,
			PLAYANIM_FAILED
		} PlayAnimStates_t;

	private: 
		str							_legAnim;  
		str							_torsoAnim;
		float						_minTime;
		float						_maxTime;

	protected:
		void	transitionToState	( PlayAnimStates_t state );
		void	setInternalState	( PlayAnimStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();
	
		void					setupStateSetup				();
		BehaviorReturnCode_t	evaluateStateSetup			();
		void					failureStateSetup			( const str& failureReason );

		void					setupStateAnimate			();
		BehaviorReturnCode_t	evaluateStateAnimate		();
		void					failureStateAnimate			( const str& failureReason );


	public:
		CLASS_PROTOTYPE( PlayAnim );

												PlayAnim();
											   ~PlayAnim();

		void									SetArgs		( Event *ev     );      
		void									Begin		( Actor &self   ) override;		
		BehaviorReturnCode_t					Evaluate	( Actor &self   ) override;
		void									End			( Actor &self   ) override;
		virtual void							Archive		( Archiver &arc ) override;

		void									setAnim		( const str &animName ) { _legAnim = animName;		}
		void									setTorsoAnim( const str &animName ) { _torsoAnim = animName;	}
		void									setMinTime	( float minTime		  ) { _minTime = minTime;		}
		void									setMaxTime	( float maxTime       ) { _maxTime = maxTime;		}


	private: 
		PlayAnimStates_t						_state;	
		float									_endTime;
		
};

inline void PlayAnim::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );	 
   
	arc.ArchiveString ( &_legAnim );
	arc.ArchiveString ( &_torsoAnim );
	arc.ArchiveFloat  ( &_minTime );
	arc.ArchiveFloat  ( &_maxTime);



	ArchiveEnum					( _state, PlayAnimStates_t		);
	arc.ArchiveFloat			( &_endTime						);
}  

#endif 





