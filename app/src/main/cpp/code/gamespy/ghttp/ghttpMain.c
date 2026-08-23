
#include "ghttpMain.h"
#include "ghttpASCII.h"
#include "ghttpConnection.h"
#include "ghttpCallbacks.h"
#include "ghttpProcess.h"
#include "ghttpPost.h"
#include "ghttpCommon.h"


GHTTPRequest ghttpGetExA(const char * URL, const char * headers, char * buffer, int bufferSize, GHTTPPost post, GHTTPBool throttle, GHTTPBool blocking, ghttpProgressCallback progressCallback, ghttpCompletedCallback completedCallback, void * param);
GHTTPRequest ghttpSaveExA(const char * URL, const char * filename, const char * headers, GHTTPPost post, GHTTPBool throttle, GHTTPBool blocking, ghttpProgressCallback progressCallback, ghttpCompletedCallback completedCallback, void * param);
GHTTPRequest ghttpStreamExA(const char * URL, const char * headers, GHTTPPost post, GHTTPBool throttle, GHTTPBool blocking, ghttpProgressCallback progressCallback, ghttpCompletedCallback completedCallback, void * param);
GHTTPRequest ghttpHeadExA(const char * URL, const char * headers, GHTTPBool throttle, GHTTPBool blocking, ghttpProgressCallback progressCallback, ghttpCompletedCallback completedCallback, void * param);
GHTTPRequest ghttpPostExA(const char * URL, const char * headers, GHTTPPost post, GHTTPBool throttle, GHTTPBool blocking, ghttpProgressCallback progressCallback, ghttpCompletedCallback completedCallback, void * param);



static int ghiReferenceCount;

static void ghiHandleStatus
(
	GHIConnection * connection
)
{
	switch(connection->statusCode / 100)
	{
	case 1:  
		return;
	case 2:  
		return;
	case 3:  
		return;
	case 4:  
		switch(connection->statusCode)
		{
		case 401:
			connection->result = GHTTPUnauthorized;
			break;
		case 403:
			connection->result = GHTTPForbidden;
			break;
		case 404:
		case 410:
			connection->result = GHTTPFileNotFound;
			break;
		default:
			connection->result = GHTTPRequestRejected;
			break;
		}
		return;
	case 5:  
		connection->result = GHTTPServerError;
		return;
	}
}

static GHTTPBool ghiProcessConnection
(
	GHIConnection * connection
)
{
	GHTTPBool completed;

	assert(connection);
	assert(ghiRequestToConnection(connection->request) == connection);

	if(connection->processing)
		return GHTTPFalse;

	connection->processing = GHTTPTrue;


	if(connection->state == GHTTPSocketInit)
		ghiDoSocketInit(connection);
	if(connection->state == GHTTPHostLookup)
		ghiDoHostLookup(connection);
	if(connection->state == GHTTPLookupPending)
		ghiDoLookupPending(connection);
	if(connection->state == GHTTPConnecting)
		ghiDoConnecting(connection);
	if(connection->state == GHTTPSecuringSession)
		ghiDoSecuringSession(connection);
	if(connection->state == GHTTPSendingRequest)
		ghiDoSendingRequest(connection);
	if(connection->state == GHTTPPosting)
		ghiDoPosting(connection);
	if(connection->state == GHTTPWaiting)
		ghiDoWaiting(connection);
	if(connection->state == GHTTPReceivingStatus)
		ghiDoReceivingStatus(connection);
	if(connection->state == GHTTPReceivingHeaders)
		ghiDoReceivingHeaders(connection);
	if(connection->state == GHTTPReceivingFile)
		ghiDoReceivingFile(connection);

	if(connection->redirectURL)
		ghiRedirectConnection(connection);

	completed = connection->completed;
	
	if (connection->result == GHTTPRequestCancelled && !connection->completed && !CanReceiveOnSocket(connection->socket))
	{
		connection->completed = GHTTPTrue;
	}

	if(connection->completed)
	{
		ghiHandleStatus(connection);

#ifndef NOFILE
		if(connection->saveFile)
		{
			fclose(connection->saveFile);
			connection->saveFile = NULL;
		}
#endif
		ghiLogResponse(connection->getFileBuffer.data, connection->getFileBuffer.len);

		ghiCallCompletedCallback(connection);

		ghiFreeConnection(connection);
	}
	else
	{
		connection->processing = GHTTPFalse;
	}

	return completed;
}

void ghttpStartup
(
	void
)
{
	ghiLock();

	ghiReferenceCount++;

	if(ghiReferenceCount == 1)
	{
		ghiCreateLock();

		ghiThrottleBufferSize = GHI_DEFAULT_THROTTLE_BUFFER_SIZE;
		ghiThrottleTimeDelay = GHI_DEFAULT_THROTTLE_TIME_DELAY;
	}
	else
	{
		ghiUnlock();
	}
}

void ghttpCleanup
(
	void
)
{
	ghiLock();

	ghiReferenceCount--;

	if(!ghiReferenceCount)
	{
		ghiCleanupConnections();

		if(ghiProxyAddress)
		{
			gsifree(ghiProxyAddress);
			ghiProxyAddress = NULL;
		}

		ghiUnlock();

		ghiFreeLock();
	}
	else
	{
		ghiUnlock();
	}
}

GHTTPRequest ghttpGetA
(
	const char * URL,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	return ghttpGetExA(URL, NULL, NULL, 0, NULL, GHTTPFalse, blocking, NULL, completedCallback, param);
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpGetW
(
	const unsigned short * URL,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024];

	assert(URL != NULL);
	UCS2ToAsciiString(URL, (char*)URL_A);
	return ghttpGetA(URL_A, blocking, completedCallback, param);
}
#endif

GHTTPRequest ghttpGetExA
(
	const char * URL,
	const char * headers,
	char * buffer,
	int bufferSize,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	GHTTPBool bResult;
	GHIConnection * connection;

	assert(URL && URL[0]);
	assert(bufferSize >= 0);
	assert(!buffer || bufferSize);

	if(!URL || !URL[0])
		return GHTTPInvalidURL;
	if(bufferSize < 0)
		return GHTTPInvalidBufferSize;
	if(buffer && !bufferSize)
		return GHTTPInvalidBufferSize;

	if(!ghiReferenceCount)
		ghttpStartup();

	connection = ghiNewConnection();
	if(!connection)
		return GHTTPInsufficientMemory;

	connection->type = GHIGET;
	connection->URL = goastrdup(URL);
	if(!connection->URL)
	{
		ghiFreeConnection(connection);
		return GHTTPInsufficientMemory;
	}
	if(headers && *headers)
	{
		connection->sendHeaders = goastrdup(headers);
		if(!connection->sendHeaders)
		{
			ghiFreeConnection(connection);
			return GHTTPInsufficientMemory;
		}
	}
	connection->post = post;
	connection->blocking = blocking;
	connection->progressCallback = progressCallback;
	connection->completedCallback = completedCallback;
	connection->callbackParam = param;
	connection->throttle = throttle;
	connection->userBufferSupplied = (buffer != NULL)?GHTTPTrue:GHTTPFalse;
	if(connection->userBufferSupplied)
		bResult = ghiInitFixedBuffer(connection, &connection->getFileBuffer, buffer, bufferSize);
	else
		bResult = ghiInitBuffer(connection, &connection->getFileBuffer, GET_FILE_BUFFER_INITIAL_SIZE, GET_FILE_BUFFER_INCREMENT_SIZE);
	if(!bResult)
	{
		ghiFreeConnection(connection);
		return GHTTPUnspecifiedError;
	}

	if(post && !ghiPostInitState(connection))
	{
		ghiFreeConnection(connection);
		return GHTTPInvalidPost;
	}

	if(blocking)
	{
		while(!ghiProcessConnection(connection))
			msleep(10);

		return 0;
	}

	return connection->request;
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpGetExW
(
	const unsigned short * URL,
	const unsigned short * headers,
	char * buffer,
	int bufferSize,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024];
	char headers_A[1024] = { '\0' };

	assert(URL != NULL);
	UCS2ToAsciiString(URL, (char*)URL_A);
	if (headers != NULL)
		UCS2ToAsciiString(headers, headers_A);
	return ghttpGetExA((char*)URL_A, (char*)headers_A, buffer, bufferSize, post, throttle, blocking, progressCallback, completedCallback, param);
}
#endif

GHTTPRequest ghttpSaveA
(
	const char * URL,
	const char * filename,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	return ghttpSaveExA(URL, filename, NULL, NULL, GHTTPFalse, blocking, NULL, completedCallback, param);
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpSaveW
(
	const unsigned short * URL,
	const unsigned short * filename,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024] = { '\0' };
	char filename_A[1024] = { '\0' };

	assert(URL != NULL);
	UCS2ToAsciiString(URL, URL_A);
	UCS2ToAsciiString(filename, filename_A);
	return ghttpSaveA(URL_A, filename_A, blocking, completedCallback, param);
}
#endif

static GHTTPRequest _ghttpSaveEx
(
	const char * URL,
	const gsi_char * filename,
	const char * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	GHIConnection * connection;

	assert(URL && URL[0]);
	assert(filename && filename[0]);

	if(!URL || !URL[0])
		return GHTTPInvalidURL;
	if(!filename || !filename[0])
		return GHTTPInvalidFileName;

	if(!ghiReferenceCount)
		ghttpStartup();

	connection = ghiNewConnection();
	if(!connection)
		return GHTTPInsufficientMemory;

	connection->type = GHISAVE;
	connection->URL = goastrdup(URL);
	if(!connection->URL)
	{
		ghiFreeConnection(connection);
		return GHTTPInsufficientMemory;
	}
	if(headers && *headers)
	{
		connection->sendHeaders = goastrdup(headers);
		if(!connection->sendHeaders)
		{
			ghiFreeConnection(connection);
			return GHTTPInsufficientMemory;
		}
	}
	connection->post = post;
	connection->blocking = blocking;
	connection->progressCallback = progressCallback;
	connection->completedCallback = completedCallback;
	connection->callbackParam = param;
	connection->throttle = throttle;

	if(post && !ghiPostInitState(connection))
	{
		ghiFreeConnection(connection);
		return GHTTPInvalidPost;
	}

#ifdef NOFILE
	connection->saveFile = NULL;
#else
	connection->saveFile = _tfopen(filename, _T("wb"));
#endif
	if(!connection->saveFile)
	{
		ghiFreeConnection(connection);
		return GHTTPFailedToOpenFile;
	}

	if(blocking)
	{
		while(!ghiProcessConnection(connection))
			msleep(10);

		return 0;
	}

	return connection->request;
}

GHTTPRequest ghttpSaveExA
(
	const char * URL,
	const char * filename,
	const char * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	#ifdef GSI_UNICODE
		unsigned short filename_W[1024];
		AsciiToUCS2String(filename, filename_W);
		return _ghttpSaveEx(URL, filename_W, headers, post, throttle, blocking, progressCallback, completedCallback, param);
	#else
		return _ghttpSaveEx(URL, filename, headers, post, throttle, blocking, progressCallback, completedCallback, param);
	#endif
}

#ifdef GSI_UNICODE
GHTTPRequest ghttpSaveExW
(
	const unsigned short * URL,
	const unsigned short * filename,
	const unsigned short * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024];
	char headers_A[1024] = { '\0' };

	assert(URL_A != NULL);
	UCS2ToAsciiString(URL, URL_A);
	if (headers != NULL)
		UCS2ToAsciiString(headers, headers_A);

	return _ghttpSaveEx(URL_A, filename, headers_A, post, throttle, blocking, progressCallback, completedCallback, param);
}
#endif

GHTTPRequest ghttpStreamA
(
	const char * URL,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	return ghttpStreamExA(URL, NULL, NULL, GHTTPFalse, blocking, progressCallback, completedCallback, param);
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpStreamW
(
	const unsigned short * URL,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char* URL_A = { '\0' };
	UCS2ToAsciiString(URL, URL_A);
	return ghttpStreamA(URL_A, blocking, progressCallback, completedCallback, param);
}
#endif

GHTTPRequest ghttpStreamExA
(
	const char * URL,
	const char * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	GHIConnection * connection;

	assert(URL && URL[0]);

	if(!URL || !URL[0])
		return GHTTPInvalidURL;

	if(!ghiReferenceCount)
		ghttpStartup();

	connection = ghiNewConnection();
	if(!connection)
		return GHTTPInsufficientMemory;

	connection->type = GHISTREAM;
	connection->URL = goastrdup(URL);
	if(!connection->URL)
	{
		ghiFreeConnection(connection);
		return GHTTPInsufficientMemory;
	}
	if(headers && *headers)
	{
		connection->sendHeaders = goastrdup(headers);
		if(!connection->sendHeaders)
		{
			ghiFreeConnection(connection);
			return GHTTPInsufficientMemory;
		}
	}
	connection->post = post;
	connection->blocking = blocking;
	connection->progressCallback = progressCallback;
	connection->completedCallback = completedCallback;
	connection->callbackParam = param;
	connection->throttle = throttle;

	if(post && !ghiPostInitState(connection))
	{
		ghiFreeConnection(connection);
		return GHTTPInvalidPost;
	}

	if(blocking)
	{
		while(!ghiProcessConnection(connection))
			msleep(10);

		return 0;
	}

	return connection->request;
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpStreamExW
(
	const unsigned short * URL,
	const unsigned short * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024] = {'\0'};
	char headers_A[1024] = {'\0'};
	UCS2ToAsciiString(URL, URL_A);
	if(headers != NULL)
		UCS2ToAsciiString(headers, headers_A);
	return ghttpStreamExA(URL_A, headers_A, post, throttle, blocking, progressCallback, completedCallback, param);
}
#endif

GHTTPRequest ghttpHeadA
(
	const char * URL,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	return ghttpHeadExA(URL, NULL, GHTTPFalse, blocking, NULL, completedCallback, param);
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpHeadW
(
	const unsigned short * URL,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024] = {'\0'};
	UCS2ToAsciiString(URL, URL_A);
	return ghttpHeadA(URL_A, blocking, completedCallback, param);
}
#endif

GHTTPRequest ghttpHeadExA
(
	const char * URL,
	const char * headers,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	GHIConnection * connection;

	assert(URL && URL[0]);

	if(!URL || !URL[0])
		return GHTTPInvalidURL;

	if(!ghiReferenceCount)
		ghttpStartup();

	connection = ghiNewConnection();
	if(!connection)
		return GHTTPInsufficientMemory;

	connection->type = GHIHEAD;
	connection->URL = goastrdup(URL);
	if(!connection->URL)
	{
		ghiFreeConnection(connection);
		return GHTTPInsufficientMemory;
	}
	if(headers && *headers)
	{
		connection->sendHeaders = goastrdup(headers);
		if(!connection->sendHeaders)
		{
			ghiFreeConnection(connection);
			return GHTTPInsufficientMemory;
		}
	}
	connection->blocking = blocking;
	connection->progressCallback = progressCallback;
	connection->completedCallback = completedCallback;
	connection->callbackParam = param;
	connection->throttle = throttle;

	if(blocking)
	{
		while(!ghiProcessConnection(connection))
			msleep(10);

		return 0;
	}

	return connection->request;
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpHeadExW
(
	const unsigned short * URL,
	const unsigned short * headers,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024] = {'\0'};
	char headers_A[1024] = {'\0'};
	if (URL != NULL)
		UCS2ToAsciiString(URL, URL_A);
	if (headers != NULL)
		UCS2ToAsciiString(headers, headers_A);
	return ghttpHeadExA(URL_A, headers_A, throttle, blocking, progressCallback, completedCallback, param);
}
#endif

GHTTPRequest ghttpPostA
(
	const char * URL,
	GHTTPPost post,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	return ghttpPostExA(URL, NULL, post, GHTTPFalse, blocking, NULL, completedCallback, param);
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpPostW
(
	const unsigned short * URL,
	GHTTPPost post,
	GHTTPBool blocking,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024] = {'\0'};
	UCS2ToAsciiString(URL, URL_A);
	return ghttpPostA(URL_A, post, blocking, completedCallback, param);
}
#endif

GHTTPRequest ghttpPostExA
(
	const char * URL,
	const char * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	GHIConnection * connection;

	assert(URL && URL[0]);
	assert(post);

	if(!URL || !URL[0])
		return GHTTPInvalidURL;
	if(!post)
		return GHTTPInvalidPost;

	if(!ghiReferenceCount)
		ghttpStartup();

	connection = ghiNewConnection();
	if(!connection)
		return GHTTPInsufficientMemory;

	connection->type = GHIPOST;
	connection->URL = goastrdup(URL);
	if(!connection->URL)
	{
		ghiFreeConnection(connection);
		return GHTTPInsufficientMemory;
	}
	if(headers && *headers)
	{
		connection->sendHeaders = goastrdup(headers);
		if(!connection->sendHeaders)
		{
			ghiFreeConnection(connection);
			return GHTTPInsufficientMemory;
		}
	}
	connection->post = post;
	connection->blocking = blocking;
	connection->progressCallback = progressCallback;
	connection->completedCallback = completedCallback;
	connection->callbackParam = param;
	connection->throttle = throttle;

	if(post && !ghiPostInitState(connection))
	{
		ghiFreeConnection(connection);
		return GHTTPInvalidPost;
	}

	if(blocking)
	{
		while(!ghiProcessConnection(connection))
			msleep(10);

		return 0;
	}

	return connection->request;
}
#ifdef GSI_UNICODE
GHTTPRequest ghttpPostExW
(
	const unsigned short * URL,
	const unsigned short * headers,
	GHTTPPost post,
	GHTTPBool throttle,
	GHTTPBool blocking,
	ghttpProgressCallback progressCallback,
	ghttpCompletedCallback completedCallback,
	void * param
)
{
	char URL_A[1024] = {'\0'};
	char headers_A[1024] = {'\0'};
	UCS2ToAsciiString(URL, URL_A);
	if (headers != NULL)
		UCS2ToAsciiString(headers, headers_A);
	return ghttpPostExA(URL_A, headers_A, post, throttle, blocking, progressCallback, completedCallback, param);
}
#endif

void ghttpThink
(
	void
)
{
	ghiEnumConnections(ghiProcessConnection);
}

GHTTPBool ghttpRequestThink
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return GHTTPFalse;

	ghiProcessConnection(connection);
	return GHTTPTrue;
}

void ghttpCancelRequest
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return;

	ghiFreeConnection(connection);
}

#if !defined(INSOCK)
void ghttpCloseRequest
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if (!connection)
		return;

	if (connection->socket)
	{
		shutdown(connection->socket, 1);
		connection->result = GHTTPRequestCancelled;
	}
}
#endif

GHTTPState ghttpGetState
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return (GHTTPState)0;

	return connection->state;
}

const char * ghttpGetResponseStatus
(
	GHTTPRequest request,
	int * statusCode
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return NULL;

	if(connection->state <= GHTTPReceivingStatus)
		return NULL;

	if(statusCode)
		*statusCode = connection->statusCode;

	return (connection->recvBuffer.data + connection->statusStringIndex);
}

const char * ghttpGetHeaders
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return NULL;

	if(connection->state < GHTTPReceivingHeaders)
		return NULL;

	if(connection->headerStringIndex >= connection->recvBuffer.len)
		return NULL;

	return (connection->recvBuffer.data + connection->headerStringIndex);
}

const char * ghttpGetURL
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return NULL;

	return connection->URL;
}

GHTTPBool ghttpSetProxy
(
	const char * server
)
{
	return ghiSetProxy(server);
}

GHTTPBool ghttpSetRequestProxy
(
	GHTTPRequest request, 
	const char * server 
)
{
	return ghiSetRequestProxy(request, server);
}


void ghttpSetThrottle
(
	GHTTPRequest request,
	GHTTPBool throttle
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return;

	connection->throttle = throttle;

	if(connection->socket != INVALID_SOCKET)
		SetReceiveBufferSize(connection->socket, throttle?ghiThrottleBufferSize:(8 * 1024));
}

void ghttpThrottleSettings
(
	int bufferSize,
	gsi_time timeDelay
)
{
	ghiThrottleSettings(bufferSize, timeDelay);
}

void ghttpSetMaxRecvTime
(
	GHTTPRequest request,
	gsi_time maxRecvTime
)
{
	GHIConnection* connection = ghiRequestToConnection(request);
	if (connection == NULL)
		return;

	connection->maxRecvTime = maxRecvTime;
}

SOCKET ghttpGetSocket(GHTTPRequest request);
GHTTPBool ghttpReuseSocket(GHTTPRequest request, SOCKET socket);

SOCKET ghttpGetSocket
(
	GHTTPRequest request
)
{
	GHIConnection * connection;
	SOCKET ret;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return INVALID_SOCKET;

	if (!connection->completed)
		return INVALID_SOCKET;

	ret = connection->socket;
	connection->socket = INVALID_SOCKET;
	
	return ret;
}

GHTTPBool ghttpReuseSocket
(
	GHTTPRequest request,
	SOCKET socket
)
{
	GHIConnection * connection;

	connection = ghiRequestToConnection(request);
	if(!connection)
		return GHTTPFalse;
	if (connection->state != GHTTPSocketInit)
		return GHTTPFalse;
	if (connection->socket != INVALID_SOCKET)
		return GHTTPFalse;

	connection->persistConnection = GHTTPTrue;
	connection->socket = socket;


	return GHTTPTrue;
}


GHTTPPost ghttpNewPost
(
	void
)
{
	return ghiNewPost();
}

void ghttpPostSetAutoFree
(
	GHTTPPost post,
	GHTTPBool autoFree
)
{
	assert(post);
	if(!post)
		return;

	ghiPostSetAutoFree(post, autoFree);
}

void ghttpFreePost
(
	GHTTPPost post
)
{
	assert(post);
	if(!post)
		return;

	ghiFreePost(post);
}

GHTTPBool ghttpPostAddStringA
(
	GHTTPPost post,
	const char * name,
	const char * string
)
{
	assert(post);
	assert(name && name[0]);

	if(!post)
		return GHTTPFalse;
	if(!name || !name[0])
		return GHTTPFalse;
	if(!string)
		string = "";

	return ghiPostAddString(post, name, string);
}
#ifdef GSI_UNICODE
GHTTPBool ghttpPostAddStringW
(
	GHTTPPost post,
	const unsigned short * name,
	const unsigned short * string
)
{
	char name_A[1024] = {'\0'};
	char string_A[1024] = {'\0'};
	if (name != NULL)
		UCS2ToAsciiString(name, name_A);
	if (string != NULL)
		UCS2ToAsciiString(string, string_A);
	return ghttpPostAddStringA(post, name_A, string_A);
}
#endif

GHTTPBool ghttpPostAddFileFromDiskA
(
	GHTTPPost post,
	const char * name,
	const char * filename,
	const char * reportFilename,
	const char * contentType
)
{
	assert(post);
	assert(name && name[0]);
	assert(filename && filename[0]);

	if(!post)
		return GHTTPFalse;
	if(!name || !name[0])
		return GHTTPFalse;
	if(!filename || !filename[0])
		return GHTTPFalse;
	if(!reportFilename || !reportFilename[0])
		reportFilename = filename;
	if(!contentType)
		contentType = "application/octet-stream";

	return ghiPostAddFileFromDisk(post, name, filename, reportFilename, contentType);
}
#ifdef GSI_UNICODE
GHTTPBool ghttpPostAddFileFromDiskW
(
	GHTTPPost post,
	const unsigned short * name,
	const unsigned short * filename,
	const unsigned short * reportFilename,
	const unsigned short * contentType
)
{
	char name_A[1024] = {'\0'};
	char filename_A[1024] = {'\0'};
	char reportFilename_A[1024] = {'\0'};
	char contentType_A[1024] = {'\0'};
	if (name != NULL)			UCS2ToAsciiString(name, name_A);
	if (filename != NULL)		UCS2ToAsciiString(filename, filename_A);
	if (reportFilename != NULL)	UCS2ToAsciiString(reportFilename, reportFilename_A);
	if (contentType != NULL)	UCS2ToAsciiString(contentType, contentType_A);
	return ghttpPostAddFileFromDiskA(post, name_A, filename_A, reportFilename_A, contentType_A);
}
#endif

GHTTPBool ghttpPostAddFileFromMemoryA
(
	GHTTPPost post,
	const char * name,
	const char * buffer,
	int bufferLen,
	const char * reportFilename,
	const char * contentType
)
{
	assert(post);
	assert(name && name[0]);
	assert(bufferLen >= 0);
#ifdef _DEBUG
	if(bufferLen > 0)
		assert(buffer);
#endif
	assert(reportFilename && reportFilename[0]);

	if(!post)
		return GHTTPFalse;
	if(!name || !name[0])
		return GHTTPFalse;
	if(bufferLen < 0)
		return GHTTPFalse;
	if(!bufferLen && !buffer)
		return GHTTPFalse;
	if(!contentType)
		contentType = "application/octet-stream";

	return ghiPostAddFileFromMemory(post, name, buffer, bufferLen, reportFilename, contentType);
}
#ifdef GSI_UNICODE
GHTTPBool ghttpPostAddFileFromMemoryW
(
	GHTTPPost post,
	const unsigned short * name,
	const char * buffer,
	int bufferLen,
	const unsigned short * reportFilename,
	const unsigned short * contentType
)
{
	char name_A[1024] = { '\0' };
	char reportFilename_A[1024] = { '\0' };
	char contentType_A[1024] = { '\0' };
	if (name != NULL)
		UCS2ToAsciiString(name, name_A);
	if (reportFilename != NULL)
		UCS2ToAsciiString(reportFilename, reportFilename_A);
	if (contentType != NULL)
		UCS2ToAsciiString(contentType, contentType_A);

	
	return ghttpPostAddFileFromMemoryA(post, name_A, buffer, bufferLen, reportFilename_A, contentType_A);
	
	GSI_UNUSED(reportFilename);
	GSI_UNUSED(contentType);
}
#endif

GHTTPBool ghttpPostAddXml
(
	GHTTPPost post,
	GSXmlStreamWriter soap
)
{
	GS_ASSERT(post != NULL);
	GS_ASSERT(soap != NULL);

	return ghiPostAddXml(post, soap);
}

void ghttpPostSetCallback
(
	GHTTPPost post,
	ghttpPostCallback callback,
	void * param
)
{
	assert(post);

	if(!post)
		return;

	ghiPostSetCallback(post, callback, param);
}


