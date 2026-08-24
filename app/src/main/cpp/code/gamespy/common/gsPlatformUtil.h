/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __GSUTILITY_H__
#define __GSUTILITY_H__


#include "gsPlatform.h"


#ifdef __cplusplus
extern "C" {
#endif



typedef struct GSIResolveHostnameInfo * GSIResolveHostnameHandle;
#define GSI_STILL_RESOLVING_HOSTNAME   0
#define GSI_ERROR_RESOLVING_HOSTNAME   0xFFFFFFFF

int  gsiStartResolvingHostname(const char * hostname, GSIResolveHostnameHandle * handle);
void gsiCancelResolvingHostname(GSIResolveHostnameHandle handle);
unsigned int gsiGetResolvedIP(GSIResolveHostnameHandle handle);


#if (defined(__MWERKS__) && !defined(_NITRO)) || defined(WIN32)
	#define GSI_UNUSED(x) x
#elif defined(_PS2) || defined(_NITRO) || defined(_PS3) || defined(_MACOSX)
	#define GSI_UNUSED(x) {void* y=&x;y=NULL;}
#elif defined(_PSP)
#define GSI_UNUSED(x) (void)x;
	
#else
	#define GSI_UNUSED(x)
#endif


void Util_RandSeed(unsigned long seed); 
int  Util_RandInt(int low, int high);   


void B64Encode(const char *input, char *output, int inlen, int encodingType);
void B64Decode(const char *input, char *output, int inlen, int * outlen, int encodingType);

int B64DecodeLen(const char *input, int encodingType);

typedef struct
{
	const char *input;
	int len;
	int encodingType;
} B64StreamData;

void B64InitEncodeStream(B64StreamData *data, const char *input, int len, int encodingType);

gsi_bool B64EncodeStream(B64StreamData *data, char output[4]);


#define XXTEA_KEY_SIZE 17
gsi_i8 * gsXxteaEncrypt(const gsi_i8 * iStr, gsi_i32 iLength, gsi_i8 key[XXTEA_KEY_SIZE], gsi_i32 *oLength);
gsi_i8 * gsXxteaDecrypt(const gsi_i8 * iStr, gsi_i32 iLength, gsi_i8 key[XXTEA_KEY_SIZE], gsi_i32 *oLength);

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#if defined(_DEBUG)
	void gsiCheckStack(void);
#else
	#define gsiCheckStack() 
#endif



gsi_time current_time(void);         
gsi_time current_time_hires(void);   
void msleep(gsi_time msec);      

struct tm * gsiSecondsToDate(const time_t *timp);		
time_t  gsiDateToSeconds(struct tm *tb);				
char * gsiSecondsToString(const time_t *timp);			



	
#if defined(_NITRO) 
	time_t time(time_t *timer);
	
	#define gmtime(t)	gsiSecondsToDate(t)
	#define ctime(t)	gsiSecondsToString(t)
	#define mktime(t)	gsiDateToSeconds(t)	
#elif defined(_REVOLUTION)
	time_t gsiTimeInSec(time_t *timer);
	struct tm *gsiGetGmTime(time_t *theTime);
	char *gsiCTime(time_t *theTime);
	#define time(t) gsiTimeInSec(t)
	#define gmtime(t) gsiGetGmTime(t)
	#define ctime(t) gsiCTime(t)
#else
	#include <time.h>
#endif


	#ifndef SOMAXCONN
	#define SOMAXCONN 5
#endif

typedef const char * (* GetUniqueIDFunction)(void);

extern GetUniqueIDFunction GOAGetUniqueID;

#ifdef _PS2
extern int wprintf(const wchar_t*,...);
#endif


gsi_i64 gsiStringToInt64(const char *theNumberStr);
void gsiInt64ToString(char theNumberStr[33], gsi_i64 theNumber);
#ifdef __cplusplus
}
#endif

#endif 

