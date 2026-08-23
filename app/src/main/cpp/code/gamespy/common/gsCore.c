#include "gsPlatform.h"
#include "gsPlatformThread.h"

#include "gsCommon.h"
#include "gsCore.h"
#include "gsAssert.h"
#include "../ghttp/ghttp.h"




#define GSI_CORE_INIT_YIELD_MS      100
#define GSI_CORE_SHUTDOWN_YIELD_MS  50


static GSCoreMgr* gsiGetStaticCore()
{
	static GSCoreMgr gStaticCore;
	return &gStaticCore;
}


static void gsiCoreAtExitShutdown(void)
{
	GSCoreMgr * aCore = gsiGetStaticCore();
	gsiDeleteCriticalSection(&aCore->mQueueCrit);
	GSI_UNUSED(aCore);
}


void gsCoreInitialize()
{
	GSCoreMgr* aCore = gsiGetStaticCore();

	while(gsi_is_true(aCore->mIsShuttingDown))
		msleep(GSI_CORE_INIT_YIELD_MS); 

	if (gsiInterlockedIncrement(&aCore->mRefCount) == 1)
	{
		if (gsi_is_false(aCore->mIsStaticInitComplete))
		{
			gsiInitializeCriticalSection(&aCore->mQueueCrit);

			#ifndef _MANAGED
				atexit(gsiCoreAtExitShutdown);
			#endif

			aCore->mIsStaticInitComplete = gsi_true;
		}

		gsiEnterCriticalSection(&aCore->mQueueCrit);
		gsiLeaveCriticalSection(&aCore->mQueueCrit);

		while(gsi_is_true(aCore->mIsShuttingDown))
			msleep(GSI_CORE_INIT_YIELD_MS);

		#ifdef GSICORE_DYNAMIC_TASK_LIST
			aCore->mTaskArray = ArrayNew(sizeof(GSTask*), 10, NULL);
			GS_ASSERT(aCore->mTaskArray);
		#else
			memset(aCore->mTaskArray, 0, sizeof(aCore->mTaskArray));
		#endif

		ghttpStartup();

		aCore->mIsInitialized = gsi_true;
	}
	else
	{
		
		while(gsi_is_false(aCore->mIsStaticInitComplete))
			msleep(GSI_CORE_INIT_YIELD_MS);

		gsiEnterCriticalSection(&aCore->mQueueCrit);
		gsiLeaveCriticalSection(&aCore->mQueueCrit);

		while(gsi_is_false(aCore->mIsInitialized))
			msleep(GSI_CORE_INIT_YIELD_MS); 
	}
}


static void gsiCoreTaskDispatchCallback(GSTask *theTask, GSTaskResult theResult)
{
	if (theTask->mIsCallbackPending)
	{
		theTask->mIsCallbackPending = 0;
		if (theTask->mCallbackFunc)
			(theTask->mCallbackFunc)(theTask->mTaskData, theResult);
	}
}


GSTaskResult gsCoreTaskThink(GSTask* theTask)
{
	GSCoreMgr* aCore = gsiGetStaticCore();
	GSTaskResult aResult = GSTaskResult_None;

	if (theTask == NULL)
		return GSTaskResult_Finished;

	if (theTask->mIsRunning && theTask->mThinkFunc)
		aResult = (theTask->mThinkFunc)(theTask->mTaskData);

	if ((!theTask->mIsCanceled) && (aResult == GSTaskResult_InProgress))
	{
		if ((theTask->mTimeout != 0) && (current_time() - theTask->mStartTime > theTask->mTimeout))
		{
			gsiCoreCancelTask(theTask);

			gsiCoreTaskDispatchCallback(theTask, GSTaskResult_TimedOut);
		}
	}
	else if (aResult != GSTaskResult_InProgress)
	{
		int i=0;
		gsi_bool removeTask = gsi_true;

		if (theTask->mIsRunning)
		{
			gsiCoreTaskDispatchCallback(theTask, aResult);
			theTask->mIsRunning = 0;
		}

		if (theTask->mCleanupFunc)
			removeTask = (theTask->mCleanupFunc)(theTask->mTaskData);

		if (gsi_is_true(removeTask))
		{
			gsiEnterCriticalSection(&aCore->mQueueCrit);
			#ifdef GSICORE_DYNAMIC_TASK_LIST
			{
				int len = ArrayLength(aCore->mTaskArray);
				for (i=0; i < len; i++)
				{
					if(*(GSTask**)ArrayNth(aCore->mTaskArray, i) == theTask)
					{
						ArrayRemoveAt(aCore->mTaskArray, i);
						gsifree(theTask);
						break;
					}
				}
			}
			#else
				for (i=0; i < GSICORE_MAXTASKS; i++)
				{
					if (aCore->mTaskArray[i] == theTask)
					{
						aCore->mTaskArray[i] = NULL;
						gsifree(theTask);
						break;
					}
				}
			#endif
			gsiLeaveCriticalSection(&aCore->mQueueCrit);
			return GSTaskResult_Finished;
		}
	}

	return GSTaskResult_InProgress;
}


void gsCoreThink(gsi_time theMS)
{
	GSCoreMgr* aCore = gsiGetStaticCore();
	int i=0;
	gsi_time aStartTime = 0;
	gsi_i32 allTasksAreDead = 1;

	if (gsi_is_false(aCore->mIsInitialized))
		return;

	gsiEnterCriticalSection(&aCore->mQueueCrit);

	aStartTime = current_time();

	#ifdef GSICORE_DYNAMIC_TASK_LIST
	{
		int len = ArrayLength(aCore->mTaskArray);
		if(len > 0)
			allTasksAreDead = 0;
		for(i=(len-1); i>=0; i--)
		{
			GSTask* task = *(GSTask**)ArrayNth(aCore->mTaskArray, i);
			if(gsi_is_true(task->mAutoThink))
				gsCoreTaskThink(task);
			if (theMS != 0 && (current_time()-aStartTime > theMS))
				break;
		}
	}
	#else
		for (i=0; i<GSICORE_MAXTASKS; i++)
		{
			if (aCore->mTaskArray[i] != NULL)
			{
				allTasksAreDead = 0;

				if (aCore->mTaskArray[i]->mAutoThink == gsi_true)
					gsCoreTaskThink(aCore->mTaskArray[i]);
			}
			if (theMS != 0 && (current_time()-aStartTime > theMS))
				break;
		}
	#endif

	if (aCore->mIsShuttingDown && allTasksAreDead)
	{
		ghttpCleanup();

#ifdef GSICORE_DYNAMIC_TASK_LIST
        if(aCore->mTaskArray)
        {
            ArrayFree(aCore->mTaskArray);
            aCore->mTaskArray = NULL;
        }
#endif

		aCore->mIsShuttingDown = 0;
	}

	gsiLeaveCriticalSection(&aCore->mQueueCrit);
	
	GSI_UNUSED(theMS);
}


void gsCoreShutdown()
{
	GSCoreMgr* aCore = gsiGetStaticCore();
	int i=0;

	if (gsi_is_false(aCore->mIsInitialized))
		return;

	gsiEnterCriticalSection(&aCore->mQueueCrit);

	if (gsiInterlockedDecrement(&aCore->mRefCount)>0)
	{
		gsiLeaveCriticalSection(&aCore->mQueueCrit);
		return;
	}
	else
	{
		aCore->mIsShuttingDown = gsi_true;

		#ifdef GSICORE_DYNAMIC_TASK_LIST
		{
			int len = ArrayLength(aCore->mTaskArray);
			for(i=0; i<len; i++)
			{
				gsiCoreCancelTask(*(GSTask**)ArrayNth(aCore->mTaskArray, i));
			}
		}
		#else
			for (i=0; i<GSICORE_MAXTASKS; i++)
			{
				if (aCore->mTaskArray[i] != NULL)
				{
					gsiCoreCancelTask(aCore->mTaskArray[i]);
				}
			}
		#endif
		gsiLeaveCriticalSection(&aCore->mQueueCrit);
	}
}


GSCoreValue gsCoreIsShutdown()
{
	GSCoreMgr* aCore = gsiGetStaticCore();

	if (gsi_is_true(aCore->mIsShuttingDown))
		return GSCore_SHUTDOWN_PENDING;
	if (aCore->mRefCount == 0)
		return GSCore_SHUTDOWN_COMPLETE;

	return GSCore_IN_USE;
}


void gsiCoreExecuteTask(GSTask* theTask, gsi_time theTimeoutMs)
{
	GSCoreMgr* aCore = gsiGetStaticCore();

	GS_ASSERT(!theTask->mIsRunning);

	theTask->mIsCallbackPending = 1;
	theTask->mIsStarted = 1;
	theTask->mIsRunning = 1;
	theTask->mTimeout = theTimeoutMs;
	theTask->mStartTime = current_time();	
	
	if (theTask->mExecuteFunc)
		(theTask->mExecuteFunc)(theTask->mTaskData);

	gsiEnterCriticalSection(&aCore->mQueueCrit);
	#ifdef GSICORE_DYNAMIC_TASK_LIST
		ArrayAppend(aCore->mTaskArray, &theTask);
	#else
	{
		int anInsertPos = -1;
		int i=0;
		for (i=0; i<GSICORE_MAXTASKS; i++)
		{
			if (aCore->mTaskArray[i] == NULL)
			{
				anInsertPos = i;
				break;
			}
		}
		GS_ASSERT(anInsertPos != -1); 
		aCore->mTaskArray[anInsertPos] = theTask;
	}
	#endif
	gsiLeaveCriticalSection(&aCore->mQueueCrit);
}


void gsiCoreCancelTask(GSTask* theTask)
{
	GSCoreMgr* aCore = gsiGetStaticCore();

	gsiEnterCriticalSection(&aCore->mQueueCrit);
	if (theTask->mIsRunning && !theTask->mIsCanceled)
	{
		theTask->mIsCanceled = 1;
		if (theTask->mCancelFunc)
			(theTask->mCancelFunc)(theTask->mTaskData);
	}
	gsiLeaveCriticalSection(&aCore->mQueueCrit);
	GSI_UNUSED(aCore);
}


GSTask* gsiCoreCreateTask()
{
	GSTask* aTask = (GSTask*)gsimalloc(sizeof(GSTask));
	if (aTask == NULL)
		return NULL;

	memset(aTask, 0, sizeof(GSTask));
	aTask->mAutoThink = gsi_true;
	return aTask;
}
