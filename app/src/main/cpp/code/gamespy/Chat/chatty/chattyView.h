/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#if !defined(AFX_CHATTYVIEW_H__E1B25967_4DA7_4ABD_A404_9C60D59E1C60__INCLUDED_)
#define AFX_CHATTYVIEW_H__E1B25967_4DA7_4ABD_A404_9C60D59E1C60__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CChattyView : public CFormView
{
protected: 
	CChattyView();
	DECLARE_DYNCREATE(CChattyView)

public:
	enum { IDD = IDD_CHATTY_FORM };
	CEdit	m_numUsers;
	CListBox	m_callbacks;
	CListBox	m_users;
	CListBox	m_list;
	CString	m_edit;
	BOOL	m_hide;

public:
	CChattyDoc* GetDocument();

public:

	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    
	virtual void OnInitialUpdate(); 

public:
	virtual ~CChattyView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnHide();
	afx_msg void OnSelchangeCallbacks();
	afx_msg void OnDblclkUsers();
	afx_msg void OnUserBan();
	afx_msg void OnUserGetinfo();
	afx_msg void OnUserKick();
	afx_msg void OnUserModeOp();
	afx_msg void OnUserModeVoice();
	afx_msg void OnUserModeNormal();
	afx_msg void OnSendbutt();
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  
inline CChattyDoc* CChattyView::GetDocument()
   { return (CChattyDoc*)m_pDocument; }
#endif



#endif 
