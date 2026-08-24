/***********************
qrcsample.c
GameSpy Query & Reporting SDK 
  
Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com

******

 See the ReadMe file for qrcsample info, and consult the GameSpy Query & Reporting 2
 SDK documentation for more information on implementing the qr2 SDK.

************************/

#include "../qr2.h"

#define GAME_VERSION	_T("2.00")
#define GAME_NAME		_T("gmtest")
#define MAX_PLAYERS		32
#define MAX_TEAMS		2
#define BASE_PORT		11111

#ifdef _WIN32_WCE
	void RetailOutputA(CHAR *tszErr, ...);
	#define printf RetailOutputA
#elif defined(_NITRO)
	#include "../../common/nitro/screen.h"
	#define printf Printf
	#define vprintf VPrintf
#endif

#define GRAVITY_KEY 100
#define RANKINGON_KEY 101
#define TIME__KEY 102
#define AVGPING_T_KEY 103

typedef struct
{
	gsi_char pname[80];
	int pfrags;
	int pdeaths;
	int ptime;
	int pping;
	int pteam;
} player_t;

typedef struct
{
	gsi_char tname[80];
	int tscore;
	int avgping;

} team_t;

typedef struct
{
	player_t players[MAX_PLAYERS];
	team_t teams[MAX_TEAMS];
	gsi_char mapname[20];
	gsi_char hostname[120];
	gsi_char gamemode[200];
	gsi_char gametype[30];
	int numteams;
	int numplayers;
	int maxplayers;
	int fraglimit;
	int timelimit;
	int teamplay;
	int rankingson;
	int gravity;
	int hostport;
} gamedata_t;

gsi_char *constnames[MAX_PLAYERS]=
{
	_T("Joe Player"), _T("L33t 0n3"), _T("Raptor"), _T("Gr81"),
	_T("Flubber"),    _T("Sarge"),    _T("Void"),   _T("runaway"),
	_T("Ph3ar"),      _T("wh00t"),    _T("gr1nder"),_T("Mace"),
	_T("stacy"),      _T("lamby"),    _T("Thrush"), _T("Leeroy")
};
gamedata_t gamedata;  

#ifdef GSI_COMMON_DEBUG
	static void DebugCallback(GSIDebugCategory theCat, GSIDebugType theType,
	                          GSIDebugLevel theLevel, const char * theTokenStr,
	                          va_list theParamList)
	{
		GSI_UNUSED(theLevel);
		printf("[%s][%s] ", 
				gGSIDebugCatStrings[theCat], 
				gGSIDebugTypeStrings[theType]);

		vprintf(theTokenStr, theParamList);
	}

	#ifdef GSI_UNICODE
	static void AppDebug(const unsigned short* format, ...)
	{
        unsigned short buf[1024];
        char tmp[2056];
        va_list aList;
        va_start(aList, format);
        vswprintf(buf, 1024, format, aList);

        UCS2ToAsciiString(buf, tmp);
        gsDebugFormat(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice,
            "%s", tmp);
	}
	#else
	static void AppDebug(const char* format, ...)
	{
		va_list aList;
		va_start(aList, format);
		gsDebugVaList(GSIDebugCat_App, GSIDebugType_Misc, GSIDebugLevel_Notice,
			format, aList);
	}
	#endif
#else
	#define AppDebug _tprintf
#endif

void serverkey_callback(int keyid, qr2_buffer_t outbuf, void *userdata);
void playerkey_callback(int keyid, int index, qr2_buffer_t outbuf, void *userdata);
void teamkey_callback(int keyid, int index, qr2_buffer_t outbuf, void *userdata);
void keylist_callback(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void *userdata);
int  count_callback(qr2_key_type keytype, void *userdata);
void adderror_callback(qr2_error_t error, gsi_char *errmsg, void *userdata);
void nn_callback(int cookie, void *userdata);
void cm_callback(gsi_char *data, int len, void *userdata);
void cc_callback(SOCKET gamesocket, struct sockaddr_in *remoteaddr, void *userdata);
void DoGameStuff(gsi_time totalTime);
int  test_main(int argc, char **argp);

void serverkey_callback(int keyid, qr2_buffer_t outbuf, void *userdata)
{
	AppDebug("Reporting server keys\n");

	switch (keyid)
	{
	case HOSTNAME_KEY:
		qr2_buffer_add(outbuf, gamedata.hostname);
		break;
	case GAMEVER_KEY:
		qr2_buffer_add(outbuf, GAME_VERSION);
		break;
	case HOSTPORT_KEY:
		qr2_buffer_add_int(outbuf, gamedata.hostport);
		break;
	case MAPNAME_KEY:
		qr2_buffer_add(outbuf, gamedata.mapname);
		break;
	case GAMETYPE_KEY:
		qr2_buffer_add(outbuf, gamedata.gametype);
		break;
	case NUMPLAYERS_KEY:
		qr2_buffer_add_int(outbuf, gamedata.numplayers);
		break;
	case NUMTEAMS_KEY:
		qr2_buffer_add_int(outbuf, gamedata.numteams);
		break;
	case MAXPLAYERS_KEY:
		qr2_buffer_add_int(outbuf, gamedata.maxplayers);
		break;
	case GAMEMODE_KEY:
		qr2_buffer_add(outbuf, gamedata.gamemode);
		break;
	case TEAMPLAY_KEY:
		qr2_buffer_add_int(outbuf, gamedata.teamplay);
		break;
	case FRAGLIMIT_KEY:
		qr2_buffer_add_int(outbuf, gamedata.fraglimit);
		break;
	case TIMELIMIT_KEY:
		qr2_buffer_add_int(outbuf, gamedata.timelimit);
		break;
	case GRAVITY_KEY:
		qr2_buffer_add_int(outbuf, gamedata.gravity);
		break;
	case RANKINGON_KEY:
		qr2_buffer_add_int(outbuf, gamedata.rankingson);
		break;
	default:
		qr2_buffer_add(outbuf, _T(""));
	}
	
	GSI_UNUSED(userdata);
}

void playerkey_callback(int keyid, int index, qr2_buffer_t outbuf, void *userdata)
{
	AppDebug("Reporting player keys\n");
	
	if (index >= gamedata.numplayers)
	{
		qr2_buffer_add(outbuf, _T(""));
		return;
	}
	switch (keyid)
	{
	case PLAYER__KEY:
		qr2_buffer_add(outbuf, gamedata.players[index].pname);
		break;
	case SCORE__KEY:
		qr2_buffer_add_int(outbuf, gamedata.players[index].pfrags);
		break;
	case DEATHS__KEY:
		qr2_buffer_add_int(outbuf, gamedata.players[index].pdeaths);
		break;
	case PING__KEY:
		qr2_buffer_add_int(outbuf, gamedata.players[index].pping);
		break;
	case TEAM__KEY:
		qr2_buffer_add_int(outbuf, gamedata.players[index].pteam);
		break;
	case TIME__KEY:
		qr2_buffer_add_int(outbuf, gamedata.players[index].ptime);
		break;
	default:
		qr2_buffer_add(outbuf, _T(""));
		break;		
	}
	
	GSI_UNUSED(userdata);
}

void teamkey_callback(int keyid, int index, qr2_buffer_t outbuf, void *userdata)
{
	AppDebug("Reporting team keys\n");

	if (index >= gamedata.numteams)
	{
		qr2_buffer_add(outbuf, _T(""));
		return;
	}
	switch (keyid)
	{
	case TEAM_T_KEY:
		qr2_buffer_add(outbuf, gamedata.teams[index].tname);
		break;
	case SCORE_T_KEY:
		qr2_buffer_add_int(outbuf, gamedata.teams[index].tscore);
		break;
	case AVGPING_T_KEY:
		qr2_buffer_add_int(outbuf, gamedata.teams[index].avgping);
		break;
	default:
		qr2_buffer_add(outbuf, _T(""));
		break;		
	}
	
	GSI_UNUSED(userdata);
}	

void keylist_callback(qr2_key_type keytype, qr2_keybuffer_t keybuffer, void *userdata)
{
	AppDebug("Reporting keylist\n");

	switch (keytype)
	{
	case key_server:
		qr2_keybuffer_add(keybuffer, HOSTNAME_KEY);
		qr2_keybuffer_add(keybuffer, GAMEVER_KEY);
		qr2_keybuffer_add(keybuffer, HOSTPORT_KEY);
		qr2_keybuffer_add(keybuffer, MAPNAME_KEY);
		qr2_keybuffer_add(keybuffer, GAMETYPE_KEY);
		qr2_keybuffer_add(keybuffer, NUMPLAYERS_KEY);
		qr2_keybuffer_add(keybuffer, NUMTEAMS_KEY);
		qr2_keybuffer_add(keybuffer, MAXPLAYERS_KEY);
		qr2_keybuffer_add(keybuffer, GAMEMODE_KEY);
		qr2_keybuffer_add(keybuffer, TEAMPLAY_KEY);
		qr2_keybuffer_add(keybuffer, FRAGLIMIT_KEY);
		qr2_keybuffer_add(keybuffer, TIMELIMIT_KEY);
		qr2_keybuffer_add(keybuffer, GRAVITY_KEY); 
		qr2_keybuffer_add(keybuffer, RANKINGON_KEY); 
		break;
	case key_player:
		qr2_keybuffer_add(keybuffer, PLAYER__KEY);
		qr2_keybuffer_add(keybuffer, SCORE__KEY);
		qr2_keybuffer_add(keybuffer, DEATHS__KEY);
		qr2_keybuffer_add(keybuffer, PING__KEY);
		qr2_keybuffer_add(keybuffer, TEAM__KEY);
		qr2_keybuffer_add(keybuffer, TIME__KEY); 
		break;
	case key_team:
		qr2_keybuffer_add(keybuffer, TEAM_T_KEY);
		qr2_keybuffer_add(keybuffer, SCORE_T_KEY);
		qr2_keybuffer_add(keybuffer, AVGPING_T_KEY); 
		break;
	default: break;
	}
	
	GSI_UNUSED(userdata);
}

int count_callback(qr2_key_type keytype, void *userdata)
{
	AppDebug("Reporting number of players/teams\n");

	if (keytype == key_player)
		return gamedata.numplayers;
	else if (keytype == key_team)
		return gamedata.numteams;
	else
		return 0;

	GSI_UNUSED(userdata);
}

void adderror_callback(qr2_error_t error, gsi_char *errmsg, void *userdata)
{
	GS_ASSERT(errmsg)
	AppDebug("Error adding server: %d, %s\n", error, errmsg);
	GSI_UNUSED(userdata);
}

void nn_callback(int cookie, void *userdata)
{
	AppDebug("Got natneg cookie: %d\n", cookie);
	GSI_UNUSED(userdata);
}

void cm_callback(gsi_char *data, int len, void *userdata)
{
	AppDebug("Got %d bytes from client\n", len);
	GSI_UNUSED(data);
	GSI_UNUSED(userdata);
}

void cc_callback(SOCKET gamesocket, struct sockaddr_in *remoteaddr, void *userdata)
{
	AppDebug("Client connected from %s:%d\n", inet_ntoa(remoteaddr->sin_addr), ntohs(remoteaddr->sin_port));
	GSI_UNUSED(gamesocket);
	GSI_UNUSED(userdata);
}

static void init_game(void)
{
	int i;
	AppDebug("Generating game data\n");
	srand((unsigned int) current_time() );
	gamedata.numplayers = rand() % 15 + 1;
	gamedata.maxplayers = MAX_PLAYERS;
	for (i = 0 ; i < gamedata.numplayers ; i++)
	{
		_tcscpy(gamedata.players[i].pname, constnames[i]);
		gamedata.players[i].pfrags = rand() % 32;
		gamedata.players[i].pdeaths = rand() % 32;
		gamedata.players[i].ptime = rand() % 1000;
		gamedata.players[i].pping = rand() % 500;
		gamedata.players[i].pteam = rand() % 2;
	}
	gamedata.numteams = 2;
	for (i = 0 ; i < gamedata.numteams ; i++)
	{
		gamedata.teams[i].tscore = rand() % 500;
		gamedata.teams[i].avgping = rand() % 500;
	}

	_tcscpy(gamedata.teams[0].tname,_T("Red"));
	_tcscpy(gamedata.teams[1].tname,_T("Blue"));
	_tcscpy(gamedata.mapname,_T("gmtmap1"));
	_tcscpy(gamedata.gametype,_T("arena"));
	_tcscpy(gamedata.hostname,_T("GameSpy QR2 Sample"));
	_tcscpy(gamedata.gamemode,_T("openplaying"));

	gamedata.fraglimit = 0;
	gamedata.timelimit = 40;
	gamedata.teamplay = 1;
	gamedata.rankingson = 1;
	gamedata.gravity = 800;
	gamedata.hostport = 25000;
}

void DoGameStuff(gsi_time totalTime)
{
	static int stateChanged = 0; 
	if (!stateChanged && totalTime > 30000) {
		AppDebug("Mapname changed, calling qr2_send_statechanged\n");
		_tcscpy(gamedata.mapname,_T("gmtmap2"));
		qr2_send_statechanged(NULL);
		stateChanged = 1;
	}

	msleep(10);
}


int test_main(int argc, char **argp)
{			
	gsi_char  secret_key[9];         
	gsi_char  ip[255];               
	const int isPublic = 1;          
	const int isNatNegSupported = 1; 
	gsi_time  aStartTime = 0;        
	void * userData = NULL;          

#if defined (_PS3) || defined (_PS2) || defined (_PSP) || defined(_NITRO)
	#ifdef GSI_COMMON_DEBUG
		gsSetDebugCallback(DebugCallback);

		gsSetDebugLevel(GSIDebugCat_All, GSIDebugType_All, GSIDebugLevel_Verbose);
	#endif
#endif

	secret_key[0] = 'H';
	secret_key[1] = 'A';
	secret_key[2] = '6';
	secret_key[3] = 'z';
	secret_key[4] = 'k';
	secret_key[5] = 'S';
	secret_key[6] = '\0';

	AppDebug("Registering custom keys\n");
	qr2_register_key(GRAVITY_KEY, _T("gravity")    );
	qr2_register_key(RANKINGON_KEY, _T("rankingon"));
	qr2_register_key(TIME__KEY,     _T("time_")    ); 
	qr2_register_key(AVGPING_T_KEY, _T("avgping_t")); 

	init_game();

#ifndef GSI_UNICODE
	if (argc>1)
		strcpy(ip, argp[1]);
#else
	if (argc>1)
		AsciiToUCS2String(argp[1], ip);
#endif

	AppDebug("Initializing SDK; server should show up on the master list within 6-10 sec.\n");
	if (qr2_init(NULL,argc>1?ip:NULL,BASE_PORT,GAME_NAME, secret_key, isPublic, isNatNegSupported,
		serverkey_callback, playerkey_callback, teamkey_callback,
		keylist_callback, count_callback, adderror_callback, userData) != e_qrnoerror)
	{
		printf("Error starting query sockets\n");
		return -1;
	}

	qr2_register_clientmessage_callback(NULL, cm_callback);

	qr2_register_natneg_callback(NULL, nn_callback);

	qr2_register_clientconnected_callback(NULL, cc_callback);

	AppDebug("Sample will quit after 60 seconds\n");
	aStartTime = current_time();
	while ((current_time() - aStartTime) < 60000)
	{
		gsi_time totalTime = current_time() - aStartTime; 

		DoGameStuff(totalTime);

		qr2_think(NULL);
	}

	AppDebug("Shutting down - server will be removed from the master server list\n");
	qr2_shutdown(NULL);

	
#ifdef GSI_UNICODE
	qr2_internal_key_list_free();
#endif

	return 0;
}

