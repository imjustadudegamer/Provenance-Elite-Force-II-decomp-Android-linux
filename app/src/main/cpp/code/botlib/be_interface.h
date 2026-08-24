

#define RANDOMIZE		

typedef struct botlib_globals_s
{
	int botlibsetup;						
	int maxentities;						
	int maxclients;							
	float time;								
#ifdef DEBUG
	qboolean debug;							
	int goalareanum;
	vec3_t goalorigin;
	int runai;
#endif
} botlib_globals_t;


extern botlib_globals_t botlibglobals;
extern botlib_import_t botimport;
extern int botDeveloper;					

int Sys_MilliSeconds(void);

