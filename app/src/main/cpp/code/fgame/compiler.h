
#ifndef __COMPILER_H__
#define __COMPILER_H__

#include "program.h"
#include "lexer.h"

#define	MAX_ERRORS		10

typedef struct
   {
	int				code;		               
	char				*file;	               
	int				file_line;
	int				parm_ofs[ MAX_PARMS ];  
   } function_t;

typedef struct
   {
	const char		*name;
	const char		*opname;
	float		priority;
	bool     right_associative;
	def_t		*type_a;
   def_t		*type_b;
   def_t		*type_c;
   } pr_opcode_t;

extern	pr_opcode_t	pr_opcodes[ 99 ];		      

enum 
   {
	OP_DONE,

   OP_UINC_F,
   OP_UDEC_F,

	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,
	OP_MUL_VF,
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_ADD_S,
	OP_ADD_FS,
	OP_ADD_SF,
	OP_ADD_VS,
	OP_ADD_SV,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE,
	OP_GE,
	OP_LT,
	OP_GT,

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FNC,

   OP_STORE_FTOS,

   OP_UMUL_F,
   OP_UDIV_F,
   OP_UADD_F,
   OP_USUB_F,
   OP_UAND_F,
   OP_UOR_F,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,

	OP_CALL,
	OP_OCALL,
   OP_THREAD,

   OP_PUSH_F,
   OP_PUSH_V,
   OP_PUSH_S,
	OP_PUSH_ENT,
	OP_PUSH_FNC,
   OP_PUSH_FTOS,

   OP_GOTO,
	OP_AND,
	OP_OR,
	
	OP_BITAND,
	OP_BITOR
   };

class Compiler
   {
   public :
      def_t		   *pr_scope;		      
      
      Program     &program;
      Lexer       lex;

      bool        callthread;

      int         filenumber;

                  Compiler( Program &prg );

      def_t       *Statement( const pr_opcode_t *op, def_t *var_a, const def_t *var_b );
      def_t	      *ParseImmediate( void );
      def_t       *ParseFunctionCall( def_t *func );
      def_t       *ParseObjectCall( def_t *func, def_t *object );
      def_t	      *ParseValue( void );
      def_t       *Term( void );
      def_t       *Expression( int priority );
      void        ParseStatement( void );
      function_t  ParseImmediateStatements( const type_t *type, const char parm_names[ MAX_PARMS ][ MAX_NAME ], int numparms );
      type_t      *ParseFunction( type_t *returntype, char parm_names[ MAX_PARMS ][ MAX_NAME ], int *numparms );
      void        ParseFunctionDef( type_t *type, const char *name );
      void        ParseVariableDef( type_t *type, const char *name );
      void        ParseDefs( void );
		bool        CompileFile( const char *text, int filenum );
   };

#endif
