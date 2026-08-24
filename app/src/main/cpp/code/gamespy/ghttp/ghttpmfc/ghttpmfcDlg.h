/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#if !defined(AFX_GHTTPMFCDLG_H__14B35CAF_3960_4669_972D_59B741AA032C__INCLUDED_)
#define AFX_GHTTPMFCDLG_H__14B35CAF_3960_4669_972D_59B741AA032C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CGhttpmfcDlg : public CDialog
{
public:
	CGhttpmfcDlg(CWnd* pParent = NULL);	

	enum { IDD = IDD_GHTTPMFC_DIALOG };
	CProgressCtrl	m_progress;
	BOOL	m_blocking;
	BOOL	m_completedCallback;
	CString	m_headers;
	BOOL	m_progressCallback;
	CString	m_url;
	int		m_bufferSize;
	CString	m_saveAs;
	BOOL	m_userBuffer;
	int		m_type;
	CString	m_file;
	CString	m_soFar;
	int		m_state;
	BOOL	m_throttle;
	CString	m_headersRecv;
	CString	m_status;
	BOOL	m_stepThink;
	BOOL	m_postFile;
	CString	m_postObjects;
	CString	m_postBytes;
	CString	m_proxy;

	GHTTPRequest m_request;
	char * m_memFile;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	

protected:
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnStart();
	afx_msg void OnCancel_();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnThrottle();
	afx_msg void OnThink();
	afx_msg void OnSetProxy();
	afx_msg void OnIeSettings();
	DECLARE_MESSAGE_MAP()
};


#endif 
