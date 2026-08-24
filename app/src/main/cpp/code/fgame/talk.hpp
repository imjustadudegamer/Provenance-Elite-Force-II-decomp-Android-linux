
class Talk;

#ifndef __TALK_HPP___
#define __TALK_HPP___

#include "behavior.h"
#include "behaviors_general.h"

class Talk : public Behavior
{
public:
	typedef enum
	{
		TALK_MODE_TURN_TO,
		TALK_MODE_COMBAT,
		TALK_MODE_PLAY_GREETING,
		TALK_MODE_WAIT_FOR_GREETING,
		TALK_MODE_TALK,
		TALK_MODE_WAIT,
		TALK_MODE_TURN_BACK,
		TALK_SUCCESS,
		TALK_FAILED
	} TalkStates_t;

private:
	TurnTo					turnto;
	SentientPtr				ent_listening;
	EntityPtr				last_headwatch_target;
	float					original_yaw;
	float					yaw;
	int						mode;
	qboolean				move_allowed;
	bool					animDone;			 
	str						oldAnimName;

public:
	CLASS_PROTOTYPE( Talk );

	void					SetUser( Sentient *user );
	void					GreetingDone( Event *ev );
	void					AnimDone( Event *ev );
	void					Begin( Actor &self ) override;
	BehaviorReturnCode_t	Evaluate( Actor &self ) override;
	void					End( Actor &self ) override;
	virtual void			Archive( Archiver &arc ) override;
};

inline void Talk::Archive( Archiver &arc )
{
	Behavior::Archive( arc );

	arc.ArchiveObject( &turnto );
	arc.ArchiveSafePointer( &ent_listening );
	arc.ArchiveSafePointer( &last_headwatch_target );
	arc.ArchiveFloat( &original_yaw );
	arc.ArchiveFloat( &yaw );
	arc.ArchiveInteger( &mode );
	arc.ArchiveBoolean( &move_allowed );
	arc.ArchiveBool( &animDone );		 
	arc.ArchiveString( &oldAnimName );
}

#endif 
