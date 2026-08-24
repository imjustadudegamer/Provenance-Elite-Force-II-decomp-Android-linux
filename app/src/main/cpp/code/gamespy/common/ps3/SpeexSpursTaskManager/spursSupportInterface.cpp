#include "spursConfiguration.h"
#include "spursSupportInterface.h"
#include <spu_printf.h>
#include <sys/ppu_thread.h>
#include <sys/spu_initialize.h>
#include <stdio.h>
#include <stdlib.h>
#include <cell/atomic.h>
#include <assert.h>

#define SCE_EXTERNAL_RELEASE

#include <sys/fs.h>

static uint32_t _g_uiNextEventQueueKey=0x92400ABDUL;
uint32_t _SpursSupportGetUniqueEventQueueKey() {
	return _g_uiNextEventQueueKey++;
}

sys_event_queue_t _g_SpuPrintfEventQueue;
sys_ppu_thread_t	_g_SpursPrintfThread;

void _SpursPrintfThreadMain(uint64_t arg);

#define	SPURS_PPU_THREAD_PRIO 1001
#define SPU_PRINTF_EVENT_QUEUE_SIZE 8
#define SPU_PRINTF_EVENT_QUEUE_PORT 0x1
#define SPU_PRINTF_THREAD_PRIO 1001
#define SPU_PRINTF_THREAD_STACK_SIZE (64 * 1024)


#ifdef _DEBUG
extern char _binary_spu_SpeexSpursTaskDebug_elf_start[];
void *g_SpursTaskElfStart = _binary_spu_SpeexSpursTaskDebug_elf_start;
#else
extern char _binary_spu_SpeexSpursTaskRelease_elf_start[];
void *g_SpursTaskElfStart = _binary_spu_SpeexSpursTaskRelease_elf_start;
#endif 




#define SPURS_SPU_THREAD_PRIORITY 200

static CellSpurs *g_spursInstance=0;
static CellSpursTaskset g_spursTaskSet __attribute__ ((aligned(128)));

static int g_iDefaultSPUCount=CELL_SPURS_DEFAULT_SPU_COUNT;
static bool g_bSpursInitialized=false;
static bool g_bUserSpursGiven=false;
static uint32_t g_uiSpursReferenceCounter=0;

void _SpursPrintfThreadMain(uint64_t arg) 
{
	sys_event_t event;
	int iReturn;

	(void) arg;

	while (1) {
		iReturn=sys_event_queue_receive(_g_SpuPrintfEventQueue, &event,
										SYS_NO_TIMEOUT);
	
		if (iReturn!=CELL_OK) {
			fprintf(stderr, "Event queue receive wasn't successful: %i\n",
					iReturn);
			exit(-1);
		}

		iReturn=spu_thread_printf(event.data1, event.data3);
		sys_spu_thread_write_spu_mb(event.data1, iReturn);
	}
}


int initializeSpursTaskSet(CellSpurs *pSpurs, int iSPUCount, uint8_t *puiPriorities) 
{
	uint8_t auiLocalPriorities[8]={1,1,1,1,1,1,1,1};
	uint8_t *pPriorities=auiLocalPriorities;
	int iReturn;
	int iNumSPUs=iSPUCount;


	if (pSpurs) 
	{
		g_bUserSpursGiven=true;

		pPriorities=puiPriorities;
		iNumSPUs=iSPUCount;
		g_spursInstance=pSpurs;
	} 
	else 
	{
		g_bUserSpursGiven=false;

		sys_ppu_thread_t idCurrentThread;
		int iCurrentThreadPriority;

		iReturn=sys_ppu_thread_get_id(&idCurrentThread);
		if (iReturn!=CELL_OK) 
		{
			return CELL_SPURS_EINVAL;
		}

		iReturn=sys_ppu_thread_get_priority(idCurrentThread,
											&iCurrentThreadPriority);
		if (iReturn!=CELL_OK) 
		{
			return CELL_SPURS_EINVAL;
		}

		g_spursInstance=(CellSpurs *) memalign(128, sizeof(CellSpurs));
		if (!g_spursInstance)
		{
			return CELL_SPURS_EINVAL;
		}
		
		sys_spu_initialize(iNumSPUs, 0);
		iReturn=cellSpursInitialize(g_spursInstance, iNumSPUs,
									SPURS_SPU_THREAD_PRIORITY,
									iCurrentThreadPriority - 1, false);
		if (iReturn!=CELL_OK) 
		{
			free(g_spursInstance);
			return CELL_SPURS_EINVAL;
		}
	}






	iReturn=cellSpursCreateTaskset(g_spursInstance, &g_spursTaskSet, 0, pPriorities,
								   iNumSPUs);

	if (iReturn!=CELL_OK) 
	{
		cellSpursFinalize(g_spursInstance);
		free(g_spursInstance);
		return CELL_SPURS_EINVAL;
	}

	g_bSpursInitialized=true;

	return CELL_SPURS_OK;
}

int checkSpursTaskSet() 
{
	if (!g_bSpursInitialized) 
	{
		int returnCode = initializeSpursTaskSet(0, g_iDefaultSPUCount, 0)!=CELL_SPURS_OK;
		return returnCode;
	}
	return CELL_SPURS_OK;
}

int spursConfiguration_initWithSpurs(CellSpurs *pSpurs, int iSPUCount, uint8_t auiPriorities[8]) 
{
	if (g_bSpursInitialized) 
	{
		return CELL_SPURS_EBUSY;
	}

	if ((!pSpurs) || (((uintptr_t) pSpurs) & 0x7f)) 
	{
		return CELL_SPURS_EINVAL;
	}

	if (iSPUCount<1 || iSPUCount>6) 
	{
		return CELL_SPURS_EINVAL;
	}

	return initializeSpursTaskSet(pSpurs, iSPUCount, auiPriorities);
}

int spursConfiguration_initWithSpuCount(int iSPUCount) 
{
	if (g_bSpursInitialized) 
	{
		return CELL_SPURS_EBUSY;
	}

	if (iSPUCount<1 || iSPUCount>6) 
	{
		return CELL_SPURS_EINVAL;
	}

	return initializeSpursTaskSet(0, iSPUCount, 0);
}

int spursConfiguration_terminate() 
{
	if (!g_bSpursInitialized) 
	{
		return CELL_SPURS_OK;
	}

	if (g_uiSpursReferenceCounter) 
	{
		return CELL_SPURS_EBUSY;
	}
	int iReturn;
	bool bUserSpurs=g_bUserSpursGiven;

	g_bSpursInitialized=false;
	g_bUserSpursGiven=false;

	iReturn=cellSpursShutdownTaskset(&g_spursTaskSet);
	if (iReturn!=CELL_OK) 
	{
		return -1;
	}

	iReturn=cellSpursJoinTaskset(&g_spursTaskSet);
	if (iReturn!=CELL_OK) 
	{
		return -2;
	}

	if (!bUserSpurs) 
	{
		int iReturn=cellSpursFinalize(g_spursInstance);

		if (iReturn!=CELL_OK) 
		{
			return -3;
		}

		free(g_spursInstance);
	}

	g_spursInstance=0;


	return CELL_SPURS_OK;
}

bool spursConfiguration_isSpursInitialized() 
{
	return g_bSpursInitialized;
}


SpursSupportInterface::SpursSupportInterface() 
{
	//assert(elfId < SPU_ELF_LAST);
	m_bQueueInitialized=false;

	cellAtomicIncr32(&g_uiSpursReferenceCounter);
}

SpursSupportInterface::~SpursSupportInterface() 
{
	stopSPU();

	cellAtomicDecr32(&g_uiSpursReferenceCounter);
}

int SpursSupportInterface::startSPU() 
{
	int iReturn;

	if (!m_bQueueInitialized) 
	{
		if (checkSpursTaskSet() != CELL_SPURS_OK)
		{
			return -1;
		}

		iReturn=cellSpursQueueInitialize(&g_spursTaskSet,
			&m_responseQueue, m_aResponseBuffer, sizeof(CellSPURSArgument),
			CELL_SPURS_RESPONSE_QUEUE_SIZE, CELL_SPURS_QUEUE_SPU2PPU);
		if (iReturn!=CELL_OK) 
		{
			return -2;
		}

		iReturn=cellSpursQueueAttachLv2EventQueue(&m_responseQueue);
		if (iReturn!=CELL_OK) 
		{
			return -3;
		}

		m_bQueueInitialized=true;
	}
	return 0;
}

int SpursSupportInterface::stopSPU() 
{
	if (m_bQueueInitialized) 
	{
		int iReturn=cellSpursQueueDetachLv2EventQueue(&m_responseQueue);
		if (iReturn != CELL_OK)
			return -1;
		m_bQueueInitialized=false;
	}
	return 0;
}

int SpursSupportInterface::sendRequest(uint32_t uiCommand, uint32_t uiArgument0, uint32_t uiArgument1) 
{
	int iReturn;
	CellSpursTaskId taskId;
	CellSPURSArgument arguments;

	arguments.ppuResponseQueue=&m_responseQueue;
	arguments.uiCommand=uiCommand;
	arguments.uiArgument0=uiArgument0;
	arguments.uiArgument1=uiArgument1;

	if (checkSpursTaskSet() != CELL_SPURS_OK)
	{
		return -1;
	}

	iReturn=cellSpursCreateTask(&g_spursTaskSet, &taskId, g_SpursTaskElfStart, NULL, 0, 0, &arguments.spursArgument);
	if (iReturn!=CELL_OK) 
	{
		return -2;
	}
	
	return 0;
}


int SpursSupportInterface::waitForResponse(unsigned int *puiArgument0, unsigned int *puiArgument1) 
{
	CellSPURSArgument response __attribute__((aligned(16)));
	int iReturn;

	iReturn=cellSpursQueuePop(&m_responseQueue, (void *) &response);
	
	if (iReturn!=CELL_OK) 
	{
		return -1;
	}

	if (puiArgument0)
		*puiArgument0=response.uiArgument0;

	if (puiArgument1)
		*puiArgument1=response.uiArgument1;
	return 0;
}

