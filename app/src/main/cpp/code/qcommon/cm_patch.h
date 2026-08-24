



#define	MAX_FACETS			1024
#define	MAX_PATCH_PLANES	6144

typedef struct {
	float	plane[4];
	int		signbits;		
} patchPlane_t;

typedef struct {
	int			surfacePlane;
	int			numBorders;		
	int			borderPlanes[4+6+16];
	int			borderInward[4+6+16];
	qboolean	borderNoAdjust[4+6+16];
} facet_t;

typedef struct patchCollide_s {
	vec3_t	bounds[2];
	int		numPlanes;			
	patchPlane_t	*planes;
	int		numFacets;
	facet_t	*facets;
} patchCollide_t;


#define	MAX_GRID_SIZE	129

typedef struct {
	int			width;
	int			height;
	qboolean	wrapWidth;
	qboolean	wrapHeight;
	vec3_t	points[MAX_GRID_SIZE][MAX_GRID_SIZE];	
} cGrid_t;

#define	SUBDIVIDE_DISTANCE	16	
#define	PLANE_TRI_EPSILON	0.1
#define	WRAP_POINT_EPSILON	0.1


patchCollide_t *CM_GeneratePatchCollide( int width, int height, vec3_t *points, float subdivisions );
