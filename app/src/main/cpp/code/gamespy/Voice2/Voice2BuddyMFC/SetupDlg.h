#if !defined(AFX_SETUPDLG_H__36B5B6D7_690D_4628_92E4_96B2CAE34B82__INCLUDED_)
#define AFX_SETUPDLG_H__36B5B6D7_690D_4628_92E4_96B2CAE34B82__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "../../voice2/gv.h"


const unsigned int MAX_DEVICES = 10;

struct VoiceSetupInfo
{
	GVDevice      m_PlaybackDevice;
	GVDevice      m_CaptureDevice;
	GVDeviceInfo  m_DeviceInfoArray[MAX_DEVICES];
};


class CSetupDlg : public CDialog
{
public:
	VoiceSetupInfo* m_SetupInfo;

	void StartSelCaptureDevice();
	void StartSelPlaybackDevice();



public:
	CSetupDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_SETUPDIALOG };
	CSliderCtrl	m_ActivateLevel;
	CProgressCtrl	m_VoiceLevelCtrl;
	CStatic	m_IsSpeakingCtrl;
	CComboBox	m_PlaybackCombo;
	CComboBox	m_CaptureCombo;


	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	virtual BOOL OnInitDialog();
	afx_msg void OnSelChangeCaptureCombo();
	afx_msg void OnSelChangePlaybackCombo();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	DECLARE_MESSAGE_MAP()
};


#endif 
