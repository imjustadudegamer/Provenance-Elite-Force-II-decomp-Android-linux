/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef __GSIMEMORY_H__
#define __GSIMEMORY_H__


#include "gsCommon.h"

#if(1)
	#define GSI_PRE_ALLOC(x)		
	#define GSI_POST_ALLOC()
#elif(0)
	#define GSI_PRE_ALLOC(x)	gsMemMgrTagPush	(x);
	#define GSI_POST_ALLOC()	gsMemMgrTagPop	();
#elif(0)
	#define GSI_PRE_ALLOC(x)	gsMemMgrContextPush	(x);
	#define GSI_POST_ALLOC()	gsMemMgrContextPop	();
#endif

#if defined (__cplusplus)
extern "C"
{
#endif


typedef enum
{
	MEMTAG_DEFAULT,
	MEMTAG_SERVER_BROWSER,
	MEMTAG_PEER,
	MEMTAG_GP,
	MEMTAG_QR2,
	MEMTAG_NN,
	MEMTAG_GT2,
	MEMTAG_COUNT
} MEMTAG_SDK;

void* gsimalloc		(size_t size);
void* gsirealloc	(void* ptr, size_t size);
void  gsifree		(void* ptr);
void* gsimemalign	(size_t boundary, size_t size); 

#ifdef WIN32
typedef	void	*(__cdecl *gsMallocCB)	(size_t size);
typedef	void	 (__cdecl *gsFreeCB)	(void* ptr);
typedef	void	*(__cdecl *gsReallocCB)	(void* ptr, size_t size);
typedef	void	*(__cdecl *gsMemalignCB)(size_t boundary, size_t size); 
#else
typedef	void	*(*gsMallocCB)	(size_t size);
typedef	void	 (*gsFreeCB)	(void* ptr);
typedef	void	*(*gsReallocCB)	(void* ptr, size_t size);
typedef	void	*(*gsMemalignCB)(size_t boundary, size_t size); 
#endif
void gsiMemoryCallbacksSet(gsMallocCB p_malloc, gsFreeCB p_free, gsReallocCB p_realloc, gsMemalignCB p_memalign);



typedef enum 
{
	gsMemMgrContext_Invalid=  -1,
	gsMemMgrContext_Default=   0,
	gsMemMgrContext_Count  =  16	
}gsMemMgrContext;


gsMemMgrContext	gsMemMgrCreate		(gsMemMgrContext context, const char *PoolName,void* thePoolBuffer, size_t thePoolSize);	

void			gsMemMgrContextPush	(gsMemMgrContext context);
gsMemMgrContext gsMemMgrContextPop	(void);


void			gsMemMgrDestroy(gsMemMgrContext context);	

void 			gsMemMgrDumpStats(void);
void 			gsMemMgrDumpAllocations(void);
void 			gsMemMgrValidateMemoryPool(void);	

gsMemMgrContext gsMemMgrContextFind	(void *ptr);
const char		*MemMgrBufferGetName(gsMemMgrContext context);


void			gsMemMgrTagPush	(gsi_u8 tag);
void			gsMemMgrTagPop	(void);
gsi_u8			gsMemMgrTagGet	(void *ptr);
gsi_u32			gsMemMgrMemUsedByTagGet(gsi_u8 tag);

gsi_u32			gsMemMgrMemAvailGet			(gsMemMgrContext context);
gsi_u32			gsMemMgrMemUsedGet			(gsMemMgrContext context);
gsi_u32			gsMemMgrMemLargestAvailGet	(gsMemMgrContext context);

gsi_u32			gsMemMgrMemHighwaterMarkGet	(gsMemMgrContext context);


void 			gsMemMgrSelfText(void);

#if defined (__cplusplus)
}
#endif

#endif 

