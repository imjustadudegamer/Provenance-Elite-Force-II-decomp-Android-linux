/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if !defined(AFX_CREATEPAGE_H__4FC1C8CA_9B4C_47F6_B226_C211DC74D504__INCLUDED_)
#define AFX_CREATEPAGE_H__4FC1C8CA_9B4C_47F6_B226_C211DC74D504__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CCreatePage : public CPropertyPage
{
	DECLARE_DYNCREATE(CCreatePage)

public:
	CCreatePage();
	~CCreatePage();

	enum { IDD = IDD_CREATE_PAGE };
	CString	m_name;
	int		m_maxPlayers;


	public:
	virtual BOOL OnSetActive();
	virtual LRESULT OnWizardBack();
	virtual LRESULT OnWizardNext();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:
	DECLARE_MESSAGE_MAP()

};


extern CCreatePage * CreatePage;

#endif 
