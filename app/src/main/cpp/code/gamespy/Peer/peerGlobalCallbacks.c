
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "peer.h"
#include "peerAscii.h"
#include "peerMain.h"
#include "peerGlobalCallbacks.h"
#include "peerRooms.h"
#include "peerPlayers.h"
#include "peerCallbacks.h"
#include "peerOperations.h"
#include "peerPing.h"
#include "peerMangle.h"
#include "peerKeys.h"
#include "peerAutoMatch.h"

#define PI_UTM_MATCH(utm)       (strncmp(piUTMCommand, utm, strlen(utm) - 1) == 0)

#define PI_UTM_COMMAND_LEN      8
#define PI_UTM_PARAMATERS_LEN   512

static char piUTMCommand[PI_UTM_COMMAND_LEN];
static char piUTMParameters[PI_UTM_PARAMATERS_LEN];

void peerMessagePlayerA(PEER peer, const char * nick, const char * message,	MessageType messageType);


static PEERBool piIsOldUTM
(
	const char * message
)
{
	assert(message);
	if(!message)
		return PEERFalse;

	if(strlen(message) < 4)
		return PEERFalse;

	if((message[0] != '@') ||
		(message[1] != '@') ||
		(message[2] != '@') ||
		(message[3] == ' '))
	{
		return PEERFalse;
	}

	return PEERTrue;
}

static PEERBool piParseUTM
(
	const char * message
)
{
	int len;

	assert(message);
	if(!message)
		return PEERFalse;

	len = (int)strcspn(message, "/ ");
	if(len >= PI_UTM_COMMAND_LEN)
		return PEERFalse;
	memcpy(piUTMCommand, message, (unsigned int)len);
	piUTMCommand[len] = '\0';

	message += len;
	if(message[0])
	{
		message++;
		if(strlen(message) >= PI_UTM_PARAMATERS_LEN)
			return PEERFalse;
		strcpy(piUTMParameters, message);
	}
	else
	{
		piUTMParameters[0] = '\0';
	}

	return PEERTrue;
}

static void piProcessUTM
(
	PEER peer,
	piPlayer * player,
	PEERBool inRoom,
	RoomType roomType
)
{
	char * params = piUTMParameters;

	PEER_CONNECTION;

	assert(piUTMCommand[0]);
	assert(player);

	if(PI_UTM_MATCH(PI_UTM_LAUNCH))
	{
#ifdef _DEBUG
		assert(connection->inRoom[StagingRoom]);
		if(inRoom)
			assert(roomType == StagingRoom);
		else
			assert(player->inRoom[StagingRoom]);
#endif
		if(!connection->inRoom[StagingRoom])
			return;
		if(inRoom && (roomType != StagingRoom))
			return;
		if(!inRoom && !player->inRoom[StagingRoom])
			return;

		if(connection->hosting)
			return;

		if(!piIsPlayerOp(player))
			return;

		connection->playing = PEERTrue;

		piSetLocalFlags(peer);

		piAddGameStartedCallback(peer, connection->hostServer, params);

		if(peerIsAutoMatching(peer))
			piSetAutoMatchStatus(peer, PEERComplete);
	}
	else if(PI_UTM_MATCH(PI_UTM_XPING))
	{
		piPlayer * other;
		int ping;
		unsigned int IP;

#ifdef _DEBUG
#endif
		if(inRoom && !connection->xpingRoom[roomType])
			return;

		if(!params[0])
			return;

		IP = piDemangleIP(params);

		params += 11;
		ping = atoi(params);

		other = piFindPlayerByIP(peer, IP);
		if(!other)
			return;
		if(strcasecmp(player->nick, other->nick) == 0)
			return;
		if(inRoom && !player->inRoom[roomType])
			return;
		if(!inRoom)
		{
			int i;
			PEERBool success = PEERFalse;

			for(i = 0 ; i < NumRooms ; i++)
			{
				if(connection->xpingRoom[i] && connection->inRoom[i] && player->inRoom[i] && other->inRoom[i])
					success = PEERTrue;
			}
			if(!success)
				return;
		}

		piUpdateXping(peer, player->nick, other->nick, ping);

		piAddCrossPingCallback(peer, player->nick, other->nick, ping);
	}
}

void piChatDisconnectedA
(
	CHAT chat,
	const char * reason,
	PEER peer
)
{
	PEER_CONNECTION;

	connection->disconnect = PEERTrue;

	piAddDisconnectedCallback(peer, reason);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
void piChatDisconnectedW
(
	CHAT chat,
	const unsigned short * reason,
	PEER peer
)
{
	char* reason_A = UCS2ToUTF8StringAlloc(reason);
	piChatDisconnectedA(chat, reason_A, peer);
	gsifree(reason_A);
}
#endif

static void piHandleOldNFO
(
	PEER peer,
	piPlayer * player,
	const char * message
)
{
	if(strncmp(message + strlen(message) - 2, "X\\", 2) != 0)
	{
		const char * str;

		if(!player->inRoom[StagingRoom])
			return;

		str = strstr(message, "\\$flags$\\");
		if(str)
		{
			PEERBool ready = PEERFalse;
			str += 9;
			while(*str && (*str != '\\'))
			{
				if(*str++ == 'r')
				{
					ready = PEERTrue;
					break;
				}
			}

			if(ready)
				player->flags[StagingRoom] |= PEER_FLAG_READY;
			else
				player->flags[StagingRoom] &= ~PEER_FLAG_READY;
		}
	}
	
	GSI_UNUSED(peer);
}

void piChatPrivateMessageA
(
	CHAT chat,
	const char * user,
	const char * message,
	int type,
	PEER peer
)
{
	assert(message);

	if(!user || !user[0])
		return;

	if(piIsOldUTM(message))
	{
		if(strncasecmp(message, "@@@NFO", 6) == 0)
		{
			piPlayer * player;

			player = piGetPlayer(peer, user);
			if(player)
				piHandleOldNFO(peer, player, message);
		}

		return;
	}

	if((type == CHAT_UTM) || (type == CHAT_ATM))
	{
		if(piParseUTM(message))
		{
			piPlayer * player;

			player = piGetPlayer(peer, user);
			if(player)
			{
				piProcessUTM(peer, player, PEERFalse, (RoomType)0);
			}

			piAddPlayerUTMCallback(peer, user, piUTMCommand, piUTMParameters, (PEERBool)(type == CHAT_ATM));
		}

		return;
	}

	piAddPlayerMessageCallback(peer, user, message, (MessageType)type);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
void piChatPrivateMessageW
(
	CHAT chat,
	const unsigned short * user,
	const unsigned short * message,
	int type,
	PEER peer
)
{
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char* message_A = UCS2ToUTF8StringAlloc(message);
	piChatPrivateMessageA(chat, user_A, message_A, type, peer);
	gsifree(user_A);
	gsifree(message_A);
}
#endif

static void piChannelMessageA
(
	CHAT chat,
	const char * channel,
	const char * user,
	const char * message,
	int type,
	PEER peer
)
{
	piPlayer * player;
	RoomType roomType;


	assert(message);

	if(!piRoomToType(peer, channel, &roomType))
		return;

	player = piGetPlayer(peer, user);

	if(player && piIsOldUTM(message))
	{
		if(roomType != StagingRoom)
			return;

		if(strncasecmp(message, "@@@GML", 6) == 0)
		{
			if(strncmp(message + strlen(message) - 4, "/OLD", 4) == 0)
				return;

			type = CHAT_UTM;
			message = "GML";
		}
		else if(strncasecmp(message, "@@@NFO", 6) == 0)
		{
			piHandleOldNFO(peer, player, message);

			return;
		}
		else
		{
			return;
		}
	}

	if((type == CHAT_UTM) || (type == CHAT_ATM))
	{
		if(piParseUTM(message))
		{
			if(player)
				piProcessUTM(peer, player, PEERTrue, roomType);

			piAddRoomUTMCallback(peer, roomType, user, piUTMCommand, piUTMParameters, (type == CHAT_ATM)?PEERTrue:PEERFalse);
		}

		return;
	}

	piAddRoomMessageCallback(peer, roomType, user, message, (MessageType)type);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelMessageW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * user,
	const unsigned short * message,
	int type,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char* message_A = UCS2ToUTF8StringAlloc(message);
	piChannelMessageA(chat, channel_A, user_A, message_A, type, peer);
	gsifree(channel_A);
	gsifree(user_A);
	gsifree(message_A);
}
#endif

static void piChannelKickedA
(
	CHAT chat,
	const char * channel,
	const char * user,
	const char * reason,
	PEER peer
)
{
	RoomType roomType;


	if(!piRoomToType(peer, channel, &roomType))
		return;

	piLeaveRoom(peer, roomType, NULL);

	piAddKickedCallback(peer, roomType, user, reason);
	
	if((roomType == StagingRoom) && peerIsAutoMatching(peer))
		piSetAutoMatchStatus(peer, PEERSearching);
		
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelKickedW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * user,
	const unsigned short * reason,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char* reason_A = UCS2ToUTF8StringAlloc(reason);
	piChannelKickedA(chat, channel_A, user_A, reason_A, peer);
	gsifree(channel_A);
	gsifree(user_A);
	gsifree(reason_A);
}
#endif

static void piChannelUserJoinedA
(
	CHAT chat,
	const char * channel,
	const char * user,
	int mode,
	PEER peer
)
{
	RoomType roomType;
	piPlayer * player;

	PEER_CONNECTION;

	if(!piRoomToType(peer, channel, &roomType))
		return;

	player = piPlayerJoinedRoom(peer, user, roomType, mode);
	if(!player)
		return;

	if(!player->gotIPAndProfileID)
	{
		const char * info;
		unsigned int IP;
		int profileID;

		if(chatGetBasicUserInfoNoWaitA(connection->chat, user, &info, NULL) && piDemangleUser(info, &IP, &profileID))
		{
			piSetPlayerIPAndProfileID(peer, user, IP, profileID);
		}
	}

	piKeyCacheRefreshPlayer(peer, roomType, user);

	piAddPlayerJoinedCallback(peer, roomType, user);

#if 1
	if((roomType == StagingRoom) && connection->ready)
		peerMessagePlayerA(peer, user, "@@@NFO \\$flags$\\rX\\", NormalMessage);
#endif

	if((roomType == StagingRoom) && peerIsAutoMatching(peer))
	{
		if(connection->autoMatchStatus == PEERWaiting)
			piSetAutoMatchStatus(peer, PEERStaging);

		if((connection->autoMatchStatus == PEERStaging) && (connection->numPlayers[StagingRoom] >= connection->maxPlayers))
			piSetAutoMatchStatus(peer, PEERReady);
	}
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelUserJoinedW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * user,
	int mode,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	piChannelUserJoinedA(chat, channel_A, user_A, mode, peer);
	gsifree(channel_A);
	gsifree(user_A);
}
#endif

static void piChannelUserPartedA
(
	CHAT chat,
	const char * channel,
	const char * user,
	int why,
	const char * reason,
	const char * kicker,
	PEER peer
)
{
	RoomType roomType;
	PEERAutoMatchStatus status = PEERFailed;
	PEERBool newStatus = PEERFalse;
	PEERBool autoMatchRoom;

	PEER_CONNECTION;

	if(!piRoomToType(peer, channel, &roomType))
		return;

	autoMatchRoom = ((roomType == StagingRoom) && peerIsAutoMatching(peer))?PEERTrue:PEERFalse;
	if(autoMatchRoom)
	{
		piPlayer * player;

		player = piGetPlayer(peer, user);

		if(!connection->hosting && piIsPlayerHost(player))
		{
			status = PEERSearching;
			newStatus = PEERTrue;
		}
		else if(connection->hosting && connection->numPlayers[StagingRoom] == 2)
		{
			status = PEERWaiting;
			newStatus = PEERTrue;
		}
		else if(connection->numPlayers[StagingRoom] == connection->maxPlayers)
		{
			status = PEERStaging;
			newStatus = PEERTrue;
		}
	}

	piPlayerLeftRoom(peer, user, roomType);

	if((why == CHAT_KICKED) || (why == CHAT_KILLED))
		reason = "Kicked";
	else if(!reason)
		reason = "";

	piAddPlayerLeftCallback(peer, roomType, user, reason);

	if(newStatus)
		piSetAutoMatchStatus(peer, status);

	GSI_UNUSED(chat);
	GSI_UNUSED(kicker);
}
#ifdef GSI_UNICODE
static void piChannelUserPartedW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * user,
	int why,
	const unsigned short * reason,
	const unsigned short * kicker,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	char* reason_A = UCS2ToUTF8StringAlloc(reason);
	char* kicker_A = UCS2ToUTF8StringAlloc(kicker);
	piChannelUserPartedA(chat, channel_A, user_A, why, reason_A, kicker_A, peer);
	gsifree(channel_A);
	gsifree(user_A);
	gsifree(reason_A);
	gsifree(kicker_A);
}
#endif

static void piChannelUserChangedNickA
(
	CHAT chat,
	const char * channel,
	const char * oldNick,
	const char * newNick,
	PEER peer
)
{
	RoomType roomType;

	if(!piRoomToType(peer, channel, &roomType))
		return;

	piPlayerChangedNick(peer, oldNick, newNick);

	piAddPlayerChangedNickCallback(peer, roomType, oldNick, newNick);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelUserChangedNickW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * oldNick,
	const unsigned short * newNick,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* oldNick_A = UCS2ToUTF8StringAlloc(oldNick);
	char* newNick_A = UCS2ToUTF8StringAlloc(newNick);
	piChannelUserChangedNickA(chat, channel_A, oldNick_A, newNick_A, peer);
	gsifree(channel_A);
	gsifree(oldNick_A);
	gsifree(newNick_A);
}
#endif

static void piChannelTopicChangedA
(
	CHAT chat,
	const char * channel,
	const char * topic,
	PEER peer
)
{
	RoomType roomType;

	PEER_CONNECTION;

	if(!piRoomToType(peer, channel, &roomType))
		return;
	
	if(!topic[0])
		return;


	strzcpy(NAME, topic, PI_NAME_MAX_LEN);

	if(IN_ROOM)
		piAddRoomNameChangedCallback(peer, roomType);
		
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelTopicChangedW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * topic,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* topic_A = UCS2ToUTF8StringAlloc(topic);
	piChannelTopicChangedA(chat, channel_A, topic_A, peer);
	gsifree(channel_A);
	gsifree(topic_A);
}
#endif

static void piChannelNewUserListA
(
	CHAT chat,
	const char * channel,
	int num,
	const char ** operators,
	int * modes,
	PEER peer
)
{
	int i;
	RoomType roomType;


	if(!piRoomToType(peer, channel, &roomType))
		return;

	piClearRoomPlayers(peer, roomType);

	for(i = 0 ; i < num ; i++)
		piPlayerJoinedRoom(peer, operators[i], roomType, modes[i]);

	piKeyCacheRefreshRoom(peer, roomType);

	piAddNewPlayerListCallback(peer, roomType);	
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelNewUserListW
(
	CHAT chat,
	const unsigned short * channel,
	int num,
	const unsigned short ** operators,
	int * modes,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char** users_A = UCS2ToUTF8StringArrayAlloc(operators, num);
	int i;
	piChannelNewUserListA(chat, channel_A, num, (const char**)users_A, modes, peer);
	gsifree(channel_A);
	for (i=0; i<num; i++)
		gsifree(users_A[i]);
	gsifree(users_A);
}
#endif

static void piBroadcastKeyChangedA
(
	CHAT chat,
	const char * channel,
	const char * user,
	const char * key,
	const char * value,
	PEER peer
)
{
	RoomType roomType;


	if(!piRoomToType(peer, channel, &roomType))
		return;

	piRoomKeyChanged(peer, roomType, user, key, value);

	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piBroadcastKeyChangedW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * user,
	const unsigned short * key,
	const unsigned short * value,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A	= UCS2ToUTF8StringAlloc(user);
	char* key_A		= UCS2ToUTF8StringAlloc(key);
	char* value_A	= UCS2ToUTF8StringAlloc(value);
	piBroadcastKeyChangedA(chat, channel_A, user_A, key_A, value_A, peer);
	gsifree(channel_A);
	gsifree(user_A);
	gsifree(key_A);
	gsifree(value_A);
}
#endif

static void piUserModeChangedA
(
	CHAT chat,
	const char * channel,
	const char * user,
	int mode,
	PEER peer
)
{
	RoomType roomType;


	if(!piRoomToType(peer, channel, &roomType))
		return;

	piSetPlayerModeFlags(peer, user, roomType, mode);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piUserModeChangedW
(
	CHAT chat,
	const unsigned short * channel,
	const unsigned short * user,
	int mode,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	char* user_A = UCS2ToUTF8StringAlloc(user);
	piUserModeChangedA(chat, channel_A, user_A, mode, peer);
	gsifree(channel_A);
	gsifree(user_A);
}
#endif

static void piChannelModeChangedA
(
	CHAT chat,
	const char * channel,
	CHATChannelMode * mode,
	PEER peer
)
{
	RoomType roomType;


	if(!piRoomToType(peer, channel, &roomType))
		return;

	piAddRoomModeChangedCallback(peer, roomType, mode);
	
	GSI_UNUSED(chat);
}
#ifdef GSI_UNICODE
static void piChannelModeChangedW
(
	CHAT chat,
	const unsigned short * channel,
	CHATChannelMode * mode,
	PEER peer
)
{
	char* channel_A = UCS2ToUTF8StringAlloc(channel);
	piChannelModeChangedA(chat, channel_A, mode, peer);
	gsifree(channel_A);
}
#endif

void piSetChannelCallbacks
(
	PEER peer,
	chatChannelCallbacks * channelCallbacks
)
{
	memset(channelCallbacks, 0, sizeof(chatChannelCallbacks));
	channelCallbacks->param = peer;
#ifndef GSI_UNICODE 
	channelCallbacks->channelMessage = piChannelMessageA;
	channelCallbacks->kicked = piChannelKickedA;
	channelCallbacks->userJoined = piChannelUserJoinedA;
	channelCallbacks->userParted = piChannelUserPartedA;
	channelCallbacks->userChangedNick = piChannelUserChangedNickA;
	channelCallbacks->topicChanged = piChannelTopicChangedA;
	channelCallbacks->newUserList = piChannelNewUserListA;
	channelCallbacks->broadcastKeyChanged = piBroadcastKeyChangedA;
	channelCallbacks->userModeChanged = piUserModeChangedA;
	channelCallbacks->channelModeChanged = piChannelModeChangedA;
#else
	channelCallbacks->channelMessage = piChannelMessageW;
	channelCallbacks->kicked = piChannelKickedW;
	channelCallbacks->userJoined = piChannelUserJoinedW;
	channelCallbacks->userParted = piChannelUserPartedW;
	channelCallbacks->userChangedNick = piChannelUserChangedNickW;
	channelCallbacks->topicChanged = piChannelTopicChangedW;
	channelCallbacks->newUserList = piChannelNewUserListW;
	channelCallbacks->broadcastKeyChanged = piBroadcastKeyChangedW;
	channelCallbacks->userModeChanged = piUserModeChangedW;
	channelCallbacks->channelModeChanged = piChannelModeChangedW;
#endif
}

