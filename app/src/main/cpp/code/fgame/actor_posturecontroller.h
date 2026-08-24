
#ifndef __ACTOR_POSTURECONTROLLER_H__
#define __ACTOR_POSTURECONTROLLER_H__

#include "g_local.h"
#include "characterstate.h"
#include "actorincludes.h"


class Actor;
class Archiver;

extern Event EV_Posture_Anim_Done;
extern Event EV_PostureChanged_Completed;

// a virtual destructor retail does not have.  Same treatment as CombatSubsystem.
class PostureController final
{
public:
    PostureController();
    PostureController(Actor *actor);
    ~PostureController();

    virtual void Archive(Archiver& arc);
    void         DoArchive(Archiver& arc, Actor *actor);

    void evaluate();

    bool requestPosture(const str& postureState, Listener *requestor);

    const str& getRequestedPostureName() { return _requestedPostureState_Name; }
    const str& getCurrentPostureName() { return _currentPostureState_Name; }

    void setPostureStateMap(const str& stateMap, bool loading);
    void setPostureState(const str& postureState);
    void setPostureState(const str& postureState, const str& requestedState);

protected:
    void init();

private:
    StateMap *_postureStateMap;
    str       _postureStateMap_Name;

    State *_currentPostureState;
    str    _currentPostureState_Name;

    State *_requestedPostureState;
    str    _requestedPostureState_Name;

    SafePtr<Listener>        _requestor;
    Container<Conditional *> _postureConditionals;

    Actor *act;
};

#endif 
