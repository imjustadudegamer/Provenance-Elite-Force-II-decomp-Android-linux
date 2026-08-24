/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_LOGINDLG_H__E77DE9BB_5AB3_4731_B26A_403169643353__INCLUDED_)
#define AFX_LOGINDLG_H__E77DE9BB_5AB3_4731_B26A_403169643353__INCLUDED_

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

	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};


#endif 
