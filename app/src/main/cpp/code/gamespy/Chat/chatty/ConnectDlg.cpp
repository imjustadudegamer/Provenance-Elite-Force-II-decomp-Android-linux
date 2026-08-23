
#include "stdafx.h"
#include "chatty.h"
#include "ConnectDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CConnectDlg::CConnectDlg(CWnd* pParent )
	: CDialog(CConnectDlg::IDD, pParent)
{
	m_address = _T("");
	m_nick = _T("");
	m_port = 0;
}


void CConnectDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_ADDRESS, m_address);
	DDX_Text(pDX, IDC_NICK, m_nick);
	DDX_Text(pDX, IDC_PORT, m_port);
}


BEGIN_MESSAGE_MAP(CConnectDlg, CDialog)
END_MESSAGE_MAP()

