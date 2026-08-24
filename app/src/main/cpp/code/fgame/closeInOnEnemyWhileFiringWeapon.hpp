
class CloseInOnEnemyWhileFiringWeapon;

#ifndef __CLOSE_IN_ON_ENEMY_WHILE_FIRING_WEAPON_HPP__
#define __CLOSE_IN_ON_ENEMY_WHILE_FIRING_WEAPON_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class CloseInOnEnemyWhileFiringWeapon : public Behavior
{
	public:
		typedef enum
		{	
			CIWF_APPROACH_SETUP_APPROACH,
			CIWF_APPROACH_FIRE,
			CIWF_APPROACH_FIRE_PAUSE,			
			CIWF_SUCCESS,
			CIWF_FAILED
		} CIWFStates_t;

	private: 
		str							_approachAnim;  
		str							_aimAnim;
		str							_fireAnim;
		float						_fireTimeMin;
		float						_fireTimeMax;
		float						_pauseTimeMin;
		float						_pauseTimeMax;
		float						_dist;

	protected:
		void	transitionToState	( CIWFStates_t state );
		void	setInternalState	( CIWFStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();
		void	updateEnemy			();
		void	setTorsoAnim		();	
	
		void					setupStateSetupApproach		();
		BehaviorReturnCode_t	evaluateStateSetupApproach	();
		void					failureStateSetupApproach	( const str& failureReason );

		void					setupStateApproachFire		();
		BehaviorReturnCode_t	evaluateStateApproachFire	();
		void					failureStateApproachFire	( const str& failureReason );

		void					setupStateApproachFirePause		();
		BehaviorReturnCode_t	evaluateStateApproachFirePause	();
		void					failureStateApproachFirePause	( const str& failureReason );


	public:
		CLASS_PROTOTYPE( CloseInOnEnemyWhileFiringWeapon );

												CloseInOnEnemyWhileFiringWeapon();
											   ~CloseInOnEnemyWhileFiringWeapon();

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
		FireWeapon								_fireWeapon;

	private: 
		CIWFStates_t							_state;
		EntityPtr								_currentEnemy;
		float									_nextFireTime;
		float									_nextPauseTime;
		Actor								   *_self;

};

inline void CloseInOnEnemyWhileFiringWeapon::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );	 
   
	arc.ArchiveString ( &_approachAnim );
	arc.ArchiveString ( &_aimAnim );
	arc.ArchiveString ( &_fireAnim );
	arc.ArchiveFloat  ( &_fireTimeMin );
	arc.ArchiveFloat  ( &_fireTimeMax );
	arc.ArchiveFloat  ( &_pauseTimeMin );
	arc.ArchiveFloat  ( &_pauseTimeMax );
	arc.ArchiveFloat  ( &_dist );

	arc.ArchiveObject ( &_chaseEnemy );
	arc.ArchiveObject ( &_fireWeapon );

	ArchiveEnum					( _state, CIWFStates_t);
	arc.ArchiveSafePointer		( &_currentEnemy				);
	arc.ArchiveFloat			( &_nextFireTime				);
	arc.ArchiveFloat			( &_nextPauseTime				);
	arc.ArchiveObjectPointer	( ( Class ** )&_self			);
}  

#endif 
