/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __SCIREPORT_H__
#define __SCIREPORT_H__


#include "sci.h"
#include "../hashtable.h"


#ifdef __cplusplus
extern "C" {
#endif 


#define SC_REPORT_PROTOCOL 2


#define SC_REPORT_ROSTERDATA_LENGTH    20 
#define SC_REPORT_PLAYERINDEX_LENGTH    2 
#define SC_REPORT_TEAMINDEX_LENGTH		4 
#define SC_REPORT_AUTHDATA_LENGTH      16 
#define SC_REPORT_ENTITYRESULT_LENGTH   4 


#define SC_REPORT_ENTITY_NONE   0
#define SC_REPORT_ENTITY_PLAYER 1
#define SC_REPORT_ENTITY_TEAM   2

#define SC_REPORT_FLAG_AUTHORITATIVE		(1<<0)
#define SC_REPORT_FLAG_NON_RELATIVE_RESULT  (1<<1)
#define SC_REPORT_FLAG_MATCHLESS_SESSION	(1<<2)

typedef enum
{
	SCIReportState_NONE,
	SCIReportState_ROSTER,
	SCIReportState_GLOBALDATA,
	SCIReportState_PLAYERDATA,
	SCIReportState_TEAMDATA
} SCIReportState;

typedef enum
{
	SCIKeyType_INT32,
	SCIKeyType_INT16,
	SCIKeyType_BYTE,
	SCIKeyType_STRING,
	SCIKeyType_FLOAT,
	SCIKeyType_INT64
} SCIKeyType;

typedef struct SCIReportHeader
{
	gsi_u32  mProtocolVersion;      
	gsi_u32  mDeveloperVersion;
	gsi_u8   mChecksum[GS_CRYPT_MD5_HASHSIZE];        
	gsi_u32  mGameStatus;
	gsi_u32  mFlags;                
	gsi_u16  mPlayerCount;          
	gsi_u16  mTeamCount;            
	gsi_u16  mGameKeyCount;
	gsi_u16  mPlayerKeyCount;
	gsi_u16  mTeamKeyCount;
	gsi_u16  mReserved;             
	gsi_u32  mRosterSectionLength;  
	gsi_u32  mAuthSectionLength;    
	gsi_u32  mResultsSectionLength; 
	gsi_u32  mGameSectionLength;    
	gsi_u32  mPlayerSectionLength;  
	gsi_u32  mTeamSectionLength;    
} SCIReportHeader;


typedef struct SCIReportBuffer
{
	gsi_bool mIsStatic;

	gsi_u32 mLen;
	gsi_u32 mCapacity;
	gsi_u32 mPos;

	char * mData;
	
} SCIReportBuffer;

typedef struct SCIReport
{
	SCIReportState mReportState;

	gsi_i32 mCurEntityStartPos;  
	gsi_u16 mCurEntityKeyCount;  

	gsi_u32 mNumResultsReported; 

	gsi_u32 mNumTeamsReported;	 
	gsi_u32 mTeamIds[SC_MAX_NUM_TEAMS];		

	SCIReportBuffer mBuffer;
} SCIReport;


SCResult sciCreateReport(gsi_u8 theSessionGuid[16], 
						 gsi_u32 theHeaderVersion, 
						 gsi_u32 thePlayerCount,
						 gsi_u32 theTeamCount,
						 SCIReport ** theReportOut);

SCResult sciDestroyReport(SCIReport *theReport);

SCResult SC_CALL sciReportSetPlayerConnectionId(SCIReport * theReport, gsi_u32 thePlayerIndex, const gsi_u8 theConnectionId[SC_CONNECTION_GUID_SIZE]);
SCResult SC_CALL sciReportSetPlayerTeamIndex   (SCIReport * theReport, gsi_u32 thePlayerIndex, gsi_u32 theTeamIndex);
SCResult SC_CALL sciReportSetPlayerGameResult  (SCIReport * theReport, gsi_u32 thePlayerIndex, SCGameResult theGameResult);
SCResult SC_CALL sciReportSetPlayerAuthInfo    (SCIReport * theReport, gsi_u32 thePlayerIndex, const GSLoginCertificate * theCertificate, const gsi_u8 theAuthHash[16]);
SCResult SC_CALL sciReportSetTeamGameResult    (SCIReport * theReport, gsi_u32 theTeamIndex  , SCGameResult theGameResult);
SCResult SC_CALL sciReportSetAsMatchless       (SCIReport * theReport);

SCResult SC_CALL sciReportBeginGlobalData(SCIReport * theReport);
SCResult SC_CALL sciReportBeginPlayerData(SCIReport * theReport);
SCResult SC_CALL sciReportBeginTeamData  (SCIReport * theReport);

SCResult SC_CALL sciReportAddIntValue(SCIReport * theReport,
									  gsi_u16     theKeyId,
									  gsi_i32     theValue);
SCResult SC_CALL sciReportAddInt64Value(SCIReport * theReport,
										gsi_u16     theKeyId,
										gsi_i64     theValue);
SCResult SC_CALL sciReportAddShortValue(SCIReport * theReport,
									    gsi_u16     theKeyId,
									    gsi_i16     theValue);
SCResult SC_CALL sciReportAddByteValue(SCIReport * theReport,
									   gsi_u16     theKeyId,
									   gsi_i8      theValue);
SCResult SC_CALL sciReportAddFloatValue(SCIReport * theReport,
									    gsi_u16     theKeyId,
									    float       theValue);
SCResult SC_CALL sciReportAddStringValue(SCIReport *      theReport,
										 gsi_u16          theKeyId,
										 const gsi_char * theValue);

SCResult SC_CALL sciReportBeginNewTeam(SCIReport * theReport);
SCResult SC_CALL sciReportBeginNewPlayer(SCIReport * theReport);
SCResult SC_CALL sciReportEndEntity(SCIReport * theReport);
SCResult SC_CALL sciReportEnd(SCIReport * theReport, gsi_bool isAuth, SCGameStatus theStatus);






#ifdef __cplusplus
} 
#endif 


#endif 
