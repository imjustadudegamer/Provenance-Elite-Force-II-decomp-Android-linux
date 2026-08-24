
#include "../client/client.h"

cvar_t *cl_shownet;
clientStatic_t cls;
clientActive_t cl;
clientConnection_t clc;
clientGameExport_t *cge;

// The loopback passthrough (SV_ShouldPassthrough @0x000EE9D0) can never fire on a
void CL_AddCommandString(int seq, const char *s) {
	(void)seq;
	(void)s;
}

void CL_CommitSnapshot(clSnapshot_t *newSnap) {
	(void)newSnap;
}

void CL_SnapshotWorldState(worldState_t *ws) {
	(void)ws;
}

void UI_UpdateCenterPrint(const char *s, float alpha) {
	(void)s;
	(void)alpha;
}

void UI_UpdateLocationPrint(int x, int y, const char *s, float alpha) {
	(void)x;
	(void)y;
	(void)s;
	(void)alpha;
}

void CL_Shutdown( void ) {
}

void CL_Init( void ) {
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_TEMP );

	memset( &cls, 0, sizeof( clientStatic_t ) );
	memset( &cl, 0, sizeof( clientActive_t ) );
	cge = NULL;
}

void CL_InitClientSavedData( void ) {
}

void CL_MouseEvent( int dx, int dy, int time ) {
}

void CL_ClearButtons( void ) {
}

void Key_WriteBindings( fileHandle_t f ) {
}

void CL_Frame ( int msec ) {
}

void CL_PacketEvent( netadr_t from, msg_t *msg ) {
}

void CL_CharEvent( int key ) {
}

void CL_Disconnect(qboolean showMenu) {
}

void CL_AbnormalDisconnect( const char *message ) {
	(void)message;
}

void CL_MapLoading( qboolean flush, const char *pszMapName, const char *pszCinematic, int stage ) {
}

void CL_MapLoadingClientOnly( const char *pszMapName ) {
}

qboolean CL_GameCommand( void ) {
  return qfalse;
}

void CL_KeyEvent (int key, qboolean down, unsigned time) {
}

qboolean UI_GameCommand( void ) {
	return qfalse;
}

void CL_ForwardCommandToServer( const char *string ) {
}

void CL_ConsolePrint( char *txt ) {
}

void CL_JoystickEvent( int axis, int value, int time ) {
}

void CL_InitKeyCommands( void ) {
}

void CL_CDDialog( void ) {
}

void CL_FlushMemory( void ) {
}

void CL_StartHunkUsers( qboolean rendererOnly ) {
}

void CL_ShutdownAll(qboolean shutdownRef) {
}

qboolean CL_CDKeyValidate( const char *key, const char *checksum ) { return qtrue; }

void UI_ClearResource( void ) {
}

void UI_LoadResource( const char *name ) {
}

int cl_loadingScreenStage = 0;

void CL_ProcessLoadingScreen( const char *loadingStatus ) {
}

const char *Key_KeynumToString( int keynum ) {
	return NULL;
}

const char *Key_KeynumToBindString( int keynum ) {
	return NULL;
}

void Key_GetKeysForCommand( const char *command, int *key1, int *key2 ) {
}

void SCR_DebugGraph( float value ) {
}

void InitializeCinematicSystem( void ) {
}

int R_CountTextureMemory(void) {
    return 0;
}

qboolean CL_UseLargeLightmap(const char* mapName) {
	return qtrue;
}

qboolean CL_DoneLoading( void ) {
	return qfalse;
}

