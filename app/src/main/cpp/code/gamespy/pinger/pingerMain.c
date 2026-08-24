/*
GameSpy Ping SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/


#include <limits.h>
#include "pinger.h"
#include "../darray.h"

#define PI_MAGIC             0x91
#define PI_VERSION           0x01

#define PI_TRIP2_TIMEOUT     10000

#define PI_DATA_MAX_LEN      (PINGER_UDP_PING_SIZE - sizeof(piUDPPing))

typedef unsigned char  uint8;
typedef unsigned short uint16;

typedef struct piUDPPing
{
	uint8  magic;    
	uint8  version;  
	uint16 trip;     
	uint16 ID_A;     
	uint16 ID_B;     
} piUDPPing;

typedef struct piActivePing
{
	PINGERBool     originator;    
	uint16         ID;            
	uint16         expectedTrip;  
	gsi_time         timestamp;     
	gsi_time         timeout;       
	unsigned int          remoteIP;      
	unsigned short        remotePort;    
	pingerGotPing  reply;         
	void *         replyParam;    
} piActivePing;

typedef struct piQueuedCallback
{
	unsigned int IP;
	unsigned short port;
	int ping;
	char * data;
	int len;
	void * param;
	pingerGotPing callback;
} piQueuedCallback;

static PINGERBool       piInitialized = PINGERFalse;
static SOCKET           piSocket = INVALID_SOCKET;
static pingerGotPing    piPingerPinged;
static void *           piPingerPingedParam;
static pingerSetData    piPingerSetData;
static void *           piPingerSetDataParam;
static PINGERBool       piSettingData;
static PINGERBool       piUDPEnabled;
static uint16           piNextID;
static DArray           piActivePingList;
static gsi_time    piLastThinkTime;
static DArray           piCallbacks;

static uint16 piGetNextID(void)
{
	uint16 ID = piNextID;

	if(piNextID == USHRT_MAX)
		piNextID = 1;
	else
		piNextID++;

	return ID;
}

static PINGERBool piBytesToPing(unsigned char * buffer, piUDPPing * udpPing, char * data)
{
	assert(buffer != NULL);
	assert(udpPing != NULL);
	assert(data != NULL);

	udpPing->magic = *buffer++;

	udpPing->version = *buffer++;

	udpPing->trip = (unsigned short)(*buffer++ << 8);
	udpPing->trip |= *buffer++;

	udpPing->ID_A = (unsigned short)(*buffer++ << 8);
	udpPing->ID_A |= *buffer++;

	udpPing->ID_B = (unsigned short)(*buffer++ << 8);
	udpPing->ID_B |= *buffer++;

	// JED 12/18/00 15:20, again from 24.156.198.26
	//assert(udpPing->magic == PI_MAGIC);
	if(udpPing->magic != PI_MAGIC)
		return PINGERFalse;

	assert(udpPing->version == PI_VERSION);
	if(udpPing->version != PI_VERSION)
		return PINGERFalse;

	assert((udpPing->trip >= 1) && (udpPing->trip <= 3));
	if((udpPing->trip < 1) || (udpPing->trip > 3))
		return PINGERFalse;

	memcpy(data, buffer, PI_DATA_MAX_LEN);

	return PINGERTrue;
}

static void piPingToBytes(piUDPPing * udpPing, unsigned char * buffer)
{
	assert(udpPing != NULL);
	assert(buffer != NULL);

	*buffer++ = udpPing->magic;

	*buffer++ = udpPing->version;

	*buffer++ = (unsigned char)((udpPing->trip & 0xFF00) >> 8);
	*buffer++ = (unsigned char)(udpPing->trip & 0x00FF);

	*buffer++ = (unsigned char)((udpPing->ID_A & 0xFF00) >> 8);
	*buffer++ = (unsigned char)(udpPing->ID_A & 0x00FF);

	*buffer++ = (unsigned char)((udpPing->ID_B & 0xFF00) >> 8);
	*buffer++ = (unsigned char)(udpPing->ID_B & 0x00FF);
}

static PINGERBool piSendPing(SOCKADDR_IN * to, uint16 trip, uint16 ID_A, uint16 ID_B, const char * data)
{
	unsigned char buffer[PINGER_UDP_PING_SIZE];
	piUDPPing udpPing;
	int rcode;

	assert(to != NULL);
	assert((trip >= 1) && (trip <= 3));
	assert(!((trip == 2) && (ID_A == 0)));

	udpPing.magic = PI_MAGIC;
	udpPing.version = PI_VERSION;
	udpPing.trip = trip;
	udpPing.ID_A = ID_A;
	udpPing.ID_B = ID_B;
	piPingToBytes(&udpPing, buffer);
	if(data != NULL)
		memcpy(buffer + sizeof(piUDPPing), data, PI_DATA_MAX_LEN);
	else
		memset(buffer + sizeof(piUDPPing), 0, PI_DATA_MAX_LEN);

	rcode = sendto(piSocket, (char *)buffer, PINGER_UDP_PING_SIZE, 0, (SOCKADDR *)to, sizeof(SOCKADDR_IN));
	if(rcode != PINGER_UDP_PING_SIZE)
		return PINGERFalse;

	return PINGERTrue;
}

static PINGERBool piSocketInit(const char * localAddress,
							   unsigned short localPort)
{
	int rcode;
	SOCKADDR_IN sockaddr;
	int bFlag;

	assert(localPort != 0);

	SocketStartUp();

	memset(&sockaddr, 0, sizeof(SOCKADDR_IN));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(localPort);
	if(localAddress != NULL)
	{
		unsigned int IP;

		IP = inet_addr(localAddress);
		if(IP == INADDR_NONE)
		{
			HOSTENT * hostent;

			hostent = gethostbyname(localAddress);
			if(hostent == NULL)
			{
				assert(0);
				return PINGERFalse;
			}

			assert(IP != 0);
			IP = *(unsigned int *)hostent->h_addr_list[0];
		}

		sockaddr.sin_addr.s_addr = IP;
	}
	else
	{
		sockaddr.sin_addr.s_addr = INADDR_ANY;
	}

	piSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(piSocket == INVALID_SOCKET)
	{
		assert(0);
		return PINGERFalse;
	}

	bFlag = 1;
#ifndef INSOCK   
	rcode = setsockopt(piSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&bFlag, sizeof(bFlag) );
#endif
	/*if(gsiSocketIsError(rcode))
	{
		iLastError = WSAGetLastError();
		assert(0);
		return PINGERFalse;
	}*/

	rcode = bind(piSocket, (SOCKADDR *)&sockaddr, sizeof(SOCKADDR_IN));
	if (gsiSocketIsError(rcode))
	{
		assert(0);
		return PINGERFalse;
	}

	return PINGERTrue;
}

static void piQueueCallback
(
	unsigned int IP,
	unsigned short port,
	int ping,
	const char * data,
	int len,
	void * param,
	pingerGotPing callbackFunc
)
{
	piQueuedCallback callback;

	assert(callbackFunc);
	if(!callbackFunc)
		return;

	callback.IP = IP;
	callback.port = port;
	callback.ping = ping;
	callback.len = len;
	callback.param = param;
	callback.callback = callbackFunc;

	if(data)
	{
		callback.data = (char *)gsimalloc((unsigned int)len);
		if(!callback.data)
			return;
		memcpy(callback.data, data, (unsigned int)len);
	}
	else
	{
		callback.data = NULL;
	}

	ArrayAppend(piCallbacks, &callback);
}

static void piCallCallbacks(void)
{
	piQueuedCallback * callback;
	piQueuedCallback callbackCopy;

	while(ArrayLength(piCallbacks) > 0)
	{
		callback = (piQueuedCallback *)ArrayNth(piCallbacks, 0);
		assert(callback);
		if(!callback)
			return;

		memcpy(&callbackCopy, callback, sizeof(piQueuedCallback));
		callback = &callbackCopy;

		ArrayDeleteAt(piCallbacks, 0);

		callback->callback(callback->IP, callback->port, callback->ping, callback->data, callback->len, callback->param);

		gsifree(callback->data);
	}
}

PINGERBool pingerInit(const char * localAddress,
					  unsigned short localPort,
					  pingerGotPing pinged,
					  void * pingedParam,
					  pingerSetData setData,
					  void * setDataParam)
{
	assert(!piInitialized);

#if !defined(_NITRO)
	assert(PINGER_UDP_PING_SIZE >= sizeof(piUDPPing));
#endif

	if(piInitialized)
		return PINGERFalse;

	if((localAddress != NULL) && (localAddress[0] == '\0'))
		localAddress = NULL;

	piPingerPinged = pinged;
	piPingerPingedParam = pingedParam;
	piPingerSetData = setData;
	piPingerSetDataParam = setDataParam;
	piSettingData = PINGERFalse;
	piUDPEnabled = (PINGERBool)(localPort != 0);
	piNextID = 1;
	piLastThinkTime = 0;

	piActivePingList = ArrayNew(sizeof(piActivePing), 0, NULL);
	if(piActivePingList == NULL)
		return PINGERFalse;

	piCallbacks = ArrayNew(sizeof(piQueuedCallback), 0, NULL);
	if(!piCallbacks)
	{
		ArrayFree(piActivePingList);
		return PINGERFalse;
	}

	if(piUDPEnabled)
	{
		if(!piSocketInit(localAddress, localPort))
		{
			assert(0);

			if(piSocket != INVALID_SOCKET)
			{
				closesocket(piSocket);
				piSocket = INVALID_SOCKET;
			}

			SocketShutDown();

			ArrayFree(piActivePingList);
			ArrayFree(piCallbacks);

			return PINGERFalse;
		}
	}

	piInitialized = PINGERTrue;

	return PINGERTrue;
}

void pingerShutdown(void)
{
	if(!piInitialized)
		return;

	if(piSettingData)
		return;

	if(piSocket != INVALID_SOCKET)
	{
		closesocket(piSocket);
		piSocket = INVALID_SOCKET;
	}

	SocketShutDown();

	ArrayFree(piActivePingList);

	ArrayFree(piCallbacks);

	piInitialized = PINGERFalse;
}

static int GS_STATIC_CALLBACK piFindActivePingCompareFn(const void *elem1, const void *elem2)
{
	piActivePing * activePing1 = (piActivePing *)elem1;
	piActivePing * activePing2 = (piActivePing *)elem2;
	assert(activePing1 != NULL);
	assert(activePing2 != NULL);
	assert(activePing1->ID != 0);
	assert(activePing2->ID != 0);

	return (activePing1->ID - activePing2->ID);
}

static piActivePing * piFindActivePing(uint16 ID, int * index)
{
	piActivePing key;
	int n;

	key.ID = ID;
	n = ArraySearch(piActivePingList, &key, piFindActivePingCompareFn, 0, 0);
	if(index != NULL)
		*index = n;

	if(n == NOT_FOUND)
		return NULL;

	return (piActivePing *)ArrayNth(piActivePingList, n);
}

static int piCalculatePing(gsi_time sendTime, gsi_time recvTime)
{
	int ping;

	ping = (int)(recvTime - sendTime);

	if(piLastThinkTime != 0)
	{
#if 0
		ping -= ((int)(recvTime - piLastThinkTime) / 2);
		if(ping < 0)
			ping = 0;
#endif
	}

	return ping;
}

static void piProcessTrip1(piUDPPing * udpPing, const char * data, SOCKADDR_IN * from, gsi_time recvTime)
{
	char dataOut[PI_DATA_MAX_LEN];
	uint16 ID;

	assert(udpPing->trip == 1);
	//assert(udpPing->ID_A != 0);
	if(udpPing->ID_A == 0)
		return;
	assert(udpPing->ID_B == 0);
	if(udpPing->ID_B != 0)
		return;

	if(piPingerPinged != NULL)
	{
		ID = piGetNextID();
	}
	else
	{
		ID = 0;
	}

	memset(dataOut, 0, PI_DATA_MAX_LEN);
	if(piPingerSetData != NULL)
	{
		piSettingData = PINGERTrue;
		piPingerSetData(from->sin_addr.s_addr, from->sin_port, dataOut, PI_DATA_MAX_LEN, piPingerSetDataParam);
		piSettingData = PINGERFalse;
	}

	piSendPing(from, 2, udpPing->ID_A, ID, dataOut);

	if(piPingerPinged != NULL)
	{
		piActivePing activePing;

		activePing.originator = PINGERFalse;
		activePing.ID = ID;
		activePing.expectedTrip = 3;
		activePing.timestamp = current_time();
		activePing.timeout = (activePing.timestamp + PI_TRIP2_TIMEOUT);
		activePing.remoteIP = from->sin_addr.s_addr;
		activePing.remotePort = from->sin_port;
		activePing.reply = NULL;
		activePing.replyParam = NULL;

		ArrayAppend(piActivePingList, &activePing);
	}
	
	GSI_UNUSED(data);
	GSI_UNUSED(recvTime);
}

static void piProcessTrip2(piUDPPing * udpPing, const char * data, SOCKADDR_IN * from, gsi_time recvTime)
{
	char dataOut[PI_DATA_MAX_LEN];
	int index;
	piActivePing * activePing;

	// This is the same IP as the shifted ping assert above.
	assert(udpPing->trip == 2);
	assert(udpPing->ID_A != 0);
	if(udpPing->ID_A == 0)
		return;
	
	activePing = piFindActivePing(udpPing->ID_A, &index);
	if(activePing == NULL)
		return;

	memset(dataOut, 0, PI_DATA_MAX_LEN);
	if(piPingerSetData != NULL)
	{
		piSettingData = PINGERTrue;
		piPingerSetData(from->sin_addr.s_addr, from->sin_port, dataOut, PI_DATA_MAX_LEN, piPingerSetDataParam);
		piSettingData = PINGERFalse;
	}

	if(udpPing->ID_B != 0)
	{
		piSendPing(from, 3, 0, udpPing->ID_B, dataOut);
	}

	if(activePing->reply != NULL)
	{
		int ping = piCalculatePing(activePing->timestamp, recvTime);

		piQueueCallback(from->sin_addr.s_addr, from->sin_port, ping, data, PI_DATA_MAX_LEN, activePing->replyParam, activePing->reply);
	}
	
	ArrayDeleteAt(piActivePingList, index);
}

static void piProcessTrip3(piUDPPing * udpPing, const char * data, SOCKADDR_IN * from, gsi_time recvTime)
{
	int index;
	piActivePing * activePing;
	
	assert(udpPing->trip == 3);
	assert(udpPing->ID_A == 0);
	if(udpPing->ID_A != 0)
		return;
	assert(udpPing->ID_B != 0);
	if(udpPing->ID_B == 0)
		return;

	activePing = piFindActivePing(udpPing->ID_B, &index);
	if(activePing == NULL)
		return;

	assert(piPingerPinged != NULL);
	if(piPingerPinged != NULL)
	{
		int ping = piCalculatePing(activePing->timestamp, recvTime);

		piPingerPinged(from->sin_addr.s_addr, from->sin_port, ping, data, PI_DATA_MAX_LEN, piPingerPingedParam);
	}
	
	ArrayDeleteAt(piActivePingList, index);
}

static void piProcessPing(piUDPPing * udpPing, const char * data, SOCKADDR_IN * from, gsi_time recvTime)
{
	assert(udpPing != NULL);
	assert(data != NULL);
	assert(from != NULL);

	if(udpPing->trip == 1)
		piProcessTrip1(udpPing, data, from, recvTime);
	else if(udpPing->trip == 2)
		piProcessTrip2(udpPing, data, from, recvTime);
	else if(udpPing->trip == 3)
		piProcessTrip3(udpPing, data, from, recvTime);
}

static void piProcessIncoming(void)
{
	int rcode;
	unsigned char buffer[PINGER_UDP_PING_SIZE];
	SOCKADDR_IN from;
	int len;
	gsi_time recvTime;
	piUDPPing udpPing;
	char data[PI_DATA_MAX_LEN];
	
	while(piInitialized && CanReceiveOnSocket(piSocket))
	{
		len = sizeof(SOCKADDR_IN);

		rcode = recvfrom(piSocket, (char *)buffer, PINGER_UDP_PING_SIZE, 0, (SOCKADDR *)&from, &len);

		if (gsiSocketIsError(rcode))
		{
			if(GOAGetLastError(piSocket) == WSAEMSGSIZE)
			{
				rcode = PINGER_UDP_PING_SIZE;
			}
			else
			{
				return; 
			}
		}
		else if(rcode < PINGER_UDP_PING_SIZE)
		{
			return;  
		}

		recvTime = current_time();

		if(piBytesToPing(buffer, &udpPing, data))
		{
			piProcessPing(&udpPing, data, &from, recvTime);
		}
	}

	piLastThinkTime = current_time();
}

static void piCheckTimeouts(void)
{
	gsi_time now;
	piActivePing * activePing;
	int len;
	int n;

	len = ArrayLength(piActivePingList);

	if(len == 0)
		return;

	now = current_time();

	for(n = (len - 1) ; n >= 0 ; n--)
	{
		activePing = (piActivePing *)ArrayNth(piActivePingList, n);
		assert(activePing != NULL);
		if(activePing != NULL)
		{
			if(activePing->timeout != 0)
			{
				if(activePing->timeout <= now)
				{
					if((activePing->originator) && (activePing->reply != NULL))
					{
						piQueueCallback(activePing->remoteIP, activePing->remotePort, PINGER_TIMEOUT, NULL, 0, activePing->replyParam, activePing->reply);
					}

					ArrayDeleteAt(piActivePingList, n);
				}
			}
		}
	}
}

void pingerThink(void)
{
	if(!piInitialized)
		return;

	if(piSettingData)
		return;

	piProcessIncoming();

	piCheckTimeouts();

	piCallCallbacks();
}

void pingerPing(unsigned int IP,
				unsigned short port,
				pingerGotPing reply,
				void * replyParam,
				PINGERBool blocking,
				gsi_time timeout)
{
	SOCKADDR_IN to;
	uint16 ID;

	assert(piInitialized);
	assert(IP != 0);

	assert(port != 0);
	assert(piUDPEnabled);
	assert(piSocket != INVALID_SOCKET);

	if(piSettingData)
		return;

	memset(&to, 0, sizeof(SOCKADDR_IN));
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = IP;

	ID = piGetNextID();

	if(piSendPing(&to, 1, ID, 0, NULL))
	{
		piActivePing activePing;

		activePing.originator = PINGERTrue;
		activePing.ID = ID;
		activePing.expectedTrip = 2;
		activePing.timestamp = current_time();
		if(timeout == 0)
			activePing.timeout = 0;
		else
			activePing.timeout = (activePing.timestamp + timeout);
		activePing.remoteIP = IP;
		activePing.remotePort = port;
		activePing.reply = reply;
		activePing.replyParam = replyParam;

		ArrayAppend(piActivePingList, &activePing);
	}

	if(blocking)
	{
		while(piFindActivePing(ID, NULL) != NULL)
		{
			pingerThink();

			msleep(1);
		}

		piCallCallbacks();
	}
}
