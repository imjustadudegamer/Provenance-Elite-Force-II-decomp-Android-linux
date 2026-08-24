////////////////////////////////////////////////////////////////////////////////
#include "gsUdpEngine.h"



typedef struct  
{
	unsigned int mAddr;
	unsigned short mPort;
	GT2Connection mConnection;
} GSUdpRemotePeer;

typedef struct 
{
	unsigned char mInitialMsg[GS_UDP_MSG_HEADER_LEN];
	unsigned char mHeader[GS_UDP_MSG_HEADER_LEN];
	DArray mPendingConnections;
	gsUdpConnClosedCallback mClosed;
	gsUdpConnReceivedDataCallback mReceived;
	gsUdpConnConnectedCallback mConnected;
	gsUdpConnPingCallback mPingReply;
	gsUdpErrorCallback mNetworkError;
	void *mUserData;
} GSUdpMsgHandler;

typedef struct 
{	
	GT2Socket mSocket;
	DArray mRemotePeers;
	DArray mMsgHandlers;
	gsi_bool mInitialized;
	gsUdpConnConnectedCallback mAppConnected;
	gsUdpConnClosedCallback mAppClosed; 
	gsUdpConnPingCallback mAppPingReply;
	gsUdpConnReceivedDataCallback mAppRecvData;
	gsUdpAppConnectAttemptCallback mAppConnAttempt;
	gsUdpErrorCallback mAppNetworkError;
	gsUdpUnknownMsgCallback mAppUnknownMessage;
	void *mAppUserData;
	
	int mAppPendingConnections;
	unsigned int mLocalAddr;
	unsigned short mLocalPort;
}GSUdpEngineObject;


GSUdpEngineObject * gsUdpEngineGetEngine()
{
	static GSUdpEngineObject aUdpObject;
	return &aUdpObject;
}


void gsUdpMsgHandlerFree(void *theMsgHandler)
{
	GSUdpMsgHandler *aHandler= (GSUdpMsgHandler *)theMsgHandler;
	ArrayFree(aHandler->mPendingConnections);
}

int gsUdpMsgHandlerCompare(const void *theFirstHandler, const void *theSecondHandler)
{
	GSUdpMsgHandler *msgHandler1 = (GSUdpMsgHandler *)theFirstHandler, 
                    *msgHandler2 = (GSUdpMsgHandler *)theSecondHandler;
	int initCmp;
	initCmp = memcmp(msgHandler1->mInitialMsg, msgHandler2->mInitialMsg, GS_UDP_MSG_HEADER_LEN);
	return initCmp;

}

int gsUdpMsgHandlerCompare2(const void *theFirstHandler, const void *theSecondHandler)
{
	GSUdpMsgHandler *msgHandler1 = (GSUdpMsgHandler *)theFirstHandler, 
                    *msgHandler2 = (GSUdpMsgHandler *)theSecondHandler;
	int headerCmp;
	headerCmp = memcmp(msgHandler1->mHeader, msgHandler2->mHeader, GS_UDP_MSG_HEADER_LEN);
	return headerCmp;
}



int gsUdpRemotePeerCompare(const void *theFirstPeer, const void *theSecondPeer)
{
	GSUdpRemotePeer *aPeer1 = (GSUdpRemotePeer *)theFirstPeer,
					*aPeer2 = (GSUdpRemotePeer *)theSecondPeer;
	if (aPeer1->mAddr != aPeer2->mAddr)
		return 1;
	if (aPeer1->mPort != aPeer2->mPort)
		return 1;
	
	return 0;
}

int gsUdpRemotePeerCompare2(const void *theFirstPeer, const void *theSecondPeer)
{
	GSUdpRemotePeer *aPeer1 = (GSUdpRemotePeer *)theFirstPeer,
		*aPeer2 = (GSUdpRemotePeer *)theSecondPeer;
	if (aPeer1->mConnection != aPeer2->mConnection)
		return 1;
	return 0;
}

void gsUdpSocketError(GT2Socket theSocket)
{
	int i, len;
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	
	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
		"[Udp Engine] Socket error, passing to app and message handlers\n");
	if (aUdp->mAppNetworkError)
		aUdp->mAppNetworkError(GS_UDP_NETWORK_ERROR, aUdp->mAppUserData);
	len = ArrayLength(aUdp->mMsgHandlers);
	for (i = 0; i < len; i++)
	{
		GSUdpMsgHandler *aMsgHandler = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, i);
		if (aMsgHandler->mNetworkError)
			aMsgHandler->mNetworkError(GS_UDP_NETWORK_ERROR, aMsgHandler->mUserData);
	}
	GSI_UNUSED(theSocket);
}

void gsUdpClosedRoutingCB(GT2Connection theConnection, GT2CloseReason reason)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GSUdpRemotePeer aRemotePeer;
	int index, len;
	GSUdpCloseReason aReason;
	char anAddr[GS_IP_ADDR_AND_PORT];

	if (reason == GT2CommunicationError || reason == GT2SocketError)
		aReason = GS_UDP_CLOSED_BY_COMM_ERROR;
	else if (reason == GT2NotEnoughMemory)
		aReason = GS_UDP_CLOSED_BY_LOW_MEM;
	else 
		aReason = (GSUdpCloseReason)reason;

	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
		"[Udp Engine] Connection closed to %s\n", gt2AddressToString(gt2GetRemoteIP(theConnection), 
		gt2GetRemotePort(theConnection), anAddr));
	len = ArrayLength(aUdp->mMsgHandlers);
	for (index = 0; index < len; index++)
	{
		GSUdpMsgHandler *aHandler = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, index);
		if (aHandler->mClosed)
		{			
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
				"[Udp Engine] Connection closed: passed to message handler\n");
			aHandler->mClosed(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), aReason, aHandler->mUserData);
		}	
	}
	
	if (aUdp->mAppClosed)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
			"[Udp Engine] Connection closed: passed to app\n");
		aUdp->mAppClosed(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), aReason, aUdp->mAppUserData);
	}

	aRemotePeer.mConnection = theConnection;
	index = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare2, 0, 0);
	if (index != NOT_FOUND)
	{
		ArrayDeleteAt(aUdp->mRemotePeers, index);
	}
	GSI_UNUSED(anAddr);
}

void gsUdpConnectedRoutingCB(GT2Connection theConnection, GT2Result theResult, GT2Byte *theMessage,
							 int theMessageLen)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	int aIndex, len;
	GSUdpErrorCode aCode;
	char anAddr[GS_IP_ADDR_AND_PORT];

	switch(theResult)
	{
		case GT2NegotiationError:
			aCode = GS_UDP_REMOTE_ERROR;
			break;
		case GT2Rejected:
			aCode = GS_UDP_REJECTED;
			break;
		case GT2TimedOut:
			aCode = GS_UDP_TIMED_OUT;
			break;
		case GT2Success:
			aCode = GS_UDP_NO_ERROR;
			break;
		default: 
			aCode = GS_UDP_UNKNOWN_ERROR;
			break;
	}
	if (theResult == GT2Rejected)
	{
		int aRemotePeerIdx;
		GSUdpRemotePeer aRemotePeer;
		aRemotePeer.mAddr = gt2GetRemoteIP(theConnection);
		aRemotePeer.mPort = gt2GetRemotePort(theConnection);
		aRemotePeerIdx = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
		if (aRemotePeerIdx != NOT_FOUND)
		{
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
				"[Udp Engine] Connect rejected by %s\n", gt2AddressToString(gt2GetRemoteIP(theConnection), 
				gt2GetRemotePort(theConnection), anAddr));

			ArrayDeleteAt(aUdp->mRemotePeers, aRemotePeerIdx);
		}
	}

	len = ArrayLength(aUdp->mMsgHandlers);
	for (aIndex = 0; aIndex < len; aIndex++)
	{
		int aRemotePeerIdx;
		GSUdpRemotePeer aRemotePeer;
		GSUdpMsgHandler *aTempHandler = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, aIndex);
		
		aRemotePeer.mAddr = gt2GetRemoteIP(theConnection);
		aRemotePeer.mPort = gt2GetRemotePort(theConnection);
		aRemotePeerIdx = ArraySearch(aTempHandler->mPendingConnections, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
		if (aRemotePeerIdx != NOT_FOUND)
		{
			if (aTempHandler->mConnected)
			{	
				gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
					"[Udp Engine] Passing connect result to message handler\n");
				aTempHandler->mConnected(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), 
					aCode, theResult == GT2Rejected ? gsi_true : gsi_false, aTempHandler->mUserData);				
			}
			ArrayDeleteAt(aTempHandler->mPendingConnections, aRemotePeerIdx);
			return;
		}
	}
	
	if (aUdp->mAppPendingConnections > 0)
	{
		if (aUdp->mAppConnected)
		{
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
				"[Udp Engine] Passing connect result to app\n");
			aUdp->mAppConnected(gt2GetRemoteIP(theConnection),gt2GetRemotePort(theConnection), 
				aCode, theResult == GT2Rejected ? gsi_true : gsi_false, aUdp->mAppUserData);
		}
		aUdp->mAppPendingConnections--;
	}
	GSI_UNUSED(theMessage);
	GSI_UNUSED(theMessageLen);
	GSI_UNUSED(anAddr);
}

void gsUdpPingRoutingCB(GT2Connection theConnection, int theLatency)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	int index, len;
	char anAddr[GS_IP_ADDR_AND_PORT];

	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
		"[Udp Engine] Received ping from %s\n", gt2AddressToString(gt2GetRemoteIP(theConnection), 
		gt2GetRemotePort(theConnection), anAddr));
	len = ArrayLength(aUdp->mMsgHandlers);
	for (index = 0; index < len; index++)
	{
		GSUdpMsgHandler *aHandler = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, index);
		if (aHandler->mPingReply)
		{			
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
				"[Udp Engine] Passed to message handler\n");
			aHandler->mPingReply(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), (unsigned int)theLatency, aHandler->mUserData);
			return;
		}	
	}

	if (aUdp->mAppPingReply)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
			"[Udp Engine] Passed to app\n");
		aUdp->mAppPingReply(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), (unsigned int)theLatency, aUdp->mAppUserData);
	}
	GSI_UNUSED(anAddr);
}

void gsUdpReceivedRoutingCB(GT2Connection theConnection, GT2Byte *theMessage, int theMessageLen, 
							GT2Bool reliable)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GSUdpMsgHandler aHandler;
	int index;
	char anAddr[GS_IP_ADDR_AND_PORT];	

	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
		"[Udp Engine] Received data from %s\n", gt2AddressToString(gt2GetRemoteIP(theConnection), 
		gt2GetRemotePort(theConnection), anAddr));
	if (theMessageLen >= GS_UDP_MSG_HEADER_LEN)
	{    
		memcpy(aHandler.mHeader, theMessage, GS_UDP_MSG_HEADER_LEN);

		index = ArraySearch(aUdp->mMsgHandlers, &aHandler, gsUdpMsgHandlerCompare2, 0, 0);
		if (index != NOT_FOUND)
		{
			GSUdpMsgHandler *aHandlerFound = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, index);
			if (aHandlerFound->mReceived)
			{
				gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
					"[Udp Engine] Passed to message handler\n");
				aHandlerFound->mReceived(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), 
					theMessage + GS_UDP_MSG_HEADER_LEN, (unsigned int)(theMessageLen - GS_UDP_MSG_HEADER_LEN), reliable, aHandlerFound->mUserData);
				return;
			}
		}
	}

	if (aUdp->mAppRecvData) 
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
			"[Udp Engine] Passed to app\n");
		aUdp->mAppRecvData(gt2GetRemoteIP(theConnection), gt2GetRemotePort(theConnection), theMessage, (unsigned int)theMessageLen, reliable,
			aUdp->mAppUserData);
	}
	GSI_UNUSED(anAddr);
}


GT2Bool gsUdpUnrecognizedMsgCB(GT2Socket theSocket, unsigned int theIp, unsigned short thePort, GT2Byte * theMessage, 
							   int theMsgLen)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	char anAddr[GS_IP_ADDR_AND_PORT];
	if (aUdp->mAppUnknownMessage)
	{
		gsi_bool aRet;
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
			"[Udp Engine] Unknown message from %s, passing to app\n", gt2AddressToString(theIp, thePort, anAddr));
		aRet = aUdp->mAppUnknownMessage(theIp, thePort, (unsigned char *)theMessage, (unsigned int)theMsgLen, aUdp->mAppUserData);
		return aRet ? GT2True : GT2False;
	}
	
	GSI_UNUSED(theSocket);
	GSI_UNUSED(anAddr);
	return GT2False;
}

void gsUdpConnAttemptCB(GT2Socket socket, GT2Connection connection, unsigned int ip, 
						unsigned short port, int latency, GT2Byte * message, int len)
{
	int index;
	GSUdpMsgHandler aHandler;
	GSUdpRemotePeer aRemotePeer;
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	char anAddr[GS_IP_ADDR_AND_PORT];
	
	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
		"[Udp Engine] Connection attempt from %s\n", gt2AddressToString(ip, port, anAddr));
	if (len >= GS_UDP_MSG_HEADER_LEN)
	{    
		memcpy(aHandler.mInitialMsg, message, GS_UDP_MSG_HEADER_LEN);
		
		aRemotePeer.mAddr = ip;
		aRemotePeer.mPort = port;
		aRemotePeer.mConnection = connection;
		
		ArrayAppend(aUdp->mRemotePeers, &aRemotePeer);
		index = ArraySearch(aUdp->mMsgHandlers, &aHandler, gsUdpMsgHandlerCompare, 0, 0);
		if (index != NOT_FOUND)
		{
			GT2ConnectionCallbacks aCallbacks;
			
			aCallbacks.closed = gsUdpClosedRoutingCB;
			aCallbacks.connected = gsUdpConnectedRoutingCB;
			aCallbacks.ping = gsUdpPingRoutingCB;
			aCallbacks.received = gsUdpReceivedRoutingCB;

	       
			gt2Accept(aRemotePeer.mConnection, &aCallbacks);	
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
				"[Udp Engine] Connection attempt auto-accepted for message handler\n");
			return;
		}
	}
	if (aUdp->mAppConnAttempt) 
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment, 
			"[Udp Engine] Connection attempt from %s, asking app to accept/reject\n", gt2AddressToString(ip, port, anAddr));
		aUdp->mAppConnAttempt(ip, port, latency, (unsigned char *)message, (unsigned int)len, aUdp->mAppUserData);
	}
	else 
	{
		gt2Reject(connection, NULL, 0);
		ArrayRemoveAt(aUdp->mRemotePeers, ArrayLength(aUdp->mRemotePeers) -1);
	}
	GSI_UNUSED(socket);
	GSI_UNUSED(anAddr);
}




gsi_bool gsUdpEngineIsInitialized()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	return aUdp->mInitialized;
}

GSUdpErrorCode gsUdpEngineInitialize(unsigned short thePort, int theIncomingBufSize, 
									 int theOutgoingBufSize, gsUdpErrorCallback theAppNetworkError, 
									 gsUdpConnConnectedCallback theAppConnected,
									 gsUdpConnClosedCallback theAppClosed,
									 gsUdpConnPingCallback theAppPing,
									 gsUdpConnReceivedDataCallback theAppReceive,
									 gsUdpUnknownMsgCallback theAppUnownMsg,
									 gsUdpAppConnectAttemptCallback theAppConnectAttempt,
									 void *theAppUserData)
{
	int incomingBufferSize,
		outgoingBufferSize;
	char anAddr[GS_IP_ADDR_AND_PORT];
	GT2Result aGt2Result;
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	
	
	incomingBufferSize = theIncomingBufSize != 0 ? theIncomingBufSize : GS_UDP_DEFAULT_IN_BUFFSIZE;
	outgoingBufferSize = theOutgoingBufSize != 0 ? theOutgoingBufSize : GS_UDP_DEFAULT_OUT_BUFFSIZE;

	aUdp->mAppNetworkError = theAppNetworkError;
	aUdp->mAppUnknownMessage = theAppUnownMsg;
	aUdp->mAppConnected = theAppConnected;
	aUdp->mAppClosed = theAppClosed;
	aUdp->mAppPingReply = theAppPing;
	aUdp->mAppRecvData = theAppReceive;
	aUdp->mAppConnAttempt = theAppConnectAttempt;

	gt2AddressToString(0, thePort, anAddr);
	aGt2Result = gt2CreateSocket(&aUdp->mSocket, anAddr,  outgoingBufferSize, incomingBufferSize, 
		gsUdpSocketError);
	if (aGt2Result != GT2Success)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_WarmError, 
			"[Udp Engine] error creating gt2 socket, error code: %d\n", aGt2Result);
		return GS_UDP_NETWORK_ERROR;
	}	
	aUdp->mRemotePeers = ArrayNew(sizeof(GSUdpRemotePeer), 1, NULL);
	if (aUdp->mRemotePeers == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError, 
			"[Udp Engine] No more memory!!!\n");
		return GS_UDP_NO_MEMORY;
	}

	aUdp->mMsgHandlers = ArrayNew(sizeof(GSUdpMsgHandler), 1, gsUdpMsgHandlerFree);
	if (aUdp->mMsgHandlers == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError, 
			"[Udp Engine] No more memory!!!\n");
		return GS_UDP_NO_MEMORY;
	}
		
	gt2SetUnrecognizedMessageCallback(aUdp->mSocket, gsUdpUnrecognizedMsgCB);
	
	gt2Listen(aUdp->mSocket, gsUdpConnAttemptCB);
	aUdp->mLocalAddr = gt2GetLocalIP(aUdp->mSocket);
	aUdp->mLocalPort = gt2GetLocalPort(aUdp->mSocket);
	aUdp->mAppPendingConnections = 0;
	aUdp->mInitialized = gsi_true;
	if (theAppUserData)
	{
		aUdp->mAppUserData = theAppUserData;
	}
	else
		aUdp->mAppUserData = NULL;
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineGetPeerState(unsigned int theIp, unsigned short thePort, GSUdpPeerState *thePeerState)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GSUdpRemotePeer aPeer, *aPeerFound;
	int index;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theIp);
	GS_ASSERT(thePort);
	GS_ASSERT(thePeerState != NULL);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_State, GSIDebugLevel_Debug, 
			"[Udp Engine] Engine not initialized\n");
		*thePeerState = GS_UDP_PEER_CLOSED;
		return GS_UDP_NOT_INITIALIZED;
	}

	aPeer.mAddr = theIp;
	aPeer.mPort = thePort;
	index = ArraySearch(aUdp->mRemotePeers, &aPeer, gsUdpRemotePeerCompare, 0, 0);
	if (index == NOT_FOUND)
	{
		*thePeerState = GS_UDP_PEER_CLOSED;
		return GS_UDP_NO_ERROR;
	}
	
	aPeerFound = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, index);

	*thePeerState = (GSUdpPeerState)gt2GetConnectionState(aPeerFound->mConnection);
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineStartTalkingToPeer(unsigned int theIp, unsigned short thePort,
									  char theInitMsg[GS_UDP_MSG_HEADER_LEN], int timeOut)
{
	char anAddr[GS_IP_ADDR_AND_PORT];
	GSUdpRemotePeer aRemotePeer;	
	GSUdpMsgHandler aHandler;
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GT2ConnectionCallbacks aCallbacks;
	int index;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theIp);
	GS_ASSERT(thePort);

	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}
	
	if (theIp == 0 || thePort == 0)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Invalid parameter(s), check ip, port");
		return GS_UDP_PARAMETER_ERROR;
	}
	
	aRemotePeer.mAddr = theIp;  
	aRemotePeer.mPort = thePort;  

	index = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
	if (index != NOT_FOUND)
	{
		GSUdpRemotePeer *aPeerFound = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, index);
		GT2ConnectionState aState = gt2GetConnectionState(aPeerFound->mConnection);
		if (aState == GT2Connected)
		{
			gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
				"[Udp Engine] Engine is already talking to remote address\n");
			return GS_UDP_ADDRESS_ALREADY_IN_USE;
		}
		else if (aState == GT2Connecting)
		{
			memcpy(aHandler.mInitialMsg, theInitMsg, GS_UDP_MSG_HEADER_LEN);
			
			index = ArraySearch(aUdp->mMsgHandlers, &aHandler, gsUdpMsgHandlerCompare, 0, 0);
			if (index != NOT_FOUND)
			{
				GSUdpMsgHandler *aHandlerFound = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, index);
				ArrayAppend(aHandlerFound->mPendingConnections, aPeerFound);
			}
		}
	}	
	else
	{
		gt2AddressToString(theIp, thePort, anAddr);
		aCallbacks.closed = gsUdpClosedRoutingCB;
		aCallbacks.connected = gsUdpConnectedRoutingCB;
		aCallbacks.ping = gsUdpPingRoutingCB;
		aCallbacks.received = gsUdpReceivedRoutingCB;

		gt2Connect(aUdp->mSocket, &aRemotePeer.mConnection, anAddr, (unsigned char *)theInitMsg, GS_UDP_MSG_HEADER_LEN, timeOut, &aCallbacks, GT2False);

		ArrayAppend(aUdp->mRemotePeers, &aRemotePeer);
		
		memcpy(aHandler.mInitialMsg, theInitMsg, GS_UDP_MSG_HEADER_LEN);
		
		index = ArraySearch(aUdp->mMsgHandlers, &aHandler, gsUdpMsgHandlerCompare, 0, 0);
		if (index != NOT_FOUND)
		{
			GSUdpRemotePeer *aRemotePeerPtr = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, ArrayLength(aUdp->mRemotePeers) - 1);
			GSUdpMsgHandler *aHandlerFound = (GSUdpMsgHandler *)ArrayNth(aUdp->mMsgHandlers, index);
			ArrayAppend(aHandlerFound->mPendingConnections, &aRemotePeerPtr);
		}
		else
		{
			aUdp->mAppPendingConnections++;
		}
	}
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineAcceptPeer(unsigned int theIp, unsigned short thePort)
{	
	GSUdpRemotePeer aRemotePeer;	
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	int index;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theIp);
	GS_ASSERT(thePort);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}

	if (theIp == 0 || thePort == 0)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Invalid parameter(s), check ip, port\n");
		return GS_UDP_PARAMETER_ERROR;
	}

	aRemotePeer.mAddr = theIp;
	aRemotePeer.mPort = thePort;
	index = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
	if (index != NOT_FOUND)
	{
		GT2ConnectionCallbacks aCallbacks;
		
		GSUdpRemotePeer *aPeerFound = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, index);
		
		aCallbacks.closed = gsUdpClosedRoutingCB;
		aCallbacks.connected = gsUdpConnectedRoutingCB;
		aCallbacks.ping = gsUdpPingRoutingCB;
		aCallbacks.received = gsUdpReceivedRoutingCB;
		
		gt2Accept(aPeerFound->mConnection, &aCallbacks);
	}
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineRejectPeer(unsigned int theIp, unsigned short thePort)
{
	GSUdpRemotePeer aRemotePeer;	
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	int index;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theIp);
	GS_ASSERT(thePort);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}

	if (theIp == 0 || thePort == 0)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Invalid parameter(s), check ip, port\n");
		return GS_UDP_PARAMETER_ERROR;
	}

	aRemotePeer.mAddr = theIp;
	aRemotePeer.mPort = thePort;
	index = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
	if (index != NOT_FOUND)
	{
		GSUdpRemotePeer *aPeerFound = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, index);
		gt2Reject(aPeerFound->mConnection, NULL, 0);
		ArrayDeleteAt(aUdp->mRemotePeers, index);
	}
	return GS_UDP_NO_ERROR;
}


// WARNING: Messages should not be greater than the outgoing buffer size minus the header 
GSUdpErrorCode gsUdpEngineSendMessage(unsigned int theIp, unsigned short thePort, 
									  char theHeader[GS_UDP_MSG_HEADER_LEN], unsigned char *theMsg, 
									  unsigned int theMsgLen, gsi_bool theReliable)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	int aTotalMessageLen, index;
	GSUdpRemotePeer aRemotePeer, *aRemotePeerFound;
	GT2Byte *fullMessage;
	GT2Result aResult;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theIp);
	GS_ASSERT(thePort);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}

	if (!theHeader[0])
		aTotalMessageLen = (int)theMsgLen;
	else 
		aTotalMessageLen = (int)(GS_UDP_MSG_HEADER_LEN + theMsgLen);

	aRemotePeer.mAddr = theIp;
	aRemotePeer.mPort = thePort;

	index = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
	if (index == NOT_FOUND)
	{
		char anAddr[GS_IP_ADDR_AND_PORT];
		gt2AddressToString(theIp, thePort, anAddr);
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"[Udp Engine] address not found for sending message\n", anAddr);
		return GS_UDP_ADDRESS_ERROR;
	}
	else
	{
		aRemotePeerFound = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, index);
	}

	if (aTotalMessageLen > gt2GetOutgoingBufferSize(aRemotePeerFound->mConnection) && theReliable)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_WarmError,
			"[Udp Engine] Message Size too large, dropping message\n");
		return GS_UDP_MSG_TOO_BIG;
	}

	fullMessage = (GT2Byte *)gsimalloc((unsigned long)aTotalMessageLen);
	memcpy(fullMessage, theHeader, GS_UDP_MSG_HEADER_LEN);
	memcpy(fullMessage + GS_UDP_MSG_HEADER_LEN, theMsg, theMsgLen);
	aResult = gt2Send(aRemotePeerFound->mConnection, fullMessage, aTotalMessageLen, theReliable);
	gsifree(fullMessage);

	if (aResult != GT2Success)
		return GS_UDP_SEND_FAILED;
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineThink()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}
	gt2Think(aUdp->mSocket);
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineShutdown()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}
	gt2CloseSocket(aUdp->mSocket);	
	ArrayFree(aUdp->mMsgHandlers);
	ArrayFree(aUdp->mRemotePeers);
	aUdp->mInitialized = gsi_false;
	return GS_UDP_NO_ERROR;
}


SOCKET gsUdpEngineGetSocket()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();

	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_HotError,
			"[Udp Engine] Engine not initialized\n");
		return INVALID_SOCKET;
	}

	return gt2GetSocketSOCKET(aUdp->mSocket);
}


unsigned int gsUdpEngineGetLocalAddr()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();

	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_HotError,
			"[Udp Engine] Engine not initialized\n");
		return (unsigned int)-1;
	}
	return aUdp->mLocalAddr;
	
}


unsigned short gsUdpEngineGetLocalPort()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();

	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_HotError,
			"[Udp Engine] Engine not initialized\n");
		return 0;
	}
	return aUdp->mLocalPort;
}


GSUdpErrorCode gsUdpEngineAddMsgHandler(char theInitMsg[GS_UDP_MSG_HEADER_LEN], char theHeader[GS_UDP_MSG_HEADER_LEN], 
									    gsUdpErrorCallback theMsgHandlerError, 
									    gsUdpConnConnectedCallback theMsgHandlerConnected, 
									    gsUdpConnClosedCallback theMsgHandlerClosed, 
									    gsUdpConnPingCallback theMsgHandlerPing,
										gsUdpConnReceivedDataCallback theMsgHandlerRecv,
										void *theUserData)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GSUdpMsgHandler aMsgHandler;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theInitMsg || theInitMsg[0]);
	GS_ASSERT(theHeader || theHeader[0]);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}
	
	
	if (!theInitMsg[0])
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Invalid init message\n");
		return GS_UDP_PARAMETER_ERROR;
	}

	if (!theHeader[0])
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Invalid header\n");
		return GS_UDP_PARAMETER_ERROR;
	}

	aMsgHandler.mClosed = theMsgHandlerClosed;
	aMsgHandler.mConnected = theMsgHandlerConnected;
	aMsgHandler.mPingReply = theMsgHandlerPing;
	aMsgHandler.mReceived = theMsgHandlerRecv;
	
	aMsgHandler.mNetworkError = theMsgHandlerError;

	memcpy(aMsgHandler.mInitialMsg, theInitMsg, GS_UDP_MSG_HEADER_LEN);
	memcpy(aMsgHandler.mHeader, theHeader, GS_UDP_MSG_HEADER_LEN);
		
	aMsgHandler.mPendingConnections = ArrayNew(sizeof(GSUdpRemotePeer *), 1, NULL);
	if (aMsgHandler.mPendingConnections == NULL)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Memory, GSIDebugLevel_HotError, 
			"[Udp Engine] No more memory!!!\n");
		return GS_UDP_NO_MEMORY;
	}
	aMsgHandler.mUserData = theUserData;
	ArrayAppend(aUdp->mMsgHandlers, &aMsgHandler);
	
	return GS_UDP_NO_ERROR;
}


GSUdpErrorCode gsUdpEngineRemoveMsgHandler(char theHeader[GS_UDP_MSG_HEADER_LEN])
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GSUdpMsgHandler aHandler;
	int index;
	
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theHeader);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return GS_UDP_NETWORK_ERROR;
	}

	if (!theHeader || !theHeader[0])
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] invalid or empty header\n");
		return GS_UDP_PARAMETER_ERROR;
	}
	
	memcpy(aHandler.mHeader, theHeader, GS_UDP_MSG_HEADER_LEN);

	index = ArraySearch(aUdp->mMsgHandlers, &aHandler, gsUdpMsgHandlerCompare2, 0, 0);
	if (index != NOT_FOUND)
	{
		ArrayDeleteAt(aUdp->mMsgHandlers, index);
	}
	return GS_UDP_NO_ERROR;
}


gsi_bool gsUdpEngineNoMoreMsgHandlers()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	
	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return gsi_true;
	}
	
	return ArrayLength(aUdp->mMsgHandlers) == 0 ? gsi_true : gsi_false;
}


gsi_bool gsUdpEngineNoApp()
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();

	GS_ASSERT(aUdp->mInitialized);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return gsi_true;
	}

	if (aUdp->mAppClosed || aUdp->mAppConnAttempt || aUdp->mAppConnected || aUdp->mAppNetworkError 
		|| aUdp->mAppPingReply || aUdp->mAppRecvData || aUdp->mAppUnknownMessage)
	{
		return gsi_false;
	}

	return gsi_true;
}


int gsUdpEngineGetPeerOutBufferFreeSpace(unsigned int theIp, unsigned short thePort)
{
	GSUdpEngineObject *aUdp = gsUdpEngineGetEngine();
	GSUdpRemotePeer aRemotePeer, *aRemotePeerFound;
	int index;
	GS_ASSERT(aUdp->mInitialized);
	GS_ASSERT(theIp);
	GS_ASSERT(thePort);
	if (!aUdp->mInitialized)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Debug,
			"[Udp Engine] Engine not initialized\n");
		return 0;
	}

	aRemotePeer.mAddr = theIp;
	aRemotePeer.mPort = thePort;
	index = ArraySearch(aUdp->mRemotePeers, &aRemotePeer, gsUdpRemotePeerCompare, 0, 0);
	if (index != NOT_FOUND)
	{
		aRemotePeerFound = (GSUdpRemotePeer *)ArrayNth(aUdp->mRemotePeers, index);
		return gt2GetOutgoingBufferFreeSpace(aRemotePeerFound->mConnection);
	}
	return 0;
}

void gsUdpEngineAddrToString(unsigned int theIp, unsigned short thePort, char addrstring[GS_IP_ADDR_AND_PORT])
{
	gt2AddressToString(theIp, thePort, addrstring);
}
