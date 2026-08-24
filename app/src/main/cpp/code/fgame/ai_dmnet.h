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


#define MAX_NODESWITCHES	50

void AIEnter_Intermission(bot_state_t *bs, char *s);
void AIEnter_Observer(bot_state_t *bs, char *s);
void AIEnter_Respawn(bot_state_t *bs, char *s);
void AIEnter_Stand(bot_state_t *bs, char *s);
void AIEnter_Seek_ActivateEntity(bot_state_t *bs, char *s);
void AIEnter_Seek_NBG(bot_state_t *bs, char *s);
void AIEnter_Seek_LTG(bot_state_t *bs, char *s);
void AIEnter_Seek_Camp(bot_state_t *bs, char *s);
void AIEnter_Battle_Fight(bot_state_t *bs, char *s);
void AIEnter_Battle_Chase(bot_state_t *bs, char *s);
void AIEnter_Battle_Retreat(bot_state_t *bs, char *s);
void AIEnter_Battle_NBG(bot_state_t *bs, char *s);
int AINode_Intermission(bot_state_t *bs);
int AINode_Observer(bot_state_t *bs);
int AINode_Respawn(bot_state_t *bs);
int AINode_Stand(bot_state_t *bs);
int AINode_Seek_ActivateEntity(bot_state_t *bs);
int AINode_Seek_NBG(bot_state_t *bs);
int AINode_Seek_LTG(bot_state_t *bs);
int AINode_Battle_Fight(bot_state_t *bs);
int AINode_Battle_Chase(bot_state_t *bs);
int AINode_Battle_Retreat(bot_state_t *bs);
int AINode_Battle_NBG(bot_state_t *bs);

void BotResetNodeSwitches(void);
void BotDumpNodeSwitches(bot_state_t *bs);

