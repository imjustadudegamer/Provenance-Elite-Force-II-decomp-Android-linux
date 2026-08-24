

#pragma once

#include "level.h"
#include "../corepp/listener.h"
#include "g_public.h"

class Game : public Listener
{
public:
    CLASS_PROTOTYPE(Game);

    gclient_t *clients;
    qboolean   autosaved;
    int        maxclients;
    int        maxentities;

public:

protected:

public:

    void Init();
    void Archive(Archiver& arc) override;
};

extern Game game;
