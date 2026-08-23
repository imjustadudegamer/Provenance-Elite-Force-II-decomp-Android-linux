#if !defined(AFX_CONNECTPAGE_H__70C3619F_ED14_49F8_9155_9F96147FF4C2__INCLUDED_)
#define AFX_CONNECTPAGE_H__70C3619F_ED14_49F8_9155_9F96147FF4C2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CConnectPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CConnectPage)

public:
	CConnectPage();
	~CConnectPage();

	enum { IDD = IDD_CONNECT_PAGE };
	CString	m_nick;
	CString	m_title;
	BOOL	m_groupRooms;
	CString	m_key;


	public:
	virtual BOOL OnSetActive();
	virtual LRESULT OnWizardNext();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:
	DECLARE_MESSAGE_MAP()

};


extern CConnectPage * ConnectPage;

#endif 
