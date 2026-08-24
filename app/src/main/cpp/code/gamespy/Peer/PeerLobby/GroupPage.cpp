
#include "stdafx.h"
#include "PeerLobby.h"
#include "GroupPage.h"
#include "LobbyWizard.h"
#include "ConnectPage.h"
#include "TitlePage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CGroupPage * GroupPage;

#define COL_NAME          0
#define COL_PING          1
#define COL_RUNNING       2
#define COL_NUM_PLAYERS   3


IMPLEMENT_DYNCREATE(CGroupPage, CPropertyPage)

CGroupPage::CGroupPage() : CPropertyPage(CGroupPage::IDD)
{
	m_message = _T("");
}

CGroupPage::~CGroupPage()
{
}

void CGroupPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PLAYERS, m_players);
	DDX_Control(pDX, IDC_GAMES, m_games);
	DDX_Control(pDX, IDC_CHAT_WINDOW, m_chatWindow);
	DDX_Control(pDX, IDC_PROGRESS, m_progress);
	DDX_Text(pDX, IDC_MESSAGE, m_message);
}


BEGIN_MESSAGE_MAP(CGroupPage, CPropertyPage)
	ON_BN_CLICKED(IDC_CREATE, OnCreate)
	ON_NOTIFY(NM_CLICK, IDC_GAMES, OnClickGames)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_GAMES, OnBegindragGames)
	ON_NOTIFY(NM_DBLCLK, IDC_GAMES, OnDblclkGames)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

static void ListingGamesCallback
(
	PEER peer,
	PEERBool success,
	const char * name,
	SBServer server,
	PEERBool staging,
	int msg,
	int progress,
	void * param
)
{
	LVITEM item;

	if(!success)
	{
		Wizard->MessageBox("Listing games failed!");
		Wizard->EndDialog(IDOK);
		return;
	}

	CListCtrl * games = &GroupPage->m_games;
	CListedGame * game = NULL;
	BOOL doUpdate = FALSE;
	int nIndex = -1;

	GroupPage->m_progress.SetPos(progress);

	if(msg == PEER_CLEAR)
	{
		GroupPage->ClearGames();
	}
	else if(msg == PEER_ADD)
	{
		game = new CListedGame;
		game->server = server;
		game->name = name;
		game->staging = staging;
		game->ping = SBServerGetPing(server);
		game->numPlayers = SBServerGetIntValue(server, "numplayers", 0);
		game->maxPlayers = SBServerGetIntValue(server, "maxplayers", 0);

		nIndex = games->InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE,
			0,
			(LPCTSTR)name,
			0, 0,
			staging ? Wizard->m_stagingRoomIndex : Wizard->m_runningGameIndex,
			(LPARAM)game);
		if(nIndex == -1)
		{
			delete game;
			return;
		}

		doUpdate = TRUE;
	}
	else if(msg == PEER_UPDATE)
	{
		nIndex = GroupPage->FindListedGame(server);
		if(nIndex != -1)
		{
			game = (CListedGame *)games->GetItemData(nIndex);
			game->name = name;
			game->staging = staging;
			game->ping = SBServerGetPing(server);
			game->numPlayers = SBServerGetIntValue(server, "numplayers", 0);
			game->maxPlayers = SBServerGetIntValue(server, "maxplayers", 0);

			item.mask = LVIF_IMAGE;
			item.iItem = nIndex;
			item.iSubItem = 0;
			item.iImage = staging ? Wizard->m_stagingRoomIndex : Wizard->m_runningGameIndex;
			games->SetItem(&item);

			doUpdate = TRUE;
		}
	}
	else if(msg == PEER_REMOVE)
	{
		nIndex = GroupPage->FindListedGame(server);
		if(nIndex != -1)
		{
			delete (CListedGame *)games->GetItemData(nIndex);
			games->DeleteItem(nIndex);
		}
	}

	if(doUpdate)
	{
		item.mask = LVIF_TEXT;
		item.iItem = nIndex;
		item.iSubItem = COL_PING;
		char buffer[32];
		sprintf(buffer, "%d", game->ping);
		item.pszText = buffer;
		games->SetItem(&item);
		item.iSubItem = COL_RUNNING;
		if(staging)
			item.pszText = "No";
		else
			item.pszText = "Yes";
		games->SetItem(&item);
		item.iSubItem = COL_NUM_PLAYERS;
		sprintf(buffer, "%d/%d", game->numPlayers, game->maxPlayers);
		item.pszText = buffer;
		games->SetItem(&item);
	}

	GSI_UNUSED(param);
	GSI_UNUSED(peer);
}

static void EnumPlayersCallback
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
		Wizard->MessageBox("Error listing players.");
		return;
	}

	if(index == -1)
		return;

	GroupPage->UpdatePlayerPing(nick, 9999);

	GSI_UNUSED(param);
	GSI_UNUSED(flags);
	GSI_UNUSED(roomType);
	GSI_UNUSED(peer);
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
		peerEnumPlayers(Wizard->m_peer, TitleRoom, EnumPlayersCallback, NULL);
	}

	GSI_UNUSED(param);
	GSI_UNUSED(roomType);
	GSI_UNUSED(result);
	GSI_UNUSED(peer);
}

BOOL CGroupPage::OnSetActive() 
{
	Wizard->SetWizardButtons(PSWIZB_BACK);

	::SetWindowText(Wizard->m_nextButtonWnd, "&Join >");

	m_progress.SetPos(0);

	ClearGames();

	m_chatWindow.ResetContent();

	if(!Wizard->m_groupRooms)
	{
		Wizard->StartHourglass();
		peerJoinTitleRoom(Wizard->m_peer, NULL, JoinTitleRoomCallback, NULL, PEERTrue);
		Wizard->StopHourglass();
		if(!joinSuccess)
		{
			MessageBox("Error joining the title room.");
			return FALSE;
		}
	}
	else
	{
		peerEnumPlayers(Wizard->m_peer, GroupRoom, EnumPlayersCallback, NULL);
	}

	unsigned char fields[] = { GAMEVER_KEY, NUMPLAYERS_KEY, MAXPLAYERS_KEY };
	peerStartListingGames(Wizard->m_peer, fields, sizeof(fields), NULL, ListingGamesCallback, NULL);

	if(Wizard->m_groupRooms)
	{
		m_players.InsertColumn(COL_NAME, "Player", LVCFMT_LEFT, 70);
		m_players.InsertColumn(COL_PING, "Ping", LVCFMT_LEFT, 50);
	}
	else
		m_players.InsertColumn(COL_NAME, "Player", LVCFMT_LEFT, 120);

	return CPropertyPage::OnSetActive();
}

BOOL CGroupPage::OnKillActive() 
{
	if(!Wizard->m_groupRooms)
		peerLeaveRoom(Wizard->m_peer, TitleRoom, NULL);

	m_players.DeleteColumn(0);
	if(Wizard->m_groupRooms)
		m_players.DeleteColumn(1);

	::SetWindowText(Wizard->m_nextButtonWnd, "&Next >");

	m_players.DeleteAllItems();

	return CPropertyPage::OnKillActive();
}

LRESULT CGroupPage::OnWizardNext() 
{
	UpdateData();

	int nIndex = m_games.GetSelectionMark();
	if(nIndex == -1)
	{
		MessageBox("You must have a game selected.");
		return -1;
	}

	JoinGame(nIndex);

	return (LRESULT)1;
}

LRESULT CGroupPage::OnWizardBack() 
{
	if(Wizard->m_groupRooms)
		peerLeaveRoom(Wizard->m_peer, GroupRoom, NULL);
	else
		return IDD_CONNECT_PAGE;

	return CPropertyPage::OnWizardBack();
}

BOOL CGroupPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	m_games.InsertColumn(COL_NAME, "Server Name", LVCFMT_LEFT, 170);
	m_games.InsertColumn(COL_PING, "Ping", LVCFMT_LEFT, 50);
	m_games.InsertColumn(COL_RUNNING, "Running", LVCFMT_LEFT, 60);
	m_games.InsertColumn(COL_NUM_PLAYERS, "Num Players", LVCFMT_LEFT, 75);
	ListView_SetExtendedListViewStyle(m_games.m_hWnd,LVS_EX_FULLROWSELECT);

	m_games.SetImageList(&Wizard->m_imageList, LVSIL_SMALL);

	return TRUE;
}

void CGroupPage::OnDestroy() 
{
	CPropertyPage::OnDestroy();

	ClearGames();
}

void CGroupPage::ClearGames()
{
	int count = m_games.GetItemCount();
	int i;
	for(i = 0 ; i < count ; i++)
	{
		LVITEM item;
		item.mask = LVIF_PARAM;
		item.iItem = i;
		item.iSubItem = 0;
		if(m_games.GetItem(&item) && item.lParam)
			delete (CListedGame *)item.lParam;
	}

	m_games.DeleteAllItems();
}

void CGroupPage::OnCreate() 
{
	peerStopListingGames(Wizard->m_peer);

	Wizard->SetActivePage(CREATE_PAGE);
}

void CGroupPage::OnDblclkGames(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMHEADER * header = (NMHEADER *)pNMHDR;

	if(header->iItem != -1)
		JoinGame(header->iItem);

	*pResult = 0;
}

void CGroupPage::OnClickGames(NMHDR* pNMHDR, LRESULT* pResult) 
{
	BOOL enable = (m_games.GetSelectedCount() > 0);
	::EnableWindow(Wizard->m_nextButtonWnd, enable);

	*pResult = 0;

	GSI_UNUSED(pNMHDR);
}
void CGroupPage::OnBegindragGames(NMHDR* pNMHDR, LRESULT* pResult) 
{
	OnClickGames(pNMHDR, pResult);
}

int CGroupPage::FindListedGame(SBServer server)
{
	int count = m_games.GetItemCount();
	int i;
	CListedGame * game;
	for(i = 0 ; i < count ; i++)
	{
		game = (CListedGame *)m_games.GetItemData(i);
		if(game && game->server == server)
			return i;
	}
	return -1;
}

int CGroupPage::FindPlayer(const char * nick)
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

void CGroupPage::UpdatePlayerPing(const char * nick, int ping)
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

	char intValue[16];
	sprintf(intValue, "%d", ping); 
	item.iItem = nIndex;
	item.iSubItem = 1;
	item.mask = LVIF_TEXT;
	item.pszText = intValue;
	m_players.SetItem(&item);
}

void CGroupPage::RemovePlayer(const char * nick)
{
	CString loweredNick = nick;
	loweredNick.MakeLower();
	nick = loweredNick;

	int nIndex = FindPlayer(nick);

	m_players.DeleteItem(nIndex);
}

void CGroupPage::ChangePlayerNick(const char * oldNick, const char * newNick)
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

void CGroupPage::JoinGame(int nIndex)
{
	CListedGame * game = (CListedGame *)m_games.GetItemData(nIndex);
	ASSERT(game);

	if(game->staging)
	{
		Wizard->SetActivePage(STAGING_PAGE);
	}
	else
	{
		CString buffer = "You are now playing at ";
		buffer += SBServerGetPublicAddress(game->server);
		buffer += ".\nHit enter when you are done.";
		MessageBox(buffer);
	}
}

void CGroupPage::SendMessage()
{
	UpdateData();

	if(m_message == "")
		return;

	peerMessageRoom(Wizard->m_peer, Wizard->m_groupRooms?GroupRoom:TitleRoom, m_message, NormalMessage);

	m_message = "";

	UpdateData(FALSE);
}
