
#include "stdafx.h"
#include "PeerLobby.h"
#include "LobbyWizard.h"
#include "GroupPage.h"
#include "StagingPage.h"
#include "TitlePage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CLobbyWizard * Wizard;

#define THINK_TIMER  100


IMPLEMENT_DYNAMIC(CLobbyWizard, CPropertySheet)

CLobbyWizard::CLobbyWizard(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	m_peer = NULL;
}

CLobbyWizard::CLobbyWizard(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	m_peer = NULL;
}

CLobbyWizard::~CLobbyWizard()
{
}


BEGIN_MESSAGE_MAP(CLobbyWizard, CPropertySheet)
	ON_WM_TIMER()
END_MESSAGE_MAP()


BOOL CLobbyWizard::OnInitDialog() 
{
	BOOL bResult = CPropertySheet::OnInitDialog();

	m_nextButtonWnd = ::FindWindowEx(this->m_hWnd, NULL, "Button", "&Next >");

	HWND hWnd = ::FindWindowEx(this->m_hWnd, NULL, "Button", "Cancel");
	if(hWnd)
		::SetWindowText(hWnd, "&Quit");

	hWnd = ::FindWindowEx(this->m_hWnd, NULL, "Button", "Help");
	if(hWnd)
		::ShowWindow(hWnd, SW_HIDE);

	m_greenSmileyIcon = AfxGetApp()->LoadIcon(IDI_GREEN_SMILEY);
	m_yellowSmileyIcon = AfxGetApp()->LoadIcon(IDI_YELLOW_SMILEY);
	m_redSmileyIcon = AfxGetApp()->LoadIcon(IDI_RED_SMILEY);
	m_stagingRoomIcon = AfxGetApp()->LoadIcon(IDI_STAGING_ROOM);
	m_runningGameIcon = AfxGetApp()->LoadIcon(IDI_RUNNING_GAME);
	if(!m_greenSmileyIcon || !m_yellowSmileyIcon || !m_redSmileyIcon || !m_stagingRoomIcon || !m_runningGameIcon)
		return FALSE;

	if(!m_imageList.Create(16, 16, ILC_COLOR, 5, 5))
		return FALSE;
	m_greenSmileyIndex = m_imageList.Add(m_greenSmileyIcon);
	m_yellowSmileyIndex = m_imageList.Add(m_yellowSmileyIcon);
	m_redSmileyIndex = m_imageList.Add(m_redSmileyIcon);
	m_stagingRoomIndex = m_imageList.Add(m_stagingRoomIcon);
	m_runningGameIndex = m_imageList.Add(m_runningGameIcon);
	if((m_yellowSmileyIndex == -1) || (m_redSmileyIndex == -1) || (m_greenSmileyIndex == -1) || (m_runningGameIndex == -1) || (m_stagingRoomIndex == -1))
		return FALSE;

	m_hosting = FALSE;

	SetTimer(THINK_TIMER, 10, NULL);

	return bResult;
}

void CLobbyWizard::OnTimer(UINT nIDEvent) 
{
	if(nIDEvent == THINK_TIMER)
	{
		if(m_peer)
			peerThink(m_peer);
	}

	CPropertySheet::OnTimer(nIDEvent);
}

void CLobbyWizard::StartHourglass()
{
	HCURSOR hourglass = LoadCursor(NULL, IDC_WAIT);
	if(!hourglass)
		return;

	m_lastCursor = SetCursor(hourglass);
}

void CLobbyWizard::StopHourglass()
{
	SetCursor(m_lastCursor);
}

BOOL CLobbyWizard::PreTranslateMessage(MSG* pMsg) 
{
	if((pMsg->message == WM_KEYDOWN) && (pMsg->wParam == 0x0d))
	{
		int page = GetActiveIndex();
		CWnd * focus;
		if(page == TITLE_PAGE)
		{
			focus = TitlePage->GetFocus();
			if(focus->m_hWnd == TitlePage->GetDlgItem(IDC_MESSAGE)->m_hWnd)
			{
				TitlePage->SendMessage();
				return TRUE;
			}
		}
		else if(page == GROUP_PAGE)
		{
			focus = GroupPage->GetFocus();
			if(focus->m_hWnd == GroupPage->GetDlgItem(IDC_MESSAGE)->m_hWnd)
			{
				GroupPage->SendMessage();
				return TRUE;
			}
		}
		else if(page == STAGING_PAGE)
		{
			focus = StagingPage->GetFocus();
			if(focus->m_hWnd == StagingPage->GetDlgItem(IDC_MESSAGE)->m_hWnd)
			{
				StagingPage->SendMessage();
				return TRUE;
			}
		}
	}

	return CPropertySheet::PreTranslateMessage(pMsg);
}
