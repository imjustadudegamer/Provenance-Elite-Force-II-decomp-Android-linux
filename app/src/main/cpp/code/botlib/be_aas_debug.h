

void AAS_ClearShownDebugLines(void);
void AAS_ClearShownPolygons(void);
void AAS_DebugLine(vec3_t start, vec3_t end, int color);
void AAS_PermanentLine(vec3_t start, vec3_t end, int color);
void AAS_DrawPermanentCross(vec3_t origin, float size, int color);
void AAS_DrawPlaneCross(vec3_t point, vec3_t normal, float dist, int type, int color);
void AAS_ShowBoundingBox(vec3_t origin, vec3_t mins, vec3_t maxs);
void AAS_ShowFace(int facenum);
void AAS_ShowArea(int areanum, int groundfacesonly);
void AAS_ShowAreaPolygons(int areanum, int color, int groundfacesonly);
void AAS_DrawCross(vec3_t origin, float size, int color);
void AAS_PrintTravelType(int traveltype);
void AAS_DrawArrow(vec3_t start, vec3_t end, int linecolor, int arrowcolor);
void AAS_ShowReachability(struct aas_reachability_s *reach);
void AAS_ShowReachableAreas(int areanum);

