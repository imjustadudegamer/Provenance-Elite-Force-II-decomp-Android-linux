
#ifndef _PEERPLAYERS_H_
#define _PEERPLAYERS_H_

#include "peerMain.h"


#ifdef __cplusplus
extern "C" {
#endif


#define PI_PING_HISTORY_LEN         4

typedef struct piPlayer
{
	char nick[PI_NICK_MAX_LEN];     
	PEERBool inRoom[NumRooms];      
	PEERBool local;                 

	unsigned int IP;                
	int profileID;                  
	PEERBool gotIPAndProfileID;     

	int flags[NumRooms];            

	unsigned long lastPingSend;     
	unsigned long lastPingRecv;     
	unsigned long lastXping;        
	PEERBool waitingForPing;        
	int pingsReturned;              
	int pingsLostConsecutive;       
	int pingAverage;                
	int pingHistory[PI_PING_HISTORY_LEN];  
	int pingHistoryNum;             
	int numPings;                   
	PEERBool xpingSent;             
	PEERBool inPingRoom;            
	PEERBool inXpingRoom;           
	PEERBool mustPing;              
	PEERBool pingOnce;              
} piPlayer;

PEERBool piPlayersInit(PEER peer);
void piPlayersCleanup(PEER peer);
piPlayer * piPlayerJoinedRoom(PEER peer, const char * nick, RoomType roomType, int mode);
void piPlayerLeftRoom(PEER peer, const char * nick, RoomType roomType);
void piPlayerChangedNick(PEER peer, const char * oldNick, const char * newNick);
void piClearRoomPlayers(PEER peer, RoomType roomType);
piPlayer * piGetPlayer(PEER peer, const char * nick);
typedef void (* piEnumRoomPlayersCallback)(PEER peer, RoomType roomType, piPlayer * player, int index, void * param);
void piEnumRoomPlayers(PEER peer, RoomType roomType, piEnumRoomPlayersCallback callback, void * param);
piPlayer * piFindPlayerByIP(PEER peer, unsigned int IP);
void piSetPlayerIPAndProfileID(PEER peer, const char * nick, unsigned int IP, int profileID);
int piParseFlags(const char * flags);
void piSetPlayerRoomFlags(PEER peer, const char * nick, RoomType roomType, const char * flags);
void piSetPlayerModeFlags(PEER peer, const char * nick, RoomType roomType, int mode);
PEERBool piIsPlayerVIP(piPlayer * player, RoomType roomType);
PEERBool piIsPlayerHost(piPlayer * player);
PEERBool piIsPlayerOp(piPlayer * player);
piPlayer * piFindPlayerByIndex(PEER peer, RoomType roomType, int index);
piPlayer * piFindRoomHost(PEER peer, RoomType roomType);
piPlayer * piFindRoomOp(PEER peer, RoomType roomType);
int piCountRoomOps(PEER peer, RoomType roomType, const char * exclude);

#ifdef __cplusplus
}
#endif

#endif
