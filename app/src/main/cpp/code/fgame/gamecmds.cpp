/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gamecmds.h"
#include "scriptvariable.h"
#include "glb_local.h"
#include "camera.h"
#include "viewthing.h"
#include "soundman.h"
#include "navigate.h"
#include "player.h"
#include "scriptthread.h"
#include "CinematicArmature.h"
#include "../qcommon/gameplaymanager.h"
#include "mp_manager.hpp"
#include "g_spawn.h" 

typedef struct {
    const char *command;
    qboolean (*func)(gentity_t *ent);
    qboolean allclients;
} consolecmd_t;

typedef struct {
    const char       *prefix;
    SafePtr<Listener> master;
} commandmaster_t;

consolecmd_t G_ConsoleCmds[] = {
    {"vtaunt",          G_VTaunt,             qtrue },
    {"vsay",            G_SayCmd,             qtrue },
    {"vosay",           G_SayCmd,             qtrue },
    {"tell",            G_TellCmd,            qtrue },
    {"vtell",           G_TellCmd,            qtrue },
    {"say",             G_SayCmd,             qtrue },
    {"taunt",           G_TauntCmd,           qtrue },
    {"vosay_team",      G_TeamSayCmd,         qtrue },
    {"vsay_team",       G_TeamSayCmd,         qtrue },
    {"tsay",            G_TeamSayCmd,         qtrue },
    {"say_team",        G_TeamSayCmd,         qtrue }, 
    {"eventlist",       G_EventListCmd,       qfalse},
    {"pendingevents",   G_PendingEventsCmd,   qfalse},
    {"eventhelp",       G_EventHelpCmd,       qfalse},
    {"dumpevents",      G_DumpEventsCmd,      qfalse},
    {"classevents",     G_ClassEventsCmd,     qfalse},
    {"dumpclassevents", G_DumpClassEventsCmd, qfalse},
    {"dumpallclasses",  G_DumpAllClassesCmd,  qfalse},
    {"classlist",       G_ClassListCmd,       qfalse},
    {"classtree",       G_ClassTreeCmd,       qfalse},
    {"cam",             G_CameraCmd,          qfalse},
    {"snd",             G_SoundCmd,           qfalse},
    {"cin",             G_CinematicCmd,       qfalse},
    {"script",          G_ScriptCmd,          qfalse},
    {"clientrunthread", G_ClientRunThreadCmd, qfalse},
    {"clientsetvar",    G_ClientSetVarCmd,    qfalse},
    {"levelvars",       G_LevelVarsCmd,       qfalse},
    {"gamevars",        G_GameVarsCmd,        qfalse},
    {"loc",             G_LocCmd,             qfalse},
    {"warp",            G_WarpCmd,            qfalse},
    {"mask",            G_MaskCmd,            qfalse},
    {"setgameplayfloat", G_SetGameplayFloatCmd, qfalse},
    {"setgameplaystring", G_SetGameplayStringCmd, qfalse},
    {"purchaseSkill",   G_PurchaseSkillCmd,   qfalse},
    {"swapItem",        G_SwapItemCmd,        qfalse},
    {"dropItem",        G_DropItemCmd,        qfalse},
    {"dialogrunthread", G_DialogRunThread,    qfalse},
    {NULL,              NULL,                 qfalse}
};

Container<commandmaster_t> commandMasters;

void G_InitConsoleCommands(void)
{
    consolecmd_t *cmds;

    gi.AddCommand("give");
    gi.AddCommand("god");
    gi.AddCommand("notarget");
    gi.AddCommand("noclip");
    gi.AddCommand("kill");
    gi.AddCommand("script");
    // Dropping these gi.AddCommand calls is parity-safe because RETAIL NEVER MADE THEM
    // EITHER -- not because the names became unreachable.  They did not: retail's

    for (cmds = G_ConsoleCmds; cmds->command != NULL; cmds++) {
        gi.AddCommand(cmds->command);
    }
}

qboolean G_ConsoleCommand(void)
{
    gentity_t *ent;
    qboolean   result;

    result = qfalse;
    try {
        if (dedicated->integer) {
            const char *cmd;

            cmd = gi.argv(0);

            if (Q_stricmp(cmd, "say") == 0) {
                G_Say(NULL, false, false);
                result = true;
            }
        }

        if (!result) {
            ent    = &g_entities[0];
            result = G_ProcessClientCommand(ent);
        }
    } catch (const char *error) {
        G_ExitWithError(error);
    }

    return result;
}

void G_ClientCommand(gentity_t *ent)
{
    try {
        if (ent && !G_ProcessClientCommand(ent)) {
            // retail G_ClientCommand: anything that does not match a command is a chat.
            G_Say(ent, false, true);
        }
    }

    catch (const char *error) {
        G_ExitWithError(error);
    }
}

qboolean G_ProcessClientCommand(gentity_t *ent)
{
    const char   *cmd;
    consolecmd_t *cmds;
    int           i;
    int           n;

    if (!ent || !ent->client || !ent->entity || !ent->inuse) {
        return qfalse;
    }

    cmd = gi.argv(0);

    for (cmds = G_ConsoleCmds; cmds->command != NULL; cmds++) {
        if ((game.maxclients > 1) && (!cmds->allclients) && !sv_cheats->integer) {
            continue;
        }

        if (!Q_stricmp(cmd, cmds->command)) {
            return cmds->func(ent);
        }
    }

    if (Event::Exists(cmd)) {
        Event *ev = new Event(cmd);
        ev->SetSource(EV_FROM_CONSOLE);
        ev->SetConsoleEdict(ent);

        n = gi.argc();

        for (i = 1; i < n; i++) {
            ev->AddToken(gi.argv(i));
        }

        if (!Q_stricmpn(cmd, "ai_", 2)) {
            return thePathManager.ProcessEvent(ev);
        } else if (!Q_stricmpn(cmd, "view", 4)) {
            return Viewmodel.ProcessEvent(ev);
        } else {
            if (ent && ent->entity) {
                return ent->entity->ProcessEvent(ev);
            }
        }
    }

    if (g_gametype->integer == GT_SINGLE_PLAYER && level.game_script.length()) {
        CThread *thread = Director.CreateThread(cmd);

        if (thread) {
            thread->Start();
            return qtrue;
        }
    }

    return qfalse;
}




void G_Say(gentity_t *ent, qboolean team, qboolean arg0)
{
    str         text;
    const char *p;

    if (gi.argc() < 2 && !arg0) {
        return;
    }

    if (arg0) {
        text = gi.argv(0);
        text += " ";
        text += gi.args();
    } else {
        p = gi.args();

        if (*p == '"') {
            p++;
            text = p;
            text[text.length() - 1] = 0;
        } else {
            text = p;
        }
    }

    if (ent && ent->entity && ent->entity->isSubclassOf(Player)) {
        multiplayerManager.say((Player *)ent->entity, text, team);
    } else {
        multiplayerManager.say(NULL, text, false);
    }
}

qboolean G_CameraCmd(gentity_t *ent)
{
    Event      *ev;
    const char *cmd;
    int         i;
    int         n;

    n = gi.argc();
    if (!n) {
        gi.WPrintf("Usage: cam [command] [arg 1]...[arg n]\n");
        return qtrue;
    }

    cmd = gi.argv(1);
    if (Event::Exists(cmd)) {
        ev = new Event(cmd);
        ev->SetSource(EV_FROM_CONSOLE);
        ev->SetConsoleEdict(NULL);

        for (i = 2; i < n; i++) {
            ev->AddToken(gi.argv(i));
        }

        CameraMan.ProcessEvent(ev);
    } else {
        gi.WPrintf("Unknown camera command '%s'.\n", cmd);
    }

    return qtrue;
}

qboolean G_SoundCmd(gentity_t *ent)
{
    Event      *ev;
    const char *cmd;
    int         i;
    int         n;

    n = gi.argc();
    if (!n) {
        gi.WPrintf("Usage: snd [command] [arg 1]...[arg n]\n");
        return qtrue;
    }

    cmd = gi.argv(1);
    if (Event::Exists(cmd)) {
        ev = new Event(cmd);
        ev->SetSource(EV_FROM_CONSOLE);
        ev->SetConsoleEdict(NULL);

        for (i = 2; i < n; i++) {
            ev->AddToken(gi.argv(i));
        }

        SoundMan.ProcessEvent(ev);
    } else {
        gi.WPrintf("Unknown sound command '%s'.\n", cmd);
    }

    return qtrue;
}

qboolean G_SayCmd(gentity_t *ent)
{
    G_Say(ent, false, false);

    return qtrue;
}

qboolean G_EventListCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.argc() > 1) {
        mask = gi.argv(1);
    }

    Event::ListCommands(mask);

    return qtrue;
}

qboolean G_PendingEventsCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.argc() > 1) {
        mask = gi.argv(1);
    }

    Event::PendingEvents(mask);

    return qtrue;
}

qboolean G_EventHelpCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.argc() > 1) {
        mask = gi.argv(1);
    }

    Event::ListDocumentation(mask, false);

    return qtrue;
}

qboolean G_DumpEventsCmd(gentity_t *ent)
{
    const char *mask;

    mask = NULL;
    if (gi.argc() > 1) {
        mask = gi.argv(1);
    }

    Event::ListDocumentation(mask, true);

    return qtrue;
}

qboolean G_ClassEventsCmd(gentity_t *ent)
{
    const char *className;

    className = NULL;
    if (gi.argc() < 2) {
        gi.WPrintf("Usage: classevents [className]\n");
        className = gi.argv(1);
    } else {
        className = gi.argv(1);
        ClassEvents(className, qfalse);
    }
    return qtrue;
}

qboolean G_DumpClassEventsCmd(gentity_t *ent)
{
    const char *className;

    className = NULL;
    if (gi.argc() < 2) {
        gi.WPrintf("Usage: dumpclassevents [className]\n");
        className = gi.argv(1);
    } else {
        className = gi.argv(1);
        ClassEvents(className, qtrue);
    }
    return qtrue;
}

qboolean G_DumpAllClassesCmd(gentity_t *ent)
{
    const char *tmpstr     = NULL;
    const char *filename   = NULL;
    int         typeFlag   = EVENT_ALL;
    int         outputFlag = OUTPUT_ALL;

    if (gi.argc() > 1) {
        tmpstr = gi.argv(1);
        if (!strcmp("tiki", tmpstr)) {
            typeFlag = EVENT_TIKI_ONLY;
        }
        if (!strcmp("script", tmpstr)) {
            typeFlag = EVENT_SCRIPT_ONLY;
        }
    }

    if (gi.argc() > 2) {
        tmpstr = gi.argv(2);
        if (!strcmp("html", tmpstr)) {
            outputFlag = OUTPUT_HTML;
        }
        if (!strcmp("cmdmap", tmpstr)) {
            outputFlag = OUTPUT_CMD;
        }
    }

    if (gi.argc() > 3) {
        filename = gi.argv(3);
    }

    DumpAllClasses(typeFlag, outputFlag, filename);
    return qtrue;
}

qboolean G_ClassListCmd(gentity_t *ent)
{
    listAllClasses();

    return qtrue;
}

qboolean G_ClassTreeCmd(gentity_t *ent)
{
    if (gi.argc() > 1) {
        listInheritanceOrder(gi.argv(1));
    } else {
        gi.SendServerCommand(ent - g_entities, "print \"Syntax: classtree [classname].\n\"");
    }

    return qtrue;
}


void PrintVariableList(ScriptVariableList *list)
{
    ScriptVariable *var;
    int             i;

    for (i = 1; i <= list->NumVariables(); i++) {
        var = list->GetVariable(i);
        gi.Printf("%s = %s\n", var->getName(), var->stringValue());
    }
    gi.Printf("%d variables\n", list->NumVariables());
}

qboolean G_LevelVarsCmd(gentity_t *ent)
{
    gi.Printf("Level Variables\n");
    PrintVariableList(&levelVars);

    return qtrue;
}

qboolean G_GameVarsCmd(gentity_t *ent)
{
    gi.Printf("Game Variables\n");
    PrintVariableList(&gameVars);

    return qtrue;
}

qboolean G_ScriptCmd(gentity_t *ent)
{
    int         i, argc;
    const char *argv[32];
    char        args[32][64];

    argc = 0;
    for (i = 1; i < gi.argc(); i++) {
        if (argc < 32) {
            strncpy(args[argc], gi.argv(i), 64);
            argv[argc] = args[argc];
            argc++;
        }
    }
    if (argc > 0) {
        level.consoleThread->ProcessCommand(argc, argv);
    }

    return qtrue;
}

qboolean G_LocCmd(gentity_t *ent)
{
    if (ent) {
        gi.Printf(
            "Origin = ( %f, %f, %f )   Angles = ( %f, %f, %f )\n",
            ent->s.origin[0],
            ent->s.origin[1],
            ent->s.origin[2],
            ent->s.angles[0],
            ent->s.angles[1],
            ent->s.angles[0]
        );
    }

    return qtrue;
}

qboolean G_WarpCmd(gentity_t *ent)
{
    Vector pos;

    if (sv_cheats->integer == 0) {
        return qtrue;
    }

    if (ent && ent->entity) {
        if (gi.argc() == 2) {
            pos = Vector(gi.argv(1));
        } else if (gi.argc() == 4) {
            pos[0] = atof(gi.argv(1));
            pos[1] = atof(gi.argv(2));
            pos[2] = atof(gi.argv(3));
        } else {
            gi.Printf("Incorrect parms\n");
            return qfalse;
        }

        ent->entity->setOrigin(pos);
    }

    return qtrue;
}

qboolean G_MaskCmd(gentity_t *ent)
{
    if (gi.argc() != 2) {
        gi.Printf("Incorrect parms\n");
        return qtrue;
    }

    if (ent && ent->entity) {
        if (Q_stricmp(gi.argv(1), "monster") == 0) {
            ent->entity->edict->clipmask = MASK_MONSTERSOLID;
        } else if (Q_stricmp(gi.argv(1), "player") == 0) {
            ent->entity->edict->clipmask = MASK_PLAYERSOLID;
        } else {
            gi.Printf("Unknown mask name - %s\n", gi.argv(1));
        }
    }

    return qtrue;
}



// WDPrintf that retail's dialogrunthread path never emits.
qboolean G_DialogRunThread(gentity_t *ent)
{
    if (ent && ent->entity && ent->entity->isSubclassOf(Player)) {
        Player *player = (Player *)ent->entity;
        player->clearBranchDialogActor();
    }

    return G_ClientRunThreadCmd(ent);
}




qboolean G_EnableWidgetOfPlayer( const gentity_t *ent, const char *widgetName, qboolean enableFlag )
{
	assert( widgetName );

	str command("globalwidgetcommand ");
	command += widgetName ;
	command += ( enableFlag ) ? " enable" : " disable" ;

	return G_SendCommandToPlayer( ent, command.c_str());
}

qboolean G_SendCommandToPlayer( const gentity_t *ent, const char *command )
{
	assert( ent );
	Entity *entity = ent->entity ;
	
	assert( entity );
	assert( entity->isSubclassOf( Player ) );
	Player *player = ( Player* )entity ;
	
	str		builtCommand("stufftext \"");
	builtCommand += command ;
	builtCommand += "\"\n";
	gi.SendServerCommand( player->edict - g_entities, "%s", builtCommand.c_str() );
	
	return true ;
}

qboolean G_SetWidgetTextOfPlayer( const gentity_t *ent, const char *widgetName, const char *widgetText )
{
	assert( widgetName );
	char tmpstr[4104];
	
	strcpy(tmpstr, widgetText);
	
	int i;
	for ( i=0; i<( int )strlen(widgetText); i++ )
	{
		if ( tmpstr[i] == '\n' )
			tmpstr[i] = '~';
		if ( tmpstr[i] == ' ' )
			tmpstr[i] = '^';
	}
	
	str command("globalwidgetcommand ");
	command += widgetName ;
	command += " labeltext " ;
	command += tmpstr ;
	
	return G_SendCommandToPlayer( ent, command.c_str());
}

qboolean G_SendCommandToAllPlayers( const char *command )
{
	bool retVal = true ;

	for( unsigned int clientIdx = 0; clientIdx < ( unsigned int )maxclients->integer; ++clientIdx )
	{
		gentity_t *ent = g_entities + clientIdx ;
		if ( !ent->inuse || !ent->client || !ent->entity ) continue;

		if ( !G_SendCommandToPlayer( ent, command ) ) retVal = false ;
	}

	return retVal ;
}


qboolean G_CinematicCmd(gentity_t *ent)
{
    Event      *ev;
    const char *cmd;
    int         n = gi.argc();

    if (!n) {
        gi.WPrintf("Usage: cin <command> <arg1> ... <argn>\n");
        return true;
    }

    cmd = gi.argv(1);
    if (Event::Exists(cmd)) {
        ev = new Event(cmd);
        ev->SetSource(EV_FROM_CONSOLE);
        ev->SetConsoleEdict(NULL);

        for (int i = 2; i < n; i++) {
            ev->AddToken(gi.argv(i));
        }

        theCinematicArmature.ProcessEvent(ev);
    } else {
        gi.WPrintf("Unknown cinematic armature command '%s'.\n", cmd);
    }

    return true;
}

qboolean G_VTaunt(gentity_t *ent)
{
    return true;
}

qboolean G_TellCmd(gentity_t *ent)
{
    str         text;
    const char *p;
    int         i;
    int         entnum;

    if (gi.argc() < 3) {
        return true;
    }

    entnum = atoi(gi.argv(1));

    for (i = 2; i < gi.argc(); i++) {
        p = gi.argv(i);

        if (*p == '"') {
            p++;
            text += p;
            text[text.length() - 1] = 0;
        } else {
            text += p;
        }
    }

    if (ent->entity && ent->entity->isSubclassOf(Player)) {
        multiplayerManager.tell((Player *)ent->entity, text, entnum);
    }

    return true;
}

qboolean G_TeamSayCmd(gentity_t *ent)
{
    G_Say(ent, true, false);

    return true;
}

qboolean G_TauntCmd(gentity_t *ent)
{
    str tauntName;

    if (gi.argc() < 2) {
        return true;
    }

    tauntName = "taunt";
    tauntName += gi.argv(1);

    if (ent->entity && ent->entity->isSubclassOf(Player)) {
        Player *player = (Player *)ent->entity;

        if (multiplayerManager.inMultiplayer() && !multiplayerManager.isPlayerSpectator(player)) {
            ent->entity->Sound(tauntName, CHAN_TAUNT, DEFAULT_VOL, 250.0f);
        }
    }

    return true;
}

qboolean G_ClientRunThreadCmd(gentity_t *ent)
{
    str      threadName;
    CThread *thread;


    if (!gi.argc()) return true;

    threadName = gi.argv(1);




    if (!threadName.length()) return true;

    thread = Director.CreateThread(threadName);

    if (thread) thread->DelayedStart(0.0f);

    return true;
}

qboolean G_ClientSetVarCmd(gentity_t *ent)
{
    str varName;
    str value;

    if (gi.argc() != 3) return true;


    varName = gi.argv(1);


    value = gi.argv(2);




    if (varName.length() && value.length()) {
        levelVars.SetVariable(varName, value);
    }

    return true;
}

qboolean G_SetGameplayFloatCmd(gentity_t *ent)
{
    str   objname;
    str   propname;
    str   create = "0";
    float value = 1.0f;

    if (gi.argc() < 4) return qfalse;

    objname = gi.argv(1);
    propname = gi.argv(2);
    value = (float)atof(gi.argv(3));
    if (gi.argc() > 4) create = gi.argv(4);

    if (create == "0")
        GameplayManager::getTheGameplayManager()->setFloatValue(objname, propname, value);
    else
        GameplayManager::getTheGameplayManager()->setFloatValue(objname, propname, value, true);

    gi.Printf("Gameplay Modified -- %s's %s changed to %g\n", objname.c_str(), propname.c_str(), value);

    return qtrue;
}

qboolean G_SetGameplayStringCmd(gentity_t *ent)
{
    str objname;
    str propname;
    str valuestr;
    str create = "0";

    if (gi.argc() < 4) return qfalse;

    objname = gi.argv(1);
    propname = gi.argv(2);
    valuestr = gi.argv(3);
    if (gi.argc() > 4) create = gi.argv(4);

    if (create == "0")
        GameplayManager::getTheGameplayManager()->setStringValue(objname, propname, valuestr);
    else
        GameplayManager::getTheGameplayManager()->setStringValue(objname, propname, valuestr, true);

    gi.Printf("Gameplay Modified -- %s's %s changed to %s\n", objname.c_str(), propname.c_str(), valuestr.c_str());

    return qtrue;
}

qboolean G_PurchaseSkillCmd(gentity_t *ent)
{
    str propname;

    if (gi.argc() < 1) return qfalse;

    propname = gi.argv(1);

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) return false;

    float skillPoints = gpm->getFloatValue("CurrentPlayer", "SkillPoints");
    if (skillPoints > 0.0f) {
        str   objName("CurrentPlayer.");
        objName += propname;
        float skillValue = gpm->getFloatValue(objName, "value");
        if (skillValue < gpm->getFloatValue(objName, "max")) {
            gpm->setFloatValue(objName, "value", skillValue + 1.0f);
            gpm->setFloatValue("CurrentPlayer", "SkillPoints", skillPoints - 1.0f);

            if (ent->entity->isSubclassOf(Player)) {
                Player *player = (Player *)ent->entity;
                player->skillChanged(objName);
            }
        }
    }

    return qtrue;
}

qboolean G_SwapItemCmd(gentity_t *ent)
{
    if (gi.argc() < 1) return qfalse;
    if (!ent->entity->isSubclassOf(Player)) return qfalse;

    Player       *player = (Player *)ent->entity;
    str           objectName = gi.argv(1);
    str           propertyName = gi.argv(2);
    str           heldItem;
    weaponhand_t  hand = WEAPON_RIGHT;

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) return false;

    str itemName = gpm->getStringValue(objectName, propertyName);
    str tikiName = gpm->getStringValue(itemName, "model");
    str itemType = gpm->getStringValue(itemName, "class");
    str playerItem("CurrentPlayer." + itemType);

    if (!gpm->hasObject(playerItem)) {
        gi.WPrintf("Warning: Unknown item type %s for item %s", itemType.c_str(), itemName.c_str());
        return qfalse;
    }

    heldItem = gpm->getStringValue(playerItem, "name");
    gpm->setStringValue(playerItem, "name", itemName);

    if (itemType == "Ranged") hand = WEAPON_LEFT;

    player->takeItem(heldItem.c_str());

    player->giveItem(tikiName);

    Weapon *weap = 0;
    weap = player->GetActiveWeapon(WEAPON_RIGHT);
    if (!weap) weap = player->GetActiveWeapon(WEAPON_LEFT);
    if (weap) {
        str tmpstr = gpm->getStringValue(weap->getArchetype(), "class");
        if (tmpstr == itemType) player->useWeapon(itemType, hand);
    }

    gpm->setStringValue(objectName, propertyName, heldItem);

    return qtrue;
}

qboolean G_DropItemCmd(gentity_t *ent)
{
    if (gi.argc() < 1) return false;
    if (!ent->entity->isSubclassOf(Player)) return false;

    Player *player = (Player *)ent->entity;
    str     objectName = gi.argv(1);
    str     propertyName = gi.argv(2);

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) return false;

    str itemName = gpm->getStringValue(objectName, propertyName);
    if (itemName == "Empty") return false;

    str tikiName = gpm->getStringValue(itemName, "model");
    str itemType = gpm->getStringValue(itemName, "class");
    str playerItem("CurrentPlayer." + itemType);

    if (!gpm->hasObject(playerItem)) {
        gi.WPrintf("Warning: Unknown item type %s for item %s", itemType.c_str(), itemName.c_str());
        return false;
    }

    gpm->setStringValue(objectName, propertyName, "Empty");


    Weapon   *dropweap;
    ClassDef *cls;
    cls = getClass(tikiName.c_str());
    if (!cls) {
        SpawnArgs args;
        args.setArg("model", tikiName.c_str());
        cls = args.getClassDef();
        if (!cls) return false;
    }
    dropweap = (Weapon *)cls->newInstance();
    dropweap->setModel(tikiName.c_str());
    dropweap->ProcessPendingEvents();
    dropweap->SetOwner(player);
    dropweap->hideModel();
    dropweap->setAttached(true);
    dropweap->Drop();

    return true;
}
