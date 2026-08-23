#ifndef _GP_H_
#define _GP_H_

#include "../common/gsCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _GPEnum
{
	GP_ERROR = 0,
	GP_RECV_BUDDY_REQUEST,
	GP_RECV_BUDDY_STATUS,
	GP_RECV_BUDDY_MESSAGE,
	GP_RECV_BUDDY_UTM,
	GP_RECV_GAME_INVITE,
	GP_TRANSFER_CALLBACK,
	GP_RECV_BUDDY_AUTH,
	GP_RECV_BUDDY_REVOKE,
		
	GP_INFO_CACHING = 0x0100,
	GP_SIMULATION,
	GP_INFO_CACHING_BUDDY_AND_BLOCK_ONLY,

	GP_BLOCKING = 1,
	GP_NON_BLOCKING = 0,

	GP_FIREWALL = 1,
	GP_NO_FIREWALL = 0,

	GP_CHECK_CACHE = 1,
	GP_DONT_CHECK_CACHE = 0,

	GP_VALID = 1,
	GP_INVALID = 0,

	GP_FATAL = 1,
	GP_NON_FATAL = 0,

	GP_MALE = 0x0500,
	GP_FEMALE,
	GP_PAT,

	GP_MORE = 0x0600,
	GP_DONE,

	GP_NICK = 0x0700,
	GP_UNIQUENICK,
	GP_EMAIL,
	GP_PASSWORD,
	GP_FIRSTNAME,
	GP_LASTNAME,
	GP_ICQUIN,
	GP_HOMEPAGE,
	GP_ZIPCODE,
	GP_COUNTRYCODE,
	GP_BIRTHDAY,
	GP_SEX,
	GP_CPUBRANDID,
	GP_CPUSPEED,
	GP_MEMORY,
	GP_VIDEOCARD1STRING,
	GP_VIDEOCARD1RAM,
	GP_VIDEOCARD2STRING,
	GP_VIDEOCARD2RAM,
	GP_CONNECTIONID,
	GP_CONNECTIONSPEED,
	GP_HASNETWORK,
	GP_OSSTRING,
	GP_AIMNAME,  
	GP_PIC,
	GP_OCCUPATIONID,
	GP_INDUSTRYID,
	GP_INCOMEID,
	GP_MARRIEDID,
	GP_CHILDCOUNT,
	GP_INTERESTS1,

	GP_REPLACE = 1,
	GP_DONT_REPLACE = 0,

	GP_CONNECTED = 1,
	GP_NOT_CONNECTED = 0,

	GP_MASK_NONE        = 0x00000000,
	GP_MASK_HOMEPAGE    = 0x00000001,
	GP_MASK_ZIPCODE     = 0x00000002,
	GP_MASK_COUNTRYCODE = 0x00000004,
	GP_MASK_BIRTHDAY    = 0x00000008,
	GP_MASK_SEX         = 0x00000010,
	GP_MASK_EMAIL       = 0x00000020,
	GP_MASK_ALL         = 0xFFFFFFFF,

	GP_OFFLINE  = 0,
	GP_ONLINE   = 1,
	GP_PLAYING  = 2,
	GP_STAGING  = 3,
	GP_CHATTING = 4,
	GP_AWAY     = 5,

	GP_SESS_IS_CLOSED = 0x00000001,
	GP_SESS_IS_OPEN = 0x00000002,
	GP_SESS_HAS_PASSWORD = 0x00000004,
	GP_SESS_IS_BEHIND_NAT = 0x00000008,
	GP_SESS_IS_RANKED = 0x000000010,
	

	GP_INTEL = 1,
	GP_AMD,
	GP_CYRIX,
	GP_MOTOROLA,
	GP_ALPHA,

	GP_MODEM = 1,
	GP_ISDN,
	GP_CABLEMODEM,
	GP_DSL,
	GP_SATELLITE,
	GP_ETHERNET,
	GP_WIRELESS,

	GP_TRANSFER_SEND_REQUEST = 0x800,  
	GP_TRANSFER_ACCEPTED,
	GP_TRANSFER_REJECTED,        
	GP_TRANSFER_NOT_ACCEPTING,   
	GP_TRANSFER_NO_CONNECTION,   
	GP_TRANSFER_DONE,            
	GP_TRANSFER_CANCELLED,       
	GP_TRANSFER_LOST_CONNECTION, 
	GP_TRANSFER_ERROR,           
	GP_TRANSFER_THROTTLE,  
	GP_FILE_BEGIN,
	GP_FILE_PROGRESS,  
	GP_FILE_END,
	GP_FILE_DIRECTORY,
	GP_FILE_SKIP,
	GP_FILE_FAILED,  

	GP_FILE_READ_ERROR = 0x900,
	GP_FILE_WRITE_ERROR,
	GP_FILE_DATA_ERROR,

	GP_TRANSFER_SENDER = 0xA00,
	GP_TRANSFER_RECEIVER,

	GP_DONT_ROUTE = 0xB00, 

	GP_SILENCE_NONE       = 0x00000000,
	GP_SILENCE_MESSAGES   = 0x00000001,
	GP_SILENCE_UTMS       = 0x00000002,
	GP_SILENCE_LIST       = 0x00000004, 
	GP_SILENCE_ALL        = 0xFFFFFFFF,

	GP_NEW_STATUS_INFO_SUPPORTED = 0xC00,
	GP_NEW_STATUS_INFO_NOT_SUPPORTED = 0xC01
} GPEnum;

typedef enum _GPResult
{
	GP_NO_ERROR,			
	GP_MEMORY_ERROR,
	GP_PARAMETER_ERROR,
	GP_NETWORK_ERROR,
	GP_SERVER_ERROR,
	GP_MISC_ERROR,
	GP_COUNT
} GPResult;

typedef enum _GPErrorCode
{
	GP_GENERAL = 0x0000,
	GP_PARSE,
	GP_NOT_LOGGED_IN,
	GP_BAD_SESSKEY,
	GP_DATABASE,
	GP_NETWORK,
	GP_FORCED_DISCONNECT,
	GP_CONNECTION_CLOSED,
	GP_UDP_LAYER,

	GP_LOGIN = 0x0100,
	GP_LOGIN_TIMEOUT,

	GP_LOGIN_BAD_NICK,
	GP_LOGIN_BAD_EMAIL,
	GP_LOGIN_BAD_PASSWORD,
	GP_LOGIN_BAD_PROFILE,
	GP_LOGIN_PROFILE_DELETED,
	GP_LOGIN_CONNECTION_FAILED,
	GP_LOGIN_SERVER_AUTH_FAILED,
	GP_LOGIN_BAD_UNIQUENICK,
	GP_LOGIN_BAD_PREAUTH,

	GP_NEWUSER = 0x0200,
	GP_NEWUSER_BAD_NICK,
	GP_NEWUSER_BAD_PASSWORD,
	GP_NEWUSER_UNIQUENICK_INVALID,
	GP_NEWUSER_UNIQUENICK_INUSE,

	GP_UPDATEUI = 0x0300,
	GP_UPDATEUI_BAD_EMAIL,

	GP_NEWPROFILE = 0x0400,
	GP_NEWPROFILE_BAD_NICK,
	GP_NEWPROFILE_BAD_OLD_NICK,

	GP_UPDATEPRO = 0x0500,
	GP_UPDATEPRO_BAD_NICK,

	GP_ADDBUDDY = 0x0600,
	GP_ADDBUDDY_BAD_FROM,
	GP_ADDBUDDY_BAD_NEW,
	GP_ADDBUDDY_ALREADY_BUDDY,

	GP_AUTHADD = 0x0700,
	GP_AUTHADD_BAD_FROM,
	GP_AUTHADD_BAD_SIG,

	GP_STATUS = 0x0800,

	GP_BM = 0x0900,
	GP_BM_NOT_BUDDY,
	GP_BM_EXT_INFO_NOT_SUPPORTED,
	GP_BM_BUDDY_OFFLINE,

	GP_GETPROFILE = 0x0A00,
	GP_GETPROFILE_BAD_PROFILE,

	GP_DELBUDDY = 0x0B00,
	GP_DELBUDDY_NOT_BUDDY,

	GP_DELPROFILE = 0x0C00,
	GP_DELPROFILE_LAST_PROFILE,

	GP_SEARCH = 0x0D00,
	GP_SEARCH_CONNECTION_FAILED,
	GP_SEARCH_TIMED_OUT,

	GP_CHECK = 0x0E00,
	GP_CHECK_BAD_EMAIL,
	GP_CHECK_BAD_NICK,
	GP_CHECK_BAD_PASSWORD,

	GP_REVOKE = 0x0F00,
	GP_REVOKE_NOT_BUDDY,

	GP_REGISTERUNIQUENICK = 0x1000,
	GP_REGISTERUNIQUENICK_TAKEN,
	GP_REGISTERUNIQUENICK_RESERVED,
	GP_REGISTERUNIQUENICK_BAD_NAMESPACE,

	GP_REGISTERCDKEY = 0x1100,
	GP_REGISTERCDKEY_BAD_KEY,
	GP_REGISTERCDKEY_ALREADY_SET,
	GP_REGISTERCDKEY_ALREADY_TAKEN,

	GP_ADDBLOCK = 0x1200,
	GP_ADDBLOCK_ALREADY_BLOCKED,

	GP_REMOVEBLOCK = 0x1300,
	GP_REMOVEBLOCK_NOT_BLOCKED

} GPErrorCode;

#define GP_NICK_LEN                 31
#define GP_UNIQUENICK_LEN           21
#define GP_FIRSTNAME_LEN            31
#define GP_LASTNAME_LEN             31
#define GP_EMAIL_LEN                51
#define GP_PASSWORD_LEN             31
#define GP_PASSWORDENC_LEN          ((((GP_PASSWORD_LEN+2)*4)/3)+1)
#define GP_HOMEPAGE_LEN             76
#define GP_ZIPCODE_LEN              11
#define GP_COUNTRYCODE_LEN          3
#define GP_PLACE_LEN                128
#define GP_AIMNAME_LEN              51
#define GP_REASON_LEN               1025
#define GP_STATUS_STRING_LEN        256
#define GP_LOCATION_STRING_LEN      256
#define GP_ERROR_STRING_LEN         256
#define GP_AUTHTOKEN_LEN            256
#define GP_PARTNERCHALLENGE_LEN     256
#define GP_CDKEY_LEN                65
#define GP_CDKEYENC_LEN             ((((GP_CDKEY_LEN+2)*4)/3)+1)
#define GP_LOGIN_TICKET_LEN         25

#define GP_RICH_STATUS_LEN          256
#define GP_STATUS_BASIC_STR_LEN     33

#define GP_XOR_SEED                 0x79707367 

#define GP_PARTNERID_GAMESPY		0
#define GP_PARTNERID_IGN			10

#define GP_MAX_NAMESPACEIDS         16

typedef void * GPConnection;

typedef int GPProfile;

typedef int GPTransfer;

typedef void (* GPCallback)(
  GPConnection * connection,
  void * arg,
  void * param
);

typedef struct
{
  GPResult result;
  GPErrorCode errorCode;
  gsi_char * errorString;
  GPEnum fatal;
} GPErrorArg;

typedef struct
{
  GPResult result;
  GPProfile profile;
  gsi_char uniquenick[GP_UNIQUENICK_LEN];
} GPConnectResponseArg;

typedef struct
{
  GPResult result;
  GPProfile profile;
} GPNewUserResponseArg;

typedef struct
{
  GPResult result;
  GPProfile profile;
} GPCheckResponseArg;

typedef struct
{
  GPResult result;
  int numSuggestedNicks;
  gsi_char ** suggestedNicks;
} GPSuggestUniqueNickResponseArg;

typedef struct
{
	GPResult result;
} GPRegisterUniqueNickResponseArg;

typedef struct
{
	GPResult result;
} GPRegisterCdKeyResponseArg;

typedef struct
{
  GPResult result;
  GPProfile profile;
} GPNewProfileResponseArg;


typedef struct  
{
	GPResult result;
	GPProfile profile;
} GPDeleteProfileResponseArg;

typedef struct
{
  GPProfile profile;
  gsi_char nick[GP_NICK_LEN];
  gsi_char uniquenick[GP_UNIQUENICK_LEN];
  int namespaceID;
  gsi_char firstname[GP_FIRSTNAME_LEN];
  gsi_char lastname[GP_LASTNAME_LEN];
  gsi_char email[GP_EMAIL_LEN];
} GPProfileSearchMatch;

typedef struct
{
  GPResult result;
  int numMatches;
  GPEnum more;
  GPProfileSearchMatch * matches;
} GPProfileSearchResponseArg;

typedef struct
{
  GPResult result;
  GPProfile profile;
  gsi_char nick[GP_NICK_LEN];
  gsi_char uniquenick[GP_UNIQUENICK_LEN];
  gsi_char email[GP_EMAIL_LEN];
  gsi_char firstname[GP_FIRSTNAME_LEN];
  gsi_char lastname[GP_LASTNAME_LEN];
  gsi_char homepage[GP_HOMEPAGE_LEN];
  int icquin;
  gsi_char zipcode[GP_ZIPCODE_LEN];
  gsi_char countrycode[GP_COUNTRYCODE_LEN];
  float longitude; 
  float latitude;  
  gsi_char place[GP_PLACE_LEN];  
  int birthday;
  int birthmonth;
  int birthyear;
  GPEnum sex;
  GPEnum publicmask;
  gsi_char aimname[GP_AIMNAME_LEN];
  int pic;
  int occupationid;
  int industryid;
  int incomeid;
  int marriedid;
  int childcount;
  int interests1;
  int ownership1;
  int conntypeid;
} GPGetInfoResponseArg;

typedef struct
{ 
  GPProfile profile;
  unsigned int date;
  gsi_char reason[GP_REASON_LEN];
} GPRecvBuddyRequestArg;

typedef struct
{ 
  GPProfile profile; 
  GPEnum status;
  gsi_char statusString[GP_STATUS_STRING_LEN];
  gsi_char locationString[GP_LOCATION_STRING_LEN];
  unsigned int ip; 
  int port;
  GPEnum quietModeFlags;
} GPBuddyStatus;


typedef struct
{
	GPProfile profile;
	GPEnum statusState;
	unsigned int buddyIp;
	unsigned short buddyPort;
	unsigned int hostIp;
	unsigned int hostPrivateIp;
	unsigned short queryPort;
	unsigned short hostPort;
	unsigned int sessionFlags;
	gsi_char richStatus[GP_RICH_STATUS_LEN];
	gsi_char gameType[GP_STATUS_BASIC_STR_LEN];
	gsi_char gameVariant[GP_STATUS_BASIC_STR_LEN];
	gsi_char gameMapName[GP_STATUS_BASIC_STR_LEN];
	GPEnum quietModeFlags;
	GPEnum newStatusInfoFlag;
} GPBuddyStatusInfo;

typedef struct  
{
	GPProfile profile;
	gsi_char **keys;
	gsi_char **values;
	int numKeys;

} GPGetBuddyStatusInfoKeysArg;


typedef struct
{
	GPProfile profile; 
	unsigned int date;
	int index;
} GPRecvBuddyStatusArg;

typedef struct
{ 
  GPProfile profile;
  unsigned int date;
  gsi_char * message;
} GPRecvBuddyMessageArg;

typedef struct 
{
  GPProfile profile;
  unsigned int date;
  gsi_char * message;
} GPRecvBuddyUTMArg;

typedef struct  
{
	GPProfile profile;
	unsigned int date;
} GPRecvBuddyAuthArg;

typedef struct 
{
	GPProfile profile;
	unsigned int date;
} GPRecvBuddyRevokeArg;

typedef struct
{
  GPTransfer transfer;
  GPEnum type;
  int index;
  int num;
  gsi_char * message;
} GPTransferCallbackArg;

typedef struct
{
  GPResult result;
  gsi_char email[GP_EMAIL_LEN];
  GPEnum isValid;
} GPIsValidEmailResponseArg;

typedef struct
{
  GPResult result;
  gsi_char email[GP_EMAIL_LEN];
  int numNicks;  
  gsi_char ** nicks;
  gsi_char ** uniquenicks;
} GPGetUserNicksResponseArg;

typedef struct
{
  GPProfile profile;
  int productID;
  gsi_char location[GP_LOCATION_STRING_LEN];
} GPRecvGameInviteArg;

typedef struct
{
	GPProfile profile;
	gsi_char nick[GP_NICK_LEN];
	GPEnum status;
	gsi_char statusString[GP_STATUS_STRING_LEN];
} GPFindPlayerMatch;

typedef struct
{
  GPResult result;
  int productID;  
  int numMatches;
  GPFindPlayerMatch * matches;
} GPFindPlayersResponseArg;

typedef struct
{
	GPResult result;
	int numProfiles;
	GPProfileSearchMatch * profiles;
} GPGetReverseBuddiesResponseArg;

typedef struct  
{
	GPProfile profile;
	gsi_char uniqueNick[GP_UNIQUENICK_LEN];
} GPUniqueMatch;

typedef struct  
{
	GPResult result;
	int numOfUniqueMatchs;
	GPUniqueMatch *matches;
} GPGetReverseBuddiesListResponseArg;


extern char GPConnectionManagerHostname[64];
extern char GPSearchManagerHostname[64];


#ifndef GSI_UNICODE
#define gpConnect			gpConnectA
#define gpConnectNewUser	gpConnectNewUserA
#define gpConnectUniqueNick gpConnectUniqueNickA
#define gpConnectPreAuthenticated  gpConnectPreAuthenticatedA
#define gpCheckUser			gpCheckUserA
#define gpNewUser			gpNewUserA
#define gpSuggestUniqueNick gpSuggestUniqueNickA
#define gpRegisterUniqueNick       gpRegisterUniqueNickA
#define gpRegisterCdKey     gpRegisterCdKeyA
#define gpGetErrorString	gpGetErrorStringA
#define gpNewProfile		gpNewProfileA
#define gpProfileSearch		gpProfileSearchA
#define gpProfileSearchUniquenick	gpProfileSearchUniquenickA
#define gpSetInfos			gpSetInfosA
#define gpSendBuddyRequest	gpSendBuddyRequestA
#ifndef GP_NEW_STATUS_INFO
#define gpSetStatus			gpSetStatusA
#endif
#ifdef GP_NEW_STATUS_INFO
#define gpSetStatusInfo     gpSetStatusInfoA
#endif
#define gpSendBuddyMessage	gpSendBuddyMessageA
#define gpSendBuddyUTM		gpSendBuddyUTMA
#define gpIsValidEmail		gpIsValidEmailA
#define gpGetUserNicks		gpGetUserNicksA
#define gpSetInfoCacheFilename     gpSetInfoCacheFilenameA
#define gpSendFiles			gpSendFilesA
#define gpAcceptTransfer	gpAcceptTransferA
#define gpRejectTransfer	gpRejectTransferA
#define gpSetTransferDirectory     gpSetTransferDirectoryA
#define gpGetFileName		gpGetFileNameA
#define gpGetFilePath		gpGetFilePathA
#define gpInvitePlayer      gpInvitePlayerA
#ifdef GP_NEW_STATUS_INFO
#define gpAddStatusInfoKey       gpAddStatusInfoKeyA
#define gpSetStatusInfoKey       gpSetStatusInfoKeyA
#define gpGetStatusInfoKeyVal    gpGetStatusInfoKeyValA
#define gpDelStatusInfoKey       gpDelStatusInfoKeyA
#endif
#else
#define gpConnect			gpConnectW
#define gpConnectNewUser	gpConnectNewUserW
#define gpConnectUniqueNick gpConnectUniqueNickW
#define gpConnectPreAuthenticated  gpConnectPreAuthenticatedW
#define gpCheckUser			gpCheckUserW
#define gpNewUser			gpNewUserW
#define gpSuggestUniqueNick gpSuggestUniqueNickW
#define gpRegisterUniqueNick       gpRegisterUniqueNickW
#define gpRegisterCdKey     gpRegisterCdKeyW
#define gpGetErrorString	gpGetErrorStringW
#define gpNewProfile		gpNewProfileW
#define gpProfileSearch		gpProfileSearchW
#define gpProfileSearchUniquenick	gpProfileSearchUniquenickW
#define gpSetInfos			gpSetInfosW
#define gpSendBuddyRequest	gpSendBuddyRequestW
#ifndef GP_NEW_STATUS_INFO
#define gpSetStatus			gpSetStatusW
#endif
#ifdef GP_NEW_STATUS_INFO
#define gpSetStatusInfo     gpSetStatusInfoW
#endif
#define gpSendBuddyMessage	gpSendBuddyMessageW
#define gpSendBuddyUTM		gpSendBuddyUTMW
#define gpIsValidEmail		gpIsValidEmailW
#define gpGetUserNicks		gpGetUserNicksW
#define gpSetInfoCacheFilename     gpSetInfoCacheFilenameW
#define gpSendFiles			gpSendFilesW
#define gpAcceptTransfer	gpAcceptTransferW
#define gpRejectTransfer	gpRejectTransferW
#define gpSetTransferDirectory     gpSetTransferDirectoryW
#define gpGetFileName		gpGetFileNameW
#define gpGetFilePath		gpGetFilePathW
#define gpInvitePlayer      gpInvitePlayerW
#endif

GPResult gpInitialize
(
  GPConnection * connection,
  int productID,				
  int namespaceID,				
  int partnerID					
);

void gpDestroy(
  GPConnection * connection
);

GPResult gpEnable
(
  GPConnection * connection, 
  GPEnum state
);

GPResult gpDisable
(
  GPConnection * connection, 
  GPEnum state
);

GPResult gpProcess
(
  GPConnection * connection
);

GPResult gpSetCallback
(
  GPConnection * connection,
  GPEnum func,
  GPCallback callback,
  void * param
);

GPResult gpConnect
(
  GPConnection * connection,
  const gsi_char nick[GP_NICK_LEN],
  const gsi_char email[GP_EMAIL_LEN],
  const gsi_char password[GP_PASSWORD_LEN],
  GPEnum firewall,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpConnectNewUser
(
  GPConnection * connection,
  const gsi_char nick[GP_NICK_LEN],
  const gsi_char uniquenick[GP_UNIQUENICK_LEN],
  const gsi_char email[GP_EMAIL_LEN],
  const gsi_char password[GP_PASSWORD_LEN],
  const gsi_char cdkey[GP_CDKEY_LEN],
  GPEnum firewall,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpConnectUniqueNick
(
  GPConnection * connection,
  const gsi_char uniquenick[GP_UNIQUENICK_LEN],
  const gsi_char password[GP_PASSWORD_LEN],
  GPEnum firewall,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpConnectPreAuthenticated
(
  GPConnection * connection,
  const gsi_char authtoken[GP_AUTHTOKEN_LEN],
  const gsi_char partnerchallenge[GP_PARTNERCHALLENGE_LEN],
  GPEnum firewall,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

void gpDisconnect
(
  GPConnection * connection
);

GPResult gpIsConnected
(
  GPConnection * connection,
  GPEnum * connected
);

GPResult gpCheckUser
(
  GPConnection * connection,
  const gsi_char nick[GP_NICK_LEN],
  const gsi_char email[GP_EMAIL_LEN],
  const gsi_char password[GP_PASSWORD_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpNewUser
(
  GPConnection * connection,
  const gsi_char nick[GP_NICK_LEN],
  const gsi_char uniquenick[GP_UNIQUENICK_LEN],
  const gsi_char email[GP_EMAIL_LEN],
  const gsi_char password[GP_PASSWORD_LEN],
  const gsi_char cdkey[GP_CDKEY_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpSuggestUniqueNick
(
  GPConnection * connection,
  const gsi_char desirednick[GP_UNIQUENICK_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpRegisterUniqueNick
(
  GPConnection * connection,
  const gsi_char uniquenick[GP_UNIQUENICK_LEN],
  const gsi_char cdkey[GP_CDKEY_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpRegisterCdKey
(
  GPConnection * connection,
  const gsi_char cdkey[GP_CDKEY_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpGetErrorCode(
  GPConnection * connection,
  GPErrorCode * errorCode
);

GPResult gpGetErrorString(
  GPConnection * connection,
  gsi_char errorString[GP_ERROR_STRING_LEN]
);

GPResult gpNewProfile(
  GPConnection * connection,
  const gsi_char nick[GP_NICK_LEN],
  GPEnum replace,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpDeleteProfile(
    GPConnection * connection,
    GPCallback callback,
	void * param
);

GPResult gpProfileFromID(
  GPConnection * connection, 
  GPProfile * profile, 
  int id
);

GPResult gpIDFromProfile(
  GPConnection * connection,
  GPProfile profile,
  int * id
);

GPResult gpUserIDFromProfile(
  GPConnection * connection,
  GPProfile profile,
  int * userid
);

GPResult gpProfileSearch(
  GPConnection * connection,
  const gsi_char nick[GP_NICK_LEN],
  const gsi_char uniquenick[GP_UNIQUENICK_LEN],
  const gsi_char email[GP_EMAIL_LEN],
  const gsi_char firstname[GP_FIRSTNAME_LEN],
  const gsi_char lastname[GP_LASTNAME_LEN],
  int icquin,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpProfileSearchUniquenick(
  GPConnection * connection,
  const gsi_char uniquenick[GP_UNIQUENICK_LEN],
  const int namespaceIDs[GP_MAX_NAMESPACEIDS],
  int numNamespaces,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpGetInfo(
  GPConnection * connection,
  GPProfile profile, 
  GPEnum checkCache,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpGetInfoNoWait(
  GPConnection * connection,
  GPProfile profile,
  GPGetInfoResponseArg * arg
);

GPResult gpSetInfoi(
  GPConnection * connection, 
  GPEnum info, 
  int value
);

GPResult gpSetInfos(
  GPConnection * connection,
  GPEnum info,
  const gsi_char * value
);

GPResult gpSetInfod(
  GPConnection * connection,
  GPEnum info,
  int day,
  int month,
  int year
);

GPResult gpSetInfoMask(
  GPConnection * connection,
  GPEnum mask
);

GPResult gpSendBuddyRequest(
  GPConnection * connection,
  GPProfile profile,
  const gsi_char reason[GP_REASON_LEN]
);

GPResult gpAuthBuddyRequest(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpDenyBuddyRequest(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpDeleteBuddy(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpAddToBlockedList(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpRemoveFromBlockedList(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpGetNumBlocked(
  GPConnection * connection,
  int * numBlocked
);

GPResult gpGetBlockedProfile(
  GPConnection * connection, 
  int index,
  GPProfile * profile
);

gsi_bool gpIsBlocked(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpGetNumBuddies(
  GPConnection * connection,
  int * numBuddies
);

#ifndef GP_NEW_STATUS_INFO
GPResult gpGetBuddyStatus(
  GPConnection * connection,
  int index,
  GPBuddyStatus * status
);
#endif

#ifdef GP_NEW_STATUS_INFO
GPResult gpGetBuddyStatusInfo(
	GPConnection * connection,
	int index, 
	GPBuddyStatusInfo * statusInfo
);

GPResult gpSetBuddyAddr(
	GPConnection *connection, 
	int index,
	unsigned int buddyIp,
	unsigned short buddyPort
);
#endif
GPResult gpGetBuddyIndex(
  GPConnection * connection, 
  GPProfile profile, 
  int * index
);

int gpIsBuddy(
  GPConnection * connection,
  GPProfile profile
);

int gpIsBuddyConnectionOpen(
  GPConnection * connection,
  GPProfile profile
);

#ifndef GP_NEW_STATUS_INFO
GPResult gpSetStatus(
  GPConnection * connection,
  GPEnum status,
  const gsi_char statusString[GP_STATUS_STRING_LEN],
  const gsi_char locationString[GP_LOCATION_STRING_LEN]
);
#endif

#ifdef GP_NEW_STATUS_INFO
GPResult gpSetStatusInfo(
	GPConnection *connection, 
	GPEnum statusState,
	unsigned int hostIp, 
	unsigned int hostPrivateIp,
	unsigned short queryPort,
	unsigned short hostPort,
	unsigned int sessionFlags,
	const gsi_char *richStatus,
	int richStatusLen,
	const gsi_char *gameType,
	int gameTypeLen,
	const gsi_char *gameVariant,
	int gameVariantLen,
	const gsi_char *gameMapName,
	int gameMapNameLen
);

GPResult gpAddStatusInfoKey(GPConnection *connection, const gsi_char *keyName, const gsi_char *keyValue);
GPResult gpSetStatusInfoKey(GPConnection *connection, const gsi_char *keyName, const gsi_char *keyValue);
GPResult gpGetStatusInfoKeyVal(GPConnection *connection, const gsi_char *keyName, gsi_char **keyValue);
GPResult gpDelStatusInfoKey(GPConnection *connection, const gsi_char *keyName);

GPResult gpGetBuddyStatusInfoKeys(GPConnection *connection, int index, GPCallback callback, void *userData);
#endif
GPResult gpSendBuddyMessage(
  GPConnection * connection,
  GPProfile profile,
  const gsi_char * message
);

GPResult gpSendBuddyUTM(
  GPConnection * connection,
  GPProfile profile,
  const gsi_char * message,
  int sendOption 
);



GPResult gpIsValidEmail(
  GPConnection * connection,
  const gsi_char email[GP_EMAIL_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpGetUserNicks(
  GPConnection * connection,
  const gsi_char email[GP_EMAIL_LEN],
  const gsi_char password[GP_PASSWORD_LEN],
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpSetInvitableGames(
  GPConnection * connection,
  int numProductIDs,
  const int * productIDs
);

GPResult gpFindPlayers(
  GPConnection * connection,
  int productID,
  GPEnum blocking,
  GPCallback callback,
  void * param
);

GPResult gpInvitePlayer(
  GPConnection * connection,
  GPProfile profile,
  int productID,
  const gsi_char location[GP_LOCATION_STRING_LEN]
);

GPResult gpGetReverseBuddies(
	GPConnection * connection,
	GPEnum blocking,
	GPCallback callback,
	void * param
);

GPResult gpGetReversBuddiesList( GPConnection * connection,
	GPProfile *targets, int numOfTargets, 
	GPEnum blocking,
	GPCallback callback,
	void * param
);

GPResult gpRevokeBuddyAuthorization(
  GPConnection * connection,
  GPProfile profile
);

GPResult gpSetCdKey(
  GPConnection * connection,
  const gsi_char cdkeyhash,
  GPCallback callback
);

GPResult gpGetLoginTicket(
  GPConnection * connection,
  char loginTicket[GP_LOGIN_TICKET_LEN]
);

GPResult gpSetQuietMode(
  GPConnection * connection,
  GPEnum flags
);

#ifndef NOFILE

void gpSetInfoCacheFilename(
  const gsi_char * filename
);

typedef void (* gpSendFilesCallback)(
  GPConnection * connection,
  int index,
  const gsi_char ** path,
  const gsi_char ** name,
  void * param
);

GPResult gpSendFiles(
  GPConnection * connection,
  GPTransfer * transfer,
  GPProfile profile,
  const gsi_char * message,
  gpSendFilesCallback callback,
  void * param
);

GPResult gpAcceptTransfer(
  GPConnection * connection,
  GPTransfer transfer,
  const gsi_char * message
);

GPResult gpRejectTransfer(
  GPConnection * connection,
  GPTransfer transfer,
  const gsi_char * message
);

GPResult gpFreeTransfer(
  GPConnection * connection,
  GPTransfer transfer
);

GPResult gpSetTransferData(
  GPConnection * connection,
  GPTransfer transfer,
  void * userData
);

void * gpGetTransferData(
  GPConnection * connection,
  GPTransfer transfer
);

GPResult gpSetTransferDirectory(
  GPConnection * connection,
  GPTransfer transfer,
  const gsi_char * directory
);

GPResult gpSetTransferThrottle(
  GPConnection * connection,
  GPTransfer transfer,
  int throttle
);

GPResult gpGetTransferThrottle(
  GPConnection * connection,
  GPTransfer transfer,
  int * throttle
);

GPResult gpGetTransferProfile(
  GPConnection * connection,
  GPTransfer transfer,
  GPProfile * profile
);

GPResult gpGetTransferSide(
  GPConnection * connection,
  GPTransfer transfer,
  GPEnum * side
);

GPResult gpGetTransferSize(
  GPConnection * connection,
  GPTransfer transfer,
  int * size
);

GPResult gpGetTransferProgress(
  GPConnection * connection,
  GPTransfer transfer,
  int * progress
);

GPResult gpGetNumFiles(
  GPConnection * connection,
  GPTransfer transfer,
  int * num
);

GPResult gpGetCurrentFile(
  GPConnection * connection,
  GPTransfer transfer,
  int * index
);

GPResult gpSkipFile(
  GPConnection * connection,
  GPTransfer transfer,
  int index
);

GPResult gpGetFileName(
  GPConnection * connection,
  GPTransfer transfer,
  int index,
  gsi_char ** name
);

GPResult gpGetFilePath(
  GPConnection * connection,
  GPTransfer transfer,
  int index,
  gsi_char ** path
);

GPResult gpGetFileSize(
  GPConnection * connection,
  GPTransfer transfer,
  int index,
  int * size
);

GPResult gpGetFileProgress(
  GPConnection * connection,
  GPTransfer transfer,
  int index,
  int * progress
);

GPResult gpGetFileModificationTime(
  GPConnection * connection,
  GPTransfer transfer,
  int index,
  gsi_time * modTime
);

GPResult gpGetNumTransfers(
  GPConnection * connection,
  int * num
);

GPResult gpGetTransfer(
  GPConnection * connection,
  int index,
  GPTransfer * transfer
);
#endif

#ifdef _DEBUG
void gpProfilesReport(
  GPConnection * connection,
  void (* report)(const char * output)
);
#endif

#ifdef __cplusplus
}
#endif

#endif
