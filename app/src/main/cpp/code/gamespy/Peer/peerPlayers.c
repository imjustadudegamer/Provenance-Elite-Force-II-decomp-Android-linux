
#include <string.h>
#include "peerPlayers.h"
#include "peerPing.h"
#include "peerCallbacks.h"
#include "peerKeys.h"

#define PI_PLAYERS_NUM_BUCKETS     32
#define PI_PLAYER_INFO_SIZE        (sizeof(piPlayer) - PI_NICK_MAX_LEN)
#define PI_PLAYER_INFO(player)     ((char *)(player) + PI_NICK_MAX_LEN)

static int piPlayersTableHashFn
(
	const void *elem,
	int numBuckets
)
{
	piPlayer * player = (piPlayer *)elem;
	int c;
	const char * str;
	unsigned int hash;

	assert(player);
	assert(player->nick[0]);

	str = player->nick;
	hash = 0;
	while((c = *str++) != '\0')
		hash += (unsigned int)tolower(c);
	hash %= (unsigned int)numBuckets;

	return (int)hash;
}

static int GS_STATIC_CALLBACK piPlayersTableCompareFn
(
	const void *elem1,
	const void *elem2
)
{
	piPlayer * player1 = (piPlayer *)elem1;
	piPlayer * player2 = (piPlayer *)elem2;
	assert(player1);
	assert(player1->nick[0]);
	assert(player2);
	assert(player2->nick[0]);

	return strcasecmp(player1->nick, player2->nick);
}

static void piPlayersTableElementFreeFn
(
	void *elem
)
{
	piPlayer * player = (piPlayer *)elem;
	assert(player);
	assert(player->nick);
	GSI_UNUSED(player);
}

PEERBool piPlayersInit
(
	PEER peer
)
{
	int i;

	PEER_CONNECTION;

	if(connection->stayInTitleRoom)
		return PEERTrue;

	connection->players = TableNew(sizeof(piPlayer), PI_PLAYERS_NUM_BUCKETS, piPlayersTableHashFn, piPlayersTableCompareFn, piPlayersTableElementFreeFn);
	if(!connection->players)
		return PEERFalse;

	for(i = 0 ; i < NumRooms ; i++)
		connection->numPlayers[i] = 0;

	return PEERTrue;
}

void piPlayersCleanup
(
	PEER peer
)
{
	int i;

	PEER_CONNECTION;

	if(connection->stayInTitleRoom)
		return;

	if(connection->players)
		TableFree(connection->players);
	connection->players = NULL;
	for(i = 0 ; i < NumRooms ; i++)
		connection->numPlayers[i] = 0;
}

static piPlayer * piAddPlayer
(
	PEER peer,
	const char * nick,
	PEERBool initialize
)
{
	piPlayer * player;
	piPlayer playerMatch;

	PEER_CONNECTION;

	assert(!piGetPlayer(peer, nick));

	player = &playerMatch;
	memset(player, 0, sizeof(piPlayer));
	strzcpy(player->nick, nick, PI_NICK_MAX_LEN);
	if(initialize)
	{
		int roomType;

		player->local = (PEERBool)(strcasecmp(nick, connection->nick) == 0);

		for(roomType = 0 ; roomType < NumRooms ; roomType++)
		{
			player->inRoom[roomType] = PEERFalse;
			player->flags[roomType] = 0;
		}

		player->IP = 0;
		player->profileID = 0;
		player->gotIPAndProfileID = PEERFalse;

		if(!piPingInitPlayer(peer, player))
			return NULL;
	}

	TableEnter(connection->players, player);

	player = piGetPlayer(peer, nick);
	assert(player);

	return player;
}

static void piRemovePlayer
(
	PEER peer,
	piPlayer * player
)
{
	PEER_CONNECTION;

	assert(player);

	TableRemove(connection->players, player);
}

piPlayer * piPlayerJoinedRoom
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	int mode
)
{
	piPlayer * player;

	PEER_CONNECTION;
	
	assert(nick);
	assert(nick[0]);
	ASSERT_ROOMTYPE(roomType);
	assert(IN_ROOM || ENTERING_ROOM);

	if(!ENTERING_ROOM && !IN_ROOM)
		return NULL;

	player = piGetPlayer(peer, nick);

	if(!player)
	{
		player = piAddPlayer(peer, nick, PEERTrue);
	}

	assert(!player->inRoom[roomType]);
	player->inRoom[roomType] = PEERTrue;
	connection->numPlayers[roomType]++;
	player->flags[roomType] = 0;
	if(mode & CHAT_OP)
		player->flags[roomType] |= PEER_FLAG_OP;
	if(mode & CHAT_VOICE)
		player->flags[roomType] |= PEER_FLAG_VOICE;

	piPingPlayerJoinedRoom(peer, player, roomType);

	return player;
}

void piPlayerLeftRoom
(
	PEER peer,
	const char * nick,
	RoomType roomType
)
{
	piPlayer * player;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);
	ASSERT_ROOMTYPE(roomType);

	player = piGetPlayer(peer, nick);
	assert(player);
	if(!player)
		return;

	assert(player->inRoom[roomType]);
	player->inRoom[roomType] = PEERFalse;
	connection->numPlayers[roomType]--;
	player->flags[roomType] = 0;

	piPingPlayerLeftRoom(peer, player);

	if(!player->inRoom[0] && !player->inRoom[1] && !player->inRoom[2])
	{
		piRemovePlayer(peer, player);
	}

	piKeyCacheCleanse(peer);
}

void piPlayerChangedNick
(
	PEER peer,
	const char * oldNick,
	const char * newNick
)
{
	char playerInfoBuffer[PI_PLAYER_INFO_SIZE];
	piPlayer * player;

	assert(oldNick);
	assert(oldNick[0]);
	assert(newNick);
	assert(newNick[0]);

	player = piGetPlayer(peer, oldNick);

	if(!player)
		return;

	memcpy(playerInfoBuffer, PI_PLAYER_INFO(player), PI_PLAYER_INFO_SIZE);

	piRemovePlayer(peer, player);

	player = piAddPlayer(peer, newNick, PEERFalse);
	assert(player);
	if(!player)
		return;

	memcpy(PI_PLAYER_INFO(player), playerInfoBuffer, PI_PLAYER_INFO_SIZE);

	piKeyCachePlayerChangedNick(peer, oldNick, newNick);
}

typedef struct piLeftRoomData
{
	PEER peer;
	RoomType roomType;
} piLeftRoomData;

static void piLeftRoomMapFn
(
	void *elem,
	void *clientdata
)
{
	piPlayer * player = (piPlayer *)elem;
	piLeftRoomData * data = (piLeftRoomData *)clientdata;
	assert(player);
	assert(player->nick);
	assert(data);

	if(player->inRoom[data->roomType])
	{
		piPlayerLeftRoom(data->peer, player->nick, data->roomType);
	}
}

void piClearRoomPlayers
(
	PEER peer,
	RoomType roomType
)
{
	piLeftRoomData data;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);

	data.peer = peer;
	data.roomType = roomType;
	TableMapSafe(connection->players, piLeftRoomMapFn, &data);
}

piPlayer * piGetPlayer
(
	PEER peer,
	const char * nick
)
{
	piPlayer playerMatch;
	piPlayer * player;

	PEER_CONNECTION;

	assert(nick);
	assert(nick[0]);

	if(!connection->players)
		return NULL;

	strzcpy(playerMatch.nick, nick, PI_NICK_MAX_LEN);
	player = (piPlayer *)TableLookup(connection->players, &playerMatch);

	return player;
}

typedef struct piEnumRoomPlayersData
{
	PEER peer;
	RoomType roomType;
	int count;
	piEnumRoomPlayersCallback callback;
	void * param;
} piEnumRoomPlayersData;

static void piEnumRoomPlayersMap
(
	void *elem,
	void *clientdata
)
{
	piPlayer * player = (piPlayer *)elem;
	piEnumRoomPlayersData * data = (piEnumRoomPlayersData *)clientdata;
	if(player->inRoom[data->roomType])
	{
		data->callback(data->peer, data->roomType, player, data->count, data->param);

		data->count++;
	}
}

void piEnumRoomPlayers
(
	PEER peer,
	RoomType roomType,
	piEnumRoomPlayersCallback callback,
	void * param
)
{
	piEnumRoomPlayersData data;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);
	assert(callback);

	data.peer = peer;
	data.roomType = roomType;
	data.count = 0;
	data.callback = callback;
	data.param = param;

	TableMap(connection->players, piEnumRoomPlayersMap, &data);

	callback(peer, roomType, NULL, -1, param);
}

static int piFindPlayersByIPMap
(
	void * elem,
	void * clientdata
)
{
	piPlayer * player = (piPlayer *)elem;
	unsigned int IP;

	IP = *(unsigned int *)clientdata;

	if(player->gotIPAndProfileID && (player->IP == IP))
		return 0;

	return 1;
}

piPlayer * piFindPlayerByIP
(
	PEER peer,
	unsigned int IP
)
{
	PEER_CONNECTION;

	return (piPlayer *)TableMap2(connection->players, piFindPlayersByIPMap, &IP);
}

void piSetPlayerIPAndProfileID
(
	PEER peer,
	const char * nick,
	unsigned int IP,
	int profileID
)
{
	piPlayer * player;

	assert(nick);
	assert(nick[0]);

	if(!nick)
		return;

	player = piGetPlayer(peer, nick);
	if(player)
	{
		player->IP = IP;
		player->profileID = profileID;

		if(!player->gotIPAndProfileID)
		{
			player->gotIPAndProfileID = PEERTrue;

			if(player->inRoom[TitleRoom])
				piPingPlayerJoinedRoom(peer, player, TitleRoom);
			if(player->inRoom[GroupRoom])
				piPingPlayerJoinedRoom(peer, player, GroupRoom);
			if(player->inRoom[StagingRoom])
				piPingPlayerJoinedRoom(peer, player, StagingRoom);
		}

	}
}

static void piSetNewPlayerFlags
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	int flags
)
{
	piPlayer * player;
	int oldFlags;

	assert(nick);
	assert(nick[0]);

	if(!nick)
		return;

	player = piGetPlayer(peer, nick);
	if(!player || !player->inRoom[roomType])
		return;

	oldFlags = player->flags[roomType];

	if(flags == oldFlags)
		return;

	player->flags[roomType] = flags;

	if((roomType == StagingRoom) && ((oldFlags & PEER_FLAG_READY) != (flags & PEER_FLAG_READY)))
		piAddReadyChangedCallback(peer, player->nick, (PEERBool)((player->flags[roomType] & PEER_FLAG_READY) != 0));

	piAddPlayerFlagsChangedCallback(peer, roomType, nick, oldFlags, flags);
}

int piParseFlags
(
	const char * flags
)
{
	int nFlags = 0;

	if(strchr(flags, 's'))
		nFlags |= PEER_FLAG_STAGING;
	if(strchr(flags, 'r'))
		nFlags |= PEER_FLAG_READY;
	if(strchr(flags, 'g'))
		nFlags |= PEER_FLAG_PLAYING;
	if(strchr(flags, 'a'))
		nFlags |= PEER_FLAG_AWAY;
	if(strchr(flags, 'h'))
		nFlags |= PEER_FLAG_HOST;

	return nFlags;
}

void piSetPlayerRoomFlags
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	const char * flags
)
{
	piPlayer * player;
	int nFlags;

	assert(nick);
	assert(nick[0]);

	if(!nick)
		return;

	player = piGetPlayer(peer, nick);
	if(!player || !player->inRoom[roomType])
		return;

	nFlags = (player->flags[roomType] & (PEER_FLAG_OP | PEER_FLAG_VOICE));

	nFlags |= piParseFlags(flags);

	piSetNewPlayerFlags(peer, nick, roomType, nFlags);
}

void piSetPlayerModeFlags
(
	PEER peer,
	const char * nick,
	RoomType roomType,
	int mode
)
{
	piPlayer * player;
	int nFlags;

	assert(nick);
	assert(nick[0]);

	if(!nick)
		return;

	player = piGetPlayer(peer, nick);
	if(!player || !player->inRoom[roomType])
		return;

	nFlags = (player->flags[roomType] & (~(PEER_FLAG_OP | PEER_FLAG_VOICE)));

	if(mode & CHAT_OP)
		nFlags |= PEER_FLAG_OP;
	if(mode & CHAT_VOICE)
		nFlags |= PEER_FLAG_VOICE;

	piSetNewPlayerFlags(peer, nick, roomType, nFlags);
}

PEERBool piIsPlayerVIP
(
	piPlayer * player,
	RoomType roomType
)
{
	if(!player)
		return PEERFalse;

	if(!player->inRoom[roomType])
		return PEERFalse;

	return (PEERBool)((player->flags[roomType] & (PEER_FLAG_OP | PEER_FLAG_VOICE)) != 0);
}

PEERBool piIsPlayerHost(piPlayer * player)
{
	if(!player)
		return PEERFalse;

	if(!player->inRoom[StagingRoom])
		return PEERFalse;

	return (PEERBool)((player->flags[StagingRoom] & (PEER_FLAG_OP | PEER_FLAG_HOST)) == (PEER_FLAG_OP | PEER_FLAG_HOST));
}

PEERBool piIsPlayerOp(piPlayer * player)
{
	if(!player)
		return PEERFalse;

	if(!player->inRoom[StagingRoom])
		return PEERFalse;

	return (PEERBool)(player->flags[StagingRoom] & PEER_FLAG_OP);
}

typedef struct piFindPlayerByIndexData
{
	int index;
	int count;
	RoomType roomType;
} piFindPlayerByIndexData;

static int piFindPlayerByIndexMap
(
	void * elem,
	void * clientdata
)
{
	piPlayer * player = (piPlayer *)elem;
	piFindPlayerByIndexData * data = (piFindPlayerByIndexData *)clientdata;

	if(player->inRoom[data->roomType])
	{
		if(data->index == data->count)
			return 0;

		data->count++;
	}

	return 1;
}

piPlayer * piFindPlayerByIndex
(
	PEER peer,
	RoomType roomType,
	int index
)
{
	piFindPlayerByIndexData data;
	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);

	data.index = index;
	data.count = 0;
	data.roomType = roomType;

	return TableMap2(connection->players, piFindPlayerByIndexMap, &data);
}

typedef struct piCountRoomOpsMapData
{
	int count;
	RoomType roomType;
	const char * exclude;
} piCountRoomOpsMapData;

static void piCountRoomOpsMap
(
	void * elem,
	void * clientdata
)
{
	piPlayer * player = (piPlayer *)elem;
	piCountRoomOpsMapData * data = (piCountRoomOpsMapData *)clientdata;

	if(data->exclude && (strcasecmp(data->exclude, player->nick) == 0))
		return;

	if(player->flags[data->roomType] & PEER_FLAG_OP)
		data->count++;
}

int piCountRoomOps(PEER peer, RoomType roomType, const char * exclude)
{
	piCountRoomOpsMapData data;

	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);

	data.count = 0;
	data.roomType = roomType;
	data.exclude = exclude;

	TableMap(connection->players, piCountRoomOpsMap, &data);

	return data.count;
}
