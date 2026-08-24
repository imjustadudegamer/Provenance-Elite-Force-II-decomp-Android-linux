//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/scriptvariable.cpp                            $
// $Revision:: 9                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/03/02 9:51a                                                 $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Dynamic variable list for scripts.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "_pch_cpp.h"
#include "class.h"
#include "scriptvariable.h"
#include "scriptmaster.h"

// delete, do not re-enable.  TIKIANIM-C blocked on "the shipped .scr corpus isn't in
//    nothing and would add registrations retail's own binary does not carry.

CLASS_DECLARATION( Listener, ScriptVariable, NULL )
{
	{ NULL, NULL }
};

ScriptVariable::ScriptVariable()
{
	value = 0;
}

qboolean ScriptVariable::isVariableCommand( const char *name_shadow1 )
{
	int i;
	
	for( i = 0; ScriptVariable::Responses[ i ].event != NULL; i++ )
	{
		if ( ScriptVariable::Responses[ i ].event->getName() == name_shadow1 )
		{
			return true;
		}
	}
	
	return false;
}

void ScriptVariable::setName( const char *newname )
{
	name = newname;
}

const char *ScriptVariable::getName( void )
{
	return name.c_str();
}

const char *ScriptVariable::stringValue( void )
{
	return string.c_str();
}

void ScriptVariable::setString( const char *value_shadow1 )
{
	string = value_shadow1;
}

void ScriptVariable::setStringValue( const char *newvalue )
{
	setString( newvalue );
	value = (float)atof( string.c_str() );
}

int ScriptVariable::intValue( void )
{
	return ( int )value;
}

void ScriptVariable::setIntValue( int newvalue )
{
	char text[ 128 ];
	
	value = ( float )newvalue;
	sprintf( text, "%d", newvalue );
	setString( text );
}

float ScriptVariable::floatValue( void )
{
	return value;
}

void ScriptVariable::setFloatValue( float newvalue )
{
	char text[ 128 ];
	
	value = newvalue;
	sprintf( text, "%f", newvalue );
	setString( text );
}

void ScriptVariable::setVectorValue( const Vector &newvector )
{
	char text[ 128 ];
	
	vec = newvector;
	sprintf( text, "(%f %f %f)", newvector.x, newvector.y, newvector.z );
	setString( text );
}

Vector ScriptVariable::vectorValue( void )
{
	return vec;
}

CLASS_DECLARATION( Class, ScriptVariableList, NULL )
{
	{ NULL, NULL }
};


ScriptVariableList::ScriptVariableList()
{
}

ScriptVariableList::~ScriptVariableList()
{
	ClearList();
}

void ScriptVariableList::ClearList( void )
{
	int i;
	int num;
	ScriptVariable *var;
	
	num = NumVariables();
	for( i = num; i > 0; i-- )
	{
		var = GetVariable( i );
		RemoveVariable( var );
		delete var;
	}
	
	list.FreeObjectList();
}

void ScriptVariableList::AddVariable( ScriptVariable *var )
{
	if ( VariableExists( var->getName() ) )
	{
		warning( "AddVariable", "Variable %s already exists.\n", var->getName() );
		return;
	}
	
	list.AddObject( var );
}

ScriptVariable *ScriptVariableList::CreateVariable( const char *name, float value )
{
	ScriptVariable *var;
	
	if ( VariableExists( name ) )
	{
		warning( "AddVariable", "Variable %s already exists.\n", name );
		return GetVariable( name );
	}
	
	var = new ScriptVariable;
	var->setName( name );
	var->setFloatValue( value );
	list.AddObject( var );
	
	return var;
}

ScriptVariable *ScriptVariableList::CreateVariable( const char *name, int value )
{
	ScriptVariable *var;
	
	if ( VariableExists( name ) )
	{
		warning( "AddVariable", "Variable %s already exists.\n", name );
		return GetVariable( name );
	}
	
	var = new ScriptVariable;
	var->setName( name );
	var->setIntValue( value );
	list.AddObject( var );
	
	return var;
}

ScriptVariable *ScriptVariableList::CreateVariable( const char *name, const char *text )
{
	ScriptVariable *var;
	
	if ( VariableExists( name ) )
	{
		warning( "AddVariable", "Variable %s already exists.\n", name );
		return GetVariable( name );
	}
	
	var = new ScriptVariable;
	var->setName( name );
	var->setStringValue( text );
	list.AddObject( var );
	
	return var;
}

ScriptVariable *ScriptVariableList::CreateVariable( const char *name, const Entity *ent )
{
	ScriptVariable *var;
	
	assert( ent );
	
	if ( VariableExists( name ) )
	{
		warning( "AddVariable", "Variable %s already exists.\n", name );
		return GetVariable( name );
	}
	
	var = new ScriptVariable;
	var->setName( name );
	list.AddObject( var );
	
	if ( !ent )
	{
		warning( "SetVariable", "Null entity\n" );
		
		var->setStringValue( "*0" );
	}
	else
	{
		var->setStringValue( va( "*%d", ent->entnum ) );
	}
	
	return var;
}

ScriptVariable *ScriptVariableList::CreateVariable( const char *name, const Vector &vec )
{
	ScriptVariable *var;
	
	if ( VariableExists( name ) )
	{
		warning( "AddVariable", "Variable %s already exists.\n", name );
		return GetVariable( name );
	}
	
	var = new ScriptVariable;
	var->setName( name );
	var->setStringValue( va( "(%f %f %f)", vec.x, vec.y, vec.z) );
	list.AddObject( var );
	
	return var;
}

void ScriptVariableList::RemoveVariable( ScriptVariable *var )
{
	if ( !list.ObjectInList( var ) )
	{
		warning( "RemoveVariable", "Variable %s does not exist.\n", var->getName() );
		return;
	}
	
	list.RemoveObject( var );
}

void ScriptVariableList::RemoveVariable( const char *name )
{
	ScriptVariable *var;
	
	var = GetVariable( name );
	if ( !var )
	{
		warning( "RemoveVariable", "Variable %s does not exist.\n", name );
		return;
	}
	
	RemoveVariable( var );
}

qboolean ScriptVariableList::VariableExists( const char *name )
{
	int i;
	int num;
	ScriptVariable *var;
	
	num = list.NumObjects();
	for( i = 1; i <= num; i++ )
	{
		var = list.ObjectAt( i );
		if ( !strcmp( var->getName(), name ) )
		{
			return true;
		}
	}
	
	return false;
}

ScriptVariable *ScriptVariableList::GetVariable( const char *name )
{
	int i;
	int num;
	ScriptVariable *var;
	
	num = list.NumObjects();
	for( i = 1; i <= num; i++ )
	{
		var = list.ObjectAt( i );
		if ( !strcmp( var->getName(), name ) )
		{
			return var;
		}
	}
	
	return NULL;
}

int ScriptVariableList::NumVariables( void )
{
	return list.NumObjects();
}

ScriptVariable *ScriptVariableList::GetVariable( int num )
{
	return list.ObjectAt( num );
}

ScriptVariable *ScriptVariableList::SetVariable( const char *name, float value )
{
	ScriptVariable *var;
	
	var = GetVariable( name );
	if ( !var )
	{
		var = new ScriptVariable;
		var->setName( name );
		list.AddObject( var );
	}
	
	var->setFloatValue( value );
	
	return var;
}

ScriptVariable *ScriptVariableList::SetVariable( const char *name, int value )
{
	ScriptVariable *var;
	
	var = GetVariable( name );
	if ( !var )
	{
		var = new ScriptVariable;
		var->setName( name );
		list.AddObject( var );
	}
	
	var->setIntValue( value );
	
	return var;
}

ScriptVariable *ScriptVariableList::SetVariable( const char *name, const char *text )
{
	ScriptVariable *var;
	
	var = GetVariable( name );
	if ( !var )
	{
		var = new ScriptVariable;
		var->setName( name );
		list.AddObject( var );
	}
	
	var->setStringValue( text );
	
	return var;
}

ScriptVariable *ScriptVariableList::SetVariable( const char *name, const Entity *ent )
{
	ScriptVariable *var;
	
	var = GetVariable( name );
	if ( !var )
	{
		var = new ScriptVariable;
		var->setName( name );
		list.AddObject( var );
	}
	
	if ( !ent )
	{
		var->setStringValue( "*0" );
	}
	else
	{
		var->setStringValue( va( "*%d", ent->entnum ) );
	}
	
	return var;
}

ScriptVariable *ScriptVariableList::SetVariable( const char *name, const Vector &vec )
{
	ScriptVariable *var;
	
	var = GetVariable( name );
	if ( !var )
	{
		var = new ScriptVariable;
		var->setName( name );
		list.AddObject( var );
	}
	
	var->setStringValue( va( "(%f %f %f)", vec.x, vec.y, vec.z) );
	var->setVectorValue ( vec );
	return var;
}


void ScriptVariable::Var_Append( Event *ev )
{
	str newstring;
	
	newstring = string + ev->GetString( 1 );
	setStringValue( newstring.c_str() );
}

void ScriptVariable::Var_AppendInt( Event *ev )
{
	str newstring;
	
	newstring = string + va( "%d", ev->GetInteger( 1 ) );
	setStringValue( newstring.c_str() );
}

void ScriptVariable::Var_AppendFloat( Event *ev )
{
	str newstring;
	
	newstring = string + va( "%f", ev->GetFloat( 1 ) );
	setStringValue( newstring.c_str() );
}

void ScriptVariable::Var_String( Event *ev )
{
	setStringValue( ev->GetString( 1 ) );
}

void ScriptVariable::Var_RandomFloat( Event *ev )
{
	setFloatValue( G_Random( ev->GetFloat( 1 ) ) );
}

void ScriptVariable::Var_RandomInteger( Event *ev )
{
	setIntValue( (int)G_Random( ev->GetInteger( 1 ) ) );
}

void ScriptVariable::Var_Equals( Event *ev )
{
	setFloatValue( ev->GetFloat( 1 ) );
}

void ScriptVariable::Var_PlusEquals( Event *ev )
{
	setFloatValue( floatValue() + ev->GetFloat( 1 ) );
}

void ScriptVariable::Var_MinusEquals( Event *ev )
{
	setFloatValue( floatValue() - ev->GetFloat( 1 ) );
}

void ScriptVariable::Var_TimesEquals( Event *ev )
{
	setFloatValue( floatValue() * ev->GetFloat( 1 ) );
}

void ScriptVariable::Var_DivideEquals( Event *ev )
{
	setFloatValue( floatValue() / ev->GetFloat( 1 ) );
}

void ScriptVariable::Var_Div( Event *ev )
{
	int temp_int;
	
	temp_int = ev->GetInteger( 1 );
	
	if ( temp_int == 0 )
	{
		gi.WDPrintf( "Script divide by zero, variable %s\n", name.c_str() );
		return;
	}
	
	setIntValue( (int)( floatValue() / temp_int ) );
}

void ScriptVariable::Var_Mod( Event *ev )
{
	int temp_int;
	
	temp_int = ev->GetInteger( 1 );
	
	if ( temp_int == 0 )
	{
		gi.WDPrintf( "Script mod by zero, variable %s\n", name.c_str() );
		return;
	}
	
	setIntValue( intValue() % temp_int );
}


void ScriptVariable::Var_GetCvar( Event *ev )
{
	cvar_t *var;
	
	var = gi.cvar( ev->GetString( 1 ), "", 0 );
	setStringValue( var->string );
}

void ScriptVariable::Var_Vector( Event *ev )
{
	setVectorValue( ev->GetVector( 1 ) );
}

void ScriptVariable::Var_Vector_Add( Event *ev )
{
	Vector tempvec;
	
	tempvec = vectorValue();
	tempvec += ev->GetVector( 1 );
	setVectorValue( tempvec );
}

void ScriptVariable::Var_Vector_Subtract( Event *ev )
{
	Vector tempvec;
	
	tempvec = vectorValue();
	tempvec -= ev->GetVector( 1 );
	setVectorValue( tempvec );
}

void ScriptVariable::Var_Vector_Scale( Event *ev )
{
	Vector tempvec;
	
	tempvec = vectorValue();
	tempvec = tempvec * ev->GetFloat( 1 );
	setVectorValue( tempvec );
}

void ScriptVariable::Var_Vector_Normalize( Event *ev )
{
	Vector vec_shadow1( vectorValue() );
	vec_shadow1.normalize();
	setVectorValue( vec_shadow1 );
}

void ScriptVariable::Var_Vector_GetX( Event *ev )
{
	setFloatValue( ev->GetVector( 1 ).x );
}

void ScriptVariable::Var_Vector_GetY( Event *ev )
{
	setFloatValue( ev->GetVector( 1 ).y );
}

void ScriptVariable::Var_Vector_GetZ( Event *ev )
{
	setFloatValue( ev->GetVector( 1 ).z );
}

void ScriptVariable::Var_Vector_SetX( Event *ev )
{
	Vector tempvec;
	
	tempvec = vectorValue();
	tempvec.x = ev->GetFloat( 1 );
	setVectorValue( tempvec );
}

void ScriptVariable::Var_Vector_SetY( Event *ev )
{
	Vector tempvec;
	
	tempvec = vectorValue();
	tempvec.y = ev->GetFloat( 1 );
	setVectorValue( tempvec );
}

void ScriptVariable::Var_Vector_SetZ( Event *ev )
{
	Vector tempvec;
	
	tempvec = vectorValue();
	tempvec.z = ev->GetFloat( 1 );
	setVectorValue( tempvec );
}


void ScriptVariable::Var_Vector_DotProduct( Event *ev )
{
	Vector vec1, vec2;
	
	vec1 = ev->GetVector( 1 );
	vec2 = ev->GetVector( 2 );
	setFloatValue( vec1 * vec2 );
}

void ScriptVariable::Var_Vector_Length( Event *ev )
{
	setFloatValue( ev->GetVector( 1 ).length() );
}

void ScriptVariable::Var_Vector_CrossProduct( Event *ev )
{
	Vector vec1, vec2, cross;
	
	vec1 = ev->GetVector( 1 );
	vec2 = ev->GetVector( 2 );
	cross.CrossProduct( vec1, vec2 );
	setVectorValue( cross );
}

void ScriptVariable::Var_Angles_ToForward( Event *ev )
{
	Vector forward;
	
	ev->GetVector( 1 ).AngleVectors( &forward, NULL, NULL );
	setVectorValue( forward );
}

void ScriptVariable::Var_Angles_ToLeft( Event *ev )
{
	Vector left;
	
	ev->GetVector( 1 ).AngleVectors( NULL, &left, NULL );
	setVectorValue( left );
}

void ScriptVariable::Var_Angles_ToUp( Event *ev )
{
	Vector up;
	
	ev->GetVector( 1 ).AngleVectors( NULL, NULL, &up );
	setVectorValue( up );
}

void ScriptVariable::Var_TargetOf( Event *ev )
{
	Entity * ent;
	const char *target;
	
	ent = ev->GetEntity( 1 );
	if ( ent )
	{
		target = ent->Target();
	}
	else
	{
		return;
	}
	
	if ( target )
	{
		setStringValue( va( "$%s", target ) );
	}
}
