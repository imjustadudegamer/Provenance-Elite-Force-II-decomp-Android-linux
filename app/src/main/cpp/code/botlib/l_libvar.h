

typedef struct libvar_s
{
	char		*name;
	char		*string;
	int		flags;
	qboolean	modified;	
	float		value;
	struct	libvar_s *next;
} libvar_t;

void LibVarDeAllocAll(void);
libvar_t *LibVarGet(const char *var_name);
char *LibVarGetString(const char *var_name);
float LibVarGetValue(const char *var_name);
libvar_t *LibVar(const char *var_name, const char *value);
float LibVarValue(const char *var_name, const char *value);
char *LibVarString(const char *var_name, const char *value);
void LibVarSet(const char *var_name, const char *value);
qboolean LibVarChanged(const char *var_name);
void LibVarSetNotModified(const char *var_name);

