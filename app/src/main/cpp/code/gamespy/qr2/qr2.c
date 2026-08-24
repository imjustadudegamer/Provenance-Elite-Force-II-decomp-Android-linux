/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "../common/gsCommon.h"
#include "../common/gsAvailable.h"
#include "qr2.h"
#include "qr2regkeys.h"
#include "../natneg/natneg.h"

#ifdef __cplusplus
extern "C" {
#endif	
	
#ifdef __MWERKS__ 
qr2_error_t qr2_initW(qr2_t *qrec, const unsigned short *ip, int baseport, const unsigned short *gamename, 
					 const unsigned short *secret_key, int ispublic, int natnegotiate, qr2_serverkeycallback_t server_key_callback, 
					 qr2_playerteamkeycallback_t player_key_callback, qr2_playerteamkeycallback_t team_key_callback, qr2_keylistcallback_t key_list_callback, 
					 qr2_countcallback_t playerteam_count_callback, qr2_adderrorcallback_t adderror_callback, void *userdata);
qr2_error_t qr2_init_socketW(qr2_t *qrec, SOCKET s, int boundport, const unsigned short *gamename, 
					 const unsigned short *secret_key, int ispublic, int natnegotiate, qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback, qr2_playerteamkeycallback_t team_key_callback, qr2_keylistcallback_t key_list_callback, 
					 qr2_countcallback_t playerteam_count_callback, qr2_adderrorcallback_t adderror_callback, void *userdata);
qr2_error_t qr2_init_socketA(qr2_t *qrec, SOCKET s, int boundport, const char *gamename, const char *secret_key,
					 int ispublic, int natnegotiate, qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback, qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback, qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback, void *userdata);
qr2_error_t qr2_initA(qr2_t *qrec, const char *ip, int baseport, const char *gamename, const char *secret_key,
					 int ispublic, int natnegotiate, qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback, qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback, qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback, void *userdata);
#endif

#define MASTER_PORT 27900
#define MASTER_ADDR "master." GSI_DOMAIN_NAME
#define FIRST_HB_TIME 10000 
#define HB_TIME 60000 
#define KA_TIME 20000 
#define MIN_STATECHANGED_HB_TIME 10000 
#define MAX_FIRST_COUNT 4 
#define MAX_DATA_SIZE 1400
#define INBUF_LEN 256
#define PUBLIC_ADDR_LEN 12
#define QR2_OPTION_USE_QUERY_CHALLENGE 128
	
#define PACKET_QUERY              0x00
#define PACKET_CHALLENGE          0x01
#define PACKET_ECHO               0x02
#define PACKET_ECHO_RESPONSE      0x05  
#define PACKET_HEARTBEAT          0x03
#define PACKET_ADDERROR           0x04
#define PACKET_CLIENT_MESSAGE     0x06
#define PACKET_CLIENT_MESSAGE_ACK 0x07
#define PACKET_KEEPALIVE          0x08
#define PACKET_PREQUERY_IP_VERIFY 0x09

	 
#define MAX_LOCAL_IP 5

#define NATNEG_MAGIC_LEN 6
#define NN_MAGIC_0 0xFD
#define NN_MAGIC_1 0xFC
#define NN_MAGIC_2 0x1E
#define NN_MAGIC_3 0x66
#define NN_MAGIC_4 0x6A
#define NN_MAGIC_5 0xB2

#define QR2_EXFLAG_SPLIT		(1<<0)

#define QR2_SPLITNUM_MAX		7
#define QR2_SPLITNUM_FINALFLAG	(1<<7)


typedef unsigned char uchar;

struct qr2_keybuffer_s
{
	uchar keys[MAX_REGISTERED_KEYS];
	int numkeys;
};

struct qr2_buffer_s
{
	char buffer[MAX_DATA_SIZE];
	int len;
};

#define AVAILABLE_BUFFER_LEN(a) (MAX_DATA_SIZE - (a)->len)

struct qr2_implementation_s static_qr2_rec = {INVALID_SOCKET};
static qr2_t current_rec = &static_qr2_rec;
char qr2_hostname[64];

static int num_local_ips = 0;
static struct in_addr local_ip_list[MAX_LOCAL_IP];


static void send_heartbeat(qr2_t qrec, int statechanged);
static void send_keepalive(qr2_t qrec);
static int get_sockaddrin(const char *host, int port, struct sockaddr_in *saddr, struct hostent **savehent);
static void qr2_check_queries(qr2_t qrec);
static void qr2_check_send_heartbeat(qr2_t qrec);
static void enum_local_ips();
static void qr2_expire_ip_verify(qr2_t qrec);
qr2_error_t qr2_create_socket(SOCKET *sock, const char *ip, int * port);


qr2_error_t qr2_init_socketA(qr2_t *qrec, SOCKET s, int boundport, const char *gamename, const char *secret_key,
					 int ispublic, int natnegotiate,
					 qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback,
					 qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback,
					 qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback,
					 void *userdata)
{
	char hostname[64];
	int ret;
	int i;
	qr2_t cr;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_init_socket()\r\n");

	if (qrec == NULL)
	{
		cr = &static_qr2_rec;		
	}
	else
	{
		*qrec = (qr2_t)gsimalloc(sizeof(struct qr2_implementation_s));
		cr = *qrec;
	}
	srand((unsigned int)current_time());
	strcpy(cr->gamename,gamename);
	strcpy(cr->secret_key,secret_key);
	cr->qport = boundport;
	cr->lastheartbeat = 0;
	cr->lastka = 0;
	cr->hbsock = s;
	cr->listed_state = 1;
	cr->udata = userdata;
	cr->server_key_callback = server_key_callback;
	cr->player_key_callback = player_key_callback;
	cr->team_key_callback = team_key_callback;
	cr->key_list_callback = key_list_callback;
	cr->playerteam_count_callback = playerteam_count_callback;
	cr->adderror_callback = adderror_callback;
	cr->nn_callback = NULL;
	cr->cm_callback = NULL;
	cr->cdkeyprocess = NULL;
	cr->ispublic = ispublic;
	cr->read_socket = 0;
	cr->nat_negotiate = natnegotiate;
	cr->publicip = 0;
	cr->publicport = 0;
	cr->pa_callback = NULL;
	cr->cc_callback = NULL;
	cr->userstatechangerequested = 0;
	cr->backendoptions = 0;

	for (i = 0 ; i < REQUEST_KEY_LEN ; i++)
		cr->instance_key[i] = (char)(rand() % 0xFF);
	for (i = 0 ; i < RECENT_CLIENT_MESSAGES_TO_TRACK ; i++)
		cr->client_message_keys[i] = -1;
	cr->cur_message_key = 0;

	memset(cr->ipverify, 0, sizeof(cr->ipverify));
	
	enum_local_ips();

	if (ispublic)
	{
		int override = qr2_hostname[0];
		if(!override)
			sprintf(hostname, "%s.master." GSI_DOMAIN_NAME, gamename);
		ret = get_sockaddrin(override?qr2_hostname:hostname, MASTER_PORT, &(cr->hbaddr), NULL);

		if (ret == 1)
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
				"%s resolved to %s\r\n", override?qr2_hostname:hostname, inet_ntoa(cr->hbaddr.sin_addr));
		}
		else
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_HotError,
				"Failed on DNS lookup for %s \r\n", override?qr2_hostname:hostname);
		}
	}
	else 
		ret = 1;
	if (!ret)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_HotError,
			"qr2_init_socket() returned failed (DNS error)\r\n");
		return e_qrdnserror;
	}
	else
	{
		return e_qrnoerror;
	}


}

qr2_error_t qr2_init_socketW(qr2_t *qrec, SOCKET s, int boundport, const unsigned short *gamename, const unsigned short *secret_key,
					 int ispublic, int natnegotiate,
					 qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback,
					 qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback,
					 qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback,
					 void *userdata)
{
	char gamename_A[255];
	char secretkey_A[255];

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_init_socketW()\r\n");

	UCS2ToAsciiString(gamename, gamename_A);
	UCS2ToAsciiString(secret_key, secretkey_A);

	return qr2_init_socketA(qrec, s, boundport, gamename_A, secretkey_A, ispublic, natnegotiate,
							server_key_callback, player_key_callback, team_key_callback,
							key_list_callback, playerteam_count_callback, adderror_callback, userdata);
}

qr2_error_t qr2_create_socket(SOCKET *sock, const char *ip, int * port)
{
	struct sockaddr_in saddr;	
	SOCKET hbsock;
	int maxport;
	int lasterror = 0;
	int baseport = *port;

#if defined(_LINUX)
	unsigned int saddrlen;
#else
	int saddrlen;
#endif

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_create_socket()\r\n");

	SocketStartUp();
	
	hbsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (INVALID_SOCKET == hbsock)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_HotError,
			"Failed to create heartbeat socket\r\n");
		return e_qrwsockerror;
	}
	
	maxport = baseport + NUM_PORTS_TO_TRY;
	while (baseport < maxport)
	{
		get_sockaddrin(ip,baseport,&saddr,NULL);
		if (saddr.sin_addr.s_addr == htonl(0x7F000001)) 
			saddr.sin_addr.s_addr = INADDR_ANY;
		
		lasterror = bind(hbsock, (struct sockaddr *)&saddr, sizeof(saddr));
		if (lasterror == 0)
			break; 
		baseport++;
	}
	
	if (lasterror != 0) 
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_HotError,
			"Failed to bind() query socket\r\n");
		return e_qrbinderror;
	}
	
	if (baseport == 0) 
	{
		saddrlen = sizeof(saddr);

		lasterror = getsockname(hbsock,(struct sockaddr *)&saddr, &saddrlen);

		if (lasterror)
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_HotError,
				"Query socket bind() success, but getsockname() failed\r\n");
			return e_qrbinderror;
		}
		baseport = ntohs(saddr.sin_port);
	}

	*sock = hbsock;
	*port = baseport;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
		"Query socket created and bound to port %d\r\n", *port);

	return e_qrnoerror;
}

qr2_error_t qr2_initA(qr2_t *qrec, const char *ip, int baseport, const char *gamename, const char *secret_key,
					 int ispublic, int natnegotiate,
					 qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback,
					 qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback,
					 qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback,
					 void *userdata)
{
	SOCKET hbsock;
	qr2_error_t ret;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_init()\r\n");

	ret = qr2_create_socket(&hbsock, ip, &baseport);
	if(ret != e_qrnoerror)
	{
		SocketShutDown();
		return ret;
	}

	ret = qr2_init_socketA(qrec, hbsock, baseport, gamename, secret_key, ispublic, natnegotiate, server_key_callback, player_key_callback, team_key_callback, key_list_callback, playerteam_count_callback, adderror_callback,  userdata);
	if (qrec == NULL)
		qrec = &current_rec;
	(*qrec)->read_socket = 1;
	
	return ret;
}

qr2_error_t qr2_initW(qr2_t *qrec, const unsigned short *ip, int baseport, const unsigned short *gamename, const unsigned short *secret_key,
					 int ispublic, int natnegotiate,
					 qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback,
					 qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback,
					 qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback,
					 void *userdata)
{
	char ip_A[255];
	char gamename_A[255];
	char secretkey_A[255];

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_initW()\r\n");

	if (ip != NULL) 
		UCS2ToAsciiString(ip, ip_A);
	UCS2ToAsciiString(gamename, gamename_A);
	UCS2ToAsciiString(secret_key, secretkey_A);

	return qr2_initA(qrec, (ip!=NULL)?ip_A:NULL, baseport, gamename_A, secretkey_A, ispublic, natnegotiate, server_key_callback, player_key_callback, team_key_callback, key_list_callback, playerteam_count_callback, adderror_callback,  userdata);
}

void qr2_register_natneg_callback(qr2_t qrec, qr2_natnegcallback_t nncallback)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_natneg_callback()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	qrec->nn_callback = nncallback;

}
void qr2_register_clientmessage_callback(qr2_t qrec, qr2_clientmessagecallback_t cmcallback)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_clientmessage_callback()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	qrec->cm_callback = cmcallback;
}
void qr2_register_publicaddress_callback(qr2_t qrec, qr2_publicaddresscallback_t pacallback)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_publicaddress_callback()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	qrec->pa_callback = pacallback;
}
void qr2_register_clientconnected_callback(qr2_t qrec, qr2_clientconnectedcallback_t cccallback)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_clientconnected_callback()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	qrec->cc_callback = cccallback;
}

void qr2_register_denyresponsetoip_callback(qr2_t qrec, qr2_denyqr2responsetoipcallback_t dertoipcallback)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_denyresponsetoip_callback()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	qrec->denyresp2_ip_callback = dertoipcallback;
}


void qr2_think(qr2_t qrec)
{
	if (qrec == NULL)
		qrec = current_rec;
	if (qrec->ispublic)
		qr2_check_send_heartbeat(qrec);
	qr2_check_queries(qrec);
	qr2_expire_ip_verify(qrec);
	NNThink();
}

void qr2_check_queries(qr2_t qrec)
{
	static char indata[INBUF_LEN]; 
	struct sockaddr_in saddr;
	int error;

#if defined(_LINUX)
	unsigned int saddrlen = sizeof(struct sockaddr_in);
#else
	int saddrlen = sizeof(struct sockaddr_in);
#endif

	if (!qrec->read_socket)
		return; 

	while(CanReceiveOnSocket(qrec->hbsock))
	{
		error = (int)recvfrom(qrec->hbsock, indata, (INBUF_LEN - 1), 0, (struct sockaddr *)&saddr, &saddrlen);
		
		if (gsiSocketIsNotError(error))
		{
			indata[error] = '\0';

			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
				"Received %d bytes on query socket\r\n", error);
			gsDebugBinary(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_RawDump,
				indata, error);

			qr2_parse_queryA(qrec, indata, error, (struct sockaddr *)&saddr);
		}
		else if (error == 0)
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
				"CanReceiveOnSocket() returned true, but recvfrom return 0!\r\n", error);
		}
		else
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
				"CanReceiveOnSocket() returned true, but recvfrom failed!\r\n", error);
		}
	}
}

void qr2_check_send_heartbeat(qr2_t qrec)
{
	gsi_time tc = current_time();

	if (INVALID_SOCKET == qrec->hbsock)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"HBSock is invalid\r\n");
		return; 
	}

	if (qrec->listed_state > 0 && tc - qrec->lastheartbeat > FIRST_HB_TIME) 
	{ 
		if (qrec->listed_state >= MAX_FIRST_COUNT)
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_HotError,
				"No response from master, generating NoChallengeResponse error\r\n");

			qrec->listed_state = 0; 
#ifndef GSI_UNICODE
			qrec->adderror_callback(e_qrnochallengeerror, "No challenge value was received from the master server.", qrec->udata);
#else
			qrec->adderror_callback(e_qrnochallengeerror, L"No challenge value was received from the master server.", qrec->udata);
#endif
			return;
		} else
		{
			send_heartbeat(qrec, 3);
			qrec->listed_state++;
		}
	}
	else if (qrec->userstatechangerequested && (tc - qrec->lastheartbeat > MIN_STATECHANGED_HB_TIME))
		send_heartbeat(qrec,1);  
	else if (tc - qrec->lastheartbeat > HB_TIME || qrec->lastheartbeat == 0 || tc < qrec->lastheartbeat)
		send_heartbeat(qrec,0);  
	
	if (current_time() - qrec->lastka > KA_TIME) 
		send_keepalive(qrec);
}

void qr2_send_statechanged(qr2_t qrec)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_send_statechanged()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	if (!qrec->ispublic)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_Warning,
			"Requested send statechange for LAN game, discarding\r\n");
		return;
	}
	if (current_time() - qrec->lastheartbeat < MIN_STATECHANGED_HB_TIME)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_Notice,
			"Queing statechange for later send (too soon)\r\n");

		qrec->userstatechangerequested = 1;
		return;  
	}

	send_heartbeat(qrec, 1);
	qrec->userstatechangerequested = 0; 
}


void qr2_shutdown(qr2_t qrec)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_StackTrace,
		"qr2_shutdown()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	if (qrec->ispublic)
		send_heartbeat(qrec, 2);
	if (INVALID_SOCKET != qrec->hbsock && qrec->read_socket) 
	{
		closesocket(qrec->hbsock);
	}
	qrec->hbsock = INVALID_SOCKET;
	qrec->lastheartbeat = 0;
	if(qrec->read_socket) 
	{
		SocketShutDown();
	}
	if (qrec != &static_qr2_rec) 
	{
		gsifree(qrec);
	}

    NNFreeNegotiateList(); 


}


gsi_bool qr2_keybuffer_add(qr2_keybuffer_t keybuffer, int keyid)
{
	if (keybuffer->numkeys >= MAX_REGISTERED_KEYS)
		return gsi_false;
	if (keyid < 1 || keyid > MAX_REGISTERED_KEYS)
		return gsi_false;

	keybuffer->keys[keybuffer->numkeys++] = (uchar)keyid;
	return gsi_true;
}

gsi_bool qr2_buffer_add_int(qr2_buffer_t outbuf, int value)
{
	char temp[20];
	sprintf(temp, "%d", value);
	return qr2_buffer_addA(outbuf, temp);
}

gsi_bool qr2_buffer_addA(qr2_buffer_t outbuf, const char *value)
{
	GS_ASSERT(outbuf)
	GS_ASSERT(value)
	{
		int copylen;
		copylen = (int)strlen(value) + 1;
		if (copylen > AVAILABLE_BUFFER_LEN(outbuf))
			copylen = AVAILABLE_BUFFER_LEN(outbuf); 
		if (copylen <= 0)
			return gsi_false; 
		memcpy(outbuf->buffer + outbuf->len, value, (unsigned int)copylen);
		outbuf->len += copylen;
		outbuf->buffer[outbuf->len - 1] = 0; 
		return gsi_true;
	}
}
#if defined(GSI_UNICODE)
gsi_bool qr2_buffer_addW(qr2_buffer_t outbuf, const unsigned short *value)
{
	char value_A[4096];
	UCS2ToUTF8String(value, value_A);
	return qr2_buffer_addA(outbuf, value_A);
}
#endif


static void enum_local_ips()
{
	struct hostent *phost;
	phost = getlocalhost();
	if (phost == NULL)
		return;
	for (num_local_ips = 0 ; num_local_ips < MAX_LOCAL_IP ; num_local_ips++)
	{
		if (phost->h_addr_list[num_local_ips] == 0)
			break;
		memcpy(&local_ip_list[num_local_ips], phost->h_addr_list[num_local_ips], sizeof(struct in_addr));
	}
}



static int get_sockaddrin(const char *host, int port, struct sockaddr_in *saddr, struct hostent **savehent)
{
	struct hostent *hent = NULL;

	saddr->sin_family = AF_INET;
	saddr->sin_port = htons((unsigned short)port);
	if (host == NULL)
		saddr->sin_addr.s_addr = INADDR_ANY;
	else
		saddr->sin_addr.s_addr = inet_addr(host);
	
	if (saddr->sin_addr.s_addr == INADDR_NONE && strcmp(host,"255.255.255.255") != 0)
	{
		hent = gethostbyname(host);
		if (!hent)
			return 0;
		saddr->sin_addr.s_addr = *(unsigned int *)hent->h_addr_list[0];
	}
	if (savehent != NULL)
		*savehent = hent;
	return 1;

} 




static void swap_byte ( uchar *a, uchar *b )
{
	uchar swapByte; 
	
	swapByte = *a; 
	*a = *b;      
	*b = swapByte;
}

static uchar encode_ct ( uchar c )
{
	if (c <  26) return (uchar)('A'+c);
	if (c <  52) return (uchar)('a'+c-26);
	if (c <  62) return (uchar)('0'+c-52);
	if (c == 62) return (uchar)('+');
	if (c == 63) return (uchar)('/');
	
	return 0;
}

static void gs_encode ( uchar *ins, int size, uchar *result )
{
	int    i,pos;
	uchar  trip[3];
	uchar  kwart[4];
	
	i=0;
	while (i < size)
	{
		for (pos=0 ; pos <= 2 ; pos++, i++)
			if (i < size) trip[pos] = *ins++;
			else trip[pos] = '\0';
			kwart[0] = (unsigned char)(  (trip[0])       >> 2);
			kwart[1] = (unsigned char)((((trip[0]) &  3) << 4) + ((trip[1]) >> 4));
			kwart[2] = (unsigned char)((((trip[1]) & 15) << 2) + ((trip[2]) >> 6));
			kwart[3] = (unsigned char)(  (trip[2]) & 63);
			for (pos=0; pos <= 3; pos++) *result++ = encode_ct(kwart[pos]);
	}
	*result='\0';
}

static void gs_encrypt ( uchar *key, int key_len, uchar *buffer_ptr, int buffer_len )
{ 
	int counter;     
	uchar x, y, xorIndex;
	uchar state[256];       
	
	for ( counter = 0; counter < 256; counter++) state[counter] = (uchar) counter;
	
	x = 0; y = 0;
	for ( counter = 0; counter < 256; counter++)
	{
		y = (uchar)((key[x] + state[counter] + y) % 256);
		x = (uchar)((x + 1) % key_len);
		swap_byte ( &state[counter], &state[y] );
	}
	
	x = 0; y = 0;
	for ( counter = 0; counter < buffer_len; counter ++)
	{
		x = (uchar)((x + buffer_ptr[counter] + 1) % 256);
		y = (uchar)((state[x] + y) % 256);
		swap_byte ( &state[x], &state[y] );
		xorIndex = (uchar)((state[x] + state[y]) % 256);
		buffer_ptr[counter] ^= state[xorIndex];
	}
}


static void NatNegProgressCallback(NegotiateState state, void *userdata)
{
	GSI_UNUSED(state);
	GSI_UNUSED(userdata);
}

static void NatNegCompletedCallback(NegotiateResult result, SOCKET gamesocket, struct sockaddr_in *remoteaddr, void *userdata)
{
	qr2_t qrec = (qr2_t)userdata;

	if(qrec->cc_callback)
	{
		if(result == nr_success)
		{
			qrec->cc_callback(gamesocket, remoteaddr, qrec->udata);
		}
	}
}



static void qr_add_packet_header(qr2_buffer_t buf, char ptype, char *reqkey)
{
	buf->buffer[0] = ptype;
	memcpy(buf->buffer + 1, reqkey, REQUEST_KEY_LEN);
	buf->len = REQUEST_KEY_LEN  + 1;
}

#define MAX_CHALLENGE 64
static void compute_challenge_response(qr2_t qrec, qr2_buffer_t buf, char *challenge, int challengelen)
{
	char encrypted_val[MAX_CHALLENGE + 1]; 
	
	if(challengelen < 1)
		return; 
	if (challengelen > (MAX_CHALLENGE + 1))
		return; 
	if (challenge[challengelen - 1] != 0)
		return; 
	
	strcpy(encrypted_val, challenge);
	gs_encrypt((uchar *)qrec->secret_key, (int)strlen(qrec->secret_key), (uchar *)encrypted_val, challengelen - 1);
	gs_encode((uchar *)encrypted_val,challengelen - 1, (uchar *)(buf->buffer + buf->len));
	buf->len += (int)strlen(buf->buffer + buf->len) + 1;
	
}

static void handle_public_address(qr2_t qrec, char * buffer)
{
	unsigned int ip;
	unsigned int portTemp;
	unsigned short port;

	sscanf(buffer, "%08X%04X", &ip, &portTemp);
	port = (unsigned short)portTemp;
	ip = htonl(ip);

	if((ip == 0) || (port == 0))
		return;

#ifdef GSI_COMMON_DEBUG
	{
		IN_ADDR addr;
		addr.s_addr = ip;
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Received public address (%s:%d)\r\n", inet_ntoa(addr), port);
	}
#endif

	if((qrec->publicip != ip) || (qrec->publicport != port))
	{
		qrec->publicip = ip;
		qrec->publicport = port;
		qrec->pa_callback(ip, port, qrec->udata);
	}
}

static void qr_build_partial_query_reply(qr2_t qrec, qr2_buffer_t buf, qr2_key_type keytype, int keycount, uchar *keys)
{
	struct qr2_keybuffer_s kb;
	int playerteamcount;
	unsigned short cttemp;
	int i;
	int pindex;
	const char *k;
	int len;

	kb.numkeys = 0;
	if (keycount == 0)
		return; 
	
	if (keytype == key_player || keytype == key_team) 
	{
		if (AVAILABLE_BUFFER_LEN(buf) < sizeof(cttemp))
			return; 
		playerteamcount = qrec->playerteam_count_callback(keytype, qrec->udata);
		cttemp = htons((unsigned short)playerteamcount);
		memcpy(buf->buffer + buf->len, &cttemp, sizeof(cttemp));
		buf->len += sizeof(cttemp);
	} else
		playerteamcount = 1;

	if (keycount == 0xFF) 
	{
		qrec->key_list_callback(keytype, &kb, qrec->udata);
		for (i = 0 ; i < kb.numkeys ; i++)
		{
			k = qr2_registered_key_list[kb.keys[i]];
			if (k == NULL)
				k = "unknown";
			qr2_buffer_addA(buf, k);
			if (keytype == key_server) 
			{
				len = buf->len;
				qrec->server_key_callback(kb.keys[i], buf, qrec->udata);
				if(len == buf->len)
					qr2_buffer_addA(buf, "");
			}
		}
		if (AVAILABLE_BUFFER_LEN(buf) < 1)
			return; 
		buf->buffer[buf->len++] = 0;
		keycount = kb.numkeys;
		keys = kb.keys;
		if (keytype == key_server)
			return;	
	}
	for (pindex = 0 ; pindex < playerteamcount ; pindex++)
	{
		for (i = 0 ; i < keycount ; i++)
		{
			len = buf->len;
			if (keytype == key_server) 
				qrec->server_key_callback(keys[i], buf, qrec->udata);
			else if (keytype == key_player)
				qrec->player_key_callback(keys[i], pindex, buf, qrec->udata);
			else if (keytype == key_team)
				qrec->team_key_callback(keys[i], pindex, buf, qrec->udata);
			if(len == buf->len)
				qr2_buffer_addA(buf, "");
		}
	}		
}

static void qr_build_query_reply(qr2_t qrec, qr2_buffer_t buf, int serverkeycount, uchar *serverkeys, int playerkeycount, uchar *playerkeys, int teamkeycount, uchar *teamkeys)
{
	qr_build_partial_query_reply(qrec, buf, key_server, serverkeycount, serverkeys);
	qr_build_partial_query_reply(qrec, buf, key_player, playerkeycount, playerkeys);
	qr_build_partial_query_reply(qrec, buf, key_team, teamkeycount, teamkeys);
}


struct QRSplitQueryProgress
{
	qr2_key_type mCurKeyType;
	int mCurPacketNum;
	int mCurKeyIndex;		
	int mCurSubCount;       
	int mCurSubIndex;		
	struct qr2_keybuffer_s mKeyBuffer; 
};


static gsi_bool qr_build_split_query_reply(qr2_t qrec, qr2_buffer_t buf, struct QRSplitQueryProgress* progress)
{
	unsigned char* packetNumPos = NULL; 

	if (progress->mCurKeyType >= key_type_count)
		return gsi_false; 

	if (AVAILABLE_BUFFER_LEN(buf) < 32)
		return gsi_false; 

	qr2_buffer_addA(buf, "splitnum");
	packetNumPos = (unsigned char*)&buf->buffer[buf->len++];
	*packetNumPos = (gsi_u8)progress->mCurPacketNum++;

	while (progress->mCurKeyType < key_type_count)
	{
		if (progress->mKeyBuffer.numkeys == 0)
			qrec->key_list_callback(progress->mCurKeyType, &progress->mKeyBuffer, qrec->udata);

		if (progress->mCurSubCount == 0 && progress->mCurKeyType != key_server)
			progress->mCurSubCount = qrec->playerteam_count_callback(progress->mCurKeyType, qrec->udata);

		if (AVAILABLE_BUFFER_LEN(buf) < 100)
			return gsi_true; 

		buf->buffer[buf->len++] = (char)progress->mCurKeyType;

		while(progress->mCurKeyIndex < progress->mKeyBuffer.numkeys)
		{
			int aRegisteredKeyIndex = progress->mKeyBuffer.keys[progress->mCurKeyIndex];
			const char* aKeyName = qr2_registered_key_list[aRegisteredKeyIndex];

			if (gsi_is_false( qr2_buffer_addA(buf,aKeyName) ))
				return gsi_true; 

			if (progress->mCurKeyType == key_server)
			{
				qrec->server_key_callback(aRegisteredKeyIndex, buf, qrec->udata);

				if (AVAILABLE_BUFFER_LEN(buf) < 1)
					return gsi_true; 
			}
			else
			{
				if (AVAILABLE_BUFFER_LEN(buf) < 1)
					return gsi_true; 

				buf->buffer[buf->len++] = (char)progress->mCurSubIndex;

				while(progress->mCurSubIndex < progress->mCurSubCount)
				{
					if (progress->mCurKeyType == key_player)
						qrec->player_key_callback(aRegisteredKeyIndex, progress->mCurSubIndex, buf, qrec->udata);
					else if (progress->mCurKeyType == key_team)
						qrec->team_key_callback(aRegisteredKeyIndex, progress->mCurSubIndex, buf, qrec->udata);

					if (AVAILABLE_BUFFER_LEN(buf) < 1)
						return gsi_true; 
				
					progress->mCurSubIndex++;
				}
				if (AVAILABLE_BUFFER_LEN(buf) > 0)
					buf->buffer[buf->len++] = '\0';
			}
			progress->mCurKeyIndex++;
			progress->mCurSubIndex = 0;
		}

		if (AVAILABLE_BUFFER_LEN(buf) > 0)
			buf->buffer[buf->len++] = '\0';

		progress->mCurKeyType = (qr2_key_type)(progress->mCurKeyType + 1);
		progress->mCurKeyIndex = 0;
		progress->mCurSubCount = 0;
		progress->mCurSubIndex = 0;
		progress->mKeyBuffer.numkeys = 0;
	}

	*packetNumPos |= QR2_SPLITNUM_FINALFLAG;
	return gsi_true; 
}

static void qr_process_query(qr2_t qrec, qr2_buffer_t buf, uchar *qdata, int len, struct sockaddr* sender)
{
	uchar serverkeycount;
	uchar playerkeycount;
	uchar teamkeycount;
	uchar exflags = 0;

	uchar *serverkeys = NULL;
	uchar *playerkeys = NULL;
	uchar *teamkeys = NULL;
	if (len < 3)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Discarding invalid query (too short#1: %d bytes total)\r\n", len);
		return; 
	}
	serverkeycount = qdata[0];
	qdata++;
	len--;
	if (serverkeycount != 0 && serverkeycount != 0xFF)
	{
		serverkeys = qdata;
		qdata += serverkeycount;
		len -= serverkeycount;
	}
	if (len < 2)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Discarding invalid query (too short#2: %d bytes remain)\r\n", len);
		return; 
	}
	playerkeycount = qdata[0];
	qdata++;
	len--;
	if (playerkeycount != 0 && playerkeycount != 0xFF)
	{
		playerkeys = qdata;
		qdata += playerkeycount;
		len -= playerkeycount;
	}
	if (len < 1)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Discarding invalid query (too short#3: %d bytes remain)\r\n", len);
		return; 
	}
	teamkeycount = qdata[0];
	qdata++;
	len--;
	if (teamkeycount != 0 && teamkeycount != 0xFF)
	{
		teamkeys = qdata;
		qdata += teamkeycount;
		len -= teamkeycount;
	}
	if (len < 0)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Discarding invalid query (too short#4: %d bytes remain)\r\n", len);
		return; 
	}

	if (len > 0)
	{
		exflags = qdata[0];
		len--;
	}
	
	if ((exflags & QR2_EXFLAG_SPLIT)==QR2_EXFLAG_SPLIT)
	{
		struct QRSplitQueryProgress progress;
		progress.mCurPacketNum = 0;
		progress.mCurKeyType = key_server;
		progress.mCurKeyIndex = 0;
		progress.mCurSubCount = 0;
		progress.mCurSubIndex = 0;
		progress.mKeyBuffer.numkeys = 0;
			
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
			"Building query reply (split packet supported)\r\n");

		while (gsi_true == qr_build_split_query_reply(qrec, buf, &progress))
		{
			sendto(qrec->hbsock, buf->buffer, buf->len, 0, sender, sizeof(struct sockaddr_in));
			buf->len = 5; 
			if (progress.mCurPacketNum > QR2_SPLITNUM_MAX)
				return; 
		}

		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
				"Finished split query reply (%d packets)\r\n", progress.mCurPacketNum);
	}
	else
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
			"Building query reply (single packet)\r\n");
		qr_build_query_reply(qrec, buf, serverkeycount, serverkeys, playerkeycount, playerkeys, teamkeycount, teamkeys);
		sendto(qrec->hbsock, buf->buffer, buf->len, 0, sender, sizeof(struct sockaddr_in));
	}

	GSI_UNUSED(sender);
}



static void qr_process_client_message(qr2_t qrec, char *buf, int len)
{
	unsigned char natNegBytes[NATNEG_MAGIC_LEN] = {NN_MAGIC_0,NN_MAGIC_1,NN_MAGIC_2,NN_MAGIC_3,NN_MAGIC_4,NN_MAGIC_5};
	int i;
	int isnatneg = 1;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_Notice,
		"Processing client message\r\n");

	if (len >= NATNEG_MAGIC_LEN + 4)
	{
		for (i = 0 ; i < NATNEG_MAGIC_LEN ; i++)
			if ((unsigned char)buf[i] != natNegBytes[i])
			{
				isnatneg = 0;
				break;
			}
	} else
		isnatneg = 0;
	if (isnatneg)
	{
		int cookie;
		memcpy(&cookie, buf + NATNEG_MAGIC_LEN, 4);
		cookie = (int)ntohl((unsigned int)cookie);

		if(qrec->cc_callback)
		{
			NegotiateError error;

			if(__GSIACResult != GSIACAvailable)
			{
				__GSIACResult = GSIACAvailable;
				strcpy(__GSIACGamename, qrec->gamename);
			}

			error = NNBeginNegotiationWithSocket(qrec->hbsock, cookie, 1, NatNegProgressCallback, NatNegCompletedCallback, qrec);
			if(error != ne_noerror)
			{
			}
		}
		else if (qrec->nn_callback)
		{
			qrec->nn_callback(cookie, qrec->udata); 
		}
	} else
		if (qrec->cm_callback)
		{
#ifndef GSI_UNICODE
			qrec->cm_callback(buf, len, qrec->udata);
#else
			unsigned short* buf_W;
			buf_W = UTF8ToUCS2StringAlloc(buf);
			qrec->cm_callback(buf_W, wcslen(buf_W), qrec->udata);
#endif
		}
}

static int qr_got_recent_message(qr2_t qrec, int msgkey)
{
	int i;
	for (i = 0 ; i < RECENT_CLIENT_MESSAGES_TO_TRACK ; i++)
	{
		if (qrec->client_message_keys[i] == msgkey)
			return 1;
	}
	qrec->cur_message_key = (qrec->cur_message_key + 1) % RECENT_CLIENT_MESSAGES_TO_TRACK;
	qrec->client_message_keys[qrec->cur_message_key] = msgkey;
	return 0;
}

static gsi_bool qr2_process_ip_verify(qr2_t qrec, struct qr2_buffer_s* buf, struct sockaddr_in* sender)
{
	int i=0;
	gsi_time now = current_time();
	int firstFreeIndex = -1;
	int numDuplicates = 0;

	if ((qrec->backendoptions & QR2_OPTION_USE_QUERY_CHALLENGE) == 0)
	{
		qr2_buffer_add_int(buf, 0);
		return gsi_true;
	}

	for (; i < QR2_IPVERIFY_ARRAY_SIZE; i++)
	{
		if (firstFreeIndex == -1 && qrec->ipverify[i].addr.sin_addr.s_addr == 0)
			firstFreeIndex = i;
		if (qrec->ipverify[i].addr.sin_addr.s_addr == sender->sin_addr.s_addr &&
			qrec->ipverify[i].addr.sin_port == sender->sin_port)
		{
			numDuplicates++;
		}
	}

	if (numDuplicates > QR2_IPVERIFY_MAXDUPLICATES)
		return gsi_false;
	if (firstFreeIndex == -1)
		return gsi_false; 

	
	qrec->ipverify[firstFreeIndex].addr = *sender;
	qrec->ipverify[firstFreeIndex].challenge = htonl( (rand() << 16) | rand() );
	qrec->ipverify[firstFreeIndex].createtime = now;

	qr2_buffer_add_int(buf, (int)qrec->ipverify[firstFreeIndex].challenge);
	return gsi_true; 
}

static gsi_bool qr2_check_ip_verify(qr2_t qrec, struct sockaddr_in* sender, gsi_u32 ipverify)
{
	int i=0;
	for (; i < QR2_IPVERIFY_ARRAY_SIZE; i++)
	{
		if (qrec->ipverify[i].addr.sin_addr.s_addr == sender->sin_addr.s_addr &&
			qrec->ipverify[i].addr.sin_port == sender->sin_port)
		{
			if (qrec->ipverify[i].challenge == ipverify)
			{
				qrec->ipverify[i].addr.sin_addr.s_addr = 0;
				qrec->ipverify[i].addr.sin_port = 0;
				return gsi_true;
			}
		}
	}
	return gsi_false;
}

static void qr2_expire_ip_verify(qr2_t qrec)
{
	int i=0;
	gsi_time now = current_time();

	for (; i < QR2_IPVERIFY_ARRAY_SIZE; i++)
	{
		if (qrec->ipverify[i].addr.sin_addr.s_addr != 0 && (now - qrec->ipverify[i].createtime > QR2_IPVERIFY_TIMEOUT))
			qrec->ipverify[i].addr.sin_addr.s_addr = 0;
	}
}

void qr2_parse_queryA(qr2_t qrec, char *query, int len, struct sockaddr *sender)
{
	struct qr2_buffer_s buf;
	char ptype;
	char *reqkey;
	char *pos;
	gsi_u32 ipverify = 0;
	int i;
	
	buf.len = 0;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_StackTrace,
		"qr2_parse_queryA()\r\n");

	if (qrec == NULL)
		qrec = current_rec;
	if (query[0] == 0x3B) 
	{
		if (qrec->cdkeyprocess != NULL)
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
				"Forwarding cdkey query onto cdkey sdk\r\n");
			qrec->cdkeyprocess(query, len, sender);
		}
		else
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
				"Received cdkey query but not using qr2-cdkey integration!\r\n");
		}
		return;
	}
	if((len >= 6) && (memcmp(query, NNMagicData, NATNEG_MAGIC_LEN) == 0)) 
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Forwarding natneg query onto natneg sdk\r\n");
		NNProcessData(query, len, (struct sockaddr_in*)sender);
		return;
	}

	if (len < 7)
		return; 
	if ((uchar)query[0] != QR_MAGIC_1 || (uchar)query[1] != QR_MAGIC_2)
		return;

#if defined(SIMULATE_HARD_FIREWALL)
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Warning,
			"SIMULATE_HARD_FIREWALL defined, ignoring QR2 packet\r\n");
	return;
#endif

#if defined(SIMULATE_FIREWALL) 
	{
		unsigned int aMasterAddr = (cr->hbaddr.sin_addr);
		if (((SOCKADDR_IN*)sender)->sin_addr.s_addr != aMasterAddr)
		{
			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Warning,
				"SIMULATE_FIREWALL defined, ignoring non-master QR2 packet\r\n");
			return;
		}
	}
#endif
		if (qrec->denyresp2_ip_callback)
		{
			unsigned int aMasterAddr = (qrec->hbaddr.sin_addr.s_addr);
			unsigned int senderAddr = ((SOCKADDR_IN*)sender)->sin_addr.s_addr;	
			if ((aMasterAddr & 0xffff) != (senderAddr & 0xffff))				
			{
				int deny_result = 0;
				qrec->denyresp2_ip_callback(qrec->udata, senderAddr, &deny_result);
				if (deny_result)
				{
					gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Warning,
						"QR2_IP_FILTER defined, ignoring QR2 packet from denied ip\r\n");
					return;
				}
			}
		}

	if (qrec->listed_state > 0)
		qrec->listed_state = 0;

	ptype = query[2];
	reqkey = &query[3];
	pos = query + 7;
	len -= 7;

	qr_add_packet_header(&buf, ptype, reqkey);
	switch (ptype)
	{
	case PACKET_PREQUERY_IP_VERIFY:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Received IP verify challenge request\r\n");
		if (gsi_is_true(qr2_process_ip_verify(qrec, &buf, (struct sockaddr_in*)sender)))
			break; 
		else
			return; 


	case PACKET_QUERY:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Processing query packet\r\n");

		if ((qrec->backendoptions & QR2_OPTION_USE_QUERY_CHALLENGE) == QR2_OPTION_USE_QUERY_CHALLENGE)
		{
			if (len < 4)
				return; 

			ipverify = ntohl(*(gsi_u32*)pos);
			pos += 4;
			len -= 4;

			if (gsi_is_false(qr2_check_ip_verify(qrec, (struct sockaddr_in*)sender, ipverify)))
			{
				return;
			}
		}
		
		qr_process_query(qrec, &buf, (uchar *)pos, len, sender);
		return;
	case PACKET_CHALLENGE:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Processing challenge packet\r\n");

		for (i = 0 ; i < REQUEST_KEY_LEN ; i++)
		{
			if (reqkey[i] != qrec->instance_key[i])
				return; 
		}

		if (len >= (PUBLIC_ADDR_LEN + 3))
		{
			unsigned int backendoptions;

			sscanf(pos + len - (PUBLIC_ADDR_LEN + 3), "%02x", &backendoptions);
			qrec->backendoptions = (gsi_u8)backendoptions;
			
			#ifdef QR2_DEBUG_FORCE_USE_QUERY_CHALLENGE
			qrec->backendoptions = QR2_OPTION_USE_QUERY_CHALLENGE;
			#endif

			gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_Notice,
				"Received setting options: %d\r\n", qrec->backendoptions);

			if(qrec->pa_callback)
				handle_public_address(qrec, pos + len - (PUBLIC_ADDR_LEN + 1));
			else
			{
				gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
					"Discarding public address (no callback set)\r\n");
			}
		}
		compute_challenge_response(qrec, &buf, pos, len);
		break;
	case PACKET_ECHO:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Processing echo packet\r\n");

		if (len > 32)
			len = 32; 
		buf.buffer[0] = PACKET_ECHO_RESPONSE;
		memcpy(buf.buffer + buf.len, pos, (size_t)len);
		buf.len += len;
		break;

	case PACKET_ADDERROR:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"Processing adderror packet\r\n");

		if (qrec->listed_state == -1)
			return; 
		for (i = 0 ; i < REQUEST_KEY_LEN ; i++)
		{
			if (reqkey[i] != qrec->instance_key[i])
				return; 
		}
		if (len < 2)
			return; 
		qrec->listed_state = -1;
#ifndef GSI_UNICODE
		qrec->adderror_callback((qr2_error_t)*pos, pos + 1, qrec->udata);
#else
		{
			unsigned short* message_W;
			message_W = UTF8ToUCS2StringAlloc(pos+1);
			qrec->adderror_callback((qr2_error_t)*pos, message_W, qrec->udata);
			gsifree(message_W);
		}
#endif
		return; 
	case PACKET_CLIENT_MESSAGE:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Notice,
			"Processing clientmessage packet\r\n");

		for (i = 0 ; i < REQUEST_KEY_LEN ; i++)
		{
			if (reqkey[i] != qrec->instance_key[i])
				return; 
		}
		if (len < 4) 
			return;
		buf.buffer[0] = PACKET_CLIENT_MESSAGE_ACK;
		memcpy(buf.buffer + buf.len, pos, (size_t)4);
		buf.len += 4;
		memcpy(&i, pos, (size_t)4);
		if (!qr_got_recent_message(qrec, i))
			qr_process_client_message(qrec, pos + 4, len - 4);
		break;
	case PACKET_KEEPALIVE:
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
			"Processing keepalive packet\r\n");
		return; 
	default:
		return; 

	}
	sendto(qrec->hbsock, buf.buffer, buf.len, 0, sender, sizeof(struct sockaddr_in));

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
		"Sent %d bytes as QR2 query response\r\n", buf.len);
	gsDebugBinary(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_RawDump,
		buf.buffer, buf.len);
}



static void send_keepalive(qr2_t qrec)
{
	struct qr2_buffer_s buf;
	buf.len = 0;
	qr_add_packet_header(&buf, PACKET_KEEPALIVE, qrec->instance_key);
	sendto(qrec->hbsock, buf.buffer, buf.len, 0, (struct sockaddr *)&(qrec->hbaddr), sizeof(struct sockaddr_in));

	qrec->lastka = current_time();

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
		"Sent keepalive to master\r\n", buf);
}

static void send_heartbeat(qr2_t qrec, int statechanged)
{
	struct qr2_buffer_s buf;
	int i;
	char ipkey[20];

	buf.len = 0;
	qr_add_packet_header(&buf, PACKET_HEARTBEAT, qrec->instance_key);
	for (i = 0 ; i < num_local_ips ; i++)
	{
		sprintf(ipkey, "localip%d", i);
		qr2_buffer_addA(&buf, ipkey);
		qr2_buffer_addA(&buf, inet_ntoa(local_ip_list[i]));
	}
	qr2_buffer_addA(&buf, "localport");
	qr2_buffer_add_int(&buf, qrec->qport);
	qr2_buffer_addA(&buf, "natneg");
	qr2_buffer_addA(&buf, qrec->nat_negotiate ? "1" : "0");
	if(statechanged)
	{
		qr2_buffer_addA(&buf, "statechanged");
		qr2_buffer_add_int(&buf, statechanged);
	}
	qr2_buffer_addA(&buf, "gamename");
	qr2_buffer_addA(&buf, qrec->gamename);
	if(qrec->pa_callback)
	{
		qr2_buffer_addA(&buf, "publicip");
		qr2_buffer_add_int(&buf, (int)qrec->publicip);
		qr2_buffer_addA(&buf, "publicport");
		qr2_buffer_add_int(&buf, qrec->publicport);
	}
	
	if (statechanged != 2) 
	{
		struct qr2_buffer_s temp;
		memcpy(temp.buffer, buf.buffer, (size_t)buf.len);
		temp.len = buf.len;
		qr_build_query_reply(qrec, &temp, 0xFF, NULL, 0xFF, NULL, 0xFF, NULL);

		if(AVAILABLE_BUFFER_LEN(&temp) < 1)
		{
			temp.len = buf.len;
			qr_build_query_reply(qrec, &temp, 0xFF, NULL, 0, NULL, 0, NULL);
		}
		memcpy(buf.buffer, temp.buffer, (size_t)temp.len);
		buf.len = temp.len;
	}	
	else
	{
		if (AVAILABLE_BUFFER_LEN(&buf) >= 1)
			buf.buffer[buf.len++] = 0;
	}

	sendto(qrec->hbsock, buf.buffer, buf.len, 0, (struct sockaddr *)&(qrec->hbaddr), sizeof(struct sockaddr_in));

	qrec->lastka = qrec->lastheartbeat = current_time();

	if (statechanged != 0)
		qrec->userstatechangerequested = 0;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Comment,
		"Sent heartbeat to master (size %d)\r\n", buf.len);
	gsDebugBinary(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_RawDump,
		buf.buffer, buf.len);
}

#ifdef __cplusplus
}
#endif


