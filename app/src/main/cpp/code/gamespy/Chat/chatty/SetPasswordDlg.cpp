/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "chatty.h"
#include "SetPasswordDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CSetPasswordDlg::CSetPasswordDlg(CWnd* pParent )
	: CDialog(CSetPasswordDlg::IDD, pParent)
{
	m_password = _T("");
	m_enable = FALSE;
}


void CSetPasswordDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_PASSWORD, m_password);
	DDX_Check(pDX, IDC_ENABLE, m_enable);
}


BEGIN_MESSAGE_MAP(CSetPasswordDlg, CDialog)
END_MESSAGE_MAP()

