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
// SPDX-License-Identifier: GPL-2.0-or-later
#include "tr_local.h"
#include "tr_vbo.h"
#include "tr_program.h"

#define SKY_SUBDIVISIONS		8
#define HALF_SKY_SUBDIVISIONS	(SKY_SUBDIVISIONS/2)

static float s_cloudTexCoords[6][SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1][2];


static vec3_t sky_clip[6] = 
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

static float	sky_mins[2][6], sky_maxs[2][6];
static float	sky_min, sky_max;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

static void AddSkyPolygon (int nump, vec3_t vecs) 
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;
	static int	vec_to_st[6][3] =
	{
		{-2,3,1},
		{2,3,-1},

		{1,3,2},
		{-1,3,-2},

		{-2,-1,3},
		{-2,1,-3}

	};

	VectorCopy (vec3_origin, v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001)
			continue;	
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < sky_mins[0][axis])
			sky_mins[0][axis] = s;
		if (t < sky_mins[1][axis])
			sky_mins[1][axis] = t;
		if (s > sky_maxs[0][axis])
			sky_maxs[0][axis] = s;
		if (t > sky_maxs[1][axis])
			sky_maxs[1][axis] = t;
	}
}

#define	ON_EPSILON		0.1f			
#define	MAX_CLIP_VERTS	64
static void ClipSkyPolygon (int nump, vec3_t vecs, int stage) 
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		ri.Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{	
		AddSkyPolygon (nump, vecs);
		return;
	}

	front = back = qfalse;
	norm = sky_clip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = qtrue;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = qtrue;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static void ClearSkyBox (void) {
	int		i;

	for (i=0 ; i<6 ; i++) {
		sky_mins[0][i] = sky_mins[1][i] = 9999;
		sky_maxs[0][i] = sky_maxs[1][i] = -9999;
	}
}

void RB_ClipSkyPolygons( shaderCommands_t *input )
{
	vec3_t		p[5];	
	int			i, j;

	ClearSkyBox();

	for ( i = 0; i < input->numIndexes; i += 3 )
	{
		for (j = 0 ; j < 3 ; j++) 
		{
			VectorSubtract( input->xyz[input->indexes[i+j]],
							backEnd.viewParms.ori.origin, 
							p[j] );
		}
		ClipSkyPolygon( 3, p[0], 0 );
	}
}


static void MakeSkyVec( float s, float t, int axis, float outSt[2], vec3_t outXYZ )
{
	static int	st_to_vec[6][3] =
	{
		{3,-1,2},
		{-3,1,2},

		{1,3,2},
		{-1,-3,2},

		{-2,-1,3},		
		{2,-1,-3}		
	};

	vec3_t		b;
	int			j, k;
	float	boxSize;

	boxSize = backEnd.viewParms.zFar / 1.75;		
	b[0] = s*boxSize;
	b[1] = t*boxSize;
	b[2] = boxSize;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
		{
			outXYZ[j] = -b[-k - 1];
		}
		else
		{
			outXYZ[j] = b[k - 1];
		}
	}

	s = (s+1)*0.5;
	t = (t+1)*0.5;
	if (s < sky_min)
	{
		s = sky_min;
	}
	else if (s > sky_max)
	{
		s = sky_max;
	}

	if (t < sky_min)
	{
		t = sky_min;
	}
	else if (t > sky_max)
	{
		t = sky_max;
	}

	t = 1.0 - t;


	if ( outSt )
	{
		outSt[0] = s;
		outSt[1] = t;
	}
}

static int	sky_texorder[6] = {0,2,1,3,4,5};
static vec3_t	s_skyPoints[SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1];
static float	s_skyTexCoords[SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1][2];

static void DrawSkySide( struct image_s *image, const int mins[2], const int maxs[2] )
{
	int s, t;

	RB_ImmFlush();
	GL_BindUnit0( image );

	{
		gles3Program_t	*pSky = GLES3_BeginFamily( GLP_FAMILY_SKY, 1 );

		GLES3_SetBaseColor4f( pSky, 1.0f, 1.0f, 1.0f, 1.0f );
		GLES3_SetUniform1f( pSky, GLES3_U_SKYALPHA, tr.refdef.sky_alpha );
		GLES3_SetUniform1i( pSky, GLES3_U_SKYALPHAMODE, 0 );
		GLES3_SetUniform2f( pSky, GLES3_U_SKYSCROLL, 0.0f, 0.0f );
	}
	/* The stream colour outlives the run that set it; a sky side that inherits
	   whatever box or line ran last tints the whole dome. */
	RB_ImmColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	for ( t = mins[1]+HALF_SKY_SUBDIVISIONS; t < maxs[1]+HALF_SKY_SUBDIVISIONS; t++ )
	{
		RB_ImmBegin( GL_TRIANGLE_STRIP );

		for ( s = mins[0]+HALF_SKY_SUBDIVISIONS; s <= maxs[0]+HALF_SKY_SUBDIVISIONS; s++ )
		{
			RB_ImmTexCoord2fv( s_skyTexCoords[t][s] );
			RB_ImmVertex3fv( s_skyPoints[t][s] );

			RB_ImmTexCoord2fv( s_skyTexCoords[t+1][s] );
			RB_ImmVertex3fv( s_skyPoints[t+1][s] );
		}

		RB_ImmEnd();
	}

	RB_ImmFlush();
}

static void DrawSkyBox( shader_t *shader )
{
	int		i;

	// Retail DrawSkyBox @0x00043880 does NOT touch sky_min/sky_max: a whole-corpus grep
	Com_Memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );

	for (i=0 ; i<6 ; i++)
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) )
		{
			continue;
		}

		sky_mins_subd[0] = sky_mins[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_mins_subd[1] = sky_mins[1][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[0] = sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[1] = sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS;

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS )
			sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		for ( t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							i, 
							s_skyTexCoords[t][s], 
							s_skyPoints[t][s] );
			}
		}

		DrawSkySide( shader->sky.outerbox[sky_texorder[i]],
			         sky_mins_subd,
					 sky_maxs_subd );
	}

}

static void FillCloudySkySide( const int mins[2], const int maxs[2], qboolean addIndexes )
{
	int s, t;
	int vertexStart = tess.numVertexes;
	int tHeight, sWidth;

	tHeight = maxs[1] - mins[1] + 1;
	sWidth = maxs[0] - mins[0] + 1;

	for ( t = mins[1]+HALF_SKY_SUBDIVISIONS; t <= maxs[1]+HALF_SKY_SUBDIVISIONS; t++ )
	{
		for ( s = mins[0]+HALF_SKY_SUBDIVISIONS; s <= maxs[0]+HALF_SKY_SUBDIVISIONS; s++ )
		{
			VectorAdd( s_skyPoints[t][s], backEnd.viewParms.ori.origin, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = s_skyTexCoords[t][s][0];
			tess.texCoords[tess.numVertexes][0][1] = s_skyTexCoords[t][s][1];

			/* Same class as the font glyph quad: these vertices bypass
			   addTriangle, so the dome drew in the last colour in the array. */
			tess.vertexColors[tess.numVertexes][0] = 255;
			tess.vertexColors[tess.numVertexes][1] = 255;
			tess.vertexColors[tess.numVertexes][2] = 255;
			tess.vertexColors[tess.numVertexes][3] = 255;

			tess.numVertexes++;

			if ( tess.numVertexes >= SHADER_MAX_VERTEXES )
			{
				ri.Error( ERR_DROP, "SHADER_MAX_VERTEXES hit in FillCloudySkySide()\n" );
			}
		}
	}

	tess.vertexColorValid = qtrue;

	if ( addIndexes ) {
		for ( t = 0; t < tHeight-1; t++ )
		{
			for ( s = 0; s < sWidth-1; s++ )
			{
				tess.indexes[tess.numIndexes] = vertexStart + s + t * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
				tess.numIndexes++;

				tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
				tess.numIndexes++;
			}
		}
	}
}

static void FillCloudBox( const shader_t *shader, int stage )
{
	int i;

	for ( i =0; i < 6; i++ )
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;
		float MIN_T;

		if ( 1 ) 
		{
			MIN_T = -HALF_SKY_SUBDIVISIONS;

			if ( i == 5 )
				continue;
		}
		else
		{
			switch( i )
			{
			case 0:
			case 1:
			case 2:
			case 3:
				MIN_T = -1;
				break;
			case 5:
				continue;
			case 4:		
			default:
				MIN_T = -HALF_SKY_SUBDIVISIONS;
				break;
			}
		}

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) )
		{
			continue;
		}

		sky_mins_subd[0] = myftol( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS );
		sky_mins_subd[1] = myftol( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS );
		sky_maxs_subd[0] = myftol( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS );
		sky_maxs_subd[1] = myftol( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS );

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_mins_subd[1] < MIN_T )
			sky_mins_subd[1] = MIN_T;
		else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_maxs_subd[1] < MIN_T )
			sky_maxs_subd[1] = MIN_T;
		else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		for ( t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							i, 
							NULL,
							s_skyPoints[t][s] );

				s_skyTexCoords[t][s][0] = s_cloudTexCoords[i][t][s][0];
				s_skyTexCoords[t][s][1] = s_cloudTexCoords[i][t][s][1];
			}
		}

		FillCloudySkySide( sky_mins_subd, sky_maxs_subd, ( stage == 0 ) );
	}
}

void R_BuildCloudData( shaderCommands_t *input )
{
	int			i;
	shader_t	*shader;

	shader = input->shader;
	(void)shader;          

	assert( shader->isSky );

	sky_min = 1.0 / 256.0f;		
	sky_max = 255.0 / 256.0f;

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	if ( input->shader->sky.cloudHeight )
	{
		for ( i = 0; i < MAX_SHADER_STAGES; i++ )
		{
			if ( !tess.xstages[i] ) {
				break;
			}
			FillCloudBox( input->shader, i );
		}
	}
}

#define SQR( a ) ((a)*(a))
void R_InitSkyTexCoords( float heightCloud )
{
	int i, s, t;
	float radiusWorld = 4096;
	float p;
	float sRad, tRad;
	vec3_t skyVec;
	vec3_t v;

	backEnd.viewParms.zFar = 1024;

	for ( i = 0; i < 6; i++ )
	{
		for ( t = 0; t <= SKY_SUBDIVISIONS; t++ )
		{
			for ( s = 0; s <= SKY_SUBDIVISIONS; s++ )
			{
				// matching retail 0x0436C8-0x043704) and FillCloudBox use.  Do NOT
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							i, 
							NULL,
							skyVec );

				p = ( 1.0f / ( 2 * DotProduct( skyVec, skyVec ) ) ) *
					( -2 * skyVec[2] * radiusWorld + 
					   2 * sqrt( SQR( skyVec[2] ) * SQR( radiusWorld ) + 
					             2 * SQR( skyVec[0] ) * radiusWorld * heightCloud +
								 SQR( skyVec[0] ) * SQR( heightCloud ) + 
								 2 * SQR( skyVec[1] ) * radiusWorld * heightCloud +
								 SQR( skyVec[1] ) * SQR( heightCloud ) + 
								 2 * SQR( skyVec[2] ) * radiusWorld * heightCloud +
								 SQR( skyVec[2] ) * SQR( heightCloud ) ) );

				VectorScale( skyVec, p, v );
				v[2] += radiusWorld;

				VectorNormalize( v );

				sRad = Q_acos( v[0] );
				tRad = Q_acos( v[1] );

				s_cloudTexCoords[i][t][s][0] = sRad;
				s_cloudTexCoords[i][t][s][1] = tRad;
			}
		}
	}
}






void RB_StageIteratorSky( void ) {
	if ( r_fastsky->integer ) {
		return;
	}

	RB_ClipSkyPolygons( &tess );

	if ( r_showsky->integer ) {
		qglDepthRange( 0.0, 0.0 );
	} else {
		qglDepthRange( 1.0, 1.0 );
	}

	if ( tess.shader->sky.outerbox[0] && tess.shader->sky.outerbox[0] != tr.defaultImage ) {
		RB_ImmFlush();
		RB_ImmColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

		RB_PushModelView ();
		GL_State( 0 );
		RB_TranslateModelView (backEnd.viewParms.ori.origin[0], backEnd.viewParms.ori.origin[1], backEnd.viewParms.ori.origin[2]);

		DrawSkyBox( tess.shader );

		RB_ImmFlush();
		RB_PopModelView ();
	}

	R_BuildCloudData( &tess );

	RB_StageIteratorGeneric();



	qglDepthRange( 0.0, 1.0 );

	backEnd.skyRenderedThisView = qtrue;
}

