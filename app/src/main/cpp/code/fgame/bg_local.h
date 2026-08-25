
#pragma once

#define MIN_WALK_NORMAL 0.65f 

#define JUMP_VELOCITY   270

#define TIMER_LAND      130
#define TIMER_GESTURE   (34 * 66 + 50)

#define OVERCLIP        1.001f

#define HEAD_TAG        0
#define TORSO_TAG       1
#define L_ARM_TAG       2
#define R_ARM_TAG       3
#define MOUTH_TAG       4

typedef struct {
    vec3_t forward, left, up;
    vec3_t flat_forward, flat_left, flat_up;
    float  frametime;

    int msec;

    qboolean walking;
    qboolean groundPlane;
    trace_t  groundTrace;

    float impactSpeed;

    vec3_t previous_origin;
    vec3_t previous_velocity;
    int    previous_waterlevel;
} pml_t;

extern pmove_t *pm;
extern pml_t    pml;

extern float pm_stopspeed;
extern float pm_swimScale;
extern float pm_wadeScale;

extern float pm_accelerate;
extern float pm_airaccelerate;
extern float pm_wateraccelerate;

extern float pm_friction;
extern float pm_waterfriction;
extern float pm_slipperyfriction;

extern const vec3_t MINS;
extern const vec3_t MAXS;

extern int c_pmove;


qboolean PM_SlideMove(qboolean gravity);
void     PM_StepSlideMove(qboolean gravity);
