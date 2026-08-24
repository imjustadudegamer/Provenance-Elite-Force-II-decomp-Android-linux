
#include "q_shared.h"
#include "qcommon.h"


#define	MAX_PACKETLEN					10000		
#define	MAX_REMOTE_PACKETLEN			1400		

#define FRAGMENT_SIZE			1300
#define	PACKET_HEADER			10			

#define	FRAGMENT_BIT	(1<<31)

cvar_t		*showpackets;
cvar_t		*showdrop;
cvar_t		*net_qport;

static char *netsrcString[2] = {
	"client",
	"server"
};

void Netchan_Init( int port ) {
	port &= 0xffff;
	showpackets = Cvar_Get ("showpackets", "0", CVAR_TEMP );
	showdrop = Cvar_Get ("showdrop", "0", CVAR_TEMP );
	net_qport = Cvar_Get ("net_qport", va("%i", port), CVAR_INIT );
}

void Netchan_Setup(netsrc_t sock, netchan_t *chan, netadr_t adr, int qport)
{
	Com_Memset (chan, 0, sizeof(*chan));
	
	chan->sock = sock;
	chan->remoteAddress = adr;
	chan->qport = qport;
	chan->incomingSequence = 0;
	chan->outgoingSequence = 1;
}

#if 0
#define	SCRAMBLE_START	6
static void Netchan_ScramblePacket( msg_t *buf ) {
	unsigned	seed;
	int			i, j, c, mask, temp;
	int			seq[MAX_PACKETLEN];

	seed = ( LittleLong( *(unsigned *)buf->data ) * 3 ) ^ ( buf->cursize * 123 );
	c = buf->cursize;
	if ( c <= SCRAMBLE_START ) {
		return;
	}
	if ( c > MAX_PACKETLEN ) {
		Com_Error( ERR_DROP, "MAX_PACKETLEN" );
	}

	for (i = 0 ; i < c ; i++) {
		seed = (119 * seed + 1);
		seq[i] = seed;
	}

	for ( mask = 1 ; mask < c-SCRAMBLE_START ; mask = ( mask << 1 ) + 1 ) {
	}
	mask >>= 1;
	for (i = SCRAMBLE_START ; i < c ; i++) {
		j = SCRAMBLE_START + ( seq[i] & mask );
		temp = buf->data[j];
		buf->data[j] = buf->data[i];
		buf->data[i] = temp;
	}

	for (i = SCRAMBLE_START ; i < c ; i++) {
		buf->data[i] ^= seq[i];
	}
}

static void Netchan_UnScramblePacket( msg_t *buf ) {
	unsigned	seed;
	int			i, j, c, mask, temp;
	int			seq[MAX_PACKETLEN];

	seed = ( LittleLong( *(unsigned *)buf->data ) * 3 ) ^ ( buf->cursize * 123 );
	c = buf->cursize;
	if ( c <= SCRAMBLE_START ) {
		return;
	}
	if ( c > MAX_PACKETLEN ) {
		Com_Error( ERR_DROP, "MAX_PACKETLEN" );
	}

	for (i = 0 ; i < c ; i++) {
		seed = (119 * seed + 1);
		seq[i] = seed;
	}

	for (i = SCRAMBLE_START ; i < c ; i++) {
		buf->data[i] ^= seq[i];
	}

	for ( mask = 1 ; mask < c-SCRAMBLE_START ; mask = ( mask << 1 ) + 1 ) {
	}
	mask >>= 1;
	for (i = c-1 ; i >= SCRAMBLE_START ; i--) {
		j = SCRAMBLE_START + ( seq[i] & mask );
		temp = buf->data[j];
		buf->data[j] = buf->data[i];
		buf->data[i] = temp;
	}
}
#endif

int Netchan_TransmitNextFragment( netchan_t *chan ) {
	msg_t		send;
	byte		send_buf[1400];
	size_t		fragmentLength;

	MSG_InitOOB (&send, send_buf, sizeof( send_buf ) );

	MSG_WriteLong( &send, chan->outgoingSequence | FRAGMENT_BIT );

	if ( chan->sock == NS_CLIENT ) {
		MSG_WriteShort( &send, net_qport->integer );
    }


	fragmentLength = FRAGMENT_SIZE;
	if ( chan->unsentFragmentStart  + fragmentLength > chan->unsentLength ) {
		fragmentLength = chan->unsentLength - chan->unsentFragmentStart;
	}

	MSG_WriteLong( &send, (int)chan->unsentFragmentStart );
	MSG_WriteShort( &send, (short)fragmentLength );
	MSG_WriteData( &send, chan->unsentBuffer + chan->unsentFragmentStart, fragmentLength );

	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remoteAddress );


	if ( showpackets->integer ) {
		Com_Printf ("%s send %4zu : s=%i fragment=%i,%zu\n"
			, netsrcString[ chan->sock ]
			, send.cursize
			, chan->outgoingSequence
			, chan->unsentFragmentStart, fragmentLength);
	}


	chan->unsentFragmentStart += fragmentLength;

	if ( chan->unsentFragmentStart == chan->unsentLength && fragmentLength != FRAGMENT_SIZE ) {
		chan->outgoingSequence++;
		chan->unsentFragments = qfalse;
	}

	return send.cursize;
}


int Netchan_Transmit( netchan_t *chan, size_t length, const byte *data ) {
	msg_t			send;
	byte			send_buf[1400];

	if ( length > MAX_MSGLEN ) {
		Com_Error( ERR_DROP, "Netchan_Transmit: length = %i", (int)length );
	}
	chan->unsentFragmentStart = 0;

	if ( length >= FRAGMENT_SIZE ) {
		chan->unsentFragments = qtrue;
		chan->unsentLength = length;
		Com_Memcpy( chan->unsentBuffer, data, length );

		return Netchan_TransmitNextFragment( chan );
	}

	MSG_InitOOB (&send, send_buf, sizeof( send_buf ) );

	MSG_WriteLong( &send, chan->outgoingSequence );

	if(chan->sock == NS_CLIENT)
		MSG_WriteShort(&send, net_qport->integer);


	chan->outgoingSequence++;

	MSG_WriteData( &send, data, length );

	NET_SendPacket( chan->sock, send.cursize, send.data, chan->remoteAddress );

	if ( showpackets->integer ) {
		Com_Printf( "%s send %4zu : s=%i ack=%i\n"
			, netsrcString[ chan->sock ]
			, send.cursize
			, chan->outgoingSequence - 1
			, chan->incomingSequence );
	}

	return send.cursize;
}

qboolean Netchan_Process( netchan_t *chan, msg_t *msg ) {
	int			sequence;
	int			fragmentStart, fragmentLength;
	qboolean	fragmented;


	MSG_BeginReadingOOB( msg );
	sequence = MSG_ReadLong( msg );

	if ( sequence & FRAGMENT_BIT ) {
		sequence &= ~FRAGMENT_BIT;
		fragmented = qtrue;
	} else {
		fragmented = qfalse;
	}

	if ( chan->sock == NS_SERVER ) {
		MSG_ReadShort( msg );		
	}


	if ( fragmented ) {
		fragmentStart = MSG_ReadLong( msg );
		fragmentLength = MSG_ReadShort( msg );
	} else {
		fragmentStart = 0;		
		fragmentLength = 0;
	}

	if ( showpackets->integer ) {
		if ( fragmented ) {
			Com_Printf( "%s recv %4zu : s=%u fragment=%i,%i\n"
				, netsrcString[ chan->sock ]
				, msg->cursize
				, sequence
				, fragmentStart, fragmentLength );
		} else {
			Com_Printf( "%s recv %4zu : s=%i\n"
				, netsrcString[ chan->sock ]
				, msg->cursize
				, sequence );
		}
	}


	if ( sequence <= chan->incomingSequence ) {
		if ( showdrop->integer || showpackets->integer ) {
			Com_Printf( "%s:Out of order packet %i at %i\n"
				, NET_AdrToLogString( chan->remoteAddress )
				,  sequence
				, chan->incomingSequence );
		}
		return qfalse;
	}

	chan->dropped = sequence - (chan->incomingSequence+1);
	if ( chan->dropped > 0 ) {
		if ( showdrop->integer || showpackets->integer ) {
			Com_Printf( "%s:Dropped %i packets at %i\n"
			, NET_AdrToLogString( chan->remoteAddress )
			, chan->dropped
			, sequence );
		}
	}
	

	if ( fragmented ) {
		if ( sequence != chan->fragmentSequence ) {
			chan->fragmentSequence = sequence;
			chan->fragmentLength = 0;
		}

		if ( fragmentStart != chan->fragmentLength ) {
			if ( showdrop->integer || showpackets->integer ) {
				Com_Printf( "%s:Dropped a message fragment\n"
				, NET_AdrToLogString( chan->remoteAddress ));
			}
			return qfalse;
		}

		if ( fragmentLength < 0 || msg->readcount + fragmentLength > msg->cursize ||
			chan->fragmentLength + fragmentLength > sizeof( chan->fragmentBuffer ) ) {
			if ( showdrop->integer || showpackets->integer ) {
				Com_Printf ("%s:illegal fragment length\n"
				, NET_AdrToLogString (chan->remoteAddress ) );
			}
			return qfalse;
		}

		Com_Memcpy( chan->fragmentBuffer + chan->fragmentLength, 
			msg->data + msg->readcount, fragmentLength );

		chan->fragmentLength += fragmentLength;

		if ( fragmentLength == FRAGMENT_SIZE ) {
			return qfalse;
		}

		if ( chan->fragmentLength > msg->maxsize ) {
			Com_Printf( "%s:fragmentLength %i > msg->maxsize\n"
				, NET_AdrToLogString (chan->remoteAddress ),
				chan->fragmentLength );
			return qfalse;
		}


		*(int *)msg->data = LittleLong( sequence );

		Com_Memcpy( msg->data + 4, chan->fragmentBuffer, chan->fragmentLength );
		msg->cursize = chan->fragmentLength + 4;
		chan->fragmentLength = 0;
		msg->readcount = 4;	
		msg->bit = 32;	

		chan->incomingSequence = sequence;
		
		return qtrue;
	}

	chan->incomingSequence = sequence;

	return qtrue;
}


#define	MAX_LOOPBACK	64

typedef struct {
	byte	data[MAX_PACKETLEN];
	size_t	datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];


qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, msg_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return qfalse;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	Com_Memcpy (net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	Com_Memset (net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;
	return qtrue;

}


void NET_SendLoopPacket (netsrc_t sock, size_t length, const void *data, netadr_t to)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	Com_Memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}



void NET_SendPacket( netsrc_t sock, size_t length, const void *data, netadr_t to ) {

	if ( showpackets->integer && *(int *)data == -1 )	{
		Com_Printf( "%s send OOB packet %4zu\n", netsrcString[ sock ], length );
	}

	if ( to.type == NA_LOOPBACK ) {
		NET_SendLoopPacket (sock, length, data, to);
		return;
	}
	if ( to.type == NA_BOT ) {
		return;
	}
	if ( to.type == NA_BAD ) {
		return;
	}

	Sys_SendPacket( length, data, to );
}

void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... ) {
	va_list		argptr;
	char		string[MAX_MSGLEN];


	string[0] = -1;
	string[1] = -1;
	string[2] = -1;
	string[3] = -1;

	va_start( argptr, format );
	Q_vsnprintf( string+4, sizeof(string)-4, format, argptr );
	va_end( argptr );

	NET_SendPacket( sock, strlen( string ), string, adr );
}

void QDECL NET_OutOfBandData( netsrc_t sock, netadr_t adr, byte *format, int len ) {
	byte		string[MAX_MSGLEN*2];
	int			i;
	msg_t		mbuf;

	string[0] = 0xff;
	string[1] = 0xff;
	string[2] = 0xff;
	string[3] = 0xff;

	for(i=0;i<len;i++) {
		string[i+4] = format[i];
	}

	mbuf.data = string;
	mbuf.cursize = len+4;
	Huff_Compress( &mbuf, 12);
	NET_SendPacket( sock, mbuf.cursize, mbuf.data, adr );
}

int NET_StringToAdr( const char *s, netadr_t *a )
{
	char	base[MAX_STRING_CHARS], *search;
	char	*port = NULL;

	if (!strcmp (s, "localhost")) {
		Com_Memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return 1;
	}

	Q_strncpyz( base, s, sizeof( base ) );
	
	if(*base == '[' || Q_CountChar(base, ':') > 1)
	{
		search = strchr(base, ']');
		if(search)
		{
			*search = '\0';
			search++;

			if(*search == ':')
				port = search + 1;
		}
		
		if(*base == '[')
			search = base + 1;
		else
			search = base;
	}
	else
	{
		port = strchr( base, ':' );
		
		if ( port ) {
			*port = '\0';
			port++;
		}
		
		search = base;
	}

	if(!Sys_StringToAdr(search, a))
	{
		a->type = NA_BAD;
		return 0;
	}

	if(port)
	{
		a->port = BigShort((short) atoi(port));
		return 1;
	}
	else
	{
		a->port = BigShort(PORT_SERVER);
		return 2;
	}
}

