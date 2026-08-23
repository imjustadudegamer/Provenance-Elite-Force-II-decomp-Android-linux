
#ifndef _PINGER_H_
#define _PINGER_H_

#include "../common/gsCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PINGER_UDP_PING_SIZE
#define PINGER_UDP_PING_SIZE     32
#endif

#define PINGER_TIMEOUT           -1

typedef enum { PINGERFalse, PINGERTrue } PINGERBool;

typedef void (* pingerGotPing)(unsigned int IP,
							   unsigned short port,
							   int ping,
							   const char * data,
							   int len,
							   void * param);

typedef void (* pingerSetData)(unsigned int IP,
							   unsigned short port,
							   char * data,
							   int len,
							   void * param);


PINGERBool pingerInit(const char * localAddress,
					  unsigned short localPort,
					  pingerGotPing pinged,
					  void * pingedParam,
					  pingerSetData setData,
					  void * setDataParam);

void pingerShutdown(void);

void pingerThink(void);

void pingerPing(unsigned int IP,
				unsigned short port,
				pingerGotPing reply,
				void * replyParam,
				PINGERBool blocking,
				gsi_time timeout);
	
#ifdef __cplusplus
}
#endif

#endif
