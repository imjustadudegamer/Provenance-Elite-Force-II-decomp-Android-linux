//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

class GameplayDatabase;
class GameplayObject;
class GameplayProperty;

#ifndef __GAMEPLAYDATABASE_H__
#define __GAMEPLAYDATABASE_H__

#ifdef GAME_DLL

#include "../fgame/g_local.h"
#include "../fgame/script.h"

#endif 


#ifdef GAME_DLL
#define Parser Script
#else
#define Parser TikiScript
#endif 

#define HASH_SIZE 256

#ifdef GAME_DLL
#include "../fgame/class.h"
#include "../fgame/listener.h"
#include "../fgame/str.h"
#else
#include "../corepp/class.h"
#include "../corepp/listener.h"
#include "../corepp/str.h"
#endif
#include "../corepp/container.h"
#include "tiki_script.h"

typedef enum GameplayValueType
{
	VALUE_FLOAT,
	VALUE_STRING,
	VALUE_VECTOR,
	GAMEPLAY_VALUE_UNSPECIFIED
} GameplayValueType;

class PendingDelta
{
private:
	str					_objName ;
	str					_propName ;
	str					_stringValue ;
	float				_floatValue ;
	Vector				_vectorValue ;
	GameplayValueType	_type ;
	
public:
	PendingDelta() : _type( GAMEPLAY_VALUE_UNSPECIFIED ) { }
	PendingDelta( const str &objName, const str &propName, float floatValue )		
		: _objName( objName ), _propName( propName ), _floatValue( floatValue ), _type( VALUE_FLOAT ) { }
	PendingDelta( const str &objName, const str &propName, const str &stringValue ) 
		: _objName( objName ), _propName( propName ), _stringValue( stringValue ), _type( VALUE_STRING ) { }
	PendingDelta( const str &objName, const str &propName, const Vector &vectorValue ) 
		: _objName( objName ), _propName( propName ), _vectorValue( vectorValue ), _type( VALUE_VECTOR ) { }

	float				getFloatValue()			{ return _floatValue ; }
	const str&			getStringValue()		{ return _stringValue ; }
	const Vector&		getVectorValue()		{ return _vectorValue ; }
	const str&			getObjectName()			{ return _objName ; }
	const str&			getPropertyName()		{ return _propName ; }
	GameplayValueType	getGameplayValueType()	{ return _type ; }
};


class GameplayProperty : public Class
{
private:
	str					_name;
	str					_valuestr;
	float				_valuefloat;
	Vector				_valuevector;
	bool				_modified;
	GameplayValueType	_type;

public:

	CLASS_PROTOTYPE( GameplayProperty );

	GameplayProperty()
		:	_name(""),
			_valuestr(""),
			_valuefloat(1.0f),
			_valuevector(vec_zero),
			_modified(false),
			_type(VALUE_FLOAT)
		{}

	virtual ~GameplayProperty() {}

	bool parseProperty(Parser &gameplayFile, const str& type);
	bool isModified( void )			{ return _modified ; }

	const str& getName();		 
	const str& getStringValue()		{ return _valuestr; }
	float getFloatValue()			{ return _valuefloat; }
	const Vector& getVectorValue()	{ return _valuevector; }
	bool getModified()				{ return _modified; }
	GameplayValueType getType()		{ return _type; }
	const str getFloatValueStr();

	void setName(const str& name);
	void setModified(bool modified)				{ _modified = modified; }
	void setType(GameplayValueType type)		{ _type = type; }
	bool setStringValue(const str& valuestr);
	bool setFloatValue(float valuefloat);
	bool setVectorValue(const Vector& vector);

#ifdef GAME_DLL
public:
	void Archive(Archiver &arc) override;
#endif 
};


class GameplayObject : public Class
{
private:
	str				_name;
	str				_category;
	str				_fullScopeName;
	int				_depth;
	bool			_modified ;
	GameplayObject* _baseObject;
	GameplayObject* _nextHashObject;

	Container<GameplayProperty *> _propertyList;
	Container<GameplayObject *> _subObjectList;

public:

	CLASS_PROTOTYPE( GameplayObject );

	GameplayObject();
	GameplayObject(int depth);
	virtual ~GameplayObject();

	bool parseObject(Parser &gameplayFile, const str& name);

	bool hasProperty(const str& propname, bool localonly = false);
	bool isModified( void )									{ return _modified ; }

	const str& getName()									{ return _name; }
	const str& getCategory()								{ return _category; }
	const str& getFullScopeName()							{ return _fullScopeName; }
	const Container<GameplayObject *>& getSubObjectList()	{ return _subObjectList; };
	const Container<GameplayProperty *>& getPropertyList()	{ return _propertyList; };
	GameplayObject* getBaseObject()							{ return _baseObject; }
	GameplayObject* getNextObject()							{ return _nextHashObject; }
	GameplayObject* getSubObject(const str& subobjname);
	GameplayProperty* getProperty(const str& propname);
	float getPropertyFloatValue(const str& propname);
	const str getPropertyStringValue(const str& propname);
	bool getModified(const str& propname);
	int	getNumberOfModifiedProperties( void );

	void setModified( bool modified )					{ _modified = modified ; }
	void setName(const str& name)						{ _name = name; }
	void setCategory(const str& category)				{ _category = category; }
	void setFullScopeName(const str& fullscope)			{ _fullScopeName = fullscope; }
	void setBaseObject(GameplayObject* baseObject)		{ _baseObject = baseObject; }
	void setNextObject(GameplayObject *nextObject)		{ _nextHashObject = nextObject; }
	bool setFloatValue(const str& propname, float value, bool create = false);
	bool setStringValue(const str& propname, const str& valuestr, bool create = false);
	bool setVectorValue( const str& propname, const Vector& valuevector, bool create = false );

	bool removeProperty(const str& propname);

#ifdef GAME_DLL
public:
	void Archive(Archiver &arc) override;
#endif 
};


class GameplayDatabase  : public Listener
{
private:
	Container<PendingDelta>	_pendingDeltaList ;
	Container<GameplayObject *> _objectList;
	GameplayObject* _hashTable[HASH_SIZE];

	GameplayObject *_lastObj;
	str				_lastObjName;

	void			_linkSubObjectsToBase();
	void			_linkToBase(GameplayObject *object);
	GameplayObject* _createFromScope(const str& scope);
	bool			_addToHashTable(GameplayObject *object);
	void			_buildHashTable();

public:

	CLASS_PROTOTYPE( GameplayDatabase );

	GameplayDatabase();
	virtual ~GameplayDatabase();

	bool			parseFile(const str& filename);

	bool			hasObject(const str& objname);

	GameplayObject* getObject(const str& objname);
	GameplayObject* getRootObject(const str& objname);
	GameplayObject* getSubObject(const str& objname, const str& subobjname);
	int				getNumberOfModifiedObjects( void );
	float			getFloatValue(const str& objname, const str& propname);
	const str		getStringValue(const str& objname, const str& propname);
	Container<PendingDelta>&	getPendingDeltaList()							{ return _pendingDeltaList ; }

	bool			setFloatValue(const str& objname, const str& propname, float value, bool create = false);
	bool			setStringValue(const str& objname, const str& propname, const str& valuestr, bool create = false);
	bool			setVectorValue( const str& objname, const str& propname, const Vector& valuevalue, bool create = false );
	bool			setBase(const str& objname, const str& baseobj);
	bool			clearPropertyOverrides(const str& objname);
	void			clearPendingDeltaList()										{ _pendingDeltaList.FreeObjectList(); }

#ifdef GAME_DLL
public:
	void			Archive(Archiver &arc) override;
#endif 
};

#endif
