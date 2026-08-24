//-----------------------------------------------------------------------------
// 
//  $Logfile:: /EF2/Code/DLLs/game/RageAI.cpp                        $
// $Revision:: 51                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2001 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// The Rage AI System, will house several components that will make the AI function
// such as the Strategos, Personality, etc...
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).
#include "_pch_cpp.h"
#include "rageai.h"
#include "player.h"
#include "actor_enemymanager.h"
#include "actor_locomotion.h"
#include "fuzzyengine.h"

Strategos::~Strategos()
{
}

void Strategos::DoArchive( Archiver &arc , Actor *actor )
{
	Archive( arc );
}

void Strategos::Archive( Archiver &arc )
{
	arc.ArchiveFloat( &_sightBasedHate );
	arc.ArchiveFloat( &_nextEvaluateTime );
	arc.ArchiveFloat( &_evaluateInterval );
}


DefaultStrategos::DefaultStrategos()
{
	gi.Error( ERR_FATAL, "DefaultStrategos::DefaultStrategos -- Default Constructor Called" );
}


DefaultStrategos::DefaultStrategos( Actor *actor )
	: _checkInConeDistMax( 128 )
{
	if ( actor )
		act = actor;
	else 
		gi.Error( ERR_FATAL, "DefaultStrategos::DefaultStrategos -- actor is NULL" );
	
	SetSightBasedHate( DEFAULT_SIGHT_BASED_HATE );
	SetEvaluateInterval( DEFAULT_EVALUATE_INTERVAL );
	SetNextEvaluateTime( 0.0f );	
}

DefaultStrategos::~DefaultStrategos()
{
	
}

void DefaultStrategos::NotifySightStatusChanged( Entity *enemy , qboolean canSee )
{
}



void DefaultStrategos::NotifyDamageChanged( Entity *enemy , float damage )
{
}



void DefaultStrategos::Evaluate()
{
	_EvaluateWorld();
	_EvaluatePackages();
}


void DefaultStrategos::_EvaluateEnemies()
{
	if ( act->enemyManager->GetCurrentEnemy() && (level.time < GetNextEvaluateTime() || act->enemyManager->IsLockedOnCurrentEnemy() ))
		return;

	act->enemyManager->FindHighestHateEnemy();
	SetNextEvaluateTime( level.time + GetEvaluateInterval() );
}



void DefaultStrategos::_EvaluatePackages()
{
   
	if ( !act->fuzzyEngine || !act->fuzzyEngine_active )
		return;

	if ( PackageList.NumObjects() < 1 )
		return;

	act->packageManager->EvaluatePackages(act->fuzzyEngine );
	
	int newPackageIndex;
	newPackageIndex = act->packageManager->GetHighestScoringPackage();

	if ( newPackageIndex < 1 || newPackageIndex > PackageList.NumObjects() )
		return;

	BehaviorPackageType_t* package;
	
	package = PackageList.ObjectAt( newPackageIndex );

	if ( package && Q_stricmp( act->statemap_name.c_str(), package->stateFile.c_str() ) )
	{
		Event *event;

		event = new Event( EV_Actor_Statemap );
		event->AddString( package->stateFile.c_str() );  
		event->AddString( "START" );                     
		event->AddInteger( 0 );                          
		event->AddInteger( 1 );                          
		act->ProcessEvent( event );		

		act->packageManager->UpdateCurrentPackageIndex( newPackageIndex );
		act->statemap_name = package->stateFile;
	}
}


void DefaultStrategos::_EvaluateWorld()
{
	_CheckForInTheWay();
	_CheckForInConeOfFire();   
}

void DefaultStrategos::_CheckForInConeOfFire()
{
	Vector dir;
	Vector check;
	float relativeYaw;
	float distance;
	Sentient *teammate;
	Player *player;
	Actor *actor;

	player = NULL;
	teammate = NULL;
   
   
	for ( int i = 1 ; i <= TeamMateList.NumObjects() ; i++ )
	{
		teammate = TeamMateList.ObjectAt( i );
		
		if ( !teammate || teammate->entnum == act->entnum )
			continue;
		
		dir = act->origin - teammate->origin;
		distance = dir.length();  
		
		if ( distance < _checkInConeDistMax )
		{
			dir = dir.toAngles();
			
			if ( teammate->isSubclassOf( Player ) )
            {
				_checkYawMin = -5.0f;
				_checkYawMax =  5.0f;
				
				
				player = (Player*)teammate;
				check = player->GetVAngles();
            }
			else
            {
				
				_checkYawMin = -5.0f;
				_checkYawMax =  5.0f;
				
				actor = (Actor*)teammate;
				check = actor->movementSubsystem->getAnimDir();
				check = check.toAngles();
            }
            
			
			relativeYaw = AngleNormalize180( AngleNormalize180(check[YAW]) - AngleNormalize180(dir[YAW]) );
			
			if ( (relativeYaw <= _checkYawMax) && (relativeYaw >= _checkYawMin ) )
            {
				act->SetActorFlag( ACTOR_FLAG_IN_CONE_OF_FIRE , true );      

				if ( teammate->isSubclassOf(Player) && distance <= 128 )
					act->SetActorFlag( ACTOR_FLAG_IN_PLAYER_CONE_OF_FIRE , true );

				return;
            }		
		}
	}
	
	act->SetActorFlag( ACTOR_FLAG_IN_CONE_OF_FIRE , false );
	act->SetActorFlag( ACTOR_FLAG_IN_PLAYER_CONE_OF_FIRE , false );
	
}

void DefaultStrategos::_CheckForInTheWay()
{
	Player *player;   
	Vector playerToSelf;
	Vector playerVelocity;
	Vector dir;
	Vector check;
	float relativeYaw;
	float dist;
	
	player = GetPlayer( 0 );
	
	if (!player)
		return;
	
	playerToSelf = act->origin - player->origin;
	dist = playerToSelf.length();
	
	playerVelocity = player->velocity;
	
	float speed;
	speed = playerVelocity.length();
	speed = speed * .75;
	
	if ( dist > 200 )
		return;
	
	if ( dist > speed )
		return;
	
	if ( DotProduct( playerVelocity , playerToSelf ) <= 0 )
		return;
	
	if ( act->EntityHasFireType( player, FT_BULLET ) || act->EntityHasFireType( player, FT_PROJECTILE ) )
	{
		_checkYawMin = -30.0f;
		_checkYawMax =  30.0f;
		
		check = player->GetVAngles();
		dir = playerToSelf.toAngles();
		
		relativeYaw = AngleNormalize180( AngleNormalize180(check[YAW]) - AngleNormalize180(dir[YAW]) );
		
		if ( (relativeYaw <= _checkYawMax) && (relativeYaw >= _checkYawMin ) )
		{
			act->AddStateFlag( STATE_FLAG_TOUCHED_BY_PLAYER );
			act->AddStateFlag( STATE_FLAG_IN_THE_WAY );
		}
		
	}
	
}

float DefaultStrategos::GetCheckYawMax()
{
	return _checkYawMax;
}

float DefaultStrategos::GetCheckYawMin()
{
	return _checkYawMin;
}

float DefaultStrategos::GetCheckInConeDistMax()
{
	return _checkInConeDistMax;
}

void DefaultStrategos::SetCheckInConeDistMax( float distance )
{
	assert( distance >= 0 );
	_checkInConeDistMax = distance;
}

void DefaultStrategos::Attack( Entity *enemy )
{
	act->enemyManager->SetCurrentEnemy( enemy );
	act->enemyManager->LockOnCurrentEnemy( true );
}


void DefaultStrategos::SetBehaviorPackage( const str &packageName )
{
	int index;
	BehaviorPackageType_t* package;
	
	index = act->packageManager->GetPackageIndex( packageName );
	if ( index == -1 )
	{
		// invariant, but it is compiled out under NDEBUG (both here and in the shipped
		assert ( index != -1 );
		gi.WDPrintf( "Actor %s, Does not have package %s registered\n" , act->targetname.c_str() , packageName.c_str() );
		return;
	}
	
	
	package = PackageList.ObjectAt( index );
	
	if ( package && stricmp( act->statemap_name.c_str(), package->stateFile.c_str() ) )
	{
		Event *event;
		
		event = new Event( EV_Actor_Statemap );
		event->AddString( package->stateFile.c_str() );  
		event->AddString( "START" );                     
		event->AddInteger( 0 );                          
		event->AddInteger( 1 );                          
		act->ProcessEvent( event );	      
		act->packageManager->UpdateCurrentPackageIndex( index );
		act->statemap_name = package->stateFile;
	}
}

void DefaultStrategos::DoArchive( Archiver &arc , Actor *actor )
{
	Archive( arc );
	
	if ( actor )
		act = actor;
	else 
		gi.Error( ERR_FATAL, "DefaultStrategos::DoArchive -- actor is NULL" );
	
}

void DefaultStrategos::Archive( Archiver &arc )
{
	Strategos::Archive( arc );

	arc.ArchiveFloat( &_checkYawMin );
	arc.ArchiveFloat( &_checkYawMax );
	arc.ArchiveFloat( &_checkInConeDistMax );
}




PackageManager::PackageManager()
{
	gi.Error( ERR_FATAL, "PackageManager::PackageManager -- Default Constructor Called" );
}



PackageManager::PackageManager( Actor *actor )
{
	if ( actor )
		act = actor;
	else 
		gi.Error( ERR_FATAL, "PackageManager::PackageManager -- actor is NULL" );
	
	_currentPackageIndex = -1;
}



PackageManager::~PackageManager()
{
	
}



void PackageManager::RegisterPackage( const str &packageName )
{
	BehaviorPackageEntry_t pEntry;
	BehaviorPackageType_t  *package;
	BehaviorPackageEntry_t *checkEntry;
	
	for ( int i = 1 ; i <= PackageList.NumObjects() ; i++ )
	{
		package = PackageList.ObjectAt( i );
		
		if ( !Q_stricmp( packageName.c_str() , package->packageName.c_str() ) )
		{
			for ( int j = 1 ; j <= _BehaviorPackages.NumObjects() ; j++ )
			{
				checkEntry = &_BehaviorPackages.ObjectAt( j );
				
				if ( checkEntry->packageIndex == i )
					return;
			}
			
			pEntry.currentScore     = 0;
			pEntry.lastScore        = 0;	
			pEntry.lastTimeExecuted = 0.0f;
			pEntry.priority         = 0.001f;
			pEntry.packageIndex     = i;
			
			_BehaviorPackages.AddObject( pEntry );
			
			
			CacheResource( package->stateFile, act );
			G_CacheStateMachineAnims( act, package->stateFile );
			
			return;
		}
	}
}



void PackageManager::UnregisterPackage( const str &packageName )
{
	BehaviorPackageEntry_t pEntry;
	BehaviorPackageType_t *package;
	int index = 0;
	int i;
	
	for ( i = 1 ; i <= PackageList.NumObjects() ; i++ )
	{
		package = PackageList.ObjectAt( i );
		if ( !Q_stricmp( packageName.c_str() , package->packageName.c_str() ) )
			index = i;
	}
	
	for ( i = _BehaviorPackages.NumObjects() ; i > 0 ; i-- )
	{
		pEntry = _BehaviorPackages.ObjectAt( i );
		if ( pEntry.packageIndex == index )
			_BehaviorPackages.RemoveObjectAt( i );
	}
}


int PackageManager::GetHighestScoringPackage()
{
	BehaviorPackageEntry_t pEntry;
	float points;
	int packageIndex = -1;
	
	points = 0;
	
	for ( int i = 1 ; i <= _BehaviorPackages.NumObjects() ; i++ )
	{
		pEntry = _BehaviorPackages.ObjectAt( i );
		
		if ( pEntry.currentScore > points )
		{
			points = pEntry.currentScore;
			packageIndex = pEntry.packageIndex;
		}
	}
	
	return packageIndex;
}


int PackageManager::GetCurrentFVarIndex()
{
	return _currentFVarIndex;
}

float PackageManager::GetCurrentFVarLastExecuteTime()
{
	return _currentFVarLastExecuteTime;
}

void PackageManager::EvaluatePackages( FuzzyEngine *fEngine )
{
	
	FuzzyVar               *currentFVar;
	BehaviorPackageEntry_t *currentEntry;
	BehaviorPackageType_t  *package;
	
	for ( int i = 1 ; i <= _BehaviorPackages.NumObjects() ; i++ )
	{
		currentEntry = &_BehaviorPackages.ObjectAt( i );
		
		_currentFVarIndex           = currentEntry->packageIndex;
		_currentFVarLastExecuteTime = currentEntry->lastTimeExecuted;
		
		package = PackageList.ObjectAt( _currentFVarIndex );
		
		
		currentFVar = fEngine->FindFuzzyVar( package->packageName.c_str() );
		
		if ( currentFVar )
		{
			currentEntry->lastScore = currentEntry->currentScore;
			currentEntry->currentScore = currentFVar->Evaluate( *act , &act->fuzzy_conditionals );
		}
		
		
	}
	
}


void PackageManager::SetLastExecutionTime(int packageIndex)
{
	BehaviorPackageEntry_t *currentEntry;
	
	
	for ( int i = 1 ; i <= _BehaviorPackages.NumObjects() ; i++ )
	{
		currentEntry = &_BehaviorPackages.ObjectAt( i );
		
		if ( currentEntry->packageIndex == packageIndex )
		{
			currentEntry->lastTimeExecuted = level.time;
			return;
		}
		
	}
}

void PackageManager::UpdateCurrentPackageIndex( int packageIndex )
{
	if ( _currentPackageIndex > -1 )
		SetLastExecutionTime( _currentPackageIndex );
	
	_currentPackageIndex = packageIndex;
}


str PackageManager::GetCurrentPackageName()
{
	BehaviorPackageType_t* package;	
	package = PackageList.ObjectAt( _currentPackageIndex );

	return package->packageName;
}

int PackageManager::GetPackageIndex( const str &packageName )
{
	BehaviorPackageEntry_t pEntry;		
	BehaviorPackageType_t  *package;	
	
	for ( int i = 1 ; i <= _BehaviorPackages.NumObjects() ; i++ )
	{
		pEntry = _BehaviorPackages.ObjectAt( i );
		package = PackageList.ObjectAt( pEntry.packageIndex );
		
		if ( !stricmp( package->packageName.c_str() , packageName.c_str() ) )
			return pEntry.packageIndex;
	}
	
	return -1;
}

void PackageManager::DoArchive( Archiver &arc , Actor *actor )
{
	int i;
	BehaviorPackageEntry_t *pEntry;
	BehaviorPackageType_t  *package;
	int numPackages;
	str packageName;

	if ( actor )
		act = actor;
	else 
		gi.Error( ERR_FATAL, "PackageManager::DoArchive -- actor is NULL" );

	if ( arc.Loading() )
	{
		arc.ArchiveInteger( &numPackages );

		for ( i = 1 ; i <= numPackages ; i++ )
		{
			arc.ArchiveString( &packageName );

			RegisterPackage( packageName );


			pEntry = &_BehaviorPackages.ObjectAt( _BehaviorPackages.NumObjects() );

			arc.ArchiveFloat( &pEntry->currentScore );
			arc.ArchiveFloat( &pEntry->lastScore );
			arc.ArchiveFloat( &pEntry->lastTimeExecuted );
			arc.ArchiveFloat( &pEntry->priority );
		}

	}
	else
	{
		numPackages = _BehaviorPackages.NumObjects();
		arc.ArchiveInteger( &numPackages );

		for ( i = 1 ; i <= _BehaviorPackages.NumObjects() ; i++ )
		{
			pEntry = &_BehaviorPackages.ObjectAt( i );

			package = PackageList.ObjectAt( pEntry->packageIndex );

			arc.ArchiveString( &package->packageName );

			arc.ArchiveFloat( &pEntry->currentScore );
			arc.ArchiveFloat( &pEntry->lastScore );
			arc.ArchiveFloat( &pEntry->lastTimeExecuted );
			arc.ArchiveFloat( &pEntry->priority );
		}
	}

	arc.ArchiveInteger( &_currentFVarIndex );
	arc.ArchiveFloat( &_currentFVarLastExecuteTime );
	arc.ArchiveInteger( &_currentPackageIndex );
}





Personality::Personality()
{
	gi.Error( ERR_FATAL, "Personality::Personality -- Default Constructor Called" );
}


Personality::Personality( Actor *actor )
{
	if ( actor )
		act = actor;
	else 
		gi.Error( ERR_FATAL, "Personality::Personality -- actor is NULL" );
	
	_aggressiveness         = 0.0f;
	_talkiness              = 0.0f;
	
}


Personality::~Personality()
{
	
}



void Personality::SetAggressiveness( float aggressiveness )
{
	_aggressiveness = _clampValue( aggressiveness );
}

float Personality::GetAggressiveness()
{
	return _aggressiveness;
}


void Personality::SetTalkiness( float talkiness )
{
	_talkiness = _clampValue( talkiness );
}

float Personality::GetTalkiness()
{  
	return _talkiness;
}

float Personality::_clampValue( float value )
{
	if ( value > 1.0f )
		value = 1.0f;
	
	if ( value < 0.0f )
		value = 0.0f;
	
	return value;
}

float Personality::GetTendency( const str &tendencyName )
{
	Tendency_t tendency;
	
	for ( int i = 1 ; i <= _Tendencies.NumObjects() ; i++ )
	{
		tendency = _Tendencies.ObjectAt( i );
		
		if ( !Q_stricmp( tendencyName.c_str() , tendency.tendencyName.c_str() ) )
		{
			return tendency.tendencyValue;
		}
		
	}   
	
	return 0.0f;
}

void Personality::DoArchive( Archiver &arc , Actor *actor )
{
	Archive( arc );
	if ( actor )
		act = actor;
	else 
		gi.Error( ERR_FATAL, "Personality::DoArchive -- actor is NULL" );
	
}

void Personality::Archive( Archiver &arc )
{
	int i;
	int j;
	int num;
	str packageName;
	BehaviorPackageType_t  *package;
	PackageTendency_t      *tendency;
	PackageTendency_t      tempTendency;
	
	Tendency_t             tempGeneralTendency;
	Tendency_t             *generalTendency;
	
	tempTendency.packageIndex = 0;
	tempTendency.tendency = 0.0f;
	tempTendency.lastTendencyCheck = 0.0f;
	
	arc.ArchiveFloat( &_aggressiveness );
	arc.ArchiveFloat( &_talkiness );
	
	arc.ArchiveFloat( &_anger );
	arc.ArchiveFloat( &_fear );
	
	if ( arc.Saving() )
	{
		num = _PackageTendencies.NumObjects();
		
		arc.ArchiveInteger( &num );
		
		for ( i = 1 ; i <= num ; i++ )
		{
			tendency = &_PackageTendencies.ObjectAt( i );
			
			package = PackageList.ObjectAt( tendency->packageIndex );
			
			arc.ArchiveString( &package->packageName );
			arc.ArchiveFloat( &tendency->tendency );
			arc.ArchiveFloat( &tendency->lastTendencyCheck );
			
		}
	}
	else
	{
		arc.ArchiveInteger( &num );
		
		_PackageTendencies.ClearObjectList();
		_PackageTendencies.Resize( num );
		
		for ( i = 1 ; i <= num ; i++ )
		{
			_PackageTendencies.AddObject( tempTendency );
			
			tendency = &_PackageTendencies.ObjectAt( i );
			
			arc.ArchiveString( &packageName );
			
			
			tendency->packageIndex = -1;
			
			for ( j = 1 ; j <= PackageList.NumObjects() ; j++ )
			{
				package = PackageList.ObjectAt( j );
				
				if ( stricmp( packageName.c_str() , package->packageName.c_str() ) == 0 )
				{
					tendency->packageIndex = j;
					break;
				}
			}
			
			assert( tendency->packageIndex != -1 );
			
			arc.ArchiveFloat( &tendency->tendency );
			arc.ArchiveFloat( &tendency->lastTendencyCheck );
		}
	}
	
	
	if ( arc.Saving() )
	{
		num = _Tendencies.NumObjects();
		
		arc.ArchiveInteger( &num );
		
		for ( i = 1 ; i <= num ; i++ )
		{
			generalTendency = &_Tendencies.ObjectAt( i );		
			
			arc.ArchiveString( &generalTendency->tendencyName  );
			arc.ArchiveFloat( &generalTendency->tendencyValue  );	
			
		}
	}
	else
	{
		arc.ArchiveInteger( &num );
		
		_Tendencies.ClearObjectList();
		_Tendencies.Resize( num );
		
		for ( i = 1 ; i <= num ; i++ )
		{
			_Tendencies.AddObject( tempGeneralTendency );
			
			generalTendency = &_Tendencies.ObjectAt( i );
			
			arc.ArchiveString( &generalTendency->tendencyName  );
			arc.ArchiveFloat( &generalTendency->tendencyValue  );
		}
	}
}


void Personality::SetBehaviorTendency( const str &packageName , float tendency )
{
	PackageTendency_t      pEntry;
	BehaviorPackageType_t  *package;
	PackageTendency_t      *checkEntry;
	
	for ( int i = 1 ; i <= PackageList.NumObjects() ; i++ )
	{
		package = PackageList.ObjectAt( i );
		
		if ( !Q_stricmp( packageName.c_str() , package->packageName.c_str() ) )
		{
			for ( int j = 1 ; j <= _PackageTendencies.NumObjects() ; j++ )
			{
				checkEntry = &_PackageTendencies.ObjectAt( j );
				
				if ( checkEntry->packageIndex == i )
				{
					checkEntry->tendency = tendency;
					return;
				}
				
			}
			
			pEntry.lastTendencyCheck = 0.0f;
			pEntry.tendency = tendency;
			pEntry.packageIndex = i;
			
			_PackageTendencies.AddObject( pEntry );
			return;
		}
	}   
}

void Personality::SetTendency( const str &tendencyName , float tendencyValue )
{
	Tendency_t *currentTendency;
	Tendency_t tendency;
	
	for ( int i = 1 ; i <= _Tendencies.NumObjects() ; i++ )
	{
		currentTendency = &_Tendencies.ObjectAt( i );
		
		if ( !Q_stricmp( tendencyName.c_str() , currentTendency->tendencyName.c_str() ) )
		{
			currentTendency->tendencyValue = tendencyValue;         
			return;
		}
		
	}
	
	
	tendency.tendencyName = tendencyName;
	tendency.tendencyValue = tendencyValue;
	
	_Tendencies.AddObject( tendency );
}

qboolean Personality::WantsToExecuteCurrentPackage(float interval)
{
	int currentIndex;
	PackageTendency_t      *checkEntry;
	
	currentIndex = act->packageManager->GetCurrentFVarIndex();
	
	for ( int i = 1 ; i <= _PackageTendencies.NumObjects() ; i++ )
	{
		checkEntry = &_PackageTendencies.ObjectAt( i );
		
		if ( checkEntry->packageIndex == currentIndex )
		{
			if ( level.time > checkEntry->lastTendencyCheck + interval )
				return _wantsToExecutePackage(checkEntry);
		}
	}
	
	return false;
}

qboolean Personality::_wantsToExecutePackage(PackageTendency_t *tendency )
{
	float percent_chance;
	float chance;
	
	tendency->lastTendencyCheck = level.time;
	
	percent_chance = tendency->tendency;
	chance = G_Random();      
	
	return ( chance < percent_chance );   
	
}

qboolean Personality::ExecutedPackageInLastTimeFrame(float interval)
{
	float lastExecutionTime;
	
	lastExecutionTime = act->packageManager->GetCurrentFVarLastExecuteTime();
	
	if ( level.time <= lastExecutionTime + interval )
		return true;
	
	return false;   
}

void FillBehaviorPackageList( void )
{
	Script script;
	const char	*token;
	char  *buf;
	BehaviorPackageType_t *packageType;
	
	if ( gi.FS_ReadFile( "global/BehaviorPackages.txt", ( void ** )&buf, true ) == -1 )
		return;
	
	script.LoadFile( "global/BehaviorPackages.txt" );
	
	if ( script.length < 0 )
		return;
	
	
	while ( script.TokenAvailable ( true ) )
	{
		token = script.GetToken(false);
		
		if (!Q_stricmp( token , "Package" ) )
		{
			packageType = 0;
			packageType = new BehaviorPackageType_t;
			
			if ( !packageType )
				gi.Error( ERR_FATAL, "FillBehaviorPackageList -- could not create packageType" );
			
			packageType->packageName = script.GetToken(false);
			packageType->stateFile   = script.GetToken(false);
			
			PackageList.AddObject( packageType );
			
		}
		
	}
	
	script.Close();
}

void ClearBehaviorPackageList( void )
{
	for ( int i = PackageList.NumObjects() ; i > 0 ; i-- )
	{
		delete PackageList.ObjectAt( i );
		PackageList.RemoveObjectAt( i );
	}
	
	if ( PackageList.NumObjects() > 0 )
		gi.Error( ERR_FATAL, "ClearBehaviorPackageList -- PackageList is NOT empty." );
}

