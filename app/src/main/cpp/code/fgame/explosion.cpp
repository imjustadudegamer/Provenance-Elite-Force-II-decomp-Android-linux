

#include "g_local.h"
#include "g_phys.h"
#include "actor.h"
#include "entity.h"
#include "trigger.h"
#include "explosion.h"
#include "weaputils.h"
#include "earthquake.h"
#include "../qcommon/gameplaymanager.h"
#define PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_probe.h"




Event EV_Exploder_SetDmg
(
	"dmg",
	EV_SCRIPTONLY,
	"i",
	"damage",
	"Sets the damage the explosion does."
);

Event EV_ExplodeObject_SetSeverity (	"severity",
	EV_DEFAULT,
	"f",
	"newSeverity",
	"How violently the debris should be ejected."
);

extern Event EV_Door_SetDmg;






#define MULTI_USE     (1 << 0)
#define RANDOM_TIME   (1 << 1)
#ifdef VISIBLE
#undef VISIBLE
#endif
#define VISIBLE       (1 << 2)  
#define RANDOM_SCALE  (1 << 3)
#define NO_EXPLOSIONS (1 << 4)

extern Event EV_Earthquake_SetDuration;

extern Event EV_Trigger_SetWait;

Event EV_Exploder_SetDuration
(
	"duration",
	EV_SCRIPTONLY,
	"f",
	"duration",
	"Sets the duration of the explosion."
);
Event EV_Exploder_SetWait (
    "wait",
    EV_SCRIPTONLY,
    "f",
    "explodewait",
    "Sets the wait time of the explosion."
);
Event EV_Exploder_SetRandom (    "random",
    EV_DEFAULT,
    "f",
    "randomness",
    "Sets the randomness value of the explosion.");


extern Event EV_SetGameplayDamage;

void CreateExplosion(
    Vector      pos,
    float       damage,
    Entity     *inflictor,
    Entity     *attacker,
    Entity     *ignore,
    const char *explosionModel,
    float       scale,
    float       radius
)
{
    Explosion *explosion;
    Event     *ev;

    assert(inflictor);

    if (!inflictor) {
        return;
    }

    if (!attacker) {
        attacker = world;
    }

    if (!explosionModel) {
        explosionModel = "fx_explosion.tik";
    }

    explosion = new Explosion;

    explosion->setModel(explosionModel);

    explosion->setSolidType(SOLID_NOT);
    explosion->CancelEventsOfType(EV_ProcessInitCommands);
    explosion->ProcessInitCommands(explosion->edict->s.modelindex);

    explosion->owner             = attacker->entnum;
    explosion->edict->ownerNum = attacker->entnum;
    explosion->setMoveType(MOVETYPE_FLYMISSILE);
    explosion->edict->clipmask = MASK_PROJECTILE;
    explosion->setSize(explosion->mins, explosion->maxs);
    explosion->setOrigin(pos);
    explosion->origin.copyTo(explosion->edict->s.origin2);

    explosion->setScale(scale);

    if (explosion->dlight_radius) {
        G_SetConstantLight(
            &explosion->edict->s.constantLight,
            &explosion->dlight_color[0],
            &explosion->dlight_color[1],
            &explosion->dlight_color[2],
            &explosion->dlight_radius
        );
    }

    explosion->BroadcastSound();

    explosion->animate->RandomAnimate("idle", EV_StopAnimating);
    RadiusDamage(inflictor, attacker, damage, ignore, MOD_EXPLOSION, radius);  

    if (explosion->life) {
        ev = new Event(EV_Remove);
        explosion->PostEvent(ev, explosion->life);
    }
}


CLASS_DECLARATION(Trigger, Exploder, "func_exploder") {
    {&EV_Trigger_Effect,     &Exploder::MakeExplosion},
    {&EV_Touch,              NULL                    },
    {&EV_Exploder_SetDmg,    &Exploder::SetDmg       },
    {&EV_SetGameplayDamage,  &Exploder::setDamage    },
    {&EV_SetHealth,          &Exploder::SetHealth     },
    {NULL,                   NULL                    }
};

void Exploder::SetHealth(Event *ev)
{
    Trigger::EventSetHealth(ev);
    triggerondeath = true;
}

void Exploder::MakeExplosion(Event *ev)
{
    CreateExplosion(origin, damage, this, ev->GetEntity(1), this);
}

Exploder::Exploder()
{
    damage    = 120;
    respondto = TRIGGER_PLAYERS | TRIGGER_MONSTERS | TRIGGER_PROJECTILES;
}

void Exploder::SetDmg(Event *ev)
{
    damage = ev->GetInteger(1);
    if (damage < 0) {
        damage = 0;
    }
}

void Exploder::setDamage(Event *ev)
{
    if (ev->NumArgs() < 1) {
        return;
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm->hasFormula("OffensiveDamage")) {
        return;
    }

    str damagestr = ev->GetString(1);
    float damagemod = 1.0f;
    str defineVal = gpm->getDefine(damagestr);
    if (defineVal.length()) {
        damagemod = (float)atof(defineVal);
    }
    GameplayFormulaData fd(this, 0, 0, "");
    float finaldamage = gpm->calculate("OffensiveDamage", fd, damagemod);
    Event *newev = new Event(EV_Exploder_SetDmg);
    newev->AddInteger((int)finaldamage);
    ProcessEvent(newev);
}


CLASS_DECLARATION(Trigger, MultiExploder, "func_multi_exploder") {
    {&EV_Touch,                NULL                         },
    {&EV_Trigger_Effect,       &MultiExploder::MakeExplosion},
    {&EV_Exploder_SetDmg,      &MultiExploder::SetDmg       },
    {&EV_Exploder_SetDuration, &MultiExploder::SetDuration  },
    {&EV_SetGameplayDamage,    &MultiExploder::setDamage    },
    {&EV_Earthquake_SetDuration, &MultiExploder::SetDuration  },
    {&EV_Exploder_SetWait,     &MultiExploder::SetWait      },
    {&EV_Exploder_SetRandom,   &MultiExploder::SetRandom    },
    {&EV_SetHealth,            &MultiExploder::SetHealth    },
    {NULL,                     NULL                         }
};

void MultiExploder::SetHealth(Event *ev)
{
    Trigger::EventSetHealth(ev);
    triggerondeath = true;
}

void MultiExploder::MakeExplosion(Event *ev)
{
    Vector  pos;
    float   t, scale;
    Entity *other;
    Event  *event;

    other = ev->GetEntity(1);

    if (!other) {
        other = world;
    }

    trigger_time = -1;

    if (!explode_time) {
        hideModel();
        explode_time = level.time + duration;
    }

    if (spawnflags & RANDOM_TIME) {
        t = explodewait * (1 + G_CRandom(randomness));
    } else {
        t = explodewait;
    }

    event = new Event(EV_Trigger_Effect);
    event->AddEntity(other);
    PostEvent(event, t);

    if (level.time > explode_time) {
        if (spawnflags & MULTI_USE) {
            trigger_time = level.time + 0.5f;
            explode_time = 0;
            CancelEventsOfType(EV_Trigger_Effect);
            health = max_health;
            return;
        } else {
            PostEvent(EV_Remove, 0);
            return;
        }
    }

    pos[0] = absmin[0] + G_Random(absmax[0] - absmin[0]);
    pos[1] = absmin[1] + G_Random(absmax[1] - absmin[1]);
    pos[2] = absmin[2] + G_Random(absmax[2] - absmin[2]);

    if (spawnflags & RANDOM_SCALE) {
        scale = edict->s.scale * 0.25f;
        scale += G_Random(3 * scale);
    } else {
        scale = 1.0f;
    }

    CreateExplosion(pos, damage, this, other, this, NULL, scale);
}

MultiExploder::MultiExploder()
{
    if (LoadingSavegame) {
        return;
    }

    damage       = 120;
    duration     = 1.0;
    explodewait  = 0.25;
    randomness   = 0.25;
    explode_time = 0;

    if (spawnflags & VISIBLE) {
        PostEvent(EV_Show, EV_POSTSPAWN);
    } else {
        PostEvent(EV_Hide, EV_POSTSPAWN);
    }

    count = -1;

    respondto = TRIGGER_PLAYERS | TRIGGER_MONSTERS | TRIGGER_PROJECTILES;
}

void MultiExploder::SetDmg(Event *ev)
{
    damage = ev->GetInteger(1);
    if (damage < 0) {
        damage = 0;
    }
}

void MultiExploder::setDamage(Event *ev)
{
    if (ev->NumArgs() < 1) {
        return;
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm->hasFormula("OffensiveDamage")) {
        return;
    }

    str damagestr = ev->GetString(1);
    float damagemod = 1.0f;
    str defineVal = gpm->getDefine(damagestr);
    if (defineVal.length()) {
        damagemod = (float)atof(defineVal);
    }
    GameplayFormulaData fd(this, 0, 0, "");
    float finaldamage = gpm->calculate("OffensiveDamage", fd, damagemod);
    Event *newev = new Event(EV_Exploder_SetDmg);
    newev->AddInteger((int)finaldamage);
    ProcessEvent(newev);
}

void MultiExploder::SetDuration(Event *ev)
{
    duration = ev->GetFloat(1);
}

void MultiExploder::SetWait(Event *ev)
{
    explodewait = ev->GetFloat(1);
}

void MultiExploder::SetRandom(Event *ev)
{
    randomness = ev->GetFloat(1);
}

#define METAL_DEBRIS (1 << 5)
#define ROCK_DEBRIS  (1 << 6)
#define NOTSOLID     (1 << 7)


    

    Event EV_ExplodeObject_SetDebrisModel (
    "debrismodel", EV_DEFAULT, "s", "debrisModel", "What kind of debris to spawn when triggered.");

    Event EV_ExplodeObject_SetDebrisAmount (
    "amount", EV_DEFAULT, "i", "amountOfDebris", "How much debris to spawn each time.");

    Event EV_ExplodeObject_SetupExplodeTiki (
    "explodetiki",
    EV_TIKIONLY,
    NULL,
    NULL,
    "Specifies that this is an exploding tiki file (visible and solid).  Use only in tiki files");

    Event EV_ExplodeObject_SetSound (
    "setsound", EV_DEFAULT, "s", "soundname", "Sound played when the debris explosion occurs");
extern Event EV_ExplodeObject_SetExplosion;

// below it are not used by any shipped map, but they are part of the same retail
    Event EV_ExplodeObject_UseEarthquake (
    "explodeObject_UseEarthquake",
    EV_DEFAULT,
    NULL,
    NULL,
    "Causes the object to spawn an earthquake when it explodes");
    Event EV_ExplodeObject_SetEarthquakeDuration (
    "explodeObject_EarthquakeDuration",
    EV_DEFAULT,
    "f",
    "earthquakeDuration",
    "Sets the earthquake duration");
    Event EV_ExplodeObject_SetEarthquakeMagnitude (
    "explodeObject_EarthquakeMagnitude",
    EV_DEFAULT,
    "f",
    "earthquakeMagnitude",
    "Sets the earthquake magnitude");
    Event EV_ExplodeObject_SetEarthquakeDistance (
    "explodeObject_EarthquakeDistance",
    EV_DEFAULT,
    "f",
    "earthquakeDistance",
    "Sets the earthquake distance");

Event EV_ExplodeObject_SetExplosion (    "doexplosion",
    EV_DEFAULT,
    "SF",
    "explosionModel explosionRadius",
    "Causes the object to explode (use dmg to set the damage)");

CLASS_DECLARATION(MultiExploder, ExplodeObject, "func_explodeobject") {
    {&EV_Trigger_Effect,                         &ExplodeObject::MakeExplosion         },
    {&EV_Damage,                                 &ExplodeObject::Damage                },
    {&EV_Touch,                                  NULL                                  },
    {&EV_ExplodeObject_SetSeverity,              &ExplodeObject::SetSeverity           },
    {&EV_ExplodeObject_SetDebrisModel,           &ExplodeObject::SetDebrisModel        },
    {&EV_ExplodeObject_SetDebrisAmount,          &ExplodeObject::SetDebrisAmount       },
    {&EV_ExplodeObject_SetupExplodeTiki,         &ExplodeObject::ExplodeTiki           },
    {&EV_ExplodeObject_SetSound,                 &ExplodeObject::SetSound              },
    {&EV_ExplodeObject_SetExplosion,             &ExplodeObject::SetExplosion          },
    {&EV_ExplodeObject_UseEarthquake,            &ExplodeObject::setSpawnEarthquake    },
    {&EV_ExplodeObject_SetEarthquakeDuration,    &ExplodeObject::setEarthquakeDuration },
    {&EV_ExplodeObject_SetEarthquakeMagnitude,   &ExplodeObject::setEarthquakeMagnitude},
    {&EV_ExplodeObject_SetEarthquakeDistance,    &ExplodeObject::setEarthquakeDistance },
    {NULL,                                       NULL                                  }
};
void ExplodeObject::Damage(Event *ev)
{
    Entity::DamageEvent(ev);
}

void ExplodeObject::ExplodeTiki(Event *ev)
{
    spawnflags |= VISIBLE;
    spawnflags |= NO_EXPLOSIONS;
    spawnflags |= NOTSOLID;
    PostEvent(EV_Show, EV_POSTSPAWN);
    setSolidType(SOLID_BBOX);
}

void ExplodeObject::SetSound(Event *ev)
{
    if (ev->NumArgs() > 0) {
        debrissound = ev->GetString(1);
    }
}

void ExplodeObject::SetExplosion(Event *ev)
{
    if (ev->NumArgs() > 0) {
        explosionmodel = ev->GetString(1);
    }
    if (ev->NumArgs() > 1) {
        explosionradius = ev->GetFloat(2);
    }

    spawnflags &= ~NO_EXPLOSIONS;
}

void ExplodeObject::setSpawnEarthquake(Event *ev)
{
    _spawnEarthquake = true;
}

void ExplodeObject::setEarthquakeDuration(Event *ev)
{
    _spawnEarthquake    = true;
    _earthquakeDuration = ev->GetFloat(1);
}

void ExplodeObject::setEarthquakeMagnitude(Event *ev)
{
    _spawnEarthquake     = true;
    _earthquakeMagnitude = ev->GetFloat(1);
}

void ExplodeObject::setEarthquakeDistance(Event *ev)
{
    _spawnEarthquake    = true;
    _earthquakeDistance = ev->GetFloat(1);
}

void ExplodeObject::SetDebrisModel(Event *ev)
{
    char        string[1024];
    const char *ptr;

    Q_strncpyz(string, ev->GetString(1), sizeof(string));
    ptr = strtok(string, " ");
    while (ptr) {
        debrismodels.AddUniqueObject(str(ptr));
        CacheResource(ptr, this);
        ptr = strtok(NULL, " ");
    }
}

void ExplodeObject::SetSeverity(Event *ev)
{
    severity = ev->GetFloat(1);
}

void ExplodeObject::SetDebrisAmount(Event *ev)
{
    debrisamount = ev->GetInteger(1);
}

void ExplodeObject::MakeExplosion(Event *ev)
{
    Vector  pos;
    float   t, scale;
    Entity *other;
    Event  *event;

    other = ev->GetEntity(1);

    {
        static int uivk_dfx_n;
        if (PROBE_On() && PROBE_Throttle(&uivk_dfx_n, 32, 32)) {
            PROBE("DFX:explodeobject", "ent=%d targetname='%s' model='%s' explmodel='%s' debris=%d noexpl=%d\n",
                       entnum, targetname.c_str(), model.c_str(), explosionmodel.c_str(),
                       debrismodels.NumObjects(), (spawnflags & NO_EXPLOSIONS) ? 1 : 0);
        }
    }

    if (!other) {
        other = world;
    }

    trigger_time = -1;

    if (!explode_time) {
        setSolidType(SOLID_NOT);
        hideModel();
        explode_time = level.time + duration;
    }

    if (_spawnEarthquake && !_haveSpawnedEarthquake) {
        Event *earthquakeEvent = new Event(EV_CreateEarthquake);

        earthquakeEvent->AddFloat(_earthquakeMagnitude);
        earthquakeEvent->AddFloat(_earthquakeDuration);
        earthquakeEvent->AddFloat(_earthquakeDistance);

        ProcessEvent(earthquakeEvent);

        _haveSpawnedEarthquake = true;
    }

    if (spawnflags & RANDOM_TIME) {
        t = explodewait * (1 + G_CRandom(randomness));
    } else {
        t = explodewait;
    }

    event = new Event(EV_Trigger_Effect);
    event->AddEntity(other);
    PostEvent(event, t);

    if (level.time > explode_time) {
        if (spawnflags & MULTI_USE) {
            trigger_time = level.time + 0.5f;
            explode_time = 0;
            CancelEventsOfType(EV_Trigger_Effect);
            health = max_health;
            if (health) {
                setSolidType(SOLID_BBOX);
            }
            if (spawnflags & VISIBLE) {
                PostEvent(EV_Show, 0.5f);
            }
            return;
        } else {
            PostEvent(EV_Remove, 0);
            return;
        }
    }

    pos[0] = absmin[0] + G_Random(absmax[0] - absmin[0]);
    pos[1] = absmin[1] + G_Random(absmax[1] - absmin[1]);
    pos[2] = absmin[2] + G_Random(absmax[2] - absmin[2]);

    if (spawnflags & RANDOM_SCALE) {
        scale = edict->s.scale * 0.25f;
        scale += G_Random(3 * scale);
    } else {
        scale = 1.0f;
    }

    if (!(spawnflags & NO_EXPLOSIONS)) {
        CreateExplosion(pos, damage, this, other, this, explosionmodel.c_str(), scale, explosionradius);
    }
    if (debrismodels.NumObjects()) {
        TossObject *to;
        int         i;

        if (debrissound.length()) {
            Sound(debrissound);
        }

        for (i = 0; i < debrisamount; i++) {
            int num;

            if (spawnflags & RANDOM_SCALE) {
                scale = edict->s.scale * 0.25f;
                scale += G_Random(3 * scale);
            } else {
                scale = 1.0f;
            }

            num = G_Random(debrismodels.NumObjects()) + 1;
            to  = new TossObject(debrismodels.ObjectAt(num));
            to->setScale(scale);
            to->setOrigin(pos);
            to->SetVelocity(severity);
            if (spawnflags & NOTSOLID) {
                to->setSolidType(SOLID_NOT);
            }
            pos[0] = absmin[0] + G_Random(absmax[0] - absmin[0]);
            pos[1] = absmin[1] + G_Random(absmax[1] - absmin[1]);
            pos[2] = absmin[2] + G_Random(absmax[2] - absmin[2]);
        }
    }
}

ExplodeObject::ExplodeObject()
{
    if (!LoadingSavegame) {
        duration    = 1;
        explodewait = 0.25f;
        severity    = 1.0f;
        explosionradius = 0.0f;
        debrismodels.ClearObjectList();
        explosionmodel = "fx_explosion.tik";
        debrisamount   = 2;
        if (spawnflags & METAL_DEBRIS) {
            debrismodels.AddUniqueObject(str("fx/fx_debris_metal1.tik"));
            debrismodels.AddUniqueObject(str("fx/fx_debris_metal2.tik"));
            debrismodels.AddUniqueObject(str("fx/fx_debris_metal3.tik"));
            CacheResource("fx/fx_debris_metal1.tik", this);
            CacheResource("fx/fx_debris_metal2.tik", this);
            CacheResource("fx/fx_debris_metal3.tik", this);
        } else if (spawnflags & ROCK_DEBRIS) {
            debrismodels.AddUniqueObject(str("fx/fx_debris_rock1.tik"));
            debrismodels.AddUniqueObject(str("fx/fx_debris_rock2.tik"));
            debrismodels.AddUniqueObject(str("fx/fx_debris_rock3.tik"));
            debrismodels.AddUniqueObject(str("fx/fx_debris_rock4.tik"));
            CacheResource("fx/fx_debris_rock1.tik", this);
            CacheResource("fx/fx_debris_rock2.tik", this);
            CacheResource("fx/fx_debris_rock3.tik", this);
            CacheResource("fx/fx_debris_rock4.tik", this);
        }
    }

    _spawnEarthquake       = false;
    _haveSpawnedEarthquake = false;
    _earthquakeMagnitude   = 1.0f;
    _earthquakeDuration    = 1.0f;
    _earthquakeDistance    = 500.0f;
}
