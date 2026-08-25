
#pragma once

typedef enum {
    L_STATBAR_NONE = 0,
    L_STATBAR_HORIZONTAL = 1,
    L_STATBAR_VERTICAL = 2
} statbar_orientation_t;

typedef enum {
    L_STATBARDIR_NONE = 0,
    L_STATBARDIR_RIGHT = 1,
    L_STATBARDIR_UP = 2,
    L_STATBARDIR_DOWN = 3
} statbar_direction_t;

typedef struct {
    float max;
    float multiplier;
} statmultiplier_t;


int CL_GetStatByName(const char *name); 
int CL_SafePlayerStat(int idx);         

class UIFakkLabel : public UILabel
{
protected:
    SafePtr<inventory_item_t> m_lastitem;  
    int                       m_lastitemindex;
    int                       m_stat;                  
    int                       m_invalidPlayerStatValue;    
    int                       m_hasInvalidPlayerStatValue;  
    int                       m_stat_configstring;     
    int                       m_maxstat;               
    int                       m_itemindex;             
    int                       m_inventoryrendermodelindex;  
    qboolean                  m_rendermodel;           
    statbar_orientation_t     m_statbar_or;            
    float                     m_statbar_min;           
    float                     m_statbar_max;           
    float                     m_lastfrac;              
    float                     m_flashtime;             
    vec3_t                    m_offset;                
    vec3_t                    m_rotateoffset;          
    vec3_t                    m_angles;                
    float                     m_scale;                 
    str                       m_anim;                  
    UIReggedMaterial         *m_statbar_material;        
    UIReggedMaterial         *m_statbar_material_flash;  
    bool                      m_useGameplayProperty;
    str                       m_stringPropertyGroup;    
    str                       m_stringPropertyName;     
    int                       m_warningLevel;         
    str                       m_warningSound;         
    int                       m_warningSoundFreq;     
    int                       m_lastWarnSoundTime;    
    int                       m_warnSoundWhenChanged;  
    Container<statmultiplier_t> m_statMultipliers;    
    int                       m_statbarDir;           
    bool                      m_displayStatAsTime;

public:
    CLASS_PROTOTYPE(UIFakkLabel);

    UIFakkLabel();

    void LayoutPlayerStat(Event *ev);
    void LayoutPlayerStatConfigstring(Event *ev);
    void LayoutMaxPlayerStat(Event *ev);
    void LayoutItemIndex(Event *ev);
    void InventoryRenderModelIndex(Event *ev);
    void LayoutStatbar(Event *ev);
    void LayoutStatbarDirection(Event *ev);
    void LayoutStatMultiplier(Event *ev);
    void LayoutStatbarShader(Event *ev);
    void LayoutStatbarTileShader(Event *ev);
    void LayoutStatbarShader_Flash(Event *ev);
    void LayoutStatbarRepeatShader(Event *ev);
    void LayoutStatbarTileShader_Flash(Event *ev);
    void LayoutInvalidPlayerStatValue(Event *ev);
    void setWarningLevel(Event *ev);
    void setWarningSound(Event *ev);
    void setWarningSoundWhenChanged(Event *ev);
    void displayStatAsTime(Event *ev);
    void LayoutRenderModel(Event *ev);
    void LayoutRenderModelOffset(Event *ev);
    void LayoutRenderModelRotateOffset(Event *ev);
    void LayoutRenderModelAngles(Event *ev);
    void LayoutRenderModelScale(Event *ev);
    void LayoutRenderModelAnim(Event *ev);
    void LayoutStatVisibility(Event *ev);
    void LayoutGameplayStatisticProperty(Event *ev);
    virtual void updateGameplayProperty(void) override;

    void DrawStatbar(float frac);

    float getGameplayFloat(const char *groupName, const char *propertyName);
    float getPlayerstatFloat(const char *statName);

    void Draw(void) override;
};

class UIColorStatLabel : public UIFakkLabel
{
protected:
    int    m_lastStatValue;    
    float  m_flashTime;        
    UColor m_increaseColor;    
    UColor m_decreaseColor;    
    UColor m_foregroundSave;   
    UColor m_flashColor;       
    int    m_warningCount;     

public:
    CLASS_PROTOTYPE(UIColorStatLabel);

    UIColorStatLabel();

    void setIncreaseColor(Event *ev);
    void setDecreaseColor(Event *ev);

    void UpdateUIElement(void) override;
};

class UIAmmoColorLabel : public UIColorStatLabel
{
protected:
    int m_lastAmmoIndex;    

public:
    CLASS_PROTOTYPE(UIAmmoColorLabel);

    UIAmmoColorLabel();

    void UpdateUIElement(void) override;
};

enum {
    OBJDISP_TITLE,       
    OBJDISP_SHADER,      
    OBJDISP_DESCRIPTION, 
    OBJDISP_STATUS       
};

class UIObjectiveLabel : public UILabel
{
protected:
    int               m_objectiveIndex;
    UIRect2D          m_textBox;          
    UColor            m_incompleteColor;
    UColor            m_completeColor;
    UColor            m_failedColor;
    int               m_displayType;
    UIReggedMaterial *m_incompleteMat;
    UIReggedMaterial *m_completeMat;
    UIReggedMaterial *m_failedMat;
    int               m_objectiveState;  

public:
    CLASS_PROTOTYPE(UIObjectiveLabel);

    UIObjectiveLabel();

    void SetObjectiveIndex(Event *ev);
    void LayoutTextBox(Event *ev);
    void SetIncompleteTextColor(Event *ev);
    void SetCompleteTextColor(Event *ev);
    void SetFailedTextColor(Event *ev);
    void SetDisplayType(Event *ev);
    void SetIncompleteShader(Event *ev);
    void SetCompleteShader(Event *ev);
    void SetFailedShader(Event *ev);

    int         HandleObjective(void);
    const char *ObjectiveText(void);

    void Draw(void) override;
};

class UIInformationLabel : public UILabel
{
protected:
    int       m_informationIndex;    
    UColor    m_textColor;           
    UIRect2D  m_textBoxFrame;        
    UIRect2D  m_screenFrame;         

public:
    CLASS_PROTOTYPE(UIInformationLabel);

    UIInformationLabel();

    void SetInformationIndex(Event *ev);
    void SetTextBoxFrame(Event *ev);
    void SetTextColor(Event *ev);

    int  HandleInformation(void);
    void ShowMe(void);
    void HideMe(void);
    void SetShader(void);
    void UpdateData(void) override;
    void ChangeResolution(void) override;

    void Draw(void) override;
};

class UIIcon : public UIWidget
{
protected:
    int               m_stat;
    int               m_lastCs;
    UIReggedMaterial *m_iconMat;

public:
    CLASS_PROTOTYPE(UIIcon);

    UIIcon();

    void iconPlayerStat(Event *ev);
    void Draw(void) override;
};

// (0x179BEC -> slot 15 setMaterial) and never stores it on the bar -- the
enum Direction {
    DIR_NEXT     = 0,
    DIR_PREVIOUS = 1
};

void ParseEquationString(const str& equation, float& value, str& remainder, float& sign, float defaultValue);

class UIMoveCommand
{
public:
    int   commandtype;  
    float x;            
    float y;            
    float startx;       
    float starty;       
    float time;         
    float xcvarscale;   
    float ycvarscale;   
    float xcvarbase;    
    float ycvarbase;    
    str   xcvarname;    
    str   ycvarname;    

    UIMoveCommand();
};

class UIMove : public UIWidget
{
protected:
    Container<UIMoveCommand *> m_commands;       
    int                        m_currentCommand;    
    float                      m_startX;         
    float                      m_startY;         
    float                      m_endX;           
    float                      m_endY;           
    float                      m_startTime;      
    float                      m_endTime;        
    float                      m_consumedTime;   
    float                      m_currentTime;    
    bool                       m_bCommandStarted;   
    bool m_moveActive;                           

public:
    CLASS_PROTOTYPE(UIMove);

    UIMove();
    ~UIMove();

    void Move(Event *ev);
    void MoveTo(Event *ev);
    void MoveToCvar(Event *ev);
    void Wait(Event *ev);
    void Restart(Event *ev);
    void RestartCommands(Event *ev);
    void ClearCommands(Event *ev);

protected:
    void           restartCommands(void);
    void           deleteCommands(void);
    void           clearCommands(void);
    void           addCommand(UIMoveCommand *cmd);
    UIMoveCommand *getCommand(void);
    float          getFraction(UIMoveCommand *cmd);
    void           move(UIMoveCommand *cmd);
    void           moveTo(UIMoveCommand *cmd);
    void           moveToCvar(UIMoveCommand *cmd);
    void Motion(void) override;
    void ChangeResolution(void) override;
    void turnOn(void) override;
    void turnOff(void) override;
};

class UIInventoryItem : public UIMove
{
public:
    inventory_item_t             *m_item;       
    bool                          m_bValid;     
    Container<inventory_item_t *> m_items;      
    bool                          m_bSelected;  

public:
    CLASS_PROTOTYPE(UIInventoryItem);

    UIInventoryItem();
    ~UIInventoryItem();

    bool  isInventoryItemValid(void);
    bool  sendEquipItemCommand(void);
    str   getShowName(void);
    void  render3dModel(void);

    void Draw(void) override;
};

class UIInventoryType : public UILabel
{
public:
    inventory_type_t             *m_type;          
    Container<UIInventoryItem *> m_items;          
    Container<UIInventoryItem *> m_visibleItems;   
    int                          m_currentItem;    

public:
    CLASS_PROTOTYPE(UIInventoryType);

    UIInventoryType();
    ~UIInventoryType();

    void addUIInventoryItem(inventory_item_t *item, int width, int height);
    bool switchToNextItem(bool bAllowEmpty);
    bool switchToPreviousItem(void);
    void updateItems(const UIRect2D& frame, float width, float height, float scale, UIWidget *widget);

    void Enable(void) override;
    void Disable(void) override;
    void setShow(bool visible) override;
    void Draw(void) override;
};

class UIInventoryBar : public UIWidget
{
protected:
    int                    m_currentType;         
    Container<UIInventoryType *> m_types;         
    UIInventoryItem       *m_selectedWidget;      
    UIWidget              *m_selectedTypeWidget;  
    UIReggedMaterial *m_noAmmoSelectionShader;    
    UIReggedMaterial *m_notAvailSelectionShader;  
    UIReggedMaterial *m_emptyGroupShader;         
    str               m_selectSound;              
    str               m_changeSound;              
    str               m_rejectSound;              
    vec2_t            m_itemSize;                 
    vec2_t            m_typeSize;                 
    vec2_t            m_authoredItemSize;         
    vec2_t            m_authoredTypeSize;         
    vec2_t            m_spacing;                  
    float             m_lastSwitchTime;           
    str               m_autoswitchCvarName;       
    cvar_t           *m_autoswitchCvar;           
    bool              m_enabled;                  

public:
    CLASS_PROTOTYPE(UIInventoryBar);

    UIInventoryBar();

    void setItemSize(Event *ev);
    void setTypeSize(Event *ev);
    void ChangeResolution(void) override;

    void setBackgroundColor(const UColor& color, WidgetState state, bool setbordercolor) override;

    void emptyGroupShader(Event *ev);
    void selectionShader(Event *ev);
    void noAmmoSelectionShader(Event *ev);
    void notAvailSelectionShader(Event *ev);
    void setSelectSound(Event *ev);
    void setChangeSound(Event *ev);
    void setRejectSound(Event *ev);
    void setBarEnable(Event *ev);
    void setBarDisable(Event *ev);

    void Draw(void) override;

    void update(void) override;

    qboolean KeyEvent(int key, unsigned int time) override;

protected:
    void updateScaledSizes(void);
    void loadItems(void);
    void updateTypes(void);
    void updateSelectedTypeWidget(void);
    void equipItem(bool force);
    int  getItem(Direction dir);
    int  getNextType(int type);
    int  getPreviousType(int type);
    int  getTypeIndex(const str& name);
    int  getCurrentWeaponPosition(void);
    void showInventory(void);
    void hideInventory(void);
    void showInventory(Event *ev);
    void hideInventory(Event *ev);
    void setShow(bool visible) override;
    void Enable(void) override;

    void nextItem(void);
    void previousItem(void);
    void nextItem(Event *ev);
    void previousItem(Event *ev);
    void warpToItem(Event *ev);
};

extern UIInventoryBar *inventoryBar;

extern Event EV_Widget_PrintDialog;
extern Event EV_Widget_ResetDialog;

class DialogEntry
{
public:
    str           _name;
    str           _text;
    unsigned char _flags;
    DialogEntry  *_next;
};

DialogEntry *GetDialogEntry(const char *name);

class UIDialogTextConsole : public UIWidget
{
protected:
    Container<str> m_lines;         
    int            m_maxLines;      
    int            m_maxLineWidth;  
    float          m_fadeInTime;    
    float          m_holdTime;      
    float          m_fadeOutTime;   
    float          m_alpha;         
    str            m_currentName;   
    str            m_currentText;   
    int            m_lastPrintTime; 
    int            m_scrollDelay;   
    int            m_firstLine;     
    bool           m_useAlpha;      
    unsigned char  m_displayLevel;  
    int            m_lastPrintDelta;

public:
    CLASS_PROTOTYPE(UIDialogTextConsole);

    UIDialogTextConsole();

    void Draw(void) override;
    void ChangeResolution(void) override;
    void PrintDialog(Event *ev);
    void ResetDialog(Event *ev);
    void OnSizeChanged(Event *ev);
    void OnSizeChanged(void);
    void LayoutRect(Event *ev);
    void PrintDialog(const str& name, const str& text, float lip);
    void PrintDialog(unsigned char level, const str& text, bool useAlpha);
    void update(void) override;
};

extern Event EV_Widget_DialogHeadEntity;

class UIDialogHead : public UIWidget
{
protected:
    int                   m_entnum;     
    qhandle_t             m_tiki;       
    const entityState_t  *m_state;      
    int                   m_sequence;   

public:
    CLASS_PROTOTYPE(UIDialogHead);

    UIDialogHead();

    void Draw(void) override;
    void SetEntityNum(Event *ev);
};


#define RADAR_MAX_BLIPS 256
#define RADAR_MAX_OBJECTIVE_BLIPS 5

typedef struct radarBlip_s {
    vec3_t pos;       
    vec3_t color;     
    int    selected;  
} radarBlip_t;

typedef struct radarObjectiveBlip_s {
    vec3_t pos;       
    vec3_t color;     
    int    selected;  
    int    inRange;   
} radarObjectiveBlip_t;

class UIRadarDialog : public UIWidget
{
protected:
    UIReggedModel *m_simpleGroundModel;           
    UIReggedModel *m_complexGroundModel;          
    UIReggedModel *m_blipModel;                   
    UIReggedModel *m_selectedBlipModel;           
    UIReggedModel *m_objectiveBlipModel;          
    UIReggedModel *m_objectiveSelectedBlipModel;  
    UIReggedModel *m_objectiveArrowModel;         
    UIReggedModel *m_fovModel;                    

    qhandle_t m_groundModel;                 

    int                  m_numBlips;                     
    radarBlip_t          m_blips[RADAR_MAX_BLIPS];       
    radarObjectiveBlip_t m_objectiveBlips[RADAR_MAX_OBJECTIVE_BLIPS];  
    int                  m_numObjectiveBlips;            

    float  m_blipScale;                   
    vec3_t m_viewAngles;                  
    bool m_viewAnglesFromGame;
    float  m_matrix[3][3];                
    float  m_range;                       
    float  m_groundModelBoundingSize;     
    bool   m_groundModelBoundingSizeSet;  
    str    m_radarString;                 
    int    m_complexMode;                 
    UColor m_complexBackgroundColor;      
    UColor m_simpleBackgroundColor;       

public:
    CLASS_PROTOTYPE(UIRadarDialog);

    UIRadarDialog();
    ~UIRadarDialog();

    void SetBlipModel(Event *ev);
    void SetSelectedBlipModel(Event *ev);
    void SetObjectiveBlipModel(Event *ev);
    void SetObjectiveSelectedBlipModel(Event *ev);
    void SetObjectiveArrow(Event *ev);
    void SetComplexGroundModel(Event *ev);
    void SetSimpleGroundModel(Event *ev);
    void SetFovModel(Event *ev);
    void SetBlipScale(Event *ev);
    void SetRadarRange(Event *ev);
    void SetGroundModelBoundingSize(Event *ev);
    void SetComplexBackgroundColor(Event *ev);
    void SetSimpleBackgroundColor(Event *ev);

    void setShow(bool visible) override;
    void Draw(void) override;

    void AddBlip(const vec3_t pos, const vec3_t color, const char *name, int selected, int alwaysShow);
    void AddMissionObjectiveBlip(const vec3_t pos, const vec3_t color, const char *name, int selected, int inRange);
    void SetViewAngles(const vec3_t angles);

private:
    void DrawRadar(void);
    void DrawFov(void);
    void DrawBlips(void);
    void DrawMissionObjectiveBlips(void);
    void AddSpriteToScene(
        qhandle_t hModel, const vec3_t origin, const vec3_t rotatedOffset, const vec3_t offset,
        const vec3_t angles, float scale, const vec4_t color
    );
    void AddModelToScene(
        qhandle_t hModel, const vec3_t origin, const vec3_t rotatedOffset, const vec3_t offset,
        const vec3_t angles, const str& anim, float scale, bool morphGate
    );
};

void Radar_AddBlip(const vec3_t pos, const vec3_t color, const char *name, int selected, int alwaysShow);
void Radar_AddMissionObjectiveBlip(
    const vec3_t pos, const vec3_t color, const char *name, int selected, int inRange
);
void Radar_SetViewAngles(const vec3_t angles);
