


#pragma once

#include "cg_local.h"
#include "listener.h"
#include "script.h"
#include "../corepp/vector.h"
#include "../qcommon/qcommon.h"

#define EMITTER_DEFAULT_LIFE  1000
#define MAX_EMITTERS          32
#define MAX_SWIPES            32

#define T_RANDSCALE           (1 << 0)
#define T_SCALEANIM           (1 << 1)
#define T_SPHERE              (1 << 2)
#define T_INWARDSPHERE        (1 << 3)
#define T_CIRCLE              (1 << 4)
#define T_FADE                (1 << 5)
#define T_DIETOUCH            (1 << 6)
#define T_ANGLES              (1 << 7)
#define T_WAVE                (1 << 8)
#define T_SWARM               (1 << 9)
#define T_ALIGN               (1 << 10)
#define T_COLLISION           (1 << 11)
#define T_FLICKERALPHA        (1 << 12)
#define T_DLIGHT              (1 << 13)
#define T_FADEIN              (1 << 14)
#define T_GLOBALFADEIN        (1 << 15)
#define T_GLOBALFADEOUT       (1 << 16)
#define T_PARENTLINK          (1 << 17)
#define T_RANDOMROLL          (1 << 18)
#define T_HARDLINK            (1 << 19)
#define T_ANIMATEONCE         (1 << 20)
#define T_BEAMTHING           (1 << 21)
#define T_RANDVELAXIS         (1 << 22)
#define T_BOUNCESOUND         (1 << 23)
#define T_BOUNCESOUNDONCE     (1 << 24)
#define T_TWINKLE             (1 << 25)
#define T_TWINKLE_OFF         (1 << 26)
#define T_ALIGNONCE           (1 << 27)
#define T_SCALEUPDOWN         (1 << 28)
#define T_AUTOCALCLIFE        (1 << 29)
#define T_ASSIGNED_NUMBER     (1 << 30)
#define T_DETAIL              (1 << 31)

#define T2_MOVE               (1 << 0)
#define T2_AMOVE              (1 << 1)
#define T2_ACCEL              (1 << 2)
#define T2_TRAIL              (1 << 3)
#define T2_PHYSICS_EVERYFRAME (1 << 4)
#define T2_TEMPORARY_DECAL    (1 << 5)
#define T2_BOUNCE_DECAL       (1 << 6)
#define T2_PARALLEL           (1 << 7)
// Bits 1<<15 and up have NO retail setter at all; the port-only names that used to sit
#define T2_COLORRANGE         (1 << 8)
#define T2_FADE               (1 << 9)
#define T2_EMITTERSPIN        (1 << 10)
#define T2_PARTICLESPIN       (1 << 11)
#define T2_DOFADE             (1 << 12)
#define T2_ALIGNSTRETCH       (1 << 13)
#define T2_ONEFRAMEONLY       (1 << 14)
#define T2_CLAMP_VEL          (1 << 15)
#define T2_CLAMP_VEL_AXIS     (1 << 16)
#define T2_CONE               (1 << 17)
#define T2_RADIALVELOCITY     (1 << 18)
#define T2_FRICTION           (1 << 19)
#define T2_VARYCOLOR          (1 << 20)
#define T2_RELATIVEANGLES     (1 << 22)
#define T2_NOTAGAXIS          (1 << 23)
// gate for the port-only forward arm in SpawnTempModel, which is now retail's randDir


class spawnthing_t;
class spawn_effectmap;
class ctempmodel_t;

typedef enum {
    TBC_DIRECTION = 0,
    TBC_SPEED = 1,
    TBC_SPAWNRATE = 2,
    TBC_PARTICLESPIN = 3
} tbc_t;

#define CG_TBC_LITERAL_0     0.0f      
#define CG_TBC_LITERAL_1     1.0f      
#define CG_TBC_LITERAL_1000  1000.0f   
#define CG_TBC_LITERAL_10    10.0f     
#define CG_TBC_MS_SCALE      1000.0f   
#define CG_TBC_LITERAL_90    90.0f     
#define CG_TBC_LITERAL_270   270.0f    
#define CG_TBC_LITERAL_180   180.0f    
#define CG_TBC_LITERAL_360   360.0f    
#define CG_TBC_DEG_TO_RAD    (M_PI / 180.0)                       
#define CG_TBC_RAD_TO_DEG(x) ((float)(180.0f * (x) / M_PI))      


// size; the words a verb does not write stay untouched exactly as retail leaves them, and
#define TBC_TYPE_DIRECTION    0
#define TBC_TYPE_SPEED        1
#define TBC_TYPE_SPAWNRATE    2
#define TBC_TYPE_PARTICLESPIN 3

class TBC_DataPoint
{
public:
    float value;                                          
    int   type;                                           
    float time;                                           
    float axis[3];                                        
    int   useBias;                                        
    int   dirType[3];                                     
    float dirBase[3];                                     
    float dirAmp[3];                                      
};

int CompareData(const void *elem1, const void *elem2);

class TimeBasedCommand;
class TimeBasedCommandIndex;

class TimeBasedCommandIndex : public Class
{
public:
    ~TimeBasedCommandIndex();

    float m_key;       
    int   m_index;     
};

class TimeBasedCommand : public Class
{
public:
    ~TimeBasedCommand();

    void Init(void);                                        

    float   m_startValue;                                  
    float   m_endValue;                                    
    float   m_currentValue;                                
    float   m_delta;                                       
    int     m_type;                                        
    qboolean m_interpolate;                                 

    virtual void SetControllerCommand(ctempmodel_t *ct);
    virtual void SetControllerCommand(spawnthing_t *sp);
    virtual void HandleCommand(ctempmodel_t *ct);
    virtual void HandleCommand(spawnthing_t *sp);
    virtual TimeBasedCommand *clone(void) = 0;
};

class TBC_Speed : public TimeBasedCommand
{
public:
    ~TBC_Speed();
    void Init(void);                                       
    TimeBasedCommand *clone(void);                          
    void HandleCommand(ctempmodel_t *ct);                   
    void SetControllerCommand(ctempmodel_t *ct);            

    float m_startTime;                                     
    float m_endTime;                                       
};

class TBC_SpawnRate : public TimeBasedCommand
{
public:
    ~TBC_SpawnRate();
    void Init(void);
    TimeBasedCommand *clone(void);                          
    void HandleCommand(spawnthing_t *sp);                   
    void SetControllerCommand(spawnthing_t *sp);            

    float m_startTime;                                     
    float m_endTime;                                       
};

class TBC_ParticleSpin : public TimeBasedCommand
{
public:
    ~TBC_ParticleSpin();
    void Init(void);
    TimeBasedCommand *clone(void);                          
    void HandleCommand(ctempmodel_t *ct);                   
    void SetControllerCommand(ctempmodel_t *ct);            

    float m_spin[3];                                       
    int   m_useBias;                                       
    float m_startTime;                                     

};

class TBC_Direction : public TimeBasedCommand
{
public:
    ~TBC_Direction();
    void Init(void);                                       
    TimeBasedCommand *clone(void);                          
    void HandleCommand(ctempmodel_t *ct);                   
    void SetControllerCommand(ctempmodel_t *ct);            

    int   m_dirType[3];                                    
    float m_dirBase[3];                                    
    float m_dirAmp[3];                                     
    float m_curDir[3];                                     
    int   m_endDirType[3];                                 
    float m_endDirBase[3];                                 
    float m_endDirAmp[3];                                  
    float m_endCurDir[3];                                  
};

class TimeBasedCommandManager : public Class
{
public:
    TimeBasedCommandManager(void);
    ~TimeBasedCommandManager(void);

    void GenerateCommands(Container<TBC_DataPoint *>& list, tbc_t type);
    void SortParticleTBIList(void);
    void SortEmitterTBIList(void);
    void SortAllTBILists(void);
    void SetNextParticleCommand(ctempmodel_t *ct);
    void SetNextEmitterCommand(spawnthing_t *sp);
    void AddEmitterTBC(TimeBasedCommand *cmd);
    void GenerateEmitterTBI(float key, int index);
    void GenerateParticleTBI(float key, int index);
    void ClearEmitterTBCList(void);
    void ClearEmitterTBIndexList(void);
    void ClearParticleTBCList(void);
    void ClearParticleTBIndexList(void);
    int  GetNumberOfParticleCommands(void);
    int  GetNumberOfEmitterCommands(void);
    float GetParticleFirstStartTime(void);

    Container<TBC_DataPoint *>        m_directionList;     
    Container<TBC_DataPoint *>        m_speedList;         
    Container<TBC_DataPoint *>        m_particleSpinList;  
    Container<TBC_DataPoint *>        m_spawnRateList;     

    Container<TimeBasedCommand *>       m_particleCommands;  
    Container<TimeBasedCommandIndex *>  m_particleIndex;     
    Container<TimeBasedCommand *>       m_emitterCommands;   
    Container<TimeBasedCommandIndex *>  m_emitterIndex;      
};

class TimeBasedCommandParticleController : public Class
{
public:
    TimeBasedCommandParticleController(void);
    ~TimeBasedCommandParticleController();

    void Update(ctempmodel_t *ct);

    // +8 is NOT the manager.  Retail's four SetControllerCommand bodies end with a zero-arg
    TimeBasedCommand *m_command;                           
};

class TimeBasedCommandEmitterController : public Class
{
public:
    TimeBasedCommandEmitterController(void);
    ~TimeBasedCommandEmitterController();

    void Update(spawnthing_t *sp);

    TimeBasedCommand *m_command;                           
};

// RANDOM upward must switch to the raw retail pair above.
typedef enum {
    NOT_RANDOM = 0,   
    RANDOM = 1,       
    CRANDOM = 2,      
    RANDOMRANGE = 3,  
    CRANDOMRANGE = 4, 
    GRANDOM = 5,      
    ERANDOM = 6       
} randtype_t;

float CGResolveRandomNumber(randtype_t type, float base, float amplitude);

class Trajectory
{
public:
    Trajectory(const Vector& launchPoint, const Vector& targetPoint, const float initialSpeed, const float gravity)
    {
        _launchPoint     = launchPoint;
        _targetPoint     = targetPoint;
        _initialVelocity = Vector(0.0f, 0.0f, 0.0f);
        _gravity         = gravity;

        const Vector direction(targetPoint - launchPoint);
        Vector       directionXY(direction);
        directionXY.z = 0.0f;
        directionXY.normalize();

        const float X  = direction.lengthXY();
        const float Y  = direction.z;
        const float R  = initialSpeed;
        const float a  = gravity;
        const float X2 = X * X;
        const float Y2 = Y * Y;
        const float R2 = R * R;
        const float R4 = R2 * R2;
        const float Ya = Y * a;

        const float denominator = 2.0f * (1.0f + Y2 / X2);
        const float radical     = R4 + 2.0f * R2 * Ya - X2 * a * a;

        if (radical < 0.0f) {
            return;
        }

        const float Vx2 = (R2 + Ya + sqrt(radical)) / denominator;

        if (Vx2 < 0.0f) {
            return;
        }

        const float Vx  = sqrt(Vx2);
        float       Vy2 = R2 - Vx * Vx;

        if (Vy2 < 0.0f) {
            Vy2 = 0.0f;
        }

        const float Vy = sqrt(Vy2);

        _initialVelocity = (directionXY * Vx) + Vector(0.0f, 0.0f, Vy);

        _launchAngle = atan2(Vy, Vx);

        const float cosA = cos(_launchAngle);
        const float sinA = sin(_launchAngle);

        _travelTime = sqrt((2.0f * X * sinA) / (a * cosA) + (2.0f * Y) / a);
    }

    const Vector& GetLaunchPoint(void) const { return _launchPoint; }
    const Vector& GetTargetPoint(void) const { return _targetPoint; }
    float         GetLaunchAngle(void) const { return _launchAngle; }
    float         GetTravelTime(void) const { return _travelTime; }
    const Vector& GetInitialVelocity(void) const { return _initialVelocity; }
    float         GetGravity(void) const { return _gravity; }

private:
    Vector _launchPoint;
    Vector _targetPoint;
    // ⚠️ Deliberately NOT initialised: retail's ctor never writes +0x18/+0x1C, so a caller
    float  _launchAngle;
    float  _travelTime;
    Vector _initialVelocity;
    float  _gravity;
};

class cg_common_data : public Class
{
public:
    cg_common_data();

    int      life;
    int      createTime;
    Vector   origin;
    Vector   oldorigin;
    Vector   accel;
    // ⚠️ Retail's cgame NEVER READS IT.  spawnthing +0x464 has exactly two references in
    float    accelDelay;
    Vector   angles;
    Vector   velocity;
    Vector   avelocity;
    Vector   parentOrigin;
    Vector   parentLinkAngles;
    Vector   parentMins;
    Vector   parentMaxs;
    float    color[4];
    float    alpha;
    float    scaleRate;
    float    linearScaleRate;
    float    scalemin;
    float    scalemax;
    float    bouncefactor;
    int      bouncecount;
    int      maxbouncecount;
    str      bouncesound;
    int      bouncesound_delay;
    float    scaleDelay;
    int      flags;
    int      flags2;
    int      tiki;         
    int      swarmfreq;
    float    swarmmaxspeed;
    float    swarmdelta;
    float    lightIntensity;
    int      lightType;
    int      fadeintime;
    int      fadedelay;
    int      parent;
    int      collisionmask;
    int      min_twinkletimeoff;
    int      max_twinkletimeoff;
    int      min_twinkletimeon;
    int      max_twinkletimeon;
    int      lightstyle;
    int      physicsRate;
    float    scale;
    float    scale2;
    str      swipe_shader;
    str      swipe_tag_start;
    str      swipe_tag_end;
    str      shadername;
    float    swipe_life;
    float    friction;

    float decal_orientation;    
    float decal_radius;         


    // has no such second operator: BeginTagEmitter @0x00010110 never stores to +0x548, and the
    float    dragDelay;
    float    wavyDistance;
    float    drag;

    // ⚠️ The "is set" state does not ride a T2_ bit: retail's is (1 << 8), which MOHAA
    // spent on `volumetric` and retail's cgame never writes -- hence hasColorRange here.
    qboolean hasColorRange;
    byte     colorRangeMin[3];
    byte     colorRangeMax[3];
    int      colorRangeTimeMs;

    Vector   spinRate;
    Vector   spinBase;
    float    spinSpeed;

public:
};

inline cg_common_data::cg_common_data()
{
    int i;

    bouncesound_delay  = 0;
    life               = 0;
    createTime         = 0;
    alpha              = 0;
    fadedelay          = 0;
    lightIntensity     = 0;
    lightType          = (dlighttype_t)0;
    bouncefactor       = 0;
    bouncecount        = 0;
    maxbouncecount     = 0;
    scaleDelay         = 0;
    scaleRate          = 0;
    linearScaleRate    = 0;
    scale              = 1;
    scalemin           = 0;
    scalemax           = 0;
    swarmfreq          = 0;
    swarmmaxspeed      = 0;
    swarmdelta         = 0;
    flags              = 0;
    flags2             = 0;
    fadeintime         = 0;
    parent             = 0;
    tiki               = -1;
    collisionmask      = 0;
    min_twinkletimeoff = 0;
    max_twinkletimeoff = 0;
    min_twinkletimeon  = 0;
    max_twinkletimeon  = 0;
    lightstyle         = -1;
    physicsRate        = 10;
    accelDelay         = 0;

    // (+0x514 and +0x4E8..+0x4F0), so an emitter that never authors the command must not
    wavyDistance     = 0;
    hasColorRange    = qfalse;
    colorRangeTimeMs = 0;

    dragDelay        = 0;
    drag             = 0;

    for (i = 0; i < 3; i++) {
        colorRangeMin[i] = 0;
        colorRangeMax[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        color[i] = 0;
    }

    for (i = 0; i < 3; i++) {
        spinRate[i] = 0;
        spinBase[i] = 0;
    }
    spinSpeed = 0;
}

//                                      InitializeSpawnthing does NOT clear (+0x480..0x488
class ctempmodel_t : public Class
{
public:
    ctempmodel_t();

    class ctempmodel_t *next;
    class ctempmodel_t *prev;

    TimeBasedCommandParticleController *tbcController = NULL;
    float                               tbcNextCommandTime = 0;
    int                                 tbcParticleCursor = 0;

    void createTBC_Controller(void);
    void deleteTBC_Controller(void);

    cg_common_data cgd;
    str            modelname;

    refEntity_t lastEnt;
    refEntity_t ent;

    int           number;
    int           lastAnimTime;
    int           lastPhysicsTime;
    int           killTime;
    int           next_bouncesound_time;
    int           seed;
    int           twinkleTime;
    int           aliveTime;
    qboolean      addedOnce;
    qboolean      lastEntValid;
    qboolean      hide;           
    spawnthing_t *m_spawnthing;

    float wavyDir[3];

    void (*touchfcn)(ctempmodel_t *ct, trace_t *trace);

public:
};

inline ctempmodel_t::ctempmodel_t()
{
    number                = 0;
    lastPhysicsTime       = 0;
    lastAnimTime          = 0;
    killTime              = 0;
    next_bouncesound_time = 0;
    seed                  = 0;
    twinkleTime           = 0;
    aliveTime             = 0;
    addedOnce             = qfalse;
    lastEntValid          = qfalse;
    m_spawnthing          = NULL;
    wavyDir[0]            = 0;
    wavyDir[1]            = 0;
    wavyDir[2]            = 0;
}

#define LIFE_SWIPE       1
#define MAX_SWIPE_POINTS 64

struct swipepoint_t {
    vec3_t points[2];
    float  time;
};

class swipething_t : public Class
{
public:
    qboolean     enabled;
    str          tagname_start;
    str          tagname_end;
    int          entitynum;
    int          instancenum;
    float        startcolor[4];
    float        endcolor[4];
    swipepoint_t swipepoints[MAX_SWIPE_POINTS];
    swipepoint_t cntPoint;
    int          num_live_swipes;
    int          first_swipe;
    float        life;
    qhandle_t    shader;
    void         Init();
};

// is what carries retail parity, not the byte offsets.  Retail's field order is

inline void swipething_t::Init()
{
    int i;

    enabled       = qfalse;
    tagname_start = "";
    tagname_end   = "";
    entitynum     = -1;
    instancenum   = -1;

    for (i = 0; i < 4; i++) {
        startcolor[i] = 1.f;
        endcolor[i]   = 0.f;
    }

    for (i = 0; i < MAX_SWIPE_POINTS; i++) {
        VectorSet(swipepoints[i].points[0], 0.f, 0.f, 0.f);
        VectorSet(swipepoints[i].points[1], 0.f, 0.f, 0.f);
        swipepoints[i].time = 0.f;
    }

    num_live_swipes = 0;
    first_swipe     = 0;
}

class enttracker_t : public Class
{
public:
    enttracker_t();
    int AssignNumber(void);

protected:
    qboolean     usedNumbers[256];
    virtual void RemoveEntity(int entnum);

public:
};

inline enttracker_t::enttracker_t()
{
    memset(usedNumbers, 0, sizeof(usedNumbers));
}

inline void enttracker_t::RemoveEntity(int entnum)
{

    if (entnum >= MAGIC_UNUSED_NUMBER) {
        entnum -= MAGIC_UNUSED_NUMBER;

        assert(entnum >= 0);
        assert(entnum < 256);

        usedNumbers[entnum] = qfalse;
    }
}

inline int enttracker_t::AssignNumber(void)
{
    int i;


    for (i = 0; i < 256; i++) {
        if (!usedNumbers[i]) {
            usedNumbers[i] = qtrue;
            return MAGIC_UNUSED_NUMBER + i;
        }
    }

    return -1;
}

class emittertime_t : public Class
{
public:
    int      entity_number;
    int      instance_number;
    int      last_emit_time;
    Vector   oldorigin;
    qboolean active;
    qboolean lerp_emitter;

public:
};

class emitterthing_t : public enttracker_t
{
protected:
    Container<emittertime_t> m_emittertimes; 

public:
    emittertime_t *GetEmitTime(int entnum, int instanceNumber = -1);
    virtual void   RemoveEntity(int entnum) override;
    qboolean       startoff;

public:
};

inline void emitterthing_t::RemoveEntity(int entnum)
{
    int            num, count;
    emittertime_t *et;

    if (entnum == -1) {
        return;
    }

    count = m_emittertimes.NumObjects();

    for (num = count; num >= 1; num--) {
        et = &m_emittertimes.ObjectAt(num);
        if (et->entity_number == entnum) {
            m_emittertimes.RemoveObjectAt(num);
        }
    }

    enttracker_t::RemoveEntity(entnum);
}

inline emittertime_t *emitterthing_t::GetEmitTime(int entnum, int instanceNumber)
{
    int            num, count;
    emittertime_t *et;

    count = m_emittertimes.NumObjects();

    for (num = 1; num <= count; num++) {
        et = &m_emittertimes.ObjectAt(num);
        if (et->entity_number == entnum) {
            if (instanceNumber != -1 && et->instance_number != instanceNumber) {
                et->last_emit_time  = cg.time;
                et->lerp_emitter    = qfalse;
                et->instance_number = instanceNumber;
                et->active          = this->startoff ? qfalse : qtrue;
            }
            return et;
        }
    }

    et                  = &m_emittertimes.ObjectAt(m_emittertimes.AddObject({}));
    et->entity_number   = entnum;
    et->instance_number = instanceNumber;
    et->last_emit_time  = cg.time;
    et->lerp_emitter    = qfalse;

    if (this->startoff) {
        et->active = qfalse;
    } else {
        et->active = qtrue;
    }

    return et;
}

class commandtime_t : public Class
{
public:
    int entity_number;
    int command_number;
    int last_command_time;

public:
};

class commandthing_t : public enttracker_t
{
    Container<commandtime_t> m_commandtimes; 

public:
    commandtime_t *GetLastCommandTime(int entnum, int commandnum);
    virtual void   RemoveEntity(int entnum) override;

};

inline void commandthing_t::RemoveEntity(int entnum)
{
    int            num, count;
    commandtime_t *ct;

    count = m_commandtimes.NumObjects();

    for (num = count; num >= 1; num--) {
        ct = &m_commandtimes.ObjectAt(num);
        if (ct->entity_number == entnum) {
            m_commandtimes.RemoveObjectAt(num);
        }
    }

    enttracker_t::RemoveEntity(entnum);
}

inline commandtime_t *commandthing_t::GetLastCommandTime(int entnum, int commandnum)
{
    int num, count;

    count = m_commandtimes.NumObjects();

    for (num = 1; num <= count; num++) {
        commandtime_t *ct = &m_commandtimes.ObjectAt(num);
        if ((ct->entity_number == entnum) && (ct->command_number == commandnum)) {
            return ct;
        }
    }

    commandtime_t ct;
    ct.entity_number     = entnum;
    ct.command_number    = commandnum;
    ct.last_command_time = 0;

    m_commandtimes.AddObject(ct);

    return &m_commandtimes.ObjectAt(m_commandtimes.NumObjects());
}

class spawnthing_t : public emitterthing_t
{
public:
    ~spawnthing_t(void);         

    Container<str> m_modellist; 
    Container<str> m_taglist;   

    cg_common_data cgd;
    int            entnum;




    qboolean alwaysCreateBeam = qfalse;

    qboolean canEndEarly = qfalse;

    qboolean hasLensFlare = qfalse;
    int      lensflareNum = -1;

    Vector   origin_offset_base;
    Vector   origin_offset_amplitude;
    Vector   axis_offset_base;
    Vector   axis_offset_amplitude;
    Vector   randvel_base;
    Vector   randvel_amplitude;
    Vector   avelocity_base;
    Vector   avelocity_amplitude;
    Vector   angles_amplitude;
    vec3_t   axis[3];
    vec3_t   aim_axis[3];
    vec3_t   tag_axis[3];

    randtype_t randvel_rand[3]     = {NOT_RANDOM, NOT_RANDOM, NOT_RANDOM}; 
    randtype_t offset_rand[3]      = {NOT_RANDOM, NOT_RANDOM, NOT_RANDOM}; 
    randtype_t avelocity_rand[3]   = {NOT_RANDOM, NOT_RANDOM, NOT_RANDOM}; 
    randtype_t angles_rand[3]      = {NOT_RANDOM, NOT_RANDOM, NOT_RANDOM}; 
    randtype_t axis_offset_rand[3] = {NOT_RANDOM, NOT_RANDOM, NOT_RANDOM}; 

    Vector     randDirBase = Vector(0, 0, 0);      
    Vector     randDirAmp = Vector(0, 0, 0);       
    randtype_t randDirType[3] = {NOT_RANDOM, NOT_RANDOM, NOT_RANDOM}; 

    randtype_t life_rand = NOT_RANDOM;

    float    life_random;
    float    forwardVelocity;
    float    sphereRadius;
    float    coneHeight;
    float    spawnRate;
    int      lastTime;
    int      count;
    int      duplicateCount;
    int      tagnum;
    str      tagname;
    str      emittername;
    str      animName;
    float    dcolor[3];
    qboolean dlight;
    float    linked_origin[3];
    float    linked_axis[3][3];
    float    fMinRangeSquared;
    float    fMaxRangeSquared;

    str      startTag;
    str      endTag;
    float    length;
    float    min_offset;
    float    max_offset;
    float    overlap;
    float    numSubdivisions;
    float    delay;
    float    toggledelay;
    int      beamflags;
    int      numspherebeams;
    float    endalpha;
    float    wavespeed;
    float    spreadx;
    float    spready;
    // ⚠️ Deliberately NOT reset by InitializeSpawnthing: retail's sub_30027610 clears
    float    endspreadx      = 0;
    float    endspready      = 0;
    float    spreadtimedelta = 0;
    int      spreadstarttime = 0;
    qboolean use_last_trace_end;
    qboolean useLastTrace;

    int      traceCount;

    float    windEffect = 0;

    float    chanceToEmitAtCamera = 0;

    qboolean         hitEntities;
    qboolean         hitWorld;

    TimeBasedCommandManager      *m_tbcManager = NULL;
    TimeBasedCommandEmitterController *tbcController = NULL;

    void createTBC_Controller(void);
    float                         tbcNextCommandTime = 0;
    int                           tbcEmitterCursor = 0;
    float                         tbcTimeOffset = 0;
    spawn_effectmap *m_armedEffectMap = NULL;

    qboolean traceImpactSpawn = qfalse;

    // never draws it.  358 shipped .tik carry the verb (mostly as an animation
    qboolean hide;

    void (*touchfcn)(ctempmodel_t *ct, trace_t *trace);
    str  GetModel(void);
    void SetModel(str model);

public:
};

inline void spawnthing_t::SetModel(str model)
{
    m_modellist.ClearObjectList();
    m_modellist.AddObject(model);
}

inline str spawnthing_t::GetModel(void)
{
    int num, index;

    num = m_modellist.NumObjects();

    if (!num) {
        return "";
    }

    index = (num * random()) + 1;

    if (index > num) {
        index = num;
    }

    return m_modellist.ObjectAt(index);
}

class beamthing_t : public emitterthing_t
{
public:
    str      beamname;
    str      shadername;
    str      startTag;
    str      endTag;
    int      numSubdivisions;
    int      tiki;         
    float    alpha;
    float    scale;
    int      flags;
    float    length;
    int      life;
    float    min_offset;
    float    max_offset;
    float    overlap;
    int      delay;
    byte     modulate[4];
};

// Retail's tempmodel pool is 1024, not MOHAA's 2048: InitializeTempModels stores 1024 into
#define MAX_TEMPMODELS 1024
#define MAX_BEAMS      4096

// bit 0x2000000; a token the engine does not know maps to 0, i.e. it matches nothing.
#define EF2_SURFACETYPE_DEFAULT 0x2000000  

class spawn_effect : public Class
{
public:
    str          name;            
    str          surfacetypeName; 
    int          surfacetype;     
    int          effectId;        
    spawnthing_t st;              

    spawn_effect() { surfacetype = EF2_SURFACETYPE_DEFAULT; effectId = 0; }
};

class sound_effect : public Class
{
public:
    str name;            
    str surfacetypeName; 
    int surfacetype;     
    int effectId;         
    str soundname;       

    sound_effect() { surfacetype = EF2_SURFACETYPE_DEFAULT; effectId = 1; }
};

class spawn_effectmap : public Class
{
public:
    str                       name;
    Container<spawn_effect *> m_spawneffects;
    Container<sound_effect *> m_soundeffects;
    spawn_effect             *m_defaultSpawn;
    sound_effect             *m_defaultSound;

    spawn_effectmap()
    {
        m_defaultSpawn = NULL;
        m_defaultSound = NULL;
    }
};

class ClientGameCommandManager : public Listener
{
private:
    spawnthing_t              m_localemitter; 
    ctempmodel_t              m_active_tempmodels;
    ctempmodel_t             *m_free_tempmodels;
    ctempmodel_t              m_tempmodels[MAX_TEMPMODELS];
    spawnthing_t             *m_spawnthing;
    Container<spawnthing_t *> m_emitters; 
    Container<spawn_effectmap *> m_effectMaps;         
    spawn_effectmap             *m_currentEffectMap;   
    int                       m_seed;
    commandthing_t            m_command_time_manager; 

    void (ClientGameCommandManager::*endblockfcn)(void);
    void          Print(Event *ev);
    void          StartBlock(Event *ev);
    void          EndBlock(Event *ev);
    void          EmitterStartOff(Event *ev);
    void          SetAlpha(Event *ev);
    void          SetDieTouch(Event *ev);
    void          SetBounceFactor(Event *ev);
    void          SetBounceSound(Event *ev);
    void          SetBounceSoundOnce(Event *ev);
    void          SetModel(Event *ev);
    void          SetLife(Event *ev);
    void          SetColor(Event *ev);
    void          SetColorRange(Event *ev);
    void          SetLightstyle(Event *ev);
    void          SetVelocity(Event *ev);
    void          SetAngularVelocity(Event *ev);
    void          SetCount(Event *ev);
    void          SetScale(Event *ev);
    void          SetScaleUpDown(Event *ev);
    void          SetScaleMin(Event *ev);
    void          SetScaleMax(Event *ev);
    void          SetScaleRate(Event *ev);
    void          SetLinearScaleRate(Event *ev);
    void          SetRandomVelocity(Event *ev);
    void          SetRandomVelocityAlongAxis(Event *ev);
    void          SetAccel(Event *ev);
    void          SetAccel(const Vector &accel, float delay);
    void          SetDrag(Event *ev);
    void          SetFade(Event *ev);
    void          SetDoFade(Event *ev);
    void          SetFadeDelay(Event *ev);
    void          SetSpawnRate(Event *ev);
    void          SetOriginOffset(Event *ev);
    void          SetOffsetAlongAxis(Event *ev);
    void          SetCircle(Event *ev);
    void          SetSphere(Event *ev);
    void          SetInwardSphere(Event *ev);
    void          SetRandomRoll(Event *ev);
    void          SetSwarm(Event *ev);
    void          Hide(Event *ev);
    void          TikiNote(Event *ev);
    void          TikiTodo(Event *ev);
    void          SetWavy(Event *ev);
    void          SetAlign(Event *ev);
    void          SetAlignOnce(Event *ev);
    void          SetCollision(Event *ev);
    void          SetFlickerAlpha(Event *ev);
    void          SetFadeIn(Event *ev);
    void          SetEntityColor(Event *ev);
    void          SetGlobalFade(Event *ev);
    void          SetRadius(Event *ev);
    void          SetParentLink(Event *ev);
    void          SetHardLink(Event *ev);
    void          SetAngles(Event *ev);
    void          ParentAngles(Event *ev);
    void          SetTwinkle(Event *ev);
    void          SetTrail(Event *ev);
    void          SetPhysicsRate(Event *ev);
    void          SetBounceDecal(Event *ev);
    void          UpdateSwarm(ctempmodel_t *p);
    void          BeginOriginSpawn(Event *ev);
    void          EndOriginSpawn(void);
    void          BeginOriginBeamSpawn(Event *ev);
    void          EndOriginBeamSpawn(void);
    void          BeginOriginBeamEmitter(Event *ev);
    void          EndOriginBeamEmitter(void);
    void          BeginTagSpawn(Event *ev);
    void          EndTagSpawn(void);
    void          BeginTagBeamSpawn(Event *ev);
    void          EndTagBeamSpawn(void);
    void          EndTagTraceImpactSpawn(void);
    void          BeginTagEmitter(Event *ev);
    void          EndTagEmitter(void);
    void          BeginOriginEmitter(Event *ev);
    void          EndOriginEmitter(void);
    void          LoadExternalEmitter(Event *ev, int bTagEmitter);
    void          BeginTagBeamEmitter(Event *ev);
    void          EndTagBeamEmitter(void);
    void          EmitterOn(Event *ev);
    void          EmitterOff(Event *ev);
    void          Sound(Event *ev);
    void          StopSound(Event *ev);
    void          LoopSound(Event *ev);
    void          Cache(Event *ev);
    void          AliasCache(Event *ev);
    void          Alias(Event *ev);
    void          Client(Event *ev);
    void          TagDynamicLight(Event *ev);
    void          OriginDynamicLight(Event *ev);
    void          DynamicLight(Event *ev);
    void          LensFlare(Event *ev);
    void          GetOrientation(int tagnum, spawnthing_t *sp);
    void          GetOrientation(const str& tagname, spawnthing_t *sp);
    Vector        GetViewEndPoint(int entnum, Vector org, qboolean useEndpoint);
    void          Swipe(Event *ev);
    void          SwipeOn(Event *ev);
    void          SwipeOff(Event *ev);
    void          AnimateOnce(Event *ev);
    void          SetAnim(Event *ev);
    void          SetDecalRadius(Event *ev);
    void          SetDecalOrientation(Event *ev);
    void          TagList(Event *ev);
    void          SetParallel(Event *ev);
    void          Footstep(Event *ev);
    void          EffectMap(Event *ev);
    void          AddSpawnEffect(Event *ev);
    void          AddSoundEffect(Event *ev);
    void          DefineSpawnEffect(Event *ev);
    void          DefineSoundEffect(Event *ev);
    void          SpawnEffect(Event *ev);
    void          HitEntities(Event *ev);
    void          HitWorld(Event *ev);
    void          SetDetail(Event *ev);
    void          SetWindEffect(Event *ev);
    void          SetEmitterSpin(Event *ev);
    void          SetParticleSpin(Event *ev);
    void          SetDuplicateCount(Event *ev);
    void          SetTraceCount(Event *ev);
    void          SetRandomDirection(Event *ev);
    // and TBC_SetDirection @0x00023430.  These do not write a spawnthing field; they append
    void          TBC_SetSpeed(Event *ev);
    void          TBC_SetSpawnRate(Event *ev);
    void          TBC_SetParticleSpin(Event *ev);
    void          TBC_SetDirection(Event *ev);
    void          SetItemRingShader(Event *ev);           
    void          SetExclusiveLightName(Event *ev);       
    ctempmodel_t *AllocateTempModel(void);
    qboolean      TempModelPhysics(ctempmodel_t *p, float ftime, float ftimeSquared, float scale);
    qboolean      TempModelRealtimeEffects(ctempmodel_t *p, float ftime, float ftimeSquared, float scale);
    qboolean      LerpTempModel(refEntity_t *newEnt, ctempmodel_t *p, float frac);
    void          SpawnEffect(int count, int timealive);
    void          SpawnTempModel(int count, int timealive);
    void          FreeTempModel(ctempmodel_t *le);
    void          AnimateTempModel(ctempmodel_t *ent, Vector origin, refEntity_t *newEnt);
    void          OtherTempModelEffects(ctempmodel_t *p, Vector origin, refEntity_t *newEnt);
    void          SetBaseAndAmplitude(Event *ev, Vector& base, Vector& amplitude);

    void SetSubdivisions(Event *ev);
    void SetMinOffset(Event *ev);
    void SetMaxOffset(Event *ev);
    void SetShader(Event *ev);
    void SetLength(Event *ev);
    void SetBeamDelay(Event *ev);
    void SetBeamToggleDelay(Event *ev);
    void SetBeamPersist(Event *ev);
    void SetBeamOffsetEndpoints(Event *ev);
    void SetBeamSphere(Event *ev);
    void SetSpread(Event *ev);
    void SetUseLastTraceEnd(Event *ev);
    void SetUseLastTrace(Event *ev);
    void SetEndAlpha(Event *ev);
    void RandomChance(Event *ev);
    void CommandDelay(Event *ev);

    void SetSpinRate(Event *ev);
    void EmitterSpin(Event *ev);
    void ParticleSpin(Event *ev);
    void TBCSpeed(Event *ev);
    void TBCSpawnRate(Event *ev);
    void TBCParticleSpin(Event *ev);
    void TBCDirection(Event *ev);
    void SetDirection(Event *ev);
    void SetScaleDelay(Event *ev);
    void SetOneFrameOnly(Event *ev);
    void SetBeamWave(Event *ev);
    void SetFullBeamWave(Event *ev);
    void SetAlwaysCreate(Event *ev);
    void SetCanEndEarly(Event *ev);
    void SetChanceToEmitAtCamera(Event *ev);
    void SetFreq(Event *ev);
    void FootstepSound(Event *ev);
    void SurfaceSound(Event *ev);
    void TagTraceImpactSpawn(Event *ev);
    void TagTraceImpactSound(Event *ev);
    void BeginTagTraceImpactMark(Event *ev);
    void EndTagTraceImpactMark(void);
    void GetSpawnEffect(trace_t& trace);
    void GetSoundEffect(const str& sound, trace_t& trace, str& out);

public:
    CLASS_PROTOTYPE(ClientGameCommandManager);

    ClientGameCommandManager();
    void GetOrientation(const str& tagname, orientation_t *ori);
    void AddTempModels(void);
    void emitterOn(const str& emittername);
    void emitterOff(const str& emittername);
    void UpdateEmitter(
        int           tiki,
        vec3_t        axis[3],
        int           entity_number,
        int           instance_number,
        int           parent_link_number,
        const Vector &entity_origin
    );
    void UpdateBeam(int tiki, int entity_number, int instance_number, spawnthing_t *beamthing);
    void PlaySound(
        const str   &sound_name,
        const vec3_t origin       = NULL,
        int          channel      = CHAN_AUTO,
        float        volume       = -1,
        float        min_distance = -1,
        float        pitch        = -1
    );

    spawnthing_t *InitializeSpawnthing(spawnthing_t *ep);
    void          SpawnEffect(int count, spawnthing_t *sp);
    void          SpawnEffectMapOnImpact(spawnthing_t *impactST, trace_t *trace);
    void          FreeAllTempModels(void);

    void          FlushCommandManager(void);

    void InitializeTempModels(void);
    void InitializeTempModelCvars(void);
    void InitializeEmitters(void);
    void RestartEmitterClocks(int time);
    void RemoveClientEntity(int number, int tiki, centity_t *cent, ctempmodel_t *p = NULL);
    void ClearSwipes(void);
    void FreeSpawnthing(spawnthing_t *sp);
    void ResetTempModels(void);
    void SpawnTempModel(int count, spawnthing_t *sp);
    void SpawnTempModel(int count, int timealive, spawnthing_t *sp);

    inline void SetSpawnthing(spawnthing_t *st) { m_spawnthing = st; };

    spawnthing_t *CreateNewEmitter(str emittername);
    spawnthing_t *CreateNewEmitter(void);
    spawnthing_t *GetEmitterByName(str emittername);
    spawnthing_t *CopySpawnthing(spawnthing_t *pDest, spawnthing_t& pSrc);
    void SwitchEmitter(int num, int setcurnum);
    void TestEmitter(int reset);
    void DumpEmitter(void);
    void          DeleteEmitters(int tiki);
    void          CGEvent(centity_t *cent);

    int           IdForTempModel(const ctempmodel_t *model);
    ctempmodel_t *TempModelForId(int id);
    int           IdForSpawnThing(const spawnthing_t *sp);
    spawnthing_t *SpawnThingForId(int id);

};

class EmitterLoader : public Listener
{
private:
    bool emitterActive;

public:
    CLASS_PROTOTYPE(EmitterLoader);

    EmitterLoader();
    bool Load(Script&);
    void ProcessEmitter(Script&);
    void Emitter(Event *ev);
};


extern ClientGameCommandManager commandManager;
