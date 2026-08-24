#if !defined(AFX_TALKDLG_H__87449421_C42C_11D3_BD38_00C0F056BC39__INCLUDED_)
#define AFX_TALKDLG_H__87449421_C42C_11D3_BD38_00C0F056BC39__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CTalkDlg : public CDialog
{
public:
	CTalkDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_TALK };
	CString	m_message;
	int		m_type;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

		// NOTE: the ClassWizard will add member functions here
	DECLARE_MESSAGE_MAP()
};


#endif 
