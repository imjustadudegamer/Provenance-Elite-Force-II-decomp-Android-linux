/*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#ifndef _PEERQR_H_
#define _PEERQR_H_

#include "peerMain.h"


#ifdef __cplusplus
extern "C" {
#endif


#define PI_QUERYPORT       6500

PEERBool piStartReporting(PEER peer, SOCKET socket, unsigned short port);
void piStopReporting(PEER peer);
void piSendStateChanged(PEER peer);
void piQRThink(PEER peer);
PEERBool piStartAutoMatchReporting(PEER peer);
void piStopAutoMatchReporting(PEER peer);

#ifdef __cplusplus
}
#endif

#endif
