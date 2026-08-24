/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#if defined(_PSP)
#include "../gsCommon.h"


#if !defined(GSI_NO_THREADS)

static void gsiResolveHostnameThread(void * arg)
{
	int result = 0;
	int resolverID = 0;
	char buf[1024]; 
	in_addr addr;
	GSIResolveHostnameHandle * info = (GSIResolveHostnameHandle)arg;

	result = sceNetResolverCreate(&resolverID, buf, sizeof(buf));
	if (result < 0)
	{
		info->ip = GSI_ERROR_RESOLVING_HOSTNAME;
		return -1;
	}
	else
	{
		result = sceNetResolverStartNtoA(resolverID, info->hostname, &addr, &info->ip, GSI_RESOLVER_TIMEOUT, GSI_RESOLVER_RETRY);
		if (result < 0)
			info->ip = GSI_ERROR_RESOLVING_HOSTNAME;
		sceNetResolverDelete(resolverID);
	}
}

int gsiStartResolvingHostname(const char * hostname, GSIResolveHostnameHandle * handle)
{
	GSIResolveHostnameInfo * info;

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

	if(gsiStartThread(gsiResolveHostnameThread, (0x1000), info, &info->threadID) == -1)
	{
		gsifree(info->hostname);
		gsifree(info);
		return -1;
	}

	*handle = info;

	return 0;
}

void gsiCancelResolvingHostname(GSIResolveHostnameHandle handle)
{
	if (0 == handle->finishedResolving)
	{
		sceNetResolverStop(handle->resolverID); 
		gsiCancelThread(handle->threadID);
	}
}

unsigned int gsiGetResolvedIP(GSIResolveHostnameHandle handle)
{
	return handle->ip;
}

#endif 


#if defined(UNIQUEID)

static const char * GetMAC(void)
{
	static struct SceNetEtherAddr mac;
	int result = 0;

	result = sceNetGetLocalEtherAddr(&mac);
	if (result == 0)
		return (char *)&mac.data;
	else
		return NULL;
}

const char * GOAGetUniqueID_Internal(void)
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

#define GSI_MIN_I64       LONG_LONG_MIN
#define GSI_MAX_I64       LONG_LONG_MAX
#define GSI_MAX_U64       ULONG_LONG_MAX
#define FL_UNSIGNED   1       
#define FL_NEG        2       
#define FL_OVERFLOW   4       
#define FL_READDIGIT  8       

gsi_i64 gsiStringToInt64(const char *theNumberStr)
{
	const char *p;
	char c;
	gsi_i64 number;
	unsigned digval;
	gsi_u64 maxval;
	int flags = 0;
	int ibase = 10;
	
	p = theNumberStr; 
	number = 0;       

	c = *p++;            

	while ( isspace(c) )
		c = *p++;        

	 
	if (c == '-') {
		flags |= FL_NEG;    
		c = *p++;
	}
	else if (c == '+')
		c = *p++;        

	if (ibase == 0) 
	{
		if (c != '0')
			ibase = 10;
		else if (*p == 'x' || *p == 'X')
			ibase = 16;
		else
			ibase = 8;
	}

	if (ibase == 16) 
	{
		if (c == '0' && (*p == 'x' || *p == 'X')) {
			++p;
			c = *p++;    
		}
	}

	maxval = GSI_MAX_U64 / ibase;


	for (;;) 
	{    
		if ( isdigit(c) )
			digval = c - '0';
		else if ( isalpha(c) )
			digval = toupper(c) - 'A' + 10;
		else
			break;

		if (digval >= (unsigned)ibase)
			break;        

		flags |= FL_READDIGIT;


		if (number < maxval || (number == maxval &&
			(gsi_u64)digval <= GSI_MAX_U64 % ibase)) 
		{
				number = number * ibase + digval;
		}
		else 
		{
			flags |= FL_OVERFLOW;
			break;
		}

		c = *p++;        
	}

	--p;                
	
	if (!(flags & FL_READDIGIT)) {
		number = 0L;        
	}
	else if ( (flags & FL_OVERFLOW) ||
		( !(flags & FL_UNSIGNED) &&
		( ( (flags & FL_NEG) && (number > GSI_MIN_I64) ) ||
		( !(flags & FL_NEG) && (number > GSI_MAX_I64) ) ) ) )
	{
		errno = ERANGE;
		if ( flags & FL_NEG )
			number = GSI_MIN_I64;
		else
			number = GSI_MAX_I64;
	}
	
	if (flags & FL_NEG)
		number = -number;

	return number;            
}

void gsiInt64ToString(char theNumberStr[33], gsi_i64 theNumber)
{
	GS_ASSERT(theNumberStr != NULL);

	sprintf(theNumberStr, "%lld", theNumber);
}
#endif 
