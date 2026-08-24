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


typedef struct shaderInfo_s {
	char		shader[MAX_QPATH];
	int			surfaceFlags;
	int			contents;
	int			value;

	char		backShader[MAX_QPATH];	
	char		flareShader[MAX_QPATH];	

	float		subdivisions;			
	float		backsplashFraction;		
	float		backsplashDistance;		
	float		lightSubdivide;			
	int			lightmapSampleSize;		

	qboolean	hasPasses;				

	qboolean	globalTexture;			

	qboolean	twoSided;				
	qboolean	autosprite;				
	qboolean	lightFilter;			
	qboolean	forceTraceLight;		
	qboolean	forceVLight;			
	qboolean	patchShadows;			
	qboolean	vertexShadows;			
	qboolean	noVertexShadows;		
	qboolean	forceSunLight;			
	qboolean	notjunc;				
	float		vertexScale;			

	char		editorimage[MAX_QPATH];	
	char		lightimage[MAX_QPATH];	
	vec3_t		color;					
	vec3_t		averageColor;

	int			width, height;
	byte		*pixels;

	vec3_t		sunLight;
	vec3_t		sunDirection;
} shaderInfo_t;

void LoadShaderInfo( void );
shaderInfo_t	*ShaderInfoForShader( const char *shader );

