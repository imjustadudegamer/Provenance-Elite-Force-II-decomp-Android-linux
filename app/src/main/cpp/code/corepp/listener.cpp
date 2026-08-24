/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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


#include "../script/scriptvariable.h"
#include "../script/scriptexception.h"
#include "Linklist.h"

#if defined(GAME_DLL)
#include "../fgame/g_main.h" 
#elif defined(CGAME_DLL)
#include "../cgame/cg_local.h" 
#endif

#ifdef WITH_SCRIPT_ENGINE
#    include "../fgame/archive.h"
#    include "../fgame/scriptmaster.h"
#    include "../fgame/scriptthread.h"
#    include "../fgame/program.h"
#    include "../script/scriptvariable.h"
#endif

#if defined(GAME_DLL)

#    include "../fgame/player.h"
#    include "../fgame/consoleevent.h"
#    include "../fgame/animationevent.h"

#    define LISTENER_Cvar_Get gi.Cvar_Get

#elif defined(CGAME_DLL)

#    define LISTENER_Cvar_Get cgi.Cvar_Get

#elif defined(UI_LIB)
#else

#    define LISTENER_Cvar_Get Cvar_Get

#    include "../client/client.h"
#endif

#include <utility>
#include <algorithm>

DataNode                          *Event::DataNodeList = NULL;
con_map<Event *, EventDef>         Event::eventDefList;
con_arrayset<command_t, command_t> Event::commandList;

#ifdef WITH_SCRIPT_ENGINE

con_map<const_str, unsigned int> Event::normalCommandList;
con_map<const_str, unsigned int> Event::returnCommandList;
con_map<const_str, unsigned int> Event::getterCommandList;
con_map<const_str, unsigned int> Event::setterCommandList;

#endif

EventQueueNode Event::EventQueue;

int DisableListenerNotify = 0;

bool Listener::EventSystemStarted = false;
bool Listener::ProcessingEvents = false;

Event EV_Listener_CancelFor
(
    "cancelFor",
    EV_DEFAULT,
    "s",
    "name",
    "Cancel for event of type name.",
    EV_NORMAL
);

Event EV_Listener_CreateReturnThread
(
    "thread",
    EV_DEFAULT,
    "s",
    "label",
    "Creates a thread starting at label.",
    EV_RETURN
);

Event EV_Listener_CreateThread
(
    "thread",
    EV_DEFAULT,
    "s",
    "label",
    "Creates a thread starting at label.",
    EV_NORMAL
);

Event EV_Listener_RunThreadNamed
(
    "runthreadnamed",
    EV_DEFAULT,
    "s",
    "label",
    "Creates and runs a thread starting at the named label (EF2).",
    EV_NORMAL
);

Event EV_Listener_RunThreadNamedReturn
(
    "runthreadnamed",
    EV_DEFAULT,
    "s",
    "label",
    "Creates and runs a thread starting at the named label (EF2).",
    EV_RETURN
);

Event EV_Listener_ExecuteReturnScript
(
    "exec",
    EV_DEFAULT,
    "s",
    "script",
    "Executes the specified script.",
    EV_RETURN
);

Event EV_Listener_ExecuteScript
(
    "exec",
    EV_DEFAULT,
    "s",
    "script",
    "Executes the specified script.",
    EV_NORMAL
);

Event EV_Remove
(
    "immediateremove",
    EV_DEFAULT,
    NULL,
    NULL,
    "Removes this listener immediately.",
    EV_NORMAL
);

Event EV_ScriptRemove
(
    "remove",
    EV_DEFAULT,
    NULL,
    NULL,
    "Removes this listener the next time events are processed.",
    EV_NORMAL
);

Event EV_Listener_EndOn
(
    "endon",
    EV_DEFAULT,
    "s",
    "name",
    "Ends the function when the specified event is triggered.",
    EV_NORMAL
);

Event EV_Listener_Notify
(
    "notify",
    EV_DEFAULT,
    "s",
    "name",
    "Triggers an event. An undefined event will be automatically created by calling waittill or endon.",
    EV_NORMAL
);

Event EV_Listener_GetOwner
(
    "owner",
    EV_DEFAULT,
    NULL,
    NULL,
    "Returns the owner.",
    EV_GETTER
);

Event EV_DelayThrow
(
    "delaythrow",
    EV_DEFAULT,
    "s",
    "label",
    "Internal usage.",
    EV_NORMAL
);

Event EV_Throw
(
    "throw",
    EV_DEFAULT,
    "s",
    "label",
    "Throws to the specified label.",
    EV_NORMAL
);

Event EV_Listener_Unregister
(
    "unregister",
    EV_DEFAULT,
    "s",
    "label",
    "Unregisters the label from the event of the same name.",
    EV_NORMAL
);

Event EV_Listener_WaitCreateReturnThread
(
    "waitthread",
    EV_DEFAULT,
    "s",
    "label",
    "Creates a thread starting at label and waits until the called thread is finished.",
    EV_RETURN
);

Event EV_Listener_WaitCreateThread
(
    "waitthread",
    EV_DEFAULT,
    "s",
    "label",
    "Creates a thread starting at label and waits until the called thread is finished.",
    EV_NORMAL
);

Event EV_Listener_WaitExecuteReturnScript
(
    "waitexec",
    EV_DEFAULT,
    "s",
    "script",
    "Executes the specified script and waits until the called thread group is finished.",
    EV_RETURN
);

Event EV_Listener_WaitExecuteScript
(
    "waitexec",
    EV_DEFAULT,
    "s",
    "script",
    "Executes the specified script and waits until the called thread group is finished.",
    EV_NORMAL
);

CLASS_DECLARATION(Class, Listener, NULL) {
    {&EV_Remove,                           &Listener::Remove                 },
    {&EV_ScriptRemove,                     &Listener::ScriptRemove           },

#ifdef WITH_SCRIPT_ENGINE
    {&EV_Listener_CancelFor,               &Listener::CancelFor              },
    {&EV_Listener_CreateReturnThread,      &Listener::CreateReturnThread     },
    {&EV_Listener_CreateThread,            &Listener::CreateThread           },
    {&EV_Listener_RunThreadNamed,          &Listener::CreateThread           },
    {&EV_Listener_RunThreadNamedReturn,    &Listener::CreateReturnThread     },
    {&EV_Listener_ExecuteReturnScript,     &Listener::ExecuteReturnScript    },
    {&EV_Listener_ExecuteScript,           &Listener::ExecuteScript          },
    {&EV_Listener_EndOn,                   &Listener::EventEndOn             },
    {&EV_Listener_GetOwner,                &Listener::EventGetOwner          },
    {&EV_Listener_Notify,                  &Listener::EventNotify            },
    {&EV_DelayThrow,                       &Listener::EventDelayThrow        },
    {&EV_Throw,                            &Listener::EventThrow             },
    {&EV_Listener_Unregister,              &Listener::EventUnregister        },
    {&EV_Listener_WaitCreateReturnThread,  &Listener::WaitCreateReturnThread },
    {&EV_Listener_WaitCreateThread,        &Listener::WaitCreateThread       },
    {&EV_Listener_WaitExecuteReturnScript, &Listener::WaitExecuteReturnScript},
    {&EV_Listener_WaitExecuteScript,       &Listener::WaitExecuteScript      },
#endif
    {NULL,                                 NULL                              }
};

cvar_t *g_showevents;
cvar_t *g_eventlimit;
cvar_t *g_timeevents;
cvar_t *g_watch;
cvar_t *g_eventstats;

template<>
int HashCode<Event *>(Event *const& key)
{
    return (int)(size_t)key;
}

template<>
int HashCode<command_t>(const command_t& key)
{
    const char *p;
    int         hash = 0;

    if (key.type == EV_NORMAL || key.type == EV_RETURN) {
        for (p = key.command; *p; p++) {
            hash = tolower(*p) + 31 * hash;
        }
    } else {
        for (p = key.command; *p; p++) {
            hash = *p + 31 * hash;
        }
    }

    return hash;
}

#if defined(ARCHIVE_SUPPORTED)

void ArchiveListenerPtr(Archiver& arc, SafePtr<Listener> *obj)
{
    arc.ArchiveSafePointer(obj);
}

template<>
void ConList::Archive(Archiver& arc)
{
    value.Archive(arc, ArchiveListenerPtr);
}

template<>
void con_set<const_str, ConList>::Entry::Archive(Archiver& arc)
{
    Director.ArchiveString(arc, key);
    value.Archive(arc);
}

void EventVar::Archive(Archiver& arc)
{
    arc.ArchiveShort(&type);
    arc.ArchiveShort(&dirtyFlags);
    arc.ArchiveString(&stringValue);
    arc.ArchiveVector(&vectorValue);
    arc.ArchiveInteger(&intValue);
    arc.ArchiveFloat(&floatValue);
}

void Event::Archive(Archiver& arc)
{
    ArchiveRetail(arc);
}

void Event::ArchiveRetail(Archiver& arc)
{
    ArchiveRetail(arc, m_sourceobject, time, flags);
}

void Event::ArchiveRetail(Archiver& arc, SafePtrBase& sourceobject, float& evTime, int& evFlags)
{
    str name;
    int num = 0;

    if (arc.Saving()) {
        const char *n = getName();
        name          = n ? n : "";
    }
    arc.ArchiveString(&name);
    if (arc.Loading()) {
        int num2 = Event::GetEvent(name);
        eventnum = (num2 > 0 ? num2 : 0);
        this->name = GetEventName(eventnum);
    }

    arc.ArchiveRaw(&info, sizeof(info));
    arc.ArchiveInteger(&threadnum);
    arc.ArchiveShort(&anim_number);
    arc.ArchiveShort(&anim_frame);
    arc.ArchiveSafePointer(&sourceobject);
    arc.ArchiveFloat(&evTime);
    arc.ArchiveInteger(&evFlags);

    if (arc.Saving()) {
        num = data ? data->NumObjects() : 0;
    }
    arc.ArchiveInteger(&num);
    if (arc.Loading() && num) {
        data = new Container<EventVar>;
        data->Resize(num);
    }

    for (int i = 1; i <= num; i++) {
        EventVar var;

        if (arc.Saving()) {
            var = data->ObjectAt(i);
        }
        var.Archive(arc);
        if (arc.Loading()) {
            data->AddObject(var);
        }
    }

    num = 0;
    arc.ArchiveInteger(&num);
    for (int i = 0; i < num; i++) {
        int   dtype = 0;
        str   dname;
        float f     = 0.0f;
        int   b     = 0;

        arc.ArchiveInteger(&dtype);
        arc.ArchiveString(&dname);
        for (int k = 0; k < 3; k++) {
            arc.ArchiveFloat(&f);
        }
        for (int k = 0; k < 3; k++) {
            arc.ArchiveInteger(&b);
        }
        for (int k = 0; k < 3; k++) {
            arc.ArchiveFloat(&f);
        }
        for (int k = 0; k < 3; k++) {
            arc.ArchiveInteger(&b);
        }
        arc.ArchiveInteger(&b);
    }

    num = 0;
    arc.ArchiveInteger(&num);
}

void L_ArchiveEvents(Archiver& arc)
{
    EventQueueNode *event;
    int             num;

    num = 0;
    for (event = Event::EventQueue.next; event != &Event::EventQueue; event = event->next) {
        Listener *obj;

        assert(event);

        obj = event->GetSourceObject();

        assert(obj);

#    if defined(GAME_DLL)
        if (obj->isSubclassOf(Entity) && (((Entity *)obj)->flags & FL_DONTSAVE)) {
            continue;
        }
#    endif

        num++;
    }

    arc.ArchiveInteger(&num);
    for (event = Event::EventQueue.next; event != &Event::EventQueue; event = event->next) {
        Listener *obj;

        assert(event);

        obj = event->GetSourceObject();

        assert(obj);

#    if defined(GAME_DLL)
        if (obj->isSubclassOf(Entity) && (((Entity *)obj)->flags & FL_DONTSAVE)) {
            continue;
        }
#    endif

        // to append here was four bytes per event that retail does not have.
        float evTime = (float)event->inttime / 1000.0f;

        event->event->ArchiveRetail(arc, event->m_sourceobject, evTime, event->flags);
        arc.ArchiveFloat(&evTime);
        arc.ArchiveInteger(&event->flags);
    }
}

void L_UnarchiveEvents(Archiver& arc)
{
    EventQueueNode *node;
    int             i, numEvents;

    L_ClearEventList();

    arc.ArchiveInteger(&numEvents);
    for (i = 0; i < numEvents; i++) {
        float evTime = 0.0f;

        node        = new EventQueueNode();
        node->event = new Event();

        node->event->ArchiveRetail(arc, node->m_sourceobject, evTime, node->flags);
        arc.ArchiveFloat(&evTime);
        arc.ArchiveInteger(&node->flags);

        node->inttime = (int)(evTime * 1000.0f);

        LL_Add(&Event::EventQueue, node, next, prev);
    }
}
#endif

void L_ClearEventList()
{
    EventQueueNode *node = Event::EventQueue.next, *tmp;

    while (node != &Event::EventQueue) {
        tmp = node->next;

        delete node->event;
        delete node;

        node = tmp;
    }

    LL_Reset(&Event::EventQueue, next, prev);

    Event_allocator.FreeAll();

}

bool L_EventSystemStarted(void)
{
    return Listener::EventSystemStarted;
}

void L_InitEvents(void)
{
    g_showevents = LISTENER_Cvar_Get("g_showevents", "0", 0);
    g_eventlimit = LISTENER_Cvar_Get("g_eventlimit", "5000", 0);
    g_timeevents = LISTENER_Cvar_Get("g_timeevents", "0", 0);
    g_watch      = LISTENER_Cvar_Get("g_watch", "0", 0);
    g_eventstats = LISTENER_Cvar_Get("g_eventstats", "0", 0);

    Event::LoadEvents();
    ClassDef::BuildEventResponses();

    LL_Reset(&Event::EventQueue, next, prev);

    L_ClearEventList();
    Listener::EventSystemStarted = true;
}

void L_ProcessPendingEvents()
{
    EventQueueNode *node;

    Listener::ProcessingEvents = true;

    int t = EVENT_msec;
    while (!LL_Empty(&Event::EventQueue, next, prev)) {
        Listener *obj;

        node = Event::EventQueue.next;

        assert(node);

        obj = node->GetSourceObject();

        assert(obj);

        if (node->inttime > t) {
            break;
        }

        LL_Remove(node, next, prev);

        obj->ProcessEvent(node->event);

        delete node;
    }

    Listener::ProcessingEvents = false;
}

void L_ShutdownEvents(void)
{
    if (!Listener::EventSystemStarted) {
        return;
    }

    L_ClearEventList();

    Event::commandList.clear();
    Event::eventDefList.clear();
#ifdef WITH_SCRIPT_ENGINE
    Event::normalCommandList.clear();
    Event::returnCommandList.clear();
    Event::getterCommandList.clear();
    Event::setterCommandList.clear();
#endif

    Listener::EventSystemStarted = false;
}


struct EventCompare
{
	bool operator()(const EventDef* d1, const EventDef* d2)
	{
		return str::icmp(d1->command, d2->command) < 0;
	}
};

struct ClassCompare
{
	bool operator()(const ClassDef* c1, const ClassDef* c2)
	{
		return str::icmp(c1->classname, c2->classname) < 0;
	}
};

void ClassEventPrinter::DumpAllClasses(fileHandle_t class_file, AbstractFormater& formater, const char* title)
{
	formater.PrintDocumentHeader(class_file, title);



	Container<EventDef*> sortedEvents;
	Container<ClassDef*> sortedClasses;

	SortEventList(sortedEvents);
	SortClassList(sortedClasses);

	const size_t num = sortedClasses.NumObjects();

	for (uintptr_t i = 1; i <= num; i++)
	{
		ClassDef* c = sortedClasses.ObjectAt(i);
		DumpClass(c, class_file, formater, sortedEvents);
	}

	formater.PrintDocumentFooter(class_file);
}

void ClassEventPrinter::SortClassList(Container<ClassDef*>& sortedList)
{
	const size_t num = ClassDef::numclasses;
	sortedList.Resize(num);

	for (ClassDef* c = ClassDef::classlist; c; c = c->next)
	{
		sortedList.AddObject(c);
	}

	std::sort(sortedList.AddressOfObjectAt(1), sortedList.AddressOfObjectAt(num) + 1, ClassCompare());
}

void ClassEventPrinter::SortEventList(Container<EventDef*>& sortedList)
{
    con_map_enum<Event *, EventDef> en = Event::eventDefList;
	const size_t num = Event::NumEventCommands();
	sortedList.Resize(num);

	for (EventDef* def = en.NextValue(); en.CurrentValue(); def = en.NextValue())
	{
		sortedList.AddObject(def);
	}

	std::sort(sortedList.AddressOfObjectAt(1), sortedList.AddressOfObjectAt(num) + 1, EventCompare());
}

void ClassEventPrinter::ClassEvents(const ClassDef* classDef, fileHandle_t class_file, AbstractFormater& formater, const Container<EventDef*>& sortedEvents)
{
	const ResponseDef<Class>* response = classDef->responses;
	if (response && response->event)
	{
		const size_t sortedNum = sortedEvents.NumObjects();
		for (uintptr_t i = 1; i <= sortedNum; ++i)
		{
			const EventDef* const sortedDef = sortedEvents.ObjectAt(i);

			for (const ResponseDef<Class>* r = classDef->responses; r->event; ++r)
			{
                const EventDef& def = Event::eventDefList[r->event];

				if (r->response && &def == sortedDef)
				{
					PrintEventDocumentation(def, class_file, formater);
					break;
				}
			}
		}
	}
}

void ClassEventPrinter::DumpClass(ClassDef* classDef, fileHandle_t class_file, AbstractFormater& formater, const Container<EventDef*>& sortedEvents)
{
	formater.BeginClassHeader(class_file, classDef->classname, classDef->classID);

	for (const ClassDef* c = classDef->super; c; c = c->super)
	{
		formater.PrintClassLineage(class_file, c->classname);
	}

	formater.EndClassHeader(class_file);

	formater.BeginClassBody(class_file);

	ClassEvents(classDef, class_file, formater, sortedEvents);

	formater.EndClassBody(class_file);
}

void ClassEventPrinter::PrintDocumentation(const EventDef& def, fileHandle_t event_file, AbstractFormater& formater)
{
	const char* name = def.command;

	formater.BeginEventDoc(event_file, name);

	const Container<EventArgDef> definition = SetupDocumentation(def);

	if (definition.NumObjects())
	{
		formater.BeginEventDefinition(event_file);

		for (int i = 1; i <= definition.NumObjects(); i++)
		{
			const EventArgDef& argDef = definition.ObjectAt(i);
			const bool isOptional = argDef.isOptional();

			formater.BeginEventArgument(event_file, isOptional);

			formater.PrintEventArgumentType(event_file, argDef.GetTypeName());
			formater.PrintEventArgumentName(event_file, argDef.getName());

			PrintEventArgumentRange(event_file, argDef, formater);

			formater.EndEventArgument(event_file, isOptional, i >= definition.NumObjects());
		}

		formater.EndEventDefinition(event_file);
	}

	formater.EndEventDoc(event_file);

	const char* documentation = def.documentation;
	if (documentation)
	{
		formater.PrintEventDocumentation(event_file, documentation);
	}
}

void ClassEventPrinter::PrintEventArgumentRange(fileHandle_t event_file, const EventArgDef& argDef, AbstractFormater& formater)
{
	size_t numRanges;
	bool integer, single;
	argDef.GetRange(numRanges, integer, single);

	for (uintptr_t i = 0; i < numRanges; i++)
	{
		if (single)
		{
			if (!argDef.GetMinRangeDefault(i))
			{
				formater.PrintEventArgumentRangeSingle(event_file, argDef.GetMinRange(i), integer);
			}
		}
		else
		{
			if (!argDef.GetMinRangeDefault(i) && !argDef.GetMaxRangeDefault(i))
			{
				formater.PrintEventArgumentRange(event_file, argDef.GetMinRange(i), argDef.GetMaxRange(i), integer);
			}
			else if (!argDef.GetMinRangeDefault(i) && argDef.GetMaxRangeDefault(i))
			{
				formater.PrintEventArgumentRange(event_file, argDef.GetMinRange(i), -1, integer);
			}
			else if (argDef.GetMinRangeDefault(i) && !argDef.GetMaxRangeDefault(i))
			{
				formater.PrintEventArgumentRange(event_file, -1, argDef.GetMaxRange(i), integer);
			}
		}
	}
}

void ClassEventPrinter::PrintEventDocumentation(const EventDef& def, fileHandle_t event_file, AbstractFormater& formater)
{
	if (def.flags & EV_CODEONLY)
	{
		return;
	}

	if (def.command[0] == '_')
	{
		return;
	}

	PrintDocumentation(def, event_file, formater);
}

Container<EventArgDef> ClassEventPrinter::SetupDocumentation(const EventDef& def)
{
	const char* name = def.command;
	const char* formatspec = def.formatspec;
	const char* argument_names = def.argument_names;

	Container<EventArgDef> definition;

	if (formatspec)
	{
		if (argument_names)
		{
			Container<str> argNames;

			const char* specPtr = formatspec;
			if (argument_names && argument_names[0])
			{
				const char* argStart = argument_names;
				const char* p = argStart;
				while (*p)
				{
					if (*p == ' ')
					{
						argNames.AddObject(str(argStart, p - argStart));
						while (*p == ' ') ++p;
						argStart = p;
					}
					else {
						p++;
					}
				}

				argNames.AddObject(str(argStart, p - argStart));
			}

			int index = 0;
			size_t specLength = strlen(formatspec);

			if (specLength && (*specPtr == '@'))
			{
				if (specLength < 2)
				{
					specLength = 0;
				}
				else
				{
					specPtr += 2;
					specLength -= 2;
					index++;   
				}
			}

			definition.Resize(argNames.NumObjects());

			while (specLength)
			{
				str rangeSpec = "";
				str argSpec = "";
				argSpec += *specPtr;
				specPtr++;
				specLength--;
				while (*specPtr == '[')
				{
					while (specLength && (*specPtr != ']'))
					{
						rangeSpec += *specPtr;
						specPtr++;
						specLength--;
					}
					if (specLength && (*specPtr == ']'))
					{
						rangeSpec += *specPtr;
						specPtr++;
						specLength--;
					}
				}
				if (index < argNames.NumObjects())
				{
					str argName = argNames.ObjectAt(index + 1);
					definition.AddObject();
					EventArgDef* argDef = &definition.ObjectAt(definition.NumObjects());
					argDef->Setup(name, argName.c_str(), argSpec.c_str(), rangeSpec.c_str());
				}
				else
				{
					assert(0);
				}
				index++;
			}
			if (index < argNames.NumObjects())
			{
				assert(0);
			}
		}
	}

	return definition;
}

void EV_Print(FILE *stream, const char *format, ...) Q_PRINTF_FUNC(2, 3);
void EV_Print(FILE *stream, const char *format, ...)
{
    char    buffer[1000];
    va_list va;

    va_start(va, format);

    Q_vsnprintf(buffer, sizeof(buffer), format, va);

    if (stream) {
        fprintf(stream, "%s", buffer);
    } else {
        EVENT_DPrintf("%s", buffer);
    }

    va_end(va);
}

void EV_Print(fileHandle_t stream, const char *format, ...) Q_PRINTF_FUNC(2, 3);
void EV_Print(fileHandle_t stream, const char *format, ...)
{
    char    buffer[1000];
    int     len;
    va_list va;

    va_start(va, format);

    len = Q_vsnprintf(buffer, sizeof(buffer), format, va);

    if (stream) {
#if defined(CGAME_DLL)
        cgi.FS_Write(buffer, len, stream);
#elif defined(GAME_DLL)
        gi.FS_Write(buffer, len, stream);
#else
        FS_Write(buffer, len, stream);
#endif
    } else {
        EVENT_DPrintf("%s", buffer);
    }

    va_end(va);
}

void EV_WriteToFile(fileHandle_t f, const char *data, size_t len)
{
    if (f) {
#if defined(GAME_DLL)
    gi.FS_Write(data, len, f);
#elif defined (CGAME_DLL)
    cgi.FS_Write(data, len, f);
#else
    FS_Write(data, len, f);
#endif
    } else {
        EVENT_DPrintf("%.*s", (int)len, data);
    }
}

void EV_WriteToFile(fileHandle_t f, const char *data)
{
    EV_WriteToFile(f, data, strlen(data));
}

EventArgDef::EventArgDef()
{
	type = vartype::IS_INTEGER;
	optional = false;
};

void EventArgDef::Setup(const char* eventName, const char* argName, const char* argType, const char* argRange)
{
	char        scratch[256];
	const char* ptr;
	char* tokptr;
	const char* endptr;
	int         index;

	name = argName;

	if (isupper(argType[0]))
	{
		optional = true;
	}
	else
	{
		optional = false;
	}

	index = 0;
	memset(minRangeDefault, true, sizeof(minRangeDefault));
	memset(minRange, 0, sizeof(minRange));
	memset(maxRangeDefault, true, sizeof(maxRangeDefault));
	memset(maxRange, 0, sizeof(maxRange));

	if (argRange && argRange[0])
	{
		ptr = argRange;
		while (1)
		{
			tokptr = (char*)strchr(ptr, '[');
			if (!tokptr)
			{
				break;
			}
			endptr = strchr(tokptr, ']');
			if (!endptr)
			{
				assert(0);
				printf("Argument defintion %s, no matching ']' found for range spec in event %s.\n", name.c_str(), eventName);
				break;
			}
			ptr = endptr;
			tokptr++;
			strncpy(scratch, tokptr, endptr - tokptr);
			scratch[endptr - tokptr] = 0;
			tokptr = strchr(scratch, ',');
			if (!tokptr)
			{
				minRange[index >> 1] = (float)atof(scratch);
				minRangeDefault[index >> 1] = false;
				index++;
				index++;
			}
			else if (tokptr == scratch)
			{
				index++;
				tokptr++;
				maxRange[index >> 1] = (float)atof(scratch);
				maxRangeDefault[index >> 1] = false;
				index++;
			}
			else
			{
				bool second;
				if (strlen(scratch) > size_t(tokptr - scratch + 1))
					second = true;
				else
					second = false;
				*tokptr = 0;
				minRange[index >> 1] = (float)atof(scratch);
				minRangeDefault[index >> 1] = false;
				index++;
				tokptr++;
				if (second)
				{
					maxRange[index >> 1] = (float)atof(tokptr);
					maxRangeDefault[index >> 1] = false;
				}
				index++;
			}
		}
	}

	switch (tolower(argType[0]))
	{
	case 'e':
		type = IS_ENTITY;
		break;
	case 'v':
		type = IS_VECTOR;
		break;
	case 'i':
		type = IS_INTEGER;
		break;
	case 'f':
		type = IS_FLOAT;
		break;
	case 's':
		type = IS_STRING;
		break;
	case 'b':
		type = IS_BOOLEAN;
		break;
	case 'l':
		type = IS_LISTENER;
		break;
	}
}

void EventArgDef::GetRange(size_t& numRanges, bool& isInteger, bool& isSingle) const
{
	switch (type)
	{
	case IS_VECTOR:
		isInteger = false;
		isSingle = false;
		numRanges = 3;
		break;
	case IS_FLOAT:
		isInteger = false;
		isSingle = false;
		numRanges = 1;
		break;
	case IS_STRING:
		isInteger = true;
		isSingle = true;
		numRanges = 1;
		break;
	default:
		isSingle = false;
		isInteger = true;
		numRanges = 1;
		break;
	}
}

float EventArgDef::GetMinRange(uintptr_t index) const
{
	if (index < 3)
		return minRange[index];
	return 0.0;
}

bool EventArgDef::GetMinRangeDefault(uintptr_t index) const
{
	if (index < 3)
		return minRangeDefault[index];
	return false;
}

float EventArgDef::GetMaxRange(uintptr_t index) const
{
	if (index < 3)
		return maxRange[index];
	return 0.0;
}

bool EventArgDef::GetMaxRangeDefault(uintptr_t index) const
{
	if (index < 3)
		return maxRangeDefault[index];
	return false;
}

int EventArgDef::getType() const
{
	return type;
}

const char* EventArgDef::GetTypeName() const
{
	switch (type)
	{
	case IS_ENTITY:
		return "Entity";
	case IS_VECTOR:
		return "Vector";
	case IS_INTEGER:
		return "Integer";
	case IS_FLOAT:
		return "Float";
	case IS_STRING:
		return "String";
	case IS_BOOLEAN:
		return "Boolean";
	case IS_LISTENER:
		return "Listener";
	default:
		return "???";
	}
}

const char* EventArgDef::getName() const
{
	return name.c_str();
}

bool EventArgDef::isOptional() const
{
	return optional;
}

AbstractFormater::~AbstractFormater()
{

}

void HTMLFormater::PrintDocumentHeader(fileHandle_t f, const char* title)
{
    EV_WriteToFile(f, "<html><head><title>");
    EV_WriteToFile(f, title);
    EV_WriteToFile(f, "</title></head><body><h1><center>");
    EV_WriteToFile(f, title);
    EV_WriteToFile(f, "</center></h1>");    
}

void HTMLFormater::PrintDocumentFooter(fileHandle_t f)
{
    EV_WriteToFile(f, "</body></html>");
}

void HTMLFormater::BeginClassHeader(fileHandle_t f, const char* className, const char* classID)
{
	if(classID) {
        EV_WriteToFile(f, "<h2> <a name=\"");
        EV_WriteToFile(f, className);
        EV_WriteToFile(f, "\">");
        EV_WriteToFile(f, className);
        EV_WriteToFile(f, " (<i>");
        EV_WriteToFile(f, classID);
        EV_WriteToFile(f, "</i>)</a>");
	}
	else {
        EV_WriteToFile(f, "<h2> <a name=\"");
        EV_WriteToFile(f, className);
        EV_WriteToFile(f, "\">");
        EV_WriteToFile(f, className);
        EV_WriteToFile(f, "</a>");
	}
}

void HTMLFormater::EndClassHeader(fileHandle_t f)
{
    EV_WriteToFile(f, "</h2>");
}

void HTMLFormater::BeginClassBody(fileHandle_t f)
{
    EV_WriteToFile(f, "<BLOCKQUOTE>");
}

void HTMLFormater::EndClassBody(fileHandle_t f)
{
    EV_WriteToFile(f, "</BLOCKQUOTE>");
}


void HTMLFormater::BeginEventDoc(fileHandle_t f, const char* eventName)
{
    EV_WriteToFile(f, "<p><tt><b>");
    EV_WriteToFile(f, eventName);
    EV_WriteToFile(f, "</b>");
}

void HTMLFormater::EndEventDoc(fileHandle_t f)
{
    EV_WriteToFile(f, "</tt><br>");
}

void HTMLFormater::BeginEventDefinition(fileHandle_t f)
{
    EV_WriteToFile(f, "( <i>");
}

void HTMLFormater::EndEventDefinition(fileHandle_t f)
{
    EV_WriteToFile(f, "</i> )");
}

void HTMLFormater::BeginEventArgument(fileHandle_t f, bool isOptional)
{
	if (isOptional) EV_WriteToFile(f, "[ ");
}

void HTMLFormater::EndEventArgument(fileHandle_t f, bool isOptional, bool isLast)
{
	if (isOptional) EV_WriteToFile(f, "] ");
	if (!isLast) EV_WriteToFile(f, ", ");
}

void HTMLFormater::PrintEventArgumentType(fileHandle_t f, const char* argumentType)
{
    EV_WriteToFile(f, argumentType);
    EV_WriteToFile(f, " " );
}

void HTMLFormater::PrintEventArgumentName(fileHandle_t f, const char* argumentName)
{
    EV_WriteToFile(f, argumentName);
}

void HTMLFormater::PrintEventArgumentRangeSingle(fileHandle_t f, float minRange, bool isInteger)
{
	if (isInteger) {
        EV_WriteToFile(f, "<");
        EV_WriteToFile(f, str((int)minRange));
        EV_WriteToFile(f, ">");
	}
	else {
        EV_WriteToFile(f, "<");
        EV_WriteToFile(f, str(minRange));
        EV_WriteToFile(f, ">");
	}
}

void HTMLFormater::PrintEventArgumentRange(fileHandle_t f, float minRange, float maxRange, bool isInteger)
{
	if (isInteger)
	{
		if (minRange != -1 && maxRange != -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str((int)minRange));
            EV_WriteToFile(f, "...");
            EV_WriteToFile(f, str((int)maxRange));
            EV_WriteToFile(f, ">");
		}
		else if (minRange != -1 && maxRange == -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str((int)minRange));
            EV_WriteToFile(f, "...max_integer>");
		}
		else if (minRange == -1 && maxRange != -1) {
            EV_WriteToFile(f, "<min_integer...");
            EV_WriteToFile(f, str((int)maxRange));
            EV_WriteToFile(f, ">");
		}
	}
	else
	{
		if (minRange != -1 && maxRange != -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str(minRange));
            EV_WriteToFile(f, "...");
            EV_WriteToFile(f, str(maxRange));
            EV_WriteToFile(f, ">");
		}
		else if (minRange != -1 && maxRange == -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str(minRange));
            EV_WriteToFile(f, "...max_integer>");
		}
		else if (minRange == -1 && maxRange != -1) {
            EV_WriteToFile(f, "<min_integer...");
            EV_WriteToFile(f, str(maxRange));
            EV_WriteToFile(f, ">");
		}
	}
}

void HTMLFormater::PrintEventDocumentation(fileHandle_t f, const char* documentation)
{
    EV_WriteToFile(f, "<ul>");

	const char* docStart = documentation;

	for(const char* p = docStart; *p; ++p)
	{
		if (*p != '\n') {
            EV_WriteToFile(f, p, 1);
		} else {
            EV_WriteToFile(f, "<br>");
		}
	}

    EV_WriteToFile(f, "</ul>");
}

void HTMLFormater::PrintClassLineage(fileHandle_t f, const char* className)
{
    EV_WriteToFile(f, " -> <a href=\"#");
    EV_WriteToFile(f, className);
    EV_WriteToFile(f, "\">");
    EV_WriteToFile(f, className);
    EV_WriteToFile(f, "</a>");
}

void BasicFormatter::PrintDocumentHeader(fileHandle_t f, const char* title)
{
}

void BasicFormatter::PrintDocumentFooter(fileHandle_t f)
{
}

void BasicFormatter::BeginClassHeader(fileHandle_t f, const char* className, const char* classID)
{
    EV_WriteToFile(f, "********************************************************\n");
    EV_WriteToFile(f, "* All Events For Class: ");
    EV_WriteToFile(f, className);
    EV_WriteToFile(f, "\n");
    EV_WriteToFile(f, "********************************************************\n");
    EV_WriteToFile(f, "********************************************************\n\n");
}

void BasicFormatter::EndClassHeader(fileHandle_t f)
{
}

void BasicFormatter::BeginClassBody(fileHandle_t f)
{
}

void BasicFormatter::EndClassBody(fileHandle_t f)
{
}

void BasicFormatter::BeginEventDoc(fileHandle_t f, const char* eventName)
{
    EV_WriteToFile(f, " -> ");
    EV_WriteToFile(f, eventName);
}

void BasicFormatter::EndEventDoc(fileHandle_t f)
{
    EV_WriteToFile(f, "\n");
}

void BasicFormatter::BeginEventDefinition(fileHandle_t f)
{
    EV_WriteToFile(f, "( ");
}

void BasicFormatter::EndEventDefinition(fileHandle_t f)
{
    EV_WriteToFile(f, ")");
}

void BasicFormatter::BeginEventArgument(fileHandle_t f, bool isOptional)
{
	if (isOptional) EV_WriteToFile(f, "[ ");
}

void BasicFormatter::EndEventArgument(fileHandle_t f, bool isOptional, bool isLast)
{
	if (isOptional) EV_WriteToFile(f, "] ");
	if (!isLast) EV_WriteToFile(f, ", ");
}

void BasicFormatter::PrintEventArgumentType(fileHandle_t f, const char* argumentType)
{
    EV_WriteToFile(f, argumentType);
    EV_WriteToFile(f, " " );
}

void BasicFormatter::PrintEventArgumentName(fileHandle_t f, const char* argumentName)
{
    EV_WriteToFile(f, argumentName);
}

void BasicFormatter::PrintEventArgumentRangeSingle(fileHandle_t f, float minRange, bool isInteger)
{
	if (isInteger) {
        EV_WriteToFile(f, "<");
        EV_WriteToFile(f, str((int)minRange));
        EV_WriteToFile(f, ">");
	}
	else {
        EV_WriteToFile(f, "<");
        EV_WriteToFile(f, str(minRange));
        EV_WriteToFile(f, ">");
	}
}

void BasicFormatter::PrintEventArgumentRange(fileHandle_t f, float minRange, float maxRange, bool isInteger)
{
	if (isInteger)
	{
		if (minRange != -1 && maxRange != -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str((int)minRange));
            EV_WriteToFile(f, "...");
            EV_WriteToFile(f, str((int)maxRange));
            EV_WriteToFile(f, ">");
		}
		else if (minRange != -1 && maxRange == -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str((int)minRange));
            EV_WriteToFile(f, "...max_integer>");
		}
		else if (minRange == -1 && maxRange != -1) {
            EV_WriteToFile(f, "<min_integer...");
            EV_WriteToFile(f, str((int)maxRange));
            EV_WriteToFile(f, ">");
		}
	}
	else
	{
		if (minRange != -1 && maxRange != -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str(minRange));
            EV_WriteToFile(f, "...");
            EV_WriteToFile(f, str(maxRange));
            EV_WriteToFile(f, ">");
		}
		else if (minRange != -1 && maxRange == -1) {
            EV_WriteToFile(f, "<");
            EV_WriteToFile(f, str(minRange));
            EV_WriteToFile(f, "...max_integer>");
		}
		else if (minRange == -1 && maxRange != -1) {
            EV_WriteToFile(f, "<min_integer...");
            EV_WriteToFile(f, str(maxRange));
            EV_WriteToFile(f, ">");
		}
	}
}

void BasicFormatter::PrintEventDocumentation(fileHandle_t f, const char* documentation)
{
	const char* docStart = documentation;

    EV_WriteToFile(f, "    ");

	for(const char* p = docStart; *p; ++p)
	{
		if (*p != '\n') {
            EV_WriteToFile(f, p, 1);
		} else {
            EV_WriteToFile(f, "\n    ");
		}
	}

    EV_WriteToFile(f, "\n");
}

void BasicFormatter::PrintClassLineage(fileHandle_t f, const char* className)
{
    EV_WriteToFile(f, "\n********************************************************\n");
    EV_WriteToFile(f, "* Class: ");
    EV_WriteToFile(f, className);
    EV_WriteToFile(f, "\n");
    EV_WriteToFile(f, "********************************************************\n\n");
}


MEM_BlockAlloc<Event> Event_allocator;

CLASS_DECLARATION(Class, Event, NULL) {
    {NULL, NULL}
};

#ifndef _DEBUG_MEM

void *Event::operator new(size_t size)
{
    return Event_allocator.Alloc();
}

void Event::operator delete(void *ptr)
{
    Event_allocator.Free(ptr);
}

#endif

unsigned int Event::FindEventNum(const char *s)
{
    command_t cmd(s, EV_NORMAL);
    return commandList.findKeyIndex(cmd);
}

int Event::NumEventCommands()
{
    return commandList.size() + 1;
}

void Event::ListCommands(const char *mask)
{
    command_t      *command;
    int             eventnum;
    int             num;
    int             i;
    int             n;
    size_t          l;
    int             p;
    int             hidden;
    str             text;
    Container<int> *sortedList;

    if (!commandList.size()) {
        EVENT_DPrintf("No events.\n");
        return;
    }

    sortedList = &ClassDef::sortedList;
    SortEventList(sortedList);

    l = 0;
    if (mask) {
        l = strlen(mask);
    }

    hidden = 0;
    num    = 0;
    n      = sortedList->NumObjects();
    for (i = 1; i <= n; i++) {
        eventnum = sortedList->ObjectAt(i);
        command  = &commandList[eventnum];

        if (command->flags & EV_CODEONLY) {
            hidden++;
            continue;
        }

        if (mask && Q_stricmpn(command->command, mask, l)) {
            continue;
        }

        num++;

        text = "   ";
        p    = 0;
        if (command->flags & EV_CONSOLE) {
            text[p++] = '*';
        }
        if (command->flags & EV_CHEAT) {
            text[p++] = 'C';
        }
        if (command->flags & EV_CACHE) {
            text[p++] = '%';
        }

        EVENT_Printf("%4d : %s%s\n", eventnum, text.c_str(), command->command);
    }

    EVENT_Printf(
        "\n* = console command.\nC = cheat command.\n%% = cache command.\n\n"
        "Printed %d of %d total commands.\n",
        num,
        n - hidden
    );

    if (developer->integer && hidden) {
        EVENT_Printf("Suppressed %d commands.\n", hidden);
    }
}

void Event::ListDocumentation(const char *mask, qboolean print_to_disk)
{
    int                             num;
    int                             n;
    size_t                          l;
    int                             flags;
    int                             hidden;
    str                             name;
    str                             text;
    fileHandle_t                    event_file = 0;
    str                             event_filename;
    con_map_enum<Event *, EventDef> en = eventDefList;
    EventDef                       *def;

    if (print_to_disk) {
        if (!mask || !mask[0]) {
            event_filename = EVENT_FILENAME;
        } else {
            event_filename = str(mask) + ".txt";
        }

#if defined(GAME_DLL)
        event_file = gi.FS_FOpenFileWrite(event_filename);
#elif defined(CGAME_DLL)
        event_file = cgi.FS_FOpenFileWrite(event_filename);
#else
        event_file = FS_FOpenFileWrite_HomeData(event_filename);
#endif

        if (event_file == 0) {
            return;
        }
    }

    l = 0;
    if (mask) {
        l = strlen(mask);
    }

    EV_Print(event_file, "\nCommand Documentation\n");
    EV_Print(event_file, "=====================\n");

    ClassEventPrinter printer;
    BasicFormatter formatter;

    hidden = 0;
    num    = 0;
    n      = 0;
    for (def = en.NextValue(); def != NULL; def = en.NextValue()) {
        flags = def->flags;
        name  = def->command;
        n++;

        if (flags & EV_CODEONLY) {
            hidden++;
            continue;
        }

        if (mask && Q_stricmpn(name, mask, l)) {
            continue;
        }

        num++;

        printer.PrintDocumentation(*def, event_file, formatter);
    }

    EV_Print(
        event_file,
        "\n* = console command.\nC = cheat command.\n%% = cache command.\n\n"
        "Printed %d of %d total commands.\n",
        num,
        n - hidden
    );

    if (developer->integer && hidden) {
        EV_Print(event_file, "Suppressed %d commands.\n", hidden);
    }

    if (event_file != 0) {
        EVENT_Printf("Printed event info to file %s\n", event_filename.c_str());
#if defined(GAME_DLL)
    gi.FS_FCloseFile(event_file);
#elif defined(CGAME_DLL)
    cgi.FS_FCloseFile(event_file);
#else
    FS_FCloseFile(event_file);
#endif
    }
}

void Event::PendingEvents(const char *mask)
{
    EventQueueNode *event;
    size_t          l;
    int             num;

    l = 0;
    if (mask) {
        l = strlen(mask);
    }

    num   = 0;
    event = EventQueue.next;
    while (event != &EventQueue) {
        assert(event);
        assert(event->m_sourceobject);

        if (!mask || !Q_stricmpn(event->event->getName(), mask, l)) {
            num++;
        }

        event = event->next;
    }
    EVENT_Printf("%d pending events as of %.2f\n", num, EVENT_time);
}

bool Event::Exists(const char *command)
{
    return FindEventNum(command) != 0;
}

#ifdef WITH_SCRIPT_ENGINE

unsigned int Event::FindNormalEventNum(const_str s)
{
    unsigned int *eventnum = normalCommandList.find(s);
    if (eventnum) {
        return *eventnum;
    } else {
        return 0;
    }
}

unsigned int Event::FindReturnEventNum(const_str s)
{
    unsigned int *eventnum = returnCommandList.find(s);
    if (eventnum) {
        return *eventnum;
    } else {
        return 0;
    }
}

unsigned int Event::FindSetterEventNum(const_str s)
{
    unsigned int *eventnum = setterCommandList.find(s);
    if (eventnum) {
        return *eventnum;
    } else {
        return 0;
    }
}

unsigned int Event::FindGetterEventNum(const_str s)
{
    unsigned int *eventnum = getterCommandList.find(s);
    if (eventnum) {
        return *eventnum;
    } else {
        return 0;
    }
}

unsigned int Event::FindNormalEventNum(str s)
{
    s.tolower();
    return FindNormalEventNum(Director.AddString(s));
}

unsigned int Event::FindReturnEventNum(str s)
{
    s.tolower();
    return FindReturnEventNum(Director.AddString(s));
}

unsigned int Event::FindSetterEventNum(str s)
{
    return FindSetterEventNum(Director.AddString(s));
}

unsigned int Event::FindGetterEventNum(str s)
{
    return FindGetterEventNum(Director.AddString(s));
}

int Event::GetEventWithFlags(str name, int flags, uchar type)
{
    unsigned int                     *index;
    con_map<const_str, unsigned int> *cmdList;

    if (type == EV_NORMAL) {
        name.tolower();
        cmdList = &normalCommandList;
    } else if (type == EV_RETURN) {
        name.tolower();
        cmdList = &returnCommandList;
    } else if (type == EV_GETTER) {
        cmdList = &getterCommandList;
    } else if (type == EV_SETTER) {
        cmdList = &setterCommandList;
    } else {
        return 0;
    }

    index = cmdList->find(Director.GetString(name));

    if (!index || !(GetEventFlags(*index) & flags)) {
        return 0;
    } else {
        return *index;
    }
}

int Event::GetEvent(str name, uchar type)
{
    return GetEventWithFlags(name, EV_DEFAULT, type);
}

#else

int Event::GetEvent(str name, uchar type)
{
    return FindEventNum(name);
}

#endif

static str str_null;

command_t *Event::GetEventInfo(int eventnum)
{
    return &commandList[eventnum];
}

int Event::GetEventFlags(int eventnum)
{
    command_t *cmd = &commandList[eventnum];

    if (cmd) {
        return cmd->flags;
    } else {
        return 0;
    }

    return 0;
}

const char *Event::GetEventName(int eventnum)
{
    command_t *cmd;

    if (eventnum <= 0) {
        return str_null;
    }

    cmd = &commandList[eventnum];

    if (cmd) {
        return cmd->command;
    } else {
        return str_null;
    }

    return str_null;
}

int Event::compareEvents(const void *arg1, const void *arg2)
{
    int        num1 = *(int *)arg1;
    int        num2 = *(int *)arg2;
    command_t *cmd1 = &commandList[num1];
    command_t *cmd2 = &commandList[num2];

    return Q_stricmp(cmd1->command, cmd2->command);
}

void Event::SortEventList(Container<int> *sortedList)
{
    unsigned int i;
    command_t   *cmd;

    sortedList->Resize(commandList.size());

    for (i = 1; i <= commandList.size(); i++) {
        cmd = &commandList[i];

        if (cmd != NULL) {
            sortedList->AddObject(i);
        }
    }

    qsort((void *)sortedList->AddressOfObjectAt(1), (size_t)sortedList->NumObjects(), sizeof(int), compareEvents);
}

void Event::LoadEvents()
{
    command_t c;
    DataNode *next;

    for (; DataNodeList; DataNodeList = next) {
        next = DataNodeList->next;

        EventDef *cmd = &eventDefList[DataNodeList->ev];

        cmd->command        = DataNodeList->command;
        cmd->flags          = ((DataNodeList->flags == EV_DEFAULT) - 1) & DataNodeList->flags;
        cmd->formatspec     = DataNodeList->formatspec;
        cmd->argument_names = DataNodeList->argument_names;
        cmd->documentation  = DataNodeList->documentation;
        cmd->type           = DataNodeList->type;

        c.command = DataNodeList->command;
        c.flags   = DataNodeList->flags;
        c.type    = cmd->type;

        DataNodeList->ev->eventnum = commandList.addKeyIndex(c);
        // name is a real retail field (SDK listener.h:383), not debug-only: Event::getName,
        DataNodeList->ev->name       = DataNodeList->command;
        DataNodeList->ev->info.flags = (unsigned)cmd->flags;

        delete DataNodeList;
    }
}

qboolean IsNumeric(const char *str)
{
    int      len;
    int      i;
    qboolean dot;

    if (*str == '-') {
        str++;
    }

    dot = false;
    len = strlen(str);
    for (i = 0; i < len; i++) {
        if (!isdigit(str[i])) {
            if ((str[i] == '.') && !dot) {
                dot = true;
                continue;
            }
            return false;
        }
    }

    return true;
}

EventVar::EventVar(const char *text)
{
    assert(text);
    if (!text) {
        text = "";
    }

    dirtyFlags  = DIRTY_ALL & ~DIRTY_STRING;
    stringValue = text;
    type        = IS_STRING;
    intValue    = 0;
    floatValue  = 0;
}

EventVar::EventVar(const str& text)
{
    dirtyFlags  = DIRTY_ALL & ~DIRTY_STRING;
    stringValue = text;
    type        = IS_STRING;
    intValue    = 0;
    floatValue  = 0;
}

const char *EventVar::GetToken(Event& ev)
{
    if (dirtyFlags & DIRTY_STRING) {
        switch (type) {
        case IS_VECTOR:
            stringValue = va("(%f %f %f)", vectorValue.x, vectorValue.y, vectorValue.z);
            break;

        case IS_INTEGER:
            stringValue = va("%d", intValue);
            break;

        case IS_FLOAT:
            stringValue = va("%f", floatValue);
            break;

        case IS_ENTITY:
            stringValue = va("*%d", intValue);
            break;
        }

        dirtyFlags &= ~DIRTY_STRING;
    }

    return stringValue.c_str();
}

const char *EventVar::GetString(Event& ev)
{
    if (dirtyFlags & DIRTY_STRING) {
        switch (type) {
        case IS_VECTOR:
            stringValue = va("(%f %f %f)", vectorValue.x, vectorValue.y, vectorValue.z);
            break;

        case IS_INTEGER:
            stringValue = va("%d", intValue);
            break;

        case IS_FLOAT:
            stringValue = va("%f", floatValue);
            break;

        case IS_ENTITY:
            stringValue = va("*%d", intValue);
            break;
        }

        dirtyFlags &= ~DIRTY_STRING;
    }

    return stringValue.c_str();
}

int EventVar::GetInteger(Event& ev)
{
    if (dirtyFlags & DIRTY_INTEGER) {
        switch (type) {
        case IS_STRING:
            intValue = atoi(stringValue.c_str());
            break;

        case IS_VECTOR:
            intValue = 0;
            break;

        case IS_FLOAT:
            intValue = int(floatValue);
            break;
        }

        dirtyFlags &= ~DIRTY_INTEGER;
    }

    return intValue;
}

float EventVar::GetFloat(Event& ev)
{
    if (dirtyFlags & DIRTY_FLOAT) {
        switch (type) {
        case IS_STRING:
            floatValue = (float)atof(stringValue.c_str());
            break;

        case IS_VECTOR:
            floatValue = 0;
            break;

        case IS_ENTITY:
        case IS_INTEGER:
            floatValue = float(intValue);
            break;
        }

        dirtyFlags &= ~DIRTY_FLOAT;
    }

    return floatValue;
}

Vector EventVar::GetVector(Event& ev)
{
    if (dirtyFlags & DIRTY_VECTOR) {
        switch (type) {
        case IS_STRING:
            {
                const char *text;

                text = stringValue.c_str();
                if (text[0] == '(') {
                    vectorValue = Vector(&text[1]);
                } else {
                    vectorValue = Vector(text);
                }
                break;
            }

        case IS_FLOAT:
        case IS_INTEGER:
            vectorValue = vec_zero;
            break;
        case IS_ENTITY:
            vectorValue = vec_zero;
            break;
        }

        dirtyFlags &= ~DIRTY_VECTOR;
    }

    return vectorValue;
}

qboolean EventVar::IsVector(Event& ev)
{
    switch (type) {
    case IS_VECTOR:
        return true;
        break;

    case IS_STRING:
        if (stringValue.c_str()[0] == '(') {
            return true;
        }
        break;
    }

    return false;
}

qboolean EventVar::IsNumeric(Event& ev)
{
    switch (type) {
    case IS_STRING:
        return ::IsNumeric(stringValue.c_str());
        break;
    case IS_FLOAT:
    case IS_INTEGER:
        return true;
        break;
    }

    return false;
}

Event::Event()
{
    info.inuse      = 0;
    info.source     = EV_FROM_CODE;
    info.flags      = 0;
    info.linenumber = 0;
    threadnum       = -1;
    eventnum        = 0;
    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    name            = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    documentation   = NULL;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;
}

Event::Event(
    const char *command,
    int         ev_flags,
    const char *theFormatspec,
    const char *theArgument_names,
    const char *theDocumentation,
    uchar       type
)
{
    DataNode *node = new DataNode();

    node->ev             = this;
    node->command        = command;
    node->flags          = ev_flags;
    node->formatspec     = theFormatspec;
    node->argument_names = theArgument_names;
    node->documentation  = theDocumentation;
    node->type           = type;
    node->next           = DataNodeList;
    DataNodeList         = node;

    info.inuse      = 0;
    info.source     = EV_FROM_CODE;
    info.flags      = 0;
    info.linenumber = 0;
    threadnum       = -1;
    eventnum        = 0;
    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    name            = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    documentation   = NULL;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;
}

Event::Event(const Event& ev)
{
    int num;
    int i;

    eventnum        = ev.eventnum;
    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    documentation   = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    name            = GetEventName(eventnum);
    info.inuse      = 0;
    info.source     = ev.info.source;
    info.flags      = ev.info.flags;
    info.linenumber = ev.info.linenumber;
    threadnum       = ev.threadnum;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;

    if (ev.data) {
        num = ev.data->NumObjects();

        data = new Container<EventVar>;
        data->Resize(num);

        for (i = 1; i <= num; i++) {
            data->AddObject(ev.data->ObjectAt(i));
        }
    }
}

Event::Event(Event *ev)
{
    int num;
    int i;

    assert(ev);
    if (!ev) {
        EVENT_Error(ERR_DROP, "NULL Event\n");
    }

    eventnum        = ev->eventnum;
    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    documentation   = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    name            = GetEventName(eventnum);
    info.inuse      = 0;
    info.source     = ev->info.source;
    info.flags      = ev->info.flags;
    info.linenumber = ev->info.linenumber;
    threadnum       = ev->threadnum;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;

    if (ev->data) {
        num = ev->data->NumObjects();

        data = new Container<EventVar>;
        data->Resize(num);

        for (i = 1; i <= num; i++) {
            data->AddObject(ev->data->ObjectAt(i));
        }
    }
}

Event::Event(int index)
{
    if ((index <= 0) || (index > (int)commandList.size())) {
        EVENT_WDPrintf("Event %d out of range.\n", index);
        index      = 0;
        name       = NULL;
        info.flags = 0;
    } else {
        name       = GetEventName(index);
        info.flags = (unsigned)GetEventFlags(index);
    }

    eventnum        = index;
    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    documentation   = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    info.inuse      = 0;
    info.source     = EV_FROM_CODE;
    info.linenumber = 0;
    threadnum       = -1;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;
}

Event::Event(const char *command)
{
    eventnum = FindEventNum(command);
    if (!eventnum) {
        EVENT_WDPrintf("Event '%s' does not exist.\n", command);
        name       = NULL;
        info.flags = 0;
    } else {
        name       = GetEventName(eventnum);
        info.flags = (unsigned)GetEventFlags(eventnum);
    }

    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    documentation   = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    info.inuse      = 0;
    info.source     = EV_FROM_CODE;
    info.linenumber = 0;
    threadnum       = -1;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;
}

Event::Event(const str& command)
{
    eventnum = FindEventNum(command);
    if (!eventnum) {
        EVENT_WDPrintf("Event '%s' does not exist.\n", command.c_str());
        name       = NULL;
        info.flags = 0;
    } else {
        name       = GetEventName(eventnum);
        info.flags = (unsigned)GetEventFlags(eventnum);
    }

    data            = NULL;
    definition      = NULL;
    returntype      = NULL;
    documentation   = NULL;
    formatspec      = NULL;
    argument_names  = NULL;
    info.inuse      = 0;
    info.source     = EV_FROM_CODE;
    info.linenumber = 0;
    threadnum       = -1;
    anim_number     = 0;
    anim_frame      = 0;
    time            = 0.0f;
    flags           = 0;
}

Event::~Event()
{
    if (data) {
        delete data;
        data = NULL;
    }

    if (definition) {
        delete definition;
        definition = NULL;
    }

    if (returntype) {
        delete returntype;
        returntype = NULL;
    }
}

Event& Event::operator=(const Event& ev)
{
    eventnum = ev.eventnum;
    info     = ev.info;

    if (ev.data) {
        int i;

        data = new Container<EventVar>;
        data->Resize(ev.data->NumObjects());
        for (i = 1; i < ev.data->NumObjects(); i++) {
            data->AddObject(ev.data->ObjectAt(i));
        }
    }

    name           = ev.name;
    definition     = NULL;
    returntype     = NULL;
    formatspec     = NULL;
    argument_names = NULL;
    documentation  = NULL;
    threadnum      = ev.threadnum;
    anim_number    = ev.anim_number;
    anim_frame     = ev.anim_frame;
    m_sourceobject = ev.m_sourceobject;
    time           = ev.time;
    flags          = ev.flags;

    return *this;
}

#ifdef WITH_SCRIPT_ENGINE


void Event::SetThread(CThread *thread)
{
    if (thread) {
        threadnum = thread->ThreadNum();
    } else {
        threadnum = -1;
    }
}

CThread *Event::GetThread(void)
{
    if (threadnum != -1) {
        return Director.GetThread(threadnum);
    }

    return NULL;
}

void Event::ReturnString(const char *text)
{
    CThread *thread;

    thread = GetThread();
    if (thread) {
        thread->program->setString(OFS_RETURN, text);
    }
}

void Event::ReturnFloat(float value)
{
    CThread *thread;

    thread = GetThread();
    if (thread) {
        thread->program->setFloat(OFS_RETURN, value);
    }
}

void Event::ReturnInteger(int value)
{
    CThread *thread;

    thread = GetThread();
    if (thread) {
        thread->program->setInteger(OFS_RETURN, value);
    }
}

void Event::ReturnVector(const Vector& vec)
{
    CThread *thread;

    thread = GetThread();
    if (thread) {
        thread->program->setVector(OFS_RETURN, vec);
    }
}

void Event::ReturnEntity(Entity *ent)
{
    CThread *thread;

    thread = GetThread();
    if (thread) {
        thread->program->setEntity(OFS_RETURN, ent);
    }
}

#endif

void Event::ErrorInternal(Listener *l, str text) const
{
    str classname;
    str eventname;

    EVENT_DPrintf("^~^~^ Game");

    classname = l->getClassname();
    eventname = getName();

    EVENT_DPrintf(" (Event: '%s', Object: '%s') : %s\n", eventname.c_str(), classname.c_str(), text.c_str());
}

int Event::NumArgs(void)
{
    if (!data) {
        return 0;
    }

    return (data->NumObjects());
}

void Event::AddToken(const char *text)
{
    if (!data) {
        data = new Container<EventVar>;
        data->Resize(1);
    }

    EventVar var(text);
    data->AddObject(var);
}

void Event::AddTokens(int argc, const char **argv)
{
    int i;

    if (!data) {
        data = new Container<EventVar>;
        data->Resize(argc);
    }

    for (i = 0; i < argc; i++) {
        assert(argv[i]);
        EventVar var(argv[i]);
        data->AddObject(var);
    }
}

void Event::AddString(const char *text)
{
    if (!data) {
        data = new Container<EventVar>;
        data->Resize(1);
    }

    EventVar var(text);
    data->AddObject(var);
}

void Event::AddString(const str& text)
{
    if (!data) {
        data = new Container<EventVar>;
        data->Resize(1);
    }

    EventVar var(text);
    data->AddObject(var);
}

void Event::AddInteger(int val)
{
    if (!data) {
        data = new Container<EventVar>;
        data->Resize(1);
    }

    EventVar var(val);
    data->AddObject(var);
}

void Event::AddFloat(float val)
{
    if (!data) {
        data = new Container<EventVar>;
        data->Resize(1);
    }

    EventVar var(val);
    data->AddObject(var);
}

void Event::AddVector(const Vector& vec)
{
    if (!data) {
        data = new Container<EventVar>;
        data->Resize(1);
    }

    EventVar var(vec);
    data->AddObject(var);
}

const char *Event::GetToken(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return "";
    }

    return data->ObjectAt(pos).GetToken(*this);
}

const char *Event::GetString(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return "";
    }

    return data->ObjectAt(pos).GetString(*this);
}

int Event::GetInteger(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return 0;
    }

    return data->ObjectAt(pos).GetInteger(*this);
}

float Event::GetFloat(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return 0;
    }

    return data->ObjectAt(pos).GetFloat(*this);
}

Vector Event::GetVector(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return vec_zero;
    }

    return data->ObjectAt(pos).GetVector(*this);
}

bool Event::GetBoolean(int pos)
{
    int val;

    val = this->GetInteger(pos);

    return (val != 0) ? true : false;
}

void Event::SetToken(int pos, const char *token)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return;
    }

    EventVar var(token);
    data->SetObjectAt(pos, var);
}

void Event::SetString(int pos, const str& text)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return;
    }

    EventVar var(text);
    data->SetObjectAt(pos, var);
}

void Event::SetInteger(int pos, int value)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return;
    }

    EventVar var(value);
    data->SetObjectAt(pos, var);
}

void Event::SetFloat(int pos, float value)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return;
    }

    EventVar var(value);
    data->SetObjectAt(pos, var);
}

void Event::SetVector(int pos, const Vector& vector)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return;
    }

    EventVar var(vector);
    data->SetObjectAt(pos, var);
}

void Event::SetBoolean(int pos, bool value)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return;
    }

    EventVar var(value);
    data->SetObjectAt(pos, var);
}

qboolean Event::IsVectorAt(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return false;
    }

    return data->ObjectAt(pos).IsVector(*this);
}

qboolean Event::IsNumericAt(int pos)
{
    if (!data || (pos < 1) || (data->NumObjects() < pos)) {
        Error("Index %d out of range.", pos);
        return false;
    }

    return data->ObjectAt(pos).IsNumeric(*this);
}

void Event::Error(const char *fmt, ...)
{
    va_list argptr;
    char    text[1024];

    va_start(argptr, fmt);
    vsprintf(text, fmt, argptr);
    va_end(argptr);

    EVENT_WDPrintf("Client: '%s' : %s\n", getName(), text);
}

EventDef *Event::getInfo()
{
    return &eventDefList[this];
}

const char *Event::getName() const
{
    if (!name) {
        return "NULL";
    }

    return name;
}


qboolean ListenerDelete = false;

Listener::Listener()
{
#ifdef WITH_SCRIPT_ENGINE

    m_EndList = NULL;

    m_NotifyList  = NULL;
    m_WaitForList = NULL;

    vars = NULL;

#endif
}

Listener::~Listener()
{
    if (EventSystemStarted) {
        CancelPendingEvents();
    }

#ifdef WITH_SCRIPT_ENGINE

    UnregisterAll();
    CancelWaitingAll();

    if (vars) {
        delete vars;
    }

#endif
}

#define L_ARCHIVE_NOTIFYLIST  1
#define L_ARCHIVE_WAITFORLIST 2
#define L_ARCHIVE_VARLIST     4
#define L_ARCHIVE_ENDLIST     8

void Listener::Archive(Archiver& arc)
{
    Class::Archive(arc);
}

void Listener::CancelEventsOfType(Event *ev)
{
    EventQueueNode *node;
    EventQueueNode *next;
    int             eventnum;

    node = Event::EventQueue.next;

    eventnum = ev->eventnum;
    while (node != &Event::EventQueue) {
        next = node->next;
        if ((node->GetSourceObject() == this) && (node->event->eventnum == eventnum)) {
            LL_Remove(node, next, prev);
            delete node->event;
            delete node;
        }
        node = next;
    }
}

void Listener::CancelEventsOfType(Event& ev)
{
    this->CancelEventsOfType(&ev);
}

void Listener::CancelFlaggedEvents(int flags)
{
    EventQueueNode *node;
    EventQueueNode *next;

    node = Event::EventQueue.next;

    while (node != &Event::EventQueue) {
        next = node->next;
        if ((node->GetSourceObject() == this) && (node->flags & flags)) {
            LL_Remove(node, next, prev);
            delete node->event;

            delete node;
        }
        node = next;
    }
}

void Listener::CancelPendingEvents(void)
{
    EventQueueNode *node;
    EventQueueNode *next;

    node = Event::EventQueue.next;

    while (node != &Event::EventQueue) {
        next = node->next;
        if (node->GetSourceObject() == this) {
            LL_Remove(node, next, prev);
            delete node->event;
            delete node;
        }
        node = next;
    }
}

qboolean Listener::EventPending(Event& ev)
{
    EventQueueNode *event;
    int             eventnum;

    event = Event::EventQueue.next;

    eventnum = ev.eventnum;

    while (event != &Event::EventQueue) {
        if ((event->GetSourceObject() == this) && (event->event->eventnum == eventnum)) {
            return true;
        }

        event = event->next;
    }

    return false;
}

EventQueueNode *Listener::PostEventInternal(Event *ev, float delay, int flags)
{
    EventQueueNode *node;
    EventQueueNode *i;
    int             inttime;

#if defined(GAME_DLL)
    if (LoadingSavegame) {
        return NULL;
    }
#endif

    if (!classinfo()->responseLookup[ev->eventnum]) {
        if (!ev->eventnum) {
        }

        delete ev;
        return NULL;
    }

    node = new EventQueueNode;

    i       = Event::EventQueue.next;
    inttime = EVENT_msec + (delay * 1000.0f + 0.5f);

    while (i != &Event::EventQueue && inttime > i->inttime) {
        i = i->next;
    }

    node->inttime = inttime;
    node->event   = ev;
    node->flags   = flags;
    node->SetSourceObject(this);

#ifdef _DEBUG
    node->name = ev->name;
#endif

    LL_Add(i, node, next, prev);

    return node;
}

void Listener::PostEvent(Event *ev, float delay, int flags)
{
    PostEventInternal(ev, delay, flags);
}

void Listener::PostEvent(const Event& ev, float delay, int flags)
{
    Event *e = new Event(ev);

    PostEventInternal(e, delay, flags);
}

qboolean Listener::PostponeAllEvents(float time)
{
    EventQueueNode *event;
    EventQueueNode *node;

    event = Event::EventQueue.next;
    while (event != &Event::EventQueue) {
        if (event->GetSourceObject() == this) {
            event->inttime += time * 1000.0f + 0.5f;

            node = event->next;
            while ((node != &Event::EventQueue) && (event->inttime >= node->inttime)) {
                node = node->next;
            }

            LL_Remove(event, next, prev);
            LL_Add(node, event, next, prev);

            return true;
        }
        event = event->next;
    }

    return false;
}

qboolean Listener::PostponeEvent(Event& ev, float time)
{
    EventQueueNode *event;
    EventQueueNode *node;
    int             eventnum;

    eventnum = ev.eventnum;

    event = Event::EventQueue.next;
    while (event != &Event::EventQueue) {
        if ((event->GetSourceObject() == this) && (event->event->eventnum == eventnum)) {
            event->inttime += time * 1000.0f + 0.5f;

            node = event->next;
            while ((node != &Event::EventQueue) && (event->inttime >= node->inttime)) {
                node = node->next;
            }

            LL_Remove(event, next, prev);
            LL_Add(node, event, next, prev);

            return true;
        }
        event = event->next;
    }

    return false;
}

bool Listener::ProcessEvent(Event *ev)
{
    try {
        return ProcessScriptEvent(ev);
    } catch (ScriptException& exc) {
        ev->ErrorInternal(this, exc.string);
        EVENT_DPrintf("%s\n", exc.string.c_str());
        delete ev;
        return false;
    }
}

bool Listener::ProcessEvent(Event& ev)
{
    try {
        return ProcessScriptEvent(ev);
    } catch (ScriptException& exc) {
        ev.ErrorInternal(this, exc.string);
        EVENT_DPrintf("%s\n", exc.string.c_str());
        return false;
    }
}

bool Listener::ProcessEvent(const Event& ev)
{
    try {
        Event event(ev);
        return ProcessScriptEvent(event);
    } catch (ScriptException& exc) {
        ev.ErrorInternal(this, exc.string);
        EVENT_DPrintf("%s\n", exc.string.c_str());
        return false;
    }
}

bool Listener::ProcessScriptEvent(Event *ev)
{
    bool result = ProcessScriptEvent(*ev);

    delete ev;
    return result;
}

bool Listener::ProcessScriptEvent(Event& ev)
{
    ClassDef           *c         = classinfo();
    ResponseDef<Class> *responses = NULL;
    Response            response  = NULL;

    if (!ev.eventnum) {
#ifdef _DEBUG
        EVENT_DPrintf("^~^~^ Failed execution of event '%s' for class '%s'\n", ev.name, c->classname);
#else
        EVENT_DPrintf("^~^~^ Failed execution of event for class '%s'\n", c->classname);
#endif

        return false;
    }

    responses = c->responseLookup[ev.eventnum];

    if (responses == NULL) {
        return true;
    }

    response = responses->response;

    if (response) {
        (this->*response)(&ev);
    }

    return true;
}

void Listener::ProcessContainerEvent(const Container<Event *>& conev)
{
    int num = conev.NumObjects();

    for (int i = 1; i <= num; i++) {
        ProcessEvent(conev.ObjectAt(i));
    }
}

qboolean Listener::ProcessPendingEvents(void)
{
    EventQueueNode *event;
    qboolean        processedEvents;
    float           t;

    processedEvents = false;

    t = EVENT_msec;

    Listener::ProcessingEvents = true;

    event = Event::EventQueue.next;
    while (event != &Event::EventQueue) {
        Listener *obj;

        assert(event);

        obj = event->GetSourceObject();

        if (event->inttime > t) {
            break;
        }

        if (obj != this) {
            event = event->next;
        } else {
            LL_Remove(event, next, prev);

            obj->ProcessEvent(event->event);

            delete event;

            event = Event::EventQueue.next;

            processedEvents = true;
        }
    }

    Listener::ProcessingEvents = false;

    return processedEvents;
}

Listener *Listener::GetScriptOwner(void)
{
    return NULL;
}

void Listener::SetScriptOwner(Listener* newOwner)
{
}

#ifdef WITH_SCRIPT_ENGINE

void Listener::CreateVars(void)
{
    vars = new ScriptVariableList;
}

void Listener::ClearVars(void)
{
    if (vars) {
        delete vars;
        CreateVars();
    }
}

ScriptVariableList *Listener::Vars(void)
{
    if (!vars) {
        CreateVars();
    }

    return vars;
}

bool Listener::BroadcastEvent(str name, Event& event)
{
    return BroadcastEvent(Director.AddString(name), event);
}

bool Listener::BroadcastEvent(const_str name, Event& event)
{
    ConList *listeners;

    if (!m_NotifyList) {
        return false;
    }

    listeners = m_NotifyList->findKeyValue(name);

    if (!listeners) {
        return false;
    }

    return BroadcastEvent(event, listeners);
}

bool Listener::BroadcastEvent(Event& event, ConList *listeners)
{
    Listener *listener;
    int       num = listeners->NumObjects();
    int       i;
    bool      found;

    if (!num) {
        return false;
    }

    if (num == 1) {
        listener = listeners->ObjectAt(1);

        if (listener) {
            listener->ProcessEvent(event);
            return true;
        }

        return false;
    }

    ConList listenersCopy;
    found = false;

    listenersCopy.Resize(num);

    for (i = num; i > 0; i--) {
        listener = listeners->ObjectAt(i);
        if (listener) {
            listenersCopy.AddObject(listener);
        }
    }

    for (i = listenersCopy.NumObjects(); i > 0; i--) {
        listener = listenersCopy.ObjectAt(i);

        if (listener) {
            listener->ProcessEvent(event);
            found = true;
        }
    }

    return found;
}

void Listener::CancelWaiting(str name)
{
    CancelWaiting(Director.AddString(name));
}

void Listener::CancelWaiting(const_str name)
{
    ConList *list;

    if (!m_WaitForList) {
        return;
    }

    list = m_WaitForList->findKeyValue(name);

    if (!list) {
        return;
    }

    ConList stoppedListeners;
    CancelWaitingSources(name, *list, stoppedListeners);

    m_WaitForList->remove(name);

    if (m_WaitForList->isEmpty()) {
        delete m_WaitForList;
        m_WaitForList = NULL;

        if (!DisableListenerNotify) {
            StoppedWaitFor(name, false);
        }
    }

    for (int i = stoppedListeners.NumObjects(); i > 0; i--) {
        Listener *listener = stoppedListeners.ObjectAt(i);

        if (listener && !DisableListenerNotify) {
            listener->StoppedNotify();
        }
    }
}

void Listener::CancelWaitingAll()
{
    CancelWaiting(0);

    if (!m_WaitForList) {
        return;
    }

    con_set_enum<const_str, ConList>    en = *m_WaitForList;
    con_set<const_str, ConList>::Entry *e;
    ConList                             stoppedListeners;

    for (e = en.NextElement(); e != NULL; e = en.NextElement()) {
        CancelWaitingSources(e->GetKey(), e->value, stoppedListeners);
    }

    delete m_WaitForList;
    m_WaitForList = NULL;

    if (!DisableListenerNotify) {
        StoppedWaitFor(STRING_NULL, false);
    }

    for (int i = stoppedListeners.NumObjects(); i > 0; i--) {
        Listener *listener = stoppedListeners.ObjectAt(i);

        if (listener && !DisableListenerNotify) {
            listener->StoppedNotify();
        }
    }
}

void Listener::CancelWaitingSources(const_str name, ConList& listeners, ConList& stoppedListeners)
{
    for (int i = listeners.NumObjects(); i > 0; i--) {
        Listener *listener = listeners.ObjectAt(i);

        if (listener && listener->UnregisterSource(name, this)) {
            stoppedListeners.AddObject(listener);
        }
    }
}

void Listener::Notify(const char *name)
{
    Unregister(name);
}

void Listener::StoppedNotify(void) {}

void Listener::StartedWaitFor(void) {}

void Listener::StoppedWaitFor(const_str name, bool bDeleting) {}

void Listener::EndOn(str name, Listener *listener)
{
    EndOn(Director.AddString(name), listener);
}

void Listener::EndOn(const_str name, Listener *listener)
{
    if (!m_EndList) {
        m_EndList = new con_set<const_str, ConList>;
    }

    ConList& list = m_EndList->addKeyValue(name);

    list.AddUniqueObject(listener);
}

void Listener::Register(str name, Listener *listener)
{
    Register(Director.AddString(name), listener);
}

void Listener::Register(const_str name, Listener *listener)
{
    RegisterSource(name, listener);
    listener->RegisterTarget(name, this);
}

void Listener::RegisterSource(const_str name, Listener *listener)
{
    if (!m_NotifyList) {
        m_NotifyList = new con_set<const_str, ConList>;
    }

    ConList& list = m_NotifyList->addKeyValue(name);

    list.AddObject(listener);
}

void Listener::RegisterTarget(const_str name, Listener *listener)
{
    if (!m_WaitForList) {
        StartedWaitFor();
        m_WaitForList = new con_set<const_str, ConList>;
    }

    ConList& list = m_WaitForList->addKeyValue(name);

    list.AddObject(listener);
}

void Listener::Unregister(str name)
{
    Unregister(Director.AddString(name));
}

void Listener::Unregister(const_str name)
{
    if (m_EndList) {
        ConList *list        = m_EndList->findKeyValue(name);
        bool     bDeleteSelf = false;

        if (list) {
            ConList listeners = *list;

            m_EndList->remove(name);

            if (m_EndList->isEmpty()) {
                delete m_EndList;
                m_EndList = NULL;
            }

            for (int i = listeners.NumObjects(); i > 0; i--) {
                Listener *listener = listeners.ObjectAt(i);

                if (listener) {
                    if (listener == this && (bDeleteSelf)) {
                        continue;
                    }

                    if (listener == this) {
                        bDeleteSelf = true;
                    }

                    delete listener;
                }
            }
        }

        if (bDeleteSelf) {
            return;
        }
    }

    if (!m_NotifyList) {
        return;
    }

    ConList             *list = m_NotifyList->findKeyValue(name);
    ConList              stoppedListeners;
    Container<const_str> stoppedNames;

    if (!list) {
        return;
    }

    UnregisterTargets(name, *list, stoppedListeners, stoppedNames);

    m_NotifyList->remove(name);

    if (m_NotifyList->isEmpty()) {
        delete m_NotifyList;
        m_NotifyList = NULL;

        if (!DisableListenerNotify) {
            StoppedNotify();
        }
    }

    for (int i = stoppedListeners.NumObjects(); i > 0; i--) {
        Listener *listener = stoppedListeners.ObjectAt(i);

        if (listener && !DisableListenerNotify) {
            listener->StoppedWaitFor(name, false);
        }
    }
}

void Listener::Unregister(str name, Listener *listener)
{
    Unregister(Director.AddString(name));
}

void Listener::Unregister(const_str name, Listener *listener)
{
    if (UnregisterSource(name, listener) && !DisableListenerNotify) {
        StoppedNotify();
    }

    if (listener->UnregisterTarget(name, this) && !DisableListenerNotify) {
        listener->StoppedWaitFor(name, false);
    }
}

void Listener::UnregisterAll(void)
{
    Unregister(0);

    if (m_EndList) {
        delete m_EndList;
        m_EndList = NULL;
    }

    if (!m_NotifyList) {
        return;
    }

    con_set_enum<const_str, ConList>    en = *m_NotifyList;
    con_set<const_str, ConList>::Entry *e;
    ConList                             stoppedListeners;
    Container<const_str>                stoppedNames;

    en = *m_NotifyList;

    for (e = en.NextElement(); e != NULL; e = en.NextElement()) {
        UnregisterTargets(e->GetKey(), e->value, stoppedListeners, stoppedNames);
    }

    delete m_NotifyList;
    m_NotifyList = NULL;

    if (!DisableListenerNotify) {
        StoppedNotify();
    }

    for (int i = stoppedListeners.NumObjects(); i > 0; i--) {
        Listener *listener = stoppedListeners.ObjectAt(i);

        if (listener && !DisableListenerNotify) {
            listener->StoppedWaitFor(stoppedNames.ObjectAt(i), true);
        }
    }
}

bool Listener::UnregisterSource(const_str name, Listener *listener)
{
    ConList *list;

    if (!m_NotifyList) {
        return false;
    }

    list = m_NotifyList->findKeyValue(name);

    if (!list) {
        return false;
    }

    list->RemoveObject(listener);

    if (list->NumObjects()) {
        return false;
    }

    m_NotifyList->remove(name);

    if (!m_NotifyList->isEmpty()) {
        return false;
    }

    delete m_NotifyList;
    m_NotifyList = NULL;

    return true;
}

bool Listener::UnregisterTarget(const_str name, Listener *listener)
{
    ConList *list;

    if (!m_WaitForList) {
        return false;
    }

    list = m_WaitForList->findKeyValue(name);

    if (!list) {
        return false;
    }

    list->RemoveObject(listener);

    if (list->NumObjects()) {
        return false;
    }

    m_WaitForList->remove(name);

    if (!m_WaitForList->isEmpty()) {
        return false;
    }

    delete m_WaitForList;
    m_WaitForList = NULL;

    return true;
}

void Listener::UnregisterTargets(
    const_str name, ConList& listeners, ConList& stoppedListeners, Container<const_str>& stoppedNames
)
{
    for (int i = listeners.NumObjects(); i > 0; i--) {
        Listener *listener = listeners.ObjectAt(i);

        if (listener && listener->UnregisterTarget(name, this)) {
            stoppedListeners.AddObject(listener);
            stoppedNames.AddObject(name);
        }
    }
}

void Listener::AbortRegistration(const_str name, Listener *l)
{
    UnregisterSource(name, l);
    l->UnregisterTarget(name, this);
}

int Listener::RegisterSize(str name) const
{
    return RegisterSize(Director.AddString(name));
}

int Listener::RegisterSize(const_str name) const
{
    ConList *listeners;

    if (!m_NotifyList) {
        return 0;
    }

    listeners = m_NotifyList->findKeyValue(name);

    if (listeners) {
        return listeners->NumObjects();
    } else {
        return 0;
    }
}

int Listener::WaitingSize(str name) const
{
    return WaitingSize(Director.AddString(name));
}

int Listener::WaitingSize(const_str name) const
{
    ConList *listeners;

    if (!m_WaitForList) {
        return 0;
    }

    listeners = m_WaitForList->findKeyValue(name);

    if (listeners) {
        return listeners->NumObjects();
    } else {
        return 0;
    }
}


#endif

int Listener::GetFlags(Event *event) const
{
    return classinfo()->GetFlags(event);
}

qboolean Listener::ValidEvent(str name) const
{
    int       num;
    EventDef *def;

    num = Event::FindEventNum(name);
    if (!num) {
        return qfalse;
    }

    def = classinfo()->GetDef(num);
    if (!def) {
        return qfalse;
    }

    return qtrue;
}


void Listener::Remove(Event *ev)
{
    if (ev->NumArgs()) {
        ScriptError("Arguments not allowed.");
    }

    delete this;
}

void Listener::ScriptRemove(Event *ev)
{
    PostEvent(EV_Remove, 0);
}

#ifdef WITH_SCRIPT_ENGINE

void Listener::CancelFor(Event *ev)
{
    BroadcastEvent(ev->GetConstString(1), EV_Remove);
}

void Listener::EventDelayThrow(Event *ev)
{
    BroadcastEvent(0, *ev);
}

void Listener::EventEndOn(Event *ev)
{
    const_str name = ev->GetConstString(1);

    if (Director.CurrentThread() == this) {
        ScriptError("cannot end for the current thread!");
    }

    EndOn(name, Director.CurrentThread());
}

void Listener::EventGetOwner(Event *ev)
{
    ev->AddListener(GetScriptOwner());
}

void Listener::EventNotify(Event *ev)
{
    str name = ev->GetString(1);

    Notify(name);
}

void Listener::EventThrow(Event *ev)
{
    BroadcastEvent(0, *ev);
}

void Listener::EventUnregister(Event *ev)
{
    Unregister(ev->GetConstString(1));
}

void Listener::ExecuteScriptInternal(Event *ev, ScriptVariable& returnValue)
{
    EVENT_DPrintf("'exec' is a MoHAA-only command and has no EF2 meaning\n");
}

void Listener::ExecuteThreadInternal(Event *ev, ScriptVariable& returnValue)
{
    EVENT_DPrintf("'thread' is a MoHAA-only command and has no EF2 meaning\n");
}

void Listener::WaitExecuteScriptInternal(Event *ev, ScriptVariable& returnValue)
{
    EVENT_DPrintf("'waitexec' is a MoHAA-only command and has no EF2 meaning\n");
}

void Listener::WaitExecuteThreadInternal(Event *ev, ScriptVariable& returnValue)
{
    EVENT_DPrintf("'waitthread' is a MoHAA-only command and has no EF2 meaning\n");
}

CThread *Listener::CreateScriptInternal(const ScriptVariable& label)
{
    return NULL;
}

CThread *Listener::CreateThreadInternal(const ScriptVariable& label)
{
    return NULL;
}

void Listener::CreateReturnThread(Event *ev)
{
    ScriptVariable returnValue;

    returnValue.newPointer();

    CThread  *thread    = Director.CreateThread(ev->GetValue(1).stringValue().c_str());
    const int threadNum = thread ? thread->ThreadNum() : 0;

    if (thread) {
        thread->DelayedStart(0.0f);
    }

    ev->ReturnInteger(threadNum);
}

void Listener::CreateThread(Event *ev)
{
    ScriptVariable returnValue;

    // class which does not register it is simply ignored by retail.  The port routes the
    if (!Director.CurrentThread()) {
        return;
    }

    ExecuteThreadInternal(ev, returnValue);
}

void Listener::ExecuteReturnScript(Event *ev)
{
    ScriptVariable returnValue;

    returnValue.newPointer();

    ExecuteScriptInternal(ev, returnValue);

    ev->AddValue(returnValue);
}

void Listener::ExecuteScript(Event *ev)
{
    ScriptVariable returnValue;

    ExecuteScriptInternal(ev, returnValue);
}

void Listener::WaitCreateReturnThread(Event *ev)
{
    ScriptVariable returnValue;

    returnValue.newPointer();

    WaitExecuteThreadInternal(ev, returnValue);

    ev->AddValue(returnValue);
}

void Listener::WaitCreateThread(Event *ev)
{
    ScriptVariable returnValue;

    WaitExecuteThreadInternal(ev, returnValue);
}

void Listener::WaitExecuteReturnScript(Event *ev)
{
    ScriptVariable returnValue;

    returnValue.newPointer();

    WaitExecuteScriptInternal(ev, returnValue);

    ev->AddValue(returnValue);
}

void Listener::WaitExecuteScript(Event *ev)
{
    ScriptVariable returnValue;

    WaitExecuteScriptInternal(ev, returnValue);
}

void Listener::ExecuteThread(str scriptName, str labelName, Event *params)
{
    CThread *thread = Director.CreateThread(labelName, this);

    if (thread) {
        thread->Start();
    }
}

void Listener::ExecuteThread(str scriptName, str labelName, Event& params)
{
    ExecuteThread(scriptName, labelName, &params);
}

#endif

command_t::command_t() {}

command_t::command_t(const char *name, byte t)
    : command(name)
    , type(t)
{}

bool operator==(const char* name, const command_t& command)
{
    if (command.type == EV_NORMAL || command.type == EV_RETURN) {
        return !str::icmp(name, command.command);
    } else {
        return !str::cmp(name, command.command);
    }
}

#ifdef WITH_SCRIPT_ENGINE
bool operator==(const command_t& cmd1, const command_t& cmd2)
{
    return (!str::icmp(cmd1.command, cmd2.command) && (cmd2.type == (uchar)-1 || cmd2.type == cmd1.type));
}
#else
bool operator==(const command_t& cmd1, const command_t& cmd2)
{
    return (!str::icmp(cmd1.command, cmd2.command));
}
#endif