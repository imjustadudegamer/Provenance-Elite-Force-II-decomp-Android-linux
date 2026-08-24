


#define MAX_STRINGFIELD				80
#define FT_CHAR						1			
#define FT_INT							2			
#define FT_FLOAT						3			
#define FT_STRING						4			
#define FT_STRUCT						6			
#define FT_TYPE						0x00FF	
#define FT_ARRAY						0x0100	
#define FT_BOUNDED					0x0200	
#define FT_UNSIGNED					0x0400

typedef struct fielddef_s
{
	char *name;										
	int offset;										
	int type;										
	int maxarray;									
	float floatmin, floatmax;					
	struct structdef_s *substruct;			
} fielddef_t;

typedef struct structdef_s
{
	int size;
	fielddef_t *fields;
} structdef_t;

int ReadStructure(source_t *source, structdef_t *def, char *structure);
int WriteStructure(FILE *fp, structdef_t *def, char *structure);
int WriteIndent(FILE *fp, int indent);
int WriteFloat(FILE *fp, float value);


