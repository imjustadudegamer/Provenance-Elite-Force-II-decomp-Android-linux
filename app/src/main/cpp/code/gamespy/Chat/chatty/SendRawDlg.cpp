
#include "stdafx.h"
#include "chatty.h"
#include "SendRawDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CSendRawDlg::CSendRawDlg(CWnd* pParent )
	: CDialog(CSendRawDlg::IDD, pParent)
{
	m_raw = _T("");
}


void CSendRawDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_RAW, m_raw);
}


BEGIN_MESSAGE_MAP(CSendRawDlg, CDialog)
		// NOTE: the ClassWizard will add message map macros here
END_MESSAGE_MAP()

