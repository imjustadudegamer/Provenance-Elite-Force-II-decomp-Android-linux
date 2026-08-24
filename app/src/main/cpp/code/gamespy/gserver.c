/******
gserver.c
GameSpy C Engine SDK
  
Copyright 1999-2000 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, CA 92614-6429
(949)798-4200
Fax(949)798-4299

http://developer.gamespy.com

******

Updated 10-15-99 (BGW)
    Modified ServerParseKeyVals to actually parse and store empty
    values for keys (i.e. "\delete\\" adds key="delete" and value="")
Updated 6-17-99 (DDW)
    Added new tokenize function to handle empty values for keys
Updated 11-9-00 (JED)
    Added ServerGetBoolValue() to get generic boolean values
    
*******/
#if defined(applec) || defined(THINK_C) || defined(__MWERKS__) && !defined(__KATANA__) && !defined(__mips64)
    #include "::nonport.h"
#else
    #include "../nonport.h"
#endif
#include "goaceng.h"
#include "gserver.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>



static int KeyValHashKeyP(const void *elem, int numbuckets);
static int KeyValCompareKeyP(const void *entry1, const void *entry2);

static char *mytok(char *instr, char delim)
{
    char *result;
    static char *thestr;

    if (instr)
        thestr = instr;
    result=thestr;
    while (*thestr && *thestr != delim)
    {
        thestr++;
    }
    if (thestr == result)
        result = NULL;
    if (*thestr) 
        *thestr++ = '\0';
    return result;
}


static char *LookupKey(GServer server, char *k)
{
    char **keyindex;

    keyindex = (char **)TableLookup(server->keylist,&k);
    if (keyindex != NULL)
        return *keyindex;
    k = strdup(k);
    TableEnter(server->keylist,&k);
    return k;
}

void ServerFree(void *elem)
{
    GServer server = *(GServer *)elem;
    
    TableFree(server->keyvals);
    gsifree(server);
}

static void ServerSetAddressFromString(GServer server, char *address)
{
    char *cpos;
    cpos = strchr(address,':');
    if (cpos == NULL)
        return; 
    *cpos = 0;
    server->ip = inet_addr(address);
    server->port = atoi(cpos+1);
}

GServer ServerNewData(char **fieldlist, int fieldcount, char *serverdata, GQueryType qtype, HashTable keylist)
{
    GServer server;
    char *k, *v;
    int curfield = 0;
    GKeyValuePair kvpair;
    server = ServerNew(0, 0, qtype, keylist);
    v = mytok(++serverdata,'\\'); 
    while (curfield < fieldcount)
    {
        k = fieldlist[curfield];
        if (v == NULL)
            v = "";
        if (strcmp(k, "ip")==0) 
        {
            ServerSetAddressFromString(server, v);
        } else if (qtype == qt_grouprooms && strcmp(k,"other")==0) 
        {
            char *temp;
            for (temp = v; *temp ; temp++)
                if (*temp == '\x1')
                    *temp = '\\';
            ServerParseKeyVals(server, v);
        } else
        {
            kvpair.key = LookupKey(server, k);
            kvpair.value = LookupKey(server, v);
            TableEnter(server->keyvals, &kvpair);
        }
        curfield++;
        if (curfield < fieldcount)
            v = mytok(NULL,'\\');
    }
    return server;
}

GServer ServerNew(goa_uint32 ip, unsigned short port, GQueryType qtype, HashTable keylist)
{
    GServer server;
    int nBuckets, nChains;

    server = gsimalloc(sizeof(struct GServerImplementation));
    server->ip = ip;
    server->port = port;
    server->ping = 9999;
    server->querytype = qtype;
    switch (qtype)
    {
    case qt_basic:
        nBuckets = 4;
        nChains = 2;
        break;
    case qt_info:
        nBuckets = 6;
        nChains = 2;
        break;
    case qt_players:
    case qt_rules:
        nBuckets = 8;
        nChains = 2;
        break;
    case qt_info_rules:
    case qt_status:
    default:
        nBuckets = 8;
        nChains=  4;
        break;
    }
    server->keyvals = TableNew2(sizeof(GKeyValuePair),nBuckets,nChains, KeyValHashKeyP, KeyValCompareKeyP, NULL);
    server->keylist = keylist;
    return server;
}


static int CheckValidKey(char *key)
{
    const char *InvalidKeys[]={"queryid","final"};
    int i;
    for (i = 0; i < sizeof(InvalidKeys)/sizeof(InvalidKeys[0]); i++)
    {
        if (strcmp(key,InvalidKeys[i]) == 0)
            return 0;
    }
    return 1;
}

void ServerParseKeyVals(GServer server, char *keyvals)
{
    char *k, *v;
    GKeyValuePair kvpair;

    k = mytok(++keyvals,'\\'); 
    while (k != NULL)
    {
        v = mytok(NULL,'\\');
        if (v == NULL)
            v = "";
        if (CheckValidKey(k))
        {
            kvpair.key = LookupKey(server, k);
            kvpair.value = LookupKey(server, v);
            TableEnter(server->keyvals, &kvpair);
        }
        k = mytok(NULL,'\\');

    }
}


int ServerGetPing(GServer server)
{
    return server->ping;
}

char *ServerGetAddress(GServer server)
{
    return (char *)inet_ntoa(*(struct in_addr*)&server->ip);
}

unsigned int ServerGetInetAddress(GServer server)
{
    return server->ip;
}


int ServerGetQueryPort(GServer server)
{
    return server->port;
}

static GKeyValuePair *ServerRuleLookup(GServer server, char *key)
{
    GKeyValuePair kvp;
    char **keyindex;
    keyindex = (char **)TableLookup(server->keylist, &key);
    if (keyindex == NULL)
        return NULL; 
    kvp.key = *keyindex;
    return (GKeyValuePair *)TableLookup(server->keyvals, &kvp);
}

char *ServerGetStringValue(GServer server, char *key, char *sdefault)
{
    GKeyValuePair *kv;

    if (strcmp(key,"hostaddr") == 0) 
        return ServerGetAddress(server);
    kv = ServerRuleLookup(server,key);
    if (!kv)
        return sdefault;
    return kv->value;
}

int ServerGetIntValue(GServer server, char *key, int idefault)
{
    GKeyValuePair *kv;

    if (strcmp(key,"ping") == 0) 
        return ServerGetPing(server);
    kv = ServerRuleLookup(server,key);
    if (!kv)
        return idefault;
    return atoi(kv->value);

}

double ServerGetFloatValue(GServer server, char *key, double fdefault)
{
    GKeyValuePair *kv;

    kv = ServerRuleLookup(server,key);
    if (!kv)
        return fdefault;
    return atof(kv->value);
}

gbool ServerGetBoolValue(GServer server, char *key, gbool bdefault)
{
    GKeyValuePair *kv;

    kv = ServerRuleLookup(server,key);
    if (!kv)
        return bdefault;

    if( '0' == *kv->value || 'F' == *kv->value || 'f' == *kv->value || 'N' == *kv->value || 'n' == *kv->value )
        return 0;

    return 1;
}

char *ServerGetPlayerStringValue(GServer server, int playernum, char *key, char *sdefault)
{
    char newkey[32];
    
    sprintf(newkey,"%s_%d",key,playernum);
    return ServerGetStringValue(server, newkey, sdefault);
}
int ServerGetPlayerIntValue(GServer server, int playernum, char *key, int idefault)
{
    char newkey[32];
    
    sprintf(newkey,"%s_%d",key,playernum);
    return ServerGetIntValue(server, newkey, idefault);

}
double ServerGetPlayerFloatValue(GServer server, int playernum, char *key, double fdefault)
{
    char newkey[32];
    
    sprintf(newkey,"%s_%d",key,playernum);
    return ServerGetFloatValue(server, newkey, fdefault);

}



static void KeyMapF(void *elem, void *clientData)
{
    GKeyValuePair *kv = (GKeyValuePair *)elem;
    GEnumData *ped = (GEnumData *)clientData;
    ped->EnumFn(kv->key, kv->value, ped->instance);
}


void ServerEnumKeys(GServer server, KeyEnumFn KeyFn, void *instance)
{
    GEnumData ed;

    ed.EnumFn = KeyFn;
    ed.instance = instance;
    ed.keylist = server->keylist;
    TableMap(server->keyvals, KeyMapF, &ed);
}




#define MULTIPLIER -1664117991
static int StringHash(char *s, int numbuckets)
{
    goa_uint32 hashcode = 0;
    while (*s != 0)
    {
        hashcode = hashcode * MULTIPLIER + tolower(*s);
        s++;
    }
    return (hashcode % numbuckets);

}

int GStringHash(const void *elem, int numbuckets)
{
    return StringHash(*(char **)elem, numbuckets);
}

static int KeyValHashKeyP(const void *elem, int numbuckets)
{
    return StringHash(((GKeyValuePair *)elem)->key, numbuckets);
}


int GCaseInsensitiveCompare(const void *entry1, const void *entry2)
{
    return strcasecmp(*(char **)entry1,*(char **)entry2);
}

static int KeyValCompareKeyP(const void *entry1, const void *entry2)
{
       return ((GKeyValuePair *)entry1)->key - ((GKeyValuePair *)entry2)->key;
}

void GStringFree(void *elem)
{
    gsifree(*(char **)elem);
}

