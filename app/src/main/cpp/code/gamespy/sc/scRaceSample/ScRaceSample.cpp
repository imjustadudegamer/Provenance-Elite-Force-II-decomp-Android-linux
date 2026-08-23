
#include "stdafx.h"
#include "ScRaceSample.h"
#include "ScRaceSampleDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CLadderTrackApp, CWinApp)
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()


SamplePlayerData gPlayerData; 



CLadderTrackApp::CLadderTrackApp()
{
}


CLadderTrackApp theApp;


BOOL CLadderTrackApp::InitInstance()
{
	AfxEnableControlContainer();


	CScRaceSampleDlg dlg;
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
