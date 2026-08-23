#include "sb_serverbrowsing.h"
#include "sb_internal.h"

#ifdef GSI_MANIC_DEBUG
void FIFODebugCheckAdd(SBServerFIFO *fifo, SBServer server)
{
	SBServer aServer = fifo->first;
	while(aServer != NULL)
	{
		assert(aServer != server);
		aServer = aServer->next;
	}
}


void FIFODebugCheck(SBServerFIFO *fifo)
{
	int i=0;
	SBServer aServer;

	assert(fifo != NULL);
	aServer = fifo->first;
	for (i=0; i < fifo->count; i++)
	{
		assert(aServer != NULL);
		aServer = aServer->next;
	}
}
#else
#define FIFODebugCheckAdd(a,b)
#define FIFODebugCheck(a)
#endif

static void FIFOAddRear(SBServerFIFO *fifo, SBServer server)
{
	FIFODebugCheckAdd(fifo, server);

	if (fifo->last != NULL)
		fifo->last->next = server;
	fifo->last = server;
	server->next = NULL;
	if (fifo->first == NULL)
		fifo->first = server;
	fifo->count++;

	FIFODebugCheck(fifo);
}

static void FIFOAddFront(SBServerFIFO *fifo, SBServer server)
{
	FIFODebugCheckAdd(fifo, server);

	server->next = fifo->first;
	fifo->first = server;
	if (fifo->last == NULL)
		fifo->last = server;
	fifo->count++;

	FIFODebugCheck(fifo);
}

static SBServer FIFOGetFirst(SBServerFIFO *fifo)
{
	SBServer hold;
	hold = fifo->first;
	if (hold != NULL)
	{
		fifo->first = hold->next;
		if (fifo->first == NULL)
			fifo->last = NULL;
		fifo->count--;
	}

	FIFODebugCheck(fifo);
	return hold;
}

static SBBool FIFORemove(SBServerFIFO *fifo, SBServer server)
{
	SBServer hold, prev;
	prev = NULL;
	hold = fifo->first;
	while (hold != NULL)
	{
		if (hold == server) 
		{
			if (prev != NULL) 
				prev->next = hold->next;
			if (fifo->first == hold)
				fifo->first = hold->next;
			if (fifo->last == hold)
				fifo->last = prev;
			fifo->count--;
			return SBTrue;
		}
		prev = hold;
		hold = hold->next;
	}

	FIFODebugCheck(fifo);
	return SBFalse;
}

static void FIFOClear(SBServerFIFO *fifo)
{
	fifo->first = fifo->last = NULL;
	fifo->count = 0;

	FIFODebugCheck(fifo);
}

#ifdef SB_ICMP_SUPPORT
static unsigned short IPChecksum(const unsigned short *buf, int len)
{
	unsigned long cksum = 0;
	
	while (len > 1)
	{
		cksum += *buf++;
		len -= sizeof(unsigned short);
	}
	
	if (len) {
		cksum += *(unsigned char*)buf;
	}
	
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	
	return (unsigned short)(~cksum);
}
#endif

static void QEStartQuery(SBQueryEngine *engine, SBServer server)
{
	unsigned char queryBuffer[256];
	int queryLen;
	gsi_bool querySuccess = gsi_false;
	struct sockaddr_in saddr;

	saddr.sin_family = AF_INET;
	server->updatetime = current_time();

	if (server->state & STATE_PENDINGICMPQUERY) 
	{
#ifdef SB_ICMP_SUPPORT
	#if !defined(SN_SYSTEMS)
		SBICMPHeader *_icmp = (SBICMPHeader *)(queryBuffer);
		_icmp->type = SB_ICMP_ECHO;
		_icmp->code = 0;
		_icmp->un.idseq = server->updatetime; 
		_icmp->cksum = 0;
		queryLen = sizeof(SBICMPHeader) + 6;
		memcpy(queryBuffer + sizeof(SBICMPHeader), &server->publicip, 4); 
		memcpy(queryBuffer + sizeof(SBICMPHeader) + 4, &server->publicport, 2); 
		_icmp->cksum = IPChecksum((unsigned short *)queryBuffer, queryLen);
		if (SBServerGetFlags(server) & ICMP_IP_FLAG) 
		{
			saddr.sin_addr.s_addr = server->icmpip;
		} else
		{
			saddr.sin_addr.s_addr = server->publicip;
		}

		sendto(engine->icmpsock, (char *)queryBuffer, queryLen, 0, (struct sockaddr *)&saddr, sizeof(saddr));
		querySuccess = gsi_true;
	#else
		int result;
		sndev_set_ping_ip_type optval;
		optval.ip_addr = server->icmpip;

		gsDebugFormat(GSIDebugCat_SB, GSIDebugType_Network, GSIDebugLevel_WarmError,
				"Attempting to send ICMP ping to %s\r\n", inet_ntoa(*((struct in_addr*)&server->icmpip)));

		result = sndev_set_options(0, SN_DEV_SET_PING_IP, (void*)&optval, sizeof(optval)); 
		if (result==SN_PING_FAIL)
		{
			gsDebugFormat(GSIDebugCat_SB, GSIDebugType_Network, GSIDebugLevel_WarmError,
				"ICMP ping attempt failed on SNSystems (unknown error)\r\n");
			querySuccess = gsi_true; 
		}
		else if (result==SN_PING_FULL)
		{
			gsDebugFormat(GSIDebugCat_SB, GSIDebugType_Network, GSIDebugLevel_WarmError,
				"ICMP ping attempt failed on SNSystems (SN_PING_FULL)\r\n");
			querySuccess = gsi_false;
		}
		else
			querySuccess = gsi_true;
	#endif
#endif
	} else 
	{
		if (engine->queryversion == QVERSION_QR2)
		{
			if (server->state & STATE_PENDINGQUERYCHALLENGE)
			{
				int pos = 0;
				queryBuffer[pos++] = QR2_MAGIC_1;
				queryBuffer[pos++] = QR2_MAGIC_2;
				queryBuffer[pos++] = 0x09; 
				memcpy(&queryBuffer[pos], &server->updatetime, 4);
				pos += 4;
				queryLen = pos;
			}
			else
			{
				gsi_u32 challengeNBO = htonl(server->querychallenge);
				int pos = 0;

				queryBuffer[pos++] = QR2_MAGIC_1;
				queryBuffer[pos++] = QR2_MAGIC_2;
				queryBuffer[pos++] = 0;
				memcpy(&queryBuffer[pos], &server->updatetime, 4); 
				pos += 4;
				if (challengeNBO != 0)
				{
					memcpy(&queryBuffer[pos], &challengeNBO, 4); 
					pos += 4;
				}
				if (server->state & STATE_PENDINGBASICQUERY) 
				{
					int i;
					queryBuffer[pos++] = (unsigned char)engine->numserverkeys;
					for (i = 0 ; i < engine->numserverkeys ; i++)
						queryBuffer[pos++] = engine->serverkeys[i];

					queryBuffer[pos++] = 0x00;
					queryBuffer[pos++] = 0x00;
					queryLen = pos;

				} else  
				{
					queryBuffer[pos++] = 0xFF;
					queryBuffer[pos++] = 0xFF;
					queryBuffer[pos++] = 0xFF;
					
					queryBuffer[pos++] = 0x1;
					queryLen = pos; 
				}
			}
		} else 
		{
			if (server->state & STATE_PENDINGBASICQUERY) 
			{
				memcpy(queryBuffer, BASIC_GOA_QUERY, BASIC_GOA_QUERY_LEN);
				queryLen = BASIC_GOA_QUERY_LEN;
			} else 
			{
				memcpy(queryBuffer, FULL_GOA_QUERY, FULL_GOA_QUERY_LEN);
				queryLen = FULL_GOA_QUERY_LEN;		
			}
		}
		if (server->publicip == engine->mypublicip && (server->flags & PRIVATE_IP_FLAG)) 
		{
			saddr.sin_addr.s_addr = server->privateip;
			saddr.sin_port = server->privateport;
		} else
		{
			saddr.sin_addr.s_addr = server->publicip;
			saddr.sin_port = server->publicport;
		}
		sendto(engine->querysock, (char *)queryBuffer, queryLen, 0, (struct sockaddr *)&saddr, sizeof(saddr));
		querySuccess = gsi_true;
	}

	if (gsi_is_true(querySuccess))
		FIFOAddRear(&engine->querylist, server);
	else
		server->updatetime = 0;
}


void SBQueryEngineInit(SBQueryEngine *engine, int maxupdates, int queryversion, SBBool lanBrowse, SBEngineCallbackFn callback, void *instance)
{
	if(lanBrowse == SBFalse)
	{
		if(__GSIACResult != GSIACAvailable)
		return;
	}
		SocketStartUp();
	engine->queryversion = queryversion;
	engine->maxupdates = maxupdates;
	engine->numserverkeys = 0;
	engine->ListCallback = callback;
	engine->instance = instance;
	engine->mypublicip = 0;
	engine->querysock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(SB_ICMP_SUPPORT)
	#if defined(SN_SYSTEMS)
	{
		sndev_set_ping_reset_type optval;
		optval.timeout_ms = MAX_QUERY_MSEC; 
		optval.reserved = 0;
		sndev_set_options(0, SN_DEV_SET_PING_RESET, &optval, sizeof(optval));
	}
	#else
	engine->icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	#endif
#endif
	FIFOClear(&engine->pendinglist);
	FIFOClear(&engine->querylist);
}

void SBQueryEngineSetPublicIP(SBQueryEngine *engine, goa_uint32 mypublicip)
{
	engine->mypublicip = mypublicip;
}

void SBEngineHaltUpdates(SBQueryEngine *engine)
{
	FIFOClear(&engine->pendinglist);
	FIFOClear(&engine->querylist);
}


void SBEngineCleanup(SBQueryEngine *engine)
{
	closesocket(engine->querysock);
#ifdef SB_ICMP_SUPPORT
	#if !defined(SN_SYSTEMS)
	closesocket(engine->icmpsock);
	#endif
#endif
	engine->querysock = INVALID_SOCKET;
	FIFOClear(&engine->pendinglist);
	FIFOClear(&engine->querylist);
}


void SBQueryEngineUpdateServer(SBQueryEngine *engine, SBServer server, int addfront, int querytype, SBBool usequerychallenge)
{
	FIFODebugCheckAdd(&engine->pendinglist, server);
	FIFODebugCheckAdd(&engine->querylist, server);

	server->state &= (unsigned char)~(STATE_PENDINGBASICQUERY|STATE_PENDINGFULLQUERY|STATE_PENDINGICMPQUERY|STATE_PENDINGQUERYCHALLENGE|STATE_QUERYFAILED); 
	server->splitResponseBitmap = 0;
	server->querychallenge = 0;

#ifndef SB_ICMP_SUPPORT
	if (querytype == QTYPE_ICMP)
		return; 
#endif

	if (querytype == QTYPE_BASIC)
		server->state |= STATE_PENDINGBASICQUERY;
	else if (querytype == QTYPE_FULL)
		server->state |= STATE_PENDINGFULLQUERY;
	else if (querytype == QTYPE_ICMP)
		server->state |= STATE_PENDINGICMPQUERY;
	else
		return; 
		
	if (usequerychallenge && (querytype == QTYPE_FULL || querytype == QTYPE_BASIC))
		server->state |= STATE_PENDINGQUERYCHALLENGE;
	
	if (engine->querylist.count < engine->maxupdates) 
	{
		QEStartQuery(engine, server);
		return;
	}
	
	if (addfront)
		FIFOAddFront(&engine->pendinglist, server);
	else
		FIFOAddRear(&engine->pendinglist, server);
}

SBServer SBQueryEngineUpdateServerByIP(SBQueryEngine *engine, const char *ip, unsigned short queryport, int addfront, int querytype, SBBool usequerychallenge)
{
	SBServer server;
	goa_uint32 ipaddr;
	ipaddr = inet_addr(ip);
	server = SBAllocServer(NULL, ipaddr, htons(queryport));
	server->flags = UNSOLICITED_UDP_FLAG; 
	SBQueryEngineUpdateServer(engine, server, addfront, querytype, usequerychallenge);
	return server;
}


static void ParseSingleQR2Reply(SBQueryEngine *engine, SBServer server, char *data, int len)
{
	int i;
	int dlen;

	if (data[0] != 0x00 && data[0] != 0x09)
		return;

	data += 5;
	len -= 5;
	if (server->state & STATE_PENDINGQUERYCHALLENGE)
	{
		server->state &= (unsigned char)~(STATE_PENDINGQUERYCHALLENGE);

		if (len > 0)
		{
			server->querychallenge = (gsi_u32)atoi(data);
			FIFORemove(&engine->querylist, server); 
			QEStartQuery(engine, server); 
			engine->ListCallback(engine, qe_challengereceived, server, engine->instance);
			return;
		}
	}
	else if (server->state & STATE_PENDINGBASICQUERY)
	{
		for (i = 0 ; i < engine->numserverkeys ; i++)
		{
			dlen = NTSLengthSB(data, len);
			if (dlen < 0)
				break;
			if (!qr2_internal_is_master_only_key(qr2_registered_key_list[engine->serverkeys[i]]))
			{
				SBServerAddKeyValue(server, qr2_registered_key_list[engine->serverkeys[i]], data);
			}
			data += dlen;
			len -= dlen;
		}
		server->state |= STATE_BASICKEYS|STATE_VALIDPING;
	}
	else 
	{
		if (*data && strncmp("splitnum", data, 8)==0)
		{
			SBServerParseQR2FullKeysSplit(server, data, len);
			if (server->splitResponseBitmap != 0xFF)
				return;
			server->state |= STATE_FULLKEYS|STATE_BASICKEYS|STATE_VALIDPING;
		}
		else
		{
			SBServerParseQR2FullKeysSingle(server, data, len);
			server->state |= STATE_FULLKEYS|STATE_BASICKEYS|STATE_VALIDPING;
		}
	}
	server->state &= (unsigned char)~(STATE_PENDINGBASICQUERY|STATE_PENDINGFULLQUERY);
	server->updatetime = current_time() - server->updatetime;
	FIFORemove(&engine->querylist, server);
	engine->ListCallback(engine, qe_updatesuccess, server, engine->instance);
}

static void ParseSingleGOAReply(SBQueryEngine *engine, SBServer server, char *data, int len)
{
	int isfinal;
	isfinal = (strstr(data,"\\final\\") != NULL);
	SBServerParseKeyVals(server, data);
	if (isfinal)
	{
		if (server->state & STATE_PENDINGBASICQUERY)
			server->state |= STATE_BASICKEYS|STATE_VALIDPING;
		else
			server->state |= STATE_FULLKEYS|STATE_VALIDPING;
		server->state &= (unsigned char)~(STATE_PENDINGBASICQUERY|STATE_PENDINGFULLQUERY);
		server->updatetime = current_time() - server->updatetime;
		FIFORemove(&engine->querylist, server);
		engine->ListCallback(engine, qe_updatesuccess, server, engine->instance);
	}
	
	GSI_UNUSED(len);
}

static SBBool ParseSingleICMPReply(SBQueryEngine *engine, SBServer server, char *data, int len)
{
#ifdef SB_ICMP_SUPPORT
	SBIPHeader *ipheader = (SBIPHeader *)data;
	SBICMPHeader *icmpheader;
	int ipheaderlen;
	goa_uint32 packetpublicip;
	unsigned short packetpublicport;
	ipheaderlen = (gsi_u8)(ipheader->ip_hl_ver & 15);
	ipheaderlen *= 4;
	icmpheader = (SBICMPHeader *)(data + ipheaderlen);
	if (icmpheader->type != SB_ICMP_ECHO_REPLY)
		return SBFalse;
	if (icmpheader->un.idseq != server->updatetime)
		return SBFalse;
	if (len < ipheaderlen + (int)sizeof(SBICMPHeader) + 6)
		return SBFalse; 
	memcpy(&packetpublicip, data + ipheaderlen + sizeof(SBICMPHeader), 4);
	memcpy(&packetpublicport, data + ipheaderlen + sizeof(SBICMPHeader) + 4, 2);
	if (packetpublicport != server->publicport || packetpublicip != server->publicip)
		return SBFalse;
	server->updatetime = current_time() - server->updatetime;
	server->state |= STATE_VALIDPING;
	server->state &= (unsigned char)~(STATE_PENDINGICMPQUERY);	
	FIFORemove(&engine->querylist, server);
	engine->ListCallback(engine, qe_updatesuccess, server, engine->instance);
#else
	GSI_UNUSED(engine);
	GSI_UNUSED(server);
	GSI_UNUSED(data);
	GSI_UNUSED(len);
#endif
	return SBTrue;
	
}

#if defined(SN_SYSTEMS) && defined(SB_ICMP_SUPPORT)
static void ProcessIncomingICMPReplies(SBQueryEngine *engine)
{
	SBServer server;
	int result = 0;
	int found  = 0;
	int i      = 0;
	sndev_stat_ping_times_type optval;
	gsi_i32 optsize = sizeof(optval);

	result = sndev_get_status(0, SN_DEV_STAT_PING_TIMES, (void*)&optval, &optsize);
	if (result != 0)
	{
		gsDebugFormat(GSIDebugCat_SB, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Failed on sndev_get_status (checking ICMP pings): %d\r\n", result);
		return;
	}
	if (optval.num_entries == 0)
		return; 

	for (server = engine->querylist.first; server != NULL; server = server->next)
	{
		if ((server->state & STATE_PENDINGICMPQUERY) == 0 ||
			(server->flags & ICMP_IP_FLAG) == 0)
			continue; 

		for (i=0; i<optval.num_entries; i++)
		{
			if (server->icmpip == optval.times[i].ip_addr)
			{
				if (optval.times[i].status == SN_PING_TIMES_CODE_GOTREPLY)
				{
					server->updatetime = optval.times[i].time_ms;
					server->state |= STATE_VALIDPING;
					server->state &= (unsigned char)~(STATE_PENDINGICMPQUERY);
					FIFORemove(&engine->querylist, server);
					engine->ListCallback(engine, qe_updatesuccess, server, engine->instance);
				}

				found++;
				if (found == optval.num_entries)
					return; 
			}
		}
	}
}
#endif 

static void ProcessIncomingReplies(SBQueryEngine *engine, SBBool icmpSocket)
{
	int i;
	char indata[MAX_RECVFROM_SIZE]; 
	struct sockaddr_in saddr;
	int saddrlen = sizeof(saddr);
	SBServer server;
	SOCKET recvSock = 0;

	if (icmpSocket)
	{
		#ifdef SB_ICMP_SUPPORT
			#if defined(SN_SYSTEMS) 
				ProcessIncomingICMPReplies(engine);
				return;
			#else
				recvSock = engine->icmpsock;
			#endif
		#endif
	}
	else
	{
		recvSock = engine->querysock;
	}
	
	while(CanReceiveOnSocket(recvSock))
	{
		i = (int)recvfrom(recvSock, indata, sizeof(indata) - 1, 0, (struct sockaddr *)&saddr, &saddrlen);

		if (gsiSocketIsError(i))
			break;
		indata[i] = 0;
		for (server = engine->querylist.first ; server != NULL ; server = server->next)
		{
			if ((icmpSocket && (server->flags & ICMP_IP_FLAG) && server->icmpip == saddr.sin_addr.s_addr) || 
				(server->publicip == saddr.sin_addr.s_addr && (server->publicport == saddr.sin_port || icmpSocket)) || 
				(server->publicip == engine->mypublicip && (server->flags & PRIVATE_IP_FLAG) && server->privateip == saddr.sin_addr.s_addr && server->privateport == saddr.sin_port)) 
			{
				if (icmpSocket)
				{
					if (ParseSingleICMPReply(engine, server, indata, i))
						break; 
				} else
				{
					if (engine->queryversion == QVERSION_QR2)
						ParseSingleQR2Reply(engine, server, indata, i);
					else
						ParseSingleGOAReply(engine, server, indata, i);
					break;
				}
			}
		}		
	}


}

static void TimeoutOldQueries(SBQueryEngine *engine)
{
	gsi_time ctime = current_time();
	while (engine->querylist.first != NULL)
	{
		if (ctime > engine->querylist.first->updatetime + MAX_QUERY_MSEC)
		{
			engine->querylist.first->flags |= STATE_QUERYFAILED;
			engine->querylist.first->updatetime = MAX_QUERY_MSEC;
			engine->querylist.first->flags  &= (unsigned char)~(STATE_PENDINGBASICQUERY|STATE_PENDINGFULLQUERY|STATE_PENDINGICMPQUERY);
			engine->ListCallback(engine, qe_updatefailed, engine->querylist.first, engine->instance);			
			FIFOGetFirst(&engine->querylist);
		} else
			break; 
	}
}

static void QueueNextQueries(SBQueryEngine *engine)
{
	while (engine->querylist.count < engine->maxupdates && engine->pendinglist.count > 0)
	{
		SBServer server = FIFOGetFirst(&engine->pendinglist);
		QEStartQuery(engine, server);
	}
}

void SBQueryEngineThink(SBQueryEngine *engine)
{
	if (engine->querylist.count == 0) 
		return;
	ProcessIncomingReplies(engine, SBFalse);
#ifdef SB_ICMP_SUPPORT
	ProcessIncomingReplies(engine, SBTrue);
#endif
	TimeoutOldQueries(engine);
	if (engine->pendinglist.count > 0)
		QueueNextQueries(engine);
	if (engine->querylist.count == 0) 
		engine->ListCallback(engine, qe_engineidle, NULL, engine->instance);
}

void SBQueryEngineAddQueryKey(SBQueryEngine *engine, unsigned char keyid)
{
	if (engine->numserverkeys < MAX_QUERY_KEYS)
		engine->serverkeys[engine->numserverkeys++] = keyid;
}


void SBQueryEngineRemoveServerFromFIFOs(SBQueryEngine *engine, SBServer server)
{
	SBBool ret;

	ret = FIFORemove(&engine->querylist, server);
	if(ret)
		return; 
	FIFORemove(&engine->pendinglist, server);
}
