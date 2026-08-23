
#include "peerAutoMatch.h"
#include "peerMangle.h"
#include "peerOperations.h"
#include "peerSB.h"
#include "peerQR.h"
#include "peerCallbacks.h"
#include "peerPlayers.h"
#include "peerRooms.h"

static PEERBool piCreateAutoMatchRoom(PEER peer);

static void piCleanAutoMatch(PEER peer)
{
	PEER_CONNECTION;

	gsifree(connection->autoMatchFilter);

	piRemoveOperation(peer, connection->autoMatchOperation);
	
	connection->autoMatchOperation = NULL;
}

void piSetAutoMatchStatus(PEER peer, PEERAutoMatchStatus status)
{
	piOperation * operation;
	PEER_CONNECTION;

	operation = connection->autoMatchOperation;
	assert(operation);
	if(!operation)
		return;

	if((status == PEERWaiting) && !connection->inRoom[StagingRoom])
	{
		if(!piCreateAutoMatchRoom(peer))
			piSetAutoMatchStatus(peer, connection->autoMatchSBFailed?PEERFailed:PEERSearching);
		return;
	}

	if(connection->autoMatchStatus != status)
	{
		connection->autoMatchStatus = status;
		
		piAddAutoMatchStatusCallback(peer);
	}

	switch(status)
	{		
	case PEERFailed:
		piSBStopListingAutoMatches(peer);
		piStopAutoMatchReporting(peer);
		piLeaveRoom(peer, StagingRoom, "");

		piCleanAutoMatch(peer);
		break;

	case PEERSearching:
		piStopAutoMatchReporting(peer);
		piLeaveRoom(peer, StagingRoom, "");

		if(!connection->autoMatchBrowsing)
		{
			if(!piSBStartListingAutoMatches(peer))
			{
				piSetAutoMatchStatus(peer, connection->autoMatchQRFailed?PEERFailed:PEERWaiting);
				return;
			}
		}
		break;

	case PEERWaiting:
		assert(connection->inRoom[StagingRoom]);
		if(!connection->autoMatchBrowsing && !connection->autoMatchSBFailed)			
			piSBStartListingAutoMatches(peer);

		if(!connection->autoMatchReporting)
		{
			if(!piStartAutoMatchReporting(peer))
			{
				piSetAutoMatchStatus(peer, connection->autoMatchSBFailed?PEERFailed:PEERSearching);
				return;
			}
		}
		break;

	case PEERStaging:
		if(!connection->hosting)
		{
			piStopAutoMatchReporting(peer);
		}
		piSBStopListingAutoMatches(peer);

		if(connection->hosting && !connection->autoMatchReporting)
		{
			if(!piStartAutoMatchReporting(peer))
			{
				piSetAutoMatchStatus(peer, PEERSearching);
				return;
			}
		}
		break;

	case PEERReady:
		if (connection->hosting && !connection->autoMatchReporting)
			piStartAutoMatchReporting(peer);
		
		break;
	case PEERComplete:
		piSBStopListingAutoMatches(peer);
		piStopAutoMatchReporting(peer);

		piCleanAutoMatch(peer);
		break;
	}
}

void piStopAutoMatch(PEER peer)
{

	PEER_CONNECTION;

	if(peerIsAutoMatching(peer))
	{
		if(connection->autoMatchOperation)
			connection->autoMatchOperation->callback = (PEERCBType)NULL;

		
		piSetAutoMatchStatus(peer, PEERFailed);

	}
}

static void piJoinAutoMatchRoomCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	PEER_CONNECTION;

	if(success && ((connection->numPlayers[StagingRoom] <= 1) || !piCountRoomOps(peer, StagingRoom, connection->nick)))
	{
		piLeaveRoom(peer, StagingRoom, "");
		success = PEERFalse;
	}

	if(success)
	{
		piSetAutoMatchStatus(peer, PEERStaging);

		if((connection->autoMatchStatus == PEERStaging) && (connection->numPlayers[StagingRoom] >= connection->maxPlayers))
			piSetAutoMatchStatus(peer, PEERReady);
	}

	if(!success && (connection->autoMatchStatus == PEERWaiting))
		piSetAutoMatchStatus(peer, PEERWaiting);
		
	GSI_UNUSED(result);
	GSI_UNUSED(roomType);
	GSI_UNUSED(param);
}

PEERBool piJoinAutoMatchRoom(PEER peer, SBServer server)
{
	unsigned int publicIP;
	unsigned int privateIP;
	unsigned short privatePort;
	char room[PI_ROOM_MAX_LEN];

	PEER_CONNECTION;

	publicIP = SBServerGetPublicInetAddress(server);
	privateIP = SBServerGetPrivateInetAddress(server);
	if(SBServerHasPrivateAddress(server))
		privatePort = SBServerGetPrivateQueryPort(server);
	else
		privatePort = SBServerGetPublicQueryPort(server);

	if(!publicIP)
		return PEERFalse;

	piMangleStagingRoom(room, connection->title, publicIP, privateIP, privatePort);

	if(!piNewJoinRoomOperation(peer, StagingRoom, room, NULL, piJoinAutoMatchRoomCallback, NULL, piGetNextID(peer)))
		return PEERFalse;

	connection->hostServer = piSBCloneServer(server);

	return PEERTrue;
}

static void piAutoMatchGetChannelCallback(CHAT chat, CHATBool success, const gsi_char *channel, CHATChannelMode *mode, void *param)
{
	piConnection *connection = (piConnection *)param;
	if (success)
	{

		mode->Limit = connection->maxPlayers;
		mode->OpsObeyChannelLimit = CHATTrue;
		
		chatSetChannelMode(chat, channel, mode);
	}
	else
	{
		connection->autoMatchQRFailed = PEERTrue;
		piSetAutoMatchStatus((PEER *)&connection, connection->autoMatchSBFailed?PEERFailed:PEERSearching);
	}
}

static void piCreateAutoMatchRoomCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	PEERAutoMatchStatus status;

	PEER_CONNECTION;

	if(success)
	{
		if(connection->numPlayers[StagingRoom] <= 1)
		{
			status = PEERWaiting;

			chatGetChannelMode(connection->chat, peerGetRoomChannel(peer, StagingRoom), piAutoMatchGetChannelCallback, 
				(void *)connection, CHATFalse);
		}
		else if(connection->numPlayers[StagingRoom] >= connection->maxPlayers)
		{
			status = PEERReady;
		}
		else
		{
			status = PEERStaging;
		}

		piSetAutoMatchStatus(peer, status);
	}
	else
	{
		connection->autoMatchQRFailed = PEERTrue;
		piSetAutoMatchStatus(peer, connection->autoMatchSBFailed?PEERFailed:PEERSearching);
	}
	
	GSI_UNUSED(result);
	GSI_UNUSED(roomType);
	GSI_UNUSED(param);
}

static PEERBool piCreateAutoMatchRoom(PEER peer)
{
	piOperation * operation;

	PEER_CONNECTION;

	operation = connection->autoMatchOperation;

	if(!piNewCreateStagingRoomOperation(peer, connection->nick, "", connection->maxPlayers, operation->socket, operation->port, piCreateAutoMatchRoomCallback, NULL, piGetNextID(peer)))
	{
		connection->autoMatchQRFailed = PEERTrue;
		return PEERFalse;
	}

	return PEERTrue;
}
