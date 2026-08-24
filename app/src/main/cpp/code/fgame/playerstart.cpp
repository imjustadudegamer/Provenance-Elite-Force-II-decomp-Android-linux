

#include "g_local.h"
#include "entity.h"
#include "trigger.h"
#include "playerstart.h"


Event EV_PlayerStart_SetThread (    "thread",
    EV_SCRIPTONLY,
    "s",
    "thread",
    "Set the thread to execute when this player start is used");

CLASS_DECLARATION(Entity, PlayerStart, "info_player_start") {
    {&EV_SetAngle,              &PlayerStart::SetAngle },
    {&EV_PlayerStart_SetThread, &PlayerStart::SetThread},
    {NULL,                      NULL                    }
};


void PlayerStart::SetAngle(Event *ev)
{
    angles = Vector(0.0f, ev->GetFloat(1), 0.0f);
}

void PlayerStart::SetThread(Event *ev)
{
    thread = ev->GetString(1);
}

str PlayerStart::getThread(void)
{
    return thread;
}


CLASS_DECLARATION(PlayerStart, TestPlayerStart, "testplayerstart") {
    {NULL, NULL}
};


Event EV_PlayerDeathmatchStart_SetType (    "spawnpoint_type",
    EV_DEFAULT,
    "s",
    "spawnpointType",
    "Sets the named type of this spawnpoint");

CLASS_DECLARATION(PlayerStart, PlayerDeathmatchStart, "info_player_deathmatch") {
    {&EV_PlayerDeathmatchStart_SetType, &PlayerDeathmatchStart::SetType},
    {NULL,                              NULL                          }
};

void PlayerDeathmatchStart::SetType(Event *ev)
{
    _type = ev->GetString(1);
}



CLASS_DECLARATION(Camera, PlayerIntermission, "info_player_intermission") {
    {NULL, NULL}
};

PlayerIntermission::PlayerIntermission()

{
    currentstate.watch.watchPath = false;
}
