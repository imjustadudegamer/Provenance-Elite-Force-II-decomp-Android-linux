#ifndef GLES3_GLES3_FIRSTLIGHT_H
#define GLES3_GLES3_FIRSTLIGHT_H

#include "qgl_gles3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLES3_FL_CLEAR 0x1
#define GLES3_FL_QUAD  0x2
#define GLES3_FL_FACE  0x4

void GLES3_FirstLight_SetLogger(void (*fn)(const char *line));

unsigned GLES3_FirstLight_ClearAndQuad(void);

unsigned GLES3_FirstLight_DrawFace(const float *xyz, const float *st, int numVerts,
								  unsigned tex);

unsigned GLES3_FirstLight_UploadRGBA8(int w, int h, const unsigned char *pixels);
void GLES3_FirstLight_DeleteTexture(unsigned tex);

void GLES3_FirstLight_Shutdown(void);

int GLES3_FirstLight_Ready(void);

#ifdef __cplusplus
}
#endif

#endif 
