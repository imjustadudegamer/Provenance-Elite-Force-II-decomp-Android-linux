
#include "stdafx.h"
#include "PeerLobby.h"
#include "StagingPage.h"
#include "LobbyWizard.h"
#include "GroupPage.h"
#include "ConnectPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CStagingPage * StagingPage;

#define COL_NAME          0
#define COL_PING          1


IMPLEMENT_DYNCREATE(CStagingPage, CPropertyPage)

CStagingPage::CStagingPage() : CPropertyPage(CStagingPage::IDD)
{
	m_message = _T("");
	m_ready = 0;
}

CStagingPage::~CStagingPage()
{
}

void CStagingPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PLAYERS, m_players);
	DDX_Control(pDX, IDC_CHAT_WINDOW, m_chatWindow);
	DDX_Text(pDX, IDC_MESSAGE, m_message);
	DDX_Radio(pDX, IDC_NOT_READY, m_ready);
}


BEGIN_MESSAGE_MAP(CStagingPage, CPropertyPage)
	ON_BN_CLICKED(IDC_READY, OnReady)
	ON_BN_CLICKED(IDC_NOT_READY, OnNotReady)
END_MESSAGE_MAP()

static void EnumStagingPlayersCallback
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
	if(nick)
		StagingPage->UpdatePlayerPing(nick, 9999);
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(flags);
	GSI_UNUSED(index);
	GSI_UNUSED(roomType);
	GSI_UNUSED(success);
}

PEERBool joinStagingSuccess;
void JoinStagingRoomCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	joinStagingSuccess = success;
	GSI_UNUSED(peer);
	GSI_UNUSED(roomType);
	GSI_UNUSED(param);
	GSI_UNUSED(result);
}

BOOL CStagingPage::OnSetActive() 
{
	Wizard->SetWizardButtons(PSWIZB_BACK | PSWIZB_DISABLEDFINISH);

	m_chatWindow.ResetContent();

	if(!Wizard->m_hosting)
	{
		int nIndex = GroupPage->m_games.GetSelectionMark();
		if(nIndex == -1)
		{
			MessageBox("You must have a game selected.");
			return 1;
		}

		CListedGame * game = (CListedGame *)GroupPage->m_games.GetItemData(nIndex);

		Wizard->StartHourglass();
		peerJoinStagingRoom(Wizard->m_peer, game->server, NULL, JoinStagingRoomCallback, Wizard, PEERTrue);
		Wizard->StopHourglass();
		if(!joinStagingSuccess)
		{
			MessageBox("Error joining the staging room.");
			return FALSE;
		}
		
		peerStopListingGames(Wizard->m_peer);
	}

	peerEnumPlayers(Wizard->m_peer, StagingRoom, EnumStagingPlayersCallback, Wizard);

	m_ready = 0;

	return CPropertyPage::OnSetActive();
}

BOOL CStagingPage::OnKillActive() 
{
	m_players.DeleteAllItems();

	return CPropertyPage::OnKillActive();
}

LRESULT CStagingPage::OnWizardBack() 
{
	peerLeaveRoom(Wizard->m_peer, StagingRoom, NULL);

	if(Wizard->m_hosting)
	{
		return IDD_CREATE_PAGE;
	}
	else
	{
		return IDD_GROUP_PAGE;
	}

}

BOOL CStagingPage::OnWizardFinish() 
{
	peerStartGame(Wizard->m_peer, "", PEER_KEEP_REPORTING);

	MessageBox("You are now playing!\nHit enter when you are done.");

	peerStopGame(Wizard->m_peer);

	return FALSE;
}

BOOL CStagingPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	HWND hWnd = ::FindWindowEx(GetParent()->m_hWnd, NULL, "Button", "Finish");
	if(hWnd)
		::SetWindowText(hWnd, "&Start Playing");

	m_players.InsertColumn(COL_NAME, "Player", LVCFMT_LEFT, 70);
	m_players.InsertColumn(COL_PING, "Ping", LVCFMT_LEFT, 50);

	m_players.SetImageList(&Wizard->m_imageList, LVSIL_SMALL);

	return TRUE;
}

void CStagingPage::OnReady() 
{
	peerSetReady(Wizard->m_peer, PEERTrue);
}

void CStagingPage::OnNotReady() 
{
	peerSetReady(Wizard->m_peer, PEERFalse);
}

int CStagingPage::FindPlayer(const char * nick)
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

void CStagingPage::UpdatePlayerPing(const char * nick, int ping)
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
		item.mask = LVIF_TEXT | LVIF_IMAGE;
		item.pszText = (char *)nick;
		item.iImage = Wizard->m_redSmileyIndex;

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

void CStagingPage::UpdatePlayerReady(const char * nick, BOOL ready)
{
	LVITEM item;

	CString loweredNick = nick;
	loweredNick.MakeLower();
	nick = loweredNick;

	int nIndex = FindPlayer(nick);

	if(nIndex != -1)
	{
		item.iItem = nIndex;
		item.iSubItem = 0;
		item.mask = LVIF_IMAGE;
		if(ready)
			item.iImage = Wizard->m_greenSmileyIndex;
		else
			item.iImage = Wizard->m_redSmileyIndex;
		m_players.SetItem(&item);
	}
	else
	{
		item.iItem = 0;
		item.iSubItem = 0;
		item.mask = LVIF_TEXT | LVIF_IMAGE;
		item.pszText = (char *)nick;
		if(ready)
			item.iImage = Wizard->m_greenSmileyIndex;
		else
			item.iImage = Wizard->m_redSmileyIndex;

		nIndex = m_players.InsertItem(&item);
		if(nIndex == -1)
			return;

		item.iItem = nIndex;
		item.iSubItem = 1;
		item.mask = LVIF_TEXT;
		item.pszText = "9999";
		m_players.SetItem(&item);
	}
}

void CStagingPage::RemovePlayer(const char * nick)
{
	CString loweredNick = nick;
	loweredNick.MakeLower();
	nick = loweredNick;

	int nIndex = FindPlayer(nick);

	m_players.DeleteItem(nIndex);
}

void CStagingPage::ChangePlayerNick(const char * oldNick, const char * newNick)
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

void CStagingPage::SendMessage()
{
	UpdateData();

	if(m_message == "")
		return;

	peerMessageRoom(Wizard->m_peer, StagingRoom, m_message, NormalMessage);

	m_message = "";

	UpdateData(FALSE);
}

void CStagingPage::CheckEnableFinish()
{
	PEERBool allReady = peerAreAllReady(Wizard->m_peer);

	if(Wizard->m_hosting && allReady)
		Wizard->SetWizardButtons(PSWIZB_BACK | PSWIZB_FINISH);
	else
		Wizard->SetWizardButtons(PSWIZB_BACK | PSWIZB_DISABLEDFINISH);
}