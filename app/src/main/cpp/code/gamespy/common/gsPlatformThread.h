/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __GSPLATFORMTHREAD_H__
#define __GSPLATFORMTHREAD_H__


#include "gsPlatform.h"


#ifdef __cplusplus
extern "C" {
#endif


#if defined(_WIN32)
	typedef CRITICAL_SECTION   GSICriticalSection;
	typedef HANDLE GSISemaphoreID;
	typedef HANDLE GSIThreadID;
	typedef DWORD (WINAPI *GSThreadFunc)(void *arg);

#elif defined(_PS2)
	typedef int GSIThreadID;
	typedef int GSISemaphoreID;
	typedef struct 
	{
		GSISemaphoreID mSemaphore;
		GSIThreadID mOwnerThread;
		gsi_u32 mEntryCount; 
		gsi_u32 mPad; 
	} GSICriticalSection;
	typedef void (*GSThreadFunc)(void *arg);

#elif defined(_NITRO)
	typedef OSMutex GSICriticalSection;
	typedef struct
	{
		OSMutex mLock;
		gsi_i32 mValue;
		gsi_i32 mMax;
	} GSISemaphoreID;
	typedef struct
	{
		OSThread mThread;
		void * mStack;
	} GSIThreadID;
	typedef void (*GSThreadFunc)(void *arg);

#elif defined(_REVOLUTION)
	typedef OSMutex GSICriticalSection;
	typedef OSSemaphore GSISemaphoreID;
	typedef struct
	{
		OSThread mThread;
		void * mStack;
	} GSIThreadID;
	typedef void *(*GSThreadFunc)(void *arg);

#elif defined(_PSP)
	#define GSI_NO_THREADS
	typedef int GSIThreadID;
	typedef int GSISemaphoreID;
	typedef struct 
	{
		GSISemaphoreID mSemaphore;
		GSIThreadID mOwnerThread;
		gsi_u32 mEntryCount; 
		gsi_u32 mPad; 
	} GSICriticalSection;
	typedef void (*GSThreadFunc)(void *arg);

#elif defined(_PS3)
	#define GSI_NO_THREADS
	typedef int GSIThreadID;
	typedef int GSISemaphoreID;
	typedef struct 
	{
		GSISemaphoreID mSemaphore;
		GSIThreadID mOwnerThread;
		gsi_u32 mEntryCount; 
		gsi_u32 mPad; 
	} GSICriticalSection;
	typedef void (*GSThreadFunc)(void *arg);

#elif defined(_UNIX)  
	#if defined(__OpenBSD__)
		#include <pthread.h>
	#endif

	typedef pthread_mutex_t GSICriticalSection;
	typedef struct
	{
		pthread_mutex_t mLock;
		gsi_i32 mValue;
		gsi_i32 mMax;
	} GSISemaphoreID;
	typedef struct  
	{
		pthread_t thread;
		pthread_attr_t attr;
	} GSIThreadID;
	typedef void (*GSThreadFunc)(void *arg);

#else
	#define GSI_NO_THREADS
#endif

#if defined(WIN32)
	#define GSI_INFINITE INFINITE
#else
	#define GSI_INFINITE (gsi_u32)(-1)
#endif


#if !defined(GSI_NO_THREADS)
	#if defined(_WIN32)
		#define gsiInterlockedIncrement(a) InterlockedIncrement((long*)a)
		#define gsiInterlockedDecrement(a) InterlockedDecrement((long*)a)
	#elif defined(_PS2)
		gsi_u32 gsiInterlockedIncrement(gsi_u32* num);
		gsi_u32 gsiInterlockedDecrement(gsi_u32* num);
	#elif defined(_PS3)
	#elif defined(_NITRO)
		gsi_u32 gsiInterlockedIncrement(gsi_u32* num);
		gsi_u32 gsiInterlockedDecrement(gsi_u32* num);
	#elif defined(_REVOLUTION)
		gsi_u32 gsiInterlockedIncrement(gsi_u32* num);
		gsi_u32 gsiInterlockedDecrement(gsi_u32* num);
	#elif defined(_UNIX)
		gsi_u32 gsiInterlockedIncrement(gsi_u32* num);
		gsi_u32 gsiInterlockedDecrement(gsi_u32* num);
	#endif
	
#else
	#define gsiInterlockedIncrement(a) (++(*a))
	#define gsiInterlockedDecrement(a) (--(*a))
#endif



#if !defined(GSI_NO_THREADS)
    int  gsiStartThread(GSThreadFunc aThreadFunc,  gsi_u32 theStackSize, void *arg, GSIThreadID* theThreadIdOut);
    void gsiCancelThread(GSIThreadID theThreadID);
    void gsiExitThread(GSIThreadID theThreadID);
    void gsiCleanupThread(GSIThreadID theThreadID);

    gsi_u32 gsiHasThreadShutdown(GSIThreadID theThreadID);

    void gsiInitializeCriticalSection(GSICriticalSection *theCrit);
    void gsiEnterCriticalSection(GSICriticalSection *theCrit);
    void gsiLeaveCriticalSection(GSICriticalSection *theCrit);
    void gsiDeleteCriticalSection(GSICriticalSection *theCrit);

    GSISemaphoreID gsiCreateSemaphore(gsi_i32 theInitialCount, gsi_i32 theMaxCount, char* theName);
    gsi_u32        gsiWaitForSemaphore(GSISemaphoreID theSemaphore, gsi_u32 theTimeoutMs);
    void           gsiReleaseSemaphore(GSISemaphoreID theSemaphore, gsi_i32 theReleaseCount);
    void           gsiCloseSemaphore(GSISemaphoreID theSemaphore);

#else
    #define gsiStartThread(a, b, c, d) (-1) 
    #define gsiCancelThread(a)
    #define gsiExitThread(a)
    #define gsiCleanupThread(a)

    #define gsiHasThreadShutdown(a) (1)  

    #define gsiInitializeCriticalSection(a)
    #define gsiEnterCriticalSection(a)
    #define gsiLeaveCriticalSection(a)
    #define gsiDeleteCriticalSection(a)

    #define gsiCreateSemaphore(a,b,c)  (-1)
    #define gsiWaitForSemaphore(a,b) (0)
    #define gsiReleaseSemaphore(a,b)
    #define gsiCloseSemaphore(a)

#endif 
	
#ifdef __cplusplus
}
#endif

#endif 
