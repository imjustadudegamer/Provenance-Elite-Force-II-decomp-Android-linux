//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/playerheuristics.h                            $
// $Revision:: 10                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 9:11a                                                 $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// PlayerHeuristics Class Definition.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


class PlayerHeuristics;


#ifndef __PLAYER_HEURISTICS_H__
#define __PLAYER_HEURISTICS_H__

#include "player.h"

class PlayerHeuristics : public Class
{
	public:
		
				 PlayerHeuristics();
				 PlayerHeuristics( const PlayerHeuristics &pHeuristic );
				~PlayerHeuristics();
		
		void	ShowHeuristics( const Player *player );
		void	SaveHeuristics( const Player *player );
		void	LoadHeuristics();
		void	ReadInHeuristicData(Script& script, str& levelName);
		void	WriteOutHeuristicData( const str& levelName );
		void	CreateInitialHeuristicFile();
		void	OpenPlayerHeuristicFile();
		void	ClosePlayerHeuristicFile();
		void	CheckForHeuristicFile();
		float	CalculateSkillLevel();
		void	ClearLevelStatistics(void);
		
		void	SetShotsFired( long int shots);	
		void	IncrementShotsFired( void );
		void	SetShotsHit( long int shots);
		void	IncrementShotsHit();
		void	SetNumberOfDeaths( long int deaths);  
		void	IncrementNumberOfDeaths();
		void	SetTimeOnLevel( float levelTime );
		void	SetPlayerHealth( int health );
		void	SetPing (int ping_value );
		void	SetItemsPickedUp ( int items );
		void	IncrementItemsPickedUp ();

		void	IncrementTeammatesKilled(void);

		void	SetSpecialEvents( int specialevent );
		void	IncrementSpecialEvents();
		void	UpdateShotsFired( void );
		void	SetEnemiesKilled( long int enemies );
		void	IncrementEnemiesKilled(void);

		void	SetObjectives(int numMissionObjectives);
		void	IncrementCompleteObjectives();
		void	IncrementFailedObjectives();


		
		long int	shotsFired;
		long int	shotsHit;
		long int	numberOfDeaths;
		long int	enemiesKilled;

		long int	shotsFiredInLevel;		
		long int	shotsHitInLevel;
		long int	enemiesKilledInLevel;
		long int	teammatesKilledInLevel;

		int			numObjectives;
		int			numCompleteObjectives;
		int			numFailedObjectives;
		

		float		timeOnLevel;
		float		levelStartTime;
		float		levelEndTime;
		float		playerHealth;
		str			lastLevel;
		str			currentLevel;

		int			ping;
		int			itemsPickedUp;
		int			specialEvents;
		


		float		skillLevel;

};




#endif 
