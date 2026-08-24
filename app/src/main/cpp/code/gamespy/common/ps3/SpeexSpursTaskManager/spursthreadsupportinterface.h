/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */


#ifndef THREAD_SUPPORT_INTERFACE_H
#define THREAD_SUPPORT_INTERFACE_H

#include "spursPlatformDefinitions.h"

class spursThreadSupportInterface
{
public:

	virtual ~spursThreadSupportInterface();

	virtual int sendRequest(uint32_t uiCommand, uint32_t uiArgument0, uint32_t uiArgument1) =0;

	virtual	int waitForResponse(unsigned int *puiArgument0, unsigned int *puiArgument1) =0;

	virtual	int startSPU() =0;

	virtual	int stopSPU()=0;
};

#endif 
