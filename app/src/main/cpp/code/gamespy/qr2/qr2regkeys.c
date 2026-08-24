
#include "qr2regkeys.h"

#include "../common/gsStringUtil.h"
#include "../common/gsDebug.h"

#ifdef __MWERKS__ 
void qr2_register_keyW(int keyid, const unsigned short *key);
void qr2_register_keyA(int keyid, const char *key);
#endif

const char *qr2_registered_key_list[MAX_REGISTERED_KEYS] =
{
	"",				
	"hostname",		
	"gamename",		
	"gamever",		
	"hostport",		
	"mapname",		
	"gametype",		
	"gamevariant",	
	"numplayers",	
	"numteams",		
	"maxplayers",	
	"gamemode",		
	"teamplay",		
	"fraglimit",	
	"teamfraglimit",
	"timeelapsed",	
	"timelimit",	
	"roundtime",	
	"roundelapsed",	
	"password",		
	"groupid",		
	"player_",		
	"score_",		
	"skill_",		
	"ping_",		
	"team_",		
	"deaths_",		
	"pid_",			
	"team_t",		
	"score_t",		
	"nn_groupid",	

	"country",		
	"region"		
};


typedef struct QR2KeyListNodeS
{
	char* mKeyData;
	struct QR2KeyListNodeS*	mNextKey;
} QR2KeyListNode;

typedef struct QR2KeyListS
{
	struct QR2KeyListNodeS* mHead;
} QR2KeyList;

static QR2KeyList qr2_internal_key_list = { NULL };

void qr2_internal_key_list_append(char* theKey)
{
	QR2KeyListNode* aNewNode;

	assert(theKey != NULL);

	aNewNode = (QR2KeyListNode*)gsimalloc(sizeof(QR2KeyListNode));
	aNewNode->mKeyData = theKey;
	aNewNode->mNextKey = NULL;

	if (qr2_internal_key_list.mHead == NULL)
		qr2_internal_key_list.mHead = aNewNode;
	else
	{
		QR2KeyListNode* aInsertPlace = qr2_internal_key_list.mHead;
		while(aInsertPlace->mNextKey != NULL)
			aInsertPlace = aInsertPlace->mNextKey;

		aInsertPlace->mNextKey = aNewNode;
	}
}

void qr2_internal_key_list_free()
{
	QR2KeyListNode* aNodeToFree;
	QR2KeyListNode* aNextNode;

	aNodeToFree = qr2_internal_key_list.mHead;
	while (aNodeToFree != NULL)
	{
		aNextNode = aNodeToFree->mNextKey;	
		gsifree(aNodeToFree->mKeyData);		
		gsifree(aNodeToFree);				
		aNodeToFree = aNextNode;			
	}
	
	qr2_internal_key_list.mHead = NULL;
}

gsi_bool qr2_internal_is_master_only_key(const char * keyname)
{
	if (strcmp(keyname,qr2_registered_key_list[COUNTRY_KEY]) == 0 || 
		strcmp(keyname,qr2_registered_key_list[REGION_KEY]) == 0)
		return gsi_true;

	return gsi_false;
}

void qr2_register_keyA(int keyid, const char *key)
{
	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_keyA()\r\n");

	if (keyid < NUM_RESERVED_KEYS || keyid > MAX_REGISTERED_KEYS)
	{
		gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_WarmError,
			"Attempted to register invalid key %d - %s\r\n", keyid, key);
		return;
	}

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_Comment,
		"Registered key %d - %s\r\n", keyid, key);

	qr2_registered_key_list[keyid] = key;
}
void qr2_register_keyW(int keyid, const unsigned short *key)
{
	char* key_A = NULL;

	gsDebugFormat(GSIDebugCat_QR2, GSIDebugType_Misc, GSIDebugLevel_StackTrace,
		"qr2_register_keyW()\r\n");

	key_A = UCS2ToUTF8StringAlloc(key);

	qr2_register_keyA(keyid, key_A);

	qr2_internal_key_list_append(key_A);
}
