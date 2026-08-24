

#pragma once

#include "g_local.h"
#include "trigger.h"

class Emitter : public Entity
{
private:
    str  emitterName;
    void setEmitter(str name);
    void EmitterName(Event *ev);

public:
    CLASS_PROTOTYPE(Emitter);
    Emitter();
    void Archive(Archiver& arc) override;
};

inline void Emitter::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveString(&emitterName);
    if (arc.Loading()) {
        setEmitter(emitterName);
    }
}

class Rain : public Emitter
{
private:
    str  rainName;
    void setRainName(str name);

public:
    CLASS_PROTOTYPE(Rain);
    Rain();
    void Archive(Archiver& arc) override;
};

inline void Rain::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveString(&rainName);
    if (arc.Loading()) {
        setRainName(rainName);
    }
}

class PuffDaddy : public Entity
{
private:
    void Touch(Event *ev);
    void Idle(Event *ev);

public:
    CLASS_PROTOTYPE(PuffDaddy);
    PuffDaddy();

};
