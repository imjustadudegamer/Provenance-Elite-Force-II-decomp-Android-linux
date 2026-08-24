


#include "player.h"

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

#include "mp_manager.hpp"

#define AREACONTENTS_MOVER				1024
#define AREACONTENTS_MODELNUMSHIFT		24
#define AREACONTENTS_MAXMODELNUM		0xFF
#define AREACONTENTS_MODELNUM			(AREACONTENTS_MAXMODELNUM << AREACONTENTS_MODELNUMSHIFT)

#define IDEAL_ATTACKDIST			140 

#define MAX_WAYPOINTS		128
bot_waypoint_t botai_waypoints[MAX_WAYPOINTS];
bot_waypoint_t *botai_freewaypoints;

//NOTE: not using a cvars which can be updated because the game should be reloaded anyway
int gametype;		

vmCvar_t bot_grapple;
vmCvar_t bot_rocketjump;
vmCvar_t bot_fastchat;
vmCvar_t bot_nochat;
vmCvar_t bot_testrchat;
vmCvar_t bot_challenge;
vmCvar_t bot_predictobstacles;
vmCvar_t g_spSkill;

extern vmCvar_t bot_developer;

vec3_t lastteleport_origin;		
float lastteleport_time;		
int max_bspmodelindex;			

bot_goal_t ctf_redflag;
bot_goal_t ctf_blueflag;
#ifdef MISSIONPACK
bot_goal_t ctf_neutralflag;
bot_goal_t redobelisk;
bot_goal_t blueobelisk;
bot_goal_t neutralobelisk;
#endif

#define MAX_ALTROUTEGOALS		32

int altroutegoals_setup;
aas_altroutegoal_t red_altroutegoals[MAX_ALTROUTEGOALS];
int red_numaltroutegoals;
aas_altroutegoal_t blue_altroutegoals[MAX_ALTROUTEGOALS];
int blue_numaltroutegoals;

void BotSetEntityNumForGoal(bot_goal_t *goal, char *classname);


void BotSetUserInfo(bot_state_t *bs, char *key, char *value) {
	char userinfo[MAX_INFO_STRING];

	gi.getUserinfo(bs->client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, key, value);
	gi.setUserinfo(bs->client, userinfo);
	G_ClientUserinfoChanged( &g_entities[bs->client] , userinfo );
}

int BotCTFCarryingFlag(bot_state_t *bs) {
	if (gametype != GT_CTF) return CTF_FLAG_NONE;

	if (bs->inventory[INVENTORY_REDFLAG] > 0) return CTF_FLAG_RED;
	else if (bs->inventory[INVENTORY_BLUEFLAG] > 0) return CTF_FLAG_BLUE;
	return CTF_FLAG_NONE;
}

int BotTeam(bot_state_t *bs) {

	if (bs->client < 0 || bs->client >= maxclients->integer) {
		return qfalse;
	}
	gentity_t *bot = &g_entities[bs->entitynum];
	if (multiplayerManager.getPlayersTeam((Player *)bot->entity )->getName() == "Blue")
		return TEAM_BLUE;
	if (multiplayerManager.getPlayersTeam((Player *)bot->entity )->getName() == "Red")
		return TEAM_RED;
	return TEAM_FREE;
}

int BotOppositeTeam(bot_state_t *bs) {
	switch(BotTeam(bs)) {
		case TEAM_RED: return TEAM_BLUE;
		case TEAM_BLUE: return TEAM_RED;
		default: return TEAM_FREE;
	}
}

bot_goal_t *BotEnemyFlag(bot_state_t *bs) {
	if (BotTeam(bs) == TEAM_RED) {
		return &ctf_blueflag;
	}
	else {
		return &ctf_redflag;
	}
}

bot_goal_t *BotTeamFlag(bot_state_t *bs) {
	if (BotTeam(bs) == TEAM_RED) {
		return &ctf_redflag;
	}
	else {
		return &ctf_blueflag;
	}
}


qboolean EntityIsDead(aas_entityinfo_t *entinfo) {
	playerState_t ps;

	if (entinfo->number >= 0 && entinfo->number < maxclients->integer) {
		if (BotAI_GetClientState( entinfo->number, &ps )) {
			if (ps.stats[ STAT_HEALTH ] <= 0)
				return qtrue;
			if (ps.pm_type != PM_NORMAL) return qtrue;
		}
	}
	return qfalse;
}

qboolean EntityCarriesFlag(aas_entityinfo_t *entinfo) {

	Player *player = (Player *)g_entities[entinfo->number].entity;

	if ( multiplayerManager.doesPlayerHaveItem( player, "ctfflag-red" ) || 
		multiplayerManager.doesPlayerHaveItem( player, "ctfflag-blue" ) ||
		multiplayerManager.doesPlayerHaveItem( player, "ctfflag-one" ) )
		return qtrue;

  return qfalse;
}

qboolean EntityIsInvisible(aas_entityinfo_t *entinfo) {
		
	if (EntityCarriesFlag(entinfo))
		return qfalse;

	if (g_entities[entinfo->number].entity && g_entities[entinfo->number].entity->_affectingViewModes & gi.GetViewModeMask( "forcevisible" ) )
		return qtrue;
	return qfalse;
}

qboolean EntityIsShooting(aas_entityinfo_t *entinfo) {
	Player *plyr = (Player *)g_entities[entinfo->number].entity;	

	if (plyr)
		if (plyr->GetLastUcmd().buttons & (BUTTON_ATTACKLEFT | BUTTON_ATTACKRIGHT))
			return qtrue;

	return qfalse;
}

qboolean EntityIsChatting(aas_entityinfo_t *entinfo) {

	return qfalse;
}

qboolean EntityHasQuad(aas_entityinfo_t *entinfo) {
	return qfalse;
}

#ifdef MISSIONPACK
qboolean EntityHasKamikaze(aas_entityinfo_t *entinfo) {
	return qfalse;
}

qboolean EntityCarriesCubes(aas_entityinfo_t *entinfo) {
	entityState_t state;

	if (gametype != GT_HARVESTER)
		return qfalse;
	BotAI_GetEntityState(entinfo->number, &state);
	return qfalse;
}

int Bot1FCTFCarryingFlag(bot_state_t *bs) {
	if (gametype != GT_1FCTF) return qfalse;

	if (bs->inventory[INVENTORY_NEUTRALFLAG] > 0) return qtrue;
	return qfalse;
}

int BotHarvesterCarryingCubes(bot_state_t *bs) {
	if (gametype != GT_HARVESTER) return qfalse;

	if (bs->inventory[INVENTORY_REDCUBE] > 0) return qtrue;
	if (bs->inventory[INVENTORY_BLUECUBE] > 0) return qtrue;
	return qfalse;
}
#endif

void BotRememberLastOrderedTask(bot_state_t *bs) {
	if (!bs->ordered) {
		return;
	}
	bs->lastgoal_decisionmaker = bs->decisionmaker;
	bs->lastgoal_ltgtype = bs->ltgtype;
	memcpy(&bs->lastgoal_teamgoal, &bs->teamgoal, sizeof(bot_goal_t));
	bs->lastgoal_teammate = bs->teammate;
}

void BotSetTeamStatus(bot_state_t *bs) {
#ifdef MISSIONPACK
	int teamtask;
	aas_entityinfo_t entinfo;

	teamtask = TEAMTASK_PATROL;

	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
			break;
		case LTG_TEAMACCOMPANY:
			BotEntityInfo(bs->teammate, &entinfo);
			if ( ( (gametype == GT_CTF || gametype == GT_1FCTF) && EntityCarriesFlag(&entinfo))
				|| ( gametype == GT_HARVESTER && EntityCarriesCubes(&entinfo)) ) {
				teamtask = TEAMTASK_ESCORT;
			}
			else {
				teamtask = TEAMTASK_FOLLOW;
			}
			break;
		case LTG_DEFENDKEYAREA:
			teamtask = TEAMTASK_DEFENSE;
			break;
		case LTG_GETFLAG:
			teamtask = TEAMTASK_OFFENSE;
			break;
		case LTG_RUSHBASE:
			teamtask = TEAMTASK_DEFENSE;
			break;
		case LTG_RETURNFLAG:
			teamtask = TEAMTASK_RETRIEVE;
			break;
		case LTG_CAMP:
		case LTG_CAMPORDER:
			teamtask = TEAMTASK_CAMP;
			break;
		case LTG_PATROL:
			teamtask = TEAMTASK_PATROL;
			break;
		case LTG_GETITEM:
			teamtask = TEAMTASK_PATROL;
			break;
		case LTG_KILL:
			teamtask = TEAMTASK_PATROL;
			break;
		case LTG_HARVEST:
			teamtask = TEAMTASK_OFFENSE;
			break;
		case LTG_ATTACKENEMYBASE:
			teamtask = TEAMTASK_OFFENSE;
			break;
		default:
			teamtask = TEAMTASK_PATROL;
			break;
	}
	BotSetUserInfo(bs, "teamtask", (char *)va("%d", teamtask));
#endif
}

int BotSetLastOrderedTask(bot_state_t *bs) {

	if (gametype == GT_CTF) {
		if ( bs->lastgoal_ltgtype == LTG_RETURNFLAG ) {
			if ( BotTeam(bs) == TEAM_RED ) {
				if ( bs->redflagstatus == 0 ) {
					bs->lastgoal_ltgtype = 0;
				}
			}
			else {
				if ( bs->blueflagstatus == 0 ) {
					bs->lastgoal_ltgtype = 0;
				}
			}
		}
	}

	if ( bs->lastgoal_ltgtype ) {
		bs->decisionmaker = bs->lastgoal_decisionmaker;
		bs->ordered = qtrue;
		bs->ltgtype = bs->lastgoal_ltgtype;
		memcpy(&bs->teamgoal, &bs->lastgoal_teamgoal, sizeof(bot_goal_t));
		bs->teammate = bs->lastgoal_teammate;
		bs->teamgoal_time = FloatTime() + 300;
		BotSetTeamStatus(bs);
		if ( gametype == GT_CTF ) {
			if ( bs->ltgtype == LTG_GETFLAG ) {
				bot_goal_t *tb, *eb;
				int tt, et;

				tb = BotTeamFlag(bs);
				eb = BotEnemyFlag(bs);
				tt = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, tb->areanum, TFL_DEFAULT);
				et = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, eb->areanum, TFL_DEFAULT);
				if (et > tt) {
					BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
				}
			}
		}
		return qtrue;
	}
	return qfalse;
}

void BotRefuseOrder(bot_state_t *bs) {
	if (!bs->ordered)
		return;
	if ( bs->order_time && bs->order_time > FloatTime() - 10 ) {
		gi.EA_Action(bs->client, ACTION_NEGATIVE);
		BotVoiceChat(bs, bs->decisionmaker, VOICECHAT_NO);
		bs->order_time = 0;
	}
}

void BotCTFSeekGoals(bot_state_t *bs) {
	float rnd, l1, l2;
	int flagstatus, c;
	vec3_t dir;
	aas_entityinfo_t entinfo;

	if (BotCTFCarryingFlag(bs)) {
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			switch(BotTeam(bs)) {
				case TEAM_RED: VectorSubtract(bs->origin, ctf_blueflag.origin, dir); break;
				case TEAM_BLUE: VectorSubtract(bs->origin, ctf_redflag.origin, dir); break;
				default: VectorSet(dir, 999, 999, 999); break;
			}
			if ( VectorLength(dir) < 128 ) {
				BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			} else {
				bs->altroutegoal.areanum = 0;
			}
			BotSetUserInfo(bs, "teamtask", (char *)va("%d", TEAMTASK_OFFENSE));
			BotVoiceChat(bs, -1, VOICECHAT_IHAVEFLAG);
		}
		else if (bs->rushbaseaway_time > FloatTime()) {
			if (BotTeam(bs) == TEAM_RED) flagstatus = bs->redflagstatus;
			else flagstatus = bs->blueflagstatus;
			if (flagstatus == 0) {
				bs->rushbaseaway_time = 0;
			}
		}
		return;
	}
	if ( bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered ) {
		BotEntityInfo(bs->teammate, &entinfo);
		if (!EntityCarriesFlag(&entinfo)) {
			bs->ltgtype = 0;
		}
	}
	if (BotTeam(bs) == TEAM_RED) flagstatus = bs->redflagstatus * 2 + bs->blueflagstatus;
	else flagstatus = bs->blueflagstatus * 2 + bs->redflagstatus;
	if (flagstatus == 1) {
		if (bs->owndecision_time < FloatTime()) {
			if (!(bs->ltgtype == LTG_DEFENDKEYAREA &&
					(bs->teamgoal.number == ctf_redflag.number ||
					bs->teamgoal.number == ctf_blueflag.number))) {
				c = BotTeamFlagCarrierVisible(bs);
				if (c >= 0 &&
						(bs->ltgtype != LTG_TEAMACCOMPANY || bs->teammate != c)) {
					BotRefuseOrder(bs);
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					bs->teammate = c;
					bs->teammatevisible_time = FloatTime();
					bs->teammessage_time = 0;
					bs->arrive_time = 1;
					BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
					bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
					bs->ltgtype = LTG_TEAMACCOMPANY;
					bs->formation_dist = 3.5 * 32;		
					BotSetTeamStatus(bs);
					bs->owndecision_time = (int) FloatTime() + 5;
				}
			}
		}
		return;
	}
	else if (flagstatus == 2) {
		if (bs->owndecision_time < FloatTime()) {
			c = BotEnemyFlagCarrierVisible(bs);
			if (c >= 0) {
			}
			if (bs->ltgtype != LTG_GETFLAG &&
				bs->ltgtype != LTG_RETURNFLAG &&
				bs->ltgtype != LTG_TEAMHELP &&
				bs->ltgtype != LTG_TEAMACCOMPANY &&
				bs->ltgtype != LTG_CAMPORDER &&
				bs->ltgtype != LTG_PATROL &&
				bs->ltgtype != LTG_GETITEM) {

				BotRefuseOrder(bs);
				bs->decisionmaker = bs->client;
				bs->ordered = qfalse;
				if (random() < 0.5) {
					bs->ltgtype = LTG_GETFLAG;
				}
				else {
					bs->ltgtype = LTG_RETURNFLAG;
				}
				bs->teammessage_time = 0;
				bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
				BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
				BotSetTeamStatus(bs);
				bs->owndecision_time = (int) (FloatTime() + 5.0f);
			}
		}
		return;
	}
	else if (flagstatus == 3) {
		if (bs->owndecision_time < FloatTime()) {
			if ( bs->ltgtype != LTG_RETURNFLAG && bs->ltgtype != LTG_TEAMACCOMPANY ) {
				c = BotTeamFlagCarrierVisible(bs);
				if (c >= 0) {
					BotRefuseOrder(bs);
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					bs->teammate = c;
					bs->teammatevisible_time = FloatTime();
					bs->teammessage_time = 0;
					bs->arrive_time = 1;
					BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
					bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
					bs->ltgtype = LTG_TEAMACCOMPANY;
					bs->formation_dist = 3.5 * 32;		
					BotSetTeamStatus(bs);
					bs->owndecision_time = (int)(FloatTime() + 5.0f);
				}
				else {
					BotRefuseOrder(bs);
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					bs->teammessage_time = FloatTime() + 2 * random();
					bs->ltgtype = LTG_RETURNFLAG;
					bs->teamgoal_time = FloatTime() + CTF_RETURNFLAG_TIME;
					BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
					BotSetTeamStatus(bs);
					bs->owndecision_time = (int)( FloatTime() + 5.0f);
				}
			}
		}
		return;
	}
	if (BotTeamLeader(bs)) {
		return;
	}
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	if ( !bs->ordered && bs->lastgoal_ltgtype ) {
		bs->ltgtype = 0;
	}
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_RETURNFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	if (BotSetLastOrderedTask(bs))
		return;
	if (bs->owndecision_time > FloatTime())
		return;;
	if (bs->ctfroam_time > FloatTime())
		return;
	if (BotAggression(bs) < 50)
		return;
	bs->teammessage_time = FloatTime() + 2 * random();
	if (bs->teamtaskpreference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	rnd = random();
	if (rnd < l1 && ctf_redflag.areanum && ctf_blueflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		bs->ltgtype = LTG_GETFLAG;
		bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
		BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
		BotSetTeamStatus(bs);
	}
	else if (rnd < l2 && ctf_redflag.areanum && ctf_blueflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
		bs->ltgtype = LTG_DEFENDKEYAREA;
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
	bs->owndecision_time = (int)(FloatTime() + 5.0f);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void BotCTFRetreatGoals(bot_state_t *bs) {
	if (BotCTFCarryingFlag(bs)) {
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotSetTeamStatus(bs);
		}
	}
}

#ifdef MISSIONPACK
void Bot1FCTFSeekGoals(bot_state_t *bs) {
	aas_entityinfo_t entinfo;
	float rnd, l1, l2;
	int c;

	if (Bot1FCTFCarryingFlag(bs)) {
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			BotSetTeamStatus(bs);
			BotVoiceChat(bs, -1, VOICECHAT_IHAVEFLAG);
		}
		return;
	}
	if ( bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered ) {
		BotEntityInfo(bs->teammate, &entinfo);
		if (!EntityCarriesFlag(&entinfo)) {
			bs->ltgtype = 0;
		}
	}
	if (bs->neutralflagstatus == 1) {
		if (bs->owndecision_time < FloatTime()) {
			if (bs->ltgtype != LTG_TEAMACCOMPANY) {
				c = BotTeamFlagCarrierVisible(bs);
				if (c >= 0) {
					BotRefuseOrder(bs);
					bs->decisionmaker = bs->client;
					bs->ordered = qfalse;
					bs->teammate = c;
					bs->teammatevisible_time = FloatTime();
					bs->teammessage_time = 0;
					bs->arrive_time = 1;
					BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
					bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
					bs->ltgtype = LTG_TEAMACCOMPANY;
					bs->formation_dist = 3.5 * 32;		
					BotSetTeamStatus(bs);
					bs->owndecision_time = (int)(FloatTime() + 5.0f);
					return;
				}
			}
			if (bs->ltgtype == LTG_TEAMHELP ||
					bs->ltgtype == LTG_TEAMACCOMPANY ||
					bs->ltgtype == LTG_DEFENDKEYAREA ||
					bs->ltgtype == LTG_GETFLAG ||
					bs->ltgtype == LTG_RUSHBASE ||
					bs->ltgtype == LTG_CAMPORDER ||
					bs->ltgtype == LTG_PATROL ||
					bs->ltgtype == LTG_ATTACKENEMYBASE ||
					bs->ltgtype == LTG_GETITEM ||
					bs->ltgtype == LTG_MAKELOVE_UNDER ||
					bs->ltgtype == LTG_MAKELOVE_ONTOP) {
				return;
			}
			if (bs->ltgtype != LTG_ATTACKENEMYBASE) {
				BotRefuseOrder(bs);
				bs->decisionmaker = bs->client;
				bs->ordered = qfalse;
				if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
				else memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
				bs->ltgtype = LTG_ATTACKENEMYBASE;
				bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
				BotSetTeamStatus(bs);
				bs->owndecision_time = (int)(FloatTime() + 5.0f);
			}
		}
		return;
	}
	else if (bs->neutralflagstatus == 2) {
		if (bs->owndecision_time < FloatTime()) {
			c = BotEnemyFlagCarrierVisible(bs);
			if (c >= 0) {
			}
			if (bs->ltgtype == LTG_TEAMHELP ||
					bs->ltgtype == LTG_TEAMACCOMPANY ||
					bs->ltgtype == LTG_CAMPORDER ||
					bs->ltgtype == LTG_PATROL ||
					bs->ltgtype == LTG_GETITEM) {
				return;
			}
			if (bs->ltgtype != LTG_DEFENDKEYAREA) {
				BotRefuseOrder(bs);
				bs->decisionmaker = bs->client;
				bs->ordered = qfalse;
				if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
				else memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
				bs->ltgtype = LTG_DEFENDKEYAREA;
				bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
				bs->defendaway_time = 0;
				BotSetTeamStatus(bs);
				bs->owndecision_time = (int)(FloatTime() + 5.0f);
			}
		}
		return;
	}
	if (BotTeamLeader(bs)) {
		return;
	}
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	if ( !bs->ordered && bs->lastgoal_ltgtype ) {
		bs->ltgtype = 0;
	}
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_RETURNFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_ATTACKENEMYBASE ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	if (BotSetLastOrderedTask(bs))
		return;
	if (bs->owndecision_time > FloatTime())
		return;;
	if (bs->ctfroam_time > FloatTime())
		return;
	if (BotAggression(bs) < 50)
		return;
	bs->teammessage_time = FloatTime() + 2 * random();
	if (bs->teamtaskpreference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	rnd = random();
	if (rnd < l1 && ctf_neutralflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		bs->ltgtype = LTG_GETFLAG;
		bs->teamgoal_time = FloatTime() + CTF_GETFLAG_TIME;
		BotSetTeamStatus(bs);
	}
	else if (rnd < l2 && ctf_redflag.areanum && ctf_blueflag.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &ctf_redflag, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &ctf_blueflag, sizeof(bot_goal_t));
		bs->ltgtype = LTG_DEFENDKEYAREA;
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
	bs->owndecision_time = (int)(FloatTime() + 5.0f);
#ifdef DEBUG
	BotPrintTeamGoal(bs);
#endif 
}

void Bot1FCTFRetreatGoals(bot_state_t *bs) {
	if (Bot1FCTFCarryingFlag(bs)) {
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			BotSetTeamStatus(bs);
		}
	}
}

void BotObeliskSeekGoals(bot_state_t *bs) {
	float rnd, l1, l2;

	if (BotTeamLeader(bs)) {
		return;
	}
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_RETURNFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_ATTACKENEMYBASE ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	if (BotSetLastOrderedTask(bs))
		return;
	if (bs->ctfroam_time > FloatTime())
		return;
	if (BotAggression(bs) < 50)
		return;
	bs->teammessage_time = FloatTime() + 2 * random();
	if (bs->teamtaskpreference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	rnd = random();
	if (rnd < l1 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
		bs->ltgtype = LTG_ATTACKENEMYBASE;
		bs->teamgoal_time = FloatTime() + TEAM_ATTACKENEMYBASE_TIME;
		BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
		BotSetTeamStatus(bs);
	}
	else if (rnd < l2 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
		bs->ltgtype = LTG_DEFENDKEYAREA;
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
}

void BotGoHarvest(bot_state_t *bs) {
	if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
	else memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
	bs->ltgtype = LTG_HARVEST;
	bs->teamgoal_time = FloatTime() + TEAM_HARVEST_TIME;
	bs->harvestaway_time = 0;
	BotSetTeamStatus(bs);
}

void BotObeliskRetreatGoals(bot_state_t *bs) {
}

void BotHarvesterSeekGoals(bot_state_t *bs) {
	aas_entityinfo_t entinfo;
	float rnd, l1, l2;
	int c;

	if (BotHarvesterCarryingCubes(bs)) {
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotGetAlternateRouteGoal(bs, BotOppositeTeam(bs));
			BotSetTeamStatus(bs);
		}
		return;
	}
	if (BotTeamLeader(bs)) {
		return;
	}
	if ( bs->ltgtype == LTG_TEAMACCOMPANY && !bs->ordered ) {
		BotEntityInfo(bs->teammate, &entinfo);
		if (!EntityCarriesCubes(&entinfo)) {
			bs->ltgtype = 0;
		}
	}
	if ( bs->lastgoal_ltgtype ) {
		bs->teamgoal_time += 60;
	}
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL ||
			bs->ltgtype == LTG_ATTACKENEMYBASE ||
			bs->ltgtype == LTG_HARVEST ||
			bs->ltgtype == LTG_GETITEM ||
			bs->ltgtype == LTG_MAKELOVE_UNDER ||
			bs->ltgtype == LTG_MAKELOVE_ONTOP) {
		return;
	}
	if (BotSetLastOrderedTask(bs))
		return;
	if (bs->ctfroam_time > FloatTime())
		return;
	if (BotAggression(bs) < 50)
		return;
	bs->teammessage_time = FloatTime() + 2 * random();
	c = BotEnemyCubeCarrierVisible(bs);
	if (c >= 0) {
	}
	if (bs->ltgtype != LTG_TEAMACCOMPANY) {
		c = BotTeamCubeCarrierVisible(bs);
		if (c >= 0) {
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			bs->teammate = c;
			bs->teammatevisible_time = FloatTime();
			bs->teammessage_time = 0;
			bs->arrive_time = 1;
			BotVoiceChat(bs, bs->teammate, VOICECHAT_ONFOLLOW);
			bs->teamgoal_time = FloatTime() + TEAM_ACCOMPANY_TIME;
			bs->ltgtype = LTG_TEAMACCOMPANY;
			bs->formation_dist = 3.5 * 32;		
			BotSetTeamStatus(bs);
			return;
		}
	}
	if (bs->teamtaskpreference & (TEAMTP_ATTACKER|TEAMTP_DEFENDER)) {
		if (bs->teamtaskpreference & TEAMTP_ATTACKER) {
			l1 = 0.7f;
		}
		else {
			l1 = 0.2f;
		}
		l2 = 0.9f;
	}
	else {
		l1 = 0.4f;
		l2 = 0.7f;
	}
	rnd = random();
	if (rnd < l1 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		BotGoHarvest(bs);
	}
	else if (rnd < l2 && redobelisk.areanum && blueobelisk.areanum) {
		bs->decisionmaker = bs->client;
		bs->ordered = qfalse;
		if (BotTeam(bs) == TEAM_RED) memcpy(&bs->teamgoal, &redobelisk, sizeof(bot_goal_t));
		else memcpy(&bs->teamgoal, &blueobelisk, sizeof(bot_goal_t));
		bs->ltgtype = LTG_DEFENDKEYAREA;
		bs->teamgoal_time = FloatTime() + TEAM_DEFENDKEYAREA_TIME;
		bs->defendaway_time = 0;
		BotSetTeamStatus(bs);
	}
	else {
		bs->ltgtype = 0;
		bs->ctfroam_time = FloatTime() + CTF_ROAM_TIME;
		BotSetTeamStatus(bs);
	}
}

void BotHarvesterRetreatGoals(bot_state_t *bs) {
	if (BotHarvesterCarryingCubes(bs)) {
		if (bs->ltgtype != LTG_RUSHBASE) {
			BotRefuseOrder(bs);
			bs->ltgtype = LTG_RUSHBASE;
			bs->teamgoal_time = FloatTime() + CTF_RUSHBASE_TIME;
			bs->rushbaseaway_time = 0;
			bs->decisionmaker = bs->client;
			bs->ordered = qfalse;
			BotSetTeamStatus(bs);
		}
		return;
	}
}
#endif

void BotTeamGoals(bot_state_t *bs, int retreat) {
	if ((gametype == GT_OBELISK) && (redobelisk.areanum) && (blueobelisk.areanum)) {
		BotSetEntityNumForGoal(&redobelisk, "DestructionObject-red");
		BotSetEntityNumForGoal(&blueobelisk, "DestructionObject-blue");
		if (redobelisk.entitynum && blueobelisk.entitynum) {
		}
		else {
			redobelisk.areanum = 0;
			blueobelisk.areanum = 0;
		}
	}

	if (gametype == GT_CTF)
		if (multiplayerManager.checkGameType("oneflag"))
			gametype = GT_1FCTF;

	if ( retreat ) {
		if (gametype == GT_CTF) {
			BotCTFRetreatGoals(bs);
		}
#ifdef MISSIONPACK
		else if (gametype == GT_1FCTF) {
			Bot1FCTFRetreatGoals(bs);
		}
		else if (gametype == GT_OBELISK) {
			BotObeliskRetreatGoals(bs);
		}
		else if (gametype == GT_HARVESTER) {
			BotHarvesterRetreatGoals(bs);
		}
#endif
	}
	else {
		if (gametype == GT_CTF) {
			BotCTFSeekGoals(bs);
		}
#ifdef MISSIONPACK
		else if (gametype == GT_1FCTF) {
			Bot1FCTFSeekGoals(bs);
		}
		else if (gametype == GT_OBELISK) {
			BotObeliskSeekGoals(bs);
		}
		else if (gametype == GT_HARVESTER) {
			BotHarvesterSeekGoals(bs);
		}
#endif
	}
	bs->order_time = 0;
}

int BotPointAreaNum(vec3_t origin) {
	int areanum, numareas, areas[10];
	vec3_t end;

	areanum = gi.AAS_PointAreaNum(origin);
	if (areanum) return areanum;
	VectorCopy(origin, end);
	end[2] += 10;
	numareas = gi.AAS_TraceAreas(origin, end, areas, NULL, 10);
	if (numareas > 0) return areas[0];
	return 0;
}

char *ClientName(int client, char *name, int size) {
	char buf[MAX_INFO_STRING];

	if (client < 0 || client >= maxclients->integer) {
		BotAI_Print(PRT_ERROR, "ClientName: client out of range\n");
		return "[client out of range]";
	}
	Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+client), sizeof(buf));
	Q_strncpyz(name, Info_ValueForKey(buf, "name"), size-1);
	name[size-1] = '\0';
	Q_CleanStr( name );
	return name;
}

char *ClientSkin(int client, char *skin, int size) {
	char buf[MAX_INFO_STRING];

	if (client < 0 || client >= maxclients->integer) {
		BotAI_Print(PRT_ERROR, "ClientSkin: client out of range\n");
		return "[client out of range]";
	}
	Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+client), sizeof(buf));
	Q_strncpyz(skin, Info_ValueForKey(buf, "model"), size-1);
	skin[size-1] = '\0';
	return skin;
}

int ClientFromName(char *name) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		Q_CleanStr( buf );
		if (!Q_stricmp(Info_ValueForKey(buf, "name"), name)) 
			return i;
	}
	return -1;
}

int ClientOnSameTeamFromName(bot_state_t *bs, char *name) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (!BotSameTeam(bs, i))
			continue;
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		Q_CleanStr( buf );
		if (!Q_stricmp(Info_ValueForKey(buf, "ni"), name)) return i;
	}
	return -1;
}

char *stristr(char *str, char *charset) {
	int i;

	while(*str) {
		for (i = 0; charset[i] && str[i]; i++) {
			if (toupper(charset[i]) != toupper(str[i])) break;
		}
		if (!charset[i]) return str;
		str++;
	}
	return NULL;
}

char *EasyClientName(int client, char *buf, int size) {
	int i;
	char *str1, *str2, *ptr, c;
	char name[128];

	strcpy(name, ClientName(client, name, sizeof(name)));
	for (i = 0; name[i]; i++) name[i] &= 127;
	for (ptr = strstr(name, " "); ptr; ptr = strstr(name, " ")) {
		memmove(ptr, ptr+1, strlen(ptr+1)+1);
	}
	str1 = strstr(name, "[");
	str2 = strstr(name, "]");
	if (str1 && str2) {
		if (str2 > str1) memmove(str1, str2+1, strlen(str2+1)+1);
		else memmove(str2, str1+1, strlen(str1+1)+1);
	}
	if ((name[0] == 'm' || name[0] == 'M') &&
			(name[1] == 'r' || name[1] == 'R')) {
		memmove(name, name+2, strlen(name+2)+1);
	}
	ptr = name;
	while(*ptr) {
		c = *ptr;
		if ((c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '_') {
			ptr++;
		}
		else if (c >= 'A' && c <= 'Z') {
			*ptr += 'a' - 'A';
			ptr++;
		}
		else {
			memmove(ptr, ptr+1, strlen(ptr + 1)+1);
		}
	}
	Q_strncpyz(buf, name, size-1);
	buf[size-1] = '\0';
	return buf;
}

int BotSynonymContext(bot_state_t *bs) {
	int context;

	context = CONTEXT_NORMAL|CONTEXT_NEARBYITEM|CONTEXT_NAMES;
	if (gametype == GT_CTF
#ifdef MISSIONPACK
		|| gametype == GT_1FCTF
#endif
		) {
		if (BotTeam(bs) == TEAM_RED) context |= CONTEXT_CTFREDTEAM;
		else context |= CONTEXT_CTFBLUETEAM;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_OBELISK) {
		if (BotTeam(bs) == TEAM_RED) context |= CONTEXT_OBELISKREDTEAM;
		else context |= CONTEXT_OBELISKBLUETEAM;
	}
	else if (gametype == GT_HARVESTER) {
		if (BotTeam(bs) == TEAM_RED) context |= CONTEXT_HARVESTERREDTEAM;
		else context |= CONTEXT_HARVESTERBLUETEAM;
	}
#endif
	return context;
}

extern Event EV_Player_UseItem;
void BotChooseWeapon(bot_state_t *bs) {
	int newweaponnum;
	weaponinfo_t wi;
	newweaponnum = gi.BotChooseBestFightWeapon(bs->ws, bs->inventory);

	if (newweaponnum) {
		gi.BotGetWeaponInfo(bs->ws, newweaponnum, &wi);
		if (bs->weaponnum != newweaponnum) {
			bs->weaponchange_time = FloatTime();
		}
		bs->lastucmd.weapon = newweaponnum;

		if (bs->weaponnum != bs->lastucmd.weapon) {
			gentity_t   *ed = &g_entities[ bs->client ];
			Entity *ent = ed->entity;
			Player *plyr = (Player *)ent;
			if (plyr->useWeapon(wi.name,WEAPON_DUAL)) 
				bs->weaponnum = bs->lastucmd.weapon;
		}
	}
}

void BotSetupForMovement(bot_state_t *bs) {
	bot_initmove_t initmove;

	memset(&initmove, 0, sizeof(bot_initmove_t));
	VectorCopy(bs->cur_ps.origin, initmove.origin);
	VectorCopy(bs->cur_ps.velocity, initmove.velocity);
	VectorClear(initmove.viewoffset);
	initmove.viewoffset[2] += bs->cur_ps.viewheight;
	initmove.entitynum = bs->entitynum;
	initmove.client = bs->client;
	initmove.thinktime = bs->thinktime;
	if (bs->cur_ps.groundEntityNum != ENTITYNUM_NONE) 
		initmove.or_moveflags |= MFL_ONGROUND;
	if ((bs->cur_ps.pm_flags & PMF_TIME_KNOCKBACK) && (bs->cur_ps.pm_time > 0)) {
		initmove.or_moveflags |= MFL_TELEPORTED;
	}
	if ((bs->cur_ps.pm_flags & PMF_TIME_WATERJUMP) && (bs->cur_ps.pm_time > 0)) {
		initmove.or_moveflags |= MFL_WATERJUMP;
	}
	if (bs->cur_ps.pm_flags & PMF_DUCKED) initmove.presencetype = PRESENCE_CROUCH;
	else initmove.presencetype = PRESENCE_NORMAL;
	if (bs->walker > 0.5) initmove.or_moveflags |= MFL_WALK;
	VectorCopy(bs->viewangles, initmove.viewangles);
	gi.BotInitMoveState(bs->ms, &initmove);
}

void BotCheckItemPickup(bot_state_t *bs, int *oldinventory) {
#ifdef MISSIONPACK
	int offence, leader;

	if (gametype <= GT_TEAM)
		return;

	offence = -1;
	if (!oldinventory[INVENTORY_KAMIKAZE] && bs->inventory[INVENTORY_KAMIKAZE] >= 1) {
		offence = qtrue;
	}
	if (!oldinventory[INVENTORY_INVULNERABILITY] && bs->inventory[INVENTORY_INVULNERABILITY] >= 1) {
		offence = qtrue;
	}
	if (!bs->inventory[INVENTORY_KAMIKAZE] && !bs->inventory[INVENTORY_INVULNERABILITY]) {
		if (!oldinventory[INVENTORY_SCOUT] && bs->inventory[INVENTORY_SCOUT] >= 1) {
			offence = qtrue;
		}
		if (!oldinventory[INVENTORY_GUARD] && bs->inventory[INVENTORY_GUARD] >= 1) {
			offence = qtrue;
		}
		if (!oldinventory[INVENTORY_DOUBLER] && bs->inventory[INVENTORY_DOUBLER] >= 1) {
			offence = qfalse;
		}
		if (!oldinventory[INVENTORY_AMMOREGEN] && bs->inventory[INVENTORY_AMMOREGEN] >= 1) {
			offence = qfalse;
		}
	}

	if (offence >= 0) {
		leader = ClientFromName(bs->teamleader);
		if (offence) {
			if (!(bs->teamtaskpreference & TEAMTP_ATTACKER)) {
				if (BotTeamLeader(bs)) {
					BotVoiceChat(bs, leader, VOICECHAT_WANTONOFFENSE);
				}
				else if (g_spSkill.integer <= 3) {
					if ( bs->ltgtype != LTG_GETFLAG &&
						 bs->ltgtype != LTG_ATTACKENEMYBASE &&
						 bs->ltgtype != LTG_HARVEST ) {
						if ((gametype != GT_CTF || (bs->redflagstatus == 0 && bs->blueflagstatus == 0)) &&
							(gametype != GT_1FCTF || bs->neutralflagstatus == 0) ) {
							BotVoiceChat(bs, leader, VOICECHAT_WANTONOFFENSE);
						}
					}
					bs->teamtaskpreference |= TEAMTP_ATTACKER;
				}
			}
			bs->teamtaskpreference &= ~TEAMTP_DEFENDER;
		}
		else {
			if (!(bs->teamtaskpreference & TEAMTP_DEFENDER)) {
				if (BotTeamLeader(bs)) {
					BotVoiceChat(bs, -1, VOICECHAT_WANTONDEFENSE);
				}
				else if (g_spSkill.integer <= 3) {
					if ( bs->ltgtype != LTG_DEFENDKEYAREA ) {
						if ((gametype != GT_CTF || (bs->redflagstatus == 0 && bs->blueflagstatus == 0)) &&
							(gametype != GT_1FCTF || bs->neutralflagstatus == 0) ) {
							BotVoiceChat(bs, -1, VOICECHAT_WANTONDEFENSE);
						}
					}
				}
				bs->teamtaskpreference |= TEAMTP_DEFENDER;
			}
			bs->teamtaskpreference &= ~TEAMTP_ATTACKER;
		}
	}
#endif
}

void BotUpdateInventory(bot_state_t *bs) {
	int oldinventory[MAX_ITEMS];
	memcpy(oldinventory, bs->inventory, sizeof(oldinventory));

	memset(bs->inventory,0,sizeof(int)*MAX_ITEMS);

	if ( ( bs->cur_ps.clientNum < 0 ) || ( bs->cur_ps.clientNum > maxclients->integer ) )
		return;

	Player *player = (Player *)g_entities[bs->cur_ps.clientNum].entity;
	if (player) {
		Item *item = player->NextItem(NULL);
		while (item != NULL) {
			bs->inventory[item->GetBotInventoryIndex()] = 
				(int)item->getAmount();
			item = player->NextItem(item);
		}
		item = player->FindBaseArmor();
		if (item)
			bs->inventory[INVENTORY_ARMOR] = (int)item->getAmount();
		bs->inventory[INVENTORY_PLASMA] = player->AmmoCount( "Plasma" );
		bs->inventory[INVENTORY_FED] = player->AmmoCount( "Fed" );
		bs->inventory[INVENTORY_IDRYLL] = player->AmmoCount( "Idryll" );

		item = (Item *)player->getHoldableItem();
		if (item)
			bs->inventory[item->GetBotInventoryIndex()] = (int)item->getAmount();
		
		bs->inventory[INVENTORY_REDFLAG] = multiplayerManager.doesPlayerHaveItem( player, "ctfflag-red" );
		bs->inventory[INVENTORY_BLUEFLAG] = multiplayerManager.doesPlayerHaveItem( player, "ctfflag-blue" );
		bs->inventory[INVENTORY_NEUTRALFLAG] = multiplayerManager.doesPlayerHaveItem( player, "ctfflag-one" );

		Player *target;
		target = multiplayerManager.getLastKillerOfPlayer(player,&bs->botdeathtype);
		if (target)
			bs->lastkilledby = target->edict->s.number;
		target = multiplayerManager.getLastKilledByPlayer(player,&bs->enemydeathtype);
		if (target)
			bs->lastkilledplayer = target->edict->s.number;
	}
	bs->lasthealth = oldinventory[INVENTORY_HEALTH];
	bs->inventory[INVENTORY_HEALTH] = bs->cur_ps.stats[STAT_HEALTH];
	
	BotCheckItemPickup(bs, oldinventory);
}

void BotUpdateBattleInventory(bot_state_t *bs, int enemy) {
	vec3_t dir;
	aas_entityinfo_t entinfo;

	BotEntityInfo(enemy, &entinfo);
	VectorSubtract(entinfo.origin, bs->origin, dir);
	bs->inventory[ENEMY_HEIGHT] = (int) dir[2];
	dir[2] = 0;

	bs->inventory[ENEMY_HORIZONTAL_DIST] = (int) VectorLength(dir);


}

#ifdef MISSIONPACK
#define KAMIKAZE_DIST		1024

void BotUseKamikaze(bot_state_t *bs) {
	int c, teammates, enemies;
	aas_entityinfo_t entinfo;
	vec3_t dir, target;
	bot_goal_t *goal;
	bsp_trace_t trace;

	if (bs->inventory[INVENTORY_KAMIKAZE] <= 0)
		return;
	if (bs->kamikaze_time > FloatTime())
		return;
	bs->kamikaze_time = FloatTime() + 0.2;
	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs))
			return;
		c = BotTeamFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
				return;
		}
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs))
			return;
		c = BotTeamFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
				return;
		}
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_OBELISK) {
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &blueobelisk; break;
			default: goal = &redobelisk; break;
		}
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST * 0.9)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs))
			return;
		c = BotTeamCubeCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST))
				return;
		}
		c = BotEnemyCubeCarrierVisible(bs);
		if (c >= 0) {
			BotEntityInfo(c, &entinfo);
			VectorSubtract(entinfo.origin, bs->origin, dir);
			if (VectorLengthSquared(dir) < Square(KAMIKAZE_DIST)) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	BotVisibleTeamMatesAndEnemies(bs, &teammates, &enemies, KAMIKAZE_DIST);
	if (enemies > 2 && enemies > teammates+1) {
		gi.EA_Use(bs->client);
		return;
	}
}

void BotUseInvulnerability(bot_state_t *bs) {
	int c;
	vec3_t dir, target;
	bot_goal_t *goal;
	bsp_trace_t trace;

	if (bs->inventory[INVENTORY_INVULNERABILITY] <= 0)
		return;
	if (bs->invulnerability_time > FloatTime())
		return;
	bs->invulnerability_time = FloatTime() + 0.2;
	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs))
			return;
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0)
			return;
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &ctf_blueflag; break;
			default: goal = &ctf_redflag; break;
		}
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(200)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs))
			return;
		c = BotEnemyFlagCarrierVisible(bs);
		if (c >= 0)
			return;
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &ctf_blueflag; break;
			default: goal = &ctf_redflag; break;
		}
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(200)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_OBELISK) {
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &blueobelisk; break;
			default: goal = &redobelisk; break;
		}
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(300)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs))
			return;
		c = BotEnemyCubeCarrierVisible(bs);
		if (c >= 0)
			return;
		switch(BotTeam(bs)) {
			case TEAM_RED: goal = &blueobelisk; break;
			default: goal = &redobelisk; break;
		}
		VectorCopy(goal->origin, target);
		target[2] += 1;
		VectorSubtract(bs->origin, target, dir);
		if (VectorLengthSquared(dir) < Square(200)) {
			BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
			if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
				gi.EA_Use(bs->client);
				return;
			}
		}
	}
}
#endif

void BotBattleUseItems(bot_state_t *bs) {

	if ( ( bs->cur_ps.clientNum < 0 ) || ( bs->cur_ps.clientNum > maxclients->integer ) )
		return;

	if (bs->inventory[INVENTORY_HEALTH] < 40) {
		if (bs->inventory[INVENTORY_TELEPORTER] > 0) {
			if (!BotCTFCarryingFlag(bs)
#ifdef MISSIONPACK
				&& !Bot1FCTFCarryingFlag(bs)
				&& !BotHarvesterCarryingCubes(bs)
#endif
				) {
				Player *player = (Player *)g_entities[bs->cur_ps.clientNum].entity;
				player->useHoldableItem();
			}
		}
	}
	
	if ((bs->inventory[INVENTORY_HEALTH] < 30) || (bs->lasthealth > bs->inventory[INVENTORY_HEALTH])) {
		if (bs->inventory[INVENTORY_INVULNERABILITY]) {
			Player *player = (Player *)g_entities[bs->cur_ps.clientNum].entity;
			player->useHoldableItem();
		}
	}

	if (bs->inventory[INVENTORY_HEALTH] < 60) {
		if (bs->inventory[INVENTORY_MEDKIT] > 0) {
			Player *player = (Player *)g_entities[bs->cur_ps.clientNum].entity;
			player->useHoldableItem();
		}
	}
#ifdef MISSIONPACK
	BotUseKamikaze(bs);
	BotUseInvulnerability(bs);
#endif
}

void BotSetTeleportTime(bot_state_t *bs) {
}

qboolean BotIsDead(bot_state_t *bs) {
	return (bs->cur_ps.pm_type == PM_DEAD);
}

qboolean BotIsObserver(bot_state_t *bs) {
	return qfalse;
}

qboolean BotIntermission(bot_state_t *bs) {
	if (level.intermissiontime) return qtrue;
	return qfalse;
}

qboolean BotInLavaOrSlime(bot_state_t *bs) {
	vec3_t feet;

	VectorCopy(bs->origin, feet);
	feet[2] -= 23;
	return (gi.AAS_PointContents(feet) & (CONTENTS_LAVA|CONTENTS_SLIME));
}

bot_waypoint_t *BotCreateWayPoint(char *name, vec3_t origin, int areanum) {
	bot_waypoint_t *wp;
	vec3_t waypointmins = {-8, -8, -8}, waypointmaxs = {8, 8, 8};

	wp = botai_freewaypoints;
	if ( !wp ) {
		BotAI_Print( PRT_WARNING, "BotCreateWayPoint: Out of waypoints\n" );
		return NULL;
	}
	botai_freewaypoints = botai_freewaypoints->next;

	Q_strncpyz( wp->name, name, sizeof(wp->name) );
	VectorCopy(origin, wp->goal.origin);
	VectorCopy(waypointmins, wp->goal.mins);
	VectorCopy(waypointmaxs, wp->goal.maxs);
	wp->goal.areanum = areanum;
	wp->next = NULL;
	wp->prev = NULL;
	return wp;
}

bot_waypoint_t *BotFindWayPoint(bot_waypoint_t *waypoints, char *name) {
	bot_waypoint_t *wp;

	for (wp = waypoints; wp; wp = wp->next) {
		if (!Q_stricmp(wp->name, name)) return wp;
	}
	return NULL;
}

void BotFreeWaypoints(bot_waypoint_t *wp) {
	bot_waypoint_t *nextwp;

	for (; wp; wp = nextwp) {
		nextwp = wp->next;
		wp->next = botai_freewaypoints;
		botai_freewaypoints = wp;
	}
}

void BotInitWaypoints(void) {
	int i;

	botai_freewaypoints = NULL;
	for (i = 0; i < MAX_WAYPOINTS; i++) {
		botai_waypoints[i].next = botai_freewaypoints;
		botai_freewaypoints = &botai_waypoints[i];
	}
}

int TeamPlayIsOn(void) {
	return ( gametype >= GT_TEAM );
}

float BotAggression(bot_state_t *bs) {
	if (bs->inventory[INVENTORY_QUAD]) {
		return 70;
	}
	if (bs->inventory[ENEMY_HEIGHT] > 200) return 0;
	if (bs->inventory[INVENTORY_HEALTH] < 60) return 0;
	if (bs->inventory[INVENTORY_HEALTH] < 80) {
		if (bs->inventory[INVENTORY_ARMOR] < 40) return 0;
	}
	if (bs->inventory[INVENTORY_BFG10K] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 7) return 100;
	if (bs->inventory[INVENTORY_RAILGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 5) return 95;
	if (bs->inventory[INVENTORY_LIGHTNING] > 0 &&
			bs->inventory[INVENTORY_IDRYLL] > 50) return 90;
	if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 &&
			bs->inventory[INVENTORY_FED] > 5) return 90;
	if (bs->inventory[INVENTORY_PLASMAGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 40) return 85;
	if (bs->inventory[INVENTORY_GRENADELAUNCHER] > 0 &&
			bs->inventory[INVENTORY_FED] > 10) return 80;
	if (bs->inventory[INVENTORY_SHOTGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 10) return 60;
	return 0;
}

float BotFeelingBad(bot_state_t *bs) {
	if (bs->inventory[INVENTORY_HEALTH] < 40) {
		return 100;
	}
	if (bs->inventory[INVENTORY_HEALTH] < 60) {
		return 80;
	}
	return 0;
}

int BotWantsToRetreat(bot_state_t *bs) {
	aas_entityinfo_t entinfo;

	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs))
			return qtrue;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs))
			return qtrue;
	}
	else if (gametype == GT_OBELISK) {
		if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
			if (bs->enemy != redobelisk.entitynum ||
						bs->enemy != blueobelisk.entitynum) {
				return qtrue;
			}
		}
		if (BotFeelingBad(bs) > 50) {
			return qtrue;
		}
		return qfalse;
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs)) return qtrue;
	}
#endif
	if (bs->enemy >= 0) {
		BotEntityInfo(bs->enemy, &entinfo);
		if (EntityCarriesFlag(&entinfo))
			return qfalse;
	}
	if (bs->ltgtype == LTG_GETFLAG)
		return qtrue;
	if (BotAggression(bs) < 50)
		return qtrue;
	return qfalse;
}

int BotWantsToChase(bot_state_t *bs) {
	aas_entityinfo_t entinfo;

	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs))
			return qfalse;
		BotEntityInfo(bs->enemy, &entinfo);
		if (EntityCarriesFlag(&entinfo))
			return qtrue;
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs))
			return qfalse;
		BotEntityInfo(bs->enemy, &entinfo);
		if (EntityCarriesFlag(&entinfo))
			return qtrue;
	}
	else if (gametype == GT_OBELISK) {
		if (bs->ltgtype == LTG_ATTACKENEMYBASE) {
			if (bs->enemy != redobelisk.entitynum ||
						bs->enemy != blueobelisk.entitynum) {
				return qfalse;
			}
		}
		else if ( ( bs->enemy == redobelisk.entitynum ) || ( bs->enemy == blueobelisk.entitynum ) ) {
			return qtrue;
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs))
			return qfalse;
	}
#endif
	if (bs->ltgtype == LTG_GETFLAG)
		return qfalse;
	if (BotAggression(bs) > 50)
		return qtrue;
	return qfalse;
}

int BotWantsToHelp(bot_state_t *bs) {
	return qtrue;
}

int BotCanAndWantsToRocketJump(bot_state_t *bs) {
	float rocketjumper;

	if (!bot_rocketjump.integer) return qfalse;
	if (bs->inventory[INVENTORY_ROCKETLAUNCHER] <= 0) return qfalse;
	if (bs->inventory[INVENTORY_PLASMA] < 3) return qfalse;
	if (bs->inventory[INVENTORY_QUAD]) return qfalse;
	if (bs->inventory[INVENTORY_HEALTH] < 60) return qfalse;
	if (bs->inventory[INVENTORY_HEALTH] < 90) {
		if (bs->inventory[INVENTORY_ARMOR] < 40) return qfalse;
	}
	rocketjumper = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_WEAPONJUMPING, 0, 1);
	if (rocketjumper < 0.5) return qfalse;
	return qtrue;
}

int BotHasPersistantPowerupAndWeapon(bot_state_t *bs) { 
#ifdef MISSIONPACK
	if (!bs->inventory[INVENTORY_SCOUT] &&
		!bs->inventory[INVENTORY_GUARD] &&
		!bs->inventory[INVENTORY_DOUBLER] &&
		!bs->inventory[INVENTORY_AMMOREGEN] ) {
		return qfalse;
	}
#endif
	if (bs->inventory[INVENTORY_HEALTH] < 60) return qfalse;
	if (bs->inventory[INVENTORY_HEALTH] < 80) {
		if (bs->inventory[INVENTORY_ARMOR] < 40) return qfalse;
	}
	if (bs->inventory[INVENTORY_BFG10K] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 7) return qtrue;
	if (bs->inventory[INVENTORY_RAILGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 5) return qtrue;
	if (bs->inventory[INVENTORY_LIGHTNING] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 50) return qtrue;
	if (bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 5) return qtrue;
	if (bs->inventory[INVENTORY_NAILGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 5) return qtrue;
	if (bs->inventory[INVENTORY_PROXLAUNCHER] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 5) return qtrue;
	if (bs->inventory[INVENTORY_CHAINGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 40) return qtrue;
	if (bs->inventory[INVENTORY_PLASMAGUN] > 0 &&
			bs->inventory[INVENTORY_PLASMA] > 20) return qtrue;
	return qfalse;
}

void BotGoCamp(bot_state_t *bs, bot_goal_t *goal) {
	float camper;

	bs->decisionmaker = bs->client;
	bs->teammessage_time = 0;
	bs->ltgtype = LTG_CAMP;
	memcpy(&bs->teamgoal, goal, sizeof(bot_goal_t));
	camper = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
	if (camper > 0.99) bs->teamgoal_time = FloatTime() + 99999;
	else bs->teamgoal_time = FloatTime() + 120 + 180 * camper + random() * 15;
	bs->camp_time = FloatTime();
	bs->teammate = 0;
	bs->arrive_time = 1;
}

int BotWantsToCamp(bot_state_t *bs) {
	float camper;
	int cs, traveltime, besttraveltime;
	bot_goal_t goal, bestgoal;

	camper = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_CAMPER, 0, 1);
	if (camper < 0.1) return qfalse;
	if (bs->ltgtype == LTG_TEAMHELP ||
			bs->ltgtype == LTG_TEAMACCOMPANY ||
			bs->ltgtype == LTG_DEFENDKEYAREA ||
			bs->ltgtype == LTG_GETFLAG ||
			bs->ltgtype == LTG_RUSHBASE ||
			bs->ltgtype == LTG_CAMP ||
			bs->ltgtype == LTG_CAMPORDER ||
			bs->ltgtype == LTG_PATROL) {
		return qfalse;
	}
	if (bs->camp_time > FloatTime() - 60 + 300 * (1-camper)) return qfalse;
	if (random() > camper) {
		bs->camp_time = FloatTime();
		return qfalse;
	}
	if (BotAggression(bs) < 50) return qfalse;
	if ((bs->inventory[INVENTORY_ROCKETLAUNCHER] <= 0 || bs->inventory[INVENTORY_FED < 10]) &&
		(bs->inventory[INVENTORY_RAILGUN] <= 0 || bs->inventory[INVENTORY_IDRYLL] < 10) &&
		(bs->inventory[INVENTORY_BFG10K] <= 0 || bs->inventory[INVENTORY_PLASMA] < 10)) {
		return qfalse;
	}
	besttraveltime = 99999;
	for (cs = gi.BotGetNextCampSpotGoal(0, &goal); cs; cs = gi.BotGetNextCampSpotGoal(cs, &goal)) {
		traveltime = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, goal.areanum, TFL_DEFAULT);
		if (traveltime && traveltime < besttraveltime) {
			besttraveltime = traveltime;
			memcpy(&bestgoal, &goal, sizeof(bot_goal_t));
		}
	}
	if (besttraveltime > 150) return qfalse;
	BotGoCamp(bs, &bestgoal);
	bs->ordered = qfalse;
	return qtrue;
}

void BotDontAvoid(bot_state_t *bs, char *itemname) {
	bot_goal_t goal;
	int num;

	num = gi.BotGetLevelItemGoal(-1, itemname, &goal);
	while(num >= 0) {
		gi.BotRemoveFromAvoidGoals(bs->gs, goal.number);
		num = gi.BotGetLevelItemGoal(num, itemname, &goal);
	}
}

void BotGoForPowerups(bot_state_t *bs) {

	BotDontAvoid(bs, "Quad Damage");
	BotDontAvoid(bs, "Regeneration");
	BotDontAvoid(bs, "Battle Suit");
	BotDontAvoid(bs, "Speed");
	BotDontAvoid(bs, "Invisibility");
	bs->ltg_time = 0;
}

void BotRoamGoal(bot_state_t *bs, vec3_t goal) {
	int pc, i;
	float len, rnd;
	vec3_t dir, bestorg, belowbestorg;
	bsp_trace_t trace;

	for (i = 0; i < 10; i++) {
		VectorCopy(bs->origin, bestorg);
		rnd = random();
		if (rnd > 0.25) {
			if (random() < 0.5) bestorg[0] -= 800 * random() + 100;
			else bestorg[0] += 800 * random() + 100;
		}
		if (rnd < 0.75) {
			if (random() < 0.5) bestorg[1] -= 800 * random() + 100;
			else bestorg[1] += 800 * random() + 100;
		}
		bestorg[2] += 2 * 48 * crandom();
		BotAI_Trace(&trace, bs->origin, NULL, NULL, bestorg, bs->entitynum, MASK_SOLID);
		VectorSubtract(trace.endpos, bs->origin, dir);
		len = VectorNormalize(dir);
		if (len > 200) {
			VectorScale(dir, len * trace.fraction - 40, dir);
			VectorAdd(bs->origin, dir, bestorg);
			belowbestorg[0] = bestorg[0];
			belowbestorg[1] = bestorg[1];
			belowbestorg[2] = bestorg[2] - 800;
			BotAI_Trace(&trace, bestorg, NULL, NULL, belowbestorg, bs->entitynum, MASK_SOLID);
			if (!trace.startsolid) {
				trace.endpos[2]++;
				pc = gi.PointContents(trace.endpos);
				if (!(pc & (CONTENTS_LAVA | CONTENTS_SLIME))) {
					VectorCopy(bestorg, goal);
					return;
				}
			}
		}
	}
	VectorCopy(bestorg, goal);
}

bot_moveresult_t BotAttackMove(bot_state_t *bs, int tfl) { 
	int movetype, i, attackentity;
	float attack_skill, jumper, croucher, dist, strafechange_time;
	float attack_dist=0, attack_range=0;
	vec3_t forward, backward, sideward, hordir, up = {0, 0, 1}, end;
	aas_entityinfo_t entinfo;
	bot_moveresult_t moveresult;
	bot_goal_t goal;

	attackentity = bs->enemy;
	if (bs->attackchase_time > FloatTime()) {
		goal.entitynum = attackentity;
		goal.areanum = bs->lastenemyareanum;
		VectorCopy(bs->lastenemyorigin, goal.origin);
		VectorSet(goal.mins, -8, -8, -8);
		VectorSet(goal.maxs, 8, 8, 8);
		BotSetupForMovement(bs);
		gi.BotMoveToGoal(&moveresult, bs->ms, &goal, tfl);
		return moveresult;
	}
	memset(&moveresult, 0, sizeof(bot_moveresult_t));
	attack_skill = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1);
	jumper = 0; 
	croucher = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_CROUCHER, 0, 1);
	if (attack_skill < 0.2) return moveresult;
	BotSetupForMovement(bs);
	BotEntityInfo(attackentity, &entinfo);
	VectorSubtract(entinfo.origin, bs->origin, forward);
	dist = VectorNormalize(forward);
	VectorNegate(forward, backward);
	movetype = MOVE_WALK;
	if (bs->attackcrouch_time < FloatTime() - 1) {
		if (random() < jumper) {
			movetype = MOVE_JUMP;
		}
		else if (bs->attackcrouch_time < FloatTime() - 1 && random() < croucher) {
			bs->attackcrouch_time = FloatTime() + croucher * 5;
		}
	}
	if (bs->attackcrouch_time > FloatTime()) movetype = MOVE_CROUCH;
	if (movetype == MOVE_JUMP) {
		if (bs->attackjump_time > FloatTime()) {
			movetype = MOVE_WALK;
		}
		else {
			bs->attackjump_time = FloatTime() + 1;
		}
	}
	// BOTTODO *NOTE* this only checks primary, not secondary fire mode, and it doesn't check against
	Player *bot1 = (Player *)g_entities[bs->client].entity;
	if (bot1) {
		Weapon *weapon = bot1->GetActiveWeapon( WEAPON_DUAL );
		if (weapon) {
			if (weapon->GetFireType(FIRE_MODE1) == FT_MELEE ) {
				attack_dist = 0;
				attack_range = 0;
			}
			else {
				attack_dist = IDEAL_ATTACKDIST;
				attack_range = 40;
			}
		}
	}
	else {
		attack_dist = IDEAL_ATTACKDIST;
		attack_range = 40;
	}
	if (attack_skill <= 0.4) {
		if (dist > attack_dist + attack_range) {
			if (gi.BotMoveInDirection(bs->ms, forward, sv_maxspeed->value, movetype)) return moveresult;
		}
		if (dist < attack_dist - attack_range) {
			if (gi.BotMoveInDirection(bs->ms, backward, sv_maxspeed->value, movetype)) return moveresult;
		}
		return moveresult;
	}
	bs->attackstrafe_time += bs->thinktime;
	strafechange_time = 0.4 + (1 - attack_skill) * 0.2;
	if (attack_skill > 0.7) strafechange_time += crandom() * 0.2;
	if (bs->attackstrafe_time > strafechange_time) {
		if (random() > 0.935) {
			bs->flags ^= BFL_STRAFERIGHT;
			bs->attackstrafe_time = 0;
		}
	}
	for (i = 0; i < 2; i++) {
		hordir[0] = forward[0];
		hordir[1] = forward[1];
		hordir[2] = 0;
		VectorNormalize(hordir);
		CrossProduct(hordir, up, sideward);
		if (bs->flags & BFL_STRAFERIGHT) VectorNegate(sideward, sideward);

		bsp_trace_t trace;
		VectorMA(bs->origin,40,sideward,end); 
		BotAI_Trace(&trace, bs->origin, NULL, NULL, end, bs->client, CONTENTS_SOLID);
		if (trace.fraction < 1.0f) {
			bs->flags ^= BFL_STRAFERIGHT;
			bs->attackstrafe_time = 0;
			VectorScale(sideward,-1.0f,sideward);
			if (gi.BotMoveInDirection(bs->ms, sideward, sv_maxspeed->value, movetype))
				return moveresult;
		}

		
		if (random() > 0.9) {
			VectorAdd(sideward, backward, sideward);
		}
		else {
			if (dist > attack_dist + attack_range) {
				VectorAdd(sideward, forward, sideward);
			}
			else if (dist < attack_dist - attack_range) {
				VectorAdd(sideward, backward, sideward);
			}
		}
		if (gi.BotMoveInDirection(bs->ms, sideward, sv_maxspeed->value, movetype))
			return moveresult;
		bs->flags ^= BFL_STRAFERIGHT;
		bs->attackstrafe_time = 0;
	}
	return moveresult;
}

int BotSameTeam(bot_state_t *bs, int entnum) {
	const char *info1 = NULL, *info2 = NULL;
	Team *team;

	if (bs->client < 0 || bs->client >= maxclients->integer) {
		return qfalse;
	}
	if (entnum < 0 || entnum >= maxclients->integer) {
		return qfalse;
	}
	if ( gametype >= GT_TEAM ) {
		Player *bot1 = (Player *)g_entities[bs->client].entity, *bot2 = (Player *)g_entities[entnum].entity;
		if (!bot1 || !bot2)
			return qfalse;
		team = multiplayerManager.getPlayersTeam(bot1 );
		if (team)
			info1 = team->getName().c_str();
		else
			return qfalse;
		team = multiplayerManager.getPlayersTeam(bot2 );
		if (team)
			info2 = team->getName().c_str();
		else
			return qfalse;
		if ((!info1) || (!info2))
			return qfalse;
		if (!strcmp(info1,info2))
			return qtrue;
	}
	return qfalse;
}

qboolean InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles)
{
	int i;
	float diff, angle;

	for (i = 0; i < 2; i++) {
		angle = AngleMod(viewangles[i]);
		angles[i] = AngleMod(angles[i]);
		diff = angles[i] - angle;
		if (angles[i] > angle) {
			if (diff > 180.0) diff -= 360.0;
		}
		else {
			if (diff < -180.0) diff += 360.0;
		}
		if (diff > 0) {
			if (diff > fov * 0.5) return qfalse;
		}
		else {
			if (diff < -fov * 0.5) return qfalse;
		}
	}
	return qtrue;
}

float BotEntityVisible(int viewer, vec3_t eye, vec3_t viewangles, float fov, int ent) {
	int i, contents_mask, passent, hitent, infog, inwater, otherinfog, pc;
	float squaredfogdist, waterfactor, vis, bestvis;
	bsp_trace_t trace;
	aas_entityinfo_t entinfo;
	vec3_t dir, entangles, start, end, middle;

	BotEntityInfo(ent, &entinfo);
	VectorAdd(entinfo.mins, entinfo.maxs, middle);
	VectorScale(middle, 0.5, middle);
	VectorAdd(entinfo.origin, middle, middle);
	VectorSubtract(middle, eye, dir);
	vectoangles(dir, entangles);
	if (!InFieldOfVision(viewangles, fov, entangles)) return 0;
	pc = gi.AAS_PointContents(eye);
	infog = (pc & CONTENTS_FOG);
	inwater = (pc & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER));
	bestvis = 0;
	for (i = 0; i < 3; i++) {
		contents_mask = CONTENTS_SOLID|CONTENTS_PLAYERCLIP;
		passent = viewer;
		hitent = ent;
		VectorCopy(eye, start);
		VectorCopy(middle, end);
		if (gi.AAS_PointContents(middle) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER)) {
			contents_mask |= (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
		}
		if (inwater) {
			if (!(contents_mask & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))) {
				passent = ent;
				hitent = viewer;
				VectorCopy(middle, start);
				VectorCopy(eye, end);
			}
			contents_mask ^= (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
		}
		BotAI_Trace(&trace, start, NULL, NULL, end, passent, contents_mask);
		waterfactor = 1.0;
		if (trace.contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER)) {
			if (1) {
				contents_mask &= ~(CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER);
				BotAI_Trace(&trace, trace.endpos, NULL, NULL, end, passent, contents_mask);
				waterfactor = 0.5;
			}
		}
		if (trace.fraction >= 1 || trace.ent == hitent) {
			otherinfog = (gi.AAS_PointContents(middle) & CONTENTS_FOG);
			if (infog && otherinfog) {
				VectorSubtract(trace.endpos, eye, dir);
				squaredfogdist = VectorLengthSquared(dir);
			}
			else if (infog) {
				VectorCopy(trace.endpos, start);
				BotAI_Trace(&trace, start, NULL, NULL, eye, viewer, CONTENTS_FOG);
				VectorSubtract(eye, trace.endpos, dir);
				squaredfogdist = VectorLengthSquared(dir);
			}
			else if (otherinfog) {
				VectorCopy(trace.endpos, end);
				BotAI_Trace(&trace, eye, NULL, NULL, end, viewer, CONTENTS_FOG);
				VectorSubtract(end, trace.endpos, dir);
				squaredfogdist = VectorLengthSquared(dir);
			}
			else {
				squaredfogdist = 0;
			}
			vis = 1 / ((squaredfogdist * 0.001) < 1 ? 1 : (squaredfogdist * 0.001));
			vis *= waterfactor;
			if (vis > bestvis) bestvis = vis;
			if (bestvis >= 0.95) return bestvis;
		}
		if (i == 0) middle[2] += entinfo.mins[2];
		else if (i == 1) middle[2] += entinfo.maxs[2] - entinfo.mins[2];
	}
	return bestvis;
}

int BotFindEnemy(bot_state_t *bs, int curenemy) {
	int i, healthdecrease;
	float f, alertness, easyfragger, vis;
	float squaredist, cursquaredist;
	aas_entityinfo_t entinfo, curenemyinfo;
	vec3_t dir, angles;

	alertness = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_ALERTNESS, 0, 1);
	easyfragger = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_EASY_FRAGGER, 0, 1);
	healthdecrease = bs->lasthealth > bs->inventory[INVENTORY_HEALTH];

	if (curenemy >= 0) {


		if ( BotSameTeam( bs, curenemy ) ) {
			bs->enemy = -1;
			return qtrue;
		}

		BotEntityInfo(curenemy, &curenemyinfo);

		if ( EntityCarriesFlag(&curenemyinfo) ) return qfalse;

		VectorSubtract(curenemyinfo.origin, bs->origin, dir);
		cursquaredist = VectorLengthSquared(dir);
	}
	else {
		cursquaredist = 0;
	}
#ifdef MISSIONPACK
	if (gametype == GT_OBELISK) {
		vec3_t target;
		bot_goal_t *goal;
		bsp_trace_t trace;

		if (BotTeam(bs) == TEAM_RED)
			goal = &blueobelisk;
		else
			goal = &redobelisk;
		VectorCopy(goal->origin, target);
		target[2] += 1;
		BotAI_Trace(&trace, bs->eye, NULL, NULL, target, bs->client, CONTENTS_SOLID);
		if (trace.fraction >= 1 || trace.ent == goal->entitynum) {
			if (goal->entitynum == bs->enemy) {
				return qfalse;
			}
			bs->enemy = goal->entitynum;
			bs->enemysight_time = FloatTime();
			bs->enemysuicide = qfalse;
			bs->enemydeath_time = 0;
			bs->enemyvisible_time = FloatTime();
			return qtrue;
		}
	}
#endif
	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {

		if (i == bs->client) continue;
		if (i == curenemy) continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid) continue;
		if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum) continue;
		if (EntityIsInvisible(&entinfo) && !EntityIsShooting(&entinfo)) {
			continue;
		}
		if (easyfragger < 0.5 && EntityIsChatting(&entinfo)) continue;
		if (lastteleport_time > FloatTime() - 3) {
			VectorSubtract(entinfo.origin, lastteleport_origin, dir);
			if (VectorLengthSquared(dir) < Square(70)) continue;
		}
		VectorSubtract(entinfo.origin, bs->origin, dir);
		squaredist = VectorLengthSquared(dir);
		if (!EntityCarriesFlag(&entinfo))
		{
			if (curenemy >= 0 && squaredist > cursquaredist) continue;
		} 
		if (squaredist > Square(900.0 + alertness * 4000.0)) continue;
		if (BotSameTeam(bs, i)) continue;
		if (curenemy < 0 && (healthdecrease || EntityIsShooting(&entinfo)))
			f = 360;
		else
			f = 90 + 90 - (90 - (squaredist > Square(810) ? Square(810) : squaredist) / (810 * 9));
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, f, i);
		if (vis <= 0) continue;
		if (curenemy < 0 && squaredist > Square(100) && !healthdecrease && !EntityIsShooting(&entinfo))
		{
			VectorSubtract(bs->origin, entinfo.origin, dir);
			vectoangles(dir, angles);
			if (!InFieldOfVision(entinfo.angles, 90, angles)) {
				BotUpdateBattleInventory(bs, i);
				if (BotWantsToRetreat(bs)) continue;
			}
		}
		bs->enemy = entinfo.number;
		if (curenemy >= 0) bs->enemysight_time = FloatTime() - 2;
		else bs->enemysight_time = FloatTime();
		bs->enemysuicide = qfalse;
		bs->enemydeath_time = 0;
		bs->enemyvisible_time = FloatTime();
		return qtrue;
	}
	return qfalse;
}

int BotTeamFlagCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (i == bs->client)
			continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid)
			continue;
		if (!EntityCarriesFlag(&entinfo))
			continue;
		if (!BotSameTeam(bs, i))
			continue;
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		return i;
	}
	return -1;
}

int BotTeamFlagCarrier(bot_state_t *bs) {
	int i;
	aas_entityinfo_t entinfo;

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (i == bs->client)
			continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid)
			continue;
		if (!EntityCarriesFlag(&entinfo))
			continue;
		if (!BotSameTeam(bs, i))
			continue;
		return i;
	}
	return -1;
}

int BotEnemyFlagCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (i == bs->client)
			continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid)
			continue;
		if (!EntityCarriesFlag(&entinfo))
			continue;
		if (BotSameTeam(bs, i))
			continue;
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		return i;
	}
	return -1;
}

void BotVisibleTeamMatesAndEnemies(bot_state_t *bs, int *teammates, int *enemies, float range) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;
	vec3_t dir;

	if (teammates)
		*teammates = 0;
	if (enemies)
		*enemies = 0;
	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (i == bs->client)
			continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid)
			continue;
		if (!EntityCarriesFlag(&entinfo))
			continue;
		VectorSubtract(entinfo.origin, bs->origin, dir);
		if (VectorLengthSquared(dir) > Square(range))
			continue;
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		if (BotSameTeam(bs, i)) {
			if (teammates)
				(*teammates)++;
		}
		else {
			if (enemies)
				(*enemies)++;
		}
	}
}

#ifdef MISSIONPACK
int BotTeamCubeCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (i == bs->client) continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid) continue;
		if (!EntityCarriesCubes(&entinfo)) continue;
		if (!BotSameTeam(bs, i)) continue;
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0) continue;
		return i;
	}
	return -1;
}

int BotEnemyCubeCarrierVisible(bot_state_t *bs) {
	int i;
	float vis;
	aas_entityinfo_t entinfo;

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if (i == bs->client)
			continue;
		BotEntityInfo(i, &entinfo);
		if (!entinfo.valid)
			continue;
		if (!EntityCarriesCubes(&entinfo)) continue;
		if (BotSameTeam(bs, i))
			continue;
		vis = BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, i);
		if (vis <= 0)
			continue;
		return i;
	}
	return -1;
}
#endif

void BotAimAtEnemy(bot_state_t *bs) {
	int i, enemyvisible;
	float dist, f, aim_skill, aim_accuracy, speed, reactiontime;
	vec3_t dir, bestorigin, end, start, groundtarget, cmdmove, enemyvelocity;
	vec3_t mins = {-4,-4,-4}, maxs = {4, 4, 4};
	weaponinfo_t wi;
	
	aas_entityinfo_t entinfo;
	bsp_trace_t trace;
	vec3_t target;

	if (bs->enemy < 0) {
		return;
	}
	BotEntityInfo(bs->enemy, &entinfo);
	if (bs->enemy >= maxclients->integer) {
		VectorCopy(entinfo.origin, target);
#ifdef MISSIONPACK
		if ( bs->enemy == redobelisk.entitynum ||
			bs->enemy == blueobelisk.entitynum ) {
			target[2] += 32;
		}
#endif
		VectorSubtract(target, bs->eye, dir);
		vectoangles(dir, bs->ideal_viewangles);
		VectorCopy(target, bs->aimtarget);
		return;
	}
	aim_skill = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_SKILL, 0, 1);
	aim_accuracy = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
	if (aim_skill > 0.95) {
		reactiontime = 0.5 * gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
		if (bs->enemysight_time > FloatTime() - reactiontime) return;
		if (bs->teleport_time > FloatTime() - reactiontime) return;
	}

	gi.BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
	if (aim_accuracy <= 0) aim_accuracy = 0.0001f;
	BotEntityInfo(bs->enemy, &entinfo);
	if (EntityIsInvisible(&entinfo)) {
		if (random() > 0.1) aim_accuracy *= 0.4f;
	}
	VectorSubtract(entinfo.origin, entinfo.lastvisorigin, enemyvelocity);
	VectorScale(enemyvelocity, 1 / entinfo.update_time, enemyvelocity);
	if (bs->enemyposition_time < FloatTime()) {
		bs->enemyposition_time = FloatTime() + 0.5;
		VectorCopy(enemyvelocity, bs->enemyvelocity);
		VectorCopy(entinfo.origin, bs->enemyorigin);
	}
	if (aim_skill < 0.9) {
		VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
		if (VectorLengthSquared(dir) > Square(48)) {
			if (DotProduct(bs->enemyvelocity, enemyvelocity) < 0) {
				aim_accuracy *= 0.7f;
			}
		}
	}
	enemyvisible = (int)BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360, bs->enemy);
	if (enemyvisible) {
		VectorCopy(entinfo.origin, bestorigin);
		bestorigin[2] += (entinfo.maxs[2] * 0.5); 
		VectorCopy(bs->origin, start);
		start[2] += bs->cur_ps.viewheight;
		start[2] += wi.offset[2];
		BotAI_Trace(&trace, start, mins, maxs, bestorigin, bs->entitynum, MASK_SHOT);
		if (trace.fraction <= 1 && trace.ent != entinfo.number) {
			bestorigin[2] += 16;
		}
		if (wi.speed) {
			VectorSubtract(bestorigin, bs->origin, dir);
			dist = VectorLength(dir);
			VectorSubtract(entinfo.origin, bs->enemyorigin, dir);
			if (!(dist > 100 && VectorLengthSquared(dir) < Square(32))) {
				if (aim_skill > 0.8) { 
					aas_clientmove_t move;
					vec3_t origin;

					VectorSubtract(entinfo.origin, bs->origin, dir);
					dist = VectorLength(dir);
					VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
					VectorScale(dir, 1 / entinfo.update_time, dir);
					VectorCopy(entinfo.origin, origin);
					origin[2] += 1;
					VectorClear(cmdmove);
					gi.AAS_PredictClientMovement(&move, bs->enemy, origin,
														PRESENCE_CROUCH, qfalse,
														dir, cmdmove, 0,
														(int)(dist * 10 / wi.speed), 0.1f, 0, 0, qfalse);
					VectorCopy(move.endpos, bestorigin);
				}
				else if (aim_skill > 0.4) {
					VectorSubtract(entinfo.origin, bs->origin, dir);
					dist = VectorLength(dir);
					VectorSubtract(entinfo.origin, entinfo.lastvisorigin, dir);
					dir[2] = 0;
					speed = VectorNormalize(dir) / entinfo.update_time;
					VectorMA(entinfo.origin, (dist / wi.speed) * speed, dir, bestorigin);
				}
			}
		}
		if (aim_skill > 0.6 && wi.proj.damagetype & DAMAGETYPE_RADIAL) {
			if (entinfo.origin[2] < bs->origin[2] + 16) {
				VectorCopy(entinfo.origin, end);
				end[2] -= 64;
				BotAI_Trace(&trace, entinfo.origin, NULL, NULL, end, entinfo.number, MASK_SHOT);
				VectorCopy(bestorigin, groundtarget);
				if (trace.startsolid) groundtarget[2] = entinfo.origin[2] - 16;
				else groundtarget[2] = trace.endpos[2] - 8;
				BotAI_Trace(&trace, start, NULL, NULL, groundtarget, bs->entitynum, MASK_SHOT);
				if (fabs(trace.endpos[2] - groundtarget[2]) < 50) {
					VectorSubtract(trace.endpos, groundtarget, dir);
					if (VectorLengthSquared(dir) < Square(60)) {
						VectorSubtract(trace.endpos, start, dir);
						if (VectorLengthSquared(dir) > Square(100)) {
							trace.endpos[2] += 1;
							BotAI_Trace(&trace, trace.endpos, NULL, NULL, entinfo.origin, entinfo.number, MASK_SHOT);
							if (trace.fraction >= 1) {
								VectorCopy(groundtarget, bestorigin);
							}
						}
					}
				}
			}
		}
		bestorigin[0] += 20 * crandom() * (1 - aim_accuracy);
		bestorigin[1] += 20 * crandom() * (1 - aim_accuracy);
		bestorigin[2] += 10 * crandom() * (1 - aim_accuracy);
	}
	else {
		VectorCopy(bs->lastenemyorigin, bestorigin);
		bestorigin[2] += 8;
		aim_accuracy = 1;
	}
	if (enemyvisible) {
		BotAI_Trace(&trace, bs->eye, NULL, NULL, bestorigin, bs->entitynum, MASK_SHOT);
		VectorCopy(trace.endpos, bs->aimtarget);
	}
	else {
		VectorCopy(bestorigin, bs->aimtarget);
	}
	VectorSubtract(bestorigin, bs->eye, dir);

		dist = VectorLength(dir);
		if (dist > 150) dist = 150;
		f = 0.6 + dist / 150 * 0.4;
		aim_accuracy *= f;
	if (aim_accuracy < 0.8) {
		VectorNormalize(dir);
		for (i = 0; i < 3; i++) dir[i] += 0.3 * crandom() * (1 - aim_accuracy);
	}
	vectoangles(dir, bs->ideal_viewangles);
	bs->ideal_viewangles[PITCH] += 6 * wi.vspread * crandom() * (1 - aim_accuracy);
	bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
	bs->ideal_viewangles[YAW] += 6 * wi.hspread * crandom() * (1 - aim_accuracy);
	bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
	if (bot_challenge.integer) {
		if (aim_accuracy > 0.9 && bs->enemysight_time < FloatTime() - 1) {
			if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
			VectorCopy(bs->ideal_viewangles, bs->viewangles);
			gi.EA_View(bs->client, bs->viewangles);
		}
	}
}

void BotCheckAttack(bot_state_t *bs) {
	float points, reactiontime, fov, firethrottle;
	int attackentity;
	bsp_trace_t bsptrace;
	vec3_t forward, right, start, end, dir, angles;
	weaponinfo_t wi;
	bsp_trace_t trace;
	aas_entityinfo_t entinfo;
	vec3_t mins = {-8, -8, -8}, maxs = {8, 8, 8};

	attackentity = bs->enemy;
	BotEntityInfo(attackentity, &entinfo);
	if (attackentity >= maxclients->integer) {
#ifdef MISSIONPACKBOTTODO
		if ( entinfo.number == redobelisk.entitynum ||
			entinfo.number == blueobelisk.entitynum ) {
			if ( g_entities[entinfo.number].activator &&
				g_entities[entinfo.number].activator->s.frame == 2 ) {
				return;
			}
		}
#endif
	}
	reactiontime = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_REACTIONTIME, 0, 1);
	if (bs->enemysight_time > FloatTime() - reactiontime) return;
	if (bs->teleport_time > FloatTime() - reactiontime) return;
	if (bs->firethrottlewait_time > FloatTime()) return;
	firethrottle = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_FIRETHROTTLE, 0, 1);
	if (bs->firethrottleshoot_time < FloatTime()) {
		if (random() > firethrottle) {
			gi.EA_ToggleFireState(bs->client);
			bs->firethrottlewait_time = FloatTime() + firethrottle;
			bs->firethrottleshoot_time = 0;
		}
		else {
			bs->firethrottleshoot_time = FloatTime() + 1 - firethrottle;
			bs->firethrottlewait_time = 0;
		}
	}
	VectorSubtract(bs->aimtarget, bs->eye, dir);
	if (VectorLengthSquared(dir) < Square(100))
		fov = 120;
	else
		fov = 50;
	vectoangles(dir, angles);
	if (!InFieldOfVision(bs->viewangles, fov, angles))
		return;
	BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, bs->aimtarget, bs->client, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
	if (bsptrace.fraction < 1 && bsptrace.ent != attackentity)
		return;

	gi.BotGetWeaponInfo(bs->ws, bs->weaponnum, &wi);
	VectorCopy(bs->origin, start);
	start[2] += bs->cur_ps.viewheight;
	AngleVectors(bs->viewangles, forward, right, NULL);
	start[0] += forward[0] * wi.offset[0] + right[0] * wi.offset[1];
	start[1] += forward[1] * wi.offset[0] + right[1] * wi.offset[1];
	start[2] += forward[2] * wi.offset[0] + right[2] * wi.offset[1] + wi.offset[2];
	VectorMA(start, 1000, forward, end);
	VectorMA(start, -12, forward, start);
	BotAI_Trace(&trace, start, mins, maxs, end, bs->entitynum, MASK_SHOT);
	if (trace.ent > 0 && trace.ent <= maxclients->integer) {
		if (trace.ent != attackentity) {
			if (BotSameTeam(bs, trace.ent))
				return;
		}
	}
	if (trace.ent != attackentity || attackentity >= maxclients->integer) {
		if (wi.proj.damagetype & DAMAGETYPE_RADIAL) {
			if (trace.fraction * 1000 < wi.proj.radius) {
				points = (wi.proj.damage - 0.5 * trace.fraction * 1000) * 0.5;
				if (points > 0) {
					return;
				}
			}
		}
	}
	if (wi.flags & WFL_FIRERELEASED) {
		if (bs->flags & BFL_ATTACKED) {
			gi.EA_Attack(bs->client, wi.primarydangerous, wi.altdangerous);
		}
	}
	else {
		gi.EA_Attack(bs->client, wi.primarydangerous, wi.altdangerous);
	}
	bs->flags ^= BFL_ATTACKED;
}

void BotMapScripts(bot_state_t *bs) {
	char info[1024];
	char mapname[128];
	int i, shootbutton;
	float aim_accuracy;
	aas_entityinfo_t entinfo;
	vec3_t dir;

	gi.SV_GetServerinfo(info, sizeof(info));

	Q_strncpyz(mapname, Info_ValueForKey( info, "mapname" ), sizeof(mapname)-1);
	mapname[sizeof(mapname)-1] = '\0';

	if (!Q_stricmp(mapname, "q3tourney6")) {
		vec3_t mins = {700, 204, 672}, maxs = {964, 468, 680};
		vec3_t buttonorg = {304, 352, 920};
		//NOTE: NEVER use the func_bobbing in q3tourney6
		bs->tfl &= ~TFL_FUNCBOB;
		if (bs->origin[0] > mins[0] && bs->origin[0] < maxs[0]) {
			if (bs->origin[1] > mins[1] && bs->origin[1] < maxs[1]) {
				if (bs->origin[2] < mins[2]) {
					return;
				}
			}
		}
		shootbutton = qfalse;
		for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {

			if (i == bs->client) continue;
			BotEntityInfo(i, &entinfo);
			if (!entinfo.valid) continue;
			if (EntityIsDead(&entinfo) || entinfo.number == bs->entitynum) continue;
			if (entinfo.origin[0] > mins[0] && entinfo.origin[0] < maxs[0]) {
				if (entinfo.origin[1] > mins[1] && entinfo.origin[1] < maxs[1]) {
					if (entinfo.origin[2] < mins[2]) {
						if (BotSameTeam(bs, i)) {
							shootbutton = qfalse;
							break;
						}
						else {
							shootbutton = qtrue;
						}
					}
				}
			}
		}
		if (shootbutton) {
			bs->flags |= BFL_IDEALVIEWSET;
			VectorSubtract(buttonorg, bs->eye, dir);
			vectoangles(dir, bs->ideal_viewangles);
			aim_accuracy = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_AIM_ACCURACY, 0, 1);
			bs->ideal_viewangles[PITCH] += 8 * crandom() * (1 - aim_accuracy);
			bs->ideal_viewangles[PITCH] = AngleMod(bs->ideal_viewangles[PITCH]);
			bs->ideal_viewangles[YAW] += 8 * crandom() * (1 - aim_accuracy);
			bs->ideal_viewangles[YAW] = AngleMod(bs->ideal_viewangles[YAW]);
			if (InFieldOfVision(bs->viewangles, 20, bs->ideal_viewangles)) {
				gi.EA_Attack(bs->client,0,0);
			}
		}
	}
	else if (!Q_stricmp(mapname, "mpq3tourney6")) {
		//NOTE: NEVER use the func_bobbing in mpq3tourney6
		bs->tfl &= ~TFL_FUNCBOB;
	}
}

static vec3_t VEC_UP		= {0, -1,  0};
static vec3_t MOVEDIR_UP	= {0,  0,  1};
static vec3_t VEC_DOWN		= {0, -2,  0};
static vec3_t MOVEDIR_DOWN	= {0,  0, -1};

void BotSetMovedir(vec3_t angles, vec3_t movedir) {
	if (VectorCompare(angles, VEC_UP)) {
		VectorCopy(MOVEDIR_UP, movedir);
	}
	else if (VectorCompare(angles, VEC_DOWN)) {
		VectorCopy(MOVEDIR_DOWN, movedir);
	}
	else {
		AngleVectors(angles, movedir, NULL, NULL);
	}
}

int BotModelMinsMaxs(int modelindex, int eType, int contents, vec3_t mins, vec3_t maxs) {
	gentity_t *ent;
	int i;

	ent = &g_entities[0];
	for (i = 0; i < globals.num_entities; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( eType && ent->s.eType != eType) {
			continue;
		}
		if ( contents && ent->contents != contents) {
			continue;
		}
		if (ent->s.modelindex == modelindex) {
			if (mins)
				VectorAdd(ent->currentOrigin, ent->mins, mins);
			if (maxs)
				VectorAdd(ent->currentOrigin, ent->maxs, maxs);
			return i;
		}
	}
	if (mins)
		VectorClear(mins);
	if (maxs)
		VectorClear(maxs);
	return 0;
}

int BotFuncButtonActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
	int i, areas[10], numareas, modelindex, entitynum;
	char model[128];
	float lip, dist, health, angle;
	vec3_t size, start, end, mins, maxs, angles, points[10];
	vec3_t movedir, origin, goalorigin, bboxmins, bboxmaxs;
	vec3_t extramins = {1, 1, 1}, extramaxs = {-1, -1, -1};
	bsp_trace_t bsptrace;

	activategoal->shoot = qfalse;
	VectorClear(activategoal->target);
	gi.AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
	if (!*model)
		return qfalse;
	modelindex = atoi(model+1);
	if (!modelindex)
		return qfalse;
	VectorClear(angles);
	entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
	gi.AAS_FloatForBSPEpairKey(bspent, "lip", &lip);
	if (!lip) lip = 4;
	gi.AAS_FloatForBSPEpairKey(bspent, "angle", &angle);
	VectorSet(angles, 0, angle, 0);
	BotSetMovedir(angles, movedir);
	VectorSubtract(maxs, mins, size);
	VectorAdd(mins, maxs, origin);
	VectorScale(origin, 0.5, origin);
	dist = fabs(movedir[0]) * size[0] + fabs(movedir[1]) * size[1] + fabs(movedir[2]) * size[2];
	dist *= 0.5;
	gi.AAS_FloatForBSPEpairKey(bspent, "health", &health);
	if (health) {
		VectorMA(origin, -dist, movedir, goalorigin);
		VectorCopy(goalorigin, activategoal->target);
		activategoal->shoot = qtrue;
		BotAI_Trace(&bsptrace, bs->eye, NULL, NULL, goalorigin, bs->entitynum, MASK_SHOT);
		if (bsptrace.fraction >= 1.0 || bsptrace.ent == entitynum) {
			activategoal->goal.entitynum = entitynum;  
			activategoal->goal.number = 0;
			activategoal->goal.flags = 0;
			VectorCopy(bs->origin, activategoal->goal.origin);
			activategoal->goal.areanum = bs->areanum;
			VectorSet(activategoal->goal.mins, -8, -8, -8);
			VectorSet(activategoal->goal.maxs, 8, 8, 8);
			return qtrue;
		}
		else {
			gi.AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
			for (i = 0; i < 3; i++) {
				if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
				else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
			}
			VectorMA(origin, -dist, movedir, goalorigin);
			VectorCopy(goalorigin, start);
			start[2] += 24;
			VectorCopy(start, end);
			end[2] -= 512;
			numareas = gi.AAS_TraceAreas(start, end, areas, points, 10);
			for (i = numareas-1; i >= 0; i--) {
				if (gi.AAS_AreaReachability(areas[i])) {
					break;
				}
			}
			if (i < 0) {
			}
			if (i >= 0) {
				VectorCopy(points[i], activategoal->goal.origin);
				activategoal->goal.areanum = areas[i];
				VectorSet(activategoal->goal.mins, 8, 8, 8);
				VectorSet(activategoal->goal.maxs, -8, -8, -8);
				for (i = 0; i < 3; i++)
				{
					if (movedir[i] < 0) activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
					else activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
				} 
				activategoal->goal.entitynum = entitynum;
				activategoal->goal.number = 0;
				activategoal->goal.flags = 0;
				return qtrue;
			}
		}
		return qfalse;
	}
	else {
		gi.AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
		for (i = 0; i < 3; i++) {
			if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
			else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
		}
		VectorMA(origin, -dist, movedir, goalorigin);
		VectorCopy(goalorigin, start);
		start[2] += 24;
		VectorCopy(start, end);
		end[2] -= 100;
		numareas = gi.AAS_TraceAreas(start, end, areas, NULL, 10);
		for (i = 0; i < numareas; i++) {
			if (gi.AAS_AreaReachability(areas[i])) {
				break;
			}
		}
		if (i < numareas) {
			VectorCopy(origin, activategoal->goal.origin);
			activategoal->goal.areanum = areas[i];
			VectorSubtract(mins, origin, activategoal->goal.mins);
			VectorSubtract(maxs, origin, activategoal->goal.maxs);
			for (i = 0; i < 3; i++)
			{
				if (movedir[i] < 0) activategoal->goal.maxs[i] += fabs(movedir[i]) * fabs(extramaxs[i]);
				else activategoal->goal.mins[i] += fabs(movedir[i]) * fabs(extramins[i]);
			} 
			activategoal->goal.entitynum = entitynum;
			activategoal->goal.number = 0;
			activategoal->goal.flags = 0;
			return qtrue;
		}
	}
	return qfalse;
}

int BotFuncDoorActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
	int modelindex, entitynum;
	char model[MAX_INFO_STRING];
	vec3_t mins, maxs, origin, angles;

	gi.AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
	if (!*model)
		return qfalse;
	modelindex = atoi(model+1);
	if (!modelindex)
		return qfalse;
	VectorClear(angles);
	entitynum = BotModelMinsMaxs(modelindex, ET_MOVER, 0, mins, maxs);
	VectorAdd(mins, maxs, origin);
	VectorScale(origin, 0.5, origin);
	VectorCopy(origin, activategoal->target);
	activategoal->shoot = qtrue;
	activategoal->goal.entitynum = entitynum;  
	activategoal->goal.number = 0;
	activategoal->goal.flags = 0;
	VectorCopy(bs->origin, activategoal->goal.origin);
	activategoal->goal.areanum = bs->areanum;
	VectorSet(activategoal->goal.mins, -8, -8, -8);
	VectorSet(activategoal->goal.maxs, 8, 8, 8);
	return qtrue;
}

int BotTriggerMultipleActivateGoal(bot_state_t *bs, int bspent, bot_activategoal_t *activategoal) {
	int i, areas[10], numareas, modelindex, entitynum;
	char model[128];
	vec3_t start, end, mins, maxs, angles;
	vec3_t origin, goalorigin;

	activategoal->shoot = qfalse;
	VectorClear(activategoal->target);
	gi.AAS_ValueForBSPEpairKey(bspent, "model", model, sizeof(model));
	if (!*model)
		return qfalse;
	modelindex = atoi(model+1);
	if (!modelindex)
		return qfalse;
	VectorClear(angles);
	entitynum = BotModelMinsMaxs(modelindex, 0, CONTENTS_MONSTERCLIP, mins, maxs); 
	VectorAdd(mins, maxs, origin);
	VectorScale(origin, 0.5, origin);
	VectorCopy(origin, goalorigin);
	VectorCopy(goalorigin, start);
	start[2] += 24;
	VectorCopy(start, end);
	end[2] -= 100;
	numareas = gi.AAS_TraceAreas(start, end, areas, NULL, 10);
	for (i = 0; i < numareas; i++) {
		if (gi.AAS_AreaReachability(areas[i])) {
			break;
		}
	}
	if (i < numareas) {
		VectorCopy(origin, activategoal->goal.origin);
		activategoal->goal.areanum = areas[i];
		VectorSubtract(mins, origin, activategoal->goal.mins);
		VectorSubtract(maxs, origin, activategoal->goal.maxs);
		activategoal->goal.entitynum = entitynum;
		activategoal->goal.number = 0;
		activategoal->goal.flags = 0;
		return qtrue;
	}
	return qfalse;
}

int BotPopFromActivateGoalStack(bot_state_t *bs) {
	if (!bs->activatestack)
		return qfalse;
	BotEnableActivateGoalAreas(bs->activatestack, qtrue);
	bs->activatestack->inuse = qfalse;
	bs->activatestack->justused_time = FloatTime();
	bs->activatestack = bs->activatestack->next;
	return qtrue;
}

int BotPushOntoActivateGoalStack(bot_state_t *bs, bot_activategoal_t *activategoal) {
	int i, best;
	float besttime;

	best = -1;
	besttime = FloatTime() + 9999;
	for (i = 0; i < MAX_ACTIVATESTACK; i++) {
		if (!bs->activategoalheap[i].inuse) {
			if (bs->activategoalheap[i].justused_time < besttime) {
				besttime = bs->activategoalheap[i].justused_time;
				best = i;
			}
		}
	}
	if (best != -1) {
		memcpy(&bs->activategoalheap[best], activategoal, sizeof(bot_activategoal_t));
		bs->activategoalheap[best].inuse = qtrue;
		bs->activategoalheap[best].next = bs->activatestack;
		bs->activatestack = &bs->activategoalheap[best];
		return qtrue;
	}
	return qfalse;
}

void BotClearActivateGoalStack(bot_state_t *bs) {
	while(bs->activatestack)
		BotPopFromActivateGoalStack(bs);
}

void BotEnableActivateGoalAreas(bot_activategoal_t *activategoal, int enable) {
	int i;

	if (activategoal->areasdisabled == !enable)
		return;
	for (i = 0; i < activategoal->numareas; i++)
		gi.AAS_EnableRoutingArea( activategoal->areas[i], enable );
	activategoal->areasdisabled = !enable;
}

int BotIsGoingToActivateEntity(bot_state_t *bs, int entitynum) {
	bot_activategoal_t *a;
	int i;

	for (a = bs->activatestack; a; a = a->next) {
		if (a->time < FloatTime())
			continue;
		if (a->goal.entitynum == entitynum)
			return qtrue;
	}
	for (i = 0; i < MAX_ACTIVATESTACK; i++) {
		if (bs->activategoalheap[i].inuse)
			continue;
		if (bs->activategoalheap[i].goal.entitynum == entitynum) {
			if (bs->activategoalheap[i].justused_time > FloatTime() - 2)
				return qtrue;
		}
	}
	return qfalse;
}


int BotGetActivateGoal(bot_state_t *bs, int entitynum, bot_activategoal_t *activategoal) {
	int i, ent, cur_entities[10], spawnflags, modelindex, areas[MAX_ACTIVATEAREAS*2], numareas, t;
	char model[MAX_INFO_STRING], tmpmodel[128];
	char target[128], classname[128];
	float health;
	char targetname[10][128];
	aas_entityinfo_t entinfo;
	aas_areainfo_t areainfo;
	vec3_t origin, angles, absmins, absmaxs;

	memset(activategoal, 0, sizeof(bot_activategoal_t));
	BotEntityInfo(entitynum, &entinfo);
	Com_sprintf(model, sizeof( model ), "*%d", entinfo.modelindex);
	for (ent = gi.AAS_NextBSPEntity(0); ent; ent = gi.AAS_NextBSPEntity(ent)) {
		if (!gi.AAS_ValueForBSPEpairKey(ent, "model", tmpmodel, sizeof(tmpmodel))) continue;
		if (!strcmp(model, tmpmodel)) break;
	}
	if (!ent) {
		BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity found with model %s\n", model);
		return 0;
	}
	gi.AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname));
	if (!classname) {
		BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model %s has no classname\n", model);
		return 0;
	}
	if (!strcmp(classname, "func_door")) {
		if (gi.AAS_FloatForBSPEpairKey(ent, "health", &health)) {
			if (health) {
				BotFuncDoorActivateGoal(bs, ent, activategoal);
				return ent;
			}
		}
		gi.AAS_IntForBSPEpairKey(ent, "spawnflags", &spawnflags);
		if ( spawnflags & 1 )
			return 0;
		if (!gi.AAS_VectorForBSPEpairKey(ent, "origin", origin)) {
			VectorClear(origin);
		}
		if (!VectorCompare(origin, entinfo.origin))
			return 0;
		gi.AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model));
		if (*model) {
			modelindex = atoi(model+1);
			if (modelindex) {
				VectorClear(angles);
				BotModelMinsMaxs(modelindex, ET_MOVER, 0, absmins, absmaxs);
				numareas = gi.AAS_BBoxAreas(absmins, absmaxs, areas, MAX_ACTIVATEAREAS*2);
				for (i = 0; i < numareas; i++) {
					if (activategoal->numareas >= MAX_ACTIVATEAREAS)
						break;
					if ( !gi.AAS_AreaReachability(areas[i]) ) {
						continue;
					}
					gi.AAS_AreaInfo(areas[i], &areainfo);
					if (areainfo.contents & AREACONTENTS_MOVER) {
						activategoal->areas[activategoal->numareas++] = areas[i];
					}
				}
				for (i = 0; i < numareas; i++) {
					if (activategoal->numareas >= MAX_ACTIVATEAREAS)
						break;
					if ( gi.AAS_AreaReachability(areas[i]) ) {
						continue;
					}
					gi.AAS_AreaInfo(areas[i], &areainfo);
					if (areainfo.contents & AREACONTENTS_MOVER) {
						activategoal->areas[activategoal->numareas++] = areas[i];
					}
				}
			}
		}
	}
	if (!strcmp(classname, "func_button")) {
		return 0;
	}
	if (!gi.AAS_ValueForBSPEpairKey(ent, "targetname", targetname[0], sizeof(targetname[0]))) {
		if (bot_developer.integer) {
			BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with model \"%s\" has no targetname\n", model);
		}
		return 0;
	}
	cur_entities[0] = gi.AAS_NextBSPEntity(0);
	for (i = 0; i >= 0 && i < 10;) {
		for (ent = cur_entities[i]; ent; ent = gi.AAS_NextBSPEntity(ent)) {
			if (!gi.AAS_ValueForBSPEpairKey(ent, "target", target, sizeof(target))) continue;
			if (!strcmp(targetname[i], target)) {
				cur_entities[i] = gi.AAS_NextBSPEntity(ent);
				break;
			}
		}
		if (!ent) {
			if (bot_developer.integer) {
				BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no entity with target \"%s\"\n", targetname[i]);
			}
			i--;
			continue;
		}
		if (!gi.AAS_ValueForBSPEpairKey(ent, "classname", classname, sizeof(classname))) {
			if (bot_developer.integer) {
				BotAI_Print(PRT_ERROR, "BotGetActivateGoal: entity with target \"%s\" has no classname\n", targetname[i]);
			}
			continue;
		}
		if (!strcmp(classname, "func_button")) {
			if (!BotFuncButtonActivateGoal(bs, ent, activategoal))
				continue;
			if ( bs->activatestack && bs->activatestack->inuse &&
				 bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
				 bs->activatestack->time > FloatTime() &&
				 bs->activatestack->start_time < FloatTime() - 2)
				continue;
			if ( gi.AAS_AreaReachability(bs->areanum) ) {
				BotEnableActivateGoalAreas( activategoal, qfalse );
				t = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
				if (!t) {
					continue;
				}
				activategoal->time = FloatTime() + t * 0.01 + 5;
			}
			return ent;
		}
		else if (!strcmp(classname, "trigger_multiple")) {
			if (!BotTriggerMultipleActivateGoal(bs, ent, activategoal))
				continue;
			if ( bs->activatestack && bs->activatestack->inuse &&
				 bs->activatestack->goal.entitynum == activategoal->goal.entitynum &&
				 bs->activatestack->time > FloatTime() &&
				 bs->activatestack->start_time < FloatTime() - 2)
				continue;
			if ( gi.AAS_AreaReachability(bs->areanum) ) {
				BotEnableActivateGoalAreas( activategoal, qfalse );
				t = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, activategoal->goal.areanum, bs->tfl);
				if (!t) {
					continue;
				}
				activategoal->time = FloatTime() + t * 0.01 + 5;
			}
			return ent;
		}
		else if (!strcmp(classname, "func_timer")) {
			continue;
		}
		else if (!strcmp(classname, "target_relay") || !strcmp(classname, "target_delay")) {
			if (gi.AAS_ValueForBSPEpairKey(ent, "targetname", targetname[i+1], sizeof(targetname[0]))) {
				i++;
				cur_entities[i] = gi.AAS_NextBSPEntity(0);
			}
		}
	}
#ifdef OBSTACLEDEBUG
	BotAI_Print(PRT_ERROR, "BotGetActivateGoal: no valid activator for entity with target \"%s\"\n", targetname[0]);
#endif
	return 0;
}

int BotGoForActivateGoal(bot_state_t *bs, bot_activategoal_t *activategoal) {
	aas_entityinfo_t activateinfo;

	activategoal->inuse = qtrue;
	if (!activategoal->time)
		activategoal->time = FloatTime() + 10;
	activategoal->start_time = FloatTime();
	BotEntityInfo(activategoal->goal.entitynum, &activateinfo);
	VectorCopy(activateinfo.origin, activategoal->origin);
	if (BotPushOntoActivateGoalStack(bs, activategoal)) {
		AIEnter_Seek_ActivateEntity(bs, "BotGoForActivateGoal");
		return qtrue;
	}
	else {
		BotEnableActivateGoalAreas(activategoal, qtrue);
		return qfalse;
	}
}

void BotPrintActivateGoalInfo(bot_state_t *bs, bot_activategoal_t *activategoal, int bspent) {
	char netname[MAX_NETNAME];
	char classname[128];
	char buf[128];

	ClientName(bs->client, netname, sizeof(netname));
	gi.AAS_ValueForBSPEpairKey(bspent, "classname", classname, sizeof(classname));
	if (activategoal->shoot) {
		Com_sprintf(buf, sizeof(buf), "%s: I have to shoot at a %s from %1.1f %1.1f %1.1f in area %d\n",
						netname, classname,
						activategoal->goal.origin[0],
						activategoal->goal.origin[1],
						activategoal->goal.origin[2],
						activategoal->goal.areanum);
	}
	else {
		Com_sprintf(buf, sizeof(buf), "%s: I have to activate a %s at %1.1f %1.1f %1.1f in area %d\n",
						netname, classname,
						activategoal->goal.origin[0],
						activategoal->goal.origin[1],
						activategoal->goal.origin[2],
						activategoal->goal.areanum);
	}
	gi.EA_Say(bs->client, buf);
}

void BotRandomMove(bot_state_t *bs, bot_moveresult_t *moveresult) {
	vec3_t dir, angles;

	angles[0] = 0;
	angles[1] = random() * 360;
	angles[2] = 0;
	AngleVectors(angles, dir, NULL, NULL);

	gi.BotMoveInDirection(bs->ms, dir, sv_maxspeed->value, MOVE_WALK);

	moveresult->failure = qfalse;
	VectorCopy(dir, moveresult->movedir);
}

void BotAIBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate) {
	int movetype, bspent;
	vec3_t hordir, start, end, mins, maxs, sideward, angles, up = {0, 0, 1};
	aas_entityinfo_t entinfo;
	bot_activategoal_t activategoal;

	if (!moveresult->blocked) {
		bs->notblocked_time = FloatTime();
		return;
	}
	if ( moveresult->type == RESULTTYPE_INSOLIDAREA ) {
		BotRandomMove(bs, moveresult);
		return;
	}
	BotEntityInfo(moveresult->blockentity, &entinfo);
#ifdef OBSTACLEDEBUG
	ClientName(bs->client, netname, sizeof(netname));
	BotAI_Print(PRT_MESSAGE, "%s: I'm blocked by model %d\n", netname, entinfo.modelindex);
#endif 
	if (activate && entinfo.modelindex > 0 && entinfo.modelindex <= max_bspmodelindex) {
		bspent = BotGetActivateGoal(bs, entinfo.number, &activategoal);
		if (bspent) {
			if (bs->activatestack && !bs->activatestack->inuse)
				bs->activatestack = NULL;
			if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
				BotGoForActivateGoal(bs, &activategoal);
			}
			if (!(moveresult->flags & MOVERESULT_ONTOPOFOBSTACLE) &&
				gi.AAS_AreaReachability(bs->areanum))
				return;
		}
		else {
			BotEnableActivateGoalAreas(&activategoal, qtrue);
		}
	}
	hordir[0] = moveresult->movedir[0];
	hordir[1] = moveresult->movedir[1];
	hordir[2] = 0;
	if (VectorNormalize(hordir) < 0.1) {
		VectorSet(angles, 0, 360 * random(), 0);
		AngleVectors(angles, hordir, NULL, NULL);
	}
	movetype = MOVE_WALK;
	VectorCopy(bs->origin, start);
	start[2] += 18;
	VectorMA(start, 5, hordir, end);
	VectorSet(mins, -22, -22, 0);
	VectorSet(maxs, 22, 22, 50);
	CrossProduct(hordir, up, sideward);
	if (bs->flags & BFL_AVOIDRIGHT) VectorNegate(sideward, sideward);
	if (movetype != MOVE_CROUCH || !gi.BotMoveInDirection(bs->ms, hordir, sv_maxspeed->value, movetype)) {
		if (!gi.BotMoveInDirection(bs->ms, sideward, sv_maxspeed->value, movetype)) {
			bs->flags ^= BFL_AVOIDRIGHT;
			VectorMA(sideward, -1, hordir, sideward);
			gi.BotMoveInDirection(bs->ms, sideward, sv_maxspeed->value, movetype);
		}
	}
	if (bs->notblocked_time < FloatTime() - 0.4) {
		if (bs->ainode == AINode_Seek_NBG) bs->nbg_time = 0;
		else if (bs->ainode == AINode_Seek_LTG) bs->ltg_time = 0;
	}
}

int BotAIPredictObstacles(bot_state_t *bs, bot_goal_t *goal) {
	int modelnum, entitynum, bspent;
	bot_activategoal_t activategoal;
	aas_predictroute_t route;

	if (!bot_predictobstacles.integer)
		return qfalse;

	if (bs->predictobstacles_goalareanum == goal->areanum &&
		bs->predictobstacles_time > FloatTime() - 6) {
		return qfalse;
	}
	bs->predictobstacles_goalareanum = goal->areanum;
	bs->predictobstacles_time = FloatTime();

	gi.AAS_PredictRoute(&route, bs->areanum, bs->origin,
							goal->areanum, bs->tfl, 100, 1000,
							RSE_USETRAVELTYPE|RSE_ENTERCONTENTS,
							AREACONTENTS_MOVER, TFL_BRIDGE, 0);
	if (route.stopevent & RSE_ENTERCONTENTS) {
		if (route.endcontents & AREACONTENTS_MOVER) {
			modelnum = (route.endcontents & AREACONTENTS_MODELNUM) >> AREACONTENTS_MODELNUMSHIFT;
			if (modelnum) {
				entitynum = BotModelMinsMaxs(modelnum, ET_MOVER, 0, NULL, NULL);
				if (entitynum) {
					bspent = BotGetActivateGoal(bs, entitynum, &activategoal);
					if (bspent) {
						if (bs->activatestack && !bs->activatestack->inuse)
							bs->activatestack = NULL;
						if (!BotIsGoingToActivateEntity(bs, activategoal.goal.entitynum)) {
							BotGoForActivateGoal(bs, &activategoal);
							return qtrue;
						}
						else {
							BotEnableActivateGoalAreas(&activategoal, qtrue);
						}
					}
				}
			}
		}
	}
	else if (route.stopevent & RSE_USETRAVELTYPE) {
		if (route.endtravelflags & TFL_BRIDGE) {
		}
	}
	return qfalse;
}

void BotCheckConsoleMessages(bot_state_t *bs) {
	char botname[MAX_NETNAME], message[MAX_MESSAGE_SIZE], netname[MAX_NETNAME], *ptr;
	float chat_reply;
	int context, handle;
	bot_consolemessage_t m;
	bot_match_t match;

	ClientName(bs->client, botname, sizeof(botname));
	while((handle = gi.BotNextConsoleMessage(bs->cs, &m)) != 0) {
		if (gi.BotNumConsoleMessages(bs->cs) < 10) {
			if (m.type == CMS_CHAT && m.time > FloatTime() - (1 + random())) break;
		}
		ptr = m.message;
		if (m.type == CMS_CHAT) {
			if (gi.BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
				ptr = m.message + match.variables[MESSAGE].offset;
			}
		}
		gi.UnifyWhiteSpaces(ptr);
		context = BotSynonymContext(bs);
		gi.BotReplaceSynonyms(ptr, context);
		if (!BotMatchMessage(bs, m.message)) {
			if (m.type == CMS_CHAT && !bot_nochat.integer) {
				if (!gi.BotFindMatch(m.message, &match, MTCONTEXT_REPLYCHAT)) {
					gi.BotRemoveConsoleMessage(bs->cs, handle);
					continue;
				}
				if (match.subtype & ST_TEAM) {
					gi.BotRemoveConsoleMessage(bs->cs, handle);
					continue;
				}
				gi.BotMatchVariable(&match, NETNAME, netname, sizeof(netname));
				gi.BotMatchVariable(&match, MESSAGE, message, sizeof(message));
				if (bs->client == ClientFromName(netname+1)) { 
					gi.BotRemoveConsoleMessage(bs->cs, handle);
					continue;
				}
				gi.UnifyWhiteSpaces(message);
				gi.Cvar_Update(&bot_testrchat);
				if (bot_testrchat.integer) {
					gi.BotLibVarSet("bot_testrchat", "1");
					if (gi.BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
															NULL, NULL,
															NULL, NULL,
															NULL, NULL,
															botname, netname)) {
						BotAI_Print(PRT_MESSAGE, "------------------------\n");
					}
					else {
						BotAI_Print(PRT_MESSAGE, "**** no valid reply ****\n");
					}
				}
				else if (bs->ainode != AINode_Stand && BotValidChatPosition(bs) && !TeamPlayIsOn()) {
					chat_reply = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_CHAT_REPLY, 0, 1);
					if (random() < 1.5 / (NumBots()+1) && random() < chat_reply) {
						if (gi.BotReplyChat(bs->cs, message, context, CONTEXT_REPLY,
																NULL, NULL,
																NULL, NULL,
																NULL, NULL,
																botname, netname)) {
							gi.BotRemoveConsoleMessage(bs->cs, handle);
							bs->stand_time = FloatTime() + BotChatTime(bs);
							AIEnter_Stand(bs, "BotCheckConsoleMessages: reply chat");
							break;
						}
					}
				}
			}
		}
		gi.BotRemoveConsoleMessage(bs->cs, handle);
	}
}

void BotCheckForGrenades(bot_state_t *bs, entityState_t *state) {
	if (state->eType != ET_MISSILE)
		return;
	gi.BotAddAvoidSpot(bs->ms, state->origin, 160, AVOID_ALWAYS);
}

#ifdef MISSIONPACKBOTTODO
void BotCheckForProxMines(bot_state_t *bs, entityState_t *state) {
	if (state->eType != ET_MISSILE || state->weapon != WP_PROX_LAUNCHER)
		return;
	if (state->generic1 == BotTeam(bs))
		return;
	if (!(bs->inventory[INVENTORY_PLASMAGUN] > 0 && bs->inventory[INVENTORY_CELLS] > 0) &&
		!(bs->inventory[INVENTORY_ROCKETLAUNCHER] > 0 && bs->inventory[INVENTORY_ROCKETS] > 0) &&
		!(bs->inventory[INVENTORY_BFG10K] > 0 && bs->inventory[INVENTORY_BFGAMMO] > 0) ) {
		return;
	}
	gi.BotAddAvoidSpot(bs->ms, state->pos.trBase, 160, AVOID_ALWAYS);
	if (bs->numproxmines >= MAX_PROXMINES)
		return;
	bs->proxmines[bs->numproxmines] = state->number;
	bs->numproxmines++;
}

void BotCheckForKamikazeBody(bot_state_t *bs, entityState_t *state) {
	if (!(state->eFlags & EF_KAMIKAZE))
		return;
	if (!(state->eFlags & EF_DEAD))
		return;
	bs->kamikazebody = state->number;
}
#endif

void BotCheckEvents(bot_state_t *bs, entityState_t *state) {
}

void BotCheckSnapshot(bot_state_t *bs) {
	int ent;
	entityState_t state;

	gi.BotAddAvoidSpot(bs->ms, vec3_origin, 0, AVOID_CLEAR);
	bs->kamikazebody = 0;
	bs->numproxmines = 0;
	ent = 0;
	while( ( ent = BotAI_GetSnapshotEntity( bs->client, ent, &state ) ) != -1 ) {
		BotCheckEvents(bs, &state);
		BotCheckForGrenades(bs, &state);
#ifdef MISSIONPACKBOTTODO
		BotCheckForProxMines(bs, &state);
		BotCheckForKamikazeBody(bs, &state);
#endif
	}
	BotAI_GetEntityState(bs->client, &state);
	BotCheckEvents(bs, &state);
}

void BotCheckAir(bot_state_t *bs) {
	if (bs->inventory[INVENTORY_ENVIRONMENTSUIT] <= 0) {
		if (gi.AAS_PointContents(bs->eye) & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA)) {
			return;
		}
	}
	bs->lastair_time = FloatTime();
}

bot_goal_t *BotAlternateRoute(bot_state_t *bs, bot_goal_t *goal) {
	int t;

	if (bs->altroutegoal.areanum) {
		if (bs->reachedaltroutegoal_time)
			return goal;
		t = gi.AAS_AreaTravelTimeToGoalArea(bs->areanum, bs->origin, bs->altroutegoal.areanum, bs->tfl);
		if (t && t < 20) {
			bs->reachedaltroutegoal_time = FloatTime();
		}
		memcpy(goal, &bs->altroutegoal, sizeof(bot_goal_t));
		return &bs->altroutegoal;
	}
	return goal;
}

int BotGetAlternateRouteGoal(bot_state_t *bs, int base) {
	aas_altroutegoal_t *altroutegoals;
	bot_goal_t *goal;
	int numaltroutegoals, rnd;

	if (base == TEAM_RED) {
		altroutegoals = red_altroutegoals;
		numaltroutegoals = red_numaltroutegoals;
	}
	else {
		altroutegoals = blue_altroutegoals;
		numaltroutegoals = blue_numaltroutegoals;
	}
	if (!numaltroutegoals)
		return qfalse;
	rnd = (int)((float) random() * (float)numaltroutegoals);
	if (rnd >= numaltroutegoals)
		rnd = numaltroutegoals-1;
	goal = &bs->altroutegoal;
	goal->areanum = altroutegoals[rnd].areanum;
	VectorCopy(altroutegoals[rnd].origin, goal->origin);
	VectorSet(goal->mins, -8, -8, -8);
	VectorSet(goal->maxs, 8, 8, 8);
	goal->entitynum = 0;
	goal->iteminfo = 0;
	goal->number = 0;
	goal->flags = 0;
	bs->reachedaltroutegoal_time = 0;
	return qtrue;
}

void BotSetupAlternativeRouteGoals(void) {

	if (altroutegoals_setup)
		return;
#ifdef MISSIONPACK
	if (gametype == GT_CTF) {
		if (gi.BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
			BotAI_Print(PRT_WARNING, "no alt routes without Neutral Flag\n");
		if (ctf_neutralflag.areanum) {
			red_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
										ctf_neutralflag.origin, ctf_neutralflag.areanum,
										ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
										red_altroutegoals, MAX_ALTROUTEGOALS,
										ALTROUTEGOAL_CLUSTERPORTALS|
										ALTROUTEGOAL_VIEWPORTALS);
			blue_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
										ctf_neutralflag.origin, ctf_neutralflag.areanum,
										ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
										blue_altroutegoals, MAX_ALTROUTEGOALS,
										ALTROUTEGOAL_CLUSTERPORTALS|
										ALTROUTEGOAL_VIEWPORTALS);
		}
	}
	else if (gametype == GT_1FCTF) {
		red_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
									ctf_neutralflag.origin, ctf_neutralflag.areanum,
									ctf_redflag.origin, ctf_redflag.areanum, TFL_DEFAULT,
									red_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
		blue_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
									ctf_neutralflag.origin, ctf_neutralflag.areanum,
									ctf_blueflag.origin, ctf_blueflag.areanum, TFL_DEFAULT,
									blue_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
	}
	else if (gametype == GT_OBELISK) {
		if (gi.BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without neutral obelisk\n");
		red_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
									red_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
		blue_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
									blue_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
	}
	else if (gametype == GT_HARVESTER) {
		red_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									redobelisk.origin, redobelisk.areanum, TFL_DEFAULT,
									red_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
		blue_numaltroutegoals = gi.AAS_AlternativeRouteGoals(
									neutralobelisk.origin, neutralobelisk.areanum,
									blueobelisk.origin, blueobelisk.areanum, TFL_DEFAULT,
									blue_altroutegoals, MAX_ALTROUTEGOALS,
									ALTROUTEGOAL_CLUSTERPORTALS|
									ALTROUTEGOAL_VIEWPORTALS);
	}
#endif
	altroutegoals_setup = qtrue;
}

void BotDeathmatchAI(bot_state_t *bs, float thinktime) {
	char gender[144], name[144], buf[144];
	char userinfo[MAX_INFO_STRING];
	int i;
	int r202_t0 = R202_BotAITick();
	int r202_t, r202_inv, r202_team, r202_node = 0;

	if (bs->setupcount > 0) {
		bs->setupcount--;
		if (bs->setupcount > 0) return;
		gi.Characteristic_String(bs->character, CHARACTERISTIC_GENDER, gender, sizeof(gender));
		gi.getUserinfo(bs->client, userinfo, sizeof(userinfo));
		Info_SetValueForKey(userinfo, "sex", gender);
		gi.setUserinfo(bs->client, userinfo);
		if ( !bs->map_restart && g_gametype->integer != GT_TOURNAMENT ) {
			Com_sprintf(buf, sizeof(buf), "team %s", bs->settings.team);
			gi.EA_Command(bs->client, buf);
		}
		if (gender[0] == 'm') gi.BotSetChatGender(bs->cs, CHAT_GENDERMALE);
		else if (gender[0] == 'f')  gi.BotSetChatGender(bs->cs, CHAT_GENDERFEMALE);
		else  gi.BotSetChatGender(bs->cs, CHAT_GENDERLESS);
		ClientName(bs->client, name, sizeof(name));
		gi.BotSetChatName(bs->cs, name, bs->client);
		bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];
		bs->setupcount = 0;
		BotSetupAlternativeRouteGoals();
	}
	bs->flags &= ~BFL_IDEALVIEWSET;
	r202_t = R202_BotAITick();
	if (!BotIntermission(bs)) {
		BotSetTeleportTime(bs);
		BotUpdateInventory(bs);
		BotCheckSnapshot(bs);
		BotCheckAir(bs);
	}
	BotCheckConsoleMessages(bs);
	R202_BotAIAdd( R202_BAIZ_dminv, R202_BotAITock( r202_t ) );
	r202_inv = R202_BotAITock( r202_t );
	r202_t = R202_BotAITick();
	if (!BotIntermission(bs) && !BotIsObserver(bs)) {
		BotTeamAI(bs);
	}
	R202_BotAIAdd( R202_BAIZ_dmteam, R202_BotAITock( r202_t ) );
	r202_team = R202_BotAITock( r202_t );
	if (!bs->ainode) {
		AIEnter_Seek_LTG(bs, "BotDeathmatchAI: no ai node");
	}
	if (!bs->entergamechat && bs->entergame_time > FloatTime() - 8) {
		if (BotChat_EnterGame(bs)) {
			bs->stand_time = FloatTime() + BotChatTime(bs);
			AIEnter_Stand(bs, "BotDeathmatchAI: chat enter game");
		}
		bs->entergamechat = qtrue;
	}
	BotResetNodeSwitches();
	r202_t = R202_BotAITick();
	for (i = 0; i < MAX_NODESWITCHES; i++) {
		qboolean r203_done;

		R203_BotNodeBegin(bs);
		r203_done = bs->ainode(bs) ? qtrue : qfalse;
		R203_BotNodeEnd();
		if (r203_done) break;
	}
	if (i >= MAX_NODESWITCHES) {
		R203_BotNodeSat();	 
	}
	r202_node = R202_BotAITock( r202_t );
	R202_BotAIAdd( R202_BAIZ_dmnode, r202_node );
	if (!bs->inuse) return;
	if (i >= MAX_NODESWITCHES) {
		gi.BotDumpGoalStack(bs->gs);
		gi.BotDumpAvoidGoals(bs->gs);
		BotDumpNodeSwitches(bs);
		ClientName(bs->client, name, sizeof(name));
		BotAI_Print(PRT_ERROR, "%s at %1.1f switched more than %d AI nodes\n", name, FloatTime(), MAX_NODESWITCHES);
	}
	bs->lastframe_health = bs->inventory[INVENTORY_HEALTH];

	{
		int r202_oth = ( R202_BotAITock( r202_t0 ) ) - ( r202_inv + r202_team + r202_node );

		if ( r202_oth < 0 ) {
			r202_oth = 0;
		}
		R202_BotAIAdd( R202_BAIZ_dmoth, r202_oth );
	}
}

int G_ModelIndex( const char *name );
void BotSetEntityNumForGoalWithModel(bot_goal_t *goal, int eType, char *modelname) {
	gentity_t *ent;
	int i, modelindex;
	vec3_t dir;

	modelindex = G_ModelIndex( modelname );
	ent = &g_entities[0];
	for (i = 0; i < globals.num_entities; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( eType && ent->s.eType != eType) {
			continue;
		}
		if (ent->s.modelindex != modelindex) {
			continue;
		}
		VectorSubtract(goal->origin, ent->s.origin, dir);
		if (VectorLengthSquared(dir) < Square(10)) {
			goal->entitynum = i;
			return;
		}
	}
}

void BotSetEntityNumForGoal(bot_goal_t *goal, char *classname) {
	gentity_t *ent;
	int i;
	vec3_t dir;

	ent = &g_entities[0];
	for (i = 0; i < globals.max_entities; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}
		if ( Q_stricmp(ent->entname, classname) ) { 
			continue;
		}
		VectorSubtract(goal->origin, ent->s.origin, dir);
		if (VectorLengthSquared(dir) < Square(10)) {
			goal->entitynum = i;
			return;
		}
	}
}

int BotGoalForBSPEntity( char *classname, bot_goal_t *goal ) {
	char value[MAX_INFO_STRING];
	vec3_t origin, start, end;
	int ent, numareas, areas[10];

	memset(goal, 0, sizeof(bot_goal_t));
	for (ent = gi.AAS_NextBSPEntity(0); ent; ent = gi.AAS_NextBSPEntity(ent)) {
		if (!gi.AAS_ValueForBSPEpairKey(ent, "classname", value, sizeof(value)))
			continue;
		if (!strcmp(value, classname)) {
			if (!gi.AAS_VectorForBSPEpairKey(ent, "origin", origin))
				return qfalse;
			VectorCopy(origin, goal->origin);
			VectorCopy(origin, start);
			start[2] -= 32;
			VectorCopy(origin, end);
			end[2] += 32;
			numareas = gi.AAS_TraceAreas(start, end, areas, NULL, 10);
			if (!numareas)
				return qfalse;
			goal->areanum = areas[0];
			return qtrue;
		}
	}
	return qfalse;
}

void BotSetupDeathmatchAI(void) {
	int ent, modelnum;
	char model[128];

	switch( gi.Cvar_VariableIntegerValue( "mp_gametype" ) )
	{
		case 0 : 
			gametype = GT_FFA;
			break;
		case 1 :  
			gametype = GT_TEAM;
			break;
		case 2 :  
			gametype = GT_CTF;
			break ;
		case 3 :  
			gametype = GT_TEAM;
			break ;
		default :
			gametype = GT_FFA;
			break ;
	}

	if ( gi.Cvar_VariableIntegerValue( "mp_modifier_Destruction" ) )
	{
		gametype = GT_OBELISK;
	}


	gi.Cvar_Register(&bot_rocketjump, "bot_rocketjump", "1", 0);
	gi.Cvar_Register(&bot_grapple, "bot_grapple", "0", 0);
	gi.Cvar_Register(&bot_fastchat, "bot_fastchat", "0", 0);
	gi.Cvar_Register(&bot_nochat, "bot_nochat", "0", 0);
	gi.Cvar_Register(&bot_testrchat, "bot_testrchat", "0", 0);
	gi.Cvar_Register(&bot_showstates, "bot_showstates","0",0);
	gi.Cvar_Register(&bot_challenge, "bot_challenge", "0", 0);
	gi.Cvar_Register(&bot_predictobstacles, "bot_predictobstacles", "1", 0);
	gi.Cvar_Register(&g_spSkill, "g_spSkill", "2", 0);
	ctf_neutralflag.areanum = 0;

#ifdef MISSIONPACK
	if (gametype == GT_OBELISK) {
		if (gi.BotGetLevelItemGoal(-1, "Red Obelisk", &redobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Obelisk without red obelisk\n");
		BotSetEntityNumForGoal(&redobelisk, "item_mp_destructionObject-red");
		if (gi.BotGetLevelItemGoal(-1, "Blue Obelisk", &blueobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Obelisk without blue obelisk\n");
		BotSetEntityNumForGoal(&blueobelisk, "item_mp_destructionObject-blue");
	}
#endif

	if (gametype == GT_CTF)
		if (multiplayerManager.checkGameType("oneflag"))
			gametype = GT_1FCTF;

	if (gametype == GT_CTF) {
		if (gi.BotGetLevelItemGoal(-1, "Red Flag", &ctf_redflag) < 0)
			BotAI_Print(PRT_WARNING, "CTF without Red Flag\n");
		if (gi.BotGetLevelItemGoal(-1, "Blue Flag", &ctf_blueflag) < 0)
			BotAI_Print(PRT_WARNING, "CTF without Blue Flag\n");
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (gi.BotGetLevelItemGoal(-1, "Neutral Flag", &ctf_neutralflag) < 0)
			BotAI_Print(PRT_WARNING, "One Flag CTF without Neutral Flag\n");
		if (gi.BotGetLevelItemGoal(-1, "Red Flag", &ctf_redflag) < 0)
			BotAI_Print(PRT_WARNING, "CTF without Red Flag\n");
		if (gi.BotGetLevelItemGoal(-1, "Blue Flag", &ctf_blueflag) < 0)
			BotAI_Print(PRT_WARNING, "CTF without Blue Flag\n");
	}
	else if (gametype == GT_HARVESTER) {
		if (gi.BotGetLevelItemGoal(-1, "Red Obelisk", &redobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without red obelisk\n");
		BotSetEntityNumForGoal(&redobelisk, "item_mp_destructionObject-red");
		if (gi.BotGetLevelItemGoal(-1, "Blue Obelisk", &blueobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without blue obelisk\n");
		BotSetEntityNumForGoal(&blueobelisk, "item_mp_destructionObject-blue");
		if (gi.BotGetLevelItemGoal(-1, "Neutral Obelisk", &neutralobelisk) < 0)
			BotAI_Print(PRT_WARNING, "Harvester without neutral obelisk\n");
		BotSetEntityNumForGoal(&neutralobelisk, "team_neutralobelisk");
	}
#endif

	max_bspmodelindex = 0;
	for (ent = gi.AAS_NextBSPEntity(0); ent; ent = gi.AAS_NextBSPEntity(ent)) {
		if (!gi.AAS_ValueForBSPEpairKey(ent, "model", model, sizeof(model))) continue;
		if (model[0] == '*') {
			modelnum = atoi(model+1);
			if (modelnum > max_bspmodelindex)
				max_bspmodelindex = modelnum;
		}
	}
	BotInitWaypoints();
}

void BotShutdownDeathmatchAI(void) {
	altroutegoals_setup = qfalse;
}

