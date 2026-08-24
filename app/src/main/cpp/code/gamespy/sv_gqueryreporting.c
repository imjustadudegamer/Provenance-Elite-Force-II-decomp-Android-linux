/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/


#if defined(applec) || defined(THINK_C) || defined(__MWERKS__) && !defined(__KATANA__) && !defined(__mips64)
#include "::nonport.h"
#else
#include "nonport.h"
#endif
#include "sv_gqueryreporting.h"
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#if !defined(UNDER_CE) && !defined(__KATANA__)
#include <assert.h>
#else
#define assert(a)
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned char uchar;
#if defined(_LINUX) || defined(__linux__)
typedef unsigned int qrsocklen_t;
#else
typedef int qrsocklen_t;
#endif

typedef enum
{
    qtunknown,
    qtbasic,
    qtinfo,
    qtrules,
    qtplayers,
    qtstatus,
    qtpackets,
    qtecho,
    qtsecure
} query_t;

typedef struct qr_implementation_s
{
    SOCKET querysock;
    SOCKET hbsock;
    char gamename[64];
    char secret_key[128];
    qr_querycallback_t qr_basic_callback;
    qr_querycallback_t qr_info_callback;
    qr_querycallback_t qr_rules_callback;
    qr_querycallback_t qr_players_callback;
    unsigned long lastheartbeat;
    int queryid;
    int packetnumber;
    int qport;
    char no_query;
    struct sockaddr_in hbaddr;
    qr_cdkey_process_t cdkeyprocess;
    void* udata;

    unsigned long next_master_delay;
    int current_master_id;
    unsigned long next_master_time;
    int next_master_pending;
    int last_statechanged;
}* qr_t;

static const char* queries[] = {"", "basic", "info", "rules", "players", "status", "packets", "echo", "secure"};
struct qr_implementation_s static_rec = {INVALID_SOCKET, INVALID_SOCKET};
static qr_t current_rec = &static_rec;

static struct sockaddr_in *MasterList = NULL;
static char players_keyvalue[8192];
static int MasterCount;
static int MasterMaxCount;

static void send_heartbeat(qr_t qrec, int statechanged);
static void qr_parse_query(qr_t qrec, char* query, int len, struct sockaddr* sender);
static int do_connect_multi(void);
void qr_check_queries(qr_t qrec);
void qr_check_send_heartbeat(qr_t qrec);

static void init_qrec(qr_t* qrec,
                      int baseport,
                      SOCKET hbsock,
                      SOCKET querysock,
                      const char* gamename,
                      const char* secret_key,
                      qr_querycallback_t qr_basic_callback,
                      qr_querycallback_t qr_info_callback,
                      qr_querycallback_t qr_rules_callback,
                      qr_querycallback_t qr_players_callback,
                      void* userdata);


int qr_init(qr_t* qrec,
            const char* ip,
            int baseport,
            const char* gamename,
            const char* secret_key,
            qr_querycallback_t qr_basic_callback,
            qr_querycallback_t qr_info_callback,
            qr_querycallback_t qr_rules_callback,
            qr_querycallback_t qr_players_callback,
            void* userdata)
{
    int lasterror;
    struct sockaddr_in saddr;
    qrsocklen_t saddrlen;
    int maxport;
    SOCKET querysock;
    SOCKET hbsock;

    if (qrec != NULL) 
        *qrec = NULL;
    SocketStartUp();

    querysock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef TCP_HEARTBEATS
    hbsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    hbsock = querysock; 
#endif
    if (INVALID_SOCKET == querysock || INVALID_SOCKET == hbsock) {
        return E_GOA_WSOCKERROR;
    }

    lasterror = 0;
    maxport = baseport + NUM_PORTS_TO_TRY;
    while (baseport < maxport) {
        get_sockaddrin(ip, baseport, &saddr, NULL);
        if (saddr.sin_addr.s_addr == htonl(0x7F000001)) 
            saddr.sin_addr.s_addr = INADDR_ANY;

        lasterror = bind(querysock, (struct sockaddr*)&saddr, sizeof(saddr));
        if (lasterror == 0)
            break; 
        baseport++;
    }

    if (lasterror != 0) 
    {
        return E_GOA_BINDERROR;
    }

    if (baseport == 0) 
    {
        saddrlen = sizeof(saddr);
        lasterror = getsockname(querysock, (struct sockaddr*)&saddr, &saddrlen);
        if (lasterror)
            return E_GOA_BINDERROR;
        baseport = ntohs(saddr.sin_port);
    }

    init_qrec(qrec,
              baseport,
              hbsock,
              querysock,
              gamename,
              secret_key,
              qr_basic_callback,
              qr_info_callback,
              qr_rules_callback,
              qr_players_callback,
              userdata);

    if (!qrec) {
        qrec = &current_rec;
    }

    return do_connect_multi();
}

void init_qrec(qr_t* qrec,
               int qport,
               SOCKET hbsock,
               SOCKET querysock,
               const char* gamename,
               const char* secret_key,
               qr_querycallback_t qr_basic_callback,
               qr_querycallback_t qr_info_callback,
               qr_querycallback_t qr_rules_callback,
               qr_querycallback_t qr_players_callback,
               void* userdata)
{
    qr_t cr;

    if (qrec == NULL) {
        cr = &static_rec;
    } else {
        *qrec = (qr_t)malloc(sizeof(struct qr_implementation_s));
        cr = *qrec;
    }

    strcpy(cr->gamename, gamename);
    strcpy(cr->secret_key, secret_key);

    cr->qport = qport;
    cr->lastheartbeat = 0;
    cr->hbsock = hbsock;
    cr->querysock = querysock;
    cr->packetnumber = 0;
    cr->queryid = 1;
    cr->no_query = 1;
    cr->udata = userdata;
    cr->qr_basic_callback = qr_basic_callback;
    cr->qr_info_callback = qr_info_callback;
    cr->qr_players_callback = qr_players_callback;
    cr->qr_rules_callback = qr_rules_callback;
    cr->cdkeyprocess = NULL;

    cr->next_master_delay = HB_SEND_DELAY;
    cr->next_master_time = 0;
    cr->next_master_pending = 0;
    cr->current_master_id = 0;
    cr->last_statechanged = -1;
}

int qr_init_socket(qr_t* qrec,
                   unsigned int s,
                   const char* gamename,
                   const char* secret_key,
                   qr_querycallback_t qr_basic_callback,
                   qr_querycallback_t qr_info_callback,
                   qr_querycallback_t qr_rules_callback,
                   qr_querycallback_t qr_players_callback,
                   void* userdata)
{
    if (qrec) {
        *qrec = NULL;
    }

    SocketStartUp();
    init_qrec(qrec,
              0,
              s,
              s,
              gamename,
              secret_key,
              qr_basic_callback,
              qr_info_callback,
              qr_rules_callback,
              qr_players_callback,
              userdata);

    return do_connect_multi();
}

void qr_process_queries(qr_t qrec)
{
    if (qrec == NULL)
        qrec = current_rec;
    qr_check_send_heartbeat(qrec);
    qr_check_queries(qrec);
}

void qr_process_queries_no_heartbeat(qr_t qrec)
{
    if (qrec == NULL)
        qrec = current_rec;
    qr_check_queries(qrec);
}

void qr_check_queries(qr_t qrec)
{
    static char indata[INBUF_LEN]; 
    struct sockaddr_in saddr;
    qrsocklen_t saddrlen = sizeof(struct sockaddr_in);
    fd_set set;
    struct timeval timeout = {0, 0};
    int error;

    if (0 == qrec->qport)
        return;

    FD_ZERO(&set);
    FD_SET(qrec->querysock, &set);

    while (1) {
        error = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
        if (gsiSocketIsError(error) || error == 0)
            return;
        error = recvfrom(qrec->querysock, indata, INBUF_LEN - 1, 0, (struct sockaddr*)&saddr, &saddrlen);
        if (gsiSocketIsNotError(error)) {
            indata[error] = '\0';
            qr_parse_query(qrec, indata, error, (struct sockaddr*)&saddr);
        }
    }
}

void qr_check_send_heartbeat(qr_t qrec)
{
    unsigned long tc = current_time();

    if (INVALID_SOCKET == qrec->hbsock)
        return; 

    if (qrec->next_master_pending) {
        if (tc >= qrec->next_master_time) {
            send_heartbeat(qrec, qrec->last_statechanged);
        }
        return;
    }

    if (tc - qrec->lastheartbeat > HB_TIME || qrec->lastheartbeat == 0 || tc < qrec->lastheartbeat)
        send_heartbeat(qrec, 0);
    else if (qrec->no_query > 0
             && tc - qrec->lastheartbeat > FIRST_HB_TIME) { 
        send_heartbeat(qrec, 0);
        qrec->no_query++;
        if (qrec->no_query > MAX_FIRST_COUNT)
            qrec->no_query = 0; 
    }
}

void qr_send_statechanged(qr_t qrec)
{
    if (qrec == NULL)
        qrec = current_rec;
    send_heartbeat(qrec, 1);
}

void qr_send_exiting(qr_t qrec)
{
    if (qrec == NULL)
        qrec = current_rec;
    send_heartbeat(qrec, 2);
}

void qr_shutdown(qr_t qrec)
{
    if (qrec == NULL)
        qrec = current_rec;
    if (INVALID_SOCKET != qrec->querysock && qrec->qport != 0) {
        closesocket(qrec->querysock);
    }
    if (INVALID_SOCKET != qrec->hbsock && qrec->hbsock != qrec->querysock) {
        closesocket(qrec->hbsock);
    }
    qrec->hbsock = INVALID_SOCKET;
    qrec->querysock = INVALID_SOCKET;
    qrec->lastheartbeat = 0;
    if (qrec != &static_rec) 
    {
        gsifree(qrec);
    }
    
    if (MasterList) {
        gsifree(MasterList);
        MasterList = NULL;
    }

    MasterCount = 0;

    SocketShutDown();
}


// NOTE: the single-master do_connect() that used to live here was superseded by

static int do_connect_multi(void)
{
    int i;

    MasterMaxCount = qr_get_num_masters();
    if (MasterList) {
        gsifree(MasterList);
        MasterList = NULL;
    }

    MasterList = gsimalloc(sizeof(struct sockaddr_in) * MasterMaxCount);
    MasterCount = 0;

    for(i = 0; i < MasterMaxCount; i++) {
        struct sockaddr_in hbaddr;
        if (get_sockaddrin(qr_get_master_host(i), qr_get_master_port(i), &hbaddr, NULL)) {
            add_master(&hbaddr);
        }
    }

    return 0;
}

int get_sockaddrin(const char* host, int port, struct sockaddr_in* saddr, struct hostent** savehent)
{
    struct hostent* hent;
    char broadcast_t[] = {
        '2', '5', '5', '.', '2', '5', '5', '.', '2', '5', '5', '.', '2', '5', '5', '\0'}; 
    hent = 0;

    memset(saddr, 0, sizeof(struct sockaddr_in));
    saddr->sin_family = AF_INET;
    saddr->sin_port = htons((unsigned short)port);
    if (host == NULL)
        saddr->sin_addr.s_addr = INADDR_ANY;
    else
        saddr->sin_addr.s_addr = inet_addr(host);

    if (saddr->sin_addr.s_addr == INADDR_NONE && strcmp(host, broadcast_t) != 0) {
        hent = gethostbyname(host);
        if (!hent)
            return 0;
        saddr->sin_addr.s_addr = *(u_long*)hent->h_addr_list[0];
    }
    if (savehent != NULL)
        *savehent = hent;
    return 1;
}

static char* value_for_key(const char* s, const char* key)
{
    static int valueindex;
    char *pos, *pos2;
    char keyspec[256] = "\\";
    static char value[2][256];

    valueindex ^= 1;
    strcat(keyspec, key);
    strcat(keyspec, "\\");
    pos = strstr(s, keyspec);
    if (!pos)
        return NULL;
    pos += strlen(keyspec);
    pos2 = value[valueindex];
    while (*pos && *pos != '\\')
        *pos2++ = *pos++;
    *pos2 = '\0';
    return value[valueindex];
}


#ifndef _GUTIL
#define _GUTIL
static void swap_byte(uchar* a, uchar* b)
{
    uchar swapByte;

    swapByte = *a;
    *a = *b;
    *b = swapByte;
}

static uchar encode_ct(uchar c)
{
    if (c < 26)
        return ('A' + c);
    if (c < 52)
        return ('a' + c - 26);
    if (c < 62)
        return ('0' + c - 52);
    if (c == 62)
        return ('+');
    if (c == 63)
        return ('/');

    return 0;
}

static void gs_encode(uchar* ins, int size, uchar* result)
{
    int i, pos;
    uchar trip[3];
    uchar kwart[4];

    i = 0;
    while (i < size) {
        for (pos = 0; pos <= 2; pos++, i++)
            if (i < size)
                trip[pos] = *ins++;
            else
                trip[pos] = '\0';
        kwart[0] = (trip[0]) >> 2;
        kwart[1] = (((trip[0]) & 3) << 4) + ((trip[1]) >> 4);
        kwart[2] = (((trip[1]) & 15) << 2) + ((trip[2]) >> 6);
        kwart[3] = (trip[2]) & 63;
        for (pos = 0; pos <= 3; pos++)
            *result++ = encode_ct(kwart[pos]);
    }
    *result = '\0';
}

static void gs_encrypt(uchar* key, int key_len, uchar* buffer_ptr, int buffer_len)
{
    short counter;
    uchar x, y, xorIndex;
    uchar state[256];

    for (counter = 0; counter < 256; counter++)
        state[counter] = (uchar)counter;

    x = 0;
    y = 0;
    for (counter = 0; counter < 256; counter++) {
        y = (key[x] + state[counter] + y) % 256;
        x = (x + 1) % key_len;
        swap_byte(&state[counter], &state[y]);
    }

    x = 0;
    y = 0;
    for (counter = 0; counter < buffer_len; counter++) {
        x = (x + buffer_ptr[counter] + 1) % 256;
        y = (state[x] + y) % 256;
        swap_byte(&state[x], &state[y]);
        xorIndex = (state[x] + state[y]) % 256;
        buffer_ptr[counter] ^= state[xorIndex];
    }
}
#endif

static void packet_send(qr_t qrec, struct sockaddr* addr, char* buffer)
{
    char keyvalue[80];

    if (strlen(buffer) == 0)
        return;           
    qrec->packetnumber++; 
    snprintf(keyvalue, sizeof(keyvalue), "\\queryid\\%d.%d", qrec->queryid, qrec->packetnumber);
    strcat(buffer, keyvalue);
    sendto(qrec->querysock, buffer, (int)strlen(buffer), 0, addr, sizeof(struct sockaddr_in));
    buffer[0] = '\0';
}

static void buffer_send(qr_t qrec, struct sockaddr* sender, char* buffer, char* newdata)
{
    char *pos, *lastkey;
    int bcount = 0;

    if (strlen(buffer) + strlen(newdata) < MAX_DATA_SIZE - 50) {
        strcat(buffer, newdata);
    } else {
        if (strlen(newdata) > MAX_DATA_SIZE - 50) 
        {
            lastkey = pos = newdata;
            while (pos - newdata < MAX_DATA_SIZE - 50) {
                if ('\\' == *pos) {
                    if (bcount % 2 == 0)
                        lastkey = pos;
                    bcount++;
                }
                pos++;
            }
            if (lastkey == newdata)
                return; 
            *lastkey = '\0';
            buffer_send(qrec, sender, buffer, newdata);
            *lastkey = '\\';
            buffer_send(qrec, sender, buffer, lastkey); 
        } else {
            packet_send(qrec, sender, buffer);
            strcpy(buffer, newdata);
        }
    }
}

static void send_basic(qr_t qrec, struct sockaddr* sender, char* outbuf)
{
    char keyvalue[BUF_SIZE] = "";

    assert(qrec->qr_basic_callback);
    qrec->qr_basic_callback(keyvalue, sizeof(keyvalue), qrec->udata);
    buffer_send(qrec, sender, outbuf, keyvalue);
}

static void send_info(qr_t qrec, struct sockaddr* sender, char* outbuf)
{
    char keyvalue[BUF_SIZE] = "";

    assert(qrec->qr_info_callback);
    qrec->qr_info_callback(keyvalue, sizeof(keyvalue), qrec->udata);
    buffer_send(qrec, sender, outbuf, keyvalue);
}

static void send_rules(qr_t qrec, struct sockaddr* sender, char* outbuf)
{
    char keyvalue[BUF_SIZE] = "";

    assert(qrec->qr_rules_callback);
    qrec->qr_rules_callback(keyvalue, sizeof(keyvalue), qrec->udata);
    buffer_send(qrec, sender, outbuf, keyvalue);
}

static void send_players(qr_t qrec, struct sockaddr* sender, char* outbuf)
{
    assert(qrec->qr_players_callback);
    qrec->qr_players_callback(players_keyvalue, sizeof(players_keyvalue), qrec->udata);
    buffer_send(qrec, sender, outbuf, players_keyvalue);
}

static void send_echo(qr_t qrec, struct sockaddr* sender, char* outbuf, char* echostr)
{
    char keyvalue[MAX_DATA_SIZE];

    if (strlen(echostr) > MAX_DATA_SIZE - 50)
        return;
    snprintf(keyvalue, sizeof(keyvalue), "\\echo\\%s", echostr);
    buffer_send(qrec, sender, outbuf, keyvalue);
}

static void send_final(qr_t qrec, struct sockaddr* sender, char* outbuf, char* validation)
{
    char keyvalue[256];
    char encrypted_val[128]; 
    char encoded_val[200];
    int keylen;

    if (validation[0]) {
        keylen = (int)strlen(validation);
        if (keylen > 128)
            return;
        strcpy(encrypted_val, validation);
        gs_encrypt((uchar*)qrec->secret_key, (int)strlen(qrec->secret_key), (uchar*)encrypted_val, keylen);
        gs_encode((uchar*)encrypted_val, keylen, (uchar*)encoded_val);
        snprintf(keyvalue, sizeof(keyvalue), "\\validate\\%s", encoded_val);
        buffer_send(qrec, sender, outbuf, keyvalue);
    }

    snprintf(keyvalue, sizeof(keyvalue), "\\final\\");
    buffer_send(qrec, sender, outbuf, keyvalue);
    packet_send(qrec, sender, outbuf);
}

static void qr_parse_query(qr_t qrec, char* query, int len, struct sockaddr* sender)
{
    int querytype;
    char buffer[MAX_DATA_SIZE] = "";
    char* value;
    char validation[256] = "";

    if (qrec == NULL)
        qrec = current_rec;
    if (query[0] == 0x3B) 
    {
        if (qrec->cdkeyprocess != NULL) {
            qrec->cdkeyprocess(query, len, sender);
        }
        return;
    }

    qrec->queryid++;
    qrec->packetnumber = 0;
    if (qrec->no_query > 0)
        qrec->no_query = 0;

    for (querytype = qtbasic; querytype <= qtsecure; querytype++) {
        if ((value = value_for_key(query, queries[querytype])))
            switch (querytype) {
            case qtbasic:
                send_basic(qrec, sender, buffer);
                break;
            case qtinfo:
                send_info(qrec, sender, buffer);
                break;
            case qtrules:
                send_rules(qrec, sender, buffer);
                break;
            case qtplayers:
                send_players(qrec, sender, buffer);
                break;
            case qtstatus:
                send_basic(qrec, sender, buffer);
                send_info(qrec, sender, buffer);
                send_rules(qrec, sender, buffer);
                send_players(qrec, sender, buffer);
                break;
            case qtpackets:
                send_basic(qrec, sender, buffer);
                packet_send(qrec, sender, buffer);
                send_info(qrec, sender, buffer);
                packet_send(qrec, sender, buffer);
                send_rules(qrec, sender, buffer);
                packet_send(qrec, sender, buffer);
                send_players(qrec, sender, buffer);
                break;
            case qtecho:
                send_echo(qrec, sender, buffer, value);
                break;
            case qtsecure:
                strcpy(validation, value);
                break;
            case qtunknown:
                break;
            }
    }
    send_final(qrec, sender, buffer, validation);
}

int get_master_count(void)
{
    return MasterCount;
}

void clear_master_list(void)
{
    // NOTE: MasterList used to be a fixed "struct sockaddr_in MasterList[8]"
    if (MasterList) {
        memset(MasterList, 0, sizeof(struct sockaddr_in) * MasterMaxCount);
    }
    MasterCount = 0;
}

void add_master(struct sockaddr_in* addr)
{
    int i;
    for (i = 0; i < MasterCount; i++) {
        if (memcmp(addr, &MasterList[i], sizeof(MasterList[i])) == 0) {
            return;
        }
    }
    if (i == MasterMaxCount) {
        return;
    } else {
        MasterList[i] = *addr;
        MasterCount++;
    }
}

static void send_heartbeat(qr_t qrec, int statechanged)
{
    char buf[256];
    int i;

    snprintf(buf, sizeof(buf), "\\heartbeat\\%d\\gamename\\%s", qrec->qport, qrec->gamename);
    if (statechanged)
        snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "\\statechanged\\");

    if (qrec->next_master_delay > 0 && statechanged != 2) {

        if (qrec->last_statechanged != statechanged) {
            qrec->current_master_id = 0;
            qrec->last_statechanged = statechanged;
        }

        i = qrec->current_master_id;
        sendto(qrec->hbsock, buf, (int)strlen(buf), 0, (struct sockaddr*)&MasterList[i], sizeof(MasterList[i]));

        qrec->current_master_id++;
        if (qrec->current_master_id < MasterCount) {
            qrec->next_master_time = current_time() + qrec->next_master_delay;
            qrec->next_master_pending = 1;
        } else {
            qrec->current_master_id = 0;
            qrec->next_master_time = 0;
            qrec->next_master_pending = 0;
            qrec->last_statechanged = -1;
        }
    } else {
        for (i = 0; i < MasterCount; i++) {
            sendto(qrec->hbsock, buf, (int)strlen(buf), 0, (struct sockaddr*)&MasterList[i], sizeof(MasterList[i]));
        }
    }

    qrec->lastheartbeat = current_time();
}

unsigned int qr_get_port(qr_t qrec) {
    if (!qrec) {
        qrec = current_rec;
    }

    return qrec->qport;
}

#ifdef __cplusplus
}
#endif
