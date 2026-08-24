
#include "stdafx.h"
#include "chatty.h"
#include "EnterDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CEnterDlg::CEnterDlg(CWnd* pParent )
	: CDialog(CEnterDlg::IDD, pParent)
{
	m_channel = _T("");
	m_password = _T("");

	m_quickChannel = "";
}


void CEnterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_CHANNEL, m_channel);
	DDX_Text(pDX, IDC_PASSWORD, m_password);
}


BEGIN_MESSAGE_MAP(CEnterDlg, CDialog)
	ON_BN_CLICKED(IDC_QUICK1, OnQuick1)
	ON_BN_CLICKED(IDC_QUICK2, OnQuick2)
	ON_BN_CLICKED(IDC_QUICK3, OnQuick3)
	ON_BN_CLICKED(IDC_QUICK4, OnQuick4)
END_MESSAGE_MAP()


void CEnterDlg::OnQuick1() 
{
	m_quickChannel = "#istanbul";
	OnOK();
}

void CEnterDlg::OnQuick2() 
{
	m_quickChannel = "#montreal";
	OnOK();
}

void CEnterDlg::OnQuick3() 
{
	m_quickChannel = "#pants-test";
	OnOK();
}

void CEnterDlg::OnQuick4() 
{
	m_quickChannel = "#zurna";
	OnOK();
}
