/*
GameSpy Chat SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "chatMain.h"
#include "chatChannel.h"
#include "chatCallbacks.h"

#if defined(_WIN32)
#pragma warning(disable:4054)
#endif

#define CHANNEL_HASH_BUCKETS    7
#define USER_HASH_BUCKETS      61
#define MAX_CHANNEL           257
#define MAX_TOPIC             128
#define MAX_NAME              128
#define MAX_CACHED_USER        24
#define MAX_CACHED_ADDRESS     64

#define ASSERT_CHANNEL()   assert(channel != NULL); assert(channel[0] != '\0'); assert(strlen(channel) < MAX_CHANNEL);
#define ASSERT_USER(user)  assert(user != NULL); assert(user[0] != '\0'); assert(strlen(user) < MAX_NAME);
#define ASSERT_STR(str)    assert(str != NULL); assert(str[0] != '\0');
#define ASSERT_MODE(mode)  assert((mode >= 0) && (mode <= 3));

typedef struct ciChatChannel
{
	char name[MAX_CHANNEL];
	chatChannelCallbacks callbacks;

	HashTable operators;
	
	CHATChannelMode mode;
	CHATBool gotMode;

	char * password;

	CHATBool joinCallbackCalled;

	char topic[MAX_TOPIC];
} ciChatChannel;

typedef struct ciChatUser
{
	char name[MAX_NAME];
	char user[MAX_CACHED_USER];
	char address[MAX_CACHED_ADDRESS];
#ifdef GSI_UNICODE
	unsigned short userW[MAX_CACHED_USER];
	unsigned short addressW[MAX_CACHED_ADDRESS];
#endif
	CHATBool gotUserAndAddress;
	int mode;
} ciChatUser;

typedef struct ciUserEnumChannelsData
{
	CHAT chat;
	ciChatUser * user;
	ciUserEnumChannelsCallback callback;
	void * param;
} ciUserEnumChannelsData;

typedef struct ciUserChangedNickData
{
	CHAT chat;
	const char * oldNick;
	const char * newNick;
} ciUserChangedNickData;

typedef struct ciChannelListUsersData
{
	CHAT chat;
	int numUsers;
	char ** operators;
	int * modes;
} ciChannelListUsersData;

typedef struct ciEnumJoinedChannelsData
{
	CHAT chat;
	chatEnumJoinedChannelsCallback callback;
	void * param;
	int index;
} ciEnumJoinedChannelsData;

typedef struct ciSetUserBasicInfoData
{
	ciChatUser * chatUser;
	char * user;
	char * address;
} ciSetUserBasicInfoData;

typedef struct ciGetUserBasicInfoData
{
	CHATBool found;
	ciChatUser * chatUser;
	char * user;
	char * address;
#ifdef GSI_UNICODE  
	unsigned short * userW;
	unsigned short * addressW;
#endif
} ciGetUserBasicInfoData;

typedef struct ciClearAllUsersData
{
	CHAT chat;
	ciChatChannel * channel;
} ciClearAllUsersData;

int GS_STATIC_CALLBACK ciEnteringChannelComparator(const void *param1, const void *param2);


static int ciHashString(const char * str, int numBuckets)
{
	unsigned int hash;
	int c;
	
	ASSERT_STR(str);

	hash = 0;
	while((c = *str++) != '\0')
		hash += (unsigned int)tolower(c);

	return ((int)hash % numBuckets);
}

static ciChatChannel * ciGetChannel(ciConnection * connection, const char * channel)
{
	ciChatChannel * chatChannel;
	ciChatChannel channelTemp;

	strzcpy(channelTemp.name, channel, MAX_CHANNEL);
	chatChannel = (ciChatChannel *)TableLookup(connection->channelTable, &channelTemp);

	return chatChannel;
}

static int ciChannelTableHashFn(const void * elem, int numBuckets)
{
	int hash;

	assert(elem != NULL);
	assert(numBuckets > 0);

	hash = ciHashString(((ciChatChannel *)elem)->name, numBuckets);

	return hash;
}

static int GS_STATIC_CALLBACK ciChannelTableCompareFn(const void * elem1, const void * elem2)
{
	const char * str1;
	const char * str2;

	assert(elem1 != NULL);
	assert(elem2 != NULL);

	str1 = ((ciChatChannel *)elem1)->name;
	str2 = ((ciChatChannel *)elem2)->name;
	assert(str1 != NULL);
	assert(str2 != NULL);
	ASSERT_STR(str1);
	ASSERT_STR(str2);

	return strcasecmp(str1, str2);
}

static void ciChannelTableElementFreeFn(void * elem)
{
	ciChatChannel * channel;

	assert(elem != NULL);

	channel = (ciChatChannel *)elem;
	gsifree(channel->password);
	if(channel->operators)
		TableFree(channel->operators);
}

static int ciUserTableHashFn(const void * elem, int numBuckets)
{
	int hash;

	assert(elem != NULL);
	assert(numBuckets > 0);

	hash = ciHashString(((ciChatUser *)elem)->name, numBuckets);

	return hash;
}

static int GS_STATIC_CALLBACK ciUserTableCompareFn(const void * elem1, const void * elem2)
{
	const char * str1;
	const char * str2;

	assert(elem1 != NULL);
	assert(elem2 != NULL);

	if (elem1 == NULL || elem2 == NULL)
	{
		if (elem1 == NULL && elem2 == NULL)
			return 0;
		else if (elem1 == NULL)
			return -1;
		else 
			return 1;
	}

	str1 = ((ciChatUser *)elem1)->name;
	str2 = ((ciChatUser *)elem2)->name;
	ASSERT_STR(str1);
	ASSERT_STR(str2);

	return strcasecmp(str1, str2);
}

static void ciUserTableElementFreeFn(void * elem)
{
	assert(elem != NULL);
	
	GSI_UNUSED(elem);

}

CHATBool ciInitChannels(ciConnection * connection)
{
	connection->channelTable = TableNew2(sizeof(ciChatChannel), CHANNEL_HASH_BUCKETS, 2, ciChannelTableHashFn, ciChannelTableCompareFn, ciChannelTableElementFreeFn);
	if(connection->channelTable == NULL)
		return CHATFalse;

	connection->enteringChannelList = ArrayNew(sizeof(ciChatChannel), 0, NULL);
	if(connection->enteringChannelList == NULL)
	{
		TableFree(connection->channelTable);
		return CHATFalse;
	}

	return CHATTrue;
}

void ciCleanupChannels(CHAT chat)
{
	CONNECTION;

	if(connection->channelTable != NULL)
		TableFree(connection->channelTable);

	if(connection->enteringChannelList != NULL)
		ArrayFree(connection->enteringChannelList);
}

void ciChannelEntering(CHAT chat, const char * channel)
{
	ciChatChannel chatChannel;

	CONNECTION;

	memset(&chatChannel, 0, sizeof(ciChatChannel));
	strzcpy(chatChannel.name, channel, MAX_CHANNEL);

	ArrayAppend(connection->enteringChannelList, &chatChannel);
}

int GS_STATIC_CALLBACK ciEnteringChannelComparator(const void *param1, const void *param2)
{
	ciChatChannel * channel1 = (ciChatChannel *)param1;
	ciChatChannel * channel2 = (ciChatChannel *)param2;
	return strcasecmp(channel1->name, channel2->name);
}

CHATBool ciIsEnteringChannel(CHAT chat, const char * channel)
{
	int i;
	int count;
	ciChatChannel * chatChannel;

	CONNECTION;

	count = ArrayLength(connection->enteringChannelList);
	for(i = 0 ; i < count ; i++)
	{
		chatChannel = (ciChatChannel *)ArrayNth(connection->enteringChannelList, i);
		assert(chatChannel);
		if(strcasecmp(chatChannel->name, channel) == 0)
			return CHATTrue;
	}

	return CHATFalse;
}

void ciChannelEntered(CHAT chat, const char * channel, chatChannelCallbacks * callbacks)
{
	ciChatChannel chatChannel;
	char * password;
	int index;
	CONNECTION;

	ASSERT_CHANNEL();
	assert(callbacks != NULL);

	password = (char *)gsimalloc(2);
	if(password == NULL)
		return; 
	strcpy(password, "");

	memset(&chatChannel, 0, sizeof(ciChatChannel));
	chatChannel.callbacks = *callbacks;
	strzcpy(chatChannel.name, channel, MAX_CHANNEL);
	chatChannel.operators = TableNew2(sizeof(ciChatUser), USER_HASH_BUCKETS, 2, ciUserTableHashFn, ciUserTableCompareFn, ciUserTableElementFreeFn);
	if(chatChannel.operators == NULL)
		return; 
	chatChannel.gotMode = CHATFalse;
	chatChannel.password = password;
	chatChannel.joinCallbackCalled = CHATFalse;
	chatChannel.topic[0] = '\0';

	index = ArraySearch(connection->enteringChannelList, &chatChannel, ciEnteringChannelComparator, 0, 0);
	if(index != NOT_FOUND)
		ArrayRemoveAt(connection->enteringChannelList, index);

	TableEnter(connection->channelTable, &chatChannel);
}

void ciChannelLeft(CHAT chat, const char * channel)
{
	ciChatChannel chatChannel;
	int index;
	CONNECTION;
	
	ASSERT_CHANNEL();

	strzcpy(chatChannel.name, channel, MAX_CHANNEL);

	index = ArraySearch(connection->enteringChannelList, &chatChannel, ciEnteringChannelComparator, 0, 0);
	if(index != NOT_FOUND)
	{
		ArrayRemoveAt(connection->enteringChannelList, index);
	}
	else
	{
		TableRemove(connection->channelTable, &chatChannel);

		// This will assert if we don't think we're in this channel.
		//assert(rcode != 0);
	}
}

chatChannelCallbacks * ciGetChannelCallbacks(CHAT chat, const char * channel)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return NULL; 

	return &chatChannel->callbacks;
}

static void ciChannelListUsersMap(void * elem, void * clientData)
{
	ciChatUser * user;
	ciChannelListUsersData * data;
	void * tempPtr;

	assert(elem != NULL);
	assert(clientData != NULL);

	user = (ciChatUser *)elem;
	ASSERT_USER(user->name);

	data = (ciChannelListUsersData *)clientData;
	assert(data->numUsers >= 0);
#ifdef _DEBUG
	{
	int i;
	for(i = 0 ; i < data->numUsers ; i++)
	{
		ASSERT_USER(data->operators[i]);
		ASSERT_MODE(data->modes[i]);
	}
	}
#endif

	tempPtr = gsirealloc(data->operators, sizeof(char *) * (data->numUsers + 1));
	if(tempPtr == NULL)
	{
		assert(0);
		return; 
	}
	data->operators = (char **)tempPtr;
	tempPtr = gsirealloc(data->modes, sizeof(int) * (data->numUsers + 1));
	if(tempPtr == NULL)
	{
		assert(0);
		return; 
	}
	data->modes = (int *)tempPtr;

	data->operators[data->numUsers] = user->name;
	data->modes[data->numUsers] = user->mode;
	data->numUsers++;
}

void ciChannelListUsers(CHAT chat, const char * channel, ciChannelListUsersCallback callback, void * param)
{
	ciChatChannel * chatChannel;
	ciChannelListUsersData data;
	CONNECTION;

	ASSERT_CHANNEL();
	assert(callback != NULL);

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return; 

	data.chat = chat;
	data.numUsers = 0;
	data.operators = NULL;
	data.modes = NULL;
	TableMap(chatChannel->operators, ciChannelListUsersMap, &data);

	callback(chat, channel, data.numUsers, (const char **)data.operators, data.modes, param);

	gsifree(data.operators);
	gsifree(data.modes);
}

CHATBool ciInChannel(CHAT chat, const char * channel)
{
	CONNECTION;

	if(ciGetChannel(connection, channel) == NULL)
		return CHATFalse;

	return CHATTrue;
}

CHATBool ciGetChannelMode(CHAT chat, const char * channel, CHATChannelMode * mode)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return CHATFalse; 

	if(!chatChannel->gotMode)
		return CHATFalse; 

	memcpy(mode, &chatChannel->mode, sizeof(CHATChannelMode));

	return CHATTrue;
}

void ciSetChannelMode(CHAT chat, const char * channel, CHATChannelMode * mode)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return; 

	chatChannel->gotMode = CHATTrue;

	memcpy(&chatChannel->mode, mode, sizeof(CHATChannelMode));
}

void ciSetChannelPassword(CHAT chat, const char * channel, const char * password)
{
	int len;
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return; 

	gsifree(chatChannel->password);

	if(password == NULL)
		password = "";
	len = (int)(strlen(password) + 1);
	chatChannel->password = (char *)gsimalloc((unsigned int)len);
	if(chatChannel->password == NULL)
		return; 
	memcpy(chatChannel->password, password, (unsigned int)len);
}

const char * ciGetChannelPassword(CHAT chat, const char * channel)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return NULL; 

	return chatChannel->password;
}

void ciJoinCallbackCalled(CHAT chat, const char * channel)
{
	ciChatChannel * chatChannel;
	CONNECTION;

	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return; 

	chatChannel->joinCallbackCalled = CHATTrue;
}

CHATBool ciWasJoinCallbackCalled(CHAT chat, const char * channel)
{
	ciChatChannel * chatChannel;
	CONNECTION;

	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return CHATFalse; 

	return chatChannel->joinCallbackCalled;
}

void ciSetChannelTopic(CHAT chat, const char * channel, const char * topic)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return; 

	strzcpy(chatChannel->topic, topic, MAX_TOPIC);
}

const char * ciGetChannelTopic(CHAT chat, const char * channel)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return NULL; 

	return chatChannel->topic;
}

int ciGetChannelNumUsers(CHAT chat, const char * channel)
{
	ciChatChannel * chatChannel;
	CONNECTION;
	
	ASSERT_CHANNEL();

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return -1;

	return TableCount(chatChannel->operators);
}

void ciUserEnteredChannel(CHAT chat, const char * name, const char * channel, int mode, const char * user, const char * address)
{
	ciChatUser chatUser;
	ciChatChannel * chatChannel;
	CONNECTION;

	ASSERT_USER(name);
	ASSERT_STR(channel);
	ASSERT_MODE(mode);

	chatChannel = ciGetChannel(connection, channel);
	if(!chatChannel)
		return;

	memset(&chatUser, 0, sizeof(ciChatUser));
	strzcpy(chatUser.name, name, MAX_NAME);
	if(user && address)
	{
		strzcpy(chatUser.user, user, MAX_CACHED_USER);
		strzcpy(chatUser.address, address, MAX_CACHED_ADDRESS);
		chatUser.gotUserAndAddress = CHATTrue;

#ifdef GSI_UNICODE 
		AsciiToUCS2String(chatUser.user, chatUser.userW);
		AsciiToUCS2String(chatUser.address, chatUser.addressW);
#endif
	}
	else
	{
		chatUser.gotUserAndAddress = CHATFalse;
	}
	chatUser.mode = mode;

	TableEnter(chatChannel->operators, &chatUser);

	assert(TableLookup(chatChannel->operators, &chatUser) != NULL);
}

void ciUserLeftChannel(CHAT chat, const char * user, const char * channel)
{
	ciChatUser chatUser;
	ciChatChannel * chatChannel;
	CONNECTION;

	ASSERT_USER(user);
	ASSERT_STR(channel);

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return; 

	strzcpy(chatUser.name, user, MAX_NAME);

	TableRemove(chatChannel->operators, &chatUser);
}

static void ciUserEnumChannelsMap(void * elem, void * clientData)
{
	ciChatChannel * channel;
	ciChatUser * user;
	ciUserEnumChannelsData * data;

	assert(elem != NULL);
	assert(clientData != NULL);

	channel = (ciChatChannel *)elem;
	assert(channel->operators != NULL);

	data = (ciUserEnumChannelsData *)clientData;
	assert(data->user != NULL);
	assert(data->user->name[0] != '\0');
	assert(data->callback != NULL);

	user = (ciChatUser *)TableLookup(channel->operators, data->user);
	if(user != NULL)
	{
		data->callback(data->chat, data->user->name, channel->name, data->param);
	}
}

void ciUserEnumChannels(CHAT chat, const char * user, ciUserEnumChannelsCallback callback, void * param)
{
	ciChatUser chatUser;
	ciUserEnumChannelsData data;
	CONNECTION;

	ASSERT_USER(user);
	assert(callback != NULL);

	strzcpy(chatUser.name, user, MAX_NAME);
	data.chat = chat;
	data.user = &chatUser;
	data.callback = callback;
	data.param = param;

	TableMap(connection->channelTable, ciUserEnumChannelsMap, &data);
}

static void ciUserChangeNickMap(void * elem, void * clientData)
{
	ciChatChannel * channel;
	ciChatUser tempUser;
	ciChatUser * user;
	ciUserChangedNickData * data;
	ciCallbackUserChangedNickParams params;
	int rcode;

	assert(elem != NULL);
	assert(clientData != NULL);

	channel = (ciChatChannel *)elem;
	assert(channel->operators != NULL);

	data = (ciUserChangedNickData *)clientData;
	ASSERT_USER(data->newNick);
	ASSERT_USER(data->oldNick);

	user = (ciChatUser *)TableLookup(channel->operators, data->oldNick);
	if(user != NULL)
	{
		memcpy(&tempUser, user, sizeof(ciChatUser));

		rcode = TableRemove(channel->operators, user);
		assert(rcode != 0);
		user = &tempUser;

		strzcpy(user->name, data->newNick, MAX_NAME);

		TableEnter(channel->operators, user);

		if(ciWasJoinCallbackCalled(data->chat, channel->name))  
		{
			if(channel->callbacks.userChangedNick != NULL)
			{
				params.channel = channel->name;
				params.oldNick = (char *)data->oldNick;
				params.newNick = (char *)data->newNick;
				ciAddCallback(data->chat, CALLBACK_USER_CHANGED_NICK, (void*)channel->callbacks.userChangedNick, &params, channel->callbacks.param, 0, channel->name);
			}
		}
	}
	GSI_UNUSED(rcode);
}

void ciUserChangedNick(CHAT chat, const char * oldNick, const char * newNick)
{
	ciUserChangedNickData data;
	CONNECTION;

	ASSERT_USER(oldNick);
	ASSERT_USER(newNick);

	data.chat = chat;
	data.oldNick = oldNick;
	data.newNick = newNick;

	TableMap(connection->channelTable, ciUserChangeNickMap, &data);
}

void ciUserChangedMode(CHAT chat, const char * user, const char * channel, int mode, CHATBool enabled)
{
	ciChatChannel channelTemp;
	ciChatChannel * chatChannel;
	ciChatUser userTemp;
	ciChatUser * chatUser;
	ciCallbackUserModeChangedParams params;
	CONNECTION;

	ASSERT_USER(user);
	ASSERT_STR(channel);
	ASSERT_MODE(mode);

	strzcpy(channelTemp.name, channel, MAX_CHANNEL);
	chatChannel = (ciChatChannel *)TableLookup(connection->channelTable, &channelTemp);
	if(chatChannel == NULL)
		return; 

	strzcpy(userTemp.name, user, MAX_NAME);
	chatUser = (ciChatUser *)TableLookup(chatChannel->operators, &userTemp);
	if(chatUser == NULL)
		return; 

	if(enabled)
		chatUser->mode |= mode;
	else
		chatUser->mode &= ~mode;

	if(chatChannel->callbacks.userModeChanged != NULL)
	{
		params.channel = (char *)channel;
		params.user = (char *)user;
		params.mode = chatUser->mode;
		ciAddCallback(chat, CALLBACK_USER_MODE_CHANGED, (void*)chatChannel->callbacks.userModeChanged, &params, chatChannel->callbacks.param, 0, channel);
	}
}

CHATBool ciUserInChannel(CHAT chat, const char * channel, const char * user)
{
	ciChatChannel * chatChannel;
	ciChatUser chatUser;
	CONNECTION;

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return CHATFalse;

	strzcpy(chatUser.name, user, MAX_NAME);
	if(TableLookup(chatChannel->operators, &chatUser) == NULL)
		return CHATFalse;

	return CHATTrue;
}

int ciGetUserMode(CHAT chat, const char * channel, const char * user)
{
	ciChatChannel * chatChannel;
	ciChatUser userTemp;
	ciChatUser * chatUser;
	CONNECTION;

	chatChannel = ciGetChannel(connection, channel);
	if(chatChannel == NULL)
		return -1; 

	strzcpy(userTemp.name, user, MAX_NAME);
	chatUser = (ciChatUser *)TableLookup(chatChannel->operators, &userTemp);
	if(chatUser == NULL)
		return -1; 

	return chatUser->mode;
}


static void ciEnumJoinedChannelsMap(void * elem, void * clientData)
{
	ciEnumJoinedChannelsData * data;
	ciChatChannel * channel;
	assert(elem != NULL);
	assert(clientData != NULL);

	channel = (ciChatChannel *)elem;

	data = (ciEnumJoinedChannelsData *)clientData;
	assert(data->callback != NULL);

#ifdef GSI_UNICODE
	{
		unsigned short* name_W = UTF8ToUCS2StringAlloc(channel->name);
		data->callback(data->chat, data->index++, name_W, data->param);
		gsifree(name_W);
	}
#else
	data->callback(data->chat, data->index++, channel->name, data->param);
#endif
}

void ciEnumJoinedChannels(CHAT chat,
					  chatEnumJoinedChannelsCallback callback,
					  void * param)
{
	ciEnumJoinedChannelsData data;
	CONNECTION;
	CONNECTED;
	data.callback = callback;
	data.param = param;
	data.index = 0;
	data.chat = chat;

	TableMap(connection->channelTable,ciEnumJoinedChannelsMap,&data);
}

static void ciSetUserBasicInfoMap(void * elem, void * clientData)
{
	ciChatChannel * channel;
	ciChatUser * user;
	ciSetUserBasicInfoData * data;

	assert(elem != NULL);
	assert(clientData != NULL);

	data = (ciSetUserBasicInfoData *)clientData;

	channel = (ciChatChannel *)elem;
	assert(channel->operators != NULL);

	user = (ciChatUser *)TableLookup(channel->operators, data->chatUser);
	if(user != NULL)
	{
		strzcpy(user->user, data->user, MAX_CACHED_USER);
		strzcpy(user->address, data->address, MAX_CACHED_ADDRESS);
		user->gotUserAndAddress = CHATTrue;

#ifdef GSI_UNICODE
		AsciiToUCS2String(user->user, user->userW);
		AsciiToUCS2String(user->address, user->addressW);
#endif
	}
}

void ciSetUserBasicInfo(CHAT chat, const char * nick, const char * user, const char * address)
{
	ciChatUser chatUser;
	ciSetUserBasicInfoData data;
	CONNECTION;

	ASSERT_USER(nick);

	strzcpy(chatUser.name, nick, MAX_NAME);
	data.chatUser = &chatUser;
	data.user = (char *)user;
	data.address = (char *)address;

	TableMap(connection->channelTable, ciSetUserBasicInfoMap, &data);
}

static void ciGetUserBasicInfoMap(void * elem, void * clientData)
{
	ciChatChannel * channel;
	ciChatUser * user;
	ciGetUserBasicInfoData * data;

	assert(elem != NULL);
	assert(clientData != NULL);

	data = (ciGetUserBasicInfoData *)clientData;

	if(data->found && (strcmp(data->address, "*") != 0))
		return;

	channel = (ciChatChannel *)elem;
	assert(channel->operators != NULL);

	user = (ciChatUser *)TableLookup(channel->operators, data->chatUser);
	if(user != NULL)
	{
		if(user->gotUserAndAddress)
		{
			data->found = CHATTrue;
			data->user = user->user;
			data->address = user->address;
#ifdef GSI_UNICODE
			data->userW = user->userW;
			data->addressW = user->addressW;
#endif
		}
	}
}

CHATBool ciGetUserBasicInfoA(CHAT chat, const char * nick, const char ** user, const char ** address)
{
	ciChatUser chatUser;
	ciGetUserBasicInfoData data;
	CONNECTION;

	ASSERT_USER(nick);

	strzcpy(chatUser.name, nick, MAX_NAME);
	data.chatUser = &chatUser;
	data.found = CHATFalse;

	TableMap(connection->channelTable, ciGetUserBasicInfoMap, &data);

	if(!data.found)
		return CHATFalse;

	if(user)
		*user = data.user;
	if(address)
		*address = data.address;

	return CHATTrue;
}

#ifdef GSI_UNICODE
CHATBool ciGetUserBasicInfoW(CHAT chat, const char * nick, const unsigned short ** user, const unsigned short ** address)
{
	ciChatUser chatUser;
	ciGetUserBasicInfoData data;
	CONNECTION;

	ASSERT_USER(nick);

	strzcpy(chatUser.name, nick, MAX_NAME);
	data.chatUser = &chatUser;
	data.found = CHATFalse;

	TableMap(connection->channelTable, ciGetUserBasicInfoMap, &data);

	if(!data.found)
		return CHATFalse;

	if(user)
		*user = data.userW;
	if(address)
		*address = data.addressW;
	return CHATTrue;
}
#endif

static void ciClearAllUsersUsersMap(void * elem, void * clientData)
{
	ciClearAllUsersData * data;
	ciChatUser * user;
	ciChatChannel * channel;

	assert(elem != NULL);
	assert(clientData != NULL);

	user = (ciChatUser *)elem;

	data = (ciClearAllUsersData *)clientData;
	channel = data->channel;

	TableRemove(channel->operators, user);
}

static void ciClearAllUsersChannelMap(void * elem, void * clientData)
{
	ciChatChannel * channel;
	ciClearAllUsersData data;
	CHAT chat;

	assert(elem != NULL);
	assert(clientData != NULL);

	channel = (ciChatChannel *)elem;
	assert(channel->operators != NULL);

	chat = (CHAT)clientData;

	data.chat = chat;
	data.channel = channel;

	TableMapSafe(channel->operators, ciClearAllUsersUsersMap, &data);

	if(channel->callbacks.userListUpdated != NULL)
	{
		ciCallbackUserListUpdatedParams params;
		params.channel = channel->name;
		ciAddCallback(chat, CALLBACK_USER_LIST_UPDATED, (void*)channel->callbacks.userListUpdated, &params, channel->callbacks.param, 0, channel->name);
	}
}

void ciClearAllUsers(CHAT chat)
{
	CONNECTION;

	TableMap(connection->channelTable, ciClearAllUsersChannelMap, chat);
}
