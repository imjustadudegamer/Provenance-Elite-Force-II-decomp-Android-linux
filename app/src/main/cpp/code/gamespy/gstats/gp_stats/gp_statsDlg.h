/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#if !defined(AFX_GP_STATSDLG_H__B7B25B2A_61E9_4BFE_BBD7_D7C4D7242B7D__INCLUDED_)
#define AFX_GP_STATSDLG_H__B7B25B2A_61E9_4BFE_BBD7_D7C4D7242B7D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "../../gp/gp.h"


class CGp_statsDlg : public CDialog
{
public:
	CGp_statsDlg(CWnd* pParent = NULL);	

	enum { IDD = IDD_GP_STATS_DIALOG };
	CListBox	m_keys;
	CString	m_email;
	CString	m_nick;
	CString	m_password;
	int		m_profileID;
	int		m_type;
	CString	m_value;
	CString	m_newKey;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	

public:
	void GotData(LPCSTR data);

protected:
	HICON m_hIcon;

	BOOL m_authenticated;
	GPConnection m_gp;

	void Authenticated();
	void UnAuthenticate();
	BOOL CheckStatsConnection();
	BOOL CheckAccount();
	BOOL CreateAccount();
	void SendPassword();
	BOOL StatsAuthentication();
	void ClearKeys();

	virtual BOOL OnInitDialog();
	afx_msg void OnAuthenticate();
	afx_msg void OnDestroy();
	afx_msg void OnGet();
	afx_msg void OnSet();
	afx_msg void OnAdd();
	afx_msg void OnSelchangeKeys();
	afx_msg void OnChangeValue();
	DECLARE_MESSAGE_MAP()
};


#endif 
