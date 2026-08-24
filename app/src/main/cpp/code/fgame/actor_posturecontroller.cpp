
#include "actor.h"
#include "actor_posturecontroller.h"
#include "characterstate.h"

extern Event EV_Actor_SetPostureStateMap;

PostureController::PostureController()
{
    gi.Error(ERR_FATAL, "PostureController::PostureController -- Default Constructor Called");
    act = NULL;
    init();
}

PostureController::PostureController(Actor *actor)
{
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_DROP, "PostureController::PostureController -- actor is NULL");
        act = NULL;
    }

    init();
}

PostureController::~PostureController()
{
    act->freeConditionals(_postureConditionals);
}

void PostureController::init()
{
    _postureStateMap      = NULL;
    _postureStateMap_Name = "";

    _currentPostureState      = NULL;
    _currentPostureState_Name = "";

    _requestedPostureState      = NULL;
    _requestedPostureState_Name = "";

    _requestor = NULL;
}

void PostureController::evaluate()
{
    int    count;
    State *laststate = NULL;
    str    currentanim;
    str    stateLegAnim;

    if (!act) {
        return;
    }

    stateLegAnim = act->animname;
    count        = 0;

    if (!_postureStateMap) {
        return;
    }

    if (act->deadflag || !_currentPostureState) {
        return;
    }

    do {
        count++;
        if (count > 10) {
            gi.WDPrintf("Possible infinite loop in posture state '%s'\n", _currentPostureState->getName());
            if (count > 50) {
                gi.Error(ERR_DROP, "Stopping due to possible infinite state loop\n");
                break;
            }
        }

        laststate            = _currentPostureState;
        _currentPostureState = _currentPostureState->Evaluate(*act, &_postureConditionals);

        if (!_currentPostureState) {
            return;
        }

        _currentPostureState_Name = _currentPostureState->getName();

        if (laststate != _currentPostureState) {
            act->SetActorFlag(ACTOR_FLAG_POSTURE_ANIM_DONE, qfalse);
        }

        if (_currentPostureState == _requestedPostureState) {
            if (_requestor) {
                _requestor->ProcessEvent(EV_PostureChanged_Completed);
            }
        }

        currentanim = _currentPostureState->getLegAnim(*act, &_postureConditionals);

        if (currentanim.length() && (Q_stricmp(stateLegAnim.c_str(), currentanim.c_str()) != 0)) {
            act->SetAnim( currentanim, EV_Posture_Anim_Done, legs );  
            stateLegAnim = currentanim;
        }
    } while (laststate != _currentPostureState);
}

bool PostureController::requestPosture(const str& postureState, Listener *requestor)
{
    if (!_postureStateMap) {
        return false;
    }

    _requestedPostureState = _postureStateMap->FindState(postureState.c_str());

    if (_requestedPostureState) {
        _requestor                  = requestor;
        _requestedPostureState_Name = postureState;
        return true;
    }

    return false;
}

void PostureController::setPostureStateMap(const str& stateMap, bool loading)
{
    _postureStateMap_Name = stateMap;

    act->freeConditionals(_postureConditionals);

    _postureStateMap =
        GetStatemap(_postureStateMap_Name, (Condition<Class> *)Actor::Conditions, &_postureConditionals, false);

    _currentPostureState_Name = "START";

    if (!loading) {
        setPostureState(_currentPostureState_Name.c_str());
    }
}

void PostureController::setPostureState(const str& postureState)
{
    if (!_postureStateMap) {
        return;
    }

    if (act && act->deadflag) {
        return;
    }

    _currentPostureState = _postureStateMap->FindState(postureState.c_str());
}

void PostureController::setPostureState(const str& postureState, const str& requestedState)
{
    if (!_postureStateMap) {
        return;
    }

    if (act && act->deadflag) {
        return;
    }

    _currentPostureState        = _postureStateMap->FindState(postureState.c_str());
    _requestedPostureState      = _postureStateMap->FindState(requestedState.c_str());
    _requestedPostureState_Name = requestedState.c_str();
}

void PostureController::DoArchive(Archiver& arc, Actor *actor)
{
    Archive(arc);
    if (actor) {
        act = actor;
    } else {
        gi.Error(ERR_FATAL, "PostureController::DoArchive -- actor is NULL");
    }
}

void PostureController::Archive(Archiver& arc)
{
    arc.ArchiveString(&_postureStateMap_Name);
    arc.ArchiveString(&_currentPostureState_Name);
    arc.ArchiveString(&_requestedPostureState_Name);
    arc.ArchiveSafePointer(&_requestor);

    if (!arc.Saving()) {
        if (_postureStateMap_Name.length() && act) {
            Event *event;

            event = new Event(EV_Actor_SetPostureStateMap);
            event->AddString(_postureStateMap_Name);
            event->AddInteger(1);
            act->ProcessEvent(event);

            if (_postureStateMap) {
                _currentPostureState   = _postureStateMap->FindState(_currentPostureState_Name.c_str());
                _requestedPostureState = _postureStateMap->FindState(_requestedPostureState_Name.c_str());
            }
        }
    }
}
