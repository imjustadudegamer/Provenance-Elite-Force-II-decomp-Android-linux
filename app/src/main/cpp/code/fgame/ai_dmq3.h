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


void BotSetupDeathmatchAI(void);
void BotShutdownDeathmatchAI(void);
void BotDeathmatchAI(bot_state_t *bs, float thinktime);
void BotFreeWaypoints(bot_waypoint_t *wp);
void BotChooseWeapon(bot_state_t *bs);
void BotSetupForMovement(bot_state_t *bs);
void BotUpdateInventory(bot_state_t *bs);
void BotUpdateBattleInventory(bot_state_t *bs, int enemy);
void BotBattleUseItems(bot_state_t *bs);
qboolean BotIsDead(bot_state_t *bs);
qboolean BotIsObserver(bot_state_t *bs);
qboolean BotIntermission(bot_state_t *bs);
qboolean BotInLavaOrSlime(bot_state_t *bs);
qboolean EntityIsDead(aas_entityinfo_t *entinfo);
qboolean EntityIsInvisible(aas_entityinfo_t *entinfo);
qboolean EntityIsShooting(aas_entityinfo_t *entinfo);
#ifdef MISSIONPACK
qboolean EntityHasKamikaze(aas_entityinfo_t *entinfo);
#endif
void BotSetUserInfo(bot_state_t *bs, char *key, char *value);
void BotSetTeamStatus(bot_state_t *bs);
char *ClientName(int client, char *name, int size);
char *EasyClientName(int client, char *name, int size);
char *ClientSkin(int client, char *skin, int size);
int BotSynonymContext(bot_state_t *bs);
int BotSetLastOrderedTask(bot_state_t *bs);
void BotTeamGoals(bot_state_t *bs, int retreat);
float BotAggression(bot_state_t *bs);
float BotFeelingBad(bot_state_t *bs);
int BotWantsToRetreat(bot_state_t *bs);
int BotWantsToChase(bot_state_t *bs);
int BotWantsToHelp(bot_state_t *bs);
int BotCanAndWantsToRocketJump(bot_state_t *bs);
int BotHasPersistantPowerupAndWeapon(bot_state_t *bs);
int BotWantsToCamp(bot_state_t *bs);
bot_moveresult_t BotAttackMove(bot_state_t *bs, int tfl);
int BotSameTeam(bot_state_t *bs, int entnum);
int TeamPlayIsOn(void);
int BotTeamFlagCarrier(bot_state_t *bs);
int BotTeamFlagCarrierVisible(bot_state_t *bs);
int BotEnemyFlagCarrierVisible(bot_state_t *bs);
void BotVisibleTeamMatesAndEnemies(bot_state_t *bs, int *teammates, int *enemies, float range);
qboolean InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles);
int BotFindEnemy(bot_state_t *bs, int curenemy);
void BotRoamGoal(bot_state_t *bs, vec3_t goal);
float BotEntityVisible(int viewer, vec3_t eye, vec3_t viewangles, float fov, int ent);
void BotAimAtEnemy(bot_state_t *bs);
void BotCheckAttack(bot_state_t *bs);
void BotAIBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate);
int BotAIPredictObstacles(bot_state_t *bs, bot_goal_t *goal);
void BotEnableActivateGoalAreas(bot_activategoal_t *activategoal, int enable);
int BotPopFromActivateGoalStack(bot_state_t *bs);
void BotClearActivateGoalStack(bot_state_t *bs);
int BotTeam(bot_state_t *bs);
int BotOppositeTeam(bot_state_t *bs);
int BotCTFCarryingFlag(bot_state_t *bs);
void BotRememberLastOrderedTask(bot_state_t *bs);
void BotCTFSeekGoals(bot_state_t *bs);
void BotCTFRetreatGoals(bot_state_t *bs);
#ifdef MISSIONPACK
int Bot1FCTFCarryingFlag(bot_state_t *bs);
int BotHarvesterCarryingCubes(bot_state_t *bs);
void Bot1FCTFSeekGoals(bot_state_t *bs);
void Bot1FCTFRetreatGoals(bot_state_t *bs);
void BotObeliskSeekGoals(bot_state_t *bs);
void BotObeliskRetreatGoals(bot_state_t *bs);
void BotGoHarvest(bot_state_t *bs);
void BotHarvesterSeekGoals(bot_state_t *bs);
void BotHarvesterRetreatGoals(bot_state_t *bs);
int BotTeamCubeCarrierVisible(bot_state_t *bs);
int BotEnemyCubeCarrierVisible(bot_state_t *bs);
#endif
int BotGetAlternateRouteGoal(bot_state_t *bs, int base);
bot_goal_t *BotAlternateRoute(bot_state_t *bs, bot_goal_t *goal);
bot_waypoint_t *BotCreateWayPoint(char *name, vec3_t origin, int areanum);
bot_waypoint_t *BotFindWayPoint(bot_waypoint_t *waypoints, char *name);
char *stristr(char *str, char *charset);
int ClientFromName(char *name);
int ClientOnSameTeamFromName(bot_state_t *bs, char *name);
int BotPointAreaNum(vec3_t origin);
void BotMapScripts(bot_state_t *bs);

#define CTF_FLAG_NONE		0
#define CTF_FLAG_RED		1
#define CTF_FLAG_BLUE		2
#define CTF_SKIN_REDTEAM	"red"
#define CTF_SKIN_BLUETEAM	"blue"

extern int gametype;		

extern vmCvar_t bot_grapple;
extern vmCvar_t bot_rocketjump;
extern vmCvar_t bot_fastchat;
extern vmCvar_t bot_nochat;
extern vmCvar_t bot_testrchat;
extern vmCvar_t bot_challenge;
extern vmCvar_t bot_showstates;

extern bot_goal_t ctf_redflag;
extern bot_goal_t ctf_blueflag;
#ifdef MISSIONPACK
extern bot_goal_t ctf_neutralflag;
extern bot_goal_t redobelisk;
extern bot_goal_t blueobelisk;
extern bot_goal_t neutralobelisk;
#endif
