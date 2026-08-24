//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/Shared/qcommon/tiki_script.h                         $
// $Revision:: 10                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 1997 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
// DESCRIPTION:
// C++ implementaion of tokenizing text interpretation.  Class accepts filename
// to load or pointer to preloaded text data.  Standard tokenizing operations
// such as skip white-space, get string, get integer, get float, get token,
// and skip line are implemented.  
//
// Note: all '//', '#', and ';' are treated as comments.  Probably should
// make this behaviour toggleable.
// 
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __TIKI_SCRIPT_H__
#define __TIKI_SCRIPT_H__

#if !defined( QERADIANT ) && !defined( UTILS )
#define	MAXTOKEN	256
#endif
#define  MAXMACROS 384 

typedef struct
	{
	char	name[ MAXTOKEN ];
	char	macro[ MAXTOKEN ];
	} tiki_macro_t;

typedef struct {
	const char *mark_script_p;
	qboolean    mark_tokenready;
	char        mark_token[ MAXTOKEN ];
} tiki_mark_t;

#ifdef __cplusplus

class TikiScript
	{
	public:
		~TikiScript();
		TikiScript();

		char*	buffer;
		int 	length;
		char	path[ MAX_QPATH ];
		char	texturepath[ MAX_QPATH ];
		static TikiScript*	currentScript;

		void			Close();
		const char*		Filename();
		int 			GetLineNumber();
		void			Reset();	
		qboolean		TokenAvailable( qboolean crossline );
		void			UnGetToken();
		const char*		GetToken( qboolean crossline );
		const char*		GetLine( qboolean crossline );
		const char*		GetAndIgnoreLine( qboolean crossline );
		const char*		GetRaw();
		const char*		GetString( qboolean crossline );
		qboolean		GetSpecific( const char *string );
		int 			GetInteger( qboolean crossline );
		double			GetDouble( qboolean crossline );
		float			GetFloat( qboolean crossline );
		void			GetVector( qboolean crossline, vec3_t vec );
		int 			LinesInFile();
		void			Parse( char *data, int length, const char *name );
		qboolean		LoadFile( const char *name, qboolean quiet = qfalse );
		qboolean		LoadFileFromTS( const char *name, const char * tikidata, qboolean quiet = qfalse );
		const char*		Token();
		float			EvaluateMacroMath(float value, float newval, char oper);
		char*			EvaluateMacroString(const char *theMacroString);
		const char*		GetMacroString(const char *theMacroName);
		void			MarkPos();
		void			ReturnToMark();
		void			ReplaceLineWithWhitespace( bool deleteFromStartOfLine );
		const char*		GetParentToken();

	protected:
		qboolean		error;
		qboolean		tokenready;
		TikiScript*		include;
		TikiScript*		parent;

		char			filename[ MAXTOKEN ];
		const char*		script_p;
		const char*		end_p;
		tiki_macro_t	macros[ MAXMACROS ];
		int 			nummacros;

		int 			line;		
		char			token[ MAXTOKEN ];

		qboolean		releaseBuffer;

		tiki_mark_t		mark[ 64 ];
		int				mark_pos;

		qboolean		AtComment();
		qboolean		AtExtendedComment();
		qboolean		AtCommand();
		qboolean		AtString( qboolean crossline );
		qboolean		ProcessCommand( qboolean crossline );
		qboolean		Completed();
		qboolean		SafeCheckOverflow();
		void			CheckOverflow();
		void			Uninclude();
		const char*		FindMacro( const char * macro );
		void			AddMacro( const char *macro, const char *expansion );

		qboolean		SkipToEOL();
		void			SkipWhiteSpace( qboolean crossline );
		void			SkipNonToken( qboolean crossline );
		qboolean		CommentAvailable( qboolean crossline );
		void			SkipExtendedComment();
	};

#endif 

#endif
