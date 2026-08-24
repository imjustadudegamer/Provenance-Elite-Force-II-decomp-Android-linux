/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "ghttpCommon.h"
#if defined(MATRIXSSL)
#include "../matrixssl/matrixssl.h"
#endif


GHTTPBool ghttpSetRequestEncryptionEngine(GHTTPRequest request, GHTTPEncryptionEngine engine)
{
	GHIConnection * connection = ghiRequestToConnection(request);
	if(!connection)
		return GHTTPFalse;
	
	if (engine == GHTTPEncryptionEngine_Default)
	{
		#if defined(MATRIXSSL)
			engine = GHTTPEncryptionEngine_MatrixSsl;
		#elif defined(REVOEXSSL)
			engine = GHTTPEncryptionEngine_RevoEx;
		#else
			engine = GHTTPEncryptionEngine_GameSpy;
		#endif
	}

	if (connection->encryptor.mEngine == engine)
		return GHTTPTrue; 

	if (connection->encryptor.mInterface != NULL &&
		connection->encryptor.mEngine != engine)
	{
		return GHTTPFalse; 
	}

	if((engine == GHTTPEncryptionEngine_None) && (strncmp(connection->URL, "https://", 8) == 0))
		return GHTTPFalse;

	connection->encryptor.mEngine = engine;

	if (engine == GHTTPEncryptionEngine_None)
	{
		connection->encryptor.mInterface = NULL;
		return GHTTPTrue; 
	}
	else
	{
		if (engine != GHTTPEncryptionEngine_Default)
		{
			#if defined(MATRIXSSL)
				GS_ASSERT(engine==GHTTPEncryptionEngine_MatrixSsl);
			#elif defined(REVOEXSSL)
				GS_ASSERT(engine==GHTTPEncryptionEngine_RevoEx);
			#else
				GS_ASSERT(engine==GHTTPEncryptionEngine_GameSpy);
			#endif
		}
		
		connection->encryptor.mInterface   = NULL;
		connection->encryptor.mInitFunc    = ghiEncryptorSslInitFunc;
		connection->encryptor.mStartFunc   = ghiEncryptorSslStartFunc;
		connection->encryptor.mCleanupFunc = ghiEncryptorSslCleanupFunc;
		connection->encryptor.mEncryptFunc = ghiEncryptorSslEncryptFunc;
		connection->encryptor.mDecryptFunc = ghiEncryptorSslDecryptFunc;
		connection->encryptor.mInitialized = GHTTPFalse;
		connection->encryptor.mSessionStarted = GHTTPFalse;
		connection->encryptor.mSessionEstablished = GHTTPFalse;
		connection->encryptor.mEncryptOnBuffer = GHTTPTrue;
		connection->encryptor.mEncryptOnSend   = GHTTPFalse;
		connection->encryptor.mLibSendsHandshakeMessages = GHTTPFalse;
		return GHTTPTrue;
	}
}


#ifdef MATRIXSSL

static int ghiSslCertValidator(struct sslCertInfo* theCertInfo, void* theUserData)
{
	sslCertInfo_t	*next;
	next = theCertInfo;
	while (next->next != NULL) {
		next = next->next;
	}
	return next->verified;
}

GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	sslKeys_t *keys = NULL;
	sslSessionId_t *id = NULL;

	int ecodeResult;

	if (matrixSslOpen() < 0)
		return GHIEncryptionResult_Error;

	if (matrixSslReadKeys(&keys, NULL, NULL, NULL, NULL) < 0)
		return GHIEncryptionResult_Error;

	if (matrixSslNewSession((ssl_t**)&theEncryptor->mInterface, keys, id, 0) < 0)
		return GHIEncryptionResult_Error;

	matrixSslSetCertValidator((ssl_t*)theEncryptor->mInterface, ghiSslCertValidator, NULL);

	theEncryptor->mInitialized = GHTTPTrue;
	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	sslBuf_t helloWrapper;
	
	helloWrapper.buf   = connection->sendBuffer.data;
	helloWrapper.size  = connection->sendBuffer.size;
	helloWrapper.start = connection->sendBuffer.data + connection->sendBuffer.pos;
	helloWrapper.end   = helloWrapper.start; 
	
	ecodeResult = matrixSslEncodeClientHello((ssl_t*)theEncryptor->mInterface, &helloWrapper, 0); 
	if (ecodeResult != 0) 
		return GHIEncryptionResult_Error; 

	connection->sendBuffer.len += (int)(helloWrapper.end - helloWrapper.start);
	connection->sendBuffer.encrypted = GHTTPTrue;
	theEncryptor->mSessionStarted = GHTTPTrue;
}

GHIEncryptionResult ghiEncryptorSslCleanupFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor)
{
	matrixSslClose();
	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslEncryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * thePlainTextBuffer,
												 int          thePlainTextLength,
												 char *       theEncryptedBuffer,
												 int *        theEncryptedLength)
{
	int encodeResult = 0;

	sslBuf_t encryptedBuf;
	encryptedBuf.buf   = theEncryptedBuffer;  
	encryptedBuf.start = theEncryptedBuffer;  
	encryptedBuf.end   = theEncryptedBuffer;  
	encryptedBuf.size  = *theEncryptedLength; 
	
	encodeResult = matrixSslEncode(connection->encryptor.mInterface, 
		(unsigned char*)thePlainTextBuffer, *thePlainTextLength, &encryptedBuf);

	if (encodeResult == SSL_ERROR)
		return GHIEncryptionResult_Error;
	else if (encodeResult == SSL_FULL)
		return GHIEncryptionResult_BufferTooSmall;
	else
	{
		*theEncryptedLength -= (int)(encryptedBuf.end - encryptedBuf.start);
		return GHIEncryptionResult_Success;
	}
}

GHIEncryptionResult ghiEncryptorSslDecryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * theEncryptedBuffer,
												 int *        theEncryptedLength,
												 char *       theDecryptedBuffer,
												 int *        theDecryptedLength)
{
	GHTTPBool decryptMore = GHTTPTrue;
	int decodeResult = 0;

	sslBuf_t inBuf;
	sslBuf_t decryptedBuf;
	int encryptedStartSize = *theEncryptedLength;

	inBuf.buf   = (unsigned char*)theEncryptedBuffer;  
	inBuf.start = (unsigned char*)theEncryptedBuffer;
	inBuf.end   = (unsigned char*)theEncryptedBuffer + *theEncryptedLength;
	inBuf.size  = *theEncryptedLength;

	decryptedBuf.buf   = theDecryptedBuffer;  
	decryptedBuf.start = theDecryptedBuffer;  
	decryptedBuf.end   = theDecryptedBuffer;  
	decryptedBuf.size  = *theDecryptedLength; 
	
	while(decryptMore != GHTTPFalse && ((inBuf.end-inBuf.start) > 0))
	{
		unsigned char error = 0;
		unsigned char alertlevel = 0;
		unsigned char alertdescription = 0;

		decodeResult = matrixSslDecode(theEncryptor->mInterface, &inBuf, &decryptedBuf, 
										&error, &alertlevel, &alertdescription);
		switch(decodeResult)
		{
		case SSL_SUCCESS:          
			if (matrixSslHandshakeIsComplete(theEncryptor->mInterface))
				theEncryptor->mSessionEstablished = GHTTPTrue;
			break;

		case SSL_PROCESS_DATA:
			break;

		case SSL_SEND_RESPONSE:
			{
			int responseSize = decryptedBuf.end - decryptedBuf.start;

			theEncryptor->mSessionEstablished = GHTTPFalse;
			ghiTrySendThenBuffer(connection, decryptedBuf.start, responseSize);
			theEncryptor->mSessionEstablished = GHTTPTrue;

			decryptedBuf.end = decryptedBuf.start; 
			break;
			}

		case SSL_ERROR:            
			decryptMore = GHTTPFalse;
			break;

		case SSL_ALERT:            
			if (alertdescription == SSL_ALERT_CLOSE_NOTIFY)
			decryptMore = GHTTPFalse;
			break;

		case SSL_PARTIAL:          
			decryptMore = GHTTPFalse;
			break;

		case SSL_FULL:             
			{
				decryptMore = GHTTPFalse;
				break;
			}
		};
	}

	*theEncryptedLength = encryptedStartSize - (inBuf.end - inBuf.start);
	*theDecryptedLength = decryptedBuf.end - decryptedBuf.start;

	if (decodeResult == SSL_FULL)
		return GHIEncryptionResult_BufferTooSmall;
	else if (decodeResult == SSL_ERROR || decodeResult == SSL_ALERT)
		return GHIEncryptionResult_Error;

	return GHIEncryptionResult_Success;
}


#elif defined(REVOEXSSL)
#include <revolution/ssl.h>

typedef struct gsRevoExInterface
{
	SSLId mId;
	SSLClientCertId mClientCertId;
	SSLRootCAId mRootCAId;
	GHTTPBool mConnected; 
} gsRevoExInterface;

GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	int i=0;
	gsRevoExInterface* sslInterface = NULL;

	GS_ASSERT(theEncryptor->mInitialized == GHTTPFalse);
	GS_ASSERT(theEncryptor->mInterface == NULL);

	theEncryptor->mInterface = gsimalloc(sizeof(gsRevoExInterface));
	if (theEncryptor->mInterface == NULL)
	{
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Memory, GSIDebugLevel_WarmError,
			"Failed to allocate SSL interface (out of memory: %d bytes)\r\n", sizeof(gsRevoExInterface));
		return GHIEncryptionResult_Error;
	}
	memset(theEncryptor->mInterface, 0, sizeof(gsRevoExInterface));
	sslInterface = (gsRevoExInterface*)theEncryptor->mInterface;

	{
		int verifyOption = 0;
		int rcode = 0;
		
		verifyOption = SSL_VERIFY_COMMON_NAME
			| SSL_VERIFY_ROOT_CA
			| SSL_VERIFY_DATE
			| SSL_VERIFY_CHAIN
			| SSL_VERIFY_SUBJECT_ALT_NAME;

		sslInterface->mId = SSLNew(verifyOption, connection->serverAddress);

		rcode = SSLSetBuiltinRootCA(sslInterface->mId, SSL_ROOTCA_NINTENDO_1);
		if(rcode != SSL_ENONE){
			SSLShutdown(sslInterface->mId);
			return GHIEncryptionResult_Error;
		}

		rcode = SSLSetBuiltinClientCert(sslInterface->mId, SSL_CLIENTCERT_NINTENDO_0);
		if(rcode != SSL_ENONE){
			SSLShutdown(sslInterface->mId);
			return GHIEncryptionResult_Error;
		}
	}

	theEncryptor->mInitialized = GHTTPTrue;
	theEncryptor->mSessionStarted = GHTTPFalse;
	theEncryptor->mSessionEstablished = GHTTPFalse;
	theEncryptor->mEncryptOnBuffer = GHTTPFalse;
	theEncryptor->mEncryptOnSend = GHTTPTrue;
	theEncryptor->mLibSendsHandshakeMessages = GHTTPTrue;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
		"GameSpy SSL (RevoEx engine) initialized\r\n");

	return GHIEncryptionResult_Success;
}

											  
GHIEncryptionResult ghiEncryptorSslCleanupFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor)
{
	if (theEncryptor != NULL)
	{
		gsRevoExInterface* sslInterface = (gsRevoExInterface*)theEncryptor->mInterface;
		if (sslInterface != NULL)
		{
			SSLShutdown(sslInterface->mId);
			gsifree(sslInterface);
			theEncryptor->mInterface = NULL;
		}
		theEncryptor->mInitialized = GHTTPFalse;
		theEncryptor->mSessionStarted = GHTTPFalse;
		theEncryptor->mSessionEstablished = GHTTPFalse;
	}

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
		"GameSpy SSL (RevoEx) engine cleanup called\r\n");

	GSI_UNUSED(connection);

	return GHIEncryptionResult_Success;
}


GHIEncryptionResult ghiEncryptorSslStartFunc(struct GHIConnection * connection,
											struct GHIEncryptor  * theEncryptor)
{
	gsRevoExInterface* sslInterface = (gsRevoExInterface*)theEncryptor->mInterface;
	int result = 0;
	
	GS_ASSERT(theEncryptor->mSessionStarted == GHTTPFalse);
	
	if (!sslInterface->mConnected)
	{
		result = SSLConnect(sslInterface->mId, connection->socket);
		if (result != SSL_ENONE)
		{
			switch(result)
			{
				case SSL_EFAILED:
					gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
						"GameSpy SSL (RevoEx) SSLConnect failed (SSL_EFAILED)\r\n");
					break;
				case SSL_ESSLID:
					gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
						"GameSpy SSL (RevoEx) SSLConnect failed (SSL_ESSLID)\r\n");
					break;
				default:
					gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
						"GameSpy SSL (RevoEx) SSLConnect failed (Unhandled Error)\r\n");
					break;
			}
			return GHIEncryptionResult_Error;
		}
		sslInterface->mConnected = GHTTPTrue;
	}

	GS_ASSERT(sslInterface->mConnected == GHTTPTrue);
		
	result = SSLDoHandshake(sslInterface->mId);
	if (result != SSL_ENONE)
	{
		if (result == SSL_EWANT_READ || result == SSL_EWANT_WRITE)
			return GHIEncryptionResult_Success;

		switch(result)
		{
			case SSL_EFAILED:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EFAILED)\r\n");
				break;
			case SSL_ESSLID:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_ESSLID)\r\n");
				break;
			case SSL_ESYSCALL:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_ESYSCALL)\r\n");
				break;
			case SSL_EZERO_RETURN:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EZERO_RETURN)\r\n");
				break;
			case SSL_EWANT_CONNECT:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EWANT_CONNECT)\r\n");
				break;
			case SSL_EVERIFY_COMMON_NAME:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EVERIFY_COMMON_NAME)\r\n");
				break;
			case SSL_EVERIFY_CHAIN:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EVERIFY_CHAIN)\r\n");
				break;
			case SSL_EVERIFY_ROOT_CA:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EVERIFY_ROOT_CA)\r\n");
				break;
			case SSL_EVERIFY_DATE:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (SSL_EVERIFY_DATE)\r\n");
				break;																																				
			default:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLDoHandshake failed (Unhandled Error)\r\n");
				break;
		}
		return GHIEncryptionResult_Error;
	}

	theEncryptor->mSessionStarted = GHTTPTrue;
	theEncryptor->mSessionEstablished = GHTTPTrue;
	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslEncryptSend(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * thePlainTextBuffer,
												 int          thePlainTextLength,
												 int *        theBytesSentOut)
{
	gsRevoExInterface* sslInterface = (gsRevoExInterface*)theEncryptor->mInterface;
	int result = 0;
	
	result = SSLWrite(sslInterface->mId, thePlainTextBuffer, thePlainTextLength);
	if (result == SSL_EZERO_RETURN)
	{
		*theBytesSentOut = 0;
	}
	else if (result == SSL_EWANT_WRITE)
	{
		*theBytesSentOut = -1; 
	}
	else if (result < 0)
	{
		switch(result)
		{
			case SSL_EFAILED:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLWrite failed (SSL_EFAILED)\r\n");
				break;
			case SSL_ESSLID:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLWrite failed (SSL_ESSLID)\r\n");
				break;
			case SSL_EWANT_READ:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLWrite failed (SSL_EWANT_READ)\r\n");
				break;
			case SSL_ESYSCALL:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLWrite failed (SSL_ESYSCALL)\r\n");
				break;
			case SSL_EWANT_CONNECT:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLWrite failed (SSL_EWANT_CONNECT)\r\n");
				break;
			default:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLWrite failed (Unhandled Error)\r\n");
				break;
		}
		return GHIEncryptionResult_Error;
	}
	else
	{
		GS_ASSERT(result > 0);
		*theBytesSentOut = result;	
	}
	GSI_UNUSED(connection);
	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslDecryptRecv(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 char * theDecryptedBuffer,
												 int *  theDecryptedLength)
{
	gsRevoExInterface* sslInterface = (gsRevoExInterface*)theEncryptor->mInterface;
	int result = 0;
	
	result = SSLRead(sslInterface->mId, theDecryptedBuffer, *theDecryptedLength);
	if (result == SSL_EZERO_RETURN)
	{
		*theDecryptedLength = 0;
	}
	else if (result == SSL_EWANT_READ)
	{
		*theDecryptedLength = -1; 
	}
	else if (result < 0)
	{
		switch(result)
		{
			case SSL_EFAILED:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLRead failed (SSL_EFAILED)\r\n");
				break;
			case SSL_ESSLID:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLRead failed (SSL_ESSLID)\r\n");
				break;
			case SSL_EWANT_WRITE:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLRead failed (SSL_EWANT_WRITE)\r\n");
				break;
			case SSL_ESYSCALL:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLRead failed (SSL_ESYSCALL)\r\n");
				break;
			case SSL_EWANT_CONNECT:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLRead failed (SSL_EWANT_CONNECT)\r\n");
				break;
			default:
				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"GameSpy SSL (RevoEx) SSLRead failed (Unhandled Error)\r\n");
				break;
		}
		return GHIEncryptionResult_Error;
	}
	else
	{
		GS_ASSERT(result > 0);
		*theDecryptedLength = result;
	}
	GSI_UNUSED(connection);
	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslEncryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * thePlainTextBuffer,
												 int          thePlainTextLength,
												 char *       theEncryptedBuffer,
												 int *        theEncryptedLength)
{
	GS_FAIL(); 

	GSI_UNUSED(connection);
	GSI_UNUSED(theEncryptor);
	GSI_UNUSED(thePlainTextBuffer);
	GSI_UNUSED(thePlainTextLength);
	GSI_UNUSED(theEncryptedBuffer);
	GSI_UNUSED(theEncryptedLength);
	
	return GHIEncryptionResult_Error;
}

GHIEncryptionResult ghiEncryptorSslDecryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * theEncryptedBuffer,
												 int *        theEncryptedLength,
												 char *       theDecryptedBuffer,
												 int *        theDecryptedLength)
{
	GS_FAIL(); 

	GSI_UNUSED(connection);
	GSI_UNUSED(theEncryptor);
	GSI_UNUSED(theEncryptedBuffer);
	GSI_UNUSED(theEncryptedLength);
	GSI_UNUSED(theDecryptedBuffer);
	GSI_UNUSED(theDecryptedLength);

	return GHIEncryptionResult_Error;
}

#else

#include "../common/gsSSL.h"
#include "../common/gsSHA1.h"
#include "../common/gsRC4.h"
#include "../md5.h"


static GHIEncryptionResult ghiEncryptorProcessSSLHandshake(struct GHIConnection * connection,
													  struct GHIEncryptor * encryptor,
													  GHIBuffer * data);

static void ghiEncryptorWriteNBOLength(unsigned char* buf, int value, int size)
{
	int NBO = (int)htonl(value);
	unsigned char* NBOData = (unsigned char*)&NBO;

	assert(size <= sizeof(NBO));
	if (size > sizeof(NBO))
		return; 

	memcpy(buf, NBOData+(sizeof(int)-size), (size_t)size);
}
static GHTTPBool ghiEncryptorReadNBOLength(GHIBuffer * data, int* value, int size)
{
	assert(size <= sizeof(*value));
	if (size > sizeof(*value))
		return GHTTPFalse;
	if (GHTTPFalse == ghiReadDataFromBufferFixed(data, ((char*)value)+(sizeof(int)-size), size))
		return GHTTPFalse;

	*value = (int)htonl(*value);
	return GHTTPTrue;
}

static GHTTPBool ghiEncryptorParseASN1Sequence(GHIBuffer * data, int* lenOut)
{
	char tempChar = '\0';

	if (GHTTPFalse == ghiReadDataFromBufferFixed(data, &tempChar, 1))
		return GHTTPFalse;
	if (tempChar != 0x30) 
		return GHTTPFalse;

	if (GHTTPFalse == ghiReadDataFromBufferFixed(data, &tempChar, 1))
		return GHTTPFalse;
	if ((tempChar & 0x80) == 0x80)
	{
		int tempInt = 0;

		tempChar ^= 0x80;
		if (GHTTPFalse == ghiEncryptorReadNBOLength(data, &tempInt, tempChar))
			return GHTTPFalse;
		if (tempInt > (data->len - data->pos))
			return GHTTPFalse;

		*lenOut = tempInt;
		return GHTTPTrue;
	}
	else
	{
		if ((int)tempChar > (data->len - data->pos))
			return GHTTPFalse;

		*lenOut = tempChar;
		return GHTTPTrue;
	}
}

static void ghiEncryptorGenerateEncryptionKeys(gsSSL* sslInterface)
{
	

	SHA1Context sha1;
	MD5_CTX md5;
	unsigned char temp[SHA1HashSize];

	unsigned int randomSize = 32;
	unsigned char keyblock[64]; 
	
	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"A", 1);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->premastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->premastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final((unsigned char*)&sslInterface->mastersecret[0*GS_CRYPT_MD5_HASHSIZE], &md5);

	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"BB", 2);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->premastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->premastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final((unsigned char*)&sslInterface->mastersecret[1*GS_CRYPT_MD5_HASHSIZE], &md5);

	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"CCC", 3);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->premastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->premastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final((unsigned char*)&sslInterface->mastersecret[2*GS_CRYPT_MD5_HASHSIZE], &md5);

	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"A", 1);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final(&keyblock[0*GS_CRYPT_MD5_HASHSIZE], &md5);

	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"BB", 2);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final(&keyblock[1*GS_CRYPT_MD5_HASHSIZE], &md5);
	
	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"CCC", 3);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final(&keyblock[2*GS_CRYPT_MD5_HASHSIZE], &md5);

	SHA1Reset(&sha1);
	SHA1Input(&sha1, (const unsigned char*)"DDDD", 4);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->serverRandom, randomSize);
	SHA1Input(&sha1, (const unsigned char*)sslInterface->clientRandom, randomSize);
	SHA1Result(&sha1, temp);
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
	MD5Update(&md5, temp, GS_CRYPT_SHA1_HASHSIZE);
	MD5Final(&keyblock[3*GS_CRYPT_MD5_HASHSIZE], &md5);


	memcpy(sslInterface->clientWriteMACSecret, &keyblock[0],  16);
	memcpy(sslInterface->clientReadMACSecret,  &keyblock[16], 16);
	memcpy(sslInterface->clientWriteKey,       &keyblock[32], 16);
	memcpy(sslInterface->clientReadKey,        &keyblock[48], 16);

	sslInterface->clientWriteMACLen = 16;
	sslInterface->clientReadMACLen = 16;
	sslInterface->clientWriteKeyLen = 16;
	sslInterface->clientReadKeyLen = 16;

	RC4Init(&sslInterface->sendRC4, (const unsigned char*)sslInterface->clientWriteKey, sslInterface->clientWriteKeyLen);
	RC4Init(&sslInterface->recvRC4, (const unsigned char*)sslInterface->clientReadKey, sslInterface->clientReadKeyLen);

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
		"Generated SSL session keys\r\n");
}


GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	gsSSL* sslInterface = NULL;

	assert(theEncryptor->mInitialized == GHTTPFalse);
	assert(theEncryptor->mInterface == NULL);

	if (connection->sendBuffer.size - connection->sendBuffer.len < sizeof(gsSSLClientHelloMsg))
		return GHIEncryptionResult_BufferTooSmall;

	theEncryptor->mInterface = gsimalloc(sizeof(gsSSL));
	if (theEncryptor->mInterface == NULL)
	{
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Memory, GSIDebugLevel_WarmError,
			"Failed to allocate SSL interface (out of memory: %d bytes)\r\n", sizeof(gsSSL));
		return GHIEncryptionResult_Error;
	}
	memset(theEncryptor->mInterface, 0, sizeof(gsSSL));
	sslInterface = (gsSSL*)theEncryptor->mInterface;

	theEncryptor->mInitialized = GHTTPTrue;
	theEncryptor->mSessionStarted = GHTTPFalse;
	theEncryptor->mSessionEstablished = GHTTPFalse;
	theEncryptor->mEncryptOnBuffer = GHTTPTrue;
	theEncryptor->mEncryptOnSend = GHTTPFalse;
	theEncryptor->mLibSendsHandshakeMessages = GHTTPFalse;
	MD5Init(&sslInterface->finishHashMD5);
	SHA1Reset(&sslInterface->finishHashSHA1);

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
		"GameSpy SSL engine initialized\r\n");

	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslCleanupFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor)
{
	if (theEncryptor != NULL)
	{
		gsSSL* sslInterface = (gsSSL*)theEncryptor->mInterface;
		if (sslInterface != NULL)
		{
			gsifree(sslInterface);
			theEncryptor->mInterface = NULL;
		}
		theEncryptor->mInitialized = GHTTPFalse;
		theEncryptor->mSessionStarted = GHTTPFalse;
		theEncryptor->mSessionEstablished = GHTTPFalse;
	}

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
		"GameSpy SSL engine cleanup called\r\n");

	GSI_UNUSED(connection);

	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslStartFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	gsSSL* sslInterface = (gsSSL*)theEncryptor->mInterface;
	gsSSLClientHelloMsg helloMsg;
	int i=0;
	
	helloMsg.header.contentType  = GS_SSL_CONTENT_HANDSHAKE;
	helloMsg.header.versionMajor = GS_SSL_VERSION_MAJOR;
	helloMsg.header.versionMinor = GS_SSL_VERSION_MINOR;

	ghiEncryptorWriteNBOLength(helloMsg.header.lengthNBO, sizeof(gsSSLClientHelloMsg)-sizeof(gsSSLRecordHeaderMsg), 2);

	helloMsg.handshakeType = GS_SSL_HANDSHAKE_CLIENTHELLO;
	helloMsg.versionMajor  = GS_SSL_VERSION_MAJOR;
	helloMsg.versionMinor  = GS_SSL_VERSION_MINOR;

	ghiEncryptorWriteNBOLength(helloMsg.lengthNBO, sizeof(gsSSLClientHelloMsg)-sizeof(gsSSLRecordHeaderMsg)-4, 3);
	ghiEncryptorWriteNBOLength(helloMsg.time, 0, 4); 
	
	Util_RandSeed(current_time());
	for (i=0; i<28; i++)
	{
		#if defined(GS_CRYPT_NO_RANDOM)
			#pragma message ("!!!WARNING: SSL Random disable for debug purposes.  SSL not secured!!!")
			helloMsg.random[i] = 0; 
		#else
			helloMsg.random[i] = (unsigned char)Util_RandInt(0, 0xff);
		#endif
	}

	memcpy(&sslInterface->clientRandom[0], helloMsg.time, 4);
	memcpy(&sslInterface->clientRandom[4], helloMsg.random, 28);

	helloMsg.sessionIdLen = 0;

	helloMsg.cipherSuitesLength = htons(sizeof(gsi_u16)*GS_SSL_NUM_CIPHER_SUITES);
	for (i=0; i < GS_SSL_NUM_CIPHER_SUITES; i++)
		helloMsg.cipherSuites[i] = htons((unsigned short)gsSSLCipherSuites[i].mSuiteID);

	helloMsg.compressionMethodLen = 1;
	helloMsg.compressionMethodList = 0;

	MD5Update(&sslInterface->finishHashMD5, (unsigned char*)&helloMsg+sizeof(gsSSLRecordHeaderMsg), sizeof(gsSSLClientHelloMsg)-sizeof(gsSSLRecordHeaderMsg));
	SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)&helloMsg+sizeof(gsSSLRecordHeaderMsg), sizeof(gsSSLClientHelloMsg)-sizeof(gsSSLRecordHeaderMsg));

	if (GHTTPFalse == ghiAppendDataToBuffer(&connection->sendBuffer, (const char*)&helloMsg, sizeof(gsSSLClientHelloMsg)))
	{
		return GHIEncryptionResult_BufferTooSmall;
	}
	
	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
		"GameSpy SSL engine handshake started\r\n");

	theEncryptor->mSessionStarted = GHTTPTrue;
	return GHIEncryptionResult_Success;

}

GHIEncryptionResult ghiEncryptorSslEncryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * thePlainTextBuffer,
												 int          thePlainTextLength,
												 char *       theEncryptedBuffer,
												 int *        theEncryptedLength)
{
	if (theEncryptor != NULL)
	{
		gsSSL* sslInterface = (gsSSL*)theEncryptor->mInterface;
		if (sslInterface == NULL || theEncryptor->mSessionEstablished == GHTTPFalse)
		{
			if (thePlainTextLength > *theEncryptedLength)
				return GHIEncryptionResult_BufferTooSmall;
			memcpy(theEncryptedBuffer, thePlainTextBuffer, (size_t)thePlainTextLength);
			*theEncryptedLength += thePlainTextLength; 
		}
		else
		{

			gsSSL* sslInterface = (gsSSL*)theEncryptor->mInterface;
			gsSSLRecordHeaderMsg* header = NULL;
			MD5_CTX md5;
			int pos = 0;
			unsigned short lengthNBO = htons((unsigned short)thePlainTextLength);
			unsigned char MAC[GS_CRYPT_MD5_HASHSIZE];

			if (*theEncryptedLength < (thePlainTextLength+(int)sizeof(gsSSLRecordHeaderMsg)))
				return GHIEncryptionResult_BufferTooSmall;

			header = (gsSSLRecordHeaderMsg*)theEncryptedBuffer;
			header->contentType = GS_SSL_CONTENT_APPLICATIONDATA;
			header->versionMajor = GS_SSL_VERSION_MAJOR;
			header->versionMinor = GS_SSL_VERSION_MINOR;
			pos += sizeof(gsSSLRecordHeaderMsg);

			MD5Init(&md5);
			MD5Update(&md5, sslInterface->clientWriteMACSecret, (unsigned int)sslInterface->clientWriteMACLen);
			MD5Update(&md5, (unsigned char*)GS_SSL_PAD_ONE, GS_SSL_MD5_PAD_LEN);
			MD5Update(&md5, sslInterface->sendSeqNBO, sizeof(sslInterface->sendSeqNBO));
			MD5Update(&md5, (unsigned char*)"\x17", 1); 
			MD5Update(&md5,(unsigned char*)&lengthNBO, sizeof(lengthNBO));
			MD5Update(&md5, (unsigned char*)thePlainTextBuffer, (unsigned int)thePlainTextLength); 
			MD5Final(MAC, &md5); 

			MD5Init(&md5);
			MD5Update(&md5, sslInterface->clientWriteMACSecret, (unsigned int)sslInterface->clientWriteMACLen);
			MD5Update(&md5, (unsigned char*)GS_SSL_PAD_TWO, GS_SSL_MD5_PAD_LEN);
			MD5Update(&md5, MAC, GS_CRYPT_MD5_HASHSIZE);
			MD5Final(MAC, &md5); 

			RC4Encrypt(&sslInterface->sendRC4, (const unsigned char*)thePlainTextBuffer, (unsigned char*)&theEncryptedBuffer[pos], thePlainTextLength);
			pos += thePlainTextLength;
			RC4Encrypt(&sslInterface->sendRC4, MAC, (unsigned char*)&theEncryptedBuffer[pos], GS_CRYPT_MD5_HASHSIZE);
			pos += GS_CRYPT_MD5_HASHSIZE;

			ghiEncryptorWriteNBOLength(header->lengthNBO, (int)(pos - sizeof(gsSSLRecordHeaderMsg)), 2); 

			*theEncryptedLength -= pos;

			pos = 7; 
			do 
			{
				if (sslInterface->sendSeqNBO[pos] == 0xFF) 
				{
					sslInterface->sendSeqNBO[pos] = 0;
					pos -= 1;
				}
				else
				{
					sslInterface->sendSeqNBO[pos] += 1;
					pos = 0; 
				}
			} while(pos >= 0);
		}
	}

	GSI_UNUSED(connection);
	return GHIEncryptionResult_Success;
}

GHIEncryptionResult ghiEncryptorSslDecryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * theEncryptedBuffer,
												 int *        theEncryptedLength,
												 char *       theDecryptedBuffer,
												 int *        theDecryptedLength)
{
	gsSSL* sslInterface = NULL;
	int readPos = 0;
	int writePos = 0;

	assert(theEncryptor != NULL);
	assert(theEncryptor->mInterface != NULL);
	if (theEncryptor == NULL || theEncryptor->mInterface == NULL)
	{
		memcpy(theDecryptedBuffer, theEncryptedBuffer, (size_t)(*theEncryptedLength));
		*theDecryptedLength = *theEncryptedLength;
		*theEncryptedLength = 0; 
		return GHIEncryptionResult_Success;
	}

	sslInterface = (gsSSL*)theEncryptor->mInterface;
	if (sslInterface == NULL)
		return GHIEncryptionResult_Error;

	while(readPos < *theEncryptedLength)
	{
		gsSSLRecordHeaderMsg* header = NULL;
		unsigned short length = 0;
		GHIEncryptionResult result;

		if ((*theEncryptedLength-readPos) < sizeof(gsSSLRecordHeaderMsg))
			break;
		header = (gsSSLRecordHeaderMsg*)&theEncryptedBuffer[readPos];

		memcpy(&length, &header->lengthNBO[0], sizeof(length));
		length = ntohs(length);
		if ( *theEncryptedLength < (readPos + length +(int)sizeof(gsSSLRecordHeaderMsg)))
			break; 

		if (connection->encryptor.mSessionEstablished)
		{
			if ((*theDecryptedLength-writePos) < length)
			{
				*theEncryptedLength = readPos; 
				*theDecryptedLength = writePos; 

				if (*theDecryptedLength>0)
					return GHIEncryptionResult_Success;
				else
					return GHIEncryptionResult_BufferTooSmall;
			}
		}


		switch(header->contentType)
		{
		case GS_SSL_CONTENT_HANDSHAKE:
			{
				GHIBuffer data;

				readPos += sizeof(gsSSLRecordHeaderMsg);
				if (connection->encryptor.mSessionEstablished)
					RC4Encrypt(&sslInterface->recvRC4, (const unsigned char*)&theEncryptedBuffer[readPos], (unsigned char*)&theEncryptedBuffer[readPos], length);

				ghiInitReadOnlyBuffer(connection, &data, &theEncryptedBuffer[readPos], length);

				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
					"SSL handshake message received\r\n");

				result = ghiEncryptorProcessSSLHandshake(connection, theEncryptor, &data);
				if (result != GHIEncryptionResult_Success)
					return result; 

				break;
			}
		case GS_SSL_CONTENT_APPLICATIONDATA:
			{
				if ( (*theDecryptedLength-writePos) < length)
				{
				}

				readPos += sizeof(gsSSLRecordHeaderMsg);
				if (connection->encryptor.mSessionEstablished)
					RC4Encrypt(&sslInterface->recvRC4, (const unsigned char*)&theEncryptedBuffer[readPos], (unsigned char*)&theEncryptedBuffer[readPos], length);


				memcpy(theDecryptedBuffer+writePos, &theEncryptedBuffer[readPos], (size_t)(length - GS_CRYPT_MD5_HASHSIZE));
				writePos += length - GS_CRYPT_MD5_HASHSIZE;
				break;
			}
			
		case GS_SSL_CONTENT_CHANGECIPHERSPEC:
			readPos += sizeof(gsSSLRecordHeaderMsg);
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_Debug,
				"SSL change cipher spec message received\r\n");

			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_Notice,
				"SSL: Incoming traffic now encrypted\r\n");
			connection->encryptor.mSessionEstablished = GHTTPTrue;
			break;

		case GS_SSL_CONTENT_ALERT:
			readPos += sizeof(gsSSLRecordHeaderMsg);

			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_WarmError,
				"SSL received unhandled ALERT\r\n");
			break;

		default:
			readPos += sizeof(gsSSLRecordHeaderMsg);
			return GHIEncryptionResult_Error; 
		};

		readPos += length;
	};

	*theEncryptedLength = readPos; 
	*theDecryptedLength = writePos; 

	if (*theEncryptedLength < 0)
		return GHIEncryptionResult_Error;
	else
		return GHIEncryptionResult_Success;
}

static GHTTPBool ghiCertificateChainIsValid(gsSSL* sslInterface)
{
	GSI_UNUSED(sslInterface);

	return GHTTPTrue;
}

#define CHECK(a) { if (GHTTPFalse == a) return GHIEncryptionResult_Error; }


GHIEncryptionResult ghiEncryptorProcessSSLHandshake(struct GHIConnection * connection,
													  struct GHIEncryptor * encryptor,
													  GHIBuffer * data)
{
	gsSSL* sslInterface = (gsSSL*)encryptor->mInterface;

	while(data->pos < data->len)
	{
		int  messageStart = data->pos;
		char messageType = 0;
		CHECK(ghiReadDataFromBufferFixed(data, &messageType, 1));

		if (messageType == GS_SSL_HANDSHAKE_SERVERHELLO)
		{
			int totalMsgLen = 0; 
			int msgDataLen = 0;  
			int tempInt = 0;
			char tempChar = '\0';
			
			if (sslInterface->sessionLen != 0)
				return GHIEncryptionResult_Error; 

			CHECK(ghiEncryptorReadNBOLength(data, &msgDataLen, 3));

			if (msgDataLen > (data->len - data->pos))
				return GHIEncryptionResult_Error; 

			data->pos += 2;

			CHECK(ghiReadDataFromBufferFixed(data, (char*)&sslInterface->serverRandom[0], 32));

			CHECK(ghiReadDataFromBufferFixed(data, &tempChar, 1));
			CHECK(ghiReadDataFromBufferFixed(data, (char*)sslInterface->sessionData, tempChar));
			sslInterface->sessionLen = (int)tempChar;

			CHECK(ghiEncryptorReadNBOLength(data, &tempInt, 2));
			sslInterface->cipherSuite = (unsigned short)tempInt;

			CHECK(ghiReadDataFromBufferFixed(data, &tempChar, 1));
			if (tempChar != 0x00)
				return GHIEncryptionResult_Error;
						
			totalMsgLen = data->pos - messageStart;
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)&data->data[messageStart], (unsigned int)totalMsgLen);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)&data->data[messageStart], (unsigned int)totalMsgLen);
		}
		else if (messageType == GS_SSL_HANDSHAKE_CERTIFICATE)
		{
			int msgLength = 0;    
			int certListLen = 0;  
			int totalMsgLen = 0;  

			int certCount = 0;
			int certListEndPos = 0;

			CHECK(ghiEncryptorReadNBOLength(data, &msgLength, 3));
			CHECK(ghiEncryptorReadNBOLength(data, &certListLen, 3));
			if (msgLength != certListLen + 3)
				return GHIEncryptionResult_Error;

			
			certListEndPos = data->pos + certListLen;
			if (certListLen > (data->len - data->pos))
				return GHIEncryptionResult_Error;
						
			while(data->pos < certListEndPos)
			{
				int certLength = 0;
				int certStartPos = 0;

				int temp = 0;
				int version = 0;

				CHECK(ghiEncryptorReadNBOLength(data, &certLength, 3));

				if (certLength > (data->len - data->pos))
					return GHIEncryptionResult_Error; 

				if (certLength > 0xFFFF) 
					return GHIEncryptionResult_Error;
	
				certStartPos = data->pos; 
				certCount++;
								

				if (certCount == 1)
				{

					int seqLen = 0;
					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					
					if (5 > (data->len - data->pos)) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0xa0) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x03) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x02) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x01) return GHIEncryptionResult_Error;
					version = (unsigned char)data->data[data->pos++];

					if ((unsigned char)data->data[data->pos++] != 0x02) return GHIEncryptionResult_Error;
					temp = (unsigned char)data->data[data->pos++]; 
					if (data->pos + temp > certListEndPos) return GHIEncryptionResult_Error;
					data->pos += temp; 

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					data->pos += seqLen; 

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					data->pos += seqLen; 

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					data->pos += seqLen; 

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					data->pos += seqLen; 

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));
					if (seqLen != 0x0d) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x06) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x09) return GHIEncryptionResult_Error;
					if (0 != memcmp(&data->data[data->pos], gsSslRsaOid, sizeof(gsSslRsaOid)))
						return GHIEncryptionResult_Error; 
					data->pos += sizeof(gsSslRsaOid);
					if ((unsigned char)data->data[data->pos++] != 0x05) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x00) return GHIEncryptionResult_Error;

					if (2 > (certListEndPos - data->pos)) return GHIEncryptionResult_Error;
					if ((unsigned char)data->data[data->pos++] != 0x03) return GHIEncryptionResult_Error; 
					if ((unsigned char)data->data[data->pos++] != 0x81) return GHIEncryptionResult_Error; 
					if (temp > (certListEndPos - data->pos)) return GHIEncryptionResult_Error;
					temp = (unsigned char)data->data[data->pos++]; 

					if ((unsigned char)data->data[data->pos++] != 0x00) return GHIEncryptionResult_Error;

					CHECK(ghiEncryptorParseASN1Sequence(data, &seqLen));

					if (data->data[data->pos++] != 0x02) return GHIEncryptionResult_Error; 
					if ((data->data[data->pos]&0x80)==0x80) 
					{
						int lensize = data->data[data->pos++]&0x7f;
						if (lensize > 4)
							return GHIEncryptionResult_Error;
						temp = 0;
						while(lensize-- > 0)
							temp = (temp << 8) | (unsigned char)data->data[data->pos++];
					}
					else
					{
						temp = (unsigned char)data->data[data->pos++];
					}
					if (data->pos + temp > certListEndPos) return GHIEncryptionResult_Error;
					if (data->data[data->pos++] != 0x00) return GHIEncryptionResult_Error; 
					if (temp-1 > GS_LARGEINT_BINARY_SIZE/sizeof(char)) return GHIEncryptionResult_Error;
					sslInterface->serverpub.modulus.mLength = (unsigned int)((temp-1)/GS_LARGEINT_DIGIT_SIZE_BYTES); 
					gsLargeIntSetFromMemoryStream(&sslInterface->serverpub.modulus, (const gsi_u8*)&data->data[data->pos], (gsi_u32)temp-1);
					data->pos += temp-1;

					if (data->data[data->pos++] != 0x02) return GHIEncryptionResult_Error; 
					if ((data->data[data->pos]&0x80)==0x80)
					{
						int lensize = data->data[data->pos++]&0x7f;
						if (lensize > 4)
							return GHIEncryptionResult_Error;
						temp = 0;
						while(lensize-- > 0)
							temp = (temp << 8) | (unsigned char)data->data[data->pos++];
					}
					else
					{
						temp = (unsigned char)data->data[data->pos++];
					}
					if (data->pos + temp > certListEndPos) return GHIEncryptionResult_Error;
					if (temp == 0) return GHIEncryptionResult_Error; 
					if (temp > GS_LARGEINT_BINARY_SIZE/sizeof(char)) return GHIEncryptionResult_Error;
					sslInterface->serverpub.exponent.mLength = (unsigned int)(((temp-1)/GS_LARGEINT_DIGIT_SIZE_BYTES)+1); 
					gsLargeIntSetFromMemoryStream(&sslInterface->serverpub.exponent, (const gsi_u8*)&data->data[data->pos], (gsi_u32)temp);
					data->pos += temp;
				}
					
				data->pos = certStartPos + certLength;

				GSI_UNUSED(version); 
			}
			if (data->pos != certListEndPos)
				return GHIEncryptionResult_Error; 

			if  (GHTTPFalse == ghiCertificateChainIsValid(sslInterface))
				return GHIEncryptionResult_Error;

			totalMsgLen = data->pos - messageStart;
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)&data->data[messageStart], (unsigned int)totalMsgLen);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)&data->data[messageStart], (unsigned int)totalMsgLen);
		}
		else if (messageType == GS_SSL_HANDSHAKE_SERVERHELLODONE)
		{
			int i=0;

			gsSSLClientKeyExchangeMsg* clientKeyExchange = NULL;
			gsSSLRecordHeaderMsg* changeCipherSpec = NULL;
			gsSSLRecordHeaderMsg* finalHandshake = NULL;

			unsigned char temp[7];
			unsigned char hashTempMD5[GS_CRYPT_MD5_HASHSIZE];
			unsigned char hashTempSHA1[GS_CRYPT_SHA1_HASHSIZE];
			int tempInt = 0;

			CHECK(ghiEncryptorReadNBOLength(data, &tempInt, 3));
			if (tempInt != 0x00) return GHIEncryptionResult_Error;

			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)&data->data[messageStart], (unsigned int)(data->pos - messageStart));
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)&data->data[messageStart], (unsigned int)(data->pos - messageStart));

			tempInt = (int)(sizeof(gsSSLClientKeyExchangeMsg) + sslInterface->serverpub.modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES);
			while (connection->sendBuffer.size - connection->sendBuffer.len < tempInt)
			{
				if (GHTTPFalse == ghiResizeBuffer(&connection->sendBuffer, connection->sendBuffer.sizeIncrement))
					return GHIEncryptionResult_Error; 
			}
						
			sslInterface->premastersecret[0] = GS_SSL_VERSION_MAJOR;
			sslInterface->premastersecret[1] = GS_SSL_VERSION_MINOR;
			for (i=2; i<GS_SSL_MASTERSECRET_LEN; i++)
			{
				#if defined(GS_CRYPT_NO_RANDOM)
					#pragma message ("!!!WARNING: SSL Random disable for debug purposes.  SSL not secured!!!")
					sslInterface->premastersecret[i] = 0; 
				#else
					Util_RandSeed(current_time());
					sslInterface->premastersecret[i] = (unsigned char)(Util_RandInt(0, 0x0100)); 
				#endif
			}

			clientKeyExchange = (gsSSLClientKeyExchangeMsg*)&connection->sendBuffer.data[connection->sendBuffer.len];
			connection->sendBuffer.len += sizeof(gsSSLClientKeyExchangeMsg);
			clientKeyExchange->header.contentType = GS_SSL_CONTENT_HANDSHAKE;
			clientKeyExchange->header.versionMajor = GS_SSL_VERSION_MAJOR;
			clientKeyExchange->header.versionMinor = GS_SSL_VERSION_MINOR;
			ghiEncryptorWriteNBOLength(clientKeyExchange->header.lengthNBO, (int)(sslInterface->serverpub.modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES+4), 2);
			clientKeyExchange->handshakeType = GS_SSL_HANDSHAKE_CLIENTKEYEXCHANGE;
			ghiEncryptorWriteNBOLength(clientKeyExchange->lengthNBO, (int)(sslInterface->serverpub.modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES), 3);
			gsCryptRSAEncryptBuffer(&sslInterface->serverpub, sslInterface->premastersecret, 
				GS_SSL_MASTERSECRET_LEN, (unsigned char*)&connection->sendBuffer.data[connection->sendBuffer.len]);
			connection->sendBuffer.len += sslInterface->serverpub.modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES;
	
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)clientKeyExchange+sizeof(gsSSLRecordHeaderMsg), 
				sizeof(gsSSLClientKeyExchangeMsg) - sizeof(gsSSLRecordHeaderMsg) + 
				sslInterface->serverpub.modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)clientKeyExchange+sizeof(gsSSLRecordHeaderMsg), 
				sizeof(gsSSLClientKeyExchangeMsg) - sizeof(gsSSLRecordHeaderMsg) +
				sslInterface->serverpub.modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES);


			changeCipherSpec = (gsSSLRecordHeaderMsg*)&connection->sendBuffer.data[connection->sendBuffer.len];
			changeCipherSpec->contentType = GS_SSL_CONTENT_CHANGECIPHERSPEC;
			changeCipherSpec->versionMajor = GS_SSL_VERSION_MAJOR;
			changeCipherSpec->versionMinor = GS_SSL_VERSION_MINOR;
			changeCipherSpec->lengthNBO[0] = 0;
			changeCipherSpec->lengthNBO[1] = 1; 
			connection->sendBuffer.len += sizeof(gsSSLRecordHeaderMsg);
			connection->sendBuffer.data[connection->sendBuffer.len++] = 0x01; 

			ghiEncryptorGenerateEncryptionKeys(sslInterface);

			finalHandshake = (gsSSLRecordHeaderMsg*)&connection->sendBuffer.data[connection->sendBuffer.len];
			finalHandshake->contentType = GS_SSL_CONTENT_HANDSHAKE;
			finalHandshake->versionMajor = GS_SSL_VERSION_MAJOR;
			finalHandshake->versionMinor = GS_SSL_VERSION_MINOR;
			finalHandshake->lengthNBO[0] = 0;
			finalHandshake->lengthNBO[1] = 56; 
			connection->sendBuffer.len += sizeof(gsSSLRecordHeaderMsg);
			connection->sendBuffer.data[connection->sendBuffer.len++] = GS_SSL_HANDSHAKE_FINISHED;
			ghiEncryptorWriteNBOLength((unsigned char*)&connection->sendBuffer.data[connection->sendBuffer.len], 36, 3);
			connection->sendBuffer.len += 3;


			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)GS_SSL_CLIENT_FINISH_VALUE, 4);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)GS_SSL_PAD_ONE, GS_SSL_MD5_PAD_LEN);
			MD5Final(hashTempMD5, &sslInterface->finishHashMD5);

			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)GS_SSL_CLIENT_FINISH_VALUE, 4);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)GS_SSL_PAD_ONE, GS_SSL_SHA1_PAD_LEN);
			SHA1Result(&sslInterface->finishHashSHA1, hashTempSHA1);

			MD5Init(&sslInterface->finishHashMD5);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)GS_SSL_PAD_TWO, GS_SSL_MD5_PAD_LEN);
			MD5Update(&sslInterface->finishHashMD5, hashTempMD5, GS_CRYPT_MD5_HASHSIZE);
			MD5Final(hashTempMD5, &sslInterface->finishHashMD5);

			SHA1Reset(&sslInterface->finishHashSHA1);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)sslInterface->mastersecret, GS_SSL_MASTERSECRET_LEN);
			SHA1Input(&sslInterface->finishHashSHA1, (unsigned char*)GS_SSL_PAD_TWO, GS_SSL_SHA1_PAD_LEN);
			SHA1Input(&sslInterface->finishHashSHA1, hashTempSHA1, GS_CRYPT_SHA1_HASHSIZE);
			SHA1Result(&sslInterface->finishHashSHA1, hashTempSHA1);

			memcpy(&connection->sendBuffer.data[connection->sendBuffer.len], hashTempMD5, GS_CRYPT_MD5_HASHSIZE);
			connection->sendBuffer.len += GS_CRYPT_MD5_HASHSIZE;
			memcpy(&connection->sendBuffer.data[connection->sendBuffer.len], hashTempSHA1, GS_CRYPT_SHA1_HASHSIZE);
			connection->sendBuffer.len += GS_CRYPT_SHA1_HASHSIZE;
			
			MD5Init(&sslInterface->finishHashMD5);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)sslInterface->clientWriteMACSecret, GS_CRYPT_MD5_HASHSIZE);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)GS_SSL_PAD_ONE, GS_SSL_MD5_PAD_LEN);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)sslInterface->sendSeqNBO, 8);
			temp[0] = 0x16;
			temp[1] = (unsigned char)((GS_CRYPT_MD5_HASHSIZE+GS_CRYPT_SHA1_HASHSIZE+4)>>8);
			temp[2] = (unsigned char)((GS_CRYPT_MD5_HASHSIZE+GS_CRYPT_SHA1_HASHSIZE+4));
			temp[3] = 0x14; 
			temp[4] = 0x00; 
			temp[5] = 0x00; 
			temp[6] = 0x24; 
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)&temp, 7);
			MD5Update(&sslInterface->finishHashMD5, hashTempMD5, GS_CRYPT_MD5_HASHSIZE);   
			MD5Update(&sslInterface->finishHashMD5, hashTempSHA1, GS_CRYPT_SHA1_HASHSIZE); 
			MD5Final(hashTempMD5, &sslInterface->finishHashMD5);
			MD5Init(&sslInterface->finishHashMD5); 
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)sslInterface->clientWriteMACSecret, GS_CRYPT_MD5_HASHSIZE);
			MD5Update(&sslInterface->finishHashMD5, (unsigned char*)GS_SSL_PAD_TWO, GS_SSL_MD5_PAD_LEN);
			MD5Update(&sslInterface->finishHashMD5, hashTempMD5, GS_CRYPT_MD5_HASHSIZE);
			MD5Final(hashTempMD5, &sslInterface->finishHashMD5);
			
			memcpy(&connection->sendBuffer.data[connection->sendBuffer.len], hashTempMD5, GS_CRYPT_MD5_HASHSIZE);
			connection->sendBuffer.len += GS_CRYPT_MD5_HASHSIZE;

			memset(sslInterface->sendSeqNBO, 0, sizeof(sslInterface->sendSeqNBO));
			ghiEncryptorWriteNBOLength(&sslInterface->sendSeqNBO[4], 1, 4);
			
			RC4Encrypt(&sslInterface->sendRC4, 
				((unsigned char*)finalHandshake)+sizeof(gsSSLRecordHeaderMsg), 
				((unsigned char*)finalHandshake)+sizeof(gsSSLRecordHeaderMsg),
				56);
		}
		else if (messageType == GS_SSL_HANDSHAKE_FINISHED)
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_Notice,
				"SSL: todo - verify server finished hash\r\n");
			data->pos = data->len; 
		}
		else
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_WarmError,
				"SSL received unexpected handshake message type: %d\r\n", messageType);
			return GHIEncryptionResult_Error; 
		}
	}

	GSI_UNUSED(connection);

	if (data->pos == data->len)
		return GHIEncryptionResult_Success;
	else
		return GHIEncryptionResult_Error; 
}

GHIEncryptionResult ghiEncryptorSslEncryptSend(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * thePlainTextBuffer,
												 int          thePlainTextLength,
												 int *        theBytesSentOut)
{
	GS_FAIL(); 

	GSI_UNUSED(connection);
	GSI_UNUSED(theEncryptor);
	GSI_UNUSED(thePlainTextBuffer);
	GSI_UNUSED(thePlainTextLength);
	GSI_UNUSED(theBytesSentOut);
	
	return GHIEncryptionResult_Error;
}

GHIEncryptionResult ghiEncryptorSslDecryptRecv(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 char * theDecryptedBuffer,
												 int *  theDecryptedLength)
{
	GS_FAIL(); 

	GSI_UNUSED(connection);
	GSI_UNUSED(theEncryptor);
	GSI_UNUSED(theDecryptedBuffer);
	GSI_UNUSED(theDecryptedLength);

	return GHIEncryptionResult_Error;	
}

#endif 


