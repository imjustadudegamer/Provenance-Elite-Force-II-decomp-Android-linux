/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gsCommon.h"
#include "gsPlatformUtil.h"

#if defined(_X360)
#elif defined(_XBOX)
#elif defined(_WIN32)
	#include "win32/gsUtilWin32.c"
#elif defined(_LINUX)
	#include "linux/gsUtilLinux.c"
#elif defined(_MACOSX)
	#include "macosx/gsUtilMacOSX.c"
#elif defined(_NITRO)
	#include "nitro/gsUtilNitro.c"
#elif defined(_PS2)
	#include "ps2/gsUtilPs2.c"
#elif defined(_PS3)
	#include "ps3/gsUtilPs3.c"
#elif defined(_PSP)
	#include "psp/gsUtilPSP.c"
#elif defined(_REVOLUTION)
	#include "revolution/gsUtilRevolution.c"
#else
	#error "Missing or unsupported platform"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

	


typedef struct GSIResolveHostnameInfo
{
    char * hostname;
    unsigned int ip;

#if defined(_WIN32)  || defined(_UNIX) || defined (_REVOLUTION)
    int finishedResolving;
    GSIThreadID threadID;
#endif

} GSIResolveHostnameInfo;

#if	(defined(_WIN32) ||  defined(_UNIX) || defined (_REVOLUTION)) && !defined(GSI_NO_THREADS) && !defined(GSI_NO_ASYNC_DNS)

#if defined(_WIN32) 
	#if defined(_WIN32)
		DWORD WINAPI gsiResolveHostnameThread(void * arg)
	#endif
	{
		HOSTENT * hostent;
		GSIResolveHostnameHandle handle = (GSIResolveHostnameHandle)arg;

		SocketStartUp();
		
		#ifdef SN_SYSTEMS
		sockAPIregthr();
		#endif

		hostent = gethostbyname(handle->hostname);
		if(hostent)
		{
			handle->ip = *(unsigned int *)hostent->h_addr_list[0];
		}
		else
		{
			handle->ip = GSI_ERROR_RESOLVING_HOSTNAME;
		}

		SocketShutDown();

		handle->finishedResolving = 1;

		#ifdef SN_SYSTEMS
		sockAPIderegthr();
		#endif

		gsiExitThread(handle->threadID);

		#if defined(_WIN32)
		return 0;
		#endif
}
#endif 


#ifdef _REVOLUTION
static void *gsiResolveHostnameThread(void * arg)
{
	static GSICriticalSection aHostnameCrit;
	static int aInitialized = 0;
	HOSTENT *aHostAddr;
	GSIResolveHostnameHandle handle = (GSIResolveHostnameHandle)arg;
		
	if (!aInitialized)
	{
		gsiInitializeCriticalSection(&aHostnameCrit);
		aInitialized = 1;
	}
	gsiEnterCriticalSection(&aHostnameCrit);
	
	aHostAddr = gethostbyname(handle->hostname);
	if (aHostAddr != 0)
	{
		char * ip;
		ip = inet_ntoa(*(in_addr *)aHostAddr->addrList[0]);

		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment,
				"Resolved host '%s' to ip '%s'\n", handle->hostname, ip);
		
		handle->ip = inet_addr(ip);
	}
	else
	{
		handle->ip = GSI_ERROR_RESOLVING_HOSTNAME;
	}
	
	
	handle->finishedResolving = 1;
	
	gsiLeaveCriticalSection(&aHostnameCrit);	
}
#endif 

#if defined(_UNIX)
static void gsiResolveHostnameThread(void * arg)
{
	GSIResolveHostnameHandle handle = (GSIResolveHostnameHandle)arg;
	struct addrinfo hints, *result = NULL;
	int error;
	char *ip;

	SocketStartUp();

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	error = getaddrinfo(handle->hostname, "http", &hints, &result);

	if (!error)
	{
		ip = inet_ntoa((*(struct sockaddr_in*)result->ai_addr).sin_addr);

		gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment,
			"Resolved host '%s' to ip '%s'\n", handle->hostname, ip);

		handle->ip = inet_addr(ip);

		freeaddrinfo(result);
	}
	else
	{
		handle->ip = GSI_ERROR_RESOLVING_HOSTNAME;
	}

	SocketShutDown();

	handle->finishedResolving = 1;

	gsiExitThread(handle->threadID);
}
#endif 


int gsiStartResolvingHostname(const char * hostname, GSIResolveHostnameHandle * handle)
{
	GSIResolveHostnameInfo * info;

#if defined(_PS2)
	GS_ASSERT_STR(gsi_false, "PS2 Threading unsupported in current version of the SDK\n");
#endif

	info = (GSIResolveHostnameInfo *)gsimalloc(sizeof(GSIResolveHostnameInfo));
	if(!info)
		return -1;

	info->hostname = goastrdup(hostname);
	if(!info->hostname)
	{
		gsifree(info);
		return -1;
	}

	info->finishedResolving = 0;

	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_State, GSIDebugLevel_Comment, 
		"(Asynchrounous) DNS lookup starting\n");

	if(gsiStartThread(gsiResolveHostnameThread, (0x1000), info, &info->threadID) == -1)
	{
		gsifree(info->hostname);
		info->hostname = NULL;
		gsifree(info);
		info = NULL;
		return -1;
	}

	*handle = info;

	return 0;
}

void gsiCancelResolvingHostname(GSIResolveHostnameHandle handle)
{
	gsiCancelThread(handle->threadID);

    if (handle->hostname)
    {
	    gsifree(handle->hostname);
	    handle->hostname = NULL;
    }
	gsifree(handle);
	handle = NULL;
}

unsigned int gsiGetResolvedIP(GSIResolveHostnameHandle handle)
{
	unsigned int ip;

	if(!handle->finishedResolving)
		return GSI_STILL_RESOLVING_HOSTNAME;

	ip = handle->ip;

	gsiCleanupThread(handle->threadID);
	gsifree(handle->hostname);
	gsifree(handle);
    handle = NULL;

	return ip;
}


#else	



int gsiStartResolvingHostname(const char * hostname, GSIResolveHostnameHandle * handle)
{
	GSIResolveHostnameInfo * info;
	HOSTENT * hostent;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, 
		"(NON-Asynchrounous) DNS lookup starting\n");
	
	hostent = gethostbyname(hostname);
	if(hostent == NULL)
		return -1;

	info = (GSIResolveHostnameHandle)gsimalloc(sizeof(GSIResolveHostnameInfo));
	if(!info)
		return -1;

	info->ip = *(unsigned int *)hostent->h_addr_list[0];

	*handle = info;

	return 0;
}

void gsiCancelResolvingHostname(GSIResolveHostnameHandle handle)
{
	gsifree(handle);
    handle = NULL;
}

unsigned int gsiGetResolvedIP(GSIResolveHostnameHandle handle)
{
	unsigned int ip = handle->ip;
	gsifree(handle);
    handle = NULL;
	return ip;
}

#endif 


char * goastrdup(const char *src)
{
	char *res;
	if(src == NULL)      
		return NULL;
	res = (char *)gsimalloc(strlen(src) + 1);
	if(res != NULL)      
		strcpy(res, src);
	return res;
}

unsigned short * goawstrdup(const unsigned short *src)
{
	unsigned short *res;
	if(src == NULL)      
		return NULL;
	res = (unsigned short *)gsimalloc((wcslen((wchar_t*)src) + 1) * sizeof(unsigned short));
	if(res != NULL)      
		wcscpy((wchar_t*)res, (const wchar_t*)src);
	return res;
}

#if !defined(_WIN32)

char *_strlwr(char *string)
{
	char *hold = string;
	while (*string)
	{
		*string = (char)tolower(*string);
		string++;
	}

	return hold;
}

char *_strupr(char *string)
{
	char *hold = string;
	while (*string)
	{
		*string = (char)toupper(*string);
		string++;
	}

	return hold;
}
#endif


void SocketStartUp()
{
#if defined(_WIN32) 
	WSADATA data;

	#if defined(_X360)
		XNetStartupParams xnsp;
		memset(&xnsp,0,sizeof(xnsp));
		xnsp.cfgSizeOfStruct=sizeof(xnsp);
		xnsp.cfgFlags=XNET_STARTUP_BYPASS_SECURITY;
		if(0 != XNetStartup(&xnsp))
		{
			OutputDebugString("XNetStartup failed\n");
		}
	#endif

	#if (!defined(_XBOX) || defined(_X360)) && (defined(GSI_WINSOCK2) || defined(_X360))
		WSAStartup(MAKEWORD(2,2), &data);
	#else
		WSAStartup(MAKEWORD(1,1), &data);
	#endif
#endif
}

void SocketShutDown()
{
#if defined(_WIN32)
	WSACleanup();
	#if defined(_X360)
		XNetCleanup();
	#endif
#endif
}


#ifdef _PS2
extern int sceCdReadClock();

#if !defined(__MWERKS__) && !defined(_PS2)
typedef unsigned char u_char;
#endif

typedef struct {
        u_char stat;            
        u_char second;          
        u_char minute;          
        u_char hour;            

        u_char pad;             
        u_char day;             
        u_char month;           
        u_char year;            
} sceCdCLOCK;

static unsigned long GetTicks()
{
	unsigned long ticks;
	asm volatile (" mfc0 %0, $9 " : "=r" (ticks));
    return ticks;
}

#define DEC(x) (10*(x/16)+(x%16))
#define _BASE_YEAR 			 70L
#define _MAX_YEAR 			138L
#define _LEAP_YEAR_ADJUST 	 17L
int _days[] = {-1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364};

static time_t _gmtotime_t (
        int yr,     
        int mo,     
        int dy,     
        int hr,
        int mn,
        int sc
        )
{
        int tmpdays;
        long tmptim;
        struct tm tb;

        if ( ((long)(yr -= 1900) < _BASE_YEAR) || ((long)yr > _MAX_YEAR) )
                return (time_t)(-1);

        tmpdays = dy + _days[mo - 1];
        if ( !(yr & 3) && (mo > 2) )
                tmpdays++;

        tmptim = (long)yr - _BASE_YEAR;

        tmptim = ( ( ( ( tmptim ) * 365L
                 + ((long)(yr - 1) >> 2) - (long)_LEAP_YEAR_ADJUST
                 + (long)tmpdays )
                 * 24L + (long)hr )
                 * 60L + (long)mn )
                 * 60L + (long)sc;

        tb.tm_yday = tmpdays;
        tb.tm_year = yr;
        tb.tm_mon = mo - 1;
        tb.tm_hour = hr;
        
        return (tmptim >= 0) ? (time_t)tmptim : (time_t)(-1);
}

time_t time(time_t *timer)
{
	time_t tim;
	sceCdCLOCK clocktime; 

	sceCdReadClock(&clocktime); 

	tim =   _gmtotime_t ( DEC(clocktime.year)+2000,
							DEC(clocktime.month),
							DEC(clocktime.day),
							DEC(clocktime.hour),
							DEC(clocktime.minute),
							DEC(clocktime.second));

	if(timer)
		*timer = tim;
		
	return tim;
}

#endif 


gsi_time current_time()  
{ 
#if defined(_WIN32)
	return (GetTickCount()); 

#elif defined(_PS2)
	unsigned int ticks;
	static unsigned int msec = 0;
	static unsigned int lastticks = 0;
	sceCdCLOCK lasttimecalled; 

	if(!msec)
	{
		sceCdReadClock(&lasttimecalled); 
		msec =  (unsigned int)(DEC(lasttimecalled.day) * 86400000) +
				(unsigned int)(DEC(lasttimecalled.hour) * 3600000) +
				(unsigned int)(DEC(lasttimecalled.minute) * 60000) +
				(unsigned int)(DEC(lasttimecalled.second) * 1000);
	}

	ticks = (unsigned int)GetTicks();
	if(lastticks > ticks)
		msec += (unsigned int)(((unsigned int)(-1) - lastticks) + ticks) / 300000;
	else
		msec += (unsigned int)(ticks-lastticks) / 300000;
	lastticks = ticks;

	return msec;

#elif defined(_UNIX)
	struct timeval time;
	
	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000 + time.tv_usec / 1000);

#elif defined(_NITRO)
	assert(OS_IsTickAvailable() == TRUE);
	return (gsi_time)OS_TicksToMilliSeconds(OS_GetTick());

#elif defined(_PSP)
	struct SceRtcTick ticks;
	int result = 0;

	result = sceRtcGetCurrentTick(&ticks);
	if (result < 0)
	{
		ScePspDateTime time;
		result = sceRtcGetCurrentClock(&time, 0);
		if (result < 0)
			return 0; 
		result = sceRtcGetTick(&time, &ticks);
		if (result < 0)
			return 0; 
	}

	return (gsi_time)(ticks.tick / 1000);

#elif defined(_PS3)
	return (gsi_time)(sys_time_get_system_time()/1000);

#elif defined(_REVOLUTION)
	OSTick aTickNow= OSGetTick();
	gsi_time aMilliseconds = (gsi_time)OSTicksToMilliseconds(aTickNow);
	return aMilliseconds;
#else
	assert(0);
#endif
	
}

gsi_time current_time_hires()  
{
#ifdef _WIN32
#if (!defined(_M_IX86) || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 64))
	static LARGE_INTEGER counterFrequency;
	static BOOL haveCounterFrequency = FALSE;
	static BOOL haveCounter = FALSE;
	LARGE_INTEGER count;

	if(!haveCounterFrequency)
	{
		haveCounter = QueryPerformanceFrequency(&counterFrequency);
		haveCounterFrequency = TRUE;
	}

	if(haveCounter)
	{
		if(QueryPerformanceCounter(&count))
		{
			return (gsi_time)(count.QuadPart * 1000000 / counterFrequency.QuadPart);
		}
	}
#endif
	
	return (current_time() / 1000);
#endif

#ifdef _PS2
	unsigned int ticks;
	static unsigned int msec = 0;
	static unsigned int lastticks = 0;
	sceCdCLOCK lasttimecalled; 

	if(!msec)
	{
		sceCdReadClock(&lasttimecalled); 
		msec =  (unsigned int)(DEC(lasttimecalled.day) * 86400000) +
				(unsigned int)(DEC(lasttimecalled.hour) * 3600000) +
				(unsigned int)(DEC(lasttimecalled.minute) * 60000) +
				(unsigned int)(DEC(lasttimecalled.second) * 1000);
		msec *= 1000;
	}

	ticks = (unsigned int)GetTicks();
	if(lastticks > ticks)
		msec += ((sizeof(unsigned int) - lastticks) + ticks) / 300;
	else
		msec += (unsigned int)(ticks-lastticks) / 300;
	lastticks = ticks;

	return msec;
#endif

#ifdef _PSP
	struct SceRtcTick ticks;
	int result = 0;

	result = sceRtcGetCurrentTick(&ticks);
	if (result < 0)
	{
		ScePspDateTime time;
		result = sceRtcGetCurrentClock(&time, 0);
		if (result < 0)
			return 0; 
		result = sceRtcGetTick(&time, &ticks);
		if (result < 0)
			return 0; 
	}

	return (gsi_time)(ticks.tick);
#endif

#ifdef _UNIX
	struct timeval time;
	
	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000000 + time.tv_usec);
#endif

#ifdef _NITRO
	assert(OS_IsTickAvailable() == TRUE);
	return (gsi_time)OS_TicksToMicroSeconds(OS_GetTick());
#endif

#ifdef _PS3
	return (gsi_time)sys_time_get_system_time();
#endif
}


void msleep(gsi_time msec)
{
#if defined(_WIN32)
	Sleep(msec);

#elif defined(_PS2)
	#ifdef SN_SYSTEMS
		sn_delay((int)msec);
	#endif
	#ifdef EENET
		if(msec >= 1000)
		{
			sleep(msec / 1000);
			msec -= (msec / 1000);
		}
		if(msec)
			usleep(msec * 1000);
	#endif
	#ifdef INSOCK
		DelayThread(msec * 1000);
	#endif

#elif defined(_PSP)
	sceKernelDelayThread(msec * 1000);

#elif defined(_UNIX)
	usleep(msec * 1000);

#elif defined(_NITRO)
	OS_Sleep(msec);

#elif defined(_PS3)
	sys_timer_usleep(msec* 1000);
#elif defined (_REVOLUTION)
	OSSleepMilliseconds(msec);
#else
	assert(0); 
#endif
}

#if defined(_NITRO) || defined(_REVOLUTION)

#define MKTIME_ERROR	  (time_t)(-1)

#define DAY_SEC           (24L * 60L * 60L)    
#define YEAR_SEC          (365L * DAY_SEC)    
#define FOUR_YEAR_SEC     (1461L * DAY_SEC)   
#define DEC_SEC           315532800L           
#define BASE_DOW          4                    
#define BASE_YEAR         70L                  
#define LEAP_YEAR_ADJUST  17L                  
#define MAX_YEAR          138L                 

#define ChkAdd(dest, src1, src2)   ( ((src1 >= 0L) && (src2 >= 0L) \
    && (dest < 0L)) || ((src1 < 0L) && (src2 < 0L) && (dest >= 0L)) )

#define ChkMul(dest, src1, src2)   ( src1 ? (dest/src1 != src2) : 0 )

int _lpdays[] = { -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
int _days[] = { -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 };

const char _dnames[] = { "SunMonTueWedThuFriSat" };
const char _mnames[] = { "JanFebMarAprMayJunJulAugSepOctNovDec" };

static struct tm tb = { 0 };    

static char buf[26];			


static char * store_dt(char *, int);
static char * store_dt(char *p, int val)
{
        *p++ = (char)(_T('0') + val / 10);
        *p++ = (char)(_T('0') + val % 10);
        return(p);
}
#endif 

struct tm * gsiSecondsToDate(const time_t *timp)
{
#if !defined(_NITRO) && !defined(_REVOLUTION)

	return gmtime(timp);

#else
	time_t caltim = *timp;            
	int islpyr = 0;                 
	int tmptim;
	int *mdays;                
	struct tm *ptb = &tb;


	if ( caltim < 0L )
		return(NULL);

	tmptim = (int)(caltim / FOUR_YEAR_SEC);
	caltim -= ((long)tmptim * FOUR_YEAR_SEC);

	tmptim = (tmptim * 4) + 70;         

	if ( caltim >= YEAR_SEC ) 
	{
		tmptim++;                       
		caltim -= YEAR_SEC;

		if ( caltim >= YEAR_SEC ) 
		{
			tmptim++;                   
			caltim -= YEAR_SEC;

			if ( caltim >= (YEAR_SEC + DAY_SEC) ) 
			{
				tmptim++;           
				caltim -= (YEAR_SEC + DAY_SEC);
			}
			else 
			{
				islpyr++;
			}
		}
	}

	ptb->tm_year = tmptim;

	ptb->tm_yday = (int)(caltim / DAY_SEC);
	caltim -= (long)(ptb->tm_yday) * DAY_SEC;

	if ( islpyr )
		mdays = _lpdays;
	else
		mdays = _days;


	for ( tmptim = 1 ; mdays[tmptim] < ptb->tm_yday ; tmptim++ ) ;

	ptb->tm_mon = --tmptim;

	ptb->tm_mday = ptb->tm_yday - mdays[tmptim];

	ptb->tm_wday = ((int)(*timp / DAY_SEC) + BASE_DOW) % 7;

	ptb->tm_hour = (int)(caltim / 3600);
	caltim -= (long)ptb->tm_hour * 3600L;

	ptb->tm_min = (int)(caltim / 60);
	ptb->tm_sec = (int)(caltim - (ptb->tm_min) * 60);

	ptb->tm_isdst = 0;
	return( (struct tm *)ptb );
#endif
}

time_t gsiDateToSeconds(struct tm *tb)
{
#if !defined(_NITRO) && !defined(_REVOLUTION)

	return mktime(tb);

#else
	time_t tmptm1, tmptm2, tmptm3;
	struct tm *tbtemp;
	if ( ((tmptm1 = tb->tm_year) < BASE_YEAR - 1) || (tmptm1 > MAX_YEAR + 1) )
		return MKTIME_ERROR;



	if ( (tb->tm_mon < 0) || (tb->tm_mon > 11) ) {

		tmptm1 += (tb->tm_mon / 12);

		if ( (tb->tm_mon %= 12) < 0 ) {
			tb->tm_mon += 12;
			tmptm1--;
		}

		if ( (tmptm1 < BASE_YEAR - 1) || (tmptm1 > MAX_YEAR + 1) )
			return MKTIME_ERROR;
	}


	tmptm2 = _days[tb->tm_mon];
	if ( !(tmptm1 & 3) && (tb->tm_mon > 1) )
                tmptm2++;

    tmptm3 = (tmptm1 - BASE_YEAR) * 365L + ((tmptm1 - 1L) >> 2)
			 - LEAP_YEAR_ADJUST;

	tmptm3 += tmptm2;

	tmptm1 = tmptm3 + (tmptm2 = (long)(tb->tm_mday));
	if ( ChkAdd(tmptm1, tmptm3, tmptm2) )
		return MKTIME_ERROR;


	tmptm2 = tmptm1 * 24L;
	if ( ChkMul(tmptm2, tmptm1, 24L) )
		return MKTIME_ERROR;

	tmptm1 = tmptm2 + (tmptm3 = (long)tb->tm_hour);
	if ( ChkAdd(tmptm1, tmptm2, tmptm3) )
		return MKTIME_ERROR;



	tmptm2 = tmptm1 * 60L;
	if ( ChkMul(tmptm2, tmptm1, 60L) )
		return MKTIME_ERROR;

	tmptm1 = tmptm2 + (tmptm3 = (long)tb->tm_min);
	if ( ChkAdd(tmptm1, tmptm2, tmptm3) )
		return MKTIME_ERROR;



	tmptm2 = tmptm1 * 60L;
	if ( ChkMul(tmptm2, tmptm1, 60L) )
		return MKTIME_ERROR;

	tmptm1 = tmptm2 + (tmptm3 = (long)tb->tm_sec);
	if ( ChkAdd(tmptm1, tmptm2, tmptm3) )
		return MKTIME_ERROR;


	if ( (tbtemp = gsiSecondsToDate(&tmptm1)) == NULL )
		return MKTIME_ERROR;
        


	*tb = *tbtemp;
	return (time_t)tmptm1;
#endif
}

char * gsiSecondsToString(const time_t *timp)
{
#if !defined(_NITRO) && !defined(_REVOLUTION)

	return ctime(timp);

#else
	char *p = buf;
	int day, mon;
	int i;
	struct tm *ptm;

	ptm = gsiSecondsToDate(timp);	 

    p = buf;						 


	day = ptm->tm_wday * 3;          
	mon = ptm->tm_mon * 3;           

	for (i=0; i < 3; i++,p++) {
		*p = *(_dnames + day + i);
		*(p+4) = *(_mnames + mon + i);
	}

	*p = _T(' ');                   

	p += 4;

	*p++ = _T(' ');
	p = store_dt(p, ptm->tm_mday);   
	*p++ = _T(' ');
	p = store_dt(p, ptm->tm_hour);   
	*p++ = _T(':');
	p = store_dt(p, ptm->tm_min);    
	*p++ = _T(':');
	p = store_dt(p, ptm->tm_sec);    
	*p++ = _T(' ');
	p = store_dt(p, 19 + (ptm->tm_year/100)); 
	p = store_dt(p, ptm->tm_year%100);
	*p++ = _T('\n');
	*p = _T('\0');

	return ((char *) buf);
#endif
}


#define RANa 16807                 
#define LONGRAND_MAX 2147483647L   

static long randomnum = 1;

static long nextlongrand(long seed)
{
	unsigned

	long lo, hi;
	lo = RANa *(unsigned long)(seed & 0xFFFF);
	hi = RANa *((unsigned long)seed >> 16);
	lo += (hi & 0x7FFF) << 16;

	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}
	lo += hi >> 15;

	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}

	return(long)lo;
}

static long longrand(void)
{
	randomnum = nextlongrand(randomnum);
	return randomnum;
}

void Util_RandSeed(unsigned long seed)
{
	randomnum = seed ? (long)(seed & LONGRAND_MAX) : 1;
}

int Util_RandInt(int low, int high)
{
	unsigned int range = (unsigned int)high-low;
	int num;
	
	if (range == 0)
		return (low); 

	num = (int)(longrand() % range);

	return(num + low);
}



static void QuartToTrip(char *quart, char *trip, int inlen)
{
	if (inlen >= 2)
		trip[0] = (char)(quart[0] << 2 | quart[1] >> 4);
	if (inlen >= 3)
		trip[1] = (char)((quart[1] & 0x0F) << 4 | quart[2] >> 2);
	if (inlen >= 4)
		trip[2] = (char)((quart[2] & 0x3) << 6 | quart[3]);
}

static void TripToQuart(const char *trip, char *quart, int inlen)
{
	unsigned char triptemp[3];
	int i;
	for (i = 0; i < inlen ; i++)
	{
		triptemp[i] = (unsigned char)trip[i];
	}
	while (i < 3) 
	{
		triptemp[i] = 0;
		i++;
	}
	quart[0] = (char)(triptemp[0] >> 2);
	quart[1] = (char)(((triptemp[0] & 3) << 4) | (triptemp[1] >> 4));
	quart[2] = (char)((triptemp[1] & 0x0F) << 2 | (triptemp[2] >> 6));
	quart[3] = (char)(triptemp[2] & 0x3F);

}

const char defaultEncoding[] = {'+','/','='};
const char alternateEncoding[] = {'[',']','_'};
const char urlSafeEncodeing[] = {'-','_','='};

void B64Decode(const char *input, char *output, int inlen, int * outlen, int encodingType)
{
	const char *encoding = NULL;
	const char *holdin = input;
	int readpos = 0;
	int writepos = 0;
	char block[4];
	

	switch(encodingType)
	{	
		case 1: 
			encoding = alternateEncoding;
			break;
		case 2:
			encoding = urlSafeEncodeing;
			break;
		default: encoding = defaultEncoding;
	}

	GS_ASSERT(inlen >= 0);
	if (inlen <= 0)
	{
		if (outlen)
			*outlen = 0;
		output[0] = '\0';
		return;
	}

	while (readpos < inlen && input[readpos] != encoding[2])
	{
		if (input[readpos] >= '0' && input[readpos] <= '9')
			block[readpos%4] = (char)(input[readpos] - 48 + 52);
		else if (input[readpos] >= 'a' && input[readpos] <= 'z')
			block[readpos%4] = (char)(input[readpos] - 71);
		else if (input[readpos] >= 'A' && input[readpos] <= 'Z')
			block[readpos%4] = (char)(input[readpos] - 65);
		else if (input[readpos] == encoding[0])
			block[readpos%4] = 62;
		else if (input[readpos] == encoding[1])
			block[readpos%4] = 63;

		else if (input[readpos] == encoding[2])
			break;
		else if (input[readpos] == '\0')
			break;
		else 
		{
			if (outlen)
				*outlen = 0;
			output[0] = '\0';
			return; 
		}

		if (readpos%4==3) 
		{
			QuartToTrip(block, &output[writepos], 4);
			writepos += 3;
		}
		readpos++;
	}

	if ((readpos != 0) && (readpos%4 != 0))
	{
		memset(&block[readpos%4], encoding[2], (unsigned int)4-(readpos%4)); 
		QuartToTrip(block, &output[writepos], readpos%4);

		if (readpos%4 == 3)
			writepos += 2;
		else 
			writepos += 1;
	}

	if (outlen)
		*outlen = writepos;

	GSI_UNUSED(holdin);
}



void B64Encode(const char *input, char *output, int inlen, int encodingType)
{
	const char *encoding;
	char *holdout = output;
	char *lastchar;
	int todo = inlen;
	
	switch(encodingType)
	{	
		case 1: 
			encoding = alternateEncoding;
			break;
		case 2:
			encoding = urlSafeEncodeing;
			break;
		default: encoding = defaultEncoding;
	}
	
	while (todo > 0)
	{
		TripToQuart(input, output, min(todo, 3));
		output += 4;
		input += 3;
		todo -= 3;
	}
	lastchar = output;
	if (inlen % 3 == 1)
		lastchar -= 2;
	else if (inlen % 3 == 2)
		lastchar -= 1;
	*output = 0; 
	while (output > holdout)
	{
		output--;
		if (output >= lastchar) 
			*output = encoding[2];
		else if (*output <= 25)
			*output = (char)(*output + 65);
		else if (*output <= 51)
			*output = (char)(*output + 71);
		else if (*output <= 61)
			*output = (char)(*output + 48 - 52);
		else if (*output == 62)
			*output = encoding[0];
		else if (*output == 63)
			*output = encoding[1];
	} 
}

int B64DecodeLen(const char *input, int encodingType)
{
	const char *encoding;
	const char *holdin = input;

	switch(encodingType)
	{	
		case 1: 
			encoding = alternateEncoding;
			break;
		case 2:
			encoding = urlSafeEncodeing;
			break;
		default: encoding = defaultEncoding;
	}

	while (*input)
	{
		if (*input == encoding[2])
			return (input - holdin) / 4 * 3 + (input - holdin - 1) % 4;
		input++;
	}

	return (input - holdin) / 4 * 3;
}

void B64InitEncodeStream(B64StreamData *data, const char *input, int len, int encodingType)
{
	data->input = input;
	data->len = len;
	data->encodingType = encodingType;
}

gsi_bool B64EncodeStream(B64StreamData *data, char output[4])
{
	const char *encoding;
	char *c;
	int i;

	if(data->len <= 0)
		return gsi_false;
	
	switch(data->encodingType)
	{	
		case 1: 
			encoding = alternateEncoding;
			break;
		case 2:
			encoding = urlSafeEncodeing;
			break;
		default: encoding = defaultEncoding;
	}

	TripToQuart(data->input, output, min(data->len, 3));
	data->input += 3;
	data->len -= 3;

	for(i = 0 ; i < 4 ; i++)
	{
		c = &output[i];
		if (*c <= 25)
			*c = (char)(*c + 65);
		else if (*c <= 51)
			*c = (char)(*c + 71);
		else if (*c <= 61)
			*c = (char)(*c + 48 - 52);
		else if (*c == 62)
			*c = encoding[0];
		else if (*c == 63)
			*c = encoding[1];
	}

	if(data->len < 0)
	{
		output[3] = encoding[2];
		if(data->len == -2)
			output[2] = encoding[2];
	}

	return gsi_true;
}

void gsiPadRight(char *cArray, char padChar, int cLength);
char * gsiXxteaAlg(const char *sIn, int nIn, char key[XXTEA_KEY_SIZE], int bEnc, int *nOut);

void gsiPadRight(char *cArray, char padChar, int cLength)
{
	int diff;
	int length = (int)strlen(cArray);
	
	diff = cLength - length;
	memset(&cArray[length], padChar, (size_t)diff);
}


char * gsiXxteaAlg(const char *sIn, int nIn, char key[XXTEA_KEY_SIZE], int bEnc, int *nOut)
{
	int	i, p, n1;
	unsigned int *k, *v, z, y;
	char *oStr = NULL, *pStr = NULL;
	char *sIn2 = NULL;
	if (!sIn || !key[0] || nIn == 0)
		return NULL;
	
	if (nIn % 4 == 0)			
		nIn	= (nIn/4)+1;
	else
		nIn	= (nIn + 3)/4;

	if ( nIn <=	1 )		
		nIn	= 2;

	gsiPadRight( key , '\0', XXTEA_KEY_SIZE);
	k = (unsigned int *)key;

	sIn2 = (char *)gsimalloc((size_t)(4 * nIn));
	strcpy(sIn2, sIn);
	gsiPadRight( sIn2, '\0', 4*nIn);
	v = (unsigned int *)sIn2;

	n1 = nIn - 1;
	z = v[ n1 ];
	y = v[ 0 ];
	i = ( int )( 6 + 52/nIn );

	if (bEnc == 1)		
	{
		unsigned int sum = 0;
		while ( i-- != 0 ) 
		{
			int	e;
			sum += 0x9E3779B9;
			e = ( int )( sum >> 2 );
			for ( p = -1; ++p < nIn; ) 
			{
				y = v[( p < n1 ) ? p + 1 : 0 ];
				z = ( v[ p ] +=
					(	 (( z >> 5 ) ^ ( y << 2 ))
					+ (( y >> 3 ) ^ ( z << 4 )))
					^ (	 ( sum ^ y )
					+ ( k[( p ^ e ) & 3 ] ^ z )));
			}
		}
	}
	else if (bEnc == 0)			
	{
		unsigned int sum = ( unsigned int ) i * 0x9E3779B9;
		while ( sum != 0 ) 
		{
			int	e = ( int )( sum >> 2 );
			for ( p = nIn; p-- != 0; )
			{
				z = v[( p != 0 ) ? p - 1 : n1 ];
				y = ( v[ p ] -=
					(	 (( z >> 5 ) ^ ( y << 2 ))
					+ (( y >> 3 ) ^ ( z << 4 )))
					^ (	 ( sum ^ y )
					+ ( k[( p ^ e ) & 3 ] ^ z )));
			}
			sum -= 0x9E3779B9;
		}
	}
	else return NULL;
	
	
	oStr = (char *)gsimalloc((size_t)(4 * nIn + 1));
	pStr = oStr;
	*nOut = 4 *nIn;
	for ( i = -1; ++i < nIn; ) 
	{
		unsigned int q = v[ i ];
		
		*pStr++ = (char)(q & 0xFF);
		*pStr++ = (char)(( q >>  8 ) & 0xFF);
		*pStr++ = (char)(( q >> 16 ) & 0xFF);
		*pStr++ = (char)(( q >> 24 ) & 0xFF);
	}
	*pStr = '\0';
	gsifree(sIn2);

	return oStr;	
}


char * gsXxteaEncrypt(const char * iStr, int iLength, char key[XXTEA_KEY_SIZE], int *oLength)
{
	return gsiXxteaAlg( iStr, iLength, key, 1, oLength );
}


char * gsXxteaDecrypt(const char * iStr, int iLength, char key[XXTEA_KEY_SIZE], int *oLength)
{
	return gsiXxteaAlg( iStr, iLength, key, 0, oLength);
}


#if defined(_DEBUG)

void gsiCheckStack(void)
{
#if defined(_NITRO)
#if 1
	OS_CheckStack(OS_GetCurrentThread());
#elif 1
	static gsi_bool checkFailed = gsi_false;
	if(!checkFailed)
	{
		OSStackStatus status = OS_GetStackStatus(OS_GetCurrentThread());
		if(status != 0)
		{
			const char * reason;
			if(status == OS_STACK_OVERFLOW)
				reason = "OVERFLOW";
			else if(status == OS_STACK_ABOUT_TO_OVERFLOW)
				reason = "ABOUT TO OVERFLOW";
			else if(status == OS_STACK_UNDERFLOW)
				reason = "UNDERFLOW";
			else
				reason = "UNKOWN REASON";

			OS_TPrintf("STACK CHECK FAILED!: %s\n", reason);

			checkFailed = gsi_true;
		}
	}
#endif
#endif 
}
#endif 


#ifdef SN_SYSTEMS
int GOAGetLastError(SOCKET s)
{
	int val = 0;
	int soval = sizeof(val);
	if (0 != getsockopt(s,SOL_SOCKET,SO_ERROR,&val,&soval))
		return 0; 
	else
		return val;
}
#endif

#ifdef _NITRO
static const char * GOAGetUniqueID_Internal(void)
{
	static char keyval[17];
	u8 MAC[MAC_ALEN];

	if(keyval[0])
		return keyval;

	IP_GetMacAddr(NULL, MAC);

	sprintf(keyval, "%02X%02X%02X%02X%02X%02X0000",
		MAC[0] & 0xFF,
		MAC[1] & 0xFF,
		MAC[2] & 0xFF,
		MAC[3] & 0xFF,
		MAC[4] & 0xFF,
		MAC[5] & 0xFF);

	return keyval;
}
#endif


#ifdef _PS2 
#ifdef UNIQUEID

#if defined(EENET)

#include <net/if_dl.h>
#include <net/if_ether.h>

static const char * GetMAC(void)
{
	static struct sceEENetEtherAddr linkAddress;
	struct sceEENetIfname * interfaces;
	struct sceEENetIfname * interface;
	int num;
	int type;
	int len;
	int i;
	const unsigned char * MAC = NULL;

	sceEENetGetIfnames(NULL, &num);
	interfaces = (struct sceEENetIfname *)gsimalloc(num * sizeof(struct sceEENetIfname));
	if(!interfaces)
		return NULL;
	sceEENetGetIfnames(interfaces, &num);

	for(i = 0 ; i < num ; i++)
	{
		interface = &interfaces[i];

		len = sizeof(type);
		if(sceEENetGetIfinfo(interface->ifn_name, sceEENET_IFINFO_IFTYPE, &type, &len) != 0)
			continue;

		if(type != sceEENET_IFTYPE_ETHER)
			continue;

		len = sizeof(linkAddress);
		if(sceEENetGetIfinfo(interface->ifn_name, sceEENET_IFINFO_MACADDR, &linkAddress, &len) != 0)
			continue;
		MAC = linkAddress.ether_addr_octet;

		break;
	}

	gsifree(interfaces);

	return MAC;
}

#elif defined(SN_SYSTEMS)

	static const char * GetMAC(void)
	{
		static char MAC[6];
		int len = sizeof(MAC);
		int rcode;

		rcode = sndev_get_status(0, SN_DEV_STAT_MAC, MAC, &len);
		if((rcode != 0) || (len != 6))
			return NULL;

		return MAC;
	}

#elif defined(INSOCK)

	static const char * GetMAC(void)
	{
		static char MAC[16];
		extern sceSifMClientData gGSIInsockClientData;
		extern u_int             gGSIInsockSocketBuffer[NETBUFSIZE] __attribute__((aligned(64)));

		int result = sceInetInterfaceControl(&gGSIInsockClientData, &gGSIInsockSocketBuffer,
			                                 1, sceInetCC_GetHWaddr, MAC, sizeof(MAC));		
		if (result == sceINETE_OK)
			return MAC;

		return NULL;
	}

#endif

static const char * GOAGetUniqueID_Internal(void)
{
	static char keyval[17];
	const char * MAC;

	if(keyval[0])
		return keyval;

	MAC = GetMAC();
	if(!MAC)
	{
		static char errorMAC[6] = { 1, 2, 3, 4, 5, 6 };
		MAC = errorMAC;
	}

	sprintf(keyval, "%02X%02X%02X%02X%02X%02X0000",
		MAC[0] & 0xFF,
		MAC[1] & 0xFF,
		MAC[2] & 0xFF,
		MAC[3] & 0xFF,
		MAC[4] & 0xFF,
		MAC[5] & 0xFF);

	return keyval;
}

#endif 
#endif 


#if ((defined(_WIN32) && !defined(_XBOX)) || defined(_UNIX))

static void GenerateID(char *keyval)
{
	int i;
	const char crypttab[63] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
#ifdef _WIN32
	LARGE_INTEGER l1;
	UINT seed;
	if (QueryPerformanceCounter(&l1))
		seed = (l1.LowPart ^ l1.HighPart);
	else
		seed = 0;
	Util_RandSeed(seed ^ GetTickCount() ^ (unsigned long)time(NULL) ^ clock());
#else
	Util_RandSeed(time(NULL) ^ clock());
#endif
	for (i = 0; i < 19; i++)
		if (i == 4 || i == 9 || i == 14)
			keyval[i] = '-';
	else
		keyval[i] = crypttab[Util_RandInt(0, 62)];
	keyval[19] = 0;
}

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#ifdef _WIN32
#define REG_KEY	  "Software\\GameSpy\\GameSpy 3D\\Registration"
#endif

const char * GOAGetUniqueID_Internal(void)
{
	static char keyval[PATH_MAX] = "";
	unsigned int ret;

#ifdef _WIN32
	int docreate;
	HKEY thekey;
	DWORD thetype = REG_SZ;
	DWORD len = MAX_PATH;
	DWORD disp;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_ALL_ACCESS, &thekey) != ERROR_SUCCESS)
		docreate = 1;
	else
		docreate = 0;
	ret = RegQueryValueExA(thekey, (LPCSTR)"Crypt", 0, &thetype, (LPBYTE)keyval, &len);
#else
	FILE *f;
	f = fopen("id.bin","r");
	if (!f)
		ret = 0;
	else
	{
		ret = fread(keyval,1,19,f);
		keyval[ret] = 0;
		fclose(f);
	}
#endif

	if (ret != 0 || strlen(keyval) != 19)
	{
		GenerateID(keyval);
#ifdef _WIN32
		if (docreate)
		{
			ret = RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &thekey, &disp);
		}
		RegSetValueExA(thekey, (LPCSTR)"Crypt", 0, REG_SZ, (const LPBYTE)keyval, strlen(keyval)+1);
#else
		f = fopen("id.bin","w");
		if (f)
		{
			fwrite(keyval,1,19,f);
			fclose(f);
		} else
			keyval[0] = 0; 
#endif
	}

#ifdef _WIN32
	RegCloseKey(thekey);
#endif

	memmove(keyval + 4, keyval + 5, 4);
	memmove(keyval + 8, keyval + 10, 4);
	memmove(keyval + 12, keyval + 15, 4);
	keyval[16] = '\0';
	
	return keyval;
}

#endif

#ifdef _PSP
const char * GOAGetUniqueID_Internal(void);
#endif


#if (!defined(_PS2) && !defined(_PS3) && !defined(_XBOX) && !defined(_PSP)) || defined(UNIQUEID)
GetUniqueIDFunction GOAGetUniqueID = GOAGetUniqueID_Internal;
#endif

#if defined(__cplusplus)
}
#endif
