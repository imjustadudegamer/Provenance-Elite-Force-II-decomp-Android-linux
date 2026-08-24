// sbmfcsample.cpp : Defines the class behaviors for the application.

#include "stdafx.h"
#include "sbmfcsample.h"
#include "sbmfcsampleDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CSbmfcsampleApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


CSbmfcsampleApp::CSbmfcsampleApp()
{
}


CSbmfcsampleApp theApp;


BOOL CSbmfcsampleApp::InitInstance()
{
	AfxEnableControlContainer();

	CSbmfcsampleDlg dlg;
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
