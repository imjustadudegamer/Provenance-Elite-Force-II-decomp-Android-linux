

#pragma once

#if defined(CGAME_DLL)
#    include "../cgame_hook/script/centity.h"
#elif defined(GAME_DLL)
#    include "entity.h"
#else
#    include "glb_local.h"
#    include "simpleentity.h"
#endif

#include "gamescript.h"

#define WORLD_CINEMATIC 1

typedef enum {
    WORLD_PHYSICS_MAXSPEED,
    WORLD_PHYSICS_AIRACCELERATE,
    WORLD_PHYSICS_GRAVITY,

    WORLD_PHYSICS_TOTAL_NUMBER
} WorldPhysicsVarTypes;

class TargetList : public Class
{
public:
    CLASS_PROTOTYPE(TargetList);

public:
    Container<SafePtr<Entity>> list;
    str       targetname;
    int index;

public:
    TargetList();
    TargetList(const str& tname);
    virtual ~TargetList() override;

    void Archive(Archiver& arc) override;

    void          AddEntity(Listener *ent);
    void          RemoveEntity(Listener *ent);
    Entity *GetNextEntity(Entity *ent);
};

class DynamicLightInfo
{
public:
    float defaultIntensity;

    float intensity;

    float start_intensity;
    float end_intensity;

    float start_fade_time;
    float fade_time;

    str      lightstyle;
    int      current_lightstyle_position;
    qboolean stop_at_end;

    DynamicLightInfo();
    void Archive(Archiver& arc);
};

class WindInfo
{
public:
    Vector direction;
    float  intensity;

    float start_intensity;
    float end_intensity;

    float min_intensity;
    float max_intensity;

    float start_fade_time;
    float fade_time;

    float gust_time;

    float max_change;

    WindInfo();
    virtual void Archive(Archiver& arc);
};

inline void WindInfo::Archive(Archiver& arc)
{
    arc.ArchiveVector(&direction);
    arc.ArchiveFloat(&intensity);
    arc.ArchiveFloat(&start_intensity);
    arc.ArchiveFloat(&end_intensity);

    arc.ArchiveFloat(&min_intensity);
    arc.ArchiveFloat(&max_intensity);

    arc.ArchiveFloat(&start_fade_time);
    arc.ArchiveFloat(&fade_time);

    arc.ArchiveFloat(&gust_time);

    arc.ArchiveFloat(&max_change);
}

class WeatherInfo
{
public:
    weather_t type;
    int       intensity;

    WeatherInfo();
    virtual void Archive(Archiver& arc);
};

inline void WeatherInfo::Archive(Archiver& arc)
{
    ArchiveEnum(type, weather_t);
    arc.ArchiveInteger(&intensity);
}

class World : public Entity
{
    Container<TargetList *> m_targetListContainer;
    qboolean                world_dying;

    Container<str> _brokenThings;

public:
    TargetList *GetTargetList(const str &targetname, qboolean createnew = true);
    TargetList *GetTargetList(int index);
    float    farplane_distance;
    Vector   farplane_color;
    qboolean farplane_cull;
    qboolean farplane_fog;

    qboolean terrain_global;
    float    terrain_global_min;

    float entity_fade_dist;

    Vector slime_color;
    float  slime_alpha;

    float    sky_alpha;
    qboolean sky_portal;

    float _physicsInfo[WORLD_PHYSICS_TOTAL_NUMBER];

    bool _canShakeCamera;

    float m_fAIVisionDistance;
    float m_fNorth;

    float radius;

    Container<str> _availableViewModes;

    DynamicLightInfo dynamic_lights[MAX_LIGHTING_GROUPS];

    WindInfo    wind;
    WeatherInfo weather;
    float       time_scale;

public:
    CLASS_PROTOTYPE(World);

    World();
    ~World();

    void AddTargetEntity(const str &targetname, Entity *ent);  
    void RemoveTargetEntity(Entity *ent);
    void RemoveTargetEntity(const str& targetname, Entity *ent);  

    void FreeTargetList();




    void SetSoundtrack(Event *ev);
    str skipthread;  
    void SetSkipThread(Event *ev);
    void SetNextMap(Event *ev);
    void SetMessage(Event *ev);
    void SetScript(Event *ev);
    void SetWaterColor(Event *ev);
    void SetWaterAlpha(Event *ev);
    void SetSlimeColor(Event *ev);
    void SetSlimeAlpha(Event *ev);
    void SetLavaColor(Event *ev);
    void SetLavaAlpha(Event *ev);
    void SetFarPlane_Color(Event *ev);
    void SetFarPlane_Cull(Event *ev);
    void SetFarPlane_Fog(Event *ev);
    void SetFarPlane(Event *ev);
    void SetTerrainGlobal(Event *ev);
    void SetTerrainGlobalMin(Event *ev);
    void SetEntityFadeDist(Event *ev);
    void SetSkyAlpha(Event *ev);
    void SetSkyPortal(Event *ev);
    void UpdateConfigStrings(void);
    void UpdateFog(void);
    void UpdateTerrain(void);
    void UpdateEntityFadeDist(void);
    void UpdateSky(void);

    void UpdateDynamicLights(void);
    void UpdateWindDirection(void);
    void UpdateWindIntensity(void);
    void UpdateWeather(void);
    void UpdateTimeScale(void);

    void SetLightIntensity(Event *ev);
    void SetLightDefaultIntensity(Event *ev);
    void SetLightFade(Event *ev);
    void SetLightLightstyle(Event *ev);
    void GetLightIntensity(Event *ev);

    void SetWindDirection(Event *ev);
    void SetWindIntensity(Event *ev);
    void SetWindGust(Event *ev);
    void SetWindFade(Event *ev);
    void SetWindMinMax(Event *ev);
    void SetWeather(Event *ev);
    void SetTimeScale(Event *ev);

    int   worldPhysicsVarNameToIndex(const char *varName);
    void  setPhysicsVar(Event *ev);
    float getPhysicsVar(const char *varName);

    int  findBrokenThing(const char *name);
    void addBrokenThing(const char *name);
    void removeBrokenThing(const char *name);
    void addBrokenThing(Event *ev);
    void removeBrokenThing(Event *ev);
    bool isThingBroken(const char *name);
    void freeAllBrokenThings(void);

    void setCanShakeCamera(Event *ev);
    bool canShakeCamera(void);

    void Think(void) override;

    void Archive(Archiver& arc) override;

public:






public:
    Entity *GetNextEntity( const str &targetname, Entity * ent ); 
    void addAvailableViewMode( Event *ev ); 
    void clearAvailableViewModes( Event *ev ); 
    void removeAvailableViewMode( Event *ev ); 
    bool isAnyViewModeAvailable( void ); 
    bool isViewModeAvailable( const char *name ); 
    void setPhysicsVar( const char *varName, float varValue ); 
    float getPhysicsVar( int index ); 
};

typedef SafePtr<World> WorldPtr;
extern WorldPtr        world;

