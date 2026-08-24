
//      entered.  Level::Start must call program.Load( game_script ) (SDK level.cpp:358)

#pragma once

#include "../corepp/listener.h"
#include "../corepp/container.h"

#define MAX_PRSTACK_DEPTH 32
#define LOCALSTACK_SIZE   2048

class Program;
class dfunction_t;
struct statement_s;
typedef struct statement_s dstatement_t;

typedef struct {
    int          s;
    dfunction_t *f;
    int          stackbase;
} prstack_t;

class Interpreter : public Listener
{
private:
    prstack_t pr_stack[MAX_PRSTACK_DEPTH];
    int       pr_depth;

    byte localstack_type[LOCALSTACK_SIZE];
    int  localstack[LOCALSTACK_SIZE];
    int  localstack_used;
    int  localstack_base;

    bool         pr_trace;
    dfunction_t *pr_xfunction;
    int          pr_xstatement;

public:
    Program *program;
    int      instruction_pointer;
    bool     doneProcessing;
    bool     threadDying;

    Container<int> updateList;

    int threadNum;
    str threadName;

    CLASS_PROTOTYPE(Interpreter);

    Interpreter();
    virtual ~Interpreter() override;

    void   PrintStatement(const dstatement_t *s);
    void   StackTrace(void);
    inline void NullEntityError(const dstatement_t *statement);  
    void   Profile(void);
    void RunError(const char *error, ...) Q_PRINTF_FUNC(2, 3);
    void   ThreadCall(const Interpreter *source, dfunction_t *newf, int args);
    int    EnterFunction(dfunction_t *func);
    int    LeaveFunction(void);
    Event *EventForFunction(const dfunction_t *func, int args);
    void   Execute(Event *ev);
    void   Reset(void);

    const char *GlobalString(int ofs);
    const char *GlobalStringNoContents(int ofs);

    const char *Filename(void);

    void        SetThreadNum(int num);
    int         ThreadNum(void) const;
    const char *ThreadName(void) const;

    virtual void DoMove(void);

    qboolean labelExists(const char *label);
    qboolean Goto(const char *label);
    qboolean Setup(int num, const char *label);

    void Archive(Archiver& arc) override;
    void CleanupStack(int newstacktop, int oldstacktop);
};

inline void Interpreter::SetThreadNum(int num)
{
    threadNum = num;
}

inline int Interpreter::ThreadNum(void) const
{
    return threadNum;
}

inline const char *Interpreter::ThreadName(void) const
{
    const char *s = threadName.c_str();
    if ((uintptr_t)s < 4096) {
        return "";
    }
    return s;
}
