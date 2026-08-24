
#include "stdafx.h"
#include "multiTrack.h"
#include "HostOrJoinDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CHostOrJoinDlg::CHostOrJoinDlg(CWnd* pParent )
	: CDialog(CHostOrJoinDlg::IDD, pParent)
{
	m_hostOrJoin = 0;
	m_joinAddress = _T("");
}


void CHostOrJoinDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_HOST, m_hostOrJoin);
	DDX_Text(pDX, IDC_JOIN_ADDRESS, m_joinAddress);
}


BEGIN_MESSAGE_MAP(CHostOrJoinDlg, CDialog)
	ON_EN_SETFOCUS(IDC_JOIN_ADDRESS, OnSetfocusJoinAddress)
END_MESSAGE_MAP()



void CHostOrJoinDlg::OnOK() 
{
	UpdateData();

	if((m_hostOrJoin == HOSTORJOIN_JOIN) && m_joinAddress.IsEmpty())
	{
		MessageBox("Please enter the address of the host to connect to");
		return;
	}

	CDialog::OnOK();
}

void CHostOrJoinDlg::OnSetfocusJoinAddress() 
{
	UpdateData();
	m_hostOrJoin = HOSTORJOIN_JOIN;
	UpdateData(FALSE);
}
