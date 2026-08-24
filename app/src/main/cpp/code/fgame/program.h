//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/program.h                                      $
// $Revision:: 11                                                             $
//     $Date:: 10/13/03 8:54a                                                 $
//
// Copyright (C) 1999 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
// 
//
// DESCRIPTION:
// 
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __PROGRAM_H__
#define __PROGRAM_H__

#include "g_local.h"
#include "../corepp/class.h"
#include "../corepp/listener.h"
#include "../corepp/container.h"
#include "../corepp/str.h"
#include "archive.h"


class Lexer;
class Entity;

#define	MAX_STRINGS		4096
#define	MAX_STATEMENTS	16384
#define	MAX_FUNCTIONS	2560

#define	MAX_PARMS	   16
#define	MAX_NAME		   64		
#define	MAX_REGS		   16384

#define	OFS_NULL		   0
#define	OFS_RETURN		1
#define	RESERVED_OFS	4
#define	OFS_CAM        4
#define  OFS_END        6

extern const str complextypestring;
extern const str emptystring;
extern const str resultstring;
extern const str immediatestring;

typedef int	gofs_t;				

typedef enum 
   {
   ev_error = -1, ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_function
   } etype_t;

typedef int	func_t;

typedef union eval_s
   {
	int				string;
	float				_float;
	float				vector[ 3 ];
	func_t			function;
	int				_int;
   int            entity;
   } eval_t;

class def_t;

class type_t : public Class
{
public:
	type_t( etype_t t = ev_error, def_t *d = NULL, type_t *n = NULL, type_t *aux = NULL ) :
		type( t ), def( d ), next( n ), aux_type( aux ) { num_parms = 0; min_parms = 0; };
	CLASS_PROTOTYPE( type_t );

	virtual void	Archive( Archiver &arc ) override;

	etype_t			type;
	def_t          *def;		                  
	type_t			*next;

	type_t			*aux_type;	               
	int				num_parms;	               
	int				min_parms;	               
	type_t			*parm_types[ MAX_PARMS ];	
};

class def_t : public Class
   {
   public:
	   type_t		   *type;
	   str            name;
      def_t          *next;
	   gofs_t		   ofs;
      gofs_t		   localofs;      
      def_t          *scope;		   
	   int			   initialized;	

		bool				caseSensitive;

		bool				_onDefList;

                     def_t();

							CLASS_PROTOTYPE( def_t );

	virtual void	Archive( Archiver &arc ) override;    

   };

typedef struct
   {
	unsigned short	type;
	unsigned short	ofs;
   signed   short localofs;
	unsigned short s_name;
   } ddef_t;

typedef struct statement_s
   {
	unsigned short	op;
	short	         a;
   short	         b;
   short	         c;
   unsigned short linenumber;
   unsigned short file;
   } dstatement_t;

class dfunction_t : public Class
   {
public:
	dfunction_t()	{ };
	CLASS_PROTOTYPE( dfunction_t );


   int      eventnum;
	int		first_statement;	
	int		parm_start;
   int		parm_total;
	int		locals;				
	
	int		profile;		      
	
	str      s_name;
	str      s_file;			   

	int		numparms;
   int      minparms;
	byte	   parm_size[ MAX_PARMS ];
   byte	   parm_type[ MAX_PARMS ];

	virtual void	Archive( Archiver &arc ) override;    
   };

class localstr_t
{
public:
	localstr_t() : inuse( false ), s() { };
	bool		inuse;
	str		s;
};

class TargetList;
class Program : public Class
   {
   public:
		CLASS_PROTOTYPE( Program );

		virtual void	Archive( Archiver &arc ) override;    


      def_t		      *pr_global_defs[ MAX_REGS ];	
      float		      pr_globals[ MAX_REGS ];
      int		      numpr_globals;

      int            locals_start;
      int			   locals_end;		

      localstr_t		strings[ MAX_STRINGS ];

      dstatement_t	statements[ MAX_STATEMENTS ];
      int			   numstatements;

      dfunction_t	   functions[ MAX_FUNCTIONS ];
      int			   numfunctions;

	   type_t		   *types;	
	   def_t		      def_head;		               
	   def_t		      *def_tail;		               

      int		      pr_error_count;

      Container<str> filenames;
      str            s_file;

      type_t         *FindType( const type_t *type );
      def_t          *GetDef( type_t *type, const char *name, def_t *scope, bool allocate, Lexer *lex );
      void           CreateDefForEvent( Event *ev );
      void           CreateEventDefs( void );

      int				CopyString( const char *str );

                     Program();
                     ~Program();

      void           InitData( void );
      void           BeginCompilation( void );
      bool           FinishCompilation( void );
      void           FreeData( void );

      const char     *GetFilename( int num );

      void           Compile( const char *filename );
      void           Load( const char *filename );

      float          getFloat( int offset );
      int            getInteger( int offset );
      float          *getVector( int offset );
      const char     *getString( int offset );
      Entity         *getEntity( int offset );
      gentity_t      *getEdict( int offset );
      TargetList     *getTargetList( int offset );
      func_t         getFunction( int offset );

      void           setFloat( int offset, float value );
      void           setInteger( int offset, int value );
      void           setVector( int offset, const Vector &vec );
      void           setString( int offset, const char *text );
      void           setFunction( int offset, func_t func );
      void           setEntity( int offset, const Entity *ent );
      void           setTargetList( int offset, const TargetList *list );

      func_t         findFunction( const char *name );

		int				AllocString();
		void				FreeString( int idx );
		int				CountUsedStrings();

   



};
extern Program program;

#endif
