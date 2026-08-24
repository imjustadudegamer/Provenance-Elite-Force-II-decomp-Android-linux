
#pragma once

#include "../uilib/ui_extern.h"
#include "../uilib/uiwidget.h"
#include "cl_scoreboard.h"

typedef struct {
    UIReggedMaterial *loading;
} static_media_t;


void CL_FillUIDef(void);
void CL_FillUIImports(void);
const char *CL_GetCurrentWeaponsStringCommand(void);

void UI_NextInventory_f(void);
void UI_PrevInventory_f(void);
void UI_WarpInventory_f(void);

void UI_AddHud(const char *name, int show);
void UI_RemoveHud(const char *name);

qboolean UI_IsView3DActive(void);

const char *CL_GetCurrentWeapon(void);
void CL_InitializeUI(void);
void CL_ShutdownUI(void);

extern inventory_t  client_inv;
extern bind_t       client_bind;
extern qboolean     server_loading;

extern cvar_t       *dm_morph_c1;

extern const UColor UWhiteChatMessageColor;
extern const UColor URedChatMessageColor;
extern const UColor UGreenChatMessageColor;

const char *CvarGetForUI(const char *name, const char *defval);

int CL_GetStatByName(const char *name);
int CL_SafePlayerStat(int idx);
void        UI_ClearState(void);
void        CL_BeginRegistration(void);
void        CL_EndRegistration(void);
void        UI_GetHighResolutionScale(vec2_t scale);
void        UI_ReloadStrings(void);

void        UI_ServerLoading(const char *mapname, int stage);
qboolean    UI_IsServerLoading(void);
void        UI_StopLoading(void);
void        UI_Disconnected(void);

extern Menu *ui_pLoadingMenu;

void     UI_ShowMenu(const char *name, qboolean bForce);
void     UI_HideMenu(const char *name, qboolean bForce);
void     Dialog_ResetConsole(const char *consoleName);
void     UI_PrintString(const char *fontname, float x, float y, const char *s, float alpha);
void     UI_AddPrintString(const char *text, float *pos, int width, float *color, const char *fontname);
void     UI_AddNotifyString(const char *text);
int      UI_GetGlobalFontHeight(void);
int      UI_GetFontHeight(const char *fontname, qboolean virtualScreen);
void     UI_NewMap(void);
void     UI_ForceMenuOff(bool force);
void     UI_PushMenu(const char *name);
void     UI_TransitionMenu(const char *name);
void     UI_ForceMenu(const char *name);
qboolean UI_BindActive(void);
void     UI_ActivateView3D(void);
void     UI_MenuEscape(const char *name);
void     UI_CreateScoreboard(void);
qboolean UI_MenuActive(void);
qboolean UI_IsMenuActive(void);    
qboolean UI_MenuUp(void);
void     UI_FocusMenuIfExists(void);
int      UI_HudCount(void);
const char *UI_HudNames(char *buf, int buflen);
void     UI_DrawConnect(void);
void     UI_ShowDisconnectMessage(const char *msg);
void     UI_CreateDialog(
        const char *title,
        char       *cvarname,
        const char *command,
        int         width,
        int         height,
        const char *shader,
        const char *okshader,
        const char *cancelshader
    );
void UI_ShowScoreboard_f(void);
void UI_DeleteScoreboard(void);

extern "C" char *Com_GetGameTypeName(int gametype, char *buf, int bufsize);

void UI_HideScoreboard_f(void);
void UI_UpdateTacticalInfo(void);

extern "C" {
int         MissionInformation_NumObjects(void);
const char *MissionInformation_DataAtIndex(int index);
void        MissionObjectives_Update(const char *objectivesName);   
}


void     UI_BeginLoad(const char *pszMapName);
void     UI_EndLoad(void);
void     UI_AbortLoad(void);
int      CL_FontStringWidth(fontheader_t *pFont, const char *pszString, int iMaxLen);
void     UI_BeginLoadResource(void);
void     UI_EndLoadResource(void);
void     UI_EndLoadResource(const char *name);
qboolean UI_IsResourceLoaded(const char *name);
void     UI_RegisterLoadResource(const char *name);
void     UI_ClearResource(void);
void     UI_LoadResource(const char *name);


void     UI_OpenConsole(void);
qboolean UI_ConsoleIsVisible(void);
qboolean UI_ConsoleIsOpen(void);
void     UI_CloseConsole(void);
void     UI_ToggleConsole(void);
void     UI_PrintConsole(const char *msg, int color);

void UI_UpdateLoadingStatus(int stage);

void Dialog_LoadDialog(const char *mapname);
void UI_DisplayBranchDialog_f(void);

bool Dialog_PrintDialog(const char *soundName, const char *consoleName);

void UI_DisplayDialogConsole(const char *dialogName, const char *dialogTextSoundName, int entnum);

void UI_Update(void);
void UI_UpdateMenus(void);
void UI_ServerLoaded(void);
void UI_ResolutionChange(void);
void UI_KeyEvent(int key, unsigned int time);
void UI_CharEvent(int ch);

class Script;

class BotAssignment {
public:
    str  m_name;      
    str  m_team;      
    str  m_specialty; 
    bool m_open;      

    BotAssignment();

    bool parseProperties(Script &script);        
    void clear(void);                            
    BotAssignment &operator=(BotAssignment &a);  
};

class BotProperties {
public:
    str m_name;             
    str m_screenshot;       
    int m_unknown56;        
    str m_defaultSpecialty; 
    str m_unknown88;        
    str m_playerModel;      

    BotProperties();
    ~BotProperties();

    bool parseBotSection(Script &script);  
};

class LevelProperties {
public:
    str                        m_mapName;             
    str                        m_longMapName;         
    str                        m_screenshot;          
    Container<BotAssignment *> m_botAssignments;      
    Container<str>             m_gameTypes;           
    Container<str>             m_matchTypes;          
    Container<str>             m_modifiers;           
    str                        m_recommendedPlayers;  
    int                        m_fraglimit;           
    bool                       m_specialties;         
    bool                       m_bots;                

    LevelProperties();
    ~LevelProperties();

    bool readFile(const str &name);           
    bool hasGameType(const str &gameType);    
    bool hasMatchType(const str &matchType);  
    void setSupportedModifiers(void);         

private:
    bool _parseProperties(Script &script);    
};

class ArenaController {
public:
    Container<LevelProperties *> m_levelProperties;         
    Container<BotProperties *>   m_botProperties;           
    Container<str>               m_dmModels;                
    BotAssignment                m_botAssignments[7];       
    str                          m_awardImages[6];          
    str                          m_unknownWidget;           
    str                          m_mapListWidget;           
    str                          m_botListWidget;           
    str                          m_botSelectListWidget;     
    bool                         m_botListRowBased;         
    str                          m_defaultMapScreen;        
    int                          m_mdsRegistry;             
    str                          m_selectedBot;             
    int                          m_selectedBotAssignment;   
    str                          m_voteOptionWidget;        
    str                          m_voteType;                
    str                          m_voteOption;              
    cvar_t                      *m_tempBotskillLevel;       
    cvar_t                      *m_tempMaxClients;          
    bool                         m_specialties;             
    bool                         m_teamGame;                
    LevelProperties             *m_selectedLevel;           
    str                          m_serverAddress;           

    ArenaController();
    ~ArenaController();

    void registerCommands(void);                                          
    void unregisterCommands(void);                                        
    void registerCvars(void);                                             
    void readDMModels(const str &directory);                              
    void clearDMModels(void);                                             
    void readLevelProperties(const str &directory);                       
    void clearLevelProperties(void);                                      
    void getLevels(const str &gameType, const str &matchType);            
    LevelProperties *getLevelProperties(const str &name);                 
    void setLevelAward(const str &levelName);                             
    void setSelectedLevel(const str &levelName, int showBotAssignments);  
    void readBotFiles(void);                                              
    void readBotProperties(const char *filename);                         
    void clearBotProperties(void);                                        
    void clearBotAssignments(void);                                       
    void copyTempCvars(void);                                             
    void startLevel(void);                                                
    void connectToServer(void);                                           
    void getAvailableBots(void);                                          
    void getLevelBotAssignments(int load);                                
    void getBotAssignments(void);                                         
    void getBotCommand(BotAssignment &assignment, str &command);          
    BotProperties *getRandomBotName(void);                                
    void addBotItem(BotProperties *bot);                                  
    void addBotRow(BotAssignment &assignment, int row);                   
    void setBotRow(BotAssignment &assignment, int row);                   
    void assignBotSelections(void);                                       
    void setVoteType(const str &voteType);                                
    void handlePlayerListVote(void);                                      
    void handleMapListVote(void);                                         
    void handleTimeLimitVote(void);                                       
};

extern ArenaController *arenaController;  

void CreateArenaController(void);         
void DeleteArenaController(void);         
