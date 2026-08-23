

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#define PRODUCT_NAME					"ProvenanceEF2"
#define PRODUCT_DATE					__DATE__

#define BASEGAME				"base"

// Support on the Mac), but a port that must run from a read-only Android/desktop
 #define HOMEPATH_NAME_UNIX_LEGACY	".provenanceef2"
 #define HOMEPATH_NAME				"provenanceef2"

#define CONFIG_PREFIX			"omconfig"

#define CLIENT_WINDOW_TITLE		"Star Trek: Elite Force II"
#define CLIENT_WINDOW_MIN_TITLE "Star Trek: Elite Force II"
    
#define MAX_MASTER_SERVERS      5	

#ifdef _MSC_VER

#pragma warning(disable : 4018)     
#pragma warning(disable : 4032)
#pragma warning(disable : 4051)
#pragma warning(disable : 4057)		
#pragma warning(disable : 4100)		
#pragma warning(disable : 4115)
#pragma warning(disable : 4125)		
#pragma warning(disable : 4127)		
#pragma warning(disable : 4136)
#pragma warning(disable : 4152)		
#pragma warning(disable : 4244)
#pragma warning(disable : 4142)		
#pragma warning(disable : 4514)
#pragma warning(disable : 4702)		
#pragma warning(disable : 4711)		
#pragma warning(disable : 4220)		
#endif

#ifdef __GNUC__
#define Q_UNUSED_VAR __attribute__((unused))
#define Q_NO_RETURN __attribute__((noreturn))

#ifdef __MINGW32__
#define Q_PRINTF_FUNC(fmt, va) \
	__attribute__((format(gnu_printf, fmt, va))) \
	__attribute__((format(ms_printf, fmt, va)))
#else
#define Q_PRINTF_FUNC(fmt, va) __attribute__((format(printf, fmt, va)))
#endif

#define Q_SCANF_FUNC(fmt, va) __attribute__((format(scanf, fmt, va)))
#define Q_ALIGN(x) __attribute__((aligned(x)))
#define Q_UNREACHABLE() __builtin_unreachable()
#else
#define Q_UNUSED_VAR
#define Q_NO_RETURN
#define Q_PRINTF_FUNC(fmt, va)
#define Q_SCANF_FUNC(fmt, va)
#define Q_ALIGN(x)
#define Q_UNREACHABLE()
#endif

#if (defined _MSC_VER)
#define Q_EXPORT __declspec(dllexport)
#elif (defined __SUNPRO_C)
#define Q_EXPORT __global
#elif ((__GNUC__ >= 3) && (!__EMX__) && (!sun))
#define Q_EXPORT __attribute__((visibility("default")))
#else
#define Q_EXPORT
#endif


#ifndef Q3_VM

#ifdef _DEBUG_MEM
#  ifdef WIN32
#    define _CRTDBG_MAP_ALLOC
#    include <stdlib.h>
#    include <crtdbg.h>
#  endif
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

#ifdef _WIN32
  size_t Q_vsnprintf(char *str, size_t size, const char *format, va_list ap) Q_PRINTF_FUNC(3, 0);
#else
#define Q_vsnprintf vsnprintf
#endif

#endif

#include "q_platform.h"


typedef unsigned char 		byte;
typedef unsigned char		uchar;

enum { qfalse, qtrue };

typedef union {
	float f;
	int i;
	unsigned int ui;
} floatint_t;

typedef int	qboolean;

typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;

#ifdef __GNUC__
#define ALIGN(x) __attribute__((aligned(x)))
#else
#define ALIGN(x)
#endif

#define PAD(base, alignment)	(((base)+(alignment)-1) & ~((alignment)-1))
#define PADLEN(base, alignment)	(PAD((base), (alignment)) - (base))

#define PADP(base, alignment)	((void *) PAD((intptr_t) (base), (alignment)))

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef BIT
#define BIT(x)				(1 << x)
#endif

#define STRING(s)			#s
#define XSTRING(s)			STRING(s)

#define	MAX_QINT			0x7fffffff
#define	MIN_QINT			(-MAX_QINT-1)

#define ARRAY_LEN(x)			(sizeof(x) / sizeof(*(x)))
#define STRARRAY_LEN(x)			(ARRAY_LEN(x) - 1)

#define	PITCH				0		
#define	YAW					1		
#define	ROLL				2		

#ifndef WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

typedef enum
{
	SIDE_FRONT = 0,
	SIDE_BACK = 1,
	SIDE_ON = 2,
	SIDE_CROSS = 3
} planeSide_t;

typedef enum
{
	SOLID_NOT,			
	SOLID_TRIGGER,		
	SOLID_BBOX,			
	SOLID_BSP			
} solid_t;

#define	MAX_STRING_CHARS	1024	
#define	MAX_STRING_TOKENS	512		
#define	MAX_TOKEN_CHARS		1024	

#define	MAX_INFO_STRING		1350
#define	MAX_INFO_KEY		  1024
#define	MAX_INFO_VALUE		1024

#define	BIG_INFO_STRING		8192  
#define	BIG_INFO_KEY		8192
#define	BIG_INFO_VALUE		8192

#define MAX_RES_NAME        64

#define	MAX_QPATH			256		
#ifdef PATH_MAX
#define MAX_OSPATH			PATH_MAX
#else
#define	MAX_OSPATH			256		
#endif

#define	MAX_NAME_LENGTH		32		
#define	MAX_HOSTNAME_LENGTH	80		

typedef enum {
	EXEC_NOW,			// don't return until completed, a VM should NEVER use this,
	EXEC_INSERT,		
	EXEC_APPEND			
} cbufExec_t;


#define	MAX_MAP_AREA_BYTES		32		


typedef enum {
	PRINT_ALL,
	PRINT_DEVELOPER,		
	PRINT_DEVELOPER_2,		
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;


#ifdef ERR_FATAL
#undef ERR_FATAL			
#endif

typedef enum {
	ERR_FATAL,					
	ERR_DROP,					
	ERR_DISCONNECT,				
	ERR_NEED_CD					
} errorParm_t;



#define PROP_GAP_WIDTH			5
#define PROP_SPACE_WIDTH		8
#define PROP_HEIGHT				30
#define PROP_SMALL_SIZE_SCALE	0.75

#define BLINK_DIVISOR			200
#define PULSE_DIVISOR			250.0f

#define UI_LEFT			0x00000000	
#define UI_CENTER		0x00000001
#define UI_RIGHT		0x00000002
#define UI_FORMATMASK	0x00000007
#define UI_SMALLFONT	0x00000010
#define UI_BIGFONT		0x00000020	
#define UI_GIANTFONT	0x00000040
#define UI_DROPSHADOW	0x00000800
#define UI_BLINK		0x00001000
#define UI_INVERSE		0x00002000
#define UI_PULSE		0x00004000

typedef enum {
	h_high,
	h_low,
	h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
#define Hunk_Alloc( size, preference )				Hunk_AllocDebug(size, preference, #size, __FILE__, __LINE__)
void *Hunk_AllocDebug( int size, ha_pref preference, const char *label, const char *file, int line );
#else
void *Hunk_Alloc( int size, ha_pref preference );
#endif

#define Com_Memset memset
#define Com_Memcpy memcpy

#define Com_Allocate malloc
#define Com_Dealloc free

#define CIN_system	1
#define CIN_loop	2
#define	CIN_hold	4
#define CIN_silent	8
#define CIN_shader	16



typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t quat_t[4]; 
typedef vec_t vec5_t[5];
typedef vec_t matrix3x3_t[9];
typedef vec_t matrix_t[16];
typedef vec3_t axis_t[3];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	
#endif

#ifndef M_SQRT2
#define M_SQRT2		1.414213562f
#endif

#ifndef M_SQRT1_2
#define M_SQRT1_2	7.0710678118654752440E-1
#endif

#define NUMVERTEXNORMALS	162
extern	vec3_t	bytedirs[NUMVERTEXNORMALS];

#define	SCREEN_WIDTH		640
#define	SCREEN_HEIGHT		480

#define TINYCHAR_WIDTH		(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT		(SMALLCHAR_HEIGHT/2)

#define SMALLCHAR_WIDTH		8
#define SMALLCHAR_HEIGHT	16

#define BIGCHAR_WIDTH		16
#define BIGCHAR_HEIGHT		16

#define	GIANTCHAR_WIDTH		32
#define	GIANTCHAR_HEIGHT	48

extern	vec4_t		colorBlack;
extern	vec4_t		colorRed;
extern	vec4_t		colorGreen;
extern	vec4_t		colorBlue;
extern	vec4_t		colorYellow;
extern	vec4_t		colorMagenta;
extern	vec4_t		colorCyan;
extern	vec4_t		colorWhite;
extern	vec4_t		colorLtGrey;
extern	vec4_t		colorMdGrey;
extern	vec4_t		colorDkGrey;

enum {
	MESSAGE_YELLOW = 1,
	MESSAGE_CHAT_WHITE,
	MESSAGE_WHITE,
	MESSAGE_CHAT_RED,
	MESSAGE_CHAT_GREEN,
	MESSAGE_MAX
};

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && isalnum(*((p)+1)) ) 

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define COLOR_NONE		'8'	 
#define ColorIndex(c)	( ( (c) - '0' ) & 7 )

#if 0
#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"
#else
#define S_COLOR_BLACK	""
#define S_COLOR_RED		""
#define S_COLOR_GREEN	""
#define S_COLOR_YELLOW	""
#define S_COLOR_BLUE	""
#define S_COLOR_CYAN	""
#define S_COLOR_MAGENTA	""
#define S_COLOR_WHITE	""
#endif

#define HUD_MESSAGE_YELLOW			"\x01"
#define HUD_MESSAGE_WHITE			"\x03"
#define HUD_MESSAGE_CHAT_WHITE		"\x02"
#define HUD_MESSAGE_CHAT_RED		"\x04"

extern vec4_t	g_color_table[8];

#define	MAKERGB( v, r, g, b ) v[0]=r;v[1]=g;v[2]=b
#define	MAKERGBA( v, r, g, b, a ) v[0]=r;v[1]=g;v[2]=b;v[3]=a

#define DEG2RAD( a ) ( ( (a) * M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )

#define Q_max(a, b)      ((a) > (b) ? (a) : (b))
#define Q_min(a, b)      ((a) < (b) ? (a) : (b))
#define Q_bound(a, b, c) (Q_max(a, Q_min(b, c)))
#define Q_clamp(a, b, c) ((b) >= (c) ? (a)=(b) : (a) < (b) ? (a)=(b) : (a) > (c) ? (a)=(c) : (a))
#define Q_lerp(from, to, frac) (from + ((to - from) * frac))

#define LERP( a, b, w ) ( ( a ) * ( 1.0f - ( w ) ) + ( b ) * ( w ) )
#define LUMA( red, green, blue ) ( 0.2126f * ( red ) + 0.7152f * ( green ) + 0.0722f * ( blue ) )

#ifndef Q_min
#define Q_min(a,b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef Q_max
#define Q_max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

int Q_clamp_int(int value, int min, int max);
float Q_clamp_float(float value, float min, float max);

#define bound(a,minval,maxval)  ( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

struct cplane_s;

extern	vec3_t	vec3_origin;
extern	vec3_t	axisDefault[3];
extern matrix_t matrixIdentity;

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

static ID_INLINE long Q_ftol(float f)
{
#if id386_sse && defined(_MSC_VER)
	static int      tmp;
	__asm fld f
	__asm fistp tmp
	__asm mov eax, tmp
#else
	return (long)f;
#endif
}

float Q_fabs( float f );
float Q_rsqrt( float f );		

#define SQRTFAST( x ) ( (x) * Q_rsqrt( x ) )

byte ClampByte(int i);
signed char ClampChar( int i );
signed short ClampShort( int i );

double		dEpsilon( void );
double 		dIdentity( void );
double		dSign( const double number );
double		dClamp( const double value, const double min, const double max );
double		dDistance( const double value1, const double value2 );
qboolean	dCloseEnough( const double value1, const double value2, const double epsilon );
qboolean	dSmallEnough( const double value, const double epsilon );

float		fEpsilon( void );
float 		fIdentity( void );
float		fSign( const float number );
float		fClamp( const float value, const float min, const float max );
float		fDistance( const float value1, const float value2 );
qboolean	fCloseEnough( const float value1, const float value2, const float epsilon );
qboolean	fSmallEnough( const float value, const float epsilon );

int			iSign( const int number );
int			iClamp( const int value, const int min, const int max );

int DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );

#if	1

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define DotProduct2D(x,y)		((x)[0]*(y)[0]+(x)[1]*(y)[1])
#define CrossProduct2D(a,b)		((a)[0]*(b)[1]-(b)[0]*(a)[1])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorAdd2D(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1])
#define VectorSub2D(a,b,c)		((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorCopy2D(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1])
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define	VectorScale2D(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s))
#define	VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))
#define	VectorMA2D(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s))

#else

#define DotProduct(x,y)			_DotProduct(x,y)
#define VectorSubtract(a,b,c)	_VectorSubtract(a,b,c)
#define VectorAdd(a,b,c)		_VectorAdd(a,b,c)
#define VectorCopy(a,b)			_VectorCopy(a,b)
#define	VectorScale(v, s, o)	_VectorScale(v,s,o)
#define	VectorMA(v, s, b, o)	_VectorMA(v,s,b,o)

#endif

#ifdef Q3_VM
#ifdef VectorCopy
#undef VectorCopy
typedef struct {
	float	v[3];
} vec3struct_t;
#define VectorCopy(a,b)	(*(vec3struct_t *)b=*(vec3struct_t *)a)
#endif
#endif

#define VectorClear2D(a)			((a)[0]=(a)[1]=0)
#define VectorLength2DSquared(a)	((a)[0]*(a)[0]+(a)[1]*(a)[1])
#define VectorLength2D(a)			(sqrt(VectorLength2DSquared(a)))
#define VectorSet2D(v, x, y)		((v)[0]=(x), (v)[1]=(y))

#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define	SnapVector(v) {v[0]=((int)(v[0]));v[1]=((int)(v[1]));v[2]=((int)(v[2]));}

#define DotProduct4(x,y)		((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2]+(x)[3]*(y)[3])
#define VectorSubtract4(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#define VectorAdd4(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2],(c)[3]=(a)[3]+(b)[3])
#define VectorCopy4(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define	VectorScale4(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s),(o)[3]=(v)[3]*(s))
#define	VectorMA4(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s),(o)[3]=(v)[3]+(b)[3]*(s))
#define VectorClear4(a)			((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#define VectorNegate4(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2],(b)[3]=-(a)[3])
#define VectorSet4(v,x,y,z,w)	((v)[0]=(x),(v)[1]=(y),(v)[2]=(z),(v)[3]=(w))

#define VectorCopy5(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3],(b)[4]=(a)[4])

vec_t _DotProduct( const vec3_t v1, const vec3_t v2 );
void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorCopy( const vec3_t in, vec3_t out );
void _VectorScale( const vec3_t in, float scale, vec3_t out );
void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc );
vec_t Q_rint( vec_t in );

unsigned ColorBytes3 (float r, float g, float b);
unsigned ColorBytes4 (float r, float g, float b, float a);

float NormalizeColor( const vec3_t in, vec3_t out );
void RotatePointAroundAxis(vec3_t dst, int axis, const vec3_t point, float degrees);
void ClampColor(vec4_t color);

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ClearBounds( vec3_t mins, vec3_t maxs );
void ZeroBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
void BoundsAdd(vec3_t mins, vec3_t maxs, const vec3_t mins2, const vec3_t maxs2);



int VectorCompare( const vec3_t v1, const vec3_t v2 );
vec_t VectorLength( const vec3_t v );
vec_t VectorLengthSquared( const vec3_t v );
vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 );
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );

#if !defined( Q3_VM ) || ( defined( Q3_VM ) && defined( __Q3_VM_MATH ) )
static ID_INLINE int VectorCompare2D( const vec2_t v1, const vec2_t v2 ) {
	if( v1[ 0 ] != v2[ 0 ] || v1[ 1 ] != v2[ 1 ] ) {
		return 0;
	}
	return 1;
}

static ID_INLINE int VectorCompareEpsilon( const vec3_t v1, const vec3_t v2, float fEpsilon )
{
	int i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( v1[ i ] - v2[ i ] ) > fEpsilon ) {
			return 0;
		}
	}
	return 1;
}

static ID_INLINE vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

static ID_INLINE void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

static ID_INLINE void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

#else
vec_t Distance( const vec3_t p1, const vec3_t p2 );

void VectorNormalizeFast( vec3_t v );

void VectorInverse( vec3_t v );

#endif

vec_t VectorNormalize (vec3_t v);		
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
vec_t VectorNormalize2D( vec2_t v );
vec_t VectorNormalize2D2( const vec2_t v, vec2_t out );
void VectorPackTo01( vec3_t v );
void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out );
void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out );

int NearestPowerOfTwo(int val);
int Q_log2(int val);

float Q_acos(float c);

int		Q_rand( int *seed );
float	Q_random( int *seed );
float	Q_crandom( int *seed );

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define random2()	((rand () & 0x7fff) * (1.0 / ((float)0x7fff)))
#define crandom()	(2.0 * (random() - 0.5))

float grandom( void );
float erandom( float mean );

#define AnglesToMat AnglesToAxis

void vectoangles( const vec3_t value1, vec3_t angles );
void VectorToAngles( const vec3_t vec, vec3_t angles );
void AnglesToAxis(const vec3_t angles, vec3_t axis[3]);
void YawToAxis(float yaw, float axis[2]);

float noise(float vec[], int len);
float noise1(float arg);
float noise2(vec3_t arg);
float noise3(vec3_t arg);

void	R_ConcatRotations( float in1[ 3 ][ 3 ], float in2[ 3 ][ 3 ], float out[ 3 ][ 3 ] );
void	R_ConcatTransforms( float in1[ 3 ][ 4 ], float in2[ 3 ][ 4 ], float out[ 3 ][ 4 ] );

void	MatrixToEulerAngles( const float mat[ 3 ][ 3 ], vec3_t ang );
void	QuatToMat( const float q[ 4 ], float m[ 3 ][ 3 ] );
void SlerpQuaternion(float from[4], float to[4], float t, float res[4]);
void	EulerToQuat( float ang[ 3 ], float q[ 4 ] );
void	MatToQuat( float srcMatrix[ 3 ][ 3 ], float destQuat[ 4 ] );

float ProjectPointOnLine( const vec3_t i_vStart, const vec3_t i_vEnd, const vec3_t i_vPoint, vec3_t o_vProj );
float ProjectLineOnPlane(const vec3_t vPlaneNorm, float fPlaneDist, const vec3_t vStart, const vec3_t vEnd, vec3_t vProj );

float anglemod( float a );
float angledist( float ang );

void AxisClear( vec3_t axis[3] );
void AxisCopy( const vec3_t in[3], vec3_t out[3] );

#define AXIS_FORWARD_VECTOR	0
#define AXIS_RIGHT_VECTOR		1
#define AXIS_UP_VECTOR			2

void AxisToAngles( vec3_t axis[3], vec3_t angles );

void SetPlaneSignbits( struct cplane_s *out );
int BoxOnPlaneSide (const vec3_t emins, const vec3_t emaxs, struct cplane_s *plane);

void  CalculateRotatedBounds( vec3_t angles, vec3_t mins, vec3_t maxs );
void  CalculateRotatedBounds2( float trans[ 3 ][ 3 ], vec3_t mins, vec3_t maxs );
int   BoundingBoxToInteger( vec3_t mins, vec3_t maxs );
void  IntegerToBoundingBox( int num, vec3_t mins, vec3_t maxs );
qboolean BoundsIntersect(const vec3_t mins, const vec3_t maxs,
		const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsIntersectSphere(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin, vec_t radius);
qboolean BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin);

float	AngleMod(float a);
float	LerpAngle (float from, float to, float frac);
float	LerpAngleFromCurrent( float from, float to, float current, float frac );
float	AngleSubtract( float a1, float a2 );
void	AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 );

float AngleNormalize360 ( float angle );
float AngleNormalize180 ( float angle );
float AngleNormalizeArbitrary ( const float angle, const float minimumAngle );  
float AngleDelta ( float angle1, float angle2 );

qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
qboolean PlanesGetIntersectionPoint(const vec4_t plane1, const vec4_t plane2, const vec4_t plane3, vec3_t out);
void PlaneIntersectRay(const vec3_t rayPos, const vec3_t rayDir, const vec4_t plane, vec3_t res);
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void RotateAroundDirection( vec3_t axis[3], float yaw );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );


void  MatrixTransformVector( const vec3_t in, const float mat[ 3 ][ 3 ], vec3_t out );
void  MatrixTransformVectorRight( const float mat[ 3 ][ 3 ], const vec3_t in, vec3_t out );
void Matrix3x3Multiply( const float in1[ 3 ][ 3 ], const float in2[ 3 ][ 3 ], float out[ 3 ][ 3 ] );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up );  
void AngleVectorsLeft( const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up );
void PerpendicularVector( vec3_t dst, const vec3_t src );

void            GetPerpendicularViewVector(const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up);
void            ProjectPointOntoVector(vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj);

float           pointToLineDistance(const vec3_t point, const vec3_t p1, const vec3_t p2);
float           VectorMinComponent(vec3_t v);
float           VectorMaxComponent(vec3_t v);

#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

vec_t           DistanceBetweenLineSegmentsSquared(const vec3_t sP0, const vec3_t sP1,
												   const vec3_t tP0, const vec3_t tP1, float *s, float *t);
vec_t           DistanceBetweenLineSegments(const vec3_t sP0, const vec3_t sP1,
											const vec3_t tP0, const vec3_t tP1, float *s, float *t);

void			VectorMatrixInverse( void* DstMatrix, const void* SrcMatrix );

void            MatrixIdentity(matrix_t m);
void            MatrixClear(matrix_t m);
void            MatrixCopy(const matrix_t in, matrix_t out);
qboolean        MatrixCompare(const matrix_t a, const matrix_t b);
void            MatrixTransposeIntoXMM(const matrix_t m);
void            MatrixTranspose(const matrix_t in, matrix_t out);

qboolean        MatrixInverse(matrix_t m);
void            MatrixSetupXRotation(matrix_t m, vec_t degrees);
void            MatrixSetupYRotation(matrix_t m, vec_t degrees);
void            MatrixSetupZRotation(matrix_t m, vec_t degrees);
void            MatrixSetupTranslation(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixSetupScale(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixSetupShear(matrix_t m, vec_t x, vec_t y);
void            Matrix4x4Multiply(const matrix_t a, const matrix_t b, matrix_t out);
void			MatrixMultiply( const float in1[ 3 ][ 3 ], const float in2[ 3 ][ 3 ], float out[ 3 ][ 3 ] );
void            MatrixMultiply2( matrix_t m, const matrix_t m2 );
void            MatrixMultiplyRotation(matrix_t m, vec_t pitch, vec_t yaw, vec_t roll);
void            MatrixMultiplyZRotation(matrix_t m, vec_t degrees);
void            MatrixMultiplyTranslation(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixMultiplyScale(matrix_t m, vec_t x, vec_t y, vec_t z);
void            MatrixMultiplyShear(matrix_t m, vec_t x, vec_t y);
void            MatrixToAngles(const matrix_t m, vec3_t angles);
void            MatrixFromAngles(matrix_t m, vec_t pitch, vec_t yaw, vec_t roll);
void            MatrixFromVectorsFLU(matrix_t m, const vec3_t forward, const vec3_t left, const vec3_t up);
void            MatrixFromVectorsFRU(matrix_t m, const vec3_t forward, const vec3_t right, const vec3_t up);
void            MatrixFromQuat(matrix_t m, const quat_t q);
void            MatrixFromPlanes(matrix_t m, const vec4_t left, const vec4_t right, const vec4_t bottom, const vec4_t top,
								 const vec4_t near, const vec4_t far);
void            MatrixToVectorsFLU(const matrix_t m, vec3_t forward, vec3_t left, vec3_t up);
void            MatrixToVectorsFRU(const matrix_t m, vec3_t forward, vec3_t right, vec3_t up);
void            MatrixSetupTransformFromVectorsFLU(matrix_t m, const vec3_t forward, const vec3_t left, const vec3_t up, const vec3_t origin);
void            MatrixSetupTransformFromVectorsFRU(matrix_t m, const vec3_t forward, const vec3_t right, const vec3_t up, const vec3_t origin);
void            MatrixSetupTransformFromRotation(matrix_t m, const matrix_t rot, const vec3_t origin);
void            MatrixSetupTransformFromQuat(matrix_t m, const quat_t quat, const vec3_t origin);
void            MatrixAffineInverse(const matrix_t in, matrix_t out);
void            MatrixTransformNormal(const matrix_t m, const vec3_t in, vec3_t out);
void            MatrixTransformNormal2(const matrix_t m, vec3_t inout);
void            MatrixTransformPoint(const matrix_t m, const vec3_t in, vec3_t out);
void            MatrixTransformPoint2(const matrix_t m, vec3_t inout);
void            MatrixTransform4(const matrix_t m, const vec4_t in, vec4_t out);
void            MatrixTransformPlane(const matrix_t m, const vec4_t in, vec4_t out);
void            MatrixTransformPlane2(const matrix_t m, vec4_t inout);
void            MatrixPerspectiveProjection(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);
void            MatrixPerspectiveProjectionLH(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);
void            MatrixPerspectiveProjectionRH(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);
void            MatrixPerspectiveProjectionFovYAspectLH(matrix_t m, vec_t fov, vec_t aspect, vec_t near, vec_t far);
void            MatrixPerspectiveProjectionFovXYLH(matrix_t m, vec_t fovX, vec_t fovY, vec_t near, vec_t far);
void            MatrixPerspectiveProjectionFovXYRH(matrix_t m, vec_t fovX, vec_t fovY, vec_t near, vec_t far);
void            MatrixPerspectiveProjectionFovXYInfiniteRH(matrix_t m, vec_t fovX, vec_t fovY, vec_t near);
void            MatrixOrthogonalProjection(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);

void			MatrixOrthogonalProjectionLH(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);
void			MatrixOrthogonalProjectionRH(matrix_t m, vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far);

void			MatrixPlaneReflection(matrix_t m, const vec4_t plane);

void            MatrixLookAtLH(matrix_t output, const vec3_t pos, const vec3_t dir, const vec3_t up);
void            MatrixLookAtRH(matrix_t m, const vec3_t eye, const vec3_t dir, const vec3_t up);
void            MatrixScaleTranslateToUnitCube(matrix_t m, const vec3_t mins, const vec3_t maxs);
void            MatrixCrop(matrix_t m, const vec3_t mins, const vec3_t maxs);

void			TransposeMatrix( float in[ 3 ][ 3 ], float out[ 3 ][ 3 ] );

static ID_INLINE void AnglesToMatrix(const vec3_t angles, matrix_t m)
{
	MatrixFromAngles(m, angles[PITCH], angles[YAW], angles[ROLL]);
}


#define QuatSet(q,x,y,z,w)	((q)[0]=(x),(q)[1]=(y),(q)[2]=(z),(q)[3]=(w))
#define QuatCopy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define QuatCompare(a,b)	((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3])

static ID_INLINE void QuatClear(quat_t q)
{
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}


static ID_INLINE void QuatCalcW(quat_t q)
{
#if 1
	vec_t           term = 1.0f - (q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);

	if(term < 0.0)
		q[3] = 0.0;
	else
		q[3] = -sqrt(term);
#else
	q[3] = sqrt(fabs(1.0f - (q[0] * q[0] + q[1] * q[1] + q[2] * q[2])));
#endif
}

static ID_INLINE void QuatInverse(quat_t q)
{
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
}

static ID_INLINE void QuatAntipodal(quat_t q)
{
	q[0] = -q[0];
	q[1] = -q[1];
	q[2] = -q[2];
	q[3] = -q[3];
}

static ID_INLINE vec_t QuatLength(const quat_t q)
{
	return (vec_t) sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

vec_t           QuatNormalize(quat_t q);

void            QuatFromAngles(quat_t q, vec_t pitch, vec_t yaw, vec_t roll);

static ID_INLINE void AnglesToQuat(const vec3_t angles, quat_t q)
{
	QuatFromAngles(q, angles[PITCH], angles[YAW], angles[ROLL]);
}

void            QuatFromMatrix(quat_t q, const matrix_t m);
void            QuatToVectorsFLU(const quat_t quat, vec3_t forward, vec3_t left, vec3_t up);
void            QuatToVectorsFRU(const quat_t quat, vec3_t forward, vec3_t right, vec3_t up);
void            QuatToAxis(const quat_t q, vec3_t axis[3]);
void            QuatToAngles(const quat_t q, vec3_t angles);

void			QuatToRotAngle( const quat_t q, vec_t *angle );
void			QuatToRotAngleAxis( const quat_t q, vec_t *angle, vec3_t axis );

void			QuatFromRotAngleAxis( quat_t q, vec_t angle, const vec3_t axis );


void            QuatMultiply0(quat_t qa, const quat_t qb);

void            QuatMultiply1(const quat_t qa, const quat_t qb, quat_t qc);

void            QuatMultiply2(const quat_t qa, const quat_t qb, quat_t qc);

void            QuatMultiply3(const quat_t qa, const quat_t qb, quat_t qc);

void            QuatMultiply4(const quat_t qa, const quat_t qb, quat_t qc);

void QuaternionMultiply( quat_t output, quat_t first, quat_t second );

void            QuatSlerp(const quat_t from, const quat_t to, float frac, quat_t out);
void            QuatTransformVector(const quat_t q, const vec3_t in, vec3_t out);
int Q_isnan( float x );



typedef struct
{
	qboolean        frameMemory;
	int             currentElements;
	int             maxElements;	
	void          **elements;
} growList_t;

void            Com_InitGrowList(growList_t * list, int maxElements);
void            Com_DestroyGrowList(growList_t * list);
int             Com_AddToGrowList(growList_t * list, void *data);
void           *Com_GrowListElement(const growList_t * list, int index);
int             Com_IndexForGrowListElement(const growList_t * list, const void *element);



float Com_Clamp( float min, float max, float value );

const char	*COM_SkipPath( const char *pathname );
const char	*COM_GetExtension( const char *name );
void	COM_StripExtension(const char *in, char *out, int destsize);
qboolean COM_CompareExtension(const char* in, const char* ext);
void	COM_DefaultExtension( char *path, int maxSize, const char *extension );

void	COM_BeginParseSession( const char *name );
int		COM_GetCurrentParseLine( void );
const char	*COM_Parse( char **data_p );
char	*COM_ParseExt( char **data_p, qboolean allowLineBreak );
char	*COM_ParseExtIgnoreQuotes( char **data_p, qboolean allowLineBreak );
const char *COM_GetToken( char **data_p, qboolean crossline );
void	Com_SkipRestOfLine(char **data);
void	Com_SkipBracedSection(char **program);
void	Com_Parse1DMatrix(char **buf_p, int x, float *m, qboolean checkBrackets);
int		COM_Compress( char *data_p );
void	COM_ParseError( char *format, ... ) Q_PRINTF_FUNC(1, 2);
void	COM_ParseWarning( char *format, ... ) Q_PRINTF_FUNC(1, 2);

#define MAX_TOKENLENGTH		1024

#ifndef TT_STRING
#define TT_STRING					1			
#define TT_LITERAL					2			
#define TT_NUMBER					3			
#define TT_NAME						4			
#define TT_PUNCTUATION				5			
#endif

typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
} pc_token_t;


void	COM_MatchToken( char**buf_p, char *match );

qboolean SkipBracedSection (char **program, int depth);
void SkipRestOfLine ( char **data );

void Parse1DMatrix (char **buf_p, int x, float *m);
void Parse2DMatrix (char **buf_p, int y, int x, float *m);
void Parse3DMatrix (char **buf_p, int z, int y, int x, float *m);

int Com_HexStrToInt( const char *str );

size_t QDECL Com_sprintf (char *dest, size_t size, const char *fmt, ...) Q_PRINTF_FUNC(3, 4);

char *Com_SkipTokens( char *s, int numTokens, const char *sep );
char *Com_SkipCharset( char *s, const char *sep );

void Com_RandomBytes(byte* string, int len);
void Com_BackslashToSlash( char *str );

typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;


#define CTRL(a) ((a)-'a'+1)

int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );
qboolean Q_isanumber( const char *s );
qboolean Q_isintegral( float f );

int		Q_stricmp (const char *s1, const char *s2);

int			SurfaceNameToType( const char *name );
const char	*SurfaceTypeToName( unsigned int surfaceType );
int		Q_strncmp( const char *s1, const char *s2, size_t n );
int		Q_stricmpn (const char *s1, const char *s2, size_t n);
char	*Q_strlwr( char *s1 );
char	*Q_strupr( char *s1 );
char	*Q_strrchr( const char* string, int c );
const char	*Q_stristr( const char *s, const char *find);
qboolean Q_strreplace(char *dest, int destsize, const char *find, const char *replace);

void	Q_strncpyz( char *dest, const char *src, size_t destsize );
void	Q_strcat( char *dest, int size, const char *src );

int Q_PrintStrlen( const char *string );
char *Q_CleanStr( char *string );
int Q_CountChar(const char *string, char tocount);


void	Swap_Init(void);

typedef struct
{
	byte	b0;
	byte	b1;
	byte	b2;
	byte	b3;
	byte	b4;
	byte	b5;
	byte	b6;
	byte	b7;
} qint64;

void	Swap_Init (void);

const char	* QDECL va(const char *format, ...) Q_PRINTF_FUNC(1, 2);

#define TRUNCATE_LENGTH	64
void Com_TruncateLongString( char *buffer, const char *s );


float vectoyaw(const vec3_t vec);
int MusicMood_NameToNum( const char * name );
const char * MusicMood_NumToName( int num );
int EAXMode_NameToNum( const char * name );
const char * EAXMode_NumToName( int num );

char *Info_ValueForKey( const char *s, const char *key );
void Info_RemoveKey( char *s, const char *key );
void Info_RemoveKey_big( char *s, const char *key );
void Info_SetValueForKey( char *s, const char *key, const char *value );
void Info_SetValueForKey_Big( char *s, const char *key, const char *value );
qboolean Info_Validate( const char *s );
void Info_NextPair( const char **s, char *key, char *value );

void	QDECL Com_Error( int level, const char *error, ... ) Q_NO_RETURN Q_PRINTF_FUNC(2, 3);
void	QDECL Com_Printf( const char *msg, ... ) Q_PRINTF_FUNC(1, 2);
void	Com_WPrintf( const char *msg, ... ) Q_PRINTF_FUNC(1, 2);  
void	QDECL Com_DPrintf( const char *msg, ... ) Q_PRINTF_FUNC(1, 2);
void	QDECL Com_DPrintf2( const char *msg, ... ) Q_PRINTF_FUNC(1, 2);
void	QDECL Com_DebugPrintf( const char *msg, ... ) Q_PRINTF_FUNC(1, 2);



#define	CVAR_ARCHIVE		0x0001	
#define	CVAR_USERINFO		0x0002	
#define	CVAR_SERVERINFO		0x0004	
#define	CVAR_SYSTEMINFO		0x0008	
#define	CVAR_INIT		0x0010	
#define	CVAR_LATCH		0x0020	
#define	CVAR_ROM		0x0040	
#define	CVAR_USER_CREATED	0x0080	
#define	CVAR_TEMP		0x0100	
#define CVAR_CHEAT		0x0200	
#define CVAR_NORESTART		0x0400	

#define CVAR_SERVER_CREATED	0x0800	

#define	CVAR_SOUND_LATCH	0x1000		

#define	CVAR_TERRAIN_LATCH	0x2000		

#define	CVAR_SAVEGAME		0x4000		

#define CVAR_VM_CREATED		0x8000	
#define CVAR_PROTECTED		0x10000	
#define CVAR_RESETSTRING	0x20000	
#define CVAR_MODIFIED		0x40000000	
#define CVAR_NONEXISTENT	0x80000000	

typedef struct cvar_s cvar_t;

struct cvar_s {
	char			*name;
	char			*string;
	char			*resetString;		
	char			*latchedString;		
	int				flags;
	qboolean	modified;			
	int				modificationCount;	
	float			value;				
	int				integer;			
	qboolean	validate;
	qboolean	integral;
	float			min;
	float			max;
	char			*description;

	cvar_t *next;
	cvar_t *prev;
	cvar_t *hashNext;
	cvar_t *hashPrev;
	int			hashIndex;

	// Cvar_Get will not overwrite the recorded default.  fgame/q_shared.h:841
	qboolean	defaultRecorded;
};

#define	MAX_CVAR_VALUE_STRING	256

typedef int	cvarHandle_t;

typedef struct {
	cvarHandle_t	handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[MAX_CVAR_VALUE_STRING];
	char		latchedString[MAX_CVAR_VALUE_STRING];
} vmCvar_t;


#include "surfaceflags.h"			

#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2
#define	PLANE_NON_AXIAL	3
#define PLANE_NON_PLANAR 4



static ID_INLINE int PlaneTypeForNormal(vec3_t normal)
{
	if(normal[0] == 1.0)
		return PLANE_X;

	if(normal[1] == 1.0)
		return PLANE_Y;

	if(normal[2] == 1.0)
		return PLANE_Z;

	if(normal[0] == 0.0 && normal[1] == 0.0 && normal[2] == 0.0)
		return PLANE_NON_PLANAR;

	return PLANE_NON_AXIAL;
}

typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			
	byte	signbits;		
	byte	pad[2];
} cplane_t;

typedef enum hitloc_e {
	HITLOC_MISS = -2,
	HITLOC_GENERAL = -1,
	HITLOC_HEAD = 0,
	HITLOC_HELMET,
	HITLOC_NECK,
	HITLOC_TORSO_UPPER,
	HITLOC_TORSO_MID,
	HITLOC_TORSO_LOWER,
	HITLOC_PELVIS,
	HITLOC_R_ARM_UPPER,
	HITLOC_L_ARM_UPPER,
	HITLOC_R_LEG_UPPER,
	HITLOC_L_LEG_UPPER,
	HITLOC_R_ARM_LOWER,
	HITLOC_L_ARM_LOWER,
	HITLOC_R_LEG_LOWER,
	HITLOC_L_LEG_LOWER,
	HITLOC_R_HAND,
	HITLOC_L_HAND,
	HITLOC_R_FOOT,
	HITLOC_L_FOOT,
	NUMBODYLOCATIONS
} hitloc_t;

// `position` and `normal` are part of the struct but retail never writes
typedef struct {
	qboolean	valid;				
	int			surface;			
	int			bone;				
	vec3_t		position;			// declared by retail, never written by it
	vec3_t		normal;				// declared by retail, never written by it
	float		damage_multiplier;	
} tikimdl_intersection_t;

typedef struct {
	int			boneIndex;		
	int			parentIndex;	
	int			surfaceIndex;	
	float		radius;			
	float		length;			
} tikiBoneMapping_t;

typedef struct {
	qboolean	allsolid;	
	qboolean	startsolid;	
	float		fraction;	
	vec3_t		endpos;		
	cplane_t	plane;		
	int			surfaceFlags;	
	int			contents;	
	int			entityNum;	
   struct gentity_s	*ent;		      
	tikimdl_intersection_t intersect; 
} trace_t;


// header and the portal state does not exist in EF2 at all (the retail engine is
// both the x86 and PPC builds, so a byte-exact writer must not swap either.
typedef struct savegamestruct_s {
	int time;
	int svsTime;
	int secondsPlayed;
	char comment[ 64 ];
	char mapName[ 64 ];
	char saveName[ 64 ];
} savegamestruct_t;

typedef char savegamestruct_is_204_bytes[ ( sizeof( savegamestruct_t ) == 204 ) ? 1 : -1 ];

// Retail's markFragment_t is 24 bytes, not the MOHAA 12 the port carries.  Three
typedef struct {
	int		firstPoint;
	int		numPoints;
	int		iIndex;
} markFragment_t;

typedef struct {
	vec3_t p;
	quat_t q;
} bone_t;

typedef struct {
	vec3_t		origin;
	vec3_t		axis[ 3 ];
} orientation_t;



#define KEYCATCH_CONSOLE		0x0001
#define	KEYCATCH_UI				0x0002
#define	KEYCATCH_MESSAGE		0x0004
#define	KEYCATCH_CGAME			0x0008


typedef enum {
	CHAN_AUTO,
	CHAN_LOCAL,				
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_DIALOG,
	CHAN_DIALOG_SECONDARY,
	CHAN_WEAPONIDLE,
	CHAN_MENU,
	CHAN_CINEMATIC,
	CHAN_MUSIC,
	CHAN_TAUNT,
	CHAN_MENU2,
	CHAN_COMBAT1 = 20,
	CHAN_COMBAT2 = 21,
	CHAN_COMBAT3 = 22,
	CHAN_COMBAT4 = 23
} soundChannel_t;

#define DEFAULT_MIN_DIST  -1.0
#define DEFAULT_VOL  -1.0

#define LEVEL_WIDE_MIN_DIST_CUTOFF  10000       
#define LEVEL_WIDE_MIN_DIST  1000000	
#define LEVEL_WIDE_STRING "levelwide"

#define  SOUND_SYNCH             0x1
#define  SOUND_SYNCH_FADE        0x2
#define  SOUND_RANDOM_PITCH_20   0x4
#define  SOUND_RANDOM_PITCH_40   0x8
#define  SOUND_LOCAL_DIALOG      0x10

typedef enum
{
	mood_none,
	mood_normal,
	mood_action,
	mood_suspense,
	mood_mystery,
	mood_success,
	mood_failure,
	mood_surprise,
	mood_special,
	mood_aux1,
	mood_aux2,
	mood_aux3,
	mood_aux4,
	mood_aux5,
	mood_aux6,
	mood_aux7,
	mood_totalnumber
} music_mood_t;

typedef enum
{
	eax_generic,
	eax_paddedcell,
	eax_room,
	eax_bathroom,
	eax_livingroom,
	eax_stoneroom,
	eax_auditorium,
	eax_concerthall,
	eax_cave,
	eax_arena,
	eax_hangar,
	eax_carpetedhallway,
	eax_hallway,
	eax_stonecorridor,
	eax_alley,
	eax_forest,
	eax_city,
	eax_mountains,
	eax_quarry,
	eax_plain,
	eax_parkinglot,
	eax_sewerpipe,
	eax_underwater,
	eax_drugged,
	eax_dizzy,
	eax_psychotic,
	eax_totalnumber
} eax_mode_t;


#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	BYTE2ANGLE(x)	((x)*(360.0/255))
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

#define	SNAPFLAG_RATE_DELAYED	1
#define	SNAPFLAG_NOT_ACTIVE		2	
#define SNAPFLAG_SERVERCOUNT	4	

#define	MAX_CLIENTS			128		
#define MAX_LOCATIONS		64

#define MAX_MAP_BOUNDS          65536
#define MIN_MAP_BOUNDS          ( -MAX_MAP_BOUNDS )
#define MAP_SIZE                ( MAX_MAP_BOUNDS - MIN_MAP_BOUNDS )

#define	GENTITYNUM_BITS		10		
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)

#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)


#define MAX_EXTRA_ENTITIES_FROM_GAME	100

// Measured from the unstripped Mac PEF, not guessed:
#define MAX_SERVER_SOUNDS			32
#define MAX_SERVER_SOUNDS_BITS		6

#define SOUND_CHANNEL_BITS			6
#define SOUND_ENTITYNUM_BITS		10		
#define SOUND_INDEX_BITS			9		

#define	MAX_MODELS			512		
#define	MAX_SOUNDS			512		
#define	MAX_IMAGES			128
#define MAX_LIGHTSTYLES		32
#define MAX_WEAPONS			64

#define	MAX_CONFIGSTRINGS	2300

#define MAX_SUBTITLES 4

#define	CS_SERVERINFO		0		
#define	CS_SYSTEMINFO		1		
#define	CS_NAME 		2		 

#define	RESERVED_CONFIGSTRINGS	2	

#define	MAX_GAMESTATE_CHARS	36000
typedef struct {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;

typedef enum {
   GT_SINGLE_PLAYER,
   GT_MULTIPLAYER,
   GT_BOT_SINGLE_PLAYER,

	GT_FFA,				
	GT_TOURNAMENT,		


	GT_TEAM,			
	GT_CTF,				
	GT_1FCTF,			
	GT_OBELISK,			
	GT_HARVESTER,

	GT_MAX_GAME_TYPE
} gametype_t;


// `pitch` is renamed to retail's `pitch_modifier` because the two are NOT interchangeable
typedef struct server_sound_s {
	vec3_t origin;			
	int entity_number;		
	int channel;			
	short int sound_index;	
	float volume;			
	float min_dist;			
	qboolean stop_flag;		
	float pitch_modifier;	
} server_sound_t;

#define MAX_LIGHTING_GROUPS			32

#define LIGHTING_GROUP_NAME_SIZE	64

typedef enum {
	WEATHER_NONE,
	WEATHER_RAIN,
	WEATHER_RAIN_PLAIN,
	WEATHER_SNOW
} weather_t;

#define DYNAMIC_LIGHT_NO_DEFAULT		255
#define DYNAMIC_LIGHT_MAX_INTENSITY		254

typedef struct worldState_s {
	byte			dynamic_light_intensities[ MAX_LIGHTING_GROUPS ];			

	byte			dynamic_light_default_intensities[ MAX_LIGHTING_GROUPS ];	

	vec3_t			wind_direction;												
	float			wind_intensity;												

	weather_t		weather_type;												
	int				weather_intensity;											

	float			time_scale;													
} worldState_t;

typedef struct usereyes_s {
	signed char ofs[ 3 ];
	float angles[ 2 ];
} usereyes_t;


typedef enum
{
	SPECTATOR_TYPE_NONE,
	SPECTATOR_TYPE_NORMAL,
	SPECTATOR_TYPE_FREEFORM,
	SPECTATOR_TYPE_FOLLOW,
	SPECTATOR_TYPE_ANY
} SpectatorTypes;

#define	MAX_STATS				96
#define	MAX_ACTIVE_ITEMS		8
#define	MAX_AMMO				16
#define	MAX_AMMOCOUNT			16
#define	MAX_INVENTORY			32
#define	MAX_ITEMS				(MAX_INVENTORY + MAX_AMMO + 16)
#define	MAX_PERSISTANT			16
#define	MAX_POWERUPS			16

#define	MAX_PS_EVENTS			2

#define	MAX_ARENA_INFO			32
#define	MAX_TEAM_INFO			64
#define	MAX_ARCHETYPES			383
#define	MAX_OBJECTIVE_NAMES		5
#define	MAX_GENERAL_STRINGS		256

#define PS_PMOVEFRAMECOUNTBITS	6




#define INFORMATION1_SHOW ( 1<<0 )
#define INFORMATION2_SHOW ( 1<<1 )
#define INFORMATION3_SHOW ( 1<<2 )
#define INFORMATION4_SHOW ( 1<<3 )
#define INFORMATION5_SHOW ( 1<<4 )
#define INFORMATION6_SHOW ( 1<<5 )
#define INFORMATION7_SHOW ( 1<<6 )
#define INFORMATION8_SHOW ( 1<<7 )

#define INFORMATION1 1
#define INFORMATION2 2
#define INFORMATION3 3
#define INFORMATION4 4
#define INFORMATION5 5
#define INFORMATION6 6
#define INFORMATION7 7
#define INFORMATION8 8


#define MISSION_FAILED  1
#define MISSION_SUCCESS 2

typedef struct playerState_s {
	int			commandTime;	
	int			pm_type;
	int			bobCycle;		
	int			pm_flags;		
	int			pm_time;
	int			pm_runtime;		

	vec3_t		origin;
	vec3_t		velocity;
	int			gravity;
	int			speed;
	int			jumpvelocity;
	int			crouchjumpvelocity;
	qboolean	crouchjumpset;
	int			pm_stopspeed;
	int			pm_airaccelerate;
	int			pm_wateraccelerate;
	int			pm_friction;
	int			pm_waterfriction;
	int			pm_accelerate;

	int			pm_defaultviewheight;

	int			delta_angles[3];	
	float		leanDelta;

	int			groundEntityNum;	
	qboolean	walking;
	qboolean	jumped;
	qboolean	groundPlane;
	qboolean	instantJump;
	qboolean	strafeJumpingAllowed;
	int			feetfalling;
	vec3_t		falldir;
	trace_t		groundTrace;

	int			clientNum;		

	vec3_t		viewangles;		
	int			viewheight;

	int			stats[MAX_STATS];
	int			activeItems[MAX_ACTIVE_ITEMS];
	int			inventory_name_index[MAX_INVENTORY];
	int			inventory_weapon_ammo_index[MAX_INVENTORY];
	int			inventory_weapon_required_ammo[MAX_INVENTORY];
	int			ammo_in_clip[MAX_INVENTORY];
	int			ammo_name_index[MAX_AMMO];
	int			ammo_amount[MAX_AMMOCOUNT];
	int			max_ammo_amount[MAX_AMMOCOUNT];

	int			current_music_mood;
	int			fallback_music_mood;
	float		music_volume;
	float		music_volume_fade_time;
	qboolean	allowMusicDucking;

	int			reverb_type;
	float		reverb_level;

	float		blend[4];			
	float		fov;				

	vec3_t		camera_origin;	
	vec3_t		cameraFocalPoint;
	qboolean	useCameraFocalPoint;
	vec3_t		camera_angles;	
	float		camera_time;	

	vec3_t		camera_offset;	
	vec3_t		damage_angles;	
	int			camera_flags;	
	vec3_t		vehicleoffset;
	qboolean	in_vehicle;

	int			dialogEntnum;
	int			dialogSoundIndex;
	int			dialogTextSoundIndex;

	int				objectiveNameIndex;
	unsigned int	objectiveStates;
	unsigned int	informationStates;

	unsigned int	missionStatus;

	unsigned int	viewMode;

	int			ping;			
} playerState_t;



// (personal/evidence/ef2-sdk/dlls/game/q_shared.h:1528-1545).  This is NOT MOHAA's table: MOHAA put
#define BUTTON_ATTACKRIGHT_BITINDEX     0
#define BUTTON_ATTACKLEFT_BITINDEX      1
#define BUTTON_RELOAD_BITINDEX          2           
#define BUTTON_RUN_BITINDEX             3
#define BUTTON_HOLSTERWEAPON_BITINDEX   4
#define BUTTON_USE_BITINDEX             5
#define BUTTON_TALK_BITINDEX            6           
#define BUTTON_ANY_BITINDEX             7           
#define BUTTON_DROP_RUNE_BITINDEX       8
#define BUTTON_TRANSFER_ENERGY_BITINDEX 8

#define USERCMD_BUTTON_MAX 16

#define BUTTON_ATTACKRIGHT      (1 << BUTTON_ATTACKRIGHT_BITINDEX)
#define BUTTON_ATTACKLEFT       (1 << BUTTON_ATTACKLEFT_BITINDEX)
#define BUTTON_RELOAD           (1 << BUTTON_RELOAD_BITINDEX)       
#define BUTTON_RUN              (1 << BUTTON_RUN_BITINDEX)
#define BUTTON_HOLSTERWEAPON    (1 << BUTTON_HOLSTERWEAPON_BITINDEX)
#define BUTTON_USE              (1 << BUTTON_USE_BITINDEX)
#define BUTTON_TALK             (1 << BUTTON_TALK_BITINDEX)         
#define BUTTON_ANY              (1 << BUTTON_ANY_BITINDEX)          
#define BUTTON_DROP_RUNE        (1 << BUTTON_DROP_RUNE_BITINDEX)
#define BUTTON_TRANSFER_ENERGY  (1 << BUTTON_TRANSFER_ENERGY_BITINDEX)

typedef enum {
    WEAPON_COMMAND_NONE,
	WEAPON_COMMAND_USE_PISTOL,
	WEAPON_COMMAND_USE_RIFLE,
	WEAPON_COMMAND_USE_SMG,
	WEAPON_COMMAND_USE_MG,
	WEAPON_COMMAND_USE_GRENADE,
	WEAPON_COMMAND_USE_HEAVY,
	WEAPON_COMMAND_USE_ITEM1,
	WEAPON_COMMAND_USE_ITEM2,
	WEAPON_COMMAND_USE_ITEM3,
	WEAPON_COMMAND_USE_ITEM4,
    WEAPON_COMMAND_USE_PREV_WEAPON,
    WEAPON_COMMAND_USE_NEXT_WEAPON,
    WEAPON_COMMAND_USE_LAST_WEAPON,
	WEAPON_COMMAND_HOLSTER,
    WEAPON_COMMAND_DROP,
} weaponcommand_t;

#define WEAPON_COMMAND_MAX_VER6		15
#define WEAPON_COMMAND_MAX_VER17	31

static inline unsigned int GetWeaponCommandMask(unsigned int maxCmds) {
	return maxCmds << 7;
}

static inline unsigned int GetWeaponCommand(unsigned int buttons, unsigned int maxCmds) {
    return (buttons & (maxCmds << 7)) >> 7;
}

#define	MOVE_RUN			120			

typedef struct usercmd_s {
	int			serverTime;
	byte		msec;
	short		buttons;
	byte		weapon;
	short		angles[3];
	short		deltaAngles[3];		
	short		choffset[2];
	vec3_t		realvieworigin;
	vec3_t		realviewangles;
	qboolean	thirdperson;
	signed char	forwardmove, rightmove, upmove, lean;
} usercmd_t;

#ifdef __cplusplus
static_assert(sizeof(usercmd_t) == 60, "EF2's usercmd_t is exactly 60 bytes");
#endif


#define  MDL_ANIM_DELTA_DRIVEN   ( 1 << 0 )
#define  MDL_ANIM_DEFAULT_ANGLES ( 1 << 3 )
#define  MDL_ANIM_NO_TIMECHECK   ( 1 << 4 )

#define	SOLID_BMODEL	0xffffff

#define	RF_THIRD_PERSON			(1<<0)   	
#define	RF_FIRST_PERSON			(1<<1)		
#define	RF_DEPTHHACK			(1<<2)		
#define	RF_VIEWLENSFLARE		(1<<3)		
#define	RF_FRAMELERP			(1<<4)		
#define	RF_BEAM					(1<<5)		
#define RF_SHADOW_FROM_BIP01	(1<<6)		 
#define RF_DONTDRAW				(1<<7)		
#define RF_LENSFLARE			(1<<8)		
#define RF_EXTRALIGHT			(1<<9)		
#define RF_DETAIL				(1<<10)		
#define RF_SHADOW				(1<<11)		
#define	RF_PORTALSURFACE		(1<<12)   	
#define	RF_SKYORIGIN   			(1<<13)   	
#define	RF_SKYENTITY   			(1<<14)   	
#define	RF_LIGHTOFFSET   		(1<<15)   	
#define	RF_CUSTOMSHADERPASS		(1<<16)		
#define	RF_MINLIGHT				(1<<17)		
#define	RF_FULLBRIGHT			(1<<18)		
#define RF_LIGHTING_ORIGIN		(1<<19)		
#define	RF_SHADOW_PLANE			(1<<20)		
#define	RF_WRAP_FRAMES			(1<<21)		
#define RF_PORTALENTITY			(1<<22)		
#define RF_DUALENTITY			(1<<23)		
#define RF_ADDITIVE_DLIGHT		(1<<24)		
#define RF_LIGHTSTYLE_DLIGHT	(1<<25)		
#define RF_SHADOW_PRECISE		(1<<26)		
#define RF_INVISIBLE			(1<<27)		
#define RF_WEAPONMODEL			(1<<28)		
#define RF_FORCE_ALPHA			(1<<29)		
#define RF_FORCE_ALPHA_EFFECTS	(1<<30)		
#define RF_CHILDREN_DONT_INHERIT_ALPHA	(1<<31)		 
#define  RF_FLAGS_NOT_INHERITED ( RF_LENSFLARE | RF_VIEWLENSFLARE | RF_BEAM | RF_EXTRALIGHT | RF_SKYORIGIN | RF_SHADOW | RF_SHADOW_PRECISE | RF_SHADOW_PLANE | RF_LIGHTOFFSET | RF_CHILDREN_DONT_INHERIT_ALPHA )

#define BEAM_LIGHTNING_EFFECT   (1<<0)
#define BEAM_USEMODEL           (1<<1)
#define BEAM_PERSIST_EFFECT     (1<<2)
#define BEAM_SPHERE_EFFECT      (1<<3)
#define BEAM_RANDOM_DELAY       (1<<4)
#define BEAM_TOGGLE             (1<<5)
#define BEAM_RANDOM_TOGGLEDELAY (1<<6)
#define BEAM_WAVE_EFFECT        (1<<7)
#define BEAM_USE_NOISE          (1<<8)
#define BEAM_PARENT             (1<<9)
#define BEAM_TILESHADER         (1<<10)
#define BEAM_OFFSET_ENDPOINTS   (1<<11)
// those flags do not exist in EF2 -- retail's CG_AddBeams @0x00002930 tests only
// 0x2/0x4/0x8/0x20/0x40 and never 0x1000/0x2000/0x4000 -- and the MOHAA branches
#define BEAM_WAVE               (1<<12)
#define BEAM_FULLWAVE           (1<<13)

typedef struct frameInfo_s {
    int index;
    float time;
    float weight;
} frameInfo_t;

typedef enum {
	TR_STATIONARY,
	TR_INTERPOLATE,				
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_SINE,					
	TR_GRAVITY,
	TR_LERP
} trType_t;

typedef struct {
	trType_t	trType;
	int		trTime;
	int		trDuration;			
	vec3_t	trBase;
	vec3_t	trDelta;			   
} trajectory_t;

static_assert(sizeof(trajectory_t) == 36, "trajectory_t must be 36 bytes (retail EF2)");
static_assert(offsetof(trajectory_t, trBase) == 12, "trajectory_t layout drift");
static_assert(offsetof(trajectory_t, trDelta) == 24, "trajectory_t layout drift");

#define NUM_BONE_CONTROLLERS 5

#define NUM_MORPH_CONTROLLERS 10

typedef struct {
    int   index;   
    float percent; 
} morph_controller_t;

#define MAX_MODEL_SURFACES   32 
#define NUM_EFFECTS_ANIMS	4

#define  MDL_SURFACE_SKINOFFSET_BIT0  ( 1 << 0 )
#define  MDL_SURFACE_SKINOFFSET_BIT1  ( 1 << 1 )
#define  MDL_SURFACE_NODRAW           ( 1 << 2 )
#define  MDL_SURFACE_SURFACETYPE_BIT0 ( 1 << 3 )
#define  MDL_SURFACE_SURFACETYPE_BIT1 ( 1 << 4 )
#define  MDL_SURFACE_SURFACETYPE_BIT2 ( 1 << 5 )
#define  MDL_SURFACE_CROSSFADE_SKINS  ( 1 << 6 )
#define  MDL_SURFACE_SKIN_NO_DAMAGE   ( 1 << 7 )

#define CROUCH_HEIGHT		36
#define CROUCH_EYE_HEIGHT	30
#define STAND_HEIGHT			72
#define STAND_EYE_HEIGHT	66

#define MAX_FRAMEINFOS			16
#define FRAMEINFO_BLEND			( MAX_FRAMEINFOS >> 1 )


typedef struct entityState_s {
	int		number;			
	int		instanceNumber;
	int		eType;			
	int		eFlags;

	trajectory_t	pos;	
	trajectory_t	apos;	

	vec3_t	netorigin;		
	vec3_t	origin;
	vec3_t	origin2;

	vec3_t	netangles;		
	vec3_t	angles;

	vec3_t	viewangles;		

	unsigned int	constantLight;	

	int		loopSound;		
	float	loopSoundVolume;
	float	loopSoundMinDist;

	int		parent;			
	int		tag_num;		
	qboolean	attach_use_angles;
	vec3_t	attach_offset;
	vec3_t	attach_angles_offset;

	int		modelindex;
	int		viewmodelindex;
	int		skinNum;
	int		customShader;
	int		customEmitter;
	float	animationRate;
	int		anim;
	int		frame;
	int		crossblend_time;	

	int		torso_anim;
	int		torso_frame;
	int		torso_crossblend_time;	

	int		bone_tag[ NUM_BONE_CONTROLLERS ];
	vec3_t	bone_angles[ NUM_BONE_CONTROLLERS ];
	vec4_t	bone_quat[ NUM_BONE_CONTROLLERS ];	

	morph_controller_t morph_controllers[ NUM_MORPH_CONTROLLERS ];

	byte	surfaces[ MAX_MODEL_SURFACES ];

	int		clientNum;		

	int		groundEntityNum;	
	int		solid;			

	float	scale;
	float	alpha;

	int				renderfx;

	unsigned int	affectingViewModes;

	int				archeTypeIndex;
	int				missionObjective;

	int				infoIcon;

	int		effectsAnims[ NUM_EFFECTS_ANIMS ];

	int		bindparent;		
	float	quat[4];		
	float	mat[3][3];		
} entityState_t;

//     0x24C (588) is bindparent, the first of the three "not sent over" tail
static_assert(sizeof(entityState_t) == 644, "entityState_t must be 644 bytes (retail EF2)");
static_assert(offsetof(entityState_t, pos) == 16, "entityState_t layout drift");
static_assert(offsetof(entityState_t, apos) == 52, "entityState_t layout drift");
static_assert(offsetof(entityState_t, netorigin) == 88, "entityState_t layout drift");
static_assert(offsetof(entityState_t, viewangles) == 148, "entityState_t layout drift");
static_assert(offsetof(entityState_t, constantLight) == 160, "entityState_t layout drift");
static_assert(offsetof(entityState_t, parent) == 176, "entityState_t layout drift");
static_assert(offsetof(entityState_t, animationRate) == 232, "entityState_t layout drift");
static_assert(offsetof(entityState_t, anim) == 236, "entityState_t layout drift");
static_assert(offsetof(entityState_t, torso_anim) == 248, "entityState_t layout drift");
static_assert(offsetof(entityState_t, bone_tag[0]) == 260, "entityState_t layout drift");
static_assert(offsetof(entityState_t, morph_controllers[0].index) == 420, "entityState_t layout drift");
static_assert(offsetof(entityState_t, morph_controllers[9].index) == 492, "entityState_t layout drift");
static_assert(offsetof(entityState_t, surfaces[0]) == 500, "entityState_t layout drift");
static_assert(offsetof(entityState_t, clientNum) == 532, "entityState_t layout drift");
static_assert(offsetof(entityState_t, groundEntityNum) == 536, "entityState_t layout drift");
static_assert(offsetof(entityState_t, scale) == 544, "entityState_t layout drift");
static_assert(offsetof(entityState_t, alpha) == 548, "entityState_t layout drift");
static_assert(offsetof(entityState_t, renderfx) == 552, "entityState_t layout drift");
static_assert(offsetof(entityState_t, affectingViewModes) == 556, "entityState_t layout drift");
static_assert(offsetof(entityState_t, archeTypeIndex) == 560, "entityState_t layout drift");
static_assert(offsetof(entityState_t, missionObjective) == 564, "entityState_t layout drift");
static_assert(offsetof(entityState_t, infoIcon) == 568, "entityState_t layout drift");
static_assert(offsetof(entityState_t, effectsAnims[0]) == 572, "entityState_t layout drift");
static_assert(offsetof(entityState_t, effectsAnims[3]) == 584, "entityState_t layout drift");
static_assert(offsetof(entityState_t, bindparent) == 588, "entityState_t layout drift");

// groundTrace / stats / ping, which retail does not network.
//   4-byte difference from retail is inherited, not introduced: the SDK's
//   so msg.cpp must carry this build's offsets, not retail's.  Substituting
#define PS_RETAIL_DELTA (sizeof(trace_t) - 100)

static_assert(sizeof(cplane_t) == 20, "cplane_t must be 20 bytes (retail EF2)");
static_assert(sizeof(tikimdl_intersection_t) == 40, "tikimdl_intersection_t must be 40 bytes (retail EF2)");
static_assert(offsetof(trace_t, entityNum) == 52, "trace_t layout drift");
static_assert(offsetof(trace_t, ent) == 56, "trace_t layout drift");
static_assert(offsetof(trace_t, intersect) == 56 + sizeof(void *), "trace_t layout drift");
static_assert(sizeof(trace_t) == 96 + sizeof(void *), "trace_t must be retail's 100 bytes plus one pointer");

static_assert(offsetof(playerState_t, commandTime) == 0, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_type) == 4, "playerState_t layout drift");
static_assert(offsetof(playerState_t, bobCycle) == 8, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_flags) == 12, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_time) == 16, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_runtime) == 20, "playerState_t layout drift");
static_assert(offsetof(playerState_t, origin) == 24, "playerState_t layout drift");
static_assert(offsetof(playerState_t, velocity) == 36, "playerState_t layout drift");
static_assert(offsetof(playerState_t, gravity) == 48, "playerState_t layout drift");
static_assert(offsetof(playerState_t, speed) == 52, "playerState_t layout drift");
static_assert(offsetof(playerState_t, jumpvelocity) == 56, "playerState_t layout drift");
static_assert(offsetof(playerState_t, crouchjumpvelocity) == 60, "playerState_t layout drift");
static_assert(offsetof(playerState_t, crouchjumpset) == 64, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_stopspeed) == 68, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_airaccelerate) == 72, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_wateraccelerate) == 76, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_friction) == 80, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_waterfriction) == 84, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_accelerate) == 88, "playerState_t layout drift");
static_assert(offsetof(playerState_t, pm_defaultviewheight) == 92, "playerState_t layout drift");
static_assert(offsetof(playerState_t, delta_angles) == 96, "playerState_t layout drift");
static_assert(offsetof(playerState_t, leanDelta) == 108, "playerState_t layout drift");
static_assert(offsetof(playerState_t, groundEntityNum) == 112, "playerState_t layout drift");
static_assert(offsetof(playerState_t, walking) == 116, "playerState_t layout drift");
static_assert(offsetof(playerState_t, jumped) == 120, "playerState_t layout drift");
static_assert(offsetof(playerState_t, groundPlane) == 124, "playerState_t layout drift");
static_assert(offsetof(playerState_t, instantJump) == 128, "playerState_t layout drift");
static_assert(offsetof(playerState_t, strafeJumpingAllowed) == 132, "playerState_t layout drift");
static_assert(offsetof(playerState_t, feetfalling) == 136, "playerState_t layout drift");
static_assert(offsetof(playerState_t, falldir) == 140, "playerState_t layout drift");
static_assert(offsetof(playerState_t, groundTrace) == 152, "playerState_t layout drift");

static_assert(offsetof(playerState_t, clientNum) == 252 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, viewangles) == 256 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, viewheight) == 268 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, stats) == 272 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(sizeof(((playerState_t *)0)->stats) == 96 * sizeof(int), "MAX_STATS must be 96 (retail EF2)");
static_assert(offsetof(playerState_t, activeItems) == 656 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, inventory_name_index) == 688 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, inventory_weapon_ammo_index) == 816 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, inventory_weapon_required_ammo) == 944 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, ammo_in_clip) == 1072 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, ammo_name_index) == 1200 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, ammo_amount) == 1264 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, max_ammo_amount) == 1328 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, current_music_mood) == 1392 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, fallback_music_mood) == 1396 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, music_volume) == 1400 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, music_volume_fade_time) == 1404 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, allowMusicDucking) == 1408 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, reverb_type) == 1412 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, reverb_level) == 1416 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, blend) == 1420 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, fov) == 1436 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, camera_origin) == 1440 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, cameraFocalPoint) == 1452 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, useCameraFocalPoint) == 1464 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, camera_angles) == 1468 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, camera_time) == 1480 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, camera_offset) == 1484 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, damage_angles) == 1496 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, camera_flags) == 1508 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, vehicleoffset) == 1512 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, in_vehicle) == 1524 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, dialogEntnum) == 1528 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, dialogSoundIndex) == 1532 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, dialogTextSoundIndex) == 1536 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, objectiveNameIndex) == 1540 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, objectiveStates) == 1544 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, informationStates) == 1548 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, missionStatus) == 1552 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, viewMode) == 1556 + PS_RETAIL_DELTA, "playerState_t layout drift");
static_assert(offsetof(playerState_t, ping) == 1560 + PS_RETAIL_DELTA, "playerState_t layout drift");

static_assert(sizeof(playerState_t) == 1564 + PS_RETAIL_DELTA, "playerState_t must be retail's 1564 bytes plus the trace_t pointer delta");

static_assert(sizeof(playerState_t) - 1564 == sizeof(trace_t) - 100,
	"playerState_t has drifted from retail by more than the trace_t::ent pointer delta");

typedef enum {
	CENTERPRINT_IMPORTANCE_NORMAL,
	CENTERPRINT_IMPORTANCE_HIGH,
	CENTERPRINT_IMPORTANCE_CRITICAL
} CenterPrintImportance;

typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED, 	
	CA_AUTHORIZING,		
	CA_CONNECTING,		
	CA_CHALLENGING,		
	CA_CONNECTED,		
	CA_LOADING,			
	CA_PRIMED,			
	CA_ACTIVE,			
	CA_CINEMATIC		
} connstate_t;

typedef struct qtime_s {
	int		tm_sec;
	int		tm_min;
	int		tm_hour;
	int		tm_mday;
	int		tm_mon;
	int		tm_year;
	int		tm_wday;
	int		tm_yday;
	int		tm_isdst;
} qtime_t;

typedef struct {
	float			start[ 3 ];
	float			end[ 3 ];
	float			color[ 3 ];
	float			alpha;
	float			width;
	unsigned short	factor;
	unsigned short	pattern;
} debugline_t;

typedef struct {
	char	szText[ 64 ];
	float	pos[ 3 ];
	float	scale;
	float	color[ 4 ];
} debugstring_t;

enum hdalign_e
{
    HUD_ALIGN_X_LEFT = 0,
    HUD_ALIGN_X_CENTER = 1,
    HUD_ALIGN_X_RIGHT = 2,
    HUD_ALIGN_Y_TOP = 0,
    HUD_ALIGN_Y_CENTER = 1,
    HUD_ALIGN_Y_BOTTOM = 2,
};

typedef struct {
	float		g_vCurrentVMPosOffset[ 3 ];
} clientAnim_t;

#define GLYPH_START 0
#define GLYPH_END 255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND 127
#define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1
typedef struct {
  int height;       
  int top;          
  int bottom;       
  int pitch;        
  int xSkip;        
  int imageWidth;   
  int imageHeight;  
  float s;          
  float t;          
  float s2;
  float t2;
  qhandle_t glyph;  
  char shaderName[32];
} glyphInfo_t;

typedef struct {
  glyphInfo_t glyphs [GLYPHS_PER_FONT];
  float glyphScale;
  char name[MAX_QPATH];
} fontInfo_t;

#define Square(x) ((x)*(x))


#define AS_LOCAL			0
#define AS_MPLAYER			99
#define AS_GLOBAL			2
#define AS_FAVORITES		3
#define AS_GAMESPY			1	


typedef enum {
	FMV_IDLE,
	FMV_PLAY,		
	FMV_EOF,		
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

typedef enum _flag_status {
	FLAG_ATBASE = 0,
	FLAG_TAKEN,			
	FLAG_TAKEN_RED,		
	FLAG_TAKEN_BLUE,	
	FLAG_DROPPED
} flagStatus_t;

#define	MAX_GLOBAL_SERVERS				2048
#define	MAX_OTHER_SERVERS					128
#define MAX_PINGREQUESTS					32
#define MAX_SERVERSTATUSREQUESTS	16

#define SAY_ALL		0
#define SAY_TEAM	1
#define SAY_TELL	2

#define CDKEY_LEN 16
#define CDCHKSUM_LEN 2

#define FLOAT_TO_INT( x, fracbits ) ( ( x ) * ( 1 << ( fracbits ) ) )

#define FLOAT_TO_PKT( x, dest, wholebits, fracbits )                            \
   {                                                                            \
   if ( ( x ) >= ( 1 << ( wholebits ) ) )                                       \
         {                                                                         \
      ( dest ) = FLOAT_TO_INT( ( 1 << ( wholebits ) ), ( fracbits ) ) - 1; \
         }                                                                         \
      else if ( ( x ) < 0 )                                                        \
      {                                                                         \
      ( dest ) = 0;                                                             \
      }                                                                         \
      else                                                                         \
      {                                                                         \
      ( dest ) = FLOAT_TO_INT( ( x ), ( fracbits ) );               \
      }                                                                         \
   }

#define SIGNED_FLOAT_TO_PKT( x, dest, wholebits, fracbits )                   \
   {                                                                          \
   float temp_x;                                                              \
   temp_x = ( x ) + ( 1 << ( wholebits ) );                                   \
   if ( temp_x >= ( 1 << ( ( wholebits ) + 1 ) ) )                            \
      ( dest ) = FLOAT_TO_INT( ( 1 << ( ( wholebits ) + 1 ) ), ( fracbits ) ) - 1;    \
      else if ( temp_x < 0 )                                                     \
      (dest) = 0;                                                             \
      else                                                                       \
      ( dest ) = FLOAT_TO_INT( temp_x, ( fracbits ) );                        \
   }

#define INT_TO_FLOAT( x, wholebits, fracbits ) ( ( float )( ( ( float )( x ) ) / ( float )( 1 << ( fracbits ) ) - ( float )( 1 << ( wholebits ) ) ) )
#define UINT_TO_FLOAT( x, fracbits ) ( ( float )( ( ( float )( x ) ) / ( float )( 1 << ( fracbits ) ) ) )

#define TRANSLATION_TO_PKT( x, dest ) FLOAT_TO_PKT( ( x ), ( dest ), 4, 11 )
#define PKT_TO_TRANSLATION( x ) UINT_TO_FLOAT( ( x ), 11 )

#define OFFSET_TO_PKT( x, dest ) FLOAT_TO_PKT( ( x ), ( dest ), 1, 14 )
#define PKT_TO_OFFSET( x ) UINT_TO_FLOAT( ( x ), 14 )

#define ROTATE_TO_PKT( x, dest ) FLOAT_TO_PKT( ( x ), ( dest ), 9, 6 )
#define PKT_TO_ROTATE( x ) UINT_TO_FLOAT( ( x ), 6 )

#define BASE_TO_PKT( x, dest ) SIGNED_FLOAT_TO_PKT( ( x ), ( dest ), 3, 4 )
#define PKT_TO_BASE( x ) INT_TO_FLOAT( ( x ), 3, 4 )

#define AMPLITUDE_TO_PKT( x, dest ) FLOAT_TO_PKT( ( x ), ( dest ), 4, 4 )
#define PKT_TO_AMPLITUDE( x ) UINT_TO_FLOAT( ( x ), 4 )

#define PHASE_TO_PKT( x, dest ) SIGNED_FLOAT_TO_PKT( ( x ), ( dest ), 3, 4 )
#define PKT_TO_PHASE( x ) INT_TO_FLOAT( ( x ), 3, 4 )

#define FREQUENCY_TO_PKT( x, dest ) FLOAT_TO_PKT( ( x ), ( dest ), 4, 4 )
#define PKT_TO_FREQUENCY( x ) UINT_TO_FLOAT( ( x ), 4 )

#define BEAM_PARM_TO_PKT( x, dest ) FLOAT_TO_PKT( ( x ), ( dest ), 4, 4 )
#define PKT_TO_BEAM_PARM( x ) UINT_TO_FLOAT( ( x ), 4 )

#define STUB() printf( "STUB: function %s in %s line %d.\n", __FUNCTION__, __FILE__, __LINE__ )
#define STUB_DESC( description ) printf( "STUB: %s in %s line %d.\n", description, __FILE__, __LINE__ )
#define UNIMPLEMENTED() Com_Printf( "FIXME: (%s) is unimplemented (file %s:%d)\n", __FUNCTION__, __FILE__, __LINE__ )

#if defined(__cplusplus)
}
#endif

#ifdef __cplusplus
#include <chrono>

using qcclock_t = std::chrono::steady_clock;
using qctime_t = qcclock_t::time_point;
using qctimedelta_t = qcclock_t::duration;

#ifdef __cplusplus
extern "C"
#endif
unsigned int GenerateHashForName( const char* name, qboolean caseSensitive, unsigned int maxHash );

static inline size_t Q_NonNegCount(int n)
{
    return n > 0 ? (size_t)n : (size_t)0;
}
#endif
