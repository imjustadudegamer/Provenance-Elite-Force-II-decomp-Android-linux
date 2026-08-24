/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "Voice2BuddyMFC.h"
#include "Voice2BuddyMFCDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CVoice2BuddyMFCApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CVoice2BuddyMFCApp::CVoice2BuddyMFCApp()
{
}


CVoice2BuddyMFCApp theApp;


BOOL CVoice2BuddyMFCApp::InitInstance()
{
	AfxEnableControlContainer();

	CVoice2BuddyMFCDlg dlg;
	m_pMainWnd = &dlg;
	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
	}
	else if (nResponse == IDCANCEL)
	{
	}

	return FALSE;
}
