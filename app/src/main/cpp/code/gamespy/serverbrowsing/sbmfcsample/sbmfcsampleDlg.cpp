/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "sbmfcsample.h"
#include "sbmfcsampleDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define TIMER_ID         100
#define TIMER_FREQUENCY  10

#define COL_SERVERNAME   0
#define COL_PING         1
#define COL_PLAYERS      2
#define COL_MAPNAME      3
#define COL_GAMETYPE     4

#define COL_PNAME        0
#define COL_PPING        1
#define COL_PSCORE       2


#define MAX_UPDATES      30


CSbmfcsampleDlg::CSbmfcsampleDlg(CWnd* pParent )
	: CDialog(CSbmfcsampleDlg::IDD, pParent)
{
	m_filter = _T("");
	m_gamename = _T("");
	m_startPort = 0;
	m_endPort = 0;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSbmfcsampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SERVERS, m_servers);
	DDX_Control(pDX, IDC_PROGRESS, m_progress);
	DDX_Control(pDX, IDC_SERVERLIST, m_serverList);
	DDX_Control(pDX, IDC_PLAYERLIST, m_playerList);
	DDX_Text(pDX, IDC_FILTER, m_filter);
	DDX_Text(pDX, IDC_GAMENAME, m_gamename);
	DDX_Text(pDX, IDC_STARTP, m_startPort);
	DDX_Text(pDX, IDC_ENDP, m_endPort);
}

BEGIN_MESSAGE_MAP(CSbmfcsampleDlg, CDialog)
	ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
	ON_NOTIFY(NM_CLICK, IDC_SERVERLIST, OnClickServerlist)
	ON_NOTIFY(NM_DBLCLK, IDC_SERVERLIST, OnDblclkServerlist)
	ON_WM_TIMER()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_SERVERLIST, OnColumnclickServerlist)
END_MESSAGE_MAP()


BOOL CSbmfcsampleDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	m_serverList.InsertColumn(COL_SERVERNAME, "Server Name", LVCFMT_LEFT, 150, -1);
	m_serverList.InsertColumn(COL_PING, "Ping", LVCFMT_LEFT, 50, 0);
	m_serverList.InsertColumn(COL_PLAYERS, "Players", LVCFMT_LEFT, 75, 1);
	m_serverList.InsertColumn(COL_MAPNAME, "Map", LVCFMT_LEFT, 75, 2);
	m_serverList.InsertColumn(COL_GAMETYPE, "GameType", LVCFMT_LEFT, 100, 3);
	m_playerList.InsertColumn(COL_PNAME, "Player Name", LVCFMT_LEFT, 150, -1);
	m_playerList.InsertColumn(COL_PPING, "Ping", LVCFMT_LEFT, 50, 0);
	m_playerList.InsertColumn(COL_PSCORE, "Score", LVCFMT_LEFT, 50, 1);
	ListView_SetExtendedListViewStyle(m_serverList.m_hWnd, LVS_EX_FULLROWSELECT);
	ListView_SetExtendedListViewStyle(m_playerList.m_hWnd, LVS_EX_FULLROWSELECT);

	CheckRadioButton(IDC_INTERNET, IDC_LAN, IDC_INTERNET);

	CheckRadioButton(IDC_GOA, IDC_QR2, IDC_QR2);

	m_serverBrowser = NULL;

	m_timerID = 0;

	m_servers.SetWindowText("");
	
	GSIACResult result;
	GSIStartAvailableCheck("gmtest");
	while((result = GSIAvailableCheckThink()) == GSIACWaiting)
		msleep(5);
	if(result != GSIACAvailable)
	{
		MessageBox("The backend is not available\n");
		return TRUE;
	}

	return TRUE;
}

BOOL CSbmfcsampleDlg::DestroyWindow() 
{
	if(m_serverBrowser)
	{
		ServerBrowserFree(m_serverBrowser);
		m_serverBrowser = NULL;
	}
	
	return CDialog::DestroyWindow();
}

void CSbmfcsampleDlg::OnTimer(UINT nIDEvent) 
{
	if(nIDEvent == m_timerID)
	{
		ServerBrowserThink(m_serverBrowser);
	}
	
	CDialog::OnTimer(nIDEvent);
}

void CSbmfcsampleDlg::OnRefresh() 
{
	UpdateData();

	if(!CreateServerList())
		return;

	SBState state = ServerBrowserState(m_serverBrowser);
	if((state != sb_connected) && (state != sb_disconnected))
	{
		ServerBrowserHalt(m_serverBrowser);
		return;
	}

	ServerBrowserClear(m_serverBrowser);
	m_serverList.DeleteAllItems();

	m_progress.SetPos(0);

	m_serverCount = 0;
	m_servers.SetWindowText("0");

	if(!m_timerID)
		m_timerID = SetTimer(TIMER_ID, TIMER_FREQUENCY, NULL);

	unsigned char fields[] = { HOSTNAME_KEY, NUMPLAYERS_KEY, MAXPLAYERS_KEY, MAPNAME_KEY, GAMETYPE_KEY };
	int numFields = sizeof(fields) / sizeof(fields[0]);

	bool internet = (IsDlgButtonChecked(IDC_INTERNET) == BST_CHECKED);

	SBError error;
	if(internet)
		error = ServerBrowserUpdate(m_serverBrowser, SBTrue, SBFalse, fields, numFields, (char *)(const char *)m_filter);
	else
		error = ServerBrowserLANUpdate(m_serverBrowser, SBTrue, (unsigned short)m_startPort, (unsigned short)m_endPort);
}

void CSbmfcsampleDlg::OnClickServerlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	m_playerList.DeleteAllItems();

	POSITION pos = m_serverList.GetFirstSelectedItemPosition();
	if(pos == NULL)
		return;
	int index = m_serverList.GetNextSelectedItem(pos);

	LVITEM item;
	item.mask = LVIF_PARAM;
	item.iItem = index;
	item.iSubItem = 0;
	m_serverList.GetItem(&item);
	SBServer server = (SBServer)item.lParam;

	if (!SBServerHasFullKeys(server)) 
	{
		HCURSOR cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

		ServerBrowserAuxUpdateServer(m_serverBrowser, server, SBFalse, SBTrue);

		SetCursor(cursor);
	}

	int count = SBServerGetIntValue(server, "numplayers", 0);

	for(int i = 0 ; i < count ; i++)
	{
		m_playerList.InsertItem(0, SBServerGetPlayerStringValue(server, i, "player", "(NO NAME)"));
		m_playerList.SetItem(0, COL_PPING, LVIF_TEXT, SBServerGetPlayerStringValue(server, i, "ping", "0"), -1, 0, 0, 0);
		m_playerList.SetItem(0, COL_PSCORE, LVIF_TEXT, SBServerGetPlayerStringValue(server, i, "score", "0"), -1, 0, 0, 0);
	}

	*pResult = 0;

	GSI_UNUSED(pNMHDR);
}

void CSbmfcsampleDlg::OnDblclkServerlist(NMHDR* pNMHDR, LRESULT* pResult) 
{

	POSITION pos = m_serverList.GetFirstSelectedItemPosition();
	if(pos == NULL)
		return;
	int index = m_serverList.GetNextSelectedItem(pos);

	LVITEM item;
	item.mask = LVIF_PARAM;
	item.iItem = index;
	item.iSubItem = 0;
	m_serverList.GetItem(&item);
	SBServer server = (SBServer)item.lParam;
	SBError error = ServerBrowserConnectToServer(m_serverBrowser, server, SBConnectCallback);
	if(error != sbe_noerror)
		MessageBox("Error starting connect to server");

	*pResult = 0;

	GSI_UNUSED(pNMHDR);
}

void CSbmfcsampleDlg::OnColumnclickServerlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	static bool ascending = false;
	ascending = !ascending;

	switch(pNMListView->iSubItem)
	{
	case COL_SERVERNAME:
		ServerBrowserSort(m_serverBrowser, (SBBool)ascending, "hostname", sbcm_stricase);
		break;
	case COL_PING:
		ServerBrowserSort(m_serverBrowser, (SBBool)ascending, "ping", sbcm_int);
		break;
	case COL_PLAYERS:
		ServerBrowserSort(m_serverBrowser, (SBBool)ascending, "numplayers", sbcm_int);
		break;
	case COL_MAPNAME:
		ServerBrowserSort(m_serverBrowser, (SBBool)ascending, "mapname", sbcm_stricase);
		break;
	case COL_GAMETYPE:
		ServerBrowserSort(m_serverBrowser, (SBBool)ascending, "gametype", sbcm_stricase);
		break;
	}

	m_serverList.SetRedraw(false);

	m_serverList.DeleteAllItems();

	m_serverCount = 0;
	m_servers.SetWindowText("0");

	for(int i = 0; i < ServerBrowserCount(m_serverBrowser) ; i++)
	{
		SBServer server = ServerBrowserGetServer(m_serverBrowser, i);
		if(SBServerHasBasicKeys(server))
			AddServer(server, FALSE);
	}

	m_serverList.SetRedraw(true);
	
	*pResult = 0;
}

void CSbmfcsampleDlg::SBCallback(ServerBrowser serverBrowser, SBCallbackReason reason, SBServer server, void *instance)
{
	CString str;
	CSbmfcsampleDlg * dlg = (CSbmfcsampleDlg *)instance;

	CString address;
	if(server)
		address.Format("%s:%d", SBServerGetPublicAddress(server), SBServerGetPublicQueryPort(server));

	switch(reason)
	{
	case sbc_serveradded:
		dlg->AddServer(server, TRUE);
		break;
	case sbc_serverupdated:
		dlg->AddServer(server, TRUE);
		break;
	case sbc_serverupdatefailed:
		break;
	case sbc_serverdeleted:
		dlg->RemoveServer(server);
		break;
	case sbc_updatecomplete:
		dlg->m_progress.SetPos(100);
		break;
	case sbc_queryerror:
		str.Format("Query Error: %s\n", ServerBrowserListQueryError(dlg->m_serverBrowser));
		dlg->MessageBox(str);
		break;
	}
	
	GSI_UNUSED(serverBrowser);
}

void CSbmfcsampleDlg::SBConnectCallback(ServerBrowser serverBrowser, SBConnectToServerState state, SOCKET gamesocket, struct sockaddr_in *remoteaddr, void *instance)
{
	CSbmfcsampleDlg * dlg = (CSbmfcsampleDlg *)instance;

	switch(state)
	{
	case sbcs_succeeded:
		dlg->MessageBox("Connected to server");
		closesocket(gamesocket);
		break;
	case sbcs_failed:
		dlg->MessageBox("Failed to connect to server");
		break;
	}

	GSI_UNUSED(serverBrowser);
	GSI_UNUSED(remoteaddr);
}

BOOL CSbmfcsampleDlg::CreateServerList()
{
	if(!m_serverBrowser)
	{
		if(m_gamename.IsEmpty())
		{
			MessageBox("No game specified");
			GetDlgItem(IDC_GAMENAME)->SetFocus();
			return FALSE;
		}
		SBBool lanBrowsing = (SBBool)(IsDlgButtonChecked(IDC_LAN) == BST_CHECKED);
		if (m_startPort == 0 && lanBrowsing == SBTrue)
		{
			AfxMessageBox("Invalid start port!");
			GetDlgItem(IDC_STARTP)->SetFocus();
			return FALSE;
		}
		if (m_endPort == 0 && lanBrowsing == SBTrue)
		{
			AfxMessageBox("Invalid end port!");
			GetDlgItem(IDC_ENDP)->SetFocus();
			return FALSE;
		}
		
		m_serverBrowser = ServerBrowserNew(m_gamename, "gmtest", "HA6zkS", 0, MAX_UPDATES, (IsDlgButtonChecked(IDC_GOA) == BST_CHECKED)?QVERSION_GOA:QVERSION_QR2, lanBrowsing,	SBCallback, this);
		if(!m_serverBrowser)
		{
			MessageBox("Unable to create the server browser object");
			return FALSE;
		}

		GetDlgItem(IDC_GAMENAME)->EnableWindow(FALSE);
	}

	return TRUE;
}

void CSbmfcsampleDlg::AddServer(SBServer server, BOOL checkForReplace)
{
	if(ServerBrowserCount(m_serverBrowser) > 0)
		m_progress.SetPos((ServerBrowserCount(m_serverBrowser) - ServerBrowserPendingQueryCount(m_serverBrowser)) * 100 / ServerBrowserCount(m_serverBrowser));

	int index = FindServer(server);
	bool replace = (index != -1);

	if(!replace)
		index = m_serverList.GetItemCount();

	const char * hostname = SBServerGetStringValue(server, "hostname","(NO NAME)");
	if(replace)
	{
		m_serverList.SetItem(index, COL_SERVERNAME, LVIF_TEXT, hostname, -1, 0, 0, NULL);
	}
	else
	{
		m_serverList.InsertItem(index, hostname);
		m_serverList.SetItem(index, COL_SERVERNAME, LVIF_PARAM, NULL, -1, 0, 0, (LPARAM)server);
	}

	int numplayers = SBServerGetIntValue(server, "numplayers", 0);
	CString ping, players;
	if (SBServerHasValidPing(server))
		ping.Format("%d%s", SBServerGetPing(server), SBServerDirectConnect(server) ? "" : "i");
	else
		ping = "Unknown";
	players.Format("%d/%d", numplayers,  SBServerGetIntValue(server, "maxplayers", 0));
	m_serverList.SetItem(index, COL_PING, LVIF_TEXT, ping, -1, 0, 0, 0);
	m_serverList.SetItem(index, COL_PLAYERS, LVIF_TEXT, players, -1, 0, 0, 0);
	m_serverList.SetItem(index, COL_MAPNAME, LVIF_TEXT, SBServerGetStringValue(server, "mapname", "(NO MAP)"), -1, 0, 0, 0);
	m_serverList.SetItem(index, COL_GAMETYPE, LVIF_TEXT, SBServerGetStringValue(server, "gametype", ""), -1, 0, 0, 0);

	if(!replace)
	{
		CString str;
		str.Format("%d", ++m_serverCount);
		m_servers.SetWindowText(str);
	}

	GSI_UNUSED(checkForReplace);
}

void CSbmfcsampleDlg::RemoveServer(SBServer server)
{
	int index = FindServer(server);
	ASSERT(index != -1);
	if(index == -1)
		return;

	m_serverList.DeleteItem(index);

	CString str;
	str.Format("%d", --m_serverCount);
	m_servers.SetWindowText(str);
}

int CSbmfcsampleDlg::FindServer(SBServer server)
{
	LVFINDINFO info;
	info.flags = LVFI_PARAM;
	info.lParam = (LPARAM)server;
	return m_serverList.FindItem(&info);
}
