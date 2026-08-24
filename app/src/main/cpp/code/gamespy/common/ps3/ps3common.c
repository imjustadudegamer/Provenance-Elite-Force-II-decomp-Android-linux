///////////////////////////////////////////////////////////////////////////////
//   Note:  This code is not intended to be used in a retail program
#include <cell/sysmodule.h>
#include <netex/net.h>
#include <netex/libnetctl.h>
#include <sysutil/sysutil_common.h>

#include "../gsPlatform.h"
#include "../gsPlatformUtil.h"
#include "../gsMemory.h"
extern int test_main(int argc, char ** argp); 

#define PRIO	  (1001)
#define HOST_NAME "localhost"


gsi_bool	_LoadModules()
{
	cellSysmoduleInitialize();

	if (cellSysmoduleLoadModule(CELL_SYSMODULE_NET) != CELL_OK)
	{
		printf("cellSysmoduleLoadModule(CELL_SYSMODULE_NET) failed\n");
		return gsi_false;
	}
	if (cellSysmoduleLoadModule(CELL_SYSMODULE_IO) != CELL_OK)
	{
		printf("cellSysmoduleLoadModule(CELL_SYSMODULE_IO) failed\n");
		return gsi_false;
	}
	if (cellSysmoduleLoadModule(CELL_SYSMODULE_USBD) != CELL_OK)
	{
		printf("cellSysmoduleLoadModule(CELL_SYSMODULE_USBD) failed\n");
		return gsi_false;
	}
	if (cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP) != CELL_OK)
	{
		printf("cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP) failed\n");
		return gsi_false;
	}
	if (cellSysmoduleLoadModule(CELL_SYSMODULE_RTC) != CELL_OK)
	{
		printf("cellSysmoduleLoadModule(CELL_SYSMODULE_RTC) failed\n");
		return gsi_false;
	}

	return gsi_true;
}

void		_UnloadModules()
{
	cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_IO);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_USBD);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_RTC);

	cellSysmoduleFinalize();
}


gsi_bool	_NetworkInit()
{
	int r = sys_net_initialize_network();

	if (r!=CELL_OK) 
	{
		printf("ccNetworkInitializeNetwork: sys_net_initialize_network() failed: %i\n",r);
		return gsi_false;
	}


	return (r==0);
}



gsi_bool	_NetworkStart()
{
	int iReturn, iNetworkState;


	iReturn = cellNetCtlInit();
	if (iReturn!=CELL_OK) 
	{
		printf("ccNetworkInitializeNetwork: cellNetCtlInit() failed: %i\n",
			   iReturn);
		return gsi_false;
	}


	while (1) 
	{
		iReturn=cellNetCtlGetState(&iNetworkState);


		if (iReturn!=CELL_OK)
		{
			printf("ccNetworkInitializeNetwork: cellNetCtlGetState() failed: %i\n",
				   iReturn);
			return gsi_false;
		}

		if (iNetworkState!=CELL_NET_CTL_STATE_IPObtained) 
		{
			sys_timer_usleep(100 * 1000);
		} 
		else 
		{
			break;
		}
	};

	return gsi_true;
}

void		_NetworkStop()
{
	cellNetCtlTerm();
}

void		_NetworkClose()
{
	sys_net_finalize_network();
}

void *  gsiMemManagedInit()
{
#if defined GSI_MEM_MANAGED
#define aMemoryPoolSize (1024*1024*8)
	char *aMemoryPool = calloc(aMemoryPoolSize,32);
	if(aMemoryPool == NULL)
	{
		printf("Failed to create memory pool - aborting\r\n");
		return NULL;
	}
	else
	{
		gsMemMgrContext	c = gsMemMgrCreate(gsMemMgrContext_Default, "Default",aMemoryPool, aMemoryPoolSize);
		GSI_UNUSED(c);
	}
	return aMemoryPool;
#else
	return NULL;
#endif

}

void gsiMemManagedClose(void * aMemoryPool)
{
#if defined(GSI_MEM_MANAGED)

	gsi_u32		MemAvail = 	gsMemMgrMemAvailGet			(gsMemMgrContext_Default);
	gsi_u32		MemUsed	 =	gsMemMgrMemUsedGet			(gsMemMgrContext_Default);
	gsi_u32		HwMark	 =	gsMemMgrMemHighwaterMarkGet	(gsMemMgrContext_Default);

	printf("MemAvail %u: MemUsed%u  MemHWMark %u\n", MemAvail,MemUsed,HwMark);
	gsMemMgrDumpStats();
	gsMemMgrDumpAllocations();
	gsMemMgrValidateMemoryPool();
	gsMemMgrDestroy(gsMemMgrContext_Default);
	free(aMemoryPool);
#endif
}

int main(int argc, char ** argp)
{
	void *heap;
    printf("\nGameSpy Test App Initializing\n" 
	       "----------------------------------\n");

	if(!_LoadModules())
	{
		printf("_LoadModules() failed\n");
		return (0);
	}

	if(!_NetworkInit())	
	{
		printf("_NetworkInit() failed\n");
		return (0);
	}

	if(!_NetworkStart())
	{
		printf("_NetworkStart() failed\n");
		return (0);
	}
	
	heap = gsiMemManagedInit();
    printf("\nGameSpy Test App Starting\n" 
	       "----------------------------------\n");
	test_main(argc, argp);

	printf("\nGameSpy Test App Exiting\n" 
	       "----------------------------------\n");
	gsiMemManagedClose(heap);
	_NetworkStop();
	_NetworkClose();

	_UnloadModules();

	while(1)
	{
		msleep(100);
	}
	return 0;
}

