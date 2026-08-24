

#ifdef AASINTERN
void AAS_InvalidateEntities(void);
void AAS_UnlinkInvalidEntities(void);
void AAS_ResetEntityLinks(void);
int AAS_UpdateEntity(int ent, bot_entitystate_t *state);
void AAS_EntityBSPData(int entnum, bsp_entdata_t *entdata);
#endif 

void AAS_EntitySize(int entnum, vec3_t mins, vec3_t maxs);
int AAS_EntityModelNum(int entnum);
int AAS_OriginOfMoverWithModelNum(int modelnum, vec3_t origin);
int AAS_BestReachableEntityArea(int entnum);
void AAS_EntityInfo(int entnum, aas_entityinfo_t *info);
int AAS_NextEntity(int entnum);
void AAS_EntityOrigin(int entnum, vec3_t origin);
int AAS_EntityType(int entnum);
int AAS_EntityModelindex(int entnum);

