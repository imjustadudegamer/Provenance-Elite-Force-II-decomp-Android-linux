

#pragma once

#include "g_local.h"
#include "../corepp/listener.h"
#include "entity.h"
#include "trigger.h"
#include "g_utils.h"

#define MOVE_ANGLES 1
#define MOVE_ORIGIN 2

class Entity;
class Mover;

typedef SafePtr<Mover> MoverPtr;

class Mover : public Listener
{
private:
    Vector finaldest;
    Vector angledest;
    Event *endevent;
    int    moveflags;

    // SDK mover.h:39.  The owning entity.  NEVER archived -- Entity::Archive re-supplies
    Entity *self;

public:
    CLASS_PROTOTYPE(Mover);

    Mover();
    explicit Mover(Entity *ent);
    virtual ~Mover() override;

    void MoveDone(Event *ev);
    void MoveTo(const Vector& tdest, const Vector& angdest, float tspeed, Event& event);
    void LinearInterpolate(const Vector& tdest, const Vector& angdest, float time, Event& event);

    int  GetMoveFlags(void) const { return moveflags; }
    void SetMoveFlags(const int moveFlags) { moveflags = moveFlags; }
    void SetEndEvent(const int endEvent);
    void SetFinalDestination(const Vector& finalDestination) { finaldest = finalDestination; }

    void Archive(Archiver& arc) override;
};

inline void Mover::Archive(Archiver& arc)
{

    arc.ArchiveVector(&finaldest);
    arc.ArchiveVector(&angledest);
    arc.ArchiveEventPointer(&endevent);
    arc.ArchiveInteger(&moveflags);
}
