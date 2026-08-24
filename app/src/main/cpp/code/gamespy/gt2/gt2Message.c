/*
GameSpy GT2 SDK
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 2002 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "gt2Message.h"
#include "gt2Buffer.h"
#include "gt2Connection.h"
#include "gt2Socket.h"
#include "gt2Callback.h"
#include "gt2Utility.h"
#include <stdlib.h>

static unsigned short gti2UShortFromBuffer(const GT2Byte * buffer, int pos)
{
	unsigned short s;
	s = (unsigned short)((buffer[pos] << 8) & 0xFF00);
	pos++;
	s |= buffer[pos];

	return s;
}


static void gti2UShortToBuffer(GT2Byte * buffer, int pos, unsigned short s)
{
	buffer[pos++] = (GT2Byte)((s >> 8) & 0xFF);
	buffer[pos] = (GT2Byte)(s & 0xFF);
}

static int gti2SNDiff(unsigned short SN1, unsigned short SN2)
{
	return (short)(SN1 - SN2);
}

static GT2Bool gti2ConnectionError(GT2Connection connection, GT2Result result, GT2CloseReason reason)
{
	if(connection->state < GTI2Connected)
	{
		if(connection->initiated)
		{
			gti2ConnectionClosed(connection);

			if(!gti2ConnectedCallback(connection, result, NULL, 0))
				return GT2False;
		}
		else
		{
			if(connection->state == GTI2AwaitingAcceptReject)
				connection->freeAtAcceptReject = GT2True;

			gti2ConnectionClosed(connection);
		}
	}
	else if(connection->state != GTI2Closed)
	{
		gti2ConnectionClosed(connection);

		if(!gti2ClosedCallback(connection, reason))
			return GT2False;
	}

	return GT2True;
}

static GT2Bool gti2ConnectionCommunicationError(GT2Connection connection)
{
	return gti2ConnectionError(connection, GT2NegotiationError, GT2CommunicationError);
}

static GT2Bool gti2ConnectionMemoryError(GT2Connection connection)
{
	if(!gti2SendClosed(connection))
		return GT2False;

	return gti2ConnectionError(connection, GT2OutOfMemory, GT2NotEnoughMemory);
}





static GT2Bool gti2HandleESN(GT2Connection connection, unsigned short ESN)
{
	int len;
	int i;
	GTI2OutgoingBufferMessage * message;
	int shortenBy;

	len = ArrayLength(connection->outgoingBufferMessages);
	if(!len)
		return GT2True;

	for(i = 0 ; i < len ; i++)
	{
		message = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, i);

		if(gti2SNDiff(message->serialNumber, ESN) >= 0)
			break;
	}

	if(i == 0)
		return GT2True;

	while(i--)
		ArrayDeleteAt(connection->outgoingBufferMessages, i);

	len = ArrayLength(connection->outgoingBufferMessages);
	if(!len)
	{
		connection->outgoingBuffer.len = 0;
		return GT2True;
	}

	message = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, 0);
	shortenBy = message->start;

	for(i = 0 ; i < len ; i++)
	{
		message = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, i);
		message->start -= shortenBy;
	}

	gti2BufferShorten(&connection->outgoingBuffer, 0, shortenBy);

	return GT2True;
}

static GT2Bool gti2HandleAppUnreliable(GT2Connection connection, GT2Byte * message, int len)
{
	if((connection->state != GTI2Connected) && (connection->state != GTI2Closing))
		return GT2True;

	if(ArrayLength(connection->receiveFilters))
	{
		if(!gti2ReceiveFilterCallback(connection, 0, message, len, GT2False))
			return GT2False;
		return GT2True;
	}

	if(!gti2ReceivedCallback(connection, message, len, GT2False))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleAppReliable(GT2Connection connection, GT2Byte * message, int len)
{
	if((connection->state != GTI2Connected) && (connection->state != GTI2Closing))
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;
	}
	else
	{
		if(ArrayLength(connection->receiveFilters))
		{
			if(!gti2ReceiveFilterCallback(connection, 0, message, len, GT2True))
				return GT2False;
			return GT2True;
		}

		if(!gti2ReceivedCallback(connection, message, len, GT2True))
			return GT2False;
	}

	return GT2True;
}

static GT2Bool gti2HandleClientChallenge(GT2Connection connection, GT2Byte * message, int len)
{
	char response[GTI2_RESPONSE_LEN];
	char challenge[GTI2_CHALLENGE_LEN];

	if(connection->state != GTI2AwaitingClientChallenge)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	if(len < GTI2_CHALLENGE_LEN)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	gti2GetResponse((GT2Byte *)response, message);

	gti2GetChallenge((GT2Byte *)challenge);

	gti2GetResponse((GT2Byte *)connection->response, (GT2Byte *)challenge);

	if(!gti2SendServerChallenge(connection, response, challenge))
		return GT2False;

	connection->state = GTI2AwaitingClientResponse;

	return GT2True;
}



static GT2Bool gti2HandleServerChallenge(GT2Connection connection, GT2Byte * message, int len)
{
	char response[GTI2_RESPONSE_LEN];

	if(connection->state != GTI2AwaitingServerChallenge)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	if(len < (GTI2_RESPONSE_LEN + GTI2_CHALLENGE_LEN))
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	if(!gti2CheckResponse(message, (GT2Byte *)connection->response))
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	gti2GetResponse((GT2Byte *)response, message + GTI2_RESPONSE_LEN);

	if(!gti2SendClientResponse(connection, response, connection->initialMessage, connection->initialMessageLen))
		return GT2False;

	if(connection->initialMessage)
	{
		gsifree(connection->initialMessage);
		connection->initialMessage = NULL;
	}

	connection->state = GTI2AwaitingAcceptance;

	return GT2True;
}

static GT2Bool gti2HandleClientResponse(GT2Connection connection, GT2Byte * message, int len)
{
	int latency;

	if(connection->state != GTI2AwaitingClientResponse)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	if(len < (GTI2_RESPONSE_LEN))
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	if(!gti2CheckResponse(message, (GT2Byte *)connection->response))
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	if(!connection->socket->connectAttemptCallback)
	{
		if(!gti2SendClosed(connection))
			return GT2False;

		gti2ConnectionClosed(connection);

		return GT2True;
	}

	connection->state = GTI2AwaitingAcceptReject;

	latency = (int)(current_time() - connection->challengeTime);

	if(!gti2ConnectAttemptCallback(connection->socket, connection, connection->ip, connection->port, latency, message + GTI2_RESPONSE_LEN, len - GTI2_RESPONSE_LEN))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleAccept(GT2Connection connection)
{
	if(connection->state != GTI2AwaitingAcceptance)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	connection->state = GTI2Connected;

	if(!gti2ConnectedCallback(connection, GT2Success, NULL, 0))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleReject(GT2Connection connection, GT2Byte * message, int len)
{
	if(connection->state != GTI2AwaitingAcceptance)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	gti2ConnectionClosed(connection);

	if(!gti2SendClosed(connection))
		return GT2False;

	if(!gti2ConnectedCallback(connection, GT2Rejected, message, len))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleClose(GT2Connection connection)
{
	GT2Bool localClose;

	if(!gti2SendClosed(connection))
		return GT2False;

	localClose = (connection->state == GTI2Closing);

	if(!gti2ConnectionError(connection, GT2Rejected, localClose?GT2LocalClose:GT2RemoteClose))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2DeliverReliableMessage(GT2Connection connection, GTI2MessageType type, GT2Byte * message, int len)
{
	connection->expectedSerialNumber++;

	if(type == GTI2MsgAppReliable)
	{
		if(!gti2HandleAppReliable(connection, message, len))
			return GT2False;
	}
	else if(type == GTI2MsgClientChallenge)
	{
		if(!gti2HandleClientChallenge(connection, message, len))
			return GT2False;
	}
	else if(type == GTI2MsgServerChallenge)
	{
		if(!gti2HandleServerChallenge(connection, message, len))
			return GT2False;
	}
	else if(type == GTI2MsgClientResponse)
	{
		if(!gti2HandleClientResponse(connection, message, len))
			return GT2False;
	}
	else if(type == GTI2MsgAccept)
	{
		if(!gti2HandleAccept(connection))
			return GT2False;
	}
	else if(type == GTI2MsgReject)
	{
		if(!gti2HandleReject(connection, message, len))
			return GT2False;
	}
	else if(type == GTI2MsgClose)
	{
		if(!gti2HandleClose(connection))
			return GT2False;
	}
	else if(type == GTI2MsgKeepAlive)
	{
	}

	return GT2True;
}
#ifdef WIN32
static int __cdecl gti2IncomingBufferMessageCompare(const void * elem1, const void * elem2)
#else
static int gti2IncomingBufferMessageCompare(const void * elem1, const void * elem2)
#endif
{
	const GTI2IncomingBufferMessage * message1 = (GTI2IncomingBufferMessage *)elem1;
	const GTI2IncomingBufferMessage * message2 = (GTI2IncomingBufferMessage *)elem2;

	return gti2SNDiff(message1->serialNumber, message2->serialNumber);
}

static GT2Bool gti2BufferIncomingMessage(GT2Connection connection, GTI2MessageType type, unsigned short SN, GT2Byte * message, int len, GT2Bool * overflow)
{
	GTI2IncomingBufferMessage messageInfo;
	GTI2IncomingBufferMessage * bufferedMessage;
	int num;
	int i;

	num = ArrayLength(connection->incomingBufferMessages);

	for(i = 0 ; i < num ; i++)
	{
		bufferedMessage = (GTI2IncomingBufferMessage *)ArrayNth(connection->incomingBufferMessages, i);

		if(bufferedMessage->serialNumber == SN)
		{
			*overflow = GT2False;
			return GT2True;
		}

		if(gti2SNDiff(bufferedMessage->serialNumber, SN) > 0)
			break;
	}

	if(gti2GetBufferFreeSpace(&connection->incomingBuffer) < len)
	{
		*overflow = GT2True;
		return GT2True;
	}

	messageInfo.start = connection->incomingBuffer.len;
	messageInfo.len = len;
	messageInfo.type = type;
	messageInfo.serialNumber = SN;

	ArrayInsertSorted(connection->incomingBufferMessages, &messageInfo, gti2IncomingBufferMessageCompare);

	if(ArrayLength(connection->incomingBufferMessages) != (num + 1))
	{
		*overflow = GT2True;
		return GT2True;
	}

	gti2BufferWriteData(&connection->incomingBuffer, message, len);

	if(num == 0)
	{
		if(!gti2SendNack(connection, connection->expectedSerialNumber, (unsigned short)(SN - 1)))
			return GT2False;
	}
	else
	{
		GTI2IncomingBufferMessage * msg;

		msg = (GTI2IncomingBufferMessage *)ArrayNth(connection->incomingBufferMessages, num);
		if(msg->serialNumber == SN)
		{
			GTI2IncomingBufferMessage * prev;
			unsigned short diff;

			prev = (GTI2IncomingBufferMessage *)ArrayNth(connection->incomingBufferMessages, num - 1);
			diff = (unsigned short)gti2SNDiff(SN, prev->serialNumber);
			if(diff > 1)
			{
				if(!gti2SendNack(connection, (unsigned short)(prev->serialNumber + 1), (unsigned short)(SN - 1)))
					return GT2False;
			}
		}
	}

	*overflow = GT2False;
	return GT2True;
}

static void gti2RemoveHoldMessage(GT2Connection connection, GTI2IncomingBufferMessage * message, int index)
{
	int moveAfter;
	int shortenBy;
	int moveEnd = 0;
	int num;
	int i;

	moveAfter = message->start;
	shortenBy = message->len;

	ArrayDeleteAt(connection->incomingBufferMessages, index);

	num = ArrayLength(connection->incomingBufferMessages);
	for(i = 0 ; i < num ; i++)
	{
		message = (GTI2IncomingBufferMessage *)ArrayNth(connection->incomingBufferMessages, i);
		if(message->start > moveAfter)
		{
			message->start -= shortenBy;
			moveEnd = max(moveEnd, message->start + message->len);
		}
	}

	gti2BufferShorten(&connection->incomingBuffer, moveAfter, shortenBy);
}

static GT2Bool gti2DeliverHoldMessages(GT2Connection connection)
{
	GTI2IncomingBufferMessage * message;
	int num;
	int i;

restart:

	num = ArrayLength(connection->incomingBufferMessages);
	for(i = (num - 1) ; i >= 0 ; i--)
	{
		message = (GTI2IncomingBufferMessage *)ArrayNth(connection->incomingBufferMessages, i);

		if(message->serialNumber == connection->expectedSerialNumber)
		{
			if(!gti2DeliverReliableMessage(connection, message->type, connection->incomingBuffer.buffer + message->start, message->len))
				return GT2False;

			gti2RemoveHoldMessage(connection, message, i);

			goto restart;
		}
	}

	return GT2True;
}

static void gti2SetPendingAck(GT2Connection connection)
{
	if(!connection->pendingAck)
	{
		connection->pendingAck = GT2True;
		connection->pendingAckTime = current_time();
	}
}

static GT2Bool gti2HandleReliableMessage(GT2Connection connection, GTI2MessageType type, GT2Byte * message, int len)
{
	unsigned short SN;
	unsigned short ESN;
	const int headerLength = connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN + 1 + 2 + 2;
	GT2Bool overflow;

	if(len < (connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN + 1 + 2 + 2))  
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	SN = gti2UShortFromBuffer(message + connection->socket->protocolOffset, GTI2_MAGIC_STRING_LEN + 1);

	ESN = gti2UShortFromBuffer(message + connection->socket->protocolOffset, GTI2_MAGIC_STRING_LEN + 3);

	if (connection->socket->protocolType == GTI2VdpProtocol && type == GTI2MsgAppReliable)
	{
		message[connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN + 3] = message[0];
		message[connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN + 4] = message[1];
		message += headerLength - connection->socket->protocolOffset;
		len -= headerLength - connection->socket->protocolOffset;
	}
	else
	{
		message += headerLength;
		len -= headerLength;
	}
	
	if(!gti2HandleESN(connection, ESN))
		return GT2False;

	if(SN == connection->expectedSerialNumber)
	{
		gti2SetPendingAck(connection);

		if(!gti2DeliverReliableMessage(connection, type, message, len))
			return GT2False;

		if(!gti2DeliverHoldMessages(connection))
			return GT2False;

		return GT2True;
	}

	if(gti2SNDiff(SN, connection->expectedSerialNumber) < 0)
	{
		gti2SetPendingAck(connection);

		return GT2True;
	}

	if(!gti2BufferIncomingMessage(connection, type, SN, message, len, &overflow))
		return GT2False;

	if(overflow)
	{
		if(!gti2ConnectionMemoryError(connection))
			return GT2False;
	}

	return GT2True;
}

static GT2Bool gti2HandleAck(GT2Connection connection, const GT2Byte * message, int len)
{
	unsigned short ESN;

	if(len != 2)
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	ESN = gti2UShortFromBuffer(message, 0);

	if(!gti2HandleESN(connection, ESN))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleNack(GT2Connection connection, const GT2Byte * message, int len)
{
	unsigned short SNMin;
	unsigned short SNMax;
	int num;
	int i;
	GTI2OutgoingBufferMessage * messageInfo;

	SNMin = gti2UShortFromBuffer(message, 0);
	if(len == 2)
	{
		SNMax = SNMin;
	}
	else if(len == 4)
	{
		SNMax = gti2UShortFromBuffer(message, 2);
	}
	else
	{
		if(!gti2ConnectionCommunicationError(connection))
			return GT2False;

		return GT2True;
	}

	num = ArrayLength(connection->outgoingBufferMessages);
	for(i = 0 ; i < num ; i++)
	{
		messageInfo = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, i);
		if((gti2SNDiff(messageInfo->serialNumber, SNMin) >= 0) && (gti2SNDiff(messageInfo->serialNumber, SNMax) <= 0))
		{
			if(!gti2ResendMessage(connection, messageInfo))
				return GT2False;
		}
	}

	return GT2True;
}

static GT2Bool gti2HandlePing(GT2Connection connection, GT2Byte * message, int len)
{
	return gti2SendPong(connection, message, len);
}

static GT2Bool gti2HandlePong(GT2Connection connection, const GT2Byte * message, int len)
{
	gsi_time startTime;

	if(!connection->callbacks.ping)
		return GT2True;

	if(len != (4 + sizeof(gsi_time)))
		return GT2True;
	if(memcmp(message, "time", 4) != 0)
		return GT2True;

	memcpy(&startTime, message + 4, sizeof(gsi_time));

	if(!gti2PingCallback(connection, (int)(current_time() - startTime)))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleClosed(GT2Connection connection)
{
	GT2Bool localClose;

	if(connection->state == GTI2Closed)
		return GT2True;

	localClose = (connection->state == GTI2Closing);

	if(!gti2ConnectionError(connection, GT2Rejected, localClose?GT2LocalClose:GT2RemoteClose))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2HandleUnreliableMessage(GT2Connection connection, GTI2MessageType type, GT2Byte * message, int len)
{
	int headerLength;
	GT2Byte * dataStart;
	int dataLen;

	headerLength = (connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN + 1);
	dataStart = (message + headerLength);
	dataLen = (len - headerLength);
	
	if(type == GTI2MsgAck)
	{
		if(!gti2HandleAck(connection, dataStart, dataLen))
			return GT2False;
	}
	else if(type == GTI2MsgNack)
	{
		if(!gti2HandleNack(connection, dataStart, dataLen))
			return GT2False;
	}
	else if(type == GTI2MsgPing)
	{
		if(!gti2HandlePing(connection, message, len))
			return GT2False;
	}
	else if(type == GTI2MsgPong)
	{
		if(!gti2HandlePong(connection, dataStart, dataLen))
			return GT2False;
	}
	else if(type == GTI2MsgClosed)
	{
		if(!gti2HandleClosed(connection))
			return GT2False;
	}

	return GT2True;
}

static GT2Bool gti2HandleMessage(GT2Socket socket, GT2Byte * message, int len, unsigned int ip, unsigned short port)
{
	GT2Connection connection;
	GT2Bool magicString;
	GT2Result result;
	GTI2MessageType type;
	GT2Bool handled;
	int actualLength = len - socket->protocolOffset;

	GT2Byte *actualMessage = message + socket->protocolOffset;
	
	connection = gti2SocketFindConnection(socket, ip, port);

	if(socket->receiveDumpCallback)
	{
		if(!gti2DumpCallback(socket, connection, ip, port, GT2False, message, len, GT2False))
			return GT2False;
	}
	
	magicString = ((actualLength > GTI2_MAGIC_STRING_LEN) && (memcmp(actualMessage, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN) == 0));
	
	if(!connection)
	{
		if(!gti2UnrecognizedMessageCallback(socket, ip, port, message, len, &handled))
			return GT2False;

		if(handled)
			return GT2True;

		if(!magicString || (actualMessage[GTI2_MAGIC_STRING_LEN] != GTI2MsgClientChallenge))
		{
			if(!magicString || (actualMessage[GTI2_MAGIC_STRING_LEN] != GTI2MsgClosed))
			{
				if(!gti2SendClosedOnSocket(socket, ip, port))
					return GT2False;
			}
			return GT2True;
		}	
		
		if(!socket->connectAttemptCallback)
			return GT2True;

		result = gti2NewIncomingConnection(socket, &connection, ip, port);
		if(result != GT2Success)
		{
			if(result != GT2DuplicateAddress)
			{
				if(!gti2SendClosedOnSocket(socket, ip, port))
					return GT2False;
			}
			return GT2True;
		}
	}

	if(connection->state == GTI2Closed)
	{
		if(!magicString || (actualMessage[GTI2_MAGIC_STRING_LEN] != GTI2MsgClosed))
		{
			if(!gti2SendClosed(connection))
				return GT2False;
		}

		return GT2True;
	}
	
	if(magicString && ((actualLength >= (GTI2_MAGIC_STRING_LEN * 2)) && (memcmp(actualMessage + GTI2_MAGIC_STRING_LEN, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN) == 0)))
	{
		message[3] = message[1];
		message[2] = message[0];
		message = actualMessage;
		actualMessage += socket->protocolOffset;
		actualLength -= socket->protocolOffset;
		len -= GTI2_MAGIC_STRING_LEN;
		magicString = GT2False;
	}
	
	if(!magicString)
	{
		if (connection->state < GTI2Connected)
		{
			if(!gti2UnrecognizedMessageCallback(socket, ip, port, message, len, &handled))
				return GT2False;
		}
		else
		{
			if(!gti2HandleAppUnreliable(connection, message, len))
				return GT2False;
		}

		return GT2True;
	}

	type = (GTI2MessageType)actualMessage[GTI2_MAGIC_STRING_LEN];


	if(type < GTI2NumReliableMessages)
	{
		if(!gti2HandleReliableMessage(connection, type, message, len))
			return GT2False;
		return GT2True;
	}

	if(!gti2HandleUnreliableMessage(connection, type, message, len))
		return GT2False;

	return GT2True;
}


GT2Bool gti2HandleConnectionReset(GT2Socket socket, unsigned int ip, unsigned short port)
{
	GT2Connection connection;

	connection = gti2SocketFindConnection(socket, ip, port);

	if(socket->receiveDumpCallback)
	{
		if(!gti2DumpCallback(socket, connection, ip, port, GT2True, NULL, 0, GT2False))
			return GT2False;
	}

	if(!connection)
		return GT2True;

	if(connection->state == GTI2AwaitingServerChallenge)
	{
		if(!connection->timeout || ((current_time() - connection->startTime) < connection->timeout))
			return GT2True;

		if(!gti2ConnectionError(connection, GT2TimedOut, GT2RemoteClose))
			return GT2False;
	}
	else
	{
		if(!gti2ConnectionError(connection, GT2Rejected, GT2RemoteClose))
			return GT2False;
	}

	return GT2True;
}

GT2Bool gti2HandleHostUnreachable(GT2Socket socket, unsigned int ip, unsigned short port, GT2Bool send)
{
	GT2Connection connection;

	connection = gti2SocketFindConnection(socket, ip, port);

	if(socket->receiveDumpCallback)
	{
		if(!gti2DumpCallback(socket, connection, ip, port, GT2True, NULL, 0, send))
			return GT2False;
	}

	if(!connection)
		return GT2True;


	if(!gti2ConnectionError(connection, GT2TimedOut, GT2RemoteClose))
		return GT2False;

	return GT2True;
}


#ifdef GSI_ADHOC



gsi_bool _NetworkAdHocSocketRecv(int socket_id,
							char	*buf,
							int		bufferlen,
							int		flags,
							char	*saddr,		
							gsi_u16 *sport);








int _NetworkAdHocCanReceiveOnSocket(int socket_id);

GT2Bool gti2ReceiveAdHocMessages(GT2Socket socket,char *buffer, int buffersize)
{
	int rcode;
	SOCKADDR_IN address;
	int addressLen;

	while	(1)
	{
		int datasize =  _NetworkAdHocCanReceiveOnSocket(socket->socket);
		if (datasize < 0)	
		{
			gti2SocketError(socket);
			return GT2False;
		}

		if (datasize == 0)
			break;		
		{
			char		mac[6];
			gsi_u16		port;

			addressLen = sizeof(address);

			rcode = _NetworkAdHocSocketRecv(socket->socket, buffer,buffersize , 0, mac,&port);
			if(rcode < 0)	
			{
				#if(0)	
					if(0)
					{
							if(!gti2HandleConnectionReset(socket, address.sin_addr.s_addr, ntohs(address.sin_port)))
								return GT2False;
					}
					else 
					if (rcode == WSAEHOSTUNREACH)
					{
						if (!gti2HandleHostUnreachable(socket, address.sin_addr.s_addr, ntohs(address.sin_port), GT2False))
							return GT2False;
					}			
					else
				#endif
					{
						gti2SocketError(socket);
						return GT2False;
					}
			}
			if(rcode == 0)	
			{
				return GT2False;
			}


			address.sin_addr.s_addr = gt2MacToIp(mac);
			address.sin_port = port;

			#ifdef RECV_LOG
				gti2LogMessage(address.sin_addr.s_addr, ntohs(address.sin_port),
					socket->ip, socket->port,
					buffer, rcode);
			#endif
			if(!gti2HandleMessage(socket, (GT2Byte *)buffer, rcode, address.sin_addr.s_addr, address.sin_port))
				return GT2False;
		}
	}

	return GT2True;
}
#endif

GT2Bool gti2ReceiveMessages(GT2Socket socket)
{
	int rcode;
	SOCKADDR_IN address;
	int addressLen;


	#if (GTI2_STACK_RECV_BUFFER_SIZE > 1600)
		static char buffer[GTI2_STACK_RECV_BUFFER_SIZE];
	#else
		char buffer[GTI2_STACK_RECV_BUFFER_SIZE];
	#endif


	#ifdef GSI_ADHOC
	if(socket->protocolType == GTI2AdHocProtocol)
	{
		return gti2ReceiveAdHocMessages(socket,buffer,GTI2_STACK_RECV_BUFFER_SIZE);
	}
	#endif

	while	(CanReceiveOnSocket(socket->socket))
	{

		addressLen = sizeof(address);
		
		rcode = recvfrom(socket->socket, buffer, sizeof(buffer), 0, (SOCKADDR *)&address, &addressLen);
		
		if (gsiSocketIsError(rcode))
		{
			rcode = GOAGetLastError(socket->socket);
			if(rcode == WSAECONNRESET)
			{
				if(!gti2HandleConnectionReset(socket, address.sin_addr.s_addr, ntohs(address.sin_port)))
					return GT2False;
			}
			#ifndef SN_SYSTEMS
				else if (rcode == WSAEHOSTUNREACH)
				{
					if (!gti2HandleHostUnreachable(socket, address.sin_addr.s_addr, ntohs(address.sin_port), GT2False))
						return GT2False;
				}
			#endif
			else if(rcode != WSAEMSGSIZE)
			{
				gti2SocketError(socket);
				return GT2False;
			}
		}
		else
		{
			#ifdef RECV_LOG
				gti2LogMessage(address.sin_addr.s_addr, ntohs(address.sin_port),
					socket->ip, socket->port,
					buffer, rcode);
			#endif
			if(!gti2HandleMessage(socket, (GT2Byte *)buffer, rcode, address.sin_addr.s_addr, ntohs(address.sin_port)))
				return GT2False;
		}
	}

	return GT2True;
}

static GT2Bool gti2StoreOutgoingReliableMessageInfo(GT2Connection connection, unsigned short SN, int len)
{
	GTI2OutgoingBufferMessage messageInfo;
	int num;

	memset(&messageInfo, 0, sizeof(messageInfo));
	messageInfo.start = connection->outgoingBuffer.len;
	messageInfo.len = len;
	messageInfo.serialNumber = SN;
	messageInfo.lastSend = current_time();

	num = ArrayLength(connection->outgoingBufferMessages);

	ArrayAppend(connection->outgoingBufferMessages, &messageInfo);

	if(ArrayLength(connection->outgoingBufferMessages) != (num + 1))
		return GT2False;

	return GT2True;
}

static GT2Bool gti2BeginReliableMessage(GT2Connection connection, GTI2MessageType type, int len, GT2Bool * overflow)
{
	int freeSpace;

	unsigned short vdpDataLength = (unsigned short)(len - connection->socket->protocolOffset);
	
	freeSpace = gti2GetBufferFreeSpace(&connection->outgoingBuffer);

	if(freeSpace < len)
	{
		if(!gti2ConnectionMemoryError(connection))
			return GT2False;

		*overflow = GT2True;
		return GT2True;
	}

	if(!gti2StoreOutgoingReliableMessageInfo(connection, connection->serialNumber, len))
	{
		if(!gti2ConnectionMemoryError(connection))
			return GT2False;

		*overflow = GT2True;
		return GT2True;
	}

	if (connection->socket->protocolType == GTI2VdpProtocol)
		gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)&vdpDataLength, connection->socket->protocolOffset);
	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN);
	gti2BufferWriteByte(&connection->outgoingBuffer, (GT2Byte)type);
	gti2BufferWriteUShort(&connection->outgoingBuffer, connection->serialNumber++);
	gti2BufferWriteUShort(&connection->outgoingBuffer, connection->expectedSerialNumber);

	*overflow = GT2False;
	return GT2True;
}

static GT2Bool gti2EndReliableMessage(GT2Connection connection)
{
	GTI2OutgoingBufferMessage * message;
	int len;

	len = ArrayLength(connection->outgoingBufferMessages);
	assert(len > 0);
	message = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, len - 1);

	if(!gti2ConnectionSendData(connection, connection->outgoingBuffer.buffer + message->start, message->len))
		return GT2False;

	connection->pendingAck = GT2False;

	return GT2True;
}

GT2Bool gti2SendAppReliable(GT2Connection connection, const GT2Byte * message, int len)
{
	int totalLen;
	GT2Bool overflow;

	totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2 + len);

	if(!gti2BeginReliableMessage(connection, GTI2MsgAppReliable, totalLen, &overflow))
		return GT2False;
	if(overflow)
		return GT2True;

	gti2BufferWriteData(&connection->outgoingBuffer, message, len);

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendClientChallenge(GT2Connection connection, const char challenge[GTI2_CHALLENGE_LEN])
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2 + GTI2_CHALLENGE_LEN);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgClientChallenge, totalLen + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)challenge, GTI2_CHALLENGE_LEN);

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendServerChallenge(GT2Connection connection, const char response[GTI2_RESPONSE_LEN], const char challenge[GTI2_CHALLENGE_LEN])
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2 + GTI2_RESPONSE_LEN + GTI2_CHALLENGE_LEN);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgServerChallenge, totalLen + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)response, GTI2_RESPONSE_LEN);

	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)challenge, GTI2_CHALLENGE_LEN);

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	connection->challengeTime = connection->lastSend;

	return GT2True;
}

GT2Bool gti2SendClientResponse(GT2Connection connection, const char response[GTI2_RESPONSE_LEN], const char * message, int len)
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2 + GTI2_RESPONSE_LEN + len);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgClientResponse, totalLen + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)response, GTI2_RESPONSE_LEN);

	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)message, len);

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendAccept(GT2Connection connection)
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgAccept, totalLen  + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendReject(GT2Connection connection, const GT2Byte * message, int len)
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2 + len);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgReject, totalLen + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	gti2BufferWriteData(&connection->outgoingBuffer, message, len);

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendClose(GT2Connection connection)
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgClose, totalLen + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendKeepAlive(GT2Connection connection)
{
	int totalLen = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2);
	GT2Bool overflow;

	if(!gti2BeginReliableMessage(connection, GTI2MsgKeepAlive, totalLen  + connection->socket->protocolOffset, &overflow))
		return GT2False;

	if(overflow)
		return GT2True;

	if(!gti2EndReliableMessage(connection))
		return GT2False;

	return GT2True;
}

GT2Bool gti2SendAppUnreliable(GT2Connection connection, const GT2Byte * message, int len)
{
	int freeSpace;
	int totalLen;
	GT2Byte * start;

	if((len < GTI2_MAGIC_STRING_LEN) || 
		(memcmp(message + connection->socket->protocolOffset, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN) != 0))
	{
		if(!gti2ConnectionSendData(connection, message, len))
			return GT2False;

		return GT2True;
	}
	
	totalLen = (GTI2_MAGIC_STRING_LEN + len);

	freeSpace = gti2GetBufferFreeSpace(&connection->outgoingBuffer);

	if(freeSpace < totalLen)
	{
		return GT2True;
	}

	start = (connection->outgoingBuffer.buffer + connection->outgoingBuffer.len);

	if (connection->socket->protocolType == GTI2VdpProtocol)
		gti2BufferWriteData(&connection->outgoingBuffer, message, 2);
	
	gti2BufferWriteData(&connection->outgoingBuffer, (const GT2Byte *)GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN);
	
	gti2BufferWriteData(&connection->outgoingBuffer, message + connection->socket->protocolOffset, 
		(int)(len - connection->socket->protocolOffset));
	
	if(!gti2ConnectionSendData(connection, start, totalLen))
		return GT2False;

	gti2BufferShorten(&connection->outgoingBuffer, -1, totalLen);
	
	return GT2True;
}

GT2Bool gti2SendAck(GT2Connection connection)
{
	char buffer[MAX_PROTOCOL_OFFSET + GTI2_MAGIC_STRING_LEN + 1 + 2];
	int pos = 0;
	
	if (connection->socket->protocolType == GTI2VdpProtocol)
	{
		short dataLength = (GTI2_MAGIC_STRING_LEN + 1 + 2);
		memcpy(buffer, &dataLength, 2);
		pos += 2;
	}

	memcpy(buffer + pos, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN);
	pos += GTI2_MAGIC_STRING_LEN;

	buffer[pos++] = GTI2MsgAck;

	gti2UShortToBuffer((GT2Byte *)buffer, pos, connection->expectedSerialNumber);
	pos += 2;
	
	if(!gti2ConnectionSendData(connection, (const GT2Byte *)buffer, pos))
		return GT2False;

	connection->pendingAck = GT2False;

	return GT2True;
}


GT2Bool gti2SendNack(GT2Connection connection, unsigned short SNMin, unsigned short SNMax)
{
	char buffer[MAX_PROTOCOL_OFFSET + GTI2_MAGIC_STRING_LEN + 1 + 2 + 2];
	int pos = 0;

	if (connection->socket->protocolType == GTI2VdpProtocol)
	{
		short dataLength = (GTI2_MAGIC_STRING_LEN + 1 + 2 + 2);
		memcpy(buffer, &dataLength, 2);
		pos += 2;
	}

	memcpy(buffer + pos, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN);
	pos += GTI2_MAGIC_STRING_LEN;

	buffer[pos++] = GTI2MsgNack;

	gti2UShortToBuffer((GT2Byte *)buffer, pos, SNMin);
	pos += 2;

	if(SNMin != SNMax)
	{
		gti2UShortToBuffer((GT2Byte *)buffer, pos, SNMax);
		pos += 2;
	}

	if(!gti2ConnectionSendData(connection, (const GT2Byte *)buffer, pos))
		return GT2False;

	return GT2True;
}


GT2Bool gti2SendPing(GT2Connection connection)
{
	char buffer[MAX_PROTOCOL_OFFSET + GTI2_MAGIC_STRING_LEN + 1 + 4 + sizeof(gsi_time)];
	int pos = 0;
	gsi_time now;

	if (connection->socket->protocolType == GTI2VdpProtocol)
	{
		short dataLength = (GTI2_MAGIC_STRING_LEN + 1 + 4 + sizeof(gsi_time));
		memcpy(buffer, &dataLength, 2);
		pos += 2;
	}
	memcpy(buffer + pos, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN);
	pos += GTI2_MAGIC_STRING_LEN;

	buffer[pos++] = GTI2MsgPing;

	memcpy(buffer + pos, "time", 4);
	pos += 4;

	now = current_time();
	memcpy(buffer + pos, &now, sizeof(gsi_time));

	pos += (int)sizeof(gsi_time);
	if(!gti2ConnectionSendData(connection, (const GT2Byte *)buffer, pos))
		return GT2False;

	return GT2True;
}


GT2Bool gti2SendPong(GT2Connection connection, GT2Byte * message, int len)
{
	message[GTI2_MAGIC_STRING_LEN] = GTI2MsgPong;

	return gti2ConnectionSendData(connection, message, len);
}

GT2Bool gti2SendClosed(GT2Connection connection)
{
	return gti2SendClosedOnSocket(connection->socket, connection->ip, connection->port);
}


GT2Bool gti2SendClosedOnSocket(GT2Socket socket, unsigned int ip, unsigned short port)
{
	char buffer[MAX_PROTOCOL_OFFSET + GTI2_MAGIC_STRING_LEN + 1]; 
	int pos = 0;

	if (socket->protocolType == GTI2VdpProtocol)
	{
		short dataLength = GTI2_MAGIC_STRING_LEN + 1;
		memcpy(buffer, &dataLength, 2);
		pos += 2;
	}

	memcpy(buffer + pos, GTI2_MAGIC_STRING, GTI2_MAGIC_STRING_LEN);
	pos += GTI2_MAGIC_STRING_LEN;

	buffer[pos++] = GTI2MsgClosed;

	if(!gti2SocketSend(socket, ip, port, (const GT2Byte *)buffer, pos))
		return GT2False;

	return GT2True;
}


GT2Bool gti2ResendMessage(GT2Connection connection, GTI2OutgoingBufferMessage * message)
{
	GTI2MessageType type;
	int pos;

	pos = (message->start + connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN + 1 + 2);
	
	gti2UShortToBuffer(connection->outgoingBuffer.buffer, pos, connection->expectedSerialNumber);

	if(!gti2ConnectionSendData(connection, connection->outgoingBuffer.buffer + message->start, message->len))
		return GT2False;

	message->lastSend = connection->lastSend;

	type = (GTI2MessageType)connection->outgoingBuffer.buffer[message->start + connection->socket->protocolOffset + GTI2_MAGIC_STRING_LEN];
	
	if(type == GTI2MsgServerChallenge)
		connection->challengeTime = connection->lastSend;

	return GT2True;
}

GT2Bool gti2Send(GT2Connection connection, const GT2Byte * message, int len, GT2Bool reliable)
{
	if (reliable)
		return gti2SendAppReliable(connection, message, len);
	return gti2SendAppUnreliable(connection, message, len);
}

GT2Bool gti2WasMessageIDConfirmed(const GT2Connection connection, GT2MessageID messageID)
{
	GTI2OutgoingBufferMessage * message;
	int len;

	len = ArrayLength(connection->outgoingBufferMessages);
	if(!len)
		return GT2True;

	message = (GTI2OutgoingBufferMessage *)ArrayNth(connection->outgoingBufferMessages, 0);

	if(gti2SNDiff(messageID, message->serialNumber) < 0)
		return GT2True;

	return GT2False;
}
