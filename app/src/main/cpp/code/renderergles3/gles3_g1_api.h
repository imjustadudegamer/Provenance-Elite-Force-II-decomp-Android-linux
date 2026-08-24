#ifndef R200_GLES3_G1_API_H
#define R200_GLES3_G1_API_H

#include "tr_program.h"

#ifdef __cplusplus
extern "C" {
#endif

void	RB_ClipPlane_Disable( void );
void	RB_ClipPlane_Set( const vec4_t plane );
qboolean RB_ClipPlane_Get( vec4_t out );

void	RB_LoadProjectionMatrix( const float *matrix );
void	RB_LoadModelViewMatrix( const float *matrix );
void	RB_LoadIdentityModelView( void );
void	RB_LoadIdentityProjection( void );
void	RB_PushModelView( void );
void	RB_PopModelView( void );
void	RB_PushProjection( void );
void	RB_PopProjection( void );
void	RB_TranslateModelView( float x, float y, float z );
void	RB_OrthoProjection( float left, float right, float bottom, float top, float zNear, float zFar );

void	RB_ImmBegin( GLenum mode );
void	RB_ImmVertex2f( float x, float y );
void	RB_ImmVertex3f( float x, float y, float z );
void	RB_ImmVertex3fv( const float *v );
void	RB_ImmColor3f( float r, float g, float b );
void	RB_ImmColor4f( float r, float g, float b, float a );
void	RB_ImmTexCoord2f( float s, float t );
void	RB_ImmTexCoord2fv( const float *st );
void	RB_ImmEnd( void );
void	RB_ImmFlush( void );

#ifdef __cplusplus
}
#endif

#endif 
