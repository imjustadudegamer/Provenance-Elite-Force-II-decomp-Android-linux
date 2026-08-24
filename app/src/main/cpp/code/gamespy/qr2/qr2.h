/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#ifndef _QR2_H_
#define _QR2_H_

#include "../common/gsCommon.h"


#include "qr2regkeys.h"


#ifndef GSI_UNICODE
#define qr2_init			qr2_initA
#define qr2_init_socket		qr2_init_socketA
#define qr2_parse_query		qr2_parse_queryA
#define qr2_buffer_add		qr2_buffer_addA
#else
#define qr2_init			qr2_initW
#define qr2_init_socket		qr2_init_socketW
#define qr2_parse_query		qr2_parse_queryW
#define qr2_buffer_add		qr2_buffer_addW
#endif


#ifdef __cplusplus
extern "C" {
#endif



typedef enum 
{e_qrnoerror, 
e_qrwsockerror, 
e_qrbinderror, 
e_qrdnserror, 
e_qrconnerror,  
e_qrnochallengeerror, 
qr2_error_t_count
} qr2_error_t;


typedef enum {key_server, key_player, key_team, key_type_count} qr2_key_type;

#define NUM_PORTS_TO_TRY 100


#define QR_MAGIC_1 0xFE
#define QR_MAGIC_2 0xFD

extern char qr2_hostname[64];

typedef struct qr2_implementation_s *qr2_t;

typedef struct qr2_keybuffer_s *qr2_keybuffer_t;


typedef struct qr2_buffer_s *qr2_buffer_t;

typedef struct qr2_ipverify_node_s *qr2_ipverify_node_t;


typedef void (*qr2_serverkeycallback_t)(int keyid, qr2_buffer_t outbuf, void *userdata);
	
typedef void (*qr2_playerteamkeycallback_t)(int keyid, int index, qr2_buffer_t outbuf, void *userdata);	

	
typedef void (*qr2_keylistcallback_t)(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void *userdata);	


typedef int  (*qr2_countcallback_t)(qr2_key_type keytype, void *userdata);	

typedef void (*qr2_adderrorcallback_t)(qr2_error_t error, gsi_char *errmsg, void *userdata);	


typedef void (*qr2_natnegcallback_t)(int cookie, void *userdata);	
typedef void (*qr2_clientmessagecallback_t)(gsi_char *data, int len, void *userdata);	
typedef void (*qr2_publicaddresscallback_t)(unsigned int ip, unsigned short port, void *userdata);
typedef void (*qr2_clientconnectedcallback_t)(SOCKET gamesocket, struct sockaddr_in *remoteaddr, void *userdata);

typedef void (*qr2_denyqr2responsetoipcallback_t)(void *userdata, unsigned int sender_ip, int * result);

void qr2_register_natneg_callback(qr2_t qrec, qr2_natnegcallback_t nncallback);
void qr2_register_clientmessage_callback(qr2_t qrec, qr2_clientmessagecallback_t cmcallback);
void qr2_register_publicaddress_callback(qr2_t qrec, qr2_publicaddresscallback_t pacallback);
void qr2_register_clientconnected_callback(qr2_t qrec, qr2_clientconnectedcallback_t cccallback);

void qr2_register_denyresponsetoip_callback(qr2_t qrec, qr2_denyqr2responsetoipcallback_t dertoipcallback);


void qr2_register_key(int keyid, const gsi_char *key);


qr2_error_t qr2_init(qr2_t *qrec, const gsi_char *ip, int baseport, const gsi_char *gamename, const gsi_char *secret_key,
			int ispublic, int natnegotiate,
			qr2_serverkeycallback_t server_key_callback,
			qr2_playerteamkeycallback_t player_key_callback,
			qr2_playerteamkeycallback_t team_key_callback,
			qr2_keylistcallback_t key_list_callback,
			qr2_countcallback_t playerteam_count_callback,
			qr2_adderrorcallback_t adderror_callback,
			void *userdata);

qr2_error_t qr2_init_socket(qr2_t *qrec, SOCKET s, int boundport, const gsi_char *gamename, const gsi_char *secret_key,
					 int ispublic, int natnegotiate,
					 qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback,
					 qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback,
					 qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback,
					 void *userdata);

void qr2_think(qr2_t qrec);

void qr2_parse_query(qr2_t qrec, gsi_char *query, int len, struct sockaddr *sender);

void qr2_send_statechanged(qr2_t qrec);

void qr2_shutdown(qr2_t qrec);



gsi_bool qr2_keybuffer_add(qr2_keybuffer_t keybuffer, int keyid);

gsi_bool qr2_buffer_add(qr2_buffer_t outbuf, const gsi_char *value);
gsi_bool qr2_buffer_add_int(qr2_buffer_t outbuf, int value);


#define REQUEST_KEY_LEN 4
#define RECENT_CLIENT_MESSAGES_TO_TRACK 10
typedef void (*cdkey_process_t)(char *buf, int len, struct sockaddr *fromaddr);

#define QR2_IPVERIFY_TIMEOUT        4000  
#define QR2_IPVERIFY_ARRAY_SIZE     200   
#define QR2_IPVERIFY_MAXDUPLICATES  5     
struct qr2_ipverify_info_s
{
	struct sockaddr_in addr;      
	gsi_u32            challenge;
	gsi_time           createtime; 
};

struct qr2_implementation_s
{
	SOCKET hbsock;
	char gamename[64];
	char secret_key[64];
	char instance_key[REQUEST_KEY_LEN];
	qr2_serverkeycallback_t server_key_callback;
	qr2_playerteamkeycallback_t player_key_callback;
	qr2_playerteamkeycallback_t team_key_callback;
	qr2_keylistcallback_t key_list_callback;
	qr2_countcallback_t playerteam_count_callback;
	qr2_adderrorcallback_t adderror_callback;					
	qr2_natnegcallback_t nn_callback;
	qr2_clientmessagecallback_t cm_callback;
	qr2_publicaddresscallback_t pa_callback;
	qr2_clientconnectedcallback_t cc_callback;
	qr2_denyqr2responsetoipcallback_t denyresp2_ip_callback;


	gsi_time lastheartbeat;
	gsi_time lastka;
	int userstatechangerequested;
	int listed_state;
	int ispublic;	 
	int qport;
	int read_socket;
	int nat_negotiate;
	struct sockaddr_in hbaddr;
	cdkey_process_t cdkeyprocess;
	int client_message_keys[RECENT_CLIENT_MESSAGES_TO_TRACK];
	int cur_message_key;
	unsigned int publicip;
	unsigned short publicport;
	void *udata;

	gsi_u8 backendoptions; 
	struct qr2_ipverify_info_s ipverify[QR2_IPVERIFY_ARRAY_SIZE];
};

void qr2_parse_queryA(qr2_t qrec, char *query, int len, struct sockaddr *sender);
gsi_bool qr2_buffer_addA(qr2_buffer_t outbuf, const char *value);
qr2_error_t qr2_initA(qr2_t *qrec, const char *ip, int baseport, const char *gamename, const char *secret_key,
			int ispublic, int natnegotiate,
			qr2_serverkeycallback_t server_key_callback,
			qr2_playerteamkeycallback_t player_key_callback,
			qr2_playerteamkeycallback_t team_key_callback,
			qr2_keylistcallback_t key_list_callback,
			qr2_countcallback_t playerteam_count_callback,
			qr2_adderrorcallback_t adderror_callback,
			void *userdata);
qr2_error_t qr2_init_socketA(qr2_t *qrec, SOCKET s, int boundport, const char *gamename, const char *secret_key,
					 int ispublic, int natnegotiate,
					 qr2_serverkeycallback_t server_key_callback,
					 qr2_playerteamkeycallback_t player_key_callback,
					 qr2_playerteamkeycallback_t team_key_callback,
					 qr2_keylistcallback_t key_list_callback,
					 qr2_countcallback_t playerteam_count_callback,
					 qr2_adderrorcallback_t adderror_callback,
					 void *userdata);


#ifdef __cplusplus
}
#endif


#endif
