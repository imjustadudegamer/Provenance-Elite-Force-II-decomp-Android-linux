#ifndef __GS_CRYPT_H__
#define __GS_CRYPT_H__


#include "gsLargeInt.h"
#include "../md5.h"


#if defined(__cplusplus)
extern "C" {
#endif


#define GS_CRYPT_HASHSIZE          GS_CRYPT_SHA1_HASHSIZE

#define GS_CRYPT_SHA1_HASHSIZE     20
#define GS_CRYPT_MD5_HASHSIZE      16

#define GS_CRYPT_RSA_ES_PKCS1v1_5 

#ifndef GS_CRYPT_RSA_BINARY_SIZE 
#define GS_CRYPT_RSA_BINARY_SIZE   1024
#endif

#define GS_CRYPT_RSA_BYTE_SIZE     (GS_CRYPT_RSA_BINARY_SIZE/8) 

#define GS_CRYPT_RSA_DATABLOCKSIZE (GS_CRYPT_RSA_BYTE_SIZE-GS_CRYPT_HASHSIZE-1)





typedef struct 
{
	gsLargeInt_t modulus;
	gsLargeInt_t exponent;
} gsCryptRSAKey;

typedef struct 
{
	gsi_u8 headerByte; 
	gsi_u8 maskedSeed[GS_CRYPT_HASHSIZE]; 
	gsi_u8 maskedData[GS_CRYPT_RSA_DATABLOCKSIZE]; 
} gsCryptRSAOAEPPacket;

typedef struct 
{
	gsi_u8 headerByte[2]; 
	gsi_u8 data[GS_CRYPT_RSA_BYTE_SIZE-2]; 
} gsCryptRSAPKCS1Packet;


gsi_i32 gsCryptRSAEncryptBuffer(const gsCryptRSAKey *publicKey, const unsigned char *plainText, gsi_u32 len, unsigned char cipherText[GS_CRYPT_RSA_BYTE_SIZE]);
gsi_i32 gsCryptRSAVerifySignedHash(const gsCryptRSAKey *publicKey, const unsigned char *hash, gsi_u32 hashLen, const unsigned char *sig, gsi_u32 sigLen);


gsi_i32 gsCryptRSADecryptBuffer(const gsCryptRSAKey *privateKey, const unsigned char cipherText[GS_CRYPT_RSA_BYTE_SIZE], unsigned char *plainTextOut, gsi_u32 *lenOut);
gsi_i32 gsCryptRSASignData(const gsCryptRSAKey *privateKey, const unsigned char *plainText, gsi_u32 plainTextLen, unsigned char *signedDataOut, gsi_u32 *lenOut);
gsi_i32 gsCryptRSASignHash(const gsCryptRSAKey *privateKey, const unsigned char *hash, gsi_u32 hashLen, unsigned char *signedDataOut, gsi_u32 *lenOut);

#if defined(__cplusplus)
}
#endif

#endif 
