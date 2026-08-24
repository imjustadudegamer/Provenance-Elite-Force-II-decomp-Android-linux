/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later
// What is deliberately NOT changed here: retail's Animate drives TWO NAMED CHANNELS

#pragma once

#include "g_local.h"
#include "../corepp/listener.h"
#include "entity.h"
#include "archive.h"


#define ANIM_PAUSED                      1
#define ANIM_SYNC                        2
#define ANIM_FINISHED                    4
#define ANIM_NOEXIT                      8
#define ANIM_NODELTA                     16
#define ANIM_NOACTION                    64

#define MINIMUM_DELTA_MOVEMENT           8
#define MINIMUM_DELTA_MOVEMENT_PER_FRAME (MINIMUM_DELTA_MOVEMENT / 20.0f)

class Entity;
class Animate;

typedef SafePtr<Animate> AnimatePtr;

class Animate : public Listener
{
public:
    // SDK animate.h:61.  The owning entity.  NEVER archived -- Entity::Archive re-supplies
    Entity *self;

    str    currentAnim;
    Event *animDoneEvent;
    Event *torso_animDoneEvent;
    float legs_animtime;
    float torso_animtime;
    float legs_starttime;
    float torso_starttime;
    float legs_frametime;
    float torso_frametime;
    int   legs_numframes;
    int   torso_numframes;

    Vector frame_delta;

public:
    CLASS_PROTOTYPE(Animate);

    Animate();
    explicit Animate(Entity *ent);
    ~Animate();


    void        NewAnim(int animnum, int slot = 0, float weight = 1.0f, int ef2Channel = EF2ANIM_INFER);
    void        NewAnim(int animnum, Event *endevent, int slot = 0, float weight = 1.0f);
    void        NewAnim(int animnum, Event &endevent, int slot = 0, float weight = 1.0f);
    void        RandomAnimate(const char *animname, Event *endevent = NULL, bodypart_t part = legs);
    void        RandomAnimate(const char *animname, const Event &endevent, bodypart_t part = legs);
    void        Legs_SetFrameEvent(Event *ev);
    qboolean    HasAnim(const char *animname);
    Event      *AnimDoneEvent(int slot = 0);
    void        SetAnimDoneEvent(Event *event, bodypart_t part = legs);
    void        SetAnimDoneEvent(const Event &event, bodypart_t part = legs);
    void        EndAnim(int part);
    void        Legs_AnimDoneEvent(Event *ev);
    void        Torso_AnimDoneEvent(Event *ev);
    void        NewAnimEvent(Event *ev);
    const char *AnimName(int slot = 0);

    int   NumAnims(void);
    float AnimTime(int slot = 0);

    int   CurrentAnim(int slot = 0);   

    void Archive(Archiver& arc) override;

    // ⚠️ DO NOT CONFUSE WITH CurrentAnim(slot)/CurrentTime(slot) directly above.  Those
    int CurrentFrame(int part) const;

    void  StopAnimating(int slot = 0);
    void  ClearTorsoAnim(void);
    void  ClearLegsAnim(void);




    int NumFrames(int slot = 0);
    virtual void AddEffectAnim(const char *animName);  
    void AddEffectAnim(Event *ev);
    virtual void ClearAllEffectAnims(void);  
    void ClearAllEffectAnims(Event *ev);
    void FrameDeltaEvent(Event *ev);
    void Legs_AnimEvent(Event *ev);
    void Legs_StopAnimating(Event *ev);
    virtual void RemoveEffectAnim(const char *animName);  
    void RemoveEffectAnim(Event *ev);
    void RestoreAnimationRate(Event *ev);
    float oldAnimationRate;  
    void SetAnimationRate(const float animationRate);  
    virtual void StopAnimatingAtEnd(bodypart_t part = legs);  
    void StopAnimatingAtEnd(Event *ev);
    void Torso_AnimEvent(Event *ev);
    void Torso_SetFrameEvent(Event *ev);
    void Torso_StopAnimating(Event *ev);
    str GetName(Event *ev);






public:
    void SetFrame( int framenum, bodypart_t part = legs, int anim = -1 ); 
    void RestoreAnimationRate( void ); 
    str GetName(); 
};

inline void Animate::RandomAnimate(const char *animname, const Event &endevent, bodypart_t part)
{
    Event *ev = new Event(endevent);
    RandomAnimate(animname, ev, part);
}
;







inline Event *Animate::AnimDoneEvent(int slot)
{
    if (slot == 0) {
        return animDoneEvent ? new Event(animDoneEvent) : NULL;
    }
    if (slot == 1) {
        return torso_animDoneEvent ? new Event(torso_animDoneEvent) : NULL;
    }
    warning("AnimDoneEvent", "Unknown body part %d", slot);
    return NULL;
}

inline int Animate::NumFrames(int slot)
{
    switch (slot) {
    case 0:
        return legs_numframes;
    case 1:
        return torso_numframes;
    default:
        warning("NumFrames", "Unknown body part %d", slot);
        return 0;
    }
}

inline int Animate::NumAnims(void)
{
    return gi.NumAnims(self->edict->s.modelindex);
}

inline float Animate::AnimTime(int slot)
{
    switch (slot) {
    case 0:
        return legs_animtime;
        break;
    case 1:
        return torso_animtime;
        break;
    default:
        warning("AnimTime", "Unknown slot %d", slot);
        return 0;
        break;
    }
}
