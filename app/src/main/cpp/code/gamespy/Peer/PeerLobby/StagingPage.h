#if !defined(AFX_STAGINGPAGE_H__E66D2915_B2B7_4A43_B875_D0C905FA1395__INCLUDED_)
#define AFX_STAGINGPAGE_H__E66D2915_B2B7_4A43_B875_D0C905FA1395__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CStagingPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CStagingPage)

public:
	CStagingPage();
	~CStagingPage();

	enum { IDD = IDD_STAGING_PAGE };
	CListCtrl	m_players;
	CListBox	m_chatWindow;
	CString	m_message;
	int		m_ready;

	int FindPlayer(const char * nick);
	void UpdatePlayerPing(const char * nick, int ping);
	void UpdatePlayerReady(const char * nick, BOOL ready);
	void RemovePlayer(const char * nick);
	void ChangePlayerNick(const char * oldNick, const char * newNick);

	void SendMessage();

	void CheckEnableFinish();

	public:
	virtual BOOL OnSetActive();
	virtual LRESULT OnWizardBack();
	virtual BOOL OnWizardFinish();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:
	afx_msg void OnReady();
	afx_msg void OnNotReady();
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

};


extern CStagingPage * StagingPage;

#endif 
