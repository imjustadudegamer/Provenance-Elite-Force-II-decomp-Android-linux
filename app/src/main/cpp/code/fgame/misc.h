

#pragma once

#include "g_local.h"
#include "entity.h"
#include "mover.h"
#include "animate.h"

class InfoNull : public Listener
{
public:
    CLASS_PROTOTYPE(InfoNull);

    InfoNull();
};

class FuncRemove : public Entity
{
public:
    CLASS_PROTOTYPE(FuncRemove);

    FuncRemove();
};

class MiscModel : public Entity
{
public:
    CLASS_PROTOTYPE(MiscModel);

    MiscModel();
};

class InfoNotNull : public Entity
{
public:
    CLASS_PROTOTYPE(InfoNotNull);
};

class ExplodingWall : public Trigger
{
protected:
    int      dmg;
    int      explosions;
    float    attack_finished;
    Vector   land_angles;
    float    land_radius;
    float    angle_speed;
    int      state;
    Vector   base_velocity;
    Vector   random_velocity;
    Vector   orig_mins, orig_maxs;
    qboolean on_ground;

public:
    CLASS_PROTOTYPE(ExplodingWall);

    ExplodingWall();
    void Setup(Event *ev);
    void AngleSpeed(Event *ev);
    void LandRadius(Event *ev);
    void LandAngles(Event *ev);
    void BaseVelocity(Event *ev);
    void RandomVelocity(Event *ev);
    void SetDmg(Event *ev);
    void setDamage(Event *ev);
    void SetExplosions(Event *ev);
    void SetupSecondStage(void);
    void Explode(Event *ev);
    void DamageEvent(Event *ev) override;
    void GroundDamage(Event *ev);
    void TouchFunc(Event *ev);
    void StopRotating(Event *ev);
    void CheckOnGround(Event *ev);
    void Archive(Archiver& arc) override;
};

inline void ExplodingWall::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveInteger(&dmg);
    arc.ArchiveInteger(&explosions);
    arc.ArchiveFloat(&attack_finished);
    arc.ArchiveVector(&land_angles);
    arc.ArchiveFloat(&land_radius);
    arc.ArchiveFloat(&angle_speed);
    arc.ArchiveInteger(&state);
    arc.ArchiveVector(&base_velocity);
    arc.ArchiveVector(&random_velocity);
    arc.ArchiveVector(&orig_mins);
    arc.ArchiveVector(&orig_maxs);
    arc.ArchiveBoolean(&on_ground);
}

class Teleporter : public Trigger
{
public:
    str               teleport_thread;
    qboolean          in_use;

    CLASS_PROTOTYPE(Teleporter);

    Teleporter();
    virtual void StartTeleport(Event *ev);
    virtual void Teleport(Event *ev);
    virtual void StopTeleport(Event *ev);
    void         SetThread(Event *ev);
    void         Archive(Archiver& arc) override;






public:
    bool doFullTeleport( Entity *entity ); 
};

inline void Teleporter::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveString(&teleport_thread);
    arc.ArchiveBoolean(&in_use);
}

class TeleporterDestination : public Entity
{
public:
    Vector movedir;

    CLASS_PROTOTYPE(TeleporterDestination);

    TeleporterDestination();
    void SetMoveDir(Event *ev);
    void Archive(Archiver& arc) override;
};

inline void TeleporterDestination::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveVector(&movedir);
}

class UseAnim : public Entity
{
public:
    int      count;
    qboolean active;
    str               thread_name;
    str               triggertarget;
    int               num_loops;
    str               state;
    str               camera;
    str               anim;
    str               key;
    float             delay;
    float             last_active_time;
    str               action_type;

    CLASS_PROTOTYPE(UseAnim);

    UseAnim();
    virtual void Touched(Event *ev);
    void         Reset(Event *ev);
    void         SetTriggerTarget(Event *ev);
    void         SetCount(Event *ev);
    void         SetAnim(Event *ev);
    void         SetState(Event *ev);
    void         SetKey(Event *ev);
    void         SetCamera(Event *ev);
    void         SetNumLoops(Event *ev);
    void         SetDelay(Event *ev);
    void         SetActionType(Event *ev);  
    bool         canBeUsed(Entity *activator);
    bool         GetInformation(
                Entity *activator, Vector *org, Vector *angles, str *animatoin, int *loopcount, str *state, str *camera
            );
    void TriggerTargets(Entity *activator);
    void Archive(Archiver& arc) override;






public:
    void SetThread( Event * ev ); 
};

inline void UseAnim::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveInteger(&count);
    arc.ArchiveBoolean(&active);
    arc.ArchiveString(&thread_name);
    arc.ArchiveString(&triggertarget);
    arc.ArchiveInteger(&num_loops);
    arc.ArchiveString(&state);
    arc.ArchiveString(&camera);
    arc.ArchiveString(&anim);
    arc.ArchiveString(&key);
    arc.ArchiveFloat(&delay);
    arc.ArchiveFloat(&last_active_time);
    arc.ArchiveString(&action_type);
}

class TouchAnim : public UseAnim
{
public:
    CLASS_PROTOTYPE(TouchAnim);

    TouchAnim();
};

class UseAnimDestination : public Entity
{
public:
    int num_loops;
    str state;
    str anim;

    CLASS_PROTOTYPE(UseAnimDestination);

    UseAnimDestination();
    void SetAnim(Event *ev);   
    void SetState(Event *ev);  
    void SetNumLoops(Event *ev); 
    int  GetNumLoops(void);
    str  GetAnim(void);
    str  GetState(void);
    void Archive(Archiver& arc) override;
};

inline void UseAnimDestination::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveInteger(&num_loops);
    arc.ArchiveString(&state);
    arc.ArchiveString(&anim);
}

class UseObject : public Entity
{
public:
    str               move_thread;
    str               stop_thread;
    str               reset_thread;
    str               triggertarget;
    Vector            offset;
    float             yaw_offset;
    int               count;
    float             cone;
    str               state;
    str               state_backwards;
    str               useMaterial;
    int               objectState;
    int               damage_type;  
    float             reset_time;
    str               action_type;
    qboolean          active;
    bool              movetheplayer;

    CLASS_PROTOTYPE(UseObject);

    UseObject();
    void SetMoveThread(Event *ev);
    void SetStopThread(Event *ev);
    void SetResetThread(Event *ev);
    void SetTriggerTarget(Event *ev);  
    void SetCount(Event *ev);          
    void SetState(Event *ev);          
    void DamageType(Event *ev);        
    void Reset(Event *ev);             
    void SetOffset(Event *ev);
    void SetYawOffset(Event *ev);
    void SetCone(Event *ev);
    void SetBackwardsState(Event *ev);
    void SetResetTime(Event *ev);
    void Resetting(Event *ev);
    void DamageTriggered(Event *ev);
    void DamageFunc(Event *ev);
    void Start(Event *ev = NULL);
    bool Loop(void);
    void SetActiveState(Event *ev);
    void Stop(Entity *activator);
    void ActivateEvent(Event *ev);
    void DeactivateEvent(Event *ev);
    void UseMaterialEvent(Event *ev);
    void MoveThePlayer(Event *ev); 
    void SetActionType(Event *ev); 
    bool canBeUsed(const Vector &org, const Vector &dir);
    void Setup(const Entity *activator, Vector *org, Vector *ang, str *newstate);
    void Archive(Archiver& arc) override;
};

// Not corpus-provable: no retail savegame contains a UseObject and no shipped BSP spawns
inline void UseObject::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveString(&move_thread);
    arc.ArchiveString(&stop_thread);
    arc.ArchiveString(&reset_thread);
    arc.ArchiveString(&triggertarget);
    arc.ArchiveVector(&offset);
    arc.ArchiveFloat(&yaw_offset);
    arc.ArchiveInteger(&count);
    arc.ArchiveFloat(&cone);
    arc.ArchiveString(&state);
    arc.ArchiveString(&state_backwards);
    arc.ArchiveString(&useMaterial);
    arc.ArchiveInteger(&objectState);
    arc.ArchiveFloat(&reset_time);
    arc.ArchiveString(&action_type);
    arc.ArchiveBoolean(&active);
    arc.ArchiveBool(&movetheplayer);
}

class Waypoint : public Entity
{
public:
    CLASS_PROTOTYPE(Waypoint);
};


class TossObject : public Entity
{
private:
    str   bouncesound;
    float bouncesoundchance;
    void  Touch(Event *ev);
    void  SetBounceSound(Event *ev);
    void  SetBounceSoundChance(Event *ev);

public:
    CLASS_PROTOTYPE(TossObject);

    TossObject();
    TossObject(const str& modelname);  
    void SetVelocity(float severity);
    void Archive(Archiver& arc) override;

    void Stop(Event *ev);    void SetBounceSound(const str &bounce);
    void SetBounceSoundChance(float chance);
};


class PushObject : public Entity
{
private:
    EntityPtr owner;
    float     attack_finished;
    int       dmg;
    str       pushsound;

public:
    CLASS_PROTOTYPE(PushObject);

    PushObject();
    void     SetDamage(Event *ev);
    void     setDamage(Event *ev); 
    void     BlockFunc(Event *ev);
    void     SetPushSound(Event *ev);
    qboolean Push(Entity *pusher, Vector move);
    qboolean canPush(Vector dir);

    Entity *getOwner(void);

    void Archive(Archiver& arc) override;






public:
    void Start( Event *ev ); 
};

inline void PushObject::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveSafePointer(&owner);
    arc.ArchiveFloat(&attack_finished);
    arc.ArchiveInteger(&dmg);
    arc.ArchiveString(&pushsound);
}

class FallingRock : public Entity
{
private:
    int     active;
    Vector  start_origin;
    Vector  last_bounce_origin;
    Entity *current;
    Entity *activator;
    Vector  bounce_dir;
    Vector  rotateaxis;
    float   attack_finished;
    float   wait;
    float   speed;
    int     dmg;
    str     bouncesound;

    void    Bounce(Event *ev);
    void    Rotate(Event *ev);
    void    Activate(Event *ev);
    void    NextBounce(void);
    void    StartFalling(Event *ev);
    void    SetWait(Event *ev);
    void    SetSpeed(Event *ev);
    void    SetBounceSound(str sound);
    Entity *SetNextBounceDir(void);
    void    Archive(Archiver& arc) override;

public:
    CLASS_PROTOTYPE(FallingRock);

    FallingRock();






public:
    void SetDmg( Event *ev ); 
    void setDamage( Event *ev ); 
    void SetBounceSound( Event *ev ); 
    void Touch( Event *ev ); 
};

inline void FallingRock::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveInteger(&active);
    arc.ArchiveVector(&start_origin);
    arc.ArchiveVector(&last_bounce_origin);
    arc.ArchiveObjectPointer((Class **)&current);
    arc.ArchiveObjectPointer((Class **)&activator);
    arc.ArchiveVector(&bounce_dir);
    arc.ArchiveVector(&rotateaxis);
    arc.ArchiveFloat(&attack_finished);
    arc.ArchiveFloat(&wait);
    arc.ArchiveFloat(&speed);
    arc.ArchiveInteger(&dmg);
    arc.ArchiveString(&bouncesound);
    if (arc.Loading()) {
        SetBounceSound(bouncesound);
    }
}

