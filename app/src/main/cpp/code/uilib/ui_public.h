/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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

#pragma once

#ifdef __cplusplus
class Event;
class Listener;
#endif

#ifdef __cplusplus
extern "C" {
#endif

    typedef int           uihandle_t;
    typedef struct tiki_s tiki_t;
    typedef struct fontheader_s    fontheader_t;
    typedef struct AliasListNode_s AliasListNode_t;

    typedef struct {
        connstate_t connState;
        int         connectPacketCount;
        int         clientNum;
        char        servername[MAX_STRING_CHARS];
        char        updateInfoString[MAX_STRING_CHARS];
        char        messageString[MAX_STRING_CHARS];
    } uiClientState_t;

    typedef enum {
        UIMENU_NONE,
        UIMENU_MAIN,
        UIMENU_INGAME,
        UIMENU_NEED_CD,
        UIMENU_BAD_CD_KEY,
        UIMENU_TEAM,
        UIMENU_POSTGAME
    } uiMenuCommand_t;

#define SORT_HOST    0
#define SORT_MAP     1
#define SORT_CLIENTS 2
#define SORT_GAME    3
#define SORT_PING    4

#ifndef __cplusplus
    struct Event;
    struct Listener;
#endif

    typedef struct uiimport_s {

        uihandle_t (*Rend_RegisterMaterial)(const char *name);                 
        uihandle_t (*Rend_RefreshMaterial)(const char *name);                  
        qhandle_t (*Rend_RegisterModel)(const char *name);                     
        void (*Rend_Set2D)(                                                    
            int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f
        );
        void (*Rend_SetColor)(const vec4_t rgba);                              
        void (*Rend_Scissor)(int x, int y, int width, int height);             
        void (*Rend_DrawPicStretched)(                                         
            float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader
        );
        void (*Rend_DrawPicTiled)(float x, float y, float w, float h, qhandle_t hShader); 
        fontheader_t *(*Rend_LoadFont)(const char *name);                      
        void (*Rend_DrawString)(                                               
            fontheader_t *font, const char *text, float x, float y, int maxlen, const float *pvVirtualScreen
        );
        void (*Rend_DrawBox)(float x, float y, float w, float h);              
        int (*Rend_GetShaderWidth)(qhandle_t hShader);                         
        int (*Rend_GetShaderHeight)(qhandle_t hShader);                        

#ifdef __cplusplus
        void (*File_PickFile)(const char *name, Listener *obj, Event& event);  
#else
    void (*File_PickFile)(const char *name, struct Listener *obj, struct Event *event);
#endif
        void (*File_ListFiles)(const char *filespec);                          
        long (*File_OpenFile)(const char *qpath, void **buffer);               
        long (*File_ReadFileEx)(const char *qpath, void **buffer, qboolean quiet); 
        void (*File_FreeFile)(void *buffer);                                   
        void (*File_WriteFile)(const char *qpath, const void *buffer, int size); 
        void (*File_GetDirectoryNames)(const char *dirname, void *buf, int len); 

        uihandle_t (*Snd_RegisterSound)(const char *sample, qboolean streamed); 
        void (*Snd_PlaySound)(const char *sound_name, int channel);            
        qboolean (*Alias_Add)(const char *alias, const char *name, const char *parameters); 
        const char *(*Alias_FindRandom)(const char *alias);                                 

        const char *(*Cvar_GetString)(const char *name, const char *defval);   
        cvar_t *(*Cvar_Find)(const char *var_name);                            
        void (*Cvar_Set)(const char *var_name, const char *value);             
        void (*Cvar_Reset)(const char *var_name);                              
        void (*Cmd_Stuff)(const char *text);                                   

        void (*Sys_Printf)(const char *text, ...);                             
        void (*Sys_Error)(int error, const char *text, ...);                   
        void (*Sys_DPrintf)(const char *text, ...);                            
        void (*Sys_WPrintf)(const char *text, ...);                            
        void (*Sys_WDPrintf)(const char *text, ...);                           
        int (*Sys_Milliseconds)(void);                                         
        int (*Sys_IsKeyDown)(int key);                                         
        const char *(*Sys_GetClipboard)(void);                                 
        void (*Sys_SetClipboard)(const char *foo);                             
        const char *(*Cmd_CompleteCommandByNumber)(const char *partial, int number);      
        const char *(*Cvar_CompleteCvarByNumber)(const char *partial, int number);        
        void (*UI_WantsKeyboard)(void);                                        
        const char *(*Client_TranslateWidgetName)(const char *widget);         

        const char *(*Key_GetKeynameForCommand)(const char *command);          
        const char *(*Key_GetCommandForKey)(int keynum);                       
        void (*Key_SetBinding)(int keynum, const char *binding);               
        void (*Key_GetKeysForCommand)(const char *command, int *key1, int *key2);         
        const char *(*Key_KeynumToString)(int keynum);                         
        int (*Key_StringToKeynum)(const char *string);                         
        void (*ActivateView3D)(void);                                          
        const char *(*GetConfigstring)(int index);                             

        const char *(*MissionObjective_GetDescription)(const char *objectivename);        
        void (*MissionObjective_SetDescription)(const char *objectivename, const char *desc); 
        qboolean (*MissionObjective_GetShowObjective)(const char *objectivename);         
        void (*MissionObjective_SetShowObjective)(const char *objectivename, qboolean show); 
        qboolean (*MissionObjective_GetObjectiveComplete)(const char *objectivename);     
        void (*MissionObjective_SetObjectiveComplete)(const char *objectivename, qboolean complete); 
        qboolean (*MissionObjective_GetObjectiveFailed)(const char *objectivename);       
        void (*MissionObjective_SetObjectiveFailed)(const char *objectivename, qboolean failed); 
        const char *(*MissionObjective_GetNameFromIndex)(unsigned int index);             
        int (*MissionObjective_GetIndexFromName)(const char *name);                       
        void *(*MissionObjective_NewObjective)(const char *name);              
        void (*MissionObjective_ClearObjectiveList)(void);                     
        qboolean (*MissionObjectives_ParseObjectiveFile)(const char *levelname);          

        const char *(*MissionInformation_GetShader)(const char *informationname);         
        void (*MissionInformation_SetShader)(const char *informationname, const char *shader); 
        const char *(*MissionInformation_GetInformationData)(const char *informationname); 
        void (*MissionInformation_SetInformationData)(const char *informationname, const char *data); 
        const char *(*MissionInformation_GetNameFromIndex)(unsigned int index);           
        int (*MissionInformation_GetIndexFromName)(const char *name);                     
        void *(*MissionInformation_NewInformation)(const char *name);          
        void (*MissionInformation_ClearInformationList)(void);                 
        void (*MissionInformation_SetShowInformation)(const char *name, qboolean show);  
        qboolean (*MissionInformation_GetShowInformation)(const char *name);   

        void (*reserved_73)(void);
        void (*reserved_74)(void);

        const char *(*SR_GetStringResource)(const char *stringId);             
        qboolean (*SR_IsStringResource)(const char *text);                     
        // port declared this as a single `void *` (a str& retail's callers never
        void (*SR_ConvertStringResourceIds)(const char *text, char *out, int size); 
        void (*Gameplay_SetStringProperty)(const char *obj, const char *prop, const char *value, qboolean create); 
        void (*Gameplay_GetStringProperty)(const char *obj, const char *prop, char *buffer, int length); 
        float (*Gameplay_GetFloatProperty)(const char *obj, const char *prop); 
        void (*Gameplay_SetFloatProperty)(const char *obj, const char *prop, float value, qboolean create); 
        qboolean (*Gameplay_HasProperty)(const char *obj, const char *prop);   
        int (*GetPlayerStat)(int idx);                                         

    } uiimport_t;

#if 1
    typedef struct uiexport_s {
        void (*AddFileToList)(const char *name);
        void (*ResolutionChange)(void);
        void (*Init)(void);
        void (*Shutdown)(void);
    } uiexport_t;
#else

typedef struct uiexport_s {
    void (*Init)(void);
    void (*Shutdown)(void);
    void (*KeyEvent)(int key, int down);
    void (*MouseEvent)(int dx, int dy);
    void (*Refresh)(int time);
    void (*DrawHUD)(playerState_t *ps);
    qboolean (*IsFullscreen)(void);
    void (*SetActiveMenu)(uiMenuCommand_t menu);
    qboolean (*ConsoleCommand)(int realTime);
    void (*DrawConnectScreen)(qboolean overlay);
    void (*AddFileToList)(const char *name);
    void (*ResolutionChange)(void);
    int (*FontStringWidth)(fontheader_t *pFont, const char *pszString, int iMaxLen);
} uiexport_t;

#endif

    // one writer, CL_FillUIDef @0x00196BE0 (212 B).  Its stores, in address order:
    typedef struct uidef_s {
        int          time;
        vec2_t       scaleRes;
        int          vidWidth;
        int          vidHeight;
        int          mouseX;
        int          mouseY;
        unsigned int mouseFlags;
        qboolean     uiHasMouse;
        qboolean     bHighResScaling;
        int          originX;
        int          originY;
        int          surfaceWidth;
        int          surfaceHeight;
    } uidef_t;

    extern uidef_t    uid;
    extern uiexport_t uie;
    extern uiimport_t uii;

    void        UI_Init(void);
    void        UI_Shutdown(void);
    void        UI_InitExports(void);
    void        UI_GetMouseState(int *mouseX, int *mouseY, int *flags);
    int         UI_GetCvarInt(const char *name, int def);
    float       UI_GetCvarFloat(const char *name, float def);
    const char *UI_GetCvarString(const char *cvar, char *def);
    cvar_t     *UI_FindCvar(const char *cvar);
    void        UI_SetCvarInt(const char *cvar, int value);
    void        UI_SetCvarFloat(const char *cvar, float value);
    void        UI_ListFiles(const char *filespec);
    const char *UI_ConfigString(int index);
    void        UI_UpdateCenterPrint(const char *s, float alpha);
    void        UI_UpdateLocationPrint(int x, int y, const char *s, float alpha);
    void        UI_SetupFiles(void);
    void        UI_PrintConsole(const char *msg, int color);

#ifdef __cplusplus
}
#endif
