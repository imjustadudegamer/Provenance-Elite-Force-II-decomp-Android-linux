
#ifdef USE_INTERNAL_SDL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../client/client.h"
#include "../client/touch_controls.h"
#include "../client/cl_cin.h"
#include "../sys/sys_local.h"

#define R120_PROBE_GET(r120_n, r120_d, r120_f) Cvar_Get(r120_n, r120_d, r120_f)
#define R120_PROBE_PRINT                       Com_Printf
#include "../qcommon/qa_r120.h"

extern qboolean UI_BindActive_C( void );
extern qboolean UI_MenuActive_C( void );
extern qboolean UI_IsHudActive_C( void );

#ifdef __ANDROID__
#include <math.h>
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 17)
#define KMOD_SCROLL KMOD_RESERVED
#endif

static cvar_t *in_keyboardDebug     = NULL;

static SDL_GameController *gamepad = NULL;
static int gamepadSentKey[SDL_CONTROLLER_BUTTON_MAX];   

#ifdef __ANDROID__
static volatile int in_androidBackgrounded;      
static int          in_androidApplied = 0;       
static qboolean     in_androidPauseOwned = qfalse; 

#define R201_LIFECYCLE_PROBE_CAP 24
static int r201_lifecycle_probe = 0;

static int SDLCALL IN_AndroidLifecycleFilter( void *userdata, SDL_Event *e )
{
	switch ( e->type )
	{
		case SDL_APP_WILLENTERBACKGROUND:
		case SDL_APP_DIDENTERBACKGROUND:
			in_androidBackgrounded = 1;
			break;
		case SDL_APP_WILLENTERFOREGROUND:
		case SDL_APP_DIDENTERFOREGROUND:
			in_androidBackgrounded = 0;
			break;

		case SDL_WINDOWEVENT:
			if ( e->window.event == SDL_WINDOWEVENT_FOCUS_LOST )
			{
				in_androidBackgrounded = 1;
			}
			else if ( e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED )
			{
				in_androidBackgrounded = 0;
			}
			break;
	}
	return 1;    
}
#endif

static SDL_Joystick *stick = NULL;

static qboolean mouseAvailable = qfalse;
static qboolean mouseActive = qfalse;

static cvar_t *in_mouse             = NULL;
static cvar_t *in_nograb;

static cvar_t *in_joystick          = NULL;
static cvar_t *in_joystickThreshold = NULL;
static cvar_t *in_joystickNo        = NULL;
static cvar_t *in_joystickUseAnalog = NULL;

static cvar_t *cl_crouchToggle      = NULL;   
static qboolean tc_crouchToggled    = qfalse; 

static int vidRestartTime = 0;

static int in_eventTime = 0;

static SDL_Window *SDL_window = NULL;

#define CTRL(a) ((a)-'a'+1)

static void IN_PrintKey( const SDL_Keysym *keysym, keyNum_t key, qboolean down )
{
	if( down )
		Com_Printf( "+ " );
	else
		Com_Printf( "  " );

	Com_Printf( "Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
			keysym->scancode, SDL_GetScancodeName( keysym->scancode ),
			keysym->sym, SDL_GetKeyName( keysym->sym ) );

	if( keysym->mod & KMOD_LSHIFT )   Com_Printf( " KMOD_LSHIFT" );
	if( keysym->mod & KMOD_RSHIFT )   Com_Printf( " KMOD_RSHIFT" );
	if( keysym->mod & KMOD_LCTRL )    Com_Printf( " KMOD_LCTRL" );
	if( keysym->mod & KMOD_RCTRL )    Com_Printf( " KMOD_RCTRL" );
	if( keysym->mod & KMOD_LALT )     Com_Printf( " KMOD_LALT" );
	if( keysym->mod & KMOD_RALT )     Com_Printf( " KMOD_RALT" );
	if( keysym->mod & KMOD_LGUI )     Com_Printf( " KMOD_LGUI" );
	if( keysym->mod & KMOD_RGUI )     Com_Printf( " KMOD_RGUI" );
	if( keysym->mod & KMOD_NUM )      Com_Printf( " KMOD_NUM" );
	if( keysym->mod & KMOD_CAPS )     Com_Printf( " KMOD_CAPS" );
	if( keysym->mod & KMOD_MODE )     Com_Printf( " KMOD_MODE" );
	if( keysym->mod & KMOD_SCROLL )   Com_Printf( " KMOD_SCROLL" );

	Com_Printf( " Q:0x%02x(%s)\n", key, Key_KeynumToString( key ) );
}

static keyNum_t IN_TranslateSDLToQ3Key( SDL_Keysym *keysym, qboolean down )
{
	keyNum_t key = 0;

	if( keysym->scancode >= SDL_SCANCODE_1 && keysym->scancode <= SDL_SCANCODE_0 )
	{
		if( keysym->scancode == SDL_SCANCODE_0 )
			key = '0';
		else
			key = '1' + keysym->scancode - SDL_SCANCODE_1;
	}
	else if( keysym->sym >= SDLK_SPACE && keysym->sym < SDLK_DELETE )
	{
		key = (int)keysym->sym;
	}
	else if (keysym->scancode == SDL_SCANCODE_GRAVE)
	{
		key = '`';
	}
	else
	{
		switch( keysym->sym )
		{
			case SDLK_PAGEUP:       key = K_PGUP;          break;
			case SDLK_KP_9:         key = K_KP_PGUP;       break;
			case SDLK_PAGEDOWN:     key = K_PGDN;          break;
			case SDLK_KP_3:         key = K_KP_PGDN;       break;
			case SDLK_KP_7:         key = K_KP_HOME;       break;
			case SDLK_HOME:         key = K_HOME;          break;
			case SDLK_KP_1:         key = K_KP_END;        break;
			case SDLK_END:          key = K_END;           break;
			case SDLK_KP_4:         key = K_KP_LEFTARROW;  break;
			case SDLK_LEFT:         key = K_LEFTARROW;     break;
			case SDLK_KP_6:         key = K_KP_RIGHTARROW; break;
			case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
			case SDLK_KP_2:         key = K_KP_DOWNARROW;  break;
			case SDLK_DOWN:         key = K_DOWNARROW;     break;
			case SDLK_KP_8:         key = K_KP_UPARROW;    break;
			case SDLK_UP:           key = K_UPARROW;       break;
			case SDLK_ESCAPE:       key = K_ESCAPE;        break;
#ifdef __ANDROID__
			case SDLK_AC_BACK:      key = K_ESCAPE;        break;
#endif
			case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
			case SDLK_RETURN:       key = K_ENTER;         break;
			case SDLK_TAB:          key = K_TAB;           break;
			case SDLK_F1:           key = K_F1;            break;
			case SDLK_F2:           key = K_F2;            break;
			case SDLK_F3:           key = K_F3;            break;
			case SDLK_F4:           key = K_F4;            break;
			case SDLK_F5:           key = K_F5;            break;
			case SDLK_F6:           key = K_F6;            break;
			case SDLK_F7:           key = K_F7;            break;
			case SDLK_F8:           key = K_F8;            break;
			case SDLK_F9:           key = K_F9;            break;
			case SDLK_F10:          key = K_F10;           break;
			case SDLK_F11:          key = K_F11;           break;
			case SDLK_F12:          key = K_F12;           break;
			case SDLK_F13:          key = K_F13;           break;
			case SDLK_F14:          key = K_F14;           break;
			case SDLK_F15:          key = K_F15;           break;

			case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
			case SDLK_DELETE:       key = K_DEL;           break;
			case SDLK_PAUSE:        key = K_PAUSE;         break;

			case SDLK_LSHIFT:
			case SDLK_RSHIFT:       key = K_SHIFT;         break;

			case SDLK_LCTRL:
			case SDLK_RCTRL:        key = K_CTRL;          break;

			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_SUPER;         break;

			case SDLK_RALT:
			case SDLK_LALT:         key = K_ALT;           break;

			case SDLK_KP_5:         key = K_KP_5;          break;
			case SDLK_INSERT:       key = K_INS;           break;
			case SDLK_KP_0:         key = K_KP_INS;        break;
			case SDLK_KP_MULTIPLY:  key = K_KP_STAR;       break;
			case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
			case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    key = K_KP_SLASH;      break;

			case SDLK_MODE:         key = K_MODE;          break;
			case SDLK_HELP:         key = K_HELP;          break;
			case SDLK_PRINTSCREEN:  key = K_PRINT;         break;
			case SDLK_SYSREQ:       key = K_SYSREQ;        break;
			case SDLK_MENU:         key = K_MENU;          break;
			case SDLK_APPLICATION:	key = K_MENU;          break;
			case SDLK_POWER:        key = K_POWER;         break;
			case SDLK_UNDO:         key = K_UNDO;          break;
			case SDLK_SCROLLLOCK:   key = K_SCROLLOCK;     break;
			case SDLK_NUMLOCKCLEAR: key = K_KP_NUMLOCK;    break;
			case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;

			default:
				if( !( keysym->sym & SDLK_SCANCODE_MASK ) && keysym->scancode <= 95 )
				{
					key = K_WORLD_0 + (int)keysym->scancode;
				}
				break;
		}
	}

	if( in_keyboardDebug->integer )
		IN_PrintKey( keysym, key, down );

	if( key == '`' || key == '~' )
		key = K_CONSOLE;

	return key;
}

static void IN_GobbleMotionEvents( void )
{
	SDL_Event dummy[ 1 ];
	int val = 0;

	SDL_PumpEvents( );
	while( ( val = SDL_PeepEvents( dummy, 1, SDL_GETEVENT,
		SDL_MOUSEMOTION, SDL_MOUSEMOTION ) ) > 0 ) { }

	if ( val < 0 )
		Com_Printf( "IN_GobbleMotionEvents failed: %s\n", SDL_GetError( ) );
}

static void IN_ActivateMouse( qboolean isFullscreen )
{
	if (!mouseAvailable || !SDL_WasInit( SDL_INIT_VIDEO ) )
		return;

	if( !mouseActive )
	{
		SDL_SetRelativeMouseMode( SDL_TRUE );
		SDL_SetWindowGrab( SDL_window, SDL_TRUE );

		IN_GobbleMotionEvents( );
	}

	if( !isFullscreen )
	{
		if( in_nograb->modified || !mouseActive )
		{
			if( in_nograb->integer ) {
				SDL_SetRelativeMouseMode( SDL_FALSE );
				SDL_SetWindowGrab( SDL_window, SDL_FALSE );
			} else {
				SDL_SetRelativeMouseMode( SDL_TRUE );
				SDL_SetWindowGrab( SDL_window, SDL_TRUE );
			}

			in_nograb->modified = qfalse;
		}
	}

	mouseActive = qtrue;
}

static void IN_DeactivateMouse( qboolean isFullscreen )
{
	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
		return;


	if( !mouseAvailable )
		return;

	if( mouseActive )
	{
		IN_GobbleMotionEvents( );

		SDL_SetWindowGrab( SDL_window, SDL_FALSE );
		SDL_SetRelativeMouseMode( SDL_FALSE );

		if( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_MOUSE_FOCUS )
			SDL_WarpMouseInWindow( SDL_window, cls.glconfig.vidWidth / 2, cls.glconfig.vidHeight / 2 );

		mouseActive = qfalse;
	}
}

static int joy_keys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28
};

static int hat_keys[16] = {
	K_JOY29, K_JOY30,
	K_JOY31, K_JOY32,
	K_JOY25, K_JOY26,
	K_JOY27, K_JOY28,
	K_JOY21, K_JOY22,
	K_JOY23, K_JOY24,
	K_JOY17, K_JOY18,
	K_JOY19, K_JOY20
};


struct
{
	qboolean buttons[SDL_CONTROLLER_BUTTON_MAX + 1]; 
	unsigned int oldaxes;
	int oldaaxes[MAX_JOYSTICK_AXIS];
	unsigned int oldhats;
} stick_state;


static void IN_FlushGamepadState( void )
{
	static const int negMap[SDL_CONTROLLER_AXIS_MAX] = { K_PAD0_LEFTSTICK_LEFT,  K_PAD0_LEFTSTICK_UP,   K_PAD0_RIGHTSTICK_LEFT,  K_PAD0_RIGHTSTICK_UP, 0, 0 };
	static const int posMap[SDL_CONTROLLER_AXIS_MAX] = { K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER };
	int i;

	for ( i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ )
	{
		if ( stick_state.buttons[i] )
		{
			int key;
#if SDL_VERSION_ATLEAST( 2, 0, 14 )
			if ( i >= SDL_CONTROLLER_BUTTON_MISC1 )
				key = K_PAD0_MISC1 + i - SDL_CONTROLLER_BUTTON_MISC1;
			else
#endif
				key = K_PAD0_A + i;

			if ( gamepadSentKey[i] )
				key = gamepadSentKey[i];   

			Com_QueueEvent( in_eventTime, SE_KEY, key, qfalse, 0, NULL );
		}
	}

	for ( i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++ )
	{
		if ( stick_state.oldaaxes[i] > 0 && posMap[i] )
			Com_QueueEvent( in_eventTime, SE_KEY, posMap[i], qfalse, 0, NULL );
		else if ( stick_state.oldaaxes[i] < 0 && negMap[i] )
			Com_QueueEvent( in_eventTime, SE_KEY, negMap[i], qfalse, 0, NULL );
	}

	for ( i = 0; i < MAX_JOYSTICK_AXIS; i++ )
		Com_QueueEvent( in_eventTime, SE_JOYSTICK_AXIS, i, 0, 0, NULL );
}

static void IN_InitJoystick( void )
{
	int i = 0;
	int total = 0;
	char buf[16384] = "";

	if (gamepad || stick)
		IN_FlushGamepadState();

	if (gamepad)
		SDL_GameControllerClose(gamepad);

	if (stick != NULL)
		SDL_JoystickClose(stick);

	stick = NULL;
	gamepad = NULL;
	memset(&stick_state, '\0', sizeof (stick_state));
	memset(&gamepadSentKey, '\0', sizeof (gamepadSentKey));

#ifdef __ANDROID__
	SDL_SetHint( SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0" );
	SDL_SetHint( SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1" );
#endif

	if (!SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_JOYSTICK)...\n");
		if (SDL_Init(SDL_INIT_JOYSTICK) != 0)
		{
			Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_JOYSTICK) passed.\n");
	}

	if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
	{
		Com_DPrintf("Calling SDL_Init(SDL_INIT_GAMECONTROLLER)...\n");
		if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0)
		{
			Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
			return;
		}
		Com_DPrintf("SDL_Init(SDL_INIT_GAMECONTROLLER) passed.\n");
	}

	total = SDL_NumJoysticks();
	Com_DPrintf("%d possible joysticks\n", total);

	for (i = 0; i < total; i++)
	{
		Q_strcat(buf, sizeof(buf), SDL_JoystickNameForIndex(i));
		Q_strcat(buf, sizeof(buf), "\n");
	}

	Cvar_Get( "in_availableJoysticks", "", CVAR_ROM );

	Cvar_Set( "in_availableJoysticks", buf );

	if( !in_joystick->integer ) {
		Com_DPrintf( "Joystick is not active.\n" );
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
		return;
	}

	in_joystickNo = Cvar_Get( "in_joystickNo", "0", CVAR_ARCHIVE );
	if( in_joystickNo->integer < 0 || in_joystickNo->integer >= total )
		Cvar_Set( "in_joystickNo", "0" );

#ifdef __ANDROID__
	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "0", CVAR_ARCHIVE );
#else
	in_joystickUseAnalog = Cvar_Get( "in_joystickUseAnalog", "1", CVAR_ARCHIVE ); 
#endif

	stick = SDL_JoystickOpen( in_joystickNo->integer );

	if (stick == NULL) {
		Com_DPrintf( "No joystick opened: %s\n", SDL_GetError() );
#ifdef __ANDROID__
		SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "1" );
#endif
		return;
	}

	if (SDL_IsGameController(in_joystickNo->integer))
		gamepad = SDL_GameControllerOpen(in_joystickNo->integer);

#ifdef __ANDROID__
	SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "1" );
	touchControl.controllerConnected = ( gamepad != NULL );
#endif

	Com_DPrintf( "Joystick %d opened\n", in_joystickNo->integer );
	Com_DPrintf( "Name:       %s\n", SDL_JoystickNameForIndex(in_joystickNo->integer) );
	Com_DPrintf( "Axes:       %d\n", SDL_JoystickNumAxes(stick) );
	Com_DPrintf( "Hats:       %d\n", SDL_JoystickNumHats(stick) );
	Com_DPrintf( "Buttons:    %d\n", SDL_JoystickNumButtons(stick) );
	Com_DPrintf( "Balls:      %d\n", SDL_JoystickNumBalls(stick) );
	Com_DPrintf( "Use Analog: %s\n", in_joystickUseAnalog->integer ? "Yes" : "No" );
	Com_DPrintf( "Is gamepad: %s\n", gamepad ? "Yes" : "No" );

	SDL_JoystickEventState(SDL_QUERY);
	SDL_GameControllerEventState(SDL_QUERY);

	R120_PROBE( "input:pad", "opened joystick %d '%s' gamepad=%d",
		in_joystickNo->integer, SDL_JoystickNameForIndex( in_joystickNo->integer ),
		gamepad ? 1 : 0 );
}

static void IN_ShutdownJoystick( void )
{
	if ( !SDL_WasInit( SDL_INIT_GAMECONTROLLER ) )
		return;

	if ( !SDL_WasInit( SDL_INIT_JOYSTICK ) )
		return;

	if (gamepad || stick)
		IN_FlushGamepadState();

	if (gamepad)
	{
		SDL_GameControllerClose(gamepad);
		gamepad = NULL;
	}

	if (stick)
	{
		SDL_JoystickClose(stick);
		stick = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}


static qboolean KeyToAxisAndSign(int keynum, int *outAxis, int *outSign)
{
	const char *bind;

	if (!keynum)
		return qfalse;

	bind = Key_GetBinding(keynum);

	if (!bind || *bind != '+')
		return qfalse;

	*outSign = 0;

	if (Q_stricmp(bind, "+forward") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+back") == 0)
	{
		*outAxis = j_forward_axis->integer;
		*outSign = j_forward->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveleft") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveright") == 0)
	{
		*outAxis = j_side_axis->integer;
		*outSign = j_side->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+lookup") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+lookdown") == 0)
	{
		*outAxis = j_pitch_axis->integer;
		*outSign = j_pitch->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+left") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+right") == 0)
	{
		*outAxis = j_yaw_axis->integer;
		*outSign = j_yaw->value > 0.0f ? -1 : 1;
	}
	else if (Q_stricmp(bind, "+moveup") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? 1 : -1;
	}
	else if (Q_stricmp(bind, "+movedown") == 0)
	{
		*outAxis = j_up_axis->integer;
		*outSign = j_up->value > 0.0f ? -1 : 1;
	}

	return *outSign != 0;
}

static keyNum_t IN_KeyFor( SDL_GameControllerButton sdlButton )
{
	switch(sdlButton)
	{
		case SDL_CONTROLLER_BUTTON_A:               return K_PAD0_A;
		case SDL_CONTROLLER_BUTTON_B:               return K_PAD0_B;
		case SDL_CONTROLLER_BUTTON_X:               return K_PAD0_X;
		case SDL_CONTROLLER_BUTTON_Y:               return K_PAD0_Y;
		case SDL_CONTROLLER_BUTTON_BACK:            return K_PAD0_BACK;
		case SDL_CONTROLLER_BUTTON_GUIDE:           return K_PAD0_GUIDE;
		case SDL_CONTROLLER_BUTTON_START:           return K_PAD0_START;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK:       return K_PAD0_LEFTSTICK_CLICK;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK:      return K_PAD0_RIGHTSTICK_CLICK;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:    return K_PAD0_LEFTSHOULDER;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:   return K_PAD0_RIGHTSHOULDER;
		case SDL_CONTROLLER_BUTTON_DPAD_UP:         return K_PAD0_DPAD_UP;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:       return K_PAD0_DPAD_DOWN;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:       return K_PAD0_DPAD_LEFT;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:      return K_PAD0_DPAD_RIGHT;
#if SDL_VERSION_ATLEAST( 2, 0, 14 )
		case SDL_CONTROLLER_BUTTON_MISC1:           return K_PAD0_MISC1;
		case SDL_CONTROLLER_BUTTON_PADDLE1:         return K_PAD0_PADDLE1;
		case SDL_CONTROLLER_BUTTON_PADDLE2:         return K_PAD0_PADDLE2;
		case SDL_CONTROLLER_BUTTON_PADDLE3:         return K_PAD0_PADDLE3;
		case SDL_CONTROLLER_BUTTON_PADDLE4:         return K_PAD0_PADDLE4;
		case SDL_CONTROLLER_BUTTON_TOUCHPAD:        return K_PAD0_TOUCHPAD;
#endif

		default:
			Com_Error(ERR_DROP, "IN_KeyFor: unhandled SDL button: %d", sdlButton);
			break;
	}

	return 0;
}

static int IN_GamepadMenuKey( int button, int defaultKey )
{
	if ( !( Key_GetCatcher( ) & KEYCATCH_UI ) )
		return defaultKey;

	if ( UI_BindActive_C() )
		return defaultKey;

	switch ( button )
	{
		case SDL_CONTROLLER_BUTTON_A:          return K_ENTER;
		case SDL_CONTROLLER_BUTTON_B:          return K_ESCAPE;
		case SDL_CONTROLLER_BUTTON_START:      return K_ENTER;
		case SDL_CONTROLLER_BUTTON_BACK:       return K_ESCAPE;
		case SDL_CONTROLLER_BUTTON_DPAD_UP:    return K_UPARROW;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return K_DOWNARROW;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return K_LEFTARROW;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return K_RIGHTARROW;
	}
	return defaultKey;
}

static void IN_GamepadMove( void )
{
	int i;
	int translatedAxes[MAX_JOYSTICK_AXIS];
	qboolean translatedAxesSet[MAX_JOYSTICK_AXIS];

	SDL_GameControllerUpdate();

	for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
	{
		SDL_GameControllerButton sdlButton = SDL_CONTROLLER_BUTTON_A + i;
		qboolean pressed = SDL_GameControllerGetButton(gamepad, sdlButton);
		if (pressed != stick_state.buttons[i])
		{
			int key = IN_KeyFor( sdlButton );

			if ( pressed ) {
				key = IN_GamepadMenuKey( sdlButton, key );
				gamepadSentKey[i] = key;
			} else if ( gamepadSentKey[i] ) {
				key = gamepadSentKey[i];   
				gamepadSentKey[i] = 0;
			}

			if ( key == K_PAD0_B && cl_crouchToggle && cl_crouchToggle->integer ) {
				if ( pressed ) {
					tc_crouchToggled = !tc_crouchToggled;
					Com_QueueEvent( in_eventTime, SE_KEY, K_PAD0_B, tc_crouchToggled ? qtrue : qfalse, 0, NULL );
				}
				stick_state.buttons[i] = pressed;
				continue;
			}

			Com_QueueEvent(in_eventTime, SE_KEY, key, pressed, 0, NULL);
			stick_state.buttons[i] = pressed;
			R120_PROBE("input:pad", "button %d -> key %d (%s) %s",
				(int)sdlButton, key, Key_KeynumToString( key ), pressed ? "down" : "up");
		}
	}

	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			translatedAxes[i] = 0;
			translatedAxesSet[i] = qfalse;
		}
	}

	for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
	{
		int axis = SDL_GameControllerGetAxis(gamepad, SDL_CONTROLLER_AXIS_LEFTX + i);
		int oldAxis = stick_state.oldaaxes[i];

		float f = ((float)abs(axis) / 32767.0f - in_joystickThreshold->value) / (1.0f - in_joystickThreshold->value);

		if (f < 0.0f)
			f = 0.0f;

		axis = (int)(32767 * ((axis < 0) ? -f : f));

		if (axis != oldAxis)
		{
			const int negMap[SDL_CONTROLLER_AXIS_MAX] = { K_PAD0_LEFTSTICK_LEFT,  K_PAD0_LEFTSTICK_UP,   K_PAD0_RIGHTSTICK_LEFT,  K_PAD0_RIGHTSTICK_UP, 0, 0 };
			const int posMap[SDL_CONTROLLER_AXIS_MAX] = { K_PAD0_LEFTSTICK_RIGHT, K_PAD0_LEFTSTICK_DOWN, K_PAD0_RIGHTSTICK_RIGHT, K_PAD0_RIGHTSTICK_DOWN, K_PAD0_LEFTTRIGGER, K_PAD0_RIGHTTRIGGER };

			qboolean posAnalog = qfalse, negAnalog = qfalse;
			int negKey = negMap[i];
			int posKey = posMap[i];

			if (in_joystickUseAnalog->integer)
			{
				int posAxis = 0, posSign = 0, negAxis = 0, negSign = 0;

				posAnalog = KeyToAxisAndSign(posKey, &posAxis, &posSign);
				negAnalog = KeyToAxisAndSign(negKey, &negAxis, &negSign);

				if (posAnalog && !translatedAxesSet[posAxis] && oldAxis > 0 && axis <= 0)
				{
					translatedAxes[posAxis] = 0;
					translatedAxesSet[posAxis] = qtrue;
				}

				if (negAnalog && !translatedAxesSet[negAxis] && oldAxis < 0 && axis >= 0)
				{
					translatedAxes[negAxis] = 0;
					translatedAxesSet[negAxis] = qtrue;
				}

				if (posAnalog && axis > 0)
				{
					translatedAxes[posAxis] = axis * posSign;
					translatedAxesSet[posAxis] = qtrue;
				}

				if (negAnalog && axis < 0)
				{
					translatedAxes[negAxis] = -axis * negSign;
					translatedAxesSet[negAxis] = qtrue;
				}
			}


			if (!posAnalog && posKey && oldAxis > 0 && axis <= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qfalse, 0, NULL);

			if (!negAnalog && negKey && oldAxis < 0 && axis >= 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qfalse, 0, NULL);

			if (!posAnalog && posKey && oldAxis <= 0 && axis > 0)
				Com_QueueEvent(in_eventTime, SE_KEY, posKey, qtrue, 0, NULL);

			if (!negAnalog && negKey && oldAxis >= 0 && axis < 0)
				Com_QueueEvent(in_eventTime, SE_KEY, negKey, qtrue, 0, NULL);

			stick_state.oldaaxes[i] = axis;
		}
	}

	if (in_joystickUseAnalog->integer)
	{
		for (i = 0; i < MAX_JOYSTICK_AXIS; i++)
		{
			if (translatedAxesSet[i])
				Com_QueueEvent(in_eventTime, SE_JOYSTICK_AXIS, i, translatedAxes[i], 0, NULL);
		}
	}
}


static void IN_JoyMove( void )
{
	unsigned int axes = 0;
	unsigned int hats = 0;
	int total = 0;
	int i = 0;

	if (gamepad)
	{
		IN_GamepadMove();
		return;
	}

	if (!stick)
		return;

	SDL_JoystickUpdate();

	total = SDL_JoystickNumBalls(stick);
	if (total > 0)
	{
		int balldx = 0;
		int balldy = 0;
		for (i = 0; i < total; i++)
		{
			int dx = 0;
			int dy = 0;
			SDL_JoystickGetBall(stick, i, &dx, &dy);
			balldx += dx;
			balldy += dy;
		}
		if (balldx || balldy)
		{
			if (abs(balldx) > 1)
				balldx *= 2;
			if (abs(balldy) > 1)
				balldy *= 2;
			Com_QueueEvent( in_eventTime, SE_MOUSE, balldx, balldy, 0, NULL );
		}
	}

	total = SDL_JoystickNumButtons(stick);
	if (total > 0)
	{
		if (total > ARRAY_LEN(stick_state.buttons))
			total = ARRAY_LEN(stick_state.buttons);
		for (i = 0; i < total; i++)
		{
			qboolean pressed = (SDL_JoystickGetButton(stick, i) != 0);
			if (pressed != stick_state.buttons[i])
			{
				Com_QueueEvent( in_eventTime, SE_KEY, K_JOY1 + i, pressed, 0, NULL );
				stick_state.buttons[i] = pressed;
			}
		}
	}

	total = SDL_JoystickNumHats(stick);
	if (total > 0)
	{
		if (total > 4) total = 4;
		for (i = 0; i < total; i++)
		{
			((Uint8 *)&hats)[i] = SDL_JoystickGetHat(stick, i);
		}
	}

	if (hats != stick_state.oldhats)
	{
		for( i = 0; i < 4; i++ ) {
			if( ((Uint8 *)&hats)[i] != ((Uint8 *)&stick_state.oldhats)[i] ) {
				switch( ((Uint8 *)&stick_state.oldhats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qfalse, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qfalse, 0, NULL );
						break;
					default:
						break;
				}
				switch( ((Uint8 *)&hats)[i] ) {
					case SDL_HAT_UP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_DOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFT:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_RIGHTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 1], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTUP:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 0], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					case SDL_HAT_LEFTDOWN:
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 2], qtrue, 0, NULL );
						Com_QueueEvent( in_eventTime, SE_KEY, hat_keys[4*i + 3], qtrue, 0, NULL );
						break;
					default:
						break;
				}
			}
		}
	}

	stick_state.oldhats = hats;

	total = SDL_JoystickNumAxes(stick);
	if (total > 0)
	{
		if (in_joystickUseAnalog->integer)
		{
			if (total > MAX_JOYSTICK_AXIS) total = MAX_JOYSTICK_AXIS;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_JoystickGetAxis(stick, i);
				float f = ( (float) abs(axis) ) / 32767.0f;
				
				if( f < in_joystickThreshold->value ) axis = 0;

				if ( axis != stick_state.oldaaxes[i] )
				{
					Com_QueueEvent( in_eventTime, SE_JOYSTICK_AXIS, i, axis, 0, NULL );
					stick_state.oldaaxes[i] = axis;
				}
			}
		}
		else
		{
			if (total > 16) total = 16;
			for (i = 0; i < total; i++)
			{
				Sint16 axis = SDL_JoystickGetAxis(stick, i);
				float f = ( (float) axis ) / 32767.0f;
				if( f < -in_joystickThreshold->value ) {
					axes |= ( 1 << ( i * 2 ) );
				} else if( f > in_joystickThreshold->value ) {
					axes |= ( 1 << ( ( i * 2 ) + 1 ) );
				}
			}
		}
	}

	if (axes != stick_state.oldaxes)
	{
		for( i = 0; i < 16; i++ ) {
			if( ( axes & ( 1 << i ) ) && !( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qtrue, 0, NULL );
			}

			if( !( axes & ( 1 << i ) ) && ( stick_state.oldaxes & ( 1 << i ) ) ) {
				Com_QueueEvent( in_eventTime, SE_KEY, joy_keys[i], qfalse, 0, NULL );
			}
		}
	}

	stick_state.oldaxes = axes;
}

#ifdef __ANDROID__
touchControlState_t touchControl;

static SDL_FingerID tc_moveId = (SDL_FingerID)-1;
static SDL_FingerID tc_lookId = (SDL_FingerID)-1;
static SDL_FingerID tc_actionFinger[TC_NUM_ACTION];   
static float        tc_actionLastX[TC_NUM_ACTION], tc_actionLastY[TC_NUM_ACTION]; 
static float        tc_moveOx, tc_moveOy;
static int          tc_moveState;
static float        tc_lookLx, tc_lookLy;
static SDL_TouchID  tc_touchId = (SDL_TouchID)-1;
static cvar_t       *cl_touchLook;       
static cvar_t       *cl_touchScale;      
static cvar_t       *cl_touchOpacity;    
#ifdef __ANDROID__
static int          tc_skipClickSuppress;
#endif

static float IN_TouchAspect( void )
{
	if ( touchControl.deviceW > 0 && touchControl.deviceH > 0 )
		return touchControl.deviceW / touchControl.deviceH;
	return ( cls.glconfig.vidHeight > 0 )
		? (float)cls.glconfig.vidWidth / (float)cls.glconfig.vidHeight : 2.0f;
}

static float IN_TouchScale( void )
{
	float s = cl_touchScale ? cl_touchScale->value : 1.0f;
	if ( s < 0.6f ) s = 0.6f;
	if ( s > 2.0f ) s = 2.0f;
	return s;
}

static qboolean IN_TouchInRect( float x, float y, float cx, float cy, float hw, float hh )
{
	return ( x > cx - hw && x < cx + hw && y > cy - hh && y < cy + hh );
}

static qboolean IN_TouchControlsActive( void )
{
	if ( Key_GetCatcher( ) & ( KEYCATCH_CONSOLE | KEYCATCH_UI ) ) return qfalse;
	if ( UI_MenuActive_C( ) ) return qfalse;              
	if ( UI_IsHudActive_C( ) ) return qfalse;              
	if ( cls.no_menus ) return qfalse;                    
	if ( cls.state != CA_ACTIVE ) return qfalse;          
	if ( cinematicRunning( ) ) return qfalse;             
	if ( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) return qfalse;
	return qtrue;
}

static void IN_TouchSetMove( int newState )
{
	static const int stickKeys[4] = { K_PAD0_LEFTSTICK_UP, K_PAD0_LEFTSTICK_DOWN,
	                                  K_PAD0_LEFTSTICK_LEFT, K_PAD0_LEFTSTICK_RIGHT };
	static const char *dirs[4] = { "UP(+forward)", "DOWN(+back)",
	                               "LEFT(+moveleft)", "RIGHT(+moveright)" };
	int i;
	for ( i = 0; i < 4; i++ )
	{
		int bit = 1 << i;
		if ( ( newState & bit ) && !( tc_moveState & bit ) )
		{
			Com_QueueEvent( in_eventTime, SE_KEY, stickKeys[i], qtrue, 0, NULL );
			R120_PROBE( "input:touch", "stick %s down (dir=%d) -> key %d (%s)",
				dirs[i], newState, stickKeys[i], Key_KeynumToString( stickKeys[i] ) );
		}
		else if ( !( newState & bit ) && ( tc_moveState & bit ) )
		{
			Com_QueueEvent( in_eventTime, SE_KEY, stickKeys[i], qfalse, 0, NULL );
			R120_PROBE( "input:touch", "stick %s up (dir=%d)", dirs[i], newState );
		}
	}
	tc_moveState = newState;
}

static void IN_TouchReleaseStale( SDL_TouchID touchId )
{
	int     have = 0, i, n;
	int     live = 0;
	SDL_Finger *f;

	if ( tc_moveId == (SDL_FingerID)-1 && tc_lookId == (SDL_FingerID)-1 )
	{
		for ( i = 0; i < TC_NUM_ACTION; i++ )
			if ( tc_actionFinger[i] != (SDL_FingerID)-1 ) { have = 1; break; }
		if ( !have && !tc_moveState )
			return;
	}

	n = SDL_GetNumTouchFingers( touchId );
	if ( n < 0 )
		return;                       

	{
		int nlive = 0;
		SDL_FingerID ids[32];
		if ( n > (int)( sizeof( ids ) / sizeof( ids[0] ) ) )
			n = (int)( sizeof( ids ) / sizeof( ids[0] ) );
		for ( i = 0; i < n; i++ )
		{
			f = SDL_GetTouchFinger( touchId, i );
			if ( f ) ids[nlive++] = f->id;
		}
		for ( i = 0; i < nlive; i++ )
			if ( ids[i] == tc_moveId || ids[i] == tc_lookId )
				live = 1;

		if ( tc_moveId != (SDL_FingerID)-1 && !live )
		{
			R120_PROBE( "input:touch", "stale stick finger %lld released (no FINGERUP)",
				(long long)tc_moveId );
			IN_TouchSetMove( 0 );
			tc_moveId = (SDL_FingerID)-1;
			touchControl.moveActive = qfalse;
		}

		live = 0;
		for ( i = 0; i < nlive; i++ )
			if ( ids[i] == tc_lookId ) { live = 1; break; }
		if ( tc_lookId != (SDL_FingerID)-1 && !live )
		{
			R120_PROBE( "input:touch", "stale look finger %lld released (no FINGERUP)",
				(long long)tc_lookId );
			tc_lookId = (SDL_FingerID)-1;
		}

		for ( i = 0; i < TC_NUM_ACTION; i++ )
		{
			int k, still;
			if ( tc_actionFinger[i] == (SDL_FingerID)-1 )
				continue;
			still = 0;
			for ( k = 0; k < nlive; k++ )
				if ( ids[k] == tc_actionFinger[i] ) { still = 1; break; }
			if ( still )
				continue;
			R120_PROBE( "input:touch", "stale pill finger %lld released (no FINGERUP)",
				(long long)tc_actionFinger[i] );
			if ( !tcActionButtons[i].tap )
				Com_QueueEvent( in_eventTime, SE_KEY, tcActionButtons[i].key, qfalse, 0, NULL );
			tc_actionFinger[i] = (SDL_FingerID)-1;
			touchControl.actionPressed &= ~( 1 << i );
		}
		touchControl.actionFingerHeld = qfalse;
		for ( i = 0; i < TC_NUM_ACTION; i++ )
			if ( tc_actionFinger[i] != (SDL_FingerID)-1 ) { touchControl.actionFingerHeld = qtrue; break; }
	}
}


static void IN_TouchReset( void )
{
	int i;
	if ( tc_moveState )
		IN_TouchSetMove( 0 );
	for ( i = 0; i < TC_NUM_ACTION; i++ ) {
		if ( tc_actionFinger[i] != (SDL_FingerID)-1 ) {
			if ( !tcActionButtons[i].tap )
				Com_QueueEvent( in_eventTime, SE_KEY, tcActionButtons[i].key, qfalse, 0, NULL );
			tc_actionFinger[i] = (SDL_FingerID)-1;
		}
	}
	touchControl.actionFingerHeld = qfalse;   
	if ( tc_crouchToggled ) {
		Com_QueueEvent( in_eventTime, SE_KEY, K_PAD0_B, qfalse, 0, NULL );
		tc_crouchToggled = qfalse;
	}
	tc_moveId = tc_lookId = (SDL_FingerID)-1;
	touchControl.moveActive = qfalse;
	touchControl.actionPressed = 0;
}

#define R201_HOLDFIX_PROBE_CAP 8
static int r201_holdfix_probe = 0;

#define R201_TAPCLICK_PROBE_CAP 8
static int r201_tapclick_probe = 0;

static void IN_TouchReleaseGone( void )
{
	int i, heldBefore = 0, heldAfter = 0;

	for ( i = 0; i < TC_NUM_ACTION; i++ )
		if ( tc_actionFinger[i] != (SDL_FingerID)-1 ) heldBefore++;

	if ( tc_touchId == (SDL_TouchID)-1 )
	{
		IN_TouchReset();          
	}
	else
	{
		IN_TouchReleaseStale( tc_touchId );
		for ( i = 0; i < TC_NUM_ACTION; i++ )
			if ( tc_actionFinger[i] != (SDL_FingerID)-1 ) heldAfter++;
	}

	if ( ( heldBefore || tc_crouchToggled ) && r201_holdfix_probe < R201_HOLDFIX_PROBE_CAP )
	{
		r201_holdfix_probe++;
		Com_Printf( "[R201:holdfix] %d overlay-off held_before=%d held_after=%d crouch=%d "
					"menus=%d\n",
					r201_holdfix_probe, heldBefore, heldAfter, tc_crouchToggled ? 1 : 0,
					UI_MenuActive_C() ? 1 : 0 );
	}

	if ( tc_crouchToggled )
	{
		Com_QueueEvent( in_eventTime, SE_KEY, K_PAD0_B, qfalse, 0, NULL );
		tc_crouchToggled = qfalse;
		for ( i = 0; i < TC_NUM_ACTION; i++ )
			if ( tcActionButtons[i].key == K_PAD0_B )
				touchControl.actionPressed &= ~( 1 << i );
	}
}


// It is DELETED, not disabled: EF1 shipped it disabled, EF2's uilib menus are driven by
static void IN_TouchFinger( SDL_TouchFingerEvent *f, int phase )
{
	touchControl.lastTouchMs = Sys_Milliseconds();   
	tc_touchId = f->touchId;                         

	if ( in_guimouse )
	{
		int ww = 0, wh = 0;
		int sx, sy;
		SDL_Window *r159win = SDL_GetWindowFromID( f->windowID );

		if ( r159win )
			SDL_GetWindowSize( r159win, &ww, &wh );

		sx = (int)( f->x * ( ( ww > 0 ) ? (float)ww : (float)cls.glconfig.vidWidth ) );
		sy = (int)( f->y * ( ( wh > 0 ) ? (float)wh : (float)cls.glconfig.vidHeight ) );

		if ( ww > 0 && wh > 0
		     && ( ww != cls.glconfig.vidWidth || wh != cls.glconfig.vidHeight ) )
		{
			sx = sx * cls.glconfig.vidWidth / ww;
			sy = sy * cls.glconfig.vidHeight / wh;
		}

		CL_SetMousePos( sx, sy );
	}

	// stays false, so cls.state is NEVER set to CA_CINEMATIC (cl_cin.cpp:1715,
	if ( cinematicRunning( ) || cls.state == CA_CINEMATIC ) {
		if ( phase == 0 ) {
			R133_PROBE( "cine:skip",
				"tap movie: cin=%d state=%d statcin=%d -> inject ESC (StopCinematic)",
				cinematicRunning( ) ? 1 : 0, cls.state,
				( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) ? 1 : 0 );
			Com_QueueEvent( in_eventTime, SE_KEY, K_ESCAPE, qtrue,  0, NULL );
			Com_QueueEvent( in_eventTime, SE_KEY, K_ESCAPE, qfalse, 0, NULL );
		}
		return;
	}
	if ( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) {
		if ( phase == 0 ) {
			R133_PROBE( "cine:skip",
				"tap cutscene: cin=%d state=%d statcin=1 -> skipcinematic",
				cinematicRunning( ) ? 1 : 0, cls.state );
			Cbuf_AddText( "skipcinematic\n" );
		}
		return;
	}

	if ( !IN_TouchControlsActive( ) )
	{
		static int r133_touchgate;
		if ( phase == 0 && R120_Throttle( &r133_touchgate, 4, 16 ) )
			R133_PROBE( "cine:swallow",
				"tap at overlay gate: cin=%d state=%d statcin=%d catcher=0x%x menus=%d",
				cinematicRunning( ) ? 1 : 0, cls.state,
				( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) ? 1 : 0,
				Key_GetCatcher( ), UI_MenuActive_C( ) ? 1 : 0 );

		IN_TouchReleaseGone( );
		return;
	}

	if ( phase == 0 )
	{
		int bi;
		IN_TouchReleaseStale( f->touchId );

		for ( bi = 0; bi < TC_NUM_ACTION; bi++ )
		{
			const tcActionButton_t *ab = &tcActionButtons[bi];
			float hw = ab->hw * IN_TouchScale( );
			float hh = ab->hh * IN_TouchScale( );
			if ( tc_actionFinger[bi] == (SDL_FingerID)-1
			     && IN_TouchInRect( f->x, f->y, ab->cx, ab->cy, hw, hh ) )
			{
				if ( ab->key == K_PAD0_B && cl_crouchToggle && cl_crouchToggle->integer )
				{
					tc_crouchToggled = !tc_crouchToggled;
					Com_QueueEvent( in_eventTime, SE_KEY, ab->key, tc_crouchToggled ? qtrue : qfalse, 0, NULL );
					if ( tc_crouchToggled ) touchControl.actionPressed |=  ( 1 << bi );
					else                    touchControl.actionPressed &= ~( 1 << bi );
					return;
				}
				tc_actionFinger[bi] = f->fingerId;
				tc_actionLastX[bi] = f->x; tc_actionLastY[bi] = f->y;
				touchControl.actionPressed |= ( 1 << bi );
				touchControl.actionFingerHeld = qtrue;   
				Com_QueueEvent( in_eventTime, SE_KEY, ab->key, qtrue, 0, NULL );
				R120_PROBE( "input:touch", "pill '%s' -> key %d (%s)",
					ab->label, ab->key, Key_KeynumToString( ab->key ) );
				if ( ab->tap )   
					Com_QueueEvent( in_eventTime, SE_KEY, ab->key, qfalse, 0, NULL );
				return;
			}
		}

		if ( f->x < 0.5f && tc_moveId == (SDL_FingerID)-1 )
		{
			tc_moveId = f->fingerId;
			tc_moveOx = f->x; tc_moveOy = f->y;
			touchControl.moveActive = qtrue;
			touchControl.padCx = f->x; touchControl.padCy = f->y;
			touchControl.thumbX = f->x; touchControl.thumbY = f->y;
			IN_TouchSetMove( 0 );
			R120_PROBE( "input:touch", "stick acquired at %.3f,%.3f (finger %lld)",
				f->x, f->y, (long long)f->fingerId );
		}
		else if ( tc_lookId == (SDL_FingerID)-1 )
		{
			tc_lookId = f->fingerId;
			tc_lookLx = f->x; tc_lookLy = f->y;
		}
	}
	else if ( phase == 1 )
	{
		int bi;
		for ( bi = 0; bi < TC_NUM_ACTION; bi++ )
		{
			if ( f->fingerId == tc_actionFinger[bi] && tcActionButtons[bi].aim )
			{
				float scale = ( cl_touchLook ? cl_touchLook->value : 1.0f );
				int mx = (int)( ( f->x - tc_actionLastX[bi] ) * cls.glconfig.vidWidth  * scale );
				int my = (int)( ( f->y - tc_actionLastY[bi] ) * cls.glconfig.vidHeight * scale );
				tc_actionLastX[bi] = f->x; tc_actionLastY[bi] = f->y;
				if ( mx || my )
					Com_QueueEvent( in_eventTime, SE_MOUSE, mx, my, 0, NULL );
				return;
			}
		}

		if ( f->fingerId == tc_moveId )
		{
			float a  = IN_TouchAspect( );
			float dx = f->x - tc_moveOx;
			float dy = ( f->y - tc_moveOy ) / a;          
			float dz = 0.035f;
			float len = sqrtf( dx * dx + dy * dy );
			int st = 0;
			if ( dy < -dz ) st |= 1;  
			if ( dy >  dz ) st |= 2;  
			if ( dx < -dz ) st |= 4;  
			if ( dx >  dz ) st |= 8;  
			IN_TouchSetMove( st );
			if ( len > TC_PAD_R && len > 0 )
			{
				dx *= TC_PAD_R / len;
				dy *= TC_PAD_R / len;
			}
			touchControl.thumbX = tc_moveOx + dx;
			touchControl.thumbY = tc_moveOy + dy * a;
		}
		else if ( f->fingerId == tc_lookId )
		{
			float scale = ( cl_touchLook ? cl_touchLook->value : 1.0f );
			int mx = (int)( ( f->x - tc_lookLx ) * cls.glconfig.vidWidth  * scale );
			int my = (int)( ( f->y - tc_lookLy ) * cls.glconfig.vidHeight * scale );
			tc_lookLx = f->x; tc_lookLy = f->y;
			if ( mx || my )
				Com_QueueEvent( in_eventTime, SE_MOUSE, mx, my, 0, NULL );
		}
	}
	else
	{
		int bi;
		for ( bi = 0; bi < TC_NUM_ACTION; bi++ )
		{
			if ( f->fingerId == tc_actionFinger[bi] )
			{
				if ( !tcActionButtons[bi].tap )
					Com_QueueEvent( in_eventTime, SE_KEY, tcActionButtons[bi].key, qfalse, 0, NULL );
				tc_actionFinger[bi] = (SDL_FingerID)-1;
				touchControl.actionPressed &= ~( 1 << bi );
				{ int k; touchControl.actionFingerHeld = qfalse;
				  for ( k = 0; k < TC_NUM_ACTION; k++ )
				    if ( tc_actionFinger[k] != (SDL_FingerID)-1 ) { touchControl.actionFingerHeld = qtrue; break; } }
				return;
			}
		}
		if ( f->fingerId == tc_moveId )
		{
			IN_TouchSetMove( 0 );
			tc_moveId = (SDL_FingerID)-1;
			touchControl.moveActive = qfalse;
		}
		else if ( f->fingerId == tc_lookId )
		{
			tc_lookId = (SDL_FingerID)-1;
		}
	}
}
#endif 

static void IN_ProcessEvents( void )
{
	SDL_Event e;
	keyNum_t key = 0;
	static keyNum_t lastKeyDown = 0;

	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
			return;

	while( SDL_PollEvent( &e ) )
	{
		switch( e.type )
		{
			case SDL_KEYDOWN:
				if ( e.key.repeat && Key_GetCatcher( ) == 0 )
					break;

				if( ( key = IN_TranslateSDLToQ3Key( &e.key.keysym, qtrue ) ) )
					Com_QueueEvent( in_eventTime, SE_KEY, key, qtrue, 0, NULL );

				if( key == K_BACKSPACE )
					Com_QueueEvent( in_eventTime, SE_CHAR, CTRL('h'), 0, 0, NULL );
				else if (key == K_ENTER || key == K_TAB || key == K_ESCAPE) 
					Com_QueueEvent( in_eventTime, SE_CHAR, key, 0, 0, NULL);
				else if( keys[K_CTRL].down && key >= 'a' && key <= 'z' )
					Com_QueueEvent( in_eventTime, SE_CHAR, CTRL(key), 0, 0, NULL );

				lastKeyDown = key;
				break;

			case SDL_KEYUP:
				if( ( key = IN_TranslateSDLToQ3Key( &e.key.keysym, qfalse ) ) )
					Com_QueueEvent( in_eventTime, SE_KEY, key, qfalse, 0, NULL );

				lastKeyDown = 0;
				break;

			case SDL_TEXTINPUT:
				if( lastKeyDown != K_CONSOLE )
				{
					char *c = e.text.text;

					while( *c )
					{
						int utf32 = 0;

						if( ( *c & 0x80 ) == 0 )
							utf32 = *c++;
						else if( ( *c & 0xE0 ) == 0xC0 ) 
						{
							utf32 |= ( *c++ & 0x1F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF0 ) == 0xE0 ) 
						{
							utf32 |= ( *c++ & 0x0F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else if( ( *c & 0xF8 ) == 0xF0 ) 
						{
							utf32 |= ( *c++ & 0x07 ) << 18;
							utf32 |= ( *c++ & 0x3F ) << 12;
							utf32 |= ( *c++ & 0x3F ) << 6;
							utf32 |= ( *c++ & 0x3F );
						}
						else
						{
							Com_DPrintf( "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
							c++;
						}

						if( utf32 != 0 )
						{
							Com_QueueEvent( in_eventTime, SE_CHAR, utf32, 0, 0, NULL );
						}
          }
        }
				break;

			case SDL_MOUSEMOTION:
#ifdef __ANDROID__
				if ( in_guimouse )
				{
					int sx = e.motion.x;
					int sy = e.motion.y;
					int ww = 0, wh = 0;
					SDL_Window *r137win = SDL_GetWindowFromID( e.motion.windowID );

					if ( r137win )
						SDL_GetWindowSize( r137win, &ww, &wh );

					if ( ww > 0 && wh > 0
					     && ( ww != cls.glconfig.vidWidth || wh != cls.glconfig.vidHeight ) )
					{
						sx = sx * cls.glconfig.vidWidth / ww;
						sy = sy * cls.glconfig.vidHeight / wh;
					}

					CL_SetMousePos( sx, sy );
					break;
				}
				if ( e.motion.which == SDL_TOUCH_MOUSEID )
					break;
#endif
				if( mouseActive )
				{
					if( !e.motion.xrel && !e.motion.yrel )
						break;
					Com_QueueEvent( in_eventTime, SE_MOUSE, e.motion.xrel, e.motion.yrel, 0, NULL );
				}
				break;

#ifdef __ANDROID__
			case SDL_FINGERDOWN:   IN_TouchFinger( &e.tfinger, 0 ); break;
			case SDL_FINGERMOTION: IN_TouchFinger( &e.tfinger, 1 ); break;
			case SDL_FINGERUP:     IN_TouchFinger( &e.tfinger, 2 ); break;


#endif

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					int b;
#ifdef __ANDROID__
					if ( e.button.which == SDL_TOUCH_MOUSEID ) {
						if ( e.type == SDL_MOUSEBUTTONUP && tc_skipClickSuppress > 0 ) {
							tc_skipClickSuppress--;
							break;
						}
						if ( in_guimouse && e.type == SDL_MOUSEBUTTONDOWN
						     && ( cinematicRunning( ) || cls.state == CA_CINEMATIC
						          || ( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) )
						     && r201_tapclick_probe < R201_TAPCLICK_PROBE_CAP ) {
							r201_tapclick_probe++;
							Com_Printf( "[R201:tapclick] %d delivered to the UI (cutscene up, "
										"ui owns the pointer) cin=%d state=%d statcin=%d catcher=0x%x\n",
										r201_tapclick_probe, cinematicRunning( ) ? 1 : 0, cls.state,
										( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) ? 1 : 0,
										Key_GetCatcher( ) );
						}
						if ( Key_GetCatcher( ) == 0 && !in_guimouse )
							break;
						if ( e.type == SDL_MOUSEBUTTONDOWN && !in_guimouse
						     && ( cinematicRunning( ) || cls.state == CA_CINEMATIC
						          || ( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) ) ) {
							tc_skipClickSuppress++;
							R120_PROBE( "tap:swallow",
								"tap click consumed with the cinematic skip: cin=%d state=%d statcin=%d catcher=0x%x",
								cinematicRunning( ) ? 1 : 0, cls.state,
								( cl.snap.ps.stats[STAT_CINEMATIC] & 1 ) ? 1 : 0,
								Key_GetCatcher( ) );
							break;
						}
					}
#endif
					switch( e.button.button )
					{
						case SDL_BUTTON_LEFT:   b = K_MOUSE1;     break;
						case SDL_BUTTON_MIDDLE: b = K_MOUSE3;     break;
						case SDL_BUTTON_RIGHT:  b = K_MOUSE2;     break;
						case SDL_BUTTON_X1:     b = K_MOUSE4;     break;
						case SDL_BUTTON_X2:     b = K_MOUSE5;     break;
						default:                b = K_AUX1 + ( e.button.button - SDL_BUTTON_X2 + 1 ) % 16; break;
					}
					Com_QueueEvent( in_eventTime, SE_KEY, b,
						( e.type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse ), 0, NULL );
				}
				break;

			case SDL_MOUSEWHEEL:
				if( e.wheel.y > 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
				}
				else if( e.wheel.y < 0 )
				{
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
					Com_QueueEvent( in_eventTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
				}
				break;

			case SDL_CONTROLLERDEVICEADDED:
			case SDL_CONTROLLERDEVICEREMOVED:
				if (in_joystick->integer)
					IN_InitJoystick();
				break;

#ifdef __ANDROID__
			case SDL_APP_WILLENTERBACKGROUND:
			case SDL_APP_DIDENTERBACKGROUND:
				Cvar_SetValue( "com_minimized", 1 );
				break;
			case SDL_APP_WILLENTERFOREGROUND:
			case SDL_APP_DIDENTERFOREGROUND:
				Cvar_SetValue( "com_minimized", 0 );
				break;
#endif

			case SDL_QUIT:
				Cbuf_ExecuteText(EXEC_NOW, "quit Closed window\n");
				break;

			case SDL_WINDOWEVENT:
				switch( e.window.event )
				{
					case SDL_WINDOWEVENT_RESIZED:
						{
							int width, height;

							width = e.window.data1;
							height = e.window.data2;

							if( cls.glconfig.isFullscreen )
							{
								break;
							}

							if( cls.glconfig.vidWidth == width && cls.glconfig.vidHeight == height )
							{
								break;
							}

							Cvar_SetValue( "r_customwidth", width );
							Cvar_SetValue( "r_customheight", height );
							Cvar_Set( "r_mode", "-1" );

							vidRestartTime = Sys_Milliseconds( ) + 1000;
						}
						break;

					case SDL_WINDOWEVENT_MINIMIZED:    Cvar_SetValue( "com_minimized", 1 ); break;
					case SDL_WINDOWEVENT_RESTORED:
					case SDL_WINDOWEVENT_MAXIMIZED:    Cvar_SetValue( "com_minimized", 0 ); break;
					case SDL_WINDOWEVENT_FOCUS_LOST:   Cvar_SetValue( "com_unfocused", 1 ); break;
					case SDL_WINDOWEVENT_FOCUS_GAINED: Cvar_SetValue( "com_unfocused", 0 ); break;
				}
				break;


			default:
				break;
		}
	}
}

void Sys_SendKeyEvents( void )
{
	qboolean loading;

#ifdef __ANDROID__
	if ( in_androidApplied != in_androidBackgrounded )
	{
		const char *action = "none";
		int ownedAfter;

		in_androidApplied = in_androidBackgrounded;
		Cvar_SetValue( "com_minimized", in_androidApplied );

		if ( in_androidApplied )
		{
			if ( cls.state == CA_ACTIVE && !paused->integer )
			{
				Com_Pause_f();
				in_androidPauseOwned = ( paused->integer != 0 ) ? qtrue : qfalse;
				if ( in_androidPauseOwned )
				{
					action = "pause";
				}
			}
			else
			{
				in_androidPauseOwned = qfalse;
			}

			ownedAfter = in_androidPauseOwned ? 1 : 0;
		}
		else
		{
			if ( in_androidPauseOwned )
			{
				Com_UnPause_f();
				in_androidPauseOwned = qfalse;
				action = "unpause";
			}

			R120_PROBE( "input:touch", "foreground: released held touch state" );
			IN_TouchReset();

			ownedAfter = in_androidPauseOwned ? 1 : 0;
		}

		if ( r201_lifecycle_probe < R201_LIFECYCLE_PROBE_CAP )
		{
			r201_lifecycle_probe++;
			Com_Printf( "[R201:lifecycle] %d edge=%s applied=%d com_minimized=%d state=%d "
						"svrun=%d paused=%d action=%s owned_after=%d\n",
						r201_lifecycle_probe,
						in_androidApplied ? "background" : "foreground",
						in_androidApplied,
						Cvar_VariableIntegerValue( "com_minimized" ), cls.state,
						com_sv_running->integer, paused->integer, action, ownedAfter );
		}
	}

	if ( !IN_TouchControlsActive() )
		IN_TouchReleaseGone();
#endif

	IN_JoyMove( );

	loading = ( cls.state != CA_DISCONNECTED && cls.state != CA_ACTIVE );

	cls.glconfig.isFullscreen = Cvar_VariableIntegerValue( "r_fullscreen" ) != 0;

	if( !cls.glconfig.isFullscreen && ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) )
	{
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}
	else if( !cls.glconfig.isFullscreen && loading )
	{
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}
	else if( !( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_INPUT_FOCUS ) )
	{
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}
	else
		IN_ActivateMouse( cls.glconfig.isFullscreen );

	IN_ProcessEvents( );

	in_eventTime = Sys_Milliseconds( );

	if( ( vidRestartTime != 0 ) && ( vidRestartTime < Sys_Milliseconds( ) ) )
	{
		vidRestartTime = 0;
		Cbuf_AddText( "vid_restart\n" );
	}
}

void IN_Init( void *windowData )
{
	int appState;

	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		Com_Error( ERR_FATAL, "IN_Init called before SDL_Init( SDL_INIT_VIDEO )" );
		return;
	}

	SDL_window = (SDL_Window *)windowData;

	Com_DPrintf( "\n------- Input Initialization -------\n" );

	in_keyboardDebug = Cvar_Get( "in_keyboardDebug", "0", CVAR_ARCHIVE );

	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	in_nograb = Cvar_Get( "in_nograb", "0", CVAR_ARCHIVE );

	// latch defers (retail's Carbon input path had no device to re-open).
#ifdef __ANDROID__
	in_joystick = Cvar_Get( "in_joystick", "1", CVAR_ARCHIVE|CVAR_LATCH );
#else
	in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH );
#endif
	in_joystickThreshold = Cvar_Get( "joy_threshold", "0.15", CVAR_ARCHIVE );

#ifdef __ANDROID__
	cl_touchLook    = Cvar_Get( "cl_touchLook",    "2.2", CVAR_ARCHIVE );
	cl_touchScale   = Cvar_Get( "cl_touchScale",   "1.0", CVAR_ARCHIVE );
	cl_touchOpacity = Cvar_Get( "cl_touchOpacity", "1.0", CVAR_ARCHIVE );
	cl_crouchToggle = Cvar_Get( "cl_crouchToggle", "1",   CVAR_ARCHIVE );  
	tc_crouchToggled = qfalse;
	{
		int i, dw = 0, dh = 0;
		for ( i = 0; i < TC_NUM_ACTION; i++ )
			tc_actionFinger[i] = (SDL_FingerID)-1;
		if ( SDL_window )
			SDL_GetWindowSize( SDL_window, &dw, &dh );
		touchControl.deviceW = ( dw > 0 ) ? (float)dw : 0.0f;
		touchControl.deviceH = ( dh > 0 ) ? (float)dh : 0.0f;
	}
#endif


#ifdef __ANDROID__
	SDL_StopTextInput( );
	SDL_SetEventFilter( IN_AndroidLifecycleFilter, NULL );
#else
	SDL_StartTextInput( );
#endif

	SDL_ShowCursor( SDL_DISABLE );

	mouseAvailable = ( in_mouse->value != 0 );
	IN_DeactivateMouse( Cvar_VariableIntegerValue( "r_fullscreen" ) != 0 );

	appState = SDL_GetWindowFlags( SDL_window );
	Cvar_SetValue( "com_unfocused",	!( appState & SDL_WINDOW_INPUT_FOCUS ) );
	Cvar_SetValue( "com_minimized", appState & SDL_WINDOW_MINIMIZED );

	IN_InitJoystick( );
	Com_DPrintf( "------------------------------------\n" );
}

void IN_Shutdown( void )
{
	SDL_StopTextInput( );

	IN_DeactivateMouse( Cvar_VariableIntegerValue( "r_fullscreen" ) != 0 );
	mouseAvailable = qfalse;

	IN_ShutdownJoystick( );

	SDL_window = NULL;
}

void IN_Restart( void )
{
	IN_ShutdownJoystick( );
	IN_Init( SDL_window );
}
