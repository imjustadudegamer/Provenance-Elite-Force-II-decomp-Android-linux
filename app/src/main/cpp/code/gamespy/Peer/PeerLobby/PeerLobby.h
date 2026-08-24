// PeerLobby.h : main header file for the PEERLOBBY application

#if !defined(AFX_PEERLOBBY_H__A167413C_51DD_4C60_BA40_97CCA3876DDB__INCLUDED_)
#define AFX_PEERLOBBY_H__A167413C_51DD_4C60_BA40_97CCA3876DDB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		


class CPeerLobbyApp : public CWinApp
{
public:
	CPeerLobbyApp();

	public:
	virtual BOOL InitInstance();


		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	DECLARE_MESSAGE_MAP()
};



#endif 
