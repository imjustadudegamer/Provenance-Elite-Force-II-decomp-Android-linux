
class CloseInOnEnemy;

#ifndef __CLOSE_IN_ON_ENEMY_HPP__
#define __CLOSE_IN_ON_ENEMY_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class CloseInOnEnemy : public Behavior
{
	public:
		typedef enum
		{	
			CLOSE_IN_ON_ENEMY_APPROACH,
			CLOSE_IN_ON_ENEMY_SUCCESS,
			CLOSE_IN_ON_ENEMY_FAILED
		} closeInOnEnemyStates_t;

	private: 
		str							_anim;  
		str							_torsoAnim;
		float						_dist;

	protected:
		void	transitionToState	( closeInOnEnemyStates_t state );
		void	setInternalState	( closeInOnEnemyStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();
		void	updateEnemy			();
		void	setTorsoAnim		();	

		void					setupStateApproach		();
		BehaviorReturnCode_t	evaluateStateApproach	();
		void					failureStateApproach	( const str& failureReason );

	public:
		CLASS_PROTOTYPE( CloseInOnEnemy );

												CloseInOnEnemy();
											   ~CloseInOnEnemy();

		void									SetArgs		( Event *ev     );      
		void									Begin		( Actor &self   ) override;		
		BehaviorReturnCode_t					Evaluate	( Actor &self   ) override;
		void									End			( Actor &self   ) override;
		virtual void							Archive		( Archiver &arc ) override;

		void									setAnim		( const str &animName );
		void									setTorsoAnim( const str &animName );
		void									setDist		( float distance      );

	private:      
		GotoEntity								_chaseEnemy;

	private: 
		closeInOnEnemyStates_t					_state;
		EntityPtr								_currentEnemy;
		Actor								   *_self;

};

inline void CloseInOnEnemy::setAnim( const str &animName )
{
	_anim = animName;
}

inline void CloseInOnEnemy::setTorsoAnim( const str &animName )
{
	_torsoAnim = animName;
}

inline void CloseInOnEnemy::setDist( float distance )
{
	_dist = distance;
}

inline void CloseInOnEnemy::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );	 
   
	arc.ArchiveString ( &_anim );
	arc.ArchiveString ( &_torsoAnim );
	arc.ArchiveFloat  ( &_dist );

	arc.ArchiveObject ( &_chaseEnemy );

	ArchiveEnum					( _state, closeInOnEnemyStates_t);
	arc.ArchiveSafePointer		( &_currentEnemy				);
	arc.ArchiveObjectPointer	( ( Class ** )&_self			);
}  

#endif 
