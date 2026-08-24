/*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "peerOperations.h"
#include "peerCallbacks.h"
#include "peerGlobalCallbacks.h"
#include "peerMangle.h"
#include "peerRooms.h"
#include "peerPlayers.h"
#include "peerKeys.h"
#include "peerSB.h"
#include "peerHost.h"
#include "peerAutoMatch.h"
#include "peerQR.h"
#include "../md5.h"

#define PI_CHAT_SERVER_ADDRESS    "peerchat." GSI_DOMAIN_NAME
#define PI_CHAT_SERVER_PORT       6667

#define PEER_CONNECTION_OP        piOperation * operation = (piOperation *)param;\
                                  piConnection * connection = NULL;\
                                  PEER peer = NULL;\
                                  assert(operation && operation->peer);\
                                  if(operation && operation->peer)\
                                      peer = operation->peer;\
                                  connection = (piConnection *)peer;\
								  GSI_UNUSED(connection);
#if 0
PEER peer;
piOperation * operation;
#endif

typedef struct piOperationContainer
{
	piOperation * operation;
} piOperationContainer;

#ifndef GSI_UNICODE
#define piGetGlobalKeysCallback		piGetGlobalKeysCallbackA
#define piGetChannelKeysCallback	piGetChannelKeysCallbackA
#define piGetPlayerInfoCallback		piGetPlayerInfoCallbackA
#define piConnectNickErrorCallback	piConnectNickErrorCallbackA
#define piConnectFillInUserCallback	piConnectFillInUserCallbackA
#define piCreateStagingRoomEnumUsersCallback piCreateStagingRoomEnumUsersCallbackA
#define piCreateStagingRoomEnterChannelCallback piCreateStagingRoomEnterChannelCallbackA
#define piJoinRoomEnumUsersCallback piJoinRoomEnumUsersCallbackA
#define piJoinRoomEnterChannelCallback piJoinRoomEnterChannelCallbackA
#define piChangeNickCallback		piChangeNickCallbackA
#define piAuthenticateCDKeyCallback piAuthenticateCDKeyCallbackA
#else
#define piGetGlobalKeysCallback		piGetGlobalKeysCallbackW
#define piGetChannelKeysCallback	piGetChannelKeysCallbackW
#define piGetPlayerInfoCallback		piGetPlayerInfoCallbackW
#define piConnectNickErrorCallback	piConnectNickErrorCallbackW
#define piConnectFillInUserCallback	piConnectFillInUserCallbackW
#define piCreateStagingRoomEnumUsersCallback piCreateStagingRoomEnumUsersCallbackW
#define piCreateStagingRoomEnterChannelCallback piCreateStagingRoomEnterChannelCallbackW
#define piJoinRoomEnumUsersCallback piJoinRoomEnumUsersCallbackW
#define piJoinRoomEnterChannelCallback piJoinRoomEnterChannelCallbackW
#define piChangeNickCallback		piChangeNickCallbackW
#define piAuthenticateCDKeyCallback piAuthenticateCDKeyCallbackW
#endif

static void piOperationsListFree(void *elem1)
{
	piOperationContainer * container = (piOperationContainer *)elem1;
	piOperation * operation;

	assert(container);
	assert(container->operation);

	operation = container->operation;

	gsifree(operation->name);

	gsifree(operation->password);

	gsifree(operation->data);

	if(operation->socketClose)
	{
		closesocket(operation->socket);
		SocketShutDown();
	}

	gsifree(operation);
}

PEERBool piOperationsInit(PEER peer)
{
	PEER_CONNECTION;

	assert(!connection->operationList);

	connection->operationsStarted = 0;
	connection->operationsFinished = 0;

	connection->operationList = ArrayNew(sizeof(piOperationContainer), 0, piOperationsListFree);
	if(!connection->operationList)
		return PEERFalse;

	return PEERTrue;
}

void piOperationsReset(PEER peer)
{
	PEER_CONNECTION;

	if(connection->operationList)
		ArrayClear(connection->operationList);
	
	if (connection->listingGroupsOperation)
		connection->listingGroupsOperation = NULL;
}

void piOperationsCleanup(PEER peer)
{
	PEER_CONNECTION;

	if(connection->operationList)
		ArrayFree(connection->operationList);
	connection->operationList = NULL;
}

void piRemoveOperation(PEER peer, piOperation * operation)
{
	piOperationContainer * container;
	piOperation * op;
	int i;
	int count;

	PEER_CONNECTION;

	if(!connection->operationList)
		return;

	count = ArrayLength(connection->operationList);
	for(i = 0 ; i < count ; i++)
	{
		container = (piOperationContainer *)ArrayNth(connection->operationList, i);
		op = container->operation;

		if(op == operation)
		{
			ArrayDeleteAt(connection->operationList, i);

			connection->operationsFinished++;

			return;
		}
	}
}

PEERBool piIsOperationFinished(PEER peer, int opID)
{
	piOperationContainer * container;
	piOperation * op;
	int i;
	int count;

	PEER_CONNECTION;

	if(!connection->operationList)
		return PEERTrue;

	count = ArrayLength(connection->operationList);
	for(i = 0 ; i < count ; i++)
	{
		container = (piOperationContainer *)ArrayNth(connection->operationList, i);
		op = container->operation;

		if(op->ID == opID)
			return PEERFalse;
	}

	return PEERTrue;
}

void piCancelJoinOperation(PEER peer, RoomType roomType)
{
	piOperationContainer * container;
	piOperation * operation;
	int i;
	int count;

	PEER_CONNECTION;

	if(!connection->operationList)
		return;

	count = ArrayLength(connection->operationList);
	for(i = 0 ; i < count ; i++)
	{
		container = (piOperationContainer *)ArrayNth(connection->operationList, i);
		operation = container->operation;

		if((operation->type == PI_JOIN_ROOM_OPERATION) || (operation->type == PI_CREATE_ROOM_OPERATION))
		{
			if(operation->roomType == roomType)
			{
				operation->cancel = PEERTrue;

				return;
			}
		}
	}
}

int piGetNextID(PEER peer)
{
	int ID;

	PEER_CONNECTION;

	ID = connection->nextID;

	connection->nextID++;
	if(connection->nextID < 0)
		connection->nextID = 0;

	return ID;
}

static piOperation * piAddOperation
(
	PEER peer,
	piOperationType type,
	void * data,
	PEERCBType callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	piOperationContainer container;

	PEER_CONNECTION;

	//assert(type >= 0);
	assert(type < PI_NUM_OPERATION_TYPES);

	if(!connection->operationList)
		return NULL;

	operation = (piOperation *)gsimalloc(sizeof(piOperation));
	if(!operation)
		return NULL;

	memset(operation, 0, sizeof(piOperation));
	operation->peer = peer;
	operation->type = type;
	operation->data = data;
	operation->ID = opID;
	operation->callback = callback;
	operation->callbackParam = callbackParam;
	operation->name = NULL;
	operation->cancel = PEERFalse;

	container.operation = operation;
	ArrayAppend(connection->operationList, &container);

	connection->operationsStarted++;

	return operation;
}


static void piConnectConnectCallback
(
	CHAT chat,
	CHATBool success,
	int failureReason,
	void *param
)
{
	PEER_CONNECTION_OP;
	
	if(success)
	{
		if(!piConnectTitle(peer))
		{
			piDisconnectTitle(peer);
			success = CHATFalse;
		}
	}

	connection->connecting = PEERFalse;
	connection->connected = (PEERBool)success;

	if(success)
	{
		const char * nick;

		connection->lastChatPing = current_time();
		
		nick = chatGetNickA(chat);
		if(strcasecmp(connection->nick, nick) != 0)
		{
			strcpy(connection->nick, nick);

#ifdef GSI_UNICODE
			UTF8ToUCS2String(connection->nick, connection->nick_W);
#endif
		}
	}
	else
	{
		connection->disconnect = PEERTrue;
	}

	piAddConnectCallback(peer, (PEERBool)success, failureReason, (peerConnectCallback)operation->callback, operation->callbackParam, operation->ID);

	piRemoveOperation(peer, operation);
}

static void piConnectNickErrorCallbackA
(
	CHAT chat,
	int type,
	const char * nick,
	int numSuggestedNicks,
	const char ** suggestedNicks,
	void *param
)
{
	PEER_CONNECTION_OP;

	piAddNickErrorCallback(peer, type, nick, numSuggestedNicks, suggestedNicks, operation->callbackParam, operation->ID);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piConnectNickErrorCallbackW
(
	CHAT chat,
	int type,
	const unsigned short * nick,
	int numSuggestedNicks,
	const unsigned short ** suggestedNicks,
	void *param
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char** suggestedNicks_A = UCS2ToUTF8StringArrayAlloc(suggestedNicks, numSuggestedNicks);
	int i;
	piConnectNickErrorCallbackA(chat, type, nick_A, numSuggestedNicks, (const char**)suggestedNicks_A, param);
	gsifree(nick_A);
	if(suggestedNicks_A)
	{
		for (i=0; i<numSuggestedNicks; i++)
			gsifree(suggestedNicks_A[i]);
	}
}
#endif

static void piConnectFillInUserCallbackA
(
	CHAT chat,
	unsigned int IP,
	char user[128],
	void * param
)
{
	int pid;

	PEER_CONNECTION_OP;

	if( !connection )
		return;

	connection->publicIP = IP;

	pid = chatGetProfileID(connection->chat);
	if(pid)
		connection->profileID = pid;

	piMangleUser(user, connection->publicIP, connection->profileID);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piConnectFillInUserCallbackW
(
	CHAT chat,
	unsigned int IP,
	unsigned short user[128], 
	void * param
)
{
	char user_A[256];
	piConnectFillInUserCallbackA(chat, IP, user_A, param);
	UTF8ToUCS2String(user_A, user);
}
#endif

PEERBool piNewConnectOperation
(
	PEER peer,
	piConnectType connectType,
	const char * nick,
	int namespaceID,
	const char * email,
	const char * profilenick,
	const char * uniquenick,
	const char * password,
	const char * authtoken,
	const char * partnerchallenge,
	peerConnectCallback callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	chatGlobalCallbacks globalCallbacks;
	const char * uniqueID;
	char encodedUniqueID[33];
	peerNickErrorCallback nickErrorCallback;

	PEER_CONNECTION;

	assert(callback);

#ifdef _DEBUG
	if(connectType == PI_CONNECT)
	{
		assert(nick && nick[0]);
	}
	else if(connectType == PI_CONNECT_UNIQUENICK_LOGIN)
	{
		assert(namespaceID > 0);
		assert(uniquenick && uniquenick[0]);
		assert(password && password[0]);
	}
	else if(connectType == PI_CONNECT_PROFILENICK_LOGIN)
	{
		assert(namespaceID >= 0);
		assert(email && email[0]);
		assert(profilenick && profilenick[0]);
		assert(password && password[0]);
	}
	else if(connectType == PI_CONNECT_PREAUTH)
	{
		assert(authtoken && authtoken[0]);
		assert(partnerchallenge && partnerchallenge[0]);
	}
#endif

	operation = piAddOperation(peer, PI_CONNECT_OPERATION, NULL, (PEERCBType)callback, callbackParam, opID);
	if(!operation)
		return PEERFalse;

	memset(&globalCallbacks, 0, sizeof(chatGlobalCallbacks));
	globalCallbacks.disconnected = piChatDisconnected;
	globalCallbacks.privateMessage = piChatPrivateMessage;
	globalCallbacks.param = peer;

	uniqueID = GOAGetUniqueID();
	MD5Digest((unsigned char *)uniqueID, strlen(uniqueID), encodedUniqueID);

	nickErrorCallback = (connection->nickErrorCallback ? piConnectNickErrorCallback : NULL);
	if(connectType == PI_CONNECT)
	{
		connection->chat = chatConnectSecureA(
			PI_CHAT_SERVER_ADDRESS,
			PI_CHAT_SERVER_PORT,
			nick,
			encodedUniqueID,
			connection->sbName,
			connection->sbSecretKey,
			&globalCallbacks,
			nickErrorCallback,
			piConnectFillInUserCallback,
			piConnectConnectCallback,
			operation,
			CHATFalse);
	}
	else if((connectType == PI_CONNECT_UNIQUENICK_LOGIN) || (connectType == PI_CONNECT_PROFILENICK_LOGIN))
	{
		connection->chat = chatConnectLoginA(
			PI_CHAT_SERVER_ADDRESS,
			PI_CHAT_SERVER_PORT,
			namespaceID,
			email,
			profilenick,
			uniquenick,
			password,
			encodedUniqueID,
			connection->sbName,
			connection->sbSecretKey,
			&globalCallbacks,
			nickErrorCallback,
			piConnectFillInUserCallback,
			piConnectConnectCallback,
			operation,
			CHATFalse);
	}
	else if(connectType == PI_CONNECT_PREAUTH)
	{
		connection->chat = chatConnectPreAuthA(
			PI_CHAT_SERVER_ADDRESS,
			PI_CHAT_SERVER_PORT,
			authtoken,
			partnerchallenge,
			encodedUniqueID,
			connection->sbName,
			connection->sbSecretKey,
			&globalCallbacks,
			nickErrorCallback,
			piConnectFillInUserCallback,
			piConnectConnectCallback,
			operation,
			CHATFalse);
	}
	if(!connection->chat)
	{
		piRemoveOperation(peer, operation);
		return PEERFalse;
	}

	return PEERTrue;
}

static PEERJoinResult piEnterResultToJoinResult
(
	CHATEnterResult result
)
{
	switch(result)
	{
	case CHATEnterSuccess:
		return PEERJoinSuccess;
	case CHATChannelIsFull:
		return PEERFullRoom;
	case CHATInviteOnlyChannel:
		return PEERInviteOnlyRoom;
	case CHATBannedFromChannel:
		return PEERBannedFromRoom;
	case CHATBadChannelPassword:
		return PEERBadPassword;
	default:
		break;
	}

	return PEERJoinFailed;
}

static void piCreateStagingRoomEnumUsersCallbackA
(
	CHAT chat,
	CHATBool success,
	const char * channel,
	int numUsers,
	const char ** operators,
	int * modes,
	void *param
)
{
	PEER_CONNECTION_OP;

	if(operation->cancel)
	{
		piRemoveOperation(peer, operation);
		return;
	}

	if(success)
	{
		if(!peerIsAutoMatching(peer))
		{
			if(!piStartHosting(peer, operation->socket, operation->port))
				success = CHATFalse;
			else
			{
				if(operation->socketClose)
				{
					operation->socketClose = PEERFalse;
					connection->queryReporting->read_socket = 1;
				}
			}
		}
		else
		{
			connection->hosting = PEERTrue;
		}
	}

	if(success)
	{
		int i;

		piFinishedEnteringRoom(peer, StagingRoom, operation->name);

		for(i = 0 ; i < numUsers ; i++)
			piPlayerJoinedRoom(peer, operators[i], StagingRoom, modes[i]);

		chatSetChannelTopicA(connection->chat, channel, operation->name);

#ifndef PI_NO_STAGING_ROOM_LIMIT
		if(connection->maxPlayers)
			chatSetChannelLimitA(connection->chat, channel, connection->maxPlayers);
#endif

		if(operation->socketClose && peerIsAutoMatching(peer))
		{
			connection->autoMatchOperation->socket = operation->socket;
			connection->autoMatchOperation->port = operation->port;
			connection->autoMatchOperation->socketClose = PEERTrue;
			operation->socketClose = PEERFalse;
		}
	}
	else
	{
		piLeaveRoom(peer, StagingRoom, NULL);
	}

	piAddJoinRoomCallback(peer, (PEERBool)success, success?PEERJoinSuccess:PEERJoinFailed, StagingRoom, (peerJoinRoomCallback)operation->callback, operation->callbackParam, operation->ID);

	piRemoveOperation(peer, operation);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piCreateStagingRoomEnumUsersCallbackW
(
	CHAT chat,
	CHATBool success,
	const unsigned short * channel,
	int numUsers,
	const unsigned short ** operators,
	int * modes,
	void *param
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char** users_A = UCS2ToUTF8StringArrayAlloc(operators, numUsers);
	int i;
	piCreateStagingRoomEnumUsersCallbackA(chat, success, channel_A, numUsers, (const char**)users_A, modes, param);
	gsifree(channel_A);
	for (i=0; i<numUsers; i++)
		gsifree(users_A[i]);
	gsifree(users_A);
}
#endif


static void piCreateStagingRoomGetChannelModeCallback(CHAT chat, CHATBool success, const gsi_char * channel, 
	CHATChannelMode * mode, void * param)
{
	piConnection *connection = (piConnection *)param;
	if (success)
	{

		mode->Limit = connection->maxPlayers;
		mode->OpsObeyChannelLimit = CHATTrue;
		
		chatSetChannelMode(chat, channel, mode);
	}
	
}


static void piCreateStagingRoomEnterChannelCallbackA
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const char * channel,
	void *param
)
{	
	PEER_CONNECTION_OP;

	assert(channel);
	assert(channel[0]);

	if(operation->cancel)
	{
		piRemoveOperation(peer, operation);
		return;
	}

	if(success)
	{
		if(operation->password)
			chatSetChannelPasswordA(connection->chat, channel, CHATTrue, operation->password);

		
		chatGetChannelMode(connection->chat, peerGetRoomChannel(peer, StagingRoom), piCreateStagingRoomGetChannelModeCallback, 
				(void *)connection, CHATFalse);

		
		chatEnumUsersA(chat, channel, piCreateStagingRoomEnumUsersCallback, operation, CHATFalse);
	}
	else
	{
		PEERJoinResult joinResult;

		piLeaveRoom(peer, StagingRoom, NULL);

		if(result == CHATEnterSuccess)
			joinResult = PEERJoinFailed;
		else
			joinResult = piEnterResultToJoinResult(result);

		piAddJoinRoomCallback(peer, PEERFalse, joinResult, StagingRoom, (peerJoinRoomCallback)operation->callback, operation->callbackParam, operation->ID);

		piRemoveOperation(peer, operation);
	}
}
#ifdef GSI_UNICODE
static void piCreateStagingRoomEnterChannelCallbackW
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const unsigned short * channel,
	void *param
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	piCreateStagingRoomEnterChannelCallbackA(chat, success, result, channel_A, param);
	gsifree(channel_A);
}
#endif

qr2_error_t qr2_create_socket(SOCKET *sock, const char *ip, int * port);

PEERBool piNewCreateStagingRoomOperation
(
	PEER peer,
	const char * name,
	const char * password,
	int maxPlayers,
	SOCKET socket,
	unsigned short port,
	peerJoinRoomCallback callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	chatChannelCallbacks channelCallbacks;
	char room[PI_ROOM_MAX_LEN];
	PEERBool createdSocket = PEERFalse;

	PEER_CONNECTION;

	assert(name);
	if(!name)
		name = "";

	assert(callback);
	if(!callback)
		return PEERFalse;

	connection->maxPlayers = maxPlayers;

	if(socket == INVALID_SOCKET)
	{
		IN_ADDR addr;
		qr2_error_t rcode;
		int privatePort;

		addr.s_addr =  0;
		if(port)
			privatePort = port;
		else
			privatePort = PI_QUERYPORT;

		rcode = qr2_create_socket(&socket, inet_ntoa(addr), &privatePort);
		if(rcode != e_qrnoerror)
			return PEERFalse;

		port = (unsigned short)privatePort;
		createdSocket = PEERTrue;
	}

	piMangleStagingRoom(room, connection->title, connection->publicIP, connection->privateIP, port);
	assert(room[0]);

	operation = piAddOperation(peer, PI_CREATE_ROOM_OPERATION, NULL, (PEERCBType)callback, callbackParam, opID);
	if(!operation)
		return PEERFalse;
	operation->socketClose = createdSocket;
	operation->name = goastrdup(name);
	if(!operation->name)
	{
		piRemoveOperation(peer, operation);
		return PEERFalse;
	}
	operation->socket = socket;
	operation->port = port;
	operation->roomType = StagingRoom;
	if(password[0])
		operation->password = goastrdup(password);

	piSetChannelCallbacks(peer, &channelCallbacks);

	piStartedEnteringRoom(peer, StagingRoom, room);
	chatEnterChannelA(connection->chat, room, NULL, &channelCallbacks, piCreateStagingRoomEnterChannelCallback, operation, CHATFalse);

	if(password[0])
		connection->passwordedRoom = PEERTrue;

	return PEERTrue;
}

static void piJoinRoomEnumUsersCallbackA
(
	CHAT chat,
	CHATBool success,
	const char * channel,
	int numUsers,
	const char ** operators,
	int * modes,
	void * param
)
{	
	PEER_CONNECTION_OP;

	if(operation->cancel)
	{
		piRemoveOperation(peer, operation);
		return;
	}

	if(success)
	{
		int i;

		piFinishedEnteringRoom(peer, operation->roomType, "");

		for(i = 0 ; i < numUsers ; i++)
			piPlayerJoinedRoom(peer, operators[i], operation->roomType, modes[i]);
	}
	else
	{
		piLeaveRoom(peer, operation->roomType, NULL);
	}

	piAddJoinRoomCallback(peer, (PEERBool)success, success?PEERJoinSuccess:PEERJoinFailed, operation->roomType, (peerJoinRoomCallback)operation->callback, operation->callbackParam, operation->ID);

	piRemoveOperation(peer, operation);
	
	GSI_UNUSED(chat);
	GSI_UNUSED(channel);
}
#ifdef GSI_UNICODE
static void piJoinRoomEnumUsersCallbackW
(
	CHAT chat,
	CHATBool success,
	const unsigned short * channel,
	int numUsers,
	const unsigned short ** operators,
	int * modes,
	void * param
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char** users_A = UCS2ToUTF8StringArrayAlloc(operators, numUsers);
	int i;
	piJoinRoomEnumUsersCallbackA(chat, success, channel_A, numUsers, (const char**)users_A, modes, param);
	gsifree(channel_A);
	for (i=0; i<numUsers; i++)
		gsifree(users_A[i]);
	gsifree(users_A);
}
#endif

static void piJoinRoomEnterChannelCallbackA
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const char * channel,
	void *param
)
{	
	PEER_CONNECTION_OP;

	assert(channel);
	assert(channel[0]);

	if(operation->cancel)
	{
		piSBFreeHostServer(peer);
		piRemoveOperation(peer, operation);
		return;
	}

	if(success)
	{
		chatEnumUsersA(chat, channel, piJoinRoomEnumUsersCallback, operation, CHATFalse);
	}
	else
	{
		PEERJoinResult joinResult;

		piLeaveRoom(peer, operation->roomType, NULL);

		if(result == CHATEnterSuccess)
			joinResult = PEERJoinFailed;
		else
			joinResult = piEnterResultToJoinResult(result);

		piAddJoinRoomCallback(peer, PEERFalse, joinResult, operation->roomType, (peerJoinRoomCallback)operation->callback, operation->callbackParam, operation->ID);

		piRemoveOperation(peer, operation);
	}
}
#ifdef GSI_UNICODE
static void piJoinRoomEnterChannelCallbackW
(
	CHAT chat,
	CHATBool success,
	CHATEnterResult result,
	const unsigned short * channel,
	void *param
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	piJoinRoomEnterChannelCallbackA(chat, success, result, channel_A, param);
	gsifree(channel_A);
}
#endif

PEERBool piNewJoinRoomOperation
(
	PEER peer,
	RoomType roomType,
	const char * channel,
	const char * password,
	peerJoinRoomCallback callback,
	void * callbackParam,
	int opID
)
{
	piOperation * operation;
	chatChannelCallbacks channelCallbacks;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);
	assert(callback);

	assert(channel);
	assert(channel[0]);
	if(!channel || !channel[0])
		return PEERFalse;

	if(!password)
		password = "";

	assert(strlen(channel) < PI_ROOM_MAX_LEN);
	if(strlen(channel) >= PI_ROOM_MAX_LEN)
		return PEERFalse;

	operation = piAddOperation(peer, PI_JOIN_ROOM_OPERATION, NULL, (PEERCBType)callback, callbackParam, opID);
	if(!operation)
		return PEERFalse;
	operation->roomType = roomType;

	piSetChannelCallbacks(peer, &channelCallbacks);

	piStartedEnteringRoom(peer, roomType, channel);
	chatEnterChannelA(connection->chat, channel, password, &channelCallbacks, piJoinRoomEnterChannelCallback, operation, CHATFalse);

	return PEERTrue;
}

PEERBool piNewListGroupRoomsOperation
(
	PEER peer,
	const char * fields,
	peerListGroupRoomsCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(callback);

	operation = connection->listingGroupsOperation = piAddOperation(peer, PI_LIST_GROUP_ROOMS_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	return piSBStartListingGroups(peer, fields);
}

static void piGetPlayerInfoCallbackA
(
	CHAT chat,
	CHATBool success,
	const char * nick,
	const char * user,
	const char * address,
	void * param
)
{
	int profileID = 0;
	unsigned int IP = 0;

	PEER_CONNECTION_OP;

	assert(nick);
	assert(nick[0]);

	if(success)
	{
		assert(user);
		assert(user[0]);

		if(!piDemangleUser(user, &IP, &profileID))
			success = CHATFalse;

		if(success)
			piSetPlayerIPAndProfileID(peer, nick, IP, profileID);
	}
	if(!success)
	{
		profileID = 0;
		IP = 0;
	}

	if(operation->callback)
	{
		if(operation->type == PI_GET_PLAYER_INFO_OPERATION)
			piAddGetPlayerInfoCallback(peer, (PEERBool)success, nick, IP, profileID, (peerGetPlayerInfoCallback)operation->callback, operation->callbackParam, operation->ID);
		else if(operation->type == PI_GET_PROFILE_ID_OPERATION)
			piAddGetPlayerProfileIDCallback(peer, (PEERBool)success, nick, profileID, (peerGetPlayerProfileIDCallback)operation->callback, operation->callbackParam, operation->ID);
		else if(operation->type == PI_GET_IP_OPERATION)
			piAddGetPlayerIPCallback(peer, (PEERBool)success, nick, IP, (peerGetPlayerIPCallback)operation->callback, operation->callbackParam, operation->ID);
		else
			assert(0);
	}

	piRemoveOperation(peer, operation);
	
	GSI_UNUSED(chat);
	GSI_UNUSED(address);
}
#ifdef GSI_UNICODE
static void piGetPlayerInfoCallbackW
(
	CHAT chat,
	CHATBool success,
	const unsigned short * nick,
	const unsigned short * user,
	const unsigned short * address,
	void * param
)
{
	char* nick_A = UCS2ToUTF8StringAlloc(nick);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char* address_A = UCS2ToUTF8StringAlloc(address);
	piGetPlayerInfoCallbackA(chat, success, nick_A, user_A, address_A, param);
	gsifree(nick_A);
	gsifree(user_A);
	gsifree(address_A);
}
#endif

PEERBool piNewGetPlayerInfoOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerInfoCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);

	operation = piAddOperation(peer, PI_GET_PLAYER_INFO_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	chatGetBasicUserInfoA(connection->chat, nick, piGetPlayerInfoCallback, operation, CHATFalse);

	return PEERTrue;
}

PEERBool piNewGetProfileIDOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerProfileIDCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);

	operation = piAddOperation(peer, PI_GET_PROFILE_ID_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	chatGetBasicUserInfoA(connection->chat, nick, piGetPlayerInfoCallback, operation, CHATFalse);

	return PEERTrue;
}

PEERBool piNewGetIPOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerIPCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);

	operation = piAddOperation(peer, PI_GET_IP_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	chatGetBasicUserInfoA(connection->chat, nick, piGetPlayerInfoCallback, operation, CHATFalse);

	return PEERTrue;
}

static void piChangeNickCallbackA
(
	CHAT chat,
	CHATBool success,
	const char * oldNick,
	const char * newNick,
	void * param
)
{
	PEER_CONNECTION_OP;

	if(success)
	{
		strcpy(connection->nick, newNick);

#ifdef GSI_UNICODE
		UTF8ToUCS2String(connection->nick, connection->nick_W);
#endif
	}

	if(operation->callback)
		piAddChangeNickCallback(peer, (PEERBool)success, oldNick, newNick, (peerChangeNickCallback)operation->callback, operation->callbackParam, operation->ID);

	piRemoveOperation(peer, operation);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChangeNickCallbackW
(
	CHAT chat,
	CHATBool success,
	const unsigned short * oldNick,
	const unsigned short * newNick,
	void * param
)
{
	char* oldNick_A = UCS2ToUTF8StringAlloc(oldNick);
	char* newNick_A = UCS2ToUTF8StringAlloc(newNick);
	piChangeNickCallbackA(chat, success, oldNick_A, newNick_A, param);
	gsifree(oldNick_A);
	gsifree(newNick_A);
}
#endif

PEERBool piNewChangeNickOperation
(
	PEER peer,
	const char * newNick,
	peerChangeNickCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(newNick);
	assert(newNick[0]);

	operation = piAddOperation(peer, PI_CHANGE_NICK_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	chatChangeNickA(connection->chat, newNick, piChangeNickCallback, operation, CHATFalse);

	return PEERTrue;
}

static void piGetGlobalKeysCallbackA
(
	CHAT chat,
	CHATBool success,
	const char * user,
	int num,
	const char ** keys,
	const char ** values,
	void * param
)
{
	int i;

	PEER_CONNECTION_OP;

	if(success && user)
	{
		for(i = 0 ; i < num ; i++)
			piGlobalKeyChanged(peer, user, keys[i], values[i]);
	}

	if(operation->callback)
		piAddGetGlobalKeysCallback(peer, (PEERBool)success, user, num, keys, values, (peerGetGlobalKeysCallback)operation->callback, operation->callbackParam, operation->ID);

	if(!success || !user || !operation->num)
		piRemoveOperation(peer, operation);

	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piGetGlobalKeysCallbackW
(
	CHAT chat,
	CHATBool success,
	const unsigned short * user,
	int num,
	const unsigned short ** keys,
	const unsigned short ** values,
	void * param
)
{
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	char** values_A = UCS2ToUTF8StringArrayAlloc(values, num);
	int i;
	piGetGlobalKeysCallbackA(chat, success, user_A, num, (const char**)keys_A, (const char**)values_A, param);
	gsifree(user_A);
	for (i=0; i<num; i++)
	{
		gsifree(keys_A[i]);
		if (values_A != NULL) 
			gsifree(values_A[i]);
	}
	gsifree(keys_A);
	gsifree(values_A);
}
#endif

PEERBool piNewGetGlobalKeysOperation
(
	PEER peer,
	const char * target,
	int num,
	const char ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(target);
	assert(num > 0);
	assert(keys);

	if(!target || !target[0])
		return PEERFalse;
	if(num <= 0)
		return PEERFalse;

	operation = piAddOperation(peer, PI_GET_GLOBAL_KEYS_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	operation->num = (target[0] == '#');

	chatGetGlobalKeysA(connection->chat, target, num, keys, piGetGlobalKeysCallback, operation, CHATFalse);

	return PEERTrue;
}

static void piGetChannelKeysCallbackA
(
	CHAT chat,
	CHATBool success,
	const char * channel,
	const char * user,
	int num,
	const char ** keys,
	const char ** values,
	void * param
)
{
	int i;

	PEER_CONNECTION_OP;

	if(success && user)
	{
		for(i = 0 ; i < num ; i++)
			piRoomKeyChanged(peer, operation->roomType, user, keys[i], values[i]);
	}

	if(operation->callback)
		piAddGetRoomKeysCallback(peer, (PEERBool)success, operation->roomType, user, num, keys, values, (peerGetRoomKeysCallback)operation->callback, operation->callbackParam, operation->ID);

	if(!success || !user || !operation->num)
		piRemoveOperation(peer, operation);

	GSI_UNUSED(chat);
	GSI_UNUSED(channel);
}
#ifdef GSI_UNICODE
static void piGetChannelKeysCallbackW
(
	CHAT chat,
	CHATBool success,
	const unsigned short * channel,
	const unsigned short * user,
	int num,
	const unsigned short ** keys,
	const unsigned short ** values,
	void * param
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char** keys_A = UCS2ToUTF8StringArrayAlloc(keys, num);
	char** values_A = UCS2ToUTF8StringArrayAlloc(values, num);
	int i;
	piGetChannelKeysCallbackA(chat, success, channel_A, user_A, num, (const char**)keys_A, (const char**)values_A, param);
	gsifree(channel_A);
	gsifree(user_A);
	for (i=0; i<num; i++)
	{
		gsifree(keys_A[i]);
		if (values_A != NULL) 
			gsifree(values_A[i]);
	}
	gsifree(keys_A);
	gsifree(values_A);

}
#endif

PEERBool piNewGetRoomKeysOperation
(
	PEER peer,
	RoomType roomType,
	const char * nick,
	int num,
	const char ** keys,
	peerGetRoomKeysCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);
	assert(num >= 0);
	assert(!num || keys);

	if(num < 0)
		return PEERFalse;
	if((num > 0) && !keys)
		return PEERFalse;

	if(!ENTERING_ROOM && !IN_ROOM)
		return PEERFalse;

	operation = piAddOperation(peer, PI_GET_ROOM_KEYS_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;
	operation->roomType = roomType;

	if(nick)
		operation->num = (strcmp(nick, "*") == 0);
	else
		operation->num = 0;

	chatGetChannelKeysA(connection->chat, ROOM, nick, num, keys, piGetChannelKeysCallback, operation, CHATFalse);

	return PEERTrue;
}

static void piAuthenticateCDKeyCallbackA
(
	CHAT chat,
	int result,
	const char * message,
	void * param
)
{

	PEER_CONNECTION_OP;

	if(operation->callback)
		piAddAuthenticateCDKeyCallback(peer, result, message, (peerAuthenticateCDKeyCallback)operation->callback, operation->callbackParam, operation->ID);

	piRemoveOperation(peer, operation);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piAuthenticateCDKeyCallbackW
(
	CHAT chat,
	int result,
	const unsigned short * message,
	void * param
)
{
	char* message_A = UCS2ToUTF8StringAlloc(message);
	piAuthenticateCDKeyCallbackA(chat, result, message_A, param);
	gsifree(message_A);
}
#endif

PEERBool piNewAuthenticateCDKeyOperation
(
	PEER peer,
	const char * cdkey,
	peerAuthenticateCDKeyCallback callback,
	void * param,
	int opID
)
{
	piOperation * operation;

	PEER_CONNECTION;

	assert(cdkey);
	assert(cdkey[0]);

	operation = piAddOperation(peer, PI_AUTHENTICATE_CDKEY_OPERATION, NULL, (PEERCBType)callback, param, opID);
	if(!operation)
		return PEERFalse;

	chatAuthenticateCDKeyA(connection->chat, cdkey, piAuthenticateCDKeyCallback, operation, CHATFalse);

	return PEERTrue;
}

PEERBool piNewAutoMatchOperation
(
	PEER peer,
	SOCKET socket,
	unsigned short port,
	peerAutoMatchStatusCallback statusCallback,
	peerAutoMatchRateCallback rateCallback,
	void * param,
	int opID
)
{
	piOperation * operation;
	PEERAutoMatchStatus status;

	PEER_CONNECTION;

	operation = piAddOperation(peer, PI_AUTO_MATCH_OPERATION, NULL, (PEERCBType)statusCallback, param, opID);
	if(!operation)
		return PEERFalse;

	operation->callback2 = (PEERCBType)rateCallback;

	operation->socket = socket;
	operation->port = port;

	connection->autoMatchOperation = operation;

	{
		status = PEERSearching;
	}

	piSetAutoMatchStatus(peer, status);

	return PEERTrue;
}
