
#include "scriptthread.h"
#include "scriptmaster.h"
#include "scriptvariable.h"
#include "scriptexception.h"

#include "g_spawn.h"
#include "level.h"
#include "game.h"
#include "camera.h"
#include "cinematic.h" 
#include "object.h"
#include "actor.h" 
#include "worldspawn.h" 
#include "weaputils.h"
#include "player.h"
#include "lightstyleclass.h"
#include "navigate.h" 
#include "debuglines.h"


#include "md5.h"

#ifdef WIN32
#    include <direct.h>
#else
#    include <sys/stat.h>
#    include <unistd.h>
#endif

extern Event EV_ScriptThread_Sqrt;

extern Event EV_ScriptThread_Goto;

extern Event EV_Trigger_SetWait;

extern Event EV_ScriptThread_Pause;

extern Event EV_ScriptThread_Print;

extern Event EV_ScriptThread_Assert;

extern Event EV_ScriptThread_Clear;

extern Event EV_ScriptSlave_Trigger;

extern Event EV_ScriptThread_Spawn;

extern Event EV_ScriptThread_CreateCinematic;

extern Event EV_ScriptThread_Map;

extern Event EV_ScriptThread_SetCvar;

extern Event EV_ScriptThread_CueCamera;

extern Event EV_ScriptThread_CuePlayer;

extern Event EV_ScriptThread_FreezePlayer;

extern Event EV_ScriptThread_ReleasePlayer;

extern Event EV_Player_KillEnt;

extern Event EV_Player_KillClass;

extern Event EV_Player_RemoveEnt;

extern Event EV_Player_RemoveClass;



extern Event EV_ScriptThread_StuffCommand;

extern Event EV_ScriptThread_SetCinematic;

extern Event EV_ScriptThread_SetNonCinematic;

extern Event EV_ScriptThread_RegisterAliasAndCache;

extern Event EV_ScriptThread_RegisterAlias;

extern Event EV_ScriptThread_Precache_Cache;

extern Event EV_FadeIn;

extern Event EV_Fade;

extern Event EV_ScriptThread_WaitForPlayer;

extern Event EV_ScriptThread_WaitForDialog;

// (Program::GetDef, program.cpp:421-436: an event def has no scope, so it is never
extern Event EV_Player_KillAllDialog;

extern Event EV_ScriptThread_WaitForAnimation;

extern Event EV_ScriptThread_WaitForThread;

extern Event EV_ScriptThread_TerminateThread;

extern Event EV_ScriptThread_WaitFor;

extern Event EV_ScriptThread_SetSkipThread;

extern Event EV_ScriptThread_KillThreadEvent;

extern Event EV_ScriptThread_SetThreadNameEvent;

extern Event EV_ScriptThread_Execute;

extern Event EV_ScriptThread_WaitDialogLength;

extern Event EV_ScriptThread_NoIntermission;

extern Event EV_ScriptThread_DontSaveOrientation;

extern Event EV_ScriptThread_DoesEntityExist;

extern Event EV_ScriptThread_TriggerEntity;


