
#ifndef PUZZLE_OBJECT_HPP
#define PUZZLE_OBJECT_HPP

#include "entity.h"


class PuzzleObject : public Entity
{
	public:
		CLASS_PROTOTYPE( PuzzleObject );

		typedef enum
		{
			PUZZLE_STATE_IDLE,			
			PUZZLE_STATE_IDLE_SOLVED,	
			PUZZLE_STATE_IDLE_LOCKED,	

			PUZZLE_STATE_OPENING,		
			
			PUZZLE_STATE_IDLE_OPEN,		
			
			PUZZLE_STATE_ACTIVE_OPEN,	

			PUZZLE_STATE_CLOSING,		
			PUZZLE_STATE_CLOSING_SOLVED,
			PUZZLE_STATE_CLOSING_LOCKED,

			PUZZLE_STATE_DEACTIVATED

		}PuzzleState;


		PuzzleObject();
		virtual ~PuzzleObject();

		PuzzleState			getPuzzleState(void)					{ return _puzzleState;			}
		void				setPuzzleState(PuzzleState puzzleState)	{ _puzzleState = puzzleState;	}

		void				setOpenDistance(Event* event);

		void				setItemUsedThread(Event* event);
		void				setFailedThread(Event* event);
		void				setSolvedThread(Event* event);
		void				setCanceledThread(Event* event);

		void				failed(Event* event);
		void				canceled(Event* event);
		void				solved(Event* event);
		void				reset( Event* event );

		void				animationDone(Event* event);
		void				setItemToUse(Event* event);
		void				useEvent(Event* event);
		void				activate( Event* event );
		void				deActivate( Event* event );

		void				normalUse( void );
		void				timedUse( void );

		void				setTimeToUse( Event *ev );
		void				timedPuzzleSolved( void );
		void				timedPuzzleCanceled( void );
		void				showTimerHud( void );
		void				hideTimerHud( void );

		void				setTimerHudName( Event * ev );
		
		void				becomeModBarInSkill( Event* ev );

		 void	Archive(Archiver &arc) override;
		 void	Think( void ) override;

	private:
		str					_itemToUse;
		str					_itemUsedThread;
		str					_failedThread;
		str					_solvedThread;
		str					_canceledThread;

		PuzzleState			_puzzleState;
		PuzzleState			_saveState;
		float				_openDistance;

		bool				_timed;
		float				_timeToUse;
		float				_lastTimeUsed;
		float				_usedTime;
		bool				_hudOn;

		float				_nextNeedToUseTime;

		str					_hudName;
		int					_minSkill; 
};


inline void PuzzleObject::Archive( Archiver &arc )
{
	Entity::Archive( arc );

	arc.ArchiveString( &_itemToUse				);
	arc.ArchiveString( &_itemUsedThread			);
	arc.ArchiveString( &_failedThread			);
	arc.ArchiveString( &_solvedThread			);
	arc.ArchiveString( &_canceledThread			);
	ArchiveEnum( _puzzleState, PuzzleState	);
	ArchiveEnum( _saveState, PuzzleState );

	arc.ArchiveFloat( &_openDistance );

	arc.ArchiveBool( &_timed );
	arc.ArchiveFloat( &_timeToUse );
	arc.ArchiveFloat( &_lastTimeUsed );
	arc.ArchiveFloat( &_usedTime );
	arc.ArchiveBool( &_hudOn );

	arc.ArchiveFloat( &_nextNeedToUseTime );

	arc.ArchiveString( &_hudName );
	
	arc.ArchiveInteger( &_minSkill );
}

#endif
