#if !defined(AFX_VOICESESSIONDLG_H__4D78C45C_9CB3_4F4B_81DF_8BF65316598E__INCLUDED_)
#define AFX_VOICESESSIONDLG_H__4D78C45C_9CB3_4F4B_81DF_8BF65316598E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "SetupDlg.h"
#include "../../gt2/gt2.h"
#include "../../natneg/natneg.h"

class CVoiceSessionDlg : public CDialog
{
public:
	int  m_NNCookie; 
	BOOL m_IsHost;   

	VoiceSetupInfo m_SetupInfo; 

	GT2Socket      m_Socket;
	GT2Connection  m_Connection;
	unsigned int   m_RemoteAddr;    


public:
	CVoiceSessionDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_SESSION };
	CStatic	m_RemoteSpeaking;
	CStatic	m_LocalSpeaking;
	CString	m_DisplayText;


	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	afx_msg void OnTimer(UINT nIDEvent);
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};


#endif 
