
#if !defined(AFX_MAINFRM_H__501D21BC_112E_496A_A17B_FB30CD931AF1__INCLUDED_)
#define AFX_MAINFRM_H__501D21BC_112E_496A_A17B_FB30CD931AF1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

class CMainFrame : public CMDIFrameWnd
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

public:

public:

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	CReBar      m_wndReBar;
	CDialogBar      m_wndDlgBar;

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnTimer(UINT nIDEvent);
	DECLARE_MESSAGE_MAP()
};



#endif 
