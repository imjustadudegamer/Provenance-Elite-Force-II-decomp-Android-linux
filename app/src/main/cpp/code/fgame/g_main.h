
#pragma once

#include "g_local.h"
#include "gamecvars.h"

class Player;

extern qboolean      LoadingSavegame;
extern qboolean      LoadingServer;
extern game_import_t gi;
extern game_export_t globals;
extern int           g_protocol;

extern qboolean g_iInThinks;

extern gentity_t active_edicts;
extern gentity_t free_edicts;

extern int sv_numtraces;

extern usercmd_t  *current_ucmd;
extern Player     *g_pPlayer;

void G_ExitWithError(const char *error);
void G_ClientDrawSplines(void);  
void G_AllocGameData(void);
void G_DeAllocGameData(void);
void G_ClientDrawBoundingBoxes(void);
void G_AddEntityToExtraList(int entityNum);       
void G_RemoveEntityFromExtraList(int entityNum);  

