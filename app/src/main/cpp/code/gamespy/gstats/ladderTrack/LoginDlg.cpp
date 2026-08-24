
#include "stdafx.h"
#include "ladderTrack.h"
#include "LoginDlg.h"
#include "../../common/gsAvailable.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CLoginDlg::CLoginDlg(CWnd* pParent )
	: CDialog(CLoginDlg::IDD, pParent)
{
	m_email = _T("");
	m_nick = _T("");
	m_password = _T("");
}


void CLoginDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EMAIL, m_email);
	DDX_Text(pDX, IDC_NICK, m_nick);
	DDX_Text(pDX, IDC_PASSWORD, m_password);
}


BEGIN_MESSAGE_MAP(CLoginDlg, CDialog)
END_MESSAGE_MAP()


GPResult checkResult;
int checkProfile;

void CheckUserCallback(GPConnection * connection, void * _arg, void * param)
{
	GPCheckResponseArg * arg = (GPCheckResponseArg *)_arg;

	checkResult = arg->result;
	checkProfile = arg->profile;
}

void CLoginDlg::OnOK() 
{
	GPConnection connection;
	HCURSOR hourglass;
	HCURSOR lastCursor;
	GPResult result;

	UpdateData();

	if(m_email.IsEmpty() || m_nick.IsEmpty() || m_password.IsEmpty())
	{
		MessageBox("Please fill in all the account information.");
		return;
	}

	GSIACResult aResult = GSIACWaiting; 
	GSIStartAvailableCheck(gcd_gamename);
	while(aResult == GSIACWaiting)
	{
		aResult = GSIAvailableCheckThink();
		msleep(5);
	}

	if (aResult == GSIACUnavailable)
	{
		MessageBox("GameSpy backend services are not available.");
		return;
	}

	if(gpInitialize(&connection, 535, 0, GP_PARTNERID_GAMESPY) != GP_NO_ERROR)
	{
		MessageBox("Error initializing the login system.");
		return;
	}

	hourglass = LoadCursor(NULL, IDC_WAIT);
	if(hourglass)
		lastCursor = SetCursor(hourglass);

	result = gpCheckUser(&connection, m_nick, m_email, m_password, GP_BLOCKING, CheckUserCallback, NULL);

	if(hourglass)
		SetCursor(lastCursor);

	gpDestroy(&connection);

	if(result != GP_NO_ERROR)
	{
		MessageBox("Error verifying the account.");
		return;
	}

	if(checkResult != GP_NO_ERROR)
	{
		if(checkResult == GP_CHECK_BAD_EMAIL)
			MessageBox("Invalid e-mail.");
		else if(checkResult == GP_CHECK_BAD_NICK)
			MessageBox("Invalid nick.");
		else if(checkResult == GP_CHECK_BAD_PASSWORD)
			MessageBox("Invalid password.");
		else
			MessageBox("Error verifying the account.");
		return;
	}

	FILE * file;
	file = fopen("login.txt", "wt");
	if(file)
	{
		fprintf(file, "%s\n%s\n%s", m_email, m_nick, m_password);
		fclose(file);
	}

	m_profile = checkProfile;

	CDialog::OnOK();
}

BOOL CLoginDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	FILE * file;
	file = fopen("login.txt", "rt");
	if(file)
	{
		char buffer[512];

		if(fgets(buffer, sizeof(buffer), file))
			m_email = buffer;
		if(fgets(buffer, sizeof(buffer), file))
			m_nick = buffer;
		if(fgets(buffer, sizeof(buffer), file))
			m_password = buffer;

		fclose(file);

		m_email.Remove('\n');
		m_nick.Remove('\n');
		m_password.Remove('\n');
	}

	UpdateData(FALSE);

	return TRUE;
}