#ifndef __SOAP_H__
#define __SOAP_H__


#include "gsCommon.h"
#include "gsCore.h"

#include "../ghttp/ghttp.h"

#if defined(__cplusplus)
extern "C"
{
#endif


typedef void(*GSSoapCallbackFunc)(GHTTPResult theHTTPResult, GSXmlStreamWriter theRequest, GSXmlStreamReader theResponse, void *theUserData);
typedef void(*GSSoapCustomFunc)(GHTTPPost theSoap, void* theUserData);


typedef struct 
{
	GSSoapCallbackFunc mCallbackFunc;
	GSSoapCustomFunc mCustomFunc;
	const char *mURL;
	const char *mService;

	GSXmlStreamWriter mRequestSoap;
	GSXmlStreamReader mResponseSoap;

	char *    mResponseBuffer; 
	GHTTPPost mPostData; 

	void *   mUserData;
	GSTask * mCoreTask;

	GHTTPRequest mRequestId;
	GHTTPResult  mRequestResult;
	gsi_bool     mCompleted;
} GSSoapTask;


GSSoapTask* gsiExecuteSoap(const char *theURL, const char *theService,
					 GSXmlStreamWriter theSoapData, GSSoapCallbackFunc theCallbackFunc,
					 void *theUserData);

GSSoapTask* gsiExecuteSoapCustom(const char* theURL, const char* theService, 
					 GSXmlStreamWriter theSoapData, GSSoapCallbackFunc theCallbackFunc, 
					 GSSoapCustomFunc theCustomFunc, void* theUserData);


void gsiCancelSoap(GSSoapTask * theTask);


#if defined(__cplusplus)
} 
#endif

#endif 
