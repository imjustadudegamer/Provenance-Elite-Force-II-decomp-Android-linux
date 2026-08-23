
#include "ghttpConnection.h"
#include "ghttpCommon.h"

#define CONNECTIONS_CHUNK_LEN      4

static GHIConnection ** ghiConnections;
static int ghiConnectionsLen;
static int ghiNumConnections;
static int ghiNextUniqueID;

static int ghiFindFreeSlot
(
	void
)
{
	int i;
	GHIConnection ** tempPtr;
	int oldLen;
	int newLen;

	for(i = 0 ; i < ghiConnectionsLen ; i++)
	{
		if(!ghiConnections[i]->inUse)
			return i;
	}

	assert(ghiNumConnections == ghiConnectionsLen);

	oldLen = ghiConnectionsLen;
	newLen = (ghiConnectionsLen + CONNECTIONS_CHUNK_LEN);
	tempPtr = (GHIConnection **)gsirealloc(ghiConnections, sizeof(GHIConnection *) * newLen);
	if(!tempPtr)
		return -1;
	ghiConnections = tempPtr;

	for(i = oldLen ; i < newLen ; i++)
	{
		ghiConnections[i] = (GHIConnection *)gsimalloc(sizeof(GHIConnection));
		if(!ghiConnections[i])
		{
			for(i-- ; i >= oldLen ; i--)
				gsifree(ghiConnections[i]);
			return -1;
		}
		ghiConnections[i]->inUse = GHTTPFalse;
	}

	ghiConnectionsLen = newLen;

	return oldLen;
}

GHIConnection * ghiNewConnection
(
	void
)
{
	int slot;
	GHIConnection * connection;
	GHTTPBool bResult;

	ghiLock();

	slot = ghiFindFreeSlot();
	if(slot == -1)
	{
		ghiUnlock();
		return NULL;
	}

	connection = ghiConnections[slot];

	memset(connection, 0, sizeof(GHIConnection));
	connection->inUse = GHTTPTrue;
	connection->request = (GHTTPRequest)slot;
	connection->uniqueID = ghiNextUniqueID++;
	connection->type = GHIGET;
	connection->state = GHTTPSocketInit;
	connection->URL = NULL;
	connection->serverAddress = NULL;
	connection->serverIP = INADDR_ANY;
	connection->serverPort = (unsigned short)0;
	connection->requestPath = NULL;
	connection->sendHeaders = NULL;
	connection->saveFile = NULL;
	connection->blocking = GHTTPFalse;
	connection->persistConnection = GHTTPFalse;
	connection->result = GHTTPSuccess;
	connection->progressCallback = NULL;
	connection->completedCallback = NULL;
	connection->callbackParam = NULL;
	connection->socket = INVALID_SOCKET;
	connection->socketError = 0;
	connection->userBufferSupplied = GHTTPFalse;
	connection->statusMajorVersion = 0;
	connection->statusMinorVersion = 0;
	connection->statusCode = 0;
	connection->statusStringIndex = 0;
	connection->headerStringIndex = 0;
	connection->completed = GHTTPFalse;
	connection->fileBytesReceived = 0;
	connection->totalSize = -1;
	connection->redirectURL = NULL;
	connection->redirectCount = 0;
	connection->chunkedTransfer = GHTTPFalse;
	connection->processing = GHTTPFalse;
	connection->throttle = GHTTPFalse;
	connection->lastThrottleRecv = 0;
	connection->post = NULL;
	connection->maxRecvTime = 500; 
	connection->proxyOverridePort = GHI_DEFAULT_PORT;	
	connection->proxyOverrideServer = NULL;
	connection->encryptor.mInterface = NULL;

#if !defined(GSI_NO_THREADS)
	connection->handle = NULL;
#endif

	bResult = ghiInitBuffer(connection, &connection->sendBuffer, SEND_BUFFER_INITIAL_SIZE, SEND_BUFFER_INCREMENT_SIZE);
	if(bResult)
		bResult = ghiInitBuffer(connection, &connection->encodeBuffer, ENCODE_BUFFER_INITIAL_SIZE, ENCODE_BUFFER_INCREMENT_SIZE);
	if(bResult)
		bResult = ghiInitBuffer(connection, &connection->recvBuffer, RECV_BUFFER_INITIAL_SIZE, RECV_BUFFER_INCREMENT_SIZE);
	if (bResult)
		bResult = ghiInitBuffer(connection, &connection->decodeBuffer, DECODE_BUFFER_INITIAL_SIZE, DECODE_BUFFER_INCREMENT_SIZE);
	
	if(!bResult)
	{
		ghiFreeConnection(connection);
		ghiUnlock();
		return NULL;
	}

	ghiNumConnections++;

	ghiUnlock();

	return connection;
}

GHTTPBool ghiFreeConnection
(
	GHIConnection * connection
)
{
	assert(connection);
	assert(connection->request >= 0);
	assert(connection->request < ghiConnectionsLen);
	assert(connection->inUse);

	if(!connection)
		return GHTTPFalse;
	if(!connection->inUse)
		return GHTTPFalse;
	if(connection->request < 0)
		return GHTTPFalse;
	if(connection->request >= ghiConnectionsLen)
		return GHTTPFalse;

	ghiLock();

	gsifree(connection->URL);
	gsifree(connection->serverAddress);
	gsifree(connection->requestPath);
	gsifree(connection->sendHeaders);
	gsifree(connection->redirectURL);
	gsifree(connection->proxyOverrideServer);
#ifndef NOFILE
	if(connection->saveFile)
		fclose(connection->saveFile);
#endif
	if(connection->socket != INVALID_SOCKET)
	{
		shutdown(connection->socket, 2);
		closesocket(connection->socket);
	}
	ghiFreeBuffer(&connection->sendBuffer);
	ghiFreeBuffer(&connection->encodeBuffer);
	ghiFreeBuffer(&connection->recvBuffer);
	ghiFreeBuffer(&connection->decodeBuffer);
	ghiFreeBuffer(&connection->getFileBuffer);
	if(connection->postingState.states)
		ghiPostCleanupState(connection);
   
#if !defined(GSI_NO_THREADS)
    if (connection->handle)
    {
        gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, 
            "Cancelling Thread and freeing memory\n");
        gsiCancelResolvingHostname(connection->handle);
    }
#endif

	if(connection->post && ghiIsPostAutoFree(connection->post))
	{
		ghiFreePost(connection->post);
		connection->post = NULL;
	}

	if (connection->encryptor.mInitialized != GHTTPFalse)
	{
		if (connection->encryptor.mCleanupFunc)
			(connection->encryptor.mCleanupFunc)(connection, &connection->encryptor);
		connection->encryptor.mInitialized = GHTTPFalse;
	}

	connection->inUse = GHTTPFalse;

	ghiNumConnections--;

	ghiUnlock();

	return GHTTPTrue;
}

GHIConnection * ghiRequestToConnection
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	assert(request >= 0);
	assert(request < ghiConnectionsLen);

	ghiLock();

	if((request < 0) || (request >= ghiConnectionsLen))
	{
		ghiUnlock();
		return NULL;
	}

	connection = ghiConnections[request];

	if(!connection->inUse)
		connection = NULL;

	ghiUnlock();

	return connection;
}

void ghiEnumConnections
(
	GHTTPBool (* callback)(GHIConnection *)
)
{
	int i;

	if(ghiNumConnections <= 0)
		return;

	ghiLock();
	for(i = 0 ; i < ghiConnectionsLen ; i++)
		if(ghiConnections[i]->inUse)
			callback(ghiConnections[i]);
	ghiUnlock();
}

void ghiRedirectConnection
(
	GHIConnection * connection
)
{
	assert(connection);
	assert(connection->redirectURL);
	
	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Redirecting Connection\n");

	connection->state = GHTTPSocketInit;

#if !defined(GSI_NO_THREADS)
    if (connection->handle)
    {
        gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, 
            "Cancelling Thread and freeing memory\n");
        gsiCancelResolvingHostname(connection->handle);
    }
#endif

	gsifree(connection->URL);
	connection->URL = connection->redirectURL;
	connection->redirectURL = NULL;

	gsifree(connection->serverAddress);
	connection->serverAddress = NULL;
	connection->serverIP = 0;
	connection->serverPort = 0;
	gsifree(connection->requestPath);
	connection->requestPath = NULL;

	shutdown(connection->socket, 2);
	closesocket(connection->socket);
	connection->socket = INVALID_SOCKET;

	ghiResetBuffer(&connection->sendBuffer);
	ghiResetBuffer(&connection->encodeBuffer);
	ghiResetBuffer(&connection->recvBuffer);
	ghiResetBuffer(&connection->decodeBuffer);

	connection->statusMajorVersion = 0;
	connection->statusMinorVersion = 0;
	connection->statusCode = 0;
	connection->statusStringIndex = 0;

	connection->headerStringIndex = 0;

	connection->connectionClosed = GHTTPFalse;

	if (connection->encryptor.mInitialized != GHTTPFalse)
	{
		if (connection->encryptor.mCleanupFunc)
			(connection->encryptor.mCleanupFunc)(connection, &connection->encryptor);
		connection->encryptor.mInitialized = GHTTPFalse;

		if(strncmp("https://", connection->URL, 8) != 0)
		{
			connection->encryptor.mEngine = GHTTPEncryptionEngine_None;
			connection->encryptor.mInterface = NULL;
		}
	}

	connection->redirectCount++;
}

void ghiCleanupConnections
(
	void
)
{
	int i;

	if(!ghiConnections)
		return;

	ghiEnumConnections(ghiFreeConnection);

	for(i = 0 ; i < ghiConnectionsLen ; i++)
		gsifree(ghiConnections[i]);
	gsifree(ghiConnections);
	ghiConnections = NULL;
	ghiConnectionsLen = 0;
	ghiNumConnections = 0;
}
