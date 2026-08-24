#include "gles3_firstlight.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <stdio.h>
#include <string.h>

static void *probe_getproc(const char *name)
{
	return (void *)eglGetProcAddress(name);
}

static void probe_log(const char *line)
{
	fputs(line, stdout);
	fflush(stdout);
}

static EGLDisplay probe_display(void)
{
	EGLDisplay dpy;

	dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (dpy != EGL_NO_DISPLAY && eglInitialize(dpy, NULL, NULL)) {
		return dpy;
	}

	{
		PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
			(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

		if (getPlatformDisplay) {
#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif
			dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
			if (dpy != EGL_NO_DISPLAY && eglInitialize(dpy, NULL, NULL)) {
				return dpy;
			}
		}
	}
	return EGL_NO_DISPLAY;
}

int main(void)
{
	EGLDisplay	dpy;
	EGLConfig	cfg;
	EGLContext	ctx;
	EGLSurface	surf;
	EGLint		numCfg = 0;
	EGLint		cfgAttrs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	EGLint		pbufAttrs[] = { EGL_WIDTH, 256, EGL_HEIGHT, 256, EGL_NONE };
	EGLint		ctxAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	int			missing;
	int			staged = 0;
	unsigned	passed;

	printf("[R200:firstlight] phase=probe start version=%s\n", "EGL/GLES3");

	dpy = probe_display();
	if (dpy == EGL_NO_DISPLAY) {
		printf("[R200:firstlight] phase=probe status=fail stage=eglGetDisplay/eglInitialize\n");
		return 2;
	}
	eglQueryString(dpy, EGL_VERSION);
	printf("[R200:firstlight] phase=probe egl_version=\"%s\" vendor=\"%s\"\n",
		   eglQueryString(dpy, EGL_VERSION) ? eglQueryString(dpy, EGL_VERSION) : "(null)",
		   eglQueryString(dpy, EGL_VENDOR) ? eglQueryString(dpy, EGL_VENDOR) : "(null)");

	if (!eglChooseConfig(dpy, cfgAttrs, &cfg, 1, &numCfg) || numCfg < 1) {
		cfgAttrs[10] = EGL_NONE; 
		if (!eglChooseConfig(dpy, cfgAttrs, &cfg, 1, &numCfg) || numCfg < 1) {
			printf("[R200:firstlight] phase=probe status=fail stage=eglChooseConfig\n");
			return 2;
		}
	}

	ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttrs);
	if (ctx == EGL_NO_CONTEXT) {
		ctxAttrs[1] = 2;
		ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttrs);
		printf("[R200:firstlight] phase=probe note=es3-context-refused trying=es2\n");
	}
	if (ctx == EGL_NO_CONTEXT) {
		printf("[R200:firstlight] phase=probe status=fail stage=eglCreateContext\n");
		return 2;
	}

	surf = eglCreatePbufferSurface(dpy, cfg, pbufAttrs);
	if (surf == EGL_NO_SURFACE) {
		printf("[R200:firstlight] phase=probe status=fail stage=eglCreatePbufferSurface\n");
		return 2;
	}
	if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
		printf("[R200:firstlight] phase=probe status=fail stage=eglMakeCurrent\n");
		return 2;
	}

	QGL_GLES3_SetGetProcAddress(probe_getproc);
	QGL_GLES3_SetLogger(probe_log);
	R200_FirstLight_SetLogger(probe_log);

	missing = QGL_GLES3_LoadProcs(&staged);
	QGL_GLES3_LogDeviceInfo();
	printf("[R200:firstlight] phase=probe procs=%d core=%d core_missing=%d staged_desktop_traps=%d\n",
		   QGL_GLES3_TotalProcs(), QGL_GLES3_CoreProcs(), missing, staged);
	if (missing > 0) {
		printf("[R200:firstlight] phase=probe core_list=%s\n", QGL_GLES3_CoreMissingList());
	}

	passed = R200_FirstLight_ClearAndQuad();
	printf("[R200:firstlight] phase=probe clear=%d quad=%d bsp_face=engine-only\n",
		   (passed & R200_FL_CLEAR) ? 1 : 0, (passed & R200_FL_QUAD) ? 1 : 0);

	R200_FirstLight_Shutdown();
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(dpy, surf);
	eglDestroyContext(dpy, ctx);
	eglTerminate(dpy);

	if (missing > 0) {
		printf("[R200:firstlight] phase=probe status=fail reason=core-missing\n");
		return 3;
	}
	if (passed != (R200_FL_CLEAR | R200_FL_QUAD)) {
		printf("[R200:firstlight] phase=probe status=fail reason=milestone\n");
		return 4;
	}
	printf("[R200:firstlight] phase=probe status=pass\n");
	return 0;
}
