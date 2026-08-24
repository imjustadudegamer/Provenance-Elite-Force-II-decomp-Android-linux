
#include "actor.h"
#include "actorgamecomponents.h"
#include "player.h"

CLASS_DECLARATION(Listener, ActorGameComponent, "actor_game_component") {
    {NULL, NULL}
};

CLASS_DECLARATION(ActorGameComponent, EFGameComponent, "ef_game_component") {
    {NULL, NULL}
};

EFGameComponent::EFGameComponent()
{
    gi.Error(ERR_FATAL, "EFGameComponent::EFGameComponent -- Default Constructor Called");
    act = NULL;
}

EFGameComponent::EFGameComponent(const Actor *actor)
{
    act = NULL;

    if (actor) {
        act = (Actor *)actor;
    }
}

void EFGameComponent::DoArchive(Archiver& arc, const Actor *actor)
{
    if (actor) {
        act = (Actor *)actor;
    } else {
        gi.Error(ERR_FATAL, "EFGameComponnet::DoArchive -- actor is NULL");
    }
}

void EFGameComponent::HandleDeath(const Entity *ent) {}

void EFGameComponent::HandleArmorDamage(Event *ev) {}

void EFGameComponent::HandleThink() {}

void EFGameComponent::HandleEvent(Event *ev)
{
    Event *new_event;
    new_event = new Event(*ev);
    ProcessEvent(new_event);
}

qboolean EFGameComponent::DoCheck(const Conditional& condition)
{
    return false;
}
