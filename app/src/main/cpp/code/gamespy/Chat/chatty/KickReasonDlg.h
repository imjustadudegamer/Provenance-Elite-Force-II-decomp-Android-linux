#if !defined(AFX_KICKREASONDLG_H__25C49432_E9A9_4BEF_8C47_31F8DF889DF0__INCLUDED_)
#define AFX_KICKREASONDLG_H__25C49432_E9A9_4BEF_8C47_31F8DF889DF0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CKickReasonDlg : public CDialog
{
public:
	CKickReasonDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_KICK_REASON };
	CString	m_reason;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	DECLARE_MESSAGE_MAP()
};


#endif 
