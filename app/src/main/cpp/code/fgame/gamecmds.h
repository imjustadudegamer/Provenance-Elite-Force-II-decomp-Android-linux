

#pragma once


#include "g_local.h"

void G_InitConsoleCommands(void);

qboolean G_ProcessClientCommand(gentity_t *ent);


void     G_Say(gentity_t *ent, qboolean team, qboolean arg0);
qboolean G_CameraCmd(gentity_t *ent);
qboolean G_SoundCmd(gentity_t *ent);
qboolean G_SayCmd(gentity_t *ent);
qboolean G_EventListCmd(gentity_t *ent);
qboolean G_PendingEventsCmd(gentity_t *ent);
qboolean G_EventHelpCmd(gentity_t *ent);
qboolean G_DumpEventsCmd(gentity_t *ent);
qboolean G_ClassEventsCmd(gentity_t *ent);
qboolean G_DumpClassEventsCmd(gentity_t *ent);
qboolean G_DumpAllClassesCmd(gentity_t *ent);
qboolean G_ClassListCmd(gentity_t *ent);
qboolean G_ClassTreeCmd(gentity_t *ent);
qboolean G_RestartCmd(gentity_t *ent);
qboolean G_LevelVarsCmd(gentity_t *ent);
qboolean G_GameVarsCmd(gentity_t* ent);
qboolean G_ScriptCmd(gentity_t* ent);
qboolean G_LocCmd(gentity_t *ent);    
qboolean G_WarpCmd(gentity_t *ent);   
qboolean G_MaskCmd(gentity_t *ent);   

qboolean G_CinematicCmd(gentity_t *ent);       
qboolean G_VTaunt(gentity_t *ent);             
qboolean G_TellCmd(gentity_t *ent);            
qboolean G_TeamSayCmd(gentity_t *ent);         
qboolean G_TauntCmd(gentity_t *ent);           
qboolean G_ClientRunThreadCmd(gentity_t *ent);  
qboolean G_ClientSetVarCmd(gentity_t *ent);    
qboolean G_SetGameplayFloatCmd(gentity_t *ent);  
qboolean G_SetGameplayStringCmd(gentity_t *ent);  
qboolean G_PurchaseSkillCmd(gentity_t *ent);   
qboolean G_SwapItemCmd(gentity_t *ent);        
qboolean G_DropItemCmd(gentity_t *ent);        

qboolean G_DialogRunThread(gentity_t *ent);

qboolean G_SendCommandToAllPlayers( const char *command );  
