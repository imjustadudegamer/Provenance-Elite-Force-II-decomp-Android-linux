/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "../gsCommon.h"

gsi_i64 gsiStringToInt64(const char *theNumberStr)
{
	return atoll(theNumberStr);
}


void gsiInt64ToString(char theNumberStr[33], gsi_i64 theNumber)
{
	GS_ASSERT(theNumberStr != NULL);

	sprintf(theNumberStr, "%lld", theNumber);
}