#include "keycodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	qboolean	down;
	int			repeats;		
	char		*binding;
} qkey_t;

extern	qboolean	key_overstrikeMode;
extern	qkey_t		keys[MAX_KEYS];

extern	int				anykeydown;
extern	qboolean	chat_team;
extern	int			chat_playerNum;

#if defined(APP_MODULE)

int Key_StringToKeynum( const char *str );
void Key_WriteBindings( fileHandle_t f );
void Key_SetBinding( int keynum, const char *binding );
const char *Key_GetBinding( int keynum );
qboolean Key_IsDown( int keynum );
qboolean Key_GetOverstrikeMode( void );
void Key_SetOverstrikeMode( qboolean state );
void Key_ClearStates( void );
int Key_GetKey(const char *binding);
const char* Key_GetKeynameForCommand(const char* command);

#endif

#ifdef __cplusplus
}
#endif
