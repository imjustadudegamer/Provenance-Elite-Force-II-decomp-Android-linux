
#if !defined(AFX_CHATTYDOC_H__A9A7E462_6E3D_4AE0_9DF8_355AE2D28FBC__INCLUDED_)
#define AFX_CHATTYDOC_H__A9A7E462_6E3D_4AE0_9DF8_355AE2D28FBC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#define NEW      1
#define DEL      2
#define RENAME   3
#define MODE     4
struct CModUsers
{
	int type;
	CString nick;
	CString newNick;
	int mode;
};

class CChattyDoc : public CDocument
{
protected: 
	CChattyDoc();
	DECLARE_DYNCREATE(CChattyDoc)

public:
	BOOL m_inChannel;
	CString m_channelName;
	CList <CString *, CString *> m_newStuff;
	CString m_topic;
	CList <CString *, CString *> m_addCallbacks;
	CList <CModUsers *, CModUsers *> m_modUsers;
	BOOL m_hide;
	int m_numUsers;

public:

	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual void OnCloseDocument();

public:
	virtual ~CChattyDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

protected:
	afx_msg void OnChannelSetTopic();
	afx_msg void OnChannelMode();
	afx_msg void OnChannelTalk();
	afx_msg void OnChannelPassword();
	afx_msg void OnChannelGetbanlist();
	DECLARE_MESSAGE_MAP()
};



#endif 
