
#include "stdafx.h"
#include "gp_stats.h"
#include "gp_statsDlg.h"
#include "../gpersist.h"
#include "../../ghttp/ghttp.h"
#include "../../common/gsAvailable.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CWnd * ToggleControls[64];


CGp_statsDlg::CGp_statsDlg(CWnd* pParent )
	: CDialog(CGp_statsDlg::IDD, pParent)
{
	m_email = _T("dan@gamespy.com");
	m_nick = _T("mrpants");
	m_password = _T("mrpants");
	m_profileID = 0;
	m_type = 0;
	m_value = _T("");
	m_newKey = _T("");
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CGp_statsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_KEYS, m_keys);
	DDX_Text(pDX, IDC_EMAIL, m_email);
	DDX_Text(pDX, IDC_NICK, m_nick);
	DDX_Text(pDX, IDC_PASSWORD, m_password);
	DDX_Text(pDX, IDC_PROFILE_ID, m_profileID);
	DDX_Radio(pDX, IDC_PRIVATE_RW, m_type);
	DDX_Text(pDX, IDC_VALUE, m_value);
	DDX_Text(pDX, IDC_NEW_KEY, m_newKey);
}

BEGIN_MESSAGE_MAP(CGp_statsDlg, CDialog)
	ON_BN_CLICKED(IDC_AUTHENTICATE, OnAuthenticate)
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_GET, OnGet)
	ON_BN_CLICKED(IDC_SET, OnSet)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_LBN_SELCHANGE(IDC_KEYS, OnSelchangeKeys)
	ON_EN_CHANGE(IDC_VALUE, OnChangeValue)
END_MESSAGE_MAP()


BOOL CGp_statsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			
	SetIcon(m_hIcon, FALSE);		

	int num = 0;
#define ADD_TOGGLE(id)    ToggleControls[num++] = this->GetDlgItem(id)
	ADD_TOGGLE(IDC_PRIVATE_RW);
	ADD_TOGGLE(IDC_PRIVATE_RO);
	ADD_TOGGLE(IDC_PUBLIC_RW);
	ADD_TOGGLE(IDC_PUBLIC_RO);
	ADD_TOGGLE(IDC_GET);
	ADD_TOGGLE(IDC_SET);
	ADD_TOGGLE(IDC_VALUE);
	ADD_TOGGLE(IDC_KEYS);
	ADD_TOGGLE(IDC_ADD);
	ADD_TOGGLE(IDC_NEW_KEY);
	ToggleControls[num++] = NULL;

	m_authenticated = FALSE;
	m_gp = NULL;

	UnAuthenticate();

	CheckStatsConnection();

	if(gpInitialize(&m_gp, 0, 0, GP_PARTNERID_GAMESPY) != GP_NO_ERROR)
		MessageBox("FAILED TO INITIALIZE GP!!!");

	return TRUE;  
}

void CGp_statsDlg::OnDestroy() 
{
	CDialog::OnDestroy();

	ClearKeys();

	CloseStatsConnection();

	gpDestroy(&m_gp);

	ghttpCleanup();
}

void CGp_statsDlg::Authenticated()
{
	m_authenticated = TRUE;

	int i;
	for(i = 0 ; ToggleControls[i] ; i++)
		ToggleControls[i]->EnableWindow();
}

void CGp_statsDlg::UnAuthenticate()
{
	m_authenticated = FALSE;
	m_profileID = 0;

	int i;
	for(i = 0 ; ToggleControls[i] ; i++)
		ToggleControls[i]->EnableWindow(FALSE);
}

BOOL CGp_statsDlg::CheckStatsConnection()
{
	if(IsStatsConnected())
		return TRUE;

#if 1
	strcpy(gcd_gamename, "gmtest");
	gcd_secret_key[0] = 'H';
	gcd_secret_key[1] = 'A';
	gcd_secret_key[2] = '6';
	gcd_secret_key[3] = 'z';
	gcd_secret_key[4] = 'k';
	gcd_secret_key[5] = 'S';
	gcd_secret_key[7] = '\0';
#else
	strcpy(gcd_gamename, "excessive");
	gcd_secret_key[0] = 'G';
	gcd_secret_key[1] = 'n';
	gcd_secret_key[2] = '3';
	gcd_secret_key[3] = 'a';
	gcd_secret_key[4] = 'Y';
	gcd_secret_key[5] = '9';
	gcd_secret_key[7] = '\0';
#endif
	
	GSIACResult ac_result;
	GSIStartAvailableCheck(gcd_gamename);
	while((ac_result = GSIAvailableCheckThink()) == GSIACWaiting)
		msleep(5);
	if(ac_result != GSIACAvailable)
	{
		MessageBox("The backend is not available\n");
		return TRUE;
	}

	int result = InitStatsConnection(0);
	if(result == GE_NOERROR)
		return TRUE;

	CString message;
	if(result == GE_NOSOCKET)
		message = "Unable to create a socket.";
	else if(result == GE_NODNS)
		message = "Unable to resolve a DNS name.";
	else if(result == GE_NOCONNECT)
		message = "Unable to connect to stats server, or connection lost.";
	else if(result == GE_DATAERROR)
		message = "Bad data from the stats server.";
	else
		message = "Error.";
	MessageBox(message, "Error connecting to the stats server");

	return FALSE;
}

void CGp_statsDlg::SendPassword()
{
	CString URL = "http://gamespyarcade.com/software/reqemail.asp?email=";
	URL += m_email;

	HCURSOR hPrevCursor = GetCursor();
	SetCursor(LoadCursor(NULL, IDC_WAIT));

	ghttpGetFile(URL, GHTTPTrue, NULL, NULL);

	SetCursor(hPrevCursor);
}

void CreateAccountCallback(GPConnection * connection, void * arg_, void * param)
{
	GPNewUserResponseArg * arg = (GPNewUserResponseArg *)arg_;
	int * pid = (int *)param;

	if(arg->result == GP_NO_ERROR)
		*pid = arg->profile;
	else
	{
		GPErrorCode errorCode;
		gpGetErrorCode(connection, &errorCode);
		if(errorCode == GP_NEWUSER_BAD_NICK)
			*pid = arg->profile;
		else
			*pid = 0;
	}
}

BOOL CGp_statsDlg::CreateAccount()
{
	int pid;
	int rcode;

	GPResult result = gpNewUser(
		&m_gp,
		m_nick,
		NULL,
		m_email,
		m_password,
		NULL,
		GP_BLOCKING,
		CreateAccountCallback,
		&pid);
	if(result != GP_NO_ERROR)
	{
		MessageBox("There was an error creating the account.");
		return FALSE;
	}

	if(pid)
	{
		m_profileID = pid;
		return TRUE;
	}

	GPErrorCode errorCode;
	gpGetErrorCode(&m_gp, &errorCode);

	if(errorCode == GP_NEWUSER_BAD_PASSWORD)
	{
		rcode = MessageBox(
			"You have entered an incorrect password for this e-mail address\n"
			"Would you like the password sent to the e-mail address?",
			NULL,
			MB_YESNO);

		if(rcode == IDNO)
			return FALSE;

		SendPassword();

		return FALSE;
	}

	MessageBox("There was an error creating the account.");
	return FALSE;
}

void CheckAccountCallback(GPConnection * connection, void * arg_, void * param)
{
	GPCheckResponseArg * arg = (GPCheckResponseArg *)arg_;
	int * pid = (int *)param;

	if(arg->result == GP_NO_ERROR)
		*pid = arg->profile;
	else
		*pid = 0;
}

BOOL CGp_statsDlg::CheckAccount()
{
	int pid;
	int rcode;

	GPResult result = gpCheckUser(
		&m_gp,
		m_nick,
		m_email,
		m_password,
		GP_BLOCKING,
		CheckAccountCallback,
		&pid);
	if(result != GP_NO_ERROR)
	{
		MessageBox("There was an error authenticating the account.");
		return FALSE;
	}

	if(pid)
	{
		m_profileID = pid;
		return TRUE;
	}

	GPErrorCode errorCode;
	gpGetErrorCode(&m_gp, &errorCode);

	if(errorCode == GP_CHECK_BAD_EMAIL)
	{
		rcode = MessageBox(
			"This account does not exist.\n"
			"Would you like to create it?",
			NULL,
			MB_YESNO);

		if(rcode == IDNO)
			return FALSE;

		return CreateAccount();
	}
	else if(errorCode == GP_CHECK_BAD_NICK)
	{
		rcode = MessageBox(
			"There are no profiles under this account with the nick you have entered\n"
			"Would you like to create one?",
			NULL,
			MB_YESNO);

		if(rcode == IDNO)
			return FALSE;

		return CreateAccount();
	}
	else if(errorCode == GP_CHECK_BAD_PASSWORD)
	{
		rcode = MessageBox(
			"You have entered an incorrect password for this e-mail address\n"
			"Would you like the password sent to the e-mail address?",
			NULL,
			MB_YESNO);

		if(rcode == IDNO)
			return FALSE;

		SendPassword();

		return FALSE;
	}

	MessageBox("There was an error authenticating the account.");
	return FALSE;
}

BOOL statsAuthFinished;
void StatsAuthenticationCallback(int localid, int profileid, int authenticated, char *errmsg, void *instance)
{
	int * result = (int *)instance;

	*result = authenticated;

	if(authenticated != 1)
		MessageBox(NULL, errmsg, "Error authenticating with the stats backend", MB_OK);

	statsAuthFinished = TRUE;
}

BOOL CGp_statsDlg::StatsAuthentication()
{
	char response[33];
	int result;

	statsAuthFinished = FALSE;
	PreAuthenticatePlayerPM(
		0,
		m_profileID,
		GenerateAuth(GetChallenge(NULL), (char *)(LPCSTR)m_password, response),
		StatsAuthenticationCallback,
		&result);

	while(!statsAuthFinished)
		if(!PersistThink())
			CheckStatsConnection();

	return (result == 1);
}

void CGp_statsDlg::OnAuthenticate()
{
	UpdateData();

	if(!m_email.GetLength() || !m_nick.GetLength() || !m_password.GetLength())
	{
		MessageBox("E-mail, nick, and password must not be blank.");
		return;
	}

	if(m_authenticated)
		UnAuthenticate();

	if(!CheckAccount())
		return;

	if(!StatsAuthentication())
		return;

	Authenticated();

	UpdateData(FALSE);
}

persisttype_t TypeConversion(int type)
{
	if(type == 0)
		return pd_private_rw;
	if(type == 1)
		return pd_private_ro;
	if(type == 2)
		return pd_public_rw;
	return pd_public_ro;
}

BOOL GetKeyValue(LPCSTR src, CString & key, CString & value)
{
	const char * str;
	const char * keyStart;
	const char * valueStart;
	int keyLen;
	int valueLen;

	if(!src)
		return FALSE;

	if(src[0] != '\\')
		return FALSE;

	key.Empty();
	value.Empty();

	keyStart = (src + 1);
	valueStart = strchr(keyStart, '\\');
	if(!valueStart || (valueStart == keyStart))
		return FALSE;
	keyLen = (valueStart - keyStart);
	valueStart++;
	str = strchr(valueStart, '\\');
	if(str)
		valueLen = (str - valueStart);
	else
		valueLen = strlen(valueStart);

	char * keyStr = key.GetBuffer(keyLen);
	memcpy(keyStr, keyStart, keyLen);
	key.ReleaseBuffer(keyLen);

	char * valueStr = value.GetBuffer(valueLen);
	memcpy(valueStr, valueStart, valueLen);
	value.ReleaseBuffer(valueLen);

	return TRUE;
}

void CGp_statsDlg::ClearKeys()
{
	int count = m_keys.GetCount();
	int i;
	for(i = 0 ; i < count ; i++)
		delete (CString *)m_keys.GetItemDataPtr(i);
	m_keys.ResetContent();
}

void CGp_statsDlg::GotData(LPCSTR data)
{
	CString key;
	CString value;
	int nIndex;

	while(GetKeyValue(data, key, value))
	{
		data += (key.GetLength() + value.GetLength() + 2);

		nIndex = m_keys.AddString(key);
		if(nIndex != -1)
			m_keys.SetItemDataPtr(nIndex, new CString(value));
	}
}

BOOL getDataFinished;
void GetDataCallback(int localid, int profileid, persisttype_t type, int index, int success, time_t modified, char *data, int len, void *instance)
{
	getDataFinished = TRUE;

	if(success)
	{
		CGp_statsDlg * dlg = (CGp_statsDlg *)instance;
		dlg->GotData(data);
	}
}

void CGp_statsDlg::OnGet() 
{
	UpdateData();

	ClearKeys();
	m_value.Empty();

	if(!CheckStatsConnection())
		return;

	getDataFinished = FALSE;
	GetPersistDataValues(0, m_profileID, TypeConversion(m_type), 0, "", GetDataCallback, this);
	while(!getDataFinished)
		if(!PersistThink())
			CheckStatsConnection();

	UpdateData(FALSE);
}

BOOL setDataFinished;
void SetDataCallback(int localid, int profileid, persisttype_t type, int index, int success, time_t modified, void *instance)
{
	setDataFinished = TRUE;
}

void CGp_statsDlg::OnSet() 
{
	UpdateData();

	if(!CheckStatsConnection())
		return;

	int count = m_keys.GetCount();
	int i;
	CString data;
	CString key;
	CString * value;
	for(i = 0 ; i < count ; i++)
	{
		m_keys.GetText(i, key);
		value = (CString *)m_keys.GetItemDataPtr(i);

		data += '\\';
		data += key;
		data += '\\';
		data += *value;
	}

	setDataFinished = FALSE;
	SetPersistDataValues(0, m_profileID, TypeConversion(m_type), 0, (char *)(LPCSTR)data, SetDataCallback, this);
	while(!setDataFinished)
		if(!PersistThink())
			CheckStatsConnection();
}

void CGp_statsDlg::OnSelchangeKeys() 
{
	UpdateData();

	int nIndex = m_keys.GetCurSel();
	if(nIndex != -1)
		m_value = *(CString *)m_keys.GetItemDataPtr(nIndex);
	
	UpdateData(FALSE);
}

void CGp_statsDlg::OnAdd() 
{
	UpdateData();

	if(m_newKey.IsEmpty())
		return;

	int nIndex = m_keys.AddString(m_newKey);
	if(nIndex != -1)
		m_keys.SetItemDataPtr(nIndex, new CString);

	m_newKey.Empty();

	UpdateData(FALSE);
}

void CGp_statsDlg::OnChangeValue() 
{
	UpdateData();
	int nIndex = m_keys.GetCurSel();
	if(nIndex != -1)
	{
		CString * string = (CString *)m_keys.GetItemDataPtr(nIndex);
		*string = m_value;
	}
}
