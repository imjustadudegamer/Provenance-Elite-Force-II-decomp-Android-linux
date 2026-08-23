
#ifndef _PEERHOST_H_
#define _PEERHOST_H_

#include "peerMain.h"


#ifdef __cplusplus
extern "C" {
#endif

PEERBool piStartHosting(PEER peer, SOCKET socket, unsigned short port);
void piStopHosting(PEER peer, PEERBool stopReporting);

#ifdef __cplusplus
}
#endif

#endif
