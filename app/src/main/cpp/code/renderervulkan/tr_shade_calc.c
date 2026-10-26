/*
===========================================================================
This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
engine descends from id Tech 3 through OpenMoHAA.

It is free software; you can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License, COPYING.txt at the repository
root, for more details.
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// Modified 2026-10-04 for this port; the changes are listed in PROVENANCE.tsv.
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


#define	WAVEVALUE( table, base, amplitude, phase, freq )  ((base) + table[ myftol( ( ( (phase) + tess.shaderTime * (freq) ) * FUNCTABLE_SIZE ) ) & FUNCTABLE_MASK ] * (amplitude))

static float *TableForFunc( genFunc_t func ) 
{
	switch ( func )
	{
	case GF_SIN:
		return tr.sinTable;
	case GF_TRIANGLE:
		return tr.triangleTable;
	case GF_SQUARE:
		return tr.squareTable;
	case GF_SAWTOOTH:
		return tr.sawToothTable;
	case GF_INVERSE_SAWTOOTH:
		return tr.inverseSawToothTable;
	case GF_NONE:
	default:
		break;
	}

	ri.Error( ERR_DROP, "TableForFunc called with invalid function '%d' in shader '%s'\n", func, tess.shader->name );
	return NULL;
}

float EvalWaveForm( const waveForm_t *wf ) 
{
	float	*table;
	float	base;
	float	amplitude;
	float	phase;
	float	frequency;

	table = TableForFunc( wf->func );

    if (wf->base != 1234567.0f) {
        base = wf->base;
    } else if (backEnd.currentEntity) {
        base = (float)backEnd.currentEntity->e.surfaces[0] / 16.0 - 8.0;
    } else {
        base = 0.0f  ;
    }

    if (wf->amplitude != 1234567.0f) {
		amplitude = wf->amplitude;
    } else if (backEnd.currentEntity) {
		amplitude = (float)backEnd.currentEntity->e.surfaces[1] / 16.0;
    } else {
		amplitude = 0.0f  ;
    }

    if (wf->phase != 1234567.0f) {
		phase = wf->phase;
    } else if (backEnd.currentEntity) {
		phase = (float)backEnd.currentEntity->e.surfaces[2] / 16.0 - 8.0;
    } else {
		phase = 0.0f  ;
    }

    if (wf->frequency != 1234567.0f) {
		frequency = wf->frequency;
    } else if (backEnd.currentEntity) {
		frequency = (float)backEnd.currentEntity->e.surfaces[3] / 16.0;
    } else {
		frequency = 0.0f  ;
    }

	if (!backEnd.currentEntity)
	{
		base *= 1.0f  ;
		amplitude *= 1.0f  ;
		phase *= 1.0f  ;
		frequency *= 1.0f  ;
	}

	return WAVEVALUE( table, base, amplitude, phase, frequency );
}

static float EvalWaveFormClamped( const waveForm_t *wf )
{
	float glow  = EvalWaveForm( wf );

	if ( glow < 0 )
	{
		return 0;
	}

	if ( glow > 1 )
	{
		return 1;
	}

	return glow;
}

void RB_CalcStretchTexCoords( const waveForm_t *wf, float *st )
{
	float p;
	texModInfo_t tmi;

	p = 1.0f / EvalWaveForm( wf );

	tmi.matrix[0][0] = p;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_CalcTransformTexCoords( &tmi, st );
}

void RB_CalcTransWaveTexCoords(const waveForm_t* wf, float* st)
{
	float p;
	int i;

    p = EvalWaveForm(wf);
    for (i = 0; i < tess.numVertexes; i++, st += 2) {
        st[0] += p;
    }
}

void RB_CalcTransWaveTexCoordsT(const waveForm_t* wf, float* st)
{
    float p;
    int i;

    p = EvalWaveForm(wf);
    for (i = 0; i < tess.numVertexes; i++, st += 2) {
        st[1] += p;
    }
}

void RB_CalcBulgeTexCoords(const waveForm_t* wf, float* st)
{
	int i;
	int off;
	float offset;
	float now;
	float amplitude;
	float width;

    amplitude = wf->amplitude;
    width = wf->base;
    now = backEnd.refdef.time / 1000.0 * wf->frequency + wf->phase;

	for (i = 0; i < tess.numVertexes; i++, st += 2) {
		off = (now + st[0] * width) * FUNCTABLE_SIZE;
		offset = tr.sinTable[off & FUNCTABLE_MASK] * amplitude;
		(void)offset;     
	}
}


void RB_CalcDeformVertexes( deformStage_t *ds )
{
	int		i;
	vec3_t	offset;
	float	scale;
	float	*xyz = ( float * ) tess.xyz;
	float	*normal = ( float * ) tess.normal;
	float	*table;
	float	base;
	float	amplitude;
	float	phase;
	float	frequency;
	float	minHeight;
	float	maxHeight;
	float	heightRange;

    if (ds->deformationWave.base != 1234567.0f) {
        base = ds->deformationWave.base;
    } else if (backEnd.currentEntity) {
        base = (float)backEnd.currentEntity->e.surfaces[0] / 16.0 - 8.0;
    } else {
        base = 0.0f  ;
    }

    if (ds->deformationWave.amplitude != 1234567.0f) {
		amplitude = ds->deformationWave.amplitude;
    } else if (backEnd.currentEntity) {
		amplitude = (float)backEnd.currentEntity->e.surfaces[1] / 16.0;
    } else {
		amplitude = 0.0f  ;
    }

    if (ds->deformationWave.phase != 1234567.0f) {
		phase = ds->deformationWave.phase;
    } else if (backEnd.currentEntity) {
		phase = (float)backEnd.currentEntity->e.surfaces[2] / 16.0 - 8.0;
    } else {
		phase = 0.0f  ;
    }

    if (ds->deformationWave.frequency != 1234567.0f) {
		frequency = ds->deformationWave.frequency;
    } else if (backEnd.currentEntity) {
		frequency = (float)backEnd.currentEntity->e.surfaces[3] / 16.0;
    } else {
		frequency = 0.0f  ;
    }

	if (!backEnd.currentEntity)
	{
		base *= 1.0f  ;
		amplitude *= 1.0f  ;
		phase *= 1.0f  ;
		frequency *= 1.0f  ;
	}

	if ( frequency == 0 )
	{
		scale = EvalWaveForm( &ds->deformationWave );

		for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 )
		{
			VectorScale( normal, scale, offset );

			xyz[0] += offset[0];
			xyz[1] += offset[1];
			xyz[2] += offset[2];
		}

		return;
	}

	table = TableForFunc( ds->deformationWave.func );

	minHeight   =  1000000.0f;
	maxHeight   = -1000000.0f;
	heightRange = 0.0f;


	if ( ds->deformation == DEFORM_HEIGHTWAVE || ds->deformation == DEFORM_INVERSEHEIGHTWAVE )
	{
		const float *v = ( const float * ) tess.xyz;

		for ( i = 0; i < tess.numVertexes; i++, v += 4 )
		{
			if ( v[2] < minHeight ) {
				minHeight = v[2];
			}
			if ( v[2] > maxHeight ) {
				maxHeight = v[2];
			}
		}

		heightRange = maxHeight - minHeight;
	}

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 )
	{
		float	off;
		float	vertAmplitude = amplitude;
		float	vertFrequency = frequency;

		if ( ds->deformationWave.wind ) {
			int n = (int)( xyz[0] + xyz[1] + xyz[2] ) % 100;

			if ( n < 0 ) {
				n = -n;
			}

			vertFrequency = frequency - ( frequency * ( (float)n / 400.0f ) );
		}

		if ( ds->deformation == DEFORM_HEIGHTWAVE && heightRange != 0 ) {
			vertAmplitude = amplitude * ( ( xyz[2] - minHeight ) / heightRange );
		} else if ( ds->deformation == DEFORM_INVERSEHEIGHTWAVE && heightRange != 0 ) {
			vertAmplitude = amplitude * ( ( maxHeight - xyz[2] ) / heightRange );
		}

		off = ( xyz[0] + xyz[1] + xyz[2] ) * ds->deformationSpread;

		scale = WAVEVALUE( table, base, vertAmplitude, phase + off, vertFrequency );

		VectorScale( normal, scale, offset );

		xyz[0] += offset[0];
		xyz[1] += offset[1];
		xyz[2] += offset[2];
	}
}

void RB_CalcDeformNormals( deformStage_t *ds ) {
	int i;
	float	scale;
	float	*xyz = ( float * ) tess.xyz;
	float	*normal = ( float * ) tess.normal;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, normal += 4 ) {
		scale = 0.98f;
		scale = R_NoiseGet4f( xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 0 ] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f( 100 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 1 ] += ds->deformationWave.amplitude * scale;

		scale = 0.98f;
		scale = R_NoiseGet4f( 200 + xyz[0] * scale, xyz[1] * scale, xyz[2] * scale,
			tess.shaderTime * ds->deformationWave.frequency );
		normal[ 2 ] += ds->deformationWave.amplitude * scale;

		VectorNormalizeFast( normal );
	}
}

void RB_CalcBulgeVertexes( deformStage_t *ds ) {
	int i;
	const float *st = ( const float * ) tess.texCoords[0];
	float		*xyz = ( float * ) tess.xyz;
	float		*normal = ( float * ) tess.normal;
	float		now;

	now = backEnd.refdef.time * ds->bulgeSpeed * 0.001f;

	for ( i = 0; i < tess.numVertexes; i++, xyz += 4, st += 4, normal += 4 ) {
		int		off;
		float scale;

		// retail scales by FUNCTABLE_SIZE / (2*PI), not FUNCTABLE_SIZE: the constant
		off = ( FUNCTABLE_SIZE / ( M_PI * 2 ) ) * ( st[0] * ds->bulgeWidth + now );

		scale = tr.sinTable[ off & FUNCTABLE_MASK ] * ds->bulgeHeight;
			
		xyz[0] += normal[0] * scale;
		xyz[1] += normal[1] * scale;
		xyz[2] += normal[2] * scale;
	}
}


void RB_CalcMoveVertexes( deformStage_t *ds ) {
	int			i;
	float		*xyz;
	float		*table;
	float		scale;
	vec3_t		offset;

	table = TableForFunc( ds->deformationWave.func );

	scale = WAVEVALUE( table, ds->deformationWave.base, 
		ds->deformationWave.amplitude,
		ds->deformationWave.phase,
		ds->deformationWave.frequency );

	VectorScale( ds->moveVector, scale, offset );

	xyz = ( float * ) tess.xyz;
	for ( i = 0; i < tess.numVertexes; i++, xyz += 4 ) {
		VectorAdd( xyz, offset, xyz );
	}
}


void DeformText( const char *text ) {
	int		i;
	vec3_t	origin, width, height;
	size_t	len;
	int		ch;
	byte	color[4];
	float	bottom, top;
	vec3_t	mid;

	height[0] = 0;
	height[1] = 0;
	height[2] = -1;
	CrossProduct( tess.normal[0], height, width );

	VectorClear( mid );
	bottom = 999999;
	top = -999999;
	for ( i = 0 ; i < 4 ; i++ ) {
		VectorAdd( tess.xyz[i], mid, mid );
		if ( tess.xyz[i][2] < bottom ) {
			bottom = tess.xyz[i][2];
		}
		if ( tess.xyz[i][2] > top ) {
			top = tess.xyz[i][2];
		}
	}
	VectorScale( mid, 0.25f, origin );

	height[0] = 0;
	height[1] = 0;
	height[2] = ( top - bottom ) * 0.5f;

	VectorScale( width, height[2] * -0.75f, width );

	len = strlen( text );
	VectorMA( origin, (len-1), width, origin );

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	color[0] = color[1] = color[2] = color[3] = 255;

	for ( i = 0 ; i < len ; i++ ) {
		ch = text[i];
		ch &= 255;

		if ( ch != ' ' ) {
			int		row, col;
			float	frow, fcol, size;

			row = ch>>4;
			col = ch&15;

			frow = row*0.0625f;
			fcol = col*0.0625f;
			size = 0.0625f;

			RB_AddQuadStampExt( origin, width, height, color, fcol, frow, fcol + size, frow + size );
		}
		VectorMA( origin, -2, width, origin );
	}
}

static void GlobalVectorToLocal( const vec3_t in, vec3_t out ) {
	out[0] = DotProduct( in, backEnd.ori.axis[0] );
	out[1] = DotProduct( in, backEnd.ori.axis[1] );
	out[2] = DotProduct( in, backEnd.ori.axis[2] );
}

static void AutospriteDeform(void) {
    int		i;
    int		oldVerts;
    float* xyz;
    vec3_t	mid, delta;
    float	radius;
    vec3_t	left, up;
    vec3_t	leftDir, upDir;

    if (tess.numVertexes & 3) {
        ri.Printf(PRINT_WARNING, "Autosprite shader %s had odd vertex count\n", tess.shader->name);
    }
    if (tess.numIndexes != (tess.numVertexes >> 2) * 6) {
        ri.Printf(PRINT_WARNING, "Autosprite shader %s had odd index count\n", tess.shader->name);
    }

    oldVerts = tess.numVertexes;
    tess.numVertexes = 0;
    tess.numIndexes = 0;

    if (backEnd.currentEntity != &tr.worldEntity) {
        GlobalVectorToLocal(backEnd.viewParms.ori.axis[1], leftDir);
        GlobalVectorToLocal(backEnd.viewParms.ori.axis[2], upDir);
    } else {
        VectorCopy(backEnd.viewParms.ori.axis[1], leftDir);
        VectorCopy(backEnd.viewParms.ori.axis[2], upDir);
    }

    for (i = 0; i < oldVerts; i += 4) {
        xyz = tess.xyz[i];

        mid[0] = 0.25f * (xyz[0] + xyz[4] + xyz[8] + xyz[12]);
        mid[1] = 0.25f * (xyz[1] + xyz[5] + xyz[9] + xyz[13]);
        mid[2] = 0.25f * (xyz[2] + xyz[6] + xyz[10] + xyz[14]);

        VectorSubtract(xyz, mid, delta);
        radius = VectorLength(delta) * 0.707f;		

        VectorScale(leftDir, radius, left);
        VectorScale(upDir, radius, up);

        if (backEnd.viewParms.isMirror) {
            VectorSubtract(vec3_origin, left, left);
        }

        if (backEnd.currentStaticModel || backEnd.currentEntity->e.nonNormalizedAxes) {
            float axisLength;

			if (backEnd.currentStaticModel) {
				axisLength = VectorLength(backEnd.currentStaticModel->axis[0]);
			} else {
				axisLength = VectorLength(backEnd.currentEntity->e.axis[0]);
			}
            
            if (!axisLength) {
                axisLength = 0;
            }
            else {
                axisLength = 1.0f / axisLength;
            }
            VectorScale(left, axisLength, left);
            VectorScale(up, axisLength, up);
        }

        RB_AddQuadStamp(mid, left, up, tess.vertexColors[i]);
    }
}


vkIndex_t edgeVerts[6][2] = {
	{ 0, 1 },
	{ 0, 3 },
	{ 0, 2 },
	{ 1, 3 },
	{ 1, 2 },
	{ 3, 2 }
};

static void Autosprite2Deform( void ) {
	int		i, j;
	int		indexes;
	float	*xyz;
	vec3_t	forward;

	if ( tess.numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "Autosprite2 shader %s had odd vertex count\n", tess.shader->name );
	}
	if ( tess.numIndexes != ( tess.numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "Autosprite2 shader %s had odd index count\n", tess.shader->name );
	}

	if ( backEnd.currentStaticModel || backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.ori.axis[0], forward );
	} else {
		VectorCopy( backEnd.viewParms.ori.axis[0], forward );
	}

	for ( i = 0, indexes = 0 ; i < tess.numVertexes ; i+=4, indexes+=6 ) {
		float shortLengths[2];
		int shortNums[2];
		float longLengths[2];
		int longNums[2];
		vec3_t	mid[2];
		vec3_t	major, minor;
		float	*v1, *v2, *v3, *v4;
		qboolean firstOnLeft, secondOnLeft;
		float edgeLength;

		xyz = tess.xyz[i];

		shortNums[0] = shortNums[1] = 0;
		shortLengths[0] = shortLengths[1] = 1000000000;
		longNums[1] = longNums[0] = 0;
		longLengths[1] = longLengths[0] = 0;

		for ( j = 0 ; j < 6 ; j++ ) {
			float	l;
			vec3_t	temp;

			v1 = xyz + 4 * edgeVerts[j][0];
			v2 = xyz + 4 * edgeVerts[j][1];

			VectorSubtract( v1, v2, temp );
			
			l = DotProduct( temp, temp );
			if ( l < shortLengths[0] ) {
				shortNums[1] = shortNums[0];
				shortLengths[1] = shortLengths[0];
				shortNums[0] = j;
				shortLengths[0] = l;
			} else if ( l < shortLengths[1] ) {
				shortNums[1] = j;
				shortLengths[1] = l;
			}

			if (l > longLengths[0]) {
				longNums[1] = longNums[0];
				longLengths[1] = longLengths[0];
				longNums[0] = j;
				longLengths[0] = l;
			} else if (l > longLengths[1]) {
				longNums[1] = j;
				longLengths[1] = l;
			}
		}

		for ( j = 0 ; j < 2 ; j++ ) {
			v1 = xyz + 4 * edgeVerts[shortNums[j]][0];
			v2 = xyz + 4 * edgeVerts[shortNums[j]][1];

			mid[j][0] = 0.5f * (v1[0] + v2[0]);
			mid[j][1] = 0.5f * (v1[1] + v2[1]);
			mid[j][2] = 0.5f * (v1[2] + v2[2]);
		}

		VectorSubtract( mid[1], mid[0], major );

		CrossProduct( major, forward, minor );
		VectorNormalize( minor );

        v1 = xyz + 4 * edgeVerts[shortNums[0]][0];
        v2 = xyz + 4 * edgeVerts[shortNums[0]][1];

		if (edgeVerts[shortNums[0]][0] == edgeVerts[longNums[0]][0]
			|| edgeVerts[shortNums[0]][0] == edgeVerts[longNums[0]][1])
		{
			firstOnLeft = qtrue;
		}
		else
		{
			firstOnLeft = qfalse;
        }

        if (edgeVerts[shortNums[1]][0] == edgeVerts[longNums[1]][0]
            || edgeVerts[shortNums[1]][0] == edgeVerts[longNums[1]][1])
        {
            secondOnLeft = qtrue;
        }
        else
        {
			secondOnLeft = qfalse;
        }

		if (firstOnLeft == secondOnLeft)
        {
            v3 = xyz + 4 * edgeVerts[shortNums[1]][0];
            v4 = xyz + 4 * edgeVerts[shortNums[1]][1];
		}
		else
        {
            v3 = xyz + 4 * edgeVerts[shortNums[1]][1];
            v4 = xyz + 4 * edgeVerts[shortNums[1]][0];
		}

		edgeLength = sqrt(shortLengths[0]) * 0.5;
        v1[0] = -edgeLength * minor[0] + mid[0][0];
        v1[1] = -edgeLength * minor[1] + mid[0][1];
        v1[2] = -edgeLength * minor[2] + mid[0][2];
		v2[0] = minor[0] * edgeLength + mid[0][0];
        v2[1] = minor[1] * edgeLength + mid[0][1];
        v2[2] = minor[2] * edgeLength + mid[0][2];

        edgeLength = sqrt(shortLengths[1]) * 0.5;
		v3[0] = -edgeLength * minor[0] + mid[1][0];
        v3[1] = -edgeLength * minor[1] + mid[1][1];
        v3[2] = -edgeLength * minor[2] + mid[1][2];
		v4[0] = minor[0] * edgeLength + mid[1][0];
        v4[1] = minor[1] * edgeLength + mid[1][1];
        v4[2] = minor[2] * edgeLength + mid[1][2];
	}
}

void RB_ProjectionShadowDeform( void ) {
}

void RB_DeformTessGeometry( void ) {
	int		i;
	deformStage_t	*ds;

	for ( i = 0 ; i < tess.shader->numDeforms ; i++ ) {
		ds = &tess.shader->deforms[ i ];

		switch ( ds->deformation ) {
		case DEFORM_NONE:
			break;
		case DEFORM_WAVE:
		case DEFORM_HEIGHTWAVE:
		case DEFORM_INVERSEHEIGHTWAVE:
			if ( r_allowGeometryDeforms->integer ) {
				RB_CalcDeformVertexes( ds );
			}
			break;
		case DEFORM_NORMALS:
			if ( r_allowGeometryDeforms->integer ) {
				RB_CalcDeformNormals( ds );
			}
			break;
		case DEFORM_BULGE:
			if ( r_allowGeometryDeforms->integer ) {
				RB_CalcBulgeVertexes( ds );
			}
			break;
		case DEFORM_MOVE:
			if ( r_allowGeometryDeforms->integer ) {
				RB_CalcMoveVertexes( ds );
			}
			break;
		case DEFORM_PROJECTION_SHADOW:
			if ( r_allowGeometryDeforms->integer ) {
				RB_ProjectionShadowDeform();
			}
			break;
		case DEFORM_AUTOSPRITE:
			AutospriteDeform();
			break;
		case DEFORM_AUTOSPRITE2:
			Autosprite2Deform();
			break;
		case DEFORM_TEXT0:
		case DEFORM_TEXT1:
		case DEFORM_TEXT2:
		case DEFORM_TEXT3:
		case DEFORM_TEXT4:
		case DEFORM_TEXT5:
		case DEFORM_TEXT6:
		case DEFORM_TEXT7:
			DeformText( backEnd.refdef.text[ds->deformation - DEFORM_TEXT0] );
			break;
		}
	}
}



void RB_CalcColorFromEntity( unsigned char *dstColors )
{
	int	i;
	int *pColors = ( int * ) dstColors;
	int c;

	if ( !backEnd.currentEntity )
		return;

	c = * ( int * ) backEnd.currentEntity->e.shaderRGBA;

	for ( i = 0; i < tess.numVertexes; i++, pColors++ )
	{
		*pColors = c;
	}
}

/* the entity-lighting record R_SetupEntityLighting computed, not the render
   entity's modulate -- the sphere's non-extralight arm draws this one */
void RB_CalcColorFromEntityLighting( unsigned char *dstColors )
{
	int   i;
	int  *pColors = ( int * ) dstColors;
	int   c;

	if ( !backEnd.currentEntity ) {
		return;
	}

	c = backEnd.currentEntity->ambientLightInt;

	for ( i = 0; i < tess.numVertexes; i++, pColors++ ) {
		*pColors = c;
	}
}

void RB_CalcColorFromOneMinusEntity( unsigned char *dstColors )
{
	int	i;
	int *pColors = ( int * ) dstColors;
	unsigned char invModulate[4];
	int c;

	if ( !backEnd.currentEntity )
		return;

	invModulate[0] = 255 - backEnd.currentEntity->e.shaderRGBA[0];
	invModulate[1] = 255 - backEnd.currentEntity->e.shaderRGBA[1];
	invModulate[2] = 255 - backEnd.currentEntity->e.shaderRGBA[2];
	invModulate[3] = 255 - backEnd.currentEntity->e.shaderRGBA[3];	

	c = * ( int * ) invModulate;
	(void)c;

	for ( i = 0; i < tess.numVertexes; i++, pColors++ )
	{
		*pColors = * ( int * ) invModulate;
	}
}

void RB_CalcColorFromConstant(unsigned char* dstColors, unsigned char* constantColor)
{
	int i;

	for (i = 0; i < tess.numVertexes; i++) {
		dstColors[i * 4] = constantColor[0];
		dstColors[i * 4 + 1] = constantColor[1];
		dstColors[i * 4 + 2] = constantColor[2];
		dstColors[i * 4 + 3] = constantColor[3];
	}
}

void RB_CalcRGBFromDot(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	vec3_t viewInModel;

	for (i = 0; i < tess.numVertexes; i++, colors += 4) {
		float f;

		VectorSubtract(backEnd.ori.viewOrigin, tess.xyz[i], viewInModel);
		VectorNormalizeFast(viewInModel);

        f = Square(DotProduct(tess.normal[i], viewInModel));
        f = ((alphaMax - alphaMin) * f + alphaMin) * 255.0;
		f = f - Q_max((int)(f - 255), 0);

		colors[0] = colors[1] = colors[2] = (unsigned char)Q_max(f, 0);
	}
}

void RB_CalcRGBFromOneMinusDot(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	vec3_t viewInModel;

	for (i = 0; i < tess.numVertexes; i++, colors += 4) {
		float f;

		VectorSubtract(backEnd.ori.viewOrigin, tess.xyz[i], viewInModel);
		VectorNormalizeFast(viewInModel);

        f = 1.0 - Square(DotProduct(tess.normal[i], viewInModel));
        f = ((alphaMax - alphaMin) * f + alphaMin) * 255.0;
		f = f - Q_max((int)(f - 255), 0);

		colors[0] = colors[1] = colors[2] = (unsigned char)Q_max(f, 0);
	}
}

void RB_CalcAlphaFromConstant(unsigned char* dstColors, int constantAlpha)
{
	int i;

	for (i = 0; i < tess.numVertexes; i++) {
		dstColors[i * 4 + 3] = constantAlpha;
	}
}

void RB_CalcAlphaFromDot(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	vec3_t viewInModel;

	for (i = 0; i < tess.numVertexes; i++, colors += 4) {
		float f;
		float d;

		VectorSubtract(backEnd.ori.viewOrigin, tess.xyz[i], viewInModel);
		VectorNormalizeFast(viewInModel);

		d = fabs(DotProduct(tess.normal[i], viewInModel));

		f = (d * (alphaMax - alphaMin) + alphaMin) * 255.0;
		f = f - Q_max((int)(f - 255), 0);

		colors[3] = (unsigned char)(int)Q_max(f, 0);
	}
}

void RB_CalcAlphaFromHeightFade(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	float dist;
	float alpha;

	for (i = 0; i < tess.numVertexes; i++) {
		dist = fabs(backEnd.ori.viewOrigin[2] - tess.xyz[i][2]);
		dist = Q_clamp_float(dist, alphaMin, alphaMax);
		alpha = (dist - alphaMin) / (alphaMax - alphaMin);
		colors[i * 4 + 3] = (unsigned char)((1.f - alpha) * 255.0);
	}
}

void RB_CalcAlphaFromOneMinusDot(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	vec3_t viewInModel;

	for (i = 0; i < tess.numVertexes; i++, colors += 4) {
		float f;

		VectorSubtract(backEnd.ori.viewOrigin, tess.xyz[i], viewInModel);
		VectorNormalizeFast(viewInModel);

		f = 1.0 - DotProduct(tess.normal[i], viewInModel);
		f = f * f;

		f = (f * (alphaMax - alphaMin) + alphaMin) * 255.0;
		f = f - Q_max((int)(f - 255), 0);

		colors[3] = (unsigned char)(int)Q_max(f, 0);
	}
}

void RB_CalcAlphaFromTexCoords(unsigned char* colors, float alphaMin, float alphaMax, int alphaMinCap, int alphaCap, float sWeight, float tWeight, float* st)
{
	int i;

	for (i = 0; i < tess.numVertexes; i++, colors += 4, st += 2) {
		float f;

        f = sWeight * tess.texCoords[i][0][0] + tWeight * tess.texCoords[i][0][1];
        f = ((alphaMax - alphaMin) * f + alphaMin) * 255.0;
		f = f - Q_max((int)(f - alphaCap), 0);
		f = alphaMinCap + Q_max((int)(f - alphaMinCap), 0);
	
		colors[3] = (unsigned char)f;
	}
}

void RB_CalcRGBFromTexCoords(unsigned char* colors, float alphaMin, float alphaMax, int alphaMinCap, int alphaCap, float sWeight, float tWeight, float* st)
{
	int i;

	for (i = 0; i < tess.numVertexes; i++, colors += 4, st += 2) {
		float f;

        f = sWeight * tess.texCoords[i][0][0] + tWeight * tess.texCoords[i][0][1];
        f = ((alphaMax - alphaMin) * f + alphaMin) * 255.0;
		f = f - Q_max((int)(f - alphaCap), 0);
		f = alphaMinCap + Q_max((int)(f - alphaMinCap), 0);

		colors[0] = colors[1] = colors[2] = (unsigned char)f;
	}
}

void RB_CalcAlphaFromEntity( unsigned char *dstColors )
{
	int	i;

	if ( !backEnd.currentEntity )
		return;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = backEnd.currentEntity->e.shaderRGBA[3];
	}
}

void RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors )
{
	int	i;

	if ( !backEnd.currentEntity )
		return;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = 0xff - backEnd.currentEntity->e.shaderRGBA[3];
	}
}

void RB_CalcWaveColor(const waveForm_t* wf, unsigned char* dstColors, unsigned char* constantColor)
{
	int i;
	int v;
	float glow;
	int *colors = ( int * ) dstColors;
	byte	color[4];


  if ( wf->func == GF_NOISE ) {
		glow = wf->base + R_NoiseGet4f( 0, 0, 0, ( tess.shaderTime + wf->phase ) * wf->frequency ) * wf->amplitude;
	} else {
		glow = EvalWaveForm( wf ) * tr.identityLight;
	}
	
	if ( glow < 0 ) {
		glow = 0;
	}
	else if ( glow > 1 ) {
		glow = 1;
	}

	if (constantColor)
	{
		color[0] = constantColor[0] * glow;
		color[1] = constantColor[1] * glow;
		color[2] = constantColor[2] * glow;
	}
	else
	{
		v = myftol(255 * glow);
		color[0] = color[1] = color[2] = v;
	}

	color[3] = 255;
	v = *(int*)color;
	
	for ( i = 0; i < tess.numVertexes; i++, colors++ ) {
		*colors = v;
	}
}

void RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors )
{
	int i;
	int v;
	float glow;

	glow = EvalWaveFormClamped( wf );

	v = 255 * glow;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		dstColors[3] = v;
	}
}



/* the walk's texcoord bodies are per-vertex heavier than the pack, so their
   break-even sits lower than VK_PACK_CHUNK's -- docs/controls/threadroot */
#define MT_VERTEX_CHUNK 1024

typedef struct {
	float		*st;
	const float	*xyz;
	const float	*normal;
	int			numVertexes;
	vec3_t		viewOrigin;
} rbEnvTex_t;

static void RB_CalcEnvironmentTexCoordsRange( void *vctx, int begin, int end ) {
	const rbEnvTex_t	*c = (const rbEnvTex_t *)vctx;
	int			i;
	const float	*v = c->xyz + 4 * begin;
	const float	*normal = c->normal + 4 * begin;
	float		*st = c->st + 2 * begin;
	vec3_t		viewer, reflected;
	float		d;

	for (i = begin ; i < end ; i++, v += 4, normal += 4, st += 2 ) 
	{
		VectorSubtract (c->viewOrigin, v, viewer);
		VectorNormalizeFast (viewer);

		d = DotProduct (normal, viewer);

		reflected[0] = normal[0]*2*d - viewer[0];
		reflected[1] = normal[1]*2*d - viewer[1];
		reflected[2] = normal[2]*2*d - viewer[2];

/* the reference renderer's tr_shade_calc.c carries this note in full */
		st[0] = 1.0 + reflected[1];
		st[1] = 1.0 - reflected[2];
	}
}

void RB_CalcEnvironmentTexCoords( float *st ) 
{
	rbEnvTex_t	c;

	c.st = st;
	c.xyz = tess.xyz[0];
	c.normal = tess.normal[0];
	c.numVertexes = tess.numVertexes;
	VectorCopy( backEnd.ori.viewOrigin, c.viewOrigin );

	JobPool_ParallelRange( c.numVertexes, MT_VERTEX_CHUNK, RB_CalcEnvironmentTexCoordsRange, &c );
}

typedef struct {
	float		*st;
	const float	*normal;
	int			numVertexes;
	vec3_t		viewer;			
} rbAngleEnvTex_t;

static void RB_CalcAngleBasedEnvironmentTexCoordsRange( void *vctx, int begin, int end ) {
	const rbAngleEnvTex_t	*c = (const rbAngleEnvTex_t *)vctx;
	int		i;
	const float	*normal = c->normal + 4 * begin;
	float	*st = c->st + 2 * begin;
	vec3_t	reflected;
	float	d;

	for ( i = begin ; i < end ; i++, normal += 4, st += 2 )
	{
		d = DotProduct( normal, c->viewer );

		reflected[0] = normal[0]*2*d - c->viewer[0];
		reflected[1] = normal[1]*2*d - c->viewer[1];
		reflected[2] = normal[2]*2*d - c->viewer[2];

/* the reference renderer's tr_shade_calc.c carries this note in full */
		st[0] = 1.0 + reflected[1];
		st[1] = 1.0 - reflected[2];
	}
}

void RB_CalcAngleBasedEnvironmentTexCoords( float *st )
{
	rbAngleEnvTex_t	c;

	c.st = st;
	c.normal = tess.normal[0];
	c.numVertexes = tess.numVertexes;

	c.viewer[0] = backEnd.ori.axis[0][0];
	c.viewer[1] = backEnd.ori.axis[0][1];
	c.viewer[2] = backEnd.ori.axis[0][2];
	VectorNormalizeFast( c.viewer );

	JobPool_ParallelRange( c.numVertexes, MT_VERTEX_CHUNK, RB_CalcAngleBasedEnvironmentTexCoordsRange, &c );
}

typedef struct {
	float		*st;
	const float	*xyz;
	const float	*normal;
	int			numVertexes;
	vec3_t		viewOrigin;
	vec3_t		axis[3];
} rbEnvTex2_t;

static void RB_CalcEnvironmentTexCoords2Range( void *vctx, int begin, int end ) {
	const rbEnvTex2_t	*c = (const rbEnvTex2_t *)vctx;
	int			i;
	const float	*v = c->xyz + 4 * begin;
	const float	*normal = c->normal + 4 * begin;
	float		*st = c->st + 2 * begin;
	vec3_t		viewer, reflected, worldReflected;
	float		d;

	for (i = begin; i < end; i++, v += 4, normal += 4, st += 2)
	{
		VectorSubtract(c->viewOrigin, v, viewer);
		VectorNormalizeFast(viewer);

		d = DotProduct(normal, viewer);
		if (d > 0.0f) {
			VectorCopy(viewer, reflected);
		} else {
			d *= -2;
			reflected[0] = normal[0] * d + viewer[0];
			reflected[1] = normal[1] * d + viewer[1];
			reflected[2] = normal[2] * d + viewer[2];
		}

		worldReflected[0] = reflected[0] * c->axis[0][0];
		worldReflected[2] = reflected[0] * c->axis[0][2];
		worldReflected[0] = c->axis[1][0] * reflected[1] + worldReflected[0];
		worldReflected[2] = c->axis[1][2] * reflected[1] + worldReflected[2];
		worldReflected[0] = c->axis[2][0] * reflected[2] + worldReflected[0];
		worldReflected[2] = c->axis[2][2] * reflected[2] + worldReflected[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
}

void RB_CalcEnvironmentTexCoords2(float* st)
{
	rbEnvTex2_t	c;
	int			k, j;

	c.st = st;
	c.xyz = tess.xyz[0];
	c.normal = tess.normal[0];
	c.numVertexes = tess.numVertexes;
	VectorCopy( backEnd.ori.viewOrigin, c.viewOrigin );
	for ( k = 0; k < 3; k++ ) {
		for ( j = 0; j < 3; j++ ) {
			c.axis[k][j] = backEnd.ori.axis[k][j];
		}
	}

	JobPool_ParallelRange( c.numVertexes, MT_VERTEX_CHUNK, RB_CalcEnvironmentTexCoords2Range, &c );
}

typedef struct {
	float		*st;
	const float	*xyz;
	const float	*normal;
	int			numVertexes;
	vec3_t		viewOrigin;
	vec3_t		sunAxis[3];
} rbSunRefl_t;

static void RB_CalcSunReflectionTexCoordsRange( void *vctx, int begin, int end ) {
	const rbSunRefl_t	*c = (const rbSunRefl_t *)vctx;
	int			i;
	const float	*v = c->xyz + 4 * begin;
	const float	*normal = c->normal + 4 * begin;
	float		*st = c->st + 2 * begin;
	vec3_t		viewer;
	vec3_t		reflected;
	vec3_t		sunReflected;
	float		d;

	for (i = begin; i < end; i++, v += 4, normal += 4, st += 2) {
		VectorSubtract(v, c->viewOrigin, viewer);
		VectorNormalizeFast(viewer);

		d = DotProduct(viewer, normal);
		if (d > 0) {
			VectorCopy(viewer, reflected);
		} else {
			d *= -2;
			VectorMA(viewer, d, normal, reflected);
		}

		VectorScale(c->sunAxis[0], reflected[0], sunReflected);
		VectorMA(sunReflected, reflected[2], c->sunAxis[1], sunReflected);
		VectorMA(sunReflected, -reflected[1], c->sunAxis[2], sunReflected);

		st[0] = sunReflected[1] + 0.5;
		st[1] = sunReflected[2] + 0.5;
	}
}

void RB_CalcSunReflectionTexCoords(float* st) {
	rbSunRefl_t	c;

	c.st = st;
	c.xyz = tess.xyz[0];
	c.normal = tess.normal[0];
	c.numVertexes = tess.numVertexes;
	VectorCopy( backEnd.ori.viewOrigin, c.viewOrigin );

	/* the sun basis is built once, before the vertex loop, as retail does: it is
	   loop-invariant and copied in so the worker reads no global */
	c.sunAxis[0][0] = tr.sunDirection[0];
	c.sunAxis[0][1] = tr.sunDirection[1];
	c.sunAxis[0][2] = tr.sunDirection[2];
	c.sunAxis[1][0] = tr.sunDirection[1];
	c.sunAxis[1][1] = -tr.sunDirection[0];
	c.sunAxis[1][2] = 0.0;
	VectorNormalizeFast(c.sunAxis[1]);
	CrossProduct(c.sunAxis[0], c.sunAxis[1], c.sunAxis[2]);

	JobPool_ParallelRange( c.numVertexes, MT_VERTEX_CHUNK, RB_CalcSunReflectionTexCoordsRange, &c );
}

typedef struct {
	float		*st;
	const float	*xyz;
	int			numVertexes;
	float		now;
	float		amplitude;
} rbTurbulent_t;

static void RB_CalcTurbulentTexCoordsRange( void *vctx, int begin, int end ) {
	const rbTurbulent_t	*c = (const rbTurbulent_t *)vctx;
	int		i;
	float	*st = c->st + 2 * begin;

	for ( i = begin; i < end; i++, st += 2 )
	{
		float s = st[0];
		float t = st[1];

		st[0] = s + tr.sinTable[ ( ( int ) ( ( ( c->xyz[i*4+0] + c->xyz[i*4+2] )* 1.0/128 * 0.125 + c->now ) * FUNCTABLE_SIZE ) ) & ( FUNCTABLE_MASK ) ] * c->amplitude;
		st[1] = t + tr.sinTable[ ( ( int ) ( ( c->xyz[i*4+1] * 1.0/128 * 0.125 + c->now ) * FUNCTABLE_SIZE ) ) & ( FUNCTABLE_MASK ) ] * c->amplitude;
	}
}

void RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *st )
{
	rbTurbulent_t	c;

	c.st = st;
	c.xyz = tess.xyz[0];
	c.numVertexes = tess.numVertexes;
	c.now = ( wf->phase + tess.shaderTime * wf->frequency );
	c.amplitude = wf->amplitude;

	JobPool_ParallelRange( c.numVertexes, MT_VERTEX_CHUNK, RB_CalcTurbulentTexCoordsRange, &c );
}

void RB_CalcScaleTexCoords( const float scale[2], float *st )
{
	int i;

	for ( i = 0; i < tess.numVertexes; i++, st += 2 )
	{
		st[0] *= scale[0];
		st[1] *= scale[1];
	}
}

void RB_CalcOffsetTexCoords( const float *offset, float *st )
{
	int i;
	float offsetS, offsetT;

	if (offset[0] != 1234567) {
		offsetS = offset[0];
	} else if (backEnd.currentEntity) {
        offsetS = backEnd.currentEntity->e.tag_num;
    } else {
        offsetS = 0.0f  ;
    }

	if (offset[0] != 1234567) {
		offsetT = offset[1];
	} else if (backEnd.currentEntity) {
		offsetT = backEnd.currentEntity->e.skin_num;
    } else {
		offsetT = 0.0f  ;
    }

    if (!backEnd.currentEntity) {
        offsetS = offsetS * 1.0f  ;
        offsetT = offsetT * 1.0f  ;
    }

	for (i = 0; i < tess.numVertexes; i++, st += 2) {
		float s, t;

		s = st[0];
		t = st[1];

		st[0] = s + offsetS;
		st[1] = t + offsetT;
	}
}

void RB_CalcParallaxTexCoords( const float *rate, float *st )
{
	int i;
	float offsetS, offsetT;

    /* frame-carry: RB_Calc* runs on the walk against the record's refdef */
    offsetS = backEnd.refdef.vieworg[0] * rate[0];
    offsetT = backEnd.refdef.vieworg[1] * rate[1];
    for (i = 0; i < tess.numVertexes; i++, st += 2) {
		st[0] += offsetS;
		st[1] += offsetT;
    }
}

static vec3_t rb_baseaxis[18];


void RB_TextureAxisFromPlane(const vec3_t normal, vec3_t xv, vec3_t yv)
{
	int bestaxis;
	vec_t dot;
	vec_t best;
	int i;

	if (normal[0] == 1) {
		bestaxis = 2;
	} else if (normal[0] == -1) {
		bestaxis = 3;
	} else if (normal[1] == 1) {
		bestaxis = 4;
	} else if (normal[1] == -1) {
		bestaxis = 5;
	} else if (normal[2] == 1) {
		bestaxis = 0;
	} else if (normal[2] == -1) {
		bestaxis = 1;
	} else {
		best = 0;
		bestaxis = 0;

		for (i = 0; i < 6; i++) {
			dot = DotProduct(rb_baseaxis[i * 3], normal);
			if (dot > best) {
				best = dot;
				bestaxis = i;
			}
		}
	}

	VectorCopy(rb_baseaxis[bestaxis * 3 + 1], xv);
	VectorCopy(rb_baseaxis[bestaxis * 3 + 2], yv);
}

void RB_QuakeTextureVecs(const vec3_t normal, const vec2_t scale, vec3_t mappingVecs[2])
{
	RB_TextureAxisFromPlane(normal, mappingVecs[0], mappingVecs[1]);

	VectorScale(mappingVecs[0], scale[0], mappingVecs[0]);
	VectorScale(mappingVecs[1], scale[1], mappingVecs[1]);
}

void RB_CalcMacroTexCoords( const  float* rate, float *st )
{
	int i;
	vec3_t vecs[2];
	vec_t* v;
	vec_t* normal;
	float width, height;
    v = tess.xyz[0];

    normal = tess.normal[0];
    width = (float)tess.shader->unfoggedStages[0]->bundle[0].image[0]->uploadWidth;
    height = (float)tess.shader->unfoggedStages[0]->bundle[0].image[0]->uploadHeight;

	for (i = 0; i < tess.numVertexes; i++, v += 4, normal += 4, st += 2) {
		RB_QuakeTextureVecs(normal, rate, vecs);
		st[0] = DotProduct(vecs[0], v) / width;
		st[1] = DotProduct(vecs[1], v) / height;
	}
}

void RB_CalcScrollTexCoords( const float scrollSpeed[2], float *st )
{
	int i;
	float timeScale = tess.shaderTime;
	float adjustedScrollS, adjustedScrollT;

	adjustedScrollS = scrollSpeed[0] * timeScale;
	adjustedScrollT = scrollSpeed[1] * timeScale;

	adjustedScrollS = adjustedScrollS - floor( adjustedScrollS );
	adjustedScrollT = adjustedScrollT - floor( adjustedScrollT );

	for ( i = 0; i < tess.numVertexes; i++, st += 2 )
	{
		st[0] += adjustedScrollS;
		st[1] += adjustedScrollT;
	}
}

void RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *st  )
{
	int i;

	for ( i = 0; i < tess.numVertexes; i++, st += 2 )
	{
		float s = st[0];
		float t = st[1];

		st[0] = s * tmi->matrix[0][0] + t * tmi->matrix[1][0] + tmi->translate[0];
		st[1] = s * tmi->matrix[0][1] + t * tmi->matrix[1][1] + tmi->translate[1];
	}
}

void RB_CalcRotateTexCoords(float degsPerSecond, float degsPerSecondCoef, float* st, float start)
{
	float degs;
	int index;
	float sinValue, cosValue;
	texModInfo_t tmi;

    if (degsPerSecond != 1234567) {
		degs = -degsPerSecond * degsPerSecondCoef * tess.shaderTime - start;
	} else if (backEnd.currentEntity) {
		degs = -(float)backEnd.currentEntity->e.tag_num * degsPerSecondCoef * tess.shaderTime - start;
	} else {
		degs = 0.0f  ;
	}
	if (!backEnd.currentEntity) {
		degs *= 1.0f  ;
	}
	index = degs * ( FUNCTABLE_SIZE / 360.0f );

	sinValue = tr.sinTable[ index & FUNCTABLE_MASK ];
	cosValue = tr.sinTable[ ( index + FUNCTABLE_SIZE / 4 ) & FUNCTABLE_MASK ];

	tmi.matrix[0][0] = cosValue;
	tmi.matrix[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.matrix[0][1] = sinValue;
	tmi.matrix[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_CalcTransformTexCoords( &tmi, st );
}






#if id386 && !( (defined __linux__ || defined __FreeBSD__ ) && (defined __i386__ ) ) 

long myftol( float f ) {
	static int tmp;
	__asm fld f
	__asm fistp tmp
	__asm mov eax, tmp
}

#endif

vec3_t lightOrigin = { -960, 1980, 96 };		

typedef struct {
	unsigned char	*alphas;
	const float		*xyz;
	const float		*normal;
	int				numVertexes;
	vec3_t			specOrigin;
	vec3_t			viewOrigin;
} rbSpecular_t;

static void RB_CalcSpecularAlphaRange( void *vctx, int begin, int end ) {
	const rbSpecular_t	*c = (const rbSpecular_t *)vctx;
	int			i;
	const float	*v = c->xyz + 4 * begin;
	const float	*normal = c->normal + 4 * begin;
	unsigned char	*alphas = c->alphas + 4 * begin + 3;
	vec3_t		viewer,  reflected;
	float		l, d;
	int			b;
	vec3_t		lightDir;

	for (i = begin ; i < end ; i++, v += 4, normal += 4, alphas += 4) {
		float ilength;

		VectorSubtract( c->specOrigin, v, lightDir );
		VectorNormalizeFast( lightDir );

		d = DotProduct (normal, lightDir);

		reflected[0] = normal[0]*2*d - lightDir[0];
		reflected[1] = normal[1]*2*d - lightDir[1];
		reflected[2] = normal[2]*2*d - lightDir[2];

		VectorSubtract (c->viewOrigin, v, viewer);
		ilength = Q_rsqrt( DotProduct( viewer, viewer ) );
		l = DotProduct (reflected, viewer);
		l *= ilength;

		if (l < 0) {
			b = 0;
		} else {
			l = l*l;
			l = l*l;
			b = l * 255;
			if (b > 255) {
				b = 255;
			}
		}

		*alphas = b;
	}
}

void RB_CalcSpecularAlpha(unsigned char* alphas, float alphaMax, vec3_t specOrigin) {
	rbSpecular_t	c;

	c.alphas = alphas;
	c.xyz = tess.xyz[0];
	c.normal = tess.normal[0];
	c.numVertexes = tess.numVertexes;
	VectorCopy( specOrigin, c.specOrigin );
	VectorCopy( backEnd.ori.viewOrigin, c.viewOrigin );

	JobPool_ParallelRange( c.numVertexes, MT_VERTEX_CHUNK, RB_CalcSpecularAlphaRange, &c );
}

void RB_CalcAlphaFromDotView(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	vec3_t viewInModel;

    for (i = 0; i < tess.numVertexes; i++, colors += 4) {
        union {
            float f;
            int i;
        } u;

		VectorCopy(backEnd.refdef.viewaxis[0], viewInModel);
		VectorNormalizeFast(viewInModel);

        u.f = DotProduct(viewInModel, tess.normal[i]);
        u.f = fabs(u.f);

		u.f = ((alphaMax - alphaMin) * u.f + alphaMin) * 255.0;
        u.f = u.f - (float)(~((int)(u.f - 255.0) >> 31) & (int)(u.f - 255.0));

        u.i &= ~(u.i >> 31);
		colors[3] = u.i;
	}
}

void RB_CalcAlphaFromOneMinusDotView(unsigned char* colors, float alphaMin, float alphaMax)
{
	int i;
	vec3_t viewInModel;

    for (i = 0; i < tess.numVertexes; i++, colors += 4) {
        union {
            float f;
            int i;
        } u;

		VectorCopy(backEnd.refdef.viewaxis[0], viewInModel);
		VectorNormalizeFast(viewInModel);

        u.f = DotProduct(viewInModel, tess.normal[i]);
        u.f = fabs(u.f);
		u.f = 1.0 - u.f;

        u.f = ((alphaMax - alphaMin) * u.f + alphaMin) * 255.0;
        u.f = u.f - (long double)(~((int)(u.f - 255.0) >> 31) & (int)(u.f - 255.0));

        u.i &= ~(u.i >> 31);
		colors[3] = u.i;
	}
}


typedef struct {
	unsigned char	*colors;
	const float		*xyz;
	const float		*normal;
	int				numVertexes;
	int				ambientLightInt;
	float			ambientLight[3];
	float			directedLight[3];
	float			lightDir[3];
} rbDiffuse_t;

static void RB_CalcDiffuseColorRange( void *vctx, int begin, int end ) {
	const rbDiffuse_t	*c = (const rbDiffuse_t *)vctx;
	int					i, j;
	const float			*v = c->xyz + 4 * begin;
	const float			*normal = c->normal + 4 * begin;
	float				incoming;
	unsigned char		*colors = c->colors;

	for (i = begin ; i < end ; i++, v += 4, normal += 4) {
		incoming = DotProduct (normal, c->lightDir);
		if ( incoming <= 0 ) {
			*(int *)&colors[i*4] = c->ambientLightInt;
			continue;
		}
		j = myftol( c->ambientLight[0] + incoming * c->directedLight[0] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+0] = j;

		j = myftol( c->ambientLight[1] + incoming * c->directedLight[1] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+1] = j;

		j = myftol( c->ambientLight[2] + incoming * c->directedLight[2] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+2] = j;

		colors[i*4+3] = 255;
	}
}

void RB_CalcDiffuseColor( unsigned char *colors )
{
	int				i, j;
	float			*v, *normal;
	float			incoming;
	trRefEntity_t	*ent;
	int				ambientLightInt;
	vec3_t			ambientLight;
	vec3_t			lightDir;
	vec3_t			directedLight;
	int				numVertexes;
	ent = backEnd.currentEntity;
	ambientLightInt = ent->ambientLightInt;
	VectorCopy( ent->ambientLight, ambientLight );
	VectorCopy( ent->directedLight, directedLight );
	VectorCopy( ent->lightDir, lightDir );

	v = tess.xyz[0];
	normal = tess.normal[0];

	numVertexes = tess.numVertexes;

	if (ent->directedLight[0] == 0.0f && ent->directedLight[1] == 0.0f && ent->directedLight[2] == 0.0f) {
		for (i = 0 ; i < numVertexes ; i++) {
			*(int *)&colors[i*4] = ambientLightInt;
		}
		return;
	}

	{
		rbDiffuse_t c;

		c.colors         = colors;
		c.xyz            = tess.xyz[0];
		c.normal         = tess.normal[0];
		c.numVertexes    = numVertexes;
		c.ambientLightInt = ambientLightInt;
		VectorCopy( ambientLight, c.ambientLight );
		VectorCopy( directedLight, c.directedLight );
		VectorCopy( lightDir, c.lightDir );

		JobPool_ParallelRange( numVertexes, MT_VERTEX_CHUNK, RB_CalcDiffuseColorRange, &c );
		return;
	}

	for (i = 0 ; i < numVertexes ; i++, v += 4, normal += 4) {
		incoming = DotProduct (normal, lightDir);
		if ( incoming <= 0 ) {
			*(int *)&colors[i*4] = ambientLightInt;
			continue;
		} 
		j = myftol( ambientLight[0] + incoming * directedLight[0] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+0] = j;

		j = myftol( ambientLight[1] + incoming * directedLight[1] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+1] = j;

		j = myftol( ambientLight[2] + incoming * directedLight[2] );
		if ( j > 255 ) {
			j = 255;
		}
		colors[i*4+2] = j;

		colors[i*4+3] = 255;
	}
}



typedef struct {
	float		*dst;
	const vec4_t	*xyz;
	int			numVertexes;
	float		sConstant, sVector[3];
	float		tConstant, tVector[3];
	qboolean	viewOutside;
} rbFogTC_t;

static void RB_CalcFogTexCoordsRange( void *vctx, int begin, int end ) {
	const rbFogTC_t	*c = (const rbFogTC_t *)vctx;
	int		i;
	float	*dstTexCoords = c->dst + 2 * begin;
	float	s, t, tRaw;

	for ( i = begin; i < end; i++ ) {
		const float *xyz = c->xyz[i];

		s = ( c->sConstant + 0.001953125f )
			+ ( ( xyz[0] * c->sVector[0] + xyz[1] * c->sVector[1] ) + xyz[2] * c->sVector[2] );
		tRaw = c->tConstant
			+ ( ( xyz[0] * c->tVector[0] + xyz[1] * c->tVector[1] ) + xyz[2] * c->tVector[2] );

		if ( c->viewOutside ) {
			if ( tRaw >= 1.0f ) {
				t = 0.03125f + ( 0.9375f * tRaw ) / ( tRaw - c->tConstant );
			} else {
				t = 0.03125f;
			}
		} else if ( tRaw >= 0.0f ) {
			t = 0.96875f;
		} else {
			t = 0.03125f;
		}

		dstTexCoords[0] = s;
		dstTexCoords[1] = t;
		dstTexCoords += 2;
	}
}

void RB_CalcFogTexCoords( float *dstTexCoords ) {
	fog_t		*fog;
	float		tcScale;
	float		sConstant, sVector[3];
	float		tConstant, tVector[3];
	qboolean	viewOutside;
	vec3_t		delta;

	fog = &tr.world->fogs[tess.fogNum];

	if ( fog->parms.wave.func ) {
		float	wave = EvalWaveForm( &fog->parms.wave );

		if ( wave < 1.0f ) {
			wave = 1.0f;
		}
		fog->tcScale = 8.0f * ( 1.0f / wave );
	}
	tcScale = fog->tcScale;

	VectorSubtract( backEnd.ori.origin, backEnd.viewParms.ori.origin, delta );
	sConstant = DotProduct( delta, backEnd.viewParms.ori.axis[0] ) * tcScale;
	sVector[0] = -backEnd.ori.modelMatrix[3] * tcScale;
	sVector[1] = -backEnd.ori.modelMatrix[7] * tcScale;
	sVector[2] = -backEnd.ori.modelMatrix[11] * tcScale;

	if ( fog->hasSurface ) {
		tVector[0] = DotProduct( fog->surface, backEnd.ori.axis[0] );
		tVector[1] = DotProduct( fog->surface, backEnd.ori.axis[1] );
		tVector[2] = DotProduct( fog->surface, backEnd.ori.axis[2] );
		tConstant = -fog->surface[3]
			+ DotProduct( fog->surface, backEnd.ori.origin )
			+ DotProduct( backEnd.ori.viewOrigin, tVector );
	} else {
		tConstant = 1.0f;
		tVector[0] = tVector[1] = tVector[2] = 0.0f;
	}

	viewOutside = ( tConstant < 0.0f );		 

	{
		rbFogTC_t c;

		c.dst = dstTexCoords;
		c.xyz = (const vec4_t *)tess.xyz;
		c.numVertexes = tess.numVertexes;
		c.sConstant = sConstant;
		c.sVector[0] = sVector[0];
		c.sVector[1] = sVector[1];
		c.sVector[2] = sVector[2];
		c.tConstant = tConstant;
		c.tVector[0] = tVector[0];
		c.tVector[1] = tVector[1];
		c.tVector[2] = tVector[2];
		c.viewOutside = viewOutside;

		JobPool_ParallelRange( tess.numVertexes, MT_VERTEX_CHUNK, RB_CalcFogTexCoordsRange, &c );
	}
}

typedef struct {
	float	*densities;
	const vec4_t	*xyz;
	float	*viewOrigin;
	int		numVertexes;
	int		arm;
	float	scale;
	float	tVector[3];
	float	planeConst, dView;
} rbFogDensity_t;

static void RB_CalcAlphaFogDensitiesRange( void *vctx, int begin, int end ) {
	const rbFogDensity_t	*c = (const rbFogDensity_t *)vctx;
	int		i;
	float	*densities = c->densities;

	if ( c->arm == 0 ) {
		const float	scale = c->scale;
		const float	*viewOrigin = c->viewOrigin;

		for ( i = begin; i < end; i++ ) {
			vec3_t	delta;
			float	dist2, dist;

			VectorSubtract( c->xyz[i], viewOrigin, delta );
			dist2 = delta[2] * delta[2] + ( delta[0] * delta[0] + delta[1] * delta[1] );
			dist = scale * sqrtf( dist2 );
			if ( dist > 1.0f ) {
				dist = 1.0f;
			}
			densities[i] = dist;
		}
		return;
	}

	{
		const float	*viewOrigin = c->viewOrigin;

		for ( i = begin; i < end; i++ ) {
			const float	*xyz = c->xyz[i];
			float	dVertex = ( ( xyz[0] * c->tVector[0] + xyz[1] * c->tVector[1] )
								+ xyz[2] * c->tVector[2] ) - c->planeConst;

			if ( c->arm == 1 ) {
				if ( dVertex >= 0.0f ) {
					vec3_t	delta;
					float	dist2, dist, density;

					VectorSubtract( viewOrigin, xyz, delta );
					dist2 = delta[2] * delta[2]
						  + ( delta[0] * delta[0] + delta[1] * delta[1] );
					dist = sqrtf( dist2 );
					density = 1.0f;
					if ( ( 0.5f * dist ) * ( c->dView + dVertex ) <= 1.0f ) {
						density = ( 0.5f * dist ) * ( c->dView + dVertex );
					}
					densities[i] = density;
				} else {
					densities[i] = 0.0f;
				}
			} else {
				if ( dVertex >= 0.0f ) {
					vec3_t	delta;
					float	dist2, dist, density;

					VectorSubtract( viewOrigin, xyz, delta );
					dist2 = delta[2] * delta[2]
						  + ( delta[0] * delta[0] + delta[1] * delta[1] );
					dist = sqrtf( dist2 );
					density = 1.0f;
					if ( ( ( dist * dVertex )
						   * ( 0.5f * ( dVertex - c->dView ) + 0.0f ) )
						 / ( dVertex - c->dView ) <= 1.0f ) {
						density = ( ( dist * dVertex )
									* ( 0.5f * ( dVertex - c->dView ) + 0.0f ) )
								  / ( dVertex - c->dView );
					}
					densities[i] = density;
				} else {
					densities[i] = 0.0f;
				}
			}
		}
	}
}

static void RB_CalcAlphaFogDensitiesRun( const rbFogDensity_t *c ) {
	JobPool_ParallelRange( c->numVertexes, MT_VERTEX_CHUNK, RB_CalcAlphaFogDensitiesRange, (void *)c );
}

void RB_CalcAlphaFogDensities( void ) {
	fog_t	*fog;

	if ( tess.fogNum == 9999 ) {
		rbFogDensity_t	c;

		c.densities = tess.fogDensities;
		c.xyz = (const vec4_t *)tess.xyz;
		c.viewOrigin = backEnd.ori.viewOrigin;
		c.numVertexes = tess.numVertexes;
		c.arm = 0;
		c.scale = tr.viewParms.fog.oolen;

		RB_CalcAlphaFogDensitiesRun( &c );
		return;
	}

	fog = &tr.world->fogs[tess.fogNum];

	if ( fog->parms.wave.func ) {
		float	wave = EvalWaveForm( &fog->parms.wave );

		if ( wave < 1.0f ) {
			wave = 1.0f;
		}
		fog->tcScale = 8.0f * ( 1.0f / wave );
	}

	{
		float	tcScale = fog->tcScale;
		float	tVector[3];
		float	planeConst, dView;
		float	*viewOrigin = backEnd.ori.viewOrigin;
		rbFogDensity_t	c;

		tVector[0] = DotProduct( fog->surface, backEnd.ori.axis[0] ) * tcScale;
		tVector[1] = DotProduct( fog->surface, backEnd.ori.axis[1] ) * tcScale;
		tVector[2] = DotProduct( fog->surface, backEnd.ori.axis[2] ) * tcScale;
		planeConst = ( fog->surface[3]
					 - DotProduct( fog->surface, backEnd.ori.origin ) ) * tcScale;
		dView = ( ( viewOrigin[0] * tVector[0] + viewOrigin[1] * tVector[1] )
				  + viewOrigin[2] * tVector[2] ) - planeConst;

		c.densities = tess.fogDensities;
		c.xyz = (const vec4_t *)tess.xyz;
		c.viewOrigin = viewOrigin;
		c.numVertexes = tess.numVertexes;
		c.tVector[0] = tVector[0];
		c.tVector[1] = tVector[1];
		c.tVector[2] = tVector[2];
		c.planeConst = planeConst;
		c.dView = dView;
		c.arm = ( dView >= 0.0f ) ? 1 : 2;
		c.scale = 0.0f;

		RB_CalcAlphaFogDensitiesRun( &c );
	}
}

void RB_CalcAlphaFogBlend( unsigned char *dstColors ) {
	unsigned	colorInt;
	int			i;

	if ( tess.fogNum == 9999 ) {
		/* the walk's own frame copy, not the front end's live struct */
		colorInt = (unsigned)backEnd.viewParms.fogColorInt;
	} else {
		colorInt = tr.world->fogs[tess.fogNum].colorInt;
	}

	for ( i = 0; i < tess.numVertexes; i++ ) {
		Com_Memcpy( dstColors, &colorInt, sizeof( colorInt ) );
		dstColors[3] = (int)( 255.0f * tess.fogDensities[i] );
		dstColors += 4;
	}
}

void RB_CalcModulateColorsByFog( unsigned char *dstColors ) {
	float	texCoords[SHADER_MAX_VERTEXES][2];
	int		i, j;

	RB_CalcFogTexCoords( (float *)texCoords );

	for ( i = 0; i < tess.numVertexes; i++ ) {
		float	factor = -( 256.0f * ( texCoords[i][0]
									 * ( texCoords[i][1] - 0.25f ) ) - 1.0f );

		if ( factor < 0.0f ) {
			factor = 0.0f;
		}

		for ( j = 0; j < 3; j++ ) {
			dstColors[i * 4 + j] = (int)( (float)(int)dstColors[i * 4 + j] * factor );
		}
	}
}

void RB_CalcModulateAlphasByFog( unsigned char *dstColors ) {
	float	texCoords[SHADER_MAX_VERTEXES][2];
	int		i;

	RB_CalcFogTexCoords( (float *)texCoords );

	for ( i = 0; i < tess.numVertexes; i++ ) {
		float	factor = -( 256.0f * ( texCoords[i][0]
									 * ( texCoords[i][1] - 0.25f ) ) - 1.0f );

		if ( factor < 0.0f ) {
			factor = 0.0f;
		}

		dstColors[i * 4 + 3] = (int)( (float)(int)dstColors[i * 4 + 3] * factor );
	}
}

void RB_CalcModulateRGBAsByFog( unsigned char *dstColors ) {
	float	texCoords[SHADER_MAX_VERTEXES][2];
	int		i, j;

	RB_CalcFogTexCoords( (float *)texCoords );

	for ( i = 0; i < tess.numVertexes; i++ ) {
		float	factor = -( 256.0f * ( texCoords[i][0]
									 * ( texCoords[i][1] - 0.25f ) ) - 1.0f );

		if ( factor < 0.0f ) {
			factor = 0.0f;
		}

		for ( j = 0; j < 4; j++ ) {
			dstColors[i * 4 + j] = (int)( (float)(int)dstColors[i * 4 + j] * factor );
		}
	}
}


void RB_CalcModulateAlphasByFog_Alpha( unsigned char *dstColors )
{
	int	i;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		dstColors[i * 4 + 3] = (int)( (float)(int)dstColors[i * 4 + 3]
									   * ( 1.0f - tess.fogDensities[i] ) );
	}
}

void RB_CalcModulateColorsByFog_Alpha( unsigned char *dstColors )
{
	int	i, j;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			dstColors[i * 4 + j] = (int)( (float)(int)dstColors[i * 4 + j]
										  * ( 1.0f - tess.fogDensities[i] ) );
		}
	}
}

void RB_CalcModulateRGBAsByFog_Alpha( unsigned char *dstColors )
{
	int	i, j;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		for ( j = 0; j < 4; j++ ) {
			dstColors[i * 4 + j] = (int)( (float)(int)dstColors[i * 4 + j]
										  * ( 1.0f - tess.fogDensities[i] ) );
		}
	}
}
