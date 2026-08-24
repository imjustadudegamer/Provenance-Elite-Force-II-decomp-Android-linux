//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/gamescript.cpp                                $
// $Revision:: 5                                                              $
//   $Author:: Steven                                                         $
//     $Date:: 10/08/02 1:37p                                                 $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Subclass of script that preprocesses labels
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#include "glb_local.h"
#include "gamescript.h"
#include "scriptmaster.h"
#include "scriptvariable.h"
#include "scriptthread.h"
#include "scriptexception.h"
#include "level.h"





















ScriptLibrarian ScriptLib;

CLASS_DECLARATION(Class, GameScriptMarker, NULL)
{
    {NULL, NULL}
};

CLASS_DECLARATION(Class, ScriptLibrarian, NULL) {
    {NULL, NULL}
};

CLASS_DECLARATION(Script, GameScript, NULL)
{
    {NULL, NULL}
};



void GameScript::Close( void )
{
	FreeLabels();
	Script::Close();
	sourcescript = this;
	crc = 0;
}


void GameScript::FindLabels( void )
{
	scriptmarker_t mark;
	const char		*tok;
	script_label_t *label;
	int				len;
	
	FreeLabels();
	
	labelList = new Container<script_label_t *>;
	
	MarkPosition( &mark );
	
	Reset();
	
	while( TokenAvailable( true ) )
	{
		tok = GetToken( true );
		if ( tok )
		{
			len = strlen( tok );
			if ( ( len > 1 ) && ( tok[ len - 1 ] == ':' ) )
			{
				if ( !labelExists( tok ) )
				{
					label = new script_label_t;
					MarkPosition( &label->pos );
					label->labelname = tok;
					labelList->AddObject( label );
				}
				else
				{
					warning( "FindLabels", "Duplicate labels %s\n", tok );
				}
			}
		}
	}
	
	RestorePosition( &mark );
}


void GameScript::FreeLabels( void )
{
	int i;
	int num;
	
	if ( labelList )
	{
		num = labelList->NumObjects();
		for( i = 1; i <= num; i++ )
		{
			delete labelList->ObjectAt( i );
		}
		
		delete labelList;
	}
	
	labelList = NULL;
}


void GameScript::LoadFile( const char *name )
{
	str n;
	
	n = G_FixSlashes( name );
	
	sourcescript = this;
	Script::LoadFile( n.c_str() );
	FindLabels();
	
	crc = gi.CalcCRC( (const unsigned char *)buffer, length );
}


void GameScript::Mark( GameScriptMarker *mark )
{
	assert( mark );
	assert( sourcescript );
	
	mark->filename = sourcescript->Filename();
	MarkPosition( &mark->scriptmarker );
}


void GameScript::Restore( GameScriptMarker *mark )
{
	GameScript *scr;
	
	assert( mark );
	
	scr = ScriptLib.FindScript( mark->filename.c_str() );
	if ( scr )
	{
		SetSourceScript( scr );
	}
	else
	{
		LoadFile( mark->filename.c_str() );
	}
	
	RestorePosition( &mark->scriptmarker );
}


void GameScript::SetSourceScript( GameScript *scr )
{
	if ( scr != this )
	{
		Close();
		
		sourcescript = scr->sourcescript;
		crc = sourcescript->crc;
		Parse( scr->buffer, scr->length, scr->Filename() );
	}
}




qboolean GameScript::Goto( const char *name )
{
	str				labelname;
	script_label_t *label;
	int				i;
	int				num;
	
	if ( !sourcescript->labelList )
	{
		return false;
	}
	
	labelname = name;
	if ( !labelname.length() )
	{
		return false;
	}
	
	if ( labelname[ labelname.length() - 1 ] != ':' )
	{
		labelname += ":";
	}
	
	num = sourcescript->labelList->NumObjects();
	for( i = 1; i <= num; i++ )
	{
		label = sourcescript->labelList->ObjectAt( i );
		if ( labelname == label->labelname )
		{
			RestorePosition( &label->pos );
			return true;
		}
	}
	
	return false;
}


qboolean GameScript::labelExists( const char *name )
{
	str				labelname;
	script_label_t *label;
	int				i;
	int				num;
	
	if ( !name )
	{
		return true;
	}
	
	if ( !sourcescript->labelList )
	{
		return false;
	}
	
	labelname = name;
	if ( !labelname.length() )
	{
		return false;
	}
	
	if ( labelname[ labelname.length() - 1 ] != ':' )
	{
		labelname += ":";
	}
	
	num = sourcescript->labelList->NumObjects();
	for( i = 1; i <= num; i++ )
	{
		label = sourcescript->labelList->ObjectAt( i );
		if ( labelname == label->labelname )
		{
			return true;
		}
	}
	
	return false;
}




void ScriptLibrarian::CloseScripts( void )
{
	int i;
	int num;
	GameScript *scr;
	
	SetGameScript( "" );
	SetDialogScript( "" );
	
	num = scripts.NumObjects();
	for( i = num; i > 0; i-- )
	{
		scr = scripts.ObjectAt( i );
		scripts.RemoveObjectAt( i );
		delete scr;
	}
}


// :1507), and retail has no ScriptThread symbols at all.  The port never called them: this

void ScriptLibrarian::SetDialogScript( const str &scriptname )
{
	dialog_script = scriptname;
}


void ScriptLibrarian::SetGameScript( const str &scriptname )
{
	game_script = scriptname;
}








GameScript::GameScript()
{
	sourcescript = this;
	labelList = NULL;
	crc = 0;
}


GameScript::GameScript( GameScript *scr )
{
	crc = 0;
	labelList = NULL;
	SetSourceScript( scr );
}




GameScript::~GameScript()
{
	Close();
}






ScriptLibrarian::~ScriptLibrarian()
{
	int i;
	int num;
	
	num = scripts.NumObjects();
	for( i = 1; i <= num; i++ )
	{
		delete scripts.ObjectAt( i );
	}
}





GameScript *ScriptLibrarian::FindScript( const char *name )
{
	int i;
	int num;
	GameScript *scr;
	str n;
	
	n = G_FixSlashes( name );
	
	num = scripts.NumObjects();
	for( i = 1; i <= num; i++ )
	{
		scr = scripts.ObjectAt( i );
		
		if ( scr->Filename() == n )
		{
			return scr;
		}
	}
	
	return NULL;
}

GameScript *ScriptLibrarian::GetScript( const char *name )
{
	GameScript *scr;
	str n;
	
	n = G_FixSlashes( name );
	if ( !n.length() )
	{
		return NULL;
	}
	
	
	if ( !strchr( n.c_str(), '/' ) )
	{
		int i;
		str mapname;
		
		mapname = "maps/";
		mapname += level.mapname;
		for( i = mapname.length() - 1; i >= 0; i-- )
		{
			if ( mapname[ i ] == '/' )
			{
				i++;
				mapname[ i ] = 0;
				break;
			}
		}
		mapname += n;
		n = mapname;
	}
	
	scr = FindScript( n.c_str() );
	if ( !scr && ( gi.FS_ReadFile( n.c_str(), NULL, false ) != -1 ) )
	{
		scr = new GameScript();
		scr->LoadFile( n.c_str() );
		scripts.AddObject( scr );
	}

	return scr;
}

void GameScript::Archive
	(
	Archiver &arc
	)

   {
   GameScriptMarker mark;

   if ( arc.Saving() )
      {
      arc.ArchiveUnsigned( &crc );
      Mark( &mark );
      arc.ArchiveObject( &mark );
      }
   else
      {
      unsigned   filecrc;
      GameScript *scr;

      arc.ArchiveUnsigned( &filecrc );
      arc.ArchiveObject( &mark );
      scr = ScriptLib.FindScript( mark.filename.c_str() );
      if ( scr )
         {
         SetSourceScript( scr );
         }
      else
         {
         LoadFile( mark.filename.c_str() );
         }

      if ( filecrc != crc )
         {
         gi.Error( ERR_DROP, "File '%s' has changed from when this savegame was written.  Load cancelled.\n", filename.c_str() );
         }

	   RestorePosition( &mark.scriptmarker );
      }
   }


