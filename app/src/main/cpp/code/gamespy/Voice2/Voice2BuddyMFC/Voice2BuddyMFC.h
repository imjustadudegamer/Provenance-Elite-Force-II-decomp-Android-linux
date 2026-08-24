// Voice2BuddyMFC.h : main header file for the VOICE2BUDDYMFC application

#if !defined(AFX_VOICE2BUDDYMFC_H__B124CD13_D911_4447_AF83_E1503E9147BF__INCLUDED_)
#define AFX_VOICE2BUDDYMFC_H__B124CD13_D911_4447_AF83_E1503E9147BF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		


class CVoice2BuddyMFCApp : public CWinApp
{
public:
	CVoice2BuddyMFCApp();

	public:
	virtual BOOL InitInstance();


		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	DECLARE_MESSAGE_MAP()
};




#endif 
