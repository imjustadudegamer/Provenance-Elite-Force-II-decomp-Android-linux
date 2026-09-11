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

#define GLES3_HAS_MULTITEXTURE qtrue



typedef struct shadertext_s {
    char name[64];
    char* text;
    shader_t* shader;
    struct shadertext_s* next;
} shadertext_t;

static char *s_shaderText;

static	shaderStage_t	unfoggedStages[MAX_SHADER_STAGES];		
static	shaderStage_t	fogStages[MAX_SHADER_STAGES];
static	shaderStage_t	globalFogStages[MAX_SHADER_STAGES];
static	shader_t		shader;
static	texModInfo_t	texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];

#define FILE_HASH_SIZE		8192

static qboolean shader_noPicMip;
static qboolean shader_noMipMaps;
static qboolean shader_force32bit;
static shadertext_t* currentShader = NULL;
static shadertext_t* hashTable[FILE_HASH_SIZE];

#define GL_CLAMP_TO_EDGE 0x812F

static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				
		if (letter =='\\') letter = '/';		
		if (letter == PATH_SEP) letter = '/';		
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

static shadertext_t* AddShaderTextToHash(const char* name, int hash)
{
	shadertext_t* shaderText;

    shaderText = (shadertext_t*)ri.Malloc(sizeof(shadertext_t));

    Com_Memset(shaderText, 0, sizeof(shadertext_t));
    strncpy(shaderText->name, name, sizeof(shaderText->name));
    shaderText->next = hashTable[hash];

    hashTable[hash] = shaderText;
    return shaderText;
}

static shadertext_t* AllocShaderText(const char* name)
{
    int hash;

    hash = generateHashValue(name);
    return AddShaderTextToHash(name, hash);
}

static shadertext_t* FindShaderText(const char* name)
{
    shadertext_t* shaderText;
    int hash;

    hash = generateHashValue(name);
    shaderText = hashTable[hash];
    if (!shaderText) {
        return AddShaderTextToHash(name, hash);
    }

    while (Q_stricmp(shaderText->name, name))
    {
        shaderText = shaderText->next;
        if (!shaderText) {
            return AddShaderTextToHash(name, hash);
        }
    }

    return shaderText;
}

void R_RemapShader(const char *shaderName, const char *newShaderName) {
	char			strippedName[MAX_QPATH];
	int				hash;
	shader_t		*sh, *sh2;
	shadertext_t	*sht;
	qhandle_t		h;

	sh = R_FindShaderByName( shaderName );
	if (sh == NULL || sh == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(shaderName, 0);
		sh = R_GetShaderByHandle(h);
	}
	if (sh == NULL || sh == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: R_RemapShader: shader %s not found\n", shaderName );
		return;
	}

	sh2 = R_FindShaderByName( newShaderName );
	if (sh2 == NULL || sh2 == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(newShaderName, 0);
		sh2 = R_GetShaderByHandle(h);
	}

	if (sh2 == NULL || sh2 == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: R_RemapShader: new shader %s not found\n", newShaderName );
		return;
	}

	COM_StripExtension(shaderName, strippedName, sizeof(strippedName));
	hash = generateHashValue(strippedName);
	for (sht = hashTable[hash]; sht; sht = sht->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			if (sht->shader != sh2) {
				sht->shader->remappedShader = sh2;
			} else {
				sht->shader->remappedShader = NULL;
			}
		}
	}
}

static qboolean ParseVector( char **text, int count, float *v ) {
	char	*token;
	int		i;

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = atof( token );
	}

	return qtrue;
}

void InitStaticShaders(void)
{
	char* text;
	const char* token;
	char* buf;
	char shadername[64];

    if (ri.FS_ReadFile("shaders/static_shaders.txt", (void**)&buf) == -1)
    {
        ri.Printf(3, "Couldn't find static shaders file: shaders/static_shaders.txt\n");
		return;
    }

    text = buf;
    while (1)
    {
        token = COM_ParseExt(&text, 1);
		if (!*token) {
			break;
		}

        strncpy(shadername, token, 0x40u);
		if (!R_FindShader(shadername, -1, 1, r_picmip->integer, 1, 1)) {
			ri.Printf(3, "InitStaticShaders: Couldn't find shader: %s\n", shadername);
		}
    }

    ri.FS_FreeFile(buf);
}

static unsigned NameToAFunc( const char *funcname )
{	
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		return GLS_ATEST_GT_0;
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return GLS_ATEST_LT_80;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return GLS_ATEST_GE_80;
	}


	ri.Printf( PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return 0;
}


static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_DSTBLEND_ONE;
}

static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return GF_NOISE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return GF_SIN;
}


static void ParseWaveForm( char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing BASE waveform parm in shader '%s'\n", shader.name );
		return;
	}
	if ( !Q_stricmp( token, "fromEntity" ) ) {
		wave->base = 1234567;
	} else {
		wave->base = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing AMPLITUDE waveform parm in shader '%s'\n", shader.name );
		return;
	}
	if ( !Q_stricmp( token, "fromEntity" ) ) {
		wave->amplitude = 1234567;
	} else {
		wave->amplitude = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing PHASE waveform parm in shader '%s'\n", shader.name );
		return;
	}
	if ( !Q_stricmp( token, "fromEntity" ) ) {
		wave->phase = 1234567;
	} else if ( !Q_stricmp( token, "random" ) ) {
		wave->amplitude = ( rand() & 0x7fff ) / 32767.0f;
	} else {
		wave->phase = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing FREQUENCY waveform parm in shader '%s'\n", shader.name );
		return;
	}
	if ( !Q_stricmp( token, "fromEntity" ) ) {
		wave->frequency = 1234567;
	} else {
		wave->frequency = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] && !Q_stricmp( token, "wind" ) ) {
		wave->wind = 1;
	} else {
		wave->wind = 0;
	}
}


static void ParseTexMod( char *_text, shaderStage_t *stage, int cntBundle )
{
	const char *token;
	char **text = &_text;
	texModInfo_t *tmi;

	if ( stage->bundle[cntBundle].numTexMods == TR_MAX_TEXMODS ) {
		ri.Error( ERR_DROP, "ERROR: too many tcMod unfoggedStages in shader '%s'\n", shader.name );
		return;
	}

	tmi = &stage->bundle[cntBundle].texMods[stage->bundle[cntBundle].numTexMods];
	stage->bundle[cntBundle].numTexMods++;

	token = COM_ParseExt( text, qfalse );

	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = atof( token );

		tmi->type = TMOD_TURBULENT;

		shader.flags |= 2;
	}
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = atof( token );
		tmi->type = TMOD_SCALE;
	}
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		if (!Q_stricmp(token, "fromEntity")) {
			tmi->scroll[0] = 1234567;
		} else {
			tmi->scroll[0] = atof(token);
		}

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		if (!Q_stricmp(token, "fromEntity")) {
			tmi->scroll[1] = 1234567;
		} else {
			tmi->scroll[1] = atof(token);
		}

        token = COM_ParseExt(text, qfalse);
		if (token[0]) {
			if (atof(token) < 0) {
				tmi->scroll[0] -= (rand() % (int)(atof(token) * 1000.0)) / 1000.0;
			} else if (atof(token) > 0) {
				tmi->scroll[0] += (rand() % (int)(atof(token) * 1000.0)) / 1000.0;
			}
		}

        token = COM_ParseExt(text, qfalse);
		if (token[0]) {
			if (atof(token) < 0) {
				tmi->scroll[1] -= (rand() % (int)(atof(token) * 1000.0)) / 1000.0;
			} else if (atof(token) > 0) {
				tmi->scroll[1] += (rand() % (int)(atof(token) * 1000.0)) / 1000.0;
			}
		}

		tmi->type = TMOD_SCROLL;

		shader.flags |= 2;
	}
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		ParseWaveForm(text, &tmi->wave);
		tmi->type = TMOD_STRETCH;
		shader.flags |= 2;
	}
	else if ( !Q_stricmp( token, "wavetrans" ) )
    {
		ParseWaveForm(text, &tmi->wave);
		tmi->type = TMOD_WAVETRANS;
		shader.flags |= 2;
	}
	else if ( !Q_stricmp( token, "wavetrant" ) )
    {
        ParseWaveForm(text, &tmi->wave);
        tmi->type = TMOD_WAVETRANT;
        shader.flags |= 2;
	}
	else if ( !Q_stricmp( token, "bulge" ) )
	{
		token = COM_ParseExt(text, qfalse);
		if (token[0] == 0) {
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod bulge parms in shader '%s'\n", shader.name);
			return;
		}

        tmi->wave.base = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod bulge parms in shader '%s'\n", shader.name);
            return;
        }

		tmi->wave.amplitude = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod bulge parms in shader '%s'\n", shader.name);
            return;
        }

		tmi->wave.frequency = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing tcMod bulge parms in shader '%s'\n", shader.name);
            return;
        }

		tmi->wave.phase = atof(token);

		tmi->type = TMOD_BULGETRANS;
		shader.flags |= 2;
	}
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}

		if (!Q_stricmp(token, "fromEntity")) {
			tmi->rotateSpeed = 1234567;
		} else {
			tmi->rotateSpeed = atof(token);
		}

        token = COM_ParseExt(text, qfalse);
		if (token[0]) {
			if (!Q_stricmp(token, "fromEntity")) {
				tmi->rotateStart = 1234567;
			} else {
				tmi->rotateStart = atof(token);
			}
		} else {
			tmi->rotateStart = 0;
        }

        token = COM_ParseExt(text, qfalse);
		if (token[0]) {
			tmi->rotateCoef = atof(token);
		} else {
			tmi->rotateCoef = 1;
		}

		tmi->type = TMOD_ROTATE;
		shader.flags |= 2;
	}
    else if (!Q_stricmp(token, "offset")) {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing offset parms in shader '%s'\n", shader.name);
            return;
		}

		if (!Q_stricmp(token, "fromEntity")) {
			tmi->scroll[0] = 1234567;
		} else {
			tmi->scroll[0] = atof(token);
		}

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing offset parms in shader '%s'\n", shader.name);
            return;
		}

		if (!Q_stricmp(token, "fromEntity")) {
			tmi->scroll[1] = 1234567;
		} else {
			tmi->scroll[1] = atof(token);
        }

        token = COM_ParseExt(text, qfalse);
		if (token[0]) {
			if (atof(token) < 0) {
				tmi->scroll[0] -= (rand() % (int)(atof(token) * 1000.0 + 1.0)) / 1000.0;
			} else if (atof(token) > 0) {
				tmi->scroll[0] += (rand() % (int)(atof(token) * 1000.0 + 1.0)) / 1000.0;
			}
		}

        token = COM_ParseExt(text, qfalse);
		if (token[0]) {
			if (atof(token) < 0) {
				tmi->scroll[1] -= (rand() % (int)(atof(token) * 1000.0 + 1.0)) / 1000.0;
			} else if (atof(token) > 0) {
				tmi->scroll[1] += (rand() % (int)(atof(token) * 1000.0 + 1.0)) / 1000.0;
			}
		}

		tmi->type = TMOD_OFFSET;
	}
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else if ( !Q_stricmp( token, "parallax" ) )
    {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing rate parms in shader '%s'\n", shader.name);
            return;
		}

		tmi->rate[0] = atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing rate parms in shader '%s'\n", shader.name);
            return;
		}

		tmi->rate[1] = atof(token);
		tmi->type = TMOD_PARALLAX;
	}
	else if ( !Q_stricmp( token, "macro" ) )
    {
        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s' for macro definition\n", shader.name);
            return;
        }

		tmi->scale[0] = 1.0 / atof(token);

        token = COM_ParseExt(text, qfalse);
        if (token[0] == 0) {
            ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s' for macro definition\n", shader.name);
            return;
        }

		tmi->scale[1] = 1.0 / atof(token);

		tmi->type = TMOD_MACRO;
		shader.needsNormal = qtrue;
	}
	else
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name);
	}
}


static qboolean ParseStage(shaderStage_t* stage, char** text, qboolean picmip)
{
	char* token;
	int depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	qboolean depthMaskExplicit = qfalse;
	int cntBundle = 0;
	int depthTestBits = 0;
	int fogBits = 0;
	qboolean shouldProcess = qtrue;

	stage->active = qtrue;
	stage->noMipMaps = shader_noMipMaps;
	stage->noPicMip = shader_noPicMip;
	stage->force32bit = shader_force32bit;

	while (1)
	{
		token = COM_ParseExt(text, qtrue);
		if (!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: no matching '}' found\n");
			return qfalse;
		}

		if (token[0] == '}')
		{
			break;
		}
		else if (!Q_stricmp(token, "nomipmaps"))
		{
			stage->noMipMaps = qtrue;
			continue;
		}
		else if (!Q_stricmp(token, "nopicmip"))
		{
			stage->noPicMip = qtrue;
			continue;
		}
		else if (!Q_stricmp(token, "nofog"))
		{
			fogBits = GLS_FOG;
			continue;
		}
		else if (!Q_stricmp(token, "map"))
		{
			token = COM_ParseExt(text, qfalse);
			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}

			if (!Q_stricmp(token, "$whiteimage"))
			{
				stage->bundle[cntBundle].image[0] = tr.whiteImage;
				continue;
			}
			else if (!Q_stricmp(token, "$lightmap"))
			{
				stage->bundle[cntBundle].isLightmap = qtrue;
				if (shader.lightmapIndex < 0) {
					stage->bundle[cntBundle].image[0] = tr.whiteImage;
				}
				else if (shader.lightmapIndex & DYNAMIC_LIGHTMAP_BIT) {
					stage->bundle[cntBundle].image[0] =
						tr.baseLightmaps[shader.lightmapIndex & LIGHTMAP_BASE_MASK];
				}
				else {
					stage->bundle[cntBundle].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			}
			else if (!Q_stricmp(token, "$bumpmap"))
            {
                token = COM_ParseExt(text, qfalse);
                if (!token[0])
                {
                    ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'bumpmap' keyword in shader '%s'\n", shader.name);
                    return qfalse;
                }

			}
			else
			{
				stage->bundle[cntBundle].image[0] = R_FindImageFileOld(token, !stage->noMipMaps, stage->noPicMip ? qfalse : picmip, stage->force32bit, 2);
				if (!stage->bundle[cntBundle].image[0])
				{
					ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
					return qfalse;
				}
			}
		}
		else if (!Q_stricmp(token, "clampmap")
			  || !Q_stricmp(token, "clampmaps")
			  || !Q_stricmp(token, "clampmapt"))
		{
			int textureMode;

			if (!Q_stricmp(token, "clampmaps")) {
				textureMode = 3;
			} else if (!Q_stricmp(token, "clampmapt")) {
				textureMode = 4;
			} else {
				textureMode = 1;
			}

			token = COM_ParseExt(text, qfalse);
			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name);
				return qfalse;
			}

			stage->bundle[cntBundle].image[0] = R_FindImageFileOld(token, !stage->noMipMaps, (!stage->noPicMip ? picmip : 0), stage->force32bit, textureMode);
			if (!stage->bundle[cntBundle].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
				return qfalse;
			}
		}
		else if ( !Q_stricmp( token, "animMap" ) || !Q_stricmp(token, "animMapOnce") || !Q_stricmp(token, "animMapPhase"))
		{
			qboolean phased;

			shader.flags |= 3;

			if (!Q_stricmp(token, "animMapOnce")) {
				stage->bundle[cntBundle].flags |= BUNDLE_ANIMATE_ONCE;
			}

			phased = !Q_stricmp(token, "animMapPhase");

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle[cntBundle].imageAnimationSpeed = atof( token );
			stage->bundle[cntBundle].imageAnimationPhase = 0;

			if (phased)
            {
                token = COM_ParseExt(text, qfalse);
                if (!token[0])
                {
                    ri.Printf(PRINT_WARNING, "WARNING: missing phase for 'animMapPhase' keyword in shader '%s'\n", shader.name);
                    return qfalse;
                }

				stage->bundle[cntBundle].imageAnimationPhase = atof(token);
			}
			while ( 1 ) {
				int		num;

				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				num = stage->bundle[cntBundle].numImageAnimations;
				if ( num < MAX_IMAGE_ANIMATIONS ) {
                    stage->bundle[cntBundle].image[num] = R_FindImageFileOld(token, !stage->noMipMaps, (!stage->noPicMip ? picmip : 0), stage->force32bit, 2 );
					if ( !stage->bundle[cntBundle].image[num] )
					{
						ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
						return qfalse;
					}
					stage->bundle[cntBundle].numImageAnimations++;
				}
			}
		}
		else if (!Q_stricmp(token, "normalmap"))
        {

            token = COM_ParseExt(text, qfalse);
            if (!token[0])
            {
                ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'normalmap' keyword in shader '%s'\n", shader.name);
                return qfalse;
            }

			stage->normalMap = R_FindImageFileOld(token, !stage->noMipMaps, (!stage->noPicMip ? picmip : 0), stage->force32bit, 2);
			if (!stage->normalMap)
            {
                ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name);
                return qfalse;
			}

			stage->hasNormalMap = qtrue;
		}
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			atestBits = NameToAFunc( token );
		}
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthTestBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthTestBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			if ( !depthMaskExplicit )
			{
				depthMaskBits = 0;
			}
		}
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if (!Q_stricmp(token, "colorwave"))
			{
				vec3_t v;

				ParseVector(text, 3, v);
				stage->colorConst[0] = v[0] * 255.0;
				stage->colorConst[1] = v[1] * 255.0;
				stage->colorConst[2] = v[2] * 255.0;
				ParseWaveForm(text, &stage->rgbWave);
				stage->rgbGen = CGEN_MULTIPLY_BY_WAVEFORM;
				shader.flags |= 2;
			}
			else if (!Q_stricmp(token, "identity"))
			{
				stage->rgbGen = CGEN_IDENTITY;
			}
			else if (!Q_stricmp(token, "identityLighting"))
			{
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if (!Q_stricmp(token, "global"))
			{
				stage->rgbGen = CGEN_GLOBAL_COLOR;
			}
			else if (!Q_stricmp(token, "entity") || !Q_stricmp(token, "fromentity"))
			{
				stage->rgbGen = CGEN_ENTITY;
			}
			else if (!Q_stricmp(token, "oneMinusEntity"))
			{
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if (!Q_stricmp(token, "vertex") || !Q_stricmp(token, "fromclient"))
			{
				stage->rgbGen = CGEN_VERTEX;
				if (stage->alphaGen == 0) {
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if (!Q_stricmp(token, "exactVertex"))
			{
				stage->rgbGen = CGEN_EXACT_VERTEX;
			}
			else if (!Q_stricmp(token, "lightingGrid") || !Q_stricmp(token, "lightingSpherical"))
			{
				stage->rgbGen = CGEN_LIGHTING_SPHERICAL;
			}
			else if (!Q_stricmp(token, "lightingDiffuse") || !Q_stricmp(token, "lightingdiffuse"))
			{
				stage->rgbGen        = CGEN_LIGHTING_SPHERICAL;
				shader.needsNormal   = qtrue;
			}
			else if (!Q_stricmp(token, "default"))
			{
				stage->rgbGen = CGEN_DEFAULT;
			}
			else if (!Q_stricmp(token, "oneMinusVertex"))
			{
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "const" ) || !Q_stricmp(token, "constant"))
			{
				vec3_t	color;

				ParseVector( text, 3, color );
				stage->colorConst[0] = 255 * color[0];
				stage->colorConst[1] = 255 * color[1];
				stage->colorConst[2] = 255 * color[2];

				stage->rgbGen = CGEN_CONSTANT;
			}
			else if (!Q_stricmp(token, "static"))
			{
				stage->rgbGen = CGEN_STATIC;
				if (stage->alphaGen == AGEN_IDENTITY) {
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if (!Q_stricmp(token, "sCoord") || !Q_stricmp(token, "tCoord"))
			{
				if (!Q_stricmp(token, "sCoord")) {
					stage->rgbGen = CGEN_SCOORD;
				}
				else {
					stage->rgbGen = CGEN_TCOORD;
				}

				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;
				stage->alphaConstMin = 0;
				stage->alphaConst = -1;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}

				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}

				stage->alphaMax = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}

				stage->alphaConstMin = atof(token) * 255.0;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					ri.Printf(PRINT_WARNING, "WARNING: missing rgbGen sCoord or tCoord parm 'max' in shader '%s'\n", shader.name);
					continue;
				}

				stage->alphaConst = atof(token) * 255.0;
			}
			else if (!Q_stricmp(token, "dot"))
			{
				shader.needsNormal = qtrue;

				stage->rgbGen = CGEN_DOT;
				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);
			}
			else if (!Q_stricmp(token, "oneminusdot"))
			{
				shader.needsNormal = qtrue;

				stage->rgbGen = CGEN_ONE_MINUS_DOT;
				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if (!Q_stricmp(token, "identity"))
			{
				stage->alphaGen = AGEN_IDENTITY;
			}
			else if (!Q_stricmp(token, "global"))
			{
				stage->alphaGen = AGEN_GLOBAL_ALPHA;
			}
			else if ( !Q_stricmp( token, "entity" ) || !Q_stricmp(token, "fromentity"))
			{
				stage->alphaGen = AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "forcedAlpha" ) )
			{
				stage->alphaGen = AGEN_FORCED_ALPHA;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) || !Q_stricmp(token, "fromclient"))
			{
				stage->alphaGen = AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) || !Q_stricmp(token, "oneMinusFromClient"))
			{
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if (!Q_stricmp(token, "lightingSpecular"))
			{
				shader.needsNormal = qtrue;
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;
				stage->alphaMax = 255;
				stage->specOrigin[0] = -960;
				stage->specOrigin[1] = 1980;
				stage->specOrigin[2] = 96;

				token = COM_ParseExt(text, qfalse);
				if (token[0]) {
					stage->alphaMax = atof(token) * 255.0;
					ParseVector(text, 3, stage->specOrigin);
				}
			}
			else if (!Q_stricmp(token, "distFade") || !Q_stricmp(token, "oneMinusDistFade")
				|| !Q_stricmp(token, "tikiDistFade") || !Q_stricmp(token, "oneMinusTikiDistFade"))
			{
				if (!Q_stricmp(token, "distFade")) {
					stage->alphaGen = AGEN_DIST_FADE;
				} else if (!Q_stricmp(token, "oneMinusDistFade")) {
					stage->alphaGen = AGEN_ONE_MINUS_DIST_FADE;
				} else if (!Q_stricmp(token, "tikiDistFade")) {
                    stage->alphaGen = AGEN_TIKI_DIST_FADE;
                } else {
                    stage->alphaGen = AGEN_ONE_MINUS_TIKI_DIST_FADE;
                }

				shader.fDistNear = 256.0;
				shader.fDistRange = 256.0;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				shader.fDistNear = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				shader.fDistRange = atof(token);
			}
			else if (!Q_stricmp(token, "dot"))
			{
				shader.needsNormal = qtrue;
				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;
				stage->alphaGen = AGEN_DOT;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);
			}
			else if (!Q_stricmp(token, "heightFade"))
            {
                shader.needsNormal = qtrue;
                stage->alphaMin = 256.0f;
                stage->alphaMax = 512.0f;
                stage->alphaGen = AGEN_HEIGHT_FADE;

                token = COM_ParseExt(text, qfalse);
                if (token[0] == 0) {
                    continue;
                }
                stage->alphaMin = atof(token);

                token = COM_ParseExt(text, qfalse);
                if (token[0] == 0) {
                    continue;
                }
                stage->alphaMax = atof(token);
			}
			else if (!Q_stricmp(token, "oneMinusDot"))
			{
				shader.needsNormal = qtrue;
				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;
				stage->alphaGen = AGEN_ONE_MINUS_DOT;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);
			}
			else if (!Q_stricmp(token, "viewDot"))
			{
				shader.needsNormal = qtrue;
				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;
				stage->alphaGen = AGEN_DOT_VIEW;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);
			}
			else if (!Q_stricmp(token, "oneMinusViewDot"))
			{
				shader.needsNormal = qtrue;
				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;
				stage->alphaGen = AGEN_ONE_MINUS_DOT_VIEW;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);
			}
			else if (!Q_stricmp(token, "const") || !Q_stricmp(token, "constant"))
			{
				token = COM_ParseExt(text, qfalse);
				stage->colorConst[3] = 255 * atof(token);
				stage->alphaGen = AGEN_CONSTANT;
			}
			else if (!Q_stricmp(token, "skyAlpha"))
			{
				stage->alphaGen = AGEN_SKYALPHA;
			}
			else if (!Q_stricmp(token, "oneMinusSkyAlpha"))
			{
				stage->alphaGen = AGEN_ONE_MINUS_SKYALPHA;
			}
			else if (!Q_stricmp(token, "sCoord") || !Q_stricmp(token, "tCoord"))
			{
				if (!Q_stricmp(token, "sCoord")) {
					stage->alphaGen = AGEN_SCOORD;
				} else {
					stage->alphaGen = AGEN_TCOORD;
				}

				stage->alphaMin = 0.0;
				stage->alphaMax = 1.0;
				stage->alphaConstMin = 0;
				stage->alphaConst = -1;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMin = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaMax = atof(token);

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					continue;
				}
				stage->alphaConstMin = atof(token) * 255.0;

				token = COM_ParseExt(text, qfalse);
				if (token[0] == 0) {
					ri.Printf(PRINT_WARNING, "WARNING: missing alphaGen s or tCoord parm 'max' in shader '%s'\n", shader.name);
					continue;
				}
				stage->alphaConst = atof(token) * 255.0;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					ri.Printf( PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = atof( token );
				}
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) ) 
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				shader.needsNormal = qtrue;
				stage->bundle[cntBundle].tcGen = TCGEN_ENVIRONMENT_MAPPED;
            }
			else if ( !Q_stricmp( token, "angle_based_environment" ) )
			{
				shader.needsNormal = qtrue;
				stage->bundle[cntBundle].tcGen = TCGEN_ANGLE_BASED_ENVIRONMENT;
            }
            else if (!Q_stricmp(token, "environmentmodel"))
            {
                shader.needsNormal = qtrue;
                stage->bundle[cntBundle].tcGen = TCGEN_ENVIRONMENT_MAPPED2;
            }
            else if (!Q_stricmp(token, "sunreflection"))
            {
                shader.needsNormal = qtrue;
                stage->bundle[cntBundle].tcGen = TCGEN_SUN_REFLECTION;
            }
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->bundle[cntBundle].tcGen = TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->bundle[cntBundle].tcGen = TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				ParseVector( text, 3, stage->bundle[cntBundle].tcGenVectors[0] );
				ParseVector( text, 3, stage->bundle[cntBundle].tcGenVectors[1] );

				stage->bundle[cntBundle].tcGen = TCGEN_VECTOR;
			}
			else 
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
					break;
				strcat( buffer, token );
				strcat( buffer, " " );
			}

			ParseTexMod( buffer, stage, cntBundle );

			continue;
		}
		else if ( !Q_stricmp( token, "depthwrite" ) || !Q_stricmp(token, "depthmask"))
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;

			continue;
        }
		else if (!Q_stricmp(token, "noDepthTest"))
		{
			depthFuncBits = GLS_DEPTHTEST_DISABLE;
			continue;
        }
        else if (!Q_stricmp(token, "nextBundle"))
        {
			if (!GLES3_HAS_MULTITEXTURE) {
				ri.Printf(PRINT_ALL, "WARNING: " PRODUCT_NAME " requires a video card with multitexturing capability\n");
				return qfalse;
			}

			token = COM_ParseExt(text, qfalse);
			if (token[0] && !Q_stricmp(token, "add")) {
				stage->multitextureEnv = VK_TENV_ADD;
			} else {
				stage->multitextureEnv = VK_TENV_MODULATE;
			}

			cntBundle++;
			if (cntBundle > NUM_TEXTURE_BUNDLES) {
				ri.Printf(PRINT_WARNING, "WARNING: too many nextBundle commands in shader '%s'\n", shader.name);
				return qfalse;
			}
            continue;
        }
		else if (!Q_stricmp(token, "ifCvar") || !Q_stricmp(token, "ifCvarnot"))
		{
			cvar_t* var;
			qboolean isNot = token[6] != 0;
			qboolean evaluatedValue;

			token = COM_ParseExt(text, qfalse);
			if (!token[0]) {
				ri.Printf(PRINT_WARNING, "WARNING: missing cvar name in %s", isNot ? "ifCvarnot" : "ifCvar");
				return qfalse;
			}

			var = ri.Cvar_Get(token, "0", 0);

			token = COM_ParseExt(text, qfalse);
			if (!token[0]) {
				ri.Printf(PRINT_WARNING, "WARNING: missing cvar value in %s", isNot ? "ifCvarnot" : "ifCvar");
				return qfalse;
			}

			if (atof(token) != var->value) {
				evaluatedValue = qfalse;
			} else if (var->value) {
				evaluatedValue = qtrue;
			} else if (token[0] == '0' || (token[0] == '.' && token[1] == '0')) {
				evaluatedValue = qtrue;
			} else if (!Q_stricmp(var->string, token)) {
				evaluatedValue = qtrue;
			} else {
				return qfalse;
			}

			shouldProcess &= evaluatedValue ^ isNot;
		}
		else if ( !Q_stricmp( token, "detail" ) )
		{
			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	if (!shouldProcess) {
		stage->active = qfalse;
		stage->rgbGen = CGEN_BAD;
		return qtrue;
	}

	if ( stage->rgbGen == CGEN_BAD ) {
		if ( (cntBundle <= 0 && stage->bundle[0].isLightmap)
			|| (blendSrcBits != 0
				&& blendSrcBits != GLS_SRCBLEND_ONE
				&& blendSrcBits != GLS_SRCBLEND_SRC_ALPHA) ) {
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		} else {
			stage->rgbGen = CGEN_IDENTITY;
		}
	}

	if ( stage->rgbGen == CGEN_DEFAULT ) {
		if ( shader.lightmapIndex == LIGHTMAP_BY_VERTEX ) {
			stage->rgbGen = CGEN_EXACT_VERTEX;
		} else if ( shader.lightmapIndex < 0 ) {
			stage->rgbGen      = CGEN_LIGHTING_SPHERICAL;
			shader.needsNormal = qtrue;
		} else {
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && 
		 ( blendDstBits == GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	if (depthFuncBits == GLS_DEPTHTEST_DISABLE) {
		depthMaskBits = 0;
	}

	if ( stage->alphaGen == AGEN_IDENTITY ) {
		if ( stage->rgbGen == CGEN_IDENTITY
			|| stage->rgbGen == CGEN_LIGHTING_SPHERICAL ) {
			stage->alphaGen = AGEN_SKIP;
		}
	}

	stage->stateBits = depthMaskBits | 
		               blendSrcBits | blendDstBits | 
					   atestBits | 
					   depthFuncBits |
					   depthTestBits |
					   fogBits;

	return qtrue;
}

static void ParseDeform( char **text ) {
	char	*token;
	deformStage_t	*ds;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS ) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "projectionShadow" ) ) {
		ds->deformation = DEFORM_PROJECTION_SHADOW;
		return;
	}

	if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE;
		shader.noMerge |= 1;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE2;
		shader.noMerge |= 1;
		return;
	}

	if ( !Q_stricmpn( token, "text", 4 ) ) {
		unsigned int	n;

		n = (unsigned int)( token[4] - '0' );
		if ( n >= 8 ) {
			n = 0;
		}
		ds->deformation = (deform_t)( DEFORM_TEXT0 + n );
		shader.needsNormal = qtrue;
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = atof( token );

		ds->deformation = DEFORM_BULGE;
		shader.needsNormal = qtrue;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) || !Q_stricmp( token, "heightwave" )
		|| !Q_stricmp( token, "inverseheightwave" ) )
	{
		if ( !Q_stricmp( token, "heightwave" ) ) {
			ds->deformation = DEFORM_HEIGHTWAVE;
		} else if ( !Q_stricmp( token, "inverseheightwave" ) ) {
			ds->deformation = DEFORM_INVERSEHEIGHTWAVE;
		} else {
			ds->deformation = DEFORM_WAVE;
		}

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		if ( atof( token ) != 0 )
		{
			ds->deformationSpread = 1.0f / atof( token );
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		shader.needsNormal = qtrue;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = atof( token );

		ds->deformation = DEFORM_NORMALS;
		shader.needsNormal = qtrue;
		return;
	}

	if ( !Q_stricmp( token, "move" ) ) {
		int		i;

		for ( i = 0 ; i < 3 ; i++ ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
				break;
			}
			ds->moveVector[i] = atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
		return;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}


static void ParseSkyParms( char **text ) {
	char		*token;
	static char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
	char		pathname[MAX_QPATH];
	int			i;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga"
				, token, suf[i] );
			shader.sky.outerbox[i] = R_FindImageFileOld((char*)pathname, qtrue, qtrue, shader_force32bit, 0);
			if ( !shader.sky.outerbox[i] ) {
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	shader.sky.cloudHeight = atof( token );
	if ( !shader.sky.cloudHeight ) {
		shader.sky.cloudHeight = 512;
	}
	R_InitSkyTexCoords( shader.sky.cloudHeight );


	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga"
				, token, suf[i] );
			shader.sky.innerbox[i] = R_FindImageFileOld((char*)pathname, qtrue, qtrue, shader_force32bit, 0);
			if ( !shader.sky.innerbox[i] ) {
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.isSky = qtrue;
}


void ParseSort( char **text ) {
	char	*token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) ) {
		shader.sort = SS_PORTAL;
	} else if ( !Q_stricmp( token, "sky" ) ) {
		shader.sort = SS_ENVIRONMENT;
	} else if ( !Q_stricmp( token, "opaque" ) ) {
		shader.sort = SS_OPAQUE;
	}else if ( !Q_stricmp( token, "decal" ) ) {
		shader.sort = SS_DECAL;
	} else if ( !Q_stricmp( token, "seeThrough" ) ) {
		shader.sort = SS_SEE_THROUGH;
	} else if ( !Q_stricmp( token, "banner" ) ) {
		shader.sort = SS_BANNER;
	} else if ( !Q_stricmp( token, "additive" ) ) {
		shader.sort = SS_BLEND1;
	} else if ( !Q_stricmp( token, "nearest" ) ) {
		shader.sort = SS_NEAREST;
	} else if ( !Q_stricmp( token, "underwater" ) ) {
		shader.sort = SS_UNDERWATER;
	} else {
		shader.sort = atof( token );
	}
}




typedef struct {
	char	*name;
	int		clearSolid, surfaceFlags, contents;
} infoParm_t;

infoParm_t	infoParms[] = {
	{"water",		1,	0,	CONTENTS_WATER },
	{"slime",		1,	0,	CONTENTS_SLIME },		
	{"lava",		1,	0,	CONTENTS_LAVA },		
	{"playerclip",	1,	0,	CONTENTS_PLAYERCLIP },
	{"monsterclip",	1,	0,	CONTENTS_MONSTERCLIP },
	{"cameraclip",	1,	0,	CONTENTS_CAMERACLIP },	 
	{"weaponclip",	1,	0,	CONTENTS_WEAPONCLIP },
	{"vehicleclip",	1,	0,	CONTENTS_DONOTENTER },
	{"nodrop",		1,	0,	CONTENTS_NODROP },		
	{"nonsolid",	1,	SURF_NONSOLID,	0 },		

	{"origin",		1,	0,	CONTENTS_ORIGIN },		
	{"trans",		0,	0,	CONTENTS_TRANSLUCENT },	
	{"detail",		0,	0,	CONTENTS_DETAIL },		
	{"structural",	0,	0,	CONTENTS_STRUCTURAL },	
	{"areaportal",	1,	0,	CONTENTS_AREAPORTAL },	
	{"fog",			1,	0,	CONTENTS_FOG},			
	{"sky",			0,	SURF_SKY,			0 },	
	{"alphashadow",	1,	SURF_ALPHASHADOW,	0 },	

	{"slick",		0,	SURF_SLICK,		0 },
	{"noimpact",	0,	SURF_NOIMPACT,	0 },		
	{"nomarks",		0,	SURF_NOMARKS,	0 },		
	{"ladder",		0,	SURF_LADDER,	0 },
	{"nodamage",	0,	SURF_NODAMAGE,	0 },
	{"nosteps",		0,	SURF_NOSTEPS,	0 },
	{"sand",		0,	SURF_TYPE_SAND,		0 },
	{"snow",		0,	SURF_TYPE_SNOW,		0 },
	{"metal_duct",	0,	SURF_TYPE_METAL_DUCT,	0 },
	{"metal",		0,	SURF_TYPE_METAL,	0 },
	{"rock",		0,	SURF_TYPE_ROCK,		0 },
	{"dirt",		0,	SURF_TYPE_DIRT,		0 },
	{"grill",		0,	SURF_TYPE_GRILL,	0 },
	{"organic",		0,	SURF_TYPE_ORGANIC,	0 },
	{"squishy",		0,	SURF_TYPE_SQUISHY,	0 },
	{"metal_hollow",0,	SURF_TYPE_METAL_HOLLOW,	0 },
	{"carpet",		0,	SURF_TYPE_CARPET,	0 },
	{"default",		0,	SURF_TYPE_DEFAULT,	0 },

	{"nodraw",		0,	SURF_NODRAW,		0 },	
	{"castshadow",	0,	SURF_CASTSHADOW,	0 },	
	{"nolightmap",	0,	SURF_NOLIGHTMAP,	0 },	
	{"nodlight",	0,	SURF_NODLIGHT,		0 },	
	{"hint",		0,	SURF_HINT,		0}			
};


static void ParseSurfaceParm( char **text ) {
	char	*token;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	int		i;

	token = COM_ParseExt( text, qfalse );
	for ( i = 0 ; i < numInfoParms ; i++ ) {
		if ( !Q_stricmp( token, infoParms[i].name ) ) {
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if ( infoParms[i].clearSolid ) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			break;
		}
	}
}

static qboolean ParseShader( char **text, qboolean picmip )
{
	char *token;
	int s;
	int matchingendifs;
	qboolean bInElseBlock;

	s = 0;
	matchingendifs = 0;
	bInElseBlock = qfalse;

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' )
	{
		ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return qfalse;
	}

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name );
			return qfalse;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		else if ( token[0] == '{' )
		{
			if ( !ParseStage( &unfoggedStages[s], text, picmip ) )
			{
				return qfalse;
			}
			if (unfoggedStages[s].rgbGen != CGEN_BAD) {
				unfoggedStages[s].active = qtrue;
				if (s > 0) {
					if (!(unfoggedStages[s].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS))) {
						ri.Printf(PRINT_WARNING, "WARNING: shader '%s' has opaque maps defined after stage 0!!!\n", shader.name);
					}
					if (unfoggedStages[s].stateBits & GLS_DEPTHMASK_TRUE) {
						ri.Printf(PRINT_WARNING, "WARNING: shader '%s' has depthmask enabled after stage 0!!!\n", shader.name);
					}
				}
			}
			s++;
			continue;
		}
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "q3map_sun" ) ) {
			float	a, b;

			token = COM_ParseExt( text, qfalse );
			tr.sunLight[0] = atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[1] = atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[2] = atof( token );
			
			VectorNormalize( tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight);

			token = COM_ParseExt( text, qfalse );
			a = atof( token );
			a = a / 180 * M_PI;

			token = COM_ParseExt( text, qfalse );
			b = atof( token );
			b = b / 180 * M_PI;

			tr.sunDirection[0] = cos( a ) * cos( b );
			tr.sunDirection[1] = sin( a ) * cos( b );
			tr.sunDirection[2] = sin( b );
		}
		else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, qfalse );
			if (token[0]) {
				shader.clampTime = atof(token);
			}
		}
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if (  !Q_stricmp( token, "surfaceParm" )
				|| !Q_stricmp( token, "surfaceLight" )
				|| !Q_stricmp( token, "surfaceColor" )
				|| !Q_stricmp( token, "surfaceAngle" )
				|| !Q_stricmp( token, "surfaceDensity" ) ) {
			if ( !Q_stricmp( token, "surfaceParm" )) {
				ParseSurfaceParm( text );
				continue;
			}
			SkipRestOfLine(text);
			continue;
		}
		else if ( !Q_stricmp( token, "nomipmaps" ) )
		{
			shader_noMipMaps = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			shader_noPicMip = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "maxpicmip" ) || !Q_stricmp( token, "minpicmip" ) )
		{
			SkipRestOfLine( text );
			continue;
		}
		else if (!Q_stricmp(token, "force32bit"))
		{
			shader_force32bit = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			shader.polygonOffset = qtrue;
			continue;
		}
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = qtrue;
			continue;
			}
		else if (!Q_stricmp(token, "noMerge"))
		{
			shader.noMerge |= 1;
			continue;
		}
		else if ( !Q_stricmp(token, "portal") )
		{
			shader.sort = SS_PORTAL;
			continue;
		}
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			ParseSkyParms( text );
			continue;
		}
		else if (!Q_stricmp(token, "portalsky"))
		{
			shader.sort = SS_PORTALSKY;
			shader.isPortalSky = qtrue;
			continue;
		}
		else if ( !Q_stricmp(token, "light") ) 
		{
			token = COM_ParseExt( text, qfalse );
			continue;
		}
		else if (!Q_stricmp(token, "spritegen"))
		{
			token = COM_ParseExt(text, qfalse);
			if (token[0] == 0) {
				ri.Printf(PRINT_WARNING, "WARNING: missing spritegen parm in shader '%s'\n", shader.name);
				continue;
			}

			if (!Q_stricmp(token, "parallel")) {
				shader.sprite.type = SPRITE_PARALLEL;
			} else if (!Q_stricmp(token, "parallel_oriented")) {
				shader.sprite.type = SPRITE_PARALLEL_ORIENTED;
			} else if (!Q_stricmp(token, "parallel_upright")) {
				shader.sprite.type = SPRITE_PARALLEL_UPRIGHT;
			} else if (!Q_stricmp(token, "oriented")) {
				shader.sprite.type = SPRITE_ORIENTED;
			}

			shader.sprite.scale = 1.0;
			continue;
		}
		else if (!Q_stricmp(token, "spritescale"))
		{
			token = COM_ParseExt(text, qfalse);
			if (token[0] == 0) {
				ri.Printf(PRINT_WARNING, "WARNING: missing spritescale parm in shader '%s'\n", shader.name);
				continue;
			}

			shader.sprite.scale = atof(token);
			continue;
		}
		else if ( !Q_stricmp( token, "cull") ) 
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		else if (!Q_stricmp(token, "#if") || !Q_stricmp(token, "if") || !Q_stricmp(token, "#if_not") || !Q_stricmp(token, "#else"))
		{
			qboolean conditionpassed;

			conditionpassed = qfalse;
			if (!Q_stricmp(token, "#else"))
			{
				if (bInElseBlock) {
					ri.Printf(
						PRINT_WARNING,
						"WARNING: illegal #else after a previous #else in shader '%s'\n",
						shader.name);
				}

				matchingendifs--;
				if (matchingendifs < 0) {
					ri.Printf(PRINT_WARNING, "WARNING: unmatched #else in shader '%s'\n", shader.name);
				}

				bInElseBlock = qtrue;
			}
			else
			{
				qboolean bInvertResult;

				bInvertResult = !Q_stricmp(token, "#if_not");

				token = COM_ParseExt(text, qfalse);
				if (!Q_stricmp(token, "separate_env")) {
					conditionpassed = r_detailtextures->integer != 0;
				} else if (!Q_stricmp(token, "mtex")) {
					conditionpassed = GLES3_HAS_MULTITEXTURE;
				} else if (!Q_stricmp(token, "no_mtex")) {
					conditionpassed = !GLES3_HAS_MULTITEXTURE;
				} else if (!Q_stricmp(token, "detail")) {
					conditionpassed = r_detailtextures->integer != 0;
				} else if (!Q_stricmp(token, "noDetail")) {
					conditionpassed = r_detailtextures->integer == 0;
				} else if (!Q_stricmp(token, "vertexLight")) {
					conditionpassed = r_vertexLight->integer != 0;
				} else if (!Q_stricmp(token, "noVertexLight")) {
					conditionpassed = r_vertexLight->integer == 0;
				} else if (!Q_stricmp(token, "0") || !Q_stricmp(token, "false")) {
					conditionpassed = qfalse;
				} else if (!Q_stricmp(token, "1") || !Q_stricmp(token, "true")) {
					conditionpassed = qtrue;
				} else {
					ri.Printf(
						PRINT_WARNING,
						"WARNING: invalid #if argument '%s' in shader '%s', not passing.\n",
						token,
						shader.name);
				}

				if (bInvertResult) {
					conditionpassed = !conditionpassed;
				}
			}

			if (!conditionpassed) {
				int nestedifs = 0;

				SkipRestOfLine(text);

				while (1) {
					token = COM_ParseExt(text, qtrue);
					if (!token[0]) {
						ri.Printf(PRINT_WARNING, "WARNING: no matching #endif in shader '%s'\n", shader.name);
						break;
					}

					if (!Q_stricmp(token, "#if") || !Q_stricmp(token, "if")) {
						nestedifs++;
						SkipRestOfLine(text);
					} else if (!Q_stricmp(token, "#endif") || !Q_stricmp(token, "endif")) {
						nestedifs--;
					} else if (!Q_stricmp(token, "#else")) {
						if (bInElseBlock) {
							ri.Printf(
								PRINT_WARNING,
								"WARNING: illegal #else after a previous #else in shader '%s'\n",
								shader.name);
							break;
						}

						if (!nestedifs) {
							matchingendifs++;
							break;
						}
						nestedifs++;
					} else {
						SkipRestOfLine(text);
					}

					if (nestedifs == -1) {
						break;
					}
				}
			} else {
				matchingendifs++;
			}

			continue;
		}
		else if (!Q_stricmp(token, "#endif") || !Q_stricmp(token, "endif"))
		{
			matchingendifs--;
			if (matchingendifs < 0) {
				ri.Printf(PRINT_WARNING, "WARNING: unmatched endif in shader '%s'\n", shader.name);
			}

			bInElseBlock = qfalse;
		}
		else if (!Q_stricmp(token, "forcedAlphaShader"))
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] && strlen( token ) < sizeof( shader.forcedAlphaShader ) ) {
				strcpy( shader.forcedAlphaShader, token );
			} else {
				ri.Printf( PRINT_WARNING, "WARNING: bad shader name for forceAlphaShader in shader '%s'\n", shader.name );
			}
			continue;
		}
		else if (!Q_stricmp(token, "detailShader"))
		{
			shader.detailShader = qtrue;
			continue;
		}
		else if (!Q_stricmp(token, "textureOnlyIfNoDetail"))
		{
			continue;
		}
		else if (!Q_stricmp(token, "nofog"))
		{
			continue;
		}
		else if (!Q_stricmp(token, "fogParms"))
		{
			int		i;

			for (i = 0; i < 3; i++) {
				token = COM_ParseExt(text, qfalse);
				if (!token[0]) {
					ri.Printf(PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name);
					return qfalse;
				}
				shader.fogParms.color[i] = atof(token);
			}

			token = COM_ParseExt(text, qfalse);
			if (!token[0]) {
				ri.Printf(PRINT_WARNING, "WARNING: missing depth for opaque parm for 'fogParms' keyword in shader '%s'\n", shader.name);
			} else {
				shader.fogParms.depthForOpaque = atof(token);
				SkipRestOfLine(text);
			}
			continue;
		}
		else if (!Q_stricmp(token, "foggen"))
		{
			ParseWaveForm(text, &shader.fogParms.wave);
			continue;
		}
		else if (!Q_stricmp(token, "fogonly"))
		{
			shaderStage_t	*pStage = &unfoggedStages[0];

			pStage->active = qtrue;
			pStage->bundle[0].image[0] = tr.whiteImage;
			pStage->stateBits = 101;
			Com_Memcpy(fogStages, unfoggedStages, sizeof(shaderStage_t));
			shader.fogonly = qtrue;
			continue;
		}
		else
		{
			ri.Printf( PRINT_DEVELOPER, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			SkipRestOfLine( text );
			continue;
		}
	}

	if ( s == 0 && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG) && !(shader.surfaceFlags & SURF_NODRAW)) {
		return qfalse;
	}

	if (!(shader.flags & 2)) {
		shader.flags &= ~1;
	}

	if ((shader.contentFlags & CONTENTS_SETCLIP) != 0 && shader.cullType == CT_TWO_SIDED) {
		shader.cullType = CT_FRONT_SIDED;
	}

	shader.explicitlyDefined = qtrue;

	return qtrue;
}


static void ComputeStageIteratorFunc( void )
{
	shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;

	if ( shader.isSky )
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
		goto done;
	}

	if ( r_ignoreFastPath->integer )
	{
		return;
	}

	if ( shader.numUnfoggedPasses == 1 )
	{
		if ( unfoggedStages[0].rgbGen == CGEN_LIGHTING_SPHERICAL )
		{
			if ( unfoggedStages[0].alphaGen == AGEN_IDENTITY || unfoggedStages[0].alphaGen == AGEN_SKIP)
			{
				if ( unfoggedStages[0].bundle[0].tcGen == TCGEN_TEXTURE && !unfoggedStages[0].bundle[0].numTexMods)
				{
					if ( !shader.polygonOffset )
					{
						if ( !unfoggedStages[0].multitextureEnv )
						{
							if ( !shader.numDeforms )
							{
								shader.optimalStageIteratorFunc = RB_StageIteratorVertexLitTextureUnfogged;
								goto done;
							}
						}
					}
				}
			}
		}
	}

	if ( shader.numUnfoggedPasses == 1 )
	{
		if ( ( unfoggedStages[0].rgbGen == CGEN_IDENTITY ) && ( unfoggedStages[0].alphaGen == AGEN_IDENTITY || unfoggedStages[0].alphaGen == AGEN_SKIP ) )
		{
			if ( unfoggedStages[0].bundle[0].tcGen == TCGEN_TEXTURE && 
				unfoggedStages[0].bundle[1].tcGen == TCGEN_LIGHTMAP )
			{
				if ( !shader.polygonOffset )
				{
					if ( !shader.numDeforms )
					{
						if (unfoggedStages[0].multitextureEnv )
						{
							shader.optimalStageIteratorFunc = RB_StageIteratorLightmappedMultitextureUnfogged;
							goto done;
						}
					}
				}
			}
		}
	}

done:
	return;
}

typedef struct {
	int		blendKey;			 
	int		multitextureEnv;	 
} collapse_t;

static collapse_t	collapse[] = {
	{ GLS_DSTBLEND_ZERO    | GLS_SRCBLEND_DST_COLOR,	1 },	
	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,		1 },	
	{ GLS_DSTBLEND_ONE     | GLS_SRCBLEND_ONE,			2 },	
	{ 0, 0 }													
};

static void CollapseMultitexture(int *stagecounter) {
	int iUseCollapse;
	int stagenum;
	int abits, bbits;
	shaderStage_t* stage;
	textureBundle_t tmpBundle;

	for (stagenum = 0; stagenum < *stagecounter - 1; stagenum++) {
		stage = &unfoggedStages[stagenum];

		if (!stage[0].active || !stage[1].active) {
			continue;
		}

		if (stage->multitextureEnv) {
			continue;
		}

		if (glConfig.driverType == GLDRV_VOODOO) {
			if (stage[1].bundle[0].image[0]->TMU ==
				unfoggedStages[stagenum].bundle[0].image[0]->TMU) {
				continue;
			}
		}

		abits = stage[0].stateBits;
		bbits = stage[1].stateBits;

		{
			int		a = abits & 0xFF;
			int		b = bbits & 0xFF;

			if ( a == ( GLS_DSTBLEND_ZERO | GLS_SRCBLEND_ONE ) ) {	
				a = 0;
			}
			if ( ( abits & ~0x1FF ) != ( bbits & ~0x1FF ) ) {
				continue;
			}
			if ( a != b && a != 0 ) {
				continue;
			}

			for ( iUseCollapse = 0; collapse[iUseCollapse].blendKey != 0; iUseCollapse++ ) {
				if ( collapse[iUseCollapse].blendKey == b ) {
					break;
				}
			}
			if ( collapse[iUseCollapse].blendKey == 0 ) {
				continue;
			}

			if ( collapse[iUseCollapse].multitextureEnv == 2 && !glConfig.textureEnvAddAvailable ) {
				continue;
			}

			if ( stage->rgbGen != unfoggedStages[stagenum + 1].rgbGen ||
				 stage->alphaGen != unfoggedStages[stagenum + 1].alphaGen ) {
				continue;
			}
			if ( ( stage->rgbGen == CGEN_WAVEFORM || stage->rgbGen == CGEN_MULTIPLY_BY_WAVEFORM ) &&
				 memcmp( &stage->rgbWave, &unfoggedStages[stagenum + 1].rgbWave, sizeof( stage->rgbWave ) ) ) {
				continue;
			}
			if ( stage->alphaGen == AGEN_WAVEFORM &&
				 memcmp( &stage->alphaWave, &unfoggedStages[stagenum + 1].alphaWave, sizeof( stage->alphaWave ) ) ) {
				continue;
			}

			{
				int		ci;
				float	merged[4];

				for ( ci = 0; ci < 4; ci++ ) {
					float	v0 = stage->colorConst[ci] * ( 1.0f / 255.0f );
					float	v1 = stage[1].colorConst[ci] * ( 1.0f / 255.0f );

					if ( collapse[iUseCollapse].multitextureEnv == 2 ) {
						float	sum = v0 + v1;
						merged[ci] = ( sum >= 1.0f ) ? sum : 1.0f;
					} else if ( collapse[iUseCollapse].multitextureEnv == 1 ) {
						merged[ci] = v0 * v1;
					} else {
						ri.Printf( PRINT_ALL, "Unknown MT mode collapsing multitexture for %s\n", shader.name );
						merged[ci] = v0;
					}
				}
				for ( ci = 0; ci < 4; ci++ ) {
					stage->colorConst[ci] = (byte)( merged[ci] * 255.0f );
				}
			}
		}

		if (stage->bundle[0].isLightmap)
		{
			tmpBundle = stage->bundle[0];
			stage->bundle[0] = stage[1].bundle[0];
			stage->bundle[1] = tmpBundle;
		}
		else
		{
			stage->bundle[1] = stage[1].bundle[0];
		}

		stage->multitextureEnv = ( collapse[iUseCollapse].multitextureEnv == 2 ) ? VK_TENV_ADD : VK_TENV_MODULATE;

		if (stagenum + 2 < MAX_SHADER_STAGES) {
			memmove(&unfoggedStages[stagenum + 1], &unfoggedStages[stagenum + 2], sizeof(unfoggedStages[0]) * (MAX_SHADER_STAGES - 2 - stagenum));
		}

		Com_Memset(&unfoggedStages[MAX_SHADER_STAGES - 1], 0, sizeof(unfoggedStages[0]));
		(*stagecounter)--;
	}
}

static void Q_UNUSED_VAR FixRenderCommandList( int newShader ) {
	/* The GLES3 twin patches the shader number packed into every recorded
	   drawSurf sort key here, because its 3D record carries numbers.  This
	   module's 2D record carries the shader_t pointer itself and resolves it
	   through remappedShader at draw time, so a remap is visible to a frame
	   that is already recorded and there is nothing to rewrite. */
	( void )newShader;
}

static void SortNewShader( void ) {
	int		i;
	float	sort;
	shader_t	*newShader;

	newShader = tr.shaders[ tr.numShaders - 1 ];
	sort = newShader->sort;

	for ( i = tr.numShaders - 2 ; i >= 0 ; i-- ) {
		if ( tr.sortedShaders[ i ]->sort <= sort ) {
			break;
		}
		tr.sortedShaders[i+1] = tr.sortedShaders[i];
		tr.sortedShaders[i+1]->sortedIndex++;
	}

	newShader->sortedIndex = i+1;
	tr.sortedShaders[i+1] = newShader;
}


static int R_AllocStageList(shaderStage_t *dst[], shaderStage_t *src, int count) {
	int i, b;

	for ( i = 0 ; i < count ; i++ ) {
		int size;

		// @0x3C2AC `v9[114] = i`, @0x3C3E4 `v9[123] = j`), so a gap can never leave
		if ( !src[i].active ) {
			break;
		}
		dst[i] = ri.Hunk_Alloc( sizeof( shaderStage_t ), h_dontcare );
		*dst[i] = src[i];

		for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
			size = dst[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			if (size) {
				dst[i]->bundle[b].texMods = ri.Hunk_Alloc(size, h_dontcare);
				Com_Memcpy(dst[i]->bundle[b].texMods, src[i].bundle[b].texMods, size);
			}
		}
	}
	return i;
}

static shader_t *GeneratePermanentShader( void ) {
	shader_t	*newShader;

	if ( tr.numShaders == MAX_SHADERS ) {
		ri.Printf( PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	newShader = ri.Hunk_Alloc( sizeof( shader_t ), h_dontcare);

	*newShader = shader;
	newShader->next = currentShader->shader;
	currentShader->shader = newShader;

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;
	
	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	newShader->numUnfoggedPasses = R_AllocStageList( newShader->unfoggedStages, unfoggedStages, newShader->numUnfoggedPasses );
	newShader->numFogPasses = R_AllocStageList( newShader->fogStages, fogStages, newShader->numFogPasses );
	newShader->numGlobalFogPasses = R_AllocStageList( newShader->globalFogStages, globalFogStages, newShader->numGlobalFogPasses );

	SortNewShader();

	currentShader = NULL;

	return newShader;
}

static void VertexLightingCollapse( void ) {
	int		stage;
	shaderStage_t	*bestStage;
	int		bestImageRank;
	int		rank;

	if ( shader.sort == SS_OPAQUE ) {

		bestStage = &unfoggedStages[0];
		bestImageRank = -999999;

		for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
			shaderStage_t *pStage = &unfoggedStages[stage];

			if ( !pStage->active ) {
				break;
			}
			rank = 0;

			if ( pStage->bundle[0].isLightmap ) {
				rank -= 100;
			}
			if ( pStage->bundle[0].tcGen != TCGEN_TEXTURE ) {
				rank -= 5;
			}
			if ( pStage->bundle[0].numTexMods ) {
				rank -= 5;
			}
			if ( pStage->rgbGen != CGEN_IDENTITY && pStage->rgbGen != CGEN_IDENTITY_LIGHTING ) {
				rank -= 3;
			}

			if ( rank > bestImageRank  ) {
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		unfoggedStages[0].bundle[0] = bestStage->bundle[0];
		unfoggedStages[0].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		unfoggedStages[0].stateBits |= GLS_DEPTHMASK_TRUE;
		if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
			unfoggedStages[0].rgbGen = CGEN_LIGHTING_SPHERICAL;
		} else {
			unfoggedStages[0].rgbGen = CGEN_EXACT_VERTEX;
		}
		unfoggedStages[0].alphaGen = AGEN_SKIP;		
	} else {
		if ( unfoggedStages[0].bundle[0].isLightmap ) {
			unfoggedStages[0] = unfoggedStages[1];
		}

		if ( unfoggedStages[0].rgbGen == CGEN_ONE_MINUS_ENTITY || unfoggedStages[1].rgbGen == CGEN_ONE_MINUS_ENTITY ) {
			unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( unfoggedStages[0].rgbGen == CGEN_WAVEFORM && unfoggedStages[0].rgbWave.func == GF_SAWTOOTH )
			&& ( unfoggedStages[1].rgbGen == CGEN_WAVEFORM && unfoggedStages[1].rgbWave.func == GF_INVERSE_SAWTOOTH ) ) {
			unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( unfoggedStages[0].rgbGen == CGEN_WAVEFORM && unfoggedStages[0].rgbWave.func == GF_INVERSE_SAWTOOTH )
			&& ( unfoggedStages[1].rgbGen == CGEN_WAVEFORM && unfoggedStages[1].rgbWave.func == GF_SAWTOOTH ) ) {
			unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &unfoggedStages[stage];

		if ( !pStage->active ) {
			break;
		}

		Com_Memset( pStage, 0, sizeof( *pStage ) );
	}
}




static shader_t	*FinishShader( void );

static void InitGlobalShader( void ) {
	int		i, j;

	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &unfoggedStages, 0, sizeof( unfoggedStages ) );
	Com_Memset( &fogStages, 0, sizeof( fogStages ) );
	Com_Memset( &globalFogStages, 0, sizeof( globalFogStages ) );
	Com_Memset( &texMods, 0, sizeof( texMods ) );

	shader.sprite.scale = 1.0f;

	shader_noPicMip = qfalse;
	shader_noMipMaps = qfalse;
	shader_force32bit = qfalse;

	shader.needsNormal = qfalse;
	shader.needsST1 = qtrue;
	shader.needsST2 = qtrue;
	shader.needsColor = qtrue;

	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		for ( j = 0 ; j < NUM_TEXTURE_BUNDLES ; j++ ) {
			unfoggedStages[i].bundle[j].texMods = texMods[i];
		}
	}
}


qhandle_t RE_ReplaceShader( const char *name, const char *text ) {
	char			strippedName[MAX_QPATH];
	shadertext_t	*shaderText;
	int				hash;

	if ( !name || !name[0] || !text || !text[0] ) {
		return 0;
	}

	COM_StripExtension( name, strippedName, sizeof( strippedName ) );

	Q_strncpyz( shader.name, strippedName, sizeof( shader.name ) );	 
	InitGlobalShader();

	hash = generateHashValue( strippedName );

	for ( currentShader = hashTable[hash]; currentShader; currentShader = currentShader->next ) {
		if ( !Q_stricmp( currentShader->name, strippedName ) ) {
			break;
		}
	}
	if ( !currentShader ) {
		shaderText = (shadertext_t *)ri.Malloc( sizeof( shadertext_t ) );
		Com_Memset( shaderText, 0, sizeof( shadertext_t ) );
		strncpy( shaderText->name, strippedName, sizeof( shaderText->name ) );
		shaderText->next = hashTable[hash];
		hashTable[hash] = shaderText;
		currentShader = shaderText;
	}

	if ( currentShader->shader == tr.defaultShader ) {
		currentShader = NULL;
		return 0;
	}

	if ( currentShader->text ) {
		currentShader->text = NULL;
	}
	currentShader->text = (char *)ri.Malloc( strlen( text ) + 1 );
	strcpy( currentShader->text, text );

	if ( !ParseShader( &currentShader->text, r_picmip->integer ) ) {
		currentShader = NULL;
		return 0;
	}

	FinishShader();

	return 0;
}


static shader_t *FinishShader( void ) {
	int stage;
	int i;
	int bundle;
	qboolean hasLightmapStage;

	if (!currentShader) {
		currentShader = FindShaderText(shader.name);
	}

    if (shader.defaultShader)
    {
        SMP_ShaderDefaultProbe( "finish-default", shader.name, "FinishShader/defaultShader", 0, 0 );
        currentShader->shader = tr.defaultShader;
        currentShader = NULL;
        return tr.defaultShader;
    }

	hasLightmapStage = qfalse;

	if ( shader.isPortalSky ) {
		shader.sort = SS_PORTALSKY;
	}
	if ( shader.isSky ) {
		shader.sort = SS_ENVIRONMENT;
	}

	if ( shader.polygonOffset && !shader.sort ) {
		shader.sort = SS_DECAL;
	}

	shader.needsLSpherical = qfalse;

	stage = 0;
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &unfoggedStages[stage];

		if ( !pStage->active ) {
			break;
		}

		if ( !pStage->bundle[0].image[0] ) {
			ri.Printf( PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name );
			pStage->active = qfalse;
			continue;
		}

		if (pStage->rgbGen == CGEN_LIGHTING_SPHERICAL || pStage->rgbGen == CGEN_STATIC) {
			shader.needsLSpherical = qtrue;
		}

		for (bundle = 0; bundle < 2; bundle++) {
			if (pStage->bundle[bundle].isLightmap) {
				if (pStage->bundle[bundle].tcGen == TCGEN_BAD) {
					pStage->bundle[bundle].tcGen = TCGEN_LIGHTMAP;
				}
				hasLightmapStage = qtrue;
			}
			else {
				if (pStage->bundle[bundle].tcGen == TCGEN_BAD) {
					pStage->bundle[bundle].tcGen = TCGEN_TEXTURE;
				}
			}
		}

		if (pStage->multitextureEnv && pStage->bundle[0].isLightmap) {
			textureBundle_t tmpBundle = pStage->bundle[0];
			pStage->bundle[0] = pStage->bundle[1];
			pStage->bundle[1] = tmpBundle;
		}
	}

	if (stage > 1)
	{
		if (r_vertexLight->integer) {
			VertexLightingCollapse();
			stage = 1;
			hasLightmapStage = qfalse;
		}

		if (GLES3_HAS_MULTITEXTURE) {
			CollapseMultitexture(&stage);
		}
	}

	for (i = 0; i < stage; i++) {
		shaderStage_t* pStage = &unfoggedStages[i];

		if ( ( pStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS) ) &&
			 ( unfoggedStages[0].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS) ) ) {
			int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			if ( !shader.fogonly ) {
				if ( ( blendSrcBits == GLS_SRCBLEND_ONE && blendDstBits == GLS_DSTBLEND_ONE )
					|| ( blendSrcBits == GLS_SRCBLEND_ZERO && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) {
					pStage->acff = ACFF_MODULATE_RGB;
				}
				else if ( ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
						|| ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE ) ) {
					pStage->acff = ACFF_MODULATE_ALPHA;
				}
				else if ( blendSrcBits == GLS_SRCBLEND_ONE && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) {
					pStage->acff = ACFF_MODULATE_RGBA;
				}
			}

			if (shader.sort == SS_BAD)
			{
				if (pStage->stateBits & GLS_DEPTHMASK_TRUE) {
					shader.sort = SS_SEE_THROUGH;
				}
				else {
					shader.sort = SS_BLEND0;
				}
			}
		}
	}

	if (shader.sort == SS_BAD) {
		shader.sort = SS_OPAQUE;
	}

	if (shader.lightmapIndex >= 0 && !hasLightmapStage) {
		ri.Printf(PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name);
		shader.lightmapIndex = LIGHTMAP_NONE;
	}


	if ( shader.fogonly ) {
		Com_Memcpy(fogStages, unfoggedStages, sizeof(shaderStage_t));
		unfoggedStages[0].active = qfalse;
		shader.sort = SS_FOG;
	} else {
		Com_Memcpy(fogStages, unfoggedStages, sizeof(fogStages));
		Com_Memcpy(globalFogStages, unfoggedStages, sizeof(globalFogStages));
	}

	if ( stage < MAX_SHADER_STAGES ) {
		unsigned	prevStateBits = 0;
		shaderStage_t	*pFogStage;

		if ( stage > 0 ) {
			prevStateBits = unfoggedStages[stage - 1].stateBits & 0xD00;
		}

		pFogStage = &fogStages[stage];
		pFogStage->active = qtrue;
		pFogStage->bundle[0].image[0] = tr.fogImage;
		pFogStage->bundle[0].tcGen = TCGEN_FOG;
		pFogStage->rgbGen = CGEN_FOG;
		pFogStage->alphaGen = AGEN_IDENTITY;
		pFogStage->stateBits = prevStateBits | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;

		pFogStage = &globalFogStages[stage];
		pFogStage->active = qtrue;
		pFogStage->bundle[0].image[0] = tr.whiteImage;
		pFogStage->bundle[0].tcGen = TCGEN_IDENTITY;
		pFogStage->rgbGen = CGEN_FOG_BLEND;
		pFogStage->alphaGen = AGEN_FOG_GLOBAL;
		pFogStage->stateBits = prevStateBits | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else {
		ri.Printf(PRINT_WARNING, "WARNING: overflowed shader stages in shader '%s'\n", shader.name);
	}

	// alphaGen ordinal retail's ParseStage never assigns, so that arm is inert.
	for (stage = 0; stage < MAX_SHADER_STAGES; stage++) {
		unfoggedStages[stage].acff = ACFF_NONE;
		if (unfoggedStages[stage].rgbGen == CGEN_FOG_BLEND) {
			unfoggedStages[stage].rgbGen = CGEN_IDENTITY;
		}
		if (unfoggedStages[stage].alphaGen == AGEN_INERT_11) {
			unfoggedStages[stage].alphaGen = AGEN_IDENTITY;
		}
	}

	shader.numUnfoggedPasses = 0;
	shader.numFogPasses = 0;
	shader.numGlobalFogPasses = 0;
	for (stage = 0; stage < MAX_SHADER_STAGES; stage++) {
		if (unfoggedStages[stage].active) {
			shader.numUnfoggedPasses++;
		}
		if (fogStages[stage].active) {
			shader.numFogPasses++;
		}
		if (globalFogStages[stage].active) {
			shader.numGlobalFogPasses++;
		}
	}

	{
		int s;
		for (s = 0; s < MAX_SHADER_STAGES; s++) {
			if (fogStages[s].rgbGen == CGEN_FOG_BLEND) {
				fogStages[s].stateBits |= fogStages[s].rgbWave.func ? 0x4000 : 0x2000;
			}
			if (globalFogStages[s].rgbGen == CGEN_FOG_BLEND) {
				globalFogStages[s].stateBits |= globalFogStages[s].rgbWave.func ? 0x4000 : 0x2000;
			}
		}
	}

	if ( !shader.fogonly ) {
		if ( fogStages[0].rgbGen == CGEN_FOG_BLEND
			|| ( ( fogStages[0].stateBits & 0xFF ) && fogStages[0].acff ) ) {
			shader.numFogPasses--;
		}
		if ( globalFogStages[0].rgbGen == CGEN_FOG_BLEND
			|| ( ( globalFogStages[0].stateBits & 0xFF ) && globalFogStages[0].acff ) ) {
			shader.numGlobalFogPasses--;
		}
	}

	shader.fogFlags = 0;
	{
		int s;
		for (s = 0; s < MAX_SHADER_STAGES; s++) {
			if (globalFogStages[s].active) {
				globalFogStages[s].fogDensityArm = qtrue;	 
				shader.fogFlags |= 4;
			}
			if (shader.numUnfoggedPasses == shader.numFogPasses && fogStages[s].active) {
				fogStages[s].fogDensityArm = qtrue;			 
				shader.fogFlags |= 2;
			}
		}
	}

	shader.usesGlobalFogIterator =
		(shader.numUnfoggedPasses == 1
		 && unfoggedStages[0].rgbGen == CGEN_LIGHTING_SPHERICAL
		 && unfoggedStages[0].alphaGen == AGEN_IDENTITY) ? 1 : 0;

	ComputeStageIteratorFunc();

	return GeneratePermanentShader();
}


shader_t *R_FindShaderByName( const char *name ) {
	char			strippedName[MAX_QPATH];
	int				hash;
	shadertext_t	*sh;

	if ( (name==NULL) || (name[0] == 0) ) {
		return tr.defaultShader;
	}

	COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName);

	for (sh=hashTable[hash]; sh; sh=sh->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			return sh->shader;
		}
	}

	SMP_ShaderDefaultProbe( "byname-miss", strippedName, "R_FindShaderByName", 0, 0 );
	return tr.defaultShader;
}

static shader_t* R_RealFindShader(const char* name, int lightmapIndex, qboolean mipRawImage, qboolean picmip, qboolean wrapx, qboolean wrapy, int variant) {
	char		strippedName[MAX_QPATH];
	char		fileName[MAX_QPATH];
	int			i, j, hash;
	char		*shaderText;
	image_t		*image;
	shader_t	*sh;

	if ( name[0] == 0 ) {
		SMP_ShaderDefaultProbe( "empty-name", name, "R_RealFindShader", lightmapIndex, variant );
		return tr.defaultShader;
	}

	COM_StripExtension( name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName);

	for (currentShader = hashTable[hash]; currentShader; currentShader = currentShader->next) {
        if (!Q_stricmp(currentShader->name, strippedName))
		{
			break;
		}
	}

	if (currentShader)
	{
		for (sh = currentShader->shader; sh; sh = sh->next)
		{
			// otherwise BOTH must match (raw 0x3D598/0x3D5A4 above).
			if (sh == tr.defaultShader ||
				(sh->lightmapIndex == lightmapIndex && sh->forcedAlphaKey == variant)) {
				if ( sh == tr.defaultShader ) {
					SMP_ShaderDefaultProbe( "chain-default", strippedName, "R_RealFindShader/chain",
											 lightmapIndex, variant );
				}
				return sh;
			}
		}
	}
	else
	{
		currentShader = AddShaderTextToHash(strippedName, hash);
	}

	Com_Memset( &shader, 0, sizeof( shader ) );
    Com_Memset(&unfoggedStages, 0, sizeof(unfoggedStages));
    Com_Memset(&fogStages, 0, sizeof(fogStages));
    Com_Memset(&globalFogStages, 0, sizeof(globalFogStages));
    Com_Memset(&texMods, 0, sizeof(texMods));

    shader.sprite.scale = 1.0;
    shader_noPicMip = qfalse;
    shader_noMipMaps = qfalse;
    shader_force32bit = qfalse;
	Q_strncpyz(shader.name, strippedName, sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;
	shader.forcedAlphaKey = variant;
	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
        for (j = 0; j < NUM_TEXTURE_BUNDLES; j++) {
            unfoggedStages[i].bundle[j].texMods = texMods[i];
		}
	}

	shader.needsNormal = qfalse;
	shader.needsST1 = qtrue;
	shader.needsST2 = qtrue;
	shader.needsColor = qtrue;

	if ( currentShader->text ) {
		shaderText = currentShader->text;
		if ( r_printShaders->integer ) {
			ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
		}

		if ( !ParseShader( &shaderText, picmip ) ) {
			SMP_ShaderDefaultProbe( "parse-failed", name, "R_RealFindShader/text", lightmapIndex, variant );
			shader.defaultShader = qtrue;
		}

		if (shader.lightmapIndex == LIGHTMAP_BY_VERTEX && !(shader.surfaceFlags & SURF_HINT)) {
			unfoggedStages[0].rgbGen = CGEN_EXACT_VERTEX;
		}

		if ( variant ) {
			unfoggedStages[0].alphaGen = AGEN_FORCED_ALPHA;
			unfoggedStages[0].stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			shader.noMerge |= 1;
		}

		sh = FinishShader();
		return sh;
	}

	Q_strncpyz( fileName, name, sizeof( fileName ) );
	COM_DefaultExtension( fileName, sizeof( fileName ), ".tga" );
	{
		int textureMode;

		if (wrapx && wrapy) {
			textureMode = 2;
		} else if (!wrapx && !wrapy) {
			textureMode = haveClampToEdge ? 1 : 0;
		} else if (!wrapx) {
			textureMode = 3;
		} else {
			textureMode = 4;
		}

		image = R_FindImageFileOld(fileName, mipRawImage, picmip, qfalse, textureMode);
	}

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "Couldn't find image for shader %s\n", name );
		SMP_ShaderDefaultProbe( "image-missing", name, "R_RealFindShader/tga", lightmapIndex, variant );
		shader.defaultShader = qtrue;
		return FinishShader();
	}

	if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
		unfoggedStages[0].bundle[0].image[0] = image;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_LIGHTING_SPHERICAL;
		unfoggedStages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_BY_VERTEX ) {
		// SHIPPED CONTENT; do not re-open it without new data.  Retail
		unfoggedStages[0].bundle[0].image[0] = image;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_EXACT_VERTEX;
		unfoggedStages[0].alphaGen = AGEN_SKIP;
		unfoggedStages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_2D ) {
		unfoggedStages[0].bundle[0].image[0] = image;
        unfoggedStages[0].active = qtrue;
        unfoggedStages[0].force32bit = qtrue;
		unfoggedStages[0].rgbGen = CGEN_GLOBAL_COLOR;
		unfoggedStages[0].alphaGen = AGEN_GLOBAL_ALPHA;
		unfoggedStages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if ( shader.lightmapIndex == LIGHTMAP_WHITEIMAGE ) {
		// R_RealFindShader @0x0003D430, case -2 (engine_decomp_all/R_RealFindShader.c:283).
		// Stage 0 is the white image at CGEN_IDENTITY (v8[43] = 2) with a
		// depth-mask-only state (v8[51] = 256); stage 1 is the image at
		// CGEN_IDENTITY_LIGHTING (v8[111] = 1) modulated by the texture.  The two
		// generators were swapped here against retail and then put back.
		unfoggedStages[0].bundle[0].image[0] = tr.whiteImage;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_IDENTITY;
		unfoggedStages[0].stateBits = GLS_DEFAULT;

		unfoggedStages[1].bundle[0].image[0] = image;
		unfoggedStages[1].active = qtrue;
		unfoggedStages[1].rgbGen = CGEN_IDENTITY_LIGHTING;
		unfoggedStages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	} else {
		if ( shader.lightmapIndex & DYNAMIC_LIGHTMAP_BIT ) {
			unfoggedStages[0].bundle[0].image[0] =
				tr.baseLightmaps[shader.lightmapIndex & LIGHTMAP_BASE_MASK];
		} else {
			unfoggedStages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		}
		unfoggedStages[0].bundle[0].isLightmap = qtrue;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_IDENTITY;	
		unfoggedStages[0].stateBits = GLS_DEFAULT;

		unfoggedStages[1].bundle[0].image[0] = image;
		unfoggedStages[1].active = qtrue;
		unfoggedStages[1].rgbGen = CGEN_IDENTITY;
		unfoggedStages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	if ( variant ) {
		unfoggedStages[0].alphaGen = AGEN_FORCED_ALPHA;
		unfoggedStages[0].stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		shader.noMerge |= 1;
	}

	return FinishShader();
}


shader_t* R_FindShader(const char* name, int lightmapIndex, qboolean mipRawImage, qboolean picmip, qboolean wrapx, qboolean wrapy) {
	return R_RealFindShader( name, lightmapIndex, mipRawImage, picmip, wrapx, wrapy, 0 );
}

shader_t* R_FindShaderForceAlpha(const char* name) {
	return R_FindShaderForceAlphaLM( name, -1 );
}

shader_t* R_FindShaderForceAlphaLM(const char* name, int lightmapIndex) {
	shader_t	*sh;

	sh = R_RealFindShader( name, lightmapIndex, qtrue, qtrue, qtrue, qtrue, 0 );
	if ( !sh ) {
		return NULL;
	}
	if ( sh->forcedAlphaShader[0] ) {
		return R_RealFindShader( sh->forcedAlphaShader, lightmapIndex, qtrue, qtrue, qtrue, qtrue, 0 );
	}
	return R_RealFindShader( name, lightmapIndex, qtrue, qtrue, qtrue, qtrue, 1 );
}


qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmapIndex, qtrue, qfalse, qfalse, qfalse );

	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

qhandle_t RE_RegisterShader( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf(PRINT_ALL,  "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qtrue, r_picmip->integer, qtrue, qtrue);

	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


qhandle_t RE_RegisterShaderNoMip( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf(PRINT_ALL,  "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qfalse, qfalse, qfalse, qfalse );

	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

qhandle_t RE_RefreshShaderNoMip(const char* name) {
	shader_t* sh;
	char strippedName[64];
	int hash;

	if (!name || !*name) {
		return 0;
	}

    COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName);

	for (currentShader = hashTable[hash]; currentShader; currentShader = currentShader->next) {
		if (!Q_stricmp(currentShader->name, strippedName)) {
			if (currentShader) {
				image_t* image;

				sh = currentShader->shader;
				image = sh->unfoggedStages[0]->bundle[0].image[0];
				currentShader = NULL;

				if (image) {
					sh->unfoggedStages[0]->bundle[0].image[0] = R_RefreshImageFileOld(
						image->imgName,
						image->numMipmaps,
						image->allowPicmip,
						image->force32bit,
						image->textureMode
					);
				}

				return sh->index;
			}

			break;
		}
	}

    sh = R_FindShader(name, -4, 0, 0, 0, 0);
	if (sh->defaultShader) {
		return 0;
	}

    return sh->index;
}

shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	if ( hShader < 0 ) {
	  ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader ); 
		return tr.defaultShader;
	}
	if ( hShader >= tr.numShaders ) {
		ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

void	R_ShaderList_f (void) {
	int			i;
	int			count;
	shader_t	*pShader;

	ri.Printf (PRINT_ALL, "-----------------------\n");

	count = 0;
	for ( i = 0 ; i < tr.numShaders ; i++ ) {
		int stage;

		if ( ri.Cmd_Argc() > 1 ) {
			pShader = tr.sortedShaders[i];
		} else {
			pShader = tr.shaders[i];
		}

		ri.Printf( PRINT_ALL, "%i ", pShader->numUnfoggedPasses );

		if (pShader->lightmapIndex >= 0 ) {
			ri.Printf (PRINT_ALL, "L ");
		} else {
			ri.Printf (PRINT_ALL, "  ");
		}

		for (stage = 0; pShader->unfoggedStages[stage] && pShader->unfoggedStages[stage]->active; stage++) {
			if (pShader->unfoggedStages[stage]->multitextureEnv == VK_TENV_ADD ) {
				ri.Printf( PRINT_ALL, "MT(a) " );
			} else if (pShader->unfoggedStages[stage]->multitextureEnv == VK_TENV_MODULATE ) {
				ri.Printf( PRINT_ALL, "MT(m) " );
			} else if (pShader->unfoggedStages[stage]->multitextureEnv == VK_TENV_DECAL ) {
				ri.Printf( PRINT_ALL, "MT(d) " );
			} else {
				ri.Printf( PRINT_ALL, "      " );
			}
		}
		if ( pShader->explicitlyDefined ) {
			ri.Printf( PRINT_ALL, "E " );
		} else {
			ri.Printf( PRINT_ALL, "  " );
		}

		if ( pShader->optimalStageIteratorFunc == RB_StageIteratorGeneric ) {
			ri.Printf( PRINT_ALL, "gen " );
		} else if ( pShader->optimalStageIteratorFunc == RB_StageIteratorSky ) {
			ri.Printf( PRINT_ALL, "sky " );
		} else if ( pShader->optimalStageIteratorFunc == RB_StageIteratorLightmappedMultitextureUnfogged ) {
			ri.Printf( PRINT_ALL, "lmmt" );
		} else if ( pShader->optimalStageIteratorFunc == RB_StageIteratorVertexLitTextureUnfogged ) {
			ri.Printf( PRINT_ALL, "vlt " );
		} else {
			ri.Printf( PRINT_ALL, "    " );
		}

		if ( pShader->defaultShader ) {
			ri.Printf (PRINT_ALL,  ": %s (DEFAULTED)\n", pShader->name);
		} else {
			ri.Printf (PRINT_ALL,  ": %s\n", pShader->name);
		}
		count++;
	}
	ri.Printf (PRINT_ALL, "%i total shaders\n", count);
	ri.Printf (PRINT_ALL, "------------------\n");
}


#define	MAX_SHADER_FILES	4096
static void ScanAndLoadShaderFiles( void )
{
    static const char *shaderDirs[] = { "scripts", "shaders" };
    char* buffers[MAX_SHADER_FILES];
    char* p;
    int   numTotal = 0;
    int   d, i;
    long  sum = 0;

    for (d = 0; d < 2 && numTotal < MAX_SHADER_FILES; d++)
    {
        int    numShaders = 0;
        char** shaderFiles = ri.FS_ListFiles(shaderDirs[d], ".shader", &numShaders);

        if (!shaderFiles || !numShaders) {
            if (shaderFiles) {
                ri.FS_FreeFileList(shaderFiles);
            }
            continue;
        }

        for (i = 0; i < numShaders && numTotal < MAX_SHADER_FILES; i++)
        {
            char filename[MAX_QPATH];

            Com_sprintf(filename, sizeof(filename), "%s/%s", shaderDirs[d], shaderFiles[i]);
            ri.Printf(PRINT_ALL, "...loading '%s'\n", filename);
            sum += ri.FS_ReadFile(filename, (void**)&buffers[numTotal]);
            if (!buffers[numTotal]) {
                ri.Error(ERR_DROP, "Couldn't load %s", filename);
            }
            numTotal++;
        }

        ri.FS_FreeFileList(shaderFiles);
    }

    if (!numTotal)
    {
        ri.Printf(PRINT_WARNING, "WARNING: no shader files found\n");
        return;
    }

    s_shaderText = ri.Malloc(sum + numTotal * 2);
    s_shaderText[0] = 0;

    for (i = numTotal - 1; i >= 0; i--) {
        strcat(s_shaderText, "\n");
        p = &s_shaderText[strlen(s_shaderText)];
        strcat(s_shaderText, buffers[i]);
        ri.FS_FreeFile(buffers[i]);
        buffers[i] = p;
    }

    COM_Compress(s_shaderText);
}

static void FindShadersInShaderText(void)
{
	char* p;
	char* oldp;
	char* token;

	if (!s_shaderText) {
		return;
	}

	p = s_shaderText;
    while (1) {
        oldp = p;
        token = COM_ParseExt(&p, qtrue);
        if (token[0] == 0) {
            break;
        }

        if (*token == '{')
        {
            p = oldp;
            SkipBracedSection(&p, 0);
        }
        else
        {
            currentShader = AllocShaderText(token);
            currentShader->text = p;
        }
    }
}

#define SMP_SHDEF_ROW_CAP		200
#define SMP_SHDEF_KEYS			128

typedef struct {
	char	kind[24];
	char	requested[96];
	char	source[80];
} smpShdefKey_t;

void SMP_ShaderDefaultProbe( const char *kind, const char *requested, const char *source, int a, int b ) {
	static smpShdefKey_t	s_keys[SMP_SHDEF_KEYS];
	static int				s_used;
	static int				s_rows;
	static cvar_t			*s_cvar;
	int						i;

	if ( !s_cvar ) {
		s_cvar = ri.Cvar_Get( "smp_shaderProbe", "1", 0 );
	}
	if ( !s_cvar || !s_cvar->integer || !R_ProbeStatsOn() ) {
		return;
	}

	if ( !requested ) requested = "-";
	if ( !source ) source = "-";

	for ( i = 0; i < s_used; i++ ) {
		if ( !Q_stricmp( s_keys[i].kind, kind ) && !Q_stricmp( s_keys[i].requested, requested ) &&
			 !Q_stricmp( s_keys[i].source, source ) ) {
			return;
		}
	}
	if ( s_used >= SMP_SHDEF_KEYS ) {
		if ( s_rows >= 0 ) {
			s_rows = -1;
			ri.Printf( PRINT_ALL, "[SMP:shdef] key table full (%d); further names untracked\n",
					   SMP_SHDEF_KEYS );
		}
		return;
	}
	Q_strncpyz( s_keys[s_used].kind, kind, sizeof( s_keys[s_used].kind ) );
	Q_strncpyz( s_keys[s_used].requested, requested, sizeof( s_keys[s_used].requested ) );
	Q_strncpyz( s_keys[s_used].source, source, sizeof( s_keys[s_used].source ) );
	s_used++;

	if ( s_rows < 0 ) {
		return;
	}
	if ( s_rows >= SMP_SHDEF_ROW_CAP ) {
		s_rows = -1;
		ri.Printf( PRINT_ALL, "[SMP:shdef] capped at %d rows; no further lines\n", SMP_SHDEF_ROW_CAP );
		return;
	}
	s_rows++;
	ri.Printf( PRINT_ALL, "[SMP:shdef] kind=%s req='%s' src=%s a=%d b=%d\n",
			   kind, requested, source, a, b );
}

static void CreateInternalShaders( void ) {
	tr.numShaders = 0;

	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &unfoggedStages, 0, sizeof( unfoggedStages ) );
	Com_Memset( &fogStages, 0, sizeof( fogStages ) );
	Com_Memset( &globalFogStages, 0, sizeof( globalFogStages ) );

	Q_strncpyz( shader.name, "<default>", sizeof( shader.name ) );
	shader.lightmapIndex = LIGHTMAP_NONE;
	unfoggedStages[0].bundle[0].image[0] = tr.defaultImage;
	unfoggedStages[0].active = qtrue;
	unfoggedStages[0].stateBits = GLS_DEFAULT;
	currentShader = FindShaderText(shader.name);
    tr.defaultShader = FinishShader();

    Q_strncpyz(shader.name, "<white>", sizeof(shader.name));
    shader.lightmapIndex = LIGHTMAP_NONE;
    unfoggedStages[0].bundle[0].image[0] = tr.whiteImage;
    unfoggedStages[0].active = qtrue;
    unfoggedStages[0].stateBits = GLS_DEFAULT;
    currentShader = FindShaderText(shader.name);
	FinishShader();

	Q_strncpyz( shader.name, "<stencil shadow>", sizeof( shader.name ) );
    shader.sort = SS_STENCIL_SHADOW;
    shader.lightmapIndex = LIGHTMAP_NONE;
	currentShader = FindShaderText(shader.name);
	tr.shadowShader = FinishShader();
}

static void CreateExternalShaders( void ) {
	tr.flareShader = 0;
}

void R_StartupShaders(void)
{
    ri.Printf(PRINT_ALL, "Initializing Shaders\n");

    currentShader = NULL;
    s_shaderText = NULL;

    Com_Memset(hashTable, 0, sizeof(hashTable));

    ScanAndLoadShaderFiles();

    FindShadersInShaderText();

    R_SetupShaders();
}

void R_ShutdownShaders(void)
{
	shadertext_t* shaderText, * next;
	int hash;

	if (s_shaderText) {
		ri.Free(s_shaderText);
	}
	s_shaderText = NULL;

    for (hash = 0; hash < FILE_HASH_SIZE; hash++) {
        for (shaderText = hashTable[hash]; shaderText; shaderText = next) {
            next = shaderText->next;
            ri.Free(shaderText);
        }
        hashTable[hash] = NULL;
	}
}

void R_SetupShaders(void)
{
	shadertext_t* shaderText;
	int hash;

    ri.Printf(0, "Setting up Shaders\n");
    currentShader = NULL;
    for (hash = 0; hash < FILE_HASH_SIZE; ++hash)
    {
		for (shaderText = hashTable[hash]; shaderText; shaderText = shaderText->next) {
			shaderText->shader = NULL;
		}
    }

    CreateInternalShaders();
    InitStaticShaders();
    CreateExternalShaders();
}
