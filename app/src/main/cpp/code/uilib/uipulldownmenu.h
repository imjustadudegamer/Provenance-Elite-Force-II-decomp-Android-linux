
#pragma once

class uipull_describe
{
public:
    str                           title;
    UColor                        highlightFGColor;
    UColor                        highlightBGColor;
    UColor                        FGColor;
    UColor                        BGColor;
    UIReggedMaterial             *material;
    UIReggedMaterial             *selected_material;
    float                         popup_width;
    Container<uipopup_describe *> desc;

    uipull_describe();
    uipull_describe(str title, UIReggedMaterial *material, UIReggedMaterial *selected_material);
};

inline uipull_describe::uipull_describe()
{
    title             = "";
    material          = NULL;
    selected_material = NULL;
    popup_width       = 0.0f;
}

inline uipull_describe::uipull_describe(
    str newTitle, UIReggedMaterial *newMaterial, UIReggedMaterial *newSelectedMaterial
)
{
    this->title             = newTitle;
    this->material          = newMaterial;
    this->selected_material = newSelectedMaterial;
    this->popup_width       = 0.0f;
}

class UIPulldownMenu : public UIWidget
{
protected:
    Container<uipull_describe *> m_desc;
    Listener                    *m_listener;
    int                          m_submenu;
    UIPopupMenu                 *m_submenuptr;
    UColor                       m_highlightBGColor;
    UColor                       m_highlightFGColor;
    bool                         m_pulledDown;

public:
    CLASS_PROTOTYPE(UIPulldownMenu);

protected:
    float            getDescWidth(uipull_describe *desc);
    uipull_describe *getPulldown(str title);
    void             HighlightBGColor(Event *ev);
    void             HighlightFGColor(Event *ev);

public:
    UIPulldownMenu();
    ~UIPulldownMenu();

    void Create(UIWidget *parent, Listener *listener, const UIRect2D& rect);
    void CreateAligned(UIWidget *parent, Listener *listener);
    void MousePressed(Event *ev);
    void MouseDragged(Event *ev);
    void MouseReleased(Event *ev);
    void removeSubMenu(void);
    void ChildKilled(Event *ev);
    void AddUIPopupDescribe(const char *title, uipopup_describe *d);
    void setHighlightFGColor(UColor c);
    void setHighlightBGColor(UColor c);
    void setPopupWidth(const str& menu, float width);
    void setPopupHighlightFGColor(str menu, UColor c);
    void setPopupHighlightBGColor(str menu, UColor c);
    void setPopupFGColor(str menu, UColor c);
    void setPopupBGColor(str menu, UColor c);
    void setPulldownShader(const char *title, UIReggedMaterial *mat);
    void setSelectedPulldownShader(const char *title, UIReggedMaterial *mat);
    void Realign(void) override;
    void Draw(void) override;
};
