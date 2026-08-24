//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/Shared/qcommon/gameplaymanager.cpp                  $
// $Revision:: 14                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// GameplayManager.cpp: implementation of the GameplayManager class.
//
//////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "gameplaymanager.h"
#include <assert.h>

#ifdef GAME_DLL
#define PROBE_GET(probe_n, probe_d, probe_f) gi.cvar(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       gi.Printf
#else
#define PROBE_GET(probe_n, probe_d, probe_f) Cvar_Get(probe_n, probe_d, probe_f)
#define PROBE_PRINT                       Com_Printf
#endif
#include "../qcommon/qa_probe.h"

GameplayManager *GameplayManager::_theGameplayManager = 0; 

GameplayManager::GameplayManager()
{

}

GameplayManager::~GameplayManager()
{

}

void GameplayManager::shutdown()
{
	if ( _theGameplayManager )
	{
		delete _theGameplayManager;
		_theGameplayManager = 0;
	}
}

bool GameplayManager::isReady()
{
	if ( _theGameplayManager )
		return true;

	return false;
}

GameplayManager* GameplayManager::getTheGameplayManager()
{
	return _theGameplayManager;
}

void GameplayManager::create()
{
	if ( _theGameplayManager )
		shutdown();

	_theGameplayManager = new GameplayManager;
	if (!_theGameplayManager)
	{
		assert(0);
	}

	if ( !_theGameplayManager->_gameplayDatabase.parseFile("global/gameplay.gdb") )
	{
		assert(0);
		Com_Error (ERR_DROP, "GameplayDatabase: Parsing failed.  Contact programmer regarding this error.");
	}
#ifdef GAME_DLL
	if ( !_theGameplayManager->_gameplayFormulaManager.parseFile("global/gameplay.gpf") )
	{
		assert(0);
		gi.Error(ERR_DROP, "GameplayFormulas: Parsing failed.  Contact programmer regarding this error.");
	}
#endif 
}


bool GameplayManager::hasObject(const str& objname)
{
	if ( !objname.length() )
		return false;
	return _gameplayDatabase.hasObject(objname);
}


bool GameplayManager::isDefined(const str& propname)
{
	GameplayObject *gpobject = _gameplayDatabase.getObject(propname);
	if ( !gpobject )
		return false;
	
	if ( gpobject->hasProperty("value") )
		return true;

	return false;
}

const str GameplayManager::getDefine(const str& propname)
{
	GameplayObject *gpobject = _gameplayDatabase.getObject(propname);
	if ( !gpobject )
		return ""; 
	
	str value = gpobject->getPropertyStringValue("value");
	if ( value == "" )
	{
		float floatval;
		char tmpstr[16];
		floatval = gpobject->getPropertyFloatValue("value");
		sprintf(tmpstr, "%g", floatval);
		value = tmpstr;
	}

	return value;
}




void GameplayManager::setFloatValue(const str& objname, const str& propname, float value, bool create)
{
#ifdef GAME_DLL
	bool valueChanged = _gameplayDatabase.setFloatValue(objname, propname, value, create);
	if ( valueChanged )
	{
		char message[256] ;
		sprintf( message, "gdb_setfloatproperty %s %s %g\n", objname.c_str(), propname.c_str(), value );
		G_SendCommandToAllPlayers( message );
	}
#else
	_gameplayDatabase.setFloatValue(objname, propname, value, create);
#endif 
}


void GameplayManager::setStringValue(const str& objname, const str& propname, const str& valuestr, bool create)
{
#ifdef GAME_DLL
	bool valueChanged = _gameplayDatabase.setStringValue(objname, propname, valuestr, create);

	if ( valueChanged )
	{
		char message[256] ;
		sprintf( message, "gdb_setstringproperty %s %s %s\n", objname.c_str(), propname.c_str(), valuestr.c_str()	);
		G_SendCommandToAllPlayers( message );
	}
#else
	_gameplayDatabase.setStringValue(objname, propname, valuestr, create);
#endif 
}


float GameplayManager::getFloatValue(const str& objname, const str& propname)
{
	if ( !hasObject(objname) )
		return 1.0f;

	GameplayObject *gpobject = 0;
	gpobject = _gameplayDatabase.getObject(objname);

	return gpobject->getPropertyFloatValue(propname);
}


const str GameplayManager::getStringValue(const str& objname, const str& propname)
{
	if ( !hasObject(objname) )
		return "";

	GameplayObject *gpobject = 0;
	gpobject = _gameplayDatabase.getObject(objname);

	return gpobject->getPropertyStringValue(propname);
}





bool GameplayManager::hasProperty(const str& objname, const str& propname)
{
	if ( !hasObject(objname) )
		return false;

	GameplayObject *gpobject = _gameplayDatabase.getObject(objname);
	return gpobject->hasProperty(propname);
}


bool GameplayManager::hasSubObject(const str& objname, const str& subobject)
{
	if ( !objname.length() || !subobject.length())
		return false;

	GameplayObject *gpobject = 0;
	gpobject = _gameplayDatabase.getObject(objname);
	if ( !gpobject )
		return false;

	if ( !gpobject->getSubObject(subobject) )
		return false;

	return true;
}

#ifdef GAME_DLL

bool GameplayManager::hasFormula(const str& formulaName)
{
	return _gameplayFormulaManager.hasFormula(formulaName);
}


float GameplayManager::calculate(const str& formulaName, const GameplayFormulaData& formulaData, float multiplier)
{
	float result = _gameplayFormulaManager.getFormulaResult(formulaName, formulaData) * multiplier;
	return result;	
}


bool GameplayManager::setBase(const str& objname, const str& baseobj)
{
	return _gameplayDatabase.setBase(objname, baseobj);
}


bool GameplayManager::clearPropertyOverrides(const str& objname)
{
	return _gameplayDatabase.clearPropertyOverrides(objname);
}

void GameplayManager::processPendingMessages( void )
{
	char message[256] ;

	Container<PendingDelta>	&pendingDeltaList = _gameplayDatabase.getPendingDeltaList();
	for ( int pendingDeltaIdx = 1; pendingDeltaIdx <= pendingDeltaList.NumObjects(); pendingDeltaIdx++ )
	{
		PendingDelta	pendingDelta	= pendingDeltaList.ObjectAt( pendingDeltaIdx );
		Vector			vectorValue ;
		float			floatValue		= 0.0f ;
		str				objName			= pendingDelta.getObjectName();
		str				propName		= pendingDelta.getPropertyName();
		str				stringValue ;

		switch ( pendingDelta.getGameplayValueType() )
		{
			case VALUE_FLOAT:
				floatValue	= pendingDelta.getFloatValue();
				sprintf( message, "gdb_setfloatproperty %s %s %g\n", objName.c_str(), propName.c_str(), floatValue );
				break ;
			case VALUE_STRING:
				stringValue	= pendingDelta.getStringValue();
				sprintf( message, "gdb_setstringproperty %s %s %s\n", objName.c_str(), propName.c_str(), stringValue.c_str() );
				break ;
			case VALUE_VECTOR:
				vectorValue = pendingDelta.getVectorValue();
				sprintf( message, "gdb_setvectorproperty %s %s %g %g %g\n", objName.c_str(), propName.c_str(), vectorValue.x, vectorValue.y, vectorValue.z );
				break ;
			default:
				break ;
		}
		G_SendCommandToAllPlayers( message );
	}

	_gameplayDatabase.clearPendingDeltaList();
}


void GameplayManager::Archive(Archiver &arc)
{
	WIDGET_PROBE("archive", "gpm: manager archive enter (%s)\n", arc.Saving() ? "SAVE" : "LOAD");
	_gameplayDatabase.Archive(arc);
	WIDGET_PROBE("archive", "gpm: manager archive done\n");
}


#else




extern "C" qboolean HasGameplayProperty(const char* objectName, const char* propertyName)
{
	GameplayManager* gpm = GameplayManager::getTheGameplayManager();
	if(gpm == 0)
		return 0;

	if(gpm->hasProperty(objectName, propertyName))
		return qtrue;

	return qfalse;
}

extern "C" void GetGameplayStringProperty(const char* objectName, const char* propertyName, char* buffer, int length)
{
	if(buffer == 0 || length <= 0)
		return;

	GameplayManager* gpm = GameplayManager::getTheGameplayManager();
	if(gpm == 0)
		return;

	str stringValue = gpm->getStringValue(objectName, propertyName);

	strncpy(buffer, stringValue.c_str(), length);
}

extern "C" void SetGameplayStringProperty(const char* objectName, const char* propertyName, const char* stringValue, qboolean create)
{
	GameplayManager* gpm = GameplayManager::getTheGameplayManager();
	if(gpm == 0)
		return;

	bool createProperty = false;

	if(create)
		createProperty = true;

	gpm->setStringValue(objectName, propertyName, stringValue, createProperty);
}



extern "C" float GetGameplayFloatProperty(const char* objectName, const char* propertyName)
{
	GameplayManager* gpm = GameplayManager::getTheGameplayManager();
	if(gpm == 0)
		return 1.0f;

	return gpm->getFloatValue(objectName, propertyName);
}


void GetGameplayFloatPropertyCmd( void )
{
	GameplayManager* gpm = GameplayManager::getTheGameplayManager();

	if ( gpm )
	{
		const char *objectName		= Cmd_Argv( 1 );
		const char *propertyName	= Cmd_Argv( 2 );
		float		floatValue		= gpm->getFloatValue( objectName, propertyName );
		Com_Printf( "%s.%s: %g\n", objectName, propertyName, floatValue );
	}
}

void GetGameplayStringPropertyCmd( void )
{
	GameplayManager* gpm = GameplayManager::getTheGameplayManager();
	if( gpm )
	{
		const char *objectName		= Cmd_Argv( 1 );
		const char *propertyName	= Cmd_Argv( 2 );
		str			stringValue		= gpm->getStringValue( objectName, propertyName );
		Com_Printf( "%s.%s: %s\n", objectName, propertyName, stringValue.c_str() );
	}
}


extern "C" void SetGameplayFloatProperty(const char* objectName, const char* propertyName, float value, qboolean create)
{
	GameplayManager* gpm = GameplayManager::getTheGameplayManager();
	if(gpm == 0)
		return;

	bool createProperty = false;

	if(create)
		createProperty = true;

	gpm->setFloatValue(objectName, propertyName, value, createProperty);
}


void SetGameplayStringProperty( void )
{
	if ( Cmd_Argc() != 4 )
	{
		Com_Printf( "Syntax: gdb_setstringproperty <objname> <propname> <stringvalue>\n" );
		return ;
	}

	const char *objectName		= Cmd_Argv( 1 );
	const char *propertyName	= Cmd_Argv( 2 );
	const char *stringValue		= Cmd_Argv( 3 );

	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm ) return ;

	gpm->setStringValue( objectName, propertyName, stringValue, true );
}

void SetGameplayFloatProperty( void )
{
	if ( Cmd_Argc() != 4 )
	{
		Com_Printf( "Syntax: gdb_setfloatproperty <objname> <propname> <floatvalue>\n" );
		return ;
	}

	const char *objectName			= Cmd_Argv( 1 );
	const char *propertyName		= Cmd_Argv( 2 );
	const char *floatStringValue	= Cmd_Argv( 3 );

	GameplayManager *gpm = GameplayManager::getTheGameplayManager();
	if ( !gpm ) return ;

	float floatValue = atof( floatStringValue );
	gpm->setFloatValue( objectName, propertyName, floatValue, true );
}


extern "C" void CreateGameplayManager( void )
{
	GameplayManager::create();
	Cmd_AddCommand( "gdb_setfloatproperty",		SetGameplayFloatProperty );
	Cmd_AddCommand( "gdb_setstringproperty",	SetGameplayStringProperty );
	Cmd_AddCommand( "gdb_getfloatproperty",		GetGameplayFloatPropertyCmd );
	Cmd_AddCommand( "gdb_getstringproperty",	GetGameplayStringPropertyCmd );
}

extern "C" void ShutdownGameplayManager( void )
{
	Cmd_RemoveCommand( "gdb_setfloatproperty" );
	Cmd_RemoveCommand( "gdb_setstringproperty" );
	Cmd_RemoveCommand( "gdb_getfloatproperty" );
	Cmd_RemoveCommand( "gdb_getstringproperty" );
	GameplayManager::shutdown();
}

#endif 
