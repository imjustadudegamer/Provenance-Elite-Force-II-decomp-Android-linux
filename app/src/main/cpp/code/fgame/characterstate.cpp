/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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


#include "characterstate.h"
#include "fuzzyengine.h" 
#include "animate.h"
#include "scriptmaster.h"

#define PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_probe.h"
#include <time.h>

#define GRS_NOW() ( PROBE_On() ? ( long long )clock() * 1000000LL / CLOCKS_PER_SEC : 0 )

static const char *MoveControl_Names[] = {
    "user",         
    "legs",         
    "anim",         
    "absolute",     
    "hanging",      
    "wallhug",      
    "monkeybars",   
    "pipecrawl",    
    "pipehang",     
    "stepup",       
    "rope_grab",    
    "rope_release", 
    "rope_move",    
    "pickupenemy",  
    "push",         
    "climbwall",    
    "useanim",      
    "crouch",       
    "loopuseanim",  
    "useobject",    
    "coolobject",   
    "fakeplayer",   
    NULL
};

static const char *Camera_Names[] = {
    "topdown",        
    "behind",         
    "front",          
    "side",           
    "behind_fixed",   
    "side_left",      
    "side_right",     
    "behind_nopitch", 
    NULL
};

Conditional::Conditional(Condition<Class>& cond)
    : condition(cond)
{
    result          = false;
    previous_result = false;
    checked         = false;
}

Conditional::Conditional()
{
    result          = false;
    previous_result = false;
    checked         = false;
}

Expression::Expression()
{
    points = 0.0f;
}

Expression::Expression(const Expression& exp)
{
    int i;

    value  = exp.value;
    points = exp.points;

    for (i = 1; i <= exp.conditions.NumObjects(); i++) {
        conditions.AddObject(exp.conditions.ObjectAt(i));
    }
}

Expression::Expression(Script& script, State& state)
{
    str         token;
    condition_t condition;
    int         start;

    value = script.GetToken(true);

    if (!script.TokenAvailable(false) || Q_stricmp(script.GetToken(false), ":")) {
        gi.Error(ERR_DROP, "%s: Expecting ':' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(false)) {
        token = script.GetToken(true);

        switch (token[0]) {
        case '!':
            condition.test = TC_ISFALSE;
            start          = 1;
            break;

        case '+':
            condition.test = TC_EDGETRUE;
            start          = 1;
            break;

        case '-':
            condition.test = TC_EDGEFALSE;
            start          = 1;
            break;

        default:
            condition.test = TC_ISTRUE;
            start          = 0;
        }

        if (token.length() <= start) {
            gi.Error(
                ERR_DROP, "%s: Illegal syntax '%s' on line %d.\n", script.Filename(), token.c_str(), script.GetLineNumber()
            );
            condition.condition_index = 0;
            continue;
        }

        condition.condition_index = state.addCondition(&token[start], script);
        if (!condition.condition_index) {
            gi.Error(ERR_DROP, "%s: Unknown condition '%s' on line %d.\n", script.Filename(), &token[start], script.GetLineNumber());
        }

        conditions.AddObject(condition);
    }
}

bool Expression::getResult(const State& state, Entity& ent, const Container<Conditional *> *sent_conditionals)  
{
    int          i;
    condition_t *cond;
    Conditional *conditional;

    for (i = 1; i <= conditions.NumObjects(); i++) {
        cond = &conditions.ObjectAt(i);

        conditional = sent_conditionals->ObjectAt(cond->condition_index);

        if (!conditional || !conditional->getResult(cond->test, ent)) {
            return false;
        }
    }

    return true;
}

Expression::Expression(Script& script, FuzzyVar& fuzzyVar)
{
    str         token;
    condition_t condition;
    int         start;

    value  = script.GetToken(true);
    points = (float)atof(value.c_str());

    if (!script.TokenAvailable(false) || Q_stricmp(script.GetToken(false), ":")) {
        gi.Error(ERR_DROP, "%s: Expecting ':' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(false)) {
        token = script.GetToken(true);

        switch (token[0]) {
        case '!':
            condition.test = TC_ISFALSE;
            start          = 1;
            break;

        case '+':
            condition.test = TC_EDGETRUE;
            start          = 1;
            break;

        case '-':
            condition.test = TC_EDGEFALSE;
            start          = 1;
            break;

        default:
            condition.test = TC_ISTRUE;
            start          = 0;
        }

        if (token.length() <= start) {
            gi.Error(
                ERR_DROP, "%s: Illegal syntax '%s' on line %d.\n", script.Filename(), token.c_str(), script.GetLineNumber()
            );
            condition.condition_index = 0;
            continue;
        }

        condition.condition_index = fuzzyVar.addCondition(&token[start], script);
        if (!condition.condition_index) {
            gi.Error(ERR_DROP, "%s: Unknown condition '%s' on line %d.\n", script.Filename(), &token[start], script.GetLineNumber());
        }

        conditions.AddObject(condition);
    }
}

bool Expression::getResult(const FuzzyVar& fVar, Entity& ent, const Container<Conditional *> *sent_conditionals)  
{
    int          i;
    condition_t *cond;
    Conditional *conditional;

    for (i = 1; i <= conditions.NumObjects(); i++) {
        cond = &conditions.ObjectAt(i);

        conditional = sent_conditionals->ObjectAt(cond->condition_index);

        if (!conditional || !conditional->getResult(cond->test, ent)) {
            return false;
        }
    }

    return true;
}

void State::readNextState(Script& script)
{
    nextState = script.GetToken(false);
}

void State::readMoveType(Script& script)
{
    str          token;
    const char **pName;
    int          i;

    token = script.GetToken(false);

    for (i = 0, pName = MoveControl_Names; *pName != NULL; pName++, i++) {
        if (!token.icmp(*pName)) {
            break;
        }
    }

    if (*pName == NULL) {
        gi.Error(
            ERR_DROP,
            "%s: Unknown movetype '%s' on line %d.\n",
            script.Filename(),
            token.c_str(),
            script.GetLineNumber()
        );
    } else {
        movetype = (movecontrol_t)i;
    }
}

qboolean State::setCameraType(str ctype)
{
    const char **pName;
    int          i;

    for (i = 0, pName = Camera_Names; *pName != NULL; pName++, i++) {
        if (!ctype.icmp(*pName)) {
            cameratype = (cameratype_t)i;
            return qtrue;
        }
    }
    return qfalse;
}

void State::readTorso(Script& script)
{
    str token;

    if (!script.TokenAvailable(true) || Q_stricmp(script.GetToken(true), "{")) {
        gi.Error(ERR_DROP, "%s: Expecting '{' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(true)) {
        token = script.GetToken(true);
        if (!Q_stricmp(token.c_str(), "}")) {
            break;
        }

        script.UnGetToken();
        torsoAnims.AddObject(Expression(script, *this));
    }
}

void State::readCamera(Script& script)
{
    str token;

    token = script.GetToken(false);

    if (!setCameraType(token)) {
        gi.Error(
            ERR_DROP,
            "%s: Unknown camera type '%s' on line %d.\n",
            script.Filename(),
            token.c_str(),
            script.GetLineNumber()
        );
    }
}

void State::readLegs(Script& script)
{
    str token;

    if (!script.TokenAvailable(true) || Q_stricmp(script.GetToken(true), "{")) {
        gi.Error(ERR_DROP, "%s: Expecting '{' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(true)) {
        token = script.GetToken(true);
        if (!Q_stricmp(token.c_str(), "}")) {
            break;
        }

        script.UnGetToken();
        legAnims.AddObject(Expression(script, *this));
    }
}


void State::readBehavior(Script& script)
{
    str token;

    if (!script.TokenAvailable(true)) {
        gi.Error(ERR_DROP, "%s: Expecting behavior name on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    behaviorName = script.GetToken(true);
    if (!getClass(behaviorName)) {
        gi.Error(ERR_DROP, "%s: Unknown behavior '%s' on line %d.\n", script.Filename(), behaviorName.c_str(), script.GetLineNumber());
    }


    while (script.TokenAvailable(false) && script.AtOpenParen(false)) {
        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), "(")) {
            script.UnGetToken();
        }

        while (!script.AtCloseParen(false)) {
            token = script.GetToken(false);
            if (Q_stricmp(token.c_str(), ",")) {
                addBehaviorParm(token);
            }
        }

        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), ")")) {
            script.UnGetToken();
        }
    }

    while (script.TokenAvailable(false) && script.AtString(false)) {
        token = script.GetToken(false);
        addBehaviorParm(token);
    }
}

void State::readTorsoBehavior(Script& script)
{
    str token;

    if (!script.TokenAvailable(true)) {
        gi.Error(ERR_DROP, "%s: Expecting behavior name on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    torsoBehaviorName = script.GetToken(true);
    if (!getClass(torsoBehaviorName)) {
        gi.Error(ERR_DROP, "%s: Unknown behavior '%s' on line %d.\n", script.Filename(), torsoBehaviorName.c_str(), script.GetLineNumber());
    }

    while (script.TokenAvailable(false) && script.AtOpenParen(false)) {
        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), "(")) {
            script.UnGetToken();
        }

        while (!script.AtCloseParen(false)) {
            token = script.GetToken(false);
            if (Q_stricmp(token.c_str(), ",")) {
                addTorsoBehaviorParm(token);
            }
        }

        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), ")")) {
            script.UnGetToken();
        }
    }

    while (script.TokenAvailable(false) && script.AtString(false)) {
        token = script.GetToken(false);
        addTorsoBehaviorParm(token);
    }
}

void State::readHeadBehavior(Script& script)
{
    str token;

    if (!script.TokenAvailable(true)) {
        gi.Error(ERR_DROP, "%s: Expecting behavior name on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    headBehaviorName = script.GetToken(true);
    if (!getClass(headBehaviorName)) {
        gi.Error(ERR_DROP, "%s: Unknown behavior '%s' on line %d.\n", script.Filename(), headBehaviorName.c_str(), script.GetLineNumber());
    }

    while (script.TokenAvailable(false) && script.AtOpenParen(false)) {
        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), "(")) {
            script.UnGetToken();
        }

        while (!script.AtCloseParen(false)) {
            token = script.GetToken(false);
            if (Q_stricmp(token.c_str(), ",")) {
                addHeadBehaviorParm(token);
            }
        }

        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), ")")) {
            script.UnGetToken();
        }
    }

    while (script.TokenAvailable(false) && script.AtString(false)) {
        token = script.GetToken(false);
        addHeadBehaviorParm(token);
    }
}

void State::readEyeBehavior(Script& script)
{
    str token;

    if (!script.TokenAvailable(true)) {
        gi.Error(ERR_DROP, "%s: Expecting behavior name on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    eyeBehaviorName = script.GetToken(true);
    if (!getClass(eyeBehaviorName)) {
        gi.Error(ERR_DROP, "%s: Unknown behavior '%s' on line %d.\n", script.Filename(), eyeBehaviorName.c_str(), script.GetLineNumber());
    }

    while (script.TokenAvailable(false) && script.AtOpenParen(false)) {
        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), "(")) {
            script.UnGetToken();
        }

        while (!script.AtCloseParen(false)) {
            token = script.GetToken(false);
            if (Q_stricmp(token.c_str(), ",")) {
                addEyeBehaviorParm(token);
            }
        }

        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), ")")) {
            script.UnGetToken();
        }
    }

    while (script.TokenAvailable(false) && script.AtString(false)) {
        token = script.GetToken(false);
        addEyeBehaviorParm(token);
    }
}

//  * The retail body does not error on any shipped statemap.  A byte-exact Python port
void State::readTime(Script& script)
{
    str token;

    if (script.TokenAvailable(false) && script.AtString(false)) {
        token   = script.GetToken(false);
        minTime = (float)atof(token.c_str());
    }

    if (script.TokenAvailable(false) && script.AtString(false)) {
        token   = script.GetToken(false);
        maxTime = (float)atof(token.c_str());
    } else {
        maxTime = minTime;
    }

    if (script.TokenAvailable(false) && script.AtOpenParen(false)) {
        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), "(")) {
            script.UnGetToken();
        }

        token   = script.GetToken(false);
        minTime = (float)atof(token.c_str());

        maxTime = minTime;

        token = script.GetToken(false);

        if (!Q_stricmp(token.c_str(), ",")) {
            token   = script.GetToken(false);
            maxTime = (float)atof(token.c_str());

            token = script.GetToken(false);
        }

        if (Q_stricmp(token.c_str(), ")")) {
            gi.Error(ERR_DROP, "%s: Missing Closing Paren ) on line %d.\n", script.Filename(), script.GetLineNumber());
        }
    }
}

void State::readStates(Script& script)
{
    str token;

    if (!script.TokenAvailable(true) || Q_stricmp(script.GetToken(true), "{")) {
        gi.Error(ERR_DROP, "%s: Expecting '{' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(true)) {
        token = script.GetToken(true);
        if (!Q_stricmp(token.c_str(), "}")) {
            break;
        }

        script.UnGetToken();
        states.AddObject(Expression(script, *this));
    }
}

void State::ParseAndProcessCommand(str command, Entity *target)
{
    int         argc;
    const char *argv[MAX_COMMANDS];
    char        args[MAX_COMMANDS][MAXTOKEN];
    Script      script;
    Event      *event;

    script.Parse(command, command.length(), "CommandString");

    argc = 0;
    while (script.TokenAvailable(false)) {
        const char *tok;

        if (argc >= MAX_COMMANDS) {
            gi.WDPrintf("State:ParseAndProcessCommand : Line exceeds %d command limit", MAX_COMMANDS);
            script.SkipToEOL();
            break;
        }
        tok = script.GetToken(false);

        Q_strncpyz(args[argc], tok, sizeof(args[argc]));
        argv[argc] = args[argc];
        argc++;
    }

    assert(argc > 0);

    if (argc <= 0) {
        return;
    }

    event = new Event(args[0]);  
    event->AddTokens(argc - 1, &argv[1]);
    target->ProcessEvent(event);
}

void State::ProcessEntryCommands(Entity *target)
{
    int i, count;
    str command;

    assert(target);
    if (!target) {
        return;
    }

    count = entryCommands.NumObjects();
    for (i = 1; i <= count; i++) {
        ParseAndProcessCommand(entryCommands.ObjectAt(i), target);
    }
}

void State::ProcessExitCommands(Entity *target)
{
    int i, count;
    str command;

    assert(target);
    if (!target) {
        return;
    }

    count = exitCommands.NumObjects();
    for (i = 1; i <= count; i++) {
        ParseAndProcessCommand(exitCommands.ObjectAt(i), target);
    }
}

void State::readCommands(Script& script, Container<str>& container)
{
    str      token;
    str      command;
    qboolean done;

    if (!script.TokenAvailable(true) || Q_stricmp(script.GetToken(true), "{")) {
        gi.Error(ERR_DROP, "%s: Expecting '{' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(true)) {
        done = false;
        while (true) {
            if (!script.TokenAvailable(false)) {
                done = true;
                break;
            }
            token = script.GetToken(false);

            while (!Q_stricmp(token.c_str(), "(") || !Q_stricmp(token.c_str(), ")") || !Q_stricmp(token.c_str(), ",")) {
                if (!script.TokenAvailable(false)) {
                    done = true;
                    break;
                }
                token = script.GetToken(false);
            }
            if (done) {
                break;
            }
            if (!Q_stricmp(token.c_str(), "}")) {
                break;
            }

            if (token.length()) {
                if (strstr(token.c_str(), " ") != NULL) {
                    command.append("\"");
                    command.append(token);
                    command.append("\"");
                } else {
                    command.append(token);
                }
            } else {
                command.append("\"\"");
            }

            command.append(" ");
        }

        if (done) {
            container.AddObject(command);
            command = "";
            continue;
        }
        return;
    }
}

// So the Klingon rig arm does NOT freeze on first contact in retail -- it never enters
State *State::Evaluate(Entity& ent, Container<Conditional *> *sent_conditionals)
{
    int         i;
    Expression *exp;
    State      *state;
    int         index;

    for (i = 1; i <= condition_indexes.NumObjects(); i++) {
        index = condition_indexes.ObjectAt(i);
        sent_conditionals->ObjectAt(index)->clearCheck();
    }

    for (i = 1; i <= states.NumObjects(); i++) {
        exp = &states.ObjectAt(i);
        if (exp->getResult(*this, ent, sent_conditionals)) {
            state = statemap.FindState(exp->getValue());
            return state;
        }
    }

    return this;
}

const char *State::getLegAnim(Entity& ent, Container<Conditional *> *sent_conditionals)
{
    int         i;
    Expression *exp;
    int         index;

    for (i = 1; i <= condition_indexes.NumObjects(); i++) {
        index = condition_indexes.ObjectAt(i);
        sent_conditionals->ObjectAt(index)->clearCheck();
    }

    for (i = 1; i <= legAnims.NumObjects(); i++) {
        exp = &legAnims.ObjectAt(i);
        if (exp->getResult(*this, ent, sent_conditionals)) {
            return exp->getValue();
        }
    }

    return "";
}

const char *State::getTorsoAnim(Entity& ent, Container<Conditional *> *sent_conditionals)
{
    int         i;
    Expression *exp;
    int         index;

    for (i = 1; i <= condition_indexes.NumObjects(); i++) {
        index = condition_indexes.ObjectAt(i);
        sent_conditionals->ObjectAt(index)->clearCheck();
    }

    for (i = 1; i <= torsoAnims.NumObjects(); i++) {
        exp = &torsoAnims.ObjectAt(i);
        if (exp->getResult(*this, ent, sent_conditionals)) {
            return exp->getValue();
        }
    }

    return "";
}

const char *State::getBehaviorName(void)
{
    return behaviorName.c_str();
}

const char *State::getTorsoBehaviorName(void)
{
    return torsoBehaviorName.c_str();
}

const char *State::getHeadBehaviorName(void)
{
    return headBehaviorName.c_str();
}

const char *State::getEyeBehaviorName(void)
{
    return eyeBehaviorName.c_str();
}

float State::getMinTime(void)
{
    return minTime;
}

float State::getMaxTime(void)
{
    return maxTime;
}

int State::addCondition(const char *conditionName, Script& script)
{
    Conditional      *condition;
    Condition<Class> *cond;
    int               index;

    str token;

    condition = NULL;
    cond      = statemap.getCondition(conditionName);
    if (cond) {
        condition = new Conditional(*cond);
    }


    while (script.TokenAvailable(false) && script.AtString(false)) {
        token = script.GetToken(false);
        if (condition) {
            condition->addParm(token);
        }
    }

    while (script.TokenAvailable(false) && script.AtOpenParen(false)) {
        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), "(")) {
            script.UnGetToken();
        }

        while (script.TokenAvailable(false) && !script.AtCloseParen(false)) {
            token = script.GetToken(false);
            if (Q_stricmp(token.c_str(), ",")) {
                if (condition) {
                    condition->addParm(token);
                }
            }
        }

        token = script.GetToken(false);
        if (Q_stricmp(token.c_str(), ")")) {
            script.UnGetToken();
        }
    }

    if (!condition) {
        return 0;
    }

    index = statemap.findConditional(condition);

    if (index) {
        delete condition;
    } else {
        index = statemap.addConditional(condition);
    }

    condition_indexes.AddUniqueObject(index);

    return index;
}

void State::CheckStates(void)
{
    const char *value;
    int         i;

    if (!statemap.FindState(nextState.c_str())) {
        gi.Error(
            ERR_DROP, "Unknown next state '%s' referenced in state '%s'.\n", nextState.c_str(), getName());
    }

    // the diagnostic is correct; do not "fix" the shipped statemap.
    for (i = 1; i <= states.NumObjects(); i++) {
        value = states.ObjectAt(i).getValue();
        if (!statemap.FindState(value)) {
            gi.WDPrintf(
                "============\n"
                "ERROR in %s:\n"
                "  Possibly Unknown state '%s' referenced in state '%s'. -- Check your includes \n"
                "============",
                statemap.Filename(), value, getName());
        }
    }
}

void State::GetLegAnims(Container<const char *> *c)
{
    int      i, j;
    qboolean addobj = true;

    for (i = 1; i <= legAnims.NumObjects(); i++) {
        const char *value = legAnims.ObjectAt(i).getValue();
        addobj            = true;

        for (j = 1; j <= c->NumObjects(); j++) {
            if (!Q_stricmp(c->ObjectAt(j), value)) {
                addobj = false;
                break;
            }
        }
        if (addobj) {
            c->AddObject(value);
        }
    }
}


State::State(const char *statename, Script& script, StateMap& map)
    : statemap(map)
{
    str cmd;

    name         = statename;
    nextState    = statename;
    movetype     = DEFAULT_MOVETYPE;
    cameratype   = DEFAULT_CAMERA;
    behaviorName      = "";
    headBehaviorName  = "";
    eyeBehaviorName   = "";
    torsoBehaviorName = "";

    ignoreGlobalStates = qfalse;

    minTime = -1.0;
    maxTime = -1.0;

    if (!script.TokenAvailable(true) || Q_stricmp(script.GetToken(true), "{")) {
        gi.Error(ERR_DROP, "%s: Expecting '{' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(true)) {
        cmd = script.GetToken(true);
        if (!cmd.icmp("nextstate")) {
            readNextState(script);
        } else if (!cmd.icmp("movetype")) {
            readMoveType(script);
        } else if (!cmd.icmp("camera")) {
            readCamera(script);
        } else if (!cmd.icmp("legs")) {
            readLegs(script);
        } else if (!cmd.icmp("torso")) {
            readTorso(script);
        } else if (!cmd.icmp("behavior")) {
            readBehavior(script);
        } else if (!cmd.icmp("torsobehavior")) {
            readTorsoBehavior(script);
        } else if (!cmd.icmp("headbehavior")) {
            readHeadBehavior(script);
        } else if (!cmd.icmp("eyebehavior")) {
            readEyeBehavior(script);
        } else if (!cmd.icmp("ignoreglobalstate")) {
            ignoreGlobalStates = qtrue;
        } else if (!cmd.icmp("time")) {
            readTime(script);
        } else if (!cmd.icmp("states")) {
            readStates(script);
        } else if (!cmd.icmp("entrycommands")) {
            readCommands(script, entryCommands);
        } else if (!cmd.icmp("exitcommands")) {
            readCommands(script, exitCommands);
        } else if (!cmd.icmp("}")) {
            break;
        } else {
            gi.Error(
                ERR_DROP,
                "%s: Unknown command '%s' on line %d.\n",
                script.Filename(),
                cmd.c_str(),
                script.GetLineNumber()
            );
        }
    }
}

StateMap::StateMap(const char *file_name, Condition<Class> *conditions, Container<Conditional *> *conditionals)
{
    assert(file_name);

    filename = file_name;

    this->current_conditions = conditions;

    this->current_conditionals = conditionals;

    Script *script;
    script = new Script(file_name);
    if (script) {
        ReadStates(script);
        delete script;
        script = 0;
    }

    int i;
    for (i = 1; i <= stateList.NumObjects(); i++) {
        stateList.ObjectAt(i)->CheckStates();
    }
}

void StateMap::ReadStates(Script *script)
{
    str         cmd;
    str         statename;
    State      *state;
    const char *includedFile;

    while (script->TokenAvailable(true)) {
        cmd = script->GetToken(true);

        if (!cmd.icmp("state")) {
            statename = script->GetToken(false);
            state     = FindState(statename.c_str());
            if (state) {
                stateList.RemoveObject(state);
                delete state;
                state = NULL;
            }

            state = new State(statename.c_str(), *script, *this);
            stateList.AddObject(state);
        } else if (!cmd.icmp("globalstate")) {
            statename = script->GetToken(false);
            state     = FindGlobalState(statename.c_str());
            if (state) {
                globalStateList.RemoveObject(state);
                delete state;
                state = NULL;
            }

            state = new State(statename.c_str(), *script, *this);
            globalStateList.AddObject(state);
        } else if (!cmd.icmp("$include")) {
            includedFile      = script->GetToken(false);
            Script *newScript = new Script(includedFile);

            ReadStates(newScript);

            Container<macro *> *macrolist = newScript->GetMacroList();
            Container<macro *> *mainlist  = script->GetMacroList();
            for (int i = 1; i <= macrolist->NumObjects(); i++) {
                macro *theMacro          = macrolist->ObjectAt(i);
                macro *addMacro          = new macro;
                addMacro->macroName      = theMacro->macroName;
                addMacro->macroText      = theMacro->macroText;
                mainlist->AddObject(addMacro);
            }

            delete newScript;
            newScript = NULL;
        } else {
            gi.Error(
                ERR_DROP,
                "%s: Unknown command '%s' on line %d.\n",
                script->Filename(),
                cmd.c_str(),
                script->GetLineNumber()
            );
        }
    }
}

State *StateMap::FindGlobalState(const char *name)
{
    int i;

    for (i = 1; i <= globalStateList.NumObjects(); i++) {
        if (!strcmp(globalStateList.ObjectAt(i)->getName(), name)) {
            return globalStateList.ObjectAt(i);
        }
    }

    return NULL;
}

StateMap::~StateMap()
{
    int i, num;

    num = stateList.NumObjects();
    for (i = num; i > 0; i--) {
        delete stateList.ObjectAt(i);
    }
    stateList.FreeObjectList();

    num = globalStateList.NumObjects();
    for (i = num; i > 0; i--) {
        delete globalStateList.ObjectAt(i);
    }
    globalStateList.FreeObjectList();
}

Condition<Class> *StateMap::getCondition(const char *name)
{
    Condition<Class> *c;

    if (current_conditions) {
        for (c = current_conditions; c->name; c++) {
            if (!strcmp(c->name, name)) {
                return c;
            }
        }
    }

    return NULL;
}

int StateMap::findConditional(Conditional *condition)
{
    int          i;
    int          j;
    Conditional *c;
    bool         found;


    if (strcmp(condition->getName(), "CHANCE") == 0) {
        return 0;
    }

    for (i = 1; i <= current_conditionals->NumObjects(); i++) {
        c = current_conditionals->ObjectAt(i);
        if ((c->getName() == condition->getName()) && (c->numParms() == condition->numParms())) {
            found = true;
            for (j = 1; j <= c->numParms(); j++) {
                if (strcmp(c->getParm(j), condition->getParm(j))) {
                    found = false;
                    break;
                }
            }

            if (found) {
                return i;
            }
        }
    }

    return 0;
}

int StateMap::addConditional(Conditional *condition)
{
    int index;
    index = current_conditionals->AddObject(condition);

    return index;
}

Conditional *StateMap::getConditional(const char *name)
{
    int               i;
    Conditional      *c;
    Condition<Class> *condition;

    for (i = 1; i <= current_conditionals->NumObjects(); i++) {
        c = current_conditionals->ObjectAt(i);
        if (!strcmp(c->getName(), name)) {
            return c;
        }
    }

    condition = getCondition(name);

    c = new Conditional(*condition);
    current_conditionals->AddObject(c);

    return c;
}

State *StateMap::FindState(const char *name)
{
    int i;

    for (i = 1; i <= stateList.NumObjects(); i++) {
        if (!strcmp(stateList.ObjectAt(i)->getName(), name)) {
            return stateList.ObjectAt(i);
        }
    }

    return NULL;
}


struct cached_statemap_t {
    StateMap                 *statemap;
    Container<Conditional *> *conditionals;
};

Container<cached_statemap_t> cached_statemaps;

StateMap *GetStatemap(
    str                       filename,
    Condition<Class>         *conditions,
    Container<Conditional *> *conditionals,
    qboolean                  reload,
    qboolean                  cache_only
)
{
    int                i;
    int                j;
    cached_statemap_t *cache = NULL;
    cached_statemap_t  new_cache;
    qboolean           found = false;
    Conditional       *new_conditional;
    Conditional       *old_conditional;
    long long          t0 = GRS_NOW(), t1;

    for (i = 1; i <= cached_statemaps.NumObjects(); i++) {
        cache = &cached_statemaps.ObjectAt(i);

        if (strcmp(cache->statemap->Filename(), filename.c_str()) == 0) {
            found = true;
            break;
        }
    }

    if (found && reload) {
        delete cache->statemap;
        delete cache->conditionals;

        cache->conditionals = new Container<Conditional *>;
        cache->statemap     = new StateMap(filename, conditions, cache->conditionals);
    }

    if (!found) {
        new_cache.conditionals = new Container<Conditional *>;
        new_cache.statemap     = new StateMap(filename, conditions, new_cache.conditionals);

        cached_statemaps.AddObject(new_cache);

        cache = &new_cache;
    }


    if (!cache_only) {
        for (i = 1; i <= cache->conditionals->NumObjects(); i++) {
            old_conditional = cache->conditionals->ObjectAt(i);

            /* The cached node already holds the resolved condition. */
            new_conditional = new Conditional(old_conditional->condition);

            for (j = 1; j <= old_conditional->parmList.NumObjects(); j++) {
                new_conditional->parmList.AddObject(old_conditional->parmList.ObjectAt(j));
            }

            conditionals->AddObject(new_conditional);
        }
    }

    t1 = GRS_NOW();
    if (t0) {
        g_restoreSplitUs[GRS_STATEMAP] += t1 - t0;
        g_restoreSplitN[GRS_STATEMAP]++;
    }

    return cache->statemap;
}

void CacheStatemap(str filename, Condition<Class> *conditions)
{
    GetStatemap(filename, conditions, NULL, false, true);
}

void StateMap::GetAllAnims(Container<const char *> *c)
{
    int i;

    for (i = 1; i <= stateList.NumObjects(); i++) {
        stateList.ObjectAt(i)->GetLegAnims(c);
        stateList.ObjectAt(i)->GetTorsoAnims(c);
    }

    for (i = 1; i <= globalStateList.NumObjects(); i++) {
        globalStateList.ObjectAt(i)->GetLegAnims(c);
        globalStateList.ObjectAt(i)->GetTorsoAnims(c);
    }
}

void ClearCachedStatemaps(void)
{
    int                i, j, num2;
    cached_statemap_t *cache;

    num2 = cached_statemaps.NumObjects();

    for (i = num2; i > 0; i--) {
        cache = &cached_statemaps.ObjectAt(i);

        delete cache->statemap;

        int num = cache->conditionals->NumObjects();
        for (j = num; j > 0; j--) {
            Conditional *cond = cache->conditionals->ObjectAt(j);
            delete cond;
        }
        delete cache->conditionals;
    }

    cached_statemaps.FreeObjectList();
}



void State::GetTorsoAnims( Container<const char *> *c )
{
	int i,j;
	qboolean addobj = true;
	
	for ( i=1; i<=torsoAnims.NumObjects(); i++ )
	{
		const char *value = torsoAnims.ObjectAt( i ).getValue();
		addobj = true;
		
		for ( j=1; j<=c->NumObjects(); j++ )
		{
			if ( !Q_stricmp( c->ObjectAt( j ), value ) )
            {
				addobj = false;
				break;
            }
		}
		if ( addobj )
			c->AddObject( value );
	}
}


qboolean State::IgnoreGlobalStates()
{
	return ignoreGlobalStates;
}

