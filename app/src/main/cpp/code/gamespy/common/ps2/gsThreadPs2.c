#include "../gscommon.h"


gsi_u32 gsiInterlockedIncrement(gsi_u32 * value)
{
	int interrupt = DI();
	int ret = ++(*value);
	if (interrupt)
		EI();

	return ret;
}

gsi_u32 gsiInterlockedDecrement(gsi_u32 * value)
{
	int interrupt = DI();
	int ret = --(*value);
	if (interrupt)
		EI();

	return ret;
}


void gsiInitializeCriticalSection(GSICriticalSection *theCrit) 
{
	theCrit->mSemaphore = gsiCreateSemaphore(1, 1, NULL); 
	theCrit->mOwnerThread = 0;
	theCrit->mEntryCount = 0;
}
void gsiEnterCriticalSection(GSICriticalSection *theCrit) 
{ 
	if (GetThreadId() != theCrit->mOwnerThread)
	{
		gsiWaitForSemaphore(theCrit->mSemaphore, 0);
		theCrit->mOwnerThread = GetThreadId();
	}

	theCrit->mEntryCount++;
}
void gsiLeaveCriticalSection(GSICriticalSection *theCrit)
{ 
	if (GetThreadId() != theCrit->mOwnerThread)
	{
		assert(GetThreadId() == theCrit->mOwnerThread);
		return;
	}

	theCrit->mEntryCount--;
	if (theCrit->mEntryCount == 0)
	{
		theCrit->mOwnerThread = 0;
		gsiReleaseSemaphore(theCrit->mSemaphore, 1);     
	}
}

void gsiDeleteCriticalSection(GSICriticalSection *theCrit) 
{ 
	gsiCloseSemaphore(theCrit->mSemaphore);       
}

gsi_u32 gsiHasThreadShutdown(GSIThreadID theThreadID) 
{ 
	struct ThreadParam aStatus;
	ReferThreadStatus(theThreadID, &aStatus);
	if (aStatus.status == THS_DORMANT)
		return 1; 
	else
		return 0; 
}

GSISemaphoreID gsiCreateSemaphore(gsi_i32 theInitialCount, gsi_i32 theMaxCount, char* theName)
{
	struct SemaParam aParam;
	int aSemaphore = 0;

	aParam.initCount = theInitialCount;
	aParam.maxCount = theMaxCount;
	
	aSemaphore = CreateSema(&aParam);
	if (aSemaphore < 0)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Misc, GSIDebugLevel_WarmError,
			"Failed to create semaphore\r\n");
	}
	
	GSI_UNUSED(theName);
	
	return aSemaphore;
}

gsi_u32 gsiWaitForSemaphore(GSISemaphoreID theSemaphore, gsi_u32 theTimeoutMs)
{
	int result = WaitSema(theSemaphore);
	return (gsi_u32)result;

	GSI_UNUSED(theTimeoutMs);
}

void gsiReleaseSemaphore(GSISemaphoreID theSemaphore, gsi_i32 theReleaseCount)
{
	while (theReleaseCount-- > 0)
		SignalSema(theSemaphore);
}

void gsiCloseSemaphore(GSISemaphoreID theSemaphore)
{
	DeleteSema(theSemaphore);
}

int gsiStartThread(GSThreadFunc func,  gsi_u32 theStackSize, void *arg, GSIThreadID *id)
{
	const unsigned int stackSize = theStackSize;
	const int threadPriority = 3;
	struct ThreadParam param;
	void * stack;
	int threadID;

	stack = gsimemalign(16, stackSize);
	if(!stack)
		return -1;

	param.entry = func;
	param.stack = stack;
	param.stackSize = (int)stackSize;
	param.gpReg = &_gp;
	param.initPriority = threadPriority;

	threadID = CreateThread(&param);
	if(threadID == -1)
	{
		gsifree(stack);
		return -1;
	}

	if(StartThread(threadID, arg) == -1)
	{
		DeleteThread(threadID);
		gsifree(stack);
		return -1;
	}

	*id = threadID;

	msleep(1);

	return 0;
}

void gsiCancelThread(GSIThreadID id)
{
	void* aStack = NULL;

	struct ThreadParam aThreadParam;
	ReferThreadStatus(id, &aThreadParam);
	aStack = (void*)aThreadParam.stack;

	TerminateThread(id);

	DeleteThread(id);

	gsifree(aStack);
}

void gsiExitThread(GSIThreadID id)
{
	GSI_UNUSED(id);
}

void gsiCleanupThread(GSIThreadID id)
{
	gsiCancelThread(id);
}