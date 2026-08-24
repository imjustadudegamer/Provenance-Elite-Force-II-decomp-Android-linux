

#ifdef AASINTERN

extern aas_t aasworld;

void QDECL AAS_Error(char *fmt, ...) Q_PRINTF_FUNC(1, 2);
void AAS_SetInitialized(void);
int AAS_Setup(void);
void AAS_Shutdown(void);
int AAS_LoadMap(const char *mapname);
int AAS_StartFrame(float time);
#endif 

int AAS_Initialized(void);
int AAS_Loaded(void);
float AAS_Time(void);
void AAS_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj );
