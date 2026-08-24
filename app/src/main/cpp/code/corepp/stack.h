

#if defined( GAME_DLL )
#include "../fgame/stack.h"
#else
#ifndef __STACK_H__
#define __STACK_H__

#if defined(GAME_DLL)
#    include "g_local.h"

#    define STACK_Error          gi.Error
#    define STACK_DPrintf        gi.DPrintf
#    define STACK_WDPrintf(text) gi.DPrintf(text)

#elif defined(CGAME_DLL)
#    include "cg_local.h"

#    define STACK_Error          cgi.Error
#    define STACK_DPrintf        cgi.DPrintf
#    define STACK_WDPrintf(text) cgi.DPrintf(text)

#else

#    define STACK_Error          Com_Error
#    define STACK_DPrintf        Com_DPrintf
#    define STACK_WDPrintf(text) Com_DPrintf(text)
#endif

template<class Type>
class StackNode
{
public:
    Type       data;
    StackNode *next;

    StackNode(Type d);
};

template<class Type>
inline StackNode<Type>::StackNode(Type d)
    : data(d)
{
    next = NULL;
}

template<class Type>
class Stack
{
private:
    StackNode<Type> *head;

public:
    Stack();
    ~Stack<Type>();
    void     Clear(void);
    qboolean Empty(void);
    void     Push(Type data);
    Type     Pop(void);
    Type     Head(void);
};

template<class Type>
inline Stack<Type>::Stack()
{
    head = NULL;
}

template<class Type>
inline Stack<Type>::~Stack()
{
    Clear();
}

template<class Type>
inline void Stack<Type>::Clear(void)
{
    while (!Empty()) {
        Pop();
    }
}

template<class Type>
inline qboolean Stack<Type>::Empty(void)
{
    if (head == NULL) {
        return true;
    }
    return false;
}

template<class Type>
inline void Stack<Type>::Push(Type data)
{
    StackNode<Type> *tmp;

    tmp = new StackNode<Type>(data);
    if (!tmp) {
        assert(NULL);
        STACK_Error(ERR_DROP, "Stack::Push : Out of memory");
    }

    tmp->next = head;
    head      = tmp;
}

template<class Type>
inline Type Stack<Type>::Pop(void)
{
    Type             ret;
    StackNode<Type> *node;

    if (!head) {
        return NULL;
    }

    node = head;
    ret  = node->data;
    head = node->next;

    delete node;

    return ret;
}

template<class Type>
inline Type Stack<Type>::Head(void)
{
    if (!head) {
        return NULL;
    }

    return head->data;
}

#endif 

#endif 