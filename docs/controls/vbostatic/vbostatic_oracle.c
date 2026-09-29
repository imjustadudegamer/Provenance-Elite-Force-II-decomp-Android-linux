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

/* which index buffer each draw read: 1 device, 2 soft */
static int				harnessDrawSrc[4096];

static int				harnessBoundIndex;
static VkBuffer			harnessSoftHandle;
static VkBuffer			harnessDevIndexHandle;

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
	harnessBoundIndex = ( buffer == harnessSoftHandle ) ? 2 : 1;	/* soft is 3 */
}

void VK_Draw_DrawIndexed( VkCommandBuffer cmd, int indexCount, int firstIndex, int vertexBase )
{
	( void )cmd; ( void )vertexBase;
	if ( harnessDrawCount < 4096 ) {
		harnessDraws[harnessDrawCount].count = indexCount;
		harnessDraws[harnessDrawCount].firstIndex = firstIndex;
		harnessDrawSrc[harnessDrawCount] = harnessBoundIndex;
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
	if ( harnessBufferCount == 5 ) {
		harnessSoftHandle = (VkBuffer)(size_t)harnessBufferCount;
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
		static byte	*soft;
		int			kind = (int)(size_t)memory;

		if ( !vboStage ) {
			vboStage = (byte *)calloc( 1, 24u * 1024u * 1024u );
			iboStage = (byte *)calloc( 1, 24u * 1024u * 1024u );
			soft = (byte *)calloc( 1, 1u * 1024u * 1024u );
		}

		/* creation order: devVbo(1) devIbo(2) stageVbo(3) stageIbo(4) soft(5) */
		switch ( kind ) {
		case 3: *data = vboStage; break;
		case 4: *data = iboStage; break;
		case 5: *data = soft; break;
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

void VK_Harness_InstallProcs( void )
{
	qvk.MapMemory = harnessMapMemory;
	qvk.UnmapMemory = harnessUnmapMemory;
	qvk.CmdCopyBuffer = harnessCmdCopyBuffer;
}

/* the engine's surface batch boundary: the harness keeps the counters, which is
   all the fast arm's accounting needs */
void RB_EndSurface( void )
{
	harnessFrame++;		/* one draw boundary */
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
static int	harnessMutant;

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
									: s_softMap + harnessDraws[k].firstIndex;
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

	printf( "vbostatic oracle: %d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}
