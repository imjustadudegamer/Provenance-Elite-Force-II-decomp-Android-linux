// ladderTrack.h : main header file for the LADDERTRACK application

#if !defined(AFX_LADDERTRACK_H__E9856F44_580A_48C0_ABFF_6FFA9BA944A3__INCLUDED_)
#define AFX_LADDERTRACK_H__E9856F44_580A_48C0_ABFF_6FFA9BA944A3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif 

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		



#define SCRACE_GAMENAME		"sc_race"
#define SCRACE_SECRETKEY	"Zc0eM6"
#define SCRACE_GAMEID		1649
#define SCRACE_PRODUCTID	11030

typedef struct SamplePlayerData
{
	gsi_u32 mProfileId;
	GSLoginCertificate mCertificate;
	GSLoginPrivateData mPrivateData;
	SCPeerCipher mPeerSendCipher; 
	SCPeerCipher mPeerRecvCipher; 
	
	SCInterfacePtr mStatsInterface;

	gsi_u8  mSessionId[SC_SESSION_GUID_SIZE];
	gsi_u8  mConnectionId[SC_CONNECTION_GUID_SIZE];
	gsi_u8  mStatsAuthdata[16];

	gsi_i32 mFrags;
	gsi_i32 mScore;
	gsi_i16 mDeaths;
	gsi_i16 mShots;
	gsi_i32 mTeam;
	
	SCHiddenData mHiddenFrags;
	SCHiddenData mHiddenDeaths;
	SCHiddenData mHiddenShots;
	SCHiddenData mHiddenScore;

	gsi_u32 mWaitCount; 

} SamplePlayerData;

extern SamplePlayerData gPlayerData;


class CLadderTrackApp : public CWinApp
{
public:
	CLadderTrackApp();

	public:
	virtual BOOL InitInstance();


	DECLARE_MESSAGE_MAP()
};




#endif 
