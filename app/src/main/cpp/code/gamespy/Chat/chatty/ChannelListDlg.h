#if !defined(AFX_CHANNELLISTDLG_H__2D417F5B_EB62_4913_BB7A_6B20477ED21D__INCLUDED_)
#define AFX_CHANNELLISTDLG_H__2D417F5B_EB62_4913_BB7A_6B20477ED21D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CChannelListDlg : public CDialog
{
public:
	CChannelListDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_CHANNEL_LIST };
	CEdit	m_num;
	CListBox	m_list;
	CString	m_filter;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	afx_msg void OnChannels();
	afx_msg void OnUsers();
	DECLARE_MESSAGE_MAP()
};


#endif 
