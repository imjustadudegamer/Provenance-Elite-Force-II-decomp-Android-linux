
#ifndef _PEERMAIN_H_
#define _PEERMAIN_H_

#include "peer.h"
#include "../darray.h"
#include "../hashtable.h"
#include "../pinger/pinger.h"
#include "../Chat/chatASCII.h"


#ifdef __cplusplus
extern "C" {
#endif


#if 0
piConnection * connection;  
#endif

#define PI_ROOM_MAX_LEN           257
#define PI_NICK_MAX_LEN           64
#define PI_NAME_MAX_LEN           512
#define PI_TITLE_MAX_LEN          32
#define PI_AWAY_MAX_LEN           128
#define PI_SB_LEN                 32  

#ifndef PI_CHAT_PING_TIME
	#ifndef _NITRO
		#define PI_CHAT_PING_TIME (5 * 60000)
	#else
		#define PI_CHAT_PING_TIME (20000)
	#endif
#endif

#define PEER_CONNECTION           piConnection * connection;\
                                  assert(peer);\
                                  connection = (piConnection *)peer;\
								  GSI_UNUSED(connection);

#define ASSERT_ROOMTYPE(type)     assert((type == TitleRoom) || (type == GroupRoom) || (type == StagingRoom))
#define ASSERT_MESSAGETYPE(type)  assert((type == NormalMessage) || (type == ActionMessage) || (type == NoticeMessage))

#define ROOM                      (connection->rooms[roomType])
#define ROOM_W					  (connection->rooms_W[roomType])
#define ROOMS                     (connection->rooms)
#define NAME                      (connection->names[roomType])
#define NAME_W					  (connection->names_W[roomType])
#define NAMES                     (connection->names)
#define IN_ROOM                   (connection->inRoom[roomType])
#define ENTERING_ROOM             (connection->enteringRoom[roomType])

#define strzcpy(dest, src, len)   { strncpy(dest, src, (len)); (dest)[(len) - 1] = '\0'; }
#define strzcat(dest, src, len)   { strncat(dest, src, (len) - strlen(dest)); (dest)[(len) - 1] = '\0'; }

#if defined(_PS3)
#define PEERCBType void*       
#else
#define PEERCBType int         
#endif

typedef struct piConnection
{
	CHAT chat;  
	char nick[PI_NICK_MAX_LEN];  
	PEERBool connecting;
	PEERBool connected;
	peerNickErrorCallback nickErrorCallback;
	gsi_time lastChatPing;

	unsigned int publicIP;
	unsigned int privateIP;
	int profileID;
	char title[PI_TITLE_MAX_LEN];

#ifdef GSI_UNICODE
	unsigned short title_W[PI_TITLE_MAX_LEN];
	unsigned short nick_W[PI_NICK_MAX_LEN];
	unsigned short names_W[NumRooms][PI_NAME_MAX_LEN];
	unsigned short rooms_W[NumRooms][PI_ROOM_MAX_LEN];
#endif

	char rooms[NumRooms][PI_ROOM_MAX_LEN];
	PEERBool enteringRoom[NumRooms];
	PEERBool inRoom[NumRooms];
	char names[NumRooms][PI_NAME_MAX_LEN];
	int oldFlags[NumRooms];
	int groupID;
	char titleRoomChannel[PI_ROOM_MAX_LEN];
	PEERBool stayInTitleRoom;

	HashTable players;
	int numPlayers[NumRooms];
	PEERBool alwaysRequestPlayerInfo;

	PEERBool doPings;
	int lastPingTimeMod;
	PEERBool pingRoom[NumRooms];
	PEERBool xpingRoom[NumRooms];
	HashTable xpings;
	unsigned int lastXpingSend;
	
	qr2_t queryReporting;
	char qrSecretKey[64]; 
	PEERBool natNegotiate;
	int reportingOptions;
	int reportingGroupID;  

	PEERBool hosting;
	PEERBool playing;
	int maxPlayers;
	PEERBool passwordedRoom;

	SBServer hostServer;
	PEERBool ready;

	char sbName[PI_SB_LEN];
	char sbSecretKey[PI_SB_LEN];
	int sbGameVersion;
	int sbMaxUpdates;
	PEERBool sbInitialized;
	SBServerList gameList;
	SBServerList groupList;
	SBQueryEngine gameEngine;
	peerListingGamesCallback gameListCallback;
	void * gameListParam;
	PEERBool initialGameList;
	struct piOperation * listingGroupsOperation;

	int nextID;

	DArray operationList;
	int operationsStarted;
	int operationsFinished;

	PEERCallbacks callbacks;
	DArray callbackList;
	int callbacksQueued;
	int callbacksCalled;
	int callbackDepth;

	PEERBool away;
	char awayReason[PI_AWAY_MAX_LEN];

	HashTable globalWatchKeys[NumRooms];
	HashTable roomWatchKeys[NumRooms];
	HashTable globalWatchCache;
	HashTable roomWatchCache[NumRooms];

	PEERAutoMatchStatus autoMatchStatus;
	SBServerList autoMatchList;
	SBQueryEngine autoMatchEngine;
	PEERBool autoMatchBrowsing;
	struct piOperation * autoMatchOperation;
	qr2_t autoMatchReporting;
	char * autoMatchFilter;
	PEERBool autoMatchSBFailed;
	PEERBool autoMatchQRFailed;

	PEERBool disconnect;
	PEERBool shutdown;
} piConnection;

void piSendChannelUTM(PEER peer, const char * channel, const char * command, const char * parameters, PEERBool authenticate);
void piSendPlayerUTM(PEER peer, const char * nick, const char * command, const char * parameters, PEERBool authenticate);
PEERBool piConnectTitle(PEER peer);
void piDisconnectTitle(PEER peer);

#ifdef __cplusplus
}
#endif

#endif
