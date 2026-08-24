//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/CinematicArmature.cpp                      $
// $Revision:: 44                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION:
//		Implementation of Cinematic Armature classes.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "CinematicArmature.h"











Event EV_CinematicArmature_PlayCinematic (    "playCinematic",
    EV_CONSOLE | EV_SCRIPTONLY,
    "sS",
    "cinematic originname",
    "Play the current cinematic or the specified one once."
);

Event EV_CinematicArmature_PlayCinematicAt (    "playCinematicAt",
    EV_CONSOLE | EV_SCRIPTONLY,
    "sVF",
    "cinematic origin yaw",
    "Play a cinematic at the specified origin with optional yaw rotatioin."   
);

Event EV_CinematicCamera_StopPlaying (    "cinematicCameraStopPlaying",
    EV_CODEONLY,
    NULL,
    NULL,
    "Stops a cinematic camera."
);

Event EV_CinematicCut_FadeOut (    "cinematiccutfadeout",
    EV_CODEONLY,
    "",
    "",
    "Fades out leading to a cinematic camera cut."
);





bool CinematicArmature::_debug = false ;

CinematicArmature	theCinematicArmature ;

CinematicArmature::CinematicArmature() :
	_cinematic( NULL )
{
}

Event EV_CinematicArmature_Debug
(
	"debug",
	EV_CONSOLE | EV_SCRIPTONLY,
	NULL,
	NULL,
	"Turns on debugging information for cinematics."
);

CLASS_DECLARATION( Listener, CinematicArmature, NULL )
{
	{ &EV_CinematicArmature_PlayCinematic,		&CinematicArmature::playCinematic },
	{ &EV_CinematicArmature_PlayCinematicAt,	&CinematicArmature::playCinematicAt },
	{ &EV_CinematicArmature_Debug,				&CinematicArmature::debugCinematics },

	{ NULL, NULL }
};


void CinematicArmature::playCinematic( Event *ev )
{
	if ( ev->NumArgs() >= 1)
	{
		str cinematicName = ev->GetString( 1 );
		_cinematic = getCinematicByName( cinematicName ) ;
		if ( !_cinematic )
		{
			if ( !loadCinematic( cinematicName ))
			{
				ev->Error("playCinematic: Unable to load cinematic %s\n", cinematicName.c_str() );
				return ;
			}
		}
	}
	else if ( !_cinematic )
	{
		ev->Error("startCinematic: No current cinematic to start!" );
		return ;
	}

	assert( _cinematic ) ;

	str originName ;
	if ( ev->NumArgs() == 2 )
	{
		originName = ev->GetString( 2 );
	}

	_cinematic->startAtNamedOrigin( originName );
}


void CinematicArmature::playCinematicAt( Event *ev )
{
	if ( ev->NumArgs() >= 1)
	{
		str cinematicName = ev->GetString( 1 );
		_cinematic = getCinematicByName( cinematicName ) ;
		if ( !_cinematic )
		{
			if ( !loadCinematic( cinematicName ))
			{
				ev->Error("playCinematicat: Unable to load cinematic %s\n", cinematicName.c_str() );
				return ;
			}
		}
	}
	else if ( !_cinematic )
	{
		ev->Error("playCinematicat: No current cinematic to start!" );
		return ;
	}

	assert( _cinematic ) ;
	Vector	origin( 0.0f, 0.0f, 0.0f ) ;
	float	yaw	= 0.0f ;

	if ( ev->NumArgs() >= 2 )
	{
		origin = ev->GetVector( 2 );
	}

	if ( ev->NumArgs() == 3 )
	{
		yaw = ev->GetFloat( 3 );
	}

	_cinematic->startAtOrigin( origin, yaw );
}


void CinematicArmature::debugCinematics( Event *ev )
{
	_debug = !_debug ;
}


Cinematic* CinematicArmature::getCinematicByName( const str &cinematicName )
{
	int numCinematics = _cinematicList.NumObjects();
	for (int cinematicIndex = 1; cinematicIndex <= numCinematics; cinematicIndex++)
	{
		Cinematic *cinematic = _cinematicList.ObjectAt(cinematicIndex);
		if (cinematic->targetname == cinematicName)
			return cinematic ;
	}
	return 0 ; 
}


















CinematicArmature::~CinematicArmature( void )
{
	deleteAllCinematics();
	_cinematicList.FreeObjectList();
	_cinematic = NULL ;
}


bool CinematicArmature::loadCinematic( const str &cinematicName )
{
	CinematicPtr	cinematic = 0 ;
	str				filename;

	filename  = "cins/";
	filename += cinematicName ;
	filename += ".cin";

	cinematic = new Cinematic ( filename );
	if ( !cinematic->load() )
	{
		return false ;
	}

	_cinematicList.AddObject(cinematic);
	_cinematic = cinematic ; 

	return true ;
}

Cinematic* CinematicArmature::createCinematic( const str &cinematicName )
{
	if ( loadCinematic( cinematicName ) )
	{
		return _cinematic ;
	}

	return 0;
}


void CinematicArmature::deleteAllCinematics( void )
{
	int numCinematics = _cinematicList.NumObjects();
	for (int cinematicIdx = 1; cinematicIdx <= numCinematics; cinematicIdx++)
	{
		Cinematic *cinematic = _cinematicList.ObjectAt(cinematicIdx);
		delete cinematic ;
	}
}

void CinematicArmature::clearCinematicsList( void )
{
	_cinematicList.ClearObjectList();
}
