
// This file is retail cgame_mac.pef's cg_testemitter, not MOHAA 2.0's.  MOHAA's editor

#include "cg_commands.h"

cvar_t *cg_te_xangles;
cvar_t *cg_te_yangles;
cvar_t *cg_te_zangles;
cvar_t *cg_te_life;
cvar_t *cg_te_alpha;
cvar_t *cg_te_dietouch;
cvar_t *cg_te_bouncefactor;
cvar_t *cg_te_scale;
cvar_t *cg_te_scalemin;
cvar_t *cg_te_scalemax;
cvar_t *cg_te_model;
cvar_t *cg_te_color_r;
cvar_t *cg_te_color_g;
cvar_t *cg_te_color_b;
cvar_t *cg_te_startcolor_r;
cvar_t *cg_te_startcolor_g;
cvar_t *cg_te_startcolor_b;
cvar_t *cg_te_endcolor_r;
cvar_t *cg_te_endcolor_g;
cvar_t *cg_te_endcolor_b;
cvar_t *cg_te_accel_x;
cvar_t *cg_te_accel_y;
cvar_t *cg_te_accel_z;
cvar_t *cg_te_count;
cvar_t *cg_te_fade;
cvar_t *cg_te_fadein;
cvar_t *cg_te_spawnrate;
cvar_t *cg_te_scalerate;
cvar_t *cg_te_circle;
cvar_t *cg_te_sphere;
cvar_t *cg_te_insphere;
cvar_t *cg_te_radius;
cvar_t *cg_te_swarm;
cvar_t *cg_te_wavy;
cvar_t *cg_te_align;
cvar_t *cg_te_flickeralpha;
cvar_t *cg_te_collision;
cvar_t *cg_te_globalfade;
cvar_t *cg_te_randomroll;
cvar_t *cg_te_orgoffset_x;
cvar_t *cg_te_orgoffset_y;
cvar_t *cg_te_orgoffset_z;
cvar_t *cg_te_rorgx;
cvar_t *cg_te_rorgy;
cvar_t *cg_te_rorgz;
cvar_t *cg_te_angles_pitch;
cvar_t *cg_te_angles_yaw;
cvar_t *cg_te_angles_roll;
cvar_t *cg_te_randpitch;
cvar_t *cg_te_randyaw;
cvar_t *cg_te_randroll;
cvar_t *cg_te_randvelx;
cvar_t *cg_te_randvely;
cvar_t *cg_te_randvelz;
cvar_t *cg_te_rvxtype;
cvar_t *cg_te_rvytype;
cvar_t *cg_te_rvztype;
cvar_t *cg_te_curnum;
cvar_t *cg_te_forwardvel;

// @0x00041068..0x41170), never Cvar_Get's them, so it keeps no cvar_t* for any of them and

// instead, which is fine: as the TestEmitter note below records, retail never actually
#define MAX_TESTEMITTERS_SAVE 10

#define TE_INUSE(i) te_emitterInUse[(i) + 1]

static qboolean te_emitterInUse[MAX_TESTEMITTERS_SAVE + 2]; 

static spawnthing_t te_testemitters[MAX_TESTEMITTERS_SAVE];

spawnthing_t *pCurrSpawnthing;

static randtype_t CG_RandTypeFromStr(const char *s)
{
    if (!Q_stricmp(s, "Ra")) {
        return RANDOM;
    }
    if (!Q_stricmp(s, "Cr")) {
        return CRANDOM;
    }
    return NOT_RANDOM;
}

spawnthing_t *ClientGameCommandManager::CopySpawnthing(spawnthing_t *pDest, spawnthing_t& pSrc)
{
    int            i;
    Container<str> savedModels;
    Container<str> savedTags;
    Vector         savedAvelocity;
    float          savedTbcTimeOffset;
    TimeBasedCommandManager          *savedTbcManager;
    TimeBasedCommandEmitterController *savedTbcController;
    float          savedTbcNextCommandTime;
    int            savedTbcEmitterCursor;

    savedModels    = pDest->m_modellist;
    savedTags      = pDest->m_taglist;
    savedAvelocity = pDest->cgd.avelocity;

    savedTbcTimeOffset      = pDest->tbcTimeOffset;
    savedTbcManager         = pDest->m_tbcManager;
    savedTbcController      = pDest->tbcController;
    savedTbcNextCommandTime = pDest->tbcNextCommandTime;
    savedTbcEmitterCursor   = pDest->tbcEmitterCursor;

    *pDest = pSrc;

    pDest->m_modellist   = savedModels;
    pDest->m_taglist     = savedTags;
    pDest->cgd.avelocity = savedAvelocity;

    pDest->tbcTimeOffset      = savedTbcTimeOffset;
    pDest->m_tbcManager       = savedTbcManager;
    pDest->tbcController      = savedTbcController;
    pDest->tbcNextCommandTime = savedTbcNextCommandTime;
    pDest->tbcEmitterCursor   = savedTbcEmitterCursor;

    for (i = 1; i <= pSrc.m_modellist.NumObjects(); ++i) {
        pDest->m_modellist.AddObject(pSrc.m_modellist.ObjectAt(i));
    }

    return pDest;
}

void CG_SetTestEmitterValues()
{
    vec3_t vAngles;

    pCurrSpawnthing->cgd.flags2 |= T2_ACCEL | T2_AMOVE | T2_MOVE; 

    vAngles[0] = cg_te_xangles->value;
    vAngles[1] = cg_te_yangles->value;
    vAngles[2] = cg_te_zangles->value;
    AnglesToAxis(vAngles, pCurrSpawnthing->axis);

    pCurrSpawnthing->m_modellist.ClearObjectList();
    pCurrSpawnthing->m_modellist.AddObject(cg_te_model->string);

    pCurrSpawnthing->cgd.alpha        = cg_te_alpha->value;
    pCurrSpawnthing->cgd.color[3]     = (byte)(255.0f * cg_te_alpha->value);
    pCurrSpawnthing->cgd.bouncefactor = cg_te_bouncefactor->value;
    pCurrSpawnthing->cgd.scale        = cg_te_scale->value;
    pCurrSpawnthing->cgd.life         = (int)(1000.0f * cg_te_life->value);
    pCurrSpawnthing->cgd.color[0]     = (byte)(255.0f * cg_te_color_r->value);
    pCurrSpawnthing->cgd.color[1]     = (byte)(255.0f * cg_te_color_g->value);
    pCurrSpawnthing->cgd.color[2]     = (byte)(255.0f * cg_te_color_b->value);

    pCurrSpawnthing->cgd.accel[0] = cg_te_accel_x->value;
    pCurrSpawnthing->cgd.accel[1] = cg_te_accel_y->value;
    pCurrSpawnthing->cgd.accel[2] = cg_te_accel_z->value;

    pCurrSpawnthing->count     = (int)cg_te_count->value;
    pCurrSpawnthing->spawnRate = 1000.0f * (1.0f / cg_te_spawnrate->value);

    pCurrSpawnthing->cgd.scaleRate = cg_te_scalerate->value;
    pCurrSpawnthing->sphereRadius  = cg_te_radius->value;
    pCurrSpawnthing->forwardVelocity = cg_te_forwardvel->value;

    pCurrSpawnthing->origin_offset_base[0] = cg_te_orgoffset_x->value;
    pCurrSpawnthing->origin_offset_base[1] = cg_te_orgoffset_y->value;
    pCurrSpawnthing->origin_offset_base[2] = cg_te_orgoffset_z->value;

    pCurrSpawnthing->offset_rand[0] = CG_RandTypeFromStr(cg_te_rorgx->string);
    pCurrSpawnthing->offset_rand[1] = CG_RandTypeFromStr(cg_te_rorgy->string);
    pCurrSpawnthing->offset_rand[2] = CG_RandTypeFromStr(cg_te_rorgz->string);

    pCurrSpawnthing->cgd.angles[0] = cg_te_angles_pitch->value;
    pCurrSpawnthing->cgd.angles[1] = cg_te_angles_yaw->value;
    pCurrSpawnthing->cgd.angles[2] = cg_te_angles_roll->value;

    pCurrSpawnthing->angles_rand[0] = CG_RandTypeFromStr(cg_te_randpitch->string);
    pCurrSpawnthing->angles_rand[0] = CG_RandTypeFromStr(cg_te_randyaw->string);
    pCurrSpawnthing->angles_rand[0] = CG_RandTypeFromStr(cg_te_randroll->string);

    pCurrSpawnthing->randvel_base[0] = cg_te_randvelx->value;
    pCurrSpawnthing->randvel_base[1] = cg_te_randvely->value;
    pCurrSpawnthing->randvel_base[2] = cg_te_randvelz->value;

    pCurrSpawnthing->randvel_rand[0] = CG_RandTypeFromStr(cg_te_rvxtype->string);
    pCurrSpawnthing->randvel_rand[1] = CG_RandTypeFromStr(cg_te_rvytype->string);
    pCurrSpawnthing->randvel_rand[2] = CG_RandTypeFromStr(cg_te_rvztype->string);

    pCurrSpawnthing->cgd.scalemin = cg_te_scalemin->value;
    pCurrSpawnthing->cgd.scalemax = cg_te_scalemax->value;

    if (pCurrSpawnthing->cgd.scalemin == 0 && pCurrSpawnthing->cgd.scalemax == 0) {
        pCurrSpawnthing->cgd.flags |= 1;
    } else {
        pCurrSpawnthing->cgd.flags &= ~1;
    }

    if (cg_te_dietouch->integer) {
        pCurrSpawnthing->cgd.flags |= 0x40;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x40;
    }

    if (cg_te_fade->integer) {
        pCurrSpawnthing->cgd.flags |= 0x20;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x20;
    }

    if (cg_te_circle->integer) {
        pCurrSpawnthing->cgd.flags |= 0x10;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x10;
    }

    if (cg_te_sphere->integer) {
        pCurrSpawnthing->cgd.flags |= 0x04;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x04;
    }

    if (cg_te_insphere->integer) {
        pCurrSpawnthing->cgd.flags |= 0x08;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x08;
    }

    if (cg_te_align->integer) {
        pCurrSpawnthing->cgd.flags |= 0x400;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x400;
    }

    if (cg_te_flickeralpha->integer) {
        pCurrSpawnthing->cgd.flags |= 0x1000;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x1000;
    }

    if (cg_te_randomroll->integer) {
        pCurrSpawnthing->cgd.flags |= 0x40000;
    } else {
        pCurrSpawnthing->cgd.flags &= ~0x40000;
    }
}

void ClientGameCommandManager::SwitchEmitter(int num, int setcurnum)
{
    str model;

    if (setcurnum) {
        cgi.Cvar_Set("cg_te_curnum", va("%f", (double)num));
    }

    pCurrSpawnthing = &te_testemitters[num];

    model = pCurrSpawnthing->GetModel();
    cgi.Cvar_Set("cg_te_model", model.c_str());

    cgi.Cvar_Set("cg_te_alpha", va("%f", pCurrSpawnthing->cgd.alpha));
    cgi.Cvar_Set("cg_te_bouncefactor", va("%f", pCurrSpawnthing->cgd.bouncefactor));
    cgi.Cvar_Set("cg_te_scale", va("%f", pCurrSpawnthing->cgd.scale));

    cgi.Cvar_Set("cg_te_life", va("%f", (double)(pCurrSpawnthing->cgd.life / 1000)));

    cgi.Cvar_Set("cg_te_color_r", va("%f", (double)((byte)(255.0f * pCurrSpawnthing->cgd.color[0]) / 255)));
    cgi.Cvar_Set("cg_te_color_g", va("%f", (double)((byte)(255.0f * pCurrSpawnthing->cgd.color[1]) / 255)));
    cgi.Cvar_Set("cg_te_color_b", va("%f", (double)((byte)(255.0f * pCurrSpawnthing->cgd.color[2]) / 255)));

    cgi.Cvar_Set("cg_te_accel_x", va("%f", pCurrSpawnthing->cgd.accel[0]));
    cgi.Cvar_Set("cg_te_accel_y", va("%f", pCurrSpawnthing->cgd.accel[1]));
    cgi.Cvar_Set("cg_te_accel_z", va("%f", pCurrSpawnthing->cgd.accel[2]));

    cgi.Cvar_Set("cg_te_count", va("%f", (double)pCurrSpawnthing->count));
    cgi.Cvar_Set("cg_te_spawnrate", va("%f", pCurrSpawnthing->spawnRate));
    cgi.Cvar_Set("cg_te_scalerate", va("%f", pCurrSpawnthing->cgd.scaleRate));
    cgi.Cvar_Set("cg_te_radius", va("%f", pCurrSpawnthing->sphereRadius));

    cgi.Cvar_Set("cg_te_forwardvelocity", va("%f", pCurrSpawnthing->forwardVelocity));

    cgi.Cvar_Set("cg_te_orgoffset_x", va("%f", pCurrSpawnthing->origin_offset_base[0]));
    cgi.Cvar_Set("cg_te_orgoffset_y", va("%f", pCurrSpawnthing->origin_offset_base[1]));
    cgi.Cvar_Set("cg_te_orgoffset_z", va("%f", pCurrSpawnthing->origin_offset_base[2]));

    if (pCurrSpawnthing->offset_rand[0] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_orgx", "No");
    } else if (pCurrSpawnthing->offset_rand[0] == CRANDOM) {
        cgi.Cvar_Set("cg_te_orgx", "Cr");
    } else if (pCurrSpawnthing->offset_rand[0] == RANDOM) {
        cgi.Cvar_Set("cg_te_orgx", "Ra");
    }

    if (pCurrSpawnthing->offset_rand[1] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_orgy", "No");
    } else if (pCurrSpawnthing->offset_rand[1] == CRANDOM) {
        cgi.Cvar_Set("cg_te_orgy", "Cr");
    } else if (pCurrSpawnthing->offset_rand[1] == RANDOM) {
        cgi.Cvar_Set("cg_te_orgy", "Ra");
    }

    if (pCurrSpawnthing->offset_rand[2] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_orgz", "No");
    } else if (pCurrSpawnthing->offset_rand[2] == CRANDOM) {
        cgi.Cvar_Set("cg_te_orgz", "Cr");
    } else if (pCurrSpawnthing->offset_rand[2] == RANDOM) {
        cgi.Cvar_Set("cg_te_orgz", "Ra");
    }

    cgi.Cvar_Set("cg_te_randvelx", va("%f", pCurrSpawnthing->randvel_base[0]));
    cgi.Cvar_Set("cg_te_randvely", va("%f", pCurrSpawnthing->randvel_base[1]));
    cgi.Cvar_Set("cg_te_randvelz", va("%f", pCurrSpawnthing->randvel_base[2]));

    cgi.Cvar_Set("cg_te_angles_pitch", va("%f", pCurrSpawnthing->cgd.angles[0]));
    cgi.Cvar_Set("cg_te_angles_yaw", va("%f", pCurrSpawnthing->cgd.angles[1]));
    cgi.Cvar_Set("cg_te_angles_roll", va("%f", pCurrSpawnthing->cgd.angles[2]));

    if (pCurrSpawnthing->angles_rand[0] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_randpitch", "No");
    } else if (pCurrSpawnthing->angles_rand[0] == CRANDOM) {
        cgi.Cvar_Set("cg_te_randpitch", "Cr");
    } else if (pCurrSpawnthing->angles_rand[0] == RANDOM) {
        cgi.Cvar_Set("cg_te_randpitch", "Ra");
    }

    if (pCurrSpawnthing->angles_rand[0] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_randyaw", "No");
    } else if (pCurrSpawnthing->angles_rand[0] == CRANDOM) {
        cgi.Cvar_Set("cg_te_randyaw", "Cr");
    } else if (pCurrSpawnthing->angles_rand[0] == RANDOM) {
        cgi.Cvar_Set("cg_te_randyaw", "Ra");
    }

    if (pCurrSpawnthing->angles_rand[0] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_randroll", "No");
    } else if (pCurrSpawnthing->angles_rand[0] == CRANDOM) {
        cgi.Cvar_Set("cg_te_randroll", "Cr");
    } else if (pCurrSpawnthing->angles_rand[0] == RANDOM) {
        cgi.Cvar_Set("cg_te_randroll", "Ra");
    }

    if (pCurrSpawnthing->randvel_rand[0] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_rvxtype", "No");
    } else if (pCurrSpawnthing->randvel_rand[0] == CRANDOM) {
        cgi.Cvar_Set("cg_te_rvxtype", "Cr");
    } else if (pCurrSpawnthing->randvel_rand[0] == RANDOM) {
        cgi.Cvar_Set("cg_te_rvxtype", "Ra");
    }

    if (pCurrSpawnthing->randvel_rand[1] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_rvytype", "No");
    } else if (pCurrSpawnthing->randvel_rand[1] == CRANDOM) {
        cgi.Cvar_Set("cg_te_rvytype", "Cr");
    } else if (pCurrSpawnthing->randvel_rand[1] == RANDOM) {
        cgi.Cvar_Set("cg_te_rvytype", "Ra");
    }

    if (pCurrSpawnthing->randvel_rand[2] == NOT_RANDOM) {
        cgi.Cvar_Set("cg_te_rvztype", "No");
    } else if (pCurrSpawnthing->randvel_rand[2] == CRANDOM) {
        cgi.Cvar_Set("cg_te_rvztype", "Cr");
    } else if (pCurrSpawnthing->randvel_rand[2] == RANDOM) {
        cgi.Cvar_Set("cg_te_rvztype", "Ra");
    }

    cgi.Cvar_Set("cg_te_scalemin", va("%f", pCurrSpawnthing->cgd.scalemin));
    cgi.Cvar_Set("cg_te_scalemax", va("%f", pCurrSpawnthing->cgd.scalemax));

    cgi.Cvar_Set("cg_te_dietouch", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x40) != 0)));
    cgi.Cvar_Set("cg_te_fade", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x20) != 0)));
    cgi.Cvar_Set("cg_te_circle", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x10) != 0)));
    cgi.Cvar_Set("cg_te_sphere", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x04) != 0)));
    cgi.Cvar_Set("cg_te_insphere", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x08) != 0)));
    cgi.Cvar_Set("cg_te_align", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x400) != 0)));
    cgi.Cvar_Set("cg_te_flickeralpha", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x1000) != 0)));
    cgi.Cvar_Set("cg_te_randomroll", va("%f", (double)((pCurrSpawnthing->cgd.flags & 0x40000) != 0)));
}

void ClientGameCommandManager::TestEmitter(int reset)
{
    vec3_t angles;
    int    i;

    pCurrSpawnthing = CreateNewEmitter();
    if (!pCurrSpawnthing) {
        return;
    }

    pCurrSpawnthing->emittername = "TestEmitter";

    VectorMA(cg.refdef.vieworg, 100.0, cg.refdef.viewaxis[0], pCurrSpawnthing->cgd.origin);
    VectorSet(angles, 0, cg.refdefViewAngles[1], 0);
    AnglesToAxis(angles, pCurrSpawnthing->axis);

    pCurrSpawnthing->entnum = 9999999;

    if (reset) {
        CG_ResetTestEmitter();
    }

    CG_SetTestEmitterValues();

    for (i = 0; i < MAX_TESTEMITTERS_SAVE; ++i) {
        if (TE_INUSE(i)) {
            continue;
        }

        CopySpawnthing(&te_testemitters[i], *pCurrSpawnthing);
        cgi.Cvar_Set("cg_te_curnum", va("%f", (double)i));
        break;
    }
}

void CG_NextEmitter_f(void)
{
    int cur;
    int i;

    cur  = cg_te_curnum->integer;
    i    = cur + 1;

    for (;;) {
        if (TE_INUSE(i) || i == cur) {
            break;
        }
        ++i;
        if (i > MAX_TESTEMITTERS_SAVE - 1) {
            i = 0;
        }
    }

    if (i != cur) {
        commandManager.SwitchEmitter(i, 1);
        CG_SetTestEmitterValues();
    }
}

void CG_PrevEmitter_f(void)
{
    int cur;
    int i;

    cur = cg_te_curnum->integer;
    i   = cur - 1;

    for (;;) {
        if (TE_INUSE(i) || i == cur) {
            break;
        }
        --i;
        if (i < 0) {
            i = MAX_TESTEMITTERS_SAVE - 1;
        }
    }

    if (i != cur) {
        commandManager.SwitchEmitter(i, 1);
        CG_SetTestEmitterValues();
    }
}


// printed is always the base, never an amplitude, because the retail editor has none.
static void CG_DumpEmitterBaseAndRand(str *buff, const char *prefix, const float *base, const randtype_t *rand)
{
    int i;

    *buff += prefix;

    for (i = 0; i < 3; i++) {
        if (rand[i] == NOT_RANDOM) {
            *buff += va("%0.2f ", base[i]);
        }
        if (rand[i] == RANDOM) {
            *buff += va("random %0.2f ", base[i]);
        }
        if (rand[i] == CRANDOM) {
            *buff += va("crandom %0.2f ", base[i]);
        }
    }

    *buff += "\n";
}

void ClientGameCommandManager::DumpEmitter(void)
{
    str     buff;
    cvar_t *filename;

    if (!pCurrSpawnthing) {
        return;
    }

    filename = cgi.Cvar_Get("cg_te_filename", "dump/emitter.txt", 0);

    buff += va("model %s\n", pCurrSpawnthing->m_modellist.ObjectAt(1).c_str());

    if (pCurrSpawnthing->cgd.alpha != 1.0f) {
        buff += va("alpha %0.2f\n", pCurrSpawnthing->cgd.alpha);
    }

    if (pCurrSpawnthing->cgd.bouncefactor != 1.0f) {
        buff += va("bouncefactor %0.2f\n", pCurrSpawnthing->cgd.bouncefactor);
    }

    if (pCurrSpawnthing->cgd.scale != 1.0f) {
        buff += va("scale %0.2f\n", pCurrSpawnthing->cgd.scale);
    }

    // 0x0003F318: cg_te_life->value (cvar_t +0x1C), not the spawnthing's millisecond life.
    buff += va("life %0.2f\n", cg_te_life->value);

    if (pCurrSpawnthing->cgd.accel[0] != 0.0f || pCurrSpawnthing->cgd.accel[1] != 0.0f
        || pCurrSpawnthing->cgd.accel[2] != 0.0f) {
        buff += va("accel %0.2f %0.2f %0.2f\n",
                   pCurrSpawnthing->cgd.accel[0],
                   pCurrSpawnthing->cgd.accel[1],
                   pCurrSpawnthing->cgd.accel[2]);
    }

    buff += va("spawnrate %0.2f\n", cg_te_spawnrate->value);

    if (pCurrSpawnthing->cgd.scaleRate != 0.0f) {
        buff += va("scalerate %0.2f\n", pCurrSpawnthing->cgd.scaleRate);
    }

    if (pCurrSpawnthing->sphereRadius != 0.0f) {
        buff += va("radius %0.2f\n", pCurrSpawnthing->sphereRadius);
    }

    if (pCurrSpawnthing->forwardVelocity != 0.0f) {
        buff += va("velocity %0.2f\n", pCurrSpawnthing->forwardVelocity);
    }

    CG_DumpEmitterBaseAndRand(&buff, "offset ", pCurrSpawnthing->origin_offset_base, pCurrSpawnthing->offset_rand);
    CG_DumpEmitterBaseAndRand(&buff, "angles ", pCurrSpawnthing->cgd.angles, pCurrSpawnthing->angles_rand);
    CG_DumpEmitterBaseAndRand(&buff, "randvel ", pCurrSpawnthing->randvel_base, pCurrSpawnthing->randvel_rand);

    if (pCurrSpawnthing->cgd.scalemin != 0.0f) {
        buff += va("scalemin %0.2f\n", pCurrSpawnthing->cgd.scalemin);
    }

    if (pCurrSpawnthing->cgd.scalemax != 0.0f) {
        buff += va("scalemax %0.2f\n", pCurrSpawnthing->cgd.scalemax);
    }

    if (pCurrSpawnthing->cgd.flags & T_DIETOUCH) {
        buff += "dietouch\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_FADE) {
        buff += "fade\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_CIRCLE) {
        buff += "circle\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_SPHERE) {
        buff += "sphere\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_INWARDSPHERE) {
        buff += "inwardsphere\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_ALIGN) {
        buff += "align\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_FLICKERALPHA) {
        buff += "flickeralpha\n";
    }

    if (pCurrSpawnthing->cgd.flags & T_RANDOMROLL) {
        buff += "randomroll\n";
    }

    cgi.FS_WriteTextFile(filename->string, buff.c_str(), buff.length() + 1);
}

void CG_UpdateTestEmitter(void)
{
    emittertime_t *et;
    int            i;
    int            count;
    int            red, green, blue;
    int            last_emit_time;
    int            timealive;
    float          dtime;
    float          lerpfrac;
    float          lerp;
    Vector         perp;
    Vector         line1end;
    Vector         end;
    Vector         delta;
    Vector         start;

    if (!pCurrSpawnthing) {
        return;
    }

    CG_SetTestEmitterValues();

    commandManager.CopySpawnthing(&te_testemitters[cg_te_curnum->integer], *pCurrSpawnthing);

    for (i = 0; i < MAX_TESTEMITTERS_SAVE; ++i) {
        if (!TE_INUSE(i)) {
            continue;
        }

        commandManager.SwitchEmitter(i, 0);
        CG_SetTestEmitterValues();

        et = pCurrSpawnthing->GetEmitTime(1);
        if (!et->active) {
            // 0x0003EB78 `beq 0x3f088` -- the epilogue, not the loop increment: a slot
            return;
        }

        pCurrSpawnthing->cgd.createTime = cg.time;

        if (i == cg_te_curnum->integer) {
            red = green = blue = 1;
        } else {
            red = green = 0;
            blue        = 1;
        }

        PerpendicularVector(perp, pCurrSpawnthing->axis[0]);
        {
            float len;

            len = sqrt(perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2]);
            if (len != 0.0f) {
                float scale = 1.0f / len;

                perp[0] = perp[0] * scale;
                perp[1] = perp[1] * scale;
                perp[2] = perp[2] * scale;
            }
        }

        start    = pCurrSpawnthing->cgd.origin;
        line1end = start + Vector(pCurrSpawnthing->axis[0]) * 50.0f;
        cgi.R_DebugLine(start, line1end, red, green, blue, 1.0f);

        end = line1end + perp * 16.0f + Vector(pCurrSpawnthing->axis[0]) * -16.0f;
        cgi.R_DebugLine(line1end, end, red, green, blue, 1.0f);

        end = line1end + perp * -16.0f + Vector(pCurrSpawnthing->axis[0]) * -16.0f;
        cgi.R_DebugLine(line1end, end, red, green, blue, 1.0f);

        if (et->last_emit_time <= 0 || pCurrSpawnthing->spawnRate == 0.0f) {
            et->last_emit_time = cg.time;
            continue;
        }

        dtime = (float)(cg.time - et->last_emit_time);
        count = (int)(dtime / pCurrSpawnthing->spawnRate);

        if (count == 0) {
            if (pCurrSpawnthing->GetModel() == "none") {
                continue;
            }
        }

        if (count == 1) {
            commandManager.SpawnTempModel(1, pCurrSpawnthing);
            et->last_emit_time = cg.time;
            continue;
        }

        lerpfrac = 1.0f / count;
        for (lerp = 0.0f; dtime > pCurrSpawnthing->spawnRate; lerp += lerpfrac) {
            last_emit_time = et->last_emit_time;

            et->last_emit_time = cg.time;
            dtime              = dtime - pCurrSpawnthing->spawnRate;

            if (et->lerp_emitter) {
                pCurrSpawnthing->cgd.origin = et->oldorigin + delta * lerp;
            }

            // (0x0003F01C..0x0003F024) -- so the sum is truncated toward zero, not rounded.
            timealive = (int)((double)last_emit_time + (double)dtime);

            commandManager.SpawnTempModel(1, timealive, pCurrSpawnthing);
        }
    }

    pCurrSpawnthing = &te_testemitters[cg_te_curnum->integer];
    commandManager.SwitchEmitter(cg_te_curnum->integer, 0);
}

void CG_TestEmitter_f(void)
{
    commandManager.TestEmitter(1);
}

void CG_ReloadEmitter_f(void)
{
    commandManager.TestEmitter(0);
}

void CG_DumpEmitter_f(void)
{
    commandManager.DumpEmitter();
}

void CG_InitTestEmitter(void)
{
    cg_te_xangles        = cgi.Cvar_Get("cg_te_xangles", "0", 0);
    cg_te_yangles        = cgi.Cvar_Get("cg_te_yangles", "0", 0);
    cg_te_zangles        = cgi.Cvar_Get("cg_te_zangles", "0", 0);
    cg_te_life           = cgi.Cvar_Get("cg_te_life", "1", 0);
    cg_te_alpha          = cgi.Cvar_Get("cg_te_alpha", "1", 0);
    cg_te_dietouch       = cgi.Cvar_Get("cg_te_dietouch", "0", 0);
    cg_te_bouncefactor   = cgi.Cvar_Get("cg_te_bouncefactor", "1", 0);
    cg_te_scale          = cgi.Cvar_Get("cg_te_scale", "1", 0);
    cg_te_scalemin       = cgi.Cvar_Get("cg_te_scalemin", "0", 0);
    cg_te_scalemax       = cgi.Cvar_Get("cg_te_scalemax", "0", 0);
    cg_te_model          = cgi.Cvar_Get("cg_te_model", "none", 0);
    cg_te_life           = cgi.Cvar_Get("cg_te_life", "1", 0);
    cg_te_color_r        = cgi.Cvar_Get("cg_te_color_r", "1", 0);
    cg_te_color_g        = cgi.Cvar_Get("cg_te_color_g", "1", 0);
    cg_te_color_b        = cgi.Cvar_Get("cg_te_color_b", "1", 0);
    cg_te_startcolor_r   = cgi.Cvar_Get("cg_te_startcolor_r", "1", 0);
    cg_te_startcolor_g   = cgi.Cvar_Get("cg_te_startcolor_g", "1", 0);
    cg_te_startcolor_b   = cgi.Cvar_Get("cg_te_startcolor_b", "1", 0);
    cg_te_endcolor_r     = cgi.Cvar_Get("cg_te_endcolor_r", "1", 0);
    cg_te_endcolor_g     = cgi.Cvar_Get("cg_te_endcolor_g", "1", 0);
    cg_te_endcolor_b     = cgi.Cvar_Get("cg_te_endcolor_b", "1", 0);
    cg_te_accel_x        = cgi.Cvar_Get("cg_te_accel_x", "0", 0);
    cg_te_accel_y        = cgi.Cvar_Get("cg_te_accel_y", "0", 0);
    cg_te_accel_z        = cgi.Cvar_Get("cg_te_accel_z", "0", 0);
    cg_te_count          = cgi.Cvar_Get("cg_te_count", "1", 0);
    cg_te_fade           = cgi.Cvar_Get("cg_te_fade", "0", 0);
    cg_te_fadein         = cgi.Cvar_Get("cg_te_fadein", "0", 0);
    cg_te_spawnrate      = cgi.Cvar_Get("cg_te_spawnrate", "1", 0);
    cg_te_scalerate      = cgi.Cvar_Get("cg_te_scalerate", "0", 0);
    cg_te_circle         = cgi.Cvar_Get("cg_te_circle", "0", 0);
    cg_te_sphere         = cgi.Cvar_Get("cg_te_sphere", "0", 0);
    cg_te_insphere       = cgi.Cvar_Get("cg_te_insphere", "0", 0);
    cg_te_radius         = cgi.Cvar_Get("cg_te_radius", "0", 0);
    cg_te_swarm          = cgi.Cvar_Get("cg_te_swarm", "0", 0);
    cg_te_wavy           = cgi.Cvar_Get("cg_te_wavy", "0", 0);
    cg_te_align          = cgi.Cvar_Get("cg_te_align", "0", 0);
    cg_te_flickeralpha   = cgi.Cvar_Get("cg_te_flickeralpha", "0", 0);
    cg_te_collision      = cgi.Cvar_Get("cg_te_collision", "0", 0);
    cg_te_globalfade     = cgi.Cvar_Get("cg_te_globalfade", "0", 0);
    cg_te_randomroll     = cgi.Cvar_Get("cg_te_randomroll", "0", 0);
    cg_te_orgoffset_x    = cgi.Cvar_Get("cg_te_orgoffset_x", "0", 0);
    cg_te_orgoffset_y    = cgi.Cvar_Get("cg_te_orgoffset_y", "0", 0);
    cg_te_orgoffset_z    = cgi.Cvar_Get("cg_te_orgoffset_z", "0", 0);
    cg_te_rorgx          = cgi.Cvar_Get("cg_te_rorgx", "No", 0);
    cg_te_rorgy          = cgi.Cvar_Get("cg_te_rorgy", "No", 0);
    cg_te_rorgz          = cgi.Cvar_Get("cg_te_rorgz", "No", 0);
    cg_te_angles_pitch   = cgi.Cvar_Get("cg_te_angles_pitch", "0", 0);
    cg_te_angles_yaw     = cgi.Cvar_Get("cg_te_angles_yaw", "0", 0);
    cg_te_angles_roll    = cgi.Cvar_Get("cg_te_angles_roll", "0", 0);
    cg_te_randpitch      = cgi.Cvar_Get("cg_te_randpitch", "No", 0);
    cg_te_randyaw        = cgi.Cvar_Get("cg_te_randyaw", "No", 0);
    cg_te_randroll       = cgi.Cvar_Get("cg_te_randroll", "No", 0);
    cg_te_randvelx       = cgi.Cvar_Get("cg_te_randvelx", "0", 0);
    cg_te_randvely       = cgi.Cvar_Get("cg_te_randvely", "0", 0);
    cg_te_randvelz       = cgi.Cvar_Get("cg_te_randvelz", "0", 0);
    cg_te_rvxtype        = cgi.Cvar_Get("cg_te_rvxtype", "No", 0);
    cg_te_rvytype        = cgi.Cvar_Get("cg_te_rvytype", "No", 0);
    cg_te_rvztype        = cgi.Cvar_Get("cg_te_rvztype", "No", 0);
    cg_te_curnum         = cgi.Cvar_Get("cg_te_curnum", "0", 0);
    cg_te_forwardvel     = cgi.Cvar_Get("cg_te_forwardvel", "0", 0);
}

void CG_ResetTestEmitter(void)
{
    cgi.Cvar_Set("cg_te_xangles", "0");
    cgi.Cvar_Set("cg_te_yangles", "0");
    cgi.Cvar_Set("cg_te_zangles", "0");
    cgi.Cvar_Set("cg_te_life", "1");
    cgi.Cvar_Set("cg_te_alpha", "1");
    cgi.Cvar_Set("cg_te_dietouch", "0");
    cgi.Cvar_Set("cg_te_bouncefactor", "1");
    cgi.Cvar_Set("cg_te_scale", "1");
    cgi.Cvar_Set("cg_te_scalemin", "0");
    cgi.Cvar_Set("cg_te_scalemax", "0");
    cgi.Cvar_Set("cg_te_model", "none");
    cgi.Cvar_Set("cg_te_life", "1");
    cgi.Cvar_Set("cg_te_color_r", "1");
    cgi.Cvar_Set("cg_te_color_g", "1");
    cgi.Cvar_Set("cg_te_color_b", "1");
    cgi.Cvar_Set("cg_te_startcolor_r", "1");
    cgi.Cvar_Set("cg_te_startcolor_g", "1");
    cgi.Cvar_Set("cg_te_startcolor_b", "1");
    cgi.Cvar_Set("cg_te_endcolor_r", "1");
    cgi.Cvar_Set("cg_te_endcolor_g", "1");
    cgi.Cvar_Set("cg_te_endcolor_b", "1");
    cgi.Cvar_Set("cg_te_accel_x", "0");
    cgi.Cvar_Set("cg_te_accel_y", "0");
    cgi.Cvar_Set("cg_te_accel_z", "0");
    cgi.Cvar_Set("cg_te_count", "1");
    cgi.Cvar_Set("cg_te_fade", "0");
    cgi.Cvar_Set("cg_te_fadein", "0");
    cgi.Cvar_Set("cg_te_spawnrate", "1");
    cgi.Cvar_Set("cg_te_scalerate", "0");
    cgi.Cvar_Set("cg_te_circle", "0");
    cgi.Cvar_Set("cg_te_sphere", "0");
    cgi.Cvar_Set("cg_te_insphere", "0");
    cgi.Cvar_Set("cg_te_radius", "0");
    cgi.Cvar_Set("cg_te_swarm", "0");
    cgi.Cvar_Set("cg_te_wavy", "0");
    cgi.Cvar_Set("cg_te_align", "0");
    cgi.Cvar_Set("cg_te_flickeralpha", "0");
    cgi.Cvar_Set("cg_te_collision", "0");
    cgi.Cvar_Set("cg_te_globalfade", "0");
    cgi.Cvar_Set("cg_te_randomroll", "0");
    cgi.Cvar_Set("cg_te_orgoffset_x", "0");
    cgi.Cvar_Set("cg_te_orgoffset_y", "0");
    cgi.Cvar_Set("cg_te_orgoffset_z", "0");
    cgi.Cvar_Set("cg_te_rorgx", "No");
    cgi.Cvar_Set("cg_te_rorgy", "No");
    cgi.Cvar_Set("cg_te_rorgz", "No");
    cgi.Cvar_Set("cg_te_angles_pitch", "0");
    cgi.Cvar_Set("cg_te_angles_yaw", "0");
    cgi.Cvar_Set("cg_te_angles_roll", "0");
    cgi.Cvar_Set("cg_te_randpitch", "No");
    cgi.Cvar_Set("cg_te_randyaw", "No");
    cgi.Cvar_Set("cg_te_randroll", "No");
    cgi.Cvar_Set("cg_te_randvelx", "0");
    cgi.Cvar_Set("cg_te_randvely", "0");
    cgi.Cvar_Set("cg_te_randvelz", "0");
    cgi.Cvar_Set("cg_te_rvxtype", "No");
    cgi.Cvar_Set("cg_te_rvytype", "No");
    cgi.Cvar_Set("cg_te_rvztype", "No");
    cgi.Cvar_Set("cg_te_curnum", "0");
    cgi.Cvar_Set("cg_te_forwardvel", "0");
}
