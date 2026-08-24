#include "q_shared.h"
#include "qcommon.h"

huffman_t msgHuff;

qboolean msgInit = qfalse;

int oldsize = 0;


void MSG_initHuffman( void );

void MSG_Init( msg_t *buf, byte *data, size_t length ) {
	if (!msgInit) {
		MSG_initHuffman();
	}
	Com_Memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void MSG_InitOOB( msg_t *buf, byte *data, size_t length ) {
	if (!msgInit) {
		MSG_initHuffman();
	}
	Com_Memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->oob = qtrue;
}

void MSG_Clear( msg_t *buf ) {
	buf->cursize = 0;
	buf->overflowed = qfalse;
	buf->bit = 0;					
}

void MSG_Bitstream( msg_t *buf ) {
	buf->oob = qfalse;
}

void MSG_BeginReading( msg_t *msg ) {
	msg->readcount = 0;
	msg->bit = 0;
	msg->oob = qfalse;
}

void MSG_BeginReadingOOB( msg_t *msg ) {
	msg->readcount = 0;
	msg->bit = 0;
	msg->oob = qtrue;
}

void MSG_Copy(msg_t *buf, byte *data, int length, msg_t *src)
{
	if ((size_t)length<src->cursize) {
		Com_Error( ERR_DROP, "MSG_Copy: can't copy into a smaller msg_t buffer");
	}
	Com_Memcpy(buf, src, sizeof(msg_t));
	buf->data = data;
	Com_Memcpy(buf->data, src->data, src->cursize);
}


int	overflows;

void MSG_WriteBits( msg_t *msg, int value, int bits ) {
	int	i;

	oldsize += bits;

	// `if (msg->overflowed) return;` latch) are not in retail: with them the port
	if ( (ptrdiff_t)msg->maxsize - (ptrdiff_t)msg->cursize < 4 ) {
		msg->overflowed = qtrue;
		return;
	}

	if ( bits == 0 || bits < -31 || bits > 32 ) {
		Com_Error( ERR_DROP, "MSG_WriteBits: bad bits %i", bits );
	}

	if ( bits != 32 ) {
		if ( bits > 0 ) {
			if ( value > ( ( 1 << bits ) - 1 ) || value < 0 ) {
				overflows++;
			}
		} else {
			int	r;

			r = 1 << (bits-1);

			if ( value >  r - 1 || value < -r ) {
				overflows++;
			}
		}
	}
	if ( bits < 0 ) {
		bits = -bits;
	}

	if (msg->oob) {
		if (bits == 8) {
			msg->data[msg->cursize] = value;
			msg->cursize += 1;
			msg->bit += 8;
		}
		else if (bits == 16) {
			short temp = value;

			CopyLittleShort(&msg->data[msg->cursize], &temp);
			msg->cursize += 2;
			msg->bit += 16;
		}
		else if (bits == 32) {
			CopyLittleLong(&msg->data[msg->cursize], &value);
			msg->cursize += 4;
			msg->bit += 32;
		}
		else {
			Com_Error(ERR_DROP, "can't write %d bits", bits);
		}
	} else {
		value &= (0xffffffff>>(32-bits));
		if (bits&7) {
			int nbits;
			nbits = bits&7;
			for(i=0;i<nbits;i++) {
				Huff_putBit((value&1), msg->data, &msg->bit);
				value = (value>>1);
			}
			bits = bits - nbits;
		}
		if (bits) {
			for(i=0;i<bits;i+=8) {
				Huff_offsetTransmit( &msgHuff.compressor, (value & 0xff), msg->data, &msg->bit );
				value = (value>>8);
			}
		}
		msg->cursize = (msg->bit>>3)+1;
	}
}

int MSG_ReadBits( msg_t *msg, int bits ) {
	int			value;
	int			get;
	qboolean	sgn;
	int			i, nbits;

	value = 0;

	if ( bits < 0 ) {
		bits = -bits;
		sgn = qtrue;
	} else {
		sgn = qfalse;
	}

	if (msg->oob) {
		if (bits == 8)
		{
			value = msg->data[msg->readcount];
			msg->readcount += 1;
			msg->bit += 8;
		}
		else if (bits == 16)
		{
			short temp;

			CopyLittleShort(&temp, &msg->data[msg->readcount]);
			value = temp;
			msg->readcount += 2;
			msg->bit += 16;
		}
		else if (bits == 32)
		{
			CopyLittleLong(&value, &msg->data[msg->readcount]);
			msg->readcount += 4;
			msg->bit += 32;
		}
		else
			Com_Error(ERR_DROP, "can't read %d bits", bits);
	} else {
		nbits = 0;
		if (bits&7) {
			nbits = bits&7;
			for(i=0;i<nbits;i++) {
				value |= (Huff_getBit(msg->data, &msg->bit)<<i);
			}
			bits = bits - nbits;
		}
		if (bits) {
			for(i=0;i<bits;i+=8) {
				Huff_offsetReceive (msgHuff.decompressor.tree, &get, msg->data, &msg->bit);
				value |= (get<<(i+nbits));
			}
		}
		msg->readcount = (msg->bit>>3)+1;
	}

	if (sgn) {
		if ( value & ( 1 << ( bits - 1 ) ) ) {
			value |= ~(((unsigned int)1 << bits) - 1);
		}
	}

	return value;
}





void MSG_WriteChar( msg_t *sb, int c ) {
#ifdef PARANOID
	if (c < -128 || c > 127)
		Com_Error (ERR_FATAL, "MSG_WriteChar: range error");
#endif

	MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteByte( msg_t *sb, int c ) {
#ifdef PARANOID
	if (c < 0 || c > 255)
		Com_Error (ERR_FATAL, "MSG_WriteByte: range error");
#endif

	MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteData( msg_t *buf, const void *data, size_t length ) {
	int i;
	for(i=0;(size_t)i<length;i++) {
		MSG_WriteByte(buf, ((byte *)data)[i]);
	}
}

void MSG_WriteShort( msg_t *sb, int c ) {
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Com_Error (ERR_FATAL, "MSG_WriteShort: range error");
#endif

	MSG_WriteBits( sb, c, 16 );
}

void MSG_WriteSVC( msg_t *sb, int c ) {
	MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteLong( msg_t *sb, int c ) {
	MSG_WriteBits( sb, c, 32 );
}

void MSG_WriteFloat( msg_t *sb, float f ) {
	union {
		float	f;
		int	l;
	} dat;
	
	dat.f = f;
	MSG_WriteBits( sb, dat.l, 32 );
}

static void MSG_WriteSanitizedString( msg_t *sb, const char *s, size_t bufsize, const char *what ) {
	if ( !s ) {
		MSG_WriteByte( sb, 0 );
		return;
	} else {
		size_t	l;
		size_t	i;
		char	string[BIG_INFO_STRING];

		l = strlen( s );
		if ( l >= bufsize ) {
			Com_Printf( "%s: buffer exceeded", what );
			MSG_WriteByte( sb, 0 );
			return;
		}
		Q_strncpyz( string, s, bufsize );

		for ( i = 0 ; i < l ; i++ ) {
			if ( (unsigned char)string[i] > 127 ) {
				string[i] = '.';
			}
		}

		for ( i = 0 ; i <= l ; i++ ) {
			MSG_WriteByte( sb, (signed char)string[i] );
		}
	}
}

void MSG_WriteString( msg_t *sb, const char *s ) {
	MSG_WriteSanitizedString( sb, s, 1024, "MSG_WriteString: MAX_STRING_CHARS" );
}

void MSG_WriteBigString( msg_t *sb, const char *s ) {
	MSG_WriteSanitizedString( sb, s, BIG_INFO_STRING, "MSG_WriteString: BIG_INFO_STRING" );
}

void MSG_WriteAngle( msg_t *sb, float f ) {
	MSG_WriteByte (sb, (int)(f*256/360) & 255);
}

void MSG_WriteAngle16( msg_t *sb, float f ) {
	MSG_WriteShort (sb, ANGLE2SHORT(f));
}




int MSG_ReadChar (msg_t *msg ) {
	int	c;
	
	c = (signed char)MSG_ReadBits( msg, 8 );
	if ( (size_t)msg->readcount > msg->cursize ) {
		c = -1;
	}	
	
	return c;
}

int MSG_ReadByte( msg_t *msg ) {
	int	c;
	
	c = (unsigned char)MSG_ReadBits( msg, 8 );
	if ( (size_t)msg->readcount > msg->cursize ) {
		c = -1;
	}	
	return c;
}

int MSG_ReadSVC( msg_t *msg ) {
	int	c;
	
	c = (unsigned char)MSG_ReadBits( msg, 8 );
	if ( (size_t)msg->readcount > msg->cursize ) {
		c = -1;
	}	
	return c;
}

int MSG_ReadShort( msg_t *msg ) {
	int	c;
	
	c = (short)MSG_ReadBits( msg, 16 );
	if ( (size_t)msg->readcount > msg->cursize ) {
		c = -1;
	}	

	return c;
}

int MSG_ReadLong( msg_t *msg ) {
	int	c;
	
	c = MSG_ReadBits( msg, 32 );
	if ( (size_t)msg->readcount > msg->cursize ) {
		c = -1;
	}	
	
	return c;
}

void MSG_ReadDir( msg_t *msg, vec3_t dir ) {
	int b;

	b = MSG_ReadByte(msg);
	ByteToDir( b, dir );
}

float MSG_ReadCoord( msg_t *msg ) {
	float	sign = 1.0f;
	int		read;
	float	rtn;

	read = MSG_ReadBits( msg, 19 );
	if ( read & 262144 )	
		sign = -1.0f;
	read &= ~262144; 
	rtn =  sign * read /16.0f;
	
	return rtn;	
}

float MSG_ReadFloat( msg_t *msg ) {
	union {
		byte	b[4];
		float	f;
		int	l;
	} dat;
	
	dat.l = MSG_ReadBits( msg, 32 );
	if ( (size_t)msg->readcount > msg->cursize ) {
		dat.f = -1;
	}	
	
	return dat.f;	
}

#define	MSG_READSTRING_BUFFER		3072
#define	MSG_READSTRINGLINE_BUFFER	1024

char *MSG_ReadString( msg_t *msg ) {
	static char	string[MSG_READSTRING_BUFFER];
	int l;
	int c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		
		if ( c == -1 || c == 0 ) {
			break;
		}
		if ( c == '%' ) {
			c = '.';
		}
		if ( c > 127 ) {
			c = '.';
		}

		string[l] = (char)c;
		l++;
	} while ((size_t)l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

char *MSG_ReadBigString( msg_t *msg ) {
	static char	string[BIG_INFO_STRING];
	int l;
	int c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		
		if ( c == -1 || c == 0 ) {
			break;
		}
		if ( c == '%' ) {
			c = '.';
		}

		string[l] = (char)c;
		l++;
	} while ((size_t)l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

char *MSG_ReadStringLine( msg_t *msg ) {
	static char	string[MSG_READSTRINGLINE_BUFFER];
	int		l,c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		
		if (c == -1 || c == 0 || c == '\n') {
			break;
		}
		if ( c == '%' ) {
			c = '.';
		}

		string[l] = c;
		l++;
	} while ((size_t)l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

void MSG_GetNullEntityState(entityState_t *nullState) {

	Com_Memset( nullState, 0, sizeof(entityState_t) );
	nullState->alpha = 1.0f;
	nullState->scale = 1.0f;
	nullState->parent = ENTITYNUM_NONE;
	nullState->constantLight = 0xffffff;
	nullState->renderfx = 16;
	nullState->bone_tag[4] = -1;
	nullState->bone_tag[3] = -1;
	nullState->bone_tag[2] = -1;
	nullState->bone_tag[1] = -1;
	nullState->bone_tag[0] = -1;
	for (int i = 0; i < NUM_MORPH_CONTROLLERS; i++) {
		nullState->morph_controllers[i].index = -1;
	}

	nullState->animationRate = 1.0f;
	nullState->clientNum     = ENTITYNUM_NONE;
	for (int i = 0; i < NUM_EFFECTS_ANIMS; i++) {
		nullState->effectsAnims[i] = -1;
	}
	nullState->pos.trType = TR_LERP;
}

float MSG_ReadAngle8( msg_t *msg ) {
	return BYTE2ANGLE(MSG_ReadShort(msg));
}

float MSG_ReadAngle16( msg_t *msg ) {
	return SHORT2ANGLE(MSG_ReadShort(msg));
}

void MSG_ReadData( msg_t *msg, void *data, int len ) {
	int		i;

	for (i=0 ; i<len ; i++) {
		((byte *)data)[i] = MSG_ReadByte (msg);
	}
}

int MSG_HashKey(const char *string, int maxlen) {
	int hash, i;

	hash = 0;
	for (i = 0; i < maxlen && string[i] != '\0'; i++) {
		hash += (signed char)string[i] * (119 + i);
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	return hash;
}


extern "C" cvar_t *cl_shownet;

#define	LOG(x) if( cl_shownet->integer == 4 ) { Com_Printf("%s ", x ); };

void MSG_WriteDelta( msg_t *msg, int oldV, int newV, int bits ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, newV, bits );
}

int	MSG_ReadDelta( msg_t *msg, int oldV, int bits ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		return MSG_ReadBits( msg, bits );
	}
	return oldV;
}

void MSG_WriteDeltaFloat( msg_t *msg, float oldV, float newV ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, *(int *)&newV, 32 );
}

float MSG_ReadDeltaFloat( msg_t *msg, float oldV ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		float	newV;

		*(int *)&newV = MSG_ReadBits( msg, 32 );
		return newV;
	}
	return oldV;
}


unsigned int kbitmask[32] = {
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F,	0x0000003F,	0x0000007F,	0x000000FF,
	0x000001FF,	0x000003FF,	0x000007FF,	0x00000FFF,
	0x00001FFF,	0x00003FFF,	0x00007FFF,	0x0000FFFF,
	0x0001FFFF,	0x0003FFFF,	0x0007FFFF,	0x000FFFFF,
	0x001FFFFf,	0x003FFFFF,	0x007FFFFF,	0x00FFFFFF,
	0x01FFFFFF,	0x03FFFFFF,	0x07FFFFFF,	0x0FFFFFFF,
	0x1FFFFFFF,	0x3FFFFFFF,	0x7FFFFFFF,	0xFFFFFFFF,
};

void MSG_WriteDeltaKey( msg_t *msg, int key, int oldV, int newV, int bits ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, newV ^ key, bits );
}

int	MSG_ReadDeltaKey( msg_t *msg, int key, int oldV, int bits ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		return MSG_ReadBits( msg, bits ) ^ (key & kbitmask[bits - 1]);
	}
	return oldV;
}

void MSG_WriteDeltaKeyFloat( msg_t *msg, int key, float oldV, float newV ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, (*(int *)&newV) ^ key, 32 );
}

float MSG_ReadDeltaKeyFloat( msg_t *msg, int key, float oldV ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		float	newV;

		*(int *)&newV = MSG_ReadBits( msg, 32 ) ^ key;
		return newV;
	}
	return oldV;
}





void MSG_WriteDeltaUsercmdKey( msg_t *msg, int key, usercmd_t *from, usercmd_t *to, qboolean extended ) {
	qboolean	changedA, changedB, changedC;

	MSG_WriteBits( msg, to->serverTime, 32 );
	key ^= to->serverTime;

	changedA = (qboolean)( from->angles[0] != to->angles[0]
		|| from->angles[1] != to->angles[1]
		|| from->angles[2] != to->angles[2]
		|| from->forwardmove != to->forwardmove
		|| from->rightmove != to->rightmove
		|| from->upmove != to->upmove
		|| from->buttons != to->buttons );

	changedB = (qboolean)( extended
		&& ( from->realvieworigin[0] != to->realvieworigin[0]
			|| from->realvieworigin[1] != to->realvieworigin[1]
			|| from->realvieworigin[2] != to->realvieworigin[2]
			|| from->realviewangles[0] != to->realviewangles[0]
			|| from->realviewangles[1] != to->realviewangles[1]
			|| from->realviewangles[2] != to->realviewangles[2]
			|| from->deltaAngles[0] != to->deltaAngles[0]
			|| from->deltaAngles[1] != to->deltaAngles[1]
			|| from->deltaAngles[2] != to->deltaAngles[2] ) );

	changedC = (qboolean)( from->choffset[0] != to->choffset[0]
		|| from->choffset[1] != to->choffset[1]
		|| from->lean != to->lean
		|| from->weapon != to->weapon
		|| from->thirdperson != to->thirdperson );

	if ( !changedA && !changedB && !changedC ) {
		// does `*off_228B04 += a3`), but retail never adds to it here.
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );

	MSG_WriteDeltaKey( msg, key, from->angles[0], to->angles[0], 16 );
	MSG_WriteDeltaKey( msg, key, from->angles[1], to->angles[1], 16 );
	MSG_WriteDeltaKey( msg, key, from->angles[2], to->angles[2], 16 );
	MSG_WriteDeltaKey( msg, key, from->forwardmove, to->forwardmove, 8 );
	MSG_WriteDeltaKey( msg, key, from->rightmove, to->rightmove, 8 );
	MSG_WriteDeltaKey( msg, key, from->upmove, to->upmove, 8 );
	MSG_WriteDeltaKey( msg, key, from->buttons, to->buttons, 16 );

	if ( extended ) {
		MSG_WriteDeltaKey( msg, key, (int)from->realvieworigin[0], (int)to->realvieworigin[0], 32 );
		MSG_WriteDeltaKey( msg, key, (int)from->realvieworigin[1], (int)to->realvieworigin[1], 32 );
		MSG_WriteDeltaKey( msg, key, (int)from->realvieworigin[2], (int)to->realvieworigin[2], 32 );
		MSG_WriteDeltaKey( msg, key, (int)from->realviewangles[0], (int)to->realviewangles[0], 32 );
		MSG_WriteDeltaKey( msg, key, (int)from->realviewangles[1], (int)to->realviewangles[1], 32 );
		MSG_WriteDeltaKey( msg, key, (int)from->realviewangles[2], (int)to->realviewangles[2], 32 );
		MSG_WriteDeltaKey( msg, key, from->deltaAngles[0], to->deltaAngles[0], 16 );
		MSG_WriteDeltaKey( msg, key, from->deltaAngles[1], to->deltaAngles[1], 16 );
		MSG_WriteDeltaKey( msg, key, from->deltaAngles[2], to->deltaAngles[2], 16 );
	}

	if ( !changedC ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );

	MSG_WriteDeltaKey( msg, key, from->choffset[0], to->choffset[0], 16 );
	MSG_WriteDeltaKey( msg, key, from->choffset[1], to->choffset[1], 16 );
	MSG_WriteDeltaKey( msg, key, from->lean, to->lean, 8 );
	MSG_WriteDeltaKey( msg, key, from->weapon, to->weapon, 8 );
	MSG_WriteDeltaKey( msg, key, from->thirdperson, to->thirdperson, 4 );
}


void MSG_ReadDeltaUsercmdKey( msg_t *msg, int key, usercmd_t *from, usercmd_t *to, qboolean extended ) {
	to->serverTime = MSG_ReadBits( msg, 32 );
	key ^= to->serverTime;

	if ( !MSG_ReadBits( msg, 1 ) ) {
		*to = *from;
		return;
	}

	to->angles[0] = (short)MSG_ReadDeltaKey( msg, key, from->angles[0], 16 );
	to->angles[1] = (short)MSG_ReadDeltaKey( msg, key, from->angles[1], 16 );
	to->angles[2] = (short)MSG_ReadDeltaKey( msg, key, from->angles[2], 16 );
	to->forwardmove = (signed char)MSG_ReadDeltaKey( msg, key, from->forwardmove, 8 );
	to->rightmove = (signed char)MSG_ReadDeltaKey( msg, key, from->rightmove, 8 );
	to->upmove = (signed char)MSG_ReadDeltaKey( msg, key, from->upmove, 8 );
	to->buttons = (short)MSG_ReadDeltaKey( msg, key, from->buttons, 16 );

	if ( extended ) {
		to->realvieworigin[0] = (float)MSG_ReadDeltaKey( msg, key, (int)from->realvieworigin[0], 32 );
		to->realvieworigin[1] = (float)MSG_ReadDeltaKey( msg, key, (int)from->realvieworigin[1], 32 );
		to->realvieworigin[2] = (float)MSG_ReadDeltaKey( msg, key, (int)from->realvieworigin[2], 32 );
		to->realviewangles[0] = (float)MSG_ReadDeltaKey( msg, key, (int)from->realviewangles[0], 32 );
		to->realviewangles[1] = (float)MSG_ReadDeltaKey( msg, key, (int)from->realviewangles[1], 32 );
		to->realviewangles[2] = (float)MSG_ReadDeltaKey( msg, key, (int)from->realviewangles[2], 32 );
		to->deltaAngles[0] = (short)MSG_ReadDeltaKey( msg, key, from->deltaAngles[0], 16 );
		to->deltaAngles[1] = (short)MSG_ReadDeltaKey( msg, key, from->deltaAngles[1], 16 );
		to->deltaAngles[2] = (short)MSG_ReadDeltaKey( msg, key, from->deltaAngles[2], 16 );
	} else {
		VectorCopy( from->realvieworigin, to->realvieworigin );
		VectorCopy( from->realviewangles, to->realviewangles );
		to->deltaAngles[0] = from->deltaAngles[0];
		to->deltaAngles[1] = from->deltaAngles[1];
		to->deltaAngles[2] = from->deltaAngles[2];
	}

	if ( MSG_ReadBits( msg, 1 ) ) {
		to->choffset[0] = (short)MSG_ReadDeltaKey( msg, key, from->choffset[0], 16 );
		to->choffset[1] = (short)MSG_ReadDeltaKey( msg, key, from->choffset[1], 16 );
		to->lean = (signed char)MSG_ReadDeltaKey( msg, key, from->lean, 8 );
		to->weapon = (byte)MSG_ReadDeltaKey( msg, key, from->weapon, 8 );
		to->thirdperson = MSG_ReadDeltaKey( msg, key, from->thirdperson, 4 );
	} else {
		to->choffset[0] = from->choffset[0];
		to->choffset[1] = from->choffset[1];
		to->lean = from->lean;
		to->weapon = from->weapon;
		to->thirdperson = from->thirdperson;
	}
}



void MSG_WriteDeltaCoord(msg_t* msg, int from, int to)
{
	int delta = to - from;
	int deltaAbs = abs(delta);

	if (deltaAbs <= 0 || deltaAbs > 128)
	{
		MSG_WriteBits(msg, 0, 1);
		MSG_WriteBits(msg, to, 16);
	}
	else
	{
		MSG_WriteBits(msg, 1, 1);

		if (delta < 0) {
			MSG_WriteBits(msg, 1 + ((deltaAbs - 1) << 1), 8);
		}
		else {
			MSG_WriteBits(msg, (deltaAbs - 1) << 1, 8);
		}
	}
}

int MSG_ReadDeltaCoord(msg_t* msg, int from)
{
	int value;
	int delta;

	if (!MSG_ReadBits(msg, 1))
	{
		return MSG_ReadBits(msg, 16);
	}

	value = MSG_ReadBits(msg, 8);
	delta = (value >> 1) + 1;

	if ((value & 1) != 0) {
		delta = -delta;
	}

	return delta + from;
}


static int c_msgChangeVectorHits;
static int c_msgChangeVectorFullReads;
static int c_msgPSChangeVectorHits;
static int c_msgPSChangeVectorFullReads;

void MSG_ReportChangeVectors_f( void ) {
	Com_Printf("FIND_NEW_CHANGE_VECTORS not defined.\n");
	Com_Printf("%i%% of vectors compressed\n",
	           (c_msgChangeVectorHits + c_msgChangeVectorFullReads)
	           ? 100 * c_msgChangeVectorHits / (c_msgChangeVectorHits + c_msgChangeVectorFullReads)
	           : 0);
}

void MSG_PlayerReportChangeVectors_f( void ) {
	Com_Printf("PLAYER_FIND_NEW_CHANGE_VECTORS not defined.\n");
	Com_Printf("%i%% of vectors compressed\n",
	           (c_msgPSChangeVectorHits + c_msgPSChangeVectorFullReads)
	           ? 100 * c_msgPSChangeVectorHits / (c_msgPSChangeVectorHits + c_msgPSChangeVectorFullReads)
	           : 0);
}

typedef struct {
	const char *name;
	int offset;
	int bits;
} netField_t;

#define	NETF(x) #x,(int)(size_t)&((entityState_t*)0)->x

//   * bone_quat[5]      @0x154 -- "not sent over" (SDK q_shared.h:1746)
//   * surfaces[32]      @0x1F4 -- NOT netfield rows in retail: it is
//   * bindparent/quat/mat @0x24C+ -- "not sent over"
netField_t	entityStateFields[] =
{
	{ NETF(instanceNumber), 16 },
	{ NETF(eType), 8 },
	{ NETF(eFlags), 16 },
	{ NETF(pos.trType), 8 },
	{ NETF(pos.trTime), 32 },
	{ NETF(pos.trDuration), 32 },
	{ NETF(pos.trBase[0]), 0 },
	{ NETF(pos.trBase[1]), 0 },
	{ NETF(pos.trBase[2]), 0 },
	{ NETF(pos.trDelta[0]), 0 },
	{ NETF(pos.trDelta[1]), 0 },
	{ NETF(pos.trDelta[2]), 0 },
	{ NETF(apos.trType), 8 },
	{ NETF(apos.trTime), 32 },
	{ NETF(apos.trDuration), 32 },
	{ NETF(apos.trBase[0]), 0 },
	{ NETF(apos.trBase[1]), 0 },
	{ NETF(apos.trBase[2]), 0 },
	{ NETF(apos.trDelta[0]), 0 },
	{ NETF(apos.trDelta[1]), 0 },
	{ NETF(apos.trDelta[2]), 0 },
	{ NETF(netorigin[0]), 0 },
	{ NETF(netorigin[1]), 0 },
	{ NETF(netorigin[2]), 0 },
	{ NETF(origin2[0]), 0 },
	{ NETF(origin2[1]), 0 },
	{ NETF(origin2[2]), 0 },
	{ NETF(netangles[0]), 0 },
	{ NETF(netangles[1]), 0 },
	{ NETF(netangles[2]), 0 },
	{ NETF(viewangles[0]), 0 },
	{ NETF(viewangles[1]), 0 },
	{ NETF(viewangles[2]), 0 },
	{ NETF(constantLight), 32 },
	{ NETF(loopSound), 16 },
	{ NETF(loopSoundVolume), 0 },
	{ NETF(loopSoundMinDist), 0 },
	{ NETF(parent), 16 },
	{ NETF(tag_num), 10 },
	{ NETF(attach_use_angles), 1 },
	{ NETF(attach_offset[0]), 0 },
	{ NETF(attach_offset[1]), 0 },
	{ NETF(attach_offset[2]), 0 },
	{ NETF(attach_angles_offset[0]), 0 },
	{ NETF(attach_angles_offset[1]), 0 },
	{ NETF(attach_angles_offset[2]), 0 },
	{ NETF(modelindex), 9 },
	{ NETF(viewmodelindex), 9 },
	{ NETF(skinNum), 16 },
	{ NETF(customShader), 7 },
	{ NETF(customEmitter), 8 },
	{ NETF(animationRate), 32 },
	{ NETF(anim), 11 },
	{ NETF(frame), 14 },
	{ NETF(crossblend_time), -16 },
	{ NETF(torso_anim), 11 },
	{ NETF(torso_frame), 14 },
	{ NETF(torso_crossblend_time), -16 },
	{ NETF(bone_tag[0]), -8 },
	{ NETF(bone_tag[1]), -8 },
	{ NETF(bone_tag[2]), -8 },
	{ NETF(bone_tag[3]), -8 },
	{ NETF(bone_tag[4]), -8 },
	{ NETF(bone_angles[0][0]), 0 },
	{ NETF(bone_angles[0][1]), 0 },
	{ NETF(bone_angles[0][2]), 0 },
	{ NETF(bone_angles[1][0]), 0 },
	{ NETF(bone_angles[1][1]), 0 },
	{ NETF(bone_angles[1][2]), 0 },
	{ NETF(bone_angles[2][0]), 0 },
	{ NETF(bone_angles[2][1]), 0 },
	{ NETF(bone_angles[2][2]), 0 },
	{ NETF(bone_angles[3][0]), 0 },
	{ NETF(bone_angles[3][1]), 0 },
	{ NETF(bone_angles[3][2]), 0 },
	{ NETF(bone_angles[4][0]), 0 },
	{ NETF(bone_angles[4][1]), 0 },
	{ NETF(bone_angles[4][2]), 0 },
	{ NETF(morph_controllers[0].index), -8 },
	{ NETF(morph_controllers[0].percent), 0 },
	{ NETF(morph_controllers[1].index), -8 },
	{ NETF(morph_controllers[1].percent), 0 },
	{ NETF(morph_controllers[2].index), -8 },
	{ NETF(morph_controllers[2].percent), 0 },
	{ NETF(morph_controllers[3].index), -8 },
	{ NETF(morph_controllers[3].percent), 0 },
	{ NETF(morph_controllers[4].index), -8 },
	{ NETF(morph_controllers[4].percent), 0 },
	{ NETF(morph_controllers[5].index), -8 },
	{ NETF(morph_controllers[5].percent), 0 },
	{ NETF(morph_controllers[6].index), -8 },
	{ NETF(morph_controllers[6].percent), 0 },
	{ NETF(morph_controllers[7].index), -8 },
	{ NETF(morph_controllers[7].percent), 0 },
	{ NETF(morph_controllers[8].index), -8 },
	{ NETF(morph_controllers[8].percent), 0 },
	{ NETF(morph_controllers[9].index), -8 },
	{ NETF(morph_controllers[9].percent), 0 },
	{ NETF(clientNum), 8 },
	{ NETF(groundEntityNum), 10 },
	{ NETF(solid), 32 },
	{ NETF(scale), 0 },
	{ NETF(alpha), 0 },
	{ NETF(renderfx), 32 },
	{ NETF(affectingViewModes), 32 },
	{ NETF(archeTypeIndex), 32 },
	{ NETF(missionObjective), 32 },
	{ NETF(infoIcon), 16 },
	{ NETF(effectsAnims[0]), 32 },
	{ NETF(effectsAnims[1]), 32 },
	{ NETF(effectsAnims[2]), 32 },
	{ NETF(effectsAnims[3]), 32 },
};
static constexpr unsigned long numEntityStateFields = sizeof(entityStateFields) / sizeof(entityStateFields[0]);

void MSG_WriteEntityNum(msg_t* sb, short number)
{
	MSG_WriteBits(sb, number, GENTITYNUM_BITS);
}

unsigned short MSG_ReadEntityNum(msg_t* sb)
{
	return (unsigned short)MSG_ReadBits(sb, GENTITYNUM_BITS);
}

#define	FLOAT_INT_BITS	13
#define	FLOAT_INT_BIAS	(1<<(FLOAT_INT_BITS-1))

static void MSG_WriteDeltaField( msg_t *msg, const void *toF, int bits ) {
	if ( bits ) {
		MSG_WriteBits( msg, *(const int *)toF, bits );
		return;
	}
	else {
		float fullFloat = *(const float *)toF;
		int trunc = (int)fullFloat;

		if ( trunc == fullFloat && (unsigned)( trunc + FLOAT_INT_BIAS ) < ( 1 << FLOAT_INT_BITS ) ) {
			MSG_WriteBits( msg, 0, 1 );
			MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
		}
		else {
			MSG_WriteBits( msg, 1, 1 );
			MSG_WriteBits( msg, *(const int *)toF, 32 );
		}
	}
}

static void MSG_ReadDeltaField( msg_t *msg, void *toF, int bits ) {
	if ( bits ) {
		*(int *)toF = MSG_ReadBits( msg, bits );
	}
	else if ( MSG_ReadBits( msg, 1 ) ) {
		*(int *)toF = MSG_ReadBits( msg, 32 );
	}
	else {
		*(float *)toF = (float)( MSG_ReadBits( msg, FLOAT_INT_BITS ) - FLOAT_INT_BIAS );
	}
}

static qboolean MSG_IsEqual( int fromValue, int toValue, int bits ) {
	unsigned mask;

	if ( bits >= 0 ) {
		if ( bits == 0 ) {
			bits = 32;
		}
	}
	else {
		bits = -bits;
	}

	if ( bits == 32 ) {
		mask = 0xFFFFFFFFu;
	}
	else {
		mask = ( 1u << bits ) - 1;
	}

	return ( toValue & mask ) == ( fromValue & mask );
}

static const int numMsgChangeVectors = 63;

static const byte msgChangeVectors[63][15] =
{
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x10, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x10, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x90, 0x00, 0x00, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x03, 0x00, 0xE0, 0x00, 0x00, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x10, 0x00, 0x00, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x80, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x03, 0x00, 0xE0, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x10, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x90, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0xE0, 0x18, 0x00, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x04, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x03, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x90, 0x00, 0x00, 0x10, 0x80, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x00, 0x80, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x10, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x07, 0x00, 0x00, 0x00, 0x62, 0xC0, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x02, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x80, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x90, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x50, 0x00, 0x00, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x90, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0x10, 0x80, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x03, 0x00, 0xE0, 0x1F, 0x00, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x10, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x10, 0x00, 0x00, 0x10, 0x80, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x04, 0x00, 0xE0, 0xC0, 0x00, 0x00, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xE0, 0xD0, 0x00, 0x00, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00 },
};

static const int numMsgPSChangeVectors = 31;

static const byte msgPSChangeVectors[31][10] =
{
	{ 0x21, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x76, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x76, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 },
	{ 0x21, 0x00, 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x7E, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x76, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x29, 0x00, 0x00, 0x76, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x09, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x72, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x11, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x01, 0x00, 0x04, 0x7E, 0x00, 0x40, 0x00, 0x00, 0x00, 0x04 },
	{ 0x01, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x29, 0x00, 0x00, 0x76, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00 },
	{ 0x21, 0x00, 0x00, 0x70, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x09, 0x00, 0x00, 0x76, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 },
	{ 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static int ChangeVectorBits( void ) {
	return 112;
}

static int GetBitsForNewFields( const byte *diff ) {
	int bits = 0;
	int byteIndex, bitIndex;

	for ( byteIndex = 0; byteIndex < 15; byteIndex++ ) {
		for ( bitIndex = 0; bitIndex < 8; bitIndex++ ) {
			int field = byteIndex * 8 + bitIndex;

			if ( field >= (int)numEntityStateFields ) {
				continue;	
			}
			if ( diff[byteIndex] & ( 1 << bitIndex ) ) {
				int fieldBits = entityStateFields[field].bits;

				if ( fieldBits < 0 ) {
					fieldBits = -fieldBits;
				} else if ( fieldBits == 0 ) {
					fieldBits = 32;
				}
				bits += fieldBits;
			}
		}
	}
	return bits;
}

static int ForceChangeVector( byte *vector ) {
	byte diff[15];
	int best = -1;
	int bestBits = 0;
	int i, b;

	for ( i = 0; i < numMsgChangeVectors; i++ ) {
		qboolean covers = qtrue;

		for ( b = 0; b < 15; b++ ) {
			diff[b] = vector[b] ^ msgChangeVectors[i][b];
			if ( diff[b] & vector[b] ) {
				covers = qfalse;	
			}
		}
		if ( covers ) {
			int newBits = GetBitsForNewFields( diff );

			if ( newBits < ChangeVectorBits() && ( best == -1 || newBits < bestBits ) ) {
				best = i;
				bestBits = newBits;
			}
		}
	}

	if ( best < 0 ) {
		return -1;
	}
	for ( b = 0; b < 15; b++ ) {
		vector[b] = msgChangeVectors[best][b];
	}
	return best;
}

// MSG_WriteDeltaEntity@0x000E9E60 does NOT write an "lc" field count followed
void MSG_WriteDeltaEntity( msg_t *msg, struct entityState_s *from, struct entityState_s *to,
						   qboolean force ) {
	int i, lc;
	netField_t *field;
	size_t numFields = numEntityStateFields;
	int *fromF, *toF;
	int surfacesMask;
	byte changeVector[15];
	int changeIndex;

	if ( to == NULL ) {
		if ( from == NULL ) {
			return;
		}
		MSG_WriteEntityNum(msg, from->number);
		MSG_WriteBits( msg, 1, 1 );
		return;
	}

	if ( to->number < 0 || to->number >= MAX_GENTITIES ) {
		Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Bad entity number: %i", to->number );
	}

	Com_Memset( changeVector, 0, sizeof( changeVector ) );
	lc = 0;
	for ( i = 0, field = entityStateFields ; (size_t)i < numFields; i++, field++ ) {
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );
		if ( !MSG_IsEqual( *fromF, *toF, field->bits ) ) {
			changeVector[i >> 3] |= 1 << ( i & 7 );
			lc = i + 1;
		}
	}

	surfacesMask = 0;
	for ( i = 0; i < MAX_MODEL_SURFACES; i++ ) {
		if ( to->surfaces[i] != from->surfaces[i] ) {
			surfacesMask |= 1 << i;
		}
	}

	if ( lc == 0 && !surfacesMask ) {
		if ( !force ) {
			return;		
		}
		MSG_WriteEntityNum(msg, to->number);
		MSG_WriteBits( msg, 0, 1 );		
		MSG_WriteBits( msg, 0, 1 );		
		return;
	}

	changeIndex = ForceChangeVector( changeVector );

	MSG_WriteEntityNum(msg, to->number);
	MSG_WriteBits( msg, 0, 1 );			
	MSG_WriteBits( msg, 1, 1 );			
	MSG_WriteBits( msg, changeIndex, 6 );	

	if ( changeIndex == -1 ) {
		for ( i = 0; (size_t)i + 8 <= numFields; i += 8 ) {
			MSG_WriteBits( msg, changeVector[i >> 3], 8 );
		}
	}

	for ( i = 0, field = entityStateFields ; (size_t)i < numFields; i++, field++ ) {
		toF = (int *)( (byte *)to + field->offset );

		if ( !( (unsigned char)changeVector[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}

		MSG_WriteDeltaField( msg, toF, field->bits );
	}

	if ( surfacesMask ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, surfacesMask, 32 );
		for ( i = 0; i < MAX_MODEL_SURFACES; i++ ) {
			if ( surfacesMask & ( 1 << i ) ) {
				MSG_WriteBits( msg, to->surfaces[i], 8 );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}
}

extern	cvar_t	*cl_shownet;

void MSG_ReadDeltaEntity( msg_t *msg, entityState_t *from, entityState_t *to,
						  int number) {
	int			i;
	size_t		numFields = numEntityStateFields;
	netField_t	*field;
	int			*fromF, *toF;
	int			print;
	int			startBit, endBit;
	byte		changeVector[15];
	const byte	*vector;
	int			changeIndex;

	if ( number < 0 || number >= MAX_GENTITIES) {
		Com_Error( ERR_DROP, "Bad delta entity number: %i", number );
	}

	if ( msg->bit == 0 ) {
		startBit = msg->readcount * 8 - GENTITYNUM_BITS;
	} else {
		startBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
	}

	if ( MSG_ReadBits( msg, 1 ) == 1 ) {
		Com_Memset( to, 0, sizeof( *to ) );	
		to->number = MAX_GENTITIES - 1;
		if ( cl_shownet->integer >= 2 || cl_shownet->integer == -1 ) {
			Com_Printf( "%3i: #%-3i remove\n", msg->readcount, number );
		}
		return;
	}

	if ( MSG_ReadBits( msg, 1 ) == 0 ) {
		*to = *from;
		to->number = number;
		return;
	}

	if ( cl_shownet->integer >= 2 || cl_shownet->integer == -1 ) {
		print = 1;
		Com_Printf( "%3i: #%-3i ", msg->readcount, to->number );
	} else {
		print = 0;
	}

	to->number = number;

	changeIndex = MSG_ReadBits( msg, 6 );

	if ( changeIndex == 63 ) {
		int b;

		c_msgChangeVectorFullReads++;
		for ( b = 0; b + 8 <= (int)numFields; b += 8 ) {
			int c = MSG_ReadBits( msg, 8 );

			if ( (size_t)msg->readcount > msg->cursize ) {
				c = -1;
			}
			changeVector[b >> 3] = (byte)c;
		}
		vector = changeVector;
	} else {
		c_msgChangeVectorHits++;
		vector = msgChangeVectors[changeIndex];
	}

	for ( i = 0, field = entityStateFields ; (size_t)i < numFields ; i++, field++ ) {
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );
#if 1
		if((size_t)field->offset > sizeof(entityState_t)) {
			Com_Error( ERR_DROP, "MSG_ReadDeltaEntity: field offset %u is larger than size of entityState (i %i)\n", (unsigned)field->offset, i );
		}
#endif
		if ( ! ( (unsigned char)vector[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			*toF = *fromF;
		} else {
			MSG_ReadDeltaField( msg, toF, field->bits );
		}
	}

	Com_Memcpy( to->surfaces, from->surfaces, sizeof( to->surfaces ) );

	if ( MSG_ReadBits( msg, 1 ) ) {
		int surfacesMask = MSG_ReadBits( msg, 32 );

		for ( i = 0; i < MAX_MODEL_SURFACES; i++ ) {
			if ( surfacesMask & ( 1 << i ) ) {
				to->surfaces[i] = ( byte )MSG_ReadBits( msg, 8 );
			}
		}
	}

	if ( print ) {
		if ( msg->bit == 0 ) {
			endBit = msg->readcount * 8 - GENTITYNUM_BITS;
		} else {
			endBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf( " (%i bits)\n", endBit - startBit  );
	}

	MSG_SetupClientEntity( to );
}

void MSG_SetupClientEntity( entityState_t *to ) {
	int		i;

	VectorCopy( to->netorigin, to->origin );
	VectorCopy( to->netangles, to->angles );

	if ( to->clientNum != 1023 ) {
		if ( to->netangles[0] == 0.0f && to->netangles[1] == 0.0f && to->netangles[2] == 0.0f ) {
			to->angles[1] = to->viewangles[1];
			to->angles[0] = 0.0f;
			to->angles[2] = 0.0f;
		}
	}

	EulerToQuat( to->angles, to->quat );

	for( i = 0; i < NUM_BONE_CONTROLLERS; i++ )
	{
		if( to->bone_tag[ i ] >= 0 ) {
			EulerToQuat( to->bone_angles[ i ], to->bone_quat[ i ] );
		}
	}
}

void MSG_ReadSounds(msg_t* msg, server_sound_t* sounds, int* snapshot_number_of_sounds) {

	int		fubar;
	int		i;

	if (MSG_ReadBits(msg, 1)) {
		fubar = MSG_ReadBits(msg, MAX_SERVER_SOUNDS_BITS);

		if (fubar <= MAX_SERVER_SOUNDS) {
			*snapshot_number_of_sounds = fubar;
			for (i = 0; i < fubar; i++) {
				if (MSG_ReadBits(msg, 1) == 1) {
					sounds[i].entity_number = MSG_ReadBits(msg, SOUND_ENTITYNUM_BITS);
					sounds[i].channel = MSG_ReadBits(msg, SOUND_CHANNEL_BITS);
					sounds[i].stop_flag = qtrue; 
				}
				else {
					sounds[i].stop_flag = qfalse;
					if (MSG_ReadBits(msg, 1) == 1) {
						sounds[i].origin[0] = MSG_ReadFloat(msg);
						sounds[i].origin[1] = MSG_ReadFloat(msg);
						sounds[i].origin[2] = MSG_ReadFloat(msg);
					}
					else {
						sounds[i].origin[0] = 0;
						sounds[i].origin[1] = 0;
						sounds[i].origin[2] = 0;
					}
					sounds[i].entity_number = MSG_ReadBits(msg, SOUND_ENTITYNUM_BITS);
					sounds[i].channel = MSG_ReadBits(msg, SOUND_CHANNEL_BITS);
					sounds[i].sound_index = MSG_ReadBits(msg, SOUND_INDEX_BITS);

					if (MSG_ReadBits(msg, 1) == 1) {
						sounds[i].volume = MSG_ReadFloat(msg);
					}
					else {
						sounds[i].volume = -1.0f;
					}

					if (MSG_ReadBits(msg, 1) == 1) {
						sounds[i].min_dist = MSG_ReadFloat(msg);
					}
					else {
						sounds[i].min_dist = -1.0f;
					}

					if (MSG_ReadBits(msg, 1) == 1) {
						sounds[i].pitch_modifier = MSG_ReadFloat(msg);
					}
					else {
						sounds[i].pitch_modifier = 1.0f; 
					}
				}
			}

		}
	}
}

void MSG_WriteSounds(msg_t* msg, server_sound_t* sounds, int snapshot_number_of_sounds) {

	int		i;

	if (!snapshot_number_of_sounds) {
		MSG_WriteBits(msg, 0, 1);
	}
	else {
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, snapshot_number_of_sounds, MAX_SERVER_SOUNDS_BITS);

		for (i = 0; i < snapshot_number_of_sounds; i++) {
			if (!sounds[i].stop_flag) {
				MSG_WriteBits(msg, 0, 1);

				if (sounds[i].origin[0] == 0.0f && sounds[i].origin[1] == 0.0f && sounds[i].origin[2] == 0.0f)
					MSG_WriteBits(msg, 0, 1);
				else {
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteFloat(msg, sounds[i].origin[0]);
					MSG_WriteFloat(msg, sounds[i].origin[1]);
					MSG_WriteFloat(msg, sounds[i].origin[2]);
				}
				MSG_WriteBits(msg, sounds[i].entity_number, SOUND_ENTITYNUM_BITS);
				MSG_WriteBits(msg, sounds[i].channel, SOUND_CHANNEL_BITS);
				MSG_WriteBits(msg, sounds[i].sound_index, SOUND_INDEX_BITS);

				if (sounds[i].volume != -1.0f) {
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteFloat(msg, sounds[i].volume);
				}
				else {
					MSG_WriteBits(msg, 0, 1);
				}

				if (sounds[i].min_dist != -1.0f) {
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteFloat(msg, sounds[i].min_dist);
				}
				else {
					MSG_WriteBits(msg, 0, 1);
				}

				if (sounds[i].pitch_modifier != 1.0f) {
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteFloat(msg, sounds[i].pitch_modifier);
				}
				else {
					MSG_WriteBits(msg, 0, 1);
				}
			}
			else {
				MSG_WriteBits(msg, 1, 1);
				MSG_WriteBits(msg, sounds[i].entity_number, SOUND_ENTITYNUM_BITS);
				MSG_WriteBits(msg, sounds[i].channel, SOUND_CHANNEL_BITS);
			}
		}
	}
}

void MSG_WriteWorldInfo( msg_t *msg, worldState_t *from, worldState_t *to )
{
	int			i;
	qboolean	intensities_changed;
	qboolean	defaults_changed;
	qboolean	wind_intensity_changed;
	qboolean	wind_direction_changed;
	qboolean	weather_type_changed;
	qboolean	weather_intensity_changed;
	qboolean	time_scale_changed;

	intensities_changed = qfalse;
	defaults_changed = qfalse;
	wind_intensity_changed = qfalse;
	wind_direction_changed = qfalse;
	weather_type_changed = qfalse;
	weather_intensity_changed = qfalse;
	time_scale_changed = qfalse;

	for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
		if ( from->dynamic_light_intensities[ i ] != to->dynamic_light_intensities[ i ] ) {
			intensities_changed = qtrue;
			break;
		}
	}

	for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
		if ( from->dynamic_light_default_intensities[ i ] != to->dynamic_light_default_intensities[ i ] ) {
			defaults_changed = qtrue;
			break;
		}
	}

	if ( from->wind_intensity != to->wind_intensity ) {
		wind_intensity_changed = qtrue;
	}

	if ( from->wind_direction[ 0 ] != to->wind_direction[ 0 ]
		|| from->wind_direction[ 1 ] != to->wind_direction[ 1 ]
		|| from->wind_direction[ 2 ] != to->wind_direction[ 2 ] ) {
		wind_direction_changed = qtrue;
	}

	if ( from->weather_type != to->weather_type ) {
		weather_type_changed = qtrue;
	}

	if ( from->weather_intensity != to->weather_intensity ) {
		weather_intensity_changed = qtrue;
	}

	if ( from->time_scale != to->time_scale ) {
		time_scale_changed = qtrue;
	}

	if ( !intensities_changed && !defaults_changed && !wind_intensity_changed
		&& !wind_direction_changed && !weather_type_changed
		&& !weather_intensity_changed && !time_scale_changed ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}

	MSG_WriteBits( msg, 1, 1 );

	if ( intensities_changed ) {
		MSG_WriteBits( msg, 1, 1 );

		for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
			if ( from->dynamic_light_intensities[ i ] == to->dynamic_light_intensities[ i ] ) {
				MSG_WriteBits( msg, 0, 1 );
			} else {
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, to->dynamic_light_intensities[ i ], 8 );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( defaults_changed ) {
		MSG_WriteBits( msg, 1, 1 );

		for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
			if ( from->dynamic_light_default_intensities[ i ] == to->dynamic_light_default_intensities[ i ] ) {
				MSG_WriteBits( msg, 0, 1 );
			} else {
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, to->dynamic_light_default_intensities[ i ], 8 );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( wind_intensity_changed ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, (int)to->wind_intensity, 32 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( wind_direction_changed ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, (int)to->wind_direction[ 0 ], 32 );
		MSG_WriteBits( msg, (int)to->wind_direction[ 1 ], 32 );
		MSG_WriteBits( msg, (int)to->wind_direction[ 2 ], 32 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( weather_type_changed ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, to->weather_type, 32 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( weather_intensity_changed ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, to->weather_intensity, 32 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}

	if ( time_scale_changed ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, (int)to->time_scale, 32 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}
}

void MSG_ReadWorldInfo( msg_t *msg, worldState_t *from, worldState_t *to )
{
	int			i;
	qboolean	changed;

	changed = MSG_ReadBits( msg, 1 ) ? qtrue : qfalse;

	for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
		to->dynamic_light_intensities[ i ] = from->dynamic_light_intensities[ i ];
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
			if ( MSG_ReadBits( msg, 1 ) == 1 ) {
				to->dynamic_light_intensities[ i ] = MSG_ReadBits( msg, 8 );
			}
		}
	}

	for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
		to->dynamic_light_default_intensities[ i ] = from->dynamic_light_default_intensities[ i ];
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		for ( i = 0 ; i < MAX_LIGHTING_GROUPS ; i++ ) {
			if ( MSG_ReadBits( msg, 1 ) == 1 ) {
				to->dynamic_light_default_intensities[ i ] = MSG_ReadBits( msg, 8 );
			}
		}
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		to->wind_intensity = (float)MSG_ReadBits( msg, 32 );
	} else {
		to->wind_intensity = from->wind_intensity;
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		to->wind_direction[ 0 ] = (float)MSG_ReadBits( msg, 32 );
		to->wind_direction[ 1 ] = (float)MSG_ReadBits( msg, 32 );
		to->wind_direction[ 2 ] = (float)MSG_ReadBits( msg, 32 );
	} else {
		to->wind_direction[ 0 ] = from->wind_direction[ 0 ];
		to->wind_direction[ 1 ] = from->wind_direction[ 1 ];
		to->wind_direction[ 2 ] = from->wind_direction[ 2 ];
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		to->weather_type = (weather_t)MSG_ReadBits( msg, 32 );
	} else {
		to->weather_type = from->weather_type;
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		to->weather_intensity = MSG_ReadBits( msg, 32 );
	} else {
		to->weather_intensity = from->weather_intensity;
	}

	if ( changed && MSG_ReadBits( msg, 1 ) ) {
		to->time_scale = (float)MSG_ReadBits( msg, 32 );
	} else {
		to->time_scale = from->time_scale;
	}
}


#define	PSF(x) #x,(int)(size_t)&((playerState_t*)0)->x

// playerState_t.  Do NOT substitute retail's byte offsets here: `offset` indexes
netField_t	playerStateFields[] =
{
{ PSF(commandTime), 32 },
{ PSF(pm_type), 8 },
{ PSF(bobCycle), 8 },
{ PSF(pm_flags), 32 },
{ PSF(pm_time), -16 },
{ PSF(pm_runtime), -16 },
{ PSF(crouchjumpset), 1 },
{ PSF(gravity), 16 },
{ PSF(pm_airaccelerate), 16 },
{ PSF(pm_wateraccelerate), 16 },
{ PSF(pm_stopspeed), 16 },
{ PSF(pm_friction), 16 },
{ PSF(pm_waterfriction), 16 },
{ PSF(crouchjumpvelocity), 16 },
{ PSF(jumpvelocity), 16 },
{ PSF(pm_defaultviewheight), 16 },
{ PSF(pm_accelerate), 16 },
{ PSF(speed), 16 },
{ PSF(groundEntityNum), GENTITYNUM_BITS },
{ PSF(delta_angles[0]), 16 },
{ PSF(delta_angles[1]), 16 },
{ PSF(delta_angles[2]), 16 },
{ PSF(instantJump), 1 },
{ PSF(strafeJumpingAllowed), 1 },
{ PSF(feetfalling), 8 },
{ PSF(velocity[0]), 0 },
{ PSF(velocity[1]), 0 },
{ PSF(velocity[2]), 0 },
{ PSF(origin[0]), 0 },
{ PSF(origin[1]), 0 },
{ PSF(origin[2]), 0 },
{ PSF(clientNum), 8 },
{ PSF(viewheight), -8 },
{ PSF(current_music_mood), 8 },
{ PSF(fallback_music_mood), 8 },
{ PSF(viewangles[0]), 0 },
{ PSF(viewangles[1]), 0 },
{ PSF(viewangles[2]), 0 },
{ PSF(music_volume), 0 },
{ PSF(music_volume_fade_time), 0 },
{ PSF(allowMusicDucking), 1 },
{ PSF(reverb_type), 6 },
{ PSF(reverb_level), 0 },
{ PSF(blend[0]), 0 },
{ PSF(blend[1]), 0 },
{ PSF(blend[2]), 0 },
{ PSF(blend[3]), 0 },
{ PSF(fov), 0 },
{ PSF(camera_angles[0]), 0 },
{ PSF(camera_angles[1]), 0 },
{ PSF(camera_angles[2]), 0 },
{ PSF(camera_origin[0]), 0 },
{ PSF(camera_origin[1]), 0 },
{ PSF(camera_origin[2]), 0 },
{ PSF(cameraFocalPoint[0]), 0 },
{ PSF(cameraFocalPoint[1]), 0 },
{ PSF(cameraFocalPoint[2]), 0 },
{ PSF(useCameraFocalPoint), 1 },
{ PSF(camera_offset[0]), 0 },
{ PSF(camera_offset[1]), 0 },
{ PSF(camera_offset[2]), 0 },
{ PSF(damage_angles[0]), 0 },
{ PSF(damage_angles[1]), 0 },
{ PSF(damage_angles[2]), 0 },
{ PSF(camera_time), 0 },
{ PSF(camera_flags), 16 },
{ PSF(dialogEntnum), 10 },
{ PSF(dialogSoundIndex), -10 },
{ PSF(dialogTextSoundIndex), -10 },
{ PSF(objectiveNameIndex), 3 },
{ PSF(objectiveStates), 32 },
{ PSF(informationStates), 8 },
{ PSF(missionStatus), 3 },
{ PSF(viewMode), 32 },
{ PSF(groundPlane), 1 },
{ PSF(leanDelta), 32 },
};
static constexpr unsigned long numPlayerStateFields = sizeof(playerStateFields) / sizeof(playerStateFields[0]);

static void MSG_WriteDeltaMaskedArray( msg_t *msg, const int *from, const int *to, int count ) {
	int i, mask = 0;

	for ( i = 0; i < count; i++ ) {
		if ( to[i] != from[i] ) {
			mask |= 1 << i;
		}
	}

	if ( mask ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, mask, count );
		for ( i = 0; i < count; i++ ) {
			if ( mask & ( 1 << i ) ) {
				MSG_WriteShort( msg, to[i] );
			}
		}
	} else {
		MSG_WriteBits( msg, 0, 1 );
	}
}

static void MSG_ReadDeltaMaskedArray( msg_t *msg, int *to, int count ) {
	int i, mask;

	if ( !MSG_ReadBits( msg, 1 ) ) {
		return;
	}
	mask = MSG_ReadBits( msg, count );
	for ( i = 0; i < count; i++ ) {
		if ( mask & ( 1 << i ) ) {
			to[i] = MSG_ReadShort( msg );
		}
	}
}

static void MSG_WriteDeltaStats( msg_t *msg, const int *from, const int *to ) {
	int i, masks[3] = { 0, 0, 0 };
	qboolean any = qfalse;

	for ( i = 0; i < MAX_STATS; i++ ) {
		if ( to[i] != from[i] ) {
			masks[i >> 5] |= 1 << ( i & 31 );
			any = qtrue;
		}
	}

	if ( !any ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}

	MSG_WriteBits( msg, 1, 1 );
	for ( i = 0; i < 3; i++ ) {
		if ( masks[i] ) {
			MSG_WriteBits( msg, 1, 1 );
			MSG_WriteBits( msg, masks[i], 32 );
		} else {
			MSG_WriteBits( msg, 0, 1 );
		}
	}
	for ( i = 0; i < MAX_STATS; i++ ) {
		if ( masks[i >> 5] & ( 1 << ( i & 31 ) ) ) {
			MSG_WriteShort( msg, to[i] );
		}
	}
}

static void MSG_ReadDeltaStats( msg_t *msg, int *to ) {
	int i, masks[3] = { 0, 0, 0 };

	if ( !MSG_ReadBits( msg, 1 ) ) {
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		if ( MSG_ReadBits( msg, 1 ) ) {
			masks[i] = MSG_ReadBits( msg, 32 );
		}
	}
	for ( i = 0; i < MAX_STATS; i++ ) {
		if ( masks[i >> 5] & ( 1 << ( i & 31 ) ) ) {
			to[i] = MSG_ReadShort( msg );
		}
	}
}

void MSG_WriteDeltaPlayerstate(msg_t *msg, struct playerState_s *from, struct playerState_s *to) {
	int				i;
	playerState_t	dummy;
	size_t			numFields = numPlayerStateFields;
	netField_t		*field;
	int				*fromF, *toF;
	byte			changeVector[10];
	int				changeIndex;

	if (!from) {
		from = &dummy;
		Com_Memset (&dummy, 0, sizeof(dummy));
	}

	Com_Memset( changeVector, 0, sizeof( changeVector ) );
	for ( i = 0, field = playerStateFields ; (size_t)i < numFields ; i++, field++ ) {
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );
		if ( *fromF != *toF ) {
			changeVector[i >> 3] |= 1 << ( i & 7 );
		}
	}

	changeIndex = -1;
	for ( i = 0; i < numMsgPSChangeVectors; i++ ) {
		if ( !memcmp( changeVector, msgPSChangeVectors[i], sizeof( changeVector ) ) ) {
			changeIndex = i;
			break;
		}
	}

	MSG_WriteBits( msg, changeIndex, 5 );	

	if ( changeIndex == -1 ) {
		for ( i = 0; i + 8 <= (int)( 8 * sizeof( changeVector ) ); i += 8 ) {
			MSG_WriteBits( msg, changeVector[i >> 3], 8 );
		}
		MSG_WriteBits( msg, changeVector[i >> 3], (int)( 8 * sizeof( changeVector ) ) - i );
	}

	for ( i = 0, field = playerStateFields ; (size_t)i < numFields ; i++, field++ ) {
		toF = (int *)( (byte *)to + field->offset );

		if ( !( (unsigned char)changeVector[i >> 3] & ( 1 << ( i & 7 ) ) ) ) {
			continue;
		}

		MSG_WriteDeltaField( msg, toF, field->bits );
	}

	MSG_WriteDeltaStats( msg, from->stats, to->stats );
	MSG_WriteDeltaMaskedArray( msg, from->activeItems, to->activeItems, MAX_ACTIVE_ITEMS );
	MSG_WriteDeltaMaskedArray( msg, from->inventory_name_index, to->inventory_name_index, MAX_INVENTORY );
	MSG_WriteDeltaMaskedArray( msg, from->inventory_weapon_ammo_index, to->inventory_weapon_ammo_index, MAX_INVENTORY );
	MSG_WriteDeltaMaskedArray( msg, from->inventory_weapon_required_ammo, to->inventory_weapon_required_ammo, MAX_INVENTORY );
	MSG_WriteDeltaMaskedArray( msg, from->ammo_in_clip, to->ammo_in_clip, MAX_INVENTORY );
	MSG_WriteDeltaMaskedArray( msg, from->ammo_name_index, to->ammo_name_index, MAX_AMMO );
	MSG_WriteDeltaMaskedArray( msg, from->ammo_amount, to->ammo_amount, MAX_AMMOCOUNT );
	MSG_WriteDeltaMaskedArray( msg, from->max_ammo_amount, to->max_ammo_amount, MAX_AMMOCOUNT );
}

void MSG_ReadDeltaPlayerstate(msg_t *msg, playerState_t *from, playerState_t *to) {
	int			i;
	size_t		numFields = numPlayerStateFields;
	netField_t	*field;
	int			*fromF, *toF;
	int			startBit, endBit;
	int			print;
	playerState_t	dummy;
	byte		changeVector[10];
	const byte	*vector;
	int			changeIndex;

	if ( !from ) {
		from = &dummy;
		Com_Memset( &dummy, 0, sizeof( dummy ) );
	}
	*to = *from;

	if ( msg->bit == 0 ) {
		startBit = msg->readcount * 8 - GENTITYNUM_BITS;
	} else {
		startBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
	}

	if ( cl_shownet->integer >= 2 || cl_shownet->integer == -2 ) {
		print = 1;
		Com_Printf( "%3i: playerstate ", msg->readcount );
	} else {
		print = 0;
	}

	changeIndex = MSG_ReadBits( msg, 5 );

	if ( changeIndex == numMsgPSChangeVectors ) {
		int b, c;

		c_msgChangeVectorFullReads++;
		for ( b = 0; b + 8 <= (int)( 8 * sizeof( changeVector ) ); b += 8 ) {
			c = MSG_ReadBits( msg, 8 );
			if ( (size_t)msg->readcount > msg->cursize ) {
				c = -1;
			}
			changeVector[b >> 3] = (byte)c;
		}
		c = MSG_ReadBits( msg, (int)( 8 * sizeof( changeVector ) ) - b );
		if ( (size_t)msg->readcount > msg->cursize ) {
			c = -1;
		}
		changeVector[b >> 3] = (byte)c;
		vector = changeVector;
	} else {
		c_msgChangeVectorHits++;
		vector = msgPSChangeVectors[changeIndex];
	}

	for ( i = 0, field = playerStateFields ; (size_t)i < numFields ; i++, field++ ) {
		fromF = (int *)( (byte *)from + field->offset );
		toF = (int *)( (byte *)to + field->offset );

		if ( ( (unsigned char)vector[i >> 3] & ( 1 << ( i & 7 ) ) ) == 0 ) {
			*toF = *fromF;
		} else {
			MSG_ReadDeltaField( msg, toF, field->bits );
		}
	}

	MSG_ReadDeltaStats( msg, to->stats );
	MSG_ReadDeltaMaskedArray( msg, to->activeItems, MAX_ACTIVE_ITEMS );
	MSG_ReadDeltaMaskedArray( msg, to->inventory_name_index, MAX_INVENTORY );
	MSG_ReadDeltaMaskedArray( msg, to->inventory_weapon_ammo_index, MAX_INVENTORY );
	MSG_ReadDeltaMaskedArray( msg, to->inventory_weapon_required_ammo, MAX_INVENTORY );
	MSG_ReadDeltaMaskedArray( msg, to->ammo_in_clip, MAX_INVENTORY );
	MSG_ReadDeltaMaskedArray( msg, to->ammo_name_index, MAX_AMMO );
	MSG_ReadDeltaMaskedArray( msg, to->ammo_amount, MAX_AMMOCOUNT );
	MSG_ReadDeltaMaskedArray( msg, to->max_ammo_amount, MAX_AMMOCOUNT );

	if ( print ) {
		if ( msg->bit == 0 ) {
			endBit = msg->readcount * 8 - GENTITYNUM_BITS;
		} else {
			endBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf( " (%i bits)\n", endBit - startBit );
	}
}

int msg_hData[256] = {
250315,			
41193,			
6292,			
7106,			
3730,			
3750,			
6110,			
23283,			
33317,			
6950,			
7838,			
9714,			
9257,			
17259,			
3949,			
1778,			
8288,			
1604,			
1590,			
1663,			
1100,			
1213,			
1238,			
1134,			
1749,			
1059,			
1246,			
1149,			
1273,			
4486,			
2805,			
3472,			
21819,			
1159,			
1670,			
1066,			
1043,			
1012,			
1053,			
1070,			
1726,			
888,			
1180,			
850,			
960,			
780,			
1752,			
3296,			
10630,			
4514,			
5881,			
2685,			
4650,			
3837,			
2093,			
1867,			
2584,			
1949,			
1972,			
940,			
1134,			
1788,			
1670,			
1206,			
5719,			
6128,			
7222,			
6654,			
3710,			
3795,			
1492,			
1524,			
2215,			
1140,			
1355,			
971,			
2180,			
1248,			
1328,			
1195,			
1770,			
1078,			
1264,			
1266,			
1168,			
965,			
1155,			
1186,			
1347,			
1228,			
1529,			
1600,			
2617,			
2048,			
2546,			
3275,			
2410,			
3585,			
2504,			
2800,			
2675,			
6146,			
3663,			
2840,			
14253,			
3164,			
2221,			
1687,			
3208,			
2739,			
3512,			
4796,			
4091,			
3515,			
5288,			
4016,			
7937,			
6031,			
5360,			
3924,			
4892,			
3743,			
4566,			
4807,			
5852,			
6400,			
6225,			
8291,			
23243,			
7838,			
7073,			
8935,			
5437,			
4483,			
3641,			
5256,			
5312,			
5328,			
5370,			
3492,			
2458,			
1694,			
1821,			
2121,			
1916,			
1149,			
1516,			
1367,			
1236,			
1029,			
1258,			
1104,			
1245,			
1006,			
1149,			
1025,			
1241,			
952,			
1287,			
997,			
1713,			
1009,			
1187,			
879,			
1099,			
929,			
1078,			
951,			
1656,			
930,			
1153,			
1030,			
1262,			
1062,			
1214,			
1060,			
1621,			
930,			
1106,			
912,			
1034,			
892,			
1158,			
990,			
1175,			
850,			
1121,			
903,			
1087,			
920,			
1144,			
1056,			
3462,			
2240,			
4397,			
12136,			
7758,			
1345,			
1307,			
3278,			
1950,			
886,			
1023,			
1112,			
1077,			
1042,			
1061,			
1071,			
1484,			
1001,			
1096,			
915,			
1052,			
995,			
1070,			
876,			
1111,			
851,			
1059,			
805,			
1112,			
923,			
1103,			
817,			
1899,			
1872,			
976,			
841,			
1127,			
956,			
1159,			
950,			
7791,			
954,			
1289,			
933,			
1127,			
3207,			
1020,			
927,			
1355,			
768,			
1040,			
745,			
952,			
805,			
1073,			
740,			
1013,			
805,			
1008,			
796,			
996,			
1057,			
11457,			
13504,			
};

void MSG_initHuffman( void ) {
	int i,j;

	msgInit = qtrue;
	Huff_Init(&msgHuff);
	for(i=0;i<256;i++) {
		for (j=0;j<msg_hData[i];j++) {
			Huff_addRef(&msgHuff.compressor,	(byte)i);			
			Huff_addRef(&msgHuff.decompressor,	(byte)i);			
		}
	}
}
