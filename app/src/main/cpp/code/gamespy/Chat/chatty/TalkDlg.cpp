
#include "stdafx.h"
#include "chatty.h"
#include "TalkDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CTalkDlg::CTalkDlg(CWnd* pParent )
	: CDialog(CTalkDlg::IDD, pParent)
{
	m_message = _T("");
	m_type = -1;
}


void CTalkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_MESSAGE, m_message);
	DDX_Radio(pDX, IDC_NORMAL, m_type);
}


BEGIN_MESSAGE_MAP(CTalkDlg, CDialog)
END_MESSAGE_MAP()

