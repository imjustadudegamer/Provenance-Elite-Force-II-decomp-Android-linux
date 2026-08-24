
#include "equipment.h"
#include "player.h"
#include "weaputils.h"       
#include "actor.h"           
#include "../qcommon/gameplaymanager.h" 
#include "worldspawn.h"

Event EV_AirStrike (	"airstrike",
	EV_CODEONLY,
	NULL,
	NULL,
	"call airstrike from equipment"
);


#define EQUIP_SCAN_INITIAL_DELAY 0.25f
#define EQUIP_SCAN_REPEAT_DELAY  0.05f
#define DETAILED_SCAN_TIME       1.5f
#define MAX_TARGET_DISTANCE         600.0f
#define MAX_TARGET_DISTANCE_SQUARED (MAX_TARGET_DISTANCE * MAX_TARGET_DISTANCE)

extern Event EV_Player_DoUse;

Event EV_ScanStart (    "scanstart",
    EV_TIKIONLY,
    NULL,
    NULL,
    "The start of the scan");
Event EV_ScanEnd (    "scanend",
    EV_TIKIONLY,
    NULL,
    NULL,
    "The end of the scan");
Event EV_Scanner (    "hasscanner",
    EV_TIKIONLY,
    NULL,
    NULL,
    "The equipment has a scanner");
Event EV_Radar (    "hasradar",
    EV_TIKIONLY,
    NULL,
    NULL,
    "The equipment has a radar");
Event EV_Equipment_ChangeMode (    "changemode",
    EV_TIKIONLY,
    NULL,
    NULL,
    "Change to the next mode.");
Event EV_Scan (    "scan",
    EV_TIKIONLY,
    NULL,
    NULL,
    "Scan");
Event EV_Equipment_SetTypeName (    "typeName",
    EV_TIKIONLY,
    "s",
    "typeName",
    "Sets the type name of this equipment");

    Event EV_Equipment_HasModes (
    "hasmodes",
    EV_TIKIONLY,
    "sSSSSSSSSS",
    "mode1 mode2 mode3 mode4 mode5 mode6 mode7 mode8 mode9 mode10",
    "Specify the modes this equipment has."
);

CLASS_DECLARATION(Weapon, Equipment, NULL) {
    {&EV_AirStrike,               &Equipment::airStrike  },
    {&EV_ScanStart,               &Equipment::scanStart  },
    {&EV_ScanEnd,                 &Equipment::scanEnd    },
    {&EV_Scanner,                 &Equipment::setScanner },
    {&EV_Radar,                   &Equipment::setRadar   },
    {&EV_Equipment_HasModes,      &Equipment::hasModes   },
    {&EV_Equipment_ChangeMode,    &Equipment::changeMode },
    {&EV_Equipment_SetTypeName,   &Equipment::setTypeName},
    {&EV_Scan,                    &Equipment::scan       },
    {NULL,                        NULL                   }
};

Equipment::Equipment()
{
    init();
}

Equipment::Equipment(const char *file)
    : Weapon(file)
{
    init();
}

Equipment::~Equipment()
{
    _modes.FreeObjectList();
}

void Equipment::init(void)
{
    scanner       = false;
    scanning      = false;
    radar         = false;
    _currentMode  = 1;
    _lastMode     = 1;
    _active       = false;
    _scanEndFrame = -1;
    _nextUseTime  = 0.0f;

    scanTime       = 0;
    _scannedEntity = NULL;

    flags |= FL_THINK;
}

void Equipment::Think(void)
{
    Weapon::Think();

    if (_modes.NumObjects() > 0 && _currentMode >= 1 && _currentMode <= _modes.NumObjects()) {
        str& modeName = _modes.ObjectAt(_currentMode);

        if (_currentMode != 1 && world && world->isAnyViewModeAvailable()
            && !world->isViewModeAvailable(modeName.c_str())) {
            Event *newEvent = new Event(EV_Equipment_ChangeMode);
            newEvent->AddInteger(1);
            ProcessEvent(newEvent);
        }
    }

    updateRadarState();
}



void Equipment::updateRadarState(void)
{
    Player *player = NULL;

    if (owner && owner->isSubclassOf(Player)) {
        player = (Player *)(Sentient *)owner;
    }

    if (player && _active == qtrue) {
        player->client->ps.pm_flags &= ~(PMF_RADAR_MODE | PMF_SCANNER);
        if (hasRadar()) {
            player->client->ps.pm_flags |= PMF_RADAR_MODE;
        }
        if (hasScanner()) {
            player->client->ps.pm_flags |= PMF_SCANNER;
        }
    }
}

// : 0' edge is not retail either: retail selects the tricorder's scan surfaces with
// Entity::SurfaceModelEvent.  s.skinNum is a different field that retail never

void Equipment::scanStart(Event *ev)
{
    if (scanner) {
        scanning = true;
    }

    if (_scanEndFrame == level.framenum) {
        PostEvent(EV_Scan, EQUIP_SCAN_REPEAT_DELAY);
        return;
    }

    PostEvent(EV_Scan, EQUIP_SCAN_INITIAL_DELAY);

}

void Equipment::scanEnd(Event *ev)
{
    if (scanner) {
        scanning = false;
    }

    _scanEndFrame = level.framenum;

    if (shootingSkin) {
        ChangeSkin(shootingSkin, false);
    }

    CancelEventsOfType(EV_Scan);
}

void Equipment::scan(Event *ev)
{
    if (owner && owner->isSubclassOf(Player)) {
        Player *player = (Player *)(Sentient *)owner;
        Event *newEvent = new Event(EV_Player_DoUse);
        newEvent->AddEntity(this);
        player->ProcessEvent(newEvent);
    }

    CancelEventsOfType(EV_Scan);
    PostEvent(EV_Scan, EQUIP_SCAN_REPEAT_DELAY);
}

void Equipment::airStrike(Event *ev)
{
    str    modeName;
    Player *player;
    str    airStrikeSound;

    if (!scanner || !scanning)
        return;

    if ((_currentMode > _modes.NumObjects()) || (_currentMode < 1))
        return;

    modeName = _modes.ObjectAt(_currentMode);

    if (Q_stricmp(modeName.c_str(), "torpedostrike") != 0)
        return;

    assert(owner);

    if (!owner || !owner->isSubclassOf(Player))
        return;

    player = (Player *)(Sentient *)owner;

    trace_t trace;
    Vector  start;
    Vector  forward, right;
    Vector  end;
    int     i;
    Vector  sweepAngles;
    Vector  dir;
    bool    hitSky;
    Vector  meleeMins;
    Vector  meleeMaxs;
    float   bestFraction = 0.0f;
    Vector  skyPosition;
    Vector  bestSkyPosition;

    player->GetViewTrace(trace, MASK_PROJECTILE, 5000.0f);

    start   = trace.endpos;
    hitSky  = false;

    meleeMins = Vector(-5.0f, -5.0f, -5.0f);
    meleeMaxs = Vector(5.0f, 5.0f, 5.0f);

    for (i = 0; i <= 360; i += 45) {
        end = start + Vector(0.0f, 0.0f, 10000.0f);

        if (i != 360) {
            sweepAngles     = vec_zero;
            sweepAngles[YAW] = i;
            sweepAngles.AngleVectors(&dir);

            end += dir * 7500.0f;
        }

        trace = G_Trace(start, vec_zero, vec_zero, end, player, MASK_PROJECTILE, false, "airStrike::skyspot1");


        if (trace.surfaceFlags & SURF_SKY) {
            skyPosition = trace.endpos;

            trace = G_Trace(skyPosition, meleeMins, meleeMaxs, start, player, MASK_PROJECTILE, false, "airStrike::skyspot2");

            if ((bestFraction == 0.0f) || (trace.fraction > bestFraction)) {
                bestFraction    = trace.fraction;
                bestSkyPosition = skyPosition;
            }

            hitSky = true;
        }
    }

    if (hitSky) {
        right   = bestSkyPosition;
        forward = right - start;

        if (forward.normalize() > 4000)
            right = start + (forward * 4000); 
        else
            right = trace.endpos;

        Projectile *airstrike;
        airstrike = new Projectile; 
        airstrike->setModel("models/weapons/projectile_airstrike.tik");
        airstrike->setOrigin(right);
        forward *= -1.0f; 
        airstrike->setAngles(forward.toAngles());
        airstrike->PostEvent(EV_Remove, 30);

        airStrikeSound = GetRandomAlias("snd_strikeincoming"); 

        _nextUseTime = level.time + 30.0f;
    } else {
        airStrikeSound = GetRandomAlias("snd_strikeblocked"); 
    }

    Sound(airStrikeSound);
}

void Equipment::setScanner(Event *ev)
{
    if (ev == NULL) {
        return;
    }

    scanner = true;
}

void Equipment::setRadar(Event *ev)
{
    if (ev == NULL) {
        return;
    }

    radar = true;
}

void Equipment::ProcessTargetedEntity(EntityPtr entity)  
{
    if (_scannedEntity != entity && entity) {
        Player *player = (Player *)(Sentient *)owner;

        if (player) {
            float distanceSquared = DistanceSquared(entity->origin, Vector(player->client->ps.origin));

            if (distanceSquared > MAX_TARGET_DISTANCE_SQUARED) {
                scanTime = (int)level.time;
            }
        }
    }

    if (_scannedEntity != entity) {
        scanTime = (int)level.time;
    }

    _scannedEntity = entity;

    if (!entity) {
        return;
    }

    Weapon::ProcessTargetedEntity(entity);

    if (isScanning()) {
        entity->edict->s.eFlags &= ~EF_DISPLAY_DESC1;
        entity->edict->s.eFlags |= EF_DISPLAY_DESC2;

        long timeElapsed = (long)(level.time - scanTime);

        if (timeElapsed >= DETAILED_SCAN_TIME) {
            entity->edict->s.eFlags |= EF_DISPLAY_DESC3;
        }
    } else {
        scanTime = (int)level.time;
        entity->edict->s.eFlags &= ~EF_DISPLAY_DESC2;
        entity->edict->s.eFlags &= ~EF_DISPLAY_DESC3;
    }
}

void Equipment::setTypeName(Event *ev)
{
    _typeName = ev->GetString(1);
}

void Equipment::hasModes(Event *ev)
{
    for (int i = 1; i <= ev->NumArgs(); i++) {
        _modes.AddObject(ev->GetString(i));
    }
}

void Equipment::changeMode(Event *ev)
{
    int newMode;

    if (ev->NumArgs() > 0) {
        newMode = ev->GetInteger(1);
    } else {
        newMode = _currentMode + 1;
    }

    changeMode(newMode);
}

void Equipment::changeMode(int newMode)
{
    int numModes = _modes.NumObjects();

    if (!numModes) {
        return;
    }

    _currentMode = newMode;
    if (_currentMode > numModes) {
        _currentMode = 1;
    }

    for (int guard = 0; guard <= numModes; guard++) {
        if (_currentMode == 1 && owner && owner->isSubclassOf(Player)) {
            Player *player = (Player *)(Sentient *)owner;

            if (player->client->ps.pm_flags & PMF_NIGHTVISION) {
                Event *newEvent = new Event(EV_Sentient_SetViewMode);
                newEvent->AddString("nightvision");
                owner->ProcessEvent(newEvent);

                return;
            }
        }

        str& modeName = _modes.ObjectAt(_currentMode);

        if (_currentMode != 1 && world && world->isAnyViewModeAvailable()
            && !world->isViewModeAvailable(modeName.c_str())) {
            _currentMode++;
            if (_currentMode > numModes) {
                _currentMode = 1;
            }
            continue;
        }


        Event *newEvent = new Event(EV_Sentient_SetViewMode);
        newEvent->AddString(modeName.c_str());
        owner->ProcessEvent(newEvent);

        return;
    }
}

void Equipment::resetMode(void)
{
    changeMode(1);
}

void Equipment::updateMode(void)
{
    changeMode(_currentMode);
}

void Equipment::AttachToOwner(weaponhand_t weaponHand)
{
    _active = true;

    Weapon::AttachToOwner(weaponHand);

    updateRadarState();

    if (_lastMode > 1) {
        changeMode(_lastMode);
    }
}

void Equipment::Uninitialize(void)
{
    if (!_active) {
        return;
    }

    Weapon::Uninitialize();

    if (!owner) {
        return;
    }

    if (!owner->isSubclassOf(Player)) {
        return;
    }

    {
        Player *player = (Player *)(Sentient *)owner;

        if (player) {
            player->client->ps.pm_flags &= ~(PMF_SCANNER | PMF_RADAR_MODE);
        }
    }

    _lastMode = _currentMode;

    resetMode();

    _active = false;
}

void Equipment::Archive(Archiver& arc)
{
    Weapon::Archive(arc);

    arc.ArchiveBool(&scanner);
    arc.ArchiveBool(&scanning);
    arc.ArchiveInteger(&_scanEndFrame);
    arc.ArchiveInteger(&scanTime);
    arc.ArchiveBool(&radar);

    _modes.Archive(arc);

    arc.ArchiveInteger(&_currentMode);
    arc.ArchiveInteger(&_lastMode);

    arc.ArchiveString(&_typeName);

    arc.ArchiveSafePointer(&_scannedEntity);

    arc.ArchiveBool(&_active);

    arc.ArchiveFloat(&_nextUseTime);

}



void Equipment::PutAway(void)
{
	Weapon::PutAway();
}




int Equipment::getStat( int statNum )
{
	if ( statNum == STAT_WEAPON_GENERIC1 )
	{

		str modeName;

		if ( !scanner || !scanning )
			return 0;

		if ( level.time < _nextUseTime )
			return 0;

		modeName = _modes.ObjectAt( _currentMode );

		if ( modeName == "torpedostrike" )
		{
			return (int)( ( ( level.time - scanTime ) / 5.0f ) * 100.0f );
		}
	}
	else if ( statNum == STAT_WEAPON_GENERIC2 )
	{
		if ( level.time > _nextUseTime )
			return 100;

		return (int)( 100.0f - ( ( _nextUseTime - level.time ) / 30.0f ) * 100.0f );
	}

	return 0;
}

str Equipment::getTypeName(void) const
{
    return _typeName;
}
