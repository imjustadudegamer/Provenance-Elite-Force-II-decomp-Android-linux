/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tr_local.h"
#include "tr_vbo.h"
#include "tr_program.h"
#include "gles3_firstlight.h"

glconfig_t	glConfig;
qboolean	textureFilterAnisotropic = qfalse;
int		 maxAnisotropy = 0;
float	   displayAspect = 0;
qboolean	haveClampToEdge;
glstate_t	glState;
int r_sequencenumber;

static void GfxInfo_f( void );

/* The per-event recorders (bind pairs, uploads, draw-key and stage-bind censuses)
 * run only under the `qa_probe' gate; the stage-bind
 * census scans up to SURFSTATS_ROWS rows for every stage once its table fills. */
qboolean R_ProbeStatsOn( void ) {
	static cvar_t	*probe;

	if ( !probe ) {
		probe = ri.Cvar_Get( "qa_probe", "1", 0 );
	}

	return ( probe && probe->integer ) ? qtrue : qfalse;
}


cvar_t	*r_ignoreFastPath;

cvar_t	*r_verbose;
cvar_t	*r_ignore;

cvar_t	*r_displayRefresh;

cvar_t	*r_znear;

cvar_t	*r_skipBackEnd;

cvar_t	*r_smp;

cvar_t	*r_ignorehwgamma;
cvar_t	*r_measureOverdraw;

cvar_t	*r_inGameVideo;
cvar_t	*r_fastsky;
cvar_t	*r_fastdlights;
cvar_t	*r_drawSun;
cvar_t	*r_dlightBacks;
cvar_t	*r_dynamiclightmaps;
cvar_t	*r_dynamiclight;		 

cvar_t	*r_lodscale;
cvar_t	*r_lodbias;
cvar_t	*r_lodScaleDistance;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawviewmodel;
cvar_t	*r_drawcurves;
cvar_t	*r_drawterrain;
cvar_t	*r_drawsprites;
cvar_t	*r_fadeDistMod;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_facePlaneCull;
cvar_t	*r_showcluster;
cvar_t	*r_nocurves;
cvar_t	*r_terrain_global;
cvar_t	*r_terrain_global_min;

cvar_t	*r_allowExtensions;

// 0x00119890; that one is not the renderer's, and nothing in the shipped PC data
cvar_t	*r_ext_compress_textures;
cvar_t	*r_ext_gamma_control;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;

cvar_t	*r_ext_texture_env_combine;
cvar_t	*r_draw2d;
cvar_t	*r_2dTrace;

cvar_t	*r_ignoreGLErrors;
cvar_t	*r_logFile;

cvar_t	*r_stencilbits;
cvar_t	*r_depthbits;
cvar_t	*r_colorbits;
cvar_t	*r_stereo;
cvar_t	*r_primitives;
cvar_t	*r_detailtextures;
cvar_t	*r_detailSurfaces;
cvar_t	*r_allowGeometryDeforms;
cvar_t	*r_texturebits;
cvar_t	*r_cubeMapping;

cvar_t	*r_drawBuffer;
cvar_t  *r_glDriver;
cvar_t	*r_lightmap;
cvar_t	*r_vertexLight;
cvar_t	*r_shadows;
cvar_t	*r_flares;
cvar_t	*r_flaresMaxDist;
cvar_t	*r_mode;
cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_lerpmodels;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_showtris;
cvar_t	*r_showsky;
cvar_t	*r_drawsky;
cvar_t	*r_shownormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_swapInterval;
cvar_t	*r_textureMode;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_fullscreen;

cvar_t	*r_customwidth;
cvar_t	*r_customheight;
cvar_t	*r_customaspect;

cvar_t	*r_overBrightBits;
cvar_t	*r_mapOverBrightBits;

cvar_t	*r_debugSurface;

cvar_t	*r_showImages;
// six retail binaries; it was the same gate, so it is deleted, not aliased.
cvar_t	*r_showlod;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;
cvar_t	*r_saveFontData;

int		max_polys;
int		max_polyverts;

cvar_t* r_lodscale;



cvar_t* r_numdebuglines;


cvar_t* r_stipplelines;
cvar_t* r_light_lines;
cvar_t* r_light_sun_line;
cvar_t* r_light_int_scale;
cvar_t* r_light_nolight;
cvar_t* r_light_emphasize;
cvar_t* r_light_emphasizePercent;
cvar_t* r_light_simple;
cvar_t* r_light_simple_dist;
cvar_t* r_light_ambientFudge;
cvar_t* r_light_ambientEmphasize;
cvar_t* r_skyportal;
cvar_t* r_skyportal_origin;
cvar_t* r_farplane;
cvar_t* r_farplane_color;
cvar_t* r_farplane_nocull;
cvar_t* r_farplane_nofog;
cvar_t* r_lightcoronasize;
cvar_t* r_useglfog;
cvar_t* r_debuglines_depthmask;
cvar_t* r_bumpmap;


cvar_t* r_arb_multisample;
cvar_t* r_arb_samples;
cvar_t* r_ext_nv_multisample_filter;
cvar_t* r_ext_nv_support_multisample;
cvar_t* r_precompressed_textures;
cvar_t* r_ati_fsaa_samples;
cvar_t* r_nv_fsaa_samples;
cvar_t* r_anisotropicFiltering;


cvar_t* r_developer;
cvar_t* r_fps;

cvar_t* r_jobs;
cvar_t* r_jobsStats;

// PORT-ONLY DIAGNOSTIC (uivk-mt), no retail counterpart: the pool's own
static void R_JobPoolPrint( const char *fmt, ... ) __attribute__((format(printf,1,2)));
static void R_JobPoolPrint( const char *fmt, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	ri.Printf( PRINT_ALL, "%s", msg );
}

static void R_JobPoolTest_f( void ) {
	JobPool_SelfTest( R_JobPoolPrint );
}

static jobpoolMeter_t	gl_meter;

void RB_MtFrameBoundary( void ) {
	if ( !r_jobsStats || !r_jobsStats->integer ) {
		return;
	}
	JobPool_MeterFrame( &gl_meter, R_JobPoolPrint, ri.Milliseconds(),
						tr.frontEndMsec, RB_FrameBackEndMsec(), 0, 0 );
}

void R_InitCommandBuffers( void ) {
	glConfig.smpActive = qfalse;

	if ( !r_smp || !r_smp->integer ) {
		return;
	}

	if ( GLimp_DeviceCoreCount() < 3 ) {
		ri.Printf( PRINT_ALL, "r_smp: %d reported core(s); staying single-threaded\n",
				   GLimp_DeviceCoreCount() );
		return;
	}

	if ( !GLimp_SpawnRenderThread( RB_RenderThread ) ) {
		ri.Printf( PRINT_WARNING, "r_smp: failed to spawn render thread; running single-threaded\n" );
		return;
	}

	glConfig.smpActive = qtrue;
	ri.Printf( PRINT_ALL, "Render thread enabled (r_smp), %d reported cores\n",
			   GLimp_DeviceCoreCount() );
}

void R_ShutdownCommandBuffers( void ) {
	if ( glConfig.smpActive ) {
		GLimp_ShutdownRenderThread();
		glConfig.smpActive = qfalse;
	}
}

void R_SyncRenderThread( void ) {
	if ( !glConfig.smpActive ) {
		return;
	}
	if ( GLimp_IsRenderThread() ) {
		return;
	}
	GLimp_FrontEndSleep();
}

static char infostring[8192];


static void InitOpenGL( void )
{
	char renderer_buffer[1024];

	
	if ( glConfig.vidWidth == 0 )
	{
		GLint		temp;
		
		GLimp_Init(qtrue);

		strcpy( renderer_buffer, glConfig.renderer_string );
		Q_strlwr( renderer_buffer );

		qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &temp );
		glConfig.maxTextureSize = temp;

		if ( glConfig.maxTextureSize <= 0 ) 
		{
			glConfig.maxTextureSize = 0;
		}

	}

	GfxInfo_f();

	GL_SetDefaultState();
}

void GL_CheckErrors( void ) {
	int		err;
	char	s[64];

	err = qglGetError();
	if ( err == GL_NO_ERROR ) {
		return;
	}
	if ( r_ignoreGLErrors->integer ) {
		return;
	}
	switch( err ) {
		case GL_INVALID_ENUM:
			strcpy( s, "GL_INVALID_ENUM" );
			break;
		case GL_INVALID_VALUE:
			strcpy( s, "GL_INVALID_VALUE" );
			break;
		case GL_INVALID_OPERATION:
			strcpy( s, "GL_INVALID_OPERATION" );
			break;
		case GL_STACK_OVERFLOW:
			strcpy( s, "GL_STACK_OVERFLOW" );
			break;
		case GL_STACK_UNDERFLOW:
			strcpy( s, "GL_STACK_UNDERFLOW" );
			break;
		case GL_OUT_OF_MEMORY:
			strcpy( s, "GL_OUT_OF_MEMORY" );
			break;
		default:
			Com_sprintf( s, sizeof(s), "%i", err);
			break;
	}

	ri.Error( ERR_FATAL, "GL_CheckErrors: %s", s );
}


typedef struct vidmode_s
{
	const char *description;
	int		 width, height;
	float		pixelAspect;		
} vidmode_t;

vidmode_t r_vidModes[] =
{
	{ "Mode  0: 320x240",		320,	240,	1 },
	{ "Mode  1: 400x300",		400,	300,	1 },
	{ "Mode  2: 512x384",		512,	384,	1 },
	{ "Mode  3: 640x480",		640,	480,	1 },
	{ "Mode  4: 800x600",		800,	600,	1 },
	{ "Mode  5: 960x720",		960,	720,	1 },
	{ "Mode  6: 1024x768",		1024,	768,	1 },
	{ "Mode  7: 1152x864",		1152,	864,	1 },
	{ "Mode  8: 1280x1024",		1280,	1024,	1 },
	{ "Mode  9: 1600x1200",		1600,	1200,	1 },
	{ "Mode 10: 2048x1536",		2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",856,	480,	1 }
};
static int	s_numVidModes = ( sizeof( r_vidModes ) / sizeof( r_vidModes[0] ) );

qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int mode ) {
	vidmode_t	*vm;

	if ( mode < -1 ) {
		return qfalse;
	}
	if ( mode >= s_numVidModes ) {
		return qfalse;
	}

	if ( mode == -1 ) {
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		*windowAspect = r_customaspect->value;
		return qtrue;
	}

	vm = &r_vidModes[mode];

	*width  = vm->width;
	*height = vm->height;
	*windowAspect = (float)vm->width / ( vm->height * vm->pixelAspect );

	return qtrue;
}

static void R_ModeList_f( void )
{
	int i;

	ri.Printf( PRINT_ALL, "\n" );
	for ( i = 0; i < s_numVidModes; i++ )
	{
		ri.Printf( PRINT_ALL, "%s\n", r_vidModes[i].description );
	}
	ri.Printf( PRINT_ALL, "\n" );
}




byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen)
{
	byte *buffer, *bufstart;
	int padwidth, linelen;
	GLint packAlign;
	
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);
	
	linelen = width * 3;
	padwidth = PAD(linelen, packAlign);
	
	buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + packAlign - 1);
	
	bufstart = PADP((intptr_t) buffer + *offset, packAlign);
	qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart);
	
	*offset = bufstart - buffer;
	*padlen = padwidth - linelen;
	
	return buffer;
}

void RB_TakeScreenshot( int x, int y, int width, int height, char *fileName ) {
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;
	
	int linelen, padlen;
	size_t offset = 18, memcount;
		
	allbuf = RB_ReadPixels(x, y, width, height, &offset, &padlen);
	buffer = allbuf + offset - 18;
	
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	

	linelen = width * 3;
	
	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * height;
	
	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;
			
			srcptr += 3;
		}
		
		srcptr += padlen;
	}

	memcount = linelen * height;

	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(allbuf + offset, memcount);

	ri.FS_WriteFile(fileName, buffer, memcount + 18);

	ri.Hunk_FreeTempMemory(allbuf);
}

const void *RB_TakeScreenshotCmd( const void *data ) {
	const screenshotCommand_t	*cmd;
	
	cmd = (const screenshotCommand_t *)data;
	
	RB_TakeScreenshot( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName);
	
	return (const void *)(cmd + 1);	
}

void R_TakeScreenshot( int x, int y, int width, int height, char *name ) {
	static char	fileName[MAX_OSPATH]; 
	screenshotCommand_t	*cmd;

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SCREENSHOT;

	cmd->x = x;
	cmd->y = y;
	cmd->width = width;
	cmd->height = height;
	Q_strncpyz( fileName, name, sizeof(fileName) );
	cmd->fileName = fileName;
}

void R_ScreenshotFilename( int lastNumber, char *fileName ) {
	int		a,b,c,d;

	if ( lastNumber < 0 || lastNumber > 9999 ) {
		Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot9999.tga" );
		return;
	}

	a = lastNumber / 1000;
	lastNumber -= a*1000;
	b = lastNumber / 100;
	lastNumber -= b*100;
	c = lastNumber / 10;
	lastNumber -= c*10;
	d = lastNumber;

	Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.tga"
		, a, b, c, d );
}

void R_ScreenshotFilenameJPEG( int lastNumber, char *fileName ) {
	int		a,b,c,d;

	if ( lastNumber < 0 || lastNumber > 9999 ) {
		Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot9999.jpg" );
		return;
	}

	a = lastNumber / 1000;
	lastNumber -= a*1000;
	b = lastNumber / 100;
	lastNumber -= b*100;
	c = lastNumber / 10;
	lastNumber -= c*10;
	d = lastNumber;

	Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot%i%i%i%i.jpg"
		, a, b, c, d );
}

void R_ResampledScreenShot(const char* filename, int destwidth, int destheight) {
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t			offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	allsource = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory(destheight * destwidth * 3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2]  = 2;		
	buffer[12] = destwidth & 255;
	buffer[13] = destwidth >> 8;
	buffer[14] = destheight & 255;
	buffer[15] = destheight >> 8;
	buffer[16] = 24;	

	xScale = glConfig.vidWidth / (float)(destwidth * 4);
	yScale = glConfig.vidHeight / (float)(destheight * 3);
	for ( y = 0 ; y < destheight ; y++ ) {
		for ( x = 0 ; x < destwidth ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + (3 * glConfig.vidWidth + padlen) * (int)((y*3 + yy) * yScale) +
						3 * (int) ((x*4 + xx) * xScale);
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * destwidth + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	if ( glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, destheight * destwidth * 3 );
	}

	ri.FS_WriteFile( filename, buffer, destheight * destwidth * 3 + 18 );

	ri.Hunk_FreeTempMemory( buffer );
	ri.Hunk_FreeTempMemory( allsource );
}

void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t			offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

	allsource = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory(128 * 128*3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	

	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for ( y = 0 ; y < 128 ; y++ ) {
		for ( x = 0 ; x < 128 ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + (3 * glConfig.vidWidth + padlen) * (int)((y*3 + yy) * yScale) +
						3 * (int) ((x*4 + xx) * xScale);
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * 128 + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	if ( glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, 128 * 128 * 3 );
	}

	ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 );

	ri.Hunk_FreeTempMemory(buffer);
	ri.Hunk_FreeTempMemory(allsource);

	ri.Printf( PRINT_ALL, "Wrote %s\n", checkname );
}

void R_ScreenShot_f(void) {
	char	checkname[MAX_OSPATH];
	static int	lastNumber = -1;
	int		width, height;
	qboolean	silent;

	if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
		R_LevelShot();
		return;
	}

	if (!strcmp(ri.Cmd_Argv(1), "silent")) {
		silent = qtrue;
	} else {
		silent = qfalse;
	}

	width = 0;
	height = 0;

	if (ri.Cmd_Argc() >= 2 && !silent) {
		const char* name = ri.Cmd_Argv(1);
		if (strchr(name, '/')) {
			Q_strncpyz(checkname, name, sizeof(checkname));
		} else {
			Com_sprintf(checkname, MAX_OSPATH, "screenshots/%s.tga", ri.Cmd_Argv(1));
		}

		if (ri.Cmd_Argc() > 2) {
			width = atoi(ri.Cmd_Argv(2));
			height = atoi(ri.Cmd_Argv(3));
		}
	} else {

		if (lastNumber == -1) {
			lastNumber = 0;
		}
		for (; lastNumber <= 9999; lastNumber++) {
			R_ScreenshotFilename(lastNumber, checkname);

			if (!ri.FS_FileExists(checkname))
			{
				break; 
			}
		}

		if (lastNumber >= 9999) {
			ri.Printf(PRINT_ALL, "ScreenShot: Couldn't create a file\n");
			return;
		}

		lastNumber++;
	}

	if (!width || !height) {
		R_TakeScreenshot(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname);

		if (!silent) {
			char message[64];

			Com_sprintf(message, sizeof(message), "centerprint \"%s %s\"\n", "Wrote", checkname);
			ri.Cmd_ExecuteText(EXEC_NOW, message);
		}
	} else {
		R_ResampledScreenShot(checkname, width, height);
	}
}


const void *RB_TakeVideoFrameCmd( const void *data )
{
	const videoFrameCommand_t	*cmd;
	byte				*cBuf;
	size_t				memcount, linelen;
	int				padwidth, avipadwidth, padlen, avipadlen;
	GLint packAlign;
	
	cmd = (const videoFrameCommand_t *)data;
	
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = cmd->width * 3;

	padwidth = PAD(linelen, packAlign);
	padlen = padwidth - linelen;
	avipadwidth = PAD(linelen, AVI_LINE_PADDING);
	avipadlen = avipadwidth - linelen;

	cBuf = PADP(cmd->captureBuffer, packAlign);
		
	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGB,
		GL_UNSIGNED_BYTE, cBuf);

	memcount = padwidth * cmd->height;

	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(cBuf, memcount);

	if(cmd->motionJpeg)
	{
		memcount = RE_SaveJPGToBuffer(cmd->encodeBuffer, linelen * cmd->height,
			90,  
			cmd->width, cmd->height, cBuf, padlen);
	}
	else
	{
		byte *lineend, *memend;
		byte *srcptr, *destptr;
	
		srcptr = cBuf;
		destptr = cmd->encodeBuffer;
		memend = srcptr + memcount;
		
		while(srcptr < memend)
		{
			lineend = srcptr + linelen;
			while(srcptr < lineend)
			{
				*destptr++ = srcptr[2];
				*destptr++ = srcptr[1];
				*destptr++ = srcptr[0];
				srcptr += 3;
			}
			
			Com_Memset(destptr, '\0', avipadlen);
			destptr += avipadlen;
			
			srcptr += padlen;
		}
		
	}

	return (const void *)(cmd + 1);	
}


void GL_SetDefaultState( void )
{
	qglClearDepth( 1.0f );

	qglCullFace(GL_FRONT);


	if ( qglActiveTexture ) {
		GL_SelectTexture( 1 );
		GL_TextureMode( r_textureMode->string );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( 0 );
	}

	qglEnable(GL_TEXTURE_2D);
	GL_TextureMode( r_textureMode->string );
	GL_TexEnv( GL_MODULATE );

	qglDepthFunc( GL_LEQUAL );

	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;

	qglDepthMask( GL_TRUE );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_SCISSOR_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_BLEND );

	glState.faceCulling = -1;

	glState.fFogColor[0] = 0.0;
	glState.fFogColor[1] = 0.0;
	glState.fFogColor[2] = 0.0;
	glState.fFogColor[3] = 1.0;
}

void PrintAndAppendString(char* string, size_t maxlen, char* fmt, ...) Q_PRINTF_FUNC(3, 4);

void PrintAndAppendString(char* string, size_t maxlen, char* fmt, ...) {
	char msg[1024];
	va_list va;

	va_start(va, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, va);
	va_end(va);

	ri.Printf(PRINT_ALL, "%s", msg);

	Q_strcat(string, maxlen, msg);
}

const char* RE_GetGraphicsInfo(void) {
	return infostring;
}

void R_PrintLongString(char* dest, size_t destsize, const char *string) {
	char buffer[1024];
	const char *p;
	int size = strlen(string);

	p = string;
	while(size > 0)
	{
		Q_strncpyz(buffer, p, sizeof (buffer) );
		PrintAndAppendString( dest, destsize, "%s", buffer );
		p += 1023;
		size -= 1023;
	}
}

void BuildGfxInfo(char* dest, size_t destsize) {
	cvar_t *sys_cpustring = ri.Cvar_Get( "sys_cpustring", "", 0 );
	const char *enablestrings[] =
	{
		"disabled",
		"enabled"
	};
	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};

	PrintAndAppendString( dest, destsize, "\nGL_VENDOR: %s\n", glConfig.vendor_string );
	PrintAndAppendString( dest, destsize, "GL_RENDERER: %s\n", glConfig.renderer_string );
	PrintAndAppendString( dest, destsize, "GL_VERSION: %s\n", glConfig.version_string );
	PrintAndAppendString( dest, destsize, "GL_EXTENSIONS: " );
	if ( qglGetStringi )
	{
		GLint numExtensions;
		int i;

		qglGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
		for ( i = 0; i < numExtensions; i++ )
		{
			PrintAndAppendString( dest, destsize, "%s ", qglGetStringi( GL_EXTENSIONS, i ) );
		}
	}
	else
	{
		R_PrintLongString( dest, destsize, (char *) qglGetString( GL_EXTENSIONS ) );
	}
	PrintAndAppendString( dest, destsize, "\n" );
	PrintAndAppendString( dest, destsize, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	PrintAndAppendString( dest, destsize, "GL_MAX_TEXTURE_UNITS_ARB: %d\n", glConfig.numTextureUnits );
	PrintAndAppendString( dest, destsize, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	PrintAndAppendString( dest, destsize, "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen->integer == 1] );
	if ( glConfig.displayFrequency )
	{
		PrintAndAppendString( dest, destsize, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		PrintAndAppendString( dest, destsize, "N/A\n" );
	}
	if ( glConfig.deviceSupportsGamma )
	{
		PrintAndAppendString( dest, destsize, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	}
	else
	{
		PrintAndAppendString( dest, destsize, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}
	PrintAndAppendString( dest, destsize, "CPU: %s\n", sys_cpustring->string );

	{
		int		primitives;

		PrintAndAppendString( dest, destsize, "rendering primitives: " );
		primitives = r_primitives->integer;
		if ( primitives == 0 ) {
			if ( qglLockArraysEXT ) {
				primitives = 2;
			} else {
				primitives = 1;
			}
		}
		if ( primitives == -1 ) {
			PrintAndAppendString( dest, destsize, "none\n" );
		} else if ( primitives == 2 ) {
			PrintAndAppendString( dest, destsize, "single glDrawElements\n" );
		} else if ( primitives == 1 ) {
			PrintAndAppendString( dest, destsize, "multiple glArrayElement\n" );
		} else if ( primitives == 3 ) {
			PrintAndAppendString( dest, destsize, "multiple glColor4ubv + glTexCoord2fv + glVertex3fv\n" );
		}
	}

	PrintAndAppendString( dest, destsize, "texturemode: %s\n", r_textureMode->string );
	PrintAndAppendString( dest, destsize, "picmip: %d\n", r_picmip->integer );
	PrintAndAppendString( dest, destsize, "texture bits: %d\n", r_texturebits->integer );
	PrintAndAppendString( dest, destsize, "multitexture: %s\n", enablestrings[qglActiveTextureARB != 0] );
	PrintAndAppendString( dest, destsize, "compiled vertex arrays: %s\n", enablestrings[qglLockArraysEXT != 0 ] );
	PrintAndAppendString( dest, destsize, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
	PrintAndAppendString( dest, destsize, "compressed textures: %s\n", enablestrings[glConfig.textureCompression!=TC_NONE] );
	if ( r_vertexLight->integer || glConfig.hardwareType == GLHW_PERMEDIA2 )
	{
		PrintAndAppendString( dest, destsize, "HACK: using vertex lightmap approximation\n" );
	}
	if ( glConfig.hardwareType == GLHW_RAGEPRO )
	{
		PrintAndAppendString( dest, destsize, "HACK: ragePro approximations\n" );
	}
	if ( glConfig.hardwareType == GLHW_RIVA128 )
	{
		PrintAndAppendString( dest, destsize, "HACK: riva128 approximations\n" );
	}
	if ( glConfig.smpActive ) {
		PrintAndAppendString( dest, destsize, "Using dual processor acceleration\n" );
	}
	if ( r_finish->integer ) {
		PrintAndAppendString( dest, destsize, "Forcing glFinish\n" );
	}
}

void GfxInfo_f( void ) 
{
	BuildGfxInfo(infostring, sizeof(infostring));
	ri.Cvar_Set("r_gfxinfo", infostring);
}

static void GLimp_ChangeMode( int mode ) {
	ri.Cvar_SetValue( "r_mode", (float)mode );
	ri.Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void GLimp_ChangeFullscreen( qboolean fullscreen ) {
	ri.Cvar_SetValue( "r_fullscreen", fullscreen ? 1.0f : 0.0f );
	ri.Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

qboolean R_SetMode(int mode, glconfig_t *glConfigOut) {
	GLimp_ChangeMode( mode );
	memcpy( glConfigOut, &glConfig, sizeof( glConfig ) );
	return qtrue;
}

void R_SetFullscreen(qboolean fullscreen, glconfig_t *glConfigOut) {
	GLimp_ChangeFullscreen( fullscreen );
	memcpy( glConfigOut, &glConfig, sizeof( glConfig ) );
}

static void R_FirstLightLog(const char *line)
{
	ri.Printf(PRINT_ALL, "%s", line);
}

static void R_FirstLight_f(void)
{
	unsigned	passed = 0;
	qboolean	faceDone = qfalse;

	if (!tr.registered) {
		RB_BindVAO(0);
		ri.Printf(PRINT_ALL, "[GLES3:firstlight] status=fail reason=renderer-not-registered\n");
		return;
	}

	/* The firstlight core is a context-agnostic utility shared with the offline
	 * probe, which has no render thread.  It creates textures, shaders, programs
	 * and buffers, so the engine entry claims the GL context first: under r_smp
	 * this parks the render thread and the whole command then owns the context. */
	R_IssuePendingRenderCommands();

	ri.Printf(PRINT_ALL, "[GLES3:firstlight] phase=engine vid=%dx%d\n",
			  glConfig.vidWidth, glConfig.vidHeight);

	GLES3_FirstLight_SetLogger(R_FirstLightLog);

	qglViewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	qglDisable(GL_SCISSOR_TEST);

	passed = GLES3_FirstLight_ClearAndQuad();

	if (tr.world && tr.world->numsurfaces > 0) {
		int i;

		for (i = 0; i < tr.world->numsurfaces && !faceDone; i++) {
			msurface_t			*ms = &tr.world->surfaces[i];
			srfSurfaceFace_t	*surf;
			image_t				*img = NULL;
			float				xyz[3 * 3];
			float				st[3 * 2];
			int					k;
			unsigned			tex = 0;

			if (!ms->data || *ms->data != SF_FACE) {
				continue;
			}
			surf = (srfSurfaceFace_t *)ms->data;
			if (surf->numPoints < 3) {
				continue;
			}
			for (k = 0; k < 3; k++) {
				xyz[k * 3 + 0] = surf->points[k][0];
				xyz[k * 3 + 1] = surf->points[k][1];
				xyz[k * 3 + 2] = surf->points[k][2];
				st[k * 2 + 0] = surf->points[k][3];
				st[k * 2 + 1] = surf->points[k][4];
			}
			if (ms->shader && ms->shader->numUnfoggedPasses > 0 &&
				ms->shader->unfoggedStages[0] &&
				ms->shader->unfoggedStages[0]->bundle[0].image[0]) {
				img = ms->shader->unfoggedStages[0]->bundle[0].image[0];
				if (img->texnum) {
					tex = (unsigned)img->texnum;
				}
			}
			ri.Printf(PRINT_ALL, "[GLES3:firstlight] milestone=bsp_face src=world surface=%d "
					  "shader=\"%s\" texture=\"%s\" texnum=%u\n",
					  i, ms->shader ? ms->shader->name : "(none)",
					  img ? img->imgName : "(white)", tex);
			passed |= GLES3_FirstLight_DrawFace(xyz, st, 3, tex);
			faceDone = qtrue;
		}
	}
	if (!faceDone) {
		ri.Printf(PRINT_ALL, "[GLES3:firstlight] milestone=bsp_face status=fail reason=no-world-face "
				  "(load a map first)\n");
	}

	ri.Printf(PRINT_ALL, "[GLES3:firstlight] phase=engine clear=%d quad=%d bsp_face=%d\n",
			  (passed & GLES3_FL_CLEAR) ? 1 : 0,
			  (passed & GLES3_FL_QUAD) ? 1 : 0,
			  (passed & GLES3_FL_FACE) ? 1 : 0);

	RB_BindVAO(0);
}

void R_Register( void )
{
	r_glDriver = ri.Cvar_Get( "r_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE | CVAR_LATCH );
	r_allowExtensions = ri.Cvar_Get( "r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_compress_textures = ri.Cvar_Get( "r_ext_compress_textures", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_precompressed_textures = ri.Cvar_Get( "r_precompressed_textures", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_gamma_control = ri.Cvar_Get( "r_ext_gamma_control", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_compiled_vertex_array = ri.Cvar_Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_LATCH);
	// WIDGET-E1: the `#ifdef __linux__ "0"' arm was ioq3's, not retail's.  Retail
	r_ext_texture_env_add = ri.Cvar_Get( "r_ext_texture_env_add", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_texture_env_combine = ri.Cvar_Get("r_ext_texture_env_combine", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_arb_multisample = ri.Cvar_Get( "r_arb_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_arb_samples = ri.Cvar_Get( "r_arb_samples", "2", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_nv_multisample_filter = ri.Cvar_Get( "r_ext_nv_multisample_filter", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_nv_support_multisample = ri.Cvar_Get( "r_ext_nv_support_multisample", "0", 0 );
	r_draw2d = ri.Cvar_Get("r_draw2d", "1", 0);

	r_2dTrace = ri.Cvar_Get("r_2dTrace", "0", 0);

	r_picmip = ri.Cvar_Get ("r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );

	r_roundImagesDown = ri.Cvar_Get ("r_roundImagesDown", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorMipLevels = ri.Cvar_Get ("r_colorMipLevels", "0", CVAR_LATCH );
	ri.Cvar_CheckRange( r_picmip, 0, 16, qtrue );
	r_detailtextures = ri.Cvar_Get("r_detailtextures", "1", 33);
	r_detailSurfaces = ri.Cvar_Get( "r_detailSurfaces", "1", CVAR_ARCHIVE );
	r_allowGeometryDeforms = ri.Cvar_Get( "r_allowGeometryDeforms", "1", CVAR_ARCHIVE );
	// `0x18a74 li r5,1` (CVAR_ARCHIVE -- NOT the ioq3 LATCH bit the old port line had),
	r_cubeMapping = ri.Cvar_Get( "r_cubeMapping", "0", CVAR_ARCHIVE );
	r_texturebits = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorbits = ri.Cvar_Get( "r_colorbits", "32", CVAR_ARCHIVE | CVAR_LATCH );
	r_stereo = ri.Cvar_Get( "r_stereo", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_stencilbits = ri.Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE | CVAR_LATCH );
	r_depthbits = ri.Cvar_Get("r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_overBrightBits = ri.Cvar_Get ("r_overBrightBits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ignorehwgamma = ri.Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_mode = ri.Cvar_Get("r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH);
	
	r_fullscreen = ri.Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_customwidth = ri.Cvar_Get( "r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = ri.Cvar_Get( "r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH );
	r_customaspect = ri.Cvar_Get( "r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_vertexLight = ri.Cvar_Get("r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_ignoreFastPath = ri.Cvar_Get("r_ignoreFastPath", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_subdivisions = ri.Cvar_Get("r_subdivisions", "3", CVAR_ARCHIVE | CVAR_LATCH);
	ri.Cvar_CheckRange(r_subdivisions, 2, 24, qtrue);

	r_displayRefresh = ri.Cvar_Get( "r_displayRefresh", "0", CVAR_LATCH );
	ri.Cvar_CheckRange( r_displayRefresh, 0, 200, qtrue );
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", CVAR_LATCH|CVAR_CHEAT );
	r_mapOverBrightBits = ri.Cvar_Get ("r_mapOverBrightBits", "0", CVAR_LATCH );
	r_intensity = ri.Cvar_Get ("r_intensity", "1", CVAR_LATCH );
	r_numdebuglines = ri.Cvar_Get("g_numdebuglines", "4096", CVAR_LATCH);
	r_singleShader = ri.Cvar_Get ("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );

	r_lerpmodels = ri.Cvar_Get("r_lerpmodels", "1", 0);
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE|CVAR_CHEAT );
	// 0x18f14 stw into TOC slot 0x227FB8.  "1", not the "0" this line carried -- and
	r_lodbias = ri.Cvar_Get("r_lodbias", "1", CVAR_ARCHIVE);
	r_flares = ri.Cvar_Get ("r_flares", "1", CVAR_ARCHIVE );
	r_flaresMaxDist = ri.Cvar_Get ("r_flaresMaxDist", "768", CVAR_ARCHIVE );
	r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_znear, 0.001f, 200, qtrue );
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", 0 );
	r_fastdlights = ri.Cvar_Get("r_fastdlights", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_drawSun = ri.Cvar_Get( "r_drawSun", "0", CVAR_ARCHIVE );
	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_ARCHIVE );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	r_dynamiclightmaps = ri.Cvar_Get( "r_dynamiclightmaps", "1", CVAR_ARCHIVE );
	r_finish = ri.Cvar_Get ("r_finish", "0", CVAR_ARCHIVE);
	r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	// shipped $$MonitorSync$$ CheckBox force a vid_restart retail never performs
	r_swapInterval = ri.Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE );
	r_gamma = ri.Cvar_Get( "r_gamma", "1.2", CVAR_ARCHIVE );
	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE );

	r_primitives = ri.Cvar_Get( "r_primitives", "0", CVAR_ARCHIVE );

	// `r_ambientScale' in a config does not mean something retail never shipped.
	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.5", CVAR_CHEAT );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );


	r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_TEMP );
	r_showlod = ri.Cvar_Get("r_showlod", "0", CVAR_TEMP | CVAR_CHEAT);

	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
	r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT );
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", CVAR_CHEAT );
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", 0 );
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT );


	r_skipBackEnd = ri.Cvar_Get ("r_skipBackEnd", "0", CVAR_CHEAT);

	r_smp = ri.Cvar_Get ("r_smp", "0", CVAR_ARCHIVE | CVAR_LATCH);

	r_measureOverdraw = ri.Cvar_Get( "r_measureOverdraw", "0", CVAR_CHEAT );
	r_norefresh = ri.Cvar_Get("r_norefresh", "0", CVAR_CHEAT);
	r_drawentities = ri.Cvar_Get("r_drawentities", "1", CVAR_CHEAT);
	r_drawcurves = ri.Cvar_Get("r_drawcurves", "1", CVAR_CHEAT);
	r_drawviewmodel = ri.Cvar_Get("r_drawviewmodel", "1", CVAR_CHEAT);
	r_drawterrain = ri.Cvar_Get("r_drawterrain", "1", CVAR_CHEAT);
	r_drawsprites = ri.Cvar_Get("r_drawsprites", "1", CVAR_CHEAT);
	r_fadeDistMod = ri.Cvar_Get("r_fadeDistMod", "1", CVAR_ARCHIVE);
	r_lodscale = ri.Cvar_Get("r_lodscale", "1", 0);
	r_lodScaleDistance = ri.Cvar_Get("r_lodScaleDistance", "1", CVAR_ARCHIVE);
	r_ignore = ri.Cvar_Get( "r_ignore", "1", CVAR_CHEAT );
	r_nocull = ri.Cvar_Get ("r_nocull", "0", CVAR_CHEAT);
	r_novis = ri.Cvar_Get ("r_novis", "0", CVAR_CHEAT);
	r_showcluster = ri.Cvar_Get ("r_showcluster", "0", CVAR_CHEAT);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", CVAR_CHEAT);
	r_verbose = ri.Cvar_Get( "r_verbose", "0", CVAR_CHEAT );
	r_logFile = ri.Cvar_Get( "r_logFile", "0", CVAR_CHEAT );
	r_debugSurface = ri.Cvar_Get ("r_debugSurface", "0", CVAR_CHEAT);
	r_nobind = ri.Cvar_Get ("r_nobind", "0", CVAR_CHEAT);
	r_showtris = ri.Cvar_Get ("r_showtris", "0", CVAR_CHEAT);
	r_showsky = ri.Cvar_Get ("r_showsky", "0", CVAR_CHEAT);
	r_drawsky = ri.Cvar_Get ("r_drawsky", "1", 0);
	r_shownormals = ri.Cvar_Get ("r_shownormals", "0", CVAR_CHEAT);
	r_clear = ri.Cvar_Get ("r_clear", "0", CVAR_CHEAT);
	r_offsetFactor = ri.Cvar_Get( "r_offsetfactor", "-1", CVAR_CHEAT );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-2", CVAR_CHEAT );
	r_drawBuffer = ri.Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", CVAR_CHEAT);
	r_noportals = ri.Cvar_Get ("r_noportals", "0", CVAR_CHEAT);
	r_shadows = ri.Cvar_Get( "cg_shadows", "1", 0 );


	r_stipplelines = ri.Cvar_Get("r_stipplelines", "1", CVAR_ARCHIVE);
	r_light_lines = ri.Cvar_Get("r_light_lines", "0", CVAR_CHEAT);
	r_light_sun_line = ri.Cvar_Get("r_light_sun_line", "0", CVAR_CHEAT);
	r_light_int_scale = ri.Cvar_Get("r_light_int_scale", "0.05", CVAR_ARCHIVE);
	r_light_nolight = ri.Cvar_Get("r_light_nolight", "0", CVAR_CHEAT | CVAR_ARCHIVE);
	r_light_emphasize = ri.Cvar_Get("r_light_emphasize", "0", 0);
	r_light_emphasizePercent = ri.Cvar_Get("r_light_emphasizePercent", "0", 0);
	r_light_simple = ri.Cvar_Get("r_light_simple", "0", 0);
	r_light_simple_dist = ri.Cvar_Get("r_light_simple_dist", "1000", 0);
	r_light_ambientFudge = ri.Cvar_Get("r_light_ambientFudge", "0.7", 0);
	r_light_ambientEmphasize = ri.Cvar_Get("r_light_ambientEmphasize", "1.6", 0);
	r_skyportal = ri.Cvar_Get("r_skyportal", "0", 0);
	r_skyportal_origin = ri.Cvar_Get("r_skyportal_origin", "0 0 0", 0);
	r_farplane = ri.Cvar_Get("r_farplane", "0", CVAR_CHEAT);
	r_farplane_color = ri.Cvar_Get("r_farplane_color", ".5 .5 .5", CVAR_CHEAT);
	r_farplane_nocull = ri.Cvar_Get("r_farplane_nocull", "0", CVAR_CHEAT);
	r_farplane_nofog = ri.Cvar_Get("r_farplane_nofog", "0", CVAR_CHEAT);
	// data).  Default off: with r_terrain_global 0 the pass at 0x2D8E4..0x2D900 never
	r_terrain_global = ri.Cvar_Get("r_terrain_global", "0", CVAR_CHEAT);
	// 0x218098 = C0F00000_00000000 = -65536.0 -- not the port's "0".
	r_terrain_global_min = ri.Cvar_Get("r_terrain_global_min", va("%f", -65536.0f), CVAR_CHEAT);
	r_skyportal = ri.Cvar_Get("r_skyportal", "0", 0);
	r_skyportal_origin = ri.Cvar_Get("r_skyportal_origin", "0 0 0", 0);
	r_lightcoronasize = ri.Cvar_Get("r_lightcoronasize", ".1", CVAR_ARCHIVE);
	r_useglfog = ri.Cvar_Get("r_useglfog", "1", CVAR_ARCHIVE);
	r_debuglines_depthmask = ri.Cvar_Get("r_debuglines_depthmask", "0", CVAR_ARCHIVE);
	r_fps = ri.Cvar_Get("fps", "0", 0);

	// MENUSET: retail's flag word is CVAR_ARCHIVE alone, not the LATCH pair the ioq3 block
	// vid_restart retail never has, because Menu::CheckRestart only acts on a
	r_anisotropicFiltering = ri.Cvar_Get("r_anisotropicFiltering", "0", CVAR_ARCHIVE);

	ri.Cmd_AddCommand( "totaltexturememinfo", R_TotalTextureMemInfo_f );
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );
	ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	ri.Cmd_AddCommand( "modelist", R_ModeList_f );
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	ri.Cmd_AddCommand( "gfxinfo", GfxInfo_f );
	// PORT-ONLY DIAGNOSTIC (uivk-mt), no retail counterpart: runs the renderer
	ri.Cmd_AddCommand( "r_jobpooltest", R_JobPoolTest_f );
	ri.Cmd_AddCommand( "gles3_firstlight", R_FirstLight_f );


	r_developer = ri.Cvar_Get("developer", "", 0);

	// PORT-ONLY, no retail counterpart and no effect on what is drawn: it runs
	r_jobs = ri.Cvar_Get("r_jobs", "-1", CVAR_ARCHIVE | CVAR_LATCH);

	r_jobsStats = ri.Cvar_Get("r_jobsStats", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription(r_jobsStats, "Print one [MT] measurement line per second: frame-interval "
		"p50/p95/p99/max, front-end and back-end msec, and the job pool's batch coverage. 0 = off.");
}

void R_InitExtensions(void) {
}

void R_Init( void ) {	
	int	err;
	int i;
	byte *ptr;

	ri.Printf( PRINT_ALL, "----- R_Init -----\n" );

	Com_Memset( &tr, 0, sizeof( tr ) );
	Com_Memset( &backEnd, 0, sizeof( backEnd ) );
	Com_Memset( &tess, 0, sizeof( tess ) );

	backEnd.currentSphere = &backEnd.sphere;


	if ( (intptr_t)tess.xyz & 15 ) {
		ri.Printf(PRINT_ALL,  "WARNING: tess.xyz not 16 byte aligned\n" );
	}
	Com_Memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );

	tr.worldEntity.e.shaderRGBA[0] = -1;
	tr.worldEntity.e.shaderRGBA[1] = 0;
	tr.worldEntity.e.shaderRGBA[2] = -1;
	tr.worldEntity.e.shaderRGBA[3] = -1;

	for ( i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		tr.sinTable[i]		= sin( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
		tr.squareTable[i]	= ( i < FUNCTABLE_SIZE/2 ) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if ( i < FUNCTABLE_SIZE / 2 )
		{
			if ( i < FUNCTABLE_SIZE / 4 )
			{
				tr.triangleTable[i] = ( float ) i / ( FUNCTABLE_SIZE / 4 );
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i-FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i-FUNCTABLE_SIZE/2];
		}
	}

	tr.rendererhandle = ri.Milliseconds();

	if (!tr.rendererhandle) {
		tr.rendererhandle = -1;
	}

	R_NoiseInit();

	R_Sky_Init();

	R_Register();

	{
		static qboolean s_smpGltexLogged;

		if ( !s_smpGltexLogged ) {
			s_smpGltexLogged = qtrue;
			ri.Printf( PRINT_ALL,
				"[GLES3:gltex] r_colorbits=%d r_texturebits=%d r_picmip=%d "
				"r_roundImagesDown=%d r_vertexLight=%d r_lightmap=%d r_fullbright=%d "
				"-> R_Upload32 samples==3 arm yields %s\n",
				r_colorbits ? r_colorbits->integer : -1,
				r_texturebits ? r_texturebits->integer : -1,
				r_picmip ? r_picmip->integer : -1,
				r_roundImagesDown ? r_roundImagesDown->integer : -1,
				r_vertexLight ? r_vertexLight->integer : -1,
				r_lightmap ? r_lightmap->integer : -1,
				r_fullbright ? r_fullbright->integer : -1,
				( r_texturebits && r_colorbits
				  && r_texturebits->integer == 0 && r_colorbits->integer == 0 )
					? "the literal 3 (INVALID with format=GL_RGBA)"
					: "a legal RGBA-column format" );
		}
	}

	max_polys = MAX_POLYS;
	max_polyverts = MAX_POLYVERTS;

	for ( i = 0; i < SMP_FRAMES; i++ ) {
		ptr = ri.Malloc(sizeof(backEndData_t) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts);
		backEndDataBuf[i] = (backEndData_t*)ptr;
		backEndDataBuf[i]->polys = (srfPoly_t*)((char*)ptr + sizeof(backEndData_t));
		backEndDataBuf[i]->polyVerts = (polyVert_t*)((char*)ptr + sizeof(backEndData_t) + sizeof(srfPoly_t) * max_polys);
		backEndDataBuf[i]->staticModels = NULL;
		backEndDataBuf[i]->staticModelData = NULL;
		backEndDataBuf[i]->commands.used = 0;
	}
	backEndData = backEndDataBuf[0];
	R_InitNextFrame();

	InitOpenGL();

	GLES3_ProgramsInit();

	R_InitExtensions();

	R_InitImages();

	R_StartupShaders();

	R_ModelInit();

	tr.pFontDebugStrings = NULL;
	g_bInfoworldtris = qfalse;

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ri.Printf (PRINT_ALL, "glGetError() = 0x%x\n", err);

	JobPool_InitCritical( "gljob", r_jobs->integer );

	R_InitCommandBuffers();

	ri.Printf( PRINT_ALL, "----- finished R_Init -----\n" );
}

void RE_Shutdown( qboolean destroyWindow ) {

	ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow );

	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand ("screenshot");
	ri.Cmd_RemoveCommand ("totaltexturememinfo");
	ri.Cmd_RemoveCommand ("imagelist");
	ri.Cmd_RemoveCommand ("shaderlist");
	ri.Cmd_RemoveCommand ("skinlist");
	ri.Cmd_RemoveCommand ("gfxinfo");
	ri.Cmd_RemoveCommand( "modelist" );
	ri.Cmd_RemoveCommand( "r_jobpooltest" );
	ri.Cmd_RemoveCommand( "gles3_firstlight" );

	R_ShutdownCommandBuffers();

	if ( tr.registered ) {
		R_IssuePendingRenderCommands();
		R_DeleteTextures();
	}

	JobPool_Shutdown();

	// SANCTIONED PLATFORM DELTA (GL1 renderer platform layer): retail @0x0001A2B0

	tr.registered = qfalse;

	R_ShutdownShadows();
	RB_2D_OwnersPurged();
	R_ShutdownShaders();
	GLES3_ProgramsShutdown();
	R_ShutdownLensflares();

	for ( int i = 0; i < SMP_FRAMES; i++ ) {
		if ( backEndDataBuf[i] ) {
			ri.Free( backEndDataBuf[i] );
			backEndDataBuf[i] = NULL;
		}
	}
	backEndData = NULL;

	R_FreeModels();

	// @0x0000CF10 exist -- and RE_Shutdown never touches the font pool.  s_loadedFonts[]

	if ( destroyWindow ) {
		GLimp_Shutdown();

		Com_Memset( &glConfig, 0, sizeof( glConfig ) );
		textureFilterAnisotropic = qfalse;
		maxAnisotropy = 0;
		displayAspect = 0.0f;
		haveClampToEdge = qfalse;

		Com_Memset( &glState, 0, sizeof( glState ) );
	}

	ri.Hunk_Clear();
}


void RE_BeginRegistration(glconfig_t* glconfigOut) {

	SMP_RLOAD_BEGIN( reginit );

	ri.UI_LoadResource("*123");

	R_IssuePendingRenderCommands();

	// (@0x00028D50), and the order matters: sprite_t and bmodel_t are hunk allocations
	R_FreeNonTikiModels();

	/* Names the batch holds die below. */
	RB_2D_OwnersPurged();

	ri.Hunk_Clear();

	*glconfigOut = glConfig;

	// the port's stand-in for retail's never-reset .bss static.  It grows by exactly one
	{
		cvar_t *generation;
		char    value[16];

		generation = ri.Cvar_Get("r_renderergeneration", "0", CVAR_NORESTART);

		r_sequencenumber = generation->integer + 1;
		if (r_sequencenumber <= 0) {
			r_sequencenumber = 1;
		}

		Com_sprintf(value, sizeof(value), "%d", r_sequencenumber);
		ri.Cvar_Set("r_renderergeneration", value);
	}
	tr.registrating = qtrue;
	tr.viewCluster = -1;		
	RE_ClearScene();

	R_ClearWorld();

	SMP_RLOAD_BEGIN( setup );
	R_SetupShaders();
	SMP_RLOAD_END( setup );
	R_InitLensFlare();
	tr.registered = qtrue;

	ri.UI_LoadResource("*124");

	SMP_RLOAD_END( reginit );
}

void RE_EndRegistration( void ) {
	tr.registrating = qfalse;
	R_IssuePendingRenderCommands();

	GLES3_PrebuildPrograms();
	GLES3_CloseProgramRegistration();

	R_FreeUnusedImages();
}

int RE_GetRegistrationSequence( void ) {
	return r_sequencenumber;
}

void RE_SetRenderTime(int t) {
	backEnd.refdef.floatTime = (long double)t / 1000.0;
	R_UpdateGhostTextures();
}

void RE_SetDynamicLights( const byte *defaultIntensities, const byte *intensities ) {
	int		i;
	byte	intensity;

	if ( !tr.world ) {
		return;
	}

	for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
		if ( tr.world->lightGroupDefaultIntensity[ i ] != defaultIntensities[ i ] ) {
			tr.world->lightGroupDefaultIntensity[ i ] = defaultIntensities[ i ];
			tr.world->lightGroupDefaultFrame[ i ] = tr.frameCount;
		}
	}

	for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
		if ( r_dynamiclightmaps->integer ) {
			intensity = intensities[ i ];
		} else if ( defaultIntensities[ i ] == DYNAMIC_LIGHT_NO_DEFAULT ) {
			intensity = intensities[ i ];
		} else {
			intensity = defaultIntensities[ i ];
		}

		if ( tr.world->lightGroupIntensity[ i ] != intensity ) {
			tr.world->lightGroupIntensity[ i ] = intensity;
			tr.world->lightGroupFrame[ i ] = tr.frameCount;
		}
	}
}

void RE_SetWindDirection( vec3_t direction ) {
	if ( !tr.world ) {
		return;
	}

	tr.world->windDirection[ 0 ] = direction[ 0 ];
	tr.world->windDirection[ 1 ] = direction[ 1 ];
	tr.world->windDirection[ 2 ] = direction[ 2 ];
}

void RE_SetWindIntensity( float intensity ) {
	if ( !tr.world ) {
		return;
	}

	tr.world->windIntensity = intensity;
}

void RE_SetTimeScale( float timeScale ) {
	if ( !tr.world ) {
		return;
	}

	tr.world->timeScale = timeScale;
}

#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#else
refexport_t *GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#endif

	static refexport_t	re;

	ri = *rimp;

	Com_Memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", 
			REF_API_VERSION, apiVersion );
		return NULL;
	}

	R_Init();


	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.ForceRegisterModel = RE_ForceRegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.RefreshShaderNoMip = RE_RefreshShaderNoMip;

	re.EndRegistration = RE_EndRegistration;

	re.GetRegistrationSequence = RE_GetRegistrationSequence;
	re.ReplaceShader = RE_ReplaceShader;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.LoadWorld = RE_LoadWorldMap;

	re.ClearScene = RE_ClearScene;
	re.ClearFlares = R_ClearLensflareStates;
	re.SetDynamicLights = RE_SetDynamicLights;
	re.SetWindDirection = RE_SetWindDirection;
	re.SetWindIntensity = RE_SetWindIntensity;
	re.SetTimeScale = RE_SetTimeScale;

	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddRefSpriteToScene = RE_AddRefSpriteToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddLensflareToScene = RE_AddLensflareToScene;
	re.LensflareNameToNum = R_LensflareNameToNum;
	re.RenderScene = RE_RenderScene;
	re.GetRenderEntity = RE_GetRenderEntity;
	re.SavePerformanceCounters = R_SavePerformanceCounters;

	re.SetColor = Draw_SetColor;
	re.Set2DWindow = Set2DWindow;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawTilePic = Draw_TilePic;
	re.DrawTilePicOffset = Draw_TilePicOffset;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;
	re.DebugLine = R_DebugLine;
	re.DrawBox = DrawBox;
	re.AddBox = AddBox;

	re.BeginFrame = RE_BeginFrame;
	re.Scissor = RE_Scissor;
	re.DrawLineLoop = DrawLineLoop;
	re.EndFrame = RE_EndFrame;
	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;
	re.ModelRadius = R_ModelRadius;

	re.R_Model_GetHandle = R_Model_GetHandle;
	re.TIKI_FlushAll = TIKI_FlushAll;
	re.DrawString = R_DrawString;
	re.GetFontHeight = R_GetFontHeight;
	re.GetFontStringWidth = R_GetFontStringWidth;
	re.LoadFont = R_LoadFont;

	re.SwipeBegin = RE_SwipeBegin;
	re.SwipePoint = RE_SwipePoint;
	re.SwipeEnd = RE_SwipeEnd;
	re.SetRenderTime = RE_SetRenderTime;
	re.Noise = R_NoiseGet4f;
	re.SetMode = R_SetMode;
	re.SetFullscreen = R_SetFullscreen;

	re.GetShaderWidth = RE_GetShaderWidth;
	re.GetShaderHeight = RE_GetShaderHeight;
	re.GetGraphicsInfo = RE_GetGraphicsInfo;

	re.LightForPoint = (void (*)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir ))R_LightForPoint;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.SpawnEffectModel = RE_SpawnEffectModel;
	re.RegisterServerModel = RE_RegisterServerModel;
	re.UnregisterServerModel = RE_UnregisterServerModel;
	re.PrintBSPFileSizes = RE_PrintBSPFileSizes;
	re.MapVersion = RE_MapVersion;
	re.AddTerrainMarkToScene = RE_AddTerrainMarkToScene;
	re.DrawStretchPic2 = Draw_StretchPic2;
	re.DrawTrianglePic = Draw_TrianglePic;
	re.MarkFragmentsForInlineModel = R_MarkFragmentsForInlineModel;
	re.GetInlineModelBounds = R_GetInlineModelBounds;
	re.GetLightingForDecal = R_GetLightingForDecal;
	re.GetLightingForSmoke = R_GetLightingForSmoke;
	re.R_GatherLightSources = R_GatherLightSources;
	re.GetShaderName = RE_GetShaderName;
	re.GetModelName = RE_GetModelName;
	re.ForceUpdatePose = RE_ForceUpdatePose;
	re.TIKI_Orientation = RE_TIKI_Orientation;
	re.TIKI_IsOnGround = RE_TIKI_IsOnGround;
	re.SetFrameNumber = RE_SetFrameNumber;
	re.ImageExists = R_ImageExists;
	re.CountTextureMemory = R_CountTextureMemory;
	re.LoadRawImage = R_LoadRawImage;
	re.FreeRawImage = R_FreeRawImage;
	re.Set2DInitialShaderTime = Set2DInitialShaderTime;
	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;
	re.TakeVideoFrame = RE_TakeVideoFrame;

	return &re;
}
