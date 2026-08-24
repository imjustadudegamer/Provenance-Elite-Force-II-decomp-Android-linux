#if !defined(AFX_HOSTORJOINDLG_H__7D7ACBE1_D82A_4CB0_B070_368D4263332E__INCLUDED_)
#define AFX_HOSTORJOINDLG_H__7D7ACBE1_D82A_4CB0_B070_368D4263332E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


#define HOSTORJOIN_HOST       0
#define HOSTORJOIN_JOIN       1

class CHostOrJoinDlg : public CDialog
{
public:
	CHostOrJoinDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_HOST_OR_JOIN };
	int		m_hostOrJoin;
	CString	m_joinAddress;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	virtual void OnOK();
	afx_msg void OnSetfocusJoinAddress();
	DECLARE_MESSAGE_MAP()
};


#endif 
