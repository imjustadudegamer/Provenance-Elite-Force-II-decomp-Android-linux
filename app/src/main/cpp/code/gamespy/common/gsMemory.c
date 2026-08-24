/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */


#include "gsPlatform.h"
#include "gsPlatformUtil.h"
#include "gsMemory.h"
#include "gsAssert.h"
#include "gsDebug.h"

#ifdef _PSP
	#include <malloc.h>
#endif

#ifdef _PS3
	#if(0)
		typedef gsi_u64			gsi_uint;
		#define PTR_ALIGNMENT	32
		#define GSI_64BIT		(1) 
		#define GS_BIG_ENDIAN
	#else
		typedef gsi_u32			gsi_uint;
		#define PTR_ALIGNMENT	16	
		#define GSI_64BIT		(0) 
		#define GS_BIG_ENDIAN
	#endif
#else
	typedef gsi_u32			gsi_uint;
	#define PTR_ALIGNMENT	16
	#define GSI_64BIT		(0)
#endif




#define MEM_PROFILE	(1)	
#if defined(MEM_PROFILE)
	#define IF_MEM_PROFILE_ISON(a) a
#else
	#define IF_MEM_PROFILE_ISON(a)
#endif

#if defined(_MSC_VER) 
	#pragma warning ( disable: 4127 )
#include <malloc.h>
#endif 

#ifdef _WIN32
	#define MEM_MANAGER_CALL _cdecl
#else
	#define MEM_MANAGER_CALL
#endif


typedef struct 
{
	void* (MEM_MANAGER_CALL *malloc  )(size_t size);
	void  (MEM_MANAGER_CALL *free    )(void* ptr);
	void* (MEM_MANAGER_CALL *realloc )(void* ptr, size_t size);
	void* (MEM_MANAGER_CALL *memalign)(size_t boundary, size_t size);
}MemManagerCallbacks;

static void* MEM_MANAGER_CALL _gsi_malloc(size_t size)
{
	return malloc(size);
}

static void MEM_MANAGER_CALL _gsi_free(void* ptr)
{
	free(ptr);
}

static void* MEM_MANAGER_CALL _gsi_realloc(void* ptr, size_t size)
{
	return realloc(ptr, size);
}

#if defined(_PS2) || defined(_PSP) || defined(_PS3)
	static void* _gsi_memalign(size_t boundary, size_t size)
	{
		return memalign(boundary, size);
	}
#elif defined (_WIN32)
	#if (_MSC_VER < 1300)
		extern void* __cdecl _aligned_malloc(size_t size, int boundary);
	#endif
	static void* __cdecl _gsi_memalign(size_t boundary, size_t size)
	{
		return  _aligned_malloc(size, (int)boundary);
	}
#else
	static void* _gsi_memalign(size_t boundary, size_t size)
	{
		void *ptr = calloc((size)/(boundary), (boundary));
		GS_ASSERT((((size_t)ptr)% boundary)==0);
		return ptr;
	}
#endif

static MemManagerCallbacks memmanagercallbacks =
{
#ifdef MEM_MANAGER_DIRECT
	&malloc,
	&free,
	&realloc,
	#if defined(_PS2) || defined(_PSP) || defined(_PS3)
		&memalign,		
	#else	
		&_gsi_memalign,	
	#endif
#else
	&_gsi_malloc,
	&_gsi_free,
	&_gsi_realloc,
	&_gsi_memalign
#endif
};


void gsiMemoryCallbacksSet(gsMallocCB p_malloc, gsFreeCB p_free, gsReallocCB p_realloc, gsMemalignCB p_memalign)
{

	memmanagercallbacks.malloc		= 	p_malloc;
	memmanagercallbacks.free		= 	p_free;
	memmanagercallbacks.realloc		= 	p_realloc;
	memmanagercallbacks.memalign	= 	p_memalign;
}
	
	
	
	 


void* gsimalloc		(size_t size)
{
	return (*memmanagercallbacks.malloc)(size);
}
void* gsirealloc	(void* ptr, size_t size)
{
	return (*memmanagercallbacks.realloc)(ptr,size);
}
void  gsifree		(void* ptr)
{
	if(ptr == NULL)
		return;
	(*memmanagercallbacks.free)(ptr);
}
void* gsimemalign	(size_t boundary, size_t size)
{
	return (*memmanagercallbacks.memalign)(boundary,size);
}



#ifdef GSI_MEM_MANAGED






#define MEM_CONTEXT_STACK_MAX	10			
static	gsMemMgrContext	MemTypeStack	[MEM_CONTEXT_STACK_MAX]	= {gsMemMgrContext_Default};	
static	gsi_u32			MemTypeStackIndex						= 0;
extern	gsMemMgrContext gsMemMgrContextCurrent; 

#define MEM_TAG_STACK_MAX	10			
static	gsi_u8			MemTagStack	[MEM_TAG_STACK_MAX]		= {0};	
static	gsi_u32			MemTagStackIndex					= 0;








#define MEM_IS_POWER_OF_2(x)	(((x) & ((x)-1)) == 0)	
#define MEMALIGN_POWEROF2(x,a)	(((gsi_uint)(x)+(a-1)) &~ ( ((gsi_uint)(a)) -1))

#if(1)	 
	#define MP_ASSERT(x)	GS_ASSERT(x)
#else
	#define MP_ASSERT(x)	
#endif


#define MEM_TYPES_MAX	127


typedef struct 
{
	gsi_u32			MemTotal;
	gsi_u32			MemAvail;
	gsi_u32			MemUsed;
	gsi_u32			MemUsed_At_HighWater;
	gsi_u32			MemWasted;					

	gsi_u32			ChunksCount;				
	gsi_u32			ChunksFreeCount;			
	gsi_u32			ChunksFreeLargestAvail;
	gsi_u32			ChunksUsedCount;			
	gsi_u32			ChunksUsedCount_At_HighWater;			
	
	gsi_u32			MemType_ChunksCount				[MEM_TYPES_MAX];
	gsi_u32			MemType_MemUsed					[MEM_TYPES_MAX];
	gsi_u32			MemType_MemUsed_At_HighWater	[MEM_TYPES_MAX];


} MEM_STATS;

void MEM_STATSAddAll	(MEM_STATS *_this,	const MEM_STATS *ms);
void MEM_STATSClear		(MEM_STATS *_this);
void MEM_STATSClearAll	(MEM_STATS *_this);


typedef struct  tMEM_CHUNK
{

		union
		{
			gsi_uint		MemUsed;		
			#ifdef GS_BIG_ENDIAN
				struct
				{
					#if (GSI_64BIT)
						char			pad[7],MemType;
					#else
						char			pad[3],MemType;
					#endif
				}MEM_TypeStruct;
			#else
				struct
				{
					#if (GSI_64BIT)
						char			MemType,pad[7];
					#else
						char			MemType,pad[3];
					#endif
				} MEM_TypeStruct;
			#endif
		} MEM_UsageStat;

	struct tMEM_CHUNK		*prev;
	struct tMEM_CHUNK		*next;			
	struct tMEM_CHUNK		*NextFree;		
} MEM_CHUNK;

	

void	MEM_CHUNKAlloc	(MEM_CHUNK *_this, gsi_u8 _MemType, size_t _UsedSize)	
{
	_UsedSize = MEMALIGN_POWEROF2(_UsedSize,4);		
	GS_ASSERT_STR(_UsedSize < 0x3FFFFFC, "Alloc Memory size is too big.");
	_this->MEM_UsageStat.MemUsed = _UsedSize<<6; 
	_this->MEM_UsageStat.MEM_TypeStruct.MemType = _MemType;	
}
void	MEM_CHUNKFree	(MEM_CHUNK *_this)								
{ 
	_this->MEM_UsageStat.MemUsed = 0;	
}

gsi_bool	MEM_CHUNKIsFree	(MEM_CHUNK *_this)			
{ 
	return (_this->MEM_UsageStat.MemUsed == 0);		
}

gsi_u32		MEM_CHUNKTotalSizeGet(MEM_CHUNK *_this)		
{ 
	if (!_this->next)
	{ 
		return PTR_ALIGNMENT + sizeof(MEM_CHUNK);	
	}
	return (gsi_uint) _this->next - (gsi_uint) _this;							
}	

gsi_u32		MEM_CHUNKChunkSizeGet(MEM_CHUNK *_this)		
{	
	if (!_this->next) 
		return PTR_ALIGNMENT;;							
	return (gsi_uint) _this->next - (gsi_uint) _this - sizeof(MEM_CHUNK);	
}	

gsi_u32		MEM_CHUNKMemUsedGet (MEM_CHUNK *_this)				
{ 
	return (_this->MEM_UsageStat.MemUsed & ~0xFF)>>6;					
}	

void	MEM_CHUNKMemUsedSet (MEM_CHUNK *_this,	gsi_u32 size)		
{ 
	_this->MEM_UsageStat.MemUsed = (MEMALIGN_POWEROF2(size,4)<<6) + _this->MEM_UsageStat.MEM_TypeStruct.MemType;		
}	

gsi_u32		MEM_CHUNKMemAvailGet(MEM_CHUNK *_this)				
{ 
	return MEM_CHUNKChunkSizeGet(_this) - MEM_CHUNKMemUsedGet(_this);			
}		

char	MEM_CHUNKMemTypeGet (MEM_CHUNK *_this)				
{
	return _this->MEM_UsageStat.MEM_TypeStruct.MemType;			
}		

void	MEM_CHUNKMemTypeSet (MEM_CHUNK *_this,	char _MemType)	
{ 
	GS_ASSERT(_MemType < MEM_TYPES_MAX);
	_this->MEM_UsageStat.MEM_TypeStruct.MemType = _MemType;	
}

void*	MEM_CHUNKMemPtrGet  (MEM_CHUNK *_this)				
{ 
	return (void*)((gsi_uint) _this + sizeof(MEM_CHUNK));		
}

MEM_CHUNK *Ptr_To_MEM_CHUNK(void *ptr)	
{ 
	return ((MEM_CHUNK *)ptr)-1; 
}

typedef struct MEM_CHUNK_POOL
{
	char		Name[20];						
	MEM_CHUNK	*HeaderStart;
	MEM_CHUNK	*HeaderEnd;
	MEM_CHUNK	*pFirstFree;
	gsi_u32		HeapSize;
	#if MEM_PROFILE
		gsi_u32		HWMemUsed;
		gsi_u32		MemUsed;
	#endif
} MEM_CHUNK_POOL;

MEM_CHUNK	*MEM_CHUNK_POOLFindPreviousFreeChunk		(MEM_CHUNK_POOL *_this,	MEM_CHUNK *header);
MEM_CHUNK	*MEM_CHUNK_POOLFindNextFreeChunk			(MEM_CHUNK_POOL *_this,	MEM_CHUNK *header);
void		 MEM_CHUNK_POOLSplitChunk					(MEM_CHUNK_POOL *_this,	MEM_CHUNK *header,gsi_bool ReAlloc);
void		 MEM_CHUNK_POOLFreeChunk					(MEM_CHUNK_POOL *_this,	MEM_CHUNK *header);
MEM_CHUNK	*MEM_CHUNK_POOLAllocChunk					(MEM_CHUNK_POOL *_this,	size_t Size,int Alignment , gsi_bool Backwards 	);

void		MEM_CHUNK_POOLChunkMove						(MEM_CHUNK_POOL *_this,	MEM_CHUNK *oldpos, MEM_CHUNK *newpos);

void		MEM_CHUNK_POOLCreate						(MEM_CHUNK_POOL *_this,	 const char *szName, char *ptr, gsi_u32 _size);
void		MEM_CHUNK_POOLDestroy						(MEM_CHUNK_POOL *_this)		;
gsi_bool		MEM_CHUNK_POOLIsValid						(MEM_CHUNK_POOL *_this)		
{	
	return _this->HeapSize > 0;	
}


void		*MEM_CHUNK_POOLmalloc						(MEM_CHUNK_POOL *_this,	size_t Size,	gsi_i32 Alignment );
void		*MEM_CHUNK_POOLmalloc_backwards				(MEM_CHUNK_POOL *_this,	size_t Size,	gsi_i32 Alignment );
void		*MEM_CHUNK_POOLrealloc						(MEM_CHUNK_POOL *_this,	void *oldmem,	size_t newSize);
void		 MEM_CHUNK_POOLfree							(MEM_CHUNK_POOL *_this,	void *mem);

void		MEM_CHUNK_POOLCheckValidity					(MEM_CHUNK_POOL *_this	);
void		MEM_CHUNK_POOLMemStatsGet					(MEM_CHUNK_POOL *_this,	MEM_STATS *stats);
gsi_u32		MEM_CHUNK_POOLWalkForType					(MEM_CHUNK_POOL *_this,	int _MemType, gsi_bool _LogUse);

gsi_bool		MEM_CHUNK_POOLIsHeapPtr						(MEM_CHUNK_POOL *_this,	void * mem);

void		MEM_CHUNK_POOLFillMemoryTable				(MEM_CHUNK_POOL *_this,	char *Table, const int TableSize, gsi_u32 _HeapStart, gsi_u32 _HeapSize);

gsi_bool		MEM_CHUNK_POOLItemIsInPoolMemory			(MEM_CHUNK_POOL *_this,	void *ptr)	
{ 
	GS_ASSERT(MEM_CHUNK_POOLIsValid(_this));	
	return (((gsi_uint)ptr >=  (gsi_uint)MEM_CHUNKMemPtrGet(_this->HeaderStart)) &&((gsi_uint)ptr <=  (gsi_uint)MEM_CHUNKMemPtrGet(_this->HeaderEnd)));
}
			









void MEM_STATSAddAll(MEM_STATS *_this, const MEM_STATS *ms)
{
	int i;
	_this->MemTotal					+=	ms->MemTotal				;
	_this->MemAvail					+=	ms->MemAvail				;
	_this->MemUsed					+=	ms->MemUsed					;
	_this->MemUsed_At_HighWater		+=	ms->MemUsed_At_HighWater	;
	_this->MemWasted				+=	ms->MemWasted				;		
	_this->ChunksCount				+=	ms->ChunksCount				;		
	_this->ChunksFreeCount			+=	ms->ChunksFreeCount			;	
	_this->ChunksFreeLargestAvail	+=	ms->ChunksFreeLargestAvail	;
	_this->ChunksUsedCount			+=	ms->ChunksUsedCount			;	
	_this->ChunksUsedCount_At_HighWater		+=	ms->ChunksUsedCount_At_HighWater;	
	for (i =0; i<MEM_TYPES_MAX;i++)
	{
		_this->MemType_ChunksCount[i]	+=ms->MemType_ChunksCount[i];
		_this->MemType_MemUsed[i]		+=ms->MemType_MemUsed[i]	;
	}

}

void MEM_STATSClear(MEM_STATS *_this )
{
	_this->MemTotal				=	0;
	_this->MemAvail				=	0;
	_this->MemUsed					=	0;
	_this->MemWasted				=	0;		
	_this->ChunksCount				=	0;		
	_this->ChunksFreeCount			=	0;	
	_this->ChunksFreeLargestAvail	=	0;
	_this->ChunksUsedCount			=	0;	

	memset(_this->MemType_ChunksCount,	0,4 * MEM_TYPES_MAX);
	memset(_this->MemType_MemUsed,		0,4 * MEM_TYPES_MAX);

}

void MEM_STATSClearAll(MEM_STATS *_this )
{
	int i;
	MEM_STATSClear(_this);
	_this->MemUsed_At_HighWater					=	0;
	for (i=0;i< MEM_TYPES_MAX;i++ )
		_this->MemType_MemUsed_At_HighWater[i]	=	0;	
	_this->ChunksUsedCount_At_HighWater			=	0;	
}



void	MEM_CHUNK_POOLChunkMove	(MEM_CHUNK_POOL *_this, MEM_CHUNK *oldpos, MEM_CHUNK *newpos)
{
	MEM_CHUNK *firstfree;
	MEM_CHUNK temp = *oldpos;

	MP_ASSERT(oldpos->prev)
	MP_ASSERT(oldpos->next)

	MP_ASSERT((gsi_uint) newpos <= (gsi_uint)oldpos->next - MEM_CHUNKMemUsedGet(oldpos)			- sizeof(MEM_CHUNK))
	MP_ASSERT((gsi_uint) newpos >= (gsi_uint)oldpos->prev + MEM_CHUNKMemUsedGet(oldpos->prev)	+ sizeof(MEM_CHUNK))

	MP_ASSERT((((gsi_uint) newpos) % sizeof(MEM_CHUNK)) == 0)

	*newpos = temp;

	newpos->prev->next = newpos;
	newpos->next->prev = newpos;

	if (MEM_CHUNKIsFree(newpos))
	{

		if (_this->pFirstFree == oldpos)
			_this->pFirstFree = newpos;
		else
		{
			firstfree = MEM_CHUNK_POOLFindPreviousFreeChunk(_this,newpos->prev);
			if (firstfree != newpos)
				firstfree->NextFree = newpos;
			else
			{
				_this->pFirstFree = newpos;
			}

			MP_ASSERT((newpos->NextFree==NULL) || ((gsi_uint)newpos->NextFree > (gsi_uint)newpos))
		}
	}


}

void MEM_CHUNK_POOLDestroy(MEM_CHUNK_POOL *_this)
{
	memset(_this, 0, sizeof (MEM_CHUNK_POOL));
}
void MEM_CHUNK_POOLCreate(MEM_CHUNK_POOL *_this, const char * szNameIn, char *ptr, gsi_u32 size)
{
	int len;
	MEM_CHUNK *HeaderMid;
	MP_ASSERT(((gsi_uint)ptr & 15 )==0) 

	len = strlen(szNameIn)+1;
	if (len > 20) len = 20;
	memcpy(_this->Name,szNameIn, len);
	_this->Name[19]='\0';	

	MP_ASSERT(size >  48 + 3 * sizeof(MEM_CHUNK))

	_this->HeaderStart  = (MEM_CHUNK *)	(ptr);
	HeaderMid			= (MEM_CHUNK *)	(ptr + 2 * sizeof(MEM_CHUNK));
	_this->HeaderEnd	= (MEM_CHUNK *)	(ptr + size - 2 * sizeof(MEM_CHUNK));

	_this->HeaderStart->prev		= NULL;
	_this->HeaderStart->next		= HeaderMid;
	_this->HeaderStart->NextFree	= HeaderMid;
	MEM_CHUNKAlloc		(_this->HeaderStart,0,sizeof(MEM_CHUNK));		

	HeaderMid->prev			= _this->HeaderStart;
	HeaderMid->next			= _this->HeaderEnd;
	HeaderMid->NextFree		= 0;
	MEM_CHUNKFree(HeaderMid);

	_this->HeaderEnd->prev			= HeaderMid;
	_this->HeaderEnd->next			= NULL;
	_this->HeaderEnd->NextFree		= NULL;
	MEM_CHUNKAlloc		(_this->HeaderEnd,0,sizeof(MEM_CHUNK));		

	_this->HeapSize		= size;
	_this->pFirstFree	= HeaderMid;

}


MEM_CHUNK *MEM_CHUNK_POOLFindPreviousFreeChunk(MEM_CHUNK_POOL *_this, MEM_CHUNK *header)
{
	while ((header) && (!MEM_CHUNKIsFree(header)))
	{
		header = header->prev;
	}

	GSI_UNUSED(_this);
	return header;
}

MEM_CHUNK *MEM_CHUNK_POOLFindNextFreeChunk(MEM_CHUNK_POOL *_this, MEM_CHUNK *header_in)
{
	MEM_CHUNK *header = header_in;
	while ((header) && (!MEM_CHUNKIsFree(header)))
	{
		header = header->next;
	}
	if (header == header_in)
		return NULL;

	GSI_UNUSED(_this);
	return header;
}




void MEM_CHUNK_POOLSplitChunk(MEM_CHUNK_POOL *_this, MEM_CHUNK *header, gsi_bool ReAlloc)
{
	MEM_CHUNK *next;
	MEM_CHUNK *PrevFree;
	MEM_CHUNK *NewHeader;

	NewHeader = (MEM_CHUNK *) ((gsi_u8*)header + MEM_CHUNKMemUsedGet(header) + sizeof(MEM_CHUNK));
	NewHeader = (MEM_CHUNK *)MEMALIGN_POWEROF2(NewHeader,sizeof(MEM_CHUNK));
	
	MP_ASSERT ((gsi_uint)NewHeader  + 2 * sizeof(MEM_CHUNK) <= (gsi_uint)header->next)
	
	#if (MEM_PROFILE)
		if(ReAlloc)
		{
			_this->MemUsed -= MEM_CHUNKChunkSizeGet(header);
			GS_ASSERT(_this->MemUsed >= 0);
		}
	#endif


	NewHeader->prev		= header;
	NewHeader->next		= header->next;
	MEM_CHUNKFree(NewHeader);

	if (NewHeader->next)
	{
		NewHeader->next->prev = NewHeader;
	}

	header->next		= NewHeader;


	if (MEM_CHUNKIsFree(NewHeader->next))
	{
		MP_ASSERT(ReAlloc)

		next				= NewHeader->next->next;
		next->prev			= NewHeader;		

		NewHeader->NextFree = NewHeader->next->NextFree;
		NewHeader->next		= next;
	}
	else
	{
		if (ReAlloc)
		{
			NewHeader->NextFree = MEM_CHUNK_POOLFindNextFreeChunk(_this,NewHeader->next);
		}
		else
			NewHeader->NextFree = header->NextFree;
	}

	if (_this->pFirstFree == header)
	{
		_this->pFirstFree = NewHeader;
	}
	else
	{
		PrevFree = MEM_CHUNK_POOLFindPreviousFreeChunk(_this,header);
		if (PrevFree)
			PrevFree->NextFree	=  NewHeader;
		else
			_this->pFirstFree			=  NewHeader;
	}

	#if (MEM_PROFILE)
		if(ReAlloc)
		{
			_this->MemUsed += MEM_CHUNKMemUsedGet(header);
			if(_this->MemUsed > _this->HWMemUsed)
				_this->HWMemUsed = _this->MemUsed;
			
			GS_ASSERT(_this->MemUsed <= _this->HeapSize);
		}
	#endif

#ifdef _DEBUG_
		header->NextFree = NULL;
#endif

}


gsi_bool	MEM_CHUNK_POOLIsHeapPtr(MEM_CHUNK_POOL *_this, void * mem)
{
	MEM_CHUNK *headertofind = Ptr_To_MEM_CHUNK(mem);
	MEM_CHUNK *header		= _this->HeaderStart;

	while (header)
	{
		header = header->next;
		if (headertofind == header)
			return gsi_true;
	}

	return gsi_false;

}







MEM_CHUNK *MEM_CHUNK_POOLAllocChunk(MEM_CHUNK_POOL *_this,size_t Size, gsi_i32 Alignment, gsi_bool Backwards)

{
	gsi_u32 Ptr				;
	gsi_u32 AlignedPtr		;
	int	delta			;
	MEM_CHUNK *PrevFree	;
	int total_size		;
	int MemRemain		;
	MEM_CHUNK *alignedheader;


	MEM_CHUNK *header; 
	gsi_u32 SizeNeeded		= Size + sizeof(MEM_CHUNK);
	SizeNeeded = MEMALIGN_POWEROF2(SizeNeeded,sizeof(MEM_CHUNK));	

	MP_ASSERT(Size)	
	MP_ASSERT(MEM_IS_POWER_OF_2(Alignment))		
	MP_ASSERT(Alignment >= PTR_ALIGNMENT)						
	


	if(Backwards)
		header = MEM_CHUNK_POOLFindPreviousFreeChunk(_this,_this->HeaderEnd);
	else
		header = _this->pFirstFree;


	while (header)
	{	
		MP_ASSERT (MEM_CHUNKIsFree(header))

		total_size	= MEM_CHUNKTotalSizeGet(header); 
		MemRemain	= total_size - SizeNeeded;

		if (MemRemain >= 0 )
		{
			Ptr			= (gsi_uint)MEM_CHUNKMemPtrGet(header);
			AlignedPtr	= MEMALIGN_POWEROF2(Ptr,Alignment);
			delta		= AlignedPtr - Ptr;
			if (delta)
			{
				if (MemRemain < delta)
				{
					header = header->NextFree;
					continue;
				}

				alignedheader = Ptr_To_MEM_CHUNK((void*)(gsi_uint)AlignedPtr);
				MEM_CHUNK_POOLChunkMove	(_this,header,alignedheader);
				header		= alignedheader;
				MemRemain  -= delta;

			}


			
			MEM_CHUNKAlloc(header,MemTagStack[MemTagStackIndex],Size);

			if (MemRemain > sizeof(MEM_CHUNK)*2)
			{

				MEM_CHUNK_POOLSplitChunk(_this,header, gsi_false);
			}
			else
			{
				if (_this->pFirstFree == header)
				{
					_this->pFirstFree = header->NextFree;

				}
				else
				{
					PrevFree	= MEM_CHUNK_POOLFindPreviousFreeChunk(_this,header);
					if (PrevFree)
						PrevFree->NextFree	= header->NextFree;
					else
						_this->pFirstFree 			= header->NextFree;

				}
			}
			{
				#if (MEM_PROFILE)
					_this->MemUsed += MEM_CHUNKMemUsedGet(header);
					if(_this->MemUsed > _this->HWMemUsed)
						_this->HWMemUsed = _this->MemUsed;
					
					GS_ASSERT(_this->MemUsed <= _this->HeapSize);
				#endif
			}
			return header;

		}
		if (Backwards)
			header = MEM_CHUNK_POOLFindPreviousFreeChunk(_this,header);
		else
			header = header->NextFree;
	}
	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice," Could not allocate %i bytes\n", Size);
	GS_ASSERT_STR(0,"Out of memory");
							

	return NULL;

}



void MEM_CHUNK_POOLFreeChunk(MEM_CHUNK_POOL *_this,MEM_CHUNK *header)
{

	MEM_CHUNK *prev = header;
	MEM_CHUNK *next = header;
	MEM_CHUNK *PrevFree;

	#if (MEM_PROFILE)
		_this->MemUsed -= MEM_CHUNKMemUsedGet(header);
		GS_ASSERT(_this->MemUsed >= 0);
	#endif

	while (next->next && (MEM_CHUNKIsFree(next->next)))
	{
		next = next->next;
	}

	while (prev->prev && (MEM_CHUNKIsFree(prev->prev)))
	{
		prev = prev->prev;
	}

	if (prev != next)
	{
		prev->next		= next->next;

		if (next->next)
			next->next->prev = prev;

	}


	PrevFree = MEM_CHUNK_POOLFindPreviousFreeChunk(_this,prev);
	if (PrevFree == NULL)
	{
		_this->pFirstFree	=  prev;

	}
	else
	{
		PrevFree->NextFree	=  prev;
	}

	if(next->next)
		prev->NextFree	= MEM_CHUNK_POOLFindNextFreeChunk(_this,next->next);
	else
		prev->NextFree	= NULL;

	MEM_CHUNKFree(prev);


#if(0)
	gsi_u32 destptr	= (gsi_u32)prev->prev + prev->prev->MemAvailGet() + sizeof(MEM_CHUNK);
	destptr	= MEMALIGN_POWEROF2(destptr,sizeof(MEM_CHUNK));

	if	(destptr < (gsi_u32)prev )
		ChunkMove(prev,(MEM_CHUNK *)destptr);
#endif
}




void *MEM_CHUNK_POOLmalloc(MEM_CHUNK_POOL *_this,size_t Size, gsi_i32 Alignment)
{
	void *mem;

	MEM_CHUNK *header = MEM_CHUNK_POOLAllocChunk( _this,Size, Alignment, gsi_false);

	if (header)
	{		
		mem = MEM_CHUNKMemPtrGet(header);
		return mem;
	}

	return NULL;
}


void *MEM_CHUNK_POOLmalloc_backwards(MEM_CHUNK_POOL *_this,size_t Size, gsi_i32 Alignment)
{
	void *mem;
 
	MEM_CHUNK *header = MEM_CHUNK_POOLAllocChunk( _this,Size, Alignment, gsi_true);

	if (header)
	{
		mem = MEM_CHUNKMemPtrGet(header);
		return mem;
	}

	return NULL;
}


void MEM_CHUNK_POOLfree(MEM_CHUNK_POOL *_this,void *mem)
{
	MEM_CHUNK *header = Ptr_To_MEM_CHUNK(mem);
	MEM_CHUNK_POOLFreeChunk(_this,header);
}


void *MEM_CHUNK_POOLrealloc(MEM_CHUNK_POOL *_this,void *oldmem, size_t newSize)
{
	MEM_CHUNK	*oldheader;
	MEM_CHUNK	*NewHeader;
	gsi_u32			OldSize;
	char		MemType;

	MP_ASSERT(newSize)

	if (!oldmem)	
	{
		return MEM_CHUNK_POOLmalloc( _this, newSize,PTR_ALIGNMENT);
	}


	oldheader	= Ptr_To_MEM_CHUNK(oldmem);
	OldSize		= MEM_CHUNKMemUsedGet(oldheader);

	if	(newSize == OldSize)
		return oldmem; 

	if	(newSize <  OldSize )
	{

		if	((newSize + 2 * sizeof(MEM_CHUNK))>  OldSize )
		{
			return oldmem;
		}

		MEM_CHUNKMemUsedSet(oldheader,newSize);
		MEM_CHUNK_POOLSplitChunk(_this,oldheader, gsi_true);
		return MEM_CHUNKMemPtrGet(oldheader);
	}
	else
	{
		MemType = MEM_CHUNKMemTypeGet(oldheader);
		MEM_CHUNK_POOLFreeChunk(_this,oldheader);
		NewHeader = MEM_CHUNK_POOLAllocChunk( _this,newSize,PTR_ALIGNMENT,gsi_false);
		MEM_CHUNKMemTypeSet(NewHeader,MemType);

		memmove(MEM_CHUNKMemPtrGet(NewHeader),oldmem,OldSize);

		return MEM_CHUNKMemPtrGet(NewHeader);
	}

}

void MEM_CHUNK_POOLMEM_CHUNK_POOL(MEM_CHUNK_POOL *_this)
{
	_this->Name[0]		= 0;
	_this->HeaderEnd	= NULL;
	_this->HeaderStart	= NULL;
	_this->HeapSize		= 0;
	_this->pFirstFree	= NULL;
}





gsi_u32 MEM_CHUNK_POOLWalkForType(MEM_CHUNK_POOL *_this,int type, gsi_bool _LogUse)
{
	MEM_CHUNK *header;
	gsi_u32	Total = 0;
	header	  = _this->HeaderStart;

	while (header) 
	{
		MP_ASSERT((header->next		== NULL) || ((gsi_uint)header		< (gsi_uint)header->next	))	
		MP_ASSERT((header->prev		== NULL) || ((gsi_uint)header->prev	< (gsi_uint)header		))	
		MP_ASSERT((header->prev		== NULL) || (header->prev->next == header))				
		MP_ASSERT((header->next		== NULL) || (header->next->prev == header))				
		MP_ASSERT( MEM_CHUNKMemUsedGet(header)  <= MEM_CHUNKChunkSizeGet(header)  )			
		
		if (!MEM_CHUNKIsFree(header) && (MEM_CHUNKMemTypeGet(header) == type))
		{
			if ((header != _this->HeaderStart) && (header != _this->HeaderEnd))
			{
				Total += MEM_CHUNKTotalSizeGet(header);
				if (_LogUse)
				{
					gsDebugFormat(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice,"MemFound ptr:0x%8x  size:%8u %s\n", MEM_CHUNKMemPtrGet(header),
						MEM_CHUNKMemUsedGet(header),MemMgrBufferGetName((gsMemMgrContext) type));				
				}
			}

		}
		
		MP_ASSERT (header->next || (header == _this->HeaderEnd))
		header = header->next;

	}
	return Total;
}


void MEM_CHUNK_POOLMemStatsGet(MEM_CHUNK_POOL *_this,MEM_STATS *pS)
{
	int	ChunksFreeLostCount ;
	int i,type;
	MEM_CHUNK *header	;
	MEM_CHUNK *NextFree;	
	MEM_STATSClear(pS);

	header		= _this->HeaderStart;
	NextFree	= _this->pFirstFree;


	
	while (header) 
	{
		MP_ASSERT((header->next		== NULL) || ((gsi_uint)header		< (gsi_uint)header->next	))	
		MP_ASSERT((header->prev		== NULL) || ((gsi_uint)header->prev	< (gsi_uint)header		))	
		MP_ASSERT((header->prev		== NULL) || (header->prev->next == header))				
		MP_ASSERT((header->next		== NULL) || (header->next->prev == header))				
		MP_ASSERT( MEM_CHUNKMemUsedGet(header)  <= MEM_CHUNKChunkSizeGet(header)  )							
		
		pS->MemTotal	+= MEM_CHUNKTotalSizeGet(header);
		if (!MEM_CHUNKIsFree(header))
		{
			pS->ChunksUsedCount++;
			if (pS->ChunksUsedCount_At_HighWater < pS->ChunksUsedCount)
				pS->ChunksUsedCount_At_HighWater = pS->ChunksUsedCount;

			pS->MemWasted	+= MEM_CHUNKTotalSizeGet(header) - MEM_CHUNKMemUsedGet(header);
			pS->MemUsed		+= MEM_CHUNKTotalSizeGet(header);

			type = MEM_CHUNKMemTypeGet(header);
			pS->MemType_MemUsed[type]	 += MEM_CHUNKTotalSizeGet(header);
			pS->MemType_ChunksCount[type]++;

		}
		else
		{
			MP_ASSERT((header->NextFree	== NULL) || ((gsi_uint)header	< (gsi_uint)header->NextFree	))	
			
			MP_ASSERT((header->next		== NULL) || (!MEM_CHUNKIsFree(header->next)))	
			MP_ASSERT((header->prev		== NULL) || (!MEM_CHUNKIsFree(header->prev)))	
			
			MP_ASSERT(header == NextFree)						
			NextFree	= header->NextFree;

			pS->MemWasted	+= MEM_CHUNKTotalSizeGet(header) - MEM_CHUNKChunkSizeGet(header);
			pS->MemUsed		+= MEM_CHUNKTotalSizeGet(header) - MEM_CHUNKChunkSizeGet(header);

			pS->ChunksFreeCount++;
			if (pS->ChunksFreeLargestAvail < MEM_CHUNKChunkSizeGet(header))
				pS->ChunksFreeLargestAvail = MEM_CHUNKChunkSizeGet(header);
		}
		
		pS->ChunksCount++;

		MP_ASSERT (header->next || (header == _this->HeaderEnd))
		header = header->next;

	}

	header			= _this->HeaderStart;


	while(header && (!MEM_CHUNKIsFree(header)))
		header = header->next;

	MP_ASSERT(_this->pFirstFree == header)

	ChunksFreeLostCount = pS->ChunksFreeCount;
	while (header) 
	{
		ChunksFreeLostCount	--;
		pS->MemAvail	+=MEM_CHUNKChunkSizeGet(header);
		header = header->NextFree;

	}


	if (pS->MemUsed_At_HighWater < pS->MemUsed)
		pS->MemUsed_At_HighWater = pS->MemUsed;

	for ( i=0;i< MEM_TYPES_MAX;i++ )
	{
		if (pS->MemType_MemUsed_At_HighWater[i]	<	pS->MemType_MemUsed[i] )	
			pS->MemType_MemUsed_At_HighWater[i]	=	pS->MemType_MemUsed[i];	
	}

	MP_ASSERT(ChunksFreeLostCount == 0)	
}

void MEM_CHUNK_POOLCheckValidity(MEM_CHUNK_POOL *_this)
{
	MEM_STATS stats;
	MEM_CHUNK_POOLMemStatsGet(_this,&stats);

}


void MEM_CHUNK_POOLFillMemoryTable(MEM_CHUNK_POOL *_this,char *Table, const int TableSize, gsi_u32 _HeapStart, gsi_u32 _HeapSize)
{
	int s,e,j;
	gsi_u32 start_address;
	gsi_u32 end_address	;
	MEM_CHUNK	*pChunk = _this->HeaderStart;
	MP_ASSERT(_this->HeapSize)


	while (pChunk)
	{
		if (!MEM_CHUNKIsFree(pChunk))
		{
			start_address	=  (gsi_uint)pChunk;
			end_address		= ((gsi_uint)pChunk->next)-1;

			s=  ((start_address - _HeapStart) * (TableSize>>4)) / (_HeapSize>>4);
			MP_ASSERT(s < TableSize)
			MP_ASSERT(s >= 0)

			e=	((  end_address - _HeapStart) * (TableSize>>4)) / (_HeapSize>>4);
			MP_ASSERT(e < TableSize)
			MP_ASSERT(e >= 0)

			for ( j= s; j<= e; j++)
			{
					Table[j] = MEM_CHUNKMemTypeGet(pChunk);
			}

		}
		pChunk = pChunk->next;
	}


}


	
static	MEM_CHUNK_POOL	gChunkPool		[gsMemMgrContext_Count] ;



gsMemMgrContext gsMemMgrContextCurrent = gsMemMgrContext_Default; 


gsMemMgrContext gsMemMgrContextFind	(void *ptr)
{
	int i;
	for (i=0; i< gsMemMgrContext_Count;i++)
	{
		if	(
				MEM_CHUNK_POOLIsValid(&gChunkPool[i])		&&
				MEM_CHUNK_POOLItemIsInPoolMemory(&gChunkPool[i],ptr)
			)
		{
			return (gsMemMgrContext) i;
		}

	}
	return gsMemMgrContext_Invalid;
}

void *gs_malloc(size_t size)
{
	GS_ASSERT(size)
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[gsMemMgrContextCurrent]),"malloc: context is invalid mempool");

	return MEM_CHUNK_POOLmalloc(&gChunkPool[gsMemMgrContextCurrent], size,PTR_ALIGNMENT);
}

void *gs_calloc(size_t size,size_t size2)
{
	GS_ASSERT(size)
	GS_ASSERT(size2)
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[gsMemMgrContextCurrent]),"calloc: context is invalid mempool");

	return MEM_CHUNK_POOLmalloc(&gChunkPool[gsMemMgrContextCurrent], size*size2,PTR_ALIGNMENT);
}

void *gs_realloc(void* ptr,size_t size)
{
	GS_ASSERT(size)
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[gsMemMgrContextCurrent]),"realloc: context is invalid mempool");

	return MEM_CHUNK_POOLrealloc(&gChunkPool[gsMemMgrContextCurrent],ptr, size);
}

void *gs_memalign(size_t boundary,size_t size)
{
	GS_ASSERT(size)
	GS_ASSERT(boundary)
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[gsMemMgrContextCurrent]),"memalign: context is invalid mempool");

	return MEM_CHUNK_POOLmalloc(&gChunkPool[gsMemMgrContextCurrent], size,boundary);
}

void  gs_free(void *ptr)
{	
	gsMemMgrContext context;

	context = gsMemMgrContextFind(ptr);
	GS_ASSERT_STR(context != gsMemMgrContext_Invalid,"Attempt to free invalid ptr")

	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context]),"free: ptr context is invalid mempool");
	MEM_CHUNK_POOLfree(&gChunkPool[context],ptr);
}

const char *MemMgrBufferGetName(gsMemMgrContext context)
{
	GS_ASSERT_STR(context != gsMemMgrContext_Invalid,	"Invalid Context");
	GS_ASSERT_STR(context < gsMemMgrContext_Count,		"Context out of range");
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context ]),"Invalid mempool");

	return gChunkPool[context].Name;
}


void gsMemMgrContextSet(gsMemMgrContext context)
{
	GS_ASSERT_STR(context != gsMemMgrContext_Invalid,	"Invalid Context");
	GS_ASSERT_STR(context < gsMemMgrContext_Count,		"Context out of range");
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context]),"Setting context to invalid mempool");

	gsMemMgrContextCurrent = context; 
}



gsMemMgrContext	gsMemMgrCreate		(gsMemMgrContext context, const char *PoolName,void* thePoolBuffer, size_t thePoolSize)
{
	char *ptr	= (char *)thePoolBuffer;

	GS_ASSERT_STR(thePoolSize,"Cannnot create a pool of size 0")
	GS_ASSERT_STR(thePoolSize,"thePoolBuffer	ptr is inivalid");
	GS_ASSERT_STR(((((gsi_uint)thePoolSize)	&15) ==0)	,"PoolSize	must be aligned to 16 bytes");
	GS_ASSERT_STR(((((gsi_uint)thePoolBuffer)&15) ==0)	,"thePoolBuffer must be aligned to 16 bytes");
	

	while (MEM_CHUNK_POOLIsValid(&gChunkPool[context]))
	{
		context = (gsMemMgrContext)(context + 1);
	}
	if (context == gsMemMgrContext_Count)
	{
		gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
					"Out of memory context handles!\n");
		GS_ASSERT(0);
		return gsMemMgrContext_Invalid;		
	}

	MEM_CHUNK_POOLCreate(&gChunkPool[context],PoolName,ptr,thePoolSize);
	gsiMemoryCallbacksSet(gs_malloc, gs_free, gs_realloc, gs_memalign);
	return context;
}

void gsMemMgrDestroy(gsMemMgrContext context)
{
	GS_ASSERT(gChunkPool[context].HeapSize != 0);
	MEM_CHUNK_POOLDestroy(&gChunkPool[context]);

#if(0)
	{
		gsiMemoryCallbacksSet(malloc,free,realloc,memalign);

		gsiDeleteCriticalSection(&gMemCrit);

	}
#endif
}


void			gsMemMgrTagPush	(gsi_u8 tag)
{
	GS_ASSERT(MemTagStackIndex <	MEM_TAG_STACK_MAX-1)
	MemTagStackIndex++;
	MemTagStack[MemTagStackIndex] = tag;
}
void gsMemMgrTagPop	()
{
	GS_ASSERT(MemTagStackIndex > 0)
	MemTagStackIndex--;
}
gsi_u8			gsMemMgrTagGet	(void *ptr)
{
	GS_ASSERT(ptr);
	return MEM_CHUNKMemTypeGet( Ptr_To_MEM_CHUNK(ptr));
}
gsi_u32			gsMemMgrMemUsedByTagGet(gsi_u8 tag)
{
	int i;
	gsi_u32 used = 0;
	for ( i=0;i< gsMemMgrContext_Count;i++)
	{
		used+= MEM_CHUNK_POOLWalkForType(&gChunkPool[i] ,tag, gsi_false);
	}
	return used;

}

void gsMemMgrContextPush(gsMemMgrContext NewType)
{
	GS_ASSERT(MemTypeStackIndex <	MEM_CONTEXT_STACK_MAX)
	GS_ASSERT(NewType <				gsMemMgrContext_Count)

	MemTypeStack[MemTypeStackIndex++] = gsMemMgrContextCurrent;
	gsMemMgrContextCurrent = NewType;
}

gsMemMgrContext gsMemMgrContextPop()
{
	GS_ASSERT(MemTypeStackIndex > 0)
	gsMemMgrContextCurrent = MemTypeStack[--MemTypeStackIndex];
	return gsMemMgrContextCurrent;
}


gsi_u32			gsMemMgrMemAvailGet			(gsMemMgrContext context)
{
	MEM_STATS stats;
	MEM_STATSClearAll(&stats);
	GS_ASSERT_STR(context <	gsMemMgrContext_Count,				"gsMemMgrMemAvailGet: context out of range");
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context]),	"gsMemMgrMemAvailGet: context is invalid mempool");
	MEM_CHUNK_POOLMemStatsGet	(&gChunkPool[context],	&stats);
	return stats.MemAvail;
}

gsi_u32			gsMemMgrMemUsedGet			(gsMemMgrContext context)
{
	MEM_STATS stats;
	MEM_STATSClearAll(&stats);
	GS_ASSERT_STR(context <	gsMemMgrContext_Count,				"gsMemMgrMemUsedGet: context out of range");
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context]),	"gsMemMgrMemUsedGet: context is invalid mempool");
	MEM_CHUNK_POOLMemStatsGet	(&gChunkPool[context],	&stats);
	return stats.MemUsed;
}


gsi_u32			gsMemMgrMemLargestAvailGet	(gsMemMgrContext context)
{
	MEM_STATS stats;
	MEM_STATSClearAll(&stats);
	GS_ASSERT_STR(context <	gsMemMgrContext_Count,				"gsMemMgrMemLargestAvailGet: context out of range");
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context]),	"gsMemMgrMemLargestAvailGet: context is invalid mempool");
	MEM_CHUNK_POOLMemStatsGet	(&gChunkPool[context],	&stats);
	return stats.ChunksFreeLargestAvail;
}

gsi_u32			gsMemMgrMemHighwaterMarkGet	(gsMemMgrContext context)
{
	GS_ASSERT_STR(context <	gsMemMgrContext_Count,				"gsMemMgrMemLargestAvailGet: context out of range");
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[context]),	"gsMemMgrMemLargestAvailGet: context is invalid mempool");
	
	#if(MEM_PROFILE)
		return gChunkPool[context].HWMemUsed;
	#else
		gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
			"gsMemMgrMemHighwaterMarkGet called without MEM_PROFILE enabled.");
		return 0;
	#endif
}

void gsMemMgrValidateMemoryPool()
{
	GS_ASSERT_STR(MEM_CHUNK_POOLIsValid(&gChunkPool[gsMemMgrContextCurrent]),"memalign: context is invalid mempool");
	MEM_CHUNK_POOLCheckValidity(&gChunkPool[gsMemMgrContextCurrent]);
}


void gsMemMgrDumpStats()
{
#if(0)
	int numUsed = 0;
	int numFree = 0;
	
	struct GSIMemoryBlock* aTempPtr = NULL;

	gsiEnterCriticalSection(&gMemCrit);

	aTempPtr = gMemoryMgr->mFirstFreeBlock;
	while(aTempPtr != NULL)
	{
		numFree++;
		aTempPtr = aTempPtr->mNext;
	}

	aTempPtr = gMemoryMgr->mFirstUsedBlock;
	while(aTempPtr != NULL)
	{
		numUsed++;
		aTempPtr = aTempPtr->mNext;
	}

	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
		"BytesUsed: %d, BlocksUsed: %d, BlocksFree: %d\r\n", 
		gMemoryMgr->mMemUsed, numUsed, numFree);

	gsiLeaveCriticalSection(&gMemCrit);
#endif
}


void gsMemMgrDumpAllocations()
{
#if(0)
	struct GSIMemoryBlock* aBlockPtr = NULL;
	gsi_time aStartTime = 0;
	gsi_i32 aNumAllocations = 0;
	gsi_i32 aNumBytesAllocated = 0;

	gsiEnterCriticalSection(&gMemCrit);

	aStartTime = current_time();
	aBlockPtr = (GSIMemoryBlock*)gMemoryMgr->mPoolStart;

	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
		"Dumping allocations from pool - [0x%08x] %d bytes.\r\n", 
		gMemoryMgr->mPoolStart, gMemoryMgr->mPoolSize);

	while(aBlockPtr != NULL)
	{
		if (gsiMemBlockIsFlagged(aBlockPtr, BlockFlag_Used))
		{
			int anObjectSize = gsiMemBlockGetObjectSize(aBlockPtr);
			aNumAllocations++;
			aNumBytesAllocated += anObjectSize;

			if (aBlockPtr == gMemoryMgr->mPoolStart)
			{
				gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
					"\t[0x%08x] Size: %d (memmgr instance)\r\n", (gsi_u32)aBlockPtr, anObjectSize);
			}
			else
			{
				gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
					"\t[0x%08x] Size: %d\r\n", (gsi_u32)(gsiMemBlockGetObjectPtr(aBlockPtr)), anObjectSize);
			}
		}
		else
		{
		}
		aBlockPtr = gsiMemBlockGetLinearNext(aBlockPtr);
	}

	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
		"\t--%d allocations, %d bytes allocated.\r\n", aNumAllocations, aNumBytesAllocated);
	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
		"\t--%d peak memory usage\r\n", gMemoryMgr->mPeakMemoryUsage);

	gsDebugFormat(GSIDebugCat_App, GSIDebugType_Memory, GSIDebugLevel_Comment,
		"Memory dump complete. (%d ms)\r\n", current_time() - aStartTime);

	gsiLeaveCriticalSection(&gMemCrit);

	GSI_UNUSED(aStartTime); 
#endif
}



#if (1)	

#define PTR_TABLE_SIZE		2048
static int	 PtrTableCount = 0;
static void	*PtrTable[2048];

int Random(int x)
{
	return Util_RandInt(0,x);
}
void gsMemMgrSelfText()
{

	
	static MEM_CHUNK_POOL gChunkPool;
	int size	= 32 * 1024 * 1024;
	int c= 0;
	int i,j,k;

	char *ptr	= (char *) ( ((gsi_uint)malloc(size-PTR_ALIGNMENT)+(PTR_ALIGNMENT-1))&~ (PTR_ALIGNMENT-1) )  ;
	MEM_CHUNK_POOLCreate(&gChunkPool,"",ptr,size);

	while(1)
	{

		i= Random(4);
		if ((i==0) &&(PtrTableCount < 1024))
		{
			j = Random(1024)+1;
			k = 32<< (Random(4));

			if (c&1)
				PtrTable[PtrTableCount] = MEM_CHUNK_POOLmalloc(&gChunkPool, j,k);
			else
				PtrTable[PtrTableCount] = MEM_CHUNK_POOLmalloc_backwards(&gChunkPool, j,k);

			if(PtrTable[PtrTableCount])
			{
				PtrTableCount++;
			}
			else
			{
				GS_ASSERT(0);
			}

		}
		else
		if ((i==1) &&(PtrTableCount))
		{
			j = Random(PtrTableCount);
			MP_ASSERT(j < PtrTableCount)


			MEM_CHUNK_POOLfree(&gChunkPool,PtrTable[j]);

			PtrTableCount--;
			PtrTable[j] = PtrTable[PtrTableCount];

		}
		else
		if ((i==2) &&(PtrTableCount))
		{
			j = Random(PtrTableCount);
			MP_ASSERT(j < PtrTableCount)

			k = Random(1024) +1;
			#if(1)
				PtrTable[j] = MEM_CHUNK_POOLrealloc(&gChunkPool,PtrTable[j], k);
			#else
				PtrTable[j] = PtrTable[j];
			#endif

			if(PtrTable[j])
			{
			}
			else
			{
				GS_ASSERT(0);
			}

		}
		else
			continue;	

		c++;
		MEM_CHUNK_POOLCheckValidity(&gChunkPool);
	}

}


#endif







#endif 

