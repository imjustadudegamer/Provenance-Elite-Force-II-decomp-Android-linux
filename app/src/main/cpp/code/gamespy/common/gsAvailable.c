#include "gsCommon.h"
#include "gsAvailable.h"

#define PACKET_TYPE    0x09
#define MASTER_PORT   27900
#define MAX_RETRIES       1
#define TIMEOUT_TIME   2000

GSIACResult __GSIACResult	= GSIACWaiting;

char __GSIACGamename[64]	= {0};

char GSIACHostname[64]		= {0};

static struct
{
	SOCKET sock;
	SOCKADDR_IN address;
	char packet[64];
	int packetLen;
	gsi_time sendTime;
	int retryCount;
} AC;

static int get_sockaddrin(const char * hostname, int port, SOCKADDR_IN * saddr)
{
	GS_ASSERT(hostname)
	GS_ASSERT(saddr)

	saddr->sin_family = AF_INET;
	saddr->sin_port = htons((unsigned short)port);
	saddr->sin_addr.s_addr = inet_addr(hostname);
	
	if(saddr->sin_addr.s_addr == INADDR_NONE)
	{
		HOSTENT * host = gethostbyname(hostname);
		if(!host)
			return 0;
		saddr->sin_addr.s_addr = *(unsigned int *)host->h_addr_list[0];
	}

	return 1;
}

static void SendPacket(void)
{
	sendto(AC.sock, AC.packet, AC.packetLen, 0, (SOCKADDR *)&AC.address, sizeof(AC.address));
	AC.sendTime = current_time();
}

void GSIStartAvailableCheckA(const char * gamename)
{
	char hostname[64];
	int override;
	int rcode;
	int len;

	GS_ASSERT(gamename)

	strcpy(__GSIACGamename, gamename);

	AC.sock = INVALID_SOCKET;

	SocketStartUp();

	override = GSIACHostname[0];
	if(!override)
		sprintf(hostname, "%s.available." GSI_DOMAIN_NAME, gamename);

	rcode = get_sockaddrin(override?GSIACHostname:hostname, MASTER_PORT, &AC.address);
	if(!rcode)
		return;

	AC.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(AC.sock == INVALID_SOCKET)
		return;

	AC.packet[0] = PACKET_TYPE;
	len = (int)strlen(gamename);
	memcpy(AC.packet + 5, gamename, (size_t)len + 1);
	AC.packetLen = (len + 6);

	SendPacket();

	AC.retryCount = 0;
}
#ifdef GSI_UNICODE
void GSIStartAvailableCheckW(const unsigned short * gamename)
{
	char gamename_A[32];
	GS_ASSERT(gamename)

	UCS2ToAsciiString(gamename, gamename_A);

	GSIStartAvailableCheckA(gamename_A);
}
#endif

static int HandlePacket(char * packet, int len, SOCKADDR_IN * address, int * disabledservices)
{
	int bitfield;

	if(len < 7)
		return 1;

	if(memcmp(&address->sin_addr, &AC.address.sin_addr, sizeof(IN_ADDR)) != 0)
		return 1;

	if(address->sin_port != AC.address.sin_port)
		return 1;

	if(memcmp(packet, "\xFE\xFD\x09", 3) != 0)
		return 1;

	bitfield = (int)((packet[3] << 24) & 0xFF000000);
	bitfield |= ((packet[4] << 16) & 0x00FF0000);
	bitfield |= ((packet[5] << 8) & 0x0000FF00);
	bitfield |= (packet[6] & 0x000000FF);

	*disabledservices = bitfield;

	return 0;
}

GSIACResult GSIAvailableCheckThink(void)
{
	char packet[64];
	SOCKADDR_IN address;
	int len = sizeof(address);
	int rcode;
	int disabledservices;

	if(AC.sock == INVALID_SOCKET)
	{
		__GSIACResult = GSIACAvailable;
		return __GSIACResult;
	}

	if(CanReceiveOnSocket(AC.sock))
	{
		rcode = (int)recvfrom(AC.sock, packet, (int)sizeof(packet), 0, (SOCKADDR *)&address, &len);

		rcode = HandlePacket(packet, rcode, &address, &disabledservices);
		if(rcode == 0)
		{
			closesocket(AC.sock);

			if(disabledservices & 1)
				__GSIACResult = GSIACUnavailable;
			else if(disabledservices & 2)
				__GSIACResult = GSIACTemporarilyUnavailable;
			else
				__GSIACResult = GSIACAvailable;

			return __GSIACResult;
		}
	}

	if(current_time() > (AC.sendTime + TIMEOUT_TIME))
	{
		if(AC.retryCount == MAX_RETRIES)
		{
			closesocket(AC.sock);
			__GSIACResult = GSIACAvailable;
			return __GSIACResult;
		}

		SendPacket();
		AC.retryCount++;
	}

	return GSIACWaiting;
}

void GSICancelAvailableCheck(void)
{
	if(AC.sock != INVALID_SOCKET)
	{
		closesocket(AC.sock);
		AC.sock = INVALID_SOCKET;
		__GSIACResult = GSIACWaiting;
	}
}
