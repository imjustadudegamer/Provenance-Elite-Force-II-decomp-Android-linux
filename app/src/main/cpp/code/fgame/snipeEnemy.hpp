

class SnipeEnemy;

#ifndef __SNIPE_ENEMY___
#define __SNIPE_ENEMY___

#include "behavior.h"
#include "behaviors_general.h"

class SnipeEnemy : public Behavior
	{
	public:  
		typedef enum
		{
			SNIPE_AIM_AND_FIRE_AIM,
			SNIPE_AIM_AND_FIRE_PRE_FIRE,
			SNIPE_AIM_AND_FIRE_ATTACK,	
			SNIPE_AIM_AND_FIRE_POST_FIRE,
			SNIPE_AIM_AND_FIRE_SUCCESS,
			SNIPE_AIM_AND_FIRE_FAILED
		} SnipeAimAndFireStates_t;

	private:
		float			_aimTime;
		float			_lockDownTime;
		float			_maxTorsoYaw;
		float			_maxTorsoPitch;
		float			_maxTorsoTurnSpeed;
		int				_shots;
		
		
	protected:
		void	transitionToState	( SnipeAimAndFireStates_t state );
		void	setInternalState	( SnipeAimAndFireStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();		
		void	updateEnemy			();
		void	AdjustTorsoAngles	( const Vector &tagPos , const Vector &watchPosition );            
		void	LerpTorsoBySpeed	( const Vector &angleDelta );	

		void					setupStateAim				();
		BehaviorReturnCode_t	evaluateStateAim			();
		void					failureStateAim				( const str& failureReason );

		void					setupStatePreFire			();
		BehaviorReturnCode_t	evaluateStatePreFire		();
		void					failureStatePreFire			( const str& failureReason );

		void					setupStateAttack			();
		BehaviorReturnCode_t	evaluateStateAttack			();
		void					failureStateAttack			( const str& failureReason );

		void					setupStatePostFire			();
		BehaviorReturnCode_t	evaluateStatePostFire		();
		void					failureStatePostFire		( const str& failureReason );



	public:
		CLASS_PROTOTYPE( SnipeEnemy );

										SnipeEnemy();
									   ~SnipeEnemy();

		void							SetArgs			( Event *ev );      
		void							AnimDone		( Event *ev );
	
		void							Begin			( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate		( Actor &self ) override;
		void							End				( Actor &self ) override;

		virtual void					Archive  ( Archiver &arc ) override;

	private: 	
		FireWeapon						_fireWeapon;

	private: 
		SnipeAimAndFireStates_t			_state;	
		Vector							_currentTorsoAngles;		
		float							_endAimTime;
		float							_endLockDownTime;
		EntityPtr						_currentEnemy;
		bool							_canAttack;
		bool							_animDone;
		str								_aimAnim;
		str								_preFireAnim;
		str								_fireAnim;	
		str								_postFireAnim;
		bool							_fireFailed;
		Vector							_lastGoodPosition;
		Vector							_targetSpread;
		Actor						   *_self;		
		

	};

inline void SnipeEnemy::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	arc.ArchiveFloat( &_aimTime );
	arc.ArchiveFloat( &_lockDownTime );
	arc.ArchiveFloat( &_maxTorsoYaw );
	arc.ArchiveFloat( &_maxTorsoPitch );
	arc.ArchiveFloat( &_maxTorsoTurnSpeed );
	arc.ArchiveInteger( &_shots );
	arc.ArchiveVector( &_lastGoodPosition );
	arc.ArchiveVector( &_targetSpread );
	
	
	arc.ArchiveObject		( &_fireWeapon		);

	ArchiveEnum				( _state, SnipeAimAndFireStates_t	);
	arc.ArchiveVector		( &_currentTorsoAngles				);

	arc.ArchiveFloat( &_endAimTime );
	arc.ArchiveFloat( &_endLockDownTime );
	arc.ArchiveSafePointer( &_currentEnemy );
	arc.ArchiveBool( &_canAttack );
	arc.ArchiveBool( &_animDone );
	arc.ArchiveString( &_aimAnim );
	arc.ArchiveString( &_preFireAnim );
	arc.ArchiveString( &_fireAnim );
	arc.ArchiveString( &_postFireAnim );
	arc.ArchiveBool( &_fireFailed );
	
	arc.ArchiveObjectPointer( ( Class ** )&_self				);
}


#endif 

