/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "Voice2BuddyMFC.h"
#include "Voice2BuddyMFCDlg.h"

#include "LoginDlg.h"
#include "SetupDlg.h"
#include "VoiceSessionDlg.h"


#include "../../common/gsAvailable.h"
#include "../../voice2/gv.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


enum V2BuddyMessage
{
	V2B_MSG_DOSETUP,
	V2B_MSG_DOLOGIN,
	V2B_MSG_DOVOICECHAT,
};

#define THINK_TIMER_ID            100
#define THINK_TIMER_DELAY         10
#define GAME_NAME                 "gmtest"
#define NAMESPACE_GAMESPY_SHARED  0
#define PRODUCTID_GMTEST          1 

#define V2B_GP_LOCATION           "Voice2BuddyMFC"
#define V2B_GP_STATUS_IDLE        "Idle"      
#define V2B_GP_STATUS_CHATTING    "Chatting"  
#define V2B_GP_INVITE_DECLINED    "Voice invitation declined."
#define V2B_GP_INVITE_ACCEPTED    "Voice invitation accepted."

BOOL IsBuddyUsingV2B(GPConnection* theConnection, GPProfile theProfileId, GPBuddyStatus* theStatus);



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


CVoice2BuddyMFCDlg::CVoice2BuddyMFCDlg(CWnd* pParent )
	: CDialog(CVoice2BuddyMFCDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVoice2BuddyMFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, ID_SETUP, m_SetupButton);
	DDX_Control(pDX, ID_VOICECHAT, m_VoiceChatButton);
	DDX_Control(pDX, IDC_BUDDYLIST, m_BuddyList);
}

BEGIN_MESSAGE_MAP(CVoice2BuddyMFCDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_SETUP, OnSetup)
	ON_BN_CLICKED(ID_VOICECHAT, OnVoiceChat)
	ON_BN_CLICKED(IDCANCEL, OnExit)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_LBN_SELCHANGE(IDC_BUDDYLIST, OnSelchangeBuddylist)
END_MESSAGE_MAP()


BOOL CVoice2BuddyMFCDlg::OnInitDialog()
{
	CDialog::OnInitDialog();


	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);			
	SetIcon(m_hIcon, FALSE);		

	m_GP = NULL;
	m_Initialized   = FALSE;
	m_Connected     = FALSE;
	m_InvitedPlayer = 0;
	m_MyProfileId   = 0;

	SocketStartUp();

	GVBool aResult = gvStartup(m_hWnd);
	if (aResult != GVTrue)
	{
		MessageBox("Failed on gvStartup!");
		PostQuitMessage(0);
	}

	gvSetSampleRate(GVRate_16KHz);
	aResult = gvSetCodec(GVCodecSuperHighQuality);
	if (aResult != GVTrue)
	{
		MessageBox("Failed on gvSetCodec!");
		PostQuitMessage(0);
	}

	m_VoiceChatButton.EnableWindow(FALSE);

	memset(&m_SetupInfo, 0, sizeof(m_SetupInfo));

	PostMessage(WM_USER+1, V2B_MSG_DOSETUP, 0);
	
	return TRUE;  
}

void CVoice2BuddyMFCDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}


void CVoice2BuddyMFCDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); 

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

HCURSOR CVoice2BuddyMFCDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CVoice2BuddyMFCDlg::OnSetup() 
{
	CSetupDlg aSetupDlg;
	aSetupDlg.m_SetupInfo = &m_SetupInfo;
	aSetupDlg.DoModal();
}

void CVoice2BuddyMFCDlg::OnVoiceChat() 
{
	int selIndex = m_BuddyList.GetCurSel();
	if (selIndex == -1)
		return;
	
	GPProfile aProfile = m_BuddyList.GetItemData(selIndex);

	GPBuddyStatus aStatus;
	if (FALSE == IsBuddyUsingV2B(&m_GP, aProfile, &aStatus))
	{
		MessageBox("Buddy is not running V2B.");
		return;
	}
	
	if (strcmp(V2B_GP_STATUS_CHATTING, aStatus.locationString) == 0)
	{
		MessageBox("Buddy is currently involved in a voice chat.");
		return;
	}

	gpInvitePlayer(&m_GP, aProfile, PRODUCTID_GMTEST, NULL);
	m_InvitedPlayer = aProfile; 
}

void CVoice2BuddyMFCDlg::OnExit() 
{
	CDialog::OnCancel();
}

LRESULT CVoice2BuddyMFCDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if (message == (WM_USER+1))
	{
		switch(wParam)
		{
		case V2B_MSG_DOSETUP:
			{
				OnSetup();

				PostMessage(WM_USER+1, V2B_MSG_DOLOGIN, 0);

				SetTimer(THINK_TIMER_ID, THINK_TIMER_DELAY, NULL);
				return 0;
			}
		case V2B_MSG_DOLOGIN:
			{
				CLoginDlg aDlg;
				int result = aDlg.DoModal();
				if (result == IDCANCEL)
					PostQuitMessage(0);
				else
					DoLogin(aDlg.m_Email, aDlg.m_Nickname, aDlg.m_Password);
				return 0;
			}
		case V2B_MSG_DOVOICECHAT:
			{
				CVoiceSessionDlg aVoiceSessionDlg;
				aVoiceSessionDlg.m_NNCookie  = lParam;
				aVoiceSessionDlg.m_SetupInfo = m_SetupInfo;
				
				if (m_MyProfileId == aVoiceSessionDlg.m_NNCookie)
					aVoiceSessionDlg.m_IsHost = TRUE;
				else
					aVoiceSessionDlg.m_IsHost = FALSE;
				
				aVoiceSessionDlg.DoModal();
				return 0;
			}
		};
	} 
	return CDialog::WindowProc(message, wParam, lParam);
}


BOOL IsBuddyUsingV2B(GPConnection* theConnection, GPProfile theProfileId, GPBuddyStatus* theStatus)
{
	GPBuddyStatus   aTempStatus;
	GPBuddyStatus*  aStatus;
	if (theStatus != NULL)
		aStatus = theStatus;	
	else
		aStatus = &aTempStatus; 

	int aBuddyIndex;
	GPResult aResult = gpGetBuddyIndex(theConnection, theProfileId, &aBuddyIndex);
	if (aResult != GP_NO_ERROR)
		return FALSE; 
	
	aResult = gpGetBuddyStatus(theConnection, aBuddyIndex, aStatus);
	if (aResult != GP_NO_ERROR)
		return FALSE; 

	if (strcmp(V2B_GP_LOCATION, aStatus->locationString) != 0)
		return FALSE; 

	return TRUE;
}


static void OnGPError(GPConnection* theConnection, GPErrorArg* theArg, void* theParam)
{
	CVoice2BuddyMFCDlg* aDialog = (CVoice2BuddyMFCDlg*)theParam;

	char buf[1024];
	sprintf(buf, "GP ERROR: %s", theArg->errorString);
	aDialog->MessageBox(buf);

	if (theArg->fatal == GP_FATAL)
		aDialog->m_Connected = FALSE;
	GSI_UNUSED(theConnection);
}


static void OnGPBuddyInfo(GPConnection* theConnection, GPGetInfoResponseArg* theArg, void* theParam)
{
	CVoice2BuddyMFCDlg* aDialog = (CVoice2BuddyMFCDlg*)theParam;

	int anIndex = aDialog->m_BuddyList.FindString(0, theArg->nick);
	if (anIndex == -1)
	{
		int anIndex = aDialog->m_BuddyList.AddString(theArg->nick);
		if (anIndex != -1)
			aDialog->m_BuddyList.SetItemData(anIndex, theArg->profile);
	}
	
	GSI_UNUSED(theConnection);
}

static void OnGPBuddyStatus(GPConnection* theConnection, GPRecvBuddyStatusArg* theArg, void* theParam)
{
	CVoice2BuddyMFCDlg* aDialog = (CVoice2BuddyMFCDlg*)theParam;
	
	gpGetInfo(theConnection, theArg->profile, GP_CHECK_CACHE, GP_NON_BLOCKING, (GPCallback)OnGPBuddyInfo, theParam);
	GSI_UNUSED(aDialog);
}


static void OnGPBuddyMessage(GPConnection* theConnection, GPRecvBuddyMessageArg* theArg, void* theParam)
{
	CVoice2BuddyMFCDlg* aDialog = (CVoice2BuddyMFCDlg*)theParam;

	if (FALSE == IsBuddyUsingV2B(theConnection, theArg->profile, NULL))
		return;

	if (theArg->profile != aDialog->m_InvitedPlayer)
		return;

	if (strcmp(V2B_GP_INVITE_ACCEPTED, theArg->message)==0)
		aDialog->PostMessage(WM_USER+1, V2B_MSG_DOVOICECHAT, aDialog->m_MyProfileId);
		
	if (strcmp(V2B_GP_INVITE_DECLINED, theArg->message)==0)
		aDialog->MessageBox(theArg->message);
}


static void OnGPInvite(GPConnection* theConnection, GPRecvGameInviteArg* theArg, void* theParam)
{
	CVoice2BuddyMFCDlg* aDialog = (CVoice2BuddyMFCDlg*)theParam;

	if (FALSE == IsBuddyUsingV2B(theConnection, theArg->profile, NULL))
		return;

	CString aBuddyName = "";
	int anIndex = 0;
	while(anIndex < aDialog->m_BuddyList.GetCount())
	{
		if (aDialog->m_BuddyList.GetItemData(anIndex) == (unsigned int)theArg->profile)
		{
			aDialog->m_BuddyList.GetText(anIndex, aBuddyName);
			break;
		}
		anIndex++;
	}
	if (aBuddyName.IsEmpty())
		return; 

	char buf[255];
	sprintf(buf, "%s has invited you to voice chat.  Accept?", (LPCSTR)aBuddyName);
	int aDialogResult = aDialog->MessageBox(buf, "Voice Chat", MB_YESNO);
	if (aDialogResult == IDYES)
	{
		gpSendBuddyMessage(&aDialog->m_GP, theArg->profile, V2B_GP_INVITE_ACCEPTED);
		aDialog->PostMessage(WM_USER+1, V2B_MSG_DOVOICECHAT, theArg->profile);
	}
	else
	{
		gpSendBuddyMessage(&aDialog->m_GP, theArg->profile, V2B_GP_INVITE_DECLINED);
	}
}


static void OnGPConnect(GPConnection* theConnection, GPConnectResponseArg* theArg, void* theParam)
{
	CVoice2BuddyMFCDlg* aDialog = (CVoice2BuddyMFCDlg*)theParam;

	if (theArg->result == GP_NO_ERROR)
	{
		aDialog->m_Connected = TRUE;
		aDialog->m_SetupButton.EnableWindow(TRUE);
		aDialog->m_MyProfileId = theArg->profile;
	}
	else
	{
		aDialog->MessageBox("Failed to connect!");
		PostQuitMessage(0);
	}
	GSI_UNUSED(theConnection);
}


void CVoice2BuddyMFCDlg::DoLogin(const CString& theEmail, const CString& theNickname, const CString& thePassword)
{
	GSIACResult aResult = GSIACWaiting;
	GSIStartAvailableCheck(GAME_NAME);
	while(aResult == GSIACWaiting)
	{
		aResult = GSIAvailableCheckThink();
		Sleep(20);
	}

	GPResult result = gpInitialize(&m_GP, PRODUCTID_GMTEST, NAMESPACE_GAMESPY_SHARED, GP_PARTNERID_GAMESPY);
	if (result != GP_NO_ERROR)
	{
		MessageBox("Failed on gpInitialize\r\n");
		PostQuitMessage(0);
	}

	gpSetCallback(&m_GP, GP_ERROR,              (GPCallback)OnGPError,       this);
	gpSetCallback(&m_GP, GP_RECV_BUDDY_STATUS,  (GPCallback)OnGPBuddyStatus, this);
	gpSetCallback(&m_GP, GP_RECV_GAME_INVITE,	(GPCallback)OnGPInvite,      this);
	gpSetCallback(&m_GP, GP_RECV_BUDDY_MESSAGE, (GPCallback)OnGPBuddyMessage,this);
	m_Initialized = TRUE;
	
	gpConnect(&m_GP, theNickname, theEmail, thePassword, GP_NO_FIREWALL, GP_BLOCKING, (GPCallback)OnGPConnect, this);

	if (m_Connected)
		SetTimer(THINK_TIMER_ID, THINK_TIMER_DELAY, NULL);

	gpSetStatus(&m_GP, GP_ONLINE, V2B_GP_STATUS_IDLE, V2B_GP_LOCATION);
}

void CVoice2BuddyMFCDlg::OnTimer(UINT nIDEvent) 
{
	static bool inTimer = false;
	if (inTimer)
		return;
	
	inTimer = true;

	if (m_Connected)
		gpProcess(&m_GP);

	inTimer = false;

	CDialog::OnTimer(nIDEvent);
}

void CVoice2BuddyMFCDlg::OnDestroy() 
{
	CDialog::OnDestroy();
	
	KillTimer(THINK_TIMER_ID);

	if (m_Connected)
		gpDisconnect(&m_GP);

	if (m_Initialized)
		gpDestroy(&m_GP);

	gvCleanup();
}

void CVoice2BuddyMFCDlg::OnSelchangeBuddylist() 
{
	int aSel = m_BuddyList.GetCurSel();
	if (aSel == -1)
		m_VoiceChatButton.EnableWindow(FALSE);
	else
		m_VoiceChatButton.EnableWindow(TRUE);
}
