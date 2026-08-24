

#ifdef AASINTERN
void AAS_InitRouting(void);
void AAS_FreeRoutingCaches(void);
unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end);
void AAS_CreateAllRoutingCache(void);
void AAS_WriteRouteCache(void);
void R203_RouteFrame(void);
void R203_RouteCount(int which, int n);
void R203_RouteBusec(int usec);
void AAS_RoutingInfo(void);
#endif 

int AAS_TravelFlagForType(int traveltype);
int AAS_AreaContentsTravelFlags(int areanum);
int AAS_NextAreaReachability(int areanum, int reachnum);
void AAS_ReachabilityFromNum(int num, struct aas_reachability_s *reach);
int AAS_RandomGoalArea(int areanum, int travelflags, int *goalareanum, vec3_t goalorigin);
int AAS_EnableRoutingArea(int areanum, int enable);
unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end);
int AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int AAS_PredictRoute(struct aas_predictroute_s *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum);


