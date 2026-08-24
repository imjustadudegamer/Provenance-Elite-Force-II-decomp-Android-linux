//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/Shared/qcommon/gameplayformulamanager.cpp           $
// $Revision:: 4                                                              $
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
// GameplayFormulaManager.cpp: implementation of the GameplayFormulaManager class.
//
//////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifdef GAME_DLL

#include "gameplayformulamanager.h"
#include "gameplaymanager.h"



GameplayFormulaData::GameplayFormulaData( Entity *pPrimary, Entity *pSecondary, Entity *pWeapon, const str& pAttackType )
{
	primary = pPrimary;
	secondary = pSecondary;
	weapon = pWeapon;
	attackType = pAttackType;
}







GameplayFormulaVariable::GameplayFormulaVariable()
{
	_categoryList.ClearObjectList();
}

GameplayFormulaVariable::~GameplayFormulaVariable()
{

}






float GameplayFormulaOperand::getResult(const GameplayFormulaData& formulaData)
{
	float finalResult = _constant; 
	str name;
	GameplayManager *gpm = GameplayManager::getTheGameplayManager();

	[[maybe_unused]] char firstobj[255], *sptr;
	strcpy(firstobj, _object.c_str());
	sptr = strchr(firstobj, '.');
	sptr = 0;

	if ( !strcmp(firstobj,"Defines") )
	{
		finalResult *= gpm->getFloatValue(_property, "value");
		return finalResult;
	}

	if ( !strcmp(firstobj, "Primary") && formulaData.primary )
		name = formulaData.primary->getArchetype();
	if ( !strcmp(firstobj, "Secondary") && formulaData.secondary )
		name = formulaData.secondary->getArchetype();
	if ( !strcmp(firstobj, "Weapon") && formulaData.weapon )
		name = formulaData.weapon->getArchetype();
	if ( !strcmp(firstobj, "AttackType") )
		name = formulaData.attackType;
	
	if ( !name.length() )
	{
		finalResult *= gpm->getFloatValue(_object, _property);
		return finalResult;
	}

	if ( _inverse )
		finalResult *= (1.0f / gpm->getFloatValue(name, _property));
	else
		finalResult *= gpm->getFloatValue(name, _property);
	
	return finalResult;
}

bool GameplayFormulaOperand::parseOperand(Script &formulaFile, const str& constant)
{
	const char *token;
	_constant = (float)atof(constant);

	if ( !formulaFile.TokenAvailable(false) )
		return false;
	token = formulaFile.GetToken(false);
	if ( token[0] == '/' )
		_inverse = true;

	if ( !formulaFile.TokenAvailable(false) )
		return false;
	token = formulaFile.GetToken(false);
	
	char *sptr, tmpstr[255];
	strcpy(tmpstr, token);
	sptr = strrchr(tmpstr,'.');
	if ( sptr )
	{
		*sptr = 0;
		sptr++;
	}

	_object = tmpstr;
	_property = sptr;

	return true;
}


GameplayFormula::GameplayFormula()
{
	_operandList.ClearObjectList();
}

GameplayFormula::~GameplayFormula()
{
	int i;
	for ( i=1; i<=_operandList.NumObjects(); i++ )
	{
		GameplayFormulaOperand *gpfo = _operandList.ObjectAt(i);
		delete gpfo;
	}

	_operandList.ClearObjectList();
}


float GameplayFormula::getResult(const GameplayFormulaData& formulaData)
{
	float finalResult = 1.0f;

	int i;
	for ( i=1; i<=_operandList.NumObjects(); i++ )
	{
		GameplayFormulaOperand *gpfo = _operandList.ObjectAt(i);
		finalResult *= gpfo->getResult(formulaData);
	}

	return finalResult;
}

bool GameplayFormula::parseFormula(Script &formulaFile, const str& name)
{
	const char *token;
	GameplayFormulaOperand *gpfo;
	
	if ( name != "FORMULA" )
		return false;

	if ( !formulaFile.TokenAvailable(false) )
		return false;
	
	token = formulaFile.GetToken(false);
	setName(token);

	token = formulaFile.GetToken(true);
	if ( token[0] != '{' )
		assert(0);

	while (formulaFile.TokenAvailable(true))
	{
		token = formulaFile.GetToken(true);

		if ( token[0] == '}' )
			return true;

		gpfo = new GameplayFormulaOperand();
		if ( gpfo->parseOperand(formulaFile, token) )
			_operandList.AddObject(gpfo);
		else
		{
			delete gpfo;
			return false;
		}
	}

	return false;
}



GameplayFormulaManager::GameplayFormulaManager()
{
	_variableList.ClearObjectList();
	_formulaList.ClearObjectList();
}


GameplayFormulaManager::~GameplayFormulaManager()
{
	int i;
	for ( i=1; i<=_variableList.NumObjects(); i++ )
	{
		GameplayFormulaVariable *gpfv = _variableList.ObjectAt(i);
		delete gpfv;
	}

	for ( i=1; i<=_formulaList.NumObjects(); i++ )
	{
		GameplayFormula *gpf = _formulaList.ObjectAt(i);
		delete gpf;
	}

	_variableList.FreeObjectList();
	_formulaList.FreeObjectList();
}


float GameplayFormulaManager::getFormulaResult(const str& formulaName, const GameplayFormulaData& formulaData)
{
	float finalResult = 1.0f;

	int i;
	for ( i=1; i<=_formulaList.NumObjects(); i++ )
	{
		GameplayFormula *gpf = _formulaList.ObjectAt(i);
		if ( gpf->getName() == formulaName )
		{
			finalResult = gpf->getResult(formulaData);
			return finalResult;
		}
	}

	return finalResult;
}


bool GameplayFormulaManager::parseFile(const str& filename)
{
	Script			formulaFile;
	const char		*token;
	GameplayFormula	*gpformula;

	if ( !gi.FS_Exists(filename.c_str()) )
		return false;

	formulaFile.LoadFile(filename.c_str());

	while (formulaFile.TokenAvailable(true))
	{
		token = formulaFile.GetToken(false);
		
		if ( strcmp(token, "FORMULA") )
			return false;

		gpformula = new GameplayFormula();
		if ( gpformula->parseFormula(formulaFile, token) )
			_formulaList.AddObject(gpformula);
		else
		{
			delete gpformula;
			return false;
		}
	}

	return true;
}


bool GameplayFormulaManager::hasFormula(const str& formulaName)
{
	int i;
	for ( i=1; i<=_formulaList.NumObjects(); i++ )
	{
		GameplayFormula *gpf = _formulaList.ObjectAt(i);
		if ( gpf->getName() == formulaName )
			return true;
	}

	return false;
}

#endif 


