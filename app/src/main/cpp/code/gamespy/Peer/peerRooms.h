
#ifndef _PEERROOMS_H_
#define _PEERROOMS_H_

#include "peerMain.h"


#ifdef __cplusplus
extern "C" {
#endif


PEERBool piRoomsInit(PEER peer);
void piRoomsCleanup(PEER peer);
void piStartedEnteringRoom(PEER peer, RoomType roomType, const char * room);
void piFinishedEnteringRoom(PEER peer, RoomType roomType, const char * name);
void piLeaveRoom(PEER peer, RoomType roomType, const char * reason);
PEERBool piRoomToType(PEER peer, const char * room, RoomType * roomType);
void piSetLocalFlags(PEER peer);

#ifdef __cplusplus
}
#endif

#endif
