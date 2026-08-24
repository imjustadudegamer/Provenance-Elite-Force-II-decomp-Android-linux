

#include "nature.h"




Event EV_PuffDaddy_Idle
( 
	"idle",
	EV_DEFAULT,
	NULL,
	NULL,
	"Animates the puff daddy."
);

Event EV_Emitter_EmitterName (	"emitter",
	EV_DEFAULT,
	"s",
	"name",
	"Emitter to use"
);








CLASS_DECLARATION(Entity, Emitter, "func_emitter") {
    {&EV_Emitter_EmitterName, &Emitter::EmitterName},
    {NULL,                    NULL                 }
};

Emitter::Emitter()
{
    edict->s.eType = ET_EMITTER;
}

void Emitter::setEmitter(str name)
{
    emitterName      = name;
    edict->s.tag_num = gi.imageindex(emitterName);
}

void Emitter::EmitterName(Event *ev)
{
    setEmitter(ev->GetString(1));
}


CLASS_DECLARATION(Entity, Rain, "func_rain") {
    {NULL, NULL}
};

Rain::Rain()
{
    if (LoadingSavegame) {
        return;
    }
    setSolidType(SOLID_NOT);
    edict->s.eType = ET_RAIN;
    setRainName("defaultrain");
}

void Rain::setRainName(str name)
{
    rainName         = name;
    edict->s.tag_num = gi.imageindex(rainName);
}



CLASS_DECLARATION(Entity, PuffDaddy, "plant_puffdaddy") {
    {&EV_Touch,          &PuffDaddy::Touch},
    {&EV_PuffDaddy_Idle, &PuffDaddy::Idle },
    {NULL,               NULL             }
};

void PuffDaddy::Idle(Event *ev)
{
    animate->RandomAnimate("idle");
}

void PuffDaddy::Touch(Event *ev)
{
    Entity *other;

    other = ev->GetEntity(1);

    if (!other->inheritsFrom("Sentient")) {
        return;
    }

    animate->RandomAnimate("touch", EV_PuffDaddy_Idle);

    SurfaceCommand("puffdaddy", "+nodraw");
    setSolidType(SOLID_NOT);
}

PuffDaddy::PuffDaddy()
{
    animate = new Animate(this);
    if (LoadingSavegame) {
        return;
    }
    setSolidType(SOLID_TRIGGER);
    edict->s.eType = ET_MODELANIM;
    setModel("plant_puffdaddy.tik");
    PostEvent(EV_Show, 0);
}
