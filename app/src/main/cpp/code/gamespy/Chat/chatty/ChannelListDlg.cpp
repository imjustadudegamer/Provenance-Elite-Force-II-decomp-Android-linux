
#include "stdafx.h"
#include "chatty.h"
#include "ChannelListDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CChannelListDlg::CChannelListDlg(CWnd* pParent )
	: CDialog(CChannelListDlg::IDD, pParent)
{
	m_filter = _T("");
}


void CChannelListDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NUM, m_num);
	DDX_Control(pDX, IDC_LIST, m_list);
	DDX_Text(pDX, IDC_FILTER, m_filter);
}


BEGIN_MESSAGE_MAP(CChannelListDlg, CDialog)
	ON_BN_CLICKED(ID_CHANNELS, OnChannels)
	ON_BN_CLICKED(ID_USERS, OnUsers)
END_MESSAGE_MAP()


void EnumChannelsCallbackEach(CHAT chat, CHATBool success, int index, const char * channel, const char * topic, int numUsers, void * param)
{
	if(success)
	{
		CChannelListDlg * dlg = (CChannelListDlg *)param;

		CListBox * list = &dlg->m_list;

		CString str;
		str = channel;
		str += " (";
		char buffer[16];
		itoa(numUsers, buffer, 10);
		str += buffer;
		str += "): ";
		str += topic;

		list->AddString(str);

		char buf[16];
		itoa(index + 1, buf, 10);
		dlg->m_num.SetWindowText(buf);
	}

	GSI_UNUSED(chat);
}

void EnumJoinedChannelsCallback(CHAT chat,
											  int index,
											  const char * channel,
											  void * param)
{
	CChannelListDlg * dlg = (CChannelListDlg *)param;

	CListBox * list = &dlg->m_list;

	CString str;
	str = "IN: ";
	str += channel;
	str += " (";
	char buffer[16];
	itoa(index, buffer, 10);
	str += buffer;
	str += "): ";
	list->AddString(str);

	GSI_UNUSED(chat);
}

void EnumChannelsCallbackAll(CHAT chat, CHATBool success, int numChannels, const char ** channels, const char ** topics, int * numUsers, void * param)
{
	if(success)
	{
		CChannelListDlg * dlg = (CChannelListDlg *)param;
		dlg->MessageBox("Search Complete");
	}

	GSI_UNUSED(numUsers);
	GSI_UNUSED(topics);
	GSI_UNUSED(channels);
	GSI_UNUSED(numChannels);
	GSI_UNUSED(chat);
}

void ListUsers(CHAT chat, CHATBool success, const char * channel, int numUsers, const char ** operators, int * modes, void * param)
{
	if(success)
	{
		CChannelListDlg * dlg = (CChannelListDlg *)param;

		CListBox * list = &dlg->m_list;

		CString str;
		for(int i = 0 ; i < numUsers ; i++)
		{
			str = operators[i];
			if(modes[i] & CHAT_OP)
				str.Insert(0, '@');
			else if(modes[i] & CHAT_VOICE)
				str.Insert(0, '?');

			list->AddString(str);
		}

		char buf[16];
		itoa(numUsers, buf, 10);
		dlg->m_num.SetWindowText(buf);
	}

	GSI_UNUSED(channel);
	GSI_UNUSED(chat);
}

void CChannelListDlg::OnChannels() 
{
	m_list.ResetContent();

	UpdateData();
	chatEnumJoinedChannels(theApp.m_chat, EnumJoinedChannelsCallback, this);
	chatEnumChannels(theApp.m_chat, m_filter, EnumChannelsCallbackEach, EnumChannelsCallbackAll, this, CHATFalse);
}

void CChannelListDlg::OnUsers() 
{
	m_list.ResetContent();

	UpdateData();
	chatEnumUsers(theApp.m_chat, m_filter, ListUsers, this, CHATFalse);
}
