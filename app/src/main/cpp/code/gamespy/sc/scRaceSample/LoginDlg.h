/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_LOGINDLG_H__58A8C6B5_2AD3_4C62_882E_DC1F8D104CC5__INCLUDED_)
#define AFX_LOGINDLG_H__58A8C6B5_2AD3_4C62_882E_DC1F8D104CC5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 



class CLoginDlg : public CDialog
{
public:
	CLoginDlg(CWnd* pParent = NULL);   
	SamplePlayerData m_playerData; 


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
