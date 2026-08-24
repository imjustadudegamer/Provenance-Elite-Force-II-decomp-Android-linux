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


#include "cl_ui.h"
#include "../gamespy/goaceng.h"

Event EV_FAKKServerList_Connect("connect", EV_DEFAULT, NULL, NULL, "Connect to the specified server");

Event EV_FAKKServerList_RefreshServerList("refreshserverlist", EV_DEFAULT, NULL, NULL, "Refresh the serverlist");

Event EV_FAKKServerList_RefreshLANServerList(
    "refreshlanserverlist", EV_DEFAULT, NULL, NULL, "Refresh the LAN serverlist"
);

class FAKKServerListItem : public UIListCtrlItem
{
public:
    str m_strings[6];

public:
    griditemtype_t getListItemType(int index) const override;
    int            getListItemValue(int i) const override;
    str            getListItemString(int i) const override;
    void           DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected) override;
};

griditemtype_t FAKKServerListItem::getListItemType(int index) const
{
    return griditemtype_t::TYPE_STRING;
}

int FAKKServerListItem::getListItemValue(int i) const
{
    return atol(m_strings[i]);
}

str FAKKServerListItem::getListItemString(int i) const
{
    return m_strings[i];
}

void FAKKServerListItem::DrawListItem(int iColumn, const UIRect2D& drawRect, bool bSelected) {}

static void UpdateServerListCallBack(GServerList serverlist, int msg, void *instance, void *param1, void *param2)
{
    UIFAKKServerList   *list;
    GServer             server;
    FAKKServerListItem *item;
    str                 sHostname;
    str                 sAddress;
    str                 sPing;
    str                 sPlayers;
    str                 sGametype;
    str                 sMapname;
    char                buf[48];

    list   = static_cast<UIFAKKServerList *>(instance);
    server = static_cast<GServer>(param1);

    Cvar_Set("dm_serverstatusbar", va("%d", (int)(uintptr_t)param2));

    if (msg == LIST_PROGRESS) {
        sHostname = ServerGetStringValue(server, (char *)"hostname", (char *)"(NONE)");
        sAddress  = ServerGetAddress(server);
        sprintf(buf, "%d", ServerGetPing(server));
        sPing     = buf;
        sPlayers  = va("%d/%d", ServerGetIntValue(server, (char *)"numplayers", 0), ServerGetIntValue(server, (char *)"maxplayers", 0));
        sGametype = ServerGetStringValue(server, (char *)"gametype", (char *)"(NONE)");
        sMapname  = ServerGetStringValue(server, (char *)"mapname", (char *)"(NONE)");

        item             = new FAKKServerListItem;
        item->m_strings[0] = sHostname;
        item->m_strings[1] = sAddress;
        item->m_strings[2] = sPing;
        item->m_strings[3] = sPlayers;
        item->m_strings[4] = sGametype;
        item->m_strings[5] = sMapname;

        list->AddItem(item);
    } else if (msg == LIST_STATECHANGED) {
        switch (ServerListState(serverlist)) {
        case GServerListState::sl_idle:
            Cvar_Set("dm_serverstatus", "Done Updating.");
            break;
        case GServerListState::sl_listxfer:
            Cvar_Set("dm_serverstatus", "Getting List.");
            break;
        case GServerListState::sl_lanlist:
            Cvar_Set("dm_serverstatus", "Searching LAN.");
            break;
        case GServerListState::sl_querying:
            Cvar_Set("dm_serverstatus", "Querying Servers.");
            break;
        default:
            break;
        }
    }
}

CLASS_DECLARATION(UIListCtrl, UIFAKKServerList, NULL) {
    {&EV_UIListBase_ItemSelected,             &UIFAKKServerList::SelectServer        },
    {&EV_UIListBase_ItemDoubleClicked,        &UIFAKKServerList::ConnectServer       },
    {&EV_FAKKServerList_RefreshServerList,    &UIFAKKServerList::RefreshServerList   },
    {&EV_FAKKServerList_RefreshLANServerList, &UIFAKKServerList::RefreshLANServerList},
    {&EV_FAKKServerList_Connect,              &UIFAKKServerList::ConnectServer       },
    {NULL,                                    NULL                                   }
};

// the literal "facfont-20" does not occur anywhere in the retail engine image.
UIFAKKServerList::UIFAKKServerList()
{
    setBackgroundColor(UWhite, WIDGET_STATE_NORMAL, true);
    Connect(this, EV_UIListBase_ItemDoubleClicked, EV_UIListBase_ItemDoubleClicked);
    Connect(this, EV_UIListBase_ItemSelected, EV_UIListBase_ItemSelected);

    AllowActivate(true);

    m_serverList = NULL;
}

void UIFAKKServerList::SelectServer(Event *ev) {}

void UIFAKKServerList::ConnectServer(Event *ev)
{
    if (getCurrentItem() <= 0) {
        return;
    }

    char cmdString[256];

    Com_sprintf(
        cmdString,
        sizeof(cmdString),
        "popmenu 0; connect %s\n",
        static_cast<const FAKKServerListItem *>(GetItem(getCurrentItem()))->getListItemString(1).c_str()
    );
    Cbuf_AddText(cmdString);
}

// UpdateServer()/CancelRefresh(), which retail does not have.
qboolean UIFAKKServerList::KeyEvent(int key, unsigned int time)
{
    switch (key) {
    case K_ENTER:
        if (getCurrentItem() > 0) {
            char cmdString[256];

            Com_sprintf(
                cmdString,
                sizeof(cmdString),
                "popmenu 0; connect %s\n",
                static_cast<const FAKKServerListItem *>(GetItem(getCurrentItem()))->getListItemString(1).c_str()
            );
            Cbuf_AddText(cmdString);
        }
        return qtrue;
    case K_UPARROW:
        if (getCurrentItem() <= 1) {
            return qfalse;
        }
        TrySelectItem(getCurrentItem() - 1);
        return qtrue;
    case K_DOWNARROW:
        if (getCurrentItem() >= getNumItems()) {
            return qfalse;
        }
        TrySelectItem(getCurrentItem() + 1);
        return qtrue;
    default:
        return UIListBase::KeyEvent(key, time);
    }
}

void UIFAKKServerList::UpdateUIElement(void)
{
    float width;

    RemoveAllColumns();

    width = getClientFrame().size.width;

    AddColumn("Hostname", 0, 0.2f * width, false, false);
    AddColumn("IP", 1, 0.2f * width, false, false);
    AddColumn("Ping", 2, 0.1f * width, true, true);
    AddColumn("Players", 3, 0.1f * width, false, false);
    AddColumn("Gametype", 4, 0.2f * width, false, false);
    AddColumn("Map", 5, 0.2f * width, false, false);

    uWinMan.ActivateControl(this);
}

void UIFAKKServerList::RefreshServerList(Event *ev)
{
    if (m_serverList) {
        ServerListClear(m_serverList);
    } else {
        m_serverList = ServerListNew("fakk2", "fakk2", "YDYBNE", 10, (void *)&UpdateServerListCallBack, 1, this);
    }

    ServerListUpdate(m_serverList, true);
}

void UIFAKKServerList::RefreshLANServerList(Event *ev)
{
    if (m_serverList) {
        ServerListClear(m_serverList);
    } else {
        m_serverList = ServerListNew("fakk2", "fakk2", "YDYBNE", 10, (void *)&UpdateServerListCallBack, 1, this);
    }

    ServerListLANUpdate(m_serverList, true, 29251, 29351, 10);
}

// deferred first-listing path are not retail.
void UIFAKKServerList::Draw(void)
{
    if (m_serverList) {
        ServerListThink(m_serverList);
    }

    UIListCtrl::Draw();
}
