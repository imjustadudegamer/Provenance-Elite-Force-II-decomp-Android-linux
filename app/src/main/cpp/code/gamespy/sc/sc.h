#ifndef __SC_H__
#define __SC_H__


#include "../common/gsCommon.h"
#include "../common/gsRC4.h"
#include "../common/gsAvailable.h"
#include "../ghttp/ghttp.h"
#include "../webservices/AuthService.h"


#ifdef __cplusplus
extern "C" {
#endif 

#define SC_CALL


#define SC_STATIC_MEM

#define SC_REPORT_BUFFER_BYTES 65536

#define SC_SERVICE_MAX_URL_LEN 128
extern char scServiceURL[SC_SERVICE_MAX_URL_LEN];

#define SC_AUTHDATA_SIZE 16
#define SC_SESSION_GUID_SIZE 40
#define SC_CONNECTION_GUID_SIZE 40

#define SC_GUID_BINARY_SIZE 16 

#define SC_MAX_NUM_TEAMS	64

#define SC_OPTIONS_NONE					0


typedef enum
{
	SCResult_NO_ERROR = 0,
	SCResult_NO_AVAILABILITY_CHECK,
	SCResult_INVALID_PARAMETERS,
	SCResult_NOT_INITIALIZED,
	SCResult_CORE_NOT_INITIALIZED,
	SCResult_OUT_OF_MEMORY,
	SCResult_CALLBACK_PENDING,

	SCResult_HTTP_ERROR,
	SCResult_UNKNOWN_RESPONSE, 
	SCResult_RESPONSE_INVALID,

	SCResult_REPORT_INCOMPLETE,
	SCResult_REPORT_INVALID,
	SCResult_SUBMISSION_FAILED,

	SCResult_UNKNOWN_ERROR,

	SCResultMax
} SCResult;


typedef enum
{
	SCGameResult_WIN,
	SCGameResult_LOSS,
	SCGameResult_DRAW,
	SCGameResult_DISCONNECT,
	SCGameResult_DESYNC,
	SCGameResult_NONE,

	SCGameResultMax
} SCGameResult;


typedef enum
{
	SCGameStatus_COMPLETE,
	SCGameStatus_PARTIAL,
	SCGameStatus_BROKEN,

	SCGameStatusMax
} SCGameStatus;


typedef void*   SCInterfacePtr;
typedef void*   SCReportPtr;





typedef char    SCHiddenData[64];



typedef void (*SCCreateSessionCallback)(const SCInterfacePtr theInterface,
										GHTTPResult          theHttpResult,
										SCResult             theResult,
										void *               theUserData);
typedef void (*SCSetReportIntentionCallback)(const SCInterfacePtr theInterface,
									         GHTTPResult          theHttpResult,
                                             SCResult             theResult,
											 void *	              theUserData);
typedef void (*SCSubmitReportCallback)(const SCInterfacePtr theInterface,
									   GHTTPResult          theHttpResult,
									   SCResult             theResult,
									   void *               theUserData);


SCResult SC_CALL scInitialize(int theGameId,
							  SCInterfacePtr * theInterfaceOut);
SCResult SC_CALL scShutdown  (SCInterfacePtr theInterface);
SCResult SC_CALL scThink     (SCInterfacePtr theInterface);

SCResult SC_CALL scCreateSession(SCInterfacePtr             theInterface,
                                 const GSLoginCertificate * theCertificate,
								 const GSLoginPrivateData * thePrivateData,
								 SCCreateSessionCallback    theCallback,
								 gsi_time                   theTimeoutMs,
								 void *                     theUserData);

SCResult SC_CALL scCreateMatchlessSession(SCInterfacePtr    theInterface,
								 const GSLoginCertificate * theCertificate,
								 const GSLoginPrivateData * thePrivateData,
								 SCCreateSessionCallback    theCallback,
								 gsi_time                   theTimeoutMs,
								 void *                     theUserData);

SCResult SC_CALL scSetReportIntention(const SCInterfacePtr         theInterface,
									  const gsi_u8                 theConnectionId[SC_CONNECTION_GUID_SIZE],
									  gsi_bool                     isAuthoritative,
  							          const GSLoginCertificate *   theCertificate,
							          const GSLoginPrivateData *   thePrivateData,
							          SCSetReportIntentionCallback theCallback,
							          gsi_time                     theTimeoutMs,
									  void *                 theUserData);

SCResult SC_CALL scSubmitReport      (const SCInterfacePtr       theInterface,
									  const SCReportPtr          theReport,
									  gsi_bool                   isAuthoritative,
									  const GSLoginCertificate * theCertificate,
									  const GSLoginPrivateData * thePrivateData,
                                      SCSubmitReportCallback     theCallback,
                                      gsi_time                   theTimeoutMs,
									  void *                     theUserData);

SCResult SC_CALL scSetSessionId(const SCInterfacePtr theInterface, const gsi_u8 theSessionId[SC_SESSION_GUID_SIZE]);

const char * scGetSessionId   (const SCInterfacePtr theInterface);
const char * scGetConnectionId(const SCInterfacePtr theInterface);



SCResult SC_CALL scCreateReport(const SCInterfacePtr theInterface, 
								gsi_u32              theHeaderVersion, 
								gsi_u32              thePlayerCount,
								gsi_u32              theTeamCount,
								SCReportPtr *  theReportOut);

SCResult SC_CALL scReportBeginGlobalData(SCReportPtr theReportData);
SCResult SC_CALL scReportBeginPlayerData(SCReportPtr theReportData);
SCResult SC_CALL scReportBeginTeamData  (SCReportPtr theReportData);


SCResult SC_CALL scReportBeginNewPlayer(SCReportPtr  theReportData);
SCResult SC_CALL scReportSetPlayerData (SCReportPtr  theReport,
									    gsi_u32            thePlayerIndex,
									    const gsi_u8       thePlayerConnectionId[SC_CONNECTION_GUID_SIZE],
									    gsi_u32            thePlayerTeamId,
									    SCGameResult       theResult,
									    gsi_u32            theProfileId,
									    const GSLoginCertificate * theCertificate,
									    const gsi_u8       theAuthData[16]);

SCResult SC_CALL scReportBeginNewTeam(SCReportPtr theReportData);
SCResult SC_CALL scReportSetTeamData (SCReportPtr theReport,
									  gsi_u32           theTeamId,
									  SCGameResult      theResult);

SCResult SC_CALL scReportEnd(SCReportPtr theReport, 
							 gsi_bool    isAuth, 
							 SCGameStatus theStatus);

SCResult SC_CALL scReportSetAsMatchless(SCReportPtr theReport);


SCResult SC_CALL scReportAddIntValue(SCReportPtr theReportData,
									 gsi_u16           theKeyId,
									 gsi_i32           theValue);
SCResult SC_CALL scReportAddInt64Value(SCReportPtr theReportData,
									   gsi_u16           theKeyId,
									   gsi_i64           theValue);
SCResult SC_CALL scReportAddShortValue(SCReportPtr theReportData,
									 gsi_u16           theKeyId,
									 gsi_i16           theValue);
SCResult SC_CALL scReportAddByteValue(SCReportPtr theReportData,
									 gsi_u16           theKeyId,
									 gsi_i8            theValue);
SCResult SC_CALL scReportAddFloatValue(SCReportPtr theReportData,
									 gsi_u16           theKeyId,
									 float             theValue);
SCResult SC_CALL scReportAddStringValue(SCReportPtr theReportData,
										gsi_u16           theKeyId,
										const gsi_char *  theValue);
SCResult SC_CALL scDestroyReport(SCReportPtr theReport);


typedef struct SCPeerCipher
{
	RC4Context mRC4;
	gsi_u8  mKey[GS_CRYPT_RSA_BYTE_SIZE];
	gsi_u32 mKeyLen;
	gsi_bool mInitialized;
} SCPeerCipher;

typedef char SCPeerKeyExchangeMsg[GS_CRYPT_RSA_BYTE_SIZE];

SCResult SC_CALL scPeerCipherInit(const GSLoginCertificate * theLocalCert, SCPeerCipher * theCipher);

SCResult SC_CALL scPeerCipherCreateKeyExchangeMsg(const GSLoginCertificate * theRemoteCert,
												  const SCPeerCipher *       theCipher, 
												  SCPeerKeyExchangeMsg       theMsgOut);

SCResult SC_CALL scPeerCipherParseKeyExchangeMsg (const GSLoginCertificate * theLocalCert,  
												  const GSLoginPrivateData * theCertPrivateData, 
												  const SCPeerKeyExchangeMsg theMsg, 
												  SCPeerCipher *             theCipherOut);

SCResult SC_CALL scPeerCipherEncryptBuffer(SCPeerCipher * theCipher, gsi_u8 * theData, gsi_u32 theLen);
SCResult SC_CALL scPeerCipherDecryptBuffer(SCPeerCipher * theCipher, gsi_u8 * theData, gsi_u32 theLen);

SCResult SC_CALL scPeerCipherEncryptBufferIV(SCPeerCipher * theCipher, gsi_u32 theMessageNum, gsi_u8 * theData, gsi_u32 theLen);
SCResult SC_CALL scPeerCipherDecryptBufferIV(SCPeerCipher * theCipher, gsi_u32 theMessageNum, gsi_u8 * theData, gsi_u32 theLen);



#ifdef __cplusplus
} 
#endif 


#endif 
