/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2026 the ProvenanceEF2 team

This file is part of a retail-exact decompilation port of Star Trek: Elite
Force II.  It is the OpenGL ES 3.0 successor to the fixed-function GL1
pipeline.

tr_program.c -- THE PROGRAM / STATE LAYER.

WHAT THIS REPLACES
------------------
The GL1 renderer (OpenMoHAA's GL1 backend) is fixed-function:
  * tr_backend.c:263  GL_State()        -> glBlendFunc/glDepthFunc/glDepthMask/
                                           glAlphaFunc/glPolygonMode/glTexEnv/
                                           glFog* + the wrap parameter
  * tr_backend.c:191  GL_TexEnv()       -> the texture-env combiner unit
  * tr_shade.c:806    ComputeColors()   -> a CPU vertex-colour ARRAY
  * tr_shade.c:1457   ComputeTexCoords()-> a CPU texcoord ARRAY
  * tr_shade.c:163    R_DrawElements()  -> glDrawElements over client arrays

GLES 3.0 has no colour array, no texture env, no glAlphaFunc, no glPolygonMode
and no fixed-function fog, so each of those becomes one of:
  * a PROGRAM PERMUTATION (GLP_* bitmask -> gles3ProgramDefs[] source pair),
  * a uniform (the generator arm; retail's own enum ordinals, no translation),
  * a render-state application (GLES3_ApplyStateBits), or
  * a CPU-side array uploaded as a vertex attribute (RB_VBO_DrawTessArrays,
    tr_vbo.c, at the attribute locations in tr_vbo.h:62-68).

THE TABLES
----------
  gles3StateDefs[]       one GLES3_STATE_DEF row per state word the shipped
                         corpus carries -- every shipped word must match a row
  gles3StateBitArms[]    every GLS_* nibble value GL_State can decode (audit)
  tr_program_table.inc   the shipped-content census: program keys, generator
                         arms, tcMod arms, acff arms, iterator families

REFERENCES
----------
  * the GL1 backend's tr_backend.c     GL_State/GL_TexEnv/GL_Cull/GL_Bind
  * the GL1 backend's tr_shade.c       ComputeColors/ComputeTexCoords/iterators
  * the GL1 backend's tr_shade_calc.c  every RB_Calc* generator
  * renderergl2 tr_glsl.c (the program cache + uniform table) and
    renderergl2 glsl/generic_vp.glsl
===========================================================================
*/

#include "tr_local.h"
#include "qgl_gles3_tokens.h"
#include "tr_program.h"
#include "tr_program_cache.h"
#include "tr_vbo.h"

#include "glsl/gles3_glsl_embedded.h"

float	EvalWaveForm( const waveForm_t *wf );

#undef GLES3_PROGRAM_KEY
#define GLES3_PROGRAM_KEY(rgb, alpha, tc0, tc1, bc, te, fs, at, def, prog, lbl) \
	{ rgb, alpha, tc0, tc1, bc, te, fs, at, def, prog, lbl },
#undef GLES3_GEN_ARM
#define GLES3_GEN_ARM(gen, value, symbol, label)
#undef GLES3_TCMOD_ARM
#define GLES3_TCMOD_ARM(type, symbol, label)
#undef GLES3_ACFF_ARM
#define GLES3_ACFF_ARM(value, symbol, label)
#undef GLES3_FASTPATH
#define GLES3_FASTPATH(family, program, label)
#define GLES3_TABLE_PROGRAM_KEYS
const gles3ProgramKey_t gles3ProgramKeys[] = {
#include "tr_program_table.inc"
};
#undef GLES3_TABLE_PROGRAM_KEYS
#undef GLES3_PROGRAM_KEY

#undef GLES3_PROGRAM_KEY
#define GLES3_PROGRAM_KEY(rgb, alpha, tc0, tc1, bc, te, fs, at, def, prog, lbl)
#undef GLES3_GEN_ARM
#define GLES3_GEN_ARM(gen, value, symbol, label) { gen, value, symbol, label },
#define GLES3_TABLE_GEN_ARMS
const gles3GenArm_t gles3GenArms[] = {
#include "tr_program_table.inc"
};
#undef GLES3_TABLE_GEN_ARMS
#undef GLES3_GEN_ARM

#undef GLES3_GEN_ARM
#define GLES3_GEN_ARM(gen, value, symbol, label)
#undef GLES3_TCMOD_ARM
#define GLES3_TCMOD_ARM(type, symbol, label) { type, symbol, label },
#define GLES3_TABLE_TCMOD_ARMS
const gles3TcModArm_t gles3TcModArms[] = {
#include "tr_program_table.inc"
};
#undef GLES3_TABLE_TCMOD_ARMS
#undef GLES3_TCMOD_ARM

#undef GLES3_TCMOD_ARM
#define GLES3_TCMOD_ARM(type, symbol, label)
#undef GLES3_ACFF_ARM
#define GLES3_ACFF_ARM(value, symbol, label) { value, symbol, label },
#define GLES3_TABLE_ACFF_ARMS
const gles3AcffArm_t gles3AcffArms[] = {
#include "tr_program_table.inc"
};
#undef GLES3_TABLE_ACFF_ARMS
#undef GLES3_ACFF_ARM

#undef GLES3_ACFF_ARM
#define GLES3_ACFF_ARM(value, symbol, label)
#undef GLES3_FASTPATH
#define GLES3_FASTPATH(family, program, label) { family, program, label },
#define GLES3_TABLE_FASTPATHS
const gles3FastPath_t gles3FastPaths[] = {
#include "tr_program_table.inc"
};
#undef GLES3_TABLE_FASTPATHS
#undef GLES3_FASTPATH

const int	gles3NumProgramKeys = (int)( sizeof( gles3ProgramKeys ) / sizeof( gles3ProgramKeys[0] ) );
const int	gles3NumGenArms      = (int)( sizeof( gles3GenArms )      / sizeof( gles3GenArms[0] ) );
const int	gles3NumTcModArms    = (int)( sizeof( gles3TcModArms )    / sizeof( gles3TcModArms[0] ) );
const int	gles3NumAcffArms     = (int)( sizeof( gles3AcffArms )     / sizeof( gles3AcffArms[0] ) );
const int	gles3NumFastPaths    = (int)( sizeof( gles3FastPaths )    / sizeof( gles3FastPaths[0] ) );

#define GLES3_STATE_DEF(mask, value, name) { mask, value, name },
const gles3StateDef_t gles3StateDefs[] = {
	GLES3_STATE_DEF(0x70008fff, 0x00000013, "blend DST_COLOR/ZERO")
	GLES3_STATE_DEF(0x70008fff, 0x00000021, "blend ZERO/ONE")
	GLES3_STATE_DEF(0x70008fff, 0x00000022, "blend ONE/ONE -- additive")
	GLES3_STATE_DEF(0x70008fff, 0x00000023, "blend DST_COLOR/ONE")
	GLES3_STATE_DEF(0x70008fff, 0x00000025, "blend SRC_ALPHA/ONE")
	GLES3_STATE_DEF(0x70008fff, 0x00000031, "blend ZERO/SRC_COLOR")
	GLES3_STATE_DEF(0x70008fff, 0x00000033, "blend DST_COLOR/SRC_COLOR")
	GLES3_STATE_DEF(0x70008fff, 0x00000041, "blend ZERO/ONE_MINUS_SRC_COLOR")
	GLES3_STATE_DEF(0x70008fff, 0x00000052, "blend ONE/SRC_ALPHA")
	GLES3_STATE_DEF(0x70008fff, 0x00000056, "blend ONE_MINUS_SRC_ALPHA/SRC_ALPHA")
	GLES3_STATE_DEF(0x70008fff, 0x00000061, "blend ZERO/ONE_MINUS_SRC_ALPHA")
	GLES3_STATE_DEF(0x70008fff, 0x00000065, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA")
	GLES3_STATE_DEF(0x70008fff, 0x00000100, "blend disabled + depthwrite")
	GLES3_STATE_DEF(0x70008fff, 0x00000113, "blend DST_COLOR/ZERO + depthwrite")
	GLES3_STATE_DEF(0x70008fff, 0x00000121, "blend ZERO/ONE + depthwrite")
	GLES3_STATE_DEF(0x70008fff, 0x00000122, "blend ONE/ONE + depthwrite")
	GLES3_STATE_DEF(0x70008fff, 0x00000131, "blend ZERO/SRC_COLOR + depthwrite")
	GLES3_STATE_DEF(0x70008fff, 0x00000165, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + depthwrite")
	GLES3_STATE_DEF(0x70008fff, 0x00000400, "blend disabled + nodepthtest")
	GLES3_STATE_DEF(0x70008fff, 0x00000425, "blend SRC_ALPHA/ONE + nodepthtest")
	GLES3_STATE_DEF(0x70008fff, 0x00000465, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + nodepthtest")
	GLES3_STATE_DEF(0x70008fff, 0x00000813, "blend DST_COLOR/ZERO + depthequal")
	GLES3_STATE_DEF(0x70008fff, 0x10000022, "blend ONE/ONE + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000025, "blend SRC_ALPHA/ONE + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000065, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000100, "blend disabled + depthwrite + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000113, "blend DST_COLOR/ZERO + depthwrite + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000122, "blend ONE/ONE + depthwrite + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000165, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + depthwrite + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x10000465, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + nodepthtest + atest GT_0")
	GLES3_STATE_DEF(0x70008fff, 0x40000023, "blend DST_COLOR/ONE + atest GE_80")
	GLES3_STATE_DEF(0x70008fff, 0x40000033, "blend DST_COLOR/SRC_COLOR + atest GE_80")
	GLES3_STATE_DEF(0x70008fff, 0x40000065, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + atest GE_80")
	GLES3_STATE_DEF(0x70008fff, 0x40000100, "blend disabled + depthwrite + atest GE_80")
	GLES3_STATE_DEF(0x70008fff, 0x40000122, "blend ONE/ONE + depthwrite + atest GE_80")
	GLES3_STATE_DEF(0x70008fff, 0x40000165, "blend SRC_ALPHA/ONE_MINUS_SRC_ALPHA + depthwrite + atest GE_80")
};
#undef GLES3_STATE_DEF

const int gles3NumStateDefs = (int)( sizeof( gles3StateDefs ) / sizeof( gles3StateDefs[0] ) );

const gles3StateBitArm_t gles3StateBitArms[] = {
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_ZERO,                "srcblend ZERO" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_ONE,                 "srcblend ONE" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_DST_COLOR,           "srcblend DST_COLOR" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_ONE_MINUS_DST_COLOR, "srcblend ONE_MINUS_DST_COLOR" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_SRC_ALPHA,           "srcblend SRC_ALPHA" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA, "srcblend ONE_MINUS_SRC_ALPHA" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_DST_ALPHA,           "srcblend DST_ALPHA" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_ONE_MINUS_DST_ALPHA, "srcblend ONE_MINUS_DST_ALPHA" },
	{ GLS_SRCBLEND_BITS, GLS_SRCBLEND_ALPHA_SATURATE,      "srcblend ALPHA_SATURATE" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_ZERO,                "dstblend ZERO" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_ONE,                 "dstblend ONE (the additive arm)" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_SRC_COLOR,           "dstblend SRC_COLOR" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_ONE_MINUS_SRC_COLOR, "dstblend ONE_MINUS_SRC_COLOR" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_SRC_ALPHA,           "dstblend SRC_ALPHA" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA, "dstblend ONE_MINUS_SRC_ALPHA" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_DST_ALPHA,           "dstblend DST_ALPHA" },
	{ GLS_DSTBLEND_BITS, GLS_DSTBLEND_ONE_MINUS_DST_ALPHA, "dstblend ONE_MINUS_DST_ALPHA" },
	{ GLS_DEPTHMASK_TRUE, GLS_DEPTHMASK_TRUE,              "depthmask TRUE" },
	{ GLS_DEPTHMASK_TRUE, 0,                               "depthmask FALSE" },
	{ GLS_POLYMODE_LINE, GLS_POLYMODE_LINE,                "polymode LINE (GLES3: gles3_wire)" },
	{ GLS_POLYMODE_LINE, 0,                                "polymode FILL" },
	{ GLS_DEPTHTEST_DISABLE, GLS_DEPTHTEST_DISABLE,        "depthtest disabled" },
	{ GLS_DEPTHTEST_DISABLE, 0,                            "depthtest enabled" },
	{ GLS_DEPTHFUNC_EQUAL, GLS_DEPTHFUNC_EQUAL,            "depthfunc EQUAL" },
	{ GLS_DEPTHFUNC_EQUAL, 0,                              "depthfunc LEQUAL" },
	{ GLS_CLAMP_EDGE, GLS_CLAMP_EDGE,                      "clamp to edge" },
	{ GLS_CLAMP_EDGE, 0,                                   "repeat" },
	{ GLS_CUBEMAP, GLS_CUBEMAP,                            "cubemap + texgen (glConfig.cubeMapAvailable has no writer)" },
	{ GLS_FOG, GLS_FOG,                                    "fixed-function fog (GLES3: shader fog)" },
	{ GLS_FOG, 0,                                          "no fixed-function fog" },
	{ GLS_ATEST_BITS, 0,                                   "alpha test off" },
	{ GLS_ATEST_BITS, GLS_ATEST_GT_0,                      "alpha test GREATER 0.0" },
	{ GLS_ATEST_BITS, GLS_ATEST_LT_80,                     "alpha test LESS 0.5" },
	{ GLS_ATEST_BITS, GLS_ATEST_GE_80,                     "alpha test GEQUAL 0.5" },
};
const int gles3NumStateBitArms = (int)( sizeof( gles3StateBitArms ) / sizeof( gles3StateBitArms[0] ) );

const gles3ProgramDef_t gles3ProgramDefs[] = {
	{ "gles3_stage",    GLP_FAMILY_STAGE,    "gles3_stage.vert",    "gles3_stage.frag" },
	{ "gles3_2d",       GLP_FAMILY_2D,       "gles3_2d.vert",       "gles3_2d.frag" },
	{ "gles3_dlight",   GLP_FAMILY_DLIGHT,   "gles3_dlight.vert",   "gles3_dlight.frag" },
	{ "gles3_sky",      GLP_FAMILY_SKY,      "gles3_sky.vert",      "gles3_sky.frag" },
	{ "gles3_sprite",   GLP_FAMILY_SPRITE,   "gles3_sprite.vert",   "gles3_sprite.frag" },
	{ "gles3_sunflare", GLP_FAMILY_SUNFLARE, "gles3_sunflare.vert", "gles3_sunflare.frag" },
	{ "gles3_shadow",   GLP_FAMILY_SHADOW,   "gles3_shadow.vert",   "gles3_shadow.frag" },
	{ "gles3_wire",     GLP_FAMILY_WIRE,     "gles3_wire.vert",     "gles3_wire.frag" },
};
const int gles3NumProgramDefs = (int)( sizeof( gles3ProgramDefs ) / sizeof( gles3ProgramDefs[0] ) );

gles3Program_t	gles3Programs[GLP_MAX_PROGRAMS];
int				gles3NumPrograms;

static qboolean	s_programsInitialised;
static unsigned	s_currentAlphaTest;	

int				gles3ProgramsCreated;
int				gles3ProgramsCreatedAfterReg;
static qboolean	s_registrationClosed;
static int		s_progCreateProbeRows;

#define GLES3_WAVETABLE_UNIT 2
static GLuint	s_waveTable;
static qboolean	s_waveTableReady;

/*
 * Uniform values live in the program object, so re-issuing the value a (program, location)
 * already carries costs a driver call and changes nothing.  A direct-mapped slot always
 * holds the last value written for its key, so a hit is exact and a collision only costs
 * the call it was going to make anyway.
 */
#define GLES3_UNIF_CACHE_BITS	11
#define GLES3_UNIF_CACHE_SIZE	( 1 << GLES3_UNIF_CACHE_BITS )
#define GLES3_UNIF_CACHE_VALS	16

typedef struct {
	unsigned	program;
	int			location;
	short		count;
	short		used;
	float		val[GLES3_UNIF_CACHE_VALS];
} gles3UnifCache_t;

static gles3UnifCache_t	s_unifCache[GLES3_UNIF_CACHE_SIZE];

void GLES3_CreateWaveTable( void )
{
	static float	lut[FUNCTABLE_SIZE * 5];
	int				i;

	if ( s_waveTableReady )
	{
		return;
	}
	if ( !qglGenTextures )
	{
		return;		
	}

	for ( i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		lut[0 * FUNCTABLE_SIZE + i] = tr.sinTable[i];
		lut[1 * FUNCTABLE_SIZE + i] = tr.squareTable[i];
		lut[2 * FUNCTABLE_SIZE + i] = tr.triangleTable[i];
		lut[3 * FUNCTABLE_SIZE + i] = tr.sawToothTable[i];
		lut[4 * FUNCTABLE_SIZE + i] = tr.inverseSawToothTable[i];
	}

	qglGenTextures( 1, &s_waveTable );
	qglActiveTexture( GL_TEXTURE0 + GLES3_WAVETABLE_UNIT );
	qglBindTexture( GL_TEXTURE_2D, s_waveTable );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	qglTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, FUNCTABLE_SIZE, 5, 0, GL_RED, GL_FLOAT, lut );

	qglActiveTexture( GL_TEXTURE0 + glState.currenttmu );

	s_waveTableReady = qtrue;
}

void GLES3_DestroyWaveTable( void )
{
	if ( s_waveTable && qglDeleteTextures )
	{
		qglDeleteTextures( 1, &s_waveTable );
	}
	s_waveTable = 0;
	s_waveTableReady = qfalse;
}

static const char *GLES3_GlslSource( const char *name )
{
	int i;

	for ( i = 0; i < GLES3_NUM_GLSL_SOURCES; i++ )
	{
		if ( !Q_stricmp( gles3GlslSources[i].name, name ) )
		{
			return gles3GlslSources[i].src;
		}
	}
	ri.Error( ERR_DROP, "GLES3_GlslSource: no embedded source '%s'\n", name );
	return "";
}

static const struct { unsigned bit; const char *name; } s_glpNames[] = {
	{ GLP_RGBAGEN,       "GLP_RGBAGEN" },
	{ GLP_TCGEN0,        "GLP_TCGEN0" },
	{ GLP_TCGEN1,        "GLP_TCGEN1" },
	{ GLP_TCMOD0,        "GLP_TCMOD0" },
	{ GLP_TCMOD1,        "GLP_TCMOD1" },
	{ GLP_FOG,           "GLP_FOG" },
	{ GLP_LIGHTMAP,      "GLP_LIGHTMAP" },
	{ GLP_VERTEXLIT,     "GLP_VERTEXLIT" },
	{ GLP_LIGHTMAP_MT,   "GLP_LIGHTMAP_MT" },
	{ GLP_DLIGHT,        "GLP_DLIGHT" },
	{ GLP_2D,            "GLP_2D" },
	{ GLP_SKY,           "GLP_SKY" },
	{ GLP_ALPHATEST,     "GLP_ALPHATEST" },
	{ GLP_SKIP_COLOR,    "GLP_SKIP_COLOR" },
	{ GLP_NORMAL,        "GLP_NORMAL" },
	{ GLP_WAVE,          "GLP_WAVE" },
	{ GLP_SPECULAR,      "GLP_SPECULAR" },
	{ GLP_ENTITY_SPACE,  "GLP_ENTITY_SPACE" },
	{ GLP_DEFORM,        "GLP_DEFORM" },
	{ GLP_SHADOW,        "GLP_SHADOW" },
	{ GLP_HEIGHTFOG,     "GLP_HEIGHTFOG" },
	{ GLP_TEXENV_ADD,         "GLP_TEXENV_ADD" },
	{ GLP_TEXENV_INTERPOLATE, "GLP_TEXENV_INTERPOLATE" },
	{ GLP_TEXENV_COMBINE4,    "GLP_TEXENV_COMBINE4" },
};

static char *GLES3_BuildSource( const char *base, unsigned defines )
{
	const char	*versionEnd;
	size_t		headLen, tailLen, cap;
	char		*out;
	int			i;

	versionEnd = strstr( base, "#version" );
	if ( versionEnd )
	{
		versionEnd = strchr( versionEnd, '\n' );
	}
	if ( !versionEnd )
	{
		ri.Error( ERR_DROP, "GLES3_BuildSource: source has no #version line\n" );
		return NULL;
	}
	versionEnd++;

	headLen = (size_t)( versionEnd - base );
	tailLen = strlen( versionEnd );
	cap = headLen + ( sizeof( s_glpNames ) / sizeof( s_glpNames[0] ) ) * 40 + tailLen + 4;

	out = (char *)ri.Malloc( cap );
	Com_Memcpy( out, base, headLen );
	out[headLen] = '\0';

	for ( i = 0; i < (int)( sizeof( s_glpNames ) / sizeof( s_glpNames[0] ) ); i++ )
	{
		if ( defines & s_glpNames[i].bit )
		{
			Q_strcat( out, (int)cap, "#define " );
			Q_strcat( out, (int)cap, s_glpNames[i].name );
			Q_strcat( out, (int)cap, " 1\n" );
		}
	}
	Q_strcat( out, (int)cap, versionEnd );
	return out;
}

static GLuint GLES3_CompileShader( GLenum type, const char *source, const char *label )
{
	GLuint		shader;
	GLint		status;
	static char	log[8192];
	GLsizei		len;

	shader = qglCreateShader( type );
	qglShaderSource( shader, 1, &source, NULL );
	qglCompileShader( shader );
	qglGetShaderiv( shader, GL_COMPILE_STATUS, &status );
	if ( !status )
	{
		log[0] = '\0';
		qglGetShaderInfoLog( shader, sizeof( log ) - 1, &len, log );
		ri.Error( ERR_DROP, "GLES3 program '%s' failed to compile:\n%s\n", label, log );
		qglDeleteShader( shader );
		return 0;
	}
	return shader;
}

static void GLES3_CacheUniforms( gles3Program_t *p )
{
	static const char *names[GLES3_U_COUNT] = {
#define X( e, n ) n,
		GLES3_UNIFORM_TABLE
#undef X
	};
	char		indexed[96];
	int			i;

	for ( i = 0; i < GLES3_U_COUNT; i++ )
	{
		p->uniforms[i] = qglGetUniformLocation( p->program, names[i] );
		if ( p->uniforms[i] == -1 )
		{
			Com_sprintf( indexed, sizeof( indexed ), "%s[0]", names[i] );
			p->uniforms[i] = qglGetUniformLocation( p->program, indexed );
		}
	}
}

static void GLES3_SetUniform4fvN( gles3Program_t *p, gles3Uniform_t u, const float *v, int count );

static int	s_progcacheRows;			
static int	s_progcacheStopLine;

typedef char progcache_gl_enums_agree[
	( GL_LINK_STATUS == PROGCACHE_GL_LINK_STATUS
	  && GL_PROGRAM_BINARY_LENGTH == PROGCACHE_GL_PROGRAM_BINARY_LENGTH
	  && GL_PROGRAM_BINARY_RETRIEVABLE_HINT == PROGCACHE_GL_PROGRAM_BINARY_RETRIEVABLE_HINT
	  && GL_ACTIVE_ATTRIBUTES == PROGCACHE_GL_ACTIVE_ATTRIBUTES
	  && GL_ACTIVE_UNIFORMS == PROGCACHE_GL_ACTIVE_UNIFORMS
	  && GL_NUM_PROGRAM_BINARY_FORMATS == PROGCACHE_GL_NUM_PROGRAM_BINARY_FORMATS ) ? 1 : -1 ];

static unsigned GLES3_ProgCacheSourceHash( void )
{
	unsigned	h = PROGCACHE_ProgCache_HashSeed();
	int			i;

	for ( i = 0; i < GLES3_NUM_GLSL_SOURCES; i++ )
	{
		h = PROGCACHE_ProgCache_FoldStr( h, gles3GlslSources[i].name );
		h = PROGCACHE_ProgCache_FoldStr( h, gles3GlslSources[i].src );
	}
	return h;
}

static unsigned GLES3_ProgCacheDefinesHash( void )
{
	unsigned	h = PROGCACHE_ProgCache_HashSeed();
	int			i, n = (int)( sizeof( s_glpNames ) / sizeof( s_glpNames[0] ) );

	for ( i = 0; i < n; i++ )
	{
		h = PROGCACHE_ProgCache_Fold( h, &s_glpNames[i].bit, (int)sizeof( s_glpNames[i].bit ) );
		h = PROGCACHE_ProgCache_FoldStr( h, s_glpNames[i].name );
	}
	return h;
}

static void GLES3_ProgCacheRow( const char *why )
{
	progcacheProgCacheStats_t	st;
	char					body[384];

	if ( !PROGCACHE_ProgCache_On() || !R_ProbeStatsOn() )
	{
		return;
	}
	if ( s_progcacheRows >= PROGCACHE_PROGCACHE_MAX_ROWS )
	{
		if ( !s_progcacheStopLine )
		{
			s_progcacheStopLine = 1;
			ri.Printf( PRINT_ALL, "[PROGCACHE:pc] capped at %d rows; no further lines\n",
					   PROGCACHE_PROGCACHE_MAX_ROWS );
		}
		return;
	}
	PROGCACHE_ProgCache_Stats( &st );
	PROGCACHE_ProgCache_Format( &st, body, (int)sizeof( body ) );
	ri.Printf( PRINT_ALL, "[PROGCACHE:pc] %s %s late=%d\n",
			   why, body, gles3ProgramsCreatedAfterReg );
	s_progcacheRows++;
}

static void GLES3_ProgCacheInitRow( void )
{
	progcacheProgCacheStats_t	st;
	char					body[384];
	cvar_t					*home;

	if ( !PROGCACHE_ProgCache_On() || !R_ProbeStatsOn()
		|| s_progcacheRows >= PROGCACHE_PROGCACHE_MAX_ROWS )
	{
		return;
	}
	home = ri.Cvar_Get( "fs_homedatapath", "", 0 );
	PROGCACHE_ProgCache_Stats( &st );
	PROGCACHE_ProgCache_Format( &st, body, (int)sizeof( body ) );
	ri.Printf( PRINT_ALL, "[PROGCACHE:pc] init %s file=%s home=%s\n",
			   body, PROGCACHE_PROGCACHE_FILE,
			   ( home && home->string[0] ) ? home->string : "(unset)" );
	s_progcacheRows++;
}

/* defines -> the index of the first program carrying it.  The linear walk this
 * replaces returned the first match in creation order, so the table is filled
 * first-wins and never rewritten, which is what keeps the answer identical. */
#define GLES3_PROG_HASH_BITS	10
#define GLES3_PROG_HASH_SIZE	( 1u << GLES3_PROG_HASH_BITS )

typedef struct {
	unsigned	defines;
	int			index;		/* -1 empty */
} gles3ProgHash_t;

static gles3ProgHash_t	s_progHash[GLES3_PROG_HASH_SIZE];

static unsigned GLES3_ProgHashSlot( unsigned defines )
{
	return ( defines * 2654435761u ) >> ( 32 - GLES3_PROG_HASH_BITS );
}

static void GLES3_ProgHashReset( void )
{
	int		i;

	for ( i = 0; i < GLES3_PROG_HASH_SIZE; i++ )
	{
		s_progHash[i].index = -1;
	}
}

/* The index the walk would have returned, or -1. */
static int GLES3_ProgHashFind( unsigned defines )
{
	unsigned	slot = GLES3_ProgHashSlot( defines );
	int			i;

	for ( i = 0; i < GLES3_PROG_HASH_SIZE; i++ )
	{
		gles3ProgHash_t	*e = &s_progHash[( slot + i ) & ( GLES3_PROG_HASH_SIZE - 1 )];

		if ( e->index < 0 )
		{
			return -1;
		}
		if ( e->defines == defines )
		{
			return e->index;
		}
	}
	return -1;
}

static void GLES3_ProgHashInsert( unsigned defines, int index )
{
	unsigned	slot = GLES3_ProgHashSlot( defines );
	int			i;

	for ( i = 0; i < GLES3_PROG_HASH_SIZE; i++ )
	{
		gles3ProgHash_t	*e = &s_progHash[( slot + i ) & ( GLES3_PROG_HASH_SIZE - 1 )];

		if ( e->index >= 0 )
		{
			if ( e->defines == defines )
			{
				return;		/* first wins, as the walk did */
			}
			continue;
		}
		e->defines = defines;
		e->index = index;
		return;
	}
}

static gles3Program_t *GLES3_NewProgram( const gles3ProgramDef_t *def, unsigned defines )
{
	gles3Program_t	*p;
	GLuint			vs, fs;
	char			*vsSource, *fsSource;
	GLint			status;
	static char		log[8192];
	GLsizei			len;
	unsigned		vsHash, fsHash, restored, savedUsec, restoreUsec;
	int				buildT0;
	qboolean		fromCache;

	if ( gles3NumPrograms >= GLP_MAX_PROGRAMS )
	{
		ri.Error( ERR_DROP, "GLES3_NewProgram: GLP_MAX_PROGRAMS (%d) exceeded\n", GLP_MAX_PROGRAMS );
		return NULL;
	}

	p = &gles3Programs[gles3NumPrograms];
	Com_Memset( p, 0, sizeof( *p ) );
	Q_strncpyz( p->name, def->name, sizeof( p->name ) );
	p->defines = defines;

	BE_RootBegin( BE_FAM_PROG );

	gles3ProgramsCreated++;
	if ( s_registrationClosed )
	{
		gles3ProgramsCreatedAfterReg++;
		if ( R_ProbeStatsOn() && s_progCreateProbeRows < 8 )
		{
			s_progCreateProbeRows++;
			ri.Printf( PRINT_WARNING,
					   "[SMP:progcreate] %s defines=0x%06x compiled AFTER RE_EndRegistration (#%d)\n",
					   def->name, defines, gles3ProgramsCreatedAfterReg );
		}
	}

	vsSource = GLES3_BuildSource( GLES3_GlslSource( def->vsName ), defines );
	fsSource = GLES3_BuildSource( GLES3_GlslSource( def->fsName ), defines );

	vsHash = PROGCACHE_ProgCache_Hash( vsSource, (int)strlen( vsSource ) );
	fsHash = PROGCACHE_ProgCache_Hash( fsSource, (int)strlen( fsSource ) );

	fromCache = ( PROGCACHE_ProgCache_On()
				  && PROGCACHE_ProgCache_TryRestore( def->name, defines, vsHash, fsHash,
												&restored, &savedUsec, &restoreUsec ) )
				? qtrue : qfalse;

	if ( fromCache )
	{
		p->program = restored;
	}
	else
	{

		buildT0 = PROGCACHE_ProgCache_Now();

		BE_ZONE( BE_FAM_PROG, BE_PZ_compile, {
			vs = GLES3_CompileShader( GL_VERTEX_SHADER, vsSource,
									  va( "%s.vs defines=0x%06x", def->name, defines ) );
			fs = GLES3_CompileShader( GL_FRAGMENT_SHADER, fsSource,
									  va( "%s.fs defines=0x%06x", def->name, defines ) );
		} );

		BE_ZONE( BE_FAM_PROG, BE_PZ_link, {
			p->program = qglCreateProgram();
			qglAttachShader( p->program, vs );
			qglAttachShader( p->program, fs );

			qglBindAttribLocation( p->program, ATTR_POSITION,  "a_Position" );
			qglBindAttribLocation( p->program, ATTR_NORMAL,    "a_Normal" );
			qglBindAttribLocation( p->program, ATTR_COLOR,     "a_Color" );
			qglBindAttribLocation( p->program, ATTR_TEXCOORD0, "a_TexCoord0" );
			qglBindAttribLocation( p->program, ATTR_TEXCOORD1, "a_TexCoord1" );

			PROGCACHE_ProgCache_HintLink( p->program );

			qglLinkProgram( p->program );
		} );
		qglGetProgramiv( p->program, GL_LINK_STATUS, &status );
		if ( !status )
		{
			log[0] = '\0';
			qglGetProgramInfoLog( p->program, sizeof( log ) - 1, &len, log );
			ri.Error( ERR_DROP, "GLES3 program '%s' (defines 0x%06x) failed to link:\n%s\n",
					  def->name, defines, log );
			return NULL;
		}

		if ( PROGCACHE_ProgCache_On() )
		{
			int		buildNow = PROGCACHE_ProgCache_Now();
			int		buildUsec = buildNow - buildT0;

			if ( buildUsec < 0 )
			{
				buildUsec = 0;
			}
			PROGCACHE_ProgCache_NoteCompiled( def->name, defines, vsHash, fsHash,
										 p->program, (unsigned)buildUsec );
		}

		qglDetachShader( p->program, vs );
		qglDetachShader( p->program, fs );
		qglDeleteShader( vs );
		qglDeleteShader( fs );
	}

	ri.Free( vsSource );
	ri.Free( fsSource );

	GLES3_CacheUniforms( p );
	p->linked = qtrue;

	GLES3_BindProgram( p );
	GLES3_SetUniform1i( p, GLES3_U_TEX0, 0 );
	GLES3_SetUniform1i( p, GLES3_U_TEX1, 1 );
	GLES3_SetUniform1i( p, GLES3_U_WAVETABLE, GLES3_WAVETABLE_UNIT );

	BE_Bump( BE_FAM_PROG, BE_PC_made, 1 );
	if ( !fromCache )
	{
		BE_Name( BE_FAM_PROG, def->name, BE_RootUsec( BE_FAM_PROG ) );
	}
	BE_RootEnd( BE_FAM_PROG );

	gles3NumPrograms++;
	GLES3_ProgHashInsert( defines, gles3NumPrograms - 1 );
	return p;
}

void GLES3_ProgramsInit( void )
{
	if ( s_programsInitialised )
	{
		return;
	}
	gles3NumPrograms = 0;
	Com_Memset( gles3Programs, 0, sizeof( gles3Programs ) );
	GLES3_ProgHashReset();
	glState.currentProgram = 0;
	GLES3_CreateWaveTable();

	{
		progcacheProgCacheKey_t	key;
		const char			*ver, *ren, *vend;
		int					mode = ri.Cvar_Get( "r_progCache", "1", CVAR_ARCHIVE )->integer;

		PROGCACHE_ProgCache_InitEngine( mode );

		if ( PROGCACHE_ProgCache_On() )
		{
			ver  = (const char *)qglGetString( GL_VERSION );
			ren  = (const char *)qglGetString( GL_RENDERER );
			vend = (const char *)qglGetString( GL_VENDOR );

			key.glVersion   = ver ? ver : "";
			key.glRenderer  = ren ? ren : "";
			key.glVendor    = vend ? vend : "";
			key.glesMajor   = (unsigned)( qglesMajorVersion > 0 ? qglesMajorVersion : qglMajorVersion );
			key.glesMinor   = (unsigned)( qglesMajorVersion > 0 ? qglesMinorVersion : qglMinorVersion );
			key.sourceHash  = GLES3_ProgCacheSourceHash();
			key.definesHash = GLES3_ProgCacheDefinesHash();

			PROGCACHE_ProgCache_Load( &key );
			GLES3_ProgCacheInitRow();
		}
	}

	s_programsInitialised = qtrue;
}

void GLES3_ProgramsShutdown( void )
{
	int i;

	if ( PROGCACHE_ProgCache_On() && gles3ProgramsCreatedAfterReg > 0 )
	{
		PROGCACHE_ProgCache_Save();
		GLES3_ProgCacheRow( "shutdown" );
	}

	for ( i = 0; i < gles3NumPrograms; i++ )
	{
		if ( gles3Programs[i].program && qglDeleteProgram )
		{
			qglDeleteProgram( gles3Programs[i].program );
		}
	}
	gles3NumPrograms = 0;
	Com_Memset( gles3Programs, 0, sizeof( gles3Programs ) );
	GLES3_ProgHashReset();
	Com_Memset( s_unifCache, 0, sizeof( s_unifCache ) );
	glState.currentProgram = 0;
	GLES3_DestroyWaveTable();
	PROGCACHE_ProgCache_Shutdown();
	s_programsInitialised = qfalse;
	gles3ProgramsCreated = 0;
	gles3ProgramsCreatedAfterReg = 0;
	s_registrationClosed = qfalse;
}

void GLES3_PrebuildPrograms( void )
{
	int			i, pass, list;
	shaderStage_t *const *stages;
	int			numStages;

	if ( !s_programsInitialised )
	{
		GLES3_ProgramsInit();
	}

	for ( i = 0; i < gles3NumProgramDefs; i++ )
	{
		GLES3_GetProgram( gles3ProgramDefs[i].family );
	}

	for ( i = 0; i < tr.numShaders; i++ )
	{
		shader_t *sh = tr.shaders[i];
		if ( !sh )
		{
			continue;
		}
		for ( list = 0; list < 3; list++ )
		{
			switch ( list )
			{
			case 0:  stages = sh->unfoggedStages;     numStages = sh->numUnfoggedPasses;     break;
			case 1:  stages = sh->fogStages;          numStages = sh->numFogPasses;          break;
			default: stages = sh->globalFogStages;    numStages = sh->numGlobalFogPasses;    break;
			}
			for ( pass = 0; pass < numStages && pass < MAX_SHADER_STAGES; pass++ )
			{
				if ( !stages[pass] )
				{
					continue;
				}
				GLES3_StageProgram( stages[pass], sh, 0 );
				GLES3_StageProgram( stages[pass], sh, GLP_LIGHTMAP_MT );
			}
		}
	}

	if ( PROGCACHE_ProgCache_On() )
	{
		PROGCACHE_ProgCache_Save();
		GLES3_ProgCacheRow( "sweep" );
	}
}

void GLES3_CloseProgramRegistration( void )
{
	s_registrationClosed = qtrue;
}

int GLES3_ProgramsCreatedAfterRegistration( void )
{
	return gles3ProgramsCreatedAfterReg;
}

gles3Program_t *GLES3_GetProgram( unsigned defines )
{
	unsigned	family;
	int			i;

	if ( !s_programsInitialised )
	{
		GLES3_ProgramsInit();
	}
	if ( !s_waveTableReady )
	{
		GLES3_CreateWaveTable();
	}

	i = GLES3_ProgHashFind( defines );
	if ( i >= 0 )
	{
		return &gles3Programs[i];
	}

	family = defines & GLP_FAMILY_MASK;
	for ( i = 0; i < gles3NumProgramDefs; i++ )
	{
		if ( gles3ProgramDefs[i].family == family )
		{
			return GLES3_NewProgram( &gles3ProgramDefs[i], defines );
		}
	}

	ri.Error( ERR_DROP, "GLES3_GetProgram: no program definition for defines 0x%08x\n", defines );
	return NULL;
}

gles3Program_t *GLES3_FamilyProgram( unsigned family )
{
	return GLES3_GetProgram( family );
}

unsigned GLES3_StageDefines_Full( const shaderStage_t *pStage, const shader_t *shader,
								  unsigned bundleCount, unsigned texEnv, unsigned fogStage,
								  unsigned extra )
{
	unsigned	d = 0;
	unsigned	rgbGen, alphaGen;
	unsigned	tc0, tc1;
	int			b;

	(void)shader;
	(void)texEnv;

	if ( !pStage )
	{
		return 0;
	}

	rgbGen = (unsigned)pStage->rgbGen;
	alphaGen = (unsigned)pStage->alphaGen;

	if ( bundleCount == 0 )
	{
		bundleCount = ( pStage->bundle[1].image[0] != NULL ) ? 2u : 1u;
	}
	if ( fogStage == 0 )
	{
		if ( rgbGen == (unsigned)CGEN_FOG )
		{
			fogStage = 1;
		}
		else if ( rgbGen == (unsigned)CGEN_FOG_BLEND )
		{
			fogStage = 2;
		}
	}
	tc0 = (unsigned)pStage->bundle[0].tcGen;
	tc1 = ( bundleCount == 2 ) ? (unsigned)pStage->bundle[1].tcGen : (unsigned)TCGEN_BAD;

	if ( rgbGen != (unsigned)CGEN_IDENTITY
		|| ( alphaGen != (unsigned)AGEN_SKIP && alphaGen != (unsigned)AGEN_IDENTITY ) )
	{
		d |= GLP_RGBAGEN;
	}
	if ( rgbGen == (unsigned)CGEN_DOT || rgbGen == (unsigned)CGEN_ONE_MINUS_DOT
		|| alphaGen == (unsigned)AGEN_DOT || alphaGen == (unsigned)AGEN_ONE_MINUS_DOT
		|| alphaGen == (unsigned)AGEN_DOT_VIEW || alphaGen == (unsigned)AGEN_ONE_MINUS_DOT_VIEW
		|| alphaGen == (unsigned)AGEN_LIGHTING_SPECULAR || alphaGen == (unsigned)AGEN_HEIGHT_FADE )
	{
		d |= GLP_NORMAL;
	}
	/* The whole class, not just the colour arms: four tcGen arms read the
	   surface normal inside the shader (tr_shader.c:1462-1487 sets retail's
	   own needsNormal for exactly them), and AGEN_DOT/ONE_MINUS_DOT_VIEW do
	   too.  Without GLP_NORMAL the shader's `nrm' is the constant vec3(0,0,1)
	   and every such stage samples one texel of its map for the whole surface
	   -- the menu hull's env_diffused stage drew a flat colour. */
	if ( tc0 == (unsigned)TCGEN_ENVIRONMENT_MAPPED || tc0 == (unsigned)TCGEN_ENVIRONMENT_MAPPED2
		|| tc0 == (unsigned)TCGEN_ANGLE_BASED_ENVIRONMENT || tc0 == (unsigned)TCGEN_SUN_REFLECTION
		|| ( bundleCount == 2
			 && ( tc1 == (unsigned)TCGEN_ENVIRONMENT_MAPPED || tc1 == (unsigned)TCGEN_ENVIRONMENT_MAPPED2
				  || tc1 == (unsigned)TCGEN_ANGLE_BASED_ENVIRONMENT || tc1 == (unsigned)TCGEN_SUN_REFLECTION ) ) )
	{
		d |= GLP_NORMAL;
	}
	if ( shader && shader->needsNormal )
	{
		d |= GLP_NORMAL;
	}
	if ( rgbGen == (unsigned)CGEN_WAVEFORM || rgbGen == (unsigned)CGEN_MULTIPLY_BY_WAVEFORM
		|| alphaGen == (unsigned)AGEN_WAVEFORM )
	{
		d |= GLP_WAVE;
	}
	if ( alphaGen == (unsigned)AGEN_LIGHTING_SPECULAR )
	{
		d |= GLP_SPECULAR;
	}
	if ( alphaGen == (unsigned)AGEN_DIST_FADE || alphaGen == (unsigned)AGEN_ONE_MINUS_DIST_FADE
		|| alphaGen == (unsigned)AGEN_TIKI_DIST_FADE || alphaGen == (unsigned)AGEN_ONE_MINUS_TIKI_DIST_FADE
		|| alphaGen == (unsigned)AGEN_HEIGHT_FADE )
	{
		d |= GLP_HEIGHTFOG;
	}
	if ( tc0 != (unsigned)TCGEN_TEXTURE && bundleCount >= 1 )
	{
		d |= GLP_TCGEN0;
	}
	if ( bundleCount == 2 && tc1 != (unsigned)TCGEN_LIGHTMAP && tc1 != (unsigned)TCGEN_TEXTURE )
	{
		d |= GLP_TCGEN1;
	}
	if ( bundleCount == 2 )
	{
		d |= GLP_LIGHTMAP;
	}
	if ( rgbGen == (unsigned)CGEN_ENTITY || rgbGen == (unsigned)CGEN_ONE_MINUS_ENTITY
		|| rgbGen == (unsigned)CGEN_LIGHTING_SPHERICAL || rgbGen == (unsigned)CGEN_STATIC
		|| alphaGen == (unsigned)AGEN_ENTITY || alphaGen == (unsigned)AGEN_ONE_MINUS_ENTITY
		|| alphaGen == (unsigned)AGEN_FORCED_ALPHA )
	{
		d |= GLP_ENTITY_SPACE;
	}

	for ( b = 0; b < (int)bundleCount; b++ )
	{
		int tm;

		if ( !pStage->bundle[b].image[0] )
		{
			continue;
		}
		if ( pStage->bundle[b].numTexMods > 0 )
		{
			d |= ( b == 0 ) ? GLP_TCMOD0 : GLP_TCMOD1;
		}
		for ( tm = 0; tm < pStage->bundle[b].numTexMods; tm++ )
		{
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_TURBULENT:
			case TMOD_STRETCH:
			case TMOD_WAVETRANS:
			case TMOD_WAVETRANT:
			case TMOD_BULGETRANS:
				d |= GLP_WAVE;
				break;
			default:
				break;
			}
		}
	}

	if ( ( (unsigned)pStage->stateBits & GLS_ATEST_BITS ) != 0 )
	{
		d |= GLP_ALPHATEST;
	}
	if ( fogStage )
	{
		d |= GLP_FOG;
	}

	return d | extra;
}

unsigned GLES3_StageDefines( const shaderStage_t *pStage, const shader_t *shader )
{
	return GLES3_StageDefines_Full( pStage, shader, 0, 0, 0, 0 );
}

unsigned GLES3_TexEnvForStage( const shaderStage_t *pStage, const shader_t *shader )
{
	unsigned	d = 0;

	(void)shader;
	if ( !pStage )
	{
		return 0;
	}

	if ( pStage->bundle[1].image[0] )
	{
		if ( pStage->multitextureEnv == GL_ADD )
		{
			d |= GLP_TEXENV_ADD;
		}
		if ( pStage->stateBits & GLS_MULTITEXTURE_ENV )
		{
			d |= GLP_TEXENV_COMBINE4;
		}
		else if ( pStage->stateBits & GLS_MULTITEXTURE )
		{
			d |= GLP_TEXENV_INTERPOLATE;
		}
	}
	return d;
}

qboolean GLES3_StageNeedsCPUColor( const shaderStage_t *pStage )
{
	if ( !pStage )
	{
		return qfalse;
	}
	switch ( pStage->rgbGen )
	{
	case CGEN_LIGHTING_SPHERICAL:
	case CGEN_STATIC:
		return qtrue;
	default:
		break;
	}
	switch ( pStage->alphaGen )
	{
	case AGEN_NOISE:
	case AGEN_DIST_FADE:
	case AGEN_ONE_MINUS_DIST_FADE:
	case AGEN_TIKI_DIST_FADE:
	case AGEN_ONE_MINUS_TIKI_DIST_FADE:
		return qtrue;
	default:
		break;
	}
	return qfalse;
}

qboolean GLES3_StageNeedsCPUTexCoords( const shaderStage_t *pStage )
{
	int b, tm;

	if ( !pStage )
	{
		return qfalse;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
	{
		for ( tm = 0; tm < pStage->bundle[b].numTexMods; tm++ )
		{
			if ( pStage->bundle[b].texMods[tm].type == TMOD_MACRO )
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

gles3Program_t *GLES3_StageProgram( const shaderStage_t *pStage, const shader_t *shader, unsigned extraDefines )
{
	unsigned	defines;

	defines = GLES3_StageDefines( pStage, shader );
	defines |= GLES3_TexEnvForStage( pStage, shader );
	defines |= extraDefines;
	return GLES3_GetProgram( defines );
}

static qboolean GLES3_UniformCached( gles3Program_t *p, int location, const void *v, int n )
{
	gles3UnifCache_t	*e;
	unsigned			 slot;

	/* a value set on a program that is not bound never reaches it, so it is not remembered */
	if ( glState.currentProgram != p->program ) {
		return qfalse;
	}

	slot = ( (unsigned)p->program * 31u + (unsigned)location ) & ( GLES3_UNIF_CACHE_SIZE - 1 );
	e = &s_unifCache[slot];

	if ( n > GLES3_UNIF_CACHE_VALS ) {
		e->program = 0;
		return qfalse;
	}
	if ( e->used && e->program == p->program && e->location == location
		 && e->count == (short)n && memcmp( e->val, v, (size_t)n * sizeof( float ) ) == 0 ) {
		return qtrue;
	}

	e->used = 1;
	e->program = p->program;
	e->location = location;
	e->count = (short)n;
	memcpy( e->val, v, (size_t)n * sizeof( float ) );
	return qfalse;
}

void GLES3_SetUniform1i( gles3Program_t *p, gles3Uniform_t u, int v )
{
	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], &v, 1 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform1i( p->uniforms[u], v );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniform1f( gles3Program_t *p, gles3Uniform_t u, float v )
{
	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], &v, 1 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform1f( p->uniforms[u], v );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniform2f( gles3Program_t *p, gles3Uniform_t u, float x, float y )
{
	float	v[2];

	v[0] = x;
	v[1] = y;

	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], v, 2 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform2f( p->uniforms[u], x, y );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniform3f( gles3Program_t *p, gles3Uniform_t u, float x, float y, float z )
{
	float	v[3];

	v[0] = x;
	v[1] = y;
	v[2] = z;

	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], v, 3 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform3f( p->uniforms[u], x, y, z );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniform4f( gles3Program_t *p, gles3Uniform_t u, float x, float y, float z, float w )
{
	float	v[4];

	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = w;

	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], v, 4 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform4f( p->uniforms[u], x, y, z, w );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniformMat4( gles3Program_t *p, gles3Uniform_t u, const float *m )
{
	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], m, 16 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniformMatrix4fv( p->uniforms[u], 1, GL_FALSE, m );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniform3fv( gles3Program_t *p, gles3Uniform_t u, const float *v )
{
	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], v, 3 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform3fv( p->uniforms[u], 1, v );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SetUniform4fv( gles3Program_t *p, gles3Uniform_t u, const float *v )
{
	if ( p && p->uniforms[u] != -1 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], v, 4 ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform4fv( p->uniforms[u], 1, v );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

static void GLES3_SetUniform4fvN( gles3Program_t *p, gles3Uniform_t u, const float *v, int count )
{
	if ( p && p->uniforms[u] != -1 && count > 0 )
	{
		if ( GLES3_UniformCached( p, p->uniforms[u], v, count ) ) {
			BE_Bump( BE_FAM_DS, BE_DSC_unifskip, 1 );
		} else {
			qglUniform4fv( p->uniforms[u], count, v );
			BE_Bump( BE_FAM_DS, BE_DSC_unifs, 1 );
		}
	}
}

void GLES3_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}
	if ( unit < 0 || unit >= NUM_TEXTURE_BUNDLES )
	{
		ri.Error( ERR_DROP, "GLES3_SelectTexture: unit = %i", unit );
		return;
	}
	qglActiveTexture( GL_TEXTURE0 + unit );
	glState.currenttmu = unit;
}

void GLES3_BindAnimatedImage( const textureBundle_t *bundle, int unit )
{
	int index;

	GLES3_SelectTexture( unit );

	if ( bundle->numImageAnimations <= 1 )
	{
		GL_Bind( bundle->image[0] );
		return;
	}

	index = myftol( ( tess.shaderTime + bundle->imageAnimationPhase )
					* bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 )
	{
		index = 0;		
	}
	if ( !( bundle->flags & BUNDLE_ANIMATE_ONCE ) )
	{
		index %= bundle->numImageAnimations;
	}
	else if ( index >= bundle->numImageAnimations )
	{
		index = bundle->numImageAnimations - 1;
	}

	GL_Bind( bundle->image[index] );
}

void GLES3_ApplyStateBits( unsigned long stateBits )
{
	GL_State( stateBits );
	s_currentAlphaTest = (unsigned)( ( stateBits & GLS_ATEST_BITS ) >> 28 );

	GLES3_RefreshClipPlane();
}

unsigned GLES3_CurrentAlphaTest( void )
{
	return s_currentAlphaTest;
}

static void GLES3_Mat4Mul( float *out, const float *a, const float *b )
{
	int c, r, k;

	for ( c = 0; c < 4; c++ )
	{
		for ( r = 0; r < 4; r++ )
		{
			float s = 0.0f;

			for ( k = 0; k < 4; k++ )
			{
				s += a[k * 4 + r] * b[c * 4 + k];
			}
			out[c * 4 + r] = s;
		}
	}
}

static gles3Program_t	*s_clipProgram;
static qboolean			s_clipValid;
static qboolean			s_clipEnabled;
static vec4_t			s_clipPlane;

void GLES3_BindProgram( gles3Program_t *p )
{
	if ( p )
	{
		if ( glState.currentProgram != p->program )
		{
			qglUseProgram( p->program );
			glState.currentProgram = p->program;
			BE_Bump( BE_FAM_DS, BE_DSC_binds, 1 );
		}
	}
}

static void GLES3_UploadClipPlane( gles3Program_t *p )
{
	vec4_t		plane;
	qboolean	enabled = RB_ClipPlane_Get( plane );

	if ( enabled )
	{
		GLES3_SetUniform4fv( p, GLES3_U_CLIPPLANE, plane );
		GLES3_SetUniform1i( p, GLES3_U_CLIPPLANEENABLE, 1 );
	}
	else
	{
		GLES3_SetUniform4f( p, GLES3_U_CLIPPLANE, 0.0f, 0.0f, 0.0f, 1.0f );
		GLES3_SetUniform1i( p, GLES3_U_CLIPPLANEENABLE, 0 );
	}

	s_clipProgram = p;
	s_clipValid = qtrue;
	s_clipEnabled = enabled;
	s_clipPlane[0] = plane[0];
	s_clipPlane[1] = plane[1];
	s_clipPlane[2] = plane[2];
	s_clipPlane[3] = plane[3];
}

void GLES3_RefreshClipPlane( void )
{
	vec4_t		plane;
	qboolean	enabled;
	GLint		current = 0;

	if ( !s_clipProgram )
	{
		return;
	}

	enabled = RB_ClipPlane_Get( plane );
	if ( s_clipValid && enabled == s_clipEnabled
		 && ( !enabled || VectorCompare( plane, s_clipPlane ) ) )
	{
		return;		
	}

	qglGetIntegerv( GL_CURRENT_PROGRAM, &current );
	if ( (GLuint)current != s_clipProgram->program )
	{
		return;
	}

	GLES3_UploadClipPlane( s_clipProgram );
}

void GLES3_SetupViewUniforms( gles3Program_t *p )
{
	float	mvp[16];
	vec3_t	entityOrigin;
	vec3_t	entityAxis[3];
	float	entityColor[4];
	const float *mv = RB_GetModelViewMatrix();
	const float *proj = RB_GetProjectionMatrix();

	GLES3_Mat4Mul( mvp, proj, mv );

	GLES3_SetUniformMat4( p, GLES3_U_MVP, mvp );
	GLES3_SetUniformMat4( p, GLES3_U_MODELMATRIX, mv );
	GLES3_SetUniform3fv( p, GLES3_U_VIEWORIGIN, backEnd.viewParms.ori.origin );
	GLES3_SetUniform3fv( p, GLES3_U_VIEWAXIS0, tr.refdef.viewaxis[0] );
	GLES3_SetUniform3f( p, GLES3_U_LOCALVIEWORIGIN,
						tr.refdef.vieworg[0], tr.refdef.vieworg[1], tr.refdef.vieworg[2] );

	if ( backEnd.currentEntity )
	{
		VectorCopy( backEnd.currentEntity->e.origin, entityOrigin );
		VectorCopy( backEnd.currentEntity->e.axis[0], entityAxis[0] );
		VectorCopy( backEnd.currentEntity->e.axis[1], entityAxis[1] );
		VectorCopy( backEnd.currentEntity->e.axis[2], entityAxis[2] );
		entityColor[0] = backEnd.currentEntity->e.shaderRGBA[0] / 255.0f;
		entityColor[1] = backEnd.currentEntity->e.shaderRGBA[1] / 255.0f;
		entityColor[2] = backEnd.currentEntity->e.shaderRGBA[2] / 255.0f;
		entityColor[3] = backEnd.currentEntity->e.shaderRGBA[3] / 255.0f;
		GLES3_SetUniform4f( p, GLES3_U_ENTITYTEXCOORD,
							backEnd.currentEntity->e.shaderTexCoord[0],
							backEnd.currentEntity->e.shaderTexCoord[1], 0.0f, 0.0f );
	}
	else
	{
		VectorCopy( backEnd.viewParms.ori.origin, entityOrigin );
		VectorCopy( backEnd.viewParms.ori.axis[0], entityAxis[0] );
		VectorCopy( backEnd.viewParms.ori.axis[1], entityAxis[1] );
		VectorCopy( backEnd.viewParms.ori.axis[2], entityAxis[2] );
		entityColor[0] = entityColor[1] = entityColor[2] = entityColor[3] = 1.0f;
		GLES3_SetUniform4f( p, GLES3_U_ENTITYTEXCOORD, 0.0f, 0.0f, 0.0f, 0.0f );
	}

	GLES3_SetUniform3fv( p, GLES3_U_ENTITYORIGIN, entityOrigin );
	GLES3_SetUniform3fv( p, GLES3_U_ENTITYAXIS, entityAxis[0] );
	GLES3_SetUniform4fv( p, GLES3_U_ENTITYCOLOR, entityColor );
	GLES3_SetUniform1f( p, GLES3_U_IDENTITYLIGHT, tr.identityLight );
	GLES3_SetUniform1f( p, GLES3_U_IDENTITYLIGHTBYTE, (float)tr.identityLightByte );
	GLES3_SetUniform1f( p, GLES3_U_ENTITYFADE, tess.entityFade );
	GLES3_SetUniform1f( p, GLES3_U_SHADERTIME, tess.shaderTime );
	GLES3_SetUniform3fv( p, GLES3_U_SUNDIRECTION, tr.sunDirection );
	GLES3_SetUniform4f( p, GLES3_U_VERTCOLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	GLES3_UploadClipPlane( p );
}

void GLES3_SetupStageUniforms( gles3Program_t *p, const shaderStage_t *pStage, const shader_t *shader )
{
	float	baseColor[4];
	qboolean	cpuColor;
	qboolean	cpuTexCoords;
	int		b;

	GLES3_SetupViewUniforms( p );

	if ( !pStage )
	{
		return;
	}

	cpuColor = GLES3_StageNeedsCPUColor( pStage );
	cpuTexCoords = GLES3_StageNeedsCPUTexCoords( pStage );

	/* In 2D the quad already carries Draw_SetColor in a_Color (tr_draw.c writes
	   it per vertex), so u_BaseColor stays 1: multiplying it in again squares
	   the colour and the alpha, which is the black-button / half-alpha class.
	   In 3D a_Color is the tess colour array, which a constant-colour stage
	   never fills, so the uniform still has to carry it. */
	if ( backEnd.in2D )
	{
		baseColor[0] = baseColor[1] = baseColor[2] = baseColor[3] = 1.0f;
	}
	else
	{
		if ( pStage->rgbGen == CGEN_GLOBAL_COLOR )
		{
			baseColor[0] = backEnd.color2D[0] / 255.0f;
			baseColor[1] = backEnd.color2D[1] / 255.0f;
			baseColor[2] = backEnd.color2D[2] / 255.0f;
		}
		else
		{
			baseColor[0] = pStage->colorConst[0] / 255.0f;
			baseColor[1] = pStage->colorConst[1] / 255.0f;
			baseColor[2] = pStage->colorConst[2] / 255.0f;
		}
		if ( pStage->alphaGen == AGEN_GLOBAL_ALPHA )
		{
			baseColor[3] = backEnd.color2D[3] / 255.0f;
		}
		else
		{
			baseColor[3] = pStage->colorConst[3] / 255.0f;
		}
	}
	GLES3_SetUniform4fv( p, GLES3_U_BASECOLOR, baseColor );

	/* In 2D every quad already carries Draw_SetColor per vertex, and u_BaseColor
	   is pinned to 1 above -- so the two arms that read that uniform read 1 and
	   the quad draws in the atlas's own colour, which for a font page is white.
	   Those two arms read the vertex instead, exactly as the CPU-colour arm does;
	   every other gen keeps its own meaning.  In 3D u_BaseColor carries the
	   global colour, exactly as before. */
	if ( backEnd.in2D && !cpuColor )
	{
		GLES3_SetUniform1i( p, GLES3_U_COLORGEN,
							pStage->rgbGen == CGEN_GLOBAL_COLOR ? (int)CGEN_EXACT_VERTEX
																: (int)pStage->rgbGen );
		GLES3_SetUniform1i( p, GLES3_U_ALPHAGEN,
							pStage->alphaGen == AGEN_GLOBAL_ALPHA ? (int)AGEN_SKIP
																  : (int)pStage->alphaGen );
	}
	else
	{
		GLES3_SetUniform1i( p, GLES3_U_COLORGEN, cpuColor ? (int)CGEN_EXACT_VERTEX : (int)pStage->rgbGen );
		GLES3_SetUniform1i( p, GLES3_U_ALPHAGEN, cpuColor ? (int)AGEN_SKIP : (int)pStage->alphaGen );
	}

	GLES3_SetUniform1f( p, GLES3_U_ALPHAMIN, pStage->alphaMin );
	GLES3_SetUniform1f( p, GLES3_U_ALPHAMAX, pStage->alphaMax );
	GLES3_SetUniform1f( p, GLES3_U_ALPHACONST, pStage->alphaConst / 255.0f );
	GLES3_SetUniform1f( p, GLES3_U_ALPHACONSTMIN, pStage->alphaConstMin / 255.0f );

	GLES3_SetUniform4f( p, GLES3_U_WAVERGB,
						(float)pStage->rgbWave.func, pStage->rgbWave.base,
						pStage->rgbWave.amplitude, pStage->rgbWave.phase );
	GLES3_SetUniform1f( p, GLES3_U_FREQRGB, pStage->rgbWave.frequency );
	GLES3_SetUniform4f( p, GLES3_U_WAVEALPHA,
						(float)pStage->alphaWave.func, pStage->alphaWave.base,
						pStage->alphaWave.amplitude, pStage->alphaWave.phase );
	GLES3_SetUniform1f( p, GLES3_U_FREQALPHA, pStage->alphaWave.frequency );

	GLES3_SetUniform1f( p, GLES3_U_PORTALRANGE, shader ? shader->portalRange : 0.0f );
	GLES3_SetUniform3fv( p, GLES3_U_SPECORIGIN, pStage->specOrigin );

	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
	{
		const textureBundle_t	*bundle = &pStage->bundle[b];
		float			packed[TR_MAX_TEXMODS * 4][4];
		int				tm;
		int				numMods = 0;

		if ( b == 0 )
		{
			GLES3_SetUniform1i( p, GLES3_U_TCGEN0,
								cpuTexCoords ? (int)TCGEN_TEXTURE : (int)bundle->tcGen );
			GLES3_SetUniform3fv( p, GLES3_U_TCGEN0VEC0, bundle->tcGenVectors[0] );
			GLES3_SetUniform3fv( p, GLES3_U_TCGEN0VEC1, bundle->tcGenVectors[1] );
		}
		else
		{
			GLES3_SetUniform1i( p, GLES3_U_TCGEN1,
								cpuTexCoords ? (int)TCGEN_TEXTURE : (int)bundle->tcGen );
			GLES3_SetUniform3fv( p, GLES3_U_TCGEN1VEC0, bundle->tcGenVectors[0] );
			GLES3_SetUniform3fv( p, GLES3_U_TCGEN1VEC1, bundle->tcGenVectors[1] );
		}

		Com_Memset( packed, 0, sizeof( packed ) );

		if ( !cpuTexCoords )
		{
			for ( tm = 0; tm < bundle->numTexMods && tm < TR_MAX_TEXMODS; tm++ )
			{
				texModInfo_t	*tmi = &bundle->texMods[tm];
				float			*p0 = packed[tm * 4 + 0];
				float			*p1 = packed[tm * 4 + 1];

				p0[0] = (float)tmi->type;
				switch ( tmi->type )
				{
				case TMOD_TRANSFORM:			 
					p0[1] = tmi->matrix[0][0];
					p0[2] = tmi->matrix[1][0];
					p0[3] = tmi->translate[0];
					p1[0] = tmi->matrix[0][1];
					p1[1] = tmi->matrix[1][1];
					p1[2] = tmi->translate[1];
					break;
				case TMOD_TURBULENT:			 
					p0[3] = tmi->wave.amplitude;
					p1[0] = tmi->wave.phase;
					p1[1] = tmi->wave.frequency;
					break;
				case TMOD_SCROLL:				 
					p0[1] = tmi->scroll[0];
					p0[2] = tmi->scroll[1];
					break;
				case TMOD_SCALE:				 
					p0[1] = tmi->scale[0];
					p0[2] = tmi->scale[1];
					break;
				case TMOD_STRETCH:				 
					p0[1] = (float)tmi->wave.func;
					p0[2] = tmi->wave.base;
					p0[3] = tmi->wave.amplitude;
					p1[0] = tmi->wave.phase;
					p1[1] = tmi->wave.frequency;
					break;
				case TMOD_ROTATE:				 
					if ( tmi->rotateSpeed == 1234567.0f )
					{
						p0[1] = backEnd.currentEntity
								? -(float)backEnd.currentEntity->e.tag_num : 0.0f;
					}
					else
					{
						p0[1] = tmi->rotateSpeed;
					}
					p0[2] = tmi->rotateCoef;
					p0[3] = tmi->rotateStart;
					break;
				case TMOD_OFFSET:				 
					if ( tmi->scroll[0] != 1234567.0f )
					{
						p0[1] = tmi->scroll[0];
						p0[2] = tmi->scroll[1];
					}
					else
					{
						p0[1] = backEnd.currentEntity ? backEnd.currentEntity->e.tag_num : 0.0f;
						p0[2] = backEnd.currentEntity ? backEnd.currentEntity->e.skin_num : 0.0f;
					}
					break;
				case TMOD_PARALLAX:				 
					p0[1] = tmi->rate[0];
					p0[2] = tmi->rate[1];
					break;
				case TMOD_WAVETRANS:			 
				case TMOD_WAVETRANT:			 
					p0[1] = (float)tmi->wave.func;
					p0[2] = tmi->wave.base;
					p0[3] = tmi->wave.amplitude;
					p1[0] = tmi->wave.phase;
					p1[1] = tmi->wave.frequency;
					break;
				case TMOD_BULGETRANS:			 
				case TMOD_MACRO:				
				case TMOD_NOISE:				
				case TMOD_NONE:
				default:
					break;
				}
				numMods = tm + 1;
			}
		}

		if ( b == 0 )
		{
			GLES3_SetUniform1i( p, GLES3_U_NUMTCMOD0, numMods );
			GLES3_SetUniform4fvN( p, GLES3_U_TCMOD0, packed[0], TR_MAX_TEXMODS * 4 );
		}
		else
		{
			GLES3_SetUniform1i( p, GLES3_U_NUMTCMOD1, numMods );
			GLES3_SetUniform4fvN( p, GLES3_U_TCMOD1, packed[0], TR_MAX_TEXMODS * 4 );
		}
	}

	GLES3_SetUniform1i( p, GLES3_U_TEXENV0, 1 );	
	GLES3_SetUniform1i( p, GLES3_U_TEXENV1, 1 );
	if ( pStage->bundle[1].image[0] )
	{
		unsigned env = GLES3_TexEnvForStage( pStage, shader );

		if ( env & GLP_TEXENV_ADD )
		{
			GLES3_SetUniform1i( p, GLES3_U_TEXENV1, 2 );		 
		}
		if ( env & GLP_TEXENV_INTERPOLATE )
		{
			GLES3_SetUniform1i( p, GLES3_U_TEXENV1, 4 );		 
		}
		if ( env & GLP_TEXENV_COMBINE4 )
		{
			GLES3_SetUniform1i( p, GLES3_U_TEXENV1, 5 );		 
		}
	}
	if ( r_lightmap->integer && pStage->bundle[1].isLightmap )
	{
		GLES3_SetUniform1i( p, GLES3_U_TEXENV1, 3 );
	}

	GLES3_SetUniform1i( p, GLES3_U_ALPHATEST, (int)s_currentAlphaTest );
	GLES3_SetUniform4f( p, GLES3_U_TEX1COLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	GLES3_SetUniform1i( p, GLES3_U_ACFF, cpuColor ? 0 : (int)pStage->acff );
}

void GLES3_SetupFogUniforms( gles3Program_t *p, int fogNum, qboolean fogDensityArm )
{
	float	color[4];
	float	fixColor[3];
	int		densityArm = 0;

	GLES3_SetUniform1i( p, GLES3_U_FOGNUM, fogNum );

	fixColor[0] = backEnd.viewParms.farplane_color[0] * tr.identityLight;
	fixColor[1] = backEnd.viewParms.farplane_color[1] * tr.identityLight;
	fixColor[2] = backEnd.viewParms.farplane_color[2] * tr.identityLight;
	GLES3_SetUniform1f( p, GLES3_U_FOGSTART, backEnd.viewParms.farplane_bias );
	GLES3_SetUniform1f( p, GLES3_U_FOGEND, backEnd.viewParms.farplane_distance );
	GLES3_SetUniform4f( p, GLES3_U_FOGFIXCOLOR, fixColor[0], fixColor[1], fixColor[2], 1.0f );
	GLES3_SetUniform1i( p, GLES3_U_FOGMODE,
						( glState.externalSetState & GLS_FOG ) ? 2 : 0 );

	if ( fogNum == 0 || !tr.world )
	{
		GLES3_SetUniform4f( p, GLES3_U_FOGCOLOR, 0.0f, 0.0f, 0.0f, 1.0f );
		GLES3_SetUniform1f( p, GLES3_U_FOGTCSCALE, 0.0f );
		GLES3_SetUniform1f( p, GLES3_U_FOGSCONSTANT, 0.0f );
		GLES3_SetUniform3f( p, GLES3_U_FOGSVECTOR, 0.0f, 0.0f, 0.0f );
		GLES3_SetUniform1f( p, GLES3_U_FOGTCONSTANT, 1.0f );
		GLES3_SetUniform3f( p, GLES3_U_FOGTVECTOR, 0.0f, 0.0f, 0.0f );
		GLES3_SetUniform1i( p, GLES3_U_FOGVIEWOUTSIDE, 0 );
		GLES3_SetUniform1f( p, GLES3_U_FOGOOLEN, 0.0f );
		GLES3_SetUniform1i( p, GLES3_U_FOGDENSITYARM, 0 );
		return;
	}

	if ( fogNum == 9999 )
	{
		unsigned colorInt = (unsigned)tr.viewParms.fogColorInt;

		Com_Memcpy( color, &colorInt, sizeof( colorInt ) );
		GLES3_SetUniform4fv( p, GLES3_U_FOGCOLOR, color );
		GLES3_SetUniform1f( p, GLES3_U_FOGOOLEN, tr.viewParms.fog.oolen );
		GLES3_SetUniform1i( p, GLES3_U_FOGDENSITYARM, fogDensityArm ? 1 : 0 );
		return;
	}

	{
		fog_t	*fog = &tr.world->fogs[fogNum];
		float	tcScale;
		float	sVector[3], tVector[3];
		float	sConstant, tConstant, viewOutside;
		vec3_t	delta;
		unsigned colorInt = fog->colorInt;

		Com_Memcpy( color, &colorInt, sizeof( colorInt ) );
		GLES3_SetUniform4fv( p, GLES3_U_FOGCOLOR, color );

		if ( fog->parms.wave.func )
		{
			float wave = EvalWaveForm( &fog->parms.wave );

			if ( wave < 1.0f )
			{
				wave = 1.0f;
			}
			fog->tcScale = 8.0f * ( 1.0f / wave );
		}
		tcScale = fog->tcScale;
		GLES3_SetUniform1f( p, GLES3_U_FOGTCSCALE, tcScale );

		VectorSubtract( backEnd.ori.origin, backEnd.viewParms.ori.origin, delta );
		sConstant = DotProduct( delta, backEnd.viewParms.ori.axis[0] ) * tcScale;
		sVector[0] = -backEnd.ori.modelMatrix[3] * tcScale;
		sVector[1] = -backEnd.ori.modelMatrix[7] * tcScale;
		sVector[2] = -backEnd.ori.modelMatrix[11] * tcScale;

		if ( fog->hasSurface )
		{
			tVector[0] = DotProduct( fog->surface, backEnd.ori.axis[0] );
			tVector[1] = DotProduct( fog->surface, backEnd.ori.axis[1] );
			tVector[2] = DotProduct( fog->surface, backEnd.ori.axis[2] );
			tConstant = -fog->surface[3]
				+ DotProduct( fog->surface, backEnd.ori.origin )
				+ DotProduct( backEnd.ori.viewOrigin, tVector );
		}
		else
		{
			tConstant = 1.0f;
			tVector[0] = tVector[1] = tVector[2] = 0.0f;
		}
		viewOutside = ( tConstant < 0.0f ) ? 1.0f : 0.0f;

		GLES3_SetUniform1f( p, GLES3_U_FOGSCONSTANT, sConstant );
		GLES3_SetUniform3fv( p, GLES3_U_FOGSVECTOR, sVector );
		GLES3_SetUniform1f( p, GLES3_U_FOGTCONSTANT, tConstant );
		GLES3_SetUniform3fv( p, GLES3_U_FOGTVECTOR, tVector );
		GLES3_SetUniform1i( p, GLES3_U_FOGVIEWOUTSIDE, (int)viewOutside );

		{
			float	tVector2[3], planeConst, dView;
			float	*viewOrigin = backEnd.ori.viewOrigin;

			tVector2[0] = DotProduct( fog->surface, backEnd.ori.axis[0] ) * tcScale;
			tVector2[1] = DotProduct( fog->surface, backEnd.ori.axis[1] ) * tcScale;
			tVector2[2] = DotProduct( fog->surface, backEnd.ori.axis[2] ) * tcScale;
			planeConst = ( fog->surface[3]
						   - DotProduct( fog->surface, backEnd.ori.origin ) ) * tcScale;
			dView = ( ( viewOrigin[0] * tVector2[0] + viewOrigin[1] * tVector2[1] )
					  + viewOrigin[2] * tVector2[2] ) - planeConst;

			GLES3_SetUniform3fv( p, GLES3_U_FOGDENSITYTVECTOR, tVector2 );
			GLES3_SetUniform1f( p, GLES3_U_FOGPLANECONST, planeConst );
			GLES3_SetUniform1f( p, GLES3_U_FOGDVIEW, dView );

			if ( fogDensityArm )
			{
				densityArm = ( dView >= 0.0f ) ? 2 : 3;
			}
		}
		GLES3_SetUniform1f( p, GLES3_U_FOGOOLEN, tr.viewParms.fog.oolen );
	}

	GLES3_SetUniform1i( p, GLES3_U_FOGDENSITYARM, densityArm );
}

#define GLES3_CENSUS_MAXKEYS      2048	
#define GLES3_CENSUS_KEYLEN       320	
#define GLES3_CENSUS_NEW_PER_FRAME 16	
#define GLES3_CENSUS_SUMMARY_FRAMES 300	
#define GLES3_CENSUS_HASHSIZE     8192	

typedef struct {
	const char	*shader;
	const char	*prog;
	unsigned	 defines;
	const char	*img0;
	const char	*img1;
	int			 rgbGen;
	int			 alphaGen;
	int			 tcGen0;
	int			 tcGen1;
	unsigned	 stateBits;
	int			 cpuColor;
	int			 cpuTexCoords;
	unsigned	 vc0;
} gles3DrawKey_t;

static void GLES3_FormatDrawKey( char *out, int outLen, const gles3DrawKey_t *k ) {
	Com_sprintf( out, outLen,
		"shader=%s prog=%s def=0x%06x img0=%s img1=%s rgb=%d alpha=%d tc=%d/%d state=0x%08x cpuC=%d cpuTC=%d vc0=%08x",
		k->shader, k->prog, k->defines, k->img0, k->img1,
		k->rgbGen, k->alphaGen, k->tcGen0, k->tcGen1,
		k->stateBits, k->cpuColor, k->cpuTexCoords, k->vc0 );
}

static unsigned GLES3_CensusHashKey( const gles3DrawKey_t *k ) {
	unsigned	h = 2166136261u;
	int			i;
	const char	*parts[4];

	parts[0] = k->shader;
	parts[1] = k->prog;
	parts[2] = k->img0;
	parts[3] = k->img1;

	for ( i = 0; i < 4; i++ ) {
		const unsigned char	*p = ( const unsigned char * )parts[i];
		while ( *p ) {
			h ^= *p++;
			h *= 16777619u;
		}
		h ^= 0x1fu;
		h *= 16777619u;
	}

	{
		const unsigned	words[6] = { k->defines, k->stateBits, k->vc0,
									   (unsigned)k->rgbGen, (unsigned)k->alphaGen,
									   (unsigned)( k->tcGen0 | ( k->tcGen1 << 8 )
												   | ( k->cpuColor << 16 )
												   | ( k->cpuTexCoords << 17 ) ) };
		for ( i = 0; i < 6; i++ ) {
			h ^= words[i];
			h *= 16777619u;
		}
	}
	return h;
}

typedef struct {
	char		*key;
	unsigned	count;
	unsigned	hash;
	gles3DrawKey_t k;
} gles3CensusKey_t;

/* A key's strings are raw pointers it does not own; the slot hash is the identity. */
static qboolean GLES3_CensusKeyEqual( const gles3CensusKey_t *a, const gles3DrawKey_t *b,
									  unsigned hash ) {
	return ( a->hash == hash
		  && a->k.defines == b->defines && a->k.stateBits == b->stateBits
		  && a->k.vc0 == b->vc0 && a->k.rgbGen == b->rgbGen && a->k.alphaGen == b->alphaGen
		  && a->k.tcGen0 == b->tcGen0 && a->k.tcGen1 == b->tcGen1
		  && a->k.cpuColor == b->cpuColor && a->k.cpuTexCoords == b->cpuTexCoords ) ? qtrue : qfalse;
}

typedef struct {
	gles3CensusKey_t	*keys;
	int				*hash;
	int				 numKeys;
	int				 printedThisFrame;
	int				 lastFrame;
	int				 lastSummaryFrame;
	unsigned		 totalDraws;
	unsigned		 budgetSuppressed;
	unsigned		 capacityDropped;
	unsigned		 truncatedKeys;
	qboolean		 noticeDone;
	unsigned		 generation;
} gles3Census_t;

static gles3Census_t	s_gles3Census3D;
static gles3Census_t	s_gles3Census2D;
static unsigned			s_censusGeneration;

/* Bounds the cache to one registration's names; equality no longer reads them. */
void GLES3_CensusInvalidate( void ) {
	s_censusGeneration++;
}

static void GLES3_CensusInit( gles3Census_t *c ) {
	if ( c->keys ) {
		int		i;

		if ( c->generation == s_censusGeneration ) {
			return;
		}
		for ( i = 0; i < c->numKeys; i++ ) {
			ri.Free( c->keys[i].key );
		}
		ri.Free( c->keys );
		ri.Free( c->hash );
		c->keys = NULL;
	}
	c->keys = (gles3CensusKey_t *)ri.Malloc( sizeof( gles3CensusKey_t ) * GLES3_CENSUS_MAXKEYS );
	c->hash = (int *)ri.Malloc( sizeof( int ) * GLES3_CENSUS_HASHSIZE );
	Com_Memset( c->hash, 0, sizeof( int ) * GLES3_CENSUS_HASHSIZE );
	c->generation = s_censusGeneration;
	c->numKeys = 0;
	c->printedThisFrame = 0;
	c->lastFrame = -1;
	c->lastSummaryFrame = -1;
	c->totalDraws = 0;
	c->budgetSuppressed = 0;
	c->capacityDropped = 0;
	c->truncatedKeys = 0;
	c->noticeDone = qfalse;
}

static const char *GLES3_CensusShaderName( const char *key ) {
	static char	name[80];
	const char	*p = key;
	int			n = 0;

	if ( Q_strncmp( p, "shader=", 7 ) ) {
		return "?";
	}
	p += 7;
	while ( *p && *p != ' ' && n < (int)sizeof( name ) - 1 ) {
		name[n++] = *p++;
	}
	name[n] = '\0';
	return name;
}

static void GLES3_CensusSummary( gles3Census_t *c, qboolean in2D ) {
	char		 top[10][80];
	unsigned	 topCount[10];
	int			 i, k;

	if ( !R_ProbeStatsOn() ) {
		return;
	}

	for ( i = 0; i < 10; i++ ) {
		top[i][0] = '\0';
		topCount[i] = 0;
	}
	for ( i = 0; i < c->numKeys; i++ ) {
		unsigned	v = c->keys[i].count;
		for ( k = 0; k < 10; k++ ) {
			if ( v > topCount[k] ) {
				int j;
				for ( j = 9; j > k; j-- ) {
					Q_strncpyz( top[j], top[j - 1], sizeof( top[j] ) );
					topCount[j] = topCount[j - 1];
				}
				Q_strncpyz( top[k], GLES3_CensusShaderName( c->keys[i].key ), sizeof( top[k] ) );
				topCount[k] = v;
				break;
			}
		}
	}

	ri.Printf( PRINT_ALL,
		"[GLES3:drawsum] %s frame=%d distinct=%d total=%u budgetSuppressed=%u "
		"capacityDropped=%u truncatedKeys=%u\n",
		in2D ? "2D" : "3D", tr.frameCount, c->numKeys, c->totalDraws,
		c->budgetSuppressed, c->capacityDropped, c->truncatedKeys );
	ri.Printf( PRINT_ALL,
		"[GLES3:drawsum] %s top:", in2D ? "2D" : "3D" );
	for ( k = 0; k < 10 && top[k][0]; k++ ) {
		ri.Printf( PRINT_ALL, " %s=%u", top[k], topCount[k] );
	}
	ri.Printf( PRINT_ALL, "\n" );
}

static void GLES3_DrawCensusKey( const gles3DrawKey_t *k, qboolean in2D ) {
	gles3Census_t	*c = in2D ? &s_gles3Census2D : &s_gles3Census3D;
	char			 keyBuf[GLES3_CENSUS_KEYLEN + 64];
	const char		*key = NULL;
	unsigned		 h;
	int				 slot;
	int				 idx;

	if ( !R_ProbeStatsOn() ) {
		return;
	}

	GLES3_CensusInit( c );

	if ( c->lastFrame != tr.frameCount ) {
		c->lastFrame = tr.frameCount;
		c->printedThisFrame = 0;
	}

	c->totalDraws++;

	h = GLES3_CensusHashKey( k );
	slot = (int)( h & ( GLES3_CENSUS_HASHSIZE - 1 ) );
	while ( c->hash[slot] ) {
		idx = c->hash[slot] - 1;
		if ( GLES3_CensusKeyEqual( &c->keys[idx], k, h ) ) {
			c->keys[idx].count++;
			goto summary;
		}
		slot = ( slot + 1 ) & ( GLES3_CENSUS_HASHSIZE - 1 );
	}

	GLES3_FormatDrawKey( keyBuf, (int)sizeof( keyBuf ), k );
	key = keyBuf;

	if ( c->numKeys >= GLES3_CENSUS_MAXKEYS ) {
		c->capacityDropped++;
		if ( !c->noticeDone ) {
			c->noticeDone = qtrue;
			ri.Printf( PRINT_ALL,
				"[GLES3:draw%s] KEY CAPACITY reached: %d distinct keys recorded; "
				"further NEW keys are counted but NOT recorded (dropped=%u so far). "
				"Rows already printed remain valid; [GLES3:drawsum] keeps counting.\n",
				in2D ? "2d" : "", GLES3_CENSUS_MAXKEYS, c->capacityDropped );
		}
		goto summary;
	}

	{
		int		 len = (int)strlen( key );
		qboolean trunc = ( len >= GLES3_CENSUS_KEYLEN ) ? qtrue : qfalse;

		if ( trunc ) {
			len = GLES3_CENSUS_KEYLEN - 1;
			c->truncatedKeys++;
		}
		idx = c->numKeys++;
		c->keys[idx].key = ri.Malloc( len + 1 );
		Com_Memcpy( c->keys[idx].key, key, len );
		c->keys[idx].key[len] = '\0';
		c->keys[idx].count = 1;
		c->keys[idx].hash = h;
		c->keys[idx].k = *k;
		c->hash[slot] = idx + 1;
	}

	if ( c->printedThisFrame < GLES3_CENSUS_NEW_PER_FRAME ) {
		c->printedThisFrame++;
		ri.Printf( PRINT_ALL, "[GLES3:draw%s] n=%u %s\n",
				   in2D ? "2d" : "", c->keys[c->numKeys - 1].count, key );
	} else {
		c->budgetSuppressed++;
	}

summary:
	if ( c->lastSummaryFrame < 0 ) {
		c->lastSummaryFrame = tr.frameCount;
	} else if ( tr.frameCount - c->lastSummaryFrame >= GLES3_CENSUS_SUMMARY_FRAMES ) {
		c->lastSummaryFrame = tr.frameCount;
		GLES3_CensusSummary( c, in2D );
	}
}


/*
 * One [NIGHTVISION:nv] row per 60 frames: `fullbright' is the `& 2' predicate
 * retail's stage iterators test, `nightvision' the shipped view mode that sets
 * it.  They are separate because the bridge question is about the second.
 */
void GLES3_FullbrightNote( void )
{
	static int s_nvSubs;
	static int s_nvFrame = -1;
	static int s_nvRows;
	static cvar_t *s_nvCvar;

	if ( !R_ProbeStatsOn() ) {
		return;
	}

	s_nvSubs++;

	if ( s_nvRows > 200 ) {
		return;						
	}
	if ( s_nvFrame != tr.frameCount ) {
		s_nvFrame = tr.frameCount;
		if ( tr.frameCount % 60 == 0 ) {
			if ( !s_nvCvar ) {
				s_nvCvar = ri.Cvar_Get( "nightvision_nvStats", "0", 0 );
			}
			if ( s_nvCvar && s_nvCvar->integer ) {
				if ( s_nvRows == 200 ) {
					ri.Printf( PRINT_ALL, "[NIGHTVISION:nv] capped at 200 rows; no further lines\n" );
				} else {
					ri.Printf( PRINT_ALL,
						"[NIGHTVISION:nv] row=%d frame=%d beRDF=0x%08x trRDF=0x%08x "
						"fullbright=%d nightvision=%d subsThisFrame=%d\n",
						s_nvRows, tr.frameCount,
						(unsigned)backEnd.refdef.rdflags, (unsigned)tr.refdef.rdflags,
						( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) ? 1 : 0,
						( backEnd.refdef.rdflags & RDF_NIGHTVISION ) ? 1 : 0,
						s_nvSubs );
				}
				s_nvRows++;
			}
		}
		s_nvSubs = 0;
	}
}

/* Three budgets: ordinary rows, rows where the fullbright arm substituted a
 * bind, and rows where a bind did not follow the stage's own image.  The arm and
 * the mismatch are different questions, so the loading screen's animated dots
 * must not spend the arm's budget before the bridge is drawn. */
#define SURFSTATS_ROWS			4096
#define SURFSTATS_ARM_ROWS		64
#define SURFSTATS_MISS_ROWS		64

#define SURFSTAT_DUP	0
#define SURFSTAT_NEW	1
#define SURFSTAT_FULL	2

typedef struct {
	const void	*sh;
	const void	*st;
	int			rgb;
	int			u0, u1;
} surfStatRow_t;

static int R_SurfStatClaim( surfStatRow_t *rows, int *used, int cap,
						   const void *sh, const void *st, int rgb, unsigned u0, unsigned u1 )
{
	int		i;

	for ( i = 0; i < *used && i < cap; i++ ) {
		if ( rows[i].sh == sh && rows[i].st == st && rows[i].rgb == rgb
		  && rows[i].u0 == (int)u0 && rows[i].u1 == (int)u1 ) {
			return SURFSTAT_DUP;
		}
	}

	if ( *used >= cap ) {
		if ( *used == cap ) {
			(*used)++;
			return SURFSTAT_FULL;
		}
		return SURFSTAT_DUP;
	}

	rows[*used].sh  = sh;
	rows[*used].st  = st;
	rows[*used].rgb = rgb;
	rows[*used].u0  = (int)u0;
	rows[*used].u1  = (int)u1;
	(*used)++;
	return SURFSTAT_NEW;
}

/*
 * One row per distinct stage bind, reported by every stage path that binds a
 * bundle -- the generic iterator through `GLES3_DrawStage' and the lightmapped
 * multitexture iterator, which is the fast path most world geometry takes and
 * which bound its bundles directly, so the census never saw the level.  The row
 * carries the raw flags word and both bits apart, and it is claimed against one
 * of three budgets: the arm actually firing, a bind that did not follow the
 * stage's own image, or neither.  `want' is the stage's own image for the unit,
 * `u' is what is bound there now.
 */
void GLES3_StageBindCensus( const shader_t *shader, const shaderStage_t *pStage,
							int stageIndex, qboolean in2D,
							unsigned w0, unsigned w1, unsigned u0, unsigned u1 )
{
	static surfStatRow_t	s_surfSeen[SURFSTATS_ROWS];
	static surfStatRow_t	s_surfArm[SURFSTATS_ARM_ROWS];
	static surfStatRow_t	s_surfMiss[SURFSTATS_MISS_ROWS];
	static int			s_surfN, s_surfArmN, s_surfMissN, s_surfRows;
	static cvar_t		*s_surfCvar;

	int			claim;
	qboolean	fbarm;
	qboolean	bindmiss;
	qboolean	nv;

	if ( !shader || !pStage ) {
		return;
	}

	if ( !s_surfCvar ) {
		s_surfCvar = ri.Cvar_Get( "lifecycle_surfStats", "0", 0 );
	}

	if ( !s_surfCvar || !s_surfCvar->integer || !R_ProbeStatsOn() ) {
		return;
	}

	fbarm = (qboolean)( ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D
						&& ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap ) );
	bindmiss = (qboolean)( u0 != w0 || ( w1 != 0 && u1 != w1 ) );
	nv = (qboolean)( ( backEnd.refdef.rdflags & RDF_NIGHTVISION ) ? qtrue : qfalse );

	if ( fbarm ) {
		GLES3_FullbrightNote();
	}

	claim = fbarm
		? R_SurfStatClaim( s_surfArm, &s_surfArmN, SURFSTATS_ARM_ROWS,
						   (const void *)shader, (const void *)pStage, (int)pStage->rgbGen, u0, u1 )
		: ( bindmiss
			? R_SurfStatClaim( s_surfMiss, &s_surfMissN, SURFSTATS_MISS_ROWS,
							   (const void *)shader, (const void *)pStage, (int)pStage->rgbGen, u0, u1 )
			: R_SurfStatClaim( s_surfSeen, &s_surfN, SURFSTATS_ROWS,
							   (const void *)shader, (const void *)pStage, (int)pStage->rgbGen, u0, u1 ) );

	if ( claim == SURFSTAT_FULL ) {
		ri.Printf( PRINT_ALL, "[LIFECYCLE:surf] capped at %d %s rows; no further rows\n",
				   fbarm ? SURFSTATS_ARM_ROWS : ( bindmiss ? SURFSTATS_MISS_ROWS : SURFSTATS_ROWS ),
				   fbarm ? "fullbright-arm" : ( bindmiss ? "bind-mismatch" : "distinct" ) );
	} else if ( claim == SURFSTAT_NEW ) {
		/* A row this frame that no frame before it printed was not recorded, so
		   the count of NEW rows can never be read as the count of draws.  The
		   dropped count is printed with the row so a thin census says which of
		   the two budgets it hit. */
		s_surfRows++;

		ri.Printf( PRINT_ALL,
			"[LIFECYCLE:surf] n=%d kind=%s shader=%s stage=%d rgb=%d lm=%d/%d in2D=%d "
			"want0=%u want1=%u u0=%u u1=%u arm0=%d arm1=%d rdf=0x%08x fullbright=%d nv=%d\n",
			s_surfRows, fbarm ? "arm" : ( bindmiss ? "miss" : "seen" ),
			shader->name, stageIndex, (int)pStage->rgbGen,
			(int)pStage->bundle[0].isLightmap, (int)pStage->bundle[1].isLightmap,
			(int)in2D, w0, w1, u0, u1,
			(int)( ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D && pStage->bundle[0].isLightmap ),
			(int)( pStage->bundle[1].image[0] && ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D && pStage->bundle[1].isLightmap ),
			(unsigned)backEnd.refdef.rdflags,
			( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) ? 1 : 0,
			nv ? 1 : 0 );
	}
}

void GLES3_DrawStage( const shaderCommands_t *input, const shaderStage_t *pStage,
					  const shader_t *shader, unsigned stateBits, float forceAlpha,
					  qboolean hudColorClobber )
{
	gles3Program_t		*prog;
	const float			*tc0, *tc1;
	const unsigned char	*colors;
	int					tcStride;
	qboolean			cpuColor = GLES3_StageNeedsCPUColor( pStage );
	qboolean			cpuTexCoords = GLES3_StageNeedsCPUTexCoords( pStage );

	BE_SUB( BE_FAM_DS, BE_DSZ_sub_unif, {
		prog = GLES3_StageProgram( pStage, shader, 0 );
		GLES3_BindProgram( prog );

		GLES3_SetupStageUniforms( prog, pStage, shader );
		GLES3_SetupFogUniforms( prog, tess.fogNum, pStage->fogDensityArm );
		GLES3_SetUniform1f( prog, GLES3_U_FORCEALPHA, forceAlpha );
	} );

	if ( hudColorClobber && ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D
		 && pStage->rgbGen != CGEN_LIGHTING_SPHERICAL
		 && pStage->rgbGen != CGEN_NOISE
		 && pStage->rgbGen != CGEN_SCOORD
		 && pStage->rgbGen != CGEN_TCOORD )
	{
		GLES3_SetUniform1i( prog, GLES3_U_COLORGEN, (int)CGEN_CONSTANT );
		GLES3_SetUniform1i( prog, GLES3_U_ALPHAGEN, (int)AGEN_IDENTITY );
		GLES3_SetBaseColor4f( prog, 1.0f, 1.0f, 1.0f, 1.0f );
	}

	if ( pStage->bundle[0].vertexLightmap && r_vertexLight->integer && r_lightmap->integer )
	{
		GLES3_SelectTexture( 0 );
		GL_Bind( tr.whiteImage );
	}
	else if ( ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D && pStage->bundle[0].isLightmap )
	{
		GLES3_SelectTexture( 0 );
		GL_Bind( tr.whiteImage );
	}
	else
	{
		GLES3_BindAnimatedImage( &pStage->bundle[0], 0 );
	}

	if ( pStage->bundle[1].image[0] )
	{
		if ( ( backEnd.refdef.rdflags & RDF_FULLBRIGHT ) && !backEnd.in2D && pStage->bundle[1].isLightmap )
		{
			GLES3_SelectTexture( 1 );
			GL_Bind( tr.whiteImage );
		}
		else
		{
			GLES3_BindAnimatedImage( &pStage->bundle[1], 1 );
		}
	}

	{
		int			sIdx = -1;
		int			i;
		unsigned	u0 = (unsigned)glState.currenttextures[0];
		unsigned	u1 = (unsigned)glState.currenttextures[1];
		unsigned	w0 = pStage->bundle[0].image[0] ? (unsigned)pStage->bundle[0].image[0]->texnum : 0u;
		unsigned	w1 = pStage->bundle[1].image[0] ? (unsigned)pStage->bundle[1].image[0]->texnum : 0u;

		for ( i = 0; i < MAX_SHADER_STAGES; i++ ) {
			if ( tess.xstages[i] == pStage ) { sIdx = i; break; }
		}

		GLES3_StageBindCensus( shader, pStage, sIdx, backEnd.in2D ? qtrue : qfalse, w0, w1, u0, u1 );
	}


	if ( cpuTexCoords )
	{
		tc0 = input->svars.texcoords[0][0];
		tc1 = input->svars.texcoords[1][0];
		tcStride = 2;
	}
	else
	{
		tc0 = input->texCoords[0][0];
		tc1 = input->texCoords[0][0] + 2;
		tcStride = 4;
	}
	colors = cpuColor ? (const unsigned char *)input->svars.colors
					  : (const unsigned char *)input->vertexColors;

	(void)stateBits;

	{
		gles3DrawKey_t k;

		k.shader  = shader->name;
		k.prog    = prog->name;
		k.defines = prog->defines;
		k.img0    = ( pStage->bundle[0].image[0] && pStage->bundle[0].image[0]->imgName[0] )
					? pStage->bundle[0].image[0]->imgName : "-";
		k.img1    = ( pStage->bundle[1].image[0] && pStage->bundle[1].image[0]->imgName[0] )
					? pStage->bundle[1].image[0]->imgName : "-";
		k.rgbGen     = (int)pStage->rgbGen;
		k.alphaGen   = (int)pStage->alphaGen;
		k.tcGen0     = (int)pStage->bundle[0].tcGen;
		k.tcGen1     = (int)pStage->bundle[1].tcGen;
		k.stateBits  = stateBits;
		k.cpuColor   = cpuColor ? 1 : 0;
		k.cpuTexCoords = cpuTexCoords ? 1 : 0;
		k.vc0        = *(const unsigned *)&input->vertexColors[0];

		GLES3_DrawCensusKey( &k, backEnd.in2D ? qtrue : qfalse );
	}

	/* Where a 3D surface actually lands on screen.  The menu's ship submits its
	   draws every frame and reaches no pixel; a box off [0,1]x[0,1] names the
	   loss a transform/position loss and a box inside one names a raster loss. */
	if ( !backEnd.in2D && input->numVertexes > 0 && R_ProbeStatsOn()
		 && !Q_stricmpn( shader->name, "menu/", 5 ) )
	{
		static int	s_gles3BBoxRows;
		const float	*mv = RB_GetModelViewMatrix();
		const float	*proj = RB_GetProjectionMatrix();
		float		mvp[16], px, py, pz, pw;
		float		minx, miny, minz, maxx, maxy, maxz, minw, maxw;
		float		mins0, mins1, maxs0, maxs1;
		float		minnx, minny, minnz, maxnx, maxny, maxnz;
		int			v, nan;

		GLES3_Mat4Mul( mvp, proj, mv );

		minx = miny = minz = minw = 1e30f;
		maxx = maxy = maxz = maxw = -1e30f;
		mins0 = mins1 = maxs0 = maxs1 = 0.0f;
		nan = 0;
		for ( v = 0; v < input->numVertexes; v++ )
		{
			const float *p = input->xyz[v];

			px = mvp[0] * p[0] + mvp[4] * p[1] + mvp[8]  * p[2] + mvp[12];
			py = mvp[1] * p[0] + mvp[5] * p[1] + mvp[9]  * p[2] + mvp[13];
			pz = mvp[2] * p[0] + mvp[6] * p[1] + mvp[10] * p[2] + mvp[14];
			pw = mvp[3] * p[0] + mvp[7] * p[1] + mvp[11] * p[2] + mvp[15];

			if ( px < minx ) minx = px;
			if ( px > maxx ) maxx = px;
			if ( py < miny ) miny = py;
			if ( py > maxy ) maxy = py;
			if ( pz < minz ) minz = pz;
			if ( pz > maxz ) maxz = pz;
			if ( pw < minw ) minw = pw;
			if ( pw > maxw ) maxw = pw;

			if ( v == 0 )
			{
				mins0 = maxs0 = input->texCoords[ v ][ 0 ][ 0 ];
				mins1 = maxs1 = input->texCoords[ v ][ 0 ][ 1 ];
				minnx = maxnx = input->normal[ v ][ 0 ];
				minny = maxny = input->normal[ v ][ 1 ];
				minnz = maxnz = input->normal[ v ][ 2 ];
			}
			else
			{
				float s = input->texCoords[ v ][ 0 ][ 0 ];
				float t = input->texCoords[ v ][ 0 ][ 1 ];

				/* A NaN fails every comparison, so the running box never grows:
				 * "max stayed below min" is the NaN detector. */
				if ( s < mins0 ) mins0 = s;
				if ( s > maxs0 ) maxs0 = s;
				if ( t < mins1 ) mins1 = t;
				if ( t > maxs1 ) maxs1 = t;
				if ( !( maxs0 >= mins0 ) || !( maxs1 >= mins1 ) ) nan = 1;

				if ( input->normal[ v ][ 0 ] < minnx ) minnx = input->normal[ v ][ 0 ];
				if ( input->normal[ v ][ 0 ] > maxnx ) maxnx = input->normal[ v ][ 0 ];
				if ( input->normal[ v ][ 1 ] < minny ) minny = input->normal[ v ][ 1 ];
				if ( input->normal[ v ][ 1 ] > maxny ) maxny = input->normal[ v ][ 1 ];
				if ( input->normal[ v ][ 2 ] < minnz ) minnz = input->normal[ v ][ 2 ];
				if ( input->normal[ v ][ 2 ] > maxnz ) maxnz = input->normal[ v ][ 2 ];
			}
		}

		s_gles3BBoxRows++;
		if ( s_gles3BBoxRows <= 12 || ( s_gles3BBoxRows % 240 ) == 0 )
		{
			ri.Printf( PRINT_ALL,
				"[GLES3:bbox] sh=%s n=%d ndc=(%.3f..%.3f, %.3f..%.3f, %.3f..%.3f) w=(%.3f..%.3f)"
				" st=(%.3f..%.3f, %.3f..%.3f) nrm=(%.2f..%.2f, %.2f..%.2f, %.2f..%.2f) nan=%d f=%d\n",
				shader->name, input->numVertexes,
				minx, maxx, miny, maxy, minz, maxz, minw, maxw,
				mins0, maxs0, mins1, maxs1,
				minnx, maxnx, minny, maxny, minnz, maxnz, nan, RB_FrameCount() );
		}
	}

	if ( shader == tr.defaultShader )
	{
		SMP_ShaderDefaultProbe( "draw", "-",
			( backEnd.currentEntity && tr.currentModel ) ? tr.currentModel->name :
			( tr.currentModel ? tr.currentModel->name : "(no-model)" ),
			backEnd.currentEntity ? backEnd.currentEntity->e.entityNumber : -1,
			backEnd.currentEntity ? backEnd.currentEntity->e.hModel : -1 );
	}

#define SMP_DECAL_MAX 16
	if ( R_ProbeStatsOn() && !Q_stricmp( shader->name, "markShadow" ) )
	{
		static int		s_smpDecalCount;
		static qboolean	s_smpDecalNotice;

		if ( s_smpDecalCount < SMP_DECAL_MAX )
		{
			GLint		iLiveTex = -1, iSrc = -1, iDst = -1, iFunc = -1;
			GLboolean	bMask = 2, bTest = 2;
			unsigned	vc0 = 0, vc1 = 0;
			float		s0x = 0.0f, s0y = 0.0f, s1x = 0.0f, s1y = 0.0f;
			const char	*n0 = ( pStage->bundle[0].image[0] && pStage->bundle[0].image[0]->imgName[0] )
							   ? pStage->bundle[0].image[0]->imgName : "-";
			unsigned	tex0 = pStage->bundle[0].image[0] ? (unsigned)pStage->bundle[0].image[0]->texnum : 0u;

			if ( input->numVertexes > 0 )
			{
				vc0 = *(const unsigned *)&input->vertexColors[0];
				s0x = input->texCoords[0][0][0];
				s0y = input->texCoords[0][0][1];
			}
			if ( input->numVertexes > 1 )
			{
				vc1 = *(const unsigned *)&input->vertexColors[1];
				s1x = input->texCoords[1][0][0];
				s1y = input->texCoords[1][0][1];
			}

			qglGetIntegerv( GL_TEXTURE_BINDING_2D, &iLiveTex );
			qglGetIntegerv( GL_BLEND_SRC_RGB, &iSrc );
			qglGetIntegerv( GL_BLEND_DST_RGB, &iDst );
			qglGetIntegerv( GL_DEPTH_FUNC, &iFunc );
			qglGetBooleanv( GL_DEPTH_WRITEMASK, &bMask );
			qglGetBooleanv( GL_DEPTH_TEST, &bTest );

			s_smpDecalCount++;

			ri.Printf(
				PRINT_ALL,
				"[SMP:decal] sh=%s prog=%s def=0x%06x state=0x%08x rgb=%d alpha=%d"
				" img0=%s tex0=%u cache0=%u"
				" tmu=%d liveTex=%d blend=%d src=0x%04x dst=0x%04x"
				" dtest=%d dmask=%d dfunc=0x%04x"
				" vc0=%08x vc1=%08x st0=(%.4f,%.4f) st1=(%.4f,%.4f)\n",
				shader->name, prog->name, prog->defines, stateBits,
				(int)pStage->rgbGen, (int)pStage->alphaGen,
				n0, tex0, (unsigned)glState.currenttextures[0],
				glState.currenttmu, iLiveTex,
				(int)qglIsEnabled( GL_BLEND ), (unsigned)iSrc, (unsigned)iDst,
				bTest ? 1 : 0, bMask ? 1 : 0, (unsigned)iFunc,
				vc0, vc1, s0x, s0y, s1x, s1y
			);
		}
		else if ( !s_smpDecalNotice )
		{
			s_smpDecalNotice = qtrue;
			ri.Printf( PRINT_ALL, "[SMP:decal] capped at %d rows; no further lines\n", SMP_DECAL_MAX );
		}
	}
#undef SMP_DECAL_MAX

#define SMP_DECALTX_MAX 4
	if ( R_ProbeStatsOn() && !Q_stricmp( shader->name, "markShadow" ) )
	{
		static int	s_smpDecaltxCount;

		if ( s_smpDecaltxCount < SMP_DECALTX_MAX )
		{
			GLint	iMinF = 0, iMagF = 0, iMaxL = 0, iBaseL = 0, iAniso = 0;
			GLint	iWrapS = 0, iWrapT = 0;
			int		iEnabled = -1, iSize = -1, iStride = -1, iType = -1, iNorm = -1;
			const void *pAttrib = NULL;
			int		iCG = -9999, iAG = -9999, iAcff = -9999, iFogNum = -9999, iFogArm = -9999;

			qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &iMinF );
			qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &iMagF );
			qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &iBaseL );
			qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &iMaxL );
			qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iWrapS );
			qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &iWrapT );
			if ( textureFilterAnisotropic ) {
				qglGetTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &iAniso );
			}

			RB_VBO_DecalAttribProbe( &iEnabled, &iSize, &iStride, &iType, &iNorm, &pAttrib );

			if ( prog->uniforms[GLES3_U_COLORGEN] != -1 ) {
				qglGetUniformiv( prog->program, prog->uniforms[GLES3_U_COLORGEN], &iCG );
			}
			if ( prog->uniforms[GLES3_U_ALPHAGEN] != -1 ) {
				qglGetUniformiv( prog->program, prog->uniforms[GLES3_U_ALPHAGEN], &iAG );
			}
			if ( prog->uniforms[GLES3_U_ACFF] != -1 ) {
				qglGetUniformiv( prog->program, prog->uniforms[GLES3_U_ACFF], &iAcff );
			}
			if ( prog->uniforms[GLES3_U_FOGNUM] != -1 ) {
				qglGetUniformiv( prog->program, prog->uniforms[GLES3_U_FOGNUM], &iFogNum );
			}
			if ( prog->uniforms[GLES3_U_FOGDENSITYARM] != -1 ) {
				qglGetUniformiv( prog->program, prog->uniforms[GLES3_U_FOGDENSITYARM], &iFogArm );
			}

			s_smpDecaltxCount++;

			ri.Printf( PRINT_ALL,
				"[SMP:decaltx] tex=%d minF=0x%04x magF=0x%04x baseL=%d maxL=%d aniso=%d wrap=%d,%d"
				" aColor(en=%d size=%d type=0x%04x stride=%d norm=%d ptr=%p)"
				" u_ColorGen=%d u_AlphaGen=%d u_Acff=%d u_FogNum=%d u_FogArm=%d\n",
				glState.currenttextures[0],
				(unsigned)iMinF, (unsigned)iMagF, (int)iBaseL, (int)iMaxL, (int)iAniso,
				(int)iWrapS, (int)iWrapT,
				iEnabled, iSize, (unsigned)iType, iStride, iNorm, pAttrib,
				iCG, iAG, iAcff, iFogNum, iFogArm );
		}
	}
#undef SMP_DECALTX_MAX

#define SMP_UI2D_MAX 64
	{
		static int	s_smpui2dCount;
		static char	*s_smpui2dSeen[SMP_UI2D_MAX];
		int			i;
		qboolean	isUI = backEnd.in2D
			|| !Q_stricmpn( shader->name, "inv-", 4 )
			|| !Q_stricmpn( shader->name, "hud/", 4 )
			|| !Q_stricmpn( shader->name, "sysimg/", 7 );

		if ( isUI && R_ProbeStatsOn() ) {
			if ( s_smpui2dCount >= SMP_UI2D_MAX ) {
				if ( s_smpui2dCount == SMP_UI2D_MAX ) {
					s_smpui2dCount++;
					ri.Printf( PRINT_ALL, "[SMP:ui2d] cap %d reached; no further rows\n",
							   SMP_UI2D_MAX );
				}
			} else {
				for ( i = 0; i < s_smpui2dCount; i++ ) {
					if ( !Q_stricmp( s_smpui2dSeen[i], shader->name ) ) {
						break;
					}
				}
				if ( i == s_smpui2dCount ) {
					const char *n0 = ( pStage->bundle[0].image[0] && pStage->bundle[0].image[0]->imgName[0] )
									 ? pStage->bundle[0].image[0]->imgName : "-";
					const char *n1 = ( pStage->bundle[1].image[0] && pStage->bundle[1].image[0]->imgName[0] )
									 ? pStage->bundle[1].image[0]->imgName : "-";
					unsigned tex0 = pStage->bundle[0].image[0] ? pStage->bundle[0].image[0]->texnum : 0;
					unsigned tex1 = pStage->bundle[1].image[0] ? pStage->bundle[1].image[0]->texnum : 0;
					unsigned vc0  = *(const unsigned *)&input->vertexColors[0];

					s_smpui2dSeen[s_smpui2dCount] = ri.Malloc( (int)strlen( shader->name ) + 1 );
					Q_strncpyz( s_smpui2dSeen[s_smpui2dCount], shader->name,
								(int)strlen( shader->name ) + 1 );
					s_smpui2dCount++;

					ri.Printf( PRINT_ALL,
						"[SMP:ui2d] sh=%s def=0x%06x in2D=%d rgb=%d alpha=%d tc=%d/%d st=0x%08x "
						"const=%d,%d,%d,%d aConst=%d base_a=%.3f c2D=%d,%d,%d,%d vc0=%08x CPUc=%d "
						"img0=%s(tex=%u) img1=%s(tex=%u) tmu=%d cur0=%u cur1=%u texenv=0x%x atest=%d\n",
						shader->name, prog->defines, backEnd.in2D ? 1 : 0,
						(int)pStage->rgbGen, (int)pStage->alphaGen,
						(int)pStage->bundle[0].tcGen, (int)pStage->bundle[1].tcGen, stateBits,
						(int)pStage->colorConst[0], (int)pStage->colorConst[1],
						(int)pStage->colorConst[2], (int)pStage->colorConst[3],
						(int)pStage->alphaConst,
						pStage->alphaGen == AGEN_GLOBAL_ALPHA
							? backEnd.color2D[3] / 255.0f
							: pStage->colorConst[3] / 255.0f,
						(int)backEnd.color2D[0], (int)backEnd.color2D[1],
						(int)backEnd.color2D[2], (int)backEnd.color2D[3], vc0,
						cpuColor ? 1 : 0, n0, tex0, n1, tex1,
						glState.currenttmu, glState.currenttextures[0], glState.currenttextures[1],
						GLES3_TexEnvForStage( pStage, shader ), (int)s_currentAlphaTest );
				}
			}
		}
	}

	RB_VBO_DrawTessArrays( input, input->indexes, input->numIndexes,
						   tc0, tc1, tcStride, colors, input->normal[0] );
}

#define GLES3_WIRE_MAX_VERTS	SHADER_MAX_VERTEXES

typedef struct {
	float	pos[4];
	float	col[4];
} gles3WireVertex_t;

static gles3WireVertex_t	s_wireVerts[GLES3_WIRE_MAX_VERTS];
static int					s_wireNumVerts;
static GLuint				s_wireVao, s_wireVbo;
static qboolean				s_wireInit;

static void GLES3_WireEnsure( void )
{
	if ( s_wireInit )
	{
		return;
	}
	qglGenVertexArrays( 1, &s_wireVao );
	qglGenBuffers( 1, &s_wireVbo );
	RB_BindVAO( s_wireVao );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_wireVbo );
	qglBufferData( GL_ARRAY_BUFFER, sizeof( s_wireVerts ), NULL, GL_STREAM_DRAW );
	qglEnableVertexAttribArray( ATTR_POSITION );
	qglVertexAttribPointer( ATTR_POSITION, 4, GL_FLOAT, GL_FALSE,
							(GLsizei)sizeof( gles3WireVertex_t ), (const void *)0 );
	qglEnableVertexAttribArray( ATTR_COLOR );
	qglVertexAttribPointer( ATTR_COLOR, 4, GL_FLOAT, GL_FALSE,
							(GLsizei)sizeof( gles3WireVertex_t ),
							(const void *)( sizeof( float ) * 4 ) );
	RB_BindVAO( 0 );
	s_wireInit = qtrue;
}

void GLES3_WireBegin( void )
{
	GLES3_WireEnsure();
	s_wireNumVerts = 0;
}

void GLES3_WireVertex( const float *xyz, const float *rgba )
{
	gles3WireVertex_t	*v;

	if ( s_wireNumVerts >= GLES3_WIRE_MAX_VERTS )
	{
		return;
	}
	v = &s_wireVerts[s_wireNumVerts++];
	v->pos[0] = xyz[0];
	v->pos[1] = xyz[1];
	v->pos[2] = xyz[2];
	v->pos[3] = 1.0f;
	v->col[0] = rgba[0];
	v->col[1] = rgba[1];
	v->col[2] = rgba[2];
	v->col[3] = rgba[3];
}

void GLES3_WireEnd( GLenum mode )
{
	GLsizeiptr	bytes;

	if ( s_wireNumVerts <= 0 )
	{
		return;
	}
	bytes = (GLsizeiptr)( (size_t)s_wireNumVerts * sizeof( gles3WireVertex_t ) );
	RB_BindVAO( s_wireVao );
	RB_BindBuffer( GL_ARRAY_BUFFER, s_wireVbo );
	qglBufferData( GL_ARRAY_BUFFER, bytes, s_wireVerts, GL_STREAM_DRAW );
	qglDrawArrays( mode, 0, s_wireNumVerts );
	BE_Bump( BE_FAM_BE, BE_BEC_draws, 1 );
	RB_BindVAO( 0 );
	s_wireNumVerts = 0;
}

gles3Program_t *GLES3_BeginFamily( unsigned family, int texEnv )
{
	gles3Program_t	*p = GLES3_FamilyProgram( family );

	GLES3_BindProgram( p );
	GLES3_SetupViewUniforms( p );

	if ( family == GLP_FAMILY_2D && !backEnd.in2D && glConfig.vidWidth > 0 ) {
		SMP_VpProbe( "2dfam", "GLES3_BeginFamily(2D)/in2D=0",
			0, 0, glConfig.vidWidth, glConfig.vidHeight, 0, qtrue );
	}
	GLES3_SetUniform1i( p, GLES3_U_TEX0, 0 );
	GLES3_SetUniform1i( p, GLES3_U_TEX1, 1 );
	GLES3_SetUniform1i( p, GLES3_U_TEXENV0, texEnv );
	GLES3_SetUniform1i( p, GLES3_U_TEXENV1, texEnv );
	GLES3_SetUniform1i( p, GLES3_U_ALPHATEST, (int)s_currentAlphaTest );
	GLES3_SetUniform4f( p, GLES3_U_TEX1COLOR, 1.0f, 1.0f, 1.0f, 1.0f );
	GLES3_SetUniform1f( p, GLES3_U_FORCEALPHA, -1.0f );
	GLES3_SetUniform1i( p, GLES3_U_ACFF, 0 );

	{
		static int s_gles3FamLogged;
		if ( R_ProbeStatsOn() && s_gles3FamLogged < 150 ) {
			ri.Printf( PRINT_ALL, "[GLES3:fam] family=%u texEnv=%d prog=%s curTMU=%d unit0tex=%d\n",
				family, texEnv, p->name, glState.currenttmu, glState.currenttextures[0] );
			s_gles3FamLogged++;
		}
	}
	return p;
}

void GLES3_Begin2D( int texEnv )
{
	gles3Program_t	*p = GLES3_BeginFamily( GLP_FAMILY_2D, texEnv );

	GLES3_SetBaseColor4f( p, backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f,
						  backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f );
}

void GLES3_BeginWire( void )
{
	GLES3_BeginFamily( GLP_FAMILY_WIRE, 1 );
}

void GLES3_BeginSprite( void )
{
	GLES3_BeginFamily( GLP_FAMILY_SPRITE, 1 );
}

void GLES3_BeginSunflare( void )
{
	GLES3_BeginFamily( GLP_FAMILY_SUNFLARE, 1 );
}

void GLES3_BeginShadow( void )
{
	GLES3_BeginFamily( GLP_FAMILY_SHADOW, 1 );
}

void GLES3_SetBaseColor4f( gles3Program_t *p, float r, float g, float b, float a )
{
	GLES3_SetUniform4f( p, GLES3_U_BASECOLOR, r, g, b, a );
}
