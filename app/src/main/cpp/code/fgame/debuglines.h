

#pragma once

#include "g_local.h"

void G_InitDebugLines( void );
void G_DebugLine( Vector start, Vector end, float r, float g, float b, float alpha );
void G_LineStipple( int factor, unsigned short pattern );
void G_LineWidth( float width );
void G_Color3f( float r, float g, float b	);
void G_Color3v( Vector color );
void G_Color4f( float r, float g,	float b,	float alpha	);
void G_Color3vf( Vector color, float alpha );
void G_BeginLine( void );
void G_Vertex( Vector v );
void G_EndLine( void );
void G_DebugBBox( Vector org, Vector mins, Vector maxs, float r, float g, float b, float alpha );
void G_DrawDebugNumber( Vector org, float number, float scale, float r, float g, float b, int precision = 0 );
void G_DebugCircle( const Vector &org, float radius, float r, float g, float b, float alpha, qboolean horizontal = qfalse );
void G_DebugOrientedCircle( Vector org, float radius, float r, float g, float b, float alpha, Vector angles );
void G_DebugPyramid( Vector org, float radius, float r, float g, float b, float alpha );
void G_DrawCoordSystem( Vector pos, Vector f, Vector r, Vector u, int len );
void G_DebugArrow( Vector org, Vector dir, float  length, float r, float g, float b, float alpha );
void G_DrawCSystem( void );
void G_DebugLineC( const vec3_t start, const vec3_t end, float r, float g, float b, float alpha );
void G_AllocDebugLines( void );
void G_DeAllocDebugLines( void );

typedef enum
   {
   north,
   south,
   east,
   west,
   up,
   down
   } facet_t;

void G_DebugHighlightFacet( Vector org, Vector mins, Vector maxs, facet_t facet, float r, float g, float b, float alpha );

extern debugline_t *DebugLines;
