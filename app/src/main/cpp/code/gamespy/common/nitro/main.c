/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "..\nonport.h"
#include "screen.h"
#include "key.h"
#include "wireless.h"
#include "touch.h"
#include "backup.h"

static void Startup(void)
{
	OS_Init();

	OS_InitTick();

	OS_InitAlarm();

	RTC_Init();

	PXI_InitFifo();

	ScreenInit();
	Printf("Screen initialized\n");
	
	SetTopScreenLineCentered(SCREEN_HEIGHT / 2, SCWhite, "Starting GameSpy Sample");

	KeyInit();
	Printf("Input initialized\n");

	TouchInit();
	Printf("Touch initialized\n");

	OS_EnableIrq();
	OS_EnableInterrupts();
	Printf("Interrupts initialized\n");

    {
		u32 nHeapAdrs = 0;
		u32 nHeapSize = 1 * 1024 * 1024;
		OSHeapHandle hHeap;

		OS_SetMainArenaLo(OS_InitAlloc(OS_ARENA_MAIN, OS_GetMainArenaLo(), OS_GetMainArenaHi(), 1));
		nHeapAdrs = (u32)OS_AllocFromMainArenaLo(nHeapSize, 32);
		hHeap = OS_CreateHeap(OS_ARENA_MAIN, (void*)(nHeapAdrs), (void*)(nHeapAdrs + nHeapSize));
		OS_SetCurrentHeap(OS_ARENA_MAIN, hHeap);
    }
	Printf("Heap initialized\n");

	BackupInit();

	WirelessInit();
}

static void Shutdown(void)
{
	Printf("Wireless cleanup\n");
	WirelessCleanup();
	
	
	Printf("Terminating OS\n");
	OS_Terminate();
}

extern int test_main(int argc, char ** argv);

static void Run(void)
{
	SetPrintMode(PRINT_TO_SCREEN|PRINT_TO_DEBUGGER);

	Printf("\n");
	Printf("GameSpy Test App Starting\n");
	Printf("-------------------------\n");
	
	test_main(0, NULL);

	Printf("------------------------\n");
	Printf("GameSpy Test App Exiting\n");

	SetPrintMode(PRINT_TO_DEBUGGER);
}

void NitroMain(void)
{
	Startup();

	Run();
	
	Shutdown();
}