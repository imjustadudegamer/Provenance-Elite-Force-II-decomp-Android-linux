/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#include "../peer.h"
#include "../../common/gsStringUtil.h"
#include "../../common/gsAvailable.h"

#define TITLE		_T("gmtest")

#ifdef UNDER_CE
	void RetailOutputA(CHAR *tszErr, ...);
	#define printf RetailOutputA
#elif defined(_NITRO)
	#include "../../common/nitro/screen.h"
	#define printf Printf
	#define vprintf VPrintf
#endif

#define LOOP_SECONDS 60   

#define NICK_SIZE 32   

gsi_char nick[NICK_SIZE]; 

gsi_char RtoS[3][16] =
{
	_T("TitleRoom"),
	_T("GroupRoom"),
	_T("StagingRoom")
};

PEERBool groupRoomCallbackDone = PEERFalse;  


static const gsi_char * ResultToString (PEERJoinResult result)
{
	switch(result)
	{
	case PEERJoinSuccess:
		return _T("Success");
	case PEERFullRoom:
		return _T("Full");
	case PEERInviteOnlyRoom:
		return _T("Invite Only");
	case PEERBannedFromRoom:
		return _T("Banned");
	case PEERBadPassword:
		return _T("Bad Password");
	case PEERAlreadyInRoom:
		return _T("Already in room");
	case PEERNoTitleSet:
		return _T("No Title");
	case PEERNoConnection:
		return _T("No Connection");
	case PEERAutoMatching:
		return _T("Auto Matching");
	case PEERJoinFailed:
		return _T("Join Failed");
	}

	return _T("<UNKNOWN RESULT>");
}

static void JoinRoomCallback
(
 PEER peer,
 PEERBool success,
 PEERJoinResult result,
 RoomType roomType,
 void * param
 )
{
	if(!success)
	{
		_tprintf(_T("Join failure: %s"), ResultToString(result));
		return;
	}
	else
	{
		_tprintf(_T("Joined %s.\n"), RtoS[roomType]);
	}

	GSI_UNUSED(param);
	GSI_UNUSED(roomType);
	GSI_UNUSED(peer);
}

static void CreateStagingRoomCallback
(
 PEER peer,
 PEERBool success,
 PEERJoinResult result,
 RoomType roomType,
 void * param
 )
{
	if (success)
		_tprintf(_T("Staging Room Created\n"));

	GSI_UNUSED(param);
	GSI_UNUSED(roomType);
	GSI_UNUSED(result);
	GSI_UNUSED(peer);
}

static void DisconnectedCallback
(
	PEER peer,
	const gsi_char * reason,
	void * param
)
{
	_tprintf(_T("Disconnected: %s\n"), reason);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void RoomMessageCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * nick,
	const gsi_char * message,
	MessageType messageType,
	void * param
)
{
	_tprintf(_T(" (%s) %s: %s\n"), RtoS[roomType], nick, message);

	GSI_UNUSED(param);
	GSI_UNUSED(peer);
	GSI_UNUSED(messageType);
}

static void RoomUTMCallback
(
	PEER peer, 
	RoomType roomType, 
	const gsi_char * nick, 
	const gsi_char * command, 
	const gsi_char * parameters,
	PEERBool authenticated, 
	void * param 
)
{
	GSI_UNUSED(peer);
	GSI_UNUSED(roomType);
	GSI_UNUSED(nick);
	GSI_UNUSED(command);
	GSI_UNUSED(parameters);
	GSI_UNUSED(authenticated);
	GSI_UNUSED(param);
}

static void PlayerMessageCallback
(
	PEER peer,
	const gsi_char * nick,
	const gsi_char * message,
	MessageType messageType,
	void * param
)
{
	_tprintf(_T("(PRIVATE) %s: %s\n"), nick, message);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(messageType);
	GSI_UNUSED(param);
}

static void ReadyChangedCallback
(
	PEER peer,
	const gsi_char * nick,
	PEERBool ready,
	void * param
)
{
	if(ready)
		_tprintf(_T("%s is ready\n"), nick);
	else
		_tprintf(_T("%s is not ready\n"), nick);

	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void GameStartedCallback
(
	PEER peer,
	SBServer server,
	const gsi_char * message,
	void * param
)
{
	_tprintf(_T("The game is starting at %s\n"), SBServerGetPublicAddress(server));
	if(message && message[0])
		_tprintf(_T(": %s\n"), message);
	else
		_tprintf(_T("\n"));
		
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void PlayerJoinedCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * nick,
	void * param
)
{
	_tprintf(_T("%s joined the %s\n"), nick, RtoS[roomType]);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void PlayerLeftCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * nick,
	const gsi_char * reason,
	void * param
)
{
	_tprintf(_T("%s left the %s\n"), nick, RtoS[roomType]);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(reason);
}

static void PlayerChangedNickCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * oldNick,
	const gsi_char * newNick,
	void * param
)
{
	_tprintf(_T("%s in %s changed nicks to %s\n"), oldNick, RtoS[roomType], newNick);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

PEERBool connectSuccess;
static void ConnectCallback
(
	PEER peer,
	PEERBool success,
	int failureReason,
	void * param
)
{
	connectSuccess = success;
	
	GSI_UNUSED(peer);
	GSI_UNUSED(success);
	GSI_UNUSED(failureReason);
	GSI_UNUSED(param);
}

static void EnumPlayersCallback
(
	PEER peer,
	PEERBool success,
	RoomType roomType,
	int index,
	const gsi_char * nick,
	int flags,
	void * param
)
{
	if(!success)
	{
		_tprintf(_T("Enum %s players failed\n"), RtoS[roomType]);
		return;
	}

	if(index == -1)
	{
		_tprintf(_T("--------------------\n"));
		return;
	}

	_tprintf(_T("%d: %s\n"), index, nick);
		

	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(flags);
}
static void ListGroupRoomsCallback
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
)
{
	if(success && (groupID > 0))
	{
		_tprintf(_T("  %s\n"), name);
		_tprintf(_T("    Players in room: %d\n"), numWaiting);
		_tprintf(_T("    Max players in room: %d\n"), maxWaiting);
		_tprintf(_T("    Games: %d\n"), numGames);
		_tprintf(_T("    Players in games: %d\n"), numPlaying);
	}
	else
		groupRoomCallbackDone = PEERTrue;  

	GSI_UNUSED(peer);
	GSI_UNUSED(server);
	GSI_UNUSED(numWaiting);
	GSI_UNUSED(maxWaiting);
	GSI_UNUSED(numGames);
	GSI_UNUSED(numPlaying);
	GSI_UNUSED(param);
}

PEERBool joinSuccess;
static void JoinCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	joinSuccess = success;

	GSI_UNUSED(peer);
	GSI_UNUSED(result);
	GSI_UNUSED(roomType);
	GSI_UNUSED(param);
}

static void NickErrorCallback
(
	PEER peer,
	int type,
	const gsi_char * badNick,
	int numSuggestedNicks,
	const gsi_char ** suggestedNicks,
	void * param
)
{
	static int errCount;

	if(errCount < 20)
	{
		_tsnprintf(nick,NICK_SIZE,_T("peerc%u"),(unsigned int)current_time());
		nick[NICK_SIZE - 1] = '\0';
		peerRetryWithNick(peer, nick);
	}
	else
	{
		peerRetryWithNick(peer, NULL);
	}
	errCount++;
	
	GSI_UNUSED(type);
	GSI_UNUSED(badNick);
	GSI_UNUSED(suggestedNicks);
	GSI_UNUSED(numSuggestedNicks);
	GSI_UNUSED(param);
}
static void RoomKeyChangedCallback(PEER thePeer, RoomType theType, const gsi_char* theNick, const gsi_char* theKey, const gsi_char* theValue, void* theParam)
{
	GSI_UNUSED(thePeer);
	GSI_UNUSED(theType);
	GSI_UNUSED(theNick);
	GSI_UNUSED(theKey);
	GSI_UNUSED(theValue);
	GSI_UNUSED(theParam);
}

static void QRServerKeyCallback
(
	PEER peer,
	int key,
	qr2_buffer_t buffer,
	void * param
)
{
#if VERBOSE
	gsi_char verbose[256];
	_stprintf(verbose, _T("QR_SERVER_KEY | %d\n"), key);
	OutputDebugString(verbose);
#endif

	switch(key)
	{
	case HOSTNAME_KEY:
		_tprintf(_T(" Server Key callback is being called for HOSTNAME_KEY\n"));
		qr2_buffer_add(buffer, _T("My Game"));
		break;
	case NUMPLAYERS_KEY:
		_tprintf(_T(" Server Key callback is being called for NUMPLAYERS_KEY\n"));
		qr2_buffer_add_int(buffer, 1);
		break;
	case GAMEMODE_KEY:
		_tprintf(_T(" Server Key callback is being called for GAMEMODE_KEY\n"));
		qr2_buffer_add(buffer, _T("openplaying"));
		break;
	case HOSTPORT_KEY:
		_tprintf(_T(" Server Key callback is being called for HOSTPORT_KEY\n"));
		qr2_buffer_add_int(buffer, 15151);
		break;
	case MAPNAME_KEY:
		_tprintf(_T(" Server Key callback is being called for MAPNAME_KEY\n"));
		qr2_buffer_add(buffer, _T("Big Crate Room"));
		break;
	case GAMETYPE_KEY:
		_tprintf(_T(" Server Key callback is being called for GAMETYPE_KEY\n"));
		qr2_buffer_add(buffer, _T("Friendly"));
		break;
	case TIMELIMIT_KEY:
		_tprintf(_T(" Server Key callback is being called for TIMELIMIT_KEY\n"));
		qr2_buffer_add_int(buffer, 100);
		break;
	case FRAGLIMIT_KEY:
		_tprintf(_T(" Server Key callback is being called for FRAGLIMIT_KEY\n"));
		qr2_buffer_add_int(buffer, 0);
		break;
	case TEAMPLAY_KEY:
		_tprintf(_T(" Server Key callback is being called for TEAMPLAY_KEY\n"));
		qr2_buffer_add_int(buffer, 0);
		break;
	default:
		qr2_buffer_add(buffer, _T(""));
		break;
	}
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void QRKeyListCallback
(
	PEER peer,
	qr2_key_type type,
	qr2_keybuffer_t keyBuffer,
	void * param
)
{
	switch(type)
	{
	case key_server:
		_tprintf(_T(" Key List Callback is being called for server keys\n"));
		if(!peerIsAutoMatching(peer))
		{
			qr2_keybuffer_add(keyBuffer, HOSTPORT_KEY);
			qr2_keybuffer_add(keyBuffer, MAPNAME_KEY);
			qr2_keybuffer_add(keyBuffer, GAMETYPE_KEY);
			qr2_keybuffer_add(keyBuffer, TIMELIMIT_KEY);
			qr2_keybuffer_add(keyBuffer, FRAGLIMIT_KEY);
			qr2_keybuffer_add(keyBuffer, TEAMPLAY_KEY);
		}
		break;
	case key_player:
		_tprintf(_T(" Key List Callback is being called for player keys\n"));
		break;
	case key_team:
		_tprintf(_T(" Key List Callback is being called for team keys\n"));
		break;
	default: break;
	}
	
	GSI_UNUSED(param);
}




#ifdef __MWERKS__ 
	int test_main(int argc, char **argv);
#endif

int test_main(int argc, char **argv)
{
	PEER peer;  

	PEERCallbacks callbacks;  

	gsi_char  secret_key[9];      
	int maxUpdates = 10;  
	PEERBool natNeg = PEERFalse;  
	PEERBool pingRooms[NumRooms];  
	PEERBool crossPingRooms[NumRooms];

	int profileID = 0;  
	void * userData = NULL;  
	PEERBool blocking = PEERTrue; 
	PEERBool non_blocking = PEERFalse; 

	int newbiesGroupID = 2;

	gsi_char * serverName = _T("UberNoobServer");
	int maxPlayers = 2;
	gsi_char * noPassword = _T("");

	GSIACResult result; 
	gsi_time startTime;

	_tsnprintf(nick,NICK_SIZE,_T("peerc%u"),(unsigned int)current_time());
	nick[NICK_SIZE - 1] = '\0';

	secret_key[0] = 'H';
	secret_key[1] = 'A';
	secret_key[2] = '6';
	secret_key[3] = 'z';
	secret_key[4] = 'k';
	secret_key[5] = 'S';
	secret_key[6] = '\0';

	GSIStartAvailableCheck(_T("gmtest"));
	while((result = GSIAvailableCheckThink()) == GSIACWaiting)
		msleep(5);
	if(result != GSIACAvailable)
	{
		_tprintf(_T("The backend is not available\n"));
		return 1;
	}

	memset(&callbacks, 0, sizeof(PEERCallbacks));
	callbacks.disconnected = DisconnectedCallback;
	callbacks.gameStarted = GameStartedCallback;
	callbacks.playerChangedNick = PlayerChangedNickCallback;
	callbacks.playerJoined = PlayerJoinedCallback;
	callbacks.playerLeft = PlayerLeftCallback;
	callbacks.readyChanged = ReadyChangedCallback;
	callbacks.roomMessage = RoomMessageCallback;
	callbacks.playerMessage = PlayerMessageCallback;
	callbacks.roomUTM = RoomUTMCallback;
	callbacks.roomKeyChanged = RoomKeyChangedCallback;
	callbacks.qrKeyList = QRKeyListCallback;
	callbacks.qrServerKey = QRServerKeyCallback;
	callbacks.param = NULL;

	peer = peerInitialize(&callbacks);
	if(!peer)
	{
		_tprintf(_T("Failed to init\n"));
		return 1;
	}

	pingRooms[TitleRoom] = PEERTrue;
	pingRooms[GroupRoom] = PEERTrue;
	pingRooms[StagingRoom] = PEERTrue;
	crossPingRooms[TitleRoom] = PEERTrue;
	crossPingRooms[GroupRoom] = PEERTrue;
	crossPingRooms[StagingRoom] = PEERTrue;

	if(!peerSetTitle(peer, TITLE, secret_key, TITLE, secret_key, 0, maxUpdates, natNeg, pingRooms, crossPingRooms))
	{
		peerShutdown(peer);
		_tprintf(_T("Failed to set the title\n"));
		return 1;
	}

	_tprintf(_T("Connecting as %s..."), nick);
	peerConnect(peer, nick, profileID, NickErrorCallback, ConnectCallback, userData, blocking);
	if(!connectSuccess)
	{
		peerShutdown(peer);
		_tprintf(_T("Failed to connect\n"));
		return 1;
	}
	printf("Connected\n\n");

	printf("Joining title room...");
	peerJoinTitleRoom(peer, NULL, JoinCallback, NULL, PEERTrue);
	if(!joinSuccess)
	{
		peerDisconnect(peer);
		peerShutdown(peer);
		_tprintf(_T("Failed to join the title room\n"));
		return 1;
	}
	printf("Joined\n\n");

	printf("Listing group rooms:\n");
	peerListGroupRooms(peer, _T(""), ListGroupRoomsCallback, userData, non_blocking);

	while (!groupRoomCallbackDone)
	{
		peerThink(peer);
		msleep(10);
	}

	printf("\nSending message to the Title Room...\n");
	peerMessageRoom(peer, TitleRoom, _T("Hi everyone in the Title Room!\n"), NormalMessage);

	startTime = current_time();
	while((current_time() - startTime) < (1000))
	{
		peerThink(peer);
		msleep(10);
	} 

	_tprintf(_T("\nJoining Group Room 'Newbies'..."));
	peerJoinGroupRoom(peer, newbiesGroupID, JoinRoomCallback, userData, blocking);

	_tprintf(_T("\nPlayers in Group Room: \n"));
	peerEnumPlayers(peer, GroupRoom, EnumPlayersCallback, NULL);

	_tprintf(_T("Hosting Staging Room...\n"));
	peerCreateStagingRoom(peer, serverName, maxPlayers, noPassword, CreateStagingRoomCallback, userData, blocking);

	startTime = current_time();
	while((current_time() - startTime) < (1000))
	{
		peerThink(peer);
		msleep(10);
	}



	peerLeaveRoom(peer, TitleRoom, NULL);


	peerDisconnect(peer);

	peerShutdown(peer); 

	GSI_UNUSED(argc);
	GSI_UNUSED(argv);
	return 0;
}
