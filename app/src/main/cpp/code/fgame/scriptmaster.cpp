

#include "g_local.h"
#include "scriptmaster.h"
#include "scriptvariable.h"
#include "scriptthread.h"
#include "globalcmd.h"
#include "gamescript.h"
#include "game.h"
#include "g_spawn.h"
#include "object.h"
#include "worldspawn.h"
#include "scriptexception.h"

#ifdef WIN32
#    include <direct.h>
#else
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#if defined(CGAME_DLL)

#    include <hud.h>

#    define SCRIPT_Printf  cgi.Printf
#    define SCRIPT_DPrintf cgi.DPrintf

#elif defined(GAME_DLL)
#    include "player.h"
#    include "entity.h"
#    include "weaputils.h"
#    include "camera.h"

#    define SCRIPT_Printf  gi.Printf
#    define SCRIPT_DPrintf gi.DPrintf

#endif


str      vision_current;
qboolean disable_team_change;
qboolean disable_team_spectate;

ScriptMaster Director;



extern Event EV_ScriptThread_RegisterAliasAndCache;

extern Event EV_ScriptThread_RegisterAlias;

extern Event EV_ScriptThread_Precache_Cache;


CLASS_DECLARATION(Listener, ScriptMaster, NULL) {
    {NULL,                      NULL                                }
};


ScriptMaster::~ScriptMaster()
{
    CloseScript();  
}







CThread *ScriptMaster::CreateThread(void)
{
    CThread *thread;

    thread = new CThread;
    thread->SetThreadNum(GetUniqueThreadNumber());
    Threads.AddObject(thread);  

    return thread;
}

CThread *ScriptMaster::CreateThread(str label, Listener *self)
{
    CThread *thread = CreateThread(label.c_str());

    if (thread && self && self->isSubclassOf(Entity)) {
        thread->SetCurrentEntity((Entity *)self);
    }

    return thread;
}

ScriptMaster::ScriptMaster()
{
    threadIndex = 0;
}





qboolean ScriptMaster::PlayerReady(void) const
{
    return player_ready;
}

void ScriptMaster::PlayerNotReady(void)
{
    player_ready = qfalse;
}


void ScriptMaster::PlayerSpawned(void)
{
    CThread *thread;
    int      i, n;

    player_ready = qtrue;

    n = Threads.NumObjects();
    for (i = 1; i <= n; i++) {
        thread = Threads.ObjectAt(i);
        assert(thread);
        if (thread) {
            if (thread->WaitingOnPlayer()) {
                thread->ClearWaitFor();
                thread->DelayedStart(0.0f);
            }
        }
    }
}


ScriptVariableList gameVars;
ScriptVariableList levelVars;

void ScriptMaster::Archive(Archiver& arc)
{
    CThread *ptr;
    int      i, num;

    arc.ArchiveObjectPointer((Class **)&currentThread);

    if (arc.Saving()) {
        num = Threads.NumObjects();
    } else {
        Threads.FreeObjectList();
    }

    arc.ArchiveInteger(&num);

    for (i = 1; i <= num; i++) {
        if (arc.Loading()) {
            ptr = new CThread;
            Threads.AddObject(ptr);
        } else {
            ptr = Threads.ObjectAt(i);
        }

        arc.ArchiveObject(ptr);
    }

    arc.ArchiveInteger(&threadIndex);

    arc.ArchiveBoolean(&player_ready);
}





CThread *ScriptMaster::CurrentThread(void)
{
    return currentThread;
}


int ScriptMaster::GetUniqueThreadNumber(void)
{
    do {
        threadIndex++;
        if (threadIndex == 0) {
            threadIndex = 1;
        }
    } while (GetThread(threadIndex));

    return threadIndex;
}

CThread *ScriptMaster::GetThread(int num)
{
    int      i;
    int      n;
    CThread *thread;

    n = Threads.NumObjects();
    for (i = 1; i <= n; i++) {
        thread = Threads.ObjectAt(i);
        assert(thread);
        if (thread) {
            if (thread->ThreadNum() == num) {
                return thread;
            }
        }
    }

    return NULL;
}

qboolean ScriptMaster::KillThread(int num)
{
    CThread *thread = GetThread(num);

    if (!thread) {
        return qfalse;
    }

    if (thread == currentThread) {
        thread->threadDying    = true;
        thread->doneProcessing = true;
        thread->PostEvent(EV_Remove, 0.0f);
        return qtrue;
    }

    delete thread;

    return qtrue;
}

void ScriptMaster::KillThread(const str& name)
{
    const char *nameStr  = name.c_str();
    volatile uintptr_t addr = (uintptr_t)nameStr;
    if (!nameStr || addr < 4096 || !*nameStr) {
        return;
    }

    const char *star     = strchr(nameStr, '*');
    bool        wildcard = (star != NULL);
    size_t      matchLen = wildcard ? (size_t)(star - nameStr) : 0;
    if (wildcard && star < nameStr) {
        matchLen = 0;
    }

    Container<SafePtr<CThread> > toKill;

    for (int i = Threads.NumObjects(); i > 0; i--) {
        CThread *thread = Threads.ObjectAt(i);

        if (!thread || thread == currentThread) {
            continue;
        }

        const char *tn = thread->ThreadName();
        volatile uintptr_t tnAddr = (uintptr_t)tn;
        if (!tn || tnAddr < 4096) {
            continue;
        }
        int result = wildcard ? Q_stricmpn(tn, nameStr, matchLen) : Q_stricmp(tn, nameStr);

        if (result == 0) {
            toKill.AddObject(SafePtr<CThread>(thread));
        }
    }

    for (int i = 1; i <= toKill.NumObjects(); i++) {
        CThread *thread = toKill.ObjectAt(i);

        if (thread) {
            delete thread;
        }
    }
}

qboolean ScriptMaster::NotifyOtherThreads(int num)
{
    CThread *thread1;
    CThread *thread2;
    int      i;
    int      n;

    thread1 = GetThread(num);

    n = Threads.NumObjects();
    for (i = 1; i <= n; i++) {
        thread2 = Threads.ObjectAt(i);
        if (!thread2) {
            continue;
        }

        if (thread2->WaitingOnThread() == thread1) {
            Event *ev = new Event(EV_ScriptThread_ThreadCallback);

            ev->SetThread(thread1);
            thread2->ProcessEvent(ev);
            return true;
        }
    }

    return qfalse;
}

qboolean ScriptMaster::RemoveThread(int num)
{
    CThread *thread;
    int      i;
    int      n;

    n = Threads.NumObjects();
    for (i = n; i > 0; i--) {
        thread = Threads.ObjectAt(i);
        if (!thread) {
            continue;
        }

        if (thread->ThreadNum() == num) {
            Threads.RemoveObjectAt(i);
            if (currentThread == thread) {
                currentThread = NULL;
            }
            return qtrue;
        }
    }

    return qfalse;
}

CThread *ScriptMaster::CreateThread(const char *label)
{
    CThread *thread;
    int      threadnum;

    thread = new CThread;

    threadnum = GetUniqueThreadNumber();
    // SDK scriptmaster.cpp:972-989.  The CThread constructor does NOT register the
    Threads.AddObject(thread);

    if (!thread->Setup(threadnum, label)) {
        KillThread(threadnum);
        return NULL;
    }

    return thread;
}













void ScriptMaster::CloseScript( void )
{
	KillThreads();
	ScriptLib.CloseScripts();
}


extern Event EV_ScriptThread_DeathCallback;


void ScriptMaster::DeathMessage( const char *name )
{
	CThread         *thread;
	Event                *ev;
	int                  i,n;
	
	n = Threads.NumObjects();
	for( i = 1; i <= n; i++ )
	{
		const char *waiting;
		thread = Threads.ObjectAt( i );
		assert( thread );
		if (thread)
		{
			waiting = thread->waitingForDeath.c_str();
			if (waiting)
            {
				if (!strcmp(waiting, name))
				{
					ev = new Event( EV_ScriptThread_DeathCallback );
					thread->ProcessEvent(ev);
				}
            }
		}
	}
}


void ScriptMaster::KillThreads( void )
{
	int i;
	int num;
	
	num = Threads.NumObjects();
	
	for( i = num; i > 0; i-- )
	{
		Threads.ObjectAt( i )->ClearWaitFor();
	}
	
	for( i = num; i > 0; i-- )
	{
		delete Threads.ObjectAt( i );
	}
	
	Threads.FreeObjectList();
	
	threadIndex		= 0;
	currentThread = NULL;
}



void ScriptMaster::SetCurrentThread( CThread *thread )
{
	currentThread = thread;
}





CThread *ScriptMaster::CreateThread( const char *label, Program *program_shadow1 )
{
	CThread *thread;
	int threadnum;
	
	thread = new CThread;
	thread->program = program_shadow1;
	
	threadnum = GetUniqueThreadNumber();
	Threads.AddObject( thread );
	
	if ( !thread->Setup( threadnum, label ) )
	{
		KillThread( threadnum );
		return NULL;
	}
	
	return thread;
}


