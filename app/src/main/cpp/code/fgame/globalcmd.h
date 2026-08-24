
#ifndef __GLOBALCMD_H__
#define __GLOBALCMD_H__

#include "interpreter.h"
#include "g_local.h"

extern Event EV_ProcessCommands;
extern Event EV_ScriptThread_Execute;
extern Event EV_ScriptThread_Callback;
extern Event EV_ScriptThread_ThreadCallback;
extern Event EV_ScriptThread_DeathCallback;
extern Event EV_ScriptThread_CreateThread;
extern Event EV_ScriptThread_TerminateThread;
extern Event EV_ScriptThread_ControlObject;
extern Event EV_ScriptThread_Print;
extern Event EV_ScriptThread_PrintInt;
extern Event EV_ScriptThread_PrintFloat;
extern Event EV_ScriptThread_PrintVector;
extern Event EV_ScriptThread_NewLine;
extern Event EV_ScriptThread_Clear;
extern Event EV_ScriptThread_Assert;
extern Event EV_ScriptThread_Break;
extern Event EV_ScriptThread_Clear;
extern Event EV_ScriptThread_Trigger;
extern Event EV_ScriptThread_Spawn;
extern Event EV_ScriptThread_Map;
extern Event EV_ScriptThread_SetCvar;
extern Event EV_ScriptThread_CueCamera;
extern Event EV_ScriptThread_CuePlayer;
extern Event EV_ScriptThread_FreezePlayer;
extern Event EV_ScriptThread_ReleasePlayer;
extern Event EV_ScriptThread_SetCinematic;
extern Event EV_ScriptThread_SetNonCinematic;
extern Event EV_ScriptThread_SetSkipThread;
extern Event EV_ScriptThread_TargetOf;
extern Event EV_ScriptThread_GetCVar;
extern Event EV_ScriptThread_Mod;
extern Event EV_ScriptThread_Div;
extern Event EV_ScriptThread_Round;
extern Event EV_ScriptThread_VectorScale;
extern Event EV_ScriptThread_VectorNormalize;
extern Event EV_ScriptThread_VectorDot;
extern Event EV_ScriptThread_VectorCross;
extern Event EV_ScriptThread_VectorLength;
extern Event EV_ScriptThread_VectorX;
extern Event EV_ScriptThread_VectorY;
extern Event EV_ScriptThread_VectorZ;
extern Event EV_ScriptThread_VectorSetX;
extern Event EV_ScriptThread_VectorSetY;
extern Event EV_ScriptThread_VectorSetZ;
extern Event EV_ScriptThread_AnglesToForward;
extern Event EV_ScriptThread_AnglesToLeft;
extern Event EV_ScriptThread_AnglesToUp;
extern Event EV_ScriptThread_FloorEvent ;
extern Event EV_ScriptThread_CeilEvent ;
extern Event EV_ScriptThread_RoundEvent ;

#include "scriptthread.h"

#endif
