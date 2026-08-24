/*
GameSpy GHTTP SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "ghttpProcess.h"
#include "ghttpCallbacks.h"
#include "ghttpPost.h"
#include "ghttpMain.h"
#include "ghttpCommon.h"

static GHTTPBool ghiParseURL
(
	GHIConnection * connection
)
{
	char * URL;
	int nIndex;
	char tempChar;
	char * str;

	assert(connection);
	if(!connection)
		return GHTTPFalse;

	assert(connection->URL);
	if(!connection->URL)
		return GHTTPFalse;

	URL = connection->URL;

	if(strncmp(URL, "http://", 7) == 0)
	{
		connection->protocol = GHIHttp;
		URL += 7;
	}
	else if (strncmp(URL, "https://", 8) == 0)
	{
		connection->protocol = GHIHttps;
		URL += 8;
	}
	else
	{
		return GHTTPFalse;
	}

	nIndex = (int)strcspn(URL, ":/");
	tempChar = URL[nIndex];
	URL[nIndex] = '\0';
	connection->serverAddress = goastrdup(URL);
	if(!connection->serverAddress)
		return GHTTPFalse;
	URL[nIndex] = tempChar;
	URL += nIndex;

	if(*URL == ':')
	{
		URL++;
		connection->serverPort = (unsigned short)atoi(URL);
		if(!connection->serverPort)
			return GHTTPFalse;
		do
		{
			URL++;
		}while(*URL && (*URL != '/'));
	}
	else
	{
		if (connection->protocol == GHIHttps)
			connection->serverPort = GHI_DEFAULT_SECURE_PORT;
		else
			connection->serverPort = GHI_DEFAULT_PORT;
	}

	if(!*URL)
		URL = "/";
	connection->requestPath = goastrdup(URL);
	while((str = strchr(connection->requestPath, ' ')) != NULL)
		*str = '+';
	if(!connection->requestPath)
		return GHTTPFalse;

	return GHTTPTrue;
}

void ghiDoSocketInit
(
	GHIConnection * connection
)
{
	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Socket Initialization\n");

	ghiCallProgressCallback(connection, NULL, 0);

	SocketStartUp();

	if(!ghiParseURL(connection))
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPParseURLFailed;
		return;
	}

	if((connection->protocol == GHIHttps) && (connection->encryptor.mEngine == GHTTPEncryptionEngine_None))
	{
		
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Encryption engine not set for HTTPS.  Using default engine\r\n");
		ghttpSetRequestEncryptionEngine(connection->request, GHTTPEncryptionEngine_Default);
	}
	else if ((connection->protocol != GHIHttps) && (connection->encryptor.mEngine != GHTTPEncryptionEngine_None))
	{
		ghttpSetRequestEncryptionEngine(connection->request, GHTTPEncryptionEngine_None);

		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Encryption engine set for unsecured URL. Removing encryption.\r\n");
	}
	
	if ((connection->protocol == GHIHttps) && connection->encryptor.mInitialized == GHTTPFalse)
	{
		GHIEncryptionResult aResult;
			
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Debug, "Initializing SSL engine\n");
		aResult = (connection->encryptor.mInitFunc)(connection, &connection->encryptor);
		if (aResult == GHIEncryptionResult_Error)
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_WarmError, "Failed to initialize SSL engine\n");
			connection->completed = GHTTPTrue;
			connection->result = GHTTPEncryptionError;
			return;
		}
	}

	connection->state = GHTTPHostLookup;
	ghiCallProgressCallback(connection, NULL, 0);
}

void ghiDoHostLookup
(
	GHIConnection * connection
)
{
	HOSTENT * host = NULL;
	const char * server = NULL;

#if !defined(GSI_NO_THREADS)
	if (connection->handle)
	{
		GSI_UNUSED(host);
		GSI_UNUSED(server);
		
		connection->state = GHTTPLookupPending;
		ghiCallProgressCallback(connection, NULL, 0);
		return;
	}
#endif

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Host Lookup\n");


	if (connection->proxyOverrideServer) 
		server = connection->proxyOverrideServer;
	else if(ghiProxyAddress)
		server = ghiProxyAddress;
	else
		server = connection->serverAddress;

	connection->serverIP = inet_addr(server);
	if(connection->serverIP == INADDR_NONE)
	{

#if defined(GSI_NO_THREADS)
		host = gethostbyname(server);

		if(host == NULL)
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_HotError, 
				"Host Lookup failed\n");
			connection->completed = GHTTPTrue;
			connection->result = GHTTPHostLookupFailed;
			return;
		}

		connection->serverIP = *(unsigned int *)host->h_addr_list[0];
#else
		
		if (gsiStartResolvingHostname(server, &(connection->handle)) == -1)
		{
			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_HotError, 
				"Thread Creation Failed\n");

			connection->handle = NULL;

			connection->completed = GHTTPTrue;
			connection->result = GHTTPHostLookupFailed;
			return;
		}
		else
		{
			GSI_UNUSED(host);
		}
#endif
	}


	if (connection->serverIP == INADDR_NONE)
	{
		connection->state = GHTTPLookupPending;
		ghiCallProgressCallback(connection, NULL, 0);
	}
	else
	{
		connection->state = GHTTPConnecting;
		ghiCallProgressCallback(connection, NULL, 0);
	}
}

void ghiDoLookupPending
(
	GHIConnection * connection
)
{
#if !defined(GSI_NO_THREADS)
	connection->serverIP = gsiGetResolvedIP(connection->handle);

	if (connection->serverIP == GSI_ERROR_RESOLVING_HOSTNAME)
	{
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_HotError, 
			"Error resolving hostname\n");

		connection->handle = NULL;

		connection->completed = GHTTPTrue;
		connection->result = GHTTPHostLookupFailed;
		return;
	}

	if (connection->serverIP == GSI_STILL_RESOLVING_HOSTNAME)
	{
		connection->state = GHTTPLookupPending;
		ghiCallProgressCallback(connection, NULL, 0);
	}
	else
	{
		connection->handle = NULL;
		
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, 
			"DNS lookup complete\n");
		connection->state = GHTTPConnecting;
		ghiCallProgressCallback(connection, NULL, 0);
	}
#endif
}

void ghiDoConnecting
(
	GHIConnection * connection
)
{
	int rcode;
	SOCKADDR_IN address;
	int writeFlag;
	int exceptFlag;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Connecting\n");

	if(connection->socket == INVALID_SOCKET)
	{
		connection->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(connection->socket == INVALID_SOCKET)
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPSocketFailed;
			connection->socketError = GOAGetLastError(connection->socket);
			return;
		}

		if(!SetSockBlocking(connection->socket, 0))
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPSocketFailed;
			connection->socketError = GOAGetLastError(connection->socket);
			return;
		}

		if(connection->throttle)
			SetReceiveBufferSize(connection->socket, ghiThrottleBufferSize);

		memset(&address, 0, sizeof(SOCKADDR_IN));
		address.sin_family = AF_INET;
		if (connection->proxyOverrideServer)
			address.sin_port = htons(connection->proxyOverridePort);
		else if(ghiProxyAddress)
			address.sin_port = htons(ghiProxyPort);
		else
			address.sin_port = htons(connection->serverPort);
		address.sin_addr.s_addr = connection->serverIP;

		rcode = connect(connection->socket, (SOCKADDR *)&address, sizeof(address));
		if(gsiSocketIsError(rcode))
		{
			int socketError = GOAGetLastError(connection->socket);
			if((socketError != WSAEWOULDBLOCK) && (socketError != WSAEINPROGRESS) && (socketError != WSAETIMEDOUT))
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPConnectFailed;
				connection->socketError = socketError;
				return;
			}
		}
	}

	rcode = GSISocketSelect(connection->socket, NULL, &writeFlag, &exceptFlag);
	if((gsiSocketIsError(rcode)) || ((rcode == 1) && exceptFlag))
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPConnectFailed;
		if(gsiSocketIsError(rcode))
			connection->socketError = GOAGetLastError(connection->socket);
		else
			connection->socketError = 0;
		return;
	}

	if((rcode == 1) && writeFlag)
	{
		if (connection->encryptor.mEngine == GHTTPEncryptionEngine_None)
			connection->state = GHTTPSendingRequest;
		else
			connection->state = GHTTPSecuringSession;
		ghiCallProgressCallback(connection, NULL, 0);
	}
} 

void ghiDoSecuringSession
(
	GHIConnection * connection
)
{


	GHIRecvResult result;
	
	char buffer[1025];
	int bufferLen;

	if (connection->encryptor.mSessionStarted == GHTTPFalse)
	{
		GHIEncryptionResult aResult;
		
		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Securing Session\n");

		GS_ASSERT(connection->encryptor.mStartFunc != NULL);
		if (connection->encryptor.mStartFunc != NULL)
		{
			aResult = (connection->encryptor.mStartFunc)(connection, &connection->encryptor);
			if (aResult == GHIEncryptionResult_Error)
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}
		}
		
		if (connection->encryptor.mSessionEstablished)
		{
			connection->state = GHTTPSendingRequest;
			ghiCallProgressCallback(connection, NULL, 0);
			return;
		}
	}
	
	GS_ASSERT(connection->encryptor.mSessionEstablished == GHTTPFalse);
	if (connection->encryptor.mLibSendsHandshakeMessages)
	{
		GS_ASSERT(connection->encryptor.mStartFunc != NULL);
		if (connection->encryptor.mStartFunc != NULL)
		{
			GHIEncryptionResult aResult = (connection->encryptor.mStartFunc)(connection, &connection->encryptor);
			if (aResult == GHIEncryptionResult_Error)
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}
		}
		
		if (connection->encryptor.mSessionEstablished)
		{
			connection->state = GHTTPSendingRequest;
			ghiCallProgressCallback(connection, NULL, 0);
		}
	}
	else
	{
		if (connection->sendBuffer.pos < connection->sendBuffer.len)
		{
			if (!ghiSendBufferedData(connection))
				return; 

			if(connection->sendBuffer.pos < connection->sendBuffer.len)
				return;

			ghiResetBuffer(&connection->sendBuffer);
		}

		bufferLen = sizeof(buffer);
		result = ghiDoReceive(connection, buffer, &bufferLen);
		
		if((result == GHIError) || (result == GHIConnClosed))
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPEncryptionError;
			return;
		}

		if(result == GHIRecvData)
		{
			if (!ghiAppendDataToBuffer(&connection->decodeBuffer, buffer, bufferLen))
				return;

			if (!ghiDecryptReceivedData(connection))
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}

			if (connection->encryptor.mSessionEstablished)
			{
				connection->state = GHTTPSendingRequest;
				ghiCallProgressCallback(connection, NULL, 0);
				return;
			}
		}
	}
}


void ghiDoSendingRequest
(
	GHIConnection * connection
)
{
	char * requestType;
	int oldPos;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Sending Request\n");
	
	if(!connection->sendBuffer.len)
	{
		GHIBuffer* writeBuffer = NULL;
		if (connection->encryptor.mEngine == GHTTPEncryptionEngine_None ||
			connection->encryptor.mEncryptOnBuffer == GHTTPFalse)
		{
			writeBuffer = &connection->sendBuffer;
		}
		else
		{
			writeBuffer = &connection->encodeBuffer;
		}

		if(connection->post && !connection->postingState.completed)
			requestType = "POST ";
		else if(connection->type == GHIHEAD)
			requestType = "HEAD ";
		else
			requestType = "GET ";
		ghiAppendDataToBuffer(writeBuffer, requestType, 0);
		if (connection->proxyOverrideServer || ghiProxyAddress)
			ghiAppendDataToBuffer(writeBuffer, connection->URL, 0);
		else
			ghiAppendDataToBuffer(writeBuffer, connection->requestPath, 0);
		ghiAppendDataToBuffer(writeBuffer, " HTTP/1.1" CRLF, 0);

		if(connection->serverPort == GHI_DEFAULT_PORT)
		{
			ghiAppendHeaderToBuffer(writeBuffer, "Host", connection->serverAddress);
		}
		else
		{
			ghiAppendDataToBuffer(writeBuffer, "Host: ", 0);
			ghiAppendDataToBuffer(writeBuffer, connection->serverAddress, 0);
			ghiAppendCharToBuffer(writeBuffer, ':');
			ghiAppendIntToBuffer(writeBuffer, connection->serverPort);
			ghiAppendDataToBuffer(writeBuffer, CRLF, 2);
		}

		if (connection->sendHeaders == NULL || strstr(connection->sendHeaders, "User-Agent")==NULL)
			ghiAppendHeaderToBuffer(writeBuffer, "User-Agent", "GameSpyHTTP/1.0");
		
		if (connection->persistConnection)
			ghiAppendHeaderToBuffer(writeBuffer, "Connection", "Keep-Alive");
		else
			ghiAppendHeaderToBuffer(writeBuffer, "Connection", "close");

		if(connection->post && !connection->postingState.completed)
		{
			char buf[16];

			sprintf(buf, "%d", connection->postingState.totalBytes);
			ghiAppendHeaderToBuffer(writeBuffer, "Content-Length", buf);

			ghiAppendHeaderToBuffer(writeBuffer, "Content-Type", ghiPostGetContentType(connection));
		}


		if(connection->sendHeaders)
			ghiAppendDataToBuffer(writeBuffer, connection->sendHeaders, 0);

		ghiAppendDataToBuffer(writeBuffer, CRLF, 2);

		if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
			connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
		{
			GS_ASSERT(writeBuffer == &connection->encodeBuffer);
			if (!ghiEncryptDataToBuffer(&connection->sendBuffer, writeBuffer->data, writeBuffer->len))
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}
			ghiResetBuffer(writeBuffer);
		}
	}

	oldPos = connection->sendBuffer.pos;

	if(!ghiSendBufferedData(connection))
		return;
	
	#ifdef HTTP_LOG
	if(connection->sendBuffer.pos != oldPos)
		ghiLogRequest(connection->sendBuffer.data + oldPos, connection->sendBuffer.pos - oldPos);
	#endif

	if(connection->sendBuffer.pos < connection->sendBuffer.len)
		return;

	ghiResetBuffer(&connection->sendBuffer);

	if(connection->post && !connection->postingState.completed)
		connection->state = GHTTPPosting;
	else
		connection->state = GHTTPWaiting;
	ghiCallProgressCallback(connection, NULL, 0);

	GSI_UNUSED(oldPos);
}

void ghiDoPosting
(
	GHIConnection * connection
)
{
	GHIPostingResult result;
	int oldBytesPosted;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Posting\n");

	oldBytesPosted = connection->postingState.bytesPosted;

	result = ghiPostDoPosting(connection);

	if(result == GHIPostingError)
	{
		int rcode = 0;
		int readFlag = 0;

		assert(connection->completed && connection->result);

		ghiPostCleanupState(connection);

		rcode = GSISocketSelect(connection->socket, &readFlag, NULL, NULL);
		if((rcode == 1) && readFlag)
		{
			connection->state = GHTTPReceivingStatus;
			ghiCallProgressCallback(connection, NULL, 0);
		}
		return;
	}

	if (result == GHIPostingWaitForContinue)
	{
		connection->postingState.waitPostContinue = GHTTPFalse;
		return;

	}

	if(oldBytesPosted != connection->postingState.bytesPosted)
		ghiCallPostCallback(connection);

	if(result == GHIPostingDone)
	{
		ghiPostCleanupState(connection);
		connection->postingState.completed = GHTTPTrue;

		connection->state = GHTTPWaiting;
		ghiCallProgressCallback(connection, NULL, 0);

		return;
	}
}

void ghiDoWaiting
(
	GHIConnection * connection
)
{
	int readFlag;
	int exceptFlag;
	int rcode;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Waiting\n");

	rcode = GSISocketSelect(connection->socket, &readFlag, NULL, &exceptFlag);
	if((gsiSocketIsError(rcode)) || ((rcode == 1) && exceptFlag))
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPSocketFailed;
		if(gsiSocketIsError(rcode))
			connection->socketError = GOAGetLastError(connection->socket);
		else
			connection->socketError = 0;
		return;
	}

	if((rcode == 1) && readFlag)
	{
		connection->state = GHTTPReceivingStatus;
		ghiCallProgressCallback(connection, NULL, 0);
	}
}

static GHTTPBool ghiParseStatus
(
	GHIConnection * connection
)
{
	int majorVersion;
	int minorVersion;
	int statusCode;
	int statusStringIndex;
	int rcode;
	char c;

	GS_ASSERT(connection);
	GS_ASSERT(connection->recvBuffer.len > 0);

#if defined(_X360)
	{
	int oldPrintCountValue = _set_printf_count_output(1);
#endif

	rcode = sscanf(connection->recvBuffer.data, "HTTP/%d.%d %d%n",
		&majorVersion,
		&minorVersion,
		&statusCode,
		&statusStringIndex);

#if defined(_X360)
	_set_printf_count_output(oldPrintCountValue);
	}
#endif

	if((rcode != 3) ||     
		(majorVersion < 1) ||  
		(statusCode < 100) ||        
		(statusCode >= 600))         
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPBadResponse;
		return GHTTPFalse;
	}

	while((c = connection->recvBuffer.data[statusStringIndex]) != '\0' && isspace(c))
		statusStringIndex++;

	connection->statusMajorVersion = majorVersion;
	connection->statusMinorVersion = minorVersion;
	connection->statusCode = statusCode;
	connection->statusStringIndex = statusStringIndex;

	return GHTTPTrue;
}

void ghiDoReceivingStatus
(
	GHIConnection * connection
)
{
	char buffer[1024];
	int bufferLen;
	GHIRecvResult result;
	char * endOfStatus;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Receiving Status\n");

	bufferLen = sizeof(buffer);
	result = ghiDoReceive(connection, buffer, &bufferLen);

	if(result == GHIError)
		return;
	if(result == GHINoData)
		return;

	if(result == GHIRecvData)
	{
		if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
			connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
		{
			if (!ghiAppendDataToBuffer(&connection->decodeBuffer, buffer, bufferLen))
				return;

			if (!ghiDecryptReceivedData(connection))
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}
		}
		else
		{			
			if(!ghiAppendDataToBuffer(&connection->recvBuffer, buffer, bufferLen))
				return;
		}
	}

	endOfStatus = strstr(connection->recvBuffer.data, CRLF);
	if(endOfStatus)
	{
		int statusLength;

		*endOfStatus = '\0';

		statusLength = (endOfStatus - connection->recvBuffer.data);

		ghiLogResponse(connection->recvBuffer.data, statusLength);
		ghiLogResponse("\n", 1);

		if(!ghiParseStatus(connection))
			return;

		connection->headerStringIndex = (statusLength + 2);


		if (connection->statusCode == 100 && connection->postingState.waitPostContinue)
		{
			connection->postingState.waitPostContinue = GHTTPFalse;
			ghiResetBuffer(&connection->recvBuffer); 
			connection->state = GHTTPPosting;
			ghiCallProgressCallback(connection, NULL, 0);

			gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_Comment,
				"Got HTTP continue\r\n");
		}
		else
		{
			connection->state = GHTTPReceivingHeaders;
			ghiCallProgressCallback(connection, NULL, 0);
		}
	}
	else if(result == GHIConnClosed)
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPBadResponse;
		connection->socketError = GOAGetLastError(connection->socket);
		return;
	}
}

static GHTTPBool ghiDeliverIncomingFileData
(
	GHIConnection * connection,
	char * data,
	int len
)
{
	char * buffer = NULL;
	int bufferLen = 0;

	connection->fileBytesReceived += len;

	if(connection->fileBytesReceived == connection->totalSize || connection->connectionClosed)
		connection->completed = GHTTPTrue;

	if(connection->type == GHIGET)
	{
		if(!ghiAppendDataToBuffer(&connection->getFileBuffer, data, len))
			return GHTTPFalse;

		buffer = connection->getFileBuffer.data;
		bufferLen = connection->getFileBuffer.len;
	}
	else if(connection->type == GHISAVE)
	{
		int bytesWritten = 0;
#ifndef NOFILE
		bytesWritten = fwrite(data, 1, len, connection->saveFile);
#endif
		if(bytesWritten != len)
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPFileWriteFailed;
			return GHTTPFalse;
		}

		buffer = data;
		bufferLen = len;
	}
	else if(connection->type == GHISTREAM)
	{
		buffer = data;
		bufferLen = len;
	}

	ghiCallProgressCallback(connection, buffer, bufferLen);

	return GHTTPTrue;
}

static int ghiParseChunkSize
(
	GHIConnection * connection
)
{
	char * header;
	int len;
	int num;
	int rcode;

	header = connection->chunkHeader;
	len = connection->chunkHeaderLen;

	assert(len);
	GSI_UNUSED(len);

	rcode = sscanf(header, "%x", &num);
	if(rcode != 1)
		return -1;

	return num;
}

static void ghiAppendToChunkHeaderBuffer
(
	GHIConnection * connection,
	char * data,
	int len
)
{
	assert(connection);
	assert(data);
	assert(len >= 0);

	if(len == 0)
		return;

	if(connection->chunkHeaderLen < CHUNK_HEADER_SIZE)
	{
		int numBytes;

		numBytes = min(CHUNK_HEADER_SIZE - connection->chunkHeaderLen, len);
		
		memcpy(connection->chunkHeader + connection->chunkHeaderLen, data, (unsigned int)numBytes);

		connection->chunkHeaderLen += numBytes;
		connection->chunkHeader[connection->chunkHeaderLen] = '\0';
	}
}

static GHTTPBool ghiProcessIncomingFileData
(
	GHIConnection * connection,
	char * data,
	int len
)
{
	assert(connection);
	assert(data);
	assert(len > 0);

	if(connection->chunkedTransfer)
	{
		while(len > 0)
		{
			if(connection->chunkReadingState == CRHeader)
			{
				char * endOfHeader;

				endOfHeader = strchr(data, 0xA);
				if(endOfHeader)
				{
					ghiAppendToChunkHeaderBuffer(connection, data, endOfHeader - data);

					endOfHeader++;
					len -= (endOfHeader - data);
					data = endOfHeader;

					connection->chunkBytesLeft = ghiParseChunkSize(connection);
					if(connection->chunkBytesLeft == -1)
					{
						connection->completed = GHTTPTrue;
						connection->result = GHTTPBadResponse;
						return GHTTPFalse;
					}

					if(connection->chunkBytesLeft == 0)
					{
						connection->chunkReadingState = CRFooter;
						gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump,
							"Reading footer\n");
					}
					else
					{
						connection->chunkReadingState = CRChunk;
						gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump,
							"Reading %d byte chunk\n", connection->chunkBytesLeft);
					}
				}
				else
				{
					ghiAppendToChunkHeaderBuffer(connection, data, len);

					return GHTTPTrue;
				}
			}
			else if(connection->chunkReadingState == CRChunk)
			{
				int numBytes;

				numBytes = min(connection->chunkBytesLeft, len);

				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump,
					"Read %d bytes of chunk\n", numBytes);

				if(!ghiDeliverIncomingFileData(connection, data, numBytes))
					return GHTTPFalse;

				data += numBytes;
				len -= numBytes;

				connection->chunkBytesLeft -= numBytes;

				if(connection->chunkBytesLeft == 0)
					connection->chunkReadingState = CRCRLF;
			}
			else if(connection->chunkReadingState == CRCRLF)
			{
				char * endOfFooter;

				endOfFooter = strchr(data, 0xA);

				if(!endOfFooter)
					return GHTTPTrue;

				endOfFooter++;
				len -= (endOfFooter - data);
				data = endOfFooter;

				connection->chunkHeader[0] = '\0';
				connection->chunkHeaderLen = 0;
				connection->chunkBytesLeft = 0;
				connection->chunkReadingState = CRHeader;

				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump,
					"Read chunk footer\n");
			}
			else if(connection->chunkReadingState == CRFooter)
			{
				connection->completed = GHTTPTrue;

				gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump,
					"Finished reading chunks\n");

				return GHTTPTrue;
			}
			else
			{
				assert(0);
				return GHTTPFalse;
			}
		}

		return GHTTPTrue;
	}

	return ghiDeliverIncomingFileData(connection, data, len);
}

void ghiDoReceivingHeaders
(
	GHIConnection * connection
)
{
	char buffer[4096];
	int bufferLen;
	GHIRecvResult result;
	GHTTPBool hasHeaders = GHTTPTrue;
	char * headers;
	char * endOfHeaders = NULL;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Receiving Headers\n");

	bufferLen = sizeof(buffer);
	result = ghiDoReceive(connection, buffer, &bufferLen);

	if(result == GHIError)
		return;
	if(result == GHINoData)
		return;

	if(result == GHIRecvData)
	{
		if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
			connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
		{
			if (!ghiAppendDataToBuffer(&connection->decodeBuffer, buffer, bufferLen))
				return;

			if (!ghiDecryptReceivedData(connection))
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}
		}
		else
		{
			if(!ghiAppendDataToBuffer(&connection->recvBuffer, buffer, bufferLen))
				return;
		}
	}

	headers = (connection->recvBuffer.data + connection->headerStringIndex);

	if( ((connection->statusCode / 100) == 1) &&
		 (strncmp(headers, "\r\n", 2) == 0 || strncmp(headers, "\xA\xA", 2) == 0)
		 )
	{
		endOfHeaders = headers;
		hasHeaders = GHTTPFalse;
	}
	else
	{
		endOfHeaders = strstr(headers, CRLF CRLF);
	}
	if(!endOfHeaders)
	{
		endOfHeaders = strstr(headers, "\xA\xA"); 
	}
	if(endOfHeaders)
	{
		char * fileStart;
		int fileLength;
		char * contentLength;

#ifdef HTTP_LOG
		int headersLength;
#endif

		if (GHTTPTrue == hasHeaders)
			endOfHeaders += 2;
		*endOfHeaders = '\0';

#ifdef HTTP_LOG
		headersLength = (endOfHeaders - headers);
#endif

		fileStart = (endOfHeaders + 2);
		fileLength = (connection->recvBuffer.len - (fileStart - connection->recvBuffer.data));

		connection->recvBuffer.len = (endOfHeaders - connection->recvBuffer.data + 1);
		connection->recvBuffer.pos = connection->recvBuffer.len;

#ifdef HTTP_LOG
		ghiLogResponse(headers, headersLength);
		ghiLogResponse("\n", 1);
#endif

		if((connection->statusCode / 100) == 1)
		{
			if(fileLength)
			{
				memmove(connection->recvBuffer.data, fileStart, (unsigned int)fileLength + 1);
				connection->recvBuffer.len = fileLength;
			}
			else
			{
				ghiResetBuffer(&connection->recvBuffer);
			}

			if (connection->postingState.waitPostContinue)
			{
				connection->postingState.waitPostContinue = GHTTPFalse;
				connection->state = GHTTPPosting;
				ghiCallProgressCallback(connection, NULL, 0);
			}

			connection->state = GHTTPReceivingStatus;
			ghiCallProgressCallback(connection, NULL, 0);

			return;
		}

		if((connection->statusCode / 100) == 3)
		{
			char * location;

			if(connection->redirectCount > 10)
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPFileNotFound;
				return;
			}

			location = strstr(headers, "Location:");
			if(location)
			{
				char * end;

				location += 9;
				while(isspace(*location))
					location++;

				for(end = location; *end && !isspace(*end) ; end++)  { };
				*end = '\0';

				if(*location == '/')
				{
					int len;

					len = (int)(strlen(connection->serverAddress) + 13 + strlen(location) + 1);
					connection->redirectURL = (char *)gsimalloc((unsigned int)len);
					if(!connection->redirectURL)
					{
						connection->completed = GHTTPTrue;
						connection->result = GHTTPOutOfMemory;
					}
					sprintf(connection->redirectURL, "http://%s:%d%s", connection->serverAddress, connection->serverPort, location);
				}
				else
				{
					connection->redirectURL = goastrdup(location);
					if(!connection->redirectURL)
					{
						connection->completed = GHTTPTrue;
						connection->result = GHTTPOutOfMemory;
					}
				}

				return;
			}
		}

		contentLength = strstr(headers, "Content-Length:");
		if(contentLength)
		{
#if (GSI_MAX_INTEGRAL_BITS >= 64)
			char  szMaxSize[] = "9223372036854775807"; 
#else
			char  szMaxSize[] = "2147483647";          
#endif
			char* pStart  = contentLength+16;
			char* pEnd    = pStart;
			int   nMaxLen = (int)strlen(szMaxSize);

			while( pEnd && *pEnd != '\0' && *pEnd != '\n' && *pEnd != '\r' && *pEnd != ' ' )
				pEnd++;

 			if( pEnd-pStart > nMaxLen )
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPFileToBig;
				return;
			}
			else
			if( pEnd-pStart == nMaxLen )
			{
				if( strncmp(pStart,szMaxSize,(unsigned int)(pEnd-pStart)) >= 0 )
				{
					connection->completed = GHTTPTrue;
					connection->result = GHTTPFileToBig;
					return;
				}
			}

#if (GSI_MAX_INTEGRAL_BITS >= 64)
			connection->totalSize = _atoi64(pStart);
#else
			connection->totalSize = atoi(pStart);
#endif
		}

		connection->chunkedTransfer = (strstr(headers, "Transfer-Encoding: chunked") != NULL)?GHTTPTrue:GHTTPFalse;
		if(connection->chunkedTransfer)
		{
			connection->chunkHeader[0] = '\0';
			connection->chunkHeaderLen = 0;
			connection->chunkBytesLeft = 0;
			connection->chunkReadingState = CRHeader;
		}

		if((connection->type == GHIHEAD) || (connection->type == GHIPOST))
		{
			connection->completed = GHTTPTrue;
			return;
		}

		connection->state = GHTTPReceivingFile;

		if(contentLength && !connection->totalSize)
		{
			connection->completed = GHTTPTrue;
			return;
		}

		if(fileLength > 0)
			ghiProcessIncomingFileData(connection, fileStart, fileLength);

	}
	else if(result == GHIConnClosed)
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPBadResponse;
		connection->socketError = GOAGetLastError(connection->socket);
	}
}

void ghiDoReceivingFile
(
	GHIConnection * connection
)
{
	char buffer[8192];
	int bufferLen;
	GHIRecvResult result;
	gsi_time start_time   = current_time();
	gsi_time running_time = 0;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Receiving File\n");

	while(!connection->completed && (running_time < connection->maxRecvTime))
	{
		bufferLen = sizeof(buffer);
		result = ghiDoReceive(connection, buffer, &bufferLen);

		if(result == GHIError)
			return;
		if(result == GHINoData)
			return;
		if(result == GHIConnClosed)
		{
			connection->completed = GHTTPTrue;

			if (connection->totalSize > 0 && connection->fileBytesReceived < connection->totalSize)
				connection->result = GHTTPFileIncomplete;
			return;
		}

		if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None &&
			connection->encryptor.mEncryptOnBuffer == GHTTPTrue)
		{
			char * decryptedData;
			int decryptedLen;

			if (!ghiAppendDataToBuffer(&connection->decodeBuffer, buffer, bufferLen))
				return;

			connection->recvBuffer.len = connection->recvBuffer.pos;

			if (!ghiDecryptReceivedData(connection))
			{
				connection->completed = GHTTPTrue;
				connection->result = GHTTPEncryptionError;
				return;
			}

			decryptedLen = (connection->recvBuffer.len - connection->recvBuffer.pos);
			if(decryptedLen)
			{
				decryptedData = (connection->recvBuffer.data + connection->recvBuffer.pos);
				if(!ghiProcessIncomingFileData(connection, decryptedData, decryptedLen))
					return;
			}
		}
		else
		{
			if(!ghiProcessIncomingFileData(connection, buffer, bufferLen))
				return;
		}

		running_time = current_time() - start_time;
	}
}
