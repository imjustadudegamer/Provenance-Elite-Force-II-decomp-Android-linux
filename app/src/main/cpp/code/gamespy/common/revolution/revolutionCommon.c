/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "../gsCommon.h"
#include <demo.h>
#include <string.h>
#include <revolution/so.h>
#include <revolution/ncd.h>

static NCDIfConfig      gIfConfig;
static NCDIpConfig      gIpConfig;
static OSMutex          gOsMemMutex;
static MEMHeapHandle    heapHandleSocket = NULL;

#define SOCKET_HEAPSIZE_DEFAULT (1024*128)

    
static void SetDefaultIfConfig( NCDIfConfig* theIfConfig )
{
	(void)memset( theIfConfig, 0, sizeof( NCDIfConfig ) );

	theIfConfig->selectedMedia	= NCD_IF_SELECT_WIRED;
}


static void SetDefaultIpConfig( NCDIpConfig* theIpConfig )
{

	(void)memset( theIpConfig, 0, sizeof( NCDIpConfig ) );

	theIpConfig->useDhcp  = TRUE;

	theIpConfig->adjust.maxTransferUnit       = 1300; 
	theIpConfig->adjust.tcpRetransTimeout     = 100;
	theIpConfig->adjust.dhcpRetransCount      = 4;
}




static void* _Alloc(unsigned long theName, long theSize)
{
    void*       a_memory_p = NULL;
    
    (void)theName;     
        
    if (0 < theSize) 
	{
     
        a_memory_p = MEMAllocFromExpHeapEx( heapHandleSocket, (u32) theSize, 32 );
	}
    
    return a_memory_p;
}


static void _Free(unsigned long theName, void* theMemoryPointer, long theSize)
{
    GSI_UNUSED(theName);
    
    if (theMemoryPointer && 0 < theSize) 
	{
        
        MEMFreeToExpHeap( heapHandleSocket, theMemoryPointer);
    }
}


BOOL StartNetwork()
{
    s32 rc;
    
    OSInitMutex(&gOsMemMutex);

    SetDefaultIfConfig( &gIfConfig );
    OSReport( "NCDSetIfConfig() ..." );
    rc = NCDSetIfConfig( &gIfConfig );
    if( rc != NCD_RESULT_SUCCESS )
    {
        OSReport( "failed (%d)\n", rc );
        return FALSE;
    }
    OSReport( "success\n" );

    
    SetDefaultIpConfig( &gIpConfig );
    OSReport( "NCDSetIpConfig() ..." );
    rc = NCDSetIpConfig( &gIpConfig );
    if( rc != NCD_RESULT_SUCCESS )
    {
        OSReport( "failed (%d)\n", rc );
        return FALSE;
    }
    OSReport( "success\n" );

    
    OSReport( "NCDIsInterfaceDecided() " );
    while( NCDIsInterfaceDecided() == FALSE )
    {
        NCDSleep( OSMillisecondsToTicks( 10 ) );
    }
    OSReport( "success\n" );

    
    OSReport( "SOInit() ..." );
    {
        SOLibraryConfig soLibConfig;

        (void)memset(&soLibConfig, 0, sizeof(soLibConfig));
        soLibConfig.alloc = _Alloc;
        soLibConfig.free  = _Free;

        rc = SOInit(&soLibConfig);
        if( rc != SO_SUCCESS )
        {
        	switch(rc)
        	{
        		case SO_EALREADY:
        			OSReport("failed (SO_EALREADY)\n");
        			break;
        		case SO_EINVAL:
        			OSReport("failed (SO_EINVAL)\n");
        			break;
        		case SO_ENOMEM:
        			OSReport("failed (SO_ENOMEM)\n");
        			break;
        		default:
        			OSReport("failed (%d)\n", rc);
        	};
            return FALSE;
        }
    }
    OSReport( "success\n" );
    

    OSReport( "SOStartup() " );
    rc = SOStartup();
    if( rc != SO_SUCCESS )
    {
        OSReport( "failed (%d)\n", rc );
        return FALSE;
    }
    OSReport( "success\n" );

    while( SOGetHostID() == 0 )
    {
        NCDSleep( OSMillisecondsToTicks( 10 ) );
    }
    {
        SOInAddr addr;
        addr.addr = (u32)SOGetHostID();
        OSReport( "IP address = %s\n", SOInetNtoA( addr ) );
    }
    return TRUE;
}


void StopNetwork()
{
    (void)SOCleanup();
}

extern int test_main(int argc, char **argv);
void main(int argc, char **argv)
{
	void*            arenaLo;
    void*            arenaHi;
    
   	DEMOInit(NULL); 
   
    arenaLo = OSGetMEM2ArenaLo();
    arenaHi = OSGetMEM2ArenaHi();
    
    if ((u32) arenaHi - (u32) arenaLo < SOCKET_HEAPSIZE_DEFAULT)
    	OSHalt("Insufficient memory in MEM2 for socket lib. Check SOCKET_HEAPSIZE_DEFAULT");
    heapHandleSocket = MEMCreateExpHeapEx( arenaLo, SOCKET_HEAPSIZE_DEFAULT, MEM_HEAP_OPT_THREAD_SAFE );
    if (heapHandleSocket == MEM_HEAP_INVALID_HANDLE)
        OSHalt("MEMCreateExpHeapEx failed.\n");
    OSSetArenaLo((u8*)arenaLo + SOCKET_HEAPSIZE_DEFAULT);

	if (!StartNetwork())
		OSHalt("Network was not started\n");
	OSReport("=======================================================================\n");
	OSReport("Gamespy:// Starting App\n");
	test_main(argc, argv);
	OSReport("=======================================================================\n");
	OSReport("Gamespy:// Ending App\n");
	StopNetwork();
}