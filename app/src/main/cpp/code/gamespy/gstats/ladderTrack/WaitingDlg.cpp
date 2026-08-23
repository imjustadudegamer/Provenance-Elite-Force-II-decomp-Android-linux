
#include "stdafx.h"
#include "ladderTrack.h"
#include "WaitingDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CWaitingDlg::CWaitingDlg(CWnd* pParent )
	: CDialog(CWaitingDlg::IDD, pParent)
{
}


void CWaitingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CWaitingDlg, CDialog)
END_MESSAGE_MAP()

