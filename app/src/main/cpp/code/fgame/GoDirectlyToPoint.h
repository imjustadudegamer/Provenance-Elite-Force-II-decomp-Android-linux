
#ifndef __GO_DIRECTLY_TO_POINT__
#define __GO_DIRECTLY_TO_POINT__

#include "g_local.h"
#include "steering.h"

class Actor;


class GoDirectlyToPoint : public Steering
{
public:
								CLASS_PROTOTYPE( GoDirectlyToPoint );

								GoDirectlyToPoint();
	virtual						~GoDirectlyToPoint() { }
	virtual void				Begin( Actor &self ) override;
	virtual const ReturnValue	Evaluate( Actor &self ) override;
	virtual void				ShowInfo( Actor &self ) override;

	void						SetDestination( const Vector &destination ) { _destination = destination; }
	void						SetRadius( const float radius ) { _radius = radius; }
	virtual void				Archive( Archiver &arc ) override;
	const bool					AtDestination( const Actor &self ) const;

private:
	Vector						_destination;
	float						_radius;
};

inline void GoDirectlyToPoint::Archive( Archiver &arc )
	
{
	Steering::Archive( arc );
	arc.ArchiveVector( &_destination );
	arc.ArchiveFloat( &_radius );
	
}

#endif 
