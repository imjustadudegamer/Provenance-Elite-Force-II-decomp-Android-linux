/*
GameSpy Peer SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "peerQR.h"
#include "peerRooms.h"

#ifdef __MWERKS__ 
PEERBool piStartHosting (PEER peer, SOCKET socket, unsigned short port);	
void piStopHosting(PEER peer, PEERBool stopReporting);						
#endif

PEERBool piStartHosting
(
	PEER peer,
	SOCKET socket,
	unsigned short port
)
{
	PEER_CONNECTION;

	assert(!connection->hosting);
	if(connection->hosting)
		return PEERFalse;

	connection->hosting = PEERTrue;

	if(!piStartReporting(peer, socket, port))
		return PEERFalse;

	return PEERTrue;
}

void piStopHosting
(
	PEER peer,
	PEERBool stopReporting
)
{
	PEER_CONNECTION;

	if(stopReporting)
		piStopReporting(peer);

	if(!connection->hosting)
		return;

	connection->hosting = PEERFalse;
	connection->playing = PEERFalse;
	connection->ready = PEERFalse;

	piSetLocalFlags(peer);
}
