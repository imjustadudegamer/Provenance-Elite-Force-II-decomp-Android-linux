
#ifndef STATIONARY_VEHICLE_HPP
#define STATIONARY_VEHICLE_HPP

#include "vehicle.h"


class StationaryVehicle : public Vehicle
{
	public:
		CLASS_PROTOTYPE( StationaryVehicle );
		StationaryVehicle();
		virtual ~StationaryVehicle();

		 void		Postthink() override;
		 qboolean	Drive( usercmd_t *ucmd ) override;
		void					PositionVehicleAndDriver(void);

		void					Killed(Event* event);

		virtual void			Archive(	Archiver &arc ) override;


	private:
		float					_yawDeltaDegrees;
		float					_pitchDeltaDegrees;
};

inline void StationaryVehicle::Archive( Archiver &arc )
{
	Vehicle::Archive( arc );

	arc.ArchiveFloat( &_yawDeltaDegrees );
	arc.ArchiveFloat( &_pitchDeltaDegrees );
}

#endif 
