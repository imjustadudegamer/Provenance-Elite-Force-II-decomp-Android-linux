//-----------------------------------------------------------------------------
//
// Copyright (C) 1998 by Ritual Entertainment, Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without
// expressly written permission by Ritual Entertainment, Inc.
//
//-----------------------------------------------------------------------------
// SPDX-License-Identifier: LicenseRef-Ritual-EF2-SDK
// Modified 2026-10-04 for this port.  The Ritual Elite Force II SDK agreement
// permits non-commercial distribution of the SDK and its New Creations
// with the agreement attached (docs/EF2-SDK-licenseAgreement.txt); it
// forbids derivative works (section 2(j)) and requires the notice above
// to stay intact (section 2(k)).


#include "g_local.h"
#include "../qcommon/qcommon.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_ea.h"
#include "be_ai_char.h"
#include "be_ai_chat.h"
#include "be_ai_gen.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"
#include "be_ai_weap.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_team.h"
#include "chars.h"				
#include "inv.h"				
#include "syn.h"				
#include "match.h"				

#include "botmenudef.h"

int notleader[MAX_CLIENTS];

#ifdef DEBUG
void BotPrintTeamGoal(bot_state_t *bs) {
	char netname[MAX_NETNAME];
	float t;

	ClientName(bs->client, netname, sizeof(netname));
	t = bs->teamgoal_time - FloatTime();
	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna help a team mate for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna accompany a team mate for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_GETFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna get the flag for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_RUSHBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna rush to the base for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_RETURNFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna try to return the flag for %1.0f secs\n", netname, t);
			break;
		}
#ifdef MISSIONPACK
		case LTG_ATTACKENEMYBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna attack the enemy base for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_HARVEST:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna harvest for %1.0f secs\n", netname, t);
			break;
		}
#endif
		case LTG_DEFENDKEYAREA:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna defend a key area for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_GETITEM:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna get an item for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_KILL:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna kill someone for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna camp for %1.0f secs\n", netname, t);
			break;
		}
		case LTG_PATROL:
		{
			BotAI_Print(PRT_MESSAGE, "%s: I'm gonna patrol for %1.0f secs\n", netname, t);
			break;
		}
		default:
		{
			if (bs->ctfroam_time > FloatTime()) {
				t = bs->ctfroam_time - FloatTime();
				BotAI_Print(PRT_MESSAGE, "%s: I'm gonna roam for %1.0f secs\n", netname, t);
			}
			else {
				BotAI_Print(PRT_MESSAGE, "%s: I've got a regular goal\n", netname);
			}
		}
	}
}
#endif 

int BotGetItemTeamGoal(char *goalname, bot_goal_t *goal) {
	int i;

	if (!strlen(goalname)) return qfalse;
	i = -1;
	do {
		i = gi.BotGetLevelItemGoal(i, goalname, goal);
		if (i > 0) {
			if (goal->flags & GFL_DROPPED)
				continue;
			return qtrue;
		}
	} while(i > 0);
	return qfalse;
}

int BotGetMessageTeamGoal(bot_state_t *bs, char *goalname, bot_goal_t *goal) {
	bot_waypoint_t *cp;

	if (BotGetItemTeamGoal(goalname, goal)) return qtrue;

	cp = BotFindWayPoint(bs->checkpoints, goalname);
	if (cp) {
		memcpy(goal, &cp->goal, sizeof(bot_goal_t));
		return qtrue;
	}
	return qfalse;
}

float BotGetTime(bot_match_t *match) {
	bot_match_t timematch;
	char timestring[MAX_MESSAGE_SIZE];
	float t;

	if (match->subtype & ST_TIME) {
		gi.BotMatchVariable(match, TIME, timestring, MAX_MESSAGE_SIZE);
		if (gi.BotFindMatch(timestring, &timematch, MTCONTEXT_TIME)) {
			if (timematch.type == MSG_FOREVER) {
				t = 99999999.0f;
			}
			else if (timematch.type == MSG_FORAWHILE) {
				t = 10 * 60; 
			}
			else if (timematch.type == MSG_FORALONGTIME) {
				t = 30 * 60; 
			}
			else {
				gi.BotMatchVariable(&timematch, TIME, timestring, MAX_MESSAGE_SIZE);
				if (timematch.type == MSG_MINUTES) t = atof(timestring) * 60;
				else if (timematch.type == MSG_SECONDS) t = atof(timestring);
				else t = 0;
			}
			if (t > 0) return FloatTime() + t;
		}
	}
	return 0;
}

int FindClientByName(char *name) {
	int i;
	char buf[MAX_INFO_STRING];
	static int maxclients;

	if (!maxclients)
		maxclients = gi.Cvar_VariableIntegerValue("sv_maxclients");
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		ClientName(i, buf, sizeof(buf));
		if (!Q_stricmp(buf, name)) return i;
	}
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		ClientName(i, buf, sizeof(buf));
		if (stristr(buf, name)) return i;
	}
	return -1;
}

int FindEnemyByName(bot_state_t *bs, char *name) {
	int i;
	char buf[MAX_INFO_STRING];
	static int maxclients;

	if (!maxclients)
		maxclients = gi.Cvar_VariableIntegerValue("sv_maxclients");
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		if (BotSameTeam(bs, i)) continue;
		ClientName(i, buf, sizeof(buf));
		if (!Q_stricmp(buf, name)) return i;
	}
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		if (BotSameTeam(bs, i)) continue;
		ClientName(i, buf, sizeof(buf));
		if (stristr(buf, name)) return i;
	}
	return -1;
}

int NumPlayersOnSameTeam(bot_state_t *bs) {
	int i, num;
	char buf[MAX_INFO_STRING];
	static int maxclients;

	if (!maxclients)
		maxclients = gi.Cvar_VariableIntegerValue("sv_maxclients");

	num = 0;
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		Q_strncpyz(buf, gi.getConfigstring(CS_PLAYERS+i), MAX_INFO_STRING);
		if (strlen(buf)) {
			if (BotSameTeam(bs, i+1)) num++;
		}
	}
	return num;
}

int BotGetPatrolWaypoints(bot_state_t *bs, bot_match_t *match) {
	char keyarea[MAX_MESSAGE_SIZE];
	int patrolflags;
	bot_waypoint_t *wp, *newwp, *newpatrolpoints;
	bot_match_t keyareamatch;
	bot_goal_t goal;

	newpatrolpoints = NULL;
	patrolflags = 0;
	gi.BotMatchVariable(match, KEYAREA, keyarea, MAX_MESSAGE_SIZE);
	while(1) {
		if (!gi.BotFindMatch(keyarea, &keyareamatch, MTCONTEXT_PATROLKEYAREA)) {
			gi.EA_SayTeam(bs->client, "what do you say?");
			BotFreeWaypoints(newpatrolpoints);
			bs->patrolpoints = NULL;
			return qfalse;
		}
		gi.BotMatchVariable(&keyareamatch, KEYAREA, keyarea, MAX_MESSAGE_SIZE);
		if (!BotGetMessageTeamGoal(bs, keyarea, &goal)) {
			BotFreeWaypoints(newpatrolpoints);
			bs->patrolpoints = NULL;
			return qfalse;
		}
		newwp = BotCreateWayPoint(keyarea, goal.origin, goal.areanum);
		if (!newwp)
			break;
		newwp->next = NULL;
		for (wp = newpatrolpoints; wp && wp->next; wp = wp->next);
		if (!wp) {
			newpatrolpoints = newwp;
			newwp->prev = NULL;
		}
		else {
			wp->next = newwp;
			newwp->prev = wp;
		}
		if (keyareamatch.subtype & ST_BACK) {
			patrolflags = PATROL_LOOP;
			break;
		}
		else if (keyareamatch.subtype & ST_REVERSE) {
			patrolflags = PATROL_REVERSE;
			break;
		}
		else if (keyareamatch.subtype & ST_MORE) {
			gi.BotMatchVariable(&keyareamatch, MORE, keyarea, MAX_MESSAGE_SIZE);
		}
		else {
			break;
		}
	}
	if (!newpatrolpoints || !newpatrolpoints->next) {
		gi.EA_SayTeam(bs->client, "I need more key points to patrol\n");
		BotFreeWaypoints(newpatrolpoints);
		newpatrolpoints = NULL;
		return qfalse;
	}
	BotFreeWaypoints(bs->patrolpoints);
	bs->patrolpoints = newpatrolpoints;
	bs->curpatrolpoint = bs->patrolpoints;
	bs->patrolflags = patrolflags;
	return qtrue;
}

int BotAddressedToBot(bot_state_t *bs, bot_match_t *match) {
	char addressedto[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char name[MAX_MESSAGE_SIZE];
	char botname[128];
	int client;
	bot_match_t addresseematch;

	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientOnSameTeamFromName(bs, netname);
	if (client < 0) return qfalse;
	if (match->subtype & ST_ADDRESSED) {
		gi.BotMatchVariable(match, ADDRESSEE, addressedto, sizeof(addressedto));
		ClientName(bs->client, botname, 128);
		while(gi.BotFindMatch(addressedto, &addresseematch, MTCONTEXT_ADDRESSEE)) {
			if (addresseematch.type == MSG_EVERYONE) {
				return qtrue;
			}
			else if (addresseematch.type == MSG_MULTIPLENAMES) {
				gi.BotMatchVariable(&addresseematch, TEAMMATE, name, sizeof(name));
				if (strlen(name)) {
					if (stristr(botname, name)) return qtrue;
					if (stristr(bs->subteam, name)) return qtrue;
				}
				gi.BotMatchVariable(&addresseematch, MORE, addressedto, MAX_MESSAGE_SIZE);
			}
			else {
				gi.BotMatchVariable(&addresseematch, TEAMMATE, name, MAX_MESSAGE_SIZE);
				if (strlen(name)) {
					if (stristr(botname, name)) return qtrue;
					if (stristr(bs->subteam, name)) return qtrue;
				}
				break;
			}
		}
		return qfalse;
	}
	else {
		bot_match_t tellmatch;

		tellmatch.type = 0;
		if (!gi.BotFindMatch(match->string, &tellmatch, MTCONTEXT_REPLYCHAT) ||
				tellmatch.type != MSG_CHATTELL) {
			if (random() > (float ) 1.0 / (NumPlayersOnSameTeam(bs)-1)) return qfalse;
		}
	}
	return qtrue;
}

int BotGPSToPosition(char *buf, vec3_t position) {
	int i, j = 0;
	int num, sign;

	for (i = 0; i < 3; i++) {
		num = 0;
		while(buf[j] == ' ') j++;
		if (buf[j] == '-') {
			j++;
			sign = -1;
		}
		else {
			sign = 1;
		}
		while (buf[j]) {
			if (buf[j] >= '0' && buf[j] <= '9') {
				num = num * 10 + buf[j] - '0';
				j++;
			}
			else {
				j++;
				break;
			}
		}
		BotAI_Print(PRT_MESSAGE, "%d\n", sign * num);
		position[i] = (float) sign * num;
	}
	return qtrue;
}

void BotMatch_HelpAccompany(bot_state_t *bs, bot_match_t *match) {
	int client, other, areanum;
	char teammate[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char itemname[MAX_MESSAGE_SIZE];
	bot_match_t teammatematch;
	aas_entityinfo_t entinfo;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
	if (gi.BotFindMatch(teammate, &teammatematch, MTCONTEXT_TEAMMATE) &&
			teammatematch.type == MSG_ME) {
		gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		client = ClientFromName(netname);
		other = qfalse;
	}
	else {
		client = FindClientByName(teammate);
		if (client == bs->client) {
			other = qfalse;
		}
		else if (!BotSameTeam(bs, client)) {
			return;
		}
		else {
			other = qtrue;
		}
	}
	if (client < 0) {
		if (other) BotAI_BotInitialChat(bs, "whois", teammate, NULL);
		else BotAI_BotInitialChat(bs, "whois", netname, NULL);
		client = ClientFromName(netname);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
		return;
	}
	if (client == bs->client) {
		return;
	}
	bs->teamgoal.entitynum = -1;
	BotEntityInfo(client, &entinfo);
	if (entinfo.valid) {
		areanum = BotPointAreaNum(entinfo.origin);
		if (areanum) {
			bs->teamgoal.entitynum = client;
			bs->teamgoal.areanum = areanum;
			VectorCopy(entinfo.origin, bs->teamgoal.origin);
			VectorSet(bs->teamgoal.mins, -8, -8, -8);
			VectorSet(bs->teamgoal.maxs, 8, 8, 8);
		}
	}
	if (bs->teamgoal.entitynum < 0) {
		if (match->subtype & ST_NEARITEM) {
			gi.BotMatchVariable(match, ITEM, itemname, sizeof(itemname));
			if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
				return;
			}
		}
	}
	if (bs->teamgoal.entitynum < 0) {
		if (other) BotAI_BotInitialChat(bs, "whereis", teammate, NULL);
		else BotAI_BotInitialChat(bs, "whereareyou", netname, NULL);
		client = ClientFromName(netname);
		gi.BotEnterChat(bs->cs, client, CHAT_TEAM);
		return;
	}
	bs->teammate = client;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientFromName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammatevisible_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->teamgoal_time = BotGetTime(match);
	if (match->type == MSG_HELP) {
		bs->ltgtype = LTG_TEAMHELP;
		if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_HELP_TIME;
	}
	else {
		bs->ltgtype = LTG_TEAMACCOMPANY;
		if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
		bs->formation_dist = 3.5 * 32;		
		bs->arrive_time = 0;
		BotSetTeamStatus(bs);
		BotRememberLastOrderedTask(bs);
	}
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_DefendKeyArea(bot_state_t *bs, bot_match_t *match) {
	char itemname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, KEYAREA, itemname, sizeof(itemname));
	if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
		return;
	}
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientFromName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_DEFENDKEYAREA;
	bs->teamgoal_time = BotGetTime(match);
	if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
	bs->defendaway_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_GetItem(bot_state_t *bs, bot_match_t *match) {
	char itemname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, ITEM, itemname, sizeof(itemname));
	if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
		return;
	}
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientOnSameTeamFromName(bs, netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_GETITEM;
	bs->teamgoal_time = FloatTime() + TEAM_GETITEM_TIME;
	BotSetTeamStatus(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_Camp(bot_state_t *bs, bot_match_t *match) {
	int client, areanum;
	char netname[MAX_MESSAGE_SIZE];
	char itemname[MAX_MESSAGE_SIZE];
	aas_entityinfo_t entinfo;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	if (client < 0) {
		BotAI_BotInitialChat(bs, "whois", netname, NULL);
		gi.BotEnterChat(bs->cs, bs->client, CHAT_TEAM);
		return;
	}
	gi.BotMatchVariable(match, KEYAREA, itemname, sizeof(itemname));
	if (match->subtype & ST_THERE) {
		bs->teamgoal.entitynum = bs->entitynum;
		bs->teamgoal.areanum = bs->areanum;
		VectorCopy(bs->origin, bs->teamgoal.origin);
		VectorSet(bs->teamgoal.mins, -8, -8, -8);
		VectorSet(bs->teamgoal.maxs, 8, 8, 8);
	}
	else if (match->subtype & ST_HERE) {
		if (client == bs->client) return;
		bs->teamgoal.entitynum = -1;
		BotEntityInfo(client, &entinfo);
		if (entinfo.valid) {
			areanum = BotPointAreaNum(entinfo.origin);
			if (areanum) {
					bs->teamgoal.entitynum = client;
					bs->teamgoal.areanum = areanum;
					VectorCopy(entinfo.origin, bs->teamgoal.origin);
					VectorSet(bs->teamgoal.mins, -8, -8, -8);
					VectorSet(bs->teamgoal.maxs, 8, 8, 8);
			}
		}
		if (bs->teamgoal.entitynum < 0) {
			BotAI_BotInitialChat(bs, "whereareyou", netname, NULL);
			client = ClientFromName(netname);
			gi.BotEnterChat(bs->cs, client, CHAT_TELL);
			return;
		}
	}
	else if (!BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal)) {
		return;
	}
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_CAMPORDER;
	bs->teamgoal_time = BotGetTime(match);
	if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_CAMP_TIME;
	bs->arrive_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_Patrol(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	if (!BotGetPatrolWaypoints(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_PATROL;
	bs->teamgoal_time = BotGetTime(match);
	if (!bs->teamgoal_time) bs->teamgoal_time = FloatTime() + TEAM_PATROL_TIME;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_GetFlag(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (gametype == GT_CTF) {
		if (!ctf_redflag.areanum || !ctf_blueflag.areanum)
			return;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (!ctf_neutralflag.areanum || !ctf_redflag.areanum || !ctf_blueflag.areanum)
			return;
	}
#endif
	else {
		return;
	}
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_GETFLAG;
	bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
	if (gametype == GT_CTF) {
		BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
	}
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_AttackEnemyBase(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (gametype == GT_CTF) {
		BotMatch_GetFlag(bs, match);
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF || gametype == GT_OBELISK || gametype == GT_HARVESTER) {
		if (!redobelisk.areanum || !blueobelisk.areanum)
			return;
	}
#endif
	else {
		return;
	}
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_ATTACKENEMYBASE;
	bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
	bs->attackaway_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

#ifdef MISSIONPACK
void BotMatch_Harvest(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (gametype == GT_HARVESTER) {
		if (!neutralobelisk.areanum || !redobelisk.areanum || !blueobelisk.areanum)
			return;
	}
	else {
		return;
	}
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_HARVEST;
	bs->teamgoal_time = FloatTime() + TEAM_HARVEST_TIME;
	bs->harvestaway_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}
#endif

void BotMatch_RushBase(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (gametype == GT_CTF) {
		if (!ctf_redflag.areanum || !ctf_blueflag.areanum)
			return;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF || gametype == GT_HARVESTER) {
		if (!redobelisk.areanum || !blueobelisk.areanum)
			return;
	}
#endif
	else {
		return;
	}
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_RUSHBASE;
	bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
	bs->rushbaseaway_time = 0;
	BotSetTeamStatus(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_TaskPreference(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_NETNAME];
	char teammatename[MAX_MESSAGE_SIZE];
	int teammate, preference;

	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) != 0) return;

	gi.BotMatchVariable(match, NETNAME, teammatename, sizeof(teammatename));
	teammate = ClientFromName(teammatename);
	if (teammate < 0) return;

	preference = BotGetTeamMateTaskPreference(bs, teammate);
	switch(match->subtype)
	{
		case ST_DEFENDER:
		{
			preference &= ~TEAMTP_ATTACKER;
			preference |= TEAMTP_DEFENDER;
			break;
		}
		case ST_ATTACKER:
		{
			preference &= ~TEAMTP_DEFENDER;
			preference |= TEAMTP_ATTACKER;
			break;
		}
		case ST_ROAMER:
		{
			preference &= ~(TEAMTP_ATTACKER|TEAMTP_DEFENDER);
			break;
		}
	}
	BotSetTeamMateTaskPreference(bs, teammate, preference);
	EasyClientName(teammate, teammatename, sizeof(teammatename));
	BotAI_BotInitialChat(bs, "keepinmind", teammatename, NULL);
	gi.BotEnterChat(bs->cs, teammate, CHAT_TELL);
	BotVoiceChatOnly(bs, teammate, VOICECHAT_YES);
	gi.EA_Action(bs->client, ACTION_AFFIRMATIVE);
}

void BotMatch_ReturnFlag(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (
		gametype != GT_CTF
#ifdef MISSIONPACK
		&& gametype != GT_1FCTF
#endif
		)
		return;
	if (!BotAddressedToBot(bs, match))
		return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_RETURNFLAG;
	bs->teamgoal_time = FloatTime() + CTF_RETURNFLAG_TIME;
	bs->rushbaseaway_time = 0;
	BotSetTeamStatus(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_JoinSubteam(bot_state_t *bs, bot_match_t *match) {
	char teammate[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, TEAMNAME, teammate, sizeof(teammate));
	Q_strncpyz(bs->subteam, teammate, 32);
	bs->subteam[31] = '\0';
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	BotAI_BotInitialChat(bs, "joinedteam", teammate, NULL);
	client = ClientFromName(netname);
	gi.BotEnterChat(bs->cs, client, CHAT_TELL);
}

void BotMatch_LeaveSubteam(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	if (strlen(bs->subteam))
	{
		BotAI_BotInitialChat(bs, "leftteam", bs->subteam, NULL);
		gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		client = ClientFromName(netname);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
	} 
	strcpy(bs->subteam, "");
}

void BotMatch_WhichTeam(bot_state_t *bs, bot_match_t *match) {
	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	if (strlen(bs->subteam)) {
		BotAI_BotInitialChat(bs, "inteam", bs->subteam, NULL);
	}
	else {
		BotAI_BotInitialChat(bs, "noteam", NULL);
	}
	gi.BotEnterChat(bs->cs, bs->client, CHAT_TEAM);
}

void BotMatch_CheckPoint(bot_state_t *bs, bot_match_t *match) {
	int areanum, client;
	char buf[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	vec3_t position;
	bot_waypoint_t *cp;

	if (!TeamPlayIsOn()) return;
	gi.BotMatchVariable(match, POSITION, buf, MAX_MESSAGE_SIZE);
	VectorClear(position);
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientFromName(netname);
	sscanf(buf, "%f %f %f", &position[0], &position[1], &position[2]);
	position[2] += 0.5;
	areanum = BotPointAreaNum(position);
	if (!areanum) {
		if (BotAddressedToBot(bs, match)) {
			BotAI_BotInitialChat(bs, "checkpoint_invalid", NULL);
			gi.BotEnterChat(bs->cs, client, CHAT_TELL);
		}
		return;
	}
	gi.BotMatchVariable(match, NAME, buf, MAX_MESSAGE_SIZE);
	cp = BotFindWayPoint(bs->checkpoints, buf);
	if (cp) {
		if (cp->next) cp->next->prev = cp->prev;
		if (cp->prev) cp->prev->next = cp->next;
		else bs->checkpoints = cp->next;
		cp->inuse = qfalse;
	}
	cp = BotCreateWayPoint(buf, position, areanum);
	cp->next = bs->checkpoints;
	if (bs->checkpoints) bs->checkpoints->prev = cp;
	bs->checkpoints = cp;
	if (BotAddressedToBot(bs, match)) {
		Com_sprintf(buf, sizeof(buf), "%1.0f %1.0f %1.0f", cp->goal.origin[0],
													cp->goal.origin[1],
													cp->goal.origin[2]);

		BotAI_BotInitialChat(bs, "checkpoint_confirm", cp->name, buf, NULL);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
	}
}

void BotMatch_FormationSpace(bot_state_t *bs, bot_match_t *match) {
	char buf[MAX_MESSAGE_SIZE];
	float space;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NUMBER, buf, MAX_MESSAGE_SIZE);
	if (match->subtype & ST_FEET) space = 0.3048 * 32 * atof(buf);
	else space = 32 * atof(buf);
	if (space < 48 || space > 500) space = 100;
	bs->formation_dist = space;
}

void BotMatch_Dismiss(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientFromName(netname);
	bs->decisionmaker = client;
	bs->ltgtype = 0;
	bs->lead_time = 0;
	bs->lastgoal_ltgtype = 0;
	BotAI_BotInitialChat(bs, "dismissed", NULL);
	gi.BotEnterChat(bs->cs, client, CHAT_TELL);
}

void BotMatch_Suicide(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	gi.EA_Command(bs->client, "kill");
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientFromName(netname);
	BotVoiceChat(bs, client, VOICECHAT_TAUNT);
	gi.EA_Action(bs->client, ACTION_AFFIRMATIVE);
}

void BotMatch_StartTeamLeaderShip(bot_state_t *bs, bot_match_t *match) {
	int client;
	char teammate[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;
	if (match->subtype & ST_I) {
		gi.BotMatchVariable(match, NETNAME, teammate, sizeof(teammate));
		Q_strncpyz(bs->teamleader, teammate, sizeof(bs->teamleader));
		bs->teamleader[sizeof(bs->teamleader) - 1] = '\0';
	}
	else {
		gi.BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
		client = FindClientByName(teammate);
		if (client >= 0) ClientName(client, bs->teamleader, sizeof(bs->teamleader));
	}
}

void BotMatch_StopTeamLeaderShip(bot_state_t *bs, bot_match_t *match) {
	int client;
	char teammate[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;
	gi.BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
	if (match->subtype & ST_I) {
		gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		client = FindClientByName(netname);
	}
	else {
		client = FindClientByName(teammate);
	} 
	if (client >= 0) {
		if (!Q_stricmp(bs->teamleader, ClientName(client, netname, sizeof(netname)))) {
			bs->teamleader[0] = '\0';
			notleader[client] = qtrue;
		}
	}
}

void BotMatch_WhoIsTeamLeader(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;

	ClientName(bs->client, netname, sizeof(netname));
	if (!Q_stricmp(netname, bs->teamleader)) {
		gi.EA_SayTeam(bs->client, "I'm the team leader\n");
	}
}

void BotMatch_WhatAreYouDoing(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_MESSAGE_SIZE];
	char goalname[MAX_MESSAGE_SIZE];
	int client;

	if (!BotAddressedToBot(bs, match)) return;
	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			EasyClientName(bs->teammate, netname, sizeof(netname));
			BotAI_BotInitialChat(bs, "helping", netname, NULL);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			EasyClientName(bs->teammate, netname, sizeof(netname));
			BotAI_BotInitialChat(bs, "accompanying", netname, NULL);
			break;
		}
		case LTG_DEFENDKEYAREA:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_BotInitialChat(bs, "defending", goalname, NULL);
			break;
		}
		case LTG_GETITEM:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_BotInitialChat(bs, "gettingitem", goalname, NULL);
			break;
		}
		case LTG_KILL:
		{
			ClientName(bs->teamgoal.entitynum, netname, sizeof(netname));
			BotAI_BotInitialChat(bs, "killing", netname, NULL);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			BotAI_BotInitialChat(bs, "camping", NULL);
			break;
		}
		case LTG_PATROL:
		{
			BotAI_BotInitialChat(bs, "patrolling", NULL);
			break;
		}
		case LTG_GETFLAG:
		{
			BotAI_BotInitialChat(bs, "capturingflag", NULL);
			break;
		}
		case LTG_RUSHBASE:
		{
			BotAI_BotInitialChat(bs, "rushingbase", NULL);
			break;
		}
		case LTG_RETURNFLAG:
		{
			BotAI_BotInitialChat(bs, "returningflag", NULL);
			break;
		}
#ifdef MISSIONPACK
		case LTG_ATTACKENEMYBASE:
		{
			BotAI_BotInitialChat(bs, "attackingenemybase", NULL);
			break;
		}
		case LTG_HARVEST:
		{
			BotAI_BotInitialChat(bs, "harvesting", NULL);
			break;
		}
#endif
		default:
		{
			BotAI_BotInitialChat(bs, "roaming", NULL);
			break;
		}
	}
	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = ClientFromName(netname);
	gi.BotEnterChat(bs->cs, client, CHAT_TELL);
}

void BotMatch_WhatIsMyCommand(bot_state_t *bs, bot_match_t *match) {
	char netname[MAX_NETNAME];

	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) != 0) return;
	bs->forceorders = qtrue;
}

float BotNearestVisibleItem(bot_state_t *bs, char *itemname, bot_goal_t *goal) {
	int i;
	char name[64];
	bot_goal_t tmpgoal;
	float dist, bestdist;
	vec3_t dir;
	bsp_trace_t trace;

	bestdist = 999999;
	i = -1;
	do {
		i = gi.BotGetLevelItemGoal(i, itemname, &tmpgoal);
		gi.BotGoalName(tmpgoal.number, name, sizeof(name));
		if (Q_stricmp(itemname, name) != 0)
			continue;
		VectorSubtract(tmpgoal.origin, bs->origin, dir);
		dist = VectorLength(dir);
		if (dist < bestdist) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, tmpgoal.origin, bs->client, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
			if (trace.fraction >= 1.0) {
				bestdist = dist;
				memcpy(goal, &tmpgoal, sizeof(bot_goal_t));
			}
		}
	} while(i > 0);
	return bestdist;
}

void BotMatch_WhereAreYou(bot_state_t *bs, bot_match_t *match) {
	float dist, bestdist;
	int i, bestitem, redtt, bluett, client;
	bot_goal_t goal;
	char netname[MAX_MESSAGE_SIZE];
	char *nearbyitems[] = {
		"Shotgun",
		"Grenade Launcher",
		"Rocket Launcher",
		"Plasmagun",
		"Railgun",
		"Lightning Gun",
		"BFG10K",
		"Quad Damage",
		"Regeneration",
		"Battle Suit",
		"Speed",
		"Invisibility",
		"Flight",
		"Armor",
		"Heavy Armor",
		"Red Flag",
		"Blue Flag",
#ifdef MISSIONPACK
		"Nailgun",
		"Prox Launcher",
		"Chaingun",
		"Scout",
		"Guard",
		"Doubler",
		"Ammo Regen",
		"Neutral Flag",
		"Red Obelisk",
		"Blue Obelisk",
		"Neutral Obelisk",
#endif
		NULL
	};
	if (!TeamPlayIsOn())
		return;
	if (!BotAddressedToBot(bs, match))
		return;

	bestitem = -1;
	bestdist = 999999;
	for (i = 0; nearbyitems[i]; i++) {
		dist = BotNearestVisibleItem(bs, nearbyitems[i], &goal);
		if (dist < bestdist) {
			bestdist = dist;
			bestitem = i;
		}
	}
	if (bestitem != -1) {
		if (gametype == GT_CTF
#ifdef MISSIONPACK
			|| gametype == GT_1FCTF
#endif
			) {
			redtt = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, ctf_redflag.areanum, TFL_DEFAULT);
			bluett = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, ctf_blueflag.areanum, TFL_DEFAULT);
			if (redtt < (redtt + bluett) * 0.4) {
				BotAI_BotInitialChat(bs, "teamlocation", nearbyitems[bestitem], "red", NULL);
			}
			else if (bluett < (redtt + bluett) * 0.4) {
				BotAI_BotInitialChat(bs, "teamlocation", nearbyitems[bestitem], "blue", NULL);
			}
			else {
				BotAI_BotInitialChat(bs, "location", nearbyitems[bestitem], NULL);
			}
		}
#ifdef MISSIONPACK
		else if (gametype == GT_OBELISK || gametype == GT_HARVESTER) {
			redtt = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, redobelisk.areanum, TFL_DEFAULT);
			bluett = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, blueobelisk.areanum, TFL_DEFAULT);
			if (redtt < (redtt + bluett) * 0.4) {
				BotAI_BotInitialChat(bs, "teamlocation", nearbyitems[bestitem], "red", NULL);
			}
			else if (bluett < (redtt + bluett) * 0.4) {
				BotAI_BotInitialChat(bs, "teamlocation", nearbyitems[bestitem], "blue", NULL);
			}
			else {
				BotAI_BotInitialChat(bs, "location", nearbyitems[bestitem], NULL);
			}
		}
#endif
		else {
			BotAI_BotInitialChat(bs, "location", nearbyitems[bestitem], NULL);
		}
		gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		client = ClientFromName(netname);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
	}
}

void BotMatch_LeadTheWay(bot_state_t *bs, bot_match_t *match) {
	aas_entityinfo_t entinfo;
	char netname[MAX_MESSAGE_SIZE], teammate[MAX_MESSAGE_SIZE];
	int client, areanum, other;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;
	if (match->subtype & ST_SOMEONE) {
		gi.BotMatchVariable(match, TEAMMATE, teammate, sizeof(teammate));
		client = FindClientByName(teammate);
		if (client == bs->client) {
			other = qfalse;
		}
		else if (!BotSameTeam(bs, client)) {
			return;
		}
		else {
			other = qtrue;
		}
	}
	else {
		gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		client = ClientFromName(netname);
		other = qfalse;
	}
	if (client < 0) {
		BotAI_BotInitialChat(bs, "whois", netname, NULL);
		gi.BotEnterChat(bs->cs, bs->client, CHAT_TEAM);
		return;
	}
	bs->lead_teamgoal.entitynum = -1;
	BotEntityInfo(client, &entinfo);
	if (entinfo.valid) {
		areanum = BotPointAreaNum(entinfo.origin);
		if (areanum) { 
			bs->lead_teamgoal.entitynum = client;
			bs->lead_teamgoal.areanum = areanum;
			VectorCopy(entinfo.origin, bs->lead_teamgoal.origin);
			VectorSet(bs->lead_teamgoal.mins, -8, -8, -8);
			VectorSet(bs->lead_teamgoal.maxs, 8, 8, 8);
		}
	}

	if (bs->teamgoal.entitynum < 0) {
		if (other) BotAI_BotInitialChat(bs, "whereis", teammate, NULL);
		else BotAI_BotInitialChat(bs, "whereareyou", netname, NULL);
		gi.BotEnterChat(bs->cs, bs->client, CHAT_TEAM);
		return;
	}
	bs->lead_teammate = client;
	bs->lead_time = FloatTime() + TEAM_LEAD_TIME;
	bs->leadvisible_time = 0;
	bs->leadmessage_time = -(FloatTime() + 2 * random());
}

void BotMatch_Kill(bot_state_t *bs, bot_match_t *match) {
	char enemy[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	int client;

	if (!TeamPlayIsOn()) return;
	if (!BotAddressedToBot(bs, match)) return;

	gi.BotMatchVariable(match, ENEMY, enemy, sizeof(enemy));
	client = FindEnemyByName(bs, enemy);
	if (client < 0) {
		BotAI_BotInitialChat(bs, "whois", enemy, NULL);
		gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
		client = ClientFromName(netname);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
		return;
	}
	bs->teamgoal.entitynum = client;
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_KILL;
	bs->teamgoal_time = FloatTime() + TEAM_KILL_SOMEONE;
	BotSetTeamStatus(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotMatch_CTF(bot_state_t *bs, bot_match_t *match) {

	char flag[128], netname[MAX_NETNAME];

	if (gametype == GT_CTF) {
		gi.BotMatchVariable(match, FLAG, flag, sizeof(flag));
		if (match->subtype & ST_GOTFLAG) {
			if (!Q_stricmp(flag, "red")) {
				bs->redflagstatus = 1;
				if (BotTeam(bs) == TEAM_BLUE) {
					gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
					bs->flagcarrier = ClientFromName(netname);
				}
			}
			else {
				bs->blueflagstatus = 1;
				if (BotTeam(bs) == TEAM_RED) {
					gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
					bs->flagcarrier = ClientFromName(netname);
				}
			}
			bs->flagstatuschanged = 1;
			bs->lastflagcapture_time = FloatTime();
		}
		else if (match->subtype & ST_CAPTUREDFLAG) {
			bs->redflagstatus = 0;
			bs->blueflagstatus = 0;
			bs->flagcarrier = 0;
			bs->flagstatuschanged = 1;
		}
		else if (match->subtype & ST_RETURNEDFLAG) {
			if (!Q_stricmp(flag, "red")) bs->redflagstatus = 0;
			else bs->blueflagstatus = 0;
			bs->flagstatuschanged = 1;
		}
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (match->subtype & ST_1FCTFGOTFLAG) {
			gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
			bs->flagcarrier = ClientFromName(netname);
		}
	}
#endif
}

void BotMatch_EnterGame(bot_state_t *bs, bot_match_t *match) {
	int client;
	char netname[MAX_NETNAME];

	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	if (client >= 0) {
		notleader[client] = qfalse;
	}
}

void BotMatch_NewLeader(bot_state_t *bs, bot_match_t *match) {
	int client;
	char netname[MAX_NETNAME];

	gi.BotMatchVariable(match, NETNAME, netname, sizeof(netname));
	client = FindClientByName(netname);
	if (!BotSameTeam(bs, client))
		return;
	Q_strncpyz(bs->teamleader, netname, sizeof(bs->teamleader));
}

int BotMatchMessage(bot_state_t *bs, char *message) {
	bot_match_t match;

	match.type = 0;
	if (!gi.BotFindMatch(message, &match, MTCONTEXT_MISC
											|MTCONTEXT_INITIALTEAMCHAT
											|MTCONTEXT_CTF)) {
		return qfalse;
	}
	switch(match.type)
	{
		case MSG_HELP:					
		case MSG_ACCOMPANY:				
		{
			BotMatch_HelpAccompany(bs, &match);
			break;
		}
		case MSG_DEFENDKEYAREA:			
		{
			BotMatch_DefendKeyArea(bs, &match);
			break;
		}
		case MSG_CAMP:					
		{
			BotMatch_Camp(bs, &match);
			break;
		}
		case MSG_PATROL:				
		{
			BotMatch_Patrol(bs, &match);
			break;
		}
		case MSG_GETFLAG:				
		{
			BotMatch_GetFlag(bs, &match);
			break;
		}
#ifdef MISSIONPACK
		case MSG_ATTACKENEMYBASE:
		{
			BotMatch_AttackEnemyBase(bs, &match);
			break;
		}
		case MSG_HARVEST:
		{
			BotMatch_Harvest(bs, &match);
			break;
		}
#endif
		case MSG_RUSHBASE:				
		{
			BotMatch_RushBase(bs, &match);
			break;
		}
		case MSG_RETURNFLAG:
		{
			BotMatch_ReturnFlag(bs, &match);
			break;
		}
		case MSG_TASKPREFERENCE:
		{
			BotMatch_TaskPreference(bs, &match);
			break;
		}
		case MSG_CTF:
		{
			BotMatch_CTF(bs, &match);
			break;
		}
		case MSG_GETITEM:
		{
			BotMatch_GetItem(bs, &match);
			break;
		}
		case MSG_JOINSUBTEAM:			
		{
			BotMatch_JoinSubteam(bs, &match);
			break;
		}
		case MSG_LEAVESUBTEAM:			
		{
			BotMatch_LeaveSubteam(bs, &match);
			break;
		}
		case MSG_WHICHTEAM:
		{
			BotMatch_WhichTeam(bs, &match);
			break;
		}
		case MSG_CHECKPOINT:			
		{
			BotMatch_CheckPoint(bs, &match);
			break;
		}
		case MSG_CREATENEWFORMATION:	
		{
			gi.EA_SayTeam(bs->client, "the part of my brain to create formations has been damaged");
			break;
		}
		case MSG_FORMATIONPOSITION:		
		{
			gi.EA_SayTeam(bs->client, "the part of my brain to create formations has been damaged");
			break;
		}
		case MSG_FORMATIONSPACE:		
		{
			BotMatch_FormationSpace(bs, &match);
			break;
		}
		case MSG_DOFORMATION:			
		{
			break;
		}
		case MSG_DISMISS:				
		{
			BotMatch_Dismiss(bs, &match);
			break;
		}
		case MSG_STARTTEAMLEADERSHIP:	
		{
			BotMatch_StartTeamLeaderShip(bs, &match);
			break;
		}
		case MSG_STOPTEAMLEADERSHIP:	
		{
			BotMatch_StopTeamLeaderShip(bs, &match);
			break;
		}
		case MSG_WHOISTEAMLAEDER:
		{
			BotMatch_WhoIsTeamLeader(bs, &match);
			break;
		}
		case MSG_WHATAREYOUDOING:		
		{
			BotMatch_WhatAreYouDoing(bs, &match);
			break;
		}
		case MSG_WHATISMYCOMMAND:
		{
			BotMatch_WhatIsMyCommand(bs, &match);
			break;
		}
		case MSG_WHEREAREYOU:
		{
			BotMatch_WhereAreYou(bs, &match);
			break;
		}
		case MSG_LEADTHEWAY:
		{
			BotMatch_LeadTheWay(bs, &match);
			break;
		}
		case MSG_KILL:
		{
			BotMatch_Kill(bs, &match);
			break;
		}
		case MSG_ENTERGAME:				
		{
			BotMatch_EnterGame(bs, &match);
			break;
		}
		case MSG_NEWLEADER:
		{
			BotMatch_NewLeader(bs, &match);
			break;
		}
		case MSG_WAIT:
		{
			break;
		}
		case MSG_SUICIDE:
		{
			BotMatch_Suicide(bs, &match);
			break;
		}
		default:
		{
			BotAI_Print(PRT_MESSAGE, "unknown match type\n");
			break;
		}
	}
	return qtrue;
}
