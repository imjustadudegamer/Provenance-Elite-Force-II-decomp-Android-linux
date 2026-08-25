
#pragma once

#include "../qcommon/q_shared.h"
#include "bg_public.h"
#include "g_public.h"
#include "container.h"
#include "str.h"


#define GAMEVERSION                "provenanceef2"

#define  EV_POSTSPAWN         -1.0f

#define  EV_REMOVE            -12.0f    
#define  EV_PRIORITY_SPAWNARG -11.0f    
#define  EV_SPAWNARG          -10.0f    
#define  EV_LINKDOORS         -9.0f     
#define  EV_LINKBEAMS         -9.0f     
#define  EV_SETUP_ROPEPIECE   -8.0f
#define  EV_SETUP_ROPEBASE    -7.0f
#define  EV_PROCESS_INIT      -6.0f

#define BODY_QUEUE_SIZE            8

#define INFINITE                   1000000

#define FRAMETIME                  level.frametime 
#define CARNAGE_REWARD_TIME        3000
#define REWARD_SPRITE_TIME         2000

#define INTERMISSION_DELAY_TIME    1000
#define SP_INTERMISSION_DELAY_TIME 5000

#define FL_FLY              (1 << 0)
#define FL_SWIM             (1 << 1) 
#define FL_INWATER          (1 << 2)
#define FL_GODMODE          (1 << 3)
#define FL_NOTARGET         (1 << 4)
#define FL_PARTIALGROUND    (1 << 5) 
#define FL_TEAMSLAVE        (1 << 6) 
#define FL_NO_KNOCKBACK     (1 << 7)
#define FL_THINK            (1 << 8)
#define FL_BLOOD            (1 << 9)  
#define FL_DIE_GIBS         (1 << 10) 
#define FL_DIE_EXPLODE      (1 << 11) 
#define FL_ROTATEDBOUNDS    (1 << 12) 
#define FL_DONTSAVE         (1 << 13) 
#define FL_IMMOBILE         (1 << 14) 
#define FL_PARTIAL_IMMOBILE (1 << 15) 
#define FL_STUNNED          (1 << 16)  
#define FL_POSTTHINK        (1 << 17) 
#define FL_TOUCH_TRIGGERS   (1 << 18) 
#define FL_AUTOAIM          (1 << 19) 
#define FL_ANIMATE          (1 << 20) 
#define FL_UNKNOWN2         (1 << 21)

#define SOUND_RADIUS        1500.0f 


#include "g_main.h"

class str;
class Vector;
class Archiver;
class Entity;
template<typename T>
class Container;

typedef enum {
    SPECTATOR_NOT,
    SPECTATOR_FREE,
    SPECTATOR_FOLLOW,
    SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
    TEAM_BEGIN, 
    TEAM_ACTIVE 
} playerTeamStateState_t;

typedef struct {
    playerTeamStateState_t state;

    int location;

    int captures;
    int basedefense;
    int carrierdefense;
    int flagrecovery;
    int fragcarrier;
    int assists;

    float lasthurtcarrier;
    float lastreturnedflag;
    float flagsince;
    float lastfraggedcarrier;
} playerTeamState_t;

#define FOLLOW_ACTIVE1 -1
#define FOLLOW_ACTIVE2 -2

typedef struct {
    teamtype_t       sessionTeam;
    int              spectatorTime; 
    spectatorState_t spectatorState;
    int              spectatorClient; 
    int              wins, losses;    
    qboolean         teamLeader;      
} clientSession_t;

#define MAX_NETNAME    36
#define MAX_VOTE_COUNT 3

const char *G_FixTIKIPath(const char *in);
void        G_GetNullEntityState(entityState_t *nullState);

void Cmd_Score_f(gentity_t *ent);
void StopFollowing(gentity_t *ent);
void BroadcastTeamChange(gclient_t *client, int oldTeam);
void SetTeam(gentity_t *ent, char *s);
void Cmd_FollowCycle_f(gentity_t *ent, int dir);


float G_Random(void);
float G_CRandom(void);

int        G_ModelIndex(const char *name);
int        G_SoundIndex(const char *name);
void       G_TeamCommand(teamtype_t team, char *cmd);
void       G_KillBox(gentity_t *ent);
qboolean   M_CheckBottom(Entity *ent);
gentity_t *G_PickTarget(char *targetname);
void       G_UseTargets(gentity_t *ent, gentity_t *activator);

void       G_InitGentity(gentity_t *e);
gentity_t *G_Spawn(void);
gentity_t *G_TempEntity(vec3_t origin, int event);
void       G_Sound(gentity_t *ent, int channel, int soundIndex);
void       G_FreeEntity(gentity_t *e);
qboolean   G_entitiesFree(void);

Entity *G_NextEntity(Entity *ent);

void G_CalcBoundsOfMove(Vector& start, Vector& end, Vector& mins, Vector& maxs, Vector *minbounds, Vector *maxbounds);

float PlayersRangeFromSpot(Entity *ent);

Entity *findradius(Entity *startent, Vector org, float rad);
Entity *findclientsinradius(Entity *startent, Vector org, float rad);

void G_TouchTriggers(gentity_t *ent);
void G_TouchSolids(gentity_t *ent);

int G_NumClients(void);

int G_WeaponIndex(const char *name);

class SkelMat4;



float *tv(float x, float y, float z);
char  *vtos(const vec3_t v);

void        G_SetOrigin(gentity_t *ent, vec3_t origin);
void        AddRemap(const char *oldShader, const char *newShader, float timeOffset);
const char *BuildShaderStateConfig(void);

void G_DebugDamage(float damage, Entity *victim, Entity *attacker, Entity *inflictor);

qboolean KillBox(Entity *ent);

void        ChangeMusic(const char *current, const char *fallback, qboolean force);
void        ChangeMusicVolume(float volume, float fade_time);
void        RestoreMusicVolume(float fade_time);
void        ChangeSoundtrack(const char *soundtrack);
void        RestoreSoundtrack(void);

void CloneEntity(Entity *dest, const Entity *src);  
void G_ArchiveEdict(Archiver& arc, gentity_t *edict);

void G_ArchiveClientRetail(Archiver& arc, gclient_t *client);

const char *G_LocationNumToDispString(int iLocation);

void G_CheckIntermissionExit(void);

void G_MoveClientToIntermission(Entity *ent);  
void G_SentInitialMessages(void);             


void G_SetTrajectory(gentity_t *ent, vec3_t org);

qboolean CanDamage(gentity_t *targ, vec3_t origin);
void     G_Damage(
        gentity_t *targ,
        gentity_t *inflictor,
        gentity_t *attacker,
        vec3_t     dir,
        vec3_t     point,
        int        damage,
        int        dflags,
        int        mod
    );
qboolean G_RadiusDamage(vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod);
int      G_InvulnerabilityEffect(gentity_t *targ, vec3_t dir, vec3_t point, vec3_t impactpoint, vec3_t bouncedir);
void     body_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath);
void     TossClientItems(gentity_t *self);
#ifdef MISSIONPACK
void TossClientPersistantPowerups(gentity_t *self);
#endif
void TossClientCubes(gentity_t *self);

#define DAMAGE_NONE          0
#define DAMAGE_RADIUS        0x00000001 
#define DAMAGE_NO_ARMOR      0x00000002 
#define DAMAGE_ENERGY        0x00000004 
#define DAMAGE_NO_KNOCKBACK  0x00000008 
#define DAMAGE_BULLET        0x00000010 
#define DAMAGE_NO_PROTECTION 0x00000020 
#define DAMAGE_NO_SKILL      0x00000040 

void G_RunMissile(gentity_t *ent);

gentity_t *fire_blaster(gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_plasma(gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_grenade(gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_rocket(gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *fire_bfg(gentity_t *self, vec3_t start, vec3_t dir);
gentity_t *fire_grapple(gentity_t *self, vec3_t start, vec3_t dir);
#ifdef MISSIONPACK
gentity_t *fire_nail(gentity_t *self, vec3_t start, vec3_t forward, vec3_t right, vec3_t up);
gentity_t *fire_prox(gentity_t *self, vec3_t start, vec3_t aimdir);
#endif

void G_RunMover(gentity_t *ent);
void Touch_DoorTrigger(gentity_t *ent, gentity_t *other, trace_t *trace);
void Use_BinaryMover(gentity_t *ent, gentity_t *other, gentity_t *activator);

void trigger_teleporter_touch(gentity_t *self, gentity_t *other, trace_t *trace);

void TeleportPlayer(gentity_t *player, vec3_t origin, vec3_t angles);
#ifdef MISSIONPACK
void DropPortalSource(gentity_t *ent);
void DropPortalDestination(gentity_t *ent);
#endif


gentity_t         *G_Find(gentity_t *from, int fieldofs, const char *match);
int                TeamLeader(int team);
void               SetClientViewAngle(gentity_t *ent, vec3_t angle);
class PlayerStart;
void               respawn(gentity_t *ent);
void               BeginIntermission(void);
void               InitClientPersistant(gclient_t *client);
void               InitClientResp(gclient_t *client);
void               player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);
void               AddScore(gentity_t *ent, vec3_t origin, int score);
void               CalculateRanks(void);

qboolean G_ConsoleCommand(void);
void     G_ProcessIPBans(void);
qboolean G_FilterPacket(char *from);

#ifdef MISSIONPACK
void G_StartKamikaze(gentity_t *ent);
#endif

void MoveClientToIntermission(gentity_t *client);
void G_SetStats(gentity_t *ent);
void DeathmatchScoreboardMessage(gentity_t *client);



void       FindIntermissionPoint(void);
void       SetLeader(int team, int client);
void       CheckTeamLeader(int team);
void       G_RunThink(gentity_t *ent);
void QDECL G_LogPrintf(const char *fmt, ...);
void       SendScoreboardMessageToAllClients(void);
void QDECL G_Error(const char *fmt, ...) Q_PRINTF_FUNC(1, 2) Q_NO_RETURN;
void QDECL G_Error(int level, const char *fmt, ...) Q_PRINTF_FUNC(2, 3);  

qboolean    G_BotConnect(int clientNum, qboolean restart);
const char *G_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot, qboolean checkPassword);
void        G_ClientUserinfoChanged(gentity_t *ent, const char *userinfo);
qboolean    G_duplicateName(const char *name);    
void        G_BuildUniqueName(const char *newName, char *name);  
void        G_ClientDisconnect(gentity_t *ent);
void        G_ClientBegin(gentity_t *ent, const usercmd_t *cmd);  
void        G_ClientCommand(gentity_t *ent);


extern "C" void G_ClientThink(gentity_t *ent, usercmd_t *ucmd);  
void G_ClientEndServerFrames(void);

qboolean OnSameTeam(gentity_t *ent1, gentity_t *ent2);
void     Team_CheckDroppedItem(gentity_t *dropped);
qboolean CheckObeliskAttack(gentity_t *obelisk, gentity_t *attacker);

void *G_Alloc(int size);
void  G_InitMemory(void);
void  Svcmd_GameMem_f(void);



void UpdateTournamentInfo(void);
void SpawnModelsOnVictoryPads(void);
void Svcmd_AbortPodium_f(void);


typedef struct mmove_s {
    vec3_t origin;
    vec3_t velocity;

    qboolean walking;
    qboolean groundPlane;
    vec3_t   groundPlaneNormal;

    float frametime;

    float desired_speed;
    int   tracemask;
    float desired_dir[2];

    int entityNum;

    vec3_t mins;
    vec3_t maxs;

    int numtouch;
    int touchents[MAXTOUCH];

    qboolean hit_obstacle;
    vec3_t   hit_origin;
    int      hit_temp_obstacle;

    vec3_t obstacle_normal;
} mmove_t;

void MmoveSingle(mmove_t *mmove);

void VmoveSingle(struct vmove_s *mmove);

#define MAX_FILEPATH 144

typedef struct bot_settings_s {
    char  characterfile[MAX_FILEPATH];
    float skill;
    char  team[MAX_FILEPATH];
} bot_settings_t;

int  BotAISetup(int restart);
int  BotAIShutdown();
int  BotAILoadMap(int restart);
int  BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart);
int  BotAIShutdownClient(int client, qboolean restart);
int  BotAIStartFrame(int time);
void BotTestAAS(vec3_t origin);

extern gentity_t *g_entities;
#define FOFS(x) ((size_t) & (((gentity_t *)0)->x))

#include "g_utils.h"
