
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <stdio.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

void CON_Shutdown( void )
{
}

void CON_Init( void )
{
}

char *CON_Input( void )
{
	return NULL;
}

void CON_Print( const char *msg )
{
	fputs( msg, stderr );

#ifdef __ANDROID__
	Sys_AndroidLog( ANDROID_LOG_INFO, msg );
#endif
}
