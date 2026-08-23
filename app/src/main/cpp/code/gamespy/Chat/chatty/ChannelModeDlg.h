#if !defined(AFX_CHANNELMODEDLG_H__87449420_C42C_11D3_BD38_00C0F056BC39__INCLUDED_)
#define AFX_CHANNELMODEDLG_H__87449420_C42C_11D3_BD38_00C0F056BC39__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 


class CChannelModeDlg : public CDialog
{
public:
	CChannelModeDlg(CWnd* pParent = NULL);   

	enum { IDD = IDD_CHANNEL_MODE };
	BOOL	m_inviteOnly;
	int		m_limit;
	BOOL	m_moderated;
	BOOL	m_noExternalMessages;
	BOOL	m_onlyOpsChangeTopic;
	BOOL	m_private;
	BOOL	m_secret;


	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    

protected:

	DECLARE_MESSAGE_MAP()
};


#endif 
