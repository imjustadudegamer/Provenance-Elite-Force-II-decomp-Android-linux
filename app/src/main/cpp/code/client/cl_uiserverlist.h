
#pragma once

#include "../gamespy/goaceng.h"

class UIFAKKServerList : public UIListCtrl
{
    friend class FAKKServerListItem;

public:
    CLASS_PROTOTYPE(UIFAKKServerList);

protected:
    struct GServerListImplementation *m_serverList;

public:
    UIFAKKServerList();

protected:
    void     SelectServer(Event *ev);
    void     ConnectServer(Event *ev);
    qboolean KeyEvent(int key, unsigned int time) override;
    void     UpdateUIElement(void) override;
    void     RefreshServerList(Event *ev);
    void     RefreshLANServerList(Event *ev);

public:
    void Draw(void) override;
};
