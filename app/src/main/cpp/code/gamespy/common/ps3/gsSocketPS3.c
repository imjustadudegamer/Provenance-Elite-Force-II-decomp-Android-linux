#include "../gsCommon.h"
#include "../gsPlatformSocket.h"
#include <netex/libnetctl.h>



HOSTENT * getlocalhost(void)
{	

	#define MAX_IPS		10
	static HOSTENT		localhost;
	static char 		* aliases = NULL;
	static char 		* ipPtrs[MAX_IPS + 1];
	static unsigned int ips		[MAX_IPS];
	int r;
	union CellNetCtlInfo gCellNetInfo;

	localhost.h_name		= "localhost";
	localhost.h_aliases		= &aliases;
	localhost.h_addrtype	= AF_INET;
	localhost.h_length		= sizeof(unsigned int);
	localhost.h_addr_list	= ipPtrs;
	ipPtrs[0] = (char *)&ips[0];
	ipPtrs[1] = NULL;

	r = cellNetCtlGetInfo(	CELL_NET_CTL_INFO_IP_ADDRESS,&gCellNetInfo	);				
	if (r == CELL_OK)		
	{
		ips[0] = inet_addr(gCellNetInfo.ip_address);
		

		return &localhost;
	}
	else	
	return NULL;

}
