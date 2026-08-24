///////////////////////////////////////////////////////////////////////////////
#include "sciReport.h"
#include "sciSerialize.h"
#include "../md5.h"

#pragma warning(disable: 4267)

SCResult SC_CALL sciCreateReport(gsi_u8 theSessionGuid[SC_SESSION_GUID_SIZE], 
								 gsi_u32 theHeaderVersion,
								 gsi_u32 thePlayerCount,
								 gsi_u32 theTeamCount,
								 SCIReport ** theReportOut)
{
	SCIReport * theNewReport;
	SCIReportHeader * theReportHeader;
	gsi_u8 * theReportData;

	const gsi_u32 theRosterSize = SC_GUID_BINARY_SIZE * thePlayerCount + 
		                          SC_REPORT_TEAMINDEX_LENGTH * thePlayerCount;

	GS_ASSERT(theReportOut != NULL);

	theNewReport = (SCIReport*)gsimalloc(sizeof(SCIReport));
	if (theNewReport == NULL)
		return SCResult_OUT_OF_MEMORY;
	memset(theNewReport, 0, sizeof(SCIReport));

	theReportData = (gsi_u8*)gsimalloc(SC_REPORT_BUFFER_BYTES);
	if (theReportData == NULL)
	{
		gsifree(theNewReport);
		return SCResult_OUT_OF_MEMORY;
	}
	memset(theReportData, 0, SC_REPORT_BUFFER_BYTES);
	theNewReport->mBuffer.mIsStatic = gsi_false;
	theNewReport->mBuffer.mCapacity = SC_REPORT_BUFFER_BYTES;

	theReportHeader = (SCIReportHeader*)theReportData;
	memset(theReportHeader, 0, sizeof(SCIReportHeader));

	theReportHeader->mProtocolVersion  = htonl(SC_REPORT_PROTOCOL);
	theReportHeader->mDeveloperVersion = htonl(theHeaderVersion);

	theReportHeader->mRosterSectionLength = theRosterSize;
	theReportHeader->mAuthSectionLength = SC_REPORT_AUTHDATA_LENGTH * thePlayerCount;
	theReportHeader->mResultsSectionLength = SC_REPORT_ENTITYRESULT_LENGTH * (thePlayerCount + theTeamCount);
	theReportHeader->mPlayerCount      = (gsi_u16)thePlayerCount;
	theReportHeader->mTeamCount        = (gsi_u16)theTeamCount;
	
	theNewReport->mReportState = SCIReportState_ROSTER;
	theNewReport->mBuffer.mData = (char *)theReportData;
	theNewReport->mCurEntityStartPos = -1;
	theNewReport->mNumTeamsReported = 0;

	*theReportOut = theNewReport;
	GSI_UNUSED(theSessionGuid);
	return SCResult_NO_ERROR;
}


SCResult sciDestroyReport(SCIReport *theReport)
{
	theReport->mReportState = SCIReportState_NONE;
	theReport->mCurEntityKeyCount = 0;
	theReport->mCurEntityStartPos = 0;
	theReport->mNumTeamsReported = 0;
	theReport->mNumResultsReported = 0;
	
	gsifree(theReport->mBuffer.mData);
	theReport->mBuffer.mData = NULL;
	theReport->mBuffer.mCapacity = 0;
	theReport->mBuffer.mLen = 0;
	theReport->mBuffer.mPos = 0;
	gsifree(theReport);
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportSetPlayerConnectionId(SCIReport * theReport, gsi_u32 thePlayerIndex, const gsi_u8 theConnectionId[SC_CONNECTION_GUID_SIZE])
{
	gsi_u32 rosterDataOffset = 0;

	rosterDataOffset = sizeof(SCIReportHeader);
	rosterDataOffset += SC_REPORT_ROSTERDATA_LENGTH * thePlayerIndex;

	GS_ASSERT((rosterDataOffset + SC_GUID_BINARY_SIZE) < theReport->mBuffer.mCapacity);

	sciSerializeGUID((gsi_u8 *)&theReport->mBuffer.mData[rosterDataOffset], theConnectionId);

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportSetPlayerTeamIndex(SCIReport * theReport, gsi_u32 thePlayerIndex, gsi_u32 theTeamIndex)
{
	gsi_u32 rosterDataOffset = 0;

	rosterDataOffset = sizeof(SCIReportHeader);
	rosterDataOffset += SC_REPORT_ROSTERDATA_LENGTH * thePlayerIndex;

	rosterDataOffset += SC_GUID_BINARY_SIZE;
	GS_ASSERT((rosterDataOffset + sizeof(gsi_i32)) < theReport->mBuffer.mCapacity);
	sciSerializeInt32((gsi_u8 *)&theReport->mBuffer.mData[rosterDataOffset], (gsi_i32)theTeamIndex);
	
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportSetPlayerGameResult(SCIReport * theReport, gsi_u32 thePlayerIndex, SCGameResult theGameResult)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	gsi_u32 resultsDataOffset = 0;

	resultsDataOffset = sizeof(SCIReportHeader) + aHeader->mRosterSectionLength + aHeader->mAuthSectionLength;
	resultsDataOffset += sizeof(gsi_u32) * thePlayerIndex; 

	GS_ASSERT((resultsDataOffset + sizeof(gsi_u32)) < theReport->mBuffer.mCapacity);
	sciSerializeInt32((gsi_u8 *)&theReport->mBuffer.mData[resultsDataOffset], theGameResult);
	
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportSetPlayerAuthInfo(SCIReport * theReport, gsi_u32 thePlayerIndex, const GSLoginCertificate * theCertificate, const gsi_u8 theAuthHash[16])
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	gsi_u32 authDataOffset = 0;

	authDataOffset = sizeof(SCIReportHeader) + aHeader->mRosterSectionLength;
	authDataOffset += SC_REPORT_AUTHDATA_LENGTH * thePlayerIndex;

	GSI_UNUSED(theAuthHash);
	GSI_UNUSED(theCertificate);
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportSetTeamGameResult(SCIReport * theReport, gsi_u32 theTeamIndex, SCGameResult theGameResult)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	gsi_u32 resultsDataOffset = 0;

	resultsDataOffset = sizeof(SCIReportHeader) + aHeader->mRosterSectionLength + aHeader->mAuthSectionLength;
	resultsDataOffset += sizeof(gsi_u32) * aHeader->mPlayerCount; 
	resultsDataOffset += sizeof(gsi_u32) * theTeamIndex; 

	GS_ASSERT((resultsDataOffset + sizeof(gsi_i32)) < theReport->mBuffer.mCapacity);
	sciSerializeInt32((gsi_u8 *)&theReport->mBuffer.mData[resultsDataOffset], theGameResult);
	
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportSetAsMatchless(SCIReport * theReport)
{
	SCIReportHeader * aHeader;

	aHeader = (SCIReportHeader *)theReport->mBuffer.mData;

	aHeader->mFlags |= SC_REPORT_FLAG_MATCHLESS_SESSION;

	return SCResult_NO_ERROR;
}



SCResult SC_CALL sciReportBeginNewTeam(SCIReport * theReport)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;


	theReport->mCurEntityKeyCount = 0;
	theReport->mCurEntityStartPos = (gsi_i32)theReport->mBuffer.mPos;
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	
	aHeader->mTeamSectionLength += sizeof(gsi_u16);


	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportBeginNewPlayer(SCIReport * theReport)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	GS_ASSERT(theReport != NULL);

	{

	}

	theReport->mCurEntityKeyCount = 0;
	theReport->mCurEntityStartPos = (gsi_i32)theReport->mBuffer.mPos;
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);

	aHeader->mPlayerSectionLength += sizeof(gsi_u16);
	


	
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportBeginGlobalData(SCIReport * theReport)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;

	GS_ASSERT(theReport != NULL);
	GS_ASSERT(theReport->mReportState == SCIReportState_ROSTER);

	theReport->mReportState = SCIReportState_GLOBALDATA;

	theReport->mBuffer.mPos = sizeof(SCIReportHeader) + aHeader->mRosterSectionLength +
		aHeader->mAuthSectionLength + aHeader->mResultsSectionLength;
	
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportBeginPlayerData(SCIReport * theReport)
{

	GS_ASSERT(theReport != NULL);

	GS_ASSERT(theReport->mReportState == SCIReportState_ROSTER ||
		theReport->mReportState == SCIReportState_GLOBALDATA);
	
	if (theReport->mReportState == SCIReportState_ROSTER)
	{
	}

	theReport->mReportState    = SCIReportState_PLAYERDATA;

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportBeginTeamData(SCIReport * theReport)
{
	GS_ASSERT(theReport != NULL);

	GS_ASSERT(
		theReport->mReportState == SCIReportState_ROSTER || 
		theReport->mReportState == SCIReportState_GLOBALDATA ||
		theReport->mReportState == SCIReportState_PLAYERDATA
		);
	
	theReport->mReportState    = SCIReportState_TEAMDATA;

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportEndEntity(SCIReport * theReport)
{

	GSI_UNUSED(theReport);
	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportEnd(SCIReport * theReport, gsi_bool isAuth, SCGameStatus theStatus)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;

	sciReportEndEntity(theReport);

	if (gsi_is_true(isAuth))
		aHeader->mFlags |= SC_REPORT_FLAG_AUTHORITATIVE;

	aHeader->mGameStatus  = htonl(theStatus);

	aHeader->mFlags       = htonl(aHeader->mFlags);
	aHeader->mPlayerCount = htons(aHeader->mPlayerCount);
	aHeader->mTeamCount   = htons(aHeader->mTeamCount);
	aHeader->mReserved = 0;

	aHeader->mGameKeyCount = htons(aHeader->mGameKeyCount);
	aHeader->mPlayerKeyCount = htons(aHeader->mPlayerKeyCount);
	aHeader->mTeamKeyCount   = htons(aHeader->mTeamKeyCount);

	aHeader->mRosterSectionLength  = htonl(aHeader->mRosterSectionLength);
	aHeader->mAuthSectionLength    = htonl(aHeader->mAuthSectionLength);
	aHeader->mResultsSectionLength = htonl(aHeader->mResultsSectionLength);

	aHeader->mGameSectionLength   = htonl(aHeader->mGameSectionLength);
	aHeader->mPlayerSectionLength = htonl(aHeader->mPlayerSectionLength);
	aHeader->mTeamSectionLength   = htonl(aHeader->mTeamSectionLength);



	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportAddIntValue(SCIReport * theReport,
									  gsi_u16     theKeyId,
									  gsi_i32     theValue)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	int writtenLen = 0;
	gsi_i16 theKeyType = SCIKeyType_INT32;
	
	writtenLen += sizeof(gsi_u16) + sizeof(gsi_u16); 
	writtenLen += sizeof(gsi_u32); 

	GS_ASSERT((theReport->mBuffer.mPos + writtenLen) < theReport->mBuffer.mCapacity);

	if (theReport->mReportState == SCIReportState_GLOBALDATA)
	{
		aHeader->mGameKeyCount++;
		aHeader->mGameSectionLength += writtenLen;
	}
	else if (theReport->mReportState == SCIReportState_PLAYERDATA)
	{
		GS_ASSERT(theReport->mCurEntityStartPos != -1);
		aHeader->mPlayerKeyCount++;
		aHeader->mPlayerSectionLength += writtenLen;
		
		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	else if (theReport->mReportState == SCIReportState_TEAMDATA)
	{
		aHeader->mTeamKeyCount++;
		aHeader->mTeamSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	// Needs real error handling code or an assertion on a non-constant expression

	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], (gsi_i16)theKeyId);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theKeyType);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt32((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theValue);
	theReport->mBuffer.mPos += sizeof(gsi_u32);

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportAddInt64Value(SCIReport * theReport, 
										gsi_u16 theKeyId, 
										gsi_i64 theValue)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	int writtenLen = 0;
	gsi_i16 theKeyType = SCIKeyType_INT64;

	writtenLen += sizeof(gsi_u16) + sizeof(gsi_u16); 
	writtenLen += sizeof(gsi_i64); 

	GS_ASSERT((theReport->mBuffer.mPos + writtenLen) < theReport->mBuffer.mCapacity);

	if (theReport->mReportState == SCIReportState_GLOBALDATA)
	{
		aHeader->mGameKeyCount++;
		aHeader->mGameSectionLength += writtenLen;
	}
	else if (theReport->mReportState == SCIReportState_PLAYERDATA)
	{
		GS_ASSERT(theReport->mCurEntityStartPos != -1);
		aHeader->mPlayerKeyCount++;
		aHeader->mPlayerSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	else if (theReport->mReportState == SCIReportState_TEAMDATA)
	{
		aHeader->mTeamKeyCount++;
		aHeader->mTeamSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}

	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], (gsi_i16)theKeyId);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theKeyType);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt64((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theValue);
	theReport->mBuffer.mPos += sizeof(gsi_i64);

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportAddShortValue(SCIReport * theReport,
									    gsi_u16     theKeyId,
									    gsi_i16     theValue)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	int writtenLen = 0;
	gsi_i16 theKeyType = SCIKeyType_INT16;
	
	writtenLen += sizeof(gsi_u16) + sizeof(gsi_u16); 
	writtenLen += sizeof(gsi_u16); 

	GS_ASSERT((theReport->mBuffer.mPos + writtenLen) < theReport->mBuffer.mCapacity);

	if (theReport->mReportState == SCIReportState_GLOBALDATA)
	{
		aHeader->mGameKeyCount++;
		aHeader->mGameSectionLength += writtenLen;
	}
	else if (theReport->mReportState == SCIReportState_PLAYERDATA)
	{
		GS_ASSERT(theReport->mCurEntityStartPos != -1);
		aHeader->mPlayerKeyCount++;
		aHeader->mPlayerSectionLength += writtenLen;
		
		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	else if (theReport->mReportState == SCIReportState_TEAMDATA)
	{
		aHeader->mTeamKeyCount++;
		aHeader->mTeamSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	// Needs real error handling code or an assertion on a non-constant expression

	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], (gsi_i16)theKeyId);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theKeyType);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theValue);
	theReport->mBuffer.mPos += sizeof(gsi_u16);

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportAddByteValue(SCIReport * theReport,
									   gsi_u16     theKeyId,
									   gsi_i8      theValue)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	int writtenLen = 0;
	gsi_i16 theKeyType = SCIKeyType_BYTE;
	
	writtenLen += sizeof(gsi_u16) + sizeof(gsi_u16); 
	writtenLen += sizeof(gsi_u8); 

	GS_ASSERT((theReport->mBuffer.mPos + writtenLen) < theReport->mBuffer.mCapacity);

	if (theReport->mReportState == SCIReportState_GLOBALDATA)
	{
		aHeader->mGameKeyCount++;
		aHeader->mGameSectionLength += writtenLen;
	}
	else if (theReport->mReportState == SCIReportState_PLAYERDATA)
	{
		GS_ASSERT(theReport->mCurEntityStartPos != -1);
		aHeader->mPlayerKeyCount++;
		aHeader->mPlayerSectionLength += writtenLen;
		
		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	else if (theReport->mReportState == SCIReportState_TEAMDATA)
	{
		aHeader->mTeamKeyCount++;
		aHeader->mTeamSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	// Needs real error handling code or an assertion on a non-constant expression

	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], (gsi_i16)theKeyId);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theKeyType);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt8((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theValue);
	theReport->mBuffer.mPos += sizeof(gsi_u8);

	return SCResult_NO_ERROR;
}


SCResult SC_CALL sciReportAddFloatValue(SCIReport * theReport,
									    gsi_u16     theKeyId,
									    float       theValue)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	int writtenLen = 0;
	gsi_i16 theKeyType = SCIKeyType_FLOAT;

	writtenLen += sizeof(gsi_u16) + sizeof(gsi_u16); 
	writtenLen += sizeof(float);					 

	GS_ASSERT((theReport->mBuffer.mPos + writtenLen) < theReport->mBuffer.mCapacity);

	if (theReport->mReportState == SCIReportState_GLOBALDATA)
	{
		aHeader->mGameKeyCount++;
		aHeader->mGameSectionLength += writtenLen;
	}
	else if (theReport->mReportState == SCIReportState_PLAYERDATA)
	{
		GS_ASSERT(theReport->mCurEntityStartPos != -1);
		aHeader->mPlayerKeyCount++;
		aHeader->mPlayerSectionLength += writtenLen;
		
		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	else if (theReport->mReportState == SCIReportState_TEAMDATA)
	{
		aHeader->mTeamKeyCount++;
		aHeader->mTeamSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	// Needs real error handling code or an assertion on a non-constant expression

	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], (gsi_i16)theKeyId);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theKeyType);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeFloat((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theValue);
	theReport->mBuffer.mPos += sizeof(float);

	return SCResult_NO_ERROR;
}

SCResult SC_CALL sciReportAddStringValue(SCIReport *      theReport,
										 gsi_u16          theKeyId,
										 const gsi_char * theValue)
{
	SCIReportHeader * aHeader = (SCIReportHeader*)theReport->mBuffer.mData;
	int writtenLen = 0, numLenBytes = 0, asciiLen = 0;
	gsi_i16 theKeyType = SCIKeyType_STRING;	


#if defined(GSI_UNICODE)
	GS_ASSERT(((theReport->mBuffer.mPos + (_tcslen(theValue)) + (sizeof(gsi_u16)*2))
		< theReport->mBuffer.mCapacity));

	asciiLen = UCS2ToAsciiString(theValue, &theReport->mBuffer.mData[theReport->mBuffer.mPos])-1;

	numLenBytes = (asciiLen/127)+1; 

#else

	numLenBytes = (gsi_i32)(strlen(theValue)/127)+1; 
	asciiLen = (int)strlen(theValue);
#endif


	writtenLen += sizeof(gsi_u16) + sizeof(gsi_u16);	
	writtenLen += asciiLen;								
	writtenLen += numLenBytes;							

	GS_ASSERT((theReport->mBuffer.mPos + writtenLen) < theReport->mBuffer.mCapacity);

	if (theReport->mReportState == SCIReportState_GLOBALDATA)
	{
		aHeader->mGameKeyCount++;
		aHeader->mGameSectionLength += writtenLen;
	}
	else if (theReport->mReportState == SCIReportState_PLAYERDATA)
	{
		aHeader->mPlayerKeyCount++;
		aHeader->mPlayerSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	else if (theReport->mReportState == SCIReportState_TEAMDATA)
	{
		aHeader->mTeamKeyCount++;
		aHeader->mTeamSectionLength += writtenLen;

		theReport->mCurEntityKeyCount++;
		sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mCurEntityStartPos], (gsi_i16)theReport->mCurEntityKeyCount);
	}
	// Needs real error handling code or an assertion on a non-constant expression

	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], (gsi_i16)theKeyId);
	theReport->mBuffer.mPos += sizeof(gsi_u16);
	sciSerializeInt16((gsi_u8 *)&theReport->mBuffer.mData[theReport->mBuffer.mPos], theKeyType);
	theReport->mBuffer.mPos += sizeof(gsi_u16);

	memset(&theReport->mBuffer.mData[theReport->mBuffer.mPos], asciiLen, (gsi_u32)numLenBytes);
	theReport->mBuffer.mPos += numLenBytes;

#if defined(GSI_UNICODE)
	theReport->mBuffer.mPos += UCS2ToAsciiString(theValue, &theReport->mBuffer.mData[theReport->mBuffer.mPos])-1;
#else
	strncpy(&theReport->mBuffer.mData[theReport->mBuffer.mPos], theValue, strlen(theValue));
	theReport->mBuffer.mPos += strlen(theValue);
#endif

	return SCResult_NO_ERROR;
}

#pragma warning(default: 4267)