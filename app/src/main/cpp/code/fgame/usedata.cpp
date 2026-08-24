

#include "g_local.h"
#include "usedata.h"

void UseData::useMe(void)
{
    if (_useCount == -1) {
        return;
    }

    _useCount--;
}
