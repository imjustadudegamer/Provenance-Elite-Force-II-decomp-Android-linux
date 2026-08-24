

#include "cg_local.h"
#include "cg_commands.h"
#include "../corepp/tiki.h"

#define R120_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define R120_PROBE_PRINT                 cgi.Printf
#define R131_PROBE_GET(name, def, flags) cgi.Cvar_Get(name, def, flags)
#define R131_PROBE_PRINT                 cgi.Printf
#include "../qcommon/qa_r120.h"
#include "../qcommon/qa_r131.h"


cvar_t *cg_showtempmodels;
cvar_t *cg_detail;
cvar_t *cg_effectdetail;

cvar_t *cg_physics_fps;

extern refEntity_t *current_entity;
extern int          current_entity_number;
extern centity_t   *current_centity;
extern float        current_scale;
extern int          current_tiki;

extern Event EV_Client_Swipe;
extern Event EV_Client_SwipeOn;
extern Event EV_Client_SwipeOff;


static float CG_RandomRange(float min, float max)
{
    return min + (max - min) * random();
}

static float CG_CRandomRange(float min, float max)
{
    float sign = crandom();

    if (sign >= 0.0f) {
        return min + (max - min) * crandom();
    }

    return (max - min) * crandom() - min;
}

static float CG_Grandom(float average, float deviation)
{
    double    v1, v2, s;
    float     x1;
    static float x2 = 0;
    static qboolean toggle = qfalse;

    if (toggle) {
        toggle = qfalse;
        return x2;
    }

    do {
        v1 = -1.0 + 2.0 * random();
        v2 = -1.0 + 2.0 * random();
        s  = v1 * v1 + v2 * v2;
    } while (s >= 1.0 || s == 0);

    s  = sqrt(-2.0 * log(s) / s);
    x1 = (float)(v1 * s);
    x2 = (float)(v2 * s);
    toggle = qtrue;

    return average + deviation * x1;
}

static float CG_Erandom(float mean)
{
    float r;

    do {
        r = random();
    } while (r == 0.0f);

    return -mean * log(r);
}

float CGResolveRandomNumber(randtype_t type, float base, float amplitude)
{
    switch (type) {
    case RANDOM:
        return base * random();
    case CRANDOM:
        return base * crandom();
    case RANDOMRANGE:
        return CG_RandomRange(base, amplitude);
    case CRANDOMRANGE:
        return CG_CRandomRange(base, amplitude);
    case GRANDOM:
        return CG_Grandom(base, amplitude);
    case ERANDOM:
        return CG_Erandom(base);
    case NOT_RANDOM:
    default:
        // nothing else; the amplitude is never read.  Every retail NOT_RANDOM record
        return base;
    }
}

ctempmodel_t *ClientGameCommandManager::AllocateTempModel(void)
{
    ctempmodel_t *p;

    p = m_free_tempmodels;
    if (!p) {
        return NULL;
    }

    m_free_tempmodels = m_free_tempmodels->next;

    p->next                        = m_active_tempmodels.next;
    p->prev                        = &m_active_tempmodels;
    m_active_tempmodels.next->prev = p;
    m_active_tempmodels.next       = p;

    return p;
}

void ClientGameCommandManager::FreeTempModel(ctempmodel_t *p)
{
    if (!p->prev) {
        cgi.Error(ERR_DROP, "CCM::FreeTempModel: not active");
    }

    p->deleteTBC_Controller();

    RemoveClientEntity(p->number, p->cgd.tiki, NULL, p);

    if (m_active_tempmodels.next == p) {
        m_active_tempmodels.next = p->next;
    }

    p->prev->next = p->next;
    p->next->prev = p->prev;

    p->next           = m_free_tempmodels;
    m_free_tempmodels = p;

    // SpawnTempModel @0x00017080 line 460 and is NOT always a heap spawnthing_t.  The manager
}

void ClientGameCommandManager::FreeAllTempModels(void)
{
    ctempmodel_t *p, *next;

    p = m_active_tempmodels.prev;
    for (; p != &m_active_tempmodels; p = next) {
        next = p->prev;
        FreeTempModel(p);
    }
}


void ClientGameCommandManager::FreeSpawnthing(spawnthing_t *sp)
{
    ctempmodel_t *model;
    ctempmodel_t *prev;

    // leaves NULL (cl_main.cpp: `ri.CG_EndTiki` is never wired -- retail's refimport_t has no
    for (model = m_active_tempmodels.prev; model != &m_active_tempmodels; model = prev) {
        prev = model->prev;

        if (model->m_spawnthing == sp) {
            FreeTempModel(model);
        }
    }

    m_emitters.RemoveObject(sp);

    if (sp == m_spawnthing) {
        m_spawnthing = NULL;
    }

    delete sp;
}

void ClientGameCommandManager::ResetTempModels(void)
{
    ctempmodel_t *p, *next;

    p = m_active_tempmodels.prev;
    for (; p != &m_active_tempmodels; p = next) {
        next = p->prev;
        FreeTempModel(p);
    }
}

static int lastTempModelFrameTime = 0;
int        next_tempmodel_warning = 0;

void CG_ResetTempModels(void)
{
    commandManager.ResetTempModels();
    lastTempModelFrameTime = cg.time;
}

void ClientGameCommandManager::InitializeTempModels(void)
{
    int i;
    int numtempmodels = MAX_TEMPMODELS;

    m_active_tempmodels.next = &m_active_tempmodels;
    m_active_tempmodels.prev = &m_active_tempmodels;

    m_free_tempmodels = &m_tempmodels[0];

    for (i = 0; i < numtempmodels - 1; i++) {
        m_tempmodels[i].next = &m_tempmodels[i + 1];
    }
    m_tempmodels[numtempmodels - 1].next = NULL;
}

void ClientGameCommandManager::InitializeTempModelCvars(void)
{
    // Cvar_CheckRange clamps are gone -- the retail cgame does not import Cvar_CheckRange
    cg_showtempmodels = cgi.Cvar_Get("cg_showtempmodels", "0", 0);
    cg_detail         = cgi.Cvar_Get("detail", "1", CVAR_ARCHIVE);
    cg_effectdetail   = cgi.Cvar_Get("cg_effectdetail", "1", CVAR_ARCHIVE);
    cg_physics_fps    = cgi.Cvar_Get("cg_physics_fps", "10", CVAR_ARCHIVE);
}

void ClientGameCommandManager::AnimateTempModel(ctempmodel_t *p, Vector origin, refEntity_t *newEnt)
{
    int   numframes;
    int   deltatime;
    int   frametime;


    if (p->cgd.tiki < 0) {
        return;
    }

    frametime = 1000.0f * cgi.TIKI_Frame_Time(p->cgd.tiki, p->ent.anim, 0);
    deltatime = cg.time - p->lastAnimTime;
    numframes = cgi.Anim_NumFrames(p->cgd.tiki, p->ent.anim);

    if (!p->addedOnce) {
        {
            static int r133_dfx_n;
            const char *r133_tn = cgi.TIKI_Name(p->cgd.tiki);
            if (R120_On() && r133_tn && Q_stristr(r133_tn, "fx") && R120_Throttle(&r133_dfx_n, 32, 64)) {
                R120_PROBE("DFX:fxentry", "tiki=%d '%s' anim=%d nf=%d ft=%d\n",
                           p->cgd.tiki, r133_tn, p->ent.anim, numframes, frametime);
            }
        }
        CG_ProcessEntityCommands(p->cgd.tiki, TIKI_FRAME_ENTRY, p->ent.anim, -1, &p->ent, NULL);
    }

    if (numframes < 2) {
        return;
    }

    while (deltatime >= frametime) {
        deltatime -= frametime;
        p->lastAnimTime += frametime;
        p->ent.oldanim  = p->ent.anim;
        p->ent.oldframe = p->ent.frame;
        p->ent.frame    = (p->ent.frame + 1) % numframes;
        CG_ProcessEntityCommands(p->cgd.tiki, p->ent.frame, p->ent.anim, -1, &p->ent, NULL);
    }

    p->ent.framelerp = 1.0f - (float)deltatime / (float)frametime;
}

void ClientGameCommandManager::UpdateSwarm(ctempmodel_t *p)
{
    if (p->cgd.swarmfreq == 0) {
        return;
    }

    if (!(rand() % p->cgd.swarmfreq)) {
        p->cgd.velocity.x = crandom() * p->cgd.swarmmaxspeed;
        p->cgd.velocity.y = crandom() * p->cgd.swarmmaxspeed;
        p->cgd.velocity.z = crandom() * p->cgd.swarmmaxspeed;
    }

    if (p->cgd.origin.x < p->cgd.parentOrigin.x) {
        p->cgd.velocity.x += p->cgd.swarmdelta;
    } else {
        p->cgd.velocity.x -= p->cgd.swarmdelta;
    }

    if (p->cgd.origin.y < p->cgd.parentOrigin.y) {
        p->cgd.velocity.y += p->cgd.swarmdelta;
    } else {
        p->cgd.velocity.y -= p->cgd.swarmdelta;
    }

    if (p->cgd.origin.z < p->cgd.parentOrigin.z) {
        p->cgd.velocity.z += p->cgd.swarmdelta;
    } else {
        p->cgd.velocity.z -= p->cgd.swarmdelta;
    }
}

qboolean ClientGameCommandManager::TempModelRealtimeEffects(
    ctempmodel_t *p,
    float         ftime,
    float         ftimeSquared,
    float         scale
)
{
    float  fade, fadein;
    float  dtime;
    byte   tempColor[4];
    Vector parentAngles(0, 0, 0);
    Vector tempVec(0, 0, 0);
    vec3_t localAxis[3];
    vec3_t invLocalAxis[3];
    vec3_t parentAxis[3];

    // ⚠️ Retail reads BOTH values off m_spawnthing, not off p->cgd, even though they are cgd
    // the null test: retail NEVER stores NULL into m_spawnthing (no `stw rX, 1558360(r3)` with
    if (m_spawnthing && m_spawnthing->cgd.drag != 0.0f) {
        if ((float)p->aliveTime >= m_spawnthing->cgd.dragDelay) {
            tempVec = p->cgd.velocity * m_spawnthing->cgd.drag;

            p->cgd.velocity -= tempVec * ftime;
        }
    }

    if (p->cgd.flags & (T_PARENTLINK | T_HARDLINK)) {
        if (p->cgd.parent != ENTITYNUM_NONE) {
            if (p->cgd.parent >= 1024) {
                ctempmodel_t *ct;

                if (p->cgd.parent < 7777) {
                    return qfalse;
                }

                for (ct = m_active_tempmodels.next; ct != &m_active_tempmodels; ct = ct->next) {
                    if (ct->number == p->cgd.parent) {
                        break;
                    }
                }

                if (ct == &m_active_tempmodels) {
                    return qfalse;
                }

                parentAngles = ct->cgd.angles;
            } else {
                centity_t   *pc;
                refEntity_t *e;

                pc = &cg_entities[p->cgd.parent];
                if (!pc->currentValid) {
                    return qfalse;
                }

                e = cgi.R_GetRenderEntity(p->cgd.parent);
                if (!e) {
                    return qfalse;
                }

                AxisToAngles(e->axis, parentAngles);
            }

            AnglesToAxis(p->cgd.parentLinkAngles, localAxis);
            TransposeMatrix(localAxis, invLocalAxis);
            AnglesToAxis(parentAngles, parentAxis);

            MatrixTransformVector(p->cgd.velocity, invLocalAxis, tempVec);
            MatrixTransformVector(tempVec, parentAxis, p->cgd.velocity);
            MatrixTransformVector(p->cgd.origin, invLocalAxis, tempVec);
            MatrixTransformVector(tempVec, parentAxis, p->cgd.origin);
            MatrixTransformVector(p->cgd.oldorigin, invLocalAxis, tempVec);
            MatrixTransformVector(tempVec, parentAxis, p->cgd.oldorigin);

            p->cgd.parentLinkAngles = parentAngles;
        }
    }

    if ((p->cgd.flags & (T_FADE | T_SCALEUPDOWN)) || (p->cgd.flags2 & T2_FADE)) {
        fade = 1.0f - (float)(p->aliveTime - p->cgd.fadedelay) / (float)(p->cgd.life - p->cgd.fadedelay);

        if (fade > 1) {
            fade = 1;
        }
        if (fade < 0) {
            fade = 0;
        }
    } else {
        fade = 1.0f;
    }

    dtime = (cg.time - p->cgd.createTime);

    if ((p->cgd.flags & T_FADEIN) || (p->cgd.flags2 & T2_FADE)) {
        fadein = dtime / (float)p->cgd.fadeintime;
    } else {
        fadein = 0;
    }

    dtime *= 0.001f;

    if (p->aliveTime >= p->cgd.scaleDelay) {
        if (ftime && p->cgd.scaleRate) {
            p->ent.scale += p->ent.scale * (p->cgd.scaleRate * ftime);
        }
        if (ftime && p->cgd.linearScaleRate) {
            p->ent.scale += p->cgd.linearScaleRate * ftime;
        }

        if (p->cgd.flags & T_SCALEUPDOWN) {
        p->ent.scale = p->cgd.scale * sin((fade)*M_PI);

        if (p->ent.scale < p->cgd.scalemin) {
            p->ent.scale = p->cgd.scalemin;
        }
        if (p->ent.scale > p->cgd.scalemax) {
            p->ent.scale = p->cgd.scalemax;
        }
        }
    }

    // 0x4000000 (T_ALIGNONCE) -- and never against 0x2000.  T_DLIGHT is MOHAA's dynamic-light
    // flag and retail's cgame never reads it here, so the arm was dead in retail and live

    if (p->cgd.lightstyle >= 0) {
        int   i;
        float color[4];
        CG_LightStyleColor(p->cgd.lightstyle, dtime * 1000, color);
        for (i = 0; i < 4; i++) {
            tempColor[i] = (byte)(color[i] * 255.0f);
        }
    } else {
        tempColor[0] = (int)(p->cgd.color[0] * 255.0f);
        tempColor[1] = (int)(p->cgd.color[1] * 255.0f);
        tempColor[2] = (int)(p->cgd.color[2] * 255.0f);
        tempColor[3] = (int)(p->cgd.color[3] * 255.0f);
    }

    // Alpha is not part of the range: retail writes only the three colour channels, leaving
    if (p->cgd.hasColorRange) {
        int i;
        int duration = p->cgd.colorRangeTimeMs ? p->cgd.colorRangeTimeMs : p->cgd.life;

        for (i = 0; i < 3; i++) {
            if (!duration || p->aliveTime >= duration) {
                tempColor[i] = p->cgd.colorRangeMax[i];
            } else {
                float frac = (float)p->aliveTime / (float)duration;

                tempColor[i] =
                    (byte)((float)(p->cgd.colorRangeMax[i] - p->cgd.colorRangeMin[i]) * frac
                           + (float)p->cgd.colorRangeMin[i]);
            }
        }
    }

    if (p->cgd.flags & T_TWINKLE) {
        if (cg.time > p->twinkleTime) {
            if (p->cgd.flags & T_TWINKLE_OFF) {
                p->cgd.flags &= ~T_TWINKLE_OFF;
                p->twinkleTime = cg.time + p->cgd.min_twinkletimeon + random() * p->cgd.max_twinkletimeon;
            } else {
                p->cgd.flags |= T_TWINKLE_OFF;
                p->twinkleTime = cg.time + p->cgd.min_twinkletimeoff + random() * p->cgd.max_twinkletimeoff;
            }
        }

        if (p->cgd.flags & T_TWINKLE_OFF) {
            memset(tempColor, 0, sizeof(tempColor));
        }
    }

    // 0x4000 (0x14FA0), 0x1000 (0x152B4), 0x4020 (0x152DC) and 0x200 (0x154A4) -- never
    if (p->cgd.flags & T_FADEIN && (fadein < 1)) 
    {
        int i;
        for (i = 0; i < 4; i++) {
            p->ent.shaderRGBA[i] = (int)((float)tempColor[i] * (fadein * p->cgd.alpha));
        }
    } else if (p->cgd.flags2 & T2_DOFADE && (fadein < 1)) 
    {
        p->ent.shaderRGBA[3] = (int)((float)tempColor[3] * (fadein * p->cgd.alpha));
    } else if (p->cgd.flags & T_FADE) 
    {
        int i;
        for (i = 0; i < 4; i++) {
            p->ent.shaderRGBA[i] = (int)((float)tempColor[i] * (fade * p->cgd.alpha));
        }
    } else if (p->cgd.flags2 & T2_FADE) 
    {
        p->ent.shaderRGBA[3] = (int)((float)tempColor[3] * (fade * p->cgd.alpha));
    } else {
        int i;
        for (i = 0; i < 4; i++) {
            p->ent.shaderRGBA[i] = (int)((float)tempColor[i] * p->cgd.alpha);
        }
    }

    if (p->cgd.flags & T_FLICKERALPHA) {
        float random = random();
        int   i;

        if (p->cgd.flags & (T_FADE | T_FADEIN)) {
            for (i = 0; i < 4; i++) {
                p->ent.shaderRGBA[i] = (int)((float)p->ent.shaderRGBA[i] * random);
            }
        } else {
            for (i = 0; i < 4; i++) {
                float colorByte = (float)(int)(p->cgd.color[i] * 255.0f);

                p->ent.shaderRGBA[i] = (int)(colorByte * random);
            }
        }
    }

    if (fade <= 0 && p->addedOnce) {
        return false;
    }

    if ((p->ent.scale <= 0 && p->addedOnce) && !(p->cgd.flags & T_SCALEUPDOWN)) {
        return false;
    }

    if (p->cgd.flags & T_SWARM) {
        UpdateSwarm(p);
    }

    return true;
}

void ClientGameCommandManager::OtherTempModelEffects(ctempmodel_t *p, Vector origin, refEntity_t *newEnt)
{
    vec3_t axis[3];

    if (p->number != -1) {
        refEntity_t *old_entity;
        int         old_tiki;
        int          oldnum;
        float        oldscale;

        AnglesToAxis(p->cgd.angles, axis);

        old_entity = current_entity;
        old_tiki   = current_tiki;
        oldnum     = current_entity_number;
        oldscale   = current_scale;

        current_scale         = newEnt->scale;
        current_entity        = newEnt;
        current_tiki          = p->cgd.tiki;
        current_entity_number = p->number;

        UpdateEmitter(p->cgd.tiki, axis, p->number, -1, p->cgd.parent, origin);

        if (p->cgd.flags2 & T2_TRAIL) {
            Event *ev = new Event(EV_Client_Swipe);
            ev->AddVector(origin);
            commandManager.ProcessEvent(ev);
        }

        current_entity_number = oldnum;
        current_tiki          = old_tiki;
        current_entity        = old_entity;
        current_scale         = oldscale;
    }

}

qboolean ClientGameCommandManager::TempModelPhysics(ctempmodel_t *p, float ftime, float ftimeSquared, float scale)
{
    Vector  parentOrigin(0, 0, 0);
    Vector  parentAngles(0, 0, 0);
    Vector  tempangles;
    trace_t trace;
    float   dot;

    VectorCopy(p->ent.origin, p->lastEnt.origin);
    AxisCopy(p->ent.axis, p->lastEnt.axis);

    p->cgd.oldorigin = p->cgd.origin;

    if (p->tbcController) {
        p->tbcController->Update(p);
    }

    // never for 0x200 (T_SWARM).  MOHAA's `if (T_SWARM) origin += velocity * ftime * scale`
    if (p->cgd.flags2 & (T2_MOVE | T2_ACCEL)) {
        p->cgd.origin += p->cgd.velocity * ftime * scale + p->cgd.accel * ftimeSquared;

        // r221-beamstep -- THE GATE WAS UNREACHABLE, SO THE PROBE NEVER FIRED.  It required
        {
            static int r196_beamstep_reports = 0;
            const char *r196_step_model = p->modelname.c_str();

            if (r196_step_model && r196_step_model[0]
                && (Q_stristr(r196_step_model, "fx")
                    || Q_stristr(r196_step_model, "spark")
                    || Q_stristr(r196_step_model, "beam")
                    || Q_stristr(r196_step_model, "cylinder")
                    || Q_stristr(r196_step_model, "lines"))) {
                Vector r196_step = p->cgd.velocity * ftime * scale + p->cgd.accel * ftimeSquared;

                if (r196_beamstep_reports < 24) {
                    r196_beamstep_reports++;
                    cgi.Printf(
                        "[R196:beamstep] n=%d model='%s' step=(%.2f,%.2f,%.2f) ftime=%.5f "
                        "ftimeSq=%.6f scale=%.4f vel=(%.2f,%.2f,%.2f) accel=(%.2f,%.2f,%.2f) "
                        "life=%d alive=%.3f physRate=%d reType=%d org=(%.2f,%.2f,%.2f) "
                        "oldorg=(%.2f,%.2f,%.2f)\n",
                        r196_beamstep_reports, r196_step_model,
                        r196_step[0], r196_step[1], r196_step[2],
                        ftime, ftimeSquared, scale,
                        p->cgd.velocity[0], p->cgd.velocity[1], p->cgd.velocity[2],
                        p->cgd.accel[0], p->cgd.accel[1], p->cgd.accel[2],
                        (int)p->cgd.life, p->aliveTime, p->cgd.physicsRate, p->ent.reType,
                        p->cgd.origin[0], p->cgd.origin[1], p->cgd.origin[2],
                        p->cgd.oldorigin[0], p->cgd.oldorigin[1], p->cgd.oldorigin[2]
                    );
                } else if (r196_beamstep_reports == 24) {
                    r196_beamstep_reports++;
                    cgi.Printf("[R196:beamstep] capped at 24 rows for this run; no further "
                               "rows\n");
                }
            }
        }
    }

    if ((p->cgd.flags & (T_PARENTLINK | T_HARDLINK)) && (p->cgd.parent != ENTITYNUM_NONE)) {
        centity_t   *pc;
        refEntity_t *e;

        pc = &cg_entities[p->cgd.parent];
        if (!pc->currentValid) {
            return false;
        }

        e = cgi.R_GetRenderEntity(p->cgd.parent);
        if (!e) {
            return false;
        }

        parentOrigin = e->origin;
        vectoangles(e->axis[0], parentAngles);
    }
    // retail TempModelPhysics never writes cgd.parentOrigin (cgd+116), and UpdateSwarm is

    if (p->cgd.flags & T_ALIGN) {
        p->cgd.angles = p->cgd.velocity.toAngles();
        parentAngles  = vec_zero;
    }

    if (p->cgd.flags & T_RANDOMROLL) {
        p->cgd.angles[ROLL] = random() * 360;
    }

    if (p->cgd.flags2 & T2_AMOVE) {
        p->cgd.angles = p->cgd.angles + (ftime * p->cgd.avelocity);
    }

    p->cgd.angles[0] = AngleMod(p->cgd.angles[0]);
    p->cgd.angles[1] = AngleMod(p->cgd.angles[1]);
    p->cgd.angles[2] = AngleMod(p->cgd.angles[2]);

    if ((p->cgd.flags & (T_ALIGN | T_RANDOMROLL | T_PARENTLINK | T_HARDLINK | T_ANGLES))
        || (p->cgd.flags2 & T2_AMOVE)) {
        tempangles = p->cgd.angles + parentAngles;

        AnglesToAxis(tempangles, p->ent.axis);
    }

    if (p->cgd.flags & T_COLLISION) {
        CG_Trace(
            &trace,
            p->cgd.oldorigin,
            vec3_origin,
            vec3_origin,
            p->cgd.origin,
            -1,
            p->cgd.collisionmask,
            qfalse,
            qfalse,
            "Collision"
        );
    } else {
        trace.fraction = 1.0;
    }

    if (trace.fraction == 1.0) {
        if (p->cgd.flags2 & T2_ACCEL) {
            p->cgd.velocity = p->cgd.velocity + ftime * p->cgd.accel;
        }


    } else {
        Vector normal;

        p->cgd.origin = trace.endpos;

        if (p->m_spawnthing && p->m_spawnthing->m_armedEffectMap) {
            SpawnEffectMapOnImpact(p->m_spawnthing, &trace);
        }

        if ((p->cgd.flags2 & T2_BOUNCE_DECAL) && (p->cgd.bouncecount < p->cgd.maxbouncecount)) {
            qhandle_t shader = cgi.R_RegisterShader(p->cgd.shadername);

            CG_ImpactMarkSimple(
                shader,
                trace.endpos,
                trace.plane.normal,
                p->cgd.decal_orientation,
                p->cgd.decal_radius,
                p->cgd.color[0],
                p->cgd.color[1],
                p->cgd.color[2],
                p->cgd.alpha,
                p->cgd.flags & T_FADE,
                p->cgd.flags2 & T2_TEMPORARY_DECAL,
                qtrue,
                p->cgd.flags & T_FADEIN
            );

            p->cgd.bouncecount++;
        }

        if (p->cgd.flags & T_DIETOUCH) {
            return false;
        }

        normal = trace.plane.normal;

        if (p->cgd.flags2 & T2_ACCEL) {
            p->cgd.velocity = p->cgd.velocity + ftime * trace.fraction * p->cgd.accel;
        }

        dot             = p->cgd.velocity * normal;
        p->cgd.velocity = p->cgd.velocity + ((-2 * dot) * normal);
        p->cgd.velocity *= p->cgd.bouncefactor;
        p->cgd.avelocity *= -p->cgd.bouncefactor;

        if (trace.plane.normal[2] > 0 && p->cgd.velocity[2] < 45) {
            p->cgd.velocity  = Vector(0, 0, 0);
            p->cgd.avelocity = Vector(0, 0, 0);
            p->cgd.flags &= ~T_WAVE;
        } else {
            if (p->cgd.flags & T_BOUNCESOUNDONCE) {
                vec3_t org;
                VectorCopy(p->cgd.origin, org);
                PlaySound(p->cgd.bouncesound, org);
                p->cgd.flags &= ~(T_BOUNCESOUNDONCE | T_BOUNCESOUND);
            } else if ((p->cgd.flags & T_BOUNCESOUND) && (p->next_bouncesound_time < cg.time)) {
                vec3_t org;
                VectorCopy(p->cgd.origin, org);
                PlaySound(p->cgd.bouncesound, org);
                p->next_bouncesound_time = cg.time + p->cgd.bouncesound_delay;
            }
        }
    }

    VectorCopy(p->cgd.origin, p->ent.origin);

    if (p->cgd.flags & T_WAVE) {
        float s = sin((double)(cg.time - p->cgd.createTime));
        int   iAxis;

        for (iAxis = 0; iAxis < 3; iAxis++) {
            p->ent.origin[iAxis] += p->wavyDir[iAxis] * s * p->cgd.wavyDistance;
        }
    }

    if (p->cgd.flags & (T_PARENTLINK | T_HARDLINK)) {
        VectorAdd(p->ent.origin, parentOrigin, p->ent.origin);

        {
            static int l1_flash;

            if (R131_On() && R131_Throttle(&l1_flash, 8, 64)) {
                R131_A1_PROBE(
                    "flash:phys",
                    "parent=%d porg=(%.1f,%.1f,%.1f) cgdorg=(%.1f,%.1f,%.1f) org=(%.1f,%.1f,%.1f) "
                    "sh='%s' life=%d age=%d scale=%.4f pla=(%.1f,%.1f,%.1f)\n",
                    p->cgd.parent,
                    parentOrigin[0], parentOrigin[1], parentOrigin[2],
                    p->cgd.origin[0], p->cgd.origin[1], p->cgd.origin[2],
                    p->ent.origin[0], p->ent.origin[1], p->ent.origin[2],
                    p->cgd.shadername.c_str(),
                    p->cgd.life,
                    cg.time - p->cgd.createTime,
                    p->ent.scale,
                    p->cgd.parentLinkAngles[0],
                    p->cgd.parentLinkAngles[1],
                    p->cgd.parentLinkAngles[2]);
            }
        }
    }

    if (!p->lastEntValid) {
        p->lastEnt = p->ent;
        VectorCopy(p->cgd.oldorigin, p->lastEnt.origin);
        p->lastEntValid = true;
    }

    return true;
}

qboolean ClientGameCommandManager::LerpTempModel(refEntity_t *newEnt, ctempmodel_t *p, float frac)
{
    int i, j;

    if (p->cgd.flags & (T_PARENTLINK | T_HARDLINK)) {
        centity_t *pc;
        Vector     parentOrigin;

        for (i = 0; i < 3; i++) {
            newEnt->origin[i] = p->cgd.oldorigin[i] + frac * (p->cgd.origin[i] - p->cgd.oldorigin[i]);
        }

        pc = &cg_entities[p->cgd.parent];

        if (pc->currentValid) {
            refEntity_t *e;

            e = cgi.R_GetRenderEntity(p->cgd.parent);

            if (!e) {
                return false;
            }

            parentOrigin = e->origin;
        } else {
            return false;
        }

        VectorAdd(newEnt->origin, parentOrigin, newEnt->origin);
    } else {
        if (p->cgd.flags2 & (T2_MOVE | T2_ACCEL)) {
            for (i = 0; i < 3; i++) {
                newEnt->origin[i] = p->lastEnt.origin[i] + frac * (p->ent.origin[i] - p->lastEnt.origin[i]);
            }
        }
    }

    if (p->cgd.flags2 & T2_PARALLEL) {
        Vector v1 = p->cgd.origin - cg.refdef.vieworg;
        vectoangles(v1, p->cgd.angles);
        AnglesToAxis(p->cgd.angles, newEnt->axis);
    } else if ((p->cgd.flags & (T_ALIGN | T_RANDOMROLL | T_PARENTLINK | T_HARDLINK)) || (p->cgd.flags2 & T2_AMOVE)) {
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                newEnt->axis[i][j] = p->lastEnt.axis[i][j] + frac * (p->ent.axis[i][j] - p->lastEnt.axis[i][j]);
            }
        }
    }

    return true;
}

void CG_AddTempModels(void)
{
    commandManager.AddTempModels();
}

#define TOO_MUCH_TIME_PASSED 500

static int r133_beamfx_cyl_live = 0;

void ClientGameCommandManager::AddTempModels(void)
{
    ctempmodel_t *p, *next;
    int           count = 0; 
    int           frameTime;
    float         effectTime;
    int           mstime       = 0;
    float         ftime        = 0;
    float         scale        = 1.0f;
    float         lerpfrac     = 0;
    int           physics_rate = 0;
    qboolean      ret;
    refEntity_t   newEnt;
    int          old_tiki;
    int           old_num;
    refEntity_t  *old_ent;

    if (lastTempModelFrameTime
        && ((cg.time < lastTempModelFrameTime) || (cg.time - lastTempModelFrameTime > TOO_MUCH_TIME_PASSED))) {
        p = m_active_tempmodels.prev;
        for (; p != &m_active_tempmodels; p = next) {
            next               = p->prev;
            p->lastPhysicsTime = cg.time;
        }
        lastTempModelFrameTime = cg.time;
        return;
    }

    if (lastTempModelFrameTime) {
        frameTime = cg.time - lastTempModelFrameTime;
    } else {
        frameTime = 0;
    }

    if (paused->integer) {
        lastTempModelFrameTime = 0;
    } else {
        lastTempModelFrameTime = cg.time;
    }

    memset(&newEnt, 0, sizeof(newEnt));
    effectTime = (float)frameTime / 1000.0f;

    if (current_entity) {
        scale = current_entity->scale;
    }

    old_ent  = current_entity;
    old_tiki = current_tiki;
    old_num  = current_entity_number;

    r133_beamfx_cyl_live = 0;

    p = m_active_tempmodels.prev;
    for (; p != &m_active_tempmodels; p = next) {
        next = p->prev;

        if (R120_On()) {
            const char *r133_cn = (p->cgd.tiki >= 0) ? cgi.TIKI_Name(p->cgd.tiki) : NULL;

            if (r133_cn && Q_stristr(r133_cn, "cylinder")) {
                r133_beamfx_cyl_live++;
            }
        }

        if ((p->cgd.flags & T_DETAIL) && !cg_detail->integer) {
            FreeTempModel(p);
            continue;
        }

        p->ent.tiki           = CG_TikiFromHandle(p->cgd.tiki);
        current_entity        = &p->ent;
        current_tiki          = p->cgd.tiki;
        current_entity_number = p->number;

        TempModelRealtimeEffects(p, effectTime, effectTime * effectTime, scale);

        if (p->lastPhysicsTime) {
            mstime = cg.time - p->lastPhysicsTime;

            physics_rate = 1000 / p->cgd.physicsRate; 

            if (mstime > physics_rate * 2) {
                mstime = physics_rate;
            }

            if ((mstime >= physics_rate) || (p->cgd.flags2 & T2_PHYSICS_EVERYFRAME)) {
                ftime = mstime / 1000.0f;
                ret   = TempModelPhysics(p, ftime, ftime * ftime, scale);

                if (!ret) {
                    FreeTempModel(p);
                    continue;
                }

                p->lastPhysicsTime = cg.time;
            }
        }

        lerpfrac = (float)(cg.time - p->lastPhysicsTime) / (float)physics_rate;
        if (lerpfrac > 1 || (p->cgd.flags2 & T2_PHYSICS_EVERYFRAME)) {
            lerpfrac = 1;
        }
        if (lerpfrac < 0) {
            lerpfrac = 0;
        }

        p->aliveTime += frameTime;

        if (p->aliveTime >= p->cgd.life && p->addedOnce) {
            FreeTempModel(p);
            continue;
        }

        if (!p->lastEntValid) {
            float t;
            t = physics_rate / 1000.0f;

            ret = TempModelPhysics(p, t, t * t, scale);
            if (!ret) {
                FreeTempModel(p);
                continue;
            }

            lerpfrac           = 0;
            p->lastPhysicsTime = cg.time;
        }


        newEnt.scale = p->ent.scale;
        memcpy(newEnt.shaderRGBA, p->ent.shaderRGBA, 4);
        AxisCopy(p->ent.axis, newEnt.axis);
        VectorCopy(p->ent.origin, newEnt.origin);

        if (!LerpTempModel(&newEnt, p, lerpfrac)) {
            FreeTempModel(p);
            continue;
        }


        newEnt.renderfx   = p->ent.renderfx;
        newEnt.hModel     = p->ent.hModel;
        newEnt.reType     = p->ent.reType;
        newEnt.shaderTime = p->ent.shaderTime;

        newEnt.crossblend_lerp = -1.0f;
        newEnt.legsLive     = p->ent.legsLive;

        newEnt.actionWeight = 1.0;
        newEnt.entityNumber = ENTITYNUM_NONE;
        newEnt.tiki         = p->ent.tiki;

        AnimateTempModel(p, newEnt.origin, &newEnt);

        {
            static int r133_beamfx_add_n;
            const char *r133_tn = (p->cgd.tiki >= 0) ? cgi.TIKI_Name(p->cgd.tiki) : NULL;

            if (R120_On() && r133_tn && Q_stristr(r133_tn, "cylinder") && R120_Throttle(&r133_beamfx_add_n, 6, 256)) {
                R133_PROBE(
                    "BEAMFX:tm-add",
                    "tiki='%s' scale=%.4f alive=%d frame=%d oldframe=%d anim=%d "
                    "org=(%.1f,%.1f,%.1f)\n",
                    r133_tn,
                    newEnt.scale,
                    p->aliveTime,
                    newEnt.frame,
                    newEnt.oldframe,
                    newEnt.anim,
                    newEnt.origin[0],
                    newEnt.origin[1],
                    newEnt.origin[2]
                );
            }
        }

        // field retail does not have.
        newEnt.anim      = p->ent.anim;
        newEnt.oldanim   = p->ent.oldanim;
        newEnt.oldframe  = p->ent.oldframe;
        newEnt.frame     = p->ent.frame;
        newEnt.framelerp = p->ent.framelerp;

        OtherTempModelEffects(p, newEnt.origin, &newEnt);

        VectorCopy(newEnt.origin, newEnt.lightingOrigin);
        newEnt.radius = 4.0;

        if (p->cgd.flags & T_DLIGHT) {
            cgi.R_AddLightToScene(
                p->cgd.origin,
                p->cgd.lightIntensity * scale,
                p->cgd.color[0],
                p->cgd.color[1],
                p->cgd.color[2],
                p->cgd.lightType
            );
        } else if (p->ent.reType == RT_SPRITE) {
            vec3_t vTestAngles;
            cgi.R_AddRefSpriteToScene(&newEnt); 
            MatrixToEulerAngles(newEnt.axis, vTestAngles);
        } else if (!p->hide) {
            cgi.R_AddRefEntityToScene(&newEnt); 
        }

        {
            static int r145_tmpsub_n;
            if (R120_On() && R120_Throttle(&r145_tmpsub_n, 24, 512)) {
                R120_PROBE(
                    "tmp:submit",
                    "ent=%d reType=%d model='%s' hModel=%d arm=%s org=(%.1f,%.1f,%.1f) "
                    "scale=%.4f alpha=%d customShader=%d hide=%d\n",
                    p->number,
                    (int)p->ent.reType,
                    p->modelname.c_str(),
                    p->ent.hModel,
                    (p->cgd.flags & T_DLIGHT) ? "dlight"
                        : (p->ent.reType == RT_SPRITE ? "sprite" : (p->hide ? "hidden" : "model")),
                    newEnt.origin[0], newEnt.origin[1], newEnt.origin[2],
                    newEnt.scale,
                    newEnt.shaderRGBA[3],
                    newEnt.customShader,
                    (int)p->hide
                );
            }
        }

        p->addedOnce = qtrue;

        count++;
    }

    current_entity        = old_ent;
    current_tiki          = old_tiki;
    current_entity_number = old_num;

    if (cg_showtempmodels->integer) {
        cgi.DPrintf("TC:%i\n", count);
    }

    if (R120_On() && r133_beamfx_cyl_live) {
        static int r133_beamfx_census_n;

        if (R120_Throttle(&r133_beamfx_census_n, 8, 128)) {
            R133_PROBE(
                "BEAMFX:census",
                "cyl_live=%d tm_added=%d cg.time=%d\n",
                r133_beamfx_cyl_live,
                count,
                cg.time
            );
        }
    }
}

void ClientGameCommandManager::SpawnTempModel(int count, spawnthing_t *sp)
{
    m_spawnthing = sp;
    SpawnTempModel(count, 0);
}

void ClientGameCommandManager::SpawnTempModel(int count, int timealive, spawnthing_t *sp)
{
    m_spawnthing = sp;
    SpawnTempModel(count, timealive);
}

void ClientGameCommandManager::SpawnTempModel(int mcount, int timealive)
{
    int           i;
    ctempmodel_t *p;
    refEntity_t   ent;
    int           count;
    float         current_entity_scale = 1.0f;
    Vector        newForward;
    Vector        delta;
    Vector        start;
    Vector        vForward, vLeft, vUp;
    float         fDist;

    if (m_spawnthing->tbcController) {
        m_spawnthing->tbcController->Update(m_spawnthing);
    }

    delta = m_spawnthing->cgd.origin - cg.refdef.vieworg;
    fDist = delta * delta * (cg.refdef.fov_x * cg.refdef.fov_x / 6400.0);
    if (fDist >= m_spawnthing->fMaxRangeSquared || fDist < m_spawnthing->fMinRangeSquared) {
        return;
    }

    if (current_entity) {
        current_entity_scale = current_entity->scale;
    } else {
        current_entity_scale = 1.0f;
    }

    if (current_scale > 0) {
        current_entity_scale *= current_scale;
    }

    if (mcount > 1) {
        mcount *= cg_effectdetail->value;
    }

    if (mcount < 1) {
        mcount = 1;
    }

    AxisCopy(m_spawnthing->aim_axis, m_spawnthing->axis);

    vForward = m_spawnthing->axis[0];
    vLeft    = m_spawnthing->axis[1];

    vUp = m_spawnthing->axis[2];

    for (count = 0; count < mcount; count++) {
        p = AllocateTempModel();

        if (!p) {
            if (cgi.Milliseconds() >= next_tempmodel_warning) {
                cgi.DPrintf("Out of tempmodels\n");
                next_tempmodel_warning = cgi.Milliseconds() + 1000;
            }
            return;
        }

        memset(&ent, 0, sizeof(refEntity_t));
        memset(&p->lastEnt, 0, sizeof(refEntity_t));

        // against cgd.flags -- never 0x100.  The whole branch was MOHAA residue and is gone;
        p->m_spawnthing = m_spawnthing;
        start           = m_spawnthing->cgd.origin;

        p->cgd = m_spawnthing->cgd;


        p->wavyDir[0] = crandom();
        p->wavyDir[1] = crandom();
        p->wavyDir[2] = 0.0f;

        newForward = vForward;

        if (m_spawnthing->cgd.flags & T_SPHERE) {
            do {
                newForward = Vector(crandom(), crandom(), crandom());
            } while (newForward * newForward > 1.0);
        } else if (m_spawnthing->cgd.flags & T_CIRCLE) {
            if (m_spawnthing->sphereRadius != 0) 
            {
                Vector dst;
                float angle;

                if (mcount == 1) {
                    angle = random() * 360;
                } else {
                    angle = ((float)count / (float)m_spawnthing->count) * 360; 
                }

                Vector end = Vector(vLeft) * m_spawnthing->sphereRadius * current_entity_scale;
                RotatePointAroundVector(dst, Vector(vForward), end, angle);

                VectorAdd(dst, m_spawnthing->cgd.origin, p->cgd.origin);
                newForward = p->cgd.origin - m_spawnthing->cgd.origin;
                newForward.normalize();
            }
        } else if (m_spawnthing->cgd.flags & T_INWARDSPHERE) {
            Vector dir, end;

            do {
                dir = Vector(crandom(), crandom(), crandom());
            } while (dir * dir > 1.0);

            end = m_spawnthing->cgd.origin + dir * m_spawnthing->sphereRadius * current_entity_scale;
            VectorCopy(end, p->cgd.origin);
            newForward = dir * -1;
        } else if (m_spawnthing->cgd.flags2 & T2_CONE) {
            float fHeight;
            float fRadius;
            float fAngle;
            float sina;
            float cosa;

            fHeight = random();
            fRadius = random();

            if (fHeight < fRadius) {
                float fTemp = fHeight;
                fHeight     = fRadius;
                fRadius     = fTemp;
            }

            fHeight *= m_spawnthing->coneHeight;
            fRadius = m_spawnthing->sphereRadius;

            fAngle = random() * M_PI * 2;
            sina   = sin(fAngle);
            cosa   = cos(fAngle);

            p->cgd.origin = start + vForward * fHeight;
            p->cgd.origin += vLeft * (cosa * fRadius);
            p->cgd.origin += vUp * (sina * fRadius);
        } else if (m_spawnthing->sphereRadius != 0) { 
            Vector dir, end;

            dir = Vector(crandom(), crandom(), crandom());

            dir.normalize();

            end = m_spawnthing->cgd.origin + dir * m_spawnthing->sphereRadius * current_entity_scale;
            VectorCopy(end, p->cgd.origin);
            newForward = dir;
        } else {
            VectorCopy(start, p->cgd.origin);
        }

        // tempmodel; retail SpawnTempModel @0x00017080 has no T_SWARM test and never writes

        p->tbcParticleCursor = 0;
        p->tbcNextCommandTime = CG_TBC_LITERAL_0;

        if (m_spawnthing->m_tbcManager
            && m_spawnthing->m_tbcManager->GetNumberOfParticleCommands() > 0) {
            p->tbcNextCommandTime = m_spawnthing->m_tbcManager->GetParticleFirstStartTime();
            p->createTBC_Controller();
        }

        for (i = 0; i < 3; i++) {
            p->cgd.origin[i] += CGResolveRandomNumber(
                m_spawnthing->offset_rand[i],
                m_spawnthing->origin_offset_base[i],
                m_spawnthing->origin_offset_amplitude[i]
                )
                * current_entity_scale;
        }

        {
            static int r196_beampart_reports = 0;
            const char *r196_em_model = m_spawnthing->GetModel();
            Vector      r196_skew;

            r196_skew = p->cgd.origin - m_spawnthing->cgd.origin;

            if (r196_em_model && r196_em_model[0]
                && (Q_stristr(r196_em_model, "fx")
                    || Q_stristr(r196_em_model, "spark")
                    || Q_stristr(r196_em_model, "beam")
                    || Q_stristr(r196_em_model, "cylinder")
                    || Q_stristr(r196_em_model, "lines"))
                && r196_beampart_reports < 24) {
                r196_beampart_reports++;
                cgi.Printf(
                    "[R196:beampart] n=%d em='%s' model='%s' skew=(%.1f,%.1f,%.1f) "
                    "emitorg=(%.1f,%.1f,%.1f) flags=0x%x flags2=0x%x sphereR=%.2f coneH=%.2f "
                    "vel=(%.2f,%.2f,%.2f) rvamp=(%.2f,%.2f,%.2f) life=%d physRate=%d scale=%.4f "
                    "hostscale=%.4f\n",
                    r196_beampart_reports,
                    m_spawnthing->emittername.c_str(),
                    r196_em_model,
                    r196_skew[0], r196_skew[1], r196_skew[2],
                    m_spawnthing->cgd.origin[0], m_spawnthing->cgd.origin[1],
                    m_spawnthing->cgd.origin[2],
                    (unsigned)m_spawnthing->cgd.flags, (unsigned)m_spawnthing->cgd.flags2,
                    m_spawnthing->sphereRadius, m_spawnthing->coneHeight,
                    p->cgd.velocity[0], p->cgd.velocity[1], p->cgd.velocity[2],
                    m_spawnthing->randvel_amplitude[0], m_spawnthing->randvel_amplitude[1],
                    m_spawnthing->randvel_amplitude[2],
                    (int)p->cgd.life, p->cgd.physicsRate, ent.scale,
                    current_entity ? current_entity->scale : -1.0f
                );
            }
        }

        p->cgd.oldorigin = p->cgd.origin;
        p->modelname     = m_spawnthing->GetModel();

        if (!(p->cgd.flags & T_DLIGHT) && !p->modelname.length()) {
            FreeTempModel(p);
            continue;
        }

        p->addedOnce    = qfalse;
        p->lastEntValid = qfalse;

        if (p->modelname.length()) {
            ent.hModel = cgi.R_RegisterModel(p->modelname.c_str());
            if (!ent.hModel) {
                FreeTempModel(p);
                continue;
            }
        }

        ent.shaderTime = cg.time / 1000.0f;

        p->cgd.tiki = cgi.R_Model_GetHandle(ent.hModel);

        if (p->modelname == "*beam") {
            ent.reType       = RT_BEAM;
            ent.customShader = cgi.R_RegisterShader("beamshader");
        } else if (strstr(p->modelname, ".spr")) {
            ent.reType = RT_SPRITE;
        } else {
            ent.reType = RT_MODEL;
        }

        if (m_spawnthing->animName.length() && p->cgd.tiki >= 0) {
            ent.anim = cgi.Anim_NumForName(p->cgd.tiki, m_spawnthing->animName);

            if (ent.anim < 0) {
                ent.anim = 0;
            }

            ent.oldanim         = ent.anim;
            ent.frame           = 0;
            ent.oldframe        = 0;
            ent.framelerp       = 0.0f;
            ent.crossblend_lerp = -1.0f;
            ent.legsLive        = qtrue;

            ent.actionWeight = 1.0f;
        } else if (ent.reType == RT_MODEL && p->cgd.tiki >= 0) {
            ent.anim = cgi.Anim_NumForName(p->cgd.tiki, "idle");

            if (ent.anim < 0) {
                ent.anim = 0;
            }

            ent.oldanim         = ent.anim;
            ent.frame           = 0;
            ent.oldframe        = 0;
            ent.framelerp       = 0.0f;
            ent.crossblend_lerp = -1.0f;
            ent.legsLive        = qtrue;

            ent.actionWeight = 1.0f;
        }

        if (p->cgd.tiki >= 0) {
            ent.legsLive = qtrue;
        }

        if (m_spawnthing->cgd.flags & T_RANDSCALE) 
        {
            ent.scale    = RandomizeRange(m_spawnthing->cgd.scalemin, m_spawnthing->cgd.scalemax);
            p->cgd.scale = ent.scale;
        } else {
            ent.scale = m_spawnthing->cgd.scale;
        }

        ent.scale *= current_entity_scale;

        {
            static int r133_beamfx_n;
            const char *r133_tn = (p->cgd.tiki >= 0) ? cgi.TIKI_Name(p->cgd.tiki) : NULL;

            if (R120_On() && r133_tn && Q_stristr(r133_tn, "cylinder") && R120_Throttle(&r133_beamfx_n, 6, 256)) {
                R133_PROBE(
                    "BEAMFX:tm-spawn",
                    "em='%s' tiki='%s' cgdscale=%.4f hostscale=%.4f csscale=%.4f spawn=%.4f rate=%.4f "
                    "life=%d hostent=%d\n",
                    m_spawnthing->emittername.c_str(),
                    r133_tn,
                    m_spawnthing->cgd.scale,
                    current_entity ? current_entity->scale : -1.0f,
                    current_scale,
                    ent.scale,
                    p->cgd.scaleRate,
                    p->cgd.life,
                    (current_entity ? current_entity->entityNumber : -1)
                );
            }
        }

        if (current_entity) {
            ent.renderfx |= (current_entity->renderfx & ~(RF_FLAGS_NOT_INHERITED | RF_LIGHTING_ORIGIN));

            if (m_spawnthing->traceImpactSpawn) {
                p->cgd.flags &= ~T_CIRCLE;
            }
        }

        for (i = 0; i < 4; i++) {
            p->cgd.color[i]   = m_spawnthing->cgd.color[i];
            ent.shaderRGBA[i] = (byte)(p->cgd.color[i] * 255.0);
        }

        if (p->cgd.flags2 & T2_VARYCOLOR) {
            for (i = 0; i < 3; i++) {
                p->cgd.color[i] *= 0.8f + random() * 0.2f;
            }
        }

        if (current_entity) {
            if (current_entity->shaderRGBA[3] != 255) {
                for (i = 0; i < 4; i++) {
                    p->cgd.color[3] *= current_entity->shaderRGBA[3] / 255.0;
                    ent.shaderRGBA[3] = current_entity->shaderRGBA[3];
                }
            }

            if (m_spawnthing->cgd.color[3] < 1.0f) {
                p->cgd.color[3] *= m_spawnthing->cgd.color[3];
                ent.shaderRGBA[3] = (int)(p->cgd.color[3] * 255.0f);
            }
        }

        p->ent     = ent;
        p->lastEnt = ent;
        p->number  = -1;

        refEntity_t *parentEnt = cgi.R_GetRenderEntity(p->cgd.parent);

        if (parentEnt) {
            AxisToAngles(parentEnt->axis, p->cgd.parentLinkAngles);
        }


        p->aliveTime = (timealive > 0) ? timealive : 0;

        if ((m_spawnthing->cgd.flags & T_ANIMATEONCE) && (p->ent.anim > 0)) {
            p->cgd.life = cgi.Anim_Time(p->cgd.tiki, p->ent.anim) * 1000.0f;
        } else {
            float fLife = CGResolveRandomNumber(
                m_spawnthing->life_rand,
                (float)m_spawnthing->cgd.life,
                m_spawnthing->life_random
            );

            if (fLife < 0.0f) {
                fLife = 0.0f;
            }

            p->cgd.life = (int)fLife;
        }

        if (p->cgd.createTime == 0) {
            p->cgd.createTime = cg.time;
        }

        p->lastAnimTime          = p->cgd.createTime;
        p->lastPhysicsTime       = p->cgd.createTime;
        p->killTime              = cg.time + p->cgd.life; 
        p->seed                  = m_seed++;              
        p->cgd.velocity          = Vector(0, 0, 0);       

        // TempModelPhysics's call at 0x00015D68 never fires.  Reproduced as-is.
        p->touchfcn              = m_spawnthing->touchfcn;

        p->hide                  = m_spawnthing->hide;

        p->next_bouncesound_time = 0;                     

        if (p->cgd.flags & T_TWINKLE) {
            if (random() > 0.5f) {
                p->cgd.flags |= T_TWINKLE_OFF;
            }
        }

        if (p->cgd.flags2 & T2_TRAIL) {
            p->number = m_command_time_manager.AssignNumber();
            p->cgd.flags |= T_ASSIGNED_NUMBER;
            int        oldnum     = current_entity_number;
            centity_t *oldcent    = current_centity;
            current_centity       = NULL;
            current_entity_number = p->number;

            Event *swipeEv = new Event(EV_Client_SwipeOn);
            swipeEv->AddString(p->cgd.swipe_shader);
            swipeEv->AddString(p->cgd.swipe_tag_start);
            swipeEv->AddString(p->cgd.swipe_tag_end);
            swipeEv->AddFloat(p->cgd.swipe_life);

            commandManager.ProcessEvent(swipeEv);

            current_centity       = oldcent;
            current_entity_number = oldnum;
        }

        if (p->cgd.tiki >= 0) {
            for (i = 1; i <= m_emitters.NumObjects(); i++) {
                spawnthing_t *st = m_emitters.ObjectAt(i);

                if (st->cgd.tiki == p->cgd.tiki) {
                    if (p->number == -1) {
                        p->number = st->AssignNumber();
                    }

                    st->GetEmitTime(p->number);
                }
            }
        }

        // 0x00018BBC is SetAccel__...FRC6Vectorf -- the two-argument overload, not the
        // NOT a cvar and NOT the renderer's world_t.  Retail dereferences cg.snap here
        // 0 in all nine, so this arm never fires on shipped data.  It is implemented
        if (m_spawnthing->windEffect > 0.0f) {
            Vector wind(cg.snap->ws.wind_direction);

            float windLength = wind.length();

            if (windLength != 0.0f) {
                wind *= 1.0f / windLength;
            }

            SetAccel(wind * (100.0f * cg.snap->ws.wind_intensity * m_spawnthing->windEffect), 0.0f);
        }

        for (i = 0; i < 3; i++) {
            p->cgd.avelocity[i] = CGResolveRandomNumber(
                m_spawnthing->avelocity_rand[i],
                m_spawnthing->avelocity_base[i],
                m_spawnthing->avelocity_amplitude[i]
                );

            p->cgd.angles[i] = CGResolveRandomNumber(
                m_spawnthing->angles_rand[i],
                m_spawnthing->cgd.angles[i],
                m_spawnthing->angles_amplitude[i]
                );
        }

        // @0x00017080 raw words 0x00018C38..0x00018E98.  This replaces the port-only
        // an angle slot that retail's `direction' verb never writes.
        Vector dirAngles = newForward;    

        if (dirAngles[0] == 0.0f && dirAngles[1] == 0.0f) {
            dirAngles[0] = -(dirAngles[2] <= 0.0f ? 270.0f : 90.0f);
            dirAngles[1] = 0.0f;
            dirAngles[2] = 0.0f;
        } else {
            float yaw   = 180.0f * atan2(dirAngles[1], dirAngles[0]) / M_PI;
            const float r     = sqrt(dirAngles[0] * dirAngles[0] + dirAngles[1] * dirAngles[1]);
            float pitch = 180.0f * atan2(dirAngles[2], r) / M_PI;

            if (yaw < 0.0f) {
                yaw += 360.0f;
            }
            if (pitch < 0.0f) {
                pitch += 360.0f;
            }

            dirAngles[0] = -pitch;
            dirAngles[1] = yaw;
            dirAngles[2] = 0.0f;
        }

        for (i = 0; i < 3; i++) {
            dirAngles[i] += CGResolveRandomNumber(
                m_spawnthing->randDirType[i],
                m_spawnthing->randDirBase[i],
                m_spawnthing->randDirAmp[i]
                );
        }

        {
            const float pitch = dirAngles[0];
            const float yaw   = dirAngles[1];
            const float cp    = cos(M_PI / 180.0f * pitch);
            const float cy    = cos(M_PI / 180.0f * yaw);

            newForward = Vector(
                cp * cy,
                cp * sin(M_PI / 180.0f * yaw),
                -sin(M_PI / 180.0f * pitch)
                );
        }

        // 0x00018DF4..0x00018EA0: the velocity is REPLACED from the forward, not added to,
        for (i = 0; i < 3; i++) {
            p->cgd.velocity[i] = newForward[i] * current_entity_scale;
        }

        if (m_spawnthing->forwardVelocity != 0) {
            p->cgd.spinSpeed = m_spawnthing->forwardVelocity;
            for (i = 0; i < 3; i++) {
                p->cgd.velocity[i] = newForward[i] * m_spawnthing->forwardVelocity * current_entity_scale;
            }
        }

        if (p->cgd.flags2 & T2_PARALLEL) {
            Vector v1 = p->cgd.origin - cg.refdef.vieworg;
            vectoangles(v1, p->cgd.angles);
        }

        AnglesToAxis(p->cgd.angles, m_spawnthing->axis);
        AxisCopy(m_spawnthing->axis, p->ent.axis);

        for (i = 0; i < 3; i++) {
            float fOffset = CGResolveRandomNumber(
                                m_spawnthing->axis_offset_rand[i],
                                m_spawnthing->axis_offset_base[i],
                                m_spawnthing->axis_offset_amplitude[i]
                                )
                            * current_entity_scale;

            if (p->cgd.flags2 & T2_PARALLEL) {
                p->cgd.origin += Vector(m_spawnthing->axis[i]) * fOffset;
            } else {
                p->cgd.origin += Vector(m_spawnthing->tag_axis[i]) * fOffset;
            }
        }

        if (m_spawnthing->cgd.flags2 & T2_RADIALVELOCITY) {
            float fLength;

            p->cgd.velocity = p->cgd.origin - start;

            fLength = p->cgd.velocity.length();
            if (fLength) {
                float fVel = m_spawnthing->cgd.velocity[0]
                           + (m_spawnthing->cgd.velocity[1] + m_spawnthing->cgd.velocity[2] * random()) / fLength;

                p->cgd.velocity *= fVel;
            }
        }

        for (i = 0; i < 3; i++) {
            float fVel = CGResolveRandomNumber(
                             m_spawnthing->randvel_rand[i],
                             m_spawnthing->randvel_base[i],
                             m_spawnthing->randvel_amplitude[i]
                             )
                         * current_entity_scale;

            if (m_spawnthing->cgd.flags & T_RANDVELAXIS) {
                if (p->cgd.flags2 & T2_NOTAGAXIS) {
                    p->cgd.velocity += fVel * Vector(m_spawnthing->axis[i]);
                } else {
                    p->cgd.velocity += fVel * Vector(m_spawnthing->tag_axis[i]);
                }
            } else {
                p->cgd.velocity[i] += fVel;
            }
        }

        // T_ALIGN/T_DETAIL `angles = velocity.toAngles()` pass at 0x000191B0, which must
        if (m_spawnthing->chanceToEmitAtCamera > 0.0f) {
            if ((float)(rand() % 100) < m_spawnthing->chanceToEmitAtCamera) {
                Vector camOrg(cg.refdef.vieworg);
                Vector dir = camOrg - p->cgd.origin;

                float emitLength = dir.length();

                if (emitLength != 0.0f) {
                    dir *= 1.0f / emitLength;
                }

                float speed = p->cgd.velocity.length();

                if (p->cgd.accel[2] == 0.0f) {
                    p->cgd.velocity = dir * speed;
                } else {
                    p->cgd.accel[0] = 0.0f;
                    p->cgd.accel[1] = 0.0f;

                    Trajectory trajectory(p->cgd.origin, camOrg, speed, p->cgd.accel[2]);

                    p->cgd.velocity = trajectory.GetInitialVelocity();
                }

                p->cgd.flags2 |= T2_MOVE;
            }
        }

        if (p->cgd.flags & (T_ALIGN | T_DETAIL)) {
            p->cgd.angles = p->cgd.velocity.toAngles();
        }

        p->cgd.origin += p->cgd.velocity * (p->aliveTime / 1000.0) * current_entity_scale;

        if (p->cgd.flags & T_AUTOCALCLIFE) {
            Vector  end, vTraceDelta;
            float   length, speed;
            vec3_t  vTraceForward;
            trace_t trace;

            AngleVectorsLeft(p->cgd.angles, vTraceForward, NULL, NULL);

            end = p->cgd.origin + Vector(vTraceForward) * MAP_SIZE;
            CG_Trace(
                &trace,
                p->cgd.origin,
                vec_zero,
                vec_zero,
                end,
                ENTITYNUM_NONE,
                CONTENTS_SOLID | CONTENTS_WATER,
                qfalse,
                qfalse,
                "AutoCalcLife"
            );

            vTraceDelta = trace.endpos - p->cgd.origin;
            length      = vTraceDelta.length();
            speed  = p->cgd.velocity.length();

            p->cgd.life = (length / speed) * 1000.0f;
        }

        if (current_entity) {
            if (m_spawnthing->cgd.flags & (T_GLOBALFADEIN | T_GLOBALFADEOUT)) {
                int numframes = cgi.Anim_NumFrames(current_tiki, current_entity->anim);

                p->cgd.alpha = (float)current_entity->frame / (float)numframes;

                if (m_spawnthing->cgd.flags & T_GLOBALFADEOUT) {
                    p->cgd.alpha = 1.0f - p->cgd.alpha;
                }
            }
        }

        if (!(p->cgd.flags & T_DLIGHT) && p->ent.reType == RT_MODEL && !p->ent.hModel) {
            Com_Printf("^~^~^ not spawning tempmodel because it is using the world as a brush model\n");
            FreeTempModel(p);
            break;
        }

        if (m_spawnthing->cgd.flags2 & T2_RELATIVEANGLES) {
            float mat[3][3];

            MatrixMultiply(m_spawnthing->axis, p->ent.axis, mat);
            VectorCopy(mat[0], p->ent.axis[0]);
            VectorCopy(mat[1], p->ent.axis[1]);
            VectorCopy(mat[2], p->ent.axis[2]);
            MatrixToEulerAngles(p->ent.axis, p->cgd.angles);
        }
    }
}
