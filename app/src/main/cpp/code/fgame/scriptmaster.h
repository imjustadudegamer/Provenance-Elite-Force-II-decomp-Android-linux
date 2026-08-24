

#pragma once

#include "../corepp/class.h"
#include "../corepp/listener.h"
#include "gamescript.h"
#include "../corepp/con_set.h"
#include <con_arrayset.h>

class CThread;
class ScriptVariableList;  
class Program;             

#define MAX_COMMANDS       20
#define MAX_EXECUTION_TIME 3000

class ScriptMaster : public Listener
{
    friend class CThread;

public:
    CThread *currentThread; 

    int threadIndex;

    Container<CThread *> Threads;


    qboolean player_ready;             

protected:

public:
    CLASS_PROTOTYPE(ScriptMaster);

    ScriptMaster();
    virtual ~ScriptMaster() override;

    CThread *CreateThread(void);
    CThread *CreateThread(str label, Listener *self);
    CThread *CurrentThread(void);

    int           GetUniqueThreadNumber(void);
    CThread *GetThread(int num);
    qboolean      KillThread(int num);

    qboolean NotifyOtherThreads(int num);
    qboolean RemoveThread(int num);
    CThread *CreateThread(const char *label);
    CThread *CreateThread(const char *label, Program *program);  



    qboolean PlayerReady(void) const;
    void     PlayerSpawned(void);
    void     PlayerNotReady(void);


    void Archive(Archiver& arc) override;

    void CloseScript(Event *ev);
    void DeathMessage(Event *ev);
    void KillThreads(Event *ev);
    void SetCurrentThread(Event *ev);
    void SetCurrentThread(CThread *thread);
     ScriptMaster(Event *ev);






public:
    void KillThread( const str &name ); 
    void CloseScript( void ); 
    void DeathMessage( const char *name ); 
    void KillThreads( void ); 
};


extern qboolean disable_team_change;
extern qboolean disable_team_spectate;
extern str      vision_current;

extern Event EV_ScriptThread_Trace;
extern Event EV_ScriptThread_SightTrace;
extern Event EV_ScriptThread_VisionSetNaked;
extern Event EV_ScriptThread_CancelWaiting;

extern ScriptMaster                    Director;
extern ScriptVariableList              gameVars;   
extern ScriptVariableList              levelVars;  
