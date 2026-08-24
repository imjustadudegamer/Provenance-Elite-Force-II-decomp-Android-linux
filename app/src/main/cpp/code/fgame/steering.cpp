//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/steering.cpp                                  $
// $Revision:: 29                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/01/02 5:47p                                                 $
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//
// DESCRIPTION:
// Steering behaviors for AI.
//
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).

#include "steering.h"
#include "actor.h"
#include "actor_locomotion.h"
#include "doors.h"
#include "debuglines.h"
#include "navigate.h"
#include "path.h"



CLASS_DECLARATION(Listener, Steering, NULL) {
    {NULL, NULL}
};

Steering::Steering()
    : _steeringForce(vec_zero)
    , _origin(vec_zero)
    , _moveDirection(vec_zero)
    , _maxSpeed(320)
{}

void Steering::ShowInfo(Actor& self)
{
    gi.Printf("steeringforce: ( %f, %f, %f )\n", _steeringForce.x, _steeringForce.y, _steeringForce.z);
    gi.Printf("origin: ( %f, %f, %f )\n", _origin.x, _origin.y, _origin.z);
    gi.Printf("movedir: ( %f, %f, %f )\n", _moveDirection.x, _moveDirection.y, _moveDirection.z);
    gi.Printf("maxspeed: %f\n", _maxSpeed);
}

void Steering::Begin(Actor& self) {}

const Steering::ReturnValue Steering::Evaluate(Actor& self)
{
    return Steering::FAILED;
}

void Steering::End(Actor& self) {}

void Steering::ResetForces(void)
{
    _steeringForce = vec_zero;
}

void Steering::SetSteeringForce(const Vector& steeringForce)
{
    Vector tempSteeringForce(steeringForce);
    tempSteeringForce.EulerNormalize();
    _steeringForce = tempSteeringForce;
}


CLASS_DECLARATION(Steering, Jump, NULL) {
    {NULL, NULL}
};

Jump::Jump()
    : _endtime(0.0f)
    , _launchAngle(0.0f)
    , _state(0)
    , _animdone(false)
    , _goal(vec_zero)
{}

void Jump::SetGoal(const Vector& goalPosition)
{
    _goal = goalPosition;
}

void Jump::SetEntity(Entity *entity_to_jump_to)
{
    if (entity_to_jump_to) {
        _goal = entity_to_jump_to->origin;
    }
}

void Jump::Begin(Actor& self)
{
    self.SetAnim("jump", EV_Anim_Done);

    float traveltime = 0.0f;
    if (self.movementSubsystem) {
        traveltime = self.movementSubsystem->JumpTo(_goal, _launchAngle);
    }
    _endtime = traveltime + level.time;

    self.last_jump_time = _endtime;

    _state = 0;
}

const Steering::ReturnValue Jump::Evaluate(Actor& self)
{
    _animdone = self.GetActorFlag(ACTOR_FLAG_ANIM_DONE) ? true : false;
    self.SetActorFlag(ACTOR_FLAG_ANIM_DONE, qfalse);

    switch (_state) {
    case 0:
        _state = 1;
        break;

    case 1:
        if (_animdone) {
            if (self.animate->HasAnim("fall")) {
                _animdone = false;
                self.SetAnim("fall", EV_Anim_Done);
                _state = 2;
            }
        }
        if (self.groundentity) {
            _state = 2;
        }
        break;

    case 2:
        if (self.groundentity) {
            _state = 3;
            if (self.animate->HasAnim("land")) {
                _animdone = false;
                self.SetAnim("land", EV_Anim_Done);
                _state = 4;
            } else {
                return Steering::EVALUATING;
            }
        }
        break;

    case 3:
        if (level.time > _endtime) {
            return Steering::SUCCESS;
        }
        break;

    case 4:
        if (_animdone) {
            return Steering::SUCCESS;
        }
        break;

    default:
        break;
    }

    return Steering::EVALUATING;
}

void Jump::End(Actor& self)
{
    self.SetAnim("idle");
}




void Steering::DrawForces(void)
{
	G_Color3f( 0.3, 0.5, 1 );
	G_BeginLine();
	G_Vertex( _origin );
	G_Vertex( _origin + _steeringForce * FRAMETIME );
	G_EndLine();
	
	G_Color3f( 1, 0, 1 );
	G_BeginLine();
	G_Vertex( _origin );
	G_Vertex( _origin + _moveDirection * _maxSpeed * FRAMETIME );
	G_EndLine();
}


void Steering::SetMaxSpeed(float speed)
{
	_maxSpeed = speed;
}


void Steering::SetMoveDirection(const Vector &dir)
{
	_moveDirection = dir;
}


void Steering::SetPosition(const Vector &pos)
{
	_origin = pos;
}

