

#include "cg_local.h"
#include "../corepp/vector.h"
#include "../corepp/container.h"
#include "cg_commands.h"

#define PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_probe.h"

#define WIDGET_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define WIDGET_PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_widget.h"

class beam_t : public Class
{
public:
    beam_t();

    int       entity;
    qhandle_t hModel;
    int       endtime;
    Vector    start, end;
    float     scale;
    float     alpha;
    int       flags;
    int       parent;
    float     max_offset;
    float     min_offset;
    int       numSubdivisions;
    int       overlap;
    int       beamshader;
    byte      shaderRGBA[4];
    int       update_time;
    int       delay;
    float     life;
    int       numspherebeams;
    float     sphereradius;
    int       toggletime;
    int       toggledelay;
    qboolean  active;
    float     alphastep;
    int       renderfx;
    str       name;
    float     wavespeed;
    Vector    amp;
};

beam_t::beam_t()
{
    entity          = 0;
    hModel          = 0;
    endtime         = 0;
    scale           = 0;
    alpha           = 0;
    flags           = 0;
    parent          = ENTITYNUM_NONE;
    max_offset      = 0;
    min_offset      = 0;
    numSubdivisions = 0;
    overlap         = 0;
    beamshader      = 0;
    update_time     = 0;
    delay           = 0;
    life            = 0;
    numspherebeams  = 0;
    sphereradius    = 0;
    toggletime      = 0;
    toggledelay     = 0;
    active          = 0;
    alphastep       = 0;
    renderfx        = 0;
    wavespeed       = 0;
    amp.x           = 0;
    amp.y           = 0;
    amp.z           = 0;
    memset(shaderRGBA, 0, 4);
}

beam_t cl_beams[MAX_BEAMS];

static int seed = 100;


#define MAX_BEAM_BACKUP   6
#define MAX_BEAM_SEGMENTS 32

typedef struct beamSegment_t {
    polyVert_t points[4];
} beamSegment_t;

typedef struct beamList_t {
    int           time;
    int           updatetime;
    int           numsegments;
    beamSegment_t segments[MAX_BEAM_SEGMENTS];
} beamList_t;

typedef struct beamEnt_t {
    int        owner;
    int        numbeams;
    int        life;
    int        renderfx;
    beamList_t beamlist[MAX_BEAM_BACKUP];
} beamEnt_t;

Container<beamEnt_t *> beamManager;


void RemoveBeamList(int owner)
{
    int i;

    for (i = beamManager.NumObjects(); i > 0; i--) {
        beamEnt_t *be = beamManager.ObjectAt(i);

        if (be->owner == owner) {
            beamManager.RemoveObjectAt(i);
            delete be;
            return;
        }
    }
}

beamEnt_t *FindBeamList(int owner)
{
    int i, num;

    num = beamManager.NumObjects();

    for (i = 1; i <= num; i++) {
        beamEnt_t *be = beamManager.ObjectAt(i);

        if (be->owner == owner) {
            return be;
        }
    }
    return NULL;
}

int CreateNewBeamEntity(int owner, float life)
{
    beamEnt_t *be;
    int        i, oldest, oldest_time;

    be = FindBeamList(owner);

    if (!be) {
        be = new beamEnt_t;

        if (!be) {
            cgi.Error(ERR_DROP, "Could not allocate memory for beamEnt.\n");
        }

        memset(be, 0, sizeof(beamEnt_t));
        memset(be->beamlist, 0, sizeof(beamList_t) * MAX_BEAM_BACKUP);
        be->owner = owner;
        be->life  = life;

        beamManager.AddObject(be);
    }

    oldest      = -1;
    oldest_time = 999999999;

    for (i = 0; i < MAX_BEAM_BACKUP; i++) {
        float t = be->beamlist[i].time;

        if (!t) {
            oldest = i;
            break;
        }

        if (t < oldest_time) {
            oldest      = i;
            oldest_time = t;
        }
    }

    be->beamlist[oldest].numsegments = 0;
    be->beamlist[oldest].time        = cg.time;
    be->beamlist[oldest].updatetime  = cg.time + be->life;
    return oldest;
}

void RemoveBeamEntity(int owner)
{
    RemoveBeamList(owner);
}

void AddBeamSegmentToList(int owner, polyVert_t points[4], int beamnum, int segnum, int renderfx)
{
    beamEnt_t *be;

    be = FindBeamList(owner);

    if (!be) {
        cgi.DPrintf("Could not find beam entity for owner:%d\n", owner);
        return;
    }

    if (segnum >= MAX_BEAM_SEGMENTS) {
        return;
    }

    be->renderfx = renderfx;

    memcpy(&be->beamlist[beamnum].segments[segnum].points, points, 4 * sizeof(polyVert_t));

    be->beamlist[beamnum].numsegments++;
}

void CG_AddBeamsFromList(int owner, int beamshader)
{
    int        i, j, k, l;
    float      frac, fade;
    beamEnt_t *be = FindBeamList(owner);
    polyVert_t newpoints[4];

    if (!be) {
        return;
    }

    for (i = 0; i < MAX_BEAM_BACKUP; i++) {
        beamList_t *bl = &be->beamlist[i];

        if (!bl->time) {
            continue;
        }

        frac = (float)(cg.time - bl->time) / (float)be->life;
        fade = 1.0f - frac;

        if (fade <= 0) {
            bl->time = 0; 
            continue;
        }

        assert(bl->numsegments < MAX_BEAM_SEGMENTS);

        for (j = 0; j < bl->numsegments; j++) {
            memcpy(newpoints, bl->segments[j].points, 4 * sizeof(polyVert_t));

            for (k = 0; k < 4; k++) {
                for (l = 0; l < 4; l++) {
                    newpoints[k].modulate[l] = bl->segments[j].points[k].modulate[l] * fade;
                }
            }

            cgi.R_AddPolyToScene(beamshader, 4, newpoints, be->renderfx);
        }
    }
}

void RenderSegment(Vector pt1a, Vector pt1b, Vector pt2a, Vector pt2b, byte modulate[4], int beamshader, int renderfx)
{
    int        i, j;
    polyVert_t points[4];

    VectorCopy(pt1a, points[0].xyz);
    VectorCopy(pt2a, points[1].xyz);
    VectorCopy(pt2b, points[2].xyz);
    VectorCopy(pt1b, points[3].xyz);

    points[0].st[0] = 1;
    points[0].st[1] = 1;
    points[1].st[0] = 0;
    points[1].st[1] = 1;
    points[2].st[0] = 0;
    points[2].st[1] = 0;
    points[3].st[0] = 1;
    points[3].st[1] = 0;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            points[i].modulate[j] = modulate[j];
        }
    }

    {
        static int n = 0;
        if (PROBE_Throttle(&n, 32, 2000)) {
            PROBE(
                "beam:addpoly",
                "RenderSegment shader=%d rf=0x%x v0=(%.1f,%.1f,%.1f) v1=(%.1f,%.1f,%.1f)",
                beamshader,
                renderfx,
                points[0].xyz[0],
                points[0].xyz[1],
                points[0].xyz[2],
                points[1].xyz[0],
                points[1].xyz[1],
                points[1].xyz[2]
            );
        }
    }
    cgi.R_AddPolyToScene(beamshader, 4, points, renderfx);
}

const int     MAX_SUBPOINTS = 256;
static Vector subpoints[MAX_SUBPOINTS];
static int    ptctr = 0;

static void CG_Subdivide(Vector a, Vector b, Vector c, Vector& out1, Vector& out2, Vector& out3)
{
    out1 = 0.5 * (a + b);
    out3 = 0.5 * (b + c);
    out2 = 0.5 * (out1 + out3);
}

void CG_MultiBeamBegin(void)
{
    ptctr = 0;
}

void CG_MultiBeamAddPoints(
    vec3_t start, vec3_t end, int numsegments, int flags, float minoffset, float maxoffset, qboolean addstartpoint
)
{
    Vector delta, dir, randdir;
    float  length;
    int    i;

    if (ptctr > MAX_SUBPOINTS) {
        return;
    }

    if (addstartpoint) {
        subpoints[ptctr++] = start;
    }

    delta  = Vector(end) - Vector(start);
    length = delta.length();
    length /= numsegments;

    dir = delta;
    dir.normalize();

    for (i = 1; i < numsegments; i++) {
        Vector newpt;

        if (ptctr > MAX_SUBPOINTS) {
            return;
        }

        randdir = Vector(crandom(), crandom(), crandom());

        newpt = Vector(start) + dir * i * length;
        newpt += minoffset * randdir + maxoffset * randdir;

        subpoints[ptctr++] = newpt;
    }
    subpoints[ptctr++] = end;
}

void CG_MultiBeamEnd(
    float scale, int renderfx, const char *beamshadername, byte modulate[4], int flags, int owner, float life
)
{
    Vector   prevpt, currpt;
    Vector   p1, p2, p3, p4, v1, v2, up, currpt1, currpt2, prevpt1, prevpt2;
    qboolean prevptvalid = false;
    int      i, beamshader;

    beamshader = cgi.R_RegisterShader(beamshadername);

    prevpt      = subpoints[0];
    prevptvalid = false;

    for (i = 1; i < ptctr; i++) {
        currpt = subpoints[i];
        v1 = prevpt - cg.refdef.vieworg;
        v2 = currpt - cg.refdef.vieworg;

#if 0
        cgi.R_DebugLine(prevpt, currpt, 1, 1, 1, 1);
        Vector pt = prevpt + up * 5;
        cgi.R_DebugLine(prevpt, pt, 0, 0, 1, 1);
#endif

        up.CrossProduct(v1, v2);
        up.normalize();

        currpt1 = currpt + (up * scale);
        currpt2 = currpt + (up * -scale);

        if (!prevptvalid) {
            prevpt1     = prevpt + up * scale;
            prevpt2     = prevpt + up * -scale;
            prevptvalid = true;
        }
#if 1
        RenderSegment(currpt1, currpt2, prevpt1, prevpt2, modulate, beamshader, renderfx);
#endif

        prevpt  = currpt;
        prevpt1 = currpt1;
        prevpt2 = currpt2;
    }

    if (flags & BEAM_PERSIST_EFFECT) {
        CG_AddBeamsFromList(owner, beamshader);
    }
}

static void CG_MultiBeamSubdivide(centity_t *cent)
{
    Vector     pt1, pt2, pt3;
    Vector     out1, out2, out3, out4, out5, out6, out7, out8, out9;
    centity_t *current;

    ptctr   = 0;
    current = cent;


    if (current->currentState.tag_num == ENTITYNUM_NONE) {
        cgi.DPrintf("CG_MultiBeamSubdivide : Multi beam entity does not have a child\n");
        return;
    }
    pt1 = current->lerpOrigin;

    current = &cg_entities[current->currentState.tag_num];
    if (current->currentState.eType != ET_MULTIBEAM) {
        return;
    }

    if (current->currentState.tag_num == ENTITYNUM_NONE) {
        cgi.DPrintf("CG_MultiBeamSubdivide : Multi beam entity does not have a child\n");
        return;
    }
    pt2 = current->lerpOrigin;

    current = &cg_entities[current->currentState.tag_num];
    if (current->currentState.eType != ET_MULTIBEAM) {
        return;
    }

    if (current->currentState.tag_num == ENTITYNUM_NONE) {
        cgi.DPrintf("CG_MultiBeamSubdivide : Multi beam entity does not have a child\n");
        return;
    }
    pt3 = current->lerpOrigin;

    subpoints[ptctr++] = pt1;

    while (1) {
        CG_Subdivide(pt1, pt2, pt3, out1, out2, out3);
        CG_Subdivide(pt1, out1, out2, out4, out5, out6);

        if ((ptctr + 4) > MAX_SUBPOINTS) {
            break;
        }

        subpoints[ptctr++] = out4;
        subpoints[ptctr++] = out5;
        subpoints[ptctr++] = out6;
        subpoints[ptctr++] = out2;

        if ((current->currentState.tag_num == ENTITYNUM_NONE) || (!current->currentValid)) {
            CG_Subdivide(out2, out3, pt3, out7, out8, out9);
            subpoints[ptctr++] = out7;
            subpoints[ptctr++] = out8;
            subpoints[ptctr++] = out9;
            subpoints[ptctr++] = pt3;
            break;
        }

        current = &cg_entities[current->currentState.tag_num];

        if (!current->currentValid) {
            break;
        }

        pt1 = out2;
        pt2 = pt3;
        pt3 = current->lerpOrigin;
    }
}

void CG_MultiBeam(centity_t *cent)
{
    Vector         prevpt, currpt;
    entityState_t *s1;
    Vector         p1, p2, p3, p4, v1, v2, up, currpt1, currpt2, prevpt1, prevpt2;
    const char    *beamshadername;
    int            beamshader;
    byte           modulate[4];
    qboolean       prevptvalid = false;
    int            i;

    s1 = &cent->currentState;

    if (!s1->surfaces[0]) {
        return;
    }

    CG_MultiBeamSubdivide(cent);

    beamshadername = CG_ConfigString(CS_IMAGES + s1->surfaces[1]); 
    beamshader     = cgi.R_RegisterShader(beamshadername);
    for (i = 0; i < 4; i++) {
        modulate[i] = cent->color[i] * 255;
    }

    if (ptctr < 3) {
        return;
    }

    prevpt      = subpoints[0];
    prevptvalid = false;

    for (i = 1; i < ptctr; i++) {
        currpt = subpoints[i];
        v1 = prevpt - cg.refdef.vieworg;
        v2 = currpt - cg.refdef.vieworg;

#if 0
        cgi.R_DebugLine(prevpt, currpt, 1, 1, 1, 1);
        Vector pt = prevpt + up * 5;
        cgi.R_DebugLine(prevpt, pt, 0, 0, 1, 1);
#endif

        up.CrossProduct(v1, v2);
        up.normalize();

        currpt1 = currpt + (up * s1->scale);
        currpt2 = currpt + (up * -s1->scale);

        if (!prevptvalid) {
            prevpt1     = prevpt + up * s1->scale;
            prevpt2     = prevpt + up * -s1->scale;
            prevptvalid = true;
        }

        RenderSegment(currpt1, currpt2, prevpt1, prevpt2, modulate, beamshader, s1->renderfx);

        prevpt  = currpt;
        prevpt1 = currpt1;
        prevpt2 = currpt2;
    }
}

void CG_BuildRendererBeam(
    const Vector& start,
    const Vector& end,
    float         angleVar,
    int           numSubdivisions,
    const byte    color[4],
    int           beamshader,
    float         scale,
    float         overlap,
    int           owner,
    float         phase,
    float         life,
    int           flags,
    float         startalpha,
    float         alphastep,
    int           renderfx,
    float         wavespeed,
    Vector        amp
)
{
    Vector     p1, p2, v1, v2, dir, prevpt1, prevpt2, nextpt, mid, delta, up;
    int        i, ii, jj;
    polyVert_t points[4];
    float      length;
    int        segnum  = 0;
    int        beamnum = 0;
    float      alphafactor;
    int        picW;

    if (flags & BEAM_PERSIST_EFFECT) {
        beamnum = CreateNewBeamEntity(owner, life);
        if (beamnum < 0) {
            return;
        }
    }


    picW = cgi.R_GetShaderWidth(beamshader);

    delta  = end - start;
    length = delta.length();
    length /= numSubdivisions;

    dir = delta;
    dir.normalize();

    v1 = start - cg.refdef.vieworg;
    v2 = end - cg.refdef.vieworg;
    up.CrossProduct(v1, v2);
    up.normalize();

    prevpt1 = start + (up * scale);
    prevpt2 = start + (up * -scale);
    p1      = start;

    for (i = 1; i <= numSubdivisions; i++) {
        p2 = start + (dir * i * length);

        if (i != numSubdivisions) {
            if (flags & BEAM_FULLWAVE) {
                float sinfrac = (float)sin((2.0 * M_PI * (double)i) / (double)numSubdivisions);
                float timedelta = (float)cg.time - phase;

                p2.x += (amp.x * sinfrac * timedelta) / 1000.0f;
                p2.y += (amp.y * sinfrac * timedelta) / 1000.0f;
                p2.z += (amp.z * sinfrac * timedelta) / 1000.0f;
            } else if (flags & BEAM_WAVE_EFFECT) {
                float sinephase = p2.x + p2.y;
                p2.z += sin(sinephase + cg.time) * angleVar;
            } else if (flags & BEAM_WAVE) {
                p2.z = (float)((double)angleVar * sin(M_PI * (double)i + (double)((float)cg.time / 1000.0f * wavespeed)) + (double)p2.z);
            } else if (flags & BEAM_USE_NOISE) {
                p2.x += cgi.R_Noise(p2.x, p2.y, p2.z, cg.time) * angleVar;
                p2.y += cgi.R_Noise(p2.x, p2.y, p2.z, cg.time) * angleVar;
                p2.z += cgi.R_Noise(p2.x, p2.y, p2.z, cg.time) * angleVar;
            } else {
                p2.x += Q_crandom(&seed) * angleVar;
                p2.y += Q_crandom(&seed) * angleVar;
                p2.z += Q_crandom(&seed) * angleVar;
            }
        }

        v1 = p1 - cg.refdef.vieworg;
        v2 = p2 - cg.refdef.vieworg;
        up.CrossProduct(v1, v2);
        up.normalize();

        VectorMA(p2, scale, up, points[0].xyz);
        VectorCopy(prevpt1, points[1].xyz);
        VectorCopy(prevpt2, points[2].xyz);
        VectorMA(p2, -scale, up, points[3].xyz);

        if (flags & BEAM_TILESHADER) 
        {
            float startS = (length * (i - 1)) / (float)picW;
            float endS   = (length * (i)) / (float)picW;

            points[0].st[0] = startS;
            points[0].st[1] = 1;
            points[1].st[0] = endS;
            points[1].st[1] = 1;
            points[2].st[0] = endS;
            points[2].st[1] = 0;
            points[3].st[0] = startS;
            points[3].st[1] = 0;
        } else {
            points[0].st[0] = 1;
            points[0].st[1] = 1;
            points[1].st[0] = 0;
            points[1].st[1] = 1;
            points[2].st[0] = 0;
            points[2].st[1] = 0;
            points[3].st[0] = 1;
            points[3].st[1] = 0;
        }

        if (!alphastep) {
            alphafactor = 1.0f;
        } else {
            alphafactor = startalpha + (alphastep * i);
        }

        for (ii = 0; ii < 4; ii++) {
            for (jj = 0; jj < 4; jj++) {
                points[ii].modulate[jj] = color[jj] * alphafactor;
            }
        }

        if (flags & BEAM_PERSIST_EFFECT) {
            AddBeamSegmentToList(owner, points, beamnum, segnum++, renderfx);
        } else {
            cgi.R_AddPolyToScene(beamshader, 4, points, renderfx);
        }

        if (overlap) {
            p2 = p2 + (dir * -overlap);
        }

        VectorMA(p2, scale, up, prevpt1);
        VectorMA(p2, -scale, up, prevpt2);
        p1 = p2;
    }
}

// CG_AddBeams' BEAM_INVERTED_FAST branch, which retail does not have either.

void CG_CreateModelBeam(beam_t *b, vec3_t org, vec3_t dist, float total_length, vec3_t ndir, vec3_t left, vec3_t up)
{
    int         tiki;
    vec3_t      mins, maxs;
    int         single_beam_length;
    refEntity_t ent;
    int         j;
    float       factor[3];
    float       t;
    vec3_t      angles;
    int         i;

    tiki = cgi.R_Model_GetHandle(b->hModel);

    cgi.TIKI_CalculateBounds(tiki, 1.0, mins, maxs);
    single_beam_length = maxs[0] - mins[0];

    memset(&ent, 0, sizeof(ent));

    for (j = 0; j < 3; j++) {
        factor[j] = 0.3f * crandom();
    }

    t = 0;

    while (t >= 0 && t < 1) {
        float  dot;
        vec3_t pdir;
        float  delta;
        vec3_t distance_point;

        VectorCopy(org, ent.origin);

        for (j = 0; j < 3; j++) {
            org[j] += dist[j] * (single_beam_length - b->overlap);
        }


        VectorMA(org, single_beam_length * factor[2], up, org);
        VectorMA(org, single_beam_length * factor[1], left, org);

        VectorSubtract(org, b->start, pdir);
        dot = DotProduct(pdir, ndir);
        t   = dot / total_length;

        VectorMA(b->start, total_length * t, ndir, distance_point);

        if (t > 0.1 && t < 0.9) {
            for (j = 0; j < 3; j++) {
                delta = org[j] - distance_point[j];
                if (delta > b->max_offset) {
                    org[j]    = distance_point[j] + b->max_offset;
                    factor[j] = -0.3 * crandom();
                } else if (delta < -b->max_offset) {
                    org[j]    = distance_point[j] - b->max_offset;
                    factor[j] = 0.3 * crandom();
                } else {
                    factor[j] = 0.3 * crandom();
                }
            }
        } else 
        {
            for (j = 0; j < 3; j++) {
                delta = org[j] - distance_point[j];
                if (delta > b->min_offset) {
                    org[j] -= 0.4 * single_beam_length;
                    factor[j] = -0.2f;
                } else if (delta < -b->min_offset) {
                    org[j] += 0.4 * single_beam_length;
                    factor[j] = 0.2f;
                } else {
                    factor[j] = 0;
                }
            }
        }

        VectorSubtract(org, ent.origin, dist);

        vectoangles(dist, angles);

        ent.hModel   = b->hModel;
        ent.scale    = b->scale;
        ent.renderfx = b->renderfx;

        for (i = 0; i < 4; i++) {
            ent.shaderRGBA[i] = b->shaderRGBA[i];
        }

        AnglesToAxis(angles, ent.axis);

        ent.fade = 1.0f; // not faded: retail computes the fade only in CG_ModelAnim
        cgi.R_AddRefEntityToScene(&ent);
    }
}

// free/active list this port used to walk does not exist.  Retail tests exactly
void CG_AddBeams(void)
{
    int     i, ii;
    beam_t *b;
    vec3_t  delta;
    vec3_t  angles;
    vec3_t  forward, left, up;
    float   length;
    byte    color[4];
    float   fade;

    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
        if (!b->hModel || b->endtime < cg.time) {
            if (b->endtime) {
                RemoveBeamList(b->entity);
                b->entity  = ENTITYNUM_NONE;
                b->endtime = 0;
            }
            continue;
        }

        fade = (float)(b->endtime - cg.time) / (float)b->life;

        {
            static int n = 0;
            if (PROBE_Throttle(&n, 32, 2000)) {
                PROBE(
                    "beam:slot",
                    "slot=%d ent=%d start=(%.1f,%.1f,%.1f) end=(%.1f,%.1f,%.1f) shader=%d "
                    "flags=0x%x",
                    i,
                    b->entity,
                    b->start[0],
                    b->start[1],
                    b->start[2],
                    b->end[0],
                    b->end[1],
                    b->end[2],
                    b->beamshader,
                    b->flags
                );
            }
        }

        for (ii = 0; ii < 4; ii++) {
            color[ii] = b->shaderRGBA[ii] * fade;
        }

        {
            static int widget_zl = 0;

            if (WIDGET_On() && WIDGET_Throttle(&widget_zl, 8, 64)) {
                float widget_dx = b->end[0] - b->start[0];
                float widget_dy = b->end[1] - b->start[1];
                float widget_dz = b->end[2] - b->start[2];

                if (widget_dx * widget_dx + widget_dy * widget_dy + widget_dz * widget_dz < 0.01f) {
                    WIDGET_A1_PROBE(
                        "beam:zerolen",
                        "slot=%d owner=%d name='%s' shader=%d start=(%.1f,%.1f,%.1f) "
                        "flags=0x%x subdiv=%d\n",
                        i,
                        b->entity,
                        b->name.c_str(),
                        b->beamshader,
                        b->start[0], b->start[1], b->start[2],
                        b->flags,
                        b->numSubdivisions);
                }
            }
        }

        if (b->flags & BEAM_TOGGLE) {
            if (cg.time > b->toggletime) {
                b->active = !b->active;
                if (b->flags & BEAM_RANDOM_TOGGLEDELAY) {
                    b->toggletime = cg.time + random() * b->toggledelay;
                } else {
                    b->toggletime = cg.time + b->toggledelay;
                }
            }
        }

        // 0x2B98: an inactive beam only re-draws its cached segments, and never
        if (!b->active) {
            CG_AddBeamsFromList(b->entity, b->beamshader);
            continue;
        }

        if ((b->flags & BEAM_PERSIST_EFFECT) && (b->update_time > cg.time)) {
            CG_AddBeamsFromList(b->entity, b->beamshader);
            continue;
        }

        b->update_time = cg.time + b->delay;

        if (!b->active) {
            continue;
        }

        if (b->flags & BEAM_USEMODEL) {
            VectorSubtract(b->start, b->end, delta);

            length = VectorLength(delta);

            vectoangles(delta, angles);
            AngleVectors(angles, forward, left, up);
            CG_CreateModelBeam(b, b->start, delta, length, forward, left, up);
        } else {
            if (b->flags & BEAM_SPHERE_EFFECT) {
                int k;

                VectorSubtract(b->start, b->end, delta);

                length = VectorLength(delta);

                for (k = 0; k < b->numspherebeams; k++) {
                    Vector offset(crandom(), crandom(), crandom());
                    Vector start(b->start + offset * b->sphereradius);
                    Vector end(b->start + offset * length);

                    CG_BuildRendererBeam(
                        start,
                        end,
                        b->max_offset,
                        b->numSubdivisions,
                        color,
                        b->beamshader,
                        b->scale,
                        (float)b->overlap,
                        b->entity,
                        (float)b->endtime - b->life,
                        b->life,
                        b->flags,
                        b->alpha,
                        b->alphastep,
                        b->renderfx,
                        b->wavespeed,
                        b->amp
                    );
                }
            } else {
                CG_BuildRendererBeam(
                    b->start,
                    b->end,
                    b->max_offset,
                    b->numSubdivisions,
                    color,
                    b->beamshader,
                    b->scale,
                    (float)b->overlap,
                    b->entity,
                    (float)b->endtime - b->life,
                    b->life,
                    b->flags,
                    b->alpha,
                    b->alphastep,
                    b->renderfx,
                    b->wavespeed,
                    b->amp
                );
            }
            if (b->flags & BEAM_PERSIST_EFFECT) {
                CG_AddBeamsFromList(b->entity, b->beamshader);
            }
        }
    }
}

void CG_CreateBeam(
    const vec3_t      start,
    const vec3_t      dir,
    int               owner,
    qhandle_t         hModel,
    float             alpha,
    float             scale,
    int               flags,
    float             length,
    int               life,
    qboolean          create,
    const vec3_t      endpointvec,
    float             min_offset,
    float             max_offset,
    int               overlap,
    int               numSubdivisions,
    int               delay,
    const char        *beamshadername,
    const byte        *modulate,
    int               numspherebeams,
    float             sphereradius,
    int               toggledelay,
    float             endalpha,
    int               renderfx,
    const char        *name,
    float             wavespeed,
    qboolean          canendearly,
    float             ampmagnitude
)
{
    int     i;
    beam_t *b;
    vec3_t  end;
    vec3_t  forward;
    trace_t trace;

    if (endpointvec) {
        VectorCopy(endpointvec, end);

        if (canendearly) {
            CG_Trace(&trace, start, vec3_origin, vec3_origin, end, 0, MASK_SHOT, false, true, "Create Beam");
            VectorCopy(trace.endpos, end);
        }
    } else {
        VectorMA(start, length, dir, end);
        CG_Trace(&trace, start, vec3_origin, vec3_origin, end, 0, MASK_SHOT, false, true, "Create Beam");
        VectorCopy(trace.endpos, end);
    }

    {
        static int n = 0;
        if (PROBE_Throttle(&n, 24, 400)) {
            PROBE(
                "beam:create",
                "owner=%d start=(%.1f,%.1f,%.1f) end=(%.1f,%.1f,%.1f) len=%.1f shader='%s' "
                "flags=0x%x create=%d name='%s'",
                owner,
                start[0],
                start[1],
                start[2],
                end[0],
                end[1],
                end[2],
                length,
                beamshadername ? beamshadername : "(null)",
                flags,
                create ? 1 : 0,
                name ? name : "(null)"
            );
        }
    }
    if (!create) {
        for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
            if (b->entity == owner) {
                if (name && b->name == name) {
                    b->endtime         = cg.time + life;
                    b->hModel          = hModel;
                    b->scale           = scale;
                    b->flags           = flags;
                    b->overlap         = overlap;
                    b->min_offset      = min_offset;
                    b->max_offset      = max_offset;
                    b->alpha           = alpha;
                    b->beamshader      = cgi.R_RegisterShader(beamshadername);
                    b->numSubdivisions = numSubdivisions;
                    b->delay           = delay;
                    b->life            = life;
                    b->numspherebeams  = numspherebeams;
                    b->sphereradius    = sphereradius;
                    b->renderfx        = renderfx;
                    b->wavespeed       = wavespeed;

                    if (alpha < 1.0f) {
                        b->shaderRGBA[3] = (int)(255.0f * alpha);
                    } else {
                        b->shaderRGBA[3] = modulate[3];
                    }

                    for (i = 0; i < 3; i++) {
                        b->shaderRGBA[i] = (int)((float)b->shaderRGBA[3] * (modulate[i] / 255.0f));
                    }

                    b->alphastep = ((endalpha - alpha) / (float)b->numSubdivisions);

                    VectorCopy(start, b->start);
                    VectorCopy(end, b->end);
                    return;
                }
            }
        }
    }

    // 0x3190..0x3368 -- the update-in-place arm above sets +0x98 but never touches
    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
        if (b->endtime < cg.time) {
            b->entity          = owner;
            b->endtime         = cg.time + life;
            b->hModel          = hModel;
            b->alpha           = alpha;
            b->scale           = scale;
            b->flags           = flags;
            b->overlap         = overlap;
            b->min_offset      = min_offset;
            b->max_offset      = max_offset;
            b->beamshader      = cgi.R_RegisterShader(beamshadername);
            b->numSubdivisions = numSubdivisions;
            b->delay           = delay;
            b->update_time     = 0; 
            b->life            = life;
            b->numspherebeams  = numspherebeams;
            b->sphereradius    = sphereradius;
            b->active          = true;
            b->toggledelay     = toggledelay;
            b->renderfx        = renderfx;
            b->wavespeed       = wavespeed;

            {
                vec3_t beamdir;
                vec3_t beamangles;
                vec3_t beamright, beamup;
                float  r1, r2;

                VectorSubtract(end, start, beamdir);
                vectoangles(beamdir, beamangles);
                AngleVectors(beamangles, forward, beamright, beamup);

                r1 = 2.0f * ((rand() & 0x3FFF) / 32766.0f - 0.5f);
                r2 = 2.0f * ((rand() & 0x3FFF) / 32766.0f - 0.5f);

                b->amp = Vector(beamright) * r1 + Vector(beamup) * r2;
                b->amp.normalize();
                b->amp *= ampmagnitude;
            }

            b->name            = name;

            if (alpha < 1.0f) {
                b->shaderRGBA[3] = (int)(255.0f * alpha);
            } else {
                b->shaderRGBA[3] = modulate[3];
            }

            for (i = 0; i < 3; i++) {
                b->shaderRGBA[i] = (int)((float)b->shaderRGBA[3] * (modulate[i] / 255.0f));
            }

            b->alphastep = ((endalpha - alpha) / (float)b->numSubdivisions);

            VectorCopy(start, b->start);
            VectorCopy(end, b->end);
            return;
        }
    }
    return;
}

void CG_KillBeams(int entity_number)
{
    int     i;
    beam_t *b;

    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
        if (b->entity == entity_number) {
            b->entity  = ENTITYNUM_NONE;
            b->endtime = 0;
        }
    }
}

void CG_RestartBeams(int timedelta)
{
    int     i;
    beam_t *b;

    for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++) {
        if (b->active && (b->update_time > cg.time)) {
            b->endtime -= timedelta;
            b->update_time -= timedelta;
            if (b->toggletime) {
                b->toggletime -= timedelta;
            }
        }
    }
}

void CG_Rope(centity_t *cent)
{
    Vector         prevpt, currpt;
    entityState_t *s1;
    Vector         top, mid, bottom, up, v1, v2;
    Vector         currpt1, currpt2, prevpt1, prevpt2;
    const char    *beamshadername;
    int            beamshader;
    byte           modulate[4];
    float          picH, length, endT;
    int            i, j;
    polyVert_t     points[4];

    s1 = &cent->currentState;

    top    = s1->origin2;
    mid    = cent->lerpOrigin;
    bottom = cent->lerpOrigin;
    bottom.z -= s1->alpha;

    beamshadername = CG_ConfigString(CS_IMAGES + s1->surfaces[0]); 
    beamshader     = cgi.R_RegisterShader(beamshadername);

    picH = cgi.R_GetShaderHeight(beamshader);

    for (i = 0; i < 4; i++) {
        modulate[i] = cent->color[i] * 255;
    }

    v1 = top - cg.refdef.vieworg;
    v2 = bottom - cg.refdef.vieworg;

    up.CrossProduct(v1, v2);
    up.normalize();

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            points[i].modulate[j] = modulate[j];
        }
    }
    points[0].st[0] = 1;
    points[1].st[0] = 1;
    points[2].st[0] = 0;
    points[3].st[0] = 0;

    prevpt1 = top + (up * s1->scale);
    prevpt2 = top + (up * -s1->scale);

    currpt1 = mid + (up * s1->scale);
    currpt2 = mid + (up * -s1->scale);

    length = Vector(mid - top).length();

    VectorCopy(currpt1, points[0].xyz);
    VectorCopy(prevpt1, points[1].xyz);
    VectorCopy(prevpt2, points[2].xyz);
    VectorCopy(currpt2, points[3].xyz);

    endT            = length / picH;
    points[0].st[1] = endT;
    points[3].st[1] = endT;
    points[1].st[1] = 0;
    points[2].st[1] = 0;

    cgi.R_AddPolyToScene(beamshader, 4, points, s1->renderfx);

    if (s1->alpha > 0) {
        prevpt1 = currpt1;
        prevpt2 = currpt2;
        currpt1 = bottom + (up * s1->scale);
        currpt2 = bottom + (up * -s1->scale);
        VectorCopy(currpt1, points[0].xyz);
        VectorCopy(prevpt1, points[1].xyz);
        VectorCopy(prevpt2, points[2].xyz);
        VectorCopy(currpt2, points[3].xyz);

        length += s1->alpha;

        points[1].st[1] = endT;
        points[2].st[1] = endT;
        endT            = length / picH;
        points[0].st[1] = endT;
        points[3].st[1] = endT;

        cgi.R_AddPolyToScene(beamshader, 4, points, s1->renderfx);
    }
}
