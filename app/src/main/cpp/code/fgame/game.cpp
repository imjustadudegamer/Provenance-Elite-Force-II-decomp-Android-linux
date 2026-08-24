/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

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
