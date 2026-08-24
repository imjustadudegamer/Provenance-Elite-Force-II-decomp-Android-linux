// gp_stats.h : main header file for the GP_STATS application

#if !defined(AFX_GP_STATS_H__90B1DDCE_A86E_49A8_B136_0B29EA43F6B4__INCLUDED_)
#define AFX_GP_STATS_H__90B1DDCE_A86E_49A8_B136_0B29EA43F6B4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		


class CGp_statsApp : public CWinApp
{
public:
	CGp_statsApp();

	public:
	virtual BOOL InitInstance();


		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	DECLARE_MESSAGE_MAP()
};




#endif 
