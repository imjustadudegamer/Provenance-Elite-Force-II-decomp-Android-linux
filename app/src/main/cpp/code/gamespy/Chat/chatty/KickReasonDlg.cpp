
#include "stdafx.h"
#include "chatty.h"
#include "KickReasonDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CKickReasonDlg::CKickReasonDlg(CWnd* pParent )
	: CDialog(CKickReasonDlg::IDD, pParent)
{
	m_reason = _T("");
}


void CKickReasonDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_REASON, m_reason);
}


BEGIN_MESSAGE_MAP(CKickReasonDlg, CDialog)
		// NOTE: the ClassWizard will add message map macros here
END_MESSAGE_MAP()

