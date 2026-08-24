/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_GETUSERINFODLG_H__600E8857_A5B5_41DF_973E_0A3DCCF0DCEB__INCLUDED_)
#define AFX_GETUSERINFODLG_H__600E8857_A5B5_41DF_973E_0A3DCCF0DCEB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CGetUserInfoDlg : public CDialog
{
public:
	CGetUserInfoDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_GET_USER_INFO };
	CListBox	m_channels;
	CString	m_address;
	CString	m_name;
	CString	m_user;
	CString	m_nick;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	afx_msg void OnGetInfo();
	DECLARE_MESSAGE_MAP()
};


#endif 
