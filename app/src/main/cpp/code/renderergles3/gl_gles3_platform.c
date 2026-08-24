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

int      r201_texTranscode = R201_TEXC_NONE;
qboolean r201_hasAstcLdr = qfalse;
qboolean r201_hasAstcSliced3d = qfalse;
qboolean r201_hasEtc2Core = qfalse;

static cvar_t *r_allowSoftwareGL;
static cvar_t *r_allowResize;
static cvar_t *r_centerWindow;
static cvar_t *r_sdlDriver;
static cvar_t *r_preferOpenGLES;

cvar_t *r200_gles3_allow_missing_core;	
cvar_t *r200_gles3_require_position;	

typedef enum {
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_UNKNOWN
} rserr_t;

#define R_MODE_FALLBACK 3  

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

static void GLES3_LogToEngine(const char *line)
{
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
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
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

	ri.Printf(PRINT_ALL, "[R200:gles3] context attempt %s (%dx%d depth=%d stencil=%d)\n",
			  what, width, height, depthBits, stencilBits);

	SDL_window = SDL_CreateWindow(CLIENT_WINDOW_TITLE, x, y, width, height, flags);
	if (!SDL_window) {
		ri.Printf(PRINT_ALL, "[R200:gles3] SDL_CreateWindow failed: %s\n", SDL_GetError());
		return qfalse;
	}

	sdl_gl_context = SDL_GL_CreateContext(SDL_window);
	if (!sdl_gl_context) {
		ri.Printf(PRINT_ALL, "[R200:gles3] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		GLES3_FreeContext();
		return qfalse;
	}

	QGL_GLES3_SetGetProcAddress(GLES3_ResolveProc);
	QGL_GLES3_SetLogger(GLES3_LogToEngine);
	{
		int staged = 0;
		int missingCore = QGL_GLES3_LoadProcs(&staged);

		QGL_GLES3_LogDeviceInfo();
		ri.Printf(PRINT_ALL, "[R200:gles3] procs=%d core=%d core_missing=%d staged_desktop_traps=%d\n",
				  QGL_GLES3_TotalProcs(), QGL_GLES3_CoreProcs(), missingCore, staged);
		if (missingCore > 0) {
			ri.Printf(PRINT_ALL, "[R200:glmissing] core_list=%s\n",
					  QGL_GLES3_CoreMissingList());
			if (!r200_gles3_allow_missing_core->integer) {
				ri.Error(ERR_FATAL,
						 "GLES3: %d required GLES 3.0 core entry point(s) missing: %s "
						 "(set r200_gles3_allow_missing_core 1 to continue for triage)",
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

	SDL_GL_SetSwapInterval(r_swapInterval->integer);

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

	r201_hasEtc2Core = coreEtc2;
	r201_hasAstcLdr = astc;
	r201_hasAstcSliced3d = astcSliced3d;
	r201_texTranscode = R201_SelectTexTranscode(
		qglesMajorVersion, qglesMinorVersion,
		s3tc ? 1 : 0, etc2 ? 1 : 0,
		QGL_GLES3_HasExtension("GL_ARB_ES3_compatibility") ? 1 : 0,
		astc ? 1 : 0, r_ext_compress_textures->value ? 1 : 0);

	ri.Printf(PRINT_ALL,
			  "[R200:gltex] s3tc=%d etc1=%d etc2=%d etc2_core=%d astc_ldr=%d astc_sliced_3d=%d "
			  "-> compressed_path=%s transcode=%s software_decode=%s\n",
			  s3tc ? 1 : 0, etc1 ? 1 : 0, etc2 ? 1 : 0, coreEtc2 ? 1 : 0,
			  astc ? 1 : 0, astcSliced3d ? 1 : 0,
			  (glConfig.textureCompression != TC_NONE) ? "glCompressedTexImage2D" : "none",
			  (r201_texTranscode == R201_TEXC_ASTC_4x4) ? "glCompressedTexImage2D/ASTC_4x4"
			  : (r201_texTranscode == R201_TEXC_ETC2_RGBA8) ? "glCompressedTexImage2D/ETC2_RGBA8"
															: "none",
			  (glConfig.textureCompression == TC_NONE &&
			   r201_texTranscode == R201_TEXC_NONE) ? "yes" : "no");

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

	ri.Printf(PRINT_ALL, "[R200:glcaps] env_add=%d env_combine=%d cube_map=1\n",
			  glConfig.textureEnvAddAvailable ? 1 : 0,
			  glConfig.textureEnvCombineAvailable ? 1 : 0);
}

void GLimp_Init(qboolean fixedFunction)
{
	if (fixedFunction) {
		ri.Printf(PRINT_ALL, "[R200:gles3] GLimp_Init(fixedFunction=1) -- GLES 3.0 "
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
	r200_gles3_allow_missing_core =
		ri.Cvar_Get("r200_gles3_allow_missing_core", "0", CVAR_TEMP);

	r_ati_fsaa_samples = ri.Cvar_Get("r_ati_fsaa_samples", "0", CVAR_ARCHIVE);
	r_nv_fsaa_samples = ri.Cvar_Get("r_nv_fsaa_samples", "4", CVAR_ARCHIVE | CVAR_LATCH);

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

	ri.Error(ERR_FATAL, "GLimp_Init() - could not create a GLES 3.0 subsystem");

success:
	GLES3_InitExtensions();
	ri.Cvar_Get("r_availableModes", "", CVAR_ROM);
	ri.IN_Init(SDL_window);
}

void GLimp_Shutdown(void)
{
	R200_FirstLight_Shutdown();
	QGL_GLES3_SetLogger(NULL);
	R200_FirstLight_SetLogger(NULL);
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

#define R201_CTX_PROBE_CAP 8

static void GLES3_ContextAttest(void)
{
	static int lastMinimized = -1;
	static int rows = 0;
	int minimized;
	GLint prog = 0;
	GLenum err;

	minimized = ri.Cvar_VariableIntegerValue("com_minimized");

	if (lastMinimized < 0 || (lastMinimized && !minimized)) {
		if (rows < R201_CTX_PROBE_CAP + 1) {
			qglGetIntegerv(GL_CURRENT_PROGRAM, &prog);
			err = qglGetError();
			rows++;
			ri.Printf(PRINT_ALL,
					  "[R201:ctx] row=%d edge=%s minimized=%d white=%d scratch=%d dlight=%d "
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
		Uint32 r201SwapStart = SDL_GetTicks();

		SDL_GL_SwapWindow(SDL_window);

		ri.NoteBlockedMsec( (int)( SDL_GetTicks() - r201SwapStart ) );
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

#define R201_RLOAD_WINDOW_MS	1000
#define R201_RLOAD_MIN_MS		30	
#define R201_RLOAD_MAX_ROWS		720

cvar_t	*r201_rloadStats = NULL;
int		r201_rloadDepth = 0;

static int	r201_rloadRows;
static int	r201_rloadStopped;
static int	r201_rloadWindowStart;
static unsigned int	r201_rloadWindowFrames;
static int	r201_rloadSums[R201_RLZ_COUNT], r201_rloadMaxs[R201_RLZ_COUNT];
static int	r201_rloadWorldBrackets;	
static int	r201_rloadRegBrackets;

int R201_RLoadNow( void ) {
	if ( !r201_rloadStats ) {
		r201_rloadStats = ri.Cvar_Get( "r201_loadStats", "1", 0 );
	}
	return (int)SDL_GetTicks();
}

static void R201_RLoadFlush( int window ) {
	static const char *zoneName[R201_RLZ_COUNT] = {
		"world", "bspread", "shaders", "lightmap", "lods",
		"reginit", "imginit", "setup", "findimg", "decode",
		"transcode", "upload", "modelreg"
	};
	char	line[1024], part[64];
	int		i, mean10, frames, unacctW;

	if ( r201_rloadStopped ) {
		return;
	}

	unacctW = r201_rloadSums[R201_RLZ_world]
			- r201_rloadSums[R201_RLZ_bspread]
			- r201_rloadSums[R201_RLZ_shaders]
			- r201_rloadSums[R201_RLZ_lightmap]
			- r201_rloadSums[R201_RLZ_lods];
	if ( unacctW < 0 ) unacctW = 0;

	frames = (int)r201_rloadWindowFrames;
	if ( frames < 1 ) frames = 1;

	line[0] = '\0';
	for ( i = 0; i < R201_RLZ_COUNT; i++ ) {
		mean10 = (int)( ( (long long)r201_rloadSums[i] * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zoneName[i], mean10 / 10, mean10 % 10, r201_rloadMaxs[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	Com_sprintf( part, sizeof( part ), " unacctWsum=%d", unacctW );
	Q_strcat( line, sizeof( line ), part );

	ri.Printf( PRINT_ALL, "[R201:rload] window=%dms worldN=%d regN=%d%s\n",
			   window, r201_rloadWorldBrackets, r201_rloadRegBrackets, line );

	r201_rloadRows++;
	if ( r201_rloadRows >= R201_RLOAD_MAX_ROWS ) {
		r201_rloadStopped = 1;
		ri.Printf( PRINT_ALL, "[R201:rload] capped at %d rows; no further [R201:rload] lines\n",
				   R201_RLOAD_MAX_ROWS );
	}

	for ( i = 0; i < R201_RLZ_COUNT; i++ ) {
		r201_rloadSums[i] = r201_rloadMaxs[i] = 0;
	}
	r201_rloadWindowFrames = 0;
	r201_rloadWorldBrackets = 0;
	r201_rloadRegBrackets = 0;
	r201_rloadWindowStart = R201_RLoadNow();
}

void R201_RLoadAdd( int zone, int msec, int depth ) {
	int	now;

	if ( zone < 0 || zone >= R201_RLZ_COUNT ) {
		return;
	}
	if ( msec < 0 ) {
		msec = 0;
	}

	if ( !r201_rloadWindowStart ) {
		r201_rloadWindowStart = R201_RLoadNow();
	}
	r201_rloadSums[zone] += msec;
	if ( msec > r201_rloadMaxs[zone] ) {
		r201_rloadMaxs[zone] = msec;
	}

	if ( depth != 0 ) {
		return;
	}

	r201_rloadWindowFrames++;
	if ( zone == R201_RLZ_world ) {
		r201_rloadWorldBrackets++;
	} else if ( zone == R201_RLZ_reginit ) {
		r201_rloadRegBrackets++;
	}

	now = R201_RLoadNow();
	if ( msec >= R201_RLOAD_MIN_MS || ( now - r201_rloadWindowStart ) >= R201_RLOAD_WINDOW_MS ) {
		R201_RLoadFlush( now - r201_rloadWindowStart );
	}
}

static cvar_t	*s_r204Stats = NULL;
static Uint64	s_r204Freq = 0;
static Uint64	s_r204PerUs = 1;

int R204_Armed( void ) {
	if ( !s_r204Stats ) {
		s_r204Stats = ri.Cvar_Get( "r204_stats", "1", 0 );
	}
	return ( s_r204Stats && s_r204Stats->integer ) ? 1 : 0;
}

int R204_Microseconds( void ) {
	Uint64	now;

	if ( !s_r204Freq ) {
		s_r204Freq = SDL_GetPerformanceFrequency();
		if ( !s_r204Freq ) {
			s_r204Freq = 1000000;
		}
		s_r204PerUs = s_r204Freq / 1000000;
		if ( !s_r204PerUs ) {
			s_r204PerUs = 1;
		}
	}

	now = SDL_GetPerformanceCounter();

	if ( s_r204Freq >= 1000000 ) {
		return (int)( now / s_r204PerUs );
	}
	return (int)( ( now * 1000000ULL ) / s_r204Freq );
}
