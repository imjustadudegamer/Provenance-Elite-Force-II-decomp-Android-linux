

#pragma once

#include "../corepp/listener.h"
#include "../script/scriptvariable.h" 
#include "g_public.h"

enum fadetype_t {
    fadein,
    fadeout
};

enum fadestyle_t {
    alphablend,
    additive
};

enum letterboxdir_t {
    letterbox_in,
    letterbox_out
};


typedef struct {
    str    m_sName;
    Vector m_vOrigin;
} landmark_t;

extern gclient_t *spawn_client;

class Camera;
class Earthquake;
class Actor;

class HelperNodeController;

class Level : public Listener
{
public:
    HelperNodeController *hNodeController;

    const char *current_map;
    const char *current_entities;



    CThread *consoleThread;

    int spawn_entnum;
    int spawnflags;
    int currentInstanceNumber;

    int framenum;
    int inttime;
    gentity_s *next_edict;
    float fixedframetime;

    float time;
    float frametime;


    str level_name;
    str mapname;
    str spawnpoint;
    str nextmap;


    qboolean playerfrozen;

    float timeInLevel;
    int   startTime;

    float     intermissiontime;
    int       exitintermission;
    float     intermission_advancetime;  
    bool      _showIntermission;         
    bool _saveOrientation;

    int total_secrets;
    int found_secrets;

    str game_script;  

    trace_t impact_trace;

    qboolean cinematic;

    qboolean ai_on;

    qboolean mission_failed;
    qboolean died_already;
    qboolean near_exit;

    Vector water_color;
    Vector slime_color;  
    Vector lava_color;
    float  water_alpha;
    float  slime_alpha;  
    float  lava_alpha;

    str current_soundtrack;
    str saved_soundtrack;

    Vector      m_fade_color;
    float       m_fade_alpha;
    float       m_fade_time;
    float       m_fade_time_start;
    fadetype_t  m_fade_type;
    fadestyle_t m_fade_style;

    float          m_letterbox_fraction;
    float          m_letterbox_time;
    float          m_letterbox_time_start;
    letterboxdir_t m_letterbox_dir;

    Container<Camera *> automatic_cameras;



    Vector m_intermission_origin;
    Vector m_intermission_angle;

    bool _cleanup;



    Container<SafePtr<Earthquake>> _earthquakes;

    int total_specialItems;
    int found_specialItems;

    qboolean restart;
    qboolean started;



    str _playerDeathThread;

    int  _totalEnemiesSpawned;

public:

public:
    CLASS_PROTOTYPE(Level);

    Level();
    virtual ~Level() override;

    void       Init();
    void       CleanUp(qboolean restart_shadow1 = qfalse);
    void       ResetEdicts();
    gentity_t *AllocEdict(Entity *ent);
    void       FreeEdict(gentity_t *ent);
    void       InitEdict(gentity_t *ent);
    void       Precache();
    void       FindTeams();

    void SpawnEntities(const char *themapname, const char *entities, int levelTime);  
    void loadLevelStrings(void);  
    qboolean inhibitEntity(int spawnflags);

    void setSkill(int value);
    void setTime(int levelTime, int frameTime);

    void AddAutomaticCamera(Camera *cam);







    void  addEarthquake(class Earthquake *e);
    void  removeEarthquake(class Earthquake *e);
    float getEarthquakeMagnitudeAtPosition(const Vector& pos);


    void Restart();

    void Archive(Archiver& arc) override;
    void EndIntermission(Event *ev);
    void NewMap(Event *ev);
    void PlayerRestart(Event *ev);
    void SetGameScript(Event *ev);
    void SetIntermissionAdvanceTime(Event *ev);
    void Start(Event *ev);
    void postLoad(Event *ev);
    void postSublevelLoad(Event *ev);
    void setPlayerDeathThread(Event *ev);
    void update(Event *ev);
    str getPlayerDeathThread(Event *ev);
    int getSkill(Event *ev);






public:
    void enemySpawned( Entity *enemy ); 
    void EndIntermission(); 
    void NewMap( const char *mapname, const char *entities, int levelTime ); 
    void PlayerRestart( void ); 
    void SetGameScript( const char *scriptname ); 
    void SetIntermissionAdvanceTime(float); 
    void Start( void ); 
    void postLoad( void ); 
    void postSublevelLoad( const char *spawnPosName ); 
    void setPlayerDeathThread( const str &threadName ); 
    void update(int levelTime, int frameTime); 
    str getPlayerDeathThread( void ); 
    int getSkill( void ); 
};


void G_BeginIntermission2(void);

void G_BeginIntermission(const char *map_name);  


void G_ExitLevel(void);

extern Level level;

extern ScriptVariableList levelVars;
extern ScriptVariableList gameVars;
