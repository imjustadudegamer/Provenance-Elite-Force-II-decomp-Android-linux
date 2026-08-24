/*
gpiPeer.c
GameSpy Presence SDK 
Dan "Mr. Pants" Schoenblum

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com

***********************************************************************
Please see the GameSpy Presence SDK documentation for more information
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpi.h"

static GPResult
gpiProcessPeerInitiatingConnection(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	char * str = NULL;
	GPIBool connClosed;
	GPIProfile * pProfile;
	GPResult result;
	GSUdpPeerState aPeerState;
	
	GS_ASSERT(peer);
	if (!peer)
		return GP_NETWORK_ERROR;

	GS_ASSERT(peer->state != GPI_PEER_DISCONNECTED && peer->state != GPI_PEER_NOT_CONNECTED);
	if (peer->state == GPI_PEER_DISCONNECTED || peer->state == GPI_PEER_NOT_CONNECTED)
		return GP_NETWORK_ERROR;
	switch(peer->state)
	{
		case GPI_PEER_GETTING_SIG:
			break;

		case GPI_PEER_GOT_SIG:
		{
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_State, GSIDebugLevel_Verbose, "Got the peer signature for profileid: %d\n", peer->profile);
			CHECK_RESULT(gpiPeerStartConnect(connection, peer));

			break;
		}
		case GPI_PEER_CONNECTING:
		{	

			gsUdpEngineGetPeerState(peer->ip, peer->port, &aPeerState);

			if(aPeerState == GS_UDP_PEER_CONNECTED)
			{
				GPIPeer * pcurr;
				GPIBool freePeerSig = GPITrue;

				if(!gpiGetProfile(connection, peer->profile, &pProfile))
					Error(connection, GP_NETWORK_ERROR, "Error connecting to a peer.");

				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\auth\\");
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\pid\\");
				gpiAppendIntToBuffer(connection, &peer->outputBuffer, iconnection->profileid);
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\nick\\");
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, iconnection->nick);
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\sig\\");
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, pProfile->peerSig);
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\final\\");

				for(pcurr = iconnection->peerList ; pcurr != NULL ; pcurr = pcurr->pnext)
					if((pcurr->profile == peer->profile) && (pcurr != peer))
						if(pcurr->state <= GPI_PEER_CONNECTING)
							freePeerSig = GPIFalse;

				if(freePeerSig)
				{
					freeclear(pProfile->peerSig);
					if(gpiCanFreeProfile(pProfile))
						gpiRemoveProfile(connection, pProfile);
				}

				peer->state = GPI_PEER_WAITING;
			}
			
			break;
		}
		case GPI_PEER_WAITING:
		{

			if (peer->inputBuffer.buffer)
				str = strstr(peer->inputBuffer.buffer, "\\final\\");
			if(str != NULL)
			{
				str[0] = '\0';
				str += 7;

				if(strncmp(peer->inputBuffer.buffer, "\\anack\\", 7) == 0)
				{
					peer->nackCount++;

					if(peer->nackCount > 1)
					{

						Error(connection, GP_NETWORK_ERROR, "Error getting buddy authorization.");
					}

					CHECK_RESULT(gpiPeerGetSig(connection, peer));
				}
				else if(strncmp(peer->inputBuffer.buffer, "\\aack\\", 6) != 0)
				{
					Error(connection, GP_NETWORK_ERROR, "Error parsing buddy message.");
				}

				peer->state = GPI_PEER_CONNECTED;
				peer->inputBuffer.len = 0;
			}

			break;
		}
		default: break;
	}

	if(peer->outputBuffer.len > 0)
	{
		result = gpiSendBufferToPeer(connection, peer->ip, peer->port, &peer->outputBuffer, &connClosed, GPITrue);
		if(connClosed || (result != GP_NO_ERROR))
			peer->state = GPI_PEER_DISCONNECTED;
	}
	
	return GP_NO_ERROR;
}

static GPResult
gpiProcessPeerAcceptingConnection(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GSUdpPeerState aPeerState;
	char * str;
	GPIBool connClosed;
	char intValue[16];
	int pid;
	char nick[GP_NICK_LEN];
	char sig[33];
	char sigCheck[33];
	char buffer[256];

	GS_ASSERT(peer->state == GPI_PEER_WAITING);
	if (peer->state != GPI_PEER_WAITING)
		return GP_NETWORK_ERROR;

	gsUdpEngineGetPeerState(peer->ip, peer->port, &aPeerState);

	if(aPeerState == GS_UDP_PEER_CLOSED)
	{
		peer->state = GPI_PEER_DISCONNECTED;
		return GP_NO_ERROR;
	}

	str = strstr(peer->inputBuffer.buffer, "\\final\\");
	if(str != NULL)
	{
		str[0] = '\0';
		str += 7;

		if(strncmp(peer->inputBuffer.buffer, "\\auth\\", 6) == 0)
		{
			if(!gpiValueForKey(peer->inputBuffer.buffer, "\\pid\\", intValue, sizeof(intValue)))
			{
				peer->state = GPI_PEER_DISCONNECTED;
				return GP_NO_ERROR;
			}
			pid = atoi(intValue);

			if(!gpiValueForKey(peer->inputBuffer.buffer, "\\nick\\", nick, sizeof(nick)))
			{
				peer->state = GPI_PEER_DISCONNECTED;
				return GP_NO_ERROR;
			}

			if(!gpiValueForKey(peer->inputBuffer.buffer, "\\sig\\", sig, sizeof(sig)))
			{
				peer->state = GPI_PEER_DISCONNECTED;
				return GP_NO_ERROR;
			}

			sprintf(buffer, "%s%d%d",
				iconnection->password,
				iconnection->profileid,
				pid);
			MD5Digest((unsigned char *)buffer, strlen(buffer), sigCheck);

			if(strcmp(sig, sigCheck) != 0)
			{
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\anack\\");
				gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\final\\");

				gpiSendBufferToPeer(connection, peer->ip, peer->port, &peer->outputBuffer, &connClosed, GPITrue);
				peer->state = GPI_PEER_DISCONNECTED;
				return GP_NO_ERROR;
			}

			gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\aack\\");
			gpiAppendStringToBuffer(connection, &peer->outputBuffer, "\\final\\");
			
			peer->state = GPI_PEER_CONNECTED;
			peer->profile = (GPProfile)pid;
		}
		else
		{
			peer->state = GPI_PEER_DISCONNECTED;
			return GP_NO_ERROR;
		}
		
		peer->inputBuffer.len = 0;
	}

	return GP_NO_ERROR;
}

GPResult
gpiPeerSendMessages(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIBool connClosed;
	GPIMessage * message;
	GPResult result;

	GS_ASSERT(peer);
	if (!peer)
		return GP_NETWORK_ERROR;
	if(peer->outputBuffer.len)
		return GP_NO_ERROR;

	while(ArrayLength(peer->messages))
	{
		message = (GPIMessage *)ArrayNth(peer->messages, 0);

		result = gpiSendBufferToPeer(connection, peer->ip, peer->port, &message->buffer, &connClosed, GPIFalse);
		if(connClosed || (result != GP_NO_ERROR))
		{
			peer->state = GPI_PEER_DISCONNECTED;
			return GP_NO_ERROR;
		}

		if(message->buffer.pos != message->buffer.len)
			break;

		ArrayDeleteAt(peer->messages, 0);
	}

	return GP_NO_ERROR;
}

static GPResult
gpiProcessPeerConnected(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GSUdpPeerState aPeerState;
	GPIBool connClosed;
	GPICallback callback;
	char * buffer;
	int type;
	int messageLen;
	GPResult result;

	GS_ASSERT(peer);
	if (!peer)
		return GP_NETWORK_ERROR;
	if(peer->outputBuffer.len)
	{
		result = gpiSendBufferToPeer(connection, peer->ip, peer->port, &peer->outputBuffer, &connClosed, GPITrue);
		if(connClosed || (result != GP_NO_ERROR))
		{
			peer->state = GPI_PEER_DISCONNECTED;
			return GP_NO_ERROR;
		}
	}

	if(!peer->outputBuffer.len)
	{
		CHECK_RESULT(gpiPeerSendMessages(connection, peer));
		if(peer->state == GPI_PEER_DISCONNECTED)
			return GP_NO_ERROR;
	}

	if(peer->inputBuffer.len > 0)
	{
		peer->timeout = (time(NULL) + GPI_PEER_TIMEOUT);
	}

	do
	{
		CHECK_RESULT(gpiReadMessageFromBuffer(connection, &peer->inputBuffer, &buffer, &type, &messageLen));
		if(buffer != NULL)
		{
			switch(type)
			{
			case GPI_BM_MESSAGE:
				callback = iconnection->callbacks[GPI_RECV_BUDDY_MESSAGE];
				if(callback.callback != NULL)
				{
					GPRecvBuddyMessageArg * arg;

					arg = (GPRecvBuddyMessageArg *)gsimalloc(sizeof(GPRecvBuddyMessageArg));
					if(arg == NULL)
						Error(connection, GP_MEMORY_ERROR, "Out of memory.");
					arg->profile = peer->profile;
#ifndef GSI_UNICODE
					arg->message = goastrdup(buffer);
#else
					arg->message = UTF8ToUCS2StringAlloc(buffer);
#endif
					arg->date = (unsigned int)time(NULL);
					CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_MESSAGE));
				}
				break;
				
			case GPI_BM_UTM:
				callback = iconnection->callbacks[GPI_RECV_BUDDY_UTM];
				if (callback.callback != NULL)
				{
					GPRecvBuddyUTMArg * arg;

					arg = (GPRecvBuddyUTMArg *)gsimalloc(sizeof(GPRecvBuddyUTMArg));
					if(arg == NULL)
						Error(connection, GP_MEMORY_ERROR, "Out of memory.");
					arg->profile = peer->profile;
#ifndef GSI_UNICODE
					arg->message = goastrdup(buffer);
#else
					arg->message = UTF8ToUCS2StringAlloc(buffer);
#endif
					arg->date = (unsigned int)time(NULL);
					CHECK_RESULT(gpiAddCallback(connection, callback, arg, NULL, GPI_ADD_MESSAGE));
				}
				break;

			case GPI_BM_PING:
				gpiSendBuddyMessage(connection, peer->profile, GPI_BM_PONG, "1", 0, NULL);

				break;

#ifndef NOFILE
			case GPI_BM_PONG:
				gpiTransfersHandlePong(connection, peer->profile, peer);
				break;
#endif
			case GPI_BM_KEYS_REQUEST:
				CHECK_RESULT(gpiBuddyHandleKeyRequest(connection, peer));
				break;
			case GPI_BM_KEYS_REPLY:
				CHECK_RESULT(gpiBuddyHandleKeyReply(connection, peer, buffer));
				break;
			case GPI_BM_FILE_SEND_REQUEST:
			case GPI_BM_FILE_SEND_REPLY:
			case GPI_BM_FILE_BEGIN:
			case GPI_BM_FILE_END:
			case GPI_BM_FILE_DATA:
			case GPI_BM_FILE_SKIP:
			case GPI_BM_FILE_TRANSFER_THROTTLE:
			case GPI_BM_FILE_TRANSFER_CANCEL:
			case GPI_BM_FILE_TRANSFER_KEEPALIVE:
				gpiHandleTransferMessage(connection, peer, type, peer->inputBuffer.buffer, buffer, messageLen);


				break;

			default:
				break;
			}

			gpiClipBufferToPosition(connection, &peer->inputBuffer);
		}
	}
	while(buffer);

	gsUdpEngineGetPeerState(peer->ip, peer->port, &aPeerState);
	if (aPeerState == GS_UDP_PEER_CLOSED)
		peer->state = GPI_PEER_DISCONNECTED;
	
	return GP_NO_ERROR;
}


void gpiCheckTimedOutPeerOperations(GPConnection * connection, GPIPeer *peer)
{
	GPIPeerOp *anIterator = peer->peerOpQueue.first;
	GS_ASSERT(peer);
	if (!peer)
		return;
	
	while (anIterator && anIterator != peer->peerOpQueue.last)
	{
		if (anIterator->state != GPI_PEER_OP_STATE_FINISHED &&  current_time() > anIterator->timeout && anIterator->callback)
		{
			if (anIterator->type == GPI_BM_KEYS_REQUEST)
			{	
				GPICallback callback;
				GPGetBuddyStatusInfoKeysArg *arg = (GPGetBuddyStatusInfoKeysArg *)gsimalloc(sizeof(GPGetBuddyStatusInfoKeysArg));
				callback.callback = anIterator->callback;
				callback.param = anIterator->userData;
				arg->keys = NULL;
				arg->numKeys = 0;
				arg->values = NULL;
				arg->profile = peer->profile;
				gpiAddCallback(connection, callback, arg, NULL, 0);
				
			}
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Notice, "Peer operation timed out");
			gpiPeerRemoveOp(peer, anIterator);
		}
		anIterator = anIterator->next;
	}
}


static GPResult
gpiProcessPeer(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPResult result = GP_NO_ERROR;
	
	GS_ASSERT(peer->state != GPI_PEER_NOT_CONNECTED);
	if (peer->state == GPI_PEER_NOT_CONNECTED)
		return GP_NETWORK_ERROR;

	if(peer->state != GPI_PEER_CONNECTED)
	{
		if(peer->initiated)
			result = gpiProcessPeerInitiatingConnection(connection, peer);
		else
			result = gpiProcessPeerAcceptingConnection(connection, peer);
	}

	if((result == GP_NO_ERROR) && (peer->state == GPI_PEER_CONNECTED))
	{
		result = gpiProcessPeerConnected(connection, peer);
		gpiCheckTimedOutPeerOperations(connection, peer);
	}

	return result;
}

void
gpiDestroyPeer(
  GPConnection * connection,
  GPIPeer * peer
)
{
#ifndef NOFILE
	gpiTransferPeerDestroyed(connection, peer);
#endif

	freeclear(peer->inputBuffer.buffer);
	freeclear(peer->outputBuffer.buffer);
	if(peer->messages)
	{
		ArrayFree(peer->messages);
		peer->messages = NULL;
	}
	freeclear(peer);
	
	GSI_UNUSED(connection);
}

void
gpiRemovePeer(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIPeer * pprev;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIMessage * message;

	GS_ASSERT(peer != NULL);
	if (peer == NULL)
		return;

	GS_ASSERT(iconnection->peerList);
	if (iconnection->peerList == NULL)
		return;
	if(iconnection->peerList == peer)
	{
		iconnection->peerList = peer->pnext;
	}
	else
	{
		for(pprev = iconnection->peerList ; pprev->pnext != peer ; pprev = pprev->pnext)
		{
			if(pprev->pnext == NULL)
			{
				assert(0);
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
					"Tried to remove peer not in list.");
				return;
			}
		}
		pprev->pnext = peer->pnext;
	}

	while(ArrayLength(peer->messages))
	{
		message = (GPIMessage *)ArrayNth(peer->messages, 0);

		if(message->type < 100)
			gpiSendServerBuddyMessage(connection, peer->profile, message->type, message->buffer.buffer + message->start);

		ArrayDeleteAt(peer->messages, 0);
	}

	gpiDestroyPeer(connection, peer);
}

GPResult gpiProcessPeers(GPConnection *connection)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIPeer * nextPeer;
	GPIPeer * peer;
	GPResult result;

	gsUdpEngineThink();
	for(peer = iconnection->peerList ; peer != NULL ; peer = nextPeer)
	{
		nextPeer = peer->pnext;
		if(peer->state == GPI_PEER_DISCONNECTED)
		{
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Notice, "Peer disconnected, pid: %d", peer->profile);
			gpiRemovePeer(connection, peer);
		}
		else
		{
			result = gpiProcessPeer(connection, peer);

			if((peer->state == GPI_PEER_DISCONNECTED) || (result != GP_NO_ERROR) || (time(NULL) > peer->timeout))
			{
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Notice, "Peer disconnected, pid: %d", peer->profile);
				gpiRemovePeer(connection, peer);
			}
		}		
	}
	
	return GP_NO_ERROR;
}

// NOTE: use this function when in a gp function
GPIPeer * gpiGetPeerByProfile(const GPConnection * connection,
							  int profileid)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIPeer * pcurr;

	for(pcurr = iconnection->peerList ; pcurr != NULL ; pcurr  = pcurr->pnext)
	{
		if(pcurr->profile == profileid)
		{
			return pcurr;
		}
	}

	return NULL;
}

// NOTE: use this function only when in a UDP layer callback
GPIPeer * gpiGetPeerByAddr(const GPConnection *connection,
                           unsigned int ip,
                           unsigned short port)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIPeer * pcurr;

	GS_ASSERT(ip);
	GS_ASSERT(port);
	if (!ip && !port)
		return NULL;
	for(pcurr = iconnection->peerList ; pcurr != NULL ; pcurr  = pcurr->pnext)
	{
		if(pcurr->ip == ip && pcurr->port == port)
		{
			return pcurr;
		}
	}

	return NULL;
}

gsi_bool gpiIsPeerConnected(GPIPeer *peer)
{	
	GS_ASSERT(peer);
	if (!peer)
		return gsi_false;

	if (peer && peer->state != GPI_PEER_CONNECTED)
		return gsi_false;
	
	return gsi_true;
}

static void gpiFreeMessage(void * elem)
{
	GPIMessage * message = (GPIMessage *)elem;

	freeclear(message->buffer.buffer);
}

GPIPeer *
gpiAddPeer(
  GPConnection * connection,
  int profileid,
  GPIBool initiate
)
{
	GPIPeer * peer;
	GPIConnection * iconnection = (GPIConnection*)*connection;

	peer = (GPIPeer *)gsimalloc(sizeof(GPIPeer));
	if(peer == NULL)
		return NULL;
	memset(peer, 0, sizeof(GPIPeer));
	peer->state = GPI_PEER_NOT_CONNECTED;
	peer->initiated = initiate;
	peer->profile = profileid;
	peer->timeout = (time(NULL) + GPI_PEER_TIMEOUT);
	peer->pnext = iconnection->peerList;
	peer->messages = ArrayNew(sizeof(GPIMessage), 0, gpiFreeMessage);
	iconnection->peerList = peer;
	peer->peerOpQueue.first = NULL;
	peer->peerOpQueue.last = NULL;
	peer->peerOpQueue.opList = NULL;
	return peer;
}

GPResult
gpiPeerGetSig(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIOperation * operation;

	CHECK_RESULT(gpiAddOperation(connection, GPI_GET_INFO, NULL, &operation, GP_NON_BLOCKING, NULL, NULL));

	CHECK_RESULT(gpiSendGetInfo(connection, peer->profile, operation->id));

	peer->state = GPI_PEER_GETTING_SIG;

	return GP_NO_ERROR;
}

GPResult
gpiPeerStartConnect(
  GPConnection * connection,
  GPIPeer * peer
)
{
	GPIProfile * profile;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GSUdpErrorCode anError;

	if(!gpiGetProfile(connection, peer->profile, &profile))
		Error(connection, GP_NETWORK_ERROR, "Error connecting to a peer.");


	if (profile->buddyStatusInfo)
	{
		GSUdpPeerState aPeerState;
		gsUdpEngineGetPeerState(profile->buddyStatusInfo->buddyIp , profile->buddyStatusInfo->buddyPort, &aPeerState);
		if (aPeerState != GS_UDP_PEER_CONNECTED || aPeerState != GS_UDP_PEER_CONNECTING)
		{
			anError = gsUdpEngineStartTalkingToPeer(profile->buddyStatusInfo->buddyIp , profile->buddyStatusInfo->buddyPort, 
			iconnection->mHeader, GPI_PEER_TIMEOUT);
			if (anError != GS_UDP_ADDRESS_ALREADY_IN_USE)
				CallbackError(connection, GP_NETWORK_ERROR, GP_NETWORK, "There was an error starting communication with a peer.");
		}
		peer->ip = profile->buddyStatusInfo->buddyIp;
		peer->port = profile->buddyStatusInfo->buddyPort;
	}
	peer->state = GPI_PEER_CONNECTING;

	return GP_NO_ERROR;
}

GPResult
gpiPeerAddMessage(
  GPConnection * connection,
  GPIPeer * peer,
  int type,
  const char * message
)
{
	GPIMessage gpiMessage;
	int len;

	GS_ASSERT(peer != NULL);
	GS_ASSERT(message != NULL);
	
	if (peer == NULL)
		return GP_NETWORK_ERROR;
	if (message == NULL)
		return GP_NETWORK_ERROR;

	len = (int)strlen(message);

	memset(&gpiMessage, 0, sizeof(GPIMessage));

	gpiMessage.type = type;

	CHECK_RESULT(gpiAppendStringToBuffer(connection, &gpiMessage.buffer, "\\m\\"));
	CHECK_RESULT(gpiAppendIntToBuffer(connection, &gpiMessage.buffer, type));
	CHECK_RESULT(gpiAppendStringToBuffer(connection, &gpiMessage.buffer, "\\len\\"));
	CHECK_RESULT(gpiAppendIntToBuffer(connection, &gpiMessage.buffer, len));
	CHECK_RESULT(gpiAppendStringToBuffer(connection, &gpiMessage.buffer, "\\msg\\\n"));

	gpiMessage.start = gpiMessage.buffer.len;
	CHECK_RESULT(gpiAppendStringToBufferLen(connection, &gpiMessage.buffer, message, len));
	CHECK_RESULT(gpiAppendCharToBuffer(connection, &gpiMessage.buffer, '\0'));

	ArrayAppend(peer->messages, &gpiMessage);

	peer->timeout = (time(NULL) + GPI_PEER_TIMEOUT);

	return GP_NO_ERROR;
}

GPResult
gpiPeerStartTransferMessage(
  GPConnection * connection,
  GPIPeer * peer,
  int type,
  const struct GPITransferID_s * transferID
)
{
	char buffer[64];
	GPITransferID tid;
	tid.count = transferID->count;
	tid.profileid = transferID->profileid;
	tid.time = transferID->time;

	GS_ASSERT(transferID);
	if (!transferID)
		return GP_NETWORK_ERROR;
	sprintf(buffer, "\\m\\%d\\xfer\\%d %u %u", type, tid.profileid, tid.count, tid.time);

	return gpiSendOrBufferString(connection, peer, buffer);
}

GPResult
gpiPeerFinishTransferMessage(
  GPConnection * connection,
  GPIPeer * peer,
  const char * message,
  int len
)
{
	char buffer[32];
	GS_ASSERT(peer != NULL);
	if (!peer)
		return GP_NETWORK_ERROR;

	if(!message)
		message = "";
	
	if(len == -1)
		len = (int)strlen(message);

	sprintf(buffer, "\\len\\%d\\msg\\\n", len);
	CHECK_RESULT(gpiSendOrBufferString(connection, peer, buffer));

	CHECK_RESULT(gpiSendOrBufferStringLenToPeer(connection, peer, message, len));
	CHECK_RESULT(gpiSendOrBufferChar(connection, peer, '\0'));

	peer->timeout = (time(NULL) + GPI_PEER_TIMEOUT);
		
	return GP_NO_ERROR;
}

void gpiPeerLeftCallback(unsigned int ip, unsigned short port, GSUdpCloseReason reason, void *userData)
{

	GPConnection *connection = (GPConnection *)userData;
	GPIPeer *aPeer;
	IN_ADDR anAddr;
	anAddr.s_addr = ip;
	aPeer = gpiGetPeerByAddr(connection, ip, port);
	if (aPeer)
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_Notice, 
			"Peer left: addr: %s:%d, profile: %d\n", inet_ntoa(anAddr), port, aPeer->profile);
		aPeer->state = GPI_PEER_DISCONNECTED;
	}
	
	GSI_UNUSED(anAddr);
	GSI_UNUSED(reason);
}

void gpiPeerMessageCallback(unsigned int ip, unsigned short port, unsigned char *message, 
							unsigned int messageLength, gsi_bool reliable, void *userData)
{
	GPConnection *connection = (GPConnection *)userData;
	GPIPeer *aPeer;
	unsigned char * buff;
	int writePos;
	int size;
	IN_ADDR anAddr;
	anAddr.s_addr = ip;
	aPeer = gpiGetPeerByAddr(connection, ip, port);
	if (!aPeer)
	{
		aPeer = gpiAddPeer(connection, -1, GPIFalse);
		if (aPeer)
		{
			aPeer->state = GPI_PEER_WAITING;
			aPeer->ip = ip;
			aPeer->port = port;
		}
		else 
		{
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Memory, GSIDebugLevel_HotError, 
				"gpiPeerMessageCallback: out of memory when allocating peer, addr: %s:%d", inet_ntoa(anAddr), port);
			return;
		}
	}

	buff = (unsigned char *)aPeer->inputBuffer.buffer;
	writePos = aPeer->inputBuffer.len;
	size = aPeer->inputBuffer.size;

	if((int)messageLength > (size - writePos))
	{
		unsigned char *reallocedBuff;
		size = (writePos + max(GPI_READ_SIZE,(int)messageLength));
		reallocedBuff = (unsigned char *)gsirealloc(buff, (unsigned int)size + 1);
		if(reallocedBuff == NULL)
		{
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Memory, GSIDebugLevel_HotError, 
				"gpiPeerMessageCallback: out of memory when reallocating buffer, addr: %s:%d", inet_ntoa(anAddr), port);
			gsifree(buff);
			gpiSetErrorString(connection, "Out of memory.");
			gpiCallErrorCallback(connection, GP_MEMORY_ERROR, GP_NON_FATAL);
			return;
		}
		else
			buff = reallocedBuff;
	}

	memcpy(&buff[writePos], message, messageLength);

	aPeer->inputBuffer.buffer = (char *)buff;
	aPeer->inputBuffer.len += messageLength;
	aPeer->inputBuffer.size = size;
	buff[aPeer->inputBuffer.len] = '\0';
	GSI_UNUSED(reliable);
	GSI_UNUSED(anAddr);
}

void gpiPeerAcceptedCallback(unsigned int ip, unsigned short port, 
							 GSUdpErrorCode error, gsi_bool rejected, void *userData)
{
	GPConnection *connection = (GPConnection *)userData;
	GPIPeer *aPeer;
	IN_ADDR anAddr;
	anAddr.s_addr = ip;
	
	aPeer = gpiGetPeerByAddr(connection, ip, port);
	if (!aPeer)
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_HotError,
			"Peer does not exist: ip-port: %s:%d\n", inet_ntoa(anAddr), port);
	}
	else
	{
		if (rejected)
		{
			aPeer->state = GPI_PEER_DISCONNECTED;
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_Notice,
				"Peer Connection rejected: ip-port: %s:%d\n", inet_ntoa(anAddr), port);
			return;
		}
	}
	
	gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_Notice,
		"Peer Connection accepted: ip-port: %s:%d\n", inet_ntoa(anAddr), port);
	
	GSI_UNUSED(userData);
	GSI_UNUSED(rejected);
	GSI_UNUSED(error);
	GSI_UNUSED(anAddr);	
}
void gpiPeerPingReplyCallback(unsigned int ip, unsigned short port, unsigned int latency, void *userData)
{
	GSI_UNUSED(userData);
	GSI_UNUSED(latency);
	GSI_UNUSED(port);
	GSI_UNUSED(ip);
}

void gpiPeerAddOp(GPIPeer *peer, GPIPeerOp *operation)
{
	GS_ASSERT(peer);
	GS_ASSERT(operation);

	if (!peer || !operation)
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_WarmError, "Peer operation not added");
		return;
	}
	if (peer->peerOpQueue.opList == NULL)
	{
		peer->peerOpQueue.first = operation;
		peer->peerOpQueue.last = operation;
		peer->peerOpQueue.opList = operation;		
	}
	else if (peer->peerOpQueue.first == peer->peerOpQueue.last)
	{
		peer->peerOpQueue.first->next = operation;
		peer->peerOpQueue.last = operation;
	}
	else
	{
		peer->peerOpQueue.last->next = operation;
		peer->peerOpQueue.last = operation;		
	}

	gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Notice, "Peer Operation Added");
}

void gpiPeerRemoveOp(GPIPeer *peer, GPIPeerOp *operation)
{
	GS_ASSERT(peer);
	GS_ASSERT(operation);
	if (!peer || !operation)
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_WarmError, "Peer operation not removed");
		return;
	}

	GS_ASSERT(peer->peerOpQueue.opList != NULL);
	if (peer->peerOpQueue.opList == NULL)
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_WarmError, "Peer operation not removed");
		return;
	}

	if (peer->peerOpQueue.first == peer->peerOpQueue.last && peer->peerOpQueue.first == operation)
	{
		peer->peerOpQueue.opList = peer->peerOpQueue.first = peer->peerOpQueue.last = operation->next;
	}
	else if (peer->peerOpQueue.first == operation)
	{
		peer->peerOpQueue.first = peer->peerOpQueue.first->next;
		peer->peerOpQueue.opList = peer->peerOpQueue.first;
	}
	else
	{
		GPIPeerOp *aPrevOp = NULL;
		for(aPrevOp = peer->peerOpQueue.first ; aPrevOp->next != operation ; aPrevOp = aPrevOp->next)
		{
			if(aPrevOp->next == NULL)
			{
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
					"Tried to remove peer operation not in list.");
				return;
			}
		}
		aPrevOp->next = operation->next;
	}

	gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Notice, "Peer operation removed");
	freeclear(operation);
}
