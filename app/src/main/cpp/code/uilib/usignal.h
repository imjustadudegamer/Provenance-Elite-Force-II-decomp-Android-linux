
#pragma once

#include "../corepp/listener.h"

class UConnection
{
    class Event                  m_inevent;
    class Event                  m_outevent;
    Container<SafePtr<Listener>> m_listeners;
    Container<Event *>           m_events;

public:
    UConnection();
    UConnection(const Event& inevent, const Event& outevent);
    bool TypeIs(Event& ev);
    bool AddListener(Listener *object, Event& ev);
    bool RemoveListener(Listener *object);
    bool SendEvent(Listener *object, Event& ev);
};

class USignal : public Listener
{
    Container<UConnection *> m_connections;

public:
    CLASS_PROTOTYPE(USignal);

    bool SendSignal(Event& ev);
    bool Connect(Listener *object, Event& inevent, Event& outevent);
    bool Disconnect(Listener *object, Event& ev);
    bool Disconnect(Listener *object);
};
