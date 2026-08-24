//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/surfaceflags.h                                $
// $Revision:: 9                                                              $
//   $Author:: Jwaters                                                        $
//     $Date:: 7/30/02 8:35p                                                  $
//
// Copyright (C) 1999 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// $Log:: /Code/DLLs/game/surfaceflags.h                              $
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef TEAMMATE_ROSTER_HPP
#define TEAMMATE_ROSTER_HPP

#include "listener.h"
#include "container.h"
#include "str.h"

const int MAX_TEAMMATES=4;

typedef enum {
    HEALTHY_STATUS = 0,
    INJURED_STATUS,
    CRITICAL_STATUS
} TeammateStatus;

typedef struct 
{
	int		entNum;
	int		injuredHealthLevel;
	int		criticalHealthLevel;
	bool	active;
	str		archeType;

}TeammateData;

class TeammateRoster
{
	public:
		TeammateRoster();
		~TeammateRoster();

		static void					createInstance(void);
		static void					deleteInstance(void);

		static TeammateRoster*		getInstance(void)			{ if(_instance == 0) TeammateRoster::createInstance(); return _instance;	}

		TeammateStatus				getTeammateStatus(int index);

		void						addTeammate(const Entity* entity, int index);
		void						removeTeammate(int index);

		void						update(void);

		void						clearTeammates(void);

		void						Archive( Archiver &arc );

	protected:
		void						removeTeammateDataFromDatabase(int index);


	private:
		TeammateData				_teammateList[MAX_TEAMMATES];
		static TeammateRoster*		_instance;

		str							_healthyShader;
		str							_injuredShader;
		str							_criticalShader;
		str							_defaultShader;

		str							_healthyAnimation;
		str							_injuredAnimation;
		str							_criticalAnimation;
};


inline void TeammateRoster::Archive( Archiver& arc )
{
	TeammateData* teammateData;

	for(int i = 0; i < MAX_TEAMMATES; i++)
	{
		teammateData = &_teammateList[i];
		arc.ArchiveInteger(	&teammateData->entNum);
		arc.ArchiveInteger(	&teammateData->injuredHealthLevel);
		arc.ArchiveInteger(	&teammateData->criticalHealthLevel);
		arc.ArchiveBool(	&teammateData->active);
		arc.ArchiveString(	&teammateData->archeType);
	}


	arc.ArchiveString(&_healthyShader);
	arc.ArchiveString(&_injuredShader);
	arc.ArchiveString(&_criticalShader);
	arc.ArchiveString(&_defaultShader);

	arc.ArchiveString(&_healthyAnimation);
	arc.ArchiveString(&_injuredAnimation);
	arc.ArchiveString(&_criticalAnimation);

}

#endif

