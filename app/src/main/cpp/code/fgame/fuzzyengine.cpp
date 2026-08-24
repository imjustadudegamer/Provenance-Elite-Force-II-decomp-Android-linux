
// never gi.Error the server: EF2 ships no .fzy files in retail data, so this

#include "fuzzyengine.h"
#include "actor.h"


struct cached_fuzzyengine_t {
    FuzzyEngine              *fengine;
    Container<Conditional *> *conditionals;
};

Container<cached_fuzzyengine_t> cached_fuzzyengines;

FuzzyEngine *GetFuzzyEngine(
    str                       filename,
    Condition<Class>         *conditions,
    Container<Conditional *> *conditionals,
    qboolean                  reload,
    qboolean                  cache_only
)
{
    int                  i;
    int                  j;
    cached_fuzzyengine_t *cache = NULL;
    cached_fuzzyengine_t  new_cache;
    qboolean             found = false;
    Conditional         *new_conditional;
    Conditional         *old_conditional;
    Condition<Class>    *cond;

    for (i = 1; i <= cached_fuzzyengines.NumObjects(); i++) {
        cache = &cached_fuzzyengines.ObjectAt(i);

        if (strcmp(cache->fengine->Filename(), filename.c_str()) == 0) {
            found = true;
            break;
        }
    }

    if (found && reload) {
        delete cache->fengine;
        delete cache->conditionals;

        cache->conditionals = new Container<Conditional *>;
        cache->fengine      = new FuzzyEngine(filename, conditions, cache->conditionals);
    }

    if (!found) {
        new_cache.conditionals = new Container<Conditional *>;
        new_cache.fengine      = new FuzzyEngine(filename, conditions, new_cache.conditionals);

        cached_fuzzyengines.AddObject(new_cache);

        cache = &new_cache;
    }


    if (!cache_only) {
        for (i = 1; i <= cache->conditionals->NumObjects(); i++) {
            old_conditional = cache->conditionals->ObjectAt(i);

            cond = cache->fengine->getCondition(old_conditional->condition.name);

            new_conditional = new Conditional(*cond);

            for (j = 1; j <= old_conditional->parmList.NumObjects(); j++) {
                new_conditional->parmList.AddObject(old_conditional->parmList.ObjectAt(j));
            }

            conditionals->AddObject(new_conditional);
        }
    }

    return cache->fengine;
}

void CacheFuzzyEngine(str filename, Condition<Class> *conditions)
{
    GetFuzzyEngine(filename, conditions, NULL, false, true);
}

void ClearCachedFuzzyEngines(void)
{
    int                   i, j, num2;
    cached_fuzzyengine_t *cache;

    num2 = cached_fuzzyengines.NumObjects();

    for (i = num2; i > 0; i--) {
        cache = &cached_fuzzyengines.ObjectAt(i);

        delete cache->fengine;

        int num = cache->conditionals->NumObjects();
        for (j = num; j > 0; j--) {
            Conditional *cond = cache->conditionals->ObjectAt(j);
            delete cond;
        }
        delete cache->conditionals;
    }

    cached_fuzzyengines.FreeObjectList();
}


FuzzyEngine::FuzzyEngine(const char *file_name, Condition<Class> *conditions, Container<Conditional *> *conditionals)
{
    assert(file_name);
    _filename = file_name;

    this->_current_conditions   = conditions;
    this->_current_conditionals = conditionals;

    ReadFuzzyVars(_filename.c_str());
}

void FuzzyEngine::ReadFuzzyVars(const char *filename)
{
    str      cmd;
    str      fuzzyVarName;
    Script   script;
    FuzzyVar *fuzzyVar;

    script.LoadFile(filename);

    while (script.TokenAvailable(true)) {
        cmd = script.GetToken(true);
        if (!cmd.icmp("fuzzyvar")) {
            fuzzyVarName = script.GetToken(false);
            fuzzyVar     = FindFuzzyVar(fuzzyVarName.c_str());
            if (fuzzyVar) {
                _varList.RemoveObject(fuzzyVar);
                gi.Error(
                    ERR_DROP,
                    "%s: Duplicate definition of FuzzyVar '%s' on line %d.\n",
                    filename,
                    fuzzyVarName.c_str(),
                    script.GetLineNumber()
                );
            }
            fuzzyVar = new FuzzyVar(fuzzyVarName.c_str(), script, *this);
            _varList.AddObject(fuzzyVar);
        } else {
            gi.Error(
                ERR_DROP, "%s: Unknown command '%s' on line %d.\n", script.Filename(), cmd.c_str(), script.GetLineNumber()
            );
        }
    }

}

FuzzyVar *FuzzyEngine::FindFuzzyVar(const char *name)
{
    int i;

    for (i = 1; i <= _varList.NumObjects(); i++) {
        if (!strcmp(_varList.ObjectAt(i)->getName(), name)) {
            return _varList.ObjectAt(i);
        }
    }

    return NULL;
}

int FuzzyEngine::addConditional(Conditional *condition)
{
    int index;
    index = _current_conditionals->AddObject(condition);

    return index;
}

int FuzzyEngine::findConditional(Conditional *condition)
{
    int          i;
    int          j;
    Conditional *c;
    bool         found;


    if (strcmp(condition->getName(), "CHANCE") == 0) {
        return 0;
    }

    for (i = 1; i <= _current_conditionals->NumObjects(); i++) {
        c = _current_conditionals->ObjectAt(i);
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


const char *FuzzyEngine::Filename(void)
{
    return _filename.c_str();
}

Condition<Class> *FuzzyEngine::getCondition(const char *name)
{
    Condition<Class> *c;

    if (_current_conditions) {
        for (c = _current_conditions; c->name; c++) {
            if (!strcmp(c->name, name)) {
                return c;
            }
        }
    }

    return NULL;
}


FuzzyVar::FuzzyVar(const char *varname, Script& script, FuzzyEngine& engine)
    : _fuzzyEngine(engine)
{
    str cmd;

    _name = varname;

    if (!script.TokenAvailable(true) || Q_stricmp(script.GetToken(true), "{")) {
        gi.Error(ERR_DROP, "%s: Expecting '{' on line %d.\n", script.Filename(), script.GetLineNumber());
    }

    while (script.TokenAvailable(true)) {
        cmd = script.GetToken(true);
        if (!cmd.icmp("evaluations")) {
            readEvaluations(script);
        } else if (!cmd.icmp("}")) {
            break;
        } else {
            gi.Error(
                ERR_DROP, "%s: Unknown command '%s' on line %d.\n", script.Filename(), cmd.c_str(), script.GetLineNumber()
            );
        }
    }
}

void FuzzyVar::readEvaluations(Script& script)
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
        _evaluations.AddObject(Expression(script, *this));
    }
}

float FuzzyVar::Evaluate(Entity& ent, Container<Conditional *> *sent_conditionals)
{
    int         i;
    Expression *exp;
    float       points;
    int         index;

    points = 0.0f;

    for (i = 1; i <= _condition_indexes.NumObjects(); i++) {
        index = _condition_indexes.ObjectAt(i);
        if (index >= 1 && index <= sent_conditionals->NumObjects()) {
            sent_conditionals->ObjectAt(index)->clearCheck();
        }
    }

    for (i = 1; i <= _evaluations.NumObjects(); i++) {
        exp = &_evaluations.ObjectAt(i);
        if (exp->getResult(*this, ent, sent_conditionals)) {
            points += exp->getPoints();
        }
    }

    return points;
}

int FuzzyVar::addCondition(const char *name, Script& script)
{
    Conditional      *condition;
    Condition<Class> *cond;
    int               index;

    str token;

    condition = NULL;
    cond      = _fuzzyEngine.getCondition(name);

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

    index = _fuzzyEngine.findConditional(condition);

    if (index) {
        delete condition;
    } else {
        index = _fuzzyEngine.addConditional(condition);
    }

    _condition_indexes.AddUniqueObject(index);

    return index;
}

const char *FuzzyVar::getName(void)
{
    return _name.c_str();
}
