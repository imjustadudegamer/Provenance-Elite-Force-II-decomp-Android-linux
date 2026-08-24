//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/Shared/qcommon/gameplaydatabase.cpp                 $
// $Revision:: 17                                                             $
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
// GameplayDatabase.cpp: implementation of the GameplayDatabase class.
//
//////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "gameplaydatabase.h"



CLASS_DECLARATION( Class, GameplayProperty, NULL )
{
	{NULL,NULL}
};

const str& GameplayProperty::getName( void )
{
	return _name;
}

void GameplayProperty::setName( const str &name )
{
	_name = name;
}


bool GameplayProperty::setFloatValue(float valuefloat)
{
	if ( _valuefloat == valuefloat ) return false ;

	_valuefloat = valuefloat ;
	return true ;
}

bool GameplayProperty::setVectorValue(const Vector& vector)
{
	if ( _valuevector == vector ) return false ;

	_valuevector = vector ;
	return true ;
}

bool GameplayProperty::setStringValue(const str& valuestr)
{
	if ( _valuestr == valuestr ) return false ;

	_valuestr = valuestr ;
	return true ;
}


bool GameplayProperty::parseProperty(Parser &gameplayFile, const str& type)
{
	const char *token;
	float val1, val2, val3;
	int readcount;
	
	if ( type == "vector" || type == "string" || type == "float" )
	{
		token = gameplayFile.GetToken(false);
		if ( !token )
			return false;
		setName(token);
	}
	else 
		setName(type);

	token = gameplayFile.GetToken(false);
	if ( !token )
		return false;

	readcount = sscanf(token,"%f %f %f",&val1, &val2, &val3);

	if ( readcount == 1 ) 
	{
		setFloatValue(val1);
		setType(VALUE_FLOAT);
	}
	else if ( readcount == 3 ) 
	{	
		setVectorValue(Vector(val1, val2, val3));
		setType(VALUE_VECTOR);
	}
	else 
	{
		setStringValue(token);
		setType(VALUE_STRING);
	}

	return true;
}


const str GameplayProperty::getFloatValueStr()
{
	char tmpstr[16];
	sprintf(tmpstr, "%g", _valuefloat);
	return tmpstr;
}


#ifdef GAME_DLL
void GameplayProperty::Archive(Archiver &arc)
{
	if ( arc.Loading() )
	{
		return ;
	}

	if ( arc.Saving() && isModified() )
	{
		ArchiveEnum( _type, GameplayValueType );
		arc.ArchiveString( &_name );
		switch ( _type )
		{
		case VALUE_FLOAT:
			arc.ArchiveFloat( &_valuefloat );
			break ;
		case VALUE_STRING:
			arc.ArchiveString( &_valuestr );
			break ;
		case VALUE_VECTOR:
			arc.ArchiveVector( &_valuevector );
			break ;
		default:
			break;
		}
	}
}

#endif 








CLASS_DECLARATION( Class, GameplayObject, NULL )
{
	{NULL,NULL}
};
GameplayObject::GameplayObject() : 
	_name(""),
	_category(""),
	_fullScopeName(""),
	_depth(0),
	_modified( false ),
	_baseObject(0),
	_nextHashObject(0)
{
	_propertyList.ClearObjectList();
	_subObjectList.ClearObjectList();
}

GameplayObject::GameplayObject(int depth) : 
	_name(""),
	_category(""),
	_fullScopeName(""),
	_depth(depth),
	_modified( false ),
	_baseObject(0),
	_nextHashObject(0)
{
	_propertyList.ClearObjectList();
	_subObjectList.ClearObjectList();
}

GameplayObject::~GameplayObject()
{
	int i;
	for ( i=1; i<=_propertyList.NumObjects(); i++ )
	{
		GameplayProperty *gp = _propertyList.ObjectAt(i);
		delete gp;
	}

	for ( i=1; i<=_subObjectList.NumObjects(); i++ )
	{
		GameplayObject *go = _subObjectList.ObjectAt(i);
		delete go;
	}

	_baseObject = 0; 

	_subObjectList.FreeObjectList();
	_propertyList.FreeObjectList();
}

bool GameplayObject::parseObject(Parser &gameplayFile, const str& name)
{
	const char *token;
	GameplayProperty *gpproperty;
	
	if ( name != "OBJECT" )
		return false;

	if ( !gameplayFile.TokenAvailable(false) )
		return false;
	
	token = gameplayFile.GetToken(false);
	setName(token);
	_fullScopeName += _name;

	if ( gameplayFile.TokenAvailable(false) )
	{
		token = gameplayFile.GetToken(false);
		setCategory(token);
	}

	token = gameplayFile.GetToken(true);
	if ( token[0] != '{' )
		assert(0);

	while (gameplayFile.TokenAvailable(true))
	{
		token = gameplayFile.GetToken(true);

		if ( token[0] == '}' )
			return true;

		if ( !strcmp(token, "OBJECT") )
		{
			GameplayObject *gpobject;
			gpobject = new GameplayObject(_depth+1);
			gpobject->setFullScopeName(_fullScopeName + ".");
			if ( gpobject->parseObject(gameplayFile, token) )
			{
				_subObjectList.AddObject(gpobject);
			}
			else
			{
				delete gpobject;
				return false;
			}
		}
		else
		{
			gpproperty = new GameplayProperty();
			if ( gpproperty->parseProperty(gameplayFile, token) )
				_propertyList.AddObject(gpproperty);
			else
			{
				delete gpproperty;
				return false;
			}
		}
	}

	return false;
}

GameplayProperty* GameplayObject::getProperty(const str& propname)
{
	int i;
	GameplayProperty *gpprop;

	if ( propname == "" )
		return 0;

	for ( i=1; i<=_propertyList.NumObjects(); i++ )
	{
		gpprop = _propertyList.ObjectAt(i);
		if ( gpprop->getName() == propname )
			return gpprop;
	}

	return 0;
}


float GameplayObject::getPropertyFloatValue(const str& propname)
{
	GameplayProperty *gpprop;
	
	gpprop = getProperty(propname);
	if ( !gpprop )
	{
		if ( _baseObject )
		{
			gpprop = _baseObject->getProperty(propname);
			if ( !gpprop )
				return 1.0f;
		}	
		else
			return 1.0f;
	}

	return gpprop->getFloatValue();
}


const str GameplayObject::getPropertyStringValue(const str& propname)
{
	GameplayProperty *gpprop;
	
	gpprop = getProperty(propname);
	if ( !gpprop )
	{
		if ( _baseObject )
		{
			gpprop = _baseObject->getProperty(propname);
			if ( !gpprop )
				return "";
		}	
		else
			return "";
	}

	return gpprop->getStringValue();
}

bool GameplayObject::setFloatValue(const str& propname, float value, bool create)
{
	GameplayProperty *gpprop = getProperty(propname);
	if ( !gpprop )
	{
		if ( !create ) return false ;


		gpprop = new GameplayProperty();
		gpprop->setName(propname);
		_propertyList.AddObject(gpprop);
	}

	bool valueChanged = gpprop->setFloatValue(value);
	if ( valueChanged )
	{
		gpprop->setModified( true );
		setModified( true );
	}
	
	return valueChanged ;
}


bool GameplayObject::setStringValue(const str& propname, const str& valuestr, bool create)
{
	GameplayProperty *gpprop = getProperty(propname);
	if ( !gpprop )
	{
		if ( !create ) return false;

		gpprop = new GameplayProperty();
		gpprop->setName(propname);
		_propertyList.AddObject(gpprop);
	}

	bool valueChanged = gpprop->setStringValue(valuestr);
	if ( valueChanged )
	{
		gpprop->setModified( true );
		setModified( true );
	}

	return valueChanged ;
}

bool GameplayObject::setVectorValue(const str& propname, const Vector& value, bool create)
{
	GameplayProperty *gpprop = getProperty(propname);
	if ( !gpprop )
	{
		if ( !create ) return false ;


		gpprop = new GameplayProperty();
		gpprop->setName(propname);
		_propertyList.AddObject(gpprop);
	}

	bool valueChanged = gpprop->setVectorValue(value);
	if ( valueChanged )
	{
		gpprop->setModified( true );
		setModified( true );
	}
	
	return valueChanged ;
}

bool GameplayObject::getModified(const str& propname)
{
	GameplayProperty* gpprop = 0;
	gpprop = getProperty(propname);
	if ( !gpprop )
	{
		if ( _baseObject )
		{
			gpprop = _baseObject->getProperty(propname);
			if ( !gpprop )
				return false;
		}	
		else
			return false;
	}

	return gpprop->getModified();
}


GameplayObject* GameplayObject::getSubObject(const str& subobjname)
{
	int i;
	for ( i=1; i<=_subObjectList.NumObjects(); i++ )
	{
		GameplayObject *go = _subObjectList.ObjectAt(i);
		if ( go->getName() == subobjname )
			return go;
	}

	return 0;
}


bool GameplayObject::hasProperty(const str& propname, bool localonly)
{
	int i;
	GameplayProperty *gpprop;

	if ( propname == "" )
		return false;

	for ( i=1; i<=_propertyList.NumObjects(); i++ )
	{
		gpprop = _propertyList.ObjectAt(i);
		if ( gpprop->getName() == propname )
			return true;
	}

	if ( _baseObject && !localonly && _baseObject->hasProperty(propname) )
		return true;

	return false;
}


bool GameplayObject::removeProperty(const str& propname)
{
	int i;
	GameplayProperty *gpprop;

	if ( propname == "" )
		return false;

	for ( i=1; i<=_propertyList.NumObjects(); i++ )
	{
		gpprop = _propertyList.ObjectAt(i);
		if ( gpprop->getName() == propname )
		{
			_propertyList.RemoveObjectAt(i);
			delete gpprop;
			return true;
		}
	}	

	return false;
}


int GameplayObject::getNumberOfModifiedProperties
(
	void
)
{
	int numModifiedProperties	= 0 ;
	int numProperties			= _propertyList.NumObjects();

	for ( int propertyIdx = 1; propertyIdx <= numProperties; ++propertyIdx )
	{
		GameplayProperty *property = _propertyList.ObjectAt( propertyIdx );
		if ( property->isModified() )
		{
			numModifiedProperties++ ;
		}
	}

	return numModifiedProperties ;
}



#ifdef GAME_DLL
void GameplayObject::Archive(Archiver &arc)
{
	if ( arc.Loading() )
	{
		return ;
	}

	if ( arc.Saving() && isModified() )
	{
		int numModifiedProperties	= getNumberOfModifiedProperties();
		int numProperties			= _propertyList.NumObjects();

		arc.ArchiveString( &_fullScopeName );
		arc.ArchiveInteger( &numModifiedProperties );
		for ( int modifiedIdx = 1; modifiedIdx <= numProperties; ++modifiedIdx )
		{
			GameplayProperty *property = _propertyList.ObjectAt( modifiedIdx );
			if ( property->isModified() )
			{
				property->Archive( arc );
			}
		}
	}
}

#endif 


CLASS_DECLARATION( Listener, GameplayDatabase, NULL )
{
	{NULL,NULL}
};
GameplayDatabase::GameplayDatabase() :	
	_lastObj(0),
	_lastObjName("")
{
	for ( int i=0; i<HASH_SIZE; i++ )
		_hashTable[i] = 0;

	_objectList.ClearObjectList();
}

GameplayDatabase::~GameplayDatabase()
{
	int i;

	for ( i=0; i<HASH_SIZE; i++ )
		_hashTable[i] = 0;

	for ( i=1; i<=_objectList.NumObjects(); i++ )
	{
		GameplayObject *go = _objectList.ObjectAt(i);
		delete go;
	}
	_objectList.FreeObjectList();
}


bool GameplayDatabase::parseFile(const str& filename)
{
	Parser			gameplayFile;
	const char		*token;
	GameplayObject	*gpobject;

#ifdef GAME_DLL
	if ( !gi.FS_Exists(filename.c_str()) )
		return false;
#else
	if ( !FS_Exists(filename.c_str()) )
		return false;
#endif 

	gameplayFile.LoadFile(filename.c_str());

	while (gameplayFile.TokenAvailable(true))
	{
		token = gameplayFile.GetToken(false);
		
		if ( strcmp(token, "OBJECT") )
			return false;

		gpobject = new GameplayObject();
		if ( gpobject->parseObject(gameplayFile, token) )
		{
			_objectList.AddObject(gpobject);
		}
		else
		{
			delete gpobject;
			return false;
		}
	}

	_buildHashTable();
	_linkSubObjectsToBase();

	return true;
}


GameplayObject* GameplayDatabase::getRootObject(const str& objname)
{
	GameplayObject *gpobject;
	unsigned int loc = GenerateHashForName(objname.c_str(), false, HASH_SIZE);
	gpobject = _hashTable[loc];
	while ( gpobject && gpobject->getName() != objname )
		gpobject = gpobject->getNextObject();

	return gpobject;
}


GameplayObject* GameplayDatabase::getObject(const str& objname)
{
	if ( objname == _lastObjName )
		return _lastObj;

	GameplayObject *gpobject;
	unsigned int loc = GenerateHashForName(objname.c_str(), false, HASH_SIZE);
	gpobject = _hashTable[loc];
	while ( gpobject && gpobject->getFullScopeName() != objname )
		gpobject = gpobject->getNextObject();

	_lastObj = gpobject;
	_lastObjName = objname;

	return gpobject;
}


GameplayObject* GameplayDatabase::getSubObject(const str& objname, const str& subobjname)
{
	int i;
	GameplayObject *gpobject;

	for ( i=1; i<=_objectList.NumObjects(); i++ )
	{
		gpobject = _objectList.ObjectAt(i);
		if ( gpobject->getName() == objname )
			return gpobject->getSubObject(subobjname);
	}

	return 0;
}


float GameplayDatabase::getFloatValue(const str& objname, const str& propname)
{
	GameplayObject *gpobject = 0;
	gpobject = getObject(objname);
	if ( !gpobject )
		return 1.0f;

	return gpobject->getPropertyFloatValue(propname);
}

const str GameplayDatabase::getStringValue(const str& objname, const str& propname)
{
	GameplayObject *gpobject = 0;
	gpobject = getObject(objname);
	if ( !gpobject )
		return "";

	return gpobject->getPropertyStringValue(propname);
}

bool GameplayDatabase::setFloatValue(const str& objname, const str& propname, float value, bool create)
{
	GameplayObject *gpobject = getObject(objname);
	if ( !gpobject )
	{
		if ( !create ) return false;

		gpobject = _createFromScope(objname);
		gpobject->setCategory("Misc"); 
		return true ;
	}
	
	return gpobject->setFloatValue(propname, value, create);
}

bool GameplayDatabase::setStringValue(const str& objname, const str& propname, const str& valuestr, bool create)
{
	GameplayObject *gpobject = getObject(objname);
	if ( !gpobject )
	{
		if ( !create ) return false;

		gpobject = _createFromScope(objname);
		gpobject->setCategory("Misc"); 
		return true ;
	}

	return gpobject->setStringValue(propname, valuestr, create);
}

bool GameplayDatabase::setVectorValue(const str& objname, const str& propname, const Vector& value, bool create)
{
	GameplayObject *gpobject = getObject(objname);
	if ( !gpobject )
	{
		if ( !create ) return false;

		gpobject = _createFromScope(objname);
		gpobject->setCategory("Misc"); 
		return true ;
	}
	
	return gpobject->setVectorValue(propname, value, create);
}


void GameplayDatabase::_linkSubObjectsToBase()
{
	int i;
	for ( i=1; i<=_objectList.NumObjects(); i++ )
	{
		GameplayObject *go = _objectList.ObjectAt(i);
		_linkToBase(go);
	}
}

void GameplayDatabase::_linkToBase(GameplayObject *object)
{
	int j;

	const Container<GameplayObject *>& subObjectList = object->getSubObjectList();
	for ( j=1; j<=subObjectList.NumObjects(); j++ )
	{
		GameplayObject *subobj = subObjectList.ObjectAt(j);
		GameplayObject *baseObj = getObject(subobj->getName());
		if ( !baseObj )
			assert(0); 
		else
			subobj->setBaseObject(baseObj);

		_linkToBase(subobj);
	}
}

bool GameplayDatabase::hasObject(const str& objname)
{
	if ( getObject(objname) )
		return true;

	return false;
}


GameplayObject* GameplayDatabase::_createFromScope(const str& scope)
{
	str scopename;
	char tmpstr[255], *sptr;
	strcpy(tmpstr, scope.c_str());
	sptr = strtok(tmpstr,".\n");
	scopename += sptr;
	GameplayObject *gpobject = 0;
	gpobject = getRootObject(sptr);
	if ( !gpobject )
	{
		gpobject = new GameplayObject;
		gpobject->setName(sptr);
		gpobject->setFullScopeName(scopename);
		_addToHashTable(gpobject);
		_objectList.AddObject(gpobject);
	}
	
	sptr = strtok(NULL,".\n");
	while ( sptr )
	{
		scopename += ".";
		scopename += sptr;
		gpobject = gpobject->getSubObject(sptr);
		if ( !gpobject )
		{
			gpobject = new GameplayObject;
			gpobject->setName(sptr);
			gpobject->setFullScopeName(scopename);
			_addToHashTable(gpobject);
			_objectList.AddObject(gpobject);
		}

		sptr = strtok(NULL,".\n");
	}

		return gpobject;
}


bool GameplayDatabase::_addToHashTable(GameplayObject *object)
{
	if ( !object )
		( void )false;

	bool collisionFlag = false;

	const Container<GameplayObject *>& subObjectList = object->getSubObjectList();
	for ( int j=1; j<=subObjectList.NumObjects(); j++ )
	{
		GameplayObject *subobj = subObjectList.ObjectAt(j);
		if ( _addToHashTable(subobj) )
			collisionFlag = true;
	}

	unsigned int loc = GenerateHashForName(object->getFullScopeName().c_str(), false, HASH_SIZE);
	GameplayObject *obj = 0;
	obj = _hashTable[loc];
	if ( !obj )
	{
		_hashTable[loc] = object;
		return collisionFlag;
	}

	GameplayObject *prev = 0;
	collisionFlag = true;
	while ( obj )
	{
		prev = obj;
		obj = obj->getNextObject();
	}
	
	prev->setNextObject(object);

	return collisionFlag;
}


void GameplayDatabase::_buildHashTable()
{
	int i;
	for ( i=1; i<=_objectList.NumObjects(); i++ )
	{
		GameplayObject *go = _objectList.ObjectAt(i);
		_addToHashTable(go);
	}
}


bool GameplayDatabase::setBase(const str& objname, const str& baseobj)
{
	GameplayObject *obj = 0;
	obj = getObject(objname);
	if ( !obj )
		return false;

	GameplayObject *base = 0;
	base = getRootObject(baseobj);
	obj->setBaseObject(base);
	
	return true;
}


bool GameplayDatabase::clearPropertyOverrides(const str& objname)
{
	int i = 0;

	GameplayObject *obj = 0;
	obj = getObject(objname);
	if ( !obj )
		return false;
	
	GameplayObject *baseobj = 0;
	baseobj = obj->getBaseObject();
	if ( !baseobj )
		return true; 

	const Container<GameplayProperty *>& proplist = baseobj->getPropertyList();
	for ( i=1; i<=proplist.NumObjects(); i++ )
	{
		GameplayProperty *gp = proplist.ObjectAt(i);
		
		if ( obj->hasProperty(gp->getName(), true) )
			obj->removeProperty(gp->getName());
	}

	return true;
}


int GameplayDatabase::getNumberOfModifiedObjects( void )
{
	int numberOfModifiedObjects	= 0 ;
	int numObjects				= _objectList.NumObjects();

	for ( int objectIdx = 1; objectIdx <= numObjects; ++objectIdx )
	{
		GameplayObject *object = _objectList.ObjectAt( objectIdx );
		if ( object->isModified() )
		{
			numberOfModifiedObjects++ ;
		}
	}

	return numberOfModifiedObjects ;
}


#ifdef GAME_DLL
void GameplayDatabase::Archive(Archiver &arc)
{
	GameplayObject	*object					= 0 ;
	int				 numModifiedObjects		= getNumberOfModifiedObjects();
	int				 numObjects				= _objectList.NumObjects();

	arc.ArchiveInteger( &numModifiedObjects );

	if ( arc.Saving() )
	{
		for ( int objectIdx = 1; objectIdx <= numObjects; ++objectIdx )
		{
			object					= _objectList.ObjectAt( objectIdx );
			object->Archive( arc );
		}
	}
	else
	{
		for ( int objectIdx = 1; objectIdx <= numModifiedObjects; ++objectIdx )
		{
			str	scopedObjectName ;
			int	numModifiedProperties	= 0 ;

			arc.ArchiveString( &scopedObjectName );
			arc.ArchiveInteger( &numModifiedProperties );

			for ( int propertyIdx = 1; propertyIdx <= numModifiedProperties; ++propertyIdx )
			{
				str					propertyName ;
				str					stringValue ;
				float				floatValue		= 0.0f ;
				bool				modified		= false ;

				arc.ArchiveString( &propertyName );
				arc.ArchiveString( &stringValue );
				arc.ArchiveFloat( &floatValue );
				arc.ArchiveBool( &modified );

				if ( modified )
				{
					setStringValue( scopedObjectName, propertyName, stringValue, true );
					_pendingDeltaList.AddObject( PendingDelta( scopedObjectName, propertyName, stringValue ) );
				}
			}
		}
	}
}

#endif 
