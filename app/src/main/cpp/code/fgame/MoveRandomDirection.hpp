

class MoveRandomDirection;

#ifndef __MOVE_RANDOM_DIRECTION___
#define __MOVE_RANDOM_DIRECTION___

#include "behavior.h"
#include "behaviors_general.h"


#define MIN_RANDOM_DIRECTION_DESTINATION 64.0f

class MoveRandomDirection : public Behavior
	{
	public: 
		typedef enum
		{
		RANDOM_MOVE_ANYWHERE,
		RANDOM_MOVE_IN_FRONT,
		RANDOM_MOVE_IN_BACK,
		} randomMoveModes_t;

	private: 
		str							anim;

	protected:
		Vector		_chooseRandomDirection    ( Actor &self );
		float		_getDistanceToDestination ( Actor &self );
		void		findDestination			  ( Actor &self );
		void		setLegAnim				  ( Actor &self );
		void		setTorsoAnim			  ( Actor &self );

	public:
		CLASS_PROTOTYPE( MoveRandomDirection );

									MoveRandomDirection();
									~MoveRandomDirection();

		void						SetArgs( Event *ev );      
		void						Begin( Actor &self ) override;
		BehaviorReturnCode_t		Evaluate( Actor &self ) override;
		void						End( Actor &self ) override;
		virtual void				Archive( Archiver &arc ) override;

		void SetDistance( float dist );
		void SetMinDistance( float dist );
		void SetAnim( const str &moveAnim );
		void SetMode( unsigned int mode );

	private: 
		GotoPoint						_chase;

		Vector							_destination;
		unsigned int					_mode;
		float							_dist;
		float							_minDistance;
		float							_nextChangeTime;      
		bool							_foundGoodDestination;
		bool							_forever;
		bool							_faceEnemy;
		str								_torsoAnim;

	};

inline void MoveRandomDirection::Archive( Archiver &arc	)
	{
	Behavior::Archive( arc );

	arc.ArchiveString( &anim );

	arc.ArchiveObject( &_chase );

	arc.ArchiveVector( &_destination );   
	arc.ArchiveUnsigned( &_mode );
	arc.ArchiveFloat( &_dist );
	arc.ArchiveFloat( &_minDistance );
	arc.ArchiveFloat( &_nextChangeTime );
	arc.ArchiveBool( &_foundGoodDestination );
	arc.ArchiveBool( &_forever );
	arc.ArchiveBool( &_faceEnemy );
	arc.ArchiveString( &_torsoAnim );
	}




#endif 

