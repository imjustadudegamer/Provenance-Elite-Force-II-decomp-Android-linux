
class RotateToEntity;

#ifndef __ROTATE_TO_ENTITY_H__
#define __ROTATE_TO_ENTITY_H__

#include "behavior.h"

class RotateToEntity : public Behavior
	{

	private:      
		float							_turnspeed;
		EntityPtr						_ent;
		str								_anim;
		str								_entityType;

	public:
		CLASS_PROTOTYPE( RotateToEntity );

										RotateToEntity();
									   ~RotateToEntity();


		void							SetArgs      ( Event *ev                          );      
		void							Begin        ( Actor &self                        ) override;		
		BehaviorReturnCode_t			Evaluate     ( Actor &self                        ) override;
		void							End          ( Actor &self                        ) override;
		virtual void					Archive      ( Archiver &arc                      ) override;
		
		void							SetAnim      ( const str& animName                );
		void							SetTurnSpeed ( float turnSpeed                    );
		void							SetEntity    ( Entity *entity                     );      
		void							SetEntityType( const str &ent					  );

	private:		
		float							_oldTurnSpeed;
		
	
	};

inline void RotateToEntity::SetTurnSpeed( float turnSpeed )
	{
	_turnspeed = turnSpeed;
	}


inline void RotateToEntity::SetEntity( Entity *entity )
	{
	if ( entity )
		_ent = entity;
	}

inline void RotateToEntity::SetAnim( const str &animName )
	{
	_anim = animName;   
	}

inline void RotateToEntity::SetEntityType( const str &ent )
	{
		if  ( ent == "player" || ent == "enemy" )
			_entityType = ent;
	}

inline void RotateToEntity::Archive( Archiver &arc	)
{
	Behavior::Archive( arc );

	arc.ArchiveFloat       ( &_turnspeed );
	arc.ArchiveSafePointer ( &_ent       );
	arc.ArchiveString      ( &_anim      );
	arc.ArchiveString( &_entityType );

	arc.ArchiveFloat	   ( &_oldTurnSpeed );
}


#endif 
