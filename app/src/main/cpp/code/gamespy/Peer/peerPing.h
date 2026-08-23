
#ifndef _PEERPING_H_
#define _PEERPING_H_

#include "peerMain.h"


#ifdef __cplusplus
extern "C" {
#endif


PEERBool piPingInit(PEER peer);
void piPingCleanup(PEER peer);
void piPingThink(PEER peer);
PEERBool piPingInitPlayer(PEER peer, piPlayer * player);
void piPingPlayerJoinedRoom(PEER peer, piPlayer * player, RoomType roomType);
void piPingPlayerLeftRoom(PEER peer, piPlayer * player);
void piUpdateXping(PEER peer, const char * nick1, const char * nick2, int ping);
PEERBool piGetXping(PEER peer, const char * nick1, const char * nick2, int * ping);

#ifdef __cplusplus
}
#endif

#endif
