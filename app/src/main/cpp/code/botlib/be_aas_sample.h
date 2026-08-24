

#ifdef AASINTERN
void AAS_InitAASLinkHeap(void);
void AAS_InitAASLinkedEntities(void);
void AAS_FreeAASLinkHeap(void);
void AAS_FreeAASLinkedEntities(void);
aas_face_t *AAS_AreaGroundFace(int areanum, vec3_t point);
aas_face_t *AAS_TraceEndFace(aas_trace_t *trace);
aas_plane_t *AAS_PlaneFromNum(int planenum);
aas_link_t *AAS_AASLinkEntity(vec3_t absmins, vec3_t absmaxs, int entnum);
aas_link_t *AAS_LinkEntityClientBBox(vec3_t absmins, vec3_t absmaxs, int entnum, int presencetype);
qboolean AAS_PointInsideFace(int facenum, vec3_t point, float epsilon);
qboolean AAS_InsideFace(aas_face_t *face, vec3_t pnormal, vec3_t point, float epsilon);
void AAS_UnlinkFromAreas(aas_link_t *areas);
#endif 

void AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs);
int AAS_AreaCluster(int areanum);
int AAS_AreaPresenceType(int areanum);
int AAS_PointPresenceType(vec3_t point);
aas_trace_t AAS_TraceClientBBox(vec3_t start, vec3_t end, int presencetype, int passent);
int AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas);
int AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
int AAS_AreaInfo( int areanum, aas_areainfo_t *info );
int AAS_PointAreaNum(vec3_t point);
int AAS_PointReachabilityAreaIndex( vec3_t point );
void AAS_FacePlane(int facenum, vec3_t normal, float *dist);

