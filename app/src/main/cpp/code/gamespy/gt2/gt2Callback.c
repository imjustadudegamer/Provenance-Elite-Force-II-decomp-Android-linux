
#include "gt2Callback.h"
#include "gt2Socket.h"


GT2Bool gti2SocketErrorCallback
(
	GT2Socket socket
)
{
	assert(socket);
	if(!socket)
		return GT2True;

	if(!socket->socketErrorCallback)
		return GT2True;

	socket->callbackLevel++;

	socket->socketErrorCallback(socket);

	socket->callbackLevel--;

	if(socket->close && !socket->callbackLevel)
	{
		gti2CloseSocket(socket);
		return GT2False;
	}

	return GT2True;
}

GT2Bool gti2ConnectAttemptCallback
(
	GT2Socket socket,
	GT2Connection connection,
	unsigned int ip,
	unsigned short port,
	int latency,
	GT2Byte * message,
	int len
)
{
	assert(socket && connection);
	if(!socket || !connection)
		return GT2True;

	if(!socket->connectAttemptCallback)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	socket->callbackLevel++;
	connection->callbackLevel++;

	socket->connectAttemptCallback(socket, connection, ip, port, latency, message, len);

	socket->callbackLevel--;
	connection->callbackLevel--;

	if(socket->close && !socket->callbackLevel)
	{
		gti2CloseSocket(socket);
		return GT2False;
	}

	return GT2True;
}


GT2Bool gti2ConnectedCallback
(
	GT2Connection connection,
	GT2Result result,
	GT2Byte * message,
	int len
)
{
	assert(connection);
	if(!connection)
		return GT2True;
	
	connection->connectionResult = result;

	if(!connection->callbacks.connected)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	connection->callbackLevel++;
	connection->socket->callbackLevel++;

	connection->callbacks.connected(connection, result, message, len);

	connection->callbackLevel--;
	connection->socket->callbackLevel--;

	if(connection->socket->close && !connection->socket->callbackLevel)
	{
		gti2CloseSocket(connection->socket);
		return GT2False;
	}

	return GT2True;
}

GT2Bool gti2ReceivedCallback
(
	GT2Connection connection,
	GT2Byte * message,
	int len,
	GT2Bool reliable
)
{
	assert(connection);
	if(!connection)
		return GT2True;

	if(!connection->callbacks.received)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	connection->callbackLevel++;
	connection->socket->callbackLevel++;

	connection->callbacks.received(connection, message, len, reliable);

	connection->callbackLevel--;
	connection->socket->callbackLevel--;

	if(connection->socket->close && !connection->socket->callbackLevel)
	{
		gti2CloseSocket(connection->socket);
		return GT2False;
	}

	return GT2True;
}

GT2Bool gti2ClosedCallback
(
	GT2Connection connection,
	GT2CloseReason reason
)
{
	assert(connection);
	if(!connection)
		return GT2True;

	if(!connection->callbacks.closed)
		return GT2True;

	connection->callbackLevel++;
	connection->socket->callbackLevel++;

	connection->callbacks.closed(connection, reason);

	connection->callbackLevel--;
	connection->socket->callbackLevel--;

	if(connection->socket->close && !connection->socket->callbackLevel)
	{
		gti2CloseSocket(connection->socket);
		return GT2False;
	}

	return GT2True;
}

GT2Bool gti2PingCallback
(
	GT2Connection connection,
	int latency
)
{
	assert(connection);
	if(!connection)
		return GT2True;

	if(!connection->callbacks.ping)
		return GT2True;

	connection->callbackLevel++;
	connection->socket->callbackLevel++;

	connection->callbacks.ping(connection, latency);

	connection->callbackLevel--;
	connection->socket->callbackLevel--;

	if(connection->socket->close && !connection->socket->callbackLevel)
	{
		gti2CloseSocket(connection->socket);
		return GT2False;
	}

	return GT2True;
}


GT2Bool gti2SendFilterCallback
(
	GT2Connection connection,
	int filterID,
	const GT2Byte * message,
	int len,
	GT2Bool reliable
)
{
	gt2SendFilterCallback * callback;

	assert(connection);
	if(!connection)
		return GT2True;

	callback = (gt2SendFilterCallback *)ArrayNth(connection->sendFilters, filterID);
	if(!callback)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	connection->callbackLevel++;
	connection->socket->callbackLevel++;

	(*callback)(connection, filterID, message, len, reliable);

	connection->callbackLevel--;
	connection->socket->callbackLevel--;

	if(connection->socket->close && !connection->socket->callbackLevel)
	{
		gti2CloseSocket(connection->socket);
		return GT2False;
	}

	return GT2True;
}

GT2Bool gti2ReceiveFilterCallback
(
	GT2Connection connection,
	int filterID,
	GT2Byte * message,
	int len,
	GT2Bool reliable
)
{
	gt2ReceiveFilterCallback * callback;

	assert(connection);
	if(!connection)
		return GT2True;

	callback = (gt2ReceiveFilterCallback *)ArrayNth(connection->receiveFilters, filterID);
	if(!callback)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	connection->callbackLevel++;
	connection->socket->callbackLevel++;

	(*callback)(connection, filterID, message, len, reliable);

	connection->callbackLevel--;
	connection->socket->callbackLevel--;

	if(connection->socket->close && !connection->socket->callbackLevel)
	{
		gti2CloseSocket(connection->socket);
		return GT2False;
	}

	return GT2True;
}


GT2Bool gti2DumpCallback
(
	GT2Socket socket,
	GT2Connection connection,
	unsigned int ip,
	unsigned short port,
	GT2Bool reset,
	const GT2Byte * message,
	int len,
	GT2Bool send
)
{
	gt2DumpCallback callback;

	assert(socket);
	if(!socket)
		return GT2True;

	if(send)
		callback = socket->sendDumpCallback;
	else
		callback = socket->receiveDumpCallback;

	if(!callback)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	socket->callbackLevel++;
	if(connection)
		connection->callbackLevel++;

	callback(socket, connection, ip, port, reset, message, len);

	socket->callbackLevel--;
	if(connection)
		connection->callbackLevel--;

	if(socket->close && !socket->callbackLevel)
	{
		gti2CloseSocket(socket);
		return GT2False;
	}

	return GT2True;
}


GT2Bool gti2UnrecognizedMessageCallback
(
	GT2Socket socket,
	unsigned int ip,
	unsigned short port,
	GT2Byte * message,
	int len,
	GT2Bool * handled
)
{
	*handled = GT2False;

	assert(socket);
	if(!socket)
		return GT2True;

	if(!socket->unrecognizedMessageCallback)
		return GT2True;

	if(!len || !message)
	{
		message = NULL;
		len = 0;
	}

	socket->callbackLevel++;

	*handled = socket->unrecognizedMessageCallback(socket, ip, port, message, len);

	socket->callbackLevel--;

	if(socket->close && !socket->callbackLevel)
	{
		gti2CloseSocket(socket);
		return GT2False;
	}

	return GT2True;
}
