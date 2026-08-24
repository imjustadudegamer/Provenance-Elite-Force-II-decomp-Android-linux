/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "server.h"

static void SV_Netchan_Encode( client_t *client, msg_t *msg ) {
	int		i, index, srdc, sbit;
	qboolean	soob;
	byte	key, *string;

	if ( msg->cursize < 4 ) {
		return;
	}

	srdc = msg->readcount;
	sbit = msg->bit;
	soob = msg->oob;

	msg->bit = 0;
	msg->readcount = 0;
	msg->oob = qfalse;

	 MSG_ReadLong(msg);

	msg->oob = soob;
	msg->bit = sbit;
	msg->readcount = srdc;

	string = (byte *)client->lastClientCommandString;
	index = 0;
	key = client->challenge ^ client->netchan.outgoingSequence;
	for ( i = 4 ; i < msg->cursize ; i++ ) {
		if (!string[index])
			index = 0;
		if (string[index] > 127 || string[index] == '%') {
			key ^= '.' << (i & 1);
		}
		else {
			key ^= string[index] << (i & 1);
		}
		index++;
		*(msg->data + i) = *(msg->data + i) ^ key;
	}
}

static void SV_Netchan_Decode( client_t *client, msg_t *msg ) {
	int serverId, messageAcknowledge, reliableAcknowledge;
	int i, index, srdc, sbit;
	qboolean soob;
	byte key, *string;

	srdc = msg->readcount;
	sbit = msg->bit;
	soob = msg->oob;

	msg->oob = qfalse;

	serverId = MSG_ReadLong(msg);
	messageAcknowledge = MSG_ReadLong(msg);
	reliableAcknowledge = MSG_ReadLong(msg);

	msg->oob = soob;
	msg->bit = sbit;
	msg->readcount = srdc;

	string = (byte *)client->reliableCommands[ reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ];
	index = 0;
	key = messageAcknowledge ^ client->challenge ^ serverId;
	for ( i = msg->readcount + 12 ; i < msg->cursize ; i++ ) {
		if (!string[index])
			index = 0;
		if (string[index] > 127 || string[index] == '%') {
			key ^= '.' << (i & 1);
		}
		else {
			key ^= string[index] << (i & 1);
		}
		if ( string[index] )
			index++;
		*(msg->data + i) = *(msg->data + i) ^ key;
	}
}




int SV_Netchan_TransmitNextFragment(client_t *client)
{
	return Netchan_TransmitNextFragment(&client->netchan);
}



int SV_Netchan_Transmit( client_t *client, msg_t *msg)
{
	MSG_WriteByte( msg, svc_EOF );

	SV_Netchan_Encode( client, msg );

	return Netchan_Transmit( &client->netchan, msg->cursize, msg->data );
}

qboolean SV_Netchan_Process( client_t *client, msg_t *msg ) {
	if (!Netchan_Process( &client->netchan, msg )) {
		return qfalse;
	}

	SV_Netchan_Decode( client, msg );

	return qtrue;
}

void SV_NET_OutOfBandPrint(netadr_t adr, const char* format, ...) {
    va_list	argptr;
    char	string[MAX_MSGLEN];

    va_start(argptr, format);
    Q_vsnprintf(string, sizeof(string), format, argptr);
    va_end(argptr);

	NET_OutOfBandPrint(NS_SERVER, adr, "%s", string);
}

