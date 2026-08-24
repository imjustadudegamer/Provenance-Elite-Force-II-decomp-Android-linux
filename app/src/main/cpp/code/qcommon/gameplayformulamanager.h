
#ifdef GAME_DLL

class GameplayFormulaManager;
class GameplayFormula;
class GameplayFormulaVariable;
class GameplayFormulaData;
class GameplayFormulaOperand;

#ifndef __GAMEPLAY_FORMULA_MANAGER_H__
#define __GAMEPLAY_FORMULA_MANAGER_H__

#include "../fgame/g_local.h"
#include "../fgame/actor.h"
#include "../fgame/weapon.h"
#include "../fgame/entity.h"
#include "../fgame/player.h"


class GameplayFormulaData : public Class
{
public:
	GameplayFormulaData(	Entity *pPrimary = 0, 
							Entity *pSecondary = 0, 
							Entity *pWeapon = 0,
							const str& pAttackType = "");

	Entity *primary;
	Entity *secondary;
	Entity *weapon;
	str attackType;;
};





// RETAIL STATUS -- DO NOT REMOVE AS RESIDUE.  This is the SDK's own name
//   * nothing in retail ever CREATES one or READS the list -- not parseFile
class GameplayFormulaVariable : public Class
{
private:
	str				_name;
	Container<str>	_categoryList;

public:
	GameplayFormulaVariable();
	virtual ~GameplayFormulaVariable();

	const str& getName()			{ return _name; }
	void setName(const str& name)	{ _name = name; }
};





class GameplayFormulaOperand : public Class
{
private:
	float	_constant;
	str		_object;
	str		_property;
	bool	_inverse;

public:
	GameplayFormulaOperand() 
		:	_constant(1.0f),
			_inverse(false)
	{}
	virtual ~GameplayFormulaOperand() {}

	const str& getObjectName()					{ return _object; }
	void setObjectName(const str& object)		{ _object = object; }

	const str& getPropertyName()				{ return _property; }
	void setPropertyName(const str& property)	{ _property = property; }

	float getConstant()					{ return _constant; }
	void setConstant(float constant)	{ _constant = constant; }

	bool getInverseFlag()				{ return _inverse; }
	void setInverseFlag(bool inverse)	{ _inverse = inverse; }

	float getResult(const GameplayFormulaData& formulaData);

	bool parseOperand(Script &formulaFile, const str& constant);
};





class GameplayFormula : public Class
{
private:
	str	_name;
	Container<GameplayFormulaOperand *>	_operandList;

public:
	GameplayFormula();
	virtual ~GameplayFormula();

	const str& getName()			{ return _name; }
	void setName(const str& name)	{ _name = name; }

	float getResult(const GameplayFormulaData& formulaData );

	bool parseFormula(Script &formulaFile, const str& name);
};





class GameplayFormulaManager : public Class
{
private:
	Container<GameplayFormulaVariable *>	_variableList;
	Container<GameplayFormula *>			_formulaList;
public:
	GameplayFormulaManager();
	virtual ~GameplayFormulaManager();

	float getFormulaResult(const str& formulaName, const GameplayFormulaData& formulaData);
	bool hasFormula(const str& formulaName);

	bool parseFile(const str& filename);
};

#endif 

#endif 
