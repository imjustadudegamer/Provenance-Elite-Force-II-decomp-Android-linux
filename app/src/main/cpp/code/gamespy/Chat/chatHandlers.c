
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "chatMain.h"
#include "chatSocket.h"
#include "chatHandlers.h"
#include "chatChannel.h"
#include "chatCallbacks.h"
#include "chatCrypt.h"

#if defined(_WIN32)
#pragma warning(disable:4054)  

#pragma warning(disable:4127)
#endif

#define FILTER_TIMEOUT        60000
#define NAMES_ARRAY_INC       100

#define ASSERT_TYPE(type)     assert((type >= 0) && (type < NUM_TYPES));
#define ASSERT_STR(str)       assert(str != NULL); assert(str[0] != '\0');

#define RPL_WELCOME                 "001"
#define RPL_USRIP                   "302"
#define RPL_WHOISUSER               "311"
#define RPL_ENDOFWHO                "315"
#define RPL_ENDOFWHOIS              "318"
#define RPL_WHOISCHANNELS           "319"
#define RPL_LISTSTART               "321"
#define RPL_LIST                    "322"
#define RPL_LISTEND                 "323"
#define RPL_CHANNELMODEIS           "324"
#define RPL_NOTOPIC                 "331"
#define RPL_TOPIC                   "332"
#define RPL_WHOREPLY                "352"
#define RPL_NAMEREPLY               "353"
#define RPL_ENDOFNAMES              "366"
#define RPL_BANLIST                 "367"
#define RPL_ENDOFBANLIST            "368"
#define RPL_GETKEY                  "700"
#define RPL_ENDGETKEY               "701"
#define RPL_GETCKEY                 "702"
#define RPL_ENDGETCKEY              "703"
#define RPL_GETCHANKEY              "704"
#define RPL_SECUREKEY               "705"
#define RPL_CDKEY                   "706"
#define RPL_LOGIN                   "707"
#define RPL_GETUDPRELAY				"712"

#define ERR_NOSUCHNICK              "401"
#define ERR_NOSUCHCHANNEL           "403"
#define ERR_TOOMANYCHANNELS         "405"
#define ERR_ERRONEUSNICKNAME        "432"
#define ERR_NICKNAMEINUSE           "433"
#define ERR_CHANNELISFULL           "471"
#define ERR_INVITEONLYCHAN          "473"
#define ERR_BANNEDFROMCHAN          "474"
#define ERR_BADCHANNELKEY           "475"
#define ERR_BADCHANMASK             "476"
#define ERR_LOGIN_FAILED            "708"
#define ERR_NO_UNIQUE_NICK          "709"
#define ERR_UNIQUE_NICK_EXPIRED     "710"
#define ERR_REGISTER_NICK_FAILED    "711"

#define MODE_END                        0
#define MODE_BAN                        1
#define MODE_INVITE_ONLY                2
#define MODE_LIMIT                      3
#define MODE_PRIVATE                    4
#define MODE_SECRET                     5
#define MODE_KEY                        6
#define MODE_MODERATED                  7
#define MODE_NO_EXTERNAL_MESSAGES       8
#define MODE_ONLY_OPS_CHANGE_TOPIC      9
#define MODE_OP                         10
#define MODE_VOICE                      11
#define MODE_USERS_HIDDEN               12
#define MODE_RECEIVE_WALLOPS            13
#define MODE_OPS_OBEY_CHANNEL_LIMIT     14


#define FINISH_FILTER          ciFinishFilter(chat, filter, &params)
#define IS_ALPHA(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))

enum
{
	TYPE_LIST,
	TYPE_JOIN,
	TYPE_TOPIC,
	TYPE_NAMES,
	TYPE_WHOIS,
	TYPE_CMODE,
	TYPE_UMODE,
	TYPE_BAN,
	TYPE_GETBAN,
	TYPE_NICK,
	TYPE_WHO,
	TYPE_CWHO,
	TYPE_GETKEY,
	TYPE_GETCKEY,
	TYPE_GETCHANKEY,
	TYPE_UNQUIET,
	TYPE_CDKEY,
	TYPE_GETUDPRELAY,
	NUM_TYPES
};

typedef struct ciModeChange
{
	int mode;
	CHATBool enable;
	char * param;
} ciModeChange;

typedef struct ciFilterMatch
{
	int type;
	const char * name;
	const char * name2;
} ciFilterMatch;

typedef struct LISTData
{
	CHATBool gotStart;
	int numChannels;
	char ** channels;
	int * numUsers;
	char ** topics;
} LISTData;

typedef struct JOINData
{
	chatChannelCallbacks callbacks;
	CHATBool joined;
	char password[MAX_PASSWORD];
} JOINData;

typedef struct NAMESData
{
	int len;
	int numUsers;
	char ** operators;
	int * modes;
} NAMESData;

typedef struct WHOISData
{
	char * user;
	char * name;
	char * address;
	int numChannels;
	char ** channels;
} WHOISData;

typedef struct BANData
{
	char * channel;
} BANData;

typedef struct GETBANData
{
	int numBans;
	char ** bans;
} GETBANData;

typedef struct GETKEYData
{
	int num;
	char ** keys;
	char * channel;
} GETKEYData;

typedef struct GETCKEYData
{
	int num;
	char ** keys;
	CHATBool channel;
	CHATBool allBroadcastKeys;
} GETCKEYData;

typedef struct GETCHANKEYData
{
	int num;
	char ** keys;
	CHATBool allBroadcastKeys;
} GETCHANKEYData;

void ciPrivmsgHandler(CHAT chat, const ciServerMessage * message);
void ciNoticeHandler(CHAT chat, const ciServerMessage * message);
void ciUTMHandler(CHAT chat, const ciServerMessage * message);
void ciATMHandler(CHAT chat, const ciServerMessage * message);
void ciPingHandler(CHAT chat, const ciServerMessage * message);
void ciNickHandler(CHAT chat, const ciServerMessage * message);
void ciJoinHandler(CHAT chat, const ciServerMessage * message);
void ciPartHandler(CHAT chat, const ciServerMessage * message);
void ciKickHandler(CHAT chat, const ciServerMessage * message);
void ciQuitHandler(CHAT chat, const ciServerMessage * message);
void ciKillHandler(CHAT chat, const ciServerMessage * message);
void ciTopicHandler(CHAT chat, const ciServerMessage * message);
void ciModeHandler(CHAT chat, const ciServerMessage * message);
void ciErrorHandler(CHAT chat, const ciServerMessage * message);
void ciNameReplyHandler(CHAT chat, const ciServerMessage * message);
void ciEndOfNamesHandler(CHAT chat, const ciServerMessage * message);
void ciInviteHandler(CHAT chat, const ciServerMessage * message);

void ciRplTopicHandler(CHAT chat, const ciServerMessage * message);
void ciRplNoTopicHandler(CHAT chat, const ciServerMessage * message);
void ciErrNickInUseHandler(CHAT chat, const ciServerMessage * message);
void ciRplWhoReplyHandler(CHAT chat, const ciServerMessage * message);
void ciRplUserIPHandler(CHAT chat, const ciServerMessage * message);
void ciRplListStartHandler(CHAT chat, const ciServerMessage * message);
void ciRplListHandler(CHAT chat, const ciServerMessage * message);
void ciRplListEndHandler(CHAT chat, const ciServerMessage * message);
void ciRplChannelModeIsHandler(CHAT chat, const ciServerMessage * message);
void ciRplWhoisUserHandler(CHAT chat, const ciServerMessage * message);
void ciRplWhoisChannelsHandler(CHAT chat, const ciServerMessage * message);
void ciRplEndOfWhoisHandler(CHAT chat, const ciServerMessage * message);
void ciRplBanListHandler(CHAT chat, const ciServerMessage * message);
void ciRplEndOfBanListHandler(CHAT chat, const ciServerMessage * message);
void ciRplWelcomeHandler(CHAT chat, const ciServerMessage * message);
void ciRplEndOfWhoHandler(CHAT chat, const ciServerMessage * message);
void ciRplGetKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplEndGetKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplGetCKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplEndGetCKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplGetChanKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplSecureKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplCDKeyHandler(CHAT chat, const ciServerMessage * message);
void ciRplLoginHandler(CHAT chat, const ciServerMessage * message);
void ciRplGetUdpRelayHandler(CHAT chat, const ciServerMessage * message);

void ciErrNoSuchChannelHandler(CHAT chat, const ciServerMessage * message);
void ciErrTooManyChannelsHandler(CHAT chat, const ciServerMessage * message);
void ciErrChannelIsFullHandler(CHAT chat, const ciServerMessage * message);
void ciErrInviteOnlyChanHandler(CHAT chat, const ciServerMessage * message);
void ciErrBannedFromChanHandler(CHAT chat, const ciServerMessage * message);
void ciErrBadChannelKeyHandler(CHAT chat, const ciServerMessage * message);
void ciErrBadChanMaskHandler(CHAT chat, const ciServerMessage * message);
void ciErrNoSuchNickHandler(CHAT chat, const ciServerMessage * message);
void ciErrErroneusNicknameHandler(CHAT chat, const ciServerMessage * message);
void ciErrLoginFailedHandler(CHAT chat, const ciServerMessage * message);
void ciErrNoUniqueNickHandler(CHAT chat, const ciServerMessage * message);
void ciErrUniqueNickExpiredHandler(CHAT chat, const ciServerMessage * message);
void ciErrRegisterNickFailedHandler(CHAT chat, const ciServerMessage * message);

ciServerMessageType serverMessageTypes[] = 
{
	{ "PRIVMSG", ciPrivmsgHandler },
	{ "NOTICE",  ciNoticeHandler },
	{ "UTM",     ciUTMHandler },
	{ "ATM",     ciATMHandler },
	{ "PING",    ciPingHandler },
	{ "NICK",    ciNickHandler },
	{ "JOIN",    ciJoinHandler },
	{ "PART",    ciPartHandler },
	{ "KICK",    ciKickHandler },
	{ "QUIT",    ciQuitHandler },
	{ "KILL",    ciKillHandler },
	{ "TOPIC",   ciTopicHandler },
	{ "MODE",    ciModeHandler },
	{ "ERROR",   ciErrorHandler },
	{ "INVITE",  ciInviteHandler },

	{ RPL_NAMEREPLY,     ciNameReplyHandler },
	{ RPL_ENDOFNAMES,    ciEndOfNamesHandler },
	{ RPL_TOPIC,         ciRplTopicHandler },
	{ RPL_NOTOPIC,       ciRplNoTopicHandler },
	{ RPL_WHOREPLY,      ciRplWhoReplyHandler },
	{ RPL_USRIP,         ciRplUserIPHandler },
	{ RPL_LISTSTART,     ciRplListStartHandler },
	{ RPL_LIST,          ciRplListHandler },
	{ RPL_LISTEND,       ciRplListEndHandler },
	{ RPL_CHANNELMODEIS, ciRplChannelModeIsHandler },
	{ RPL_WHOISUSER,     ciRplWhoisUserHandler },
	{ RPL_WHOISCHANNELS, ciRplWhoisChannelsHandler },
	{ RPL_ENDOFWHOIS,    ciRplEndOfWhoisHandler },
	{ RPL_BANLIST,       ciRplBanListHandler },
	{ RPL_ENDOFBANLIST,  ciRplEndOfBanListHandler },
	{ RPL_WELCOME,       ciRplWelcomeHandler },
	{ RPL_ENDOFWHO,      ciRplEndOfWhoHandler },
	{ RPL_GETKEY,        ciRplGetKeyHandler },
	{ RPL_ENDGETKEY,     ciRplEndGetKeyHandler },
	{ RPL_GETCKEY,       ciRplGetCKeyHandler },
	{ RPL_ENDGETCKEY,    ciRplEndGetCKeyHandler },
	{ RPL_GETCHANKEY,    ciRplGetChanKeyHandler },
	{ RPL_SECUREKEY,     ciRplSecureKeyHandler },
	{ RPL_CDKEY,         ciRplCDKeyHandler },
	{ RPL_LOGIN,         ciRplLoginHandler },
	{ RPL_GETUDPRELAY,   ciRplGetUdpRelayHandler },

	{ ERR_NICKNAMEINUSE,          ciErrNickInUseHandler },
	{ ERR_NOSUCHCHANNEL,          ciErrNoSuchChannelHandler },
	{ ERR_TOOMANYCHANNELS,        ciErrTooManyChannelsHandler },
	{ ERR_CHANNELISFULL,          ciErrChannelIsFullHandler },
	{ ERR_INVITEONLYCHAN,         ciErrInviteOnlyChanHandler },
	{ ERR_BANNEDFROMCHAN,         ciErrBannedFromChanHandler },
	{ ERR_BADCHANNELKEY,          ciErrBadChannelKeyHandler },
	{ ERR_BADCHANMASK,            ciErrBadChanMaskHandler },
	{ ERR_NOSUCHNICK,             ciErrNoSuchNickHandler },
	{ ERR_ERRONEUSNICKNAME,       ciErrErroneusNicknameHandler },
	{ ERR_LOGIN_FAILED,           ciErrLoginFailedHandler },
	{ ERR_NO_UNIQUE_NICK,         ciErrNoUniqueNickHandler },
	{ ERR_UNIQUE_NICK_EXPIRED,    ciErrUniqueNickExpiredHandler },
	{ ERR_REGISTER_NICK_FAILED,   ciErrRegisterNickFailedHandler }
};
int numServerMessageTypes = (sizeof(serverMessageTypes) / sizeof(ciServerMessageType));


static ciServerMessageFilter * ciFindFilter(CHAT chat, int numMatches, ciFilterMatch * matches)
{
	int i;
	const char * name;
	const char * name2;
	ciServerMessageFilter * filter;
	CONNECTION;

	assert(numMatches > 0);
	assert(matches);

	for(filter = connection->filterList ; filter != NULL ; filter = filter->pnext)
	{
		for(i = 0 ; i < numMatches ; i++)
		{
			ASSERT_TYPE(matches[i].type);
			if(filter->type == matches[i].type)
			{
				name = matches[i].name;
				name2 = matches[i].name2;
				if((!name && !filter->name) || (name && filter->name && (strcasecmp(name, filter->name) == 0)))
				{
					if((!name2 && !filter->name2) || (name2 && filter->name2 && (strcasecmp(name2, filter->name2) == 0)))
					{
						filter->timeout = (current_time() + FILTER_TIMEOUT);

						return filter;
					}
				}
			}
		}
	}

	return NULL;
}

static ciServerMessageFilter * ciFindGetKeyFilter(CHAT chat, const char * channel)
{
	GETKEYData * data;
	ciServerMessageFilter * filter;
	CONNECTION;

	assert(channel);
	assert(channel[0]);

	for(filter = connection->filterList ; filter != NULL ; filter = filter->pnext)
	{
		if(filter->type == TYPE_GETKEY)
		{
			data = (GETKEYData *)filter->data;
			if(strcasecmp(data->channel, channel) == 0)
				return filter;
		}
	}

	return NULL;
}

static void ciDestroyFilter(ciServerMessageFilter * filter)
{
	assert(filter != NULL);

	gsifree(filter->data);
	gsifree(filter->name);
	gsifree(filter->name2);
	gsifree(filter);
}

static void ciRemoveFilter(CHAT chat, ciServerMessageFilter * filter)
{
	ciServerMessageFilter * pcurr;
	ciServerMessageFilter * pprev = NULL;
	CONNECTION;

	assert(filter != NULL);

	for(pcurr = connection->filterList ; pcurr != NULL ; pcurr = pcurr->pnext)
	{
		if(pcurr == filter)
		{
			if(connection->filterList == pcurr)
				connection->filterList = pcurr->pnext;

			if(connection->lastFilter == pcurr)
				connection->lastFilter = pprev;

			if(pprev != NULL)
				pprev->pnext = pcurr->pnext;

			ciDestroyFilter(pcurr);

			return;
		}

		pprev = pcurr;
	}

}

static void ciFinishFilter(CHAT chat, ciServerMessageFilter * filter, void * params)
{
	int i;

	assert(filter);
	ASSERT_TYPE(filter->type);

	if(filter->type == TYPE_LIST)
	{
		LISTData * data = (LISTData *)filter->data;
		
		if(filter->callback2)
			ciAddCallback_(chat, CALLBACK_ENUM_CHANNELS_ALL, filter->callback2, params, filter->param, filter->ID, NULL, sizeof(ciCallbackEnumChannelsAllParams));

		for(i = 0 ; i < data->numChannels ; i++)
		{
			gsifree(data->channels[i]);
			gsifree(data->topics[i]);
		}
		gsifree(data->channels);
		gsifree(data->topics);
		gsifree(data->numUsers);
	}
	else if(filter->type == TYPE_JOIN)
	{
		if(filter->callback != NULL)
			ciAddCallback_(chat, CALLBACK_ENTER_CHANNEL, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackEnterChannelParams));
	}
	else if(filter->type == TYPE_TOPIC)
	{
		const char * channel = ((ciCallbackGetChannelTopicParams *)params)->channel;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_CHANNEL_TOPIC, filter->callback, params, filter->param, filter->ID, channel, sizeof(ciCallbackGetChannelTopicParams));
	}
	else if(filter->type == TYPE_NAMES)
	{
		NAMESData * data = (NAMESData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_ENUM_USERS, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackEnumUsersParams));

		for(i = 0 ; i < data->numUsers ; i++)
			gsifree(data->operators[i]);
		gsifree(data->operators);
		gsifree(data->modes);
	}
	else if(filter->type == TYPE_WHOIS)
	{
		WHOISData * data = (WHOISData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_USER_INFO, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetUserInfoParams));

		for(i = 0 ; i < data->numChannels ; i++)
			gsifree(data->channels[i]);
		gsifree(data->channels);
		gsifree(data->name);
		gsifree(data->address);
		gsifree(data->user);
	}
	else if(filter->type == TYPE_WHO)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_BASIC_USER_INFO, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetBasicUserInfoParams));
	}
	else if(filter->type == TYPE_CWHO)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_CHANNEL_BASIC_USER_INFO, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetBasicUserInfoParams));
	}
	else if(filter->type == TYPE_CMODE)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_CHANNEL_MODE, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetChannelModeParams));
	}
	else if(filter->type == TYPE_UMODE)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_USER_MODE, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetUserModeParams));
	}
	else if(filter->type == TYPE_GETUDPRELAY)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_UDPRELAY, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetUdpRelayParams));
	}
	else if(filter->type == TYPE_BAN)
	{
		BANData * data = (BANData *)filter->data;

		gsifree(data->channel);
	}
	else if(filter->type == TYPE_GETBAN)
	{
		GETBANData * data = (GETBANData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_ENUM_CHANNEL_BANS, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackEnumChannelBansParams));

		for(i = 0 ; i < data->numBans ; i++)
			gsifree(data->bans[i]);
	}
	else if(filter->type == TYPE_NICK)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_CHANGE_NICK, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackChangeNickParams));
	}
	else if(filter->type == TYPE_GETKEY)
	{
		GETKEYData * data = (GETKEYData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_GLOBAL_KEYS, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetGlobalKeysParams));

		gsifree(data->channel);
		for(i = 0 ; i < data->num ; i++)
			gsifree(data->keys[i]);
		gsifree(data->keys);
	}
	else if(filter->type == TYPE_GETCKEY)
	{
		GETCKEYData * data = (GETCKEYData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_CHANNEL_KEYS, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetChannelKeysParams));

		for(i = 0 ; i < data->num ; i++)
			gsifree(data->keys[i]);
		gsifree(data->keys);
	}
	else if(filter->type == TYPE_GETCHANKEY)
	{
		GETCHANKEYData * data = (GETCHANKEYData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_GET_CHANNEL_KEYS, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackGetChannelKeysParams));

		for(i = 0 ; i < data->num ; i++)
			gsifree(data->keys[i]);
		gsifree(data->keys);
	}
	else if(filter->type == TYPE_UNQUIET)
	{
		NAMESData * data = (NAMESData *)filter->data;

		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_NEW_USER_LIST, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackNewUserListParams));

		for(i = 0 ; i < data->numUsers ; i++)
			gsifree(data->operators[i]);
		gsifree(data->operators);
		gsifree(data->modes);
	}
	else if(filter->type == TYPE_CDKEY)
	{
		if(filter->callback)
			ciAddCallback_(chat, CALLBACK_AUTHENTICATE_CDKEY, filter->callback, params, filter->param, filter->ID, NULL, sizeof(ciCallbackAuthenticateCDKeyParams));
	}
	else
	{
		assert(0);
	}

	ciRemoveFilter(chat, filter);
}

static void ciFilterTimedout(CHAT chat, ciServerMessageFilter * filter)
{
	assert(filter);
	ASSERT_TYPE(filter->type);
	
	if(filter->type == TYPE_LIST)
	{
		ciCallbackEnumChannelsAllParams params;
		params.success = CHATFalse;
		params.numChannels = 0;
		params.channels = NULL;
		params.topics = NULL;
		params.numUsers = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_JOIN)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATEnterTimedOut;
		params.channel = filter->name;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_TOPIC)
	{
		ciCallbackGetChannelTopicParams params;
		params.success = CHATFalse;
		params.channel = filter->name;
		params.topic = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_NAMES)
	{
		ciCallbackEnumUsersParams params;
		params.success = CHATFalse;
		params.channel = filter->name;
		params.numUsers = 0;
		params.operators = NULL;
		params.modes = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_WHOIS)
	{
		ciCallbackGetUserInfoParams params;
		params.success = CHATFalse;
		params.nick = filter->name;
		params.user = NULL;
		params.name = NULL;
		params.address = NULL;
		params.numChannels = 0;
		params.channels = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_WHO)
	{
		ciCallbackGetBasicUserInfoParams params;
		params.success = CHATFalse;
		params.nick = filter->name;
		params.user = NULL;
		params.address = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_CWHO)
	{
		ciCallbackGetChannelBasicUserInfoParams params;
		params.success = CHATFalse;
		params.channel = filter->name;
		params.nick = NULL;
		params.user = NULL;
		params.address = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_CMODE)
	{
		ciCallbackGetChannelModeParams params;
		params.success = CHATFalse;
		params.channel = filter->name;
		params.mode = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_UMODE)
	{
		ciCallbackGetUserModeParams params;
		params.success = CHATFalse;
		params.channel = filter->name2;
		params.user = filter->name;
		params.mode = 0;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_BAN)
	{
		ciFinishFilter(chat, filter, NULL);
	}
	else if(filter->type == TYPE_GETBAN)
	{
		ciCallbackEnumChannelBansParams params;
		params.channel = filter->name;
		params.numBans = 0;
		params.bans = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_NICK)
	{
		ciCallbackChangeNickParams params;
		params.success = CHATFalse;
		params.oldNick = filter->name;
		params.newNick = filter->name2;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_GETKEY)
	{
		ciCallbackGetGlobalKeysParams params;
		params.success = CHATFalse;
		params.user = NULL;
		params.num = 0;
		params.keys = NULL;
		params.values = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_GETCKEY)
	{
		ciCallbackGetChannelKeysParams params;
		params.success = CHATFalse;
		params.channel = NULL;
		params.user = NULL;
		params.num = 0;
		params.keys = NULL;
		params.values = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_GETCHANKEY)
	{
		ciCallbackGetChannelKeysParams params;
		params.success = CHATFalse;
		params.channel = NULL;
		params.user = NULL;
		params.num = 0;
		params.keys = NULL;
		params.values = NULL;

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_UNQUIET)
	{
		ciRemoveFilter(chat, filter);
	}
	else if(filter->type == TYPE_CDKEY)
	{
		ciCallbackAuthenticateCDKeyParams params;
		params.result = 0;
		params.message = "Timed out";

		FINISH_FILTER;
	}
	else if(filter->type == TYPE_GETUDPRELAY)
	{
		ciCallbackGetUdpRelayParams params;
		params.channel = filter->name;
		params.udpIp = NULL;
		params.udpKey = 0;
		params.udpPort = 0;

		FINISH_FILTER;
	}
	else
	{
		assert(0);
	}
}

void ciFilterThink(CHAT chat)
{
	ciServerMessageFilter * filter;
	ciServerMessageFilter * pnext;
	gsi_time now;
	CONNECTION;

	now = 0;
	now = 2;
	now = current_time();

	for(filter = connection->filterList ; filter != NULL ; filter = pnext)
	{
		pnext = filter->pnext;
		if(now > filter->timeout)
		{
			ciFilterTimedout(chat, filter);
		}
	}
}

int ciGetNextID(CHAT chat)
{
	int rcode;
	CONNECTION;

	rcode = connection->nextID;

	if(connection->nextID == INT_MAX)
		connection->nextID = 1;
	else
		connection->nextID++;

	return rcode;
}

CHATBool ciCheckFiltersForID(CHAT chat, int ID)
{
	ciServerMessageFilter * filter;
	CONNECTION;

	assert(ID > 0);

	for(filter = connection->filterList ; filter != NULL ; filter = filter->pnext)
	{
		if(filter->ID == ID)
			return CHATTrue;
	}

	return CHATFalse;
}

void ciCleanupFilters(CHAT chat)
{
	CONNECTION;

	while(connection->filterList)
		ciFilterTimedout(chat, connection->filterList);  
}

static int ciAddFilter(CHAT chat, int type, const char * name, const char * name2, void * callback, void * callback2, void * param, void * data)
{
	ciServerMessageFilter * filter;
	CONNECTION;

	filter = (ciServerMessageFilter *)gsimalloc(sizeof(ciServerMessageFilter));
	if(filter == NULL)
		return 0; 

	memset(filter, 0, sizeof(ciServerMessageFilter));
	filter->type = type;
	filter->timeout = (current_time() + FILTER_TIMEOUT);
	filter->callback = callback;
	filter->callback2 = callback2;
	filter->param = param;
	filter->data = data;
	if(name)
		filter->name = goastrdup(name);
	else
		filter->name = NULL;
	if(name2)
		filter->name2 = goastrdup(name2);
	else
		filter->name2 = NULL;
	filter->ID = ciGetNextID(chat);

	if(connection->filterList == NULL)
		connection->filterList = filter;
	else
		connection->lastFilter->pnext = filter;
	connection->lastFilter = filter;

	return filter->ID;
}

int ciAddLISTFilter(CHAT chat, chatEnumChannelsCallbackEach callbackEach, chatEnumChannelsCallbackAll callbackAll, void * param)
{
	LISTData * data = (LISTData *)gsimalloc(sizeof(LISTData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(LISTData));

	return ciAddFilter(chat, TYPE_LIST, NULL, NULL, (void*)callbackEach, (void*)callbackAll, param, data);
}

int ciAddJOINFilter(CHAT chat, const char * channel, chatEnterChannelCallback callback, void * param, chatChannelCallbacks * callbacks, const char * password)
{
	int rcode;
	JOINData * data;

	assert(password != NULL);
	assert(strlen(password) < MAX_PASSWORD);

	data = (JOINData *)gsimalloc(sizeof(JOINData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(JOINData));
	data->callbacks = *callbacks;
	strzcpy(data->password, password, MAX_PASSWORD);
	
	rcode = ciAddFilter(chat, TYPE_JOIN, channel, NULL, (void*)callback, NULL, param, data);
	if(rcode == 0)
		gsifree(data);

	return rcode;
}

int ciAddTOPICFilter(CHAT chat, const char * channel, chatGetChannelTopicCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_TOPIC, channel, NULL, (void*)callback, NULL, param, NULL);
}

int ciAddNAMESFilter(CHAT chat, const char * channel, chatEnumUsersCallback callback, void * param)
{
	NAMESData * data = (NAMESData *)gsimalloc(sizeof(NAMESData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(NAMESData));

	return ciAddFilter(chat, TYPE_NAMES, channel, NULL, (void*)callback, NULL, param, data);
}

int ciAddWHOISFilter(CHAT chat, const char * user, chatGetUserInfoCallback callback, void * param)
{
	int rcode;
	WHOISData * data = (WHOISData *)gsimalloc(sizeof(WHOISData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(WHOISData));

	rcode = ciAddFilter(chat, TYPE_WHOIS, user, NULL, (void*)callback, NULL, param, data);
	if(rcode == 0)
		gsifree(data);
	return rcode;
}

int ciAddWHOFilter(CHAT chat, const char * user, chatGetBasicUserInfoCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_WHO, user, NULL, (void*)callback, NULL, param, NULL);
}

int ciAddCWHOFilter(CHAT chat, const char * channel, chatGetChannelBasicUserInfoCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_CWHO, channel, NULL, (void*)callback, NULL, param, NULL);
}

int ciAddCMODEFilter(CHAT chat, const char * channel, chatGetChannelModeCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_CMODE, channel, NULL, (void*)callback, NULL, param, NULL);
}

int ciAddUMODEFilter(CHAT chat, const char * user, const char * channel, chatGetUserModeCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_UMODE, user, channel, (void*)callback, NULL, param, NULL);
}

int ciAddGETUDPRELAYFilter(CHAT chat, const char * channel, chatGetUdpRelayCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_GETUDPRELAY, channel, NULL, (void*)callback, NULL, param, NULL);
}


int ciAddBANFilter(CHAT chat, const char * user, const char * channel)
{
	BANData * data = (BANData *)gsimalloc(sizeof(BANData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(BANData));
	data->channel = goastrdup(channel);
	if(data->channel == NULL)
	{
		gsifree(data);
		return 0; 
	}

	return ciAddFilter(chat, TYPE_BAN, user, NULL, NULL, NULL, NULL, data);
}

int ciAddGETBANFilter(CHAT chat, const char * channel, chatEnumChannelBansCallback callback, void * param)
{
	GETBANData * data = (GETBANData *)gsimalloc(sizeof(GETBANData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(GETBANData));

	return ciAddFilter(chat, TYPE_GETBAN, channel, NULL, (void*)callback, NULL, param, data);
}

int ciAddNICKFilter(CHAT chat, const char * oldNick, const char * newNick, chatChangeNickCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_NICK, oldNick, newNick, (void*)callback, NULL, param, NULL);
}

int ciAddGETKEYFilter(CHAT chat, const char * cookie, int num, const char ** keys, const char * channel, chatGetGlobalKeysCallback callback, void * param)
{
	int i;
	GETKEYData * data = (GETKEYData *)gsimalloc(sizeof(GETKEYData));
	if(!data)
		return 0;
	memset(data, 0, sizeof(GETKEYData));

	data->num = num;
	if(channel)
	{
		data->channel = goastrdup(channel);
		if(!data->channel)
		{
			gsifree(data);
			return 0;
		}
	}
	data->keys = (char **)gsimalloc(sizeof(char *) * num);
	if(!data->keys)
	{
		gsifree(data->channel);
		gsifree(data);
		return 0;
	}
	for(i = 0 ; i < num ; i++)
	{
		data->keys[i] = goastrdup(keys[i]);
		if(!data->keys[i])
		{
			for( i-- ; i >= 0 ; i--)
				gsifree(data->keys[i]);
			gsifree(data->keys);
			gsifree(data->channel);
			gsifree(data);
			return 0;
		}
	}

	return ciAddFilter(chat, TYPE_GETKEY, cookie, NULL, (void*)callback, NULL, param, data);
}

int ciAddGETCKEYFilter(CHAT chat, const char * cookie, int num, const char ** keys, CHATBool channel, CHATBool getBroadcastKeys, chatGetChannelKeysCallback callback, void * param)
{
	int src;
	int dest;
	GETCKEYData * data = (GETCKEYData *)gsimalloc(sizeof(GETCKEYData));
	if(!data)
		return 0;
	memset(data, 0, sizeof(GETCKEYData));
	data->allBroadcastKeys = getBroadcastKeys;

	data->num = num;
	data->channel = channel;
	if(getBroadcastKeys)
		data->num--;
	data->keys = (char **)gsimalloc(sizeof(char *) * data->num);
	if(!data->keys)
	{
		gsifree(data);
		return 0;
	}
	for(src = 0, dest = 0 ; src < num ; src++)
	{
		if(strcmp(keys[src], "b_*") != 0)
		{
			data->keys[dest] = goastrdup(keys[src]);
			if(!data->keys[dest])
			{
				for( dest-- ; dest >= 0 ; dest--)
					gsifree(data->keys[dest]);
				gsifree(data->keys);
				gsifree(data);
				return 0;
			}
			dest++;
		}
	}
	data->num = dest;

	return ciAddFilter(chat, TYPE_GETCKEY, cookie, NULL, (void*)callback, NULL, param, data);
}

int ciAddGETCHANKEYFilter(CHAT chat, const char * cookie, int num, const char ** keys, CHATBool getBroadcastKeys, chatGetChannelKeysCallback callback, void * param)
{
	int src;
	int dest;
	GETCHANKEYData * data = (GETCHANKEYData *)gsimalloc(sizeof(GETCHANKEYData));
	if(!data)
		return 0;
	memset(data, 0, sizeof(GETCHANKEYData));
	data->allBroadcastKeys = getBroadcastKeys;

	data->num = num;
	if(getBroadcastKeys)
		data->num--;
	if(data->num)
	{
		data->keys = (char **)gsimalloc(sizeof(char *) * data->num);
		if(!data->keys)
		{
			gsifree(data);
			return 0;
		}
		for(src = 0, dest = 0 ; src < num ; src++)
		{
			if(strcmp(keys[src], "b_*") != 0)
			{
				data->keys[dest] = goastrdup(keys[src]);
				if(!data->keys[dest])
				{
					for( dest-- ; dest >= 0 ; dest--)
						gsifree(data->keys[dest]);
					gsifree(data->keys);
					gsifree(data);
					return 0;
				}
				dest++;
			}
		}
		data->num = dest;
	}

	return ciAddFilter(chat, TYPE_GETCHANKEY, cookie, NULL, (void*)callback, NULL, param, data);
}

int ciAddUNQUIETFilter(CHAT chat, const char * channel)
{
	chatChannelCallbacks * callbacks;
	NAMESData * data;

	callbacks = ciGetChannelCallbacks(chat, channel);
	if(!callbacks || !callbacks->newUserList)
		return 0;

	data = (NAMESData *)gsimalloc(sizeof(NAMESData));
	if(data == NULL)
		return 0; 
	memset(data, 0, sizeof(NAMESData));

	return ciAddFilter(chat, TYPE_UNQUIET, channel, NULL, (void*)callbacks->newUserList, NULL, callbacks->param, data);
}

int ciAddCDKEYFilter(CHAT chat, chatAuthenticateCDKeyCallback callback, void * param)
{
	return ciAddFilter(chat, TYPE_CDKEY, NULL, NULL, (void*)callback, NULL, param, NULL);
}

static ciModeChange * ciParseMode(char * mode, char ** params, int numParams)
{
	CHATBool enable;
	int c;
	ciModeChange * changes = NULL;
	int numChanges = 0;
	ciModeChange * change;
	int modeChange;
	CHATBool addParam = CHATFalse;
	void * tempPtr;

	assert(mode != NULL);

	if(*mode == '+')
		enable = CHATTrue;
	else if(*mode == '-')
		enable = CHATFalse;
	else
		return NULL; 
	mode++;

	do
	{
		c = *mode++;

		switch(c)
		{
		case '+':
			enable = CHATTrue;
			modeChange = -1;
			break;

		case '-':
			enable = CHATFalse;
			modeChange = -1;
			break;

		case '\0':
			modeChange = MODE_END;
			addParam = CHATFalse;
			break;

		case 'i':
			modeChange = MODE_INVITE_ONLY;
			addParam = CHATFalse;
			break;

		case 'l':
			modeChange = MODE_LIMIT;
			addParam = CHATTrue;
			break;

		case 'p':
			modeChange = MODE_PRIVATE;
			addParam = CHATFalse;
			break;

		case 's':
			modeChange = MODE_SECRET;
			addParam = CHATFalse;
			break;

		case 'k':
			modeChange = MODE_KEY;
			addParam = CHATTrue;
			break;

		case 'm':
			modeChange = MODE_MODERATED;
			addParam = CHATFalse;
			break;

		case 'n':
			modeChange = MODE_NO_EXTERNAL_MESSAGES;
			addParam = CHATFalse;
			break;

		case 't':
			modeChange = MODE_ONLY_OPS_CHANGE_TOPIC;
			addParam = CHATFalse;
			break;

		case 'o':
			modeChange = MODE_OP;
			addParam = CHATTrue;
			break;

		case 'v':
			modeChange = MODE_VOICE;
			addParam = CHATTrue;
			break;

		case 'b':
			modeChange = MODE_BAN;
			addParam = CHATTrue;
			break;

		case 'u':
			modeChange = MODE_USERS_HIDDEN;
			addParam = CHATFalse;
			break;

		case 'w':
			modeChange = MODE_RECEIVE_WALLOPS;
			addParam = CHATFalse;
			break;

		case 'e':
			modeChange = MODE_OPS_OBEY_CHANNEL_LIMIT;
			addParam = CHATFalse;
			break;

		default:
			modeChange = -1;
		}

		if(modeChange != -1)
		{
			tempPtr = gsirealloc(changes, sizeof(ciModeChange) * (numChanges + 1));
			if(tempPtr == NULL)
			{
				gsifree(changes);
				return NULL; 
			}
			changes = (ciModeChange *)tempPtr;
			change = &changes[numChanges++];
			memset(change, 0, sizeof(ciModeChange));
			change->enable = enable;
			change->mode = modeChange;

			if(addParam)
			{
				if(numParams > 0)
				{
					change->param = *params++;
					numParams--;
				}
				else
					change->param = NULL;
			}
		}
	}
	while(c != '\0');

	return changes;
}

static void ciApplyChangesToMode(CHATChannelMode * mode, ciModeChange * changes)
{
	ciModeChange * change;

	for(change = changes ; change->mode != MODE_END ; change++)
	{
		switch(change->mode)
		{
		case MODE_BAN:
			break;

		case MODE_INVITE_ONLY:
			mode->InviteOnly = change->enable;
			break;

		case MODE_LIMIT:
			if(change->enable && change->param)
				mode->Limit = atoi(change->param);
			else
				mode->Limit = 0;
			break;

		case MODE_PRIVATE:
			mode->Private = change->enable;
			break;

		case MODE_SECRET:
			mode->Secret = change->enable;
			break;

		case MODE_KEY:
			break;

		case MODE_MODERATED:
			mode->Moderated = change->enable;
			break;

		case MODE_NO_EXTERNAL_MESSAGES:
			mode->NoExternalMessages = change->enable;
			break;

		case MODE_ONLY_OPS_CHANGE_TOPIC:
			mode->OnlyOpsChangeTopic = change->enable;
			break;

		case MODE_OP:
			break;

		case MODE_VOICE:
			break;

		case MODE_USERS_HIDDEN:
			break;

		case MODE_RECEIVE_WALLOPS:
			break;

		case MODE_OPS_OBEY_CHANNEL_LIMIT:
			mode->OpsObeyChannelLimit = change->enable;
			break;

		default:
			assert(0);
		}
	}
}

void ciPrivmsgHandler(CHAT chat, const ciServerMessage * message)
{
	char * ctcp;
	char * target;
	char * from;
	CHATBool action = CHATFalse;
	char * msg;
	int len;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciPrivmsgHandler called\n");
#endif

	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 

	target = message->params[0];
	msg = message->params[1];
	from = message->nick;

	len = (int)strlen(msg);
	ctcp = "";
	if((msg[0] == '\001') && IS_ALPHA(msg[1]) && (msg[len - 1] == '\001'))
	{
		char * str;

		msg[len - 1] = '\0';

		str = strchr(msg, ' ');
		if(str != NULL)
		{
			ctcp = &msg[1];

			*str = '\0';
			msg = (str + 1);
		}
	}

	if(strcmp(ctcp, "ACTION") == 0)
	{
		action = CHATTrue;
	}
#if 0
	else if(strcmp(ctcp, "PING") == 0)
	{
		ciSocketSendf(&connection->chatSocket, "NOTICE %s :\001PING %s\001", from, msg);

		return;
	}
#endif
	else if(ctcp[0] != '\0')
	{
		return;
	}

	if(strcasecmp(target, connection->nick) == 0)
	{
		if(connection->globalCallbacks.privateMessage != NULL)
		{
			ciCallbackPrivateMessageParams params;
			params.user = from;
			params.message = msg;
			if(action)
				params.type = CHAT_ACTION;
			else
				params.type = CHAT_MESSAGE;
			ciAddCallback(chat, CALLBACK_PRIVATE_MESSAGE, (void*)connection->globalCallbacks.privateMessage, &params, connection->globalCallbacks.param, 0, NULL);
		}
	}
	else
	{
		chatChannelCallbacks * callbacks = ciGetChannelCallbacks(chat, target);
		if((callbacks != NULL) && (callbacks->channelMessage != NULL))
		{
			ciCallbackChannelMessageParams params;
			params.channel = target;
			params.user = from;
			params.message = msg;
			if(action)
				params.type = CHAT_ACTION;
			else
				params.type = CHAT_MESSAGE;
			ciAddCallback(chat, CALLBACK_CHANNEL_MESSAGE, (void*)callbacks->channelMessage, &params, callbacks->param, 0, target);
		}
	}
}

void ciNoticeHandler(CHAT chat, const ciServerMessage * message)
{
	char * target;
	char * msg;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciNoticeHandler called\n");
#endif
	
	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 
	
	target = message->params[0];
	msg = message->params[1];
	
	if(strcasecmp(target, connection->nick) == 0)
	{
		if(connection->globalCallbacks.privateMessage != NULL)
		{
			ciCallbackPrivateMessageParams params;
			if(message->nick != NULL)
				params.user = message->nick;
			else
				params.user = NULL;
			params.message = msg;
			params.type = CHAT_NOTICE;
			ciAddCallback(chat, CALLBACK_PRIVATE_MESSAGE, (void*)connection->globalCallbacks.privateMessage, &params, connection->globalCallbacks.param, 0, NULL);
		}
	}
	else
	{
		chatChannelCallbacks * callbacks = ciGetChannelCallbacks(chat, target);
		if((callbacks != NULL) && (callbacks->channelMessage != NULL))
		{
			ciCallbackChannelMessageParams params;
			params.channel = target;
			if(message->nick != NULL)
				params.user = message->nick;
			else
				params.user = NULL;
			params.message = msg;
			params.type = CHAT_NOTICE;
			ciAddCallback(chat, CALLBACK_CHANNEL_MESSAGE, (void*)callbacks->channelMessage, &params, callbacks->param, 0, target);
		}
	}
}

void ciUTMHandler(CHAT chat, const ciServerMessage * message)
{
	char * target;
	char * msg;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciUTMHandler called\n");
#endif
	
	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 
	
	target = message->params[0];
	msg = message->params[1];
	
	if(strcasecmp(target, connection->nick) == 0)
	{
		if(connection->globalCallbacks.privateMessage != NULL)
		{
			ciCallbackPrivateMessageParams params;
			if(message->nick != NULL)
				params.user = message->nick;
			else
				params.user = NULL;
			params.message = msg;
			params.type = CHAT_UTM;
			ciAddCallback(chat, CALLBACK_PRIVATE_MESSAGE, (void*)connection->globalCallbacks.privateMessage, &params, connection->globalCallbacks.param, 0, NULL);
		}
	}
	else
	{
		chatChannelCallbacks * callbacks = ciGetChannelCallbacks(chat, target);
		if((callbacks != NULL) && (callbacks->channelMessage != NULL))
		{
			ciCallbackChannelMessageParams params;
			params.channel = target;
			if(message->nick != NULL)
				params.user = message->nick;
			else
				params.user = NULL;
			params.message = msg;
			params.type = CHAT_UTM;
			ciAddCallback(chat, CALLBACK_CHANNEL_MESSAGE, (void*)callbacks->channelMessage, &params, callbacks->param, 0, target);
		}
	}
}

void ciATMHandler(CHAT chat, const ciServerMessage * message)
{
	char * target;
	char * msg;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciATMHandler called\n");
#endif
	
	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 
	
	target = message->params[0];
	msg = message->params[1];
	
	if(strcasecmp(target, connection->nick) == 0)
	{
		if(connection->globalCallbacks.privateMessage != NULL)
		{
			ciCallbackPrivateMessageParams params;
			if(message->nick != NULL)
				params.user = message->nick;
			else
				params.user = NULL;
			params.message = msg;
			params.type = CHAT_ATM;
			ciAddCallback(chat, CALLBACK_PRIVATE_MESSAGE, (void*)connection->globalCallbacks.privateMessage, &params, connection->globalCallbacks.param, 0, NULL);
		}
	}
	else
	{
		chatChannelCallbacks * callbacks = ciGetChannelCallbacks(chat, target);
		if((callbacks != NULL) && (callbacks->channelMessage != NULL))
		{
			ciCallbackChannelMessageParams params;
			params.channel = target;
			if(message->nick != NULL)
				params.user = message->nick;
			else
				params.user = NULL;
			params.message = msg;
			params.type = CHAT_ATM;
			ciAddCallback(chat, CALLBACK_CHANNEL_MESSAGE, (void*)callbacks->channelMessage, &params, callbacks->param, 0, target);
		}
	}
}

void ciPingHandler(CHAT chat, const ciServerMessage * message)
{
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciPingHandler called\n");
#endif
	
	ciSocketSendf(&connection->chatSocket, "PONG %s", message->param);
}

void ciNickHandler(CHAT chat, const ciServerMessage * message)
{
	char * oldNick;
	char * newNick;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciNickHandler called\n");
#endif
	
	assert(message->numParams == 1);
	if(message->numParams != 1)
		return; 

	oldNick	= message->nick;
	newNick = message->params[0];

	if(strcasecmp(oldNick, connection->nick) == 0)
	{
		ciServerMessageFilter * filter;
		ciFilterMatch match;

		assert(strlen(newNick) < MAX_NICK);
		strzcpy(connection->nick, newNick, MAX_NICK);

#ifdef GSI_UNICODE
		AsciiToUCS2String(connection->nick, connection->nickW);
#endif

		memset(&match, 0, sizeof(ciFilterMatch));
		match.type = TYPE_NICK;
		match.name = oldNick;
		match.name2 = newNick;

		filter = ciFindFilter(chat, 1, &match);
		if(filter)
		{
			ciCallbackChangeNickParams params;
			params.success = CHATTrue;
			params.oldNick = oldNick;
			params.newNick = newNick;

			FINISH_FILTER;
		}
	}

	ciUserChangedNick(chat, oldNick, newNick);
}

void ciJoinHandler(CHAT chat, const ciServerMessage * message)
{
	char * channel;
	char * nick;
	char * user;
	char * address;
	int mode;
	chatChannelCallbacks * callbacks;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciJoinHandler called\n");
#endif
	
	assert(message->numParams == 1);
	if(message->numParams != 1)
		return; 

	channel = message->params[0];
	nick = message->nick;
	user = message->user;
	address = message->host;

	if(*nick == '@')
	{
		mode = CHAT_OP;
		nick++;
		assert(*nick != '\0');
	}
	else if(*nick == '+')
	{
		mode = CHAT_VOICE;
		nick++;
		assert(*nick != '\0');
	}
	else
	{
		mode = CHAT_NORMAL;
	}

	if(strcmp(nick, connection->nick) == 0)
	{
		ciServerMessageFilter * filter;
		ciFilterMatch match;

		if(!ciIsEnteringChannel(chat, channel))
			return;

		memset(&match, 0, sizeof(ciFilterMatch));
		match.type = TYPE_JOIN;
		match.name = channel;

		filter = ciFindFilter(chat, 1, &match);
		if(filter != NULL)
		{
			JOINData * data;

			data = (JOINData *)filter->data;

			ciChannelEntered(chat, channel, &data->callbacks);

			ciSetChannelPassword(chat, channel, data->password);

			data->joined = CHATTrue;

			ciSocketSendf(&connection->chatSocket, "MODE %s", channel);
		}

		return;
	}

	if(ciInChannel(chat, channel))
		ciUserEnteredChannel(chat, nick, channel, mode, user, address);

	if(ciWasJoinCallbackCalled(chat, channel))  
	{
		callbacks = ciGetChannelCallbacks(chat, channel);
		if(callbacks != NULL)
		{
			if(callbacks->userJoined != NULL)
			{
				ciCallbackUserJoinedParams params;
				params.channel = channel;
				params.user = nick;
				params.mode = mode;
				ciAddCallback(chat, CALLBACK_USER_JOINED, (void*)callbacks->userJoined, &params, callbacks->param, 0, channel);
			}

			if(callbacks->userListUpdated != NULL)
			{
				ciCallbackUserListUpdatedParams params;
				params.channel = channel;
				ciAddCallback(chat, CALLBACK_USER_LIST_UPDATED, (void*)callbacks->userListUpdated, &params, callbacks->param, 0, channel);
			}
		}
	}
}

void ciPartHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;
	char * channel;
	char * reason;
	chatChannelCallbacks * callbacks;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciPartHandler called\n");
#endif
	
	nick = message->nick;
	channel = message->params[0];
	if (message->numParams > 1) 
		reason = message->params[1];
	else
		reason = "";

	if(strcmp(nick, connection->nick) == 0)
	{
	}
	else
	{
		ciUserLeftChannel(chat, nick, channel);

		if(ciWasJoinCallbackCalled(chat, channel))
		{
			callbacks = ciGetChannelCallbacks(chat, channel);
			if(callbacks != NULL)
			{
				if(callbacks->userParted != NULL)
				{
					ciCallbackUserPartedParams params;
					params.channel = channel;
					params.user = nick;
					params.why = CHAT_LEFT;
					params.reason = reason;
					params.kicker = NULL;
					ciAddCallback(chat, CALLBACK_USER_PARTED, (void*)callbacks->userParted, &params, callbacks->param, 0, channel);
				}

				if(callbacks->userListUpdated != NULL)
				{
					ciCallbackUserListUpdatedParams params;
					params.channel = channel;
					ciAddCallback(chat, CALLBACK_USER_LIST_UPDATED, (void*)callbacks->userListUpdated, &params, callbacks->param, 0, channel);
				}
			}
		}
	}
}

void ciKickHandler(CHAT chat, const ciServerMessage * message)
{
	char * channel;
	char * kicker;
	char * kickee;
	char * reason;
	chatChannelCallbacks * callbacks;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciKickHandler called\n");
#endif
	
	assert((message->numParams == 2) || (message->numParams == 3));
	if((message->numParams != 2) && (message->numParams != 3))
		return; 

	channel = message->params[0];
	kicker = message->nick;
	kickee = message->params[1];
	if(message->numParams == 3)
		reason = message->params[2];
	else
		reason = "";

	ciUserLeftChannel(chat, kickee, channel);

	callbacks = ciGetChannelCallbacks(chat, channel);
	if(callbacks != NULL)
	{	
		if(strcasecmp(kickee, connection->nick) == 0)
		{
			if(callbacks->kicked != NULL)
			{
				ciCallbackKickedParams params;
				params.channel = channel;
				params.user = kicker;
				params.reason = reason;
				ciAddCallback(chat, CALLBACK_KICKED, (void*)callbacks->kicked, &params, callbacks->param, 0, NULL);
			}
			
			ciChannelLeft(chat, channel);
		}
		else
		{
			if(ciWasJoinCallbackCalled(chat, channel))
			{
				if(callbacks->userParted != NULL)
				{
					ciCallbackUserPartedParams params;
					params.channel = channel;
					params.user = kickee;
					params.why = CHAT_KICKED;
					params.reason = reason;
					params.kicker = kicker;
					ciAddCallback(chat, CALLBACK_USER_PARTED, (void*)callbacks->userParted, &params, callbacks->param, 0, channel);
				}
				
				if(callbacks->userListUpdated != NULL)
				{
					ciCallbackUserListUpdatedParams params;
					params.channel = channel;
					ciAddCallback(chat, CALLBACK_USER_LIST_UPDATED, (void*)callbacks->userListUpdated, &params, callbacks->param, 0, channel);
				}
			}
		}
	}
}

static void ciQuitEnumChannelsCallback(CHAT chat, const char * user, const char * channel, void * reason)
{
	chatChannelCallbacks * callbacks;
	
	ASSERT_STR(user);
	ASSERT_STR(channel);
	assert(reason != NULL);

	ciUserLeftChannel(chat, user, channel);

	if(ciWasJoinCallbackCalled(chat, channel))
	{
		callbacks = ciGetChannelCallbacks(chat, channel);
		if(callbacks != NULL)
		{
			if(callbacks->userParted != NULL)
			{
				ciCallbackUserPartedParams params;
				params.channel = (char *)channel;
				params.user = (char *)user;
				params.why = CHAT_QUIT;
				params.reason = (char *)reason;
				params.kicker = NULL;
				ciAddCallback(chat, CALLBACK_USER_PARTED, (void*)callbacks->userParted, &params, callbacks->param, 0, channel);
			}

			if(callbacks->userListUpdated != NULL)
			{
				ciCallbackUserListUpdatedParams params;
				params.channel = (char *)channel;
				ciAddCallback(chat, CALLBACK_USER_LIST_UPDATED, (void*)callbacks->userListUpdated, &params, callbacks->param, 0, channel);
			}
		}
	}
}

void ciQuitHandler(CHAT chat, const ciServerMessage * message)
{
	char * reason;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciQuitHandler called\n");
#endif
	
	assert(message->numParams == 1);
	if(message->numParams != 1)
		return; 

	reason = message->params[0];

	ciUserEnumChannels(chat, message->nick, ciQuitEnumChannelsCallback, reason);
}

static void ciKillEnumChannelsCallback(CHAT chat, const char * user, const char * channel, void * param)
{
	chatChannelCallbacks * callbacks;
	char *reason = (char *)param;
	ASSERT_STR(user);
	ASSERT_STR(channel);
	assert(reason != NULL);

	ciUserLeftChannel(chat, user, channel);

	if(ciWasJoinCallbackCalled(chat, channel))
	{
		callbacks = ciGetChannelCallbacks(chat, channel);
		if(callbacks != NULL)
		{
			if(callbacks->userParted != NULL)
			{
				ciCallbackUserPartedParams params;
				params.channel = (char *)channel;
				params.user = (char *)user;
				params.why = CHAT_KILLED;
				params.reason = (char *)reason;
				params.kicker = NULL;
				ciAddCallback(chat, CALLBACK_USER_PARTED, (void*)callbacks->userParted, &params, callbacks->param, 0, channel);
			}

			if(callbacks->userListUpdated != NULL)
			{
				ciCallbackUserListUpdatedParams params;
				params.channel = (char *)channel;
				ciAddCallback(chat, CALLBACK_USER_LIST_UPDATED, (void*)callbacks->userListUpdated, &params, callbacks->param, 0, channel);
			}
		}
	}
}

void ciKillHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;
	char * reason;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciKillHandler called\n");
#endif
	
	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 

	nick = message->params[0];
	reason = message->params[1];

	ciUserEnumChannels(chat, nick, ciKillEnumChannelsCallback, reason);
}

void ciTopicHandler(CHAT chat, const ciServerMessage * message)
{
	char * channel;
	char * topic;
	chatChannelCallbacks * callbacks;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciTopicHandler called\n");
#endif
	
	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 

	channel = message->params[0];
	topic = message->params[1];

	ciSetChannelTopic(chat, channel, topic);

	callbacks = ciGetChannelCallbacks(chat, channel);
	if((callbacks != NULL) && (callbacks->topicChanged != NULL))
	{
		ciCallbackTopicChangedParams params;
		params.channel = channel;
		params.topic = topic;
		ciAddCallback(chat, CALLBACK_TOPIC_CHANGED, (void*)callbacks->topicChanged, &params, callbacks->param, 0, channel);
	}
}

void ciModeHandler(CHAT chat, const ciServerMessage * message)
{
	ciModeChange * changes;
	ciModeChange * change;
	CHATChannelMode channelMode;
	char * channel;
	char * mode;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciModeHandler called\n");
#endif
	
	assert(message->numParams >= 2);
	if(message->numParams < 2)
		return; 

	channel = message->params[0];
	mode = message->params[1];

	if(!ciInChannel(chat, channel))
		return;

	changes = ciParseMode(mode, message->params + 2, message->numParams - 2);
	if(changes == NULL)
		return; 

	for(change = changes ; change->mode != MODE_END ; change++)
	{
		switch(change->mode)
		{
		case MODE_BAN:
			break;

		case MODE_INVITE_ONLY:
			break;

		case MODE_LIMIT:
			break;

		case MODE_PRIVATE:
			break;

		case MODE_SECRET:
			break;

		case MODE_KEY:
			if(change->enable)
				ciSetChannelPassword(chat, channel, change->param);
			else
				ciSetChannelPassword(chat, channel, NULL);
			break;

		case MODE_MODERATED:
			break;

		case MODE_NO_EXTERNAL_MESSAGES:
			break;

		case MODE_ONLY_OPS_CHANGE_TOPIC:
			break;

		case MODE_USERS_HIDDEN:
			break;

		case MODE_RECEIVE_WALLOPS:
			break;

		case MODE_OP:
			if(change->param)
				ciUserChangedMode(chat, change->param, channel, CHAT_OP, change->enable);
			break;

		case MODE_VOICE:
			if(change->param)
				ciUserChangedMode(chat, change->param, channel, CHAT_VOICE, change->enable);
			break;
		case MODE_OPS_OBEY_CHANNEL_LIMIT:
			break;
		default:
			assert(0);
		}
	}

	if(!ciGetChannelMode(chat, channel, &channelMode))
	{
		gsifree(changes);
		return; 
	}
	ciApplyChangesToMode(&channelMode, changes);
	ciSetChannelMode(chat, channel, &channelMode);

	gsifree(changes);
}

void ciErrorHandler(CHAT chat, const ciServerMessage * message)
{

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrorHandler called\n");
#endif

	assert(message->numParams == 1);
	if(message->numParams != 1)
		return; 

	ciHandleDisconnect(chat, message->params[0]);
}

void ciInviteHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;
	char * channel;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciInviteHandler called\n");
#endif
	
	assert(message->numParams == 2);
	if(message->numParams != 2)
		return; 

	nick = message->nick;
	channel = message->params[1];

	if(connection->globalCallbacks.invited != NULL)
	{
		ciCallbackInvitedParams params;
		params.channel = channel;
		params.user = nick;
		ciAddCallback(chat, CALLBACK_INVITED, (void*)connection->globalCallbacks.invited, &params, connection->globalCallbacks.param, 0, NULL);
	}
}

void ciNameReplyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch matches[4];
	ciServerMessageFilter * filter;
	char * channel;
	NAMESData * data = NULL;
	char * names;
	char * nick;
	char * str;
	void * tempPtr;
	int mode;
	int len;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciNameReplyHandler called\n");
#endif
	
	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	channel = message->params[2];
	names = message->params[3];

	memset(matches, 0, sizeof(matches));
	matches[0].type = TYPE_JOIN;
	matches[0].name = channel;
	matches[1].type = TYPE_UNQUIET;
	matches[1].name = channel;
	matches[2].type = TYPE_NAMES;
	matches[2].name = channel;
	matches[3].type = TYPE_NAMES;

	filter = ciFindFilter(chat, 4, matches);
	if(!filter)
		return;

	if(filter->type != TYPE_JOIN)
		data = (NAMESData *)filter->data;

	nick = strtok(names, " ");
	while(nick != NULL)
	{
		assert(nick[0] != '\0');
		
		if(nick[0] == '@')
		{
			assert(nick[1] != '\0');
			mode = CHAT_OP;
			nick++;
		}
		else if(nick[0] == '+')
		{
			assert(nick[1] != '\0');
			mode = CHAT_VOICE;
			nick++;
		}
		else
			mode = CHAT_NORMAL;

		if(filter->type != TYPE_JOIN)
		{
			if(data->numUsers == data->len)
			{
				tempPtr = (char **)gsirealloc(data->operators, sizeof(char *) * (data->len + NAMES_ARRAY_INC));
				if(tempPtr == NULL)
				{
					assert(0);
					return; 
				}
				data->operators = (char **)tempPtr;
				tempPtr = (char **)gsirealloc(data->modes, sizeof(int) * (data->len + NAMES_ARRAY_INC));
				if(tempPtr == NULL)
				{
					assert(0);
					return; 
				}
				data->modes = (int *)tempPtr;
				data->len += NAMES_ARRAY_INC;
			}

			len = (int)(strlen(nick) + 1);
			str = (char *)gsimalloc((unsigned int)len);
			if(str == NULL)
			{
				assert(0);
				return; 
			}
			memcpy(str, nick, (unsigned int)len);

			data->operators[data->numUsers] = str;
			data->modes[data->numUsers] = mode;
			data->numUsers++;
		}

		if((filter->type == TYPE_JOIN) || (filter->type == TYPE_UNQUIET))
		{
			ciUserEnteredChannel(chat, nick, channel, mode, NULL, NULL);
		}

		nick = strtok(NULL, " ");
	}
}

void ciEndOfNamesHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch matches[4];
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciEndOfNamesHandler called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	if(strcmp(channel, "*") == 0)
		channel = NULL;

	memset(matches, 0, sizeof(matches));
	matches[0].type = TYPE_JOIN;
	matches[0].name = channel;
	matches[1].type = TYPE_UNQUIET;
	matches[1].name = channel;
	matches[2].type = TYPE_NAMES;
	matches[2].name = channel;
	matches[3].type = TYPE_NAMES;
	
	filter = ciFindFilter(chat, 4, matches);
	if(!filter)
		return;

	if(filter->type == TYPE_JOIN)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATTrue;
		params.result = CHATEnterSuccess;
		params.channel = channel;

		if(!filter->callback)
		{
			ciJoinCallbackCalled(chat, channel);
		}

		FINISH_FILTER;

		return;
	}

	if(filter->type == TYPE_UNQUIET)
	{
		NAMESData * data = (NAMESData *)filter->data;
		ciCallbackNewUserListParams params;
		params.channel = channel;
		params.numUsers = data->numUsers;
		params.operators = data->operators;
		params.modes = data->modes;

		FINISH_FILTER;

		return;
	}

	if(filter->type == TYPE_NAMES)
	{
		NAMESData * data = (NAMESData *)filter->data;
		ciCallbackEnumUsersParams params;
		params.success = CHATTrue;
		params.channel = channel;
		params.numUsers = data->numUsers;
		params.operators = data->operators;
		params.modes = data->modes;

		FINISH_FILTER;

		return;
	}
}

void ciRplTopicHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * topic;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplTopicHandler called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];
	topic = message->params[2];

	ciSetChannelTopic(chat, channel, topic);

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_TOPIC;
	match.name = channel;

	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackGetChannelTopicParams params;
		params.success = CHATTrue;
		params.channel = channel;
		params.topic = topic;

		FINISH_FILTER;
	}
	else
	{
		chatChannelCallbacks * callbacks;
		callbacks = ciGetChannelCallbacks(chat, channel);
		if((callbacks != NULL) && (callbacks->topicChanged != NULL))
		{
			ciCallbackTopicChangedParams params;
			params.channel = channel;
			params.topic = topic;
			ciAddCallback(chat, CALLBACK_TOPIC_CHANGED, (void*)callbacks->topicChanged, &params, callbacks->param, 0, channel);
		}
	}
}

void ciRplNoTopicHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplNoTopicHandler called\n");
#endif
	
	assert(message->numParams >= 2);
	if(message->numParams < 2)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_TOPIC;
	match.name = channel;

	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackGetChannelTopicParams params;
		params.success = CHATTrue;
		params.channel = channel;
		params.topic = "";

		FINISH_FILTER;
	}
	else
	{
		chatChannelCallbacks * callbacks;
		callbacks = ciGetChannelCallbacks(chat, channel);
		if((callbacks != NULL) && (callbacks->topicChanged != NULL))
		{
			ciCallbackTopicChangedParams params;
			params.channel = channel;
			params.topic = "";
			ciAddCallback(chat, CALLBACK_TOPIC_CHANGED, (void*)callbacks->topicChanged, &params, callbacks->param, 0, channel);
		}
	}
}

void ciErrNickInUseHandler(CHAT chat, const ciServerMessage * message)
{
	char * oldNick;
	char * newNick;
	ciServerMessageFilter * filter;
	ciFilterMatch match;
	
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplNickErrorHandler called\n");
#endif

	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	oldNick = message->params[0];
	newNick = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_NICK;
	match.name = oldNick;
	match.name2 = newNick;

	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackChangeNickParams params;
		params.success = CHATFalse;
		params.oldNick = oldNick;
		params.newNick = newNick;

		FINISH_FILTER;
	}
	else
	{
		assert(connection->connecting);
		if(connection->connecting)
			ciNickError(chat, CHAT_IN_USE, connection->nick, 0, NULL);
	}
}

void ciRplWhoReplyHandler(CHAT chat, const ciServerMessage * message)
{
	char * channel;
	char * nick;
	char * user;
	char * address;
	ciServerMessageFilter * filter;
	ciFilterMatch matches[3];

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplWhoReplyHandler called\n");
#endif

	assert(message->numParams == 8);
	if(message->numParams != 8)
		return; 

	channel = message->params[1];
	user = message->params[2];
	address = message->params[3];
	nick = message->params[5];

	ciSetUserBasicInfo(chat, nick, user, address);

	memset(matches, 0, sizeof(matches));
	matches[0].type = TYPE_UMODE;
	matches[0].name = nick;
	matches[0].name2 = channel;
	matches[1].type = TYPE_WHO;
	matches[1].name = nick;
	matches[2].type = TYPE_CWHO;
	matches[2].name = channel;

	filter = ciFindFilter(chat, 3, matches);
	if(!filter)
		return;

	if(filter->type == TYPE_UMODE)
	{
		ciCallbackGetUserModeParams params;
		int mode;

		mode = 0;
		if(strchr(message->params[6], '@'))
			mode |= CHAT_OP;
		if(strchr(message->params[6], '+'))
			mode |= CHAT_VOICE;

		params.success = CHATTrue;
		params.channel = channel;
		params.user = nick;
		params.mode = mode;

		FINISH_FILTER;

		return;
	}

	if(filter->type == TYPE_WHO)
	{
		ciCallbackGetBasicUserInfoParams params;

		params.success = CHATTrue;
		params.nick = nick;
		params.user = user;
		params.address = address;
		
		ciAddCallback(chat, CALLBACK_GET_BASIC_USER_INFO, filter->callback, &params, filter->param, filter->ID, NULL);

		filter->callback = NULL;

		return;
	}

	if((filter->type == TYPE_CWHO) && filter->callback)
	{
		ciCallbackGetChannelBasicUserInfoParams params;

		params.success = CHATTrue;
		params.channel = filter->name;
		params.nick = nick;
		params.user = user;
		params.address = address;

		ciAddCallback(chat, CALLBACK_GET_CHANNEL_BASIC_USER_INFO, filter->callback, &params, filter->param, filter->ID, NULL);

		return;
	}
}

void ciRplEndOfWhoHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch matches[2];
	ciServerMessageFilter * filter;
	char * name;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplEndOfWhoHandler called\n");
#endif

	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	name = message->params[1];

	memset(matches, 0, sizeof(matches));
	matches[0].type = TYPE_WHO;
	matches[0].name = name;
	matches[1].type = TYPE_CWHO;
	matches[1].name = name;

	filter = ciFindFilter(chat, 2, matches);
	if(!filter)
		return;

	if(filter->type == TYPE_WHO)
	{
		ciCallbackGetBasicUserInfoParams params;

		params.success = CHATFalse;
		params.nick = name;
		params.user = NULL;
		params.address = NULL;

		FINISH_FILTER;

		return;
	}

	if(filter->type == TYPE_CWHO)
	{
		ciCallbackGetChannelBasicUserInfoParams params;

		params.success = CHATTrue;
		params.channel = name;
		params.nick = NULL;
		params.user = NULL;
		params.address = NULL;

		FINISH_FILTER;

		return;
	}
}

static char * ciParseValue(const char * flags, int * len)
{
	int i;
	char * str;

	assert(flags);
	assert(len);

	if(!flags || (flags[0] != '\\'))
		return NULL;

	flags++;

	for(i = 0 ; flags[i] && (flags[i] != '\\') ; i++)  { };

	str = (char *)gsimalloc((unsigned int)i + 1);
	if(!str)
		return NULL;

	memcpy(str, flags, (unsigned int)i);
	str[i] = '\0';

	*len = (i + 1);
	return str;
}

void ciRplGetKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	const char * nick;
	const char * cookie;
	const char * flags;
	int num;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplGetKeyHandler called\n");
#endif

	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	nick = message->params[1];
	cookie = message->params[2];
	flags = message->params[3];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETKEY;
	match.name = cookie;

	filter = ciFindFilter(chat, 1, &match);
	if(filter)
	{
		GETKEYData * data;
		ciCallbackGetGlobalKeysParams params;
		char ** values;
		char * str;
		int i;
		int len;

		data = (GETKEYData *)filter->data;
		num = data->num;

		values = (char **)gsimalloc(sizeof(char *) * num);
		if(!values)
			return;

		for(i = 0 ; i < num ; i++)
		{
			str = ciParseValue(flags, &len);
			if(str)
			{
				values[i] = str;
				flags += len;
			}
			else
			{
				values[i] = goastrdup("");
			}
		}

		params.success = CHATTrue;
		params.user = (char *)nick;
		params.num = num;
		params.keys = data->keys;
		params.values = values;

		if(!data->channel)
		{
			FINISH_FILTER;
		}
		else
		{
			ciAddCallback(chat, CALLBACK_GET_GLOBAL_KEYS, filter->callback, &params, filter->param, filter->ID, NULL);
		}

		for(i = 0 ; i < num ; i++)
			gsifree(values[i]);
		gsifree(values);
	}
}

void ciRplEndGetKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	const char * cookie;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplEndGetKeyHandler called\n");
#endif

	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	cookie = message->params[2];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETKEY;
	match.name = cookie;

	filter = ciFindFilter(chat, 1, &match);
	if(filter)
	{
		ciCallbackGetGlobalKeysParams params;
		GETKEYData * data;

		data = (GETKEYData *)filter->data;

		params.success = CHATTrue;
		params.user = NULL;
		params.num = data->num;
		params.keys = data->keys;
		params.values = NULL;

		FINISH_FILTER;
	}
}

void ciRplGetCKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	const char * nick;
	const char * cookie;
	char * flags;
	const char * channel;
	int num;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplGetCKeyHandler called\n");
#endif

	assert(message->numParams == 5);
	if(message->numParams != 5)
		return; 

	channel = message->params[1];
	nick = message->params[2];
	cookie = message->params[3];
	flags = message->params[4];

	if(strcmp(cookie, "BCAST") == 0)
	{
		chatChannelCallbacks * callbacks;
		ciCallbackBroadcastKeyChangedParams params;
		char * key;
		char * value;
		int temp;

		callbacks = ciGetChannelCallbacks(chat, channel);
		if(callbacks && callbacks->broadcastKeyChanged)
		{
			memset(&params, 0, sizeof(ciCallbackBroadcastKeyChangedParams));
			params.channel = (char *)channel;
			params.user = (char *)nick;

			while(*flags)
			{
				key = strstr(flags, "b_");
				flags = key;
				while(*flags && (*flags != '\\'))
					flags++;
				if(!*flags)
					break;
				*flags++ = '\0';
				value = flags;
				while(*flags && (*flags != '\\'))
					flags++;
				temp = *flags;
				*flags = '\0';

				params.key = key;
				params.value = value;
				ciAddCallback(chat, CALLBACK_BROADCAST_KEY_CHANGED, (void*)callbacks->broadcastKeyChanged, &params, callbacks->param, 0, channel);

				*flags = (char)temp;
			}
		}

		return;
	}

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETCKEY;
	match.name = cookie;

	filter = ciFindFilter(chat, 1, &match);
	if(filter)
	{
		GETCKEYData * data;
		ciCallbackGetChannelKeysParams params;
		char ** values;
		char * key;
		char * value;
		int i;
		int len;
		char ** tempPtr;

		data = (GETCKEYData *)filter->data;
		num = data->num;

		values = (char **)gsimalloc(sizeof(char *) * num);
		if(!values)
			return;

		for(i = 0 ; i < num ; i++)
		{
			value = ciParseValue(flags, &len);
			values[i] = value;
			if(value)
				flags += len;
		}

		if(data->allBroadcastKeys)
		{
			while((key = ciParseValue(flags, &len)) != NULL)
			{
				flags += len;
				value = ciParseValue(flags, &len);
				if(value)
				{
					flags += len;

					tempPtr = (char **)gsirealloc(data->keys, sizeof(char *) * (num + 1));
					if(tempPtr)
					{
						data->keys = tempPtr;
						tempPtr = (char **)gsirealloc(values, sizeof(char *) * (num + 1));
						if(tempPtr)
						{
							values = tempPtr;
							data->keys[num] = key;
							values[num] = value;
							num++;
						}
						else
						{
							gsifree(key);
							gsifree(value);
						}
					}
					else
					{
						gsifree(key);
						gsifree(value);
					}
				}
				else
				{
					gsifree(key);
					break;
				}
			}

			data->num = num;
		}

		params.success = CHATTrue;
		params.channel = (char *)channel;
		params.user = (char *)nick;
		params.num = num;
		params.keys = data->keys;
		params.values = values;

		if(!data->channel)
		{
			FINISH_FILTER;
		}
		else
		{
			ciAddCallback(chat, CALLBACK_GET_CHANNEL_KEYS, filter->callback, &params, filter->param, filter->ID, NULL);
		}

		for(i = 0 ; i < num ; i++)
			gsifree(values[i]);
		gsifree(values);
	}
}

void ciRplEndGetCKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	const char * cookie;
	const char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplEndGetCKeyHandler called\n");
#endif

	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	channel = message->params[1];
	cookie = message->params[2];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETCKEY;
	match.name = cookie;

	filter = ciFindFilter(chat, 1, &match);
	if(filter)
	{
		GETCKEYData * data;
		ciCallbackGetChannelKeysParams params;

		data = (GETCKEYData *)filter->data;

		params.success = CHATTrue;
		params.channel = (char *)channel;
		params.user = NULL;
		params.num = data->num;
		params.keys = data->keys;
		params.values = NULL;

		FINISH_FILTER;
	}
}

void ciRplGetChanKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	const char * cookie;
	char * flags;
	const char * channel;
	int num;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplGetChanKeyHandler called\n");
#endif

	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	channel = message->params[1];
	cookie = message->params[2];
	flags = message->params[3];

	if(strcmp(cookie, "BCAST") == 0)
	{
		chatChannelCallbacks * callbacks;
		ciCallbackBroadcastKeyChangedParams params;
		char * key;
		char * value;
		int temp;

		callbacks = ciGetChannelCallbacks(chat, channel);
		if(callbacks && callbacks->broadcastKeyChanged)
		{
			memset(&params, 0, sizeof(ciCallbackBroadcastKeyChangedParams));
			params.channel = (char *)channel;
			params.user = NULL;

			while(*flags)
			{
				key = strstr(flags, "b_");
				flags = key;
				while(*flags && (*flags != '\\'))
					flags++;
				if(!*flags)
					break;
				*flags++ = '\0';
				value = flags;
				while(*flags && (*flags != '\\'))
					flags++;
				temp = *flags;
				*flags = '\0';

				params.key = key;
				params.value = value;
				ciAddCallback(chat, CALLBACK_BROADCAST_KEY_CHANGED, (void*)callbacks->broadcastKeyChanged, &params, callbacks->param, 0, channel);

				*flags = (char)temp;
			}
		}

		return;
	}

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETCHANKEY;
	match.name = cookie;

	filter = ciFindFilter(chat, 1, &match);
	if(filter)
	{
		GETCHANKEYData * data;
		ciCallbackGetChannelKeysParams params;
		char ** values;
		char ** keys = NULL;
		char * key;
		char * value;
		int i;
		int len;
		char ** tempPtr;

		data = (GETCHANKEYData *)filter->data;
		num = data->num;

		if(num)
		{
			values = (char **)gsimalloc(sizeof(char *) * num);
			if(!values)
				return;

			for(i = 0 ; i < num ; i++)
			{
				value = ciParseValue(flags, &len);
				values[i] = value;
				if(value)
					flags += len;
			}

			if(data->allBroadcastKeys)
			{
				while((key = ciParseValue(flags, &len)) != NULL)
				{
					flags += len;
					value = ciParseValue(flags, &len);
					if(value)
					{
						flags += len;

						tempPtr = (char **)gsirealloc(data->keys, sizeof(char *) * (num + 1));
						if(tempPtr)
						{
							data->keys = tempPtr;
							tempPtr = (char **)gsirealloc(values, sizeof(char *) * (num + 1));
							if(tempPtr)
							{
								values = tempPtr;
								data->keys[num] = key;
								values[num] = value;
								num++;
							}
							else
							{
								gsifree(key);
								gsifree(value);
							}
						}
						else
						{
							gsifree(key);
							gsifree(value);
						}
					}
					else
					{
						gsifree(key);
						break;
					}
				}

				data->num = num;
			}
		}
		else
		{
			char ** keysTemp;
			char ** valuesTemp;

			keys = NULL;
			values = NULL;
			num = 0;
			while(1)
			{
				key = ciParseValue(flags, &len);
				if(!key)
					break;
				flags += len;
				value = ciParseValue(flags, &len);
				if(!value)
				{
					gsifree(key);
					break;
				}
				flags += len;

				keysTemp = (char **)gsirealloc(keys, sizeof(char *) * (num + 1));
				valuesTemp = (char **)gsirealloc(values, sizeof(char *) * (num + 1));
				if(!keysTemp || !valuesTemp)
				{
					gsifree(key);
					gsifree(value);
					while(num--)
					{
						gsifree(keys[num]);
						gsifree(values[num]);
					}
					if(keysTemp)
						gsifree(keysTemp);
					else
						gsifree(keys);
					if(valuesTemp)
						gsifree(valuesTemp);
					else
						gsifree(values);
				}

				keys = keysTemp;
				keys[num] = key;
				values = valuesTemp;
				values[num] = value;

				num++;
			}

			data->num = num;
			data->keys = keys;
		}

		params.success = CHATTrue;
		params.channel = (char *)channel;
		params.user = NULL;
		params.num = num;
		params.keys = data->keys;
		params.values = values;

		FINISH_FILTER;

		for(i = 0 ; i < num ; i++)
			gsifree(values[i]);
		gsifree(values);
	}
}

void ciRplUserIPHandler(CHAT chat, const ciServerMessage * message)
{
	char * IP;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplUserIPHandler called\n");
#endif

	assert(message->numParams >= 1);
	if(message->numParams < 1)
		return; 

	IP = strchr(message->params[message->numParams - 1], '@');
	if(IP)
	{
		IP++;

		if(connection->fillInUserCallback)
		{
#ifndef GSI_UNICODE
			char user[MAX_USER];
			connection->fillInUserCallback(chat, inet_addr(IP), user, connection->connectParam);
			strzcpy(connection->user, user, MAX_USER);
#else
			unsigned short user[MAX_USER];
			connection->fillInUserCallback(chat, inet_addr(IP), user, connection->connectParam);
			wcszcpy(connection->userW, user, MAX_USER);		
			UCS2ToAsciiString(user, connection->user); 
#endif
		}
	}

	ciSendNickAndUser(chat);
}

void ciRplListStartHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplListStartHandler called\n");
#endif

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_LIST;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		LISTData * data = (LISTData *)filter->data;
		assert(data != NULL);

		data->gotStart = CHATTrue;
	}
	
	GSI_UNUSED(message);
}

void ciRplListHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplListHandler called\n");
#endif
	
	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_LIST;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		if(filter->callback != NULL)
		{
			ciCallbackEnumChannelsEachParams params;
			int index;
			char * channel;
			int numUsers;
			char * topic;
			int len;
			void * tempPtr;
			LISTData * data = (LISTData *)filter->data;

			assert(data != NULL);

			len = (int)(strlen(message->params[1]) + 1);
			channel = (char *)gsimalloc((unsigned int)len);
			if(channel == NULL)
				return; 
			memcpy(channel, message->params[1], (unsigned int)len);

			numUsers = atoi(message->params[2]);

			len = (int)(strlen(message->params[3]) + 1);
			topic = (char *)gsimalloc((unsigned int)len);
			if(topic == NULL)
			{
				gsifree(channel);
				return; 
			}
			memcpy(topic, message->params[3], (unsigned int)len);

			index = data->numChannels;

			params.success = CHATTrue;
			params.index = index;
			params.channel = channel;
			params.topic = topic;
			params.numUsers = numUsers;
			ciAddCallback(chat, CALLBACK_ENUM_CHANNELS_EACH, filter->callback, &params, filter->param, filter->ID, NULL);


			tempPtr = gsirealloc(data->channels, sizeof(char *) * (data->numChannels + 1));
			if(tempPtr == NULL)
			{
				gsifree(channel);
				gsifree(topic);
				return; 
			}
			data->channels = (char **)tempPtr;
			data->channels[index] = channel;

			tempPtr = gsirealloc(data->numUsers, sizeof(int) * (data->numChannels + 1));
			if(tempPtr == NULL)
			{
				gsifree(channel);
				gsifree(topic);
				return; 
			}
			data->numUsers = (int *)tempPtr;
			data->numUsers[index] = numUsers;

			tempPtr = gsirealloc(data->topics, sizeof(char *) * (data->numChannels + 1));
			if(tempPtr == NULL)
			{
				gsifree(channel);
				gsifree(topic);
				return; 
			}
			data->topics = (char **)tempPtr;
			data->topics[index] = topic;

			data->numChannels++;
		}
	}
}

void ciRplListEndHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplListEndHandler called\n");
#endif

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_LIST;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		LISTData * data = (LISTData *)filter->data;
		ciCallbackEnumChannelsAllParams params;
		params.success = CHATTrue;
		params.numChannels = data->numChannels;
		params.channels = data->channels;
		params.numUsers = data->numUsers;
		params.topics = data->topics;

		FINISH_FILTER;
	}
	
	GSI_UNUSED(message);
}

void ciRplChannelModeIsHandler(CHAT chat, const ciServerMessage * message)
{
	char * channel;
	char * modes;
	ciModeChange * changes;
	CHATChannelMode mode;
	CHATChannelMode dummyMode;
	ciServerMessageFilter * filter;
	ciFilterMatch match;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplChannelModeIs called\n");
#endif
	
	assert(message->numParams >=3);
	if(message->numParams < 3)
		return; 

	channel = message->params[1];
	modes = message->params[2];

	changes = ciParseMode(modes, message->params + 3, message->numParams - 3);
	if(changes == NULL)
		return; 

	memset(&mode, 0, sizeof(CHATChannelMode));
	ciApplyChangesToMode(&mode, changes);

	if(!ciGetChannelMode(chat, channel, &dummyMode))
	{
		chatChannelCallbacks * callbacks;

		ciSetChannelMode(chat, channel, &mode);

		callbacks = ciGetChannelCallbacks(chat, channel);
		if((callbacks != NULL) && (callbacks->channelModeChanged != NULL))
		{
			ciCallbackChannelModeChangedParams params;
			params.channel = channel;
			params.mode = &mode;
			ciAddCallback(chat, CALLBACK_CHANNEL_MODE_CHANGED, (void*)callbacks->channelModeChanged, &params, callbacks->param, 0, channel);
		}
	}

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_CMODE;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackGetChannelModeParams params;
		params.success = CHATTrue;
		params.channel = channel;
		params.mode = &mode;

		FINISH_FILTER;
	}

	gsifree(changes);
}

void ciRplWhoisUserHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;
	ciFilterMatch matches[2];
	ciServerMessageFilter * filter;
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplWhoisUserHandler called\n");
#endif
	
	assert(message->numParams == 6);
	if(message->numParams != 6)
		return; 

	nick = message->params[1];

	memset(matches, 0, sizeof(matches));
	matches[0].type = TYPE_WHOIS;
	matches[0].name = nick;
	matches[1].type = TYPE_BAN;
	matches[1].name = nick;

	filter = ciFindFilter(chat, 2, matches);
	if(!filter)
		return;

	if(filter->type == TYPE_WHOIS)
	{
		char * user;
		char * name;
		char * address;
		
		WHOISData * data = (WHOISData *)filter->data;

		user = message->params[2];
		name = message->params[5];
		address = message->params[3];

		data->user = goastrdup(user);
		if(data->user == NULL)
			return; 

		data->name = goastrdup(name);
		if(data->name == NULL)
			return; 

		data->address = goastrdup(address);
		if(data->address == NULL)
			return; 

		return;
	}

	if(filter->type == TYPE_BAN)
	{
		char * host;

		BANData * data = (BANData *)filter->data;
		assert(data != NULL);
		ASSERT_STR(data->channel);

		host = message->params[3];

		ciSocketSendf(&connection->chatSocket, "MODE %s +b *!*@%s", data->channel, host);

		ciFinishFilter(chat, filter, NULL);
	}
}

void ciRplWhoisChannelsHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;
	ciServerMessageFilter * filter;
	ciFilterMatch match;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplWhoisChannelsHandler called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	nick = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_WHOIS;
	match.name = nick;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		char * channels;
		char * str;
		char * channel;
		char ** tempPtr;
		WHOISData * data = (WHOISData *)filter->data;

		channels = message->params[2];
		str = strtok(channels, " ");
		while(str != NULL)
		{
			if(str[0] == '-')
				str++;

			if((str[0] == '@') || (str[0] == '+'))
				str++;

			channel = goastrdup(str);
			if(channel == NULL)
				return; 
			tempPtr = (char **)gsirealloc(data->channels, sizeof(char *) * (data->numChannels + 1));
			if(tempPtr == NULL)
			{
				gsifree(channel);
				return; 
			}
			data->channels = tempPtr;
			data->channels[data->numChannels] = channel;
			data->numChannels++;

			str = strtok(NULL, " ");
		}
	}
}

void ciRplEndOfWhoisHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;
	ciServerMessageFilter * filter;
	ciFilterMatch match;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplEndOfWhoisHandler called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	nick = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_WHOIS;
	match.name = nick;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		WHOISData * data = (WHOISData *)filter->data;
		ciCallbackGetUserInfoParams params;
		params.success = (CHATBool)(data->user != NULL);  
		params.nick = nick;
		params.user = data->user;
		params.name = data->name;
		params.address = data->address;
		params.numChannels = data->numChannels;
		params.channels = data->channels;

		FINISH_FILTER;
	}
}

void ciRplBanListHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;
	char * ban;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplBanListHandler called\n");
#endif

	assert(message->numParams >= 3);
	if(message->numParams < 3)
		return; 

	channel = message->params[1];
	ban = message->params[2];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETBAN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		int len;
		void * tempPtr;
		GETBANData * data = (GETBANData *)filter->data;
		assert(data != NULL);
		assert(data->numBans >= 0);

		tempPtr = gsirealloc(data->bans, sizeof(char *) * (data->numBans + 1));
		if(tempPtr == NULL)
			return; 
		data->bans = (char **)tempPtr;

		len = (int)(strlen(ban) + 1);
		tempPtr = gsimalloc((unsigned int)len);
		if(tempPtr == NULL)
			return; 
		memcpy(tempPtr, ban, (unsigned int)len);
		data->bans[data->numBans] = (char *)tempPtr;
		data->numBans++;
	}
}

void ciRplEndOfBanListHandler(CHAT chat, const ciServerMessage * message)
{
	char * channel;
	ciServerMessageFilter * filter;
	ciFilterMatch match;
	
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplEndOfBanListHandler called\n");
#endif

	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETBAN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		GETBANData * data = (GETBANData *)filter->data;
		ciCallbackEnumChannelBansParams params;
		params.success = CHATTrue;
		params.channel = channel;
		params.numBans = data->numBans;
		params.bans = data->bans;

		FINISH_FILTER;
	}
}

void ciRplWelcomeHandler(CHAT chat, const ciServerMessage * message)
{
	char * nick;

	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplWelcomeHandler called\n");
#endif

	assert(message->numParams == 2);
	if(message->numParams != 2)
		return;

	nick = message->params[0];

	if(strcmp(connection->nick, nick) != 0)
	{
		strzcpy(connection->nick, nick, MAX_NICK);
#ifdef GSI_UNICODE
		AsciiToUCS2String(connection->nick, connection->nickW);
#endif
	}

	connection->connecting = CHATFalse;
	connection->connected = CHATTrue;

	if(connection->connectCallback != NULL)
		connection->connectCallback(chat, CHATTrue, 0, connection->connectParam);
}

void ciRplSecureKeyHandler(CHAT chat, const ciServerMessage * message)
{
	char * outKeyRand;
	char * inKeyRand;
	int outKeyLen;
	int inKeyLen;

	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplSecureKeyHandler called\n");
#endif

	assert(message->numParams == 3);
	if(message->numParams != 3)
		return;

	outKeyRand = message->params[1];
	inKeyRand = message->params[2];

	outKeyLen = (int)strlen(outKeyRand);
	inKeyLen = (int)strlen(inKeyRand);
	gs_xcode_buf(outKeyRand, outKeyLen, connection->secretKey);
	gs_xcode_buf(inKeyRand, inKeyLen, connection->secretKey);
	gs_prepare_key((unsigned char *)outKeyRand, outKeyLen, &connection->chatSocket.outKey);
	gs_prepare_key((unsigned char *)inKeyRand, inKeyLen, &connection->chatSocket.inKey);

	connection->chatSocket.secure = CHATTrue;

	if(connection->loginType != CINoLogin)
	{
		ciSendLogin(chat);
	}
	else if(connection->fillInUserCallback)
	{
		ciSocketSend(&connection->chatSocket, "USRIP");
	}
	else
	{
		ciSendNickAndUser(chat);
	}
}

void ciRplCDKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	int result;
	char * msg;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplCDKeyHandler called\n");
#endif

	assert(message->numParams == 3);
	if(message->numParams != 3)
		return;

	result = atoi(message->params[1]);
	msg = message->params[2];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_CDKEY;

	filter = ciFindFilter(chat, 1, &match);
	if(filter)
	{
		ciCallbackAuthenticateCDKeyParams params;
		params.result = result;
		params.message = msg;

		FINISH_FILTER;
	}
}

void ciRplLoginHandler(CHAT chat, const ciServerMessage * message)
{
	CONNECTION;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplLoginHandler called\n");
#endif

	assert(message->numParams >= 3);
	if(message->numParams < 3)
		return;

	connection->userID = atoi(message->params[1]);
	connection->profileID = atoi(message->params[2]);

	if(connection->fillInUserCallback)
	{
		ciSocketSend(&connection->chatSocket, "USRIP");
	}
	else
	{
		ciSendNickAndUser(chat);
	}
}

void ciRplGetUdpRelayHandler(CHAT chat, const ciServerMessage * message)
{
	char *channel;
	int udpKey;
	char *udpIp;
	unsigned short udpPort;
	ciServerMessageFilter * filter;
	ciFilterMatch match;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciRplGetUdpRelayHandler called\n");
#endif
	assert(message->numParams >= 5);
	if(message->numParams < 5)
		return;

	channel = message->params[1];
	udpKey = atoi(message->params[2]);
	udpIp = message->params[3];
	udpPort = (unsigned short)atoi(message->params[4]);

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_GETUDPRELAY;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackGetUdpRelayParams params;
		params.channel = channel;
		params.udpIp = udpIp;
		params.udpPort = udpPort;
		params.udpKey = udpKey;
		FINISH_FILTER;
	}


}

void ciErrNoSuchChannelHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch matches[2];
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrNoSuchChannel called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&matches, 0, sizeof(ciFilterMatch));
	matches[0].type = TYPE_JOIN;
	matches[0].name = channel;
	matches[1].type = TYPE_CMODE;
	matches[1].name = channel;

	filter = ciFindFilter(chat, 2, matches);
	if(filter)
	{
		if(filter->type == TYPE_JOIN)
		{
			ciCallbackEnterChannelParams params;
			params.success = CHATFalse;
			params.result = CHATBadChannelName;
			params.channel = channel;

			FINISH_FILTER;

			return;
		}

		if(filter->type == TYPE_CMODE)
		{
			ciCallbackGetChannelModeParams params;
			params.success = CHATFalse;
			params.channel = channel;
			params.mode = NULL;

			FINISH_FILTER;

			return;
		}
	}

	filter = ciFindGetKeyFilter(chat, channel);
	if(filter)
	{
		ciCallbackGetGlobalKeysParams params;
		params.success = CHATFalse;
		params.user = NULL;
		params.num = 0;
		params.keys = NULL;
		params.values = NULL;

		FINISH_FILTER;

		return;
	}
}

void ciErrTooManyChannelsHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrTooManyChannels called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_JOIN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATTooManyChannels;
		params.channel = channel;

		FINISH_FILTER;
	}
}

void ciErrChannelIsFullHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrChannelIsFull called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_JOIN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATChannelIsFull;
		params.channel = channel;

		FINISH_FILTER;
	}
}

void ciErrInviteOnlyChanHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrInviteOnlyChan called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_JOIN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATInviteOnlyChannel;
		params.channel = channel;

		FINISH_FILTER;
	}
}

void ciErrBannedFromChanHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrBannedFromChan called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_JOIN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATBannedFromChannel;
		params.channel = channel;

		FINISH_FILTER;
	}
}

void ciErrBadChannelKeyHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrBadChannelKey called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_JOIN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATBadChannelPassword;
		params.channel = channel;

		FINISH_FILTER;
	}
}

void ciErrBadChanMaskHandler(CHAT chat, const ciServerMessage * message)
{
	ciFilterMatch match;
	ciServerMessageFilter * filter;
	char * channel;

#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrBadChanMask called\n");
#endif
	
	assert(message->numParams == 3);
	if(message->numParams != 3)
		return; 

	channel = message->params[1];

	memset(&match, 0, sizeof(ciFilterMatch));
	match.type = TYPE_JOIN;
	match.name = channel;
	filter = ciFindFilter(chat, 1, &match);
	if(filter != NULL)
	{
		ciCallbackEnterChannelParams params;
		params.success = CHATFalse;
		params.result = CHATBadChannelMask;
		params.channel = channel;

		FINISH_FILTER;
	}
}

void ciErrNoSuchNickHandler(CHAT chat, const ciServerMessage * message)
{
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrNoSuchNickHandler called\n");
#endif

	GSI_UNUSED(chat);
	GSI_UNUSED(message);
}

void ciErrErroneusNicknameHandler(CHAT chat, const ciServerMessage * message)
{
	CONNECTION;
	
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrErroneusNicknameHandler called\n");
#endif

	if(connection->connecting)
		ciNickError(chat, CHAT_INVALID, connection->nick, 0, NULL);
		
	GSI_UNUSED(message);
}

void ciErrLoginFailedHandler(CHAT chat, const ciServerMessage * message)
{
	CONNECTION;
	
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrLoginFailedHandler called\n");
#endif

	if(connection->connecting)
	{
		connection->connecting = CHATFalse;

		if(connection->connectCallback != NULL)
			connection->connectCallback(chat, CHATFalse, CHAT_LOGIN_FAILED, connection->connectParam);
	}
		
	GSI_UNUSED(message);
}

void ciErrNoUniqueNickHandler(CHAT chat, const ciServerMessage * message)
{
	CONNECTION;
	
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrNoUniqueNickHandler called\n");
#endif

	if(connection->connecting)
		ciNickError(chat, CHAT_NO_UNIQUENICK, "", 0, NULL);
		
	GSI_UNUSED(message);
}

void ciErrUniqueNickExpiredHandler(CHAT chat, const ciServerMessage * message)
{
	CONNECTION;
	
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrUniqueNickExpiredHandler called\n");
#endif

	if(connection->connecting)
		ciNickError(chat, CHAT_UNIQUENICK_EXPIRED, "", 0, NULL);
		
	GSI_UNUSED(message);
}

void ciErrRegisterNickFailedHandler(CHAT chat, const ciServerMessage * message)
{
	int num;
	char * nicks;
	char * nick;
	char ** nickArray;
	int i;
	CONNECTION;
	
#ifdef FEEDBACK_HANDLERS
	OutputDebugString("ciErrRegisterNickFailedHandler called\n");
#endif

	assert(message->numParams == 4);
	if(message->numParams != 4)
		return; 

	num = atoi(message->params[1]);
	nicks = message->params[2];

	if(!connection->connecting)
		return;

	nickArray = (char **)gsimalloc(sizeof(char *) * num);
	if(!nickArray)
		return;

	nick = strtok(nicks, "\\");
	for(i = 0 ; (i < num) && nick ; i++)
	{
		nickArray[i] = goastrdup(nick);
		if(!nickArray[i])
			break;

		nick = strtok(NULL, "\\");
	}

	num = i;

	ciNickError(chat, CHAT_INVALID_UNIQUENICK, connection->uniquenick, num, nickArray);

	for(i = 0 ; i < num ; i++)
		gsifree(nickArray[i]);
	gsifree(nickArray);
}
