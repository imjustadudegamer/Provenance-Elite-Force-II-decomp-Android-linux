
#include "shrapnelbomb.h"
#include "level.h"
#include "g_utils.h"

Event EV_ShrapnelBomb_ShrapnelModel (	"setshrapnelmodel",
	EV_TIKIONLY,
	"s",
	"modelname",
	"The model of the shrapnel pieces that are spawned when the main bomb explodes"
);


    Event EV_ShrapnelBomb_ShrapnelCount (
    "setshrapnelcount",
    EV_TIKIONLY,
    "i",
    "count",
    "The number of shrapnel pieces to spawn"
);

// SDK shrapnelbomb.cpp:37-44.  Note that retail does NOT add a row for
CLASS_DECLARATION(Projectile, ShrapnelBomb, NULL) {
    {&EV_ShrapnelBomb_ShrapnelCount, &ShrapnelBomb::SetShrapnelCount},
    {&EV_ShrapnelBomb_ShrapnelModel, &ShrapnelBomb::SetShrapnelModel},
    {NULL,                           NULL                           }
};

ShrapnelBomb::ShrapnelBomb()
{
    shrapnelCount   = 5;
    _splitOnDescent = true;
    _randomSpread   = true;
    _startTime      = level.time;

    if (LoadingSavegame) {
        return;
    }

    flags |= FL_THINK;
}

void ShrapnelBomb::Think(void)
{
    if (_splitOnDescent && (level.time >= _startTime + minlife)) {
        if (velocity.z < 0.0f) {
            Explode();
        }
    }
}

void ShrapnelBomb::Explode(Event *ev)
{
    Explode();
}

void ShrapnelBomb::Explode(void)
{
    Vector dir;
    int    i;
    Vector dirAngles;
    Vector left;

    for (i = 0; i < shrapnelCount; i++) {
        dir = velocity;
        dir.normalize();

        dirAngles = dir.toAngles();

        dirAngles.AngleVectors(NULL, &left, NULL);

        if (_randomSpread) {
            dir += left * G_CRandom(.5f);
        } else {
            dir += left * ((i * 0.1f) - ((shrapnelCount - 1) * 0.1f / 2.0f));
            dir += left * 0.05f;
        }

        dir.normalize();

        ProjectileAttack(this->origin, dir, this, shrapnelModel, 1.0f, (velocity.length() / 2.0f));
    }

    animate->RandomAnimate("explode");

    PostEvent(EV_Remove, 0.0f);
}

void ShrapnelBomb::SetShrapnelCount(Event *ev)
{
    shrapnelCount = ev->GetInteger(1);
}

void ShrapnelBomb::SetShrapnelModel(Event *ev)
{
    shrapnelModel = ev->GetString(1);
}



void Projectile::SetOwnerControl( Event *ev )
{
	ownercontrol = ev->GetBoolean( 1 );
	
	if ( ownercontrol )
	{
		turnThinkOn();
	}
}


void Projectile::destroyed( Event *ev )
{
	Entity *shooter;

	shooter = ev->GetEntity( 1 );

	if ( shooter )
	{
		owner           = shooter->entnum;
		edict->ownerNum = shooter->entnum;
	}

	ProcessEvent( EV_Projectile_Explode );
}


void Projectile::setScaleByCharge( Event *ev )
{
	_scaleByCharge = true;

	_minScaleFromCharge = ev->GetFloat( 1 );
	_maxScaleFromCharge = ev->GetFloat( 2 );
}


void Projectile::setScaleExplosion( Event *ev )
{
	_scaleExplosion = true;
}




Projectile::~Projectile()
{
	detach();
}

