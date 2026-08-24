/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#ifndef _GHTTPCOMMON_H_
#define _GHTTPCOMMON_H_

#include "ghttp.h"
#include "ghttpConnection.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CRLF    "\xD\xA"

#define GHI_LEGAL_URLENCODED_CHARS      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_@-.*"
#define GHI_DIGITS                      "0123456789ABCDEF"

#define GHI_DEFAULT_PORT                      80
#define GHI_DEFAULT_SECURE_PORT               443
#define GHI_DEFAULT_THROTTLE_BUFFER_SIZE      125
#define GHI_DEFAULT_THROTTLE_TIME_DELAY       250

extern char * ghiProxyAddress;
extern unsigned short ghiProxyPort;

extern int ghiThrottleBufferSize;
extern gsi_time ghiThrottleTimeDelay;

void ghiCreateLock(void);
void ghiFreeLock(void);
void ghiLock(void);
void ghiUnlock(void);

#ifdef HTTP_LOG
void ghiLogToFile
(
	const char * buffer,
	int len,
	const char * fileName
);
#define ghiLogRequest(b,c)  ghiLogToFile(b,c,"request.log");
#define ghiLogResponse(b,c)	ghiLogToFile(b,c,"response.log");
#define ghiLogPost(b,c)	    ghiLogToFile(b,c,"post.log");
#else
#define ghiLogRequest(b,c)
#define ghiLogResponse(b,c)
#define ghiLogPost(b,c)
#endif


typedef enum
{
	GHIRecvData,         
	GHINoData,           
	GHIConnClosed,       
	GHIError             
} GHIRecvResult;

GHIRecvResult ghiDoReceive
(
	GHIConnection * connection,
	char buffer[],
	int * bufferLen
);

int ghiDoSend
(
	GHIConnection * connection,
	const char * buffer,
	int len
);

typedef enum
{
	GHITrySendError,     
	GHITrySendSent,      
	GHITrySendBuffered   
} GHITrySendResult;

GHITrySendResult ghiTrySendThenBuffer
(
	GHIConnection * connection,
	const char * buffer,
	int len
);

GHTTPBool ghiSetProxy
(
	const char * server
);

GHTTPBool ghiSetRequestProxy
(
	GHTTPRequest request,
	const char * server
);

void ghiThrottleSettings
(
	int bufferSize,
	gsi_time timeDelay
);

GHTTPBool ghiDecryptReceivedData(struct GHIConnection * connection);


#ifdef __cplusplus
}
#endif

#endif
