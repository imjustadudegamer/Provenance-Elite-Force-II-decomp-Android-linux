
#include "actor.h"
#include "actor_locomotion.h"
#include "archive.h"
#include "doors.h"    
#include "navigate.h" 
#include "gamecvars.h"

Vector MovementSubsystem::_step;

MovementSubsystem::MovementSubsystem()
    : act(NULL)
{
    _lastmove           = STEPMOVE_OK;
    _forwardspeed       = 0.0f;
    _movespeed          = 1.0f;
    _totallen           = 0.0f;
    _turnspeed          = TURN_SPEED;
    _fliplegs           = qfalse;
    _movingBackwards    = qfalse;
    _faceEnemy          = false;
    _adjustAnimDir      = true;
    _movementType       = MOVEMENT_TYPE_NORMAL;
    _stickToGround      = true;
    _useCodeDrivenSpeed = false;
    _step               = Vector(0.0f, 0.0f, STEPSIZE);

    _steeringCommandedMoveDir = false;
}

MovementSubsystem::MovementSubsystem(Actor *actor)
{
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_DROP, "MovementSubsystem::MovementSubsystem -- actor is NULL");
    }

    _init();
}

MovementSubsystem::~MovementSubsystem()
{
    if (_path) {
        delete _path;
        _path = NULL;
    }
}

void MovementSubsystem::_init()
{
    _lastmove     = STEPMOVE_OK;
    _path         = NULL; 
    _turnspeed    = TURN_SPEED;
    _startpos     = act->origin;
    _forwardspeed = 0;
    _divedir      = vec_zero;
    act->angles.AngleVectors(&_movedir);

    _animdir = _movedir;

    _step      = Vector(0.0f, 0.0f, STEPSIZE);
    _movespeed = 1.0f;

    _fliplegs           = qfalse;
    _movingBackwards    = qfalse;
    _faceEnemy          = false;
    _adjustAnimDir      = true;
    _useCodeDrivenSpeed = false;

    act->feet_width  = 0.0f;
    act->last_origin = act->origin;

    _movementType  = MOVEMENT_TYPE_NORMAL;
    _stickToGround = true;

    _steeringCommandedMoveDir = false;

    CheckWater();
}

const Vector MovementSubsystem::SteerTowardsPoint(
    const Vector& targetPosition,
    const Vector& targetVelocity,
    const Vector& moveDirection,
    const float   maxSpeed,
    const bool    adjustSpeed
)
{
    assert(act != NULL);

    Vector myPosition(act->origin);
    Vector predictedPosition =
        targetPosition + targetVelocity * (Vector::Distance(targetPosition, myPosition) / maxSpeed);
    Vector desiredDirection = predictedPosition - myPosition;

    Vector newSteeringForce = Vector::AnglesBetween(desiredDirection, moveDirection);
    newSteeringForce[ROLL]  = 0.0f;
    newSteeringForce.EulerNormalize();

    if (adjustSpeed) {
        const float currentSpeed = _forwardspeed;
        const float turnRate     = _turnspeed;
        const float turnRadius   = currentSpeed / turnRate;

        Vector right;
        right.CrossProduct(moveDirection, Vector(0, 0, 1));
        const Vector centerOfTurn(myPosition + (right * DotProduct(right, desiredDirection)));

        const float forwardComponent = DotProduct(desiredDirection, moveDirection);
        const float rightComponent   = DotProduct(desiredDirection, right);

        if (Vector::DistanceXY(centerOfTurn, predictedPosition) < turnRadius) {
            const float newTurnRadius =
                ((rightComponent * rightComponent) + (forwardComponent * forwardComponent)) / (2.0f * rightComponent);
            _forwardspeed = 2.0f * turnRate * newTurnRadius;
        }
    }

    return newSteeringForce;
}

void MovementSubsystem::Accelerate(const Vector& original_steering)
{
    Vector steering = original_steering;
    Vector newDir   = _movedir.toAngles();

    if (steering.y) {
        newDir.y += steering.y;
    }

    newDir.EulerNormalize();

    if (act->animate->frame_delta.x > 4.0f) {
        newDir.z = _movespeed * (0.4f / 320.0f) * steering.y;

        if ((act->flags & FL_FLY) || ((act->flags & FL_SWIM) && act->waterlevel > 0)) {
            newDir.z = bound(act->angles.z, -2.0f, 2.0f);
        } else {
            newDir.z = bound(act->angles.z, -5.0f, 5.0f);
        }
    } else {
        newDir.z = 0.0f;
    }

    newDir.AngleVectors(&_movedir);

    _steeringCommandedMoveDir = true;

    Vector newAng = _animdir.toAngles();

    float newDirYaw = AngleNormalize180(newDir.yaw());
    float newAngYaw = AngleNormalize180(newAng.yaw());

    if (act->bind_info && act->bind_info->bindmaster) {
        float orientation[3][3];
        float parentOrientation[3][3];
        float mat[3][3];
        AnglesToAxis(newDir, mat);
        Vector parentAngles;

        MatrixToEulerAngles(act->bind_info->bindmaster->orientation, parentAngles);
        parentAngles[YAW] = AngleNormalize180(-parentAngles[YAW]);
        AnglesToAxis(parentAngles, parentOrientation);

        R_ConcatRotations(mat, parentOrientation, orientation);
        MatrixToEulerAngles(orientation, newDir);

        AnglesToAxis(newAng, mat);
        R_ConcatRotations(mat, parentOrientation, orientation);
        MatrixToEulerAngles(orientation, newAng);
    }

    newDirYaw = AngleNormalize180(newDir.yaw());
    newAngYaw = AngleNormalize180(newAng.yaw());

    if (_movingBackwards) {
        newDirYaw = AngleNormalize180(newDirYaw - 180.0f);
    }

    float AngleDiff = newDirYaw - newAngYaw;
    AngleDiff       = AngleNormalize180(AngleDiff);

    if (((AngleDiff >= 0.0f) && (AngleDiff < _turnspeed)) || ((AngleDiff <= 0.0f) && (AngleDiff > -_turnspeed))) {
        newAng[YAW] = AngleNormalize360(newDirYaw);

        if (_faceEnemy) {
            Entity *currentEnemy = act->enemyManager->GetCurrentEnemy();

            if (!currentEnemy) {
                act->enemyManager->FindHighestHateEnemy();
                currentEnemy = act->enemyManager->GetCurrentEnemy();
            }

            if (currentEnemy) {
                Vector selfToEnemy = currentEnemy->origin - act->origin;
                selfToEnemy        = selfToEnemy.toAngles();
                selfToEnemy[PITCH] = 0.0f;

                newAng = selfToEnemy;
            }
        }

        if (_adjustAnimDir) {
            act->setAngles(newAng);
        }
        return;
    }

    if (AngleDiff > 0.0f) {
        newAng[YAW] = AngleNormalize360(newAng[YAW] += _turnspeed);
    } else {
        newAng[YAW] = AngleNormalize360(newAng[YAW] -= _turnspeed);
    }

    if (_movingBackwards) {
        newAng[YAW] = AngleNormalize180(newDir[YAW] - 180.0f);
    }

    if (_faceEnemy) {
        Entity *currentEnemy = act->enemyManager->GetCurrentEnemy();

        if (!currentEnemy) {
            act->enemyManager->FindHighestHateEnemy();
            currentEnemy = act->enemyManager->GetCurrentEnemy();
        }

        if (currentEnemy) {
            Vector selfToEnemy = currentEnemy->origin - act->origin;
            selfToEnemy        = selfToEnemy.toAngles();
            selfToEnemy[PITCH] = 0.0f;

            newAng = selfToEnemy;
        }
    }

    if (_adjustAnimDir) {
        act->setAngles(newAng);
    }
}

// displacements, NOT speeds.  Retail's TryMove traces origin -> origin + _move directly
void MovementSubsystem::CalcMove(void)
{
    _totallen = 0;

    if (act->total_delta != vec_zero) {
        _totallen = act->total_delta.length();
    }

    if (_forwardspeed) {
        _totallen = _forwardspeed;
    }

    if (_movementType == MOVEMENT_TYPE_ANIM) {
        MatrixTransformVector(act->total_delta, act->orientation, _move);
    } else {
        _movedir.normalize();
        _move = _movedir;
        _move *= _totallen;
    }

    if (act->movetype == MOVETYPE_NONE) {
        _move *= 0.0f;
    }

    act->total_delta = vec_zero;

    _animdir = act->orientation[0];

    _steeringCommandedMoveDir = false;
}

qboolean MovementSubsystem::CanMoveTo(const Vector& pos)
{
    return _canMoveSimplePath(act->mins, act->maxs, pos);
}

bool MovementSubsystem::CanWalkTowardsPoint(const Vector& goalPoint, const int mask)
{
    Vector      step(goalPoint - act->origin);
    const float distance = Vector::Distance(goalPoint, act->origin);
    step.normalize();
    step *= Q_min(distance, Q_max(64.0f, Vector::Distance(act->origin, act->last_origin)));
    return CanWalkTo(act->origin + step, 0, ENTITYNUM_NONE, mask) == 1;
}

qboolean MovementSubsystem::CanWalkTo(const Vector& pos, float bounding_box_extra, int entnum, const int mask)
{
    return CanWalkToFrom(act->origin, pos, bounding_box_extra, entnum, mask);
}

// Retail also builds a local `startPos` (origin with z+15) that it never reads; it has no
qboolean MovementSubsystem::CanWalkToFrom(
    const Vector& origin, const Vector& pos, float bounding_box_extra, int entnum, const int mask
)
{
    Vector real_pos;
    Vector test_mins;
    Vector test_maxs;

    test_mins = act->mins;
    test_maxs = act->maxs;

    test_mins.x -= bounding_box_extra;
    test_mins.y -= bounding_box_extra;

    test_maxs.x += bounding_box_extra;
    test_maxs.y += bounding_box_extra;

    if (entnum != ENTITYNUM_NONE) {
        real_pos = _getRealDestinationPosition(pos);
    } else {
        real_pos = pos;
    }

    if (act->GetActorFlag(ACTOR_FLAG_SIMPLE_PATHFINDING)) {
        return _canMoveSimplePath(test_mins, test_maxs, real_pos);
    }

    return _checkHaveGroundEachStep(origin, real_pos, test_mins, test_maxs, mask);
}

stepmoveresult_t MovementSubsystem::WaterMove(void)
{
    Vector  oldorg;
    Vector  neworg;
    trace_t trace;
    int     oldwater;

    if ((_totallen <= 0.01f) || (_move == vec_zero)) {
        return STEPMOVE_OK;
    }

    oldorg = act->origin;
    neworg = act->origin + _move;

    trace = G_Trace(oldorg, act->mins, act->maxs, neworg, act, act->edict->clipmask, qfalse, "Actor::WaterMove 1");
    if (trace.fraction == 0.0f) {
        return STEPMOVE_STUCK;
    }

    oldwater = act->waterlevel;

    act->setOrigin(trace.endpos);

    CheckWater();

    if ((oldwater > 1) && (act->waterlevel < 2)) {
        act->waterlevel = oldwater;
        act->setOrigin(oldorg);
        return STEPMOVE_STUCK;
    }

    return STEPMOVE_OK;
}

stepmoveresult_t MovementSubsystem::AirMove(void)
{
    Vector  oldorg;
    Vector  neworg;
    trace_t trace;
    int     oldwater;

    if ((_totallen <= 0.01f) || (_move == vec_zero)) {
        return STEPMOVE_OK;
    }

    oldorg = act->origin;
    neworg = act->origin + _move;

    trace = G_Trace(oldorg, act->mins, act->maxs, neworg, act, act->edict->clipmask, qfalse, "Actor::AirMove 1");
    if (trace.fraction < 0.0001f) {
        return STEPMOVE_BLOCKED_BY_WATER;
    }

    oldwater = act->waterlevel;

    act->setOrigin(trace.endpos);

    if (!act->GetActorFlag(ACTOR_FLAG_IGNORE_WATER)) {
        CheckWater();

        if (!oldwater && act->waterlevel) {
            act->waterlevel = oldwater;
            act->setOrigin(oldorg);
            return STEPMOVE_STUCK;
        }
    }

    return STEPMOVE_OK;
}

stepmoveresult_t MovementSubsystem::IsMoveValid(
    trace_t& horizontalTrace, trace_t& verticalTrace, const Vector& moveBegin, const Vector& moveEnd
)
{
    horizontalTrace = act->Trace( moveBegin, moveEnd, "MovementSubsystem::IsMoveValid" );  

    if (horizontalTrace.startsolid) {
        horizontalTrace = G_Trace(
            moveBegin,
            act->mins,
            act->maxs,
            moveBegin + _move,
            act,
            act->edict->clipmask,
            qfalse,
            "MovementSubsystem::IsMoveValid2"
        );
    }

    _blockingNormal = vec_zero;
    if (horizontalTrace.plane.normal != vec_zero) {
        _blockingNormal = horizontalTrace.plane.normal;
    }

    if (horizontalTrace.startsolid || (horizontalTrace.fraction < 0.0001f)) {
        return STEPMOVE_STUCK;
    }

    if (_isBlockedByDoor(horizontalTrace)) {
        return STEPMOVE_BLOCKED_BY_DOOR;
    }

    if (!act->GetActorFlag(ACTOR_FLAG_USE_GRAVITY) || (act->gravity == 0) || act->deadflag) {
        return _noGravityTryMove(horizontalTrace.endpos, verticalTrace);
    }

    stepmoveresult_t result = STEPMOVE_OK;
    if (horizontalTrace.fraction < 1.0f) {
        if (horizontalTrace.entityNum == ENTITYNUM_WORLD) {
            result = STEPMOVE_BLOCKED_BY_WORLD;
        } else {
            result          = STEPMOVE_BLOCKED_BY_ENTITY;
            _blockingEntity = horizontalTrace.ent ? horizontalTrace.ent->entity : NULL;
            act->AddStateFlag(STATE_FLAG_BLOCKED_BY_ENTITY);  
        }
    }

    Vector traceBegin(horizontalTrace.endpos);
    Vector traceEnd(traceBegin - (_step * 2.0f));

    verticalTrace = G_Trace(
        traceBegin, act->mins, act->maxs, traceEnd, act, act->edict->clipmask, qfalse, "MovementSubsystem::IsMoveValid3"
    );

    if (_isBlockedByFall(verticalTrace)) {
        return STEPMOVE_BLOCKED_BY_FALL;
    }

    if (act->feet_width) {
        verticalTrace = _feetWidthTrace(
            verticalTrace.endpos, Vector(verticalTrace.endpos) - (_step * 3.0f), verticalTrace.endpos,
            act->edict->clipmask
        );

        if (_isBlockedByFall(verticalTrace)) {
            return STEPMOVE_BLOCKED_BY_FALL;
        }
    }

    return result;
}

stepmoveresult_t MovementSubsystem::TryMove(void)
{
    if (!_shouldTryMove()) {
        return STEPMOVE_OK;
    }

    Vector moveBegin = act->origin;

    trace_t horizontalTrace;
    trace_t verticalTrace;
    verticalTrace.ent = 0;

    stepmoveresult_t returnValue = IsMoveValid(horizontalTrace, verticalTrace, moveBegin, moveBegin + _move);
    if (returnValue == STEPMOVE_OK) {
        act->setOrigin(verticalTrace.endpos);

        if (verticalTrace.fraction < 1.0f) {
            _saveGroundInformation(verticalTrace);
        }

        act->flags &= ~FL_PARTIALGROUND; 
        CheckWater();
    } else if (returnValue == STEPMOVE_BLOCKED_BY_FALL) {
        act->AddStateFlag(STATE_FLAG_STUCK);
    }

    return returnValue;
}

stepmoveresult_t MovementSubsystem::SimpleMove(const bool stickToGround)
{
    Vector newPosition(act->origin + _move);

    if (stickToGround) {
        Vector traceBegin(newPosition + _step);
        Vector traceEnd(traceBegin - (_step * 2.0f));

        trace_t trace = G_Trace(
            traceBegin, act->mins, act->maxs, traceEnd, act, MASK_PATHSOLID, qfalse, "MovementSubsystem::SimpleMove"
        );
        newPosition = trace.endpos;
    }

    act->setOrigin(newPosition);
    return STEPMOVE_OK;
}

qboolean MovementSubsystem::Push(const Vector& dir)
{
    Vector  oldorg;
    Vector  neworg;
    trace_t trace;
    int     i;

    if (!act->GetActorFlag(ACTOR_FLAG_PUSHABLE)) {
        return qfalse;
    }

    for (i = 0; i < 5; i++) {
        oldorg = act->origin + _step;
        neworg = oldorg + dir;

        trace = G_Trace(oldorg, act->mins, act->maxs, neworg, act, act->edict->clipmask, qfalse, "Actor::Push 1");

        if (trace.startsolid) {
            oldorg = act->origin;
            neworg = oldorg + dir;

            trace = G_Trace(oldorg, act->mins, act->maxs, neworg, act, act->edict->clipmask, qfalse, "Actor::Push 2");

            if (trace.startsolid) {
                return qfalse;
            }
        }

        if (trace.ent && trace.ent->entity && trace.ent->entity->isSubclassOf(Actor)) {
            Actor *actor = (Actor *)trace.ent->entity;
            if (actor->movementSubsystem) {
                actor->movementSubsystem->Push(dir);
            }
            continue;
        } else {
            break;
        }
    }

    if (Vector(trace.endpos) == oldorg) {
        return qfalse;
    }


    oldorg = trace.endpos;

    if (act->flags & FL_FLY) {
        neworg = oldorg - _step;
    } else {
        neworg = oldorg - _step * 2.0f;
    }

    trace = G_Trace(oldorg, act->mins, act->maxs, neworg, act, act->edict->clipmask, qfalse, "Actor::Push 3");

    act->setOrigin(trace.endpos);

    return qtrue;
}

void MovementSubsystem::CheckWater(void)
{
    Vector sample[3];
    int    cont;

    act->waterlevel = 0;
    act->watertype  = 0;

    sample[0] = act->origin;
    sample[2] = act->EyePosition();
    sample[1] = (sample[0] + sample[2]) * 0.5f;

    cont = gi.pointcontents(sample[0], 0);

    if ((cont != -1) && (cont & MASK_WATER)) {
        act->watertype  = cont;
        act->waterlevel = 1;

        cont = gi.pointcontents(sample[2], 0);
        if (cont & MASK_WATER) {
            act->waterlevel = 3;
        } else {
            cont = gi.pointcontents(sample[1], 0);
            if (cont & MASK_WATER) {
                act->waterlevel = 2;
            }
        }
    }
}

float MovementSubsystem::JumpTo(const Vector& targetPosition, const Angle angle)
{
    Trajectory trajectory(act->origin, targetPosition, angle, act->gravity * -sv_currentGravity->value);
    act->velocity = trajectory.GetInitialVelocity();

    Vector directionXY(targetPosition - act->origin);
    directionXY.z = 0.0f;
    act->setAngles(directionXY.toAngles());
    _movedir = directionXY;

    act->groundentity = NULL;
    return trajectory.GetTravelTime();
}

float MovementSubsystem::JumpTo(PathNode *goal, const Angle angle)
{
    if (goal) {
        return JumpTo(goal->origin, angle);
    } else {
        return 0;
    }
}

float MovementSubsystem::JumpTo(Entity *goal, const Angle angle)
{
    if (goal) {
        return JumpTo(goal->origin, angle);
    } else {
        return 0;
    }
}

qboolean MovementSubsystem::_canMoveSimplePath(const Vector& mins, const Vector& maxs, const Vector& pos) const
{
    trace_t trace = act->Trace( act->origin + _step, pos + _step, "Actor::_canMoveSimplePath" );  
    if (trace.fraction == 1.0f) {
        return qtrue;
    }

    return qfalse;
}

Vector MovementSubsystem::_getRealDestinationPosition(const Vector& pos) const
{
    Vector temp_dir    = pos - act->origin;
    float  temp_length = temp_dir.length();

    temp_length -= sqrt(act->maxs.x * act->maxs.x * 2.0f) + 5.0f;

    if (temp_length < 0.0f) {
        temp_length = 0.0f;
    }

    temp_dir.normalize();
    temp_dir *= temp_length;

    return act->origin + temp_dir;
}

trace_t MovementSubsystem::_feetWidthTrace(
    const Vector& currentLoc, const Vector& bottom, const Vector& endPos, const int mask
) const
{
    trace_t trace;
    Vector  temp_mins;
    Vector  temp_maxs;
    Vector  saved_endpos;

    temp_mins[0] = -act->feet_width;
    temp_mins[1] = -act->feet_width;
    temp_mins[2] = act->mins[2];

    temp_maxs[0] = act->feet_width;
    temp_maxs[1] = act->feet_width;
    temp_maxs[2] = act->maxs[2];

    trace = G_Trace(currentLoc, temp_mins, temp_maxs, bottom, act, mask, qfalse, "Actor::CanWalkTo2");

    saved_endpos = endPos;
    saved_endpos.copyTo(trace.endpos);

    return trace;
}

float MovementSubsystem::_getTraceStep() const
{
    float trace_step = act->maxs[0] * 2.0f; 

    trace_step = ((int)(trace_step / 8.0f)) * 8.0f;

    if (trace_step < 8.0f) {
        trace_step = 8.0f;
    }

    return trace_step;
}

qboolean MovementSubsystem::_checkHaveGroundEachStep(
    const Vector& start, const Vector& end, const Vector& test_mins, const Vector& test_maxs, const int mask
) const
{
    int clipMask = act->edict->clipmask;
    if (mask >= 0) {
        clipMask = mask;
    }

    Vector dir    = end - start;
    float  length = dir.length();
    dir.normalize();

    float trace_step       = _getTraceStep();
    float small_trace_step = 8;

    float last_height = end[2];

    Vector last_loc = Vector::Identity();

    for (float i = 0; i < length; i += trace_step) {
        Vector current_loc = start + (dir * i);
        current_loc[2]     = last_height + STEPSIZE;

        Vector bottom = current_loc;

        if (!act->GetActorFlag(ACTOR_FLAG_ALLOW_FALL)) {
            bottom[2] = last_height - STEPSIZE;
        } else {
            bottom[2] = last_height - 1000.0f;
        }

        trace_t trace = G_Trace(current_loc, test_mins, test_maxs, bottom, act, clipMask, qfalse, "Actor::CanWalkTo1");

        if (!((trace.fraction == 1.0f) || trace.startsolid || trace.allsolid) && act->feet_width) {
            trace = _feetWidthTrace(current_loc, bottom, trace.endpos, clipMask);
        }

        if ((trace.fraction == 1.0f) || trace.startsolid || trace.allsolid) {
            if ((i == 0) || (trace_step == small_trace_step)) {
                return qfalse;
            }

            for (float j = small_trace_step; j <= trace_step; j += small_trace_step) {
                current_loc    = last_loc + (dir * j);
                current_loc[2] = last_height + STEPSIZE;

                bottom    = current_loc;
                bottom[2] = last_height - STEPSIZE;

                trace = G_Trace(current_loc, test_mins, test_maxs, bottom, act, clipMask, qfalse, "Actor::CanWalkTo3");

                if ((trace.fraction == 1.0f) || trace.startsolid || trace.allsolid
                    || (trace.ent && trace.ent->entity && trace.ent->entity->isSubclassOf(Sentient)
                        && !act->GetActorFlag(ACTOR_FLAG_CAN_WALK_ON_OTHERS))) {
                    return qfalse;
                }

                if (act->feet_width) {
                    trace = _feetWidthTrace(current_loc, bottom, trace.endpos, clipMask);
                }

                last_height = trace.endpos[2];
            }
        }

        last_height = trace.endpos[2];
        last_loc    = current_loc;
    }

    if ((last_height > (end.z + (STEPSIZE * 2.0f))) || (last_height < (end.z - (STEPSIZE * 2.0f)))) {
        return qfalse;
    }

    return qtrue;
}

qboolean MovementSubsystem::_isBlockedByDoor(trace_t& trace) const
{
    Door *door;

    if (!act->deadflag && trace.ent && trace.ent->entity) {

        if (trace.ent->entity->isSubclassOf(Door)) {
            door = (Door *)trace.ent->entity;
            if (!door->locked && !door->isOpen()) {
                return qtrue;
            }
        }
    }

    return qfalse;
}

stepmoveresult_t MovementSubsystem::_noGravityTryMove(const Vector& oldorg, trace_t& verticalTrace) const
{
    Vector neworg = oldorg - _step;

    verticalTrace =
        G_Trace(oldorg, act->mins, act->maxs, neworg, act, act->edict->clipmask, qfalse, "Actor::TryMove 2");

    if (verticalTrace.startsolid) {
        return STEPMOVE_STUCK;
    }

    return STEPMOVE_OK;
}

qboolean MovementSubsystem::_isBlockedByFall(trace_t& trace) const
{
    qboolean allow_fall = _allowFall();

    if (trace.ent && trace.ent->entity && (trace.ent->entity->flags & FL_FLY) && !allow_fall) {
        return qtrue;
    }

    if (trace.ent && trace.ent->entity && trace.ent->entity->isSubclassOf(Sentient) && !allow_fall
        && !act->GetActorFlag(ACTOR_FLAG_CAN_WALK_ON_OTHERS)) {
        return qtrue;
    }

    if (trace.fraction == 1.0f) {
        if (allow_fall) {
            act->groundentity = NULL;
            return qfalse;
        } else {
            return qtrue;
        }
    }

    if ((trace.plane.normal[2] <= 0.7f) && !allow_fall) {
        return qtrue;
    }

    return qfalse;
}

qboolean MovementSubsystem::_allowFall() const
{
    if ((act->flags & FL_PARTIALGROUND)
        || (act->groundentity && act->groundentity->entity && (act->groundentity->entity->isSubclassOf(Sentient)))
        || (act->GetActorFlag(ACTOR_FLAG_ALLOW_FALL))) {
        return qtrue;
    }

    return qfalse;
}

qboolean MovementSubsystem::_shouldTryMove() const
{
    if ((act->velocity != vec_zero) && !act->deadflag) {
        return qfalse;
    }

    if ((_totallen <= 0.01f) || (_move == vec_zero)) {
        return qfalse;
    }

    return qtrue;
}

void MovementSubsystem::_saveGroundInformation(trace_t& trace)
{
    act->groundentity   = trace.ent;
    act->groundplane    = trace.plane;
    act->groundcontents = trace.contents;
    act->last_origin    = act->origin;
    act->SetActorFlag(ACTOR_FLAG_HAVE_MOVED, qtrue);
}



void MovementSubsystem::Archive(Archiver& arc)
{

    ArchiveEnum(_lastmove, stepmoveresult_t);
    arc.ArchiveFloat(&_forwardspeed);
    arc.ArchiveSafePointer(&_path);
    arc.ArchiveVector(&_move);
    arc.ArchiveVector(&_movedir);
    arc.ArchiveFloat(&_movespeed);
    arc.ArchiveVector(&_movevelocity);
    arc.ArchiveFloat(&_totallen);
    arc.ArchiveFloat(&_turnspeed);
    arc.ArchiveVector(&_animdir);
    arc.ArchiveVector(&_divedir);
    arc.ArchiveVector(&_startpos);
    arc.ArchiveBoolean(&_fliplegs);
    arc.ArchiveBoolean(&_movingBackwards);
    arc.ArchiveBool(&_faceEnemy);
    arc.ArchiveBool(&_adjustAnimDir);
    ArchiveEnum(_movementType, MovementType_t);
    arc.ArchiveBool(&_stickToGround);
    arc.ArchiveBool(&_useCodeDrivenSpeed);
    arc.ArchiveSafePointer(&_blockingEntity);
}

void MovementSubsystem::DoArchive(Archiver& arc, Actor *actor)
{
    Archive(arc);

    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_FATAL, "MovementSubsystem::DoArchive -- actor is NULL");
    }
}



const Vector& MovementSubsystem::getStep() const  
{
    return _step;
}

void MovementSubsystem::setLastMove(stepmoveresult_t lastMove)  
{
    _lastmove = lastMove;
}

stepmoveresult_t MovementSubsystem::getLastMove()  
{
    return _lastmove;
}

void MovementSubsystem::setMoveDir(const Vector& moveDir)  
{
    _movedir = moveDir;
}

Vector MovementSubsystem::getMoveDir()  
{
    return _movedir;
}

void MovementSubsystem::setAnimDir(const Vector& animDir)  
{
    _animdir = animDir;
}

Vector MovementSubsystem::getAnimDir()  
{
    return _animdir;
}

void MovementSubsystem::setDiveDir(const Vector& diveDir)  
{
    _divedir = diveDir;
}

Vector MovementSubsystem::getDiveDir()  
{
    return _divedir;
}

void MovementSubsystem::setTurnSpeed(float turnSpeed)  
{
    _turnspeed = turnSpeed;
}

float MovementSubsystem::getTurnSpeed()  
{
    return _turnspeed;
}

void MovementSubsystem::setForwardSpeed(float forwardSpeed)  
{
    _forwardspeed = forwardSpeed;
}

float MovementSubsystem::getForwardSpeed()  
{
    return _forwardspeed;
}

void MovementSubsystem::setMoveSpeed(float moveSpeed)  
{
    _movespeed = moveSpeed;
}

float MovementSubsystem::getMoveSpeed()  
{
    return _movespeed;
}

// SDK actor_locomotion.cpp:1897.  NOT the `!_fliplegs` one-liner this port used to
void MovementSubsystem::flipLegs(void)
{
    Vector Angles;

    _fliplegs = !_fliplegs;

    if (_fliplegs)
        _movingBackwards = true;
    else
        _movingBackwards = false;

    Angles = _animdir;
    Angles = Angles.toAngles();

    Angles[PITCH] = AngleNormalize180(Angles[PITCH]);
    Angles[ROLL]  = AngleNormalize180(Angles[ROLL]);

    Angles[YAW] = AngleNormalize180(Angles[YAW] + 180.0f);

    Angles.AngleVectors(&_animdir);

}

void MovementSubsystem::setMovingBackwards(qboolean backwards)  
{
    _movingBackwards = backwards;
}

qboolean MovementSubsystem::getMovingBackwards()  
{
    return _movingBackwards;
}

void MovementSubsystem::setFaceEnemy(bool faceEnemy)  
{
    _faceEnemy = faceEnemy;
}

bool MovementSubsystem::getFaceEnemy()  
{
    return _faceEnemy;
}

void MovementSubsystem::setAdjustAnimDir(bool adjustAnimDir)  
{
    _adjustAnimDir = adjustAnimDir;
}

bool MovementSubsystem::getAdjustAnimDir()  
{
    return _adjustAnimDir;
}

void MovementSubsystem::setMovementType(MovementType_t mType)  
{
    _movementType = mType;
}

MovementType_t MovementSubsystem::getMovementType()  
{
    return _movementType;
}

void MovementSubsystem::SetStickToGround(const bool stick)  
{
    _stickToGround = stick;
}

const bool MovementSubsystem::GetStickToGround(void) const  
{
    return _stickToGround;
}

Entity *MovementSubsystem::getBlockingEntity()  
{
    return _blockingEntity;
}

void MovementSubsystem::clearBlockingEntity()  
{
    _blockingEntity = NULL;
}





// DoArchive/Archive/_init -- the 9,025-file decompile corpus never mentions the name, and
