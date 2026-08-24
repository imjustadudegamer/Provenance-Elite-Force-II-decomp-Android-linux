#if !defined(AFX_WAITINGDLG_H__874B6CC6_8058_4BDF_B714_8FE3610A12B4__INCLUDED_)
#define AFX_WAITINGDLG_H__874B6CC6_8058_4BDF_B714_8FE3610A12B4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CWaitingDlg : public CDialog
{
public:
	CWaitingDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_WAITING };
		// NOTE: the ClassWizard will add data members here


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

		// NOTE: the ClassWizard will add member functions here
	DECLARE_MESSAGE_MAP()
};


#endif 
