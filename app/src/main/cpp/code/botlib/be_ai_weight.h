

#define WT_BALANCE			1
#define MAX_WEIGHTS			128

typedef struct fuzzyseperator_s
{
	int index;
	int value;
	int type;
	float weight;
	float minweight;
	float maxweight;
	struct fuzzyseperator_s *child;
	struct fuzzyseperator_s *next;
} fuzzyseperator_t;

typedef struct weight_s
{
	char *name;
	struct fuzzyseperator_s *firstseperator;
} weight_t;

typedef struct weightconfig_s
{
	int numweights;
	weight_t weights[MAX_WEIGHTS];
	char		filename[MAX_QPATH];
} weightconfig_t;

weightconfig_t *ReadWeightConfig(char *filename);
void FreeWeightConfig(weightconfig_t *config);
qboolean WriteWeightConfig(char *filename, weightconfig_t *config);
int FindFuzzyWeight(weightconfig_t *wc, char *name);
float FuzzyWeight(int *inventory, weightconfig_t *wc, int weightnum);
float FuzzyWeightUndecided(int *inventory, weightconfig_t *wc, int weightnum);
void ScaleWeight(weightconfig_t *config, char *name, float scale);
void ScaleBalanceRange(weightconfig_t *config, float scale);
void EvolveWeightConfig(weightconfig_t *config);
void InterbreedWeightConfigs(weightconfig_t *config1, weightconfig_t *config2, weightconfig_t *configout);
void BotShutdownWeights(void);
