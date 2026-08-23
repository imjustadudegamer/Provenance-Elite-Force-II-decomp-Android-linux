

#include <stdio.h>
#include "../gt2.h"
#include "../../qr2/qr2.h"

#define PORT              12345
#define MAX_CLIENTS       8
#define TIMEOUT           (30 * 1000)
#define SEND_TIME        500

#define REPORT

GT2Bool hosting;
GT2Bool quit;
GT2ConnectionCallbacks connectionCallbacks;
char messageKey[128];
char messageValue[128];
unsigned int backupHost;
int backupHostIndex;
GT2Bool gotBackupHost;

#ifdef REPORT
#define QR2_GAME_VERSION	_T("2.00")
#define QR2_GAME_NAME		_T("gmtest")
#define QR2_MAX_PLAYERS		MAX_CLIENTS
#define QR2_BASE_PORT		11111
#define QR2_HOSTNAME        _T("My Host")
#endif

GT2Connection clients[MAX_CLIENTS];
int numClients;

GT2Bool ParseMessage
(
	const char * message,
	int len
)
{
	const char * keyStart;
	const char * valueStart;
	int copyLen;
	int i;

	for(i = 0 ; i < (len - 1) ; i++)
	{
		if(!message[i])
			return GT2False;
	}
	if(message[i])
		return GT2False;
	if(message[0] != '\\')
		return GT2False;

	keyStart = &message[1];
	valueStart = strchr(keyStart, '\\');
	if(!valueStart)
		return GT2False;
	valueStart++;

	copyLen = (valueStart - keyStart - 1);
	if(copyLen >= sizeof(messageKey))
		return GT2False;
	memcpy(messageKey, keyStart, copyLen);
	messageKey[copyLen] = '\0';

	if(strlen(valueStart) >= sizeof(messageValue))
		return GT2False;
	strcpy(messageValue, valueStart);

	return GT2True;
}

const char * GetConnectionName
(
	GT2Connection connection
)
{
	return gt2AddressToString(gt2GetRemoteIP(connection), 0, NULL);
}

void TellBackupHost
(
	int index
)
{
	char buffer[32];
	GT2Connection connection = clients[index];

	sprintf(buffer, "\\backuphost\\%s", gt2AddressToString(backupHost, 0, NULL));
	gt2Send(connection, (const GT2Byte *)buffer, -1, GT2True);
}

void NewBackupHost
(
	int index
)
{
	int i;
	GT2Connection connection = clients[index];

	gt2Send(connection, (const GT2Byte *)("\\backuphost\\"), -1, GT2True);
	backupHost = gt2GetRemoteIP(connection);
	backupHostIndex = index;

	for(i = 0 ; i < MAX_CLIENTS ; i++)
	{
		if(clients[i] && (i != index))
			TellBackupHost(i);
	}
}

#ifdef REPORT

void server_key_callback(int keyid, qr2_buffer_t outbuf, void *userdata)
{
	switch (keyid)
	{
	case HOSTNAME_KEY:
		qr2_buffer_add(outbuf, QR2_HOSTNAME);
		break;
	case GAMEVER_KEY:
		qr2_buffer_add(outbuf, QR2_GAME_VERSION);
		break;
	case HOSTPORT_KEY:
		qr2_buffer_add_int(outbuf, PORT);
		break;
	case NUMPLAYERS_KEY:
		qr2_buffer_add_int(outbuf, numClients);
		break;
	case MAXPLAYERS_KEY:
		qr2_buffer_add_int(outbuf, QR2_MAX_PLAYERS);
		break;

	default:
		qr2_buffer_add(outbuf, _T(""));
	}
	
	GSI_UNUSED(userdata);
}

void playerkey_callback(int keyid, int index, qr2_buffer_t outbuf, void *userdata)
{
	if (index >= numClients)
	{
		qr2_buffer_add(outbuf, _T(""));
		return;
	}
	switch (keyid)
	{
		case PLAYER__KEY:
			qr2_buffer_add(outbuf, GetConnectionName(clients[index]));
			break;
		default:
			qr2_buffer_add(outbuf, _T(""));
			break;		
	}
	
	GSI_UNUSED(userdata);
}

void teamkey_callback(int keyid, int index, qr2_buffer_t outbuf, void *userdata)
{
	qr2_buffer_add(outbuf, _T(""));

	GSI_UNUSED(userdata);
	GSI_UNUSED(index);
	GSI_UNUSED(keyid);
}	

void keylist_callback(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void *userdata)
{
	switch (keytype)
	{
	case key_server:
		qr2_keybuffer_add(keybuffer, HOSTNAME_KEY);
		qr2_keybuffer_add(keybuffer, GAMEVER_KEY);
		qr2_keybuffer_add(keybuffer, HOSTPORT_KEY);
		qr2_keybuffer_add(keybuffer, NUMPLAYERS_KEY);
		qr2_keybuffer_add(keybuffer, MAXPLAYERS_KEY);
		break;
	case key_player:
		qr2_keybuffer_add(keybuffer, PLAYER__KEY);
		break;
	case key_team:
		break;
	}
	
	GSI_UNUSED(userdata);
}

int count_callback(qr2_key_type keytype, void *userdata)
{
	if (keytype == key_player)
		return numClients;
	else if (keytype == key_team)
		return 0;
	else
		return 0;
		
	GSI_UNUSED(userdata);
}

void adderror_callback(qr2_error_t error, gsi_char *errmsg, void *userdata)
{
	_tprintf(_T("Error adding server: %d, %s\n"), error, errmsg);
	
	GSI_UNUSED(userdata);
}

#endif

void ConnectAttemptCallback
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
	int i;

	for(i = 0 ; i < MAX_CLIENTS ; i++)
	{
		if(!clients[i])
			break;
	}
	if(i == MAX_CLIENTS)
	{
		gt2Reject(connection, (const GT2Byte *)("Server full"), -1);
		printf("Rejected client, server full\n");
		return;
	}

	if(!gt2Accept(connection, &connectionCallbacks))
		return;

	clients[i] = connection;
	numClients++;

	gt2SetConnectionData(connection, (void *)i);

	printf("Connection accepted from %s\n", GetConnectionName(connection));

	if(numClients == 1)
	{
		NewBackupHost(i);
	}
	else
	{
		TellBackupHost(i);
	}

	GSI_UNUSED(len);
	GSI_UNUSED(message);
	GSI_UNUSED(latency);
	GSI_UNUSED(port);
	GSI_UNUSED(ip);
	GSI_UNUSED(socket);
}

void SocketErrorCallback
(
	GT2Socket socket
)
{
	printf("socket error!\n");
	quit = GT2True;

	GSI_UNUSED(socket);
}

void ConnectedCallback
(
	GT2Connection connection,
	GT2Result result,
	GT2Byte * message,
	int len
)
{
	if(result != GT2Success)
	{
		printf("Connection failed: ");
		if(result == GT2OutOfMemory)
			printf("OutOfMemory\n");
		else if(result == GT2Rejected)
			printf("Rejected: %s\n", message);
		else if(result == GT2NetworkError)
			printf("NetworkError\n");
		else if(result == GT2AddressError)
			printf("AddressError\n");
		else if(result == GT2DuplicateAddress)
			printf("DuplicateAddress\n");
		else if(result == GT2TimedOut)
			printf("TimedOut\n");
		else if(result == GT2NegotiationError)
			printf("NegotiationError\n");
		else
			printf("Unknown error\n");

		quit = GT2True;
		return;
	}

	printf("Connection accepted\n");

	GSI_UNUSED(len);
	GSI_UNUSED(connection);
}

void ReceivedCallback
(
	GT2Connection connection,
	GT2Byte * message,
	int len,
	GT2Bool reliable
)
{
	if(!ParseMessage((const char *)message, len))
	{
		printf("Error parsing incoming message from %s\n", GetConnectionName(connection));
		return;
	}

	if(strcasecmp(messageKey, "quit") == 0)
	{
		printf("Got remote quit: %s\n", messageValue);
		quit = GT2True;
		return;
	}

	if(hosting)
	{
		if(strcasecmp(messageKey, "echo") == 0)
		{
			gt2Send(connection, message, len, reliable);
			return;
		}
	}
	else
	{
		if(strcasecmp(messageKey, "echo") == 0)
			return;

		if(strcasecmp(messageKey, "backuphost") == 0)
		{
			printf("New backup host: %s\n", messageValue);

			gt2StringToAddress(messageValue, &backupHost, NULL);
			gotBackupHost = GT2True;
			return;
		}
	}

	printf("Unknown message key: %s = %s\n", messageKey, messageValue);
}

void ClosedCallback
(
	GT2Connection connection,
	GT2CloseReason reason
)
{
	if(hosting)
	{
		int i;
		
		i = (int)gt2GetConnectionData(connection);

		clients[i] = NULL;
		numClients--;

		if(i == backupHostIndex)
		{
			if(numClients)
			{
				for(i = 0 ; i < MAX_CLIENTS ; i++)
					if(clients[i])
					{
						NewBackupHost(i);
						break;
					}
			}
		}

		printf("Client connection with %s closed: %d\n", GetConnectionName(connection), reason);
	}
	else
	{
		printf("Connection closed\n");

		quit = GT2True;
	}
}

GT2Bool StartHosting
(
	void
)
{
	qr2_error_t rcode;
	GT2Socket socket;
	GT2Result result;

	quit = GT2False;
	hosting = GT2True;
	numClients = 0;
	memset(clients, 0, sizeof(GT2Connection) * MAX_CLIENTS);


	result = gt2CreateSocket(&socket, gt2AddressToString(0, PORT, NULL), 0, 0, SocketErrorCallback);
	if(result != GT2Success)
	{
		printf("Failed to create socket (%d)\n", result);
		return GT2False;
	}

	printf("Starting to listen...\n");
	gt2Listen(socket, ConnectAttemptCallback);

#ifdef REPORT
	printf("Starting reporting...\n");
	
	rcode = qr2_init(NULL, NULL, QR2_BASE_PORT, "gmtest", "HA6zkS", 1, 1, server_key_callback, playerkey_callback,
					 teamkey_callback, keylist_callback, count_callback, adderror_callback, NULL);
	
	if(rcode != e_qrnoerror)
		return GT2False;
#endif
	
	while(!quit)
	{
		gt2Think(socket);
		
#ifdef REPORT
		qr2_think(NULL);
#endif
		msleep(10);
	}

#ifdef REPORT
	qr2_shutdown(NULL);
#endif
	
	return GT2True;
}

GT2Bool StartConnecting
(
	const char * server
)
{
	GT2Connection connection;
	GT2Socket socket;
	GT2Result result;
	char address[256];
	unsigned long lastSendTime = 0;
	unsigned long now;

	quit = GT2False;
	hosting = GT2False;
	gotBackupHost = GT2False;

	sprintf(address, "%s:%d", server, PORT);

	result = gt2CreateSocket(&socket, NULL, 0, 0, SocketErrorCallback);
	if(result != GT2Success)
	{
		printf("Error creating the socket! (%d)\n", result);
		return GT2False;
	}

	printf("Connecting to %s (timeout after %d seconds)...\n", address, TIMEOUT / 1000);
	result = gt2Connect(socket, &connection, address, NULL, 0, TIMEOUT, &connectionCallbacks, GT2True);
	if(result != GT2Success)
	{
		printf("Error creating the connection! (%d)\n", result);
		return GT2False;
	}

	while(!quit)
	{
		now = current_time();
		if((now - lastSendTime) > SEND_TIME)
		{
			gt2Send(connection, (const GT2Byte *)("\\echo\\Hello."), -1, GT2False);

			lastSendTime = now;
		}

		gt2Think(socket);

		msleep(10);
	}

	return GT2True;
}

int main
(
	int argc,
	char ** argv
)
{
	memset(&connectionCallbacks, 0, sizeof(GT2ConnectionCallbacks));
	connectionCallbacks.connected = ConnectedCallback;
	connectionCallbacks.received = ReceivedCallback;
	connectionCallbacks.closed = ClosedCallback;

	if(argc < 2)
	{
		if(!StartHosting())
			printf("Failed to start hosting\n");
	}
	else
	{
		if(!StartConnecting(argv[1]))
		{
			printf("Failed to connect\n");
		}
		else
		{
			GT2Bool done = GT2False;
			do
			{
				if(gotBackupHost)
				{
					if(!backupHost)
					{
						printf("Starting up backup host...\n");

						if(!StartHosting())
						{
							printf("Failed to start backup host.\n");
							done = GT2True;
						}
					}
					else
					{
						const char * address = gt2AddressToString(backupHost, 0, NULL);

						printf("Connecting to backup host %s...\n", address);

						if(!StartConnecting(address))
						{
							printf("Failed to connect to backup host.\n");
							done = GT2True;
						}
					}
				}
				else
				{
					printf("No backup host, quitting\n");
					done = GT2True;
				}
			}
			while(!done);
		}
	}

	getchar();

	return 0;
}