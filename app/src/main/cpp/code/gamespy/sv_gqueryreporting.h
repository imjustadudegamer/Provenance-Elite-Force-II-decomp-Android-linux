

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct sockaddr_in;
struct hostent;


#define E_GOA_WSOCKERROR 1
#define E_GOA_BINDERROR  2
#define E_GOA_DNSERROR   3
#define E_GOA_CONNERROR  4
#define NUM_PORTS_TO_TRY 100

#define MASTER_PORT     qr_get_master_port(0)
#define MASTER_ADDR     qr_get_master_host(0)
#define FIRST_HB_TIME   30000  
#define HB_TIME         300000 
#define HB_SEND_DELAY   0      
#define MAX_FIRST_COUNT 10     
#define MAX_DATA_SIZE   1400
#define INBUF_LEN       256
#define BUF_SIZE        1400

extern char qr_hostname[64];

extern unsigned int qr_get_num_masters(void);
extern const char *qr_get_master_host(int index);
extern int qr_get_master_port(int index);

typedef void (*qr_querycallback_t)(char* outbuf, int maxlen, void* userdata);

typedef void (*qr_cdkey_process_t)(char* buf, int len, struct sockaddr* fromaddr);

typedef struct qr_implementation_s *qr_t;
int qr_init( qr_t* qrec,
            const char* ip,
            int baseport,
            const char* gamename,
            const char* secret_key,
            qr_querycallback_t qr_basic_callback,
            qr_querycallback_t qr_info_callback,
            qr_querycallback_t qr_rules_callback,
            qr_querycallback_t qr_players_callback,
            void* userdata);

void qr_process_queries(qr_t qrec);
void qr_process_queries_no_heartbeat(qr_t qrec);

void qr_send_statechanged(qr_t qrec);

void qr_shutdown(qr_t qrec);

void qr_check_queries(qr_t qrec);

void qr_send_exiting(qr_t qrec);

int get_master_count(void);

void clear_master_list(void);

int get_sockaddrin(const char *host, int port, struct sockaddr_in *saddr, struct hostent **savehent);

void add_master(struct sockaddr_in *addr);

unsigned int qr_get_port(qr_t qrec);

#ifdef __cplusplus
}
#endif
