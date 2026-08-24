/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "gsCore.h"
#include "gsSoap.h"
#include "gsPlatformThread.h"
#include "gsXML.h"
#include "../ghttp/ghttpASCII.h"




static void gsiSoapTaskExecute(void* theTask);
static void gsiSoapTaskCallback(void* theTask, GSTaskResult theResult);
static void gsiSoapTaskCancel(void* theTask);
static gsi_bool gsiSoapTaskCleanup(void* theTask);
static GSTaskResult gsiSoapTaskThink(void* theTask);

static GHTTPBool gsiSoapTaskHttpCompletedCallback(GHTTPRequest request, GHTTPResult result, 
											 char * buffer, GHTTPByteCount bufferLen, 
											 void * param);


GSSoapTask* gsiExecuteSoap(const char* theURL, const char* theService,
					 GSXmlStreamWriter theRequestSoap, GSSoapCallbackFunc theCallbackFunc, 
					 void* theUserData)
{
	GSSoapTask* aSoapTask = NULL;
	GSTask* aCoreTask = NULL;

	aSoapTask = (GSSoapTask*)gsimalloc(sizeof(GSSoapTask));
	if (aSoapTask == NULL)
		return NULL; 
	
	aSoapTask->mCallbackFunc = theCallbackFunc;
	aSoapTask->mCustomFunc   = NULL;
	aSoapTask->mURL          = theURL;
	aSoapTask->mService      = theService;
	aSoapTask->mRequestSoap  = theRequestSoap;
	aSoapTask->mPostData     = NULL;
	aSoapTask->mResponseSoap = NULL;
	aSoapTask->mResponseBuffer = NULL;
	aSoapTask->mUserData     = theUserData;
	aSoapTask->mRequestResult= (GHTTPResult)0;
	aSoapTask->mCompleted    = gsi_false;

	aCoreTask = gsiCoreCreateTask();
	if (aCoreTask == NULL)
	{
		gsifree(aSoapTask);
		return NULL; 
	}

	aCoreTask->mCallbackFunc = gsiSoapTaskCallback;
	aCoreTask->mExecuteFunc  = gsiSoapTaskExecute;
	aCoreTask->mThinkFunc    = gsiSoapTaskThink;
	aCoreTask->mCleanupFunc  = gsiSoapTaskCleanup;
	aCoreTask->mCancelFunc   = gsiSoapTaskCancel;
	aCoreTask->mTaskData     = (void*)aSoapTask;

	aSoapTask->mCoreTask = aCoreTask;

	gsiCoreExecuteTask(aCoreTask, 0);

	return aSoapTask;
}


GSSoapTask* gsiExecuteSoapCustom(const char* theURL, const char* theService,
					 GSXmlStreamWriter theRequestSoap, GSSoapCallbackFunc theCallbackFunc, 
					 GSSoapCustomFunc theCustomFunc, void* theUserData)
{
	GSSoapTask* aSoapTask = NULL;
	GSTask* aCoreTask = NULL;

	aSoapTask = (GSSoapTask*)gsimalloc(sizeof(GSSoapTask));
	aSoapTask->mCallbackFunc = theCallbackFunc;
	aSoapTask->mCustomFunc   = theCustomFunc;
	aSoapTask->mURL          = theURL;
	aSoapTask->mService      = theService;
	aSoapTask->mRequestSoap  = theRequestSoap;
	aSoapTask->mPostData     = NULL;
	aSoapTask->mResponseSoap = NULL;
	aSoapTask->mResponseBuffer = NULL;
	aSoapTask->mUserData     = theUserData;
	aSoapTask->mRequestResult= (GHTTPResult)0;
	aSoapTask->mCompleted    = gsi_false;
	
	aCoreTask = gsiCoreCreateTask();
	aCoreTask->mCallbackFunc = gsiSoapTaskCallback;
	aCoreTask->mExecuteFunc  = gsiSoapTaskExecute;
	aCoreTask->mThinkFunc    = gsiSoapTaskThink;
	aCoreTask->mCleanupFunc  = gsiSoapTaskCleanup;
	aCoreTask->mCancelFunc   = gsiSoapTaskCancel;
	aCoreTask->mTaskData     = (void*)aSoapTask;

	aSoapTask->mCoreTask = aCoreTask;

	gsiCoreExecuteTask(aCoreTask, 0);

	return aSoapTask;
}


void gsiCancelSoap(GSSoapTask * theTask)
{
	GS_ASSERT(theTask != NULL);

	if (gsi_is_false(theTask->mCompleted))
		gsiCoreCancelTask(theTask->mCoreTask);
}



static GHTTPBool gsiSoapTaskHttpCompletedCallback(GHTTPRequest request, GHTTPResult result, 
											 char * buffer, GHTTPByteCount bufferLen, 
											 void * param)
{
	gsi_bool parseResult = gsi_false;

	GSSoapTask* aSoapTask = (GSSoapTask*)param;
	aSoapTask->mRequestResult = result;
	aSoapTask->mCompleted = gsi_true;
	aSoapTask->mResponseBuffer = buffer;

	if (result == GHTTPSuccess)
	{
		aSoapTask->mResponseSoap = gsXmlCreateStreamReader();
		if (aSoapTask->mResponseSoap == NULL)
		{
			aSoapTask->mRequestResult = GHTTPOutOfMemory;
		}
		else
		{
			parseResult = gsXmlParseBuffer(aSoapTask->mResponseSoap, buffer, (int)bufferLen);
			if (gsi_is_false(parseResult))
			{
				aSoapTask->mRequestResult = GHTTPBadResponse;
			}
		}
	}

	GSI_UNUSED(request);

	return GHTTPFalse; 
}




static GSTaskResult gsiSoapTaskThink(void* theTask)
{
	GSSoapTask* aSoapTask = (GSSoapTask*)theTask;
	if (gsi_is_true(aSoapTask->mCompleted))
		return GSTaskResult_Finished;
	else
	{
		ghttpRequestThink(aSoapTask->mRequestId);
		return GSTaskResult_InProgress;
	}
}


static void gsiSoapTaskExecute(void* theTask)
{
	GSSoapTask* aSoapTask = (GSSoapTask*)theTask;

	GS_ASSERT(gsi_is_false(aSoapTask->mCompleted));

	aSoapTask->mPostData = ghttpNewPost();
	if (aSoapTask->mPostData == NULL)
	{
		aSoapTask->mCompleted = gsi_true;
		aSoapTask->mRequestResult = GHTTPOutOfMemory;
		return;
	}

	ghttpPostSetAutoFree(aSoapTask->mPostData, GHTTPFalse);
	ghttpPostAddXml(aSoapTask->mPostData, aSoapTask->mRequestSoap);

	if (aSoapTask->mCustomFunc != NULL)
		(aSoapTask->mCustomFunc)(aSoapTask->mPostData, aSoapTask->mUserData);


	aSoapTask->mRequestId = ghttpGetExA(aSoapTask->mURL, aSoapTask->mService, 
		NULL, 0, aSoapTask->mPostData, GHTTPFalse, GHTTPFalse, NULL,
		gsiSoapTaskHttpCompletedCallback, (void*)aSoapTask);
}


static void gsiSoapTaskCancel(void* theTask)
{
	GSSoapTask * soapTask = (GSSoapTask*)theTask;
	if (gsi_is_false(soapTask->mCompleted))
	{
		if (soapTask->mRequestId >= 0)
			ghttpCancelRequest(soapTask->mRequestId);
		soapTask->mRequestResult = GHTTPRequestCancelled;
		soapTask->mCompleted = gsi_true;
	}
}


static void gsiSoapTaskCallback(void* theTask, GSTaskResult theResult)
{
	GSSoapTask* aSoapTask = (GSSoapTask*)theTask;

	(aSoapTask->mCallbackFunc)(aSoapTask->mRequestResult, aSoapTask->mRequestSoap, 
		aSoapTask->mResponseSoap, aSoapTask->mUserData);

	GSI_UNUSED(theResult);
}


static gsi_bool gsiSoapTaskCleanup(void *theTask)
{
	GSSoapTask* aSoapTask = (GSSoapTask*)theTask;

	if (aSoapTask->mResponseSoap != NULL)
		gsXmlFreeReader(aSoapTask->mResponseSoap);
	if (aSoapTask->mResponseBuffer != NULL)
		gsifree(aSoapTask->mResponseBuffer);
	if (aSoapTask->mPostData != NULL)
		ghttpFreePost(aSoapTask->mPostData); 
	gsifree(aSoapTask);
	
	return gsi_true;
}
