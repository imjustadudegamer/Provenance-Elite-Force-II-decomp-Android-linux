#include "sci.h"
#include "sciInterface.h"
#include "sciReport.h"

#include "../md5.h"
#include "../common/gsRC4.h"



SCResult SC_CALL scInitialize(int theGameId, SCInterfacePtr* theInterfaceOut)
{
	SCInterface* anInterface  = NULL;
	SCResult     anInitResult = SCResult_NO_ERROR;

	if (theInterfaceOut == NULL)
	{
		return SCResult_INVALID_PARAMETERS;
	}

	*theInterfaceOut = NULL;

	anInitResult = sciInterfaceCreate(&anInterface);
	if (anInitResult != SCResult_NO_ERROR)
	{
		return anInitResult;
	}

	anInitResult = sciInterfaceInit(anInterface);
	if (anInitResult != SCResult_NO_ERROR)
	{
		return anInitResult;
	}

	anInterface->mGameId = (gsi_u32)theGameId;

	*theInterfaceOut = anInterface;
	return SCResult_NO_ERROR;
}


SCResult SC_CALL scShutdown(SCInterfacePtr theInterface)
{
	SCInterface* anInterface = (SCInterface*)theInterface;

	if (anInterface == NULL)
	{
		return SCResult_INVALID_PARAMETERS;
	}

	if (anInterface->mInit)
	{
		sciInterfaceDestroy(anInterface);
		gsifree(anInterface);
	}

	return SCResult_NO_ERROR;
}


SCResult SC_CALL scThink(SCInterfacePtr theInterface)
{
	SCInterface* anInterface = (SCInterface*)theInterface;

	if (anInterface == NULL)
	{
		return SCResult_INVALID_PARAMETERS;
	}
	if (!anInterface->mInit)
	{
		return SCResult_NOT_INITIALIZED;
	}

	sciWsThink(&anInterface->mWebServices);

	return SCResult_NO_ERROR;
}


const char * scGetSessionId(const SCInterfacePtr theInterface)
{
	SCInterface * anInterface = (SCInterface*)theInterface;
	GS_ASSERT(theInterface != NULL);
	GS_ASSERT(gsi_is_true(anInterface->mInit));
	return (char *)anInterface->mSessionId;
}


const char * scGetConnectionId(const SCInterfacePtr theInterface)
{
	SCInterface * anInterface = (SCInterface*)theInterface;
	GS_ASSERT(theInterface != NULL);
	GS_ASSERT(gsi_is_true(anInterface->mInit));
	return (char *)anInterface->mConnectionId;
}


SCResult SC_CALL scSetSessionId(const SCInterfacePtr theInterface, const gsi_u8 theSessionId[SC_SESSION_GUID_SIZE])
{
	SCInterface * anInterface = (SCInterface*)theInterface;
	GS_ASSERT(theInterface != NULL);
	GS_ASSERT(gsi_is_true(anInterface->mInit));
	
	sciInterfaceSetSessionId(anInterface, (char *)theSessionId);
	return SCResult_NO_ERROR;
}


SCResult SC_CALL scCreateSession(SCInterfacePtr             theInterface,
							     const GSLoginCertificate * theCertificate,
							     const GSLoginPrivateData * thePrivateData,
							     SCCreateSessionCallback    theCallback,
							     gsi_time                   theTimeoutMs,
								 void *                     theUserData)
{
	SCInterface * anInterface = (SCInterface*)theInterface;
	GS_ASSERT(theInterface != NULL);
	GS_ASSERT(theCertificate != NULL || (theCertificate==NULL && thePrivateData==NULL));
	
	if (!wsLoginCertIsValid(theCertificate))
		return SCResult_INVALID_PARAMETERS;

	return sciWsCreateSession(&anInterface->mWebServices, anInterface->mGameId, theCertificate, thePrivateData, theCallback, theTimeoutMs, theUserData);
}

SCResult SC_CALL scCreateMatchlessSession(SCInterfacePtr    theInterface,
								 const GSLoginCertificate * theCertificate,
								 const GSLoginPrivateData * thePrivateData,
								 SCCreateSessionCallback    theCallback,
								 gsi_time                   theTimeoutMs,
								 void *                     theUserData)
{
	SCInterface * anInterface = (SCInterface*)theInterface;
	GS_ASSERT(theInterface != NULL);
	GS_ASSERT(theCertificate != NULL || (theCertificate==NULL && thePrivateData==NULL));

	if (!wsLoginCertIsValid(theCertificate))
		return SCResult_INVALID_PARAMETERS;

	return sciWsCreateMatchlessSession(&anInterface->mWebServices, anInterface->mGameId, theCertificate, thePrivateData, theCallback, theTimeoutMs, theUserData);
}




SCResult SC_CALL scSetReportIntention(const SCInterfacePtr         theInterface,
									  const gsi_u8                 theConnectionId[SC_CONNECTION_GUID_SIZE],
									  gsi_bool                     isAuthoritative,
									  const GSLoginCertificate *   theCertificate,
									  const GSLoginPrivateData *   thePrivateData,
									  SCSetReportIntentionCallback theCallback,
									  gsi_time                     theTimeoutMs,
									  void *                 theUserData)
{
	SCInterface* anInterface = (SCInterface*)theInterface;

	if (anInterface == NULL)
	{
		return SCResult_INVALID_PARAMETERS;
	}
	if (!theCertificate)
	{
		return SCResult_INVALID_PARAMETERS;
	}
	if (!thePrivateData)
	{
		return SCResult_INVALID_PARAMETERS;
	}
	if (!anInterface->mInit)
	{
		return SCResult_NOT_INITIALIZED;
	}

	sciInterfaceSetConnectionId(anInterface, (const char *)theConnectionId);
	return sciWsSetReportIntention(&anInterface->mWebServices, anInterface->mGameId,
		(char *)anInterface->mSessionId, (char *)anInterface->mConnectionId, isAuthoritative,
		theCertificate, thePrivateData, theCallback, theTimeoutMs, theUserData);
}

									 
SCResult SC_CALL scSubmitReport(const SCInterfacePtr  theInterface,
								const SCReportPtr      theReport,
								gsi_bool              isAuthoritative,
								const GSLoginCertificate * theCertificate,
								const GSLoginPrivateData * thePrivateData,
								SCSubmitReportCallback theCallback,
								gsi_time               theTimeoutMs,
								void *           theUserData)
{
	SCInterface* anInterface = (SCInterface*)theInterface;
	SCIReport *aReport = (SCIReport *)theReport;
	
	if (anInterface == NULL)
	{
		return SCResult_INVALID_PARAMETERS;
	}
	if (!anInterface->mInit)
	{
		return SCResult_NOT_INITIALIZED;
	}


	{
		SCIReportHeader * header = (SCIReportHeader*)aReport->mBuffer.mData;
		MD5_CTX md5;
		memset(header->mChecksum, 0, sizeof(header->mChecksum));
		
		MD5Init(&md5);
		MD5Update(&md5, (unsigned char *)aReport->mBuffer.mData, aReport->mBuffer.mPos);
		MD5Final(header->mChecksum, &md5);	
	}
	
	return sciWsSubmitReport(&anInterface->mWebServices, anInterface->mGameId,
		(char *)anInterface->mSessionId, (char *)anInterface->mConnectionId, aReport, isAuthoritative,
		theCertificate, thePrivateData, theCallback, theTimeoutMs, theUserData);

}


SCResult SC_CALL scCreateReport(const SCInterfacePtr theInterface, 
								gsi_u32 theHeaderVersion,
								gsi_u32 thePlayerCount,
								gsi_u32 theTeamCount,
								SCReportPtr * theReportDataOut)
{
	SCResult aResult = SCResult_NO_ERROR;
	SCIReport * aReport = NULL;
	SCInterface * aInterface = (SCInterface*)theInterface;

	GS_ASSERT(theInterface != NULL);
	GS_ASSERT(theReportDataOut != NULL);
	if (theInterface == NULL)
		return SCResult_INVALID_PARAMETERS;
	if (theReportDataOut == NULL)
		return SCResult_INVALID_PARAMETERS;
	
	aResult = sciCreateReport(aInterface->mSessionId, theHeaderVersion, 
		thePlayerCount, theTeamCount, &aReport);
	if (aResult == SCResult_NO_ERROR)
		*theReportDataOut = (SCReportPtr)aReport;

	return aResult;
}


SCResult SC_CALL scReportBeginGlobalData(SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;

	return sciReportBeginGlobalData(aReport);
}


SCResult SC_CALL scReportBeginPlayerData(SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;

	return sciReportBeginPlayerData(aReport);
}


SCResult SC_CALL scReportBeginTeamData(SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;

	return sciReportBeginTeamData(aReport);
}


SCResult SC_CALL scReportBeginNewTeam(SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportBeginNewTeam(aReport);
}


SCResult SC_CALL scReportBeginNewPlayer(SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportBeginNewPlayer(aReport);
}


SCResult SC_CALL scReportEnd(SCReportPtr theReport, gsi_bool isAuth, SCGameStatus theStatus)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;

	return sciReportEnd(aReport, isAuth, theStatus);
}


SCResult SC_CALL scReportSetPlayerData(SCReportPtr theReport,
								       gsi_u32 thePlayerIndex,
								       const gsi_u8  thePlayerConnectionId[SC_CONNECTION_GUID_SIZE],
								       gsi_u32 thePlayerTeamId,
								       SCGameResult theResult,
								       gsi_u32 theProfileId,
								       const GSLoginCertificate * theCertificate,
								       const gsi_u8  theAuthHash[16])
{
	SCIReport * aReport = NULL;
	SCResult aResult = SCResult_NO_ERROR;
	gsi_u32 i;
	gsi_bool isNewTeam = gsi_true;

	GS_ASSERT(theReport != NULL);
	GS_ASSERT(theCertificate != NULL);
	GSI_UNUSED(theProfileId);
	aReport = (SCIReport*)theReport;
	
	for (i=0; i<aReport->mNumTeamsReported; i++)
	{
		if (aReport->mTeamIds[i] == thePlayerTeamId)
		{
			isNewTeam = gsi_false;
			break;
		}
	}
	
	if (isNewTeam)
	{	
		aReport->mNumTeamsReported++;

		GS_ASSERT(aReport->mNumTeamsReported < SC_MAX_NUM_TEAMS);
		if (aReport->mNumTeamsReported > SC_MAX_NUM_TEAMS)
			return SCResult_OUT_OF_MEMORY;	
		
		aReport->mTeamIds[aReport->mNumTeamsReported-1] = thePlayerTeamId;
	}


	if (aResult == SCResult_NO_ERROR) aResult = sciReportSetPlayerConnectionId(aReport, thePlayerIndex, thePlayerConnectionId);
	if (aResult == SCResult_NO_ERROR) aResult = sciReportSetPlayerTeamIndex   (aReport, thePlayerIndex, thePlayerTeamId);
	if (aResult == SCResult_NO_ERROR) aResult = sciReportSetPlayerGameResult  (aReport, thePlayerIndex, theResult);
	if (aResult == SCResult_NO_ERROR) aResult = sciReportSetPlayerAuthInfo    (aReport, thePlayerIndex, theCertificate, theAuthHash);

	return aResult;
}


SCResult SC_CALL scReportSetTeamData(const SCReportPtr theReport,
									 gsi_u32     theTeamId,
									 SCGameResult theResult)
{
	SCIReport * aReport = NULL;
	gsi_u32 i;
	gsi_i32 aTeamIndex = -1;
	GS_ASSERT(theReport != NULL);
	GS_ASSERT(theResult < SCGameResultMax);

	aReport = (SCIReport*)theReport;

	GS_ASSERT(aReport->mNumTeamsReported < SC_MAX_NUM_TEAMS);
		
	for (i=0; i<aReport->mNumTeamsReported; i++)
	{
		if (aReport->mTeamIds[i] == theTeamId)
		{
			aTeamIndex = (gsi_i32)i;
			break;
		}
	}

	if (aTeamIndex == -1)
		return SCResult_INVALID_PARAMETERS;
	
	return sciReportSetTeamGameResult(aReport, (gsi_u32)aTeamIndex, theResult);
}


SCResult SC_CALL scReportSetAsMatchless(const SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return(sciReportSetAsMatchless(aReport));
}


SCResult SC_CALL scReportAddIntValue(const SCReportPtr theReport, 
									 gsi_u16     theKeyId, 
									 gsi_i32     theValue)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportAddIntValue(aReport, theKeyId, theValue);
}


SCResult SC_CALL scReportAddInt64Value(SCReportPtr theReport, 
									   gsi_u16 theKeyId, 
									   gsi_i64 theValue)
{
	SCIReport *aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport *)theReport;
	return sciReportAddInt64Value(aReport, theKeyId, theValue);
}


SCResult SC_CALL scReportAddShortValue(const SCReportPtr theReport, 
									   gsi_u16     theKeyId, 
									   gsi_i16     theValue)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportAddShortValue(aReport, theKeyId, theValue);
}


SCResult SC_CALL scReportAddByteValue(const SCReportPtr theReport, 
									  gsi_u16     theKeyId, 
									  gsi_i8      theValue)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportAddByteValue(aReport, theKeyId, theValue);
}


SCResult SC_CALL scReportAddFloatValue(const SCReportPtr  theReport, 
									   gsi_u16      theKeyId, 
									   float		theValue)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportAddFloatValue(aReport, theKeyId, theValue);

}


SCResult SC_CALL scReportAddStringValue(const SCReportPtr  theReport, 
									    gsi_u16      theKeyId, 
									    const gsi_char * theValue)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	GS_ASSERT(theValue != NULL);
	aReport = (SCIReport*)theReport;
	return sciReportAddStringValue(aReport, theKeyId, theValue);

}


SCResult SC_CALL scDestroyReport(SCReportPtr theReport)
{
	SCIReport * aReport = NULL;
	GS_ASSERT(theReport != NULL);
	aReport = (SCIReport*)theReport;

	return sciDestroyReport(aReport);
}


SCResult SC_CALL scPeerCipherInit(const GSLoginCertificate * theLocalCert, SCPeerCipher * theCipher)
{

	unsigned char randBytes[32];
	int i=0;
	MD5_CTX md5;

	GS_ASSERT(theCipher != NULL);

	#if defined (GS_CRYPT_NO_RANDOM)
		Util_RandSeed(0x2d2d2d2d);
	#else
		Util_RandSeed(current_time());
	#endif

	for (i=0; i < 32; i++)
		randBytes[i] = (unsigned char)Util_RandInt(0x00, 0xFF);

	MD5Init(&md5);
	MD5Update(&md5, randBytes, 16);
	MD5Update(&md5, (unsigned char*)theLocalCert->mPeerPublicKey.modulus.mData, 16); 
	MD5Final(&theCipher->mKey[0], &md5);

	MD5Init(&md5);
	MD5Update(&md5, &randBytes[16], 16);
	MD5Update(&md5, (unsigned char*)&theLocalCert->mPeerPublicKey.modulus.mData[16], 16); 
	MD5Final(&theCipher->mKey[16], &md5);

	theCipher->mKeyLen = 32;

	RC4Init(&theCipher->mRC4, theCipher->mKey, (int)theCipher->mKeyLen);
	theCipher->mInitialized = gsi_true;

	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice, "Created PeerCipher key block\r\n");
	gsDebugBinary(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice, (char *)theCipher->mKey, (gsi_i32)theCipher->mKeyLen);
	
	return SCResult_NO_ERROR;
}


SCResult SC_CALL scPeerCipherCreateKeyExchangeMsg(const GSLoginCertificate * theRemoteCert, const SCPeerCipher * theCipher, SCPeerKeyExchangeMsg theMsgOut)
{
	if (0 == gsCryptRSAEncryptBuffer(&theRemoteCert->mPeerPublicKey, theCipher->mKey, theCipher->mKeyLen, (unsigned char *)theMsgOut))
		return SCResult_NO_ERROR;
	else
		return SCResult_UNKNOWN_ERROR;
}


SCResult SC_CALL scPeerCipherParseKeyExchangeMsg (const GSLoginCertificate * theLocalCert, const GSLoginPrivateData * theCertPrivateData, const SCPeerKeyExchangeMsg theMsg, SCPeerCipher * theCipherOut)
{
	GSI_UNUSED(theLocalCert);
	if (0 == gsCryptRSADecryptBuffer(&theCertPrivateData->mPeerPrivateKey, (unsigned char *)theMsg, theCipherOut->mKey, &theCipherOut->mKeyLen))
	{
		gsDebugFormat(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice, "Decrypted PeerCipher key block\r\n");
		gsDebugBinary(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice, (char *)theCipherOut->mKey, (gsi_i32)theCipherOut->mKeyLen);
		RC4Init(&theCipherOut->mRC4, theCipherOut->mKey, (int)theCipherOut->mKeyLen);
		theCipherOut->mInitialized = gsi_true;
		return SCResult_NO_ERROR;
	}
	else
		return SCResult_UNKNOWN_ERROR;
}


SCResult SC_CALL scPeerCipherEncryptBufferIV(SCPeerCipher * theCipher, gsi_u32 theMessageNum, gsi_u8 * theData, gsi_u32 theLen)
{
	RC4Context rc4;
	MD5_CTX md5;
	char tempHash[16];

	GS_ASSERT(gsi_is_true(theCipher->mInitialized));

	MD5Init(&md5);
	MD5Update(&md5, theCipher->mKey, theCipher->mKeyLen);
	MD5Update(&md5, (unsigned char*)&theMessageNum, sizeof(theMessageNum));
	MD5Final((unsigned char *)tempHash, &md5);

	RC4Init(&rc4, (unsigned char *)tempHash, GS_CRYPT_MD5_HASHSIZE);
	RC4Encrypt(&rc4, (const unsigned char*)theData, theData, (int)theLen);
	return SCResult_NO_ERROR; 
}


SCResult SC_CALL scPeerCipherDecryptBufferIV(SCPeerCipher * theCipher, gsi_u32 theMessageNum, gsi_u8 * theData, gsi_u32 theLen)
{
	return scPeerCipherEncryptBufferIV(theCipher, theMessageNum, theData, theLen);
}


SCResult SC_CALL scPeerCipherEncryptBuffer(SCPeerCipher * theCipher, gsi_u8 * theData, gsi_u32 theLen)
{
	GS_ASSERT(gsi_is_true(theCipher->mInitialized));
	RC4Encrypt(&theCipher->mRC4, (const unsigned char*)theData, theData, (int)theLen);
	return SCResult_NO_ERROR; 
}


SCResult SC_CALL scPeerCipherDecryptBuffer(SCPeerCipher * theCipher, gsi_u8 * theData, gsi_u32 theLen)
{
	return scPeerCipherEncryptBuffer(theCipher, theData, theLen);
}
