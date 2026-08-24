

#include "game.h"
#include "scriptmaster.h"
#include "worldspawn.h"

#include "camera.h"
#include "entity.h"
#include "player.h"



Game game;



void Game::Init()
{
    clients     = NULL;
    autosaved   = qfalse;
    maxentities = 0;
    maxclients  = 0;
}

void Game::Archive(Archiver& arc)
{
    int i;

    Class::Archive(arc);

    if (arc.Loading()) {
        G_AllocGameData();
    }

    for (i = 0; i < maxclients; i++) {
        // memcpy: retail's trace_t embeds a POINTER, so a 64-bit build can never produce
        G_ArchiveClientRetail(arc, &clients[i]);
    }

    arc.ArchiveBoolean(&autosaved);
    arc.ArchiveInteger(&maxentities);
    arc.ArchiveInteger(&maxclients);
}



CLASS_DECLARATION(Listener, Game, NULL) {
    {NULL,            NULL            }
};
