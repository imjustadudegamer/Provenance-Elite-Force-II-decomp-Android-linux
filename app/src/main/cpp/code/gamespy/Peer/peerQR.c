
#include "peerQR.h"
#include "peerGlobalCallbacks.h"
#include "peerPlayers.h"
#include "peerAutoMatch.h"
#include "peerOperations.h"

#ifndef GSI_UNICODE
#define piQRAddErrorCallback piQRAddErrorCallbackA
#else
#define piQRAddErrorCallback piQRAddErrorCallbackW
#endif

static void piQRServerKeyCallback
(
	int key,
	qr2_buffer_t buffer,
	PEER peer
)
{
	PEER_CONNECTION;

	if(connection->inRoom[StagingRoom] && (!connection->playing || (connection->reportingOptions & PEER_REPORT_INFO)))
	{
		switch(key)
		{
		case HOSTNAME_KEY:
			qr2_buffer_addA(buffer, NAMES[StagingRoom]);
			return;
		case NUMPLAYERS_KEY:
			qr2_buffer_add_int(buffer, connection->numPlayers[StagingRoom]);
			return;
		case MAXPLAYERS_KEY:
			if(!connection->maxPlayers)
				break;
			qr2_buffer_add_int(buffer, connection->maxPlayers);
			return;
		case GAMEMODE_KEY:
			if(connection->playing)
				break;
			qr2_buffer_addA(buffer, "openstaging");
			return;
		case PASSWORD_KEY:
			qr2_buffer_add_int(buffer, connection->passwordedRoom?1:0);
			return;
		}
	}

	if(key == GROUPID_KEY)
	{
		qr2_buffer_add_int(buffer, connection->reportingGroupID);
		return;
	}

	if(connection->callbacks.qrServerKey)
		connection->callbacks.qrServerKey(peer, key, buffer, connection->callbacks.param);
}

static void piQRPlayerKeyCallback
(
	int key,
	int index,
	qr2_buffer_t buffer,
	PEER peer
)
{
	piPlayer * player;
	PEER_CONNECTION;

	if(connection->inRoom[StagingRoom] && (!connection->playing || (connection->reportingOptions & PEER_REPORT_PLAYERS)))
	{
		if((key == PLAYER__KEY) || (key == PING__KEY))
		{
			player = piFindPlayerByIndex(peer, StagingRoom, index);
			if(!player)
				qr2_buffer_addA(buffer, "");
			else if(key == PLAYER__KEY)
				qr2_buffer_addA(buffer, player->nick);
			else if(key == PING__KEY)
				qr2_buffer_add_int(buffer, player->pingAverage);

			return;
		}
	}

	if(connection->callbacks.qrPlayerKey)
		connection->callbacks.qrPlayerKey(peer, key, index, buffer, connection->callbacks.param);
}

static void piQRTeamKeyCallback
(
	int key,
	int index,
	qr2_buffer_t buffer,
	PEER peer
)
{
	PEER_CONNECTION;

	if(connection->callbacks.qrTeamKey)
		connection->callbacks.qrTeamKey(peer, key, index, buffer, connection->callbacks.param);
}

static void piQRKeyListCallback
(
	qr2_key_type type,
	qr2_keybuffer_t keyBuffer,
	PEER peer
)
{
	PEER_CONNECTION;

	switch(type)
	{
	case key_server:
		if(!connection->autoMatchReporting)
		{
			qr2_keybuffer_add(keyBuffer, HOSTNAME_KEY);
			qr2_keybuffer_add(keyBuffer, GAMEMODE_KEY);
			if(connection->passwordedRoom)
				qr2_keybuffer_add(keyBuffer, PASSWORD_KEY);
			if(connection->reportingGroupID)
				qr2_keybuffer_add(keyBuffer, GROUPID_KEY);
		}
		qr2_keybuffer_add(keyBuffer, NUMPLAYERS_KEY);
		if(connection->maxPlayers)
			qr2_keybuffer_add(keyBuffer, MAXPLAYERS_KEY);
		break;
	case key_player:
		qr2_keybuffer_add(keyBuffer, PLAYER__KEY);
		qr2_keybuffer_add(keyBuffer, PING__KEY);
		break;
	default:
		break;
	}

	if(connection->callbacks.qrKeyList)
		connection->callbacks.qrKeyList(peer, type, keyBuffer, connection->callbacks.param);
}

static int piQRCountCallback
(
	qr2_key_type type,
	PEER peer
)
{
	PEER_CONNECTION;

	if(type == key_player)
	{
		if(connection->inRoom[StagingRoom] && (!connection->playing || (connection->reportingOptions & PEER_REPORT_PLAYERS)))
			return connection->numPlayers[StagingRoom];
	}

	if(connection->callbacks.qrCount)
		return connection->callbacks.qrCount(peer, type, connection->callbacks.param);

	return 0;
}

static void piQRAddErrorCallbackA
(
	qr2_error_t error,
	char * errorString,
	PEER peer
)
{
	PEER_CONNECTION;

	if(peerIsAutoMatching(peer))
	{
		PEERAutoMatchStatus status;

		connection->autoMatchQRFailed = PEERTrue;

		if(connection->autoMatchSBFailed && (connection->autoMatchStatus != PEERStaging))
			status = PEERFailed;
		else
			status = PEERSearching;
		piSetAutoMatchStatus(peer, status);

		return;
	}

	if(connection->callbacks.qrAddError)
	{
#ifndef GSI_UNICODE
		connection->callbacks.qrAddError(peer, error, errorString, connection->callbacks.param);
#else
		unsigned short* errorString_W = UTF8ToUCS2StringAlloc(errorString);
		connection->callbacks.qrAddError(peer, error, errorString_W, connection->callbacks.param);
		gsifree(errorString_W);
#endif
	}
}
#ifdef GSI_UNICODE
static void piQRAddErrorCallbackW
(
	qr2_error_t error,
	unsigned short * errorString,
	PEER peer
)
{
	char* errorString_A = UCS2ToUTF8StringAlloc(errorString);
	piQRAddErrorCallbackA(error, errorString_A, peer);
	gsifree(errorString_A);
}
#endif

static void piQRNatNegotiateCallback
(
	int cookie,
	PEER peer
)
{
	PEER_CONNECTION;

	if(connection->callbacks.qrNatNegotiateCallback)
		connection->callbacks.qrNatNegotiateCallback(peer, cookie, connection->callbacks.param);
}

static void piQRPublicAddressCallback
(
	unsigned int ip,
	unsigned short port,
	PEER peer
)
{
	PEER_CONNECTION;

	if(connection->callbacks.qrPublicAddressCallback)
		connection->callbacks.qrPublicAddressCallback(peer, ip, port, connection->callbacks.param);
}

PEERBool piStartReporting
(
	PEER peer,
	SOCKET socket,
	unsigned short port
)
{
	int rcode;

	PEER_CONNECTION;

	assert(!connection->queryReporting);
	if(connection->queryReporting)
		piStopReporting(peer);

	if(socket == INVALID_SOCKET)
	{
		rcode = qr2_initA(&connection->queryReporting, NULL, PI_QUERYPORT, connection->title, connection->qrSecretKey,
			1, connection->natNegotiate,
			piQRServerKeyCallback,
			piQRPlayerKeyCallback,
			piQRTeamKeyCallback,
			piQRKeyListCallback,
			piQRCountCallback,
			piQRAddErrorCallback,
			peer);
	}
	else
	{
		rcode = qr2_init_socketA(&connection->queryReporting, socket, port, connection->title, connection->qrSecretKey,
			1, connection->natNegotiate,
			piQRServerKeyCallback,
			piQRPlayerKeyCallback,
			piQRTeamKeyCallback,
			piQRKeyListCallback,
			piQRCountCallback,
			piQRAddErrorCallback,
			peer);
	}

	if(rcode != 0)
		return PEERFalse;

	connection->reportingGroupID = connection->groupID;
	
	qr2_register_natneg_callback(connection->queryReporting, piQRNatNegotiateCallback);
	
	qr2_register_publicaddress_callback(connection->queryReporting, piQRPublicAddressCallback);

	connection->reportingOptions = 0;

	return PEERTrue;
}

void piStopReporting
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(!connection->queryReporting)
		return;

	qr2_shutdown(connection->queryReporting);
	connection->queryReporting = NULL;
}

void piSendStateChanged
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(!connection->queryReporting)
		return;

	qr2_send_statechanged(connection->queryReporting);
}

void piQRThink
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(connection->queryReporting)
		qr2_think(connection->queryReporting);
	if(connection->autoMatchReporting)
		qr2_think(connection->autoMatchReporting);
}

PEERBool piStartAutoMatchReporting
(
	PEER peer
)
{
	piOperation * operation;
	char autoMatchTitle[PI_TITLE_MAX_LEN];
	int rcode;

	PEER_CONNECTION;

	assert(!connection->autoMatchReporting);
	if(connection->autoMatchReporting)
		piStopAutoMatchReporting(peer);

	operation = connection->autoMatchOperation;
	assert(operation);

	strzcpy(autoMatchTitle, connection->title, sizeof(autoMatchTitle));
	strzcat(autoMatchTitle, "am", sizeof(autoMatchTitle));

	if(operation->socket == INVALID_SOCKET)
	{
		rcode = qr2_initA(&connection->autoMatchReporting, NULL, PI_QUERYPORT, autoMatchTitle, connection->qrSecretKey,
			1, connection->natNegotiate,
			piQRServerKeyCallback,
			piQRPlayerKeyCallback,
			piQRTeamKeyCallback,
			piQRKeyListCallback,
			piQRCountCallback,
			piQRAddErrorCallback,
			peer);
	}
	else
	{
		rcode = qr2_init_socketA(&connection->autoMatchReporting, operation->socket, operation->port, autoMatchTitle, connection->qrSecretKey,
			1, connection->natNegotiate,
			piQRServerKeyCallback,
			piQRPlayerKeyCallback,
			piQRTeamKeyCallback,
			piQRKeyListCallback,
			piQRCountCallback,
			piQRAddErrorCallback,
			peer);

		if(operation->socketClose)
		{
			operation->socketClose = PEERFalse;
			connection->autoMatchReporting->read_socket = 1;
		}
	}

	connection->autoMatchQRFailed = (rcode == 0)?PEERTrue:PEERFalse;

	if(rcode != 0)
		return PEERFalse;
	
	qr2_register_natneg_callback(connection->autoMatchReporting, piQRNatNegotiateCallback);
	
	qr2_register_publicaddress_callback(connection->autoMatchReporting, piQRPublicAddressCallback);

	return PEERTrue;
}

void piStopAutoMatchReporting
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(!connection->autoMatchReporting)
		return;

	qr2_shutdown(connection->autoMatchReporting);
	connection->autoMatchReporting = NULL;
	
	if (connection->autoMatchOperation->socket != INVALID_SOCKET)
	{
		connection->autoMatchOperation->socket = INVALID_SOCKET;
	}
}
