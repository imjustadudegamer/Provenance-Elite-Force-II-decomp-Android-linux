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
Copyright (C) 2024 the OpenMoHAA team

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


static	world_t		s_worldData;
static	byte		Q_UNUSED_VAR *fileBase;

int			c_subdivisions;
int			c_gridVerts;

static int map_length;
static int nummodels;
static int numShaders;
static int numbrushes;
static int numbrushsides;
static int numFogs;
static int numplanes;
static int Q_UNUSED_VAR num_entities;
static int numnodes;
static int numleafs;
static int numleafsurfaces;
static int numDrawVerts;
static int numDrawIndexes;
static int numDrawSurfaces;
static int numLightDefs;
static int numLightBytes;
static int numVisBytes;
static int numSLights;
static int entLightVisSize;
static int g_nStaticModelData;
static int map_version;
static int g_nStaticModelIndices;
static int g_nStaticModels;


static void HSVtoRGB( float h, float s, float v, float rgb[3] )
{
    int i;
    float f;
    float p, q, t;

    h *= 5;

    i = floor( h );
    f = h - i;

    p = v * ( 1 - s );
    q = v * ( 1 - s * f );
    t = v * ( 1 - s * ( 1 - f ) );

    switch ( i )
    {
    case 0:
        rgb[0] = v;
        rgb[1] = t;
        rgb[2] = p;
        break;
    case 1:
        rgb[0] = q;
        rgb[1] = v;
        rgb[2] = p;
        break;
    case 2:
        rgb[0] = p;
        rgb[1] = v;
        rgb[2] = t;
        break;
    case 3:
        rgb[0] = p;
        rgb[1] = q;
        rgb[2] = v;
        break;
    case 4:
        rgb[0] = t;
        rgb[1] = p;
        rgb[2] = v;
        break;
    case 5:
        rgb[0] = v;
        rgb[1] = p;
        rgb[2] = q;
        break;
    }
}

static	void R_ColorShiftLightingBytes( byte in[3], byte out[3] ) {
    int		r, g, b;

    r = in[0] << tr.overbrightShift;
    g = in[1] << tr.overbrightShift;
    b = in[2] << tr.overbrightShift;
    
    if ( ( r | g | b ) > 255 ) {
        int		max;

        max = r > g ? r : g;
        max = max > b ? max : b;
        r = r * 255 / max;
        g = g * 255 / max;
        b = b * 255 / max;
    }

    out[0] = r;
    out[1] = g;
    out[2] = b;
}

static	void R_ColorShiftLightingBytesAlpha( byte in[4], byte out[4] ) {
    int		r, g, b;

    r = in[0] << tr.overbrightShift;
    g = in[1] << tr.overbrightShift;
    b = in[2] << tr.overbrightShift;
    
    if ( ( r | g | b ) > 255 ) {
        int		max;
        
        max = r > g ? r : g;
        max = max > b ? max : b;
        r = r * 255 / max;
        g = g * 255 / max;
        b = b * 255 / max;
    }

    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = in[3];
}

static void R_RealColorShiftLightingBytes( byte in[4], byte out[4], int copyAlpha ) {
    int		r, g, b, max;
    int		shift = r_mapOverBrightBits->integer - tr.overbrightBits;

    r = in[0] << shift;
    b = in[2] << shift;
    g = in[1] << shift;

    if ( ( b | r | g ) > 255 ) {
        max = r > g ? r : g;		 
        max = max > b ? max : b;	 
        r = r * 255 / max;
        g = g * 255 / max;
        b = b * 255 / max;
    }

    out[0] = r;
    out[1] = g;
    out[2] = b;
    if ( copyAlpha ) {
        out[3] = in[3];
    }
}

static void R_ColorShiftLightingBytesShifted( const byte in[4], byte out[4], int copyAlpha, int shift ) {
    int		r, g, b, max;

    r = in[0] << shift;
    b = in[2] << shift;
    g = in[1] << shift;

    if ( ( b | r | g ) > 255 ) {
        max = r > g ? r : g;
        max = max > b ? max : b;
        r = r * 255 / max;
        g = g * 255 / max;
        b = b * 255 / max;
    }

    out[0] = r;
    out[1] = g;
    out[2] = b;
    if ( copyAlpha ) {
        out[3] = in[3];
    }
}

typedef struct rbShiftBatch_s {
    const byte	*in;
    byte		*out;
    int			inStride;
    int			outStride;
    int			colorInOff;
    int			colorOutOff;
    int			shift;
} rbShiftBatch_t;

static void RB_ColorShiftRange( void *ctx, int begin, int end ) {
    rbShiftBatch_t *c = (rbShiftBatch_t *)ctx;
    int i;

    for ( i = begin; i < end; i++ ) {
        R_ColorShiftLightingBytesShifted( c->in + (size_t)c->inStride * i + c->colorInOff,
                                          c->out + (size_t)c->outStride * i + c->colorOutOff,
                                          0, c->shift );
    }
}

#define RB_SHIFT_MIN_CHUNK 16384

static void RB_ColorShiftRun( const byte *in, byte *out, int count, int inStride, int outStride,
                              int colorInOff, int colorOutOff, int minChunk ) {
    rbShiftBatch_t c;

    if ( count <= 0 || !in || !out ) {
        return;
    }
    c.in = in;
    c.out = out;
    c.inStride = inStride;
    c.outStride = outStride;
    c.colorInOff = colorInOff;
    c.colorOutOff = colorOutOff;
    c.shift = r_mapOverBrightBits->integer - tr.overbrightBits;

    JobPool_ParallelRange( count, minChunk, RB_ColorShiftRange, &c );
}

typedef struct rbLmExpand_s {
    const byte	*in;			
    byte		*out;			
    int			outRowStride;	
    int			outRowOffset;	
} rbLmExpand_t;

static void RB_LightmapExpandRows( void *vctx, int begin, int end ) {
    const rbLmExpand_t	*c = (const rbLmExpand_t *)vctx;
    byte		*in;	 
    byte		*out;
    int			y, x;

    for ( y = begin; y < end; y++ ) {
        in = (byte *)( c->in + (size_t)y * LIGHTMAP_WIDTH * 3 );
        out = c->out + 4 * ( (size_t)y * c->outRowStride + c->outRowOffset );
        for ( x = 0; x < LIGHTMAP_WIDTH; x++, in += 3, out += 4 ) {
            R_ColorShiftLightingBytes( in, out );
            out[3] = 255;
        }
    }
}

#define RB_LM_EXPAND_MIN_CHUNK 16

static void RB_LightmapExpandRun( const byte *in, byte *out, int outRowStride, int outRowOffset ) {
    rbLmExpand_t c;

    c.in = in;
    c.out = out;
    c.outRowStride = outRowStride;
    c.outRowOffset = outRowOffset;

    JobPool_ParallelRange( LIGHTMAP_HEIGHT, RB_LM_EXPAND_MIN_CHUNK, RB_LightmapExpandRows, &c );
}

typedef struct {
    const byte	*in;
    byte		*out;
    int			inStride;
    int			outStride;
} rbRecordConv_t;

#define RB_RECORD_MIN_CHUNK 8192

static void RB_ConvertLightingSurfsRange( void *ctx, int begin, int end ) {
    rbRecordConv_t	*c = (rbRecordConv_t *)ctx;
    const byte		*in = c->in + (size_t)c->inStride * begin;
    lightingSurf_t	*out = (lightingSurf_t *)( c->out + (size_t)c->outStride * begin );
    int				i;

    for ( i = begin; i < end; i++, in += c->inStride, out++ ) {
        out->lightmapNum	= LittleLong( *(const int *)( in + 0 ) );
        out->lightmapX		= LittleLong( *(const int *)( in + 4 ) );
        out->lightmapY		= LittleLong( *(const int *)( in + 8 ) );
        out->lightGroupNum	= LittleLong( *(const int *)( in + 12 ) );
    }
}

static void RB_ConvertLightingVertSurfsRange( void *ctx, int begin, int end ) {
    rbRecordConv_t		*c = (rbRecordConv_t *)ctx;
    const byte			*in = c->in + (size_t)c->inStride * begin;
    lightingVertSurf_t	*out = (lightingVertSurf_t *)( c->out + (size_t)c->outStride * begin );
    int					i;

    for ( i = begin; i < end; i++, in += c->inStride, out++ ) {
        out->firstLightingVert	= LittleLong( *(const int *)( in + 4 ) );
        out->lightGroupNum		= LittleLong( *(const int *)( in + 0 ) );
    }
}

static void RB_ConvertBaseLightingSurfsRange( void *ctx, int begin, int end ) {
    rbRecordConv_t		*c = (rbRecordConv_t *)ctx;
    const byte			*in = c->in + (size_t)c->inStride * begin;
    baseLightingSurf_t	*out = (baseLightingSurf_t *)( c->out + (size_t)c->outStride * begin );
    int					i;

    for ( i = begin; i < end; i++, in += c->inStride, out++ ) {
        out->baseLightmapNum		= LittleLong( *(const int *)( in + 0 ) );
        out->baseLightmapX			= LittleLong( *(const int *)( in + 4 ) );
        out->baseLightmapY			= LittleLong( *(const int *)( in + 8 ) );
        out->baseLightmapWidth		= LittleLong( *(const int *)( in + 12 ) );
        out->baseLightmapHeight		= LittleLong( *(const int *)( in + 16 ) );
        out->firstLightingSurface	= LittleLong( *(const int *)( in + 24 ) );
        out->numLightingSurfaces	= LittleLong( *(const int *)( in + 28 ) );
        out->baseLightingVerts		= LittleLong( *(const int *)( in + 32 ) );
        out->firstLightingVertSurface	= LittleLong( *(const int *)( in + 36 ) );
        out->numLightingVertSurfaces	= LittleLong( *(const int *)( in + 40 ) );
        out->lastUpdatedFrame		= -1;	 
    }
}

static	void R_LoadLightmaps(gamelump_t* l) {
    byte		*buf, *buf_p;
    int			len;
    MAC_STATIC byte		image[LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*4];
    int			i, j;
    float maxIntensity = 0;

    tr.numLightmaps = 0;

    len = l->length;
    if (!len) {
        return;
    }
    buf = l->buffer;

    R_IssuePendingRenderCommands();

    tr.numLightmaps = len / LIGHTMAP_SIZE;
    
	if ( r_vertexLight->integer || vkConfig.hardwareType == GLHW_PERMEDIA2 ) {
        return;
    }

    for ( i = 0 ; i < tr.numLightmaps ; i++ ) {
        buf_p = buf + i * LIGHTMAP_SIZE;

        if ( r_lightmap->integer == 2 )
        {	
            for ( j = 0; j < LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT; j++ )
            {
                float r = buf_p[j*3+0];
                float g = buf_p[j*3+1];
                float b = buf_p[j*3+2];
                float intensity;
                float out[3];

                intensity = 0.33f * r + 0.685f * g + 0.063f * b;

                if ( intensity > 255 )
                    intensity = 1.0f;
                else
                    intensity /= 255.0f;

                if ( intensity > maxIntensity )
                    maxIntensity = intensity;

                HSVtoRGB( intensity, 1.00, 0.50, out );

                image[j*4+0] = out[0] * 255;
                image[j*4+1] = out[1] * 255;
                image[j*4+2] = out[2] * 255;
                image[j*4+3] = 255;

            }
        } else {
            RB_LightmapExpandRun( buf_p, image, LIGHTMAP_WIDTH, 0 );
        }
        tr.lightmaps[i] = R_CreateImageOld(va("*lightmap%d", i), image,
            LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 0, 1, qfalse, qfalse, qfalse, qfalse, 0);
    }

    if ( r_lightmap->integer == 2 )	{
        ri.Printf( PRINT_ALL, "Brightest lightmap value: %d\n", ( int ) ( maxIntensity * 255 ) );
    }
}


void		RE_SetWorldVisData( const byte *vis ) {
    tr.externalVisData = vis;
}


static	void R_LoadVisibility(gamelump_t* l) {
    int		len;
    byte	*buf;

    len = ( s_worldData.numClusters + 63 ) & ~63;
    s_worldData.novis = ri.Hunk_Alloc( len, h_dontcare );
    Com_Memset( s_worldData.novis, 0xff, len );

    len = l->length;
    if (!len) {
        return;
    }
    buf = l->buffer;

    s_worldData.numClusters = LittleLong( ((int *)buf)[0] );
    s_worldData.clusterBytes = LittleLong( ((int *)buf)[1] );

    if ( tr.externalVisData ) {
        s_worldData.vis = tr.externalVisData;
    } else {
        byte	*dest;

        dest = ri.Hunk_Alloc( len - 8, h_dontcare );
        Com_Memcpy( dest, buf + 8, len - 8 );
        s_worldData.vis = dest;
    }
}

typedef char mapspherel_t_size_check[(sizeof(mapspherel_t) == 104) ? 1 : -1];

void R_LoadSphereLights(gamelump_t* l) {
    int				i, j;
    mapspherel_t* at;
    spherel_t* light;

    tr.numSLights = 0;
    tr.sphereLightVis = qfalse;

    if (!l->buffer || !l->length) {
        return;
    }

    if (l->length % sizeof(mapspherel_t)) {
        Com_Error(ERR_DROP, "LoadMap: funny lump size in spherelight data for %s\n", s_worldData.name);
    }

    tr.numSLights = l->length / sizeof(mapspherel_t);

    if (tr.numSLights > MAX_MAP_SPHERE_LIGHTS_EF2) {
        Com_Error(ERR_DROP, "LoadMap: Too many spherelights on map %s, limit is %d\n", s_worldData.name, MAX_MAP_SPHERE_LIGHTS_EF2);
    }

    at = l->buffer;
    light = tr.sLights;

    for (i = 0; i < tr.numSLights; at++, light++, i++) {
        int iLeaf;


        VectorCopy(at->origin, light->origin);
        VectorCopy(at->color, light->color);
        VectorCopy(at->targetNormal, light->targetNormal);

        for (j = 0; j < 3; j++) {
            light->origin[j] = LittleFloat(light->origin[j]);
            light->color[j] = LittleFloat(light->color[j]);
            light->targetNormal[j] = LittleFloat(light->targetNormal[j]);
        }

        light->needs_trace         = LittleLong(at->needs_trace);
        light->lensflare           = LittleLong(at->lensflare);
        light->type                = LittleLong(at->type);
        light->group_num           = LittleLong(at->group_num);
        light->dotProduct_hotspot  = LittleFloat(at->dotProduct_hotspot);
        light->dotProduct_penumbra = LittleFloat(at->dotProduct_penumbra);
        light->falloff_start_dist  = LittleFloat(at->falloff_start_dist);
        light->falloff_end_dist    = LittleFloat(at->falloff_end_dist);
        light->curveOffset         = LittleFloat(at->curveOffset);
        light->spherical_ambient   = LittleFloat(at->spherical_ambient);
        light->brightness          = LittleFloat(at->brightness);

        light->exclusiveLightNameCount = LittleLong(at->exclusiveLightNameCount);
        for (j = 0; j < light->exclusiveLightNameCount; j++) {
            light->exclusiveLightNameHashList[j] = LittleLong(at->exclusiveLightNameHashList[j]);
        }

        iLeaf = LittleLong(at->leaf);
        light->leaf = &s_worldData.nodes[s_worldData.numDecisionNodes + iLeaf];
    }
}

void R_LoadSphereLightVis(gamelump_t* l) {
    int i;
    int j;
    int h;
    mnode_t* node;
    int numentries;
    int* entries;

    if (!l->buffer || !l->length || !tr.numSLights) {
        return;
    }

    if (l->length % 4) {
        ri.Error(ERR_DROP, "LoadMap: funny lump size in spherelight vis data for %s\n",
            s_worldData.name);
    }
    else {
        entries = l->buffer;
        numentries = l->length / sizeof(int);

        if (numentries) {
            if (numentries != s_worldData.numnodes - s_worldData.numDecisionNodes) {
                tr.sphereLightVis = qtrue;
                for (i = 0; i < numentries; ++i)
                    entries[i] = LittleLong(entries[i]);

                for (node = &s_worldData.nodes[s_worldData.numDecisionNodes]; numentries; node++) {
                    j = 0;
                    if (entries[0] != -1) {
                        do
                            ++j;
                        while (entries[j] != -1);
                    }
                    if (j) {
                        node->lights = ri.Hunk_Alloc(j * sizeof(spherel_t*), h_dontcare);
                        node->numlights = j;
                        for (h = 0; h < j; h++) {
                            int idx = entries[h];

                            node->lights[h] = &tr.sLights[idx];
                        }
                        entries += j + 1;
                        numentries = numentries - 1 - j;
                    }
                    else {
                        numentries--;
                        entries++;
                    }
                }
            }
        }
    }
}


typedef char staticLodModel_stride_pin[( sizeof( staticLodModel_t ) == 44 ) ? 1 : -1];

void R_LoadStaticLodModels(gamelump_t* l) {
    int						i;
    int						count;
    staticLodModel_t*		records;
    const byte*				in;

    if (!l->buffer || !l->length) {
        s_worldData.numStaticLodModels = 0;
        s_worldData.staticLodModels = NULL;
        return;
    }

    if (l->length % sizeof(staticLodModel_t)) {
        ri.Error(ERR_DROP, "LoadMap: funny lump size in static lod models for %s\n",
            s_worldData.name);
        return;
    }

    count = l->length / sizeof(staticLodModel_t);

    if (!count) {
        s_worldData.numStaticLodModels = 0;
        s_worldData.staticLodModels = NULL;
        return;
    }

    s_worldData.numStaticLodModels = count;
    s_worldData.staticLodModels = ri.Hunk_Alloc(count * sizeof(staticLodModel_t), h_dontcare);
    records = s_worldData.staticLodModels;
    in = l->buffer;

    for (i = 0; i < count; i++, in += sizeof(staticLodModel_t), records++) {
        records->totalNumberOfVerts	= LittleLong(*(const int*)(in + 0));
        records->origin[0]			= LittleFloat(*(const float*)(in + 4));
        records->origin[1]			= LittleFloat(*(const float*)(in + 8));
        records->origin[2]			= LittleFloat(*(const float*)(in + 12));
        records->startDistance		= LittleFloat(*(const float*)(in + 16));
        records->startPerCentVerts	= LittleFloat(*(const float*)(in + 20));
        records->stopDistance		= LittleFloat(*(const float*)(in + 24));
        records->stopPerCentVerts	= LittleFloat(*(const float*)(in + 28));
        records->alphaFadeDistance	= LittleFloat(*(const float*)(in + 32));
        records->lodNumberOfVerts	= 0;	 
        records->frameCount			= -1;	 
    }
}



#define	LIGHTMAP_BYTES	( LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 3 )

void R_LoadBaseLightmaps( gamelump_t *l ) {
    int			i, j;
    int			num;
    const byte	*in;
    byte		image[LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4];

    s_worldData.baseLightmapData = NULL;
    tr.numBaseLightmaps = 0;
    s_worldData.baseLightmapImage = NULL;
    s_worldData.baseLightmapFrame = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }
    if ( r_vertexLight->integer || vkConfig.hardwareType == GLHW_PERMEDIA2 ) {
        return;
    }

    num = l->length / LIGHTMAP_BYTES;
    tr.numBaseLightmaps = num;

    s_worldData.baseLightmapData = ri.Hunk_Alloc( num * LIGHTMAP_BYTES, h_dontcare );
    in = l->buffer;
    RB_ColorShiftRun( in, s_worldData.baseLightmapData, num * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT,
                      3, 3, 0, 0, RB_SHIFT_MIN_CHUNK );

    s_worldData.baseLightmapImage = ri.Hunk_Alloc( num * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * 4, h_dontcare );

    for ( j = 0; j < num; j++ ) {
        /* The create carries the tile's own static base lightmap, not the white
           fill: this is the write the composite's first pass makes, and a tile
           it never reaches must not draw unlit-white. */
        {
            const byte	*src = s_worldData.baseLightmapData + j * LIGHTMAP_BYTES;
            byte		*dst = image;
            int			k;

            for ( k = 0; k < LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT; k++, src += 3, dst += 4 ) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
            }
        }

        tr.baseLightmaps[j] = R_CreateImageOld( va( "*lightmapD%i", j ), image,
            LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 0, -1, qfalse, qfalse, qfalse, qfalse,
            0 );
    }

    s_worldData.baseLightmapFrame = ri.Hunk_Alloc( num * sizeof( int ), h_dontcare );
    for ( i = 0; i < num; i++ ) {
        s_worldData.baseLightmapFrame[i] = 0;
    }
}

void R_LoadContLightmaps( gamelump_t *l ) {
    int			num;
    const byte	*in;

    tr.numContLightmaps = 0;
    s_worldData.contLightmapData = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }

    num = l->length / LIGHTMAP_BYTES;
    tr.numContLightmaps = num;

    if ( r_vertexLight->integer || vkConfig.hardwareType == GLHW_PERMEDIA2 ) {
        return;
    }

    s_worldData.contLightmapData = ri.Hunk_Alloc( num * LIGHTMAP_BYTES, h_dontcare );
    in = l->buffer;
    RB_ColorShiftRun( in, s_worldData.contLightmapData, num * LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT,
                      3, 3, 0, 0, RB_SHIFT_MIN_CHUNK );
}

void R_LoadBaseLightingVerts( gamelump_t *l ) {
    int			num;
    const byte	*in;

    s_worldData.baseLightingVerts = NULL;
    s_worldData.baseLightingVertsWork = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }

    if ( l->length % 3 ) {
        ri.Error( ERR_DROP, "LoadMap: funny lump size in base lighting vert data for %s\n",
            s_worldData.name );
        return;
    }

    num = l->length / 3;
    if ( !num ) {
        return;
    }

    s_worldData.baseLightingVerts = ri.Hunk_Alloc( 3 * num, h_dontcare );
    in = l->buffer;
    RB_ColorShiftRun( in, s_worldData.baseLightingVerts, num, 3, 3, 0, 0, RB_SHIFT_MIN_CHUNK );

    // retail never initialises this copy; R_CalcSurfLightVerts fills the range it uses
    s_worldData.baseLightingVertsWork = ri.Hunk_Alloc( 3 * num, h_dontcare );
}

void R_LoadContLightingVerts( gamelump_t *l ) {
    int			num;
    const byte	*in;

    s_worldData.contLightingVerts = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }

    if ( l->length % 3 ) {
        ri.Error( ERR_DROP, "LoadMap: funny lump size in contribution lighting vert data for %s\n",
            s_worldData.name );
        return;
    }

    num = l->length / 3;
    if ( !num ) {
        return;
    }

    s_worldData.contLightingVerts = ri.Hunk_Alloc( 3 * num, h_dontcare );
    in = l->buffer;
    RB_ColorShiftRun( in, s_worldData.contLightingVerts, num, 3, 3, 0, 0, RB_SHIFT_MIN_CHUNK );
}

void R_LoadBaseLightingSurfs( gamelump_t *l ) {
    int					count;
    baseLightingSurf_t	*out;
    const byte			*in;

    s_worldData.numBaseLightingSurfs = 0;
    s_worldData.baseLightingSurfs = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }

    if ( l->length % sizeof( baseLightingSurf_t ) ) {
        ri.Error( ERR_DROP, "LoadMap: funny lump size in base lighting surf data for %s\n",
            s_worldData.name );
        return;
    }

    count = l->length / sizeof( baseLightingSurf_t );
    if ( !count ) {
        return;
    }

    s_worldData.numBaseLightingSurfs = count;
    s_worldData.baseLightingSurfs = ri.Hunk_Alloc( count * sizeof( baseLightingSurf_t ), h_dontcare );
    out = s_worldData.baseLightingSurfs;
    in = l->buffer;

    {
        rbRecordConv_t c;
        c.in = in;
        c.out = (byte *)out;
        c.inStride = (int)sizeof( baseLightingSurf_t );
        c.outStride = (int)sizeof( baseLightingSurf_t );
        JobPool_ParallelRange( count, RB_RECORD_MIN_CHUNK, RB_ConvertBaseLightingSurfsRange, &c );
    }
}

void R_LoadLightingSurfs( gamelump_t *l ) {
    int				count;
    lightingSurf_t	*out;
    const byte		*in;

    s_worldData.numLightingSurfs = 0;
    s_worldData.lightingSurfs = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }

    if ( l->length % sizeof( lightingSurf_t ) ) {
        ri.Error( ERR_DROP, "LoadMap: funny lump size in lighting surf data for %s\n",
            s_worldData.name );
        return;
    }

    count = l->length / sizeof( lightingSurf_t );
    if ( !count ) {
        return;
    }

    s_worldData.numLightingSurfs = count;
    s_worldData.lightingSurfs = ri.Hunk_Alloc( count * sizeof( lightingSurf_t ), h_dontcare );
    out = s_worldData.lightingSurfs;
    in = l->buffer;

    {
        rbRecordConv_t c;
        c.in = in;
        c.out = (byte *)out;
        c.inStride = (int)sizeof( lightingSurf_t );
        c.outStride = (int)sizeof( lightingSurf_t );
        JobPool_ParallelRange( count, RB_RECORD_MIN_CHUNK, RB_ConvertLightingSurfsRange, &c );
    }

    RB_ColorShiftRun( l->buffer, (byte *)s_worldData.lightingSurfs, count,
                      (int)sizeof( lightingSurf_t ), (int)sizeof( lightingSurf_t ),
                      (int)offsetof( lightingSurf_t, color ), (int)offsetof( lightingSurf_t, color ),
                      1024 );
}

void R_LoadLightingVertSurfs( gamelump_t *l ) {
    int					count;
    lightingVertSurf_t	*out;
    const byte			*in;

    s_worldData.numLightingVertSurfs = 0;
    s_worldData.lightingVertSurfs = NULL;

    if ( !l->buffer || !l->length ) {
        return;
    }

    if ( l->length % sizeof( lightingVertSurf_t ) ) {
        ri.Error( ERR_DROP, "LoadMap: funny lump size in lighting vert surf data for %s\n",
            s_worldData.name );
        return;
    }

    count = l->length / sizeof( lightingVertSurf_t );
    if ( !count ) {
        return;
    }

    s_worldData.numLightingVertSurfs = count;
    s_worldData.lightingVertSurfs = ri.Hunk_Alloc( count * sizeof( lightingVertSurf_t ), h_dontcare );
    out = s_worldData.lightingVertSurfs;
    in = l->buffer;

    {
        rbRecordConv_t c;
        c.in = in;
        c.out = (byte *)out;
        c.inStride = (int)sizeof( lightingVertSurf_t );
        c.outStride = (int)sizeof( lightingVertSurf_t );
        JobPool_ParallelRange( count, RB_RECORD_MIN_CHUNK, RB_ConvertLightingVertSurfsRange, &c );
    }

    RB_ColorShiftRun( l->buffer, (byte *)s_worldData.lightingVertSurfs, count,
                      (int)sizeof( lightingVertSurf_t ), (int)sizeof( lightingVertSurf_t ),
                      (int)offsetof( lightingVertSurf_t, color ), (int)offsetof( lightingVertSurf_t, color ),
                      1024 );
}


static shader_t *ShaderForShaderNum( int shaderNum, int lightmapNum ) {
    shader_t	*shader;
    dshader_t	*dsh;

    if ( shaderNum < 0 || shaderNum >= s_worldData.numShaders ) {
        ri.Error( ERR_DROP, "ShaderForShaderNum: bad num %i", shaderNum );
    }
    dsh = &s_worldData.shaders[ shaderNum ];

    if ( r_vertexLight->integer || vkConfig.hardwareType == GLHW_PERMEDIA2 ) {
        lightmapNum = LIGHTMAP_BY_VERTEX;
    }

    if ( ( r_fullbright->integer
            || ( !tr.numLightmaps && !tr.numBaseLightmaps && !tr.numContLightmaps ) )
        && lightmapNum != LIGHTMAP_BY_VERTEX ) {
        lightmapNum = LIGHTMAP_WHITEIMAGE;
    }

    shader = R_FindShader( dsh->shader, lightmapNum, qtrue, qtrue, qtrue, qtrue);

    if ( shader->defaultShader ) {
        SMP_ShaderDefaultProbe( "bsp-surface", dsh->shader, s_worldData.name, shaderNum, lightmapNum );
        return tr.defaultShader;
    }

    return shader;
}




static void ParseFace( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes  ) {
    int			i, j;
    srfSurfaceFace_t	*cv;
    int			numPoints, numIndexes;
    int			lightmapNum;
    int			sfaceSize, ofsIndexes;
    static surfaceType_t skipData = SF_SKIP;

    lightmapNum = LittleLong( ds->lightmapNum );

    surf->shader = ShaderForShaderNum(LittleLong(ds->shaderNum), lightmapNum);
    if ( r_singleShader->integer && !surf->shader->isSky ) {
        surf->shader = tr.defaultShader;
    }

    if (surf->shader->surfaceFlags & SURF_NODRAW) {
        surf->data = &skipData;
        return;
    }

    surf->fogIndex = LittleLong( ds->fogNum ) + 1;

    numPoints = LittleLong( ds->numVerts );
    if (numPoints > MAX_FACE_POINTS) {
        ri.Printf( PRINT_WARNING, "WARNING: MAX_FACE_POINTS exceeded: %i\n", numPoints);
        numPoints = MAX_FACE_POINTS;
        surf->shader = tr.defaultShader;
    }

    numIndexes = LittleLong( ds->numIndexes );

    sfaceSize = ( int ) ( size_t ) &((srfSurfaceFace_t *)0)->points[numPoints];
    ofsIndexes = sfaceSize;
    sfaceSize += sizeof( int ) * numIndexes;

    cv = ri.Hunk_Alloc( sfaceSize, h_dontcare );
    cv->surfaceType = SF_FACE;
    cv->numPoints = numPoints;
    cv->numIndices = numIndexes;
    cv->ofsIndices = ofsIndexes;

    verts += LittleLong( ds->firstVert );
    for ( i = 0 ; i < numPoints ; i++ ) {
        for ( j = 0 ; j < 3 ; j++ ) {
            cv->points[i][j] = LittleFloat( verts[i].xyz[j] );
        }
        for ( j = 0 ; j < 2 ; j++ ) {
            cv->points[i][3+j] = LittleFloat( verts[i].st[j] );
            cv->points[i][5+j] = LittleFloat( verts[i].lightmap[j] );
        }
        R_ColorShiftLightingBytesAlpha( verts[i].color, (byte *)&cv->points[i][7] );
    }

    indexes += LittleLong( ds->firstIndex );
    for ( i = 0 ; i < numIndexes ; i++ ) {
        ((int *)((byte *)cv + cv->ofsIndices ))[i] = LittleLong( indexes[ i ] );
    }

    for ( i = 0 ; i < 3 ; i++ ) {
        cv->plane.normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
    }
    cv->plane.dist = DotProduct( cv->points[0], cv->plane.normal );
    SetPlaneSignbits( &cv->plane );
    cv->plane.type = PlaneTypeForNormal( cv->plane.normal );

    // lmVecs/lmInverseVecs) is not part of retail's 84-byte record and is gone.
    cv->baseLightingSurface = LittleLong( ds->baseLightingSurface );

    surf->data = (surfaceType_t *)cv;
}


static void ParseMesh ( dsurface_t *ds, drawVert_t *verts, msurface_t *surf ) {
    srfGridMesh_t	*grid;
    int				i, j;
    int				width, height, numPoints;
    MAC_STATIC drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE];
    int				lightmapNum;
    vec3_t			bounds[2];
    vec3_t			tmpVec;
    float           subdivisions;
    static surfaceType_t	skipData = SF_SKIP;

    lightmapNum = LittleLong( ds->lightmapNum );

    surf->fogIndex = LittleLong( ds->fogNum ) + 1;

    surf->shader = ShaderForShaderNum(LittleLong(ds->shaderNum), lightmapNum);
    if ( r_singleShader->integer && !surf->shader->isSky ) {
        surf->shader = tr.defaultShader;
    }

    if ( s_worldData.shaders[ LittleLong( ds->shaderNum ) ].surfaceFlags & SURF_NODRAW ) {
        surf->data = &skipData;
        return;
    }

    width = LittleLong( ds->patchWidth );
    height = LittleLong( ds->patchHeight );

    verts += LittleLong( ds->firstVert );
    numPoints = width * height;
    for ( i = 0 ; i < numPoints ; i++ ) {
        for ( j = 0 ; j < 3 ; j++ ) {
            points[i].xyz[j] = LittleFloat( verts[i].xyz[j] );
            points[i].normal[j] = LittleFloat( verts[i].normal[j] );
        }
        for ( j = 0 ; j < 2 ; j++ ) {
            points[i].st[j] = LittleFloat( verts[i].st[j] );
            points[i].lightmap[j] = LittleFloat( verts[i].lightmap[j] );
        }
        R_ColorShiftLightingBytesAlpha( verts[i].color, points[i].color );
    }

    subdivisions = LittleFloat(ds->subdivisions);
    if (subdivisions) {
        grid = R_SubdividePatchToGrid(width, height, subdivisions * (r_subdivisions->value / 10.0), points);
    } else if (surf->shader->subdivisions) {
        grid = R_SubdividePatchToGrid(width, height, surf->shader->subdivisions * (r_subdivisions->value / 10.0), points);
    } else {
        grid = R_SubdividePatchToGrid(width, height, r_subdivisions->value, points);
    }
    surf->data = (surfaceType_t *)grid;

    for ( i = 0 ; i < 3 ; i++ ) {
        bounds[0][i] = LittleFloat( ds->lightmapVecs[0][i] );
        bounds[1][i] = LittleFloat( ds->lightmapVecs[1][i] );
    }
    VectorAdd( bounds[0], bounds[1], bounds[1] );
    VectorScale( bounds[1], 0.5f, grid->lodOrigin );
    VectorSubtract( bounds[0], grid->lodOrigin, tmpVec );
    grid->lodRadius = VectorLength( tmpVec );

    // (lightmapOffset/lmX/lmY/lmWidth/lmHeight/lmData) is not part of retail's 92-byte
    grid->baseLightingSurface = LittleLong( ds->baseLightingSurface );
}

static void ParseTriSurfMesh( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes,
							  surfaceType_t surfaceType ) {
    srfTriangles_t	*tri;
    int				i, j;
    int				numVerts, numIndexes;
    int				lightmapNum, shaderNum;
    vec3_t			bounds;
    int				*indexSrc;

    surf->fogIndex = LittleLong( ds->fogNum ) + 1;

    lightmapNum = LittleLong( ds->lightmapNum );             
    shaderNum   = LittleLong( ds->shaderNum );               
    if ( ds->lightmapNum < 0 ) {                             
        surf->shader = ShaderForShaderNum( shaderNum, LIGHTMAP_BY_VERTEX );    
    } else {
        surf->shader = ShaderForShaderNum( shaderNum, lightmapNum );           
    }
    if ( r_singleShader->integer && !surf->shader->isSky ) {   
        surf->shader = tr.defaultShader;                       
    }

    numVerts   = LittleLong( ds->numVerts );                 
    numIndexes = LittleLong( ds->numIndexes );               

    // (lwz r5, 4(r20) @0x5160 loads surf->shader, not surf->shader->name).  Retail bug,
    if ( numVerts > 1200 ) {
        // verbatim: ALL=0 DEVELOPER=1 DEVELOPER_2=2 WARNING=3 ERROR=4), not PRINT_ERROR.
        ri.Printf( PRINT_WARNING, "Too many verts in tri_surf with shader %s\n",
                   (char *)surf->shader );
    }

    tri = ri.Hunk_Alloc( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] )
        + numIndexes * sizeof( tri->indexes[0] ), h_dontcare );
    tri->surfaceType = surfaceType;                         
    tri->numVerts    = numVerts;                             
    tri->numIndexes  = numIndexes;                           
    tri->verts       = (drawVert_t *)( tri + 1 );            
    tri->indexes     = (int *)( (byte *)tri->verts + 48 * tri->numVerts );   

    surf->data = (surfaceType_t *)tri;                       

    ClearBounds( tri->bounds[0], tri->bounds[1] );           
    verts += LittleLong( ds->firstVert );                    
    for ( i = 0 ; i < numVerts ; i++ ) {
        for ( j = 0 ; j < 3 ; j++ ) {
            tri->verts[i].xyz[j]    = LittleFloat( verts[i].xyz[j]    );   
            tri->verts[i].normal[j] = LittleFloat( verts[i].normal[j] );   
        }
        AddPointToBounds( tri->verts[i].xyz, tri->bounds[0], tri->bounds[1] );   
        for ( j = 0 ; j < 2 ; j++ ) {
            tri->verts[i].st[j] = LittleFloat( verts[i].st[j] );           
        }

        tri->verts[i].collapseMap = LittleLong( verts[i].collapseMap );     
        tri->verts[i].lodExtra    = LittleFloat( verts[i].lodExtra );       
        tri->verts[i].lightmap[0] = LittleFloat( verts[i].lightmap[0] );    
        tri->verts[i].lightmap[1] = LittleFloat( verts[i].lightmap[1] );    

        R_RealColorShiftLightingBytes( verts[i].color, tri->verts[i].color, qtrue );   
    }

    VectorSubtract( tri->bounds[1], tri->bounds[0], bounds );    
    tri->localOrigin[0] = 0.5f * bounds[0] + tri->bounds[0][0];  
    tri->localOrigin[1] = 0.5f * bounds[1] + tri->bounds[0][1];  
    tri->localOrigin[2] = 0.5f * bounds[2] + tri->bounds[0][2];  
    tri->radius         = VectorLength( bounds );                

    indexSrc = indexes + LittleLong( ds->firstIndex );           
    for ( i = 0 ; i < numIndexes ; i++ ) {
        tri->indexes[i] = LittleLong( indexSrc[i] );
        if ( tri->indexes[i] < 0 || tri->indexes[i] >= numVerts ) {
            ri.Error( ERR_DROP, "Bad index in triangle surface" );    
        }
    }

    tri->baseLightingSurface = LittleLong( ds->baseLightingSurface );   
}

static void ParseTriSurf( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes ) {
    ParseTriSurfMesh( ds, verts, surf, indexes, SF_TRIANGLES );
}

static void ParseTerrainSurf( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes ) {
    ParseTriSurfMesh( ds, verts, surf, indexes, SF_TERRAIN );
}

static void ParseFlare( dsurface_t *ds, drawVert_t *verts, msurface_t *surf, int *indexes ) {
    srfFlare_t		*flare;
    int				i;

    surf->fogIndex = LittleLong( ds->fogNum ) + 1;

    surf->shader = ShaderForShaderNum(LittleLong(ds->shaderNum), LIGHTMAP_BY_VERTEX);
    if ( r_singleShader->integer && !surf->shader->isSky ) {
        surf->shader = tr.defaultShader;
    }

    flare = ri.Hunk_Alloc( sizeof( *flare ), h_dontcare );
    flare->surfaceType = SF_FLARE;

    surf->data = (surfaceType_t *)flare;

    for ( i = 0 ; i < 3 ; i++ ) {
        flare->origin[i] = LittleFloat( ds->lightmapOrigin[i] );
        flare->color[i] = LittleFloat( ds->lightmapVecs[0][i] );
        flare->normal[i] = LittleFloat( ds->lightmapVecs[2][i] );
    }
}


int R_MergedWidthPoints(srfGridMesh_t *grid, int offset) {
    int i, j;

    for (i = 1; i < grid->width-1; i++) {
        for (j = i + 1; j < grid->width-1; j++) {
            if ( fabs(grid->verts[i + offset].xyz[0] - grid->verts[j + offset].xyz[0]) > .1) continue;
            if ( fabs(grid->verts[i + offset].xyz[1] - grid->verts[j + offset].xyz[1]) > .1) continue;
            if ( fabs(grid->verts[i + offset].xyz[2] - grid->verts[j + offset].xyz[2]) > .1) continue;
            return qtrue;
        }
    }
    return qfalse;
}

int R_MergedHeightPoints(srfGridMesh_t *grid, int offset) {
    int i, j;

    for (i = 1; i < grid->height-1; i++) {
        for (j = i + 1; j < grid->height-1; j++) {
            if ( fabs(grid->verts[grid->width * i + offset].xyz[0] - grid->verts[grid->width * j + offset].xyz[0]) > .1) continue;
            if ( fabs(grid->verts[grid->width * i + offset].xyz[1] - grid->verts[grid->width * j + offset].xyz[1]) > .1) continue;
            if ( fabs(grid->verts[grid->width * i + offset].xyz[2] - grid->verts[grid->width * j + offset].xyz[2]) > .1) continue;
            return qtrue;
        }
    }
    return qfalse;
}

void R_FixSharedVertexLodError_r( int start, srfGridMesh_t *grid1 ) {
    int j, k, l, m, n, offset1, offset2, touch;
    srfGridMesh_t *grid2;

    for ( j = start; j < s_worldData.numsurfaces; j++ ) {
        grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
        if ( grid2->surfaceType != SF_GRID ) continue;
        if ( grid2->lodFixed == 2 ) continue;
        if ( grid1->lodRadius != grid2->lodRadius ) continue;
        if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
        if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
        if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
        touch = qfalse;
        for (n = 0; n < 2; n++) {
            if (n) offset1 = (grid1->height-1) * grid1->width;
            else offset1 = 0;
            if (R_MergedWidthPoints(grid1, offset1)) continue;
            for (k = 1; k < grid1->width-1; k++) {
                for (m = 0; m < 2; m++) {

                    if (m) offset2 = (grid2->height-1) * grid2->width;
                    else offset2 = 0;
                    if (R_MergedWidthPoints(grid2, offset2)) continue;
                    for ( l = 1; l < grid2->width-1; l++) {
                        if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
                        if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
                        if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
                        grid2->widthLodError[l] = grid1->widthLodError[k];
                        touch = qtrue;
                    }
                }
                for (m = 0; m < 2; m++) {

                    if (m) offset2 = grid2->width-1;
                    else offset2 = 0;
                    if (R_MergedHeightPoints(grid2, offset2)) continue;
                    for ( l = 1; l < grid2->height-1; l++) {
                        if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
                        if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
                        if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
                        grid2->heightLodError[l] = grid1->widthLodError[k];
                        touch = qtrue;
                    }
                }
            }
        }
        for (n = 0; n < 2; n++) {
            if (n) offset1 = grid1->width-1;
            else offset1 = 0;
            if (R_MergedHeightPoints(grid1, offset1)) continue;
            for (k = 1; k < grid1->height-1; k++) {
                for (m = 0; m < 2; m++) {

                    if (m) offset2 = (grid2->height-1) * grid2->width;
                    else offset2 = 0;
                    if (R_MergedWidthPoints(grid2, offset2)) continue;
                    for ( l = 1; l < grid2->width-1; l++) {
                        if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
                        if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
                        if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
                        grid2->widthLodError[l] = grid1->heightLodError[k];
                        touch = qtrue;
                    }
                }
                for (m = 0; m < 2; m++) {

                    if (m) offset2 = grid2->width-1;
                    else offset2 = 0;
                    if (R_MergedHeightPoints(grid2, offset2)) continue;
                    for ( l = 1; l < grid2->height-1; l++) {
                        if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
                        if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
                        if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
                        grid2->heightLodError[l] = grid1->heightLodError[k];
                        touch = qtrue;
                    }
                }
            }
        }
        if (touch) {
            grid2->lodFixed = 2;
            R_FixSharedVertexLodError_r ( start, grid2 );
        }
    }
}

void R_FixSharedVertexLodError( void ) {
    int i;
    srfGridMesh_t *grid1;

    for ( i = 0; i < s_worldData.numsurfaces; i++ ) {
        grid1 = (srfGridMesh_t *) s_worldData.surfaces[i].data;
        if ( grid1->surfaceType != SF_GRID )
            continue;
        if ( grid1->lodFixed )
            continue;
        grid1->lodFixed = 2;
        R_FixSharedVertexLodError_r( i + 1, grid1);
    }
}

static	void R_LoadSurfaces(gamelump_t* surfs, gamelump_t* verts, gamelump_t* indexLump) {
    dsurface_t	*in;
    msurface_t	*out;
    drawVert_t	*dv;
    int			*indexes;
    int			count;
    int			numFaces, numMeshes, numTriSurfs, numFlares;
    int			i;

    numFaces = 0;
    numMeshes = 0;
    numTriSurfs = 0;
    numFlares = 0;

    if (surfs->length % sizeof(*in))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
    count = surfs->length / sizeof(*in);

    dv = (void*)verts->buffer;
    if (verts->length % sizeof(*dv))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);

    indexes = (void*)indexLump->buffer;
    if (indexLump->length % sizeof(*indexes))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);

    in = (void*)surfs->buffer;
    out = ri.Hunk_Alloc ( count * sizeof(*out), h_dontcare );	

    s_worldData.surfaces = out;
    s_worldData.numsurfaces = count;

    for ( i = 0 ; i < count ; i++, in++, out++ ) {
        switch ( LittleLong( in->surfaceType ) ) {
        case MST_PATCH:
            ParseMesh ( in, dv, out );
            numMeshes++;
            break;
        case MST_TRIANGLE_SOUP:
            ParseTriSurf( in, dv, out, indexes );
            numTriSurfs++;
            break;
        case MST_PLANAR:
            ParseFace( in, dv, out, indexes );
            numFaces++;
            break;
        case MST_FLARE:
            ParseFlare( in, dv, out, indexes );
            numFlares++;
            break;
        case MST_TERRAIN:
            // Note retail does NOT count it: @0x5F08 branches straight past the counter
            ParseTerrainSurf( in, dv, out, indexes );
            break;
        default:
            ri.Error( ERR_DROP, "Bad surfaceType %i", LittleLong( in->surfaceType ) );
        }
    }

    R_FixSharedVertexLodError();

    ri.Printf( PRINT_ALL, "...loaded %d faces, %i meshes, %i trisurfs, %i flares\n",
        numFaces, numMeshes, numTriSurfs, numFlares );
}



static	void R_LoadSubmodels(gamelump_t* l) {
    dmodel_t	*in;
    bmodel_t	*out;
    int			i, j, count;

    in = (void*)l->buffer;
    if (l->length % sizeof(*in))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
    count = l->length / sizeof(*in);

    s_worldData.bmodels = out = ri.Hunk_Alloc( count * sizeof(*out), h_dontcare );
    s_worldData.numBmodels = count;

    for ( i=0 ; i<count ; i++, in++, out++ ) {
        model_t *model;

        model = R_AllocModel();

        assert( model != NULL );			

        model->type = MOD_BRUSH;
        model->d.bmodel = out;
        Com_sprintf( model->name, sizeof( model->name ), "*%d", i );

        for (j=0 ; j<3 ; j++) {
            out->bounds[0][j] = LittleFloat (in->mins[j]);
            out->bounds[1][j] = LittleFloat (in->maxs[j]);
        }

        out->firstSurface = s_worldData.surfaces + LittleLong( in->firstSurface );
        out->numSurfaces = LittleLong( in->numSurfaces );
    }
}




static	void R_SetParent (mnode_t *node, mnode_t *parent)
{
    node->parent = parent;
    if (node->contents != -1)
        return;
    R_SetParent (node->children[0], node);
    R_SetParent (node->children[1], node);
}

static	void R_LoadNodesAndLeafs(gamelump_t* nodeLump, gamelump_t* leafLump) {
    int			i, j, p;
    dnode_t* in;
    dleaf_t* inLeaf;
    mnode_t* out;
    int			numNodes, numLeafs;

    in = (void*)nodeLump->buffer;
    if (nodeLump->length % sizeof(dnode_t) ||
        leafLump->length % sizeof(dleaf_t)) {
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
    }
    numNodes = nodeLump->length / sizeof(dnode_t);
    numLeafs = leafLump->length / sizeof(dleaf_t);

    out = ri.Hunk_Alloc((numNodes + numLeafs) * sizeof(*out), h_dontcare);

    s_worldData.nodes = out;
    s_worldData.numnodes = numNodes + numLeafs;
    s_worldData.numDecisionNodes = numNodes;

    for (i = 0; i < numNodes; i++, in++, out++)
    {
        for (j = 0; j < 3; j++)
        {
            out->mins[j] = LittleLong(in->mins[j]);
            out->maxs[j] = LittleLong(in->maxs[j]);
        }

        p = LittleLong(in->planeNum);
        out->plane = s_worldData.planes + p;

        out->contents = CONTENTS_NODE;	

        for (j = 0; j < 2; j++)
        {
            p = LittleLong(in->children[j]);
            if (p >= 0)
                out->children[j] = s_worldData.nodes + p;
            else
                out->children[j] = s_worldData.nodes + numNodes + (-1 - p);
        }
    }

    inLeaf = (void*)leafLump->buffer;
    for (i = 0; i < numLeafs; i++, inLeaf++, out++)
    {
        for (j = 0; j < 3; j++)
        {
            out->mins[j] = LittleLong(inLeaf->mins[j]);
            out->maxs[j] = LittleLong(inLeaf->maxs[j]);
        }

        out->cluster = LittleLong(inLeaf->cluster);
        out->area = LittleLong(inLeaf->area);

        if (out->cluster >= s_worldData.numClusters) {
            s_worldData.numClusters = out->cluster + 1;
        }

        out->firstmarksurface = s_worldData.marksurfaces +
            LittleLong(inLeaf->firstLeafSurface);
        out->nummarksurfaces = LittleLong(inLeaf->numLeafSurfaces);

    }

    R_SetParent(s_worldData.nodes, NULL);
}



static	void R_LoadShaders(gamelump_t* l) {
    int		i, count;
    dshader_t* in, * out;

    in = l->buffer;
    if (l->length % sizeof(*in))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
    count = l->length / sizeof(*in);
    out = ri.Hunk_Alloc(count * sizeof(*out), h_dontcare);

    s_worldData.shaders = out;
    s_worldData.numShaders = count;

    Com_Memcpy(out, in, count * sizeof(*out));

    for (i = 0; i < count; i++) {
        out[i].surfaceFlags = LittleLong(out[i].surfaceFlags);
        out[i].contentFlags = LittleLong(out[i].contentFlags);
    }
}


static	void R_LoadMarksurfaces(gamelump_t* l)
{
    int		i, j, count;
    int* in;
    msurface_t** out;

    in = l->buffer;
    if (l->length % sizeof(*in))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
    count = l->length / sizeof(*in);
    out = (msurface_t**)ri.Hunk_Alloc(count * sizeof(*out), h_dontcare);

    s_worldData.marksurfaces = out;
    s_worldData.nummarksurfaces = count;

    for (i = 0; i < count; i++)
    {
        j = LittleLong(in[i]);
        out[i] = &s_worldData.surfaces[j];
    }
}


static	void R_LoadPlanes(gamelump_t* l) {
    int			i, j;
    cplane_t* out;
    dplane_t* in;
    int			count;
    int			bits;

    in = l->buffer;
    if (l->length % sizeof(*in))
        ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
    count = l->length / sizeof(*in);
    out = ri.Hunk_Alloc(count * 2 * sizeof(*out), h_dontcare);

    s_worldData.planes = out;
    s_worldData.numplanes = count;

    for (i = 0; i < count; i++, in++, out++) {
        bits = 0;
        for (j = 0; j < 3; j++) {
            out->normal[j] = LittleFloat(in->normal[j]);
            if (out->normal[j] < 0) {
                bits |= 1 << j;
            }
        }

        out->dist = LittleFloat(in->dist);
        out->type = PlaneTypeForNormal(out->normal);
        out->signbits = bits;
    }
}

static void R_LoadFogs(gamelump_t *fogs, gamelump_t *brushes, gamelump_t *brushsides) {
	dfog_t       *in;
	dbrush_t     *inBrush;
	dbrushside_t *inSide;
	fog_t        *out;
	int           i, j;
	int           count, numBrushes, numBrushSides;

	// `count >= MAX_MAP_FOGS` ERR_DROP was a port-only guard and is gone; retail simply

	if (fogs->length % sizeof(dfog_t)) {
		ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
	}
	count = fogs->length / sizeof(dfog_t);

	numFogs = count;

	s_worldData.numfogs = count + 1;
	s_worldData.fogs = ri.Hunk_Alloc(sizeof(fog_t) * s_worldData.numfogs, h_dontcare);

	if (!count) {
		return;
	}

	in = (dfog_t *)fogs->buffer;
	inBrush = (dbrush_t *)brushes->buffer;
	inSide = (dbrushside_t *)brushsides->buffer;

	if (brushes->length % sizeof(dbrush_t)) {
		ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
	}
	numBrushes = brushes->length / sizeof(dbrush_t);
	if (brushsides->length & 7) {
		ri.Error(ERR_DROP, "LoadMap: funny lump size in %s", s_worldData.name);
	}
	numBrushSides = brushsides->length / sizeof(dbrushside_t);

	out = s_worldData.fogs + 1;

	for (i = 0; i < count; i++, in++, out++) {
		shader_t *shader;
		int       brushNum    = LittleLong(in->brushNum);
		int       visibleSide = LittleLong(in->visibleSide);
		int       firstSide;
		float     depth;

		out->originalBrushNumber = brushNum;

		if ((unsigned)brushNum >= (unsigned)numBrushes) {
			ri.Error(ERR_DROP, "fog brushNumber out of range");
		}
		firstSide = LittleLong(inBrush[brushNum].firstSide);
		if ((unsigned)firstSide > (unsigned)(numBrushSides - 6)) {
			ri.Error(ERR_DROP, "fog brush sideNumber out of range");
		}

		for (j = 0; j < 6; j++) {
			int pn = LittleLong(inSide[firstSide + j].planeNum);

			if (j & 1) {
				out->bounds[1][j >> 1] = s_worldData.planes[pn].dist;
			} else {
				out->bounds[0][j >> 1] = -s_worldData.planes[pn].dist;
			}
		}

		shader = R_FindShader(in->shader, LIGHTMAP_NONE, qtrue, qtrue, qtrue, qfalse);

		out->parms.color[0] = shader->fogParms.color[0];
		out->parms.color[1] = shader->fogParms.color[1];
		out->parms.color[2] = shader->fogParms.color[2];
		out->parms.depthForOpaque = shader->fogParms.depthForOpaque;
		out->parms.wave = shader->fogParms.wave;

		out->colorInt = ColorBytes4(
			shader->fogParms.color[0] * tr.identityLight,
			shader->fogParms.color[1] * tr.identityLight,
			shader->fogParms.color[2] * tr.identityLight,
			1.0f);

		depth = shader->fogParms.depthForOpaque;
		if (depth < 1.0f) {
			depth = 1.0f;
		}
		out->tcScale = 1.0f / (8.0f * depth);

		out->hasSurface = qfalse;
		if (visibleSide != -1) {
			int pn = LittleLong(inSide[firstSide + visibleSide].planeNum);

			out->hasSurface  = qtrue;
			out->surface[0] = -s_worldData.planes[pn].normal[0];
			out->surface[1] = -s_worldData.planes[pn].normal[1];
			out->surface[2] = -s_worldData.planes[pn].normal[2];
			out->surface[3] = -s_worldData.planes[pn].dist;
		}
	}
}






qboolean R_GetEntityToken( char *buffer, int size ) {
    return qfalse;
}

static byte *r_bspFileBuffer;
static int   r_bspFileLength;

int R_LoadLump(fileHandle_t handle, lump_t* lump, gamelump_t* glump, int size)
{
    glump->buffer = NULL;
    glump->length = lump->filelen;

    if (lump->filelen) {
        if (!r_bspFileBuffer) {
            glump->buffer = ri.Hunk_AllocateTempMemory(lump->filelen);

            if (ri.FS_Seek(handle, lump->fileofs, FS_SEEK_SET) < 0) {
                Com_Error(ERR_DROP, "R_LoadLump: Error seeking to lump.");
            }

            ri.FS_Read(glump->buffer, lump->filelen, handle);
        } else {
            if (lump->fileofs < 0 || lump->fileofs + lump->filelen > r_bspFileLength) {
                Com_Error(
                    ERR_DROP,
                    "R_LoadLump: lump offset %i length %i outside %s (%i bytes)",
                    lump->fileofs,
                    lump->filelen,
                    s_worldData.name,
                    r_bspFileLength
                );
            }

            glump->buffer = ri.Hunk_AllocateTempMemory(lump->filelen);
            Com_Memcpy(glump->buffer, r_bspFileBuffer + lump->fileofs, lump->filelen);
        }

        if (size) {
            return lump->filelen / size;
        }
    }

    return 0;
}

void R_FreeLump(gamelump_t* lump)
{
    if (lump->buffer)
    {
        ri.Hunk_FreeTempMemory(lump->buffer);
        lump->buffer = NULL;
        lump->length = 0;
    }
}


#define _R( id ) ri.UI_LoadResource( "*" #id )

void RE_LoadWorldMap( const char *name ) {
    int			i;
    dheader_t	header;
    fileHandle_t	h;
    int				length;
    vec3_t			vDefSundir;
    gamelump_t		lump, lump2, lump3;

    vDefSundir[0] = 0.45f;
    vDefSundir[1] = 0.3f;
    vDefSundir[2] = 0.9f;

    SMP_RLOAD_BEGIN( world );

    _R(52);

    VectorCopy(vDefSundir, tr.sunDirection);
    VectorNormalize( tr.sunDirection );

    tr.worldMapLoaded = qtrue;

    {
        SMP_RLOAD_BEGIN( bspread );
        length = ri.FS_ReadFile(name, (void **)&r_bspFileBuffer);
        SMP_RLOAD_END( bspread );
    }
    if (length <= 0 || !r_bspFileBuffer) {
        r_bspFileBuffer = NULL;
        ri.Error(ERR_DROP, "RE_LoadWorldMap: %s not found", name);
    }
    r_bspFileLength = (int)length;

    if (r_bspFileLength < (int)sizeof(dheader_t)) {
        ri.FS_FreeFile(r_bspFileBuffer);
        r_bspFileBuffer = NULL;
        ri.Error(ERR_DROP, "RE_LoadWorldMap: %s is too small to hold a BSP header", name);
    }

    Com_Memcpy(&header, r_bspFileBuffer, sizeof(dheader_t));
    h = 0;    

    for (i=0 ; i<sizeof(dheader_t)/4 ; i++) {
        ((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);
    }

    map_length = length;
    map_version = header.version;

    tr.world = NULL;

    Com_Memset( &s_worldData, 0, sizeof( s_worldData ) );
    Q_strncpyz( s_worldData.name, name, sizeof( s_worldData.name ) );

    Q_strncpyz( s_worldData.baseName, COM_SkipPath( s_worldData.name ), sizeof( s_worldData.name ) );
    COM_StripExtension( s_worldData.baseName, s_worldData.baseName, sizeof(s_worldData.baseName));

    c_gridVerts = 0;

    i = header.version;
    if (i != BSP_VERSION) {
        ri.Error(ERR_DROP, "RE_LoadWorldMap: %s has wrong version number (%i should be %i)",
            name, i, BSP_VERSION);
    }

    _R(53);
        numShaders = R_LoadLump(h, &header.lumps[LUMP_SHADERS], &lump, sizeof(dshader_t));
    _R(54);
    SMP_RLOAD_BEGIN( shaders );
    R_LoadShaders(&lump);
    SMP_RLOAD_END( shaders );
    _R(55);
    R_FreeLump(&lump);
    _R(56);
    numplanes = R_LoadLump(h, &header.lumps[LUMP_PLANES], &lump, sizeof(dplane_t));
    R_LoadPlanes(&lump);
    _R(57);
    R_FreeLump(&lump);
    _R(58);
    numLightBytes = R_LoadLump(h, &header.lumps[LUMP_LIGHTMAPS], &lump, sizeof(byte));
    _R(59);
    SMP_RLOAD_BEGIN( lightmap );
    R_LoadLightmaps(&lump);
    _R(60);
    R_FreeLump(&lump);
    R_LoadLump(h, &header.lumps[LUMP_BASELIGHTMAPS], &lump, sizeof(byte));
    R_LoadBaseLightmaps(&lump);
    R_FreeLump(&lump);
    R_LoadLump(h, &header.lumps[LUMP_CONTLIGHTMAPS], &lump, sizeof(byte));
    R_LoadContLightmaps(&lump);
    R_FreeLump(&lump);
    SMP_RLOAD_END( lightmap );
    _R(61);
        numDrawSurfaces = R_LoadLump(h, &header.lumps[LUMP_SURFACES], &lump, sizeof(dsurface_t));
    _R(62);
        numDrawVerts = R_LoadLump(h, &header.lumps[LUMP_DRAWVERTS], &lump2, sizeof(drawVert_t));
    _R(63);
    numDrawIndexes = R_LoadLump(h, &header.lumps[LUMP_DRAWINDEXES], &lump3, sizeof(int));
    _R(64);
    R_LoadSurfaces(&lump, &lump2, &lump3);
    _R(65);
    R_FreeLump(&lump3);
    _R(66);
    R_FreeLump(&lump2);
    _R(67);
    R_FreeLump(&lump);
    _R(68);
    numleafsurfaces = R_LoadLump(h, &header.lumps[LUMP_LEAFSURFACES], &lump, sizeof(int));
    _R(69);
    R_LoadMarksurfaces(&lump);
    _R(70);
    R_FreeLump(&lump);
    _R(71);
        numleafs = R_LoadLump(h, &header.lumps[LUMP_LEAFS], &lump2, sizeof(dleaf_t));
    _R(72);
    numnodes = R_LoadLump(h, &header.lumps[LUMP_NODES], &lump, sizeof(mnode_t));
    _R(73);
    R_LoadNodesAndLeafs(&lump, &lump2);
    _R(74);
    R_FreeLump(&lump);
    _R(75);
    R_FreeLump(&lump2);
    _R(76);
    {
        gamelump_t fogLump, brushLump, sideLump;

        R_LoadLump(h, &header.lumps[LUMP_FOGS], &fogLump, sizeof(dfog_t));
        R_LoadLump(h, &header.lumps[LUMP_BRUSHES], &brushLump, sizeof(dbrush_t));
        R_LoadLump(h, &header.lumps[LUMP_BRUSHSIDES], &sideLump, sizeof(dbrushside_t));
        R_LoadFogs(&fogLump, &brushLump, &sideLump);
        R_FreeLump(&fogLump);
        R_FreeLump(&brushLump);
        R_FreeLump(&sideLump);
    }
    nummodels = R_LoadLump(h, &header.lumps[LUMP_MODELS], &lump, sizeof(bmodel_t));
    _R(77);
    R_LoadSubmodels(&lump);
    _R(78);
    R_FreeLump(&lump);
    _R(79);
    numVisBytes = R_LoadLump(h, &header.lumps[LUMP_VISIBILITY], &lump, sizeof(byte));
    _R(80);
    R_LoadVisibility(&lump);
    _R(81);
    R_FreeLump(&lump);
    _R(82);
    // more; EF2 lump 18 is empty by design and 16/17 do not exist in an EF2 BSP.)
    _R(89);
    numSLights = R_LoadLump(h, &header.lumps[LUMP_ENTLIGHTS], &lump, sizeof(mapspherel_t));
    _R(90);
    R_LoadSphereLights(&lump);
    _R(91);
    R_FreeLump(&lump);
    _R(92);
    entLightVisSize = R_LoadLump(h, &header.lumps[LUMP_ENTLIGHTSVIS], &lump, sizeof(byte));
    _R(93);
    R_LoadSphereLightVis(&lump);
    _R(94);
    R_FreeLump(&lump);
    _R(95);
    R_LoadLump(h, &header.lumps[LUMP_BASELIGHTINGVERTS], &lump, sizeof(byte));
    R_LoadBaseLightingVerts(&lump);
    R_FreeLump(&lump);
    R_LoadLump(h, &header.lumps[LUMP_CONTLIGHTINGVERTS], &lump, sizeof(byte));
    R_LoadContLightingVerts(&lump);
    R_FreeLump(&lump);
    R_LoadLump(h, &header.lumps[LUMP_BASELIGHTINGSURFS], &lump, sizeof(baseLightingSurf_t));
    R_LoadBaseLightingSurfs(&lump);
    R_FreeLump(&lump);
    R_LoadLump(h, &header.lumps[LUMP_LIGHTINGSURFS], &lump, sizeof(lightingSurf_t));
    R_LoadLightingSurfs(&lump);
    R_FreeLump(&lump);
    R_LoadLump(h, &header.lumps[LUMP_LIGHTINGVERTSURFS], &lump, sizeof(lightingVertSurf_t));
    R_LoadLightingVertSurfs(&lump);
    R_FreeLump(&lump);


    _R(96);
    R_LoadLump(h, &header.lumps[LUMP_STATIC_LOD_MODELS], &lump, sizeof(staticLodModel_t));
    _R(97);
    SMP_RLOAD_BEGIN( lods );
    R_LoadStaticLodModels(&lump);
    SMP_RLOAD_END( lods );
    _R(98);
    R_FreeLump(&lump);
    _R(99);

    tr.world = &s_worldData;

    ri.FS_FreeFile(r_bspFileBuffer);
    r_bspFileBuffer = NULL;
    r_bspFileLength = 0;

    ri.UI_LoadResource("*111");
    R_Sphere_InitLights();
    ri.UI_LoadResource("*112");
    ri.UI_LoadResource("*113");
    ri.UI_LoadResource("*115");
    ri.UI_LoadResource("*116");

    SMP_RLOAD_END( world );
}

int RE_MapVersion(void)
{
	return map_version;
}

void RE_PrintBSPFileSizes(void)
{
  ri.Printf(PRINT_ALL, "%s: %i\n", s_worldData.name, map_length);
  ri.Printf(PRINT_ALL, "%6i   models                %7i\n", nummodels, 40 * nummodels);
  ri.Printf(PRINT_ALL, "%6i   shaders               %7i\n", numShaders, 140 * numShaders);
  ri.Printf(PRINT_ALL, "%6i   brushes               %7i\n", numbrushes, 12 * numbrushes);
  ri.Printf(PRINT_ALL, "%6i   brushsides            %7i\n", numbrushsides, 12 * numbrushsides);
  ri.Printf(PRINT_ALL, "%6i   fogs                  %7i\n", numFogs, 72 * numFogs);
  ri.Printf(PRINT_ALL, "%6i   planes                %7i\n", numplanes, 16 * numplanes);
  ri.Printf(PRINT_ALL, "%6i   nodes                 %7i\n", numnodes, 36 * numnodes);
  ri.Printf(PRINT_ALL, "%6i   leafs                 %7i\n", numleafs, numleafs << 6);
  ri.Printf(PRINT_ALL, "%6i   leafsurfaces          %7i\n", numleafsurfaces, 4 * numleafsurfaces);
  ri.Printf(PRINT_ALL, "%6i   drawverts             %7i\n", numDrawVerts, 44 * numDrawVerts);
  ri.Printf(PRINT_ALL, "%6i   drawindexes           %7i\n", numDrawIndexes, 4 * numDrawIndexes);
  ri.Printf(PRINT_ALL, "%6i   drawsurfaces          %7i\n", numDrawSurfaces, 108 * numDrawSurfaces);
  ri.Printf(PRINT_ALL, "%6i   lightdefs             %7i\n", numLightDefs, 52 * numLightDefs);
  ri.Printf(PRINT_ALL, "%6i   lightmaps             %7i\n", numLightBytes / 49152, numLightBytes);
  ri.Printf(PRINT_ALL, "         visibility            %7i\n", numVisBytes);
  ri.Printf(PRINT_ALL, "%6i   entitylights          %7i\n", numSLights, (int)sizeof(mapspherel_t) * numSLights);
  ri.Printf(PRINT_ALL, "         entitylightvis        %7i\n", entLightVisSize);
  ri.Printf(PRINT_ALL, "         static model data     %7i\n", g_nStaticModelData);
  ri.Printf(PRINT_ALL, "%6i   static models defs    %7i\n", g_nStaticModels, 164 * g_nStaticModels);
  ri.Printf(PRINT_ALL, "         static model indexes  %7i\n", g_nStaticModelIndices);
}

void R_ClearWorld(void) {
    tr.world = NULL;
    tr.worldMapLoaded = qfalse;

    /* Every pointer here was in the arena just freed; RE_LoadWorldMap memsets
       it again, this closes the window between. */
    Com_Memset( &s_worldData, 0, sizeof( s_worldData ) );
}
