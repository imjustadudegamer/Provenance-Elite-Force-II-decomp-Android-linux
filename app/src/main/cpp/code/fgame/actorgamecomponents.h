
#ifndef __ACTORGAMECOMPONENTS_H__
#define __ACTORGAMECOMPONENTS_H__


class Actor;
class Entity;
class Archiver;

class ActorGameComponent : public Listener
{
public:
    CLASS_PROTOTYPE(ActorGameComponent);

    ActorGameComponent() {}

    virtual void HandleEvent(Event *ev) {}
    virtual void HandleArmorDamage(Event *ev) {}
    virtual void HandleDeath(const Entity *ent) {}
    virtual void HandleThink() {}

    virtual qboolean DoCheck(const Conditional& condition) { return false; }
    virtual void     DoArchive(Archiver& arc, const Actor *act) {}
};

class EFGameComponent : public ActorGameComponent
{
public:
    CLASS_PROTOTYPE(EFGameComponent);

    EFGameComponent();
    EFGameComponent(const Actor *actor);

    void HandleEvent(Event *ev) override;
    void HandleArmorDamage(Event *ev) override;
    void HandleDeath(const Entity *ent) override;
    void HandleThink() override;

    qboolean DoCheck(const Conditional& condition) override;
    void     DoArchive(Archiver& arc, const Actor *act) override;

private:
    Actor *act;
};

#endif 
