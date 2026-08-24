
#include "stdafx.h"
#include "Voice2BuddyMFC.h"
#include "SetupDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define VOICE_THINK_TIMER_ID            101
#define VOICE_THINK_TIMER_DELAY         10

enum SetupMessage
{
	V2B_STARTDEVICES = 0
};



CSetupDlg::CSetupDlg(CWnd* pParent )
	: CDialog(CSetupDlg::IDD, pParent)
{
		// NOTE: the ClassWizard will add member initialization here
}


void CSetupDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ACTIVATELEVEL, m_ActivateLevel);
	DDX_Control(pDX, IDC_VOICELEVEL, m_VoiceLevelCtrl);
	DDX_Control(pDX, IDC_ISSPEAKING, m_IsSpeakingCtrl);
	DDX_Control(pDX, IDC_PLAYBACKCOMBO, m_PlaybackCombo);
	DDX_Control(pDX, IDC_CAPTURECOMBO, m_CaptureCombo);
}


BEGIN_MESSAGE_MAP(CSetupDlg, CDialog)
	ON_CBN_SELCHANGE(IDC_CAPTURECOMBO, OnSelChangeCaptureCombo)
	ON_CBN_SELCHANGE(IDC_PLAYBACKCOMBO, OnSelChangePlaybackCombo)
	ON_WM_TIMER()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()


BOOL CSetupDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	int aNumDevices = gvListDevices(m_SetupInfo->m_DeviceInfoArray, MAX_DEVICES, GV_CAPTURE_AND_PLAYBACK);

	for (int aDeviceNum = 0; aDeviceNum < aNumDevices; aDeviceNum++)
	{
		if (GV_CAPTURE & m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_deviceType)
		{
			m_CaptureCombo.InsertString(0, m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_name);
			m_CaptureCombo.SetItemData(0, aDeviceNum);
			if (GV_CAPTURE & m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_defaultDevice)
				m_CaptureCombo.SetCurSel(0);
		}

		if (GV_PLAYBACK & m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_deviceType)
		{
			m_PlaybackCombo.InsertString(0, m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_name);
			m_PlaybackCombo.SetItemData(0, aDeviceNum);
			if (GV_PLAYBACK & m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_defaultDevice)
				m_PlaybackCombo.SetCurSel(0);
		}
	}

	if (m_CaptureCombo.GetCurSel() == -1)
		m_CaptureCombo.SetCurSel(0);
	if (m_PlaybackCombo.GetCurSel() == -1)
		m_PlaybackCombo.SetCurSel(0);

	StartSelCaptureDevice();
	StartSelPlaybackDevice();

	UpdateData(FALSE);

	SetTimer(VOICE_THINK_TIMER_ID, VOICE_THINK_TIMER_DELAY, NULL);


	return TRUE;  
}


void CSetupDlg::OnSelChangeCaptureCombo() 
{
	StartSelCaptureDevice();	
}

void CSetupDlg::OnSelChangePlaybackCombo() 
{
	StartSelPlaybackDevice();
}
 

void CSetupDlg::StartSelCaptureDevice()
{
	if (m_SetupInfo->m_CaptureDevice != NULL)
	{
		gvStopDevice(m_SetupInfo->m_CaptureDevice, GV_CAPTURE);
		gvFreeDevice(m_SetupInfo->m_CaptureDevice);
	}

	int aSelDevice = m_CaptureCombo.GetCurSel();
	if (aSelDevice == -1)
		return;

	int aDeviceNum = m_CaptureCombo.GetItemData(aSelDevice);
	if (aDeviceNum == CB_ERR)
		return;

	m_SetupInfo->m_CaptureDevice = gvNewDevice(m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_id, GV_CAPTURE);
	if (m_SetupInfo->m_CaptureDevice != NULL)
		gvStartDevice(m_SetupInfo->m_CaptureDevice, GV_CAPTURE);
}


void CSetupDlg::StartSelPlaybackDevice()
{
	if (m_SetupInfo->m_PlaybackDevice != NULL)
	{
		gvStopDevice(m_SetupInfo->m_PlaybackDevice, GV_PLAYBACK);
		gvFreeDevice(m_SetupInfo->m_PlaybackDevice);
	}

	int aSelDevice = m_PlaybackCombo.GetCurSel();
	if (aSelDevice == -1)
		return;

	int aDeviceNum = m_PlaybackCombo.GetItemData(aSelDevice);
	if (aDeviceNum == CB_ERR)
		return;

	m_SetupInfo->m_PlaybackDevice = gvNewDevice(m_SetupInfo->m_DeviceInfoArray[aDeviceNum].m_id, GV_PLAYBACK);
	if (m_SetupInfo->m_PlaybackDevice != NULL)
		gvStartDevice(m_SetupInfo->m_PlaybackDevice, GV_PLAYBACK);
}
 
static GVBool gMute = GVFalse;

void CSetupDlg::OnTimer(UINT nIDEvent) 
{
	gvThink();

	int aBytesAvailable = gvGetAvailableCaptureBytes(m_SetupInfo->m_CaptureDevice);
	if (aBytesAvailable > 0)
	{
		GVByte        aBuffer[1024];
		int           aLength = 1024;
		GVFrameStamp  aFrameStamp;
		GVScalar      aVolume = 0;

		GVBool gotPacket = gvCapturePacket(m_SetupInfo->m_CaptureDevice, aBuffer, &aLength, &aFrameStamp, &aVolume);
		if (gotPacket == GVTrue)
		{
			gvPlayPacket(m_SetupInfo->m_PlaybackDevice, aBuffer, aLength, 0, aFrameStamp, gMute);
			m_IsSpeakingCtrl.ShowWindow(SW_SHOW);
		}
		else
		{
			m_IsSpeakingCtrl.ShowWindow(SW_HIDE);
		}

		m_VoiceLevelCtrl.SetPos( (int)(aVolume*100) );
	}
	

	CDialog::OnTimer(nIDEvent);
}

void CSetupDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	if (pScrollBar == (CScrollBar*)&m_ActivateLevel)
	{
		double aThreshold = (float)m_ActivateLevel.GetPos()/100;
		if (m_SetupInfo->m_CaptureDevice != NULL)
			gvSetCaptureThreshold(m_SetupInfo->m_CaptureDevice, aThreshold);
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
} 

BOOL CSetupDlg::DestroyWindow() 
{
	if (m_SetupInfo->m_CaptureDevice != NULL)
		gvStopDevice(m_SetupInfo->m_CaptureDevice, GV_CAPTURE);
	if (m_SetupInfo->m_PlaybackDevice != NULL)
		gvStopDevice(m_SetupInfo->m_PlaybackDevice, GV_PLAYBACK);


	KillTimer(VOICE_THINK_TIMER_ID);

	
	return CDialog::DestroyWindow();
}
