/*
gpiConnect.c
GameSpy Presence SDK 
Dan "Mr. Pants" Schoenblum

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com

***********************************************************************
Please see the GameSpy Presence SDK documentation for more information
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "gpi.h"
#include <string.h>


#define GPI_CONNECTION_MANAGER_NAME    "gpcm." GSI_DOMAIN_NAME
#define GPI_CONNECTION_MANAGER_PORT    29900

#define GPI_UDP_HEADER "gamespygp"
#define RANDSTRING      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define RANDOMCHAR()    (RANDSTRING[rand() % (sizeof(RANDSTRING) - 1)])

char GPConnectionManagerHostname[64] = GPI_CONNECTION_MANAGER_NAME;

static void randomString(
  char * buffer,
  int numChars
)
{
	int i;

	for(i = 0 ; i < numChars ; i++)
		buffer[i] = RANDOMCHAR();
	buffer[i] = '\0';
}

static GPResult
gpiStartConnect(
  GPConnection * connection,
  GPIOperation * operation
)
{
	struct sockaddr_in address;
	int rcode;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	struct hostent * host;
	
	GSUdpErrorCode anError;
	strncpy(iconnection->mHeader, GPI_UDP_HEADER, GS_UDP_MSG_HEADER_LEN);

	if (!gsUdpEngineIsInitialized())
	{
		unsigned short peerPort = GPI_PEER_PORT;
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_Notice, 
			"Initializing UDP Layer\n");
		anError = gsUdpEngineInitialize(peerPort, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		if (anError != GS_UDP_NO_ERROR)
		{
			while (anError != GS_UDP_NO_ERROR && peerPort < GPI_PEER_PORT + 100)
			{
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_Comment, 
					"Port %d failed, trying next port\n", peerPort);
				anError = gsUdpEngineInitialize(++peerPort, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			}
			if (anError != GS_UDP_NO_ERROR)
			{
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_HotError, 
					"Tryed all 100 ports after default port, giving up.\n");
				CallbackFatalError(connection, GP_NETWORK_ERROR, GP_UDP_LAYER, 
					"There was error starting the UDP layer.");
			}
		}	
		if (!iconnection->firewall)
		{
			iconnection->peerPort = peerPort;
		}
	}
	else
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Network, GSIDebugLevel_Notice, 
			"UDP Layer already initialized, using existing port.\n");
		iconnection->peerPort = gsUdpEngineGetLocalPort();
	}
	anError = gsUdpEngineAddMsgHandler(iconnection->mHeader, iconnection->mHeader, NULL, gpiPeerAcceptedCallback, gpiPeerLeftCallback, 
		gpiPeerPingReplyCallback, gpiPeerMessageCallback, connection);
	if (anError != GS_UDP_NO_ERROR)
	{
		CallbackFatalError(connection, GP_NETWORK_ERROR, GP_UDP_LAYER, "There was an error starting the UDP Layer.");
	}
	if(iconnection->firewall)
	{
		
		
			
		iconnection->peerPort = 0;
	}
	

	
	iconnection->cmSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(iconnection->cmSocket == INVALID_SOCKET)
		CallbackFatalError(connection, GP_NETWORK_ERROR, GP_NETWORK, "There was an error creating a socket.");

	rcode = SetSockBlocking(iconnection->cmSocket,0);
	if(rcode == 0)
		CallbackFatalError(connection, GP_NETWORK_ERROR, GP_NETWORK, "There was an error making a socket non-blocking.");
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	if (inet_addr(GPConnectionManagerHostname) == INADDR_NONE)
	{
		host = gethostbyname(GPConnectionManagerHostname);
		if(host == NULL)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_NETWORK, "Could not resolve connection mananger host name.");
		address.sin_addr.s_addr = *(unsigned int *)host->h_addr_list[0];
	}
	else
	{
		address.sin_addr.s_addr = inet_addr(GPConnectionManagerHostname);
	}

	assert(address.sin_addr.s_addr != 0);
	address.sin_port = htons(GPI_CONNECTION_MANAGER_PORT);
	rcode = connect(iconnection->cmSocket, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
	if (gsiSocketIsError(rcode))
	{
		int error = GOAGetLastError(iconnection->cmSocket);
		if((error != WSAEWOULDBLOCK) && (error != WSAEINPROGRESS) && (error != WSAETIMEDOUT))
		{
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_NETWORK, "There was an error connecting a socket.");
		}
	}

	operation->state = GPI_CONNECTING;
	iconnection->connectState = GPI_CONNECTING;

	return GP_NO_ERROR;
}

GPResult
gpiConnect(
  GPConnection * connection,
  const char nick[GP_NICK_LEN],
  const char uniquenick[GP_UNIQUENICK_LEN],
  const char email[GP_EMAIL_LEN],
  const char password[GP_PASSWORD_LEN],
  const char authtoken[GP_AUTHTOKEN_LEN],
  const char partnerchallenge[GP_PARTNERCHALLENGE_LEN],
  const char cdkey[GP_CDKEY_LEN],
  GPEnum firewall,
  GPIBool newuser,
  GPEnum blocking,
  GPCallback callback,
  void * param
)
{
	GPIConnectData * data;
	GPIOperation * operation;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPResult result;

	if(iconnection->connectState == GPI_DISCONNECTED)
		CHECK_RESULT(gpiReset(connection));

	if(iconnection->connectState != GPI_NOT_CONNECTED)
		Error(connection, GP_PARAMETER_ERROR, "Invalid connection.");

#if defined(GS_WIRELESS_DEVICE)
	GSI_UNUSED(firewall);
	iconnection->firewall = GPITrue;
#else
	switch(firewall)
	{
	case GP_FIREWALL:
		iconnection->firewall = GPITrue;
		break;
	case GP_NO_FIREWALL:
		iconnection->firewall = GPIFalse;
		break;
	default:
		Error(connection, GP_PARAMETER_ERROR, "Invalid firewall.");
	}
#endif

	strzcpy(iconnection->nick, nick, GP_NICK_LEN);
	strzcpy(iconnection->uniquenick, uniquenick, GP_UNIQUENICK_LEN);
	strzcpy(iconnection->email, email, GP_EMAIL_LEN);
	strzcpy(iconnection->password, password, GP_PASSWORD_LEN);

#ifdef GSI_UNICODE
	UTF8ToUCS2StringLen(iconnection->nick, iconnection->nick_W, GP_NICK_LEN);
	UTF8ToUCS2StringLen(iconnection->uniquenick, iconnection->uniquenick_W, GP_UNIQUENICK_LEN);
	UTF8ToUCS2StringLen(iconnection->email, iconnection->email_W, GP_EMAIL_LEN);
	UTF8ToUCS2StringLen(iconnection->password, iconnection->password_W, GP_PASSWORD_LEN);
#endif

	_strlwr(iconnection->email);

#ifdef GSI_UNICODE
	AsciiToUCS2String(iconnection->email, iconnection->email_W);
#endif

	data = (GPIConnectData *)gsimalloc(sizeof(GPIConnectData));
	if(data == NULL)
		Error(connection, GP_MEMORY_ERROR, "Out of memory.");
	memset(data, 0, sizeof(GPIConnectData));

	data->newuser = newuser;

	if(authtoken[0] && partnerchallenge[0])
	{
		strzcpy(data->authtoken, authtoken, GP_AUTHTOKEN_LEN);
		strzcpy(data->partnerchallenge, partnerchallenge, GP_PARTNERCHALLENGE_LEN);
	}

	if(cdkey)
		strzcpy(data->cdkey, cdkey, GP_CDKEY_LEN);

	CHECK_RESULT(gpiAddOperation(connection, GPI_CONNECT, data, &operation, blocking, callback, param));

	result = gpiStartConnect(connection, operation);
	if(result != GP_NO_ERROR)
	{
		operation->result = result;
		gpiFailedOpCallback(connection, operation);
		gpiDisconnect(connection, GPIFalse);
		return result;
	}

	if(operation->blocking)
		CHECK_RESULT(gpiProcess(connection, operation->id));

	return GP_NO_ERROR;
}

static GPResult
gpiSendLogin(
  GPConnection * connection,
  GPIConnectData * data
)
{
	char buffer[512];
	char response[33];
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIProfile * profile;
	char * passphrase;
	char userBuffer[GP_NICK_LEN + GP_EMAIL_LEN];
	char partnerBuffer[11];
	char * user;

	randomString(data->userChallenge, sizeof(data->userChallenge) - 1);

	if(data->partnerchallenge[0])
		passphrase = data->partnerchallenge;
	else
		passphrase = iconnection->password;
	MD5Digest((unsigned char*)passphrase, strlen(passphrase), data->passwordHash);

	if(iconnection->partnerID != GP_PARTNERID_GAMESPY)
	{
		sprintf(partnerBuffer, "%d@", iconnection->partnerID);
	}
	else
	{
		strcpy(partnerBuffer, "");
	}

	if(data->authtoken[0])
		user = data->authtoken;
	else if(iconnection->uniquenick[0])
	{
		sprintf(userBuffer, "%s%s", partnerBuffer, iconnection->uniquenick);
		user = userBuffer;
	}
	else
	{
		sprintf(userBuffer, "%s%s@%s", partnerBuffer, iconnection->nick, iconnection->email);
		user = userBuffer;
	}

	sprintf(buffer, "%s%s%s%s%s%s",
		data->passwordHash,
		"                                                ",
		user,
		data->userChallenge,
		data->serverChallenge,
		data->passwordHash);
	MD5Digest((unsigned char *)buffer, strlen(buffer), response);

	if(iconnection->infoCaching)
	{
		gpiFindProfileByUser(connection, iconnection->nick, iconnection->email, &profile);
		if(profile != NULL)
		{
			iconnection->userid = profile->userId;
			iconnection->profileid = profile->profileId;
		}
	}

	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\login\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\challenge\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, data->userChallenge);
	if(data->authtoken[0])
	{
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\authtoken\\");
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, data->authtoken);
	}
	else if(iconnection->uniquenick[0])
	{
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\uniquenick\\");
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, iconnection->uniquenick);
	}
	else
	{
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\user\\");
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, iconnection->nick);
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "@");
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, iconnection->email);
	}
	if(iconnection->userid != 0)
	{
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\userid\\");
		gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->userid);
	}
	if(iconnection->profileid != 0)
	{
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\profileid\\");
		gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->profileid);
	}
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\partnerid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->partnerID);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\response\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, response);
	if(iconnection->firewall == GP_FIREWALL)
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\firewall\\1");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\port\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->peerPort);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\productid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->productID);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\gamename\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, __GSIACGamename);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\namespaceid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->namespaceID);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\sdkrevision\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, GPI_SDKREV);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\quiet\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->quietModeFlags);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\id\\1");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\final\\");

	return GP_NO_ERROR;
}

static GPResult
gpiSendNewuser(
  GPConnection * connection,
  GPIConnectData * data
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	size_t i;
	const int useAlternateEncoding = 1;

	char passwordenc[GP_PASSWORDENC_LEN];
	gpiEncodeString(iconnection->password, passwordenc);

	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\newuser\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\email\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, iconnection->email);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\nick\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, iconnection->nick);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\passwordenc\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, passwordenc);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\productid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->productID);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\gamename\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, __GSIACGamename);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\namespaceid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->namespaceID);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\uniquenick\\");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, iconnection->uniquenick);
	if(data->cdkey[0])
	{
		char cdkeyxor[GP_CDKEY_LEN];
		char cdkeyenc[GP_CDKEYENC_LEN];
		size_t cdkeylen = strlen(data->cdkey);
		
		Util_RandSeed((unsigned long)GP_XOR_SEED);
		for (i=0; i < cdkeylen; i++)
		{
			char aRand = (char)Util_RandInt(0, 0xFF);
			cdkeyxor[i] = (char)(data->cdkey[i] ^ aRand);
		}
		cdkeyxor[i] = '\0';

		B64Encode(cdkeyxor, cdkeyenc, (int)cdkeylen, useAlternateEncoding);

		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\cdkeyenc\\");
		gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, cdkeyenc);
	}
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\partnerid\\");
	gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->partnerID);
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\id\\1");
	gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\final\\");

	return GP_NO_ERROR;
}

GPResult
gpiProcessConnect(
  GPConnection * connection,
  GPIOperation * operation,
  const char * input
)
{
	char buffer[512];
	char check[33];
	char uniquenick[GP_UNIQUENICK_LEN];
	GPIConnectData * data;
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPICallback callback;
	GPIProfile * profile;
	char userBuffer[GP_NICK_LEN + GP_EMAIL_LEN];
	char partnerBuffer[11];
	char * user;

	if(gpiCheckForError(connection, input, GPIFalse))
	{
		if((iconnection->errorCode == GP_LOGIN_PROFILE_DELETED) && iconnection->profileid)
		{
			gpiRemoveProfileByID(connection, iconnection->profileid);

			iconnection->userid = 0;
			iconnection->profileid = 0;
		}
		else if(iconnection->errorCode == GP_NEWUSER_BAD_NICK)
		{
			if(gpiValueForKey(input, "\\pid\\", buffer, sizeof(buffer)))
				iconnection->profileid = atoi(buffer);
		}

		CallbackFatalError(connection, GP_SERVER_ERROR, iconnection->errorCode, iconnection->errorString);
	}

	data = (GPIConnectData*)operation->data;

	switch(operation->state)
	{
	case GPI_CONNECTING:
		if(strncmp(input, "\\lc\\1", 5) != 0)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		if(!gpiValueForKey(input, "\\challenge\\", data->serverChallenge, sizeof(data->serverChallenge)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		if(data->newuser)
		{
			CHECK_RESULT(gpiSendNewuser(connection, data));

			operation->state = GPI_REQUESTING;
		}
		else
		{
			CHECK_RESULT(gpiSendLogin(connection, data));

			operation->state = GPI_LOGIN;
		}

		break;

	case GPI_REQUESTING:
		if(strncmp(input, "\\nur\\", 5) != 0)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		if(!gpiValueForKey(input, "\\userid\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexepected data was received from the server.");
		iconnection->userid = atoi(buffer);

		if(!gpiValueForKey(input, "\\profileid\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexepected data was received from the server.");
		iconnection->profileid = atoi(buffer);

		CHECK_RESULT(gpiSendLogin(connection, data));

		operation->state = GPI_LOGIN;

		break;
		
	case GPI_LOGIN:
		if(strncmp(input, "\\lc\\2", 5) != 0)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexpected data was received from the server.");

		if(!gpiValueForKey(input, "\\sesskey\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexepected data was received from the server.");
		iconnection->sessKey = atoi(buffer);

		if(!gpiValueForKey(input, "\\userid\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexepected data was received from the server.");
		iconnection->userid = atoi(buffer);

		if(!gpiValueForKey(input, "\\profileid\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexepected data was received from the server.");
		iconnection->profileid = atoi(buffer);

		if(!gpiValueForKey(input, "\\uniquenick\\", uniquenick, sizeof(uniquenick)))
			uniquenick[0] = '\0';

		if(!gpiValueForKey(input, "\\lt\\", iconnection->loginTicket, sizeof(iconnection->loginTicket)))
			iconnection->loginTicket[0] = '\0';


		if(iconnection->partnerID != GP_PARTNERID_GAMESPY)
		{
			sprintf(partnerBuffer, "%d@", iconnection->partnerID);
		}
		else
		{
			strcpy(partnerBuffer, "");
		}

		if(data->authtoken[0])
			user = data->authtoken;
		else if(iconnection->uniquenick[0])  
		{
			sprintf(userBuffer, "%s%s", partnerBuffer, iconnection->uniquenick);
			user = userBuffer;
		}
		else
		{
			sprintf(userBuffer, "%s%s@%s", partnerBuffer, iconnection->nick, iconnection->email);
			user = userBuffer;
		}

		sprintf(buffer, "%s%s%s%s%s%s",
			data->passwordHash,
			"                                                ",
			user,
			data->serverChallenge,
			data->userChallenge,
			data->passwordHash);
		MD5Digest((unsigned char *)buffer, strlen(buffer), check);

		if(!gpiValueForKey(input, "\\proof\\", buffer, sizeof(buffer)))
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_PARSE, "Unexepected data was received from the server.");

		if(memcmp(check, buffer, 32) != 0)
			CallbackFatalError(connection, GP_NETWORK_ERROR, GP_LOGIN_SERVER_AUTH_FAILED, "Could not authenticate server.");

		if(iconnection->infoCaching)
		{
			profile = gpiProfileListAdd(connection, iconnection->profileid);
			profile->profileId = iconnection->profileid;
			profile->userId = iconnection->userid;
		}

		iconnection->connectState = GPI_CONNECTED;

		callback = operation->callback;
		if(callback.callback != NULL)
		{
			GPConnectResponseArg * arg;
			arg = (GPConnectResponseArg *)gsimalloc(sizeof(GPConnectResponseArg));
			if(arg == NULL)
				Error(connection, GP_MEMORY_ERROR, "Out of memory.");
			memset(arg, 0, sizeof(GPConnectResponseArg));

			arg->profile = (GPProfile)iconnection->profileid;
			arg->result = GP_NO_ERROR;
#ifndef GSI_UNICODE
			strzcpy(arg->uniquenick, uniquenick, GP_UNIQUENICK_LEN);
#else
			UTF8ToUCS2StringLen(uniquenick, arg->uniquenick, GP_UNIQUENICK_LEN);
#endif
			
			CHECK_RESULT(gpiAddCallback(connection, callback, arg, operation, 0));
		}

		gpiRemoveOperation(connection, operation);

#if 0
		gpiAddOperation(connection, GPI_GET_INFO, NULL, &operation, GP_NON_BLOCKING, NULL, NULL);
		gpiSendGetInfo(connection, iconnection->profileid, operation->id);
#endif


#ifdef _PS3
		iconnection->npPerformBuddySync = gsi_true;
        iconnection->npPerformBlockSync = gsi_true;
		iconnection->loginTime = current_time();

		if (!iconnection->npInitialized)
			gpiInitializeNpBasic(connection);
#endif

		break;
		
	default:
		break;
	}
	
	return GP_NO_ERROR;
}

GPResult
gpiCheckConnect(
  GPConnection * connection
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	int state;
	
	CHECK_RESULT(gpiCheckSocketConnect(connection, iconnection->cmSocket, &state));
	
	if(state == GPI_DISCONNECTED)
		CallbackFatalError(connection, GP_SERVER_ERROR, GP_LOGIN_CONNECTION_FAILED, "The server has refused the connection.");

	if(state == GPI_NOT_CONNECTED)
		return GP_NO_ERROR;
	
	assert(state == GPI_CONNECTED);
	iconnection->connectState = GPI_NEGOTIATING;

	return GP_NO_ERROR;
}

static GPIBool
gpiDisconnectCleanupProfile(
  GPConnection * connection,
  GPIProfile * profile,
  void * data
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GSI_UNUSED(data);

	if(profile->buddyStatus)
	{
        profile->buddyOrBlockCache = gsi_true;
        freeclear(profile->buddyStatus->statusString);
        freeclear(profile->buddyStatus->locationString);
        freeclear(profile->buddyStatus);	
	}
	if (profile->buddyStatusInfo)
	{
        profile->buddyOrBlockCache = gsi_true;
        freeclear(profile->buddyStatusInfo->richStatus);
        freeclear(profile->buddyStatusInfo->gameType);
        freeclear(profile->buddyStatusInfo->gameVariant);
        freeclear(profile->buddyStatusInfo->gameMapName);
        if (profile->buddyStatusInfo->extendedInfoKeys)
        {
            ArrayFree(profile->buddyStatusInfo->extendedInfoKeys);
            profile->buddyStatusInfo->extendedInfoKeys = NULL;
        }
        freeclear(profile->buddyStatusInfo);
	}
    if (profile->blocked)
        profile->buddyOrBlockCache = gsi_true;

	freeclear(profile->authSig);
	freeclear(profile->peerSig);
	profile->requestCount = 0;

	if ((!profile->cache) ||
		(iconnection->infoCachingBuddyAndBlockOnly==GPITrue && !profile->buddyOrBlockCache))
	{
		gpiRemoveProfile(connection, profile);
		return GPIFalse;
	}

	return GPITrue;
}

void
gpiDisconnect(
  GPConnection * connection,
  GPIBool tellServer
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;
	GPIPeer * peer;
	GPIPeer * delPeer;
	GPIBool connClosed;

	if(iconnection->connectState == GPI_DISCONNECTED)
		return;

	if(iconnection->connectState != GPI_NOT_CONNECTED)
	{
		if(tellServer && (iconnection->connectState == GPI_CONNECTED))
		{
			gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\logout\\\\sesskey\\");
			gpiAppendIntToBuffer(connection, &iconnection->outputBuffer, iconnection->sessKey);
			gpiAppendStringToBuffer(connection, &iconnection->outputBuffer, "\\final\\");
		}

		gpiSendFromBuffer(connection, iconnection->cmSocket, &iconnection->outputBuffer, &connClosed, GPITrue, "CM");

		if(iconnection->cmSocket != INVALID_SOCKET)
		{
			shutdown(iconnection->cmSocket, 2);
			closesocket(iconnection->cmSocket);
			iconnection->cmSocket = INVALID_SOCKET;
		}
		
		if( gsUdpEngineIsInitialized())
		{
			gsUdpEngineRemoveMsgHandler(iconnection->mHeader);
			if (gsUdpEngineNoMoreMsgHandlers() && gsUdpEngineNoApp())
				gsUdpEngineShutdown();
		}

		iconnection->connectState = GPI_DISCONNECTED;

		iconnection->userid = 0;
		iconnection->profileid = 0;
	}
	
	freeclear(iconnection->socketBuffer.buffer);
	freeclear(iconnection->inputBuffer);
	freeclear(iconnection->outputBuffer.buffer);
	freeclear(iconnection->updateproBuffer.buffer);
	freeclear(iconnection->updateuiBuffer.buffer);
	while(iconnection->operationList != NULL)
		gpiRemoveOperation(connection, iconnection->operationList);
	iconnection->operationList = NULL;
	for(peer = iconnection->peerList ; peer != NULL ; )
	{
		delPeer = peer;
		peer = peer->pnext;
		gpiDestroyPeer(connection, delPeer);
	}
	iconnection->peerList = NULL;
	
	while(!gpiProfileMap(connection, gpiDisconnectCleanupProfile, NULL))  { };
}
