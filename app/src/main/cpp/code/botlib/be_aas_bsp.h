

#ifdef AASINTERN
int AAS_LoadBSPFile(void);
void AAS_DumpBSPData(void);
void AAS_UnlinkFromBSPLeaves(bsp_link_t *leaves);
bsp_link_t *AAS_BSPLinkEntity(vec3_t absmins,
										vec3_t absmaxs,
										int entnum,
										int modelnum);

qboolean AAS_EntityCollision(int entnum,
										vec3_t start,
										vec3_t boxmins,
										vec3_t boxmaxs,
										vec3_t end,
										int contentmask,
										bsp_trace_t *trace);
void AAS_PrintFreeBSPLinks(char *str);
#endif 

#define MAX_EPAIRKEY		128

bsp_trace_t AAS_Trace(	vec3_t start,
								vec3_t mins,
								vec3_t maxs,
								vec3_t end,
								int passent,
								int contentmask);
int AAS_PointContents(vec3_t point);
qboolean AAS_inPVS(vec3_t p1, vec3_t p2);
qboolean AAS_inPHS(vec3_t p1, vec3_t p2);
qboolean AAS_AreasConnected(int area1, int area2);
int AAS_BoxEntities(vec3_t absmins, vec3_t absmaxs, int *list, int maxcount);
void AAS_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin);
int AAS_NextBSPEntity(int ent);
int AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size);
int AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v);
int AAS_FloatForBSPEpairKey(int ent, char *key, float *value);
int AAS_IntForBSPEpairKey(int ent, char *key, int *value);

