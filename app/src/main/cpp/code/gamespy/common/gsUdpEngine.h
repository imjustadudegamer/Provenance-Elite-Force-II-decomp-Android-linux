#ifndef __GS_UDP_ENGINE_H__
#define __GS_UDP_ENGINE_H__

#include "gsCommon.h"
#include "../gt2/gt2.h"
#include "../darray.h"


#define GS_UDP_DEFAULT_OUT_BUFFSIZE  1460
#define GS_UDP_DEFAULT_IN_BUFFSIZE 1500

#define GS_IP_ADDR_AND_PORT          22

#define GS_UDP_MSG_HEADER_LEN        16
#define GS_UDP_RELIABLE_MSG_HEADER   7

typedef enum _GSUdpErrorCode
{
	GS_UDP_NO_ERROR,
	GS_UDP_NO_MEMORY,
	GS_UDP_REJECTED,
	GS_UDP_NETWORK_ERROR,
	GS_UDP_ADDRESS_ERROR,
	GS_UDP_ADDRESS_ALREADY_IN_USE,
	GS_UDP_TIMED_OUT,
	GS_UDP_REMOTE_ERROR,
	GS_UDP_SEND_FAILED,
	GS_UDP_INVALID_MESSAGE,
	GS_UDP_PARAMETER_ERROR,
	GS_UDP_NOT_INITIALIZED,
	GS_UDP_MSG_TOO_BIG,
	GS_UDP_UNKNOWN_ERROR,
	GS_UDP_NUM_ERROR_CODES
} GSUdpErrorCode;

typedef enum _GSUdpPeerState
{
	GS_UDP_PEER_CONNECTING,
	GS_UDP_PEER_CONNECTED,
	GS_UDP_PEER_CLOSING,
	GS_UDP_PEER_CLOSED,
	GS_UDP_PEER_STATE_NUM
} GSUdpPeerState;

typedef enum _GSUdpCloseReason
{
	GS_UDP_CLOSED_LOCALLY,
	GS_UDP_CLOSED_REMOTELY,
	GS_UDP_CLOSED_BY_COMM_ERROR, 
	GS_UDP_CLOSED_BY_LOW_MEM,
	GS_UDP_CLOSED_NUM
} GSUdpCloseReason;


typedef void (*gsUdpErrorCallback)(GSUdpErrorCode theCode, void *theUserData);


typedef void (*gsUdpAppConnectAttemptCallback)(unsigned int theIp, unsigned short thePort, 
											   int theLatency, unsigned char *theInitMsg, 
											   unsigned int theInitMsgLen, void *theUserData);


typedef void (*gsUdpConnClosedCallback)(unsigned int ip, unsigned short port, GSUdpCloseReason reason, 
										void *theUserData);
typedef void (*gsUdpConnReceivedDataCallback)(unsigned int ip, unsigned short port, 
											  unsigned char *message, unsigned int messageLength, 
											  gsi_bool reliable, void *theUserData);
typedef void (*gsUdpConnConnectedCallback)(unsigned int ip, unsigned short port, 
										   GSUdpErrorCode error, gsi_bool rejected, 
										   void *theUserData);
typedef void (*gsUdpConnPingCallback)(unsigned int ip, unsigned short port, unsigned int latency, 
									  void *theUserData);


typedef gsi_bool (*gsUdpUnknownMsgCallback)(unsigned int ip, unsigned short port, 
										unsigned char *message, unsigned int messageLength, 
										void *theUserData);

gsi_bool gsUdpEngineIsInitialized();
GSUdpErrorCode gsUdpEngineInitialize(unsigned short thePort, int theIncomingBufSize, 
									 int theOutgoingBufSize, gsUdpErrorCallback theAppNetworkError, 
									 gsUdpConnConnectedCallback theAppConnected,
									 gsUdpConnClosedCallback theAppClosed,
									 gsUdpConnPingCallback theAppPing,
									 gsUdpConnReceivedDataCallback theAppReceive,
									 gsUdpUnknownMsgCallback theAppUnownMsg,
									 gsUdpAppConnectAttemptCallback theAppConnectAttempt,
									 void *theAppUserData);
GSUdpErrorCode gsUdpEngineThink();
GSUdpErrorCode gsUdpEngineShutdown();

GSUdpErrorCode gsUdpEngineGetPeerState(unsigned int theIp, unsigned short thePort, 
									   GSUdpPeerState *thePeerState);
GSUdpErrorCode gsUdpEngineStartTalkingToPeer(unsigned int theIp, unsigned short thePort,
											 char theInitMsg[GS_UDP_MSG_HEADER_LEN], int timeOut);
GSUdpErrorCode gsUdpEngineAcceptPeer(unsigned int theIp, unsigned short thePort);
GSUdpErrorCode gsUdpEngineRejectPeer(unsigned int theIp, unsigned short thePort);

GSUdpErrorCode gsUdpEngineSendMessage(unsigned int theIp, unsigned short thePort, 
									  char theHeader[GS_UDP_MSG_HEADER_LEN], unsigned char *theMsg, 
									  unsigned int theMsgLen, gsi_bool theReliable);

GSUdpErrorCode gsUdpEngineAddMsgHandler(char theInitMsg[GS_UDP_MSG_HEADER_LEN], 
										char theHeader[GS_UDP_MSG_HEADER_LEN], 
										gsUdpErrorCallback theMsgHandlerError, 
										gsUdpConnConnectedCallback theMsgHandlerConnected, 
										gsUdpConnClosedCallback theMsgHandlerClosed, 
										gsUdpConnPingCallback theMsgHandlerPing,
										gsUdpConnReceivedDataCallback theMsgHandlerRecv,
										void *theUserData);
GSUdpErrorCode gsUdpEngineRemoveMsgHandler(char theHeader[GS_UDP_MSG_HEADER_LEN]);
SOCKET gsUdpEngineGetSocket();
void gsUdpEngineAddrToString(unsigned int theIp, unsigned short thePort, 
							 char addrstring[GS_IP_ADDR_AND_PORT]);
unsigned int gsUdpEngineGetLocalAddr();
unsigned short gsUdpEngineGetLocalPort();

gsi_bool gsUdpEngineNoMoreMsgHandlers();
gsi_bool gsUdpEngineNoApp();

int gsUdpEngineGetPeerOutBufferFreeSpace(unsigned int theIp, unsigned short thePort);

#endif
