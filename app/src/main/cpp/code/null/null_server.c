
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

cvar_t *sv_mapname;

void SV_Init( void ) {
}

void SV_Shutdown( const char *finalmsg ) {
}

void SV_Frame( int msec ) {
}

void SV_PacketEvent( netadr_t from, msg_t *msg ) {
}

qboolean SV_GameCommand( void ) {
	return qfalse;
}
