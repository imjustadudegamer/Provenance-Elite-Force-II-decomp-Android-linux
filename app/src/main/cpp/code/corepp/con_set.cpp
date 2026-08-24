

#include "con_set.h"
#include "short3.h"
#include "../corepp/str.h"
#include "../corepp/vector.h"


template<>
int HashCode<const char *>(const char *const& key)
{
    const char *p;
    int         hash = 0;

    for (p = key; *p; p++) {
        hash = *p + 31 * hash;
    }

    return hash;
}

template<>
int HashCode<void *>(void *const& key)
{
    return 0;
}

template<>
int HashCode<const void *>(const void *const& key)
{
    return 0;
}

template<>
int HashCode<char>(const char& key)
{
    return key;
}

template<>
int HashCode<unsigned char>(const unsigned char& key)
{
    return key;
}

template<>
int HashCode<unsigned char *>(unsigned char *const& key)
{
    return (int)(size_t)key;
}

template<>
int HashCode<const unsigned char *>(const unsigned char *const& key)
{
    return (int)(size_t)key;
}

template<>
int HashCode<short>(const short& key)
{
    return key;
}

template<>
int HashCode<unsigned short>(const unsigned short& key)
{
    return key;
}

template<>
int HashCode<short3>(const short3& key)
{
    return key;
}

template<>
int HashCode<unsigned_short3>(const unsigned_short3& key)
{
    return key;
}

template<>
int HashCode<int>(const int& key)
{
    return key;
}

template<>
int HashCode<unsigned int>(const unsigned int& key)
{
    return key;
}

template<>
int HashCode<float>(const float& key)
{
    return *(int *)&key;
}

template<>
int HashCode<double>(const double& key)
{
    return *(int *)&key;
}

template<>
int HashCode<str>(const str& key)
{
    return HashCode<const char *>(key.c_str());
}

template<>
int HashCode<Vector>(const Vector& key)
{
    return (int)((key[0] + key[1] + key[2]) / 3);
}
