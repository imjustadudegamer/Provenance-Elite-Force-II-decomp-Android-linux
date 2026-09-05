/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
#include "tr_local.h"
#include "tr_hwtex.h"
#include "gles3_firstlight.h"

#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <string.h>

SDL_Window *SDL_window = NULL;
static SDL_GLContext sdl_gl_context = NULL;

int      smp_texTranscode = SMP_TEXC_NONE;
qboolean smp_hasAstcLdr = qfalse;
qboolean smp_hasAstcSliced3d = qfalse;
qboolean smp_hasEtc2Core = qfalse;

static cvar_t *r_allowSoftwareGL;
static cvar_t *r_allowResize;
static cvar_t *r_centerWindow;
static cvar_t *r_sdlDriver;
static cvar_t *r_preferOpenGLES;

cvar_t *gles3_gles3_allow_missing_core;	
cvar_t *gles3_gles3_require_position;	

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_UNKNOWN
} rserr_t;

#define R_MODE_FALLBACK 3  

/* The renderer asks for ES 3.2 and runs on nothing lower: a refusal is a
 * refusal.  The requested values are here so the startup line and the refusal
 * line cannot drift from the attributes actually set. */
#define GLES3_CONTEXT_MAJOR 3
#define GLES3_CONTEXT_MINOR 2

static void *GLES3_ResolveProc(const char *name);

/* A refused 3.2 request has to name what the device offered.  This opens one
 * throwaway context with no version attribute (the driver then picks its
 * highest) only to read the version and renderer strings, and destroys it; the
 * renderer never runs on it. */
static void GLES3_LogOfferedVersion(void)
{
	typedef const unsigned char *(*gles3GetString_t)(unsigned int);
	gles3GetString_t getString;
	SDL_GLContext	 probe;
	const char		*version, *renderer;

	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	probe = SDL_GL_CreateContext(SDL_window);
	if (!probe) {
		ri.Printf(PRINT_ALL,
				  "[GLES3:gles3] context requested=%d.%d offered=\"none\" (%s)\n",
				  GLES3_CONTEXT_MAJOR, GLES3_CONTEXT_MINOR, SDL_GetError());
		return;
	}

	getString = (gles3GetString_t)GLES3_ResolveProc("glGetString");
	version   = getString ? (const char *)getString(0x1F02 /* GL_VERSION */) : NULL;
	renderer  = getString ? (const char *)getString(0x1F01 /* GL_RENDERER */) : NULL;
	ri.Printf(PRINT_ALL,
			  "[GLES3:gles3] context requested=%d.%d offered=\"%s\" renderer=\"%s\"\n",
			  GLES3_CONTEXT_MAJOR, GLES3_CONTEXT_MINOR,
			  version ? version : "(null)", renderer ? renderer : "(null)");
	SDL_GL_DeleteContext(probe);
}

static void *GLES3_ResolveProc(const char *name)
{
	void *p = (void *)SDL_GL_GetProcAddress(name);

#if defined( __ANDROID__ ) || defined( __linux__ )
	if (!p) {
		typedef void *(*eglgetproc_t)(const char *);
		static eglgetproc_t egl_get = NULL;
		static int tried = 0;

		if (!tried) {
			tried = 1;
			egl_get = (eglgetproc_t)SDL_GL_GetProcAddress("eglGetProcAddress");
		}
		if (egl_get) {
			p = egl_get(name);
		}
	}
#endif
	return p;
}

/* The loader's per-event rows are instruments; the identity and condition rows
 * are not, and print regardless. */
static void GLES3_LogToEngine(const char *line)
{
	static const char *const eventRows[2] = { "[GLES3:glfixed]", "[GLES3:glcap]" };
	size_t i;

	for (i = 0; i < sizeof(eventRows) / sizeof(eventRows[0]); i++) {
		if (strncmp(line, eventRows[i], strlen(eventRows[i])) == 0) {
			if (!R_ProbeStatsOn()) {
				return;
			}
			break;
		}
	}
	ri.Printf(PRINT_ALL, "%s", line);
}

static void GLES3_FreeContext(void)
{
	if (sdl_gl_context) {
		SDL_GL_DeleteContext(sdl_gl_context);
		sdl_gl_context = NULL;
	}
	if (SDL_window) {
		SDL_DestroyWindow(SDL_window);
		SDL_window = NULL;
	}
	SDL_GL_ResetAttributes();
}

static qboolean GLES3_TryContext(int width, int height, qboolean fullscreen,
								 int profileMask, int depthBits, int stencilBits,
								 int colorBits, const char *what)
{
	Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL;
	int display = 0;
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;
	SDL_DisplayMode desktopMode;

	if (r_allowResize->integer) {
		flags |= SDL_WINDOW_RESIZABLE;
	}
	if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	if (SDL_GetDesktopDisplayMode(display, &desktopMode) == 0) {
		glConfig.windowAspect = (float)desktopMode.w / (float)desktopMode.h;
		if (r_centerWindow->integer && !fullscreen) {
			x = (desktopMode.w / 2) - (width / 2);
			y = (desktopMode.h / 2) - (height / 2);
		}
	}

	glConfig.vidWidth = width;
	glConfig.vidHeight = height;
	glConfig.isFullscreen = fullscreen;

	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						profileMask ? profileMask : SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GLES3_CONTEXT_MAJOR);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GLES3_CONTEXT_MINOR);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthBits);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilBits);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if (r_stereo->integer) {
		glConfig.stereoEnabled = qtrue;
		SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
	} else {
		glConfig.stereoEnabled = qfalse;
		SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
	}

	ri.Printf(PRINT_ALL,
			  "[GLES3:gles3] context attempt %s requested=%d.%d (%dx%d depth=%d stencil=%d)\n",
			  what, GLES3_CONTEXT_MAJOR, GLES3_CONTEXT_MINOR,
			  width, height, depthBits, stencilBits);

	SDL_window = SDL_CreateWindow(CLIENT_WINDOW_TITLE, x, y, width, height, flags);
	if (!SDL_window) {
		ri.Printf(PRINT_ALL, "[GLES3:gles3] SDL_CreateWindow failed: %s\n", SDL_GetError());
		return qfalse;
	}

	sdl_gl_context = SDL_GL_CreateContext(SDL_window);
	if (!sdl_gl_context) {
		ri.Printf(PRINT_ALL,
				  "[GLES3:gles3] context REFUSED: requested=%d.%d not granted (%s)\n",
				  GLES3_CONTEXT_MAJOR, GLES3_CONTEXT_MINOR, SDL_GetError());
		GLES3_LogOfferedVersion();
		GLES3_FreeContext();
		return qfalse;
	}

	QGL_GLES3_SetGetProcAddress(GLES3_ResolveProc);
	QGL_GLES3_SetLogger(GLES3_LogToEngine);
	{
		int staged = 0;
		int missingCore = QGL_GLES3_LoadProcs(&staged);

		QGL_GLES3_LogDeviceInfo();
		{
			const char *grantedRenderer =
				qglGetString ? (const char *)qglGetString(GL_RENDERER) : NULL;

			ri.Printf(PRINT_ALL,
					  "[GLES3:gles3] context requested=%d.%d granted=%d.%d renderer=\"%s\"\n",
					  GLES3_CONTEXT_MAJOR, GLES3_CONTEXT_MINOR,
					  qglesMajorVersion, qglesMinorVersion,
					  grantedRenderer ? grantedRenderer : "(null)");
		}
		ri.Printf(PRINT_ALL, "[GLES3:gles3] procs=%d core=%d core32=%d core_missing=%d staged_desktop_traps=%d\n",
				  QGL_GLES3_TotalProcs(), QGL_GLES3_CoreProcs(), QGL_GLES3_Core32Procs(),
				  missingCore, staged);
		if (missingCore > 0) {
			ri.Printf(PRINT_ALL, "[GLES3:glmissing] core_list=%s\n",
					  QGL_GLES3_CoreMissingList());
			if (!gles3_gles3_allow_missing_core->integer) {
				ri.Error(ERR_FATAL,
						 "GLES3: %d required GLES 3.0 core entry point(s) missing: %s "
						 "(set gles3_gles3_allow_missing_core 1 to continue for triage)",
						 missingCore, QGL_GLES3_CoreMissingList());
			}
		}
	}

	{
		int r = 0, g = 0, b = 0;

		SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
		SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
		SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
		glConfig.colorBits = r + g + b;
		glConfig.depthBits = depthBits;
		glConfig.stencilBits = stencilBits;
		SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &glConfig.depthBits);
		SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &glConfig.stencilBits);
	}

	Q_strncpyz(glConfig.vendor_string,
			   (const char *)qglGetString(GL_VENDOR), sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string,
			   (const char *)qglGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
	if (*glConfig.renderer_string &&
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] == '\n') {
		glConfig.renderer_string[strlen(glConfig.renderer_string) - 1] = 0;
	}
	Q_strncpyz(glConfig.version_string,
			   (const char *)qglGetString(GL_VERSION), sizeof(glConfig.version_string));

	{
		GLint numExtensions = 0;
		int i, listLength = 0;

		glConfig.extensions_string[0] = '\0';
		if (qglGetStringi) {
			qglGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
			for (i = 0; i < numExtensions; i++) {
				const char *extension = (const char *)qglGetStringi(GL_EXTENSIONS, (GLuint)i);
				int extensionLength;

				if (!extension) {
					continue;
				}
				extensionLength = (int)strlen(extension);
				if ((listLength + extensionLength + 1) >=
					(int)sizeof(glConfig.extensions_string)) {
					break;
				}
				if (i > 0) {
					Q_strcat(glConfig.extensions_string,
							 sizeof(glConfig.extensions_string), " ");
					listLength++;
				}
				Q_strcat(glConfig.extensions_string,
						 sizeof(glConfig.extensions_string), extension);
				listLength += extensionLength;
			}
		}
	}

#ifdef __ANDROID__
	/* Android: vsync is forced off.  A pending swap stalls every subsequent GL
	 * call on this driver -- buffer writes included -- so a vsync'd sparse frame
	 * pays the swap inside its writes.  The cvar stays; desktop honours it. */
	SDL_GL_SetSwapInterval( 0 );
#else
	SDL_GL_SetSwapInterval(r_swapInterval->integer);
#endif

	return qtrue;
}

static int GLES3_SetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	int width, height;
	float aspect;
	int depthBits, stencilBits, colorBits;
	int attempt;

	(void)noborder; 

	ri.Printf(PRINT_ALL, "Initializing GLES 3.0 display\n");

	if (mode == -2) {
		SDL_DisplayMode desktopMode;

		if (SDL_GetDesktopDisplayMode(0, &desktopMode) == 0 && desktopMode.h > 0) {
			width = desktopMode.w;
			height = desktopMode.h;
		} else {
			width = 640;
			height = 480;
		}
		glConfig.windowAspect = (float)width / (float)height;
	} else if (!R_GetModeInfo(&width, &height, &aspect, mode)) {
		return RSERR_INVALID_MODE;
	}

	colorBits = r_colorbits->value;
	if (!colorBits || colorBits >= 32) {
		colorBits = 24;
	}
	depthBits = r_depthbits->value ? r_depthbits->value : 24;
	stencilBits = r_stencilbits->value;

	for (attempt = 0; attempt < 3; attempt++) {
		int tryDepth = depthBits, tryStencil = stencilBits;
		int profileMask = SDL_GL_CONTEXT_PROFILE_ES;

		if (attempt == 1) {
			tryDepth = (depthBits == 24) ? 16 : depthBits;
		} else if (attempt == 2) {
			tryStencil = 0;
			profileMask = 0;
		}

		if (GLES3_TryContext(width, height, fullscreen, profileMask,
							 tryDepth, tryStencil, colorBits, "es3")) {
			SDL_ShowWindow(SDL_window);

			glConfig.deviceSupportsGamma = !r_ignorehwgamma->integer &&
				SDL_SetWindowBrightness(SDL_window, 1.0f) >= 0;

			glConfig.driverType = GLDRV_ICD;
			glConfig.hardwareType = GLHW_GENERIC;

			ri.Printf(PRINT_ALL, "Using %d color bits, %d depth, %d stencil display.\n",
					  glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
			ri.Printf(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
			return RSERR_OK;
		}
	}

	GLES3_FreeContext();
	ri.Printf(PRINT_ALL, "Couldn't get an ES 3.0 visual\n");
	return RSERR_INVALID_MODE;
}

static qboolean GLES3_StartDriverAndSetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_SetHint(SDL_HINT_APP_NAME, PRODUCT_NAME);
		if (SDL_Init(SDL_INIT_VIDEO) != 0) {
			ri.Printf(PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n", SDL_GetError());
			return qfalse;
		}
		ri.Printf(PRINT_ALL, "SDL using driver \"%s\"\n", SDL_GetCurrentVideoDriver());
		ri.Cvar_Set("r_sdlDriver", SDL_GetCurrentVideoDriver());
	}

	if (fullscreen && ri.Cvar_VariableIntegerValue("in_nograb")) {
		ri.Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set("r_fullscreen", "0");
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}

	switch (GLES3_SetMode(mode, fullscreen, noborder)) {
	case RSERR_INVALID_FULLSCREEN:
		ri.Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
		return qfalse;
	case RSERR_INVALID_MODE:
		ri.Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
		return qfalse;
	default:
		break;
	}
	return qtrue;
}

static void GLES3_InitExtensions(void)
{
	qboolean s3tc, etc1, etc2, astc, astcSliced3d, aniso, envAdd, envCombine;
	int      coreEtc2;

	if (!r_allowExtensions->integer) {
		ri.Printf(PRINT_ALL, "* IGNORING GLES EXTENSIONS *\n");
		return;
	}

	ri.Printf(PRINT_ALL, "Initializing GLES extensions\n");

	s3tc = QGL_GLES3_HasExtension("GL_EXT_texture_compression_s3tc") ? qtrue : qfalse;
	etc1 = QGL_GLES3_HasExtension("GL_OES_compressed_ETC1_RGB8_texture") ? qtrue : qfalse;
	astc = QGL_GLES3_HasExtension("GL_KHR_texture_compression_astc_ldr") ? qtrue : qfalse;
	astcSliced3d = QGL_GLES3_HasExtension("GL_KHR_texture_compression_astc_sliced_3d") ? qtrue : qfalse;

	coreEtc2 = (qglesMajorVersion > 3 ||
				(qglesMajorVersion == 3 && qglesMinorVersion >= 0)) ? qtrue : qfalse;
	etc2 = coreEtc2
		   || QGL_GLES3_HasExtension("GL_EXT_texture_compression_etc2")
		   || QGL_GLES3_HasExtension("GL_ARB_ES3_compatibility")
		   ? qtrue : qfalse;

	aniso = QGL_GLES3_HasExtension("GL_EXT_texture_filter_anisotropic") ? qtrue : qfalse;
	envAdd = QGL_GLES3_HasExtension("GL_EXT_texture_env_add") ? qtrue : qfalse;
	envCombine = QGL_GLES3_HasExtension("GL_EXT_texture_env_combine") ? qtrue : qfalse;

	glConfig.textureCompression = TC_NONE;
	if (s3tc && r_ext_compress_textures->value) {
		glConfig.textureCompression = TC_S3TC_ARB;
	} else if (s3tc) {
		ri.Printf(PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc (r_ext_compress_textures 0)\n");
	} else {
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n");
	}

	smp_hasEtc2Core = coreEtc2;
	smp_hasAstcLdr = astc;
	smp_hasAstcSliced3d = astcSliced3d;
	smp_texTranscode = SMP_SelectTexTranscode(
		qglesMajorVersion, qglesMinorVersion,
		s3tc ? 1 : 0, etc2 ? 1 : 0,
		QGL_GLES3_HasExtension("GL_ARB_ES3_compatibility") ? 1 : 0,
		astc ? 1 : 0, r_ext_compress_textures->value ? 1 : 0);

	ri.Printf(PRINT_ALL,
			  "[GLES3:gltex] s3tc=%d etc1=%d etc2=%d etc2_core=%d astc_ldr=%d astc_sliced_3d=%d "
			  "-> compressed_path=%s transcode=%s software_decode=%s\n",
			  s3tc ? 1 : 0, etc1 ? 1 : 0, etc2 ? 1 : 0, coreEtc2 ? 1 : 0,
			  astc ? 1 : 0, astcSliced3d ? 1 : 0,
			  (glConfig.textureCompression != TC_NONE) ? "glCompressedTexImage2D" : "none",
			  (smp_texTranscode == SMP_TEXC_ASTC_4x4) ? "glCompressedTexImage2D/ASTC_4x4"
			  : (smp_texTranscode == SMP_TEXC_ETC2_RGBA8) ? "glCompressedTexImage2D/ETC2_RGBA8"
															: "none",
			  (glConfig.textureCompression == TC_NONE &&
			   smp_texTranscode == SMP_TEXC_NONE) ? "yes" : "no");

	glConfig.textureEnvAddAvailable = (envAdd && r_ext_texture_env_add->integer) ? qtrue : qfalse;
	glConfig.textureEnvCombineAvailable = envCombine ? qtrue : qfalse;
	glConfig.cubeMapAvailable = qtrue; 
	glConfig.maxTextureSize = 0;
	glConfig.numTextureUnits = 0;
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize);
	qglGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &glConfig.numTextureUnits);
	ri.Printf(PRINT_ALL, "MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize);
	ri.Printf(PRINT_ALL, "MAX_TEXTURE_UNITS: %d\n", glConfig.numTextureUnits);

	textureFilterAnisotropic = aniso ? qtrue : qfalse;
	maxAnisotropy = 0;
	if (aniso) {
		GLfloat maxA = 0.0f;
		qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxA);
		maxAnisotropy = (int)maxA;
	}
	ri.Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n",
			  maxAnisotropy);

	ri.Printf(PRINT_ALL, "[GLES3:glcaps] env_add=%d env_combine=%d cube_map=1\n",
			  glConfig.textureEnvAddAvailable ? 1 : 0,
			  glConfig.textureEnvCombineAvailable ? 1 : 0);
}

void GLimp_Init(qboolean fixedFunction)
{
	if (fixedFunction) {
		ri.Printf(PRINT_ALL, "[GLES3:gles3] GLimp_Init(fixedFunction=1) -- GLES 3.0 "
							 "has no fixed-function pipeline; GLSL ES 3.0 programs "
							 "replace it (G2)\n");
	}

	r_allowSoftwareGL = ri.Cvar_Get("r_allowSoftwareGL", "0", CVAR_LATCH);
	r_sdlDriver = ri.Cvar_Get("r_sdlDriver", "", CVAR_ROM);
	r_allowResize = ri.Cvar_Get("r_allowResize", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_centerWindow = ri.Cvar_Get("r_centerWindow", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_preferOpenGLES = ri.Cvar_Get("r_preferOpenGLES", "1", CVAR_ARCHIVE | CVAR_LATCH);
	(void)r_allowSoftwareGL;
	(void)r_preferOpenGLES;
	gles3_gles3_allow_missing_core =
		ri.Cvar_Get("gles3_gles3_allow_missing_core", "0", CVAR_TEMP);

#ifdef __ANDROID__
	/* Android: the full-screen-antialiasing cvars are forced off.  The GLES
	 * surface never carries multisample attributes, so they were inert anyway;
	 * pinning them keeps the video menu honest. */
	r_ati_fsaa_samples = ri.Cvar_Get("r_ati_fsaa_samples", "0", CVAR_ARCHIVE);
	r_nv_fsaa_samples = ri.Cvar_Get("r_nv_fsaa_samples", "0", CVAR_ARCHIVE | CVAR_LATCH);
#else
	r_ati_fsaa_samples = ri.Cvar_Get("r_ati_fsaa_samples", "0", CVAR_ARCHIVE);
	r_nv_fsaa_samples = ri.Cvar_Get("r_nv_fsaa_samples", "4", CVAR_ARCHIVE | CVAR_LATCH);
#endif

	if (ri.Cvar_VariableIntegerValue("com_abnormalExit")) {
		ri.Cvar_Set("r_mode", va("%d", R_MODE_FALLBACK));
		ri.Cvar_Set("r_fullscreen", "0");
		ri.Cvar_Set("r_centerWindow", "0");
		ri.Cvar_Set("com_abnormalExit", "0");
	}

	ri.Sys_GLimpInit();

	if (GLES3_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, qfalse)) {
		goto success;
	}

	ri.Sys_GLimpSafeInit();

	if (GLES3_StartDriverAndSetMode(r_mode->integer, r_fullscreen->integer, qfalse)) {
		goto success;
	}

	if (r_mode->integer != R_MODE_FALLBACK) {
		ri.Printf(PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n",
				  r_mode->integer, R_MODE_FALLBACK);
		if (GLES3_StartDriverAndSetMode(R_MODE_FALLBACK, qfalse, qfalse)) {
			goto success;
		}
	}

	ri.Error(ERR_FATAL,
			 "GLimp_Init() - no OpenGL ES %d.%d context: every configuration was refused; "
			 "see the [GLES3:gles3] context lines above for the version the device offered",
			 GLES3_CONTEXT_MAJOR, GLES3_CONTEXT_MINOR);

success:
	GLES3_InitExtensions();
	ri.Cvar_Get("r_availableModes", "", CVAR_ROM);
	ri.IN_Init(SDL_window);
}

void GLimp_Shutdown(void)
{
	GLES3_FirstLight_Shutdown();
	QGL_GLES3_SetLogger(NULL);
	GLES3_FirstLight_SetLogger(NULL);
	GLES3_FreeContext();
	ri.IN_Shutdown();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void GLimp_Minimize(void)
{
	SDL_MinimizeWindow(SDL_window);
}

void GLimp_LogComment(char *comment)
{
	(void)comment;
}

#define SMP_CTX_PROBE_CAP 8

static void GLES3_ContextAttest(void)
{
	static int lastMinimized = -1;
	static int rows = 0;
	int minimized;
	GLint prog = 0;
	GLenum err;

	minimized = ri.Cvar_VariableIntegerValue("com_minimized");

	if (lastMinimized < 0 || (lastMinimized && !minimized)) {
		if (R_ProbeStatsOn() && rows < SMP_CTX_PROBE_CAP + 1) {
			qglGetIntegerv(GL_CURRENT_PROGRAM, &prog);
			err = qglGetError();
			rows++;
			ri.Printf(PRINT_ALL,
					  "[SMP:ctx] row=%d edge=%s minimized=%d white=%d scratch=%d dlight=%d "
					  "prog=%d isprog=%d glerr=0x%x\n",
					  rows, (lastMinimized < 0) ? "baseline" : "resume", minimized,
					  (int)qglIsTexture(tr.whiteImage ? tr.whiteImage->texnum : 0),
					  (int)qglIsTexture(tr.scratchImage ? tr.scratchImage->texnum : 0),
					  (int)qglIsTexture(tr.dlightImage ? tr.dlightImage->texnum : 0),
					  (int)prog, (int)qglIsProgram((GLuint)prog), (unsigned)err);
		}
	}

	lastMinimized = minimized;
}

qboolean GLimp_GLContextMakeCurrent( qboolean makeCurrent )
{
	if ( SDL_window == NULL ) {
		return qfalse;
	}
	if ( makeCurrent && sdl_gl_context == NULL ) {
		return qfalse;
	}
	if ( SDL_GL_MakeCurrent( SDL_window, makeCurrent ? sdl_gl_context : NULL ) != 0 ) {
		return qfalse;
	}
	return qtrue;
}

void GLimp_EndFrame(void)
{
	GLES3_ContextAttest();
	if (r_ati_fsaa_samples->modified) {
		const char *vendor = (const char *)qglGetString(GL_VENDOR);
		int v;

		r_ati_fsaa_samples->modified = qfalse;
		if (vendor && (strstr(vendor, "ATI Radeon") || strstr(vendor, "ATI R-200"))) {
			v = r_ati_fsaa_samples->integer;
			if ((unsigned int)(v - 1) <= 1 || v == 4) {
				ri.Printf(PRINT_ALL, "Changing ATI FSAA samples to %d\n", v);
			} else {
				ri.Printf(PRINT_ALL, "Valid ATI FSAA samples are 1, 2, 4.  Resetting to 1\n");
				ri.Cvar_Set("r_ati_fsaa_samples", "1");
			}
		}
	}

	if (Q_stricmp(r_drawBuffer->string, "GL_FRONT") != 0) {
		Uint32 smpSwapStart = SDL_GetTicks();

		SDL_GL_SwapWindow(SDL_window);

		ri.NoteBlockedMsec( (int)( SDL_GetTicks() - smpSwapStart ) );
	}

	if (r_fullscreen->modified) {
		int fullscreen = !!(SDL_GetWindowFlags(SDL_window) & SDL_WINDOW_FULLSCREEN);
		qboolean needToToggle;
		qboolean sdlToggled = qfalse;

		if (r_fullscreen->integer && ri.Cvar_VariableIntegerValue("in_nograb")) {
			ri.Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
			ri.Cvar_Set("r_fullscreen", "0");
			r_fullscreen->modified = qfalse;
		}

		needToToggle = (!!r_fullscreen->integer != fullscreen) ? qtrue : qfalse;
		if (needToToggle) {
			sdlToggled = (SDL_SetWindowFullscreen(SDL_window, r_fullscreen->integer) >= 0)
						 ? qtrue : qfalse;
			if (!sdlToggled) {
				ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
			}
			ri.IN_Restart();
		}
		r_fullscreen->modified = qfalse;
	}
}

#define SMP_RLOAD_WINDOW_MS	1000
#define SMP_RLOAD_MIN_MS		30	
#define SMP_RLOAD_MAX_ROWS		720

cvar_t	*smp_rloadStats = NULL;
int		smp_rloadDepth = 0;

static int	smp_rloadRows;
static int	smp_rloadStopped;
static int	smp_rloadWindowStart;
static unsigned int	smp_rloadWindowFrames;
static int	smp_rloadSums[SMP_RLZ_COUNT], smp_rloadMaxs[SMP_RLZ_COUNT];
static int	smp_rloadWorldBrackets;	
static int	smp_rloadRegBrackets;

int SMP_RLoadNow( void ) {
	if ( !smp_rloadStats ) {
		smp_rloadStats = ri.Cvar_Get( "smp_loadStats", "1", 0 );
	}
	return (int)SDL_GetTicks();
}

static void SMP_RLoadFlush( int window ) {
	static const char *zoneName[SMP_RLZ_COUNT] = {
		"world", "bspread", "shaders", "lightmap", "lods",
		"reginit", "imginit", "setup", "findimg", "decode",
		"transcode", "upload", "modelreg"
	};
	char	line[1024], part[64];
	int		i, mean10, frames, unacctW;

	if ( smp_rloadStopped || !SMP_RLoadArmed() ) {
		return;
	}

	unacctW = smp_rloadSums[SMP_RLZ_world]
			- smp_rloadSums[SMP_RLZ_bspread]
			- smp_rloadSums[SMP_RLZ_shaders]
			- smp_rloadSums[SMP_RLZ_lightmap]
			- smp_rloadSums[SMP_RLZ_lods];
	if ( unacctW < 0 ) unacctW = 0;

	frames = (int)smp_rloadWindowFrames;
	if ( frames < 1 ) frames = 1;

	line[0] = '\0';
	for ( i = 0; i < SMP_RLZ_COUNT; i++ ) {
		mean10 = (int)( ( (long long)smp_rloadSums[i] * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zoneName[i], mean10 / 10, mean10 % 10, smp_rloadMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ), " unacctWsum=%d", unacctW );
	Q_strcat( line, sizeof( line ), part );

	ri.Printf( PRINT_ALL, "[SMP:rload] window=%dms worldN=%d regN=%d%s\n",
			   window, smp_rloadWorldBrackets, smp_rloadRegBrackets, line );

	smp_rloadRows++;
	if ( smp_rloadRows >= SMP_RLOAD_MAX_ROWS ) {
		smp_rloadStopped = 1;
		ri.Printf( PRINT_ALL, "[SMP:rload] capped at %d rows; no further [SMP:rload] lines\n",
				   SMP_RLOAD_MAX_ROWS );
	}

	for ( i = 0; i < SMP_RLZ_COUNT; i++ ) {
		smp_rloadSums[i] = smp_rloadMaxs[i] = 0;
	}
	smp_rloadWindowFrames = 0;
	smp_rloadWorldBrackets = 0;
	smp_rloadRegBrackets = 0;
	smp_rloadWindowStart = SMP_RLoadNow();
}

void SMP_RLoadAdd( int zone, int msec, int depth ) {
	int	now;

	if ( zone < 0 || zone >= SMP_RLZ_COUNT ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	if ( !smp_rloadWindowStart ) {
		smp_rloadWindowStart = SMP_RLoadNow();
	}
	smp_rloadSums[zone] += msec;
	if ( msec > smp_rloadMaxs[zone] ) {
		smp_rloadMaxs[zone] = msec;
	}

	if ( depth != 0 ) {
		return;
	}

	smp_rloadWindowFrames++;
	if ( zone == SMP_RLZ_world ) {
		smp_rloadWorldBrackets++;
	} else if ( zone == SMP_RLZ_reginit ) {
		smp_rloadRegBrackets++;
	}

	now = SMP_RLoadNow();
	if ( msec >= SMP_RLOAD_MIN_MS || ( now - smp_rloadWindowStart ) >= SMP_RLOAD_WINDOW_MS ) {
		SMP_RLoadFlush( now - smp_rloadWindowStart );
	}
}

static cvar_t	*s_beStats = NULL;
static Uint64	s_beFreq = 0;
static Uint64	s_bePerUs = 1;

int BE_Armed( void ) {
	if ( !s_beStats ) {
		s_beStats = ri.Cvar_Get( "be_stats", "1", 0 );
	}
	return ( s_beStats && s_beStats->integer && R_ProbeStatsOn() ) ? 1 : 0;
}

int BE_Microseconds( void ) {
	Uint64	now;

	if ( !s_beFreq ) {
		s_beFreq = SDL_GetPerformanceFrequency();
		if ( !s_beFreq ) {
			s_beFreq = 1000000;
		}
		s_bePerUs = s_beFreq / 1000000;
		if ( !s_bePerUs ) {
			s_bePerUs = 1;
		}
	}

	now = SDL_GetPerformanceCounter();

	if ( s_beFreq >= 1000000 ) {
		return (int)( now / s_bePerUs );
	}
	return (int)( ( now * 1000000ULL ) / s_beFreq );
}
