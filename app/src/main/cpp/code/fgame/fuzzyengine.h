

#pragma once

#include "g_local.h"
#include "characterstate.h"
#include "../corepp/script.h"

class FuzzyEngine;

class FuzzyVar : public Class
{
public:
    FuzzyVar(const char *name, Script& script, FuzzyEngine& f_engine);

    float Evaluate(Entity& ent, Container<Conditional *> *ent_conditionals);
    int  addCondition(const char *name, Script& script);
    void readEvaluations(Script& script);

    const char *getName(void);

private:
    Container<int>        _condition_indexes; 
    Container<Expression> _evaluations;       

    FuzzyEngine& _fuzzyEngine;

    str              _name;
    Container<float> _points; 
};

class FuzzyEngine : public Class
{
public:
    FuzzyEngine(const char *filename, Condition<Class> *conditions, Container<Conditional *> *conditionals);

    Condition<Class> *getCondition(const char *name);
    int               findConditional(Conditional *condition);
    int               addConditional(Conditional *condition);
    FuzzyVar         *FindFuzzyVar(const char *name);
    const char       *Filename(void);
    void              ReadFuzzyVars(const char *filename);

private:
    Container<FuzzyVar *>     _varList;
    Condition<Class>         *_current_conditions;
    Container<Conditional *> *_current_conditionals;
    str                       _filename;
};

void         ClearCachedFuzzyEngines(void);
FuzzyEngine *GetFuzzyEngine(
    str                       filename,
    Condition<Class>         *conditions,
    Container<Conditional *> *conditionals,
    qboolean                  reload,
    qboolean                  cache_only = false
);
void CacheFuzzyEngine(str filename, Condition<Class> *conditions);
