#ifndef R200_QGL_GLES3_H
#define R200_QGL_GLES3_H

#include "../renderercommon/qgl.h"
#include "qgl_gles3_procs.h"
#include "qgl_gles3_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef R200_PRINTF_FUNC
#	if defined(__GNUC__)
#		define R200_PRINTF_FUNC(f, v) __attribute__((format(printf, f, v)))
#	else
#		define R200_PRINTF_FUNC(f, v)
#	endif
#endif

#define GLE(ret, name, ...) typedef ret APIENTRY name##proc(__VA_ARGS__);
QGL_GLES3_CORE_PROCS;
#undef GLE

#ifdef __cplusplus
#define GLE(ret, name, ...) extern "C" name##proc *qgl##name;
#else
#define GLE(ret, name, ...) extern name##proc *qgl##name;
#endif
QGL_GLES3_CORE_PROCS;
#undef GLE

#ifdef __cplusplus
#define GLE(ret, name, ...) extern "C" name##proc *qgl##name;
#else
#define GLE(ret, name, ...) extern name##proc *qgl##name;
#endif
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
QGL_ES_1_1_PROCS;
QGL_ES_1_1_FIXED_FUNCTION_PROCS;
QGL_1_3_PROCS;
QGL_1_5_PROCS;
QGL_2_0_PROCS;
QGL_3_0_PROCS;
QGL_ARB_occlusion_query_PROCS;
QGL_ARB_framebuffer_object_PROCS;
QGL_ARB_vertex_array_object_PROCS;
QGL_EXT_direct_state_access_PROCS;
#undef GLE


void QGL_GLES3_SetLogger(void (*fn)(const char *line));

typedef void *(*QGL_GLES3_GetProcAddress_t)(const char *name);
void QGL_GLES3_SetGetProcAddress(QGL_GLES3_GetProcAddress_t fn);

int QGL_GLES3_LoadProcs(int *staged);

int QGL_GLES3_TotalProcs(void);
int QGL_GLES3_CoreProcs(void);
int QGL_GLES3_CoreMissing(void);
int QGL_GLES3_StagedTraps(void);

void QGL_GLES3_QueryVersion(void);

const char *QGL_GLES3_CoreMissingList(void);

void QGL_GLES3_LogDeviceInfo(void);

int QGL_GLES3_HasExtension(const char *ext);

int QGL_GLES3_StagedCapDrops(void);

#define R200_GL_MAX_TEXTURE_UNITS_ARB 0x84E2

#ifdef __cplusplus
}
#endif

#endif 
