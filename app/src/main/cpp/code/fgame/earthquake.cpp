//-----------------------------------------------------------------------------
//
//  $Logfile:: /EF2/Code/DLLs/game/earthquake.cpp                            $
// $Revision:: 11                                                             $
//   $Author:: Singlis                                                        $
//     $Date:: 9/26/03 2:36p                                                  $
//
// Copyright (C) 2002 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source is may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Earthquake trigger causes a localized earthquake when triggered.
// The earthquake effect is visible to the user as the shaking of his screen.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "earthquake.h"
#include "weapon.h"
#include "sentient.h"
#include "level.h"
#define PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_probe.h"

Event EV_Earthquake_Deactivate (	"earthquake_deactivate",
	EV_SCRIPTONLY,
	NULL,
	NULL,
	"Stops the earthquake."
);



#define EARTHQUAKE_NO_RAMPUP   (1 << 0)
#define EARTHQUAKE_NO_RAMPDOWN (1 << 1)

Event EV_Earthquake_SetDuration
(
	"duration",
	EV_SCRIPTONLY,
	"f",
	"duration",
	"Sets the duration of the earthquake."
);

Event EV_Earthquake_SetMagnitude (    "magnitude",
    EV_SCRIPTONLY,
    "f",
    "theMagnitude",
    "Sets the magnitude of the earthquake.");
Event EV_Earthquake_SetDistance (    "distance",
    EV_SCRIPTONLY,
    "f",
    "distance",
    "Sets the distance that an earthquake will effect things.");
Event EV_Earthquake_SetPlaySound (    "earthquake_playsound",
    EV_SCRIPTONLY,
    "B",
    "bool",
    "Sets whether or not the earthquake plays its looping sound or not (defaults to true)."
);

CLASS_DECLARATION(Trigger, Earthquake, "func_earthquake") {
    {&EV_Touch,                    NULL                    },
    {&EV_Trigger_Effect,           &Earthquake::Activate   },
    {&EV_Earthquake_Deactivate,    &Earthquake::Deactivate },
    {&EV_Earthquake_SetDuration,   &Earthquake::SetDuration },
    {&EV_Earthquake_SetMagnitude,  &Earthquake::SetMagnitude },
    {&EV_Earthquake_SetDistance,   &Earthquake::SetDistance },
    {&EV_Earthquake_SetPlaySound,  &Earthquake::setPlaySound },
    {NULL,                         NULL                    }
};

Earthquake::Earthquake()
{
    CacheResource("snd_earthquake", this);

    if (LoadingSavegame) {
        return;
    }

    _duration         = 0.8f;
    _magnitude        = 1.0f;
    _distance         = 0.0f;
    _startTime        = 0.0f;
    _currentMagnitude = 0.0f;
    _active           = false;
    _playSound        = false;

    edict->svflags &= ~SVF_NOCLIENT;
}

Earthquake::~Earthquake()
{
    level.removeEarthquake(this);
    StopLoopSound();
}

void Earthquake::SetDuration(Event *ev)
{
    _duration = ev->GetFloat(1);
}

void Earthquake::SetMagnitude(Event *ev)
{
    _magnitude = ev->GetFloat(1);
}

void Earthquake::SetDistance(Event *ev)
{
    _distance = ev->GetFloat(1);
}

void Earthquake::setPlaySound(Event *ev)
{
    _playSound = (ev->NumArgs() > 0) ? ev->GetBoolean(1) : true;
}

void Earthquake::Activate(Event *ev)
{
    if (_duration <= 0.0f) {
        return;
    }

    level.addEarthquake(this);

    _startTime        = level.time;
    _currentMagnitude = _magnitude;
    _active           = true;

    if (_playSound) {
        LoopSound("snd_earthquake", DEFAULT_VOL, LEVEL_WIDE_MIN_DIST);
    }

    PostEvent(EV_Earthquake_Deactivate, _duration);

    turnThinkOn();

    {
        static int uivk_dfx_n;
        if (PROBE_On() && PROBE_Throttle(&uivk_dfx_n, 32, 32)) {
            PROBE("DFX:quake-activate", "ent=%d mag=%.3f dur=%.3f dist=%.1f org=(%.1f,%.1f,%.1f) think=%d\n",
                       entnum, _magnitude, _duration, _distance,
                       origin[0], origin[1], origin[2], isSubclassOf(Entity) && (flags & FL_THINK) ? 1 : 0);
        }
    }
}

void Earthquake::Deactivate(Event *ev)
{
    level.removeEarthquake(this);

    _active           = false;
    _currentMagnitude = 0.0f;

    StopLoopSound();

    turnThinkOff();
}

float Earthquake::getMagnitudeAtPosition(const Vector& position)
{
    float magnitudeAtPosition = 0.0f;


    if (_active) {

        magnitudeAtPosition = _currentMagnitude;


        if (_distance) {
            Vector      dir      = origin - position;
            const float distance = dir.length();


            if (distance > _distance) {
                magnitudeAtPosition = 0.0f;
            } else {
                magnitudeAtPosition *= 1.0f - (distance / _distance);
            }
        }
    }

    return magnitudeAtPosition;
}

// same shake magnitude but NEVER ramped the sound volume.
void Earthquake::Think(void)
{
    float timeDelta;

    if (!_active) {
        return;
    }

    timeDelta = level.time - _startTime;

    if (timeDelta < (_duration * 0.5f)) {
        if (!(spawnflags & EARTHQUAKE_NO_RAMPUP)) {
            float rampUpTime;

            rampUpTime = _startTime + (_duration * 0.33f);

            if (level.time < rampUpTime) {
                float scale;

                scale                = (timeDelta) / (_duration * 0.33f);
                _currentMagnitude    = _magnitude * scale;
                edict->s.loopSoundVolume = scale;
            } else {
                _currentMagnitude    = _magnitude;
                edict->s.loopSoundVolume = 1.0f;
            }
        }
    }
    else {
        if (!(spawnflags & EARTHQUAKE_NO_RAMPDOWN)) {
            float rampDownTime;

            rampDownTime = _startTime + (_duration * 0.66f);

            if (level.time > rampDownTime) {
                float scale;

                scale                = 1.0f - ((level.time - rampDownTime) / (_duration * 0.33f));
                _currentMagnitude    = _magnitude * scale;
                edict->s.loopSoundVolume = scale;
            } else {
                _currentMagnitude    = _magnitude;
                edict->s.loopSoundVolume = 1.0f;
            }
        }
    }
}





void Trigger::Archive
	(
	Archiver &arc
	)
   {
   Entity::Archive( arc );

   arc.ArchiveFloat( &wait );
   arc.ArchiveFloat( &delay );
   arc.ArchiveFloat( &trigger_time );
   arc.ArchiveBoolean( &triggerActivated );
   arc.ArchiveInteger( &count );
   arc.ArchiveString( &noise );
   if ( arc.Loading() )
      {
      SetNoise( noise.c_str() );
      }
   arc.ArchiveString( &message );
   arc.ArchiveString( &key );
   arc.ArchiveString( &thread );
   arc.ArchiveSafePointer( &activator );
   arc.ArchiveInteger( &respondto );
   arc.ArchiveBoolean( &useTriggerDir );
   arc.ArchiveFloat( &triggerCone );
   arc.ArchiveVector( &triggerDir );
   arc.ArchiveFloat( &triggerDirYaw );
   arc.ArchiveBoolean( &triggerable );
   arc.ArchiveBoolean( &removable );
   arc.ArchiveBoolean( &edgeTriggered );
   arc.ArchiveInteger( &multiFaceted );

   entList.Archive( arc );
   arc.ArchiveBool( &destructible );
   arc.ArchiveInteger( &instantdeath );
   arc.ArchiveBool( &triggerondamage );
   arc.ArchiveBool( &triggerondeath );
   }
