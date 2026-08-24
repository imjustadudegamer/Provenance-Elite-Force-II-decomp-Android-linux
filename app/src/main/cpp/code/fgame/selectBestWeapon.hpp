
class SelectBestWeapon;

#ifndef __SELECT_BEST_WEAPON_HPP__
#define __SELECT_BEST_WEAPON_HPP__

#include "behavior.h"
#include "behaviors_general.h"

class SelectBestWeapon : public Behavior
	{
	public:  
		typedef enum
		{
			SBW_SELECT_WEAPON,
			SBW_PUT_AWAY_CURRENT_WEAPON,
			SBW_PULL_OUT_NEW_WEAPON,
			SBW_READY_NEW_WEAPON,			
			SBW_SUCCESS,
			SBW_FAILED
		} selectBestWeaponStates_t;

	private:
		EntityPtr	_currentEnemy;

	protected:
		void	transitionToState	( selectBestWeaponStates_t state );
		void	setInternalState	( selectBestWeaponStates_t state , const str &stateName );
		void	init				( Actor &self );
		void	think				();		

		void					setupStateSelectWeapon ();
		BehaviorReturnCode_t	evaluateStateSelectWeapon();
		void					failureStateSelectWeapon( const str& failureReason );

		void					setupStatePutAwayCurrentWeapon();
		BehaviorReturnCode_t	evaluateStatePutAwayCurrentWeapon();
		void					failureStatePutAwayCurrentWeapon(const str& failureReason);

		void					setupStatePullOutNewWeapon();
		BehaviorReturnCode_t	evaluateStatePullOutNewWeapon();
		void					failureStatePullOutNewWeapon(const str& failureReason);

		void					setupStateReadyNewWeapon();
		BehaviorReturnCode_t	evaluateStateReadyNewWeapon();
		void					failureStateReadyNewWeapon( const str& failureReason );

	public:
		CLASS_PROTOTYPE( SelectBestWeapon );

										SelectBestWeapon();
									   ~SelectBestWeapon();

		void							SetArgs						( Event *ev );      
		void							AnimDone					( Event *ev );
		void							PostureDone					( Event *ev );

		void							Begin						( Actor &self ) override;		
		BehaviorReturnCode_t			Evaluate					( Actor &self ) override;
		void							End							( Actor &self ) override;
		
		void							SetCurrentEnemy ( Entity* enemy );

		static	bool					CanExecute( Actor &self );
		virtual void					Archive  ( Archiver &arc ) override;

		

	private: 	


	private: 
		selectBestWeaponStates_t	_state;
		Actor					   *_self;
		WeaponPtr					_bestWeapon;
		str							_currentWeaponName;
		str							_bestWeaponName;
		bool						_animDone;

	};

inline void	SelectBestWeapon::SetCurrentEnemy( Entity *enemy )
{
	_currentEnemy = enemy;
}

inline void SelectBestWeapon::Archive( Archiver &arc	)
{
	Behavior::Archive ( arc );	     

	arc.ArchiveSafePointer( &_currentEnemy );
	

	ArchiveEnum				( _state, selectBestWeaponStates_t   );
	arc.ArchiveObjectPointer( ( Class ** )&_self			);
	arc.ArchiveSafePointer	( &_bestWeapon );
	arc.ArchiveString		( &_currentWeaponName );
	arc.ArchiveString		( &_bestWeaponName );
	arc.ArchiveBool			( &_animDone );
}


#endif 
