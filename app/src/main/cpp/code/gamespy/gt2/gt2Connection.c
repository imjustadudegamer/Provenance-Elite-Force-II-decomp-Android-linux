/*
GameSpy GT2 SDK
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 2002 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "gt2Connection.h"
#include "gt2Socket.h"
#include "gt2Message.h"
#include "gt2Callback.h"
#include "gt2Utility.h"
#include <stdlib.h>

GT2Result gti2NewOutgoingConnection(GT2Socket socket, GT2Connection * connection, unsigned int ip, unsigned short port)
{
	GT2Result result;

	result = gti2NewSocketConnection(socket, connection, ip, port);
	if(result != GT2Success)
		return result;

	(*connection)->state = GTI2AwaitingServerChallenge;
	(*connection)->initiated = GT2True;

	return GT2Success;
}

GT2Result gti2NewIncomingConnection(GT2Socket socket, GT2Connection * connection, unsigned int ip, unsigned short port)
{
	GT2Result result;

	result = gti2NewSocketConnection(socket, connection, ip, port);
	if(result != GT2Success)
		return result;

	(*connection)->state = GTI2AwaitingClientChallenge;
	(*connection)->initiated = GT2False;

	return GT2Success;
}

GT2Result gti2StartConnectionAttempt
(
	GT2Connection connection,
	const GT2Byte * message,
	int len,
	GT2ConnectionCallbacks * callbacks
)
{
	char challenge[GTI2_CHALLENGE_LEN];

	gti2MessageCheck(&message, &len);

	if(len > 0)
	{
		connection->initialMessage = (char *)gsimalloc((unsigned int)len);
		if(!connection->initialMessage)
			return GT2OutOfMemory;

		memcpy(connection->initialMessage, message, (unsigned int)len);
		connection->initialMessageLen = len;
	}

	if(callbacks)
		connection->callbacks = *callbacks;

	gti2GetChallenge((GT2Byte *)challenge);

	gti2GetResponse((GT2Byte *)connection->response, (GT2Byte *)challenge);

	gti2SendClientChallenge(connection, challenge);

	connection->state = GTI2AwaitingServerChallenge;

	return GT2Success;
}

GT2Bool gti2AcceptConnection(GT2Connection connection, GT2ConnectionCallbacks * callbacks)
{
	if(connection->freeAtAcceptReject)
	{
		connection->freeAtAcceptReject = GT2False;

		return GT2False;
	}

	connection->freeAtAcceptReject = GT2False;

	if(connection->state != GTI2AwaitingAcceptReject)
		return GT2False;

	gti2SendAccept(connection);

	connection->state = GTI2Connected;

	if(callbacks)
		connection->callbacks = *callbacks;

	return GT2True;
}

void gti2RejectConnection(GT2Connection connection, const GT2Byte * message, int len)
{
	connection->freeAtAcceptReject = GT2False;

	if(connection->state != GTI2AwaitingAcceptReject)
		return;

	gti2MessageCheck(&message, &len);

	gti2SendReject(connection, message, len);

	connection->state = GTI2Closing;
}

GT2Bool gti2ConnectionSendData(GT2Connection connection, const GT2Byte * message, int len)
{
	if(!gti2SocketSend(connection->socket, connection->ip, connection->port, message, len))
		return GT2False;

	connection->lastSend = current_time();

	return GT2True;
}

static GT2Bool gti2CheckTimeout(GT2Connection connection, gsi_time now)
{
	if(connection->state < GTI2Connected)
	{
		GT2Bool timedOut = GT2False;

		if(connection->initiated)
		{
			if(connection->timeout)
			{
				if((now - connection->startTime) > connection->timeout)
					timedOut = GT2True;
			}
		}
		else
		{
			if(connection->state < GTI2AwaitingAcceptReject)
			{
				if((now - connection->startTime) > GTI2_SERVER_TIMEOUT)
					timedOut = GT2True;
			}
		}

		if(timedOut)
		{
			gti2SendClosed(connection);

			gti2ConnectionClosed(connection);

			if(!gti2ConnectedCallback(connection, GT2TimedOut, NULL, 0))
				return GT2False;
		}
	}

	return GT2True;
}

static GT2Bool gti2SendRetries(GT2Connection connection, gsi_time now)
{
	int i;
	int len;
	GTI2OutgoingBufferMessage * message;

	len = ArrayLength(connection->outgoingBufferMessages);
	for(i = 0 ; i < len ; i++)
	{
		message = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, i);

		if((now - message->lastSend) > GTI2_RESEND_TIME)
		{
			if(!gti2ResendMessage(connection, message))
				return GT2False;
		}
	}

	return GT2True;
}

static GT2Bool gti2CheckPendingAck(GT2Connection connection, gsi_time now)
{
	if(!connection->pendingAck)
		return GT2True;

	if((now - connection->pendingAckTime) > GTI2_PENDING_ACK_TIME)
	{
		if(!gti2SendAck(connection))
			return GT2False;
	}

	return GT2True;
}

static GT2Bool gti2CheckKeepAlive(GT2Connection connection, gsi_time now)
{
	if((now - connection->lastSend) > GTI2_KEEP_ALIVE_TIME)
	{
		if(!gti2SendKeepAlive(connection))
			return GT2False;
	}

	return GT2True;
}

GT2Bool gti2ConnectionThink(GT2Connection connection, gsi_time now)
{
	if(!gti2CheckTimeout(connection, now))
		return GT2False;

	if(!gti2CheckKeepAlive(connection, now))
		return GT2False;

	if(!gti2SendRetries(connection, now))
		return GT2False;

	if(!gti2CheckPendingAck(connection, now))
		return GT2False;

	return GT2True;
}

void gti2CloseConnection(GT2Connection connection, GT2Bool hard)
{
	if(hard)
	{
		if(connection->state >= GTI2Closed)
			return;

		gti2ConnectionClosed(connection);

		gti2SendClosed(connection);

		gti2ClosedCallback(connection, GT2LocalClose);

		gti2FreeSocketConnection(connection);
	}
	else
	{
		connection->state = GTI2Closing;

		gti2SendClose(connection);
	}
}

void gti2ConnectionClosed(GT2Connection connection)
{
	if(connection->state == GTI2Closed)
		return;

	connection->state = GTI2Closed;

	TableRemove(connection->socket->connections, &connection);

	ArrayAppend(connection->socket->closedConnections, &connection);
}

void gti2ConnectionCleanup(GT2Connection connection)
{
	if(connection->initialMessage)
		gsifree(connection->initialMessage);

	if(connection->incomingBuffer.buffer)
		gsifree(connection->incomingBuffer.buffer);
	if(connection->outgoingBuffer.buffer)
		gsifree(connection->outgoingBuffer.buffer);

	if(connection->incomingBufferMessages)
		ArrayFree(connection->incomingBufferMessages);
	if(connection->outgoingBufferMessages)
		ArrayFree(connection->outgoingBufferMessages);
	
	if(connection->sendFilters)
		ArrayFree(connection->sendFilters);
	if(connection->receiveFilters)
		ArrayFree(connection->receiveFilters);

	gsifree(connection);
}
