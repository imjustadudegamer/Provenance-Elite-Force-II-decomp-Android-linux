/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "PeerLobby.h"
#include "TitlePage.h"
#include "LobbyWizard.h"
#include "ConnectPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTitlePage * TitlePage;

#define COL_NAME          0
#define COL_NUM_WAITING   1
#define COL_MAX_WAITING   2
#define COL_NUM_GAMES     3
#define COL_NUM_PLAYING   4


IMPLEMENT_DYNCREATE(CTitlePage, CPropertyPage)

CTitlePage::CTitlePage() : CPropertyPage(CTitlePage::IDD)
{
	m_message = _T("");
}

CTitlePage::~CTitlePage()
{
}

void CTitlePage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_GROUPS, m_groups);
	DDX_Control(pDX, IDC_PLAYERS, m_players);
	DDX_Control(pDX, IDC_CHAT_WINDOW, m_chatWindow);
	DDX_Text(pDX, IDC_MESSAGE, m_message);
}


BEGIN_MESSAGE_MAP(CTitlePage, CPropertyPage)
	ON_NOTIFY(NM_CLICK, IDC_GROUPS, OnClickGroups)
	ON_NOTIFY(NM_DBLCLK, IDC_GROUPS, OnDblclkGroups)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_GROUPS, OnBegindragGroups)
END_MESSAGE_MAP()


static void EnumTitlePlayersCallback
(
	PEER peer,
	PEERBool success,
	RoomType roomType,
	int index,
	const char * nick,
	int flags,
	void * param
)
{
	if(!success)
	{
		Wizard->MessageBox("Error listing title-room players.");
		return;
	}

	if(index == -1)
		return;

	TitlePage->UpdatePlayerPing(nick, 9999);
	GSI_UNUSED(peer);
	GSI_UNUSED(flags);
	GSI_UNUSED(roomType);
	GSI_UNUSED(param);
}

static PEERBool joinSuccess;
static void JoinTitleRoomCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	joinSuccess = success;

	if(success)
	{
		peerEnumPlayers(Wizard->m_peer, TitleRoom, EnumTitlePlayersCallback, NULL);
	}
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(roomType);
	GSI_UNUSED(result);
}

static void ListGroupRoomsCallback
(
	PEER peer,
	PEERBool success,
	int groupID,
	SBServer server,
	const char * name,
	int numWaiting,
	int maxWaiting,
	int numGames,
	int numPlaying,
	void * param
)
{
	if(!success)
	{
		Wizard->MessageBox("Listing groups failed!");
		Wizard->EndDialog(IDOK);
		return;
	}

	if(groupID == 0)
		return;

	CListCtrl * groups = &TitlePage->m_groups;
	int nIndex = groups->InsertItem(LVIF_TEXT | LVIF_PARAM,
		0,
		name,
		0, 0,
		0,
		(LPARAM)groupID);
	if(nIndex == -1)
		return;

	LV_ITEM item;
	CString str;
	item.mask = LVIF_TEXT;
	item.iItem = nIndex;
	item.iSubItem = COL_NUM_WAITING;
	str.Format("%d", numWaiting);
	item.pszText = (LPSTR)(LPCSTR)str;
	groups->SetItem(&item);
	item.iSubItem = COL_MAX_WAITING;
	str.Format("%d", maxWaiting);
	item.pszText = (LPSTR)(LPCSTR)str;
	groups->SetItem(&item);
	item.iSubItem = COL_NUM_GAMES;
	str.Format("%d", numGames);
	item.pszText = (LPSTR)(LPCSTR)str;
	groups->SetItem(&item);
	item.iSubItem = COL_NUM_PLAYING;
	str.Format("%d", numPlaying);
	item.pszText = (LPSTR)(LPCSTR)str;
	groups->SetItem(&item);
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(server);
}

BOOL CTitlePage::OnSetActive() 
{
	Wizard->SetWizardButtons(PSWIZB_BACK);

	::SetWindowText(Wizard->m_nextButtonWnd, "&Join >");

	m_groups.DeleteAllItems();

	m_chatWindow.ResetContent();

	Wizard->StartHourglass();
	peerJoinTitleRoom(Wizard->m_peer, NULL, JoinTitleRoomCallback, NULL, PEERTrue);
	Wizard->StopHourglass();
	if(!joinSuccess)
	{
		MessageBox("Error joining the title room.");
		return FALSE;
	}

	peerListGroupRooms(Wizard->m_peer, "", ListGroupRoomsCallback, NULL, PEERFalse);

	return CPropertyPage::OnSetActive();
}

BOOL CTitlePage::OnKillActive() 
{
	::SetWindowText(Wizard->m_nextButtonWnd, "&Next >");

	m_players.DeleteAllItems();

	if(Wizard->m_peer)
		peerLeaveRoom(Wizard->m_peer, TitleRoom, NULL);

	return CPropertyPage::OnKillActive();
}

LRESULT CTitlePage::OnWizardNext() 
{
	UpdateData();

	int nIndex = m_groups.GetSelectionMark();
	if(nIndex == -1)
	{
		MessageBox("You must have a group selected.");
		return -1;
	}

	JoinGroup(nIndex);

	return (LRESULT)GROUP_PAGE;
}

BOOL CTitlePage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	m_groups.InsertColumn(COL_NAME, "Group", LVCFMT_LEFT, 90);
	m_groups.InsertColumn(COL_NUM_WAITING, "Num Waiting", LVCFMT_LEFT, 75);
	m_groups.InsertColumn(COL_MAX_WAITING, "Max Waiting", LVCFMT_LEFT, 75);
	m_groups.InsertColumn(COL_NUM_GAMES, "Num Games", LVCFMT_LEFT, 75);
	m_groups.InsertColumn(COL_NUM_PLAYING, "Num Playing", LVCFMT_LEFT, 75);
	ListView_SetExtendedListViewStyle(m_groups.m_hWnd,LVS_EX_FULLROWSELECT);

	m_players.InsertColumn(COL_NAME, "Player", LVCFMT_LEFT, 120);

	return TRUE;
}

int CTitlePage::FindPlayer(const char *nick)
{
	CString loweredNick = nick;
	loweredNick.MakeLower();
	nick = loweredNick;

	LVFINDINFO findInfo;
	findInfo.flags = LVFI_STRING;
	findInfo.psz = nick;

	int nIndex = m_players.FindItem(&findInfo);

	return nIndex;
}

void CTitlePage::UpdatePlayerPing(const char *nick, int ping)
{
	LVITEM item;

	if(strcasecmp(nick, ConnectPage->m_nick) == 0)
		ping = 0;

	CString loweredNick = nick;
	loweredNick.MakeLower();
	nick = loweredNick;

	int nIndex = FindPlayer(nick);

	if(nIndex == -1)
	{
		item.iItem = 0;
		item.iSubItem = 0;
		item.mask = LVIF_TEXT;
		item.pszText = (char *)nick;

		nIndex = m_players.InsertItem(&item);
		if(nIndex == -1)
			return;
	}

#if 0
	char intValue[16];
	sprintf(intValue, "%d", ping); 
	item.iItem = nIndex;
	item.iSubItem = 1;
	item.mask = LVIF_TEXT;
	item.pszText = intValue;
	m_players.SetItem(&item);
#endif
}

void CTitlePage::RemovePlayer(const char * nick)
{
	CString loweredNick = nick;
	loweredNick.MakeLower();
	nick = loweredNick;

	int nIndex = FindPlayer(nick);

	m_players.DeleteItem(nIndex);
}

void CTitlePage::ChangePlayerNick(const char * oldNick, const char * newNick)
{
	CString loweredNick = oldNick;
	loweredNick.MakeLower();
	oldNick = loweredNick;
	loweredNick = newNick;
	loweredNick.MakeLower();
	newNick = loweredNick;

	int nIndex = FindPlayer(oldNick);

	LVITEM item;
	item.iItem = nIndex;
	item.iSubItem = 0;
	item.mask = LVIF_TEXT;
	item.pszText = (char *)newNick;
	m_players.SetItem(&item);
}

void CTitlePage::SendMessage()
{
	UpdateData();

	if(m_message == "")
		return;

	peerMessageRoom(Wizard->m_peer, TitleRoom, m_message, NormalMessage);

	m_message = "";

	UpdateData(FALSE);
}

void CTitlePage::OnDblclkGroups(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMHEADER * header = (NMHEADER *)pNMHDR;

	if(header->iItem != -1)
		JoinGroup(header->iItem);

	*pResult = 0;
}

void CTitlePage::OnClickGroups(NMHDR* pNMHDR, LRESULT* pResult) 
{
	BOOL enable = (m_groups.GetSelectedCount() > 0);
	::EnableWindow(Wizard->m_nextButtonWnd, enable);

	*pResult = 0;
	GSI_UNUSED(pNMHDR);
}
void CTitlePage::OnBegindragGroups(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnClickGroups(pNMHDR, pResult);
}

static void JoinGroupRoomCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	joinSuccess = success;
	GSI_UNUSED(roomType);
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(result);
}

void CTitlePage::JoinGroup(int nIndex)
{
	int groupID = (int)m_groups.GetItemData(nIndex);
	ASSERT(groupID);

	Wizard->StartHourglass();
	peerJoinGroupRoom(Wizard->m_peer, groupID, JoinGroupRoomCallback, NULL, PEERTrue);
	Wizard->StopHourglass();
	if(!joinSuccess)
	{
		MessageBox("Error joining the group room.");
		return;
	}

	Wizard->SetActivePage(GROUP_PAGE);
}
