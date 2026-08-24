/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "stdafx.h"
#include "ghttpmfc.h"
#include "ghttpmfcDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CGhttpmfcApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CGhttpmfcApp::CGhttpmfcApp()
{
}


CGhttpmfcApp theApp;


BOOL CGhttpmfcApp::InitInstance()
{
	CGhttpmfcDlg dlg;
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
