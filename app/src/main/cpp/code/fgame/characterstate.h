

#pragma once

#include "g_local.h"
#include "entity.h"
#include "../corepp/script.h"

enum testcondition_t {
    TC_ISTRUE,   
    TC_ISFALSE,  
    TC_EDGETRUE, 
    TC_EDGEFALSE 
};

// ArchiveEnum(movecontrol, movecontrol_t) -- so they must be retail's exactly.
enum movecontrol_t {
    MOVECONTROL_USER,       
    MOVECONTROL_LEGS,       
    MOVECONTROL_ANIM,       
    MOVECONTROL_ABSOLUTE,   
    MOVECONTROL_HANGING,    
    MOVECONTROL_WALLHUG,
    MOVECONTROL_MONKEYBARS,
    MOVECONTROL_PIPECRAWL,
    MOVECONTROL_PIPEHANG,
    MOVECONTROL_STEPUP,
    MOVECONTROL_ROPE_GRAB,
    MOVECONTROL_ROPE_RELEASE,
    MOVECONTROL_ROPE_MOVE,
    MOVECONTROL_PICKUPENEMY,
    MOVECONTROL_PUSH,
    MOVECONTROL_CLIMBWALL,
    MOVECONTROL_USEANIM,
    MOVECONTROL_CROUCH,
    MOVECONTROL_LOOPUSEANIM,
    MOVECONTROL_USEOBJECT,
    MOVECONTROL_COOLOBJECT,
    MOVECONTROL_FAKEPLAYER,
};

enum cameratype_t {
    CAMERA_TOPDOWN,
    CAMERA_BEHIND,
    CAMERA_FRONT,
    CAMERA_SIDE,
    CAMERA_BEHIND_FIXED,
    CAMERA_SIDE_LEFT,
    CAMERA_SIDE_RIGHT,
    CAMERA_BEHIND_NOPITCH
};

#define DEFAULT_MOVETYPE MOVECONTROL_LEGS  
#define DEFAULT_CAMERA   CAMERA_BEHIND

class Conditional;

template<class Type>
struct Condition {
    const char *name;
    qboolean (Type::*func)(Conditional& condition);
};

class Conditional : public Class
{
private:
    qboolean result;
    qboolean previous_result;
    bool     checked;

public:
    CLASS_PROTOTYPE(Conditional);

    Condition<Class> condition;
    Container<str>   parmList;

    bool        getResult(testcondition_t test, Entity& ent);
    const char *getName(void);

    Conditional(Condition<Class>& condition);
    Conditional();

    const char *getParm(int number);
    void        addParm(str parm);
    int         numParms(void);
    void        clearCheck(void);
};

inline void Conditional::addParm(str parmValue)
{
    parmList.AddObject(parmValue);
}

inline const char *Conditional::getParm(int number)
{
    if ((number < 1) || (number > parmList.NumObjects())) {
        gi.Error(ERR_DROP, "Parm #%d out of range on %s condition\n", number, condition.name);
    }
    return parmList.ObjectAt(number).c_str();
}

inline int Conditional::numParms(void)
{
    return parmList.NumObjects();
}

inline void Conditional::clearCheck(void)
{
    checked = false;
}


inline const char *Conditional::getName(void)
{
    return condition.name;
}

inline bool Conditional::getResult(testcondition_t test, Entity& ent)
{
    if (condition.func && !checked) {
        checked         = true;
        previous_result = result;

        result = (ent.*condition.func)(*this);
    }

    switch (test) {
    case TC_ISFALSE:
        return !result;

    case TC_EDGETRUE:
        return result && !previous_result;

    case TC_EDGEFALSE:
        return !result && previous_result;

    case TC_ISTRUE:
    default:
        return result != false;
    }
}

class State;
class StateMap;
class FuzzyVar; 

class Expression : public Class
{
private:
    struct condition_t {
        testcondition_t test;
        int             condition_index;
    };

    str                    value;
    Container<condition_t> conditions;

    float points;

public:
    Expression();
    Expression(const Expression& exp);
    Expression(Script& script, State& state);
    Expression(Script& script, FuzzyVar& fuzzyVar); 

    void operator=(const Expression& exp);

    const char *getValue(void);

    float getPoints(void);






public:
    bool getResult( const State &state, Entity &ent, const Container<Conditional *> *sent_conditionals ); 
    bool getResult( const FuzzyVar &fVar, Entity &ent, const Container<Conditional *> *sent_conditionals );  
};

inline void Expression::operator=(const Expression& exp)
{
    int i;

    value  = exp.value;
    points = exp.points;

    conditions.FreeObjectList();
    for (i = 1; i <= exp.conditions.NumObjects(); i++) {
        conditions.AddObject(exp.conditions.ObjectAt(i));
    }
}

inline const char *Expression::getValue(void)
{
    return value.c_str();
}

inline float Expression::getPoints(void)
{
    return points;
}

class State : public Class
{
private:
    Container<int> condition_indexes;

    StateMap& statemap;

    str name;

    str           nextState;
    movecontrol_t movetype;
    cameratype_t  cameratype;

    str            behaviorName;
    Container<str> behaviorParmList;

    str            headBehaviorName;
    Container<str> headBehaviorParmList;
    str            eyeBehaviorName;
    Container<str> eyeBehaviorParmList;
    str            torsoBehaviorName;
    Container<str> torsoBehaviorParmList;

    float minTime;
    float maxTime;

    Container<Expression> legAnims;
    Container<Expression> torsoAnims;  

    Container<Expression> states;
    Container<str>        entryCommands;
    Container<str>        exitCommands;

    qboolean ignoreGlobalStates;

    void readNextState(Script& script);
    void readMoveType(Script& script);
    void readCamera(Script& script);
    void readLegs(Script& script);
    void readTorso(Script& script);  
    void readBehavior(Script& script);
    void readTorsoBehavior(Script& script); 
    void readHeadBehavior(Script& script);  
    void readEyeBehavior(Script& script);   
    void readTime(Script& script);
    void readStates(Script& script);
    void readCommands(Script& script, Container<str>& container);

    void ParseAndProcessCommand(str command, Entity *target);

public:
    State(const char *name, Script& script, StateMap& map);

    State *Evaluate(Entity& ent, Container<Conditional *> *ent_conditionals);
    int    addCondition(const char *name, Script& script);
    void   CheckStates(void);

    const char *getName(void);

    const char   *getLegAnim(Entity& ent, Container<Conditional *> *sent_conditionals);
    const char   *getTorsoAnim(Entity& ent, Container<Conditional *> *sent_conditionals);
    const char   *getBehaviorName(void);
    const char   *getTorsoBehaviorName(void); 
    const char   *getHeadBehaviorName(void);  
    const char   *getEyeBehaviorName(void);   
    State        *getNextState(void);
    movecontrol_t getMoveType(void);
    cameratype_t  getCameraType(void);
    qboolean      setCameraType(str ctype);

    const char *getBehaviorParm(int number = 1);
    void        addBehaviorParm(str parm);
    int         numBehaviorParms(void);

    const char *getTorsoBehaviorParm(int number = 1);
    void        addTorsoBehaviorParm(str parm);
    int         numTorsoBehaviorParms(void);
    const char *getHeadBehaviorParm(int number = 1);
    void        addHeadBehaviorParm(str parm);
    int         numHeadBehaviorParms(void);
    const char *getEyeBehaviorParm(int number = 1);
    void        addEyeBehaviorParm(str parm);
    int         numEyeBehaviorParms(void);

    qboolean  IgnoreGlobalStates(void);  

    float getMinTime(void);
    float getMaxTime(void);
    void  ProcessEntryCommands(Entity *target);
    void  ProcessExitCommands(Entity *target);
    void  GetLegAnims(Container<const char *> *c);




public:
    void GetTorsoAnims( Container<const char *> *c ); 
};

inline void State::addBehaviorParm(str parmValue)
{
    behaviorParmList.AddObject(parmValue);
}

inline const char *State::getBehaviorParm(int number)
{
    return behaviorParmList.ObjectAt(number).c_str();
}

inline int State::numBehaviorParms(void)
{
    return behaviorParmList.NumObjects();
}

inline void State::addTorsoBehaviorParm(str parmValue)
{
    torsoBehaviorParmList.AddObject(parmValue);
}

inline const char *State::getTorsoBehaviorParm(int number)
{
    return torsoBehaviorParmList.ObjectAt(number).c_str();
}

inline int State::numTorsoBehaviorParms(void)
{
    return torsoBehaviorParmList.NumObjects();
}

inline void State::addHeadBehaviorParm(str parmValue)
{
    headBehaviorParmList.AddObject(parmValue);
}

inline const char *State::getHeadBehaviorParm(int number)
{
    return headBehaviorParmList.ObjectAt(number).c_str();
}

inline int State::numHeadBehaviorParms(void)
{
    return headBehaviorParmList.NumObjects();
}

inline void State::addEyeBehaviorParm(str parmValue)
{
    eyeBehaviorParmList.AddObject(parmValue);
}

inline const char *State::getEyeBehaviorParm(int number)
{
    return eyeBehaviorParmList.ObjectAt(number).c_str();
}

inline int State::numEyeBehaviorParms(void)
{
    return eyeBehaviorParmList.NumObjects();
}

class Script; 

class StateMap : public Class
{
private:
    Container<State *>        stateList;
    Container<State *>        globalStateList; 
    Condition<Class>         *current_conditions;
    Container<Conditional *> *current_conditionals;
    str                       filename;

public:
    StateMap(const char *filename, Condition<Class> *conditions, Container<Conditional *> *conditionals);
    ~StateMap();

    void              ReadStates(Script *script); 
    Condition<Class> *getCondition(const char *name);
    int               findConditional(Conditional *condition);
    int               addConditional(Conditional *condition);
    Conditional      *getConditional(const char *name);
    void              GetAllAnims(Container<const char *> *c);
    State            *FindState(const char *name);
    State            *FindGlobalState(const char *name); 
    const char       *Filename();
};

inline const char *StateMap::Filename(void)
{
    return filename.c_str();
}

inline const char *State::getName(void)
{
    return name.c_str();
}

inline State *State::getNextState(void)
{
    return statemap.FindState(nextState.c_str());
}

inline movecontrol_t State::getMoveType(void)
{
    return movetype;
}

inline cameratype_t State::getCameraType(void)
{
    return cameratype;
}

void      ClearCachedStatemaps(void);
StateMap *GetStatemap(
    str                       filename,
    Condition<Class>         *conditions,
    Container<Conditional *> *conditionals,
    qboolean                  reload,
    qboolean                  cache_only = false
);
void CacheStatemap(str filename, Condition<Class> *conditions);
