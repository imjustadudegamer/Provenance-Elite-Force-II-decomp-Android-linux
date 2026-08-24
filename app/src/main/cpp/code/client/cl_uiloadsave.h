
#pragma once

class UIFAKKLoadGameClass : public UIListCtrl
{
    bool m_bRemovePending;

public:
    CLASS_PROTOTYPE(UIFAKKLoadGameClass);

protected:
    void     SelectGame(Event *ev);
    void     LoadGame(Event *ev);
    void     SaveGame(Event *ev);
    void     RemoveGame(Event *ev);
    void     DeleteGame(Event *ev);
    void     NoDeleteGame(Event *ev);
    qboolean KeyEvent(int key, unsigned int time) override;
    void     UpdateUIElement(void) override;

public:
    UIFAKKLoadGameClass();
    ~UIFAKKLoadGameClass();

    void SetupFiles(void);
};
