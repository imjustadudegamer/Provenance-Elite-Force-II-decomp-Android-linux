//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/q_shared.h                                    $
// $Revision:: 167                                                            $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 9:42a                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// included first by ALL program modules.  A user mod should never modify
// this file
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#ifndef __Q_SHARED_H__
#define __Q_SHARED_H__



#include "gamedefs.h"

#define	DEVELOPER_NAME			"Ritual Entertainment"

#define	QDECL	

#ifdef _WIN32

#pragma warning(disable : 4018)     
#pragma warning(disable : 4051)     
#pragma warning(disable : 4057)		
#pragma warning(disable : 4100)		
#pragma warning(disable : 4115)     
#pragma warning(disable : 4127)		
#pragma warning(disable : 4136)     
#pragma warning(disable : 4201)     
#pragma warning(disable : 4244)     
#pragma warning(disable : 4305)		
#pragma warning(disable : 4512)     
#pragma warning(disable : 4514)     
#pragma warning(disable : 4710)     
#pragma warning(disable : 4711)		


#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32


#endif


#if (defined _M_IX86 || defined __i386__) && !defined C_ONLY && !defined __sun__
#define id386	1
#else
#define id386	0
#endif


#ifdef WIN32

#define	MAC_STATIC

#ifdef NDEBUG
#ifdef _M_IX86
#define	CPUSTRING	"win-x86"
#elif defined _M_ALPHA
#define	CPUSTRING	"win-AXP"
#endif
#else
#ifdef _M_IX86
#define	CPUSTRING	"win-x86-debug"
#elif defined _M_ALPHA
#define	CPUSTRING	"win-AXP-debug"
#endif
#endif


#define	PATH_SEP '\\'

#endif






#ifdef __linux__

#define	MAC_STATIC

#ifdef __i386__
#define	CPUSTRING	"linux-i386"
#elif defined __axp__
#define	CPUSTRING	"linux-alpha"
#else
#define	CPUSTRING	"linux-other"
#endif

#define	PATH_SEP '/'

#endif


#define DATATYPE_SCHAR_MIN		-128
#define DATATYPE_SCHAR_MAX		 127

#define DATATYPE_UCHAR_MIN		 0
#define DATATYPE_UCHAR_MAX		 255

#define DATATYPE_SSHORT_MIN	-32768
#define DATATYPE_SSHORT_MAX	 32767

#define DATATYPE_USHORT_MIN	 0
#define DATATYPE_USHORT_MAX	 65535

#define DATATYPE_SINT_MIN		-2147483648
#define DATATYPE_SINT_MAX		 2147483647

#define DATATYPE_UINT_MIN		 0
#define DATATYPE_UINT_MAX		 4294967295

#define DATATYPE_SLONG_MIN		-2147483648
#define DATATYPE_SLONG_MAX		 2147483647

#define DATATYPE_ULONG_MIN		 0
#define DATATYPE_ULONG_MAX		 4294967295


#ifdef __cplusplus
extern "C"
   {
#endif

#ifndef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

#define bound(a,minval,maxval)  ( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

typedef unsigned char 		byte;

#ifdef __cplusplus
typedef int qboolean;
#define qfalse (0)
#define qtrue (!qfalse)
#else
typedef enum {qfalse, qtrue}	qboolean;
#endif

typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;


#ifndef NULL
#define NULL ((void *)0)
#endif

#define	MAX_QINT			0x7fffffff
#define	MIN_QINT			(-MAX_QINT-1)


#define	PITCH				0		
#define	YAW					1		
#define	ROLL				2		

#define AXIS_FORWARD_VECTOR	0
#define AXIS_RIGHT_VECTOR		1
#define AXIS_UP_VECTOR			2

#define	MAX_STRING_CHARS	1024	
#define	MAX_STRING_TOKENS	256		
#define	MAX_TOKEN_CHARS		1024	
#define MAX_ARGUMENTS		512		

#define	MAX_INFO_STRING		1024
#define	MAX_INFO_KEY		1024
#define	MAX_INFO_VALUE		1024

#define	BIG_INFO_STRING		8192  
#define	BIG_INFO_KEY		8192
#define	BIG_INFO_VALUE		8192

#define	MAX_QPATH			64		
#define	MAX_OSPATH			256		

#define	MAX_NAME_LENGTH		32		

#define	MAX_LIGHTING_GROUPS		32
#define  MAX_STATIC_LOD_MODLES	256

#define MAX_EXTRA_ENTITIES_FROM_GAME  100

typedef enum {
	CENTERPRINT_IMPORTANCE_NORMAL,
	CENTERPRINT_IMPORTANCE_HIGH,
	CENTERPRINT_IMPORTANCE_CRITICAL
} CenterPrintImportance;


#define AS_LOCAL			0
#define AS_GLOBAL			1
#define AS_FAVORITES		2

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


typedef enum {
	ERR_FATAL,					
	ERR_DROP,					
	ERR_DISCONNECT,				
	ERR_NEED_CD					
} errorParm_t;

#define CIN_system	1
#define CIN_loop	2
#define	CIN_hold	4
#define CIN_silent	8
#define CIN_shader	16



typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef struct
{
	float	radius;
	float	height;
	vec3_t	center;
}cylinder_t;

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	
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

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && ( *(p) == Q_COLOR_ESCAPE ) && ( *((p)+1) ) && ( *((p)+1) != Q_COLOR_ESCAPE ) )

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

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"

extern vec4_t	g_color_table[8];

#define	MAKERGB( v, r, g, b ) v[0]=r;v[1]=g;v[2]=b
#define	MAKERGBA( v, r, g, b, a ) v[0]=r;v[1]=g;v[2]=b;v[3]=a

#define DEG2RAD( a ) ( ( (a) * M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )

struct cplane_s;

extern	vec3_t	vec3_origin;
extern	vec3_t	axisDefault[3];

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

#ifndef __Q_FABS__
#define __Q_FABS__
#endif

float Q_fabs( float f );
float Q_rsqrt( float f );		

#define SQRTFAST( x ) ( 1.0f / Q_rsqrt( x ) )

signed char ClampChar( int i );
signed short ClampShort( int i );

double		dEpsilon( void );
double 		dIdentity( void );
double		dSign( const double number );
double		dClamp( const double value, const double min, const double max );
double		dDistance (const double value1, const double value2 );
qboolean	dCloseEnough( const double value1, const double value2, const double epsilon );
qboolean	dSmallEnough( const double value, const double epsilon );

float		fEpsilon( void );
float 		fIdentity( void );
float		fSign( const float number );
float		fClamp( const float value, const float min, const float max );
float		fDistance (const float value1, const float value2 );
qboolean	fCloseEnough( const float value1, const float value2, const float epsilon );
qboolean	fSmallEnough( const float value, const float epsilon );

int			iSign( const int number);
int			iClamp( const int value, const int min, const int max);


int DirToByte( const vec3_t dir );
void ByteToDir( int b, vec3_t dir );

#if	1

#define DotProduct(x,y)			( ((x)[0]*(y)[0]) + ((x)[1]*(y)[1]) + ((x)[2]*(y)[2]) )
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define	VectorMA(v, s, b, o)	((o)[0]= (v)[0] + ((b)[0]*(s)), (o)[1] = (v)[1] + ((b)[1]*(s)), (o)[2] = (v)[2] + ((b)[2]*(s)))
#define Round(a)				(int)(a + 0.5f)
#else

#define DotProduct(x,y)			_DotProduct(x,y)

#ifdef MSVC_BUILD
void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out );
#endif

#define VectorCopy(a,b)			_VectorCopy(a,b)
#define	VectorScale(v, s, o)	_VectorScale(v,s,o)
#define	VectorMA(v, s, b, o)	_VectorMA(v,s,b,o)

#endif

#ifdef __LCC__
#ifdef VectorCopy
#undef VectorCopy
typedef struct {
	float	v[3];
} vec3struct_t;
#define VectorCopy(a,b)	*(vec3struct_t *)b=*(vec3struct_t *)a;
#endif
#endif

#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define OrientClear( orient )   \
   ( VectorSet( orient.origin, 0, 0, 0 ), \
   VectorSet( orient.axis[ 0 ], 1, 0, 0 ), \
   VectorSet( orient.axis[ 1 ], 0, 1, 0 ), \
   VectorSet( orient.axis[ 2 ], 0, 0, 1 ) )
#define OrientCopy( a, b )   \
   ( VectorCopy( (a).origin, (b).origin ), \
   VectorCopy( (a).axis[ 0 ], (b).axis[ 0 ] ), \
   VectorCopy( (a).axis[ 1 ], (b).axis[ 1 ] ), \
   VectorCopy( (a).axis[ 2 ], (b).axis[ 2 ] ) )

#define QuatSet( q, x, y, z, w ) ((q)[0]=(x),(q)[1]=(y),(q)[2]=(z),(q)[3]=(w))
#define QuatCopy( a,b ) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define uint_cast(X) ( *(unsigned int *) &(X) )
#define int_cast(X) ( *(int *) &(X) )
#define IsNegative(X) ( uint_cast(X) >> 31 )
#define USES_CLAMP_ZERO const unsigned s_clamp0table[2] = { 0xFFFFFFFF, 0x00000000 }
#define ClampZero(X) ( uint_cast(X) &= s_clamp0table[IsNegative(X)] )
#define ClampNormalFloat255Byte(dst,X) { \
   float _f_ = X; \
   unsigned mask = ~ ( (int) ( uint_cast(_f_) - 1 ) >> 31 ); \
   \
   uint_cast(_f_) += (unsigned) ( 0x04000000 - 0x437F0000 ); \
   uint_cast(_f_) &= ( (int) uint_cast(_f_) ) >> 31; \
   uint_cast(_f_) += (unsigned) 0x437F0000; \
   uint_cast(_f_) &= mask; \
   dst = (unsigned char) _f_; \
}

#define SetHighest(type, dst,upperbound) { \
   type _ff_=dst-upperbound;\
   ClampZero ( _ff_ );\
   dst = dst - _ff_;\
}

#define SetLowest(type, dst,lowerbound) { \
   type _ff_= dst - lowerbound;\
   ClampZero ( _ff_ );\
   dst = lowerbound + _ff_;\
}

#define SetLowestFloat(dst,l) ( dst = ( ( dst < l ) ? l : dst ) )
#define SetHighestFloat(dst,l) ( dst = ( ( dst > l ) ? l : dst ) )
#define SetLowestInt(dst,l) SetLowest ( int, dst, l )
#define SetHighestInt(dst,l) SetHighest ( int, dst, l )

#ifdef __cplusplus
inline bool VectorFromString ( const char *ss, vec3_t v ) {
   return sscanf ( ss, "%f %f %f", &v[0], &v[1], &v[2] ) == 3;
}
#else
#define VectorFromString(ss,v)   ( sscanf ( (ss), "%f %f %f", &(v)[0], &(v)[1], &(v)[2] ) == 3 )
#endif

#define	SnapVector(v) {v[0]=(int)v[0];v[1]=(int)v[1];v[2]=(int)v[2];}

vec_t _DotProduct( const vec3_t v1, const vec3_t v2 );
void _VectorCopy( const vec3_t in, vec3_t out );
void _VectorScale( const vec3_t in, float scale, vec3_t out );
void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc );

unsigned ColorBytes3 (float r, float g, float b);
unsigned ColorBytes4 (float r, float g, float b, float a);

float NormalizeColor( const vec3_t in, vec3_t out );

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ClearBounds( vec3_t mins, vec3_t maxs );
qboolean BoundsClear( const vec3_t mins, const vec3_t maxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
int VectorCompare( const vec3_t v1, const vec3_t v2 );
vec_t VectorLength( const vec3_t v );
vec_t VectorLengthSquared( const vec3_t v );
vec_t Distance( const vec3_t p1, const vec3_t p2 );
vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 );
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross );
vec_t VectorNormalize (vec3_t v);		
void VectorNormalizeFast(vec3_t v);		
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void VectorInverse (vec3_t v);
void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out );
void VectorRotate( const vec3_t in, const vec3_t matrix[3], vec3_t out );
int Q_log2(int val);
unsigned short NormalToLatLong( const vec3_t normal );


int		Q_rand( int *seed );
float	Q_random( int *seed );
float	Q_crandom( int *seed );

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0f * (random() - 0.5f))

float randomrange( float min, float max );
float crandomrange( float min, float max );
float grandom( float average, float deviation );
float erandom( float mean );

void AccumulateTransform( vec3_t dstLocation, vec3_t dstAxes[3],
						  const vec3_t childLocation, const vec3_t childAxes[3],
						  const vec3_t parentLocation, const vec3_t parentAxes[3] );
void AccumulatePosition( vec3_t dstOrigin, const vec3_t childOrigin, const vec3_t parentOrigin, const vec3_t parentAxes[3] );

void vectoangles( const vec3_t value1, vec3_t angles);
float vectoyaw( const vec3_t vec );

#define AnglesToMat AnglesToAxis

void AxisClear( vec3_t axis[3] );
void AxisCopy( const vec3_t in[3], vec3_t out[3] );

void SetPlaneSignbits( struct cplane_s *out );
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);

float	AngleMod(float a);
float	LerpAngle (float from, float to, float frac);
float LerpAngleFromCurrent (float from, float to, float current, float frac);
float	AngleSubtract( float a1, float a2 );
void	AnglesSubtract( const vec3_t v1, const vec3_t v2, vec3_t v3 );

float AngleNormalize360 ( float angle );
float AngleNormalize180 ( float angle );
float AngleNormalizeArbitrary ( const float angle, const float minimumAngle );
float AngleDelta ( float angle1, float angle2 );

qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void RotateAroundDirection( vec3_t axis[3], float yaw );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );

float noise(float vec[], int len);
float noise1(float arg);
float noise2(const float arg[2]);
float noise3(const float arg[3]);

void  R_ConcatRotations ( const float in1[3][3], const float in2[3][3], float out[3][3] );
void  R_ConcatTransforms ( const float in1[3][4], const float in2[3][4], float out[3][4] );
float	anglemod(float a);
float angledist( float ang );
int   BoxOnPlaneSide2 ( const vec3_t emins, const vec3_t emaxs, const struct cplane_s *p );
void  CalculateRotatedBounds( const vec3_t angles, vec3_t mins, vec3_t maxs );
void  CalculateRotatedBounds2( float trans[3][3], vec3_t mins, vec3_t maxs );
int   BoundingBoxToInteger( const vec3_t mins, const vec3_t maxs );
void  IntegerToBoundingBox( int num, vec3_t mins, vec3_t maxs );
void  MatrixTransformVector( const vec3_t in, const float mat[ 3 ][ 3 ], vec3_t out );
void  Matrix4TransformVector( const vec3_t in, const float mat[ 4 ][ 4 ], vec3_t out );
void  TransposeMatrix( const float in[ 3 ][ 3 ], float out[ 3 ][ 3 ] );
void  OrthoNormalize( float mat[3][3] );
float NormalizeQuat( float q[ 4 ] );
void  RotateAxis( const float axis[ 3 ], float angle, float q[ 4 ] );
void  MultQuat( const float q1[ 4 ], const float q2[ 4 ], float out[ 4 ] );
void  SlerpQuaternion( const float from[ 4 ], const float to[ 4 ], float t, float res[ 4 ] );
int	PlaneTypeForNormal ( const vec3_t normal );




float Com_Clamp( float min, float max, float value );

void COM_FilePath (const char *in, char *out);
void COM_FileBase (const char *in, char *out);
void COM_FileName (const char *in, char *out);

const char	*COM_SkipPath( const char *pathname );
void	      COM_StripExtension( const char *in, char *out );
void	      COM_DefaultExtension( char *path, int maxSize, const char *extension );
void        Com_BackslashToSlash( char *str );

void	      COM_BeginParseSession( void );
int         COM_GetCurrentParseLine( void );
const char	*COM_Parse( const char **data_p );
char	      *COM_ParseExt( char **data_p, qboolean allowLineBreak );
const char  *COM_GetToken(const char **data_p, qboolean crossline);
int			COM_GetParseLineNumber(void);


void	COM_MatchToken( char**buf_p, char *match );

qboolean SkipBracedSection (char **program);
void SkipRestOfLine ( char **data );

void Parse1DMatrix (char **buf_p, int x, float *m);
void Parse2DMatrix (char **buf_p, int y, int x, float *m);
void Parse3DMatrix (char **buf_p, int z, int y, int x, float *m);

void	Com_sprintf (char *dest, int size, const char *fmt, ...);


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


int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );

int	Q_stricmp (const char *s1, const char *s2);
int	Q_strncmp (const char *s1, const char *s2, int n);
int	Q_stricmpn (const char *s1, const char *s2, int n);
char	*Q_strlwr( char *s1 );
char	*Q_strupr( char *s1 );
char	*Q_strrchr( const char* string, int c );

void	Q_strncpyz( char *dest, const char *src, int destsize );
void	Q_strcat( char *dest, int size, const char *src );
char *Q_CleanStr( char *string );


extern qboolean bigendian;

short	BigShort(short l);
short	LittleShort(short l);
int	BigLong (int l);
int	LittleLong (int l);
float	BigFloat (float l);
float	LittleFloat (float l);
unsigned short	LittleUnsignedShort(unsigned short l);
unsigned short	BigUnsignedShort(unsigned short l);

void	Swap_Init (void);
const char *va(const char *format, ...);


int MusicMood_NameToNum( const char * name );
const char * MusicMood_NumToName( int num );
int EAXMode_NameToNum( const char * name );
const char * EAXMode_NumToName( int num );

unsigned int GenerateHashForName( const char* name, qboolean caseSensitive, unsigned int maxHash );

int PlayerStat_NameToNum( const char * name );
const char * PlayerStat_NumToName( int num );

const char *Info_ValueForKey( const char *s, const char *key );
void        Info_RemoveKey( char *s, const char *key );
void        Info_SetValueForKey( char *s, const char *key, const char *value );
qboolean    Info_Validate( const char *s );
void        Info_NextPair( const char **s, char key[MAX_INFO_KEY], char value[MAX_INFO_VALUE] );

void ParseMapName( const char *fullName, char *mapName, char *spawnposName, char *movieName );

void	Com_Error( int level, const char *error, ... );
void	Com_Printf( const char *msg, ... );
void	Com_WPrintf( const char *msg, ... );

void	Com_WidgetPrintf ( const char *widgetName, const char *fmt, ...	);


#define	CVAR_ARCHIVE		1	
#define	CVAR_USERINFO		2	
#define	CVAR_SERVERINFO	4	
#define	CVAR_SYSTEMINFO	8	
#define	CVAR_INIT			16	
#define	CVAR_LATCH			32	
#define	CVAR_ROM			   64	
#define	CVAR_USER_CREATED	128	
#define	CVAR_TEMP			256	
#define  CVAR_CHEAT			512	
#define  CVAR_NORESTART		1024	
#define  CVAR_RESETSTRING  2048  
#define	CVAR_SOUND_LATCH	4096	
#define	CVAR_UNDEFINED		8192

#define CVAR_UNDEFINED_STRING "UNDEFINED"

typedef struct cvar_s {
	char		*name;
	char		*string;
	char		*resetString;		
	char		*latchedString;		
	int			flags;
	qboolean	modified;			
	int			modificationCount;	
	float		value;				
	int			integer;			
	struct cvar_s *next;
	qboolean	defaultSet;			
} cvar_t;

#define	MAX_CVAR_VALUE_STRING	256

typedef int	cvarHandle_t;

typedef struct {
	cvarHandle_t	handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;


#include "surfaceflags.h"			

#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2
#define	PLANE_NON_AXIAL	3


typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			
	byte	signbits;		
	byte	pad[2];
} cplane_t;

typedef struct
   {
   qboolean valid;
   int      surface;
	int		bone;
   vec3_t   position;
   vec3_t   normal;
   float    damage_multiplier;
   } tikimdl_intersection_t;

typedef struct {
	qboolean	         allsolid;	   
	qboolean	         startsolid;	   
	float		         fraction;	   
	vec3_t	         endpos;		   
	cplane_t	         plane;		   
	int		         surfaceFlags;	
	int		         contents;	   
	int		         entityNum;	   
   struct gentity_s	*ent;		      
	tikimdl_intersection_t intersect; 
} trace_t;

typedef struct {
	vec3_t		boxmins, boxmaxs;
	const float	*mins;
	const float *maxs;	
	const float	*start;
	vec3_t		end;
	trace_t		trace;
	int			passEntityNum;
	int			contentmask;
   qboolean    cylinder;
	qboolean		fulltrace;
} moveclip_t;



typedef struct {
	int		firstPoint;
	int		numPoints;

	vec3_t	normal;
	float		dist;
} markFragment_t;

typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;

typedef struct {
   int      time;                   
   int      serverTime;             
   int		totalGameTime;			
   char		comment[ MAX_QPATH ];   
   char     mapName[ MAX_QPATH ];   
   char     saveName[ MAX_QPATH ];  
} savegamestruct_t;


#define KEYCATCH_CONSOLE	1
#define	KEYCATCH_UI			2
#define	KEYCATCH_MESSAGE	4
#define KEYCATCH_HUD		8


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

#define DEFAULT_MIN_DIST  -1.0f
#define DEFAULT_VOL  -1.0f

#define CONTEXT_WIDE_MIN_DIST 1024
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

#define LIP_SYNC_HZ  20.0f

typedef enum {
	MORPH_CHAN_NONE,
	MORPH_CHAN_MOUTH,
	MORPH_CHAN_BROW,
   MORPH_CHAN_LEFT_BROW,
	MORPH_CHAN_RIGHT_BROW,
	MORPH_CHAN_EYES,
	MORPH_CHAN_LEFT_LID,
	MORPH_CHAN_RIGHT_LID
} morphChannel_t;



#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

#define	SNAPFLAG_RATE_DELAYED	1
#define	SNAPFLAG_NOT_ACTIVE		2	
#define  SNAPFLAG_SERVERCOUNT	   4	

#define	MAX_CLIENTS			128		
#define MAX_LOCATIONS		64

#define	GENTITYNUM_BITS		10		
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)

#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)


#define  MAX_MODEL_BITS			9
#define	 MAX_MODELS				(1 << MAX_MODEL_BITS) 

#define  MAX_IMAGES           128
#define  IMAGE_BITS			  7	

#define  MAX_ACTIVE_ITEMS     8
#define  MAX_INVENTORY        32
#define  MAX_AMMO             16
#define  MAX_ITEMS            (MAX_INVENTORY + MAX_AMMO + 16)
#define  MAX_AMMOCOUNT        16
#define  MAX_SOUNDS_BITS      9 
#define	 MAX_SOUNDS			   (1<<MAX_SOUNDS_BITS)
#define  MAX_LIGHTSTYLES      32
#define  MAX_ARENA_INFO       32
#define  MAX_TEAM_INFO        64
#define  MAX_ARCHETYPES		  383
#define  MAX_OBJECTIVE_NAMES  5
#define  MAX_GENERAL_STRINGS  256

#define	MAX_CONFIGSTRINGS	2300 

#define	CS_SERVERINFO		0		
#define	CS_SYSTEMINFO		1		
#define	CS_NAME     		2		

#define	MAX_GAMESTATE_CHARS	36000
typedef struct {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;

#define Square(x) ((x)*(x))   






typedef enum
{
	SPECTATOR_TYPE_NONE,
	SPECTATOR_TYPE_NORMAL,
	SPECTATOR_TYPE_FREEFORM,
	SPECTATOR_TYPE_FOLLOW,
	SPECTATOR_TYPE_ANY
} SpectatorTypes;


#define	MAX_STATS				96
#define	MAX_PERSISTANT			16
#define	MAX_POWERUPS			16
#define	MAX_WEAPONS				16

#define	MAX_PS_EVENTS			2


#define OBJECTIVE1_SHOW     ( 1<<0 )
#define OBJECTIVE1_COMPLETE ( 1<<1 )
#define OBJECTIVE1_FAILED   ( 1<<2 )

#define OBJECTIVE2_SHOW     ( 1<<3 )
#define OBJECTIVE2_COMPLETE ( 1<<4 )
#define OBJECTIVE2_FAILED   ( 1<<5 )

#define OBJECTIVE3_SHOW     ( 1<<6 )
#define OBJECTIVE3_COMPLETE ( 1<<7 )
#define OBJECTIVE3_FAILED   ( 1<<8 )

#define OBJECTIVE4_SHOW     ( 1<<9 )
#define OBJECTIVE4_COMPLETE ( 1<<10 )
#define OBJECTIVE4_FAILED   ( 1<<11 )

#define OBJECTIVE5_SHOW     ( 1<<12 )
#define OBJECTIVE5_COMPLETE ( 1<<13 )
#define OBJECTIVE5_FAILED   ( 1<<14 )

#define OBJECTIVE6_SHOW     ( 1<<15 )
#define OBJECTIVE6_COMPLETE ( 1<<16 )
#define OBJECTIVE6_FAILED   ( 1<<17 )

#define OBJECTIVE7_SHOW     ( 1<<18 )
#define OBJECTIVE7_COMPLETE ( 1<<19 )
#define OBJECTIVE7_FAILED   ( 1<<20 )

#define OBJECTIVE8_SHOW     ( 1<<21 )
#define OBJECTIVE8_COMPLETE ( 1<<22 )
#define OBJECTIVE8_FAILED   ( 1<<23 )

#define OBJECTIVE1 1
#define OBJECTIVE2 2
#define OBJECTIVE3 3
#define OBJECTIVE4 4
#define OBJECTIVE5 5
#define OBJECTIVE6 6
#define OBJECTIVE7 7
#define OBJECTIVE8 8


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
   int         pm_runtime;    
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
	qboolean	   walking;
	qboolean	   jumped;
	qboolean	   groundPlane;
	qboolean		instantJump;
	qboolean		strafeJumpingAllowed;
   int         feetfalling;
   vec3_t      falldir;
	trace_t		groundTrace;

	int			clientNum;		

	vec3_t		viewangles;		
	int			viewheight;

	int			stats[MAX_STATS];
   int         activeItems[MAX_ACTIVE_ITEMS];
   int         inventory_name_index[MAX_INVENTORY];
   int			inventory_weapon_ammo_index[MAX_INVENTORY];
   int			inventory_weapon_required_ammo[MAX_INVENTORY];
   int			ammo_in_clip[MAX_INVENTORY];
   int         ammo_name_index[MAX_AMMO];
   int         ammo_amount[MAX_AMMOCOUNT];
   int         max_ammo_amount[MAX_AMMOCOUNT];

   int         current_music_mood;
   int         fallback_music_mood;
	float		   music_volume;
	float		   music_volume_fade_time;
	qboolean	   allowMusicDucking;

	int			reverb_type;
	float			reverb_level;

  	float		   blend[4];		   
   float       fov;              

	vec3_t		camera_origin;    
	vec3_t		cameraFocalPoint;
	qboolean	useCameraFocalPoint;
   vec3_t      camera_angles;    
   float       camera_time;      

   vec3_t      camera_offset;    
   vec3_t      damage_angles;    
   int         camera_flags;     
	vec3_t      vehicleoffset;
	qboolean    in_vehicle;

	int			dialogEntnum;
	int			dialogSoundIndex;
	int			dialogTextSoundIndex;

	int				objectiveNameIndex;
	unsigned int	objectiveStates;
	unsigned int	informationStates;

	unsigned int  missionStatus;

	unsigned int	viewMode;

	int			ping;			
} playerState_t;

typedef enum {
	WEATHER_NONE,
	WEATHER_RAIN,
	WEATHER_RAIN_PLAIN,
	WEATHER_SNOW
} weather_t;

#define DYNAMIC_LIGHT_NO_DEFAULT		255
#define DYNAMIC_LIGHT_MAX_INTENSITY		254

typedef struct worldState_s {
	byte				dynamic_light_intensities[ MAX_LIGHTING_GROUPS ];

	byte				dynamic_light_default_intensities[ MAX_LIGHTING_GROUPS ];

	vec3_t				wind_direction;
	float				wind_intensity;

	weather_t		weather_type;
	int				weather_intensity;

	float				time_scale;
} worldState_t;

#define MAX_SERVER_SOUNDS        32
#define MAX_SERVER_SOUNDS_BITS   6

typedef struct
{
	vec3_t origin;
	int entity_number;
	int channel;
	short sound_index;
	float volume;
	float min_dist;
	qboolean stop_flag;
	float pitch_modifier;
} server_sound_t;



#define	BUTTON_ATTACKRIGHT_BITINDEX 	0
#define	BUTTON_ATTACKLEFT_BITINDEX  	1
#define	BUTTON_RELOAD_BITINDEX			2			
#define  BUTTON_RUN_BITINDEX           3
#define  BUTTON_HOLSTERWEAPON_BITINDEX 4
#define  BUTTON_USE_BITINDEX           5
#define	BUTTON_TALK_BITINDEX			   6			
#define	BUTTON_ANY_BITINDEX			   7		   
#define	BUTTON_DROP_RUNE_BITINDEX	   8
#define	BUTTON_TRANSFER_ENERGY_BITINDEX	   8

#define	USERCMD_BUTTON_MAX	16

#define	BUTTON_ATTACKRIGHT 	( 1 << BUTTON_ATTACKRIGHT_BITINDEX )
#define	BUTTON_ATTACKLEFT  	( 1 << BUTTON_ATTACKLEFT_BITINDEX )
#define  BUTTON_RUN           ( 1 << BUTTON_RUN_BITINDEX )
#define  BUTTON_HOLSTERWEAPON ( 1 << BUTTON_HOLSTERWEAPON_BITINDEX )
#define  BUTTON_USE           ( 1 << BUTTON_USE_BITINDEX )
#define	BUTTON_TALK			   ( 1 << BUTTON_TALK_BITINDEX )			
#define	BUTTON_RELOAD			( 1 << BUTTON_RELOAD_BITINDEX )		
#define	BUTTON_ANY			   ( 1 << BUTTON_ANY_BITINDEX )		   
#define	BUTTON_DROP_RUNE	   ( 1 << BUTTON_DROP_RUNE_BITINDEX )
#define	BUTTON_TRANSFER_ENERGY ( 1 << BUTTON_TRANSFER_ENERGY_BITINDEX )



typedef struct usercmd_s {
	int	serverTime;
   byte	msec;
	short	buttons;
	byte	weapon;
	short	angles[3];
	short	deltaAngles[3];	
	short choffset[2];
	vec3_t realvieworigin;
	vec3_t realviewangles;
	qboolean thirdperson;
	signed char	forwardmove, rightmove, upmove, lean;
	
} usercmd_t;


#define  MDL_ANIM_DELTA_DRIVEN   ( 1 << 0 )
#define  MDL_ANIM_DEFAULT_ANGLES ( 1 << 3 )
#define  MDL_ANIM_NO_TIMECHECK   ( 1 << 4 )

#define	SOLID_BMODEL	0xffffff

#define	RF_THIRD_PERSON	   (1<<0)   	
#define	RF_FIRST_PERSON	   (1<<1)		
#define  RF_DEPTHHACK         (1<<2)      
#define  RF_VIEWLENSFLARE     (1<<3)      
#define	RF_FRAMELERP		   (1<<4)      
#define	RF_BEAM				   (1<<5)      
#define  RF_SHADOW_FROM_BIP01   (1<<6)
#define  RF_DONTDRAW			   (1<<7)      
#define  RF_LENSFLARE         (1<<8)      
#define  RF_EXTRALIGHT        (1<<9)      
#define  RF_DETAIL            (1<<10)     
#define  RF_SHADOW            (1<<11)     
#define	RF_PORTALSURFACE	   (1<<12)   	
#define	RF_SKYORIGIN   	   (1<<13)   	
#define	RF_SKYENTITY   	   (1<<14)   	
#define	RF_LIGHTOFFSET   	   (1<<15)   	
#define	RF_CUSTOMSHADERPASS  (1<<16)     
#define	RF_MINLIGHT			   (1<<17)		
#define	RF_FULLBRIGHT		   (1<<18)		
#define  RF_LIGHTING_ORIGIN	(1<<19)		
#define	RF_SHADOW_PLANE      (1<<20)		
#define	RF_WRAP_FRAMES		   (1<<21)		
#define  RF_PORTALENTITY      (1<<22)     
#define  RF_DUALENTITY        (1<<23)     
#define  RF_ADDITIVE_DLIGHT   (1<<24)     
#define  RF_LIGHTSTYLE_DLIGHT (1<<25)     
#define  RF_SHADOW_PRECISE    (1<<26)     
#define  RF_INVISIBLE         (1<<27)     
#define  RF_WEAPONMODEL       (1<<28)     
#define  RF_FORCE_ALPHA			(1<<29)		
#define  RF_FORCE_ALPHA_EFFECTS	(1<<30)	
#define  RF_CHILDREN_DONT_INHERIT_ALPHA	(1<<31)

#define  RF_FLAGS_NOT_INHERITED ( RF_LENSFLARE | RF_VIEWLENSFLARE | RF_BEAM | RF_EXTRALIGHT | RF_SKYORIGIN | RF_SHADOW | RF_SHADOW_PRECISE | RF_SHADOW_PLANE | RF_LIGHTOFFSET | RF_CHILDREN_DONT_INHERIT_ALPHA )

#define  PMF_CAMERA_VIEW      ( 1<<8 )    

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
#define BEAM_WAVE_EFFECT2       (1<<12)
#define BEAM_FULLWAVE_EFFECT    (1<<13)


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

#define MAX_MODEL_SURFACES   32 

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

#define NUM_BONE_CONTROLLERS 5

#define NUM_MORPH_CONTROLLERS 10

#define NUM_EFFECTS_ANIMS	4

typedef struct {
	int		index;
	float		percent;
} morph_controller_t;


typedef struct entityState_s {
	int		number;			
	int		instanceNumber;
	int		eType;			
	int		eFlags;

	trajectory_t	pos;	
	trajectory_t	apos;	

   vec3_t   netorigin;    
	vec3_t	origin;
	vec3_t	origin2;

   vec3_t   netangles;    
	vec3_t	angles;

	vec3_t  viewangles ;  

	unsigned int	constantLight;	   

	int      loopSound;		   
	float		loopSoundVolume;
	float		loopSoundMinDist;

   int      parent;           
   int      tag_num;          
	qboolean attach_use_angles;
	vec3_t	attach_offset;
	vec3_t	attach_angles_offset;

	int      modelindex;
	int		viewmodelindex;
	int      skinNum;
	int		customShader;
	int		customEmitter;
	float	animationRate;
   int      anim;
   int      frame;
   int      crossblend_time;  

   int      torso_anim;
   int      torso_frame;
   int      torso_crossblend_time; 

   int      bone_tag[ NUM_BONE_CONTROLLERS ];
   vec3_t   bone_angles[ NUM_BONE_CONTROLLERS ];
   vec4_t   bone_quat[ NUM_BONE_CONTROLLERS ];        

	morph_controller_t morph_controllers[ NUM_MORPH_CONTROLLERS ];

   byte     surfaces[MAX_MODEL_SURFACES];

	int		clientNum;		

   int		groundEntityNum;  
	int		solid;			

   float    scale;
   float    alpha;

	int				renderfx;

	unsigned int	affectingViewModes;

	int				archeTypeIndex;
	int				missionObjective;

	int				infoIcon;

	int		effectsAnims[ NUM_EFFECTS_ANIMS ];

	int      bindparent;    
  	float		quat[4];       
	float		mat[3][3];     
} entityState_t;

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


void MatrixMultiply( const float in1[3][3], const float in2[3][3], float out[3][3] );
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] );
void AxisToAngles( vec3_t axis[3], vec3_t angles );
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up);
void PerpendicularVector( vec3_t dst, const vec3_t src );
void EulerToQuat( float ang[ 3 ], float q[ 4 ] );
void MatToQuat( const float srcMatrix[ 3 ][ 3 ], float destQuat[ 4 ] );
void QuatToMat( const float q[ 4 ], float m[ 3 ][ 3 ]	);
void MatrixToEulerAngles( const float mat[ 3 ][ 3 ], vec3_t ang );



#define TIKI_CMD_MAX_CMDS 256
#define TIKI_CMD_MAX_ARGS 12

typedef struct
   {
   int num_args;
   char *args[TIKI_CMD_MAX_ARGS];
   } tiki_singlecmd_t;

typedef struct
   {
   int num_cmds;
   tiki_singlecmd_t cmds[ TIKI_CMD_MAX_CMDS ];
   } tiki_cmd_t;

typedef struct
	{
	vec3_t	start;
	vec3_t	end;
	vec3_t	color;
	float		alpha;
   float    width;
   unsigned short factor;
   unsigned short pattern;
	} debugline_t;

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

typedef enum {
	FMV_IDLE,
	FMV_PLAY,		
	FMV_EOF,		
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

#define CDKEY_LEN	 16
#define CDCHKSUM_LEN 2


#ifdef __cplusplus
}
#endif

#endif	
