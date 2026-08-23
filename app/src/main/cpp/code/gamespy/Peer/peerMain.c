
#include <limits.h>
#include "peer.h"
#include "peerAscii.h"
#include "peerMain.h"
#include "peerCallbacks.h"
#include "peerGlobalCallbacks.h"
#include "peerOperations.h"
#include "peerPlayers.h"
#include "peerRooms.h"
#include "peerPing.h"
#include "peerSB.h"
#include "peerMangle.h"
#include "peerKeys.h"
#include "peerQR.h"
#include "peerHost.h"
#include "peerAutoMatch.h"
#include "../common/gsAvailable.h"

#define PEER_CONNECTED         assert(connection->connected);

#define PI_CHECK_SHUTDOWN      if(connection->shutdown && (connection->callbackDepth == 0))\
                               {\
                                   peerShutdown(peer);\
                               }

#define PI_INIT_FAILED         {\
                                   piShutdownCleanup(peer);\
                                   return NULL;\
                               }

#define PI_DO_BLOCKING         if(blocking)\
                               {\
                                   do\
                                   {\
                                       msleep(1);\
                                       piThink(peer, opID);\
                                   }\
                                   while(!piCheckBlockingID(peer, opID));\
								   PI_CHECK_SHUTDOWN\
                               }

#define PI_OP_ID               int opID = piGetNextID(peer)
#if 0
int opID;  
#endif

static void piShutdownCleanup(PEER peer);
static void piDisconnectCleanup(PEER peer);
static void piThink(PEER peer,int ID);

void peerCreateStagingRoomWithSocketA(PEER peer, const char * name, int maxPlayers, const char password[PEER_PASSWORD_LEN], SOCKET socket, unsigned short port, peerJoinRoomCallback callback, void * param, PEERBool blocking);
void peerStartAutoMatchWithSocketA(PEER peer, int maxPlayers, const char * filter, SOCKET socket, unsigned short port, peerAutoMatchStatusCallback statusCallback, peerAutoMatchRateCallback rateCallback, void * param, PEERBool blocking);

static PEERBool piCheckBlockingID
(
	PEER peer,
	int ID
)
{
	return (PEERBool)(piIsOperationFinished(peer, ID) && piIsCallbackFinished(peer, ID));
}

static unsigned int piGetPrivateIP(void)
{
	HOSTENT * host;
	IN_ADDR * addr;
	int i;

	host = getlocalhost();
	if(!host)
		return 0;

	for(i = 0 ; host->h_addr_list[i] ; i++)
	{
		addr = (IN_ADDR *)host->h_addr_list[i];
		if(IsPrivateIP(addr))
			return addr->s_addr;
	}

	return 0;
}

PEER peerInitialize
(
	PEERCallbacks * callbacks
)
{
	PEER peer;
	piConnection * connection;

	assert(callbacks);

	if(__GSIACResult != GSIACAvailable)
		return NULL;

	SocketStartUp();

	connection = (piConnection *)gsimalloc(sizeof(piConnection));
	if(!connection)
		return NULL;
	memset(connection, 0, sizeof(piConnection));
	peer = (PEER)connection;

	connection->chat = NULL;
	connection->nick[0] = '\0';
	connection->connecting = PEERFalse;
	connection->connected = PEERFalse;

	connection->privateIP = piGetPrivateIP();
	connection->title[0] = '\0';

#ifdef GSI_UNICODE
	connection->title_W[0] = '\0';
	connection->nick_W[0] = '\0';
#endif

	connection->nextID = 0;

	if(!piOperationsInit(peer))
		PI_INIT_FAILED

	connection->callbacks = *callbacks;
	if(!piCallbacksInit(peer))
		PI_INIT_FAILED

	if(!piKeysInit(peer))
		PI_INIT_FAILED

	connection->shutdown = PEERFalse;

	return peer;
}

void peerConnectA
(
	PEER peer,
	const char * nick,
	int profileID,
	peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);
	assert(profileID >= 0);
	assert(connectCallback);
	assert(strlen(nick) < PI_NICK_MAX_LEN);

	if(connection->connected || connection->connecting)
		success = PEERFalse;

	if(success && !connection->title[0])
		success = PEERFalse;

	if(success)
	{
		connection->chat = NULL;
		strzcpy(connection->nick, nick, PI_NICK_MAX_LEN);
		connection->connected = PEERFalse;
		connection->connecting = PEERTrue;
		connection->nickErrorCallback = nickErrorCallback;

#ifdef GSI_UNICODE
		UTF8ToUCS2String(connection->nick, connection->nick_W);
#endif
		connection->profileID = profileID;
		connection->disconnect = PEERFalse;

		if(!piNewConnectOperation(peer, PI_CONNECT, nick, 0, NULL, NULL, NULL, NULL, NULL, NULL, connectCallback, param, opID))
		{
			success = PEERFalse;
			piDisconnectCleanup(peer);
		}
	}

	if(!success)
		piAddConnectCallback(peer, PEERFalse, PEER_DISCONNECTED, connectCallback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerConnectW
(
	PEER peer,
	const unsigned short * nick,
	int profileID,
	peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback,
	void * param,
	PEERBool blocking
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	peerConnectA(peer, nick_A, profileID, nickErrorCallback, connectCallback, param, blocking);
	gsifree(nick_A);
}
#endif

void peerConnectLoginA
(
	PEER peer,
	int namespaceID,
	const char * email,
	const char * profilenick,
	const char * uniquenick,
	const char * password,
	peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;

	assert(connectCallback);

	if(connection->connected || connection->connecting)
		success = PEERFalse;

	if(success && !connection->title[0])
		success = PEERFalse;

	if(success)
	{
		connection->chat = NULL;
		connection->nick[0] = '\0';
		connection->connected = PEERFalse;
		connection->connecting = PEERTrue;
		connection->nickErrorCallback = nickErrorCallback;

#ifdef GSI_UNICODE
		UTF8ToUCS2String(connection->nick, connection->nick_W);
#endif
		connection->profileID = 0;
		connection->disconnect = PEERFalse;

		if(!piNewConnectOperation(peer, (uniquenick && uniquenick[0])?PI_CONNECT_UNIQUENICK_LOGIN:PI_CONNECT_PROFILENICK_LOGIN, NULL, namespaceID, email, profilenick, uniquenick, password, NULL, NULL, connectCallback, param, opID))
		{
			success = PEERFalse;
			piDisconnectCleanup(peer);
		}
	}

	if(!success)
		piAddConnectCallback(peer, PEERFalse, PEER_DISCONNECTED, connectCallback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerConnectLoginW
(
	PEER peer,
	int namespaceID,
	const unsigned short * email,
	const unsigned short * profilenick,
	const unsigned short * uniquenick,
	const unsigned short * password,
	peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback,
	void * param,
	PEERBool blocking
)
{
	char* email_A = UCS2ToUTF8StringAlloc(email);
	char* profilenick_A = UCS2ToUTF8StringAlloc(profilenick);
	char* uniquenick_A = UCS2ToUTF8StringAlloc(uniquenick);
	char* password_A = UCS2ToUTF8StringAlloc(password);
	peerConnectLoginA(peer, namespaceID, email_A, profilenick_A, uniquenick_A, password_A, nickErrorCallback, connectCallback, param, blocking);
	gsifree(email_A);
	gsifree(profilenick_A);
	gsifree(uniquenick_A);
	gsifree(password_A);
}
#endif

void peerConnectPreAuthA
(
	PEER peer,
	const char * authtoken,
	const char * partnerchallenge,
	peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;

	assert(authtoken && authtoken[0]);
	assert(partnerchallenge && partnerchallenge[0]);
	assert(connectCallback);

	if(connection->connected || connection->connecting)
		success = PEERFalse;

	if(success && !connection->title[0])
		success = PEERFalse;

	if(success)
	{
		connection->chat = NULL;
		connection->nick[0] = '\0';
		connection->connected = PEERFalse;
		connection->connecting = PEERTrue;
		connection->nickErrorCallback = nickErrorCallback;

#ifdef GSI_UNICODE
		UTF8ToUCS2String(connection->nick, connection->nick_W);
#endif
		connection->profileID = 0;
		connection->disconnect = PEERFalse;

		if(!piNewConnectOperation(peer, PI_CONNECT_PREAUTH, NULL, 0, NULL, NULL, NULL, NULL, authtoken, partnerchallenge, connectCallback, param, opID))
		{
			success = PEERFalse;
			piDisconnectCleanup(peer);
		}
	}

	if(!success)
		piAddConnectCallback(peer, PEERFalse, PEER_DISCONNECTED, connectCallback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerConnectPreAuthW
(
	PEER peer,
	const unsigned short * authtoken,
	const unsigned short * partnerchallenge,
	peerNickErrorCallback nickErrorCallback,
	peerConnectCallback connectCallback,
	void * param,
	PEERBool blocking
)
{
	char* authtoken_A = UCS2ToUTF8StringAlloc(authtoken);
	char* partnerchallenge_A = UCS2ToUTF8StringAlloc(partnerchallenge);
	peerConnectPreAuthA(peer, authtoken_A, partnerchallenge_A, nickErrorCallback, connectCallback, param, blocking);
	gsifree(authtoken_A);
	gsifree(partnerchallenge_A);
}
#endif

void peerRetryWithNickA
(
	PEER peer,
	const char * nick
)
{
	PEER_CONNECTION;

	assert(connection->connecting);
	if(!connection->connecting)
		return;

	if(nick && nick[0])
	{
		strzcpy(connection->nick, nick, PI_NICK_MAX_LEN);

#ifdef GSI_UNICODE
		UTF8ToUCS2String(connection->nick, connection->nick_W);
#endif
	}

	chatRetryWithNickA(connection->chat, nick);
}
#ifdef GSI_UNICODE
void peerRetryWithNickW
(
	PEER peer,
	const unsigned short * nick
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	peerRetryWithNickA(peer, nick_A);
	gsifree(nick_A);
}
#endif

void peerRegisterUniqueNickA
(
	PEER peer,
	int namespaceID,
	const char * uniquenick,
	const char * cdkey)
{
	PEER_CONNECTION;

	assert(connection->connecting);
	if(!connection->connecting)
		return;

	chatRegisterUniqueNickA(connection->chat, namespaceID, uniquenick, cdkey);
}
#ifdef GSI_UNICODE
void peerRegisterUniqueNickW
(
	PEER peer,
	int namespaceID,
	const unsigned short * uniquenick,
	const unsigned short * cdkey
)
{
	char* uniquenick_A = UCS2ToUTF8StringAlloc(uniquenick);
	char* cdkey_A = UCS2ToUTF8StringAlloc(cdkey);
	peerRegisterUniqueNickA(peer, namespaceID, uniquenick_A, cdkey_A);
	gsifree(uniquenick_A);
	gsifree(cdkey_A);
}
#endif

PEERBool peerIsConnected(PEER peer)
{
	PEER_CONNECTION;

	return connection->connected;
}

PEERBool piConnectTitle(PEER peer)
{
	if(!piRoomsInit(peer))
		return PEERFalse;

	if(!piPlayersInit(peer))
		return PEERFalse;

	piPingInit(peer);

	return PEERTrue;
}

void piDisconnectTitle(PEER peer)
{
	piRoomsCleanup(peer);

	piPlayersCleanup(peer);

	piPingCleanup(peer);

	piStopAutoMatch(peer);
}

PEERBool peerSetTitleA
(
	PEER peer,
	const char * title,
	const char * qrSecretKey,
	const char * sbName,
	const char * sbSecretKey,
	int sbGameVersion,
	int sbMaxUpdates,
	PEERBool natNegotiate,
	PEERBool pingRooms[NumRooms],
	PEERBool crossPingRooms[NumRooms]
)
{
	static PEERBool noPings[NumRooms];
	PEERBool pingTitleRoom = PEERFalse;
	PEERBool xpingTitleRoom = PEERFalse;

	PEER_CONNECTION;

	assert(title);
	assert(title[0]);
	assert(strlen(title) < PI_TITLE_MAX_LEN);
	assert(qrSecretKey);
	assert(sbName);
	assert(sbName[0]);
	assert(sbSecretKey);

	if(connection->title[0])
		peerClearTitle(peer);

	strcpy(connection->title, title);

#ifdef GSI_UNICODE
	AsciiToUCS2String(title, connection->title_W);
#endif

	if(!pingRooms)
		pingRooms = noPings;
	if(!crossPingRooms)
		crossPingRooms = noPings;

	if(connection->stayInTitleRoom)
	{
		pingTitleRoom = connection->pingRoom[TitleRoom];
		xpingTitleRoom = connection->xpingRoom[TitleRoom];
	}

	memcpy(connection->pingRoom, pingRooms, sizeof(PEERBool) * NumRooms);
	memcpy(connection->xpingRoom, crossPingRooms, sizeof(PEERBool) * NumRooms);

	if(connection->stayInTitleRoom)
	{
		connection->pingRoom[TitleRoom] = pingTitleRoom;
		connection->xpingRoom[TitleRoom] = xpingTitleRoom;
	}

	strzcpy(connection->sbName, sbName, PI_SB_LEN);
	strzcpy(connection->sbSecretKey, sbSecretKey, PI_SB_LEN);
	connection->sbGameVersion = sbGameVersion;
	connection->sbMaxUpdates = sbMaxUpdates;

	if(!piSBInit(peer))
		return PEERFalse;

	if(connection->connected)
	{
		if(!piConnectTitle(peer))
		{
			peerClearTitle(peer);
			return PEERFalse;
		}
	}

	strcpy(connection->qrSecretKey, qrSecretKey);
	piStopHosting(peer, PEERTrue);
	connection->hosting = PEERFalse;
	connection->playing = PEERFalse;
	connection->natNegotiate = natNegotiate;

	connection->ready = PEERFalse;

	connection->stayInTitleRoom = PEERFalse;

	return PEERTrue;
}
#ifdef GSI_UNICODE
PEERBool peerSetTitleW
(
	PEER peer,
	const unsigned short * title,
	const unsigned short * qrSecretKey,
	const unsigned short * sbName,
	const unsigned short * sbSecretKey,
	int sbGameVersion,
	int sbMaxUpdates,
	PEERBool natNegotiate,
	PEERBool pingRooms[NumRooms],
	PEERBool crossPingRooms[NumRooms]
)
{
	char* title_A = UCS2ToUTF8StringAlloc(title);
	char* qrSecretKey_A = UCS2ToUTF8StringAlloc(qrSecretKey);
	char* sbName_A = UCS2ToUTF8StringAlloc(sbName);
	char* sbSecretKey_A = UCS2ToUTF8StringAlloc(sbSecretKey);
	PEERBool result = peerSetTitleA(peer, title_A, qrSecretKey_A, sbName_A, sbSecretKey_A, sbGameVersion, sbMaxUpdates, natNegotiate, pingRooms, crossPingRooms);
	gsifree(title_A);
	gsifree(qrSecretKey_A);
	gsifree(sbName_A);
	gsifree(sbSecretKey_A);
	return result;
}
#endif

void peerClearTitle(PEER peer)
{
	PEER_CONNECTION;

	piStopHosting(peer, PEERTrue);

	piSBCleanup(peer);

	piDisconnectTitle(peer);

	connection->title[0] = '\0';

#ifdef GSI_UNICODE
	connection->title_W[0] = '\0';
#endif

	connection->qrSecretKey[0] = '\0';
}

const char* peerGetTitleA(PEER peer)
{
	PEER_CONNECTION;

	if(!connection->title[0])
		return NULL;

	return connection->title;
}
#ifdef GSI_UNICODE
const unsigned short* peerGetTitleW(PEER peer)
{
	PEER_CONNECTION;

	if(!connection->title_W[0])
		return NULL;

	return connection->title_W;
}
#endif

static void piDisconnectCleanup(PEER peer)
{
	PEER_CONNECTION;

	if(connection->chat)
		chatDisconnect(connection->chat);
	connection->chat = NULL;
	connection->nick[0] = '\0';
	connection->connecting = PEERFalse;
	connection->connected = PEERFalse;
	
#ifdef GSI_UNICODE
	connection->nick_W[0] = '\0';
#endif

	piOperationsReset(peer);

	piDisconnectTitle(peer);

	connection->away = PEERFalse;
	connection->awayReason[0] = '\0';

	connection->disconnect = PEERFalse;
}

static void piDisconnect(PEER peer)
{
	PEER_CONNECTION;

	if(connection->callbackDepth > 0)
	{
		connection->disconnect = PEERTrue;

		return;
	}

	connection->stayInTitleRoom = PEERFalse;

	piDisconnectCleanup(peer);

	piThink(peer, -1);
}

void peerDisconnect(PEER peer)
{
	PEER_CONNECTION;

	piDisconnect(peer);

	PI_CHECK_SHUTDOWN
}

static void piShutdownCleanup(PEER peer)
{
	PEER_CONNECTION;

	piOperationsCleanup(peer);

	piCallbacksCleanup(peer);

	SocketShutDown();

	piKeysCleanup(peer);

	gsifree(connection);
}

void peerShutdown(PEER peer)
{
	PEER_CONNECTION;

	if(connection->connected || connection->connecting)
	{
		peerDisconnectedCallback callback;

		callback = connection->callbacks.disconnected;
		connection->callbacks.disconnected = NULL;
		piDisconnect(peer);
		connection->callbacks.disconnected = callback;
	}

	if(connection->title[0])
		peerClearTitle(peer);

	if(connection->callbackDepth > 0)
	{
		connection->shutdown = PEERTrue;

		return;
	}

	piShutdownCleanup(peer);
}

#ifdef _DEBUG
static void piNumPlayersConsistencyCheckMap
(
	piPlayer * player,
	int count[NumRooms]
)
{
	int i;

	assert(player);
	assert(count);

	for(i = 0 ; i < NumRooms ; i++)
	{
		if(player->inRoom[i])
			count[i]++;
	}
}
#endif

static void piThink
(
	PEER peer,
	int opID
)
{
	gsi_time now;
	PEER_CONNECTION;

#ifdef _DEBUG
	if(connection->players)
	{
		{
			int count[NumRooms];
			int i;

			for(i = 0 ; i < NumRooms ; i++)
				count[i] = 0;

			TableMap(connection->players, (TableMapFn)piNumPlayersConsistencyCheckMap, count);

			for(i = 0 ; i < NumRooms ; i++)
				assert(count[i] == connection->numPlayers[i]);
		}
	}
#endif

#if 0
	{
		char buffer[1024];
		static int counter = -1;

		counter++;
		counter %= 20;
		if(counter == 0)
		{
			sprintf(buffer,
				"---------------------\n"
				"operationsStarted:  %d\n"
				"operationsFinished: %d\n"
				"callbacksQueued:    %d\n"
				"callbacksCalled:    %d\n"
				"callbackDepth:      %d\n"
				"titleRoomPlayers:   %d\n"
				"groupRoomPlayers:   %d\n"
				"stagingRoomPlayers: %d\n",
				connection->operationsStarted,
				connection->operationsFinished,
				connection->callbacksQueued,
				connection->callbacksCalled,
				connection->callbackDepth,
				connection->numPlayers[TitleRoom],
				connection->numPlayers[GroupRoom],
				connection->numPlayers[StagingRoom]);
			OutputDebugString(buffer);
		}
	}
#endif

	if(connection->connected || connection->connecting)
	{
		chatThink(connection->chat);

		if(!connection->disconnect)
		{
			if(connection->title[0])
			{
				piPingThink(peer);
			}

			if(connection->connected)
			{
				now = current_time();

				if((now - connection->lastChatPing) > PI_CHAT_PING_TIME)
				{
					chatSendRawA(connection->chat, "PING");

					connection->lastChatPing = now;
				}
			}
		}
	}

	piSBThink(peer);

	piQRThink(peer);

	if(connection->disconnect && (connection->callbackDepth == 0))
		piDisconnect(peer);

	piCallbacksThink(peer, opID);
}

void peerThink(PEER peer)
{
	PEER_CONNECTION;

	piThink(peer, -1);

	PI_CHECK_SHUTDOWN
}

CHAT peerGetChat(PEER peer)
{
	PEER_CONNECTION;

	return connection->chat;
}

const char * peerGetNickA(PEER peer)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->connected)
		return NULL;

	return connection->nick;
}
#ifdef GSI_UNICODE
const unsigned short * peerGetNickW(PEER peer)
{
	PEER_CONNECTION;
	PEER_CONNECTED;
	if (!connection->connected)
		return NULL;
	return connection->nick_W;
}
#endif

#ifndef GSI_UNICODE
void peerFixNickA
(
	char * newNick,
	const char * oldNick
)
{
	chatFixNickA(newNick, oldNick);
}
const char * peerTranslateNickA
(
	char * nick,
	const char * extension
)
{
	return chatTranslateNickA(nick, extension);
}
#else
void peerFixNickW
(
	unsigned short * newNick,
	const unsigned short * oldNick
)
{
	chatFixNickW(newNick, oldNick);
}
const unsigned short * peerTranslateNickW
(
	unsigned short * nick,
	const unsigned short * extension
)
{
	return chatTranslateNickW(nick, extension);
}
#endif

unsigned int peerGetPublicIP(PEER peer)
{
	PEER_CONNECTION;

	return connection->publicIP;
}

unsigned int peerGetPrivateIP(PEER peer)
{
	PEER_CONNECTION;

	return connection->privateIP;
}

int peerGetUserID(PEER peer)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->connected)
		return 0;

	return chatGetUserID(connection->chat);
}

int peerGetProfileID(PEER peer)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->connected)
		return 0;

	return chatGetProfileID(connection->chat);
}

void peerChangeNickA
(
	PEER peer,
	const char * newNick,
	peerChangeNickCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;
	
	assert(callback);

	if(!piNewChangeNickOperation(peer, newNick, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddChangeNickCallback(peer, PEERFalse, connection->nick, newNick, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerChangeNickW
(
	PEER peer,
	const unsigned short * newNick,
	peerChangeNickCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* newNick_A = UCS2ToUTF8StringAlloc(newNick);
	peerChangeNickA(peer, newNick_A, callback, param, blocking);
	gsifree(newNick_A);
}
#endif

void peerStayInRoom
(
	PEER peer,
	RoomType roomType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(roomType == TitleRoom);
	if(roomType != TitleRoom)
		return;

	if(!connection->title[0])
		return;

	connection->stayInTitleRoom = PEERTrue;
}

void peerSetQuietMode
(
	PEER peer,
	PEERBool quiet
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	chatSetQuietMode(connection->chat, (CHATBool)quiet);
}

void peerSetAwayModeA
(
	PEER peer,
	const char * reason
)
{
	char buffer[PI_AWAY_MAX_LEN + 6];
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!reason)
		reason = "";

	connection->away = (PEERBool)(reason[0] != '\0');
	strzcpy(connection->awayReason, reason, PI_AWAY_MAX_LEN);

	piSetLocalFlags(peer);

	sprintf(buffer, "AWAY :%s", connection->awayReason);
	chatSendRawA(connection->chat, buffer);
}
#ifdef GSI_UNICODE
void peerSetAwayModeW
(
	PEER peer,
	const unsigned short* reason
)
{
	char* reason_A = UCS2ToUTF8StringAlloc(reason);
	peerSetAwayModeA(peer, reason_A);
	gsifree(reason_A);
}
#endif

void peerParseQueryA
(
	PEER peer,
	char * query,
	int len,
	struct sockaddr * sender
)
{
	PEER_CONNECTION;

	assert(query);
	assert(sender);

	if(connection->queryReporting)
		qr2_parse_queryA(connection->queryReporting, query, len, sender);
	else if(connection->autoMatchReporting)
		qr2_parse_queryA(connection->autoMatchReporting, query, len, sender);
}
void peerAuthenticateCDKeyA
(
	PEER peer,
	const char * cdkey,
	peerAuthenticateCDKeyCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;
	
	assert(callback);

	if(!piNewAuthenticateCDKeyOperation(peer, cdkey, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddAuthenticateCDKeyCallback(peer, 0, "Error starting CD Key check", callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerAuthenticateCDKeyW
(
	PEER peer,
	const unsigned short* cdkey,
	peerAuthenticateCDKeyCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* cdkey_A = UCS2ToUTF8StringAlloc(cdkey);
	peerAuthenticateCDKeyA(peer, cdkey_A, callback, param, blocking);
	gsifree(cdkey_A);
}
#endif

void peerSendNatNegotiateCookie
(
	PEER peer,
	unsigned int ip,
	unsigned short port,
	int cookie
)
{
	piSendNatNegotiateCookie(peer, ip, port, cookie);
}

void peerSendMessageToServer
(
	PEER peer,
	unsigned int ip,
	unsigned short port,
	const char * data,
	int len
)
{
	piSendMessageToServer(peer, ip, port, data, len);
}

void peerAlwaysGetPlayerInfo
(
	PEER peer,
	PEERBool always
)
{
	PEER_CONNECTION;

	connection->alwaysRequestPlayerInfo = always;
}

void peerJoinTitleRoomA
(
	PEER peer,
	const char password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	PEERJoinResult result = PEERJoinFailed;
	char buffer[PI_ROOM_MAX_LEN];

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;
	
	assert(callback);

	if(!password)
		password = "";

	if(!connection->title[0])
	{
		success = PEERFalse;
		result = PEERNoTitleSet;
	}

	if(success && !connection->connected)
	{
		success = PEERFalse;
		result = PEERNoConnection;
	}

	assert(!connection->enteringRoom[TitleRoom] && !connection->inRoom[TitleRoom]);
	if((success && connection->enteringRoom[TitleRoom]) || connection->inRoom[TitleRoom])
	{
		success = PEERFalse;
		result = PEERAlreadyInRoom;
	}

	assert(!peerIsAutoMatching(peer));
	if(success && peerIsAutoMatching(peer))
	{
		success = PEERFalse;
		result = PEERAutoMatching;
	}

	if(success)
	{
		if(connection->titleRoomChannel[0])
			strcpy(buffer, connection->titleRoomChannel);
		else
			piMangleTitleRoom(buffer, connection->title);
	}

	if(success && !piNewJoinRoomOperation(peer, TitleRoom, buffer, password, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddJoinRoomCallback(peer, PEERFalse, result, TitleRoom, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerJoinTitleRoomW
(
	PEER peer,
	const unsigned short password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* password_A = NULL;
	if (password != NULL)
		password_A = UCS2ToUTF8StringAlloc(password);
	peerJoinTitleRoomA(peer, password_A, callback, param, blocking);
	gsifree(password_A);
}
#endif

void peerJoinGroupRoom
(
	PEER peer,
	int groupID,
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	PEERJoinResult result = PEERJoinFailed;
	char room[PI_ROOM_MAX_LEN];

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(callback);

	if(!connection->title[0])
	{
		success = PEERFalse;
		result = PEERNoTitleSet;
	}

	if(success && !connection->connected)
	{
		success = PEERFalse;
		result = PEERNoConnection;
	}

	assert(!peerIsAutoMatching(peer));
	if(success && peerIsAutoMatching(peer))
	{
		success = PEERFalse;
		result = PEERAutoMatching;
	}

	assert(groupID);
	if(success && !groupID)
		success = PEERFalse;

	if(success && (connection->enteringRoom[GroupRoom] || connection->inRoom[GroupRoom]))
	{
		success = PEERFalse;
		result = PEERAlreadyInRoom;
	}

	piMangleGroupRoom(room, groupID);

	connection->groupID = groupID;

	if(success && !piNewJoinRoomOperation(peer, GroupRoom, room, NULL, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddJoinRoomCallback(peer, PEERFalse, result, GroupRoom, callback, param, opID);

	PI_DO_BLOCKING;
}

void peerSetGroupID
(
	PEER peer,
	int groupID
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	connection->groupID = groupID;
}

int peerGetGroupID
(
	PEER peer
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return 0;

	if(!connection->connected)
		return 0;

	return connection->groupID;
}

static void piJoinStagingRoom
(
	PEER peer,
	SBServer server,
	const char * channel,
	const char password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	unsigned int publicIP      = 0;
	unsigned int privateIP     = 0;
	unsigned short privatePort = 0;
	char room[PI_ROOM_MAX_LEN];
	PEERBool success = PEERTrue;
	PEERJoinResult result = PEERJoinFailed;

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(callback);

	if(!password)
		password = "";

	if(!connection->title[0])
	{
		success = PEERFalse;
		result = PEERNoTitleSet;
	}

	if(success && !connection->connected)
	{
		success = PEERFalse;
		result = PEERNoConnection;
	}

	if(success && (connection->enteringRoom[StagingRoom] || connection->inRoom[StagingRoom]))
	{
		success = PEERFalse;
		result = PEERAlreadyInRoom;
	}

	assert(!peerIsAutoMatching(peer));
	if(success && peerIsAutoMatching(peer))
	{
		success = PEERFalse;
		result = PEERAutoMatching;
	}

	if(success && server)
	{
		publicIP = SBServerGetPublicInetAddress(server);
		privateIP = SBServerGetPrivateInetAddress(server);
		if(SBServerHasPrivateAddress(server))
			privatePort = SBServerGetPrivateQueryPort(server);
		else
			privatePort = SBServerGetPublicQueryPort(server);

		if(!publicIP)
			success = PEERFalse;
	}

	if(success && !server)
	{
		assert(channel);
		assert(channel[0]);
		if(!channel || !channel[0])
			success = PEERFalse;
	}

	if(success)
		piStopHosting(peer, PEERTrue);

	if(success && server)
		piMangleStagingRoom(room, connection->title, publicIP, privateIP, privatePort);

	if(success	&& !piNewJoinRoomOperation(peer, StagingRoom, server?room:channel, password, callback, param, opID))
		success = PEERFalse;

	if(success && server)
		connection->hostServer = piSBCloneServer(server);

	if(!success)
		piAddJoinRoomCallback(peer, PEERFalse, result, StagingRoom, callback, param, opID);

	PI_DO_BLOCKING;
}

void peerJoinStagingRoomA
(
	PEER peer,
	SBServer server,
	const char password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	piJoinStagingRoom(peer, server, NULL, password, callback, param, blocking);
}
#ifdef GSI_UNICODE
void peerJoinStagingRoomW
(
	PEER peer,
	SBServer server,
	const unsigned short password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* password_A = UCS2ToUTF8StringAlloc(password);
	peerJoinStagingRoomA(peer, server, password_A, callback, param, blocking);
	gsifree(password_A);
}
#endif

void peerJoinStagingRoomByChannelA
(
	PEER peer,
	const char * channel,
	const char password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	piJoinStagingRoom(peer, NULL, channel, password, callback, param, blocking);
}
#ifdef GSI_UNICODE
void peerJoinStagingRoomByChannelW
(
	PEER peer,
	const unsigned short * channel,
	const unsigned short password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* password_A = UCS2ToUTF8StringAlloc(password);
	peerJoinStagingRoomByChannelA(peer, channel_A, password_A, callback, param, blocking);
	gsifree(password_A);
	gsifree(channel_A);
}
#endif

void peerCreateStagingRoomA
(
	PEER peer,
	const char * name,
	int maxPlayers,
	const char password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	peerCreateStagingRoomWithSocketA(peer, name, maxPlayers, password, INVALID_SOCKET, 0, callback, param, blocking);
}
#ifdef GSI_UNICODE
void peerCreateStagingRoomW
(
	PEER peer,
	const unsigned short * name,
	int maxPlayers,
	const unsigned short password[PEER_PASSWORD_LEN],
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* name_A = UCS2ToUTF8StringAlloc(name);
	char* password_A = UCS2ToUTF8StringAlloc(password);
	peerCreateStagingRoomA(peer, name_A, maxPlayers, password_A, callback, param, blocking);
	gsifree(password_A);
	gsifree(name_A);
}
#endif

void peerCreateStagingRoomWithSocketA
(
	PEER peer,
	const char * name,
	int maxPlayers,
	const char password[PEER_PASSWORD_LEN],
	SOCKET socket,
	unsigned short port,
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	PEERJoinResult result = PEERJoinFailed;
	PI_OP_ID;

	PEER_CONNECTION;

	assert(name);
	assert(connection->title[0]);
	assert(callback);
	assert(maxPlayers >= 0);

	if(!password)
		password = "";

	if(!connection->title[0])
	{
		success = PEERFalse;
		result = PEERNoTitleSet;
	}

	if(success && !connection->connected)
	{
		success = PEERFalse;
		result = PEERNoConnection;
	}

	if(success && (connection->enteringRoom[StagingRoom] || connection->inRoom[StagingRoom]))
	{
		success = PEERFalse;
		result = PEERAlreadyInRoom;
	}

	assert(!peerIsAutoMatching(peer));
	if(success && peerIsAutoMatching(peer))
	{
		success = PEERFalse;
		result = PEERAutoMatching;
	}

	if(success)
		piStopHosting(peer, PEERTrue);

	if(success && !piNewCreateStagingRoomOperation(peer, name, password, maxPlayers, socket, port, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddJoinRoomCallback(peer, PEERFalse, result, StagingRoom, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerCreateStagingRoomWithSocketW
(
	PEER peer,
	const unsigned short * name,
	int maxPlayers,
	const unsigned short password[PEER_PASSWORD_LEN],
	SOCKET socket,
	unsigned short port,
	peerJoinRoomCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* name_A = UCS2ToUTF8StringAlloc(name);
	char* password_A = UCS2ToUTF8StringAlloc(password);
	peerCreateStagingRoomWithSocketA(peer, name_A, maxPlayers, password_A, socket, port, callback, param, blocking);
	gsifree(password_A);
	gsifree(name_A);
}
#endif


void peerLeaveRoomA
(
	PEER peer,
	RoomType roomType,
	const char * reason
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	piLeaveRoom(peer, roomType, reason);

	if((roomType == StagingRoom) && peerIsAutoMatching(peer))
	{
		piSetAutoMatchStatus(peer, PEERSearching);
	}
}
#ifdef GSI_UNICODE
void peerLeaveRoomW
(
	PEER peer,
	RoomType roomType,
	const unsigned short* reason
)
{
	if (reason != NULL)
	{
		char* reason_A = UCS2ToUTF8StringAlloc(reason);
		peerLeaveRoomA(peer, roomType, reason_A);
		gsifree(reason_A);
	}
	else
		peerLeaveRoomA(peer, roomType, NULL);
}
#endif

void peerListGroupRoomsA
(
	PEER peer,
	const char * fields,
	peerListGroupRoomsCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	PI_OP_ID;
	PEER_CONNECTION;

	assert(callback);

	if(!connection->title[0])
		success = PEERFalse;

	if(!fields)
		fields = "";

	if(success && !piNewListGroupRoomsOperation(peer, fields, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddListGroupRoomsCallback(peer, PEERFalse, 0, NULL, NULL, 0, 0, 0, 0, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerListGroupRoomsW
(
	PEER peer,
	const unsigned short * fields,
	peerListGroupRoomsCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* fields_A = UCS2ToUTF8StringAlloc(fields);
	peerListGroupRoomsA(peer, fields_A, callback, param, blocking);
	gsifree(fields_A);
}
#endif

void peerStartListingGamesA
(
	PEER peer,
	const unsigned char * fields,
	int numFields,
	const char * filter,
	peerListingGamesCallback callback,
	void * param
)
{
	PEERBool success;

	PEER_CONNECTION;

	assert(callback);

	if(!connection->title[0])
		return;

	if(filter && !filter[0])
		filter = NULL;

	if(!fields || (numFields <= 0))
		numFields = 0;

	connection->gameListCallback = callback;
	connection->gameListParam = param;

	success = piSBStartListingGames(peer, fields, numFields, filter);

	if(!success)
		piAddListingGamesCallback(peer, PEERFalse, NULL, 0);
}
#ifdef GSI_UNICODE
void peerStartListingGamesW
(
	PEER peer,
	const unsigned char * fields,
	int numFields,
	const unsigned short * filter,
	peerListingGamesCallback callback,
	void * param
)
{
	char* filter_A = UCS2ToUTF8StringAlloc(filter);
	peerStartListingGamesA(peer, fields, numFields, filter_A, callback, param);
	gsifree(filter_A);
}
#endif

void peerStopListingGames
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(!connection->title[0])
		return;

	piSBStopListingGames(peer);
}

void peerUpdateGame
(
	PEER peer,
	SBServer server,
	PEERBool fullUpdate
)
{
	PEER_CONNECTION;

	assert(server);

	if(!connection->title[0])
		return;

	piSBUpdateGame(peer, server, fullUpdate, PEERFalse, PEERFalse);
}

void peerUpdateGameByMaster(PEER peer, SBServer server, PEERBool fullUpdate)
{
	PEER_CONNECTION;

	assert(server);

	if(!connection->title[0])
		return;

	piSBUpdateGame(peer, server, fullUpdate, PEERTrue, PEERFalse);
}

void peerUpdateGamePing(PEER peer, SBServer server)
{
	PEER_CONNECTION;

	assert(server);

	if(!connection->title[0])
		return;

	piSBUpdateGame(peer, server, PEERFalse, PEERFalse, PEERTrue);
}

void peerMessageRoomA
(
	PEER peer,
	RoomType roomType,
	const char * message,
	MessageType messageType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);
	ASSERT_MESSAGETYPE(messageType);

	if(!message || !message[0])
		return;

	assert(IN_ROOM);
	if(!IN_ROOM)
		return;

	chatSendChannelMessageA(connection->chat, ROOM, message, (int)messageType);
}
#ifdef GSI_UNICODE
void peerMessageRoomW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * message,
	MessageType messageType
)
{
	char* message_A = UCS2ToUTF8StringAlloc(message);
	peerMessageRoomA(peer, roomType, message_A, messageType);
	gsifree(message_A);
}
#endif

void peerUTMRoomA
(
	PEER peer,
	RoomType roomType,
	const char * command,
	const char * parameters,
	PEERBool authenticate
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);

	assert(IN_ROOM);
	if(!IN_ROOM)
		return;

	piSendChannelUTM(peer, ROOM, command, parameters, authenticate);
}
#ifdef GSI_UNICODE
void peerUTMRoomW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * command,
	const unsigned short * parameters,
	PEERBool authenticate
)
{
	char* command_A = UCS2ToUTF8StringAlloc(command);
	char* parameters_A = UCS2ToUTF8StringAlloc(parameters);
	peerUTMRoomA(peer, roomType, command_A, parameters_A, authenticate);
	gsifree(parameters_A);
	gsifree(command_A);
}
#endif

void peerSetPasswordA
(
	PEER peer,
	RoomType roomType,
	const char password[PEER_PASSWORD_LEN]
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);

	assert(roomType == StagingRoom);
	if(roomType != StagingRoom)
		return;

	if(!password)
		password = "";

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	assert(connection->hosting);
	if(!connection->hosting)
		return;

	if(password[0])
		chatSetChannelPasswordA(connection->chat, ROOM, CHATTrue, password);
	else
		chatSetChannelPasswordA(connection->chat, ROOM, CHATFalse, "x");

	connection->passwordedRoom = password[0]?PEERTrue:PEERFalse;

	piSendStateChanged(peer);
}
#ifdef GSI_UNICODE
void peerSetPasswordW
(
	PEER peer,
	RoomType roomType,
	const unsigned short password[PEER_PASSWORD_LEN]
)
{
	char* password_A = UCS2ToUTF8StringAlloc(password);
	peerSetPasswordA(peer, roomType, password_A);
	gsifree(password_A);
}
#endif

void peerSetRoomNameA
(
	PEER peer,
	RoomType roomType,
	const char * name
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);

	assert(roomType == StagingRoom);

	if(!name)
		name = "";

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	assert(connection->hosting);
	if(!connection->hosting)
		return;

	chatSetChannelTopicA(connection->chat, ROOM, name);
}
#ifdef GSI_UNICODE
void peerSetRoomNameW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * name
)
{
	char* name_A = UCS2ToUTF8StringAlloc(name);
	peerSetRoomNameA(peer, roomType, name_A);
	gsifree(name_A);
}
#endif

const char * peerGetRoomNameA
(
	PEER peer,
	RoomType roomType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);
	assert(IN_ROOM);
	if(!IN_ROOM)
		return NULL;

	return NAME;
}
#ifdef GSI_UNICODE
const unsigned short * peerGetRoomNameW
(
	PEER peer,
	RoomType roomType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);
	assert(IN_ROOM);
	if(!IN_ROOM)
		return NULL;

	return NAME_W;
}
#endif

const char * peerGetRoomChannelA
(
	PEER peer,
	RoomType roomType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);
	assert(IN_ROOM || ENTERING_ROOM);
	if(!IN_ROOM && ! ENTERING_ROOM)
		return NULL;

	return ROOM;
}
#ifdef GSI_UNICODE
const unsigned short * peerGetRoomChannelW
(
	PEER peer,
	RoomType roomType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);
	assert(IN_ROOM || ENTERING_ROOM);
	if(!IN_ROOM && ! ENTERING_ROOM)
		return NULL;

	return ROOM_W;
}
#endif

PEERBool peerInRoom
(
	PEER peer,
	RoomType roomType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);

	return IN_ROOM;
}

void peerSetTitleRoomChannelA
(
	PEER peer,
	const char * channel
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	if(!channel)
		channel = "";

	strzcpy(connection->titleRoomChannel, channel, PI_ROOM_MAX_LEN);
}
#ifdef GSI_UNICODE
void peerSetTitleRoomChannelW
(
	PEER peer,
	const unsigned short * channel
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	peerSetTitleRoomChannelA(peer, channel_A);
	gsifree(channel_A);
}
#endif

SBServer peerGetHostServer(PEER peer)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	return connection->hostServer;
}

typedef struct piEnumPlayersData
{
	peerEnumPlayersCallback callback;
	void * param;
} piEnumPlayersData;

static void piEnumPlayersEnumRoomPlayersCallback
(
	PEER peer,
	RoomType roomType,
	piPlayer * player,
	int index,
	void *param
)
{
	piEnumPlayersData * data = (piEnumPlayersData *)param;
	const char * nick;

	int flags;

	if(player)
	{
		nick = player->nick;
		flags = player->flags[roomType];
	}
	else
	{
		nick = NULL;
		flags = 0;
	}

#ifndef GSI_UNICODE
	data->callback(peer, PEERTrue, roomType, index, nick, flags, data->param);
#else
	if (nick == NULL)
		data->callback(peer, PEERTrue, roomType, index, NULL, flags, data->param);
	else
	{
		unsigned short nick_W[512];
		UTF8ToUCS2String(nick, nick_W);
		data->callback(peer, PEERTrue, roomType, index, nick_W, flags, data->param);
	}
#endif
}

void peerEnumPlayers
(
	PEER peer,
	RoomType roomType,
	peerEnumPlayersCallback callback,
	void * param
)
{
	PEERBool success = PEERTrue;
	piEnumPlayersData data;

	PEER_CONNECTION;
	PEER_CONNECTED;
	
	assert(callback);
	ASSERT_ROOMTYPE(roomType);

	assert(IN_ROOM);
	if(success && !IN_ROOM)
		success = PEERFalse;

	if(!success)
	{
		callback(peer, PEERFalse, roomType, -1, NULL, PEERFalse, param);
		return;
	}

	data.callback = callback;
	data.param = param;
	piEnumRoomPlayers(peer, roomType, piEnumPlayersEnumRoomPlayersCallback, &data);
}

void peerMessagePlayerA
(
	PEER peer,
	const char * nick,
	const char * message,
	MessageType messageType
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_MESSAGETYPE(messageType);
	assert(nick);
	assert(nick[0]);

	if(!connection->connected)
		return;

	if(!message || !message[0])
		return;

	chatSendUserMessageA(connection->chat, nick, message, (int)messageType);
}
#ifdef GSI_UNICODE
void peerMessagePlayerW
(
	PEER peer,
	const unsigned short * nick,
	const unsigned short * message,
	MessageType messageType
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char* message_A = UCS2ToUTF8StringAlloc(message);
	peerMessagePlayerA(peer, nick_A, message_A, messageType);
	gsifree(nick_A);
	gsifree(message_A);
}
#endif

void peerUTMPlayerA
(
	PEER peer,
	const char * nick,
	const char * command,
	const char * parameters,
	PEERBool authenticate
)
{
	piSendPlayerUTM(peer, nick, command, parameters, authenticate);
}
#ifdef GSI_UNICODE
void peerUTMPlayerW
(
	PEER peer,
	const unsigned short * nick,
	const unsigned short * command,
	const unsigned short * parameters,
	PEERBool authenticate
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char* command_A = UCS2ToUTF8StringAlloc(command);
	char* parameters_A = UCS2ToUTF8StringAlloc(parameters);
	peerUTMPlayerA(peer, nick_A, command_A, parameters_A, authenticate);
	gsifree(parameters_A);
	gsifree(command_A);
	gsifree(nick_A);
}
#endif

void peerKickPlayerA
(
	PEER peer,
	RoomType roomType,
	const char * nick,
	const char * reason
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	ASSERT_ROOMTYPE(roomType);
	assert(IN_ROOM || ENTERING_ROOM);
	assert(nick);
	assert(nick[0]);

	if(!connection->connected)
		return;

	chatKickUserA(connection->chat, ROOM, nick, reason);
}
#ifdef GSI_UNICODE
void peerKickPlayerW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * nick,
	const unsigned short * reason
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char* reason_A = UCS2ToUTF8StringAlloc(reason);
	peerKickPlayerA(peer, roomType, nick_A, reason_A);
	gsifree(reason_A);
	gsifree(nick_A);
}
#endif

PEERBool peerGetPlayerPingA
(
	PEER peer,
	const char * nick,
	int * ping
)
{
	piPlayer * player;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(nick);
	assert(nick[0]);
	assert(ping);

	player = piGetPlayer(peer, nick);
	if(!player)
		return PEERFalse;

	if(player->local)
	{
		*ping = 0;
	}
	else
	{
		if(!player->numPings)
			return PEERFalse;

		*ping = player->pingAverage;
	}

	return PEERTrue;
}
#ifdef GSI_UNICODE
PEERBool peerGetPlayerPingW
(
	PEER peer,
	const unsigned short * nick,
	int * ping
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	PEERBool result = peerGetPlayerPingA(peer, nick_A, ping);
	gsifree(nick_A);
	return result;
}
#endif

PEERBool peerGetPlayersCrossPingA
(
	PEER peer,
	const char * nick1,
	const char * nick2,
	int * crossPing
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(nick1);
	assert(nick1[0]);
	assert(nick2);
	assert(nick2[0]);
	assert(crossPing);

	return piGetXping(peer, nick1, nick2, crossPing);
}
#ifdef GSI_UNICODE
PEERBool peerGetPlayersCrossPingW
(
	PEER peer,
	const unsigned short * nick1,
	const unsigned short * nick2,
	int * crossPing
)
{
	char* nick1_A = UCS2ToUTF8StringAlloc(nick1);
	char* nick2_A = UCS2ToUTF8StringAlloc(nick2);
	PEERBool result = peerGetPlayersCrossPingA(peer, nick1_A, nick2_A, crossPing);
	gsifree(nick2_A);
	gsifree(nick1_A);
	return result;
}
#endif

PEERBool peerPingPlayerA
(
	PEER peer,
	const char * nick
)
{
	piPlayer * player;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(nick);
	assert(nick[0]);

	player = piGetPlayer(peer, nick);
	if(!player)
		return PEERFalse;

	if(player->local)
		return PEERFalse;

	if(!player->gotIPAndProfileID)
		return PEERFalse;

	if(player->waitingForPing)
		return PEERTrue;

	player->mustPing = PEERTrue;

	if(!player->inPingRoom)
		player->pingOnce = PEERTrue;

	return PEERTrue;
}
#ifdef GSI_UNICODE
PEERBool peerPingPlayerW
(
	PEER peer,
	const unsigned short * nick
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	PEERBool result = peerPingPlayerA(peer, nick_A);
	gsifree(nick_A);
	return result;
}
#endif

PEERBool peerGetPlayerInfoNoWaitA
(
	PEER peer,
	const char * nick,
	unsigned int * IP,
	int * profileID
)
{
	piPlayer * player;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(nick);

	player = piGetPlayer(peer, nick);
	if(!player || !player->gotIPAndProfileID)
	{
		const char * info;
		unsigned int locIP;
		int locProfileID;

		if(chatGetBasicUserInfoNoWaitA(connection->chat, nick, &info, NULL) && piDemangleUser(info, &locIP, &locProfileID))
		{
			if(player)
				piSetPlayerIPAndProfileID(peer, nick, locIP, locProfileID);

			if(IP)
				*IP = locIP;
			if(profileID)
				*profileID = locProfileID;

			return PEERTrue;
		}
		return PEERFalse;
	}

	if(IP)
		*IP = player->IP;
	if(profileID)
		*profileID = player->profileID;

	return PEERTrue;
}
#ifdef GSI_UNICODE
PEERBool peerGetPlayerInfoNoWaitW
(
	PEER peer,
	const unsigned short * nick,
	unsigned int * IP,
	int * profileID
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	PEERBool result = peerGetPlayerInfoNoWaitA(peer, nick_A, IP, profileID);
	gsifree(nick_A);
	return result;
}
#endif

void peerGetPlayerInfoA
(
	PEER peer,
	const char * nick,
	peerGetPlayerInfoCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	piPlayer * player;
	PI_OP_ID;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(callback);
	assert(nick);
	assert(nick[0]);
	assert(callback);

	player = piGetPlayer(peer, nick);

	if(player && !player->gotIPAndProfileID)
	{
		const char * info;
		unsigned int IP;
		int profileID;

		if(chatGetBasicUserInfoNoWaitA(connection->chat, nick, &info, NULL) && piDemangleUser(info, &IP, &profileID))
		{
			piSetPlayerIPAndProfileID(peer, nick, IP, profileID);
		}
	}

	if(player && player->gotIPAndProfileID)
	{
		piAddGetPlayerInfoCallback(peer, PEERTrue, nick, player->IP, player->profileID, callback, param, opID);
	}
	else
	{
		if(!piNewGetPlayerInfoOperation(peer, nick, callback, param, opID))
			success = PEERFalse;
	}

	if(!success)
		piAddGetPlayerInfoCallback(peer, PEERFalse, nick, 0, 0, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerGetPlayerInfoW
(
	PEER peer,
	const unsigned short * nick,
	peerGetPlayerInfoCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	peerGetPlayerInfoA(peer, nick_A, callback, param, blocking);
	gsifree(nick_A);
}
#endif

void peerGetPlayerProfileIDA
(
	PEER peer,
	const char * nick,
	peerGetPlayerProfileIDCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	piPlayer * player;
	PI_OP_ID;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(callback);
	assert(nick);
	assert(nick[0]);
	assert(callback);

	player = piGetPlayer(peer, nick);

	if(player && !player->gotIPAndProfileID)
	{
		const char * info;
		unsigned int IP;
		int profileID;

		if(chatGetBasicUserInfoNoWaitA(connection->chat, nick, &info, NULL) && piDemangleUser(info, &IP, &profileID))
		{
			piSetPlayerIPAndProfileID(peer, nick, IP, profileID);
		}
	}

	if(player && player->gotIPAndProfileID)
	{
		piAddGetPlayerProfileIDCallback(peer, PEERTrue, nick, player->profileID, callback, param, opID);
	}
	else
	{
		if(!piNewGetProfileIDOperation(peer, nick, callback, param, opID))
			success = PEERFalse;
	}

	if(!success)
		piAddGetPlayerProfileIDCallback(peer, PEERFalse, nick, 0, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerGetPlayerProfileIDW
(
	PEER peer,
	const unsigned short * nick,
	peerGetPlayerProfileIDCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	peerGetPlayerProfileIDA(peer, nick_A, callback, param, blocking);
	gsifree(nick_A);
}
#endif

void peerGetPlayerIPA
(
	PEER peer,
	const char * nick,
	peerGetPlayerIPCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;
	piPlayer * player;
	PI_OP_ID;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(callback);
	assert(nick);
	assert(nick[0]);
	assert(callback);

	player = piGetPlayer(peer, nick);

	if(player && !player->gotIPAndProfileID)
	{
		const char * info;
		unsigned int IP;
		int profileID;

		if(chatGetBasicUserInfoNoWaitA(connection->chat, nick, &info, NULL) && piDemangleUser(info, &IP, &profileID))
		{
			piSetPlayerIPAndProfileID(peer, nick, IP, profileID);
		}
	}

	if(player && player->gotIPAndProfileID)
	{
		piAddGetPlayerIPCallback(peer, PEERTrue, nick, player->IP, callback, param, opID);
	}
	else
	{
		if(!piNewGetIPOperation(peer, nick, callback, param, opID))
			success = PEERFalse;
	}

	if(!success)
		piAddGetPlayerIPCallback(peer, PEERFalse, nick, 0, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerGetPlayerIPW
(
	PEER peer,
	const unsigned short * nick,
	peerGetPlayerIPCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	peerGetPlayerIPA(peer, nick_A, callback, param, blocking);
	gsifree(nick_A);
}
#endif

PEERBool peerIsPlayerHostA
(
	PEER peer,
	const char * nick,
	RoomType roomType
)
{
	piPlayer * player;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(IN_ROOM);
	if(!IN_ROOM)
		return PEERFalse;

	player = piGetPlayer(peer, nick);
	if(!player)
		return PEERFalse;

	if(player->local)
		return connection->hosting;

	return piIsPlayerHost(player);
}
#ifdef GSI_UNICODE
PEERBool peerIsPlayerHostW
(
	PEER peer,
	const unsigned short * nick,
	RoomType roomType
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	PEERBool result = peerIsPlayerHostA(peer, nick_A, roomType);
	gsifree(nick_A);
	return result;
}
#endif

PEERBool peerGetPlayerFlagsA
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	int * flags
)
{
	piPlayer * player;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(flags);
	if(!flags)
		return PEERFalse;

	assert(IN_ROOM);
	if(!IN_ROOM)
		return PEERFalse;

	player = piGetPlayer(peer, nick);
	if(!player)
		return PEERFalse;

	if(!player->inRoom[roomType])
		return PEERFalse;

	*flags = player->flags[roomType];

	return PEERTrue;
}
#ifdef GSI_UNICODE
PEERBool peerGetPlayerFlagsW
(
	PEER peer,
	const unsigned short * nick,
	RoomType roomType,
	int * flags
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	PEERBool result = peerGetPlayerFlagsA(peer, nick_A, roomType, flags);
	gsifree(nick_A);
	return result;
}
#endif

void peerSetReady
(
	PEER peer,
	PEERBool ready
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	assert(connection->inRoom[StagingRoom]);
	if(!connection->inRoom[StagingRoom])
		return;

	if(connection->ready == ready)
		return;

	connection->ready = ready;

	piSetLocalFlags(peer);

#if 1
	{
	char buffer[32];
	strcpy(buffer, "@@@NFO \\$flags$\\");
	if(ready)
		strcat(buffer, "r");
	strcat(buffer, "X\\");  
	peerMessageRoomA(peer, StagingRoom, buffer, NormalMessage);
	}
#endif
}

PEERBool peerGetReadyA
(
	PEER peer,
	const char * nick,
	PEERBool * ready
)
{
	piPlayer * player;

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(nick);
	assert(nick[0]);
	assert(ready);

	if(!connection->title[0])
		return PEERFalse;

	if(!connection->connected)
		return PEERFalse;

	assert(connection->inRoom[StagingRoom]);
	if(!connection->inRoom[StagingRoom])
		return PEERFalse;

	player = piGetPlayer(peer, nick);
	if(!player || !player->inRoom[StagingRoom])
		return PEERFalse;

	*ready = (PEERBool)((player->flags[StagingRoom] & PEER_FLAG_READY) != 0);

	return PEERTrue;
}
#ifdef GSI_UNICODE
PEERBool peerGetReadyW
(
	PEER peer,
	const unsigned short * nick,
	PEERBool * ready
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	PEERBool result = peerGetReadyA(peer, nick_A, ready);
	gsifree(nick_A);
	return result;
}
#endif

static void piAreAllReadyEnumRoomPlayersCallback
(
	PEER peer,
	RoomType roomType,
	piPlayer * player,
	int index,
	void *param
)
{
	if(player)
	{
		PEERBool * allReadyPtr = (PEERBool *)param;

		if(!(player->flags[StagingRoom] & PEER_FLAG_READY))
			*allReadyPtr = PEERFalse;
	}
	
	GSI_UNUSED(peer);
	GSI_UNUSED(roomType);
	GSI_UNUSED(index);
}

PEERBool peerAreAllReady
(
	PEER peer
)
{
	PEERBool allReady;

	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return PEERFalse;

	if(!connection->connected)
		return PEERFalse;

	assert(connection->inRoom[StagingRoom]);
	if(!connection->inRoom[StagingRoom])
		return PEERFalse;

	allReady = PEERTrue;
	piEnumRoomPlayers(peer, StagingRoom, piAreAllReadyEnumRoomPlayersCallback, &allReady);

	return allReady;
}

void peerStartGameA
(
	PEER peer,
	const char * message,
	int reportingOptions
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	assert(connection->inRoom[StagingRoom]);
	if(!connection->inRoom[StagingRoom])
		return;

	assert(connection->hosting);
	if(!connection->hosting)
		return;

	if(!message)
		message = "";

	piSendChannelUTM(peer, connection->rooms[StagingRoom], PI_UTM_LAUNCH, message, PEERFalse);

#if 1
	{
	char buffer[32];
	IN_ADDR addr;
	addr.s_addr = connection->publicIP;
	sprintf(buffer, "@@@GML %s/OLD", inet_ntoa(addr));
	peerMessageRoomA(peer, StagingRoom, buffer, NormalMessage);
	}
#endif

	connection->playing = PEERTrue;

	piSetLocalFlags(peer);

	if(peerIsAutoMatching(peer))
	{
		piSetAutoMatchStatus(peer, PEERComplete);
	}
	else if(connection->queryReporting)
	{
		if(reportingOptions & PEER_STOP_REPORTING)
		{
			piStopReporting(peer);
		}
		else
		{
			connection->reportingOptions = reportingOptions;

			piSendStateChanged(peer);
		}
	}
}
#ifdef GSI_UNICODE
void peerStartGameW
(
	PEER peer,
	const unsigned short * message,
	int reportingOptions
)
{
	char* message_A = UCS2ToUTF8StringAlloc(message);
	peerStartGameA(peer, message_A, reportingOptions);
	gsifree(message_A);
}
#endif

PEERBool peerStartReporting
(
	PEER peer
)
{
	return peerStartReportingWithSocket(peer, INVALID_SOCKET, 0);
}

PEERBool peerStartReportingWithSocket
(
	PEER peer,
	SOCKET socket,
	unsigned short port
)
{
	PEER_CONNECTION;

	if(!connection->title[0])
		return PEERFalse;

	if(!piStartReporting(peer, socket, port))
		return PEERFalse;

	return PEERTrue;
}

void peerStartPlaying
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(!connection->title[0])
		return;

	connection->playing = PEERTrue;

	piSetLocalFlags(peer);
}

PEERBool peerIsPlaying
(
	PEER peer
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return PEERFalse;

	if(!connection->connected)
		return PEERFalse;

	return connection->playing;
}

void peerStopGame
(
	PEER peer
)
{
	PEER_CONNECTION;

	connection->playing = PEERFalse;

	piSetLocalFlags(peer);

	if(connection->queryReporting)
	{
		if(connection->inRoom[StagingRoom])
			piSendStateChanged(peer);
		else
			piStopReporting(peer);
	}
}

void peerStateChanged
(
	PEER peer
)
{
	PEER_CONNECTION;

	assert(connection->queryReporting);

	piSendStateChanged(peer);
}

void piSendChannelUTM
(
	PEER peer,
	const char * channel,
	const char * command,
	const char * parameters,
	PEERBool authenticate
)
{
	char buffer[512];

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(channel && channel[0]);
	assert(command && command[0]);
	assert(parameters);

	if(!connection->connected)
		return;

	if(!channel || !channel[0])
		return;

	if(!command || !command[0])
		return;

	if(!parameters)
		parameters = "";

	if((strlen(command) + strlen(parameters) + 5) > sizeof(buffer))
		return;

	sprintf(buffer, "%s %s", command, parameters);

	chatSendChannelMessageA(connection->chat, channel, buffer, authenticate?CHAT_ATM:CHAT_UTM);
}

void piSendPlayerUTM
(
	PEER peer,
	const char * nick,
	const char * command,
	const char * parameters,
	PEERBool authenticate
)
{
	char buffer[512];

	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(nick && nick[0]);
	assert(command && command[0]);
	assert(parameters);

	if(!connection->connected)
		return;

	if(!nick || !nick[0])
		return;

	if(!command || !command[0])
		return;

	if(!parameters)
		parameters = "";

	if((strlen(command) + strlen(parameters) + 5) > sizeof(buffer))
		return;

	sprintf(buffer, "%s %s", command, parameters);

	chatSendUserMessageA(connection->chat, nick, buffer, authenticate?CHAT_ATM:CHAT_UTM);
}

void peerSetGlobalKeysA
(
	PEER peer,
	int num,
	const char ** keys,
	const char ** values
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(keys);
	assert(values);
	assert(num > 0);

	if(!connection->connected)
		return;

	chatSetGlobalKeysA(connection->chat, num, keys, values);
}
#ifdef GSI_UNICODE
void peerSetGlobalKeysW
(
	PEER peer,
	int num,
	const unsigned short ** keys,
	const unsigned short ** values
)
{
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	char** values_A = UCS2ToUTF8StringArrayAlloc(values, num);
	int i;
	peerSetGlobalKeysA(peer, num, (const char**)keys_A, (const char**)values_A);
	for (i=0; i<num; i++)
	{
		gsifree(keys_A[i]);
		gsifree(values_A[i]);
	}
	gsifree(keys_A);
	gsifree(values_A);
}
#endif

void peerGetPlayerGlobalKeysA
(
	PEER peer,
	const char * nick,
	int num,
	const char ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->connected)
		return;

	if(!nick || !nick[0])
		nick = connection->nick;

	assert(callback);

	if(!piNewGetGlobalKeysOperation(peer, nick, num, keys, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddGetGlobalKeysCallback(peer, PEERFalse, nick, 0, NULL, NULL, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerGetPlayerGlobalKeysW
(
	PEER peer,
	const unsigned short * nick,
	int num,
	const unsigned short ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	PEERBool blocking
)
{
	char*  nick_A = UCS2ToUTF8StringAlloc(nick);
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	int i;
	peerGetPlayerGlobalKeysA(peer, nick_A, num, (const char**)keys_A, callback, param, blocking);
	gsifree(nick_A);
	for (i=0; i<num; i++)
		gsifree(keys_A[i]);
	gsifree(keys_A);
}
#endif

void peerGetRoomGlobalKeysA
(
	PEER peer,
	RoomType roomType,
	int num,
	const char ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->connected)
		return;

	ASSERT_ROOMTYPE(roomType);
	assert(callback);

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	if(!piNewGetGlobalKeysOperation(peer, ROOM, num, keys, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddGetGlobalKeysCallback(peer, PEERFalse, "", 0, NULL, NULL, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerGetRoomGlobalKeysW
(
	PEER peer,
	RoomType roomType,
	int num,
	const unsigned short ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	PEERBool blocking
)
{
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	int i;
	peerGetRoomGlobalKeysA(peer, roomType, num, (const char**)keys_A, callback, param, blocking);
	for (i=0; i<num; i++)
		gsifree(keys_A[i]);
	gsifree(keys_A);
}
#endif

void peerSetRoomKeysA
(
	PEER peer,
	RoomType roomType,
	const char * nick,
	int num,
	const char ** keys,
	const char ** values
)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(keys);
	assert(values);
	assert(num > 0);
	ASSERT_ROOMTYPE(roomType);

	if(!connection->connected)
		return;

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	chatSetChannelKeysA(connection->chat, ROOM, nick, num, keys, values);
}
#ifdef GSI_UNICODE
void peerSetRoomKeysW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * nick,
	int num,
	const unsigned short ** keys,
	const unsigned short ** values
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	char** values_A = UCS2ToUTF8StringArrayAlloc(values, num);
	int i;
	peerSetRoomKeysA(peer, roomType, nick_A, num, (const char**)keys_A, (const char**)values_A);
	gsifree(nick_A);
	for (i=0; i<num; i++)
	{
		gsifree(keys_A[i]);
		gsifree(values_A[i]);
	}
	gsifree(keys_A);
	gsifree(values_A);
}
#endif

void peerGetRoomKeysA
(
	PEER peer,
	RoomType roomType,
	const char * nick,
	int num,
	const char ** keys,
	peerGetRoomKeysCallback callback,
	void * param,
	PEERBool blocking
)
{
	PEERBool success = PEERTrue;

	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->connected)
		return;

	ASSERT_ROOMTYPE(roomType);
	assert(callback);

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	if(!piNewGetRoomKeysOperation(peer, roomType, nick, num, keys, callback, param, opID))
		success = PEERFalse;

	if(!success)
		piAddGetRoomKeysCallback(peer, PEERFalse, roomType, nick, 0, NULL, NULL, callback, param, opID);

	PI_DO_BLOCKING;
}
#ifdef GSI_UNICODE
void peerGetRoomKeysW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * nick,
	int num,
	const unsigned short ** keys,
	peerGetRoomKeysCallback callback,
	void * param,
	PEERBool blocking
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	int i;
	peerGetRoomKeysA(peer, roomType, nick_A, num, (const char**)keys_A, callback, param, blocking);
	gsifree(nick_A);
	for (i=0; i<num; i++)
		gsifree(keys_A[i]);
	gsifree(keys_A);
}
#endif

void peerSetGlobalWatchKeysA
(
	PEER peer,
	RoomType roomType,
	int num,
	const char ** keys,
	PEERBool addKeys
)
{
	piSetGlobalWatchKeys(peer, roomType, num, keys, addKeys);
}
#ifdef GSI_UNICODE
void peerSetGlobalWatchKeysW
(
	PEER peer,
	RoomType roomType,
	int num,
	const unsigned short ** keys,
	PEERBool addKeys
)
{
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	int i;
	peerSetGlobalWatchKeysA(peer, roomType, num, (const char**)keys_A, addKeys);
	for (i=0; i<num; i++)
		gsifree(keys_A[i]);
	gsifree(keys_A);
}
#endif

void peerSetRoomWatchKeysA
(
	PEER peer,
	RoomType roomType,
	int num,
	const char ** keys,
	PEERBool addKeys
)
{
	piSetRoomWatchKeys(peer, roomType, num, keys, addKeys);
}
#ifdef GSI_UNICODE
void peerSetRoomWatchKeysW
(
	PEER peer,
	RoomType roomType,
	int num,
	const unsigned short ** keys,
	PEERBool addKeys
)
{
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	int i;
	peerSetGlobalWatchKeysA(peer, roomType, num, (const char**)keys_A, addKeys);
	for (i=0; i<num; i++)
		gsifree(keys_A[i]);
	gsifree(keys_A);
}
#endif

const char * peerGetGlobalWatchKeyA
(
	PEER peer,
	const char * nick,
	const char * key
)
{
	return piGetGlobalWatchKeyA(peer, nick, key);
}
#ifdef GSI_UNICODE
const unsigned short * peerGetGlobalWatchKeyW
(
	PEER peer,
	const unsigned short  * nick,
	const unsigned short  * key
)
{
	return piGetGlobalWatchKeyW(peer, nick, key);
}
#endif

const char * peerGetRoomWatchKeyA
(
	PEER peer,
	RoomType roomType,
	const char * nick,
	const char * key
)
{
	return piGetRoomWatchKeyA(peer, roomType, nick, key);
}
#ifdef GSI_UNICODE
const unsigned short * peerGetRoomWatchKeyW
(
	PEER peer,
	RoomType roomType,
	const unsigned short * nick,
	const unsigned short * key
)
{
	return piGetRoomWatchKeyW(peer, roomType, nick, key);
}
#endif

void peerStartAutoMatchA
(
	PEER peer,
	int maxPlayers,
	const char * filter,
	peerAutoMatchStatusCallback statusCallback,
	peerAutoMatchRateCallback rateCallback,
	void * param,
	PEERBool blocking
)
{
	peerStartAutoMatchWithSocketA(peer, maxPlayers, filter, INVALID_SOCKET, 0, statusCallback, rateCallback, param, blocking);
}
#ifdef GSI_UNICODE
void peerStartAutoMatchW
(
	PEER peer,
	int maxPlayers,
	const unsigned short * filter,
	peerAutoMatchStatusCallback statusCallback,
	peerAutoMatchRateCallback rateCallback,
	void * param,
	PEERBool blocking
)
{
	char* filter_A = UCS2ToUTF8StringAlloc(filter);
	peerStartAutoMatchA(peer, maxPlayers, filter_A, statusCallback, rateCallback, param, blocking);
	gsifree(filter_A);
}
#endif

void peerStartAutoMatchWithSocketA
(
	PEER peer,
	int maxPlayers,
	const char * filter,
	SOCKET socket,
	unsigned short port,
	peerAutoMatchStatusCallback statusCallback,
	peerAutoMatchRateCallback rateCallback,
	void * param,
	PEERBool blocking
)
{
	PI_OP_ID;
	PEER_CONNECTION;
	PEER_CONNECTED;

	assert(maxPlayers >= 2);

	if(!filter)
		filter = "";

	if(!connection->title[0])
		goto failed;

	if(!connection->connected)
		goto failed;

	assert(!peerIsAutoMatching(peer));
	if(peerIsAutoMatching(peer))
		goto failed;

	if(connection->enteringRoom[StagingRoom])
		piLeaveRoom(peer, StagingRoom, "");

	piStopReporting(peer);

	piSBStopListingGames(peer);

	connection->maxPlayers = maxPlayers;
	connection->autoMatchFilter = goastrdup(filter);
	if(!connection->autoMatchFilter)
		goto failed;

	connection->autoMatchStatus = PEERFailed;

	connection->autoMatchSBFailed = PEERFalse;
	connection->autoMatchQRFailed = PEERFalse;

	if(!piNewAutoMatchOperation(peer, socket, port, statusCallback, rateCallback, param, opID))
	{
		gsifree(connection->autoMatchFilter);
		goto failed;
	}
	
	PI_DO_BLOCKING;

	return;

failed:
	connection->autoMatchStatus = PEERFailed;
	piAddAutoMatchStatusCallback(peer);
}
#ifdef GSI_UNICODE
void peerStartAutoMatchWithSocketW
(
	PEER peer,
	int maxPlayers,
	const unsigned short * filter,
	SOCKET socket,
	unsigned short port,
	peerAutoMatchStatusCallback statusCallback,
	peerAutoMatchRateCallback rateCallback,
	void * param,
	PEERBool blocking
)
{
	char* filter_A = UCS2ToUTF8StringAlloc(filter);
	peerStartAutoMatchWithSocketA(peer, maxPlayers, filter_A, socket, port, statusCallback, rateCallback, param, blocking);
	gsifree(filter_A);
}
#endif

void peerStopAutoMatch(PEER peer)
{
	PEER_CONNECTION;
	PEER_CONNECTED;

	if(!connection->title[0])
		return;

	if(!connection->connected)
		return;

	piStopAutoMatch(peer);
}

PEERBool peerIsAutoMatching(PEER peer)
{
	PEER_CONNECTION;

	if(connection->autoMatchStatus == PEERFailed)
		return PEERFalse;
	if(connection->autoMatchStatus == PEERComplete)
		return PEERFalse;

	return PEERTrue;
}

PEERAutoMatchStatus peerGetAutoMatchStatus(PEER peer)
{
	PEER_CONNECTION;

	return connection->autoMatchStatus;
}

void peerSetStagingRoomMaxPlayers(PEER peer, int maxPlayers)
{
	PEER_CONNECTION;
	GS_ASSERT(connection->inRoom[StagingRoom]);
	if (connection->inRoom[StagingRoom])
	{
		connection->maxPlayers = maxPlayers;
		piSendStateChanged(peer);

		chatSetChannelLimitA(connection->chat, connection->rooms[StagingRoom], maxPlayers);
	}
}
