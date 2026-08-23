#ifndef __GSSSL_H__
#define __GSSSL_H__

#include "../darray.h"
#include "../md5.h"
#include "gsCrypt.h"
#include "gsSHA1.h"
#include "gsRC4.h"

#if defined(__cplusplus)
extern "C"
{
#endif


#define GS_SSL_VERSION_MAJOR             (0x03)
#define GS_SSL_VERSION_MINOR             (0x00)


#define GS_SSL_CONTENT_CHANGECIPHERSPEC  (0x14) 
#define GS_SSL_CONTENT_ALERT             (0x15) 
#define GS_SSL_CONTENT_HANDSHAKE         (0x16) 
#define GS_SSL_CONTENT_APPLICATIONDATA   (0x17) 

#define GS_SSL_HANDSHAKE_CLIENTHELLO        (1)
#define GS_SSL_HANDSHAKE_SERVERHELLO        (2)
#define GS_SSL_HANDSHAKE_CERTIFICATE        (11) 
#define GS_SSL_HANDSHAKE_SERVERHELLODONE    (14) 
#define GS_SSL_HANDSHAKE_CLIENTKEYEXCHANGE  (16) 
#define GS_SSL_HANDSHAKE_FINISHED           (20) 

#define GS_SSL_MAX_CONTENTLENGTH ((0x4000) - (0xFF))

#ifndef HAVE_CIPHER_SUITES
	#define TLS_RSA_WITH_RC4_128_MD5                0x04
	#define TLS_RSA_WITH_RCA_128_SHA                0x05
	#define TLS_RSA_WITH_3DES_EDE_CBC_SHA           0x0a
	#define TLS_RSA_WITH_DES_CBC_SHA                0x09
	#define TLS_RSA_EXPORT1024_WITH_RC4_56_SHA      0x64
	#define TLS_RSA_EXPORT1024_WITH_DES_CBC_SHA     0x62
	#define TLS_RSA_EXPORT_WITH_RC4_40_MD5          0x03
	#define TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5      0x06
	#define TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA       0x13
	#define TLS_DHE_DSS_WITH_DES_CBC_SHA            0x12
	#define TLS_DHE_DSS_EXPORT1024_WITH_DES_CBC_SHA 0x63
#endif

#define GS_SSL_MAX_MAC_SECRET_SIZE    (20)
#define GS_SSL_MAX_SYMMETRIC_KEY_SIZE (16)
#define GS_SSL_MAX_IV_SIZE            (16)
#define GS_SSL_NUM_CIPHER_SUITES       (1)  
#define GS_SSL_MASTERSECRET_LEN       (48)
#define GS_SSL_PAD_ONE  "666666666666666666666666666666666666666666666666" 
#define GS_SSL_PAD_TWO  "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\" 
#define GS_SSL_MD5_PAD_LEN            (48)
#define GS_SSL_SHA1_PAD_LEN           (40) 
#define GS_SSL_CLIENT_FINISH_VALUE    "CLNT"
#define GS_SSL_SERVER_FINISH_VALUE    "SRVR"


typedef struct gsSSL
{
	int sessionLen;
	unsigned char sessionData[255]; 
	unsigned short cipherSuite;

	gsCryptRSAKey serverpub;
	unsigned char sendSeqNBO[8];    
	unsigned char receiveSeqNBO[8]; 

	unsigned char clientWriteMACSecret[GS_CRYPT_SHA1_HASHSIZE];
	unsigned char clientReadMACSecret [GS_CRYPT_SHA1_HASHSIZE];
	unsigned char clientWriteKey      [GS_SSL_MAX_SYMMETRIC_KEY_SIZE];
	unsigned char clientReadKey       [GS_SSL_MAX_SYMMETRIC_KEY_SIZE];
	unsigned char clientWriteIV       [GS_SSL_MAX_IV_SIZE];
	unsigned char clientReadIV        [GS_SSL_MAX_IV_SIZE];

	int clientWriteMACLen;
	int clientReadMACLen;
	int clientWriteKeyLen;
	int clientReadKeyLen;
	int clientWriteIVLen;
	int clientReadIVLen;

	RC4Context sendRC4; 
	RC4Context recvRC4; 

	MD5_CTX finishHashMD5;
	SHA1Context finishHashSHA1;
	unsigned char serverRandom[32]; 
	unsigned char clientRandom[32]; 
	unsigned char premastersecret[GS_SSL_MASTERSECRET_LEN]; 
	unsigned char mastersecret[GS_SSL_MASTERSECRET_LEN];

} gsSSL;


typedef struct gsSSLRecordHeaderMsg
{
	unsigned char contentType;  
	unsigned char versionMajor; 
	unsigned char versionMinor; 
	unsigned char lengthNBO[2]; 
	

} gsSSLRecordHeaderMsg;

typedef struct gsSSLClientHelloMsg
{
	gsSSLRecordHeaderMsg header;    
	unsigned char handshakeType; 
	unsigned char lengthNBO[3];  
	unsigned char versionMajor;  
	unsigned char versionMinor;  
	unsigned char time[4];       
	unsigned char random[28];    
	unsigned char sessionIdLen;   
	
	unsigned short cipherSuitesLength; 
	unsigned short cipherSuites[GS_SSL_NUM_CIPHER_SUITES];
	unsigned char compressionMethodLen; 
	unsigned char compressionMethodList; 
} gsSSLClientHelloMsg;

typedef struct gsSSLClientKeyExchangeMsg
{
	gsSSLRecordHeaderMsg header; 
	unsigned char handshakeType; 
	unsigned char lengthNBO[3];
} gsSSLClientKeyExchangeMsg;


typedef struct gsSSLCipherSuiteDesc
{
	int mSuiteID;
	int mKeyLen;
	int mMACLen;
	int mIVLen;
} gsSSLCipherSuiteDesc;

extern const gsSSLCipherSuiteDesc gsSSLCipherSuites[GS_SSL_NUM_CIPHER_SUITES];
extern const unsigned char gsSslRsaOid[9];


#if defined(__cplusplus)
} 
#endif

#endif 
