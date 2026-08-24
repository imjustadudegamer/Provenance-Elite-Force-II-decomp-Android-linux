
class GeneralCombatWithRangedWeapon;

#ifndef __GENERALCOMBAT_WITH_RANGEDWEAPON_HPP__
#define __GENERALCOMBAT_WITH_RANGEDWEAPON_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class GeneralCombatWithRangedWeapon : public Behavior
	{
	public:  
		typedef enum
		{		
		GC_WRW_CHANGE_POSTURE_TO_ADVANCE,
		GC_WRW_CHANGE_POSTURE_TO_RETREAT,
		GC_WRW_ADVANCE,
		GC_WRW_ADVANCE_FIRING,
		GC_WRW_RETREAT,
		GC_WRW_RETREAT_FIRING,
		GC_WRW_STRAFE,		
		GC_WRW_CHANGE_POSTURE_DUCK,
		GC_WRW_DUCKED,
		GC_WRW_DUCKED_FIRING,
		GC_WRW_CHANGE_POSTURE_STAND,
		GC_WRW_STAND,
		GC_WRW_STAND_FIRING,
		GC_WRW_FAILED
		} generalCombatStates_t;

	private:
		str								_movementAnim;
		str								_torsoAnim;
		str								_fireAnim;
		str								_preFireAnim;
		str								_postFireAnim;
		float							_approachDist;
		float							_retreatDist;
		float							_strafeChance;
		float							_strafeTime;
		float							_strafeRandomFactor;
		float							_postureChangeChance;
		float							_fireTimeMin;
		float							_fireTimeMax;
		float							_fireTimeRandomFactor;
		float							_pauseTimeMin;
		float							_pauseTimeMax;
		float							_pauseTimeRandomFactor;



	protected:
		void	transitionToState			( generalCombatStates_t state );
		void	setInternalState			( generalCombatStates_t state , const str &stateName );
		void	init						( Actor &self );
		void	think						();
		void	updateEnemy					();
		void	setTorsoAnim				();			
		bool	checkShouldApproach			( float distToEnemy );
		bool	checkShouldRetreat			( float distToEnemy );
		bool	checkShouldStrafe			( float distToEnemy );
		bool	checkShouldChangePosture	( float distToEnemy );		


		void					setupRotate		();
		BehaviorReturnCode_t	evaluateRotate	();

		void					setupStateChangePostureToAdvance	();
		BehaviorReturnCode_t	evaluateStateChangePostureToAdvance	();
		void					failureStateChangePostureToAdvance	( const str& failureReason );

		void					setupStateChangePostureToRetreat	();
		BehaviorReturnCode_t	evaluateStateChangePostureToRetreat	();
		void					failureStateChangePostureToRetreat	( const str& failureReason );

		void					setupStateAdvance					();
		BehaviorReturnCode_t	evaluateStateAdvance				();
		void					failureStateAdvance					( const str& failureReason );

		void					setupStateAdvanceFiring				();
		BehaviorReturnCode_t	evaluateStateAdvanceFiring			();
		void					failureStateAdvanceFiring			( const str& failureReason );

		void					setupStateRetreat					();
		BehaviorReturnCode_t	evaluateStateRetreat				();
		void					failureStateRetreat					( const str& failureReason );

		void					setupStateRetreatFiring				();
		BehaviorReturnCode_t	evaluateStateRetreatFiring			();
		void					failureStateRetreatFiring			( const str& failureReason );

		void					setupStateStrafe					();
		BehaviorReturnCode_t	evaluateStateStrafe					();
		void					failureStateStrafe					( const str& failureReason );

		void					setupStateChangePostureDuck			();
		BehaviorReturnCode_t	evaluateStateChangePostureDuck		();
		void					failureStateChangePostureDuck		( const str& failureReason );

		void					setupStateDucked					();
		BehaviorReturnCode_t	evaluateStateDucked					();
		void					failureStateDucked					( const str& failureReason );

		void					setupStateDuckedFiring				();
		BehaviorReturnCode_t	evaluateStateDuckedFiring			();
		void					failureStateDuckedFiring			( const str& failureReason );

		void					setupStateChangePostureStand		();
		BehaviorReturnCode_t	evaluateStateChangePostureStand		();
		void					failureStateChangePostureStand		( const str& failureReason );

		void					setupStateStand						();
		BehaviorReturnCode_t	evaluateStateStand					();
		void					failureStateStand					( const str& failureReason );

		void					setupStateStandFiring				();
		BehaviorReturnCode_t	evaluateStateStandFiring			();
		void					failureStateStandFiring				( const str& failureReason );

	public:
		CLASS_PROTOTYPE( GeneralCombatWithRangedWeapon );

										GeneralCombatWithRangedWeapon();
									   ~GeneralCombatWithRangedWeapon();

		void							SetArgs					( Event *ev );      
		void							AnimDone				( Event *ev );
		void							PostureDone				( Event *ev );

		void							Begin					( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate				( Actor &self ) override;
		void							End						( Actor &self ) override;
	

		void							SetMovementAnim			 ( const str& movementAnim		) { _movementAnim = movementAnim;					}
		void							SetTorsoAnim			 ( const str& torsoAnim			) { _torsoAnim = torsoAnim;							}
		void							SetFireAnim				 ( const str& fireAnim			) { _fireAnim = fireAnim;							}
		void							SetPreFireAnim			 ( const str& preFireAnim		) { _preFireAnim = preFireAnim;						}
		void							SetPostFireAnim			 ( const str& postFireAnim		) {	_postFireAnim = postFireAnim;					}
		void							SetApproachDist			 ( float approachDist			) { _approachDist = approachDist;					}
		void							SetRetreatDist			 ( float retreatDist			) { _retreatDist = retreatDist;						}
		void							SetStrafeChance			 ( float strafeChance			) { _strafeChance = strafeChance;					}
		void							SetStrafeTime			 ( float strafeTime				) { _strafeTime = strafeTime;						}
		void							SetStrafeRandomFactor	 ( float strafeRandomFactor		) { _strafeRandomFactor = strafeRandomFactor;		}
		void							SetPostureChangeChance	 ( float postureChangeChance	) { _postureChangeChance = postureChangeChance;		}		
		void							SetFireTimeMin			 ( float fireTimeMin			) { _fireTimeMin = fireTimeMin;						}
		void							SetFireTimeMax			 ( float fireTimeMax			) { _fireTimeMax = fireTimeMax;						}
		void							SetFireTimeRandomFactor  ( float fireTimeRandomFactor	) { _fireTimeRandomFactor = fireTimeRandomFactor;	}
		void							SetPauseTimeMin			 ( float pauseTimeMin			) { _pauseTimeMin = pauseTimeMin;					}
		void							SetPauseTimeMax			 ( float pauseTimeMax			) { _pauseTimeMax = pauseTimeMax;					}
		void							SetPauseTimeRandomFactor ( float pauseTimeRandomFactor	) { _pauseTimeRandomFactor = pauseTimeRandomFactor;	}

		virtual void					Archive  ( Archiver &arc ) override;

	private: 	
		RotateToEntity					_rotate;
		MoveRandomDirection				_moveRandomDir;		
		FireWeapon						_fireWeapon;
		Strafe							_strafeComponent;


	private: 
		generalCombatStates_t			_state;		
		Actor						   *_self;
		EntityPtr						_currentEnemy;
		bool							_finishedPostureTransition;
		bool							_randomAdvanceLockOut;
		bool							_randomRetreatLockOut;

		float							_nextRotateTime;
		float							_nextStrafeAttemptTime;
		float							_nextPostureChange;
		float							_nextClearRetreatLockOutTime;
		float							_nextClearAdvanceLockOutTime;
		      
		int								_randomAdvanceFailures;
		int								_randomRetreatFailures;

		float							_nextFireTime;
		float							_nextPauseTime;

};


inline void GeneralCombatWithRangedWeapon::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	 
    
		
	arc.ArchiveString( &_movementAnim );
	arc.ArchiveString( &_torsoAnim );
	arc.ArchiveString( &_fireAnim );
	arc.ArchiveString( &_preFireAnim );
	arc.ArchiveString( &_postFireAnim );
	arc.ArchiveFloat( &_approachDist );
	arc.ArchiveFloat( &_retreatDist );
	arc.ArchiveFloat( &_strafeChance );
	arc.ArchiveFloat( &_strafeTime );
	arc.ArchiveFloat( &_strafeRandomFactor );
	arc.ArchiveFloat( &_postureChangeChance );
	arc.ArchiveFloat( &_fireTimeMin );
	arc.ArchiveFloat( &_fireTimeMax );
	arc.ArchiveFloat( &_fireTimeRandomFactor );
	arc.ArchiveFloat( &_pauseTimeMin );
	arc.ArchiveFloat( &_pauseTimeMax );
	arc.ArchiveFloat( &_pauseTimeRandomFactor );

	arc.ArchiveObject( &_rotate );
	arc.ArchiveObject( &_moveRandomDir );
	arc.ArchiveObject( &_fireWeapon );
	arc.ArchiveObject( &_strafeComponent );	

	ArchiveEnum					( _state, generalCombatStates_t   );
	arc.ArchiveObjectPointer	( ( Class ** )&_self			);
	arc.ArchiveSafePointer		( &_currentEnemy );
	arc.ArchiveBool				( &_finishedPostureTransition );
	arc.ArchiveBool				( &_randomAdvanceLockOut );
	arc.ArchiveBool				( &_randomRetreatLockOut );
	arc.ArchiveFloat			( &_nextRotateTime );
	arc.ArchiveFloat			( &_nextStrafeAttemptTime );
	arc.ArchiveFloat			( &_nextPostureChange );
	arc.ArchiveFloat			( &_nextClearRetreatLockOutTime );
	arc.ArchiveFloat			( &_nextClearAdvanceLockOutTime );
	arc.ArchiveInteger			( &_randomAdvanceFailures );
	arc.ArchiveInteger			( &_randomRetreatFailures );
	arc.ArchiveFloat			( &_nextFireTime );
	arc.ArchiveFloat			( &_nextPauseTime );	
	

}


#endif 

