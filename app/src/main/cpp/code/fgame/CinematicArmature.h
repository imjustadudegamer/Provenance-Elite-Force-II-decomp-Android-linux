//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/CinematicArmature.h                            $
// $Revision:: 24                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 5/16/03 8:27p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// DESCRIPTION: Classes related to supporting Cinematic Armature.  The
//				CinematicArmature itself is a singleton that governs
//				the loading, cacheing, playing, and stopping of 
//				cinematics.  The Cinematic class represents a single
//				cinematic with actors, cameras, objects, and associated
//				events.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


class Cinematic ;
class CinematicArmature ;
class CinematicActor ;
class CinematicCamera ;

#ifndef __CINEMATIC_ARMATURE_H_
#define __CINEMATIC_ARMATURE_H_

#include "g_local.h"
#include "actor.h"
#include "../corepp/container.h"

#include "cinematic.h"

extern CinematicArmature	theCinematicArmature ;
extern Event				EV_Cinematic_CinematicThink;
extern Event				EV_Cinematic_Start ;
extern Event				EV_Cinematic_Stop ;
extern Event				EV_Cinematic_Loop ;
extern Event				EV_Cinematic_Pause ;
extern Event				EV_Cinematic_Continue ;

typedef SafePtr<Cinematic> CinematicPtr;


class CinematicArmature : public Listener
{
	public:
		CLASS_PROTOTYPE(CinematicArmature );

								 CinematicArmature();
								 ~CinematicArmature();

		static bool				 isInDebugMode( void )					{ return _debug ; }

		void					 clearCinematicsList();

		Cinematic*				 createCinematic( const str &cinematicName );
		void					 deleteAllCinematics();

	protected:
		Cinematic*				 getCinematicByName( const str &cinematicName );

		void					 setStartThread( Event *ev );
		void					 setStopThread( Event *ev );

		void					 debugCinematics( Event *ev );
		void					 playCinematic( Event *ev );
		void					 playCinematicAt( Event *ev );
		void					 loadCinematic( Event *ev );
		bool					 loadCinematic( const str &cinematicName );  
    void Archive(Archiver&)  override;

	private:
		Container<CinematicPtr>	 _cinematicList ;
		Cinematic				*_cinematic ;
		static bool				 _debug ;
};


inline void CinematicArmature::Archive
(
	Archiver &arc
)
{

	int numCinematics = _cinematicList.NumObjects();
	arc.ArchiveInteger( &numCinematics );

	if ( arc.Loading() )
	{
		_cinematicList.Resize( numCinematics );
		for ( int cinematicIdx = 1; cinematicIdx <= numCinematics; ++cinematicIdx )
		{
			Cinematic		*cinematic		= 0 ;
			CinematicPtr	*cinematicPtr	= 0 ;

			_cinematicList.AddObject( cinematic );
			cinematicPtr = &_cinematicList.ObjectAt( cinematicIdx );
			arc.ArchiveSafePointer( cinematicPtr );
		}
	}
	else
	{
		for ( int cinematicIdx = 1; cinematicIdx <= numCinematics; ++cinematicIdx )
		{
			CinematicPtr *cinematic = &_cinematicList.ObjectAt( cinematicIdx );
			arc.ArchiveSafePointer( cinematic );
		}
	}

	arc.ArchiveObjectPointer( (Class**) &_cinematic );
	arc.ArchiveBool( &_debug );
}



#endif 
