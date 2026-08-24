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
#include "ai_vcmd.h"

#include "match.h"

#include "botmenudef.h"

typedef struct bot_ctftaskpreference_s
{
	char		name[36];
	int			preference;
} bot_ctftaskpreference_t;

bot_ctftaskpreference_t ctftaskpreferences[MAX_CLIENTS];


int BotValidTeamLeader(bot_state_t *bs) {
	if (!strlen(bs->teamleader)) return qfalse;
	if (ClientFromName(bs->teamleader) == -1) return qfalse;
	return qtrue;
}

int BotNumTeamMates(bot_state_t *bs) {
	int i, numplayers;
	char buf[MAX_INFO_STRING];
	static int maxclients;

	if (!maxclients)
		maxclients = gi.Cvar_VariableIntegerValue("sv_maxclients");

	numplayers = 0;
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		if (BotSameTeam(bs, i)) {
			numplayers++;
		}
	}
	return numplayers;
}

int BotClientTravelTimeToGoal(int client, bot_goal_t *goal) {
	playerState_t ps;
	int areanum;

	BotAI_GetClientState(client, &ps);
	areanum = BotPointAreaNum(ps.origin);
	if (!areanum) return 1;
	return gi.AAS_AreaTravelTimeToGoalArea(areanum, ps.origin, goal->areanum, TFL_DEFAULT);
}

int BotSortTeamMatesByBaseTravelTime(bot_state_t *bs, int *teammates, int maxteammates) {

	int i, j, k, numteammates, traveltime;
	char buf[MAX_INFO_STRING];
	static int maxclients;
	int traveltimes[MAX_CLIENTS];
	bot_goal_t *goal = NULL;

	if (gametype == GT_CTF) { 
		if (BotTeam(bs) == TEAM_RED)
			goal = &ctf_redflag;
		else
			goal = &ctf_blueflag;
	}
#ifdef MISSIONPACK
	else {
		if (BotTeam(bs) == TEAM_RED)
			goal = &redobelisk;
		else
			goal = &blueobelisk;
	}
#endif
	if (!maxclients)
		maxclients = gi.Cvar_VariableIntegerValue("sv_maxclients");

	numteammates = 0;
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		if (BotSameTeam(bs, i)) {
			traveltime = BotClientTravelTimeToGoal(i, goal);
			for (j = 0; j < numteammates; j++) {
				if (traveltime < traveltimes[j]) {
					for (k = numteammates; k > j; k--) {
						traveltimes[k] = traveltimes[k-1];
						teammates[k] = teammates[k-1];
					}
					break;
				}
			}
			traveltimes[j] = traveltime;
			teammates[j] = i;
			numteammates++;
			if (numteammates >= maxteammates) break;
		}
	}
	return numteammates;
}

void BotSetTeamMateTaskPreference(bot_state_t *bs, int teammate, int preference) {
	char teammatename[MAX_NETNAME];

	ctftaskpreferences[teammate].preference = preference;
	ClientName(teammate, teammatename, sizeof(teammatename));
	strcpy(ctftaskpreferences[teammate].name, teammatename);
}

int BotGetTeamMateTaskPreference(bot_state_t *bs, int teammate) {
	char teammatename[MAX_NETNAME];

	if (!ctftaskpreferences[teammate].preference) return 0;
	ClientName(teammate, teammatename, sizeof(teammatename));
	if (Q_stricmp(teammatename, ctftaskpreferences[teammate].name)) return 0;
	return ctftaskpreferences[teammate].preference;
}

int BotSortTeamMatesByTaskPreference(bot_state_t *bs, int *teammates, int numteammates) {
	int defenders[MAX_CLIENTS], numdefenders;
	int attackers[MAX_CLIENTS], numattackers;
	int roamers[MAX_CLIENTS], numroamers;
	int i, preference;

	numdefenders = numattackers = numroamers = 0;
	for (i = 0; i < numteammates; i++) {
		preference = BotGetTeamMateTaskPreference(bs, teammates[i]);
		if (preference & TEAMTP_DEFENDER) {
			defenders[numdefenders++] = teammates[i];
		}
		else if (preference & TEAMTP_ATTACKER) {
			attackers[numattackers++] = teammates[i];
		}
		else {
			roamers[numroamers++] = teammates[i];
		}
	}
	numteammates = 0;
	memcpy(&teammates[numteammates], defenders, numdefenders * sizeof(int));
	numteammates += numdefenders;
	memcpy(&teammates[numteammates], roamers, numroamers * sizeof(int));
	numteammates += numroamers;
	memcpy(&teammates[numteammates], attackers, numattackers * sizeof(int));
	numteammates += numattackers;

	return numteammates;
}

void BotSayTeamOrderAlways(bot_state_t *bs, int toclient) {
	char teamchat[MAX_MESSAGE_SIZE];
	char buf[MAX_MESSAGE_SIZE];
	char name[MAX_NETNAME];

	if (bs->client == toclient) {
		gi.BotGetChatMessage(bs->cs, buf, sizeof(buf));
		ClientName(bs->client, name, sizeof(name));
		Com_sprintf(teamchat, sizeof(teamchat), EC "(%s" EC ")" EC ": %s", name, buf);
		gi.BotQueueConsoleMessage(bs->cs, CMS_CHAT, teamchat);
	}
	else {
		gi.BotEnterChat(bs->cs, toclient, CHAT_TELL);
	}
}

void BotSayTeamOrder(bot_state_t *bs, int toclient) {
#ifdef MISSIONPACK
	char buf[MAX_MESSAGE_SIZE];

	gi.BotGetChatMessage(bs->cs, buf, sizeof(buf));
#else
	BotSayTeamOrderAlways(bs, toclient);
#endif
}

void BotVoiceChat(bot_state_t *bs, int toclient, char *voicechat) {
#ifdef MISSIONPACK
	if (toclient == -1)
		gi.EA_Command(bs->client, va("vsay_team %s", voicechat));
	else
		gi.EA_Command(bs->client, va("vtell %d %s", toclient, voicechat));
#endif
}

void BotVoiceChatOnly(bot_state_t *bs, int toclient, char *voicechat) {
#ifdef MISSIONPACK
	if (toclient == -1)
		gi.EA_Command(bs->client, va("vosay_team %s", voicechat));
	else
		gi.EA_Command(bs->client, va("votell %d %s", toclient, voicechat));
#endif
}

void BotSayVoiceTeamOrder(bot_state_t *bs, int toclient, char *voicechat) {
#ifdef MISSIONPACK
	BotVoiceChat(bs, toclient, voicechat);
#endif
}

void BotCTFOrders_BothFlagsNotAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i, other;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME], carriername[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	switch(bs->numteammates) {
		case 1: break;
		case 2:
		{
			if (teammates[0] != bs->flagcarrier) other = teammates[0];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
			break;
		}
		case 3:
		{
			if (teammates[0] != bs->flagcarrier) other = teammates[0];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			if ( bs->flagcarrier != -1 ) {
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				if (bs->flagcarrier == bs->client) {
					BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWME);
				}
				else {
					BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWFLAGCARRIER);
				}
			}
			else {
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
			}
			BotSayTeamOrder(bs, other);
			if (teammates[2] != bs->flagcarrier) other = teammates[2];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_RETURNFLAG);
			break;
		}
		default:
		{
			defenders = (int) ((float) numteammates * 0.4 + 0.5);
			if (defenders > 4) defenders = 4;
			attackers = (int) ((float) numteammates * 0.5 + 0.5);
			if (attackers > 5) attackers = 5;
			if (bs->flagcarrier != -1) {
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				for (i = 0; i < defenders; i++) {
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[i], name, sizeof(name));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_FOLLOWFLAGCARRIER);
					}
					BotSayTeamOrder(bs, teammates[i]);
				}
			}
			else {
				for (i = 0; i < defenders; i++) {
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_GETFLAG);
					BotSayTeamOrder(bs, teammates[i]);
				}
			}
			for (i = 0; i < attackers; i++) {
				if (teammates[numteammates - i - 1] == bs->flagcarrier) {
					continue;
				}
				ClientName(teammates[numteammates - i - 1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_RETURNFLAG);
			}
			break;
		}
	}
}

void BotCTFOrders_FlagNotAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(bs->numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.3 + 0.5);
				if (defenders > 3) defenders = 3;
				attackers = (int) ((float) numteammates * 0.7 + 0.5);
				if (attackers > 6) attackers = 6;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
	else {
		switch(bs->numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_GETFLAG);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.2 + 0.5);
				if (defenders > 2) defenders = 2;
				attackers = (int) ((float) numteammates * 0.7 + 0.5);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
}

void BotCTFOrders_EnemyFlagNotAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i, other;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME], carriername[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	switch(numteammates) {
		case 1: break;
		case 2:
		{
			if (teammates[0] == bs->flagcarrier) other = teammates[1];
			else other = teammates[0];

			if ( bs->flagcarrier == -1 ) {
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
				BotSayTeamOrder(bs, other);
			}
			else {
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
			}
			break;
		}
		case 3:
		{
			if (teammates[0] != bs->flagcarrier) other = teammates[0];
			else other = teammates[1];
			ClientName(other, name, sizeof(name));
			BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
			BotSayTeamOrder(bs, other);
			BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);

			if (teammates[2] != bs->flagcarrier) 
				other = teammates[2];
			else 
				other = teammates[1];

			if ( bs->flagcarrier == -1 ) {
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
				BotSayTeamOrder(bs, other);
			}
			else {
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
			}
			break;
		}
		default:
		{
			defenders = (int) ((float) numteammates * 0.6 + 0.5);
			if (defenders > 6) defenders = 6;
			attackers = (int) ((float) numteammates * 0.3 + 0.5);
			if (attackers > 3) attackers = 3;
			for (i = 0; i < defenders; i++) {
				if (teammates[i] == bs->flagcarrier) {
					continue;
				}
				ClientName(teammates[i], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[i]);
				BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
			}
			if ( bs->flagcarrier != -1 ) {
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				for (i = 0; i < attackers; i++) {
					if (teammates[numteammates - i - 1] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWFLAGCARRIER);
					}
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				}
			}
			else {
				for (i = 0; i < attackers; i++) {
					if (teammates[numteammates - i - 1] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				}
			}
			break;
		}
	}
}


void BotCTFOrders_BothFlagsAtBase(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.5 + 0.5);
				if (defenders > 5) defenders = 5;
				attackers = (int) ((float) numteammates * 0.4 + 0.5);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
	else {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.4 + 0.5);
				if (defenders > 4) defenders = 4;
				attackers = (int) ((float) numteammates * 0.5 + 0.5);
				if (attackers > 5) attackers = 5;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
}

void BotCTFOrders(bot_state_t *bs) {
	int flagstatus;

	if (BotTeam(bs) == TEAM_RED) flagstatus = bs->redflagstatus * 2 + bs->blueflagstatus;
	else flagstatus = bs->blueflagstatus * 2 + bs->redflagstatus;
	switch(flagstatus) {
		case 0: BotCTFOrders_BothFlagsAtBase(bs); break;
		case 1: BotCTFOrders_EnemyFlagNotAtBase(bs); break;
		case 2: BotCTFOrders_FlagNotAtBase(bs); break;
		case 3: BotCTFOrders_BothFlagsNotAtBase(bs); break;
	}
}


void BotCreateGroup(bot_state_t *bs, int *teammates, int groupsize) {
	char name[MAX_NETNAME], leadername[MAX_NETNAME];
	int i;

	ClientName(teammates[0], leadername, sizeof(leadername));
	for (i = 1; i < groupsize; i++)
	{
		ClientName(teammates[i], name, sizeof(name));
		if (teammates[0] == bs->client) {
			BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
		}
		else {
			BotAI_BotInitialChat(bs, "cmd_accompany", name, leadername, NULL);
		}
		BotSayTeamOrderAlways(bs, teammates[i]);
	}
}

void BotTeamOrders(bot_state_t *bs) {
	int teammates[MAX_CLIENTS];
	int numteammates, i;
	char buf[MAX_INFO_STRING];
	static int maxclients;

	if (!maxclients)
		maxclients = gi.Cvar_VariableIntegerValue("sv_maxclients");

	numteammates = 0;
	for (i = 0; i < maxclients && i < MAX_CLIENTS; i++) {
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_SPECTATOR) continue;
		if (BotSameTeam(bs, i)) {
			teammates[numteammates] = i;
			numteammates++;
		}
	}
	switch(numteammates) {
		case 1: break;
		case 2:
		{
			break;
		}
		case 3:
		{
			BotCreateGroup(bs, teammates, 2);
			break;
		}
		case 4:
		{
			BotCreateGroup(bs, teammates, 2);		
			BotCreateGroup(bs, &teammates[2], 2);	
			break;
		}
		case 5:
		{
			BotCreateGroup(bs, teammates, 2);		
			BotCreateGroup(bs, &teammates[2], 3);	
			break;
		}
		default:
		{
			if (numteammates <= 10) {
				for (i = 0; i < numteammates / 2; i++) {
					BotCreateGroup(bs, &teammates[i*2], 2);	
				}
			}
			break;
		}
	}
}

#ifdef MISSIONPACK

void Bot1FCTFOrders_FlagAtCenter(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.5 + 0.5);
				if (defenders > 5) defenders = 5;
				attackers = (int) ((float) numteammates * 0.4 + 0.5);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
	else { 
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.3 + 0.5);
				if (defenders > 3) defenders = 3;
				attackers = (int) ((float) numteammates * 0.6 + 0.5);
				if (attackers > 6) attackers = 6;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
}

void Bot1FCTFOrders_TeamHasFlag(bot_state_t *bs) {
	int numteammates, defenders, attackers, i, other;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME], carriername[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				if (teammates[0] == bs->flagcarrier) other = teammates[1];
				else other = teammates[0];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				if (teammates[0] != bs->flagcarrier) other = teammates[0];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
				if (teammates[2] != bs->flagcarrier) other = teammates[2];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				if ( bs->flagcarrier != -1 ) {
					ClientName(bs->flagcarrier, carriername, sizeof(carriername));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWFLAGCARRIER);
					}
				}
				else {
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_GETFLAG);
				}
				BotSayTeamOrder(bs, other);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.3 + 0.5);
				if (defenders > 3) defenders = 3;
				attackers = (int) ((float) numteammates * 0.7 + 0.5);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				if (bs->flagcarrier != -1) {
					ClientName(bs->flagcarrier, carriername, sizeof(carriername));
					for (i = 0; i < attackers; i++) {
						if (teammates[numteammates - i - 1] == bs->flagcarrier) {
							continue;
						}
						ClientName(teammates[numteammates - i - 1], name, sizeof(name));
						if (bs->flagcarrier == bs->client) {
							BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
							BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWME);
						}
						else {
							BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
							BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWFLAGCARRIER);
						}
						BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					}
				}
				else {
					for (i = 0; i < attackers; i++) {
						if (teammates[numteammates - i - 1] == bs->flagcarrier) {
							continue;
						}
						ClientName(teammates[numteammates - i - 1], name, sizeof(name));
						BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
						BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
					}
				}
				break;
			}
		}
	}
	else { 
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				if (teammates[0] == bs->flagcarrier) other = teammates[1];
				else other = teammates[0];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
				break;
			}
			case 3:
			{
				if (teammates[0] != bs->flagcarrier) other = teammates[0];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, other);
				BotSayVoiceTeamOrder(bs, other, VOICECHAT_DEFEND);
				if (teammates[2] != bs->flagcarrier) other = teammates[2];
				else other = teammates[1];
				ClientName(other, name, sizeof(name));
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				if (bs->flagcarrier == bs->client) {
					BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWME);
				}
				else {
					BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
					BotSayVoiceTeamOrder(bs, other, VOICECHAT_FOLLOWFLAGCARRIER);
				}
				BotSayTeamOrder(bs, other);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.2 + 0.5);
				if (defenders > 2) defenders = 2;
				attackers = (int) ((float) numteammates * 0.8 + 0.5);
				if (attackers > 8) attackers = 8;
				for (i = 0; i < defenders; i++) {
					if (teammates[i] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				ClientName(bs->flagcarrier, carriername, sizeof(carriername));
				for (i = 0; i < attackers; i++) {
					if (teammates[numteammates - i - 1] == bs->flagcarrier) {
						continue;
					}
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					if (bs->flagcarrier == bs->client) {
						BotAI_BotInitialChat(bs, "cmd_accompanyme", name, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWME);
					}
					else {
						BotAI_BotInitialChat(bs, "cmd_accompany", name, carriername, NULL);
						BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_FOLLOWFLAGCARRIER);
					}
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
				}
				break;
			}
		}
	}
}

void Bot1FCTFOrders_EnemyHasFlag(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_DEFEND);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.8 + 0.5);
				if (defenders > 8) defenders = 8;
				attackers = (int) ((float) numteammates * 0.1 + 0.5);
				if (attackers > 2) attackers = 2;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_returnflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
	else { 
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_returnflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.7 + 0.5);
				if (defenders > 8) defenders = 8;
				attackers = (int) ((float) numteammates * 0.2 + 0.5);
				if (attackers > 2) attackers = 2;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_returnflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
}

void Bot1FCTFOrders_EnemyDroppedFlag(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.5 + 0.5);
				if (defenders > 5) defenders = 5;
				attackers = (int) ((float) numteammates * 0.4 + 0.5);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_GETFLAG);
				}
				break;
			}
		}
	}
	else { 
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_GETFLAG);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_GETFLAG);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.3 + 0.5);
				if (defenders > 3) defenders = 3;
				attackers = (int) ((float) numteammates * 0.6 + 0.5);
				if (attackers > 6) attackers = 6;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_getflag", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_DEFEND);
				}
				break;
			}
		}
	}
}

void Bot1FCTFOrders(bot_state_t *bs) {
	switch(bs->neutralflagstatus) {
		case 0: Bot1FCTFOrders_FlagAtCenter(bs); break;
		case 1: Bot1FCTFOrders_TeamHasFlag(bs); break;
		case 2: Bot1FCTFOrders_EnemyHasFlag(bs); break;
		case 3: Bot1FCTFOrders_EnemyDroppedFlag(bs); break;
	}
}

void BotObeliskOrders(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_OFFENSE);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.5 + 0.5);
				if (defenders > 5) defenders = 5;
				attackers = (int) ((float) numteammates * 0.4 + 0.5);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_OFFENSE);
				}
				break;
			}
		}
	}
	else {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_OFFENSE);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.3 + 0.5);
				if (defenders > 3) defenders = 3;
				attackers = (int) ((float) numteammates * 0.7 + 0.5);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_attackenemybase", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_OFFENSE);
				}
				break;
			}
		}
	}
}

void BotHarvesterOrders(bot_state_t *bs) {
	int numteammates, defenders, attackers, i;
	int teammates[MAX_CLIENTS];
	char name[MAX_NETNAME];

	numteammates = BotSortTeamMatesByBaseTravelTime(bs, teammates, sizeof(teammates));
	BotSortTeamMatesByTaskPreference(bs, teammates, numteammates);
	if (!(bs->ctfstrategy & CTFS_AGRESSIVE)) {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_DEFEND);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_OFFENSE);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.5 + 0.5);
				if (defenders > 5) defenders = 5;
				attackers = (int) ((float) numteammates * 0.4 + 0.5);
				if (attackers > 4) attackers = 4;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_OFFENSE);
				}
				break;
			}
		}
	}
	else {
		switch(numteammates) {
			case 1: break;
			case 2:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				break;
			}
			case 3:
			{
				ClientName(teammates[0], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
				BotSayTeamOrder(bs, teammates[0]);
				BotSayVoiceTeamOrder(bs, teammates[0], VOICECHAT_DEFEND);
				ClientName(teammates[1], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
				BotSayTeamOrder(bs, teammates[1]);
				BotSayVoiceTeamOrder(bs, teammates[1], VOICECHAT_OFFENSE);
				ClientName(teammates[2], name, sizeof(name));
				BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
				BotSayTeamOrder(bs, teammates[2]);
				BotSayVoiceTeamOrder(bs, teammates[2], VOICECHAT_OFFENSE);
				break;
			}
			default:
			{
				defenders = (int) ((float) numteammates * 0.3 + 0.5);
				if (defenders > 3) defenders = 3;
				attackers = (int) ((float) numteammates * 0.7 + 0.5);
				if (attackers > 7) attackers = 7;
				for (i = 0; i < defenders; i++) {
					ClientName(teammates[i], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_defendbase", name, NULL);
					BotSayTeamOrder(bs, teammates[i]);
					BotSayVoiceTeamOrder(bs, teammates[i], VOICECHAT_DEFEND);
				}
				for (i = 0; i < attackers; i++) {
					ClientName(teammates[numteammates - i - 1], name, sizeof(name));
					BotAI_BotInitialChat(bs, "cmd_harvest", name, NULL);
					BotSayTeamOrder(bs, teammates[numteammates - i - 1]);
					BotSayVoiceTeamOrder(bs, teammates[numteammates - i - 1], VOICECHAT_OFFENSE);
				}
				break;
			}
		}
	}
}
#endif

int FindHumanTeamLeader(bot_state_t *bs) {
	return qfalse;
}

void BotTeamAI(bot_state_t *bs) {
	int numteammates;
	char netname[MAX_NETNAME];

	if ( gametype < GT_TEAM  )
		return;
	if (!BotValidTeamLeader(bs)) {
		if (!FindHumanTeamLeader(bs)) {
			if (!bs->askteamleader_time && !bs->becometeamleader_time) {
				if (bs->entergame_time + 10 > FloatTime()) {
					bs->askteamleader_time = FloatTime() + 5 + random() * 10;
				}
				else {
					bs->becometeamleader_time = FloatTime() + 5 + random() * 10;
				}
			}
			if (bs->askteamleader_time && bs->askteamleader_time < FloatTime()) {
				BotAI_BotInitialChat(bs, "whoisteamleader", NULL);
				gi.BotEnterChat(bs->cs, 0, CHAT_TEAM);
				bs->askteamleader_time = 0;
				bs->becometeamleader_time = FloatTime() + 8 + random() * 10;
			}
			if (bs->becometeamleader_time && bs->becometeamleader_time < FloatTime()) {
				BotAI_BotInitialChat(bs, "iamteamleader", NULL);
				gi.BotEnterChat(bs->cs, 0, CHAT_TEAM);
				BotSayVoiceTeamOrder(bs, -1, VOICECHAT_STARTLEADER);
				ClientName(bs->client, netname, sizeof(netname));
				Q_strncpyz(bs->teamleader, netname, sizeof(bs->teamleader));
				bs->teamleader[sizeof(bs->teamleader) - 1] = '\0';
				bs->becometeamleader_time = 0;
			}
			return;
		}
	}
	bs->askteamleader_time = 0;
	bs->becometeamleader_time = 0;

	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) != 0) return;
	numteammates = BotNumTeamMates(bs);
	switch(gametype) {
		case GT_TEAM:
		{
			if (bs->numteammates != numteammates || bs->forceorders) {
				bs->teamgiveorders_time = FloatTime();
				bs->numteammates = numteammates;
				bs->forceorders = qfalse;
			}
			if (bs->teamgiveorders_time && bs->teamgiveorders_time < FloatTime() - 5) {
				BotTeamOrders(bs);
				bs->teamgiveorders_time = FloatTime() + 120;
			}
			break;
		}
		case GT_CTF:
		{
			if (bs->numteammates != numteammates || bs->flagstatuschanged || bs->forceorders) {
				bs->teamgiveorders_time = FloatTime();
				bs->numteammates = numteammates;
				bs->flagstatuschanged = qfalse;
				bs->forceorders = qfalse;
			}
			if (bs->lastflagcapture_time < FloatTime() - 240) {
				bs->lastflagcapture_time = FloatTime();
				if (random() < 0.4) {
					bs->ctfstrategy ^= CTFS_AGRESSIVE;
					bs->teamgiveorders_time = FloatTime();
				}
			}
			if (bs->teamgiveorders_time && bs->teamgiveorders_time < FloatTime() - 3) {
				BotCTFOrders(bs);
				bs->teamgiveorders_time = 0;
			}
			break;
		}
#ifdef MISSIONPACK
		case GT_1FCTF:
		{
			if (bs->numteammates != numteammates || bs->flagstatuschanged || bs->forceorders) {
				bs->teamgiveorders_time = FloatTime();
				bs->numteammates = numteammates;
				bs->flagstatuschanged = qfalse;
				bs->forceorders = qfalse;
			}
			if (bs->lastflagcapture_time < FloatTime() - 240) {
				bs->lastflagcapture_time = FloatTime();
				if (random() < 0.4) {
					bs->ctfstrategy ^= CTFS_AGRESSIVE;
					bs->teamgiveorders_time = FloatTime();
				}
			}
			if (bs->teamgiveorders_time && bs->teamgiveorders_time < FloatTime() - 2) {
				Bot1FCTFOrders(bs);
				bs->teamgiveorders_time = 0;
			}
			break;
		}
		case GT_OBELISK:
		{
			if (bs->numteammates != numteammates || bs->forceorders) {
				bs->teamgiveorders_time = FloatTime();
				bs->numteammates = numteammates;
				bs->forceorders = qfalse;
			}
			if (bs->teamgiveorders_time && bs->teamgiveorders_time < FloatTime() - 5) {
				BotObeliskOrders(bs);
				bs->teamgiveorders_time = FloatTime() + 30;
			}
			break;
		}
		case GT_HARVESTER:
		{
			if (bs->numteammates != numteammates || bs->forceorders) {
				bs->teamgiveorders_time = FloatTime();
				bs->numteammates = numteammates;
				bs->forceorders = qfalse;
			}
			if (bs->teamgiveorders_time && bs->teamgiveorders_time < FloatTime() - 5) {
				BotHarvesterOrders(bs);
				bs->teamgiveorders_time = FloatTime() + 30;
			}
			break;
		}
#endif
	}
}

