/*
gpiPS3.c
GameSpy Presence SDK 

devsupport@gamespy.com

***********************************************************************
Please see the GameSpy Presence SDK documentation for more information
**********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "gpi.h"

#ifdef _PS3
uint8_t	gpi_np_pool[SCE_NP_MIN_POOL_SIZE];
SceNpCommunicationId gpi_communication_id = {
	{'N','P','X','S','0','0','0','0','5'},
	'\0',
	0,
	0
};

int gpiNpBasicCallback(
  int event, 
  int retCode, 
  uint32_t reqId, 
  void *arg
)
{
	return 0;
}

GPResult gpiInitializeNpBasic(
  GPConnection * connection
)
{
	int ret = 0;
    GPIConnection * iconnection = (GPIConnection*)*connection;

    iconnection->npInitialized = gsi_true;

	ret = sceNpInit(SCE_NP_MIN_POOL_SIZE, gpi_np_pool);

    if (ret == SCE_NP_ERROR_ALREADY_INITIALIZED)
    {
        // If already initialized - DO NOT terminate after sync (game might need it)
        iconnection->npBasicGameInitialized = gsi_true;
    }
	else if (ret < 0) 
	{
		iconnection->npBasicGameInitialized = gsi_true;
        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
            "gpiInitializeNpBasic: sceNpInit() failed, NP-functionality disabled. ret = 0x%x\n", ret);	
        return GP_MISC_ERROR;
	}
    else
        iconnection->npBasicGameInitialized = gsi_false; 

	return GP_NO_ERROR;
}

void gpiNpTransactionListFree(void *element)
{
    npIdLookupTrans *aTrans = (npIdLookupTrans *)element;
    freeclear(aTrans->npIdForAdd);
}


GPResult gpiCheckNpStatus(
  GPConnection * connection
)
{
	int ret = 0;
	int status = SCE_NP_MANAGER_STATUS_OFFLINE;
    SceNpId npId;
	GPIConnection * iconnection = (GPIConnection*)*connection;

	ret = sceNpManagerGetStatus(&status);
	if (ret < 0) 
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
			"gpiCheckNpStatus: sceNpGetStatus() failed. ret = 0x%x\n", ret);	
	}
	gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
		"gpiCheckNpStatus: sceNpGetStatus - status = %d\n", status);	


	if (status != SCE_NP_MANAGER_STATUS_ONLINE && (current_time() - iconnection->loginTime > GPI_NP_STATUS_TIMEOUT))
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
			"gpiCheckNpStatus: NP Status not online - timed out\n");	
		
		iconnection->npPerformBuddySync = gsi_false;
        iconnection->npPerformBlockSync = gsi_false;

		return GP_MISC_ERROR;
	}

	if (status == SCE_NP_MANAGER_STATUS_ONLINE)
	{
		iconnection->loginTime = current_time();

        if (!iconnection->npBasicGameInitialized)
        {
		    ret = sceNpBasicInit(); 
		    if (ret < 0) 
		    {
			    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
				    "gpiCheckNpStatus: sceNpBasicInit() failed. ret = 0x%x\n", ret);	
		    }

		    ret = sceNpBasicRegisterHandler(&gpi_communication_id, gpiNpBasicCallback, NULL);
		    if (ret < 0) 
		    {
			    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
				    "gpiCheckNpStatus: sceNpBasicRegisterHandler() failed. ret = 0x%x\n", ret);
		    }
        }

        ret = sceNpLookupInit();
        if (ret == SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED)
        {
            // If already initialized - DO NOT terminate after GP destroy (game might need it)
            iconnection->npLookupGameInitialized = gsi_true;
        }
        else if (ret < 0) 
        {
            gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                "gpiCheckNpStatus: sceNpLookupInit() failed. ret = 0x%x\n", ret);    
            iconnection->npLookupGameInitialized = gsi_true;
        }
        else
            iconnection->npLookupGameInitialized = gsi_false;

        ret = sceNpManagerGetNpId(&npId);
        if (ret < 0) 
        {
            gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                "gpiCheckNpStatus: sceNpManagerGetNpId() failed. ret = 0x%x\n", ret);  
        }

        ret = sceNpLookupCreateTitleCtx(&gpi_communication_id, &npId);
        if (ret < 0) 
        {
            gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                "gpiCheckNpStatus: sceNpLookupCreateTitleCtx() failed. ret = 0x%x\n", ret);  
        }

        iconnection->npLookupTitleCtxId = ret;

		iconnection->npStatusRetrieved = gsi_true;
        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
            "gpiCheckNpStatus: NP is now initialized with status.\n");	

        iconnection->npTransactionList = ArrayNew(sizeof(npIdLookupTrans), 1, gpiNpTransactionListFree);
        if (!iconnection->npTransactionList)
            Error(connection, GP_MEMORY_ERROR, "Out of memory.");
	}

	return GP_NO_ERROR;
}

GPResult gpiDestroyNpBasic(
  GPConnection * connection
)
{
	GPIConnection * iconnection = (GPIConnection*)*connection;	

    if (iconnection->npLookupTitleCtxId >= 0)
        sceNpLookupDestroyTitleCtx(iconnection->npLookupTitleCtxId);

    if (!iconnection->npLookupGameInitialized)
        sceNpLookupTerm();

    if (!iconnection->npBasicGameInitialized)
    {
	    sceNpBasicUnregisterHandler();

	    sceNpBasicTerm();

        sceNpTerm();
    }

    if (iconnection->npTransactionList)
        ArrayFree(iconnection->npTransactionList);

	iconnection->npInitialized = gsi_false;
    iconnection->npStatusRetrieved = gsi_false;

	return GP_NO_ERROR;
}

GPResult gpiSyncNpBuddies(
  GPConnection * connection
)
{
	int ret; 
	SceNpId npId;	
	gsi_u32 i, count = 0;
	GPIConnection * iconnection = (GPIConnection*)*connection;


	iconnection->npPerformBuddySync = gsi_false;

	ret = sceNpBasicGetFriendListEntryCount(&count);
	if ( ret < 0 ) 
	{
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
			"PS3BuddySync: Failed to get NP friend list count\n");
	}

	for (i = 0; i < count; i++) 
	{
		memset(&npId, 0x00, sizeof(npId));
		ret = sceNpBasicGetFriendListEntry(i, &npId);
		if (ret < 0) 
		{
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
				"PS3BuddySync: Failed to get NP friend entry #%d\n", i);
			return GP_MISC_ERROR;
		}

		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
			"PS3BuddySync: NP friend entry #%d, npid = %s. Queueing Search.\n", i, npId.handle.data);

		gpiProfileSearchUniquenick(connection, npId.handle.data, &iconnection->namespaceID, 
			1, GP_NON_BLOCKING, (GPCallback)gpiSyncNpBuddiesCallback, NULL);
	}

	return GP_NO_ERROR;
}

void gpiSyncNpBuddiesCallback(
  GPConnection * pconnection, 
  GPProfileSearchResponseArg * arg, 
  void * param
)
{
	if(arg->result == GP_NO_ERROR)
	{
		if(arg->numMatches == 1)
		{
		    if (!gpIsBuddy(pconnection, arg->matches[0].profile))
            {
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
					"PS3BuddySync: NP Buddy \"%s\" found in namespace %d. Sending Request.\n", 
					arg->matches[0].uniquenick, arg->matches[0].namespaceID);

				gpSendBuddyRequest(pconnection, arg->matches[0].profile, _T("PS3 Buddy Sync"));
			}
			else
				gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
					"PS3BuddySync: \"%s\" is already a buddy\n", arg->matches[0].uniquenick);
		}
		else
			gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
				"PS3BuddySync: No suitable match found\n");
	}
	else
		gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
			"PS3BuddySync: Buddy Search FAILED!\n");

	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}

GPResult gpiSyncNpBlockList(
  GPConnection * connection
)
{
    int ret; 
    SceNpId npId;	
    gsi_u32 i, count = 0;
    GPIConnection * iconnection = (GPIConnection*)*connection;


    iconnection->npPerformBlockSync = gsi_false;

    ret = sceNpBasicGetBlockListEntryCount(&count);
    if ( ret < 0 ) 
    {
        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
            "PS3BlockSync: Failed to get NP block list count\n");
    }

    for (i = 0; i < count; i++) 
    {
        memset(&npId, 0x00, sizeof(npId));
        ret = sceNpBasicGetBlockListEntry(i, &npId);
        if (ret < 0) 
        {
            gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                "PS3BlockSync: Failed to get NP block entry #%d\n", i);
            return GP_MISC_ERROR;
        }

        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
            "PS3BlockSync: NP block entry #%d, npid = %s. Queueing Search.\n", i, npId.handle.data);

        gpiProfileSearchUniquenick(connection, npId.handle.data, &iconnection->namespaceID, 
            1, GP_NON_BLOCKING, (GPCallback)gpiSyncNpBlockListCallback, NULL);
    }

    return GP_NO_ERROR;
}

void gpiSyncNpBlockListCallback(
  GPConnection * pconnection, 
  GPProfileSearchResponseArg * arg, 
  void * param
)
{
	GPIProfile * pProfile;
    GPIConnection * iconnection = (GPIConnection*)*pconnection;

    if(arg->result == GP_NO_ERROR)
    {
        if(arg->numMatches == 1)
        {
            if(!gpiGetProfile(pconnection, arg->matches[0].profile, &pProfile) || !pProfile->blocked)
            {
                gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
                    "PS3BlockSync: NP Block Entry \"%s\" found in namespace %d. Adding to BlockedList.\n", 
                    arg->matches[0].uniquenick, arg->matches[0].namespaceID);

                iconnection->npSyncLock = gsi_true;
                gpiAddToBlockedList(pconnection, arg->matches[0].profile);
                iconnection->npSyncLock = gsi_false;
            }
            else
                gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
                    "PS3BlockSync: \"%s\" is already blocked\n", arg->matches[0].uniquenick);
        }
        else
            gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
                "PS3BlockSync: No suitable match found\n");
    }
    else
        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
        "PS3BlockSync: Block Entry Search FAILED!\n");

    GSI_UNUSED(param);
}

GPResult gpiAddToNpBlockList(
  GPConnection * connection, 
  int profileid
)
{
    GPIConnection * iconnection = (GPIConnection*)*connection;

    if (!iconnection->npTransactionList || iconnection->npLookupTitleCtxId < 0)
    {
        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
            "PS3AddToNpBlockList: Cancelling add - NP status not yet resolved.\n");        
        return GP_NO_ERROR;
    }

    gpiGetInfo(connection, profileid, GP_CHECK_CACHE, GP_NON_BLOCKING, 
        (GPCallback)gpiAddToNpBlockListInfoCallback, NULL);

    return GP_NO_ERROR;
}

void gpiAddToNpBlockListInfoCallback(
  GPConnection * pconnection, 
  GPGetInfoResponseArg * arg, 
  void * param
)
{
    SceNpOnlineId onlineId;
    int ret;
    npIdLookupTrans transaction;
    GPIConnection * iconnection = (GPIConnection*)*pconnection;
#ifdef GSI_UNICODE
    char asciiUniquenick[GP_UNIQUENICK_LEN];
#endif

    if(arg->result == GP_NO_ERROR)
    {
        if (_tcslen(arg->uniquenick) != 0)
        {
            memset(&onlineId, 0, sizeof(onlineId));

#ifdef GSI_UNICODE
            UCS2ToAsciiString(arg->uniquenick, (char*)asciiUniquenick);
            strncpy(onlineId.data, asciiUniquenick, SCE_NET_NP_ONLINEID_MAX_LENGTH);
#else
            strncpy(onlineId.data, arg->uniquenick, SCE_NET_NP_ONLINEID_MAX_LENGTH);
#endif

            if (ArrayLength(iconnection->npTransactionList) < GPI_NP_NUM_TRANSACTIONS)
            {
                ret = sceNpLookupCreateTransactionCtx(iconnection->npLookupTitleCtxId);
                if (ret < 0)
                {
                    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                        "PS3AddToNpBlockList: sceNpLookupCreateTransactionCtx() failed. ret = 0x%x\n", ret);  
                }
                else
                {
                    transaction.npIdForAdd = (SceNpId*)gsimalloc(sizeof(SceNpId));
                    if(transaction.npIdForAdd == NULL)
                    {
                        sceNpLookupDestroyTransactionCtx(ret);
                        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                            "PS3AddToNpBlockList: Out of memory.\n");  
                        return;
                    }
                    transaction.npTransId = ret;
                    transaction.npLookupDone = gsi_false;
                    ArrayAppend(iconnection->npTransactionList, &transaction);

                    ret = sceNpLookupNpIdAsync(transaction.npTransId, &onlineId, 
                        transaction.npIdForAdd, 0, NULL);
                    if (ret < 0) 
                    {
                        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                            "PS3AddToNpBlockList: sceNpLookupNpIdAsync() failed. ret = 0x%x\n", ret);  
                    } 
                }
            }
            else
            {
                gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_WarmError,
                    "PS3AddToNpBlockList: Transactions limit reached for np lookups\n");  
            }
        }
        else
            gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                "PS3AddToNpBlockList: Profile [%d] does not have a uniquenick in namespace %d!\n",
                arg->profile, iconnection->namespaceID);
    }
    else
        gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
            "PS3AddToNpBlockList: Player Info lookup FAILED!\n");

    GSI_UNUSED(pconnection);
    GSI_UNUSED(param);
}

GPResult gpiProcessNp(GPConnection * connection)
{
    int i, ret=0;
    GPIConnection * iconnection = (GPIConnection*)*connection;
    npIdLookupTrans * transaction;

    if (!iconnection->npTransactionList)
        return GP_NO_ERROR;

    if (ArrayLength(iconnection->npTransactionList) > 0)
        cellSysutilCheckCallback();

    for (i=0; i < ArrayLength(iconnection->npTransactionList); i++)
    {
        transaction = (npIdLookupTrans *)ArrayNth(iconnection->npTransactionList, i);

        if (!transaction->npLookupDone)
        {
            if (sceNpLookupPollAsync(transaction->npTransId, &ret)==0)
                transaction->npLookupDone = gsi_true;
        }
        else
        {
            if (ret<0)
            {
                gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                    "PS3AddToNpBlockList: sceNpLookupWaitAsync. ret = 0x%x\n", ret);
                if (ret == (int)SCE_NP_COMMUNITY_SERVER_ERROR_NO_SUCH_USER_NPID)
                {
                    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                        "PS3AddToNpBlockList: Player '%s' is not an NP user.\n", 
                        transaction->npIdForAdd->handle.data);
                }
            }
            else
            {
                ret = sceNpBasicAddBlockListEntry(transaction->npIdForAdd);
                if (ret == (int)SCE_NP_BASIC_ERROR_BUSY)
                {
                    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
                        "PS3AddToNpBlockList: SCE_NP_BASIC_ERROR_BUSY. continue trying to add to NP\n"); 
                    return GP_NO_ERROR;
                }
                else if ( ret < 0 ) 
                {
                    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                        "PS3AddToNpBlockList: sceNpBasicAddBlockListEntry() failed. ret = 0x%x\n", ret); 
                }                
                else
                {
                    gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_Comment,
                        "PS3AddToNpBlockList: Player '%s' added to NP Block list.\n", 
                        transaction->npIdForAdd->handle.data); 
                }
            }

            ret = sceNpLookupDestroyTransactionCtx(transaction->npTransId);
            if (ret<0)
            {
                gsDebugFormat(GSIDebugCat_GP, GSIDebugType_Misc, GSIDebugLevel_HotError,
                    "PS3AddToNpBlockList: sceNpLookupDestroyTransactionCtx() failed. ret = 0x%x\n", ret); 
            }

            ArrayDeleteAt(iconnection->npTransactionList, i);
        }
    }

    return GP_NO_ERROR;
}

#endif
