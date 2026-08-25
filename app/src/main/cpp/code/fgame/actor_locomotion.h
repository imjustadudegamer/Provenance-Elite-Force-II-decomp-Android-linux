
#pragma once

#include "g_local.h"
#include "g_phys.h" 
#include "path.h"    
#include "weapon.h"   
#include "steering.h"  

class Actor;
class Archiver;
class PathNode;

// drop defines its ten members in actor_locomotion.cpp:21-190 -- but RETAIL NEVER EMITS

#define TURN_SPEED 60


// without adding a virtual destructor retail does not have.
class MovementSubsystem final
{
public:
    MovementSubsystem();
    MovementSubsystem(Actor *actor);
    ~MovementSubsystem();

    qboolean CanMoveTo(const Vector& pos);
    bool     CanWalkTowardsPoint(const Vector& goalPoint, const int mask = -1);
    qboolean CanWalkTo(const Vector& pos, float bounding_box_extra = 0, int entnum = ENTITYNUM_NONE, const int mask = -1);
    qboolean CanWalkToFrom(
        const Vector& origin,
        const Vector& pos,
        float         bounding_box_extra = 0,
        int           entnum             = ENTITYNUM_NONE,
        const int     mask               = -1
    );

    void Accelerate(const Vector& steering);

    const Vector SteerTowardsPoint(
        const Vector& targetPosition,
        const Vector& targetVelocity,
        const Vector& moveDirection,
        const float   maxSpeed    = 1.0f,
        const bool    adjustSpeed = false
    );

    void CalcMove(void);

    stepmoveresult_t WaterMove(void);
    stepmoveresult_t AirMove(void);
    stepmoveresult_t
    IsMoveValid(trace_t& horizontalTrace, trace_t& verticalTrace, const Vector& moveBegin, const Vector& moveEnd);
    stepmoveresult_t TryMove(void);
    stepmoveresult_t SimpleMove(const bool stickToGround);

    qboolean Push(const Vector& dir);
    void     CheckWater(void);

    float JumpTo(const Vector& targetPosition, const Angle angle = 45.0f);
    float JumpTo(PathNode *goal, const Angle angle = 45.0f);
    float JumpTo(Entity *goal, const Angle angle = 45.0f);

    virtual void Archive(Archiver& arc);
    void         DoArchive(Archiver& arc, Actor *actor);


    void          setStep(const Vector& step) { _step = step; }
    const Vector& getStep() const;

    void  setPath(Path *path) { _path = path; }
    Path *getPath() { return _path; }

    void             setLastMove(stepmoveresult_t lastMove);
    stepmoveresult_t getLastMove();

    void   setMove(const Vector& move) { _move = move; }
    Vector getMove() { return _move; }

    void   setMoveDir(const Vector& moveDir);
    Vector getMoveDir();

    void   setMoveVelocity(const Vector& moveVelocity) { _movevelocity = moveVelocity; }
    Vector getMoveVelocity() { return _movevelocity; }

    void   setAnimDir(const Vector& animDir);
    Vector getAnimDir();

    void   setDiveDir(const Vector& diveDir);
    Vector getDiveDir();

    void   setStartPos(const Vector& startPos) { _startpos = startPos; }
    Vector getStartPos() { return _startpos; }

    void  setTotalLen(float totalLen) { _totallen = totalLen; }
    float getTotalLen() { return _totallen; }

    void  setTurnSpeed(float turnSpeed);
    float getTurnSpeed();

    void  setForwardSpeed(float forwardSpeed);
    float getForwardSpeed();

    void  setMoveSpeed(float moveSpeed);
    float getMoveSpeed();

    void     setFlipLegs(qboolean flip) { _fliplegs = flip; }
    qboolean getFlipLegs() { return _fliplegs; }
    void     flipLegs(void);

    void     setMovingBackwards(qboolean backwards);
    qboolean getMovingBackwards();

    void setFaceEnemy(bool faceEnemy);
    bool getFaceEnemy();

    void setAdjustAnimDir(bool adjustAnimDir);
    bool getAdjustAnimDir();

    void           setMovementType(MovementType_t mType);
    MovementType_t getMovementType();

    void       SetStickToGround(const bool stick);
    const bool GetStickToGround(void) const;

    void setUseCodeDrivenSpeed(bool useCode) { _useCodeDrivenSpeed = useCode; }
    bool getUseCodeDrivenSpeed() { return _useCodeDrivenSpeed; }

    Entity *getBlockingEntity();
    void    clearBlockingEntity();



protected:
    Vector           _getRealDestinationPosition(const Vector& pos) const;
    stepmoveresult_t _noGravityTryMove(const Vector& oldorg, trace_t& verticalTrace) const;
    trace_t
    _feetWidthTrace(const Vector& currentLoc, const Vector& bottom, const Vector& endPos, const int mask) const;
    float    _getTraceStep() const;
    void     _saveGroundInformation(trace_t& trace);
    qboolean _isBlockedByDoor(trace_t& trace) const;
    qboolean _canMoveSimplePath(const Vector& mins, const Vector& maxs, const Vector& pos) const;
    qboolean _isBlockedByFall(trace_t& trace) const;
    qboolean _allowFall() const;
    qboolean _shouldTryMove() const;
    qboolean _checkHaveGroundEachStep(
        const Vector& start,
        const Vector& end,
        const Vector& test_mins,
        const Vector& test_maxs,
        const int     mask = -1
    ) const;

    void _init();

private:
    static Vector _step;

    stepmoveresult_t _lastmove;
    float            _forwardspeed;
    PathPtr          _path;  
    Vector           _move;
    Vector           _movedir;
    float            _movespeed;
    Vector           _movevelocity;
    float            _totallen;
    float            _turnspeed;
    Vector           _animdir;
    Vector           _divedir;
    Vector           _startpos;
    qboolean         _fliplegs;
    qboolean         _movingBackwards;
    bool             _faceEnemy;
    bool             _adjustAnimDir;
    MovementType_t   _movementType;
    bool             _stickToGround;
    bool             _useCodeDrivenSpeed;

    bool _steeringCommandedMoveDir; 

    Actor    *act;
    EntityPtr _blockingEntity;

    Vector _blockingNormal;
};
