
// So this class is carried for completeness of the retail command surface, not because
#pragma once

#include "g_local.h"
#include "../corepp/class.h"
#include "archive.h"

class UseData : public Class
{
private:
    str _useAnim;
    str _useThread;
    str _useType;

    float _useMaxDist;
    int   _useCount;

public:
    UseData()
        : _useAnim("")
        , _useThread("")
        , _useType("")
        , _useMaxDist(64.0f)
        , _useCount(-1)
    {}

    ~UseData() {}

    const str& getUseAnim(void) const { return _useAnim; }
    const str& getUseThread(void) const { return _useThread; }
    const str& getUseType(void) const { return _useType; }
    float      getUseMaxDist(void) const { return _useMaxDist; }
    int        getUseCount(void) const { return _useCount; }

    void setUseAnim(const str& newanim) { _useAnim = newanim; }
    void setUseThread(const str& newthread) { _useThread = newthread; }
    void setUseType(const str& newtype) { _useType = newtype; }
    void setUseMaxDist(float newdist) { _useMaxDist = newdist; }
    void setUseCount(float newcount) { _useCount = (int)newcount; }

    void useMe(void);

    void Archive(Archiver& arc);
};

inline void UseData::Archive(Archiver& arc)
{
    Class::Archive(arc);

    arc.ArchiveString(&_useAnim);
    arc.ArchiveString(&_useThread);
    arc.ArchiveString(&_useType);
    arc.ArchiveFloat(&_useMaxDist);
    arc.ArchiveInteger(&_useCount);
}
