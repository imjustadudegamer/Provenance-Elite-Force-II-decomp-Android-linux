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
#include "peerMain.h"
#include "peerOperations.h"
#include "peerSB.h"
#include "peerCallbacks.h"
#include "peerMangle.h"
#include "peerRooms.h"
#include "peerAutoMatch.h"
#include "peerQR.h"

int piSBQueryVersion = QVERSION_QR2;

static void piSBGamesListCallback
(
	SBServerListPtr serverlist,
	SBListCallbackReason reason,
	SBServer server,
	void * instance
)
{
	PEER peer = (PEER)instance;
	PEER_CONNECTION;

#if 0
	{
	static const char reasonStrings[][32] =
	{
		"serveradded",
		"serverupdated",
		"serverdeleted",
		"initiallistcomplete",
		"disconnected",
		"queryerror",
		"publicipdetermined"
	};
	char buffer[256];
	sprintf(buffer, "GAMES  | LIST   | %s", reasonStrings[reason]);
	if(server)
		sprintf(buffer + strlen(buffer), " | %s:%d", SBServerGetPublicAddress(server), SBServerGetPublicQueryPort(server));
	strcat(buffer, "\n");
	OutputDebugString(buffer);
	}
#endif

	switch(reason)
	{
	case slc_serveradded:
		piAddListingGamesCallback(peer, PEERTrue, server, PEER_ADD);
		if(!SBServerHasBasicKeys(server))
		{
			SBBool usequerychallenge = SBFalse;
			if (serverlist->backendgameflags & QR2_USE_QUERY_CHALLENGE)
				usequerychallenge = SBTrue;
			SBQueryEngineUpdateServer(&connection->gameEngine, server, 0, QTYPE_BASIC, usequerychallenge);
		}
		break;

	case slc_serverupdated:
		piAddListingGamesCallback(peer, PEERTrue, server, PEER_UPDATE);
		break;

	case slc_serverdeleted:
		if ((server->state & (STATE_PENDINGBASICQUERY|STATE_PENDINGFULLQUERY|STATE_PENDINGICMPQUERY)) != 0) 
			SBQueryEngineRemoveServerFromFIFOs(&connection->gameEngine, server);
		piAddListingGamesCallback(peer, PEERTrue, server, PEER_REMOVE);
		break;

	case slc_initiallistcomplete:
		connection->initialGameList = PEERFalse;
			
		piAddListingGamesCallback(peer, PEERTrue, NULL, PEER_COMPLETE);

		break;

	case slc_queryerror:
		piAddListingGamesCallback(peer, PEERFalse, NULL, 0);
		break;
		
	case slc_publicipdetermined:
		connection->publicIP = serverlist->mypublicip;
		SBQueryEngineSetPublicIP(&connection->gameEngine, serverlist->mypublicip);
		break;

	default:
		break;
	}
}

static void piSBGamesEngineCallback
(
	SBQueryEnginePtr engine,
	SBQueryEngineCallbackReason reason,
	SBServer server,
	void * instance
)
{
	PEER peer = (PEER)instance;

#if 0
	{
	static const char reasonStrings[][32] =
	{
		"updatesuccess",
		"updatefailed",
		"engineidle"
	};
	char buffer[256];
	sprintf(buffer, "GAMES  | ENGINE | %s", reasonStrings[reason]);
	if(server)
		sprintf(buffer + strlen(buffer), " | %s:%d", SBServerGetPublicAddress(server), SBServerGetPublicQueryPort(server));
	strcat(buffer, "\n");
	OutputDebugString(buffer);
	}
#endif

	if(reason == qe_updatesuccess)
		piAddListingGamesCallback(peer, PEERTrue, server, PEER_UPDATE);
		
	GSI_UNUSED(engine);
}

PEERBool piSBStartListingGames
(
	PEER peer,
	const unsigned char * fields,
	int numFields,
	const char * filter
)
{
	char groupFilter[32];
	char smartSpyFilter[MAX_FILTER_LEN];
	char fullFields[MAX_FIELD_LIST_LEN];
	int listLen;
	int keyLen;
	int i;

	PEER_CONNECTION;

	assert(connection->sbInitialized);
	if(!connection->sbInitialized)
		return PEERFalse;

	piSBStopListingGames(peer);

	SBServerListClear(&connection->gameList);

	if(connection->groupID)
		sprintf(groupFilter, "groupid=%d", connection->groupID);
	else
		strcpy(groupFilter, "groupid is null");

	if(filter)
	{
		sprintf(smartSpyFilter, "(%s) AND (", groupFilter);
		strzcat(smartSpyFilter, filter, sizeof(smartSpyFilter) - 1);
		strcat(smartSpyFilter, ")");
	}
	else
	{
		strcpy(smartSpyFilter, groupFilter);
	}

	connection->gameEngine.numserverkeys = 0;

	strcpy(fullFields, "\\hostname\\gamemode");
	listLen = (int)strlen(fullFields);
	SBQueryEngineAddQueryKey(&connection->gameEngine, HOSTNAME_KEY);
	SBQueryEngineAddQueryKey(&connection->gameEngine, GAMEMODE_KEY);

	for(i = 0 ; i < numFields ; i++)
	{
		keyLen = (int)strlen(qr2_registered_key_list[fields[i]]);
		if((listLen + keyLen + 1) >= MAX_FIELD_LIST_LEN)
			break;

		listLen += sprintf(fullFields + listLen, "\\%s", qr2_registered_key_list[fields[i]]);

		SBQueryEngineAddQueryKey(&connection->gameEngine, fields[i]);
	}

	if(SBServerListConnectAndQuery(&connection->gameList, fullFields, smartSpyFilter, PUSH_UPDATES, 0) != sbe_noerror)
	{
		piSBStopListingGames(peer);
		return PEERFalse;
	}

	connection->initialGameList = PEERTrue;

	piAddListingGamesCallback(peer, PEERTrue, NULL, PEER_CLEAR);

	return PEERTrue;
}

void piSBStopListingGames
(
	PEER peer
)
{
	PEER_CONNECTION;

	assert(connection->sbInitialized);
	if(!connection->sbInitialized)
		return;

	SBServerListDisconnect(&connection->gameList);
	
	SBEngineHaltUpdates(&connection->gameEngine);

	piClearCallbacks(peer, PI_LISTING_GAMES_CALLBACK);
}

static void piSBGroupsListCallback
(
	SBServerListPtr serverlist,
	SBListCallbackReason reason,
	SBServer server,
	void * instance
)
{
	piOperation * operation;
	PEER peer = (PEER)instance;
	PEER_CONNECTION;

	operation = connection->listingGroupsOperation;

#if 0
	{
	static const char reasonStrings[][32] =
	{
		"serveradded",
		"serverupdated",
		"serverdeleted",
		"initiallistcomplete",
		"disconnected",
		"queryerror",
		"publicipdetermined"
	};
	char buffer[256];
	sprintf(buffer, "GROUPS | LIST   | %s", reasonStrings[reason]);
	if(server)
		sprintf(buffer + strlen(buffer), " | %d", ntohl(SBServerGetPublicInetAddress(server)));
	strcat(buffer, "\n");
	OutputDebugString(buffer);
	}
#endif

	switch(reason)
	{
	case slc_serveradded:
		{
		int groupID = (int)ntohl(SBServerGetPublicInetAddress(server));
		const char * name = SBServerGetStringValueA(server, "hostname", "(No Name)");
		int numWaiting = SBServerGetIntValueA(server, "numwaiting", 0);
		int maxWaiting = SBServerGetIntValueA(server, "maxwaiting", 0);
		int numGames = SBServerGetIntValueA(server, "numservers", 0);
		int numPlaying = SBServerGetIntValueA(server, "numplayers", 0);
		piAddListGroupRoomsCallback(peer, PEERTrue, groupID, server, name, numWaiting, maxWaiting, numGames, numPlaying, (peerListGroupRoomsCallback)operation->callback, operation->callbackParam, operation->ID);
		break;
		}

	case slc_initiallistcomplete:
		piAddListGroupRoomsCallback(peer, PEERTrue, 0, NULL, NULL, 0, 0, 0, 0, (peerListGroupRoomsCallback)operation->callback, operation->callbackParam, operation->ID);
		piRemoveOperation(peer, operation);
		connection->listingGroupsOperation = NULL;
		break;

	case slc_queryerror:
		piAddListGroupRoomsCallback(peer, PEERFalse, 0, NULL, NULL, 0, 0, 0, 0, (peerListGroupRoomsCallback)operation->callback, operation->callbackParam, operation->ID);
		piRemoveOperation(peer, operation);
		connection->listingGroupsOperation = NULL;
		break;

	default:
		break;
	}
	
	GSI_UNUSED(serverlist);
}

PEERBool piSBStartListingGroups
(
	PEER peer,
	const char * fields
)
{
	char fullFields[MAX_FIELD_LIST_LEN + 1];
	int len;

	PEER_CONNECTION;

	assert(connection->sbInitialized);
	if(!connection->sbInitialized)
		return PEERFalse;

	piSBStopListingGroups(peer);

	SBServerListClear(&connection->groupList);

	strcpy(fullFields, "\\hostname\\numwaiting\\maxwaiting\\numservers\\numplayers");
	len = (int)strlen(fullFields);
	strzcpy(fullFields + len, fields, (unsigned int)MAX_FIELD_LIST_LEN - len);

	if(SBServerListConnectAndQuery(&connection->groupList, fullFields, "", SEND_GROUPS, 0) != sbe_noerror)
	{
		piSBStopListingGroups(peer);
		return PEERFalse;
	}

	return PEERTrue;
}

void piSBStopListingGroups
(
	PEER peer
)
{
	PEER_CONNECTION;

	assert(connection->sbInitialized);
	if(!connection->sbInitialized)
		return;

	SBServerListDisconnect(&connection->groupList);
}

static PEERBool piIsLocalServer(PEER peer, SBServer server)
{
	PEER_CONNECTION;

	if(SBServerGetPublicInetAddress(server) != connection->publicIP)
		return PEERFalse;

	if(SBServerHasPrivateAddress(server))
	{
		if(SBServerGetPrivateInetAddress(server) != connection->privateIP)
			return PEERFalse;

		if(SBServerGetPrivateQueryPort(server) != connection->autoMatchOperation->port)
			return PEERFalse;
	}
	else
	{
		if(SBServerGetPublicQueryPort(server) != connection->autoMatchOperation->port)
			return PEERFalse;
	}

	return PEERTrue;
}

static int piSBAutoMatchGetServerRating(PEER peer, SBServer server)
{
	PEER_CONNECTION;
		
	if(connection->autoMatchReporting && piIsLocalServer(peer, server))
		return 0;

	if(!SBServerHasFullKeys(server))
		return 0;
	
#ifndef GSI_UNICODE
	if(SBServerGetIntValue(server, "numplayers", 0) >= SBServerGetIntValue(server, "maxplayers", 0))
		return 0;
#else
	if(SBServerGetIntValue(server, L"numplayers", 0) >= SBServerGetIntValue(server, L"maxplayers", 0))
		return 0;
#endif

	return piCallAutoMatchRateCallback(peer, server);
}

static void piSBAutoMatchCheckUpdatedServer(PEER peer, SBServer server)
{
	int rating;

	PEER_CONNECTION;
	
	if(connection->autoMatchStatus != PEERWaiting)
		return;
	
	if(connection->enteringRoom[StagingRoom])
		return;

	rating = piSBAutoMatchGetServerRating(peer, server);

	if(rating <= 0)
		return;

	piStopAutoMatchReporting(peer);
	piLeaveRoom(peer, StagingRoom, "");

	if(!piJoinAutoMatchRoom(peer, server))
	{
		piSetAutoMatchStatus(peer, PEERFailed);
	}
}

static void piSBAutoMatchCheckPushedServer(PEER peer, SBServer server)
{
	SortInfo info;
	int aRating;
	PEER_CONNECTION;
	
	if(connection->autoMatchStatus != PEERWaiting && connection->autoMatchStatus != PEERSearching)
		return;

	if(connection->enteringRoom[StagingRoom])
		return;

	if (SBServerListCount(&connection->autoMatchList) == 1)
	{
		aRating = piSBAutoMatchGetServerRating(peer, server);

		if(aRating <= 0)
			return;

		piStopAutoMatchReporting(peer);
		piLeaveRoom(peer, StagingRoom, "");

		if(!piJoinAutoMatchRoom(peer, server))
		{
			piSetAutoMatchStatus(peer, PEERFailed);
		}	
	}

	else if (SBServerListCount(&connection->autoMatchList) > 1)
	{
		int count, i;
		SBServer aServer;

		count = SBServerListCount(&connection->autoMatchList);
		for(i = (count - 1) ; i >= 0 ; i--)
		{
			aServer = SBServerListNth(&connection->autoMatchList, i);

			aRating = piSBAutoMatchGetServerRating(peer, aServer);
			if(aRating <= 0)
			{
				SBServerListRemoveAt(&connection->autoMatchList, i);
				continue;
			}

			SBServerAddIntKeyValue(aServer, PI_AUTOMATCH_RATING_KEY, aRating);
		}

		count = SBServerListCount(&connection->autoMatchList);

		if(!count)
		{
			piSetAutoMatchStatus(peer, PEERWaiting);
			return;
		}

#ifdef GSI_UNICODE
		_tcscpy(info.sortkey, (const unsigned short *)PI_AUTOMATCH_RATING_KEY);
#else
		_tcscpy(info.sortkey, PI_AUTOMATCH_RATING_KEY);
#endif
		info.comparemode = sbcm_int;
		SBServerListSort(&connection->autoMatchList, SBTrue, info);
		
		piStopAutoMatchReporting(peer);
		piLeaveRoom(peer, StagingRoom, "");

		if(!piJoinAutoMatchRoom(peer, SBServerListNth(&connection->autoMatchList, 0)))
		{
			piSetAutoMatchStatus(peer, PEERFailed);
		}
	}

}

static void piSBAutoMatchListCallback
(
	SBServerListPtr serverlist,
	SBListCallbackReason reason,
	SBServer server,
	void * instance
)
{
	PEER peer = (PEER)instance;
	PEER_CONNECTION;

#if 0
	{
	static const char reasonStrings[][32] =
	{
		"serveradded",
		"serverupdated",
		"serverdeleted",
		"initiallistcomplete",
		"disconnected",
		"queryerror",
		"publicipdetermined"
	};
	char buffer[256];
	sprintf(buffer, "AUTOMATCH | LIST   | %s", reasonStrings[reason]);
	if(server)
		sprintf(buffer + strlen(buffer), " | %s:%d", SBServerGetPublicAddress(server), SBServerGetPublicQueryPort(server));
	strcat(buffer, "\n");
	OutputDebugString(buffer);
	}
#endif

	switch(reason)
	{
	case slc_serveradded:
		if(server->state & (STATE_FULLKEYS|STATE_PENDINGFULLQUERY))
			break;

		if (!SBServerDirectConnect(server))
			SBGetServerRulesFromMaster(&connection->autoMatchList, SBServerGetPublicInetAddress(server), SBServerGetPublicQueryPortNBO(server));
		else 
		{
			SBBool usequerychallenge = SBFalse;
			if (serverlist->backendgameflags & QR2_USE_QUERY_CHALLENGE)
				usequerychallenge = SBTrue;
			SBQueryEngineUpdateServer(&connection->autoMatchEngine, server, 0, QTYPE_FULL, usequerychallenge);
		}
		break;

	case slc_serverupdated:
		if(!SBServerHasFullKeys(server) && SBServerDirectConnect(server) && !(server->state & STATE_PENDINGFULLQUERY))
		{
			SBBool usequerychallenge = SBFalse;
			if (serverlist->backendgameflags & QR2_USE_QUERY_CHALLENGE)
				usequerychallenge = SBTrue;
			SBQueryEngineUpdateServer(&connection->autoMatchEngine, server, 0, QTYPE_FULL, usequerychallenge);
		}
		else if(!SBServerHasFullKeys(server) && !SBServerDirectConnect(server))
		{
			SBGetServerRulesFromMaster(&connection->autoMatchList, SBServerGetPublicInetAddress(server), SBServerGetPublicQueryPortNBO(server));
		}
		else
			piSBAutoMatchCheckPushedServer(peer, server);
		break;

	case slc_serverdeleted:
		if ((server->state & (STATE_PENDINGBASICQUERY|STATE_PENDINGFULLQUERY)) != 0) 
			SBQueryEngineRemoveServerFromFIFOs(&connection->autoMatchEngine, server);
		break;

	case slc_initiallistcomplete:
		if(!SBServerListCount(&connection->autoMatchList) ||
			(0==connection->autoMatchEngine.querylist.count))
		{
			piSetAutoMatchStatus(peer, PEERWaiting);
		}
		break;
	case slc_queryerror:
		connection->autoMatchSBFailed = PEERTrue;
		if(connection->autoMatchStatus == PEERSearching)
			piSetAutoMatchStatus(peer, connection->autoMatchQRFailed?PEERFailed:PEERWaiting);
		break;

	case slc_publicipdetermined:
		connection->publicIP = serverlist->mypublicip;
		SBQueryEngineSetPublicIP(&connection->gameEngine, serverlist->mypublicip);
		break;

	default:
		break;
	}
}

static void piSBAutoMatchEngineCallback
(
	SBQueryEnginePtr engine,
	SBQueryEngineCallbackReason reason,
	SBServer server,
	void * instance
)
{
	PEER peer = (PEER)instance;
	int i;
	int count;
	int rating;
	SortInfo info;

	PEER_CONNECTION;

#if 0
	{
	static const char reasonStrings[][32] =
	{
		"updatesuccess",
		"updatefailed",
		"engineidle"
	};
	char buffer[256];
	sprintf(buffer, "AUTOMATCH | ENGINE | %s", reasonStrings[reason]);
	if(server)
		sprintf(buffer + strlen(buffer), " | %s:%d", SBServerGetPublicAddress(server), SBServerGetPublicQueryPort(server));
	strcat(buffer, "\n");
	OutputDebugString(buffer);
	}
#endif

	switch(reason)
	{
	case qe_updatesuccess:
		piSBAutoMatchCheckUpdatedServer(peer, server);
		break;

	case qe_updatefailed:
		if(!SBServerListCount(&connection->autoMatchList))
			piSetAutoMatchStatus(peer, PEERWaiting);
		break;

	case qe_engineidle:
		if(connection->autoMatchStatus != PEERSearching)
			return;
		
		if(connection->inRoom[StagingRoom] || connection->enteringRoom[StagingRoom])
			return;

		count = SBServerListCount(&connection->autoMatchList);
		for(i = (count - 1) ; i >= 0 ; i--)
		{
			server = SBServerListNth(&connection->autoMatchList, i);

			rating = piSBAutoMatchGetServerRating(peer, server);
			if(rating <= 0)
			{
				SBServerListRemoveAt(&connection->autoMatchList, i);
				continue;
			}

			SBServerAddIntKeyValue(server, PI_AUTOMATCH_RATING_KEY, rating);
		}

		count = SBServerListCount(&connection->autoMatchList);

		if(!count)
		{
			piSetAutoMatchStatus(peer, PEERWaiting);
			return;
		}

#ifdef GSI_UNICODE
		_tcscpy(info.sortkey, (const unsigned short *)PI_AUTOMATCH_RATING_KEY);
#else
		_tcscpy(info.sortkey, PI_AUTOMATCH_RATING_KEY);
#endif
		
		info.comparemode = sbcm_int;
		SBServerListSort(&connection->autoMatchList, SBTrue, info);

		if(!piJoinAutoMatchRoom(peer, SBServerListNth(&connection->autoMatchList, 0)))
		{
			piSetAutoMatchStatus(peer, PEERFailed);
		}

		break;

	default:
		break;
	}
	
	GSI_UNUSED(engine);
}

PEERBool piSBStartListingAutoMatches
(
	PEER peer
)
{
	PEER_CONNECTION;

	assert(connection->sbInitialized);
	if(!connection->sbInitialized)
	{
		connection->autoMatchSBFailed = PEERTrue;
		return PEERFalse;
	}

	piSBStopListingAutoMatches(peer);

	SBServerListClear(&connection->autoMatchList);

	if(SBServerListConnectAndQuery(&connection->autoMatchList, NULL, connection->autoMatchFilter, PUSH_UPDATES, 0) != sbe_noerror)
	{
		piSBStopListingAutoMatches(peer);
		connection->autoMatchSBFailed = PEERTrue;
		return PEERFalse;
	}

	connection->autoMatchSBFailed = PEERFalse;

	connection->autoMatchBrowsing = PEERTrue;

	return PEERTrue;
}

void piSBStopListingAutoMatches
(
	PEER peer
)
{
	PEER_CONNECTION;

	assert(connection->sbInitialized);
	if(!connection->sbInitialized)
		return;

	connection->autoMatchBrowsing = PEERFalse;

	SBServerListDisconnect(&connection->autoMatchList);
	
	SBEngineHaltUpdates(&connection->autoMatchEngine);
}

PEERBool piSBInit
(
	PEER peer
)
{
	char autoMatchTitle[PI_TITLE_MAX_LEN];

	PEER_CONNECTION;

	memset(&connection->gameList, 0, sizeof(connection->gameList));
	SBServerListInit(&connection->gameList, connection->title, connection->sbName, connection->sbSecretKey, connection->sbGameVersion, SBFalse, piSBGamesListCallback, peer);
	SBQueryEngineInit(&connection->gameEngine, connection->sbMaxUpdates, piSBQueryVersion, SBFalse, piSBGamesEngineCallback, peer);

	memset(&connection->groupList, 0, sizeof(connection->groupList));
	SBServerListInit(&connection->groupList, connection->title, connection->sbName, connection->sbSecretKey, connection->sbGameVersion, SBFalse, piSBGroupsListCallback, peer);

	strzcpy(autoMatchTitle, connection->title, sizeof(autoMatchTitle));
	strzcat(autoMatchTitle, "am", sizeof(autoMatchTitle));
	memset(&connection->autoMatchList, 0, sizeof(connection->autoMatchList));
	SBServerListInit(&connection->autoMatchList, autoMatchTitle, connection->sbName, connection->sbSecretKey, connection->sbGameVersion, SBFalse, piSBAutoMatchListCallback, peer);
	SBQueryEngineInit(&connection->autoMatchEngine, connection->sbMaxUpdates, piSBQueryVersion, SBFalse, piSBAutoMatchEngineCallback, peer);

	connection->hostServer = NULL;

	connection->sbInitialized = PEERTrue;

	return PEERTrue;
}

void piSBCleanup
(
	PEER peer
)
{
	PEER_CONNECTION;

	piSBStopListingGames(peer);
	piSBStopListingGroups(peer);

	if(connection->sbInitialized)
	{
		SBServerListCleanup(&connection->gameList);
		SBEngineCleanup(&connection->gameEngine);
		SBServerListCleanup(&connection->groupList);
		SBServerListCleanup(&connection->autoMatchList);
		SBEngineCleanup(&connection->autoMatchEngine);
	}

	connection->autoMatchBrowsing = PEERFalse;
}


void piSBUpdateGame
(
	PEER peer,
	SBServer server,
	PEERBool fullUpdate, 
	PEERBool forceUpdateByMaster,
	PEERBool icmpEcho
)
{
	PEER_CONNECTION;

	assert(connection->sbInitialized);
	
	if(!connection->sbInitialized)
		return;
	
	if (forceUpdateByMaster)
	{
		if(server->flags & UNSOLICITED_UDP_FLAG)
			SBQueryEngineRemoveServerFromFIFOs(&connection->gameEngine, server);
		
		SBGetServerRulesFromMaster(&connection->gameList, server->publicip, server->publicport);
		return;
	}

	if (icmpEcho && (server->flags & ICMP_IP_FLAG))
	{
		SBBool usequerychallenge = SBFalse;
		if (connection->gameList.backendgameflags & QR2_USE_QUERY_CHALLENGE)
			usequerychallenge = SBTrue;

		if (server->flags & UNSOLICITED_UDP_FLAG)
			SBQueryEngineRemoveServerFromFIFOs(&connection->gameEngine, server);

		SBQueryEngineUpdateServer(&connection->gameEngine, server, 1, QTYPE_ICMP, usequerychallenge);
		return;
	}
	if(server->flags & UNSOLICITED_UDP_FLAG)
	{
		SBBool usequerychallenge = SBFalse;
		if (connection->gameList.backendgameflags & QR2_USE_QUERY_CHALLENGE)
			usequerychallenge = SBTrue;

		SBQueryEngineRemoveServerFromFIFOs(&connection->gameEngine, server);

		SBQueryEngineUpdateServer(&connection->gameEngine, server, 1, fullUpdate?QTYPE_FULL:QTYPE_BASIC, usequerychallenge);
	}
	else
	{
		SBGetServerRulesFromMaster(&connection->gameList, server->publicip, server->publicport);
	}
}

void piSBThink
(
	PEER peer
)
{
	PEER_CONNECTION;

	if(!connection->sbInitialized)
		return;

	SBListThink(&connection->gameList);
	SBListThink(&connection->groupList);
	SBQueryEngineThink(&connection->gameEngine);
	SBListThink(&connection->autoMatchList);
	SBQueryEngineThink(&connection->autoMatchEngine);
}

void piSendNatNegotiateCookie
(
	PEER peer,
	unsigned int ip,
	unsigned short port,
	int cookie
)
{
	PEER_CONNECTION;

	if(!connection->sbInitialized)
		return;

	if (peerIsAutoMatching(peer))
	{
		SBSendNatNegotiateCookieToServer(&connection->autoMatchList, ip, htons(port), cookie);
	}
	else
		SBSendNatNegotiateCookieToServer(&connection->gameList, ip, htons(port), cookie);
}

void piSendMessageToServer
(
	PEER peer,
	unsigned int ip,
	unsigned short port,
	const char * data,
	int len
)
{
	PEER_CONNECTION;

	if(!connection->sbInitialized)
		return;

	if(peerIsAutoMatching(peer))
	{
		SBSendMessageToServer(&connection->autoMatchList, ip, port, data, len);
	}
	else
	{
		SBSendMessageToServer(&connection->gameList, ip, port, data, len);
	}
}

static void piSBCloneServerTableMap(void * elem, void * clientData)
{
	SBKeyValuePair * kvPair = (SBKeyValuePair *)elem;
	SBServer clone = (SBServer)clientData;

	SBServerAddKeyValue(clone, kvPair->key, kvPair->value);
}

SBServer piSBCloneServer(SBServer server)
{
	SBServer clone;
	HashTable table;

	clone = SBAllocServer(NULL, server->publicip, server->publicport);

	table = clone->keyvals;

	memcpy(clone, server, sizeof(struct _SBServer));
	clone->keyvals = table;
	clone->next = NULL;

	TableMap(server->keyvals, piSBCloneServerTableMap, clone);

	return clone;
}

void piSBFreeHostServer(PEER peer)
{
	PEER_CONNECTION;

	if(!connection->hostServer)
		return;

	SBServerFree(&connection->hostServer);
	connection->hostServer = NULL;
}
