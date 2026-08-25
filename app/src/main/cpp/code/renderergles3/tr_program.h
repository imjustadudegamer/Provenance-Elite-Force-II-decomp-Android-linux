
#include "tr_vbo.h"

#ifndef __TR_PROGRAM_H__
#define __TR_PROGRAM_H__

#include "tr_local.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLP_RGBAGEN     0x00000001u  
#define GLP_TCGEN0      0x00000002u  
#define GLP_TCGEN1      0x00000004u  
#define GLP_TCMOD0      0x00000008u  
#define GLP_TCMOD1      0x00000010u  
#define GLP_FOG         0x00000020u  
#define GLP_LIGHTMAP    0x00000040u  
#define GLP_VERTEXLIT   0x00000080u  
#define GLP_LIGHTMAP_MT 0x00000100u  
/* 0x00000200 .. 0x00100000 are the per-arm selectors the GLSL reads; they are
 * derived, not free: GLES3_StageDefines() is the single writer. */
#define GLP_DLIGHT      0x00000200u  
#define GLP_2D          0x00000400u  
#define GLP_SKY         0x00000800u  
#define GLP_ALPHATEST   0x00001000u  
#define GLP_SKIP_COLOR  0x00002000u  
#define GLP_NORMAL      0x00004000u  
#define GLP_WAVE        0x00008000u  
#define GLP_SPECULAR    0x00010000u  
#define GLP_ENTITY_SPACE 0x00020000u 
#define GLP_DEFORM      0x00040000u  
#define GLP_SHADOW      0x00080000u  
#define GLP_HEIGHTFOG   0x00100000u  

#define GLP_TEXENV_ADD         0x00200000u
#define GLP_TEXENV_INTERPOLATE 0x00400000u
#define GLP_TEXENV_COMBINE4    0x00800000u

#define GLP_FAMILY_MASK     0xff000000u
#define GLP_FAMILY_STAGE    0x00000000u  
#define GLP_FAMILY_2D       0x01000000u  
#define GLP_FAMILY_DLIGHT   0x02000000u  
#define GLP_FAMILY_SKY      0x04000000u  
#define GLP_FAMILY_SPRITE   0x08000000u  
#define GLP_FAMILY_SUNFLARE 0x10000000u  
#define GLP_FAMILY_SHADOW   0x20000000u  
#define GLP_FAMILY_WIRE     0x40000000u  

#define GLP_MAX_PROGRAMS 512

typedef struct {
	unsigned		stateMask;		
	unsigned		stateValue;		
	const char		*name;			
} gles3StateDef_t;

extern const gles3StateDef_t	gles3StateDefs[];
extern const int				gles3NumStateDefs;

typedef struct {
	unsigned		mask;
	unsigned		value;
	const char		*name;
} gles3StateBitArm_t;

extern const gles3StateBitArm_t	gles3StateBitArms[];
extern const int				gles3NumStateBitArms;

typedef struct {
	unsigned		rgbGen;			
	unsigned		alphaGen;		
	unsigned		tcGen0;			
	unsigned		tcGen1;			
	unsigned		bundleCount;	
	unsigned		texEnv;			
	unsigned		fogStage;		
	unsigned		atest;			
	unsigned		defines;		
	const char		*program;		
	const char		*label;			
} gles3ProgramKey_t;

extern const gles3ProgramKey_t	gles3ProgramKeys[];
extern const int				gles3NumProgramKeys;

typedef struct {
	unsigned		gen;
	unsigned		value;
	const char		*symbol;		
	const char		*label;			
} gles3GenArm_t;

extern const gles3GenArm_t		gles3GenArms[];
extern const int				gles3NumGenArms;

typedef struct {
	unsigned		type;			
	const char		*symbol;
	const char		*label;
} gles3TcModArm_t;

extern const gles3TcModArm_t	gles3TcModArms[];
extern const int				gles3NumTcModArms;

typedef struct {
	unsigned		acff;			
	const char		*symbol;
	const char		*label;
} gles3AcffArm_t;

extern const gles3AcffArm_t		gles3AcffArms[];
extern const int				gles3NumAcffArms;

typedef struct {
	const char		*family;		
	const char		*program;
	const char		*label;
} gles3FastPath_t;

extern const gles3FastPath_t	gles3FastPaths[];
extern const int				gles3NumFastPaths;

typedef struct {
	const char		*name;			 
	unsigned		family;			
	const char		*vsName;		
	const char		*fsName;
} gles3ProgramDef_t;

extern const gles3ProgramDef_t	gles3ProgramDefs[];
extern const int				gles3NumProgramDefs;

#define GLES3_UNIFORM_TABLE \
	X( MVP,               "u_MVP" )               \
	X( MODELVIEW,         "u_ModelView" )         \
	X( MODELMATRIX,       "u_ModelMatrix" )       \
	X( VIEWORIGIN,        "u_ViewOrigin" )        \
	X( VIEWAXIS0,         "u_ViewAxis0" )         \
	X( LOCALVIEWORIGIN,   "u_LocalViewOrigin" )   \
	X( ENTITYORIGIN,      "u_EntityOrigin" )      \
	X( ENTITYAXIS,        "u_EntityAxis" )        \
	X( ENTITYCOLOR,       "u_EntityColor" )       \
	X( BASECOLOR,         "u_BaseColor" )         \
	X( VERTCOLOR,         "u_VertColor" )         \
	X( IDENTITYLIGHT,     "u_IdentityLight" )     \
	X( IDENTITYLIGHTBYTE, "u_IdentityLightByte" ) \
	X( ENTITYFADE,        "u_EntityFade" )        \
	X( PORTALRANGE,       "u_PortalRange" )       \
	X( SPECORIGIN,        "u_SpecOrigin" )        \
	X( SUNDIRECTION,      "u_SunDirection" )      \
	X( SKYALPHA,          "u_SkyAlpha" )          \
	X( SKYSCROLL,         "u_SkyScroll" )         \
	X( SKYALPHAMODE,      "u_SkyAlphaMode" )      \
	X( ENTITYTEXCOORD,    "u_EntityTexCoord" )    \
	X( COLORGEN,          "u_ColorGen" )          \
	X( ALPHAGEN,          "u_AlphaGen" )          \
	X( ALPHAMIN,          "u_AlphaMin" )          \
	X( ALPHAMAX,          "u_AlphaMax" )          \
	X( ALPHACONST,        "u_AlphaConst" )        \
	X( ALPHACONSTMIN,     "u_AlphaConstMin" )     \
	X( WAVERGB,           "u_WaveRGB" )           \
	X( WAVEALPHA,         "u_WaveAlpha" )         \
	X( FREQRGB,           "u_FreqRGB" )           \
	X( FREQALPHA,         "u_FreqAlpha" )         \
	X( TCGEN0,            "u_TCGen0" )            \
	X( TCGEN1,            "u_TCGen1" )            \
	X( TCGEN0VEC0,        "u_TCGen0Vec0" )        \
	X( TCGEN0VEC1,        "u_TCGen0Vec1" )        \
	X( TCGEN1VEC0,        "u_TCGen1Vec0" )        \
	X( TCGEN1VEC1,        "u_TCGen1Vec1" )        \
	X( NUMTCMOD0,         "u_NumTexMod0" )        \
	X( NUMTCMOD1,         "u_NumTexMod1" )        \
	X( TCMOD0,            "u_TexMod0" )           \
	X( TCMOD1,            "u_TexMod1" )           \
	X( ALPHATEST,         "u_AlphaTest" )         \
	X( FORCEALPHA,        "u_ForceAlpha" )        \
	X( TEXENV0,           "u_TexEnv0" )           \
	X( TEXENV1,           "u_TexEnv1" )           \
	X( TEX0,              "u_Tex0" )              \
	X( TEX1,              "u_Tex1" )              \
	X( TEX1COLOR,         "u_Tex1Color" )         \
	X( WAVETABLE,         "u_WaveTable" )         \
	X( CLIPPLANE,         "u_ClipPlane" )         \
	X( CLIPPLANEENABLE,   "u_ClipPlaneEnable" )   \
	X( SHADERTIME,        "u_ShaderTime" )        \
	X( ACFF,              "u_Acff" )              \
	X( FOGNUM,            "u_FogNum" )            \
	X( FOGTCSCALE,        "u_FogTcScale" )        \
	X( FOGSCONSTANT,      "u_FogSConstant" )      \
	X( FOGSVECTOR,        "u_FogSVector" )        \
	X( FOGTCONSTANT,      "u_FogTConstant" )      \
	X( FOGTVECTOR,        "u_FogTVector" )        \
	X( FOGVIEWOUTSIDE,    "u_FogViewOutside" )    \
	X( FOGCOLOR,          "u_FogColor" )          \
	X( FOGDENSITYARM,     "u_FogDensityArm" )     \
	X( FOGOOLEN,          "u_FogOolen" )          \
	X( FOGPLANECONST,     "u_FogPlaneConst" )     \
	X( FOGDVIEW,          "u_FogDView" )          \
	X( FOGDENSITYTVECTOR, "u_FogDensityTVector" ) \
	X( FOGMODE,           "u_FogMode" )           \
	X( FOGSTART,          "u_FogStart" )          \
	X( FOGEND,            "u_FogEnd" )            \
	X( FOGFIXCOLOR,       "u_FogFixColor" )

typedef enum {
#define X( e, n ) GLES3_U_##e,
	GLES3_UNIFORM_TABLE
#undef X
	GLES3_U_COUNT
} gles3Uniform_t;

typedef struct {
	char			name[64];
	unsigned		defines;
	unsigned		program;		
	int				uniforms[GLES3_U_COUNT];
	qboolean		linked;
} gles3Program_t;

extern gles3Program_t	gles3Programs[GLP_MAX_PROGRAMS];
extern int				gles3NumPrograms;

void		GLES3_ProgramsInit( void );
void		GLES3_ProgramsShutdown( void );
void		GLES3_PrebuildPrograms( void );
void		GLES3_CloseProgramRegistration( void );
int			GLES3_ProgramsCreatedAfterRegistration( void );
gles3Program_t *GLES3_GetProgram( unsigned defines );
gles3Program_t *GLES3_StageProgram( const shaderStage_t *pStage, const shader_t *shader, unsigned extraDefines );
gles3Program_t *GLES3_FamilyProgram( unsigned family );

unsigned	GLES3_StageDefines( const shaderStage_t *pStage, const shader_t *shader );
unsigned	GLES3_StageDefines_Full( const shaderStage_t *pStage, const shader_t *shader,
									  unsigned bundleCount, unsigned texEnv, unsigned fogStage,
									  unsigned extra );
unsigned	GLES3_TexEnvForStage( const shaderStage_t *pStage, const shader_t *shader );

void		GLES3_SetUniform1i( gles3Program_t *p, gles3Uniform_t u, int v );
void		GLES3_SetUniform1f( gles3Program_t *p, gles3Uniform_t u, float v );
void		GLES3_SetUniform2f( gles3Program_t *p, gles3Uniform_t u, float x, float y );
void		GLES3_SetUniform3f( gles3Program_t *p, gles3Uniform_t u, float x, float y, float z );
void		GLES3_SetUniform4f( gles3Program_t *p, gles3Uniform_t u, float x, float y, float z, float w );
void		GLES3_SetUniformMat4( gles3Program_t *p, gles3Uniform_t u, const float *m );
void		GLES3_SetUniform3fv( gles3Program_t *p, gles3Uniform_t u, const float *v );
void		GLES3_SetUniform4fv( gles3Program_t *p, gles3Uniform_t u, const float *v );

void		GLES3_ApplyStateBits( unsigned long stateBits );
unsigned	GLES3_CurrentAlphaTest( void );

qboolean	GLES3_StageNeedsCPUColor( const shaderStage_t *pStage );
qboolean	GLES3_StageNeedsCPUTexCoords( const shaderStage_t *pStage );

void		GLES3_SelectTexture( int unit );
void		GLES3_BindAnimatedImage( const textureBundle_t *bundle, int unit );

void		GLES3_SetupStageUniforms( gles3Program_t *p, const shaderStage_t *pStage, const shader_t *shader );
void		GLES3_SetupFogUniforms( gles3Program_t *p, int fogNum, qboolean fogDensityArm );
void		GLES3_SetupViewUniforms( gles3Program_t *p );
void		GLES3_DrawStage( const shaderCommands_t *input, const shaderStage_t *pStage,
							 const shader_t *shader, unsigned stateBits, float forceAlpha,
							  qboolean hudColorClobber );

void		GLES3_BindProgram( gles3Program_t *p );
void		GLES3_RefreshClipPlane( void );

gles3Program_t	*GLES3_BeginFamily( unsigned family, int texEnv );
void		GLES3_Begin2D( int texEnv );		
void		GLES3_BeginWire( void );		
void		GLES3_BeginSprite( void );		
void		GLES3_BeginSunflare( void );		
void		GLES3_BeginShadow( void );		
void		GLES3_SetBaseColor4f( gles3Program_t *p, float r, float g, float b, float a );

void		GLES3_WireBegin( void );
void		GLES3_WireVertex( const float *xyz, const float *rgba );
void		GLES3_WireEnd( GLenum mode );

void		GLES3_CreateWaveTable( void );
void		GLES3_DestroyWaveTable( void );

#ifdef __cplusplus
}	
#endif

#endif 
