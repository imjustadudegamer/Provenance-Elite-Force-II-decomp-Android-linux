
#ifndef __LEXER_H__
#define __LEXER_H__

#include "program.h"

#define	MAX_FRAMES	256

extern	int		type_size[ 6 ];

extern	type_t	type_void;
extern	type_t	type_string;
extern	type_t	type_float;
extern	type_t	type_vector;
extern	type_t	type_entity;
extern	type_t	type_function;

extern	def_t	   def_void;
extern	def_t	   def_string;
extern	def_t	   def_float;
extern	def_t	   def_vector;
extern	def_t	   def_entity;
extern	def_t	   def_function;

extern	def_t	   def_ret;
extern	def_t	   junkdef;

typedef enum
   {
   tt_eof,			
   tt_name, 		
   tt_punct, 		
   tt_immediate,	
   tt_preprocessor  
   } token_type_t;

class Lexer
   {
   private:
      int			   source_line;
      
      const char		*pr_file_p;
      const char		*pr_line_start;		
      
      int			   pr_bracelevel;

      char		      pr_token[ 2048 ];

   public:
   
      token_type_t	pr_token_type;

      type_t		   *pr_immediate_type;
      eval_t		   pr_immediate;
      char	         pr_immediate_string[ 2048 ];

      Program        &program;

      Lexer( Program &prg );  


      void           NewLine( void );
      void           LexString( void );
      float          LexNumber( void );
      void           LexVector( void );
      void           LexEntity( void );
      void           LexName( void );
      void           LexPunctuation( void );
      void           LexWhitespace( void );
      void           LexPreprocessor( void );
      void           Lex( void );
      void           ParseError( const char *error, ... ) Q_PRINTF_FUNC( 2, 3 );
      void           Expect( const char *string );
      bool           Check( const char *string );
      char           *ParseName( void );
      void           SkipOutOfFunction( void );
      void           SkipToSemicolon( void );
      type_t         *CheckType( void );
      type_t         *ParseType( void );

      int            SourceLine( void );
      void           SetSource( const char *text );
   };

inline int Lexer::SourceLine
   (
   void
   )

   {
   return source_line;
   }

inline void Lexer::SetSource
   (
   const char *text
   )

   {
   pr_file_p = text;
	source_line = 0;
   NewLine();
   }

#endif
