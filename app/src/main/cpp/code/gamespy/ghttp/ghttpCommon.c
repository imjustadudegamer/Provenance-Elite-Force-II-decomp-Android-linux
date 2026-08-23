
#include "ghttpCommon.h"

#if defined(_MSC_VER) 
#pragma warning ( disable: 4127 )
#endif 

#ifdef WIN32
typedef void * GLock;

static GLock ghiGlobalLock;
#endif

char * ghiProxyAddress;
unsigned short ghiProxyPort;

int ghiThrottleBufferSize = 125;
gsi_time ghiThrottleTimeDelay = 250;

extern int ghiNumConnections;


#ifdef WIN32
static GLock GNewLock(void)
{
	CRITICAL_SECTION * criticalSection;

	criticalSection = (CRITICAL_SECTION *)gsimalloc(sizeof(CRITICAL_SECTION));
	if(!criticalSection)
		return NULL;

	InitializeCriticalSection(criticalSection);

	return (GLock)criticalSection;
}

static void GFreeLock(GLock lock)
{
	CRITICAL_SECTION * criticalSection = (CRITICAL_SECTION *)lock;

	if(!lock)
		return;

	DeleteCriticalSection(criticalSection);

	gsifree(criticalSection);
}

static void GLockLock(GLock lock)
{
	CRITICAL_SECTION * criticalSection = (CRITICAL_SECTION *)lock;

	if(!lock)
		return;

	EnterCriticalSection(criticalSection);
}

static void GUnlockLock(GLock lock)
{
	CRITICAL_SECTION * criticalSection = (CRITICAL_SECTION *)lock;

	if(!lock)
		return;

	LeaveCriticalSection(criticalSection);
}
#endif

void ghiCreateLock(void)
{
#ifdef WIN32
	assert(!ghiGlobalLock);

	ghiGlobalLock = GNewLock();
#endif
}

void ghiFreeLock(void)
{
#ifdef WIN32
	if(!ghiGlobalLock)
		return;

	GFreeLock(ghiGlobalLock);
	ghiGlobalLock = NULL;
#endif
}

void ghiLock
(
	void
)
{
#ifdef WIN32
	if(!ghiGlobalLock)
		return;

	GLockLock(ghiGlobalLock);
#endif
}

void ghiUnlock
(
	void
)
{
#ifdef WIN32
	if(!ghiGlobalLock)
		return;

	GUnlockLock(ghiGlobalLock);
#endif
}

#ifdef HTTP_LOG
void ghiLogToFile(const char * buffer, int len, const char* fileName)
{
#ifdef _NITRO
	int i;

	if(!buffer || !len)
		return;

	for(i = 0 ; i < len ; i++)
		OS_PutChar(buffer[i]);
#else
	FILE * file;

	if(!buffer || !len)
		return;

	file = fopen(fileName, "ab");
	if(file)
	{
		fwrite(buffer, 1, len, file);
		fclose(file);
	}
#endif
}
#endif

GHTTPBool ghiDecryptReceivedData(struct GHIConnection * connection)
{
	GHIEncryptionResult aResult = GHIEncryptionResult_None;

	char* aReadPos  = NULL;
	char* aWritePos = NULL;
	int   aReadLen  = 0;
	int   aWriteLen = 0;

	do
	{
		do 
		{
			aReadPos  = connection->decodeBuffer.data + connection->decodeBuffer.pos;
			aReadLen  = connection->decodeBuffer.len  - connection->decodeBuffer.pos; 
			aWritePos = connection->recvBuffer.data + connection->recvBuffer.len;
			aWriteLen = connection->recvBuffer.size - connection->recvBuffer.len;    

			aResult = (connection->encryptor.mDecryptFunc)(connection, &connection->encryptor, 
				aReadPos, &aReadLen, aWritePos, &aWriteLen);
			if (aResult == GHIEncryptionResult_BufferTooSmall)
			{
				if (GHTTPFalse == ghiResizeBuffer(&connection->recvBuffer, connection->recvBuffer.sizeIncrement))
					return GHTTPFalse; 
			}
			else if(aResult == GHIEncryptionResult_Error)
			{
				return GHTTPFalse;
			}
		} while (aResult == GHIEncryptionResult_BufferTooSmall && aWriteLen == 0);

		if(aReadLen > connection->decodeBuffer.len)
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Misc, GSIDebugLevel_HotError,
				"ghiDecryptReceivedData read past the end of connection->decodeBuffer! (%d\\%d bytes)\r\n",
				aReadLen, connection->decodeBuffer.len);										  
			return GHTTPFalse;
		}

		connection->decodeBuffer.pos += aReadLen;
		connection->recvBuffer.len   += aWriteLen;

	} while(aWriteLen > 0);

	if (connection->decodeBuffer.pos > 0xFF)
	{
		int bytesToKeep = connection->decodeBuffer.len - connection->decodeBuffer.pos;
		if (bytesToKeep == 0)
			ghiResetBuffer(&connection->decodeBuffer);
		else
		{
			memmove(connection->decodeBuffer.data,
					connection->decodeBuffer.data + connection->decodeBuffer.pos,
					(size_t)bytesToKeep);
			connection->decodeBuffer.pos = 0;
			connection->decodeBuffer.len = bytesToKeep;
		}
	}

	return GHTTPTrue; 
}

GHIRecvResult ghiDoReceive
(
	GHIConnection * connection,
	char buffer[],
	int * bufferLen
)
{
	int rcode;
	int socketError;
	int len;

	len = (*bufferLen - 1);

	if(connection->throttle)
	{
		unsigned long now;

		now = current_time();
		if(now < (connection->lastThrottleRecv + ghiThrottleTimeDelay))
			return GHINoData;

		connection->lastThrottleRecv = (unsigned int)now;

		len = min(len, ghiThrottleBufferSize);
	}

	if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
		connection->encryptor.mSessionEstablished == GHTTPTrue &&
		connection->encryptor.mEncryptOnSend == GHTTPTrue )
	{
		GHIEncryptionResult result;
		int recvLength = len;
		
		result = ghiEncryptorSslDecryptRecv(connection, &connection->encryptor, buffer, &recvLength);
		if (result == GHIEncryptionResult_Success)
			rcode = recvLength;
		else
			rcode = -1; 
	}
	else
	{
		rcode = recv(connection->socket, buffer, len, 0);
	}
	

	if(gsiSocketIsError(rcode))
	{
		socketError = GOAGetLastError(connection->socket);

		if(socketError == WSAENOTCONN)
		{
			connection->connectionClosed = GHTTPTrue;
			return GHIConnClosed;
		}

		if((socketError == WSAEWOULDBLOCK) || (socketError == WSAEINPROGRESS) || (socketError == WSAETIMEDOUT))
			return GHINoData;

		connection->completed = GHTTPTrue;
		connection->result = GHTTPSocketFailed;
		connection->socketError = socketError;
		connection->connectionClosed = GHTTPTrue;

		return GHIError;
	}

	if(rcode == 0)
	{
		connection->connectionClosed = GHTTPTrue;
		return GHIConnClosed;
	}

	buffer[rcode] = '\0';
	*bufferLen = rcode;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump, "Received %d bytes\n", rcode);

	return GHIRecvData;
}

int ghiDoSend
(
	struct GHIConnection * connection,
	const char * buffer,
	int len
)
{
	int rcode;

	if (buffer == NULL || len == 0)
		return 0;
	
	if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
		connection->encryptor.mSessionEstablished == GHTTPTrue &&
		connection->encryptor.mEncryptOnSend == GHTTPTrue)
	{
		int bytesSent = 0;
		GHIEncryptionResult result;
		
		result = ghiEncryptorSslEncryptSend(connection, &connection->encryptor, buffer, len, &bytesSent);
	
		if(result != GHIEncryptionResult_Success)
			rcode = -1; 
		else
			rcode = bytesSent;
	}
	else
	{
		rcode = send(connection->socket, buffer, len, 0);
	}

	if(gsiSocketIsError(rcode))
	{
		int error;

		error = GOAGetLastError(connection->socket);
		if((error == WSAEWOULDBLOCK) || (error == WSAEINPROGRESS) || (error == WSAETIMEDOUT))
			return 0;

		connection->completed = GHTTPTrue;
		connection->result = GHTTPSocketFailed;
		connection->socketError = error;

		return -1;
	}

	if(connection->state == GHTTPPosting && connection->postingState.waitPostContinue == GHTTPFalse)
	{
		connection->postingState.bytesPosted += rcode;
		ghiLogRequest(buffer, rcode);
	}

	return rcode;
}

GHITrySendResult ghiTrySendThenBuffer
(
	GHIConnection * connection,
	const char * buffer,
	int len
)
{
	int rcode = 0;

	if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
		connection->encryptor.mSessionEstablished == GHTTPTrue &&
		connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
	{
		if (!ghiEncryptDataToBuffer(&connection->sendBuffer, buffer + rcode, len - rcode))
			return GHITrySendError;

		if (ghiSendBufferedData(connection) == GHTTPFalse)
			return GHITrySendError;
		if (connection->sendBuffer.pos >= connection->sendBuffer.len)
		{
			ghiResetBuffer(&connection->sendBuffer);
			return GHITrySendSent; 
		}
		return GHITrySendBuffered;
	}


	if(connection->sendBuffer.pos >= connection->sendBuffer.len)
	{
		rcode = ghiDoSend(connection, buffer, len);
		if(gsiSocketIsError(rcode))
			return GHITrySendError;

		if(rcode == len)
			return GHITrySendSent;
	}
	
	if(!ghiAppendDataToBuffer(&connection->sendBuffer, buffer + rcode, len - rcode))
		return GHITrySendError;
	return GHITrySendBuffered;
}

static GHTTPBool ghiParseProxyServer
(
	const char * server,
	char ** proxyAddress,       
	unsigned short * proxyPort  
)
{
	char * strPort;

	assert(server && *server);
	assert(proxyAddress && !*proxyAddress);
	assert(proxyPort);

	*proxyAddress = goastrdup(server);
	if(!*proxyAddress)
		return GHTTPFalse;

	if((strPort = strchr(*proxyAddress, ':')) != NULL)
	{
		*strPort++ = '\0';

		*proxyPort = (unsigned short)atoi(strPort);
		if(!*proxyPort)
		{
			gsifree(*proxyAddress);
			*proxyAddress = NULL;
			return GHTTPFalse;
		}
	}
	else
	{
		*proxyPort = GHI_DEFAULT_PORT;
	}
	return GHTTPTrue;
}

GHTTPBool ghiSetProxy
(
	const char * server
)
{
	if(ghiProxyAddress)
	{
		gsifree(ghiProxyAddress);
		ghiProxyAddress = NULL;
	}
	ghiProxyPort = 0;

	if(server && *server)
		return ghiParseProxyServer(server, &ghiProxyAddress, &ghiProxyPort);

	return GHTTPTrue;
}

GHTTPBool ghiSetRequestProxy
(
	GHTTPRequest request,
	const char * server
)
{
	GHIConnection* connection = ghiRequestToConnection(request);
	if (connection == NULL)
		return GHTTPFalse;

	if(connection->proxyOverrideServer)
	{
		gsifree(connection->proxyOverrideServer);
		connection->proxyOverrideServer = NULL;
		connection->proxyOverridePort = GHI_DEFAULT_PORT;
	}

	if(server && *server)
		return ghiParseProxyServer(server, &connection->proxyOverrideServer, &connection->proxyOverridePort);
	
	return GHTTPTrue;
}

void ghiThrottleSettings
(
	int bufferSize,
	gsi_time timeDelay
)
{
	ghiThrottleBufferSize = bufferSize;
	ghiThrottleTimeDelay = timeDelay;
}

#if defined(_MSC_VER)
#pragma warning ( default: 4127 )
#endif 

