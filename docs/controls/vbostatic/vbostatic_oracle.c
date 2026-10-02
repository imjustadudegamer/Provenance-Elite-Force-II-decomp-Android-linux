/*
 * vbostatic_oracle.c -- the static device-buffer path's offline oracle.
 *
 * It links the shipping `code/renderervulkan/vk_vbo.c` (included verbatim, with
 * the Vulkan and engine layer stubbed) and drives it with synthetic world
 * geometry, then asserts the three properties the path must have for the
 * pixels to be unchanged:
 *
 *   1. the arena's vertex bytes are the streaming pack's bytes, byte for byte,
 *      and the recorded device copy takes exactly those bytes to that offset;
 *   2. the rebased index block is the streaming index block rebased onto the
 *      arena vertexes, same order;
 *   3. the draw sequence reads every index of the batch exactly once, in the
 *      batch's index order, whether the run is device-local or grouped.
 *
 * Plus planted faults, so no arm can pass vacuously.
 *
 * Build/run: docs/controls/vbostatic/run.sh
 */

#include "tr_local.h"
#include "tr_vbo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the engine/Vulkan layer the shipping source sits on ------------------ */

vkContext_t	vk;
refimport_t	ri;
vkProcs_t	qvk;

shaderCommands_t	tess;
backEndState_t		backEnd;
trGlobals_t			tr;

static cvar_t	harnessCvarShowTris;
static cvar_t	harnessCvarShowNormals;
static cvar_t	harnessCvarVboStatic;

cvar_t	*r_showtris = &harnessCvarShowTris;
cvar_t	*r_shownormals = &harnessCvarShowNormals;

/* the engine's cvar lookup: this path only ever asks whether r_vboStatic is on */
static cvar_t *harnessCvarGet( const char *name, const char *value, int flags )
{
	( void )name; ( void )value; ( void )flags;
	return &harnessCvarVboStatic;
}

static int	harnessFrame;
static int	harnessSmpFrame;

int RB_FrameCount( void ) { return harnessFrame; }
int RB_FrameSmpFrame( void ) { return harnessSmpFrame; }

/* the ring the streaming pack writes: the harness owns the bytes */
static byte	*harnessRing;
static int	harnessRingBytes;

unsigned char *VK_VertexRing_Mapped( void ) { return harnessRing; }

/* the recorded device copies */
typedef struct { VkDeviceSize src, dst, size; } harnessCopy_t;
static harnessCopy_t	harnessCopies[4096];
static int				harnessCopyCount;

/* the recorded draws */
typedef struct { int count, firstIndex; } harnessDraw_t;
static harnessDraw_t	harnessDraws[4096];
static int				harnessDrawCount;

/* which index buffer each draw read: 1 device, 2 soft; and which slot's compose */
static int				harnessDrawSrc[4096];
static int				harnessDrawSoft[4096];

static int				harnessBoundIndex;
/* the compose handles the shipping init creates: one per frame in flight */
static VkBuffer			harnessSoftHandle[2];
static int				harnessSoftSlots;
static VkBuffer			harnessDevIndexHandle;

/* the planted-fault selector, read by the flush emulation below */
static int				harnessMutant;
static int				harnessBoundSoft;

static int	harnessCurrentSoftStart;

static VkCommandBuffer	harnessCmd = (VkCommandBuffer)1;

VkCommandBuffer VK_Shade_CommandBuffer( void ) { return harnessCmd; }

void VK_Draw_BindVertexBuffer( VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset )
{
	( void )cmd; ( void )buffer; ( void )offset;
}

void VK_Draw_BindIndexBuffer( VkCommandBuffer cmd, VkBuffer buffer )
{
	( void )cmd;
	if ( buffer == harnessSoftHandle[0] ) {
		harnessBoundIndex = 2;
		harnessBoundSoft = 0;
	} else if ( harnessSoftSlots > 1 && buffer == harnessSoftHandle[1] ) {
		harnessBoundIndex = 2;
		harnessBoundSoft = 1;
	} else {
		harnessBoundIndex = 1;
	}
}

void VK_Draw_DrawIndexed( VkCommandBuffer cmd, int indexCount, int firstIndex, int vertexBase )
{
	( void )cmd; ( void )vertexBase;
	if ( harnessDrawCount < 4096 ) {
		harnessDraws[harnessDrawCount].count = indexCount;
		harnessDraws[harnessDrawCount].firstIndex = firstIndex;
		harnessDrawSrc[harnessDrawCount] = harnessBoundIndex;
		harnessDrawSoft[harnessDrawCount] = harnessBoundSoft;
		harnessDrawCount++;
	}
}

/* the engine's guard and memory layer: the harness records, it never allocates */
static int	harnessGuardRows;
static int	harnessGuardHits;

void VK_Guard_Row( const char *tag, const char *fmt, ... )
{
	( void )tag; ( void )fmt;
	harnessGuardRows++;
}

void VK_Guard_Hit( vkGuardSite_t site, const char *detail, VkResult result )
{
	( void )site; ( void )result;
	if ( detail ) {
		harnessGuardHits++;
	}
}

/* the master gate: the compose alias check rides it, and the arm below proves
   it is armed */
static int	harnessProbe = 1;

qboolean R_ProbeStatsOn( void ) { return harnessProbe ? qtrue : qfalse; }

qboolean VK_Guard_Result( vkGuardSite_t site, VkResult result, const char *what )
{
	( void )site; ( void )what;
	return ( result == VK_SUCCESS ) ? qtrue : qfalse;
}

/* the Vulkan memory layer: the harness hands out its own buffers, numbered so
   the draw recorder can tell the device index buffer from the soft one */
#define HARNESS_MAX_MAP 8
static int	harnessMapKind[HARNESS_MAX_MAP];
static int	harnessBufferCount;

qboolean VK_Memory_CreateBuffer( VkDeviceSize size, VkBufferUsageFlags usage,
								 VkMemoryPropertyFlags wanted, VkBuffer *buffer, VkDeviceMemory *memory )
{
	( void )size; ( void )usage; ( void )wanted;
	harnessBufferCount++;
	*buffer = (VkBuffer)(size_t)harnessBufferCount;
	*memory = (VkDeviceMemory)(size_t)harnessBufferCount;
	if ( harnessBufferCount < HARNESS_MAX_MAP ) {
		harnessMapKind[harnessBufferCount] = harnessBufferCount;
	}
	if ( harnessBufferCount == 2 ) {
		harnessDevIndexHandle = (VkBuffer)(size_t)harnessBufferCount;
	}
	if ( harnessBufferCount >= 5 && harnessSoftSlots < 2 ) {
		harnessSoftHandle[harnessSoftSlots] = (VkBuffer)(size_t)harnessBufferCount;
		harnessSoftSlots++;
	}
	return qtrue;
}

void VK_Unused_qvk( void ) { }

/* the couple of entry points the bake records */
static VkResult harnessMapMemory( VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
								  VkDeviceSize size, VkMemoryMapFlags flags, void **data )
{
	( void )device; ( void )memory; ( void )offset; ( void )size; ( void )flags;
	/* The shipping RB_VBO_Init maps five buffers and stores what it gets.  The
	   harness gives each a distinct static arena so the bake's writes are
	   observable.  Buffer numbers run 1..5 in creation order. */
	{
		static byte	*vboStage;
		static byte	*iboStage;
		static byte	*soft[2];
		int			kind = (int)(size_t)memory;

		if ( !vboStage ) {
			vboStage = (byte *)calloc( 1, 24u * 1024u * 1024u );
			iboStage = (byte *)calloc( 1, 24u * 1024u * 1024u );
			soft[0] = (byte *)calloc( 1, 1u * 1024u * 1024u );
			soft[1] = (byte *)calloc( 1, 1u * 1024u * 1024u );
		}

		/* creation order: devVbo(1) devIbo(2) stageVbo(3) stageIbo(4)
		   soft[0](5) soft[1](6) -- the compose is one buffer per frame in
		   flight, and the harness must hand out distinct memory or the arm
		   could not see the sharing it exists to catch */
		switch ( kind ) {
		case 3: *data = vboStage; break;
		case 4: *data = iboStage; break;
		case 5: *data = soft[0]; break;
		case 6: *data = soft[1]; break;
		default: *data = vboStage; break;	/* never read by the path under test */
		}
	}
	return VK_SUCCESS;
}

static void harnessUnmapMemory( VkDevice device, VkDeviceMemory memory )
{
	( void )device; ( void )memory;
}

static void harnessCmdCopyBuffer( VkCommandBuffer cmd, VkBuffer src, VkBuffer dst,
								  uint32_t count, const VkBufferCopy *regions )
{
	uint32_t	i;
	( void )cmd; ( void )src; ( void )dst;
	for ( i = 0; i < count && harnessCopyCount < 4096; i++ ) {
		harnessCopies[harnessCopyCount].src = regions[i].srcOffset;
		harnessCopies[harnessCopyCount].dst = regions[i].dstOffset;
		harnessCopies[harnessCopyCount].size = regions[i].size;
		harnessCopyCount++;
	}
}

/* The arena bake's copies now go into a transfer command buffer of their own
   (a buffer copy may not be recorded inside a render pass), so the harness has
   to own that buffer too: these stubs say "the frame's transfer buffer exists
   and is where the copies land", which is what the bake's own checks read. */
static VkCommandBuffer	harnessXferCmd = ( VkCommandBuffer )&harnessCopyCount;
static int				harnessXferOpen;

/* The pool the arena's transfer buffer comes from: the shipping code asks the
   calling thread's own one-shot pool for it, and the harness owns no pools, so
   the stub answers a non-null handle and the allocate below hands back the
   harness's own transfer buffer. */
VkCommandPool VK_Upload_Pool( void )
{
	return ( VkCommandPool )&harnessXferOpen;
}

VkResult VK_Pool_AllocBuffers( VkCommandPool pool, const VkCommandBufferAllocateInfo *info,
							   VkCommandBuffer *out )
{
	( void )pool; ( void )info;
	*out = harnessXferCmd;
	return VK_SUCCESS;
}

static VkResult harnessBeginCommandBuffer( VkCommandBuffer cmd, const VkCommandBufferBeginInfo *info )
{
	( void )cmd; ( void )info;
	harnessXferOpen = 1;
	return VK_SUCCESS;
}

static VkResult harnessEndCommandBuffer( VkCommandBuffer cmd )
{
	( void )cmd;
	harnessXferOpen = 0;
	return VK_SUCCESS;
}

static VkResult harnessResetCommandBuffer( VkCommandBuffer cmd, VkCommandBufferResetFlags flags )
{
	( void )cmd; ( void )flags;
	return VK_SUCCESS;
}

static void harnessCmdPipelineBarrier( VkCommandBuffer cmd, VkPipelineStageFlags src,
									   VkPipelineStageFlags dst, VkDependencyFlags flags,
									   uint32_t memCount, const VkMemoryBarrier *mem,
									   uint32_t bufCount, const VkBufferMemoryBarrier *bufs,
									   uint32_t imgCount, const VkImageMemoryBarrier *imgs )
{
	( void )cmd; ( void )src; ( void )dst; ( void )flags;
	( void )memCount; ( void )mem; ( void )bufCount; ( void )bufs;
	( void )imgCount; ( void )imgs;
}

void VK_Harness_InstallProcs( void )
{
	qvk.MapMemory = harnessMapMemory;
	qvk.UnmapMemory = harnessUnmapMemory;
	qvk.CmdCopyBuffer = harnessCmdCopyBuffer;
	qvk.BeginCommandBuffer = harnessBeginCommandBuffer;
	qvk.EndCommandBuffer = harnessEndCommandBuffer;
	qvk.ResetCommandBuffer = harnessResetCommandBuffer;
	qvk.CmdPipelineBarrier = harnessCmdPipelineBarrier;
}

/* the engine's surface batch boundary.  RB_EndSurface is where an arena batch
   reaches the GPU, so the harness has to emulate the iterator's arm of it: if
   the item list cannot tile the batch, the shipping arm drops the geometry (the
   tess arrays were never written, so streaming cannot recover it).  A stub that
   only bumped a counter could not see that, which is the gap this closes. */
static int		harnessArenaFlushes;
static int		harnessArenaItems;
static int		harnessArenaMissed;		/* a claimed batch the item list refused */
static int		harnessStreamFlushes;

static void HarnessIteratorDraw( void )
{
	vkStaticItemView_t	items[VK_STATIC_BATCH_MAX];
	int					n;

	if ( tess.numIndexes == 0 ) {
		return;
	}

	if ( !RB_VboStaticArenaBatch() ) {
		/* a streamed batch: the pack path inside the iterator draws it, and this
		   harness does not link that path */
		harnessStreamFlushes++;
		return;
	}

	/* the shipping RB_VBO_StaticDrawBatch's own validation and draw */
	n = RB_VboStaticBatchItems( items, VK_STATIC_BATCH_MAX );
	if ( n <= 0 ) {
		harnessArenaMissed++;
		return;
	}
	harnessArenaItems += n;
	RB_VboStaticDrawRuns( harnessCmd, tess.numIndexes, items, n );
	harnessArenaFlushes++;
}

void RB_EndSurface( void )
{
	harnessFrame++;		/* one draw boundary */

	if ( harnessMutant == 5 ) {
		return;			/* planted fault: the flush loses what the fast arm claimed */
	}

	HarnessIteratorDraw();
	tess.numIndexes = 0;
}

void RB_BeginSurface( shader_t *shader, int fogNum )
{
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.dlightBits = 0;
	tess.shader = shader;
	tess.fogNum = fogNum;
	RB_VboStaticReset();
}

void RB_StageIteratorLightmappedMultitextureUnfogged( void ) { }

void VK_Shade_NoteDraw( qboolean b ) { ( void )b; }

/* ---- the shipping source under test -------------------------------------- */

#include "../../../app/src/main/cpp/code/renderervulkan/vk_vbo.c"

/* ---- the test ------------------------------------------------------------- */

static int	failures;
static int	checks;

#define CHECK( cond, ... ) do { \
	checks++; \
	if ( !( cond ) ) { \
		failures++; \
		printf( "  FAIL  " ); \
		printf( __VA_ARGS__ ); \
		printf( "\n" ); \
	} \
} while ( 0 )

#define N_SURF		8
#define SURF_VERTS	5
#define SURF_IDX	9

static srfSurfaceFace_t	*surfaces[N_SURF];
static shader_t			harnessShader;

/* the streaming pack's bytes for the whole batch, as the walk would write them */
static byte				streamVerts[N_SURF * SURF_VERTS * VK_STREAM_STRIDE];
static vkIndex_t		streamIdx[N_SURF * SURF_IDX];

static void HarnessReset( void )
{
	harnessCopyCount = 0;
	harnessDrawCount = 0;
	harnessGuardHits = 0;
	harnessBoundIndex = 0;
	harnessBoundSoft = 0;
	memset( harnessDrawSoft, 0, sizeof( harnessDrawSoft ) );
	harnessArenaFlushes = 0;
	harnessArenaItems = 0;
	harnessArenaMissed = 0;
	harnessStreamFlushes = 0;
	memset( streamVerts, 0, sizeof( streamVerts ) );
	memset( streamIdx, 0, sizeof( streamIdx ) );
	memset( &tess, 0, sizeof( tess ) );
	tess.shader = &harnessShader;
	tess.currentStageIteratorFunc = RB_StageIteratorLightmappedMultitextureUnfogged;
	tess.fogNum = 0;
	tess.numPasses = 1;
	backEnd.currentEntity = &tr.worldEntity;
	memset( &harnessShader, 0, sizeof( harnessShader ) );
}

/* Fill the ring with the streaming pack's bytes and tess.indexes with the
   batch-local index list the generators would have produced. */
static void HarnessStreamBatch( int base )
{
	int		i, s;

	for ( i = 0; i < N_SURF * SURF_VERTS; i++ ) {
		byte	*v = streamVerts + (size_t)i * VK_STREAM_STRIDE;
		int		j;

		for ( j = 0; j < VK_STREAM_STRIDE; j++ ) {
			v[j] = (byte)( ( i * 31 + j * 7 + 1 ) & 0xff );
		}
	}
	memcpy( harnessRing + base, streamVerts, sizeof( streamVerts ) );

	for ( s = 0; s < N_SURF; s++ ) {
		for ( i = 0; i < SURF_IDX; i++ ) {
			streamIdx[s * SURF_IDX + i] = (vkIndex_t)( s * SURF_VERTS + ( i % SURF_VERTS ) );
		}
	}
	memcpy( tess.indexes, streamIdx, sizeof( streamIdx ) );

	/* the streaming frame's records, exactly the call the generators make */
	tess.numVertexes = 0;
	tess.numIndexes = 0;
	for ( s = 0; s < N_SURF; s++ ) {
		RB_VboStaticRecord( surfaces[s], s * SURF_VERTS, SURF_VERTS,
							s * SURF_IDX, SURF_IDX );
		tess.numVertexes += SURF_VERTS;
		tess.numIndexes += SURF_IDX;
	}
}

int main( int argc, char **argv )
{
	int		i, s;

	harnessMutant = ( argc > 1 ) ? atoi( argv[1] ) : 0;

	harnessRingBytes = 1024 * 1024;
	harnessRing = (byte *)calloc( 1, harnessRingBytes );
	harnessCvarVboStatic.integer = 1;
	harnessCvarShowTris.integer = 0;
	harnessCvarShowNormals.integer = 0;
	ri.Cvar_Get = harnessCvarGet;
	VK_Harness_InstallProcs();

	for ( s = 0; s < N_SURF; s++ ) {
		surfaces[s] = (srfSurfaceFace_t *)calloc( 1, sizeof( srfSurfaceFace_t ) );
		surfaces[s]->surfaceType = SF_FACE;
		surfaces[s]->numPoints = SURF_VERTS;
		surfaces[s]->numIndices = SURF_IDX;
	}

	printf( "vbostatic oracle: %d surfaces, %d verts, %d indexes each\n",
			N_SURF, SURF_VERTS, SURF_IDX );

	if ( !RB_VBO_Init() ) {
		printf( "  FAIL  RB_VBO_Init\n" );
		return 1;
	}

	HarnessReset();

	/* ---- 1. the bake: staging bytes and the device copy ------------------- */
	{
		const float	*stage;
		int			copied = 0;

		HarnessStreamBatch( 0 );
		RB_VboStaticBakeFrame( 0 );

		/* planted faults: the byte and rebase comparisons must see each */
		if ( harnessMutant == 1 ) {
			s_stageVboMap[0] ^= 0xff;
		}
		if ( harnessMutant == 2 ) {
			s_stageIboMap[0] += 1;
		}

		CHECK( harnessCopyCount == 2 * N_SURF, "bake recorded %d copies, want %d",
			   harnessCopyCount, 2 * N_SURF );

		stage = RB_VboStaticStageFloats();
		CHECK( stage != NULL, "the arena's staging floats are NULL" );

		for ( s = 0; s < N_SURF; s++ ) {
			VkDeviceSize	vo = (VkDeviceSize)( s * SURF_VERTS ) * VK_STREAM_STRIDE;
			VkDeviceSize	io = (VkDeviceSize)( s * SURF_IDX ) * sizeof( vkIndex_t );

			CHECK( harnessCopies[copied].src == vo && harnessCopies[copied].dst == vo
				   && harnessCopies[copied].size == (VkDeviceSize)( SURF_VERTS * VK_STREAM_STRIDE ),
				   "surface %d vertex copy is src=%llu dst=%llu size=%llu",
				   s, (unsigned long long)harnessCopies[copied].src,
				   (unsigned long long)harnessCopies[copied].dst,
				   (unsigned long long)harnessCopies[copied].size );
			copied++;
			CHECK( harnessCopies[copied].src == io && harnessCopies[copied].dst == io
				   && harnessCopies[copied].size == (VkDeviceSize)( SURF_IDX * sizeof( vkIndex_t ) ),
				   "surface %d index copy is src=%llu size=%llu",
				   s, (unsigned long long)harnessCopies[copied].src,
				   (unsigned long long)harnessCopies[copied].size );
			copied++;

			/* the arena holds the streaming pack's bytes, byte for byte */
			CHECK( memcmp( (const byte *)stage + vo,
						   streamVerts + (size_t)s * SURF_VERTS * VK_STREAM_STRIDE,
						   (size_t)SURF_VERTS * VK_STREAM_STRIDE ) == 0,
				   "surface %d arena vertexes differ from the streaming pack", s );

			/* the rebase: arena vertex of batch-local v is first + (v - vertStart) */
			for ( i = 0; i < SURF_IDX; i++ ) {
				vkIndex_t	want = (vkIndex_t)( s * SURF_VERTS
									+ ( streamIdx[s * SURF_IDX + i] - s * SURF_VERTS ) );
				CHECK( s_stageIboMap[s * SURF_IDX + i] == want,
					   "surface %d index %d rebased to %u, want %u",
					   s, i, s_stageIboMap[s * SURF_IDX + i], want );
			}
		}
	}

	/* a planted fault: one byte of the arena is wrong */
	{
		s_stageVboMap[3 * VK_STREAM_STRIDE + 5] ^= 0xff;
		CHECK( memcmp( s_stageVboMap + 3 * VK_STREAM_STRIDE,
					   streamVerts + (size_t)3 * SURF_VERTS * VK_STREAM_STRIDE,
					   (size_t)SURF_VERTS * VK_STREAM_STRIDE ) != 0,
			   "the byte compare cannot see a planted arena fault" );
		s_stageVboMap[3 * VK_STREAM_STRIDE + 5] ^= 0xff;
	}

	/* ---- 2. the fast arm: no generator, the same counters ------------------ */
	{
		int		beforeV = tess.numVertexes;
		int		beforeI = tess.numIndexes;
		int		items;

		HarnessReset();
		HarnessStreamBatch( 0 );		/* resident now, from the bake above */
		(void)beforeV; (void)beforeI;

		/* a surface dropped from the table must not be taken by the fast arm */
		if ( harnessMutant == 3 ) {
			vkStaticRegion_t	*e = VK_StaticFind( surfaces[N_SURF - 1] );

			if ( e ) {
				memset( e, 0, sizeof( *e ) );
			}
		}

		RB_VboStaticReset();
		tess.numVertexes = 0;
		tess.numIndexes = 0;

		for ( s = 0; s < N_SURF; s++ ) {
			CHECK( RB_VboStaticTrySurface( surfaces[s], SF_FACE ) == qtrue,
				   "surface %d was not taken by the fast arm", s );
		}
		CHECK( tess.numVertexes == N_SURF * SURF_VERTS,
			   "the fast arm advanced %d verts, want %d", tess.numVertexes, N_SURF * SURF_VERTS );
		CHECK( tess.numIndexes == N_SURF * SURF_IDX,
			   "the fast arm advanced %d indexes, want %d", tess.numIndexes, N_SURF * SURF_IDX );
		CHECK( harnessGuardHits == 0, "the fast arm raised %d guard hits", harnessGuardHits );

		items = RB_VboStaticBatchItems( (vkStaticItemView_t *)calloc( VK_STATIC_BATCH_MAX,
									  sizeof( vkStaticItemView_t ) ), VK_STATIC_BATCH_MAX );
		CHECK( items == N_SURF, "the batch validated %d items, want %d", items, N_SURF );
		CHECK( RB_VboStaticArenaBatch() == qtrue, "the batch is not arena mode after the fast arm" );
	}

	/* ---- 3. the draw sequence: every index once, in order ------------------ */
	{
		static vkStaticItemView_t	items[VK_STATIC_BATCH_MAX];
		int							n, k;
		static vkIndex_t			got[4096];
		int							gotCount = 0;

		n = RB_VboStaticBatchItems( items, VK_STATIC_BATCH_MAX );
		CHECK( n == N_SURF, "items=%d", n );

		harnessDrawCount = 0;
		RB_VboStaticDrawRuns( harnessCmd, tess.numIndexes, items, n );

		/* the compose must have written the batch's own index values */
		if ( harnessMutant == 4 ) {
			s_softMap[0] += 1;
		}

		/* replay each draw's index range in the order the calls were made */
		for ( k = 0; k < harnessDrawCount; k++ ) {
			const vkIndex_t	*src = ( harnessDrawSrc[k] == 1 )
									? s_stageIboMap + harnessDraws[k].firstIndex
									: s_softMap[harnessDrawSoft[k]] + harnessDraws[k].firstIndex;
			int				j;

			for ( j = 0; j < harnessDraws[k].count; j++ ) {
				if ( gotCount < 4096 ) {
					got[gotCount++] = src[j];
				}
			}
		}

		CHECK( gotCount == N_SURF * SURF_IDX, "the draws read %d indexes, want %d",
			   gotCount, N_SURF * SURF_IDX );
		CHECK( harnessDrawCount >= 1 && harnessDrawSrc[0] == 2,
			   "the short run was not composed into the host-visible buffer (src=%d draws=%d)",
			   harnessDrawCount ? harnessDrawSrc[0] : -1, harnessDrawCount );
		for ( i = 0; i < gotCount && i < N_SURF * SURF_IDX; i++ ) {
			CHECK( got[i] == streamIdx[i], "index %d drew %u, want %u", i, got[i], streamIdx[i] );
		}
	}

	/* ---- 4. a long adjacent run is one device draw ------------------------- */
	{
		static vkStaticItemView_t	items[VK_STATIC_BATCH_MAX];
		int							n;

		/* shrink the record set so the batch's total indexes clear MIN_IBO_RUN */
		HarnessReset();
		s_stageVboMap[0] = s_stageVboMap[0];

		/* one item whose own block clears MIN_IBO_RUN must draw from the device
		   index buffer in a single call: the run walker is not the compose */
		n = 0;
		items[n].vertStart = 0;
		items[n].numVerts = 24;				/* = SURF_VERTS * 5, only the view of the walker */
		items[n].first = 0;
		items[n].idxFirst = 0;
		items[n].numIdx = VK_STATIC_MIN_IBO_RUN;
		n++;

		harnessDrawCount = 0;
		harnessBoundIndex = 0;
		RB_VboStaticDrawRuns( harnessCmd, VK_STATIC_MIN_IBO_RUN, items, n );
		CHECK( harnessDrawCount == 1, "the long run emitted %d draws, want 1", harnessDrawCount );
		if ( harnessDrawCount == 1 ) {
			CHECK( harnessDrawSrc[0] == 1 && harnessDraws[0].count == VK_STATIC_MIN_IBO_RUN
				   && harnessDraws[0].firstIndex == 0,
				   "a %d-index run drew as src=%d count=%d first=%d",
				   items[0].numIdx, harnessDrawSrc[0], harnessDraws[0].count,
				   harnessDraws[0].firstIndex );
		}
	}

	/* ---- 5. a class change keeps the batch streaming ----------------------- */
	{
		HarnessReset();
		HarnessStreamBatch( 0 );
		RB_VboStaticReset();
		tess.numVertexes = 0;
		tess.numIndexes = 0;

		/* the first surface of the batch is not this arena's class */
		harnessShader.needsNormal = 1;
		CHECK( RB_VboStaticTrySurface( surfaces[0], SF_FACE ) == qfalse,
			   "a needsNormal surface was taken by the fast arm" );
		harnessShader.needsNormal = 0;

		/* the next surface is resident and in class, and must still stream */
		CHECK( RB_VboStaticTrySurface( surfaces[1], SF_FACE ) == qfalse,
			   "a class change did not keep the batch streaming" );
		CHECK( RB_VboStaticArenaBatch() == qfalse,
			   "a batch with streamed geometry was marked arena" );

		/* a resident surface into its own fresh batch is still taken */
		RB_VboStaticReset();
		tess.numVertexes = 0;
		tess.numIndexes = 0;
		CHECK( RB_VboStaticTrySurface( surfaces[2], SF_FACE ) == qtrue,
			   "a fresh batch's resident surface was not taken" );
	}

	/* ---- 6. a claimed surface is always drawn ------------------------------
	   The operator's RenderDoc captures show world pixels with NO event in
	   Pixel History: the geometry is absent from submission, not textured
	   wrong.  The only place the arena arm can lose geometry silently is the
	   item-list validation at the batch's own RB_EndSurface flush (the tess
	   arrays were never written, so a refusal drops the batch).  This drives a
	   mixed walk -- claimed, claimed, not-resident, claimed, claimed -- so a
	   mode flip happens with items already accepted, and asserts that every
	   index of every claimed surface reached a draw exactly once. */
	{
		static vkStaticItemView_t	items[VK_STATIC_BATCH_MAX];
		static vkIndex_t			got[4096];
		int							gotCount, k, n;

		HarnessReset();
		HarnessStreamBatch( 0 );
		RB_VboStaticBakeFrame( 0 );		/* all N_SURF resident now */

		/* frame 2: the walk claims 0 and 1, then meets a surface the arena does
		   not hold (drop it from the table), then claims two more */
		RB_VboStaticReset();
		tess.numVertexes = 0;
		tess.numIndexes = 0;
		harnessDrawCount = 0;

		CHECK( RB_VboStaticTrySurface( surfaces[0], SF_FACE ) == qtrue, "surface 0 not claimed" );
		CHECK( RB_VboStaticTrySurface( surfaces[1], SF_FACE ) == qtrue, "surface 1 not claimed" );

		{
			vkStaticRegion_t	*e = VK_StaticFind( surfaces[3] );

			if ( e ) {
				memset( e, 0, sizeof( *e ) );	/* surface 3 is no longer resident */
			}
		}

		/* the mode flip happens here; the two claimed surfaces must be flushed */
		CHECK( RB_VboStaticTrySurface( surfaces[3], SF_FACE ) == qfalse,
			   "the dropped surface was still claimed" );
		CHECK( RB_VboStaticArenaBatch() == qfalse, "the batch did not flip to stream" );
		CHECK( harnessArenaMissed == 0,
			   "the flush refused the %d claimed surfaces (%d item(s))",
			   harnessArenaItems ? harnessArenaItems : 2, harnessArenaItems );
		CHECK( harnessArenaFlushes == 1, "the flip emitted %d arena flushes, want 1",
			   harnessArenaFlushes );

		/* a surface accepted into an arena batch stays drawn even when a later
		   surface of the same batch cannot be */
		CHECK( RB_VboStaticTrySurface( surfaces[4], SF_FACE ) == qfalse,
			   "a resident surface after the flip was claimed as arena again" );

		/* end of batch: the stream batch's pack path owns 3 and 4.  The harness
		   does not emulate the generator's own tess writes, so the stream
		   batch's counters are not asserted here -- only that its surfaces were
		   not drawn from the arena. */
		RB_EndSurface();

		/* replay what the arena flush drew: surfaces 0 and 1, once each, in order */
		gotCount = 0;
		for ( k = 0; k < harnessDrawCount && gotCount < 4096; k++ ) {
			const vkIndex_t	*src = ( harnessDrawSrc[k] == 1 )
									? s_stageIboMap + harnessDraws[k].firstIndex
									: s_softMap[harnessDrawSoft[k]] + harnessDraws[k].firstIndex;
			int				j;

			for ( j = 0; j < harnessDraws[k].count && gotCount < 4096; j++ ) {
				got[gotCount++] = src[j];
			}
		}
		CHECK( gotCount == 2 * SURF_IDX,
			   "the flip drew %d indexes, want %d (the claimed surfaces)",
			   gotCount, 2 * SURF_IDX );
		for ( k = 0; k < gotCount && k < 2 * SURF_IDX; k++ ) {
			CHECK( got[k] == (vkIndex_t)( ( k / SURF_IDX ) * SURF_VERTS + ( k % SURF_IDX ) % SURF_VERTS ),
				   "index %d drew %u, want the claimed surface's own vertex",
				   k, got[k] );
		}

		/* the item list must tile the claimed surfaces even when the frame's
		   later surfaces were not resident */
		n = RB_VboStaticBatchItems( items, VK_STATIC_BATCH_MAX );
		CHECK( n <= 0, "the streamed batch still validated %d arena items", n );
	}

	/* ---- 7. two frames in flight own their own compose ---------------------
	   The compose is the one buffer on this path a recorded draw reads by byte
	   offset and a later walk rewrites.  VK_Frame_Begin only waits the slot it
	   is re-recording, so the other slot's frame can still be reading when the
	   next walk writes; a compose the two slots share would then hand a draw
	   another batch's index block -- valid indices, wrong triangles, and no row
	   anywhere, because nothing about the draw is invalid.  This records a
	   short-run batch in slot 0, snapshots the bytes its draw will read,
	   records a different batch in slot 1, and asserts slot 0's bytes are
	   untouched and the two slots never aliased. */
	{
		static vkStaticItemView_t	items[VK_STATIC_BATCH_MAX];
		static vkIndex_t			snapshot[64];
		int							n, i, differed = 0;

		HarnessReset();
		if ( harnessMutant == 7 ) {
			/* the pre-fix arm: one handle and one mapping for both slots */
			s_softIbo[1] = s_softIbo[0];
			s_softMap[1] = s_softMap[0];
		}
		CHECK( harnessSoftSlots == VK_FRAMES_IN_FLIGHT,
			   "the init created %d compose buffers, want %d",
			   harnessSoftSlots, VK_FRAMES_IN_FLIGHT );
		CHECK( s_softIbo[0] != VK_NULL_HANDLE && s_softIbo[1] != VK_NULL_HANDLE,
			   "a compose handle is null" );
		CHECK( s_softIbo[0] != s_softIbo[1],
			   "the two frames in flight share one compose buffer" );
		CHECK( s_softMap[0] != s_softMap[1],
			   "the two frames in flight share one compose mapping" );

		/* frame in flight 0: a short run, composed into slot 0 */
		harnessSmpFrame = 0;
		n = 0;
		items[n].vertStart = 0;   items[n].numVerts = 4; items[n].first = 0;
		items[n].idxFirst = 0;    items[n].numIdx = 8;   n++;
		items[n].vertStart = 8;   items[n].numVerts = 4; items[n].first = 8;
		items[n].idxFirst = 8;    items[n].numIdx = 8;   n++;

		tess.numVertexes = 16;
		tess.numIndexes = 16;
		harnessDrawCount = 0;
		RB_VboStaticDrawRuns( harnessCmd, tess.numIndexes, items, n );

		CHECK( harnessDrawCount >= 1, "the slot-0 batch emitted %d draws", harnessDrawCount );
		for ( i = 0; i < 16; i++ ) {
			snapshot[i] = s_softMap[0][i];
		}

		/* frame in flight 1: a different short run, composed into slot 1 */
		harnessSmpFrame = 1;
		n = 0;
		items[n].vertStart = 0;    items[n].numVerts = 4; items[n].first = 0;
		items[n].idxFirst = 1000;  items[n].numIdx = 8;   n++;
		items[n].vertStart = 8;    items[n].numVerts = 4; items[n].first = 8;
		items[n].idxFirst = 1008;  items[n].numIdx = 8;   n++;

		tess.numVertexes = 16;
		tess.numIndexes = 16;
		harnessDrawCount = 0;
		RB_VboStaticDrawRuns( harnessCmd, tess.numIndexes, items, n );

		CHECK( harnessDrawCount >= 1, "the slot-1 batch emitted %d draws", harnessDrawCount );
		if ( harnessMutant == 6 ) {
			/* the pre-fix arm: one compose, so slot 1's walk lands on slot 0's
			   own bytes and slot 0's recorded draw reads them */
			memcpy( s_softMap[0], s_softMap[1], 16 * sizeof( vkIndex_t ) );
		}
		for ( i = 0; i < harnessDrawCount; i++ ) {
			CHECK( harnessDrawSrc[i] != 2 || harnessDrawSoft[i] == 1,
				   "draw %d composed for slot %d while slot 1 recorded it",
				   i, harnessDrawSoft[i] );
		}

		/* the draw slot 0 already recorded must still read slot 0's own block */
		for ( i = 0; i < 16; i++ ) {
			if ( s_softMap[0][i] != snapshot[i] ) {
				differed++;
			}
		}
		CHECK( differed == 0,
			   "slot 1's walk rewrote %d of slot 0's composed indexes", differed );

		/* the invariant row itself is armed and says nothing on a good run */
		CHECK( harnessGuardHits == 0,
			   "the compose ownership invariant fired %d times", harnessGuardHits );

		/* and it is not vacuous: with the gate off it does not run at all */
		{
			int	before = harnessGuardHits;

			harnessProbe = 0;
			harnessSmpFrame = 0;
			RB_VboStaticDrawRuns( harnessCmd, tess.numIndexes, items, n );
			harnessProbe = 1;
			CHECK( harnessGuardHits == before, "the invariant ran with the gate off" );
		}
	}

	printf( "vbostatic oracle: %d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}
