/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_LOBBYWIZARD_H__683E0CE1_CBA4_46D1_948A_6047481CDBF6__INCLUDED_)
#define AFX_LOBBYWIZARD_H__683E0CE1_CBA4_46D1_948A_6047481CDBF6__INCLUDED_

#include "../peer.h"

#if _MSC_VER > 1000
#pragma once
#endif 


#define CONNECT_PAGE   0
#define TITLE_PAGE     1
#define GROUP_PAGE     2
#define CREATE_PAGE    3
#define STAGING_PAGE   4

class CLobbyWizard : public CPropertySheet
{
	DECLARE_DYNAMIC(CLobbyWizard)

public:
	CLobbyWizard(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CLobbyWizard(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

public:
	PEER m_peer;        
	BOOL m_hosting;     
	BOOL m_groupRooms;  

	HWND m_nextButtonWnd;

	HICON m_greenSmileyIcon;
	int m_greenSmileyIndex;
	HICON m_yellowSmileyIcon;
	int m_yellowSmileyIndex;
	HICON m_redSmileyIcon;
	int m_redSmileyIndex;
	HICON m_stagingRoomIcon;
	int m_stagingRoomIndex;
	HICON m_runningGameIcon;
	int m_runningGameIndex;
	CImageList m_imageList;

	HCURSOR m_lastCursor;

public:
	void StartHourglass();
	void StopHourglass();

	public:
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
	virtual ~CLobbyWizard();

protected:
	afx_msg void OnTimer(UINT nIDEvent);
	DECLARE_MESSAGE_MAP()
};



extern CLobbyWizard * Wizard;

#endif 
