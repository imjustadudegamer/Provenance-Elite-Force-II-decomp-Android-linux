#include "g_local.h"
#include "player.h"
#include "playerstart.h"
#include "ipfilter.h"        
#include "mp_manager.hpp"    
#include "scriptmaster.h"
#include "g_spawn.h"
#include "gameplaymanager.h"  


static vec3_t Q_UNUSED_VAR playerMins = {-15, -15, -24};
static vec3_t Q_UNUSED_VAR playerMaxs = {15, 15, 32};




#define MAX_SPAWN_POINTS 128



void SelectSpawnPoint(Vector& org, Vector& ang, str& thread)
{
    Entity *spot = NULL;


    while ((spot = G_FindClass(spot, "info_player_start")) != NULL) {
        if (level.spawnpoint.icmp(spot->TargetName()) == 0) {
            break;
        }
    }

    if (!spot && !level.spawnpoint.length()) {
        spot = G_FindClass(NULL, "info_player_start");
    }

    if (!spot) {
        gi.Error(ERR_DROP, "No player spawn position named '%s'.  Can't spawn player.\n", level.spawnpoint.c_str());
    }

    org = spot->origin;
    ang = spot->angles;
    if (spot->isSubclassOf(PlayerStart)) {
        thread = ((PlayerStart *)spot)->getThread();
    }
}


qboolean G_duplicateName(const char *name)
{
    gentity_t *edict;
    int        i;

    for (i = 0; i < game.maxclients; i++) {
        edict = &g_entities[i];

        if (!edict->inuse || !edict->entity || !edict->client) {
            continue;
        }

        if (strcmp(edict->client->pers.netname, name) == 0) {
            return true;
        }
    }

    return false;
}

void G_BuildUniqueName(const char *newName, char *name)
{
    char builtName[MAX_NETNAME];
    int  i;

    for (i = 2; i <= 9; i++) {
        sprintf(builtName, "%s%d", newName, i);

        if (!G_duplicateName(builtName)) {
            strcpy(name, builtName);
            return;
        }
    }

    sprintf(builtName, "%s?", newName);

    strcpy(name, builtName);
}

void G_ClientUserinfoChanged(gentity_t *ent, const char *u)
{
    const char *s;
    int         playernum;
    Player     *player;
    bool        autoSwitchWeapons;
    char        tempName[MAX_NETNAME];
    int         i;
    bool        validName;

    try {
        if (!ent) {
            assert(0);
            return;
        }

        player = (Player *)ent->entity;

        s = Info_ValueForKey(u, "name");
        if (!s) {
            assert(0);
            return;
        }


        strncpy(tempName, s, MAX_NETNAME - 1 - 3);


        validName = false;

        for (i = 0; i < (int)strlen(tempName); i++) {
            if ((tempName[i] == '^') && (tempName[i + 1] >= '0') && (tempName[i + 1] <= '9')) {
                i++;
                continue;
            }

            validName = true;
        }

        if (!validName) {
            strcpy(tempName, "RedShirt");
        }

        if (strcmp(ent->client->pers.netname, tempName) != 0) {
            if (G_duplicateName(tempName)) {
                G_BuildUniqueName(tempName, ent->client->pers.netname);
            } else {
#pragma GCC diagnostic push
#if !defined(__clang__) 
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
                strncpy(ent->client->pers.netname, tempName, MAX_NETNAME - 1 - 3);
#pragma GCC diagnostic pop
            }
        }


        for (i = 0; i < (int)strlen(ent->client->pers.netname); i++) {
            if (ent->client->pers.netname[i] == ':') {
                ent->client->pers.netname[i] = '.';
            }
        }


        if (strstr(ent->client->pers.netname, "^")) {
            int length = strlen(ent->client->pers.netname);

            if ((length < 2) || (ent->client->pers.netname[length - 2] != '^')
                || (ent->client->pers.netname[length - 1] != '8')) {
                ent->client->pers.netname[length]     = '^';
                ent->client->pers.netname[length + 1] = '8';
                ent->client->pers.netname[length + 2] = 0;
            }
        }

        s = Info_ValueForKey(u, "mp_playermodel");
        if (!s) {
            assert(0);
            return;
        }

        multiplayerManager.changePlayerModel(player, s);

        strncpy(ent->client->pers.mp_playermodel, s, sizeof(ent->client->pers.mp_playermodel) - 1);

        s = Info_ValueForKey(u, "mp_lowBandwidth");
        if (!s) {
            assert(0);
            return;
        }

        ent->client->pers.mp_lowBandwidth = atoi(s);

        s = Info_ValueForKey(u, "dm_morph_c1");
        if (!s) {
            assert(0);
            return;
        }
        strncpy(ent->client->pers.dm_morph_c1, s, sizeof(ent->client->pers.dm_morph_c1) - 1);

        playernum = ent - g_entities;
        gi.setConfigstring(CS_PLAYERS + playernum, va("name\\%s", ent->client->pers.netname));

        multiplayerManager.changePlayerName(player, ent->client->pers.netname);

        if (player) {
            float fov;
            Event *ev;


            fov = (float)atof(Info_ValueForKey(u, "userFov"));
            if (fov < 1.0f) {
                fov = sv_defaultFov->value;
            } else if (fov > 160.0f) {
                fov = 160.0f;
            }

            player->userFov = fov;

            ev = new Event(EV_Player_Fov);
            ev->AddFloat(fov);
            player->ProcessEvent(ev);


            if (atoi(Info_ValueForKey(u, "mp_autoSwitchWeapons"))) {
                autoSwitchWeapons = true;
            } else {
                autoSwitchWeapons = false;
            }

            player->setAutoSwitchWeapons(autoSwitchWeapons);

            s = Info_ValueForKey(u, "mp_savingDemo");

            if (!s) {
                assert(0);
                return;
            }

            player->mp_savingDemo = atoi(s);
        }

        strncpy(ent->client->pers.userinfo, u, sizeof(ent->client->pers.userinfo) - 1);

        Info_SetValueForKey(ent->client->pers.userinfo, "name", ent->client->pers.netname);
        gi.setUserinfo(playernum, ent->client->pers.userinfo);
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

const char *G_ClientConnect(int clientNum, qboolean firstTime, qboolean isBot, qboolean checkPassword)
{
    const char *value;
    gentity_t  *ent;
    char        userinfo[MAX_INFO_STRING];
    gclient_t  *client;

    try {
        ent = &g_entities[clientNum];
        gi.getUserinfo(clientNum, userinfo, sizeof(userinfo));

        value = Info_ValueForKey(userinfo, "ip");
        if (SV_FilterPacket(value)) {
            return "$$BannedIP$$";
        }

        if (checkPassword) {
            value = Info_ValueForKey(userinfo, "password");
            if (strcmp(password->string, value) != 0) {
                return "$$InvalidPassword$$";
            }
        }

        ent->client = game.clients + clientNum;
        client      = ent->client;

        if (firstTime && !ent->entity) {
            memset(client, 0, sizeof(*client));

            if (!game.autosaved) {
                G_InitClientPersistant(client);
                playersLastTeam[clientNum] = "";
            }
        }

        if (isBot) {
            ent->svflags |= SVF_BOT;
            if (!G_BotConnect(clientNum, !firstTime)) {
                return "BotConnectfailed";
            }
        }

        G_ClientUserinfoChanged(ent, userinfo);

        if (firstTime && (game.maxclients > 1)) {
            gi.Printf("%s connected\n", ent->client->pers.netname);
        }

        LoadingServer = false;
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }

    return NULL;
}

extern "C" void G_ClientThink(gentity_t *ent, usercmd_t *ucmd)
{
    try {
        if (ent->entity) {
            current_ucmd = ucmd;
            ent->entity->ProcessEvent(EV_ClientMove);
            current_ucmd = NULL;
        }
    } catch (const char *error) {
        G_ExitWithError(error);
    }
}

void G_ClientBegin(gentity_t *ent, const usercmd_t *cmd)
{
    try {
        assert(ent->s.number < game.maxclients);
        assert(ent->client != NULL);

        if (ent->inuse && ent->entity) {
            if (ent->client->ps.pm_type != PM_SECRET_MOVE_MODE) {
                ent->entity->SetDeltaAngles();
            }
        } else {
            Player *Q_UNUSED_VAR player;

            level.spawn_entnum = ent->s.number;
            player             = new Player;
        }

        if (level.intermissiontime && ent->entity) {
            G_MoveClientToIntermission(ent->entity);
        } else {
            ent->client->pers.enterTime = level.time;
            if (game.maxclients > 1) {
                gi.Printf("%s entered the game\n", ent->client->pers.netname);
            }
        }

        if (ent->entity) {
            ent->entity->ProcessEvent(EV_ClientEndFrame);

            if (!Director.PlayerReady()) {
                Director.PlayerSpawned();
            }

            GameplayManager::getTheGameplayManager()->processPendingMessages();
            G_SentInitialMessages();
        }
    } catch (const char *error) {
        G_ExitWithError(error);
    }
}


void G_ClientDisconnect(gentity_t *ent)
{
    try {
        if (!ent || (!ent->inuse) || (!ent->client) || (!ent->entity)) {
            return;
        }

        ((Player *)ent->entity)->Disconnect();

        delete ent->entity;
        ent->entity = NULL;
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

