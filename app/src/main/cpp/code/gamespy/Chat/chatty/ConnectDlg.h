#if !defined(AFX_CONNECTDLG_H__999D9DD8_12B0_4F46_BF46_F607FB3341A0__INCLUDED_)
#define AFX_CONNECTDLG_H__999D9DD8_12B0_4F46_BF46_F607FB3341A0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CConnectDlg : public CDialog
{
public:
	CConnectDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_CONNECT };
	CString	m_address;
	CString	m_nick;
	int		m_port;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

		// NOTE: the ClassWizard will add member functions here
	DECLARE_MESSAGE_MAP()
};


#endif 
