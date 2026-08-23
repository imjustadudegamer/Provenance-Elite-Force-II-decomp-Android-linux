#if !defined(AFX_SETTOPICDLG_H__15C72001_C3D8_11D3_BD38_00C0F056BC39__INCLUDED_)
#define AFX_SETTOPICDLG_H__15C72001_C3D8_11D3_BD38_00C0F056BC39__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CSetTopicDlg : public CDialog
{
public:
	CSetTopicDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_SET_TOPIC };
	CString	m_topic;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	DECLARE_MESSAGE_MAP()
};


#endif 
