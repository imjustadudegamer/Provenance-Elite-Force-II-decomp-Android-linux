

#pragma once

#include "g_local.h"
#include "trigger.h"

#define EARTHQUAKE_STRENGTH 50

extern Event EV_Earthquake_SetDuration;
extern Event EV_Earthquake_SetMagnitude;
extern Event EV_Earthquake_SetDistance;

class Earthquake : public Trigger
{
protected:
    float _startTime;
    bool  _active;
    float _magnitude;
    float _duration;
    float _distance;
    float _currentMagnitude;
    bool  _playSound;

public:
    CLASS_PROTOTYPE(Earthquake);

    Earthquake();
    ~Earthquake();

    void         Activate(Event *ev);
    void         Deactivate(Event *ev);
    void         SetDuration(Event *ev);
    void         SetMagnitude(Event *ev);
    void         SetDistance(Event *ev);
    void         Think(void) override;
    bool         EarthquakeActive() { return _active; }
    void         Archive(Archiver& arc) override;
    float        getMagnitudeAtPosition(const Vector& position);
    void         setPlaySound(Event *ev);
};

inline void Earthquake::Archive(Archiver& arc)
{
    Trigger::Archive(arc);

    arc.ArchiveFloat(&_startTime);
    arc.ArchiveBool(&_active);
    arc.ArchiveFloat(&_magnitude);
    arc.ArchiveFloat(&_duration);
    arc.ArchiveFloat(&_distance);
    arc.ArchiveFloat(&_currentMagnitude);
    arc.ArchiveBool(&_playSound);
}
