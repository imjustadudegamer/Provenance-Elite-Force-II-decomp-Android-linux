
#pragma once

#include "../corepp/listener.h"
#include "interpreter.h"

class Entity;     
class TargetList; 

extern Event EV_ScriptThread_Execute;

class CThread : public Interpreter
{
    friend class EndOn;
    friend class Listener;
    friend class ScriptMaster;

private:
    // SDK globalcmd.h:83-92 -- retail verbatim.  A retail thread does not suspend a
    float      waitUntil;         
    CThread   *waitingForThread;  
    str        waitingForDeath;   
    qboolean   waitingForPlayer;  
    TargetList *waitingFor;       
    int        waitingNumObjects; 
    EntityPtr  currentEntity;     

protected:

    void CreateThread(Event *ev);
    void EventWait(Event *ev);
    void EventPause(Event *ev);
    void Print(Event *ev);

    void Assert(Event *ev);

    void TriggerEvent(Event *ev);
    void CacheResourceEvent(Event *ev);
    void RegisterAliasAndCache(Event *ev);
    void RegisterAlias(Event *ev);
    void MapEvent(Event *ev);
    void SetCvarEvent(Event *ev);

    void      Spawn(Event *ev);
    void      CreateCinematic(Event *ev); 

    void CueCamera(Event *ev);
    void CuePlayer(Event *ev);
    void FreezePlayer(Event *ev);
    void ReleasePlayer(Event *ev);

    void FadeIn(Event *ev);
    void FadeOut(Event *ev);
    void ClearFade(Event *ev);
    void Letterbox(Event *ev);
    void ClearLetterbox(Event *ev);
    void MusicEvent(Event *ev);
    void ForceMusicEvent(Event *ev);
    void MusicVolumeEvent(Event *ev);
    void RestoreMusicVolumeEvent(Event *ev);

    void SoundtrackEvent(Event *ev);

    void SetCinematic(Event *ev);
    void SetNonCinematic(Event *ev);


    void StuffCommand(Event *ev);
    void KillEnt(Event *ev);
    void RemoveEnt(Event *ev);
    void KillClass(Event *ev);
    void RemoveClass(Event *ev);
    void CameraCommand(Event *ev);
    void SetLightStyle(Event *ev);
    void CenterPrint(Event *ev);




    void SendClientCommand(Event *ev);
    void GetNumFreeReliableServerCommands(Event *ev);
    void FakePlayer(Event *ev);
    void RemoveFakePlayer(Event *ev);





public:
    CLASS_PROTOTYPE(CThread);

    CThread();

    void Parse(const char *filename);

    void DelayedStart(float delay);
    void Start(void);


    void     ExecuteFunc(Event *ev);
    void     ClearWaitFor(void);
    qboolean WaitingFor(Entity *obj);
    void     ObjectMoveDone(Event *ev);
    void     ScriptCallback(Event *ev);
    void     ThreadCallback(Event *ev);
    void     DeathCallback(Event *ev);

    void                  SetCurrentEntity(Entity *ent);



    void EventWaitForPlayer(Event *ev);    
    void EventWaitForDialog(Event *ev);    
    void EventWaitDialogLength(Event *ev); 

    void EventWaitForAnimation(Event *ev); 
    void EventWaitForThread(Event *ev);    
    void EventWaitFor(Event *ev);          


    CThread    *WaitingOnThread(void) { return waitingForThread; }
    const char *WaitingOnDeath(void)  { return waitingForDeath; }
    qboolean    WaitingOnPlayer(void) { return waitingForPlayer; }





    void                    TerminateThread(Event *ev);
    void                    EventWaitForDeath(Event *ev);
    void                    EventWaitForSound(Event *ev);
    void                    CPrint(Event *ev);
    void                    PrintInt(Event *ev);
    void                    PrintFloat(Event *ev);
    void                    PrintVector(Event *ev);
    void                    NewLine(Event *ev);
    void                    Break(Event *ev);
    void                    DoMove(void) override;
    void                    TriggerEntityEvent(Event *ev);
    void                    noIntermission(Event *ev);
    void                    dontSaveOrientation(Event *ev);
    void                    setPlayerDeathThread(Event *ev);
    void                    endPlayerDeathThread(Event *ev);
    void                    GetCvarEvent(Event *ev);
    void                    GetCvarFloatEvent(Event *ev);
    void                    GetCvarIntEvent(Event *ev);
    void                    FadeIsActive(Event *ev);
    void                    allowMusicDucking(Event *ev);
    void                    allowActionMusic(Event *ev);
    void                    RestoreSoundtrackEvent(Event *ev);
    void                    SetLevelAI(Event *ev);
    void                    SetSkipThread(Event *ev);
    void                    PassToPathmanager(Event *ev);
    void                    RemoveActorsNamed(Event* ev);
    void                    doesEntityExist(Event *ev);
    void                    GetEntityEvent(Event *ev);
    void                    GetNextEntityEvent(Event *ev);
    void                    CosDegrees(Event *ev);
    void                    SinDegrees(Event *ev);
    void                    CosRadians(Event *ev);
    void                    SinRadians(Event *ev);
    void                    ArcTanDegrees(Event *ev);
    void                    ScriptSqrt(Event *ev);
    void                    ScriptLog(Event *ev);
    void                    ScriptExp(Event *ev);
    void                    RandomFloat(Event *ev);
    void                    RandomInteger(Event *ev);
    void                    KillThreadEvent(Event *ev);
    void                    SetThreadName(Event *ev);
    void                    GetCurrentEntity(Event *ev);
    void                    SendCommandToSlaves(const char *name, Event *ev);
    void                    ProcessCommand(int argc, const char **argv);
    void                    SetFloatVar(Event *ev);
    void                    SetVectorVar(Event *ev);
    void                    SetStringVar(Event *ev);
    void                    doesVarExist(Event *ev);
    void                    RemoveVariable(Event* ev);
    void                    GetFloatVar(Event *ev);
    void                    GetVectorVar(Event *ev);
    void                    GetStringVar(Event *ev);
    void                    isActorDead(Event *ev);
    qboolean                isActorDead(const str &actor_name);
    void                    isActorAlive(Event *ev);
    qboolean                isActorAlive(const str &actor_name);
    void                    SendClientVar(Event *ev);
    void                    ModEvent(Event *ev);
    void                    DivEvent(Event *ev);
    void                    VectorScaleEvent(Event *ev);
    void                    VectorDotEvent(Event *ev);
    void                    VectorCrossEvent(Event *ev);
    void                    VectorNormalizeEvent(Event *ev);
    void                    VectorLengthEvent(Event *ev);
    void                    VectorSetXEvent(Event *ev);
    void                    VectorSetYEvent(Event *ev);
    void                    VectorSetZEvent(Event *ev);
    void                    VectorGetXEvent(Event *ev);
    void                    VectorGetYEvent(Event *ev);
    void                    VectorGetZEvent(Event *ev);
    void                    VectorForwardEvent(Event *ev);
    void                    VectorLeftEvent(Event *ev);
    void                    VectorUpEvent(Event *ev);
    void                    vectorToString(Event *ev);
    void                    TargetOfEvent(Event *ev);
    void                    FloorEvent(Event *ev);
    void                    CeilEvent(Event *ev);
    void                    RoundEvent(Event *ev);
    void                    GetGameplayFloat(Event *ev);
    void                    GetGameplayString(Event *ev);
    void                    SetGameplayFloat(Event *ev);
    void                    SetGameplayString(Event *ev);
    void                    GetIntegerFromString(Event *ev);
    void                    GetLevelTime(Event* ev);
    void                    connectPathnodes(Event *ev);
    void                    disconnectPathnodes(Event *ev);

    virtual void Archive(Archiver& arc) override;






};

inline void CThread::DelayedStart(float delay)
{
    CancelEventsOfType(EV_ScriptThread_Execute);
    PostEvent(EV_ScriptThread_Execute, delay);
}

inline void CThread::Start(void)
{
    CancelEventsOfType(EV_ScriptThread_Execute);
    ProcessEvent(EV_ScriptThread_Execute);
}

typedef SafePtr<CThread> ThreadPtr;

