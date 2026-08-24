/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_ENTERDLG_H__56D121BA_9750_4FF9_AEA2_0CDAC1CB1DC2__INCLUDED_)
#define AFX_ENTERDLG_H__56D121BA_9750_4FF9_AEA2_0CDAC1CB1DC2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CEnterDlg : public CDialog
{
public:
	CEnterDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_ENTER };
	CString	m_channel;
	CString	m_password;

	CString m_quickChannel;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	afx_msg void OnQuick1();
	afx_msg void OnQuick2();
	afx_msg void OnQuick3();
	afx_msg void OnQuick4();
	DECLARE_MESSAGE_MAP()
};


#endif 
