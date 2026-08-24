

#ifdef AASINTERN
extern aas_settings_t aassettings;
#endif 

int AAS_PredictClientMovement(struct aas_clientmove_s *move,
							int entnum, vec3_t origin,
							int presencetype, int onground,
							vec3_t velocity, vec3_t cmdmove,
							int cmdframes,
							int maxframes, float frametime,
							int stopevent, int stopareanum, int visualize);
int AAS_ClientMovementHitBBox(struct aas_clientmove_s *move,
								int entnum, vec3_t origin,
								int presencetype, int onground,
								vec3_t velocity, vec3_t cmdmove,
								int cmdframes,
								int maxframes, float frametime,
								vec3_t mins, vec3_t maxs, int visualize);
int AAS_OnGround(vec3_t origin, int presencetype, int passent);
int AAS_Swimming(vec3_t origin);
void AAS_JumpReachRunStart(struct aas_reachability_s *reach, vec3_t runstart);
int AAS_AgainstLadder(vec3_t origin);
float AAS_RocketJumpZVelocity(vec3_t origin);
float AAS_BFGJumpZVelocity(vec3_t origin);
int AAS_HorizontalVelocityForJump(float zvel, vec3_t start, vec3_t end, float *velocity);
void AAS_SetMovedir(vec3_t angles, vec3_t movedir);
int AAS_DropToFloor(vec3_t origin, vec3_t mins, vec3_t maxs);
void AAS_InitSettings(void);
