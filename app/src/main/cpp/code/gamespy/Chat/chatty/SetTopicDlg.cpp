
#include "stdafx.h"
#include "chatty.h"
#include "SetTopicDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CSetTopicDlg::CSetTopicDlg(CWnd* pParent )
	: CDialog(CSetTopicDlg::IDD, pParent)
{
	m_topic = _T("");
}


void CSetTopicDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_TOPIC, m_topic);
}


BEGIN_MESSAGE_MAP(CSetTopicDlg, CDialog)
		// NOTE: the ClassWizard will add message map macros here
END_MESSAGE_MAP()

