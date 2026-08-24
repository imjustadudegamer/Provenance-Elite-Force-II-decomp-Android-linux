#ifndef R200_GLES3_FIRSTLIGHT_H
#define R200_GLES3_FIRSTLIGHT_H

#include "qgl_gles3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define R200_FL_CLEAR 0x1
#define R200_FL_QUAD  0x2
#define R200_FL_FACE  0x4

void R200_FirstLight_SetLogger(void (*fn)(const char *line));

unsigned R200_FirstLight_ClearAndQuad(void);

unsigned R200_FirstLight_DrawFace(const float *xyz, const float *st, int numVerts,
								  unsigned tex);

unsigned R200_FirstLight_UploadRGBA8(int w, int h, const unsigned char *pixels);
void R200_FirstLight_DeleteTexture(unsigned tex);

void R200_FirstLight_Shutdown(void);

int R200_FirstLight_Ready(void);

#ifdef __cplusplus
}
#endif

#endif 
