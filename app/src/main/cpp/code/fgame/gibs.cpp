

#include "gibs.h"
#include "decals.h"
#include "level.h"
#include "g_phys.h"

Event EV_ThrowGib (	"throwgib",
	EV_DEFAULT,
	"eff",
	"ent velocity scale",
	"Throw a gib."
);



CLASS_DECLARATION(Entity, Gib, "gib") {
    {&EV_ThrowGib, &Gib::Throw },
    {&EV_Touch,    &Gib::Splat },
    {&EV_Stop,     &Gib::Stop  },
    {&EV_Damage,   &Gib::Damage},
    {NULL,         NULL        }
};

Gib::Gib(
    str      name,
    qboolean blood_trail,
    str      bloodtrailname,
    str      bloodspurtname,
    str      bloodsplatname,
    float    bloodsplatsize,
    float    pitch
)
{
    animate = new Animate(this);
    setSize(Vector("0 0 0"), Vector("0 0 0"));

    if (name.length()) {
        setModel(name.c_str());
    }

    setMoveType(MOVETYPE_GIB);
    setSolidType(SOLID_BBOX);
    takedamage = DAMAGE_YES;
    sprayed    = false;
    fadesplat  = true;
    scale      = 2.0f;

    next_bleed_time = 0;

    final_pitch = pitch;

    if (blood_trail) {
        blood = new Entity(ENTITY_CREATE_FLAG_ANIMATE);

        if (bloodtrailname.length()) {
            blood->setModel(bloodtrailname.c_str());
        }

        blood->setMoveType(MOVETYPE_BOUNCE);
        blood->setSolidType(SOLID_NOT);
        blood->bind(this);


        if (bloodspurtname.length()) {
            blood_spurt_name = bloodspurtname;
        }


        if (bloodsplatname.length()) {
            blood_splat_name = bloodsplatname;
        }

        blood_splat_size = bloodsplatsize;
    } else {
        blood = NULL;
    }

    Sound("snd_decap", CHAN_BODY, 1, 300);
}

Gib::~Gib()
{
    if (blood) {
        blood->PostEvent(EV_Remove, 0);
    }
    blood = NULL;
}

Gib::Gib()
{
    animate = new Animate(this);
    if (LoadingSavegame) {
        return;
    }

    setSize(Vector("0 0 0"), Vector("0 0 0"));
    setModel("gib1.def");
    setMoveType(MOVETYPE_GIB);
    setSolidType(SOLID_BBOX);
    sprayed   = 0;
    fadesplat = true;
    scale     = 2.0f;
}

void Gib::Stop(Event *ev)
{
    if (blood) {
        blood->PostEvent(EV_Remove, 0);
    }
    blood = NULL;
}

void Gib::Splat(Event *ev)
{
    if (g_gametype->integer != GT_SINGLE_PLAYER) {
        return;
    }

    if (sprayed > 3) {
        return;
    }

    sprayed++;
    scale -= 0.2f;


    avelocity = vec_zero;

    if (final_pitch != NO_FINAL_PITCH) {
        angles[PITCH] = final_pitch;
        setAngles(angles);
    }

    SprayBlood(origin);
    Sound("snd_gibhit");
}

void Gib::Damage(Event *ev)
{
    Vector  direction;
    Entity *bloodEnt;
    Vector  dir;

    if (next_bleed_time > level.time) {
        return;
    }

    direction = ev->GetVector(5);


    if (blood_spurt_name.length() > 0) {
        bloodEnt = new Entity(ENTITY_CREATE_FLAG_ANIMATE);
        bloodEnt->setModel(blood_spurt_name.c_str());

        dir[0] = -direction[0];
        dir[1] = -direction[1];
        dir[2] = -direction[2];

        bloodEnt->angles = dir.toAngles();
        bloodEnt->setAngles(bloodEnt->angles);

        bloodEnt->setOrigin(centroid);
        bloodEnt->origin.copyTo(bloodEnt->edict->s.origin2);
        bloodEnt->setSolidType(SOLID_NOT);

        bloodEnt->PostEvent(EV_Remove, 1);

        next_bleed_time = level.time + 0.5;
    }
}

void Gib::SprayBlood(Vector start)
{
    Vector  norm;
    trace_t trace;
    Vector  trace_end;

    trace_end = velocity;
    trace_end.normalize();
    trace_end *= 1000;
    trace_end += start;

    trace = G_Trace(start, vec_zero, vec_zero, trace_end, this, MASK_SOLID, false, "Gib::SprayBlood");

    if (HitSky(&level.impact_trace) || (!level.impact_trace.ent) || (level.impact_trace.ent->solid != SOLID_BSP)) {
        return;
    }

    if (blood_splat_name.length()) {
        Decal *decal = new Decal;
        decal->setShader(blood_splat_name);
        decal->setOrigin(Vector(trace.endpos) + (Vector(level.impact_trace.plane.normal) * 0.2f));
        decal->setDirection(level.impact_trace.plane.normal);
        decal->setOrientation("random");
        decal->setRadius(blood_splat_size + G_Random(blood_splat_size));
    }
}

void Gib::ClipGibVelocity(void)
{
    if (velocity[0] < -400) {
        velocity[0] = -400;
    } else if (velocity[0] > 400) {
        velocity[0] = 400;
    }
    if (velocity[1] < -400) {
        velocity[1] = -400;
    } else if (velocity[1] > 400) {
        velocity[1] = 400;
    }
    if (velocity[2] < 200) {
        velocity[2] = 200; 
    } else if (velocity[2] > 600) {
        velocity[2] = 600;
    }
}

void Gib::SetVelocity(float damage)
{
    velocity[0] = 100.0 * crandom();
    velocity[1] = 100.0 * crandom();
    velocity[2] = 200.0 + 100.0 * random();

    avelocity = Vector(G_Random(600), G_Random(600), G_Random(600));

    if ((damage < -150) && (G_Random() > 0.95f)) {
        velocity *= 2.0f;
    } else if (damage < -100) {
        velocity *= 1.5f;
    }

    ClipGibVelocity();
}

void Gib::Throw(Event *ev)
{
    Entity *ent;

    ent = ev->GetEntity(1);
    setOrigin(ent->centroid);
    origin.copyTo(edict->s.origin2);
    SetVelocity(ev->GetInteger(2));
    edict->s.scale = ev->GetFloat(3);
    PostEvent(EV_FadeOut, 10 + G_Random(5));
}

void CreateGibs(Entity *ent, float damage, float scale, int num, const char *modelname) {}
