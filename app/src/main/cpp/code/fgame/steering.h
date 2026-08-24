//-----------------------------------------------------------------------------
//
//  $Logfile:: /Code/DLLs/game/steering.h                                    $
// $Revision:: 21                                                             $
//   $Author:: Steven                                                         $
//     $Date:: 10/13/03 8:54a                                                 $
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

#pragma once

#include "../corepp/listener.h"
#include "entity.h"
#include "g_phys.h" 
#include "navigate.h"

class Actor;

#ifdef ERROR
#    undef ERROR
#endif

class Steering : public Listener
{
public:
    enum ReturnValue {
        EVALUATING,
        EVALUATING_REACHED_NODE,
        SUCCESS,
        FAILED_BLOCKED_BY_ENEMY,
        FAILED_BLOCKED_BY_CIVILIAN,
        FAILED_BLOCKED_BY_FRIEND,
        FAILED_BLOCKED_BY_TEAMMATE,
        FAILED_BLOCKED_BY_WORLD,
        FAILED_BLOCKED_BY_FALL,
        FAILED_BLOCKED_BY_DOOR,
        FAILED_CANNOT_GET_TO_PATH,
        FAILED_NO_PATH,
        FAILED,
        ERROR,
        NUMBER_OF_RETURN_VALUES
    };

    CLASS_PROTOTYPE(Steering);

    Steering();

    virtual void              ShowInfo(Actor& self);
    virtual void              Begin(Actor& self);
    virtual const ReturnValue Evaluate(Actor& self);
    virtual void              End(Actor& self);

    virtual void ResetForces(void);

    virtual const Vector& GetSteeringForce(void) const { return _steeringForce; }
    virtual void          SetSteeringForce(const Vector& steeringForce);
    virtual const Vector& GetPosition(void) const { return _origin; }
    virtual void          SetPosition(const Vector& pos);  
    virtual const Vector& GetMoveDirection(void) const { return _moveDirection; }
    virtual void          SetMoveDirection(const Vector& dir);  
    virtual const float   GetMaxSpeed(void) const { return _maxSpeed; }
    virtual void          SetMaxSpeed(float speed);             
    virtual void          DrawForces(void);                     

    virtual void Archive(Archiver& arc) override;

private:
    Vector _steeringForce;
    Vector _origin;
    Vector _moveDirection;
    float  _maxSpeed;
};

inline void Steering::Archive(Archiver& arc)
{

    arc.ArchiveVector(&_steeringForce);
    arc.ArchiveVector(&_origin);
    arc.ArchiveVector(&_moveDirection);
    arc.ArchiveFloat(&_maxSpeed);
}

enum JumpStates {
    JUMP_PREPARE,
    JUMP_LAUNCH,
    JUMP_FALL,
    JUMP_LANDING
};

class Jump : public Steering
{
public:
    CLASS_PROTOTYPE(Jump);

    Jump();

    void SetGoal(const Vector& goalPosition);
    void SetEntity(Entity *ent_to_jump_to);
    void SetLaunchAngle(const float launchAngle) { _launchAngle = launchAngle; }

    virtual const ReturnValue Evaluate(Actor& self) override;

    virtual void Archive(Archiver& arc) override;

private:
    float  _endtime;
    Angle  _launchAngle;
    str    _anim;
    int    _state;
    bool   _animdone;
    Vector _goal;






public:
    void Begin( Actor &self ) override; 
    void End( Actor &self ) override; 
};

inline void Jump::Archive(Archiver& arc)
{
    Steering::Archive(arc);

    arc.ArchiveFloat(&_endtime);
    arc.ArchiveFloat(&_launchAngle.GetValue());
    arc.ArchiveString(&_anim);
    arc.ArchiveInteger(&_state);
    arc.ArchiveBool(&_animdone);
    arc.ArchiveVector(&_goal);
}

class FollowNode : public Class
{
public:
    CLASS_PROTOTYPE(FollowNode);

    FollowNode(void);
    FollowNode(
        const Vector& position, const bool isTemporary = false, const bool isJumpNode = false, const float jumpAngle = 45.0f
    );

    Vector const& GetPosition(void) const { return _position; }
    const bool    GetIsTemporary(void) const { return _isTemporary; }
    const bool    GetIsJumpNode(void) const { return _isJumpNode; }
    void          SetIsJumpNode(const bool isJumpNode) { _isJumpNode = isJumpNode; }
    const float   GetJumpAngle(void) const { return _jumpAngle; }
    void          SetJumpAngle(const float jumpAngle) { _jumpAngle = jumpAngle; }

    virtual void Archive(Archiver& arc) override;

private:
    Vector _position;
    bool   _isTemporary;
    bool   _isJumpNode;
    float  _jumpAngle;
};

typedef SafePtr<FollowNode> FollowNodePtr;

inline void FollowNode::Archive(Archiver& arc)
{
    Class::Archive(arc);

    arc.ArchiveVector(&_position);
    arc.ArchiveBool(&_isTemporary);
    arc.ArchiveBool(&_isJumpNode);
    arc.ArchiveFloat(&_jumpAngle);
}

enum PathCreationReturnCodes {
    PATH_CREATION_SUCCESS,
    PATH_CREATION_FAILED_DEGENERATE_PATH,
    PATH_CREATION_FAILED_NODES_NOT_CONNECTED,
    PATH_CREATION_FAILED_START_NODE_NOT_FOUND,
    PATH_CREATION_FAILED_END_NODE_NOT_FOUND
};

class FollowNodePath : public Class
{
public:
    CLASS_PROTOTYPE(FollowNodePath);

    FollowNodePath();
    ~FollowNodePath() { Clear(); }

    void          AddNode(FollowNodePtr node);  
    void          RemoveNode(FollowNodePtr node);
    void          InsertNode(FollowNodePtr node, const int index);
    FollowNodePtr GetPreviousNode(const FollowNodePtr node);
    FollowNodePtr GetNextNode(const FollowNodePtr node);

    const Steering::ReturnValue Evaluate(Actor& self, const Vector& goalPosition);
    unsigned int const          SetPath(Actor& self, const Vector& from, const Vector& to);

    void Clear();
    void Draw(void);

    void          SetCurrentNode(FollowNodePtr node);                     
    FollowNodePtr GetCurrentNode(void) const;                             
    int           GetCurrentNodeIndex(void) { return GetNodeIndex(GetCurrentNode()); }
    FollowNodePtr GetNodeAt(const int index);
    int const     GetNodeIndex(const FollowNodePtr node);                 
    FollowNodePtr FirstNode(void);                                        
    FollowNodePtr LastNode(void);                                         
    int const     NumNodes(void) const { return _nodes.NumObjects(); }

    virtual void Archive(Archiver& arc) override;

private:
    void                        BuildFromPathNodes(Path *path, const Actor& self);
    const bool                  FindNewCurrentNode(const Actor& self);
    const Steering::ReturnValue AdvanceCurrentNode(const Actor& self);
    const Steering::ReturnValue RetreatCurrentNode(Actor& self, const Vector& goalPosition);

    Container<FollowNodePtr> _nodes;
    FollowNodePtr            _currentNode;
};

class FollowPath : public Steering
{
public:
    CLASS_PROTOTYPE(FollowPath);

    FollowPath();
    virtual ~FollowPath() { DeleteTemporaryPathNodes(); }

    virtual const Vector& GetGoalPosition(void) const { return vec_zero; }
    virtual const float   GetGoalRadius(void) const { return 0.0f; }
    virtual FollowNodePath& GetPath(void) { return _path; }
    virtual const float   GetRadius(void) const { return _radius; }
    virtual void          SetRadius(const float radius) { _radius = radius; }
    virtual const float   GetAvoidanceDistance(void) const { return _avoidanceDistance; }
    virtual void          SetAvoidanceDistance(const float avoidanceDistance) { _avoidanceDistance = avoidanceDistance; }
    virtual void          SetSteeringForceAndDesiredHeading(Actor& self, const Vector& steeringForce);

    virtual void              Begin(Actor& self) override;
    virtual const ReturnValue Evaluate(Actor& self) override;
    virtual void              ShowInfo(Actor& self) override;

    virtual void Archive(Archiver& arc) override;

protected:
    virtual void              DeleteTemporaryPathNodes(void);
    virtual const bool        ClearTraceToGoal(Actor& self, const trace_t& traceToGoal, const float radius) const;
    virtual const bool        DoneTurning(Actor& self) const;
    virtual const ReturnValue GotoCurrentNode(Actor& self);
    virtual const ReturnValue GotoGoal(Actor& self);
    virtual const bool        BuildAvoidancePath(
               Actor&        self,
               const bool    passOnTheLeft,
               const Vector& obstaclePosition,
               const float   avoidanceRadius,
               const bool    pursueGoal
           );
    virtual const ReturnValue AvoidObstacle(Actor& self, const trace_t& trace, const bool pursueGoal = false);
    virtual const bool        TraceBlockedByEntity(Actor& self, const trace_t& trace) const;
    virtual const bool        CheckBlocked(Actor& self);
    virtual const ReturnValue ReturnBlockingObject(const trace_t& trace) const;
    virtual const bool        AtDestination(Actor& self) const;
    virtual void              SteerToCurrentNode(Actor& self);
    virtual void              SteerToGoal(Actor& self);
    virtual void              FreePath(void) { _path.Clear(); }
    virtual void              AddPathNode(FollowNodePtr node) { GetPath().AddNode(node); }
    virtual void              InsertPathNode(FollowNodePtr node, const int index)
    {
        GetPath().InsertNode(node, index);
        SetCurrentNode(node);
    }
    virtual FollowNodePtr GetCurrentNode(void) const { return _path.GetCurrentNode(); }
    virtual void          SetCurrentNode(FollowNodePtr node) { _path.SetCurrentNode(node); }

    // PORT-ONLY.  This is the seam described at the head of the file: retail's

private:
    float          _radius;
    FollowNodePath _path;
    float          _avoidanceDistance;
    Vector         _desiredHeading;
    Jump           _jump;
    qboolean       _jumping;
    str            _oldAnim;
};

inline void FollowPath::Archive(Archiver& arc)
{
    Steering::Archive(arc);

    arc.ArchiveFloat(&_radius);
    arc.ArchiveObject(&_path);
    arc.ArchiveFloat(&_avoidanceDistance);
    arc.ArchiveVector(&_desiredHeading);
    arc.ArchiveObject(&_jump);
    arc.ArchiveBoolean(&_jumping);
    arc.ArchiveString(&_oldAnim);
}

class FollowPathToPoint : public FollowPath
{
public:
    CLASS_PROTOTYPE(FollowPathToPoint);

    FollowPathToPoint();

    virtual void SetGoal(const Vector &targetPoint, const float radius, Actor &self);

    virtual const Vector& GetGoalPosition(void) const override { return _targetPoint; }
    virtual const float   GetGoalRadius(void) const override { return 16.0f; }

    virtual void Archive(Archiver& arc) override;

private:
    Vector _targetPoint;
};

inline void FollowPathToPoint::Archive(Archiver& arc)
{
    FollowPath::Archive(arc);

    arc.ArchiveVector(&_targetPoint);
}

class FollowPathToEntity : public FollowPath
{
public:
    CLASS_PROTOTYPE(FollowPathToEntity);

    FollowPathToEntity();

    virtual void SetGoal(Entity *ent, const float radius, Actor &self);

    virtual const Vector& GetGoalPosition(void) const override;
    virtual const float   GetGoalRadius(void) const override;

    virtual const ReturnValue Evaluate(Actor& self) override;


    virtual void Archive(Archiver& arc) override;

protected:
    virtual const bool ClearTraceToGoal(Actor& self, const trace_t& traceToGoal, const float radius) const override;

private:
    EntityPtr _targetEntity;
    Vector    _oldGoalPosition;
};

inline void FollowPathToEntity::Archive(Archiver& arc)
{
    FollowPath::Archive(arc);

    arc.ArchiveSafePointer(&_targetEntity);
    arc.ArchiveVector(&_oldGoalPosition);
}
