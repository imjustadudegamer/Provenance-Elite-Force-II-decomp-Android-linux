

#ifndef _PT_H_
#define _PT_H_

#include "../common/gsCommon.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef int PTBool;
#define PTFalse 0
#define PTTrue  1

#ifndef GSI_UNICODE
#define ptCheckForPatch		ptCheckForPatchA
#define ptTrackUsage		ptTrackUsageA
#define ptCheckForPatchAndTrackUsage	ptCheckForPatchAndTrackUsageA
#define ptCreateCheckPatchTrackUsageReq       ptCreateCheckPatchTrackUsageReqA
#else
#define ptCheckForPatch		ptCheckForPatchW
#define ptTrackUsage		ptTrackUsageW
#define ptCheckForPatchAndTrackUsage	ptCheckForPatchAndTrackUsageW
#define ptCreateCheckPatchTrackUsageReq ptCreateCheckPatchTrackUsageReqW
#endif

typedef void (* ptPatchCallback)
(
	PTBool available,         
	PTBool mandatory,         
	const gsi_char * versionName, 
	int fileID,               
	const gsi_char * downloadURL, 
	void * param              
);

PTBool ptCheckForPatch
(
	int productID,                
	const gsi_char * versionUniqueID, 
	int distributionID,           
	ptPatchCallback callback,     
	PTBool blocking,              
	void * param                  
);

PTBool ptTrackUsage
(
	int userID,                   
	int productID,                
	const gsi_char * versionUniqueID, 
	int distributionID,           
	PTBool blocking
);

PTBool ptCheckForPatchAndTrackUsage
(
	int userID,                   
	int productID,                
	const gsi_char * versionUniqueID, 
	int distributionID,           
	ptPatchCallback callback,     
	PTBool blocking,              
	void * param                  
);

PTBool ptCreateCheckPatchTrackUsageReq
(
	int userID,                   
	int productID,                
	const gsi_char * versionUniqueID, 
	int distributionID,           
	ptPatchCallback callback,     
	PTBool blocking,              
	int *request,                 
	void * param                  
 );

typedef void (* ptFilePlanetInfoCallback)
(
	int fileID,                
	PTBool found,              
	const gsi_char * description,  
	const gsi_char * size,         
	int numMirrors,            
	const gsi_char ** mirrorNames, 
	const gsi_char ** mirrorURLs,  
	void * param               
);

PTBool ptLookupFilePlanetInfo
(
	int fileID,                        
	ptFilePlanetInfoCallback callback, 
	PTBool blocking,                   
	void * param                       
);

#ifdef __cplusplus
}
#endif

#endif
