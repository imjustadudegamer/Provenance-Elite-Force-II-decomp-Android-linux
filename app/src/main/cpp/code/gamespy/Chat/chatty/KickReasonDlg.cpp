/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

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
END_MESSAGE_MAP()

