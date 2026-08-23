
#include "stdafx.h"
#include "chatty.h"
#include "ChannelModeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CChannelModeDlg::CChannelModeDlg(CWnd* pParent )
	: CDialog(CChannelModeDlg::IDD, pParent)
{
	m_inviteOnly = FALSE;
	m_limit = 0;
	m_moderated = FALSE;
	m_noExternalMessages = FALSE;
	m_onlyOpsChangeTopic = FALSE;
	m_private = FALSE;
	m_secret = FALSE;
}


void CChannelModeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_INVITE_ONLY, m_inviteOnly);
	DDX_Text(pDX, IDC_LIMIT, m_limit);
	DDV_MinMaxInt(pDX, m_limit, 0, 9999);
	DDX_Check(pDX, IDC_MODERATED, m_moderated);
	DDX_Check(pDX, IDC_NO_EXTERNAL_MESSAGES, m_noExternalMessages);
	DDX_Check(pDX, IDC_ONLY_OPS_CHANGE_TOPIC, m_onlyOpsChangeTopic);
	DDX_Check(pDX, IDC_PRIVATE, m_private);
	DDX_Check(pDX, IDC_SECRET, m_secret);
}


BEGIN_MESSAGE_MAP(CChannelModeDlg, CDialog)
END_MESSAGE_MAP()

