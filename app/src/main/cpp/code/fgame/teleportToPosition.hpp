
class AnimatedTeleportToPosition;

#ifndef __TELEPORT_TO_POSITION_H__
#define __TELEPORT_TO_POSITION_H__

#include "behavior.h"


class AnimatedTeleportToPosition : public Behavior
	{
	public:
		typedef enum
			{
			ANIM_TELEPORT_BEGIN,
			ANIM_TELEPORT_START_ANIM,
			ANIM_TELEPORT_START_ANIMATING,
			ANIM_TELEPORT_TELEPORT,
			ANIM_TELEPORT_END_ANIM,
			ANIM_TELEPORT_END_ANIMATING,
			} animTeleportStates_t;

	private:
		str						_teleportPositionName;
		int						_numberOfTeleportPositions;
		str						_startAnim;
		str						_endAnim;


	public:
		CLASS_PROTOTYPE( AnimatedTeleportToPosition );

		void					SetArgs( Event *ev );
		void					Begin( Actor &self ) override;
		BehaviorReturnCode_t	Evaluate( Actor &self ) override;
		void					End( Actor &self ) override;
		virtual void			Archive( Archiver &arc ) override;

	private:		
		animTeleportStates_t	_state;
		PathNodePtr				_goal;

	};

inline void AnimatedTeleportToPosition::Archive( Archiver &arc )
{
	Behavior::Archive( arc );

	arc.ArchiveString		( &_teleportPositionName	    );
	arc.ArchiveInteger		( &_numberOfTeleportPositions   );
	arc.ArchiveString		( &_startAnim				    );
	arc.ArchiveString		( &_endAnim				        );

	ArchiveEnum				(  _state, animTeleportStates_t );
	arc.ArchiveSafePointer	( &_goal					    );
}



#endif 
