///////////////////////////////////////////////////////////////////////////////
#include "../gscommon.h"


#if defined(INSOCK)
#define INSOCK_MAX_UDP_BUFSIZE 8000000  
#define INSOCK_MAX_TCP_BUFSIZE 32000

extern sceSifMClientData gGSIInsockClientData;
extern u_int             gGSIInsockSocketBuffer[NETBUFSIZE] __attribute__((aligned(64)));

int SetReceiveBufferSize(SOCKET sock, int size)
{return -1; GSI_UNUSED(sock); GSI_UNUSED(size); }

int SetSendBufferSize(SOCKET sock, int size)
{return -1; GSI_UNUSED(sock); GSI_UNUSED(size); }

int GetReceiveBufferSize(SOCKET sock)
{return NETBUFSIZE; GSI_UNUSED(sock); }
	
int GetSendBufferSize(SOCKET sock)
{return NETBUFSIZE; GSI_UNUSED(sock); }

int GSISocketSelect(SOCKET theSocket, int* theReadFlag, int* theWriteFlag, int* theExceptFlag)
{
	int result = 0;
	sceInetPollFd_t aPollFdSet;

	if ((theReadFlag   != NULL))
		*theReadFlag = 0;
	if ((theWriteFlag  != NULL))
		*theWriteFlag = 0;
	if ((theExceptFlag != NULL))
		*theExceptFlag = 0;

	aPollFdSet.cid     = theSocket;  
	aPollFdSet.events  = 0;          
	aPollFdSet.revents = 0;          

	if (theReadFlag   != NULL) aPollFdSet.events |= sceINET_POLLIN;
	if (theWriteFlag  != NULL) aPollFdSet.events |= sceINET_POLLOUT;
	if (theExceptFlag != NULL) aPollFdSet.events |= sceINET_POLLERR;

	result = sceInsockPoll(&aPollFdSet, 1, 0);
	if (result > 0)
	{
		if ((theReadFlag   != NULL))
			*theReadFlag    = (aPollFdSet.revents & sceINET_POLLIN)  ? 1:0;
		if ((theWriteFlag  != NULL))
			*theWriteFlag   = (aPollFdSet.revents & sceINET_POLLOUT) ? 1:0;
		if ((theExceptFlag != NULL))
			*theExceptFlag  = (aPollFdSet.revents & sceINET_POLLERR) ? 1:0;
	}
	return result;	
}

int gsiShutdown(SOCKET s, int how)
{
	sceInsockSetShutdownTimeout(s, 30);
	return sceInsockShutdown(s, how);
}
#endif