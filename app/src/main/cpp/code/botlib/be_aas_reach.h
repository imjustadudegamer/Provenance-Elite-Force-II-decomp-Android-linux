

#ifdef AASINTERN
void AAS_InitReachability(void);
int AAS_ContinueInitReachability(float time);
int AAS_BestReachableLinkArea(aas_link_t *areas);
#endif 

int AAS_AreaReachability(int areanum);
int AAS_BestReachableArea(vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t goalorigin);
int AAS_BestReachableFromJumpPadArea(vec3_t origin, vec3_t mins, vec3_t maxs);
int AAS_NextModelReachability(int num, int modelnum);
float AAS_AreaGroundFaceArea(int areanum);
int AAS_AreaCrouch(int areanum);
int AAS_AreaSwim(int areanum);
int AAS_AreaLiquid(int areanum);
int AAS_AreaLava(int areanum);
int AAS_AreaSlime(int areanum);
int AAS_AreaGrounded(int areanum);
int AAS_AreaLadder(int areanum);
int AAS_AreaJumpPad(int areanum);
int AAS_AreaDoNotEnter(int areanum);
