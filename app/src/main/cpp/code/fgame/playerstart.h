

#pragma once

#include "g_local.h"
#include "entity.h"
#include "camera.h"
#include "navigate.h"

extern Event EV_PlayerStart_SetThread;

class PlayerStart : public Entity
{
public:
    CLASS_PROTOTYPE(PlayerStart);  

private:
    str thread;

public:


    void Archive(Archiver& arc) override;

    void SetAngle(Event *ev);
    void SetThread(Event *ev);
    str  getThread(void);
};

inline void PlayerStart::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    arc.ArchiveString(&thread);
}

class TestPlayerStart : public PlayerStart
{
public:
    CLASS_PROTOTYPE(TestPlayerStart);  
};

class PlayerDeathmatchStart : public PlayerStart
{
private:
    void SetType(Event *ev);

public:
    str _type;

    PlayerDeathmatchStart() {}

    CLASS_PROTOTYPE(PlayerDeathmatchStart);
};

class PlayerIntermission : public Camera
{
public:
    CLASS_PROTOTYPE(PlayerIntermission);
    PlayerIntermission();
};
