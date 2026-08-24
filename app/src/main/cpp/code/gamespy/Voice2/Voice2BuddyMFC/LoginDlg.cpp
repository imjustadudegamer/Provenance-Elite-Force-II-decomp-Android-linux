
#include "stdafx.h"
#include "Voice2BuddyMFC.h"
#include "LoginDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CLoginDlg::CLoginDlg(CWnd* pParent )
	: CDialog(CLoginDlg::IDD, pParent)
{
	m_Email = _T("");
	m_Nickname = _T("");
	m_Password = _T("");
}


void CLoginDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EMAIL, m_Email);
	DDX_Text(pDX, IDC_NICKNAME, m_Nickname);
	DDX_Text(pDX, IDC_PASSWORD, m_Password);
}


BEGIN_MESSAGE_MAP(CLoginDlg, CDialog)
END_MESSAGE_MAP()


void CLoginDlg::OnOK() 
{
	UpdateData(TRUE);

	HKEY aRegKey = NULL;
	LONG aResult = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\GameSpy\\Voice2BuddyMFC", 0, NULL, 
                                  REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &aRegKey, NULL);
	if (aResult == ERROR_SUCCESS)
	{
		RegSetValue(aRegKey, "LastLoginNickname", REG_SZ, m_Nickname, m_Nickname.GetLength());
		RegSetValue(aRegKey, "LastLoginEmail",    REG_SZ, m_Email,    m_Email.GetLength());
		RegCloseKey(aRegKey);
	}

	
	CDialog::OnOK();
}

void CLoginDlg::OnCancel() 
{
	
	CDialog::OnCancel();
}

BOOL CLoginDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	char aStringBuf[255];
	LONG aLongBuf;

	HKEY aRegKey = NULL;
	LONG aResult = 	RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\GameSpy\\Voice2BuddyMFC", 0, NULL, 
                                   REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &aRegKey, NULL);
	if (aResult == ERROR_SUCCESS)
	{
		aLongBuf = 255;
		aResult = RegQueryValue(aRegKey, "LastLoginNickname", aStringBuf, &aLongBuf);
		if (aResult == ERROR_SUCCESS)
			m_Nickname = aStringBuf;

		aLongBuf = 255;
		aResult = RegQueryValue(aRegKey, "LastLoginEmail", aStringBuf, &aLongBuf);
		if (aResult == ERROR_SUCCESS)
			m_Email = aStringBuf;

		RegCloseKey(aRegKey);
	}

	UpdateData(FALSE);

	return TRUE;  
}
