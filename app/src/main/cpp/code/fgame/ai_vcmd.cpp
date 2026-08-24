

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
#include "chars.h"				
#include "inv.h"				
#include "syn.h"				
#include "match.h"				

#include "botmenudef.h"


typedef struct voiceCommand_s
{
	char *cmd;
	void (*func)(bot_state_t *bs, int client, int mode);
} voiceCommand_t;

void BotVoiceChat_GetFlag(bot_state_t *bs, int client, int mode) {
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

void BotVoiceChat_Offense(bot_state_t *bs, int client, int mode) {
	if ( gametype == GT_CTF
#ifdef MISSIONPACK
		|| gametype == GT_1FCTF
#endif
		) {
		BotVoiceChat_GetFlag(bs, client, mode);
		return;
	}
#ifdef MISSIONPACK
	if (gametype == GT_HARVESTER) {
		bs->decisionmaker = client;
		bs->ordered = qtrue;
		bs->order_time = FloatTime();
		bs->teammessage_time = FloatTime() + 2 * random();
		bs->ltgtype = LTG_HARVEST;
		bs->teamgoal_time = FloatTime() + TEAM_HARVEST_TIME;
		bs->harvestaway_time = 0;
		BotSetTeamStatus(bs);
		BotRememberLastOrderedTask(bs);
	}
	else
#endif
	{
		bs->decisionmaker = client;
		bs->ordered = qtrue;
		bs->order_time = FloatTime();
		bs->teammessage_time = FloatTime() + 2 * random();
		bs->ltgtype = LTG_ATTACKENEMYBASE;
		bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
		bs->attackaway_time = 0;
		BotSetTeamStatus(bs);
		BotRememberLastOrderedTask(bs);
	}
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotVoiceChat_Defend(bot_state_t *bs, int client, int mode) {
#ifdef MISSIONPACK
	if ( gametype == GT_OBELISK || gametype == GT_HARVESTER) {
		switch(BotTeam(bs)) {
			case TEAM_RED: memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t)); break;
			case TEAM_BLUE: memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t)); break;
			default: return;
		}
	}
	else
#endif
		if (gametype == GT_CTF
#ifdef MISSIONPACK
			|| gametype == GT_1FCTF
#endif
			) {
		switch(BotTeam(bs)) {
			case TEAM_RED: memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t)); break;
			case TEAM_BLUE: memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t)); break;
			default: return;
		}
	}
	else {
		return;
	}
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_DEFENDKEYAREA;
	bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
	bs->defendaway_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotVoiceChat_DefendFlag(bot_state_t *bs, int client, int mode) {
	BotVoiceChat_Defend(bs, client, mode);
}

void BotVoiceChat_Patrol(bot_state_t *bs, int client, int mode) {
	bs->decisionmaker = client;
	bs->ltgtype = 0;
	bs->lead_time = 0;
	bs->lastgoal_ltgtype = 0;
	BotAI_BotInitialChat(bs, "dismissed", NULL);
	gi.BotEnterChat(bs->cs, client, CHAT_TELL);
	BotVoiceChatOnly(bs, -1, VOICECHAT_ONPATROL);
	BotSetTeamStatus(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotVoiceChat_Camp(bot_state_t *bs, int client, int mode) {
	int areanum;
	aas_entityinfo_t entinfo;
	char netname[MAX_NETNAME];

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
		BotAI_BotInitialChat(bs, "whereareyou", EasyClientName(client, netname, sizeof(netname)), NULL);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
		return;
	}
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->ltgtype = LTG_CAMPORDER;
	bs->teamgoal_time = FloatTime() + TEAM_CAMP_TIME;
	bs->teammate = client;
	bs->arrive_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotVoiceChat_FollowMe(bot_state_t *bs, int client, int mode) {
	int areanum;
	aas_entityinfo_t entinfo;
	char netname[MAX_NETNAME];

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
		BotAI_BotInitialChat(bs, "whereareyou", EasyClientName(client, netname, sizeof(netname)), NULL);
		gi.BotEnterChat(bs->cs, client, CHAT_TELL);
		return;
	}
	bs->decisionmaker = client;
	bs->ordered = qtrue;
	bs->order_time = FloatTime();
	bs->teammate = client;
	bs->teammatevisible_time = FloatTime();
	bs->teammessage_time = FloatTime() + 2 * random();
	bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
	bs->ltgtype = LTG_TEAMACCOMPANY;
	bs->formation_dist = 3.5 * 32;		
	bs->arrive_time = 0;
	BotSetTeamStatus(bs);
	BotRememberLastOrderedTask(bs);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotVoiceChat_FollowFlagCarrier(bot_state_t *bs, int client, int mode) {
	int carrier;

	carrier = BotTeamFlagCarrier(bs);
	if (carrier >= 0)
		BotVoiceChat_FollowMe(bs, carrier, mode);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotVoiceChat_ReturnFlag(bot_state_t *bs, int client, int mode) {
	if (
		gametype != GT_CTF
#ifdef MISSIONPACK
		&& gametype != GT_1FCTF
#endif
		) {
		return;
	}
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

void BotVoiceChat_StartLeader(bot_state_t *bs, int client, int mode) {
	ClientName(client, bs->teamleader, sizeof(bs->teamleader));
}

void BotVoiceChat_StopLeader(bot_state_t *bs, int client, int mode) {
	char netname[MAX_MESSAGE_SIZE];

	if (!Q_stricmp(bs->teamleader, ClientName(client, netname, sizeof(netname)))) {
		bs->teamleader[0] = '\0';
		notleader[client] = qtrue;
	}
}

void BotVoiceChat_WhoIsLeader(bot_state_t *bs, int client, int mode) {
	char netname[MAX_MESSAGE_SIZE];

	if (!TeamPlayIsOn()) return;

	ClientName(bs->client, netname, sizeof(netname));
	if (!Q_stricmp(netname, bs->teamleader)) {
		BotAI_BotInitialChat(bs, "iamteamleader", NULL);
		gi.BotEnterChat(bs->cs, 0, CHAT_TEAM);
		BotVoiceChatOnly(bs, -1, VOICECHAT_STARTLEADER);
	}
}

void BotVoiceChat_WantOnDefense(bot_state_t *bs, int client, int mode) {
	char netname[MAX_NETNAME];
	int preference;

	preference = BotGetTeamMateTaskPreference(bs, client);
	preference &= ~TEAMTP_ATTACKER;
	preference |= TEAMTP_DEFENDER;
	BotSetTeamMateTaskPreference(bs, client, preference);
	EasyClientName(client, netname, sizeof(netname));
	BotAI_BotInitialChat(bs, "keepinmind", netname, NULL);
	gi.BotEnterChat(bs->cs, client, CHAT_TELL);
	BotVoiceChatOnly(bs, client, VOICECHAT_YES);
	gi.EA_Action(bs->client, ACTION_AFFIRMATIVE);
}

void BotVoiceChat_WantOnOffense(bot_state_t *bs, int client, int mode) {
	char netname[MAX_NETNAME];
	int preference;

	preference = BotGetTeamMateTaskPreference(bs, client);
	preference &= ~TEAMTP_DEFENDER;
	preference |= TEAMTP_ATTACKER;
	BotSetTeamMateTaskPreference(bs, client, preference);
	EasyClientName(client, netname, sizeof(netname));
	BotAI_BotInitialChat(bs, "keepinmind", netname, NULL);
	gi.BotEnterChat(bs->cs, client, CHAT_TELL);
	BotVoiceChatOnly(bs, client, VOICECHAT_YES);
	gi.EA_Action(bs->client, ACTION_AFFIRMATIVE);
}

void BotVoiceChat_Dummy(bot_state_t *bs, int client, int mode) {
}

voiceCommand_t voiceCommands[] = {
	{VOICECHAT_GETFLAG, BotVoiceChat_GetFlag},
	{VOICECHAT_OFFENSE, BotVoiceChat_Offense },
	{VOICECHAT_DEFEND, BotVoiceChat_Defend },
	{VOICECHAT_DEFENDFLAG, BotVoiceChat_DefendFlag },
	{VOICECHAT_PATROL, BotVoiceChat_Patrol },
	{VOICECHAT_CAMP, BotVoiceChat_Camp },
	{VOICECHAT_FOLLOWME, BotVoiceChat_FollowMe },
	{VOICECHAT_FOLLOWFLAGCARRIER, BotVoiceChat_FollowFlagCarrier },
	{VOICECHAT_RETURNFLAG, BotVoiceChat_ReturnFlag },
	{VOICECHAT_STARTLEADER, BotVoiceChat_StartLeader },
	{VOICECHAT_STOPLEADER, BotVoiceChat_StopLeader },
	{VOICECHAT_WHOISLEADER, BotVoiceChat_WhoIsLeader },
	{VOICECHAT_WANTONDEFENSE, BotVoiceChat_WantOnDefense },
	{VOICECHAT_WANTONOFFENSE, BotVoiceChat_WantOnOffense },
	{NULL, BotVoiceChat_Dummy}
};

int BotVoiceChatCommand(bot_state_t *bs, int mode, char *voiceChat) {
	int i, clientNum;
	char *ptr, buf[MAX_MESSAGE_SIZE], *cmd,*cmd2;

	if (!TeamPlayIsOn()) {
		return qfalse;
	}

	if ( mode == SAY_ALL ) {
		return qfalse;
	}

	Q_strncpyz(buf, voiceChat, sizeof(buf));

	cmd = buf;
	ptr = strchr(buf,' ');
	*ptr++ = 0;
	clientNum = atoi(ptr);
	cmd2 = ptr+2;
	ptr = strchr(cmd2,' ');
	*ptr = 0;

	if (!BotSameTeam(bs, clientNum)) {
		return qfalse;
	}

	for (i = 0; voiceCommands[i].cmd; i++) {
		if (!Q_stricmp(cmd2, voiceCommands[i].cmd)) {
			voiceCommands[i].func(bs, clientNum, mode);
			return qtrue;
		}
	}
	return qfalse;
}
