#ifndef __AUTHSERVICE_H__
#define __AUTHSERVICE_H__



#include "../common/gsSoap.h"
#include "../common/gsCrypt.h"
#include "../common/gsLargeInt.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define WS_LOGIN_MAX_URL_LEN		  (128)
extern char wsAuthServiceURL[WS_LOGIN_MAX_URL_LEN];


#define	WSLogin_PARTNERCODE_GAMESPY        0
#define	WSLogin_NAMESPACE_SHARED_NONUNIQUE 0
#define	WSLogin_NAMESPACE_SHARED_UNIQUE    1

typedef enum WSLoginValue
{
	WSLogin_Success = 0,
	WSLogin_ServerInitFailed,

	WSLogin_UserNotFound,
	WSLogin_InvalidPassword,
	WSLogin_InvalidProfile,
	WSLogin_UniqueNickExpired,

	WSLogin_DBError,
	WSLogin_ServerError,
	WSLogin_FailureMax, 

	WSLogin_HttpError = 100,    
	WSLogin_ParseError,         
	WSLogin_InvalidCertificate, 
	WSLogin_LoginFailed,        
	WSLogin_OutOfMemory,        
	WSLogin_InvalidParameters,  
	WSLogin_NoAvailabilityCheck,
	WSLogin_Cancelled,          
	WSLogin_UnknownError        

} WSLoginValue;


#define WS_LOGIN_SIGKEY_LEN_BITS      (GS_CRYPT_RSA_BINARY_SIZE)
#define WS_LOGIN_PEERKEY_LEN_BITS     (GS_CRYPT_RSA_BINARY_SIZE)

#define WS_LOGIN_NICK_LEN             (30+1)
#define WS_LOGIN_EMAIL_LEN            (50+1)
#define WS_LOGIN_PASSWORD_LEN         (30+1)
#define WS_LOGIN_UNIQUENICK_LEN       (20+1)
#define WS_LOGIN_CDKEY_LEN            (64+1)
#define WS_LOGIN_PEERKEYMOD_LEN       (WS_LOGIN_PEERKEY_LEN_BITS/8)
#define WS_LOGIN_PEERKEYEXP_LEN       (WS_LOGIN_PEERKEY_LEN_BITS/8)
#define WS_LOGIN_PEERKEYPRV_LEN       (WS_LOGIN_PEERKEY_LEN_BITS/8)
#define WS_LOGIN_KEYHASH_LEN          (33) 
#define WS_LOGIN_SIGNATURE_LEN        (WS_LOGIN_SIGKEY_LEN_BITS/8)
#define WS_LOGIN_SERVERDATA_LEN       (WS_LOGIN_PEERKEY_LEN_BITS/8)
#define WS_LOGIN_AUTHTOKEN_LEN        (256)
#define WS_LOGIN_PARTNERCHALLENGE_LEN (256)

typedef struct GSLoginCertificate
{
	gsi_bool mIsValid;
	
	gsi_u32 mLength;
	gsi_u32 mVersion;
	gsi_u32 mPartnerCode; 
	gsi_u32 mNamespaceId;
	gsi_u32 mUserId;
	gsi_u32 mProfileId;
	gsi_u32 mExpireTime;
	gsi_char mProfileNick[WS_LOGIN_NICK_LEN];
	gsi_char mUniqueNick[WS_LOGIN_UNIQUENICK_LEN];
	gsi_char mCdKeyHash[WS_LOGIN_KEYHASH_LEN];       
 	gsCryptRSAKey mPeerPublicKey;
	gsi_u8 mSignature[GS_CRYPT_RSA_BYTE_SIZE];   
	gsi_u8 mServerData[WS_LOGIN_SERVERDATA_LEN]; 
} GSLoginCertificate;

typedef struct GSLoginCertificatePrivate
{
	gsCryptRSAKey mPeerPrivateKey;
	char mKeyHash[GS_CRYPT_MD5_HASHSIZE];
} GSLoginPrivateData;


typedef enum 
{
	wsLoginType_INVALID,
	wsLoginType_PROFILE,
	wsLoginType_UNIQUENICK,
	wsLoginType_GPTICKET,
	wsLoginType_REMOTEAUTH
} WSLoginType;



typedef struct WSLoginResponse
{
	WSLoginValue mLoginResult;        
	WSLoginValue mResponseCode;       
	GSLoginCertificate mCertificate;  
	GSLoginPrivateData mPrivateData;  
	void * mUserData;
} WSLoginResponse;

typedef void (*WSLoginCallback)(GHTTPResult httpResult, WSLoginResponse * response, void * userData);


typedef struct WSLoginPs3CertResponse
{
	WSLoginValue mLoginResult;   
	WSLoginValue mResponseCode;  
	char mRemoteAuthToken[WS_LOGIN_AUTHTOKEN_LEN];         
	char mPartnerChallenge[WS_LOGIN_PARTNERCHALLENGE_LEN]; 
	void * mUserData;
} WSLoginPs3CertResponse;

typedef void (*WSLoginPs3CertCallback)(GHTTPResult httpResult, WSLoginPs3CertResponse * response, void * userData);


gsi_u32 wsLoginProfile(int partnerCode, int namespaceId, const gsi_char * profileNick, const gsi_char * email, const gsi_char * password, const gsi_char * cdkeyhash, WSLoginCallback callback, void * userData);
gsi_u32 wsLoginUnique (int partnerCode, int namespaceId, const gsi_char * uniqueNick, const gsi_char * password, const gsi_char * cdkeyhash, WSLoginCallback callback, void * userData);
gsi_u32 wsLoginRemoteAuth(int partnerCode, int namespaceId, const gsi_char authtoken[WS_LOGIN_AUTHTOKEN_LEN], const gsi_char partnerChallenge[WS_LOGIN_PARTNERCHALLENGE_LEN], WSLoginCallback callback, void * userData);

gsi_u32 wsLoginPs3Cert(int gameId, int partnerCode, int namespaceId, const gsi_u8 * ps3cert, int certLen, WSLoginPs3CertCallback callback, void * userData);

gsi_bool wsLoginCertIsValid    (const GSLoginCertificate * cert);
gsi_bool wsLoginCertWriteXML   (const GSLoginCertificate * cert, const char * anamespace, GSXmlStreamWriter writer);
gsi_bool wsLoginCertWriteBinary(const GSLoginCertificate * cert, char * bufout, unsigned int maxlen, unsigned int * lenout);
gsi_bool wsLoginCertReadBinary (GSLoginCertificate * certOut, char * bufin, unsigned int maxlen);
gsi_bool wsLoginCertReadXML    (GSLoginCertificate * cert, GSXmlStreamReader reader);


#if defined(__cplusplus)
} 
#endif

#endif 
