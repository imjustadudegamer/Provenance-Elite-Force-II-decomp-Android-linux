
#include "goaceng.h"

#ifndef _GSERVER_H_
#define _GSERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(applec) || defined(THINK_C) || defined(__MWERKS__) && !defined(__KATANA__) && !defined(__mips64)
    #include "::hashtable.h"
    #include "::nonport.h"
#else
    #include "../hashtable.h"
    #include "../nonport.h"
#endif

struct GServerImplementation
{
    goa_uint32 ip;
    unsigned short port;
    short ping;
    GQueryType querytype;
    HashTable keyvals;
    HashTable keylist;

};

typedef struct
{
    char *key;
    char *value;
} GKeyValuePair;
typedef struct 
{
    KeyEnumFn EnumFn;
    void *instance;
    HashTable keylist;
} GEnumData;

void ServerFree(void *elem);
GServer ServerNew(goa_uint32 ip, unsigned short port, GQueryType qtype, HashTable keylist);
GServer ServerNewData(char **fieldlist, int fieldcount, char *serverdata, GQueryType qtype, HashTable keylist);
void ServerParseKeyVals(GServer server, char *keyvals);
int GStringHash(const void *elem, int numbuckets);
int GCaseInsensitiveCompare(const void *entry1, const void *entry2);
void GStringFree(void *elem);
#ifdef __cplusplus
}
#endif

#endif