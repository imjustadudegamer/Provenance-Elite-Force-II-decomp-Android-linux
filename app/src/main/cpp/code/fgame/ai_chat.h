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


int BotChat_EnterGame(bot_state_t *bs);
int BotChat_ExitGame(bot_state_t *bs);
int BotChat_StartLevel(bot_state_t *bs);
int BotChat_EndLevel(bot_state_t *bs);
int BotChat_HitTalking(bot_state_t *bs);
int BotChat_HitNoDeath(bot_state_t *bs);
int BotChat_HitNoKill(bot_state_t *bs);
int BotChat_Death(bot_state_t *bs);
int BotChat_Kill(bot_state_t *bs);
int BotChat_EnemySuicide(bot_state_t *bs);
int BotChat_Random(bot_state_t *bs);
float BotChatTime(bot_state_t *bs);
int BotValidChatPosition(bot_state_t *bs);
void BotChatTest(bot_state_t *bs);

