
#include "stdafx.h"
#include "multiTrack.h"
#include "WaitingDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CWaitingDlg::CWaitingDlg(CWnd* pParent )
	: CDialog(CWaitingDlg::IDD, pParent)
{
		// NOTE: the ClassWizard will add member initialization here
}


void CWaitingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
		// NOTE: the ClassWizard will add DDX and DDV calls here
}


BEGIN_MESSAGE_MAP(CWaitingDlg, CDialog)
END_MESSAGE_MAP()


