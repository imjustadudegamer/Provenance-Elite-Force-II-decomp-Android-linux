/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#ifndef _GOACENG_H
#define _GOACENG_H

#ifdef __cplusplus
extern "C" {
#endif


#define ENGINE_VERSION "3.0"
#define SERVER_TIMEOUT 3000
typedef struct GServerListImplementation *GServerList;
typedef struct GServerImplementation *GServer;

typedef int GError; 

typedef int gbool;  

typedef enum {sl_idle, sl_listxfer, sl_lanlist, sl_querying} GServerListState;

typedef enum {cm_int, cm_float, cm_strcase, cm_stricase} GCompareMode;


typedef enum {qt_basic, qt_info, qt_rules, qt_players, qt_info_rules, qt_status, qt_grouprooms, qt_masterinfo} GQueryType;

#define	LIST_STATECHANGED		1 
#define LIST_PROGRESS			2 

typedef void (*ListCallBackFn)(GServerList serverlist, int msg, void *instance, void *param1, void *param2);

typedef void (*KeyEnumFn)(char *key, char *value, void *instance);


#define GCALLBACK_FUNCTION 1	

#define GE_NOERROR		0
#define GE_NOSOCKET		1
#define GE_NODNS		2
#define GE_NOCONNECT	3
#define GE_BUSY			4
#define GE_DATAERROR	5

extern char ServerListHostname[64];


GServerList	ServerListNew( const char *gamename, const char *enginename, const char *seckey, int maxconcupdates, void *CallBackFn, int CallBackFnType, void *instance);

void ServerListFree(GServerList serverlist);

GError ServerListUpdate(GServerList serverlist, gbool async);


GError ServerListUpdate2(GServerList serverlist, gbool async, char *filter, GQueryType querytype);



 
GError ServerListLANUpdate(GServerList serverlist, gbool async, int startsearchport, int endsearchport, int searchdelta);

GError ServerListAuxUpdate(GServerList serverlist, const char *ip, int port, gbool async, GQueryType querytype);

void ServerListRemoveServer(GServerList serverlist, char *ip, int port);

GError ServerListThink(GServerList serverlist); 

GError ServerListHalt(GServerList serverlist);

GError ServerListClear(GServerList serverlist);

GServerListState ServerListState(GServerList serverlist);

char *ServerListErrorDesc(GServerList serverlist, GError error);

GServer ServerListGetServer(GServerList serverlist, int index);

int ServerListCount(GServerList serverlist);

void ServerListSort(GServerList serverlist, gbool ascending, char *sortkey, GCompareMode comparemode);



int ServerGetPing(GServer server);

char *ServerGetAddress(GServer server);

unsigned int ServerGetInetAddress(GServer server);


int ServerGetQueryPort(GServer server);

char *ServerGetStringValue(GServer server, char *key, char *sdefault);
int ServerGetIntValue(GServer server, char *key, int idefault);
double ServerGetFloatValue(GServer server, char *key, double fdefault);
gbool ServerGetBoolValue(GServer server, char *key, gbool bdefault);


char *ServerGetPlayerStringValue(GServer server, int playernum, char *key, char *sdefault);
int ServerGetPlayerIntValue(GServer server, int playernum, char *key, int idefault);
double ServerGetPlayerFloatValue(GServer server, int playernum, char *key, double fdefault);

void ServerEnumKeys(GServer server, KeyEnumFn KeyFn, void *instance);

#ifdef __cplusplus
}
#endif

#endif 