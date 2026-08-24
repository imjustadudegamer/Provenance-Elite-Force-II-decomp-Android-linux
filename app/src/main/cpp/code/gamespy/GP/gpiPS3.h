/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */

#ifndef _GPIPS3_H_
#define _GPIPS3_H_

#include "gpi.h"
#include <np.h>
#include <np\common.h>
#include <sysutil/sysutil_common.h>


#define	GPI_NP_SYNC_DELAY		5000	
#define	GPI_NP_STATUS_TIMEOUT	5000	
#define	GPI_NP_NUM_TRANSACTIONS	32	    

typedef struct 
{
    int       npTransId;
    SceNpId   *npIdForAdd;
    gsi_bool  npLookupDone;
} npIdLookupTrans;

GPResult gpiInitializeNpBasic();
GPResult gpiCheckNpStatus(GPConnection * connection);
GPResult gpiDestroyNpBasic(GPConnection * connection);
GPResult gpiProcessNp(GPConnection * connection);
int gpiNpBasicCallback(int event, int retCode, uint32_t reqId, void *arg);

GPResult gpiSyncNpBuddies(GPConnection * connection);
void gpiSyncNpBuddiesCallback(GPConnection * pconnection, GPProfileSearchResponseArg * arg, void * param);

GPResult gpiSyncNpBlockList(GPConnection * connection);
void gpiSyncNpBlockListCallback(GPConnection * pconnection, GPProfileSearchResponseArg * arg, void * param);

GPResult gpiAddToNpBlockList(GPConnection * connection, int profileid);
void gpiAddToNpBlockListInfoCallback(GPConnection * pconnection, GPGetInfoResponseArg * arg, void * param);

#endif
