/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


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
