
#if !defined(AFX_CHATTY_H__D2C88C24_CA2E_4ED3_B08E_BF48F6C0A36B__INCLUDED_)
#define AFX_CHATTY_H__D2C88C24_CA2E_4ED3_B08E_BF48F6C0A36B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       


class CChattyApp : public CWinApp
{
public:
	CChattyApp();

	CHAT m_chat;
	bool m_connected;

	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

	afx_msg void OnAppAbout();
	afx_msg void OnFileConnect();
	afx_msg void OnFileDisconnect();
	afx_msg void OnFileListchannels();
	afx_msg void OnFileSendraw();
	afx_msg void OnFileGetuserinfo();
	afx_msg void OnFileSilence();
	afx_msg void OnFileUnsilence();
	DECLARE_MESSAGE_MAP()
};

extern CChattyApp theApp;



#endif 
