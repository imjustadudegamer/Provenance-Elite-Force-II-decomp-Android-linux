// chatty.cpp : Defines the class behaviors for the application.

#include "stdafx.h"
#include "chatty.h"

#include "MainFrm.h"
#include "ChildFrm.h"
#include "chattyDoc.h"
#include "chattyView.h"
#include "ConnectDlg.h"
#include "ChannelListDlg.h"
#include "SendRawDlg.h"
#include "GetUserInfoDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CChattyApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_CONNECT, OnFileConnect)
	ON_COMMAND(ID_FILE_DISCONNECT, OnFileDisconnect)
	ON_COMMAND(ID_FILE_LISTCHANNELS, OnFileListchannels)
	ON_COMMAND(ID_FILE_SENDRAW, OnFileSendraw)
	ON_COMMAND(ID_FILE_GETUSERINFO, OnFileGetuserinfo)
	ON_COMMAND(ID_FILE_SILENCE, OnFileSilence)
	ON_COMMAND(ID_FILE_UNSILENCE, OnFileUnsilence)
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()


CChattyApp::CChattyApp()
{
	m_chat = NULL;
	m_connected = false;
}


CChattyApp theApp;


BOOL CChattyApp::InitInstance()
{
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	SetRegistryKey(_T("GameSpy"));

	LoadStdProfileSettings(0);  


	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(
		IDR_CHATTYTYPE,
		RUNTIME_CLASS(CChattyDoc),
		RUNTIME_CLASS(CChildFrame), 
		RUNTIME_CLASS(CChattyView));
	AddDocTemplate(pDocTemplate);

	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;

	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	return TRUE;
}



class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

void CChattyApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


void Raw(CHAT chat, const char * raw, void * param)
{
	GSI_UNUSED(chat);
	GSI_UNUSED(raw);
	GSI_UNUSED(param);
}

void Disconnected(CHAT chat, const char * reason, void * param)
{
	OutputDebugString("Disconnected called\n");
	char buffer[256];

	sprintf(buffer, "You have been disconnected: %s", reason);
	theApp.m_pActiveWnd->MessageBox(buffer);

	theApp.m_chat = NULL;

	GSI_UNUSED(chat);
	GSI_UNUSED(param);
}

void PrivateMessage(CHAT chat, const char * user, const char * message, int type, void * param)
{
	OutputDebugString("PrivateMessage called\n");
#ifdef FEEDBACK
	CString str = user;
	str.MakeLower();

	if(strstr(str, ".org") || strstr(str, ".net") || strstr(str, ".com"))
		return;

	char buffer[256];

	sprintf(buffer, "%s: %s", user, message);
	if(type == CHAT_ACTION)
		strcat(buffer, "<action>");
	else if(type == CHAT_NOTICE)
		strcat(buffer, "<notice>");
	theApp.m_pActiveWnd->MessageBox(buffer);
#endif

	GSI_UNUSED(param);
	GSI_UNUSED(type);
	GSI_UNUSED(message);
	GSI_UNUSED(user);
	GSI_UNUSED(chat);
}

void Invited(CHAT chat, const char * channel, const char * user, void * param)
{
	OutputDebugString("Invited called\n");
#ifdef FEEDBACK
	char buffer[256];

	sprintf(buffer, "%s has invited you to join %s", user, channel);
	theApp.m_pActiveWnd->MessageBox(buffer);
#endif

	GSI_UNUSED(param);
	GSI_UNUSED(user);
	GSI_UNUSED(channel);
	GSI_UNUSED(chat);
}

void NickErrorCallback(CHAT chat, int type, const char * nick, int numSuggestedNicks, const char ** suggestedNicks, void * param)
{
	OutputDebugString("NickErrorCallback called\n");

	chatRetryWithNick(chat, "testnick");

	GSI_UNUSED(param);
	GSI_UNUSED(suggestedNicks);
	GSI_UNUSED(numSuggestedNicks);
	GSI_UNUSED(nick);
	GSI_UNUSED(type);
}

void ConnectCallback(CHAT chat, CHATBool success, int failureReason, void * param)
{
	OutputDebugString("ConnectCallback called\n");
	if(success)
	{
		theApp.m_pMainWnd->MessageBox("Connected!");
		theApp.m_connected = true;
	}
	else
	{
		theApp.m_pMainWnd->MessageBox("Connect Failed!");
		theApp.m_connected = false;
	}

	GSI_UNUSED(param);
	GSI_UNUSED(failureReason);
	GSI_UNUSED(chat);
}

void CChattyApp::OnFileConnect() 
{
	if(m_chat == NULL)
	{
		CConnectDlg dlg;
		dlg.m_nick = "Pants";
		dlg.m_address = "peerchat." GSI_DOMAIN_NAME;
		dlg.m_port = 6667;

		if(dlg.DoModal() == IDOK)
		{
			chatGlobalCallbacks callbacks;
			memset(&callbacks, 0, sizeof(chatGlobalCallbacks));
			callbacks.raw = Raw;
			callbacks.disconnected = Disconnected;
			callbacks.privateMessage = PrivateMessage;
			callbacks.invited = Invited;

			m_chat = chatConnect(dlg.m_address, dlg.m_port, dlg.m_nick, "pants", "pants", &callbacks, NickErrorCallback, ConnectCallback, this, CHATTrue);

			if(!m_chat)
				MessageBox(NULL, "chatConnect() failed", NULL, MB_OK);
		}
	}
}

void CChattyApp::OnFileDisconnect() 
{
	if(m_chat != NULL)
	{
		chatDisconnect(m_chat);

		m_chat = NULL;
	}
}

int CChattyApp::ExitInstance() 
{
	if(m_chat != NULL)
	{
		chatDisconnect(m_chat);

		m_chat = NULL;
	}
	
	return CWinApp::ExitInstance();
}

void CChattyApp::OnFileListchannels() 
{
	if(theApp.m_chat != NULL)
	{
		CChannelListDlg dlg;
		dlg.DoModal();
	}
}

void CChattyApp::OnFileSendraw() 
{
	if(theApp.m_chat != NULL)
	{
		CSendRawDlg dlg;
		if(dlg.DoModal() == IDOK)
		{
			chatSendRaw(theApp.m_chat, dlg.m_raw);
		}
	}
}

void CChattyApp::OnFileGetuserinfo() 
{
	if(theApp.m_chat != NULL)
	{
		CGetUserInfoDlg dlg;
		dlg.DoModal();
	}
}

void CChattyApp::OnFileSilence() 
{
	if(theApp.m_chat != NULL)
		chatSetQuietMode(theApp.m_chat, CHATTrue);
}

void CChattyApp::OnFileUnsilence() 
{
	if(theApp.m_chat != NULL)
		chatSetQuietMode(theApp.m_chat, CHATFalse);
}
