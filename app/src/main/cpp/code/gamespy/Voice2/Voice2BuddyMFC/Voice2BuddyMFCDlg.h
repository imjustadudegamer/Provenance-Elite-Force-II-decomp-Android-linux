/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#if !defined(AFX_VOICE2BUDDYMFCDLG_H__F4BA3DE6_2C65_4B7D_A1BC_A9C3BAF53DEE__INCLUDED_)
#define AFX_VOICE2BUDDYMFCDLG_H__F4BA3DE6_2C65_4B7D_A1BC_A9C3BAF53DEE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "../../gp/gp.h"
#include "SetupDlg.h"

class CVoice2BuddyMFCDlg : public CDialog
{
public:
	GPConnection  m_GP;
	BOOL          m_Initialized;
	BOOL          m_Connected;
	unsigned int  m_NNCookie;
	GPProfile     m_InvitedPlayer; 
	GPProfile     m_MyProfileId;   

	VoiceSetupInfo   m_SetupInfo;

	void DoLogin(const CString& theEmail, const CString& theNickname, const CString& thePassword);

	
public:
	CVoice2BuddyMFCDlg(CWnd* pParent = NULL);	

	enum { IDD = IDD_VOICE2BUDDYMFC_DIALOG };
	CButton	m_SetupButton;
	CButton	m_VoiceChatButton;
	CListBox	m_BuddyList;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSetup();
	afx_msg void OnVoiceChat();
	afx_msg void OnExit();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnSelchangeBuddylist();
	DECLARE_MESSAGE_MAP()
};


#endif 
