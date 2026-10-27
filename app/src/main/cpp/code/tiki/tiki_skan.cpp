/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction


#include "q_shared.h"
#include "qcommon.h"
#include "tiki_shared.h"
#include "tiki_skan.h"
#include "../skeletor/skeletor.h"
#include "tiki_files.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

#include <stddef.h>
#include <math.h>

/* The rotation stream IS the evidence for the tilt, so it carries its own budget
   instead of sharing the census window the anomaly families use. */
#define ROT_PROBE(tag, ...) PROBE_EMIT_N("[CHAN:", tag, 20000000, __VA_ARGS__)
#define TIKI_PROBE(tag, ...) PROBE_EMIT("[TIKI:", tag, __VA_ARGS__)
#define TIKI_MAP_CENSUS(tag, ...) PROBE_CENSUS_EMIT("[TIKI:", tag, __VA_ARGS__)

const char *TIKI_Skan_MapVerdictName(int verdict)
{
    switch (verdict) {
    case TIKI_MAPV_HANDLE:
        return "registered";
    case TIKI_MAPV_IDENTITY:
        return "identity";
    case TIKI_MAPV_NOSKEL:
        return "noskel";
    case TIKI_MAPV_TOOMANY:
        return "toomanybones";
    case TIKI_MAPV_TABLEFULL:
        return "tablefull";
    default:
        return "?";
    }
}

extern "C" void TIKI_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

extern "C" const char *TIKI_Anim_NameForNum(dtiki_t *pmdl, int animnum);

static_assert(sizeof(skelBone_t) == 16, "skelBone_t must be retail's 16-byte bone row");
static_assert(sizeof(skelFrame_t) == 56, "skelFrame_t must be retail's 40-byte header + one bone row");
static_assert(offsetof(skelFrame_t, radius) == 24, "skelFrame_t::radius must be at retail's +24");
static_assert(offsetof(skelFrame_t, delta) == 28, "skelFrame_t::delta must be at retail's +28");
static_assert(offsetof(skelFrame_t, bones) == 40, "skelFrame_t::bones must be at retail's +40");

static const int SKAN_FRAME_HEADER_SIZE = (int)(sizeof(skelFrame_t) - sizeof(skelBone_t));

struct skanRetained_t {
    const skelAnimDataGameHeader_t *owner;
    skelAnimHeaderRec_t            *hdr;
    size_t                          bytes;
};

static skanRetained_t m_skanRetained[MAX_TIKI_ALIASES];
static int            m_skanRetainedCount;

skelAnimHeaderRec_t *TIKI_Skan_RetainNative(const void *image, size_t bytes, const char *path)
{
    skelAnimHeaderRec_t *copy;
    int                  version;
    int                  numFrames;
    int                  numbones;
    int                  ofsChannelNames;
    int                  ofsFrames;
    size_t               need;

    if (!image || bytes < 112) {
        return NULL;
    }

    version         = LittleLong(*(const int *)((const byte *)image + 4));
    numFrames       = LittleLong(*(const int *)((const byte *)image + 76));
    numbones        = LittleLong(*(const int *)((const byte *)image + 80));
    ofsChannelNames = LittleLong(*(const int *)((const byte *)image + 104));
    ofsFrames       = LittleLong(*(const int *)((const byte *)image + 108));

    /* retail TIKI_LoadAnimSkel @0x000F4120: version 4 only, the exact size
       equation, and no clamp -- a frame count the bytes cannot hold is a load
       error, because reading it walks past the buffer into whatever the
       allocator left there, and that lands in the bone rows as a quat. */
    if (version != 4) {
        TIKI_Error("TIKI_LoadAnimSkel: %s has wrong version (%i should be 4)\n", path, version);
        return NULL;
    }

    if (numFrames <= 0 || numbones <= 0 || numbones > TIKI_MAX_BONES) {
        TIKI_Error("TIKI_LoadAnimSkel: %s has no frames or too many bones (%i)\n", path, numbones);
        return NULL;
    }

    need = (size_t)numFrames * (SKAN_FRAME_HEADER_SIZE + (size_t)numbones * sizeof(skelBone_t))
         + (size_t)numbones * 36 + 112;
    if (need != bytes || ofsFrames != (int)(numbones * 36 + 112) || ofsChannelNames != 112) {
        TIKI_Error("TIKI_LoadAnimSkel: %s is %u bytes, wants %u (frames %i bones %i)\n",
                   path,
                   (unsigned)bytes,
                   (unsigned)need,
                   numFrames,
                   numbones);
        return NULL;
    }

    if (m_skanRetainedCount >= MAX_TIKI_ALIASES) {
        Com_DPrintf("TIKI_Skan_RetainNative: native SKAN image table full (%d)\n", MAX_TIKI_ALIASES);
        /* the animation still plays -- only its bone mapping is lost to it, and the
           loss is silent otherwise.  This row is the reason a slot stays -1. */
        TIKI_PROBE("map", "table full anim='%s' count=%d\n", path ? path : "?", m_skanRetainedCount);
        return NULL;
    }

    copy = (skelAnimHeaderRec_t *)Skel_Alloc(need);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, image, need);
    TIKI_SwapAnimSkel(copy);
    copy->numFrames = numFrames;

    m_skanRetained[m_skanRetainedCount].owner = NULL;
    m_skanRetained[m_skanRetainedCount].hdr   = copy;
    m_skanRetained[m_skanRetainedCount].bytes = need;
    m_skanRetainedCount++;

    return copy;
}

void TIKI_Skan_Bind(skelAnimDataGameHeader_t *owner, skelAnimHeaderRec_t *hdr)
{
    int i;

    if (!owner || !hdr) {
        return;
    }

    for (i = 0; i < m_skanRetainedCount; i++) {
        if (m_skanRetained[i].hdr == hdr) {
            m_skanRetained[i].owner = owner;
            return;
        }
    }
}

void TIKI_Skan_Release(const skelAnimDataGameHeader_t *owner)
{
    int i;

    if (!owner) {
        return;
    }

    for (i = 0; i < m_skanRetainedCount; i++) {
        if (m_skanRetained[i].owner == owner) {
            Skel_Free(m_skanRetained[i].hdr);
            m_skanRetained[i]       = m_skanRetained[m_skanRetainedCount - 1];
            m_skanRetainedCount--;
            return;
        }
    }
}

skelAnimHeaderRec_t *TIKI_Skan_Find(const skelAnimDataGameHeader_t *owner)
{
    int i;

    if (!owner) {
        return NULL;
    }

    for (i = 0; i < m_skanRetainedCount; i++) {
        if (m_skanRetained[i].owner == owner) {
            return m_skanRetained[i].hdr;
        }
    }

    return NULL;
}

skelAnimHeaderRec_t *TIKI_Skan_FindByAnim(dtiki_t *tiki, int animnum)
{
    if (!tiki || !tiki->a) {
        return NULL;
    }

    if (animnum < 0 || animnum >= tiki->a->num_anims) {
        return NULL;
    }

    return TIKI_Skan_Find(SkeletorCacheGetData(tiki->a->m_aliases[animnum]));
}

/* retail TIKI_SetupTikiAnim @0x000F4400 registers into any slot still holding -1 and
   TIKI_GetAnim @0x000F4810 re-enters it on every fetch, so a slot the build could not
   fill lands on first use.  The port poses straight off the records and never re-enters
   the setup, so the retry lives here: the one site that provably runs with the
   animation's data resident.  A failed registration stays UNSET and is retried, which
   is retail's own behaviour; a resolved identity latches and stops costing Calc. */
const int *TIKI_Skan_AnimBoneMapping(dtiki_t *tiki, int anim)
{
    static int s_healMiss;
    skelAnimHeaderRec_t *hdr;
    const char          *name;
    int                  handle;
    int                  mapped;
    int                  verdict;

    if (!tiki || !tiki->a || !tiki->boneMappings) {
        return NULL;
    }

    if (anim < 0 || anim >= tiki->a->num_anims) {
        return NULL;
    }

    handle = tiki->boneMappings[anim];
    if (handle >= 0) {
        const int *map = TIKI_GetAnimBoneMapping(handle);

        if (map) {
            return map;
        }

        /* retail TIKI_FreeTiki @0x000FD240 frees only at zero uses, so a handle that
           resolves to nothing is a dead entry a survivor still holds: drop the slot
           back to unset so the refill below rebuilds it instead of reading by bone
           index for good. */
        tiki->boneMappings[anim] = TIKI_BONEMAP_UNSET;
    } else if (handle != TIKI_BONEMAP_UNSET) {
        return NULL;
    }

    hdr  = TIKI_Skan_FindByAnim(tiki, anim);
    name = TIKI_Anim_NameForNum(tiki, anim);

    if (!hdr || hdr->ident != TIKI_SKC_HEADER_IDENT) {
        /* data resident and still no native SKAN header: it never gets one (.tan or
           .skc), so latch instead of re-probing every pose.  An anim whose data is
           absent stays UNSET -- that one is transient and the retry IS the heal. */
        if (SkeletorCacheEntryLoaded(tiki->a->m_aliases[anim])) {
            tiki->boneMappings[anim] = TIKI_BONEMAP_NOHEADER;
        }

        if (PROBE_On() && PROBE_Throttle(&s_healMiss, 8, 600)) {
            PROBE_CENSUS_EMIT(
                "[TIKI:",
                "map",
                "heal tiki=%s anim=%d name='%s' handle=%d calc=%s misses=%d\n",
                tiki->name ? tiki->name : "?",
                anim,
                name ? name : "?",
                tiki->boneMappings[anim],
                hdr ? "ident" : "nohdr",
                s_healMiss
            );
        }
        return NULL;
    }

    mapped = TIKI_RegisterAnimBoneMappingV(hdr, tiki->skelIndex, &verdict);
    if (verdict == TIKI_MAPV_IDENTITY) {
        tiki->boneMappings[anim] = TIKI_BONEMAP_IDENTITY;
    } else if (mapped >= 0) {
        tiki->boneMappings[anim] = mapped;
        TIKI_MAP_CENSUS(
            "map",
            "heal tiki=%s anim=%d name='%s' handle=%d calc=%s\n",
            tiki->name ? tiki->name : "?",
            anim,
            name ? name : "?",
            tiki->boneMappings[anim],
            TIKI_Skan_MapVerdictName(verdict)
        );
    } else if (PROBE_On() && PROBE_Throttle(&s_healMiss, 8, 600)) {
        PROBE_CENSUS_EMIT(
            "[TIKI:",
            "map",
            "heal tiki=%s anim=%d name='%s' handle=-1 calc=%s misses=%d\n",
            tiki->name ? tiki->name : "?",
            anim,
            name ? name : "?",
            TIKI_Skan_MapVerdictName(verdict),
            s_healMiss
        );
    }

    if (mapped < 0) {
        return NULL;
    }

    return TIKI_GetAnimBoneMapping(mapped);
}

/* retail TIKI_RefreshCachedTIKI @0x000FBE60 rewrites every anim bone mapping from the
   live data on a warm hit.  Divergence: this one only fills a slot still unset, so a
   warm hit does not pay Calc again; the walk in TIKI_Skan_AnimBoneMapping holds the
   same guarantee retail's unconditional rewrite bought. */
void TIKI_RefreshAnimBoneMapping(dtiki_t *tiki, int animnum)
{
    skelAnimHeaderRec_t *hdr;
    const char          *name;
    int                  verdict = TIKI_MAPV_NOSKEL;
    int                  mapped;

    if (!tiki || !tiki->a || !tiki->boneMappings) {
        return;
    }

    if (animnum < 0 || animnum >= tiki->a->num_anims) {
        return;
    }

    if (tiki->boneMappings[animnum] != TIKI_BONEMAP_UNSET) {
        return;
    }

    /* only a slot whose data is already paged in: this path is load time, not the
       walk, and the walk pages in and fills whatever this one left */
    if (!SkeletorCacheEntryLoaded(tiki->a->m_aliases[animnum])) {
        return;
    }

    hdr  = TIKI_Skan_FindByAnim(tiki, animnum);
    name = TIKI_Anim_NameForNum(tiki, animnum);

    if (!hdr || hdr->ident != TIKI_SKC_HEADER_IDENT) {
        TIKI_PROBE(
            "map",
            "fill tiki=%s anim=%d name='%s' handle=-1 calc=%s\n",
            tiki->name ? tiki->name : "?",
            animnum,
            name ? name : "?",
            hdr ? "ident" : "nohdr"
        );
        return;
    }

    mapped = TIKI_RegisterAnimBoneMappingV(hdr, tiki->skelIndex, &verdict);
    if (verdict == TIKI_MAPV_IDENTITY) {
        tiki->boneMappings[animnum] = TIKI_BONEMAP_IDENTITY;
    } else if (mapped >= 0) {
        tiki->boneMappings[animnum] = mapped;
    }

    TIKI_PROBE(
        "map",
        "fill tiki=%s anim=%d name='%s' handle=%d calc=%s\n",
        tiki->name ? tiki->name : "?",
        animnum,
        name ? name : "?",
        tiki->boneMappings[animnum],
        TIKI_Skan_MapVerdictName(verdict)
    );
}

void TIKI_SwapAnimSkel(skelAnimHeaderRec_t *hdr)
{
    skelFrame_t *frame;
    float       *f;
    int          i;
    int          j;
    int          k;
    int          ofsFrames;

    hdr->ident           = LittleLong(hdr->ident);
    hdr->version         = LittleLong(hdr->version);
    hdr->numFrames       = LittleLong(hdr->numFrames);
    hdr->numbones        = LittleLong(hdr->numbones);
    hdr->totaltime       = LittleFloat(hdr->totaltime);
    hdr->frametime       = LittleFloat(hdr->frametime);
    hdr->totaldelta[0]   = LittleFloat(hdr->totaldelta[0]);
    hdr->totaldelta[1]   = LittleFloat(hdr->totaldelta[1]);
    hdr->totaldelta[2]   = LittleFloat(hdr->totaldelta[2]);
    hdr->ofsChannelNames = LittleLong(hdr->ofsChannelNames);
    ofsFrames            = LittleLong(hdr->ofsFrames);
    hdr->ofsFrames       = ofsFrames;

    frame = (skelFrame_t *)((byte *)hdr + ofsFrames);
    for (i = 0; i < hdr->numFrames; i++) {
        frame->radius = LittleFloat(frame->radius);

        f = (float *)frame;
        for (j = 0; j < 3; j++) {
            f[0] = LittleFloat(f[0]);
            f[3] = LittleFloat(f[3]);
            f[7] = LittleFloat(f[7]);
            f++;
        }

        frame = (skelFrame_t *)((byte *)frame + sizeof(skelFrame_t));

        f = (float *)frame;
        for (k = 0; k < hdr->numbones; k++) {
            unsigned short *s = (unsigned short *)f;

            for (j = 0; j < 4; j++) {
                s[0] = LittleShort(s[0]);
                if (j < 3) {
                    s[4] = LittleShort(s[4]);
                }
                s++;
            }

            f += 4; 
        }
    }

    f = (float *)((byte *)hdr + hdr->ofsChannelNames);
    for (i = 0; i < hdr->numbones; i++) {
        f[0] = LittleFloat(f[0]);
        f += 9;
    }
}

qboolean TIKI_ValidateSkelAnimAndFrame(
    dtiki_t              *tiki,
    int                   animnum,
    int                   framenum,
    skelAnimHeaderRec_t **hdrOut,
    skelFrameRec_t      **frameOut
)
{
    skelAnimHeaderRec_t *hdr;
    const char          *animName;

    if (!tiki || !hdrOut || !frameOut) {
        return qfalse;
    }

    *hdrOut   = NULL;
    *frameOut = NULL;

    if (!tiki->a || animnum >= tiki->a->num_anims || animnum < 0) {
        Com_WPrintf("TIKI_ValidateSkelAnimAndFrame: no such anim %d for model '%s'\n", animnum, tiki->name);
        return qfalse;
    }

    hdr = TIKI_Skan_FindByAnim(tiki, animnum);
    if (!hdr) {
        animName = TIKI_Anim_NameForNum(tiki, animnum);
        Com_WPrintf(
            "TIKI_ValidateSkelAnimAndFrame: couldn't get anim %d (alias: %s) in '%s'\n",
            animnum,
            animName ? animName : "",
            tiki->name
        );
        return qfalse;
    }

    if (framenum >= hdr->numFrames || framenum < 0) {
        Com_WPrintf(
            "TIKI_ValidateSkelAnimAndFrame: no such frame %d in anim %d (name: %s) in tiki '%s'\n",
            framenum,
            animnum,
            hdr->name,
            tiki->name
        );
        return qfalse;
    }

    *hdrOut   = hdr;
    *frameOut = (skelFrameRec_t *)((byte *)hdr + hdr->ofsFrames + framenum * (SKAN_FRAME_HEADER_SIZE + (int)sizeof(skelBone_t) * hdr->numbones));
    return qtrue;
}

#define TIKI_SKAN_QUAT_SCALE   (1.0f / 32767.0f)
#define TIKI_SKAN_OFFSET_SCALE (1.0f / 63.0f)

static void TIKI_Skan_DequantRow(const short int *row, float *quat, float *offset)
{
    int i;

    for (i = 0; i < 4; i++) {
        quat[i] = TIKI_SKAN_QUAT_SCALE * (float)row[i];
    }
    for (i = 0; i < 3; i++) {
        offset[i] = TIKI_SKAN_OFFSET_SCALE * (float)row[4 + i];
    }
}

static void TIKI_Skan_QuatMul(const float *q, const float *p, float *r)
{
    r[0] = q[3] * p[0] + q[0] * p[3] + q[1] * p[2] - q[2] * p[1];
    r[1] = q[3] * p[1] + q[1] * p[3] + q[2] * p[0] - q[0] * p[2];
    r[2] = q[3] * p[2] + q[2] * p[3] + q[0] * p[1] - q[1] * p[0];
    r[3] = q[3] * p[3] - q[0] * p[0] - q[1] * p[1] - q[2] * p[2];
}

/* No map means the name tables matched positionally at registration, so retail's
   bone-index read is exact.  A mapped channel past this anim's own table is the
   same "no such channel" case as -1: it takes the base row, never a byte from
   the next frame record. */
static int TIKI_Skan_ChannelForBone(const int *mapping, skelAnimHeader_t *hdr, int bone)
{
    if (mapping) {
        if (hdr && bone >= 0 && bone < hdr->numbones) {
            int chan = mapping[bone];
            if (chan >= 0 && chan < hdr->numbones) {
                return chan;
            }
        }
        return -1;
    }
    if (hdr && bone >= 0 && bone < hdr->numbones) {
        return bone;
    }
    return -1;
}

/* fgame binds the head per class: player HEAD_TAG=0, actor ACTOR_HEAD_TAG=1. */
static int TIKI_Skan_HeadLayout(skelHeaderGame_t *skel, const int *bone_tag)
{
    static const char *head = "Bip01 Head";
    int                n;
    int                b;

    for (n = 0; n < 2; n++) {
        b = bone_tag[n];
        if (b < 0 || b >= skel->numBones) {
            continue;
        }
        const char *nm = skel_BoneNames.FindName(skel->pBones[b].channel);
        if (nm && !stricmp(nm, head)) {
            return n;
        }
    }
    return -1;
}

/* The renderer latches who is being posed so the rows can name their subject. */
static int   s_probeEnt = -1;
static vec3_t s_probeOrg;

void TIKI_Skan_SetProbeSubject(int entnum, const vec3_t origin)
{
    s_probeEnt = entnum;
    if (origin) {
        VectorCopy(origin, s_probeOrg);
    } else {
        VectorClear(s_probeOrg);
    }
}

static const float skanIdentityQuat[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

typedef struct {
    skelAnimHeader_t *mainHdr;
    skelFrame_t      *mainRec;
    const int        *mainMap;

    skelAnimHeader_t *aHdr;    
    skelFrame_t      *aRec;
    const int        *aMap;

    skelAnimHeader_t *crossHdr;
    skelFrame_t      *crossRec;
    const int        *crossMap;

    int               mode;     
    float             innerT;  
    float             outerT;  
} skelSolveChannel_t;

/* Every bone_quat writer is unit-preserving, so the value's shape names the site. */
const char *TIKI_Skan_QuatWriter(const float *q)
{
    float len2;

    len2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (len2 != len2) {
        return "e2q"; /* msg.cpp:1412 / entity.cpp:5171 / level.cpp:797 */
    }
    if (len2 == 0.0f) {
        return "gate";
    }
    return "slerp"; /* cg_modelanim.c:884 */
}

/* A non-unit quat reaching QuatToMat scales and shears the bone, so both the
   controller quat and the composed world quat are measured here.  The nonunit
   row carries the whole chain so one boot names the site: the sampled row, the
   solved local quat or the parent's stored quat. */
static void TIKI_Skan_QuatLenProbe(
    dtiki_t                  *tiki,
    skelHeaderGame_t         *skel,
    int                       bone,
    const char               *boneName,
    int                       slot,
    const float              *cq,
    const float              *w,
    const skelSolveChannel_t *ch,
    int                       chan,
    const short int          *row,
    const float              *localQuat,
    int                       parent,
    const float              *parentQuat,
    int                       legs,
    int                       anim,
    int                       frame,
    int                       oldanim,
    int                       oldframe,
    float                     framelerp,
    float                     cblerp)
{
    static int  baseline;
    float       clen;
    float       wlen;
    float       llen;
    float       plen;
    const char *cw;
    const char *ww;
    const char *chanName;
    const char *parName;

    if (!PROBE_On()) {
        return;
    }

    clen = sqrt(cq[0] * cq[0] + cq[1] * cq[1] + cq[2] * cq[2] + cq[3] * cq[3]);
    wlen = sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2] + w[3] * w[3]);

    /* negated range, not the positive pair: a NaN length must fire */
    if (!(clen >= 0.999f && clen <= 1.001f)) {
        cw = TIKI_Skan_QuatWriter(cq);
        CHAN_PROBE(
            "head",
            "why=ctl tiki=%s slot=%d bone=%d name='%s' clen=%.4f ctl=(%.3f,%.3f,%.3f,%.3f) writer=%s\n",
            tiki->name,
            slot,
            bone,
            boneName ? boneName : "?",
            clen,
            cq[0],
            cq[1],
            cq[2],
            cq[3],
            cw
        );
    }
    /* 0.99, not 0.999: dequant rounding over a 7-bone chain wobbles to 0.998-0.999
       on every bone every frame, and the artifact class this row exists for
       measured 0.95 and 1.42, so 0.999 fired constantly and starved the log */
    if (!(wlen >= 0.99f && wlen <= 1.01f)) {
        llen = sqrt(localQuat[0] * localQuat[0] + localQuat[1] * localQuat[1] + localQuat[2] * localQuat[2]
                    + localQuat[3] * localQuat[3]);
        ww = TIKI_Skan_QuatWriter(w);
        chanName = NULL;
        if (ch && ch->mainHdr && chan >= 0 && chan < ch->mainHdr->numbones) {
            chanName = (const char *)ch->mainHdr + ch->mainHdr->ofsChannelNames + 36 * chan + 4;
        }
        parName = NULL;
        if (parent >= 0 && parent < skel->numBones) {
            parName = skeletor_c::m_boneNames.FindName(skel->pBones[parent].channel);
        }
        plen = parentQuat ? sqrt(parentQuat[0] * parentQuat[0] + parentQuat[1] * parentQuat[1]
                                 + parentQuat[2] * parentQuat[2] + parentQuat[3] * parentQuat[3])
                          : 0.0f;
        PROBE_CENSUS_EMIT(
            "[CHAN:", "head",
            "why=nonunit tiki=%s bone=%d name='%s' slot=%d clen=%.4f wlen=%.4f src=%s "
            "ent=%d org=(%.1f,%.1f,%.1f) ch=%s mode=%d t=%.3f anim=%d frame=%d oldanim=%d oldframe=%d "
            "flerp=%.3f cb=%.3f\n",
            tiki->name,
            bone,
            boneName ? boneName : "?",
            slot,
            clen,
            wlen,
            ww,
            s_probeEnt,
            s_probeOrg[0],
            s_probeOrg[1],
            s_probeOrg[2],
            legs ? "legs" : "torso",
            ch ? ch->mode : -1,
            ch ? ((ch->mode == 3) ? ch->outerT : ch->innerT) : 0.0f,
            anim,
            frame,
            oldanim,
            oldframe,
            framelerp,
            cblerp
        );
        PROBE_CENSUS_EMIT(
            "[CHAN:", "head",
            "why=row tiki=%s bone=%d chan=%d channame='%s' row=(%d,%d,%d,%d) "
            "lq=(%.4f,%.4f,%.4f,%.4f) llen=%.4f\n",
            tiki->name,
            bone,
            chan,
            chanName ? chanName : "?",
            row ? row[0] : 0,
            row ? row[1] : 0,
            row ? row[2] : 0,
            row ? row[3] : 0,
            localQuat[0],
            localQuat[1],
            localQuat[2],
            localQuat[3],
            llen
        );
        PROBE_CENSUS_EMIT(
            "[CHAN:", "head",
            "why=par tiki=%s bone=%d par=%d parname='%s' pq=(%.4f,%.4f,%.4f,%.4f) plen=%.4f\n",
            tiki->name,
            bone,
            parent,
            parName ? parName : "?",
            parentQuat ? parentQuat[0] : 0.0f,
            parentQuat ? parentQuat[1] : 0.0f,
            parentQuat ? parentQuat[2] : 0.0f,
            parentQuat ? parentQuat[3] : 0.0f,
            plen
        );
        return;
    }
    if (boneName && !stricmp(boneName, "Bip01 Head") && baseline < 8) {
        baseline++;
        CHAN_PROBE(
            "head",
            "why=len tiki=%s bone=%d name='%s' slot=%d clen=%.4f wlen=%.4f\n",
            tiki->name,
            bone,
            boneName,
            slot,
            clen,
            wlen
        );
    }
}

/* A wrong-but-unit rotation carries no length anomaly, so the head's controller
   and its composed pose are read as axis-angle here. */
static void TIKI_Skan_QuatAxisAngle(const float *q, float *axis, float *deg)
{
    float s;
    float w;

    w = q[3];
    if (w > 1.0f) {
        w = 1.0f;
    } else if (w < -1.0f) {
        w = -1.0f;
    }
    *deg = 2.0f * acos(w) * (180.0f / M_PI);
    s = sqrt(1.0f - w * w);
    if (s < 1e-6f) {
        axis[0] = 0.0f;
        axis[1] = 0.0f;
        axis[2] = 1.0f;
    } else {
        axis[0] = q[0] / s;
        axis[1] = q[1] / s;
        axis[2] = q[2] / s;
    }
}

/* The upper chain the tilt can be carried by: head-only is the head aim
   controller, Neck is the neck row or blend, Pelvis..Spine2 is the torso arm. */
static qboolean TIKI_Skan_RotChainBone(const char *boneName)
{
    static const char *const chain[] = {
        "Bip01 Pelvis", "Bip01 Spine", "Bip01 Spine1", "Bip01 Spine2", "Bip01 Neck", "Bip01 Head"
    };
    int i;

    if (!boneName) {
        return qfalse;
    }
    for (i = 0; i < (int)(sizeof(chain) / sizeof(chain[0])); i++) {
        if (!stricmp(boneName, chain[i])) {
            return qtrue;
        }
    }
    return qfalse;
}

static void TIKI_Skan_RotProbe(
    dtiki_t          *tiki,
    int               bone,
    const char       *boneName,
    int               slot,
    const float      *cq,
    const float      *w,
    qboolean          legs,
    int               anim,
    int               frame,
    int               oldanim,
    int               oldframe,
    float             framelerp
)
{
    float caxis[3];
    float waxis[3];
    float cdeg;
    float wdeg;
    float cang[3];
    float wang[3];

    static float    s_prev[TIKI_MAX_BONES][3];
    static qboolean s_have[TIKI_MAX_BONES];
    static int      s_tail[TIKI_MAX_BONES];
    static int      s_rows;
    static qboolean s_noted;
    static qboolean s_armed;
    qboolean        tail;
    qboolean        moved;
    int             i;
    int             key;
    float           d;

    if (!TIKI_Skan_RotChainBone(boneName)) {
        return;
    }

    /* the arm prints its own arming once, so a log that has no rot rows is a log
       where the arm did not take and not a log where the pose never moved */
    if (!s_armed) {
        s_armed = qtrue;
        ROT_PROBE(
            "head",
            "why=rotarm bones=6 budget=20000000 tails=30 trigger=5 ent=%d tiki=%s\n",
            s_probeEnt,
            tiki->name
        );
    }

    /* a wrong-but-unit rotation never trips a length test, so this arm samples
       every armed frame and additionally latches a tail whenever the pose moves,
       because the cant is a transition into a held pose and a cadence can land
       on either side of it.  Static state only: nothing is allocated per frame. */
    key = (int)(((unsigned)s_probeEnt * 31u + (unsigned)bone) % TIKI_MAX_BONES);

    moved = qfalse;
    if (s_have[key]) {
        for (i = 0; i < 3; i++) {
            d = wang[i] - s_prev[key][i];
            if (d < 0.0f) {
                d = -d;
            }
            if (d > 180.0f) {
                d = 360.0f - d;
            }
            if (d > 5.0f) {
                moved = qtrue;
            }
        }
    }
    for (i = 0; i < 3; i++) {
        s_prev[key][i] = wang[i];
    }
    s_have[key] = qtrue;

    if (moved) {
        s_tail[key] = 30;
    } else if (s_tail[key] > 0) {
        s_tail[key]--;
    }
    tail = (s_tail[key] > 0);

    if (!tail) {
        if (s_rows >= 20000000) {
            if (!s_noted) {
                s_noted = qtrue;
                ROT_PROBE(
                    "head",
                    "why=rotcap note=every-frame rot rows capped at 20000000 frames=%d; change-triggered tails still print\n", s_rows
                );
            }
            return;
        }
    }
    s_rows++;

    TIKI_Skan_QuatAxisAngle(cq, caxis, &cdeg);
    TIKI_Skan_QuatAxisAngle(w, waxis, &wdeg);
    QuatToAngles(cq, cang);
    QuatToAngles(w, wang);

    ROT_PROBE(
        "head",
        "why=rot tiki=%s bone=%d name='%s' slot=%d ent=%d ch=%s anim=%d frame=%d oldanim=%d "
        "oldframe=%d flerp=%.3f "
        "caxis=(%.3f,%.3f,%.3f) cdeg=%.2f cang=(%.2f,%.2f,%.2f) "
        "waxis=(%.3f,%.3f,%.3f) wdeg=%.2f wang=(%.2f,%.2f,%.2f)\n",
        tiki->name,
        bone,
        boneName,
        slot,
        s_probeEnt,
        legs ? "legs" : "torso",
        anim,
        frame,
        oldanim,
        oldframe,
        framelerp,
        caxis[0],
        caxis[1],
        caxis[2],
        cdeg,
        cang[0],
        cang[1],
        cang[2],
        waxis[0],
        waxis[1],
        waxis[2],
        wdeg,
        wang[0],
        wang[1],
        wang[2]
    );
}

/* Walks the ancestors of a short quat so one row names the bone that creates
   the shortening and every bone that merely inherits it. */
static void TIKI_Skan_ChainProbe(
    dtiki_t          *tiki,
    skelHeaderGame_t *skel,
    const float      *world,
    int               bone,
    int               slot
)
{
    char  chain[192];
    int   b;
    int   n;
    float len;

    chain[0] = 0;
    n = 0;
    b = skel->pBones[bone].skelParent;
    while (b >= 0 && b < skel->numBones && n < 8) {
        const float *q = &world[b * 7];
        const char  *nm = skeletor_c::m_boneNames.FindName(skel->pBones[b].channel);

        len = sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        Q_strcat(
            chain,
            sizeof(chain),
            va("%s%d'%s'=%.4f", n ? " " : "", b, nm ? nm : "?", len)
        );
        n++;
        b = skel->pBones[b].skelParent;
    }

    CHAN_PROBE(
        "head",
        "why=chain tiki=%s bone=%d slot=%d ent=%d chain=\"%s\"\n",
        tiki->name,
        bone,
        slot,
        s_probeEnt,
        chain
    );
}

/* A non-unit quat after the controller fold, with every ancestor unit, convicts
   the fold itself; this row names the slot, the loss and the bones that inherit. */
static void TIKI_Skan_FoldProbe(
    dtiki_t          *tiki,
    skelHeaderGame_t *skel,
    int               bone,
    const char       *boneName,
    int               slot,
    const float      *pre,
    const float      *post,
    const float      *cq
)
{
    static int rows;
    char       kids[192];
    float      plen;
    float      qlen;
    int        b;
    int        n;

    if (!PROBE_On() || rows >= 64) {
        return;
    }
    rows++;

    plen = sqrt(pre[0] * pre[0] + pre[1] * pre[1] + pre[2] * pre[2] + pre[3] * pre[3]);
    qlen = sqrt(post[0] * post[0] + post[1] * post[1] + post[2] * post[2] + post[3] * post[3]);

    kids[0] = 0;
    n = 0;
    for (b = 0; b < skel->numBones && n < 8; b++) {
        const char *nm;

        if (skel->pBones[b].skelParent != bone) {
            continue;
        }
        nm = skeletor_c::m_boneNames.FindName(skel->pBones[b].channel);
        Q_strcat(kids, sizeof(kids), va("%s%d'%s'", n ? " " : "", b, nm ? nm : "?"));
        n++;
    }

    CHAN_PROBE(
        "head",
        "why=fold tiki=%s bone=%d name='%s' slot=%d ent=%d pre=%.4f post=%.4f "
        "cq=(%.4f,%.4f,%.4f,%.4f) clen=%.4f kids=\"%s\"\n",
        tiki->name,
        bone,
        boneName ? boneName : "?",
        slot,
        s_probeEnt,
        plen,
        qlen,
        cq[0],
        cq[1],
        cq[2],
        cq[3],
        sqrt(cq[0] * cq[0] + cq[1] * cq[1] + cq[2] * cq[2] + cq[3] * cq[3]),
        kids
    );
}

/* One row per folded bone naming the surfaces skinned to it, so the row says
   which visible geometry the corrupt matrix reaches. */
static void TIKI_Skan_SkinProbe(dtiki_t *tiki, skelHeaderGame_t *skel, int bone, const char *boneName)
{
    static int         done[TIKI_MAX_BONES];
    static const dtiki_t *doneTiki;
    char               surf[256];
    skelSurfaceGame_t *s;
    int                n;

    if (!PROBE_On() || bone < 0 || bone >= TIKI_MAX_BONES) {
        return;
    }
    if (doneTiki != tiki) {
        doneTiki = tiki;
        memset(done, 0, sizeof(done));
    }
    if (done[bone]) {
        return;
    }
    done[bone] = 1;

    surf[0] = 0;
    n = 0;
    for (s = skel->pSurfaces; s && n < 6; s = s->pNext) {
        const byte         *v;
        const skelWeight_t *w;
        int                 i;
        int                 hits;

        if (!s->pVerts || s->numVerts <= 0) {
            continue;
        }
        hits = 0;
        v = (const byte *)s->pVerts;
        for (i = 0; i < s->numVerts; i++) {
            const skeletorVertex_t *vert = (const skeletorVertex_t *)v;
            const byte             *p = v + sizeof(skeletorVertex_t);
            int                     k;

            p += (size_t)vert->numMorphs * sizeof(skeletorMorph_t);
            w = (const skelWeight_t *)p;
            for (k = 0; k < vert->numWeights; k++) {
                if (w[k].boneIndex == bone) {
                    hits++;
                }
            }
            v = p + (size_t)vert->numWeights * sizeof(skelWeight_t);
        }
        if (hits) {
            Q_strcat(surf, sizeof(surf), va("%s'%s'=%dv", n ? " " : "", s->name, hits));
            n++;
        }
    }

    CHAN_PROBE(
        "head",
        "why=skin tiki=%s bone=%d name='%s' surf=\"%s\"\n",
        tiki->name,
        bone,
        boneName ? boneName : "?",
        surf
    );
}

/* Once per (tiki, single-bone screen-class surface): the bind offsets, the live
   world slot, the posed verts and the blend state, so a boot reads against the
   shipped bind data and against the offline pose. */
static void TIKI_Skan_AttachCensus(
    dtiki_t            *tiki,
    skelHeaderGame_t   *skel,
    const float        *world,
    const int          *legsAnim,
    int                 anim,
    int                 frame,
    int                 torsoAnim,
    int                 torsoFrame,
    const skelSolveChannel_t *ch)
{
    static int             done[TIKI_MAX_BONES];
    static const dtiki_t  *doneTiki;
    static int             rearm;
    skelSurfaceGame_t     *s;
    const skeletorVertex_t *v;
    const skeletorVertex_t *next;

    if (!PROBE_On()) {
        return;
    }
    if (doneTiki != tiki) {
        doneTiki = tiki;
        memset(done, 0, sizeof(done));
        rearm = 0;
    }
    if (rearm && (rearm % 600) != 0) {
        rearm++;
        return;
    }
    rearm++;

    for (s = skel->pSurfaces; s; s = s->pNext) {
        const skelWeight_t     *w;
        const float            *slot;
        float                   cx, cy, cz, px, py, pz;
        SkelMat3                m;
        int                     bone, multi, i, k;
        const char             *name;

        if (!s->pVerts || s->numVerts <= 0 || s->numVerts > 8) {
            continue;
        }
        bone  = -1;
        multi = 0;
        cx = cy = cz = 0.0f;
        v = s->pVerts;
        for (i = 0; i < s->numVerts; i++, v = next) {
            const byte *p = (const byte *)v;

            p += sizeof(skeletorVertex_t) + (size_t)v->numMorphs * sizeof(skeletorMorph_t);
            w = (const skelWeight_t *)p;
            next = (const skeletorVertex_t *)(p + (size_t)v->numWeights * sizeof(skelWeight_t));
            for (k = 0; k < v->numWeights; k++) {
                if (bone == -1) {
                    bone = w[k].boneIndex;
                } else if (bone != w[k].boneIndex) {
                    multi = 1;
                }
                cx += w[k].offset[0];
                cy += w[k].offset[1];
                cz += w[k].offset[2];
            }
        }
        if (multi || bone < 0 || bone >= skel->numBones || bone >= TIKI_MAX_BONES || done[bone]) {
            continue;
        }
        done[bone] = 1;
        slot = &world[bone * 7];
        name = skeletor_c::m_boneNames.FindName(skel->pBones[bone].channel);
        SkelQuat                q(slot);
        q.GetMat3(m);
        PROBE_EMIT(
            "[SKEL:",
            "attach",
            "tiki=%s skel='%s' surf='%s' verts=%d bone=%d name='%s' bind=(%.2f,%.2f,%.2f) "
            "bonepos=(%.3f,%.3f,%.3f) bonequat=(%.4f,%.4f,%.4f,%.4f) qlen=%.5f "
            "mat=(%.4f %.4f %.4f|%.4f %.4f %.4f|%.4f %.4f %.4f) "
            "anim=%d/%d frame=%d torso=%d/%d mode=%d innerT=%.3f outerT=%.3f ent=%d entorg=(%.2f,%.2f,%.2f)\n",
            tiki->name ? tiki->name : "?",
            skel->name ? skel->name : "?",
            s->name ? s->name : "?",
            s->numVerts,
            bone,
            name ? name : "?",
            cx / s->numVerts,
            cy / s->numVerts,
            cz / s->numVerts,
            slot[4],
            slot[5],
            slot[6],
            slot[0],
            slot[1],
            slot[2],
            slot[3],
            sqrt(slot[0] * slot[0] + slot[1] * slot[1] + slot[2] * slot[2] + slot[3] * slot[3]),
            m.val[0][0], m.val[0][1], m.val[0][2],
            m.val[1][0], m.val[1][1], m.val[1][2],
            m.val[2][0], m.val[2][1], m.val[2][2],
            legsAnim ? *legsAnim : -1,
            anim,
            frame,
            torsoAnim,
            torsoFrame,
            ch ? ch->mode : -1,
            ch ? ch->innerT : 0.0f,
            ch ? ch->outerT : 0.0f,
            s_probeEnt,
            s_probeOrg[0],
            s_probeOrg[1],
            s_probeOrg[2]
        );

        /* the posed verts, in the order the surface lists them */
        px = py = pz = 0.0f;
        v = s->pVerts;
        for (i = 0; i < s->numVerts; i++, v = next) {
            const byte   *p = (const byte *)v;
            const float  *ps;

            p += sizeof(skeletorVertex_t) + (size_t)v->numMorphs * sizeof(skeletorMorph_t);
            w = (const skelWeight_t *)p;
            next = (const skeletorVertex_t *)(p + (size_t)v->numWeights * sizeof(skelWeight_t));
            px = py = pz = 0.0f;
            for (k = 0; k < v->numWeights; k++) {
                const float *wo = w[k].offset;

                ps = &world[w[k].boneIndex * 7];
                px += (wo[0] * m.val[0][0] + wo[1] * m.val[1][0] + wo[2] * m.val[2][0] + ps[4]) * w[k].boneWeight;
                py += (wo[0] * m.val[0][1] + wo[1] * m.val[1][1] + wo[2] * m.val[2][1] + ps[5]) * w[k].boneWeight;
                pz += (wo[0] * m.val[0][2] + wo[1] * m.val[1][2] + wo[2] * m.val[2][2] + ps[6]) * w[k].boneWeight;
            }
            PROBE_EMIT(
                "[SKEL:",
                "attachvert",
                "tiki=%s surf='%s' v%d=(%.3f,%.3f,%.3f) anim=%d frame=%d\n",
                tiki->name ? tiki->name : "?",
                s->name ? s->name : "?",
                i,
                px,
                py,
                pz,
                anim,
                frame
            );
        }
    }
}

/* Names the head bone's sampled row, and each controller slot's bone once. */
static void TIKI_Skan_HeadCantProbe(
    dtiki_t                *tiki,
    skelHeaderGame_t       *skel,
    int                     bone,
    const char             *boneName,
    const int              *mapping,
    skelAnimHeader_t       *hdr,
    int                     slot,
    int                     layout,
    const float            *cq,
    const float            *w)
{
    static int  slotRows;
    const char *chanName;
    const char *lay;
    int         headchan;
    int         mapped;
    int         j;
    vec3_t      head;

    if (!PROBE_On()) {
        return;
    }

    lay = (layout == 0) ? "player" : (layout == 1) ? "actor" : "none";

    if (!boneName || (stricmp(boneName, "Bip01 Head") && stricmp(boneName, "Bip01 Neck"))) {
        if (slotRows < 24) {
            slotRows++;
            CHAN_PROBE(
                "head",
                "why=slot tiki=%s slot=%d bone=%d name='%s' layout=%s bones=%d\n",
                tiki->name,
                slot,
                bone,
                boneName ? boneName : "?",
                lay,
                skel->numBones
            );
        }
        return;
    }

    /* A bone absent from the anim maps to -1 by design and takes its base row;
       -1 with the index out of the channel table is the only lost-map shape. */
    if (!mapping && hdr && bone >= hdr->numbones) {
        QuatToAngles(w, head);
        CHAN_PROBE(
            "head",
            "why=nomap tiki=%s bone=%d head='%s' slot=%d layout=%s bones=%d chans=%d "
            "ctl=(%.1f,%.1f,%.1f) headang=(%.1f,%.1f,%.1f)\n",
            tiki->name,
            bone,
            boneName,
            slot,
            lay,
            skel->numBones,
            hdr->numbones,
            cq[0],
            cq[1],
            cq[2],
            head[0],
            head[1],
            head[2]
        );
        return;
    }

    mapped   = TIKI_Skan_ChannelForBone(mapping, hdr, bone);
    chanName = NULL;
    if (mapped >= 0 && hdr) {
        chanName = (const char *)hdr + hdr->ofsChannelNames + 36 * mapped + 4;
    }

    if (mapped >= 0 && (!chanName || stricmp(chanName, boneName))) {
        headchan = -1;
        if (hdr) {
            for (j = 0; j < hdr->numbones; j++) {
                if (!stricmp(boneName, (const char *)hdr + hdr->ofsChannelNames + 36 * j + 4)) {
                    headchan = j;
                    break;
                }
            }
        }
        CHAN_PROBE(
            "head",
            "why=chan tiki=%s bone=%d head='%s' chan=%d chan='%s' headchan=%d slot=%d layout=%s "
            "bones=%d chans=%d\n",
            tiki->name,
            bone,
            boneName,
            mapped,
            chanName ? chanName : "?",
            headchan,
            slot,
            lay,
            skel->numBones,
            hdr ? hdr->numbones : -1
        );
    }
}

const short int *TIKI_Skan_RecordRow(
    skelAnimHeader_t *hdr,
    skelFrame_t      *frameRec,
    const int        *mapping,
    int               bone,
    float             baseLen,
    const char       *boneName,
    const short int  *bindRow,
    float            *outRatio
)
{
    const short int *row = NULL;
    float            ratio = 1.0f;
    int              mapped;

    mapped = TIKI_Skan_ChannelForBone(mapping, hdr, bone);

    if (mapped != -1 && hdr) {
        float chanLen = *(const float *)((const byte *)hdr + hdr->ofsChannelNames + 36 * mapped);

        if (chanLen != 0.0f && chanLen != baseLen) {
            ratio = baseLen / chanLen;
        }
    }

    if (boneName && !strnicmp(boneName, "tag", 3)) {
        ratio = 1.0f;
    }

    if (mapped != -1 && frameRec) {
        row = (const short int *)((const byte *)frameRec + 40 + 16 * mapped);
    }
    if (!row) {
        row = bindRow;
    }

    *outRatio = ratio;
    return row;
}


static qboolean TIKI_Skan_SetupChannel(
    dtiki_t            *tiki,
    int                 anim,
    int                 frame,
    int                 oldanim,
    int                 oldframe,
    float               framelerp,
    int                 cbAnim,
    int                 cbFrame,
    float               cbLerp,
    qboolean            resolveCb,
    skelSolveChannel_t *ch
)
{
    memset(ch, 0, sizeof(*ch));

    if (!TIKI_ValidateSkelAnimAndFrame(tiki, anim, frame, &ch->mainHdr, &ch->mainRec)) {
        return qfalse;
    }
    ch->mainMap = TIKI_Skan_AnimBoneMapping(tiki, anim);

    TIKI_ValidateSkelAnimAndFrame(tiki, oldanim, oldframe, &ch->aHdr, &ch->aRec);
    ch->aMap = TIKI_Skan_AnimBoneMapping(tiki, oldanim);

    if (!ch->aRec || ch->aRec == ch->mainRec) {
        framelerp = 0.0f;
    }

    if (resolveCb) {
        TIKI_ValidateSkelAnimAndFrame(tiki, cbAnim, cbFrame, &ch->crossHdr, &ch->crossRec);
        ch->crossMap = TIKI_Skan_AnimBoneMapping(tiki, cbAnim);
    }

    if (cbLerp < 0.0f) {
        if (framelerp == 0.0f) {
            ch->mode = 1;
        } else {
            ch->mode   = 2;
            ch->innerT = 1.0f - framelerp;
        }
        return qtrue;
    }

    if (!ch->crossRec) {
        if (framelerp == 0.0f) {
            ch->mode = 1;
        } else {
            ch->mode   = 2;
            ch->innerT = 1.0f - framelerp;
        }
        return qtrue;
    }

    if (framelerp == 0.0f) {
        ch->mode   = 2;
        ch->aHdr   = ch->crossHdr;
        ch->aRec   = ch->crossRec;
        ch->aMap   = ch->crossMap;
        ch->innerT = cbLerp;
    } else {
        ch->mode   = 3;
        ch->innerT = 1.0f - framelerp;
        ch->outerT = cbLerp;
    }

    return qtrue;
}

static void TIKI_Skan_SolveChannelBone(
    skelHeaderGame_t         *skel,
    int                       bone,
    float                     baseLen,
    const char               *boneName,
    const skelSolveChannel_t *ch,
    float                    *out_quat,
    float                    *out_offset,
    int                      *out_chan,
    const short int         **out_row
)
{
    const short int *bindRow = skel->pBones[bone].hasBaseRow ? skel->pBones[bone].baseRow : NULL;
    const short int *row;
    float            qA[4];
    float            oA[3];
    float            qB[4];
    float            oB[3];
    float            qC[4];
    float            oC[3];
    float            ratio;
    int              i;

    row = TIKI_Skan_RecordRow(
        ch->mainHdr, ch->mainRec, ch->mainMap, bone, baseLen, boneName, bindRow, &ratio
    );
    if (out_chan) {
        *out_chan = TIKI_Skan_ChannelForBone(ch->mainMap, ch->mainHdr, bone);
    }
    if (out_row) {
        *out_row = row;
    }

    if (!row) {
        out_quat[0] = 0.0f;
        out_quat[1] = 0.0f;
        out_quat[2] = 0.0f;
        out_quat[3] = 1.0f;
        VectorCopy(skel->pBones[bone].offset, out_offset);
        return;
    }

    TIKI_Skan_DequantRow(row, qB, oB);
    for (i = 0; i < 3; i++) {
        oB[i] *= ratio;
    }

    if (ch->mode != 1) {
        row = TIKI_Skan_RecordRow(
            ch->aHdr, ch->aRec, ch->aMap, bone, baseLen, boneName, bindRow, &ratio
        );

        if (!row) {
            qA[0] = 0.0f;
            qA[1] = 0.0f;
            qA[2] = 0.0f;
            qA[3] = 1.0f;
            VectorCopy(skel->pBones[bone].offset, oA);
        } else {
            TIKI_Skan_DequantRow(row, qA, oA);
            for (i = 0; i < 3; i++) {
                oA[i] *= ratio;
            }
        }

        SlerpQuaternion(qA, qB, ch->innerT, qB);

        for (i = 0; i < 3; i++) {
            oB[i] = oA[i] * (1.0f - ch->innerT) + oB[i] * ch->innerT;
        }
    }

    if (ch->mode == 3) {
        row = TIKI_Skan_RecordRow(
            ch->crossHdr, ch->crossRec, ch->crossMap, bone, baseLen, boneName, bindRow, &ratio
        );

        if (!row) {
            qC[0] = 0.0f;
            qC[1] = 0.0f;
            qC[2] = 0.0f;
            qC[3] = 1.0f;
            VectorCopy(skel->pBones[bone].offset, oC);
        } else {
            TIKI_Skan_DequantRow(row, qC, oC);
            for (i = 0; i < 3; i++) {
                oC[i] *= ratio;
            }
        }

        SlerpQuaternion(qC, qB, ch->outerT, out_quat);

        for (i = 0; i < 3; i++) {
            out_offset[i] = oC[i] * (1.0f - ch->outerT) + oB[i] * ch->outerT;
        }
    } else {
        memcpy(out_quat, qB, sizeof(qB));
        VectorCopy(oB, out_offset);
    }
}

int TIKI_SkelBonesFromRecords(
    dtiki_t         *tiki,
    int              anim,
    int              frame,
    int              oldanim,
    int              oldframe,
    float            framelerp,
    int              crossblend_anim,
    int              crossblend_frame,
    float            crossblend_lerp,
    int              torso_anim,
    int              torso_frame,
    int              torso_oldanim,
    int              torso_oldframe,
    float            torso_framelerp,
    int              torso_crossblend_anim,
    int              torso_crossblend_frame,
    float            torso_crossblend_lerp,
    qboolean         uselegs,
    qboolean         usetorso,
    const int       *bone_tag,
    const vec4_t    *bone_quat,
    const vec3_t     localOrigin,
    skelBoneCache_t *bones,
    int              maxBones,
    float           *radius,
    vec3_t          *mins,
    vec3_t          *maxs
)
{
    skelHeaderGame_t   *skel;
    skelSolveChannel_t  legsCh;
    skelSolveChannel_t  torsoCh;
    skelSolveChannel_t *ch;
    const char         *boneName;
    float              *world;
    float               localQuat[4];
    float               localOffset[3];
    float               parentQuat[4];
    float               parentOffset[3];
    float               cq[4];
    float               folded[4];
    float               preFold[4];
    float               pw;
    qboolean            legs;
    int                 parent;
    int                 numBones;
    int                 headLayout;
    int                 b;
    int                 i;
    int                 n;

    skel = TIKI_GetSkel(tiki->skelIndex);
    if (!skel) {
        return 0;
    }

    if (!tiki->a) {
        return 0;
    }

    memset(&legsCh, 0, sizeof(legsCh));
    memset(&torsoCh, 0, sizeof(torsoCh));

    if (uselegs) {
        if (!TIKI_Skan_SetupChannel(
                tiki,
                anim,
                frame,
                oldanim,
                oldframe,
                framelerp,
                crossblend_anim,
                crossblend_frame,
                crossblend_lerp,
                (crossblend_lerp >= 0.0f) ? qtrue : qfalse,
                &legsCh
            )) {
            return 0;
        }
    }

    if (usetorso) {
        if (!TIKI_Skan_SetupChannel(
                tiki,
                torso_anim,
                torso_frame,
                torso_oldanim,
                torso_oldframe,
                torso_framelerp,
                torso_crossblend_anim,
                torso_crossblend_frame,
                torso_crossblend_lerp,
                qtrue,
                &torsoCh
            )) {
            return 0;
        }
    }

    if (!uselegs && !usetorso) {
        return 0;
    }

    ch = uselegs ? &legsCh : &torsoCh;

    if (radius && ch->mainRec) {
        *radius = ch->mainRec->radius;
    }
    if (mins && maxs && ch->mainRec) {
        VectorCopy(ch->mainRec->bounds[0], *mins);
        VectorCopy(ch->mainRec->bounds[1], *maxs);
    }

    numBones = skel->numBones;
    if (numBones > maxBones) {
        TIKI_Error("R_AddSkelSurfaces: too many skeleton models visible on '%s'\n", tiki->a->name);
        return 0;
    }

    world = (float *)Skel_Alloc(numBones * 7 * sizeof(float));
    if (!world) {
        return 0;
    }
    memset(world, 0, numBones * 7 * sizeof(float));

    headLayout = PROBE_On() ? TIKI_Skan_HeadLayout(skel, bone_tag) : -1;

    for (b = 0; b < numBones; b++) {
        float           *w = &world[b * 7];
        const short int *row = NULL;
        int              chan = -1;
        int              b_anim;
        int              b_frame;
        int              b_oldanim;
        int              b_oldframe;
        float            b_flerp;
        float            b_cb;

        boneName = skeletor_c::m_boneNames.FindName(skel->pBones[b].channel);

        if (uselegs && usetorso) {
            legs = (skel->pBones[b].ef2BodyPart == 1);
        } else if (uselegs) {
            legs = qtrue;
        } else {
            legs = qfalse;
        }

        ch = legs ? &legsCh : &torsoCh;

        TIKI_Skan_SolveChannelBone(
            skel,
            b,
            VectorLength(skel->pBones[b].offset),
            boneName,
            ch,
            localQuat,
            localOffset,
            &chan,
            &row
        );

        parent = skel->pBones[b].skelParent;

        if (parent < 0 || parent >= numBones || parent == b) {
            memcpy(w, localQuat, 4 * sizeof(float));
            w[4] = localOffset[0];
            w[5] = localOffset[1];
            w[6] = localOffset[2];
            parentQuat[0] = 0.0f;
            parentQuat[1] = 0.0f;
            parentQuat[2] = 0.0f;
            parentQuat[3] = 0.0f;
        } else {
            pw = localQuat[3];
            for (i = 0; i < 4; i++) {
                parentQuat[i] = world[parent * 7 + i];
            }
            parentOffset[0] = world[parent * 7 + 4];
            parentOffset[1] = world[parent * 7 + 5];
            parentOffset[2] = world[parent * 7 + 6];

            TIKI_Skan_QuatMul(localQuat, parentQuat, w);

            {
                float px = localOffset[0];
                float py = localOffset[1];
                float pz = localOffset[2];
                SkelQuat  q;
                SkelMat3  m;
                q.Set(parentQuat);
                q.GetMat3(m);
                w[4] = px * m.val[0][0] + py * m.val[1][0] + pz * m.val[2][0] + parentOffset[0];
                w[5] = px * m.val[0][1] + py * m.val[1][1] + pz * m.val[2][1] + parentOffset[1];
                w[6] = px * m.val[0][2] + py * m.val[1][2] + pz * m.val[2][2] + parentOffset[2];
            }
        }

        b_anim = legs ? anim : torso_anim;
        b_frame = legs ? frame : torso_frame;
        b_oldanim = legs ? oldanim : torso_oldanim;
        b_oldframe = legs ? oldframe : torso_oldframe;
        b_flerp = legs ? framelerp : torso_framelerp;
        b_cb = legs ? crossblend_lerp : torso_crossblend_lerp;

        /* the rotation arm samples the whole upper chain and the length arm covers
           every bone; both run before the slot loop so a bone no slot names is not
           blind, and the length rows stay anomaly-gated so the stream is bounded */
        TIKI_Skan_QuatLenProbe(
            tiki, skel, b, boneName, -1, skanIdentityQuat, w, ch, chan, row, localQuat, parent,
            parentQuat, legs, b_anim, b_frame, b_oldanim, b_oldframe, b_flerp, b_cb
        );
        TIKI_Skan_RotProbe(tiki, b, boneName, -1, w, w, legs, b_anim, b_frame, b_oldanim, b_oldframe, b_flerp);

        for (n = NUM_BONE_CONTROLLERS - 1; n >= 0; n--) {
            if (!bone_tag || !bone_quat || bone_tag[n] != b) {
                if (!bone_tag || !bone_quat) {
                    break;
                }
                continue;
            }
            cq[0] = bone_quat[n][0];
            cq[1] = bone_quat[n][1];
            cq[2] = bone_quat[n][2];
            cq[3] = bone_quat[n][3];
            TIKI_Skan_QuatLenProbe(
                tiki, skel, b, boneName, n, cq, w, ch, chan, row, localQuat, parent, parentQuat,
                legs, b_anim, b_frame, b_oldanim, b_oldframe, b_flerp, b_cb
            );
            /* never in place: r[0] lands before r[1..3] read q[0]; retail reads temps */
            Vector4Copy(w, preFold);
            TIKI_Skan_QuatMul(w, cq, folded);
            Vector4Copy(folded, w);
            if (!(w[0] * w[0] + w[1] * w[1] + w[2] * w[2] + w[3] * w[3] >= 0.998f
                  && w[0] * w[0] + w[1] * w[1] + w[2] * w[2] + w[3] * w[3] <= 1.002f)) {
                TIKI_Skan_ChainProbe(tiki, skel, world, b, n);
                TIKI_Skan_FoldProbe(tiki, skel, b, boneName, n, preFold, w, cq);
            }
            /* once per folded bone: the row that names the geometry it carries */
            TIKI_Skan_SkinProbe(tiki, skel, b, boneName);
            TIKI_Skan_HeadCantProbe(
                tiki, skel, b, boneName, ch->mainMap, ch->mainHdr, n, headLayout, cq, w
            );
            break;
        }

        if (parent < 0 || parent >= numBones || parent == b) {
            for (i = 0; i < 3; i++) {
                w[4 + i] += localOrigin[i];
            }
        }
    }

    {
        /* the split the whole solve hangs on: every bone takes legs or torso by
           ef2BodyPart alone, so a degenerate count while both channels are live is
           one channel painted over the whole skeleton -- the flattened sprawl */
        int legsBones = 0;
        int torsoBones = 0;

        for (b = 0; b < numBones; b++) {
            if (skel->pBones[b].ef2BodyPart == 1) {
                legsBones++;
            } else {
                torsoBones++;
            }
        }

        if (uselegs && usetorso && (legsBones == 0 || torsoBones == 0)) {
            static int s_splitRows;
            if (PROBE_Throttle(&s_splitRows, 8, 600)) {
                PROBE_CENSUS_EMIT(
                    "",
                    "SKEL:split",
                    "model='%s' bones=%d legs=%d torso=%d legsbp1=%d split=%s",
                    tiki->a ? tiki->a->name : "?",
                    numBones,
                    legsBones,
                    torsoBones,
                    (int)skel->pBones[0].ef2BodyPart,
                    legsBones == 0 ? "all-torso" : "all-legs"
                );
            }
        }
    }

    for (b = 0; b < numBones; b++) {
        float       *w = &world[b * 7];
        SkelQuat     q;
        SkelMat3     m;
        skelBoneCache_t *out = &bones[b];
        int          r;
        int          c;

        q.Set(w);
        q.GetMat3(m);

        out->offset[0] = w[4];
        out->offset[1] = w[5];
        out->offset[2] = w[6];
        out->offset[3] = 0.0f;

        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) {
                out->matrix[r][c] = m.val[r][c];
            }
            out->matrix[r][3] = 0.0f;
        }
    }

    TIKI_Skan_AttachCensus(
        tiki,
        skel,
        world,
        uselegs ? &anim : &torso_anim,
        anim,
        frame,
        torso_anim,
        torso_frame,
        uselegs ? &legsCh : &torsoCh
    );

    Skel_Free(world);
    (void)pw;
    return numBones;
}

skelMorphHeader_t *TIKI_BuildMorphHeader(const void *morphDataVoid, size_t *outSize)
{
    const skmMorphData_t *morphData = (const skmMorphData_t *)morphDataVoid;

    skelMorphHeader_t       *hdr;
    skelMorphTargetRecord_t *targets;
    int                     *remap;
    size_t                   size;
    size_t                   off;
    int                      i;
    int                      k;

    if (!morphData || morphData->numTargets <= 0 || !morphData->targets) {
        return NULL;
    }

    size = sizeof(skelMorphHeader_t);
    off  = size;

    {
        size_t remapBytes = (size_t)morphData->numMorphVertices * sizeof(int);
        size += remapBytes;
        (void)remapBytes;
    }
    size += (size_t)morphData->numTargets * sizeof(skelMorphTargetRecord_t);
    for (i = 0; i < morphData->numTargets; i++) {
        size += (size_t)morphData->targets[i].numVerts * sizeof(int);
    }

    if (size > (size_t)INT_MAX) {
        return NULL;
    }

    hdr = (skelMorphHeader_t *)Skel_Alloc(size);
    if (!hdr) {
        return NULL;
    }
    memset(hdr, 0, size);

    hdr->numMorphVertices = morphData->numMorphVertices;
    hdr->numTargets       = morphData->numTargets;

    off = sizeof(skelMorphHeader_t);
    hdr->ofsVertexRemap = (int)off;
    off += (size_t)morphData->numMorphVertices * sizeof(int);
    hdr->ofsTargets = (int)off;

    remap = (int *)((byte *)hdr + hdr->ofsVertexRemap);
    for (i = 0; i < morphData->numMorphVertices; i++) {
        remap[i] = -1;
    }

    targets = (skelMorphTargetRecord_t *)((byte *)hdr + hdr->ofsTargets);
    for (i = 0; i < morphData->numTargets; i++) {
        const skmMorphTarget_t *src = &morphData->targets[i];
        int                    *dst;
        int                     listOff;

        Q_strncpyz(targets[i].name, src->name, sizeof(targets[i].name));
        targets[i].numVerts = src->numVerts;

        listOff = (int)off;
        off += (size_t)src->numVerts * sizeof(int);
        targets[i].ofsIndices = listOff;

        dst = (int *)((byte *)hdr + listOff);
        for (k = 0; k < src->numVerts; k++) {
            dst[k] = src->verts[k].index;
            if (dst[k] >= 0 && dst[k] < morphData->numMorphVertices) {
                remap[dst[k]] = k;
            }
        }
    }

    if (outSize) {
        *outSize = size;
    }
    return hdr;
}
