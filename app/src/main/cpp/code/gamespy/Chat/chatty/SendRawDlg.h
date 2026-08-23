#if !defined(AFX_SENDRAWDLG_H__543BE7A8_4BE8_4ED0_972D_2339364077F6__INCLUDED_)
#define AFX_SENDRAWDLG_H__543BE7A8_4BE8_4ED0_972D_2339364077F6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CSendRawDlg : public CDialog
{
public:
	CSendRawDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_SEND_RAW };
	CString	m_raw;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	DECLARE_MESSAGE_MAP()
};


#endif 
