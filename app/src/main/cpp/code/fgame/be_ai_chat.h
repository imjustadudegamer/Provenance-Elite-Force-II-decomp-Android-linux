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

#define MAX_MESSAGE_SIZE		256
#define MAX_CHATTYPE_NAME		32
#define MAX_MATCHVARIABLES		8

#define CHAT_GENDERLESS			0
#define CHAT_GENDERFEMALE		1
#define CHAT_GENDERMALE			2

#define CHAT_ALL					0
#define CHAT_TEAM					1
#define CHAT_TELL					2

typedef struct bot_consolemessage_s
{
	int handle;
	float time;									
	int type;									
	char message[MAX_MESSAGE_SIZE];				
	struct bot_consolemessage_s *prev, *next;	
} bot_consolemessage_t;

typedef struct bot_matchvariable_s
{
	char offset;
	int length;
} bot_matchvariable_t;
typedef struct bot_match_s
{
	char string[MAX_MESSAGE_SIZE];
	int type;
	int subtype;
	bot_matchvariable_t variables[MAX_MATCHVARIABLES];
} bot_match_t;

int BotSetupChatAI(void);
void BotShutdownChatAI(void);
int BotAllocChatState(void);
void BotFreeChatState(int handle);
void BotQueueConsoleMessage(int chatstate, int type, char *message);
void BotRemoveConsoleMessage(int chatstate, int handle);
int BotNextConsoleMessage(int chatstate, bot_consolemessage_t *cm);
int BotNumConsoleMessages(int chatstate);
void BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
int BotNumInitialChats(int chatstate, char *type);
int BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7);
int BotChatLength(int chatstate);
void BotEnterChat(int chatstate, int clientto, int sendto);
void BotGetChatMessage(int chatstate, char *buf, int size);
int StringContains(char *str1, char *str2, int casesensitive);
int BotFindMatch(char *str, bot_match_t *match, unsigned long int context);
void BotMatchVariable(bot_match_t *match, int variable, char *buf, int size);
void UnifyWhiteSpaces(char *string);
void BotReplaceSynonyms(char *string, unsigned long int context);
int BotLoadChatFile(int chatstate, char *chatfile, char *chatname);
void BotSetChatGender(int chatstate, int gender);
void BotSetChatName(int chatstate, char *name, int client);

