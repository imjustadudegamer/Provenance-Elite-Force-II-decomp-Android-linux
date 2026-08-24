/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_WAITINGDLG_H__E9C283EE_68D1_40E9_83C6_AAB81D13ED25__INCLUDED_)
#define AFX_WAITINGDLG_H__E9C283EE_68D1_40E9_83C6_AAB81D13ED25__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CWaitingDlg : public CDialog
{
public:
	CWaitingDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_WAITING };


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	DECLARE_MESSAGE_MAP()
};


#endif 
