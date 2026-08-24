

#pragma once

class Sentient;

#include "animate.h"

class Object : public Entity
{
public:
    CLASS_PROTOTYPE(Object);

    Object();

    void Killed(Event *ev);  
    void Setup(Event *ev);            
    void MakeShootable(Event *ev);

    void Archive(Archiver& arc) override;
    void SetAnim(Event *ev);
};

// SDK object.cpp:52 -- Object::Archive does not exist in retail, so Object's frame IS
// pause / delta, 293 bytes minimum) in after it -- bytes retail never writes.  Measured
inline void Object::Archive(Archiver& arc)
{
    Entity::Archive(arc);
}

extern Event EV_ThrowObject_Pickup;
extern Event EV_ThrowObject_Throw;

class ThrowObject : public Object
{
private:
    int      owner;
    Vector   pickup_offset;
    str      throw_sound;
    float    damage;
    qboolean hurt_target;

public:
    CLASS_PROTOTYPE(ThrowObject);

    ThrowObject();
    void Touch(Event *ev);
    void Throw(Event *ev);
    void Throw(const Entity *owner, float speed, const Sentient *targetent, float gravity, float throw_damage);  
    void Pickup(Event *ev);
    void Pickup(Entity *ent, const str &bone);  
    void PickupOffset(Event *ev);
    void ThrowSound(Event *ev);
    void Archive(Archiver& arc) override;
};

