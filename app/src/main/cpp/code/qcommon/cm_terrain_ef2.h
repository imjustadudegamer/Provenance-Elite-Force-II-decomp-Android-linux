
#ifndef __CM_TERRAIN_EF2_H__
#define __CM_TERRAIN_EF2_H__

#ifdef __cplusplus
extern "C" {
#endif

#define EF2_TERRAIN_VERTS   81      
#define EF2_TERRAIN_NODES   128     
#define EF2_TERRAIN_LEAVES  64      
#define EF2_TERRAIN_NONE    255     

typedef struct {
	byte	bounds[4];
	byte	children[2];
	byte	tri[2][3];
} ef2TerrainNode_t;

typedef struct cTerrainEF2_s {
	int					checkcount;
	int					surfaceFlags;
	int					contents;
	int					flipFlag;
	int					shaderNum;
	ef2TerrainNode_t	nodes[EF2_TERRAIN_NODES];
	vec3_t				pts[EF2_TERRAIN_VERTS];
} cTerrainEF2_t;

struct traceWork_s;

void CM_Terrain_InitializeEF2( void );
void CM_Terrain_BuildAABBTreeEF2( cTerrainEF2_t *terrain, int flip, const int holes[4] );
void CM_TraceThroughTerrainEF2( struct traceWork_s *tw, cTerrainEF2_t *terrain );

#ifdef __cplusplus
}
#endif

#endif 
