/*
cmtrace_probe -- offline differential rig for qcommon/cm_trace.c.

Loads a real EF2 BSP's collision lumps straight into the engine's own
clipMap_t (the same field-for-field mapping CMod_Load* does), then runs a
deterministic sweep of box and point traces two ways:

  A. the shipped traversal   -- CM_TraceThroughTree from node 0 (pruned)
  B. the brute-force ground truth -- CM_TraceToLeaf over EVERY leaf

and compares trace_t bit for bit.  B is a superset of A's brush set by
construction, so A == B proves the prune never discards a brush that the
trace result depends on.  Both paths call the unmodified
CM_TraceThroughBrush / CM_TraceToLeaf / CM_TraceThroughTree, so the only
thing under test is the set of leafs each one reaches.

Not part of the shipped tree; evidence for the r40 collision prune work.
SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cm_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ---- engine globals cm_trace.c / cm_test.c expect ---- */
#define BOX_PLANES   12
#define BOX_BRUSHES  1
#define BOX_SIDES    6
#define BOX_LEAFS    1

void CM_TraceToLeaf( traceWork_t *tw, cLeaf_t *leaf, int leafFlags );
void CM_TraceThroughTree( traceWork_t *tw, int num, float p1f, float p2f,
                          vec3_t p1, vec3_t p2, int leafFlags );
void CM_PositionTest( traceWork_t *tw );
qboolean CM_SightTraceThroughTree( traceWork_t *tw, int num, float p1f, float p2f,
                                     vec3_t p1, vec3_t p2 );
qboolean CM_SightTraceToLeaf( traceWork_t *tw, cLeaf_t *leaf );
void CM_BoundBrush( cbrush_t *b );

/* the masks the engine traces with (fgame/bg_public.h) */
#define M_SOLID        (CONTENTS_SOLID)
#define M_PLAYERSOLID  (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY|CONTENTS_SETCLIP)
#define M_SHOT         (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE|CONTENTS_WEAPONCLIP|CONTENTS_SHOOTABLE_ONLY|CONTENTS_SETCLIP)
#define M_CAMERASOLID  (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_SETCLIP)


/* ---- tiny hunk/alloc stand-ins (probe lifetime = process lifetime) ---- */
static void *s_hunk = NULL;
static size_t s_hunkUsed = 0;
#define HUNK_CAP (512u * 1024u * 1024u)

void *Hunk_Alloc( int size, ha_pref preference ) {
    (void)preference;
    size = ( size + 63 ) & ~63;
    if ( !s_hunk ) s_hunk = malloc( HUNK_CAP );
    if ( s_hunkUsed + (size_t)size > HUNK_CAP ) {
        fprintf( stderr, "hunk exhausted\n" );
        exit( 3 );
    }
    {
        char *p = (char *)s_hunk + s_hunkUsed;
        s_hunkUsed += size;
        memset( p, 0, size );
        return p;
    }
}

void Com_Error( int level, const char *fmt, ... ) {
    va_list ap;
    fprintf( stderr, "Com_Error(%d): ", level );
    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
    exit( 4 );
}

void Com_DPrintf( const char *fmt, ... ) { (void)fmt; }

/* ---- BSP lump reading ---- */
typedef struct { int fileofs, filelen; } plump_t;
typedef struct { int ident, version, checksum; plump_t lumps[30]; } pdheader_t;

enum {
    PL_SHADERS = 0, PL_PLANES = 1, PL_LEAFBRUSHES = 8, PL_LEAFSURFACES = 9,
    PL_LEAFS = 10, PL_NODES = 11, PL_BRUSHSIDES = 12, PL_BRUSHES = 13,
    PL_MODELS = 15
};

#define P_BSP_MAX_QPATH 64
typedef struct { char shader[P_BSP_MAX_QPATH]; int surfaceFlags, contentFlags, subdivisions; } pdshader_t;
typedef struct { float normal[3], dist; } pdplane_t;
typedef struct { int planeNum, children[2], mins[3], maxs[3]; } pdnode_t;
typedef struct { int cluster, area, mins[3], maxs[3], numLeafSurfaces, firstLeafSurface,
                     firstLeafBrush, numLeafBrushes; } pdleaf_t;
typedef struct { int shaderNum, planeNum; } pdbrushside_t;
typedef struct { int numSides, firstSide, shaderNum; } pdbrush_t;
typedef struct { float mins[3], maxs[3]; int firstSurface, numSurfaces, firstBrush, numBrushes; } pdmodel_t;

#define PLANE_TYPE_FOR_NORMAL( n ) \
    ( (n)[0] == 1.0f ? 0 : (n)[1] == 1.0f ? 1 : (n)[2] == 1.0f ? 2 : 3 )

static byte *readFile( const char *path, int *len ) {
    FILE *f = fopen( path, "rb" );
    byte *buf;
    long n;
    if ( !f ) return NULL;
    fseek( f, 0, SEEK_END );
    n = ftell( f );
    fseek( f, 0, SEEK_SET );
    buf = malloc( n );
    if ( fread( buf, 1, n, f ) != (size_t)n ) { fclose( f ); free( buf ); return NULL; }
    fclose( f );
    *len = (int)n;
    return buf;
}

static void SetSignbits( cplane_t *out ) {
    int bits = 0, j;
    for ( j = 0; j < 3; j++ ) if ( out->normal[j] < 0 ) bits |= 1 << j;
    out->signbits = bits;
}


static void loadBsp( const char *path ) {
    int len;
    byte *buf = readFile( path, &len );
    pdheader_t *h;
    byte *base;

    if ( !buf ) { fprintf( stderr, "cannot read %s\n", path ); exit( 2 ); }
    h = (pdheader_t *)buf;
    if ( h->version != 20 ) { fprintf( stderr, "bsp version %d != 20\n", h->version ); exit( 2 ); }
    base = buf;

    memset( &cm, 0, sizeof( cm ) );
    Q_strncpyz( cm.name, path, sizeof( cm.name ) );

    /* shaders */
    {
        plump_t *l = &h->lumps[PL_SHADERS];
        pdshader_t *in = (pdshader_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdshader_t );
        cm.shaders = Hunk_Alloc( count * sizeof( cshader_t ), h_dontcare );
        cm.numShaders = count;
        for ( int i = 0; i < count; i++ ) {
            memcpy( cm.shaders[i].shader, in[i].shader, P_BSP_MAX_QPATH );
            cm.shaders[i].surfaceFlags = in[i].surfaceFlags;
            cm.shaders[i].contentFlags = in[i].contentFlags;
            cm.shaders[i].subdivisions = in[i].subdivisions;
        }
    }
    /* planes: BOX_PLANES spare entries in front, exactly as CMod_LoadPlanes */
    {
        plump_t *l = &h->lumps[PL_PLANES];
        pdplane_t *in = (pdplane_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdplane_t );
        cm.planes = Hunk_Alloc( ( BOX_PLANES + count ) * sizeof( cplane_t ), h_dontcare );
        cm.numPlanes = count;
        for ( int i = 0; i < count; i++ ) {
            cplane_t *out = &cm.planes[i];
            for ( int j = 0; j < 3; j++ ) out->normal[j] = in[i].normal[j];
            out->dist = in[i].dist;
            out->type = PLANE_TYPE_FOR_NORMAL( out->normal );
            SetSignbits( out );
        }
    }
    /* nodes */
    {
        plump_t *l = &h->lumps[PL_NODES];
        pdnode_t *in = (pdnode_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdnode_t );
        cm.nodes = Hunk_Alloc( count * sizeof( cNode_t ), h_dontcare );
        cm.numNodes = count;
        for ( int i = 0; i < count; i++ ) {
            cm.nodes[i].plane = cm.planes + in[i].planeNum;
            cm.nodes[i].children[0] = in[i].children[0];
            cm.nodes[i].children[1] = in[i].children[1];
        }
    }
    /* leafs */
    {
        plump_t *l = &h->lumps[PL_LEAFS];
        pdleaf_t *in = (pdleaf_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdleaf_t );
        cm.leafs = Hunk_Alloc( ( BOX_LEAFS + count ) * sizeof( cLeaf_t ), h_dontcare );
        cm.numLeafs = count;
        for ( int i = 0; i < count; i++ ) {
            cLeaf_t *o = &cm.leafs[i];
            o->cluster = in[i].cluster;
            o->area = in[i].area;
            o->firstLeafBrush = in[i].firstLeafBrush;
            o->numLeafBrushes = in[i].numLeafBrushes;
            o->firstLeafSurface = in[i].firstLeafSurface;
            o->numLeafSurfaces = in[i].numLeafSurfaces;
            if ( o->cluster >= cm.numClusters ) cm.numClusters = o->cluster + 1;
            if ( o->area >= cm.numAreas ) cm.numAreas = o->area + 1;
        }
        cm.areas = Hunk_Alloc( cm.numAreas * sizeof( cArea_t ), h_dontcare );
        cm.areaPortals = Hunk_Alloc( (size_t)cm.numAreas * cm.numAreas * sizeof( int ), h_dontcare );
    }
    /* leafbrushes / leafsurfaces */
    {
        plump_t *l = &h->lumps[PL_LEAFBRUSHES];
        int count = l->filelen / 4;
        int *in = (int *)( base + l->fileofs );
        cm.leafbrushes = Hunk_Alloc( ( count + BOX_BRUSHES ) * sizeof( int ), h_dontcare );
        cm.numLeafBrushes = count;
        for ( int i = 0; i < count; i++ ) cm.leafbrushes[i] = in[i];

        l = &h->lumps[PL_LEAFSURFACES];
        count = l->filelen / 4;
        in = (int *)( base + l->fileofs );
        cm.leafsurfaces = Hunk_Alloc( count * sizeof( int ), h_dontcare );
        cm.numLeafSurfaces = count;
        int maxSurf = 0;
        for ( int i = 0; i < count; i++ ) {
            cm.leafsurfaces[i] = in[i];
            if ( in[i] > maxSurf ) maxSurf = in[i];
        }
        /* the probe loads no LUMP_SURFACES, so the patch table is all NULL and
           CM_TraceToLeaf's patch loop is a no-op in BOTH paths */
        cm.numSurfaces = maxSurf + 1;
        cm.surfaces = Hunk_Alloc( (size_t)cm.numSurfaces * sizeof( cPatch_t * ), h_dontcare );
        for ( int i = 0; i < cm.numSurfaces; i++ ) cm.surfaces[i] = NULL;
    }
    /* brushsides */
    {
        plump_t *l = &h->lumps[PL_BRUSHSIDES];
        pdbrushside_t *in = (pdbrushside_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdbrushside_t );
        cm.brushsides = Hunk_Alloc( ( BOX_SIDES + count ) * sizeof( cbrushside_t ), h_dontcare );
        cm.numBrushSides = count;
        for ( int i = 0; i < count; i++ ) {
            int num = in[i].planeNum;
            cm.brushsides[i].plane = &cm.planes[num];
            cm.brushsides[i].shaderNum = in[i].shaderNum;
            cm.brushsides[i].surfaceFlags = cm.shaders[in[i].shaderNum].surfaceFlags;
        }
    }
    /* brushes */
    {
        plump_t *l = &h->lumps[PL_BRUSHES];
        pdbrush_t *in = (pdbrush_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdbrush_t );
        cm.brushes = Hunk_Alloc( ( BOX_BRUSHES + count ) * sizeof( cbrush_t ), h_dontcare );
        cm.numBrushes = count;
        for ( int i = 0; i < count; i++ ) {
            cbrush_t *o = &cm.brushes[i];
            o->sides = cm.brushsides + in[i].firstSide;
            o->numsides = in[i].numSides;
            o->shaderNum = in[i].shaderNum;
            o->contents = cm.shaders[o->shaderNum].contentFlags;
            CM_BoundBrush( o );
        }
    }
    /* cmodels: world model only (the probe traces handle 0) */
    {
        plump_t *l = &h->lumps[PL_MODELS];
        pdmodel_t *in = (pdmodel_t *)( base + l->fileofs );
        int count = l->filelen / sizeof( pdmodel_t );
        cm.cmodels = Hunk_Alloc( count * sizeof( cmodel_t ), h_dontcare );
        cm.numSubModels = count;
        for ( int i = 0; i < count; i++ ) {
            for ( int j = 0; j < 3; j++ ) {
                cm.cmodels[i].mins[j] = in[i].mins[j];
                cm.cmodels[i].maxs[j] = in[i].maxs[j];
            }
            cm.cmodels[i].leaf.firstLeafBrush = in[i].firstBrush;
            cm.cmodels[i].leaf.numLeafBrushes = in[i].numBrushes;
            cm.cmodels[i].leaf.firstLeafSurface = in[i].firstSurface;
            cm.cmodels[i].leaf.numLeafSurfaces = in[i].numSurfaces;
        }
    }

    free( buf );
}

/* ---- traceWork_t setup, verbatim from CM_BoxTraceInternal ---- */
static void setupTw( traceWork_t *tw, const vec3_t start, const vec3_t end,
                     const vec3_t mins, const vec3_t maxs, int brushmask ) {
    vec3_t offset;
    int i;

    memset( tw, 0, sizeof( *tw ) );
    tw->trace.fraction = 1;
    tw->contents = brushmask;

    for ( i = 0; i < 3; i++ ) {
        offset[i]       = ( mins[i] + maxs[i] ) * 0.5;
        tw->size[0][i]  = mins[i] - offset[i];
        tw->size[1][i]  = maxs[i] - offset[i];
        tw->start[i]    = start[i] + offset[i];
        tw->end[i]      = end[i] + offset[i];
    }
    tw->height = tw->size[1][2];
    tw->radius = tw->size[1][0];
    tw->maxOffset = tw->size[1][0] + tw->size[1][1] + tw->size[1][2];

    for ( i = 0; i < 8; i++ ) {
        tw->offsets[i][0] = ( i & 1 ) ? tw->size[1][0] : tw->size[0][0];
        tw->offsets[i][1] = ( i & 2 ) ? tw->size[1][1] : tw->size[0][1];
        tw->offsets[i][2] = ( i & 4 ) ? tw->size[1][2] : tw->size[0][2];
    }

    for ( i = 0; i < 3; i++ ) {
        if ( tw->start[i] < tw->end[i] ) {
            tw->bounds[0][i] = tw->start[i] + tw->size[0][i];
            tw->bounds[1][i] = tw->end[i] + tw->size[1][i];
        } else {
            tw->bounds[0][i] = tw->end[i] + tw->size[0][i];
            tw->bounds[1][i] = tw->start[i] + tw->size[1][i];
        }
    }

    if ( tw->size[0][0] == 0 && tw->size[0][1] == 0 && tw->size[0][2] == 0 ) {
        tw->isPoint = qtrue;
        VectorClear( tw->extents );
    } else {
        tw->isPoint = qfalse;
        tw->extents[0] = tw->size[1][0];
        tw->extents[1] = tw->size[1][1];
        tw->extents[2] = tw->size[1][2];
    }
}

static void finishTrace( trace_t *out, const traceWork_t *tw,
                         const vec3_t start, const vec3_t end ) {
    *out = tw->trace;
    if ( out->fraction == 1 ) {
        VectorCopy( end, out->endpos );
    } else {
        for ( int i = 0; i < 3; i++ )
            out->endpos[i] = start[i] + out->fraction * ( end[i] - start[i] );
    }
}

/* path A: the shipped pruned traversal */
static void tracePruned( trace_t *res, const vec3_t start, const vec3_t end,
                         const vec3_t mins, const vec3_t maxs, int mask ) {
    traceWork_t tw;
    setupTw( &tw, start, end, mins, maxs, mask );
    cm.checkcount++;
    if ( tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0 &&
         start[0] == end[0] && start[1] == end[1] && start[2] == end[2] ) {
        CM_PositionTest( &tw );
        finishTrace( res, &tw, start, end );
        return;
    }
    CM_TraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end, 0 );
    finishTrace( res, &tw, start, end );
}

/* path B: brute force -- every leaf, no BSP pruning at all */
static void traceBrute( trace_t *res, const vec3_t start, const vec3_t end,
                        const vec3_t mins, const vec3_t maxs, int mask ) {
    traceWork_t tw;
    int i;
    setupTw( &tw, start, end, mins, maxs, mask );
    cm.checkcount++;
    if ( tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0 &&
         start[0] == end[0] && start[1] == end[1] && start[2] == end[2] ) {
        CM_PositionTest( &tw );
        finishTrace( res, &tw, start, end );
        return;
    }
    for ( i = 0; i < cm.numLeafs; i++ ) {
        CM_TraceToLeaf( &tw, &cm.leafs[i], 0 );
        if ( !tw.trace.fraction ) break;
    }
    finishTrace( res, &tw, start, end );
}

/* path A/B for the sight trace: same two traversals, boolean verdict */
static qboolean sightPruned( const vec3_t start, const vec3_t end,
                             const vec3_t mins, const vec3_t maxs, int mask ) {
    traceWork_t tw;
    setupTw( &tw, start, end, mins, maxs, mask );
    cm.checkcount++;
    return CM_SightTraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end );
}
static qboolean sightBrute( const vec3_t start, const vec3_t end,
                            const vec3_t mins, const vec3_t maxs, int mask ) {
    traceWork_t tw;
    int i;
    setupTw( &tw, start, end, mins, maxs, mask );
    cm.checkcount++;
    for ( i = 0; i < cm.numLeafs; i++ ) {
        if ( !CM_SightTraceToLeaf( &tw, &cm.leafs[i] ) ) return qfalse;
    }
    return qtrue;
}

/* pruned-path-only timing: the brute leg is unchanged by the fix, so the
   wall clock of a pruned+brute run measures nothing.  This times the
   traversal the engine actually pays for. */
static double benchPruned( const vec3_t start, const vec3_t end,
                           const vec3_t mins, const vec3_t maxs, int mask,
                           long reps, long *outBrushTraces ) {
    traceWork_t tw;
    struct timespec t0, t1;
    c_brush_traces = 0;
    clock_gettime( CLOCK_MONOTONIC, &t0 );
    for ( long i = 0; i < reps; i++ ) {
        setupTw( &tw, start, end, mins, maxs, mask );
        cm.checkcount++;
        CM_TraceThroughTree( &tw, 0, 0, 1, tw.start, tw.end, 0 );
    }
    clock_gettime( CLOCK_MONOTONIC, &t1 );
    *outBrushTraces = c_brush_traces;
    return (double)( t1.tv_sec - t0.tv_sec ) + 1e-9 * (double)( t1.tv_nsec - t0.tv_nsec );
}

/* ---- deterministic xorshift PRNG so both runs see identical inputs ---- */
static unsigned long long rngState = 0x9e3779b97f4a7c15ULL;
static unsigned long long rnd( void ) {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 7;
    rngState ^= rngState << 17;
    return rngState;
}
static float rndUnit( void ) {  /* [-1,1) */
    return (float)( (double)( (long long)rnd() % 2000001 - 1000000 ) / 1000000.0 );
}

/* the four cvars CM_LoadMap would have registered; the probe drives the
   traversal directly, so it owns them */
static cvar_t s_noCurves      = { 0, 0, "0" };
static cvar_t s_noAreas       = { 0, 0, "0" };
static cvar_t s_playerCurve   = { 0, 0, "1" };
static cvar_t s_terrain       = { 0, 0, "1" };

int main( int argc, char **argv ) {
    const char *path = argv[1];
    long iters = ( argc > 2 ) ? atol( argv[2] ) : 200000;
    int mismatches = 0;
    long checked = 0;
    long sightchecked = 0, sightmismatches = 0;
    int mi;
    vec3_t pmins, pmaxs;

    if ( argc < 2 ) { fprintf( stderr, "usage: %s <bsp> [iters]\n", argv[0] ); return 1; }

    cm_noCurves        = &s_noCurves;
    cm_noAreas         = &s_noAreas;
    cm_playerCurveClip = &s_playerCurve;
    cm_terrainCollision = &s_terrain;

    loadBsp( path );
    printf( "map %-46s planes=%d nodes=%d leafs=%d brushes=%d leafbrushes=%d\n",
            path, cm.numPlanes, cm.numNodes, cm.numLeafs, cm.numBrushes, cm.numLeafBrushes );

    /* the trace masks EF2 actually uses on these paths */
    {
        static const int masks[] = {
            M_SOLID, M_PLAYERSOLID, M_SHOT, M_CAMERASOLID,
            M_SOLID, M_PLAYERSOLID | CONTENTS_BODY, 0x7fffffff
        };
        const int nmasks = (int)( sizeof( masks ) / sizeof( masks[0] ) );

        /* a spread of real box sizes: point, player standing, player crouched,
           camera, a large vehicle-ish box */
        {
            float boxmins[6][3] = {
                { 0, 0, 0 },        { -15, -15, -24 }, { -15, -15, -18 },
                { -12, -12, -4 },   { -32, -32, -16 }, { -48, -48, -48 }
            };
            float boxmaxs[6][3] = {
                { 0, 0, 0 },        {  15,  15,  32 }, {  15,  15,  32 },
                {  12,  12,  4 },   {  32,  32,  16 }, {  48,  48,  48 }
            };
            const int nboxes = 6;

            for ( mi = 0; mi < nboxes; mi++ ) {
                for ( int k = 0; k < 3; k++ ) { pmins[k] = boxmins[mi][k]; pmaxs[k] = boxmaxs[mi][k]; }

                int boxBrushes = 0;

                for ( long it = 0; it < iters; it++ ) {
                    vec3_t start, end;
                    trace_t a, b;
                    int mask = masks[ it % nmasks ];

                    /* keep start inside the world and biased into the brushed
                       interior: the map's own model bounds walk inward 10% */
                    for ( int k = 0; k < 3; k++ ) {
                        float lo = cm.cmodels[0].mins[k] * 0.9f;
                        float hi = cm.cmodels[0].maxs[k] * 0.9f;
                        if ( lo > hi ) { float t = lo; lo = hi; hi = t; }
                        start[k] = lo + rndUnit() * 0.5f * ( hi - lo );
                        /* short-to-long segments: mixes the sweep and the
                           near-stationary cases the prune decides on */
                        end[k] = start[k] + rndUnit() * ( ( it % 4 == 0 ) ? 400.0f : 40.0f );
                    }

                    if ( sightPruned( start, end, pmins, pmaxs, mask )
                         != sightBrute( start, end, pmins, pmaxs, mask ) ) {
                        sightmismatches++;
                    }
                    sightchecked++;

                    memset( &a, 0xAA, sizeof( a ) );
                    memset( &b, 0xAA, sizeof( b ) );
                    { int t0 = c_brush_traces;
                      tracePruned( &a, start, end, pmins, pmaxs, mask );
                      boxBrushes += c_brush_traces - t0; }
                    traceBrute( &b, start, end, pmins, pmaxs, mask );
                    checked++;

                    if ( memcmp( &a, &b, sizeof( a ) ) != 0 ) {
                        if ( mismatches < 5 ) {
                            printf( "  MISMATCH box=%d mask=0x%x\n    start %.3f %.3f %.3f -> end %.3f %.3f %.3f\n",
                                    mi, mask, start[0], start[1], start[2], end[0], end[1], end[2] );
                            printf( "    pruned: frac=%.9g allsolid=%d startsolid=%d plane=(%.6f,%.6f,%.6f) c=%d sf=%d\n",
                                    a.fraction, a.allsolid, a.startsolid,
                                    a.plane.normal[0], a.plane.normal[1], a.plane.normal[2],
                                    a.contents, a.surfaceFlags );
                            printf( "    brute : frac=%.9g allsolid=%d startsolid=%d plane=(%.6f,%.6f,%.6f) c=%d sf=%d\n",
                                    b.fraction, b.allsolid, b.startsolid,
                                    b.plane.normal[0], b.plane.normal[1], b.plane.normal[2],
                                    b.contents, b.surfaceFlags );
                        }
                        mismatches++;
                    }
                }
                printf( "  box %d (%g,%g,%g)-(%g,%g,%g): %ld traces, %d mismatches, %d brush tests\n", mi,
                        pmins[0], pmins[1], pmins[2], pmaxs[0], pmaxs[1], pmaxs[2],
                        iters, mismatches, boxBrushes );
                fflush( stdout );
            }
        }
    }

    if ( argc > 3 && !strcmp( argv[3], "-bench" ) ) {
        /* one representative player-box trace, repeated: isolates the prune */
        vec3_t bs = { -200.0f, -200.0f, 40.0f };
        vec3_t be = {  640.0f,  512.0f, 40.0f };
        vec3_t bmins = { -15, -15, -24 }, bmaxs = { 15, 15, 32 };
        long bt;
        double secs = benchPruned( bs, be, bmins, bmaxs, M_PLAYERSOLID, 200000, &bt );
        printf( "BENCH pruned player-box x200000: %.4fs, %ld brush tests -> %.1f ns/trace, %.1f brushes/trace\n",
                secs, bt, secs * 1e9 / 200000.0, (double)bt / 200000.0 );
        return 0;
    }

    printf( "brush tests over the sweep: before-this-run total = %d\n", c_brush_traces );
    printf( "leaf visits are not counted (would need a shipped counter)\n" );
    printf( "sight: %ld compared, %ld mismatches\n", sightchecked, sightmismatches );
    printf( "RESULT %s: %ld traces compared, %d mismatches\n",
            ( mismatches || sightmismatches ) ? "FAIL" : "PASS", checked, mismatches );
    return ( mismatches || sightmismatches ) ? 1 : 0;
}
