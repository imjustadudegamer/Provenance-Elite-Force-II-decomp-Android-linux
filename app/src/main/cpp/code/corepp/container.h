/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

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

// copy constructor that does not copy every member, and retail NEVER

#pragma once

#if defined(GAME_DLL)
#    include "../fgame/g_local.h"

// GCC build fix (matches retail SDK container.h): gi must be declared before
extern game_import_t gi;

#    define CONTAINER_Error          gi.Error
#    define CONTAINER_DPrintf        gi.DPrintf
#    define CONTAINER_WDPrintf(text) gi.DPrintf(text)

#elif defined(CGAME_DLL)
#    include "../cgame/cg_local.h"

#    define CONTAINER_Error          cgi.Error
#    define CONTAINER_DPrintf        cgi.DPrintf
#    define CONTAINER_WDPrintf(text) cgi.DPrintf(text)

#elif defined(REF_DLL)

#    include "../renderercommon/tr_common.h"

#    define CONTAINER_Error          Com_Error
#    define CONTAINER_DPrintf        Com_DPrintf
#    define CONTAINER_WDPrintf(text) Com_DPrintf(text)

#else

#    include "../qcommon/qcommon.h"

#    define CONTAINER_Error          Com_Error
#    define CONTAINER_DPrintf        Com_DPrintf
#    define CONTAINER_WDPrintf(text) Com_DPrintf(text)
#endif

#include <stdlib.h>

class Archiver;

template<class Type>
class Container
{
private:
    Type *objlist;
    int   numobjects;
    int   maxobjects;

private:
    void Copy(const Container<Type>& container);

public:
    Container();

    Container(const Container<Type>& container);
    Container<Type>& operator=(const Container<Type>& container);

    Container(Container<Type>&& container);
    Container<Type>& operator=(Container<Type>&& container);

    ~Container();

    void Archive(Archiver& arc);
    void Archive(Archiver& arc, void (*ArchiveFunc)(Archiver& arc, Type *obj));

    int   AddObject(const Type& obj);
    int   AddObject();
    int   AddUniqueObject(const Type& obj);
    void  AddObjectAt(int index, const Type& obj);
    Type *AddressOfObjectAt(int index);
    void  ClearObjectList(void);
    void  FreeObjectList(void);
    int   IndexOfObject(const Type& obj);
    int   IndexOfObject(const Type& obj) const;
    void  InsertObjectAt(int index, const Type& obj);
    int   MaxObjects(void) const;
    int   NumObjects(void) const;
    Type& ObjectAt(const size_t index) const;
    bool  ObjectInList(const Type& obj);
    void  RemoveObjectAt(int index);
    void  RemoveObject(const Type& obj);
    void  RemoveObject(const Type* obj);
    void  Reset(void);
    void  Resize(int maxelements);
    void  SetObjectAt(int index, const Type& obj);
    void  Sort(int (*compare)(const void *elem1, const void *elem2));
    Type& operator[](const uintptr_t index) const;
};

template<class Type>
Container<Type>::Container()
{
    objlist = NULL;
    FreeObjectList();
}

template<class Type>
Container<Type>::Container(const Container<Type>& container)
{
    objlist = NULL;

    Copy(container);
}

template<class Type>
Container<Type>& Container<Type>::operator=(const Container<Type>& container)
{
    Copy(container);

    return *this;
}

template<class Type>
Container<Type>::Container(Container<Type>&& container)
{
    objlist              = container.objlist;
    numobjects           = container.numobjects;
    maxobjects           = container.maxobjects;
    container.objlist    = NULL;
    container.numobjects = 0;
    container.maxobjects = 0;
}

template<class Type>
Container<Type>& Container<Type>::operator=(Container<Type>&& container)
{
    FreeObjectList();

    objlist              = container.objlist;
    numobjects           = container.numobjects;
    maxobjects           = container.maxobjects;
    container.objlist    = NULL;
    container.numobjects = 0;
    container.maxobjects = 0;

    return *this;
}

template<class Type>
Container<Type>::~Container()
{
    FreeObjectList();
}

template<class Type>
void Container<Type>::FreeObjectList(void)
{
    if (objlist) {
        delete[] objlist;
    }

    objlist    = NULL;
    numobjects = 0;
    maxobjects = 0;
}

template<class Type>
void Container<Type>::ClearObjectList(void)
{
    if (objlist && numobjects) {
        delete[] objlist;

        if (maxobjects <= 0) {
            objlist = NULL;
            return;
        }

        objlist    = new Type[maxobjects];
        numobjects = 0;
    }
}

template<class Type>
int Container<Type>::NumObjects(void) const
{
    return numobjects;
}

template<class Type>
void Container<Type>::Resize(int maxelements)
{
    Type *temp;
    int   i;

    if (maxelements <= 0) {
        FreeObjectList();
        return;
    }

    if (!objlist) {
        maxobjects = maxelements;
        objlist    = new Type[maxobjects];
    } else {
        temp = objlist;

        maxobjects = maxelements;

        if (maxobjects < numobjects) {
            maxobjects = numobjects;
        }

        objlist = new Type[maxobjects];

        for (i = 0; i < numobjects; i++) {
            objlist[i] = temp[i];
        }

        delete[] temp;
    }
}

template<class Type>
void Container<Type>::SetObjectAt(int index, const Type& obj)
{
    if (!objlist) {
        return;
    }

    if ((index <= 0) || (index > numobjects)) {
        CONTAINER_Error(ERR_DROP, "Container::SetObjectAt : index out of range");
    }

    objlist[index - 1] = obj;
}

template<class Type>
int Container<Type>::AddObject(const Type& obj)
{
    if (!objlist) {
        Resize(10);
    }

    if (numobjects == maxobjects) {
        Resize(maxobjects * 2);
    }

    objlist[numobjects] = obj;
    numobjects++;

    return numobjects;
}

template<class Type>
int Container<Type>::AddObject()
{
    if (!objlist) {
        Resize(10);
    }

    if (numobjects == maxobjects) {
        Resize(maxobjects * 2);
    }

    numobjects++;

    return numobjects;
}

template<class Type>
int Container<Type>::AddUniqueObject(const Type& obj)
{
    int index;

    index = IndexOfObject(obj);

    if (!index) {
        index = AddObject(obj);
    }

    return index;
}

template<class Type>
void Container<Type>::AddObjectAt(int index, const Type& obj)
{
    if (index > maxobjects) {
        Resize(index);
    }

    if (index > numobjects) {
        numobjects = index;
    }

    SetObjectAt(index, obj);
}

template<class Type>
int Container<Type>::IndexOfObject(const Type& obj)
{
    int i;

    if (!objlist) {
        return 0;
    }

    for (i = 0; i < numobjects; i++) {
        if (objlist[i] == obj) {
            return i + 1;
        }
    }

    return 0;
}

template<class Type>
int Container<Type>::IndexOfObject(const Type& obj) const
{
    int i;

    if (!objlist) {
        return 0;
    }

    for (i = 0; i < numobjects; i++) {
        if (objlist[i] == obj) {
            return i + 1;
        }
    }

    return 0;
}

template<class Type>
bool Container<Type>::ObjectInList(const Type& obj)
{
    if (!IndexOfObject(obj)) {
        return false;
    }

    return true;
}

template<class Type>
Type& Container<Type>::ObjectAt(const size_t index) const
{
    if ((index == 0) || (index > (size_t)numobjects)) {
        CONTAINER_Error(ERR_DROP, "Container::ObjectAt : index out of range");
    }

    return objlist[index - 1];
}

template<class Type>
Type *Container<Type>::AddressOfObjectAt(int index)
{
    if (index > maxobjects) {
        CONTAINER_Error(ERR_DROP, "Container::AddressOfObjectAt : index is greater than maxobjects");
    }

    if (index > numobjects) {
        numobjects = index;
    }

    return &objlist[index - 1];
}

template<class Type>
void Container<Type>::InsertObjectAt(int index, const Type& obj)
{
    if ((index <= 0) || (index > numobjects + 1)) {
        CONTAINER_Error(ERR_DROP, "Container::RemoveObjectAt : index out of range");
        return;
    }

    numobjects++;
    int arrayIndex = index - 1;

    if (numobjects > maxobjects) {
        maxobjects = numobjects;
        if (!objlist) {
            objlist = new Type[maxobjects];
            objlist[arrayIndex] = obj;
            return;
        } else {
            Type *temp = objlist;
            if (maxobjects < numobjects) {
                maxobjects = numobjects;
            }

            objlist = new Type[maxobjects];
            int i;
            for (i = arrayIndex - 1; i >= 0; i--) {
                objlist[i] = temp[i];
            }

            objlist[arrayIndex] = obj;
            for (i = numobjects - 1; i > arrayIndex; i--) {
                objlist[i] = temp[i - 1];
            }
            delete[] temp;
        }
    } else {
        for (int i = numobjects - 1; i > arrayIndex; i--) {
            objlist[i] = objlist[i - 1];
        }
        objlist[arrayIndex] = obj;
    }
}

template<class Type>
int Container<Type>::MaxObjects(void) const
{
    return maxobjects;
}

template<class Type>
void Container<Type>::RemoveObjectAt(int index)
{
    int i;

    if (!objlist) {
        CONTAINER_WDPrintf("Container::RemoveObjectAt : Empty list\n");
        return;
    }

    if ((index <= 0) || (index > numobjects)) {
        CONTAINER_Error(ERR_DROP, "Container::RemoveObjectAt : index out of range");
        return;
    }

    i = index - 1;
    numobjects--;
    for (i = index - 1; i < numobjects; i++) {
        objlist[i] = objlist[i + 1];
    }
}

template<class Type>
void Container<Type>::RemoveObject(const Type& obj)
{
    int index;

    index = IndexOfObject(obj);

    if (!index) {
        CONTAINER_WDPrintf("Container::RemoveObject : Object not in list\n");
        return;
    }

    RemoveObjectAt(index);
}

template<class Type>
void Container<Type>::RemoveObject(const Type* obj)
{
    size_t index;

    index = (size_t)(obj - objlist);
    if (index >= (size_t)numobjects) {
        return;
    }

    RemoveObjectAt((int)index + 1);
}

template<class Type>
void Container<Type>::Reset()
{
    objlist    = NULL;
    numobjects = 0;
    maxobjects = 0;
}

template<class Type>
static void Container_qsort_swap(char *p1, char *p2, size_t width)
{
    char *end = p1 + width;

    while (p1 != end) {
        char c = *p1;
        *p1++  = *p2;
        *p2++  = c;
    }
}

template<class Type>
static void Container_qsort(void *base, size_t numobjects, size_t width, int (*compare)(const void *, const void *))
{
    char  *root;
    char  *last;
    size_t remaining;
    size_t index;
    char  *parent;
    char  *child;
    size_t j;

    if (numobjects < 2) {
        return;
    }

    remaining = numobjects;
    index     = (numobjects >> 1) + 1;
    root      = (char *)base + width * (numobjects >> 1);
    last      = (char *)base + width * (numobjects - 1);

    for (;;) {
        if (index <= 1) {
            Container_qsort_swap<Type>(last, root, width);
            if (--remaining == 1) {
                return;
            }
            last -= width;
        } else {
            --index;
            root -= width;
        }

        parent    = (char *)base + width * (index - 1);
        j         = index;
        child     = parent;
        while (2 * j <= remaining) {
            j *= 2;
            child = (char *)base + width * (j - 1);
            if (j < remaining && compare(child, child + width) < 0) {
                ++j;
                child += width;
            }
            if (compare(parent, child) >= 0) {
                break;
            }
            Container_qsort_swap<Type>(parent, child, width);
            parent = child;
        }
    }
}

template<class Type>
void Container<Type>::Sort(int (*compare)(const void *elem1, const void *elem2))
{
    if (!objlist) {
        CONTAINER_WDPrintf("Container::Sort : Empty list\n");
        return;
    }

    Container_qsort<Type>((void *)objlist, (size_t)numobjects, sizeof(Type), compare);
}

template<class Type>
Type& Container<Type>::operator[](const uintptr_t index) const
{
    return ObjectAt(index + 1);
}

template<class Type>
void Container<Type>::Copy(const Container<Type>& container)
{
    int i;

    if (&container == this) {
        return;
    }

    FreeObjectList();

    numobjects = container.numobjects;
    maxobjects = container.maxobjects;
    objlist    = NULL;

    if (container.objlist == NULL || !container.maxobjects) {
        return;
    }

    Resize(maxobjects);

    if (!container.numobjects) {
        return;
    }

    for (i = 0; i < container.numobjects; i++) {
        objlist[i] = container.objlist[i];
    }

    return;
}
