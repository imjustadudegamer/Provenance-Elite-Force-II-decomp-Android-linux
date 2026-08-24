#ifndef TR_VBO_H
#define TR_VBO_H

#include "tr_local.h"

enum {
	ATTR_POSITION  = 0,
	ATTR_NORMAL    = 1,
	ATTR_COLOR     = 2,
	ATTR_TEXCOORD0 = 3,
	ATTR_TEXCOORD1 = 4
};

#ifdef __cplusplus
extern "C" {
#endif

void RB_VBO_Init( void );
void RB_VBO_Shutdown( void );

void RB_VBO_DrawTessIndexed( const shaderCommands_t *input, const glIndex_t *indexes, int numIndexes );
void RB_VBO_DrawTessArrays( const shaderCommands_t *input,
							const glIndex_t *indexes, int numIndexes,
							const float *texcoord0, const float *texcoord1, int tcStrideFloats,
							const unsigned char *colors,
							const float *normals );
void RB_VBO_DrawTess( const shaderCommands_t *input );

void RB_VBO_DecalAttribProbe( int *enabled, int *size, int *stride, int *type,
							  int *normalized, const void **pointer );

#include "tr_vbo_imm.h"

const float *RB_GetModelViewMatrix( void );
const float *RB_GetProjectionMatrix( void );

extern GLuint r_vaoBound;
extern int r_spriteListOpen;
void RB_BindVAO( GLuint vao );

#ifdef __cplusplus
}
#endif

#endif 
