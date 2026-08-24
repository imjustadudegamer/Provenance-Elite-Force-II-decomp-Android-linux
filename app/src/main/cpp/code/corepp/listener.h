/*
===========================================================================
Copyright (C) 2008 the OpenMoHAA team

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
#if defined( GAME_DLL )
#include "../fgame/listener.h"
#else

#include "../corepp/class.h"
#include "containerclass.h"
#include "con_arrayset.h"
#include "../corepp/str.h"
#include "../corepp/vector.h"

class Entity;
class Listener;
class CThread;
class ScriptVariable;
class ScriptVariableList;
class Archiver;
class EventQueueNode;

#define ECF_ENTITY        (1 << 0)
#define ECF_SENTIENT      (1 << 2)
#define ECF_PLAYER        (1 << 3)
#define ECF_ACTOR         (1 << 4)
#define ECF_ITEM          (1 << 5)
#define ECF_INVENTORYITEM (1 << 6)
#define ECF_WEAPON        (1 << 7)
#define ECF_PROJECTILE    (1 << 8)
#define ECF_DOOR          (1 << 9)
#define ECF_CAMERA        (1 << 10)
#define ECF_VEHICLE       (1 << 11)
#define ECF_VEHICLETANK   (1 << 12)
#define ECF_VEHICLETURRET (1 << 13)
#define ECF_TURRET        (1 << 14)
#define ECF_PATHNODE      (1 << 15)
#define ECF_WAYPOINT      (1 << 16)
#define ECF_TEMPWAYPOINT  (1 << 17)
#define ECF_VEHICLEPOINT  (1 << 18)
#define ECF_SPLINEPATH    (1 << 19)
#define ECF_CRATEOBJECT   (1 << 20)
#define ECF_BOT           (1 << 21)

#define EV_CONSOLE    (1 << 0) 
#define EV_CHEAT      (1 << 1) 
#define EV_CODEONLY   (1 << 2) 
#define EV_CACHE      (1 << 3) 
#define EV_TIKIONLY   (1 << 4) 
#define EV_SCRIPTONLY (1 << 5) 
#define EV_SERVERCMD  (1 << 6) 
#define EV_DEFAULT    -1       
#define EV_ZERO       0

#define INUSE_BITS      2
#define MAX_EVENT_USE   ( ( 1 << INUSE_BITS ) - 1 )

#define EV_NORMAL 0 
#define EV_RETURN 1 
#define EV_GETTER 2 
#define EV_SETTER 3 


#define EV_REMOVE              -9.0f 
#define EV_LINKBEAMS           -9.0f 
#define EV_VEHICLE             -9.0f
#define EV_PRIORITY_SPAWNARG   -8.0f 
#define EV_SETUP_ROPEPIECE     -8.0f
#define EV_SPAWNARG            -7.0f 
#define EV_SETUP_ROPEBASE      -7.0f
#define EV_PROCESS_INIT        -6.0f
#define EV_LINKDOORS           -6.0f 
#define EV_POSTSPAWN           -5.0f 
#define EV_SPAWNENTITIES       -4.0f
#define EV_PRIORITY_SPAWNACTOR -3.0f
#define EV_SPAWNACTOR          -2.0f

#define EVENT_LEGS_ANIM   (1 << 0) 
#define EVENT_TORSO_ANIM  (1 << 1) 
#define EVENT_DIALOG_ANIM (1 << 2) 

class EventArgDef;
class EventDef;
class AbstractFormater;

class ClassEventPrinter
{
public:
    void DumpAllClasses(fileHandle_t class_file, AbstractFormater& formater, const char *title);

    void               DumpClass(
                      ClassDef                         *classDef,
                      fileHandle_t                     class_stream,
                      AbstractFormater&                 formater,
                      const Container<EventDef *>& sortedEvents
                  );
    void ClassEvents(
        const ClassDef                   *classDef,
        fileHandle_t                     class_stream,
        AbstractFormater&                 formater,
        const Container<EventDef *>& sortedEvents
    );
    void PrintDocumentation(const EventDef& def, fileHandle_t event_file, AbstractFormater& formater);
    void PrintEventDocumentation(const EventDef& def, fileHandle_t event_file, AbstractFormater& formater);
    void PrintEventArgumentRange(fileHandle_t event_file, const EventArgDef& argDef, AbstractFormater& formater);

    Container<EventArgDef> SetupDocumentation(const EventDef& def);

private:
    void SortEventList(Container<EventDef *>& eventList);
    void SortClassList(Container<ClassDef *>& classList);
};

enum vartype {
    IS_STRING,
    IS_VECTOR,
    IS_BOOLEAN,
    IS_INTEGER,
    IS_FLOAT,
    IS_ENTITY,
    IS_LISTENER
};

class EventArgDef
{
private:
    vartype type;
    str     name;
    float   minRange[3];
    bool    minRangeDefault[3];
    float   maxRange[3];
    bool    maxRangeDefault[3];
    bool    optional;

public:
    EventArgDef();

    void
    Setup(const char *eventName, const char *argName, const char *argType, const char *argRange);
    int              getType() const;
    const char *GetTypeName() const;
    const char *getName() const;
    bool             isOptional() const;
    void             GetRange(size_t& numRanges, bool& isInteger, bool& isSingle) const;
    float            GetMinRange(uintptr_t index) const;
    bool             GetMinRangeDefault(uintptr_t index) const;
    float            GetMaxRange(uintptr_t index) const;
    bool             GetMaxRangeDefault(uintptr_t index) const;
};

class AbstractFormater
{
public:
    virtual ~AbstractFormater();

    virtual void PrintDocumentHeader(fileHandle_t f, const char *title)                             = 0;
    virtual void BeginClassHeader(fileHandle_t f, const char *className, const char *classID)  = 0;
    virtual void EndClassHeader(fileHandle_t f)                                                          = 0;
    virtual void PrintClassLineage(fileHandle_t f, const char *className)                           = 0;
    virtual void BeginClassBody(fileHandle_t f)                                                          = 0;
    virtual void EndClassBody(fileHandle_t f)                                                            = 0;
    virtual void BeginEventDoc(fileHandle_t f, const char *eventName)                               = 0;
    virtual void EndEventDoc(fileHandle_t f)                                                             = 0;
    virtual void BeginEventDefinition(fileHandle_t f)                                                    = 0;
    virtual void EndEventDefinition(fileHandle_t f)                                                      = 0;
    virtual void BeginEventArgument(fileHandle_t f, bool isOptional)                                     = 0;
    virtual void EndEventArgument(fileHandle_t f, bool isOptional, bool isLast)                          = 0;
    virtual void PrintEventArgumentType(fileHandle_t f, const char *argumentType)                   = 0;
    virtual void PrintEventArgumentName(fileHandle_t f, const char *argumentName)                   = 0;
    virtual void PrintEventArgumentRangeSingle(fileHandle_t f, float minRange, bool isInteger)           = 0;
    virtual void PrintEventArgumentRange(fileHandle_t f, float minRange, float maxRange, bool isInteger) = 0;
    virtual void PrintEventDocumentation(fileHandle_t f, const char *documentation)                 = 0;
    virtual void PrintDocumentFooter(fileHandle_t f)                                                     = 0;
};

class HTMLFormater : public AbstractFormater
{
public:
    void PrintDocumentHeader(fileHandle_t f, const char *title) override;
    void BeginClassHeader(fileHandle_t f, const char *className, const char *classID) override;
    void EndClassHeader(fileHandle_t f) override;
    void BeginClassBody(fileHandle_t f) override;
    void EndClassBody(fileHandle_t f) override;
    void BeginEventDoc(fileHandle_t f, const char *eventName) override;
    void EndEventDoc(fileHandle_t f) override;
    void BeginEventDefinition(fileHandle_t f) override;
    void EndEventDefinition(fileHandle_t f) override;
    void BeginEventArgument(fileHandle_t f, bool isOptional) override;
    void EndEventArgument(fileHandle_t f, bool isOptional, bool isLast) override;
    void PrintEventArgumentType(fileHandle_t f, const char *argumentType) override;
    void PrintEventArgumentName(fileHandle_t f, const char *argumentName) override;
    void PrintEventArgumentRangeSingle(fileHandle_t f, float minRange, bool isInteger) override;
    void PrintEventArgumentRange(fileHandle_t f, float minRange, float maxRange, bool isInteger) override;
    void PrintEventDocumentation(fileHandle_t f, const char *documentation) override;
    void PrintClassLineage(fileHandle_t f, const char *className) override;
    void PrintDocumentFooter(fileHandle_t f) override;
};

class BasicFormatter : public AbstractFormater
{
public:
    void PrintDocumentHeader(fileHandle_t f, const char *title) override;
    void BeginClassHeader(fileHandle_t f, const char *className, const char *classID) override;
    void EndClassHeader(fileHandle_t f) override;
    void BeginClassBody(fileHandle_t f) override;
    void EndClassBody(fileHandle_t f) override;
    void BeginEventDoc(fileHandle_t f, const char *eventName) override;
    void EndEventDoc(fileHandle_t f) override;
    void BeginEventDefinition(fileHandle_t f) override;
    void EndEventDefinition(fileHandle_t f) override;
    void BeginEventArgument(fileHandle_t f, bool isOptional) override;
    void EndEventArgument(fileHandle_t f, bool isOptional, bool isLast) override;
    void PrintEventArgumentType(fileHandle_t f, const char *argumentType) override;
    void PrintEventArgumentName(fileHandle_t f, const char *argumentName) override;
    void PrintEventArgumentRangeSingle(fileHandle_t f, float minRange, bool isInteger) override;
    void PrintEventArgumentRange(fileHandle_t f, float minRange, float maxRange, bool isInteger) override;
    void PrintEventDocumentation(fileHandle_t f, const char *documentation) override;
    void PrintClassLineage(fileHandle_t f, const char *className) override;
    void PrintDocumentFooter(fileHandle_t f) override;
};

class EventDef
{
public:
    str                     command;
    int                     flags;
    const char             *formatspec;
    const char             *argument_names;
    const char             *documentation;
    uchar                   type;
    Container<EventArgDef> *definition;

    EventDef() { definition = NULL; }

    void Error(const char *format, ...);

    void PrintDocumentation(FILE *event_file, bool html);
    void PrintEventDocumentation(FILE *event_file, bool html);

    void DeleteDocumentation(void);
    void SetupDocumentation(void);
};

class DataNode
{
public:
    Event      *ev;
    const char *command;
    int         flags;
    const char *formatspec;
    const char *argument_names;
    const char *documentation;
    int         type;
    DataNode   *next;
};

class command_t
{
public:
    const char *command;
    int         flags;
    byte        type;

public:
    command_t();
    command_t(const char *name, byte t);

    friend bool operator==(const char *name, const command_t& command);
    friend bool operator==(const command_t& cmd1, const command_t& cmd2);
};

typedef enum {
    EV_FROM_CODE,
    EV_FROM_CONSOLE,
    EV_FROM_SCRIPT,
    EV_FROM_ANIMATION
} eventsource_t;

#define DIRTY_STRING  (1 << 0)
#define DIRTY_VECTOR  (1 << 1)
#define DIRTY_INTEGER (1 << 2)
#define DIRTY_FLOAT   (1 << 3)

#define DIRTY_ALL (0x7fff)

class EventVar : public Class
{
private:
    short  type;
    short  dirtyFlags;
    str    stringValue;
    Vector vectorValue;
    int    intValue;
    float  floatValue;

public:
    EventVar()
    {
        type       = IS_INTEGER;
        dirtyFlags = DIRTY_ALL;
        intValue   = 0;
        floatValue = 0;
    }

    // survives exactly as in retail; do not "fix" this.
    EventVar(const EventVar& ev)
    {
        type       = ev.type;
        dirtyFlags = ev.dirtyFlags;
        intValue   = ev.intValue;
        floatValue = ev.floatValue;
    }

    EventVar(const char *text);
    EventVar(const str& text);

    EventVar(int val)
    {
        type       = IS_INTEGER;
        dirtyFlags = DIRTY_ALL & ~DIRTY_INTEGER;
        intValue   = val;
        floatValue = 0;
    }

    EventVar(float val)
    {
        type       = IS_FLOAT;
        dirtyFlags = DIRTY_ALL & ~DIRTY_FLOAT;
        intValue   = 0;
        floatValue = val;
    }

    EventVar(const Vector& vec)
    {
        type        = IS_VECTOR;
        dirtyFlags  = DIRTY_ALL & ~DIRTY_VECTOR;
        vectorValue = vec;
        intValue    = 0;
        floatValue  = 0;
    }

    const char *GetToken(Event& ev);
    const char *GetString(Event& ev);
    qboolean    GetBoolean(Event& ev);
    int         GetInteger(Event& ev);
    float       GetFloat(Event& ev);
    Vector      GetVector(Event& ev);

    void SetString(const char *text);

    qboolean IsVector(Event& ev);
    qboolean IsNumeric(Event& ev);

#if defined(ARCHIVE_SUPPORTED)
    void Archive(Archiver& arc);
#endif
};

class Event : public Class
{
private:
    friend class Listener;

    typedef struct EventInfo {
        unsigned inuse : INUSE_BITS;  
        unsigned source : 2;
        unsigned flags : 9;
        unsigned linenumber : 19;     
    } EventInfo_t;

    int                     eventnum;
    EventInfo               info;
    const char             *name;
    const char             *formatspec;
    const char             *argument_names;
    const char             *documentation;
    Container<EventVar>    *data;
    Container<EventArgDef> *definition;
    EventArgDef            *returntype;

    int threadnum;

    short             anim_number = 0;
    short             anim_frame  = 0;
    SafePtr<Listener> m_sourceobject;
    float             time        = 0.0f;
    int               flags       = 0;

    friend class Level;

private:
    static DataNode *DataNodeList;

public:
    CLASS_PROTOTYPE(Event);

    static con_map<Event *, EventDef>         eventDefList;
    static con_arrayset<command_t, command_t> commandList;

    static con_map<const_str, unsigned int> normalCommandList;
    static con_map<const_str, unsigned int> returnCommandList;
    static con_map<const_str, unsigned int> getterCommandList;
    static con_map<const_str, unsigned int> setterCommandList;

    static void LoadEvents(void);

    static EventQueueNode EventQueue;

    static int NumEventCommands();

    static void ListCommands(const char *mask = NULL);
    static void ListDocumentation(const char *mask, qboolean print_to_file = qfalse);
    static void PendingEvents(const char *mask = NULL);

    static int GetEvent(str name, uchar type = EV_NORMAL);
    static int GetEventWithFlags(str name, int flags, uchar type = EV_NORMAL);

    static command_t  *GetEventInfo(int eventnum);
    static int         GetEventFlags(int eventnum);
    static const char *GetEventName(int index);

    static int  compareEvents(const void *arg1, const void *arg2);
    static void SortEventList(Container<int> *sortedList);

    virtual void ErrorInternal(Listener *l, str text) const;

    static bool         Exists(const char *command);
    static unsigned int FindEventNum(const char *s);
    static unsigned int FindNormalEventNum(const_str s);
    static unsigned int FindNormalEventNum(str s);
    static unsigned int FindReturnEventNum(const_str s);
    static unsigned int FindReturnEventNum(str s);
    static unsigned int FindSetterEventNum(const_str s);
    static unsigned int FindSetterEventNum(str s);
    static unsigned int FindGetterEventNum(const_str s);
    static unsigned int FindGetterEventNum(str s);

    bool operator==(Event ev) { return eventnum == ev.eventnum; }

    bool operator!=(Event ev) { return eventnum != ev.eventnum; }

#ifndef _DEBUG_MEM
    void *operator new(size_t size);
    void  operator delete(void *ptr);
#endif

    Event();
    Event(const Event& ev);
    Event(Event *ev);
    Event(int index);
    Event(const char *command);
    Event(const str& command);
    Event(
        const char *command,
        int         ev_flags,
        const char *
            theFormatspec, 
        const char *theArgument_names,
        const char *theDocumentation,
        byte        type = EV_NORMAL
    );

    Event& operator=(const Event& ev);

    ~Event();

#if defined(ARCHIVE_SUPPORTED)
    void Archive(Archiver& arc) override;
#endif

#ifdef _GAME_DLL
    eventInfo_t *getInfo();
#else
    EventDef *getInfo();
#endif

    const char *getName() const;

    void ArchiveRetail(Archiver& arc, SafePtrBase& sourceobject, float& evTime, int& evFlags);
    void ArchiveRetail(Archiver& arc);

    // NEVER throws -- this is what removes the ScriptException dependency from the

    void SetSource(eventsource_t source) { info.source = (unsigned)source; }
    void SetLineNumber(unsigned int linenumber) { info.linenumber = linenumber; }
    eventsource_t GetSource(void) const { return (eventsource_t)info.source; }

    int GetLineNumber(void) const
    {
        if (info.source == EV_FROM_SCRIPT) {
            return info.linenumber;
        }

        return 0;
    }

    void SetSourceObject(Listener *source) { m_sourceobject = source; }
    Listener *GetSourceObject(void) const { return m_sourceobject; }
    SafePtr<Listener> *GetSourceObjectPointer(void) { return &m_sourceobject; }

    int  GetAnimationNumber(void) const { return anim_number; }
    int  GetAnimationFrame(void) const { return anim_frame; }
    void SetAnimationNumber(int anim) { anim_number = anim; }
    void SetAnimationFrame(int frame) { anim_frame = frame; }

    int GetFlags(void) const { return info.flags; }

    operator int() { return eventnum; }
    operator const char *() { return getName(); }

    int NumArgs(void);

    qboolean IsVectorAt(int pos);
    qboolean IsEntityAt(int pos);
    qboolean IsNumericAt(int pos);

    const char *GetToken(int pos);
    const char *GetString(int pos);
    int         GetInteger(int pos);
    float       GetFloat(int pos);
    Vector      GetVector(int pos);
    bool        GetBoolean(int pos);

    void SetToken(int pos, const char *text);
    void SetString(int pos, const str& text);
    void SetInteger(int pos, int value);
    void SetFloat(int pos, float value);
    void SetVector(int pos, const Vector& vector);
    void SetBoolean(int pos, bool value);

    void AddToken(const char *text);
    void AddTokens(int argc, const char **argv);
    void AddString(const char *text);
    void AddString(const str& text);
    void AddInteger(int val);
    void AddFloat(float val);
    void AddVector(const Vector& vec);

#ifdef WITH_SCRIPT_ENGINE
    void ReturnString(const char *text);
    void ReturnFloat(float value);
    void ReturnInteger(int value);
    void ReturnVector(const Vector& vec);
    void ReturnEntity(Entity *ent);

    void     SetThread(CThread *thread);
    CThread *GetThread(void);
#endif

    void Error(const char *format, ...) Q_PRINTF_FUNC(2, 3);
};

#define NODE_CANCEL      1
#define NODE_FIXED_EVENT 2

class EventQueueNode : public LightClass
{
public:
    Event            *event;
    int               inttime;
    int               flags;
    SafePtr<Listener> m_sourceobject;

    EventQueueNode *prev;
    EventQueueNode *next;

#ifdef _DEBUG
    const char *name;
#endif

    EventQueueNode()
    {
        prev = this;
        next = this;

#ifdef _DEBUG
        name = NULL;
#endif
    }

    Listener *GetSourceObject(void) { return m_sourceobject; }

    void SetSourceObject(Listener *obj) { m_sourceobject = obj; }
};

template<class Type1, class Type2>
class con_map;

using ConList  = ContainerClass<SafePtr<Listener>>;
using eventMap = con_map<Event *, EventDef *>;

using ListenerPtr = SafePtr<Listener>;

class Listener : public Class
{
public:
#ifdef WITH_SCRIPT_ENGINE
    con_set<const_str, ConList> *m_NotifyList;
    con_set<const_str, ConList> *m_WaitForList;
    con_set<const_str, ConList> *m_EndList;
    ScriptVariableList          *vars;
#endif

    static bool EventSystemStarted;
    static bool ProcessingEvents;

private:
#ifdef WITH_SCRIPT_ENGINE
    void ExecuteScriptInternal(Event *ev, ScriptVariable& scriptVariable);
    void ExecuteThreadInternal(Event *ev, ScriptVariable& returnValue);
    void WaitExecuteScriptInternal(Event *ev, ScriptVariable& returnValue);
    void WaitExecuteThreadInternal(Event *ev, ScriptVariable& returnValue);
#endif

    EventQueueNode *PostEventInternal(Event *ev, float delay, int flags);

public:
    CLASS_PROTOTYPE(Listener);

#ifdef WITH_SCRIPT_ENGINE
    virtual CThread *CreateThreadInternal(const ScriptVariable& label);
    virtual CThread *CreateScriptInternal(const ScriptVariable& label);
    virtual void          StoppedNotify(void);
    virtual void          StartedWaitFor(void);
    virtual void          StoppedWaitFor(const_str name, bool bDeleting);
#endif

    virtual Listener *GetScriptOwner(void);
    virtual void      SetScriptOwner(Listener *newOwner);

    Listener();
    virtual ~Listener();

    void Archive(Archiver& arc) override;

    void CancelEventsOfType(Event *ev);
    void CancelEventsOfType(Event& ev);
    void CancelFlaggedEvents(int flags);
    void CancelPendingEvents(void);

    qboolean EventPending(Event& ev);

    void PostEvent(Event *ev, float delay, int flags = 0);
    void PostEvent(const Event& ev, float delay, int flags = 0);

    qboolean PostponeAllEvents(float time);
    qboolean PostponeEvent(Event& ev, float time);

    bool            ProcessEvent(const Event& ev);
    bool            ProcessEvent(Event *ev);
    bool            ProcessEvent(Event& ev);

    void ProcessContainerEvent(const Container<Event *>& conev);

    qboolean ProcessPendingEvents(void);

    bool ProcessScriptEvent(Event& ev);
    bool ProcessScriptEvent(Event *ev);

#ifdef WITH_SCRIPT_ENGINE

    void                CreateVars(void);
    void                ClearVars(void);
    ScriptVariableList *Vars(void);

    bool BroadcastEvent(Event& event, ConList *listeners);
    bool BroadcastEvent(str name, Event& event);
    bool BroadcastEvent(const_str name, Event& event);
    void CancelWaiting(str name);
    void CancelWaiting(const_str name);
    void CancelWaitingAll(void);
    void CancelWaitingSources(const_str name, ConList& listeners, ConList& stoppedListeners);

    void ExecuteThread(str scriptName, str label, Event *params = NULL);
    void ExecuteThread(str scriptName, str label, Event& params);

    void EndOn(str name, Listener *listener);
    void EndOn(const_str name, Listener *listener);
    void Notify(const char *name);
    void Register(str name, Listener *listener);
    void Register(const_str name, Listener *listener);
    void RegisterSource(const_str name, Listener *listener);
    void RegisterTarget(const_str name, Listener *listener);
    void Unregister(str name);
    void Unregister(const_str name);
    void Unregister(str name, Listener *listener);
    void Unregister(const_str name, Listener *listener);
    void UnregisterAll(void);
    bool UnregisterSource(const_str name, Listener *listener);
    bool UnregisterTarget(const_str name, Listener *listener);
    void UnregisterTargets(
        const_str name, ConList& listeners, ConList& stoppedListeners, Container<const_str>& stoppedNames
    );
    void AbortRegistration(const_str name, Listener *l);

    int RegisterSize(const_str name) const;
    int RegisterSize(str name) const;
    int WaitingSize(const_str name) const;
    int WaitingSize(str name) const;

#endif

    int      GetFlags(Event *event) const;
    qboolean ValidEvent(str name) const;

    void Remove(Event *ev);
    void ScriptRemove(Event *ev);

#ifdef WITH_SCRIPT_ENGINE
    void CancelFor(Event *ev);
    void CreateReturnThread(Event *ev);
    void CreateThread(Event *ev);
    void ExecuteReturnScript(Event *ev);
    void ExecuteScript(Event *ev);
    void EventDelayThrow(Event *ev);
    void EventEndOn(Event *ev);
    void EventGetOwner(Event *ev);
    void EventNotify(Event *ev);
    void EventThrow(Event *ev);
    void EventUnregister(Event *ev);
    void WaitCreateReturnThread(Event *ev);
    void WaitCreateThread(Event *ev);
    void WaitExecuteReturnScript(Event *ev);
    void WaitExecuteScript(Event *ev);
#endif
};

qboolean IsNumeric(const char *str);

extern Event EV_DelayThrow;
extern Event EV_Remove;
extern Event EV_ScriptRemove;
extern Event EV_Throw;

extern Event EV_Listener_CreateThread;
extern Event EV_Listener_CreateReturnThread;
extern Event EV_Listener_ExecuteReturnScript;
extern Event EV_Listener_ExecuteScript;
extern Event EV_Listener_WaitCreateReturnThread;

extern int DisableListenerNotify;

extern cvar_t *g_showevents;
extern cvar_t *g_eventlimit;
extern cvar_t *g_timeevents;
extern cvar_t *g_watch;
extern cvar_t *g_eventstats;

extern MEM_BlockAlloc<Event> Event_allocator;

#if defined(GAME_DLL)
#    define EVENT_DebugPrintf gi.DebugPrintf
#    define EVENT_DPrintf     gi.DPrintf
#    define EVENT_Printf      gi.Printf
#    define EVENT_WDPrintf    gi.WDPrintf
#    define EVENT_time        level.time
#    define EVENT_msec        level.inttime
#    define EVENT_realtime    gi.Milliseconds()
#    define EVENT_Error       gi.Error

#    define EVENT_FILENAME    "events.txt"

#elif defined(CGAME_DLL)
#    define EVENT_DebugPrintf cgi.DebugPrintf
#    define EVENT_DPrintf     cgi.DPrintf
#    define EVENT_Printf      cgi.Printf
#    define EVENT_WDPrintf    cgi.Com_WDPrintf
#    define EVENT_time        (((float)cg.time / 1000.0f))
#    define EVENT_msec        cg.time
#    define EVENT_realtime    cgi.Milliseconds()
#    define EVENT_Error       cgi.Error

#    define EVENT_FILENAME    "cg_events.txt"

#elif defined(UI_LIB)

#    define EVENT_DebugPrintf Com_DebugPrintf
#    define EVENT_DPrintf     Com_DPrintf
#    define EVENT_Printf      Com_Printf
#    define EVENT_WDPrintf    Com_WDPrintf
#    define EVENT_time        (((float)cls.realtime / 1000.0f))
#    define EVENT_msec        cls.realtime
#    define EVENT_realtime    Sys_Milliseconds()
#    define EVENT_Error       Com_Error

#    define EVENT_FILENAME    "ui_events.txt"

#else
#    define EVENT_DebugPrintf Com_DebugPrintf
#    define EVENT_DPrintf     Com_DPrintf
#    define EVENT_Printf      Com_Printf
#    define EVENT_WDPrintf    Com_WDPrintf
#    define EVENT_time        (((float)cls.realtime / 1000.0f))
#    define EVENT_msec        cls.realtime
#    define EVENT_realtime    Sys_Milliseconds()
#    define EVENT_Error       Com_Error

#    define EVENT_FILENAME    "cl_events.txt"
#endif

void L_ClearEventList();
bool L_EventSystemStarted(void);
void L_InitEvents(void);
void L_ProcessPendingEvents();
void L_ShutdownEvents(void);
void L_ArchiveEvents(Archiver& arc);
void L_UnarchiveEvents(Archiver& arc);

#endif 