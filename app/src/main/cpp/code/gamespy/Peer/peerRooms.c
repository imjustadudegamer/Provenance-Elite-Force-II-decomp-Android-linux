/*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "peerRooms.h"
#include "peerPlayers.h"
#include "peerMangle.h"
#include "peerCallbacks.h"
#include "peerKeys.h"
#include "peerQR.h"
#include "peerHost.h"
#include "peerOperations.h"
#include "peerSB.h"

PEERBool piRoomsInit
(
	PEER peer
)
{
	int roomType;

	PEER_CONNECTION;

	for(roomType = 0 ; roomType < NumRooms ; roomType++)
	{
		if(connection->stayInTitleRoom && ((RoomType)roomType == TitleRoom))
			continue;

		ROOM[0] = '\0';
		NAME[0] = '\0';
		ENTERING_ROOM = PEERFalse;
		IN_ROOM = PEERFalse;
		connection->oldFlags[roomType] = 0;

#ifdef GSI_UNICODE
		ROOM_W[0] = '\0';
#endif
	}
	connection->groupID = 0;
	connection->titleRoomChannel[0] = '\0';

	return PEERTrue;
}

void piRoomsCleanup
(
	PEER peer
)
{
	int roomType;

	PEER_CONNECTION;

	for(roomType = 0 ; roomType < NumRooms ; roomType++)
	{
		if(connection->stayInTitleRoom && ((RoomType)roomType == TitleRoom))
			continue;

		if(IN_ROOM || ENTERING_ROOM)
		{
			piLeaveRoom(peer, (RoomType)roomType, NULL);
		}
		ROOM[0] = '\0';
		NAME[0] = '\0';
		ENTERING_ROOM = PEERFalse;
		IN_ROOM = PEERFalse;
	}
	connection->titleRoomChannel[0] = '\0';
}

void piStartedEnteringRoom
(
	PEER peer,
	RoomType roomType,
	const char * room
)
{
	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);
	assert(room);
	assert(room[0]);
	assert(strlen(room) < PI_ROOM_MAX_LEN);
	if(strlen(room) >= PI_ROOM_MAX_LEN)
		return;

	assert(!ROOM[0]);
	assert(!ENTERING_ROOM);
	assert(!IN_ROOM);

	strcpy(ROOM, room);
	ENTERING_ROOM = PEERTrue;
	connection->oldFlags[roomType] = 0;

#ifdef GSI_UNICODE
	UTF8ToUCS2String(ROOM, ROOM_W);
#endif
}


void piFinishedEnteringRoom
(
	PEER peer,
	RoomType roomType,
	const char * name
)
{
	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);

	if(!name)
		name = "";

	assert(ROOM[0]);
	assert(ENTERING_ROOM);
	assert(!IN_ROOM);
	assert(strlen(name) < PI_ROOM_MAX_LEN);

	IN_ROOM = PEERTrue;
	ENTERING_ROOM = PEERFalse;
	strzcpy(NAME, name, PI_NAME_MAX_LEN);

#ifdef GSI_UNICODE
	UTF8ToUCS2String(NAME, NAME_W);
#endif

	piSetLocalFlags(peer);

	piKeyCacheRefreshRoom(peer, roomType);
}

void piLeaveRoom
(
	PEER peer,
	RoomType roomType,
	const char * reason
)
{
	PEER_CONNECTION;

	ASSERT_ROOMTYPE(roomType);

	if(!ENTERING_ROOM && !IN_ROOM)
		return;

	assert(ROOM[0]);

	if(ENTERING_ROOM)
	{
		piCancelJoinOperation(peer, roomType);
	}

	if(connection->connected)
		chatLeaveChannelA(connection->chat, ROOM, reason);

	piClearRoomPlayers(peer, roomType);

	if(IN_ROOM)
	{
		assert(!ENTERING_ROOM);
		IN_ROOM = PEERFalse;
	}
	else
	{
		assert(ENTERING_ROOM);
		ENTERING_ROOM = PEERFalse;
	}

	ROOM[0] = '\0';
	NAME[0] = '\0';

#ifdef GSI_UNICODE
	ROOM_W[0] = '\0';
#endif

	connection->oldFlags[roomType] = 0;

	if(roomType == StagingRoom)
	{
		piStopHosting(peer, PEERFalse);

		if(!connection->playing)
			piStopReporting(peer);

		piSBFreeHostServer(peer);

		connection->ready = PEERFalse;

		connection->passwordedRoom = PEERFalse;

		piSetLocalFlags(peer);
	}
	else if(roomType == GroupRoom)
	{
		connection->groupID = 0;
	}

	piKeyCacheCleanse(peer);
}

PEERBool piRoomToType
(
	PEER peer,
	const char * room,
	RoomType * roomType
)
{
	int i;

	PEER_CONNECTION;

	for(i = 0 ; i < NumRooms ; i++)
	{
		if(strcasecmp(room, ROOMS[i]) == 0)
		{
			*roomType = (RoomType)i;
			return PEERTrue;
		}
	}

	return PEERFalse;
}

void piSetLocalFlags
(
	PEER peer
)
{
	char buffer[NumRooms][128];
	char * titleRoom;
	char * groupRoom;
	char * stagingRoom;
	const char * key = "b_flags";
	int nFlags;

	PEER_CONNECTION;
	
	if(!connection->connected)
		return;

	if(connection->inRoom[TitleRoom] || connection->enteringRoom[TitleRoom])
		titleRoom = buffer[TitleRoom];
	else
		titleRoom = NULL;
	if(connection->inRoom[GroupRoom] || connection->enteringRoom[GroupRoom])
		groupRoom = buffer[GroupRoom];
	else
		groupRoom = NULL;
	if(connection->inRoom[StagingRoom] || connection->enteringRoom[StagingRoom])
		stagingRoom = buffer[StagingRoom];
	else
		stagingRoom = NULL;

	if(connection->inRoom[StagingRoom])
	{
		if(titleRoom)
			*titleRoom++ = 's';
		if(groupRoom)
			*groupRoom++ = 's';
		*stagingRoom++ = 's';

		if(connection->ready)
			*stagingRoom++ = 'r';

		if(connection->hosting)
			*stagingRoom++ = 'h';
	}

	if(connection->playing)
	{
		if(titleRoom)
			*titleRoom++ = 'g';
		if(groupRoom)
			*groupRoom++ = 'g';
		if(stagingRoom)
			*stagingRoom++ = 'g';
	}

	if(connection->away)
	{
		if(titleRoom)
			*titleRoom++ = 'a';
		if(groupRoom)
			*groupRoom++ = 'a';
		if(stagingRoom)
			*stagingRoom++ = 'a';
	}

	if(titleRoom)
	{
		*titleRoom = '\0';
		titleRoom = buffer[TitleRoom];
	}
	if(groupRoom)
	{
		*groupRoom = '\0';
		groupRoom = buffer[GroupRoom];
	}
	if(stagingRoom)
	{
		*stagingRoom = '\0';
		stagingRoom = buffer[StagingRoom];
	}

	if(titleRoom)
	{
		nFlags = piParseFlags(titleRoom);
		if(nFlags != connection->oldFlags[TitleRoom])
		{
			chatSetChannelKeysA(connection->chat, connection->rooms[TitleRoom], connection->nick, 1, &key, (const char **)&titleRoom);
			connection->oldFlags[TitleRoom] = nFlags;
		}
	}
	if(groupRoom)
	{
		nFlags = piParseFlags(groupRoom);
		if(nFlags != connection->oldFlags[GroupRoom])
		{
			chatSetChannelKeysA(connection->chat, connection->rooms[GroupRoom], connection->nick, 1, &key, (const char **)&groupRoom);
			connection->oldFlags[GroupRoom] = nFlags;
		}
	}
	if(stagingRoom)
	{
		nFlags = piParseFlags(stagingRoom);
		if(nFlags != connection->oldFlags[StagingRoom])
		{
			chatSetChannelKeysA(connection->chat, connection->rooms[StagingRoom], connection->nick, 1, &key, (const char **)&stagingRoom);
			connection->oldFlags[StagingRoom] = nFlags;
		}
	}
}
