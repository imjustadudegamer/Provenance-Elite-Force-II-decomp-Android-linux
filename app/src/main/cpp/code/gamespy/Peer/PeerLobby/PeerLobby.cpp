/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "PeerLobby.h"
#include "ConnectPage.h"
#include "TitlePage.h"
#include "GroupPage.h"
#include "CreatePage.h"
#include "StagingPage.h"
#include "LobbyWizard.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CPeerLobbyApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CPeerLobbyApp::CPeerLobbyApp()
{
}


CPeerLobbyApp theApp;


BOOL CPeerLobbyApp::InitInstance()
{
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	CLobbyWizard wizard("Peer Lobby");
	Wizard = &wizard;
	CConnectPage connectPage;
	ConnectPage = &connectPage;
	CTitlePage titlePage;
	TitlePage = &titlePage;
	CGroupPage groupPage;
	GroupPage = &groupPage;
	CCreatePage createPage;
	CreatePage = &createPage;
	CStagingPage stagingPage;
	StagingPage = &stagingPage;
	wizard.AddPage(ConnectPage);
	wizard.AddPage(TitlePage);
	wizard.AddPage(GroupPage);
	wizard.AddPage(CreatePage);
	wizard.AddPage(StagingPage);
	wizard.SetWizardMode();

	wizard.DoModal();

	if(wizard.m_peer)
		peerShutdown(wizard.m_peer);

	return FALSE;
}
