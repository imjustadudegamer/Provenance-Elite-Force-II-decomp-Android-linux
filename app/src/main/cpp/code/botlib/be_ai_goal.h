
#define MAX_AVOIDGOALS			256
#define MAX_GOALSTACK			8

#define GFL_NONE				0
#define GFL_ITEM				1
#define GFL_ROAM				2
#define GFL_DROPPED				4

typedef struct bot_goal_s
{
	vec3_t origin;				
	int areanum;				
	vec3_t mins, maxs;			
	int entitynum;				
	int number;					
	int flags;					
	int iteminfo;				
} bot_goal_t;

void BotResetGoalState(int goalstate);
void BotResetAvoidGoals(int goalstate);
void BotRemoveFromAvoidGoals(int goalstate, int number);
void BotPushGoal(int goalstate, bot_goal_t *goal);
void BotPopGoal(int goalstate);
void BotEmptyGoalStack(int goalstate);
void BotDumpAvoidGoals(int goalstate);
void BotDumpGoalStack(int goalstate);
void BotGoalName(int number, char *name, int size);
int BotGetTopGoal(int goalstate, bot_goal_t *goal);
int BotGetSecondGoal(int goalstate, bot_goal_t *goal);
int BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags);
int BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags,
							bot_goal_t *ltg, float maxtime);
int BotTouchingGoal(vec3_t origin, bot_goal_t *goal);
int BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, bot_goal_t *goal);
int BotGetLevelItemGoal(int index, char *classname, bot_goal_t *goal);
int BotGetNextCampSpotGoal(int num, bot_goal_t *goal);
int BotGetMapLocationGoal(char *name, bot_goal_t *goal);
float BotAvoidGoalTime(int goalstate, int number);
void BotSetAvoidGoalTime(int goalstate, int number, float avoidtime);
void BotInitLevelItems(void);
void BotUpdateEntityItems(void);
void BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child);
void BotSaveGoalFuzzyLogic(int goalstate, char *filename);
void BotMutateGoalFuzzyLogic(int goalstate, float range);
int BotLoadItemWeights(int goalstate, char *filename);
void BotFreeItemWeights(int goalstate);
int BotAllocGoalState(int client);
void BotFreeGoalState(int handle);
int BotSetupGoalAI(void);
void BotShutdownGoalAI(void);
