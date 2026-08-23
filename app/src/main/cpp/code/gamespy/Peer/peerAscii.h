
#ifndef _PEERASCII_H_
#define _PEERASCII_H_


#ifdef __cplusplus
extern "C" {
#endif


void peerConnectA
(
	PEER peer,  
	const char * nick,  
	int profileID,  
	peerNickErrorCallback nickErrorCallback,  
	peerConnectCallback connectCallback,  
	void * param,  
	PEERBool blocking   
);

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
);

void peerConnectPreAuthA
(
	PEER peer,  
	const char * authtoken,  
	const char * partnerchallenge,  
	peerNickErrorCallback nickErrorCallback,  
	peerConnectCallback connectCallback,  
	void * param,  
	PEERBool blocking   
);

void peerRetryWithNickA
(
	PEER peer,
	const char * nick
);

void peerRegisterUniqueNickA
(
	PEER peer,
	int namespaceID,
	const char * uniquenick,
	const char * cdkey
);

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
);

const char * peerGetTitleA(PEER peer);

const char * peerGetNickA(PEER peer);

void peerFixNickA
(
	char * newNick,
	const char * oldNick
);

const char * peerTranslateNickA
(
	char * nick,  
	const char * extension  
);

void peerChangeNickA
(
	PEER peer,  
	const char * newNick,  
	peerChangeNickCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerSetAwayModeA
(
	PEER peer,  
	const char * reason  
);

void peerParseQueryA
(
	PEER peer,  
	char * query,  
	int len,  
	struct sockaddr * sender  
);

void peerAuthenticateCDKeyA
(
	PEER peer,  
	const char * cdkey,  
	peerAuthenticateCDKeyCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerJoinTitleRoomA
(
	PEER peer,  
	const char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerJoinStagingRoomA
(
	PEER peer,  
	SBServer server,  
	const char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking   
);

void peerJoinStagingRoomByChannelA
(
	PEER peer,  
	const char * channel,  
	const char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking   
);

void peerCreateStagingRoomA
(
	PEER peer,  
	const char * name,  
	int maxPlayers,  
	const char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking 
);

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
);

void peerLeaveRoomA
(
	PEER peer,  
	RoomType roomType,  
	const char * reason  
);

void peerListGroupRoomsA
(
	PEER peer,  
	const char * fields,  
	peerListGroupRoomsCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerStartListingGamesA
(
	PEER peer,  
	const unsigned char * fields,  
	int numFields,  
	const char * filter,  
	peerListingGamesCallback callback,  
	void * param  
);

void peerMessageRoomA
(
	PEER peer,  
	RoomType roomType,  
	const char * message,  
	MessageType messageType  
);

void peerUTMRoomA
(
	PEER peer,  
	RoomType roomType,  
	const char * command,  
	const char * parameters,  
	PEERBool authenticate  
);

void peerSetPasswordA
(
	PEER peer,  
	RoomType roomType,  
	const char password[PEER_PASSWORD_LEN]  
);

void peerSetRoomNameA
(
	PEER peer,  
	RoomType roomType,  
	const char * name  
);

const char * peerGetRoomNameA
(
	PEER peer,  
	RoomType roomType  
);

const char * peerGetRoomChannelA
(
	PEER peer,  
	RoomType roomType  
);

void peerSetTitleRoomChannelA
(
	PEER peer,  
	const char * channel  
);


void peerMessagePlayerA
(
	PEER peer,  
	const char * nick,  
	const char * message,  
	MessageType messageType  
);

void peerUTMPlayerA
(
	PEER peer,  
	const char * nick,  
	const char * command,  
	const char * parameters,  
	PEERBool authenticate  
);

void peerKickPlayerA
(
	PEER peer,  
	RoomType roomType,  
	const char * nick,  
	const char * reason  
);

PEERBool peerGetPlayerPingA
(
	PEER peer,  
	const char * nick,  
	int * ping  
);

PEERBool peerGetPlayersCrossPingA
(
	PEER peer,  
	const char * nick1,  
	const char * nick2,  
	int * crossPing  
);

PEERBool peerPingPlayerA
(
	PEER peer,  
	const char * nick  
);

PEERBool peerGetPlayerInfoNoWaitA
(
	PEER peer,
	const char * nick,
	unsigned int * IP,
	int * profileID
);

void peerGetPlayerInfoA
(
	PEER peer,  
	const char * nick,  
	peerGetPlayerInfoCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerGetPlayerProfileIDA
(
	PEER peer,  
	const char * nick,  
	peerGetPlayerProfileIDCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerGetPlayerIPA
(
	PEER peer,  
	const char * nick,  
	peerGetPlayerIPCallback callback,  
	void * param,  
	PEERBool blocking  
);

PEERBool peerIsPlayerHostA
(
	PEER peer,  
	const char * nick,  
	RoomType roomType  
);

PEERBool peerGetPlayerFlagsA
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	int * flags
);

PEERBool peerGetReadyA
(
	PEER peer,  
	const char * nick, 
	PEERBool * ready  
);

void peerStartGameA
(
	PEER peer,  
	const char * message,  
	int reportingOptions  
);

void peerSetGlobalKeysA
(
	PEER peer,  
	int num,  
	const char ** keys,  
	const char ** values  
);


void peerGetPlayerGlobalKeysA
(
	PEER peer,  
	const char * nick,  
	int num,  
	const char ** keys,  
	peerGetGlobalKeysCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerGetRoomGlobalKeysA
(
	PEER peer,  
	RoomType roomType,  
	int num,  
	const char ** keys,  
	peerGetGlobalKeysCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerSetRoomKeysA
(
	PEER peer,  
	RoomType roomType,  
	const char * nick,  
	int num,  
	const char ** keys,  
	const char ** values  
);

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
);

void peerSetGlobalWatchKeysA
(
	PEER peer,  
	RoomType roomType,  
	int num,  
	const char ** keys,  
	PEERBool addKeys  
);

void peerSetRoomWatchKeysA
(
	PEER peer,  
	RoomType roomType,  
	int num,  
	const char ** keys,  
	PEERBool addKeys  
);

const char * peerGetGlobalWatchKeyA
(
	PEER peer,  
	const char * nick,  
	const char * key  
);

const char * peerGetRoomWatchKeyA
(
	PEER peer,  
	RoomType roomType,  
	const char * nick,  
	const char * key  
);


void peerStartAutoMatchA
(
	PEER peer,  
	int maxPlayers,  
	const char * filter,  
	peerAutoMatchStatusCallback statusCallback,  
	peerAutoMatchRateCallback rateCallback,  
	void * param,  
	PEERBool blocking   
);

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
);


#ifdef __cplusplus
}
#endif

#endif
