
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


// Field sizes and offsets are the Mac-binary measurements, not the SDK's (the SDK
enum {
    ALIAS_NAME_MAX       = 32,
    ALIAS_REAL_NAME_MAX  = 128,
    MAX_ANIM_NAME_LENGTH = 128,
    MAX_ALIASLIST_NAME_LENGTH = 64
};

typedef struct AliasListNode_s {
    char  alias_name[ALIAS_NAME_MAX];
    char  real_name[ALIAS_REAL_NAME_MAX];
    char  anim_name[MAX_ANIM_NAME_LENGTH];
    float weight;  
    int   loop_anim;

    byte  global_flag;
    byte  stop_flag;
    float timeout;     
    int   maximum_use; 

    int  number_of_times_played;
    byte been_played_this_loop;
    int  last_time_played;

    struct AliasActorNode_s *actor_list;
    struct AliasListNode_s  *next;
} AliasListNode_t;

typedef struct AliasList_s {
    char              name[MAX_ALIASLIST_NAME_LENGTH];
    qboolean          dirty;
    int               num_in_list;
    AliasListNode_t **sorted_list;
    AliasListNode_t  *data_list;
} AliasList_t;

#if defined(APP_MODULE)

const char  *Alias_Find(const char *alias);
qboolean     Alias_Add(const char *alias, const char *name, const char *parameters);
qboolean     Alias_Delete(const char *alias);
const char *Alias_FindRandom(const char *alias);
void        Alias_Dump(void);
void        Alias_Clear(void);

int         S_ChannelNameToNum(const char *pszName);
const char *S_ChannelNumToName(int iChannel);

#endif


typedef struct AliasActorNode_s {
    int actor_number;

    int  number_of_times_played;
    byte been_played_this_loop;
    int  last_time_played;

    struct AliasActorNode_s *next;
} AliasActorNode_t;

void              Alias_ListClearActors(AliasList_t *list);
AliasList_t      *AliasList_New(const char *name);
const char       *Alias_ListFind(AliasList_t *list, const char *alias);
AliasListNode_t  *Alias_ListFindNode(AliasList_t *list, const char *alias);
qboolean          Alias_ListAdd(AliasList_t *list, const char *alias, const char *name, const char *parameters);
const char       *Alias_ListFindRandom(AliasList_t *list, const char *alias);
void              Alias_ListDump(AliasList_t *list);
void              Alias_ListClear(AliasList_t *list);
void              Alias_ListDelete(AliasList_t *list);
void              Alias_ListSort(AliasList_t *list);
int               Alias_IsGlobal(AliasListNode_t *node, int actor_number);
AliasActorNode_t *Alias_FindActor(AliasListNode_t *node, int actor_number);
void              Alias_ListFindRandomRange(
                 AliasList_t *list, const char *alias, int *min_index, int *max_index, float *total_weight
             );
const char *Alias_ListFindDialog(AliasList_t *list, const char *alias, qboolean random_flag, int actor_number);
void        Alias_ListUpdateDialog(
           AliasList_t *list, const char *alias, int number_of_times_played, byte been_played_this_loop, int last_time_played
       );
const char       *Alias_ListFindSpecificAnim(AliasList_t *list, const char *name);
qboolean          Alias_ListCheckLoopAnim(AliasList_t *list, const char *name);
void              Alias_ListAddActorDialog(
                  AliasList_t *list, const char *alias, int actor_number, int number_of_times_played, byte been_played_this_loop, int last_time_played
              );
int               Alias_ListNumNotUsed(AliasList_t *list, int min_index, int max_index, int actor_number);
void              Alias_ListResetAlias(AliasList_t *list, int min_index, int max_index, int actor_number);
float       randweight(void);

#ifdef __cplusplus
}
#endif
