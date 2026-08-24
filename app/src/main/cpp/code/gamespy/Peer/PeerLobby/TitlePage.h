#if !defined(AFX_TITLEPAGE_H__15BC897B_80B2_4DED_8622_568C60F30225__INCLUDED_)
#define AFX_TITLEPAGE_H__15BC897B_80B2_4DED_8622_568C60F30225__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "../peer.h"


class CTitlePage : public CPropertyPage
{
	DECLARE_DYNCREATE(CTitlePage)

public:
	void JoinGroup(int nIndex);
	CTitlePage();
	~CTitlePage();

	enum { IDD = IDD_TITLE_PAGE };
	CListCtrl	m_groups;
	CListCtrl	m_players;
	CListBox	m_chatWindow;
	CString	m_message;

	void UpdatePlayerPing(const char * nick, int ping);
	int FindPlayer(const char * nick);
	void RemovePlayer(const char * nick);
	void ChangePlayerNick(const char * oldNick, const char * newNick);

	void SendMessage();

	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual LRESULT OnWizardNext();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnClickGroups(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkGroups(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBegindragGroups(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

};


extern CTitlePage * TitlePage;

#endif 
