#if !defined(AFX_LOGINDLG_H__0E2CE143_57CF_4FE7_9DFB_9C63388C0680__INCLUDED_)
#define AFX_LOGINDLG_H__0E2CE143_57CF_4FE7_9DFB_9C63388C0680__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CLoginDlg : public CDialog
{
public:
	CLoginDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_LOGIN };
	CString	m_Email;
	CString	m_Nickname;
	CString	m_Password;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};


#endif 
