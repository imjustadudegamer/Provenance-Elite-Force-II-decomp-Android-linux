
#ifndef __ACTOR_SENSORYPERCEPTION_H__
#define __ACTOR_SENSORYPERCEPTION_H__


class Actor;
class Entity;
class Archiver;

#define EF2_DEFAULT_FOV              150.0f
#define EF2_DEFAULT_VISION_DISTANCE  1536.0f

// a virtual destructor retail does not have.  Same treatment as CombatSubsystem.
class SensoryPerception final
{
public:
    SensoryPerception();
    SensoryPerception(Actor *actor);
    ~SensoryPerception();

    void     SenseEnemies();
    qboolean _SenseEntity(Entity *ent);

    void     Stimuli(int stimuli);
    void     Stimuli(int stimuli, Entity *ent);
    void     Stimuli(int stimuli, const Vector& position);
    void     Stimuli(int stimuli, const Vector& position, int strength);
    void     RespondTo(const str& stimuli_name, qboolean respond);
    void     RespondTo(int stimuli, qboolean respond);
    void     PermanentlyRespondTo(const str& stimuli_name, qboolean respond);
    qboolean ShouldRespondToStimuli(int new_stimuli);

    qboolean WithinVisionDistance(const Entity *ent);
    qboolean WithinVisionDistance(const Vector& position);
    qboolean InFOV(const Vector& pos, float check_fov, float check_fovdot);
    qboolean InFOV(const Vector& pos);
    qboolean InFOV(const Entity *ent);

    qboolean CanSeeEntity(Entity *start, const Entity *target, qboolean useFOV, qboolean useVisionDistance);
    qboolean CanSeeEntity(const Vector &start, const Entity *target, qboolean useFOV, qboolean useVisionDistance);

    qboolean CanSeeEntityComplex(Entity *start, Entity *target, qboolean useFOV, qboolean useVisionDistance);
    qboolean CanSeeEntityComplex(Vector& start, Entity *target, qboolean useFOV, qboolean useVisionDistance);

    qboolean CanSeePosition(const Vector& start, const Vector& position, qboolean useFOV, qboolean useVisionDistance);

    qboolean isInLineOfSight(const Vector& position, const int entNum);
    qboolean checkInLineOfSight(const Vector& position, const int entNum);

    qboolean _CanSeeComplex(Vector& start, Entity *target, qboolean useFOV, qboolean useVisionDistance);

    void ShowInfo();

    void   SetNoisePosition(const Vector& pos) { _noise_position = pos; }
    Vector GetNoisePosition();

    void SetLastSoundType(int soundtype);
    int  GetLastSoundType();

    void  SetNoiseTime(float noisetime) { _noise_time = noisetime; }
    float GetNoiseTime() { return _noise_time; }

    // ⚠️ retail does NOT touch _fovdot here -- see the header comment.
    void  SetFOV(float fov);
    float GetFOV() { return _fov; }

    void  SetFOVdot(float fov_dot) { _fovdot = fov_dot; }
    float GetFOVdot() { return _fovdot; }

    void  SetVisionDistance(float vision_distance);
    float GetVisionDistance();

    virtual void Archive(Archiver& arc);
    void         DoArchive(Archiver& arc, Actor *actor);

private:
    void _init();

private:
    int _stimuli;
    int _permanent_stimuli;

    Vector _noise_position;
    int    _last_soundType;
    float  _noise_time;

    float _fov;
    float _fovdot;
    float _vision_distance;

    float _nextSenseTime;

    LineOfSight_t _lineOfSight;

    Actor *act;
};

#endif 
