
#include "g_local.h"
#include "../qcommon/gameplaymanager.h"  
#include "g_utils.h"
#include "ctype.h"
#include "worldspawn.h"
#include "scriptmaster.h"
#include "../corepp/script.h"
#include "scriptthread.h"
#include "player.h"
#include "playerstart.h"
#include "trigger.h"
#include "debuglines.h"
#include "../corepp/tiki.h"

Vector vec_zero( 0, 0, 0 );

Vector &Vector::Identity(void)
{
    return vec_zero;
}

float G_CRandom(void)
{
    return G_Random(2) - 1;
}

float G_CRandom(float n)
{
    return G_CRandom() * n;
}

float G_Random(void)
{
    return ((float)(rand() & 0x7fff)) / ((float)0x8000);
}


float Vector::Dot(const Vector &vector1, const Vector &vector2)
{
    return vector1 * vector2;
}

float Vector::length(void) const
{
    return sqrt(lengthSquared());
}

float Vector::DistanceXY(const Vector &vector1, const Vector &vector2)
{
    return (vector1 - vector2).lengthXY();
}

bool Vector::CloseEnough(const Vector &vector1, const Vector &vector2, const float epsilon)
{
    return Distance(vector1, vector2) < epsilon;
}



const Vector &Vector::operator=(const Vector &a)
{
    x = a.x;
    y = a.y;
    z = a.z;

    return *this;
}

const Vector &Vector::operator+=(const Vector &a)
{
    x += a.x;
    y += a.y;
    z += a.z;

    return *this;
}

float Vector::operator[](const int index) const
{
    assert((index >= 0) && (index < 3));
    return (&x)[index];
}

float &Vector::operator[](const int index)
{
    assert((index >= 0) && (index < 3));
    return (&x)[index];
}


const char *means_of_death_strings[MOD_TOTAL_NUMBER] = {
    "none",
    "drown",
    "suicide",
    "crush",
    "crush_every_frame",
    "telefrag",
    "lava",
    "slime",
    "falling",
    "last_self_inflicted",
    "explosion",
    "explodewall",
    "electric",
    "electricwater",
    "thrownobject",
    "beam",
    "rocket",
    "impact",
    "gas",
    "gas_blockable",
    "acid",
    "sword",
    "plasma",
    "plasmabeam",
    "plasmashotgun",
    "sting",
    "sting2",
    "sling",
    "bullet",
    "fast_bullet",
    "vehicle",
    "fire",
    "fire_blockable",
    "vortex",
    "lifedrain",
    "flashbang",
    "poo_explosion",
    "axe",
    "chainsword",
    "on_fire",
    "firesword",
    "electricsword",
    "circleofprotection",
    "radiation",
    "lightsword",
    "gib",
    "impale",
    "uppercut",
    "poison",
    "eat",
    "redemption",
    "stasis",
    "phaser",
    "vaporize",
    "comp_rifle",
    "vaporize_comp",
    "imod_primary",
    "imod_secondary",
    "small_explosion",
    "tetryon",
    "disruptor",
    "vaporize_disruptor",
    "vaporize_photon",
    "sniper",
    "melee",
    "alien_melee",
    "klingon_melee",
    "turret",
    "deathQuad",
    "empathyShield",
    "armorPiercing"
};


qboolean MOD_matches(int incoming_damage, int damage_type)
{
    if (damage_type == -1) {
        return true;
    }

    if (damage_type == MOD_SWORD) {
        if ((incoming_damage == MOD_SWORD) || (incoming_damage == MOD_ELECTRICSWORD)
            || (incoming_damage == MOD_LIGHTSWORD) || (incoming_damage == MOD_FIRESWORD)) {
            return true;
        }
    } else if (damage_type == incoming_damage) {
        return true;
    }

    return false;
}

int MOD_NameToNum(const str &meansOfDeath)
{
    int i;

    for (i = 0; i < MOD_TOTAL_NUMBER; i++) {
        if (!meansOfDeath.icmp(means_of_death_strings[i])) {
            return i;
        }
    }

    gi.WDPrintf("Unknown means of death - %s\n", meansOfDeath.c_str());
    return -1;
}

const char *MOD_NumToName(int meansOfDeath)
{
    if ((meansOfDeath > MOD_TOTAL_NUMBER) || (meansOfDeath < 0)) {
        gi.WDPrintf("Unknown means of death num - %d\n", meansOfDeath);
        return "";
    }

    return means_of_death_strings[meansOfDeath];
}

void G_TouchTriggers(Entity *ent)
{
    int        i;
    int        num;
    int        touch[MAX_GENTITIES];
    gentity_t *hit;
    Event     *ev;

    if ((ent->client || (ent->edict->svflags & SVF_MONSTER)) && (ent->health <= 0.0f)) {
        return;
    }

    num = gi.AreaEntities(ent->absmin, ent->absmax, touch, MAX_GENTITIES, qfalse);

    Container<EntityPtr> &lastTouchedList = ent->GetLastTouchedList();
    Container<EntityPtr>  newTouchedList;
    Container<EntityPtr>  newLastTouchedList;
    int                   numTouchedEntities = lastTouchedList.NumObjects();
    int                   touchedEntityIdx = 1;

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (!hit->inuse || (hit->entity == ent) || (hit->solid != SOLID_TRIGGER)) {
            continue;
        }

        assert(hit->entity);
        newTouchedList.AddObject(hit->entity);
    }

    for (touchedEntityIdx = 1; touchedEntityIdx <= numTouchedEntities; ++touchedEntityIdx) {
        Entity *touchedEntity = lastTouchedList.ObjectAt(touchedEntityIdx);
        if (!touchedEntity) {
            continue;
        }
        if (!newTouchedList.ObjectInList(touchedEntity)) {
            ev = new Event(EV_LostContact); 
            ev->AddEntity(ent);
            touchedEntity->ProcessEvent(ev);
        } else {
            ev = new Event(EV_Touch);
            ev->AddEntity(ent);
            touchedEntity->ProcessEvent(ev);
            newTouchedList.RemoveObject(touchedEntity);
            newLastTouchedList.AddObject(touchedEntity);
        }
    }

    lastTouchedList.ClearObjectList();
    numTouchedEntities = newTouchedList.NumObjects();
    for (touchedEntityIdx = 1; touchedEntityIdx <= numTouchedEntities; ++touchedEntityIdx) {
        Entity *touchedEntity = newTouchedList.ObjectAt(touchedEntityIdx);
        lastTouchedList.AddObject(touchedEntity);

        ev = new Event(EV_Touch);
        ev->AddEntity(ent);
        touchedEntity->ProcessEvent(ev);

        ev = new Event(EV_Contact);
        ev->AddEntity(ent);
        touchedEntity->ProcessEvent(ev);
    }

    numTouchedEntities = newLastTouchedList.NumObjects();
    for (touchedEntityIdx = 1; touchedEntityIdx <= numTouchedEntities; ++touchedEntityIdx) {
        lastTouchedList.AddObject(newLastTouchedList.ObjectAt(touchedEntityIdx));
    }
}

void G_TouchSolids(Entity *ent)
{
    int        i;
    int        num;
    int        touch[MAX_GENTITIES];
    gentity_t *hit;
    Event     *ev;

    num = gi.AreaEntities(ent->absmin, ent->absmax, touch, MAX_GENTITIES, qfalse);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (!hit->inuse) {
            continue;
        }

        assert(hit->entity);

        ev = new Event(EV_Touch);
        ev->AddEntity(ent);
        hit->entity->ProcessEvent(ev);
    }
}

void G_ShowTrace(const trace_t *trace, const gentity_t *passent, const char *reason, Vector& start, Vector& end)
{
    str    text;
    str    pass;
    str    hit;
    Vector delta;
    float  dist;

    assert(reason);
    assert(trace);

    if (passent) {
        pass = va("'%s'(%d)", passent->entname, passent->s.number);
    } else {
        pass = "NULL";
    }

    if (trace->ent) {
        hit = va("'%s'(%d)", trace->ent->entname, trace->ent->s.number);
    } else {
        hit = "NULL";
    }

    delta = end - start;
    dist  = delta.length();

    text =
        va("%0.2f : Pass %s Frac %f Hit %s len %f: '%s'\n",
           level.time,
           pass.c_str(),
           trace->fraction,
           hit.c_str(),
           dist,
           reason ? reason : "");

    if (sv_traceinfo->integer == 3) {
        gi.DebugPrintf("%s", text.c_str());
    } else {
        gi.DPrintf("%s", text.c_str());
    }
}


void G_CalcBoundsOfMove(Vector& start, Vector& end, Vector& mins, Vector& maxs, Vector *minbounds, Vector *maxbounds)
{
    Vector bmin;
    Vector bmax;

    ClearBounds(bmin, bmax);
    AddPointToBounds(start, bmin, bmax);
    AddPointToBounds(end, bmin, bmax);
    bmin += mins;
    bmax += maxs;

    if (minbounds) {
        *minbounds = bmin;
    }

    if (maxbounds) {
        *maxbounds = bmax;
    }
}


trace_t G_FullTrace(
    vec3_t     start,
    vec3_t     mins,
    vec3_t     maxs,
    vec3_t     end,
    gentity_t *passent,
    int        contentmask,
    qboolean   cylinder,
    const char *reason
)
{
    return G_Trace(start, mins, maxs, end, passent, contentmask, cylinder, reason, qtrue);
}

trace_t G_FullTrace(
    const Vector& start,
    const Vector& mins,
    const Vector& maxs,
    const Vector& end,
    Entity       *passent,
    int           contentmask,
    qboolean      cylinder,
    const char   *reason
)
{
    return G_Trace(start, mins, maxs, end, passent, contentmask, cylinder, reason, qtrue);
}

trace_t G_Trace(
    vec3_t           start,
    vec3_t           mins,
    vec3_t           maxs,
    vec3_t           end,
    const gentity_t *passent,
    int              contentmask,
    qboolean         cylinder,
    const char      *reason,
    qboolean         fulltrace
)
{
    int     entnum;
    trace_t trace;

    if (passent) {
        entnum = passent->s.number;
    } else {
        entnum = ENTITYNUM_NONE;
    }

    if (fulltrace) {
        gi.fulltrace(&trace, start, mins, maxs, end, entnum, contentmask, cylinder);
    } else {
        gi.trace(&trace, start, mins, maxs, end, entnum, contentmask, cylinder);
    }

    if (trace.entityNum == ENTITYNUM_NONE) {
        trace.ent = NULL;
    } else {
        trace.ent = &g_entities[trace.entityNum];
    }

    if (sv_traceinfo->integer > 1) {
        Vector start_vector = start;
        Vector end_vector   = end;

        G_ShowTrace(&trace, passent, reason, start_vector, end_vector);
    }
    sv_numtraces++;

    if (sv_drawtrace->integer) {
        G_DebugLine(Vector(start), Vector(end), 1, 1, 0, 1);
    }

    return trace;
}

trace_t G_Trace(
    const Vector& start,
    const Vector& mins,
    const Vector& maxs,
    const Vector& end,
    const Entity *passent,
    int           contentmask,
    qboolean      cylinder,
    const char   *reason,
    qboolean      fulltrace
)
{
    gentity_t *ent;
    int        entnum;
    trace_t    trace;

    assert(reason);

    if (passent == NULL) {
        ent    = NULL;
        entnum = ENTITYNUM_NONE;
    } else {
        ent    = passent->edict;
        entnum = ent->s.number;
    }

    if (fulltrace) {
        gi.fulltrace(&trace, start, mins, maxs, end, entnum, contentmask, cylinder);
    } else {
        gi.trace(&trace, start, mins, maxs, end, entnum, contentmask, cylinder);
    }

    if (trace.entityNum == ENTITYNUM_NONE) {
        trace.ent = NULL;
    } else {
        trace.ent = &g_entities[trace.entityNum];
    }

    if (sv_traceinfo->integer > 1) {
        Vector start_vector = start;
        Vector end_vector   = end;

        G_ShowTrace(&trace, ent, reason, start_vector, end_vector);
    }

    sv_numtraces++;

    if (sv_drawtrace->integer) {
        G_DebugLine(start, end, 1, 1, 0, 1);
    }

    return trace;
}

void G_TraceEntities(
    Vector&              start,
    Vector&              mins,
    Vector&              maxs,
    Vector&              end,
    Container<Entity *> *victimlist,
    int                  contentmask,
    qboolean             bIncludeTriggers
)
{
    trace_t    trace;
    vec3_t     boxmins;
    vec3_t     boxmaxs;
    int        num;
    int        touchlist[MAX_GENTITIES];
    gentity_t *touch;
    int        i;


    for (i = 0; i < 3; i++) {
        if (end[i] > start[i]) {
            boxmins[i] = start[i] + mins[i] - 1;
            boxmaxs[i] = end[i] + maxs[i] + 1;
        } else {
            boxmins[i] = end[i] + mins[i] - 1;
            boxmaxs[i] = start[i] + maxs[i] + 1;
        }
    }


    num = gi.AreaEntities(boxmins, boxmaxs, touchlist, MAX_GENTITIES, qfalse);

    for (i = 0; i < num; i++) {
        touch = &g_entities[touchlist[i]];

        if (touch->solid == SOLID_NOT) {
            continue;
        }
        if (touch->solid == SOLID_TRIGGER && !bIncludeTriggers) {
            continue;
        }

        gi.ClipToEntity(&trace, start, mins, maxs, end, touchlist[i], contentmask);

        if (trace.entityNum == touchlist[i]) {
            victimlist->AddObject(touch->entity);
        }
    }
}


float PlayersRangeFromSpot(Entity *spot)
{
    Entity *player;
    float   bestplayerdistance;
    Vector  v;
    int     n;
    float   playerdistance;

    bestplayerdistance = 9999999;
    for (n = 0; n < maxclients->integer; n++) {
        if (!g_entities[n].inuse || !g_entities[n].entity) {
            continue;
        }

        player = g_entities[n].entity;
        if (player->health <= 0) {
            continue;
        }

        v              = spot->origin - player->origin;
        playerdistance = v.length();

        if (playerdistance < bestplayerdistance) {
            bestplayerdistance = playerdistance;
        }
    }

    return bestplayerdistance;
}

Entity *SelectRandomDeathmatchSpawnPoint(void)
{
    Entity *spot, *spot1, *spot2;
    int     count = 0;
    int     selection;
    float   range, range1, range2;

    spot   = NULL;
    range1 = range2 = 99999;
    spot1 = spot2 = NULL;

    for (spot = G_FindClass(spot, "info_player_deathmatch"); spot; spot = G_FindClass(spot, "info_player_deathmatch")) {
        count++;
        range = PlayersRangeFromSpot(spot);
        if (range < range1) {
            range1 = range;
            spot1  = spot;
        } else if (range < range2) {
            range2 = range;
            spot2  = spot;
        }
    }

    if (!count) {
        return NULL;
    }

    if (count <= 2) {
        spot1 = spot2 = NULL;
    } else {
        count -= 2;
    }

    selection = rand() % count;

    spot = NULL;
    do {
        spot = G_FindClass(spot, "info_player_deathmatch");

        if (!spot) {
            break;
        }

        if (spot == spot1 || spot == spot2) {
            selection++;
        }
    } while (selection--);

    return spot;
}

Entity *SelectFarthestDeathmatchSpawnPoint(void)
{
    Entity *bestspot;
    float   bestdistance;
    float   bestplayerdistance;
    Entity *spot;

    spot         = NULL;
    bestspot     = NULL;
    bestdistance = 0;
    for (spot = G_FindClass(spot, "info_player_deathmatch"); spot; spot = G_FindClass(spot, "info_player_deathmatch")) {
        bestplayerdistance = PlayersRangeFromSpot(spot);
        if (bestplayerdistance > bestdistance) {
            bestspot     = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if (bestspot) {
        return bestspot;
    }

    spot = G_FindClass(NULL, "info_player_deathmatch");

    return spot;
}


int c_yes, c_no;

qboolean M_CheckBottom(Entity *ent)
{
    Vector  mins, maxs, start, stop;
    trace_t trace;
    int     x, y;
    float   mid, bottom;

    mins = ent->origin + ent->mins * 0.5;
    maxs = ent->origin + ent->maxs * 0.5;

    start[2] = mins[2] - 1;

    for (x = 0; x <= 1; x++) {
        for (y = 0; y <= 1; y++) {
            start[0] = x ? maxs[0] : mins[0];
            start[1] = y ? maxs[1] : mins[1];
            if (gi.pointcontents(start, 0) != CONTENTS_SOLID) {
                goto realcheck;
            }
        }
    }

    c_yes++;
    return true; 

realcheck:

    c_no++;

    start[2] = mins[2];

    start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5;
    start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5;
    stop[2]            = start[2] - 3 * STEPSIZE; 

    trace = G_Trace(start, vec_zero, vec_zero, stop, ent, MASK_MONSTERSOLID, false, "M_CheckBottom 1");

    if (trace.fraction == 1.0) {
        return false;
    }

    mid = bottom = trace.endpos[2];

    for (x = 0; x <= 1; x++) {
        for (y = 0; y <= 1; y++) {
            start[0] = stop[0] = x ? maxs[0] : mins[0];
            start[1] = stop[1] = y ? maxs[1] : mins[1];

            trace = G_Trace(start, vec_zero, vec_zero, stop, ent, MASK_MONSTERSOLID, false, "M_CheckBottom 1");   

            if (trace.fraction != 1.0 && trace.endpos[2] > bottom) {
                bottom = trace.endpos[2];
            }

            if (trace.fraction == 1.0 || mid - trace.endpos[2] > STEPSIZE) {
                return false;
            }
        }
    }

    c_yes++;
    return true;
}

Entity *G_FindClass(Entity *ent, const char *classname)
{
    int        entnum;
    gentity_t *from;

    if (ent) {
        entnum = ent->entnum;
    } else {
        entnum = -1;
    }

    for (from = &g_entities[entnum + 1]; from < &g_entities[globals.num_entities]; from++) {
        if (!from->inuse) {
            continue;
        }
        if (!Q_stricmp(from->entity->getClassID(), classname)) {
            return from->entity;
        }
    }

    return NULL;
}

Entity *G_FindTarget(Entity *ent, const char *name)
{
    Entity *next;

    if (name && name[0]) {
        if (name[0] == '$') {
            name++;
        }

        next = world->GetNextEntity(str(name), ent);
        if (next) {
            return next;
        }
    }

    return NULL;
}



Entity *G_NextEntity(Entity *ent)
{
    gentity_t *from;

    if (!g_entities) {
        return NULL;
    }

    if (!ent) {
        from = g_entities;
    } else {
        from = ent->edict + 1;
    }

    if (!from) {
        return NULL;
    }

    for (; from < &g_entities[globals.num_entities]; from++) {
        if (!from->inuse || !from->entity) {
            continue;
        }

        return from->entity;
    }

    return NULL;
}

Vector G_GetMovedir(float angle)
{
    if (angle == -1) {
        return Vector(0, 0, 1);
    } else if (angle == -2) {
        return Vector(0, 0, -1);
    }

    angle *= (M_PI * 2 / 360);
    return Vector(cos(angle), sin(angle), 0);
}



qboolean KillBox(Entity *ent)
{
    int        i;
    int        num;
    int        touch[MAX_GENTITIES];
    gentity_t *hit;
    Vector     min;
    Vector     max;
    int        fail;

    fail = 0;

    min = ent->origin + ent->mins;
    max = ent->origin + ent->maxs;

    num = gi.AreaEntities(min, max, touch, MAX_GENTITIES, qfalse);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];

        if (!hit->inuse || (hit->entity == ent) || !hit->entity || (hit->entity == world)
            || (!hit->entity->edict->solid)) {
            continue;
        }

        hit->entity->Damage(
            ent,
            ent,
            hit->entity->health + 100000,
            ent->origin,
            vec_zero,
            vec_zero,
            0,
            DAMAGE_NO_PROTECTION,
            MOD_TELEFRAG
        );

        if ((hit->entity->getSolidType() != SOLID_NOT) && (hit->entity->health > 0.0f)) {
            fail++;
        }
    }

    return !fail;
}

qboolean IsNumeric(const char *str)
{
    size_t   len;
    int      i;
    qboolean dot;

    if (*str == '-') {
        str++;
    }

    dot = false;
    len = strlen(str);
    for (i = 0; i < (int)len; i++) {
        if (!isdigit(str[i])) {
            if ((str[i] == '.') && !dot) {
                dot = true;
                continue;
            }
            return false;
        }
    }

    return true;
}

Entity *findradius(Entity *startent, Vector org, float rad)
{
    Vector     eorg;
    gentity_t *from;
    float      r2, distance;

    if (!startent) {
        from = active_edicts.next;
    } else {
        from = startent->edict->next;
    }

    assert(from);
    if (!from) {
        return NULL;
    }

    assert((from == &active_edicts) || (from->inuse));

    r2 = rad * rad;

    for (; from != &active_edicts; from = from->next) {
        assert(from->inuse);
        assert(from->entity);

        eorg = org - from->entity->centroid;

        distance = eorg * eorg;

        if (distance <= r2) {
            return from->entity;
        } else {
            distance -= from->radius2;
            if (distance <= r2) {
                return from->entity;
            }
        }
    }

    return NULL;
}

Entity *findclientsinradius(Entity *startent, Vector org, float rad)
{
    Vector     eorg;
    gentity_t *ed;
    float      r2;
    int        i;

    r2 = rad * rad;

    if (!startent) {
        i = 0;
    } else {
        i = startent->entnum + 1;
    }

    for (; i < game.maxclients; i++) {
        ed = &g_entities[i];

        if (!ed->inuse || !ed->entity) {
            continue;
        }

        eorg = org - ed->entity->centroid;

        if ((eorg * eorg) <= r2) {
            return ed->entity;
        }
    }

    return NULL;
}

Vector G_CalculateImpulse(Vector start, Vector end, float speed, float gravity)
{
    float  traveltime, vertical_speed;
    Vector dir, xydir, velocity;

    dir            = end - start;
    xydir          = dir;
    xydir.z        = 0;
    traveltime     = xydir.length() / speed;
    vertical_speed = (dir.z / traveltime) + (0.5f * gravity * sv_currentGravity->value * traveltime);
    xydir.normalize();

    velocity   = speed * xydir;
    velocity.z = vertical_speed;
    return velocity;
}

Vector G_PredictPosition(Vector start, Vector target, Vector targetvelocity, float speed)
{
    Vector projected;
    float  traveltime;
    Vector dir, xydir;

    dir        = target - start;
    xydir      = dir;
    xydir.z    = 0;
    traveltime = xydir.length() / speed;
    projected  = target + (targetvelocity * traveltime);

    return projected;
}

#define EF2_MAX_STATS       96
#define EF2_MAX_ACTIVE_ITEMS 8
#define EF2_MAX_INVENTORY   32
#define EF2_MAX_AMMO        16
#define EF2_MAX_AMMOCOUNT   16
#define EF2_MAX_INFO_STRING 1024
#define EF2_MAX_NETNAME     36
#define EF2_MAX_QPATH       64

static_assert(EF2_MAX_STATS == MAX_STATS, "retail ps.stats is 96 entries");
static_assert(EF2_MAX_ACTIVE_ITEMS == MAX_ACTIVE_ITEMS, "retail activeItems is 8");
static_assert(EF2_MAX_INVENTORY == MAX_INVENTORY, "retail inventory arrays are 32");
static_assert(EF2_MAX_AMMO == MAX_AMMO, "retail ammo_name_index is 16");
static_assert(EF2_MAX_AMMOCOUNT == MAX_AMMOCOUNT, "retail ammo arrays are 16");

static_assert(sizeof(qboolean) == sizeof(int), "the ps casts in G_ArchiveClientRetail need a 4-byte qboolean");

static void EF2_ArchiveCharBlock(Archiver& arc, char *src, size_t srcSize, size_t retailSize)
{
    char *tmp = (char *)gi.Malloc(retailSize);

    memset(tmp, 0, retailSize);
    if (arc.Saving() && src) {
        size_t n = (srcSize < retailSize) ? srcSize : retailSize;
        memcpy(tmp, src, n);
        tmp[retailSize - 1] = 0;
    }

    arc.ArchiveRaw(tmp, retailSize);

    if (arc.Loading() && src) {
        size_t n = (srcSize < retailSize) ? srcSize : retailSize;
        memcpy(src, tmp, n);
        src[srcSize - 1] = 0;
    }

    gi.Free(tmp);
}

static void EF2_ArchiveTraceImage(Archiver& arc, trace_t *tr)
{
    int zero = 0;

    arc.ArchiveInteger((int *)&tr->allsolid);
    arc.ArchiveInteger((int *)&tr->startsolid);
    arc.ArchiveFloat(&tr->fraction);
    arc.ArchiveVec3(tr->endpos);

    arc.ArchiveVec3(tr->plane.normal);
    arc.ArchiveFloat(&tr->plane.dist);
    arc.ArchiveByte(&tr->plane.type);
    arc.ArchiveByte(&tr->plane.signbits);
    arc.ArchiveByte(&tr->plane.pad[0]);
    arc.ArchiveByte(&tr->plane.pad[1]);

    arc.ArchiveInteger(&tr->surfaceFlags);
    arc.ArchiveInteger(&tr->contents);
    arc.ArchiveInteger(&tr->entityNum);

    // retail's `struct gentity_s *ent` -- a 32-bit pointer, never meaningful on reload
    arc.ArchiveInteger(&zero);

    arc.ArchiveInteger((int *)&tr->intersect.valid);
    arc.ArchiveInteger(&tr->intersect.surface);
    arc.ArchiveInteger(&tr->intersect.bone);
    arc.ArchiveVec3(tr->intersect.position);
    arc.ArchiveVec3(tr->intersect.normal);
    arc.ArchiveFloat(&tr->intersect.damage_multiplier);
}

void G_ArchiveClientRetail(Archiver& arc, gclient_t *client)
{
    playerState_t *ps    = &client->ps;
    int            zero  = 0;
    int            i;

    arc.ArchiveInteger(&ps->commandTime);
    arc.ArchiveInteger(&ps->pm_type);
    arc.ArchiveInteger(&ps->bobCycle);
    arc.ArchiveInteger(&ps->pm_flags);
    arc.ArchiveInteger(&ps->pm_time);
    arc.ArchiveInteger(&ps->pm_runtime);

    arc.ArchiveVec3(ps->origin);
    arc.ArchiveVec3(ps->velocity);

    arc.ArchiveInteger(&ps->gravity);
    arc.ArchiveInteger(&ps->speed);
    arc.ArchiveInteger(&ps->jumpvelocity);
    arc.ArchiveInteger(&ps->crouchjumpvelocity);
    arc.ArchiveInteger((int *)&ps->crouchjumpset);
    arc.ArchiveInteger(&ps->pm_stopspeed);
    arc.ArchiveInteger(&ps->pm_airaccelerate);
    arc.ArchiveInteger(&ps->pm_wateraccelerate);
    arc.ArchiveInteger(&ps->pm_friction);
    arc.ArchiveInteger(&ps->pm_waterfriction);
    arc.ArchiveInteger(&ps->pm_accelerate);
    arc.ArchiveInteger(&ps->pm_defaultviewheight);

    for (i = 0; i < 3; i++) {
        arc.ArchiveInteger(&ps->delta_angles[i]);
    }
    arc.ArchiveFloat(&ps->leanDelta);

    arc.ArchiveInteger(&ps->groundEntityNum);
    arc.ArchiveInteger((int *)&ps->walking);
    arc.ArchiveInteger((int *)&ps->jumped);
    arc.ArchiveInteger((int *)&ps->groundPlane);
    arc.ArchiveInteger((int *)&ps->instantJump);
    arc.ArchiveInteger((int *)&ps->strafeJumpingAllowed);
    arc.ArchiveInteger(&ps->feetfalling);
    arc.ArchiveVec3(ps->falldir);

    EF2_ArchiveTraceImage(arc, &ps->groundTrace);

    arc.ArchiveInteger(&ps->clientNum);
    arc.ArchiveVec3(ps->viewangles);
    arc.ArchiveInteger(&ps->viewheight);

    for (i = 0; i < EF2_MAX_STATS; i++) {
        arc.ArchiveInteger(&ps->stats[i]);
    }
    for (i = 0; i < EF2_MAX_ACTIVE_ITEMS; i++) {
        arc.ArchiveInteger(&ps->activeItems[i]);
    }
    for (i = 0; i < EF2_MAX_INVENTORY; i++) {
        arc.ArchiveInteger(&ps->inventory_name_index[i]);
    }
    for (i = 0; i < EF2_MAX_INVENTORY; i++) {
        arc.ArchiveInteger(&ps->inventory_weapon_ammo_index[i]);
    }
    for (i = 0; i < EF2_MAX_INVENTORY; i++) {
        arc.ArchiveInteger(&ps->inventory_weapon_required_ammo[i]);
    }
    for (i = 0; i < EF2_MAX_INVENTORY; i++) {
        arc.ArchiveInteger(&ps->ammo_in_clip[i]);
    }
    for (i = 0; i < EF2_MAX_AMMO; i++) {
        arc.ArchiveInteger(&ps->ammo_name_index[i]);
    }
    for (i = 0; i < EF2_MAX_AMMOCOUNT; i++) {
        arc.ArchiveInteger(&ps->ammo_amount[i]);
    }
    for (i = 0; i < EF2_MAX_AMMOCOUNT; i++) {
        arc.ArchiveInteger(&ps->max_ammo_amount[i]);
    }

    arc.ArchiveInteger(&ps->current_music_mood);
    arc.ArchiveInteger(&ps->fallback_music_mood);
    arc.ArchiveFloat(&ps->music_volume);
    arc.ArchiveFloat(&ps->music_volume_fade_time);
    arc.ArchiveInteger((int *)&ps->allowMusicDucking);

    arc.ArchiveInteger(&ps->reverb_type);
    arc.ArchiveFloat(&ps->reverb_level);

    for (i = 0; i < 4; i++) {
        arc.ArchiveFloat(&ps->blend[i]);
    }
    arc.ArchiveFloat(&ps->fov);

    arc.ArchiveVec3(ps->camera_origin);
    arc.ArchiveVec3(ps->cameraFocalPoint);
    arc.ArchiveInteger((int *)&ps->useCameraFocalPoint);
    arc.ArchiveVec3(ps->camera_angles);
    arc.ArchiveFloat(&ps->camera_time);
    arc.ArchiveVec3(ps->camera_offset);
    arc.ArchiveVec3(ps->damage_angles);
    arc.ArchiveInteger(&ps->camera_flags);
    arc.ArchiveVec3(ps->vehicleoffset);
    arc.ArchiveInteger((int *)&ps->in_vehicle);

    arc.ArchiveInteger(&ps->dialogEntnum);
    arc.ArchiveInteger(&ps->dialogSoundIndex);
    arc.ArchiveInteger(&ps->dialogTextSoundIndex);

    arc.ArchiveInteger(&ps->objectiveNameIndex);
    arc.ArchiveInteger((int *)&ps->objectiveStates);
    arc.ArchiveInteger((int *)&ps->informationStates);
    arc.ArchiveInteger((int *)&ps->missionStatus);
    arc.ArchiveInteger((int *)&ps->viewMode);

    arc.ArchiveInteger(&ps->ping);

    arc.ArchiveInteger(&client->ping);

    EF2_ArchiveCharBlock(arc, client->pers.userinfo, sizeof(client->pers.userinfo), EF2_MAX_INFO_STRING);
    EF2_ArchiveCharBlock(arc, client->pers.netname, sizeof(client->pers.netname), EF2_MAX_NETNAME);
    EF2_ArchiveCharBlock(arc, client->pers.mp_playermodel, sizeof(client->pers.mp_playermodel), EF2_MAX_QPATH);
    EF2_ArchiveCharBlock(arc, client->pers.dm_morph_c1, sizeof(client->pers.dm_morph_c1), EF2_MAX_INFO_STRING);
    arc.ArchiveFloat(&client->pers.enterTime);
    arc.ArchiveInteger((int *)&client->pers.mp_lowBandwidth);
    arc.ArchiveInteger(&zero); 
    arc.ArchiveInteger(&zero); 

    arc.ArchiveVec3(client->cmd_angles);
}



void G_ArchiveEdict(Archiver& arc, gentity_t *edict)
{
    int    i;
    str    tempStr;
    [[maybe_unused]] float fzero = 0.0f;

    assert(edict);


    if (edict->client) {
        G_ArchiveClientRetail(arc, edict->client);
    }

    arc.ArchiveInteger(&edict->s.instanceNumber);  

    arc.ArchiveInteger(&edict->s.eType);
    arc.ArchiveInteger(&edict->s.eFlags);

    // Retail deliberately does not archive pos / apos.

    arc.ArchiveVec3(edict->s.netorigin);
    arc.ArchiveVec3(edict->s.origin);
    arc.ArchiveVec3(edict->s.origin2);
    arc.ArchiveVec3(edict->s.netangles);
    arc.ArchiveVec3(edict->s.angles);

    // because it is the same divergence, not because it is load-bearing there.
    arc.ArchiveVec3(edict->s.viewangles);  

    arc.ArchiveUnsigned((unsigned *)&edict->s.constantLight);

    if (arc.Saving()) {
        if (edict->s.loopSound) {
            tempStr = gi.getConfigstring(CS_SOUNDS + edict->s.loopSound);
        } else {
            tempStr = "";
        }

        arc.ArchiveString(&tempStr);
    } else {
        arc.ArchiveString(&tempStr);

        if (tempStr.length()) {
            edict->s.loopSound = gi.soundindex(tempStr.c_str());
        } else {
            edict->s.loopSound = 0;
        }
    }

    arc.ArchiveFloat(&edict->s.loopSoundVolume);
    arc.ArchiveFloat(&edict->s.loopSoundMinDist);

    arc.ArchiveInteger(&edict->s.parent);
    arc.ArchiveInteger(&edict->s.tag_num);
    arc.ArchiveBoolean(&edict->s.attach_use_angles);
    arc.ArchiveVec3(edict->s.attach_offset);
    arc.ArchiveVec3(edict->s.attach_angles_offset);

    arc.ArchiveInteger(&edict->s.modelindex);
    arc.ArchiveInteger(&edict->s.viewmodelindex);  
    arc.ArchiveInteger(&edict->s.skinNum);

    if (arc.Saving()) {
        if (edict->s.customShader) {
            tempStr = gi.getConfigstring(CS_IMAGES + edict->s.customShader);
        } else {
            tempStr = "";
        }

        arc.ArchiveString(&tempStr);
    } else {
        arc.ArchiveString(&tempStr);

        if (tempStr.length()) {
            edict->s.customShader = gi.imageindex(tempStr.c_str());
        } else {
            edict->s.customShader = 0;
        }
    }

    if (arc.Saving()) {
        if (edict->s.customEmitter) {
            tempStr = gi.getConfigstring(CS_IMAGES + edict->s.customEmitter);
        } else {
            tempStr = "";
        }

        arc.ArchiveString(&tempStr);
    } else {
        arc.ArchiveString(&tempStr);

        if (tempStr.length()) {
            edict->s.customEmitter = gi.imageindex(tempStr.c_str());
        } else {
            edict->s.customEmitter = 0;
        }
    }

    arc.ArchiveFloat(&edict->s.animationRate);
    arc.ArchiveInteger(&edict->s.anim);
    arc.ArchiveInteger(&edict->s.frame);

    arc.ArchiveInteger(&edict->s.crossblend_time);

    arc.ArchiveInteger(&edict->s.torso_anim);
    arc.ArchiveInteger(&edict->s.torso_frame);
    arc.ArchiveInteger(&edict->s.torso_crossblend_time);

    for (i = 0; i < NUM_BONE_CONTROLLERS; i++) {
        arc.ArchiveInteger(&edict->s.bone_tag[i]);
        arc.ArchiveVec3(edict->s.bone_angles[i]);
        arc.ArchiveVec4(edict->s.bone_quat[i]);
    }

    arc.ArchiveRaw(&edict->s.morph_controllers, sizeof(edict->s.morph_controllers));

    arc.ArchiveRaw(&edict->s.surfaces, sizeof(edict->s.surfaces));

    arc.ArchiveInteger(&edict->s.clientNum);
    arc.ArchiveInteger(&edict->s.groundEntityNum);
    arc.ArchiveInteger(&edict->s.solid);

    arc.ArchiveFloat(&edict->s.scale);
    arc.ArchiveFloat(&edict->s.alpha);
    arc.ArchiveInteger(&edict->s.renderfx);

    arc.ArchiveUnsigned(&edict->s.affectingViewModes);
    arc.ArchiveInteger(&edict->s.archeTypeIndex);
    arc.ArchiveInteger(&edict->s.missionObjective);
    arc.ArchiveInteger(&edict->s.infoIcon);

    arc.ArchiveRaw(&edict->s.effectsAnims, sizeof(edict->s.effectsAnims));

    arc.ArchiveInteger(&edict->s.bindparent);

    arc.ArchiveVec4(edict->s.quat);
    arc.ArchiveRaw(&edict->s.mat, sizeof(edict->s.mat));

    arc.ArchiveInteger(&edict->svflags);

    arc.ArchiveVec3(edict->mins);
    arc.ArchiveVec3(edict->maxs);
    arc.ArchiveInteger(&edict->contents);
    arc.ArchiveVec3(edict->absmin);
    arc.ArchiveVec3(edict->absmax);
    arc.ArchiveFloat(&edict->radius);

    if (arc.Loading()) {
        edict->radius2 = edict->radius * edict->radius;
    }

    arc.ArchiveVec3(edict->centroid);

    arc.ArchiveVec3(edict->currentOrigin);
    arc.ArchiveVec3(edict->currentAngles);

    arc.ArchiveInteger(&edict->ownerNum);
    ArchiveEnum(edict->solid, solid_t);
    arc.ArchiveFloat(&edict->freetime);
    arc.ArchiveFloat(&edict->spawntime);

    tempStr = str(edict->entname);
    arc.ArchiveString(&tempStr);
    strncpy(edict->entname, tempStr.c_str(), sizeof(edict->entname) - 1);

    arc.ArchiveInteger(&edict->clipmask);

    if (arc.Loading()) {
        gi.linkentity(edict);
    }
}

void CloneEntity(Entity *dest, const Entity *src)
{
    int i, num;

    dest->setModel(src->model);
    dest->setOrigin(src->origin);
    dest->setAngles(src->angles);
    dest->setScale(src->edict->s.scale);
    dest->setAlpha(src->edict->s.alpha);
    dest->health = src->health;
    memcpy(dest->edict->s.surfaces, src->edict->s.surfaces, sizeof(src->edict->s.surfaces));
    dest->edict->s.constantLight = src->edict->s.constantLight;
    dest->edict->s.renderfx = src->edict->s.renderfx;

    if (src->bind_info) {
        num = src->bind_info->numchildren;
        for (i = 0; (i < MAX_MODEL_CHILDREN) && num; i++) {
            Entity *clone;
            Entity *child;

            if (!src->bind_info->children[i]) {
                continue;
            }
            child = G_GetEntity(src->bind_info->children[i]);
            if (child) {
                clone = new Entity(ENTITY_CREATE_FLAG_ANIMATE);
                CloneEntity(clone, child);
                clone->attach(dest->entnum, child->edict->s.tag_num);
            }
            num--;
        }
    }
}

weaponhand_t WeaponHandNameToNum(str side)
{
    if (!side.length()) {
        gi.DPrintf("WeaponHandNameToNum : Weapon hand not specified\n");
        return WEAPON_ERROR;
    }

    // Retail's strings (SDK g_utils.cpp:2134-2158), RETAIL-EXACT -- no port-only
    if (!side.icmp("righthand") || !side.icmp("right")) {
        return WEAPON_RIGHT;
    } else if (!side.icmp("lefthand") || !side.icmp("left")) {
        return WEAPON_LEFT;
    } else if (!side.icmp("dualhand") || !side.icmp("dual")) {
        return WEAPON_DUAL;
    } else {
        return (weaponhand_t)atoi(side);
    }
}

const char *WeaponHandNumToName(weaponhand_t hand)
{
    switch (hand) {
    case WEAPON_RIGHT:
        return "righthand";
    case WEAPON_LEFT:
        return "lefthand";
    case WEAPON_DUAL:
        return "dualhand";
    case WEAPON_ANY:
        return "anyhand";
    default:
        return "Invalid Hand";
    }
}

firemode_t WeaponModeNameToNum(str mode)
{
    if (!mode.length()) {
        gi.DPrintf("WeaponModeNameToNum : Weapon mode not specified\n");
        return FIRE_ERROR;
    }

    if (!mode.icmp("primary")) {
        return FIRE_MODE1;
    }

    if (!mode.icmp("alternate")) {
        return FIRE_MODE2;
    }

    if (!mode.icmp("MODE1") || !mode.icmp("FIRE_MODE1")) {
        return FIRE_MODE1;
    }

    if (!mode.icmp("MODE2") || !mode.icmp("FIRE_MODE2")) {
        return FIRE_MODE2;
    }

    if (!mode.icmp("MODE3") || !mode.icmp("FIRE_MODE3")) {
        return FIRE_MODE3;
    }

    return (firemode_t)atoi(mode);
}


// names/percentages are not the per-model ones retail uses (ferengi_oolpax's exp_Relief

void G_DebugTargets(Entity *e, str from)
{
    gi.DPrintf("DEBUGTARGETS:%s ", from.c_str());

    if (e->TargetName() && strlen(e->TargetName())) {
        gi.DPrintf("Targetname=\"%s\"\n", e->TargetName());
    } else {
        gi.DPrintf("Targetname=\"None\"\n");
    }

    if (e->Target() && strlen(e->Target())) {
        gi.DPrintf("Target=\"%s\"\n", e->Target());
    } else {
        gi.DPrintf("Target=\"None\"\n");
    }
}

void G_DebugDamage(float damage, Entity *victim, Entity *attacker, Entity *inflictor)
{
    gi.DPrintf(
        "Victim:%s Attacker:%s Inflictor:%s Damage:%f\n",
        victim->getClassname(),
        attacker->getClassname(),
        inflictor->getClassname(),
        damage
    );
}

void G_FadeOut(float delaytime)
{
    // Make sure we are not already faded or fading out -- SDK g_utils.cpp:2241-2251.
    if ((level.m_fade_type == fadeout) && (level.m_fade_time_start > 0.0f)) {
        float alpha;

        alpha = 1.0f - (level.m_fade_time / level.m_fade_time_start);

        if (alpha > 0.0f) {
            return;
        }
    }

    level.m_fade_color      = Vector(0, 0, 0);
    level.m_fade_alpha      = 1.0f;
    level.m_fade_time       = delaytime;
    level.m_fade_time_start = delaytime;
    level.m_fade_type       = fadeout;
    level.m_fade_style      = alphablend;
}

void G_AutoFadeIn(void)
{
    level.m_fade_time_start = 1;
    level.m_fade_time       = 1;
    level.m_fade_color[0]   = 0;
    level.m_fade_color[1]   = 0;
    level.m_fade_color[2]   = 0;
    level.m_fade_alpha      = 1;
    level.m_fade_type       = fadein;
    level.m_fade_style      = alphablend;
}

void G_ClearFade(void)
{
    level.m_fade_time = -1;
    level.m_fade_type = fadein;
}

void G_FadeSound(float delaytime)
{
    float time;

    time = delaytime * 1000;
    gi.SendServerCommand(0, "%s", va("fadesound %0.2f", time));
}

void G_RestartLevelWithDelay(float delaytime)
{
    int i;

    if (g_gametype->integer != GT_SINGLE_PLAYER) {
        return;
    }

    if (level.died_already) {
        return;
    }

    level.died_already = true;

    for (i = 0; i < game.maxclients; i++) {
        if (g_entities[i].inuse) {
            if (g_entities[i].entity) {
                g_entities[i].entity->PostEvent(EV_Player_Respawn, delaytime);
            }
        }
    }

    // used to append is a port-only invention: retail's G_RestartLevelWithDelay__Ff
}


static str g_failedCondition;

void G_MissionFailed(const str& reason)
{
    str playerDeathThread;

    if (level.mission_failed) {
        return;
    }


    ChangeMusic("failure", "normal", true);

    g_failedCondition = reason;
    gi.failedcondition(reason.c_str());

    playerDeathThread = level._playerDeathThread;

    if ((Q_stricmpn(reason.c_str(), "PlayerKilled", strlen("PlayerKilled")) == 0) && playerDeathThread.length()) {
        ExecuteThread(playerDeathThread);  
    } else {
        G_FinishMissionFailed();
    }
}

void G_FinishMissionFailed(void)
{
    Entity *entity;
    Player *player;


    entity = g_entities[0].entity;

    if (!entity || !entity->isSubclassOf(Player)) {
        return;
    }

    player = (Player *)entity;

    G_FadeOut(1.0f);
    G_FadeSound(1.0f);


    player->setMissionFailed();

    level.mission_failed = true;
    level.playerfrozen   = true;


    if (level.m_letterbox_dir == letterbox_in) {
        level.m_letterbox_time_start = 0.5f;
        level.m_letterbox_dir        = letterbox_out;

        level.m_letterbox_time     = 0.5f;
        level.m_letterbox_fraction = 1.0f / 8.0f;
    }
}

void G_StartCinematic(void)
{
    Entity *entity;

    level.cinematic = true;
    gi.cvar_set("sv_cinematic", "1");

    entity = g_entities[0].entity;
    if (entity && entity->isSubclassOf(Player)) {
        Player *player = (Player *)entity;
        player->cinematicStarted();
    }
}

void G_StopCinematic(void)
{
    Entity *entity;

    world->skipthread = "";

    level.cinematic = false;
    gi.cvar_set("sv_cinematic", "0");

    entity = g_entities[0].entity;
    if (entity && entity->isSubclassOf(Player)) {
        Player *player = (Player *)entity;
        player->cinematicStopped();
    }
}

int G_NumClients(void)
{
    gentity_t *ent;
    int        i;
    int        count = 0;

    for (i = 0, ent = g_entities; i < game.maxclients; ent++, i++) {
        if (!ent->inuse || !ent->entity) {
            continue;
        }

        count++;
    }

    return count;
}
















BindInfo *CreateBindInfo(void)
{
    BindInfo *new_bind_info;

    new_bind_info = new BindInfo;

    if (!new_bind_info) {
        gi.Error(ERR_DROP, "Couldn't alloc BindInfo");
    }

    return new_bind_info;
}

MorphInfo *CreateMorphInfo(void)
{
    MorphInfo *new_morph_info;

    new_morph_info = new MorphInfo;

    if (!new_morph_info) {
        gi.Error(ERR_DROP, "Couldn't alloc MorphInfo");
    }

    return new_morph_info;
}

void G_CacheStateMachineAnims(Entity *ent, const char *stateMachineName)
{
    Container<const char *> animNames;
    const char *animName;
    StateMap *stateMap;
    int i;

    if (ent->isSubclassOf(Player)) {
        stateMap = GetStatemap(stateMachineName, (Condition<Class> *)Player::Conditions, NULL, false, true);
    } else if (ent->isSubclassOf(Actor)) {
        stateMap = GetStatemap(stateMachineName, (Condition<Class> *)Actor::Conditions, NULL, false, true);
    } else {
        return;
    }

    stateMap->GetAllAnims(&animNames);


    for (i = 1; i <= animNames.NumObjects(); i++) {
        animName = animNames.ObjectAt(i);


        gi.Anim_Random(ent->edict->s.modelindex, animName);
    }
}



int G_FindConfigstringIndex( const char *name, int start, int max, qboolean create )
{
	int		i;
	char	   *s;
	
	if ( !name || !name[0] )
	{
		return 0;
	   }
	
	for ( i=1 ; i<max ; i++ )
	{
		s = gi.getConfigstring( start + i );
		if ( !s || !s[0] )
		{
			break;
		}
		if ( !strcmp( s, name ) )
		{
			return i;
		}
	   }
	
	if ( !create )
	{
		return 0;
	   }
	
	if ( i == max )
	{
		gi.Error( ERR_DROP, "G_FindConfigstringIndex: overflow" );
	   }
	
	gi.setConfigstring( start + i, name );
	return i;
}

int G_ModelIndex( const char *name )
{
	return G_FindConfigstringIndex (name, CS_MODELS, MAX_MODELS, true);
}


void G_AllowMusicDucking( bool allowMusicDucking )
{
	int      j;
	gentity_t  *other;

	for( j = 0; j < game.maxclients; j++ )
	{
		other = &g_entities[ j ];

		if ( other->inuse && other->client )
		{
			Player *player;

			player = ( Player * )other->entity;
			player->allowMusicDucking( allowMusicDucking );
		}
	}
}

void G_AllowActionMusic( bool allowActionMusic )
{
	int      j;
	gentity_t  *other;

	for( j = 0; j < game.maxclients; j++ )
	{
		other = &g_entities[ j ];

		if ( other->inuse && other->client )
		{
			Player *player;

			player = ( Player * )other->entity;
			player->allowActionMusic( allowActionMusic );
		}
	}
}



void CacheResource( const char * stuff, Entity * ent )
{
	str real_stuff;

	if ( !stuff || !ent )
		return;

	if ( !strchr( stuff, '.' ) )
	{
		stuff = gi.GlobalAlias_FindRandom( stuff );
		if ( !stuff )
		{
			return;
		}
	}

	real_stuff = stuff;
	real_stuff.tolower();

	if ( strstr( real_stuff.c_str(), ".wav" ) )
	{
		gi.soundindex( real_stuff.c_str() );
	}
	else if ( strstr( real_stuff.c_str(), ".mp3" ) )
	{
		gi.soundindex( real_stuff.c_str() );
	}
	else if ( strstr( real_stuff.c_str(), ".tik" ) )
	{
		int index;

		index = modelIndex( real_stuff.c_str() );

		if ( index > 0 )
		{
			 if ( !ent )
			 {
				 ent = new Entity;

				 ent->ProcessInitCommands( index, true );

				 delete ent;
				 ent = NULL;
			 }
			 else
			 {
				 ent->ProcessInitCommands( index, true );
			 }
		}
	}
	else if ( strstr( real_stuff.c_str(), ".spr" ) )
	{
		gi.modelindex( real_stuff.c_str() );
	}
	else if ( strstr( real_stuff.c_str(), ".ska" ) )
	{
		gi.TIKI_CacheAnim( real_stuff.c_str() );
	}
	else if ( strstr( real_stuff.c_str(), ".st" ) )
	{
		if ( strncmp( real_stuff.c_str(), "ai/", 3 ) == 0 )
		{
			CacheStatemap( stuff, ( Condition<Class> * )Actor::Conditions );
		}
	}
	else if ( strstr( real_stuff.c_str(), ".img" ) )
	{
		real_stuff.CapLength( real_stuff.length() - 4 );
		gi.imageindex( real_stuff.c_str() );
	}
	else if ( strstr( real_stuff.c_str(), ".itm" ) )
	{
		real_stuff.CapLength( real_stuff.length() - 4 );
		gi.itemindex( real_stuff.c_str() );
	}
	else if ( strstr( real_stuff.c_str(), ".arc" ) )
	{
		real_stuff.CapLength( real_stuff.length() - 4 );
		gi.archetypeindex( real_stuff.c_str() );
	}
}

void ChangeSoundtrack( const char * soundtrack )
{
	level.saved_soundtrack = level.current_soundtrack;
	level.current_soundtrack = soundtrack;
	
	gi.setConfigstring( CS_SOUNDTRACK, soundtrack );
	gi.DPrintf( "soundtrack switched to %s.\n", soundtrack );
}

void ChangeMusic( const char *current, const char *fallback, qboolean force )
{
	int      j;
	gentity_t  *other;

	if ( current || fallback )
	{
		for( j = 0; j < game.maxclients; j++ )
		{
			other = &g_entities[ j ];
			if ( other->inuse && other->client )
			{
				Player *client;

				client = ( Player * )other->entity;
				client->ChangeMusic( current, fallback, force );
			}
		}
		if ( current && fallback )
		{
			gi.DPrintf( "music set to %s with fallback %s\n", current, fallback );
		}
	}
}

void ChangeMusicVolume( float volume, float fade_time )
{
	int      j;
	gentity_t  *other;

	for( j = 0; j < game.maxclients; j++ )
	{
		other = &g_entities[ j ];
		if ( other->inuse && other->client )
		{
			Player *client;

			client = ( Player * )other->entity;
			client->ChangeMusicVolume( volume, fade_time );
		}
	}
	gi.DPrintf( "music volume set to %.2f, fade time %.2f\n", volume, fade_time );
}

void RestoreMusicVolume( float fade_time )
{
	int      j;
	gentity_t  *other;

	for( j = 0; j < game.maxclients; j++ )
	{
		other = &g_entities[ j ];
		if ( other->inuse && other->client )
		{
			Player *client;

			client = ( Player * )other->entity;
			client->RestoreMusicVolume( fade_time );
		}
	}
}

CThread *ExecuteThread( const str &thread_name, qboolean start, Entity *currentEnt )
{
	if ( thread_name.length() )
	{
		CThread *pThread;
		
		pThread = Director.CreateThread( thread_name.c_str() );
		if ( pThread )
		{
			if ( currentEnt )
				pThread->SetCurrentEntity(currentEnt);
			
			if ( start )
            {
				pThread->Start();
            }
		}
		else
		{
			gi.WDPrintf( "StartThread::unable to go to %s\n", thread_name.c_str() );
			return NULL;
		}
		
		return pThread;
	}
	
	return NULL;
}

void G_BroadcastAlert( Entity *soundent, const Vector &origin, const Vector &enemy,	float radius )
{
	Sentient *ent;
	Vector	delta;
	Event		*ev;
	str		name;
	float    r2;
	float    dist2;
	int		i;
	int		n;

	
	
	assert( soundent );
	if ( soundent && !( soundent->flags & FL_NOTARGET ) )
	{
		r2 = radius * radius;
		n = SentientList.NumObjects();
		for( i = 1; i <= n; i++ )
		{
			ent = SentientList.ObjectAt( i );
			if ( ( ent == soundent ) || ent->deadflag )
            {
				continue;
            }
			
			if ( ent->isSubclassOf( Actor ) )
			{
				delta = origin - ent->centroid;
				
				dist2 = delta * delta;
				if (
					( dist2 <= r2 ) &&
					(
					( soundent->edict->areanum == ent->edict->areanum ) ||
					( gi.AreasConnected( soundent->edict->areanum, ent->edict->areanum ) )
					)
					)
					
				{
					ev = new Event( EV_HeardSound );
					ev->AddEntity( soundent );
					ev->AddVector( enemy );
					ent->PostEvent( ev, 0.0f );


				}		
			}
		}


	}
}

void G_BroadcastSound( Entity *soundent, const Vector &origin, float radius, int soundType )
{
	Sentient *ent;
	Vector	delta;
	Event		*ev;
	str		name;
	float    r2;
	float    dist2;
	int		i;
	int		n;

	
	
	assert( soundent );
	if ( soundent && !( soundent->flags & FL_NOTARGET ) )
	{
		r2 = radius * radius;
		n = SentientList.NumObjects();
		for( i = 1; i <= n; i++ )
		{
			ent = SentientList.ObjectAt( i );
			if ( ( ent == soundent ) || ent->deadflag )
            {
				continue;
            }
			
			if ( ent->isSubclassOf( Actor ) )
			{
				delta = origin - ent->centroid;
				
				dist2 = delta * delta;
				if (
					( dist2 <= r2 ) &&
					(
					( soundent->edict->areanum == ent->edict->areanum ) ||
					( gi.AreasConnected( soundent->edict->areanum, ent->edict->areanum ) )
					)
					)
					
				{
					ev = new Event( EV_HeardSound );
					ev->AddEntity( soundent );
					ev->AddVector( origin );
					ev->AddInteger( soundType );
					ent->PostEvent( ev, 0.0f );

					
				}
				
			}
		}
		
		
	}
}

void G_SetConstantLight( unsigned int * constantlight, const float * red, const float * green, const float * blue,
		const float * radius, int * lightStyle )
{
	int ir, ig, ib, iradius;
	
	if ( !constantlight )
		return;
	
	ir = (*constantlight) & 255;
	ig = ( (*constantlight) >> 8 ) & 255;
	ib = ( (*constantlight) >> 16 ) & 255;
	iradius = ( (*constantlight) >> 24 ) & 255;
	
	if ( red )
	{
		ir = *red * 255;
		if ( ir > 255 )
			ir = 255;
	}
	
	if ( green )
	{
		ig = *green * 255;
		if ( ig > 255 )
			ig = 255;
	}
	
	if ( blue )
	{
		ib = *blue * 255;
		if ( ib > 255 )
			ib = 255;
	}
	
	if ( radius )
	{
		iradius = *radius / CONSTANTLIGHT_RADIUS_SCALE;
		if ( iradius > 255 )
			iradius = 255;
	}
	
	if ( lightStyle )
	{
		ir = *lightStyle;
		if ( ir > 255 )
			ir = 255;
	}
	*constantlight = ( ir ) + ( ig << 8 ) + ( ib << 16 ) + ( iradius << 24 );
}

void RestoreSoundtrack( void )
{
	if ( level.saved_soundtrack.length() )
	{
		level.current_soundtrack = level.saved_soundtrack;
		level.saved_soundtrack = "";
		gi.setConfigstring( CS_SOUNDTRACK, level.current_soundtrack.c_str() );
		gi.DPrintf( "soundtrack restored %s.\n", level.current_soundtrack.c_str() );
	}
}

char soundtype_strings[ SOUNDTYPE_TOTAL_NUMBER ][ 32 ] =
{
	"none",
	"general",
	"explosion",
	"weaponfire",
	"alert",
	"footsteps_walk",
	"footsteps_run",
	"fall",
		
	"dialog_context_spotted_enemy",
	"dialog_context_injured",
	"dialog_context_in_combat",
	"dialog_context_weapon_useless",
	"dialog_context_investigating"
};

int Soundtype_string_to_int ( const str &soundtype_string )
{
	for (int i = 0 ; i < SOUNDTYPE_TOTAL_NUMBER ; i++ )
	{
		if ( !soundtype_string.icmp( soundtype_strings[ i ] ) )
			return i;
	}
	
	gi.WDPrintf( "Unknown soundtype - %s\n", soundtype_string.c_str() );
	return -1;
}



int modelIndex( const char *mdl )
{
	str name;
	
	assert( mdl );
	
	if ( !mdl )
	{
		return 0;
	}
	
	if ( ( strlen( mdl ) > 0 ) && !strchr( mdl, '*' ) && strnicmp( "models/", mdl, 7 ) && strstr( mdl, ".tik" ) )
	{
		name = "models/";
	}
	
	name += mdl;
	
	return gi.modelindex( name.c_str() );
}

float G_GetDatabaseFloat( const str &prefix, const str &objectName, const str &varName )
{
	GameplayManager *gpm;
	str gameplayObjectName;


	gpm = GameplayManager::getTheGameplayManager();

	if ( !gpm )
		return 0.0f;


	gameplayObjectName = prefix + objectName;

	return gpm->getFloatValue( gameplayObjectName, varName );
}

str G_GetDatabaseString( const str &prefix, const str &objectName, const str &varName )
{
	GameplayManager *gpm;
	str gameplayObjectName;


	gpm = GameplayManager::getTheGameplayManager();

	if ( !gpm )
		return 0.0f;


	gameplayObjectName = prefix + objectName;

	return gpm->getStringValue( gameplayObjectName, varName );
}

void G_DrawXYBox(const Vector &center, float width, float r, float g, float b, float alpha )
{
	float squareSize=width/2.0f;
	Vector topLeftPoint		= center + Vector(-squareSize, -squareSize, 0);
	Vector topRightPoint	= center + Vector(+squareSize, -squareSize, 0);
	Vector bottomRightPoint	= center + Vector(+squareSize, +squareSize, 0);
	Vector bottomLeftPoint	= center + Vector(-squareSize, +squareSize, 0);
	G_DebugLine( topLeftPoint,		topRightPoint, r, g, b, alpha );
	G_DebugLine( topRightPoint,		bottomRightPoint, r, g, b, alpha );
	G_DebugLine( bottomRightPoint,	bottomLeftPoint, r, g, b, alpha );
	G_DebugLine( bottomLeftPoint,	topLeftPoint, r, g, b, alpha );
}

void G_TouchTeleporters( Entity *ent )
{
	int		i;
	int		num;
	int      touch[ MAX_GENTITIES ];
	gentity_t  *hit;
	Event		*ev;
	

	if ( ( ent->client || ( ent->edict->svflags & SVF_MONSTER ) ) && ( ent->health <= 0.0f ) )
	{
		return;
	}
	

	num = gi.AreaEntities( ent->absmin, ent->absmax, touch, MAX_GENTITIES, qfalse );
	

	for( i = 0 ; i < num ; i++ )
	{
		hit = &g_entities[ touch[ i ] ];

		if ( !hit->inuse || ( hit->entity == ent ) || ( hit->solid != SOLID_TRIGGER ) ) 
			continue;
		
		if ( hit->entity && hit->entity->isSubclassOf( Teleporter ) )
		{
			ev = new Event( EV_Touch );
			ev->AddEntity( ent );
			hit->entity->ProcessEvent( ev );
		}
	}
}

