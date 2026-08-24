
#include "qfiles.h"
#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void		CM_LoadMap( const char *name, qboolean clientload, int *checksum);
void		CM_ClearMap( void );
clipHandle_t CM_InlineModel( int index );		
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int contents );

void		CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
void		CM_ModelBoundsFromName( const char *name, vec3_t mins, vec3_t maxs );

byte		*CM_VisibilityPointer( void );

void		CM_PrintBSPFileSizes( void );

int			CM_NumClusters (void);
int			CM_NumInlineModels( void );
char		*CM_EntityString( void );
const char	*CM_MapTime( void );

int			CM_GetLightingGroup( const char *group_name );

int			CM_PointContents( const vec3_t p, clipHandle_t model );
int			CM_PointBrushNum( const vec3_t p, clipHandle_t model );
int			CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );

qboolean	CM_BoxSightTrace( const vec3_t start, const vec3_t end,
							 const vec3_t mins, const vec3_t maxs,
							 clipHandle_t model, int brushmask, qboolean cylinder );
qboolean	CM_TransformedBoxSightTrace( const vec3_t start, const vec3_t end,
										 const vec3_t mins, const vec3_t maxs,
										 clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, qboolean cylinder );
void		CM_BoxTrace ( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask, int cylinder );
void		CM_BoxTraceEx ( trace_t *results, const vec3_t start, const vec3_t end,
							const vec3_t mins, const vec3_t maxs,
							clipHandle_t model, int brushmask, int cylinder, int leafFlags );
void		CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  clipHandle_t model, int brushmask,
						  const vec3_t origin, const vec3_t angles, int cylinder );

byte		*CM_ClusterPVS (int cluster);

int			CM_PointLeafnum( const vec3_t p );

int			CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list,
		 					int listsize, int *lastLeaf );

int			CM_LeafCluster (int leafnum);
int			CM_LeafArea (int leafnum);

void		CM_AdjustAreaPortalState( int area1, int area2, qboolean open );
void		CM_ResetAreaPortals( void );
void		CM_WritePortalState( fileHandle_t f );
void		CM_ReadPortalState( fileHandle_t f );
int			CM_AreaForPoint( vec3_t vPos );
qboolean	CM_AreasConnected( int area1, int area2 );

int			CM_WriteAreaBits( byte *buffer, int area );

qboolean	CM_inPVS( vec3_t p1, vec3_t p2 );
qboolean	CM_LeafInPVS( int leaf1, int leaf2 );

dshader_t *CM_ShaderPointer( int iShaderNum );

int			CM_LerpTag( orientation_t *tag,  clipHandle_t model, int startFrame, int endFrame, 
					 float frac, const char *tagName );


int	CM_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

void CM_DrawDebugSurface( void (*drawPoly)(int color, int numPoints, float *points) );



#ifdef __cplusplus
}
#endif
