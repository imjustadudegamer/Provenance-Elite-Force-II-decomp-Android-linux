
#include <limits.h>
#include "../gp.h"
#include "../../common/gsAvailable.h"


#define PROFILES_MAX                  10000

#define LOG_FILE                      "gpstress.log"

#define NEXT_OP_TIME_MIN              2000000
#define NEXT_OP_TIME_MAX              2000000

#define IS_INSTANT_OP(op)             (((op) >= FIRST_INSTANT_OP) && ((op) <= LAST_INSTANT_OP))

#define CONNECT_TIME_MIN              20000
#define CONNECT_TIME_MAX              20000

#define SHUTDOWN_DISCONNECTS_PER_SEC  50

#define DEFAULT_RUN_TIME              (2 * 60 * 60 * 1000)
#define DEFAULT_MAX_CONNECTED         100

#define SHOW_INFO_TIME                1000

#define DEFAULT_CONNECTS_PER_SEC      15

#define MAX_OTHERS                    20

#define PRODUCT_ID_MAX                18

#define VALIDATION_MAX_NICKS          512

#define MAX_OUTSTANDING_CONNECTIONS   1

#ifndef __cplusplus
typedef enum
{
	false,
	true
} bool;
#endif

typedef enum
{
	OpNull = -1,

	OpConnect,
	OpNewProfile,
	OpGetInfo,
#ifdef TEST_SEARCH
	OpProfileSearch,
	OpFindPlayers,
#endif

#define FIRST_INSTANT_OP OpSetInfo
	OpSetInfo,
	OpBuddyRequest,
	OpDeleteBuddy,
	OpSetStatus1,  
	OpSetStatus2,
	OpSetStatus3,
	OpSetStatus4,
	OpSetStatus5,
	OpSendBuddyMessage,
	OpSetInvitable,
#define LAST_INSTANT_OP OpSetInvitable

	NumOps
} Op;

typedef struct
{
	int index;

	int userID;
	char email[GP_EMAIL_LEN];
	char password[GP_PASSWORD_LEN];
	int profileID;
	char nick[GP_NICK_LEN];
	bool invalid;

	GPConnection gp;               
	bool gpInitialized;            
	unsigned long disconnectTime;  
	GPProfile others[MAX_OTHERS];  
	int numOthers;                 
	unsigned long connectTime;     
	bool connected;                

	Op activeOp;               
	unsigned long nextOpTime;  
	int completedOps;          
} Profile;

typedef struct
{
	GPResult result;
	char nicks[VALIDATION_MAX_NICKS][GP_NICK_LEN];
	int numNicks;
	Profile * profile;
} ValidationData;

char profilesFile[MAX_PATH];         
Profile profiles[PROFILES_MAX];      
int numProfiles;                     
int numConnections;                  
int maxConnections;                  
int highestConnections;              
int totalConnections;                
int numConnected;                    
int highestConnected;                
int totalConnected;                  
int connectsPerSecond;               
unsigned long lastConnectTime;       
unsigned long startShutdownTime;     
bool shuttingDown;                   
unsigned long runTime;               

#pragma warning ( disable: 4702 )
#pragma warning ( disable: 4127 )
bool ReadField(FILE * fp, char * field)
{
	char * str = field;
	int c;
	*str = '\0';
	while(1)
	{
		c = fgetc(fp);

		if((c == ',') || (c == '\n') || (c == -1))
		{
			if(field == str)
				return false;

			*str = '\0';
			return true;
		}

		*str++ = (char)c;
	}

	return false;
}

bool ReadProfile(FILE * fp, Profile * profile)
{
	char intBuffer[16];

	if(!ReadField(fp, intBuffer))
		return false;
	profile->userID = atoi(intBuffer);
	assert(profile->userID);

	ReadField(fp, profile->email);

	ReadField(fp, profile->password);

	ReadField(fp, intBuffer);
	profile->profileID = atoi(intBuffer);
	assert(profile->profileID);

	ReadField(fp, profile->nick);

	return true;
}

bool LoadProfiles(void)
{
#if 0
	FILE * fp;

	fp = fopen(profilesFile, "rt");
	if(!fp)
	{
		printf("Failed to open the profiles file (%s)!\n", profilesFile);
		return false;
	}

	while(ReadProfile(fp, &profiles[numProfiles]))
	{
		numProfiles++;

		if(numProfiles == PROFILES_MAX)
		{
			printf("Too many profiles in the profile file!\nIncrease PROFILES_MAX (%d)\n", PROFILES_MAX);
			return false;
		}
	}

	fclose(fp);

	printf("Loaded %d profiles from %s\n", numProfiles, profilesFile);
#else
	int i;

	for(i = 0 ; i < maxConnections ; i++)
	{
		strcpy(profiles[i].nick, "gpstress");
		sprintf(profiles[i].email, "gpstress%04d@gamespy.com", i);
		strcpy(profiles[i].password, "gpstress");
	}

	numProfiles = i;
#endif

	return true;
}

char RandomChar(void)
{
	static const char characters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789!@#$%^&*()_+-=~`<,>.?/:;\"'{[}]|\\";
	int index;

	index = (rand() % (sizeof(characters) - 1));

	return characters[index];
}

char * RandomString(char * buffer)
{
	int i;
	int len;

	len = ((rand() % 10) + 5);

	for(i = 0 ; i < len ; i++)
		buffer[i] = RandomChar();
	buffer[i] = '\0';

	return buffer;
}

int RandomInt(int min, int max)
{
	int range = ((max - min) + 1);
	return (min + ((rand() * range) / (RAND_MAX + 1)));
}

bool RandomBool(void)
{
	return (bool)RandomInt(0, 1);
}

GPProfile RandomOther(Profile * profile)
{
	int nIndex;

	assert(profile->numOthers > 0);

	nIndex = RandomInt(0, profile->numOthers - 1);
	assert(nIndex >= 0);
	assert(nIndex < profile->numOthers);

	return profile->others[nIndex];
}

void RandomServer(void)
{
#if 0
	strcpy(GPConnectionManagerHostname, "mrpants");
#else
	static const char * servers[] =
	{
#if 1
		"mrpants",
		"mrpants1"
#else
		"aphexgp1",
		"aphexgp2"
#endif
	};

	const char * server = servers[RandomInt(0, (sizeof(servers) / sizeof(*servers)) - 1)];
	strcpy(GPConnectionManagerHostname, server);
#endif
}

void MessageOther(Profile * profile, GPProfile other)
{
#if 1
	int pid;

	gpIDFromProfile(&profile->gp, other, &pid);
	switch(pid)
	{
	case 100001: 
	case 100002: 
	case 100013: 
		return;
	}

	gpSendBuddyMessage(&profile->gp, other, "STRESS-TEST");
#endif
}

void Log(const char * buffer)
{
	FILE * fp;

	static char timeBuffer[64];
	time_t thetime;
	struct tm *ltime;
	time(&thetime);
	ltime = localtime(&thetime);
	sprintf(timeBuffer, "%-2.2d/%-2.2d/%-2.2d %-2.2d:%-2.2d:%-2.2d: ", ltime->tm_mon+1, ltime->tm_mday, ltime->tm_year % 100, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);

	fp = fopen(LOG_FILE, "at");
	if(fp)
	{
		fprintf(fp, "%s%s", timeBuffer, buffer);
		fclose(fp);
	}

	OutputDebugString(timeBuffer);
	OutputDebugString(buffer);

	printf("%s%s", timeBuffer, buffer);
}

void GetInfoCallback(GPConnection * connnection, GPGetInfoResponseArg * arg, Profile * profile);
void AddOther(Profile * profile, GPProfile gpProfile);
void PrintOp(Profile * profile, const char * opString);

void ErrorCallback(GPConnection * connection, GPErrorArg * arg, Profile * profile)
{
	char * errorCodeString;
	char * resultString;
	static char buffer[4098];

	switch(arg->errorCode)
	{
	case GP_ADDBUDDY_ALREADY_BUDDY:
	case GP_BM_NOT_BUDDY:
		return;
	}

#define RESULT(x) case x: resultString = #x; break;
	switch(arg->result)
	{
	RESULT(GP_NO_ERROR)
	RESULT(GP_MEMORY_ERROR)
	RESULT(GP_PARAMETER_ERROR)
	RESULT(GP_NETWORK_ERROR)
	RESULT(GP_SERVER_ERROR)
	default:
		resultString = "Unknown result!\n";
	}

#define ERRORCODE(x) case x: errorCodeString = #x; break;
	switch(arg->errorCode)
	{
	ERRORCODE(GP_GENERAL)
	ERRORCODE(GP_PARSE)
	ERRORCODE(GP_NOT_LOGGED_IN)
	ERRORCODE(GP_BAD_SESSKEY)
	ERRORCODE(GP_DATABASE)
	ERRORCODE(GP_NETWORK)
	ERRORCODE(GP_FORCED_DISCONNECT)
	ERRORCODE(GP_CONNECTION_CLOSED)
	ERRORCODE(GP_LOGIN)
	ERRORCODE(GP_LOGIN_TIMEOUT)
	ERRORCODE(GP_LOGIN_BAD_NICK)
	ERRORCODE(GP_LOGIN_BAD_EMAIL)
	ERRORCODE(GP_LOGIN_BAD_PASSWORD)
	ERRORCODE(GP_LOGIN_BAD_PROFILE)
	ERRORCODE(GP_LOGIN_PROFILE_DELETED)
	ERRORCODE(GP_LOGIN_CONNECTION_FAILED)
	ERRORCODE(GP_LOGIN_SERVER_AUTH_FAILED)
	ERRORCODE(GP_LOGIN_BAD_UNIQUENICK)
	ERRORCODE(GP_LOGIN_BAD_PREAUTH)
	ERRORCODE(GP_NEWUSER)
	ERRORCODE(GP_NEWUSER_BAD_NICK)
	ERRORCODE(GP_NEWUSER_BAD_PASSWORD)
	ERRORCODE(GP_NEWUSER_UNIQUENICK_INVALID)
	ERRORCODE(GP_NEWUSER_UNIQUENICK_INUSE)
	ERRORCODE(GP_UPDATEUI)
	ERRORCODE(GP_UPDATEUI_BAD_EMAIL)
	ERRORCODE(GP_NEWPROFILE)
	ERRORCODE(GP_NEWPROFILE_BAD_NICK)
	ERRORCODE(GP_NEWPROFILE_BAD_OLD_NICK)
	ERRORCODE(GP_UPDATEPRO)
	ERRORCODE(GP_UPDATEPRO_BAD_NICK)
	ERRORCODE(GP_ADDBUDDY)
	ERRORCODE(GP_ADDBUDDY_BAD_FROM)
	ERRORCODE(GP_ADDBUDDY_BAD_NEW)
	ERRORCODE(GP_ADDBUDDY_ALREADY_BUDDY)
	ERRORCODE(GP_AUTHADD)
	ERRORCODE(GP_AUTHADD_BAD_FROM)
	ERRORCODE(GP_AUTHADD_BAD_SIG)
	ERRORCODE(GP_STATUS)
	ERRORCODE(GP_BM)
	ERRORCODE(GP_BM_NOT_BUDDY)
	ERRORCODE(GP_GETPROFILE)
	ERRORCODE(GP_GETPROFILE_BAD_PROFILE)
	ERRORCODE(GP_DELBUDDY)
	ERRORCODE(GP_DELBUDDY_NOT_BUDDY)
	ERRORCODE(GP_DELPROFILE)
	ERRORCODE(GP_DELPROFILE_LAST_PROFILE)
	ERRORCODE(GP_SEARCH)
	ERRORCODE(GP_SEARCH_CONNECTION_FAILED)
	ERRORCODE(GP_CHECK)
	ERRORCODE(GP_CHECK_BAD_EMAIL)
	ERRORCODE(GP_CHECK_BAD_NICK)
	ERRORCODE(GP_CHECK_BAD_PASSWORD)
	ERRORCODE(GP_REVOKE)
	ERRORCODE(GP_REVOKE_NOT_BUDDY)
	ERRORCODE(GP_REGISTERUNIQUENICK)
	ERRORCODE(GP_REGISTERUNIQUENICK_TAKEN)
	ERRORCODE(GP_REGISTERUNIQUENICK_RESERVED)
	ERRORCODE(GP_REGISTERUNIQUENICK_BAD_NAMESPACE)
	default:
		errorCodeString = "Unknown error code!\n";
	}

	if(arg->fatal)
		sprintf(buffer, "%04d: FATAL ERROR", profile->index);
	else
		sprintf(buffer, "%04d: ERROR", profile->index);
	sprintf(buffer + strlen(buffer), ", RESULT: %s (%d)", resultString, arg->result);
	sprintf(buffer + strlen(buffer), ", ERROR CODE: %s (0x%X)", errorCodeString, arg->errorCode);
	sprintf(buffer + strlen(buffer), ", ERROR STRING: %s\n", arg->errorString);
	Log(buffer);

	profile->disconnectTime = 0;
	GSI_UNUSED(connection);
}

void RecvBuddyStatusCallback(GPConnection * connection, GPRecvBuddyStatusArg * arg, Profile * profile)
{
	PrintOp(profile, "RecvBuddyStatusCallback");

#if 1
	if(RandomBool())
		gpGetInfo(&profile->gp, arg->profile, GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);

	if(RandomInt(0, 9) == 0)
		MessageOther(profile, arg->profile);

	AddOther(profile, arg->profile);
#endif
	GSI_UNUSED(connection);
}

#if 1
void RecvBuddyRequestCallback(GPConnection * connection, GPRecvBuddyRequestArg * arg, Profile * profile)
{
	PrintOp(profile, "RecvBuddyRequestCallback");

	if(RandomBool())
		gpGetInfo(&profile->gp, arg->profile, GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);

	if(RandomBool())
		gpAuthBuddyRequest(&profile->gp, arg->profile);

	AddOther(profile, arg->profile);
	GSI_UNUSED(connection);
}

void RecvBuddyMessageCallback(GPConnection * connection, GPRecvBuddyMessageArg * arg, Profile * profile)
{
	PrintOp(profile, "RecvBuddyMessageCallback");

	if(RandomBool())
		gpGetInfo(&profile->gp, arg->profile, GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);

	if(RandomInt(0, 9) == 0)
		MessageOther(profile, arg->profile);

	AddOther(profile, arg->profile);
	GSI_UNUSED(connection);
}

void RecvGameInviteCallback(GPConnection * connection, GPRecvGameInviteArg * arg, Profile * profile)
{
	PrintOp(profile, "RecvGameInviteCallback");

	if(RandomBool())
		gpGetInfo(&profile->gp, arg->profile, GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);

	if(RandomInt(0, 9) == 0)
		MessageOther(profile, arg->profile);

	AddOther(profile, arg->profile);
	GSI_UNUSED(connection);
}
#endif

void EndOp(Profile * profile);

const int RemoteAuthProfiles[] =
{
	58214083,
	58214074,
	58214075,
	58214076,
	58214077,
	58214078,
	58214079,
	58214080,
	58214081,
	58214082
};

void ConnectCallback(GPConnection * connection, GPConnectResponseArg * arg, Profile * profile)
{
	if(arg->result == GP_NO_ERROR)
	{
		profile->connected = true;
		numConnected++;
		totalConnected++;
		highestConnected = max(numConnected, highestConnected);

		if(arg->profile != RemoteAuthProfiles[profile->index % 10])
		{
			char buffer[4096];
			gsi_time ms = (current_time() - profile->connectTime);
			sprintf(buffer, "%d: XXX Connection ProfileID Error (%d != %d) [%d]: %d.%03ds\n", totalConnected, arg->profile, RemoteAuthProfiles[profile->index % 10], (profile->index % 10), ms / 1000, ms % 1000);
			Log(buffer);
		}

#if 1
		{
			char buffer[256];
			gsi_time ms = (current_time() - profile->connectTime);
			sprintf(buffer, "%sConnected: %d time: %d.%03ds\n", (ms>=1000)?"XX ":"", totalConnected, ms / 1000, ms % 1000);
			Log(buffer);
		}
#endif

#if 1
		profile->disconnectTime = 0;
#endif
	}
	else
	{
#if 1
		{
			char buffer[4096];
			gsi_time ms = (current_time() - profile->connectTime);
			sprintf(buffer, "%d: XXX Connection Failed [%d]: %d.%03ds\n", totalConnected, (profile->index % 10), ms / 1000, ms % 1000);
			Log(buffer);
		}
#endif
		profile->disconnectTime = 0;
	}

	EndOp(profile);
	GSI_UNUSED(connection);
}

void GetInfoCallback(GPConnection * connection, GPGetInfoResponseArg * arg, Profile * profile)
{
	if(arg->result != GP_NO_ERROR)
	{
		printf("%04d: gpGetInfo failed\n", profile->index);
		return;
	}
	GSI_UNUSED(connection);
}

void FindPlayersCallback(GPConnection * connection, GPFindPlayersResponseArg * arg, Profile * profile)
{
	int i;

	if(arg->result != GP_NO_ERROR)
	{
		printf("%04d: gpFindPlayers failed\n", profile->index);
		return;
	}

	for(i = 0 ; i < arg->numMatches ; i++)
		gpGetInfo(&profile->gp, arg->matches[i].profile, GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);

	if(arg->numMatches > 0)
	{
		int index = RandomInt(0, arg->numMatches - 1);
		int productID = RandomInt(1, PRODUCT_ID_MAX);

		gpInvitePlayer(&profile->gp, arg->matches[index].profile, productID,NULL);
	}
	GSI_UNUSED(connection);
}

void ProfileSearchCallback(GPConnection * connection, GPProfileSearchResponseArg * arg, Profile * profile)
{
	int i;

	if(arg->result != GP_NO_ERROR)
	{
		printf("%04d: gpProfileSearch failed\n", profile->index);
		return;
	}

	for(i = 0 ; i < arg->numMatches ; i++)
		gpGetInfo(&profile->gp, arg->matches[i].profile, GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);
	GSI_UNUSED(connection);
}

void AddOther(Profile * profile, GPProfile gpProfile)
{
	int i;

	if(profile->numOthers == MAX_OTHERS)
		return;

	for(i = 0 ; i < profile->numOthers ; i++)
		if(profile->others[i] == gpProfile)
			return;

	profile->others[profile->numOthers] = gpProfile;
	profile->numOthers++;
}

unsigned long GetRandomOpTime(void)
{
	return RandomInt(NEXT_OP_TIME_MIN, NEXT_OP_TIME_MAX);
}

Op GetRandomOp(void)
{
	Op op = (Op)(((rand() * (NumOps - 1)) / (RAND_MAX + 1)) + 1);

	assert(op > OpConnect);
	assert(op < NumOps);

	return op;
}

void NewOp(Profile * profile)
{
	if(profile->completedOps == 0)
	{
		profile->activeOp = OpConnect;
	}
	else
	{
#if 1
		profile->activeOp = GetRandomOp();
#else
		profile->activeOp = OpSetStatus1;
#endif
	}
}

void EndOp(Profile * profile)
{
	profile->activeOp = OpNull;

	profile->nextOpTime = (current_time() + GetRandomOpTime());

	profile->completedOps++;
}

void PrintOp(Profile * profile, const char * opString)
{
#if 0
	static char buffer[256];
	sprintf(buffer, "%04d: %s\n", profile->index, opString);
	OutputDebugString(buffer);
#endif
	GSI_UNUSED(profile);
	GSI_UNUSED(opString);
}

void StartOp(Profile * profile)
{
	int i;
	int num;
	int intArray[16];
	char string1[16];
	char string2[16];

	switch(profile->activeOp)
	{
	case OpNull:
		assert(0);
		printf("%04d: Tried to start null op\n", profile->index);
		break;

	case OpConnect:
		PrintOp(profile, "OpConnect");

#if 0
		RandomServer();
#endif
#if 0
		gpConnect(&profile->gp, profile->nick, profile->email, profile->password, (RandomInt(0, 9) == 0)?GP_FIREWALL:GP_NO_FIREWALL, GP_NON_BLOCKING, ConnectCallback, profile);
#else
		{
			const char * authtokens[] =
			{
				"GMT/61LHe4Yu+pHI0eDXu8GMdc51iXjsNvcz1GGoWJ9orpnbZIbp3CVBQddeSZOSq58",
				"GMTcUM+iiQuzaw8crHxBFPCVV6J9V14KnSNHnHbQIXGwN6XpKv97yqSbOqxrgIWZCeZ",
				"GMTxiKFKsnGdOeBaJxmJOQBNSqbm9zOjbu/cTiqpBJp1pqqOAnI1Jh81TPlWmFuvt0h",
				"GMTqUg6QEerwnwi87fZQMcfzQ0xA/zNC96VCtx6GavTv7wzDtGpOWxJHUkm20qMxVsg",
				"GMTV2uHalvcDgtvBsz62QF1ifbCJJ93QONO9TNGVGlDghs6ocL2qOiQVqNJS7LvTZ39",
				"GMTq9Z7Tg/tkEMzqEODuQdNSQPgt5MdAwH26+lQGHliL9n3nH0b0D38fTFWRbkmk4K9",
				"GMTwHyinDuomW0XmQswNyVpgdlQvnuGCR1it+kHCnUDmR5CG3svSufrDcjuHdlwfVON",
				"GMTBZgPikENuiEON0by04RMR112ShZsDctzVbqljZ3ZufDvyugygztRlKIM9gyLwSdy",
				"GMTmHNpmNufV9WuGIGET8E0Xd//56eYWO4mDaD+4KebWQWvc8CqWbqSeII6x9BxQMxQ",
				"GMTyI447fNIYVh1vdI6aErka4TzJcYWctTU/Dzi2GK+4J3Vf/VAqQOs43R73I7dyX+u"
			};
			const char * partnerchallenges[] =
			{
				"WI[A)IuA",
				"RNd$)q)0",
				"LQTi;4#X",
				"AOA)(57=",
				"ob=0XiWJ",
				"mRnTENJ@",
				"9%@A+NKA",
				"L=0O)Lg2",
				"YR6[Zz%B",
				"$kWZ%]~@"
			};
			int tokenIndex = (profile->index % 10);
			gpConnectPreAuthenticated(&profile->gp, authtokens[tokenIndex], partnerchallenges[tokenIndex], GP_FIREWALL, GP_NON_BLOCKING, ConnectCallback, profile);
		}
#endif
		break;

	case OpNewProfile:
		PrintOp(profile, "OpNewProfile");

		profile->activeOp = OpNull;
		break;

	case OpGetInfo:
		PrintOp(profile, "OpGetInfo");

		if(profile->numOthers > 0)
			gpGetInfo(&profile->gp, RandomOther(profile), GP_DONT_CHECK_CACHE, GP_NON_BLOCKING, GetInfoCallback, profile);
		else
			profile->activeOp = OpNull;
		break;

#ifdef TEST_SEARCH
	case OpProfileSearch:
		PrintOp(profile, "OpProfileSearch");

		num = RandomInt(0, 3);
		if(num == 0)
			gpProfileSearch(&profile->gp, "pants", "dan@gamespy.com", NULL, NULL, 0, GP_NON_BLOCKING, ProfileSearchCallback, profile);
		else if(num == 1)
			gpProfileSearch(&profile->gp, "crt", NULL, NULL, NULL, 0, GP_NON_BLOCKING, ProfileSearchCallback, profile);
		else
			gpProfileSearch(&profile->gp, "STRESS-TEST", NULL, NULL, NULL, 0, GP_NON_BLOCKING, ProfileSearchCallback, profile);
		break;

	case OpFindPlayers:
		PrintOp(profile, "OpFindPlayers");

		gpFindPlayers(&profile->gp, RandomInt(1, PRODUCT_ID_MAX), GP_NON_BLOCKING, FindPlayersCallback, profile);
		break;
#endif

	case OpSetInfo:
		PrintOp(profile, "OpSetInfo");

		gpSetInfos(&profile->gp, GP_FIRSTNAME, "STRESS-TEST");
		gpSetInfos(&profile->gp, GP_LASTNAME, "STRESS-TEST");
		gpSetInfoi(&profile->gp, GP_ICQUIN, RandomInt(100000, 40000000));
		gpSetInfos(&profile->gp, GP_HOMEPAGE, "STRESS-TEST");
		gpSetInfod(&profile->gp, GP_BIRTHDAY, RandomInt(1, 28), RandomInt(1, 12), RandomInt(1940, 1990));
		break;

	case OpBuddyRequest:
		PrintOp(profile, "OpBuddyRequest");

#if 1
		profile->activeOp = OpNull;
#else
		if(profile->numOthers > 0)
			gpSendBuddyRequest(&profile->gp, RandomOther(profile), "STRESS-TEST");
#endif
		break;

	case OpDeleteBuddy:
		PrintOp(profile, "OpDeleteBuddy");

		profile->activeOp = OpNull;
		break;

	case OpSetStatus1:
	case OpSetStatus2:
	case OpSetStatus3:
	case OpSetStatus4:
	case OpSetStatus5:
		PrintOp(profile, "OpSetStatus");

		gpSetStatus(&profile->gp, GP_ONLINE, RandomString(string1), RandomString(string2));
		break;

	case OpSendBuddyMessage:
		PrintOp(profile, "OpSendBuddyMessage");

		if(profile->numOthers > 0)
			MessageOther(profile, RandomOther(profile));
		break;

	case OpSetInvitable:
		PrintOp(profile, "OpSetInvitable");

		num = RandomInt(0, 10);

		for(i = 0 ; i < num ; i++)
			intArray[i] = RandomInt(1, PRODUCT_ID_MAX);

		gpSetInvitableGames(&profile->gp, num, intArray);
		break;

	default:
		assert(0);
		printf("%04d: Tried to start unknown op: %d\n", profile->index, profile->activeOp);
		break;
	}

	if(IS_INSTANT_OP(profile->activeOp))
	{
		EndOp(profile);
	}
}

void ProcessProfile(Profile * profile)
{
	DWORD now = current_time();

	if(profile->gpInitialized)
	{
		gpProcess(&profile->gp);

		if((profile->activeOp == OpConnect) && ((current_time() - profile->connectTime) > (200 * 60 * 1000)))
		{
			char buffer[256];
			gsi_time ms = (current_time() - profile->connectTime);
			sprintf(buffer, "%04d: XXX Excessive Connect Time: %d.%03ds\n", profile->index, ms / 1000, ms % 1000);
			Log(buffer);
		}

		if((now > profile->disconnectTime) && ((profile->activeOp != OpConnect) || shuttingDown))
		{
			if(profile->connected)
			{
				numConnected--;
				gpDisconnect(&profile->gp);
			}
			gpDestroy(&profile->gp);

			profile->gpInitialized = false;
			numConnections--;
		}
		else if(!shuttingDown && (profile->activeOp == OpNull))
		{
#if 1
			if(now > profile->nextOpTime)
#else
			if((now > profile->nextOpTime) && (profile->completedOps == 0))
#endif
			{
				NewOp(profile);

				StartOp(profile);
			}
		}
	}
}

Profile * GetUninitializedProfile(void)
{
	int index;

	do
	{
		index = ((rand() * numProfiles) / (RAND_MAX + 1));
		assert(index >= 0);
		assert(index < numProfiles);
	}
	while(profiles[index].gpInitialized || profiles[index].invalid);

	return &profiles[index];
}

void InitializeProfile(Profile * profile)
{
	GPResult result;

	result = gpInitialize(&profile->gp, 0, 0, GP_PARTNERID_GAMESPY);
	if(result != GP_NO_ERROR)
	{
		printf("%04d: Failed to initialize GP for %s@%s\n", profile->index, profile->nick, profile->email);
		return;
	}

	gpSetCallback(&profile->gp, GP_ERROR, ErrorCallback, profile);
	gpSetCallback(&profile->gp, GP_RECV_BUDDY_STATUS, RecvBuddyStatusCallback, profile);
#if 1
	gpSetCallback(&profile->gp, GP_RECV_BUDDY_REQUEST, RecvBuddyRequestCallback, profile);
	gpSetCallback(&profile->gp, GP_RECV_BUDDY_MESSAGE, RecvBuddyMessageCallback, profile);
	gpSetCallback(&profile->gp, GP_RECV_GAME_INVITE, RecvGameInviteCallback, profile);
#endif

	profile->gpInitialized = true;

	profile->connected = false;

	profile->connectTime = current_time();

	profile->invalid = false;

	profile->disconnectTime = current_time();
	profile->disconnectTime += (((rand() * (CONNECT_TIME_MAX - CONNECT_TIME_MIN)) / RAND_MAX) + CONNECT_TIME_MIN);

	profile->numOthers = 0;

	profile->activeOp = OpNull;
	profile->nextOpTime = 0;
	profile->completedOps = 0;

	numConnections++;
	totalConnections++;
	highestConnections = max(highestConnections, numConnections);
}

void GetUserNicksCallback(GPConnection * connection, GPGetUserNicksResponseArg * arg, ValidationData * validationData)
{
	int i;

	validationData->result = arg->result;

	if(arg->result != GP_NO_ERROR)
	{
		printf("%04d: gpGetUserNicks failed\n", validationData->profile->index);
		return;
	}

	validationData->numNicks = min(arg->numNicks, VALIDATION_MAX_NICKS);
	for(i = 0 ; i < arg->numNicks ; i++)
		strcpy(validationData->nicks[i], arg->nicks[i]);
	GSI_UNUSED(connection);
}

void NewUserResponse(GPConnection * connection, GPNewUserResponseArg * arg, void * param)
{
	GSI_UNUSED(connection);
	GSI_UNUSED(arg);
	GSI_UNUSED(param);
}

bool Validate(void)
{
	int i;
	GPConnection gp;
	char email[GP_EMAIL_LEN] = "";
	Profile * profile;
	ValidationData validationData;

	validationData.profile = NULL;
	validationData.result = GP_NO_ERROR;
	validationData.numNicks = 0;
	startShutdownTime = ULONG_MAX;

	if(gpInitialize(&gp, 0, 0, GP_PARTNERID_GAMESPY) != GP_NO_ERROR)
	{
		printf("Failed to initialize GP for validation\n");
		return false;
	}
	for(i = 0 ; i < numProfiles ; i++)
	{
		if(current_time() > startShutdownTime)
			return false;

		profile = &profiles[i];

		if(strcmp(email, profile->email) != 0)
		{
			strcpy(email, profile->email);

			validationData.profile = profile;

			printf("%04d: Getting nicks for: %s\n", profile->index, email);

			gpGetUserNicks(&gp, email, profile->password, GP_BLOCKING, GetUserNicksCallback, &validationData);
		}

		if(validationData.result == GP_NO_ERROR)
		{
			int n;
			bool match = false;

			for(n = 0 ; (n < validationData.numNicks) && !match ; n++)
			{
				if(strcasecmp(profile->nick, validationData.nicks[n]) == 0)
					match = true;
			}

			profile->invalid = !match;
		}
		else
		{
			profile->invalid = true;
		}

		if(profile->invalid)
		{
			static char buffer[256];
			sprintf(buffer, "%s (%d) : %04d: Invalid profile (%s@%s)\n", profilesFile, (profile->index + 1), profile->index, profile->nick, profile->email);
			printf(buffer);
			OutputDebugString(buffer);
		}
	}

	return true;
}

#ifdef WIN32
BOOL WINAPI CtrlHandlerRoutine(DWORD dwCtrlType)
{
	startShutdownTime = 0;

	GSI_UNUSED(dwCtrlType);
	return TRUE;
}
#endif

bool Initialize(void)
{
	int i;
	Profile * profile;
	DWORD now;
	GSIACResult aResult = GSIACWaiting;

	srand(time(NULL));

	GSIStartAvailableCheck("gmtest");
	while(aResult == GSIACWaiting)
		aResult = GSIAvailableCheckThink();
	if (aResult != GSIACAvailable)
	{
		printf("Backend services are not available.\r\n");
		return false;
	}


#ifdef WIN32
	SetConsoleCtrlHandler(CtrlHandlerRoutine, TRUE);
#endif

#ifdef CONNECTION_MANAGER
	if(CONNECTION_MANAGER[0])
		strcpy(GPConnectionManagerHostname, CONNECTION_MANAGER);
#endif

	if(!LoadProfiles())
		return false;

	if(maxConnections > numProfiles)
		maxConnections = numProfiles;

	now = current_time();

	for(i = 0 ; i < numProfiles ; i++)
	{
		profile = &profiles[i];

		profile->index = i;

		profile->activeOp = OpNull;

#if 0
		if(((i + 1) % 100) == 0)
		{
			printf("%d/%d profiles initialized\n", i + 1, numProfiles);
			msleep(1);
		}
#endif
	}

	return true;
}

void Shutdown(void)
{
	int i;

	for(i = 0 ; i < numProfiles ; i++)
	{
		if(profiles[i].gpInitialized)
		{
			gpDestroy(&profiles[i].gp);

			if(((i + 1) % 100) == 0)
			{
				printf("%d/%d profiles destroyed\n", i + 1, numProfiles);
				msleep(1);
			}
		}
	}
}

void StartShutdown(unsigned long now)
{
	Profile * profile;
	int i;

	for(i = 0 ; i < numProfiles ; i++)
	{
		profile = &profiles[i];

		if(profile->gpInitialized)
		{
#if 0
			if(!profile->connected)
			{
				char buffer[64];

				sprintf(buffer, "%04d: Waiting %ds for connection attempt\n", profile->index, (current_time() - profile->connectTime) / 1000);
				Log(buffer);
			}
#endif

			profile->disconnectTime = (now + ((rand() * ((numConnections / SHUTDOWN_DISCONNECTS_PER_SEC) * 1000)) / RAND_MAX));
		}
	}

	shuttingDown = true;
}

void Frame(unsigned long now)
{
	int i;

	if(!shuttingDown && (numConnections < maxConnections) && ((numConnections - numConnected) < MAX_OUTSTANDING_CONNECTIONS))
	{
		Profile * profile;
		unsigned long timeDiff;
		int numConnects;

		timeDiff = (now - lastConnectTime);

		numConnects = ((timeDiff * connectsPerSecond) / 1000);
		numConnects = min(numConnects, (MAX_OUTSTANDING_CONNECTIONS - (numConnections - numConnected)));

		for(i = 0 ; i < numConnects ; i++)
		{
			if(numConnections < maxConnections)
			{
				profile = GetUninitializedProfile();

				InitializeProfile(profile);

				lastConnectTime = now;
			}
		}
	}

	for(i = 0 ; i < numProfiles ; i++)
	{
		ProcessProfile(&profiles[i]);
	}
}

void Run(void)
{
	bool done;
	unsigned long now;
#if 0
	unsigned long nextInfoTime = 0;
#endif
	now = current_time();

	startShutdownTime = (now + runTime);

	lastConnectTime = (now - 100);

	for(done = false ; !done ; )
	{
		now = current_time();

		if(shuttingDown)
		{
			if(numConnections == 0)
			{
				done = true;
			}
		}
		else if(now > startShutdownTime)
		{
			StartShutdown(now);
		}

#if 0
		if(now > nextInfoTime)
		{
			static char buffer[256];

			nextInfoTime = (now + SHOW_INFO_TIME);

			sprintf(buffer, "INFO: Connections: %d current, %d highest, %d total\n", numConnections, highestConnections, totalConnections);
			Log(buffer);

			sprintf(buffer, "INFO: Connected:   %d current, %d highest, %d total\n", numConnected, highestConnected, totalConnected);
			Log(buffer);

			if(shuttingDown)
				sprintf(buffer, "INFO: Shutting down\n");
			else
			{
				unsigned long totalSeconds = ((startShutdownTime - now) / 1000);
				unsigned long hours, minutes, seconds;
				seconds = (totalSeconds % 60);
				minutes = (totalSeconds / 60 % 60);
				hours = (totalSeconds / 60 / 60);
				sprintf(buffer, "INFO: Shut down in %d:%02d:%02d\n", hours, minutes, seconds);
			}
			Log(buffer);
		}
#endif

		Frame(now);

		msleep(10);
	}
}

bool ParseArgs(int argc, char ** argv)
{
	int i;
	maxConnections = DEFAULT_MAX_CONNECTED;
	runTime = DEFAULT_RUN_TIME;
	connectsPerSecond = DEFAULT_CONNECTS_PER_SEC;

	for(i = 1 ; i < argc ; i++)
	{
		if(strcasecmp(argv[i], "-m") == 0)
		{
			if(++i < argc)
			{
				maxConnections = atoi(argv[i]);
				if(maxConnections <= 0)
					maxConnections = DEFAULT_MAX_CONNECTED;
			}
		}
		else if(strcasecmp(argv[i], "-t") == 0)
		{
			if(++i < argc)
			{
				runTime = (atoi(argv[i]) * 1000);
				if(runTime <= 0)
					runTime = DEFAULT_RUN_TIME;
			}
		}
		else if(strcasecmp(argv[i], "-s") == 0)
		{
			if(++i < argc)
			{
				connectsPerSecond = atoi(argv[i]);
				if(connectsPerSecond <= 0)
					connectsPerSecond = DEFAULT_CONNECTS_PER_SEC;
			}
		}
	}

	return true;
}

int main(int argc, char ** argv)
{
	if(!ParseArgs(argc, argv))
		return 1;

	if(!Initialize())
		return 1;

#if 0
	if(!Validate())
		return 1;
#endif

	Run();

	Shutdown();

	return 0;
}