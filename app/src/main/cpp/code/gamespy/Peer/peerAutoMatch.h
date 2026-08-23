
#ifndef _PEERAUTOMATCH_H_
#define _PEERAUTOMATCH_H_

#include "peerMain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PI_AUTOMATCH_RATING_KEY "gsi_am_rating"

void piSetAutoMatchStatus(PEER peer, PEERAutoMatchStatus status);
void piStopAutoMatch(PEER peer);
PEERBool piJoinAutoMatchRoom(PEER peer, SBServer server);

#ifdef __cplusplus
}
#endif

#endif
