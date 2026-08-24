
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

void Sys_Init( void ) {
}

void SyScriptError( const char *error, ... ) {
}

void Sys_Quit( void ) {
}

void Sys_Print( const char *msg ) {
	printf( msg );
}

char *Sys_ConsoleInput( void ) {
	return NULL;
}
