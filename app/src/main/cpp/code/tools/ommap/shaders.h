

typedef struct shaderInfo_s {
	char		shader[MAX_QPATH];
	int			surfaceFlags;
	int			contents;
	int			value;

	char		backShader[MAX_QPATH];	
	char		flareShader[MAX_QPATH];	

	float		subdivisions;			
	float		backsplashFraction;		
	float		backsplashDistance;		
	float		lightSubdivide;			
	int			lightmapSampleSize;		

	qboolean	hasPasses;				

	qboolean	globalTexture;			

	qboolean	twoSided;				
	qboolean	autosprite;				
	qboolean	lightFilter;			
	qboolean	forceTraceLight;		
	qboolean	forceVLight;			
	qboolean	patchShadows;			
	qboolean	vertexShadows;			
	qboolean	noVertexShadows;		
	qboolean	forceSunLight;			
	qboolean	notjunc;				
	float		vertexScale;			

	char		editorimage[MAX_QPATH];	
	char		lightimage[MAX_QPATH];	
	vec3_t		color;					
	vec3_t		averageColor;

	int			width, height;
	byte		*pixels;

	vec3_t		sunLight;
	vec3_t		sunDirection;
} shaderInfo_t;

void LoadShaderInfo( void );
shaderInfo_t	*ShaderInfoForShader( const char *shader );

