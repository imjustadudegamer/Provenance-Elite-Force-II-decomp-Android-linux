#include "gsCrypt.h"
#include "gsLargeInt.h"
#include "gsSHA1.h"



 
#ifdef GS_CRYPT_RSA_ES_OAEP
	static gsi_i32 gsCryptRSAOAEPEncryptBuffer(const gsCryptRSAKey *publicKey, const unsigned char *plainText, gsi_u32 len, unsigned char buffer[GS_CRYPT_RSA_BYTE_SIZE]);
	static gsi_i32 gsCryptRSAOAEPDecryptBuffer(const gsCryptRSAKey *privateKey, const unsigned char cipherText[GS_CRYPT_RSA_BYTE_SIZE], unsigned char *plainText, gsi_u32 *lenout);

	static void gsiCryptRSAGenerateSeed(unsigned char *buffer, gsi_u32 len);
	static gsi_bool gsiCryptRSAMaskData(unsigned char *data, gsi_u32 len, const unsigned char *maskSeed, gsi_u32 maskLen);
#else
	static gsi_i32 gsCryptRSAPKCS1EncryptBuffer(const gsCryptRSAKey *publicKey, const unsigned char *plainText, gsi_u32 len, unsigned char buffer[GS_CRYPT_RSA_BYTE_SIZE]);
	static gsi_i32 gsCryptRSAPKCS1DecryptBuffer(const gsCryptRSAKey *privateKey, const unsigned char ciperText[GS_CRYPT_RSA_BYTE_SIZE], unsigned char *plainTextOut, gsi_u32 *lenOut);

	static void gsiCryptRSAGeneratePad(unsigned char *buffer, gsi_u32 len);
#endif



#ifdef GS_CRYPT_RSA_ES_OAEP
	void gsiCryptRSAGenerateSeed(unsigned char *buffer, gsi_u32 len)
	{
		unsigned int i=0;

		Util_RandSeed(current_time());
		for (i=0; i < len; i++)
		{
			buffer[i] = (unsigned char)(Util_RandInt(0x00, 0xFF)+1);
			GS_ASSERT(buffer[i] != 0x00);
		}
	}

	gsi_bool gsiCryptRSAMaskData(unsigned char *data, gsi_u32 dataLen, const unsigned char* seed, gsi_u32 seedLen)
	{
		int i=0;
		int k=0;
		
		unsigned char hashValue[GS_CRYPT_HASHSIZE]; 

		if (seedLen > GS_CRYPT_RSA_DATABLOCKSIZE)
			return gsi_false;

		for (i=0; (gsi_u32)i<dataLen;i+=GS_CRYPT_HASHSIZE)
		{
			#if (GS_CRYPT_HASHSIZE==GS_CRYPT_MD5_HASHSIZE)
			{
				gsi_u32 temp=0;

				char seedPlusIter[GS_CRYPT_RSA_DATABLOCKSIZE+4]; 
				char hashHexStr[GS_CRYPT_RSA_DATABLOCKSIZE*2+1]; 
				memcpy(&seedPlusIter[0], seed, seedLen);
				seedPlusIter[GS_CRYPT_RSA_DATABLOCKSIZE+0] = 0x00;
				seedPlusIter[GS_CRYPT_RSA_DATABLOCKSIZE+1] = 0x00;
				seedPlusIter[GS_CRYPT_RSA_DATABLOCKSIZE+2] = 0x00;
				seedPlusIter[GS_CRYPT_RSA_DATABLOCKSIZE+3] = (gsi_u8)(i/GS_CRYPT_HASHSIZE);
				MD5Digest(seedPlusIter, seedLen+sizeof(gsi_u32), hashHexStr);

				for(k=0; k<seedLen+sizeof(gsi_u32))
				{
					gsi_u32 temp;
					temp = sscanf(&hashHexStr[k*2], "%02X", &temp);
					hashValue[k] = (gsi_u8)temp;
				}
			}
			#elif (GS_CRYPT_HASHSIZE==GS_CRYPT_SHA1_HASHSIZE)
			{
				gsi_u8 counter[4] = { 0x00,0x00,0x00,0x00 };
				SHA1Context sha;
				counter[3] = (gsi_u8)(i/GS_CRYPT_HASHSIZE); 
				SHA1Reset(&sha);
				SHA1Input(&sha, (const unsigned char*)seed, seedLen);
				SHA1Input(&sha, counter, 4);
				SHA1Result(&sha, hashValue);
			}
			#endif

			for (k=0; k<GS_CRYPT_HASHSIZE; k++)
			{
				if ((gsi_u32)(i+k) >= dataLen)
					return gsi_true;

				data[i+k] ^= (gsi_u8)hashValue[k];
			}
		}
		return gsi_true;
	}
#else
	void gsiCryptRSAGeneratePad(unsigned char *buffer, gsi_u32 len)
	{
		unsigned int i=0;

		Util_RandSeed(current_time());
		for (i=0; i < len; i++)
		{
			#if defined(GS_CRYPT_NO_RANDOM)
            #pragma message("GS_CRYPT_NO_RANDOM defined, SSL is NOT SECURE!!!!\r\n")
			buffer[i] = 0x0c;
			#else
			buffer[i] = (unsigned char)(Util_RandInt(0x00, 0xFF)+1);
			#endif
			GS_ASSERT(buffer[i] != 0x00);
		}
	}
#endif


gsi_i32 gsCryptRSAEncryptBuffer(const gsCryptRSAKey *publicKey, const unsigned char *plainText, gsi_u32 len, unsigned char buffer[GS_CRYPT_RSA_BYTE_SIZE])
{
	#ifdef GS_CRYPT_RSA_ES_OAEP
		return gsCryptRSAOAEPEncryptBuffer(publicKey, plainText, len, buffer);
	#else
		return gsCryptRSAPKCS1EncryptBuffer(publicKey, plainText, len, buffer);
	#endif
}

#ifdef GS_CRYPT_RSA_ES_OAEP
	gsi_i32 gsCryptRSAOAEPEncryptBuffer(const gsCryptRSAKey *publicKey, const unsigned char *plainText, gsi_u32 len, unsigned char buffer[GS_CRYPT_RSA_BYTE_SIZE])
	{
		gsLargeInt_t lintRSAPacket;
		gsCryptRSAOAEPPacket* packet = (gsCryptRSAOAEPPacket*)lintRSAPacket.mData; 
		
	#if (GS_CRYPT_HASHSIZE==GS_CRYPT_MD5_HASHSIZE) 
		const gsi_u8 lhash[GS_CRYPT_HASHSIZE] = {0xd4,0x1d,0xd4,0x1d,0x8c,0xd9,0x8f,0x00,0xb2,0x04,0xe9,0x80,0x09,0x98,0xec,0xf8,0x42,0x7e}; 
	#else
		const gsi_u8 lhash[GS_CRYPT_HASHSIZE] = {0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,0x32,0x55,0xbf,0xef,0x95,0x60,0x18,0x90,0xaf,0xd8,0x07,0x09}; 
	#endif
		const unsigned int maxPlainTextLen = GS_CRYPT_RSA_BYTE_SIZE-2*GS_CRYPT_HASHSIZE-2;
		const unsigned int padSize = maxPlainTextLen-len;
			
		if (len > maxPlainTextLen)
			return -1;


		memcpy(packet->maskedData, lhash, GS_CRYPT_HASHSIZE);
		memset(&packet->maskedData[GS_CRYPT_HASHSIZE], 0, padSize); 
		packet->maskedData[GS_CRYPT_HASHSIZE+padSize] = 0x01; 
		memcpy(&packet->maskedData[GS_CRYPT_HASHSIZE+padSize+1], plainText, len);

		gsiCryptRSAGenerateSeed(packet->maskedSeed, GS_CRYPT_HASHSIZE);

		gsiCryptRSAMaskData(packet->maskedData, GS_CRYPT_RSA_DATABLOCKSIZE, packet->maskedSeed, GS_CRYPT_HASHSIZE);
		
		gsiCryptRSAMaskData(packet->maskedSeed, GS_CRYPT_HASHSIZE, packet->maskedData, GS_CRYPT_RSA_DATABLOCKSIZE);

		packet->headerByte = 0x00;

		GS_ASSERT(0); 

		return 0;
	}
#else
	gsi_i32 gsCryptRSAPKCS1EncryptBuffer(const gsCryptRSAKey *publicKey, const unsigned char *plainText, gsi_u32 len, unsigned char buffer[GS_CRYPT_RSA_BYTE_SIZE])
	{
		gsi_u8 buf[GS_CRYPT_RSA_BYTE_SIZE];
		gsCryptRSAPKCS1Packet* packet = (gsCryptRSAPKCS1Packet*)buf;
		gsLargeInt_t lintRSAPacket;

		if (len > (GS_CRYPT_RSA_BYTE_SIZE-11)) 
			return -1;

		packet->headerByte[0] = 0x00;
		packet->headerByte[1] = 0x02;

		gsiCryptRSAGeneratePad(packet->data, GS_CRYPT_RSA_BYTE_SIZE-len-3);
		packet->data[GS_CRYPT_RSA_BYTE_SIZE-len-3] = 0x00; 
		memcpy(&packet->data[GS_CRYPT_RSA_BYTE_SIZE-len-3+1], plainText, len);

		if (gsi_is_false(gsLargeIntSetFromMemoryStream(&lintRSAPacket, (const gsi_u8*)buf, GS_CRYPT_RSA_BYTE_SIZE)) ||
			gsi_is_false(gsLargeIntPowerMod(&lintRSAPacket, &publicKey->exponent, &publicKey->modulus, &lintRSAPacket)) ||
			gsi_is_false(gsLargeIntWriteToMemoryStream(&lintRSAPacket, buffer)) )
		{
			return -1;
		}
		
		return 0;
	}
#endif


gsi_i32 gsCryptRSADecryptBuffer(const gsCryptRSAKey *privateKey, const unsigned char cipherText[GS_CRYPT_RSA_BYTE_SIZE], unsigned char *plainText, gsi_u32 *lenout)
{
	#ifdef GS_CRYPT_RSA_ES_OAEP
		return gsCryptRSAOAEPDecryptBuffer(privateKey, cipherText, plainText, lenout);
	#else
		return gsCryptRSAPKCS1DecryptBuffer(privateKey, cipherText, plainText, lenout);
	#endif
}

#ifndef GS_CRYPT_RSA_ES_OAEP
	static gsi_i32 gsCryptRSAPKCS1DecryptBuffer(const gsCryptRSAKey *privateKey, const unsigned char cipherText[GS_CRYPT_RSA_BYTE_SIZE], unsigned char *plainText, gsi_u32 *lenout)
	{
		int i=0;
		char* temp;
		gsLargeInt_t lintRSAPacket;
		lintRSAPacket.mLength = GS_CRYPT_RSA_BYTE_SIZE/GS_LARGEINT_DIGIT_SIZE_BYTES;
		memcpy(lintRSAPacket.mData, cipherText, GS_CRYPT_RSA_BYTE_SIZE);

		if (gsi_is_false(gsLargeIntReverseBytes(&lintRSAPacket)) || 
			gsi_is_false(gsLargeIntPowerMod(&lintRSAPacket, &privateKey->exponent, &privateKey->modulus, &lintRSAPacket)) ||
			gsi_is_false(gsLargeIntReverseBytes(&lintRSAPacket)) 
			)
		{
			return -1;
		}

		if (lintRSAPacket.mLength < (GS_CRYPT_RSA_BYTE_SIZE/GS_LARGEINT_DIGIT_SIZE_BYTES))
			return -1;

		temp = (char*)lintRSAPacket.mData;
		if (temp[0] != 0x00)
			return -1;
		if (temp[1] != 0x02)
			return -2;

		temp = (char*)lintRSAPacket.mData;
		for (i=2; i<GS_CRYPT_RSA_BYTE_SIZE; i++)
		{
			if (temp[i] == 0)
				break;
		}
		if (i < (2+8)) 
			return -3; 
		if (i == GS_CRYPT_RSA_BYTE_SIZE)
			return -4; 

		memcpy(plainText, (temp+i+1), GS_CRYPT_RSA_BYTE_SIZE); 
		*lenout = (gsi_u32)(GS_CRYPT_RSA_BYTE_SIZE-(i+1)); 

		return 0;
	}
#else
	static gsi_i32 gsCryptRSAOAEPDecryptBuffer(const gsCryptRSAKey *privateKey, const unsigned char cipherText[GS_CRYPT_RSA_BYTE_SIZE], unsigned char *plainText, gsi_u32 *lenout)
	{
		int i=0;
		gsLargeInt_t lintRSAPacket;
		gsCryptRSAOAEPPacket* packet = (gsCryptRSAOAEPPacket*)lintRSAPacket.mData;
		
	#if (GS_CRYPT_HASHSIZE==GS_CRYPT_MD5_HASHSIZE) 
		const gsi_u8 lhash[GS_CRYPT_HASHSIZE] = {0xd4,0x1d,0xd4,0x1d,0x8c,0xd9,0x8f,0x00,0xb2,0x04,0xe9,0x80,0x09,0x98,0xec,0xf8,0x42,0x7e}; 
	#else
		const gsi_u8 lhash[GS_CRYPT_HASHSIZE] = {0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,0x32,0x55,0xbf,0xef,0x95,0x60,0x18,0x90,0xaf,0xd8,0x07,0x09}; 
	#endif

		lintRSAPacket.mLength = GS_CRYPT_RSA_BYTE_SIZE/4;
		memcpy(lintRSAPacket.mData, cipherText, GS_CRYPT_RSA_BYTE_SIZE);

		gsLargeIntReverseBytes(&lintRSAPacket); 
		gsLargeIntPowerMod(&lintRSAPacket, &privateKey->exponent, &privateKey->modulus, &lintRSAPacket);
		gsLargeIntReverseBytes(&lintRSAPacket); 

		if (lintRSAPacket.mLength < (GS_CRYPT_RSA_BYTE_SIZE/4))
			return -1;

		gsiCryptRSAMaskData(packet->maskedSeed, GS_CRYPT_HASHSIZE, packet->maskedData, GS_CRYPT_RSA_DATABLOCKSIZE);
		gsiCryptRSAMaskData(packet->maskedData, GS_CRYPT_RSA_DATABLOCKSIZE, packet->maskedSeed, GS_CRYPT_HASHSIZE);
		if (0 != memcmp(packet->maskedData, lhash, GS_CRYPT_HASHSIZE))
			return -2; 
		i = 33;
		while(i<GS_CRYPT_RSA_BYTE_SIZE && packet->maskedData[i] == 0x00)
			i++; 
		if (i==GS_CRYPT_RSA_BYTE_SIZE || packet->maskedData[i] != 0x01)
			return -3; 
		i++;
		if (i == GS_CRYPT_RSA_BYTE_SIZE)
			return -4; 

		memcpy(plainText, &packet->maskedData[i], (size_t)(GS_CRYPT_RSA_DATABLOCKSIZE-i));
		*lenout = (gsi_u32)(GS_CRYPT_RSA_DATABLOCKSIZE-i); 
		return 0;
	}
#endif


gsi_i32 gsCryptRSASignData(const gsCryptRSAKey *privateKey, const unsigned char *plainText, gsi_u32 plainTextLen, unsigned char *signedDataOut, gsi_u32 *lenOut)
{
	const unsigned char * hash = NULL;

	GSI_UNUSED(privateKey);
	GSI_UNUSED(plainText);
	GSI_UNUSED(plainTextLen);
	GSI_UNUSED(signedDataOut);
	GSI_UNUSED(lenOut);



	return gsCryptRSASignHash(privateKey, hash, GS_CRYPT_MD5_HASHSIZE, signedDataOut, lenOut);
}


gsi_i32 gsCryptRSASignHash(const gsCryptRSAKey *privateKey, const unsigned char *hash, gsi_u32 hashLen, unsigned char *signedDataOut, gsi_u32 *lenOut)
{
	gsi_u32 aKeyByteLength = privateKey->modulus.mLength * GS_LARGEINT_DIGIT_SIZE_BYTES; 
	gsi_u32 aReservedLength = 3;
	gsi_u32 anOidLen;

	gsLargeInt_t dataToSign;
	char * writeBuf = (char*)dataToSign.mData;

	gsi_u8 md5Header[18] =  {0x30,0x20,0x30,0x0C,0x06,0x08,0x2A,0x86,0x48,0x86,0xF7,0x0D,0x02,0x05,0x05,0x00,0x04,0x10};
	gsi_u8 sha1Header[15] = {0x30,0x21,0x30,0x09,0x06,0x05,0x2B,0x0E,0x03,0x02,0x1A,0x05,0x00,0x04,0x14};

	if (hashLen == GS_CRYPT_MD5_HASHSIZE)
		anOidLen = sizeof(md5Header);
	else if (hashLen == GS_CRYPT_SHA1_HASHSIZE)
		anOidLen = sizeof(sha1Header);
	else
		return -1; 

	GS_ASSERT(hashLen + anOidLen + aReservedLength <= aKeyByteLength); 
	if (hashLen + anOidLen + aReservedLength > aKeyByteLength)
		return -2; 

	writeBuf[0] = 0x00;
	writeBuf[1] = 0x01;

	memset(&writeBuf[2], 0xFF, aKeyByteLength - hashLen - anOidLen - aReservedLength);

	writeBuf[aKeyByteLength - hashLen - anOidLen - 1] = 0x00;

	if (hashLen == GS_CRYPT_MD5_HASHSIZE)
		memcpy(&writeBuf[aKeyByteLength-hashLen-anOidLen], md5Header, sizeof(md5Header));
	else if (hashLen == GS_CRYPT_SHA1_HASHSIZE)
		memcpy(&writeBuf[aKeyByteLength-hashLen-anOidLen], sha1Header, sizeof(sha1Header));
	else
		return -1;  

	memcpy(&writeBuf[aKeyByteLength-hashLen], hash, hashLen);
	
	dataToSign.mLength = privateKey->modulus.mLength;
	gsLargeIntReverseBytes(&dataToSign); 

	gsLargeIntPowerMod(&dataToSign, &privateKey->exponent, &privateKey->modulus, &dataToSign);
	
	GS_ASSERT(dataToSign.mLength == privateKey->modulus.mLength);
	gsLargeIntReverseBytes(&dataToSign); 
	memcpy(signedDataOut, dataToSign.mData, aKeyByteLength);
	*lenOut = aKeyByteLength;
	
	return 0;
}


gsi_i32 gsCryptRSAVerifySignedHash(const gsCryptRSAKey *publicKey, const unsigned char *hash, gsi_u32 hashLen, const unsigned char *sig, gsi_u32 sigLen)
{
	gsLargeInt_t lintRSASignature;
	gsi_u8* packet = (gsi_u8*)lintRSASignature.mData;
	int i=0;

	gsi_u8 md5Header[18] =  {0x30,0x20,0x30,0x0C,0x06,0x08,0x2A,0x86,0x48,0x86,0xF7,0x0D,0x02,0x05,0x05,0x00,0x04,0x10};
	gsi_u8 sha1Header[15] = {0x30,0x21,0x30,0x09,0x06,0x05,0x2B,0x0E,0x03,0x02,0x1A,0x05,0x00,0x04,0x14};

	if (hash==NULL || sig==NULL)
		return -1;
	if (sigLen != publicKey->modulus.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES)
		return -1;
	if (hashLen != GS_CRYPT_MD5_HASHSIZE && hashLen != GS_CRYPT_SHA1_HASHSIZE)
		return -1; 

	lintRSASignature.mLength = (l_word)(sigLen / GS_LARGEINT_DIGIT_SIZE_BYTES);
	memcpy(lintRSASignature.mData, sig, sigLen);
	gsLargeIntReverseBytes(&lintRSASignature);
	gsLargeIntPowerMod(&lintRSASignature, &publicKey->exponent, &publicKey->modulus, &lintRSASignature);
	gsLargeIntReverseBytes(&lintRSASignature);

	if (packet[0] != 0x00 || packet[1] != 0x01)
		return -2;

	for (i=2; i<GS_CRYPT_RSA_BYTE_SIZE; i++)
	{
		if (packet[i] == 0x00)
			break;
		if (packet[i] != 0xFF)
			return -3;
	}
	i++; 

	if ((i+sizeof(md5Header)+GS_CRYPT_MD5_HASHSIZE) == (lintRSASignature.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES))
	{
		if (0 != memcmp(md5Header, &packet[i], sizeof(md5Header)))
			return -4;
		i += sizeof(md5Header);
		if (hashLen != GS_CRYPT_MD5_HASHSIZE)
			return -5;
		if (0 != memcmp(&packet[i], hash, GS_CRYPT_MD5_HASHSIZE))
			return -5;
	}
	else if ((i+sizeof(sha1Header)+GS_CRYPT_SHA1_HASHSIZE) == (lintRSASignature.mLength*GS_LARGEINT_DIGIT_SIZE_BYTES))
	{
		if (0 != memcmp(sha1Header, &packet[i], sizeof(sha1Header)))
			return -4;
		i += sizeof(sha1Header);
		if (hashLen != GS_CRYPT_SHA1_HASHSIZE)
			return -5;
		if (0 != memcmp(&packet[i], hash, GS_CRYPT_SHA1_HASHSIZE))
			return -5;
	}
	else
		return -4; 
	
	return 0;
}
