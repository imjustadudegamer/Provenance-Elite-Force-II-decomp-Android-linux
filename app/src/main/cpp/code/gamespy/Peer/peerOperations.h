/*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#ifndef _PEEROPERATIONS_H_
#define _PEEROPERATIONS_H_

#include "peerMain.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum piOperationType
{
	PI_CONNECT_OPERATION,
	PI_CREATE_ROOM_OPERATION,
	PI_JOIN_ROOM_OPERATION,
	PI_ENUM_PLAYERS_OPERATION,
	PI_LIST_GROUP_ROOMS_OPERATION,
	PI_LIST_STAGING_ROOMS_OPERATION,
	PI_GET_PLAYER_INFO_OPERATION,
	PI_GET_PROFILE_ID_OPERATION,
	PI_GET_IP_OPERATION,
	PI_CHANGE_NICK_OPERATION,
	PI_GET_GLOBAL_KEYS_OPERATION,
	PI_GET_ROOM_KEYS_OPERATION,
	PI_AUTHENTICATE_CDKEY_OPERATION,
	PI_AUTO_MATCH_OPERATION,
	PI_NUM_OPERATION_TYPES
} piOperationType;

typedef enum piConnectType
{
	PI_CONNECT,
	PI_CONNECT_UNIQUENICK_LOGIN,
	PI_CONNECT_PROFILENICK_LOGIN,
	PI_CONNECT_PREAUTH
} piConnectType;

typedef struct piOperation
{
	PEER peer;             
	piOperationType type;  
	void * data;           
	int ID;                
	PEERCBType callback;       
	PEERCBType callback2;      
	void * callbackParam;  
	RoomType roomType;     
	char * name;           
	char * password;       
	int num;               
	SOCKET socket;         
	unsigned short port;   
	PEERBool socketClose;  
	PEERBool cancel;       
} piOperation;

PEERBool piOperationsInit(PEER peer);
void piOperationsReset(PEER peer);
void piOperationsCleanup(PEER peer);
PEERBool piIsOperationFinished(PEER peer, int opID);
void piRemoveOperation(PEER peer, piOperation * operation);
void piCancelJoinOperation(PEER peer, RoomType roomType);
piOperation * piGetOperation(PEER peer, int opID);
int piGetNextID(PEER peer);

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
);

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
);

PEERBool piNewJoinRoomOperation
(
	PEER peer,
	RoomType roomType,
	const char * channel,
	const char * password,
	peerJoinRoomCallback callback,
	void * callbackParam,
	int opID
);

PEERBool piNewListGroupRoomsOperation
(
	PEER peer,
	const char * fields,
	peerListGroupRoomsCallback callback,
	void * param,
	int opID
);

PEERBool piNewGetPlayerInfoOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerInfoCallback callback,
	void * param,
	int opID
);

PEERBool piNewGetProfileIDOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerProfileIDCallback callback,
	void * param,
	int opID
);

PEERBool piNewGetIPOperation
(
	PEER peer,
	const char * nick,
	peerGetPlayerIPCallback callback,
	void * param,
	int opID
);

PEERBool piNewChangeNickOperation
(
	PEER peer,
	const char * newNick,
	peerChangeNickCallback callback,
	void * param,
	int opID
);

PEERBool piNewGetGlobalKeysOperation
(
	PEER peer,
	const char * target,
	int num,
	const char ** keys,
	peerGetGlobalKeysCallback callback,
	void * param,
	int opID
);

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
);

PEERBool piNewAuthenticateCDKeyOperation
(
	PEER peer,
	const char * cdkey,
	peerAuthenticateCDKeyCallback callback,
	void * param,
	int opID
);

PEERBool piNewAutoMatchOperation
(
	PEER peer,
	SOCKET socket,
	unsigned short port,
	peerAutoMatchStatusCallback statusCallback,
	peerAutoMatchRateCallback rateCallback,
	void * param,
	int opID
);

#ifdef __cplusplus
}
#endif

#endif
