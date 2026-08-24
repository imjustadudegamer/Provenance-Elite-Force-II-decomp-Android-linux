
#pragma once

class UIPulldownMenuContainer : public UIWidget
{
protected:
    Container<uipopup_describe *> m_popups;
    Container<char *>             m_dataContainer;
    UIPulldownMenu               *m_menu;

public:
    CLASS_PROTOTYPE(UIPulldownMenuContainer);

    UIPulldownMenuContainer();
    ~UIPulldownMenuContainer();

protected:
    void FrameInitialized(void) override;
    void setBackgroundAlpha(float f) override;
    void setBackgroundColor(const UColor& color, WidgetState state, bool setbordercolor) override;
    void setForegroundColor(const UColor& color, WidgetState state) override;
    void SetPopupHighlightFGColor(Event *ev);
    void SetPopupHighlightBGColor(Event *ev);
    void SetPopupFGColor(Event *ev);
    void SetPopupBGColor(Event *ev);
    void SetHighlightFGColor(Event *ev);
    void SetHighlightBGColor(Event *ev);
    void SetMenuShader(Event *ev);
    void SetSelectedMenuShader(Event *ev);
    void SetPopupWidth(Event *ev); 
    void LayoutFont(Event *ev);
    void LayoutShaderColor(Event *ev);
    void LayoutHoverShaderColor(Event *ev);
    void LayoutSelectedShaderColor(Event *ev);
    void LayoutArmedShaderColor(Event *ev);
    void LayoutClickSound(Event *ev);
    void Realign(void) override;

public:
    void AddPopup(Event *ev);
    void reloadStrings(void) override;
};
