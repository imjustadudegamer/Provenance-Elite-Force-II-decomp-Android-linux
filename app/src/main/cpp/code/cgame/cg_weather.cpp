/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction


#include "cg_local.h"
#include "../corepp/vector.h"
#include "../corepp/container.h"
#include "cg_commands.h"
#include "surfaceflags.h"

typedef struct rain_s {
    vec3_t origin;         
    vec3_t renderStart;    
    vec3_t velocity;       
    vec3_t splashOrigin;   
    vec3_t splashNormal;   
    int    isDuplicate;    
    int    hitSomething;   
    float  lifeMs;         
    float  killTime;       
    int    reuseCount;     
    int    active;         
    int    spawned;        
    int    lastDupTime;    
    int    lastSplashTime; 
} rain_t;

#define MAX_RAIN_DROPS 1000

static rain_t cgw_rain[MAX_RAIN_DROPS]; 
static vec3_t cgw_lastViewOrigin;       
static int    cgw_numSpawnedThisFrame;  

static float cgw_largeSplashTokens; 
static float cgw_splashTokens;      
static int   cgw_lastTime;          
static float cgw_intensity;         
static int   cgw_targetDrops;       
static int   cgw_numDrops;          

static void CG_InitRainDrop(rain_t *drop, const Vector& center, const Vector& vel, float spawnRadius);
static void CG_InitDuplicateRainDrop(rain_t *drop);
static void CG_AddRainSplash(rain_t *drop);
static void CG_AddWeatherEffect(const str& name, const Vector& origin, Vector angles);

static rain_t *CG_FindRainDuplicateSource(void)
{
    int index;
    int tries;

    index = (int)(random() * (float)cgw_numDrops);
    if (index >= cgw_numDrops) {
        index = cgw_numDrops - 1;
    }

    tries = 0;
    if (cgw_numDrops <= 0) {
        return NULL;
    }

    while (1) {
        tries++;

        if (!cgw_rain[index].isDuplicate && cgw_rain[index].lastDupTime != cg.time) {
            break;
        }

        if (++index >= cgw_numDrops) {
            index = 0;
        }

        if (tries >= cgw_numDrops) {
            return NULL;
        }
    }

    return &cgw_rain[index];
}

static void CG_InitDuplicateRainDrop(rain_t *drop)
{
    rain_t *src;
    float   ofsx, ofsy;

    src = CG_FindRainDuplicateSource();
    if (!src) {
        return;
    }

    src->lastDupTime = cg.time;
    drop->active     = 1;

    ofsx = crandom() * 100.0f;
    ofsy = crandom() * 100.0f;

    drop->origin[0] = src->origin[0] + ofsx;
    drop->origin[1] = src->origin[1] + ofsy;
    drop->origin[2] = src->origin[2] + 0.0f;

    drop->renderStart[0] = src->renderStart[0] + ofsx;
    drop->renderStart[1] = src->renderStart[1] + ofsy;
    drop->renderStart[2] = src->renderStart[2] + 0.0f;

    drop->isDuplicate = 1;

    drop->splashOrigin[0] = src->splashOrigin[0] + ofsx;
    drop->splashOrigin[1] = src->splashOrigin[1] + ofsy;
    drop->splashOrigin[2] = src->splashOrigin[2] + 0.0f;

    VectorCopy(src->velocity, drop->velocity);

    drop->lifeMs         = src->lifeMs;
    drop->reuseCount     = 0;
    drop->lastSplashTime = 0;
    drop->spawned        = 0;
}

static void CG_InitRainDrop(rain_t *drop, const Vector& center, const Vector& vel, float spawnRadius)
{
    Vector  end;
    Vector  back;
    Vector  delta;
    Vector  dropVel;
    trace_t trace;
    float   dist;
    float   speed;
    float   newSpeed;

    if (drop->active && !drop->isDuplicate) {
        Vector flat;
        Vector oldDir;
        Vector newDir;
        float  flatDist;

        flat[0]  = center[0] - drop->origin[0];
        flat[1]  = center[1] - drop->origin[1];
        flat[2]  = 0;
        flatDist = flat.length();

        oldDir = Vector(drop->velocity);
        oldDir.normalize();
        newDir = vel;
        newDir.normalize();

        if (flatDist < 1000.0f && oldDir[0] == newDir[0] && oldDir[1] == newDir[1] && oldDir[2] == newDir[2]
            && random() < 1.0f - (float)drop->reuseCount * 0.05f) {
            drop->reuseCount++;
            drop->spawned = 0;
            return;
        }
    }

    if (cgw_numSpawnedThisFrame >= 20) {
        CG_InitDuplicateRainDrop(drop);
        return;
    }

    cgw_numSpawnedThisFrame++;

    drop->active      = 1;
    drop->origin[0]   = crandom() * spawnRadius + center[0];
    drop->origin[1]   = crandom() * spawnRadius + center[1];
    drop->origin[2]   = center[2];
    drop->isDuplicate = 0;

    end = vel;
    end.normalize();
    end = end * 2500.0f + Vector(drop->origin);

    CG_Trace(
        &trace, drop->origin, vec3_origin, vec3_origin, end, ENTITYNUM_NONE, CONTENTS_SOLID, qfalse, qtrue, "weather"
    );

    drop->hitSomething = (trace.fraction < 1.0f);

    back = vel * -1.0f;
    back.normalize();
    back = back * 500.0f;

    drop->renderStart[0] = back[0] + trace.endpos[0];
    drop->renderStart[1] = back[1] + trace.endpos[1];
    drop->renderStart[2] = back[2] + trace.endpos[2];

    VectorCopy(trace.endpos, drop->splashOrigin);
    VectorCopy(trace.plane.normal, drop->splashNormal);

    delta = Vector(drop->splashOrigin) - Vector(drop->renderStart);
    dist  = delta.length();

    VectorCopy(vel, drop->velocity);
    speed    = vel.length();
    newSpeed = (crandom() * 0.1f + 1.0f) * speed;

    dropVel = Vector(drop->velocity);
    dropVel.normalize();
    dropVel = dropVel * newSpeed;
    VectorCopy(dropVel, drop->velocity);

    if (newSpeed != 0) {
        drop->lifeMs = dist / newSpeed * 1000.0f;
    }

    drop->reuseCount = 0;
    drop->spawned    = 0;
}

static spawnthing_t cgw_effectSpawnthing;

static void CG_AddWeatherEffect(const str& name, const Vector& origin, Vector angles)
{
    spawnthing_t *sp = &cgw_effectSpawnthing;

    commandManager.InitializeSpawnthing(sp);

    sp->m_modellist.ClearObjectList();
    sp->m_modellist.AddObject(name);

    sp->cgd.origin  = origin;
    sp->cgd.life    = 1000;
    sp->life_random = 0;
    sp->cgd.flags |= (T_FADE | T_ANGLES);  
    sp->cgd.angles = angles;

    AnglesToAxis(angles, sp->axis);

    commandManager.SpawnTempModel(1, sp);
}

static void CG_AddRainSplash(rain_t *drop)
{
    Vector flat;
    Vector angles;
    Vector perp;

    if (cg.snap->ws.weather_type != WEATHER_RAIN) {
        return;
    }

    flat[0] = cg.refdef.vieworg[0] - drop->splashOrigin[0];
    flat[1] = cg.refdef.vieworg[1] - drop->splashOrigin[1];
    flat[2] = 0;

    if (flat.length() > 500.0f) {
        return;
    }
    if (cgw_splashTokens < 1.0f) {
        return;
    }
    if (drop->lastSplashTime + 2000 >= cg.time) {
        return;
    }

    cgw_splashTokens -= 1.0f;
    drop->lastSplashTime = cg.time;

    vectoangles(drop->splashNormal, angles);
    CG_AddWeatherEffect("models/fx_rainsplash.tik", Vector(drop->splashOrigin), angles);

    if (cgw_largeSplashTokens < 1.0f) {
        return;
    }

    cgw_largeSplashTokens -= 1.0f;

    PerpendicularVector(perp, drop->splashNormal);
    vectoangles(perp, angles);
    CG_AddWeatherEffect("models/fx_largerainsplash.tik", Vector(drop->splashOrigin), angles);
}

void CG_InitializeWeather(void)
{
    cgw_lastTime          = 0;
    cgw_targetDrops       = 0;
    cgw_numDrops          = 0;
    cgw_largeSplashTokens = 0;
    cgw_splashTokens      = 0;
    cgw_intensity         = 0;
}

void CG_Weather(void)
{
    spawnthing_t *emitter;
    Vector        origin;
    Vector        velocity;
    Vector        wind;
    Vector        dir;
    Vector        angles;
    float         frametime;
    float         tokens;
    float         spawnRadius;
    float         windIntensity;
    float         placeScale;
    int           target;
    int           i;
    int           oldNumDrops;

    if (cg.snap->ws.weather_type == WEATHER_SNOW) {
        emitter = commandManager.GetEmitterByName("snow");
    } else {
        emitter = commandManager.GetEmitterByName("rain");
    }

    if (!emitter) {
        return;
    }

    frametime    = (float)(cg.time - cgw_lastTime) * 0.001f;
    cgw_lastTime = cg.time;

    tokens                = 100.0f * frametime + cgw_splashTokens;
    cgw_splashTokens      = (25.0f >= tokens) ? tokens : 25.0f;
    tokens                = 10.0f * frametime + cgw_largeSplashTokens;
    cgw_largeSplashTokens = (tokens <= 25.0f) ? tokens : 25.0f;

    if (frametime == 0.0f) {
        spawnRadius = 0.0f;
    } else {
        spawnRadius = (Vector(cg.refdef.vieworg) - Vector(cgw_lastViewOrigin)).length() / frametime;
    }
    spawnRadius += 1000.0f;

    VectorCopy(cg.refdef.vieworg, cgw_lastViewOrigin);
    cgw_numSpawnedThisFrame = 0;

    target = cg.snap->ws.weather_intensity;
    if (target > MAX_RAIN_DROPS) {
        target = MAX_RAIN_DROPS;
    }

    if ((float)target <= cgw_intensity) {
        if (cgw_intensity > (float)target) {
            float next = frametime * -25.0f + cgw_intensity;

            cgw_intensity = (next >= (float)target) ? next : (float)target;
        }
    } else {
        float next = 25.0f * frametime + cgw_intensity;

        cgw_intensity = (next <= (float)target) ? next : (float)target;
    }

    cgw_targetDrops = (int)cgw_intensity;

    wind = Vector(cg.snap->ws.wind_direction);
    wind.normalize();

    if (cg.snap->ws.weather_type == WEATHER_SNOW) {
        wind[0] += crandom() * 0.2f;
        wind[1] += crandom() * 0.2f;
        if (wind.length() != 0.0f) {
            wind.normalize();
        }
    }

    windIntensity = cg.snap->ws.wind_intensity;

    velocity[0] = wind[0] * windIntensity * 100.0f;
    velocity[1] = wind[1] * windIntensity * 100.0f;
    velocity[2] = 100.0f * (windIntensity * wind[2]);

    if (cg.snap->ws.weather_type == WEATHER_SNOW) {
        velocity[2] = crandom() * 50.0f + -200.0f;
    } else {
        velocity[2] = -1250.0f;
    }

    dir = velocity;
    dir.normalize();
    placeScale = (cg.snap->ws.weather_type == WEATHER_SNOW) ? -1000.0f : -2000.0f;
    origin     = Vector(cg.refdef.vieworg) + dir * placeScale;

    oldNumDrops = cgw_numDrops;
    if (oldNumDrops < cgw_targetDrops) {
        i = oldNumDrops;
        do {
            CG_InitRainDrop(&cgw_rain[i], origin, velocity, spawnRadius);
            i++;
        } while (i < cgw_targetDrops);
    }

    for (i = cgw_targetDrops; i < oldNumDrops; i++) {
        cgw_rain[i].active = 0;
    }

    cgw_numDrops = cgw_targetDrops;

    for (i = 0; i < cgw_numDrops; i++) {
        rain_t *drop = &cgw_rain[i];

        if (drop->active && drop->spawned && drop->killTime < (float)cg.time) {
            CG_InitRainDrop(drop, origin, velocity, spawnRadius);

            if (!drop->isDuplicate && drop->hitSomething) {
                CG_AddRainSplash(drop);
            }
        }
    }

    for (i = 0; i < cgw_numDrops; i++) {
        rain_t *drop = &cgw_rain[i];

        if (!drop->active || drop->spawned) {
            continue;
        }

        drop->spawned  = 1;
        drop->killTime = (float)cg.time + drop->lifeMs;

        emitter->cgd.origin   = Vector(drop->renderStart);
        emitter->cgd.velocity = Vector(drop->velocity);

        vectoangles(drop->velocity, angles);
        AnglesToAxis(angles, emitter->axis);

        emitter->cgd.life        = (int)drop->lifeMs;
        emitter->life_random     = 0;
        emitter->forwardVelocity = VectorLength(drop->velocity);
        emitter->cgd.flags2 |= T2_MOVE;

        commandManager.SpawnTempModel(1, emitter);
    }
}
