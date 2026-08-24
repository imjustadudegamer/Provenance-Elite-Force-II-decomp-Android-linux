

#pragma once

#include "g_local.h"
#include "entity.h"
#include "camera.h"
#include "../corepp/container.h"

template<class T> class SafePtr;
class Actor;
typedef SafePtr<Actor> ActorPtr;

#define CIN_FRAMETIME 0.05f

class Cinematic;


class CinematicOrigin : public Listener
{
public:
    CLASS_PROTOTYPE(CinematicOrigin);

    CinematicOrigin();
    ~CinematicOrigin() {}

    Vector      getOrigin(void) { return _origin; }
    const str&  getName(void) { return _name; }
    float       getYaw(void) { return _yaw; }

    void setOrigin(const Vector origin) { _origin = origin; }
    void setYaw(float yaw) { _yaw = yaw; }
    void setName(const str& name) { _name = name; }

    qboolean parse(Script& cinematicFile);
    void     debug(void);

    void Archive(Archiver& arc) override;

private:
    Vector _origin;
    str    _name;
    float  _yaw;


};


class CinematicActor : public Listener
{
public:
    typedef enum {
        CINEMATIC_ACTOR_STATE_INACTIVE,
        CINEMATIC_ACTOR_STATE_MOVING,
        CINEMATIC_ACTOR_STATE_TURNING,
        CINEMATIC_ACTOR_STATE_PLAYING,
        CINEMATIC_ACTOR_STATE_FINISHED
    } CinematicActorState;

    // and the parser's *default* is KILL, not LEAVE (SDK .cpp:440-467).
    typedef enum {
        CINEMATIC_ACTOR_AFTER_BEHAVIOR_REMOVE_FROM_GAME,
        CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_WITH_AI,
        CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_NO_AI,
        CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_FREEZE,
        CINEMATIC_ACTOR_AFTER_BEHAVIOR_KILL,
        NUM_CINEMATIC_ACTOR_AFTER_BEHAVIORS
    } CinematicActorAfterBehavior;

    CLASS_PROTOTYPE(CinematicActor);

    CinematicActor();
    ~CinematicActor() {}

    bool isAtSpot(void) { return _isAtSpot; }
    bool isAnimDone(void) { return _isAnimDone; }
    bool isRootActor(void) { return _rootActor; }
    bool doesSnapToSpot(void) { return _snapToSpot; }
    bool doesIgnorePain(void) { return _ignorePain; }
    bool doesIgnoreSight(void) { return _ignoreSight; }
    bool doesIgnoreSound(void) { return _ignoreSound; }
    bool doesRemoveAfterCinematic(void) { return _removeAfter; }

    const str& getName(void) { return _name; }
    const str& getTiki(void) { return _tiki; }
    const str& getAnim(void) { return _anim; }
    const str& getMoveAnim(void) { return _moveAnim; }
    Vector     getOrigin(void) { return _origin; }
    ActorPtr   getActor(void) { return _actor; }

    CinematicActorAfterBehavior getAfterBehavior(void) { return _afterBehavior; }

    void setName(const str& name) { _name = name; }
    void setTiki(const str& tiki) { _tiki = tiki; }
    void setMoveAnim(const str& anim) { _moveAnim = anim; }
    void setOriginOffset(const Vector& off) { _originOffset = off; }
    void setYawOffset(float yawOffset) { _yawOffset = yawOffset; }
    void setSnapToSpot(bool snapToSpot) { _snapToSpot = snapToSpot; }
    void setIgnorePain(bool ignorePain) { _ignorePain = ignorePain; }
    void setIgnoreSight(bool ignoreSight) { _ignoreSight = ignoreSight; }
    void setIgnoreSound(bool ignoreSound) { _ignoreSound = ignoreSound; }
    void setRemoveAfter(bool removeAfter) { _removeAfter = removeAfter; }
    void setRootActor(bool rootActor) { _rootActor = rootActor; }
    void setAlwaysSpawn(bool alwaysSpawn) { _alwaysSpawn = alwaysSpawn; }
    void setAfterBehavior(const str& s);
    void setAnim(const str& anim);

    qboolean parse(Script& cinematicFile);  
    void     getToPosition(void);
    void     takeControlOfActor(const Vector& origin = Vector(0.0f, 0.0f, 0.0f), float yaw = 0.0f);
    void     releaseControlOfActor(void);
    void     playAnimation(void);
    void     reset(void);  
    void     debug(void);  

    void Archive(Archiver& arc) override;

    void actorBehaviorFinishedEvent(Event *ev);
    void actorControlLostEvent(Event *ev);   

protected:
    void _spawn(void);
    void _turnActor(bool useAnims);
    void _locateActor(void);

private:
    str                         _name;
    str                         _tiki;
    str                         _anim;
    str                         _moveAnim;
    Vector                      _origin;
    Vector                      _originOffset;
    ActorPtr                    _actor;
    float                       _yaw;
    float                       _yawOffset;
    bool                        _snapToSpot;
    bool                        _ignorePain;
    bool                        _ignoreSight;
    bool                        _ignoreSound;
    bool                        _isAtSpot;
    bool                        _isAnimDone;
    bool                        _hasActorControl;
    bool                        _rootActor;
    bool                        _removeAfter;
    bool                        _alwaysSpawn;
    CinematicActorAfterBehavior _afterBehavior;
    CinematicActorState         _state;




};


class CinematicCamera : public Listener
{
public:
    typedef enum {
        CAMERA_MOVE_TYPE_STATIC,
        CAMERA_MOVE_TYPE_FOLLOW_ANIM,
        CAMERA_MOVE_TYPE_FOLLOW_PLAYER,
        CAMERA_MOVE_TYPE_FOLLOW_ACTOR,
    } CameraMoveType;

    typedef enum {
        CAMERA_LOOK_TYPE_WATCH_ANIM,
        CAMERA_LOOK_TYPE_WATCH_PLAYER,
        CAMERA_LOOK_TYPE_WATCH_ACTOR,
    } CameraLookType;

    CLASS_PROTOTYPE(CinematicCamera);

    CinematicCamera();
    ~CinematicCamera() {}

    bool isAnimDone(void) { return !_playing; }  
    bool isSelfRemoving(void) { return _selfRemoving; }  

    const str& getName(void) { return _name; }
    const str& getMoveActor(void) { return _moveActor; }
    const str& getLookActor(void) { return _lookActor; }

    void setName(const str& name) { _name = name; }
    void setCamFile(const str& camFile) { _camFile = camFile; }
    void setOriginOffset(const Vector& originOffset) { _originOffset = originOffset; }
    void setYawOffset(float yawOffset) { _yawOffset = yawOffset; }
    void setMoveActor(const str& moveActor) { _moveActor = moveActor; }
    void setLookActor(const str& lookActor) { _lookActor = lookActor; }
    void setMoveType(CameraMoveType moveType) { _moveType = moveType; }
    void setLookType(CameraLookType lookType) { _lookType = lookType; }
    void setSelfRemovingFlag(bool selfRemoving) { _selfRemoving = selfRemoving; }  
    void setMoveType(const str& moveType);
    void setLookType(const str& lookType);

    void     start(void);
    void     cut(void);
    void     takeControlOfCamera(const Vector& origin = Vector(0.0f, 0.0f, 0.0f), float yaw = 0.0f);
    void     releaseControlOfCamera(void);
    qboolean parse(Script& cinematicFile);  
    void     reset(void);                   
    void     debug(void) {}

    void Archive(Archiver& arc) override;

protected:
    void _locateCamera(void);
    void _handleStopPlayingEvent(Event *ev);

private:
    CameraMoveType _moveType;
    CameraLookType _lookType;
    CameraPtr      _camera;
    Vector         _originOffset;
    float          _yawOffset;
    bool           _playing;       
    bool           _selfRemoving;  

    static_assert(sizeof(_playing) == 1 && sizeof(_selfRemoving) == 1, "retail archives these as 1-byte bools");
    str            _name;
    str            _camFile;
    str            _moveActor;
    str            _lookActor;




public:
    void _spawn(); 
};


class CinematicCut : public Listener
{
public:
    CLASS_PROTOTYPE(CinematicCut);

    CinematicCut();
    ~CinematicCut() {}

    bool doesLerp(void) { return _lerpFlag; }  

    CinematicCamera *getCinematicCamera(void) { return _cinematicCamera; }
    const str&       getCameraName(void) { return _cameraName; }
    int              getFrame(void) { return _frame; }
    int              getFadeOut(void) { return _fadeOut; }
    int              getFadeIn(void) { return _fadeIn; }

    void setCinematicCamera(CinematicCamera *camera) { _cinematicCamera = camera; }
    void setCameraName(const str& name) { _cameraName = name; }
    void setFrame(int frame) { _frame = frame; }
    void setFadeOut(int fadeOut) { _fadeOut = fadeOut; }
    void setFadeIn(int fadeIn) { _fadeIn = fadeIn; }
    void setLerpFlag(bool lerpFlag) { _lerpFlag = lerpFlag; }  

    void     postEvents(void);
    qboolean parse(Script& cinematicFile);  
    void     reset(void);                  // SDK CinematicArmature.h:352 (no retail body; Cinematic::reset() never touches cuts)
    void     stop(void);
    void     debug(void) {}

    void Archive(Archiver& arc) override;

protected:
    void _handleFadeOutEvent(Event *event);
    void _handleCutEvent(Event *event);

private:
    CinematicCamera *_cinematicCamera;
    bool             _lerpFlag;  
    static_assert(sizeof(_lerpFlag) == 1, "retail archives this as a 1-byte bool");
    int              _frame;
    int              _fadeOut;
    int              _fadeIn;
    str              _cameraName;
};


class Cinematic : public Entity
{
public:
    CLASS_PROTOTYPE(Cinematic);

    typedef enum {
        CINEMATIC_STAGE_UNLOADED,
        CINEMATIC_STAGE_READY,
        CINEMATIC_STAGE_WAITING_FOR_ACTORS,
        CINEMATIC_STAGE_ANIMATING,
        CINEMATIC_STAGE_FINISHED,
    } CinematicStage;

    Cinematic();
    Cinematic(const str& filename);  
    virtual ~Cinematic() override;
    bool load(void);

    bool doesResetCamera(void) { return _resetCamera; }  

    str& getStartThread(void) { return _startThread; }
    str& getStopThread(void) { return _stopThread; }
    str& getFilename(void) { return _filename; }

    void setResetCameraFlag(bool resetCamera) { _resetCamera = resetCamera; }  
    void setLooping(bool looping) { _looping = looping; }  
    void setFilename(const str& filename) { _filename = filename; }
    void setStartThread(const str& startThread) { _startThread = startThread; }
    void setStopThread(const str& stopThread) { _stopThread = stopThread; }



    void startAtNamedOrigin(const str& originName, qboolean callStartThread = qtrue);
    void startAtOrigin(const Vector& origin = Vector(0.0f, 0.0f, 0.0f), float yaw = 0.0f,
                       qboolean callStartThread = qtrue);

    virtual void Think(void) override;
    virtual void Archive(Archiver& arc) override;

    void handleBeginEvent(Event *ev);
    void handleBeginAtEvent(Event *ev);
    void handleEndEvent(Event *ev);

protected:
    CinematicActor  *getCinematicActorByName(const str& name);
    CinematicCamera *getCinematicCameraByName(const str& cameraName);
    CinematicOrigin *getCinematicOriginByName(const str& originName);

    void init(void);

    qboolean parseActors(Script& cinematicFile);
    qboolean parseActor(Script& cinematicFile);
    qboolean parseCameras(Script& cinematicFile);
    qboolean parseCamera(Script& cinematicFile);
    qboolean parseCut(Script& cinematicFile);
    qboolean parseObjects(Script& cinematicFile);
    qboolean parseObject(Script& cinematicFile);
    qboolean parseOrigins(Script& cinematicFile);
    qboolean parseOrigin(Script& cinematicFile);
    qboolean parseOpenBlock(Script& cinematicFile, const str& blockName, const str& openToken = "{");
    bool parseCloseBlock(Script& cinematicFile, const str& blockName, const str& closeToken = "}");

    void setStartThreadEvent(Event *ev);
    void setStopThreadEvent(Event *ev);

    qboolean areActorsAtTheirPlaces(void);
    qboolean checkForCompletion(void);

    void _startAnimation(void);
    void _endAnimation(void);

private:
    Container<CinematicActor *>  _actorList;
    Container<CinematicCamera *> _cameraList;
    Container<CinematicCut *>    _cutList;
    Container<CinematicOrigin *> _originList;
    str                          _filename;
    str                          _startThread;
    str                          _stopThread;
    bool                         _looping;              
    bool                         _playing;              
    bool                         _callStartThreadFlag;  
    bool                         _resetCamera;          

    static_assert(
        sizeof(_looping) == 1 && sizeof(_playing) == 1 && sizeof(_callStartThreadFlag) == 1 && sizeof(_resetCamera) == 1,
        "retail archives these as 1-byte bools"
    );
    CinematicStage               _stage;




public:
    void reset( void ); 
    void start( qboolean callStartThread = qtrue ); 
    void stop( qboolean callEndThread = qtrue ); 
    void handleSetBeginThreadEvent( Event *ev ); 
    void handleSetEndThreadEvent( Event *ev ); 
    void debug( void ); 

};

typedef SafePtr<Cinematic> CinematicPtr;
