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

#include "usignal.h"
#include "../qcommon/q_shared.h"
#include "../corepp/str.h"
#include "ui_public.h"
#include "uirect2d.h"
#include "ucolor.h"
#include "uifont.h"
#include <cstddef>

class UIFont;

class UIReggedMaterial
{
    uihandle_t hMat;
    str        name;
    bool       isSet;
    bool       isGot;

public:
    UIReggedMaterial();

    uihandle_t GetMaterial(void);
    void       ReregisterMaterial(void);
    void       CleanupMaterial(void);
    void       SetMaterial(const str& name);
    void       RefreshMaterial(void);
    str        GetName(void);
};

class UIReggedModel
{
    str       name;
    qhandle_t handle;

public:
    UIReggedModel();

    void      SetModel(const str& name);
    void      ReregisterModel(void);
    qhandle_t GetHandle(void) const;
    str       GetName(void) const;
};

#define WF_STRETCH_VERTICAL       (1 << 0)
#define WF_STRETCH_HORIZONTAL     (1 << 1)
#define WF_DIRECTED               (1 << 2)
#define WF_TILESHADER             (1 << 3)
#define WF_NOPARENTADJUST         (1 << 4)
#define WF_STATBAR_REPEATSHADER   (1 << 5)

#define WA_LEFT    (1 << 0)
#define WA_RIGHT   (1 << 1)
#define WA_TOP     (1 << 3)
#define WA_BOTTOM  (1 << 4)
#define WA_CENTERX (1 << 5)
#define WA_CENTERY (1 << 6)
#define WA_FULL    (WA_LEFT | WA_RIGHT | WA_TOP | WA_BOTTOM | WA_CENTERX | WA_CENTERY)

typedef class UIReggedMaterial UIReggedMaterial;

typedef enum {
    border_none,
    border_3D,
    border_indent,
    border_outline
} borderstyle_t;

typedef enum {
    D_NONE = 0,
    D_FROM_LEFT = 1,
    D_FROM_RIGHT = 2,
    D_FROM_BOTTOM = 3,
    D_FROM_TOP = 4,
    D_STRETCH_LEFT = 5,
    D_STRETCH_RIGHT = 6,
    D_STRETCH_DOWN = 7,
    D_STRETCH_UP = 8,
    D_ROLL_LEFT = 9,
    D_ROLL_RIGHT = 10,
    D_ROLL_UP = 11,
    D_ROLL_DOWN = 12
} direction_t;

typedef enum {
    MOTION_IN,
    MOTION_OUT
} motion_t;

struct WidgetCvarPair {
    str cvarname;
    str value;
    int type;
};


typedef enum {
    WIDGET_STATE_NORMAL = 0,
    WIDGET_STATE_HOVER = 1,
    WIDGET_STATE_SELECTED = 2,
    WIDGET_STATE_ARMED = 3,
    WIDGET_STATE_DISABLED = 4,

    WIDGET_STATE_COUNT = 5
} WidgetState;

struct widget_state_color_t {
    UColor   color;
    qboolean set;
};

struct widget_state_bordercolor_t {
    UBorderColor color;
    qboolean     set;
};

struct WidgetStatData {
    int  statnum;
    int  value;
    bool equal;
};

class WidgetKeyCommand
{
public:
    int key;
    str command;
};

class UIWidget : public USignal
{
    friend class Menu;
    friend class UILayout;
    friend class UIWindowManager;
    friend struct UIWidgetLayout;

protected:
    UIRect2D              m_frame;
    UIRect2D              m_drawRegion;
    class WidgetMover    *m_mover;

    void move(Event *ev);
    void moveTo(Event *ev);
    void moveToCvar(Event *ev);
    void moveToStat(Event *ev);
    void moveToProp(Event *ev);
    void restart(Event *ev);
    void wait(Event *ev);
    void clearCommands(Event *ev);
    void restartCommands(Event *ev);
    void setPosition(Event *ev);
    UIPoint2D             m_startingpos;
    UIPoint2D             m_origin;
    UIRect2D              m_screenframe;
    UIPoint2D             m_screenorigin;
    UIRect2D              m_clippedframe;
    UIPoint2D             m_clippedorigin;
    widget_state_color_t       m_foregroundColors[5];
    widget_state_color_t       m_backgroundColors[5];
    widget_state_color_t       m_textShadowColors[5];
    widget_state_color_t       m_materialColors[5];
    widget_state_bordercolor_t m_borderColors[5];
    // through them sets the colour but not entry 0's "set" flag; retail's getters
    UColor                &m_foreground_color; 
    UColor                &m_background_color; 
    UBorderColor          &m_border_color;     
    int                   m_borderWidth;
    Container<str>        m_disableCvars;  
    Container<float>      m_disableValues; 
    Container<str>        m_disableOps;    
    borderstyle_t         m_borderStyle;
    bool                  m_visible;
    bool                  m_enabled;
    UIFont               *m_font;
    UISize2D              m_size;
    class UIWidget       *m_parent;
    Container<UIWidget *> m_children;
    str                   m_name;
    bool                  m_canactivate;
    str                   m_cvarname;
    str                   m_cvarvalue;
    str                   m_command;
    Container<str *>      m_showcommands;    
    Container<str *>      m_hidecommands;    
    Container<str *>      m_pushcommands;
    Container<str *>      m_popcommands;
    Container<WidgetKeyCommand *> m_keyCommands;
    str                   m_title;
    str                   m_rawtitle;
    str                   m_labelText;
    float                 m_indent;
    float                 m_bottomindent;
    UIReggedMaterial     *m_material;
    UIReggedMaterial     *m_hovermaterial;
    UIReggedMaterial     *m_pressedmaterial;
    UIReggedMaterial     *m_selectedmaterial;
    UIReggedMaterial     *m_disabledmaterial;
    direction_t           m_direction;
    direction_t           m_direction_orig;
    motion_t              m_motiontype;
    float                 m_starttime;
    // itself does not have.  UIWidget::setShow @0x000A21A0's hidden->shown arm (store
    float                 m_fadeStartTime;
    float                 m_fadetime;
    float                 m_fadeDelay;
    float                 m_fadeout;
    float                 m_fadeGate;
    float                 m_alpha;
    float                 m_local_alpha;
    float                 m_motiontime;
    str                   m_stopsound;
    str                   m_clicksound;
    str                   m_altclicksound;
    str                   m_showsound;
    str                   m_hidesound;
    int                   m_align;
    int                   m_flags;
    // pool 0x00020B72).  Not read by the draw path in the shipped engine.
    float                 m_textureCoordinates[4];
    // test on a field retail does not have.  (It is NOT m_bVirtual: retail's
    bool                  m_bMoveActive;
    WidgetState           m_widgetState;
    bool                  m_dying;
    int                   m_ordernum;
    int                   m_configstring_index;
    fonthorzjustify_t     m_iFontAlignmentHorizontal;
    fontvertjustify_t     m_iFontAlignmentVertical;
    qboolean              m_bVirtual;
    str                            m_gameplayPropertyGroup;
    str                            m_gameplayPropertyName;
    int                            m_gameplayPropertyType;
    bool                           m_gameplayPropertyInUse;
    Container<WidgetStatData *> m_statVisibilityData;
    float                 m_depth;
    bool                  m_noresize;
    bool                  m_hideDuringLoad;
    str                   m_hovercommand;     
    int                   m_hovercommandCol;  
    str                   m_hoverexitcommand; 
    str                   m_hoversound;       
    str                   m_altcommand;
    Container<WidgetCvarPair *> m_cvarVisibilityData;
    UIRect2D m_authoredFrame;
    bool     m_bFrameLatched = false;
    /* In-game HUD menus carry the effective HUD scale here; 1.0 means retail. `m_hudScaleSet' is
       what puts the menu in the HUD scope at all, so a menu whose factor is 1.0 -- the multiplayer
       scoreboard, or a HUD set after the cvar returns to 1.0 -- is still laid out by the scope and
       therefore restored to its authored geometry instead of keeping an old factor. */
    float    m_hudScale = 1.0f;
    bool     m_hudScaleSet = false;
    /* one descent per UI update: MenuManager::UpdateAllMenus walks every registered menu container and UIWindowManager::UpdateViews then walks the same
       containers as its own children, so the same visible subtrees were descended twice.  The window manager also owns children that are not menu containers, so its
       walk stays and the stamp makes a second descent over a subtree already walked this update a no-op. */
    int m_updateStamp = -1;

public:
    CLASS_PROTOTYPE(UIWidget);

    static void FrameStampBump(void);
    static int  FrameStamp(void);

    void (*m_commandhandler)(const char *, void *);

    void        setHudScale(float scale);
    float       getHudScale(void) const;
    bool        isHudScaleSet(void) const;
    bool        isHudFullScreen(void) const;
    static void HudScalePush(float scale);
    static void HudScalePop(void);
    static float HudScaleCurrent(void);
    static float HudTextScale(void);

    // PAINT TEMP PROBE helper -- PORT-ONLY, TEMPORARY, NEVER MERGE.  Prints this
    void PAINTDumpState(int index, const char *menuName);

    // UITEXT TEMP PROBE helper -- PORT-ONLY, TEMPORARY, NEVER MERGE.  ONE probe for the
    void UITEXTDumpTextColor(const char *accessor);

    // UIRECT TEMP PROBE accessors -- PORT-ONLY, TEMPORARY, NEVER MERGE.  Read-only view of
    UIPoint2D UIRECTProbeScreenOrigin(void) const { return m_screenorigin; }
    UIRect2D  UIRECTProbeClippedFrame(void) const { return m_clippedframe; }
    bool      UIRECTProbeVirtual(void) const { return m_bVirtual; }

protected:
    bool         addChild(UIWidget *widget);
    bool         removeChild(UIWidget *widget);
    void         PropogateCoordinateSystem(void);
    void         set2D(void);
    virtual void Draw(void);
    virtual void FrameInitialized(void);
    void         calculateDrawRegion(void);
    void         DrawTitle(float x, float y);
    virtual void Motion(void);
    virtual void AlignPosition(void);
    void         Hide(Event *ev);
    void         Activate(Event *ev);

public:
    UIWidget();
    virtual ~UIWidget() override;

    virtual void Shutdown(void);
    virtual void InitFrame(
        UIWidget   *parentview,
        float       x,
        float       y,
        float       width,
        float       height,
        int         border   = -1,
        const char *fontname = "tahoma-8"
    );
    void InitFrame(UIWidget *parentview, const UIRect2D& r, int border, const char *fontname = "Tahoma-10");
    void LayoutSize(Event *ev);
    void LayoutRect(Event *ev);
    void LayoutName(Event *ev);
    void LayoutTitle(Event *ev);
    void LayoutLabelText(Event *ev);
    void LayoutFont(Event *ev);
    void LayoutBgColor(Event *ev);
    void LayoutFgColor(Event *ev);
    void LayoutHoverFgColor(Event *ev);
    void LayoutHoverBgColor(Event *ev);
    void LayoutSelectedFgColor(Event *ev);
    void LayoutSelectedBgColor(Event *ev);
    void LayoutDisabledFgColor(Event *ev);
    void LayoutTextShadowColor(Event *ev);
    void LayoutHoverTextShadowColor(Event *ev);
    void LayoutHoverShaderColor(Event *ev);
    void LayoutArmedShaderColor(Event *ev);
    void LayoutSelectedShaderColor(Event *ev);
    void LayoutDisabledShaderColor(Event *ev);
    void LayoutArmedFgColor(Event *ev);
    void LayoutArmedBgColor(Event *ev);
    void LayoutArmedTextShadowColor(Event *ev);
    void LayoutSelectedTextShadowColor(Event *ev);
    void LayoutHoverBorderColor(Event *ev);
    void LayoutSelectedBorderColor(Event *ev);
    void LayoutDisabledBgColor(Event *ev);
    void LayoutDisabledTextShadowColor(Event *ev);
    void LayoutArmedBorderColor(Event *ev);
    void LayoutDisabledBorderColor(Event *ev);
    void LayoutShaderProperty(Event *ev);
    void SetWidgetState(Event *ev);     
    void LayoutBorderWidth(Event *ev);
    void cvarCheckDisable(Event *ev);   
    void LayoutBorderStyle(Event *ev);
    void LayoutBorderColor(Event *ev);
    void LayoutShader(Event *ev);
    void LayoutTileShader(Event *ev);
    void LayoutHoverShader(Event *ev);
    void LayoutArmedShader(Event *ev);
    void LayoutSelectedShader(Event *ev);
    void LayoutFadeOut(Event *ev);
    void LayoutAltClickSound(Event *ev);
    void Layout_TextureCoordinates(Event *ev);
    void LayoutFadeIn(Event *ev);
    void LayoutFadeDelay(Event *ev);
    void LayoutStopSound(Event *ev);
    void LayoutClickSound(Event *ev);
    void LayoutStretch(Event *ev);
    void LayoutInitData(Event *ev);
    void LayoutDirection(Event *ev);
    void LayoutAlign(Event *ev);
    void LayoutStuffCommand(Event *ev);
    void LayoutLinkCvar(Event *ev);
    void LayoutNoParentClipping(Event *ev);
    void LayoutNoParentAdjustment(Event *ev);
    void LayoutOrderNumber(Event *ev);
    // retail's does not have.  Nothing in this tree referenced either handler and
    void LayoutAliasCache(Event *ev);


    void LayoutHorizontalJustify(Event *ev);
    void LayoutVerticalJustify(Event *ev);
    void LayoutShaderColor(Event *ev);
    void LayoutHoverCommand(Event *ev);
    void LayoutHoverExitCommand(Event *ev);
    void SetHoverSound(Event *ev);       
    void SetShowSound(Event *ev);        
    void SetHideSound(Event *ev);        
    void cvarVisibility(Event *ev);      

    void            setParent(UIWidget *parent);
    class UIWidget *getParent(void);
    class UIWidget *getFirstChild(void);
    class UIWidget *getNextSibling(void);
    class UIWidget *getNextChild(UIWidget *child);
    class UIWidget *getPrevChild(UIWidget *child);
    class UIWidget *getPrevSibling(void);
    class UIWidget *getPrevSibling(UIWidget *curr);
    class UIWidget *getLastSibling(void);
    class UIWidget *findSibling(str name);

    virtual void       Enable(void);
    virtual void       Disable(void);
    bool               isEnabled(void);
    bool               IsDying(void);
    class UIWidget    *FindResponder(const UIPoint2D& pos);
    void               setFont(const char *name);
    void               setFontHorizontalAlignment(fonthorzjustify_t alignment);
    void               setFontVerticalAlignment(fontvertjustify_t alignment);
    virtual void       setShow(bool visible);
    virtual void       ChangeResolution(void);
    virtual void       turnOn(void);
    virtual void       turnOff(void);
    bool               getShow(void);
    class UColor       getBackgroundColor(void);

    /* Read-only child container for the stale-raw-pointer accessor; it exposes
       what retail's own Container<P8UIWidget> already is. */
    Container<UIWidget *>& getChildren(void) { return m_children; }
    virtual void       setBackgroundColor(const UColor& color, WidgetState state, bool setbordercolor);
    class UBorderColor getBorderColor(void);
    void               setBorderColor(const UBorderColor& color);
    void               setBorderColor(const UBorderColor& color, WidgetState state);
    class UColor       getTextShadowColor(void);
    class UColor       getMaterialColor(void);
    class UColor       getForegroundColor(void);
    virtual void       setForegroundColor(const UColor& color, WidgetState state);
    void               setForegroundColor(const UColor& color) { setForegroundColor(color, WIDGET_STATE_NORMAL); }
    virtual void       setTextShadowColor(const UColor& color, WidgetState state);
    virtual void       setMaterialColor(const UColor& color, WidgetState state);
    virtual bool       IsDisabled(void);
    class UColor       getStateForegroundColor(void);
    class UColor       getStateBackgroundColor(void);
    class UColor       getStateShaderColor(void);
    class UColor       getStateTextShadowColor(void);
    class UColor       getStateBorderColor(void);
    borderstyle_t      getBorderStyle(void);
    void               setBorderStyle(borderstyle_t style);
    class UISize2D     getSize(void);
    void               setSize(const UISize2D& newsize);
    class UIRect2D     getFrame(void);
    // [WIDGET:B1:whitegrid] TEMP PROBE accessors -- PORT-ONLY, TEMPORARY, NEVER MERGE.
    const char *WIDGETProbeMaterialName(void) const
    {
        return m_material ? m_material->GetName().c_str() : "(null)";
    }

    virtual void        PasteFromWindowsClipboard(void);
    virtual void        setData(const str& data);
    virtual const char *getData(void);
    virtual void        copyProperties(UIWidget& other);
    bool               isResizeable(void);
    void               setFrame(const UIRect2D& newframe);
    class UIPoint2D    getOrigin(void);
    void               setOrigin(const UIPoint2D& neworigin);
    void               setName(str name);
    const char        *getName(void);
    void               setTitle(str title);
    void               setLabelText(str text);
    const char        *getTitle(void);
    const char        *getCvarName(void);
    const char        *getMaterialName(void) const
    {
        return m_material ? m_material->GetName().c_str() : NULL;
    }
    void               setDirection(direction_t dir);
    direction_t        getDirection(void);
    void               setMotionType(motion_t type);
    motion_t           getMotionType(void);
    void               setMotionTime(float time);
    float              getMotionTime(void);
    void               setAlign(int align);
    int                getAlign(void);
    void               setMaterial(UIReggedMaterial *mat, WidgetState state);
    void               setHoverMaterial(UIReggedMaterial *mat);
    void               setPressedMaterial(UIReggedMaterial *mat);
    class UIPoint2D    getLocalPoint(const UIPoint2D& pos);
    class UIPoint2D    getGlobalPoint(const UIPoint2D& pos);
    virtual void       setBackgroundAlpha(float f);
    float              getBackgroundAlpha(void);
    void               Display(const UIRect2D& drawframe, float parent_alpha);
    virtual qboolean   KeyEvent(int key, unsigned int time);
    virtual void       CharEvent(int ch);
    virtual void       UpdateData(void);
    virtual void       UpdateUIElement(void);
    virtual void       update(void);
    virtual void       reloadStrings(void);
    void               updateCvarVisibility(void);
    bool               testCvarTypes(const Container<WidgetCvarPair *>& data);
    int                getCheckType(const str& op);
    float              getTitleWidth(void);
    float              getTitleHeight(void);
    bool               CanActivate(void);
    void               AllowActivate(bool canactivate);
    bool               IsActive(void);
    bool               IsVisible(void);
    virtual void       LinkCvar(str cvarname);
    void               LinkCommand(str cmd);
    void               ExecuteShowCommands(void);
    void               ClearShowCommands(void);
    void               ExecuteHideCommands(void);
    void               ExecutePushCommands(void);
    void               ExecutePopCommands(void);
    void               InitializeCommandHandler(void (*fcn)(const char *, void *));
    class UIWidget    *FindWidget(str name);
    void               ResetMotion(motion_t type);
    virtual void       Realign(void);
    void               BringToFrontPropogated(void);
    class UIWidget    *IsThisOrChildActive(void);
    class UIPoint2D    MouseEventToClientPoint(Event *ev);
    class UIRect2D     getClientFrame(void);
    class UIRect2D     getDrawRegion(void);
    bool               SendSignal(Event& event);
    void               ShowCommand(Event *ev);
    void               HideCommand(Event *ev);
    void               PushCommand(Event *ev);
    void               PopCommand(Event *ev);
    void               KeyCommand(Event *ev);
    void               AddFlag(int flag);
    virtual bool       StaysPressed(void) const { return false; }
    virtual void       setWidgetState(WidgetState state);
    void               setNormalState(void);
    void               setHoverState(void);
    void               setSelectedState(void);
    void               setArmedState(void);
    void               ExecuteHoverCommand(void);
    void               ExecuteHoverExitCommand(void);
    void               ExecuteAltCommand(void);
    virtual void       MouseEntered(Event *ev);
    virtual void       MouseExited(Event *ev);
    virtual void       Pressed(Event *ev);
    virtual void       RightMouseButtonPressed(Event *ev);
    virtual int        getNumItems(void);

    void               SetChildWidget(Event *ev);

    void               refreshShader(Event *ev);

    void               SetAltCommand(Event *ev);
    int                getOrderNum(void);
    class str          getCommand(void);
    void               ActivateOrder(void);
    void               EnableEvent(Event *ev);
    void               DisableEvent(Event *ev);
    void               setConfigstringIndex(int cs);
    int                getConfigstringIndex(void);
    bool               PassEventToWidget(str name, Event *ev, qboolean deleteOnFail);

    void               LayoutGameplayStringProperty(Event *ev);
    void               LayoutGameplayFloatProperty(Event *ev);
    void               LayoutGameplayVisibilityProperty(Event *ev);
    void               LayoutGameplayEnableProperty(Event *ev);
    void               LayoutGameplayShaderProperty(Event *ev);
    void               statVisibility(Event *ev);
    void               setGameplayProperty(const char *group, const char *name, int kind);
    /* a widget an adapter has retired is out of the page: no shipped visibility
       driver may bring it back on the next update walk */
    void               clearGameplayProperty(void) { m_gameplayPropertyInUse = false; }
    virtual void       updateVisibility(void);
    virtual void       updateGameplayProperty(void);
    virtual void       updateStatVisibility(void);

    void  setDepth(Event *ev);
    void  NoResize(Event *ev);
    void  setHideDuringLoad(Event *ev);
    void  setDepth(float depth) { m_depth = depth; }
    // PAINT TEMP PROBE read side of the same accessor -- PORT-ONLY, TEMPORARY, NEVER
    float getDepth(void) const { return m_depth; }
    static int DepthCompare(const void *elem1, const void *elem2);

    UIFont       *getFont() const;
};

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

struct UIWidgetLayout {
    static_assert(sizeof(UColor) == 16, "UColor must stay 4 floats");
    static_assert(sizeof(UBorderColor) == 64, "UBorderColor must stay 4 UColors");
    static_assert(offsetof(widget_state_color_t, set) == 16, "set flag sits at +16");
    static_assert(sizeof(widget_state_color_t) == 20, "colour entry stride is 20");
    static_assert(offsetof(widget_state_bordercolor_t, set) == 64, "set flag sits at +64");
    static_assert(sizeof(widget_state_bordercolor_t) == 68, "border entry stride is 68");
    static_assert(sizeof(widget_state_color_t[5]) == 100, "one colour table is 100 bytes");
    static_assert(sizeof(widget_state_bordercolor_t[5]) == 340, "the border table is 340 bytes");

    static_assert(
        offsetof(UIWidget, m_backgroundColors) - offsetof(UIWidget, m_foregroundColors) == 100,
        "background table follows the foreground table"
    );
    static_assert(
        offsetof(UIWidget, m_textShadowColors) - offsetof(UIWidget, m_backgroundColors) == 100,
        "text shadow table follows the background table"
    );
    static_assert(
        offsetof(UIWidget, m_materialColors) - offsetof(UIWidget, m_textShadowColors) == 100,
        "material colour table follows the text shadow table"
    );
    static_assert(
        offsetof(UIWidget, m_borderColors) - offsetof(UIWidget, m_materialColors) == 100,
        "border table follows the material colour table"
    );
    static_assert(
        offsetof(UIWidget, m_borderColors) - offsetof(UIWidget, m_foregroundColors) == 400,
        "retail: border table is 400 bytes below the foreground table"
    );

    static_assert(
        offsetof(UIWidget, m_widgetState) > offsetof(UIWidget, m_flags),
        "the state word sits above the flag word"
    );
    static_assert(
        offsetof(UIWidget, m_depth) > offsetof(UIWidget, m_widgetState),
        "m_depth sits above the state word"
    );
};

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

class UIWidgetContainer : public UIWidget
{
    UColor   m_bgfill;
    qboolean m_fullscreen;
    int      m_vidmode;
    int      m_currentwidnum;
    int      m_maxordernum;
    qboolean m_bNoEscape = qfalse;
    bool     m_nomouse        = false;
    bool     m_allowGameFocus = false;
    /* The child the pad's focus highlight is on. EF2's uilib has no keyboard focus of its own --
       retail's UIWindowManager::KeyEvent @0x000B2C50 only forwards to the bind target and the
       mouse-activated control -- so the pad model lives here, in the container, and is armed only
       by a navigation key. A SafePtr, because a menu transition deletes its widgets. */
    SafePtr<UIWidget> m_focusWidget;


public:
    CLASS_PROTOTYPE(UIWidgetContainer);

private:
    void AlignPosition(void) override;
    void Draw(void) override;

public:
    UIWidgetContainer();

    void      Realign(void) override;
    void      SetBGFill(Event *ev);
    void      SetFullscreen(Event *ev);
    void      SetVidMode(Event *ev);
    void      SetNoEscape(Event *ev);
    void      NoMouse(Event *ev);
    void      AllowGameFocus(Event *ev);
    int       getVidMode(void);
    qboolean  isFullscreen(void);
    qboolean  getNoEscape(void) const { return m_bNoEscape; }
    bool      CheckNoMouse(void) const { return m_nomouse; }
    bool      getAllowGameFocus(void) const { return m_allowGameFocus; }
    void      SetActiveWidgetOrderNum(UIWidget *wid);
    void      SetActiveWidgetOrderNum(int num);
    void      SetLastActiveWidgetOrderNum(void);

    /* Pad focus: which child holds it, and movement among the visible, enabled
       ones by child order.  Movement is CLAMPED at both ends so a held key in a
       list cannot run past the last entry. */
    UIWidget *getFocusWidget(void) const;
    void      setFocusWidget(UIWidget *wid);
    void      clearFocusWidget(void);
    qboolean  hasPadStop(void);
    qboolean  moveFocus(int dir);
    qboolean  moveFocus(int dir, int key, qboolean directional);
};

extern Event W_Destroyed;
extern Event W_SizeChanged;
extern Event W_FrameChanged;
extern Event W_OriginChanged;
extern Event W_Activated;
extern Event EV_Widget_Activate;
extern Event EV_Widget_Deactivate;
extern Event W_Deactivated;
extern Event EV_Widget_Hide;
extern Event EV_Widget_Enable;
extern Event EV_Widget_Disable;
extern Event W_RealignWidget;
extern Event W_Draw;
extern Event EV_Layout_Size;
extern Event EV_Layout_Rect;
extern Event EV_Layout_Name;
extern Event EV_Layout_Title;
extern Event EV_Layout_LabelText;
extern Event EV_Layout_Font;
extern Event EV_Layout_BGColor;
extern Event EV_Layout_FGColor;
extern Event EV_Layout_Borderstyle;
extern Event EV_Layout_BorderColor;
extern Event EV_Layout_Shader;
extern Event EV_Layout_TileShader;
extern Event EV_Layout_HoverShader;
extern Event EV_Layout_PressedShader;
extern Event EV_Layout_FadeIn;
extern Event EV_Layout_FadeDelay;
extern Event EV_Layout_StopSound;
extern Event EV_Layout_ClickSound;
extern Event EV_Layout_Stretch;
extern Event EV_Layout_InitData;
extern Event EV_Layout_Direction;
extern Event EV_Layout_Align;
extern Event EV_Layout_StuffCommand;
extern Event EV_Layout_LinkCvar;
extern Event EV_Layout_NoParentClipping;
extern Event EV_Layout_NoParentAdjustment;
extern Event EV_Widget_HideCommand;
extern Event EV_Widget_ShowCommand;
extern Event EV_Widget_OrderNumber;
extern Event EV_Layout_AliasCache;
extern Event EV_Layout_BGFill;
extern Event EV_Layout_Fullscreen;
extern Event EV_Layout_VidMode;
extern Event EV_Widget_PushCommand;
extern Event EV_Widget_PopCommand;
extern Event EV_Widget_KeyCommand;
extern Event EV_Widget_SetChild;
extern Event EV_Widget_MwcMove;
extern Event EV_Widget_MwcMoveTo;
extern Event EV_Widget_MwcMoveToCvar;
extern Event EV_Widget_MwcMoveToStat;
extern Event EV_Widget_MwcMoveToProp;
extern Event EV_Widget_MwcRestart;
extern Event EV_Widget_MwcWait;
extern Event EV_Widget_ClearCommands;
extern Event EV_Widget_RestartCommands;
extern Event EV_Widget_MwcSetPosition;
extern Event EV_Layout_Depth;
extern Event EV_Layout_NoResize;
extern Event EV_Layout_HideDuringLoad;
extern Event EV_Layout_NoMouse;
extern Event EV_Layout_GameFocus;
extern Event EV_Layout_HorizontalJustify;
extern Event EV_Layout_VerticalJustify;
extern Event EV_Layout_ShaderColor;
extern Event EV_Layout_HoverCommand;
extern Event EV_Layout_HoverExitCommand;
extern Event EV_Layout_HoverSound;
extern Event EV_Layout_CvarVisibility;
extern Event EV_Layout_HoverFGColor;
extern Event EV_Layout_HoverBGColor;
extern Event EV_Layout_SelectedFGColor;
extern Event EV_Layout_SelectedBGColor;
extern Event EV_Layout_DisabledFGColor;
extern Event EV_Layout_TextShadowColor;
extern Event EV_Layout_HoverTextShadowColor;
extern Event EV_Layout_HoverShaderColor;
extern Event EV_Layout_ArmedShaderColor;
extern Event EV_Layout_SelectedShaderColor;
extern Event EV_Layout_DisabledShaderColor;
extern Event EV_Layout_BorderWidth;
extern Event EV_Layout_ArmedShader;
extern Event EV_Layout_SelectedShader;
extern Event EV_Layout_FadeOut;
extern Event EV_Layout_AltClickSound;
extern Event EV_Layout_TextureCoordinates;
extern Event EV_Layout_CvarCheckDisable;
extern Event EV_Layout_ArmedFGColor;
class WidgetMoverCommand
{
public:
    WidgetMoverCommand(void);

    int   type;
    float x;
    float y;
    float xpos;
    float ypos;
    float time;
    float xscale;
    float yscale;
    float xscalepos;
    float yscalepos;
    str   arg1;
    str   arg2;
};

class WidgetMover
{
public:
    WidgetMover(UIWidget *widget);
    ~WidgetMover();

    void Motion(void);
    void setPosition(float x, float y);

    void move(Event *ev);
    void moveTo(Event *ev);
    void moveToCvar(Event *ev);
    void moveToStat(Event *ev);
    void moveToProp(Event *ev);
    void restart(Event *ev);
    void wait(Event *ev);
    void clearCommands(Event *ev);
    void restartCommands(Event *ev);
    void ChangeResolution(void);
    static void ParseEquationString(const str &eq, float& value, str& name, float& scale, float def);

    int NumCommands(void) const { return m_commands.NumObjects(); }
    int CurrentCommand(void) const { return m_currentcommand; }

private:
    WidgetMoverCommand *getCommand(void);
    void                deleteCommands(void);
    void                move(WidgetMoverCommand *cmd);
    void                moveTo(WidgetMoverCommand *cmd);
    void                moveToCvar(WidgetMoverCommand *cmd);
    void                moveToStat(WidgetMoverCommand *cmd);
    void                moveToProp(WidgetMoverCommand *cmd);
    float               Fraction(const WidgetMoverCommand *cmd);
    void                SetFrameFraction(const WidgetMoverCommand *cmd, float frac);

    Container<WidgetMoverCommand *> m_commands;       
    int                             m_currentcommand;  
    bool                            m_started;         
    float                           m_startx;          
    float                           m_starty;          
    float                           m_endx;            
    float                           m_endy;            
    float                           m_starttime;       
    float                           m_endtime;         
    float                           m_time;            
    float                           m_totaltime;       
    UIWidget                       *m_widget;          
};

extern Event EV_Layout_ArmedBGColor;
extern Event EV_Layout_ArmedTextShadowColor;
extern Event EV_Layout_SelectedTextShadowColor;
extern Event EV_Layout_HoverBorderColor;
extern Event EV_Layout_SelectedBorderColor;
extern Event EV_Layout_ArmedBorderColor;
extern Event EV_Layout_DisabledBorderColor;
extern Event EV_Layout_DisabledBGColor;
extern Event EV_Layout_DisabledTextShadowColor;
extern Event EV_Layout_GameplayShaderProperty;

void SetColor(const UColor& color, float alpha);
void DrawBox(const UIRect2D& rect, const UColor& color, float alpha);
void Draw3DBox(const UIRect2D& rect, bool indent, const UBorderColor& color, float alpha);
void DrawBox(float x, float y, float width, float height, const UColor& color, float alpha);
void Draw3DBox(float x, float y, float width, float height, bool indent, const UBorderColor& color, float alpha);
void DrawMac3DBox(const UIRect2D& rect, bool indent, const UBorderColor& color, int inset, float alpha);
void DrawBoxWithSolidBorder(
    const UIRect2D& rect, const UColor& inside, const UColor& outside, int size, int flags, float alpha
);
