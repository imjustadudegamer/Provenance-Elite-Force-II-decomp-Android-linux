

#pragma once

#include "glb_local.h"
#include "animate.h"
#include "gamescript.h"

class ScriptMaster;

extern Event EV_Trigger_Effect;
extern Event EV_Trigger_SetDestructible;  
extern Event EV_Trigger_SetTriggerable;  
extern Event EV_Trigger_SetNotTriggerable;  
extern Event EV_Trigger_ActivateTargets;  
extern Event EV_Trigger_StartThread;      

#define VISIBLE           1
#define TRIGGER_PLAYERS     4
#define TRIGGER_MONSTERS    8
#define TRIGGER_PROJECTILES 16
#define TRIGGER_SCRIPTSLAVE 32

class Trigger : public Entity
{
protected:
    float             wait;
    float             delay;
    float             trigger_time;
    qboolean          triggerActivated;
    int               count;
    str               noise;
    str               message;
    EntityPtr         activator;
    int               respondto;
    qboolean          useTriggerDir;
    float             triggerCone;
    Vector            triggerDir;
    float             triggerDirYaw;
    qboolean          triggerable;
    qboolean          removable;     
    qboolean          edgeTriggered; 
    int               multiFaceted;  

    // SDK trigger.h:64 -- the item a Sentient must be carrying before this trigger will
    str key;

    str thread;

    bool destructible;
    int  instantdeath; 
    bool triggerondamage;
    bool triggerondeath;

    bool CanTrigger(void);

public:
    CLASS_PROTOTYPE(Trigger);

    Trigger();
    virtual ~Trigger() override;

    virtual qboolean respondTo(Entity *other);

    virtual Entity *getActivator(Entity *other);

    void Touch(Event *ev);
    void EventSetKey(Event *ev);  

    void EventSetWait(Event *ev);            
    void EventSetDelay(Event *ev);           
    void EventSetCount(Event *ev);           
    void EventSetThread(Event *ev);          
    void EventSetMessage(Event *ev);         
    void SetMessage(const char *text);       
    void EventSetNoise(Event *ev);           
    void SetNoise(const char *text);         
    void SetTakeDamage(bool cantakedamage);  
    void SetMultiFaceted(int newFacet);      
    void SetEdgeTriggered(qboolean newEdge); 
    void SetMultiFaceted(Event *ev);         
    void SetEdgeTriggered(Event *ev);        
    void SetTriggerable(Event *ev);          
    void SetNotTriggerable(Event *ev);       
    void EventSetTriggerOnDamage(Event *ev); 
    void EventSetTriggerOnDeath(Event *ev);  
    void EventSetDestructible(Event *ev);    
    void EventSetInstantDeath(Event *ev);    
    void EventSetHealth(Event *ev);          

    str& Message(void);

    str& Noise(void);

    void     SetTriggerDir(float angle);
    void     SetTriggerDir(Event *ev);
    Vector   GetTriggerDir(void);
    qboolean UsingTriggerDir(void);
    void     SetTriggerCone(Event *ev);
    void     SetModelEvent(Event *ev);


    int      GetMultiFaceted(void);
    qboolean GetEdgeTriggered(void);

    void StartThread(Event *ev);
    void TriggerStuff(Event *ev);
    void ActivateTargets(Event *ev);


    Container<int> entList;

    void HandleDamage(Event *ev);
    void HandleKilled(Event *ev);

    void Archive(Archiver& arc) override;






public:
    void AddOtherToEntList(Entity *other); 
    void CheckEntList(Event *ev); 
    void EventGetLastActivatingEntity( Event *ev ); 
    void setActivatingEntity( Entity *activatingEntity ); 
    qboolean IsEntityInBoundingBox(Entity* ent); 
};

class TouchField : public Trigger
{
private:
    Event    *ontouch;
    EntityPtr owner;

public:
    CLASS_PROTOTYPE(TouchField);

    TouchField();
    void         Setup(Entity *ownerentity, const Event &touchevent, const Vector &min, const Vector &max, int respondto);  
    void         SendEvent(Event *ev);
    void         Archive(Archiver& arc) override;
};

inline void TouchField::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveEventPointer(&ontouch);
    arc.ArchiveSafePointer(&owner);
}

typedef SafePtr<TouchField> TouchFieldPtr;

class TriggerOnce : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerOnce);
    TriggerOnce();
};

class TriggerRelay : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerRelay);

    TriggerRelay();
};

class TriggerSecret : public TriggerOnce
{
public:
    CLASS_PROTOTYPE(TriggerSecret);

    TriggerSecret();
    void FoundSecret(Event *ev);
    void Activate(Event *ev);
};

class TriggerPush : public Trigger
{
protected:
    float speed;

public:
    CLASS_PROTOTYPE(TriggerPush);

    TriggerPush();
    void Push(Event *ev);
    void Archive(Archiver& arc) override;






public:
    void SetPushDir( Event *ev ); 
    void SetPushSpeed( Event *ev ); 
};

inline void TriggerPush::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&speed);
}

class TriggerPushAny : public Trigger
{
protected:
    float speed;

public:
    CLASS_PROTOTYPE(TriggerPushAny);

    TriggerPushAny();
    void Push(Event *ev);
    void Archive(Archiver& arc) override;






public:
    void SetSpeed( Event *ev ); 
};

inline void TriggerPushAny::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&speed);
}

#define AMBIENT_ON  (1 << 0)
#define AMBIENT_OFF (1 << 1)
#define TOGGLESOUND (1 << 5)

class TriggerPlaySound : public Trigger
{
protected:
    friend class SoundManager;

    int      state;
    float    min_dist;
    float    volume;
    int      channel;
    qboolean ambient;

public:
    CLASS_PROTOTYPE(TriggerPlaySound);

    TriggerPlaySound();
    void ToggleSound(Event *ev);

    void StartSound(void);

    void Archive(Archiver& arc) override;






public:
    void SetVolume( Event *ev ); 
    void SetMinDist( Event *ev ); 
    void SetVolume( float vol ); 
    void SetMinDist( float dist ); 
    void SetChannel( Event *ev ); 
};

inline void TriggerPlaySound::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveInteger(&state);
    arc.ArchiveFloat(&min_dist);
    arc.ArchiveFloat(&volume);
    arc.ArchiveInteger(&channel);
    arc.ArchiveBoolean(&ambient);
    if (arc.Loading()) {
        if (spawnflags & TOGGLESOUND) {
            state = !state;
            PostEvent(EV_Trigger_Effect, EV_POSTSPAWN);
        }
    }
}

class TriggerSpeaker : public TriggerPlaySound
{
public:
    CLASS_PROTOTYPE(TriggerSpeaker);

    TriggerSpeaker();
};

class RandomSpeaker : public TriggerSpeaker
{
protected:
    friend class SoundManager;

    float chance;
    float mindelay;
    float maxdelay;

public:
    CLASS_PROTOTYPE(RandomSpeaker);

    RandomSpeaker();
    void TriggerSound(Event *ev);
    void SetMinDelay(Event *ev);
    void SetMaxDelay(Event *ev);
    void SetChance(Event *ev);
    void SetMinDelay(float newMinDelay);
    void SetMaxDelay(float newMaxDelay);
    void SetChance(float newChance);

    void ScheduleSound(void);

    void Archive(Archiver& arc) override;
};

inline void RandomSpeaker::Archive(Archiver& arc)
{
    TriggerSpeaker::Archive(arc);

    arc.ArchiveFloat(&chance);
    arc.ArchiveFloat(&mindelay);
    arc.ArchiveFloat(&maxdelay);
}

class TriggerChangeLevel : public Trigger
{
protected:
    str      map;
    str      spawnspot;

    str changethread;

public:
    CLASS_PROTOTYPE(TriggerChangeLevel);

    TriggerChangeLevel();
    void        SetSpawnSpot(Event *ev);  
    void        SetThread(Event *ev);  
    void        ChangeLevel(Event *ev);
    const char *Map(void);
    const char *SpawnSpot(void);
    void        Archive(Archiver& arc) override;






public:
    void SetMap( Event *ev ); 
};

class TriggerExit : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerExit);

    TriggerExit();
    void DisplayExitSign(Event *ev);
    void TurnExitSignOff(Event *ev);
};

class TriggerUse : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerUse);

    TriggerUse();
};

class TriggerUseOnce : public TriggerUse
{
public:
    CLASS_PROTOTYPE(TriggerUseOnce);

    TriggerUseOnce();
};

class TriggerHurt : public TriggerUse
{
protected:
    float damage;

    void Hurt(Event *ev);
    void SetDamage(Event *ev);
    void setDamage(Event *ev);

public:
    CLASS_PROTOTYPE(TriggerHurt);

    TriggerHurt();
    void Archive(Archiver& arc) override;
};

inline void TriggerHurt::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&damage);
}

class TriggerDamageTargets : public Trigger
{
protected:
    float damage;

    void DamageTargets(Event *ev);

public:
    CLASS_PROTOTYPE(TriggerDamageTargets);

    TriggerDamageTargets();
    void SetDamage(Event *ev);
    void setDamage(Event *ev);
    void PassDamage(Event *ev);
    void Archive(Archiver& arc) override;
};

inline void TriggerDamageTargets::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&damage);
}

class TriggerCameraUse : public TriggerUse
{
public:
    CLASS_PROTOTYPE(TriggerCameraUse);

    void TriggerCamera(Event *ev);
};

class TriggerBox : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerBox);

    void SetMins(Event *ev);
    void SetMaxs(Event *ev);
    // the port's invented ones were declared-but-never-defined and polluted the vtable.)
};

class TriggerMusic : public Trigger
{
private:
    friend class SoundManager;

    qboolean  oneshot;
    str       current;
    str       fallback;
    str       altcurrent;
    str       altfallback;

public:
    CLASS_PROTOTYPE(TriggerMusic);

    TriggerMusic();
    void SetCurrentMood(Event *ev);
    void SetFallbackMood(Event *ev);
    void SetAltCurrentMood(Event *ev);
    void SetAltFallbackMood(Event *ev);
    void ChangeMood(Event *ev);
    void AltChangeMood(Event *ev);
    void SetOneShot(Event *ev);

    void SetMood(const str &crnt, const str &fback);
    void SetAltMood(const str &crnt, const str &fback);
    void SetOneShot(qboolean once);

    void Archive(Archiver& arc) override;
};

class TriggerReverb : public Trigger
{
private:
    friend class SoundManager;

    qboolean oneshot;
    int      reverbtype;
    int      altreverbtype;
    float    reverblevel;
    float    altreverblevel;

public:
    CLASS_PROTOTYPE(TriggerReverb);

    TriggerReverb();
    void SetReverbType(Event *ev);
    void SetReverbLevel(Event *ev);
    void SetAltReverbType(Event *ev);
    void SetAltReverbLevel(Event *ev);
    void ChangeReverb(Event *ev);
    void AltChangeReverb(Event *ev);
    void SetOneShot(Event *ev);

    void SetReverb(int type, float level);
    void SetAltReverb(int type, float level);
    void SetOneShot(qboolean once);

    void Archive(Archiver& arc) override;
};

inline void TriggerReverb::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveBoolean(&oneshot);
    arc.ArchiveInteger(&reverbtype);
    arc.ArchiveInteger(&altreverbtype);
    arc.ArchiveFloat(&reverblevel);
    arc.ArchiveFloat(&altreverblevel);
}

class TriggerByPushObject : public TriggerOnce
{
private:
    str       triggername;

    void setTriggerName(Event *event);

public:
    CLASS_PROTOTYPE(TriggerByPushObject);

    qboolean respondTo(Entity *other) override;
    Entity  *getActivator(Entity *other) override;

    void Archive(Archiver& arc) override;
};

class TriggerGivePowerup : public Trigger
{
private:
    qboolean  oneshot;
    str       powerup_name;

public:
    CLASS_PROTOTYPE(TriggerGivePowerup);

    TriggerGivePowerup();
    void SetOneShot(Event *ev);
    void SetPowerupName(Event *ev);

    void GivePowerup(Event *ev);
    void Archive(Archiver& arc) override;
};

class TriggerLadder : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerLadder);

    void handleTouchEvent(Event *ev);
};


typedef enum
{
    VAR_INCREMENT,
    VAR_DECREMENT,
    VAR_TOGGLE,
    VAR_NORMAL
} variableType_t;

class TriggerSetVariable : public Trigger
{
private:
    str            variableName;
    str            variableValue;
    variableType_t variableType;

public:
    CLASS_PROTOTYPE(TriggerSetVariable);

    TriggerSetVariable();

    void SetVariableName(Event *ev);
    void SetVariableValue(Event *ev);
    void SetVariable(Event *ev);

    void Archive(Archiver& arc) override;
};

inline void TriggerSetVariable::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveString(&variableName);
    arc.ArchiveString(&variableValue);
    ArchiveEnum(variableType, variableType_t);
}

class LevelInteractionTrigger : public Trigger
{
public:
    CLASS_PROTOTYPE(LevelInteractionTrigger);

    LevelInteractionTrigger();
    qboolean respondTo(Entity *other) override;
    void     Archive(Archiver& arc) override;
};

inline void LevelInteractionTrigger::Archive(Archiver& arc)
{
    Trigger::Archive(arc);
}

class WorkTrigger;
typedef SafePtr<WorkTrigger> WorkTriggerPtr;

class WorkTrigger : public Trigger
{
private:
    str            _animation;
    float          _time;
    qboolean       _reserved;
    int            _currentEnt;
    Container<str> _allowedWorkers;

public:
    CLASS_PROTOTYPE(WorkTrigger);

    WorkTrigger();

    void  SetAnimation(Event *ev);
    str   GetAnimation();

    void  SetTime(Event *ev);
    float GetTime();

    void  Reserve(int entNum);
    void  Release();

    void     AddWorker(Event *ev);
    qboolean isAllowedToWork(const str& name, int entNum);
    qboolean isReserved();

    void Archive(Archiver& arc) override;
};

inline void WorkTrigger::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveString(&_animation);
    arc.ArchiveFloat(&_time);
    arc.ArchiveBoolean(&_reserved);
    arc.ArchiveInteger(&_currentEnt);

    _allowedWorkers.Archive(arc);
}

class TriggerGroupEvent : public Trigger
{
private:
    Event passEvent;
    // retail copies itself.  They must live in the class: a fixup registered for a valid
    int               groupNumber;

public:
    CLASS_PROTOTYPE(TriggerGroupEvent);

    TriggerGroupEvent();
    void TriggerStuff(Event *ev);
    void SetPassEvent(Event *ev);
    void SetGroupNumber(Event *ev);
    void PassEvent(Event *ev);

    void Archive(Archiver& arc) override;
};

inline void TriggerGroupEvent::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    passEvent.ArchiveRetail(arc);
    arc.ArchiveInteger(&groupNumber);
}

class TriggerCallVolume : public Trigger
{
private:
    Container<str> _requiredEntities;
    qboolean       _ready;
    str            _exitThread;

protected:
    Entity *_getEntity(const str& name);
    void    _notifyRequiredEnts(bool inCallVolume);
    void    _checkForRequiredEnts();
    bool    _isRequiredEnt(Entity *other);

public:
    CLASS_PROTOTYPE(TriggerCallVolume);

    TriggerCallVolume();
    ~TriggerCallVolume();

    void AddRequiredEntity(Event *ev);
    void EntityLeftVolume(Event *ev);
    void TriggerStuff(Event *ev);
    void CheckReady(Event *ev);
    void SetExitThread(Event *ev);

    virtual void Archive(Archiver& arc) override;
};

inline void TriggerCallVolume::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    _requiredEntities.Archive(arc);
    arc.ArchiveBoolean(&_ready);
    arc.ArchiveString(&_exitThread);
}

inline void TriggerCallVolume::SetExitThread(Event *ev)
{
    _exitThread = ev->GetString(1);
}

class TriggerEntryAndExit : public Trigger
{
private:
    str      _entryThread;
    str      _exitThread;
    qboolean _entered;

    float    _forcefieldtrigger;
    float    _triggernumber;

public:
    CLASS_PROTOTYPE(TriggerEntryAndExit);

    TriggerEntryAndExit();
    ~TriggerEntryAndExit();

    void EntityLeftVolume(Event *ev);
    void TriggerStuff(Event *ev);

    void SetEntryThread(Event *ev);
    void SetExitThread(Event *ev);

    void EnterTrigger(Event *ev);
    void ExitTrigger(Event *ev);

    void EnterTrigger();
    void ExitTrigger();

    void HackSetParms(Event *ev);
    void HackGetForceFieldTrigger(Event *ev);
    void HackGetTriggerNumber(Event *ev);

    virtual void Archive(Archiver& arc) override;





};

inline void TriggerEntryAndExit::SetEntryThread(Event *ev)
{
    _entryThread = ev->GetString(1);
}

inline void TriggerEntryAndExit::SetExitThread(Event *ev)
{
    _exitThread = ev->GetString(1);
}

inline void TriggerEntryAndExit::EnterTrigger()
{
    _entered = true;
}

inline void TriggerEntryAndExit::ExitTrigger()
{
    _entered = false;
}

inline void TriggerEntryAndExit::EnterTrigger(Event *ev)
{
    EnterTrigger();
}

inline void TriggerEntryAndExit::ExitTrigger(Event *ev)
{
    ExitTrigger();
}

inline void TriggerEntryAndExit::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveString(&_entryThread);
    arc.ArchiveString(&_exitThread);
    arc.ArchiveBoolean(&_entered);

    arc.ArchiveFloat(&_forcefieldtrigger);
    arc.ArchiveFloat(&_triggernumber);
}

class TriggerVolume : public Trigger
{
public:
    CLASS_PROTOTYPE(TriggerVolume);
    ~TriggerVolume() {}

    void HandleTouchEvent(Event *ev);
    void HandleContactEvent(Event *ev);
    void HandleLostContactEvent(Event *ev);
    void HandleSetEntryThreadEvent(Event *ev);
    void HandleSetExitThreadEvent(Event *ev);

    virtual void Archive(Archiver& arc) override;

private:
    str _entryThread;
    str _exitThread;
};

inline void TriggerVolume::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveString(&_entryThread);
    arc.ArchiveString(&_exitThread);
}

