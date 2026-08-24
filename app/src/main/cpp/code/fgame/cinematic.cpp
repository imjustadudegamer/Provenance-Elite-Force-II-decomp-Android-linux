

#include "g_local.h"
#include "entity.h"
#include "actor.h"
#include "g_spawn.h"
#include "g_phys.h"
#include "camera.h"
#include "cinematic.h"
#include "scriptslave.h"
#include "scriptmaster.h"
#include "animate.h"
#include "gamecvars.h"
#include "debuglines.h"
#include "scriptexception.h"
#include "../corepp/script.h"


#define KFC_FRAMETIME CIN_FRAMETIME






CLASS_DECLARATION(Listener, CinematicOrigin, NULL) {
    {NULL, NULL}
};

CinematicOrigin::CinematicOrigin()
    : _origin(0.0f, 0.0f, 0.0f)
    , _name("")
    , _yaw(0.0f)
{}

void CinematicOrigin::Archive(Archiver& arc)
{

    arc.ArchiveVector(&_origin);
    arc.ArchiveString(&_name);
    arc.ArchiveFloat(&_yaw);
}

void CinematicOrigin::debug(void)
{
    G_DebugBBox(_origin, Vector(-8, -8, -8), Vector(8, 8, 8), 0.8f, 0.6f, 0.8f, 1.0f);
}

qboolean CinematicOrigin::parse(Script& cinematicFile)
{
    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!Q_stricmp(token, "name")) {
            setName(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "origin")) {
            setOrigin(cinematicFile.GetVector(false));
        } else if (!Q_stricmp(token, "yaw")) {
            setYaw(cinematicFile.GetFloat(false));
        } else if (!Q_stricmp(token, "}")) {
            break;
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}


// name retail has never had; it is deleted and the retail event is wired instead.
CLASS_DECLARATION(Listener, CinematicActor, NULL) {
    {&EV_Actor_ControlLost,      &CinematicActor::actorControlLostEvent      },
    {&EV_Actor_BehaviorFinished, &CinematicActor::actorBehaviorFinishedEvent },
    {NULL,                       NULL                                        }
};

CinematicActor::CinematicActor()
    : _name("")
    , _tiki("")
    , _anim("")
    , _moveAnim("walk")
    , _origin(0.0f, 0.0f, 0.0f)
    , _originOffset(0.0f, 0.0f, 0.0f)
    , _yaw(0.0f)
    , _yawOffset(0.0f)
    , _snapToSpot(false)
    , _ignorePain(true)
    , _ignoreSight(true)
    , _ignoreSound(true)
    , _isAtSpot(false)
    , _isAnimDone(false)
    , _hasActorControl(false)
    , _rootActor(false)
    , _removeAfter(false)
    , _alwaysSpawn(false)
    , _afterBehavior(CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_WITH_AI)
    , _state(CINEMATIC_ACTOR_STATE_INACTIVE)
{
}

void CinematicActor::Archive(Archiver& arc)
{
    arc.ArchiveString(&_name);
    arc.ArchiveString(&_tiki);
    arc.ArchiveString(&_anim);
    arc.ArchiveString(&_moveAnim);
    arc.ArchiveVector(&_origin);
    arc.ArchiveVector(&_originOffset);

    arc.ArchiveSafePointer(&_actor);

    arc.ArchiveFloat(&_yaw);
    arc.ArchiveFloat(&_yawOffset);
    arc.ArchiveBool(&_snapToSpot);
    arc.ArchiveBool(&_ignorePain);
    arc.ArchiveBool(&_ignoreSight);
    arc.ArchiveBool(&_ignoreSound);
    arc.ArchiveBool(&_isAtSpot);
    arc.ArchiveBool(&_isAnimDone);
    arc.ArchiveBool(&_hasActorControl);
    arc.ArchiveBool(&_rootActor);
    arc.ArchiveBool(&_removeAfter);
    arc.ArchiveBool(&_alwaysSpawn);

    ArchiveEnum(_afterBehavior, CinematicActorAfterBehavior);
    ArchiveEnum(_state, CinematicActorState);
}

void CinematicActor::setAnim(const str& anim)
{
    _anim = anim;
    CacheResource(_anim);
}

void CinematicActor::setAfterBehavior(const str& actorBehaviorName)
{
    if (actorBehaviorName == "REMOVE") {
        _afterBehavior = CINEMATIC_ACTOR_AFTER_BEHAVIOR_REMOVE_FROM_GAME;
        _removeAfter   = true;
    } else if (actorBehaviorName == "LEAVE" || actorBehaviorName == "AI") {
        _afterBehavior = CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_WITH_AI;
    } else if (actorBehaviorName == "NOAI") {
        _afterBehavior = CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_NO_AI;
    } else if (actorBehaviorName == "FREEZE") {
        _afterBehavior = CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_FREEZE;
    } else if (actorBehaviorName == "KILL") {
        _afterBehavior = CINEMATIC_ACTOR_AFTER_BEHAVIOR_KILL;
    } else {
        _afterBehavior = CINEMATIC_ACTOR_AFTER_BEHAVIOR_KILL;
    }
}

void CinematicActor::actorBehaviorFinishedEvent(Event *ev)
{
    assert(_actor);

    switch (_state) {
    case CINEMATIC_ACTOR_STATE_MOVING:
        _actor->setOrigin(_origin);
        _turnActor(true);
        break;
    case CINEMATIC_ACTOR_STATE_TURNING:
        _isAtSpot = true;
        _state    = CINEMATIC_ACTOR_STATE_PLAYING;
        break;
    case CINEMATIC_ACTOR_STATE_PLAYING:
        _isAnimDone = true;
        _state      = CINEMATIC_ACTOR_STATE_FINISHED;
        break;
    default:
        break;
    }
}

// (SDK actor.cpp:326) and takes a float yaw AND an int useAnims -- not the port's
void CinematicActor::_turnActor(bool useAnims)
{
    assert(_actor);

    Event *ev = new Event(EV_Actor_TurnTo);
    ev->AddFloat(_yaw);
    ev->AddInteger(useAnims);
    _actor->PostEvent(ev, 0.0f);
    _state = CINEMATIC_ACTOR_STATE_TURNING;
}

void CinematicActor::_locateActor(void)
{
    if (!_alwaysSpawn) {
        if (_actor) {
            return;
        }
        _actor = Actor::FindActorByName(_name);
    } else {
        _actor = NULL;
    }

    if (!_actor || _actor->checkActorDead()) {
        _spawn();
        _isAtSpot = true;
    }

    assert(_actor);

    if (!_actor) {
        gi.Error(ERR_DROP, "Unable to locate or spawn cinematic actor %s\n", _name.c_str());
    }
}

void CinematicActor::takeControlOfActor(const Vector& origin, float yaw)
{
    Event *ev = 0;

    _origin = origin + _originOffset;
    _yaw    = yaw + _yawOffset;

    _locateActor();
    _actor->TurnAIOff();
    _actor->RequestControl(this, ACTOR_CONTROL_LOCKED);

    ev = new Event(EV_Actor_RespondTo);
    ev->AddString("pain");
    ev->AddInteger(doesIgnorePain() ? 0 : 1);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_PermanentlyRespondTo);
    ev->AddString("pain");
    ev->AddInteger(doesIgnorePain() ? 0 : 1);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_RespondTo);
    ev->AddString("sight");
    ev->AddInteger(doesIgnoreSight() ? 0 : 1);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_PermanentlyRespondTo);
    ev->AddString("sight");
    ev->AddInteger(doesIgnoreSight() ? 0 : 1);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_RespondTo);
    ev->AddString("sound");
    ev->AddInteger(doesIgnoreSound() ? 0 : 1);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_PermanentlyRespondTo);
    ev->AddString("sound");
    ev->AddInteger(doesIgnoreSound() ? 0 : 1);
    _actor->ProcessEvent(ev);

    _actor->edict->svflags |= SVF_BROADCAST;
}

void CinematicActor::actorControlLostEvent(Event *ev)
{
    _hasActorControl = false;
}

// after-behavior if we held control" guard is therefore not retail and is gone;
void CinematicActor::releaseControlOfActor(void)
{
    assert(_actor);
    Event *ev = 0;

    _actor->ReleaseControl(this);

    _actor->edict->svflags &= ~SVF_BROADCAST;

    switch (_afterBehavior) {
    case CINEMATIC_ACTOR_AFTER_BEHAVIOR_REMOVE_FROM_GAME:
        _actor->ProcessEvent(EV_Remove);
        _actor = NULL;
        break;

    case CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_FREEZE:
        ev = new Event(EV_Actor_SetStickToGround);
        ev->AddInteger(1);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetSimplifiedThink);
        ev->AddInteger(0);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetUseGravity);
        ev->AddInteger(1);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetMovementMode);
        ev->AddString("normal");
        _actor->ProcessEvent(ev);


        if (_actor->edict->s.torso_anim & ANIM_BLEND) {
            _actor->animate->StopAnimatingAtEnd(torso);
        }

        if (_actor->edict->s.anim & ANIM_BLEND) {
            _actor->animate->StopAnimatingAtEnd(legs);
        }

        break;

    case CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_NO_AI:
        ev = new Event(EV_Actor_SetStickToGround);
        ev->AddInteger(1);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetSimplifiedThink);
        ev->AddInteger(0);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetUseGravity);
        ev->AddInteger(1);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetMovementMode);
        ev->AddString("normal");
        _actor->ProcessEvent(ev);
        break;

    case CINEMATIC_ACTOR_AFTER_BEHAVIOR_LEAVE_WITH_AI:
        ev = new Event(EV_Actor_SetStickToGround);
        ev->AddInteger(1);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetSimplifiedThink);
        ev->AddInteger(0);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetUseGravity);
        ev->AddInteger(1);
        _actor->ProcessEvent(ev);

        ev = new Event(EV_Actor_SetMovementMode);
        ev->AddString("normal");
        _actor->ProcessEvent(ev);

        _actor->TurnAIOn();
        break;

    case CINEMATIC_ACTOR_AFTER_BEHAVIOR_KILL:
        _actor->ProcessEvent(EV_Actor_Dead);
        _actor = NULL;
        break;

    default:
        break;
    }
}

void CinematicActor::getToPosition(void)
{
    if (!doesSnapToSpot() && !isAtSpot()) {
        Event *ev = new Event(EV_Actor_WalkTo);
        ev->AddVector(_origin);
        ev->AddString(_moveAnim);
        _actor->PostEvent(ev, 0.1f);
        _state = CINEMATIC_ACTOR_STATE_MOVING;
    } else if (!isAtSpot()) {
        _actor->setOrigin(_origin);
        _actor->NoLerpThisFrame();
        _turnActor(!doesSnapToSpot());
    } else {
        _turnActor(!doesSnapToSpot());
    }
}

// SDK CinematicArmature.cpp:804.  The tiki must resolve to a subclass of Actor;
void CinematicActor::_spawn(void)
{
    ClassDef *cls;
    Event    *ev;
    SpawnArgs args;

    if (!strstr(_tiki.c_str(), ".tik")) {
        _tiki += ".tik";
    }

    args.setArg("model", _tiki.c_str());
    cls = args.getClassDef();

    if (!cls || !checkInheritance(&Actor::ClassInfo, cls)) {
        gi.Error(ERR_DROP, "%s is not a valid Actor", _tiki.c_str());
        return;
    }

    _actor = (Actor *)cls->newInstance();

    ev = new Event(EV_Model);
    ev->AddString(_tiki.c_str());
    _actor->ProcessEvent(ev); 

    ev = new Event(EV_SetOrigin);
    ev->AddVector(_origin);
    _actor->ProcessEvent(ev); 

    ev = new Event(EV_SetAngles);
    ev->AddVector(Vector(0.0f, _yaw, 0.0f));
    _actor->ProcessEvent(ev); 

    ev = new Event(EV_Actor_SetUseGravity);
    ev->AddInteger(0);
    _actor->ProcessEvent(ev);

    _actor->SetTargetName(_name.c_str());
}

void CinematicActor::playAnimation(void)
{
    Event *ev;

    _actor->velocity = Vector::Identity();

    ev = new Event(EV_Actor_SetUseGravity);
    ev->AddInteger(0);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_SetSimplifiedThink);
    ev->AddInteger(1);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_SetStickToGround);
    ev->AddInteger(0);
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_SetMovementMode);
    ev->AddString("anim");
    _actor->ProcessEvent(ev);

    ev = new Event(EV_Actor_Anim);
    ev->AddString(_anim);
    _actor->ProcessEvent(ev);
}

void CinematicActor::debug(void)
{
    G_DebugBBox(_origin, Vector(-8, -8, -8), Vector(8, 8, 8), 0.8f, 0.6f, 0.3f, 1.0f);
}

void CinematicActor::reset(void)
{
    _state           = CINEMATIC_ACTOR_STATE_INACTIVE;
    _hasActorControl = false;
    _isAtSpot        = false;
    _isAnimDone      = false;
}

qboolean CinematicActor::parse(Script& cinematicFile)
{
    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!Q_stricmp(token, "name")) {
            setName(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "tiki")) {
            setTiki(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "anim")) {
            setAnim(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "origin")) {
            setOriginOffset(cinematicFile.GetVector(false));
        } else if (!Q_stricmp(token, "yaw")) {
            setYawOffset(cinematicFile.GetFloat(false));
        } else if (!Q_stricmp(token, "moveanim")) {
            setMoveAnim(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "snap")) {
            setSnapToSpot(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "nopain")) {
            setIgnorePain(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "nosight")) {
            setIgnoreSight(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "nosound")) {
            setIgnoreSound(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "removeAfter")) {
            setRemoveAfter(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "rootActor")) {
            setRootActor(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "afterBehavior")) {
            setAfterBehavior(cinematicFile.GetToken(false));
        } else if (!Q_stricmp(token, "alwaysSpawn")) {
            setAlwaysSpawn(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "}")) {
            break;
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}



extern Event EV_CinematicCamera_StopPlaying;

CLASS_DECLARATION(Listener, CinematicCamera, NULL) {
    {&EV_CinematicCamera_StopPlaying, &CinematicCamera::_handleStopPlayingEvent},
    {NULL,                            NULL                                     }
};

CinematicCamera::CinematicCamera()
    : _moveType(CAMERA_MOVE_TYPE_FOLLOW_ANIM)
    , _lookType(CAMERA_LOOK_TYPE_WATCH_ANIM)
    , _originOffset(0.0f, 0.0f, 0.0f)
    , _yawOffset(0.0f)
    , _playing(false)
    , _selfRemoving(true)
    , _name("")
    , _camFile("")
    , _moveActor("")
    , _lookActor("")
{
    _camera = NULL;
}

void CinematicCamera::Archive(Archiver& arc)
{

    ArchiveEnum(_moveType, CameraMoveType);
    ArchiveEnum(_lookType, CameraLookType);

    arc.ArchiveSafePointer(&_camera);

    arc.ArchiveVector(&_originOffset);
    arc.ArchiveFloat(&_yawOffset);
    arc.ArchiveBool(&_playing);
    arc.ArchiveBool(&_selfRemoving);
    arc.ArchiveString(&_name);
    arc.ArchiveString(&_camFile);
    arc.ArchiveString(&_moveActor);
    arc.ArchiveString(&_lookActor);
}

void CinematicCamera::setMoveType(const str& moveType)
{
    if (moveType == "STATIC") {
        _moveType = CAMERA_MOVE_TYPE_STATIC;
    } else if (moveType == "ANIM") {
        _moveType = CAMERA_MOVE_TYPE_FOLLOW_ANIM;
    } else if (moveType == "PLAYER") {
        _moveType = CAMERA_MOVE_TYPE_FOLLOW_PLAYER;
    } else if (moveType == "ACTOR") {
        _moveType = CAMERA_MOVE_TYPE_FOLLOW_ACTOR;
    }
}

void CinematicCamera::setLookType(const str& lookType)
{
    if (lookType == "ANIM") {
        _lookType = CAMERA_LOOK_TYPE_WATCH_ANIM;
    } else if (lookType == "PLAYER") {
        _lookType = CAMERA_LOOK_TYPE_WATCH_PLAYER;
    } else if (lookType == "ACTOR") {
        _lookType = CAMERA_LOOK_TYPE_WATCH_ACTOR;
    }
}

void CinematicCamera::start(void)
{
    _playing = qtrue;

    if (_camera) {
        _camera->PostEvent(EV_Camera_CameraThink, FRAMETIME);
    }
}

// hard.  (The `lerp` field is parsed and never read; all 834 shipped cuts set it
void CinematicCamera::cut(void)
{
    if (!_camera) {
        return;
    }

    _camera->Cut(NULL);
    SetCamera(_camera, 0.0f); 
}


// The sum goes to the camera and NOT back into the members: retail passes
void CinematicCamera::takeControlOfCamera(const Vector& origin, float yaw)
{
    _locateCamera();

    if (_camera) {
        _camera->SetPlaybackOffsets(_yawOffset + yaw, _originOffset + origin);  
    }
}

void CinematicCamera::releaseControlOfCamera(void)
{
    if (!_camera) {
        return;
    }

    CancelPendingEvents();

    if (isSelfRemoving()) {
        _camera->PostEvent(EV_Remove, 0.0f);
    }

    _camera  = NULL;
    _playing = qfalse;
}

void CinematicCamera::_locateCamera(void)
{
    if (_camera) {
        return;
    }

    _spawn();
}

void CinematicCamera::_spawn(void)
{
    _camera = new Camera;

    str cameraName(_name);
    cameraName += "Camera";

    _camera->SetTargetName(cameraName.c_str());
    _camera->ProcessPendingEvents();

    if (_moveType == CAMERA_MOVE_TYPE_FOLLOW_ANIM) {
        Event *event = new Event(EV_Camera_LoadKFC);
        event->AddString(_camFile);
        _camera->ProcessEvent(event);

        float pathLengthInSeconds = _camera->GetPathLengthInSeconds();  

        event = new Event(EV_CinematicCamera_StopPlaying);
        PostEvent(event, pathLengthInSeconds);
    }
}

void CinematicCamera::_handleStopPlayingEvent(Event *ev)
{
    _playing = qfalse;
}

void CinematicCamera::reset(void)
{
    _playing = qfalse;
}

qboolean CinematicCamera::parse(Script& cinematicFile)
{
    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!Q_stricmp(token, "name")) {
            setName(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "camfile")) {
            setCamFile(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "movetype")) {
            setMoveType(str(cinematicFile.GetString(false)));
        } else if (!Q_stricmp(token, "looktype")) {
            setLookType(str(cinematicFile.GetString(false)));
        } else if (!Q_stricmp(token, "moveactor")) {
            setMoveActor(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "lookactor")) {
            setLookActor(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "origin")) {
            setOriginOffset(cinematicFile.GetVector(false));
        } else if (!Q_stricmp(token, "yaw")) {
            setYawOffset(cinematicFile.GetFloat(false));
        } else if (!Q_stricmp(token, "selfRemoving")) {
            setSelfRemovingFlag(cinematicFile.GetBoolean(false));
        } else if (!Q_stricmp(token, "}")) {
            break;
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}


extern Event EV_CinematicCut_FadeOut;

Event EV_CinematicCut_Cut (    "cinematiccut",
    EV_CODEONLY,
    "",
    "",
    "Makes a cinematic camera cut.");

CLASS_DECLARATION(Listener, CinematicCut, NULL) {
    {&EV_CinematicCut_FadeOut, &CinematicCut::_handleFadeOutEvent},
    {&EV_CinematicCut_Cut,     &CinematicCut::_handleCutEvent    },
    {NULL,                     NULL                              }
};

CinematicCut::CinematicCut()
    : _cinematicCamera(NULL)
    , _lerpFlag(false)
    , _frame(0)
    , _fadeOut(0)
    , _fadeIn(0)
    , _cameraName("")
{}

void CinematicCut::postEvents(void)
{
    Event *ev = 0;

    if (_fadeOut) {
        ev               = new Event(EV_CinematicCut_FadeOut);
        int fadeOutFrame = _frame - _fadeOut;
        PostEvent(ev, fadeOutFrame * CIN_FRAMETIME);
    }

    ev = new Event(EV_CinematicCut_Cut);
    PostEvent(ev, _frame * CIN_FRAMETIME);
}

void CinematicCut::Archive(Archiver& arc)
{

    arc.ArchiveObjectPointer((Class **)&_cinematicCamera);
    arc.ArchiveBool(&_lerpFlag);
    arc.ArchiveInteger(&_frame);
    arc.ArchiveInteger(&_fadeOut);
    arc.ArchiveInteger(&_fadeIn);
    arc.ArchiveString(&_cameraName);
}

void CinematicCut::stop(void)
{
    CancelPendingEvents();
}


qboolean CinematicCut::parse(Script& cinematicFile)
{
    const char *token = cinematicFile.GetToken(true);
    qboolean    lerp  = qfalse;
    int         value = 0;

    while (token) {
        if (!Q_stricmp(token, "toCamera")) {
            setCameraName(cinematicFile.GetString(false));
        } else if (!Q_stricmp(token, "fadeOut")) {
            value = cinematicFile.GetInteger(false);
            setFadeOut(value);
        } else if (!Q_stricmp(token, "fadeIn")) {
            value = cinematicFile.GetInteger(false);
            setFadeIn(value);
        } else if (!Q_stricmp(token, "frame")) {
            value = cinematicFile.GetInteger(false);
            setFrame(value);
        } else if (!Q_stricmp(token, "lerp")) {
            lerp = cinematicFile.GetBoolean(false);
            setLerpFlag(lerp);
        } else if (!Q_stricmp(token, "}")) {
            break;
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}

void CinematicCut::_handleFadeOutEvent(Event *ev)
{
    level.m_fade_color      = Vector(0.0f, 0.0f, 0.0f);
    level.m_fade_time_start = _fadeOut * CIN_FRAMETIME;
    level.m_fade_time       = _fadeOut * CIN_FRAMETIME;
    level.m_fade_alpha      = 1.0f;
    level.m_fade_type       = fadeout;
    level.m_fade_style      = alphablend;
}

// name does not resolve returns the view to the player.  47 shipped cuts carry a
void CinematicCut::_handleCutEvent(Event *ev)
{
    if (_cinematicCamera) {
        _cinematicCamera->cut();
    } else {
        SetCamera(NULL, 0.0f);
    }

    if (_fadeIn) {
        level.m_fade_color      = Vector(0.0f, 0.0f, 0.0f);
        level.m_fade_time_start = _fadeIn * CIN_FRAMETIME;
        level.m_fade_time       = _fadeIn * CIN_FRAMETIME;
        level.m_fade_alpha      = 1.0f;
        level.m_fade_type       = fadein;
        level.m_fade_style      = alphablend;
    }
}


Event EV_Cinematic_Begin (    "beginCinematic",
    EV_DEFAULT,
    "SB",
    "originName callStartThreadFlag",
    "Begins the cinematic at the specified named origin. If the"
    "origin name isn't specified, begins at 0,0,0 with 0 degrees of yaw rotation.");
Event EV_Cinematic_BeginAt (    "beginCinematicAt",
    EV_DEFAULT,
    "VFB",
    "originVector yawRotation callStartThreadFlag",
    "Begins the cinematic at the specified origin with the specified rotation."
    "If they are not specified, begins at 0,0,0 with 0 degrees of yaw rotation.");
Event EV_Cinematic_End (    "endCinematic",
    EV_DEFAULT,
    "B",
    "callEndThreadFlag",
    "Stop the cinematic.");
Event EV_Cinematic_SetBeginThread (    "setbeginthread",
    EV_DEFAULT,
    "s",
    "threadName",
    "Sets the thread to call when the cinematic begins.");
Event EV_Cinematic_SetEndThread (    "setendthread",
    EV_DEFAULT,
    "s",
    "threadName",
    "Sets the thread to call when the cinematic ends.");

CLASS_DECLARATION(Entity, Cinematic, "cinematic") {
    {&EV_Cinematic_Begin,           &Cinematic::handleBeginEvent          },
    {&EV_Cinematic_BeginAt,         &Cinematic::handleBeginAtEvent        },
    {&EV_Cinematic_End,             &Cinematic::handleEndEvent            },
    {&EV_Cinematic_SetBeginThread,  &Cinematic::handleSetBeginThreadEvent },
    {&EV_Cinematic_SetEndThread,    &Cinematic::handleSetEndThreadEvent   },
    {NULL,                          NULL                                  }
};

// form, so retail never trips over it.  This port reaches Cinematic only through
Cinematic::Cinematic()
    : _filename("")
    , _startThread("")
    , _stopThread("")
    , _looping(false)
    , _playing(false)
    , _callStartThreadFlag(true)
    , _resetCamera(true)
    , _stage(CINEMATIC_STAGE_UNLOADED)
{
    init();
}

Cinematic::~Cinematic()
{
    int i;

    for (i = _actorList.NumObjects(); i >= 1; i--) {
        CinematicActor *actor = _actorList.ObjectAt(i);
        if (actor) {
            actor->CancelPendingEvents();
            delete actor;
        }
    }
    _actorList.ClearObjectList();

    for (i = _cameraList.NumObjects(); i >= 1; i--) {
        CinematicCamera *camera = _cameraList.ObjectAt(i);
        if (camera) {
            camera->CancelPendingEvents();
            delete camera;
        }
    }
    _cameraList.ClearObjectList();

    for (i = _cutList.NumObjects(); i >= 1; i--) {
        CinematicCut *cut = _cutList.ObjectAt(i);
        if (cut) {
            cut->CancelPendingEvents();
            delete cut;
        }
    }
    _cutList.ClearObjectList();

    for (i = _originList.NumObjects(); i >= 1; i--) {
        CinematicOrigin *origin_shadow1 = _originList.ObjectAt(i);
        if (origin_shadow1) {
            origin_shadow1->CancelPendingEvents();
            delete origin_shadow1;
        }
    }
    _originList.ClearObjectList();
}

bool Cinematic::load(void)
{
    Script      cinematicFile;
    const char *token;

    if (!gi.FS_Exists(_filename.c_str())) {
        gi.Printf("File %s not found.\n", _filename.c_str());
        return false;
    }

    cinematicFile.LoadFile(_filename.c_str());
    while (cinematicFile.TokenAvailable(true)) {
        token = cinematicFile.GetToken(true);
        if (stricmp(token, "Actors") == 0) {
            if (!parseActors(cinematicFile)) {
                return false;
            }
        } else if (stricmp(token, "Cameras") == 0) {
            if (!parseCameras(cinematicFile)) {
                return false;
            }
        } else if (stricmp(token, "Objects") == 0) {
            if (!parseObjects(cinematicFile)) {
                return false;
            }
        } else if (stricmp(token, "Origins") == 0) {
            if (!parseOrigins(cinematicFile)) {
                return false;
            }
        } else {
            gi.Printf("Unexpected token %s in cinematic file %s.\n", token, _filename.c_str());
            return false;
        }
    }

    _stage = CINEMATIC_STAGE_READY;
    return true;
}

bool Cinematic::parseCloseBlock(Script& cinematicFile, const str& blockName, const str& closeToken)
{
    const char *token = cinematicFile.GetToken(true);

    if (strcmp(token, closeToken.c_str()) != 0) {
        gi.Error(ERR_DROP, "Expected %s to close %s block, found %s.  Aborting parsing.", closeToken.c_str(), blockName.c_str(), token);
        return false;
    }

    return false;
}

void Cinematic::Archive(Archiver& arc)
{
    Entity::Archive(arc);

    int numActors = _actorList.NumObjects();
    arc.ArchiveInteger(&numActors);

    if (arc.Loading()) {
        _actorList.Resize(numActors);
        for (int actorIdx = 1; actorIdx <= numActors; ++actorIdx) {
            CinematicActor *actor = NULL;
            _actorList.AddObject(actor);
            CinematicActor **actorPtr = &_actorList.ObjectAt(actorIdx);
            *actorPtr                 = (CinematicActor *)arc.ReadObject();
        }
    } else {
        for (int actorIdx = 1; actorIdx <= numActors; ++actorIdx) {
            CinematicActor *actor = _actorList.ObjectAt(actorIdx);
            arc.ArchiveObject(actor);
        }
    }

    int numCameras = _cameraList.NumObjects();
    arc.ArchiveInteger(&numCameras);

    if (arc.Loading()) {
        _cameraList.Resize(numCameras);
        for (int cameraIdx = 1; cameraIdx <= numCameras; ++cameraIdx) {
            CinematicCamera *camera = NULL;
            _cameraList.AddObject(camera);
            CinematicCamera **cameraPtr = &_cameraList.ObjectAt(cameraIdx);
            *cameraPtr                  = (CinematicCamera *)arc.ReadObject();
        }
    } else {
        for (int cameraIdx = 1; cameraIdx <= numCameras; ++cameraIdx) {
            CinematicCamera *camera = _cameraList.ObjectAt(cameraIdx);
            arc.ArchiveObject(camera);
        }
    }

    int numCuts = _cutList.NumObjects();
    arc.ArchiveInteger(&numCuts);

    if (arc.Loading()) {
        _cutList.Resize(numCuts);
        for (int cutIdx = 1; cutIdx <= numCuts; ++cutIdx) {
            CinematicCut *cut = NULL;
            _cutList.AddObject(cut);
            CinematicCut **cutPtr = &_cutList.ObjectAt(cutIdx);
            *cutPtr               = (CinematicCut *)arc.ReadObject();
        }
    } else {
        for (int cutIdx = 1; cutIdx <= numCuts; ++cutIdx) {
            CinematicCut *cut = _cutList.ObjectAt(cutIdx);
            arc.ArchiveObject(cut);
        }
    }

    int numOrigins = _originList.NumObjects();
    arc.ArchiveInteger(&numOrigins);

    if (arc.Loading()) {
        // Retail resizes the CAMERA list here, not the origin list (SDK
        _cameraList.Resize(numCameras);
        for (int originIdx = 1; originIdx <= numOrigins; ++originIdx) {
            CinematicOrigin *cinematicOrigin = NULL;
            _originList.AddObject(cinematicOrigin);
            CinematicOrigin **originPtr = &_originList.ObjectAt(originIdx);
            *originPtr                  = (CinematicOrigin *)arc.ReadObject();
        }
    } else {
        for (int originIdx = 1; originIdx <= numOrigins; ++originIdx) {
            CinematicOrigin *cinematicOrigin = _originList.ObjectAt(originIdx);
            arc.ArchiveObject(cinematicOrigin);
        }
    }

    arc.ArchiveString(&_filename);
    arc.ArchiveString(&_startThread);
    arc.ArchiveString(&_stopThread);
    arc.ArchiveBool(&_playing);
    arc.ArchiveBool(&_looping);
    arc.ArchiveBool(&_callStartThreadFlag);
    arc.ArchiveBool(&_resetCamera);

    ArchiveEnum(_stage, CinematicStage);
}

Cinematic::Cinematic(const str& filename) :
    _filename(filename),
    _looping(false),
    _playing(false),
    _callStartThreadFlag(true),
    _resetCamera(true),
    _stage(CINEMATIC_STAGE_UNLOADED)
{
    init();
}

void Cinematic::init(void)
{
    flags |= FL_THINK;

    _stage = CINEMATIC_STAGE_UNLOADED;

    setSolidType(SOLID_NOT);
    setMoveType(MOVETYPE_NONE);

    if (sv_showcameras->integer) {
        setModel("cinematic.tik");
        showModel();
    } else {
        hideModel();
    }
}

void Cinematic::reset(void)
{
    int i;

    for (i = 1; i <= _actorList.NumObjects(); i++) {
        _actorList.ObjectAt(i)->reset();
    }

    for (i = 1; i <= _cameraList.NumObjects(); i++) {
        _cameraList.ObjectAt(i)->reset();
    }

    _stage   = CINEMATIC_STAGE_READY;
    _playing = qfalse;
}

void Cinematic::Think(void)
{
    switch (_stage) {
    case CINEMATIC_STAGE_WAITING_FOR_ACTORS:
        if (areActorsAtTheirPlaces()) {
            start();
        }
        break;
    case CINEMATIC_STAGE_ANIMATING:
        if (checkForCompletion()) {
            stop();
        }
        break;
    default:
        break;
    }

    if (sv_showcameras->integer) {
        debug();
    }
}

CinematicActor *Cinematic::getCinematicActorByName(const str& name)
{
    for (int i = 1; i <= _actorList.NumObjects(); i++) {
        CinematicActor *cinematicActor = _actorList.ObjectAt(i);
        if (cinematicActor->getName() == name) {
            return cinematicActor;
        }
    }

    return NULL;
}

CinematicCamera *Cinematic::getCinematicCameraByName(const str& cameraName)
{
    for (int i = 1; i <= _cameraList.NumObjects(); i++) {
        CinematicCamera *cinematicCamera = _cameraList.ObjectAt(i);
        if (cinematicCamera->getName() == cameraName) {
            return cinematicCamera;
        }
    }

    return NULL;
}

CinematicOrigin *Cinematic::getCinematicOriginByName(const str& originName)
{
    for (int i = 1; i <= _originList.NumObjects(); i++) {
        CinematicOrigin *cinematicOrigin = _originList.ObjectAt(i);
        if (str::icmp(cinematicOrigin->getName().c_str(), originName.c_str()) == 0) {
            return cinematicOrigin;
        }
    }

    return NULL;
}

void Cinematic::_startAnimation(void)
{
    int i;

    for (i = 1; i <= _actorList.NumObjects(); i++) {
        _actorList.ObjectAt(i)->playAnimation();
    }

    for (i = 1; i <= _cameraList.NumObjects(); i++) {
        _cameraList.ObjectAt(i)->start();
    }

    for (i = 1; i <= _cutList.NumObjects(); i++) {
        _cutList.ObjectAt(i)->postEvents();
    }
}

void Cinematic::_endAnimation(void)
{
    int i;

    for (i = 1; i <= _actorList.NumObjects(); i++) {
        _actorList.ObjectAt(i)->releaseControlOfActor();
    }

    for (i = 1; i <= _cameraList.NumObjects(); i++) {
        _cameraList.ObjectAt(i)->releaseControlOfCamera();
    }

    int numCinematicCuts = _cutList.NumObjects();
    for (i = 1; i <= numCinematicCuts; i++) {
        _cutList.ObjectAt(i)->stop();
    }

    if (doesResetCamera() && numCinematicCuts) {
        SetCamera(NULL, 0.0f);
    }
}

qboolean Cinematic::checkForCompletion(void)
{
    int i;

    for (i = 1; i <= _actorList.NumObjects(); i++) {
        if (!_actorList.ObjectAt(i)->isAnimDone()) {
            return qfalse;
        }
    }

    for (i = 1; i <= _cameraList.NumObjects(); i++) {
        if (!_cameraList.ObjectAt(i)->isAnimDone()) {
            return qfalse;
        }
    }

    return qtrue;
}

qboolean Cinematic::areActorsAtTheirPlaces(void)
{
    for (int i = 1; i <= _actorList.NumObjects(); i++) {
        if (!_actorList.ObjectAt(i)->isAtSpot()) {
            return qfalse;
        }
    }

    return qtrue;
}

// at the WORLD origin with zero yaw -- retail does not fall back to the first
void Cinematic::startAtNamedOrigin(const str& originName, qboolean callStartThread)
{
    CinematicOrigin *cinematicOrigin = getCinematicOriginByName(originName);
    Vector           originVec(0.0f, 0.0f, 0.0f);
    float            yaw = 0.0f;

    if (cinematicOrigin) {
        originVec = cinematicOrigin->getOrigin();
        yaw       = cinematicOrigin->getYaw();
    }

    startAtOrigin(originVec, yaw, callStartThread);
}

void Cinematic::startAtOrigin(const Vector& originVec, float yaw, qboolean callStartThread)
{
    int i;

    reset();

    for (i = 1; i <= _actorList.NumObjects(); i++) {
        CinematicActor *cinematicActor = _actorList.ObjectAt(i);
        cinematicActor->takeControlOfActor(originVec, yaw);
        cinematicActor->getToPosition();
    }

    for (i = 1; i <= _cameraList.NumObjects(); i++) {
        _cameraList.ObjectAt(i)->takeControlOfCamera(originVec, yaw);
    }

    for (i = 1; i <= _cutList.NumObjects(); i++) {
        CinematicCut    *cut    = _cutList.ObjectAt(i);
        CinematicCamera *camera = getCinematicCameraByName(cut->getCameraName());

        cut->setCinematicCamera(camera);
    }

    _stage               = CINEMATIC_STAGE_WAITING_FOR_ACTORS;
    _playing             = qtrue;
    _callStartThreadFlag = callStartThread;
}


void Cinematic::start(qboolean callStartThread)
{
    if (_startThread.length() && callStartThread) {
        ExecuteThread(_startThread);
    }

    _stage = CINEMATIC_STAGE_ANIMATING;

    _startAnimation();
}

// NOTE what is NOT here, and was: G_StartCinematic() / G_StopCinematic().  In
void Cinematic::stop(qboolean callEndThread)
{
    if (!_playing) {
        return;
    }

    _playing = qfalse;

    _endAnimation();

    if (_stopThread.length() && callEndThread) {
        ExecuteThread(_stopThread);
    }

    _stage = CINEMATIC_STAGE_FINISHED;
}

void Cinematic::handleBeginEvent(Event *ev)
{
    qboolean callStartThread = qtrue;
    str      originName;

    if (ev->NumArgs() == 1) {
        originName = ev->GetString(1);
    }
    if (ev->NumArgs() == 2) {
        callStartThread = ev->GetBoolean(2);
    }

    startAtNamedOrigin(originName, callStartThread);
}

void Cinematic::handleBeginAtEvent(Event *ev)
{
    Vector   originVec(0.0f, 0.0f, 0.0f);
    float    yawRotation     = 0.0f;
    qboolean callStartThread = qtrue;

    if (ev->NumArgs() >= 1) {
        originVec = ev->GetVector(1);
    }
    if (ev->NumArgs() == 2) {
        yawRotation = ev->GetFloat(2);
    }
    if (ev->NumArgs() == 3) {
        callStartThread = ev->GetBoolean(3);
    }

    startAtOrigin(originVec, yawRotation, callStartThread);
}

void Cinematic::handleEndEvent(Event *ev)
{
    qboolean callEndThread = qtrue;

    if (ev->NumArgs() == 1) {
        callEndThread = ev->GetBoolean(1);
    }

    stop(callEndThread);
}

void Cinematic::handleSetBeginThreadEvent(Event *ev)
{
    assert(ev);

    if (ev->NumArgs() != 1) {
        ev->Error("Usage: setStartThread <threadname>");
        return;
    }

    setStartThread(ev->GetString(1));
}

void Cinematic::handleSetEndThreadEvent(Event *ev)
{
    assert(ev);

    if (ev->NumArgs() != 1) {
        ev->Error("Usage: setStopThread <threadname>");
        return;
    }

    setStopThread(ev->GetString(1));
}



void Cinematic::debug(void)
{
    int i;

    for (i = 1; i <= _actorList.NumObjects(); i++) {
        _actorList.ObjectAt(i)->debug();
    }

    for (i = 1; i <= _cameraList.NumObjects(); i++) {
        _cameraList.ObjectAt(i)->debug();
    }

    for (i = 1; i <= _cutList.NumObjects(); i++) {
        _cutList.ObjectAt(i)->debug();
    }

    for (i = 1; i <= _originList.NumObjects(); i++) {
        _originList.ObjectAt(i)->debug();
    }
}


qboolean Cinematic::parseActors(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Actors")) {
        return qfalse;
    }

    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!Q_stricmp(token, "}")) {
            break;
        }
        if (!Q_stricmp(token, "Actor")) {
            if (!parseActor(cinematicFile)) {
                return qfalse;
            }
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}

qboolean Cinematic::parseActor(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Actor")) {
        return qfalse;
    }

    CinematicActor *actor = new CinematicActor;
    if (!actor->parse(cinematicFile)) {
        delete actor;
        return qfalse;
    }

    _actorList.AddObject(actor);
    return qtrue;
}

qboolean Cinematic::parseCameras(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Cameras")) {
        return qfalse;
    }

    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!strcmp(token, "}")) {
            break;
        }

        if (!Q_stricmp(token, "Camera")) {
            if (!parseCamera(cinematicFile)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token, "Cut")) {
            if (!parseCut(cinematicFile)) {
                return qfalse;
            }
        } else if (!Q_stricmp(token, "ResetCamera")) {
            setResetCameraFlag(cinematicFile.GetBoolean(false));
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}

qboolean Cinematic::parseCamera(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Camera")) {
        return qfalse;
    }

    CinematicCamera *cinematicCamera = new CinematicCamera();
    if (!cinematicCamera->parse(cinematicFile)) {
        delete cinematicCamera;
        return qfalse;
    }

    _cameraList.AddObject(cinematicCamera);
    return qtrue;
}

qboolean Cinematic::parseCut(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Cut")) {
        return qfalse;
    }

    CinematicCut *cinematicCut = new CinematicCut();
    if (!cinematicCut->parse(cinematicFile)) {
        delete cinematicCut;
        return qfalse;
    }

    _cutList.AddObject(cinematicCut);
    return qtrue;
}

qboolean Cinematic::parseObjects(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Objects")) {
        return qfalse;
    }

    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!strcmp(token, "}")) {
            break;
        }
        if (!Q_stricmp(token, "Object")) {
            if (!parseObject(cinematicFile)) {
                return qfalse;
            }
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}

qboolean Cinematic::parseObject(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Object")) {
        return qfalse;
    }

    return qtrue;
}

qboolean Cinematic::parseOrigins(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Origins")) {
        return qfalse;
    }

    const char *token = cinematicFile.GetToken(true);

    while (token) {
        if (!Q_stricmp(token, "}")) {
            break;
        }
        if (!Q_stricmp(token, "Origin")) {
            if (!parseOrigin(cinematicFile)) {
                return qfalse;
            }
        }

        if (!cinematicFile.TokenAvailable(true)) {
            break;
        }
        token = cinematicFile.GetToken(true);
    }

    return qtrue;
}

qboolean Cinematic::parseOrigin(Script& cinematicFile)
{
    if (!parseOpenBlock(cinematicFile, "Origin")) {
        return qfalse;
    }

    CinematicOrigin *cinematicOrigin = new CinematicOrigin();
    if (!cinematicOrigin->parse(cinematicFile)) {
        delete cinematicOrigin;
        return qfalse;
    }

    _originList.AddObject(cinematicOrigin);
    return qtrue;
}

qboolean Cinematic::parseOpenBlock(Script& cinematicFile, const str& blockName, const str& openToken)
{
    const char *token = cinematicFile.GetToken(true);

    if (strcmp(token, openToken.c_str()) != 0) {
        gi.Error(ERR_DROP, "Expected %s to open %s block, found %s.  Aborting parsing.",
                 openToken.c_str(),
                 blockName.c_str(),
                 token);
        return qfalse;
    }

    return qtrue;
}
