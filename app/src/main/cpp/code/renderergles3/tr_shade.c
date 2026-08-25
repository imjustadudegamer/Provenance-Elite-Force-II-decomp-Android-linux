
#include "tr_local.h"
#include "tr_program.h"




shaderCommands_t	tess;


static float	s_debugColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static void gles3WireColor3f( float r, float g, float b ) {
	s_debugColor[0] = r; s_debugColor[1] = g; s_debugColor[2] = b; s_debugColor[3] = 1.0f;
}
static void gles3WireColor4f( float r, float g, float b, float a ) {
	s_debugColor[0] = r; s_debugColor[1] = g; s_debugColor[2] = b; s_debugColor[3] = a;
}
static void gles3WireVertex3fv( const float *v ) {
	GLES3_WireVertex( v, s_debugColor );
}

static void DrawTris (shaderCommands_t *input, int flags) {
	gles3Program_t	*prog;
	vec3_t			start, end;
	int				i;

	if ( flags < 3 ) {
		prog = GLES3_FamilyProgram( GLP_FAMILY_WIRE );
		GLES3_BindProgram( prog );
		if ( flags == 1 ) {
			GLES3_ApplyStateBits( GLS_DEPTHMASK_TRUE );
		} else {
			GLES3_ApplyStateBits( GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE );
		}
		GLES3_SetupViewUniforms( prog );

		GLES3_WireBegin();
		gles3WireColor3f( 1.0f, 1.0f, 1.0f );
		for ( i = 0; i + 2 < input->numIndexes; i += 3 ) {
			const glIndex_t *t = &input->indexes[i];
			gles3WireVertex3fv( input->xyz[t[0]] );
			gles3WireVertex3fv( input->xyz[t[1]] );
			gles3WireVertex3fv( input->xyz[t[1]] );
			gles3WireVertex3fv( input->xyz[t[2]] );
			gles3WireVertex3fv( input->xyz[t[2]] );
			gles3WireVertex3fv( input->xyz[t[0]] );
		}
		GLES3_WireEnd( GL_LINES );
	}

	if (flags > 1) {
        start[1] = start[0] = 0.0;
		end[1] = end[0] = 0.0;

        GLES3_ApplyStateBits( GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE );

        GLES3_WireBegin();

        gles3WireColor4f(1.0, 0.0, 0.0, 1.0);

        start[2] = 0.0 - 5.0;
        end[2] = 0.0 + 5.0;
        gles3WireVertex3fv(start);
        gles3WireVertex3fv(end);

        gles3WireColor4f(0.0, 1.0, 0.0, 1.0);

        start[2] = start[2] + 5.0;
        end[2] = end[2] - 5.0;
        start[1] = start[1] - 5.0;
        end[1] = end[1] + 5.0;
        gles3WireVertex3fv(start);
        gles3WireVertex3fv(end);

        gles3WireColor4f(0.0, 0.0, 1.0, 1.0);

        start[1] = start[1] + 5.0;
        start[0] = start[0] - 5.0;
        end[0] = end[0] + 5.0;
        gles3WireVertex3fv(start);
        gles3WireVertex3fv(end);

        start[0] = start[0] + 5.0;
        end[0] = end[0] - 5.0;

        GLES3_WireEnd( GL_LINES );
	}
}

static void DrawNormals (shaderCommands_t *input, int pass) {
	int		i;
	vec3_t	temp;
	gles3Program_t	*prog;

	prog = GLES3_FamilyProgram( GLP_FAMILY_WIRE );
	GLES3_BindProgram( prog );
	GLES3_SetupViewUniforms( prog );
	GLES3_ApplyStateBits( 0 );

	if ( pass == 0 ) {
		GLES3_ApplyStateBits( GLS_DEPTHTEST_DISABLE );
		GLES3_WireBegin();
		gles3WireColor3f (1,1,1);
		for (i = 0 ; i < input->numVertexes ; i++) {
			gles3WireVertex3fv (input->xyz[i]);
			// 0x218320 + 20 -- not the 2.0 this used to be.
			VectorMA (input->xyz[i], 6, input->normal[i], temp);
			gles3WireVertex3fv (temp);
		}
		GLES3_WireEnd( GL_LINES );
		GLES3_ApplyStateBits( 0 );
	}

	GLES3_WireBegin();
	gles3WireColor3f (1,1,1);
	for (i = 0 ; i < input->numVertexes ; i++) {
		gles3WireVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 6, input->normal[i], temp);
		gles3WireVertex3fv (temp);
	}
	GLES3_WireEnd( GL_LINES );

	// Not transcribed: retail also calls qglLineWidth around both passes and a
}

void RB_BeginSurface( shader_t *shader, int fogNum ) {
	shader_t	*state;

	state = shader->remappedShader ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.dlightBits = 0;		
	tess.vertexColorValid = (r_vertexLight->integer != 0) || (glConfig.hardwareType == 4);
	tess.entityFade = 1.0f;
	tess.shaderTime = 1.0f;

	// ---- fogNum promotion to the global-fog sentinel 9999 (0x270F), 0x3493C-0x34980
	if ( backEnd.viewParms.fog.enabled		 
		&& fogNum >= 0						 
		&& !tess.no_global_fog				 
		&& ( !state->isSky || backEnd.viewParms.fog.extrafrustums )	 
		&& !backEnd.in2D ) {				 
		fogNum = 9999;
	}
	if ( fogNum < 0 ) {						 
		fogNum = 0;
	}
	tess.fogNum = fogNum;					 

	if ( fogNum == 9999
		&& state->usesGlobalFogIterator		 
		&& r_useglfog->integer ) {			 
		tess.xstages = state->unfoggedStages;							 
		tess.numPasses = state->numUnfoggedPasses;						 
		tess.currentStageIteratorFunc = RB_GLFogStageIteratorFunc;		 
		return;
	}
	if ( fogNum == 0 ) {					 
		tess.xstages = state->unfoggedStages;							 
		tess.numPasses = state->numUnfoggedPasses;						 
		tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;	 
		return;
	}
	if ( fogNum == 9999 ) {					 
		tess.xstages = state->globalFogStages;							 
		tess.numPasses = state->numGlobalFogPasses;						 
	} else {								 
		tess.xstages = state->fogStages;								 
		tess.numPasses = state->numFogPasses;							 
	}
	tess.currentStageIteratorFunc = state->isSky ? state->optimalStageIteratorFunc
		: RB_StageIteratorGeneric;			 
}

void RB_GLFogStageIteratorFunc( void ) {
	glState.externalSetState |= GLS_FOG;
	tess.shader->optimalStageIteratorFunc();
	glState.externalSetState &= ~GLS_FOG;
}





static void ProjectDlightTexture( void ) {
	int		i, l;
	vec3_t	origin;
	vec3_t	normal;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	MAC_STATIC float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	unsigned	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	int		planetype;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}


	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	
		}
		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius * 1.75f;
		scale = 1.75f / radius;

		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;
		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	dist;
			float	vertdist;
			int		clip;

			backEnd.pc.c_dlightVertexes++;

			VectorCopy(tess.normal[i], normal);

			if (fabs(normal[0]) > fabs(normal[1])) {
				if (fabs(normal[2]) < fabs(normal[0])) {
					planetype = 0;
				} else {
					planetype = 1;
				}
			} else {
				if (fabs(normal[2]) < fabs(normal[1])) {
					planetype = 1;
				}
				else {
					planetype = 2;
				}
			}

			VectorSubtract( origin, tess.xyz[i], dist );
			if (planetype == 0)
			{
				texCoords[0] = 0.5f + dist[1] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else if (planetype == 1)
			{
				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else
			{
				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[1] * scale;
			}

			vertdist = DotProduct(dist, normal);
			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			if (vertdist > radius) {
				clip |= 16;
				modulate = 0.0f;
			}
			else if (vertdist < -radius) {
				clip |= 32;
				modulate = 0.0f;
			}
			else {
				dist[2] = Q_fabs(dist[2]);
				if (dist[2] < radius * 0.5f) {
					modulate = 1.0f;
				}
				else {
					modulate = 2.0f * (radius - dist[2]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = myftol(floatColor[0] * modulate);
			colors[1] = myftol(floatColor[1] * modulate);
			colors[2] = myftol(floatColor[2] * modulate);
			colors[3] = 255;
		}

		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		{
			gles3Program_t	*dlprog = GLES3_FamilyProgram( GLP_FAMILY_DLIGHT );

			GLES3_BindProgram( dlprog );
			GLES3_SetupViewUniforms( dlprog );
			GLES3_SetUniform1i( dlprog, GLES3_U_TEX0, 0 );
			GLES3_SetUniform1i( dlprog, GLES3_U_ALPHATEST, 0 );

			GLES3_SelectTexture( 0 );
			GL_Bind( tr.dlightImage );

			if ( dl->type & additive) {
				GLES3_ApplyStateBits( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}
			else {
				GLES3_ApplyStateBits( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}
			RB_VBO_DrawTessArrays( &tess, hitIndexes, numIndexes,
								   texCoordsArray[0], NULL, 2,
								   colorArray[0], NULL );
		}
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}

static void ComputeColors( shaderStage_t *pStage )
{
	int		i;

	switch ( pStage->rgbGen )
	{
		case CGEN_IDENTITY:
			Com_Memset( tess.svars.colors, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			Com_Memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity((unsigned char*)tess.svars.colors);
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity((unsigned char*)tess.svars.colors);
			break;
		case CGEN_EXACT_VERTEX:
			// aspect-2dcolor ORACLE CORRECTION (retail, not this file).  Retail's
			Com_Memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_VERTEX:
			// aspect-2dcolor ORACLE CORRECTION (retail, not this file).  Retail case 6 tests
			if (!tess.vertexColorValid) {
				static qboolean bWarned = qfalse;

				if (!bWarned) {
					bWarned = qtrue;

					ri.Printf(
						PRINT_WARNING,
						"Vertex color specified for shader '%s', but vertex colors are not valid for this model\n",
						tess.shader->name);
				}
				break;
			}
			if (tr.identityLight == 1)
			{
				Com_Memcpy(tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof(tess.vertexColors[0]));
			}
			else
			{
				for (i = 0; i < tess.numVertexes; i++)
				{
					tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
					tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
					tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( tr.identityLight == 1 )
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = 255 - tess.vertexColors[i][0];
					tess.svars.colors[i][1] = 255 - tess.vertexColors[i][1];
					tess.svars.colors[i][2] = 255 - tess.vertexColors[i][2];
				}
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = ( 255 - tess.vertexColors[i][0] ) * tr.identityLight;
					tess.svars.colors[i][1] = ( 255 - tess.vertexColors[i][1] ) * tr.identityLight;
					tess.svars.colors[i][2] = ( 255 - tess.vertexColors[i][2] ) * tr.identityLight;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor(&pStage->rgbWave, (unsigned char*)tess.svars.colors, NULL);
			break;
		case CGEN_MULTIPLY_BY_WAVEFORM:
			RB_CalcWaveColor(&pStage->rgbWave, (unsigned char*)tess.svars.colors, pStage->colorConst);
			break;
		case CGEN_LIGHTING_SPHERICAL:
		case CGEN_STATIC:
			if (r_fullbright->integer) {
				break;
			}

			if (!backEnd.currentStaticModel) {
				if (backEnd.currentEntity == &tr.worldEntity) {
					if (tess.vertexColorValid) {
						Com_Memcpy(
							tess.svars.colors, tess.vertexColors,
							tess.numVertexes * sizeof(tess.vertexColors[0]));
					} else {
						byte amb = (byte)(tr.identityLight * 255.0f);

						for (i = 0; i < tess.numVertexes; i++) {
							tess.svars.colors[i][0] = amb;
							tess.svars.colors[i][1] = amb;
							tess.svars.colors[i][2] = amb;
							tess.svars.colors[i][3] = 0xFF;
						}
					}
					break;
				}

				if (backEnd.currentSphere->TessFunction) {
					backEnd.currentSphere->TessFunction((unsigned char*)tess.svars.colors);
				}
				break;
			}

			if (!tess.vertexColorValid) {
				ri.Printf(PRINT_WARNING, "Vertex color specified for shader '%s', but vertex colors are not valid for this model\n", tess.shader->name);
			}

			if (backEnd.currentStaticModel->useSpecialLighting)
			{
				for (i = 0; i < tess.numVertexes; i++)
				{
					int j;
					vec3_t colorout;
					int r, g, b;

					colorout[0] = tess.vertexColors[i][0];
					colorout[1] = tess.vertexColors[i][1];
					colorout[2] = tess.vertexColors[i][2];

					for (j = 0; j < backEnd.currentStaticModel->numdlights; j++)
					{
						float ooLightDistSquared;
						float dot;
						vec3_t diff;
						dlight_t* dl;

						dl = &backEnd.refdef.dlights[backEnd.currentStaticModel->dlights[j].index];
						VectorSubtract(backEnd.currentStaticModel->dlights[j].transformed, tess.xyz[i], diff);

						dot = DotProduct(diff, tess.normal[i]);
						if (dot >= 0)
						{
							float ooLen;

							ooLen = 1.0 / VectorLengthSquared(diff);
							ooLightDistSquared = dot * (7500.0 * dl->radius * ooLen * sqrt(ooLen));
							colorout[0] = dl->color[0] * ooLightDistSquared + colorout[0];
							colorout[1] = dl->color[1] * ooLightDistSquared + colorout[1];
							colorout[2] = dl->color[2] * ooLightDistSquared + colorout[2];
						}
					}

					r = colorout[0];
					g = colorout[1];
					b = colorout[2];

					if (tr.overbrightShift)
					{
						r = (int)((float)r * tr.overbrightMult);
						g = (int)((float)g * tr.overbrightMult);
						b = (int)((float)b * tr.overbrightMult);
					}

					if (r > 0xFF || g > 0xFF || b > 0xFF)
					{
						float t;

						t = 255.0 / (float)Q_max(r, Q_max(g, b));

						r = (int)((float)r * t);
						g = (int)((float)g * t);
						b = (int)((float)b * t);
					}

					tess.svars.colors[i][0] = r;
					tess.svars.colors[i][1] = g;
					tess.svars.colors[i][2] = b;
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			else
			{
				if (tr.identityLight == 1.0)
				{
					Com_Memcpy(tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof(tess.vertexColors[0]));
				}
				else
				{
					for (i = 0; i < tess.numVertexes; i++) {
						tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
						tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
						tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					}
				}
			}
			break;
		case CGEN_CONSTANT:
			RB_CalcColorFromConstant((unsigned char*)tess.svars.colors, pStage->colorConst);
			break;
		case CGEN_GLOBAL_COLOR:
			RB_CalcColorFromConstant((unsigned char*)tess.svars.colors, backEnd.color2D);
			break;
		case CGEN_SCOORD:
			RB_CalcRGBFromTexCoords(
				(unsigned char*)tess.svars.colors,
				pStage->alphaMin,
				pStage->alphaMax,
				pStage->alphaConstMin,
				pStage->alphaConst,
				1.0,
				0.0,
				tess.svars.texcoords[0][0]
			);
			break;
		case CGEN_TCOORD:
			RB_CalcRGBFromTexCoords(
				(unsigned char*)tess.svars.colors,
				pStage->alphaMin,
				pStage->alphaMax,
				pStage->alphaConstMin,
				pStage->alphaConst,
				0.0,
				1.0,
				tess.svars.texcoords[0][0]
			);
			break;
		case CGEN_DOT:
			RB_CalcRGBFromDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
			break;
		case CGEN_ONE_MINUS_DOT:
			RB_CalcRGBFromOneMinusDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
			break;
		case CGEN_FOG:
			{
				unsigned	colorInt = tr.world->fogs[tess.fogNum].colorInt;

				for ( i = 0; i < tess.numVertexes; i++ ) {
					Com_Memcpy( tess.svars.colors[i], &colorInt, sizeof( colorInt ) );
				}
			}
			break;
		case CGEN_FOG_BLEND:
			RB_CalcAlphaFogBlend( (unsigned char*)tess.svars.colors );
			break;
	}

	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( pStage->rgbGen != CGEN_IDENTITY
			&& pStage->rgbGen != CGEN_LIGHTING_SPHERICAL)
		{
			if ( pStage->rgbGen != CGEN_VERTEX || tr.identityLight != 1 ) {
				for ( i = 0; i < tess.numVertexes; i++ ) {
					tess.svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity((unsigned char*)tess.svars.colors);
		break;
	case AGEN_FORCED_ALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, (int)(255.0f * tess.entityFade));
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity((unsigned char*)tess.svars.colors);
		break;
	case AGEN_VERTEX:
		if (pStage->rgbGen != CGEN_VERTEX) {
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for (i = 0; i < tess.numVertexes; i++)
		{
			tess.svars.colors[i][3] = 255 - tess.vertexColors[i][3];
		}
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha((unsigned char*)tess.svars.colors, pStage->alphaMax, pStage->specOrigin);
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha(&pStage->alphaWave, (unsigned char*)tess.svars.colors);
		break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.ori.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_DOT:
		RB_CalcAlphaFromDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
		return;
	case AGEN_ONE_MINUS_DOT:
		RB_CalcAlphaFromOneMinusDot((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
		return;
	case AGEN_CONSTANT:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, pStage->colorConst[3]);
		break;
	case AGEN_GLOBAL_ALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, backEnd.color2D[3]);
		break;
	case AGEN_SKYALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, tr.refdef.sky_alpha * 255.0);
		break;
	case AGEN_ONE_MINUS_SKYALPHA:
		RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, (1.0 - tr.refdef.sky_alpha) * 255.0);
		break;
	case AGEN_SCOORD:
		RB_CalcAlphaFromTexCoords(
			(unsigned char*)tess.svars.colors,
			pStage->alphaMin,
			pStage->alphaMax,
			pStage->alphaConstMin,
			pStage->alphaConst,
			1.0,
			0.0,
			tess.svars.texcoords[0][0]
		);
		break;
	case AGEN_TCOORD:
		RB_CalcAlphaFromTexCoords(
			(unsigned char*)tess.svars.colors,
			pStage->alphaMin,
			pStage->alphaMax,
			pStage->alphaConstMin,
			pStage->alphaConst,
			0.0,
			1.0,
			tess.svars.texcoords[0][0]
		);
		break;
	case AGEN_DIST_FADE:
		if (backEnd.currentStaticModel)
		{
			unsigned char alpha;

			for (i = 0; i < tess.numVertexes; i++) {
				float len;
				vec3_t org, v;

				VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentStaticModel->origin, v);
				org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentStaticModel->axis[0]);
				org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentStaticModel->axis[1]);
				org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentStaticModel->axis[2]);
			
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0xff;
				} else if (len > 1) {
					alpha = 0;
				} else {
					alpha = ((1.0 - len) * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		else
        {
            unsigned char alpha;

            for (i = 0; i < tess.numVertexes; i++) {
                float len;
                vec3_t org, v;

				if (backEnd.currentEntity) {
					VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentEntity->e.origin, v);
					org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentEntity->e.axis[0]);
					org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentEntity->e.axis[1]);
					org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentEntity->e.axis[2]);
				} else {
					VectorCopy(backEnd.viewParms.ori.origin, org);
				}

				
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0xff;
				} else if (len > 1) {
					alpha = 0;
				} else {
					alpha = ((1.0 - len) * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_ONE_MINUS_DIST_FADE:
		if (backEnd.currentStaticModel)
		{
			unsigned char alpha;

			for (i = 0; i < tess.numVertexes; i++) {
				float len;
				vec3_t org, v;

                VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentStaticModel->origin, v);
                org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentStaticModel->axis[0]);
                org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentStaticModel->axis[1]);
                org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentStaticModel->axis[2]);
			
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0;
				} else if (len > 1) {
					alpha = 0xff;
				} else {
					alpha = (len * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		else
        {
            unsigned char alpha;

            for (i = 0; i < tess.numVertexes; i++) {
                float len;
                vec3_t org, v;

				if (backEnd.currentEntity) {
                    VectorSubtract(backEnd.viewParms.ori.origin, backEnd.currentEntity->e.origin, v);
                    org[0] = tess.xyz[i][0] - DotProduct(v, backEnd.currentEntity->e.axis[0]);
                    org[1] = tess.xyz[i][1] - DotProduct(v, backEnd.currentEntity->e.axis[1]);
                    org[2] = tess.xyz[i][2] - DotProduct(v, backEnd.currentEntity->e.axis[2]);
				} else {
					VectorCopy(backEnd.viewParms.ori.origin, org);
				}

				
				len = (VectorLength(org) - tess.shader->fDistNear) / tess.shader->fDistRange;
				if (len < 0) {
					alpha = 0;
				} else if (len > 1) {
					alpha = 0xff;
				} else {
					alpha = (len * 255.0);
                }
                tess.svars.colors[i][3] = alpha;
			}
		}
		break;
    case AGEN_TIKI_DIST_FADE:
    case AGEN_ONE_MINUS_TIKI_DIST_FADE:
		{
			unsigned char alpha;

			if (backEnd.currentStaticModel)
			{
				float lenSqr;
				float fDistNearSqr;
				float fDistFarSqr;
				vec3_t org;

				VectorSubtract(backEnd.currentStaticModel->origin, backEnd.viewParms.ori.origin, org);

				lenSqr = VectorLengthSquared(org);
                fDistNearSqr = tess.shader->fDistNear * tess.shader->fDistNear;
                fDistFarSqr = (tess.shader->fDistNear + tess.shader->fDistRange) * (tess.shader->fDistNear + tess.shader->fDistRange);
				if (lenSqr <= fDistNearSqr) {
					alpha = 0;
				} else if (lenSqr >= fDistFarSqr) {
					alpha = 0xff;
				} else {
					float len;

					len = VectorLength(org);
					alpha = (((len - tess.shader->fDistNear) / tess.shader->fDistRange) * 255.0);
				}

				if (pStage->alphaGen == AGEN_TIKI_DIST_FADE) {
					alpha = 0xff - alpha;
				}
			}
			else if (backEnd.currentEntity)
			{
				float lenSqr;
				float fDistNearSqr;
				float fDistFarSqr;
				vec3_t org;

				VectorSubtract(backEnd.currentEntity->e.origin, backEnd.viewParms.ori.origin, org);

				lenSqr = VectorLengthSquared(org);
				fDistNearSqr = tess.shader->fDistNear * tess.shader->fDistNear;
				fDistFarSqr = (tess.shader->fDistNear + tess.shader->fDistRange) * (tess.shader->fDistNear + tess.shader->fDistRange);
				if (lenSqr <= fDistNearSqr) {
					alpha = 0;
				} else if (lenSqr >= fDistFarSqr) {
					alpha = 0xff;
				} else {
					float len;

                    len = VectorLength(org);
                    alpha = (((len - tess.shader->fDistNear) / tess.shader->fDistRange) * 255.0);
				}

				if (pStage->alphaGen == AGEN_TIKI_DIST_FADE) {
					alpha = 0xff - alpha;
				}
			}
			else
			{
				const char* type;
				if (pStage->alphaGen == AGEN_TIKI_DIST_FADE) {
					type = "tikiDistFade";
				} else {
					type = "oneMinusTikiDistFade";
				}

				ri.Error(ERR_DROP, "ERROR: '%s' shader command used on a non-tiki\n", type);
				alpha = 0xff;
			}
			RB_CalcAlphaFromConstant((unsigned char*)tess.svars.colors, alpha);
		}
		break;
	case AGEN_DOT_VIEW:
		RB_CalcAlphaFromDotView((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
		break;
	case AGEN_ONE_MINUS_DOT_VIEW:
		RB_CalcAlphaFromOneMinusDotView((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
        break;
    case AGEN_HEIGHT_FADE:
        RB_CalcAlphaFromHeightFade((unsigned char*)tess.svars.colors, pStage->alphaMin, pStage->alphaMax);
        break;
	default:
		break;
	}

	// Retail never writes that selector, so the ramp arm is the one retail
	switch ( pStage->acff )
	{
	case ACFF_NONE:
		break;
	case ACFF_MODULATE_RGB:
		if ( pStage->fogDensityArm ) {
			RB_CalcModulateColorsByFog_Alpha( (unsigned char*)tess.svars.colors );
		} else {
			RB_CalcModulateColorsByFog( (unsigned char*)tess.svars.colors );
		}
		break;
	case ACFF_MODULATE_RGBA:
		if ( pStage->fogDensityArm ) {
			RB_CalcModulateRGBAsByFog_Alpha( (unsigned char*)tess.svars.colors );
		} else {
			RB_CalcModulateRGBAsByFog( (unsigned char*)tess.svars.colors );
		}
		break;
	case ACFF_MODULATE_ALPHA:
		if ( pStage->fogDensityArm ) {
			RB_CalcModulateAlphasByFog_Alpha( (unsigned char*)tess.svars.colors );
		} else {
			RB_CalcModulateAlphasByFog( (unsigned char*)tess.svars.colors );
		}
		break;
	}
}

static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;

	for ( b = 0; b < NUM_TEXTURE_BUNDLES && pStage->bundle[b].image[0]; b++ ) {
		int tm;

		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			Com_Memset( tess.svars.texcoords[b], 0, sizeof( float ) * 2 * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][0][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][0][1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][1][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][1][1];
			}
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			RB_CalcEnvironmentTexCoords( ( float * ) tess.svars.texcoords[b] );
            break;
		case TCGEN_VECTOR:
			for (i = 0; i < tess.numVertexes; i++) {
				tess.svars.texcoords[b][i][0] = DotProduct(tess.xyz[i], pStage->bundle[b].tcGenVectors[0]);
				tess.svars.texcoords[b][i][1] = DotProduct(tess.xyz[i], pStage->bundle[b].tcGenVectors[1]);
			}
			break;
        case TCGEN_ENVIRONMENT_MAPPED2:
            RB_CalcEnvironmentTexCoords2((float*)tess.svars.texcoords[b]);
            break;
        case TCGEN_ANGLE_BASED_ENVIRONMENT:
            RB_CalcAngleBasedEnvironmentTexCoords((float*)tess.svars.texcoords[b]);
            break;
		case TCGEN_SUN_REFLECTION:
			RB_CalcSunReflectionTexCoords(tess.svars.texcoords[b][0]);
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords( (float *)tess.svars.texcoords[b] );
			break;
        default:
            ri.Printf(
				PRINT_DEVELOPER,
                "WARNING: invalid tcGen '%d' specified for shader '%s'\n",
                pStage->bundle[b].tcGen,
                tess.shader->name
			);
			break;
		}

		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
									 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].scroll,
										 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].scale,
									 ( float * ) tess.svars.texcoords[b] );
				break;
			
			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						               ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].rotateSpeed,
										pStage->bundle[b].texMods[tm].rotateCoef,
										( float * ) tess.svars.texcoords[b],
										pStage->bundle[b].texMods[tm].rotateStart );
				break;

			case TMOD_OFFSET:
				RB_CalcOffsetTexCoords(pStage->bundle[b].texMods[tm].scroll, tess.svars.texcoords[b][0]);
				break;

            case TMOD_PARALLAX:
				RB_CalcParallaxTexCoords(pStage->bundle[b].texMods[tm].rate, tess.svars.texcoords[b][0]);
                break;

            case TMOD_MACRO:
				RB_CalcMacroTexCoords(pStage->bundle[b].texMods[tm].scale, tess.svars.texcoords[b][0]);
                break;

            case TMOD_WAVETRANS:
				RB_CalcTransWaveTexCoords(&pStage->bundle[b].texMods[tm].wave, tess.svars.texcoords[b][0]);
                break;

            case TMOD_WAVETRANT:
				RB_CalcTransWaveTexCoordsT(&pStage->bundle[b].texMods[tm].wave, tess.svars.texcoords[b][0]);
                break;

            case TMOD_BULGETRANS:
				RB_CalcBulgeTexCoords(&pStage->bundle[b].texMods[tm].wave, tess.svars.texcoords[b][0]);
                break;

			default:
				ri.Printf(PRINT_WARNING, "WARNING: invalid tcMod '%d' specified for shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	int stage;

	for (stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t* pStage = tess.xstages[stage];

		if (!pStage)
		{
			break;
		}

		if (pStage->alphaGen == AGEN_SKYALPHA && tr.refdef.sky_alpha < 0.01) {
			continue;
		}
		else if (pStage->alphaGen == AGEN_ONE_MINUS_SKYALPHA && tr.refdef.sky_alpha > 0.99) {
			continue;
		}

		if ( GLES3_StageNeedsCPUColor( pStage ) ) {
			ComputeColors( pStage );
		}
		if ( GLES3_StageNeedsCPUTexCoords( pStage ) ) {
			ComputeTexCoords( pStage );
		}

		unsigned stateBits = pStage->stateBits;
		float forceAlpha = -1.0f;
		if (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FORCE_ALPHA)
			&& backEnd.currentEntity->e.shaderRGBA[3] < 255)
		{
			int   a = backEnd.currentEntity->e.shaderRGBA[3];
			forceAlpha = a / 255.0f;

			if ( GLES3_StageNeedsCPUColor( pStage ) ) {
				int   v;
				for (v = 0; v < input->numVertexes; v++) {
					input->svars.colors[v][3] = (byte)a;
				}
			}
		}
		stateBits = RF_ForceAlphaStateBits(stateBits);

		if (backEnd.in2D) {
			GLES3_ApplyStateBits(stateBits | GLS_DEPTHTEST_DISABLE);
		}
		else {
			GLES3_ApplyStateBits(stateBits);
		}

		GLES3_DrawStage( input, pStage, tess.shader, stateBits, forceAlpha, qtrue );   
		if (r_lightmap->integer && (pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap))
		{
			break;
		}
	}
}


void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;

	input = &tess;

	input->shaderTime = backEnd.refdef.floatTime;
	if ((input->shader->flags & BUNDLE_ANIMATE_ONCE) != 0) {
		input->shaderTime = backEnd.refdef.floatTime - backEnd.shaderStartTime;
	}

	RB_DeformTessGeometry();

	if ( tess.fogNum ) {
		if ( tess.fogNum == 9999 ) {
			if ( !( tess.shader->fogFlags & 4 ) ) {
				goto noFogDensities;
			}
		} else if ( !( tess.shader->fogFlags & 2 ) ) {
			goto noFogDensities;
		}
		RB_CalcAlphaFogDensities();
	}
noFogDensities:

	if ( r_logFile->integer ) 
	{
		GLimp_LogComment( (char *)va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	GL_Cull( input->shader->cullType );

	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	RB_IterateStagesGeneric( input );

	if (tess.dlightBits && !tess.dlightMap && tess.shader->sort <= SS_OPAQUE) {
		ProjectDlightTexture();
	}

	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}


void RB_StageIteratorVertexLitTextureUnfogged( void )
{
	shaderCommands_t *input;
	shader_t		*shader;

	input = &tess;

	shader = input->shader;
	(void)shader;

	// retail code but its callers are RB_IterateStagesGeneric and friends, never this
	if (backEnd.currentSphere->TessFunction)
	{
		backEnd.currentSphere->TessFunction((unsigned char*)tess.svars.colors);
	}
	else
	{
		RB_CalcDiffuseColor((unsigned char*)tess.svars.colors);
	}

	if (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FORCE_ALPHA)
		&& backEnd.currentEntity->e.shaderRGBA[3] < 255)
	{
		int a = backEnd.currentEntity->e.shaderRGBA[3];
		int v;
		for (v = 0; v < input->numVertexes; v++) {
			tess.svars.colors[v][3] = (byte)a;
		}
	}

	if ( r_logFile->integer )
	{
		GLimp_LogComment( (char *)va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", tess.shader->name) );
	}

	GL_Cull( input->shader->cullType );

	{
		unsigned	stateBits = RF_ForceAlphaStateBits( tess.xstages[0]->stateBits );

		GLES3_ApplyStateBits( stateBits );
		GLES3_DrawStage( input, tess.xstages[0], tess.shader, stateBits, -1.0f, qfalse );  
	}

	if ( tess.dlightBits && !tess.dlightMap && tess.shader->sort <= SS_OPAQUE ) {
		ProjectDlightTexture();
	}
}


void RB_StageIteratorLightmappedMultitextureUnfogged( void ) {
	shaderCommands_t *input;

	input = &tess;

	if ( r_logFile->integer ) {
		GLimp_LogComment( (char *)va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", tess.shader->name) );
	}

	GL_Cull( input->shader->cullType );

	{
		gles3Program_t	*lmprog;

		GLES3_ApplyStateBits( GLS_DEPTHMASK_TRUE );

		lmprog = GLES3_StageProgram( tess.xstages[0], tess.shader, GLP_LIGHTMAP_MT );
		GLES3_BindProgram( lmprog );
		GLES3_SetupStageUniforms( lmprog, tess.xstages[0], tess.shader );
		GLES3_SetupFogUniforms( lmprog, tess.fogNum, tess.xstages[0]->fogDensityArm );
		GLES3_SetUniform1i( lmprog, GLES3_U_COLORGEN, CGEN_EXACT_VERTEX );
		GLES3_SetUniform1i( lmprog, GLES3_U_ALPHAGEN, AGEN_SKIP );
		GLES3_SetUniform1i( lmprog, GLES3_U_ACFF, 0 );
		if ( r_lightmap->integer ) {
			GLES3_SetUniform1i( lmprog, GLES3_U_TEXENV1, 3 );	
		}
		GLES3_SetUniform1f( lmprog, GLES3_U_FORCEALPHA, -1.0f );

		GLES3_BindAnimatedImage( &tess.xstages[0]->bundle[0], 0 );
		GLES3_BindAnimatedImage( &tess.xstages[0]->bundle[1], 1 );

		RB_VBO_DrawTessArrays( input, input->indexes, input->numIndexes,
							   input->texCoords[0][0], input->texCoords[0][0] + 2, 4,
							   (const unsigned char *)tess.constantColor255, input->normal[0] );
	}

	if ( tess.dlightBits && !tess.dlightMap && tess.shader->sort <= SS_OPAQUE ) {
		ProjectDlightTexture();
	}

}

void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}
	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}	
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	if (tess.xyz[SHADER_MAX_VERTEXES - 1][0] && r_shadows->integer != 3) {
		tess.xyz[SHADER_MAX_VERTEXES - 1][0] = 0;
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	BE_Bump( BE_FAM_DS, BE_DSC_batches, 1 );
	BE_Bump( BE_FAM_DS, BE_DSC_verts, tess.numVertexes );

	BE_Bump( BE_FAM_DS, BE_DSC_stages, 1 );
	BE_SUB( BE_FAM_DS, BE_DSZ_sub_stage, tess.currentStageIteratorFunc() );

	if (!(tr.refdef.rdflags & RDF_NOWORLDMODEL) && !backEnd.in2D)
	{
		if (r_showtris->integer && r_developer->integer) {
			DrawTris(input, r_showtris->integer);
		}
		if (r_shownormals->integer && r_developer->integer) {
			DrawNormals(input, r_shownormals->integer);
		}
	}

	tess.numIndexes = 0;

	GLimp_LogComment( "----------\n" );
}

