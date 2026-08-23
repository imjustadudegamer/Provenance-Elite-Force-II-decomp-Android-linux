#if !defined(AFX_SETPASSWORDDLG_H__666C66DA_E10E_420D_954E_FB2BCE01D798__INCLUDED_)
#define AFX_SETPASSWORDDLG_H__666C66DA_E10E_420D_954E_FB2BCE01D798__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CSetPasswordDlg : public CDialog
{
public:
	CSetPasswordDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_SET_PASSWORD };
	CString	m_password;
	BOOL	m_enable;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	DECLARE_MESSAGE_MAP()
};


#endif 
