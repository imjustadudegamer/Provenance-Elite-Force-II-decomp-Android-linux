

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "botlib.h"
#include "be_interface.h"
#include "be_ea.h"

#define MAX_USERMOVE				400

bot_input_t *botinputs;

void EA_Say(int client, char *str)
{
	botimport.BotClientCommand(client, va("say %s", str) );
} 
void EA_SayTeam(int client, char *str)
{
	botimport.BotClientCommand(client, va("say_team %s", str));
} 
void EA_Gesture(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_GESTURE;
} 
void EA_Command(int client, const char *command)
{
	botimport.BotClientCommand(client, command);
} 
void EA_SelectWeapon(int client, int weapon)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->weapon = weapon;
} 
// Retail's EA_Attack carries two extra arguments the Q3 original did not have
// Retail (EA_Attack @0x0014D8D0) never sets both bits.  It picks exactly one of
void EA_Attack(int client, int primarydangerous, int altdangerous)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	if (!primarydangerous || altdangerous) {
		if (!altdangerous || primarydangerous) {
			if (bi->firestate) {
				bi->actionflags |= ACTION_ATTACK;
			} else {
				bi->actionflags |= ACTION_ATTACKRIGHT;
			}
		} else {
			bi->actionflags |= ACTION_ATTACK;
		}
	} else {
		bi->actionflags |= ACTION_ATTACKRIGHT;
	}
} 

void EA_ToggleFireState(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->firestate = rand() % 2;
} 
void EA_Talk(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_TALK;
} 
void EA_Use(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_USE;
} 
void EA_Respawn(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_RESPAWN;
} 
void EA_Jump(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	if (bi->actionflags & ACTION_CROUCH)
	{
		bi->actionflags &= ~ACTION_JUMP;
	} 
	else
	{
		bi->actionflags |= ACTION_JUMP;
	} 
} 
void EA_DelayedJump(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	if (bi->actionflags & ACTION_CROUCH)
	{
		bi->actionflags &= ~ACTION_DELAYEDJUMP;
	} 
	else
	{
		bi->actionflags |= ACTION_DELAYEDJUMP;
	} 
} 
void EA_Crouch(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_CROUCH;
} 
void EA_Walk(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_WALK;
} 
void EA_Action(int client, int action)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= action;
} 
void EA_MoveUp(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_MOVEUP;
} 
void EA_MoveDown(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_MOVEDOWN;
} 
void EA_MoveForward(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_MOVEFORWARD;
} 
void EA_MoveBack(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_MOVEBACK;
} 
void EA_MoveLeft(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_MOVELEFT;
} 
void EA_MoveRight(int client)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	bi->actionflags |= ACTION_MOVERIGHT;
} 
void EA_Move(int client, vec3_t dir, float speed)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	VectorCopy(dir, bi->dir);
	if (speed > MAX_USERMOVE) speed = MAX_USERMOVE;
	else if (speed < -MAX_USERMOVE) speed = -MAX_USERMOVE;
	bi->speed = speed;
} 
void EA_View(int client, vec3_t viewangles)
{
	bot_input_t *bi;

	bi = &botinputs[client];

	VectorCopy(viewangles, bi->viewangles);
} 
void EA_EndRegular(int client, float thinktime)
{
} 
void EA_GetInput(int client, float thinktime, bot_input_t *input)
{
	bot_input_t *bi;

	bi = &botinputs[client];
	bi->thinktime = thinktime;
	Com_Memcpy(input, bi, sizeof(bot_input_t));
} 
void EA_ResetInput(int client)
{
	bot_input_t *bi;
	int jumped = qfalse;

	bi = &botinputs[client];

	bi->thinktime = 0;
	VectorClear(bi->dir);
	bi->speed = 0;
	jumped = bi->actionflags & ACTION_JUMP;
	bi->actionflags = 0;
	if (jumped) bi->actionflags |= ACTION_CROUCH;
} 
int EA_Setup(void)
{
	botinputs = (bot_input_t *) GetClearedHunkMemory(
									botlibglobals.maxclients * sizeof(bot_input_t));
	return BLERR_NOERROR;
} 
void EA_Shutdown(void)
{
	FreeMemory(botinputs);
	botinputs = NULL;
} 
