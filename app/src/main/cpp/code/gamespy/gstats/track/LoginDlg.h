#if !defined(AFX_LOGINDLG_H__6C88CC46_47E9_420F_BC33_BA7F28711BAD__INCLUDED_)
#define AFX_LOGINDLG_H__6C88CC46_47E9_420F_BC33_BA7F28711BAD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CLoginDlg : public CDialog
{
public:
	int m_profile;
	CLoginDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_LOGIN };
	CString	m_email;
	CString	m_nick;
	CString	m_password;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	virtual void OnOK();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};


#endif 
