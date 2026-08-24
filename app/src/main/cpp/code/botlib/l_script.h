

#define BINARYNUMBERS
#define NUMBERVALUE
#define DOLLAR

#define MAX_TOKEN					1024

#if defined(BSPC) && !defined(QDECL)
#define QDECL
#endif


#define SCFL_NOERRORS				0x0001
#define SCFL_NOWARNINGS				0x0002
#define SCFL_NOSTRINGWHITESPACES	0x0004
#define SCFL_NOSTRINGESCAPECHARS	0x0008
#define SCFL_PRIMITIVE				0x0010
#define SCFL_NOBINARYNUMBERS		0x0020
#define SCFL_NONUMBERVALUES		0x0040

#define TT_STRING						1			
#define TT_LITERAL					2			
#define TT_NUMBER						3			
#define TT_NAME						4			
#define TT_PUNCTUATION				5			

#define TT_DECIMAL					0x0008	
#define TT_HEX							0x0100	
#define TT_OCTAL						0x0200	
#ifdef BINARYNUMBERS
#define TT_BINARY						0x0400	
#endif 
#define TT_FLOAT						0x0800	
#define TT_INTEGER					0x1000	
#define TT_LONG						0x2000	
#define TT_UNSIGNED					0x4000	
#define P_RSHIFT_ASSIGN				1
#define P_LSHIFT_ASSIGN				2
#define P_PARMS						3
#define P_PRECOMPMERGE				4

#define P_LOGIC_AND					5
#define P_LOGIC_OR					6
#define P_LOGIC_GEQ					7
#define P_LOGIC_LEQ					8
#define P_LOGIC_EQ					9
#define P_LOGIC_UNEQ					10

#define P_MUL_ASSIGN					11
#define P_DIV_ASSIGN					12
#define P_MOD_ASSIGN					13
#define P_ADD_ASSIGN					14
#define P_SUB_ASSIGN					15
#define P_INC							16
#define P_DEC							17

#define P_BIN_AND_ASSIGN			18
#define P_BIN_OR_ASSIGN				19
#define P_BIN_XOR_ASSIGN			20
#define P_RSHIFT						21
#define P_LSHIFT						22

#define P_POINTERREF					23
#define P_CPP1							24
#define P_CPP2							25
#define P_MUL							26
#define P_DIV							27
#define P_MOD							28
#define P_ADD							29
#define P_SUB							30
#define P_ASSIGN						31

#define P_BIN_AND						32
#define P_BIN_OR						33
#define P_BIN_XOR						34
#define P_BIN_NOT						35

#define P_LOGIC_NOT					36
#define P_LOGIC_GREATER				37
#define P_LOGIC_LESS					38

#define P_REF							39
#define P_COMMA						40
#define P_SEMICOLON					41
#define P_COLON						42
#define P_QUESTIONMARK				43

#define P_PARENTHESESOPEN			44
#define P_PARENTHESESCLOSE			45
#define P_BRACEOPEN					46
#define P_BRACECLOSE					47
#define P_SQBRACKETOPEN				48
#define P_SQBRACKETCLOSE			49
#define P_BACKSLASH					50

#define P_PRECOMP						51
#define P_DOLLAR						52

typedef struct punctuation_s
{
	char *p;						
	int n;							
	struct punctuation_s *next;		
} punctuation_t;

typedef struct token_s
{
	char string[MAX_TOKEN];			
	int type;						
	int subtype;					
#ifdef NUMBERVALUE
	unsigned long int intvalue;	
	float floatvalue;			
#endif 
	char *whitespace_p;				
	char *endwhitespace_p;			
	int line;						
	int linescrossed;				
	struct token_s *next;			
} token_t;

typedef struct script_s
{
	char filename[1024];			
	char *buffer;					
	char *script_p;					
	char *end_p;					
	char *lastscript_p;				
	char *whitespace_p;				
	char *endwhitespace_p;			
	int length;						
	int line;						
	int lastline;					
	int tokenavailable;				
	int flags;						
	punctuation_t *punctuations;	
	punctuation_t **punctuationtable;
	token_t token;					
	struct script_s *next;			
} script_t;

int PS_ReadToken(script_t *script, token_t *token);
int PS_ExpectTokenString(script_t *script, char *string);
int PS_ExpectTokenType(script_t *script, int type, int subtype, token_t *token);
int PS_ExpectAnyToken(script_t *script, token_t *token);
int PS_CheckTokenString(script_t *script, char *string);
int PS_CheckTokenType(script_t *script, int type, int subtype, token_t *token);
int PS_SkipUntilString(script_t *script, char *string);
void PS_UnreadLastToken(script_t *script);
void PS_UnreadToken(script_t *script, token_t *token);
char PS_NextWhiteSpaceChar(script_t *script);
void StripDoubleQuotes(char *string);
void StripSingleQuotes(char *string);
signed long int ReadSignedInt(script_t *script);
float ReadSignedFloat(script_t *script);
void SetScriptPunctuations(script_t *script, punctuation_t *p);
void SetScriptFlags(script_t *script, int flags);
int GetScriptFlags(script_t *script);
void ResetScript(script_t *script);
int EndOfScript(script_t *script);
char *PunctuationFromNum(script_t *script, int num);
script_t *LoadScriptFile(const char *filename);
script_t *LoadScriptMemory(char *ptr, int length, char *name);
void FreeScript(script_t *script);
void PS_SetBaseFolder(char *path);
void QDECL ScriptError(script_t *script, char *str, ...) Q_PRINTF_FUNC(2, 3);
void QDECL ScriptWarning(script_t *script, char *str, ...) Q_PRINTF_FUNC(2, 3);


