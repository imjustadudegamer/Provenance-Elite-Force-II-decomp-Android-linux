/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#ifndef _PEER_H_
#define _PEER_H_

#include "../common/gsCommon.h"
#include "../Chat/chat.h"
#include "../qr2/qr2.h"
#include "../serverbrowsing/sb_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PEER_ADD        0  
#define PEER_UPDATE     1  
#define PEER_REMOVE     2  
#define PEER_CLEAR      3  
#define PEER_COMPLETE   4  

#define PEER_NICK_OK                0
#define PEER_IN_USE                 1  
#define PEER_INVALID                2  
#define PEER_UNIQUENICK_EXPIRED     3  
#define PEER_NO_UNIQUENICK          4  
#define PEER_INVALID_UNIQUENICK     5  
#define PEER_NICK_TOO_LONG          6  
	
#define PEER_DISCONNECTED           0  
#define PEER_NICK_ERROR             1  
#define PEER_LOGIN_FAILED           2  

#define PEER_PASSWORD_LEN     24

#define PEER_FLAG_STAGING     0x01  
#define PEER_FLAG_READY       0x02  
#define PEER_FLAG_PLAYING     0x04  
#define PEER_FLAG_AWAY        0x08  
#define PEER_FLAG_HOST        0x10  
#define PEER_FLAG_OP          0x20
#define PEER_FLAG_VOICE       0x40

#define PEER_KEEP_REPORTING      0  
#define PEER_STOP_REPORTING      1  
#define PEER_REPORT_INFO         2  
#define PEER_REPORT_PLAYERS      4  

typedef void * PEER;

typedef enum
{
	PEERFalse,
	PEERTrue
} PEERBool;

typedef enum
{
	TitleRoom,  
	GroupRoom,  
	StagingRoom,  
	NumRooms
} RoomType;

typedef enum
{
	NormalMessage,
	ActionMessage,
	NoticeMessage
} MessageType;

typedef enum
{
	PEERJoinSuccess,     

	PEERFullRoom,        
	PEERInviteOnlyRoom,  
	PEERBannedFromRoom,  
	PEERBadPassword,     

	PEERAlreadyInRoom,   
	PEERNoTitleSet,      
	PEERNoConnection,    
	PEERAutoMatching,    

	PEERJoinFailed       
} PEERJoinResult;

typedef enum
{
	PEERFailed,      

	PEERSearching,   
	PEERWaiting,     
	PEERStaging,     
	PEERReady,       
	PEERComplete     
} PEERAutoMatchStatus;

typedef void (* peerDisconnectedCallback)
(
	PEER peer,  
	const gsi_char * reason,  
	void * param  
);

typedef void (* peerRoomMessageCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * message,  
	MessageType messageType,  
	void * param  
);

typedef void (* peerRoomUTMCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * command, 
	const gsi_char * parameters,  
	PEERBool authenticated,  
	void * param  
);

typedef void (* peerRoomNameChangedCallback)
(
	PEER peer,  
	RoomType roomType,  
	void * param  
);

typedef void (* peerRoomModeChangedCallback)
(
	PEER peer,  
	RoomType roomType,  
	CHATChannelMode * mode,  
	void * param  
);

typedef void (* peerPlayerMessageCallback)
(
	PEER peer,  
	const gsi_char * nick,  
	const gsi_char * message,  
	MessageType messageType,  
	void * param  
);

typedef void (* peerPlayerUTMCallback)
(
	PEER peer,  
	const gsi_char * nick,  
	const gsi_char * command, 
	const gsi_char * parameters,  
	PEERBool authenticated,  
	void * param  
);

typedef void (* peerReadyChangedCallback)
(
	PEER peer,  
	const gsi_char * nick,  
	PEERBool ready,  
	void * param  
);

typedef void (* peerGameStartedCallback)
(
	PEER peer,  
	SBServer server,  
	const gsi_char * message,  
	void * param  
);

typedef void (* peerPlayerJoinedCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	void * param  
);

typedef void (* peerPlayerLeftCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * reason,  
	void * param  
);

typedef void (* peerKickedCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * reason,  
	void * param  
);

typedef void (* peerNewPlayerListCallback)
(
	PEER peer,  
	RoomType roomType,  
	void * param  
);

typedef void (* peerPlayerChangedNickCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * oldNick,  
	const gsi_char * newNick,  
	void * param  
);

typedef void (* peerPlayerInfoCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	unsigned int IP,  
	int profileID,  
	void * param  
);

typedef void (* peerPlayerFlagsChangedCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	int oldFlags,  
	int newFlags,  
	void * param  
);

typedef void (* peerPingCallback)
(
	PEER peer,  
	const gsi_char * nick,  
	int ping,  
	void * param  
);

typedef void (* peerCrossPingCallback)
(
	PEER peer,  
	const gsi_char * nick1,  
	const gsi_char * nick2,  
	int crossPing,  
	void * param  
);

typedef void (* peerGlobalKeyChangedCallback)
(
	PEER peer,  
	const gsi_char * nick,  
	const gsi_char * key,  
	const gsi_char * value,  
	void * param  
);

typedef void (* peerRoomKeyChangedCallback)
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * key,  
	const gsi_char * value,  
	void * param  
);

typedef void (* peerQRServerKeyCallback)
(
	PEER peer,  
	int key,  
	qr2_buffer_t buffer,  
	void * param  
);

typedef void (* peerQRPlayerKeyCallback)
(
	PEER peer,  
	int key,  
	int index,  
	qr2_buffer_t buffer,  
	void * param  
);

typedef void (* peerQRTeamKeyCallback)
(
	PEER peer,  
	int key,  
	int index,  
	qr2_buffer_t buffer,  
	void * param  
);

typedef void (* peerQRKeyListCallback)
(
	PEER peer,  
	qr2_key_type type,  
	qr2_keybuffer_t keyBuffer,  
	void * param  
);

typedef int (* peerQRCountCallback)
(
	PEER peer,  
	qr2_key_type type,  
	void * param  
);

typedef void (* peerQRAddErrorCallback)
(
	PEER peer,  
	qr2_error_t error,  
	gsi_char * errorString,  
	void * param  
);

typedef void (* peerQRNatNegotiateCallback)
(
	PEER peer,  
	int cookie,  
	void * param  
);

typedef void (* peerQRPublicAddressCallback)
(
	PEER peer,  
	unsigned int ip,  
	unsigned short port,  
	void * param  
);

typedef struct PEERCallbacks
{
	peerDisconnectedCallback disconnected;
	peerRoomMessageCallback roomMessage;
	peerRoomUTMCallback roomUTM;
	peerRoomNameChangedCallback roomNameChanged;  
	peerRoomModeChangedCallback roomModeChanged;  
	peerPlayerMessageCallback playerMessage;
	peerPlayerUTMCallback playerUTM;
	peerReadyChangedCallback readyChanged;
	peerGameStartedCallback gameStarted;
	peerPlayerJoinedCallback playerJoined;
	peerPlayerLeftCallback playerLeft;
	peerKickedCallback kicked;
	peerNewPlayerListCallback newPlayerList;
	peerPlayerChangedNickCallback playerChangedNick;
	peerPlayerInfoCallback playerInfo;  
	peerPlayerFlagsChangedCallback playerFlagsChanged;  
	peerPingCallback ping;
	peerCrossPingCallback crossPing;
	peerGlobalKeyChangedCallback globalKeyChanged;
	peerRoomKeyChangedCallback roomKeyChanged;
	peerQRServerKeyCallback qrServerKey;
	peerQRPlayerKeyCallback qrPlayerKey;
	peerQRTeamKeyCallback qrTeamKey;
	peerQRKeyListCallback qrKeyList;
	peerQRCountCallback qrCount;
	peerQRAddErrorCallback qrAddError;
	peerQRNatNegotiateCallback qrNatNegotiateCallback;
	peerQRPublicAddressCallback qrPublicAddressCallback;
	void * param;
} PEERCallbacks;

#ifndef GSI_UNICODE
#define peerConnect					peerConnectA
#define peerConnectLogin            peerConnectLoginA
#define peerConnectPreAuth          peerConnectPreAuthA
#define peerRetryWithNick			peerRetryWithNickA
#define peerRegisterUniqueNick      peerRegisterUniqueNickA
#define peerSetTitle				peerSetTitleA
#define peerGetTitle				peerGetTitleA
#define peerGetNick					peerGetNickA
#define peerFixNick                 peerFixNickA
#define peerTranslateNick           peerTranslateNickA
#define peerChangeNick				peerChangeNickA
#define peerSetAwayMode				peerSetAwayModeA
#define peerParseQuery				peerParseQueryA
#define peerAuthenticateCDKey		peerAuthenticateCDKeyA
#define peerJoinTitleRoom			peerJoinTitleRoomA
#define peerJoinStagingRoom			peerJoinStagingRoomA
#define peerJoinStagingRoomByChannel	peerJoinStagingRoomByChannelA
#define peerCreateStagingRoom		peerCreateStagingRoomA
#define peerCreateStagingRoomWithSocket	peerCreateStagingRoomWithSocketA
#define peerLeaveRoom				peerLeaveRoomA
#define peerListGroupRooms			peerListGroupRoomsA
#define peerStartListingGames		peerStartListingGamesA
#define peerMessageRoom				peerMessageRoomA
#define peerUTMRoom					peerUTMRoomA
#define peerSetPassword				peerSetPasswordA
#define peerSetRoomName				peerSetRoomNameA
#define peerGetRoomName				peerGetRoomNameA
#define peerGetRoomChannel			peerGetRoomChannelA
#define peerSetTitleRoomChannel		peerSetTitleRoomChannelA
#define peerMessagePlayer			peerMessagePlayerA
#define peerUTMPlayer				peerUTMPlayerA
#define peerKickPlayer				peerKickPlayerA
#define peerGetPlayerPing			peerGetPlayerPingA
#define peerGetPlayersCrossPing		peerGetPlayersCrossPingA
#define peerPingPlayer				peerPingPlayerA 
#define peerGetPlayerInfoNoWait		peerGetPlayerInfoNoWaitA
#define peerGetPlayerInfo			peerGetPlayerInfoA
#define peerGetPlayerProfileID		peerGetPlayerProfileIDA
#define peerGetPlayerIP				peerGetPlayerIPA
#define peerIsPlayerHost			peerIsPlayerHostA
#define peerGetPlayerFlags			peerGetPlayerFlagsA
#define peerGetReady				peerGetReadyA
#define peerStartGame				peerStartGameA
#define peerSetGlobalKeys			peerSetGlobalKeysA
#define peerGetPlayerGlobalKeys		peerGetPlayerGlobalKeysA
#define peerGetRoomGlobalKeys		peerGetRoomGlobalKeysA
#define peerSetRoomKeys				peerSetRoomKeysA
#define peerGetRoomKeys				peerGetRoomKeysA
#define peerSetGlobalWatchKeys		peerSetGlobalWatchKeysA
#define peerSetRoomWatchKeys		peerSetRoomWatchKeysA
#define peerGetGlobalWatchKey		peerGetGlobalWatchKeyA
#define peerGetRoomWatchKey			peerGetRoomWatchKeyA
#define peerStartAutoMatch			peerStartAutoMatchA
#define peerStartAutoMatchWithSocket	peerStartAutoMatchWithSocketA
#else
#define peerConnect					peerConnectW
#define peerConnectLogin            peerConnectLoginW
#define peerConnectPreAuth          peerConnectPreAuthW
#define peerRetryWithNick			peerRetryWithNickW
#define peerRegisterUniqueNick      peerRegisterUniqueNickW
#define peerSetTitle				peerSetTitleW
#define peerGetTitle				peerGetTitleW
#define peerGetNick					peerGetNickW
#define peerFixNick                 peerFixNickW
#define peerTranslateNick           peerTranslateNickW
#define peerChangeNick				peerChangeNickW
#define peerSetAwayMode				peerSetAwayModeW
#define peerParseQuery				peerParseQueryA
#define peerAuthenticateCDKey		peerAuthenticateCDKeyW
#define peerJoinTitleRoom			peerJoinTitleRoomW
#define peerJoinStagingRoom			peerJoinStagingRoomW
#define peerJoinStagingRoomByChannel	peerJoinStagingRoomByChannelW
#define peerCreateStagingRoom		peerCreateStagingRoomW
#define peerCreateStagingRoomWithSocket	peerCreateStagingRoomWithSocketW
#define peerLeaveRoom				peerLeaveRoomW
#define peerListGroupRooms			peerListGroupRoomsW
#define peerStartListingGames		peerStartListingGamesW
#define peerMessageRoom				peerMessageRoomW
#define peerUTMRoom					peerUTMRoomW
#define peerSetPassword				peerSetPasswordW
#define peerSetRoomName				peerSetRoomNameW
#define peerGetRoomName				peerGetRoomNameW
#define peerGetRoomChannel			peerGetRoomChannelW
#define peerSetTitleRoomChannel		peerSetTitleRoomChannelW
#define peerMessagePlayer			peerMessagePlayerW
#define peerUTMPlayer				peerUTMPlayerW
#define peerKickPlayer				peerKickPlayerW
#define peerGetPlayerPing			peerGetPlayerPingW
#define peerGetPlayersCrossPing		peerGetPlayersCrossPingW
#define peerPingPlayer				peerPingPlayerW
#define peerGetPlayerInfoNoWait		peerGetPlayerInfoNoWaitW
#define peerGetPlayerInfo			peerGetPlayerInfoW
#define peerGetPlayerProfileID		peerGetPlayerProfileIDW
#define peerGetPlayerIP				peerGetPlayerIPW
#define peerIsPlayerHost			peerIsPlayerHostW
#define peerGetPlayerFlags			peerGetPlayerFlagsW
#define peerGetReady				peerGetReadyW
#define peerStartGame				peerStartGameW
#define peerSetGlobalKeys			peerSetGlobalKeysW
#define peerGetPlayerGlobalKeys		peerGetPlayerGlobalKeysW
#define peerGetRoomGlobalKeys		peerGetRoomGlobalKeysW
#define peerSetRoomKeys				peerSetRoomKeysW
#define peerGetRoomKeys				peerGetRoomKeysW
#define peerSetGlobalWatchKeys		peerSetGlobalWatchKeysW
#define peerSetRoomWatchKeys		peerSetRoomWatchKeysW
#define peerGetGlobalWatchKey		peerGetGlobalWatchKeyW
#define peerGetRoomWatchKey			peerGetRoomWatchKeyW
#define peerStartAutoMatch			peerStartAutoMatchW
#define peerStartAutoMatchWithSocket	peerStartAutoMatchWithSocketW
#endif

PEER peerInitialize
(
	PEERCallbacks * callbacks  
);

typedef void (* peerConnectCallback)
(
	PEER peer,  
	PEERBool success,  
	int failureReason,  
	void * param  
);

typedef void (* peerNickErrorCallback)
(
	PEER peer,  
	int type,  
	const gsi_char * nick,  
	int numSuggestedNicks,  
	const gsi_char ** suggestedNicks,  
	void * param  
);

void peerConnect
(
	PEER peer,  
	const gsi_char * nick,  
	int profileID,  
	peerNickErrorCallback nickErrorCallback,  
	peerConnectCallback connectCallback,  
	void * param,  
	PEERBool blocking   
);

void peerConnectLogin
(
	PEER peer,  
	int namespaceID,  
	const gsi_char * email,  
	const gsi_char * profilenick,  
	const gsi_char * uniquenick,  
	const gsi_char * password,  
	peerNickErrorCallback nickErrorCallback,  
	peerConnectCallback connectCallback,  
	void * param,  
	PEERBool blocking   
);

void peerConnectPreAuth
(
	PEER peer,  
	const gsi_char * authtoken,  
	const gsi_char * partnerchallenge,  
	peerNickErrorCallback nickErrorCallback,  
	peerConnectCallback connectCallback,  
	void * param,  
	PEERBool blocking   
);

void peerRetryWithNick
(
	PEER peer,
	const gsi_char * nick
);

void peerRegisterUniqueNick
(
	PEER peer,
	int namespaceID,
	const gsi_char * uniquenick,
	const gsi_char * cdkey
);

PEERBool peerIsConnected(PEER peer);

PEERBool peerSetTitle
(
	PEER peer,  
	const gsi_char * title,  
	const gsi_char * qrSecretKey,  
	const gsi_char * sbName,  
	const gsi_char * sbSecretKey,  
	int sbGameVersion,  
	int sbMaxUpdates,  
	PEERBool natNegotiate,  
	PEERBool pingRooms[NumRooms],  
	PEERBool crossPingRooms[NumRooms]  
);

void peerClearTitle(PEER peer);

const gsi_char * peerGetTitle(PEER peer);

void peerDisconnect(PEER peer);

void peerShutdown(PEER peer);

void peerThink(PEER peer);

CHAT peerGetChat(PEER peer);

const gsi_char * peerGetNick(PEER peer);

void peerFixNick
(
	gsi_char * newNick,
	const gsi_char * oldNick
);

const gsi_char * peerTranslateNick
(
	gsi_char * nick,  
	const gsi_char * extension  
);

#define peerGetLocalIP  peerGetPublicIP

unsigned int peerGetPublicIP(PEER peer);

unsigned int peerGetPrivateIP(PEER peer);

int peerGetUserID(PEER peer);

int peerGetProfileID(PEER peer);

typedef void (* peerChangeNickCallback)
(
	PEER peer,  
	PEERBool success,  
	const gsi_char * oldNick,  
	const gsi_char * newNick,  
	void * param  
);

void peerChangeNick
(
	PEER peer,  
	const gsi_char * newNick,  
	peerChangeNickCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerStayInRoom
(
	PEER peer,  
	RoomType roomType  
);

void peerSetQuietMode
(
	PEER peer,  
	PEERBool quiet  
);

void peerSetAwayMode
(
	PEER peer,  
	const gsi_char * reason  
);

void peerParseQuery
(
	PEER peer,  
	char * query,  
	int len,  
	struct sockaddr * sender  
);

typedef void (* peerAuthenticateCDKeyCallback)
(
	PEER peer,  
	int result,  
	const gsi_char * message,  
	void * param  
);

void peerAuthenticateCDKey
(
	PEER peer,  
	const gsi_char * cdkey,  
	peerAuthenticateCDKeyCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerSendNatNegotiateCookie
(
	PEER peer,  
	unsigned int ip,  
	unsigned short port,  
	int cookie  
);

void peerSendMessageToServer
(
	PEER peer,
	unsigned int ip,
	unsigned short port,
	const char * data,
	int len
);

void peerAlwaysGetPlayerInfo
(
	PEER peer,  
	PEERBool always  
);

typedef void (* peerJoinRoomCallback)
(
	PEER peer,  
	PEERBool success,  
	PEERJoinResult result,  
	RoomType roomType,  
	void * param  
);

void peerJoinTitleRoom
(
	PEER peer,  
	const gsi_char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerJoinGroupRoom
(
	PEER peer,  
	int groupID,  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking 
);

void peerSetGroupID
(
	PEER peer,  
	int groupID  
);

int peerGetGroupID
(
	PEER peer  
);

void peerJoinStagingRoom
(
	PEER peer,  
	SBServer server,  
	const gsi_char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking   
);

void peerJoinStagingRoomByChannel
(
	PEER peer,  
	const gsi_char * channel,  
	const gsi_char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking   
);

void peerCreateStagingRoom
(
	PEER peer,  
	const gsi_char * name,  
	int maxPlayers,  
	const gsi_char password[PEER_PASSWORD_LEN],  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking 
);

void peerCreateStagingRoomWithSocket
(
	PEER peer,  
	const gsi_char * name,  
	int maxPlayers,  
	const gsi_char password[PEER_PASSWORD_LEN],  
	SOCKET socket,  
	unsigned short port,  
	peerJoinRoomCallback callback,  
	void * param,  
	PEERBool blocking 
);

void peerLeaveRoom
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * reason  
);

typedef void (* peerListGroupRoomsCallback)
(
	PEER peer,  
	PEERBool success,  
	int groupID,  
	SBServer server,  
	const gsi_char * name,  
	int numWaiting,  
	int maxWaiting,  
	int numGames,  
	int numPlaying,  
	void * param  
);

void peerListGroupRooms
(
	PEER peer,  
	const gsi_char * fields,  
	peerListGroupRoomsCallback callback,  
	void * param,  
	PEERBool blocking  
);

typedef void (* peerListingGamesCallback)
(
	PEER peer,  
	PEERBool success,  
	const gsi_char * name,  
	SBServer server,  
	PEERBool staging,  
	int msg,  
	int progress,  
	void * param  
);

void peerStartListingGames
(
	PEER peer,  
	const unsigned char * fields,  
	int numFields,  
	const gsi_char * filter,  
	peerListingGamesCallback callback,  
	void * param  
);

void peerStopListingGames
(
	PEER peer  
);

void peerUpdateGame
(
	PEER peer,  
	SBServer server,  
	PEERBool fullUpdate  
);

void peerUpdateGameByMaster
(
	PEER peer,  
	SBServer server,  
	PEERBool fullUpdate  
);

void peerUpdateGamePing(PEER peer, SBServer server);

void peerMessageRoom
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * message,  
	MessageType messageType  
);

void peerUTMRoom
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * command,  
	const gsi_char * parameters,  
	PEERBool authenticate  
);

void peerSetPassword
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char password[PEER_PASSWORD_LEN]  
);

void peerSetRoomName
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * name  
);

const gsi_char * peerGetRoomName
(
	PEER peer,  
	RoomType roomType  
);

const gsi_char * peerGetRoomChannel
(
	PEER peer,  
	RoomType roomType  
);

PEERBool peerInRoom
(
	PEER peer,  
	RoomType roomType  
);

void peerSetTitleRoomChannel
(
	PEER peer,  
	const gsi_char * channel  
);

SBServer peerGetHostServer(PEER peer);

void peerSetStagingRoomMaxPlayers
(
	PEER peer,
	int maxPlayers
);

typedef void (* peerEnumPlayersCallback)
(
	PEER peer,  
	PEERBool success,  
	RoomType roomType,  
	int index,  
	const gsi_char * nick,  
	int flags,  
	void * param  
);

void peerEnumPlayers
(
	PEER peer,  
	RoomType roomType,  
	peerEnumPlayersCallback callback,  
	void * param  
);

void peerMessagePlayer
(
	PEER peer,  
	const gsi_char * nick,  
	const gsi_char * message,  
	MessageType messageType  
);

void peerUTMPlayer
(
	PEER peer,  
	const gsi_char * nick,  
	const gsi_char * command,  
	const gsi_char * parameters,  
	PEERBool authenticate  
);

void peerKickPlayer
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * reason  
);

PEERBool peerGetPlayerPing
(
	PEER peer,  
	const gsi_char * nick,  
	int * ping  
);

PEERBool peerGetPlayersCrossPing
(
	PEER peer,  
	const gsi_char * nick1,  
	const gsi_char * nick2,  
	int * crossPing  
);

PEERBool peerPingPlayer
(
	PEER peer,  
	const gsi_char * nick  
);

PEERBool peerGetPlayerInfoNoWait
(
	PEER peer,
	const gsi_char * nick,
	unsigned int * IP,
	int * profileID
);

typedef void (* peerGetPlayerInfoCallback)
(
	PEER peer,  
	PEERBool success,  
	const gsi_char * nick,  
	unsigned int IP,  
	int profileID,  
	void * param  
);

void peerGetPlayerInfo
(
	PEER peer,  
	const gsi_char * nick,  
	peerGetPlayerInfoCallback callback,  
	void * param,  
	PEERBool blocking  
);

typedef void (* peerGetPlayerProfileIDCallback)
(
	PEER peer,  
	PEERBool success,  
	const gsi_char * nick,  
	int profileID,  
	void * param  
);

void peerGetPlayerProfileID
(
	PEER peer,  
	const gsi_char * nick,  
	peerGetPlayerProfileIDCallback callback,  
	void * param,  
	PEERBool blocking  
);

typedef void (* peerGetPlayerIPCallback)
(
	PEER peer,  
	PEERBool success,  
	const gsi_char * nick,  
	unsigned int IP,  
	void * param  
);

void peerGetPlayerIP
(
	PEER peer,  
	const gsi_char * nick,  
	peerGetPlayerIPCallback callback,  
	void * param,  
	PEERBool blocking  
);

PEERBool peerIsPlayerHost
(
	PEER peer,  
	const gsi_char * nick,  
	RoomType roomType  
);

PEERBool peerGetPlayerFlags
(
	PEER peer,
	const gsi_char * nick,
	RoomType roomType,
	int * flags
);


void peerSetReady
(
	PEER peer,  
	PEERBool ready  
);

PEERBool peerGetReady
(
	PEER peer,  
	const gsi_char * nick, 
	PEERBool * ready  
);

PEERBool peerAreAllReady
(
	PEER peer  
);

void peerStartGame
(
	PEER peer,  
	const gsi_char * message,  
	int reportingOptions  
);

PEERBool peerStartReporting
(
	PEER peer  
);

PEERBool peerStartReportingWithSocket
(
	PEER peer,  
	SOCKET socket,  
	unsigned short port  
);

void peerStartPlaying
(
	PEER peer  
);

PEERBool peerIsPlaying
(
	PEER peer  
);

void peerStopGame
(
	PEER peer  
);

void peerStateChanged
(
	PEER peer  
);

void peerSetGlobalKeys
(
	PEER peer,  
	int num,  
	const gsi_char ** keys,  
	const gsi_char ** values  
);

typedef void (* peerGetGlobalKeysCallback)
(
	PEER peer,  
	PEERBool success,  
	const gsi_char * nick,  
	int num,  
	const gsi_char ** keys,  
	const gsi_char ** values,  
	void * param  
);

void peerGetPlayerGlobalKeys
(
	PEER peer,  
	const gsi_char * nick,  
	int num,  
	const gsi_char ** keys,  
	peerGetGlobalKeysCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerGetRoomGlobalKeys
(
	PEER peer,  
	RoomType roomType,  
	int num,  
	const gsi_char ** keys,  
	peerGetGlobalKeysCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerSetRoomKeys
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	int num,  
	const gsi_char ** keys,  
	const gsi_char ** values  
);

typedef void (* peerGetRoomKeysCallback)
(
	PEER peer,  
	PEERBool success,  
	RoomType roomType,  
	const gsi_char * nick,  
	int num,  
	gsi_char ** keys,  
	gsi_char ** values,  
	void * param  
);

void peerGetRoomKeys
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	int num,  
	const gsi_char ** keys,  
	peerGetRoomKeysCallback callback,  
	void * param,  
	PEERBool blocking  
);

void peerSetGlobalWatchKeys
(
	PEER peer,  
	RoomType roomType,  
	int num,  
	const gsi_char ** keys,  
	PEERBool addKeys  
);

void peerSetRoomWatchKeys
(
	PEER peer,  
	RoomType roomType,  
	int num,  
	const gsi_char ** keys,  
	PEERBool addKeys  
);

const gsi_char * peerGetGlobalWatchKey
(
	PEER peer,  
	const gsi_char * nick,  
	const gsi_char * key  
);

const gsi_char * peerGetRoomWatchKey
(
	PEER peer,  
	RoomType roomType,  
	const gsi_char * nick,  
	const gsi_char * key  
);


typedef void (* peerAutoMatchStatusCallback)
(
	PEER peer,  
	PEERAutoMatchStatus status,  
	void * param  
);

typedef int (* peerAutoMatchRateCallback)
(
	PEER peer,  
	SBServer match,  
	void * param  
);

void peerStartAutoMatch
(
	PEER peer,  
	int maxPlayers,  
	const gsi_char * filter,  
	peerAutoMatchStatusCallback statusCallback,  
	peerAutoMatchRateCallback rateCallback,  
	void * param,  
	PEERBool blocking   
);

void peerStartAutoMatchWithSocket
(
	PEER peer,  
	int maxPlayers,  
	const gsi_char * filter,  
	SOCKET socket,  
	unsigned short port,  
	peerAutoMatchStatusCallback statusCallback,  
	peerAutoMatchRateCallback rateCallback,  
	void * param,  
	PEERBool blocking   
);

void peerStopAutoMatch(PEER peer);

PEERBool peerIsAutoMatching(PEER peer);

PEERAutoMatchStatus peerGetAutoMatchStatus(PEER peer);

#ifdef __cplusplus
}
#endif

#endif
