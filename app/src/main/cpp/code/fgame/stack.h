

#pragma once

#include "g_local.h"
#include "../corepp/class.h"

template<class Type>
class StackNode : public Class
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
class Stack : public Class
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
        gi.Error(ERR_DROP, "Stack::Push : Out of memory");
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
