///////////////////////////////////////////////////////////////////////////////
#include <psptypes.h>
#include <wlan.h>
#include <errno.h>
#include <thread.h>
#include <pspnet.h>
#include <pspnet_error.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_adhoc.h>
#include <pspnet_adhocctl.h>
#include <pspnet_resolver.h>
#include <pspnet_ap_dialog_dummy.h>
#include <pspnet/sys/socket.h>
#include <pspnet/netinet/in.h>
#include <utility\utility_common.h>
#include <utility\utility_netconf.h>

#include "../gsCommon.h"
#include "../gsPlatformSocket.h"


#if(0)	
int SetSockBlocking(SOCKET sock, int isblocking)
{
	int rcode;
	unsigned long argp;
		
	if(isblocking)
		argp = 0;
	else
		argp = 1;

	rcode = setsockopt(sock, SCE_NET_INET_SOL_SOCKET, SCE_NET_INET_SO_NBIO, &argp, sizeof(argp));

	if(rcode == 0)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment,
			"SetSockBlocking: Set socket %d to %s\r\n", (unsigned int)sock, isblocking ? "blocking":"non-blocking");
		return 1;
	}

	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment,
			"SetSockBlocking failed: tried to set socket %d to %s\r\n", (unsigned int)sock, isblocking ? "blocking":"non-blocking");
	return 0;
}
#endif


#define MAX_IPS  5
struct hostent* gsSocketGetHostByName(const char* name)
{
	GS_ASSERT(name);
	{
		int result = 0;
		int resolverID = 0;
		#define GetHostByNameBufferSize	1024
		char *buf = gsimalloc(GetHostByNameBufferSize);
		struct in_addr ip;
		
		static struct hostent ahostent;
		static char * aliases = NULL;
		static char * ipPtrs[MAX_IPS + 1];
		static unsigned int ips[MAX_IPS];

		GS_ASSERT(buf);

		ahostent.h_name = "";
		ahostent.h_aliases = &aliases;
		ahostent.h_addrtype = AF_INET;
		ahostent.h_addr_list = ipPtrs;
		
		result = sceNetResolverCreate(&resolverID, buf, GetHostByNameBufferSize);
		if (result < 0)
		{
			gsifree(buf);
			return NULL;  
		}
		result = sceNetResolverStartNtoA(resolverID, name, &ip, GSI_RESOLVER_TIMEOUT, GSI_RESOLVER_RETRY);
		sceNetResolverDelete(resolverID); 
		if (result < 0)
		{
			gsifree(buf);
			return NULL;
		}
		ahostent.h_length = sizeof(struct in_addr);
		memcpy(&ips[0], &ip, sizeof(struct in_addr));
		ahostent.h_addr_list[0] = (char*)&ips[0];
		ahostent.h_addr_list[1] = NULL;

		{
			const char * out = sceNetInetInetNtop(SCE_NET_INET_AF_INET, &ip, buf, sizeof(buf));
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Notice,
				"gsSocketGetHostByName: %s = %s\r\n", name, out?out:"void");
		}
		gsifree(buf);
		return &ahostent;
	}
}


const char* gsSocketInetNtoa(struct in_addr in)
{
	static char buf[sizeof("XXX.XXX.XXX.XXX")];
	sceNetInetInetNtop(SCE_NET_INET_AF_INET, &in, buf, sizeof(buf));
	return buf;
}

HOSTENT * getlocalhost(void)
{
	#define MAX_IPS  5
	static HOSTENT localhost;
	static char * aliases = NULL;
	static char * ipPtrs[MAX_IPS + 1];
	static unsigned int ips[MAX_IPS];
	int result = 0;
	gsi_u32 addr;

	static union SceNetApctlInfo info;

	localhost.h_name = "localhost";
	localhost.h_aliases = &aliases;
	localhost.h_addrtype = AF_INET;
	localhost.h_length = 0;
	localhost.h_addr_list = (char **)ipPtrs;
	ipPtrs[0] = (char *)&ips[0];
	ipPtrs[1] = NULL;
	ips[0] = 0;

	result = sceNetApctlGetInfo(SCE_NET_APCTL_INFO_IP_ADDRESS, &info);
	if (result < 0)
	{
		printf("getlocalhost sceNetApctlGetInfo returned %d\r\n", result);
		return NULL;
	}

	addr = inet_addr(info.ip_address); 
	memcpy(&ips[0], &addr, sizeof(addr)); 
	localhost.h_length = (gsi_u16)sizeof(addr);

	return &localhost;
}


#define RXBUFLEN		1024

#ifdef GSI_ADHOC
void	NetAdhocMacGet			(char *mac)
{
	int ret = sceWlanGetEtherAddr((struct SceNetEtherAddr *)mac);
}


int _NetworkAdHocSocketCreate(gsi_u16 port)
{
	int ret, id;
	struct SceWlanEtherAddr tmp_local;
	static struct SceNetEtherAddr addr;
	ret = sceWlanGetEtherAddr(&tmp_local);
	if (ret < 0) 
	{
		return -1;
	}
	memcpy(&addr, &tmp_local.addr,sizeof(struct SceNetEtherAddr));
	#ifdef _PSPNET_LOG
	printf("Create Adhoc Socket: MAC:%x,%x,%x,%x,%x,%x\n",(gsi_u32)tmp_local.addr[0],(gsi_u32)tmp_local.addr[1],(gsi_u32)tmp_local.addr[2],(gsi_u32)tmp_local.addr[3],(gsi_u32)tmp_local.addr[4],(gsi_u32)tmp_local.addr[5]);
	#endif

	id = sceNetAdhocPdpCreate(&addr, port, RXBUFLEN, 0);
	if (id < 0) 
	{
		return -1;
	}
	return id;
}

void _NetworkAdHocSocketDestroy(int socket)
{
	int ret;
	ret = sceNetAdhocPdpDelete(socket,0);
	if (ret < 0) 
	{
	}
}

int _NetworkAdHocSocketBroadcast(	int			socketid,
									const void *data,
									int			len,
									int			flags,
									gsi_u16		dest_port
								)
{
	const static gsi_u8 broadcast_addr[SCE_NET_ETHER_ADDR_LEN] = {0xff};
	int ret = sceNetAdhocPdpSend(
							socketid,		
							broadcast_addr,	
							dest_port,		
							data,			
							len,			
							10,				
							flags			
		);

	return ret;
}


int _NetworkAdHocSocketSendTo(	int			socketid,
								const void *data,
								int			len,
								int			flags,
								const char *dest_addr,
								gsi_u16		dest_port
							 )
{
#if(0)	
	const static gsi_u8 broadcast_addr[SCE_NET_ETHER_ADDR_LEN] = {0xff};
	int ret = sceNetAdhocPdpSend(
							socketid,		
							broadcast_addr,	
							dest_port,		
							data,			
							len,			
							10,				
							flags			
		);
#else

	int ret = sceNetAdhocPdpSend(
							socketid,		
							dest_addr,		
							dest_port,		
							data,			
							len,			
							10,				
							flags			
		);
#endif
	#ifdef _PSPNET_LOG
	printf("AdHoc SendTo: MAC:%x,%x,%x,%x,%x,%x port=%d Len=%d\n",(gsi_u32)dest_addr[0],(gsi_u32)dest_addr[1],(gsi_u32)dest_addr[2],(gsi_u32)dest_addr[3],(gsi_u32)dest_addr[4],(gsi_u32)dest_addr[5],dest_port,len);
	#endif
	return ret;
}

int _NetworkAdHocCanReceiveOnSocket(int socket_id)
{
	int		ret;
	struct	SceNetAdhocPdpStat	stat;
	int		stat_buflen;


	stat_buflen = sizeof(struct SceNetAdhocPdpStat);
	ret = sceNetAdhocGetPdpStat(&stat_buflen, (void *)&stat);
	if (ret < 0) 
	{

		if(ret == SCE_ERROR_NET_ADHOC_INVALID_ARG)
		{
			printf("sceNetAdhocGetPdpStat() failed. SCE_ERROR_NET_ADHOC_INVALID_ARG     ret = 0x%x\n", ret);
		}
		else
		if(ret == SCE_ERROR_NET_ADHOC_NOT_INITIALIZED)
		{
			printf("sceNetAdhocGetPdpStat() failed. SCE_ERROR_NET_ADHOC_NOT_INITIALIZED ret = 0x%x\n", ret);
		}
		else
		{
			printf("sceNetAdhocGetPdpStat() failed. ret = 0x%x\n", ret);
		}
		return -1;
	}

	if (stat_buflen == 0 || stat.id != socket_id) 
	{
		printf("sceNetAdhocGetPdpStat() invalid data.\n");
		return 0;
	}

	return stat.rcv_sb_cc;	
}

int _NetworkAdHocSocketRecv(int socket_id,
							char	*buf,
							int		bufferlen,
							int		flags,
							char	*saddr,		
							u_int16 *sport
							)
{
	int ret = sceNetAdhocPdpRecv(	
			socket_id,				
			saddr,					
			sport,					
			buf,					
			&bufferlen,				
			0,						
			SCE_NET_ADHOC_F_NONBLOCK
			);

	if (ret < 0)
	{
		return ret;
	}
	if (ret == (int)SCE_ERROR_NET_ADHOC_WOULD_BLOCK) 
	{
		return SCE_OK;
	}
	#ifdef _PSPNET_LOG
	if(ret >= 0)
		printf("AdHoc Recv From: MAC:%x,%x,%x,%x,%x,%x: port=%d len:%d\n",saddr[0],saddr[1],saddr[2],saddr[3],saddr[4],saddr[5],*sport,bufferlen);
	#endif


	return bufferlen;
}
#endif

