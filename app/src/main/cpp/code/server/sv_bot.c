// Retail does NOT ship a separate aas/ea/ai export table across the game

#include "server.h"
#include "../botlib/botlib.h"

typedef struct bot_debugpoly_s {
    int    inuse;
    int    color;
    int    numPoints;
    vec3_t points[128];
} bot_debugpoly_t;

static bot_debugpoly_t *debugpolygons;
static int              bot_maxdebugpolys;

botlib_export_t *botlib_export = NULL;

// capture's retail run shows the export is never called with bot_enable 0.
cvar_t *bot_enable;
int     sv_botEnable = 0;

static qboolean botlib_initialized = qfalse;


static int FS_Read_Bot(void *ptr, int size, fileHandle_t f)
{
    return (int)FS_Read(ptr, size, f);
}

static int FS_Write_Bot(const void *ptr, int size, fileHandle_t f)
{
    return (int)FS_Write(ptr, size, f);
}

__attribute__((format(printf, 2, 3)))

void QDECL BotImport_Print(int type, char *fmt, ...)
{
    char    str[2048];
    va_list ap;

    va_start(ap, fmt);
    Q_vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    switch (type) {
    case PRT_MESSAGE:
        Com_Printf("%s", str);
        break;
    case PRT_WARNING:
        Com_Printf("Warning: %s", str);
        break;
    case PRT_ERROR:
        Com_Printf("Error: %s", str);
        break;
    case PRT_FATAL:
        Com_Printf("Fatal: %s", str);
        break;
    case PRT_EXIT:
        Com_Error(ERR_DROP, "Exit: %s", str);
        break;
    default:
        Com_Printf("unknown print type\n");
        break;
    }
}

static void BotImport_Trace(
    bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask
)
{
    trace_t trace;

    SV_Trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse);

    bsptrace->allsolid   = trace.allsolid;
    bsptrace->startsolid = trace.startsolid;
    bsptrace->fraction   = trace.fraction;
    VectorCopy(trace.endpos, bsptrace->endpos);
    bsptrace->plane.dist     = trace.plane.dist;
    VectorCopy(trace.plane.normal, bsptrace->plane.normal);
    bsptrace->plane.signbits = trace.plane.signbits;
    bsptrace->plane.type     = trace.plane.type;
    bsptrace->surface.value  = 0;
    bsptrace->surface.flags  = trace.surfaceFlags;
    bsptrace->ent            = trace.entityNum;
    bsptrace->exp_dist       = 0;
    bsptrace->sidenum        = 0;
    bsptrace->contents       = 0;
}

static void BotImport_EntityTrace(
    bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask
)
{
    trace_t trace;

    SV_ClipToEntity(&trace, start, mins, maxs, end, entnum, contentmask);

    bsptrace->allsolid   = trace.allsolid;
    bsptrace->startsolid = trace.startsolid;
    bsptrace->fraction   = trace.fraction;
    VectorCopy(trace.endpos, bsptrace->endpos);
    bsptrace->plane.dist     = trace.plane.dist;
    VectorCopy(trace.plane.normal, bsptrace->plane.normal);
    bsptrace->plane.signbits = trace.plane.signbits;
    bsptrace->plane.type     = trace.plane.type;
    bsptrace->surface.value  = 0;
    bsptrace->surface.flags  = trace.surfaceFlags;
    bsptrace->ent            = trace.entityNum;
    bsptrace->exp_dist       = 0;
    bsptrace->sidenum        = 0;
    bsptrace->contents       = 0;
}

int BotImport_PointContents(vec3_t point)
{
    return SV_PointContents(point, -1);
}

static int BotImport_inPVS(vec3_t p1, vec3_t p2)
{
    return SV_inPVS(p1, p2);
}

char *BotImport_BSPEntityData(void)
{
    return CM_EntityString();
}

void BotImport_BSPModelMinsMaxsOrigin(
    int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin
)
{
    clipHandle_t h;
    vec3_t       mins, maxs;
    float        max;
    int          i;

    h = CM_InlineModel(modelnum);
    CM_ModelBounds(h, mins, maxs);

    if (angles[0] || angles[1] || angles[2]) {
        max = RadiusFromBounds(mins, maxs);
        for (i = 0; i < 3; i++) {
            mins[i] = -max;
            maxs[i] = max;
        }
    }

    if (outmins) {
        VectorCopy(mins, outmins);
    }
    if (outmaxs) {
        VectorCopy(maxs, outmaxs);
    }
    if (origin) {
        VectorClear(origin);
    }
}

static void *BotImport_GetMemory(int size)
{
    return Z_TagMalloc(size, TAG_BOTLIB);
}

static void BotImport_FreeMemory(void *ptr)
{
    Z_Free(ptr);
}

int BotImport_AvailableMemory(void)
{
    return 0;
}

void *BotImport_HunkAlloc(int size)
{
    return Hunk_Alloc(size, h_high);
}


static void BotImport_DebugPolygonShow(int id, int color, int numPoints, vec3_t *points)
{
    bot_debugpoly_t *poly;

    if (!debugpolygons) {
        return;
    }
    poly = &debugpolygons[id];
    poly->inuse = qtrue;
    poly->color = color;
    poly->numPoints = numPoints;
    Com_Memcpy(poly->points, points, numPoints * sizeof(vec3_t));
}

int BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t *points)
{
    bot_debugpoly_t *poly;
    int              i;

    if (!debugpolygons) {
        return 0;
    }

    for (i = 1; i < bot_maxdebugpolys; i++) {
        if (!debugpolygons[i].inuse) {
            break;
        }
    }
    if (i >= bot_maxdebugpolys) {
        return 0;
    }

    poly = &debugpolygons[i];
    poly->inuse = qtrue;
    poly->color = color;
    poly->numPoints = numPoints;
    Com_Memcpy(poly->points, points, numPoints * sizeof(vec3_t));

    return i;
}

void BotImport_DebugPolygonDelete(int id)
{
    if (!debugpolygons) {
        return;
    }
    debugpolygons[id].inuse = qfalse;
}

int BotImport_DebugLineCreate(void)
{
    vec3_t points[1];

    return BotImport_DebugPolygonCreate(0, 0, points);
}

void BotImport_DebugLineDelete(int line)
{
    BotImport_DebugPolygonDelete(line);
}

void BotImport_DebugLineShow(int line, vec3_t start, vec3_t end, int color)
{
    vec3_t points[4], dir, cross, up = {0, 0, 1};
    float  dot;

    VectorCopy(start, points[0]);
    VectorCopy(start, points[1]);
    VectorCopy(end, points[2]);
    VectorCopy(end, points[3]);

    VectorSubtract(end, start, dir);
    VectorNormalize(dir);
    dot = DotProduct(dir, up);
    if (dot > 0.99 || dot < -0.99) {
        VectorSet(cross, 1, 0, 0);
    } else {
        CrossProduct(dir, up, cross);
    }
    VectorNormalize(cross);

    VectorMA(points[0], 2, cross, points[0]);
    VectorMA(points[1], -2, cross, points[1]);
    VectorMA(points[2], -2, cross, points[2]);
    VectorMA(points[3], 2, cross, points[3]);

    BotImport_DebugPolygonShow(line, color, 4, points);
}

void BotClientCommand(int client, const char *command)
{
    SV_ExecuteClientCommand(&svs.clients[client], command);  
}


void SV_BotInitCvars(void)
{
    Cvar_Get("bot_developer", "0", CVAR_CHEAT);
    Cvar_Get("bot_debug", "0", CVAR_CHEAT);
    Cvar_Get("bot_maxdebugpolys", "2", 0);
    Cvar_Get("bot_groundonly", "1", 0);
    Cvar_Get("bot_reachability", "0", 0);
    Cvar_Get("bot_visualizejumppads", "0", CVAR_CHEAT);
    Cvar_Get("bot_forceclustering", "0", 0);
    Cvar_Get("bot_forcereachability", "0", 0);
    Cvar_Get("bot_forcewrite", "0", 0);
    Cvar_Get("bot_aasoptimize", "0", 0);
    Cvar_Get("bot_saveroutingcache", "0", 0);
    Cvar_Get("bot_thinktime", "100", CVAR_CHEAT);
    Cvar_Get("bot_reloadcharacters", "0", 0);
    Cvar_Get("bot_testichat", "0", 0);
    Cvar_Get("bot_testrchat", "0", 0);
    Cvar_Get("bot_testsolid", "0", CVAR_CHEAT);
    Cvar_Get("bot_testclusters", "0", CVAR_CHEAT);
    Cvar_Get("bot_fastchat", "0", 0);
    Cvar_Get("bot_nochat", "0", 0);
    Cvar_Get("bot_pause", "0", CVAR_CHEAT);
    Cvar_Get("bot_report", "0", CVAR_CHEAT);
    Cvar_Get("bot_grapple", "0", 0);
    Cvar_Get("bot_rocketjump", "1", 0);
    Cvar_Get("bot_challenge", "0", 0);
    Cvar_Get("bot_minplayers", "0", 0);
    Cvar_Get("bot_interbreedchar", "", CVAR_CHEAT);
    Cvar_Get("bot_interbreedbots", "10", CVAR_CHEAT);
    Cvar_Get("bot_interbreedcycle", "20", CVAR_CHEAT);
    Cvar_Get("bot_interbreedwrite", "", CVAR_CHEAT);
}

void SV_BotInitBotLib(void)
{
    botlib_import_t botlib_import;

    if (debugpolygons) {
        Z_Free(debugpolygons);
    }
    bot_maxdebugpolys = Cvar_VariableIntegerValue("bot_maxdebugpolys");
    debugpolygons = Z_Malloc(sizeof(bot_debugpoly_t) * bot_maxdebugpolys);

    memset(&botlib_import, 0, sizeof(botlib_import));

    botlib_import.Print = BotImport_Print;
    botlib_import.Trace = BotImport_Trace;
    botlib_import.EntityTrace = BotImport_EntityTrace;
    botlib_import.PointContents = BotImport_PointContents;
    botlib_import.inPVS = BotImport_inPVS;
    botlib_import.BSPEntityData = BotImport_BSPEntityData;
    botlib_import.BSPModelMinsMaxsOrigin = BotImport_BSPModelMinsMaxsOrigin;
    botlib_import.BotClientCommand = BotClientCommand;

    botlib_import.GetMemory = BotImport_GetMemory;
    botlib_import.FreeMemory = BotImport_FreeMemory;
    botlib_import.AvailableMemory = BotImport_AvailableMemory;
    botlib_import.HunkAlloc = BotImport_HunkAlloc;

    botlib_import.FS_FOpenFile = FS_FOpenFileByMode;
    botlib_import.FS_Read = FS_Read_Bot;
    botlib_import.FS_Write = FS_Write_Bot;
    botlib_import.FS_FCloseFile = FS_FCloseFile;
    botlib_import.FS_Seek = FS_Seek;

    botlib_import.DebugLineCreate = BotImport_DebugLineCreate;
    botlib_import.DebugLineDelete = BotImport_DebugLineDelete;
    botlib_import.DebugLineShow = BotImport_DebugLineShow;

    botlib_import.DebugPolygonCreate = BotImport_DebugPolygonCreate;
    botlib_import.DebugPolygonDelete = BotImport_DebugPolygonDelete;

    botlib_export = GetBotLibAPI(BOTLIB_API_VERSION, &botlib_import);
    botlib_initialized = (botlib_export != NULL);
}


int SV_BotLibSetup(void)
{
    if (!bot_enable || !bot_enable->integer) {
        return BLERR_NOERROR;
    }
    if (!botlib_export) {
        Com_Printf("Error: SV_BotLibSetup without SV_BotInitBotLib\n");
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibSetup();
}

int SV_BotLibShutdown(void)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibShutdown();
}

int SV_BotLibVarSet(char *var_name, char *value)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibVarSet(var_name, value);
}

int SV_BotLibVarGet(char *var_name, char *value, int size)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibVarGet(var_name, value, size);
}

int SV_PC_AddGlobalDefine(char *string)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->PC_AddGlobalDefine(string);
}

int SV_PC_LoadSourceHandle(const char *filename)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->PC_LoadSourceHandle(filename);
}

int SV_PC_FreeSourceHandle(int handle)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->PC_FreeSourceHandle(handle);
}

int SV_PC_SourceFileAndLine(int handle, char *filename, int *line)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->PC_SourceFileAndLine(handle, filename, line);
}

void SV_BotFrame(int time)
{
    if (sv_botEnable) {
        ge->BotAIStartFrame(time);
    }
}

int SV_BotLibStartFrame(float time)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibStartFrame(time);
}

int SV_BotLibLoadMap(const char *mapname)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibLoadMap(mapname);
}

int SV_BotLibUpdateEntity(int ent, bot_entitystate_t *state)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->BotLibUpdateEntity(ent, state);
}

int SV_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3)
{
    if (!botlib_export) {
        return BLERR_LIBRARYNOTSETUP;
    }
    return botlib_export->Test(parm0, parm1, parm2, parm3);
}

qboolean SV_BotLibReady(void)
{
    return (qboolean)botlib_initialized;
}


int SV_BotAllocateClient(void)
{
    int       i;
    client_t *cl;

    for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
        if (cl->state == CS_FREE) {
            break;
        }
    }

    if (i == sv_maxclients->integer) {
        return -1;
    }

    cl->gentity = SV_GentityNum(i);
    cl->gentity->s.number = i;
    cl->state = CS_ACTIVE;
    cl->lastPacketTime = svs.time;
    cl->netchan.remoteAddress.type = NA_BOT;
    cl->rate = 16384;

    return i;
}

void SV_BotFreeClient(int clientNum)
{
    client_t *cl;

    if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
        Com_Error(ERR_DROP, "SV_BotFreeClient: bad clientNum: %i", clientNum);
    }
    cl = &svs.clients[clientNum];
    cl->state = CS_FREE;
    cl->name[0] = 0;
    if (cl->gentity) {
        cl->gentity->svflags &= ~SVF_BOT;
    }
}


int SV_BotGetConsoleMessage(int client, char *buf, int size)
{
    client_t *cl;
    int       index;

    cl = &svs.clients[client];
    cl->lastPacketTime = svs.time;

    if (cl->reliableAcknowledge == cl->reliableSequence) {
        return qfalse;
    }

    cl->reliableAcknowledge++;
    index = cl->reliableAcknowledge & (MAX_RELIABLE_COMMANDS - 1);

    if (!cl->reliableCommands[index][0]) {
        return qfalse;
    }

    Q_strncpyz(buf, cl->reliableCommands[index], size);
    return qtrue;
}

int SV_BotGetSnapshotEntity(int client, int sequence)
{
    client_t         *cl;
    clientSnapshot_t *frame;

    cl = &svs.clients[client];
    frame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];
    if (sequence < 0 || sequence >= frame->num_entities) {
        return -1;
    }
    return svs.snapshotEntities[(frame->first_entity + sequence) % svs.numSnapshotEntities].number;
}

void SV_BotUserCommand(int client, usercmd_t *cmd)
{
    SV_ClientThink(&svs.clients[client], cmd);
}
