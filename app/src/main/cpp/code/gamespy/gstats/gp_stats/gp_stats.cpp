// gp_stats.cpp : Defines the class behaviors for the application.

#include "stdafx.h"
#include "gp_stats.h"
#include "gp_statsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CGp_statsApp, CWinApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CGp_statsApp::CGp_statsApp()
{
}


CGp_statsApp theApp;


BOOL CGp_statsApp::InitInstance()
{
	AfxEnableControlContainer();

	CGp_statsDlg dlg;
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
