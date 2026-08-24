/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gsSSL.h"


const struct gsSSLCipherSuiteDesc gsSSLCipherSuites[GS_SSL_NUM_CIPHER_SUITES] =
{

	{ TLS_RSA_WITH_RC4_128_MD5,            16, 16, 00 },
	


};


const unsigned char gsSslRsaOid[9] =
{ 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };
