

#pragma once

#include "g_local.h"
#include "../corepp/vector.h"

class Entity;

typedef enum {
    STEPMOVE_OK,
    STEPMOVE_BLOCKED_BY_ENTITY,
    STEPMOVE_BLOCKED_BY_WORLD,
    STEPMOVE_BLOCKED_BY_WATER,
    STEPMOVE_BLOCKED_BY_FALL,
    STEPMOVE_BLOCKED_BY_DOOR,
    STEPMOVE_STUCK
} stepmoveresult_t;

typedef enum {
    MOVETYPE_NONE,       
    MOVETYPE_STATIONARY, 
    MOVETYPE_NOCLIP,     
    MOVETYPE_PUSH,       
    MOVETYPE_STOP,       
    MOVETYPE_WALK,       
    MOVETYPE_STEP,       
    MOVETYPE_FLY,
    MOVETYPE_TOSS,       
    MOVETYPE_FLYMISSILE, 
    MOVETYPE_BOUNCE,
    MOVETYPE_SLIDE,
    MOVETYPE_ROPE,
    MOVETYPE_GIB,
    MOVETYPE_VEHICLE,
} movetype_t;

typedef struct {
    Entity *ent;
    Vector  _localOrigin;
    Vector  origin;
    Vector  localangles;
    Vector  angles;
    float   deltayaw;
} pushed_t;

extern pushed_t  pushed[];
extern pushed_t *pushed_p;

void     G_RunEntity(Entity *ent);
void     G_Impact(Entity *e1, const trace_t *trace);
qboolean G_PushMove(Entity *pusher, const Vector &move, const Vector &amove);
void     G_CheckWater(Entity *ent);

void R133_KB_Arm(int entnum);
Entity  *G_TestEntityPosition(Entity *ent, Vector vOrg);

void G_EntTrash_Pointer(const char *where, Entity *player, void *pheur);
void G_EntTrash_KilledEntry(Entity *victim, Entity *attacker, float damage);
void G_EntTrash_Tick(void);

class Angle
{
public:
    Angle(const float value)
        : _value(value)
    {}

    operator float() { return _value; }

    operator float const() const { return _value; }

    float GetValue(void) const { return _value; }

    float& GetValue(void) { return _value; }

private:
    float _value;
};

class Trajectory
{
public:
    Trajectory(const Vector& launchPoint, const Vector& targetPoint, const Angle launchAngle, const float gravity);
    Trajectory(
        const Vector& launchPoint,
        const Vector& targetPoint,
        const float   initialSpeed,
        const float   gravity,
        const bool    useHighTrajectory = false
    );

    const Vector& GetLaunchPoint(void) const { return _launchPoint; }

    const Vector& GetTargetPoint(void) const { return _targetPoint; }

    float GetLaunchAngle(void) const { return _launchAngle; }

    float GetTravelTime(void) const { return _travelTime; }

    const Vector& GetInitialVelocity(void) const { return _initialVelocity; }

    float GetGravity(void) const { return _gravity; }

private:
    Vector _launchPoint;
    Vector _targetPoint;
    float  _launchAngle;
    float  _travelTime;
    Vector _initialVelocity;
    float  _gravity;
};
