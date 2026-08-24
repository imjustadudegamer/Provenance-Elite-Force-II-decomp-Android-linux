/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_GROUPPAGE_H__BC96C600_E6B9_49FF_8E49_75A013B824B2__INCLUDED_)
#define AFX_GROUPPAGE_H__BC96C600_E6B9_49FF_8E49_75A013B824B2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#include "../peer.h"


struct CListedGame
{
	SBServer server;
	CString name;
	PEERBool staging;
	int ping;
	int numPlayers;
	int maxPlayers;
};

class CGroupPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CGroupPage)

public:
	CGroupPage();
	~CGroupPage();

	enum { IDD = IDD_GROUP_PAGE };
	CListCtrl	m_players;
	CListCtrl	m_games;
	CListBox	m_chatWindow;
	CProgressCtrl	m_progress;
	CButton	m_sendButton;
	CString	m_message;

	void ClearGames();
	int FindListedGame(SBServer server);

	int FindPlayer(const char * nick);
	void UpdatePlayerPing(const char * nick, int ping);
	void RemovePlayer(const char * nick);
	void ChangePlayerNick(const char * oldNick, const char * newNick);

	void JoinGame(int nIndex);

	void SendMessage();

	public:
	virtual BOOL OnSetActive();
	virtual LRESULT OnWizardNext();
	virtual BOOL OnKillActive();
	virtual LRESULT OnWizardBack();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnCreate();
	afx_msg void OnClickGames(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBegindragGames(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkGames(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
};


extern CGroupPage * GroupPage;

#endif 
