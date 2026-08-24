
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "polylib.h"
#include "imagelib.h"
#include "threads.h"
#include "scriplib.h"

#include "shaders.h"
#include "mesh.h"



typedef enum
{
	emit_point,
	emit_area,
	emit_spotlight,
	emit_sun
} emittype_t;

#define	MAX_LIGHT_EDGES		8
typedef struct light_s
{
	struct light_s *next;
	emittype_t	type;
	struct shaderInfo_s	*si;

	vec3_t		origin;
	vec3_t		normal;		
	float		dist;		

	qboolean	linearLight;
	int			photons;
	int			style;
	vec3_t		color;
	float		radiusByDist;	

	qboolean	twosided;		

	winding_t	*w;
	vec3_t		emitColor;		
} light_t;


extern	float	lightscale;
extern	float	ambient;
extern	float	maxlight;
extern	float	direct_scale;
extern	float	entity_scale;

extern	qboolean	noSurfaces;



typedef struct cFacet_s {
	float	surface[4];
	int		numBoundaries;		
	float	boundaries[4][4];	

	vec3_t	points[4];			

	float	textureMatrix[2][4];	
} cFacet_t;

typedef struct {
	vec3_t		mins, maxs;
	vec3_t		origin;
	float		radius;

	qboolean	patch;

	int			numFacets;
	cFacet_t	*facets;

	shaderInfo_t	*shader;		
} surfaceTest_t;


typedef struct {
	vec3_t		filter;				

	vec3_t		hit;				
	float		hitFraction;		
	qboolean	passSolid;
} trace_t;

extern	surfaceTest_t	*surfaceTest[MAX_MAP_DRAW_SURFS];

void	InitTrace( void );

typedef struct {
	vec3_t		start, end;
	int			numOpenLeafs;
	int			openLeafNumbers[MAX_MAP_LEAFS];
	trace_t		*trace;
	int			patchshadows;
} traceWork_t;

void TraceLine( const vec3_t start, const vec3_t stop, trace_t *trace,
			   qboolean testAll, traceWork_t *tw );
qboolean PointInSolid( vec3_t start );




typedef struct {
	int		textureNum;
	int		x, y, width, height;

	qboolean	patch;
	mesh_t		mesh;

	vec3_t	origin;
	vec3_t	vecs[3];
} lightmap_t;


