


#include "g_local.h"
// never hits the collision; every other fgame TU reaches qcommon.h first through
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
#include "ai_vcmd.h"

#include "chars.h"
#include "inv.h"
#include "syn.h"

#include "player.h"

// R121 TEMP PROBE (port-only, worktree-only): the bot-think leg.  Symptom 1 is
#define R120_PROBE_GET(name, def, flags) gi.cvar(name, def, flags)
#define R120_PROBE_PRINT                 gi.Printf
#include "../qcommon/qa_r120.h"

#define MAX_PATH		144


bot_state_t	*botstates[MAX_CLIENTS];
int numbots;
float floattime;
float regularupdate_time;
int bot_interbreed;
int bot_interbreedmatchcount;
vmCvar_t bot_thinktime;
vmCvar_t bot_memorydump;
vmCvar_t bot_saveroutingcache;
vmCvar_t bot_pause;
vmCvar_t bot_report;
vmCvar_t bot_testsolid;
vmCvar_t bot_testclusters;
vmCvar_t bot_developer;
vmCvar_t bot_interbreedchar;
vmCvar_t bot_interbreedbots;
vmCvar_t bot_interbreedcycle;
vmCvar_t bot_interbreedwrite;


void QDECL BotAI_Print(int type, char *fmt, ...) {
	char str[2048];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(str, fmt, ap);
	va_end(ap);

	switch(type) {
		case PRT_MESSAGE: {
			gi.Printf("%s", str);
			break;
		}
		case PRT_WARNING: {
			gi.Printf( S_COLOR_YELLOW "Warning: %s", str );
			break;
		}
		case PRT_ERROR: {
			gi.Printf( S_COLOR_RED "Error: %s", str );
			break;
		}
		case PRT_FATAL: {
			gi.Printf( S_COLOR_RED "Fatal: %s", str );
			break;
		}
		case PRT_EXIT: {
			gi.Printf( S_COLOR_RED "Exit: %s", str );
			break;
		}
		default: {
			gi.Printf( "unknown print type\n" );
			break;
		}
	}
}


void BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask) {
	trace_t trace;

	gi.trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse);
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy(trace.endpos, bsptrace->endpos);
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy(trace.plane.normal, bsptrace->plane.normal);
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}

int BotAI_GetClientState( int clientNum, playerState_t *state ) {
	gentity_t	*ent;

	ent = &g_entities[clientNum];
	if ( !ent->inuse ) {
		return qfalse;
	}
	if ( !ent->client ) {
		return qfalse;
	}

	memcpy( state, &ent->client->ps, sizeof(playerState_t) );
	return qtrue;
}

int BotAI_GetEntityState( int entityNum, entityState_t *state ) {
	gentity_t	*ent;

	ent = &g_entities[entityNum];
	memset( state, 0, sizeof(entityState_t) );
	if (!ent->inuse) return qfalse;
	if (!ent->linked) return qfalse;
	if (ent->svflags & SVF_NOCLIENT) return qfalse;
	memcpy( state, &ent->s, sizeof(entityState_t) );
	return qtrue;
}

int BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state ) {
	int		entNum;

	entNum = gi.BotGetSnapshotEntity( clientNum, sequence );
	if ( entNum == -1 ) {
		memset(state, 0, sizeof(entityState_t));
		return -1;
	}

	BotAI_GetEntityState( entNum, state );

	return sequence + 1;
}

void QDECL BotAI_BotInitialChat( bot_state_t *bs, char *type, ... ) {
	int		i, mcontext;
	va_list	ap;
	char	*p;
	char	*vars[MAX_MATCHVARIABLES];

	memset(vars, 0, sizeof(vars));
	va_start(ap, type);
	p = va_arg(ap, char *);
	for (i = 0; i < MAX_MATCHVARIABLES; i++) {
		if( !p ) {
			break;
		}
		vars[i] = p;
		p = va_arg(ap, char *);
	}
	va_end(ap);

	mcontext = BotSynonymContext(bs);

	gi.BotInitialChat( bs->cs, type, mcontext, vars[0], vars[1], vars[2], vars[3], vars[4], vars[5], vars[6], vars[7] );
}


void BotTestAAS(vec3_t origin) {
	int areanum;
	aas_areainfo_t info;

	gi.Cvar_Update(&bot_testsolid);
	gi.Cvar_Update(&bot_testclusters);
	if (bot_testsolid.integer) {
		if (!gi.AAS_Initialized()) return;
		areanum = BotPointAreaNum(origin);
		if (areanum) BotAI_Print(PRT_MESSAGE, "emtpy area");   
		else BotAI_Print(PRT_MESSAGE, "^1SOLID area");   
	}
	else if (bot_testclusters.integer) {
		if (!gi.AAS_Initialized()) return;
		areanum = BotPointAreaNum(origin);
		if (!areanum)
			BotAI_Print(PRT_MESSAGE, "^1Solid!                              ");    
		else {
			gi.AAS_AreaInfo(areanum, &info);
			BotAI_Print(PRT_MESSAGE, "\rarea %d, cluster %d       ", areanum, info.cluster);
		}
	}
}

void BotReportStatus(bot_state_t *bs) {
	char goalname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char *leader, flagstatus[32];
	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) == 0) leader = "L";
	else leader = " ";

	strcpy(flagstatus, "  ");
	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs)) {
			if (BotTeam(bs) == TEAM_RED) strcpy(flagstatus, S_COLOR_RED"F ");
			else strcpy(flagstatus, S_COLOR_BLUE"F ");
		}
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs)) {
			if (BotTeam(bs) == TEAM_RED) strcpy(flagstatus, S_COLOR_RED"F ");
			else strcpy(flagstatus, S_COLOR_BLUE"F ");
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs)) {
			if (BotTeam(bs) == TEAM_RED) Com_sprintf(flagstatus, sizeof(flagstatus), S_COLOR_RED"%2d", bs->inventory[INVENTORY_REDCUBE]);
			else Com_sprintf(flagstatus, sizeof(flagstatus), S_COLOR_BLUE"%2d", bs->inventory[INVENTORY_BLUECUBE]);
		}
	}
#endif

	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: helping %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: accompanying %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_DEFENDKEYAREA:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: defending %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_GETITEM:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: getting item %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_KILL:
		{
			ClientName(bs->teamgoal.entitynum, goalname, sizeof(goalname));
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: killing %s\n", netname, leader, flagstatus, goalname);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: camping\n", netname, leader, flagstatus);
			break;
		}
		case LTG_PATROL:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: patrolling\n", netname, leader, flagstatus);
			break;
		}
		case LTG_GETFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: capturing flag\n", netname, leader, flagstatus);
			break;
		}
		case LTG_RUSHBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: rushing base\n", netname, leader, flagstatus);
			break;
		}
		case LTG_RETURNFLAG:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: returning flag\n", netname, leader, flagstatus);
			break;
		}
		case LTG_ATTACKENEMYBASE:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: attacking the enemy base\n", netname, leader, flagstatus);
			break;
		}
		case LTG_HARVEST:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: harvesting\n", netname, leader, flagstatus);
			break;
		}
		default:
		{
			BotAI_Print(PRT_MESSAGE, "%-20s%s%s: roaming\n", netname, leader, flagstatus);
			break;
		}
	}
}

void BotTeamplayReport(void) {
	int i;
	char buf[MAX_INFO_STRING];

	BotAI_Print(PRT_MESSAGE, S_COLOR_RED"RED\n");
	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if ( !botstates[i] || !botstates[i]->inuse ) continue;
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_RED) {
			BotReportStatus(botstates[i]);
		}
	}
	BotAI_Print(PRT_MESSAGE, S_COLOR_BLUE"BLUE\n");
	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if ( !botstates[i] || !botstates[i]->inuse ) continue;
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name"))) continue;
		if (atoi(Info_ValueForKey(buf, "t")) == TEAM_BLUE) {
			BotReportStatus(botstates[i]);
		}
	}
}

void BotSetInfoConfigString(bot_state_t *bs) {
	char goalname[MAX_MESSAGE_SIZE];
	char netname[MAX_MESSAGE_SIZE];
	char action[MAX_MESSAGE_SIZE];
	char *leader, carrying[32];
	bot_goal_t goal;
	ClientName(bs->client, netname, sizeof(netname));
	if (Q_stricmp(netname, bs->teamleader) == 0) leader = "L";
	else leader = " ";

	strcpy(carrying, "  ");
	if (gametype == GT_CTF) {
		if (BotCTFCarryingFlag(bs)) {
			strcpy(carrying, "F ");
		}
	}
#ifdef MISSIONPACK
	else if (gametype == GT_1FCTF) {
		if (Bot1FCTFCarryingFlag(bs)) {
			strcpy(carrying, "F ");
		}
	}
	else if (gametype == GT_HARVESTER) {
		if (BotHarvesterCarryingCubes(bs)) {
			if (BotTeam(bs) == TEAM_RED) Com_sprintf(carrying, sizeof(carrying), "%2d", bs->inventory[INVENTORY_REDCUBE]);
			else Com_sprintf(carrying, sizeof(carrying), "%2d", bs->inventory[INVENTORY_BLUECUBE]);
		}
	}
#endif

	switch(bs->ltgtype) {
		case LTG_TEAMHELP:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "helping %s", goalname);
			break;
		}
		case LTG_TEAMACCOMPANY:
		{
			EasyClientName(bs->teammate, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "accompanying %s", goalname);
			break;
		}
		case LTG_DEFENDKEYAREA:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "defending %s", goalname);
			break;
		}
		case LTG_GETITEM:
		{
			gi.BotGoalName(bs->teamgoal.number, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "getting item %s", goalname);
			break;
		}
		case LTG_KILL:
		{
			ClientName(bs->teamgoal.entitynum, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "killing %s", goalname);
			break;
		}
		case LTG_CAMP:
		case LTG_CAMPORDER:
		{
			Com_sprintf(action, sizeof(action), "camping");
			break;
		}
		case LTG_PATROL:
		{
			Com_sprintf(action, sizeof(action), "patrolling");
			break;
		}
		case LTG_GETFLAG:
		{
			Com_sprintf(action, sizeof(action), "capturing flag");
			break;
		}
		case LTG_RUSHBASE:
		{
			Com_sprintf(action, sizeof(action), "rushing base");
			break;
		}
		case LTG_RETURNFLAG:
		{
			Com_sprintf(action, sizeof(action), "returning flag");
			break;
		}
		case LTG_ATTACKENEMYBASE:
		{
			Com_sprintf(action, sizeof(action), "attacking the enemy base");
			break;
		}
		case LTG_HARVEST:
		{
			Com_sprintf(action, sizeof(action), "harvesting");
			break;
		}
		default:
		{
			gi.BotGetTopGoal(bs->gs, &goal);
			gi.BotGoalName(goal.number, goalname, sizeof(goalname));
			Com_sprintf(action, sizeof(action), "roaming %s", goalname);
			break;
		}
	}
  	gi.setConfigstring (CS_BOTINFO + bs->client, va("l\\%s\\c\\%s\\a\\%s",
				leader,
				carrying,
				action));
}

void BotUpdateInfoConfigStrings(void) {
	int i;
	char buf[MAX_INFO_STRING];

	for (i = 0; i < maxclients->integer && i < MAX_CLIENTS; i++) {
		if ( !botstates[i] || !botstates[i]->inuse )
			continue;
		Q_strncpyz(buf,gi.getConfigstring(CS_PLAYERS+i), sizeof(buf));
		if (!strlen(buf) || !strlen(Info_ValueForKey(buf, "name")))
			continue;
		BotSetInfoConfigString(botstates[i]);
	}
}

void BotInterbreedBots(void) {
	float ranks[MAX_CLIENTS];
	int parent1, parent2, child;
	int i;

	for (i = 0; i < maxclients->integer; i++) {
		if ( botstates[i] && botstates[i]->inuse ) {
			ranks[i] = botstates[i]->num_kills * 2 - botstates[i]->num_deaths;
		}
		else {
			ranks[i] = -1;
		}
	}

	if (gi.GeneticParentsAndChildSelection(MAX_CLIENTS, ranks, &parent1, &parent2, &child)) {
		gi.BotInterbreedGoalFuzzyLogic(botstates[parent1]->gs, botstates[parent2]->gs, botstates[child]->gs);
		gi.BotMutateGoalFuzzyLogic(botstates[child]->gs, 1);
	}
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (botstates[i] && botstates[i]->inuse) {
			botstates[i]->num_kills = 0;
			botstates[i]->num_deaths = 0;
		}
	}
}

void BotWriteInterbreeded(char *filename) {
	float rank, bestrank;
	int i, bestbot;

	bestrank = 0;
	bestbot = -1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if ( botstates[i] && botstates[i]->inuse ) {
			rank = botstates[i]->num_kills * 2 - botstates[i]->num_deaths;
		}
		else {
			rank = -1;
		}
		if (rank > bestrank) {
			bestrank = rank;
			bestbot = i;
		}
	}
	if (bestbot >= 0) {
		gi.BotSaveGoalFuzzyLogic(botstates[bestbot]->gs, filename);
	}
}

void BotInterbreedEndMatch(void) {

	if (!bot_interbreed) return;
	bot_interbreedmatchcount++;
	if (bot_interbreedmatchcount >= bot_interbreedcycle.integer) {
		bot_interbreedmatchcount = 0;
		gi.Cvar_Update(&bot_interbreedwrite);
		if (strlen(bot_interbreedwrite.string)) {
			BotWriteInterbreeded(bot_interbreedwrite.string);
			gi.cvar_set("bot_interbreedwrite", "");
		}
		BotInterbreedBots();
	}
}

int BotAIShutdownClient(int client, qboolean restart);
void G_ExitLevel(void);
void BotInterbreeding(void) {
	int i;

	gi.Cvar_Update(&bot_interbreedchar);
	if (!strlen(bot_interbreedchar.string)) return;
	if (gametype != GT_TOURNAMENT) {
		gi.cvar_set("g_gametype", va("%d", GT_TOURNAMENT));
		G_ExitLevel();
		return;
	}
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (botstates[i] && botstates[i]->inuse) {
			BotAIShutdownClient(botstates[i]->client, qfalse);
		}
	}
	//make sure all item weight configs are reloaded and Not shared
	gi.BotLibVarSet("bot_reloadcharacters", "1");
	for (i = 0; i < bot_interbreedbots.integer; i++) {
		gi.SendConsoleCommand( va("addbot %s 4 free %i %s%d\n",
						bot_interbreedchar.string, i * 50, bot_interbreedchar.string, i) ); 
	}
	gi.cvar_set("bot_interbreedchar", "");
	bot_interbreed = qtrue;
}

void BotEntityInfo(int entnum, aas_entityinfo_t *info) {
	gi.AAS_EntityInfo(entnum, info);

	if ( !g_entities[info->number].entity )
		info->valid = qfalse;
}

int NumBots(void) {
	return numbots;
}

int BotTeamLeader(bot_state_t *bs) {
	int leader;

	leader = ClientFromName(bs->teamleader);
	if (leader < 0) return qfalse;
	if (!botstates[leader] || !botstates[leader]->inuse) return qfalse;
	return qtrue;
}

float AngleDifference(float ang1, float ang2) {
	float diff;

	diff = ang1 - ang2;
	if (ang1 > ang2) {
		if (diff > 180.0) diff -= 360.0;
	}
	else {
		if (diff < -180.0) diff += 360.0;
	}
	return diff;
}

float BotChangeViewAngle(float angle, float ideal_angle, float speed) {
	float move;

	angle = AngleMod(angle);
	ideal_angle = AngleMod(ideal_angle);
	if (angle == ideal_angle) return angle;
	move = ideal_angle - angle;
	if (ideal_angle > angle) {
		if (move > 180.0) move -= 360.0;
	}
	else {
		if (move < -180.0) move += 360.0;
	}
	if (move > 0) {
		if (move > speed) move = speed;
	}
	else {
		if (move < -speed) move = -speed;
	}
	return AngleMod(angle + move);
}

void BotChangeViewAngles(bot_state_t *bs, float thinktime) {
	float diff, factor, maxchange, anglespeed, disired_speed;
	int i;

	if (bs->ideal_viewangles[PITCH] > 180) bs->ideal_viewangles[PITCH] -= 360;
	if (bs->enemy >= 0) {
		factor = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_VIEW_FACTOR, 0.01f, 1);
		maxchange = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_VIEW_MAXCHANGE, 1, 1800);
	}
	else {
		factor = 0.05f;
		maxchange = 360;
	}
	if (maxchange < 240) maxchange = 240;
	maxchange *= thinktime;
	for (i = 0; i < 2; i++) {
		if (bot_challenge.integer) {
			diff = abs(AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]));
			anglespeed = diff * factor;
			if (anglespeed > maxchange) anglespeed = maxchange;
			bs->viewangles[i] = BotChangeViewAngle(bs->viewangles[i],
											bs->ideal_viewangles[i], anglespeed);
		}
		else {
			bs->viewangles[i] = AngleMod(bs->viewangles[i]);
			bs->ideal_viewangles[i] = AngleMod(bs->ideal_viewangles[i]);
			diff = AngleDifference(bs->viewangles[i], bs->ideal_viewangles[i]);
			disired_speed = diff * factor;
			bs->viewanglespeed[i] += (bs->viewanglespeed[i] - disired_speed);
			if (bs->viewanglespeed[i] > 180) bs->viewanglespeed[i] = maxchange;
			if (bs->viewanglespeed[i] < -180) bs->viewanglespeed[i] = -maxchange;
			anglespeed = bs->viewanglespeed[i];
			if (anglespeed > maxchange) anglespeed = maxchange;
			if (anglespeed < -maxchange) anglespeed = -maxchange;
			bs->viewangles[i] += anglespeed;
			bs->viewangles[i] = AngleMod(bs->viewangles[i]);
			bs->viewanglespeed[i] *= 0.45 * (1 - factor);
		}
	}
	if (bs->viewangles[PITCH] > 180) bs->viewangles[PITCH] -= 360;
	gi.EA_View(bs->client, bs->viewangles);
}

void BotInputToUserCommand(bot_input_t *bi, usercmd_t *ucmd, int delta_angles[3], int time) {
	vec3_t angles, forward, right;
	short temp;
	int j;

	memset(ucmd, 0, sizeof(usercmd_t));
	ucmd->serverTime = time;
	if (bi->actionflags & ACTION_DELAYEDJUMP) {
		bi->actionflags |= ACTION_JUMP;
		bi->actionflags &= ~ACTION_DELAYEDJUMP;
	}

	if (bi->actionflags & ACTION_RESPAWN) ucmd->buttons = (BUTTON_ATTACKRIGHT | BUTTON_ATTACKLEFT);
	if (bi->actionflags & ACTION_ATTACK) ucmd->buttons |= BUTTON_ATTACKLEFT;
	if (bi->actionflags & ACTION_ATTACKRIGHT) ucmd->buttons |= BUTTON_ATTACKRIGHT;
	if (bi->actionflags & ACTION_TALK) ucmd->buttons |= BUTTON_TALK;
	if (bi->actionflags & ACTION_USE) ucmd->buttons |= BUTTON_USE;
	if (!(bi->actionflags & ACTION_WALK))
		ucmd->buttons |= BUTTON_RUN;
	ucmd->weapon = bi->weapon;
	ucmd->angles[PITCH] = ANGLE2SHORT(bi->viewangles[PITCH]);
	ucmd->angles[YAW] = ANGLE2SHORT(bi->viewangles[YAW]);
	ucmd->angles[ROLL] = ANGLE2SHORT(bi->viewangles[ROLL]);
	for (j = 0; j < 3; j++) {
		temp = ucmd->angles[j] - delta_angles[j];
		ucmd->angles[j] = temp;
	}
	if (bi->dir[2]) 
		angles[PITCH] = bi->viewangles[PITCH];
	else 
		angles[PITCH] = 0;
	angles[YAW] = bi->viewangles[YAW];
	angles[ROLL] = 0;
	AngleVectors(angles, forward, right, NULL);
	bi->speed = bi->speed * 127 / sv_maxspeed->value;
	ucmd->forwardmove = (signed char) (DotProduct(forward, bi->dir) * bi->speed);
	ucmd->rightmove = (signed char) (0 - DotProduct(right, bi->dir) * bi->speed);
	ucmd->upmove = (signed char) (abs(forward[2]) * bi->dir[2] * bi->speed);
	if (bi->actionflags & ACTION_MOVEFORWARD) ucmd->forwardmove += 127;
	if (bi->actionflags & ACTION_MOVEBACK) ucmd->forwardmove -= 127;
	if (bi->actionflags & ACTION_MOVELEFT) ucmd->rightmove -= 127;
	if (bi->actionflags & ACTION_MOVERIGHT) ucmd->rightmove += 127;
	if ((bi->actionflags & ACTION_MOVEUP) || (bi->actionflags & ACTION_JUMP)) ucmd->upmove += 127;
	if (bi->actionflags & ACTION_CROUCH) ucmd->upmove -= 127;
}

void BotUpdateInput(bot_state_t *bs, int time, int elapsed_time) {
	bot_input_t bi;
	int j;

	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	BotChangeViewAngles(bs, (float) elapsed_time / 1000);
	gi.EA_GetInput(bs->client, (float) time / 1000, &bi);
	if (bi.actionflags & ACTION_RESPAWN) {
		if (bs->lastucmd.buttons & (BUTTON_ATTACKRIGHT | BUTTON_ATTACKLEFT)) bi.actionflags &= ~(ACTION_RESPAWN|ACTION_ATTACK);
	}
	BotInputToUserCommand(&bi, &bs->lastucmd, bs->cur_ps.delta_angles, time);
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
}

void BotAIRegularUpdate(void) {
	if (regularupdate_time < FloatTime()) {
		gi.BotUpdateEntityItems();
		regularupdate_time = FloatTime() + 0.3;
	}
}

void RemoveColorEscapeSequences( char *text ) {
	int i, l;

	l = 0;
	for ( i = 0; text[i]; i++ ) {
		if (Q_IsColorString(&text[i])) {
			i++;
			continue;
		}
		if (text[i] > 0x7E)
			continue;
		text[l++] = text[i];
	}
	text[l] = '\0';
}



int BotAI(int client, float thinktime) {
	bot_state_t *bs;
	char buf[1024], *args,cmd[1024], *ptr, *ptr2;
	int j;
	vec3_t tmpv;
	int r202_t0 = R202_BotAITick();
	int r202_t;


	if ( !g_entities[ client ].inuse || !g_entities[ client ].entity || !g_entities[ client ].client )
	{
		BotAIShutdownClient( client, qfalse );
		return qfalse;
	}

	gi.EA_ResetInput(client);
	bs = botstates[client];
	if (!bs || !bs->inuse) {
		BotAI_Print(PRT_FATAL, "BotAI: client %d is not setup\n", client);
		return qfalse;
	}

	r202_t = R202_BotAITick();
	BotAI_GetClientState( client, &bs->cur_ps );
	R202_BotAIAdd( R202_BAIZ_cstate, R202_BotAITock( r202_t ) );

	r202_t = R202_BotAITick();
	while( gi.BotGetConsoleMessage(client, buf, sizeof(buf)) ) {
		args = strchr( buf, ':');
		if (!args) continue;
		*args++; 
		*args++;

		RemoveColorEscapeSequences( args );

		ptr = args;
		args = strchr(args,' ');
		Q_strncpyz(cmd,ptr,++args - ptr );
		cmd[(args - ptr) - 1] = 0;



		if (!Q_stricmp(buf, "cp "))
			{  }
#ifdef MISSIONPACK
		else if (!strncmp(cmd, "vchat",5)) {
			BotVoiceChatCommand(bs, SAY_ALL, ptr);
		}
		else if (!strncmp(cmd, "vtchat",6)) {
			BotVoiceChatCommand(bs, SAY_TEAM, ptr);
		}
		else if (!strncmp(cmd, "vtell",5)) {
			BotVoiceChatCommand(bs, SAY_TELL, ptr);
		}
#endif
		else if (!Q_stricmp(buf, "cs"))
			{  }
		else if (!strncmp(buf, "hudprint",8)) {
			ptr2= strchr(buf,':');
			memcpy(cmd,ptr2,strlen(ptr2)+1);
			memcpy(ptr2+1,cmd,strlen(cmd)+1);
			*ptr2 = 25;
			gi.BotQueueConsoleMessage(bs->cs, CMS_NORMAL, strchr(buf,'"'));
		}
		else if (!strncmp(buf, "hudsay",6)) {
			ptr2= strchr(buf,':');
			memcpy(cmd,ptr2,strlen(ptr2)+1);
			memcpy(ptr2+1,cmd,strlen(cmd)+1);
			*ptr2 = 25;
			gi.BotQueueConsoleMessage(bs->cs, CMS_CHAT, strchr(buf,'"'));
		}
		else if (!Q_stricmp(buf, "tchat")) {
			memmove(args, args+1, strlen(args));
			args[strlen(args)-1] = '\0';
			gi.BotQueueConsoleMessage(bs->cs, CMS_CHAT, args);
		}
		else if (!Q_stricmp(buf, "scores"))
			{  }
		else if (!Q_stricmp(buf, "clientLevelShot"))
			{  }
		else if ( stricmp(buf, "disconnect") == 0 ) { 
			int i = 0;
			i++;
		}
	}
	R202_BotAIAdd( R202_BAIZ_cmsg, R202_BotAITock( r202_t ) );
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] + SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}
	bs->ltime += thinktime;
	bs->thinktime = thinktime;
	VectorCopy(bs->cur_ps.origin, bs->origin);
	VectorCopy(bs->cur_ps.origin, bs->eye);
	bs->eye[2] += (bs->cur_ps.viewheight);
	VectorCopy(bs->origin,tmpv);
	tmpv[2] += 20;
	r202_t = R202_BotAITick();
	bs->areanum = BotPointAreaNum(bs->origin);
	R202_BotAIAdd( R202_BAIZ_area, R202_BotAITock( r202_t ) );
	r202_t = R202_BotAITick();
	BotDeathmatchAI(bs, thinktime);
	R202_BotAIAdd( R202_BAIZ_dm, R202_BotAITock( r202_t ) );
	r202_t = R202_BotAITick();
	gi.EA_SelectWeapon(bs->client, bs->weaponnum);
	R202_BotAIAdd( R202_BAIZ_weap, R202_BotAITock( r202_t ) );
	for (j = 0; j < 3; j++) {
		bs->viewangles[j] = AngleMod(bs->viewangles[j] - SHORT2ANGLE(bs->cur_ps.delta_angles[j]));
	}

	R202_BotAIBump( R202_BAIC_calls, 1 );
	R202_BotAIFrame( R202_BotAITock( r202_t0 ) );
	R203_BotFrame();

	return qtrue;
}

void BotScheduleBotThink(void) {
	int i, botnum;

	botnum = 0;

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		botstates[i]->botthink_residual = bot_thinktime.integer * botnum / numbots;
		botnum++;
	}
}

void BotWriteSessionData(bot_state_t *bs) {
	const char	*s;
	const char	*var;

	s = va(
			"%i %i %i %i %i %i %i %i"
			" %f %f %f"
			" %f %f %f"
			" %f %f %f",
		bs->lastgoal_decisionmaker,
		bs->lastgoal_ltgtype,
		bs->lastgoal_teammate,
		bs->lastgoal_teamgoal.areanum,
		bs->lastgoal_teamgoal.entitynum,
		bs->lastgoal_teamgoal.flags,
		bs->lastgoal_teamgoal.iteminfo,
		bs->lastgoal_teamgoal.number,
		bs->lastgoal_teamgoal.origin[0],
		bs->lastgoal_teamgoal.origin[1],
		bs->lastgoal_teamgoal.origin[2],
		bs->lastgoal_teamgoal.mins[0],
		bs->lastgoal_teamgoal.mins[1],
		bs->lastgoal_teamgoal.mins[2],
		bs->lastgoal_teamgoal.maxs[0],
		bs->lastgoal_teamgoal.maxs[1],
		bs->lastgoal_teamgoal.maxs[2]
		);

	var = va( "botsession%i", bs->client );

	gi.cvar_set( var, s );
}

void BotReadSessionData(bot_state_t *bs) {
	char	s[MAX_STRING_CHARS];
	const char	*var;

	var = va( "botsession%i", bs->client );
	gi.Cvar_VariableStringBuffer( var, s, sizeof(s) );

	sscanf(s,
			"%i %i %i %i %i %i %i %i"
			" %f %f %f"
			" %f %f %f"
			" %f %f %f",
		&bs->lastgoal_decisionmaker,
		&bs->lastgoal_ltgtype,
		&bs->lastgoal_teammate,
		&bs->lastgoal_teamgoal.areanum,
		&bs->lastgoal_teamgoal.entitynum,
		&bs->lastgoal_teamgoal.flags,
		&bs->lastgoal_teamgoal.iteminfo,
		&bs->lastgoal_teamgoal.number,
		&bs->lastgoal_teamgoal.origin[0],
		&bs->lastgoal_teamgoal.origin[1],
		&bs->lastgoal_teamgoal.origin[2],
		&bs->lastgoal_teamgoal.mins[0],
		&bs->lastgoal_teamgoal.mins[1],
		&bs->lastgoal_teamgoal.mins[2],
		&bs->lastgoal_teamgoal.maxs[0],
		&bs->lastgoal_teamgoal.maxs[1],
		&bs->lastgoal_teamgoal.maxs[2]
		);
}

int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart) {
	char filename[MAX_PATH], name[MAX_PATH], gender[MAX_PATH];
	bot_state_t *bs;
	int errnum;

	if (!botstates[client]) botstates[client] = (bot_state_t *)gi.Malloc(sizeof(bot_state_t)); 
	bs = botstates[client];

	memset(bs,0,sizeof(bot_state_t)); 
	if (bs && bs->inuse) {
		BotAI_Print(PRT_FATAL, "BotAISetupClient: client %d already setup\n", client);
		return qfalse;
	}

	if (!gi.AAS_Initialized()) {
		BotAI_Print(PRT_FATAL, "AAS not initialized\n");
		return qfalse;
	}

	bs->character = gi.BotLoadCharacter(settings->characterfile, settings->skill);
	if (!bs->character) {
		BotAI_Print(PRT_FATAL, "couldn't load skill %f from %s\n", settings->skill, settings->characterfile);
		return qfalse;
	}
	memcpy(&bs->settings, settings, sizeof(bot_settings_t));
	bs->gs = gi.BotAllocGoalState(client);
	gi.Characteristic_String(bs->character, CHARACTERISTIC_ITEMWEIGHTS, filename, MAX_PATH);
	errnum = gi.BotLoadItemWeights(bs->gs, filename);
	if (errnum != BLERR_NOERROR) {
		gi.BotFreeGoalState(bs->gs);
		return qfalse;
	}
	bs->ws = gi.BotAllocWeaponState();
	gi.Characteristic_String(bs->character, CHARACTERISTIC_WEAPONWEIGHTS, filename, MAX_PATH);
	errnum = gi.BotLoadWeaponWeights(bs->ws, filename);
	if (errnum != BLERR_NOERROR) {
		gi.BotFreeGoalState(bs->gs);
		gi.BotFreeWeaponState(bs->ws);
		return qfalse;
	}
	bs->cs = gi.BotAllocChatState();
	gi.Characteristic_String(bs->character, CHARACTERISTIC_CHAT_FILE, filename, MAX_PATH);
	gi.Characteristic_String(bs->character, CHARACTERISTIC_CHAT_NAME, name, MAX_PATH);
	errnum = gi.BotLoadChatFile(bs->cs, filename, name);
	if (errnum != BLERR_NOERROR) {
		gi.BotFreeChatState(bs->cs);
		gi.BotFreeGoalState(bs->gs);
		gi.BotFreeWeaponState(bs->ws);
		return qfalse;
	}
	gi.Characteristic_String(bs->character, CHARACTERISTIC_GENDER, gender, MAX_PATH);
	if (*gender == 'f' || *gender == 'F') gi.BotSetChatGender(bs->cs, CHAT_GENDERFEMALE);
	else if (*gender == 'm' || *gender == 'M') gi.BotSetChatGender(bs->cs, CHAT_GENDERMALE);
	else gi.BotSetChatGender(bs->cs, CHAT_GENDERLESS);

	bs->inuse = qtrue;
	bs->client = client;
	bs->entitynum = client;
	bs->setupcount = 4;
	bs->entergame_time = FloatTime();
	bs->ms = gi.BotAllocMoveState();
	bs->walker = gi.Characteristic_BFloat(bs->character, CHARACTERISTIC_WALKER, 0, 1);
	numbots++;

	if (gi.Cvar_VariableIntegerValue("bot_testichat")) {
		gi.BotLibVarSet("bot_testichat", "1");
		BotChatTest(bs);
	}
	BotScheduleBotThink();
	if (bot_interbreed) {
		gi.BotMutateGoalFuzzyLogic(bs->gs, 1);
	}
	if (restart) {
		BotReadSessionData(bs);
	}
	return qtrue;
}

int BotAIShutdownClient(int client, qboolean restart) {
	bot_state_t *bs;
	qboolean characterStillBeingUsed;
	int i;

	bs = botstates[client];
	if (!bs || !bs->inuse) {
		return qfalse;
	}

	if (restart) {
		BotWriteSessionData(bs);
	}

	if (BotChat_ExitGame(bs)) {
		gi.BotEnterChat(bs->cs, bs->client, CHAT_ALL);
	}

	gi.BotFreeMoveState(bs->ms);
	gi.BotFreeGoalState(bs->gs);
	gi.BotFreeChatState(bs->cs);
	gi.BotFreeWeaponState(bs->ws);

	characterStillBeingUsed = qfalse;

	for( i = 0 ; i < maxclients->integer ; i++ )
	{
		bot_state_t *otherBotState;

		otherBotState = botstates[ i ];

		if ( !otherBotState || !otherBotState->inuse || otherBotState == bs )
			continue;

		if ( otherBotState->character == bs->character )
			characterStillBeingUsed = qtrue;
		
	}

	if ( !characterStillBeingUsed )
		gi.BotFreeCharacter(bs->character);

	BotFreeWaypoints(bs->checkpoints);
	BotFreeWaypoints(bs->patrolpoints);
	BotClearActivateGoalStack(bs);
	memset(bs, 0, sizeof(bot_state_t));
	bs->inuse = qfalse;
	numbots--;
	return qtrue;
}

void BotResetState(bot_state_t *bs) {
	int client, entitynum, inuse;
	int movestate, goalstate, chatstate, weaponstate;
	bot_settings_t settings;
	int character;
	playerState_t ps;							
	float entergame_time;

	memcpy(&settings, &bs->settings, sizeof(bot_settings_t));
	memcpy(&ps, &bs->cur_ps, sizeof(playerState_t));
	inuse = bs->inuse;
	client = bs->client;
	entitynum = bs->entitynum;
	character = bs->character;
	movestate = bs->ms;
	goalstate = bs->gs;
	chatstate = bs->cs;
	weaponstate = bs->ws;
	entergame_time = bs->entergame_time;
	BotFreeWaypoints(bs->checkpoints);
	BotFreeWaypoints(bs->patrolpoints);
	memset(bs, 0, sizeof(bot_state_t));
	bs->ms = movestate;
	bs->gs = goalstate;
	bs->cs = chatstate;
	bs->ws = weaponstate;
	memcpy(&bs->cur_ps, &ps, sizeof(playerState_t));
	memcpy(&bs->settings, &settings, sizeof(bot_settings_t));
	bs->inuse = inuse;
	bs->client = client;
	bs->entitynum = entitynum;
	bs->character = character;
	bs->entergame_time = entergame_time;
	if (bs->ms) gi.BotResetMoveState(bs->ms);
	if (bs->gs) gi.BotResetGoalState(bs->gs);
	if (bs->ws) gi.BotResetWeaponState(bs->ws);
	if (bs->gs) gi.BotResetAvoidGoals(bs->gs);
	if (bs->ms) gi.BotResetAvoidReach(bs->ms);
}

int BotAILoadMap( int restart ) {
	int			i;
	vmCvar_t	mapname;

	if (!restart) {
		gi.Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		gi.BotLibLoadMap( mapname.string );
	}

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (botstates[i] && botstates[i]->inuse) {
			BotResetState( botstates[i] );
			botstates[i]->setupcount = 4;
		}
	}

	BotSetupDeathmatchAI();

	return qtrue;
}

#ifdef MISSIONPACK
void ProximityMine_Trigger( gentity_t *trigger, gentity_t *other, trace_t *trace );
#endif

void G_CheckBotSpawn( void );


extern Event EV_Player_UseItem;

#define R201_BOT_WINDOW_MS   1000
#define R201_BOT_MAX_FRAMES  3600
#define R201_BOT_MAX_ZONES   9

static int		r201_bEntRuns;

static void R201_BotBudget( int totalMsec, int spawnMsec, int libstartMsec, int updentMsec,
							int regularMsec, int thinkMsec, int inputMsec,
							int botCount, int entCount )
{
	static cvar_t	*r201_botStats;
	static int		r201_bTotal, r201_bTotalMax;
	static int		r201_bSpawn, r201_bSpawnMax;
	static int		r201_bLib,   r201_bLibMax;
	static int		r201_bUpd,   r201_bUpdMax;
	static int		r201_bReg,   r201_bRegMax;
	static int		r201_bThink, r201_bThinkMax;
	static int		r201_bInput, r201_bInputMax;
	static int		r201_bOther, r201_bOtherMax;
	static int		r201_bBots,  r201_bBotsMax;
	static int		r201_bEnts,  r201_bEntsMax;
	static int		r201_bFrames, r201_bWindowStart;

	int		otherMsec, window, frames, i, mean10;
	char	line[1024], part[64];
	struct { const char *name; int sum; int max; } zone[R201_BOT_MAX_ZONES];

	if ( !r201_botStats ) {
		r201_botStats = gi.cvar( "r201_botStats", "1", 0 );
	}

	if ( !r201_botStats || !r201_botStats->integer ) {
		r201_bFrames = 0;
		r201_bWindowStart = 0;
		r201_bTotal = r201_bTotalMax = 0;
		r201_bSpawn = r201_bSpawnMax = 0;
		r201_bLib   = r201_bLibMax   = 0;
		r201_bUpd   = r201_bUpdMax   = 0;
		r201_bReg   = r201_bRegMax   = 0;
		r201_bThink = r201_bThinkMax = 0;
		r201_bInput = r201_bInputMax = 0;
		r201_bOther = r201_bOtherMax = 0;
		r201_bBots  = r201_bBotsMax  = 0;
		r201_bEnts  = r201_bEntsMax  = 0;
		r201_bEntRuns = 0;
		return;
	}

	if ( totalMsec   < 0 ) totalMsec   = 0;
	if ( spawnMsec   < 0 ) spawnMsec   = 0;
	if ( libstartMsec< 0 ) libstartMsec= 0;
	if ( updentMsec  < 0 ) updentMsec  = 0;
	if ( regularMsec < 0 ) regularMsec = 0;
	if ( thinkMsec   < 0 ) thinkMsec   = 0;
	if ( inputMsec   < 0 ) inputMsec   = 0;

	otherMsec = totalMsec - spawnMsec - libstartMsec - updentMsec
			  - regularMsec - thinkMsec - inputMsec;
	if ( otherMsec < 0 ) {
		otherMsec = 0;
	}

	r201_bTotal += totalMsec;	if ( totalMsec  > r201_bTotalMax ) r201_bTotalMax = totalMsec;
	r201_bSpawn += spawnMsec;	if ( spawnMsec  > r201_bSpawnMax ) r201_bSpawnMax = spawnMsec;
	r201_bLib   += libstartMsec;if ( libstartMsec> r201_bLibMax  ) r201_bLibMax   = libstartMsec;
	r201_bUpd   += updentMsec;	if ( updentMsec > r201_bUpdMax  ) r201_bUpdMax   = updentMsec;
	r201_bReg   += regularMsec;	if ( regularMsec> r201_bRegMax  ) r201_bRegMax   = regularMsec;
	r201_bThink += thinkMsec;	if ( thinkMsec  > r201_bThinkMax) r201_bThinkMax = thinkMsec;
	r201_bInput += inputMsec;	if ( inputMsec  > r201_bInputMax) r201_bInputMax = inputMsec;
	r201_bOther += otherMsec;	if ( otherMsec  > r201_bOtherMax) r201_bOtherMax = otherMsec;
	r201_bBots  += botCount;	if ( botCount   > r201_bBotsMax ) r201_bBotsMax  = botCount;
	r201_bEnts  += entCount;	if ( entCount   > r201_bEntsMax ) r201_bEntsMax  = entCount;

	r201_bFrames++;

	if ( !r201_bWindowStart ) {
		r201_bWindowStart = gi.Milliseconds();
	}

	window = gi.Milliseconds() - r201_bWindowStart;
	if ( window < R201_BOT_WINDOW_MS && r201_bFrames < R201_BOT_MAX_FRAMES ) {
		return;
	}

	frames = r201_bFrames;
	if ( frames < 1 ) {
		frames = 1;
	}

	zone[0].name = "spawn";		zone[0].sum = r201_bSpawn;	zone[0].max = r201_bSpawnMax;
	zone[1].name = "libstart";	zone[1].sum = r201_bLib;	zone[1].max = r201_bLibMax;
	zone[2].name = "updent";	zone[2].sum = r201_bUpd;	zone[2].max = r201_bUpdMax;
	zone[3].name = "regular";	zone[3].sum = r201_bReg;	zone[3].max = r201_bRegMax;
	zone[4].name = "think";		zone[4].sum = r201_bThink;	zone[4].max = r201_bThinkMax;
	zone[5].name = "input";		zone[5].sum = r201_bInput;	zone[5].max = r201_bInputMax;
	zone[6].name = "other";		zone[6].sum = r201_bOther;	zone[6].max = r201_bOtherMax;
	zone[7].name = "total";		zone[7].sum = r201_bTotal;	zone[7].max = r201_bTotalMax;
	zone[8].name = "bots";		zone[8].sum = r201_bBots;	zone[8].max = r201_bBotsMax;

	line[0] = '\0';
	for ( i = 0; i < R201_BOT_MAX_ZONES; i++ ) {
		mean10 = (int)( ( (long long)zone[i].sum * 10 + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
					 zone[i].name, mean10 / 10, mean10 % 10, zone[i].max );
		Q_strcat( line, sizeof( line ), part );
	}

	gi.Printf( "[R201:bot] frames=%d runs=%d window=%dms ents=%d/%d%s\n",
			   frames, r201_bEntRuns, window,
			   r201_bEntRuns ? ( r201_bEnts / r201_bEntRuns ) : 0, r201_bEntsMax, line );

	r201_bFrames = 0;
	r201_bWindowStart = gi.Milliseconds();
	r201_bTotal = r201_bTotalMax = 0;
	r201_bSpawn = r201_bSpawnMax = 0;
	r201_bLib   = r201_bLibMax   = 0;
	r201_bUpd   = r201_bUpdMax   = 0;
	r201_bReg   = r201_bRegMax   = 0;
	r201_bThink = r201_bThinkMax = 0;
	r201_bInput = r201_bInputMax = 0;
	r201_bOther = r201_bOtherMax = 0;
	r201_bBots  = r201_bBotsMax  = 0;
	r201_bEnts  = r201_bEntsMax  = 0;
	r201_bEntRuns = 0;
}

#define R202_BAI_WINDOW_MS   1000
#define R202_BAI_MAX_FRAMES  3600
#define R202_BAI_ZONES       10
#define R202_BAI_COUNTERS    2

static int	r202_baiOn = -1;	
static int	r202_baiZones[R202_BAI_ZONES];
static int	r202_baiZoneMax[R202_BAI_ZONES];
static int	r202_baiCnt[R202_BAI_COUNTERS];
static int	r202_baiCntMax[R202_BAI_COUNTERS];
static int	r202_baiCallNamed;
static int	r202_baiFrames;
static int	r202_baiWindowStart;
static int	r202_baiTotal, r202_baiTotalMax;

static const char *r202_baiZoneName[R202_BAI_ZONES] = {
	"cstate", "cmsg", "area", "dm", "weap", "dminv", "dmteam", "dmnode", "dmoth",
	"other"
};

static const char *r202_baiCntName[R202_BAI_COUNTERS] = { "calls", "bots" };

static int R202_BotAIOn( void ) {
	cvar_t *v;

	if ( r202_baiOn < 0 ) {
		v = gi.cvar( "r202_stats", "1", 0 );
		r202_baiOn = ( v && v->integer ) ? 1 : 0;
	}

	return r202_baiOn;
}

int R202_BotAIMicro( void ) {
#ifndef _WIN32
	struct timespec ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	return (int)( ( ts.tv_sec % 1000000 ) * 1000000 + ts.tv_nsec / 1000 );
#else
	return gi.Milliseconds() * 1000;
#endif
}

int R202_BotAITick( void ) {
	return R202_BotAIOn() ? R202_BotAIMicro() : 0;
}

int R202_BotAITock( int t0 ) {
	return R202_BotAIOn() ? ( R202_BotAIMicro() - t0 ) : 0;
}

void R202_BotAIAdd( int zone, int usec ) {
	int tenths;

	if ( !R202_BotAIOn() ) {
		return;
	}
	if ( usec <= 0 ) {
		return;
	}

	r202_baiCallNamed += usec;
	tenths = usec / 100;
	if ( tenths < 0 ) {
		tenths = 0;
	}

	if ( zone >= 0 && zone < R202_BAI_ZONES ) {
		r202_baiZones[zone] += tenths;
		if ( tenths > r202_baiZoneMax[zone] ) {
			r202_baiZoneMax[zone] = tenths;
		}
	}
}

void R202_BotAIBump( int which, int n ) {
	if ( !R202_BotAIOn() || n <= 0 || which < 0 || which >= R202_BAI_COUNTERS ) {
		return;
	}

	r202_baiCnt[which] += n;
	if ( n > r202_baiCntMax[which] ) {
		r202_baiCntMax[which] = n;
	}
}

void R202_BotAIFrame( int totalUsec ) {
	int window, frames, i, mean10, other;

	if ( !R202_BotAIOn() ) {
		return;
	}

	other = totalUsec - r202_baiCallNamed;
	if ( other < 0 ) {
		other = 0;
	}
	R202_BotAIAdd( R202_BAIZ_other, other );	

	r202_baiTotal += totalUsec / 100;
	if ( totalUsec / 100 > r202_baiTotalMax ) {
		r202_baiTotalMax = totalUsec / 100;
	}
	r202_baiCallNamed = 0;
	r202_baiFrames++;

	if ( !r202_baiWindowStart ) {
		r202_baiWindowStart = gi.Milliseconds();
	}

	window = gi.Milliseconds() - r202_baiWindowStart;
	if ( window < R202_BAI_WINDOW_MS && r202_baiFrames < R202_BAI_MAX_FRAMES ) {
		return;
	}

	frames = r202_baiFrames;
	if ( frames < 1 ) {
		frames = 1;
	}

	{
		char line[1024], part[64];

		line[0] = '\0';
		for ( i = 0; i < R202_BAI_ZONES; i++ ) {
			mean10 = (int)( ( (long long)r202_baiZones[i] + frames / 2 ) / frames );
			Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d.%d",
						 r202_baiZoneName[i], mean10 / 10, mean10 % 10,
						 r202_baiZoneMax[i] / 10, r202_baiZoneMax[i] % 10 );
			Q_strcat( line, sizeof( line ), part );
		}
		for ( i = 0; i < R202_BAI_COUNTERS; i++ ) {
			mean10 = (int)( ( (long long)r202_baiCnt[i] * 10 + frames / 2 ) / frames );
			Com_sprintf( part, sizeof( part ), " %s=%d.%d/%d",
						 r202_baiCntName[i], mean10 / 10, mean10 % 10, r202_baiCntMax[i] );
			Q_strcat( line, sizeof( line ), part );
		}
		mean10 = (int)( ( (long long)r202_baiTotal + frames / 2 ) / frames );
		Com_sprintf( part, sizeof( part ), " total=%d.%d/%d.%d",
					 mean10 / 10, mean10 % 10,
					 r202_baiTotalMax / 10, r202_baiTotalMax % 10 );
		Q_strcat( line, sizeof( line ), part );

		gi.Printf( "[R202:botai] calls=%d window=%dms%s\n", frames, window, line );
	}

	r202_baiFrames = 0;
	r202_baiWindowStart = gi.Milliseconds();
	r202_baiTotal = r202_baiTotalMax = 0;
	for ( i = 0; i < R202_BAI_ZONES; i++ ) {
		r202_baiZones[i] = r202_baiZoneMax[i] = 0;
	}
	for ( i = 0; i < R202_BAI_COUNTERS; i++ ) {
		r202_baiCnt[i] = r202_baiCntMax[i] = 0;
	}
}

#define R203_BN_WINDOW_MS	1000
#define R203_BN_MAX_FRAMES	3600

typedef struct {
	const char	*name;
	int			(*fn)(bot_state_t *bs);
} r203_bnode_t;

static const r203_bnode_t r203_bNodeTab[R203_BNODE_MAX] = {
	{ "intermission", AINode_Intermission },
	{ "observer",     AINode_Observer },
	{ "respawn",      AINode_Respawn },
	{ "stand",        AINode_Stand },
	{ "activate",     AINode_Seek_ActivateEntity },
	{ "seeknbg",      AINode_Seek_NBG },
	{ "seekltg",      AINode_Seek_LTG },
	{ "battlefight",  AINode_Battle_Fight },
	{ "battlechase",  AINode_Battle_Chase },
	{ "battleretreat",AINode_Battle_Retreat },
	{ "battlenbg",    AINode_Battle_NBG },
	{ "unknown",      NULL }
};

static cvar_t	*r203_bStats;
static int		r203_bStatsLast = -1;
static int		r203_bNodeCalls[R203_BNODE_MAX];
static int		r203_bNodeUsec[R203_BNODE_MAX];
static int		r203_bNodeMax[R203_BNODE_MAX];
static int		r203_bLoops, r203_bSat, r203_bOther;
static int		r203_bCurNode;
static int		r203_bCurT0;
static int		r203_bWindowStart, r203_bFrames;

static int R203_BotOn( void ) {
	return ( r203_bStats && r203_bStats->integer ) ? 1 : 0;
}

static void R203_BotSyncSwitch( void ) {
	if ( !r203_bStats ) {
		r203_bStats = gi.cvar( "r203_stats", "1", 0 );
	}
	if ( r203_bStats && r203_bStats->integer != r203_bStatsLast ) {
		char v[2];

		r203_bStatsLast = r203_bStats->integer;
		v[0] = r203_bStatsLast ? '1' : '0';
		v[1] = '\0';
		gi.BotLibVarSet( "r203_stats", v );
	}
}

static int R203_BotTick( void ) {
	return R203_BotOn() ? R202_BotAIMicro() : 0;
}

static int R203_BotTock( int t0 ) {
	return R203_BotOn() ? ( R202_BotAIMicro() - t0 ) : 0;
}

void R203_BotNodeBegin( bot_state_t *bs ) {
	int i;

	if ( !R203_BotOn() ) {
		return;
	}

	r203_bCurNode = R203_BN_unknown;
	for ( i = 0; i < R203_BNODE_MAX; i++ ) {
		if ( r203_bNodeTab[i].fn && r203_bNodeTab[i].fn == bs->ainode ) {
			r203_bCurNode = i;
			break;
		}
	}
	r203_bCurT0 = R203_BotTick();
}

void R203_BotNodeEnd( void ) {
	int usec;

	if ( !R203_BotOn() ) {
		return;
	}

	usec = R203_BotTock( r203_bCurT0 );
	r203_bLoops++;
	if ( r203_bCurNode < 0 || r203_bCurNode >= R203_BNODE_MAX ) {
		r203_bOther++;
		return;
	}
	r203_bNodeCalls[r203_bCurNode]++;
	r203_bNodeUsec[r203_bCurNode] += usec;
	if ( usec > r203_bNodeMax[r203_bCurNode] ) {
		r203_bNodeMax[r203_bCurNode] = usec;
	}
}

void R203_BotNodeSat( void ) {
	if ( !R203_BotOn() ) {
		return;
	}
	r203_bSat++;
}

void R203_BotFrame( void ) {
	int window, i;
	char line[1024], part[64];

	if ( !R203_BotOn() ) {
		r203_bLoops = r203_bSat = r203_bOther = 0;
		r203_bWindowStart = r203_bFrames = 0;
		for ( i = 0; i < R203_BNODE_MAX; i++ ) {
			r203_bNodeCalls[i] = r203_bNodeUsec[i] = r203_bNodeMax[i] = 0;
		}
		return;
	}

	if ( !r203_bWindowStart ) {
		r203_bWindowStart = gi.Milliseconds();
	}

	window = gi.Milliseconds() - r203_bWindowStart;
	r203_bFrames++;
	if ( window < R203_BN_WINDOW_MS && r203_bFrames < R203_BN_MAX_FRAMES ) {
		return;
	}

	line[0] = '\0';
	Com_sprintf( part, sizeof( part ), " loops=%d sat=%d other=%d",
				 r203_bLoops, r203_bSat, r203_bOther );
	Q_strcat( line, sizeof( line ), part );
	for ( i = 0; i < R203_BNODE_MAX; i++ ) {
		int mean = r203_bNodeCalls[i] ? ( r203_bNodeUsec[i] / r203_bNodeCalls[i] ) : 0;

		Com_sprintf( part, sizeof( part ), " %s=%d/%d/%d",
					 r203_bNodeTab[i].name, r203_bNodeCalls[i], mean, r203_bNodeMax[i] );
		Q_strcat( line, sizeof( line ), part );
	}
	gi.Printf( "[R203:bot] window=%dms%s\n", window, line );

	r203_bWindowStart = gi.Milliseconds();
	r203_bFrames = 0;
	r203_bLoops = r203_bSat = r203_bOther = 0;
	for ( i = 0; i < R203_BNODE_MAX; i++ ) {
		r203_bNodeCalls[i] = r203_bNodeUsec[i] = r203_bNodeMax[i] = 0;
	}
}


static int local_time;
static int botlib_residual;
static int lastbotthink_time;

int BotAIStartFrame(int time) {
	int i;
	gentity_t	*ent;
	bot_entitystate_t state;
	int elapsed_time, thinktime;
	int r201_t0, r201_tspawn, r201_tlib, r201_tupd, r201_treg, r201_tthink, r201_tend;
	int r201_bots = 0, r201_ents = 0;

	r201_t0 = gi.Milliseconds();

	R203_BotSyncSwitch();

	G_CheckBotSpawn();

	gi.Cvar_Update(&bot_rocketjump);
	gi.Cvar_Update(&bot_grapple);
	gi.Cvar_Update(&bot_fastchat);
	gi.Cvar_Update(&bot_nochat);
	gi.Cvar_Update(&bot_testrchat);
	gi.Cvar_Update(&bot_thinktime);
	gi.Cvar_Update(&bot_memorydump);
	gi.Cvar_Update(&bot_saveroutingcache);
	gi.Cvar_Update(&bot_pause);
	gi.Cvar_Update(&bot_report);

	if (bot_report.integer) {
		BotUpdateInfoConfigStrings();
	}

	if (bot_pause.integer) {
		R121_PROBE("bot:pause", "bot_pause=%d -- the WHOLE think leg is parked, ucmds still sent\n",
		           bot_pause.integer);
		for( i = 0; i < MAX_CLIENTS; i++ ) {
			if( !botstates[i] || !botstates[i]->inuse ) {
				continue;
			}
			if( g_entities[i].client->pers.enterTime) { 
				continue;
			}
			botstates[i]->lastucmd.forwardmove = 0;
			botstates[i]->lastucmd.rightmove = 0;
			botstates[i]->lastucmd.upmove = 0;
			botstates[i]->lastucmd.buttons = 0;
			botstates[i]->lastucmd.serverTime = time;
			gi.BotUserCommand(botstates[i]->client, &botstates[i]->lastucmd);
		}
		return qtrue;
	}

	if (bot_memorydump.integer) {
		gi.BotLibVarSet("memorydump", "1");
		gi.cvar_set("bot_memorydump", "0");
	}
	if (bot_saveroutingcache.integer) {
		gi.BotLibVarSet("saveroutingcache", "1");
		gi.cvar_set("bot_saveroutingcache", "0");
	}
	BotInterbreeding();
	if (bot_thinktime.integer > 200) {
		gi.cvar_set("bot_thinktime", "200");
	}
	if (bot_thinktime.integer != lastbotthink_time) {
		lastbotthink_time = bot_thinktime.integer;
		BotScheduleBotThink();
	}

	// default to zero-cost when the think budget does not fire this frame.
	r201_tspawn = gi.Milliseconds();
	r201_tlib = r201_tupd = r201_treg = r201_tspawn;

	elapsed_time = time - local_time;
	local_time = time;

	botlib_residual += elapsed_time;

	if (elapsed_time > bot_thinktime.integer) thinktime = elapsed_time;
	else thinktime = bot_thinktime.integer;

	if ( botlib_residual >= thinktime ) {
		botlib_residual -= thinktime;

		gi.BotLibStartFrame((float) time / 1000);

		if (!gi.AAS_Initialized()) {
			static int r121_aas;
			R121_PROBE("bot:aasdead", "AAS NOT initialized at BotLibStartFrame, returning qfalse (call %d)\n",
			           ++r121_aas);
			return qfalse;
		}

		r201_tlib = gi.Milliseconds();

		r201_bEntRuns++;	

		for (i = 0; i < MAX_GENTITIES; i++) {
			ent = &g_entities[i];
			r201_ents++;
			if (!ent->inuse) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			if (!ent->linked) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			if (ent->svflags & SVF_NOCLIENT) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			// do not update missiles
			if (ent->s.eType == ET_MISSILE) { 
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
			// do not update event only entities
			if (ent->s.eType > ET_EVENTS) {
				gi.BotLibUpdateEntity(i, NULL);
				continue;
			}
#ifdef MISSIONPACKBOTTODO
			// never link prox mine triggers
			if (ent->contents == CONTENTS_TRIGGER) {
				if (ent->touch == ProximityMine_Trigger) {
					gi.BotLibUpdateEntity(i, NULL);
					continue;
				}
			}
#endif
			memset(&state, 0, sizeof(bot_entitystate_t));
			VectorCopy(ent->currentOrigin, state.origin);
			VectorCopy(ent->currentAngles, state.angles);
			VectorCopy(ent->s.origin2, state.old_origin);
			VectorCopy(ent->mins, state.mins);
			VectorCopy(ent->maxs, state.maxs);
			state.type = ent->s.eType;
			state.flags = ent->s.eFlags;
			if (ent->bmodel) state.solid = SOLID_BSP;
			else state.solid = SOLID_BBOX;
			state.groundent = ent->s.groundEntityNum;
			state.modelindex = ent->s.modelindex;
			state.frame = ent->s.frame;
			gi.BotLibUpdateEntity(i, &state);
		}

		r201_tupd = gi.Milliseconds();

		BotAIRegularUpdate();

		r201_treg = gi.Milliseconds();
	}

	floattime = gi.AAS_Time();

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		r201_bots++;
		botstates[i]->botthink_residual += elapsed_time;
		if ( botstates[i]->botthink_residual >= thinktime ) {
			botstates[i]->botthink_residual -= thinktime;

			if (!gi.AAS_Initialized()) {
				static int r121_aas2;
				R121_PROBE("bot:aasdead2", "AAS NOT initialized before BotAI(%d), returning qfalse (call %d)\n",
				           i, ++r121_aas2);
				return qfalse;
			}

			if (g_entities[i].client->pers.enterTime) { 
				static int r121_think;
				if (R120_Throttle(&r121_think, 4, 200)) {
					R121_PROBE("bot:think", "BotAI(%d) thinktime=%d residual_budget=%d enterTime=%.0f level.time=%.0f\n",
					           i, thinktime, botstates[i]->botthink_residual,
					           g_entities[i].client->pers.enterTime, level.time);
				}
				BotAI(i, (float) thinktime / 1000);
			} else {
				static int r121_skip;
				if (R120_Throttle(&r121_skip, 4, 200)) {
					R121_PROBE("bot:skipthink", "bot %d SKIPPED by enterTime==0 gate (retail ai_main.cpp:1587) level.time=%.0f\n",
					           i, level.time);
				}
			}
		}
	}


	r201_tthink = gi.Milliseconds();

	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		if( !g_entities[i].client->pers.enterTime) { 
			continue;
		}

		BotUpdateInput(botstates[i], time, elapsed_time);
		{
			static int r121_ucmd;
			if (R120_Throttle(&r121_ucmd, 8, 400)) {
				R121_PROBE("bot:ucmd", "BotUserCommand(%d) fwd=%d right=%d up=%d buttons=%d weapon=%d "
				                       "ucmdYaw=%.1f ucmdPitch=%.1f botYaw=%.1f botPitch=%.1f velYaw=%.1f\n",
				           i, botstates[i]->lastucmd.forwardmove, botstates[i]->lastucmd.rightmove,
				           botstates[i]->lastucmd.upmove, botstates[i]->lastucmd.buttons,
				           botstates[i]->lastucmd.weapon,
				           SHORT2ANGLE(botstates[i]->lastucmd.angles[YAW]),
				           SHORT2ANGLE(botstates[i]->lastucmd.angles[PITCH]),
				           botstates[i]->viewangles[YAW],
				           botstates[i]->viewangles[PITCH],
				           botstates[i]->cur_ps.delta_angles[YAW] * 360.0 / 65536.0);
			}
		}
		gi.BotUserCommand(botstates[i]->client, &botstates[i]->lastucmd);
	}

	r201_tend = gi.Milliseconds();
	R201_BotBudget( r201_tend - r201_t0,
					r201_tspawn - r201_t0,
					r201_tlib   - r201_tspawn,
					r201_tupd   - r201_tlib,
					r201_treg   - r201_tupd,
					r201_tthink - r201_treg,
					r201_tend   - r201_tthink,
					r201_bots, r201_ents );

	return qtrue;
}

int BotInitLibrary(void) {
	char buf[144];

	gi.Cvar_VariableStringBuffer("sv_maxclients->integer", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "8");
	gi.BotLibVarSet("maxclients->integer", buf);
	Com_sprintf(buf, sizeof(buf), "%d", MAX_GENTITIES);
	gi.BotLibVarSet("maxentities", buf);
	gi.Cvar_VariableStringBuffer("sv_mapChecksum", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("sv_mapChecksum", buf);
	gi.Cvar_VariableStringBuffer("max_aaslinks", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("max_aaslinks", buf);
	gi.Cvar_VariableStringBuffer("max_levelitems", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("max_levelitems", buf);
	gi.Cvar_VariableStringBuffer("g_gametype", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "0");
	gi.BotLibVarSet("g_gametype", buf);
	gi.BotLibVarSet("bot_developer", bot_developer.string);
	gi.BotLibVarSet("log", buf);
	gi.Cvar_VariableStringBuffer("bot_nochat", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("nochat", "0");
	gi.Cvar_VariableStringBuffer("bot_visualizejumppads", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("bot_visualizejumppads", buf);
	gi.Cvar_VariableStringBuffer("bot_forceclustering", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("forceclustering", buf);
	gi.Cvar_VariableStringBuffer("bot_forcereachability", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("forcereachability", buf);
	gi.Cvar_VariableStringBuffer("bot_forcewrite", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("forcewrite", buf);
	gi.Cvar_VariableStringBuffer("bot_aasoptimize", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("aasoptimize", buf);
	gi.Cvar_VariableStringBuffer("bot_saveroutingcache", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("saveroutingcache", buf);
	gi.Cvar_VariableStringBuffer("bot_reloadcharacters", buf, sizeof(buf));
	if (!strlen(buf)) strcpy(buf, "0");
	gi.BotLibVarSet("bot_reloadcharacters", buf);
	gi.Cvar_VariableStringBuffer("fs_basepath", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("basedir", buf);
	gi.Cvar_VariableStringBuffer("fs_game", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("gamedir", buf);
	gi.Cvar_VariableStringBuffer("fs_cdpath", buf, sizeof(buf));
	if (strlen(buf)) gi.BotLibVarSet("cddir", buf);
#ifdef MISSIONPACKBOTTODO
	gi.BotLibDefine("MISSIONPACK");
#endif
	return gi.BotLibSetup();
}

int BotAISetup( int restart ) {
	int			errnum;

	local_time = 0;
	botlib_residual = 0;
	lastbotthink_time = 0;

	floattime = 0;
	regularupdate_time = 0;

	gi.Cvar_Register(&bot_thinktime, "bot_thinktime", "100", CVAR_CHEAT);
	gi.Cvar_Register(&bot_memorydump, "bot_memorydump", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_saveroutingcache, "bot_saveroutingcache", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_pause, "bot_pause", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_report, "bot_report", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_testsolid, "bot_testsolid", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_testclusters, "bot_testclusters", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_developer, "bot_developer", "0", CVAR_CHEAT);
	gi.Cvar_Register(&bot_interbreedchar, "bot_interbreedchar", "", 0);
	gi.Cvar_Register(&bot_interbreedbots, "bot_interbreedbots", "10", 0);
	gi.Cvar_Register(&bot_interbreedcycle, "bot_interbreedcycle", "20", 0);
	gi.Cvar_Register(&bot_interbreedwrite, "bot_interbreedwrite", "", 0);

	if (restart) {
		return qtrue;
	}

	memset( botstates, 0, sizeof(botstates) );

	errnum = BotInitLibrary();
	if (errnum != BLERR_NOERROR) return qfalse;
	return qtrue;
}

int BotAIShutdown( int restart ) {

	int i;

	if ( restart ) {
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (botstates[i] && botstates[i]->inuse) {
				BotAIShutdownClient(botstates[i]->client, restart);
			}
		}
	}
	else {
		gi.BotLibShutdown();
	}
	return qtrue;
}

