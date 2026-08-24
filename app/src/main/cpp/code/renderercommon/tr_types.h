#ifndef __TR_TYPES_H
#define __TR_TYPES_H


#define	MAX_DLIGHTS			32		
#define MAX_REAL_LIGHTS		32

#define	REFENTITYNUM_BITS	12		
#define	REFENTITYNUM_MASK	((1<<REFENTITYNUM_BITS) - 1)
#define	MAX_REFENTITIES		((1<<REFENTITYNUM_BITS) - 1)
#define	REFENTITYNUM_WORLD	((1<<REFENTITYNUM_BITS) - 1)

#define RF_CROSSHAIR		(1<<31)		

#define	RF_NOSHADOW		0x0040		

#define RDF_NOWORLDMODEL	0x0001		
#define RDF_HUD				0x0002
#define RDF_HYPERSPACE		0x0004		

typedef struct {
	vec3_t		xyz;
	float		st[2];
	byte		modulate[4];
} polyVert_t;

typedef struct poly_s {
	qhandle_t			hShader;
	int					numVerts;
	polyVert_t			*verts;
} poly_t;

// Every value below was read out of the retail PPC engine, not inferred:
// not retail's numeric, and the number is observable: cg_archive.cpp archives
typedef enum {
	RT_MODEL,			
	RT_POLY,			
	RT_SPRITE,			
	RT_BEAM,			
	RT_PORTALSURFACE,	

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

struct skelFrame_s;
struct dtiki_s;

// decompiles, not inferred.
typedef struct refEntity_s {
	refEntityType_t	reType;                     
	int			renderfx;                       

	qhandle_t	hModel;				 

	vec3_t		lightingOrigin;		 
	float		shadowPlane;		 

	vec3_t		axis[3];			 
	qboolean	nonNormalizedAxes;	// axis are not normalized       retail +64
	vec3_t		origin;				 

	int			anim;                           
	int			oldanim;                        
	short		oldframe;                       
	short		frame;                          
	float		scale;                          

	int			boneTagGate;                    

	int			torso_anim;                     
	int			torso_oldanim;                  
	short		torso_frame;                    
	short		torso_oldframe;                 

	float		crossblend_lerp;                
	float		crossblend_framelerp;           
	int			crossblend_anim;                
	short		crossblend_frame;               
	short		crossblend_oldframe;            

	int			legsLive;                       

	float		torso_crossblend_lerp;          
	float		torso_crossblend_framelerp;     
	int			torso_crossblend_anim;          
	short		torso_crossblend_frame;         
	short		torso_crossblend_oldframe;      
	int			torsoLive;                      

	vec3_t		attachOrigin;                   

	float		framelerp;                      
	float		torso_framelerp;                

	int			skinNum;			 
	qhandle_t	customSkin;			 
	qhandle_t	customShader;		 

	byte		shaderRGBA[4];		
	float		shaderTexCoord[2];	 
	float		shaderTime;			 

	int			entityNumber;		 

	byte		surfaces[MAX_MODEL_SURFACES];  

	// Retail +236/+238 are two INDEPENDENT halfwords, not one int: CG_ModelAnim
	short		tag_num;                        
	short		skin_num;                       

	int			bone_tag[NUM_BONE_CONTROLLERS];  

	// Retail +260..+339: the five bone-controller quaternions, INLINE (not the
	vec4_t		bone_quat[NUM_BONE_CONTROLLERS];    

	morph_controller_t	morph_controllers[NUM_MORPH_CONTROLLERS];    

	struct skelFrame_s	*of;                    
	struct skelFrame_s	*nf;                    
	struct dtiki_s		*tiki;                  
	int					bonestart;              

	float				fade;                   

	float				radius;                 
	float				rotation;               

	// PORT-ONLY TAIL.  Nothing below this line has a retail offset.  Every member
	// SETTLED, KEPT -- port implementation state, not IOQ3 and not retail-pinnable

	float		actionWeight;



	int				morphstart;
	qboolean		hasMorph;
} refEntity_t;

#if defined(__cplusplus)
static_assert(sizeof(refEntity_t) > 448, "retail refEntity_t is 448 bytes before the port-only tail");
static_assert(offsetof(refEntity_t, renderfx) == 4, "retail refEntity_t+4");
static_assert(offsetof(refEntity_t, hModel) == 8, "retail refEntity_t+8");
static_assert(offsetof(refEntity_t, lightingOrigin) == 12, "retail refEntity_t+12");
static_assert(offsetof(refEntity_t, shadowPlane) == 24, "retail refEntity_t+24");
static_assert(offsetof(refEntity_t, axis) == 28, "retail refEntity_t+28");
static_assert(offsetof(refEntity_t, nonNormalizedAxes) == 64, "retail refEntity_t+64");
static_assert(offsetof(refEntity_t, origin) == 68, "retail refEntity_t+68");
static_assert(offsetof(refEntity_t, anim) == 80, "retail refEntity_t+80");
static_assert(offsetof(refEntity_t, oldanim) == 84, "retail refEntity_t+84");
static_assert(offsetof(refEntity_t, oldframe) == 88, "retail refEntity_t+88");
static_assert(offsetof(refEntity_t, frame) == 90, "retail refEntity_t+90");
static_assert(offsetof(refEntity_t, scale) == 92, "retail refEntity_t+92");
static_assert(offsetof(refEntity_t, boneTagGate) == 96, "retail refEntity_t+96");
static_assert(offsetof(refEntity_t, torso_anim) == 100, "retail refEntity_t+100");
static_assert(offsetof(refEntity_t, torso_oldanim) == 104, "retail refEntity_t+104");
static_assert(offsetof(refEntity_t, torso_frame) == 108, "retail refEntity_t+108");
static_assert(offsetof(refEntity_t, torso_oldframe) == 110, "retail refEntity_t+110");
static_assert(offsetof(refEntity_t, crossblend_lerp) == 112, "retail refEntity_t+112");
static_assert(offsetof(refEntity_t, crossblend_framelerp) == 116, "retail refEntity_t+116");
static_assert(offsetof(refEntity_t, crossblend_anim) == 120, "retail refEntity_t+120");
static_assert(offsetof(refEntity_t, crossblend_frame) == 124, "retail refEntity_t+124");
static_assert(offsetof(refEntity_t, crossblend_oldframe) == 126, "retail refEntity_t+126");
static_assert(offsetof(refEntity_t, legsLive) == 128, "retail refEntity_t+128");
static_assert(offsetof(refEntity_t, torso_crossblend_lerp) == 132, "retail refEntity_t+132");
static_assert(offsetof(refEntity_t, torso_crossblend_framelerp) == 136, "retail refEntity_t+136");
static_assert(offsetof(refEntity_t, torso_crossblend_anim) == 140, "retail refEntity_t+140");
static_assert(offsetof(refEntity_t, torso_crossblend_frame) == 144, "retail refEntity_t+144");
static_assert(offsetof(refEntity_t, torso_crossblend_oldframe) == 146, "retail refEntity_t+146");
static_assert(offsetof(refEntity_t, torsoLive) == 148, "retail refEntity_t+148");
static_assert(offsetof(refEntity_t, attachOrigin) == 152, "retail refEntity_t+152");
static_assert(offsetof(refEntity_t, framelerp) == 164, "retail refEntity_t+164");
static_assert(offsetof(refEntity_t, torso_framelerp) == 168, "retail refEntity_t+168");
static_assert(offsetof(refEntity_t, skinNum) == 172, "retail refEntity_t+172");
static_assert(offsetof(refEntity_t, customSkin) == 176, "retail refEntity_t+176");
static_assert(offsetof(refEntity_t, customShader) == 180, "retail refEntity_t+180");
static_assert(offsetof(refEntity_t, shaderRGBA) == 184, "retail refEntity_t+184");
static_assert(offsetof(refEntity_t, shaderTexCoord) == 188, "retail refEntity_t+188");
static_assert(offsetof(refEntity_t, shaderTime) == 196, "retail refEntity_t+196");
static_assert(offsetof(refEntity_t, entityNumber) == 200, "retail refEntity_t+200");
static_assert(offsetof(refEntity_t, surfaces) == 204, "retail refEntity_t+204");
static_assert(offsetof(refEntity_t, tag_num) == 236, "retail refEntity_t+236");
static_assert(offsetof(refEntity_t, skin_num) == 238, "retail refEntity_t+238");
static_assert(offsetof(refEntity_t, bone_tag[0]) == 240, "retail refEntity_t+240");
static_assert(offsetof(refEntity_t, bone_tag[4]) == 256, "retail refEntity_t+256");
static_assert(offsetof(refEntity_t, bone_quat) == 260, "retail refEntity_t+260");
static_assert(sizeof(((refEntity_t *)0)->bone_quat) == 80, "retail refEntity_t+260..+339");
static_assert(offsetof(refEntity_t, morph_controllers) == 340, "retail refEntity_t+340");
static_assert(sizeof(((refEntity_t *)0)->morph_controllers) == 80, "retail refEntity_t+340..+419");

#define REFENT_PTRDELTA (sizeof(void *) - 4)
static_assert(offsetof(refEntity_t, of) == 420 + 1 * REFENT_PTRDELTA, "retail refEntity_t+420 (of)");
static_assert(offsetof(refEntity_t, nf) == 424 + 2 * REFENT_PTRDELTA, "retail refEntity_t+424 (nf)");
static_assert(offsetof(refEntity_t, tiki) == 428 + 3 * REFENT_PTRDELTA, "retail refEntity_t+428 (tiki)");
static_assert(offsetof(refEntity_t, bonestart) == 432 + 4 * REFENT_PTRDELTA, "retail refEntity_t+432 (bonestart)");
static_assert(offsetof(refEntity_t, fade) == 436 + 4 * REFENT_PTRDELTA, "retail refEntity_t+436 (fade)");
static_assert(offsetof(refEntity_t, radius) == 440 + 4 * REFENT_PTRDELTA, "retail refEntity_t+440 (radius)");
static_assert(offsetof(refEntity_t, rotation) == 444 + 4 * REFENT_PTRDELTA, "retail refEntity_t+444 (rotation)");
static_assert(offsetof(refEntity_t, entityNumber) == 200, "retail refEntity_t+200 (entityNumber)");
static_assert(offsetof(refEntity_t, bone_quat) == 260, "retail refEntity_t+260 (bone_quat)");
static_assert(offsetof(refEntity_t, morph_controllers) == 340, "retail refEntity_t+340 (morph_controllers)");
// retail offset, so the port-only tail is free to be any size.
static_assert(sizeof(refEntity_t) > 444 + 4 * REFENT_PTRDELTA, "retail tail ends at +444");
#undef REFENT_PTRDELTA
#endif 


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		

	int			time;

	int			rdflags;			

	byte		areamask[MAX_MAP_AREA_BYTES];

    char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	// Derived twice from the shipped Mac PPC image, not from a prior report:

    float       farplane_distance;       
    vec3_t      farplane_color;          
    qboolean    farplane_cull;           
    qboolean	farplane_fog;            
    // viewParms+0x288 / +0x28C, which is the same latch the global-terrain pass in
    qboolean	terrain_global_bool;     
    float		terrain_global_min;      

    float       entity_fade_dist;        

    qboolean    sky_portal;              
    float       sky_alpha;               
    vec3_t      sky_origin;              
    vec3_t      sky_axis[3];             

	// (the deleted oracle's tr_backend.c:1255), i.e. a sanctioned platform-layer consumer, pinned
	float		farplane_bias;           
} refdef_t;

#if defined(__cplusplus)
static_assert(offsetof(refdef_t, farplane_distance) == 0x170, "retail refdef_t+0x170");
static_assert(offsetof(refdef_t, farplane_color) == 0x174, "retail refdef_t+0x174");
static_assert(offsetof(refdef_t, farplane_cull) == 0x180, "retail refdef_t+0x180");
static_assert(offsetof(refdef_t, farplane_fog) == 0x184, "retail refdef_t+0x184");
static_assert(offsetof(refdef_t, terrain_global_bool) == 0x188, "retail refdef_t+0x188");
static_assert(offsetof(refdef_t, terrain_global_min) == 0x18C, "retail refdef_t+0x18C");
static_assert(offsetof(refdef_t, entity_fade_dist) == 0x190, "retail refdef_t+0x190");
static_assert(offsetof(refdef_t, sky_portal) == 0x194, "retail refdef_t+0x194");
static_assert(offsetof(refdef_t, sky_alpha) == 0x198, "retail refdef_t+0x198");
static_assert(offsetof(refdef_t, sky_origin) == 0x19C, "retail refdef_t+0x19C");
static_assert(offsetof(refdef_t, sky_axis) == 0x1A8, "retail refdef_t+0x1A8");
static_assert(offsetof(refdef_t, farplane_bias) == 0x1CC, "accepted-platform tail starts at retail's sizeof");
static_assert(sizeof(refdef_t) == 0x1D0, "retail 0x1CC + the one accepted-platform farplane_bias");
#endif


typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;


typedef enum {
	TC_NONE,
	TC_S3TC,  
	TC_S3TC_ARB  
} textureCompression_t;

typedef enum {
	GLDRV_ICD,					
	GLDRV_STANDALONE,			
	GLDRV_VOODOO				
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,			
	GLHW_3DFX_2D3D,			
	GLHW_RIVA128,			
	GLHW_RAGEPRO,			
	GLHW_PERMEDIA2			
} glHardwareType_t;

typedef struct {
	char					renderer_string[1024];
	char					vendor_string[1024];
	char					version_string[1024];
	char					extensions_string[2048];

	int						maxTextureSize;			
	int						numTextureUnits;		

	int						colorBits, depthBits, stencilBits;

	glDriverType_t			driverType;
	glHardwareType_t		hardwareType;

	qboolean				deviceSupportsGamma;
	textureCompression_t	textureCompression;
	qboolean				textureEnvAddAvailable;

	// dword 1290: a boolean retail tests but whose meaning the corpus does not name.
	qboolean				unknownCapability;
	qboolean				cubeMapAvailable;
	qboolean				textureEnvCombineAvailable;
	int						vidWidth, vidHeight;
	float					windowAspect;

	int						displayFrequency;

	qboolean				isFullscreen;
	qboolean				stereoEnabled;
    qboolean				smpActive;		
    int						registerCombinerAvailable;
    qboolean				secondaryColorAvailable;
    qboolean				VAR;
} glconfig_t;


static_assert(sizeof(glconfig_t) == 5212, "glconfig_t must be 5212 bytes (retail EF2 GLimp_Init memset)");
static_assert(offsetof(glconfig_t, extensions_string) == 3072, "glconfig_t string block drift");
static_assert(offsetof(glconfig_t, hardwareType) == 5144, "glconfig_t dword 1286 drift");
static_assert(offsetof(glconfig_t, textureCompression) == 5152, "glconfig_t dword 1288 drift");
static_assert(offsetof(glconfig_t, textureEnvAddAvailable) == 5156, "glconfig_t dword 1289 drift");
static_assert(offsetof(glconfig_t, cubeMapAvailable) == 5164, "glconfig_t dword 1291 (cube map) drift");
static_assert(offsetof(glconfig_t, textureEnvCombineAvailable) == 5168, "glconfig_t dword 1292 drift");
static_assert(offsetof(glconfig_t, vidWidth) == 5172, "glconfig_t vidWidth must be retail's +5172 (guest RE_StretchRaw reads its w there)");
static_assert(offsetof(glconfig_t, vidHeight) == 5176, "glconfig_t vidHeight must be retail's +5176");

#include "new/tr_types_new.h"

#endif	
