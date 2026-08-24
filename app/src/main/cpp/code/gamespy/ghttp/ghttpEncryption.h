///////////////////////////////////////////////////////////////////////////////
#ifndef __GHTTPENCRYPTION_H__
#define __GHTTPENCRYPTION_H__




typedef enum
{
	GHIEncryptionMethod_None,
	GHIEncryptionMethod_Encrypt,  
	GHIEncryptionMethod_Decrypt   
} GHIEncryptionMethod;

typedef enum
{
	GHIEncryptionResult_None,
	GHIEncryptionResult_Success,        
	GHIEncryptionResult_BufferTooSmall, 
	GHIEncryptionResult_Error           
} GHIEncryptionResult;


struct GHIEncryptor; 
struct GHIConnection;

typedef GHIEncryptionResult (*GHTTPEncryptorInitFunc)   (struct GHIConnection * theConnection, 
											   struct GHIEncryptor * theEncryptor);

typedef GHIEncryptionResult (*GHTTPEncryptorConnectFunc)(struct GHIConnection * theConnection, 
											   struct GHIEncryptor * theEncryptor);

typedef GHIEncryptionResult (*GHTTPEncryptorStartFunc)(struct GHIConnection * theConnection, 
											   struct GHIEncryptor * theEncryptor);

typedef GHIEncryptionResult (*GHTTPEncryptorCleanupFunc)(struct GHIConnection * theConnection, 
											   struct GHIEncryptor * theEncryptor);

typedef GHIEncryptionResult (*GHTTPEncryptorEncryptFunc)(struct GHIConnection * theConnection, 
											   struct GHIEncryptor * theEncryptor,
											   const char * thePlainTextBuffer,
											   int          thePlainTextLength, 
											   char *       theEncryptedBuffer,
											   int *        theEncryptedLength); 

typedef GHIEncryptionResult (*GHTTPEncryptorDecryptFunc)(struct GHIConnection * theConnection, 
											   struct GHIEncryptor* theEncryptor,
											   const char * theEncryptedBuffer,
											   int *        theEncryptedLength, 
											   char *       theDecryptedBuffer,
											   int *        theDecryptedLength);


typedef struct GHIEncryptor
{
	void*     mInterface;   
	GHTTPEncryptionEngine mEngine;
	GHTTPBool mInitialized;
	GHTTPBool mSessionStarted;      
	GHTTPBool mSessionEstablished;  
	
	GHTTPBool mEncryptOnBuffer;  
	GHTTPBool mEncryptOnSend;    

	GHTTPBool mLibSendsHandshakeMessages;  

	GHTTPEncryptorInitFunc      mInitFunc;
	GHTTPEncryptorCleanupFunc   mCleanupFunc;
	GHTTPEncryptorStartFunc     mStartFunc;  
	GHTTPEncryptorEncryptFunc   mEncryptFunc;
	GHTTPEncryptorDecryptFunc   mDecryptFunc;
} GHIEncryptor;


GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
									struct GHIEncryptor  * theEncryptor);
GHIEncryptionResult ghiEncryptorSslCleanupFunc(struct GHIConnection * connection,
									   struct GHIEncryptor  * theEncryptor);

GHIEncryptionResult ghiEncryptorSslStartFunc(struct GHIConnection * connection,
								struct GHIEncryptor * theEncryptor);
									   
GHIEncryptionResult ghiEncryptorSslEncryptFunc(struct GHIConnection * connection,
									   struct GHIEncryptor  * theEncryptor,
									   const char * thePlainTextBuffer,
									   int          thePlainTextLength,
									   char *       theEncryptedBuffer,
									   int *        theEncryptedLength);
GHIEncryptionResult ghiEncryptorSslDecryptFunc(struct GHIConnection * connection,
									   struct GHIEncryptor  * theEncryptor,
									   const char * theEncryptedBuffer,
									   int *        theEncryptedLength,
									   char *       theDecryptedBuffer,
									   int *        theDecryptedLength);
GHIEncryptionResult ghiEncryptorSslEncryptSend(struct GHIConnection * connection,
                                 struct GHIEncryptor * theEncryptor,
                                 const char * thePlainTextBuffer,
                                 int thePlainTextLength,
                                 int * theBytesSent);
GHIEncryptionResult ghiEncryptorSslDecryptRecv(struct GHIConnection * connection,
                                 struct GHIEncryptor * theEncryptor,
                                 char * theDecryptedBuffer,
                                 int * theDecryptedLength);
                                 

#endif 
