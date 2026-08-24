#include "../client/client.h"

static cvar_t in_mouse_real;
cvar_t* in_mouse = NULL;

void IN_Init(void *windowsData) {
    memset(&in_mouse_real, 0, sizeof(in_mouse_real));
    in_mouse_real.integer = 0;
    in_mouse = &in_mouse_real;
}

void IN_Restart( void ) {
}

void IN_Shutdown( void ) {
}

void Sys_SendKeyEvents (void) {
}

void Key_KeynameCompletion( void( *callback )( const char *s ) ) {
}

void IN_Activate(qboolean active) {
}

void IN_MouseEvent(int mstate) {

}